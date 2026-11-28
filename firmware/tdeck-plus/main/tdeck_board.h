#pragma once
// LilyGO T-Deck Plus board layer for the MeshCrossway controller.
// Pins: LilyGO T-Deck wiki (ESP32-S3FN16R8, 8 MB octal PSRAM, 16 MB flash).
// TFT, microSD and the onboard SX1262 share one SPI bus. This build leaves the
// SX1262 idle (CS held high); both mesh networks use external BLE radios.
#include <atomic>
#include <stdio.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdmmc_cmd.h"

namespace tdeck {
constexpr gpio_num_t POWER_ON = GPIO_NUM_10;
constexpr gpio_num_t I2C_SDA = GPIO_NUM_18, I2C_SCL = GPIO_NUM_8;
constexpr gpio_num_t SPI_MOSI = GPIO_NUM_41, SPI_MISO = GPIO_NUM_38, SPI_SCK = GPIO_NUM_40;
constexpr gpio_num_t TFT_CS = GPIO_NUM_12, TFT_DC = GPIO_NUM_11, TFT_BACKLIGHT = GPIO_NUM_42;
constexpr gpio_num_t SD_CS = GPIO_NUM_39, RADIO_CS = GPIO_NUM_9;
constexpr gpio_num_t KEYBOARD_INT = GPIO_NUM_46, TOUCH_INT = GPIO_NUM_16;
// Trackball pulse lines (Meshtastic T-Deck variant mapping) and BOOT as click.
constexpr gpio_num_t BALL_UP = GPIO_NUM_3, BALL_DOWN = GPIO_NUM_15, BALL_LEFT = GPIO_NUM_1,
                     BALL_RIGHT = GPIO_NUM_2, BALL_CLICK = GPIO_NUM_0;
constexpr adc_channel_t BATTERY_CHANNEL = ADC_CHANNEL_3; // GPIO4 on ADC1, 1:2 divider.
constexpr uint8_t KEYBOARD_ADDR = 0x55;
constexpr int WIDTH = 320, HEIGHT = 240;
constexpr spi_host_device_t SPI_HOST_ID = SPI2_HOST;
constexpr int DRAW_LINES = 15; // 2 x 9.6 KB internal DMA draw buffers.

// Panel orientation for landscape with the keyboard below the screen.
// Adjust here if the first hardware test shows a mirrored or inverted image.
constexpr bool PANEL_SWAP_XY = true, PANEL_MIRROR_X = true, PANEL_MIRROR_Y = false,
               PANEL_INVERT = true;
constexpr bool TOUCH_SWAP_XY = true, TOUCH_MIRROR_X = false, TOUCH_MIRROR_Y = true;

static esp_lcd_panel_io_handle_t lcd_io = nullptr;
static esp_lcd_panel_handle_t panel = nullptr;
static i2c_master_bus_handle_t i2c_bus = nullptr;
static i2c_master_dev_handle_t keyboard_dev = nullptr, touch_dev = nullptr;
static adc_oneshot_unit_handle_t adc = nullptr;
static adc_cali_handle_t adc_cali = nullptr;
static sdmmc_card_t *card = nullptr;
static bool sd_mounted = false, keyboard_present = false, touch_present = false;

static void power_on() {
  gpio_config_t out = {};
  out.pin_bit_mask = (1ULL << POWER_ON) | (1ULL << TFT_CS) | (1ULL << SD_CS) | (1ULL << RADIO_CS) |
                     (1ULL << TFT_BACKLIGHT);
  out.mode = GPIO_MODE_OUTPUT;
  gpio_config(&out);
  // Keep every SPI chip select inactive before the bus carries any clock.
  gpio_set_level(TFT_CS, 1);
  gpio_set_level(SD_CS, 1);
  gpio_set_level(RADIO_CS, 1);
  gpio_set_level(TFT_BACKLIGHT, 0);
  gpio_set_level(POWER_ON, 1); // Peripheral rail: keyboard, display and GNSS on battery.
  vTaskDelay(pdMS_TO_TICKS(200)); // Keyboard ESP32-C3 needs time to boot.
}

static esp_err_t spi_bus() {
  spi_bus_config_t bus = {};
  bus.mosi_io_num = SPI_MOSI;
  bus.miso_io_num = SPI_MISO;
  bus.sclk_io_num = SPI_SCK;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = WIDTH * DRAW_LINES * 2 + 8;
  return spi_bus_initialize(SPI_HOST_ID, &bus, SPI_DMA_CH_AUTO);
}

// The SD card must enter SPI mode before other devices talk on the shared bus.
static void mount_sd() {
  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = SPI_HOST_ID;
  host.max_freq_khz = 20000;
  sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot.gpio_cs = SD_CS;
  slot.host_id = SPI_HOST_ID;
  esp_vfs_fat_sdmmc_mount_config_t config = {};
  config.format_if_mount_failed = false; // Never format a user's card.
  config.max_files = 8;
  config.allocation_unit_size = 16 * 1024;
  const esp_err_t rc = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot, &config, &card);
  sd_mounted = rc == ESP_OK;
  printf("[MCX-TDECK] microSD %s (%s)\n", sd_mounted ? "mounted" : "unavailable", esp_err_to_name(rc));
}

