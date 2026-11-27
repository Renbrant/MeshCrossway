package com.meshcrossway.android;

import java.util.*;
import java.util.function.Consumer;
import java.util.function.Predicate;

/**
 * Interference analysis between the co-located Meshtastic and MeshCore radios.
 *
 * Reads the real settings of both radios, estimates how much each one transmits on its own
 * (without the user sending anything), the packet loss that causes on the other radio and the
 * peak power reaching the other radio's antenna port. Also builds the setting changes for the
 * presets. Model and figures: hardware/3D/car-dual-solar-mount/RF-COEXISTENCE.md.
 *
 * Meshtastic field numbers: meshtastic/protobufs config.proto, module_config.proto, channel.proto.
 */
final class RfCoexistence {
    private RfCoexistence(){}

    // Config / module indexes as used by ProtocolClient.mtConfig(type) / mtModule(type).
    static final int CFG_DEVICE=0,CFG_POSITION=1,CFG_LORA=5;
    static final int MOD_RANGE_TEST=4,MOD_TELEMETRY=5,MOD_NEIGHBOR=9;
    // DeviceConfig
    static final int DEV_ROLE=1,DEV_REBROADCAST=6,DEV_NODE_INFO_SECS=7;
    // PositionConfig
    static final int POS_BROADCAST_SECS=1,POS_SMART=2,POS_FIXED=3,POS_SMART_MIN_SECS=11,POS_GPS_MODE=13;
    // LoRaConfig
    static final int LORA_TX_ENABLED=9,LORA_TX_POWER=10;
    // TelemetryConfig
    static final int TEL_DEVICE_SECS=1,TEL_ENV_SECS=2,TEL_ENV=3,TEL_AIR=6,TEL_AIR_SECS=7,TEL_POWER=8,TEL_POWER_SECS=9,TEL_HEALTH=11,TEL_HEALTH_SECS=12,TEL_DEVICE_TO_MESH=14;
    // RebroadcastMode
    static final int RB_ALL=0,RB_ALL_SKIP=1,RB_LOCAL_ONLY=2,RB_KNOWN_ONLY=3,RB_NONE=4,RB_CORE_ONLY=5;
    static final String[] REBROADCAST={"All","All (skip decoding)","Local only","Known only","None","Core port numbers only"};
    // Roles
    static final int ROLE_CLIENT=0,ROLE_CLIENT_MUTE=1,ROLE_ROUTER=2,ROLE_ROUTER_CLIENT=3,ROLE_REPEATER=4,ROLE_ROUTER_LATE=11;

    static final double WAVELENGTH_M=0.330; // ≈ 908 MHz
    static final int DEFAULT_SPACING_CM=20;

    enum Level {GOOD,ATTENTION,HIGH,UNKNOWN}
    enum Status {OK,CHANGE,INFO,UNKNOWN}
    enum Profile {
        MINIMUM("Minimum interference","Recommended for vehicles. Each radio only transmits what you send. Others will not see your position or battery."),
        BALANCED("Balanced","No relaying and no battery reports, but your position is still shared about once an hour."),
        HELP_MESH("Help the mesh","For roads or areas without fixed infrastructure. The Meshtastic radio relays other nodes again; expect more losses on MeshCore.");
        final String title,summary;Profile(String t,String s){title=t;summary=s;}
    }

    /** Antenna geometry chosen by the user. */
    static final class Geometry {
        final int cm;final boolean vertical;
        Geometry(int cm,boolean vertical){this.cm=Math.max(5,cm);this.vertical=vertical;}
        double isolationDb(){double ratio=cm/100.0/WAVELENGTH_M;double horizontal=Math.max(7,22+20*Math.log10(ratio)-4.3);if(!vertical)return horizontal;return Math.max(horizontal,28+40*Math.log10(ratio));}
        String label(){return cm+" cm "+(vertical?"vertical (one above the other)":"side by side");}
    }

