'use strict';
const assert=require('node:assert/strict');
const {crc8,decodeLink,LinkStream,LINK_QUERY}=require('../bench/telemetry.js');
assert.deepEqual(LINK_QUERY,[0xa5,0x55,0x26,0x0d,0x0a]);
function frame(rssi=-80,flag=1){const f=[0x67,12,80,80,100,0,0,2,1,rssi&255,100,0,flag];return [...f,crc8(f.slice(2))];}
const good=frame();assert.equal(decodeLink(good).rssiDbm,-80);
assert.equal(decodeLink(good).bars,2);
for(let split=0;split<=14;split++){
  const got=[],stream=new LinkStream(f=>got.push(f));
  stream.feed(good.slice(0,split),1);stream.feed(good.slice(split),2);
  assert.equal(got.length,1);assert.equal(got[0].rssiDbm,-80);
}
const bad=good.slice();bad[13]^=1;
const got=[],s=new LinkStream(f=>got.push(f));
s.feed([...bad,...good,...frame(-40)],10);
assert.equal(got.length,2);assert.equal(s.badCrc,1);
s.feed(Array(100000).fill(0x67),11);assert(s.buffer.length<=13);
s.feed(good,12);assert.equal(got.length,3);
for(let i=0;i<256;i++){
  const f=frame(i),d=decodeLink(f);
  assert.equal(d.rssiDbm,i>=129?i-256:null);
  assert.equal(decodeLink(frame(i,0)).bars,0);
}
for(let byte=2;byte<14;byte++)for(let bit=0;bit<8;bit++){
  const f=good.slice();f[byte]^=1<<bit;assert.equal(decodeLink(f),null);
}
const fs=require('node:fs');
const html=fs.readFileSync(require('node:path').join(__dirname,'../bench/T8L_Bench.html'),'utf8');
for(const script of html.matchAll(/<script>([\s\S]*?)<\/script>/g))new Function(script[1]);
assert.equal((html.match(/writer\.write\(/g)||[]).length,1);
assert(html.includes('writer.write(Uint8Array.from(LINK_QUERY))'));
assert(!/\b(fetch|XMLHttpRequest|WebSocket|eval)\s*\(/.test(html));
console.log('bench: framing, split/noisy input, CRC, signed RSSI, bounded memory, sole query command and script syntax PASS');
