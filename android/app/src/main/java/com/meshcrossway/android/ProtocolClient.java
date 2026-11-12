package com.meshcrossway.android;

import java.nio.charset.StandardCharsets;
import java.util.*;
import static com.meshcrossway.android.MeshData.*;

/** Independent, bounded state per radio. Transport writes and remote acknowledgements are distinct. */
final class ProtocolClient {
    interface Listener {void node(Node n);void channel(Channel c);void message(Message m);void delivery(String uid,String state);}
    static final class Command {
        final byte[] data;final int response;final String uid;boolean written,replied;long deadline;McAdmin.Edit mcEdit;boolean mcVerification,repeaterAdmin;
        Command(byte[] data,int response,String uid){this.data=data;this.response=response;this.uid=uid;}
    }
    static final class Sent {String uid,peer;long deadline,ack,retainUntil;boolean ackKnown;Sent(String u,String p,long d){uid=u;peer=p;deadline=d;}}
    final McAdmin mcAdmin;
    final McRemoteDiagnostics mcRemote=new McRemoteDiagnostics();
    final McRepeaterAdmin mcRepeaterAdmin=new McRepeaterAdmin();
    final McRepeaterSettings mcRepeaterSettings=new McRepeaterSettings();
    final LinkedHashMap<String,Node> mcDiscoveries=new LinkedHashMap<>();
    final LinkedHashMap<Long,Sent> mcLateSent=new LinkedHashMap<>();
    final Set<String> mcSyncKeys=new HashSet<>(),mcSyncDeleted=new HashSet<>(),mcFailedReads=new HashSet<>(),mcDeferredContacts=new LinkedHashSet<>();
    final Set<Integer> mcDeferredChannels=new TreeSet<>(),mcUnverifiedChannels=new HashSet<>();
    final Map<String,Long> mcLiveContacts=new HashMap<>();
    boolean mcScopeLoaded,mcRepeat,mcRepeatKnown,hasTxPower,mcNameNeedsRepair,mcBacklogDrained,mcFullContacts;int mcExtraAcks,mcManualFlags,mcMaxContacts;long mcRevision;
    byte[] mcSelf=Wire.EMPTY;
    String mcStatus="",mcError="",mcAdvertStatus="";boolean mcAdvertPending;long mcAdvertLast=-30000;
    final RadioProfile profile;final Listener listener;final ProtocolProbe probe;
    final LinkedHashMap<String,Node> nodes=new LinkedHashMap<>();final TreeMap<Integer,Channel> channels=new TreeMap<>();
    final ArrayDeque<Command> commands=new ArrayDeque<>();final LinkedHashMap<Long,Sent> sent=new LinkedHashMap<>();
    final ArrayDeque<byte[]> earlyPackets=new ArrayDeque<>();
    final Map<Integer,byte[]> channelKeys=new HashMap<>();
    final Map<Integer,byte[]> mtConfigs=new HashMap<>(),mtModules=new HashMap<>(),mtChannels=new HashMap<>();
    byte[] mtSessionKey=Wire.EMPTY,mtOwner=Wire.EMPTY;boolean mtAdminRequested,mtOwnerLoaded,mtHasSignatures;int mtHardware;long mtExcludedModules,mtRevision;byte[] mtRegionMap=Wire.EMPTY;final MtAdmin admin;final LinkedHashMap<Long,Long> mtTraces=new LinkedHashMap<>();final Set<Integer> mtRefreshingChannels=new HashSet<>();
    private final LinkedHashMap<String,RxInfo.Observation> observedRoutes=new LinkedHashMap<>();
    private final LinkedHashMap<String,Message> receivedChannels=new LinkedHashMap<>();
    Command active;boolean ready;String description="Synchronizing…",localName="",firmware="",hardware="",build="",publicKey="",defaultFloodScope="";long myNum,lastPoll,lastStamp,lastWrite,uptimeSecs;int hopLimit=3,region,channelCount,nextChannel,contactsExpected,contactsSeen,payloadLimit=173,firmwareCode=-1,maxTxPower=-1,txPower=-1,pathHashMode=-1,batteryMv=-1,usedKb=-1,totalKb=-1,noiseFloor=-999,lastRssi=-999,lastSnrQuarter=0;long txAirSecs,rxAirSecs;double latitude,longitude,frequencyMhz,bandwidthKhz;int spreadingFactor,codingRate,telemetryModes,advertLocationPolicy;boolean txEnabled,pki,waiting,contactsStarted,manualAddContacts,multiAcks,hasLocation,hasRadioStats,hasBattery;
    ProtocolClient(RadioProfile profile,long nonce,Listener listener){this.profile=profile;this.listener=listener;probe=new ProtocolProbe(nonce);admin=new MtAdmin(new MtAdmin.Host(){public void enqueue(byte[] value,long id,boolean response){queue(mtAdminPacket(value,id,response),-1,"");}public void apply(int kind,int index,byte[] body){applyMtAdmin(kind,index,body);}});
        mcAdmin=new McAdmin(new McAdmin.Host(){
            public void enqueue(byte[] data,int response,McAdmin.Edit edit,boolean verification){queue(data,response,"");Command command=commands.peekLast();command.mcEdit=edit;command.mcVerification=verification;}
            public void cancel(McAdmin.Edit edit){commands.removeIf(command->command.mcEdit==edit);}
            public void refresh(){if(ready){requestMcSelf();requestMcDevice();for(int slot:mcUnverifiedChannels){mcFailedReads.remove(mcReadKey(new byte[]{31,(byte)slot}));mcDeferredChannels.add(slot);}}}
        });
        queue(profile==RadioProfile.MESHTASTIC?probe.meshtasticRequest():ProtocolProbe.meshcoreQuery(),profile==RadioProfile.MESHTASTIC?-1:13,"");}
    void cancel(String uid){commands.removeIf(c->c.uid.equals(uid));removeSent(uid);}
    void queue(byte[] data,int response,String uid){if(commands.size()>=32)throw Wire.bad("Radio command queue is full");commands.add(new Command(data,response,uid));}
    /** Expire only the current session's read-only request, never queue a resend. */
    void checkRemoteDeadline(long now){
        if(profile!=RadioProfile.MESHCORE)return;
        long adminVersion=mcRepeaterAdmin.revision;
        mcRepeaterAdmin.tick(now);
        if(adminVersion!=mcRepeaterAdmin.revision){
            if(!mcRepeaterAdmin.pending&&!mcRepeaterAdmin.admin)mcRepeaterSettings.interrupted();
            commands.removeIf(command->{if(!command.repeaterAdmin)return false;Arrays.fill(command.data,(byte)0);return true;});
            mcRevision++;
        }
        long version=mcRemote.revision;
        mcRemote.tick(now);
        if(mcRemote.revision!=version){
            commands.removeIf(command->command.data.length>0
                &&(command.data[0]&255)==McRemoteDiagnostics.CMD_BINARY);
            mcRevision++;
        }
    }
    Command next(long now){
        if(profile==RadioProfile.MESHCORE)checkRemoteDeadline(now);
        if(profile==RadioProfile.MESHTASTIC){admin.tick(now);if(ready)admin.ready();}else{mcAdmin.tick(now);if(ready)mcAdmin.ready();mcLateSent.entrySet().removeIf(e->now>=e.getValue().retainUntil);}
        for(Iterator<Map.Entry<Long,Sent>> it=sent.entrySet().iterator();it.hasNext();){Map.Entry<Long,Sent> entry=it.next();Sent pending=entry.getValue();if(now>=pending.deadline){listener.delivery(pending.uid,"uncertain");if(profile==RadioProfile.MESHCORE&&pending.ackKnown){pending.retainUntil=now+600000;long savedKey=entry.getKey();while(mcLateSent.containsKey(savedKey))savedKey++;mcLateSent.put(savedKey,pending);trim(mcLateSent,128);}it.remove();}}

        if(active!=null){if(now>=active.deadline){if(profile==RadioProfile.MESHCORE&&active.mcEdit!=null)mcAdmin.timeout(active.mcEdit);throw Wire.bad("Radio command timed out • reconnect to restore response ordering");}if(active.written&&active.replied){if(active.repeaterAdmin)Arrays.fill(active.data,(byte)0);active=null;}else return null;}

        if(profile==RadioProfile.MESHCORE&&ready&&(mcAdmin.edit==null||mcAdmin.edit.deadline==Long.MAX_VALUE)&&!mcDeferredContacts.isEmpty()&&commands.size()<8){String id=mcDeferredContacts.iterator().next();mcDeferredContacts.remove(id);requestMcContact(id);}
        if(profile==RadioProfile.MESHCORE&&ready&&(mcAdmin.edit==null||mcAdmin.edit.deadline==Long.MAX_VALUE)&&!mcDeferredChannels.isEmpty()&&commands.size()<8){int slot=mcDeferredChannels.iterator().next();mcDeferredChannels.remove(slot);requestMcChannel(slot);}
        if(profile==RadioProfile.MESHCORE&&ready)advanceRepeaterSettings(now);
        if(profile==RadioProfile.MESHCORE&&ready&&commands.isEmpty()&&(waiting||now-lastPoll>=20000)){waiting=false;lastPoll=now;queue(new byte[]{10},10,"");}
        if(profile==RadioProfile.MESHTASTIC&&ready&&commands.isEmpty()&&now-lastWrite>=30000)queue(new Wire.Out().bytes(7,Wire.EMPTY).array(),-1,"");
        if(commands.isEmpty())return null;active=commands.remove();active.deadline=now+30000;return active;
    }
    /** Called only after GATT accepts a write; delivery and write completion remain unconfirmed. */
    void writeAccepted(byte[] data){
        if(profile==RadioProfile.MESHCORE&&active!=null&&Arrays.equals(active.data,data)&&active.mcEdit!=null&&!active.mcVerification){if((data[0]&255)==32&&data.length>=2)mcUnverifiedChannels.add(data[1]&255);mcAdmin.written(active.mcEdit);}
    }
    McAdmin.Edit pendingMcSave(){return mcAdmin.edit!=null&&mcAdmin.edit.dispatched?mcAdmin.edit:null;}
    void written(long now){lastWrite=now;admin.clock=now;if(profile==RadioProfile.MESHTASTIC&&active!=null){byte[] packet=Wire.bytes(Wire.fields(active.data),1);if(packet.length>0)admin.written(Wire.number(Wire.fields(packet),6,0));}if(active==null)throw Wire.bad("Unexpected write completion");if(profile==RadioProfile.MESHCORE)writeAccepted(active.data);if(!active.uid.isEmpty()&&!active.replied)listener.delivery(active.uid,"submitted");active.written=true;if(active.response<0)active.replied=true;}
    void receive(byte[] data,long now){admin.clock=now;if(profile==RadioProfile.MESHTASTIC)meshtastic(data,now);else meshcore(data,now);}
    private Node node(String id){Node old=nodes.get(id);Node n=old==null?new Node():new Node(old);n.id=id;return n;}
    private void publish(Node n){if(!nodes.containsKey(n.id)&&nodes.size()>=4096)return;nodes.put(n.id,n);listener.node(n);}
    private void user(byte[] data,Node n){List<Wire.F> fs=Wire.fields(data);byte[] name=Wire.bytes(fs,2);if(name.length>0)n.name=Wire.utf8(name);byte[] key=Wire.bytes(fs,8);n.key=key.length==32?Wire.hex(key):"";n.licensed=Wire.number(fs,6,0)!=0;n.unmessagable=Wire.number(fs,9,n.unmessagable?1:0)!=0;n.shortName=Wire.utf8(Wire.bytes(fs,3));n.hardwareModel=(int)Wire.number(fs,5,0);n.role=(int)Wire.number(fs,7,0);}
    private void position(byte[] data,Node n){List<Wire.F> fs=Wire.fields(data);Wire.F lat=Wire.field(fs,1),lon=Wire.field(fs,2);if(lat!=null&&lon!=null){n.lat=(int)lat.number/1e7;n.lon=(int)lon.number/1e7;n.position=Math.abs(n.lat)<=90&&Math.abs(n.lon)<=180;}}
    private void meshtastic(byte[] data,long now){
        List<Wire.F> fs=Wire.fields(data);
        for(Wire.F f:fs){
            if(f.tag==3){myNum=Wire.number(Wire.fields(f.bytes),1,0);}
            else if(f.tag==4){List<Wire.F> ns=Wire.fields(f.bytes);long id=Wire.number(ns,1,0);if(id==0)continue;Node n=node(Long.toString(id));
                for(Wire.F nf:ns){if(nf.tag==2)user(nf.bytes,n);if(nf.tag==3)position(nf.bytes,n);if(nf.tag==5)n.heard=nf.number;if(nf.tag==6){n.battery=(int)Wire.number(Wire.fields(nf.bytes),1,-1);metric(n,"Device",MtMetrics.describe(2,nf.bytes),n.heard);}}publish(n);
            }else if(f.tag==5){applyMtConfig(f.bytes,false);}
            else if(f.tag==9){applyMtConfig(f.bytes,true);}
            else if(f.tag==10){applyMtChannel(f.bytes);}
            else if(f.tag==13){applyMtMetadata(f.bytes);}
            else if(f.tag==19){mtRegionMap=f.bytes;mtRevision++;}
            else if(f.tag==2){if(!ready){if(earlyPackets.size()>=256)throw Wire.bad("Too many packets during synchronization");earlyPackets.add(f.bytes);}else mtPacket(f.bytes,now);}
            else if(f.tag==11){List<Wire.F> qs=Wire.fields(f.bytes);Sent s=sent.get(Wire.number(qs,4,0));if(s!=null){long code=Wire.number(qs,1,0);listener.delivery(s.uid,code==0?"radio accepted":"rejected");if(code!=0)sent.values().remove(s);}}
        }
        probe.meshtastic(data);if(probe.complete){ready=true;description="Ready • live messages";while(!earlyPackets.isEmpty())mtPacket(earlyPackets.remove(),now);}
    }
    private void mtPacket(byte[] bytes,long now){
        List<Wire.F> ps=Wire.fields(bytes);long from=Wire.number(ps,1,0),to=Wire.number(ps,2,0),id=Wire.number(ps,6,0),time=Wire.number(ps,7,0);int slot=(int)Wire.number(ps,3,0);
        byte[] decoded=Wire.bytes(ps,4);if(decoded.length==0)return;List<Wire.F> ds=Wire.fields(decoded);int port=(int)Wire.number(ds,1,0);byte[] payload=Wire.bytes(ds,2);
        if(port==6){MtAdmin.Edit before=admin.edit;admin.response(from,to,myNum,Wire.number(ds,6,0),payload);mtSessionKey=admin.key;if(before!=null&&before.setField==18&&admin.edit==null&&admin.error.isEmpty()){requestMtConfig(5);for(int index=0;index<8;index++){mtRefreshingChannels.add(index);admin.read(2,index);}}return;}
        if(port==5){long request=Wire.number(ds,6,0);Wire.F error=Wire.field(Wire.fields(payload),3);admin.ack(from,to,myNum,request,error==null?0:error.number);Sent s=sent.get(request);
            if(s!=null&&to==myNum&&(from==myNum||Long.toString(from).equals(s.peer))){long code=error==null?0:error.number;boolean delivered=code==0&&from!=myNum&&to==myNum&&s.peer.equals(Long.toString(from));listener.delivery(s.uid,(error!=null&&error.number!=0)?"rejected":delivered?"delivered":"radio accepted");if(delivered||code!=0)sent.remove(request);}return;}
        if(from==0)return;
        if(port==3||port==4||port==36||port==67||port==70||port==71){Node n=node(Long.toString(from));if(port==3){position(payload,n);if(n.position)metric(n,"Position",String.format(Locale.ROOT,"%.7f, %.7f",n.lat,n.lon),time);}else if(port==4)user(payload,n);else if(port==36){n.status=Wire.utf8(Wire.bytes(Wire.fields(payload),1));n.statusKnown=true;}else if(port==67){List<Wire.F> telemetry=Wire.fields(payload);long stamp=Wire.number(telemetry,1,time);for(Wire.F f:telemetry)if(f.wire==2){if(f.tag==2)n.battery=(int)Wire.number(Wire.fields(f.bytes),1,-1);metric(n,MtMetrics.kind(f.tag),MtMetrics.describe(f.tag,f.bytes),stamp);}}else if(port==71){StringBuilder edges=new StringBuilder();for(Wire.F f:Wire.fields(payload))if(f.tag==4){List<Wire.F> edge=Wire.fields(f.bytes);edges.append(String.format(Locale.ROOT,"!%08x",Wire.number(edge,1,0))).append(" SNR ").append(Float.intBitsToFloat((int)Wire.number(edge,2,0))).append(" dB; ");}metric(n,"Neighbors",edges.toString(),time);}else if(port==70){long request=Wire.number(ds,6,0);Long expected=mtTraces.get(request);if(to!=myNum||expected==null||expected!=from)return;mtTraces.remove(request);metric(n,"Traceroute","Outbound: "+mtRoute(payload,1)+"; return: "+mtRoute(payload,3),time);}n.heard=Math.max(n.heard,time);publish(n);return;}
        if(port!=1 || id==0 || payload.length==0 || payload.length>237 || (to!=0xffffffffL&&to!=myNum&&from!=myNum))return;
        if(from==myNum&&sent.containsKey(id)){listener.delivery(sent.get(id).uid,"radio accepted");return;}
        Message m=new Message();m.uid="mt:"+from+":"+id;m.body=Wire.utf8(payload);m.outgoing=from==myNum;m.read=m.outgoing;m.timestamp=time>=1577836800L?time:System.currentTimeMillis()/1000;m.slot=slot;m.sender=nodeName(Long.toString(from));m.group=to==0xffffffffL;
        if(m.group){Channel c=channels.get(slot);if(c==null||!c.enabled){c=new Channel();c.slot=slot;c.name="Channel "+slot;c.identity="unknown";}m.topic=c.topic();m.title=c.name;m.identity=c.identity;}
        else{m.peer=Long.toString(m.outgoing?to:from);m.topic="d:"+m.peer;m.title=nodeName(m.peer);}
        m.rx.packetId=id;m.rx.source=from;m.rx.replyId=Wire.number(ds,7,0);m.rx.emoji=Wire.number(ds,8,0)!=0;m.rx.kind="meshtastic";int start=(int)Wire.number(ps,15,0),remaining=(int)Wire.number(ps,9,0);if(start>0&&start<=7&&remaining>=0&&remaining<=start)m.rx.hops=start-remaining;Wire.F snr=Wire.field(ps,8),rssi=Wire.field(ps,12);if(snr!=null&&snr.wire==5){float value=Float.intBitsToFloat((int)snr.number);if(Float.isFinite(value))m.rx.snr=value;}if(rssi!=null)m.rx.rssi=(int)rssi.number;int relay=(int)Wire.number(ps,19,0);if(relay>0&&relay<=255)m.rx.relay=relay;m.rx.mqtt=Wire.number(ps,14,0)!=0;
        m.state=m.outgoing?"radio accepted":"received";listener.message(m);
    }
    private void metric(Node n,String kind,String value,long time){if(value.isEmpty())return;if(value.length()>1000)value=value.substring(0,1000);n.metrics.put(kind,value);String sample=(time>0?time:System.currentTimeMillis()/1000)+" · "+kind+": "+value;if(n.metricHistory.isEmpty()||!n.metricHistory.get(n.metricHistory.size()-1).equals(sample))n.metricHistory.add(sample);while(n.metricHistory.size()>20)n.metricHistory.remove(0);}
    private String mtRoute(byte[] payload,int field){StringBuilder result=new StringBuilder();for(Wire.F f:Wire.fields(payload))if(f.tag==field){if(f.wire==2){if(f.bytes.length%4!=0)throw Wire.bad("Invalid packed route");for(int i=0;i<f.bytes.length;i+=4){if(result.length()>0)result.append(" → ");result.append(String.format(Locale.ROOT,"!%08x",Wire.u32(f.bytes,i)));}}else if(f.wire==5){if(result.length()>0)result.append(" → ");result.append(String.format(Locale.ROOT,"!%08x",f.number));}}return result.length()==0?"No intermediate nodes":result.toString();}
    void requestMtMetrics(String nodeId,boolean trace){if(!ready||region==0||!txEnabled)throw Wire.bad("Connect and enable LoRa transmission first");long dest=Long.parseLong(nodeId),id=admin.id();Wire.Out data=new Wire.Out().scalar(1,trace?70:67).bytes(2,trace?Wire.EMPTY:new Wire.Out().bytes(2,Wire.EMPTY).array()).scalar(3,1);Wire.Out packet=new Wire.Out().fixed(2,dest).fixed(6,id).bytes(4,data.array()).scalar(9,hopLimit).scalar(10,1);if(trace){if(mtTraces.size()>=8)mtTraces.remove(mtTraces.keySet().iterator().next());mtTraces.put(id,dest);}queue(new Wire.Out().bytes(1,packet.array()).array(),-1,"");}
    private void applyMtMetadata(byte[] body){List<Wire.F> fs=Wire.fields(body);firmware=Wire.utf8(Wire.bytes(fs,1));mtHardware=(int)Wire.number(fs,9,0);hardware=MtLora.hardwareName(mtHardware);pki=Wire.number(fs,11,0)!=0;mtExcludedModules=Wire.number(fs,12,0);mtHasSignatures=Wire.number(fs,14,0)!=0;mtRevision++;}
    private void applyMtConfig(byte[] body,boolean module){for(Wire.F f:Wire.fields(body))if(f.wire==2){if(module){mtModules.put(f.tag,f.bytes);}else{mtConfigs.put(f.tag,f.bytes);if(f.tag==6){List<Wire.F> ls=Wire.fields(f.bytes);region=(int)Wire.number(ls,7,0);hopLimit=(int)Wire.number(ls,8,0);txEnabled=Wire.number(ls,9,0)!=0;}}}mtRevision++;}
    private void applyMtChannel(byte[] body){List<Wire.F> fs=Wire.fields(body);int slot=(int)Wire.number(fs,1,0);if(slot<0||slot>=8)throw Wire.bad("Invalid channel slot");mtChannels.put(slot,body);mtRefreshingChannels.remove(slot);byte[] settings=Wire.bytes(fs,2);Channel c=new Channel();c.slot=slot;c.enabled=Wire.number(fs,3,0)!=0;c.name=Wire.utf8(Wire.bytes(Wire.fields(settings),3));if(c.name.isEmpty())c.name=MtLora.presetName((int)Wire.number(Wire.fields(mtConfig(5)),2,0));c.legacyIdentity=Wire.hash(settings);c.identity="mt-key-"+Wire.hash(Wire.bytes(Wire.fields(settings),2));channels.put(slot,c);listener.channel(c);mtRevision++;}
    private void applyMtAdmin(int kind,int index,byte[] body){if(kind==4){mtOwner=body;mtOwnerLoaded=true;Node n=node(Long.toString(myNum));user(body,n);publish(n);}else if(kind==6||kind==8)applyMtConfig(new Wire.Out().bytes(index+1,body).array(),kind==8);else if(kind==2)applyMtChannel(body);else if(kind==13)applyMtMetadata(body);mtRevision++;}
    private byte[] mtAdminPacket(byte[] value,long id,boolean response){byte[] data=new Wire.Out().scalar(1,6).bytes(2,value).scalar(3,response?1:0).array();byte[] packet=new Wire.Out().fixed(2,myNum).bytes(4,data).fixed(6,id).scalar(9,0).scalar(10,1).array();byte[] frame=new Wire.Out().bytes(1,packet).array();if(frame.length>payloadLimit)throw Wire.bad("Settings exceed the negotiated Bluetooth packet size");return frame;}
    boolean mtAtLeast(int minor,int patch){return MtLora.atLeast(firmware,2,minor,patch);}
    boolean mtModuleSupported(int type){return type>=0&&type<=16&&(mtExcludedModules&(1L<<type))==0&&(type!=13||mtAtLeast(7,20))&&(type<14||mtAtLeast(8,0));}
    void requestMtSessionKey(){admin.read(13,0);}
    void requestMtOwner(){admin.read(4,0);}
    void requestMtConfig(int type){admin.read(6,type);}
    void requestMtModule(int type){if(mtModuleSupported(type))admin.read(8,type);}
    byte[] mtModule(int type){return mtModules.getOrDefault(type+1,Wire.EMPTY);}
    byte[] mtConfig(int type){return mtConfigs.getOrDefault(type+1,Wire.EMPTY);}
    boolean mtConfigFlag(int type,int field){return Wire.number(Wire.fields(mtConfig(type)),field,0)!=0;}
    boolean mtModuleLoaded(int type){return mtModules.containsKey(type+1);}
    boolean mtConfigLoaded(int type){return mtConfigs.containsKey(type+1);}
    private void mtSave(int kind,int index,int field,byte[] before,byte[] after,boolean restart){if(!ready)throw Wire.bad("Connect and synchronize the radio first");byte[] expected=Wire.changes(before,after);if(expected.length==0)return;admin.save(new MtAdmin.Edit(kind,index,field,after,expected,restart));}
    void setMtModule(int type,byte[] body){if(!mtModuleLoaded(type)||!mtModuleSupported(type))throw Wire.bad("Read a supported module before saving");mtSave(8,type,35,mtModule(type),body,type!=2&&type!=13&&type!=16);}
    void setMtModuleField(int type,int field,long value){setMtModule(type,Wire.replace(mtModule(type),field,value));}
    void setMtStatus(String value){setMtModule(13,Wire.replaceBytes(mtModule(13),1,Wire.optionalText(value,80)));}
    String mtStatus(){return Wire.utf8(Wire.bytes(Wire.fields(mtModule(13)),1));}
    void setMtOwner(byte[] owner){if(!mtOwnerLoaded)throw Wire.bad("Wait for the radio identity to load");mtSave(4,0,32,mtOwner,owner,false);}
    void setMtConfig(int type,byte[] config){if(!mtConfigLoaded(type))throw Wire.bad("Read this configuration before saving");if(type==5)MtLora.validate(config,this);mtSave(6,type,34,mtConfig(type),config,type==1||type==3||type==5||type==6||type==7);}
    void setMtConfigField(int type,int field,long value){if(type==7&&field==9&&!mtHasSignatures)throw Wire.bad("This firmware does not support packet signatures");if(type==0&&field==1&&(value==3||value==4||value<0||value>12||value==12&&!mtAtLeast(7,0)))throw Wire.bad("Unsupported device role");byte[] body=Wire.replace(mtConfig(type),field,value);setMtConfig(type,body);}
    void setMtOwnerName(boolean shortName,String value){setMtOwner(Wire.replaceBytes(mtOwner,shortName?3:2,Wire.text(value,shortName?4:mtAtLeast(8,0)?24:39)));}
    boolean mtOwnerIsEmpty(){return !mtOwnerLoaded;}
    String mtOwnerName(){return mtOwnerValue(2);}
    String mtOwnerValue(int field){return Wire.utf8(Wire.bytes(Wire.fields(mtOwner),field));}
    long mtOwnerNumber(int field){return Wire.number(Wire.fields(mtOwner),field,0);}
    boolean mtHam(){Node own=nodes.get(Long.toString(myNum));return mtOwnerLoaded?mtOwnerNumber(6)!=0:own!=null&&own.licensed;}
    String regionName(){return MtLora.regionName(region);}
    void setMtOwnerField(int field,long value){if(field==6&&value!=0)throw Wire.bad("Use the licensed callsign setup");if(field==9&&!mtAtLeast(6,9))throw Wire.bad("Unmessageable requires firmware 2.6.9+");setMtOwner(Wire.replace(mtOwner,field,value));}
    void setMtHam(String callSign,String longName){if(!mtOwnerLoaded||!mtConfigLoaded(5)||mtHam())throw Wire.bad("Load the current identity and LoRa settings first");if(!callSign.matches("[A-Z0-9/]{3,7}"))throw Wire.bad("Enter a 3–7 character callsign");if(!mtAtLeast(8,0)&&!longName.trim().isEmpty())throw Wire.bad("An optional licensed description requires firmware 2.8.0 or newer; use only the callsign on this radio");String composed=longName.trim().isEmpty()?callSign:callSign+"//"+longName;Wire.optionalText(composed,mtAtLeast(8,0)?24:39);List<Wire.F> ls=Wire.fields(mtConfig(5));byte[] params=new Wire.Out().bytes(1,Wire.text(callSign,7)).scalar(2,Wire.number(ls,10,0)).fixed(3,Wire.number(ls,14,0)).bytes(4,Wire.bytes(Wire.fields(mtOwner),3)).bytes(5,Wire.optionalText(longName,14)).array();byte[] expected=new Wire.Out().bytes(2,composed.getBytes(StandardCharsets.UTF_8)).bytes(3,Wire.bytes(Wire.fields(mtOwner),3)).scalar(6,1).array();admin.save(new MtAdmin.Edit(4,0,18,params,expected,true));}
    void setMtConfigFloat(int type,int field,double value){if(!Double.isFinite(value)||value<0||value>2500)throw Wire.bad("Enter a finite frequency between 0 and 2500 MHz");setMtConfig(type,Wire.floatField(mtConfig(type),field,(float)value));}
    void setMtChannelName(int slot,String name){byte[] channel=mtChannels.get(slot);if(channel==null)throw Wire.bad("Read the channel first");byte[] settings=Wire.bytes(Wire.fields(channel),2);byte[] renamed=Wire.replaceBytes(settings,3,Wire.optionalText(name,11));byte[] body=Wire.replaceBytes(channel,2,renamed);mtSave(2,slot,33,channel,body,false);}
    void setMtChannel(int slot,byte[] settings,int role){
        byte[] before=mtChannels.get(slot);if(before==null||slot<0||slot>=8)throw Wire.bad("Read this channel slot before saving");
        if(role<0||role>2||slot==0&&role!=1||slot!=0&&role==1)throw Wire.bad("Keep slot 0 as primary and other slots secondary or disabled");
        MtChannelLink.validate(settings);if(mtHam()&&Wire.bytes(Wire.fields(settings),2).length>0)throw Wire.bad("Licensed mode does not allow encrypted channels");
        byte[] after=Wire.replaceBytes(Wire.replace(Wire.replace(before,1,slot),3,role),2,settings);
        mtSave(2,slot,33,before,after,false);
    }
    String shareMtChannel(int slot){
        if(!ready||admin.edit!=null||!mtRefreshingChannels.isEmpty()||!mtConfigLoaded(5))throw Wire.bad("Wait for channel and LoRa verification before sharing");
        List<byte[]> shared=new ArrayList<>();for(int i:new int[]{0,slot}){if(i==0&&!shared.isEmpty()||i==slot&&slot==0&&!shared.isEmpty())continue;byte[] ch=mtChannels.get(i);if(ch==null||Wire.number(Wire.fields(ch),3,0)==0)throw Wire.bad("Read an enabled primary and selected channel before sharing");shared.add(Wire.bytes(Wire.fields(ch),2));}
        return new MtChannelLink(shared,mtConfig(5)).link();
    }
    String nodeName(String id){Node n=nodes.get(id);if(n!=null&&!n.name.isEmpty())return n.name;return profile==RadioProfile.MESHTASTIC?String.format(Locale.ROOT,"!%08x",Long.parseLong(id)):id.substring(0,Math.min(12,id.length()));}
    static final int MC_DISCOVERED=1,MC_APPROVED=2,MC_RETIRED=3;
    private void mcContact(byte[] data,boolean approved){
        if(data.length<148)throw Wire.bad("Truncated MeshCore contact");String id=Wire.hex(Arrays.copyOfRange(data,1,33));
        Node n=nodes.containsKey(id)?new Node(nodes.get(id)):mcDiscoveries.containsKey(id)?new Node(mcDiscoveries.get(id)):node(id);
        n.key=id;n.type=data[33]&255;n.name=mcName(data,100,32);n.mcAdvert=Wire.u32(data,132);n.mcModified=Wire.u32(data,144);
        n.mcRaw=Wire.hex(Arrays.copyOf(data,148));n.lat=(int)Wire.u32(data,136)/1e6;n.lon=(int)Wire.u32(data,140)/1e6;
        n.position=(n.lat!=0||n.lon!=0)&&Math.abs(n.lat)<=90&&Math.abs(n.lon)<=180;
        Long live=mcLiveContacts.remove(id);if(live!=null)n.mcReceived=Math.max(n.mcReceived,live);
        long epoch=System.currentTimeMillis()/1000;
        n.heard=Math.max(n.mcReceived,mcValidTime(n.mcModified,epoch)?n.mcModified:0);
        if(approved){n.mcState=MC_APPROVED;mcDiscoveries.remove(id);if(!contactsStarted||!mcSyncDeleted.contains(id))publish(n);}
        else{n.mcState=MC_DISCOVERED;mcDiscoveries.put(id,n);trim(mcDiscoveries,4096);listener.node(n);}
        mcRevision++;
    }
    private static boolean mcValidTime(long value,long epoch){return value>=1577836800L&&value<=epoch+300;}
    private static String mcName(byte[] data,int offset,int length){
        int end=offset;while(end<offset+length&&data[end]!=0)end++;
        return new String(Arrays.copyOfRange(data,offset,end),StandardCharsets.UTF_8);
    }
    private void retireMcContact(String id){
        Node n=nodes.remove(id);mcDeferredContacts.remove(id);mcLiveContacts.remove(id);if(contactsStarted)mcSyncDeleted.add(id);
        if(n!=null){n=new Node(n);n.mcState=MC_RETIRED;listener.node(n);}if(mcMaxContacts>0&&nodes.size()<mcMaxContacts){mcFullContacts=false;if(mcError.startsWith("Radio contacts are full"))mcError="";}mcRevision++;
    }
    private void deferMcContact(String id,boolean live){
        if(id.length()!=64)return;
        mcFailedReads.remove(mcReadKey(join(new byte[]{30},Wire.unhex(id))));
        if(live){mcLiveContacts.put(id,System.currentTimeMillis()/1000);if(mcLiveContacts.size()>4096)mcLiveContacts.clear();}
        if(mcDeferredContacts.size()<4096)mcDeferredContacts.add(id);
    }
    private void nextMcChannel(){if(nextChannel<channelCount){description="Synchronizing channel "+(nextChannel+1)+" / "+channelCount;queue(new byte[]{31,(byte)nextChannel++},18,"");}else{ready=true;waiting=true;description="Ready • live messages";mcAdmin.ready();}}
    private void applyMcDevice(byte[] data){
        if(data.length<8||(data[1]&255)<3)throw Wire.bad("MeshCore Companion protocol v3+ required");
        firmwareCode=data[1]&255;mcMaxContacts=(data[2]&255)*2;channelCount=data[3]&255;
        if(data.length>=80){build=Wire.fixedText(data,8,12);hardware=Wire.fixedText(data,20,40);firmware=Wire.fixedText(data,60,20);}
        mcRepeatKnown=data.length>80;mcRepeat=mcRepeatKnown&&data[80]!=0;pathHashMode=data.length>81?data[81]&255:-1;mcRevision++;
    }
    private void applyMcSelf(byte[] data){
        if(data.length<58)throw Wire.bad("Truncated MeshCore identity");mcSelf=data.clone();txPower=data[2];hasTxPower=true;maxTxPower=data[3]&255;
        publicKey=Wire.hex(Arrays.copyOfRange(data,4,36));latitude=(int)Wire.u32(data,36)/1e6;longitude=(int)Wire.u32(data,40)/1e6;hasLocation=latitude!=0||longitude!=0;
        mcExtraAcks=data[44]&255;multiAcks=mcExtraAcks!=0;advertLocationPolicy=data[45]&255;telemetryModes=data[46]&255;mcManualFlags=data[47]&255;manualAddContacts=(mcManualFlags&1)!=0;
        frequencyMhz=Wire.u32(data,48)/1000.0;bandwidthKhz=Wire.u32(data,52)/1000.0;spreadingFactor=data[56]&255;codingRate=data[57]&255;
        localName=mcName(data,58,data.length-58);try{Wire.fixedText(data,58,data.length-58);mcNameNeedsRepair=false;}catch(IllegalArgumentException invalid){mcNameNeedsRepair=true;}
        myNum=1;mcRevision++;
    }
    private void meshcore(byte[] data,long now){
        if(data.length==0||data.length>176)throw Wire.bad("Invalid MeshCore frame");int code=data[0]&255;if(code==136){observeChannel(data,now);return;}
        if(code==0x85||code==0x86){mcRepeaterAdmin.loginResponse(data,now);mcRevision++;return;}
        if(code==McRemoteDiagnostics.PUSH_BINARY){long revision=mcRemote.revision;mcRemote.receive(data,now);if(mcRemote.revision!=revision)mcRevision++;return;}
        Command current=active;int command=current==null?-1:current.data[0]&255;
        boolean matched=current!=null&&(code==current.response||code==1||code==15||(command==4&&code==4)||(command==10&&(code==7||code==8||code==16||code==17||code==27)));
        if(command==31&&code==18)matched=data.length>=2&&data[1]==current.data[1];
        if(command==56&&code==24)matched=data.length>=2&&data[1]==current.data[1];
        if(command==30&&code==3)matched=data.length>=33&&Arrays.equals(Arrays.copyOfRange(data,1,33),Arrays.copyOfRange(current.data,1,33));
        if((code==1||code==15)&&matched){
            current.replied=true;
            if(!current.uid.isEmpty()){listener.delivery(current.uid,"rejected");removeSent(current.uid);return;}
            if(current.data[0]==7){mcAdvertPending=false;mcAdvertLast=-30000;mcError="Radio rejected the announcement";mcAdvertStatus=mcError;mcRevision++;return;}
            if(command==30&&data.length>1&&data[1]==2){
                String id=Wire.hex(Arrays.copyOfRange(current.data,1,33));boolean newerAdvert=mcDeferredContacts.contains(id);retireMcContact(id);
                mcFailedReads.remove(mcReadKey(current.data));
                if(newerAdvert)deferMcContact(id,false);
                if(current.mcEdit!=null){mcAdmin.clock=now;mcAdmin.reply(current.mcEdit,current.mcVerification,data);return;}
                mcStatus=mcDiscoveries.containsKey(id)?"Discovery checked — approval required":"Contact is no longer in the radio contact list";mcRevision++;return;
            }
            if(current.mcEdit!=null){mcAdmin.clock=now;mcAdmin.reply(current.mcEdit,current.mcVerification,data);return;}
            if(current.repeaterAdmin){mcRepeaterAdmin.rejected();mcRepeaterSettings.interrupted();mcRevision++;return;}
            if(command==McRemoteDiagnostics.CMD_BINARY){mcRemote.rejected(data.length>1?data[1]&255:code);mcRevision++;return;}
            if(command==31&&data.length>=2&&data[1]==2&&!ready){nextMcChannel();return;}
            if(!ready)throw Wire.bad("MeshCore rejected bootstrap command "+command);
            mcFailedReads.add(mcReadKey(current.data));mcError="Radio rejected command "+command+" (code "+(data.length>1?data[1]&255:code)+")";mcRevision++;return;
        }
        if(code==13&&matched){applyMcDevice(data);if(!ready)queue(ProtocolProbe.meshcoreAppStart(),5,"");}
        else if(code==5&&matched){applyMcSelf(data);if(!ready){refreshContacts();description="Synchronizing MeshCore contacts…";}}
        else if(code==2&&command==4){
            if(data.length<5)throw Wire.bad("Invalid contacts header");contactsExpected=(int)Wire.u32(data,1);if(contactsExpected<0||contactsExpected>4096)throw Wire.bad("Too many contacts");
            contactsSeen=0;contactsStarted=true;mcSyncKeys.clear();mcSyncDeleted.clear();
        }
        else if(code==3&&((command==4&&contactsStarted)||matched)){
            mcContact(data,true);
            if(command==30){mcStatus="Radio contact synchronized";if(mcError.startsWith("Radio rejected command 30 "))mcError="";}
            if(command==4){contactsSeen++;mcSyncKeys.add(Wire.hex(Arrays.copyOfRange(data,1,33)));description="MeshCore contacts "+contactsSeen+" / "+contactsExpected;current.deadline=now+30000;}
        }
        else if(code==138){
            if(data.length<148)throw Wire.bad("Truncated MeshCore discovery");String id=Wire.hex(Arrays.copyOfRange(data,1,33));
            if(!nodes.containsKey(id)){mcContact(data,false);Node n=mcDiscoveries.get(id);n.mcReceived=System.currentTimeMillis()/1000;n.heard=n.mcReceived;listener.node(n);}
            // NEW_ADVERT is shared by successful auto-add and unapproved discovery. Read actual membership.
            deferMcContact(id,true);
        }
        else if(code==143&&data.length>=33)retireMcContact(Wire.hex(Arrays.copyOfRange(data,1,33)));
        else if(code==144){mcFullContacts=true;mcError="Radio contacts are full. Review contacts before approving more devices.";mcRevision++;}
        else if(code==4&&matched){
            if(!contactsStarted||contactsSeen!=contactsExpected)throw Wire.bad("Incomplete contacts sync");contactsStarted=false;
            for(String id:new ArrayList<>(nodes.keySet()))if(!mcSyncKeys.contains(id))retireMcContact(id);
            mcSyncDeleted.clear();description="Ready • live messages";
            if(!ready)nextMcChannel();else{mcStatus="Radio contacts refreshed";mcRevision++;}
        }
        else if(code==18&&matched){
            if(data.length<50)throw Wire.bad("Truncated MeshCore channel");Channel channel=new Channel();channel.slot=data[1]&255;channel.name=mcName(data,2,32);byte[] secret=Arrays.copyOfRange(data,34,50);
            channelKeys.put(channel.slot,secret);mcUnverifiedChannels.remove(channel.slot);channel.identity=Wire.hash(secret);channel.enabled=!channel.name.isEmpty();channels.put(channel.slot,channel);listener.channel(channel);if(!ready)nextMcChannel();else mcRevision++;
        }
        else if(code==0&&matched&&command==7){mcAdvertPending=false;mcStatus="Radio accepted the announcement; reception by others is not confirmed";mcAdvertStatus=mcStatus;mcRevision++;}
        else if(code==0&&matched&&!current.uid.isEmpty()){listener.delivery(current.uid,"radio accepted");removeSent(current.uid);}
        else if(code==6&&matched){
            if(data.length<10)throw Wire.bad("Truncated send acknowledgement");
            if(current.repeaterAdmin){mcRepeaterAdmin.accepted(data,now);mcRevision++;}
            if(command==McRemoteDiagnostics.CMD_BINARY){mcRemote.sentToRadio(data,now);mcRevision++;}
            for(Sent pending:sent.values())if(pending.uid.equals(current.uid)){pending.ack=Wire.u32(data,2);pending.ackKnown=true;pending.deadline=now+Math.max(5000,Math.min(600000,Wire.u32(data,6)));listener.delivery(pending.uid,"radio accepted");}
        }
        else if(code==12&&matched){if(data.length<3)throw Wire.bad("Truncated battery response");batteryMv=(int)Wire.u16(data,1);hasBattery=true;if(data.length>=11){usedKb=(int)Wire.u32(data,3);totalKb=(int)Wire.u32(data,7);}mcRevision++;}
        else if(code==24&&matched){
            if(data.length>=14&&data[1]==1){noiseFloor=(short)Wire.u16(data,2);lastRssi=data[4];lastSnrQuarter=data[5];txAirSecs=Wire.u32(data,6);rxAirSecs=Wire.u32(data,10);hasRadioStats=true;}
            else if(data.length>=11&&data[1]==0){batteryMv=(int)Wire.u16(data,2);uptimeSecs=Wire.u32(data,4);hasBattery=true;}else throw Wire.bad("Truncated radio statistics");mcRevision++;
        }
        else if(code==28&&matched){if(data.length!=1&&data.length<48)throw Wire.bad("Truncated flood scope");defaultFloodScope=data.length>1?Wire.fixedText(data,1,31):"";mcScopeLoaded=true;mcRevision++;}
        else if(code==7||code==8||code==16||code==17){mcMessage(data,now);waiting=true;}
        else if(code==27&&matched){mcStatus="Skipped an unsupported channel data item; continuing message synchronization";waiting=true;mcRevision++;}
        else if(code==10&&matched)mcBacklogDrained=true;
        else if(code==130){
            if(data.length<9)throw Wire.bad("Truncated delivery ACK");long ack=Wire.u32(data,1);Sent found=null;int matches=0;
            for(Sent pending:sent.values())if(pending.ackKnown&&pending.ack==ack){found=pending;matches++;}
            for(Sent pending:mcLateSent.values())if(pending.ackKnown&&pending.ack==ack){found=pending;matches++;}
            if(matches==1){listener.delivery(found.uid,"delivered");removeSent(found.uid);}
        }
        else if(code==131)waiting=true;
        else if((code==128||code==129)&&data.length>=33)deferMcContact(Wire.hex(Arrays.copyOfRange(data,1,33)),code==128);
        if(matched){current.replied=true;if(current.mcEdit!=null){mcAdmin.clock=now;mcAdmin.reply(current.mcEdit,current.mcVerification,data);}else if(ready){mcFailedReads.remove(mcReadKey(current.data));}}
    }
    private void mcMessage(byte[] data,long now){
        int code=data[0]&255;boolean group=code==8||code==17,v3=code==16||code==17;int start=v3?4:1,meta=group?3:8,typepos=start+(group?2:7);
        if(data.length<start+meta+4)throw Wire.bad("Truncated MeshCore message");int type=data[typepos]&255,offset=start+meta+4+(type==2?4:0);if(!group&&(type==0||type==1)){
            String prefix=Wire.hex(Arrays.copyOfRange(data,start,start+6));
            boolean repeaterReply=type==1;
            for(Node known:nodes.values())if(RepeaterInventory.isRepeater(known)&&known.id.startsWith(prefix))repeaterReply=true;
            if(repeaterReply){if(mcRepeaterAdmin.cliResponse(data,start,offset,now))mcRepeaterSettings.received(mcRepeaterAdmin.reply,now);mcRevision++;return;}
        }if(type!=0&&type!=2)return;if(offset>data.length)throw Wire.bad("Truncated signed message");
        Message m=new Message();m.rx=RxInfo.meshcore(data[start+(group?1:6)]&255);if(v3)m.rx.snr=data[1]/4.0;m.group=group;m.timestamp=Wire.u32(data,start+meta);if(m.timestamp<1577836800L)m.timestamp=System.currentTimeMillis()/1000;m.body=Wire.utf8(Arrays.copyOfRange(data,offset,data.length));if(m.body.isEmpty())return;
        if(group){m.slot=data[start]&255;Channel c=channels.get(m.slot);if(c==null||!c.enabled)return;m.topic=c.topic();m.title=c.name;m.identity=c.identity;int colon=m.body.indexOf(": ");m.sender=colon>0&&colon<40?m.body.substring(0,colon):"Channel participant";}
        else{String prefix=Wire.hex(Arrays.copyOfRange(data,start,start+6));Node found=null;int count=0;for(Node n:nodes.values())if(n.id.startsWith(prefix)){found=n;count++;}m.peer=count==1?found.id:prefix;m.topic="d:"+prefix;m.title=count==1?nodeName(m.peer):prefix;m.sender=m.title;if(count==1){if(mcBacklogDrained){Node live=new Node(found);live.mcReceived=System.currentTimeMillis()/1000;live.heard=Math.max(live.heard,live.mcReceived);publish(live);}deferMcContact(found.id,false);}}
        // SNR/path metadata can change when the same radio backlog packet is replayed.
        m.uid="mc:"+Wire.hash((m.topic+"|"+Wire.u32(data,start+meta)+"|"+type+"|"+(type==2?Wire.hex(Arrays.copyOfRange(data,start+meta+4,offset)):"")+"|"+m.body).getBytes(StandardCharsets.UTF_8));if(group){String key=routeKey(m.slot,Wire.u32(data,start+meta),type,m.body);RxInfo.Observation observation=observedRoutes.get(key);if(observation!=null&&now-observation.at<=120000)m.rx=RxInfo.merge(m.rx,observation.info);receivedChannels.put(key,m);trim(receivedChannels,256);}listener.message(m);
    }
    private String routeKey(int slot,long time,int type,String body){Channel c=channels.get(slot);return (c==null?"":c.topic())+"|"+time+"|"+type+"|"+body;}
    private static void trim(LinkedHashMap<?,?> map,int maximum){while(map.size()>maximum)map.remove(map.keySet().iterator().next());}
    private void observeChannel(byte[] data,long now){
        observedRoutes.entrySet().removeIf(entry->now-entry.getValue().at>120000);
        for(Map.Entry<Integer,byte[]> key:channelKeys.entrySet()){Channel channel=channels.get(key.getKey());if(channel==null||!channel.enabled)continue;RxInfo.Observation observation=RxInfo.channelLog(data,key.getValue(),now);if(observation==null)continue;String match=routeKey(key.getKey(),observation.time,observation.type,observation.body);observedRoutes.putIfAbsent(match,observation);trim(observedRoutes,256);Message message=receivedChannels.get(match);if(message!=null&&!message.rx.observed){message.rx=RxInfo.merge(message.rx,observation.info);listener.message(message);}}
    }
    private void removeSent(String uid){sent.values().removeIf(s->s.uid.equals(uid));mcLateSent.values().removeIf(s->s.uid.equals(uid));}
    private static String mcReadKey(byte[] data){return Wire.hex(data);}
    private boolean mcRead(byte[] data,int response){
        if(mcFailedReads.contains(mcReadKey(data)))return false;
        if(active!=null&&!active.replied&&Arrays.equals(active.data,data))return true;
        for(Command command:commands)if(Arrays.equals(command.data,data))return true;
        queue(data,response,"");return true;
    }
    void retryMcReads(){mcFailedReads.clear();mcError="";mcRevision++;}
    /** Remote read-only requests are explicit, serialized and session-scoped. */
    void requestMcRepeater(McRemoteDiagnostics.Kind kind,String key,int offset,long now){
        if(profile!=RadioProfile.MESHCORE||!ready||firmwareCode<10)
            throw Wire.bad("Remote diagnostics require a Ready MeshCore Companion radio (v10+)");
        Node node=nodes.get(key);
        if(node==null||node.type!=2||node.mcState!=MC_APPROVED)
            throw Wire.bad("Select an approved repeater from the current radio contacts");
        if(mcRepeaterAdmin.pending||mcAdmin.edit!=null||commands.size()>=8)
            throw Wire.bad("Radio is busy; finish pending operations before querying repeaters");
        byte[] packet=mcRemote.request(kind,node,offset,now);
        queue(packet,McRemoteDiagnostics.RESP_SENT,"");
        mcRevision++;
    }

