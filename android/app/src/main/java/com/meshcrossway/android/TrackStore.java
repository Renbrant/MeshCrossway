package com.meshcrossway.android;

import android.content.*;
import android.database.Cursor;
import android.database.sqlite.*;
import android.os.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.function.Consumer;

/** Tracks have their own local database and worker, independent of both BLE stacks. */
final class TrackStore extends SQLiteOpenHelper {
    static final class Track {
        String id,name,state;int color,count;boolean visible;long created;double distance;
        final List<FieldTools.Fix> points=new ArrayList<>();
        String summary(){return state+" · "+count+" points · "+String.format(Locale.ROOT,"%.2f km",distance/1000);}
    }
    static final int[] COLORS={0xff72d8a5,0xffff8a24,0xff6cbde6,0xffe68bcc};
    final List<Track> tracks=new ArrayList<>();String error="";Runnable listener;
    private final HandlerThread thread=new HandlerThread("mesh-tracks");private final Handler worker,main=new Handler(Looper.getMainLooper());private boolean closed;
    private String active="";private int segment;private FieldTools.Fix previous;
    TrackStore(Context c){this(c,"meshcrossway-tracks.db");}
    TrackStore(Context c,String name){super(c,name,null,1);thread.start();worker=new Handler(thread.getLooper());work(db->{db.execSQL("UPDATE tracks SET state='paused' WHERE state='recording'");});}
    @Override public void onConfigure(SQLiteDatabase db){db.enableWriteAheadLogging();}
    @Override public void onCreate(SQLiteDatabase db){db.execSQL("CREATE TABLE tracks(id TEXT PRIMARY KEY,name TEXT,state TEXT,color INTEGER,visible INTEGER,created INTEGER,distance REAL DEFAULT 0,count INTEGER DEFAULT 0)");db.execSQL("CREATE TABLE fixes(track TEXT,seq INTEGER,segment INTEGER,lat REAL,lon REAL,accuracy REAL,altitude REAL,time INTEGER,PRIMARY KEY(track,seq))");}
    @Override public void onUpgrade(SQLiteDatabase db,int old,int next){}
    private interface Work{void run(SQLiteDatabase db)throws Exception;}
    private void work(Work work){if(closed)return;worker.post(()->{try{work.run(getWritableDatabase());publish();}catch(Exception e){main.post(()->{error="Track storage failed • stop recording and export saved tracks";if(listener!=null)listener.run();});}});}
    private void publish(){ArrayList<Track> result=new ArrayList<>();SQLiteDatabase db=getReadableDatabase();try(Cursor c=db.rawQuery("SELECT * FROM tracks ORDER BY created DESC LIMIT 100",null)){while(c.moveToNext()){Track t=new Track();t.id=c.getString(0);t.name=c.getString(1);t.state=c.getString(2);t.color=c.getInt(3);t.visible=c.getInt(4)==1;t.created=c.getLong(5);t.distance=c.getDouble(6);t.count=c.getInt(7);if(t.visible)t.points.addAll(preview(db,t.id,t.count));result.add(t);}}main.post(()->{if(closed)return;tracks.clear();tracks.addAll(result);error="";if(listener!=null)listener.run();});}
    Track current(){for(Track t:tracks)if(!t.state.equals("saved"))return t;return null;}
    void start(String name,Consumer<Boolean> done){work(db->{try(Cursor c=db.rawQuery("SELECT count(*) FROM tracks",null)){c.moveToFirst();if(c.getInt(0)>=100){main.post(()->done.accept(false));return;}}db.execSQL("UPDATE tracks SET state='saved' WHERE state!='saved'");active=UUID.randomUUID().toString();segment=0;previous=null;ContentValues v=new ContentValues();v.put("id",active);v.put("name",name.trim().isEmpty()?"Track":name.trim().substring(0,Math.min(name.trim().length(),80)));v.put("state","recording");v.put("color",COLORS[0]);v.put("visible",1);v.put("created",System.currentTimeMillis());db.insertOrThrow("tracks",null,v);main.post(()->done.accept(true));});}
    void resume(String id,Consumer<Boolean> done){work(db->{try(Cursor c=db.rawQuery("SELECT state FROM tracks WHERE id=?",new String[]{id})){if(!c.moveToFirst()||!c.getString(0).equals("paused")){main.post(()->done.accept(false));return;}}db.execSQL("UPDATE tracks SET state='paused' WHERE state='recording'");active=id;previous=null;try(Cursor c=db.rawQuery("SELECT coalesce(max(segment),-1)+1 FROM fixes WHERE track=?",new String[]{id})){c.moveToFirst();segment=c.getInt(0);}db.execSQL("UPDATE tracks SET state='recording' WHERE id=?",new Object[]{id});main.post(()->done.accept(true));});}
    void pause(boolean finish){work(db->{db.execSQL("UPDATE tracks SET state=? WHERE state!='saved'",new Object[]{finish?"saved":"paused"});active="";previous=null;});}
    void add(FieldTools.Fix raw,long now){work(db->{if(active.isEmpty())return;int seg=segment;if(previous!=null&&raw.time-previous.time>120000)seg++;
        FieldTools.Fix fix=new FieldTools.Fix(raw.lat,raw.lon,raw.accuracy,raw.altitude,raw.time,seg);if(!FieldTools.accept(previous,fix,now))return;
        int count;try(Cursor c=db.rawQuery("SELECT count FROM tracks WHERE id=?",new String[]{active})){if(!c.moveToFirst())return;count=c.getInt(0);}if(count>=100000){db.execSQL("UPDATE tracks SET state='paused' WHERE id=?",new Object[]{active});active="";main.post(()->{error="Track point limit reached • finish this track";if(listener!=null)listener.run();});return;}
        double distance=previous!=null&&previous.segment==seg?GeoMath.distance(previous.lat,previous.lon,fix.lat,fix.lon):0;ContentValues v=new ContentValues();v.put("track",active);v.put("seq",count);v.put("segment",seg);v.put("lat",fix.lat);v.put("lon",fix.lon);v.put("accuracy",fix.accuracy);if(Double.isFinite(fix.altitude))v.put("altitude",fix.altitude);v.put("time",fix.time);
        db.beginTransaction();try{db.insertOrThrow("fixes",null,v);db.execSQL("UPDATE tracks SET count=count+1,distance=distance+? WHERE id=?",new Object[]{distance,active});db.setTransactionSuccessful();}finally{db.endTransaction();}previous=fix;segment=seg;});}
    void metadata(String id,String name,int color,boolean visible){work(db->{ContentValues v=new ContentValues();v.put("name",name.trim().isEmpty()?"Track":name.trim().substring(0,Math.min(80,name.trim().length())));v.put("color",color);v.put("visible",visible?1:0);db.update("tracks",v,"id=?",new String[]{id});});}
    void delete(String id){work(db->{try(Cursor c=db.rawQuery("SELECT state FROM tracks WHERE id=?",new String[]{id})){if(!c.moveToFirst()||!c.getString(0).equals("saved"))return;}db.beginTransaction();try{db.delete("fixes","track=?",new String[]{id});db.delete("tracks","id=?",new String[]{id});db.setTransactionSuccessful();}finally{db.endTransaction();}});}
    private static List<FieldTools.Fix> preview(SQLiteDatabase db,String id,int count){int stride=Math.max(1,(count+4999)/5000);ArrayList<FieldTools.Fix> result=new ArrayList<>();try(Cursor c=db.rawQuery("SELECT lat,lon,accuracy,altitude,time,segment FROM fixes WHERE track=? AND (seq % ?=0 OR seq=?) ORDER BY seq",new String[]{id,Integer.toString(stride),Integer.toString(count-1)})){while(c.moveToNext())result.add(new FieldTools.Fix(c.getDouble(0),c.getDouble(1),c.getDouble(2),c.isNull(3)?Double.NaN:c.getDouble(3),c.getLong(4),c.getInt(5)));}return result;}
    static List<FieldTools.Fix> points(SQLiteDatabase db,String id,int limit){ArrayList<FieldTools.Fix> points=new ArrayList<>();try(Cursor c=db.rawQuery("SELECT lat,lon,accuracy,altitude,time,segment FROM fixes WHERE track=? ORDER BY seq LIMIT ?",new String[]{id,Integer.toString(limit)})){while(c.moveToNext())points.add(new FieldTools.Fix(c.getDouble(0),c.getDouble(1),c.getDouble(2),c.isNull(3)?Double.NaN:c.getDouble(3),c.getLong(4),c.getInt(5)));}return points;}
    void export(String id,OutputStream stream,Consumer<String> done){if(closed)return;worker.post(()->{try(Writer out=new OutputStreamWriter(stream,StandardCharsets.UTF_8)){SQLiteDatabase db=getReadableDatabase();String name;try(Cursor c=db.rawQuery("SELECT name FROM tracks WHERE id=?",new String[]{id})){if(!c.moveToFirst())throw new IOException();name=c.getString(0);}FieldTools.gpx(out,name,points(db,id,100000));main.post(()->done.accept(""));}catch(Exception e){main.post(()->done.accept("Track export failed"));}});}
    void barrier(Runnable done){if(!closed)worker.post(()->main.post(done));}
    @Override public void close(){closed=true;listener=null;worker.post(()->{super.close();thread.quitSafely();});}
}
