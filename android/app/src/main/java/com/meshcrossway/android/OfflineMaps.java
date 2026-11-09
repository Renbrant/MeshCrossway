package com.meshcrossway.android;

import android.content.Context;
import java.nio.charset.StandardCharsets;
import java.util.*;
import org.json.*;
import org.maplibre.android.geometry.LatLngBounds;
import org.maplibre.android.offline.*;

/** One explicit region download at a time; independent of the two BLE clients. */
final class OfflineMaps {
    static final long BYTE_LIMIT=45_000_000L;
    static final class Area {OfflineRegion region;String name="Area",status="Saved";long bytes,tiles;boolean complete,limited;}
    final List<Area> areas=new ArrayList<>();private final OfflineManager manager;private Area active;Runnable listener;String error="";boolean loading;
    OfflineMaps(Context context){manager=OfflineManager.getInstance(context);refresh();}
    private void changed(){if(listener!=null)listener.run();}
    void refresh(){if(loading)return;loading=true;manager.listOfflineRegions(new OfflineManager.ListOfflineRegionsCallback(){
        public void onList(OfflineRegion[] regions){loading=false;areas.clear();for(OfflineRegion r:regions){Area a=active!=null&&active.region.getId()==r.getId()?active:new Area();a.region=r;try{JSONObject meta=new JSONObject(new String(r.getMetadata(),StandardCharsets.UTF_8));a.name=meta.optString("name","Area");a.limited=meta.optBoolean("limited",false);}catch(JSONException ignored){}areas.add(a);r.getStatus(new OfflineRegion.OfflineRegionStatusCallback(){public void onStatus(OfflineRegionStatus status){update(a,status);}public void onError(String message){a.status="Could not read status";changed();}});}changed();}
        public void onError(String message){loading=false;error="Saved areas could not be loaded";changed();}
    });}
    private void update(Area area,OfflineRegionStatus status){area.bytes=status.getCompletedResourceSize();area.tiles=status.getCompletedTileCount();area.complete=status.isComplete()&&status.isRequiredResourceCountPrecise();area.status=area.complete?"Ready offline":(active==area?"Downloading":"Paused")+" • "+area.tiles+" tiles • "+String.format(Locale.ROOT,"%.1f MB",area.bytes/1e6);
        if(active==area){if(area.bytes>=BYTE_LIMIT||area.tiles>=2800){area.limited=true;pause();area.status="Limit reached • select a smaller area";saveMetadata(area);}else if(area.complete){pause();area.status="Ready offline";}}changed();}
    private void saveMetadata(Area area){try{byte[] data=new JSONObject().put("name",area.name).put("limited",area.limited).toString().getBytes(StandardCharsets.UTF_8);area.region.updateMetadata(data,new OfflineRegion.OfflineRegionUpdateMetadataCallback(){public void onUpdate(byte[] metadata){}public void onError(String message){}});}catch(JSONException ignored){}}
    void create(String name,String style,LatLngBounds bounds,int zoom,float pixelRatio){
        if(areas.size()>=20){error="Remove an old area before adding another";changed();return;}
        if(name.trim().isEmpty()||name.length()>80||GeoMath.tileEstimate(bounds.getLatSouth(),bounds.getLonWest(),bounds.getLatNorth(),bounds.getLonEast(),zoom)>GeoMath.MAX_TILES){error="Zoom in on a smaller area or lower download detail";changed();return;}
        pause();error="";OfflineTilePyramidRegionDefinition definition=new OfflineTilePyramidRegionDefinition(style,bounds,0,zoom,pixelRatio,false);
        byte[] metadata;try{metadata=new JSONObject().put("name",name.trim()).toString().getBytes(StandardCharsets.UTF_8);}catch(JSONException impossible){throw new AssertionError(impossible);}
        manager.createOfflineRegion(definition,metadata,new OfflineManager.CreateOfflineRegionCallback(){public void onCreate(OfflineRegion region){Area area=new Area();area.region=region;area.name=name.trim();areas.add(area);start(area);}public void onError(String message){error="Area could not be created • check storage";changed();}});
    }
    void start(Area area){if(area.limited){error="This area reached the download limit • create a smaller one";changed();return;}if(area.complete)return;pause();active=area;MapEngine.budget=new TrafficBudget(BYTE_LIMIT-area.bytes);error="";
        area.region.setObserver(new OfflineRegion.OfflineRegionObserver(){public void onStatusChanged(OfflineRegionStatus status){update(area,status);}public void onError(OfflineRegionError failure){error="Download paused • check connection or use a smaller area";pause();changed();}public void mapboxTileCountLimitExceeded(long limit){area.limited=true;pause();saveMetadata(area);error="Tile limit reached • create a smaller area";changed();}});
        area.region.setDownloadState(OfflineRegion.STATE_ACTIVE);changed();
    }
    void pause(){if(active!=null){Area previous=active;active=null;previous.region.setDownloadState(OfflineRegion.STATE_INACTIVE);previous.status=previous.complete?"Ready offline":"Paused";}MapEngine.budget=null;changed();}
    void delete(Area area){if(active==area)pause();area.region.delete(new OfflineRegion.OfflineRegionDeleteCallback(){public void onDelete(){areas.remove(area);changed();}public void onError(String message){error="Area could not be deleted";changed();}});}
    boolean active(Area area){return active==area;}
}