    /** One automatic transmission source. */
    static final class Source {
        final String label;final double perHour,airtime;
        Source(String label,double perHour,double airtime){this.label=label;this.perHour=perHour;this.airtime=airtime;}
        double secondsPerHour(){return perHour*airtime;}
    }
    static final class Budget {
        final List<Source> sources=new ArrayList<>();boolean known,txOff;String note="";
        double packetsPerHour(){double n=0;for(Source s:sources)n+=s.perHour;return n;}
        double secondsPerHour(){double n=0;for(Source s:sources)n+=s.secondsPerHour();return n;}
        /** Fraction of packets lost on the other radio, whose own packets last victimAirtime seconds. */
        double lossOn(double victimAirtime){double occupied=0;for(Source s:sources)occupied+=s.perHour*(s.airtime+victimAirtime);return 1-Math.exp(-occupied/3600);}
    }

    /** A setting shown in the advanced list. */
    static final class Item {
        final String id,title,current,recommended,pros,cons;final Status status;final boolean restart;
        Item(String id,String title,String current,String recommended,Status status,boolean restart,String pros,String cons){this.id=id;this.title=title;this.current=current;this.recommended=recommended;this.status=status;this.restart=restart;this.pros=pros;this.cons=cons;}
    }

    static final class Report {
        boolean mtConnected,mcConnected,mtLoaded,mcLoaded;
        Budget mt=new Budget(),mc=new Budget();
        double mtPacketAirtime=.64,mcPacketAirtime=.21;
        double lossOnMc=Double.NaN,lossOnMt=Double.NaN;
        int mtTxDbm=Integer.MIN_VALUE,mcTxDbm=Integer.MIN_VALUE;boolean mtTxEstimated;
        double isolationDb;double peakAtMc=Double.NaN,peakAtMt=Double.NaN;
        double mtAirUtilTx=Double.NaN,mcTxPercent=Double.NaN;
        final List<Item> mtItems=new ArrayList<>(),mcItems=new ArrayList<>();
        Level traffic=Level.UNKNOWN,hardware=Level.UNKNOWN;
        /** Exactly one radio is connected: the radios cannot interfere right now. */
        boolean oneOffline(){return mtConnected!=mcConnected;}
        /** Short name of the offline radio when exactly one is connected, else empty. */
        String offlineRadio(){return !oneOffline()?"":mtConnected?"MeshCore":"Meshtastic";}
        double worstLoss(){double a=Double.isNaN(lossOnMc)?0:lossOnMc,b=Double.isNaN(lossOnMt)?0:lossOnMt;return Math.max(a,b);}
        double worstPeak(){double a=Double.isNaN(peakAtMc)?-99:peakAtMc,b=Double.isNaN(peakAtMt)?-99:peakAtMt;return Math.max(a,b);}
        Level overall(){if(traffic==Level.HIGH||hardware==Level.HIGH)return Level.HIGH;if(traffic==Level.ATTENTION||hardware==Level.ATTENTION)return Level.ATTENTION;if(traffic==Level.UNKNOWN&&hardware==Level.UNKNOWN)return Level.UNKNOWN;return Level.GOOD;}
        int changes(){int n=0;for(Item i:mtItems)if(i.status==Status.CHANGE)n++;for(Item i:mcItems)if(i.status==Status.CHANGE)n++;return n;}
    }

    // ---------------------------------------------------------------- LoRa airtime

    /** Semtech time-on-air, explicit header, CRC on. cr is 5..8 (4/5..4/8). */
    static double airtime(int sf,double bwKhz,int cr,int payload,int preamble){
        if(sf<5||sf>12||!(bwKhz>0))return Double.NaN;cr=Math.max(5,Math.min(8,cr));
        double ts=Math.pow(2,sf)/(bwKhz*1000);boolean ldro=ts>0.016;
        double n=8+Math.max(Math.ceil((8.0*payload-4*sf+28+16)/(4*(sf-(ldro?2:0))))*cr,0);
        return (preamble+4.25)*ts+n*ts;
    }
    static final int PREAMBLE=16;
    static final int BYTES_NODE_INFO=110,BYTES_POSITION=60,BYTES_TELEMETRY=50,BYTES_RELAY=75,BYTES_NEIGHBOR=80,BYTES_RANGE=40,BYTES_TYPICAL=50;

    // ---------------------------------------------------------------- reading helpers

