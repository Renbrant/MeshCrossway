#pragma once
// Stage B: bounded HTTP status only. No LVGL calls, framebuffers or remote input.
#include "meshcrossway_network.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_timer.h"
#include <atomic>
#include <stdio.h>

namespace meshhealth {
static std::atomic<bool> running{false};
static bool is_running(){return running.load();}
static const char PAGE[]=R"MCX(<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>MeshCrossway Remote — Status</title><style>body{background:#090c10;color:#f2f4f7;font:16px system-ui;max-width:640px;margin:40px auto;padding:0 20px}h1{color:#ff8a24}section{background:#181d24;padding:20px;border-radius:12px}dl{display:grid;grid-template-columns:1fr 1fr;gap:16px}dd{margin:0}small{color:#9aa6b4}</style><h1>MeshCrossway Remote</h1><p>HTTP status test · v0.5.8</p><section><p id="connection" role="status">Connecting to device…</p><dl><dt>Firmware</dt><dd id="version">—</dd><dt>Uptime</dt><dd id="uptime">—</dd><dt>Free heap</dt><dd id="heap">—</dd><dt>Local IP</dt><dd id="ip">—</dd></dl></section><p>Screen viewing and remote touch are disabled in this test.</p><small>Keep using the physical touchscreen while this page is open.</small><script>
async function update(){const controller=new AbortController();const timeout=setTimeout(()=>controller.abort(),5000);try{const r=await fetch('/health',{cache:'no-store',signal:controller.signal});if(!r.ok)throw Error();const d=await r.json();document.getElementById('connection').textContent='Device online · status only';document.getElementById('version').textContent=d.version;document.getElementById('uptime').textContent=Math.floor(d.uptime_ms/1000)+' s';document.getElementById('heap').textContent=Math.floor(d.free_heap/1024)+' KB';document.getElementById('ip').textContent=d.ip||'Not connected';}catch(e){document.getElementById('connection').textContent='Device unreachable · retrying';}finally{clearTimeout(timeout);setTimeout(update,3000);}}update();
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
    "{\"version\":\"v0.5.8\",\"uptime_ms\":%llu,\"free_heap\":%lu,\"ip\":\"%s\",\"screen\":false,\"touch\":false}",
    (unsigned long long)(esp_timer_get_time()/1000),(unsigned long)esp_get_free_heap_size(),ip);
  if(n<0 || n>=(int)sizeof(body))return ESP_FAIL;
  headers(req,"application/json");
  return httpd_resp_send(req,body,n);
}
static void start(){
  if(running.load())return;
  httpd_config_t config=HTTPD_DEFAULT_CONFIG();
  config.server_port=80;config.stack_size=4096;
  config.max_open_sockets=2;config.max_uri_handlers=2;
  config.recv_wait_timeout=2;config.send_wait_timeout=2;
  config.lru_purge_enable=true;
  httpd_handle_t server=nullptr;
  esp_err_t result=httpd_start(&server,&config);
  if(result!=ESP_OK){printf("[MCX-HTTP] start failed err=%d\n",(int)result);return;}
  httpd_uri_t page={};page.uri="/";page.method=HTTP_GET;page.handler=home;
  httpd_uri_t status={};status.uri="/health";status.method=HTTP_GET;status.handler=health;
  result=httpd_register_uri_handler(server,&page);
  if(result==ESP_OK)result=httpd_register_uri_handler(server,&status);
  if(result!=ESP_OK){httpd_stop(server);printf("[MCX-HTTP] route setup failed err=%d\n",(int)result);return;}
  running.store(true);
  printf("[MCX-HTTP v0.5.8] ready port=80 routes=/,/health screen=OFF touch=OFF\n");
}
} // namespace meshhealth
