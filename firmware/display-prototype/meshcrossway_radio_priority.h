#pragma once
#include <atomic>
namespace mcxpriority {
// No LVGL or transport calls across tasks. Map HTTP yields before BLE init.
static std::atomic<bool> ble_initializing{false},map_busy{false};
}
