#pragma once
// MeshCrossway v0.4 local development screen capture.
// Temporary Wi-Fi SoftAP. Never expose this HTTP server to the public internet.
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_heap_caps.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "lv_snapshot.h"
#include <stdio.h>
#include <string.h>

namespace meshremote {
static const char PAGE[] = R"HTML(<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>MeshCrossway Remote Display</title><style>body{background:#090c10;color:#eee;font:16px system-ui;text-align:center;margin:22px}h1{color:#ff8a24}canvas{display:block;width:min(960px,100%);aspect-ratio:3/2;image-rendering:pixelated;margin:20px auto;border:2px solid #ff8a24;border-radius:8px;background:#222}button{background:#ff8a24;border:0;padding:12px 20px;border-radius:9px;font-weight:bold;cursor:pointer}small{color:#aaa}</style></head><body><h1>MeshCrossway / Live Screen</h1><p>Actual 480 x 320 LVGL framebuffer snapshot. No remote touch control in this build.</p><canvas id="c" width="480" height="320"></canvas><button onclick="save()">Save PNG screenshot</button><p id="status"></p><small>Development access point only — do not use on untrusted networks.</small><script>
const canvas=document.querySelector('canvas'),ctx=canvas.getContext('2d');
async function refresh(){
try{
 const r=await fetch('/screen.raw?x='+Date.now(),{cache:'no-store'});
 if(!r.ok)throw Error('Capture unavailable (HTTP '+r.status+')');
 const a=new Uint8Array(await r.arrayBuffer());
 if(a.length!==480*320*2)throw Error('Wrong frame size: '+a.length);
 const pixels=ctx.createImageData(480,320);
 for(let i=0;i<480*320;i++){
   const n=a[i*2]|(a[i*2+1]<<8),j=i*4;
   pixels.data[j]=((n>>11)&31)*255/31;
   pixels.data[j+1]=((n>>5)&63)*255/63;
   pixels.data[j+2]=(n&31)*255/31;
   pixels.data[j+3]=255;
 }
 ctx.putImageData(pixels,0,0);
 document.getElementById('status').textContent='Connected / Live capture';
}catch(e){document.getElementById('status').textContent=e.message;}
}
function save(){const a=document.createElement('a');a.download='meshcrossway-screen.png';a.href=canvas.toDataURL('image/png');a.click();}
refresh();setInterval(refresh,2000);
</script></body></html>)HTML";

static esp_err_t home(httpd_req_t *req) {
 httpd_resp_set_type(req,"text/html");
 return httpd_resp_send(req,PAGE,HTTPD_RESP_USE_STRLEN);
}
static esp_err_t screen(httpd_req_t *req) {
 // LVGL is owned by its dedicated FreeRTOS task. Lock its port before snapshotting.
 if(!lvgl_port_lock(1000))return httpd_resp_send_err(req,HTTPD_503_SERVICE_UNAVAILABLE,"LVGL busy");
 lv_img_dsc_t *img=lv_snapshot_take(lv_scr_act(),LV_IMG_CF_TRUE_COLOR);
 if(!img){lvgl_port_unlock();return httpd_resp_send_err(req,HTTPD_503_SERVICE_UNAVAILABLE,"Snapshot unavailable");}
 // Copy under lock so the snapshot can be freed before sending over Wi-Fi.
 const size_t len=(size_t)480*320*2;
 uint8_t *buf=(uint8_t*)heap_caps_malloc(len,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(buf && img->data_size>=len)memcpy(buf,img->data,len);
 lv_snapshot_free(img);
 lvgl_port_unlock();
 if(!buf)return httpd_resp_send_err(req,HTTPD_503_SERVICE_UNAVAILABLE,"Capture memory unavailable");
 httpd_resp_set_type(req,"application/octet-stream");
 httpd_resp_set_hdr(req,"Cache-Control","no-store");
 esp_err_t result=httpd_resp_send(req,(const char*)buf,len);
 heap_caps_free(buf);
 return result;
}
static void start() {
 esp_netif_init();
 esp_event_loop_create_default();
 esp_netif_create_default_wifi_ap();
 wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();
 ESP_ERROR_CHECK(esp_wifi_init(&cfg));
 wifi_config_t ap={};
 strcpy((char*)ap.ap.ssid,"MeshCrossway-Dev");
 strcpy((char*)ap.ap.password,"meshcrossway-dev");
 ap.ap.ssid_len=strlen((char*)ap.ap.ssid);
 ap.ap.channel=6;
 ap.ap.max_connection=2;
 ap.ap.authmode=WIFI_AUTH_WPA2_PSK;
 ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
 ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&ap));
 ESP_ERROR_CHECK(esp_wifi_start());
 httpd_config_t web=HTTPD_DEFAULT_CONFIG();
 web.server_port=80;
 web.stack_size=8192;
 httpd_handle_t server=nullptr;
 if(httpd_start(&server,&web)==ESP_OK){
   httpd_uri_t root={};root.uri="/";root.method=HTTP_GET;root.handler=home;
   httpd_register_uri_handler(server,&root);
   httpd_uri_t shot={};shot.uri="/screen.raw";shot.method=HTTP_GET;shot.handler=screen;
   httpd_register_uri_handler(server,&shot);
 }
}
} // namespace meshremote
