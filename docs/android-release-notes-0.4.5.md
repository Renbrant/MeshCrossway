# MeshCrossway Android 0.4.5 beta

Repeater configuration now opens directly from Contacts. After administrator login,
the app reads the available settings and fills the cards automatically.

- View name, position, LoRa parameters, TX power, repeating, announcement intervals,
  maximum flood hops and guest read-only access in one scrollable screen.
- Tap a card to edit its current value, using numeric keyboards, switches and labeled
  radio inputs. Ordinary edits need one Save action.
- After saving, the app reads that setting again and displays the value reported by
  the repeater, including a different or clamped value.
- Find console, password changes and reboot under Advanced options, with diagnostics
  in an expandable section.

Configuration reads run one at a time while this screen is visible, without periodic
polling or automatic retries. Initial login and an administrator password are required.
Actual options depend on repeater firmware. This interaction is awaiting field retest.

Install over the permanently signed 0.4.4 APK **without uninstalling** to retain app
history and settings. Version code 39; permanent signing certificate unchanged.
SHA256SUMS.txt and SIGNING-CERT-SHA256.txt accompany the APK.
