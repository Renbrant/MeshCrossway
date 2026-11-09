package com.meshcrossway.android;

/** Pure geometry shared by map, distance ordering and bounded offline selection. */
final class GeoMath {
    static final int MAX_TILES=2400; // Headroom for SDK source variants below provider's 3,000 limit.
    static boolean valid(double lat,double lon){return Double.isFinite(lat)&&Double.isFinite(lon)&&Math.abs(lat)<=90&&Math.abs(lon)<=180;}
    static double distance(double lat1,double lon1,double lat2,double lon2){if(!valid(lat1,lon1)||!valid(lat2,lon2))return Double.POSITIVE_INFINITY;double a=Math.toRadians(lat1),b=Math.toRadians(lat2),dlat=b-a,dlon=Math.toRadians(lon2-lon1);double h=Math.sin(dlat/2)*Math.sin(dlat/2)+Math.cos(a)*Math.cos(b)*Math.sin(dlon/2)*Math.sin(dlon/2);return 6371008.8*2*Math.asin(Math.sqrt(Math.min(1,h)));}
    static long tileEstimate(double south,double west,double north,double east,int maxZoom){
        if(!valid(south,west)||!valid(north,east)||north<=south||east<=west||east-west>180||maxZoom<1||maxZoom>15)return Long.MAX_VALUE;
        long count=0;for(int z=0;z<=maxZoom;z++){long n=1L<<z;long x1=(long)Math.floor((west+180)/360*n),x2=(long)Math.floor((east+180)/360*n);long y1=tileY(north,n),y2=tileY(south,n);count+=(Math.min(n-1,x2)-Math.max(0,x1)+1)*(Math.min(n-1,y2)-Math.max(0,y1)+1);if(count>MAX_TILES)return count;}return count;
    }
    private static long tileY(double lat,long n){double r=Math.toRadians(Math.max(-85.05112878,Math.min(85.05112878,lat)));return (long)Math.floor((1-Math.log(Math.tan(r)+1/Math.cos(r))/Math.PI)/2*n);}
}
