package com.meshcrossway.android;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.time.Instant;
import java.util.*;
import java.util.regex.*;

/** Local field geometry and GPX serialization; no radio or network dependencies. */
final class FieldTools {
    static final class Fix {
        final double lat,lon,accuracy,altitude;final long time;final int segment;
        Fix(double lat,double lon,double accuracy,double altitude,long time,int segment){this.lat=lat;this.lon=lon;this.accuracy=accuracy;this.altitude=altitude;this.time=time;this.segment=segment;}
    }
    static boolean usable(Fix f,long now){return GeoMath.valid(f.lat,f.lon)&&Double.isFinite(f.accuracy)&&f.accuracy>0&&f.accuracy<=50&&f.time>0&&now-f.time>=-5000&&now-f.time<=30000;}
    static boolean accept(Fix previous,Fix fix,long now){
        if(!usable(fix,now))return false;if(previous==null||previous.segment!=fix.segment)return true;
        long delta=fix.time-previous.time;if(delta<2000)return false;
        double distance=GeoMath.distance(previous.lat,previous.lon,fix.lat,fix.lon);
        return distance>=3&&distance/(delta/1000.0)<=150;
    }
    static double bearing(double a,double b,double c,double d){double p=Math.toRadians(a),q=Math.toRadians(c),l=Math.toRadians(d-b);return (Math.toDegrees(Math.atan2(Math.sin(l)*Math.cos(q),Math.cos(p)*Math.sin(q)-Math.sin(p)*Math.cos(q)*Math.cos(l)))+360)%360;}
    static String direction(double bearing){return new String[]{"N","NE","E","SE","S","SW","W","NW"}[(int)Math.round(bearing/45)%8];}
    // Decimal coordinates separated by comma; avoid treating arbitrary pairs of numbers as locations.
    private static final Pattern COORD=Pattern.compile("(?<![\\w.])([+-]?\\d{1,2}\\.\\d{3,8})\\s*,\\s*([+-]?\\d{1,3}\\.\\d{3,8})(?![\\d.])");
    static double[] coordinates(String text){Matcher m=COORD.matcher(text);while(m.find()){double a=Double.parseDouble(m.group(1)),b=Double.parseDouble(m.group(2));if(GeoMath.valid(a,b))return new double[]{a,b};}return null;}
    static String xml(String s){return s.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;").replace("\"","&quot;").replace("'","&apos;").replaceAll("[\\x00-\\x08\\x0B\\x0C\\x0E-\\x1F]","");}
    static void gpx(Writer out,String name,List<Fix> fixes)throws IOException{
        out.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<gpx version=\"1.1\" creator=\"MeshCrossway\" xmlns=\"http://www.topografix.com/GPX/1/1\"><trk><name>"+xml(name)+"</name>");
        int segment=Integer.MIN_VALUE;for(Fix f:fixes){if(segment!=f.segment){if(segment!=Integer.MIN_VALUE)out.write("</trkseg>");out.write("<trkseg>");segment=f.segment;}
            out.write(String.format(Locale.ROOT,"<trkpt lat=\"%.7f\" lon=\"%.7f\">",f.lat,f.lon));if(Double.isFinite(f.altitude))out.write(String.format(Locale.ROOT,"<ele>%.2f</ele>",f.altitude));out.write("<time>"+Instant.ofEpochMilli(f.time)+"</time></trkpt>");}
        if(segment!=Integer.MIN_VALUE)out.write("</trkseg>");out.write("</trk></gpx>\n");out.flush();
    }
}
