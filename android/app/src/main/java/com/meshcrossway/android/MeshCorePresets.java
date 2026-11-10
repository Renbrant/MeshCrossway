package com.meshcrossway.android;

import android.os.Handler;
import android.os.Looper;
import org.json.JSONArray;
import org.json.JSONObject;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;

/** Community suggested radio presets served by the official MeshCore app API. */
final class MeshCorePresets {
    static final class Preset {
        final String title;
        final double frequency, bandwidth;
        final int spreadingFactor, codingRate, pathHashBytes;
        Preset(String title,double frequency,double bandwidth,int sf,int cr,int pathBytes){
            this.title=title;this.frequency=frequency;this.bandwidth=bandwidth;
            spreadingFactor=sf;codingRate=cr;pathHashBytes=pathBytes;
        }
    }
    interface Callback { void complete(ArrayList<Preset> presets,String error); }
    static void fetch(String userAgent,Callback callback){
        Handler main=new Handler(Looper.getMainLooper());
        new Thread(()->{
            ArrayList<Preset> presets=new ArrayList<>();String error="";
            HttpURLConnection connection=null;
            try{
                connection=(HttpURLConnection)new URL("https://api.meshcore.nz/api/v1/config").openConnection();
                connection.setConnectTimeout(7000);connection.setReadTimeout(7000);
                connection.setRequestProperty("User-Agent",userAgent);connection.setRequestProperty("Accept","application/json");
                if(connection.getResponseCode()!=200)throw new IllegalStateException("Preset service returned HTTP "+connection.getResponseCode());
                java.io.InputStream in=connection.getInputStream();java.io.ByteArrayOutputStream out=new java.io.ByteArrayOutputStream();byte[] buffer=new byte[4096];int count;
                while((count=in.read(buffer))!=-1){if(out.size()+count>512*1024)throw new IllegalStateException("Preset response is too large");out.write(buffer,0,count);}in.close();
                JSONObject root=new JSONObject(out.toString("UTF-8"));JSONObject config=root.optJSONObject("config");JSONObject settings=config==null?null:config.optJSONObject("suggested_radio_settings");JSONArray entries=settings==null?null:settings.optJSONArray("entries");
                if(entries==null)throw new IllegalStateException("No radio presets were provided");
                for(int i=0;i<entries.length();i++){
                    JSONObject item=entries.optJSONObject(i);if(item==null)continue;
                    double f=number(item,"frequency"),bw=number(item,"bandwidth");int sf=(int)number(item,"spreading_factor"),cr=(int)number(item,"coding_rate");
                    if(f<150||f>2500||bw<7||bw>500||sf<5||sf>12||cr<5||cr>8)continue;
                    int pathBytes=0;JSONObject network=item.optJSONObject("network_settings");if(network!=null)pathBytes=network.optInt("path_hash_size",0);
                    if(pathBytes<0||pathBytes>3)pathBytes=0;
                    presets.add(new Preset(item.optString("title","Radio preset"),f,bw,sf,cr,pathBytes));
                }
                if(presets.isEmpty())throw new IllegalStateException("No valid radio presets were found");
            }catch(Exception e){error=e.getMessage()==null?"Could not load MeshCore presets":e.getMessage();}
            finally{if(connection!=null)connection.disconnect();}
            String result=error;main.post(()->callback.complete(presets,result));
        },"meshcore-presets").start();
    }
    private static double number(JSONObject item,String key){try{return Double.parseDouble(item.optString(key,""));}catch(Exception ignored){return 0;}}
    private MeshCorePresets(){}
}
