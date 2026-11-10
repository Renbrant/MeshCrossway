package com.meshcrossway.android;

import java.nio.charset.StandardCharsets;
import java.util.*;

/** Official-app boundary cases and full readback verification; no radio mutations. */
public final class MtConformanceTest {
    static int checks;
    static void check(boolean ok,String message){checks++;if(!ok)throw new AssertionError(message);}
    static void rejects(Runnable action,String message){boolean rejected=false;try{action.run();}catch(IllegalArgumentException e){rejected=true;}check(rejected,message);}
    static ProtocolClient client(){ProtocolClient c=MtSettingsTest.fresh();c.admin.key=new byte[]{1};c.admin.keyAt=c.admin.clock;return c;}
    static byte[] lora(int region,int preset,int cr,int slot){return new Wire.Out().scalar(1,1).scalar(2,preset).scalar(5,cr).scalar(7,region).scalar(8,3).scalar(9,1).scalar(11,slot).array();}
    static MtAdmin verify(int kind,int index,int set,byte[] desired,byte[] delta,byte[] actual){
        long[] request={0};MtAdmin admin=new MtAdmin(new MtAdmin.Host(){
            public void enqueue(byte[] value,long id,boolean response){if(response)request[0]=id;}
            public void apply(int k,int i,byte[] body){}
        });
        MtAdmin.Edit edit=new MtAdmin.Edit(kind,index,set,desired,delta,true);admin.restore(edit);admin.ready();
        byte[] body=kind==6?new Wire.Out().bytes(index+1,actual).array():actual;
        admin.response(100,100,100,request[0],new Wire.Out().bytes(kind,body).array());
        check(admin.edit==null,"Verification must finish and unlock controls");return admin;
    }
    static boolean confirms(byte[] desired,byte[] actual){return verify(6,5,34,desired,new Wire.Out().scalar(8,4).array(),actual).error.isEmpty();}
    public static void main(String[] args){
        checks=0;
        int[] rates={5,8,8,5,5,5,5,8,5,8,5,5,6,6,5,6,5};
        for(int i=0;i<rates.length;i++){
            check(MtLora.presetCodingRate(i)==rates[i],"Official coding rate for preset "+i);
            byte[] desired=lora(1,i,0,0);
            for(int stored=5;stored<=rates[i];stored++)check(MtLora.codingMatches(Wire.replace(desired,5,stored),desired),"Stored rate below/equal preset default is effective default");
            if(rates[i]<8)check(!MtLora.codingMatches(Wire.replace(desired,5,rates[i]+1),desired),"Actual higher override must not confirm default");
        }
        ProtocolClient c=client();
        for(String version:new String[]{"2.6.9","2.7.0","2.7.16"}){c.firmware=version;check(!MtLora.selectable(c,1,9),"LONG_TURBO hidden on "+version);}
        c.firmware="2.7.17";check(MtLora.selectable(c,1,9),"LONG_TURBO arrives at 2.7.17");check(!MtLora.selectable(c,1,16),"MEDIUM_TURBO needs 2.8");
        c.firmware="2.8.1";check(MtLora.selectable(c,1,16),"MEDIUM_TURBO available at 2.8");
        int[][] cases={{1,0,104},{3,0,1},{26,0,22},{29,10,4},{32,12,3},{27,14,100},{28,14,200},{33,14,200},{34,12,100},{35,12,300},{36,12,200},{37,12,50},{22,0,7},{13,0,103}};
        c.mtOwnerLoaded=true;c.mtOwner=new Wire.Out().scalar(6,1).array();
        for(int[] test:cases){int region=test[0],preset=test[1],max=test[2];check(MtLora.slotCount(region,MtLora.presetBandwidth(preset,region))==max,"Official slot count region "+region);MtLora.validate(lora(region,preset,0,max),c);rejects(()->MtLora.validate(lora(region,preset,0,max+1),c),"Reject slot beyond region "+region);}
        for(MtLora.Region r:MtLora.REGION_INFO)check(MtLora.powerLimit(r.id)==r.power,"Region power table");
        check(MtLora.powerLimit(29)==27&&MtLora.powerLimit(32)==27,"New EU regions have explicit 27 dBm ceiling");
        c.mtOwner=new Wire.Out().scalar(6,0).array();rejects(()->MtLora.validate(lora(27,14,0,1),c),"Amateur region requires licensed mode");
        rejects(()->MtLora.validate(Wire.replace(lora(29,10,0,1),10,28),c),"EU866 power limit");
        rejects(()->MtLora.validate(lora(30,0,0,1),c),"Reserved region has no invented band profile");
        check(MtLora.regions(c).stream().noneMatch(o->o.value==30||o.value==31),"Reserved regions are not offered");
        byte[] manual=new Wire.Out().scalar(3,250).scalar(4,6).scalar(5,5).scalar(7,1).scalar(9,1).array();
        c.mtHardware=12;c.hardware=MtLora.hardwareName(12);MtLora.validate(manual,c);check(MtLora.sfCapability(12)==MtLora.SF_UNKNOWN,"TBEAM S3 modem variant is not guessed");
        c.mtHardware=113;c.hardware=MtLora.hardwareName(113);MtLora.validate(manual,c);check(MtLora.sfCapability(113)==MtLora.SF_UNKNOWN,"V2 in a name is not RF95 evidence");
        c.mtHardware=5;rejects(()->MtLora.validate(manual,c),"Known RF95 rejects SF6");
        c.mtHardware=43;MtLora.validate(manual,c);check(MtLora.sfCapability(43)==MtLora.SF_MODERN,"Heltec V3 supports modern SF range");
        byte[] desired=lora(1,0,0,0),normalized=Wire.replace(desired,5,5);
        check(confirms(desired,normalized),"Default CR normalization");
        check(confirms(lora(1,9,0,0),lora(1,9,8,0)),"LONG_TURBO CR8 normalization");
        byte[] hop=Wire.replace(desired,8,4);
        MtAdmin drift=verify(6,5,34,hop,new Wire.Out().scalar(8,4).array(),Wire.replace(hop,2,4));
        check(drift.error.contains("Preset differs")&&drift.error.contains("MEDIUM FAST"),"Hop success must expose unexpected preset drift");
        for(int tag:new int[]{1,7,8,9,11,12,13,14,15,104,105,106}){
            long next=Wire.number(Wire.fields(desired),tag,0)+1;
            check(!confirms(desired,Wire.replace(desired,tag,next)),"Unchanged or omitted LoRa field drift detected: "+tag);
        }
        check(confirms(desired,Wire.replace(Wire.replace(normalized,3,500),4,11)),"Inactive manual fields may normalize in preset mode");
        check(confirms(manual,Wire.replace(manual,2,4)),"Inactive preset may differ in manual mode");
        check(!confirms(manual,Wire.replace(manual,4,11)),"Unsupported manual SF normalization never claims success");
        check(confirms(desired,Wire.replace(normalized,10,30)),"Automatic TX power resolves");
        check(!confirms(Wire.replace(desired,10,22),Wire.replace(normalized,10,23)),"Explicit TX power drift rejected");
        byte[] withUnknown=Wire.replace(desired,200,7);check(!confirms(withUnknown,Wire.replace(withUnknown,200,8)),"Preserved unknown field drift rejected");
        check(confirms(desired,Wire.replace(normalized,200,8)),"New unknown firmware field does not break forward compatibility");
        byte[] ignored=Wire.append(desired,new Wire.Out().scalar(103,200).scalar(103,300).array());
        check(confirms(ignored,Wire.append(desired,new Wire.Out().bytes(103,new byte[]{(byte)0xc8,1,(byte)0xac,2}).array())),"Packed/repeated ignore-list encodings equivalent");
        check(!confirms(ignored,Wire.append(desired,new Wire.Out().scalar(103,200).array())),"Dropped ignored node detected");
        c.mtConfigs.put(7,Wire.EMPTY);c.mtConfigs.put(8,Wire.EMPTY);
        check(c.mtConfigLoaded(6)&&!c.mtConfigFlag(6,1),"Omitted Bluetooth false stays loaded and false");
        check(c.mtConfigLoaded(7)&&!c.mtConfigFlag(7,5),"Omitted serial false stays loaded and false");
        c.mtConfigs.put(7,new Wire.Out().scalar(1,1).array());check(c.mtConfigFlag(6,1),"Explicit Bluetooth true");
        ProtocolClient ham=client();ham.mtOwnerLoaded=true;ham.mtOwner=new Wire.Out().bytes(2,"Old".getBytes(StandardCharsets.UTF_8)).bytes(3,"R74t".getBytes(StandardCharsets.UTF_8)).array();
        rejects(()->ham.setMtHam("K1ABCDE","123456789012345"),"15-byte Ham description rejected");
        rejects(()->ham.setMtHam("K1ABCDE","é".repeat(8)),"UTF8 Ham description overflow rejected");
        ham.setMtHam("K1ABCDE","é".repeat(7));MtAdmin.Edit edit=ham.admin.edit;
        check(Wire.utf8(Wire.bytes(Wire.fields(edit.expected),2)).equals("K1ABCDE//"+"é".repeat(7)),"Compose Ham name with double slash");
        check(verify(4,0,18,edit.body,edit.expected,edit.expected).error.isEmpty(),"Full returned Ham identity confirms");
        check(!verify(4,0,18,edit.body,edit.expected,new Wire.Out().scalar(6,1).array()).error.isEmpty(),"Licensed flag alone cannot confirm lost identity");
        ProtocolClient blank=client();blank.mtOwnerLoaded=true;blank.mtOwner=Wire.EMPTY;blank.setMtHam("K0ABC","");
        check(Wire.utf8(Wire.bytes(Wire.fields(blank.admin.edit.expected),2)).equals("K0ABC"),"Empty Ham description adds no separator");
        ProtocolClient legacy=client();legacy.firmware="2.7.17";legacy.mtOwnerLoaded=true;legacy.mtOwner=Wire.EMPTY;
        rejects(()->legacy.setMtHam("K0ABC","Description"),"Legacy firmware must not silently drop Ham description");
        legacy.setMtHam("K0ABC","");check(Wire.utf8(Wire.bytes(Wire.fields(legacy.admin.edit.expected),2)).equals("K0ABC"),"Legacy Ham callsign still verifies");
        ProtocolClient modules=client();modules.mtExcludedModules=1;check(!modules.mtModuleSupported(0)&&modules.mtModuleSupported(13),"Status independent of excluded MQTT");
        modules.firmware="2.7.19";check(!modules.mtModuleSupported(13),"Status hidden before 2.7.20");modules.firmware="2.7.20";check(modules.mtModuleSupported(13),"Status arrives at 2.7.20");
        modules.mtExcludedModules=1L<<13;check(modules.mtModuleSupported(0)&&!modules.mtModuleSupported(13),"Status exclusion independent of MQTT");
        System.out.println("Meshtastic official-app conformance: "+checks+" checks passed");
    }
}
