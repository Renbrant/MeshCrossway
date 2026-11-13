package com.meshcrossway.android;

/** Bluetooth received signal indicator for the phone-to-radio link (not LoRa RSSI). */
final class BleSignal {
    static final int UNKNOWN=Integer.MIN_VALUE;
    static final long MAX_AGE_MS=24L*60*60*1000;
    // Readings older than 90 seconds are cached history, never a current BLE measurement.
    static final long LIVE_MAX_AGE_MS=90_000L;
    private BleSignal(){}
    static boolean valid(int rssi){return rssi>=-127&&rssi<=-10;}
    static int bars(int rssi){
        if(!valid(rssi))return 0;
        if(rssi>=-60)return 4;
        if(rssi>=-70)return 3;
        if(rssi>=-80)return 2;
        if(rssi>=-90)return 1;
        return 0;
    }
    static String quality(int rssi){
        if(!valid(rssi))return "Unavailable";
        return new String[]{"Weak","Weak","Fair","Good","Excellent"}[bars(rssi)];
    }
    static String label(int rssi,boolean connected){
        if(!valid(rssi))return "BLE signal unavailable";
        return (connected?"BLE ":"Last BLE ")+rssi+" dBm · "+quality(rssi);
    }
    static boolean recent(long recordedAt,long now){
        return recordedAt>0&&recordedAt<=now&&now-recordedAt<=MAX_AGE_MS;
    }
    static boolean live(boolean radioReady,boolean currentLink,long recordedAt,long now){
        return radioReady&&currentLink&&recordedAt>0&&recordedAt<=now&&now-recordedAt<=LIVE_MAX_AGE_MS;
    }
    static String age(long recordedAt,long now){
        if(!recent(recordedAt,now))return "age unavailable";
        long seconds=(now-recordedAt)/1000;
        if(seconds<60)return "just now";
        if(seconds<3600)return seconds/60+" min ago";
        return seconds/3600+" h ago";
    }
    static String label(int rssi,boolean live,long recordedAt,long now){
        if(!valid(rssi)||!recent(recordedAt,now))return "BLE signal unavailable";
        return (live?"BLE ":"Last BLE ")+rssi+" dBm · "+quality(rssi)+" · "+age(recordedAt,now);
    }
}
