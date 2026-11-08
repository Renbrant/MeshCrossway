package com.meshcrossway.android;

import java.io.*;
import java.util.*;
import javax.xml.parsers.DocumentBuilderFactory;
import org.w3c.dom.*;

public final class FieldToolsTest {
    static void check(boolean condition,String message){if(!condition)throw new AssertionError(message);}
    static FieldTools.Fix fix(double lat,double lon,double accuracy,long time,int segment){return new FieldTools.Fix(lat,lon,accuracy,123.4,time,segment);}
    public static void main(String[] args)throws Exception{
        long now=1700000000000L;FieldTools.Fix first=fix(40,-105,4,now,0);
        check(FieldTools.accept(null,first,now),"fresh accurate first point accepted");
        check(!FieldTools.accept(null,fix(40,-105,70,now,0),now),"poor accuracy excluded");
        check(!FieldTools.accept(null,first,now+31000),"cached fix excluded");
        check(!FieldTools.accept(first,fix(40,-105,4,now+3000,0),now+3000),"stationary jitter excluded");
        check(!FieldTools.accept(first,fix(45,-105,4,now+3000,0),now+3000),"GPS teleport excluded");
        check(!FieldTools.accept(first,fix(40.001,-105,4,now-3000,0),now),"out of order fix excluded");
        FieldTools.Fix next=fix(40.0001,-105,4,now+3000,0),resumed=fix(41,-104,4,now+6000,1);
        check(FieldTools.accept(first,next,now+3000),"moving fix accepted");check(FieldTools.accept(next,resumed,now+6000),"resume starts separate segment");
        check(Math.abs(FieldTools.bearing(0,0,0,1)-90)<0.001&&FieldTools.direction(359).equals("N"),"bearing and compass wrap");
        check(FieldTools.coordinates("Meet at 40.12345, -105.12345")!=null,"message coordinates recognized");check(FieldTools.coordinates("99.12345, 999.12345")==null,"invalid coordinates rejected");check(FieldTools.coordinates("SNR 9.00, 123 hops")==null,"radio metadata is not a location");
        StringWriter out=new StringWriter();FieldTools.gpx(out,"A & B <trail> 👣",Arrays.asList(first,next,resumed));DocumentBuilderFactory f=DocumentBuilderFactory.newInstance();f.setNamespaceAware(true);Document doc=f.newDocumentBuilder().parse(new ByteArrayInputStream(out.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8)));String ns="http://www.topografix.com/GPX/1/1";
        check(doc.getElementsByTagNameNS(ns,"trkseg").getLength()==2,"GPX pause breaks preserved");check(doc.getElementsByTagNameNS(ns,"trkpt").getLength()==3,"GPX points preserved");check(doc.getElementsByTagNameNS(ns,"name").item(0).getTextContent().equals("A & B <trail> 👣"),"GPX escaped UTF-8 name preserved");check(doc.getElementsByTagNameNS(ns,"time").item(0).getTextContent().equals("2023-11-14T22:13:20Z"),"GPX uses UTC");
        System.out.println("Field tools: GPS quality, gaps, bearing, coordinate parsing and GPX round trip passed");
    }
}
