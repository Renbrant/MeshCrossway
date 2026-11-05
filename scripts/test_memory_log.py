#!/usr/bin/env python3
"""Exercise the production diagnostic ring, timing, failure retention and console output."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    t=Path(directory);(t/'freertos').mkdir()
    (t/'esp_heap_caps.h').write_text('''#pragma once
#include <cstdlib>
#include <cstdint>
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_DMA 2
#define MALLOC_CAP_8BIT 4
#define MALLOC_CAP_SPIRAM 8
static bool fail_alloc=false;
static void *heap_caps_calloc(size_t n,size_t s,int caps){if(caps!=12)abort();return fail_alloc?nullptr:calloc(n,s);}
static uint32_t heap_caps_get_free_size(int){return 65536;}
''')
    (t/'esp_timer.h').write_text('#pragma once\n#include <cstdint>\nstatic int64_t us=0;\nstatic int64_t esp_timer_get_time(){return us;}\n')
    (t/'esp_system.h').write_text('#pragma once\nstatic int esp_reset_reason(){return 1;}\n')
    (t/'esp_random.h').write_text('#pragma once\nstatic unsigned esp_random(){return 123;}\n')
    (t/'freertos/FreeRTOS.h').write_text('#pragma once\nusing portMUX_TYPE=int;\n#define portMUX_INITIALIZER_UNLOCKED 0\n#define portENTER_CRITICAL(x) ((void)(x))\n#define portEXIT_CRITICAL(x) ((void)(x))\n')
    (t/'freertos/task.h').write_text('#pragma once\nstatic unsigned hwm=2048,stack_calls=0;\nstatic unsigned uxTaskGetStackHighWaterMark(void*){++stack_calls;return hwm;}\n')
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
 assert(output.find("[MCX-MEMLOG] v0.9.19,123,")==0);
 mcxmem::last_sample=30000;mcxmem::last_flush=59000;us=59999000;before=mcxmem::count;mcxmem::poll();assert(mcxmem::count==before);
 us=60000000;mcxmem::poll();assert(mcxmem::count==before+1-4);
 before=mcxmem::count;mcxmem::page(3);mcxmem::page(3);assert(mcxmem::count==before+1);
 free(mcxmem::rows.exchange(nullptr));
}
'''
    (t/'test.cpp').write_text(code)
    binary=t/'test'
    subprocess.run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(t),'-I'+str(ROOT/'firmware/display-prototype'),str(t/'test.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary),str(t)],check=True)
print('PASS: diagnostic allocation failure, bounded ring/overflow, stack cadence/minimum, console failure retention, bounded output and CSV formatting')
