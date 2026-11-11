// Exercise the real page script with a synthetic Web Serial device and tiny DOM.
// No browser rendering, real USB or external traffic is involved.
const assert=require('node:assert/strict');
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const root=path.join(__dirname,'..');
const html=fs.readFileSync(path.join(root,'bench/T8L_Bench.html'),'utf8');
const elements=new Map();
const canvas={clearRect(){},beginPath(){},moveTo(){},lineTo(){},stroke(){}};
function element(id){if(!elements.has(id))elements.set(id,{value:id==='baud'?'420000':id==='phase'?'off_before':'',disabled:false,textContent:'',getContext:()=>canvas});return elements.get(id);}
let ledFlags=[],timerId=0,timers=new Map(),savedBlob=null,downloadName=null,now=0;
const {crc8}=require('../bench/telemetry.js');
const f=[0x67,12,80,80,100,0,0,2,1,176,100,0,1];f.push(crc8(f.slice(2)));
let receive,writes=[],closed=0,opened=0;
const port={
  getInfo:()=>({usbVendorId:123,usbProductId:456}),
  async open(options){assert.equal(options.baudRate,420000);opened++;},
  readable:new ReadableStream({start(c){receive=c;}}),
  writable:new WritableStream({write(bytes){writes.push([...bytes]);receive.enqueue(Uint8Array.from(f.slice(0,5)));receive.enqueue(Uint8Array.from(f.slice(5)));}}),
  async close(){closed++;}
};
const context=vm.createContext({
  document:{getElementById:element,querySelectorAll:()=>Array.from({length:4},(_,i)=>({classList:{toggle(_,v){ledFlags[i]=v;}}})),
    createElement:()=>({click(){downloadName=this.download;}})},
  navigator:{serial:{requestPort:async()=>port}},performance:{now:()=>now},
  Blob,URL:{createObjectURL(b){savedBlob=b;return 'blob:local-test';},revokeObjectURL(){}},
  setInterval(fn){timers.set(++timerId,fn);return timerId;},clearInterval(id){timers.delete(id);},
  setTimeout(fn){fn();},console,Date,Uint8Array
});
vm.runInContext(fs.readFileSync(path.join(root,'bench/telemetry.js'),'utf8'),context);
vm.runInContext([...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m=>m[1]).join('\n'),context);
async function flush(){for(let i=0;i<8;i++)await Promise.resolve();}
(async()=>{
  await element('connect').onclick();await flush();
  assert.equal(opened,1);assert.equal(element('rssi').textContent,'-80 dBm');
  assert.deepEqual(ledFlags,[true,true,false,false]);
  assert.equal(element('disconnect').disabled,false);
  now=1000;element('phase').value='near';element('phase').onchange();
  for(const fn of [...timers.values()])await fn();await flush();
  element('save').onclick();
  const report=JSON.parse(await savedBlob.text());
  assert.equal(downloadName,'T8L_Bench_Report.json');assert.equal(report.format,'t8l-bench-0.6');
  assert.equal(report.records.length,2);assert.equal(report.records[1].phase,'near');
  assert.deepEqual(report.phases.map(p=>p.phase),['off_before','near']);
  assert(writes.every(w=>JSON.stringify(w)==='[165,85,38,13,10]'));
  now=4000;vm.runInContext('render()',context);
  assert.equal(element('rssi').textContent,'— dBm');assert(ledFlags.every(x=>!x));
  await element('disconnect').onclick();
  assert.equal(closed,1);assert.equal(element('connect').disabled,false);
  assert.equal(element('disconnect').disabled,true);assert.equal(timers.size,1); // render timer only
  element('phase').value='off_after';element('phase').onchange();
  element('save').onclick();const stopped=JSON.parse(await savedBlob.text());assert.equal(stopped.records.length,2);
  assert.equal(stopped.phases.length,2);assert.equal(stopped.recording,false);assert.equal(stopped.lastFrameMs,1000);
  assert(stopped.transport.some(e=>e.kind==='disconnected'));assert.equal(element('phase').disabled,true);
  console.log('bench UI: connect, split receive, phases, JSON export, stale display, disconnect/retained report PASS (synthetic DOM/serial)');
})().catch(e=>{console.error(e);process.exitCode=1;});
