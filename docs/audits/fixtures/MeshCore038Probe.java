package com.meshcrossway.android;

import java.util.*;
import java.nio.charset.StandardCharsets;
import static com.meshcrossway.android.MeshData.*;

/** Audit-only protocol fixtures from official Companion 1.18.0. No BLE or RF writes. */
public final class MeshCore038Probe {
    static int checks,failures;
    static final long ADVERT=1700000000L,LASTMOD=1794337200L;
    static final class Events implements ProtocolClient.Listener {
        final List<Message> messages=new ArrayList<>();final List<String> deliveries=new ArrayList<>();
        public void node(Node n){}public void channel(Channel c){}
        public void message(Message m){messages.add(m);}public void delivery(String uid,String state){deliveries.add(uid+":"+state);}
    }
    static void expect(String id,boolean ok,String detail){checks++;if(!ok)failures++;System.out.println((ok?"PASS ":"FAIL ")+id+" | "+detail);}
    static ProtocolClient.Command command(ProtocolClient c,long now){ProtocolClient.Command q=c.next(now);if(q==null)throw new AssertionError("Missing command");c.written(now+1);return q;}
    static byte[] metadata(int version){byte[] f=new byte[82];f[0]=13;f[1]=(byte)version;f[2]=100;f[3]=1;put(f,8,"15 Oct 2026");put(f,20,"Heltec V3");put(f,60,"v1.18.0");f[80]=1;f[81]=1;return f;}
    static byte[] self(int power,int acks,String name){byte[] text=name.getBytes(StandardCharsets.UTF_8),f=new byte[58+text.length];f[0]=5;f[1]=1;f[2]=(byte)power;f[3]=30;f[4]=1;f[44]=(byte)acks;f[45]=1;f[46]=0x15;Wire.put32(f,48,915000);Wire.put32(f,52,62500);f[56]=7;f[57]=5;System.arraycopy(text,0,f,58,text.length);return f;}
    static void put(byte[] f,int pos,String text){byte[] bytes=text.getBytes(StandardCharsets.UTF_8);System.arraycopy(bytes,0,f,pos,bytes.length);}
    static byte[] contact(int kind,int prefix){byte[] f=new byte[148];f[0]=(byte)kind;f[1]=(byte)prefix;f[33]=1;f[35]=(byte)255;put(f,100,"Peer");Wire.put32(f,132,ADVERT);Wire.put32(f,144,LASTMOD);return f;}
    static ProtocolClient start(Events e,int version){ProtocolClient c=new ProtocolClient(RadioProfile.MESHCORE,42,e);command(c,0);c.receive(metadata(version),2);command(c,3);return c;}
    static ProtocolClient ready(Events e,int power,int acks){return ready(e,power,acks,30);}
    static ProtocolClient ready(Events e,int power,int acks,int maximum){ProtocolClient c=start(e,14);byte[] info=self(power,acks,"Audit");info[3]=(byte)maximum;c.receive(info,5);command(c,6);c.receive(new byte[]{2,1,0,0,0},8);c.receive(contact(3,7),9);c.receive(new byte[]{4,0,0,0,0},10);command(c,11);byte[] ch=new byte[50];ch[0]=18;put(ch,2,"Test");Arrays.fill(ch,34,50,(byte)1);c.receive(ch,13);command(c,14);c.receive(new byte[]{10},16);c.next(17);return c;}
    static ProtocolClient ready(){return ready(new Events(),22,0);}
    static boolean rejects(Runnable r){try{r.run();return false;}catch(IllegalArgumentException e){return true;}}
    public static void main(String[] args){
        ProtocolClient c=ready();expect("BOOTSTRAP",c.ready&&c.nodes.size()==1&&c.channels.size()==1,"device/self/contacts/channels bootstrap succeeds");
        expect("NUS-UUID",RadioProfile.MESHCORE.service.toString().equals("6e400001-b5a3-f393-e0a9-e50e24dcca9e"),"official Nordic UART service UUID");
        expect("MTU-BUDGET",RadioProfile.MESHCORE.payloadLimit(176)==173,"176-byte ATT MTU leaves 173-byte writes");
        c=ready(new Events(),-5,0);expect("SIGNED-TX-POWER",c.txPower==-5,"firmware TX -5 dBm decoded as "+c.txPower);
        c=ready();c.setRadio(915,62.5,7,5);ProtocolClient.Command radio=command(c,100);
        expect("RADIO-PRESERVE-REPEAT",radio.data.length>11&&radio.data[11]==1,"boot advertises repeat enabled; radio save sends repeat="+(radio.data[11]&255));
        c=ready();String old=c.localName;c.setName("Changed");expect("NO-OPTIMISTIC-SETTINGS",c.localName.equals(old),"before a write/ACK/readback, localName="+c.localName);
        c=ready(new Events(),17,0,20);c.setTxPower(22);command(c,100);final ProtocolClient rejected=c;boolean threw=rejects(()->rejected.receive(new byte[]{1,6},102));
        expect("SETTING-ERROR-RECOVERY",!threw,"a normal settings ERR throws="+threw+"; RadioService.received closes GATT on this exception");
        expect("REJECTED-SETTING-CACHE",c.txPower==17,"a radio with maximum 20 rejects 22 dBm, but displayed TX power remains "+c.txPower);
        ProtocolClient nonfinite=ready();expect("RADIO-FINITE-VALIDATION",rejects(()->nonfinite.setRadio(Double.NaN,62.5,7,5)),"nonfinite radio input must fail before queueing an invalid command");
        ProtocolClient badLocation=ready();expect("LOCATION-FINITE-VALIDATION",rejects(()->badLocation.setLocation(Double.NaN,Double.NaN)),"NaN coordinates must not become an optimistic zero/zero position");
        ProtocolClient ascii=ready();expect("NAME-31",!rejects(()->ascii.setName("a".repeat(31))),"31-byte name is accepted");
        ProtocolClient longName=ready();expect("NAME-32-REJECT",rejects(()->longName.setName("a".repeat(32))),"firmware stores at most 31 bytes");
        ProtocolClient unicode=ready();expect("NAME-UTF8-REJECT",rejects(()->unicode.setName("é".repeat(16))),"32-byte UTF-8 name must be rejected before firmware truncates a code point");
        ProtocolClient trunc=start(new Events(),14);byte[] malformed=Arrays.copyOf(self(22,0,"é".repeat(16)),58+31);
        expect("TRUNCATED-NAME-STRICT-DECODE",rejects(()->trunc.receive(malformed,5)),"firmware byte truncation can make the next identity response fail strict UTF-8 decoding");
        c=ready();Node peer=c.nodes.values().iterator().next();byte[] del=new byte[33];del[0]=(byte)143;System.arraycopy(Wire.unhex(peer.id),0,del,1,32);c.receive(del,100);
        expect("CONTACT-DELETED",!c.nodes.containsKey(peer.id),"deleted key remains eligible as a synchronized contact="+c.nodes.containsKey(peer.id));
        c=ready();c.manualAddContacts=true;c.receive(contact(138,8),100);String discovered=Wire.hex(Arrays.copyOfRange(contact(138,8),1,33));
        expect("DISCOVERY-NOT-APPROVAL",!c.nodes.containsKey(discovered),"manual discovery was promoted to active contact="+c.nodes.containsKey(discovered));
        c=ready();c.refreshContacts();command(c,100);c.receive(new byte[]{2,0,0,0,0},102);c.receive(new byte[]{4,0,0,0,0},103);
        expect("CONTACT-FULL-REFRESH",c.nodes.isEmpty(),"a fresh empty radio contact list leaves local count="+c.nodes.size());
        c=ready();expect("CONTACT-LAST-HEARD",c.nodes.values().iterator().next().heard==LASTMOD,"lastmod="+LASTMOD+", advert timestamp="+ADVERT+", displayed heard="+c.nodes.values().iterator().next().heard);
        c=ready();peer=c.nodes.values().iterator().next();long heard=peer.heard;byte[] dm=new byte[18];dm[0]=7;System.arraycopy(Wire.unhex(peer.id),0,dm,1,6);Wire.put32(dm,9,LASTMOD+60);put(dm,13,"hello");c.receive(dm,100);
        expect("DM-UPDATES-ACTIVITY",c.nodes.get(peer.id).heard>heard,"receiving a fresh DM does not advance the contact activity timestamp");
        c=ready(new Events(),22,3);c.setOtherParams(c.manualAddContacts,c.telemetryModes,0,c.multiAcks);ProtocolClient.Command other=command(c,100);
        expect("PRESERVE-MULTI-ACK-COUNT",other.data[4]==3,"unrelated privacy edit changes firmware extra ACK count 3 to "+(other.data[4]&255));
        Events e=new Events();c=ready(e,22,0);peer=c.nodes.values().iterator().next();Message outgoing=c.send(Target.node(peer),"late",1,100);command(c,101);byte[] sent=new byte[10];sent[0]=6;Wire.put32(sent,2,1234);Wire.put32(sent,6,5000);c.receive(sent,103);c.next(104);c.next(5104);byte[] ack=new byte[9];ack[0]=(byte)130;Wire.put32(ack,1,1234);c.receive(ack,5107);
        expect("LATE-ACK-RECONCILIATION",e.deliveries.contains(outgoing.uid+":delivered"),"valid matching ACK after the estimate is forgotten; events="+String.join(", ",e.deliveries.stream().map(v->v.substring(v.lastIndexOf(':')+1)).toArray(String[]::new)));
        c=ready();c.firmwareCode=3;c.requestStats(1);expect("OLD-PROTOCOL-STATS-GATE",c.commands.isEmpty(),"protocol v3 queues v8+ stats opcode="+(c.commands.peekFirst().data[0]&255));
        c=ready();c.firmwareCode=3;c.requestDefaultFloodScope();expect("OLD-PROTOCOL-SCOPE-GATE",c.commands.isEmpty(),"protocol v3 queues newer scope opcode="+(c.commands.peekFirst().data[0]&255));
        c=ready();c.requestStats(1);command(c,100);byte[] stats=new byte[14];stats[0]=24;stats[1]=1;stats[2]=(byte)-115;stats[3]=(byte)-1;stats[4]=(byte)-90;stats[5]=-12;Wire.put32(stats,6,55);Wire.put32(stats,10,66);c.receive(stats,103);
        expect("RADIO-STATS-OFFSETS",c.noiseFloor==-115&&c.lastRssi==-90&&c.lastSnrQuarter==-12&&c.txAirSecs==55&&c.rxAirSecs==66,"signed RSSI/SNR, noise and airtime match the official frame");
        c=ready();c.requestDefaultFloodScope();command(c,100);byte[] scope=new byte[48];scope[0]=28;put(scope,1,"#test");c.receive(scope,103);expect("DEFAULT-SCOPE-READ",c.defaultFloodScope.equals("#test"),"scope name offset is correct");
        c=ready();c.next(20020);c.written(20021);c.receive(new byte[]{27,0,0,0,0,0,0,0,0},20022);ProtocolClient.Command drain=c.next(20023);
        expect("BINARY-BACKLOG-DRAIN",drain!=null&&drain.data[0]==10,"consumed channel-data frame does not immediately request the next queued message");
        c=ready();Message group=c.send(Target.channel(c.channels.get(0)),"hello",1,100);ProtocolClient.Command text=command(c,101);
        expect("GROUP-TEXT-WIRE",text.data[0]==3&&text.data[1]==0&&text.data[2]==0&&new String(Arrays.copyOfRange(text.data,7,text.data.length),StandardCharsets.UTF_8).equals("hello"),"plain channel send offsets match firmware");
        c=ready();peer=c.nodes.values().iterator().next();Node collision=new Node(peer);collision.id=peer.id.substring(0,62)+"01";c.nodes.put(collision.id,collision);final ProtocolClient ambiguous=c;final Node recipient=peer;
        expect("AMBIGUOUS-DM-REJECT",rejects(()->ambiguous.send(Target.node(recipient),"test",1,100)),"a 6-byte key-prefix collision cannot select an arbitrary outgoing recipient");
        System.out.println("SUMMARY checks="+checks+" failures="+failures);if(failures>0)System.exit(1);
    }
}
