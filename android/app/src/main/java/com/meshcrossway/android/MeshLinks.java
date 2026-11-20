package com.meshcrossway.android;

import java.util.*;

/**
 * Radio links between nodes, kept apart from routing. A link exists only when a radio
 * measured it or a node reported it: a hop count is never turned into a drawn line.
 * Each link records who received (and measured) the signal and who transmitted it.
 */
final class MeshLinks {
    enum Source {
        HEARD("Heard directly by your radio",true),
        NEIGHBOR_INFO("Meshtastic neighbor report",false),
        TRACEROUTE("Meshtastic traceroute hop",false),
        REPEATER("MeshCore repeater neighbor list",false),
        PATH("MeshCore flood path · no signal value",false);
        final String label;final boolean local;
        Source(String label,boolean local){this.label=label;this.local=local;}
    }
    enum Quality {
        GOOD("Strong",0xff3fbf6f),FAIR("Fair",0xffe8c547),WEAK("Weak",0xffe5484d),UNKNOWN("No signal value",0xff8a94a3);
        final String label;final int color;
        Quality(String label,int color){this.label=label;this.color=color;}
    }
    static final class Link {
        final String rx,tx;final Source source;double snr=Double.NaN;int rssi=NO_RSSI;long at;
        Link(String rx,String tx,Source source){this.rx=rx;this.tx=tx;this.source=source;}
    }
    static final int NO_RSSI=Integer.MIN_VALUE,MAX_LINKS=600;
    /** Links older than this are dropped from the map. Neighbor reports arrive every 4–6 hours. */
    static final long MAX_AGE_MS=24L*3600_000L;
    private final LinkedHashMap<String,Link> links=new LinkedHashMap<>();
    long revision;

    /** Records a measurement; at is SystemClock.elapsedRealtime() milliseconds. */
    void add(String rx,String tx,Source source,double snr,int rssi,long at){
        if(rx==null||tx==null||rx.isEmpty()||tx.isEmpty()||rx.equalsIgnoreCase(tx))return;
        rx=rx.toLowerCase(Locale.ROOT);tx=tx.toLowerCase(Locale.ROOT);
        if(source==Source.PATH&&rx.compareTo(tx)>0){String swap=rx;rx=tx;tx=swap;}
        String key=rx+"|"+tx+"|"+source;Link existing=links.get(key);
        if(existing!=null&&existing.at>at)return;
        Link link=new Link(rx,tx,source);link.snr=Double.isFinite(snr)?snr:Double.NaN;link.rssi=rssi;link.at=at;
        links.remove(key);links.put(key,link);
        while(links.size()>MAX_LINKS)links.remove(links.keySet().iterator().next());
        revision++;
    }
    void clear(){if(!links.isEmpty()){links.clear();revision++;}}
    int size(){return links.size();}
    List<Link> recent(long now){ArrayList<Link> result=new ArrayList<>();for(Link l:links.values())if(now-l.at<=MAX_AGE_MS)result.add(l);return result;}

    // ---- Signal quality -------------------------------------------------------------

    /** Demodulation floor (dB) per spreading factor, from the Semtech SX126x/SX127x datasheets. */
    static double floor(int sf){switch(sf){case 5:return -2.5;case 6:return -5;case 7:return -7.5;case 8:return -10;case 9:return -12.5;case 10:return -15;case 11:return -17.5;case 12:return -20;default:return Double.NaN;}}
    /** LongFast (SF11) is the most common setting, so it is assumed when the radio has not reported one. */
    static final int ASSUMED_SF=11;
    static double margin(double snr,int sf){double f=floor(sf);if(!Double.isFinite(f))f=floor(ASSUMED_SF);return snr-f;}
    static Quality quality(double snr,int sf){if(!Double.isFinite(snr))return Quality.UNKNOWN;double m=margin(snr,sf);return m>=10?Quality.GOOD:m>=5?Quality.FAIR:Quality.WEAK;}
    static int spreadingFactor(ProtocolClient c){
        if(c==null)return 0;
        if(c.profile==RadioProfile.MESHCORE)return c.spreadingFactor;
        if(!c.mtConfigLoaded(5))return 0;
        List<Wire.F> fs=Wire.fields(c.mtConfig(5));
        return Wire.number(fs,1,0)!=0?MtLora.presetSpreadingFactor((int)Wire.number(fs,2,0)):(int)Wire.number(fs,4,0);
    }

    // ---- Meshtastic -----------------------------------------------------------------

