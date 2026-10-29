#!/usr/bin/env python3
"""Run the production conversation + journal code with ASan/UBSan on the host."""
from pathlib import Path
import subprocess
import tempfile
from check_chat_memory import check

for size in (33188, 7296, 4096):
    try:
        check(f"1: 3fc80000 {size} OBJECT LOCAL DEFAULT 30 mcxhistory::regression")
    except ValueError:
        pass
    else:
        raise AssertionError("ELF guard accepted a large static chat buffer")

def compile_worker(temp, root):
    stubs=temp/'stubs';(stubs/'freertos').mkdir(parents=True)
    (stubs/'esp_heap_caps.h').write_text('''#pragma once
#include <cstdlib>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
static int last_caps=0,calls=0;
static size_t last_bytes=0;
static bool fail_allocation=false;
static void *heap_caps_malloc(size_t n,int caps){last_caps=caps;last_bytes=n;++calls;return fail_allocation?nullptr:malloc(n);}
static void heap_caps_free(void *p){free(p);}
''')
    (stubs/'freertos/FreeRTOS.h').write_text('''#pragma once
#include <cstdint>
using TaskHandle_t=void *;
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define pdMS_TO_TICKS(n) (n)
''')
    (stubs/'freertos/task.h').write_text('''#pragma once
static void vTaskDelay(uint32_t){}
static void vTaskDelete(void *){}
static uint32_t xTaskGetTickCount(){return 0;}
static void xTaskCreatePinnedToCore(void (*)(void *),const char *,int,void *,int,TaskHandle_t *,int){}
''')
    source=temp/'worker.cpp';source.write_text(r'''#include "meshcrossway_sd_history.h"
#include <cassert>
int main(){
  assert(!mcxhistory::workspace());
  fail_allocation=true;assert(!mcxhistory::init_workspace());assert(!mcxhistory::workspace());
  fail_allocation=false;assert(mcxhistory::init_workspace());
  assert(last_caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  assert(last_bytes==sizeof(mcxchat::Workspace));
  assert(mcxhistory::preferences().quick[0][0]);
  mcxhistory::preferences().revision=42;
  const int allocations=calls;assert(mcxhistory::init_workspace());assert(calls==allocations);
  assert(mcxhistory::preferences().revision==42);
  printf("PASS: %zu-byte workspace uses explicit PSRAM; OOM has no internal fallback; initialization preserves preferences\n",sizeof(mcxchat::Workspace));
}
''')
    subprocess.run(['c++','-std=c++20','-O1','-Wall','-Werror=all','-Wno-unused-function',
        '-Wno-unused-variable','-Wframe-larger-than=4096','-I'+str(stubs),
        '-I'+str(root/'firmware/display-prototype'),str(source),'-o',str(temp/'worker')],check=True)
    subprocess.run([str(temp/'worker')],check=True)
    subprocess.run(['python3',str(root/'scripts/check_chat_memory.py'),str(temp/'worker')],check=True)

ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as temp:
    compile_worker(Path(temp),ROOT)
    binary=Path(temp)/'history-test'
    subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer','-I'+str(ROOT/'firmware/display-prototype'),
                    str(ROOT/'tests/conversations_history_test.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary),str(Path(temp)/'card')],check=True)
