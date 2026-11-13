package com.meshcrossway.android;

import org.junit.Test;
import java.util.*;
import java.nio.charset.StandardCharsets;
import static org.junit.Assert.*;
import static com.meshcrossway.android.MeshData.*;

public class McRepeaterAdminTest {
    static final long NOW=500000;
    static Node node(){Node n=new Node();n.id="ab".repeat(32);n.type=2;n.mcState=ProtocolClient.MC_APPROVED;return n;}
    static byte[] ack(){byte[] d=new byte[10];d[0]=6;Wire.put32(d,6,5000);return d;}
    static byte[] login(Node n,int permissions){byte[] d=new byte[14];d[0]=(byte)0x85;d[1]=(byte)permissions;System.arraycopy(Wire.unhex(n.id.substring(0,12)),0,d,2,6);Wire.put32(d,8,1700000000);return d;}
    static void authorize(McRepeaterAdmin a,Node n){a.login(n,List.of(n),"test",NOW,173);a.accepted(ack(),NOW+1);assertTrue(a.loginResponse(login(n,1),NOW+2));}
    static byte[] reply(Node n,long stamp,boolean v3){int start=v3?4:1;byte[] text="> test".getBytes(StandardCharsets.UTF_8);byte[] d=new byte[start+12+text.length];d[0]=(byte)(v3?16:7);System.arraycopy(Wire.unhex(n.id.substring(0,12)),0,d,start,6);d[start+7]=1;Wire.put32(d,start+8,stamp);System.arraycopy(text,0,d,start+12,text.length);return d;}
    static void rejects(Runnable action){try{action.run();fail("Expected validation failure");}catch(IllegalArgumentException expected){}}

