#pragma once
// v0.6: HTTP status and PIN-free manual read-only capture.
// No LVGL calls from HTTP thread and no remote input.
#include "meshcrossway_network.h"
#include "meshcrossway_sd_history.h"
#include "meshcrossway_meshcore.h"
#include "meshcrossway_capture.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "meshcrossway_navigation_http.h"
#include <atomic>
#include <stdio.h>

namespace meshhealth {
static std::atomic<bool> running{false};
static bool is_running(){return running.load();}
static const char PAGE[]=R"MCX(<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>MeshCrossway Remote</title>
<style>
body{background:#090c10;color:#f2f4f7;font:16px system-ui;max-width:720px;margin:24px auto;padding:0 20px}
h1{color:#ff8a24}section{background:#181d24;padding:20px;border-radius:12px;margin:14px 0}
dl{display:grid;grid-template-columns:1fr 1fr;gap:12px}dd{margin:0}small{color:#9aa6b4}
.controls{display:flex;align-items:center;gap:10px;flex-wrap:wrap}
button{font:inherit;padding:12px 16px;background:#ff8a24;color:#090c10;border:0;border-radius:6px;cursor:pointer}
button:disabled{opacity:.5;cursor:wait}canvas[hidden]{display:none}canvas{display:block;width:100%;max-width:480px;image-rendering:pixelated;border:1px solid #9aa6b4;margin-top:14px}
form{background:#242b34;padding:14px;margin:12px 0;border-radius:8px}form label{display:grid;grid-template-columns:140px 1fr;gap:8px;margin:8px 0}input{font:inherit;max-width:100%;box-sizing:border-box;padding:8px}input[type=color]{width:48px;height:40px;padding:2px;vertical-align:middle}button{margin:4px}a{color:#ff8a24}
</style><h1>MeshCrossway Remote</h1><p>Experimental manual screen view · v0.8.8</p>
<section><p id="connection" role="status">Connecting to device…</p><dl>
<dt>Firmware</dt><dd id="version">—</dd><dt>Uptime</dt><dd id="uptime">—</dd>
<dt>Free heap</dt><dd id="heap">—</dd><dt>Internal heap</dt><dd id="internal">—</dd><dt>Reset reason / boot</dt><dd id="reset">—</dd><dt>Previous activity</dt><dd id="activity">—</dd><dt>Maximum UI operation</dt><dd id="ui-ms">—</dd><dt>Local IP</dt><dd id="ip">—</dd><dt>Wi-Fi SD backup</dt><dd id="sd">—</dd><dt>Chat SD history</dt><dd id="history">—</dd><dt>Last history operation</dt><dd id="history-ms">—</dd><dt>Manual captures</dt><dd id="captures">—</dd><dt>Last capture</dt><dd id="latency">—</dd></dl></section>
<section><h2>Read-only screen capture</h2>
<p>Press Capture screen to take a screenshot. No PIN is required. Captures are manual and limited to one every five seconds.</p>
<div class="controls"><button type="button" id="capture">Capture screen</button></div>
<p role="status" id="capture-status">No capture requested.</p><canvas id="screen" width="480" height="320" hidden></canvas></section>
<section><h2>Tracks and recording</h2><p id="nav-status">Loading tracks…</p>
<div class="controls"><button onclick="navCommand(1)">Start</button><button onclick="navCommand(2)">Pause</button><button onclick="navCommand(3)">Resume</button><button onclick="navCommand(4)">Finish and save GPX</button><button onclick="refreshNavigation()">Refresh</button></div>
<p>Saved points remain on the microSD. Up to eight tracks may be visible together. Changes apply on the device.</p><div id="track-list"></div></section>
<section><h2>Recording presets</h2><p>Points require both the minimum distance and interval. Low-speed GPS drift is filtered; elapsed time alone never adds a stationary point. Preset changes apply to the current recording.</p><div id="preset-list"></div><p id="nav-message" role="status"></p></section>
<p>Remote touch is disabled. Keep using the physical touchscreen normally.</p>
<small>Development only: HTTP is unencrypted. Use a trusted local network; never forward device port 80 to the Internet.</small>
<script>
async function update(){
 const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),5000);
 try{const r=await fetch('/health',{cache:'no-store',signal:controller.signal});if(!r.ok)throw Error();
 const d=await r.json();
 document.getElementById('connection').textContent='Device online · '+(d.screen?'manual view available':'status only');
 document.getElementById('version').textContent=d.version;
 document.getElementById('uptime').textContent=Math.floor(d.uptime_ms/1000)+' s';
 document.getElementById('heap').textContent=Math.floor(d.free_heap/1024)+' KB';
 document.getElementById('internal').textContent=String(d.internal_free??0)+' B; largest '+String(d.internal_largest??0)+' B';
 document.getElementById('reset').textContent=String(d.reset_reason??0)+' / '+String(d.boot_count??1);
 document.getElementById('activity').textContent=d.previous_activity||'idle';
 document.getElementById('ui-ms').textContent=String(d.ui_max_ms??0)+' ms';
 document.getElementById('ip').textContent=d.ip||'Not connected';
 document.getElementById('sd').textContent=d.sd_backup||'unknown';
 document.getElementById('history').textContent=d.history||'unknown';
 document.getElementById('history-ms').textContent=String(d.history_ms??0)+' ms (max '+String(d.history_max_ms??0)+' ms)';
 document.getElementById('captures').textContent=String(d.captures??0);
 document.getElementById('latency').textContent=String(d.capture_ms??0)+' ms';
 document.getElementById('capture').disabled=taking||!d.screen;
 }catch(e){document.getElementById('connection').textContent='Device unreachable · retrying';}
 finally{clearTimeout(timeout);setTimeout(update,3000);}
}
const button=document.getElementById('capture'),message=document.getElementById('capture-status');
let taking=false;
button.addEventListener('click',async()=>{
 taking=true;button.disabled=true;message.textContent='Capturing once…';
 try{
  const response=await fetch('/screen.raw',{cache:'no-store'});
  if(!response.ok)throw Error((await response.text()).slice(0,100)||'Capture error '+response.status);
  const bytes=await response.arrayBuffer();
  if(bytes.byteLength!==480*320*2)throw Error('Unexpected image size');
  const swap=response.headers.get('X-MCX-RGB565-Swap')==='1';
  const data=new DataView(bytes),canvas=document.getElementById('screen'),ctx=canvas.getContext('2d');
  const frame=ctx.createImageData(480,320);
  for(let i=0;i<480*320;i++){
   const color=data.getUint16(i*2,!swap),j=i*4;
   frame.data[j]=((color>>11)&31)*255/31;
   frame.data[j+1]=((color>>5)&63)*255/63;
   frame.data[j+2]=(color&31)*255/31;
   frame.data[j+3]=255;
  }
  ctx.putImageData(frame,0,0);canvas.hidden=false;message.textContent='Read-only snapshot received.';
 }catch(error){message.textContent='Capture unavailable: '+error.message;}
 finally{taking=false;button.disabled=false;}
});
const navMessage=document.getElementById('nav-message');
async function navCommand(action,id=0,value=0,distance=0,interval=0,name=''){
 try{const r=await fetch('/navigation/command',{method:'POST',headers:{'X-MCX-Request':'1','Content-Type':'text/plain'},body:[action,id,value,distance,interval,name].join('|')});const text=await r.text();if(!r.ok)throw Error(text);navMessage.textContent=text;setTimeout(refreshNavigation,900);}catch(e){navMessage.textContent=e.message;}
}
function element(tag,text){const e=document.createElement(tag);if(text!==undefined)e.textContent=text;return e;}
async function refreshNavigation(){try{const r=await fetch('/navigation',{cache:'no-store'});if(!r.ok)throw Error(await r.text());const data=await r.json();document.getElementById('nav-status').textContent=data.status+' · current track '+data.current;
 const list=document.getElementById('track-list');list.replaceChildren();for(const t of data.tracks){const row=element('section');row.append(element('h3',(t.name||('Track '+t.id))+' · '+(t.metres/1000).toFixed(2)+' km · '+t.points+' points'));const name=element('input');name.value=t.name||('Track '+t.id);name.maxLength=39;name.setAttribute('aria-label','Track name');row.append(name);const rename=element('button','Rename');rename.onclick=()=>navCommand(13,t.id,0,0,0,name.value);row.append(rename);const remove=element('button','Delete');remove.disabled=t.id===data.current&&(data.mode===1||data.mode===2);remove.onclick=()=>{if(confirm('Delete '+name.value+' and its GPX?'))navCommand(14,t.id);};row.append(remove);const visible=element('input');visible.type='checkbox';visible.checked=t.visible;visible.onchange=()=>navCommand(8,t.id,Number(visible.checked));const label=element('label',' Show on map ');label.prepend(visible);row.append(label);const color=element('input');color.type='color';color.value='#'+t.color.toString(16).padStart(6,'0');color.setAttribute('aria-label','Track '+t.id+' color');color.onchange=()=>navCommand(9,t.id,parseInt(color.value.slice(1),16));row.append(color);const exportButton=element('button','Export GPX');exportButton.onclick=()=>navCommand(12,t.id);row.append(exportButton);if(t.gpx){const link=element('a',' Download GPX');link.href='/navigation/download?id='+t.id;link.download='track-'+t.id+'.gpx';row.append(link);}list.append(row);}
 const presets=document.getElementById('preset-list');presets.replaceChildren();data.presets.forEach((p,i)=>{const form=element('form');const name=element('input');name.value=p.name;name.maxLength=23;name.required=true;name.pattern='[A-Za-z0-9 _-]+';const distance=element('input');distance.type='number';distance.min=3;distance.max=1000;distance.value=p.distance;distance.required=true;const interval=element('input');interval.type='number';interval.min=1;interval.max=300;interval.value=p.interval;interval.required=true;for(const [text,input]of [['Name ',name],['Distance (m) ',distance],['Interval (s) ',interval]]){const label=element('label',text);label.append(input);form.append(label);}const save=element('button','Save preset');save.type='submit';form.append(save);const use=element('button',data.selected===i?'Selected':'Use preset');use.type='button';use.onclick=()=>navCommand(10,i);form.append(use);form.onsubmit=e=>{e.preventDefault();navCommand(11,i,0,Number(distance.value),Number(interval.value),name.value);};presets.append(form);});
 }catch(e){document.getElementById('nav-status').textContent=e.message;}}
refreshNavigation();
setInterval(async()=>{try{const r=await fetch('/navigation',{cache:'no-store'});if(r.ok){const d=await r.json();document.getElementById('nav-status').textContent=d.status+' · current track '+d.current;}}catch(e){}},3000);
update();
</script></html>)MCX";
static void headers(httpd_req_t *req,const char *type){
  httpd_resp_set_type(req,type);
  httpd_resp_set_hdr(req,"Cache-Control","no-store");
  httpd_resp_set_hdr(req,"X-Content-Type-Options","nosniff");
  httpd_resp_set_hdr(req,"Content-Security-Policy","default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'; form-action 'self'");
}
static esp_err_t home(httpd_req_t *req){
  headers(req,"text/html; charset=utf-8");
  return httpd_resp_send(req,PAGE,HTTPD_RESP_USE_STRLEN);
}
static esp_err_t health(httpd_req_t *req){
  char ip[16],state[30],body[1152],history[96];
  mcxhistory::describe(history,sizeof(history));
  meshnetwork::get_status(ip,sizeof(ip),state,sizeof(state));
  const int n=snprintf(body,sizeof(body),
    "{\"version\":\"v0.8.8\",\"uptime_ms\":%llu,\"free_heap\":%lu,\"ip\":\"%s\",\"sd_backup\":\"%s\",\"sd_step\":\"%s\",\"sd_errno\":%d,\"screen\":%s,\"touch\":false,\"captures\":%lu,\"capture_failures\":%lu,\"capture_ms\":%lu,\"history\":\"%s\",\"history_step\":\"%s\",\"history_errno\":%d,\"history_failures\":%lu,\"history_ms\":%lu,\"history_max_ms\":%lu,\"internal_free\":%lu,\"internal_largest\":%lu,\"internal_min\":%lu,\"reset_reason\":%d,\"boot_count\":%lu,\"previous_activity\":\"%s\",\"ui_max_ms\":%lu,\"ui_stack_min\":%lu,\"allocation_failures\":%lu,\"allocation_bytes\":%lu,\"allocation_caps\":%lu,\"mailbox_peak\":%lu,\"mailbox_overflows\":%lu}",
    (unsigned long long)(esp_timer_get_time()/1000),(unsigned long)esp_get_free_heap_size(),ip,
    meshbackup::status(),meshbackup::step_name(meshbackup::failure_step.load()),
    meshbackup::failure_errno.load(),meshcapture::available()?"true":"false",
    (unsigned long)meshcapture::captures.load(),(unsigned long)meshcapture::failures.load(),
    (unsigned long)meshcapture::last_capture_ms.load(),history,
    mcxstore::step_name(mcxhistory::error_step.load()),mcxhistory::error_number.load(),
    (unsigned long)mcxhistory::failures.load(),(unsigned long)mcxhistory::operation_ms.load(),
    (unsigned long)mcxhistory::max_operation_ms.load(),
    (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
    (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
    (unsigned long)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
    (int)esp_reset_reason(),(unsigned long)mcxruntime::boots.load(),mcxruntime::stage_name(mcxruntime::previous_stage.load()),
    (unsigned long)mcxruntime::ui_max_ms.load(),(unsigned long)mcxruntime::ui_stack_min.load(),
    (unsigned long)mcxruntime::allocation_failures.load(),(unsigned long)mcxruntime::last_allocation_bytes.load(),
    (unsigned long)mcxruntime::last_allocation_caps.load(),
    (unsigned long)meshmc::mailbox_peak.load(),(unsigned long)meshmc::mailbox_overflows.load());
  if(n<0 || n>=(int)sizeof(body))return ESP_FAIL;
  headers(req,"application/json");
  return httpd_resp_send(req,body,n);
}
static void start(){
  if(running.load())return;
  httpd_config_t config=HTTPD_DEFAULT_CONFIG();
  config.server_port=80;config.stack_size=4096;
  config.max_open_sockets=2;config.max_uri_handlers=6;
  config.recv_wait_timeout=2;config.send_wait_timeout=2;
  config.lru_purge_enable=true;
  httpd_handle_t server=nullptr;
  esp_err_t result=httpd_start(&server,&config);
  if(result!=ESP_OK){printf("[MCX-HTTP] start failed err=%d\n",(int)result);return;}
  httpd_uri_t page={};page.uri="/";page.method=HTTP_GET;page.handler=home;
  httpd_uri_t status={};status.uri="/health";status.method=HTTP_GET;status.handler=health;
  result=httpd_register_uri_handler(server,&page);
  if(result==ESP_OK)result=httpd_register_uri_handler(server,&status);
  httpd_uri_t screenshot={};screenshot.uri="/screen.raw";screenshot.method=HTTP_GET;
  screenshot.handler=meshcapture::screenshot;
  if(result==ESP_OK)result=httpd_register_uri_handler(server,&screenshot);
  httpd_uri_t nav={};nav.uri="/navigation";nav.method=HTTP_GET;nav.handler=mcxnavhttp::state;
  if(result==ESP_OK)result=httpd_register_uri_handler(server,&nav);
  httpd_uri_t command={};command.uri="/navigation/command";command.method=HTTP_POST;command.handler=mcxnavhttp::command;
  if(result==ESP_OK)result=httpd_register_uri_handler(server,&command);
  httpd_uri_t download={};download.uri="/navigation/download";download.method=HTTP_GET;download.handler=mcxnavhttp::download;
  if(result==ESP_OK)result=httpd_register_uri_handler(server,&download);
  if(result!=ESP_OK){httpd_stop(server);printf("[MCX-HTTP] route setup failed err=%d\n",(int)result);return;}
  running.store(true);
  printf("[MCX-HTTP v0.8.8] ready port=80 routes=/,/health,/screen.raw manual=ON touch=OFF\n");
}
} // namespace meshhealth

