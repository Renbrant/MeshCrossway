package com.meshcrossway.android;

import org.junit.Test;
import static org.junit.Assert.*;

/** Stage 1: cached BLE signal is never misrepresented as a live RF measurement. */
public class BleSignalFreshnessTest {
    private static final long NOW=1_800_000_000_000L;

    @Test public void freshSampleFromActiveGattIsLive() {
        long at=NOW-20_000;
        assertTrue(BleSignal.live(true,true,at,NOW));
        assertTrue(BleSignal.label(-65,true,at,NOW).startsWith("BLE -65 dBm"));
        assertTrue(BleSignal.label(-65,true,at,NOW).contains("just now"));
    }

    @Test public void oldSampleRemainsHistoricalEvenIfGattIsReady() {
        long at=NOW-BleSignal.LIVE_MAX_AGE_MS-1;
        assertFalse(BleSignal.live(true,true,at,NOW));
        assertTrue(BleSignal.recent(at,NOW));
        assertTrue(BleSignal.label(-81,false,at,NOW).startsWith("Last BLE -81 dBm"));
        assertTrue(BleSignal.label(-81,false,at,NOW).contains("min ago"));
    }

    @Test public void disconnectedAndNewLinkSamplesCannotBeCurrent() {
        assertFalse(BleSignal.live(false,true,NOW-1000,NOW));
        assertFalse(BleSignal.live(true,false,NOW-1000,NOW));
        assertTrue(BleSignal.label(-55,false,NOW-1000,NOW).startsWith("Last BLE"));
    }

    @Test public void missingFutureExpiredOrInvalidReadingsAreNotCurrent() {
        assertFalse(BleSignal.live(true,true,0,NOW));
        assertFalse(BleSignal.live(true,true,NOW+1000,NOW));
        assertFalse(BleSignal.live(true,true,NOW-24L*60*60*1000-1,NOW));
        assertEquals("BLE signal unavailable",BleSignal.label(-70,false,0,NOW));
        assertEquals("BLE signal unavailable",BleSignal.label(-70,false,NOW+1,NOW));
        assertEquals("BLE signal unavailable",BleSignal.label(-70,false,NOW-BleSignal.MAX_AGE_MS-1,NOW));
        assertEquals("BLE signal unavailable",BleSignal.label(BleSignal.UNKNOWN,false,NOW-1000,NOW));
        assertEquals(0,BleSignal.bars(BleSignal.UNKNOWN));
    }

    @Test public void sampleAgeUsesElapsedTimeRatherThanPacketTime() {
        assertEquals("just now",BleSignal.age(NOW-59_000,NOW));
        assertEquals("1 min ago",BleSignal.age(NOW-60_000,NOW));
        assertEquals("2 h ago",BleSignal.age(NOW-2L*60*60*1000,NOW));
    }
}
