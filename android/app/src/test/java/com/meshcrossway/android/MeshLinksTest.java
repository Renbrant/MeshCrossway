package com.meshcrossway.android;

import java.util.*;

public final class MeshLinksTest {
    static int checks;
    static void check(boolean condition,String message){checks++;if(!condition)throw new AssertionError(message);}
    static byte[] packed(int... values){ByteArrayBuilder b=new ByteArrayBuilder();for(int v:values)b.varint(v);return b.bytes();}
    static final class ByteArrayBuilder{final java.io.ByteArrayOutputStream out=new java.io.ByteArrayOutputStream();void varint(long n){while((n&~127L)!=0){out.write((int)((n&127)|128));n>>>=7;}out.write((int)n);}byte[] bytes(){return out.toByteArray();}}
    static byte[] fixed32(long... values){byte[] b=new byte[values.length*4];for(int i=0;i<values.length;i++)Wire.put32(b,i*4,values[i]);return b;}
    static MeshLinks.Link only(MeshLinks store,MeshLinks.Source source){MeshLinks.Link found=null;for(MeshLinks.Link l:store.recent(200_000))if(l.source==source){check(found==null,"one "+source+" link");found=l;}return found;}
    static List<MeshLinks.Link> of(MeshLinks store,MeshLinks.Source source){ArrayList<MeshLinks.Link> r=new ArrayList<>();for(MeshLinks.Link l:store.recent(200_000))if(l.source==source)r.add(l);return r;}

