package com.meshcrossway.android;

import android.content.Context;
import android.net.*;
import android.os.Handler;
import android.os.Looper;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import fi.iki.elonen.NanoHTTPD;
import java.io.*;
import java.math.BigInteger;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.security.*;
import java.security.cert.Certificate;
import java.util.*;
import java.util.concurrent.*;
import javax.net.ssl.*;
import javax.security.auth.x500.X500Principal;
import org.json.*;
import org.maplibre.android.geometry.LatLngBounds;
import static com.meshcrossway.android.MeshData.*;

/**
 * Explicitly enabled local HTTPS workspace, owned by RadioService.
 * Binds only to the Wi-Fi IPv4 address (never 0.0.0.0 or mobile data).
 * All radio, MapLibre and track-state mutations are marshalled onto the main looper.
 * No cross-network routing or automatic forwarding is performed.
 */
final class RemoteWorkspace extends NanoHTTPD {
    static final int PORT=8080;
    private static final SecureRandom RANDOM=new SecureRandom();
    private static final long SESSION_MS=3_600_000;
    private final RadioService owner;
    private final Handler main=new Handler(Looper.getMainLooper());
    final String ip,fingerprint;
    private volatile String code;
    static String newShortPin(){return String.format(Locale.ROOT,"%06d",RANDOM.nextInt(1_000_000));}
    static boolean validPin(String pin){return pin!=null&&pin.matches("[0-9]{4,8}");}
    String accessPin(){return code;}
    /** Changing a PIN terminates existing authenticated browser sessions. */
    synchronized void changePin(String pin){
        if(!validPin(pin))throw new IllegalArgumentException("Enter 4 to 8 digits");
        code=pin;
        sessions.clear();
        failures=0;
        lockedUntil=0;
    }
    private final ConcurrentHashMap<String,Long> sessions=new ConcurrentHashMap<>();
    String tlsDiagnostics(){return "AES-GCM sealed local RSA TLS identity";}
    SSLContext tlsContext; // Package-local for isolated Android TLS smoke tests.
    private int failures;
    private long lockedUntil;