    static String mt(long num){return Long.toString(num&0xffffffffL);}
    static boolean mtNode(long num){num&=0xffffffffL;return num!=0&&num!=0xffffffffL;}
    /** NeighborInfo: the reporting node lists the 0-hop neighbors it heard and their SNR. */
    void meshtasticNeighbors(byte[] payload,long from,long now){try{neighbors(payload,from,now);}catch(IllegalArgumentException malformed){/* informative only */}}
    private void neighbors(byte[] payload,long from,long now){
        List<Wire.F> fs=Wire.fields(payload);long reporter=Wire.number(fs,1,from);if(!mtNode(reporter))reporter=from;
        for(Wire.F f:fs)if(f.tag==4&&f.wire==2){
            List<Wire.F> edge=Wire.fields(f.bytes);long neighbor=Wire.number(edge,1,0);if(!mtNode(neighbor))continue;
            Wire.F snr=Wire.field(edge,2);double value=snr!=null&&snr.wire==5?Float.intBitsToFloat((int)snr.number):Double.NaN;
            add(mt(reporter),mt(neighbor),Source.NEIGHBOR_INFO,value,NO_RSSI,now);
        }
    }
    /**
     * RouteDiscovery reply from dest: route/snr_towards describe self→dest, route_back/snr_back
     * dest→self. SNR values are dB×4, and -128 marks a hop whose SNR is unknown.
     */
    void meshtasticTrace(byte[] payload,long self,long dest,long now){
        try{List<Wire.F> fs=Wire.fields(payload);chain(fs,1,2,self,dest,now);chain(fs,3,4,dest,self,now);}
        catch(IllegalArgumentException malformed){/* the traceroute text metric still reports it */}
    }
    private void chain(List<Wire.F> fs,int routeTag,int snrTag,long start,long end,long now){
        ArrayList<Long> nodes=new ArrayList<>();nodes.add(start);ArrayList<Integer> snr=new ArrayList<>();boolean any=false;
        for(Wire.F f:fs){
            if(f.tag==routeTag){if(f.wire==2){if(f.bytes.length%4!=0)return;for(int i=0;i<f.bytes.length;i+=4)nodes.add(Wire.u32(f.bytes,i));}else if(f.wire==5)nodes.add(f.number&0xffffffffL);}
            else if(f.tag==snrTag){any=true;if(f.wire==2)snr.addAll(MtLora.packed(f.bytes));else if(f.wire==0)snr.add((int)f.number);}
        }
        nodes.add(end);
        // A route without any SNR list is from firmware older than 2.3 or an unanswered return path.
        if(!any)return;
        for(int i=0;i+1<nodes.size();i++){
            long tx=nodes.get(i),rx=nodes.get(i+1);if(!mtNode(tx)||!mtNode(rx))continue;
            double value=i<snr.size()&&snr.get(i)!=-128?snr.get(i)/4.0:Double.NaN;
            if(Double.isFinite(value))add(mt(rx),mt(tx),Source.TRACEROUTE,value,NO_RSSI,now);
        }
    }

    // ---- MeshCore -------------------------------------------------------------------

    /**
     * PUSH_CODE_LOG_RX_DATA (0x88): [code, snr×4, rssi, raw packet]. In a flood packet every
     * repeater appends its hash before retransmitting, so the last hash is the node this
     * radio heard. Direct packets carry remaining instructions, not travelled hops: ignored.
     */
    void meshcoreLog(byte[] frame,String self,long now){
        if(frame.length<5||(frame[0]&255)!=136||self==null||self.isEmpty())return;
        double snr=frame[1]/4.0;int rssi=frame[2];int pos=3;int header=frame[pos++]&255;
        int route=header&3,type=(header>>>2)&15;if((header>>>6)!=0||route==2||route==3)return;
        if(route==0)pos+=4;if(pos>=frame.length)return;
        RxInfo encoded=RxInfo.meshcore(frame[pos++]&255);if(!"flood".equals(encoded.kind))return;
        int bytes=encoded.hops*encoded.width;if(pos+bytes>frame.length)return;
        ArrayList<String> path=new ArrayList<>();for(int i=0;i<encoded.hops;i++)path.add(Wire.hex(Arrays.copyOfRange(frame,pos+i*encoded.width,pos+(i+1)*encoded.width)));
        pos+=bytes;
        // An advert names its origin with the full public key. The firmware verifies the
        // signature before it lists the contact, and only listed nodes are ever drawn.
        String origin=type==4&&frame.length-pos>=32?Wire.hex(Arrays.copyOfRange(frame,pos,pos+32)):"";
        if(path.isEmpty()){if(!origin.isEmpty())add(self,origin,Source.HEARD,snr,rssi,now);return;}
        add(self,path.get(path.size()-1),Source.HEARD,snr,rssi,now);
        for(int i=0;i+1<path.size();i++)add(path.get(i+1),path.get(i),Source.PATH,Double.NaN,NO_RSSI,now);
        if(!origin.isEmpty())add(path.get(0),origin,Source.PATH,Double.NaN,NO_RSSI,now);
    }
    /** A repeater's own neighbor table: the repeater measured each neighbor's adverts. */
    void meshcoreRepeater(String repeater,McRemoteDiagnostics.Neighbors list,long now){
        if(list==null)return;
        for(McRemoteDiagnostics.Neighbor n:list.list)add(repeater,n.prefix,Source.REPEATER,n.snr,NO_RSSI,now-Math.max(0,n.secondsAgo)*1000L);
    }

    // ---- Map lines ------------------------------------------------------------------

