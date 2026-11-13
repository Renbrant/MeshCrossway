package com.meshcrossway.android;

import org.junit.Test;
import static org.junit.Assert.*;

/** Stage 1: LoRa packet metadata and a verified full path are different facts. */
public class RxProvenanceTest {
    @Test public void meshtasticRouteIsNotInvented() {
        RxInfo rx=new RxInfo();
        rx.kind="meshtastic";
        rx.hops=2;
        rx.rssi=-92;
        rx.snr=4.5;
        assertTrue(rx.provenance().contains("full route unavailable"));
        assertFalse(rx.observed);
        assertTrue(rx.summary().contains("RSSI -92 dBm"));
        rx.mqtt=true;
        assertTrue(rx.provenance().contains("RF reception not guaranteed"));
    }

    @Test public void meshcoreDirectRouteIsNotZeroHop() {
        RxInfo rx=RxInfo.meshcore(255);
        assertEquals(-1,rx.hops);
        assertTrue(rx.provenance().contains("travelled hops unavailable"));
        assertTrue(rx.summary().contains("hops unavailable"));
    }

    @Test public void onlyValidatedObservedFloodGetsObservedLabel() {
        RxInfo rx=RxInfo.meshcore(2);
        assertTrue(rx.provenance().contains("full route not observed"));
        rx.observed=true;
        rx.path="aabb";
        assertTrue(rx.provenance().contains("observed flood path"));
        RxInfo copy=new RxInfo(rx);
        assertEquals(rx.provenance(),copy.provenance());
    }

    @Test public void unknownMetadataStaysUnknown() {
        RxInfo rx=new RxInfo();
        assertEquals("Reception metadata unavailable",rx.provenance());
        assertTrue(rx.summary().contains("Hops unavailable"));
    }
}
