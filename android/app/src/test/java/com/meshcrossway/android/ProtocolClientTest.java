package com.meshcrossway.android;

import java.util.*;
import static com.meshcrossway.android.MeshData.*;

public final class ProtocolClientTest {
    static final class Events implements ProtocolClient.Listener {
        List<Message> messages=new ArrayList<>();List<String> deliveries=new ArrayList<>();
        public void node(Node n){}public void channel(Channel c){}public void message(Message m){messages.add(m);}public void delivery(String uid,String state){deliveries.add(uid+":"+state);}
    }
    static void check(boolean condition,String msg){if(!condition)throw new AssertionError(msg);}
    static void rejects(Runnable fn){try{fn.run();}catch(IllegalArgumentException expected){return;}throw new AssertionError("Unsafe operation accepted");}
    static byte[] wrapped(int field,byte[] body){return new Wire.Out().bytes(field,body).array();}
    static ProtocolClient mt(Events e){ProtocolClient c=new ProtocolClient(RadioProfile.MESHTASTIC,42,e);c.payloadLimit=512;check(c.next(0).data[0]==0x18,"config request");c.written(1);c.next(2);
        c.receive(wrapped(3,new Wire.Out().scalar(1,100).array()),3);
        c.receive(wrapped(13,new Wire.Out().scalar(11,1).array()),4);
        c.receive(wrapped(5,new Wire.Out().bytes(6,new Wire.Out().scalar(7,1).scalar(8,3).scalar(9,1).array()).array()),5);
        c.receive(wrapped(10,new Wire.Out().scalar(1,0).bytes(2,new Wire.Out().bytes(3,"Public".getBytes()).array()).scalar(3,1).array()),6);
        c.receive(wrapped(4,new Wire.Out().scalar(1,200).bytes(2,new Wire.Out().bytes(2,"Peer".getBytes()).bytes(8,new byte[32]).array()).array()),7);
        c.receive(new Wire.Out().scalar(7,42).array(),8);check(c.ready,"MT ready");return c;
    }
    static ProtocolClient mc(Events e){ProtocolClient c=new ProtocolClient(RadioProfile.MESHCORE,42,e);c.next(0);c.written(1);c.receive(new byte[]{13,3,1,1,0,0,0,0},2);
        check(c.next(3).data[0]==1,"app start");c.written(4);byte[] identity=new byte[62];identity[0]=5;identity[58]='T';c.receive(identity,5);
        check(c.next(6).data[0]==4,"contacts request");c.written(7);c.receive(new byte[]{2,1,0,0,0},8);
        byte[] contact=new byte[148];contact[0]=3;contact[1]=7;contact[33]=1;contact[100]='P';c.receive(contact,9);c.receive(new byte[]{4,0,0,0,0},10);
        check(c.next(11).data[0]==31,"channel request");c.written(12);byte[] channel=new byte[50];channel[0]=18;channel[2]='C';c.receive(channel,13);check(c.ready,"MC ready");
        check(c.next(14).data[0]==10,"pull messages");c.written(15);c.receive(new byte[]{10},16);c.next(17);return c;
    }
    public static void main(String[] args){
        Events a=new Events();ProtocolClient mt=mt(a);Channel channel=mt.channels.get(0);
        rejects(()->mt.send(new Target("bad","x","","wrong",0,true),"test",9,20));
        Message outgoing=mt.send(Target.channel(channel),"Olá 👋",9,20);ProtocolClient.Command tx=mt.next(21);List<Wire.F> packet=Wire.fields(Wire.bytes(Wire.fields(tx.data),1));check(Wire.number(packet,2,0)==0xffffffffL,"broadcast destination");check(Wire.number(packet,6,0)==9,"packet ID");check(Wire.number(packet,10,1)==0,"broadcast no remote ACK claim");mt.written(22);check(a.deliveries.contains(outgoing.uid+":submitted"),"BLE status");mt.next(23);
        Node peer=mt.nodes.get("200");Message dm=mt.send(Target.node(peer),"private",10,24);tx=mt.next(25);packet=Wire.fields(Wire.bytes(Wire.fields(tx.data),1));check(Wire.bytes(packet,16).length==32&&Wire.number(packet,17,0)==1,"PKI public key supplied");mt.written(26);mt.next(27);
        byte[] ack=new Wire.Out().fixed(1,200).fixed(2,100).bytes(4,new Wire.Out().scalar(1,5).fixed(6,10).bytes(2,new Wire.Out().scalar(3,0).array()).array()).array();mt.receive(wrapped(2,ack),28);check(a.deliveries.contains(dm.uid+":delivered"),"remote ACK");
        byte[] textPacket=new Wire.Out().fixed(1,200).fixed(2,0xffffffffL).fixed(6,99).scalar(3,0).bytes(4,new Wire.Out().scalar(1,1).bytes(2,"hello".getBytes()).array()).array();mt.receive(wrapped(2,textPacket),29);check(a.messages.size()==1&&a.messages.get(0).topic.equals(channel.topic()),"MT text routing");
        mt.receive(wrapped(2,new Wire.Out().fixed(1,200).fixed(2,300).fixed(6,98).bytes(4,new Wire.Out().scalar(1,1).bytes(2,"not ours".getBytes()).array()).array()),30);check(a.messages.size()==1,"foreign DM ignored");
        rejects(()->mt.send(Target.channel(channel),"😀".repeat(56),11,31));mt.txEnabled=false;rejects(()->mt.send(Target.channel(channel),"blocked",11,31));
        Events b=new Events();ProtocolClient mc=mc(b);Message cm=mc.send(Target.channel(mc.channels.get(0)),"hello",1,100);tx=mc.next(101);check(tx.data[0]==3&&tx.data.length==12,"MC channel frame");mc.receive(new byte[]{0},102);mc.written(103);check(!b.deliveries.contains(cm.uid+":submitted"),"early radio reply not downgraded");check(b.deliveries.contains(cm.uid+":radio accepted"),"channel acceptance");mc.next(104);
        Message md=mc.send(Target.node(mc.nodes.values().iterator().next()),"direct",2,105);mc.next(106);mc.written(107);byte[] accepted=new byte[10];accepted[0]=6;Wire.put32(accepted,2,1234);Wire.put32(accepted,6,10000);mc.receive(accepted,108);mc.next(109);byte[] delivered=new byte[9];delivered[0]=(byte)130;Wire.put32(delivered,1,1234);mc.receive(delivered,110);check(b.deliveries.contains(md.uid+":delivered"),"MC remote delivery ACK");
        byte[] incoming=new byte[14];incoming[0]=8;Wire.put32(incoming,4,1700000000);System.arraycopy("hello!".getBytes(),0,incoming,8,6);mc.receive(incoming,111);check(b.messages.size()==1,"MC channel receive");String uid=b.messages.get(0).uid;byte[] v3=new byte[17];v3[0]=17;System.arraycopy(incoming,1,v3,4,incoming.length-1);mc.receive(v3,112);check(uid.equals(b.messages.get(1).uid),"backlog dedup ignores SNR/path metadata");
        rejects(()->mc.receive(new byte[]{17},113));check(a.messages.size()==1,"MC cannot inject MT history");
        Snapshot snap=new Snapshot(new ArrayList<>(),new ArrayList<>(),b.messages);check(snap.conversations().get(0).unread==2,"unread aggregation");
        check(Wire.field(Wire.fields(mt.next(31000).data),7)!=null,"PhoneAPI keepalive");
        System.out.println("Protocol clients: bootstrap, text, PKI, ACK ordering, dedup, isolation, limits passed");
    }
}
