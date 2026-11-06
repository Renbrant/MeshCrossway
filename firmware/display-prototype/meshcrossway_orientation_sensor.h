#pragma once
#include <atomic>
#include <stdio.h>
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "meshcrossway_orientation_policy.h"
extern i2c_master_bus_handle_t i2c_bus_handle;
namespace mcxorientation {
static std::atomic<bool> paused{false},available{false};
// Publish sign and timestamp in one atomic word (200ms sampling, 750ms expiry).
static std::atomic<uint32_t> sample{0};
static i2c_master_dev_handle_t sensor=nullptr;
static bool read_reg(uint8_t reg,uint8_t *data,size_t n){
  return i2c_master_transmit_receive(sensor,&reg,1,data,n,12)==ESP_OK;
}
static bool write_reg(uint8_t reg,uint8_t value){
  const uint8_t data[]={reg,value};return i2c_master_transmit(sensor,data,2,12)==ESP_OK;
}
// Called once by the existing battery worker. No LVGL lock, task, FIFO, gyro,
// blocking retry loop, self-test, bus reinitialization or full sensor library.
static void sensor_init(){
  const uint8_t addresses[]={0x6b,0x6a};
  for(uint8_t address:addresses){
    i2c_device_config_t cfg={};cfg.dev_addr_length=I2C_ADDR_BIT_LEN_7;
    cfg.device_address=address;cfg.scl_speed_hz=400000;
    if(i2c_master_bus_add_device(i2c_bus_handle,&cfg,&sensor)!=ESP_OK){sensor=nullptr;continue;}
    uint8_t id=0;
    if(read_reg(0,&id,1)&&id==0x05&&write_reg(0x08,0)&&
       write_reg(0x02,0x40)&&write_reg(0x03,0x08)&& // little-endian, 2g, 31.25Hz
       write_reg(0x06,0x01)&&write_reg(0x08,0x01)){
      available.store(true);printf("[MCX-ROTATE] QMI8658 ready; accel only, poll=200ms\n");return;
    }
    i2c_master_bus_rm_device(sensor);sensor=nullptr;
  }
  printf("[MCX-ROTATE] sensor unavailable; manual rotation retained\n");
}
static void sensor_poll(){
  if(!sensor||!available.load()||paused.load()){sample.store(0);return;}
  uint8_t status=0,data[6]={};
  if(!read_reg(0x2e,&status,1)||!(status&1)||!read_reg(0x35,data,6)){sample.store(0);return;}
  auto axis=[&](int i){return float(int16_t(uint16_t(data[i])|(uint16_t(data[i+1])<<8)))/16384.f;};
  const int sign=gravity_sign(axis(0),axis(2),axis(4));
  const uint32_t now=uint32_t(esp_timer_get_time()/1000);
  sample.store(sign?((now&0x3fffffffu)<<2)|(sign>0?1u:2u):0);
}
static int fresh_sign(){
  const uint32_t v=sample.load(),now=uint32_t(esp_timer_get_time()/1000)&0x3fffffffu;
  return !v||((now-(v>>2))&0x3fffffffu)>750?0:(v&3)==1?1:-1;
}
}
