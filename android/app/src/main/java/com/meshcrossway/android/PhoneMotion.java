package com.meshcrossway.android;

/** Course of travel, not phone orientation. Pure state for the local map marker. */
final class PhoneMotion {
    static final long FRESH_MS=15000;
    static final class Fix {
        final double lat,lon,accuracy,speed,speedAccuracy,bearing,bearingAccuracy;
        final long elapsed;
        Fix(double lat,double lon,long elapsed,double accuracy,double speed,double speedAccuracy,double bearing,double bearingAccuracy){
            this.lat=lat;this.lon=lon;this.elapsed=elapsed;this.accuracy=accuracy;this.speed=speed;
            this.speedAccuracy=speedAccuracy;this.bearing=bearing;this.bearingAccuracy=bearingAccuracy;
        }
    }
    boolean moving;double bearing;private Fix anchor;private long last=-1;
    void reset(){moving=false;anchor=null;last=-1;}
    boolean update(Fix fix,long now){
        if(fix==null||!GeoMath.valid(fix.lat,fix.lon)||fix.elapsed<0||now-fix.elapsed<0||now-fix.elapsed>FRESH_MS
            ||!Double.isFinite(fix.accuracy)||fix.accuracy<=0||fix.accuracy>50){reset();return false;}
        if(fix.elapsed==last)return moving;
        if(fix.elapsed<last)return moving; // Never derive course from an out-of-order sample.
        last=fix.elapsed;
        double course=Double.NaN;boolean displaced=false;
        if(anchor!=null){
            double seconds=(fix.elapsed-anchor.elapsed)/1000.0;
            double distance=GeoMath.distance(anchor.lat,anchor.lon,fix.lat,fix.lon);
            displaced=seconds>=2&&seconds<=30&&distance>=Math.max(8,1.5*Math.hypot(anchor.accuracy,fix.accuracy))
                &&distance/seconds>=0.8&&distance/seconds<=100;
            if(displaced)course=FieldTools.bearing(anchor.lat,anchor.lon,fix.lat,fix.lon);
            if(seconds>30)anchor=fix;
        }
        boolean hasSpeed=Double.isFinite(fix.speed)&&fix.speed>=0;
        double threshold=moving?0.5:1.0;
        boolean reliableSpeed=hasSpeed&&fix.speed>=threshold&&fix.speed<=100
            &&(!Double.isFinite(fix.speedAccuracy)||(fix.speedAccuracy>=0&&fix.speed>1.5*fix.speedAccuracy));
        boolean travel=hasSpeed?reliableSpeed:displaced;
        boolean courseValid=Double.isFinite(fix.bearing)&&fix.bearing>=0&&fix.bearing<360
            &&(!Double.isFinite(fix.bearingAccuracy)||(fix.bearingAccuracy>=0&&fix.bearingAccuracy<=45));
        if(travel&&courseValid)course=fix.bearing;
        moving=travel&&Double.isFinite(course);
        if(moving)bearing=course;
        if(anchor==null||displaced||hasSpeed&&!reliableSpeed)anchor=fix;
        return moving;
    }
}
