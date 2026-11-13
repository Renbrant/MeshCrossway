package com.meshcrossway.android;

import static com.meshcrossway.android.MeshData.*;

/** Recent stored message observations, not a packet sniffer or RF utilization. */
final class Stage2Traffic {
    static final long WINDOW_SECONDS=15*60;
    private Stage2Traffic(){}

    static final class Counts {
        final int incoming,outgoing,rxRssi,rxSnr,observedPaths,mqttFlagged;
        Counts(int incoming,int outgoing,int rxRssi,int rxSnr,int observedPaths,int mqttFlagged){
            this.incoming=incoming;this.outgoing=outgoing;this.rxRssi=rxRssi;
            this.rxSnr=rxSnr;this.observedPaths=observedPaths;this.mqttFlagged=mqttFlagged;
        }
    }

    static Counts last15Minutes(Snapshot snapshot,long nowSeconds){
        int incoming=0,outgoing=0,rssi=0,snr=0,paths=0,mqtt=0;
        if(snapshot!=null)for(Message message:snapshot.messages){
            if(message==null||message.timestamp<=0||message.timestamp>nowSeconds
                    ||nowSeconds-message.timestamp>WINDOW_SECONDS)continue;
            if(message.outgoing){outgoing++;continue;}
            incoming++;
            RxInfo rx=message.rx;
            if(rx==null)continue;
            if(rx.rssi>=-160&&rx.rssi<=0)rssi++;
            if(Double.isFinite(rx.snr)&&rx.snr>=-50&&rx.snr<=50)snr++;
            if(rx.observed&&rx.path!=null&&!rx.path.isEmpty())paths++;
            if(rx.mqtt)mqtt++;
        }
        return new Counts(incoming,outgoing,rssi,snr,paths,mqtt);
    }
}
