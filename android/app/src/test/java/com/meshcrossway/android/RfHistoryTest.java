package com.meshcrossway.android;

import org.junit.Test;
import static org.junit.Assert.*;
import static com.meshcrossway.android.MeshData.*;

/** Stage 1: only cryptographically or protocol-identified senders get RF history. */
public class RfHistoryTest {
    private static Message incoming(String uid){
        Message m=new Message();m.uid=uid;m.timestamp=1700000100;m.body="test";
        return m;
    }
    private static String meshCoreKey(){return "a1".repeat(32);}

    @Test public void meshtasticGroupUsesNativeSourceNotSenderName(){
        Message m=incoming("mt:123:1");
        m.group=true;m.sender="Someone else";m.rx.kind="meshtastic";
        m.rx.source=123;m.rx.rssi=-94;m.rx.snr=5.25;m.rx.hops=2;
        RfHistory.Sample sample=RfHistory.from(RadioProfile.MESHTASTIC,"123",m);
        assertNotNull(sample);
        assertEquals(-94,sample.rssi);assertEquals(5.25,sample.snr,0.001);
        assertEquals("123",sample.nodeId);
        assertNull(RfHistory.from(RadioProfile.MESHTASTIC,"456",m));
        m.rx.source=0;
        assertNull(RfHistory.from(RadioProfile.MESHTASTIC,"123",m));
    }

    @Test public void meshcoreDirectRequiresFullUnambiguousPublicKey(){
        Message m=incoming("mc:abc");
        m.peer=meshCoreKey();m.group=false;m.rx.kind="direct";
        m.rx.snr=-4.5;
        RfHistory.Sample sample=RfHistory.from(RadioProfile.MESHCORE,meshCoreKey(),m);
        assertNotNull(sample);
        assertTrue(sample.hasSnr());
        assertFalse(sample.hasHops()); // direct routing does not mean zero hops
        m.peer=meshCoreKey().substring(0,12);
        assertNull(RfHistory.from(RadioProfile.MESHCORE,meshCoreKey(),m));
    }

    @Test public void meshcoreGroupNameIsNotAStableIdentity(){
        Message m=incoming("mc:group");
        m.peer=meshCoreKey();m.group=true;m.sender="Alice";
        m.rx=RxInfo.meshcore(2);m.rx.snr=8.25;
        assertNull(RfHistory.from(RadioProfile.MESHCORE,meshCoreKey(),m));
    }

    @Test public void missingUnreliableAndOutgoingMetadataAreExcluded(){
        Message m=incoming("mt:5:4");m.rx.source=5;
        assertNull(RfHistory.from(RadioProfile.MESHTASTIC,"5",m));
        m.rx.rssi=400;
        assertNull(RfHistory.from(RadioProfile.MESHTASTIC,"5",m));
        m.rx.snr=Double.NaN;m.rx.rssi=-110;
        assertNotNull(RfHistory.from(RadioProfile.MESHTASTIC,"5",m));
        m.outgoing=true;
        assertNull(RfHistory.from(RadioProfile.MESHTASTIC,"5",m));
    }

    @Test public void sourceAndMetricTagsKeepHistoricalCaveats(){
        Message m=incoming("mt:8:7");
        m.rx.kind="meshtastic";m.rx.source=8;m.rx.rssi=-99;m.rx.mqtt=true;
        RfHistory.Sample sample=RfHistory.from(RadioProfile.MESHTASTIC,"8",m);
        assertNotNull(sample);assertTrue(sample.mqtt);
        assertTrue(sample.provenance.contains("RF reception not guaranteed"));
        assertEquals("RSSI -99 dBm",sample.metrics());
        assertEquals(1700000100,sample.messageTime);
    }

    @Test public void identityCannotLeakAcrossNetworks(){
        Message mt=incoming("shared-id");mt.rx.kind="meshtastic";
        mt.rx.source=42;mt.rx.hops=3;
        assertNotNull(RfHistory.from(RadioProfile.MESHTASTIC,"42",mt));
        assertNull(RfHistory.from(RadioProfile.MESHCORE,"42",mt));
        assertNull(RfHistory.from(RadioProfile.MESHTASTIC,"43",mt));
    }
}
