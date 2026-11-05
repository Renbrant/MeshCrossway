#!/usr/bin/env python3
"""Exercise the production diagnostic ring, timing, failure retention and console output."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    t=Path(directory);(t/'freertos').mkdir()
    (t/'esp_attr.h').write_text('#pragma once\n#define DRAM_ATTR\n')
    (t/'esp_heap_caps.h').write_text('''#pragma once
#include <cstdlib>
#include <cstdint>
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_DMA 2
#define MALLOC_CAP_8BIT 4
#define MALLOC_CAP_SPIRAM 8
static bool fail_alloc=false;
static unsigned heap_calls=0;
static void *heap_caps_calloc(size_t n,size_t s,int caps){++heap_calls;if(caps!=12)abort();return fail_alloc?nullptr:calloc(n,s);}
static uint32_t heap_caps_get_free_size(int){++heap_calls;return 65536;}
''')
    (t/'esp_timer.h').write_text('#pragma once\n#include <cstdint>\nstatic int64_t us=0;\nstatic int64_t esp_timer_get_time(){return us;}\n')
    (t/'esp_system.h').write_text('#pragma once\nstatic int esp_reset_reason(){return 1;}\n')
    (t/'esp_random.h').write_text('#pragma once\nstatic unsigned esp_random(){return 123;}\n')
    (t/'freertos/FreeRTOS.h').write_text('#pragma once\nusing portMUX_TYPE=int;\n#define portMUX_INITIALIZER_UNLOCKED 0\n#define portENTER_CRITICAL(x) ((void)(x))\n#define portEXIT_CRITICAL(x) ((void)(x))\n#define portENTER_CRITICAL_SAFE(x) ((void)(x))\n#define portEXIT_CRITICAL_SAFE(x) ((void)(x))\nstatic bool in_isr=false;\nstatic int xPortInIsrContext(){return in_isr;}\nstatic int xPortGetCoreID(){return 1;}\n')
    (t/'freertos/task.h').write_text('#pragma once\nstatic unsigned hwm=2048,stack_calls=0;\nstatic void *task_handle=(void*)1;\nstatic const char *task_name="map worker";\nstatic void *xTaskGetCurrentTaskHandle(){if(in_isr)abort();return task_handle;}\nstatic const char *pcTaskGetName(void*){if(in_isr)abort();return task_name;}\nstatic unsigned uxTaskGetStackHighWaterMark(void*){++stack_calls;return hwm;}\n')
    code=r'''
#define MCX_MEMORY_DIAGNOSTICS 1
#include <string>
static std::string output;
static bool fail_write=false;
static size_t sink(const char *data,size_t length){if(fail_write)return 0;output.append(data,length);return length;}
#define MCX_MEMLOG_WRITE(data, length) sink(data, length)
#include "meshcrossway_memory_log.h"
#include <cassert>
#include <string>
int main(){
 fail_alloc=true;mcxmem::begin();assert(!mcxmem::rows.load());
 fail_alloc=false;mcxmem::begin();assert(mcxmem::count==1);
 mcxmem::observe(mcxmem::GNSS);assert(stack_calls==1);mcxmem::observe(mcxmem::GNSS);assert(stack_calls==1);
 us=30000000;hwm=1024;mcxmem::observe(mcxmem::GNSS);assert(stack_calls==2&&mcxmem::stacks[mcxmem::GNSS]==1024);
 mcxmem::allocation_failed(400,3);mcxmem::capture(9,2);
 auto &row=mcxmem::rows.load()[1];assert(row.boot==123&&row.failures==1&&row.failed_bytes==400&&row.stack[mcxmem::GNSS]==1024);
 for(unsigned i=0;i<40;++i)mcxmem::capture();
 assert(mcxmem::count==32&&mcxmem::dropped==10);
 unsigned before=mcxmem::count;
 fail_write=true;mcxmem::flush();assert(mcxmem::count==before&&mcxmem::write_errors==1);
 fail_write=false;mcxmem::flush();assert(mcxmem::count==before-4);
 unsigned lines=0;for(char ch:output)if(ch=='\n')++lines;assert(lines==4);
 assert(output.find("[MCX-MEMLOG] v0.9.23,123,")==0);
 mcxmem::last_sample=30000;mcxmem::last_flush=59000;us=59999000;before=mcxmem::count;mcxmem::poll();assert(mcxmem::count==before);
 us=60000000;mcxmem::poll();assert(mcxmem::count==before+1-4);
 before=mcxmem::count;mcxmem::page(3);mcxmem::page(3);assert(mcxmem::count==before+1);

 // No heap query/allocation or console write is allowed in the failure callback.
 mcxmem::failure_count=mcxmem::failure_head=mcxmem::failure_dropped=0;
 output.clear();const auto calls=heap_calls;
 us=123456000;mcxmem::allocation_failed(124,2060,"heap_caps_malloc");
 assert(heap_calls==calls&&output.empty());
 fail_write=true;mcxmem::flush_failures();assert(mcxmem::failure_count==1);
 fail_write=false;mcxmem::flush_failures();assert(mcxmem::failure_count==0);
 assert(output.find("uptime_ms=123456,bytes=124,caps=0x80c,core=1,isr=0,task=map_worker,function=heap_caps_malloc,dropped=0")!=std::string::npos);
 output.clear();in_isr=true;mcxmem::allocation_failed(8,7,nullptr);in_isr=false;
 task_handle=nullptr;mcxmem::allocation_failed(9,7,"calloc");task_handle=(void*)1;
 mcxmem::flush_failures();assert(output.find("isr=1,task=ISR,function=unknown")!=std::string::npos);
 assert(output.find("isr=0,task=startup,function=calloc")!=std::string::npos);
 task_name="long,name\nwith_01234567890123456789";
 for(unsigned i=0;i<7;++i)mcxmem::allocation_failed(100+i,7,"a_long_allocation_function_name_more_than_32_chars");
 assert(mcxmem::failure_count==4&&mcxmem::failure_dropped==3);
 output.clear();mcxmem::flush_failures();assert(mcxmem::failure_count==2);
 assert(output.find("bytes=103,")!=std::string::npos&&output.find("bytes=104,")!=std::string::npos);
 assert(output.find("task=long_name_with_")!=std::string::npos&&output.find("dropped=3")!=std::string::npos);
 mcxmem::flush_failures();assert(mcxmem::failure_count==0);
 assert(heap_calls==calls);
 free(mcxmem::rows.exchange(nullptr));
}
'''
    (t/'test.cpp').write_text(code)
    binary=t/'test'
    subprocess.run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(t),'-I'+str(ROOT/'firmware/display-prototype'),str(t/'test.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary),str(t)],check=True)
print('PASS: diagnostic allocation task/API/ISR attribution, callback heap/I-O exclusion, overflow/drop accounting, failure retention, bounded ring/overflow, stack cadence/minimum, console failure retention, bounded output and CSV formatting')
