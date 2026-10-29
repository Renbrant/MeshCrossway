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
#include <cstring>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
static int last_caps=0,calls=0;
static size_t last_bytes=0;
static bool fail_allocation=false;
static void *heap_caps_malloc(size_t n,int caps){last_caps=caps;last_bytes=n;++calls;return fail_allocation?nullptr:malloc(n);}
static void *heap_caps_calloc(size_t n,size_t size,int caps){void *p=heap_caps_malloc(n*size,caps);if(p)memset(p,0,n*size);return p;}
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
#define portTICK_PERIOD_MS 1
''')
    (stubs/'freertos/task.h').write_text('''#pragma once
static uint32_t fake_ticks=0;
static void (*idle_hook)(uint32_t)=nullptr;
static void vTaskDelay(uint32_t n){fake_ticks+=n;if(idle_hook)idle_hook(n);}
static void vTaskDelete(void *){}
static uint32_t xTaskGetTickCount(){return fake_ticks;}
static int task_calls=0;
static void xTaskCreatePinnedToCore(void (*)(void *),const char *,int,void *,int,TaskHandle_t *out,int){++task_calls;*out=(void *)1;}
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
  auto *client=new meshmc::Client();
  mcxhistory::ble_startup.store(true);mcxhistory::poll(*client,0,0);assert(task_calls==0);
  mcxhistory::ble_startup.store(false);mcxhistory::poll(*client,0,0);assert(task_calls==1);
  mcxhistory::poll(*client,0,0);assert(task_calls==1);
  // First offline restore is queued at boot with BLE still off, not at 5/12 s.
  assert(mcxhistory::count==1&&mcxhistory::offline_requested&&mcxhistory::jobs[0].offline);
  mcxhistory::head=mcxhistory::count=0;
  mcxhistory::loaded_offline=true;mcxhistory::load_unavailable=true;
  mcxhistory::complete.store(true);mcxhistory::poll(*client,100,0);
  mcxhistory::poll(*client,5099,0);assert(mcxhistory::count==0);
  mcxhistory::poll(*client,5100,0);assert(mcxhistory::count==1);
  mcxhistory::head=mcxhistory::count=0; // Mock worker consumed the retry.
  assert(!client->connected&&client->qcount==0);

  client->public_key[0]=7;mcxhistory::restored=true;mcxhistory::preferences().revision=42;
  fail_allocation=true;assert(!mcxhistory::before_attach(*client,100));
  assert(mcxhistory::restored&&mcxhistory::preferences().revision==42&&client->public_key[0]==7);
  fail_allocation=false;mcxhistory::restored=false;
  client->connected=true;client->initialized=false;client->fault=true;
  mcxhistory::bound_epoch=mcxhistory::loaded_epoch=5;memcpy(mcxhistory::bound_radio,client->public_key,32);
  mcxhistory::loaded=mcxhistory::allocate();mcxhistory::loaded->meta.radio[0]=7;
  mcxhistory::loaded_offline=false;mcxhistory::load_ok=true;mcxhistory::restore_requested=true;
  mcxhistory::complete.store(true);mcxhistory::poll(*client,2000,5);
  assert(!mcxhistory::restored&&!client->history_ready&&mcxhistory::count==0);
  delete client;
  printf("PASS: %zu-byte workspace uses explicit PSRAM; OOM has no internal fallback; initialization preserves preferences\n",sizeof(mcxchat::Workspace));
}
''')
    subprocess.run(['c++','-std=c++20','-O1','-Wall','-Werror=all','-Wno-unused-function',
        '-Wno-unused-variable','-Wframe-larger-than=4096','-I'+str(stubs),
        '-I'+str(root/'firmware/display-prototype'),str(source),'-o',str(temp/'worker')],check=True)
    subprocess.run([str(temp/'worker')],check=True)
    subprocess.run(['python3',str(root/'scripts/check_chat_memory.py'),str(temp/'worker')],check=True)
    subprocess.run(['c++','-std=c++20','-O1','-Wall','-Werror=all','-Wno-unused-function',
        '-Wno-unused-variable','-Wframe-larger-than=4096','-I'+str(stubs),
        '-I'+str(root/'firmware/display-prototype'),str(root/'tests/sd_history_worker_test.cpp'),
        '-o',str(temp/'worker-flow')],check=True)
    subprocess.run([str(temp/'worker-flow'),str(temp/'worker-card')],check=True)
    (stubs/'mbedtls').mkdir()
    (stubs/'mbedtls/sha256.h').write_text('#pragma once\nstatic int mbedtls_sha256(const unsigned char *,size_t,unsigned char *,int){return -1;}\n')
    (stubs/'esp_random.h').write_text('#pragma once\nstatic void esp_fill_random(void *p,size_t n){memset(p,0x5a,n);}\n')
    (stubs/'esp_timer.h').write_text('#pragma once\nstatic int64_t esp_timer_get_time(){return 1000000;}\n')
    subprocess.run(['c++','-std=c++20','-O1','-Wall','-Werror=all','-Wno-unused-function',
        '-Wno-unused-variable','-Wframe-larger-than=4096','-I'+str(stubs),
        '-I'+str(root/'firmware/display-prototype'),str(root/'tests/meshcore_transport_test.cpp'),
        '-o',str(temp/'transport')],check=True)
    subprocess.run([str(temp/'transport')],check=True)

ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as temp:
    compile_worker(Path(temp),ROOT)
    binary=Path(temp)/'history-test'
    subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer','-I'+str(ROOT/'firmware/display-prototype'),
                    str(ROOT/'tests/conversations_history_test.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary),str(Path(temp)/'card')],check=True)
