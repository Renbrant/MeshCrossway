#pragma once
// Served from firmware memory; no external JavaScript or internet dependency.
static const char MESHCROSSWAY_REMOTE_PAGE[]=R"MESHHTML(<!doctype html><html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>MeshCrossway / Remote Control</title>
<style>
*{box-sizing:border-box}body{margin:20px;background:#090c10;color:#f2f4f7;font:16px system-ui;text-align:center}
h1{color:#ff8a24}p{color:#b7c3cd}
canvas{display:block;width:min(960px,100%);height:auto;image-rendering:pixelated;margin:16px auto;border:2px solid #ff8a24;border-radius:8px;background:#15191f;touch-action:none;cursor:crosshair}
button{background:#ff8a24;color:#161616;border:0;padding:11px 18px;border-radius:9px;font-weight:800;cursor:pointer;margin:3px}
input{padding:12px;background:#19232e;border:1px solid #34495a;border-radius:8px;color:white;font:inherit;max-width:180px}
[hidden]{display:none!important}small{color:#a9b6c2}
</style></head><body><h1>MeshCrossway / Remote Control</h1>
<p>Live 480 × 320 screen. Click or touch this image to operate the physical device.</p>
<section id="pair"><p>Enter the eight-digit pairing code shown on Settings of your MeshCrossway.</p>
<input id="code" type="password" inputmode="numeric" maxlength="8" placeholder="Pairing code" autocomplete="off">
<button id="pairButton">Pair device</button></section>
<section id="viewer" hidden><canvas id="screen" width="480" height="320"></canvas>
<button id="save">Save PNG screenshot</button><button id="forget">Disconnect viewer</button></section>
<p id="status" role="status">Enter the pairing code.</p>
<small>Local development feature. Use trusted Wi-Fi only; do not forward port 80.</small>
<script>
const canvas=document.getElementById('screen'),ctx=canvas.getContext('2d');
let secret='',paired=false,pending=Promise.resolve(),hold=null,active=false,lastX=0,lastY=0,started=0,lastMove=0;
function status(s){document.getElementById('status').textContent=s;}
function coords(e){
 const r=canvas.getBoundingClientRect();
 return [Math.max(0,Math.min(479,Math.floor((e.clientX-r.left)*480/r.width))),
         Math.max(0,Math.min(319,Math.floor((e.clientY-r.top)*320/r.height)))];
}
function reset(){
 paired=false;secret='';active=false;clearInterval(hold);hold=null;
 document.getElementById('viewer').hidden=true;
 document.getElementById('pair').hidden=false;
}
async function call(path,opts={}){
 const headers=Object.assign({'X-MCX-Pin':secret},opts.headers||{});
 const res=await fetch(path,Object.assign({cache:'no-store'},opts,{headers}));
 if(res.status===403){reset();throw Error('Pairing rejected');}
 return res;
}
async function pair(){
 const value=document.getElementById('code').value.trim();
 if(!/^[0-9]{8}$/.test(value)){status('Use the eight-digit code on the display.');return;}
 secret=value;
 try{
  const r=await call('/health');
  if(!r.ok)throw Error('HTTP '+r.status);
  paired=true;document.getElementById('pair').hidden=true;document.getElementById('viewer').hidden=false;
  document.getElementById('code').value='';
  status('Paired. Remote touch enabled.');
  refresh();
 }catch(e){reset();status(e.message);}
}
async function refresh(){
 if(!paired)return;
 try{
  const r=await call('/screen.raw?t='+Date.now());
  if(!r.ok)throw Error('Snapshot HTTP '+r.status);
  const bytes=new Uint8Array(await r.arrayBuffer());
  if(bytes.length!==480*320*2)throw Error('Frame size '+bytes.length);
  const image=ctx.createImageData(480,320);
  for(let i=0;i<480*320;i++){
   const v=bytes[2*i]|(bytes[2*i+1]<<8),j=i*4;
   image.data[j]=((v>>>11)&31)*255/31;
   image.data[j+1]=((v>>>5)&63)*255/63;
   image.data[j+2]=(v&31)*255/31;image.data[j+3]=255;
  }
  ctx.putImageData(image,0,0);
  status('Connected • '+new Date().toLocaleTimeString());
 }catch(e){if(paired)status(e.message);}
}
function send(action,x,y){
 if(!paired)return;
 pending=pending.catch(()=>{}).then(async()=>{
  const r=await call('/touch',{method:'POST',headers:{'Content-Type':'text/plain'},body:action+','+x+','+y});
  if(!r.ok)throw Error('Touch HTTP '+r.status);
 }).catch(e=>{if(paired)status(e.message);});
}
canvas.addEventListener('pointerdown',e=>{
 if(!paired || active)return;
 e.preventDefault();canvas.setPointerCapture(e.pointerId);
 active=true;[lastX,lastY]=coords(e);started=Date.now();
 send('d',lastX,lastY);
 hold=setInterval(()=>{if(active)send('m',lastX,lastY);},300);
});
canvas.addEventListener('pointermove',e=>{
 if(!active)return;
 e.preventDefault();[lastX,lastY]=coords(e);
 if(Date.now()-lastMove>=65){send('m',lastX,lastY);lastMove=Date.now();}
});
function release(e){
 if(!active)return;
 active=false;
 if(e && typeof e.clientX==='number')[lastX,lastY]=coords(e);
 clearInterval(hold);hold=null;
 const wait=Math.max(0,120-(Date.now()-started));
 const x=lastX,y=lastY;
 setTimeout(()=>send('u',x,y),wait);
}
canvas.addEventListener('pointerup',release);
canvas.addEventListener('pointercancel',release);
window.addEventListener('blur',()=>release(null));
document.getElementById('pairButton').onclick=pair;
document.getElementById('code').addEventListener('keydown',e=>{if(e.key==='Enter')pair();});
document.getElementById('save').onclick=()=>{
 const link=document.createElement('a');link.download='meshcrossway-screen.png';
 link.href=canvas.toDataURL('image/png');link.click();
};
document.getElementById('forget').onclick=()=>{release(null);reset();status('Disconnected.');};
setInterval(()=>{if(paired)refresh();},1500);
</script></body></html>)MESHHTML";
