package com.meshcrossway.android;

import java.util.Locale;

/**
 * Battery of the radio itself (not the phone), shown beside the Bluetooth bars on Home.
 * Both radios use the same MeshCrossway voltage curve, so two radios on one battery show the
 * same level: MeshCore reports millivolts; Meshtastic's own battery_level (computed by its
 * firmware with a different curve) is only used to recognise external power without a voltage.
 */
final class RadioBattery {
    static final int UNKNOWN=-1,POWERED=101;
    private RadioBattery(){}
    /** Single-cell LiPo resting voltage to charge. */
    private static final int[][] CURVE={{4200,100},{4100,90},{4000,80},{3920,70},{3850,60},{3800,50},{3750,40},{3700,30},{3650,20},{3550,10},{3300,0}};
    static int percentFromMillivolts(int mv){
        if(mv<=0)return UNKNOWN;
        if(mv>=CURVE[0][0])return 100;
        for(int i=1;i<CURVE.length;i++)if(mv>=CURVE[i][0]){int hiV=CURVE[i-1][0],hiP=CURVE[i-1][1],loV=CURVE[i][0],loP=CURVE[i][1];return loP+Math.round((mv-loV)*(hiP-loP)/(float)(hiV-loV));}
        return 0;
    }
    /** Meshtastic DeviceMetrics.voltage (field 2, float volts) in millivolts, or -1. */
    static int millivolts(byte[] deviceMetrics){
        Wire.F f=Wire.field(Wire.fields(deviceMetrics),2);if(f==null||f.wire!=5)return -1;
        float volts=Float.intBitsToFloat((int)f.number);return Float.isFinite(volts)&&volts>0.5f&&volts<10?Math.round(volts*1000):-1;
    }
    /** Battery voltage of the radio in millivolts, or -1 when the radio has not reported one. */
    static int millivolts(ProtocolClient c){
        if(c==null||!c.ready)return -1;
        if(c.profile==RadioProfile.MESHCORE)return c.hasBattery&&c.batteryMv>0?c.batteryMv:-1;
        MeshData.Node own=c.nodes.get(Long.toString(c.myNum));return own==null?-1:own.batteryMv;
    }
    /** Percent 0–100 from the voltage, POWERED for external power without a voltage, or UNKNOWN. */
    static int level(ProtocolClient c){
        int mv=millivolts(c);if(mv>0)return percentFromMillivolts(mv);
        if(c==null||!c.ready||c.profile==RadioProfile.MESHCORE)return UNKNOWN;
        MeshData.Node own=c.nodes.get(Long.toString(c.myNum));
        // Meshtastic battery_level 101 = external power; used only when no voltage is reported.
        return own!=null&&own.battery>100?POWERED:UNKNOWN;
    }
    /** Text beside the battery icon: the voltage, or USB for external power without a voltage. */
    static String label(int level,int mv){
        if(mv>0)return String.format(Locale.ROOT,"%.2f V",mv/1000.0);
        return level==POWERED?"USB":"";
    }
}