    static long num(byte[] msg,int field,long fallback){return Wire.number(Wire.fields(msg),field,fallback);}
    static boolean flag(byte[] msg,int field){return num(msg,field,0)!=0;}
    static int seconds(byte[] msg,int field,int defaultSecs){long v=num(msg,field,0);return v<=0?defaultSecs:(int)Math.min(Integer.MAX_VALUE,v);}
    static String duration(int secs){if(secs%3600==0)return (secs/3600)+" h";if(secs%60==0)return (secs/60)+" min";return secs+" s";}
    static boolean relaysWithRole(int role){return role!=ROLE_CLIENT_MUTE;}
    static boolean routerRole(int role){return role==ROLE_ROUTER||role==ROLE_ROUTER_CLIENT||role==ROLE_REPEATER||role==ROLE_ROUTER_LATE;}
    static int hardwareMaxDbm(ProtocolClient c){return c.mtHardware==110||c.mtHardware==132?28:22;}

    /** Meshtastic LoRa parameters actually on air: sf, bandwidth kHz, coding rate. */
    static double[] mtLora(ProtocolClient c){
        byte[] lora=c.mtConfig(CFG_LORA);List<Wire.F> fs=Wire.fields(lora);int region=(int)Wire.number(fs,7,0);
        try{
            if(Wire.number(fs,1,0)!=0){int p=(int)Wire.number(fs,2,0);return new double[]{MtLora.presetSpreadingFactor(p),MtLora.presetBandwidth(p,region),Math.max(MtLora.presetCodingRate(p),(int)Wire.number(fs,5,0))};}
            return new double[]{Wire.number(fs,4,0),MtLora.manualBandwidth(Wire.number(fs,3,0),region),Wire.number(fs,5,0)};
        }catch(IllegalArgumentException e){return new double[]{11,250,5};}
    }
    static double mtAir(ProtocolClient c,int bytes){double[] l=mtLora(c);double t=airtime((int)l[0],l[1],(int)l[2],bytes,PREAMBLE);return Double.isNaN(t)?airtime(11,250,5,bytes,PREAMBLE):t;}
    static double mcAir(ProtocolClient c,int bytes){int sf=c.spreadingFactor>0?c.spreadingFactor:7;double bw=c.bandwidthKhz>0?c.bandwidthKhz:62.5;int cr=c.codingRate>0?c.codingRate:5;double t=airtime(sf,bw,cr,bytes,PREAMBLE);return Double.isNaN(t)?airtime(7,62.5,5,bytes,PREAMBLE):t;}

    /** True when at least one enabled channel shares position (precision > 0). Null when channels are not loaded. */
    static Boolean channelsSharePosition(ProtocolClient c){
        if(c.mtChannels.isEmpty())return null;
        for(byte[] ch:c.mtChannels.values()){List<Wire.F> fs=Wire.fields(ch);if(Wire.number(fs,3,0)==0)continue;byte[] settings=Wire.bytes(fs,2);byte[] module=Wire.bytes(Wire.fields(settings),7);if(Wire.number(Wire.fields(module),1,0)>0)return true;}
        return false;
    }
    static List<Integer> positionChannels(ProtocolClient c){List<Integer> out=new ArrayList<>();for(Map.Entry<Integer,byte[]> e:new TreeMap<>(c.mtChannels).entrySet()){List<Wire.F> fs=Wire.fields(e.getValue());if(Wire.number(fs,3,0)==0)continue;byte[] module=Wire.bytes(Wire.fields(Wire.bytes(fs,2)),7);if(Wire.number(Wire.fields(module),1,0)>0)out.add(e.getKey());}return out;}

    /** Copy of a protobuf message without one field. */
    static byte[] without(byte[] message,int tag){Wire.Out out=new Wire.Out();for(Wire.F f:Wire.fields(message))if(f.tag!=tag)out.add(f);return out.array();}

    /** Air util TX (%) of the local Meshtastic node, from its device metrics. */
    static double mtAirUtil(ProtocolClient c){
        MeshData.Node own=c.nodes.get(Long.toString(c.myNum));if(own==null)return Double.NaN;String device=own.metrics.get("Device");if(device==null)return Double.NaN;
        java.util.regex.Matcher m=java.util.regex.Pattern.compile("air util tx: ([0-9.]+)").matcher(device);return m.find()?Double.parseDouble(m.group(1)):Double.NaN;
    }

    // ---------------------------------------------------------------- analysis

