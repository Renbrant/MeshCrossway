#pragma once
#include "meshcrossway_gnss_parser.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
namespace mcxgnss {
static portMUX_TYPE mutex=portMUX_INITIALIZER_UNLOCKED;
static Fix latest;
static unsigned baud=9600;
static int error=0;
static TaskHandle_t task=nullptr;
static Fix snapshot(){portENTER_CRITICAL(&mutex);Fix out=latest;portEXIT_CRITICAL(&mutex);if(uint32_t(uint32_t(esp_timer_get_time()/1000)-out.position_ms)>3000)out.valid=false;return out;}
static void worker(void *){
  Parser parser;uint8_t bytes[128];uint32_t switched=uint32_t(esp_timer_get_time()/1000);
  for(;;){
    const int n=uart_read_bytes(UART_NUM_1,bytes,sizeof(bytes),pdMS_TO_TICKS(100));
    const uint32_t now=uint32_t(esp_timer_get_time()/1000);
    if(n<0){portENTER_CRITICAL(&mutex);error=-1;portEXIT_CRITICAL(&mutex);vTaskDelay(pdMS_TO_TICKS(100));continue;}
    for(int i=0;i<n;++i)parser.feed(char(bytes[i]),now);
    if(uint32_t(now-switched)>5000&&(!parser.fix.frames||uint32_t(now-parser.fix.received_ms)>5000)){
      baud=baud==9600?38400:9600;const esp_err_t rc=uart_set_baudrate(UART_NUM_1,baud);
      uart_flush_input(UART_NUM_1);parser.collecting=false;parser.used=0;parser.fix.valid=false;switched=now;
      portENTER_CRITICAL(&mutex);error=rc;portEXIT_CRITICAL(&mutex);
    }
    portENTER_CRITICAL(&mutex);latest=parser.snapshot(now);portEXIT_CRITICAL(&mutex);
  }
}
static void start(){
  if(task)return;
  uart_config_t config={};config.baud_rate=9600;config.data_bits=UART_DATA_8_BITS;config.parity=UART_PARITY_DISABLE;config.stop_bits=UART_STOP_BITS_1;config.flow_ctrl=UART_HW_FLOWCTRL_DISABLE;config.source_clk=UART_SCLK_DEFAULT;
  esp_err_t rc=uart_param_config(UART_NUM_1,&config);
  // UART1 is routed to the header's RXD44/TXD43. No configuration is sent to the GPS.
  if(rc==ESP_OK)rc=uart_set_pin(UART_NUM_1,43,44,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);
  if(rc==ESP_OK)rc=uart_driver_install(UART_NUM_1,2048,0,0,nullptr,0);
  if(rc==ESP_OK&&xTaskCreatePinnedToCore(worker,"mcx_gnss",4096,nullptr,1,&task,0)!=pdPASS){uart_driver_delete(UART_NUM_1);rc=ESP_ERR_NO_MEM;}
  portENTER_CRITICAL(&mutex);error=rc;portEXIT_CRITICAL(&mutex);
  printf("[MCX-GNSS] UART1 RX44 TX43 start=%d; receive only\n",int(rc));
}
}
