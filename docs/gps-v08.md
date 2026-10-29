# v0.8.0 local UART GNSS

Waveshare ESP32-S3 Touch LCD 3.5 Rev2.0 non-B, MAX-M10S breakout.
The supplied diagram identifies RXD GPIO44 (header pin 25) and TXD GPIO43
(header pin 27). This implementation assumes those are the connected terminals:
GPS TX -> Waveshare RX44; GPS RX -> Waveshare TX43; shared ground.
The diagram is a pinout, not photographic confirmation of the physical wires.

UART1 is routed through the GPIO matrix, leaving UART0 unused by the application.
Console output is USB Serial/JTAG, avoiding application console traffic on the GPS
pins. ROM/download activity can still occur before application startup. No GPS
configuration or flash writes are sent. Preserve the receiver configuration.

A low-priority core-0 task (4096-byte internal stack, 2048-byte UART RX ring)
reads bounded chunks with a timeout. LVGL only consumes a locked snapshot once
per second. It never reads UART synchronously. Baud detection alternates 9600
and 38400 every five seconds without valid NMEA; it locks to valid sentences even
without a position fix. No valid position is invented at boot. Positions expire
after three seconds, and GGA quality zero/RMC V immediately invalidates the fix.

Home, Map and GNSS Status show local coordinates, fix state, satellites used and
HDOP when provided. HDOP is dimensionless, not measured accuracy in metres.
The map background remains a demonstration; real georeferenced map rendering,
UBX-NAV-PVT horizontal accuracy, sky view, track recording, UTC clock discipline,
POI persistence and location sharing are outside this first UART implementation.
MeshCore and Meshtastic remain independent and receive no automatic GPS packets.

Host parser tests cover checksums, GGA/RMC, coordinates, speed, expiry, overflow
and resynchronization; preflight compiles them. Physical receiver, pin wiring,
console/USB coexistence, UART noise, cold start and BLE/touch responsiveness need
hardware validation before this is considered stable. v0.7.8 stays recovery.
Install without Erase to preserve NVS and SD history.

References: u-blox MAX-M10S DataSheet UBX-20035208 and IntegrationManual
UBX-20053088, and ESP-IDF ESP32-S3 USB Serial/JTAG Console documentation.