    static Report analyze(ProtocolClient mt,ProtocolClient mc,Geometry geometry){
        Report r=new Report();r.isolationDb=geometry.isolationDb();
        if(mt!=null&&mt.ready){r.mtConnected=true;analyzeMeshtastic(mt,r);}
        if(mc!=null&&mc.ready){r.mcConnected=true;analyzeMeshCore(mc,r);}
        if(r.mtConnected)r.mtPacketAirtime=mtAir(mt,BYTES_TYPICAL);
        if(r.mcConnected)r.mcPacketAirtime=mcAir(mc,BYTES_TYPICAL);
        // Interference needs both radios on: with one offline nothing can disturb anything, so the
        // live figures are zero and no peak is shown. The settings analysis above still runs.
        if(r.oneOffline()){r.lossOnMc=0;r.lossOnMt=0;r.traffic=Level.GOOD;return r;}
        if(r.mt.known)r.lossOnMc=r.mt.lossOn(r.mcPacketAirtime);
        if(r.mc.known)r.lossOnMt=r.mc.lossOn(r.mtPacketAirtime);
        if(r.mtTxDbm!=Integer.MIN_VALUE&&!r.mt.txOff)r.peakAtMc=r.mtTxDbm-r.isolationDb;
        if(r.mcTxDbm!=Integer.MIN_VALUE)r.peakAtMt=r.mcTxDbm-r.isolationDb;
        if(r.mt.known||r.mc.known){double loss=r.worstLoss();r.traffic=loss<.01?Level.GOOD:loss<.05?Level.ATTENTION:Level.HIGH;}
        if(!Double.isNaN(r.peakAtMc)||!Double.isNaN(r.peakAtMt)){double peak=r.worstPeak();r.hardware=peak<=0?Level.GOOD:peak<=10?Level.ATTENTION:Level.HIGH;}
        return r;
    }

