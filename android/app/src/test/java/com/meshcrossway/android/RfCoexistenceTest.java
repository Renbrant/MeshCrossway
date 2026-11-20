package com.meshcrossway.android;

import java.util.*;
import static com.meshcrossway.android.MeshData.*;

/** RF coexistence analysis: airtime, background budget, isolation and preset plans from real config bytes. */
public final class RfCoexistenceTest {
    static void check(boolean yes,String label){if(!yes)throw new AssertionError(label);}
    static ProtocolClient.Listener quiet(){return new ProtocolClient.Listener(){public void node(Node n){}public void channel(Channel ch){}public void message(Message m){}public void delivery(String uid,String state){}};}
    static byte[] channel(int slot,int role,int precision){byte[] module=precision>0?new Wire.Out().scalar(1,precision).array():Wire.EMPTY;Wire.Out settings=new Wire.Out().bytes(3,Wire.text("Ch"+slot,11));if(module.length>0)settings.bytes(7,module);return new Wire.Out().scalar(1,slot).bytes(2,settings.array()).scalar(3,role).array();}
    static ProtocolClient meshtastic(){
        ProtocolClient c=new ProtocolClient(RadioProfile.MESHTASTIC,1,quiet());c.ready=true;c.firmware="2.8.1";c.mtHardware=110;
        c.mtConfigs.put(1,Wire.EMPTY); // device: Client, rebroadcast ALL, node info default 3 h
        c.mtConfigs.put(2,new Wire.Out().scalar(2,1).scalar(13,1).array()); // position: smart on, GPS enabled
        c.mtConfigs.put(6,new Wire.Out().scalar(1,1).scalar(2,0).scalar(7,1).scalar(9,1).array()); // LoRa: LongFast, US, TX on, power 0
        c.mtModules.put(6,new Wire.Out().scalar(14,1).array()); // telemetry: device metrics to mesh
        c.mtModules.put(10,new Wire.Out().scalar(1,1).scalar(3,1).array()); // neighbor info on
        c.mtModules.put(5,Wire.EMPTY); // range test off
        c.mtChannels.put(0,channel(0,1,13));c.mtChannels.put(1,channel(1,2,0));
        return c;
    }
    static ProtocolClient meshcore(){
        ProtocolClient c=new ProtocolClient(RadioProfile.MESHCORE,2,quiet());c.ready=true;c.firmwareCode=10;c.spreadingFactor=7;c.bandwidthKhz=62.5;c.codingRate=5;
        c.hasTxPower=true;c.txPower=22;c.maxTxPower=22;c.telemetryModes=(2<<2)|(1<<4);c.multiAcks=true;return c;
    }
    public static void main(String[] args){
        // Airtime matches the values published in RF-COEXISTENCE.md section 8.1.
        check(Math.abs(RfCoexistence.airtime(11,250,5,50,16)-0.641)<0.01,"LongFast 50 bytes ≈ 0.64 s");
        check(Math.abs(RfCoexistence.airtime(7,62.5,5,50,16)-0.211)<0.01,"MeshCore 50 bytes ≈ 0.21 s");
        check(Math.abs(RfCoexistence.airtime(11,250,5,110,16)-1.051)<0.01,"node info ≈ 1.05 s");
        // Isolation: ≈ 13.4 dB side by side at 20 cm, much more stacked.
        double h=new RfCoexistence.Geometry(20,false).isolationDb();check(h>12.5&&h<14.5,"20 cm horizontal isolation "+h);
        check(new RfCoexistence.Geometry(66,true).isolationDb()>39,"66 cm vertical ≈ 40 dB");
        check(new RfCoexistence.Geometry(5,false).isolationDb()>=7,"isolation floor");

        ProtocolClient mt=meshtastic(),mc=meshcore();
        RfCoexistence.Report r=RfCoexistence.analyze(mt,mc,new RfCoexistence.Geometry(20,false));
        check(r.mtConnected&&r.mcConnected&&r.mtLoaded,"both analysed");
        check(r.mtTxDbm==28&&r.mtTxEstimated,"Heltec V4 region default capped at 28 dBm");
        check(r.peakAtMc>13&&r.peakAtMc<16,"≈ +14 dBm at MeshCore "+r.peakAtMc);
        check(r.hardware==RfCoexistence.Level.HIGH,"hardware margin exceeded");
        boolean relay=false,smart=false,tel=false,neigh=false;for(RfCoexistence.Source s:r.mt.sources){relay|=s.label.startsWith("Relaying");smart|=s.label.startsWith("Position while driving");tel|=s.label.startsWith("Battery");neigh|=s.label.startsWith("Neighbor");}
        check(relay&&smart&&tel&&neigh,"default sources counted");
        check(r.mt.secondsPerHour()>150&&r.lossOnMc>0.03,"defaults cause noticeable loss on MeshCore "+r.lossOnMc);
        check(r.mc.sources.isEmpty()&&r.lossOnMt==0,"MeshCore companion has no automatic transmissions");
        check(r.changes()>=6,"several recommended changes "+r.changes());

        // Minimum preset plan, compliance after applying the targets, and restart count.
        List<RfCoexistence.Step> plan=RfCoexistence.plan(RfCoexistence.Profile.MINIMUM,mt,mc);
        List<RfCoexistence.Step> todo=RfCoexistence.pending(plan,mt,mc);
        check(todo.size()==6,"device, position, channel 0, telemetry, neighbor, MeshCore: "+todo.size());
        check(RfCoexistence.restarts(todo)==3,"position, telemetry and neighbor restart the radio");
        // Simulate the radio confirming each target.
        byte[] device=mt.mtConfig(0);device=Wire.replace(Wire.replace(device,6,4),7,43200);mt.mtConfigs.put(1,device);
        mt.mtConfigs.put(2,Wire.replace(Wire.replace(mt.mtConfig(1),2,0),1,43200));
        mt.mtChannels.put(0,channel(0,1,0));
        mt.mtModules.put(6,RfCoexistence.telemetryTarget(mt,true));
        mt.mtModules.put(10,Wire.replace(mt.mtModule(9),1,0));
        mc.telemetryModes=0;mc.multiAcks=false;
        check(RfCoexistence.pending(plan,mt,mc).isEmpty(),"all targets satisfied");
        r=RfCoexistence.analyze(mt,mc,new RfCoexistence.Geometry(20,false));
        check(r.mt.sources.size()==1&&r.mt.secondsPerHour()<0.2,"only node info every 12 h remains");
        check(r.traffic==RfCoexistence.Level.GOOD&&r.lossOnMc<0.001,"traffic interference minimal");
        check(r.changes()==1,"only TX power remains recommended "+r.changes());

        // TX off removes every Meshtastic source and the peak at MeshCore.
        mt.mtConfigs.put(6,Wire.replace(mt.mtConfig(5),9,0));r=RfCoexistence.analyze(mt,mc,new RfCoexistence.Geometry(20,false));
        check(r.mt.txOff&&r.mt.sources.isEmpty()&&Double.isNaN(r.peakAtMc),"TX off is silent");
        check(r.hardware==RfCoexistence.Level.ATTENTION,"MeshCore at 22 dBm still ≈ +8 dBm at Meshtastic");

        // Router roles are moved to Client when relaying is turned off; Help the mesh restores relaying.
        ProtocolClient router=meshtastic();router.mtConfigs.put(1,new Wire.Out().scalar(1,2).array());
        RfCoexistence.Step step=RfCoexistence.deviceStep(true,43200);check(!step.done.test(router),"router relays");
        check(RfCoexistence.plan(RfCoexistence.Profile.HELP_MESH,mt,null).size()==3,"help-mesh plan: device, telemetry, range test");

        // Channel helper keeps other module settings and drops precision only.
        byte[] muted=new Wire.Out().scalar(1,1).bytes(2,new Wire.Out().bytes(3,Wire.text("A",11)).bytes(7,new Wire.Out().scalar(1,13).scalar(2,1).array()).array()).scalar(3,1).array();
        byte[] module=Wire.bytes(Wire.fields(Wire.bytes(Wire.fields(muted),2)),7);byte[] kept=RfCoexistence.without(module,1);
        check(Wire.number(Wire.fields(kept),1,0)==0&&Wire.number(Wire.fields(kept),2,0)==1,"is_muted kept, precision removed");
        System.out.println("RfCoexistenceTest OK");
    }
}
