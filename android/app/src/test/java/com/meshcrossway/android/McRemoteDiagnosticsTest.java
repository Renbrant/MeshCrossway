package com.meshcrossway.android;

import org.junit.Test;
import java.util.*;
import static org.junit.Assert.*;
import static com.meshcrossway.android.MeshData.*;

public class McRemoteDiagnosticsTest {
    private static final long NOW=500_000L;
    static Node repeater(String key){
        Node n=new Node();n.id=key;n.type=2;n.mcState=ProtocolClient.MC_APPROVED;return n;
    }
    static String key(char digit){
        char[] s=new char[64];Arrays.fill(s,digit);return new String(s);
    }
    static byte[] ack(long tag){
        byte[] f=new byte[10];f[0]=6;Wire.put32(f,2,tag);Wire.put32(f,6,5000);return f;
    }
    static byte[] response(long tag,byte[] payload){
        byte[] f=new byte[6+payload.length];f[0]=(byte)0x8c;
        Wire.put32(f,2,tag);System.arraycopy(payload,0,f,6,payload.length);return f;
    }

    @Test public void statusOnlyChangesAfterMatchingRemoteResponse(){
        McRemoteDiagnostics remote=new McRemoteDiagnostics();
        Node n=repeater(key('a'));
        byte[] request=remote.request(McRemoteDiagnostics.Kind.STATUS,n,0,NOW);
        assertEquals(34,request.length);
        assertEquals(50,request[0]&255);assertEquals(1,request[33]&255);
        assertEquals("Queued for local radio",remote.find(n.id).state);
        assertFalse(remote.receive(response(123,new byte[52]),NOW+1000));
        remote.sentToRadio(ack(123),NOW+1000);
        assertEquals("Awaiting remote repeater response",remote.find(n.id).state);
        assertFalse(remote.receive(response(321,new byte[52]),NOW+1500));
        assertTrue(remote.receive(response(123,new byte[52]),NOW+2000));
        assertNotNull(remote.find(n.id).stats);
        assertTrue(remote.find(n.id).state.startsWith("Received"));
        assertNull(remote.pending);
        assertFalse(remote.receive(response(123,new byte[52]),NOW+2200)); // duplicate
    }

    @Test public void statusParserReadsReal52And56ByteLayouts(){
        byte[] data=new byte[56];
        data[0]=(byte)0xe4;data[1]=0x0c; // 3300 mV
        data[2]=3;data[4]=(byte)0xa4;data[5]=(byte)0xff; // -92
        data[6]=(byte)0xba;data[7]=(byte)0xff; // -70
        Wire.put32(data,8,123);Wire.put32(data,12,456);
        Wire.put32(data,16,20);Wire.put32(data,20,3600);
        Wire.put32(data,48,45);Wire.put32(data,52,2);
        data[42]=(byte)0xfd;data[43]=(byte)0xff; // -3 qdB
        McRemoteDiagnostics.Status stats=new McRemoteDiagnostics.Status(data);
        assertEquals(3300,stats.batteryMv);
        assertEquals(3,stats.queue);
        assertEquals(-92,stats.noiseFloor);
        assertEquals(-70,stats.lastRssi);
        assertEquals(-0.75,stats.lastSnr,0.001);
        assertEquals(123L,stats.received);
        assertEquals(456L,stats.sent);
        assertEquals(3600L,stats.uptime);
        assertEquals(45L,stats.rxAirtime);
        assertTrue(stats.errorsKnown);assertEquals(2,stats.errors);
        McRemoteDiagnostics.Status legacy=new McRemoteDiagnostics.Status(Arrays.copyOf(data,52));
        assertFalse(legacy.errorsKnown);
        try{new McRemoteDiagnostics.Status(new byte[50]);fail("Accepts truncated frame");}
        catch(IllegalArgumentException expected){}
    }

