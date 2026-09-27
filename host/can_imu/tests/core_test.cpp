#include "ImuCore.hpp"
#include <cassert>
#include <cstdio>
using namespace canimu;
int main() {
 assert(crc(reinterpret_cast<const uint8_t *>("123456789"),9)==0x29b1);
 Stream accel,gyro; float v[3]{1,2,3};
 for(uint64_t t=1000;t<=16000;t+=625) { accel.add(t,v,625,60,false); }
 for(uint64_t t=1000;t<=16000;t+=500) { gyro.add(t,v,500,60,false); }
 float out[3];bool clip=false;
 assert(!accel.get(5000,out,clip)); assert(accel.get(10000,out,clip));
 assert(out[0]==1 && out[1]==2 && out[2]==3); assert(gyro.get(10000,out,clip));
 accel.add(30000,v,625,60,false); assert(accel.gaps==1); assert(!accel.get(25000,out,clip));
 accel.add(29000,v,625,60,false); assert(accel.backwards==1);
 Stream clipped;
 for(uint64_t t=1000;t<=16000;t+=500) { clipped.add(t,v,500,60,t==7000); }
 clip=false;assert(clipped.get(10000,out,clip)&&clip);
 Stream polled;
 for(uint64_t t=1000;t<=16000;t+=500) { polled.add(t,v,500,60,false,t==7000); }
 bool timing_bad=false;clip=false;
 assert(polled.get(10000,out,clip,&timing_bad)&&timing_bad&&!clip);
 uint8_t p[48]{};put64(p,5000);for(int i=0;i<3;++i) { putfloat(p+8+4*i,float(i+1));putfloat(p+20+4*i,float(i+1)*.1f); }
 put32(p+32,1);put16(p+36,5000);put16(p+38,1);put16(p+40,4800);p[43]=1;put16(p+44,1);put16(p+46,packet_crc(DataId,1,p,46));
 for(auto b:p) { printf("%02x",b); }puts("");
}
