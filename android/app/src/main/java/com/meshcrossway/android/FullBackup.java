package com.meshcrossway.android;

import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import org.json.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.Consumer;
import java.util.zip.*;

/**
 * Explicit, portable migration archive. Users choose a document outside the
 * private app sandbox, allowing a one-time transition between signing keys.
 * The archive contains private message history, ALL GPS points/folders and
 * named preferences; never includes AndroidKeyStore keys, access PINs, or
 * Android OS permission grants. Offline MapLibre tiles are not copied.
 *
 * ZIP entries are a fixed allowlist; caps defend against malformed archives
 * and decompression bombs. Imports merge instead of deleting existing rows.
 */
final class FullBackup {
    private static final String FORMAT="MeshCrosswayDeviceBackup";
    private static final String MANIFEST="manifest.json",SETTINGS="settings.json";
    private static final String HISTORY="history.json",TRACKS="tracks.json";
    private static final int MAX_SETTINGS=131_072,MAX_HISTORY=40*1024*1024;
    private static final int MAX_TRACKS=160*1024*1024;
    private static final long MAX_TOTAL=MAX_SETTINGS+MAX_HISTORY+(long)MAX_TRACKS+16_384;
    private static final String[] PREFERENCES={"radios","ui","map"};
    private static final Handler MAIN=new Handler(Looper.getMainLooper());

