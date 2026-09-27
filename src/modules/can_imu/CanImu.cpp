#include "ImuCore.hpp"
#include "HeaterControl.hpp"
#include <px4_platform_common/module.h>
#include <px4_platform_common/param.h>
#include <drivers/drv_hrt.h>
#include <board_config.h>
#include <uORB/Subscription.hpp>
#include <uORB/topics/sensor_accel_fifo.h>
#include <uORB/topics/sensor_gyro_fifo.h>
#include <uORB/topics/sensor_accel.h>
#include <uavcan_stm32h7/can.hpp>
#include <uavcan_stm32h7/clock.hpp>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

using namespace canimu;
class CanImu : public ModuleBase<CanImu> {
public:
 ~CanImu() override { HEATER1_OUTPUT_EN(false); HEATER2_OUTPUT_EN(false); if(iface) { can.driver.getIface(0)->shutdown(); } }
 static int task_spawn(int argc,char *argv[]) {
  // Drain FIFO publications ahead of USB diagnostics (top: 237), but below
  // the SPI sensor work queues (250/253). Sixteen queued updates cover only
  // about 8 ms; console formatting must not starve this consumer.
  _task_id=px4_task_spawn_cmd("can_imu",SCHED_DEFAULT,SCHED_PRIORITY_MAX-10,8000,run_trampoline,argv);
  return _task_id<0?-1:0;
 }
 static CanImu *instantiate(int,char**) { return new CanImu(); }
 static int custom_command(int,char**) { return print_usage(); }
 static int print_usage(const char *reason=nullptr) { if(reason) { PX4_WARN("%s",reason); } PX4_INFO("can_imu {start|stop|status}; configuration takes effect after restart"); return 0; }
 int print_status() override {
  PX4_INFO("boot=%lu source=%ld seq=%u queued=%lu tx_drop=%lu incomplete=%lu CANerr=%llu",(unsigned long)boot,(long)source,seq,(unsigned long)sent,(unsigned long)tx_drop,(unsigned long)incomplete,iface?(unsigned long long)iface->getErrorCount():0ULL);
  for(int s=0;s<2;++s) { PX4_INFO("IMU%d T=%.2f heater_fault=%d accel/gyro gap=%lu/%lu reverse=%lu/%lu",s,(double)temp[s],int(heater[s].fault()),(unsigned long)streams[s][0].gaps,(unsigned long)streams[s][1].gaps,(unsigned long)streams[s][0].backwards,(unsigned long)streams[s][1].backwards); }
  PX4_INFO("heater mask=%ld target=%.2f duty=%.1f/%.1f%% warm=%d/%d",(long)heat_en,(double)heat_target,(double)(100*heater[0].duty()),(double)(100*heater[1].duty()),heater[0].warm(hrt_absolute_time()),heater[1].warm(hrt_absolute_time()));
  PX4_INFO("heater gains P=%.4f/%.4f I=%.5f/%.5f",(double)heat_kp[0],(double)heat_kp[1],(double)heat_ki[0],(double)heat_ki[1]);
  if(iface) { uint32_t r[8]; can.driver.getIface(0)->diagnosticRegisters(r); PX4_INFO("CAN CCCR=%08lx PSR=%08lx FQS=%08lx BRP=%08lx BTO=%08lx BCF=%08lx IR=%08lx IE=%08lx",(unsigned long)r[0],(unsigned long)r[1],(unsigned long)r[2],(unsigned long)r[3],(unsigned long)r[4],(unsigned long)r[5],(unsigned long)r[6],(unsigned long)r[7]); }
  return 0;
 }
 void run() override;
private:
 uavcan_stm32h7::CanInitHelper<32> can{1};
 uavcan::ICanIface *iface=nullptr;
 uORB::Subscription accel[2]{{ORB_ID(sensor_accel_fifo),0},{ORB_ID(sensor_accel_fifo),1}};
 uORB::Subscription gyro[2]{{ORB_ID(sensor_gyro_fifo),0},{ORB_ID(sensor_gyro_fifo),1}};
 uORB::Subscription temperatures[2]{{ORB_ID(sensor_accel),0},{ORB_ID(sensor_accel),1}};
 Stream streams[2][2];
 float coefficients[2][2][3][3]{};
 float temp[2]{NAN,NAN},heat_target=48,hz=60;
 uint64_t temp_at[2]{};
 HeaterControl heater[2];
 float heat_kp[2]{0.10f,0.10f},heat_ki[2]{0.01f,0.01f};
 bool calibrated[2]{},config_ok=true;
 int32_t source=0,tx_en=1,heat_en=0,epoch=1;
 uint32_t boot=0,sent=0,tx_drop=0,incomplete=0,orb_lost=0;
 uint16_t seq=0;
 template<class T> void parameter(const char *name,T &value) { param_t p=param_find(name); if(p!=PARAM_INVALID) { param_get(p,&value); } }
 void settings();
 template<class T> void ingest(const T &m,int kind);
 void heaters(uint64_t now);
 bool send(uint32_t id,const uint8_t *data,unsigned length=8);
 void packet(uint32_t id,uint16_t number,uint8_t *p,unsigned size);
 void output(uint64_t end);
 void service(uint64_t now);
};
void CanImu::settings() {
 parameter("CI_SOURCE",source); parameter("CI_TX_EN",tx_en); parameter("CI_HEAT_EN",heat_en);
 parameter("CI_HEAT_T",heat_target); parameter("CI_LPF_HZ",hz); parameter("CI_EPOCH",epoch);
 // ICM polling timestamps are diagnostic only; fusion needs validated timing and lever arms.
 if(source<0 || source>1) { config_ok=false; PX4_ERR("Unsupported source"); }
 if(!std::isfinite(hz)||hz<1||hz>90) { config_ok=false; }
 if(!std::isfinite(heat_target)||heat_target<30||heat_target>50||heat_en<0||heat_en>3) { config_ok=false; }
 for(int s=0;s<2;++s) {
  int32_t ac=0,gc=0; char name[17];
  snprintf(name,sizeof(name),"CI%d_HEAT_P",s); parameter(name,heat_kp[s]);
  snprintf(name,sizeof(name),"CI%d_HEAT_I",s); parameter(name,heat_ki[s]);
  if(!std::isfinite(heat_kp[s]) || heat_kp[s]<0 || heat_kp[s]>1 || !std::isfinite(heat_ki[s]) || heat_ki[s]<0 || heat_ki[s]>0.1f) { config_ok=false; }
  snprintf(name,sizeof(name),"CI%d_ACAL",s); parameter(name,ac);
  snprintf(name,sizeof(name),"CI%d_GCAL",s); parameter(name,gc); calibrated[s]=ac&&gc;
  for(int k=0;k<2;++k) { for(int a=0;a<3;++a) { for(int c=0;c<3;++c) {
   snprintf(name,sizeof(name),"CI%d_%c%c%c",s,"AG"[k],"XYZ"[a],"BST"[c]);
   float &v=coefficients[s][k][a][c]; v=c==1?1.0f:0.0f; parameter(name,v);
   if(!std::isfinite(v)||(c==1&&(v<0.5f||v>1.5f))) { v=c==1?1.0f:0.0f; calibrated[s]=false; config_ok=false; }
  } } }
 }
 // Commit the boot counter before emitting any frames. Failed persistence stops startup.
 int32_t counter=0; parameter("CI_BOOT",counter);
 counter=counter>=2147483646?1:counter+1;
 if(param_set(param_find("CI_BOOT"),&counter)==0 && param_save_default(true)==0) { boot=uint32_t(counter); }

}
template<class T> void CanImu::ingest(const T &m,int kind) {
 const unsigned type=(m.device_id>>16)&255;
 const int s=type==0x34?1:((type==0x6a||type==0x66)?0:-1);
 if(s<0||m.samples==0||m.samples>32||!(m.dt>0)||!std::isfinite(m.scale)) { return; }
 const uint64_t span=uint64_t(float(m.samples-1)*m.dt+0.5f);
 if(m.timestamp_sample<=span) { return; }
 for(unsigned i=0;i<m.samples;++i) {
  const int16_t raw[3]{m.x[i],m.y[i],m.z[i]}; float v[3]; bool clipped=false;
  for(int a=0;a<3;++a) {
   const float *c=coefficients[s][kind][a];
   v[a]=(float(raw[a])*m.scale-c[0]-c[2]*(std::isfinite(temp[s])?temp[s]-48.0f:0.0f))*c[1];
   clipped|=raw[a]>=32760||raw[a]<=-32760;
  }
  streams[s][kind].add(m.timestamp_sample-uint64_t(float(m.samples-1-i)*m.dt+0.5f),v,m.dt,hz,clipped,m.timestamp_source!=1);
 }
}
bool CanImu::send(uint32_t id,const uint8_t *data,unsigned length) {
 uavcan::CanFrame f; f.id=id; f.dlc=length; memcpy(f.data,data,length);
 return iface->send(f,uavcan_stm32h7::clock::getMonotonic()+uavcan::MonotonicDuration::fromUSec(3000),0)>0;
}
void CanImu::packet(uint32_t id,uint16_t number,uint8_t *p,unsigned size) {
 put16(p+size-2,packet_crc(id,number,p,size-2));
 for(unsigned i=0;i<size/6;++i) { uint8_t d[8]; put16(d,number); memcpy(d+2,p+6*i,6); if(!send(id+i,d)) { ++tx_drop; return; } }
 ++sent;
}
void CanImu::output(uint64_t end) {
 float a[3],g[3]; bool clipped=false,timing_bad=false;
 const bool valid=streams[source][0].get(end,a,clipped,&timing_bad)&&streams[source][1].get(end,g,clipped,&timing_bad);
 ++seq;
 if(!valid) { ++incomplete; return; }
 if(!tx_en) { return; }
 uint8_t p[48]{}; put64(p,end);
 for(int k=0;k<3;++k) { putfloat(p+8+4*k,a[k]); putfloat(p+20+4*k,g[k]); }
 put32(p+32,boot); put16(p+36,WindowUs);
 uint16_t flags=Valid|(calibrated[source]?Calibrated:0)|(timing_bad?PollTimestamp:0)|(clipped?Clipped:0)|(heater[source].fault()!=HeaterControl::None?HeaterFault:0);
 if(heater[source].warm(end)) { flags|=Warm; }
 if(clipped) { flags&=~Valid; }
 put16(p+38,flags); put16(p+40,std::isfinite(temp[source])?int16_t(temp[source]*100):0x8000);
 p[42]=source; p[43]=1; put16(p+44,epoch); packet(DataId,seq,p,sizeof(p));
}
void CanImu::heaters(uint64_t now) {
 for(int s=0;s<2;++s) {
  heater[s].update(now,temp[s],temp_at[s],heat_en & (1<<s),heat_target,heat_kp[s],heat_ki[s]);
  if(s==0) { HEATER1_OUTPUT_EN(heater[s].on()); } else { HEATER2_OUTPUT_EN(heater[s].on()); }
 }
}
void CanImu::service(uint64_t now) {
 uavcan::ICanDriver &driver=can.driver;
 uavcan::CanSelectMasks masks; masks.read=1; masks.write=0;
 const uavcan::CanFrame *pending[uavcan::MaxCanIfaces]{};
 driver.select(masks,pending,uavcan_stm32h7::clock::getMonotonic());
 uavcan::CanFrame f; uavcan::MonotonicTime mt; uavcan::UtcTime utc; uavcan::CanIOFlags flags;
 static uint64_t last_reply=0,last_status=0;
 for(int n=0;n<8 && iface->receive(f,mt,utc,flags)>0;++n) {
  if(f.id!=SyncRequestId||f.dlc!=2||now-last_reply<50000) { continue; }
  last_reply=now; uint8_t p[24]{};
  const uint64_t age=uavcan_stm32h7::clock::getMonotonic().toUSec()-mt.toUSec();
  put64(p,hrt_absolute_time()-age); put64(p+8,hrt_absolute_time()); put32(p+16,boot); put16(p+20,1);
  packet(SyncReplyId,uint16_t(f.data[0])|(uint16_t(f.data[1])<<8),p,24);
 }
 if(now-last_status>=1000000) {
  last_status=now; uint8_t p[48]{}; put64(p,now); put32(p+8,boot);
  put32(p+12,tx_drop); put32(p+16,incomplete); put32(p+20,uint32_t(iface->getErrorCount())); put32(p+24,orb_lost);
  for(int s=0;s<2;++s) { put32(p+28+4*s,streams[s][0].gaps+streams[s][1].gaps); put16(p+36+2*s,std::isfinite(temp[s])?int16_t(temp[s]*100):0x8000); }
  p[40]=source; p[41]=1; put16(p+42,epoch); p[44]=(heater[0].fault()!=HeaterControl::None)|((heater[1].fault()!=HeaterControl::None)<<1); p[45]=calibrated[0]|(calibrated[1]<<1);
  packet(StatusId,uint16_t(now/1000000),p,48);
 }
}
void CanImu::run() {
 px4_arch_configgpio(GPIO_HEATER1_OUTPUT); px4_arch_configgpio(GPIO_HEATER2_OUTPUT);
 HEATER1_OUTPUT_EN(false); HEATER2_OUTPUT_EN(false); settings();
 if(!config_ok) { PX4_ERR("Invalid IMU configuration"); return; }
 if(!boot) { PX4_ERR("Cannot persist boot session, refusing CAN startup"); return; }
 // The standalone module does not construct a UAVCAN node to initialize this clock.
 (void)uavcan_stm32h7::SystemClock::instance();
 if(can.init(uint32_t(1000000))<0) { PX4_ERR("CAN init failed"); return; }
 iface=can.driver.getIface(0);
 uavcan::CanFilterConfig filter; filter.id=SyncRequestId; filter.mask=uavcan::CanFrame::MaskExtID|uavcan::CanFrame::FlagEFF|uavcan::CanFrame::FlagRTR;
 if(iface->configureFilters(&filter,1)<0) { PX4_ERR("CAN filter failed"); return; }
 const uint64_t heat_start=hrt_absolute_time();
 for(auto &h:heater) { h.start(heat_start); }
 uint64_t next=(heat_start/WindowUs+2)*WindowUs;
 while(!should_exit()) {
  for(int i=0;i<2;++i) {
   sensor_accel_s temperature;
   if(temperatures[i].update(&temperature)) { const unsigned type=(temperature.device_id>>16)&255; const int s=type==0x34?1:type==0x6a?0:-1;
    if(s>=0) { temp[s]=temperature.temperature; temp_at[s]=temperature.timestamp_temperature; }
   }
   sensor_accel_fifo_s a; sensor_gyro_fifo_s g;
   for(int n=0;n<16;++n) { const unsigned before=accel[i].get_last_generation(); if(!accel[i].update(&a)) { break; } const unsigned delta=accel[i].get_last_generation()-before; if(before && delta>1) { orb_lost+=delta-1; } ingest(a,0); }
   for(int n=0;n<16;++n) { const unsigned before=gyro[i].get_last_generation(); if(!gyro[i].update(&g)) { break; } const unsigned delta=gyro[i].get_last_generation()-before; if(before && delta>1) { orb_lost+=delta-1; } ingest(g,1); }
  }
  const uint64_t now=hrt_absolute_time();
  heaters(now);
  if(now>next+3000) { if(now-next>50000) { incomplete+=uint32_t((now-next)/WindowUs); next=(now/WindowUs)*WindowUs; } else { output(next); next+=WindowUs; } }
  service(now); px4_usleep(250);
 }
 HEATER1_OUTPUT_EN(false); HEATER2_OUTPUT_EN(false);
}
extern "C" __EXPORT int can_imu_main(int argc,char *argv[]) { return CanImu::main(argc,argv); }