    private static void analyzeMeshtastic(ProtocolClient c,Report r){
        boolean device=c.mtConfigLoaded(CFG_DEVICE),position=c.mtConfigLoaded(CFG_POSITION),lora=c.mtConfigLoaded(CFG_LORA);
        r.mtLoaded=device&&position&&lora;r.mtAirUtilTx=mtAirUtil(c);
        Budget b=r.mt;List<Item> items=r.mtItems;
        // Transmit enabled and power
        if(lora){
            byte[] l=c.mtConfig(CFG_LORA);boolean tx=flag(l,LORA_TX_ENABLED);b.txOff=!tx;
            int power=(int)num(l,LORA_TX_POWER,0);int region=(int)num(l,7,0);int max=hardwareMaxDbm(c);
            int limit;try{limit=MtLora.powerLimit(region);}catch(IllegalArgumentException e){limit=max;}
            r.mtTxDbm=power<=0?Math.min(limit,max):Math.min(power,max);r.mtTxEstimated=power<=0;
            items.add(new Item("mt_tx","Transmission",tx?"On":"Off (receive only)","On, or Off when not in use",tx?Status.INFO:Status.OK,true,
                "Off: the radio never transmits, so it causes no interference at all and still receives.",
                "Off: you cannot send, acknowledge or appear to others until it is turned back on."));
            items.add(new Item("mt_power","Transmit power",(power<=0?"Region default (≈ "+r.mtTxDbm+" dBm)":r.mtTxDbm+" dBm"),"20–22 dBm if your range allows",r.mtTxDbm>22?Status.CHANGE:Status.OK,true,
                "Lower: less power reaches the MeshCore radio (each 6 dB divides it by four) and less battery used.",
                "Lower: shorter range for the Meshtastic network."));
        }
        if(device){
            byte[] d=c.mtConfig(CFG_DEVICE);int role=(int)num(d,DEV_ROLE,0),mode=(int)num(d,DEV_REBROADCAST,0);int info=seconds(d,DEV_NODE_INFO_SECS,10800);
            boolean relays=relaysWithRole(role)&&mode!=RB_NONE;
            if(relays){double rate=routerRole(role)?240:(mode==RB_ALL||mode==RB_ALL_SKIP)?120:60;b.sources.add(new Source("Relaying other nodes (estimate)",rate,mtAir(c,BYTES_RELAY)));}
            b.sources.add(new Source("Node info every "+duration(Math.max(3600,info)),3600.0/Math.max(3600,info),mtAir(c,BYTES_NODE_INFO)));
            String modeName=role==ROLE_CLIENT_MUTE?"None (Client Mute role)":REBROADCAST[Math.max(0,Math.min(REBROADCAST.length-1,mode))];
            items.add(new Item("mt_relay","Relaying (rebroadcast)",modeName,"None",relays?Status.CHANGE:Status.OK,false,
                "None: the largest source of automatic transmissions disappears. In a busy mesh this cuts losses on MeshCore from over 10 % to about 1 %.",
                "None: your node no longer helps carry other people's messages. A moving car is a poor relay anyway; turn it back on where there is no infrastructure."));
            items.add(new Item("mt_nodeinfo","Node info interval",duration(Math.max(3600,info)),"12 h",info<43200?Status.CHANGE:Status.OK,false,
                "Longer: the largest automatic packet (≈ 1 s on air) is sent less often.",
                "Longer: new nodes take longer to learn your name and key. It cannot be turned off."));
        }
        boolean posPossible=false;
        if(position){
            byte[] p=c.mtConfig(CFG_POSITION);boolean smart=flag(p,POS_SMART),fixed=flag(p,POS_FIXED);int gps=(int)num(p,POS_GPS_MODE,0);int interval=seconds(p,POS_BROADCAST_SECS,900);int smartMin=seconds(p,POS_SMART_MIN_SECS,30);
            Boolean channels=channelsSharePosition(c);boolean source=fixed||gps==1;posPossible=source&&(channels==null||channels);
            if(posPossible){double rate=smart?3600.0/smartMin:3600.0/interval;b.sources.add(new Source(smart?"Position while driving (smart, every ≈ "+duration(smartMin)+")":"Position every "+duration(interval),rate,mtAir(c,BYTES_POSITION)));}
            items.add(new Item("mt_smart","Smart position",smart?"On":"Off","Off",smart&&posPossible?Status.CHANGE:Status.OK,true,
                "Off: while driving, the radio stops sending a position as often as every "+duration(smartMin)+". This is the largest source after relaying.",
                "Off: others see your position less often while you move."));
            items.add(new Item("mt_posint","Position interval",duration(interval),"12 h",interval<43200&&posPossible?Status.CHANGE:Status.OK,true,
                "Longer: fewer position packets when smart position is off.",
                "Longer: your position on other people's maps gets old."));
            String chText=channels==null?"Reading channels…":channels?"Shared on "+positionChannels(c).size()+" channel(s)":"Not shared";
            items.add(new Item("mt_poschan","Position on channels",chText,"Not shared",Boolean.TRUE.equals(channels)?Status.CHANGE:channels==null?Status.UNKNOWN:Status.OK,false,
                "Not shared: master switch. No position is sent, whatever the GPS, fixed position or phone settings are.",
                "Not shared: nobody on the mesh sees where you are."));
            if(!source)b.note="No position source is set on the radio (no GPS enabled and no fixed position). If the phone app shares its location with the radio, position packets are still sent; turn sharing off on the channels to be sure.";
        }
        if(c.mtModuleSupported(MOD_TELEMETRY)&&c.mtModuleLoaded(MOD_TELEMETRY)){
            byte[] t=c.mtModule(MOD_TELEMETRY);boolean modern=c.mtAtLeast(7,13);boolean deviceToMesh=modern?flag(t,TEL_DEVICE_TO_MESH):true;int deviceSecs=seconds(t,TEL_DEVICE_SECS,1800);
            if(deviceToMesh)b.sources.add(new Source("Battery / device telemetry every "+duration(deviceSecs),3600.0/deviceSecs,mtAir(c,BYTES_TELEMETRY)));
            int[][] sensors={{TEL_ENV,TEL_ENV_SECS},{TEL_AIR,TEL_AIR_SECS},{TEL_POWER,TEL_POWER_SECS},{TEL_HEALTH,TEL_HEALTH_SECS}};int sensorsOn=0;
            for(int[] s:sensors)if(flag(t,s[0])){sensorsOn++;int secs=seconds(t,s[1],1800);b.sources.add(new Source("Sensor telemetry every "+duration(secs)+" (if a sensor is fitted)",3600.0/secs,mtAir(c,BYTES_TELEMETRY)));}
            items.add(new Item("mt_devtel","Device telemetry to the mesh",modern?(deviceToMesh?"On, every "+duration(deviceSecs):"Off"):"Every "+duration(deviceSecs)+" (firmware before 2.7.13 always sends)",modern?"Off":"Longest interval",deviceToMesh&&(modern||deviceSecs<43200)?Status.CHANGE:Status.OK,true,
                "Off: no battery and channel-use reports over the air. The phone still receives them over Bluetooth.",
                "Off: others cannot see your battery level on the map."));
            items.add(new Item("mt_sensors","Sensor telemetry",sensorsOn==0?"Off":sensorsOn+" type(s) on","Off",sensorsOn>0?Status.CHANGE:Status.OK,true,
                "Off: no environment, air quality, power or health reports.",
                "Off: sensor data is not shared. Only matters if a sensor is fitted."));
        }else if(c.mtModuleSupported(MOD_TELEMETRY))items.add(new Item("mt_devtel","Device telemetry to the mesh","Reading…","Off",Status.UNKNOWN,true,"",""));
        if(c.mtModuleSupported(MOD_NEIGHBOR)&&c.mtModuleLoaded(MOD_NEIGHBOR)){
            byte[] n=c.mtModule(MOD_NEIGHBOR);boolean on=flag(n,1);boolean overLora=c.mtAtLeast(5,13)?flag(n,3):true;int secs=Math.max(14400,seconds(n,2,21600));
            if(on&&overLora)b.sources.add(new Source("Neighbor info every "+duration(secs),3600.0/secs,mtAir(c,BYTES_NEIGHBOR)));
            items.add(new Item("mt_neighbor","Neighbor info",on?"On":"Off","Off",on&&overLora?Status.CHANGE:Status.OK,true,"Off: no periodic neighbour reports.","Off: mesh mapping tools get less data from your node."));
        }
        if(c.mtModuleSupported(MOD_RANGE_TEST)&&c.mtModuleLoaded(MOD_RANGE_TEST)){
            byte[] rt=c.mtModule(MOD_RANGE_TEST);boolean on=flag(rt,1);long sender=num(rt,2,0);
            if(on&&sender>0)b.sources.add(new Source("Range test every "+duration((int)sender),3600.0/Math.max(15,sender),mtAir(c,BYTES_RANGE)));
            items.add(new Item("mt_range","Range test",on?(sender>0?"Sending every "+duration((int)sender):"Receiving"):"Off","Off",on&&sender>0?Status.CHANGE:Status.OK,true,"Off: no test packets.","Off: only needed while testing range."));
        }
        b.known=device&&lora;
        if(b.txOff){b.sources.clear();b.note="Transmission is off: this radio sends nothing until it is turned back on.";}
    }

