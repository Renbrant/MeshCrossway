Warning: truncated output (original token count: 12687)
Total output lines: 456

package com.meshcrossway.android;

import android.app.*;
import android.content.*;
import android.database.sqlite.SQLiteDatabase;
import android.os.Bundle;
import android.view.*;
import android.widget.*;
import java.lang.reflect.Method;
import java.io.*;
import android.graphics.*;
import java.util.concurrent.*;
import static com.meshcrossway.android.MeshData.*;

/** Device/emulator tests with no radio and no third-party runner dependency. */
public final class SmokeInstrumentation extends Instrumentation {
    @Override public void onCreate(Bundle args){super.onCreate(args);start();}
    private static void check(boolean condition,String message){if(!condition)throw new AssertionError(message);}
    private static void await(CountDownLatch latch)throws Exception{check(latch.await(10,TimeUnit.SECONDS),"Async storage timed out");}
    private static Message message(String id,String body){Message m=new Message();m.uid=id;m.body=body;m.topic="c:0:hash";m.title="Test";m.timestamp=1700000000;m.group=true;return m;}
    private void barrier(HistoryStore store,int id)throws Exception{CountDownLatch done=new CountDownLatch(1);store.outgoing(RadioProfile.MESHCORE,"barrier",message("barrier"+id,"test"),done::countDown,()->{throw new AssertionError("Barrier failed");});await(done);}
    private void database()throws Exception{
        Context context=getTargetContext();String name="instrumentation-only.db";context.deleteDatabase(name);HistoryStore store=new HistoryStore(context,name);
        Message inbound=message("same-id","hello");inbound.rx.hops=6;inbound.rx.width=2;inbound.rx.kind="flood";inbound.rx.snr=1.25;store.message(RadioProfile.MESHTASTIC,"radio-a",inbound);store.message(RadioProfile.MESHTASTIC,"radio-a",inbound);store.message(RadioProfile.MESHCORE,"radio-a",message("same-id","other network"));store.message(RadioProfile.MESHTASTIC,"radio-b",message("same-id","other radio"));store.draft(RadioProfile.MESHTASTIC,"radio-a",inbound.topic,"draft 👋");barrier(store,1);
        SQLiteDatabase db=store.getReadableDatabase();check(HistoryStore.load(db,RadioProfile.MESHTASTIC,"radio-a").messages.size()==1,"Duplicate message inserted");check(HistoryStore.load(db,RadioProfile.MESHCORE,"radio-a").messages.get(0).body.equals("other network"),"Network isolation failed");check(HistoryStore.load(db,RadioProfile.MESHTASTIC,"radio-b").messages.get(0).body.equals("other radio"),"Radio isolation failed");
        check(HistoryStore.load(db,RadioProfile.MESHTASTIC,"radio-a").messages.get(0).rx.hops==6,"Initial receive metadata not saved");store.read(RadioProfile.MESHTASTIC,"radio-a",inbound.topic);Message enriched=message("same-id","hello");enriched.rx.hops=2;enriched.rx.width=2;enriched.rx.kind="flood";enriched.rx.path="1234abcd";enriched.rx.observed=true;enriched.rx.snr=1.25;store.message(RadioProfile.MESHTASTIC,"radio-a",enriched);store.message(RadioProfile.MESHTASTIC,"radio-a",inbound);Message out=message("out","outgoing");out.outgoing=true;out.state="queued";store.message(RadioProfile.MESHTASTIC,"radio-a",out);store.delivery(RadioProfile.MESHTASTIC,"radio-a",out.uid,"delivered");store.delivery(RadioProfile.MESHTASTIC,"radio-a",out.uid,"submitted");store.uncertain(RadioProfile.MESHTASTIC,"radio-a");barrier(store,2);
        Snapshot snapshot=HistoryStore.load(db,RadioProfile.MESHTASTIC,"radio-a");check(snapshot.unread()==0,"Replay reset read state");check(snapshot.messages.stream().anyMatch(m->m.uid.equals("same-id")&&m.rx.hops==2&&m.rx.snr==1.25&&m.rx.observed&&m.rx.path.equals("1234abcd")),"Receive metadata lost or downgraded");check(snapshot.messages.stream().anyMatch(m->m.uid.equals("out")&&m.state.equals("delivered")),"Late write downgraded delivery");store.close();
        HistoryStore reopened=new HistoryStore(context,name);check(HistoryStore.load(reopened.getReadableDatabase(),RadioProfile.MESHTASTIC,"radio-a").messages.size()==2,"History lost after reopen");CountDownLatch draft=new CountDownLatch(1);reopened.loadDraft(RadioProfile.MESHTASTIC,"radio-a",inbound.topic,value->{check(value.equals("draft 👋"),"Draft lost");draft.countDown();});await(draft);reopened.close();
    }
    private void backups()throws Exception{
        Context context=getTargetContext();String sourceName="backup-source.db",targetName="backup-target.db",rejectedName="backup-rejected.db";context.deleteDatabase(sourceName);context.deleteDatabase(targetName);context.deleteDatabase(rejectedName);
        HistoryStore source=new HistoryStore(context,sourceName);Node node=new Node();node.id="123";node.name="Saved node";node.position=true;node.lat=40;node.lon=-105;source.node(RadioProfile.MESHTASTIC,"radio-one",node);source.message(RadioProfile.MESHTASTIC,"radio-one",message("backup-message","Private test message"));source.flag(RadioProfile.MESHTASTIC,"radio-one","node","123","favorite",true);source.poi("Test POI",40,-105);source.draft(RadioProfile.MESHTASTIC,"radio-one","c:0:hash","Draft survives backup");barrier(source,10);
        ByteArrayOutputStream bytes=new ByteArrayOutputStream();CountDownLatch exported=new CountDownLatch(1);java.util.concurrent.atomic.AtomicReference<String> error=new java.util.concurrent.atomic.AtomicReference<>();source.exportBackup(bytes,result->{error.set(result);exported.countDown();});await(exported);check(error.get().isEmpty(),"Export failed");
        HistoryStore restored=new HistoryStore(context,targetName);CountDownLatch imported=new CountDownLatch(1);restored.importBackup(new ByteArrayInputStream(bytes.toByteArray()),result->{error.set(result);imported.countDown();});await(imported);check(error.get().isEmpty(),"Import failed: "+error.get());Snapshot snapshot=HistoryStore.load(restored.getReadableDatabase(),RadioProfile.MESHTASTIC,"radio-one");check(snapshot.nodes.size()==1&&snapshot.messages.size()==1&&snapshot.flags("node","123").favorite,"Backup lost history/favorite");try(android.database.Cursor c=restored.getReadableDatabase().rawQuery("SELECT count(*) FROM pois",null)){c.moveToFirst();check(c.getInt(0)==1,"POI backup missing");}
        org.json.JSONObject invalid=new org.json.JSONObject(new String(bytes.toByteArray(),java.nio.charset.StandardCharsets.UTF_8));invalid.getJSONObject("tables").getJSONArray("messages").getJSONObject(0).put("network","INVALID");HistoryStore rejected=new HistoryStore(context,rejectedName);CountDownLatch failed=new CountDownLatch(1);rejected.importBackup(new ByteArrayInputStream(invalid.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8)),result->{error.set(result);failed.countDown();});await(failed);check(!error.get().isEmpty(),"Invalid backup accepted");check(HistoryStore.load(rejected.getReadableDatabase(),RadioProfile.MESHTASTIC,"radio-one").nodes.isEmpty(),"Rejected import was not atomic");org.json.JSONObject legacyJson=new org.json.JSONObject(new String(bytes.toByteArray(),java.nio.charset.StandardCharsets.UTF_8));org.json.JSONArray oldMessages=legacyJson.getJSONObject("tables").getJSONArray("messages");for(int i=0;i<oldMessages.length();i++)oldMessages.getJSONObject(i).remove("rx_meta");String legacyBackupName="legacy-backup.db";context.deleteDatabase(legacyBackupName);HistoryStore legacyBackup=new HistoryStore(context,legacyBackupName);CountDownLatch legacyImported=new CountDownLatch(1);legacyBackup.importBackup(new ByteArrayInputStream(legacyJson.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8)),result->{error.set(result);legacyImported.countDown();});await(legacyImported);check(error.get().isEmpty()&&HistoryStore.load(legacyBackup.getReadableDatabase(),RadioProfile.MESHTASTIC,"radio-one").messages.get(0).rx.hops==-1,"Old backup did not import with unknown metadata");legacyBackup.close();source.close();restored.close();rejected.close();
        String legacyName="migration-test.db";context.deleteDatabase(legacyName);SQLiteDatabase old=context.openOrCreateDatabase(legacyName,0,null);old.execSQL("CREATE TABLE messages(uid TEXT PRIMARY KEY,body TEXT)");old.execSQL("INSERT INTO messages VALUES('retained','old data')");old.setVersion(1);old.close();HistoryStore upgraded=new HistoryStore(context,legacyName);SQLiteDatabase db=upgraded.getWritableDatabase();check(db.getVersion()==4,"Schema did not migrate");try(android.database.Cursor c=db.rawQuery("SELECT body FROM messages WHERE uid='retained'",null)){check(c.moveToFirst()&&c.getString(0).equals("old data"),"Migration erased history");}try(android.database.Cursor c=db.rawQuery("SELECT count(*) FROM flags",null)){check(c.moveToFirst(),"Migration missing local flags");}upgraded.close();
    }
    private void tracks()throws Exception{
        Context context=getTargetContext();String name="track-instrumentation.db";context.deleteDatabase(name);TrackStore store=new TrackStore(context,name);CountDownLatch started=new CountDownLatch(1);store.start("Field & trail",ok->{check(ok,"Track did not start");started.countDown();});await(started);CountDownLatch loaded=new CountDownLatch(1);store.barrier(loaded::countDown);await(loaded);String id=store.tracks.get(0).id;
        long now=System.currentTimeMillis();store.add(new FieldTools.Fix(40,-105,4,Double.NaN,now,0),now);store.add(new FieldTools.Fix(40.0001,-105,4,123,now+3000,0),now+3000);store.add(new FieldTools.Fix(41,-105,4,123,now+6000,0),now+6000);store.pause(false);CountDownLatch resumed=new CountDownLatch(1);store.resume(id,ok->{check(ok,"Resume failed");resumed.countDown();});await(resumed);store.add(new FieldTools.Fix(41,-104,4,124,now+9000,0),now+9000);store.pause(true);CountDownLatch saved=new CountDownLatch(1);store.barrier(saved::countDown);await(saved);TrackStore.Track track=store.tracks.get(0);check(track.count==3&&track.state.equals("saved"),"Track points/status not persisted");check(track.distance>10&&track.distance<12,"Pause gap or GPS jump added to distance");
        ByteArrayOutputStream bytes=new ByteArrayOutputStream();CountDownLatch exported=new CountDownLatch(1);store.export(id,bytes,error->{check(error.isEmpty(),"GPX export failed");exported.countDown();});await(exported);String gpx=bytes.toString("UTF-8");check(gpx.contains("Field &amp; trail")&&gpx.split("<trkseg>",-1).length==3,"Export lost name or segment break");
        String gpxTestName="track-gpx-imported.db";context.deleteDatabase(gpxTestName);
        TrackStore gpxRestored=new TrackStore(context,gpxTestName);
        CountDownLatch gpxImport=new CountDownLatch(1);
        java.util.concurrent.atomic.AtomicReference<String> gpxError=new java.util.concurrent.atomic.AtomicReference<>("");
        gpxRestored.importGpx(new ByteArrayInputStream(bytes.toByteArray()),result->{
            gpxError.set(result);gpxImport.countDown();
        });
        await(gpxImport);
        check(gpxError.get().isEmpty()&&gpxRestored.tracks.size()==1,
            "Legacy GPX track could not be imported");
        check(gpxRestored.tracks.get(0).count==3&&
            gpxRestored.tracks.get(0).name.equals("Field & trail"),
            "GPX import lost recorded points or escaped track name");
        gpxRestored.close();
        store.createFolder("Hiking");
        CountDownLatch folderAdded=new CountDownLatch(1);store.barrier(folderAdded::countDown);await(folderAdded);
        check(store.folders.size()==1&&store.folders.get(0).name.equals("Hiking"),"Track folder did not persist");
        String folderId=store.folders.get(0).id;
        store.moveTrack(id,folderId);
        CountDownLatch moved=new CountDownLatch(1);store.barrier(moved::countDown);await(moved);
        check(store.tracks.get(0).folderId.equals(folderId),"Track folder assignment did not persist");
        store.metadata(id,"Renamed",TrackStore.COLORS[1],false);CountDownLatch updated=new CountDownLatch(1);store.barrier(updated::countDown);await(updated);check(!store.tracks.get(0).visible&&store.tracks.get(0).name.equals("Renamed"),"Track metadata not updated");
        ByteArrayOutputStream fullTrackBackup=new ByteArrayOutputStream();
        CountDownLatch fullExport=new CountDownLatch(1);
        java.util.concurrent.atomic.AtomicReference<String> archiveStatus=new java.util.concurrent.atomic.AtomicReference<>("");
        store.exportArchive(fullTrackBackup,status->{archiveStatus.set(status);fullExport.countDown();});
        await(fullExport);
        check(archiveStatus.get().isEmpty(),"Complete track archive could not export");
        String backupJSON=fullTrackBackup.toString("UTF-8");
        check(backupJSON.contains("MeshCrosswayTracks")&&backupJSON.contains("Hiking")&&
            backupJSON.contains("Renamed"),"Full backup lost folder/track metadata");
        String migrateDb="track-migration-destination.db";
        context.deleteDatabase(migrateDb);
        TrackStore imported=new TrackStore(context,migrateDb);
        CountDownLatch fullImport=new CountDownLatch(1);
        imported.importArchive(new ByteArrayInputStream(fullTrackBackup.toByteArray()),status->{
            archiveStatus.set(status);fullImport.countDown();
        });
        await(fullImport);
        check(archiveStatus.get().isEmpty(),"Complete track archive import failed");
        CountDownLatch migrated=new CountDownLatch(1);imported.barrier(migrated::countDown);await(migrated);
        check(imported.tracks.size()==1&&imported.folders.size()==1,"Full archive lost track/folder");
        TrackStore.Track migratedTrack=imported.tracks.get(0);
        check(migratedTrack.id.equals(id)&&migratedTrack.count==3&&
            migratedTrack.folderId.equals(folderId)&&migratedTrack.name.equals("Renamed")&&
            !migratedTrack.visible,"Complete track archive lost recording or metadata");
        try(android.database.Cursor points=imported.getReadableDatabase().rawQuery(
            "SELECT count(*) FROM fixes WHERE track=?",new String[]{id})){
            check(points.moveToFirst()&&points.getInt(0)==3,
                "Complete track archive did not restore ALL GPS points");
        }
        String corrupted=backupJSON.replace("MeshCrosswayTracks","InvalidTrackArchive");
        CountDownLatch rejectedBackup=new CountDownLatch(1);
        imported.importArchive(new ByteArrayInputStream(corrupted.getBytes("UTF-8")),status->{
            archiveStatus.set(status);rejectedBackup.countDown();
        });
        await(rejectedBackup);
        check(!archiveStatus.get().isEmpty()&&imported.tracks.size()==1,
            "Malformed track archive should be rejected without deleting existing records");
        imported.close();
        store.close();
        TrackStore reopened=new TrackStore(context,name);CountDownLatch ready=new CountDownLatch(1);reopened.barrier(ready::countDown);await(ready);check(reopened.tracks.size()==1&&reopened.tracks.get(0).count==3,"Track lost after reopen");check(reopened.tracks.get(0).folderId.equals(folderId)&&reopened.folders.size()==1,"Track folders lost on reopen");reopened.deleteFolder(folderId);CountDownLatch unfoldered=new CountDownLatch(1);reopened.barrier(unfoldered::countDown);await(unfoldered);check(reopened.folders.isEmpty()&&reopened.tracks.get(0).folderId.isEmpty(),"Deleting folder removed track or left an orphan");reopened.delete(id);CountDownLatch deleted=new CountDownLatch(1);reopened.barrier(deleted::countDown);await(deleted);check(reopened.tracks.isEmpty(),"Track delete failed");CountDownLatch interrupted=new CountDownLatch(1);reopened.start("Interrupted",ok->{check(ok,"Interrupted track start failed");interrupted.countDown();});await(interrupted);CountDownLatch committed=new CountDownLatch(1);reopened.barrier(committed::countDown);await(committed);reopened.close();TrackStore recovered=new TrackStore(context,name);CountDownLatch recovery=new CountDownLatch(1);recovered.barrier(recovery::countDown);await(recovery);check(recovered.current()!=null&&recovered.current().state.equals("paused"),"Interrupted recording did not recover as paused");recovered.close();
    }
    private void waitUntil(java.util.function.BooleanSupplier condition,String message)throws Exception{long deadline=android.os.SystemClock.elapsedRealtime()+20000;java.util.concurrent.atomic.AtomicBoolean ready=new java.util.concurrent.atomic.AtomicBoolean();do{onUi(()->ready.set(condition.getAsBoolean()));if(ready.get())return;Thread.sleep(50);}while(android.os.SystemClock.elapsedRealtime()<deadline);throw new AssertionError(message);}
    private void maps(MainActivity activity)throws Exception{
        java.lang.reflect.Field field=MainActivity.class.getDeclaredField("service");field.setAccessible(true);waitUntil(()->{try{return field.get(activity)!=null;}catch(Exception e){throw new RuntimeException(e);}},"Service did not bind");
        onUi(()->{try{RadioService service=(RadioService)field.get(activity);Node mt=new Node();mt.id="123";mt.name="MT test";mt.position=true;mt.lat=40;mt.lon=-105;mt.heard=System.currentTimeMillis()/1000;Node mc=new Node();mc.id="0909090909090909090909090909090909090909090909090909090909090909";mc.name="MC test";mc.position=true;mc.lat=40.001;mc.lon=-105.001;mc.heard=mt.heard;service.sessions.get(RadioProfile.MESHTASTIC).address="map-mt";service.sessions.get(RadioProfile.MESHCORE).address="map-mc";service.history.select(RadioProfile.MESHTASTIC,"map-mt");service.history.select(RadioProfile.MESHCORE,"map-mc");service.history.node(RadioProfile.MESHTASTIC,"map-mt",mt);service.history.node(RadioProfile.MESHCORE,"map-mc",mc);activity.showMap(40,-105);}catch(Exception e){throw new RuntimeException(e);}});
        waitUntil(()->activity.mapPanel!=null&&activity.mapPanel.styleReady&&activity.mapPanel.markerCount==2,"Native map did not load both network sources");waitUntil(()->{MapPanel panel=activity.mapPanel;return panel.map.queryRenderedFeatures(new android.graphics.RectF(0,0,panel.mapView.getWidth(),panel.mapView.getHeight()),"mesh-points").size()==2;},"Native renderer did not draw both network markers");screenshot(activity,"dual-map");
        final OfflineMaps[] offline=new OfflineMaps[1];onUi(()->{offline[0]=activity.mapPanel.offline;offline[0].create("Offline te…3687 tokens truncated…;
            try(InputStream in=homepage.getInputStream()){
                byte[] buffer=new byte[1024];int n;
                while((n=in.read(buffer))!=-1)html.write(buffer,0,n);
            }
            String page=html.toString("UTF-8");
            check(page.contains("data-message-view="+(char)34+"chats"+(char)34) &&
                  page.contains("data-message-view="+(char)34+"channels"+(char)34) &&
                  page.contains("data-message-view="+(char)34+"nodes"+(char)34),
                "Remote Home is missing separate Chats, Channels and Nodes");
            check(page.contains("id="+(char)34+"map-viewport"+(char)34) &&
                  page.contains("id="+(char)34+"map-draw"+(char)34),
                "Remote Home is missing visual offline map selection");
            check(homepage.getHeaderField("Content-Security-Policy").contains("tile.openstreetmap.org"),
                "Map preview tile host is not restricted by CSP");
            check(homepage.getHeaderField("Referrer-Policy").contains("strict-origin"),
                "Interactive OSM map does not send provider-required Referrer");
            homepage.disconnect();
            javax.net.ssl.HttpsURLConnection mapSource=connect.apply("/remote-map.js");
            check(mapSource.getResponseCode()==200,"Bundled map selector JS is inaccessible");
            mapSource.disconnect();
            javax.net.ssl.HttpsURLConnection denied=connect.apply("/api/state");
            check(denied.getResponseCode()==401,"API allowed unauthenticated data");
            denied.disconnect();
            javax.net.ssl.HttpsURLConnection login=connect.apply("/api/login");
            login.setRequestMethod("POST");login.setDoOutput(true);
            login.setRequestProperty("Content-Type","application/json");
            byte[] payload=new org.json.JSONObject().put("code",workspace.accessPin()).toString()
                .getBytes(java.nio.charset.StandardCharsets.UTF_8);
            login.setFixedLengthStreamingMode(payload.length);
            try(OutputStream out=login.getOutputStream()){out.write(payload);}
            check(login.getResponseCode()==200,"Local HTTPS login failed");
            String cookie=login.getHeaderField("Set-Cookie");
            check(cookie!=null&&cookie.contains("HttpOnly")&&cookie.contains("SameSite=Strict"),
                "Login cookie is not protected");
            login.disconnect();
            javax.net.ssl.HttpsURLConnection authenticated=connect.apply("/api/state");
            authenticated.setRequestProperty("Cookie",cookie.split(";",2)[0]);
            check(authenticated.getResponseCode()==200,"Authorized HTTPS state denied");
            ByteArrayOutputStream response=new ByteArrayOutputStream();
            try(InputStream in=authenticated.getInputStream()){
                byte[] buffer=new byte[512];int n;
                while((n=in.read(buffer))!=-1)response.write(buffer,0,n);
            }
            String data=response.toString("UTF-8");
            check(data.contains("Meshtastic")&&data.contains("MeshCore"),
                "HTTPS dashboard failed to keep both radios separate");
            org.json.JSONObject parsed=new org.json.JSONObject(data);
            org.json.JSONArray radioStates=parsed.getJSONArray("radios");
            check(radioStates.length()==2,"Both network state entries are required");
            for(int i=0;i<radioStates.length();i++){
                org.json.JSONObject item=radioStates.getJSONObject(i);
                check(item.has("contacts")&&item.has("channels")&&item.has("conversations"),
                    "Remote state must expose nodes, channels and chats for both meshes");
            }
            authenticated.disconnect();
            javax.net.ssl.HttpsURLConnection csrf=connect.apply("/api/logout");
            csrf.setRequestMethod("POST");
            csrf.setRequestProperty("Cookie",cookie.split(";",2)[0]);
            check(csrf.getResponseCode()==403,"Mutating HTTP endpoint accepted request without CSRF header");
            csrf.disconnect();
            workspace.changePin("0123");
            check(workspace.accessPin().equals("0123"),"Changed PIN not applied");
            javax.net.ssl.HttpsURLConnection invalidated=connect.apply("/api/state");
            invalidated.setRequestProperty("Cookie",cookie.split(";",2)[0]);
            check(invalidated.getResponseCode()==401,"Changing PIN did not revoke prior browser sessions");
            invalidated.disconnect();
            boolean rejected=false;
            try{workspace.changePin("ABC");}catch(IllegalArgumentException invalid){rejected=true;}
            check(rejected&&workspace.accessPin().equals("0123"),"Invalid changed PIN replaced active PIN");
            javax.net.ssl.HttpsURLConnection newLogin=connect.apply("/api/login");
            newLogin.setRequestMethod("POST");newLogin.setDoOutput(true);
            newLogin.setRequestProperty("Content-Type","application/json");
            byte[] newPayload=new org.json.JSONObject().put("code","0123").toString()
                .getBytes(java.nio.charset.StandardCharsets.UTF_8);
            newLogin.setFixedLengthStreamingMode(newPayload.length);
            try(OutputStream out=newLogin.getOutputStream()){out.write(newPayload);}
            check(newLogin.getResponseCode()==200,"Changed PIN cannot log in");
            newLogin.disconnect();
        }catch(Throwable failure){
            throw new AssertionError("HTTPS diagnostics ("+workspace.tlsDiagnostics()+")",failure);
        }finally{workspace.stop();}
    }
    private void completeBackup(MainActivity activity)throws Exception{
        java.lang.reflect.Field radioField=MainActivity.class.getDeclaredField("service");
        java.lang.reflect.Field trackField=MainActivity.class.getDeclaredField("trackService");
        radioField.setAccessible(true);trackField.setAccessible(true);
        waitUntil(()->{
            try{return radioField.get(activity)!=null&&trackField.get(activity)!=null;}
            catch(Exception e){return false;}
        },"Backup stores did not bind");
        RadioService radios=(RadioService)radioField.get(activity);
        TrackService tracks=(TrackService)trackField.get(activity);
        File zip=new File(getTargetContext().getCacheDir(),"migration-instrumentation.zip");
        if(zip.exists())zip.delete();
        java.util.concurrent.atomic.AtomicReference<String> error=new java.util.concurrent.atomic.AtomicReference<>("");
        CountDownLatch exported=new CountDownLatch(1);
        FullBackup.export(getTargetContext(),radios.history,tracks.store,android.net.Uri.fromFile(zip),
            result->{error.set(result);exported.countDown();});
        check(exported.await(40,TimeUnit.SECONDS),"Complete backup export timed out");
        check(error.get().isEmpty()&&zip.length()>300,"Complete backup ZIP could not be saved: "+error.get());
        try(java.util.zip.ZipFile archive=new java.util.zip.ZipFile(zip)){
            for(String name:new String[]{"manifest.json","history.json","tracks.json","settings.json"})
                check(archive.getEntry(name)!=null,"Complete backup missing "+name);
        }
        CountDownLatch imported=new CountDownLatch(1);
        FullBackup.restore(getTargetContext(),radios.history,tracks.store,android.net.Uri.fromFile(zip),
            result->{error.set(result);imported.countDown();});
        check(imported.await(40,TimeUnit.SECONDS),"Complete backup restore timed out");
        check(error.get().isEmpty(),"Complete backup restore failed: "+error.get());
        zip.delete();
    }
    private void appearance()throws Exception{
        Context context=getTargetContext();
        android.content.SharedPreferences ui=context.getSharedPreferences("ui",Context.MODE_PRIVATE);
        String[] keys={"appearance.mode","appearance.gradient","appearance.hue",
            "appearance.dim","appearance.blur"};
        java.util.HashMap<String,Object> original=new java.util.HashMap<>();
        for(String key:keys)if(ui.contains(key))original.put(key,ui.getAll().get(key));
        File image=Appearance.photoFile(context);
        File temporary=new File(context.getFilesDir(),"appearance-test-backup.jpg");
        if(image.exists()){
            try(java.io.InputStream in=new FileInputStream(image);
                java.io.OutputStream out=new FileOutputStream(temporary)){
                byte[] block=new byte[4096];int n;
                while((n=in.read(block))!=-1)out.write(block,0,n);
            }
        }
        try{
            check(Appearance.validMode(Appearance.SYSTEM)&&
                Appearance.validMode(Appearance.COLOR)&&
                Appearance.validMode(Appearance.GRADIENT)&&
                Appearance.validMode(Appearance.PHOTO)&&
                !Appearance.validMode("other"),"Theme mode whitelist failed");
            Appearance.setMode(context,Appearance.COLOR);
            Appearance.setHue(context,202);
            Appearance color=Appearance.load(context);
            check(color.mode.equals(Appearance.COLOR)&&color.hue==202,
                "Saved solid theme did not survive reload");
            check(color.colors.bg!=Appearance.palette(context,Appearance.SYSTEM,202).bg,
                "Solid theme should change app background");
            Appearance.setMode(context,Appearance.GRADIENT);
            Appearance.setGradient(context,Appearance.MESH);
            Appearance mesh=Appearance.load(context);
            check(mesh.mode.equals(Appearance.GRADIENT)&&mesh.gradientType.equals(Appearance.MESH),
                "Saved mesh gradient did not survive reload");
            check(mesh.background()!=null&&
                Appearance.gradientDrawable(200,Appearance.LINEAR)!=null,
                "Both gradient modes must render without external resources");
            Appearance.setDim(context,70);
            Appearance.setBlur(context,13);
            check(Appearance.load(context).dim==70&&Appearance.load(context).blur==13,
                "Photo dim/blur controls were not persisted");
            Appearance.setMode(context,Appearance.PHOTO);
            android.graphics.Bitmap bitmap=android.graphics.Bitmap.createBitmap(
                160,120,android.graphics.Bitmap.Config.ARGB_8888);
            bitmap.eraseColor(0xff224466);
            try(FileOutputStream out=new FileOutputStream(image)){
                check(bitmap.compress(android.graphics.Bitmap.CompressFormat.JPEG,83,out),
                    "Could not create private test photo");
            }
            bitmap.recycle();
            Appearance photo=Appearance.load(context);
            check(photo.hasPhoto&&photo.loadPhoto(context)!=null,
                "Stored private appearance photo could not be loaded");
            check(photo.colors.text!=photo.colors.bg,
                "Photo overlay must keep text visually distinct");
            boolean rejected=false;
            try{Appearance.setMode(context,"invalid");}catch(IllegalArgumentException e){rejected=true;}
            check(rejected,"Unrecognized theme mode should be rejected");
            rejected=false;
            try{Appearance.setHue(context,400);}catch(IllegalArgumentException e){rejected=true;}
            check(rejected,"Invalid hue accepted");
        }finally{
            android.content.SharedPreferences.Editor reset=ui.edit();
            for(String key:keys){
                Object value=original.get(key);
                if(value==null)reset.remove(key);
                else if(value instanceof String)reset.putString(key,(String)value);
                else if(value instanceof Integer)reset.putInt(key,(Integer)value);
            }
            reset.commit();
            if(temporary.exists()){
                image.delete();
                temporary.renameTo(image);
            }else image.delete();
        }
    }
    private void bleSignal()throws Exception{
        check(BleSignal.bars(BleSignal.UNKNOWN)==0&&
            BleSignal.bars(-50)==4&&BleSignal.bars(-64)==3&&
            BleSignal.bars(-74)==2&&BleSignal.bars(-85)==1&&
            BleSignal.bars(-105)==0,"BLE RSSI four-bar mapping is wrong");
        check(BleSignal.label(BleSignal.UNKNOWN,false).equals("BLE signal unavailable"),
            "Unknown RSSI must not be misrepresented as signal strength");
        check(BleSignal.label(-71,true).startsWith("BLE -71 dBm")&&
            BleSignal.label(-71,false).startsWith("Last BLE -71 dBm"),
            "Current and previous BLE readings must be identified separately");
        check(BleSignal.recent(System.currentTimeMillis()-1000,System.currentTimeMillis())&&
            !BleSignal.recent(System.currentTimeMillis()-25L*60*60*1000,
                System.currentTimeMillis()),"Old RSSI should not be treated as current");
    }
    private void activity()throws Exception{
        MainActivity.mapStyleOverride="asset://map-test-style.json";Intent intent=new Intent(getTargetContext(),MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);MainActivity activity=(MainActivity)startActivitySync(intent);waitForIdleSync();
        onUi(()->{
            View screen=activity.getWindow().getDecorView();
            check(find(screen,"Dual mesh dashboard")==null,
                "Home should not repeat the radio status in a full-width summary");
            check(find(screen,"RADIO CONNECTIONS")==null&&
                find(screen,"NODES")==null&&find(screen,"FIELD TOOLS")==null,
                "Home should not waste vertical space on duplicate section headings");
            check(find(screen,"MESHTASTIC")!=null&&find(screen,"MESHCORE")!=null,
                "Both radio status cards must appear at the top");
            check(find(screen,"Remote Access")!=null,
                "Remote Access must remain available after the layout change");
            check(find(screen,"MT · NODES")!=null,"Meshtastic node card is missing");
            check(find(screen,"MC · NODES")!=null,"MeshCore node card is missing");
            check(find(screen,"MT · CHATS")!=null,"Meshtastic chat card is missing");
            check(find(screen,"MC · CHATS")!=null,"MeshCore chat card is missing");
            check(Appearance.validMode(Appearance.load(activity).mode), "Appearance preference not loaded");
        });
        screenshot(activity,"home");remoteSecurity(activity);completeBackup(activity);
        Method openAppearance=MainActivity.class.getDeclaredMethod("openSettings",String.class,RadioProfile.class);
        openAppearance.setAccessible(true);
        onUi(()->{
            try{openAppearance.invoke(activity,"appearance",null);}
            catch(Exception e){throw new RuntimeException(e);}
            View view=activity.getWindow().getDecorView();
            check((find(view,"System")!=null||find(view,"✓  System")!=null)&&
                (find(view,"Color")!=null||find(view,"✓  Color")!=null)&&
                (find(view,"Gradient")!=null||find(view,"✓  Gradient")!=null)&&
                (find(view,"Photo")!=null||find(view,"✓  Photo")!=null),
                "Appearance settings must expose all four selectable theme choices");
        });
        screenshot(activity,"settings-appearance");
        onUi(activity::onBackPressed);
        for(String page:new String[]{"Contacts","Chats","Channels","Map","Settings","Home"})click(activity,page);menus(activity);recording(activity);maps(activity);mapPanels(activity);click(activity,"Home");receptionDetails(activity);
        Method open=MainActivity.class.getDeclaredMethod("openChat",RadioProfile.class,String.class,Target.class);open.setAccessible(true);onUi(()->{try{open.invoke(activity,RadioProfile.MESHTASTIC,"offline-test",new Target("c:0:test","Test channel","","test",0,true));EditText entry=composer(activity.getWindow().getDecorView());check(entry!=null,"Composer missing");entry.setText("Draft survives rotation 👋");}catch(Exception e){throw new RuntimeException(e);}});
        waitForIdleSync();screenshot(activity,"chat-draft");Method settings=MainActivity.class.getDeclaredMethod("openSettings",String.class,RadioProfile.class);settings.setAccessible(true);onUi(()->{try{settings.invoke(activity,"data",null);}catch(Exception e){throw new RuntimeException(e);}});screenshot(activity,"settings-data");
        ActivityMonitor monitor=addMonitor(MainActivity.class.getName(),null,false);onUi(activity::recreate);MainActivity recreated=(MainActivity)waitForMonitorWithTimeout(monitor,10000);check(recreated!=null,"Activity recreation timed out");waitForIdleSync();onUi(recreated::onBackPressed);waitForIdleSync();java.util.concurrent.atomic.AtomicReference<String> restored=new java.util.concurrent.atomic.AtomicReference<>("");long deadline=android.os.SystemClock.elapsedRealtime()+10000;do{onUi(()->{EditText entry=composer(recreated.getWindow().getDecorView());restored.set(entry==null?"<no composer>":entry.getText().toString());});if(restored.get().equals("Draft survives rotation 👋"))break;Thread.sleep(50);}while(android.os.SystemClock.elapsedRealtime()<deadline);screenshot(recreated,"chat-restored");check(restored.get().equals("Draft survives rotation 👋"),"Rotation lost composer draft: "+restored.get());onUi(recreated::finish);removeMonitor(monitor);
    }
    @Override public void onStart(){Bundle status=new Bundle();status.putString("id","MeshCrossway");status.putString("class",getClass().getName());status.putString("test","databaseAndNavigation");status.putInt("numtests",1);status.putInt("current",1);sendStatus(1,status);Bundle result=new Bundle();try{database();backups();tracks();appearance();bleSignal();activity();sendStatus(0,status);result.putString("stream","\nOK (1 test)\n");finish(Activity.RESULT_OK,result);}catch(Throwable error){status.putString("stack",android.util.Log.getStackTraceString(error));sendStatus(-2,status);result.putString("stream",status.getString("stack"));finish(Activity.RESULT_CANCELED,result);}}
}