    private FullBackup(){}
    static void export(Context context,HistoryStore history,TrackStore tracks,Uri destination,Consumer<String> done){
        new Thread(()->{
            String result="";
            try(OutputStream output=context.getContentResolver().openOutputStream(destination,"w")){
                if(output==null)throw new IOException("Document could not be opened");
                try(ZipOutputStream archive=new ZipOutputStream(new BufferedOutputStream(output))){
                    archive.setLevel(6);
                    archive.putNextEntry(new ZipEntry(MANIFEST));
                    byte[] metadata=new JSONObject().put("format",FORMAT).put("version",1)
                        .put("exportedAt",System.currentTimeMillis())
                        .put("warning","Private location and conversations; unencrypted archive")
                        .toString().getBytes(StandardCharsets.UTF_8);
                    archive.write(metadata);archive.closeEntry();

                    archive.putNextEntry(new ZipEntry(SETTINGS));
                    archive.write(exportSettings(context).toString().getBytes(StandardCharsets.UTF_8));
                    archive.closeEntry();

                    archive.putNextEntry(new ZipEntry(HISTORY));
                    CompletableFuture<String> messages=new CompletableFuture<>();
                    history.exportBackup(new FilterOutputStream(archive){
                        @Override public void close()throws IOException{flush();}
                    },messages::complete);
                    waitDone(messages,"History backup");
                    archive.closeEntry();

                    archive.putNextEntry(new ZipEntry(TRACKS));
                    CompletableFuture<String> gps=new CompletableFuture<>();
                    tracks.exportArchive(archive,gps::complete);
                    waitDone(gps,"Track backup");
                    archive.closeEntry();
                    archive.finish();
                }
            }catch(Exception e){result="Backup failed. Keep the installed app and choose another file.";}
            String status=result;
            MAIN.post(()->done.accept(status));
        },"meshcrossway-full-backup-export").start();
    }
    static void restore(Context context,HistoryStore history,TrackStore tracks,Uri source,Consumer<String> done){
        new Thread(()->{
            String result="";
            File folder=new File(context.getCacheDir(),"migration-"+UUID.randomUUID());
            try{
                if(!folder.mkdirs())throw new IOException("Cannot stage backup");
                HashMap<String,File> entries=new HashMap<>();
                try(InputStream stream=context.getContentResolver().openInputStream(source)){
                    if(stream==null)throw new IOException("Backup document unavailable");
                    try(ZipInputStream archive=new ZipInputStream(new BufferedInputStream(stream))){
                        ZipEntry entry;long total=0;
                        while((entry=archive.getNextEntry())!=null){
                            String name=entry.getName();
                            int limit=name.equals(MANIFEST)?16_384:
                                name.equals(SETTINGS)?MAX_SETTINGS:
                                name.equals(HISTORY)?MAX_HISTORY:
                                name.equals(TRACKS)?MAX_TRACKS:0;
                            if(limit==0||entry.isDirectory()||entries.containsKey(name))
                                throw new IOException("Unexpected or duplicate archive entry");
                            File file=new File(folder,name);
                            try(OutputStream output=new BufferedOutputStream(new FileOutputStream(file))){
                                byte[] buffer=new byte[8192];int n;long bytes=0;
                                while((n=archive.read(buffer))!=-1){
                                    bytes+=n;total+=n;
                                    if(bytes>limit||total>MAX_TOTAL)
                                        throw new IOException("Backup exceeds size limit");
                                    output.write(buffer,0,n);
                                }
                            }
                            entries.put(name,file);
                            archive.closeEntry();
                        }
                    }
                }
                if(entries.size()!=4||!entries.containsKey(MANIFEST)||
                   !entries.containsKey(SETTINGS)||!entries.containsKey(HISTORY)||
                   !entries.containsKey(TRACKS))throw new IOException("Incomplete archive");
                JSONObject metadata=new JSONObject(readSmall(entries.get(MANIFEST),16_384));
                if(!FORMAT.equals(metadata.optString("format"))||metadata.optInt("version")!=1)
                    throw new IOException("Backup format mismatch");
                // Reject invalid preference files BEFORE importing the history.
                JSONObject settings=new JSONObject(readSmall(entries.get(SETTINGS),MAX_SETTINGS));
                validateSettings(settings);

                CompletableFuture<String> messages=new CompletableFuture<>();
                history.importBackup(new FileInputStream(entries.get(HISTORY)),messages::complete);
                waitDone(messages,"History restore");
                CompletableFuture<String> gps=new CompletableFuture<>();
                tracks.importArchive(new FileInputStream(entries.get(TRACKS)),gps::complete);
                waitDone(gps,"Track restore");
                restoreSettings(context,settings);
            }catch(Exception e){
                result="Backup import failed or was incomplete. Existing app data was not deliberately erased. "+ 
                    "Check the archive and try again.";
            }finally{
                File[] files=folder.listFiles();
                if(files!=null)for(File file:files)file.delete();
                folder.delete();
            }
            String status=result;
            MAIN.post(()->done.accept(status));
        },"meshcrossway-full-backup-import").start();
    }
    private static void waitDone(CompletableFuture<String> operation,String label)throws Exception{
        String status=operation.get(180,TimeUnit.SECONDS);
        if(status==null||!status.isEmpty())
            throw new IOException(label+": "+status);
    }
    private static String readSmall(File file,int max)throws Exception{
        if(file.length()>max)throw new IOException("Settings or manifest too large");
        try(InputStream input=new FileInputStream(file)){
            ByteArrayOutputStream out=new ByteArrayOutputStream();
            byte[] buffer=new byte[4096];int n;
            while((n=input.read(buffer))!=-1){
                if(out.size()+n>max)throw new IOException("File too large");
                out.write(buffer,0,n);
            }
            return out.toString("UTF-8");
        }
    }
    private static JSONObject exportSettings(Context context)throws Exception{
        JSONObject result=new JSONObject().put("format","MeshCrosswaySettings").put("version",1);
        JSONObject files=new JSONObject();
        for(String file:PREFERENCES){
            JSONObject values=new JSONObject();
            for(Map.Entry<String,?> entry:context.getSharedPreferences(file,Context.MODE_PRIVATE).getAll().entrySet()){
                Object v=entry.getValue();if(v==null)continue;
                String type=v instanceof Boolean?"boolean":v instanceof Integer?"integer":
                    v instanceof Long?"long":v instanceof Float?"float":
                    v instanceof String?"string":"";
                if(type.isEmpty())continue;
                values.put(entry.getKey(),new JSONObject().put("type",type).put("value",v));
            }
            files.put(file,values);
        }
        return result.put("files",files);
    }
    private static void validateSettings(JSONObject settings)throws Exception{
        if(!"MeshCrosswaySettings".equals(settings.optString("format"))||
           settings.optInt("version")!=1)throw new IOException("Unsupported settings");
        JSONObject files=settings.getJSONObject("files");
        for(String file:PREFERENCES){
            JSONObject values=files.optJSONObject(file);
            if(values==null)continue;
            Iterator<String> iterator=values.keys();int count=0;
            while(iterator.hasNext()){
                if(++count>100)throw new IOException("Too many setting keys");
                String name=iterator.next();
                if(name.length()>100||name.isEmpty())throw new IOException("Invalid setting key");
                JSONObject value=values.getJSONObject(name);
                String type=value.getString("type");
                if(!Arrays.asList("boolean","integer","long","float","string").contains(type))
                    throw new IOException("Invalid setting type");
                String asText=value.get("value").toString();
                if(asText.length()>16_384)throw new IOException("Setting is too long");
                if(type.equals("integer"))value.getInt("value");
                if(type.equals("long"))value.getLong("value");
                if(type.equals("float")){
                    double number=value.getDouble("value");
                    if(!Double.isFinite(number))throw new IOException("Invalid setting number");
                }
                if(type.equals("boolean")){
                    Object raw=value.get("value");
                    if(!(raw instanceof Boolean))throw new IOException("Invalid setting boolean");
                }
            }
        }
    }
    private static void restoreSettings(Context context,JSONObject settings)throws Exception{
        JSONObject files=settings.getJSONObject("files");
        for(String file:PREFERENCES){
            JSONObject values=files.optJSONObject(file);
            if(values==null)continue;
            SharedPreferences preferences=context.getSharedPreferences(file,Context.MODE_PRIVATE);
            SharedPreferences.Editor editor=preferences.edit();
            Iterator<String> keys=values.keys();
            while(keys.hasNext()){
                String name=keys.next();
                // Respect settings the user already configured on this installation.
                if(preferences.contains(name))continue;
                JSONObject record=values.getJSONObject(name);
                String type=record.getString("type");
                if(file.equals("radios")&&(name.equals(RadioProfile.MESHTASTIC.name())||
                        name.equals(RadioProfile.MESHCORE.name()))){
                    String address=record.optString("value","");
                    if(!android.bluetooth.BluetoothAdapter.checkBluetoothAddress(address))continue;
                }
                switch(type){
                    case "string":editor.putString(name,record.getString("value"));break;
                    case "boolean":editor.putBoolean(name,record.getBoolean("value"));break;
                    case "integer":editor.putInt(name,record.getInt("value"));break;
                    case "long":editor.putLong(name,record.getLong("value"));break;
                    case "float":editor.putFloat(name,(float)record.getDouble("value"));break;
                    default:throw new IOException("Unknown setting");
                }
            }
            if(!editor.commit())throw new IOException("Could not persist settings");
        }
    }
}
