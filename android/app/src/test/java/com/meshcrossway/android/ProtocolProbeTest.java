package com.meshcrossway.android;

import java.util.Arrays;

/** Dependency-free wire fixture regression tests: run scripts/test_android_probe.sh. */
public class ProtocolProbeTest {
    static void check(boolean condition){if(!condition)throw new AssertionError();}
    static void rejects(Runnable action){try{action.run();}catch(IllegalArgumentException expected){return;}throw new AssertionError("Malformed input accepted");}
    public static void main(String[] args){
        ProtocolProbe mt=new ProtocolProbe(300);
        check(Arrays.equals(mt.meshtasticRequest(),new byte[]{0x18,(byte)0xac,2}));
        mt.meshtastic(new byte[]{0x1a,2,8,42}); // FromRadio.my_info.my_node_num
        mt.meshtastic(new byte[]{0x6a,0}); // FromRadio.metadata
        mt.meshtastic(new byte[]{0x2a,2,0x32,0}); // FromRadio.config.lora
        mt.meshtastic(new byte[]{0x38,1});check(!mt.complete); // stale configuration nonce
        mt.meshtastic(new byte[]{0x38,(byte)0xac,2});check(mt.complete);
        rejects(()->new ProtocolProbe(300).meshtastic(new byte[]{0x38,(byte)0xac,2}));
        rejects(()->mt.meshtastic(new byte[]{0x1a,5,1}));
        rejects(()->mt.meshtastic(new byte[]{0}));
        rejects(()->mt.meshtastic(new byte[]{(byte)0x80}));
        rejects(()->mt.meshtastic(new byte[513]));
        ProtocolProbe mc=new ProtocolProbe(1);
        rejects(()->mc.meshcore(new byte[]{13,2,0,0,0,0,0,0}));
        rejects(()->mc.meshcore(new byte[]{13,3}));
        check(mc.meshcore(new byte[]{13,3,1,1,0,0,0,0}));check(!mc.complete);
        rejects(()->mc.meshcore(new byte[]{5,1}));
        byte[] info=new byte[58];info[0]=5;mc.meshcore(info);check(mc.complete);
        check(ProtocolProbe.meshcoreAppStart().length==20);
        check(Arrays.equals(ProtocolProbe.meshcoreQuery(),new byte[]{22,14}));
        System.out.println("Protocol probe fixtures passed");
    }
}
