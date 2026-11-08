package com.meshcrossway.android;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.*;
import android.content.*;
import android.content.pm.PackageManager;
import android.location.*;
import android.os.*;

/** Explicit user-started location foreground service; it never sends mesh packets. */
public final class TrackService extends Service implements LocationListener {
    final class LocalBinder extends Binder {TrackService service(){return TrackService.this;}}
    TrackStore store;Runnable listener;boolean recording;private boolean starting;String status="Not recording";private LocationManager manager;
    @Override public void onCreate(){super.onCreate();manager=getSystemService(LocationManager.class);store=new TrackStore(this);store.listener=()->{if(recording&&!store.error.isEmpty()){status=store.error;stopLocation();store.pause(false);}TrackStore.Track active=store.current();if(recording&&(active==null||!active.state.equals("recording")))stopLocation();if(listener!=null)listener.run();};getSystemService(NotificationManager.class).createNotificationChannel(new NotificationChannel("tracks","Track recording",NotificationManager.IMPORTANCE_LOW));}
    @Override public IBinder onBind(Intent intent){return new LocalBinder();}
    @Override public int onStartCommand(Intent intent,int flags,int startId){if(intent==null)return START_NOT_STICKY;String action=intent.getAction();if("PAUSE".equals(action)||"FINISH".equals(action)){pause("FINISH".equals(action));return START_NOT_STICKY;}
        boolean fine=checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)==PackageManager.PERMISSION_GRANTED;if(!fine||manager==null||!manager.isProviderEnabled(LocationManager.GPS_PROVIDER)){status="Enable precise phone location and GPS before recording";stopSelf();if(listener!=null)listener.run();return START_NOT_STICKY;}
        PendingIntent open=PendingIntent.getActivity(this,8,new Intent(this,MainActivity.class),PendingIntent.FLAG_IMMUTABLE|PendingIntent.FLAG_UPDATE_CURRENT);PendingIntent pause=PendingIntent.getService(this,9,new Intent(this,TrackService.class).setAction("PAUSE"),PendingIntent.FLAG_IMMUTABLE|PendingIntent.FLAG_UPDATE_CURRENT);
        Notification n=new Notification.Builder(this,"tracks").setSmallIcon(R.drawable.ic_launcher).setContentTitle("MeshCrossway · recording track").setContentText("Phone GPS · local recording · tap to open").setContentIntent(open).addAction(new Notification.Action.Builder(null,"Pause",pause).build()).setOngoing(true).build();startForeground(2,n);
        if(recording||starting)return START_NOT_STICKY;starting=true;java.util.function.Consumer<Boolean> started=ok->{if(!starting)return;starting=false;if(ok)startLocation();else{status="Could not start track • check track limit";stopLocation();if(listener!=null)listener.run();}};
        if("RESUME".equals(action))store.resume(intent.getStringExtra("track"),started);else if("START".equals(action))store.start(intent.getStringExtra("name")==null?"Track":intent.getStringExtra("name"),started);else stopLocation();return START_NOT_STICKY;
    }
    @SuppressLint("MissingPermission") private void startLocation(){try{recording=true;status="Recording • waiting for a fresh GPS fix (±50 m or better)";manager.requestLocationUpdates(LocationManager.GPS_PROVIDER,3000,3,this,Looper.getMainLooper());if(listener!=null)listener.run();}catch(SecurityException|IllegalArgumentException e){pause(false);status="GPS unavailable • track paused";}}
    private void stopLocation(){starting=false;recording=false;if(manager!=null)manager.removeUpdates(this);stopForeground(STOP_FOREGROUND_REMOVE);stopSelf();}
    void pause(boolean finish){stopLocation();store.pause(finish);status=finish?"Track saved":"Track paused";if(listener!=null)listener.run();}
    @Override public void onLocationChanged(Location fix){long age=(SystemClock.elapsedRealtimeNanos()-fix.getElapsedRealtimeNanos())/1000000;if(!recording||age<0||age>30000)return;FieldTools.Fix point=new FieldTools.Fix(fix.getLatitude(),fix.getLongitude(),fix.hasAccuracy()?fix.getAccuracy():Double.NaN,fix.hasAltitude()?fix.getAltitude():Double.NaN,fix.getTime(),0);store.add(point,System.currentTimeMillis());status=FieldTools.usable(point,System.currentTimeMillis())?"Recording • GPS ±"+Math.round(fix.getAccuracy())+" m":"Recording • waiting for a better GPS fix";}
    @Override public void onProviderDisabled(String provider){status="GPS disabled • recording has a gap";if(listener!=null)listener.run();}
    @Override public void onProviderEnabled(String provider){}
    @Override public void onStatusChanged(String provider,int status,Bundle extras){}
    @Override public void onDestroy(){stopLocation();store.pause(false);store.close();super.onDestroy();}
}
