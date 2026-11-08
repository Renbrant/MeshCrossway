package com.meshcrossway.android;

import android.content.Context;
import java.io.IOException;
import org.maplibre.android.MapLibre;
import org.maplibre.android.module.http.HttpRequestUtil;
import okhttp3.*;
import okio.*;

final class MapEngine {
    static final String DARK="https://tiles.openfreemap.org/styles/dark",LIGHT="https://tiles.openfreemap.org/styles/liberty";
    static volatile TrafficBudget budget;
    private static OfflineMaps offline;
    static void pauseDownloads(){if(offline!=null)offline.pause();}
    static synchronized OfflineMaps init(Context context){
        if(offline!=null)return offline;
        MapLibre.getInstance(context.getApplicationContext());
        HttpRequestUtil.setLogEnabled(false);HttpRequestUtil.setPrintRequestUrlOnFailure(false);
        Dispatcher dispatcher=new Dispatcher();dispatcher.setMaxRequests(4);dispatcher.setMaxRequestsPerHost(4);
        OkHttpClient client=new OkHttpClient.Builder().dispatcher(dispatcher).addInterceptor(chain->{
            Request request=chain.request().newBuilder().header("User-Agent","MeshCrosswayAndroid/0.3.0 (+https://github.com/Renbrant/MeshCrossway)").build();
            Response response=chain.proceed(request);ResponseBody original=response.body();TrafficBudget limit=budget;
            if(original==null||limit==null)return response;
            ResponseBody guarded=new ResponseBody(){private BufferedSource source;
                @Override public MediaType contentType(){return original.contentType();}
                @Override public long contentLength(){return original.contentLength();}
                @Override public BufferedSource source(){if(source==null)source=Okio.buffer(new ForwardingSource(original.source()){
                    @Override public long read(Buffer sink,long bytes)throws IOException{if(bytes==0)return 0;long grant=limit.reserve(Math.min(8192,bytes));if(grant==0)throw new IOException("Offline download budget reached");try{long n=super.read(sink,grant);limit.refund(n<0?grant:grant-n);return n;}catch(IOException e){limit.refund(grant);throw e;}}
                });return source;}
            };return response.newBuilder().body(guarded).build();
        }).build();HttpRequestUtil.setOkHttpClient(client);offline=new OfflineMaps(context.getApplicationContext());return offline;
    }
}