static esp_err_t lcd(esp_lcd_panel_io_color_trans_done_cb_t done, void *context) {
  esp_lcd_panel_io_spi_config_t io = {};
  io.dc_gpio_num = TFT_DC;
  io.cs_gpio_num = TFT_CS;
  io.pclk_hz = 40 * 1000 * 1000;
  io.lcd_cmd_bits = 8;
  io.lcd_param_bits = 8;
  io.spi_mode = 0;
  io.trans_queue_depth = 6;
  io.on_color_trans_done = done;
  io.user_ctx = context;
  esp_err_t rc = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI_HOST_ID, &io, &lcd_io);
  if (rc != ESP_OK) return rc;
  esp_lcd_panel_dev_config_t dev = {};
  dev.reset_gpio_num = -1;
  dev.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
  dev.bits_per_pixel = 16;
  rc = esp_lcd_new_panel_st7789(lcd_io, &dev, &panel);
  if (rc != ESP_OK) return rc;
  esp_lcd_panel_reset(panel);
  esp_lcd_panel_init(panel);
  esp_lcd_panel_invert_color(panel, PANEL_INVERT);
  esp_lcd_panel_swap_xy(panel, PANEL_SWAP_XY);
  esp_lcd_panel_mirror(panel, PANEL_MIRROR_X, PANEL_MIRROR_Y);
  esp_lcd_panel_disp_on_off(panel, true);
  return ESP_OK;
}

static void backlight(bool on) { gpio_set_level(TFT_BACKLIGHT, on ? 1 : 0); }

static void i2c() {
  i2c_master_bus_config_t bus = {};
  bus.i2c_port = I2C_NUM_0;
  bus.sda_io_num = I2C_SDA;
  bus.scl_io_num = I2C_SCL;
  bus.clk_source = I2C_CLK_SRC_DEFAULT;
  bus.glitch_ignore_cnt = 7;
  bus.flags.enable_internal_pullup = true;
  if (i2c_new_master_bus(&bus, &i2c_bus) != ESP_OK) {
    printf("[MCX-TDECK] I2C bus unavailable\n");
    return;
  }
  auto add = [](uint16_t address, i2c_master_dev_handle_t *out) {
    if (i2c_master_probe(i2c_bus, address, 50) != ESP_OK) return false;
    i2c_device_config_t dev = {};
    dev.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev.device_address = address;
    dev.scl_speed_hz = 400000;
    return i2c_master_bus_add_device(i2c_bus, &dev, out) == ESP_OK;
  };
  keyboard_present = add(KEYBOARD_ADDR, &keyboard_dev);
  touch_present = add(0x5D, &touch_dev) || add(0x14, &touch_dev); // GT911 strap options.
  printf("[MCX-TDECK] keyboard %s, GT911 touch %s\n", keyboard_present ? "found" : "missing",
         touch_present ? "found" : "missing");
}

// Returns an ASCII key from the keyboard controller, or 0 when none is waiting.
static char read_key() {
  if (!keyboard_present) return 0;
  uint8_t key = 0;
  if (i2c_master_receive(keyboard_dev, &key, 1, 20) != ESP_OK) return 0;
  return char(key);
}