    void administerRepeater(String key,String value,boolean login,long now){
        if(profile!=RadioProfile.MESHCORE||!ready||!mcBacklogDrained)
            throw Wire.bad("Connect and finish message synchronization first");
        if(mcRemote.pending!=null||mcAdmin.edit!=null||commands.size()>=8)
            throw Wire.bad("Finish the pending radio operation first");
        Node node=nodes.get(key);
        if(login)mcRepeaterSettings.resetLogin(key);
        byte[] packet=login?mcRepeaterAdmin.login(node,nodes.values(),value,now,payloadLimit):
            mcRepeaterAdmin.command(node,nodes.values(),value,now,System.currentTimeMillis()/1000,payloadLimit);
        queue(packet,6,"");commands.peekLast().repeaterAdmin=true;mcRevision++;
    }

    private void advanceRepeaterSettings(long now){
        if(active!=null||!commands.isEmpty()||!mcBacklogDrained||mcAdmin.edit!=null||mcRemote.pending!=null||mcRepeaterAdmin.pending
                ||!mcRepeaterAdmin.authorized(mcRepeaterSettings.key,now))return;
        McRepeaterSettings.Operation op=mcRepeaterSettings.next(now);
        if(op==null)return;
        try{administerRepeater(op.key,op.command,false,now);mcRepeaterSettings.dispatched(op);}
        catch(IllegalArgumentException error){mcRepeaterSettings.interrupted();mcError=error.getMessage();mcRevision++;}
    }
    void saveRepeaterSetting(String key,McRepeaterSettings.Field field,String value,long now){
        if(!mcRepeaterSettings.visible||!mcRepeaterSettings.key.equals(key))throw Wire.bad("Reopen this repeater's settings");
        McRepeaterSettings.Operation op=mcRepeaterSettings.edit(field,value,now);
        administerRepeater(key,op.command,false,now);
        mcRepeaterSettings.dispatched(op);
    }