    @Test public void loginRequiresMatchingRemotePermissionNotLocalAck(){
        Node n=node();McRepeaterAdmin a=new McRepeaterAdmin();byte[] frame=a.login(n,List.of(n),"test",NOW,173);
        assertEquals(26,frame[0]);assertEquals(37,frame.length);
        assertArrayEquals(Wire.unhex(n.id),Arrays.copyOfRange(frame,1,33));
        assertFalse(a.authorized(n.id,NOW));assertFalse(a.loginResponse(login(n,1),NOW+1));
        a.accepted(ack(),NOW+2);assertFalse(a.authorized(n.id,NOW+3));
        byte[] wrong=login(n,1);wrong[2]=0;assertFalse(a.loginResponse(wrong,NOW+4));
        assertTrue(a.loginResponse(login(n,0),NOW+5));assertFalse(a.authorized(n.id,NOW+6));
        rejects(()->a.command(n,List.of(n),"set name test",NOW+7,1700000001,173));
        authorize(a,n);assertTrue(a.authorized(n.id,NOW+8));assertFalse(a.authorized("cd".repeat(32),NOW+8));
        a.tick(NOW+McRepeaterAdmin.SESSION_MS+3);assertFalse(a.admin);
    }
    @Test public void validationTimeoutCollisionAndSecretReplies(){
        Node n=node(),other=node();other.id=n.id.substring(0,12)+"cd".repeat(26);
        McRepeaterAdmin a=new McRepeaterAdmin();
        rejects(()->a.login(n,List.of(n,other),"test",NOW,173));
        rejects(()->a.login(n,List.of(n),"x".repeat(16),NOW,173));
        rejects(()->a.login(n,List.of(n),"test\n",NOW,173));
        authorize(a,n);
        rejects(()->a.command(n,List.of(n),"set name a\nreboot",NOW+3,1700000001,173));
        rejects(()->a.command(n,List.of(n),"x".repeat(151),NOW+3,1700000001,173));
        byte[] frame=a.command(n,List.of(n),"password hidden",NOW+3,1700000001,173);
        assertEquals(2,frame[0]);assertEquals(1,frame[1]);assertEquals(0,frame[2]);
        assertEquals("Private command",a.label);assertEquals("",a.reply);
        a.accepted(ack(),NOW+4);
        assertTrue(a.cliResponse(reply(n,1700000002,false),1,13,NOW+5));
        assertFalse(a.reply.contains("test"));
        a.command(n,List.of(n),"reboot",NOW+6,1700000003,173);a.accepted(ack(),NOW+7);
        a.tick(NOW+McRepeaterAdmin.WAIT_MS+7);assertFalse(a.pending);assertFalse(a.admin);
        assertTrue(a.state.contains("unknown"));assertFalse(a.loginResponse(login(n,1),NOW+McRepeaterAdmin.WAIT_MS+8));
    }
    @Test public void cliV3DuplicatesAndStaleRepliesAreRejected(){
        McRepeaterAdmin a=new McRepeaterAdmin();Node n=node();authorize(a,n);
        a.command(n,List.of(n),"get name",NOW+3,1700000001,173);a.accepted(ack(),NOW+4);
        assertFalse(a.cliResponse(reply(n,1699999999,true),4,16,NOW+5));
        assertTrue(a.cliResponse(reply(n,1700000002,true),4,16,NOW+6));assertEquals("> test",a.reply);
        a.command(n,List.of(n),"get tx",NOW+7,1700000003,173);a.accepted(ack(),NOW+8);
        assertFalse(a.cliResponse(reply(n,1700000002,true),4,16,NOW+9));assertTrue(a.pending);
        a.forget();assertFalse(a.pending);assertFalse(a.admin);assertEquals("",a.reply);
    }
    @Test public void protocolRoutesCliOutsideChatsAndRejectsConcurrentDiagnostics(){
        int[] messages={0};ProtocolClient c=new ProtocolClient(RadioProfile.MESHCORE,1,new ProtocolClient.Listener(){
            public void node(Node n){}public void channel(Channel n){}public void message(Message m){messages[0]++;}public void delivery(String id,String s){}
        });
        c.commands.clear();c.ready=true;c.mcBacklogDrained=true;c.firmwareCode=14;
        Node n=node();c.nodes.put(n.id,n);
        c.administerRepeater(n.id,"test",true,NOW);
        rejects(()->c.requestMcRepeater(McRemoteDiagnostics.Kind.STATUS,n.id,0,NOW+1));
        ProtocolClient.Command login=c.next(NOW);c.written(NOW+1);c.receive(ack(),NOW+2);
        assertFalse(c.mcRepeaterAdmin.admin);c.receive(login(n,1),NOW+3);assertTrue(c.mcRepeaterAdmin.admin);
        c.administerRepeater(n.id,"get name",false,NOW+4);
        c.next(NOW+5);for(byte b:login.data)assertEquals(0,b);
        c.written(NOW+6);c.receive(ack(),NOW+7);c.receive(reply(n,1700000002,true),NOW+8);
        assertEquals("> test",c.mcRepeaterAdmin.reply);assertEquals(0,messages[0]);
        c.mcRepeaterAdmin.forget();
        byte[] late=reply(n,1700000003,false);late[8]=0; // legacy CLI echo after closing
        c.receive(late,NOW+9);assertEquals(0,messages[0]);
        c.clearRepeaterAdministration();
        ProtocolClient replacement=new ProtocolClient(RadioProfile.MESHCORE,2,c.listener);
        assertFalse(replacement.mcRepeaterAdmin.authorized(n.id,NOW+9));
    }
    @Test public void formsValidateBeforeSending(){
        assertEquals("set radio 910.525,62.5,7,5",McRepeaterAdmin.setting("radio","910.525,62.5,7,5"));
        rejects(()->McRepeaterAdmin.setting("lat","NaN"));rejects(()->McRepeaterAdmin.setting("lon","181"));
        rejects(()->McRepeaterAdmin.setting("radio","910,62.5,99,5"));rejects(()->McRepeaterAdmin.setting("tx","31"));
        rejects(()->McRepeaterAdmin.setting("repeat","maybe"));rejects(()->McRepeaterAdmin.setting("advert.interval","3"));
        rejects(()->McRepeaterAdmin.setting("flood.advert.interval","2"));
        rejects(()->McRepeaterAdmin.setting("prv.key","x"));
        assertEquals("8d 0h",McRepeaterAdmin.duration(692402));
    }
    @Test public void neighborPagesAccumulateAndNewSnapshotResets(){
        McRemoteDiagnostics d=new McRemoteDiagnostics();Node n=node();
        for(int page=0;page<2;page++){
            int offset=page*8,count=page==0?8:5;long time=NOW+page*31000;
            d.request(McRemoteDiagnostics.Kind.NEIGHBORS,n,offset,time);d.sentToRadio(McRemoteDiagnosticsTest.ack(page+1),time+1);
            byte[] payload=new byte[4+count*11];payload[0]=13;payload[2]=(byte)count;
            for(int i=0;i<count;i++)payload[4+i*11]=(byte)(offset+i);
            d.receive(McRemoteDiagnosticsTest.response(page+1,payload),time+2);
        }
        assertEquals(13,d.allNeighbors(n.id).list.size());
        d.request(McRemoteDiagnostics.Kind.NEIGHBORS,n,0,NOW+62000);d.sentToRadio(McRemoteDiagnosticsTest.ack(3),NOW+62001);
        d.receive(McRemoteDiagnosticsTest.response(3,new byte[]{0,0,0,0}),NOW+62002);
        assertEquals(0,d.allNeighbors(n.id).list.size());
    }
}
