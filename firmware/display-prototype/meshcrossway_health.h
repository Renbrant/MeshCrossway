#pragma once
// Stage B: bounded HTTP status only. No LVGL calls, framebuffers or remote input.
#include "meshcrossway_network.h"
#include "meshcrossway_capture.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_timer.h"
#include <atomic>
#include <stdio.h>

namespace meshhealth {
static std::atomic<bool> running{false};
static bool is_running(){return running.load();}
static const char PAGE[]=R"MCX(<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>MeshCrossway Remote — View only</title>
<style>
body{background:#090c10;color:#f2f4f7;font:16px system-ui;max-width:720px;margin:24px auto;padding:0 20px}
h1{color:#ff8a24}section{background:#181d24;padding:20px;border-radius:12px;margin:14px 0}
dl{display:grid;grid-template-columns:1fr 1fr;gap:12px}dd{margin:0}small{color:#9aa6b4}
.controls{display:flex;align-items:center;gap:10px;flex-wrap:wrap}
input{font:inherit;padding:12px;max-width:190px;background:#090c10;border:1px solid #9aa6b4;border-radius:6px;color:white}
button{font:inherit;padding:12px 16px;background:#ff8a24;color:#090c10;border:0;border-radius:6px;cursor:pointer}
button:disabled{opacity:.5;cursor:wait}canvas{display:block;width:100%;max-width:480px;image-rendering:pixelated;border:1px solid #9aa6b4;margin-top:14px}
</style><h1>MeshCrossway Remote</h1><p>Experimental manual screen view · v0.5.9</p>
<section><p id="connection" role="status">Connecting to device…</p><dl>
<dt>Firmware</dt><dd id="version">—</dd><dt>Uptime</dt><dd id="uptime">—</dd>
<dt>Free heap</dt><dd id="heap">—</dd><dt>Local IP</dt><dd id="ip">—</dd></dl></section>
<section><h2>Read-only screen capture</h2>
<p>Enter the eight-digit View PIN shown in device Settings. Captures are manual and limited to one every five seconds.</p>
<div class="controls"><input type="password" id="pin" inputmode="numeric" maxlength="8" placeholder="View PIN" autocomplete="off" aria-label="View PIN"><button type="button" id="capture">Capture screen</button></div>
<p role="status" id="capture-status">No capture requested.</p><canvas id="screen" width="480" height="320" hidden></canvas></section>
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
 document.getElementById('ip').textContent=d.ip||'Not connected';
 document.getElementById('capture').disabled=!d.screen;
 }catch(e){document.getElementById('connection').textContent='Device unreachable · retrying';}
 finally{clearTimeout(timeout);setTimeout(update,3000);}
}
const button=document.getElementById('capture'),message=document.getElementById('capture-status');
button.addEventListener('click',async()=>{
 const pin=document.getElementById('pin').value;
 if(!/^\d{8}$/.test(pin)){message.textContent='Enter the eight-digit PIN shown on the device.';return;}
 button.disabled=true;message.textContent='Capturing once…';
 try{
  const response=await fetch('/screen.raw',{headers:{'X-MCX-Pin':pin},cache:'no-store'});
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
 finally{button.disabled=false;}
});
update();
</script></html>)MCX";
static void headers(httpd_req_t *req,const char *type){
  httpd_resp_set_type(req,type);
  httpd_resp_set_hdr(req,"Cache-Control","no-store");
  httpd_resp_set_hdr(req,"X-Content-Type-Options","nosniff");
  httpd_resp_set_hdr(req,"Content-Security-Policy","default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'");
}
static esp_err_t home(httpd_req_t *req){
  headers(req,"text/html; charset=utf-8");
  return httpd_resp_send(req,PAGE,HTTPD_RESP_USE_STRLEN);
}
static esp_err_t health(httpd_req_t *req){
  char ip[16],state[30],body[256];
  meshnetwork::get_status(ip,sizeof(ip),state,sizeof(state));
  const int n=snprintf(body,sizeof(body),
    "{\"version\":\"v0.5.9\",\"uptime_ms\":%llu,\"free_heap\":%lu,\"ip\":\"%s\",\"screen\":%s,\"touch\":false}",
    (unsigned long long)(esp_timer_get_time()/1000),(unsigned long)esp_get_free_heap_size(),ip,
    meshcapture::available()?"true":"false");
  if(n<0 || n>=(int)sizeof(body))return ESP_FAIL;
  headers(req,"application/json");
  return httpd_resp_send(req,body,n);
}
static void start(){
  if(running.load())return;
  httpd_config_t config=HTTPD_DEFAULT_CONFIG();
  config.server_port=80;config.stack_size=4096;
  config.max_open_sockets=2;config.max_uri_handlers=3;
  config.recv_wait_timeout=2;config.send_wait_timeout=2;
  config.lru_purge_enable=true;
  httpd_handle_t server=nullptr;
  esp_err_t result=httpd_start(&server,&config);
  if(result!=ESP_OK){printf("[MCX-HTTP] start failed err=%d\n",(int)result);return;}
  httpd_uri_t page={};page.uri="/";page.method=HTTP_GET;page.handler=home;
  httpd_uri_t status={};status.uri="/health";status.method=HTTP_GET;status.handler=health;
  result=httpd_register_uri_handler(server,&page);
  if(result==ESP_OK)result=httpd_register_uri_handler(server,&status);
  httpd_uri_t capture={};capture.uri="/screen.raw";capture.method=HTTP_GET;capture.handler=meshcapture::screenshot;
  if(result==ESP_OK)result=httpd_register_uri_handler(server,&capture);
  if(result!=ESP_OK){httpd_stop(server);printf("[MCX-HTTP] route setup failed err=%d\n",(int)result);return;}
  running.store(true);
  printf("[MCX-HTTP v0.5.9] ready port=80 routes=/,/health,/screen.raw manual=ON touch=OFF\n");
}
} // namespace meshhealth