    @Test public void neighborsRequestAndResponseAreBoundedAndNeverFullIdentities(){
        McRemoteDiagnostics remote=new McRemoteDiagnostics();Node n=repeater(key('b'));
        byte[] request=remote.request(McRemoteDiagnostics.Kind.NEIGHBORS,n,8,NOW);
        assertEquals(44,request.length);
        assertEquals(6,request[33]&255); // req_type neighbors
        assertEquals(0,request[34]&255); // version0
        assertEquals(8,request[35]&255); // max 8 results
        assertEquals(8,request[36]&255); // offset 8
        assertEquals(6,request[39]&255); // prefix bytes
        remote.sentToRadio(ack(0x12345678L),NOW+100);
        byte[] d=new byte[4+11];d[0]=25;d[2]=1;
        byte[] prefix=Wire.unhex("abcdef012345");
        System.arraycopy(prefix,0,d,4,prefix.length);
        Wire.put32(d,10,120);d[14]=(byte)0xf8; // SNR -2.0
        assertTrue(remote.receive(response(0x12345678L,d),NOW+400));
        McRemoteDiagnostics.Neighbors found=remote.find(n.id).neighbors;
        assertEquals(25,found.total);assertEquals(8,found.offset);
        assertEquals(1,found.list.size());
        assertEquals("abcdef012345",found.list.get(0).prefix);
        assertEquals(120L,found.list.get(0).secondsAgo);
        assertEquals(-2.0,found.list.get(0).snr,0.001);
    }

    @Test public void rejectAmbiguousMalformedAndOutOfScopeRemoteData(){
        McRemoteDiagnostics r=new McRemoteDiagnostics();
        Node n=repeater(key('c'));
        r.request(McRemoteDiagnostics.Kind.STATUS,n,0,NOW);
        try{r.request(McRemoteDiagnostics.Kind.STATUS,n,0,NOW+1);fail("accepted overlapping request");}
        catch(IllegalArgumentException expected){}
        r.sentToRadio(ack(22),NOW+10);
        assertFalse(r.receive(response(21,new byte[52]),NOW+30));
        assertTrue(r.receive(response(22,new byte[5]),NOW+35));
        assertEquals("Error",r.find(n.id).state);
        try{r.request(McRemoteDiagnostics.Kind.STATUS,n,0,NOW+1000);fail("ignored cooldown");}
        catch(IllegalArgumentException expected){}
        byte[] bad=new byte[4+11];bad[2]=9;
        try{McRemoteDiagnostics.parseNeighbors(bad,0);fail("Accepted out-of-bounds count");}
        catch(IllegalArgumentException expected){}
        bad=new byte[4+11];bad[0]=1;bad[2]=1;
        assertEquals(1,McRemoteDiagnostics.parseNeighbors(bad,0).list.size());
        bad[0]=0;
        try{McRemoteDiagnostics.parseNeighbors(bad,0);fail("Count > total");}
        catch(IllegalArgumentException expected){}
    }

    @Test public void everyStatusAndNeighborPageSharesThirtySecondRepeaterCooldown(){
        McRemoteDiagnostics remote=new McRemoteDiagnostics();
        Node repeater=repeater(key('a'));
        assertEquals(30_000L,McRemoteDiagnostics.COOLDOWN_MS);
        remote.request(McRemoteDiagnostics.Kind.STATUS,repeater,0,NOW);
        remote.sentToRadio(ack(441),NOW+10);
        assertTrue(remote.receive(response(441,new byte[52]),NOW+20));
        assertEquals(30_000L,remote.remainingCooldownMs(repeater.id,NOW));
        assertEquals(1L,remote.remainingCooldownMs(repeater.id,NOW+29_999));
        try{
            remote.request(McRemoteDiagnostics.Kind.NEIGHBORS,repeater,0,NOW+29_999);
            fail("Neighbor request bypassed status cooldown");
        }catch(IllegalArgumentException expected){}
        byte[] firstPage=remote.request(McRemoteDiagnostics.Kind.NEIGHBORS,repeater,0,NOW+30_000);
        assertEquals(44,firstPage.length);
        remote.sentToRadio(ack(442),NOW+30_010);
        assertTrue(remote.receive(response(442,new byte[4]),NOW+30_020));
        try{
            remote.request(McRemoteDiagnostics.Kind.NEIGHBORS,repeater,8,NOW+40_000);
            fail("Next neighbor page bypassed shared cooldown");
        }catch(IllegalArgumentException expected){}
        try{
            remote.request(McRemoteDiagnostics.Kind.STATUS,repeater,0,NOW+40_000);
            fail("Status bypassed neighbor-page cooldown");
        }catch(IllegalArgumentException expected){}
        assertEquals(0L,remote.remainingCooldownMs(repeater.id,NOW+60_000));
        assertEquals(44,remote.request(McRemoteDiagnostics.Kind.NEIGHBORS,
            repeater,8,NOW+60_000).length);
    }

