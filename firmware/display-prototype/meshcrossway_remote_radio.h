#pragma once
#include "meshcrossway_ble.h"
#include "meshcrossway_remote_chat_http.h"
// Radio commands share the LVGL owner, but not the chat mailbox or its model.
namespace mcxremoteradio {
struct Request {uint32_t action=0,value=0;char peer[65]={};};
static Request command;static std::atomic<unsigned> phase{0};static int result_code=202;static char message[96]={};
static void peer_token(const meshble::Peer &p,char out[65]){mcxremotechat::token("bt",&p.addr,sizeof(p.addr),out);}
static esp_err_t state(httpd_req_t *r){
  if(!mcxremotechat::work)return mcxremotechat::error(r,"503 Service Unavailable","Radio controls unavailable");
  const auto s=meshble::snapshot();std::string body="{\"phase\":"+std::to_string(s.phase)+",\"network\":"+mcxnavhttp::quote(s.radio==meshble::MESHTASTIC?"mt":s.radio==meshble::MESHCORE?"mc":"unknown")+",\"name\":"+mcxnavhttp::quote(s.active_name)+",\"status\":"+mcxnavhttp::quote(s.status)+",\"pin_required\":"+(s.pin_required?"true":"false")+",\"confirm_required\":"+(s.confirm_required?"true":"false")+",\"compare_number\":"+std::to_string(s.confirm_required?s.compare_number:0)+",\"peers\":[";
  for(unsigned i=0;i<s.count;++i){if(i)body+=',';char token[65];peer_token(s.peers[i],token);body+="{\"id\":"+mcxnavhttp::quote(token)+",\"name\":"+mcxnavhttp::quote(s.peers[i].name)+",\"rssi\":"+std::to_string(s.peers[i].rssi)+",\"network\":"+mcxnavhttp::quote(s.peers[i].advertised==meshble::MESHTASTIC?"mt":s.peers[i].advertised==meshble::MESHCORE?"mc":"unknown")+"}";}
  body+="]}";httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");return httpd_resp_send(r,body.c_str(),body.size());
}
static void poll(){unsigned ready=1;if(!phase.compare_exchange_strong(ready,2))return;
  const auto s=meshble::snapshot();bool ok=false;
  switch(command.action){
   case 1:ok=meshble::request_start();break;
   case 2:if(s.phase==meshble::IDLE||s.phase==meshble::FAILED){meshble::scan(false);ok=meshble::snapshot().phase==meshble::SCANNING;}break;
   case 3:if(s.phase==meshble::IDLE||s.phase==meshble::FAILED||s.phase==meshble::SCANNING)for(unsigned i=0;i<s.count;++i){char token[65];peer_token(s.peers[i],token);if(!strcmp(token,command.peer)){meshble::connect_peer(s.peers[i]);ok=meshble::snapshot().phase==meshble::CONNECTING;break;}}break;
   case 4:meshble::disconnect();ok=true;break;
   case 5:ok=s.pin_required&&meshble::submit_pin(command.value);break;
   case 6:ok=s.confirm_required&&meshble::confirm_number(command.value==1);break;
  }
  command.value=0;result_code=ok?202:409;snprintf(message,sizeof(message),"%s",ok?"Radio action accepted; follow the live status.":"Radio state changed or action unavailable. Refresh and try again.");phase.store(3);
}
static esp_err_t send(httpd_req_t *r){if(!mcxremotechat::work)return mcxremotechat::error(r,"503 Service Unavailable","Radio controls unavailable");char header[8]={};if(httpd_req_get_hdr_value_str(r,"X-MCX-Request",header,sizeof(header))!=ESP_OK||strcmp(header,"1"))return mcxremotechat::error(r,"403 Forbidden","Local application header required");
  if(r->content_len<4||r->content_len>80)return mcxremotechat::error(r,"400 Bad Request","Invalid radio command");
  char body[81]={};unsigned used=0;const unsigned length=unsigned(r->content_len);while(used<length){int n=httpd_req_recv(r,body+used,r->content_len-used);if(n<=0)return mcxremotechat::error(r,"408 Request Timeout","Incomplete radio command");used+=unsigned(n);}if(memchr(body,0,used))return mcxremotechat::error(r,"400 Bad Request","Invalid radio command");
  char *a=strchr(body,'|'),*b=a?strchr(a+1,'|'):nullptr;if(!a||!b||strchr(b+1,'|'))return mcxremotechat::error(r,"400 Bad Request","Invalid radio fields");*a++=0;*b++=0;Request next;
  if(!mcxnavhttp::number(body,next.action)||next.action<1||next.action>6||!mcxnavhttp::number(b,next.value)||next.value>999999||!mcxremotechat::valid_target(a)||(next.action==3&&strlen(a)!=64)||(next.action==6&&next.value>1))return mcxremotechat::error(r,"400 Bad Request","Invalid radio fields");
  memcpy(next.peer,a,strlen(a)+1);
  if(phase.load()!=0){return mcxremotechat::error(r,"503 Service Unavailable","Radio controls busy");}
  command=next;phase.store(1);unsigned waited=0;
  while(phase.load()!=3){vTaskDelay(pdMS_TO_TICKS(10));if(++waited==150){unsigned expected=1;if(phase.compare_exchange_strong(expected,0)){command.value=0;return mcxremotechat::error(r,"503 Service Unavailable","Device busy; radio action not applied");}}}
  httpd_resp_set_type(r,"application/json");httpd_resp_set_status(r,result_code==202?"202 Accepted":"409 Conflict");std::string response="{\"message\":"+mcxnavhttp::quote(message)+"}";const auto rc=httpd_resp_send(r,response.c_str(),response.size());phase.store(0);return rc;
}
}
