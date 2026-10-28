#pragma once
// MeshCrossway v0.4 development-only screen viewer.
// HTTP is accessible only on the temporary Wi-Fi access point.
#include "esp_err.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_lvgl_port.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lvgl.h"  // LVGL 8.x includes snapshot via lv_extra.h / lv_others.h
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if LVGL_VERSION_MAJOR != 8
#error "MeshCrossway screen capture currently requires LVGL 8.x"
#endif
#if !LV_USE_SNAPSHOT
#error "Enable CONFIG_LV_USE_SNAPSHOT in sdkconfig.defaults"
#endif
#if LV_COLOR_DEPTH != 16
#error "The screen viewer expects RGB565 / LV_COLOR_DEPTH=16"
#endif

namespace meshremote {
static constexpr int WIDTH = 480;
static constexpr int HEIGHT = 320;
static constexpr size_t FRAME_BYTES = (size_t)WIDTH * HEIGHT * 2;

static const char PAGE[] = R"HTML(<!doctype html><html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>MeshCrossway - Live Screen</title><style>
*{box-sizing:border-box}body{margin:20px;background:#090c10;color:#f2f4f7;font:16px system-ui;text-align:center}
h1{color:#ff8a24}canvas{display:block;width:min(960px,100%);height:auto;image-rendering:pixelated;margin:20px auto;border:2px solid #ff8a24;border-radius:8px;background:#15191f}
button{background:#ff8a24;color:#161616;border:0;padding:12px 20px;border-radius:9px;font-weight:800;cursor:pointer}small{color:#9aa6b4}
</style></head><body><h1>MeshCrossway / Live Screen</h1>
<p>Actual 480 &times; 320 LVGL screen. Read-only development viewer.</p>
<canvas id="screen" width="480" height="320"></canvas>
<button onclick="saveScreenshot()">Save PNG screenshot</button><p id="status" role="status">Connecting...</p>
<small>MeshCrossway development access point. Internet access may be unavailable while connected.</small>
<script>
const canvas=document.getElementById('screen'), ctx=canvas.getContext('2d');
async function refreshScreen(){
  try {
    const response=await fetch('/screen.raw?t='+Date.now(),{cache:'no-store'});
    if(!response.ok) throw Error('Screenshot HTTP '+response.status);
    const raw=new Uint8Array(await response.arrayBuffer());
    if(raw.length!==480*320*2) throw Error('Unexpected frame size '+raw.length);
    const img=ctx.createImageData(480,320);
    for(let i=0;i<480*320;i++) {
      const color=raw[i*2] | (raw[i*2+1]<<8), index=i*4;
      img.data[index]=((color>>>11)&31)*255/31;
      img.data[index+1]=((color>>>5)&63)*255/63;
      img.data[index+2]=(color&31)*255/31;
      img.data[index+3]=255;
    }
    ctx.putImageData(img,0,0);
    document.getElementById('status').textContent='Connected / updated '+new Date().toLocaleTimeString();
  } catch (e) { document.getElementById('status').textContent=String(e.message||e); }
}
function saveScreenshot(){
  const link=document.createElement('a');
  link.download='meshcrossway-screen.png'; link.href=canvas.toDataURL('image/png'); link.click();
}
refreshScreen(); setInterval(refreshScreen,2000);
</script></body></html>)HTML";

static esp_err_t unavailable(httpd_req_t *req, const char *message) {
  httpd_resp_set_status(req, "503 Service Unavailable");
  httpd_resp_set_type(req, "text/plain");
  return httpd_resp_send(req, message, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t home(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, PAGE, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t screenshot(httpd_req_t *req) {
  if(!lvgl_port_lock(1000)) return unavailable(req,"LVGL is busy");

  lv_img_dsc_t *image=lv_snapshot_take(lv_scr_act(), LV_IMG_CF_TRUE_COLOR);
  if(!image) {
    lvgl_port_unlock();
    return unavailable(req,"Snapshot allocation failed");
  }

  uint8_t *copy=nullptr;
  if(image->header.w==WIDTH && image->header.h==HEIGHT && image->data_size==FRAME_BYTES) {
    copy=(uint8_t*)heap_caps_malloc(FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if(copy) memcpy(copy,image->data,FRAME_BYTES);
  }
  lv_snapshot_free(image);
  lvgl_port_unlock();
  if(!copy) return unavailable(req,"Capture dimensions or memory unavailable");

#if LV_COLOR_16_SWAP
  // Send canonical little-endian RGB565 to the browser, independent of LCD byte order.
  for(size_t i=0;i<FRAME_BYTES;i+=2) {
    uint8_t tmp=copy[i];copy[i]=copy[i+1];copy[i+1]=tmp;
  }
#endif

  httpd_resp_set_type(req,"application/octet-stream");
  httpd_resp_set_hdr(req,"Cache-Control","no-store");
  const esp_err_t result=httpd_resp_send(req,(const char*)copy,FRAME_BYTES);
  heap_caps_free(copy);
  return result;
}

static void start() {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_ap();
  wifi_init_config_t config=WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&config));

  wifi_config_t ap={};
  strncpy((char*)ap.ap.ssid,"MeshCrossway-Dev",sizeof(ap.ap.ssid)-1);
  strncpy((char*)ap.ap.password,"meshcrossway-dev",sizeof(ap.ap.password)-1);
  ap.ap.ssid_len=strlen((char*)ap.ap.ssid);
  ap.ap.channel=6;
  ap.ap.max_connection=2;
  ap.ap.authmode=WIFI_AUTH_WPA2_PSK;
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&ap));
  ESP_ERROR_CHECK(esp_wifi_start());

  httpd_config_t config_http=HTTPD_DEFAULT_CONFIG();
  config_http.server_port=80;
  config_http.stack_size=8192;
  httpd_handle_t server=nullptr;
  if(httpd_start(&server,&config_http)!=ESP_OK) return;

  httpd_uri_t root={};
  root.uri="/";root.method=HTTP_GET;root.handler=home;
  ESP_ERROR_CHECK(httpd_register_uri_handler(server,&root));
  httpd_uri_t frame={};
  frame.uri="/screen.raw";frame.method=HTTP_GET;frame.handler=screenshot;
  ESP_ERROR_CHECK(httpd_register_uri_handler(server,&frame));
}
}  // namespace meshremote