static bool read_touch(int16_t &x, int16_t &y) {
  if (!touch_present) return false;
  uint8_t reg[2] = {0x81, 0x4E}, status = 0;
  if (i2c_master_transmit_receive(touch_dev, reg, 2, &status, 1, 20) != ESP_OK) return false;
  if (!(status & 0x80)) return false;
  bool pressed = false;
  if (status & 0x0F) {
    uint8_t point_reg[2] = {0x81, 0x50}, p[4] = {};
    if (i2c_master_transmit_receive(touch_dev, point_reg, 2, p, 4, 20) == ESP_OK) {
      int raw_x = p[0] | (p[1] << 8), raw_y = p[2] | (p[3] << 8);
      int tx = TOUCH_SWAP_XY ? raw_y : raw_x, ty = TOUCH_SWAP_XY ? raw_x : raw_y;
      if (TOUCH_MIRROR_X) tx = WIDTH - 1 - tx;
      if (TOUCH_MIRROR_Y) ty = HEIGHT - 1 - ty;
      x = int16_t(tx < 0 ? 0 : tx >= WIDTH ? WIDTH - 1 : tx);
      y = int16_t(ty < 0 ? 0 : ty >= HEIGHT ? HEIGHT - 1 : ty);
      pressed = true;
    }
  }
  uint8_t clear[3] = {0x81, 0x4E, 0x00};
  i2c_master_transmit(touch_dev, clear, 3, 20);
  return pressed;
}

// Trackball: each roll step pulses a line. ISRs only count; LVGL consumes.
static std::atomic<int> ball_vertical{0}, ball_horizontal{0};
static void IRAM_ATTR ball_isr(void *arg) {
  switch ((intptr_t)arg) {
  case 0: ball_vertical.fetch_sub(1); break;
  case 1: ball_vertical.fetch_add(1); break;
  case 2: ball_horizontal.fetch_sub(1); break;
  default: ball_horizontal.fetch_add(1); break;
  }
}
static void trackball() {
  gpio_config_t in = {};
  in.pin_bit_mask = (1ULL << BALL_UP) | (1ULL << BALL_DOWN) | (1ULL << BALL_LEFT) | (1ULL << BALL_RIGHT);
  in.mode = GPIO_MODE_INPUT;
  in.pull_up_en = GPIO_PULLUP_ENABLE;
  in.intr_type = GPIO_INTR_NEGEDGE;
  gpio_config(&in);
  gpio_config_t click = {};
  click.pin_bit_mask = 1ULL << BALL_CLICK;
  click.mode = GPIO_MODE_INPUT;
  click.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&click);
  if (gpio_install_isr_service(0) != ESP_OK) return;
  gpio_isr_handler_add(BALL_UP, ball_isr, (void *)0);
  gpio_isr_handler_add(BALL_DOWN, ball_isr, (void *)1);
  gpio_isr_handler_add(BALL_LEFT, ball_isr, (void *)2);
  gpio_isr_handler_add(BALL_RIGHT, ball_isr, (void *)3);
}
static bool ball_pressed() { return gpio_get_level(BALL_CLICK) == 0; }

static void battery_init() {
  adc_oneshot_unit_init_cfg_t unit = {};
  unit.unit_id = ADC_UNIT_1;
  if (adc_oneshot_new_unit(&unit, &adc) != ESP_OK) return;
  adc_oneshot_chan_cfg_t channel = {};
  channel.atten = ADC_ATTEN_DB_12;
  channel.bitwidth = ADC_BITWIDTH_DEFAULT;
  adc_oneshot_config_channel(adc, BATTERY_CHANNEL, &channel);
  adc_cali_curve_fitting_config_t cali = {};
  cali.unit_id = ADC_UNIT_1;
  cali.chan = BATTERY_CHANNEL;
  cali.atten = ADC_ATTEN_DB_12;
  cali.bitwidth = ADC_BITWIDTH_DEFAULT;
  if (adc_cali_create_scheme_curve_fitting(&cali, &adc_cali) != ESP_OK) adc_cali = nullptr;
}
// Battery voltage in millivolts, or 0 when unavailable.
static int battery_mv() {
  if (!adc || !adc_cali) return 0;
  int raw = 0, mv = 0;
  if (adc_oneshot_read(adc, BATTERY_CHANNEL, &raw) != ESP_OK) return 0;
  if (adc_cali_raw_to_voltage(adc_cali, raw, &mv) != ESP_OK) return 0;
  return mv * 2;
}
static int battery_percent(int mv) {
  if (mv <= 0) return -1;
  const int pct = (mv - 3300) * 100 / (4150 - 3300);
  return pct < 0 ? 0 : pct > 100 ? 100 : pct;
}
} // namespace tdeck
