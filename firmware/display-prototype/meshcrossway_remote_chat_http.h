#pragma once
#include "meshcrossway_remote_chat.h"
#include "esp_http_server.h"
namespace mcxremotechat {
static esp_err_t error(httpd_req_t *r,const char *code,const char *message){httpd_resp_set_status(r,code);httpd_resp_set_type(r,"text/plain");return httpd_resp_send(r,message,HTTPD_RESP_USE_STRLEN);}
static bool valid_target(const char *s){if(!*s)return true;if(strlen(s)!=64)return false;for(;*s;++s)if(!strchr("0123456789abcdef",*s))return false;return true;}
static esp_err_t exchange(httpd_req_t *r,const Request &request){
  if(!work||phase.load()!=0)return error(r,"503 Service Unavailable","Messaging busy; try again.");
  work->request=request;phase.store(1);
  unsigned waited=0;
  while(phase.load()!=3){
    vTaskDelay(pdMS_TO_TICKS(10));
    if(++waited==150){unsigned expected=1;
      // Cancel only a request the owner has not consumed. Never report a
      // timeout then send it later, and never free a buffer being processed.
      if(phase.compare_exchange_strong(expected,0))return error(r,"503 Service Unavailable","Device busy; message was not queued.");
    }
  }
  httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");
  httpd_resp_set_status(r,work->code==202?"202 Accepted":work->code==409?"409 Conflict":work->code==503?"503 Service Unavailable":"200 OK");
  const esp_err_t rc=httpd_resp_send(r,work->response,work->length);phase.store(0);return rc;
}
static esp_err_t state(httpd_req_t *r){
  char query[160]={},page[5]={};Request request;
  if(httpd_req_get_url_query_str(r,query,sizeof(query))!=ESP_OK||httpd_query_key_value(query,"network",request.network,sizeof(request.network))!=ESP_OK||
     (strcmp(request.network,"mc")&&strcmp(request.network,"mt")))return error(r,"400 Bad Request","Select mc or mt network.");
  // Optional fields must be bounded, not silently ignored when too long.
  if(strstr(query,"target=")){if(httpd_query_key_value(query,"target",request.target,sizeof(request.target))!=ESP_OK||!valid_target(request.target))return error(r,"400 Bad Request","Invalid conversation.");}
  if(strstr(query,"page=")){if(httpd_query_key_value(query,"page",page,sizeof(page))!=ESP_OK)return error(r,"400 Bad Request","Invalid page.");for(const char *p=page;*p;++p)if(*p<'0'||*p>'9')return error(r,"400 Bad Request","Invalid page.");request.page=unsigned(atoi(page));if(request.page>33)return error(r,"400 Bad Request","Invalid page.");}
  return exchange(r,request);
}
static esp_err_t send(httpd_req_t *r){
  char header[8]={};
  if(httpd_req_get_hdr_value_str(r,"X-MCX-Request",header,sizeof(header))!=ESP_OK||strcmp(header,"1"))return error(r,"403 Forbidden","Local application header required.");
  if(r->content_len<69||r->content_len>305)return error(r,"400 Bad Request","Invalid message length.");
  char body[306]={};int used=0;
  while(used<r->content_len){int n=httpd_req_recv(r,body+used,r->content_len-used);if(n<=0)return error(r,"408 Request Timeout","Incomplete message.");used+=n;}
  if(memchr(body,0,used))return error(r,"400 Bad Request","Invalid message text.");
  // network|opaque target|UTF-8 text; pipes within the text remain unchanged.
  if(body[2]!='|'||body[67]!='|')return error(r,"400 Bad Request","Invalid message fields.");
  body[2]=body[67]=0;Request request;request.sending=true;
  if((strcmp(body,"mc")&&strcmp(body,"mt"))||!valid_target(body+3)||!body[68]||!meshmt::utf8(body+68,used-68))return error(r,"400 Bad Request","Invalid network, conversation or UTF-8 text.");
  for(int i=68;i<used;++i)if((unsigned char)body[i]<32&&body[i]!='\n'&&body[i]!='\t')return error(r,"400 Bad Request","Invalid text characters.");
  memcpy(request.network,body,3);memcpy(request.target,body+3,65);memcpy(request.text,body+68,used-68+1);
  return exchange(r,request);
}
} // namespace mcxremotechat
