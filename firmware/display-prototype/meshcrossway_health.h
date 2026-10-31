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
#include "meshcrossway_remote_chat_http.h"
#include "meshcrossway_remote_radio.h"
#include <atomic>
#include <stdio.h>

namespace meshhealth {
static constexpr unsigned HTTP_STACK_BYTES=8192;
static std::atomic<bool> running{false};
static std::atomic<uint32_t> http_stack_min{UINT32_MAX};
template<esp_err_t (*Handler)(httpd_req_t*)> static esp_err_t observe(httpd_req_t *req){
  const esp_err_t rc=Handler(req);
  const uint32_t bytes=uxTaskGetStackHighWaterMark(nullptr),before=http_stack_min.load();
  if(bytes<before){http_stack_min.store(bytes);if(before==UINT32_MAX||bytes<2048)printf("[MCX-HTTP] stack_free_min=%lu bytes / %u\n",(unsigned long)bytes,HTTP_STACK_BYTES);}
  return rc;
}
static bool is_running(){return running.load();}
#include "meshcrossway_companion_page.h"
#include "meshcrossway_track_map_js.h"
static void headers(httpd_req_t *req,const char *type){
  httpd_resp_set_type(req,type);
  httpd_resp_set_hdr(req,"Cache-Control","no-store");
  httpd_resp_set_hdr(req,"X-Content-Type-Options","nosniff");
  httpd_resp_set_hdr(req,"Content-Security-Policy","default-src 'none'; script-src 'self' 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'; form-action 'self'; img-src 'self' data: https://tile.openstreetmap.org; base-uri 'none'");
}
static esp_err_t home(httpd_req_t *req){
  headers(req,"text/html; charset=utf-8");
  return httpd_resp_send(req,PAGE,HTTPD_RESP_USE_STRLEN);
}
static esp_err_t track_map_script(httpd_req_t *req){headers(req,"application/javascript; charset=utf-8");return httpd_resp_send(req,TRACK_MAP_JS,HTTPD_RESP_USE_STRLEN);}
static esp_err_t health(httpd_req_t *req){
  char ip[16],state[30],body[1280],history[96];
  mcxhistory::describe(history,sizeof(history));
  meshnetwork::get_status(ip,sizeof(ip),state,sizeof(state));
  const int n=snprintf(body,sizeof(body),
    "{\"version\":\"v0.8.14\",\"uptime_ms\":%llu,\"free_heap\":%lu,\"ip\":\"%s\",\"sd_backup\":\"%s\",\"sd_step\":\"%s\",\"sd_errno\":%d,\"screen\":%s,\"touch\":false,\"captures\":%lu,\"capture_failures\":%lu,\"capture_ms\":%lu,\"history\":\"%s\",\"history_step\":\"%s\",\"history_errno\":%d,\"history_failures\":%lu,\"history_ms\":%lu,\"history_max_ms\":%lu,\"internal_free\":%lu,\"internal_largest\":%lu,\"internal_min\":%lu,\"reset_reason\":%d,\"boot_count\":%lu,\"previous_activity\":\"%s\",\"ui_max_ms\":%lu,\"ui_stack_min\":%lu,\"navigation_stack_min\":%lu,\"allocation_failures\":%lu,\"allocation_bytes\":%lu,\"allocation_caps\":%lu,\"mailbox_peak\":%lu,\"mailbox_overflows\":%lu}",
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
    (unsigned long)(mcxnav::stack_min.load()==UINT32_MAX?0:mcxnav::stack_min.load()),
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
  config.server_port=80;config.stack_size=HTTP_STACK_BYTES;
  config.max_open_sockets=2;config.max_uri_handlers=12;
  config.recv_wait_timeout=2;config.send_wait_timeout=2;
  config.lru_purge_enable=true;
  if(!mcxremotechat::work)printf("[MCX-HTTP] messaging PSRAM unavailable\n");
  httpd_handle_t server=nullptr;
  esp_err_t result=httpd_start(&server,&config);
  if(result!=ESP_OK){printf("[MCX-HTTP] start failed err=%d\n",(int)result);return;}
  httpd_uri_t page={};page.uri="/";page.method=HTTP_GET;page.handler=observe<home>;
  httpd_uri_t status={};status.uri="/health";status.method=HTTP_GET;status.handler=observe<health>;
  result=httpd_register_uri_handler(server,&page);
  if(result==ESP_OK)result=httpd_register_uri_handler(server,&status);
  httpd_uri_t screenshot={};screenshot.uri="/screen.raw";screenshot.method=HTTP_GET;
  screenshot.handler=observe<meshcapture::screenshot>;
  if(result==ESP_OK)result=httpd_register_uri_handler(server,&screenshot);
  httpd_uri_t nav={};nav.uri="/navigation";nav.method=HTTP_GET;nav.handler=observe<mcxnavhttp::state>;
  if(result==ESP_OK)result=httpd_register_uri_handler(server,&nav);
  httpd_uri_t command={};command.uri="/navigation/command";command.method=HTTP_POST;command.handler=observe<mcxnavhttp::command>;
  if(result==ESP_OK)result=httpd_register_uri_handler(server,&command);
  httpd_uri_t download={};download.uri="/navigation/download";download.method=HTTP_GET;download.handler=observe<mcxnavhttp::download>;
  if(result==ESP_OK)result=httpd_register_uri_handler(server,&download);
  httpd_uri_t chat={};chat.uri="/messages";chat.method=HTTP_GET;chat.handler=observe<mcxremotechat::state>;
  if(result==ESP_OK)result=httpd_register_uri_handler(server,&chat);
  httpd_uri_t send={};send.uri="/messages/send";send.method=HTTP_POST;send.handler=observe<mcxremotechat::send>;
  if(result==ESP_OK)result=httpd_register_uri_handler(server,&send);
  httpd_uri_t geometry={};geometry.uri="/navigation/geometry";geometry.method=HTTP_GET;geometry.handler=observe<mcxnavhttp::geometry>;
  if(result==ESP_OK)result=httpd_register_uri_handler(server,&geometry);
  httpd_uri_t script={};script.uri="/track-map.js";script.method=HTTP_GET;script.handler=observe<track_map_script>;
  if(result==ESP_OK)result=httpd_register_uri_handler(server,&script);
  httpd_uri_t radios={};radios.uri="/radios";radios.method=HTTP_GET;radios.handler=observe<mcxremoteradio::state>;
  if(result==ESP_OK)result=httpd_register_uri_handler(server,&radios);
  httpd_uri_t radio_command={};radio_command.uri="/radios/command";radio_command.method=HTTP_POST;radio_command.handler=observe<mcxremoteradio::send>;
  if(result==ESP_OK)result=httpd_register_uri_handler(server,&radio_command);
  if(result!=ESP_OK){httpd_stop(server);printf("[MCX-HTTP] route setup failed err=%d\n",(int)result);return;}
  running.store(true);
  printf("[MCX-HTTP v0.8.14] ready port=80 routes=/,/health,/screen.raw manual=ON touch=OFF\n");
}
} // namespace meshhealth

