package com.meshcrossway.android;

import org.junit.Test;
import java.util.*;
import static org.junit.Assert.*;
import static com.meshcrossway.android.MeshData.*;
import static com.meshcrossway.android.McRepeaterSettings.*;

public class McRepeaterSettingsTest {
    static final String KEY="ab".repeat(32);
    static String response(Field f){switch(f){case NAME:return "> Repeater";case LAT:return "> 12.345670";case LON:return "> -45.6789";case RADIO:return "> 910.525,62.5,7,5";case TX:return "> 22";case REPEAT:case READ_ONLY:return "> on";case LOCAL_ADVERT:return "> 10";case FLOOD_ADVERT:return "> 24";default:return "> 3";}}
    static void load(McRepeaterSettings s){s.open(KEY);for(int i=0;i<10;i++){Operation op=s.next(10000+i*2000);assertNotNull(op);assertFalse(op.write);s.dispatched(op);s.received(response(op.field),10001+i*2000);}}
    static void rejects(Runnable r){try{r.run();fail("Expected rejection");}catch(IllegalArgumentException expected){}}
    @Test public void enteringQueuesExactlyOnePublicReadPerFieldNotPerRender(){
        McRepeaterSettings s=new McRepeaterSettings();s.open(KEY);Set<Field> read=new HashSet<>();
        for(int i=0;i<10;i++){
            s.open(KEY);Operation op=s.next(10000+i*2000);assertNotNull(op);assertTrue(read.add(op.field));assertEquals("get "+op.field.key,op.command);assertFalse(op.write);
            s.dispatched(op);assertNull(s.next(999999));s.received(response(op.field),10001+i*2000);assertNull(s.next(10002+i*2000));
        }
        assertEquals(10,read.size());assertEquals(10,s.loaded());assertNull(s.next(999999));s.open(KEY);assertNull(s.next(999999));
        assertEquals("22 dBm",s.display(Field.TX));assertEquals("On",s.display(Field.REPEAT));assertTrue(s.display(Field.RADIO).contains("62.5 kHz"));
    }
    @Test public void writesNeedExplicitSaveAndOkDoesNotReplaceTheCurrentValue(){
        McRepeaterSettings s=new McRepeaterSettings();load(s);
        Operation write=s.edit(Field.TX,"30",40000);assertTrue(write.write);assertEquals("set tx 30",write.command);s.dispatched(write);
        assertEquals("22",s.values.get(Field.TX).raw);s.received("OK",40001);
        assertEquals("22",s.values.get(Field.TX).raw);assertTrue(s.values.get(Field.TX).state.contains("checking"));
        Operation read=s.next(41001);assertFalse(read.write);assertEquals(Field.TX,read.field);s.dispatched(read);s.received("> 30",41002);
        assertEquals("30 dBm",s.display(Field.TX));assertEquals("Confirmed by readback",s.values.get(Field.TX).state);assertNull(s.next(50000));
        write=s.edit(Field.TX,"30",50000);s.dispatched(write);s.received("OK",50001);read=s.next(51001);s.dispatched(read);s.received("> 22",51002);
        assertEquals("22 dBm",s.display(Field.TX));assertTrue(s.values.get(Field.TX).state.contains("different"));assertNull(s.next(60000));
    }
    @Test public void unsupportedTimeoutAndLeavingNeverRetryOrContinueSilently(){
        McRepeaterSettings s=new McRepeaterSettings();s.open(KEY);Operation op=s.next(10000);s.dispatched(op);s.received("Unknown config",10001);
        assertFalse(s.values.get(op.field).valid);assertTrue(s.values.get(op.field).state.contains("Unavailable"));
        op=s.next(11001);s.dispatched(op);s.interrupted();assertNull(s.next(999999));assertTrue(s.status.contains("stopped"));
        s.refresh();op=s.next(1000000);assertNotNull(op);s.dispatched(op);s.close();s.received(response(op.field),1000001);assertNull(s.next(2000000));
        s.open("cd".repeat(32));assertEquals(0,s.loaded());assertNull(s.current);
    }
    @Test public void editingPausesTheBatchAndInvalidInputDoesNotWrite(){
        McRepeaterSettings s=new McRepeaterSettings();s.open(KEY);Operation op=s.next(10000);s.dispatched(op);s.received("> Repeater",10001);
        s.editing=true;assertNull(s.next(12000));rejects(()->s.edit(Field.TX,"31",12000));assertNull(s.next(12000));
        Operation write=s.edit(Field.NAME,"New name",12000);assertEquals("set name New name",write.command);
        s.dispatched(write);s.received("ERR: rejected",12001);assertEquals("Repeater",s.values.get(Field.NAME).raw);assertTrue(s.values.get(Field.NAME).state.contains("not accepted"));
        s.editing=false;assertNotNull(s.next(13001));
    }
    @Test public void parserDoesNotFabricateValuesFromErrorsOrNonFiniteNumbers(){
        rejects(()->parse(Field.TX,"OK"));rejects(()->parse(Field.LAT,"> NaN"));rejects(()->parse(Field.RADIO,"> 910,62.5,7"));rejects(()->parse(Field.REPEAT,"> maybe"));
        assertTrue(equivalent(Field.LAT,"12.34567","12.345670"));assertFalse(equivalent(Field.TX,"30","22"));
        assertTrue(equivalent(Field.RADIO,"910.525,62.5,7,5","910.525000,62.500,7,5"));
    }
    @Test public void publicNameContainingKeyStillGetsAnOkAndReadback(){
        Node n=McRepeaterAdminTest.node();ProtocolClient c=new ProtocolClient(RadioProfile.MESHCORE,1,new ProtocolClient.Listener(){public void node(Node n){}public void channel(Channel ch){}public void message(Message m){fail("Public configuration reply leaked to chat");}public void delivery(String id,String s){}});
        c.commands.clear();c.ready=true;c.mcBacklogDrained=true;c.firmwareCode=14;c.lastPoll=500000;c.nodes.put(n.id,n);
        McRepeaterAdminTest.authorize(c.mcRepeaterAdmin,n);c.mcRepeaterSettings.open(n.id);c.mcRepeaterSettings.started=true;
        Value value=c.mcRepeaterSettings.values.get(Field.NAME);value.raw="Old";value.valid=true;
        c.saveRepeaterSetting(n.id,Field.NAME,"Monkey",500010);assertFalse(c.mcRepeaterAdmin.sensitive);
        c.next(500011);c.written(500012);c.receive(McRepeaterAdminTest.ack(),500013);
        byte[] body="OK".getBytes(java.nio.charset.StandardCharsets.UTF_8),reply=new byte[16+body.length];reply[0]=16;reply[11]=1;System.arraycopy(Wire.unhex(n.id.substring(0,12)),0,reply,4,6);Wire.put32(reply,12,1700000002);System.arraycopy(body,0,reply,16,body.length);
        c.receive(reply,500014);assertEquals("OK",c.mcRepeaterAdmin.reply);assertTrue(c.mcRepeaterSettings.hasReads());
        // Password commands through the advanced path must remain private.
        McRepeaterAdmin privateAdmin=new McRepeaterAdmin();McRepeaterAdminTest.authorize(privateAdmin,n);
        privateAdmin.command(n,java.util.Collections.singletonList(n),"password private",500020,1700000003,173);assertTrue(privateAdmin.sensitive);
    }
    @Test public void protocolAutoReadsOnlyWhileAuthenticatedVisibleAndReady(){
        Node n=McRepeaterAdminTest.node();ProtocolClient c=new ProtocolClient(RadioProfile.MESHCORE,1,new ProtocolClient.Listener(){public void node(Node n){}public void channel(Channel ch){}public void message(Message m){fail("Configuration reply leaked to chat");}public void delivery(String id,String s){}});
        c.commands.clear();c.ready=true;c.mcBacklogDrained=true;c.firmwareCode=14;c.lastPoll=500000;c.nodes.put(n.id,n);c.mcRepeaterSettings.open(n.id);
        assertNull(c.next(500001));McRepeaterAdminTest.authorize(c.mcRepeaterAdmin,n);
        ProtocolClient.Command read=c.next(500010);assertNotNull(read);assertEquals(2,read.data[0]);assertEquals(1,read.data[1]);assertTrue(new String(read.data,13,read.data.length-13,java.nio.charset.StandardCharsets.UTF_8).startsWith("get "));
        c.written(500011);c.receive(McRepeaterAdminTest.ack(),500012);c.receive(McRepeaterAdminTest.reply(n,1700000002,true),500013);
        assertEquals("test",c.mcRepeaterSettings.values.get(Field.NAME).raw);c.mcRepeaterSettings.close();ProtocolClient.Command background=c.next(502000);assertTrue(background==null||background.data[0]==10);
        ProtocolClient replacement=new ProtocolClient(RadioProfile.MESHCORE,2,c.listener);assertEquals(0,replacement.mcRepeaterSettings.loaded());assertFalse(replacement.mcRepeaterSettings.visible);
    }
}
