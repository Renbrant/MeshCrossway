// Exercise unchanged browser folder/state functions with deterministic DOM/time/HTTP.
const fs=require('fs'),vm=require('vm'),assert=require('assert'),path=require('path');
const root=path.resolve(__dirname,'..'),source=fs.readFileSync(root+'/firmware/display-prototype/meshcrossway_companion_page.h','utf8');
function segment(start,end){return source.slice(source.indexOf(start),source.indexOf(end,source.indexOf(start)));}
class Node{
 constructor(tag='div',text=''){this.tag=tag;this.children=[];this.text=text;this.style={};this.attrs={};this._value='';this.disabled=false;this.dataset={};}
 append(...nodes){this.children.push(...nodes);if(this.tag==='select'&&!this._value&&this.options.length)this._value=String(this.options[0].value);}
 prepend(...nodes){this.children.unshift(...nodes);}
 replaceChildren(...nodes){this.children=[];this._value='';this.append(...nodes);}
 setAttribute(k,v){this.attrs[k]=v;}
 get options(){return this.children.filter(c=>c.tag==='option');}
 get value(){return this._value;}
 set value(v){this._value=String(v);}
 get textContent(){return this.text+this.children.map(c=>c.textContent).join('');}
 set textContent(v){this.text=String(v);this.children=[];}
}
const nodes=new Map(),$=id=>{if(!nodes.has(id))nodes.set(id,new Node(id==='folder-filter'||id==='record-mode'?'select':'div'));return nodes.get(id);};
$('folder-filter').value='all';$('track-search').value='';
let now=0,reads=0,posts=0,active=0,peak=0,transient=0,malformed=0,delayReads=0,pending=null,apply=true,interval;
let device={revision:1,mode:0,current:0,status:'Ready',selected:0,folders_ready:true,folders:[{id:1,name:'Existing'}],presets:[{name:'Walking',distance:5,interval:2}],tracks:[{id:5,name:'Saved route',folder:0,metres:1500,points:6,color:0xff8a24}]};
const context={console,Set,Number,JSON,Date:{now:()=>now},document:{getElementById:$,hidden:false,activeElement:{matches:()=>false}},$: $,navMessage:$('nav-message'),navData:null,selectedTrackId:0,previewEpoch:0,routeMap:{clear(){},draw(){}},view:'tracks',refreshChat(){},recordState(){},renderTrackTools(){},selectTrack(){},element:(tag,text='')=>new Node(tag,text),setTimeout:(fn,ms)=>{now+=ms;queueMicrotask(fn);},setInterval:fn=>{interval=fn;},async readJSON(r){return JSON.parse(await r.text());},async request(url,options){
 if(url==='/navigation/command'){posts++;pending=options.body.split('|');delayReads=5;transient=1;malformed=1;return {ok:true,text:async()=> 'Command queued'};}
 assert.equal(url,'/navigation');reads++;active++;peak=Math.max(peak,active);await Promise.resolve();active--;
 if(pending&&--delayReads<=0&&apply){const [a,id,value,,,name]=pending;switch(Number(a)){case 16:device.folders.push({id:2,name});break;case 17:device.folders.find(f=>f.id===+id).name=name;break;case 18:device.folders=device.folders.filter(f=>f.id!==+id);device.tracks.forEach(t=>{if(t.folder===+id)t.folder=0;});break;case 19:device.tracks.find(t=>t.id===+id).folder=+value;break;}pending=null;device.revision++;}
 if(transient-->0)return {ok:false,text:async()=> 'Navigation busy'};
 if(malformed-->0)return {ok:true,text:async()=> '{"status":"Incomplete"}'};
 return {ok:true,text:async()=>JSON.stringify(device)};
}};
vm.createContext(context);
vm.runInContext(segment('let navRefreshPromise=','let selectedTrackId=')+segment('function folderName(','function previewSummary(')+segment('async function loadNavigation(){',"$('folder-filter').onchange="),context);
(async()=>{
 const call=js=>vm.runInContext(js,context);
 await call('refreshNavigation()');assert.equal(context.navData.folders[0].name,'Existing');
 const oldReads=reads;await Promise.all(Array.from({length:20},()=>call('refreshNavigation()')));assert.equal(reads-oldReads,1);assert.equal(peak,1);
 await call("navCommand(16,0,0,0,0,'Weekend hikes')");assert.equal(posts,1);assert.equal(context.navData.folders.length,2);assert.equal($('folder-filter').value,'2');assert.match($('nav-message').textContent,/confirmed/);assert.ok(reads>=oldReads+6);
 // A bad refresh cannot erase the last complete folder/track list.
 const saved=JSON.stringify(context.navData);transient=1;assert.equal(await call('refreshNavigation()'),null);assert.equal(JSON.stringify(context.navData),saved);
 malformed=1;assert.equal(await call('refreshNavigation()'),null);assert.equal(JSON.stringify(context.navData),saved);
 await call("navCommand(19,5,2)");assert.equal(context.navData.tracks[0].folder,2);
 await call("navCommand(17,2,0,0,0,'Renamed')");assert.equal(context.navData.folders[1].name,'Renamed');
 await call('navCommand(18,2)');assert.equal(context.navData.folders.length,1);assert.equal(context.navData.tracks[0].folder,0);
 // Accepted-but-never-saved is never reported as confirmed; concurrent commands
 // cannot duplicate the operation. Virtual time bounds the full wait.
 apply=false;const start=now,beforePosts=posts;await Promise.all([call("navCommand(16,0,0,0,0,'Not saved')"),call("navCommand(16,0,0,0,0,'Duplicate')")]);assert.equal(posts-beforePosts,1);assert.ok(now-start>=15000);assert.doesNotMatch($('nav-message').textContent,/confirmed on device/);assert.equal(context.navData.folders.length,1);
 // Device-side changes arrive in the regular Tracks refresh without a reload.
 apply=true;pending=null;device.folders.push({id:3,name:'From device'});device.revision++;
 vm.runInContext(source.slice(source.indexOf('setInterval(()=>'),source.indexOf('},4000);')+8),context);interval();await new Promise(resolve=>setImmediate(resolve));assert.equal(context.navData.folders[1].name,'From device');
 console.log('PASS: production folder UI delayed saves, transient/malformed responses, preserved lists, single-flight refresh, rename/move/delete, duplicate suppression, timeout honesty and device-side refresh');
})().catch(e=>{console.error(e);process.exitCode=1;});
