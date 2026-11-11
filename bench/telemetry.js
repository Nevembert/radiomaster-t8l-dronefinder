'use strict';
// T8L Controller v1.7 USB query. Not CRSF and not an update command.
const LINK_QUERY = Object.freeze([0xa5,0x55,0x26,0x0d,0x0a]);
function crc8(bytes) {
  let crc=0;
  for (const b of bytes) {
    crc^=b;
    for (let i=0;i<8;i++) crc=((crc<<1)^((crc&128)?0xd5:0))&255;
  }
  return crc;
}
function decodeLink(f) {
  if(f.length!==14 || f[0]!==0x67 || f[1]!==12 || crc8(f.slice(2,13))!==f[13]) return null;
  const rssi=f[9]>=128?f[9]-256:f[9];
  const valid=rssi>=-127 && rssi<0;
  return {rssiDbm:valid?rssi:null,rawRssi:f[9],uplinkRssi1Raw:f[2],uplinkRssi2Raw:f[3],
    uplinkLq:f[4],downlinkLbt:f[10],connectFlag:f[12],
    bars:valid && f[12]===1 ? (rssi>=-35?4:rssi>=-70?3:rssi>=-90?2:1):0,
    note:'USB RAM snapshot; repeated values do not prove a fresh RF packet'};
}
class LinkStream {
  constructor(onFrame) { this.buffer=[]; this.onFrame=onFrame; this.badCrc=0; this.bytes=0; }
  feed(chunk,timeMs) {
    this.bytes+=chunk.length;
    // Bytewise bounded accumulator: at most 14 pending bytes, even for noise.
    for (const b of chunk) {
      this.buffer.push(b);
      while(this.buffer.length) {
        if(this.buffer[0]!==0x67) {this.buffer.shift(); continue;}
        if(this.buffer.length<2) break;
        if(this.buffer[1]!==12) {this.buffer.shift(); continue;}
        if(this.buffer.length<14) break;
        const frame=this.buffer.slice(0,14), value=decodeLink(frame);
        if(!value) {this.badCrc++; this.buffer.shift(); continue;}
        this.buffer.splice(0,14);
        this.onFrame({timeMs,frame,...value});
      }
    }
  }
}
if(typeof module!=='undefined') module.exports={LINK_QUERY,crc8,decodeLink,LinkStream};
