#pragma once
// Waveshare ESP32-S3-Touch-LCD-3.5 Rev2: read-only AXP2101 telemetry.
// The vendor's esp_axp2101_port.cpp already creates the PMU device on the
// shared I2C bus. DO NOT add a second I2C device, reconfigure charger rails,
// or write any PMU register. Readings run in a low-priority worker, never LVGL.
#include <atomic>
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "meshcrossway_battery_decode.h"
#include "meshcrossway_orientation_sensor.h"

// Existing global handle in Waveshare's ESP-IDF 02_lvgl_example BSP.
extern i2c_master_dev_handle_t i2c_device;
namespace meshpower {
static std::atomic<uint32_t> published{0u};
static TaskHandle_t worker=nullptr;

static inline mcxbattery::Reading snapshot(){
  return mcxbattery::unpack(published.load(std::memory_order_relaxed));
}
static bool read_only(uint8_t reg,uint8_t &value){
  if(!i2c_device)return false;
  // 35 ms bound: do not hold the touch controller's shared I2C bus forever.
  return i2c_master_transmit_receive(i2c_device,&reg,1,&value,1,35)==ESP_OK;
}
static void task(void *){
  // Allow the vendor AXP2101 initialization and the LVGL input service to settle.
  vTaskDelay(pdMS_TO_TICKS(6000));
  mcxorientation::sensor_init();
  uint32_t battery_at=0;bool first=true;
  for(;;){
    mcxorientation::sensor_poll();
    const uint32_t now=uint32_t(esp_timer_get_time()/1000);
    if(first||uint32_t(now-battery_at)>=15000){
    first=false;battery_at=now;
    uint8_t id=0,s1=0,s2=0,pct=255;
    if(read_only(0x03,id)&&read_only(0x00,s1)&&
       read_only(0x01,s2)&&read_only(0xA4,pct)){
      published.store(mcxbattery::pack(mcxbattery::decode(id,s1,s2,pct)),
                      std::memory_order_relaxed);
    }else{
      published.store(0u,std::memory_order_relaxed); // unknown, not zero!
    }
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}
static void start(){
  if(worker)return;
  const BaseType_t ok=xTaskCreate(task,"mcx_battery_ro",4096,nullptr,
                                  tskIDLE_PRIORITY+1,&worker);
  if(ok!=pdPASS)worker=nullptr; // UI remains usable without battery telemetry.
}
} // namespace meshpower
