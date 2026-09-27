#pragma once
#include <cstdint>
#include <cstring>
#include <cmath>

namespace canimu {
constexpr uint32_t DataId = 0x680;
constexpr uint32_t StatusId = 0x6a0;
constexpr uint32_t SyncRequestId = 0x6c0;
constexpr uint32_t SyncReplyId = 0x6b0;
constexpr uint64_t WindowUs = 5000;
enum Flags : uint16_t { Valid=1, Calibrated=2, Warm=4, PollTimestamp=8, Clipped=16, HeaterFault=32 };
inline void put16(uint8_t *p, uint16_t v) { p[0]=v; p[1]=v>>8; }
inline void put32(uint8_t *p, uint32_t v) { for (int i=0;i<4;++i) { p[i]=v>>(8*i); } }
inline void put64(uint8_t *p, uint64_t v) { for (int i=0;i<8;++i) { p[i]=v>>(8*i); } }
inline void putfloat(uint8_t *p, float v) { uint32_t u; memcpy(&u,&v,4); put32(p,u); }
inline uint16_t crc(const uint8_t *p, unsigned n, uint16_t c=0xffff) {
 for (unsigned i=0;i<n;++i) { c^=uint16_t(p[i])<<8; for (int b=0;b<8;++b) { c=(c&0x8000)?(c<<1)^0x1021:c<<1; } } return c;
}
inline uint16_t packet_crc(uint32_t id, uint16_t seq, const uint8_t *p, unsigned n) {
 uint8_t prefix[6]; put32(prefix,id); put16(prefix+4,seq); return crc(p,n,crc(prefix,6));
}
struct Bin { uint64_t end=0; float integral[3]{}; float covered=0; bool clipped=false; bool timing_bad=false; };
// Integrate the interpolated, filtered signal on a common device-time grid.
// A missing interval is never filled; incomplete bins are rejected downstream.
class Stream {
public:
 Bin bins[16]{};
 uint64_t last=0;
 uint32_t gaps=0, backwards=0;
 bool previous_clipped=false, previous_polled=false;
 float previous[3]{}, stage1[3]{}, stage2[3]{};
 void add(uint64_t t, const float *v, float nominal_dt, float hz, bool clipped, bool polled=false) {
  if (!t || !(nominal_dt>0)) { return; }
  if (last && t<=last) { ++backwards; return; }
  const uint64_t elapsed=last?t-last:0;
  if (!last || elapsed>uint64_t(nominal_dt*3.0f)) {
   if (last) { ++gaps; }
   for(int k=0;k<3;++k) { previous[k]=stage1[k]=stage2[k]=v[k]; }
   last=t; previous_clipped=clipped; previous_polled=polled; return;
  }
  const float a=1.0f-expf(-6.283185307f*hz*float(elapsed)*1e-6f);
  float value[3];
  for (int k=0;k<3;++k) { stage1[k]+=a*(v[k]-stage1[k]); stage2[k]+=a*(stage1[k]-stage2[k]); value[k]=stage2[k]; }
  uint64_t begin=last;
  while (begin<t) {
   const uint64_t end=(begin/WindowUs+1)*WindowUs;
   const uint64_t stop=t<end?t:end;
   Bin &b=bins[(end/WindowUs)%16];
   if (b.end!=end) { b=Bin{}; b.end=end; }
   const float f0=float(begin-last)/float(elapsed), f1=float(stop-last)/float(elapsed);
   for (int k=0;k<3;++k) { b.integral[k]+=(previous[k]+(value[k]-previous[k])*(f0+f1)*0.5f)*float(stop-begin); }
   b.covered+=float(stop-begin); b.clipped|=clipped||previous_clipped; b.timing_bad|=polled||previous_polled; begin=stop;
  }
  for(int k=0;k<3;++k) { previous[k]=value[k]; } last=t; previous_clipped=clipped; previous_polled=polled;
 }
 bool get(uint64_t end, float *v, bool &clipped, bool *timing_bad=nullptr) const {
  const Bin &b=bins[(end/WindowUs)%16];
  if (b.end!=end || b.covered<float(WindowUs)-0.5f || b.covered>float(WindowUs)+0.5f) { return false; }
  for(int k=0;k<3;++k) { v[k]=b.integral[k]/float(WindowUs); } clipped|=b.clipped; if(timing_bad) { *timing_bad|=b.timing_bad; } return true;
 }
};
}
