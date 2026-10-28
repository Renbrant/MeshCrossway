#pragma once
// MeshCrossway v0.5 Wi-Fi HTTP screenshot + remote pointer, paired per boot.
#include "meshcrossway_network.h"
#include "meshcrossway_remote_page.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_lvgl_port.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#if LVGL_VERSION_MAJOR != 8 || !LV_USE_SNAPSHOT || LV_COLOR_DEPTH != 16
#error "Remote capture requires LVGL 8 with snapshot and RGB565"
#endif

namespace meshremote {
static constexpr int WIDTH=480, HEIGHT=320;
static constexpr size_t FRAME_BYTES=(size_t)WIDTH*HEIGHT*2;
static portMUX_TYPE pointer_mux=portMUX_INITIALIZER_UNLOCKED;
static lv_point_t pointer={240,160};
static bool pressed=false;
static int64_t pointer_updated=0;
static char pairing_pin[9]="";
static int64_t auth_cooldown=0;
static const char *pin(){return pairing_pin;}

static void read_remote(lv_indev_drv_t *,lv_indev_data_t *data){
  const int64_t now=esp_timer_get_time();
  portENTER_CRITICAL(&pointer_mux);
  if(pressed && now-pointer_updated>1500000)pressed=false;
  data->point=pointer;
  data->state=pressed?LV_INDEV_STATE_PR:LV_INDEV_STATE_REL;
  portEXIT_CRITICAL(&pointer_mux);
}
// Register as a SECOND LVGL pointer so the physical touch driver remains intact.
// Called only while the Waveshare LVGL port is locked.
static void register_input(){
  lv_indev_drv_t drv;lv_indev_drv_init(&drv);
  drv.type=LV_INDEV_TYPE_POINTER;
  drv.read_cb=read_remote;
  drv.disp=lv_disp_get_default();
  lv_indev_drv_register(&drv);
}
static esp_err_t unavailable(httpd_req_t *req,const char *reason){
  httpd_resp_set_status(req,"503 Service Unavailable");
  httpd_resp_set_type(req,"text/plain");
  return httpd_resp_send(req,reason,HTTPD_RESP_USE_STRLEN);
}
static esp_err_t deny(httpd_req_t *req){
  httpd_resp_set_status(req,"403 Forbidden");
  return httpd_resp_send(req,"Pairing required",HTTPD_RESP_USE_STRLEN);
}
static bool authorized(httpd_req_t *req){
  char input[24]={};
  if(httpd_req_get_hdr_value_str(req,"X-MCX-Pin",input,sizeof(input))!=ESP_OK)return false;
  const int64_t now=esp_timer_get_time();
  if(now<auth_cooldown)return false;
  if(strlen(input)!=8 || strcmp(input,pairing_pin)!=0){
    auth_cooldown=now+800000;
    return false;
  }
  return true;
}
static esp_err_t home(httpd_req_t *req){
  httpd_resp_set_type(req,"text/html; charset=utf-8");
  httpd_resp_set_hdr(req,"Cache-Control","no-store");
  httpd_resp_set_hdr(req,"X-Content-Type-Options","nosniff");
  return httpd_resp_send(req,MESHCROSSWAY_REMOTE_PAGE,HTTPD_RESP_USE_STRLEN);
}
static esp_err_t health(httpd_req_t *req){
  if(!authorized(req))return deny(req);
  httpd_resp_set_type(req,"application/json");
  return httpd_resp_send(req,"{\"paired\":true}",HTTPD_RESP_USE_STRLEN);
}
static esp_err_t screenshot(httpd_req_t *req){
  if(!authorized(req))return deny(req);
  if(!lvgl_port_lock(1000))return unavailable(req,"LVGL busy");
  lv_img_dsc_t *image=lv_snapshot_take(lv_scr_act(),LV_IMG_CF_TRUE_COLOR);
  if(!image){lvgl_port_unlock();return unavailable(req,"Snapshot failed");}
  uint8_t *buf=nullptr;
  if(image->header.w==WIDTH && image->header.h==HEIGHT && image->data_size==FRAME_BYTES){
    buf=(uint8_t*)heap_caps_malloc(FRAME_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(buf)memcpy(buf,image->data,FRAME_BYTES);
  }
  lv_snapshot_free(image);lvgl_port_unlock();
  if(!buf)return unavailable(req,"Capture unavailable");
#if LV_COLOR_16_SWAP
  for(size_t i=0;i<FRAME_BYTES;i+=2){
    uint8_t b=buf[i];buf[i]=buf[i+1];buf[i+1]=b;
  }
#endif
  httpd_resp_set_type(req,"application/octet-stream");
  httpd_resp_set_hdr(req,"Cache-Control","no-store");
  const esp_err_t result=httpd_resp_send(req,(const char*)buf,FRAME_BYTES);
  heap_caps_free(buf);return result;
}
static esp_err_t touch(httpd_req_t *req){
  if(!authorized(req))return deny(req);
  if(req->content_len<5 || req->content_len>24)
    return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid pointer payload");
  char payload[25]={};
  int received=0;
  while(received<req->content_len){
    const int n=httpd_req_recv(req,payload+received,req->content_len-received);
    if(n<=0)return ESP_FAIL;
    received+=n;
  }
  char action=0,trailing=0;int x=-1,y=-1;
  if(sscanf(payload,"%c,%d,%d%c",&action,&x,&y,&trailing)!=3 ||
     x<0 || x>=WIDTH || y<0 || y>=HEIGHT ||
     (action!='d' && action!='m' && action!='u'))
    return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid pointer coordinates");
  const int64_t now=esp_timer_get_time();
  portENTER_CRITICAL(&pointer_mux);
  pointer.x=x;pointer.y=y;
  pressed=action!='u';
  pointer_updated=now;
  portEXIT_CRITICAL(&pointer_mux);
  httpd_resp_set_status(req,"204 No Content");
  return httpd_resp_send(req,nullptr,0);
}
static void start(){
  // Ephemeral PIN is displayed on the physical device, never returned by HTTP.
  snprintf(pairing_pin,sizeof(pairing_pin),"%08lu",
           (unsigned long)(10000000u+esp_random()%90000000u));
  meshnetwork::start();
  httpd_config_t conf=HTTPD_DEFAULT_CONFIG();
  conf.server_port=80;conf.stack_size=8192;
  httpd_handle_t server=nullptr;
  if(httpd_start(&server,&conf)!=ESP_OK)return;
  httpd_uri_t page={};page.uri="/";page.method=HTTP_GET;page.handler=home;
  ESP_ERROR_CHECK(httpd_register_uri_handler(server,&page));
  httpd_uri_t check={};check.uri="/health";check.method=HTTP_GET;check.handler=health;
  ESP_ERROR_CHECK(httpd_register_uri_handler(server,&check));
  httpd_uri_t frame={};frame.uri="/screen.raw";frame.method=HTTP_GET;frame.handler=screenshot;
  ESP_ERROR_CHECK(httpd_register_uri_handler(server,&frame));
  httpd_uri_t click={};click.uri="/touch";click.method=HTTP_POST;click.handler=touch;
  ESP_ERROR_CHECK(httpd_register_uri_handler(server,&click));
}
} // namespace meshremote