    static final class Endpoint {
        final String id,name;final boolean positioned;final double lat,lon;
        Endpoint(String id,String name,boolean positioned,double lat,double lon){this.id=id.toLowerCase(Locale.ROOT);this.name=name;this.positioned=positioned;this.lat=lat;this.lon=lon;}
    }
    static final class Line {
        RadioProfile profile;String radio="",key="";Endpoint a,b;Link primary;Quality quality;final List<Link> links=new ArrayList<>();int spreadingFactor;
        /** Whether each link's receiver resolved to end a (true) or end b (false). */
        final Map<Link,Boolean> receiverIsA=new IdentityHashMap<>();
        boolean local(){return primary.source.local;}
    }

    /**
     * Resolves link ends to known nodes. MeshCore paths and neighbor lists name nodes by a
     * key prefix: a prefix that matches more than one known node is ambiguous and not drawn.
     */
    static Endpoint resolve(String id,Map<String,Endpoint> exact,List<Endpoint> all,boolean prefixes){
        Endpoint found=exact.get(id);if(found!=null||!prefixes)return found;
        for(Endpoint e:all)if(e.id.startsWith(id)){if(found!=null&&!found.id.equals(e.id))return null;found=e;}
        return found;
    }
    static List<Line> lines(RadioProfile profile,String radio,List<Endpoint> endpoints,List<Link> links,int sf){
        LinkedHashMap<String,Endpoint> exact=new LinkedHashMap<>();for(Endpoint e:endpoints)exact.put(e.id,e);
        boolean prefixes=profile==RadioProfile.MESHCORE;
        LinkedHashMap<String,Line> grouped=new LinkedHashMap<>();
        for(Link link:links){
            Endpoint rx=resolve(link.rx,exact,endpoints,prefixes),tx=resolve(link.tx,exact,endpoints,prefixes);
            if(rx==null||tx==null||rx.id.equals(tx.id)||!rx.positioned||!tx.positioned)continue;
            if(!GeoMath.valid(rx.lat,rx.lon)||!GeoMath.valid(tx.lat,tx.lon))continue;
            Endpoint a=rx.id.compareTo(tx.id)<0?rx:tx,b=a==rx?tx:rx;String key=profile.name()+"|"+a.id+"|"+b.id;
            Line line=grouped.get(key);if(line==null){line=new Line();line.profile=profile;line.radio=radio;line.key=key;line.a=a;line.b=b;line.spreadingFactor=sf;grouped.put(key,line);}
            line.links.add(link);line.receiverIsA.put(link,rx==a);
        }
        for(Line line:grouped.values()){
            line.links.sort((x,y)->Long.compare(y.at,x.at));
            line.primary=pick(line.links);line.quality=quality(line.primary.snr,sf);
        }
        return new ArrayList<>(grouped.values());
    }
    /** Prefer a value measured by your radio, then any measured value, then the newest. */
    static Link pick(List<Link> newestFirst){
        for(Link l:newestFirst)if(l.source.local&&Double.isFinite(l.snr))return l;
        for(Link l:newestFirst)if(Double.isFinite(l.snr))return l;
        return newestFirst.get(0);
    }
    /** Full opacity for the first hour, then fading to 35% at the 24-hour limit. */
    static float opacity(long ageMs){if(ageMs<=3600_000L)return .95f;double f=Math.min(1,(ageMs-3600_000.0)/(MAX_AGE_MS-3600_000.0));return (float)(.95-.6*f);}
    static String age(long ms){long s=Math.max(0,ms/1000);if(s<60)return s+" s ago";if(s<3600)return s/60+" min ago";return s/3600+" h "+(s%3600)/60+" min ago";}
    static String describe(Line line,long now){
        StringBuilder text=new StringBuilder();
        text.append(line.quality.label);
        if(Double.isFinite(line.primary.snr))text.append(String.format(Locale.ROOT," · SNR %.1f dB · margin %.1f dB",line.primary.snr,margin(line.primary.snr,line.spreadingFactor)));
        text.append(line.spreadingFactor>0?" at SF"+line.spreadingFactor:" (SF not reported; SF"+ASSUMED_SF+" assumed)");
        text.append("\n");
        for(Link l:line.links){
            boolean aHeard=Boolean.TRUE.equals(line.receiverIsA.get(l));String rx=aHeard?line.a.name:line.b.name,tx=aHeard?line.b.name:line.a.name;
            text.append("\n• ").append(l.source.label).append(l.source==Source.PATH?"":"\n   "+rx+" heard "+tx);
            if(Double.isFinite(l.snr))text.append(String.format(Locale.ROOT," · SNR %.1f dB",l.snr));
            if(l.rssi!=NO_RSSI)text.append(" · RSSI ").append(l.rssi).append(" dBm");
            text.append(" · ").append(age(now-l.at));
        }
        text.append("\n\nSolid lines were measured by your radio. Dashed lines were reported by other nodes or seen in a packet path. Lines show radio contact, not a guaranteed route.");
        return text.toString();
    }
}
