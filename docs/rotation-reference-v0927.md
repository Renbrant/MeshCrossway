# v0.9.27: manual correction updates the automatic orientation reference

Hardware feedback on v0.9.26 reported that automatic rotation kept the display upside down in either physical landscape orientation. The original first-use reference could be learned with the image inverted. A confirmed defect then prevented correcting this through the existing manual control: Flip 180 changed the display, but retained the old automatic reference, so re-enabling AUTO could undo the readable choice.

This candidate makes a successfully applied manual inversion relearn the reference from stable gravity with the manually selected image. It saves that correction in the existing NVS orientation key. Both possible sensor axes and polarities are supported. No blanket inversion of existing calibrations is applied, because correctly calibrated installations must retain their reference. The v0.9.25 stable image remains unchanged.

## Correct an inverted reference

1. Open the top-bar quick panel and use **FLIP 180** while holding the device in its normal operating position, so the image becomes readable. This also locks rotation.
2. Hold that readable position, hands off, for 1–2 seconds, then tap **ROTATION LOCKED** to enable **AUTO ROTATE**.
3. Turn the hardware to the opposite landscape orientation, hands off. After the debounce interval the image should remain readable. Return to the original orientation.
4. Reboot and repeat. Check four-corner touches, keyboard and map dragging in each orientation.

Reference recalibration starts only after the LCD mirror operation succeeds, under the LVGL lock and without an active gesture or flush. Sensing continues briefly while locked until stable gravity is learned. Cancelled/double manual requests do not silently reverse the reference. A missing or invalid gravity sample leaves calibration pending and keeps the manually selected image. The existing worker, debounce, cooldown, fixed-size preferences and buffers are retained. Console confirmation: `[MCX-ROTATE] manual reference corrected normal=...`.

Host regression tests reproduce a reversed reference for both axes, both gravity polarities and both initial display states: after a manual correction, AUTO must preserve the readable choice and rotate at the opposite physical pose, including after movement while locked. Physical validation remains required; the reference issue is not declared hardware-fixed solely from host tests.