    void clearRepeaterAdministration(){
        if(active!=null&&active.repeaterAdmin)Arrays.fill(active.data,(byte)0);
        for(Command command:commands)if(command.repeaterAdmin)Arrays.fill(command.data,(byte)0);
        mcRepeaterAdmin.forget();mcRepeaterSettings.clear();
    }

    boolean mcSupportsStats(){return firmwareCode>=8;}
    boolean mcSupportsScope(){return firmwareCode>=11;}
    boolean mcSupportsPath(){return firmwareCode>=10;}
    boolean mcSupportsOther(){return firmwareCode>=5;}
    void requestMcSelf(){mcRead(ProtocolProbe.meshcoreAppStart(),5);}
    void requestMcDevice(){mcRead(ProtocolProbe.meshcoreQuery(),13);}
    void requestMcContact(String id){mcRead(join(new byte[]{30},Wire.unhex(id)),3);}
    private void mcSettingsReady(){if(profile!=RadioProfile.MESHCORE||!ready||mcSelf.length<58)throw Wire.bad("Connect and synchronize the MeshCore radio first");if(mcAdmin.edit!=null)throw Wire.bad("Wait for the pending MeshCore change to be verified");}
    private McAdmin.Check mcSelfCheck(int offset,byte[] expected,String label){return new McAdmin.Check(ProtocolProbe.meshcoreAppStart(),5,offset,expected,label);}
    private McAdmin.Check mcDeviceCheck(int offset,byte[] expected,String label){return new McAdmin.Check(ProtocolProbe.meshcoreQuery(),13,offset,expected,label);}
    private void mcSave(String title,byte[][] writes,McAdmin.Check... checks){mcSettingsReady();mcError="";List<McAdmin.Check> all=new ArrayList<>(Arrays.asList(checks));all.add(mcSelfCheck(4,Wire.unhex(publicKey),"Radio identity"));mcAdmin.save(new McAdmin.Edit(title,writes,all.toArray(new McAdmin.Check[0])));}
    void announceSelf(boolean flood){
        mcSettingsReady();if(mcAdvertPending)throw Wire.bad("An announcement is already pending");if(mcAdmin.clock-mcAdvertLast<30000)throw Wire.bad("Wait 30 seconds before announcing again");
        queue(new byte[]{7,(byte)(flood?1:0)},0,"");mcAdvertLast=mcAdmin.clock;mcAdvertPending=true;mcError="";mcStatus="Announcement queued — waiting for the radio";mcAdvertStatus=mcStatus;mcRevision++;
    }
    void refreshMcChannels(){mcSettingsReady();for(int slot=0;slot<channelCount;slot++){mcFailedReads.remove(mcReadKey(new byte[]{31,(byte)slot}));mcDeferredChannels.add(slot);}mcRevision++;}
    void requestMcChannel(int slot){if(slot<0||slot>=channelCount)throw Wire.bad("Invalid channel slot");mcRead(new byte[]{31,(byte)slot},18);}
    boolean mcChannelChanging(int slot){
        if(mcUnverifiedChannels.contains(slot))return true;
        if(mcAdmin.edit!=null)for(McAdmin.Check check:mcAdmin.edit.checks)if(check.response==18&&check.request.length==2&&(check.request[0]&255)==31&&(check.request[1]&255)==slot)return true;
        return false;
    }
    void setMcChannel(int slot,String name,byte[] secret){
        mcSettingsReady();if(mcChannelChanging(slot))throw Wire.bad("Refresh this channel before editing it again");if(slot<0||slot>=channelCount||!channels.containsKey(slot))throw Wire.bad("Read this channel slot before editing it");
        if(name==null)throw Wire.bad("Enter a channel name");byte[] title=name.isEmpty()?Wire.EMPTY:Wire.text(name,31);if(secret==null||secret.length!=16)throw Wire.bad("Channel key must contain 16 bytes");
        boolean zero=true;for(byte b:secret)if(b!=0)zero=false;if(name.isEmpty()!=zero)throw Wire.bad("An empty channel needs both an empty name and an all-zero key");
        if(!name.isEmpty())for(Channel other:channels.values())if(other.slot!=slot&&other.enabled&&Arrays.equals(channelKeys.get(other.slot),secret))throw Wire.bad("This channel key is already configured in slot "+other.slot);
        byte[] frame=new byte[50];frame[0]=32;frame[1]=(byte)slot;System.arraycopy(title,0,frame,2,title.length);System.arraycopy(secret,0,frame,34,16);
        mcSave(name.isEmpty()?"channel removal":"channel settings",new byte[][]{frame},new McAdmin.Check(new byte[]{31,(byte)slot},18,1,Arrays.copyOfRange(frame,1,50),"Channel name and key"));mcUnverifiedChannels.add(slot);mcDeferredChannels.remove(slot);
    }
    String shareMcChannel(int slot){
        mcSettingsReady();if(mcChannelChanging(slot))throw Wire.bad("Refresh this channel before sharing it");Channel channel=channels.get(slot);byte[] key=channelKeys.get(slot);if(channel==null||!channel.enabled||key==null)throw Wire.bad("Read an enabled channel before sharing it");
        if(mcSupportsScope()&&!mcScopeLoaded){requestDefaultFloodScope();throw Wire.bad("Wait for the default region to load before sharing");}
        return new McChannelLink(channel.name,key,mcSupportsScope()?defaultFloodScope:"").link();
    }
    void setName(String value){byte[] name=Wire.text(value,31);mcSave("node name",new byte[][]{join(new byte[]{8},name)},mcSelfCheck(58,name,"Node name"));}
    void setLocation(double lat,double lon){
        if(!Double.isFinite(lat)||!Double.isFinite(lon)||Math.abs(lat)>90||Math.abs(lon)>180)throw Wire.bad("Enter finite coordinates within latitude ±90 and longitude ±180");
        byte[] frame=new byte[9];frame[0]=14;put32(frame,1,(int)Math.round(lat*1e6));put32(frame,5,(int)Math.round(lon*1e6));
        mcSave("location",new byte[][]{frame},mcSelfCheck(36,Arrays.copyOfRange(frame,1,9),"Location"));
    }
    void setRadio(double freq,double bw,int sf,int cr){setRadioAndPath(freq,bw,sf,cr,-1);}
    void setRadioAndPath(double freq,double bw,int sf,int cr,int pathMode){
        mcSettingsReady();
        if(!Double.isFinite(freq)||!Double.isFinite(bw)||freq<150||freq>2500||bw<7||bw>500||sf<5||sf>12||cr<5||cr>8)throw Wire.bad("Enter finite radio values: 150–2500 MHz, BW 7–500 kHz, SF 5–12 and CR 5–8");
        if(pathMode< -1||pathMode>2||pathMode>=0&&!mcSupportsPath())throw Wire.bad("Path hash size requires Companion protocol v10+");
        if(firmwareCode>=9&&!mcRepeatKnown){requestMcDevice();throw Wire.bad("Read the radio repeat mode before changing radio parameters");}
        byte[] frame=new byte[firmwareCode>=9?12:11];frame[0]=11;put32(frame,1,(int)Math.round(freq*1000));put32(frame,5,(int)Math.round(bw*1000));frame[9]=(byte)sf;frame[10]=(byte)cr;
        List<byte[]> writes=new ArrayList<>();List<McAdmin.Check> checks=new ArrayList<>();writes.add(frame);checks.add(mcSelfCheck(48,Arrays.copyOfRange(frame,1,11),"Radio parameters"));
        if(frame.length==12){frame[11]=(byte)(mcRepeat?1:0);checks.add(mcDeviceCheck(80,new byte[]{frame[11]},"Repeat mode"));}
        if(pathMode>=0){writes.add(new byte[]{61,0,(byte)pathMode});checks.add(mcDeviceCheck(81,new byte[]{(byte)pathMode},"Path hash size"));}
        mcSave("radio settings",writes.toArray(new byte[0][]),checks.toArray(new McAdmin.Check[0]));
    }
    void setTxPower(int dbm){if(!hasTxPower||maxTxPower<0)throw Wire.bad("Read the radio power limits first");if(dbm< -9||dbm>maxTxPower)throw Wire.bad("TX power must be between -9 and "+maxTxPower+" dBm");mcSave("TX power",new byte[][]{new byte[]{12,(byte)dbm}},mcSelfCheck(2,new byte[]{(byte)dbm},"TX power"));}
    void setPathHashMode(int mode){if(!mcSupportsPath()||mode<0||mode>2)throw Wire.bad("Path hash size requires Companion protocol v10+ and 1–3 bytes");mcSave("path hash size",new byte[][]{new byte[]{61,0,(byte)mode}},mcDeviceCheck(81,new byte[]{(byte)mode},"Path hash size"));}
    void setOtherParams(boolean manual,int telemetry,int locationPolicy,boolean acks){
        if(!mcSupportsOther())throw Wire.bad("Privacy/contact parameters require Companion protocol v5+");
        if(telemetry<0||telemetry>255||locationPolicy<0||locationPolicy>1)throw Wire.bad("Invalid privacy parameters");
        int count=acks==multiAcks?mcExtraAcks:acks?1:0,flags=(mcManualFlags&254)|(manual?1:0);
        byte[] frame={38,(byte)flags,(byte)telemetry,(byte)locationPolicy,(byte)count};
        mcSave("privacy/contact settings",new byte[][]{frame},mcSelfCheck(44,new byte[]{(byte)count,(byte)locationPolicy,(byte)telemetry,(byte)flags},"Privacy/contact settings"));
    }
    void setDeviceTime(long epoch){
        if(epoch<1||epoch>0xffffffffL)throw Wire.bad("Invalid radio clock value");byte[] frame=new byte[5];frame[0]=6;put32(frame,1,(int)epoch);
        mcSave("radio clock",new byte[][]{frame},new McAdmin.Check(new byte[]{5},9,1,Arrays.copyOfRange(frame,1,5),"Radio clock",false,true));
    }
    void refreshContacts(){mcRead(new byte[]{4},4);}
    boolean requestStats(int type){return mcSupportsStats()&&type>=0&&type<=1&&mcRead(new byte[]{56,(byte)type},24);}
    void requestBattery(){mcRead(new byte[]{20},12);}
    void reboot(){if(mcAdmin.edit!=null)throw Wire.bad("Verify the pending change before rebooting");queue(join(new byte[]{19},"reboot".getBytes(StandardCharsets.UTF_8)),-1,"");}
    boolean requestDefaultFloodScope(){return mcSupportsScope()&&mcRead(new byte[]{64},28);}
    void setDefaultFloodScope(String name,byte[] key){
        if(!mcSupportsScope())throw Wire.bad("Default flood scope requires Companion protocol v11+");if(key.length!=16)throw Wire.bad("Region key must contain exactly 16 bytes");
        byte[] frame=new byte[48];frame[0]=63;byte[] text=Wire.text(name,30);System.arraycopy(text,0,frame,1,text.length);System.arraycopy(key,0,frame,32,16);
        mcSave("default flood region",new byte[][]{frame},new McAdmin.Check(new byte[]{64},28,1,Arrays.copyOfRange(frame,1,48),"Default flood region"));
    }
    void clearDefaultFloodScope(){if(!mcSupportsScope())throw Wire.bad("Default flood scope requires Companion protocol v11+");mcSave("default flood region",new byte[][]{new byte[]{63}},new McAdmin.Check(new byte[]{64},28,1,Wire.EMPTY,"Default flood region"));}
    void approveMcContact(Node candidate){
        mcSettingsReady();byte[] raw=Wire.unhex(candidate.mcRaw);
        if(raw.length!=148||candidate.id.length()!=64||!candidate.id.equals(Wire.hex(Arrays.copyOfRange(raw,1,33))))throw Wire.bad("Wait for a fresh discovery before approving this contact");
        byte[] frame=raw.clone();frame[0]=9;byte[] get=join(new byte[]{30},Wire.unhex(candidate.id));
        mcSave("contact approval",new byte[][]{frame},new McAdmin.Check(get,3,1,Wire.unhex(candidate.id),"Approved contact"));mcDeferredContacts.remove(candidate.id);
    }
    void removeMcContact(String id){
        if(!nodes.containsKey(id))throw Wire.bad("This contact is not currently approved on the radio");byte[] key=Wire.unhex(id),get=join(new byte[]{30},key);
        mcSave("contact removal",new byte[][]{join(new byte[]{15},key)},new McAdmin.Check(get,3,1,key,"Removed contact",true,false));mcDeferredContacts.remove(id);
    }
    List<Sent> mcRetainDeliveries(long now){
        List<Sent> retained=new ArrayList<>();for(Sent value:sent.values())if(value.ackKnown){value.retainUntil=now+600000;retained.add(value);}for(Sent value:mcLateSent.values())if(value.retainUntil>now)retained.add(value);return retained;
    }
    void mcRestoreDeliveries(List<Sent> retained,long now){long key=1;for(Sent value:retained)if(value.ackKnown&&value.retainUntil>now){mcLateSent.put(key++,value);}trim(mcLateSent,128);}
    private static byte[] join(byte[] a,byte[] b){byte[] out=Arrays.copyOf(a,a.length+b.length);System.arraycopy(b,0,out,a.length,b.length);return out;}
    private static void put32(byte[] f,int i,int value){f[i]=(byte)value;f[i+1]=(byte)(value>>>8);f[i+2]=(byte)(value>>>16);f[i+3]=(byte)(value>>>24);}
    Message send(Target target,String text,long id,long now){
        if(!ready)throw Wire.bad("Wait for this radio to finish synchronization");if(sent.size()>=32||commands.size()>=20)throw Wire.bad("Wait for pending transmissions");
        if(profile==RadioProfile.MESHCORE&&target.group&&mcChannelChanging(target.slot))throw Wire.bad("Wait for this channel change to be verified or refresh its current values");
        Channel c=channels.get(target.slot);Node n=nodes.get(target.peer);
        if(target.group&&(c==null||!c.enabled||!c.identity.equals(target.identity)))throw Wire.bad("Channel changed • select it again in Groups");
        if(!target.group&&(n==null||n.unmessagable))throw Wire.bad("Select a synchronized, message-capable node");
        Message m=new Message();m.topic=target.topic;m.title=target.title;m.peer=target.peer;m.identity=target.identity;m.slot=target.slot;m.group=target.group;m.body=text;m.outgoing=m.read=true;m.timestamp=System.currentTimeMillis()/1000;m.sender="You";m.state="queued";
        byte[] frame;int response=-1;
        if(profile==RadioProfile.MESHTASTIC){
            if(admin.edit!=null&&(admin.edit.kind==2||admin.edit.kind==6&&admin.edit.index==5))throw Wire.bad("Wait for channel or LoRa verification before sending");
            if(!mtRefreshingChannels.isEmpty())throw Wire.bad("Wait for the radio channels to synchronize after licensed setup");
            if(region==0||!txEnabled)throw Wire.bad("Set LoRa region and enable TX using the official Meshtastic app first");
            if(!target.group&&(channels.get(0)==null||!channels.get(0).enabled))throw Wire.bad("Primary channel is disabled or not synchronized");
            if(!target.group&&Long.parseLong(target.peer)==myNum)throw Wire.bad("Choose another node");
            boolean usePki=!target.group&&pki&&!mtHam()&&!n.licensed;
            if(usePki&&(n.key.length()!=64))throw Wire.bad("Wait for this node's public key before sending a DM");
            byte[] body=Wire.text(text,220);Wire.Out decoded=new Wire.Out().scalar(1,1).bytes(2,body);if(target.replyId!=0)decoded.fixed(7,target.replyId);if(target.reaction){if(target.replyId==0)throw Wire.bad("Select a message to react to");decoded.fixed(8,1);}m.rx.packetId=id;m.rx.source=myNum;m.rx.replyId=target.replyId;m.rx.emoji=target.reaction;
            Wire.Out packet=new Wire.Out().fixed(2,target.group?0xffffffffL:Long.parseLong(target.peer)).scalar(3,target.group?target.slot:0).bytes(4,decoded.array()).fixed(6,id).scalar(9,hopLimit).scalar(10,target.group?0:1).scalar(11,target.group?64:70);
            if(usePki)packet.bytes(16,Wire.unhex(n.key)).scalar(17,1);
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
