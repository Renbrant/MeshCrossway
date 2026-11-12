package com.meshcrossway.android;

import java.util.Locale;
import static com.meshcrossway.android.MeshData.*;

/**
 * RF observations derived only from persisted inbound message metadata.
 * Never infer a sender from a display name or infer LoRa RSSI from Bluetooth.
 */
final class RfHistory {
    static final int SCAN_LIMIT = 4096;
    static final int DISPLAY_LIMIT = 96;
    private RfHistory() {}

    static final class Sample {
        final String uid, nodeId, provenance;
        final long messageTime;
        final int rssi, hops;
        final double snr;
        final boolean mqtt, observed;

        Sample(String uid, String nodeId, long messageTime, RxInfo rx,
               int validRssi, double validSnr, int validHops) {
            this.uid=uid;this.nodeId=nodeId;this.messageTime=messageTime;
            this.rssi=validRssi;this.snr=validSnr;this.hops=validHops;
            this.mqtt=rx.mqtt;this.observed=rx.observed;
            this.provenance=rx.provenance();
        }
        boolean hasRssi(){return rssi!=Integer.MIN_VALUE;}
        boolean hasSnr(){return Double.isFinite(snr);}
        boolean hasHops(){return hops>=0;}
        String metrics(){
            StringBuilder result=new StringBuilder();
            if(hasRssi())result.append("RSSI ").append(rssi).append(" dBm");
            if(hasSnr()){
                if(result.length()>0)result.append(" · ");
                result.append(String.format(Locale.ROOT,"SNR %.1f dB",snr));
            }
            if(hasHops()){
                if(result.length()>0)result.append(" · ");
                result.append(hops).append(hops==1?" hop":" hops");
            }
            return result.toString();
        }
    }

    /**
     * MT packets carry a native numeric sender; MC direct messages can carry a
     * resolved full 32-byte public identity. MC group display names and partial
     * DM prefixes are insufficient evidence for per-node attribution.
     */
    static String source(RadioProfile network, Message message) {
        if(message==null||message.outgoing||message.rx==null)return "";
        if(network==RadioProfile.MESHTASTIC){
            long id=message.rx.source;
            return id>0&&id<=0xffffffffL?Long.toString(id):"";
        }
        if(network==RadioProfile.MESHCORE&&!message.group&&message.peer!=null
                &&message.peer.matches("(?i)[0-9a-f]{64}"))
            return message.peer.toLowerCase(Locale.ROOT);
        return "";
    }

    static Sample from(RadioProfile network, String nodeId, Message message) {
        if(nodeId==null||nodeId.isEmpty()||message==null||message.uid==null
                ||message.uid.isEmpty()||message.timestamp<=0)return null;
        String source=source(network,message);
        if(!nodeId.equalsIgnoreCase(source))return null;
        RxInfo rx=message.rx;
        int rssi=rx.rssi>=-160&&rx.rssi<=0?rx.rssi:Integer.MIN_VALUE;
        double snr=Double.isFinite(rx.snr)&&rx.snr>=-50&&rx.snr<=50?rx.snr:Double.NaN;
        int hops=rx.hops>=0&&rx.hops<=63?rx.hops:-1;
        if(rssi==Integer.MIN_VALUE&&!Double.isFinite(snr)&&hops<0)return null;
        return new Sample(message.uid,source,message.timestamp,rx,rssi,snr,hops);
    }
}
