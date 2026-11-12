package com.meshcrossway.android;

/** Pure policy for single-radio, bounded Android BLE reconnects. */
final class ReconnectPolicy {
    static final int GATT_ERROR = 133;
    static final int CONNECTION_TIMEOUT = 147;
    static final int MAX_RETRIES = 5;

    private ReconnectPolicy() {}

    /** Attempt is zero-based. More conservative when GATT setup itself failed. */
    static long delayMs(int attempt, int status) {
        int bounded=Math.max(0,Math.min(MAX_RETRIES,attempt));
        long first=(status==GATT_ERROR||status==CONNECTION_TIMEOUT)?6000L:2000L;
        return Math.min(60000L,first<<bounded);
    }

    /** Do not tear down an active connection or override a scheduled backoff. */
    static boolean canStartSaved(boolean connected, boolean gattActive, boolean retryScheduled) {
        return !connected&&!gattActive&&!retryScheduled;
    }

    static String failure(int status) {
        if(status==GATT_ERROR)
            return "GATT 133: Bluetooth connection failed; check radio power, advertising and other connected apps";
        if(status==CONNECTION_TIMEOUT)
            return "GATT 147: Bluetooth connection timed out; check radio range and advertising";
        return "Disconnected (GATT "+status+")";
    }
}