    @Test public void floodFlagIsFromLocalRadioAcknowledgementOnly(){
        McRemoteDiagnostics remote=new McRemoteDiagnostics();
        Node repeater=repeater(key('f'));
        remote.request(McRemoteDiagnostics.Kind.STATUS,repeater,0,NOW);
        McRemoteDiagnostics.Result item=remote.find(repeater.id);
        assertNull(item.usedFlood); // Cannot know path before radio replies.
        byte[] ack=ack(443);
        ack[1]=1; // MSG_SEND_SENT_FLOOD reported by Companion
        remote.sentToRadio(ack,NOW+1);
        assertEquals(Boolean.TRUE,item.usedFlood);
        assertTrue(item.radioRoute().contains("flood"));
        assertTrue(item.radioRoute().contains("relays"));
        McRemoteDiagnostics second=new McRemoteDiagnostics();
        second.request(McRemoteDiagnostics.Kind.STATUS,repeater,0,NOW);
        second.sentToRadio(ack(444),NOW+1);
        assertEquals(Boolean.FALSE,second.find(repeater.id).usedFlood);
    }

    @Test public void thirtySecondWallClockCooldownSurvivesReconnectAndAppRestart(){
        long now=1_800_000_000_000L;
        assertEquals(0,McRemoteDiagnostics.remainingFromWallClock(0,now));
        assertEquals(30_000L,McRemoteDiagnostics.remainingFromWallClock(now,now));
        assertEquals(1L,McRemoteDiagnostics.remainingFromWallClock(now,now+29_999));
        assertEquals(0L,McRemoteDiagnostics.remainingFromWallClock(now,now+30_000));
        assertEquals(30_000L,McRemoteDiagnostics.remainingFromWallClock(now,now-1000));
        assertTrue(McRemoteDiagnostics.remainingLabel(1).contains("1 second"));
        assertTrue(McRemoteDiagnostics.remainingLabel(30_000).contains("30 seconds"));
        assertTrue(McRemoteDiagnostics.remainingLabel(1_234).contains("2 seconds"));
    }

    @Test public void unsupportedRemoteStatusShowsOnlySafeLengthNotRawPayload(){
        McRemoteDiagnostics remote=new McRemoteDiagnostics();
        Node n=repeater(key('c'));
        remote.request(McRemoteDiagnostics.Kind.STATUS,n,0,NOW);
        remote.sentToRadio(ack(0x55aaL),NOW+10);
        byte[] unusual=new byte[48];
        Arrays.fill(unusual,(byte)0xfe);
        assertTrue(remote.receive(response(0x55aaL,unusual),NOW+50));
        McRemoteDiagnostics.Result result=remote.find(n.id);
        assertEquals("Error",result.state);
        assertTrue(result.error.contains("48 data bytes"));
        assertTrue(result.error.contains("requires at least 52"));
        assertFalse(result.error.contains("fefefe")); // no raw packet or public key
        assertNull(result.stats);
        assertNull(remote.pending);
        assertEquals(29_950,remote.remainingCooldownMs(n.id,NOW+50));
    }

    @Test public void unexpectedNeighborsDataIsNotMisrepresentedAsValid(){
        McRemoteDiagnostics remote=new McRemoteDiagnostics();
        Node n=repeater(key('e'));
        remote.request(McRemoteDiagnostics.Kind.NEIGHBORS,n,0,NOW);
        remote.sentToRadio(ack(35),NOW+20);
        byte[] notAList=new byte[7];
        Arrays.fill(notAList,(byte)0x7f);
        assertTrue(remote.receive(response(35,notAList),NOW+60));
        assertTrue(remote.find(n.id).error.contains("7 data bytes"));
        assertNull(remote.find(n.id).neighbors);
    }

