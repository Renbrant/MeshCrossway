"""Run production map backend and navigation UI against host LVGL objects."""
from pathlib import Path
import os
import subprocess
ROOT=Path(__file__).resolve().parents[1]
def run(temp, flags, objects, allocator):
    (temp/'esp_netif_sntp.h').write_text('#pragma once\nstruct esp_sntp_config_t {const char *server;};\n#define ESP_NETIF_SNTP_DEFAULT_CONFIG(x) {x}\n#define ESP_ERR_INVALID_STATE 259\nstatic int esp_netif_sntp_init(const esp_sntp_config_t *){return 0;}\n')
    (temp/'esp_vfs_fat.h').write_text('#pragma once\n#include <cstdint>\nstatic uint64_t fake_free_space=1024ULL*1024*1024;\nstatic int esp_vfs_fat_info(const char *,uint64_t *total,uint64_t *free_space){*total=2ULL*1024*1024*1024;*free_space=fake_free_space;return 0;}\n')
    (temp/'esp_sdcard_port.h').write_text('''#pragma once
#include <cstdint>
static uint64_t fake_sd_size=1000000;
static uint64_t esp_sdcard_port_get_size(){return fake_sd_size;}
''')
    (temp/'nvs.h').write_text('''#pragma once
#include <cstddef>
using nvs_handle_t=int;
#define NVS_READONLY 0
#define NVS_READWRITE 1
static int nvs_open(const char *,int,nvs_handle_t *){return -1;}
static int nvs_get_blob(int,const char *,void *,size_t *){return -1;}
static int nvs_set_blob(int,const char *,const void *,size_t){return 0;}
static int nvs_commit(int){return 0;}
static void nvs_close(int){}
''')
    (temp/'esp_crt_bundle.h').write_text('#pragma once\nstatic int esp_crt_bundle_attach(void *){return 0;}\n')
    (temp/'esp_http_client.h').write_text('''#pragma once
#include <cstdint>
#include <vector>
#include <cstring>
#include <cassert>
#include <algorithm>
using esp_err_t=int;
#define ESP_OK 0
#define ESP_ERR_NO_MEM 257
struct esp_http_client_config_t {const char *url=nullptr;int timeout_ms=0,buffer_size=0,buffer_size_tx=0;const char *user_agent=nullptr;int (*crt_bundle_attach)(void *)=nullptr;bool disable_auto_redirect=false;};
using esp_http_client_handle_t=void *;
static bool fake_http_enabled=false,fake_read_failure=false;static int fake_http_calls=0,fake_http_status=200;static size_t fake_http_pos=0;
static std::vector<unsigned char> fake_http_body;
static void *esp_http_client_init(const esp_http_client_config_t *c){++fake_http_calls;assert(!strncmp(c->user_agent,"MeshCrossway/",12)&&c->crt_bundle_attach&&c->disable_auto_redirect);fake_http_pos=0;return fake_http_enabled?(void *)&fake_http_pos:nullptr;}
static int esp_http_client_open(void *,int){return 0;}
static int64_t esp_http_client_fetch_headers(void *){return fake_http_body.size();}
static int esp_http_client_get_status_code(void *){return fake_http_status;}
static int esp_http_client_read(void *,char *out,int cap){if(fake_read_failure&&fake_http_pos>=64)return -1;const size_t n=std::min(size_t(fake_read_failure?std::min(cap,64):cap),fake_http_body.size()-fake_http_pos);memcpy(out,fake_http_body.data()+fake_http_pos,n);fake_http_pos+=n;return int(n);}
static bool esp_http_client_is_complete_data_received(void *){return fake_http_pos==fake_http_body.size();}
static void esp_http_client_close(void *){}
static void esp_http_client_cleanup(void *){}
''')
    task_header=temp/'freertos/task.h'
    task_header.write_text(task_header.read_text().replace('static void vTaskDelay(unsigned){}', 'static void (*fake_delay_hook)(unsigned)=nullptr;\nstatic void vTaskDelay(unsigned n){if(fake_delay_hook)fake_delay_hook(n);}'))
    ui=(ROOT/'firmware/display-prototype/meshcrossway_ui.h').read_text()
    refresh=ui[ui.index('static void gps_refresh('):ui.index('static void draw(){',ui.index('static void gps_refresh('))]
    header=ui[ui.index('  gps_icon=color_label(top,'):ui.index('  top_battery=panel_at(top,')]
    actions=ui[ui.index('enum Action {'):ui.index('static void draw(); // Forward-declared')]
    glyphs=ui[ui.index('static const char *context_glyph('):ui.index('// Place a separate, thin scroll indicator')]
    icons=ui[ui.index('static void context_icon('):ui.index('static void ble_select_now(')]
    test=(ROOT/'tests/map_navigation_test.cpp').read_text().replace('// PRODUCTION_GPS_REFRESH',refresh).replace('// PRODUCTION_GPS_HEADER',header).replace('// PRODUCTION_ACTIONS',actions).replace('// PRODUCTION_CONTEXT',glyphs+icons)
    source=temp/'map_navigation.cpp';source.write_text(allocator+test)
    binary=temp/'map_navigation'
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror=misleading-indentation','-Wno-unused-function','-Wno-unused-variable','-fsanitize=address,undefined',*flags,str(source),*objects,'-lm','-o',str(binary)],check=True)
    subprocess.run([str(binary),str(temp/'card')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

    (temp/'driver').mkdir(exist_ok=True)
    (temp/'driver/uart.h').write_text(r'''#pragma once
#include <cstdint>
#include "esp_http_client.h"
#define UART_NUM_1 1
#define UART_DATA_8_BITS 8
#define UART_PARITY_DISABLE 0
#define UART_STOP_BITS_1 1
#define UART_HW_FLOWCTRL_DISABLE 0
#define UART_SCLK_DEFAULT 0
#define UART_PIN_NO_CHANGE -1
struct uart_config_t {int baud_rate=0,data_bits=0,parity=0,stop_bits=0,flow_ctrl=0,source_clk=0;};
static int fake_uart_rx=0,fake_uart_tx=0,fake_uart_baud=0,fake_baud_calls=0,fake_flush_calls=0;
static int (*fake_uart_read)(void *,unsigned)=nullptr;
static int uart_param_config(int,const uart_config_t *){return 0;}
static int uart_set_pin(int,int tx,int rx,int,int){fake_uart_tx=tx;fake_uart_rx=rx;return 0;}
static int uart_driver_install(int,int,int,int,void *,int){return 0;}
static int uart_driver_delete(int){return 0;}
static int uart_set_baudrate(int,int rate){++fake_baud_calls;fake_uart_baud=rate;return 0;}
static int uart_flush_input(int){++fake_flush_calls;return 0;}
static int uart_read_bytes(int,void *p,unsigned cap,unsigned){return fake_uart_read?fake_uart_read(p,cap):0;}
''')
    source=temp/'gnss_runtime.cpp';source.write_text(allocator+(ROOT/'tests/gnss_runtime_test.cpp').read_text())
    binary=temp/'gnss_runtime'
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror=misleading-indentation','-Wno-unused-function','-Wno-unused-variable','-fsanitize=address,undefined',*flags,str(source),*objects,'-lm','-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

    (temp/'freertos/semphr.h').write_text(r'''#pragma once
#include "FreeRTOS.h"
#define pdTRUE 1
#define portMAX_DELAY 0xffffffffu
using SemaphoreHandle_t=void *;
static SemaphoreHandle_t xSemaphoreCreateMutex(){return (void *)1;}
static int xSemaphoreTake(void *,unsigned){return pdTRUE;}
static void xSemaphoreGive(void *){}
''')
    source=temp/'navigation_runtime.cpp';source.write_text(allocator+(ROOT/'tests/navigation_runtime_test.cpp').read_text())
    binary=temp/'navigation_runtime'
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror=misleading-indentation','-Wno-unused-function','-Wno-unused-variable','-fsanitize=address,undefined',*flags,str(source),*objects,'-lm','-o',str(binary)],check=True)
    subprocess.run([str(binary),str(temp/'navigation-card')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
