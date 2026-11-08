package com.meshcrossway.android;

import java.nio.charset.StandardCharsets;
import java.util.*;
import static com.meshcrossway.android.MeshData.*;

/** Independent, bounded state per radio. Transport writes and remote acknowledgements are distinct. */
final class ProtocolClient {
    interface Listener {void node(Node n);void channel(Channel c);void message(Message m);void delivery(String uid,String state);}
    static final class Command {
        final byte[] data;final int response;final String uid;boolean written,replied;long deadline;
        Command(byte[] data,int response,String uid){this.data=data;this.response=response;this.uid=uid;}
    }
    private static final class Sent {String uid,peer;long deadline,ack;Sent(String u,String p,long d){uid=u;peer=p;deadline=d;}}
    final RadioProfile profile;final Listener listener;final ProtocolProbe probe;
    final LinkedHashMap<String,Node> nodes=new LinkedHashMap<>();final TreeMap<Integer,Channel> channels=new TreeMap<>();
    final ArrayDeque<Command> commands=new ArrayDeque<>();final LinkedHashMap<Long,Sent> sent=new LinkedHashMap<>();
    final ArrayDeque<byte[]> earlyPackets=new ArrayDeque<>();
    Command active;boolean ready;String description="Synchronizing…",localName="";long myNum,lastPoll,lastStamp,lastWrite;int hopLimit=3,region,channelCount,nextChannel,contactsExpected,contactsSeen,payloadLimit=173;boolean txEnabled,pki,waiting,contactsStarted;
    ProtocolClient(RadioProfile profile,long nonce,Listener listener){this.profile=profile;this.listener=listener;probe=new ProtocolProbe(nonce);
        queue(profile==RadioProfile.MESHTASTIC?probe.meshtasticRequest():ProtocolProbe.meshcoreQuery(),profile==RadioProfile.MESHTASTIC?-1:13,"");}
    void cancel(String uid){commands.removeIf(c->c.uid.equals(uid));removeSent(uid);}
    void queue(byte[] data,int response,String uid){if(commands.size()>=32)throw Wire.bad("Radio command queue is full");commands.add(new Command(data,response,uid));}
    Command next(long now){
        for(Iterator<Map.Entry<Long,Sent>> it=sent.entrySet().iterator();it.hasNext();){Sent s=it.next().getValue();if(now>=s.deadline){listener.delivery(s.uid,"uncertain");it.remove();}}
        if(active!=null){if(now>=active.deadline)throw Wire.bad("Radio command timed out • delivery may be unknown");if(active.written && active.replied)active=null;else return null;}
        if(profile==RadioProfile.MESHCORE&&ready&&commands.isEmpty()&&(waiting||now-lastPoll>=20000)){waiting=false;lastPoll=now;queue(new byte[]{10},10,"");}
        if(profile==RadioProfile.MESHTASTIC&&ready&&commands.isEmpty()&&now-lastWrite>=30000)queue(new Wire.Out().bytes(7,Wire.EMPTY).array(),-1,"");
        if(commands.isEmpty())return null;active=commands.remove();active.deadline=now+30000;return active;
    }
    void written(long now){lastWrite=now;if(active==null)throw Wire.bad("Unexpected write completion");if(!active.uid.isEmpty()&&!active.replied)listener.delivery(active.uid,"submitted");active.written=true;if(active.response<0)active.replied=true;}
    void receive(byte[] data,long now){if(profile==RadioProfile.MESHTASTIC)meshtastic(data,now);else meshcore(data,now);}
    private Node node(String id){Node old=nodes.get(id);Node n=old==null?new Node():new Node(old);n.id=id;return n;}
    private void publish(Node n){if(!nodes.containsKey(n.id)&&nodes.size()>=4096)return;nodes.put(n.id,n);listener.node(n);}
    private void user(byte[] data,Node n){List<Wire.F> fs=Wire.fields(data);byte[] name=Wire.bytes(fs,2);if(name.length>0)n.name=Wire.utf8(name);byte[] key=Wire.bytes(fs,8);if(key.length==32)n.key=Wire.hex(key);n.unmessagable=Wire.number(fs,9,n.unmessagable?1:0)!=0;}
    private void position(byte[] data,Node n){List<Wire.F> fs=Wire.fields(data);Wire.F lat=Wire.field(fs,1),lon=Wire.field(fs,2);if(lat!=null&&lon!=null){n.lat=(int)lat.number/1e7;n.lon=(int)lon.number/1e7;n.position=Math.abs(n.lat)<=90&&Math.abs(n.lon)<=180;}}
    private void meshtastic(byte[] data,long now){
        List<Wire.F> fs=Wire.fields(data);
        for(Wire.F f:fs){
            if(f.tag==3){myNum=Wire.number(Wire.fields(f.bytes),1,0);}
            else if(f.tag==4){List<Wire.F> ns=Wire.fields(f.bytes);long id=Wire.number(ns,1,0);if(id==0)continue;Node n=node(Long.toString(id));
                for(Wire.F nf:ns){if(nf.tag==2)user(nf.bytes,n);if(nf.tag==3)position(nf.bytes,n);if(nf.tag==5)n.heard=nf.number;if(nf.tag==6)n.battery=(int)Wire.number(Wire.fields(nf.bytes),1,-1);}publish(n);
            }else if(f.tag==5){byte[] lora=Wire.bytes(Wire.fields(f.bytes),6);if(lora.length>0){List<Wire.F> ls=Wire.fields(lora);region=(int)Wire.number(ls,7,0);hopLimit=(int)Wire.number(ls,8,3);txEnabled=Wire.number(ls,9,0)!=0;}}
            else if(f.tag==10){List<Wire.F> cs=Wire.fields(f.bytes);int index=(int)Wire.number(cs,1,0);if(index<0||index>=8)throw Wire.bad("Invalid Meshtastic channel slot");Channel c=new Channel();c.slot=index;c.enabled=Wire.number(cs,3,0)>0;byte[] settings=Wire.bytes(cs,2);c.name=Wire.utf8(Wire.bytes(Wire.fields(settings),3));if(c.name.isEmpty())c.name="Channel "+index;c.identity=Wire.hash(settings);channels.put(index,c);listener.channel(c);}
            else if(f.tag==13){pki=Wire.number(Wire.fields(f.bytes),11,0)!=0;}
            else if(f.tag==2){if(!ready){if(earlyPackets.size()>=256)throw Wire.bad("Too many packets during synchronization");earlyPackets.add(f.bytes);}else mtPacket(f.bytes,now);}
            else if(f.tag==11){List<Wire.F> qs=Wire.fields(f.bytes);Sent s=sent.get(Wire.number(qs,4,0));if(s!=null){long code=Wire.number(qs,1,0);listener.delivery(s.uid,code==0?"radio accepted":"rejected");if(code!=0)sent.values().remove(s);}}
        }
        probe.meshtastic(data);if(probe.complete){ready=true;description="Ready • live messages";while(!earlyPackets.isEmpty())mtPacket(earlyPackets.remove(),now);}
    }
    private void mtPacket(byte[] bytes,long now){
        List<Wire.F> ps=Wire.fields(bytes);long from=Wire.number(ps,1,0),to=Wire.number(ps,2,0),id=Wire.number(ps,6,0),time=Wire.number(ps,7,0);int slot=(int)Wire.number(ps,3,0);
        byte[] decoded=Wire.bytes(ps,4);if(decoded.length==0)return;List<Wire.F> ds=Wire.fields(decoded);int port=(int)Wire.number(ds,1,0);byte[] payload=Wire.bytes(ds,2);
        if(port==5){long request=Wire.number(ds,6,0);Wire.F error=Wire.field(Wire.fields(payload),3);Sent s=sent.get(request);
            if(s!=null&&error!=null&&to==myNum&&(from==myNum||Long.toString(from).equals(s.peer))){boolean delivered=error.number==0&&from!=myNum&&to==myNum&&s.peer.equals(Long.toString(from));listener.delivery(s.uid,error.number!=0?"rejected":delivered?"delivered":"radio accepted");if(delivered||error.number!=0)sent.remove(request);}return;}
        if(from==0)return;
        if(port==3||port==4||port==67){Node n=node(Long.toString(from));if(port==3)position(payload,n);else if(port==4)user(payload,n);else{byte[] metrics=Wire.bytes(Wire.fields(payload),2);if(metrics.length>0)n.battery=(int)Wire.number(Wire.fields(metrics),1,-1);}n.heard=time;publish(n);return;}
        if(port!=1 || id==0 || payload.length==0 || payload.length>237 || (to!=0xffffffffL&&to!=myNum&&from!=myNum))return;
        if(from==myNum&&sent.containsKey(id)){listener.delivery(sent.get(id).uid,"radio accepted");return;}
        Message m=new Message();m.uid="mt:"+from+":"+id;m.body=Wire.utf8(payload);m.outgoing=from==myNum;m.read=m.outgoing;m.timestamp=time>=1577836800L?time:System.currentTimeMillis()/1000;m.slot=slot;m.sender=nodeName(Long.toString(from));m.group=to==0xffffffffL;
        if(m.group){Channel c=channels.get(slot);if(c==null||!c.enabled){c=new Channel();c.slot=slot;c.name="Channel "+slot;c.identity="unknown";}m.topic=c.topic();m.title=c.name;m.identity=c.identity;}
        else{m.peer=Long.toString(m.outgoing?to:from);m.topic="d:"+m.peer;m.title=nodeName(m.peer);}
        m.state=m.outgoing?"radio accepted":"received";listener.message(m);
    }
    String nodeName(String id){Node n=nodes.get(id);if(n!=null&&!n.name.isEmpty())return n.name;return profile==RadioProfile.MESHTASTIC?String.format(Locale.ROOT,"!%08x",Long.parseLong(id)):id.substring(0,Math.min(12,id.length()));}
    private void mcContact(byte[] data){if(data.length<148)throw Wire.bad("Truncated MeshCore contact");String id=Wire.hex(Arrays.copyOfRange(data,1,33));Node n=node(id);n.key=id;n.type=data[33]&255;n.name=Wire.fixedText(data,100,32);n.heard=Wire.u32(data,132);n.lat=(int)Wire.u32(data,136)/1e6;n.lon=(int)Wire.u32(data,140)/1e6;n.position=(n.lat!=0||n.lon!=0)&&Math.abs(n.lat)<=90&&Math.abs(n.lon)<=180;publish(n);}
    private void nextMcChannel(){if(nextChannel<channelCount){description="Synchronizing channel "+(nextChannel+1)+" / "+channelCount;queue(new byte[]{31,(byte)nextChannel++},18,"");}else{ready=true;waiting=true;description="Ready • live messages";}}
    private void meshcore(byte[] data,long now){
        if(data.length==0||data.length>176)throw Wire.bad("Invalid MeshCore frame");int code=data[0]&255;
        int command=active==null?-1:active.data[0]&255;boolean matched=active!=null&&(code==active.response||code==1||code==15||(command==4&&code==4)||(command==10&&(code==7||code==8||code==16||code==17||code==27)));
        if(command==31&&code==18)matched=data.length>=2&&data[1]==active.data[1];
        if((code==1||code==15)&&matched){active.replied=true;if(!active.uid.isEmpty()){listener.delivery(active.uid,"rejected");removeSent(active.uid);return;}
            if(command==31&&data.length>=2&&data[1]==2){nextMcChannel();return;}throw Wire.bad("MeshCore rejected command "+command);}
        if(code==13&&matched){if(data.length<8||(data[1]&255)<3)throw Wire.bad("MeshCore Companion v3+ required");channelCount=data[3]&255;queue(ProtocolProbe.meshcoreAppStart(),5,"");}
        else if(code==5&&matched){if(data.length<58)throw Wire.bad("Truncated MeshCore identity");localName=Wire.fixedText(data,58,data.length-58);myNum=1;queue(new byte[]{4},4,"");description="Synchronizing MeshCore contacts…";}
        else if(code==2&&command==4){if(data.length<5)throw Wire.bad("Invalid contacts header");contactsExpected=(int)Wire.u32(data,1);if(contactsExpected<0||contactsExpected>4096)throw Wire.bad("Too many contacts");contactsSeen=0;contactsStarted=true;}
        else if(code==3||code==138){mcContact(data);if(code==3&&command==4){contactsSeen++;active.deadline=now+30000;}}
        else if(code==4&&matched){if(!contactsStarted||contactsSeen!=contactsExpected)throw Wire.bad("Incomplete contacts sync");contactsStarted=false;nextMcChannel();}
        else if(code==18&&matched){if(data.length<50)throw Wire.bad("Truncated MeshCore channel");Channel c=new Channel();c.slot=data[1]&255;c.name=Wire.fixedText(data,2,32);c.identity=Wire.hash(Arrays.copyOfRange(data,34,50));c.enabled=!c.name.isEmpty();channels.put(c.slot,c);listener.channel(c);nextMcChannel();}
        else if(code==0&&matched&&!active.uid.isEmpty()){listener.delivery(active.uid,"radio accepted");removeSent(active.uid);}
        else if(code==6&&matched){if(data.length<10)throw Wire.bad("Truncated send acknowledgement");for(Sent s:sent.values())if(s.uid.equals(active.uid)){s.ack=Wire.u32(data,2);s.deadline=now+Math.max(5000,Math.min(600000,Wire.u32(data,6)));listener.delivery(s.uid,"radio accepted");}}
        else if(code==7||code==8||code==16||code==17){mcMessage(data);waiting=true;}
        else if(code==130){if(data.length<9)throw Wire.bad("Truncated delivery ACK");long ack=Wire.u32(data,1);Sent found=null;int matches=0;for(Sent s:sent.values())if(s.ack!=0&&s.ack==ack){found=s;matches++;}if(matches==1){listener.delivery(found.uid,"delivered");removeSent(found.uid);}}
        else if(code==131)waiting=true;
        else if(code==128&&ready&&data.length>=33&&commands.size()<8){byte[] request=Arrays.copyOf(data,33);request[0]=30;queue(request,3,"");}
        if(matched)active.replied=true;
    }
    private void mcMessage(byte[] data){
        int code=data[0]&255;boolean group=code==8||code==17,v3=code==16||code==17;int start=v3?4:1,meta=group?3:8,typepos=start+(group?2:7);
        if(data.length<start+meta+4)throw Wire.bad("Truncated MeshCore message");int type=data[typepos]&255,offset=start+meta+4+(type==2?4:0);if(type!=0&&type!=2)return;if(offset>data.length)throw Wire.bad("Truncated signed message");
        Message m=new Message();m.group=group;m.timestamp=Wire.u32(data,start+meta);if(m.timestamp<1577836800L)m.timestamp=System.currentTimeMillis()/1000;m.body=Wire.utf8(Arrays.copyOfRange(data,offset,data.length));if(m.body.isEmpty())return;
        if(group){m.slot=data[start]&255;Channel c=channels.get(m.slot);if(c==null||!c.enabled)return;m.topic=c.topic();m.title=c.name;m.identity=c.identity;int colon=m.body.indexOf(": ");m.sender=colon>0&&colon<40?m.body.substring(0,colon):"Channel participant";}
        else{String prefix=Wire.hex(Arrays.copyOfRange(data,start,start+6));Node found=null;int count=0;for(Node n:nodes.values())if(n.id.startsWith(prefix)){found=n;count++;}m.peer=count==1?found.id:prefix;m.topic="d:"+prefix;m.title=count==1?nodeName(m.peer):prefix;m.sender=m.title;}
        // SNR/path metadata can change when the same radio backlog packet is replayed.
        m.uid="mc:"+Wire.hash((m.topic+"|"+Wire.u32(data,start+meta)+"|"+type+"|"+(type==2?Wire.hex(Arrays.copyOfRange(data,start+meta+4,offset)):"")+"|"+m.body).getBytes(StandardCharsets.UTF_8));listener.message(m);
    }
    private void removeSent(String uid){sent.values().removeIf(s->s.uid.equals(uid));}
    Message send(Target target,String text,long id,long now){
        if(!ready)throw Wire.bad("Wait for this radio to finish synchronization");if(sent.size()>=32||commands.size()>=20)throw Wire.bad("Wait for pending transmissions");
        Channel c=channels.get(target.slot);Node n=nodes.get(target.peer);
        if(target.group&&(c==null||!c.enabled||!c.identity.equals(target.identity)))throw Wire.bad("Channel changed • select it again in Groups");
        if(!target.group&&(n==null||n.unmessagable))throw Wire.bad("Select a synchronized, message-capable node");
        Message m=new Message();m.topic=target.topic;m.title=target.title;m.peer=target.peer;m.identity=target.identity;m.slot=target.slot;m.group=target.group;m.body=text;m.outgoing=m.read=true;m.timestamp=System.currentTimeMillis()/1000;m.sender="You";m.state="queued";
        byte[] frame;int response=-1;
        if(profile==RadioProfile.MESHTASTIC){
            if(region==0||!txEnabled)throw Wire.bad("Set LoRa region and enable TX using the official Meshtastic app first");
            if(!target.group&&(channels.get(0)==null||!channels.get(0).enabled))throw Wire.bad("Primary channel is disabled or not synchronized");
            if(!target.group&&Long.parseLong(target.peer)==myNum)throw Wire.bad("Choose another node");
            if(!target.group&&pki&&(n.key.length()!=64))throw Wire.bad("Wait for this node's public key before sending a DM");
            byte[] body=Wire.text(text,220);Wire.Out decoded=new Wire.Out().scalar(1,1).bytes(2,body);
            Wire.Out packet=new Wire.Out().fixed(2,target.group?0xffffffffL:Long.parseLong(target.peer)).scalar(3,target.group?target.slot:0).bytes(4,decoded.array()).fixed(6,id).scalar(9,hopLimit).scalar(10,target.group?0:1).scalar(11,target.group?64:70);
            if(!target.group&&pki)packet.bytes(16,Wire.unhex(n.key)).scalar(17,1);
            frame=new Wire.Out().bytes(1,packet.array()).array();m.uid="mt:"+myNum+":"+id;
        }else{
            if(!target.group&&(n.type!=1||target.peer.length()!=64))throw Wire.bad("Direct chat requires a client contact with a unique key");
            if(!target.group){int count=0;for(Node other:nodes.values())if(other.id.startsWith(target.peer.substring(0,12)))count++;if(count!=1)throw Wire.bad("Ambiguous recipient key prefix");}
            int offset=target.group?7:13,limit=Math.min(150,payloadLimit-offset);if(target.group)limit=Math.min(limit,Math.max(0,160-localName.getBytes(StandardCharsets.UTF_8).length-2));
            byte[] body=Wire.text(text,limit);frame=new byte[offset+body.length];frame[0]=(byte)(target.group?3:2);frame[2]=(byte)(target.group?target.slot:0);lastStamp=Math.max(lastStamp+1,m.timestamp);Wire.put32(frame,3,lastStamp);m.timestamp=lastStamp;
            if(!target.group)System.arraycopy(Wire.unhex(target.peer),0,frame,7,6);System.arraycopy(body,0,frame,offset,body.length);response=target.group?0:6;m.uid="mc-out:"+UUID.randomUUID();
        }
        if(frame.length>payloadLimit)throw Wire.bad("Message exceeds negotiated Bluetooth frame size • shorten it");
        long key=profile==RadioProfile.MESHTASTIC?id:now;while(sent.containsKey(key))key++;sent.put(key,new Sent(m.uid,target.group?"broadcast":target.peer,now+120000));queue(frame,response,m.uid);return m;
    }
}
