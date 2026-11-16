# MeshCrossway Android 0.4.9 beta

The Meshtastic radio page now opens with **Current radio settings**: the values the
radio itself reports. It shows the tuned LoRa frequency and how it was chosen (automatic
slot from the primary channel name, explicit slot or frequency override), the preset
with bandwidth, spreading factor and coding rate, region, hop limit, transmit power,
primary channel, hardware and firmware. MeshCore shows a matching summary line.

The LoRa page shows what the radio is using now, and a **Tuned frequency** line next to
Frequency slot and Frequency override that updates while you edit. The frequency follows
the firmware's own slot arithmetic. With a default channel name, changing preset also
changes the frequency: in the US, LongFast uses 906.875 MHz and LongTurbo 908.750 MHz.

Saves that restart the radio are checked again after it reconnects. If the radio comes
back with different settings (for example another preset), the difference is reported.
Messages the firmware sends about adjusted settings are shown.

Home radio cards show only the Bluetooth symbol and signal bars, plus a small LoRa
frequency. The dBm reading remains available to screen readers.

Install over the current app **without uninstalling**. Version code 43, same permanent
signing certificate and data format. Field testing remains pending.