    private static void analyzeMeshCore(ProtocolClient c,Report r){
        Budget b=r.mc;List<Item> items=r.mcItems;b.known=true;r.mcLoaded=true;
        b.note="Companion firmware does not relay and only sends adverts when you ask. Replies to direct messages and allowed requests are not counted.";
        if(c.hasRadioStats&&c.hasBattery&&c.uptimeSecs>0)r.mcTxPercent=100.0*c.txAirSecs/c.uptimeSecs;
        if(c.hasTxPower){r.mcTxDbm=c.txPower;items.add(new Item("mc_power","Transmit power",c.txPower+" dBm"+(c.maxTxPower>0?" (max "+c.maxTxPower+")":""),"20–22 dBm if your range allows",c.txPower>22?Status.CHANGE:Status.OK,false,
            "Lower: less power reaches the Meshtastic radio and less battery used.","Lower: shorter range for the MeshCore network."));}
        items.add(new Item("mc_relay","Relaying","Off (Companion firmware)","Off",Status.OK,false,"Companions never relay on the public preset.","—"));
        if(c.mcSupportsOther()){
            int loc=(c.telemetryModes>>2)&3,env=(c.telemetryModes>>4)&3;String[] modes={"Deny","Follow contact permissions","Allow all"};
            items.add(new Item("mc_loc","Location telemetry",modes[Math.min(2,loc)],"Deny",loc!=0?Status.CHANGE:Status.OK,false,"Deny: the radio does not answer location requests from other nodes.","Deny: contacts cannot ask where you are."));
            items.add(new Item("mc_env","Sensor telemetry",modes[Math.min(2,env)],"Deny",env!=0?Status.CHANGE:Status.OK,false,"Deny: the radio does not answer sensor requests.","Deny: contacts cannot read your sensors."));
            items.add(new Item("mc_acks","Extra acknowledgements",c.multiAcks?"On":"Off","Off",c.multiAcks?Status.CHANGE:Status.OK,false,"Off: each acknowledgement is sent only once.","Off: on very weak links an acknowledgement is a bit more likely to be lost."));
            items.add(new Item("mc_advloc","Location in adverts",c.advertLocationPolicy!=0?"Shared":"Not shared","Your choice",Status.INFO,false,"Not shared: more privacy. Does not change how often the radio transmits.","Not shared: you do not appear on maps."));
        }
        items.add(new Item("mc_advert","Adverts","Only when you send one","Prefer zero-hop",Status.INFO,false,"Zero-hop: heard only by nearby nodes and not relayed.","Zero-hop: distant nodes do not learn about you."));
    }

