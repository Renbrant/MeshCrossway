#!/usr/bin/env python3
"""Compile actual reset/latency bookkeeping and HTTP health formatting on host."""
from pathlib import Path
import json
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
health=(ROOT/'firmware/display-prototype/meshcrossway_health.h').read_text()
health=health[health.index('static esp_err_t health(httpd_req_t *req){'):health.index('static void start(){')]
with tempfile.TemporaryDirectory() as directory:
    temp=Path(directory);(temp/'freertos').mkdir()
    (temp/'esp_attr.h').write_text('#pragma once\n#define RTC_NOINIT_ATTR\n')
    (temp/'esp_system.h').write_text('''#pragma once
#include <stdint.h>
using esp_err_t=int;
static constexpr int ESP_OK=0,ESP_FAIL=-1,ESP_RST_POWERON=1;
static int reset=ESP_RST_POWERON;
static int esp_reset_reason(){return reset;}
static uint32_t esp_get_free_heap_size(){return UINT32_MAX;}
''')
    (temp/'esp_heap_caps.h').write_text('''#pragma once
#include <stddef.h>
#include <stdint.h>
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
static void (*allocation_hook)(size_t,uint32_t,const char *)=nullptr;
static int heap_caps_register_failed_alloc_callback(void (*hook)(size_t,uint32_t,const char *)){allocation_hook=hook;return 0;}
static uint32_t heap_caps_get_free_size(int){return UINT32_MAX;}
static uint32_t heap_caps_get_largest_free_block(int){return UINT32_MAX;}
static uint32_t heap_caps_get_minimum_free_size(int){return UINT32_MAX;}
''')
    (temp/'esp_timer.h').write_text('#pragma once\n#include <stdint.h>\nstatic int64_t fake_us=0;\nstatic int64_t esp_timer_get_time(){return fake_us;}\n')
    (temp/'freertos/FreeRTOS.h').write_text('#pragma once\n')
    (temp/'freertos/task.h').write_text('#pragma once\nstatic unsigned uxTaskGetStackHighWaterMark(void *){return 2048;}\n')
    code=r'''
#include "meshcrossway_runtime.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
struct httpd_req_t{};
static std::string result;
static void headers(httpd_req_t *,const char *){}
static int httpd_resp_send(httpd_req_t *,const char *data,size_t n){result.assign(data,n);return ESP_OK;}
namespace meshnetwork {static void get_status(char *ip,size_t n,char *state,size_t cap){snprintf(ip,n,"255.255.255.255");snprintf(state,cap,"connected");}}
namespace meshbackup {
static const char *status(){return "write failed";}
static const char *step_name(int){return "remove invalid";}
static std::atomic<int> failure_step{12},failure_errno{-2147483647};
}
namespace mcxhistory {
static void describe(char *s,size_t n){memset(s,'x',n-1);s[n-1]=0;}
static std::atomic<int> error_step{12},error_number{-2147483647};
static std::atomic<uint32_t> failures{UINT32_MAX},operation_ms{UINT32_MAX},max_operation_ms{UINT32_MAX};
}
namespace mcxstore {static const char *step_name(int){return "directory";}}
namespace mcxnav {static std::atomic<uint32_t> stack_min{UINT32_MAX};}
namespace meshmc {static std::atomic<uint32_t> mailbox_peak{128},mailbox_overflows{UINT32_MAX};}
namespace meshcapture {
static bool available(){return false;}
static std::atomic<uint32_t> captures{UINT32_MAX},failures{UINT32_MAX},last_capture_ms{UINT32_MAX};
}
'''+health+r'''
int main(){
  mcxruntime::begin();assert(mcxruntime::boots==1&&mcxruntime::previous_stage==0&&allocation_hook);
  {mcxruntime::Scope ui(mcxruntime::DRAW);fake_us+=5000;
    {mcxruntime::Scope restore(mcxruntime::HISTORY);fake_us+=7000;}
    assert(mcxruntime::retained.stage==mcxruntime::DRAW);
  }
  assert(mcxruntime::ui_max_ms==12&&mcxruntime::retained.stage==mcxruntime::IDLE);
  mcxruntime::sample_ui_stack();assert(mcxruntime::ui_stack_min==2048);
  allocation_hook(256,3,"fake");assert(mcxruntime::allocation_failures==1&&mcxruntime::last_allocation_bytes==256);
  reset=4;mcxruntime::retained.stage=mcxruntime::CAPTURE;mcxruntime::begin();
  assert(mcxruntime::boots==2&&mcxruntime::previous_stage==mcxruntime::CAPTURE);
  reset=ESP_RST_POWERON;mcxruntime::begin();assert(mcxruntime::boots==1&&mcxruntime::previous_stage==0);
  fake_us=INT64_MAX;httpd_req_t request;assert(health(&request)==ESP_OK&&result.size()<1280);
  assert(result.find("\"navigation_stack_min\":0")!=std::string::npos);
  mcxnav::stack_min=UINT32_MAX-1;assert(health(&request)==ESP_OK&&result.size()<1280);
  puts(result.c_str());
}
'''
    cpp=temp/'runtime.cpp';cpp.write_text(code);binary=temp/'runtime'
    subprocess.run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Wformat=2','-Werror',
                    '-I'+str(temp),'-I'+str(ROOT/'firmware/display-prototype'),str(cpp),'-o',str(binary)],check=True)
    result=json.loads(subprocess.check_output([str(binary)],text=True))
    assert result['navigation_stack_min']==4294967294
    assert result['history_failures']==4294967295 and result['mailbox_peak']==128
    assert result['reset_reason']==1 and result['ui_max_ms']==12 and result['internal_largest']==4294967295
    print('PASS: production warm/cold reset breadcrumbs, nested latency, allocation failure/stack metrics and worst-case health JSON')
