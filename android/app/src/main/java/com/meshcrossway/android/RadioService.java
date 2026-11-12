package com.meshcrossway.android;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.*;
import android.bluetooth.*;
import android.bluetooth.le.*;
import android.content.*;
import android.content.pm.PackageManager;
import android.os.*;
import java.security.SecureRandom;
import java.util.*;

/** All mutable state is confined to the main looper; each radio owns its GATT queue. */
@SuppressLint("MissingPermission")
public final class RadioService extends Service {
    final SupportReport supportLog=new SupportReport();
    public final class LocalBinder extends Binder { public RadioService service(){return RadioService.this;} }
    public final class Session {
        public final RadioProfile profile;
        public String address="", name="", status="Disconnected";
        public long frames;
        public boolean connected, verified;
        /** Latest phone-to-radio BLE RSSI; unrelated to the received LoRa signal. */
        public int rssiDbm=BleSignal.UNKNOWN;
        public long rssiMeasuredAtMs;
        public boolean rssiCurrentLink;
        private boolean rssiPending;
        private Runnable rssiPoll;
        private BluetoothGatt gatt;
        private BluetoothGattCharacteristic tx, rx, notice;
        private final ArrayDeque<Op> queue=new ArrayDeque<>();
        private Op active;
        ProtocolClient client;MtAdmin.Edit pendingMtEdit;String pendingMtAddress="";McAdmin.Edit pendingMcEdit;String pendingMcAddress="";final List<ProtocolClient.Sent> pendingMcDeliveries=new ArrayList<>();
        boolean desired, transportReady, persisting;
        int retries, mtu=23;
        String phase="Idle",pendingAddress;boolean forcePair;long phaseAt;
        final ArrayDeque<String> events=new ArrayDeque<>();
        long nextWriteAt;
        Runnable retry;
        private boolean readPending;
        private Runnable timeout, probeTimeout, poll;
        Session(RadioProfile p){profile=p;}
    }
    private static final class Op {
        final int kind; final byte[] data;
        Op(int kind,byte[] data){this.kind=kind;this.data=data;}
    }
    public final EnumMap<RadioProfile,Session> sessions=new EnumMap<>(RadioProfile.class);
    final ConnectionGate gate=new ConnectionGate();
    final Map<String,Integer> signal=new HashMap<>();final Map<String,RadioProfile> hints=new HashMap<>();
    RadioProfile scanTarget;boolean showOtherDevices;
    public final LinkedHashMap<String,BluetoothDevice> devices=new LinkedHashMap<>();
    HistoryStore history;private MessageAlerts alerts;boolean uiVisible;
    boolean running, autoReconnect=true;
    public boolean scanning;
    public String scanStatus="";
    public Runnable listener;
    private final Handler handler=new Handler(Looper.getMainLooper());
    RemoteWorkspace remoteWorkspace;
    TrackService remoteTracks;
    private boolean remoteTrackBound;
    private final ServiceConnection remoteTrackConnection=new ServiceConnection(){
        @Override public void onServiceConnected(ComponentName name,IBinder binder){
            remoteTracks=((TrackService.LocalBinder)binder).service();
            changed();
        }
        @Override public void onServiceDisconnected(ComponentName name){remoteTracks=null;changed();}
    };
    String remoteUrl(){return remoteWorkspace==null?"":remoteWorkspace.url();}
    String remoteCode(){return remoteWorkspace==null?"":remoteWorkspace.accessPin();}
    String remoteFingerprint(){return remoteWorkspace==null?"":remoteWorkspace.fingerprint;}
    String remoteError="";
    private final Runnable remoteNetworkWatch=new Runnable(){
        @Override public void run(){
            if(remoteWorkspace==null)return;
            String current=RemoteWorkspace.wifiAddress(RadioService.this);
            if(!remoteWorkspace.ip.equals(current)){
                stopRemote();remoteError="Wi-Fi network changed • activate Remote Access again";changed();
            }else handler.postDelayed(this,10000);
        }
    };
    boolean startRemote(String pin){
        if(remoteWorkspace!=null)return true;
        if(!RemoteWorkspace.validPin(pin)){
            remoteError="Remote Access PIN must contain 4–8 digits";
            changed();return false;
        }
        try{
            remoteWorkspace=RemoteWorkspace.start(this,pin);
            remoteTrackBound=bindService(new Intent(this,TrackService.class),remoteTrackConnection,BIND_AUTO_CREATE);
            remoteError="";
            handler.removeCallbacks(remoteNetworkWatch);
            handler.postDelayed(remoteNetworkWatch,10000);
            changed();return true;
        }catch(Exception e){
            remoteError="Local HTTPS server unavailable • check Wi-Fi, port 8080 and certificate setup";
            changed();return false;
        }
    }
    boolean changeRemotePin(String pin){
        if(remoteWorkspace==null||!RemoteWorkspace.validPin(pin))return false;
        remoteWorkspace.changePin(pin);changed();return true;
    }
    void stopRemote(){
        handler.removeCallbacks(remoteNetworkWatch);
        if(remoteWorkspace!=null){remoteWorkspace.stop();remoteWorkspace=null;MapEngine.pauseDownloads();}
        if(remoteTrackBound){unbindService(remoteTrackConnection);remoteTrackBound=false;}
        remoteTracks=null;remoteError="";changed();
    }