    public static void main(String[] args){
        // Signal margin against the spreading factor's decoding floor.
        check(MeshLinks.quality(-5,11)==MeshLinks.Quality.GOOD,"SF11 -5 dB has 12.5 dB margin");
        check(MeshLinks.quality(-10,11)==MeshLinks.Quality.FAIR,"SF11 -10 dB has 7.5 dB margin");
        check(MeshLinks.quality(-15,11)==MeshLinks.Quality.WEAK,"SF11 -15 dB has 2.5 dB margin");
        check(MeshLinks.quality(-5,7)==MeshLinks.Quality.WEAK,"SF7 -5 dB is weak");
        check(MeshLinks.quality(-5,0)==MeshLinks.Quality.GOOD,"unknown SF assumes SF11");
        check(MeshLinks.quality(Double.NaN,11)==MeshLinks.Quality.UNKNOWN,"no SNR is not a colour");
        check(MeshLinks.opacity(0)>MeshLinks.opacity(MeshLinks.MAX_AGE_MS),"old links fade");

        // Meshtastic: a packet nobody relayed is a direct link; a relayed or MQTT one is not.
        ProtocolClientTest.Events events=new ProtocolClientTest.Events();ProtocolClient mt=ProtocolClientTest.mt(events);
        byte[] text=new Wire.Out().scalar(1,1).bytes(2,"hi".getBytes()).array();
        byte[] direct=new Wire.Out().fixed(1,200).fixed(2,0xffffffffL).fixed(6,1).scalar(15,3).scalar(9,3).fixed(8,Float.floatToIntBits(-4.5f)).scalar(12,-101).bytes(4,text).array();
        mt.receive(ProtocolClientTest.wrapped(2,direct),1000);
        MeshLinks.Link heard=only(mt.links,MeshLinks.Source.HEARD);
        check(heard!=null&&heard.rx.equals("100")&&heard.tx.equals("200")&&heard.snr==-4.5&&heard.rssi==-101&&heard.at==1000,"direct Meshtastic reception");
        byte[] relayed=new Wire.Out().fixed(1,300).fixed(2,0xffffffffL).fixed(6,2).scalar(15,3).scalar(9,2).fixed(8,Float.floatToIntBits(5f)).bytes(4,text).array();
        byte[] mqtt=new Wire.Out().fixed(1,301).fixed(2,0xffffffffL).fixed(6,3).scalar(15,3).scalar(9,3).scalar(14,1).fixed(8,Float.floatToIntBits(5f)).bytes(4,text).array();
        byte[] legacy=new Wire.Out().fixed(1,302).fixed(2,0xffffffffL).fixed(6,4).scalar(9,3).fixed(8,Float.floatToIntBits(5f)).bytes(4,text).array();
        mt.receive(ProtocolClientTest.wrapped(2,relayed),1001);mt.receive(ProtocolClientTest.wrapped(2,mqtt),1002);mt.receive(ProtocolClientTest.wrapped(2,legacy),1003);
        check(of(mt.links,MeshLinks.Source.HEARD).size()==1,"relayed, MQTT and hop_start-less packets draw no direct line");

        // NeighborInfo: reporter heard each neighbor.
        byte[] neighbors=new Wire.Out().scalar(1,200).bytes(4,new Wire.Out().scalar(1,400).fixed(2,Float.floatToIntBits(8.25f)).array()).bytes(4,new Wire.Out().scalar(1,0).fixed(2,Float.floatToIntBits(1f)).array()).array();
        byte[] info=new Wire.Out().fixed(1,200).fixed(2,0xffffffffL).fixed(6,5).bytes(4,new Wire.Out().scalar(1,71).bytes(2,neighbors).array()).array();
        mt.receive(ProtocolClientTest.wrapped(2,info),1100);
        MeshLinks.Link reported=only(mt.links,MeshLinks.Source.NEIGHBOR_INFO);
        check(reported!=null&&reported.rx.equals("200")&&reported.tx.equals("400")&&reported.snr==8.25,"neighbor report direction and SNR; node 0 skipped");

        // Traceroute: me(100) to 500 to dest 600, back 600 to 100. SNR ×4, -128 unknown.
        mt.requestMtMetrics("600",true);long request=mt.mtTraces.keySet().iterator().next();
        byte[] route=new Wire.Out().bytes(1,fixed32(500)).bytes(2,packed(-20,24)).bytes(4,packed(-128)).array();
        byte[] reply=new Wire.Out().fixed(1,600).fixed(2,100).fixed(6,7).bytes(4,new Wire.Out().scalar(1,70).bytes(2,route).scalar(6,request).array()).array();
        mt.receive(ProtocolClientTest.wrapped(2,reply),1200);
        List<MeshLinks.Link> hops=of(mt.links,MeshLinks.Source.TRACEROUTE);
        check(hops.size()==2,"two measured towards hops, unknown return hop skipped: "+hops.size());
        boolean first=false,second=false;for(MeshLinks.Link l:hops){if(l.rx.equals("500")&&l.tx.equals("100")&&l.snr==-5)first=true;if(l.rx.equals("600")&&l.tx.equals("500")&&l.snr==6)second=true;}
        check(first&&second,"towards hops measured at each receiver");
        byte[] unsolicited=new Wire.Out().fixed(1,700).fixed(2,100).fixed(6,8).bytes(4,new Wire.Out().scalar(1,70).bytes(2,route).scalar(6,99).array()).array();
        mt.receive(ProtocolClientTest.wrapped(2,unsolicited),1201);check(of(mt.links,MeshLinks.Source.TRACEROUTE).size()==2,"uncorrelated traceroute ignored");

        // Map lines: both ends need a position; links between the same pair become one line.
        ArrayList<MeshLinks.Endpoint> ends=new ArrayList<>(Arrays.asList(new MeshLinks.Endpoint("100","Me",true,40,-105),new MeshLinks.Endpoint("200","Hill",true,40.1,-105.1),new MeshLinks.Endpoint("400","Hidden",false,0,0)));
        mt.links.add("200","100",MeshLinks.Source.NEIGHBOR_INFO,-16,MeshLinks.NO_RSSI,1300);
        List<MeshLinks.Line> lines=MeshLinks.lines(RadioProfile.MESHTASTIC,"radio",ends,mt.links.recent(1400),11);
        check(lines.size()==1,"one line per pair and none to an unplaced node: "+lines.size());
        MeshLinks.Line line=lines.get(0);
        check(line.local()&&line.primary.source==MeshLinks.Source.HEARD&&line.quality==MeshLinks.Quality.GOOD&&line.links.size()==2,"your own measurement colours the line");
        String details=MeshLinks.describe(line,1400);
        check(details.contains("Me heard Hill")&&details.contains("Hill heard Me")&&details.contains("SF11"),"details name who measured: "+details);
        check(MeshLinks.lines(RadioProfile.MESHTASTIC,"radio",ends,mt.links.recent(1000+MeshLinks.MAX_AGE_MS+400),11).isEmpty(),"links expire after 24 hours");

        // MeshCore RX log: flood path [aa, bb] heard by self; advert origin precedes the path.
        String self="00".repeat(32),origin="cc".repeat(32);
        MeshLinks mc=new MeshLinks();
        byte[] frame=new byte[3+1+1+2+32+4];frame[0]=(byte)136;frame[1]=(byte)-28;frame[2]=(byte)-95;frame[3]=(byte)((4<<2)|1);frame[4]=2;frame[5]=(byte)0xaa;frame[6]=(byte)0xbb;System.arraycopy(Wire.unhex(origin),0,frame,7,32);
        mc.meshcoreLog(frame,self,2000);
        MeshLinks.Link last=only(mc,MeshLinks.Source.HEARD);
        check(last!=null&&last.rx.equals(self)&&last.tx.equals("bb")&&last.snr==-7&&last.rssi==-95,"last repeater heard by this radio");
        check(of(mc,MeshLinks.Source.PATH).size()==2,"path segments origin-aa-bb without invented SNR");
        byte[] directAdvert=new byte[3+1+1+32];directAdvert[0]=(byte)136;directAdvert[1]=20;directAdvert[2]=(byte)-60;directAdvert[3]=(byte)((4<<2)|1);directAdvert[4]=0;System.arraycopy(Wire.unhex(origin),0,directAdvert,5,32);
        MeshLinks zero=new MeshLinks();zero.meshcoreLog(directAdvert,self,2001);MeshLinks.Link originHeard=only(zero,MeshLinks.Source.HEARD);
        check(originHeard!=null&&originHeard.tx.equals(origin)&&originHeard.snr==5,"zero-hop advert heard from its origin");
        byte[] directRouted=frame.clone();directRouted[3]=(byte)((2<<2)|2);MeshLinks none=new MeshLinks();none.meshcoreLog(directRouted,self,2002);
        check(none.size()==0,"direct-routed packets carry no travelled path");
        none.meshcoreLog(Arrays.copyOf(frame,6),self,2003);check(none.size()==0,"truncated path ignored");

        // Prefix resolution: unique prefix resolves; an ambiguous one is not drawn.
        ArrayList<MeshLinks.Endpoint> mcEnds=new ArrayList<>(Arrays.asList(new MeshLinks.Endpoint(self,"Me",true,40,-105),new MeshLinks.Endpoint("bb"+"11".repeat(31),"Ridge",true,40.2,-105),new MeshLinks.Endpoint("aa"+"11".repeat(31),"A1",true,40.3,-105),new MeshLinks.Endpoint("aa"+"22".repeat(31),"A2",true,40.4,-105),new MeshLinks.Endpoint(origin,"Origin",true,40.5,-105)));
        List<MeshLinks.Line> mcLines=MeshLinks.lines(RadioProfile.MESHCORE,"radio",mcEnds,mc.recent(2100),9);
        boolean ridge=false;for(MeshLinks.Line l:mcLines){check(!l.a.name.startsWith("A")&&!l.b.name.startsWith("A"),"ambiguous aa hash not drawn");if((l.a.name+l.b.name).contains("Ridge")&&(l.a.name+l.b.name).contains("Me"))ridge=l.local()&&l.quality==MeshLinks.Quality.FAIR;}
        check(ridge&&mcLines.size()==1,"self <-> Ridge only, fair at SF9 -7 dB: "+mcLines.size());

        // MeshCore repeater neighbor lists, through the client.
        ProtocolClient client=ProtocolClientTest.mc(new ProtocolClientTest.Events());
        check(client.publicKey.equals(self),"fixture self key");
        McRemoteDiagnostics.Neighbors list=new McRemoteDiagnostics.Neighbors(1,0,Collections.singletonList(new McRemoteDiagnostics.Neighbor("bb1111111111",30,-3.25)),0);
        MeshLinks repeaters=new MeshLinks();repeaters.meshcoreRepeater("07"+"00".repeat(31),list,100_000);
        MeshLinks.Link table=only(repeaters,MeshLinks.Source.REPEATER);
        check(table!=null&&table.tx.equals("bb1111111111")&&table.snr==-3.25&&table.at==70_000,"repeater neighbor age and SNR");

        System.out.println("Map links: "+checks+" checks passed");
    }
}
