package com.meshcrossway.android;

import android.content.*;
import android.database.Cursor;
import android.database.sqlite.*;
import android.os.*;
import java.util.*;
import static com.meshcrossway.android.MeshData.*;
import com.meshcrossway.android.MeshData.Message;

/** Private durable data. Keys always include network + radio; channel topics include key fingerprint.
 * All production database access runs on a dedicated worker. No payloads/keys are logged. */
final class HistoryStore extends SQLiteOpenHelper {
    private final HandlerThread thread=new HandlerThread("mesh-history");private final Handler worker,main=new Handler(Looper.getMainLooper());
    final EnumMap<RadioProfile,Snapshot> snapshots=new EnumMap<>(RadioProfile.class);private final EnumMap<RadioProfile,String> radios=new EnumMap<>(RadioProfile.class);
    int historyLimit=2000;
    Runnable listener;String error="";private boolean closed;
    private final Runnable reloadTask=this::reload;
    HistoryStore(Context context){this(context,"meshcrossway-history.db");}
    HistoryStore(Context context,String name){super(context,name,null,1);thread.start();worker=new Handler(thread.getLooper());for(RadioProfile p:RadioProfile.values())snapshots.put(p,Snapshot.empty());}
    @Override public void onConfigure(SQLiteDatabase db){db.enableWriteAheadLogging();}
    @Override public void onCreate(SQLiteDatabase db){
        db.execSQL("CREATE TABLE nodes(network TEXT,radio TEXT,id TEXT,name TEXT,key TEXT,heard INTEGER,lat REAL,lon REAL,position INTEGER,battery INTEGER,type INTEGER,blocked INTEGER,PRIMARY KEY(network,radio,id))");
        db.execSQL("CREATE TABLE channels(network TEXT,radio TEXT,slot INTEGER,name TEXT,identity_hash TEXT,enabled INTEGER,PRIMARY KEY(network,radio,slot))");
        db.execSQL("CREATE TABLE messages(network TEXT,radio TEXT,uid TEXT,topic TEXT,title TEXT,sender TEXT,body TEXT,state TEXT,time INTEGER,outgoing INTEGER,is_read INTEGER,peer TEXT,identity_hash TEXT,slot INTEGER,is_group INTEGER,PRIMARY KEY(network,radio,uid))");
        db.execSQL("CREATE INDEX message_scope_time ON messages(network,radio,time)");
        db.execSQL("CREATE INDEX message_conversation ON messages(network,radio,topic,is_read)");
        db.execSQL("CREATE TABLE drafts(network TEXT,radio TEXT,topic TEXT,body TEXT,PRIMARY KEY(network,radio,topic))");
    }
    @Override public void onUpgrade(SQLiteDatabase db,int old,int version){throw new IllegalStateException("Unsupported history migration");}
    private interface Work {void run(SQLiteDatabase db);}
    private void work(Work action){work(action,true);}
    private void work(Work action,boolean refresh){if(closed)return;worker.post(()->{try{action.run(getWritableDatabase());main.post(()->{error="";if(refresh){main.removeCallbacks(reloadTask);main.postDelayed(reloadTask,150);}});}catch(SQLiteException e){main.post(()->{error="History storage unavailable • do not uninstall; free phone storage and reopen";if(listener!=null)listener.run();});}});}
    void select(RadioProfile p,String radio){radios.put(p,radio);snapshots.put(p,Snapshot.empty());reload();}
    private static ContentValues scope(RadioProfile p,String r){ContentValues v=new ContentValues();v.put("network",p.name());v.put("radio",r);return v;}
    void node(RadioProfile p,String r,Node n){ContentValues v=scope(p,r);v.put("id",n.id);v.put("name",n.name);v.put("key",n.key);v.put("heard",n.heard);v.put("lat",n.lat);v.put("lon",n.lon);v.put("position",n.position?1:0);v.put("battery",n.battery);v.put("type",n.type);v.put("blocked",n.unmessagable?1:0);work(db->db.insertWithOnConflict("nodes",null,v,SQLiteDatabase.CONFLICT_REPLACE));}
    void channel(RadioProfile p,String r,Channel c){ContentValues v=scope(p,r);v.put("slot",c.slot);v.put("name",c.name);v.put("identity_hash",c.identity);v.put("enabled",c.enabled?1:0);work(db->db.insertWithOnConflict("channels",null,v,SQLiteDatabase.CONFLICT_REPLACE));}
    private ContentValues messageValues(RadioProfile p,String r,Message m){ContentValues v=scope(p,r);v.put("uid",m.uid);v.put("topic",m.topic);v.put("title",m.title);v.put("sender",m.sender);v.put("body",m.body);v.put("state",m.state);v.put("time",m.timestamp);v.put("outgoing",m.outgoing?1:0);v.put("is_read",m.read?1:0);v.put("peer",m.peer);v.put("identity_hash",m.identity);v.put("slot",m.slot);v.put("is_group",m.group?1:0);return v;}
    void message(RadioProfile p,String r,Message m){ContentValues v=messageValues(p,r,m);work(db->db.insertWithOnConflict("messages",null,v,SQLiteDatabase.CONFLICT_IGNORE));}
    void outgoing(RadioProfile p,String r,Message m,Runnable onSaved,Runnable onError){ContentValues v=messageValues(p,r,m);if(closed){onError.run();return;}worker.post(()->{try{long row=getWritableDatabase().insertOrThrow("messages",null,v);if(row<0)throw new SQLiteException();main.post(()->{onSaved.run();reload();});}catch(SQLiteException e){main.post(()->{error="Could not save message • transmission cancelled";onError.run();if(listener!=null)listener.run();});}});}
    void delivery(RadioProfile p,String r,String uid,String state){work(db->{
        // Late BLE callbacks cannot downgrade an acknowledged or accepted packet.
        String extra=state.equals("submitted")?" AND state='queued'":state.equals("radio accepted")?" AND state IN ('queued','submitted','uncertain')":state.equals("uncertain")?" AND state IN ('queued','submitted','radio accepted')":" AND state!='delivered'";
        ContentValues v=new ContentValues();v.put("state",state);db.update("messages",v,"network=? AND radio=? AND uid=?"+extra,new String[]{p.name(),r,uid});});}
    void uncertain(RadioProfile p,String r){work(db->{ContentValues v=new ContentValues();v.put("state","uncertain");db.update("messages",v,"network=? AND radio=? AND outgoing=1 AND state IN ('queued','submitted','radio accepted')",new String[]{p.name(),r});});}
    void read(RadioProfile p,String r,String topic){work(db->{ContentValues v=new ContentValues();v.put("is_read",1);db.update("messages",v,"network=? AND radio=? AND topic=? AND is_read=0",new String[]{p.name(),r,topic});});}
    void draft(RadioProfile p,String r,String topic,String body){ContentValues v=scope(p,r);v.put("topic",topic);v.put("body",body);work(db->db.insertWithOnConflict("drafts",null,v,SQLiteDatabase.CONFLICT_REPLACE),false);}
    void loadDraft(RadioProfile p,String r,String topic,java.util.function.Consumer<String> callback){if(closed)return;worker.post(()->{String body="";try(Cursor c=getReadableDatabase().query("drafts",new String[]{"body"},"network=? AND radio=? AND topic=?",new String[]{p.name(),r,topic},null,null,null)){if(c.moveToFirst())body=c.getString(0);}catch(SQLiteException ignored){}String result=body;main.post(()->callback.accept(result));});}
    private void reload(){
        if(closed)return;EnumMap<RadioProfile,String> selection=new EnumMap<>(radios);
        worker.post(()->{try{EnumMap<RadioProfile,Snapshot> result=new EnumMap<>(RadioProfile.class);for(Map.Entry<RadioProfile,String> entry:selection.entrySet())result.put(entry.getKey(),load(getReadableDatabase(),entry.getKey(),entry.getValue(),historyLimit));
            main.post(()->{if(!selection.equals(radios)||closed)return;snapshots.putAll(result);if(listener!=null)listener.run();});
        }catch(SQLiteException e){main.post(()->{error="History could not be loaded";if(listener!=null)listener.run();});}});
    }
    void older(){historyLimit=Math.min(20000,historyLimit+2000);reload();}
    static Snapshot load(SQLiteDatabase db,RadioProfile p,String r){return load(db,p,r,2000);}
    static Snapshot load(SQLiteDatabase db,RadioProfile p,String r,int limit){String where="network=? AND radio=?";String[] args={p.name(),r};ArrayList<Node> ns=new ArrayList<>();ArrayList<Channel> cs=new ArrayList<>();ArrayList<Message> ms=new ArrayList<>();
        try(Cursor c=db.query("nodes",null,where,args,null,null,"heard DESC","4096")){while(c.moveToNext()){Node n=new Node();n.id=s(c,"id");n.name=s(c,"name");n.key=s(c,"key");n.heard=l(c,"heard");n.lat=c.getDouble(c.getColumnIndexOrThrow("lat"));n.lon=c.getDouble(c.getColumnIndexOrThrow("lon"));n.position=l(c,"position")==1;n.battery=(int)l(c,"battery");n.type=(int)l(c,"type");n.unmessagable=l(c,"blocked")==1;ns.add(n);}}
        try(Cursor c=db.query("channels",null,where,args,null,null,"slot ASC")){while(c.moveToNext()){Channel channel=new Channel();channel.slot=(int)l(c,"slot");channel.name=s(c,"name");channel.identity=s(c,"identity_hash");channel.enabled=l(c,"enabled")==1;cs.add(channel);}}
        try(Cursor c=db.query("messages",null,where,args,null,null,"time DESC,rowid DESC",Integer.toString(limit))){while(c.moveToNext()){Message m=new Message();m.uid=s(c,"uid");m.topic=s(c,"topic");m.title=s(c,"title");m.sender=s(c,"sender");m.body=s(c,"body");m.state=s(c,"state");m.timestamp=l(c,"time");m.outgoing=l(c,"outgoing")==1;m.read=l(c,"is_read")==1;m.peer=s(c,"peer");m.identity=s(c,"identity_hash");m.slot=(int)l(c,"slot");m.group=l(c,"is_group")==1;ms.add(m);}}
        Collections.reverse(ms);return new Snapshot(ns,cs,ms);
    }
    private static String s(Cursor c,String key){return c.getString(c.getColumnIndexOrThrow(key));}private static long l(Cursor c,String key){return c.getLong(c.getColumnIndexOrThrow(key));}
    @Override public void close(){closed=true;listener=null;main.removeCallbacksAndMessages(null);worker.post(()->{super.close();thread.quitSafely();});}
}
