#!/usr/bin/env python3
"""Exercise the production Meshtastic codec/session/store with sanitizers."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as folder:
    binary=Path(folder)/'meshtastic-test'
    subprocess.run(['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror',
                    '-Wno-unused-function','-fsanitize=address,undefined','-fno-omit-frame-pointer',
                    '-I'+str(ROOT/'firmware/display-prototype'),
                    str(ROOT/'tests/meshtastic_protocol_test.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary),str(Path(folder)/'card')],check=True)
    stubs=Path(folder)/'stubs';(stubs/'freertos').mkdir(parents=True)
    (stubs/'esp_heap_caps.h').write_text('''#pragma once
#include <cstdlib>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
static void *heap_caps_malloc(size_t n,int){return calloc(1,n);}
static void heap_caps_free(void *p){free(p);}
''')
    (stubs/'freertos/FreeRTOS.h').write_text('''#pragma once
#include <cstdint>
using TaskHandle_t=void *;using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define pdMS_TO_TICKS(n) (n)
#define portTICK_PERIOD_MS 1
''')
    (stubs/'freertos/task.h').write_text('''#pragma once
static uint32_t ticks=0;
static void vTaskDelay(uint32_t n){ticks+=n;}
static void vTaskDelete(void *){}
static uint32_t xTaskGetTickCount(){return ticks;}
static void xTaskCreatePinnedToCore(void (*)(void *),const char *,int,void *,int,TaskHandle_t *out,int){*out=(void *)1;}
''')
    (stubs/'esp_timer.h').write_text('#pragma once\nstatic int64_t fake_us=0;static int64_t esp_timer_get_time(){return fake_us;}\n')
    (stubs/'esp_random.h').write_text('#pragma once\nstatic uint32_t esp_random(){return 1234;}\n')
    runtime=Path(folder)/'runtime'
    subprocess.run(['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror',
                    '-Wno-unused-function','-Wno-unused-variable','-Wframe-larger-than=4096',
                    '-I'+str(stubs),'-I'+str(ROOT/'firmware/display-prototype'),
                    str(ROOT/'tests/meshtastic_runtime_test.cpp'),'-o',str(runtime)],check=True)
    subprocess.run([str(runtime),str(Path(folder)/'runtime-card')],check=True)
