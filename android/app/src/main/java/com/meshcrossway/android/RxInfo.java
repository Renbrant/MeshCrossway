package com.meshcrossway.android;

import java.util.*;
import java.security.*;
import javax.crypto.*;
import javax.crypto.spec.SecretKeySpec;

/** Receive-only metadata. A hop count is never substituted for an observed route. */
final class RxInfo {
    int hops=-1,rssi=Integer.MIN_VALUE,width=1,relay=-1;double snr=Double.NaN;
    String kind="",path="";boolean observed,mqtt;
    RxInfo(){}RxInfo(RxInfo x){hops=x.hops;rssi=x.rssi;width=x.width;relay=x.relay;snr=x.snr;kind=x.kind;path=x.path;observed=x.observed;mqtt=x.mqtt;}
    static RxInfo merge(RxInfo old,RxInfo next){if(next.observed&&!old.observed)return new RxInfo(next);RxInfo result=new RxInfo(old);if(result.hops<0&&next.hops>=0)result.hops=next.hops;if(result.kind.isEmpty())result.kind=next.kind;if(!Double.isFinite(result.snr))result.snr=next.snr;if(result.rssi==Integer.MIN_VALUE)result.rssi=next.rssi;if(result.relay<0)result.relay=next.relay;result.mqtt|=next.mqtt;return result;}
    static RxInfo meshcore(int encoded){RxInfo r=new RxInfo();if(encoded==255){r.kind="direct";return r;}int width=(encoded>>>6)+1,count=encoded&63;if(width>3||width*count>64)return r;r.kind="flood";r.width=width;r.hops=count;return r;}
    String summary(){String result=hops>=0?hops+" hop"+(hops==1?"":"s"):kind.equals("direct")?"Direct routing · hops unavailable":"Hops unavailable";if(Double.isFinite(snr))result+=String.format(Locale.ROOT," · SNR %.2f dB",snr);if(rssi!=Integer.MIN_VALUE)result+=" · RSSI "+rssi+" dBm";if(mqtt)result+=" · MQTT";return result;}
    static final class Observation {RxInfo info;long time,at;int type;String body;}
    /** Decodes only version-1 group text, and only after validating the firmware's MAC. */
    static Observation channelLog(byte[] frame,byte[] key,long now){
        try{if(frame.length<9||frame.length>176||(frame[0]&255)!=136||key.length!=16)return null;
            int header=frame[3]&255,route=header&3;if((header>>>6)!=0||((header>>>2)&15)!=5)return null;
            int pos=4+((route==0||route==3)?4:0);if(pos>=frame.length)return null;int encoded=frame[pos++]&255;RxInfo rx=meshcore(encoded);if(rx.kind.isEmpty()||encoded==255)return null;int bytes=rx.hops*rx.width;if(pos+bytes+3>=frame.length)return null;byte[] path=Arrays.copyOfRange(frame,pos,pos+bytes);pos+=bytes;
            byte[] hash=MessageDigest.getInstance("SHA-256").digest(key);if((frame[pos++]&255)!=(hash[0]&255))return null;byte[] expected=Arrays.copyOfRange(frame,pos,pos+2);pos+=2;byte[] encrypted=Arrays.copyOfRange(frame,pos,frame.length);if(encrypted.length==0||encrypted.length%16!=0)return null;
            Mac mac=Mac.getInstance("HmacSHA256");mac.init(new SecretKeySpec(Arrays.copyOf(key,32),"HmacSHA256"));if(!MessageDigest.isEqual(expected,Arrays.copyOf(mac.doFinal(encrypted),2)))return null;
            Cipher cipher=Cipher.getInstance("AES/ECB/NoPadding");cipher.init(Cipher.DECRYPT_MODE,new SecretKeySpec(key,"AES"));byte[] plain=cipher.doFinal(encrypted);if(plain.length<6)return null;int type=plain[4]&255;if(type!=0)return null;String body=Wire.fixedText(plain,5,plain.length-5);if(body.isEmpty())return null;
            // A direct packet contains remaining routing instructions, not a record of travelled hops.
            if(route==0||route==1){rx.observed=true;rx.path=Wire.hex(path);}else{rx.observed=false;rx.hops=-1;rx.kind="direct";}
            rx.snr=frame[1]/4.0;rx.rssi=frame[2];Observation result=new Observation();result.info=rx;result.time=Wire.u32(plain,0);result.type=type;result.body=body;result.at=now;return result;
        }catch(GeneralSecurityException|IllegalArgumentException e){return null;}
    }
}
