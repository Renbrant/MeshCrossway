package com.meshcrossway.android;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.PackageManager;
import android.location.*;
import android.os.*;

/** Foreground-only phone position. Never calls either mesh protocol client. */
final class PhonePosition implements LocationListener {
    private final Context context;private final LocationManager manager;private boolean active;Location location;Runnable listener;
    PhonePosition(Context context){this.context=context;manager=context.getSystemService(LocationManager.class);}
    boolean permitted(){return context.checkSelfPermission(Manifest.permission.ACCESS_COARSE_LOCATION)==PackageManager.PERMISSION_GRANTED||context.checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)==PackageManager.PERMISSION_GRANTED;}
    @SuppressLint("MissingPermission") void start(){if(active||!permitted()||manager==null)return;active=true;try{for(String provider:new String[]{LocationManager.GPS_PROVIDER,LocationManager.NETWORK_PROVIDER}){
        if(provider.equals(LocationManager.GPS_PROVIDER)&&context.checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)!=PackageManager.PERMISSION_GRANTED)continue;
        if(manager.isProviderEnabled(provider)){Location last=manager.getLastKnownLocation(provider);if(last!=null&&System.currentTimeMillis()-last.getTime()<120000)onLocationChanged(last);manager.requestLocationUpdates(provider,3000,3,this,Looper.getMainLooper());}
    }}catch(SecurityException|IllegalArgumentException ignored){active=false;}}
    void stop(){if(manager!=null)manager.removeUpdates(this);active=false;}
    @Override public void onLocationChanged(Location fix){if(!GeoMath.valid(fix.getLatitude(),fix.getLongitude()))return;if(location!=null&&fix.getTime()<location.getTime())return;location=new Location(fix);if(listener!=null)listener.run();}
    @Override public void onStatusChanged(String provider,int status,Bundle extras){}
    @Override public void onProviderEnabled(String provider){}
    @Override public void onProviderDisabled(String provider){if(listener!=null)listener.run();}
}