    private BluetoothAdapter adapter;
    private BluetoothLeScanner scanner;
    private final Runnable stopScanTask=this::stopScan;
    private final Runnable tick=new Runnable(){public void run(){if(!running)return;for(Session s:sessions.values())advance(s);handler.postDelayed(this,500);}};
    private long now(){return SystemClock.elapsedRealtime();}
    MeshData.Snapshot data(RadioProfile p){return history.snapshots.get(p);}
    void setAutoReconnect(boolean enabled){autoReconnect=enabled;getSharedPreferences("radios",MODE_PRIVATE).edit().putBoolean("autoReconnect",enabled).apply();for(Session s:sessions.values()){if(!enabled&&s.retry!=null){handler.removeCallbacks(s.retry);s.retry=null;}if(enabled&&s.desired&&s.gatt==null&&s.retry==null)scheduleReconnect(s);}}
    void reloadSavedSelection(){for(Session s:sessions.values())if(!s.connected&&!s.desired){s.address=getSharedPreferences("radios",MODE_PRIVATE).getString(s.profile.name(),s.address);history.select(s.profile,s.address);}}
    boolean shouldResume(){for(Session s:sessions.values())if(s.desired&&!s.address.isEmpty())return autoReconnect;return false;}

    void phase(Session s,String phase,String detail){supportLog.record(s.profile,SupportReport.Event.PHASE,SupportReport.phase(phase),now());s.phase=phase;s.phaseAt=now();s.status=detail;s.events.addLast((now()/1000)+"s · "+phase+" · "+detail);while(s.events.size()>12)s.events.removeFirst();changed();}
    private void releaseSetup(Session s){RadioProfile next=gate.release(s.profile);if(next!=null)handler.postDelayed(()->{Session waiting=sessions.get(next);if(running&&waiting.desired&&"Queued".equals(waiting.phase)&&waiting.pendingAddress!=null)open(waiting,waiting.pendingAddress);},350);}
    void connectSaved(){for(Session s:sessions.values())if(!s.address.isEmpty()&&ReconnectPolicy.canStartSaved(s.connected,s.gatt!=null,s.retry!=null)){s.desired=true;s.retries=0;open(s,s.address);}}
    void pair(RadioProfile profile){Session s=sessions.get(profile);if(s.address.isEmpty())return;s.forcePair=true;connect(profile,s.address);}
    java.util.List<BluetoothDevice> discovered(RadioProfile profile){if(!allowed())return Collections.emptyList();ArrayList<BluetoothDevice> result=new ArrayList<>();String selected=sessions.get(profile).address;for(BluetoothDevice d:devices.values()){RadioProfile hint=hints.get(d.getAddress());if(showOtherDevices||hint==profile||d.getAddress().equals(selected)||(hint==null&&d.getBondState()==BluetoothDevice.BOND_BONDED))result.add(d);}result.sort((a,b)->{String aa=a.getAddress(),bb=b.getAddress();int ar=aa.equals(selected)?3:hints.get(aa)==profile?2:a.getBondState()==BluetoothDevice.BOND_BONDED?1:0,br=bb.equals(selected)?3:hints.get(bb)==profile?2:b.getBondState()==BluetoothDevice.BOND_BONDED?1:0;return ar!=br?Integer.compare(br,ar):Integer.compare(signal.getOrDefault(bb,-127),signal.getOrDefault(aa,-127));});return result;}
    String discoveryLabel(BluetoothDevice d){String address=d.getAddress();RadioProfile hint=hints.get(address);return deviceName(d)+" · "+(hint==null?"unverified service":hint.title)+"\n"+signal.getOrDefault(address,-127)+" dBm"+(d.getBondState()==BluetoothDevice.BOND_BONDED?" · paired":"");}
    String diagnostics(RadioProfile p){Session s=sessions.get(p);return p.title+" · "+s.phase+"\nMTU "+s.mtu+" · "+s.frames+" frames\n"+String.join("\n",s.events);}
    String supportReport(){
        List<SupportReport.Session> state=new ArrayList<>();for(Session s:sessions.values()){
            SupportReport.Session item=new SupportReport.Session();item.profile=s.profile;item.phase=SupportReport.phase(s.phase);item.mtu=s.mtu;item.retries=s.retries;item.frames=s.frames;item.connected=s.connected;item.ready=s.verified;item.queued=s.queue.size()+(s.active==null?0:1);
            if(s.client!=null){ProtocolClient c=s.client;item.protocolQueued=c.commands.size()+(c.active==null?0:1);item.firmware=c.firmware;item.protocol=s.profile==RadioProfile.MESHCORE?c.firmwareCode:-1;item.pending=s.profile==RadioProfile.MESHCORE?c.mcAdmin.edit!=null||!c.mcUnverifiedChannels.isEmpty():c.admin.edit!=null;item.approved=c.nodes.size();item.discovered=c.mcDiscoveries.size();}else item.pending=s.pendingMcEdit!=null||s.pendingMtEdit!=null;state.add(item);
        }
        String version="unknown";try{version=getPackageManager().getPackageInfo(getPackageName(),0).versionName;}catch(PackageManager.NameNotFoundException ignored){}
        return supportLog.render(version,Build.VERSION.SDK_INT,Build.MANUFACTURER+" "+Build.MODEL,state);
    }
    private final BroadcastReceiver adapterChanges=new BroadcastReceiver(){
        @Override public void onReceive(Context c,Intent i){
            if(BluetoothDevice.ACTION_BOND_STATE_CHANGED.equals(i.getAction())){
                BluetoothDevice device=i.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);if(device==null)return;
                for(Session s:sessions.values())if(s.desired&&"Pairing".equals(s.phase)&&device.getAddress().equals(s.address)){
                    int state=i.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE,BluetoothDevice.BOND_NONE);
                    if(state==BluetoothDevice.BOND_BONDED){cancelTimeout(s);s.forcePair=false;startGatt(s,device);}
                    else if(state==BluetoothDevice.BOND_NONE){s.desired=false;getSharedPreferences("radios",MODE_PRIVATE).edit().putBoolean(s.profile.name()+".desired",false).apply();close(s,"Pairing cancelled or rejected • pair in Android settings and reconnect");}
                }return;
            }
            if(i.getIntExtra(BluetoothAdapter.EXTRA_STATE,-1)!=BluetoothAdapter.STATE_ON){
                stopScan(); for(Session s:sessions.values()) close(s,"Bluetooth is off • waiting for Bluetooth");
            }else if(running&&autoReconnect){for(Session s:sessions.values())if(s.desired&&s.gatt==null&&s.retry==null){s.retries=0;scheduleReconnect(s);}}
        }
    };
    @Override public void onCreate(){
        super.onCreate();
        history=new HistoryStore(this);history.listener=this::changed;alerts=new MessageAlerts(this);
        autoReconnect=getSharedPreferences("radios",MODE_PRIVATE).getBoolean("autoReconnect",true);
        for(RadioProfile p:RadioProfile.values()){
            Session s=new Session(p);
            SharedPreferences radios=getSharedPreferences("radios",MODE_PRIVATE);
            s.address=radios.getString(p.name(),"");
            s.desired=radios.getBoolean(p.name()+".desired",false);
            // A previous reading belongs only to this same radio MAC address.
            if(s.address.equals(radios.getString(p.name()+".rssi.address",""))){
                int previous=radios.getInt(p.name()+".rssi.dbm",BleSignal.UNKNOWN);
                long measured=radios.getLong(p.name()+".rssi.at",0);
                if(BleSignal.valid(previous)&&BleSignal.recent(measured,System.currentTimeMillis())){
                    s.rssiDbm=previous;s.rssiMeasuredAtMs=measured;
                }
            }
            sessions.put(p,s);history.select(p,s.address);history.uncertain(p,s.address);
        }
        BluetoothManager manager=getSystemService(BluetoothManager.class);adapter=manager==null?null:manager.getAdapter();
        IntentFilter events=new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED);events.addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED);registerReceiver(adapterChanges,events);
        getSystemService(NotificationManager.class).createNotificationChannel(new NotificationChannel("radios","Radio connections",NotificationManager.IMPORTANCE_LOW));
    }
    @Override public int onStartCommand(Intent intent,int flags,int id){
        if(intent!=null && "STOP".equals(intent.getAction())){
            stopRemote();
            running=false;handler.removeCallbacks(tick);stopScan();for(RadioProfile p:RadioProfile.values())disconnect(p);stopForeground(STOP_FOREGROUND_REMOVE);stopSelf();return START_NOT_STICKY;
        }
        if(!allowed()){stopSelf();return START_NOT_STICKY;}
        Intent open=new Intent(this,MainActivity.class);
        PendingIntent content=PendingIntent.getActivity(this,0,open,PendingIntent.FLAG_IMMUTABLE|PendingIntent.FLAG_UPDATE_CURRENT);
        PendingIntent stop=PendingIntent.getService(this,1,new Intent(this,RadioService.class).setAction("STOP"),PendingIntent.FLAG_IMMUTABLE|PendingIntent.FLAG_UPDATE_CURRENT);
        Notification notification=new Notification.Builder(this,"radios").setSmallIcon(R.drawable.ic_launcher)
            .setContentTitle("MeshCrossway radio controller").setContentText("Two independent radios • tap to view connections")
            .setContentIntent(content).addAction(new Notification.Action.Builder(null,"Disconnect all",stop).build()).setOngoing(true).build();
        startForeground(1,notification);
        if(!running){running=true;handler.post(tick);if(autoReconnect)for(Session s:sessions.values())if(s.desired&&s.gatt==null&&!s.address.isEmpty())open(s,s.address);}
        return START_NOT_STICKY;
    }
    @Override public IBinder onBind(Intent intent){return new LocalBinder();}
    public boolean allowed(){return Build.VERSION.SDK_INT>=31 ? checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN)==PackageManager.PERMISSION_GRANTED && checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT)==PackageManager.PERMISSION_GRANTED : checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)==PackageManager.PERMISSION_GRANTED;}
    private void changed(){if(listener!=null)listener.run();}
    public void scan(){scan(scanTarget);}
    public void scan(RadioProfile target){
        stopScan();scanTarget=target;showOtherDevices=false;devices.clear();signal.clear();hints.clear();
        if(!allowed() || adapter==null || !adapter.isEnabled()){scanStatus="Enable Bluetooth and grant Nearby devices permission";changed();return;}
        if(Build.VERSION.SDK_INT<=30 && !locationEnabled()){scanStatus="Enable phone Location to discover BLE devices on Android 8–11";changed();return;}
        try{
            scanner=adapter.getBluetoothLeScanner();if(scanner==null){scanStatus="BLE scanner unavailable";changed();return;}
            scanning=true;scanStatus="Scanning for 12 seconds…";
            scanner.startScan(null,new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(),scanCallback);
            handler.postDelayed(stopScanTask,12000);
        }catch(SecurityException|IllegalStateException e){scanning=false;scanStatus="Scan unavailable • check Bluetooth permissions";}
        changed();
    }
    private boolean locationEnabled(){
        android.location.LocationManager lm=getSystemService(android.location.LocationManager.class);
        if(lm==null)return false;
        if(Build.VERSION.SDK_INT>=28)return lm.isLocationEnabled();
        return lm.isProviderEnabled(android.location.LocationManager.GPS_PROVIDER) || lm.isProviderEnabled(android.location.LocationManager.NETWORK_PROVIDER);
    }
    private final ScanCallback scanCallback=new ScanCallback(){
        @Override public void onScanResult(int type,ScanResult result){handler.post(()->{
            if(!scanning)return;
            BluetoothDevice device=result.getDevice();String addr=device.getAddress();
            if(devices.size()<100 || devices.containsKey(addr)){devices.put(addr,device);signal.put(addr,result.getRssi());ScanRecord record=result.getScanRecord();if(record!=null&&record.getServiceUuids()!=null)for(ParcelUuid uuid:record.getServiceUuids())for(RadioProfile p:RadioProfile.values())if(p.service.equals(uuid.getUuid()))hints.put(addr,p);}changed();
        });}
        @Override public void onScanFailed(int error){handler.post(()->{stopScan();scanStatus="Scan failed ("+error+") • try again";changed();});}
    };
    public void stopScan(){
        handler.removeCallbacks(stopScanTask);
        if(scanning && scanner!=null)try{scanner.stopScan(scanCallback);}catch(SecurityException|IllegalStateException ignored){}
        scanning=false;scanStatus=devices.isEmpty()?"No radios found • keep radios awake and close other mesh apps":"Select a radio below";changed();
    }
    public String deviceName(BluetoothDevice d){try{String name=d.getName();return name==null?"Unnamed BLE device":name;}catch(SecurityException e){return "Permission required";}}
    public void connect(RadioProfile profile,String address){
        Session s=sessions.get(profile);s.retries=0;s.desired=true;open(s,address);
    }
    private void open(Session s,String address){
        if(!allowed() || adapter==null || !adapter.isEnabled()){s.status="Enable Bluetooth and grant Nearby devices permission";changed();return;}
        for(Session other:sessions.values())if(other!=s && (other.gatt!=null||other.desired) && address.equals(other.address)){s.status="This radio is already assigned to "+other.profile.title;s.desired=false;changed();return;}
        s.pendingAddress=address;
        if(!gate.acquire(s.profile)){phase(s,"Queued","Waiting for "+gate.owner().title+" Bluetooth setup");return;}
        stopScan();release(s,"Connecting…");s.mtu=23;
        if(!address.equals(s.address)){
            s.rssiDbm=BleSignal.UNKNOWN;s.rssiMeasuredAtMs=0;
        }
        s.address=address;s.frames=0;history.select(s.profile,address);
        long nonce=(new SecureRandom().nextInt() & 0x7fffffffL)+1;if(nonce==69420||nonce==69421)nonce+=2;
        final String radio=address;final RadioProfile profile=s.profile;
        s.client=new ProtocolClient(profile,nonce,new ProtocolClient.Listener(){
            public void node(MeshData.Node n){history.node(profile,radio,n);}
            public void channel(MeshData.Channel c){history.channel(profile,radio,c);}
            public void message(MeshData.Message m){history.message(profile,radio,m,muted->{if(!muted&&!m.outgoing&&!uiVisible&&s.address.equals(radio)&&System.currentTimeMillis()/1000-m.timestamp<120)alerts.show(profile,radio,m);});}
            public void delivery(String uid,String state){history.delivery(profile,radio,uid,state);}
        });
        if(profile==RadioProfile.MESHCORE){if(s.pendingMcAddress.equals(address)){if(s.pendingMcEdit!=null)s.client.mcAdmin.restore(s.pendingMcEdit);s.client.mcRestoreDeliveries(s.pendingMcDeliveries,now());}else{s.pendingMcEdit=null;s.pendingMcDeliveries.clear();}s.pendingMcAddress=address;}
        if(profile==RadioProfile.MESHTASTIC&&s.pendingMtEdit!=null&&s.pendingMtAddress.equals(address))s.client.admin.restore(s.pendingMtEdit);else{s.pendingMtEdit=null;s.pendingMtAddress="";}
        try{
            BluetoothDevice device=adapter.getRemoteDevice(address);s.name=deviceName(device);
            if((s.profile==RadioProfile.MESHTASTIC||s.forcePair)&&device.getBondState()!=BluetoothDevice.BOND_BONDED){
                phase(s,"Pairing","Confirm the Android pairing dialog and enter the PIN shown by your radio");
                arm(s,"Pairing timed out • open Android Bluetooth settings",90000);
                if(device.getBondState()!=BluetoothDevice.BOND_BONDING&&!device.createBond())close(s,"Pairing could not start • open Android Bluetooth settings");
            }else startGatt(s,device);
        }catch(SecurityException|IllegalArgumentException|IllegalStateException e){close(s,"Cannot connect • check address and permissions");}
        changed();
    }
    private void startGatt(Session s,BluetoothDevice device){
        if(!s.desired||gate.owner()!=s.profile)return;
        try{phase(s,"Connecting","Connecting to "+s.profile.title+" radio…");s.gatt=device.connectGatt(this,false,callback(s),BluetoothDevice.TRANSPORT_LE);if(s.gatt==null)close(s,"Connection could not start");else arm(s,"Connection timed out • check radio power, range and other mesh apps");}
        catch(SecurityException|IllegalStateException e){close(s,"Bluetooth access failed • check permissions");}
    }
    public void disconnect(RadioProfile profile){Session s=sessions.get(profile);s.desired=false;getSharedPreferences("radios",MODE_PRIVATE).edit().putBoolean(profile.name()+".desired",false).apply();release(s,"Disconnected by you");s.pendingAddress=null;releaseSetup(s);}
    private void scheduleReconnect(Session s){scheduleReconnect(s,0);}
    private void scheduleReconnect(Session s,int failureStatus){
        if(!running||!autoReconnect||!s.desired||!allowed()||adapter==null||!adapter.isEnabled()||s.address.isEmpty()||s.retry!=null)return;
        if(s.retries>=ReconnectPolicy.MAX_RETRIES){s.status+=" • automatic retries paused; tap Reconnect";changed();return;}
        long delay=ReconnectPolicy.delayMs(s.retries++,failureStatus);
        s.status+=" • retry in "+delay/1000+" s";
        // Exactly one scheduled retry per radio. A stale callback cannot
        // enqueue an overlapping GATT attempt while one is already active.
        s.retry=new Runnable(){public void run(){
            if(s.retry!=this)return;
            s.retry=null;
            if(running&&s.desired&&s.gatt==null)open(s,s.address);
        }};
        handler.postDelayed(s.retry,delay);changed();
    }
    void send(RadioProfile profile,String radio,MeshData.Target target,String text,java.util.function.Consumer<String> result){
        Session s=sessions.get(profile);if(!s.address.equals(radio)||!s.connected||!s.verified||s.client==null){result.accept("This conversation's radio is offline • draft kept");return;}
        if(!history.error.isEmpty()){result.accept(history.error);return;}if(s.persisting){result.accept("Saving previous message • please wait");return;}
        final ProtocolClient client=s.client;final MeshData.Message message;
        try{long id;do{id=new SecureRandom().nextInt()&0xffffffffL;}while(id==0||client.sent.containsKey(id));message=client.send(target,text,id,now());}
        catch(IllegalArgumentException e){result.accept(e.getMessage());return;}
        s.persisting=true;
        history.outgoing(profile,radio,message,()->{
            if(s.client!=client||!s.connected){history.delivery(profile,radio,message.uid,"uncertain");result.accept("Radio disconnected while saving • message was not retried");return;}
            s.persisting=false;history.draft(profile,radio,target.topic,"");result.accept("");advance(s);
        },()->{client.cancel(message.uid);if(s.client==client)s.persisting=false;result.accept("Could not save message • transmission cancelled; draft kept");});
    }
    void configureMeshCore(String radio,java.util.function.Consumer<ProtocolClient> action){
        Session s=sessions.get(RadioProfile.MESHCORE);
        if(s==null||!s.address.equals(radio)||!s.connected||!s.verified||s.client==null){
            if(listener!=null)listener.run();return;
        }
        try{s.client.mcAdmin.clock=now();action.accept(s.client);advance(s);changed();}
        catch(IllegalArgumentException e){phase(s,"Settings",e.getMessage());}
    }
    void configureMeshtastic(String radio,java.util.function.Consumer<ProtocolClient> action){
        Session s=sessions.get(RadioProfile.MESHTASTIC);
        if(s==null||!s.address.equals(radio)||!s.connected||!s.verified||s.client==null){if(listener!=null)listener.run();return;}
        try{action.accept(s.client);advance(s);changed();}catch(IllegalArgumentException e){s.client.admin.error=e.getMessage();s.client.admin.revision++;phase(s,"Settings",e.getMessage());changed();}
    }
    private void advance(Session s){
        if(!running||!s.transportReady||s.gatt==null||s.client==null||s.persisting||s.rssiPending||s.active!=null||!s.queue.isEmpty()||now()<s.nextWriteAt)return;
        try{long revision=s.client.admin.revision;ProtocolClient.Command command=s.client.next(now());if(s.client.admin.revision!=revision)changed();if(command!=null)enqueue(s,2,command.data);}
        catch(IllegalArgumentException e){close(s,e.getMessage());}
    }
    private void arm(Session s,String reason){arm(s,reason,30000);}
    private void arm(Session s,String reason,long delay){
        if(s.timeout!=null)handler.removeCallbacks(s.timeout);
        s.timeout=()->close(s,reason);handler.postDelayed(s.timeout,delay);
    }
    private void cancelTimeout(Session s){if(s.timeout!=null)handler.removeCallbacks(s.timeout);s.timeout=null;}
    private void close(Session s,String reason){close(s,reason,0);}
    private void close(Session s,String reason,int failureStatus){release(s,reason);releaseSetup(s);scheduleReconnect(s,failureStatus);}
    private void release(Session s,String reason){
        if(s.retry!=null)handler.removeCallbacks(s.retry);s.retry=null;
        if(s.rssiPoll!=null)handler.removeCallbacks(s.rssiPoll);
        s.rssiPoll=null;s.rssiPending=false;s.rssiCurrentLink=false;
        if(s.client!=null){history.uncertain(s.profile,s.address);if(s.profile==RadioProfile.MESHTASTIC){s.pendingMtEdit=s.client.admin.edit!=null&&s.client.admin.edit.writeId!=0?s.client.admin.edit:null;s.pendingMtAddress=s.address;}else{s.pendingMcEdit=s.client.pendingMcSave();s.pendingMcAddress=s.address;s.pendingMcDeliveries.clear();s.pendingMcDeliveries.addAll(s.client.mcRetainDeliveries(now()));}}s.client=null;s.transportReady=false;s.persisting=false;
        cancelTimeout(s);if(s.probeTimeout!=null)handler.removeCallbacks(s.probeTimeout);if(s.poll!=null)handler.removeCallbacks(s.poll);
        BluetoothGatt old=s.gatt;boolean wasConnected=s.connected;s.gatt=null;s.queue.clear();s.active=null;s.connected=s.verified=s.readPending=false;
        s.tx=s.rx=s.notice=null;phase(s,"Disconnected",reason);
        // A failed pre-connection GATT (e.g. status 133) is already offline:
        // close its client, do not call disconnect() on a never-opened link.
        if(old!=null)try{if(wasConnected)old.disconnect();old.close();}catch(SecurityException|IllegalStateException ignored){}
        changed();
    }
    private void enqueue(Session s,int kind,byte[] data){s.queue.add(new Op(kind,data));pump(s);}
    private void pump(Session s){
        if(s.gatt==null || s.rssiPending || s.active!=null || s.queue.isEmpty())return;
        s.active=s.queue.remove();boolean ok;
        try{
            if(s.active.kind==0)ok=s.gatt.requestMtu(517);
            else if(s.active.kind==1){
                BluetoothGattDescriptor cccd=s.notice.getDescriptor(RadioProfile.CCCD);
                if(cccd==null || !s.gatt.setCharacteristicNotification(s.notice,true)){close(s,"Required notification descriptor missing");return;}
                cccd.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);ok=s.gatt.writeDescriptor(cccd);
            }else if(s.active.kind==2){if(s.active.data.length>s.profile.payloadLimit(s.mtu)){close(s,"Command exceeds negotiated Bluetooth frame size • not sent");return;}s.tx.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);s.tx.setValue(s.active.data);ok=s.gatt.writeCharacteristic(s.tx);}
            else ok=s.gatt.readCharacteristic(s.rx);
        }catch(SecurityException|IllegalStateException e){ok=false;}
        if(!ok){supportLog.record(s.profile,SupportReport.Event.TRANSPORT_ERROR,-1,now());close(s,"Bluetooth operation failed • reconnect");return;}
        if(s.active.kind==2&&s.client!=null){s.client.writeAccepted(s.active.data);supportLog.record(s.profile,SupportReport.Event.WRITE_ACCEPTED,s.profile==RadioProfile.MESHCORE?s.active.data[0]&255:0,now());}
        arm(s,"Bluetooth operation timed out • reconnect");
    }
    private void done(Session s){cancelTimeout(s);s.active=null;pump(s);handler.postDelayed(()->advance(s),90);}
    private void read(Session s){if(s.gatt!=null && s.transportReady && !s.readPending){s.readPending=true;enqueue(s,3,null);}}
    private void beginProbe(Session s){
        s.transportReady=true;s.pendingAddress=null;phase(s,"Synchronizing","BLE ready • synchronizing "+s.profile.title);releaseSetup(s);
        s.probeTimeout=()->{if(!s.verified)close(s,"Radio synchronization timed out • reconnect");};handler.postDelayed(s.probeTimeout,180000);
        advance(s);
        if(s.profile==RadioProfile.MESHTASTIC){s.poll=new Runnable(){public void run(){if(s.gatt!=null){read(s);handler.postDelayed(this,3000);}}};handler.postDelayed(s.poll,100);}
    }
    private void received(Session s,byte[] data){
        if(data.length==0||s.client==null)return;s.frames++;supportLog.record(s.profile,SupportReport.Event.RX_CODE,s.profile==RadioProfile.MESHCORE?data[0]&255:0,now());
        try{
            boolean wasReady=s.verified;s.client.receive(data,now());s.verified=s.client.ready;s.status=s.client.description;if(s.profile==RadioProfile.MESHCORE&&s.client.mcAdmin.edit==null)s.pendingMcEdit=null;if(s.verified&&!wasReady){phase(s,"Ready",s.status);startSignalMonitoring(s);}
            if(s.verified){handler.removeCallbacks(s.probeTimeout);s.retries=0;getSharedPreferences("radios",MODE_PRIVATE).edit().putString(s.profile.name(),s.address).putBoolean(s.profile.name()+".desired",s.desired).apply();}
        }catch(IllegalArgumentException e){close(s,e.getMessage());return;}changed();handler.postDelayed(()->advance(s),90);
    }
    /**
     * RSSI polling is independent for each GATT link. Only request a reading
     * when its normal protocol operation queue is idle. A failed RSSI read
     * must never disconnect a radio or mark a message as delivered.
     */
    private void startSignalMonitoring(Session s){
        if(s.rssiPoll!=null)handler.removeCallbacks(s.rssiPoll);
        s.rssiPoll=new Runnable(){
            @Override public void run(){
                if(!running||!s.verified||s.gatt==null)return;
                // Always schedule the next measurement, including while radio is busy.
                handler.postDelayed(this,20000);
                if(s.rssiPending||s.active!=null||!s.queue.isEmpty()||
                   s.readPending||s.persisting||now()<s.nextWriteAt)return;
                final BluetoothGatt current=s.gatt;
                try{
                    if(!current.readRemoteRssi())return;
                    s.rssiPending=true;
                    handler.postDelayed(()->{
                        if(s.gatt==current&&s.rssiPending){
                            s.rssiPending=false;
                            pump(s);advance(s);
                        }
                    },3000);
                }catch(SecurityException|IllegalStateException ignored){
                    // RSSI is optional; message delivery takes priority.
                }
            }
        };
        handler.postDelayed(s.rssiPoll,2200);
    }
    private BluetoothGattCallback callback(Session s){return new BluetoothGattCallback(){
        private void onCurrent(BluetoothGatt g,Runnable action){handler.post(()->{if(g==s.gatt)action.run();});}
        @Override public void onReadRemoteRssi(BluetoothGatt g,int rssi,int status){
            onCurrent(g,()->{
                if(!s.rssiPending)return;
                s.rssiPending=false;
                if(status==BluetoothGatt.GATT_SUCCESS&&BleSignal.valid(rssi)){
                    s.rssiDbm=rssi;s.rssiMeasuredAtMs=System.currentTimeMillis();s.rssiCurrentLink=true;
                    // Persist only the last reading and the matching device address.
                    getSharedPreferences("radios",MODE_PRIVATE).edit()
                        .putString(s.profile.name()+".rssi.address",s.address)
                        .putInt(s.profile.name()+".rssi.dbm",rssi)
                        .putLong(s.profile.name()+".rssi.at",s.rssiMeasuredAtMs)
                        .apply();
                    changed();
                }
                pump(s);advance(s);
            });
        }
        @Override public void onConnectionStateChange(BluetoothGatt g,int status,int state){onCurrent(g,()->{
            supportLog.record(s.profile,SupportReport.Event.CONNECTION_STATE,state,now());if(status!=0)supportLog.record(s.profile,SupportReport.Event.TRANSPORT_ERROR,status,now());
            if(status!=BluetoothGatt.GATT_SUCCESS || state==BluetoothProfile.STATE_DISCONNECTED){close(s,ReconnectPolicy.failure(status),status);return;}
            if(state==BluetoothProfile.STATE_CONNECTED){s.connected=true;phase(s,"Discovering","Connected • discovering radio services");arm(s,"Service discovery timed out • check pairing and other mesh apps");handler.postDelayed(()->{if(s.gatt!=g)return;try{if(!g.discoverServices())close(s,"Service discovery could not start");}catch(SecurityException e){close(s,"Bluetooth permission revoked");}},350);}
        });}
        @Override public void onServicesDiscovered(BluetoothGatt g,int status){onCurrent(g,()->{
            cancelTimeout(s);BluetoothGattService service=g.getService(s.profile.service);
            if(status!=0 || service==null){close(s,"Selected radio does not expose "+s.profile.title+" BLE service");return;}
            s.tx=service.getCharacteristic(s.profile.write);s.rx=service.getCharacteristic(s.profile.read);s.notice=service.getCharacteristic(s.profile.notify);
            if(s.tx==null || s.rx==null || s.notice==null || (s.tx.getProperties()&BluetoothGattCharacteristic.PROPERTY_WRITE)==0 || (s.notice.getProperties()&BluetoothGattCharacteristic.PROPERTY_NOTIFY)==0 || (s.profile==RadioProfile.MESHTASTIC && (s.rx.getProperties()&BluetoothGattCharacteristic.PROPERTY_READ)==0)){close(s,"Required BLE characteristics missing");return;}
            phase(s,"MTU","Negotiating Bluetooth packet size…");enqueue(s,0,null);
        });}
        @Override public void onMtuChanged(BluetoothGatt g,int mtu,int status){onCurrent(g,()->{
            // Never allow a short-MTU write to silently truncate the protocol probe.
            if(s.active==null||s.active.kind!=0)return;
            int negotiated=status==0?mtu:23;s.mtu=negotiated;
            if(status!=0||!s.profile.supportsMtu(negotiated)){close(s,"Bluetooth MTU negotiation failed (status "+status+", MTU "+mtu+") • reconnect");return;}s.client.payloadLimit=s.profile.payloadLimit(negotiated);done(s);phase(s,"Subscribing","Enabling radio notifications…");enqueue(s,1,null);
        });}
        @Override public void onDescriptorWrite(BluetoothGatt g,BluetoothGattDescriptor d,int status){onCurrent(g,()->{
            if(status!=0){close(s,"Notification subscription failed ("+status+") • pair and reconnect");return;}done(s);beginProbe(s);
        });}
        @Override public void onCharacteristicWrite(BluetoothGatt g,BluetoothGattCharacteristic c,int status){onCurrent(g,()->{
            supportLog.record(s.profile,SupportReport.Event.WRITE_COMPLETE,status,now());
            if(status!=0){close(s,"Radio write failed ("+status+") • check system pairing");return;}try{if(s.client!=null)s.client.written(now());}catch(IllegalArgumentException e){close(s,e.getMessage());return;}s.nextWriteAt=now()+80;done(s);
        });}
        @Override public void onCharacteristicRead(BluetoothGatt g,BluetoothGattCharacteristic c,int status){byte[] value=c.getValue();byte[] copy=value==null?new byte[0]:value.clone();onCurrent(g,()->{
            if(status!=0){close(s,"Radio read failed ("+status+") • reconnect");return;}
            s.readPending=false;received(s,copy);done(s);if(copy.length>0)read(s);
        });}
        @Override public void onCharacteristicChanged(BluetoothGatt g,BluetoothGattCharacteristic c){byte[] value=c.getValue();byte[] copy=value==null?new byte[0]:value.clone();onCurrent(g,()->{
            if(s.profile==RadioProfile.MESHTASTIC)read(s);else received(s,copy);
        });}
    };}
    @Override public void onDestroy(){stopRemote();running=false;gate.clear();listener=null;stopScan();for(Session s:sessions.values())release(s,"Disconnected");handler.removeCallbacksAndMessages(null);history.close();unregisterReceiver(adapterChanges);super.onDestroy();}
}