    static String wifiName(Context context){
        try{
            android.net.wifi.WifiManager manager=(android.net.wifi.WifiManager)
                context.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
            if(manager==null||manager.getConnectionInfo()==null)return "Wi-Fi";
            String ssid=manager.getConnectionInfo().getSSID();
            if(ssid==null||ssid.isEmpty()||ssid.contains("unknown ssid"))return "Wi-Fi";
            return ssid.replace(String.valueOf((char)34),"");
        }catch(SecurityException e){return "Wi-Fi";}
    }
    static String wifiAddress(Context context){
        ConnectivityManager cm=(ConnectivityManager)context.getSystemService(Context.CONNECTIVITY_SERVICE);
        if(cm==null)return null;
        for(Network network:cm.getAllNetworks()){
            NetworkCapabilities caps=cm.getNetworkCapabilities(network);
            if(caps==null||!caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI))continue;
            LinkProperties links=cm.getLinkProperties(network);
            if(links==null)continue;
            for(LinkAddress link:links.getLinkAddresses()){
                InetAddress address=link.getAddress();
                if(address instanceof Inet4Address && address.isSiteLocalAddress())
                    return address.getHostAddress();
            }
        }
        return null;
    }
    private static String randomToken(int bytes){
        byte[] data=new byte[bytes];RANDOM.nextBytes(data);
        return Base64.getUrlEncoder().withoutPadding().encodeToString(data);
    }
    static RemoteWorkspace start(RadioService owner,String pin)throws Exception{
        String ip=wifiAddress(owner);
        if(ip==null)throw new IOException("Connect the phone to a local Wi-Fi network first");
        RemoteWorkspace server=new RemoteWorkspace(owner,ip,pin);
        server.start(SOCKET_READ_TIMEOUT,false);
        return server;
    }
    RemoteWorkspace(RadioService owner,String ip)throws Exception{this(owner,ip,null);}
    RemoteWorkspace(RadioService owner,String ip,String chosenPin)throws Exception{
        super(ip,PORT);this.owner=owner;this.ip=ip;
        changePin(chosenPin==null?newShortPin():chosenPin);
        RemoteTlsIdentity identity=RemoteTlsIdentity.load(owner,ip);
        Certificate cert=identity.certificate;
        byte[] digest=MessageDigest.getInstance("SHA-256").digest(cert.getEncoded());
        StringBuilder formatted=new StringBuilder();
        for(byte b:digest)formatted.append(String.format(Locale.ROOT,"%02X",b&255));
        fingerprint=formatted.toString();
        tlsContext=identity.context;
        makeSecure(tlsContext.getServerSocketFactory(),new String[]{"TLSv1.2"});
    }
    String url(){return "https://"+ip+":"+PORT+"/";}
    private <T> T onMain(Callable<T> action)throws Exception{
        FutureTask<T> task=new FutureTask<>(action);
        if(Looper.myLooper()==Looper.getMainLooper())task.run();else main.post(task);
        try{return task.get(6,TimeUnit.SECONDS);}catch(TimeoutException e){task.cancel(false);throw new IOException("Android service did not respond",e);}
    }
    private static JSONObject obj(Object... pairs)throws JSONException{
        JSONObject value=new JSONObject();
        for(int i=0;i+1<pairs.length;i+=2)value.put((String)pairs[i],pairs[i+1]);
        return value;
    }
    private Response respond(Response.Status status,String mime,String text){
        Response response=newFixedLengthResponse(status,mime,text);
        response.addHeader("Cache-Control","no-store");
        response.addHeader("X-Content-Type-Options","nosniff");
        response.addHeader("X-Frame-Options","DENY");
        // Map preview requests need a browser Referer for the OpenStreetMap tile policy.
        // Only the HTTPS origin (never messages, paths, or PINs) is sent.
        response.addHeader("Referrer-Policy","strict-origin-when-cross-origin");
        response.addHeader("Content-Security-Policy",
            "default-src 'none'; connect-src 'self'; img-src 'self' data: https://tile.openstreetmap.org; "+
            "script-src 'self'; style-src 'self' 'unsafe-inline'; base-uri 'none'; "+
            "frame-ancestors 'none'; form-action 'self'");
        return response;
    }
    private Response json(Response.Status status,JSONObject payload){
        return respond(status,"application/json; charset=utf-8",payload.toString());
    }
    private Response error(Response.Status status,String message){
        try{return json(status,obj("error",message));}
        catch(JSONException e){return respond(status,MIME_PLAINTEXT,"Request rejected");}
    }
    private Response asset(String name,String mime){
        try(InputStream in=owner.getAssets().open("remote/"+name);
            ByteArrayOutputStream out=new ByteArrayOutputStream()){
            byte[] buffer=new byte[4096];int n;
            while((n=in.read(buffer))!=-1)out.write(buffer,0,n);
            return respond(Response.Status.OK,mime,out.toString("UTF-8"));
        }catch(IOException e){return error(Response.Status.INTERNAL_ERROR,"Web interface unavailable");}
    }
    private boolean authorized(IHTTPSession request){
        String cookies=request.getHeaders().getOrDefault("cookie","");
        for(String entry:cookies.split(";")){
            String item=entry.trim();
            if(!item.startsWith("mcx_session="))continue;
            String token=item.substring("mcx_session=".length());
            Long until=sessions.get(token);
            if(until!=null&&until>System.currentTimeMillis())return true;
        }
        return false;
    }
    private synchronized boolean checkCode(String supplied){
        long now=System.currentTimeMillis();
        if(now<lockedUntil)return false;
        boolean valid=MessageDigest.isEqual(code.getBytes(StandardCharsets.UTF_8),
            supplied.getBytes(StandardCharsets.UTF_8));
        if(valid){failures=0;return true;}
        if(++failures>=5){lockedUntil=now+60_000;failures=0;}
        return false;
    }
    private JSONObject parse(IHTTPSession request)throws Exception{
        String length=request.getHeaders().getOrDefault("content-length","");
        String transfer=request.getHeaders().getOrDefault("transfer-encoding","");
        long size;
        try{size=Long.parseLong(length);}catch(NumberFormatException e){throw new IOException("Content-Length required");}
        if(size<2||size>8192||!transfer.isEmpty())throw new IOException("Request too large");
        if(!request.getHeaders().getOrDefault("content-type","").toLowerCase(Locale.ROOT).startsWith("application/json"))
            throw new IOException("Use application/json");
        Map<String,String> body=new HashMap<>();
        request.parseBody(body);
        String json=body.get("postData");
        if(json==null||json.length()>8192)throw new IOException("Invalid JSON body");
        return new JSONObject(json);
    }
    private boolean sameOrigin(IHTTPSession request){
        String host=request.getHeaders().getOrDefault("host","");
        String origin=request.getHeaders().get("origin");
        return host.equals(ip+":"+PORT)&&(origin==null||origin.equals("https://"+host));
    }
    @Override public Response serve(IHTTPSession request){
        if(!sameOrigin(request))return error(Response.Status.FORBIDDEN,"Invalid host or origin");
        String path=request.getUri();
        boolean get=request.getMethod()==Method.GET,post=request.getMethod()==Method.POST;
        if(get&&path.equals("/"))return asset("index.html","text/html; charset=utf-8");
        if(get&&path.equals("/app.js"))return asset("app.js","application/javascript; charset=utf-8");
        if(get&&path.equals("/remote-map.js"))return asset("remote-map.js","application/javascript; charset=utf-8");
        if(post&&path.equals("/api/login")){
            try{
                String entered=parse(request).optString("code","");
                if(!checkCode(entered))return error(Response.Status.UNAUTHORIZED,"Invalid code or temporarily locked");
                String token=randomToken(32);
                sessions.put(token,System.currentTimeMillis()+SESSION_MS);
                if(sessions.size()>10){sessions.clear();sessions.put(token,System.currentTimeMillis()+SESSION_MS);}
                Response response=json(Response.Status.OK,obj("ok",true));
                response.addHeader("Set-Cookie","mcx_session="+token+"; Path=/; Secure; HttpOnly; SameSite=Strict; Max-Age=3600");
                return response;
            }catch(Exception e){return error(Response.Status.BAD_REQUEST,"Invalid login request");}
        }
        if(!authorized(request))return error(Response.Status.UNAUTHORIZED,"Sign in on your phone's local workspace");
        if(post&&!request.getHeaders().getOrDefault("x-mcx-request","").equals("1"))
            return error(Response.Status.FORBIDDEN,"Request verification required");
        try{
            if(get&&path.equals("/api/state"))return json(Response.Status.OK,onMain(this::state));
            if(get&&path.equals("/api/messages"))return json(Response.Status.OK,
                onMain(()->messages(request.getParms().get("network"),request.getParms().get("topic"))));
            if(post&&path.equals("/api/send")){
                JSONObject body=parse(request);
                return json(Response.Status.OK,send(body));
            }
            if(post&&path.equals("/api/read")){
                JSONObject body=parse(request);
                return json(Response.Status.OK,onMain(()->markRead(body)));
            }
            if(get&&path.equals("/api/maps"))return json(Response.Status.OK,onMain(this::maps));
            if(post&&path.equals("/api/maps/action")){
                JSONObject body=parse(request);
                return json(Response.Status.OK,onMain(()->mapAction(body)));
            }
            if(get&&path.equals("/api/tracks"))return json(Response.Status.OK,onMain(this::tracks));
            if(get&&path.equals("/api/tracks/preview"))
                return json(Response.Status.OK,trackPreview(request.getParms().get("id")));
            if(get&&path.equals("/api/tracks/export"))
                return trackExport(request.getParms().get("id"));
            if(post&&path.equals("/api/tracks/action")){
                JSONObject body=parse(request);
                return json(Response.Status.OK,onMain(()->trackAction(body)));
            }
            if(post&&path.equals("/api/logout")){
                sessions.clear();
                Response response=json(Response.Status.OK,obj("ok",true));
                response.addHeader("Set-Cookie","mcx_session=; Path=/; Secure; HttpOnly; SameSite=Strict; Max-Age=0");
                return response;
            }
            return error(Response.Status.NOT_FOUND,"Unknown endpoint");
        }catch(IllegalArgumentException e){
            return error(Response.Status.BAD_REQUEST,e.getMessage()==null?"Invalid request":e.getMessage());
        }catch(Exception e){return error(Response.Status.INTERNAL_ERROR,"Local workspace request failed");}
    }
    private static RadioProfile profile(String network){
        if("mt".equals(network))return RadioProfile.MESHTASTIC;
        if("mc".equals(network))return RadioProfile.MESHCORE;
        throw new IllegalArgumentException("Choose Meshtastic or MeshCore");
    }
    private JSONObject state()throws JSONException{
        JSONArray radios=new JSONArray();
        for(RadioProfile p:RadioProfile.values()){
            RadioService.Session session=owner.sessions.get(p);
            Snapshot data=owner.data(p);
            JSONArray contacts=new JSONArray(),channels=new JSONArray(),conversations=new JSONArray();
            int nodes=0;
            for(Node node:data.nodes){
                if(nodes++>=500)break;
                contacts.put(obj("id",node.id,"topic",Target.node(node).topic,
                    "name",node.name.isEmpty()?node.id:node.name,
                    "messageable",!node.unmessagable,"heard",node.heard,"battery",node.battery,"position",node.position,
                    "lat",node.position?node.lat:JSONObject.NULL,"lon",node.position?node.lon:JSONObject.NULL));
            }
            HashMap<String,Conversation> conversationsByTopic=new HashMap<>();
            for(Conversation conversation:data.conversations())
                conversationsByTopic.put(conversation.target.topic,conversation);
            for(Channel channel:data.channels){
                Conversation seen=conversationsByTopic.get(channel.topic());
                channels.put(obj("topic",channel.topic(),"name",channel.name,"slot",channel.slot,
                    "enabled",channel.enabled,"unread",seen==null?0:seen.unread,
                    "preview",seen==null?"":seen.last.body,
                    "time",seen==null?0:seen.last.timestamp));
            }
            int count=0;
            for(Conversation conversation:data.conversations()){
                if(count++>=200)break;
                Message last=conversation.last;
                conversations.put(obj("topic",conversation.target.topic,"title",conversation.target.title,
                    "unread",conversation.unread,"preview",last.body==null?"":last.body,
                    "time",last.timestamp,"sender",last.sender,"group",conversation.target.group));
            }
            radios.put(obj("network",p==RadioProfile.MESHTASTIC?"mt":"mc","name",p.title,
                "ready",session.verified,"status",session.verified?"Ready":session.desired?session.phase:"Offline",
                "device",session.name,"nodes",data.nodes.size(),"unread",data.unread(),
                "contacts",contacts,"channels",channels,"conversations",conversations));
        }
        return obj("version","0.2.2","radios",radios,"trackManagerReady",owner.remoteTracks!=null);
    }
    private static Target target(Snapshot data,String topic){
        if(topic==null||topic.length()>256)return null;
        for(Conversation conversation:data.conversations())if(conversation.target.topic.equals(topic))return conversation.target;
        for(Channel channel:data.channels)
            if(channel.enabled&&channel.topic().equals(topic))return Target.channel(channel);
        for(Node node:data.nodes){
            Target item=Target.node(node);
            if(item.topic.equals(topic))return item;
        }
        return null;
    }
    private JSONObject messages(String network,String topic)throws JSONException{
        RadioProfile p=profile(network);
        Snapshot data=owner.data(p);
        if(target(data,topic)==null)throw new IllegalArgumentException("Unknown conversation");
        JSONArray messages=new JSONArray();
        ArrayList<Message> found=new ArrayList<>();
        for(Message message:data.messages)if(message.topic.equals(topic))found.add(message);
        found.sort(Comparator.comparingLong(m->m.timestamp));
        for(Message message:found.subList(Math.max(0,found.size()-200),found.size()))
            messages.put(obj("uid",message.uid,"sender",message.sender,"body",message.body,
                "time",message.timestamp,"outgoing",message.outgoing,"state",message.state));
        return obj("messages",messages,"totalLoaded",found.size());
    }
    private JSONObject markRead(JSONObject body)throws JSONException{
        RadioProfile p=profile(body.optString("network"));
        String topic=body.optString("topic","");
        if(target(owner.data(p),topic)==null)throw new IllegalArgumentException("Unknown conversation");
        RadioService.Session s=owner.sessions.get(p);
        owner.history.read(p,s.address,topic);
        return obj("ok",true);
    }
    private JSONObject send(JSONObject body)throws Exception{
        RadioProfile p=profile(body.optString("network"));
        String topic=body.optString("topic","");
        String text=body.optString("text","");
        if(text.trim().isEmpty()||text.length()>2000)throw new IllegalArgumentException("Invalid message length");
        CompletableFuture<String> outcome=new CompletableFuture<>();
        main.post(()->{
            RadioService.Session session=owner.sessions.get(p);
            Target target=target(owner.data(p),topic);
            if(target==null){outcome.complete("Unknown conversation");return;}
            owner.send(p,session.address,target,text,outcome::complete);
        });
        String error;
        try{error=outcome.get(18,TimeUnit.SECONDS);}
        catch(TimeoutException e){return obj("ok",false,"status","Unknown; check local history before retrying");}
        return error.isEmpty()?obj("ok",true,"status","Saved; submitted to radio queue, not recipient-delivered"):
            obj("ok",false,"status",error);
    }
    private OfflineMaps offline(){return MapEngine.init(owner);}
    private JSONObject maps()throws JSONException{
        OfflineMaps manager=offline();
        JSONArray areas=new JSONArray();
        for(OfflineMaps.Area area:manager.areas){
            org.maplibre.android.offline.OfflineRegionDefinition definition=area.region.getDefinition();
            double north=0,south=0,east=0,west=0;int zoom=0;
            if(definition instanceof org.maplibre.android.offline.OfflineTilePyramidRegionDefinition){
                org.maplibre.android.offline.OfflineTilePyramidRegionDefinition tile=
                    (org.maplibre.android.offline.OfflineTilePyramidRegionDefinition)definition;
                LatLngBounds bounds=tile.getBounds();
                north=bounds.getLatNorth();south=bounds.getLatSouth();
                east=bounds.getLonEast();west=bounds.getLonWest();
                zoom=(int)tile.getMaxZoom();
            }
            areas.put(obj("id",area.region.getId(),"name",area.name,"status",area.status,
                "bytes",area.bytes,"tiles",area.tiles,"complete",area.complete,
                "north",north,"south",south,"east",east,"west",west,"zoom",zoom,
                "active",manager.active(area)));
        }
        return obj("areas",areas,"error",manager.error,"maxTiles",GeoMath.MAX_TILES,
            "maxBytes",OfflineMaps.BYTE_LIMIT);
    }
    private JSONObject mapAction(JSONObject body)throws JSONException{
        OfflineMaps manager=offline();
        String action=body.optString("action","");
        if(action.equals("create")){
            String name=body.optString("name","").trim();
            double north=body.optDouble("north",Double.NaN),south=body.optDouble("south",Double.NaN);
            double east=body.optDouble("east",Double.NaN),west=body.optDouble("west",Double.NaN);
            int zoom=body.optInt("zoom",-1);
            if(name.length()<1||name.length()>80||!GeoMath.valid(north,east)||
               !GeoMath.valid(south,west)||north<=south||west>=east||zoom<1||zoom>15)
                throw new IllegalArgumentException("Invalid map bounds, name or zoom");
            long estimated=GeoMath.tileEstimate(south,west,north,east,zoom);
            if(estimated>GeoMath.MAX_TILES)throw new IllegalArgumentException("Area exceeds tile limit");
            String style=body.optString("style","dark").equals("light")?MapEngine.LIGHT:MapEngine.DARK;
            manager.create(name,style,LatLngBounds.from(north,east,south,west),zoom,1f);
            return obj("ok",true,"estimatedTiles",estimated);
        }
        if(action.equals("pause")){manager.pause();return obj("ok",true);}
        long id=body.optLong("id",-1);
        for(OfflineMaps.Area area:manager.areas)if(area.region.getId()==id){
            if(action.equals("resume"))manager.start(area);
            else if(action.equals("delete"))manager.delete(area);
            else throw new IllegalArgumentException("Unknown map action");
            return obj("ok",true);
        }
        throw new IllegalArgumentException("Map area not found");
    }
    private TrackStore trackStore(){
        if(owner.remoteTracks==null)throw new IllegalArgumentException("Track manager is starting");
        return owner.remoteTracks.store;
    }
    private JSONObject tracks()throws JSONException{
        TrackStore store=trackStore();JSONArray folders=new JSONArray(),tracks=new JSONArray();
        for(TrackStore.Folder folder:store.folders)folders.put(obj("id",folder.id,"name",folder.name));
        for(TrackStore.Track track:store.tracks){
            tracks.put(obj("id",track.id,"name",track.name,"folder",track.folderId,
                "state",track.state,"color",track.color,"visible",track.visible,"count",track.count,
                "distance",track.distance,"created",track.created));
        }
        return obj("folders",folders,"tracks",tracks,"error",store.error);
    }
    private JSONObject trackAction(JSONObject body)throws JSONException{
        TrackStore store=trackStore();
        String action=body.optString("action",""),id=body.optString("id","");
        if(action.equals("folderCreate")){
            String name=body.optString("name","").trim();
            if(name.isEmpty()||name.length()>80)throw new IllegalArgumentException("Invalid folder name");
            store.createFolder(name);return obj("ok",true);
        }
        if(action.equals("folderRename")||action.equals("folderDelete")){
            boolean exists=false;
            for(TrackStore.Folder folder:store.folders)if(folder.id.equals(id))exists=true;
            if(!exists)throw new IllegalArgumentException("Folder not found");
            if(action.equals("folderRename")){
                String name=body.optString("name","").trim();
                if(name.isEmpty()||name.length()>80)throw new IllegalArgumentException("Invalid folder name");
                store.renameFolder(id,name);
            }else store.deleteFolder(id);
            return obj("ok",true);
        }
        TrackStore.Track track=null;
        for(TrackStore.Track candidate:store.tracks)if(candidate.id.equals(id))track=candidate;
        if(track==null)throw new IllegalArgumentException("Track not found");
        if(action.equals("move")){
            String folder=body.optString("folder","");
            if(!folder.isEmpty()){
                boolean exists=false;
                for(TrackStore.Folder candidate:store.folders)if(candidate.id.equals(folder))exists=true;
                if(!exists)throw new IllegalArgumentException("Unknown folder");
            }
            store.moveTrack(id,folder);
        }else if(action.equals("metadata")){
            String name=body.optString("name",track.name).trim();
            boolean visible=body.optBoolean("visible",track.visible);
            int color=body.optInt("color",track.color);
            if(name.isEmpty()||name.length()>80||!validColor(color))
                throw new IllegalArgumentException("Invalid track metadata");
            store.metadata(id,name,color,visible);
        }else if(action.equals("delete")){
            if(!track.state.equals("saved"))throw new IllegalArgumentException("Finish the track before deleting");
            store.delete(id);
        }else throw new IllegalArgumentException("Unknown track action");
        return obj("ok",true);
    }
    private static boolean validColor(int color){
        for(int allowed:TrackStore.COLORS)if(color==allowed)return true;
        return false;
    }
    private JSONObject trackPreview(String id)throws Exception{
        TrackStore store=onMain(this::trackStore);
        CompletableFuture<List<FieldTools.Fix>> promise=new CompletableFuture<>();
        store.preview(id,promise::complete);
        List<FieldTools.Fix> fixes=promise.get(8,TimeUnit.SECONDS);
        JSONArray points=new JSONArray();
        for(FieldTools.Fix fix:fixes)
            points.put(new JSONArray().put(fix.lat).put(fix.lon).put(fix.segment));
        return obj("points",points);
    }
    private Response trackExport(String id)throws Exception{
        TrackStore store=onMain(this::trackStore);
        boolean exists=onMain(()->{
            for(TrackStore.Track track:store.tracks)if(track.id.equals(id))return true;
            return false;
        });
        if(!exists)return error(Response.Status.NOT_FOUND,"Track not found");
        PipedInputStream input=new PipedInputStream(32768);
        PipedOutputStream output=new PipedOutputStream(input);
        store.export(id,output,ignored->{});
        Response response=newChunkedResponse(Response.Status.OK,"application/gpx+xml",input);
        response.addHeader("Content-Disposition","attachment; filename=\"MeshCrossway-track.gpx\"");
        response.addHeader("Cache-Control","no-store");
        response.addHeader("X-Content-Type-Options","nosniff");
        return response;
    }
    @Override public void stop(){
        sessions.clear();super.stop();
    }
}
