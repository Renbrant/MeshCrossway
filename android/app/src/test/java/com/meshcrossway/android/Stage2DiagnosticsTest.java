package com.meshcrossway.android;

import org.junit.Test;
import java.util.*;
import static org.junit.Assert.*;
import static com.meshcrossway.android.MeshData.*;

public class Stage2DiagnosticsTest {
    private static String fullKey(){return "ab".repeat(32);}
    private static Node node(int type,String key,long heard,long received){
        Node n=new Node();n.type=type;n.id=key;n.heard=heard;n.mcReceived=received;return n;
    }
    private static Snapshot snapshot(List<Node> nodes,List<Message> messages){
        return new Snapshot(nodes,new ArrayList<>(),messages);
    }
    private static Message message(boolean outgoing,long time){
        Message m=new Message();m.outgoing=outgoing;m.timestamp=time;return m;
    }

    @Test public void inventoryRequiresFullRepeaterIdentity(){
        Node repeater=node(2,fullKey(),100,85);
        Node client=node(1,fullKey().replace('a','c'),101,99);
        Node truncated=node(2,"abbccd",102,102);
        Snapshot input=snapshot(Arrays.asList(client,truncated,repeater),new ArrayList<>());
        assertEquals(1,RepeaterInventory.total(input));
        assertEquals(repeater,RepeaterInventory.visible(input).get(0));
        assertFalse(RepeaterInventory.isRepeater(truncated));
    }

    @Test public void inventoryUsesObservedTimestampsNotDisplayNames(){
        long now=1_800_000_000L;
        Node a=node(2,fullKey(),now,now-60);
        Node b=node(2,fullKey().replace('a','c'),now+20000,now-3600);
        Node c=node(2,fullKey().replace('a','d'),now,0);
        Snapshot input=snapshot(Arrays.asList(b,c,a),new ArrayList<>());
        assertEquals(a,RepeaterInventory.visible(input).get(0));
        assertEquals(b,RepeaterInventory.visible(input).get(1));
        assertTrue(RepeaterInventory.activity(a,now).contains("15 minutes"));
        assertTrue(RepeaterInventory.activity(b,now).contains("Last local"));
        assertTrue(RepeaterInventory.activity(c,now).contains("No confirmed"));
        assertEquals(0,RepeaterInventory.visible(null).size());
    }

    @Test public void onlyStoredMessagesContributeToPassiveTrafficCounts(){
        long now=1_800_000_000L;
        Message mt=message(false,now-30);
        mt.rx.kind="meshtastic";mt.rx.rssi=-116;mt.rx.snr=3.75;
        Message mc=message(false,now-10);
        mc.rx.kind="flood";mc.rx.observed=true;mc.rx.path="abcd";mc.rx.snr=-6.25;
        Message mqtt=message(false,now-15);
        mqtt.rx.mqtt=true;
        Message sent=message(true,now-30);
        Message expired=message(false,now-901);
        Message future=message(false,now+60);
        Snapshot input=snapshot(new ArrayList<>(),
            Arrays.asList(mt,mc,mqtt,sent,expired,future));
        Stage2Traffic.Counts counts=Stage2Traffic.last15Minutes(input,now);
        assertEquals(3,counts.incoming);
        assertEquals(1,counts.outgoing);
        assertEquals(1,counts.rxRssi);
        assertEquals(2,counts.rxSnr);
        assertEquals(1,counts.observedPaths);
        assertEquals(1,counts.mqttFlagged);
    }

    @Test public void noDataIsUnknownInsteadOfFakedMetrics(){
        Stage2Traffic.Counts counts=Stage2Traffic.last15Minutes(null,1_800_000_000L);
        assertEquals(0,counts.incoming);
        assertEquals(0,counts.rxRssi);
        assertEquals(0,counts.rxSnr);
        assertEquals(0,counts.observedPaths);
    }
}