    // ---------------------------------------------------------------- presets

    /** A single change to one radio. done() is re-evaluated on the live client before applying. */
    static final class Step {
        final boolean meshtastic,restart;final String label;final Predicate<ProtocolClient> done;final Consumer<ProtocolClient> apply;
        Step(boolean meshtastic,String label,boolean restart,Predicate<ProtocolClient> done,Consumer<ProtocolClient> apply){this.meshtastic=meshtastic;this.label=label;this.restart=restart;this.done=done;this.apply=apply;}
    }

    static List<Step> plan(Profile profile,ProtocolClient mt,ProtocolClient mc){
        List<Step> steps=new ArrayList<>();
        if(mt!=null&&mt.ready){
            boolean relayOff=profile!=Profile.HELP_MESH;int nodeInfo=profile==Profile.MINIMUM?43200:10800;
            steps.add(deviceStep(relayOff,nodeInfo));
            if(profile!=Profile.HELP_MESH){int interval=profile==Profile.MINIMUM?43200:3600;
                steps.add(new Step(true,"Smart position off, position every "+duration(interval),true,
                    c->!c.mtConfigLoaded(CFG_POSITION)||!flag(c.mtConfig(CFG_POSITION),POS_SMART)&&seconds(c.mtConfig(CFG_POSITION),POS_BROADCAST_SECS,900)>=interval,
                    c->c.setMtConfig(CFG_POSITION,Wire.replace(Wire.replace(c.mtConfig(CFG_POSITION),POS_SMART,0),POS_BROADCAST_SECS,interval))));}
            if(profile==Profile.MINIMUM)for(int slot:positionChannels(mt))steps.add(channelPositionStep(slot));
            steps.add(new Step(true,"Device telemetry to the mesh off"+(profile==Profile.MINIMUM?", sensor telemetry off":""),true,
                c->!c.mtModuleLoaded(MOD_TELEMETRY)||telemetryDone(c,profile==Profile.MINIMUM),
                c->c.setMtModule(MOD_TELEMETRY,telemetryTarget(c,profile==Profile.MINIMUM))));
            if(profile!=Profile.HELP_MESH)steps.add(new Step(true,"Neighbor info off",true,c->!c.mtModuleLoaded(MOD_NEIGHBOR)||!flag(c.mtModule(MOD_NEIGHBOR),1),c->c.setMtModuleField(MOD_NEIGHBOR,1,0)));
            steps.add(new Step(true,"Range test off",true,c->!c.mtModuleLoaded(MOD_RANGE_TEST)||!flag(c.mtModule(MOD_RANGE_TEST),1),c->c.setMtModuleField(MOD_RANGE_TEST,1,0)));
        }
        if(mc!=null&&mc.ready&&mc.mcSupportsOther()){
            int loc=profile==Profile.MINIMUM?0:1;
            steps.add(new Step(false,"Location telemetry "+(loc==0?"deny":"follow contact permissions")+", sensor telemetry deny, extra acknowledgements off",false,
                c->((c.telemetryModes>>2)&3)==loc&&((c.telemetryModes>>4)&3)==0&&!c.multiAcks,
                c->c.setOtherParams(c.manualAddContacts,(c.telemetryModes&0xC3)|(loc<<2),c.advertLocationPolicy,false)));
        }
        return steps;
    }
    static List<Step> pending(List<Step> steps,ProtocolClient mt,ProtocolClient mc){List<Step> out=new ArrayList<>();for(Step s:steps){ProtocolClient c=s.meshtastic?mt:mc;if(c!=null&&!s.done.test(c))out.add(s);}return out;}
    static int restarts(List<Step> steps){int n=0;for(Step s:steps)if(s.restart)n++;return n;}

