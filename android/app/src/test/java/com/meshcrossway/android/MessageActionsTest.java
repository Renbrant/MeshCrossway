package com.meshcrossway.android;

import org.junit.Test;
import static org.junit.Assert.*;
import static com.meshcrossway.android.MeshData.*;

public class MessageActionsTest {
    private Message message(String sender, String body, boolean group, boolean outgoing) {
        Message m = new Message();
        m.sender = sender;
        m.body = body;
        m.group = group;
        m.outgoing = outgoing;
        return m;
    }

    @Test public void meshcoreGroupDoesNotRepeatItsOwnSenderPrefix() {
        Message m = message("Specter 6", "Specter 6: Thank you", true, false);
        assertEquals("Thank you", MessageActions.displayBody(RadioProfile.MESHCORE, m));
        assertEquals("Specter 6: Thank you", m.body); // the saved payload is not mutated
    }

    @Test public void otherTextAndNetworksRemainUntouched() {
        Message m = message("Specter 6", "A test: SNR looks good", true, false);
        assertEquals(m.body, MessageActions.displayBody(RadioProfile.MESHCORE, m));
        m.body = "Specter 6: Thank you";
        assertEquals(m.body, MessageActions.displayBody(RadioProfile.MESHTASTIC, m));
        m.outgoing = true;
        assertEquals(m.body, MessageActions.displayBody(RadioProfile.MESHCORE, m));
        m.outgoing = false;
        m.group = false;
        assertEquals(m.body, MessageActions.displayBody(RadioProfile.MESHCORE, m));
    }

    @Test public void noGuessingWhenSenderIsUnknown() {
        Message m = message("Channel participant", "Alice: test", true, false);
        assertEquals(m.body, MessageActions.displayBody(RadioProfile.MESHCORE, m));
        assertEquals("", MessageActions.mention("Channel participant"));
    }

    @Test public void mentionAndReceptionReportStayEditableAndRadioAccurate() {
        Message m = message("Specter 6", "Specter 6: Test", true, false);
        m.rx.snr = 12.25;
        m.rx.rssi = -55;
        m.rx.hops = 3;
        m.rx.width = 1;
        m.rx.observed = true;
        m.rx.path = "ba57df";
        assertEquals("@[Specter 6]", MessageActions.mention(m.sender));
        assertEquals("ACK @[Specter 6]: SNR 12.25 dB, RSSI -55 dBm, 3 hops, observed via BA, 57, DF",
                MessageActions.receptionReply(m));
        assertTrue(MessageActions.hasReception(m.rx));
    }

    @Test public void noSignalIsFabricated() {
        Message m = message("Unknown", "Test", true, false);
        assertFalse(MessageActions.hasReception(m.rx));
        assertEquals("ACK @[Unknown]: received (reception data unavailable)", MessageActions.receptionReply(m));
    }
}
