#pragma once
// LVGL thread owns Client; NimBLE callbacks only put complete frames into this bounded mailbox.
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "mbedtls/sha256.h"
#include "meshcrossway_meshcore_protocol.h"
#include "meshcrossway_sd_history.h"
#include <atomic>
#include <new>
namespace meshmc {
static Client *client = nullptr;
static Frame *mailbox = nullptr;
static portMUX_TYPE mailbox_mux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t mail_head = 0, mail_count = 0;
static std::atomic<bool> overflow{false}, link{false};
static std::atomic<uint16_t> negotiated_payload{0};
static std::atomic<uint32_t> connection_epoch{0};
static uint32_t seen_epoch = 0;
static std::atomic<int> write_result{0};
static bool (*send_frame)(const uint8_t *, size_t) = nullptr;
static void (*close_link)() = nullptr;
static bool channel_secret(const char *name, uint8_t out[16]) {
  if (name[0] == '#') {
    uint8_t digest[32];
    if (mbedtls_sha256((const unsigned char *)name, strlen(name), digest, 0))
      return false;
    memcpy(out, digest, 16);
    memset(digest, 0, sizeof(digest));
  } else
    esp_fill_random(out, 16);
  return true;
}
static bool init() {
  if (client)
    return true;
  void *memory = heap_caps_malloc(sizeof(Client), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  mailbox = (Frame *)heap_caps_calloc(16, sizeof(Frame), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!memory || !mailbox) {
    if (memory)
      heap_caps_free(memory);
    if (mailbox)
      heap_caps_free(mailbox);
    mailbox = nullptr;
    return false;
  }
  for (int i=0;i<16;i++)new(mailbox+i) Frame();
  client = new (memory) Client();
  return true;
}
static void transport_down() {
  link.store(false);
  write_result.store(0);
}
static void transport_up(uint16_t payload) {
  portENTER_CRITICAL(&mailbox_mux);
  mail_head = mail_count = 0;
  portEXIT_CRITICAL(&mailbox_mux);
  overflow.store(false);
  write_result.store(0);
  negotiated_payload.store(payload);
  connection_epoch.fetch_add(1);
  link.store(true);
}
static void on_write(bool ok) {
  write_result.store(ok ? 1 : -1);
}
static void on_frame(const uint8_t *p, size_t n) {
  if (!link.load() || !mailbox)
    return;
  if (!n || n > MAX_FRAME) {
    overflow.store(true);
    return;
  }
  portENTER_CRITICAL(&mailbox_mux);
  if (mail_count == 16) {
    overflow.store(true);
  } else {
    Frame &f = mailbox[(mail_head + mail_count) % 16];
    f.len = n;
    memcpy(f.data, p, n);
    ++mail_count;
  }
  portEXIT_CRITICAL(&mailbox_mux);
}
static void poll() {
  if (!client)
    return;
  const bool up = link.load();
  if (!up) {
    if (client->connected)
      client->detach();
    return;
  }
  if (!client->connected || seen_epoch != connection_epoch.load()) {
    mcxhistory::before_attach(*client,(uint32_t)(esp_timer_get_time()/1000));
    client->attach(negotiated_payload.load());
    seen_epoch = connection_epoch.load();
  }
  if (overflow.exchange(false)) {
    client->fail("Receive queue overflow; reconnect to synchronize");
    if (close_link)
      close_link();
    return;
  }
  int result = write_result.exchange(0);
  if (result)
    client->write_done(result == 1);
  const uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
  for (int k = 0; k < 4; k++) {
    Frame f;
    bool got = false;
    portENTER_CRITICAL(&mailbox_mux);
    if (mail_count) {
      f = mailbox[mail_head];
      mail_head = (mail_head + 1) % 16;
      --mail_count;
      got = true;
    }
    portEXIT_CRITICAL(&mailbox_mux);
    if (!got)
      break;
    const bool was_fault=client->fault;
    if (!client->receive(f.data, f.len, now) && !was_fault && client->fault) {
      // Diagnostic contains ONLY packet type and length, never payload,
      // public/private keys, BLE PIN, channel secrets or radio message text.
      printf("[MCX-MC-FRAME] invalid code=0x%02X len=%u; see status on screen\n",
             (unsigned)f.data[0],(unsigned)f.len);
    }
  }
  mcxhistory::poll(*client,now,seen_epoch);
  if (!mcxhistory::ready_for(*client)) return; // Restore before draining the radio backlog.
  Frame f;
  if (client->next(now, f) && (!send_frame || !send_frame(f.data, f.len)))
    client->write_done(false);
  if (client->fault && close_link)
    close_link();
}
} // namespace meshmc