    static Step deviceStep(boolean relayOff,int nodeInfo){
        String label=(relayOff?"Relaying off":"Relaying on (all)")+", node info every "+duration(nodeInfo);
        return new Step(true,label,false,c->{if(!c.mtConfigLoaded(CFG_DEVICE))return true;byte[] d=c.mtConfig(CFG_DEVICE);int role=(int)num(d,DEV_ROLE,0),mode=(int)num(d,DEV_REBROADCAST,0);boolean off=role==ROLE_CLIENT_MUTE||mode==RB_NONE;
                return (relayOff?off&&!routerRole(role):(mode==RB_ALL&&role!=ROLE_CLIENT_MUTE))&&seconds(d,DEV_NODE_INFO_SECS,10800)>=nodeInfo;},
            c->{byte[] d=c.mtConfig(CFG_DEVICE);int role=(int)num(d,DEV_ROLE,0);
                if(relayOff){if(routerRole(role))d=Wire.replace(d,DEV_ROLE,ROLE_CLIENT);if(role!=ROLE_CLIENT_MUTE)d=Wire.replace(d,DEV_REBROADCAST,RB_NONE);}
                else {if(role==ROLE_CLIENT_MUTE)d=Wire.replace(d,DEV_ROLE,ROLE_CLIENT);d=Wire.replace(d,DEV_REBROADCAST,RB_ALL);}
                if(seconds(d,DEV_NODE_INFO_SECS,10800)<nodeInfo)d=Wire.replace(d,DEV_NODE_INFO_SECS,nodeInfo);
                c.setMtConfig(CFG_DEVICE,d);});
    }
    static Step channelPositionStep(int slot){
        return new Step(true,"Stop sharing position on channel "+slot,false,
            c->{byte[] ch=c.mtChannels.get(slot);if(ch==null)return true;return Wire.number(Wire.fields(Wire.bytes(Wire.fields(Wire.bytes(Wire.fields(ch),2)),7)),1,0)==0;},
            c->c.setMtChannelPositionPrecision(slot,0));
    }
    static boolean telemetryDone(ProtocolClient c,boolean sensors){
        byte[] t=c.mtModule(MOD_TELEMETRY);boolean device=c.mtAtLeast(7,13)?!flag(t,TEL_DEVICE_TO_MESH):seconds(t,TEL_DEVICE_SECS,1800)>=43200;
        if(!device)return false;if(!sensors)return true;return !flag(t,TEL_ENV)&&!flag(t,TEL_AIR)&&!flag(t,TEL_POWER)&&!flag(t,TEL_HEALTH);
    }
    static byte[] telemetryTarget(ProtocolClient c,boolean sensors){
        byte[] t=c.mtModule(MOD_TELEMETRY);
        t=c.mtAtLeast(7,13)?Wire.replace(t,TEL_DEVICE_TO_MESH,0):Wire.replace(t,TEL_DEVICE_SECS,43200);
        if(sensors)for(int f:new int[]{TEL_ENV,TEL_AIR,TEL_POWER,TEL_HEALTH})if(flag(t,f))t=Wire.replace(t,f,0);
        return t;
    }

    /** Text for the dashboard: "1.2 %". */
    static String percent(double fraction){if(Double.isNaN(fraction))return "—";double p=fraction*100;return p<0.1?"< 0.1 %":String.format(Locale.ROOT,p<10?"%.1f %%":"%.0f %%",p);}
    static String dbm(double value){return Double.isNaN(value)?"—":String.format(Locale.ROOT,"%+.0f dBm",value);}
}
