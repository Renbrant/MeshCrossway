package com.meshcrossway.android;

import java.util.Locale;

/** Battery of the radio itself (not the phone), shown beside the Bluetooth bars on Home. */
final class RadioBattery {
    static final int UNKNOWN=-1,POWERED=101;
    private RadioBattery(){}
    /** Single-cell LiPo resting voltage to charge, matching the curve most companion apps use. */
    private static final int[][] CURVE={{4200,100},{4100,90},{4000,80},{3920,70},{3850,60},{3800,50},{3750,40},{3700,30},{3650,20},{3550,10},{3300,0}};
    static int percentFromMillivolts(int mv){
        if(mv<=0)return UNKNOWN;
        if(mv>=CURVE[0][0])return 100;
        for(int i=1;i<CURVE.length;i++)if(mv>=CURVE[i][0]){int hiV=CURVE[i-1][0],hiP=CURVE[i-1][1],loV=CURVE[i][0],loP=CURVE[i][1];return loP+Math.round((mv-loV)*(hiP-loP)/(float)(hiV-loV));}
        return 0;
    }
    /** Percent 0–100, POWERED for external power without a battery reading, or UNKNOWN. */
    static int level(ProtocolClient c){
        if(c==null||!c.ready)return UNKNOWN;
        if(c.profile==RadioProfile.MESHCORE)return c.hasBattery?percentFromMillivolts(c.batteryMv):UNKNOWN;
        MeshData.Node own=c.nodes.get(Long.toString(c.myNum));
        if(own==null)return UNKNOWN;
        // Meshtastic battery_level: 1–100 percent, 101 = powered; 0 or absent = no reading.
        if(own.battery>100)return POWERED;
        return own.battery>0?own.battery:UNKNOWN;
    }
    static String label(int level){
        if(level==POWERED)return "USB";
        return level<0?"":level+"%";
    }
}
