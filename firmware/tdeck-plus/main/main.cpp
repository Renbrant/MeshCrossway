// MeshCrossway controller for the LilyGO T-Deck Plus: two independent BLE
// radios (Meshtastic + MeshCore), keyboard/trackball UI. No Wi-Fi in v0.1.
#include <stdio.h>
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "nvs_flash.h"
#include "meshcrossway_ble.h"
#include "meshcrossway_offline_boot.h"
#include "tdeck_board.h"
#include "tdeck_ui.h"

static lv_disp_draw_buf_t draw_buffer;
static lv_disp_drv_t display_driver;
static SemaphoreHandle_t lvgl_mutex = nullptr;

static bool flush_done(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *, void *context) {
  lv_disp_flush_ready((lv_disp_drv_t *)context);
  return false;
}
static void flush(lv_disp_drv_t *, const lv_area_t *area, lv_color_t *pixels) {
  esp_lcd_panel_draw_bitmap(tdeck::panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, pixels);
}
static void tick(void *) { lv_tick_inc(2); }

// Keyboard + trackball as one LVGL keypad: roll moves focus, click/Enter activates.
static uint32_t pending_key = 0;
static bool key_down = false, click_down = false;
static void keypad_read(lv_indev_drv_t *, lv_indev_data_t *data) {
  if (key_down) { // Release the key reported on the previous read.
    data->key = pending_key;
    data->state = LV_INDEV_STATE_RELEASED;
    key_down = false;
    return;
  }
  const bool click = tdeck::ball_pressed();
  if (click != click_down) {
    click_down = click;
    data->key = LV_KEY_ENTER;
    data->state = click ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    return;
  }
  uint32_t key = 0;
  constexpr int STEP = 2; // Pulses per focus move; the ball is very sensitive.
  if (tdeck::ball_vertical.load() <= -STEP) { tdeck::ball_vertical.fetch_add(STEP); key = LV_KEY_PREV; }
  else if (tdeck::ball_vertical.load() >= STEP) { tdeck::ball_vertical.fetch_sub(STEP); key = LV_KEY_NEXT; }
  else if (tdeck::ball_horizontal.load() <= -STEP) { tdeck::ball_horizontal.fetch_add(STEP); key = LV_KEY_PREV; }
  else if (tdeck::ball_horizontal.load() >= STEP) { tdeck::ball_horizontal.fetch_sub(STEP); key = LV_KEY_NEXT; }
  else {
    const char c = tdeck::read_key();
    if (c == '\r' || c == '\n') key = LV_KEY_ENTER;
    else if (c == 0x08) key = LV_KEY_BACKSPACE;
    else if (c >= 0x20 && c < 0x7f) key = uint32_t(c);
  }
  data->key = key ? key : pending_key;
  data->state = key ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  if (key) {
    pending_key = key;
    key_down = true;
  }
}
static void touch_read(lv_indev_drv_t *, lv_indev_data_t *data) {
  static int16_t x = 0, y = 0;
  const bool pressed = tdeck::read_touch(x, y);
  data->point.x = x;
  data->point.y = y;
  data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void lvgl_task(void *) {
  for (;;) {
    uint32_t wait = 10;
    if (xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
      wait = lv_timer_handler();
      xSemaphoreGiveRecursive(lvgl_mutex);
    }
    vTaskDelay(pdMS_TO_TICKS(wait < 5 ? 5 : wait > 30 ? 30 : wait));
  }
}

extern "C" void app_main(void) {
  printf("[MCX-TDECK] MeshCrossway T-Deck Plus %s starting\n", MCX_TDECK_VERSION);
  esp_err_t rc = nvs_flash_init();
  if (rc == ESP_ERR_NVS_NO_FREE_PAGES || rc == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // Never erase silently: saved BLE bonds live here. Report and continue.
    printf("[MCX-TDECK] NVS needs repair (%s); BLE bonds unavailable\n", esp_err_to_name(rc));
  }
  tdeck::power_on();
  ESP_ERROR_CHECK(tdeck::spi_bus());
  tdeck::mount_sd(); // Must precede LCD traffic on the shared SPI bus.
  lv_init();
  const size_t pixels = tdeck::WIDTH * tdeck::DRAW_LINES;
  auto *a = (lv_color_t *)heap_caps_malloc(pixels * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  auto *b = (lv_color_t *)heap_caps_malloc(pixels * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  assert(a && b);
  lv_disp_draw_buf_init(&draw_buffer, a, b, pixels);
  ESP_ERROR_CHECK(tdeck::lcd(flush_done, &display_driver));
  lv_disp_drv_init(&display_driver);
  display_driver.hor_res = tdeck::WIDTH;
  display_driver.ver_res = tdeck::HEIGHT;
  display_driver.flush_cb = flush;
  display_driver.draw_buf = &draw_buffer;
  lv_disp_drv_register(&display_driver);

  tdeck::i2c();
  tdeck::trackball();
  tdeck::battery_init();
  static lv_indev_drv_t keypad, touch;
  lv_indev_drv_init(&keypad);
  keypad.type = LV_INDEV_TYPE_KEYPAD;
  keypad.read_cb = keypad_read;
  lv_indev_t *keypad_device = lv_indev_drv_register(&keypad);
  if (tdeck::touch_present) {
    lv_indev_drv_init(&touch);
    touch.type = LV_INDEV_TYPE_POINTER;
    touch.read_cb = touch_read;
    lv_indev_drv_register(&touch);
  }

  const esp_timer_create_args_t tick_args = {.callback = tick, .arg = nullptr, .dispatch_method = ESP_TIMER_TASK,
                                             .name = "lv_tick", .skip_unhandled_events = true};
  esp_timer_handle_t tick_timer = nullptr;
  ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 2000));

  lvgl_mutex = xSemaphoreCreateRecursiveMutex();
  xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY);
  tdui::init();
  lv_indev_set_group(keypad_device, lv_group_get_default());
  xSemaphoreGiveRecursive(lvgl_mutex);
  tdeck::backlight(true);
  xTaskCreatePinnedToCore(lvgl_task, "taskLVGL", 8192, nullptr, 4, nullptr, 1);

  // Same order as the Waveshare controller: controller memory first, then the
  // offline SD history restore, then resume only previously bonded radios.
  mcxmem::capture(2);
  meshble::prepare_controller_early();
  mcxmem::capture(3);
  if (xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
    mcxoffline::begin();
    xSemaphoreGiveRecursive(lvgl_mutex);
  }
  meshble::schedule_auto_resume();
  printf("[MCX-TDECK] ready: internal free %u, PSRAM free %u\n",
         unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
         unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
}
