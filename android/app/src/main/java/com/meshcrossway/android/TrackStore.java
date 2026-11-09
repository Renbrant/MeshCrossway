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
        String id,name,state,folderId="";int color,count;boolean visible;long created;double distance;
        final List<FieldTools.Fix> points=new ArrayList<>();
        String summary(){return state+" · "+count+" points · "+String.format(Locale.ROOT,"%.2f km",distance/1000);}
    }
    static final class Folder {String id,name;Folder(String id,String name){this.id=id;this.name=name;}}
    final List<Folder> folders=new ArrayList<>();
    static final int[] COLORS={0xff72d8a5,0xffff8a24,0xff6cbde6,0xffe68bcc};
    final List<Track> tracks=new ArrayList<>();String error="";Runnable listener;
    private final HandlerThread thread=new HandlerThread("mesh-tracks");private final Handler worker,main=new Handler(Looper.getMainLooper());private boolean closed;
    private String active="";private int segment;private FieldTools.Fix previous;
    TrackStore(Context c){this(c,"meshcrossway-tracks.db");}
    TrackStore(Context c,String name){super(c,name,null,2);thread.start();worker=new Handler(thread.getLooper());work(db->{db.execSQL("UPDATE tracks SET state='paused' WHERE state='recording'");});}
    @Override public void onConfigure(SQLiteDatabase db){db.enableWriteAheadLogging();}
    @Override public void onCreate(SQLiteDatabase db){db.execSQL("CREATE TABLE tracks(id TEXT PRIMARY KEY,name TEXT,state TEXT,color INTEGER,visible INTEGER,created INTEGER,distance REAL DEFAULT 0,count INTEGER DEFAULT 0,folder_id TEXT NOT NULL DEFAULT '')");db.execSQL("CREATE TABLE folders(id TEXT PRIMARY KEY,name TEXT NOT NULL)");db.execSQL("CREATE TABLE fixes(track TEXT,seq INTEGER,segment INTEGER,lat REAL,lon REAL,accuracy REAL,altitude REAL,time INTEGER,PRIMARY KEY(track,seq))");}
    @Override public void onUpgrade(SQLiteDatabase db,int old,int next){
        if(old<2){
            db.execSQL("ALTER TABLE tracks ADD COLUMN folder_id TEXT NOT NULL DEFAULT ''");
            db.execSQL("CREATE TABLE IF NOT EXISTS folders(id TEXT PRIMARY KEY,name TEXT NOT NULL)");
        }
    }
    private interface Work{void run(SQLiteDatabase db)throws Exception;}
    private void work(Work work){if(closed)return;worker.post(()->{try{work.run(getWritableDatabase());publish();}catch(Exception e){main.post(()->{error="Track storage failed • stop recording and export saved tracks";if(listener!=null)listener.run();});}});}
    private void publish(){ArrayList<Track> result=new ArrayList<>();SQLiteDatabase db=getReadableDatabase();try(Cursor c=db.rawQuery("SELECT * FROM tracks ORDER BY created DESC LIMIT 100",null)){while(c.moveToNext()){Track t=new Track();t.id=c.getString(0);t.name=c.getString(1);t.state=c.getString(2);t.color=c.getInt(3);t.visible=c.getInt(4)==1;t.created=c.getLong(5);t.distance=c.getDouble(6);t.count=c.getInt(7);t.folderId=c.getString(8);if(t.visible)t.points.addAll(preview(db,t.id,t.count));result.add(t);}}
        ArrayList<Folder> directories=new ArrayList<>();
        try(Cursor c=db.rawQuery("SELECT id,name FROM folders ORDER BY name COLLATE NOCASE",null)){
            while(c.moveToNext())directories.add(new Folder(c.getString(0),c.getString(1)));
        }
        main.post(()->{if(closed)return;tracks.clear();tracks.addAll(result);folders.clear();folders.addAll(directories);error="";if(listener!=null)listener.run();});}
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
    void createFolder(String name){work(db->{
        try(Cursor c=db.rawQuery("SELECT count(*) FROM folders",null)){
            c.moveToFirst();if(c.getInt(0)>=60)throw new SQLiteException("Folder limit reached");
        }
        ContentValues value=new ContentValues();value.put("id",UUID.randomUUID().toString());value.put("name",name.trim());
        db.insertOrThrow("folders",null,value);
    });}
    void renameFolder(String id,String name){work(db->{
        ContentValues value=new ContentValues();value.put("name",name.trim());
        db.update("folders",value,"id=?",new String[]{id});
    });}
    void deleteFolder(String id){work(db->{
        db.beginTransaction();try{
            ContentValues value=new ContentValues();value.put("folder_id","");
            db.update("tracks",value,"folder_id=?",new String[]{id});
            db.delete("folders","id=?",new String[]{id});
            db.setTransactionSuccessful();
        }finally{db.endTransaction();}
    });}
    void moveTrack(String id,String folder){work(db->{
        ContentValues value=new ContentValues();value.put("folder_id",folder);
        db.update("tracks",value,"id=?",new String[]{id});
    });}
    void preview(String id,Consumer<List<FieldTools.Fix>> done){
        if(closed){main.post(()->done.accept(Collections.emptyList()));return;}
        worker.post(()->{
            List<FieldTools.Fix> points=Collections.emptyList();
            try{
                SQLiteDatabase db=getReadableDatabase();
                try(Cursor c=db.rawQuery("SELECT count FROM tracks WHERE id=?",new String[]{id})){
                    if(c.moveToFirst())points=preview(db,id,c.getInt(0));
                }
            }catch(Exception ignored){}
            List<FieldTools.Fix> value=points;
            main.post(()->done.accept(value));
        });
    }
    private static List<FieldTools.Fix> preview(SQLiteDatabase db,String id,int count){int stride=Math.max(1,(count+4999)/5000);ArrayList<FieldTools.Fix> result=new ArrayList<>();try(Cursor c=db.rawQuery("SELECT lat,lon,accuracy,altitude,time,segment FROM fixes WHERE track=? AND (seq % ?=0 OR seq=?) ORDER BY seq",new String[]{id,Integer.toString(stride),Integer.toString(count-1)})){while(c.moveToNext())result.add(new FieldTools.Fix(c.getDouble(0),c.getDouble(1),c.getDouble(2),c.isNull(3)?Double.NaN:c.getDouble(3),c.getLong(4),c.getInt(5)));}return result;}
    static List<FieldTools.Fix> points(SQLiteDatabase db,String id,int limit){ArrayList<FieldTools.Fix> points=new ArrayList<>();try(Cursor c=db.rawQuery("SELECT lat,lon,accuracy,altitude,time,segment FROM fixes WHERE track=? ORDER BY seq LIMIT ?",new String[]{id,Integer.toString(limit)})){while(c.moveToNext())points.add(new FieldTools.Fix(c.getDouble(0),c.getDouble(1),c.getDouble(2),c.isNull(3)?Double.NaN:c.getDouble(3),c.getLong(4),c.getInt(5)));}return points;}
    void locate(String id,Consumer<FieldTools.Fix> done){work(db->{ContentValues v=new ContentValues();v.put("visible",1);db.update("tracks",v,"id=?",new String[]{id});List<FieldTools.Fix> fixes=points(db,id,1);FieldTools.Fix first=fixes.isEmpty()?null:fixes.get(0);main.post(()->done.accept(first));});}
    void export(String id,OutputStream stream,Consumer<String> done){if(closed)return;worker.post(()->{try(Writer out=new OutputStreamWriter(stream,StandardCharsets.UTF_8)){SQLiteDatabase db=getReadableDatabase();String name;try(Cursor c=db.rawQuery("SELECT name FROM tracks WHERE id=?",new String[]{id})){if(!c.moveToFirst())throw new IOException();name=c.getString(0);}FieldTools.gpx(out,name,points(db,id,100000));main.post(()->done.accept(""));}catch(Exception e){main.post(()->done.accept("Track export failed"));}});}
    // Full-device migration backup. Structured JSON streams every recorded
    // point, unlike GPX preview/export, and includes virtual folder metadata.
    private static final String[] ARCHIVE_TABLES={"folders","tracks","fixes"};
    void exportArchive(OutputStream output,Consumer<String> done){
        if(closed){main.post(()->done.accept("Track database unavailable"));return;}
        worker.post(()->{
            String result="";
            try{
                android.util.JsonWriter writer=new android.util.JsonWriter(
                    new OutputStreamWriter(output,StandardCharsets.UTF_8));
                SQLiteDatabase db=getReadableDatabase();
                db.beginTransactionNonExclusive();
                try{
                    writer.beginObject();
                    writer.name("format").value("MeshCrosswayTracks");
                    writer.name("version").value(1);
                    for(String table:ARCHIVE_TABLES){
                        writer.name(table).beginArray();
                        try(Cursor c=db.rawQuery("SELECT * FROM "+table,null)){
                            int rows=0;
                            while(c.moveToNext()){
                                if(++rows>10_000_000)throw new IOException("Too many backup rows");
                                writer.beginObject();
                                for(int i=0;i<c.getColumnCount();i++){
                                    writer.name(c.getColumnName(i));
                                    switch(c.getType(i)){
                                        case Cursor.FIELD_TYPE_NULL:writer.nullValue();break;
                                        case Cursor.FIELD_TYPE_INTEGER:writer.value(c.getLong(i));break;
                                        case Cursor.FIELD_TYPE_FLOAT:{
                                            double value=c.getDouble(i);
                                            if(!Double.isFinite(value))throw new IOException("Invalid track number");
                                            writer.value(value);break;
                                        }
                                        case Cursor.FIELD_TYPE_STRING:writer.value(c.getString(i));break;
                                        default:throw new IOException("Unsupported track backup column");
                                    }
                                }
                                writer.endObject();
                            }
                        }
                        writer.endArray();
                    }
                    writer.endObject();
                    writer.flush();
                    db.setTransactionSuccessful();
                }finally{db.endTransaction();}
            }catch(Exception ignored){result="Track backup could not be exported";}
            final String status=result;
            main.post(()->done.accept(status));
        });
    }
    private static final Set<String> VALID_FOLDER_COLS=new HashSet<>(
        Arrays.asList("id","name"));
    private static final Set<String> VALID_TRACK_COLS=new HashSet<>(
        Arrays.asList("id","name","state","color","visible","created","distance","count","folder_id"));
    private static final Set<String> VALID_FIX_COLS=new HashSet<>(
        Arrays.asList("track","seq","segment","lat","lon","accuracy","altitude","time"));
    private static ContentValues archiveRow(android.util.JsonReader reader,String table)throws IOException{
        Set<String> columns=table.equals("folders")?VALID_FOLDER_COLS:
            table.equals("tracks")?VALID_TRACK_COLS:VALID_FIX_COLS;
        ContentValues values=new ContentValues();
        reader.beginObject();
        while(reader.hasNext()){
            String field=reader.nextName();
            if(!columns.contains(field))throw new IOException("Unsupported track field");
            android.util.JsonToken token=reader.peek();
            if(token==android.util.JsonToken.NULL){reader.nextNull();values.putNull(field);continue;}
            try{
                if(field.equals("distance")||field.equals("lat")||field.equals("lon")||
                   field.equals("accuracy")||field.equals("altitude")){
                    double value=reader.nextDouble();
                    if(!Double.isFinite(value))throw new IOException("Invalid coordinate");
                    values.put(field,value);
                }else if(field.equals("color")||field.equals("visible")||field.equals("count")||
                         field.equals("segment")||field.equals("seq")){
                    values.put(field,reader.nextInt());
                }else if(field.equals("created")||field.equals("time")){
                    values.put(field,reader.nextLong());
                }else{
                    String value=reader.nextString();
                    if(value.length()>128)throw new IOException("Invalid track text");
                    values.put(field,value);
                }
            }catch(NumberFormatException invalid){throw new IOException("Invalid numeric field",invalid);}
        }
        reader.endObject();
        if(values.size()!=columns.size())throw new IOException("Incomplete track backup row");
        if(table.equals("folders")){
            checkArchiveId(values.getAsString("id"));
            String name=values.getAsString("name");
            if(name==null||name.trim().isEmpty()||name.length()>80)throw new IOException("Invalid folder name");
        }else if(table.equals("tracks")){
            checkArchiveId(values.getAsString("id"));
            String name=values.getAsString("name");
            String folder=values.getAsString("folder_id");
            if(name==null||name.trim().isEmpty()||name.length()>80||
               folder==null||(!folder.isEmpty()&&!validArchiveId(folder)))
                throw new IOException("Invalid track name/folder");
            String state=values.getAsString("state");
            if(!Arrays.asList("saved","recording","paused").contains(state))
                throw new IOException("Invalid track status");
            if(values.getAsInteger("count")<0||values.getAsInteger("count")>100000||
               values.getAsDouble("distance")<0)throw new IOException("Invalid track metrics");
            // Never automatically restart a location recording after migration.
            if(state.equals("recording"))values.put("state","paused");
        }else{
            checkArchiveId(values.getAsString("track"));
            if(values.getAsInteger("seq")<0||values.getAsInteger("seq")>=100000||
                values.getAsInteger("segment")<0||
                !GeoMath.valid(values.getAsDouble("lat"),values.getAsDouble("lon")))
                throw new IOException("Invalid track fix");
        }
        return values;
    }
    private static boolean validArchiveId(String id){
        try{return id!=null&&UUID.fromString(id).toString().equalsIgnoreCase(id);}
        catch(Exception invalid){return false;}
    }
    private static void checkArchiveId(String id)throws IOException{
        if(!validArchiveId(id))throw new IOException("Invalid backup identifier");
    }
    void importArchive(InputStream input,Consumer<String> done){
        if(closed){main.post(()->done.accept("Track database unavailable"));return;}
        worker.post(()->{
            String result="";
            try{
                android.util.JsonReader reader=new android.util.JsonReader(
                    new InputStreamReader(input,StandardCharsets.UTF_8));
                SQLiteDatabase db=getWritableDatabase();
                db.beginTransaction();
                try{
                    reader.beginObject();
                    if(!"format".equals(reader.nextName())||
                       !"MeshCrosswayTracks".equals(reader.nextString())||
                       !"version".equals(reader.nextName())||reader.nextInt()!=1)
                        throw new IOException("Unsupported track backup");
                    int total=0;
                    for(String table:ARCHIVE_TABLES){
                        if(!table.equals(reader.nextName()))throw new IOException("Invalid track table order");
                        reader.beginArray();
                        while(reader.hasNext()){
                            if(++total>1_000_000)throw new IOException("Backup row limit exceeded");
                            ContentValues values=archiveRow(reader,table);
                            db.insertWithOnConflict(table,null,values,SQLiteDatabase.CONFLICT_IGNORE);
                        }
                        reader.endArray();
                    }
                    reader.endObject();
                    if(reader.peek()!=android.util.JsonToken.END_DOCUMENT)
                        throw new IOException("Trailing backup data");
                    db.setTransactionSuccessful();
                }finally{db.endTransaction();}
                publish();
            }catch(Exception ignored){result="Track backup rejected • existing tracks kept";}
            final String status=result;
            main.post(()->done.accept(status));
        });
    }
    void barrier(Runnable done){if(!closed)worker.post(()->main.post(done));}
    @Override public void close(){closed=true;listener=null;worker.post(()->{super.close();thread.quitSafely();});}
}
