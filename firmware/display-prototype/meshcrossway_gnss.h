#pragma once
#include "meshcrossway_gnss_parser.h"
#include "meshcrossway_gnss_settings.h"
#include "nvs.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
namespace mcxgnss {
static portMUX_TYPE mutex=portMUX_INITIALIZER_UNLOCKED;
static Fix latest;
static unsigned baud=9600;
static TaskHandle_t task=nullptr;
static Settings settings;
static uint32_t revision=0,changed_ms=0;
struct Diagnostics {unsigned baud=0;int error=0;uint32_t bytes=0;bool saved=true;};
static Diagnostics diagnostics;
static Settings configuration(){portENTER_CRITICAL(&mutex);auto out=settings;portEXIT_CRITICAL(&mutex);return out;}
static Diagnostics status(){portENTER_CRITICAL(&mutex);auto out=diagnostics;portEXIT_CRITICAL(&mutex);return out;}
static void configure(const Settings &value){if(!valid_settings(value))return;
  portENTER_CRITICAL(&mutex);settings=value;++revision;changed_ms=uint32_t(esp_timer_get_time()/1000);diagnostics.saved=false;portEXIT_CRITICAL(&mutex);
}
static Fix snapshot(){portENTER_CRITICAL(&mutex);Fix out=latest;const auto cfg=settings;portEXIT_CRITICAL(&mutex);
  if(!cfg.enabled||uint32_t(uint32_t(esp_timer_get_time()/1000)-out.position_ms)>cfg.timeout_ms)out.valid=false;
  const uint32_t now=uint32_t(esp_timer_get_time()/1000);
  if(!cfg.enabled||uint32_t(now-out.satellites_ms)>cfg.timeout_ms){out.have_satellites=out.have_hdop=out.have_altitude=false;}
  if(!cfg.enabled||uint32_t(now-out.speed_ms)>cfg.timeout_ms)out.have_speed=false;
  return out;
}
static void worker(void *){
  Parser parser;Settings applied;bool have_applied=false;int io_error=0;uint8_t bytes[128];uint32_t switched=0,seen=UINT32_MAX,persisted=0,last_save=0;
  for(;;){
    const uint32_t now=uint32_t(esp_timer_get_time()/1000);
    portENTER_CRITICAL(&mutex);const auto cfg=settings;const uint32_t rev=revision,edited=changed_ms;portEXIT_CRITICAL(&mutex);
    if(rev!=seen){
      if(!have_applied||cfg.baud!=applied.baud||cfg.enabled!=applied.enabled){
        baud=cfg.baud?cfg.baud:9600;io_error=uart_set_baudrate(UART_NUM_1,baud);uart_flush_input(UART_NUM_1);
        parser=Parser{};switched=now;
      }
      applied=cfg;have_applied=true;seen=rev;
    }
    if(rev!=persisted&&uint32_t(now-edited)>=700&&(!last_save||uint32_t(now-last_save)>=5000)){
      last_save=now;
      nvs_handle_t h;bool saved=false;
      if(nvs_open("mcx_gps",NVS_READWRITE,&h)==ESP_OK){saved=nvs_set_blob(h,"settings",&cfg,sizeof(cfg))==ESP_OK&&nvs_commit(h)==ESP_OK;nvs_close(h);}
      portENTER_CRITICAL(&mutex);if(revision==rev)diagnostics.saved=saved;portEXIT_CRITICAL(&mutex);
      if(saved)persisted=rev;

    }
    const int n=cfg.enabled?uart_read_bytes(UART_NUM_1,bytes,sizeof(bytes),pdMS_TO_TICKS(100)):0;
    if(!cfg.enabled)vTaskDelay(pdMS_TO_TICKS(100));
    if(n<0){io_error=-1;vTaskDelay(pdMS_TO_TICKS(100));}
    if(n>0)io_error=0;
    for(int i=0;i<n;++i)parser.feed(char(bytes[i]),now);
    if(cfg.enabled&&!cfg.baud&&uint32_t(now-switched)>5000&&(!parser.fix.frames||uint32_t(now-parser.fix.received_ms)>5000)){
      baud=baud==9600?38400:9600;io_error=uart_set_baudrate(UART_NUM_1,baud);uart_flush_input(UART_NUM_1);
      parser.collecting=false;parser.used=0;parser.fix.valid=false;switched=now;
    }
    portENTER_CRITICAL(&mutex);latest=parser.fix;diagnostics.baud=baud;diagnostics.error=io_error;if(n>0)diagnostics.bytes+=uint32_t(n);portEXIT_CRITICAL(&mutex);
  }
}
static void start(){
  if(task)return;
  nvs_handle_t h;
  if(nvs_open("mcx_gps",NVS_READONLY,&h)==ESP_OK){Settings loaded;size_t size=sizeof(loaded);
    if(nvs_get_blob(h,"settings",&loaded,&size)==ESP_OK&&size==sizeof(loaded)&&valid_settings(loaded))settings=loaded;
    nvs_close(h);
  }
  uart_config_t config={};config.baud_rate=9600;config.data_bits=UART_DATA_8_BITS;config.parity=UART_PARITY_DISABLE;config.stop_bits=UART_STOP_BITS_1;config.flow_ctrl=UART_HW_FLOWCTRL_DISABLE;config.source_clk=UART_SCLK_DEFAULT;
  esp_err_t rc=uart_param_config(UART_NUM_1,&config);
  // UART1 is routed to the header's RXD44/TXD43. No configuration is sent to the GPS.
  if(rc==ESP_OK)rc=uart_set_pin(UART_NUM_1,43,44,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);
  if(rc==ESP_OK)rc=uart_driver_install(UART_NUM_1,2048,0,0,nullptr,0);
  if(rc==ESP_OK&&xTaskCreatePinnedToCore(worker,"mcx_gnss",4096,nullptr,1,&task,0)!=pdPASS){uart_driver_delete(UART_NUM_1);rc=ESP_ERR_NO_MEM;}
  portENTER_CRITICAL(&mutex);diagnostics.error=rc;portEXIT_CRITICAL(&mutex);
  printf("[MCX-GNSS] UART1 RX44 TX43 start=%d; receive only\n",int(rc));
}
}
