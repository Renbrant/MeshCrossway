"use strict";
const $=id=>document.getElementById(id);
const app={data:null,network:"mt",topic:"",messageView:"chats",tab:"dashboard",tracks:null,selectedTrack:"",busy:false,map:null,mapAreas:[],mapLimit:2400};
function element(tag,label,cls){const e=document.createElement(tag);if(label!==undefined)e.textContent=String(label);if(cls)e.className=cls;return e;}
function clear(e){e.replaceChildren();}
function btn(label,fn){const b=element("button",label);b.type="button";b.addEventListener("click",fn);return b;}
function notice(text,error){$("notice").textContent=text;$("notice").style.color=error?"#ff9292":"#72d8a5";}
async function api(url,method="GET",body){
 const options={method,credentials:"same-origin",cache:"no-store",headers:{Accept:"application/json"}};
 if(method!=="GET")options.headers["X-MCX-Request"]="1";
 if(body!==undefined){options.headers["Content-Type"]="application/json";options.body=JSON.stringify(body);}
 const response=await fetch(url,options),raw=await response.text();let data;
 try{data=JSON.parse(raw);}catch{throw Error("Invalid phone response");}
 if(response.status===401&&url!=="/api/login")showLogin();
 if(!response.ok||data.error)throw Error(data.error||"Phone request failed");
 return data;
}
function showLogin(){$("login").hidden=false;$("app").hidden=true;$("logout").hidden=true;app.data=null;}
function showApp(){$("login").hidden=true;$("app").hidden=false;$("logout").hidden=false;}
function network(){return app.data?.radios?.find(r=>r.network===app.network);}
function time(value){return value?new Date(value*1000).toLocaleString():"Unknown";}
async function reload(){app.data=await api("/api/state");showApp();renderHome();if(app.tab==="messages")renderMessageDirectory();}
function renderHome(){
 const holder=$("radio-cards");clear(holder);
 for(const radio of app.data?.radios||[]){
  const card=element("section",undefined,"card");
  card.style.borderTop="3px solid "+(radio.network==="mt"?"#ff8a24":"#6cbde6");
  card.append(element("h2",radio.name),element("strong",radio.status+" · "+(radio.device||"Device not chosen"),"metric"));
  card.append(element("p",radio.nodes+" contacts · "+radio.unread+" unread · "+radio.channels.length+" channels"));
  const shortcuts=element("div",undefined,"actions");
  shortcuts.append(btn("Chats",()=>{app.network=radio.network;app.messageView="chats";app.topic="";showTab("messages");}));
  shortcuts.append(btn("Channels",()=>{app.network=radio.network;app.messageView="channels";app.topic="";showTab("messages");}));
  shortcuts.append(btn("Nodes",()=>{app.network=radio.network;app.messageView="nodes";app.topic="";showTab("messages");}));
  card.append(shortcuts);
  holder.append(card);
 }
}
function showTab(tab){
 app.tab=tab;
 document.querySelectorAll("[data-panel]").forEach(e=>e.hidden=e.id!==tab);
 document.querySelectorAll("[data-tab]").forEach(e=>e.setAttribute("aria-selected",String(e.dataset.tab===tab)));
 if(tab==="messages")renderMessageDirectory();
 if(tab==="maps")renderMaps();
 if(tab==="tracks")renderTracks();
}
function resetSelected(){
 app.topic="";
 $("conversation-title").textContent="Select a chat, channel or node";
 $("selected-context").textContent="Select an entry from the left to read or send messages.";
 clear($("message-list"));
 $("send-message").disabled=true;
 $("mark-read").disabled=true;
}
function switchNetwork(value){
 if(app.network===value)return;
 app.network=value;
 resetSelected();
 renderMessageDirectory();
}
function changeMessageView(view){
 if(!["chats","channels","nodes"].includes(view))return;
 app.messageView=view;
 renderMessageDirectory();
}
function renderMessageDirectory(){
 const radio=network(),holder=$("message-directory");
 clear(holder);
 $("switch-mt").style.borderColor=app.network==="mt"?"#ff8a24":"#34404b";
 $("switch-mc").style.borderColor=app.network==="mc"?"#6cbde6":"#34404b";
 document.querySelectorAll("[data-message-view]").forEach(b=>{
  b.setAttribute("aria-selected",String(b.dataset.messageView===app.messageView));
 });
 $("view-heading").textContent={chats:"Chats",channels:"Channels",nodes:"Nodes"}[app.messageView];
 if(!radio){
  holder.append(element("p","No radio data yet."));return;
 }
 const conversations=radio.conversations||[],channels=radio.channels||[],contacts=radio.contacts||[];
 const query=$("conversation-search").value.trim().toLowerCase();
 const byTopic=new Map(conversations.map(c=>[c.topic,c]));
 let entries=[];
 if(app.messageView==="chats"){
  entries=conversations.filter(c=>!c.group).map(c=>({
   topic:c.topic,title:c.title,preview:c.preview,unread:c.unread,
   time:c.time,context:"Private chat",enabled:true
  }));
 }else if(app.messageView==="channels"){
  const found=new Set();
  for(const channel of channels){
   const conversation=byTopic.get(channel.topic);
   entries.push({
    topic:channel.topic,title:channel.name||"Channel "+channel.slot,
    preview:conversation?.preview||channel.preview||"",
    unread:conversation?.unread||channel.unread||0,
    time:conversation?.time||channel.time||0,
    context:"Channel "+channel.slot+(channel.enabled?"":" · disabled"),
    enabled:channel.enabled
   });
   found.add(channel.topic);
  }
  // MeshCore group conversations may not appear in the configured channel slots.
  for(const c of conversations.filter(c=>c.group&&!found.has(c.topic))){
   entries.push({topic:c.topic,title:c.title,preview:c.preview,unread:c.unread,
                 time:c.time,context:"Group conversation",enabled:true});
  }
 }else{
  entries=contacts.map(c=>{
   const conversation=byTopic.get(c.topic);
   return {topic:c.topic,title:c.name||c.id,
           preview:conversation?.preview||("Last heard: "+time(c.heard)),
           unread:conversation?.unread||0,time:conversation?.time||c.heard,
           context:"Direct message · "+c.id,enabled:c.messageable!==false};
  });
 }
 if(app.messageView!=="nodes")entries.sort((a,b)=>(b.unread>0)-(a.unread>0)||b.time-a.time||a.title.localeCompare(b.title));
 else entries.sort((a,b)=>b.time-a.time||a.title.localeCompare(b.title));
 const filtered=entries.filter(e=>
  (e.title+" "+e.preview+" "+e.context).toLowerCase().includes(query));
 $("view-count").textContent=filtered.length+" "+app.messageView+
     " · "+(radio.ready?"Radio connected":radio.status);
 if(!filtered.length){
  const emptyState=element("div",undefined,"item");
  const label=app.messageView==="chats"?
    "No private chats yet. Open Nodes to start a direct conversation.":
    app.messageView==="channels"?
    "No configured channels or group messages are available from this radio yet.":
    "No nodes received. Connect your radio and let it synchronize.";
  emptyState.append(element("p",query?"No matches. Clear the search to see all entries.":label));
  if(app.messageView==="chats"&&!query)
   emptyState.append(btn("Open Nodes",()=>changeMessageView("nodes")));
  holder.append(emptyState);return;
 }
 for(const entry of filtered){
  const row=element("button",undefined,"item"+(entry.topic===app.topic?" selected":""));
  row.type="button";row.style.width="100%";row.style.textAlign="left";
  row.disabled=!entry.enabled;
  row.append(element("strong",entry.title+(entry.unread?" · "+entry.unread+" unread":"")));
  row.append(element("div",entry.context,"muted"));
  row.append(element("div",entry.preview||"No messages yet","muted"));
  if(entry.time)row.append(element("div",time(entry.time),"muted"));
  row.addEventListener("click",()=>selectConversation(entry.topic,entry.title,entry.context));
  holder.append(row);
 }
 // Refresh never discards a selected conversation or disables its composer.
 if(app.topic)$("send-message").disabled=!radio.ready;
}
async function selectConversation(topic,title,context){
 if(!topic)return;
 const changed=app.topic!==topic;
 app.topic=topic;
 $("conversation-title").textContent=title;
 $("selected-context").textContent=(app.network==="mt"?"Meshtastic":"MeshCore")+" · "+context;
 $("send-message").disabled=!network()?.ready;
 $("mark-read").disabled=false;
 if(changed)$("message-text").value="";
 renderMessageDirectory();
 try{await renderMessages();}
 catch(e){notice(e.message,true);}
}
async function renderMessages(){
 if(!app.topic)return;
 const chosenTopic=app.topic,chosenNetwork=app.network;
 const response=await api("/api/messages?network="+encodeURIComponent(chosenNetwork)+
                          "&topic="+encodeURIComponent(chosenTopic));
 // Ignore responses from a previously selected network or conversation.
 if(app.topic!==chosenTopic||app.network!==chosenNetwork)return;
 const list=$("message-list");clear(list);
 if(!response.messages.length){
  list.append(element("p","There are no messages in this conversation yet.","muted"));
  return;
 }
 for(const message of response.messages){
  const row=element("div",undefined,"message"+(message.outgoing?" outgoing":""));
  row.append(element("div",(message.sender||"Unknown")+" · "+time(message.time)+" · "+message.state,"meta"));
  row.append(element("div",message.body));
  list.append(row);
 }
 list.scrollTop=list.scrollHeight;
}
async function sendMessage(event){
 event.preventDefault();if(!app.topic)return;
 const message=$("message-text").value.trim();if(!message)return;
 $("send-message").disabled=true;
 try{
  const result=await api("/api/send","POST",{network:app.network,topic:app.topic,text:message});
  notice(result.status||"Send completed",!result.ok);
  if(result.ok){$("message-text").value="";await reload();await renderMessages();}
 }catch(e){notice(e.message,true);}
 finally{$("send-message").disabled=!app.topic;}
}
async function actionMap(data){
 try{const result=await api("/api/maps/action","POST",data);notice("Map request accepted"+(result.estimatedTiles?" · "+result.estimatedTiles+" estimated tiles":""));await renderMaps();}
 catch(e){notice(e.message,true);}
}
async function renderMaps(){
 try{
  const data=await api("/api/maps"),holder=$("area-list");clear(holder);
  if(data.error)notice(data.error,true);
  if(!data.areas.length)holder.append(element("p","No saved offline areas"));
  for(const a of data.areas){
   const row=element("div",undefined,"item");
   row.append(element("strong",a.name),element("p",a.status+" · "+a.tiles+" tiles · "+(a.bytes/1000000).toFixed(2)+" MB · zoom "+a.zoom));
   row.append(element("p","N "+a.north+" · S "+a.south+" · W "+a.west+" · E "+a.east,"muted"));
   if(a.active)row.append(btn("Pause",()=>actionMap({action:"pause"})));
   else if(!a.complete)row.append(btn("Resume",()=>actionMap({action:"resume",id:a.id})));
   row.append(btn("Use bounds",()=>{
    const f=$("area-form");
    for(const key of ["north","south","east","west"])f.elements[key].value=a[key];
    f.elements.name.value=a.name+" copy";f.elements.zoom.value=a.zoom;
   }));
   row.append(btn("Delete",()=>{if(confirm("Delete offline area "+a.name+"?"))actionMap({action:"delete",id:a.id});}));
   holder.append(row);
  }
 }catch(e){notice(e.message,true);}
}
async function areaSubmit(event){
 event.preventDefault();const f=event.currentTarget;
 const payload={action:"create",name:f.elements.name.value,style:f.elements.style.value};
 for(const key of ["north","south","east","west","zoom"])payload[key]=Number(f.elements[key].value);
 if(payload.north<=payload.south||payload.west>=payload.east){notice("Invalid bounds",true);return;}
 await actionMap(payload);
}
async function trackAction(data){
 try{await api("/api/tracks/action","POST",data);notice("Change saved to phone");await renderTracks();}
 catch(e){notice(e.message,true);}
}
async function renderTracks(){
 try{
  app.tracks=await api("/api/tracks");
  const f=$("folder-list"),t=$("track-list");clear(f);clear(t);
  for(const folder of app.tracks.folders){
   const row=element("div",undefined,"item");row.append(element("strong",folder.name));
   row.append(btn("Rename",()=>{
    const name=prompt("New name",folder.name);
    if(name?.trim())trackAction({action:"folderRename",id:folder.id,name:name.trim()});
   }));
   row.append(btn("Delete",()=>{
    if(confirm("Delete folder? Tracks are moved to Unfiled."))trackAction({action:"folderDelete",id:folder.id});
   }));f.append(row);
  }
  for(const track of app.tracks.tracks){
   const row=element("div",undefined,"item");
   row.append(element("strong",track.name),element("p",track.state+" · "+track.count+" points · "+(track.distance/1000).toFixed(2)+" km"));
   row.append(btn("Preview / edit",()=>selectTrack(track.id)));t.append(row);
  }
 }catch(e){notice(e.message,true);}
}
async function selectTrack(id){
 const track=app.tracks?.tracks.find(t=>t.id===id);if(!track)return;app.selectedTrack=id;
 const holder=$("track-actions");clear(holder);
 const name=element("input");name.value=track.name;name.maxLength=80;
 const folder=element("select");
 for(const item of [{id:"",name:"Unfiled"},...app.tracks.folders]){
  const option=element("option",item.name);option.value=item.id;folder.append(option);
 }folder.value=track.folder||"";
 const palette=[0xff72d8a5,0xffff8a24,0xff6cbde6,0xffe68bcc],color=element("select");
 for(let i=0;i<palette.length;i++){const option=element("option",["Mint","Orange","Blue","Pink"][i]);option.value=palette[i]|0;color.append(option);}
 color.value=String(track.color);
 const visible=element("input");visible.type="checkbox";visible.checked=track.visible;visible.style.width="auto";
 const label=element("label","Show track on map ");label.prepend(visible);
 holder.append(element("label","Name"),name,element("label","Folder"),folder,element("label","Color"),color,label);
 const controls=element("div",undefined,"actions");
 controls.append(btn("Save",async()=>{
  await trackAction({action:"metadata",id,name:name.value,color:Number(color.value),visible:visible.checked});
  await trackAction({action:"move",id,folder:folder.value});
 }));
 controls.append(btn("Export GPX",()=>{
  const link=element("a");link.href="/api/tracks/export?id="+encodeURIComponent(id);
  link.download="MeshCrossway-track.gpx";link.click();
 }));
 controls.append(btn("Delete",()=>{
  if(track.state!=="saved"){notice("Finish track on phone first",true);return;}
  if(confirm("Permanently delete track?"))trackAction({action:"delete",id});
 }));holder.append(controls);
 try{
  const points=(await api("/api/tracks/preview?id="+encodeURIComponent(id))).points;
  drawTrack(points);$("track-preview-info").textContent=track.name+" · "+points.length+" preview points";
 }catch(e){notice(e.message,true);}
}
function drawTrack(points){
 const canvas=$("track-map"),ctx=canvas.getContext("2d"),w=canvas.width,h=canvas.height;
 ctx.fillStyle="#0b1920";ctx.fillRect(0,0,w,h);ctx.strokeStyle="#274252";ctx.lineWidth=1;
 for(let i=1;i<8;i++){ctx.beginPath();ctx.moveTo(i*w/8,0);ctx.lineTo(i*w/8,h);ctx.stroke();}
 if(!points.length){ctx.fillStyle="white";ctx.fillText("No points",20,28);return;}
 const lat=points.map(p=>p[0]),lon=points.map(p=>p[1]),north=Math.max(...lat),south=Math.min(...lat),east=Math.max(...lon),west=Math.min(...lon);
 const dx=Math.max(east-west,0.00001),dy=Math.max(north-south,0.00001);
 let segment=null;ctx.lineWidth=3;ctx.strokeStyle="#72d8a5";
 for(const point of points){
  const x=20+(point[1]-west)/dx*(w-40),y=20+(north-point[0])/dy*(h-40);
  if(segment!==point[2]){if(segment!==null)ctx.stroke();ctx.beginPath();ctx.moveTo(x,y);segment=point[2];}
  else ctx.lineTo(x,y);
 }ctx.stroke();
}
async function pulse(){
 if(app.busy||$("app").hidden)return;app.busy=true;
 try{await reload();if(app.tab==="messages"&&app.topic)await renderMessages();if(app.tab==="maps")await renderMaps();if(app.tab==="tracks")await renderTracks();}
 catch(e){notice(e.message,true);}finally{app.busy=false;}
}
$("login-form").addEventListener("submit",async e=>{
 e.preventDefault();$("login-error").textContent="";
 try{await api("/api/login","POST",{code:$("code").value});$("code").value="";await reload();}
 catch(err){$("login-error").textContent=err.message;}
});
$("logout").addEventListener("click",async()=>{try{await api("/api/logout","POST");}catch{}showLogin();});
document.querySelectorAll("[data-tab]").forEach(button=>button.addEventListener("click",()=>showTab(button.dataset.tab)));
$("switch-mt").addEventListener("click",()=>switchNetwork("mt"));
$("switch-mc").addEventListener("click",()=>switchNetwork("mc"));
document.querySelectorAll("[data-message-view]").forEach(b=>
 b.addEventListener("click",()=>changeMessageView(b.dataset.messageView)));
$("refresh-messages").addEventListener("click",pulse);
$("conversation-search").addEventListener("input",renderMessageDirectory);
$("compose-form").addEventListener("submit",sendMessage);
$("mark-read").addEventListener("click",async()=>{
 try{await api("/api/read","POST",{network:app.network,topic:app.topic});await reload();notice("Conversation marked read");}
 catch(err){notice(err.message,true);}
});
$("refresh-maps").addEventListener("click",renderMaps);
$("area-form").addEventListener("submit",areaSubmit);
$("folder-form").addEventListener("submit",async e=>{
 e.preventDefault();const name=$("folder-name").value.trim();
 if(name){await trackAction({action:"folderCreate",name});$("folder-name").value="";}
});
setInterval(pulse,5000);
reload().catch(showLogin);