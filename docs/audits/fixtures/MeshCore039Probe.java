package com.meshcrossway.android;

import java.util.*;
import java.nio.charset.StandardCharsets;
import static com.meshcrossway.android.MeshData.*;

/** Audit-only protocol fixtures from official Companion 1.18.0. No BLE or RF writes. */
public final class MeshCore039Probe {
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
        ProtocolClient c=ready();expect("BOOTSTRAP",c.ready&&c.nodes.size()==1&&c.channels.size()==1,"published production bootstrap");
        expect("NUS-MTU",RadioProfile.MESHCORE.payloadLimit(176)==173,"safe ATT payload budget");
        c=ready();String before=c.localName;c.setName("Changed");expect("MC01-NO-OPTIMISM",c.localName.equals(before),"queued name does not mutate confirmed values");command(c,100);c.receive(new byte[]{0},102);expect("MC01-ACK-NOT-CONFIRMATION",c.mcAdmin.edit!=null,"ACK still requires readback");command(c,103);c.receive(self(22,0,"Changed"),105);expect("MC01-READBACK",c.mcAdmin.edit==null&&c.mcAdmin.error.isEmpty(),"fresh matching SELF_INFO confirms the name");
        c=ready();c.setName("Rejected");command(c,100);final ProtocolClient errorClient=c;expect("MC01-LOCAL-ERR",!rejects(()->errorClient.receive(new byte[]{1,6},102))&&c.ready&&c.localName.equals("Audit"),"ordinary ERR keeps ready state and old value");
        c=ready();c.setRadio(915,62.5,7,5);expect("MC02-REPEAT",command(c,100).data[11]==1,"repeat enabled remains enabled in SET");
        c=ready();c.receive(contact(138,8),100);String id=Wire.hex(Arrays.copyOfRange(contact(138,8),1,33));expect("MC03-NO-BLIND-APPROVAL",!c.nodes.containsKey(id)&&c.mcDiscoveries.containsKey(id),"discovery is not blindly approved");
        // Same NEW_ADVERT is emitted for successful auto-add and for an unapproved discovery.
        c=ready();c.receive(contact(138,8),100);boolean lookup=c.mcDeferredContacts.contains(id)||c.commands.stream().anyMatch(q->q.data[0]==30);expect("R01-AUTOADD-RESOLUTION",lookup,"manual approval is off; new advert must resolve actual radio membership, lookup queued="+lookup);
        // NOT_FOUND is a contact lifecycle result, not a permanently unsupported feature.
        c=ready();String peer=c.nodes.keySet().iterator().next();c.requestMcContact(peer);command(c,100);c.receive(new byte[]{1,2},102);c.next(103);byte[] advert=new byte[33];advert[0]=(byte)128;System.arraycopy(Wire.unhex(peer),0,advert,1,32);c.receive(advert,104);ProtocolClient.Command retry=c.next(105);expect("R02-READD-AFTER-NOTFOUND",retry!=null&&retry.data[0]==30,"new advert after NOT_FOUND must allow a fresh contact lookup; actual="+(retry==null?"none":retry.data[0]));
        c=ready();final ProtocolClient names=c;expect("MC04-NAME-BYTES",rejects(()->names.setName("é".repeat(16))),"32 UTF8 bytes are rejected before SET");c.requestMcSelf();command(c,100);c.receive(Arrays.copyOf(self(22,0,"é".repeat(16)),89),102);expect("MC04-LEGACY-REPAIR",c.ready&&c.mcNameNeedsRepair,"truncated legacy identity remains repairable");
        c=ready(new Events(),-5,0);expect("MC05-SIGNED-POWER",c.txPower==-5,"negative TX power remains signed");c.setTxPower(27);expect("MC05-DYNAMIC-MAX",command(c,100).data[1]==27,"advertised 30 dBm maximum allows 27");
        c=ready();c.firmwareCode=3;expect("MC06-GATES",!c.requestStats(1)&&!c.requestDefaultFloodScope()&&c.commands.isEmpty(),"old protocol sends neither stats nor scope");
        c=ready();expect("MC07-RECEIVER-TIME",c.nodes.values().iterator().next().heard==LASTMOD,"receiver lastmod replaces sender advert time");Node n=c.nodes.values().iterator().next();byte[] dm=new byte[18];dm[0]=7;System.arraycopy(Wire.unhex(n.id),0,dm,1,6);Wire.put32(dm,9,LASTMOD);put(dm,13,"hello");c.receive(dm,100);expect("MC07-LIVE-DM",c.nodes.get(n.id).heard>LASTMOD,"live receipt advances activity");
        Events e=new Events();c=ready(e,22,0);n=c.nodes.values().iterator().next();Message m=c.send(Target.node(n),"late",1,100);command(c,101);byte[] accepted=new byte[10];accepted[0]=6;Wire.put32(accepted,2,1234);Wire.put32(accepted,6,5000);c.receive(accepted,103);c.next(104);c.next(5104);byte[] ack=new byte[9];ack[0]=(byte)130;Wire.put32(ack,1,1234);c.receive(ack,5107);expect("MC08-LATE-ACK",e.deliveries.contains(m.uid+":delivered"),"late matching ACK reconciles delivery");
        c=ready(new Events(),22,3);c.setOtherParams(c.manualAddContacts,c.telemetryModes,0,c.multiAcks);expect("MC09-RAW-COUNT",command(c,100).data[4]==3,"unrelated edit retains extra ACK count three");
        c=ready();command(c,20020);c.receive(new byte[]{27,0,0,0,0,0,0,0,0},20022);ProtocolClient.Command drain=c.next(20023);expect("MC10-DRAIN",drain!=null&&drain.data[0]==10,"binary item immediately advances backlog");
        // Model successful handoff to GATT followed by disconnect before Android write callback.
        c=ready();c.setTxPower(20);ProtocolClient.Command handedOff=c.next(100);boolean retained=c.mcAdmin.edit!=null&&c.mcAdmin.edit.dispatched;expect("R03-PRECALLBACK-RECOVERY",retained,"SET selected for GATT handoff, callback/response not received: RadioService.release retention predicate="+retained+", opcode="+handedOff.data[0]);
        // The corrected path does retain transactions once Android's callback has occurred.
        c.written(101);expect("R03-POSTCALLBACK-CONTROL",c.mcAdmin.edit.dispatched,"post-write-callback recovery marker is present");
        System.out.println("SUMMARY checks="+checks+" failures="+failures);if(failures>0)System.exit(1);
    }
}
