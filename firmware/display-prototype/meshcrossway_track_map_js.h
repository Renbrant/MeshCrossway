#pragma once
// Original small XYZ map viewer. No third-party JavaScript dependency.
static const char TRACK_MAP_JS[]=R"MCXJS((function(global){
'use strict';
const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
function project(lat,lon){lat=clamp(lat,-85.05112878,85.05112878);const s=Math.sin(lat*Math.PI/180);return {x:(lon+180)/360,y:clamp(.5-Math.log((1+s)/(1-s))/(4*Math.PI),0,1)};}
function unproject(x,y){return {lat:Math.atan(Math.sinh(Math.PI*(1-2*y)))*180/Math.PI,lon:x*360-180};}
function unwrap(points){let previous=null;return points.map(p=>{let lon=p[1];if(previous!==null)lon+=Math.round((previous-lon)/360)*360;previous=lon;const q=project(p[0],lon);q.segment=p[2];return q;});}
class TrackMap{
 constructor(container,canvas,status){this.container=container;this.canvas=canvas;this.status=status;this.tiles=new Map();this.points=[];this.center=project(0,0);this.zoom=14;this.background=true;this.color='#f07120';this.scheduled=false;this.layer=document.createElement('div');this.layer.className='tile-layer';container.prepend(this.layer);this.drag=null;
 container.addEventListener('pointerdown',e=>{if(e.button!==0)return;e.preventDefault();container.setPointerCapture(e.pointerId);this.drag={id:e.pointerId,x:e.clientX,y:e.clientY,center:{...this.center}};});
 container.addEventListener('pointermove',e=>{if(!this.drag||e.pointerId!==this.drag.id)return;const scale=256*2**this.zoom;this.center={x:this.drag.center.x-(e.clientX-this.drag.x)/scale,y:clamp(this.drag.center.y-(e.clientY-this.drag.y)/scale,0,1)};this.draw();});
 for(const type of ['pointerup','pointercancel','lostpointercapture'])container.addEventListener(type,()=>{this.drag=null;});
 container.addEventListener('wheel',e=>{e.preventDefault();const r=container.getBoundingClientRect();this.changeZoom(e.deltaY<0?1:-1,e.clientX-r.left,e.clientY-r.top);},{passive:false});
 this.resize=new ResizeObserver(()=>this.draw());this.resize.observe(container);
 }
 setTrack(track,geometry){this.color='#'+Number(track.color).toString(16).padStart(6,'0');this.points=unwrap(geometry.points||[]);this.fit();}
 fit(){if(!this.points.length){this.draw();return;}let left=Infinity,right=-Infinity,top=Infinity,bottom=-Infinity;for(const p of this.points){left=Math.min(left,p.x);right=Math.max(right,p.x);top=Math.min(top,p.y);bottom=Math.max(bottom,p.y);}const w=Math.max(80,this.container.clientWidth-80),h=Math.max(80,this.container.clientHeight-80);const ratio=Math.min(w/Math.max(right-left,1e-7),h/Math.max(bottom-top,1e-7));this.zoom=clamp(Math.floor(Math.log2(ratio/256)),3,19);this.center={x:(left+right)/2,y:(top+bottom)/2};this.draw();}
 changeZoom(delta,x=this.container.clientWidth/2,y=this.container.clientHeight/2){const next=clamp(this.zoom+delta,3,19),old=256*2**this.zoom,scale=256*2**next;this.center={x:this.center.x+(x-this.container.clientWidth/2)*(1/old-1/scale),y:clamp(this.center.y+(y-this.container.clientHeight/2)*(1/old-1/scale),0,1)};this.zoom=next;this.draw();}
 setBackground(enabled){this.background=enabled;this.draw();}
 clear(){this.points=[];this.draw();}
 draw(){if(this.scheduled)return;this.scheduled=true;requestAnimationFrame(()=>{this.scheduled=false;this.paint();});}
 paint(){const w=this.container.clientWidth,h=this.container.clientHeight;if(!w||!h)return;const dpr=Math.min(2,global.devicePixelRatio||1);if(this.canvas.width!==Math.round(w*dpr)||this.canvas.height!==Math.round(h*dpr)){this.canvas.width=Math.round(w*dpr);this.canvas.height=Math.round(h*dpr);}const ctx=this.canvas.getContext('2d');ctx.setTransform(dpr,0,0,dpr,0,0);ctx.clearRect(0,0,w,h);const used=new Set(),scale=256*2**this.zoom,left=this.center.x*scale-w/2,top=this.center.y*scale-h/2,n=2**this.zoom;
 if(this.points.length&&this.background){let requests=0;for(let y=Math.floor(top/256);y<=Math.floor((top+h)/256);++y)for(let x=Math.floor(left/256);x<=Math.floor((left+w)/256);++x){if(y<0||y>=n||requests++>=32)continue;const wrapped=((x%n)+n)%n,key=this.zoom+'/'+wrapped+'/'+y;let tile=this.tiles.get(key);if(!tile){const img=new Image();img.alt='';img.draggable=false;img.decoding='async';img.style.opacity='0';img.referrerPolicy='origin';tile={img,failed:false,loaded:false,used:0};img.onload=()=>{tile.loaded=true;img.style.opacity='1';this.draw();};img.onerror=()=>{tile.failed=true;this.draw();};img.src='https://tile.openstreetmap.org/'+key+'.png';this.tiles.set(key,tile);}tile.used=Date.now();used.add(key);tile.img.style.transform='translate('+Math.round(x*256-left)+'px,'+Math.round(y*256-top)+'px)';if(tile.img.parentNode!==this.layer)this.layer.append(tile.img);}}
 for(const img of [...this.layer.children]){let keep=false;for(const key of used)if(this.tiles.get(key).img===img){keep=true;break;}if(!keep)img.remove();}
 if(this.tiles.size>64)for(const [key,tile]of [...this.tiles].sort((a,b)=>a[1].used-b[1].used)){if(this.tiles.size<=64)break;if(!used.has(key)){tile.img.onload=tile.img.onerror=null;this.tiles.delete(key);}}
 if(!this.points.length){this.status.textContent='Select a saved track to preview its route.';return;}
 ctx.strokeStyle=this.color;ctx.lineWidth=4;ctx.lineJoin='round';ctx.lineCap='round';ctx.beginPath();let previous=null;for(const p of this.points){const x=p.x*scale-left,y=p.y*scale-top;if(!previous||previous.segment!==p.segment)ctx.moveTo(x,y);else ctx.lineTo(x,y);previous=p;}ctx.stroke();
 for(const [i,p]of [this.points[0],this.points[this.points.length-1]].entries()){const x=p.x*scale-left,y=p.y*scale-top;ctx.beginPath();ctx.arc(x,y,6,0,Math.PI*2);ctx.fillStyle=i?this.color:'#ffffff';ctx.fill();ctx.lineWidth=2;ctx.strokeStyle='#15202b';ctx.stroke();}
 const visible=[...used].map(k=>this.tiles.get(k)),failed=visible.filter(t=>t.failed).length,loading=visible.some(t=>!t.failed&&!t.loaded);this.status.textContent=!this.background?'Route preview · background off':failed?'Map background unavailable in some areas · route remains visible':loading?'Loading map background…':'Drag to explore · scroll or use + / − to zoom';
 }
}
TrackMap.project=project;TrackMap.unproject=unproject;TrackMap.unwrap=unwrap;global.MCXTrackMap=TrackMap;
})(typeof window==='undefined'?globalThis:window);
)MCXJS";
