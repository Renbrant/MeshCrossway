# v0.9.5 — BLE coordinator memory fix

The v0.9.4 device log shows the BLE controller now initializes before Wi-Fi, but automatic reconnect later tries to create a second 6 KB worker task. At that point the largest remaining internal-RAM block is only 4.6–4.8 KB, so task creation fails and Bluetooth cannot connect.

The persistent coordinator task now handles both manual pairing requests and automatic reconnect itself. It wakes through a FreeRTOS task notification and initializes NimBLE using its existing stack, avoiding a duplicate internal-RAM stack allocation. v0.8.26 remains the recommended stable release; this candidate still needs device validation.