    @Test public void upstreamCompatibleStatusAcceptsRealSixtyByteExtendedReply(){
        // MeshCore official open Python/JS clients parse known fields by offset
        // and ignore optional tail data. The 0.4.2 field report had 60 data bytes.
        byte[] payload=new byte[60];
        payload[0]=(byte)0xE4;payload[1]=0x0c; // 3300 mV
        Wire.put32(payload,8,24);
        Wire.put32(payload,12,100);
        Wire.put32(payload,20,3600);
        Wire.put32(payload,48,90);
        Wire.put32(payload,52,8); // known receive-error extension
        payload[56]=(byte)0xf1;payload[57]=(byte)0xe2;
        payload[58]=(byte)0xd3;payload[59]=(byte)0xc4; // unknown, never interpreted
        McRemoteDiagnostics.Status stats=new McRemoteDiagnostics.Status(payload);
        assertEquals(3300,stats.batteryMv);
        assertEquals(24L,stats.received);
        assertEquals(100L,stats.sent);
        assertEquals(3600L,stats.uptime);
        assertTrue(stats.errorsKnown);
        assertEquals(8L,stats.errors);
        assertEquals(4,stats.extensionBytes);

        McRemoteDiagnostics remote=new McRemoteDiagnostics();
        Node repeater=repeater(key('a'));
        remote.request(McRemoteDiagnostics.Kind.STATUS,repeater,0,NOW);
        remote.sentToRadio(ack(555),NOW+1);
        assertTrue(remote.receive(response(555,payload),NOW+3));
        assertNotNull(remote.find(repeater.id).stats);
        assertEquals(4,remote.find(repeater.id).stats.extensionBytes);
        assertTrue(remote.find(repeater.id).state.startsWith("Received"));

        // Minimum known fields are still mandatory, and huge payloads rejected.
        for(int len:new int[]{0,4,48,51,171}){
            try{new McRemoteDiagnostics.Status(new byte[len]);fail("accepted invalid "+len);}
            catch(IllegalArgumentException expected){}
        }
        assertFalse(new McRemoteDiagnostics.Status(new byte[52]).errorsKnown);
        assertTrue(new McRemoteDiagnostics.Status(new byte[56]).errorsKnown);
    }

    @Test public void upstreamCountedNeighborsAcceptSixtyByteFinalPageWithOneExtraByte(){
        // 13 reported total, first page 8, final page 5 => 4+5*11=59 bytes.
        byte[] payload=new byte[60];
        payload[0]=13;payload[2]=5;
        for(int i=0;i<5;i++){
            int at=4+i*11;
            for(int j=0;j<6;j++)payload[at+j]=(byte)(i*6+j+1);
            Wire.put32(payload,at+6,1000+i);
            payload[at+10]=(byte)(-4*i);
        }
        payload[59]=(byte)0xA5; // observed extra byte, not an extra entry
        McRemoteDiagnostics.Neighbors neighbors=McRemoteDiagnostics.parseNeighbors(payload,8);
        assertEquals(13,neighbors.total);
        assertEquals(8,neighbors.offset);
        assertEquals(5,neighbors.list.size());
        assertEquals(1,neighbors.extensionBytes);
        assertEquals(1004,neighbors.list.get(4).secondsAgo);
        assertEquals(-4.0,neighbors.list.get(4).snr,0.001);

        McRemoteDiagnostics remote=new McRemoteDiagnostics();
        Node repeater=repeater(key('b'));
        remote.request(McRemoteDiagnostics.Kind.NEIGHBORS,repeater,8,NOW);
        remote.sentToRadio(ack(556),NOW+1);
        assertTrue(remote.receive(response(556,payload),NOW+3));
        assertNotNull(remote.find(repeater.id).neighbors);
        assertEquals(5,remote.find(repeater.id).neighbors.list.size());

        // A 60-byte status reply cannot masquerade as an empty neighbors list.
        byte[] masquerade=new byte[60];masquerade[0]=(byte)0xe4;
        masquerade[1]=0x0c; // 3300 mV, not a neighbor header
        try{McRemoteDiagnostics.parseNeighbors(masquerade,8);fail("accepted unrelated reply");}
        catch(IllegalArgumentException expected){}
        payload[2]=9; // more than requested 8
        try{McRemoteDiagnostics.parseNeighbors(payload,8);fail("accepted overlong page");}
        catch(IllegalArgumentException expected){}
        payload[2]=5;payload[0]=10; // offset 8 plus 5 > 10 reported total
        try{McRemoteDiagnostics.parseNeighbors(payload,8);fail("accepted overrun");}
        catch(IllegalArgumentException expected){}
    }

    @Test public void timeoutDoesNotSendAgainAndNewSessionCannotAcceptOldResponse(){
        McRemoteDiagnostics first=new McRemoteDiagnostics();Node n=repeater(key('d'));
        first.request(McRemoteDiagnostics.Kind.STATUS,n,0,NOW);
        first.sentToRadio(ack(123),NOW+50);
        first.tick(NOW+120_050L);
        assertTrue(first.find(n.id).state.contains("Timed out"));
        assertFalse(first.receive(response(123,new byte[52]),NOW+120_100L));
        McRemoteDiagnostics other=new McRemoteDiagnostics();
        assertFalse(other.receive(response(123,new byte[52]),NOW+200));
        assertNull(other.find(n.id));
        Node notRepeater=repeater(key('e'));notRepeater.type=1;
        try{other.request(McRemoteDiagnostics.Kind.STATUS,notRepeater,0,NOW);fail("Non-repeater target accepted");}
        catch(IllegalArgumentException expected){}
    }
}
