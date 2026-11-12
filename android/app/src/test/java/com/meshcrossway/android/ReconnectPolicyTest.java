package com.meshcrossway.android;

import org.junit.Test;
import static org.junit.Assert.*;

/** Regression: GATT failures must not create rapid duplicate BLE attempts. */
public class ReconnectPolicyTest {
    @Test public void status133UsesSlowBoundedBackoff(){
        assertEquals(6000L,ReconnectPolicy.delayMs(0,133));
        assertEquals(12000L,ReconnectPolicy.delayMs(1,133));
        assertEquals(24000L,ReconnectPolicy.delayMs(2,133));
        assertEquals(48000L,ReconnectPolicy.delayMs(3,133));
        assertEquals(60000L,ReconnectPolicy.delayMs(4,133));
        assertEquals(60000L,ReconnectPolicy.delayMs(15,133));
        assertEquals(6000L,ReconnectPolicy.delayMs(0,147));
    }

    @Test public void genericDisconnectStillUsesBoundedBackoff(){
        assertEquals(2000L,ReconnectPolicy.delayMs(0,0));
        assertEquals(4000L,ReconnectPolicy.delayMs(1,0));
        assertEquals(32000L,ReconnectPolicy.delayMs(4,0));
        assertEquals(60000L,ReconnectPolicy.delayMs(5,0));
        assertEquals(2000L,ReconnectPolicy.delayMs(-10,0));
    }

    @Test public void repeatedConnectSavedMustNotRestartGattOrPendingRetry(){
        assertTrue(ReconnectPolicy.canStartSaved(false,false,false));
        assertFalse(ReconnectPolicy.canStartSaved(false,true,false));
        assertFalse(ReconnectPolicy.canStartSaved(false,false,true));
        assertFalse(ReconnectPolicy.canStartSaved(true,true,false));
        assertFalse(ReconnectPolicy.canStartSaved(true,false,false));
        assertTrue(ReconnectPolicy.failure(133).contains("advertising"));
        assertTrue(ReconnectPolicy.failure(147).contains("timed out"));
    }
}
