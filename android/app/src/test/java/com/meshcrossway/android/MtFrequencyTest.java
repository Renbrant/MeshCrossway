package com.meshcrossway.android;
import java.util.*;

/** Tuned-frequency arithmetic must match firmware RadioInterface::applyModemConfig, and a save
 *  verified before a restart must be checked again against what the radio reports afterwards. */
public final class MtFrequencyTest {
    static void check(boolean yes,String label){if(!yes)throw new AssertionError(label);}
    static boolean near(double a,double b){return Math.abs(a-b)<0.0005;}
    static byte[] lora(int region,int preset,long slot){Wire.Out o=new Wire.Out().scalar(1,1);if(preset!=0)o.scalar(2,preset);o.scalar(7,region).scalar(8,3).scalar(9,1);if(slot!=0)o.scalar(11,slot);return o.array();}
    static ProtocolClient radio(String firmware,String primaryName){
        ProtocolClient c=ProtocolClientTest.mt(new ProtocolClientTest.Events());c.firmware=firmware;
        byte[] settings=new Wire.Out().bytes(2,new byte[]{1}).bytes(3,primaryName.getBytes(java.nio.charset.StandardCharsets.UTF_8)).array();
        c.receive(ProtocolClientTest.wrapped(10,new Wire.Out().scalar(1,0).bytes(2,settings).scalar(3,1).array()),10);
        return c;
    }
    public static void main(String[] args){
        for(String fw:new String[]{"2.7.15","2.8.1"}){
            ProtocolClient c=radio(fw,"");
            MtLora.Frequency f=MtLora.frequency(c,lora(1,0,0));
            check(f!=null&&f.slot==20&&f.slots==104&&near(f.mhz,906.875)&&f.automatic,"US LongFast default is slot 20, 906.875 MHz on "+fw);
            check(f.source().contains("LongFast"),"blank primary name hashes as the preset name");
            MtLora.Frequency turbo=MtLora.frequency(c,lora(1,9,0));
            check(turbo!=null&&turbo.slots==52&&turbo.slot==14&&near(turbo.mhz,908.75),"US LongTurbo default moves to slot 14, 908.750 MHz on "+fw);
            MtLora.Frequency explicit=MtLora.frequency(c,lora(1,0,1));
            check(explicit!=null&&!explicit.automatic&&explicit.slot==1&&near(explicit.mhz,902.125),"explicit slot 1 on "+fw);
        }
        ProtocolClient named=radio("2.8.1","Crossway");
        MtLora.Frequency custom=MtLora.frequency(named,lora(1,0,0));
        check(custom!=null&&custom.slot==(int)(MtLora.djb2("Crossway")%104)+1&&custom.source().contains("Crossway"),"custom primary name selects the slot");
        byte[] override=Wire.floatField(lora(1,0,0),14,915.5f);
        MtLora.Frequency o=MtLora.frequency(named,override);
        check(o!=null&&o.override&&near(o.mhz,915.5),"frequency override wins");
        check(MtLora.frequency(named,lora(0,0,0))==null,"unset region has no frequency");
        check(MtLora.djb2("LongFast")==MtLora.djb2("LongFast")&&MtLora.presetDisplayName(9,true).equals("LongTurbo")&&MtLora.presetDisplayName(9,false).equals("Custom"),"firmware preset display names");
        check(MtLora.loraSummary(lora(1,9,0)).equals("BW 500 kHz · SF11 · CR 4/8"),"LongTurbo modem parameters");
        check(MtLora.loraSummary(lora(1,0,0)).equals("BW 250 kHz · SF11 · CR 4/5"),"LongFast modem parameters");

        // A LoRa save confirmed before the restart, then the radio comes back with another preset.
        ProtocolClient after=radio("2.8.1","");
        after.receive(ProtocolClientTest.wrapped(5,new Wire.Out().bytes(6,lora(1,0,0)).array()),20);
        byte[] desired=lora(1,9,0);
        MtAdmin.Edit edit=new MtAdmin.Edit(6,5,34,desired,Wire.changes(lora(1,0,0),desired),true);
        after.admin.restartCheck=edit;after.admin.restartCheckPending=true;edit.verifiedAt=25;
        after.next(30);
        check(after.admin.restartCheck==null&&after.admin.error.contains("requested LONG TURBO; radio LONG FAST")&&after.admin.lastSave.startsWith("After restarting"),"post-restart mismatch is reported");

        ProtocolClient kept=radio("2.8.1","");
        kept.receive(ProtocolClientTest.wrapped(5,new Wire.Out().bytes(6,desired).array()),20);
        MtAdmin.Edit same=new MtAdmin.Edit(6,5,34,desired,Wire.changes(lora(1,0,0),desired),true);
        kept.admin.restartCheck=same;kept.admin.restartCheckPending=true;same.verifiedAt=25;
        kept.next(30);
        check(kept.admin.error.isEmpty()&&kept.admin.status.contains("Confirmed after radio restart — LONG TURBO"),"post-restart match is confirmed");

        ProtocolClient stale=radio("2.8.1","");
        stale.receive(ProtocolClientTest.wrapped(5,new Wire.Out().bytes(6,lora(1,0,0)).array()),20);
        MtAdmin.Edit old=new MtAdmin.Edit(6,5,34,desired,Wire.changes(lora(1,0,0),desired),true);
        stale.admin.restartCheck=old;stale.admin.restartCheckPending=true;old.verifiedAt=0;
        stale.next(MtAdmin.RESTART_CHECK_MS+10);
        check(stale.admin.restartCheck==null&&stale.admin.error.isEmpty(),"a much later reconnect is not treated as the restart");

        ProtocolClient notified=radio("2.8.1","");
        notified.receive(ProtocolClientTest.wrapped(16,new Wire.Out().scalar(3,40).bytes(4,"Preset LongTurbo invalid for US, using LongFast".getBytes(java.nio.charset.StandardCharsets.UTF_8)).array()),40);
        check(notified.mtNotices.size()==1&&notified.mtNotices.get(0).contains("using LongFast"),"firmware client notifications are kept");
        System.out.println("Meshtastic frequency: slot arithmetic, overrides, post-restart verification and radio notices passed");
    }
}
