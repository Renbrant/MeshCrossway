package com.meshcrossway.android;

import java.util.*;
import java.util.concurrent.atomic.AtomicLong;
import static com.meshcrossway.android.MeshData.*;

public final class FieldFeaturesTest {
    static void check(boolean b,String message){if(!b)throw new AssertionError(message);}
    static Message message(String title,long time){Message m=new Message();m.uid=title;m.topic="c:0:samehash";m.title=title;m.timestamp=time;m.body="hello";return m;}
    public static void main(String[] args)throws Exception{
        check(RadioProfile.MESHCORE.supportsMtu(176),"ESP32 MeshCore MTU 176 must connect");
        check(RadioProfile.MESHCORE.payloadLimit(176)==173,"MTU 176 reserves ATT overhead");
        check(RadioProfile.MESHCORE.supportsMtu(185)&&RadioProfile.MESHCORE.payloadLimit(185)==176,"larger MTU preserves protocol frame cap");
        check(!RadioProfile.MESHCORE.supportsMtu(23)&&!RadioProfile.MESHCORE.supportsMtu(175),"small MTU cannot carry full notifications");
        check(RadioProfile.MESHTASTIC.supportsMtu(23)&&RadioProfile.MESHTASTIC.payloadLimit(23)==20,"Meshtastic minimum unchanged");
        check(Math.abs(GeoMath.distance(0,0,0,1)-111195)<2,"great-circle distance");check(Math.abs(GeoMath.distance(0,179,0,-179)-222390)<3,"antimeridian distance");check(Double.isInfinite(GeoMath.distance(91,0,0,0)),"invalid coordinates rejected");
        check(GeoMath.tileEstimate(40,-105,40.01,-104.99,13)<GeoMath.MAX_TILES,"small offline area allowed");check(GeoMath.tileEstimate(-80,-170,80,170,15)==Long.MAX_VALUE,"world-sized area rejected");check(GeoMath.tileEstimate(10,179,11,-179,12)==Long.MAX_VALUE,"crossing selection rejected safely");check(GeoMath.tileEstimate(40,-105,40.01,-104.99,14)>=GeoMath.tileEstimate(40,-105,40.01,-104.99,10),"zoom estimate monotonic");
        TrafficBudget budget=new TrafficBudget(1000);AtomicLong received=new AtomicLong();List<Thread> workers=new ArrayList<>();for(int i=0;i<8;i++){Thread t=new Thread(()->{long grant;while((grant=budget.reserve(17))>0)received.addAndGet(grant);});workers.add(t);t.start();}for(Thread t:workers)t.join();check(received.get()==1000,"concurrent HTTP budget bounded");budget.refund(10);check(budget.reserve(100)==10,"unused reservation returned");
        ConnectionGate gate=new ConnectionGate();check(gate.acquire(RadioProfile.MESHTASTIC),"first handshake starts");check(!gate.acquire(RadioProfile.MESHCORE),"second handshake waits");check(!gate.acquire(RadioProfile.MESHCORE),"queue coalesces duplicate attempts");check(gate.release(RadioProfile.MESHTASTIC)==RadioProfile.MESHCORE,"second starts after first subscription");check(gate.acquire(RadioProfile.MESHCORE),"queued owner can proceed");check(gate.release(RadioProfile.MESHCORE)==null,"queue drained");gate.acquire(RadioProfile.MESHTASTIC);gate.acquire(RadioProfile.MESHCORE);check(gate.release(RadioProfile.MESHCORE)==null&&gate.owner()==RadioProfile.MESHTASTIC,"cancel queued setup preserves other connection");gate.clear();
        Snapshot mt=new Snapshot(new ArrayList<>(),new ArrayList<>(),Arrays.asList(message("Public",10)));Snapshot mc=new Snapshot(new ArrayList<>(),new ArrayList<>(),Arrays.asList(message("Public",20)));EnumMap<RadioProfile,Snapshot> snapshots=new EnumMap<>(RadioProfile.class);snapshots.put(RadioProfile.MESHTASTIC,mt);snapshots.put(RadioProfile.MESHCORE,mc);EnumMap<RadioProfile,String> radios=new EnumMap<>(RadioProfile.class);radios.put(RadioProfile.MESHTASTIC,"radio-mt");radios.put(RadioProfile.MESHCORE,"radio-mc");List<UiFeed.Item> feed=UiFeed.conversations(snapshots,radios,0,false,false,"");check(feed.size()==2&&feed.get(0).profile==RadioProfile.MESHCORE,"combined inbox sorts across networks");check(feed.get(0).radio.equals("radio-mc")&&feed.get(1).radio.equals("radio-mt"),"same channel/topic never merges networks");
        Flags pinned=new Flags();pinned.pinned=true;Map<String,Flags> flags=new HashMap<>();flags.put("chat:c:0:samehash",pinned);snapshots.put(RadioProfile.MESHTASTIC,new Snapshot(new ArrayList<>(),new ArrayList<>(),mt.messages,flags));feed=UiFeed.conversations(snapshots,radios,0,false,false,"");check(feed.get(0).profile==RadioProfile.MESHTASTIC,"pinning is scoped to source network");check(UiFeed.conversations(snapshots,radios,2,false,false,"").size()==1,"network filtering");check(UiFeed.conversations(snapshots,radios,0,false,true,"").size()==1,"pinned filtering");
        System.out.println("Field features: geometry, offline bounds, streaming budget, handshake sequencing and dual inbox passed");
    }
}
