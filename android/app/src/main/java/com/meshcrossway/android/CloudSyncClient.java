package com.meshcrossway.android;

import android.content.Context;
import android.content.SharedPreferences;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.*;
import org.json.*;
import static com.meshcrossway.android.MeshData.*;

/** User-triggered first version of cloud sync: recent messages and favorite chats. */
final class CloudSyncClient {
    interface Callback { void complete(String message, boolean success); }
    private static final ExecutorService executor=Executors.newSingleThreadExecutor(r->{Thread t=new Thread(r,"mesh-cloud-sync");t.setDaemon(true);return t;});
    static boolean configured(){return BuildConfig.CLOUD_API_BASE_URL.trim().startsWith("https://");}

    static void sync(Context context,RadioService service,Callback callback){
        if(!configured()){callback.complete("Cloud API is not configured in this build",false);return;}
        if(service==null||service.history==null){callback.complete("Local history is not ready",false);return;}
        ClerkBridge.sessionToken(token->{if(token==null||token.isEmpty()){callback.complete("Sign in again to synchronize",false);return;}executor.execute(()->{
            try{
                SharedPreferences radios=context.getSharedPreferences("radios",Context.MODE_PRIVATE);
                for(RadioProfile profile:RadioProfile.values()){
                    requireEnabled(context);
                    String radio=radios.getString(profile.name(),"");
                    syncFavorites(context,service.history,profile,radio,token);
                    requireEnabled(context);
                    JSONArray messages=await(cb->service.history.cloudMessages(profile,radio,cb));
                    if(messages==null)throw new IOException("Could not read local message history");
                    for(int start=0;start<messages.length();start+=40){requireEnabled(context);JSONArray batch=new JSONArray();for(int i=start;i<Math.min(messages.length(),start+40);i++)batch.put(messages.get(i));JSONObject payload=new JSONObject().put("messages",batch);request("POST","/v1/messages",token,payload.toString(),"application/json; charset=utf-8");}
                    String cursor=null;
                    do{
                        requireEnabled(context);
                        String path="/v1/messages"+(cursor==null?"":"?cursor="+URLEncoder.encode(cursor,"UTF-8"));
                        JSONObject page=new JSONObject(request("GET",path,token,null,null));
                        JSONArray remote=page.optJSONArray("messages");
                        if(remote!=null&&remote.length()>0)awaitDone(done->service.history.mergeCloudMessages(profile,radio,remote,done));
                        cursor=page.isNull("nextCursor")?null:page.optString("nextCursor",null);
                    }while(cursor!=null&&!cursor.isEmpty());
                }
                callback.complete("Messages and favorites synchronized",true);
            }catch(Exception e){callback.complete(safeMessage(e),false);}
        });});
    }

    private static void syncFavorites(Context context,HistoryStore history,RadioProfile profile,String radio,String token)throws Exception{
        requireEnabled(context);
        JSONArray local=await(cb->history.cloudFavorites(profile,radio,cb));
        if(local==null)throw new IOException("Could not read local favorites");
        requireEnabled(context);
        JSONObject first=new JSONObject(request("GET","/v1/favorites",token,null,null));
        JSONArray remote=first.optJSONArray("favorites");if(remote==null)remote=new JSONArray();
        java.util.HashMap<String,Long> remoteTimes=new java.util.HashMap<>();
        for(int i=0;i<remote.length();i++){JSONObject row=remote.optJSONObject(i);if(row!=null&&profile.name().equals(row.optString("network")))remoteTimes.put(row.optString("conversation_id",""),row.optLong("updated_at",0));}
        for(int i=0;i<local.length();i++){
            requireEnabled(context);
            JSONObject row=local.optJSONObject(i);if(row==null)continue;String topic=row.optString("conversationId","");long time=row.optLong("updatedAt",0);
            if(!topic.isEmpty()&&time>remoteTimes.getOrDefault(topic,0L))request("PUT","/v1/favorites",token,row.toString(),"application/json; charset=utf-8");
        }
        requireEnabled(context);
        JSONObject latest=new JSONObject(request("GET","/v1/favorites",token,null,null));JSONArray all=latest.optJSONArray("favorites");
        if(all!=null&&all.length()>0)awaitDone(done->history.mergeCloudFavorites(profile,radio,all,done));
    }

    private static void requireEnabled(Context context)throws IOException{
        if(!context.getSharedPreferences("cloud_sync",Context.MODE_PRIVATE).getBoolean("enabled",false))throw new IOException("Cloud sync was disabled");
    }

    private interface AsyncValue<T>{void get(java.util.function.Consumer<T> callback);}
    private interface AsyncAction{void run(Runnable callback);}
    private static <T>T await(AsyncValue<T> operation)throws Exception{
        CountDownLatch latch=new CountDownLatch(1);java.util.concurrent.atomic.AtomicReference<T> value=new java.util.concurrent.atomic.AtomicReference<>();
        operation.get(result->{value.set(result);latch.countDown();});
        if(!latch.await(60,TimeUnit.SECONDS))throw new IOException("Local sync timed out");return value.get();
    }
    private static void awaitDone(AsyncAction operation)throws Exception{
        CountDownLatch latch=new CountDownLatch(1);operation.run(latch::countDown);
        if(!latch.await(60,TimeUnit.SECONDS))throw new IOException("Local sync timed out");
    }
    private static String request(String method,String path,String token,String body,String contentType)throws Exception{
        URL url=new URL(BuildConfig.CLOUD_API_BASE_URL.trim().replaceAll("/+$","")+path);if(!"https".equalsIgnoreCase(url.getProtocol()))throw new IOException("Cloud API must use HTTPS");HttpURLConnection connection=(HttpURLConnection)url.openConnection();
        connection.setRequestMethod(method);connection.setConnectTimeout(15000);connection.setReadTimeout(30000);connection.setRequestProperty("Authorization","Bearer "+token);connection.setRequestProperty("Accept","application/json");
        if(body!=null){connection.setDoOutput(true);connection.setRequestProperty("Content-Type",contentType);byte[] bytes=body.getBytes(StandardCharsets.UTF_8);connection.setFixedLengthStreamingMode(bytes.length);try(OutputStream out=connection.getOutputStream()){out.write(bytes);}}
        int status=connection.getResponseCode();InputStream stream=status>=400?connection.getErrorStream():connection.getInputStream();String response="";if(stream!=null)try(InputStream in=stream;ByteArrayOutputStream out=new ByteArrayOutputStream()){byte[] buffer=new byte[8192];int n;while((n=in.read(buffer))!=-1){if(out.size()+n>2*1024*1024)throw new IOException("Cloud response too large");out.write(buffer,0,n);}response=out.toString("UTF-8");}connection.disconnect();
        if(status<200||status>=300){String code="Cloud sync failed (HTTP "+status+")";try{code=new JSONObject(response).optString("error",code);}catch(Exception ignored){}throw new IOException(code);}return response;
    }
    private static String safeMessage(Exception error){String message=error.getMessage();if(message==null||message.isEmpty())return "Cloud sync failed • check your connection and try again";if(message.startsWith("TRACK_")||message.equals("UNAUTHORIZED"))return message;return "Cloud sync failed • "+message;}
}

