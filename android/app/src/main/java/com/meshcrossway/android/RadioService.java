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
    public final class LocalBinder extends Binder { public RadioService service(){return RadioService.this;} }
    public final class Session {
        public final RadioProfile profile;
        public String address="", name="", status="Disconnected";
        public long frames;
        public boolean connected, verified;
        private BluetoothGatt gatt;
        private BluetoothGattCharacteristic tx, rx, notice;
        private final ArrayDeque<Op> queue=new ArrayDeque<>();
        private Op active;
        private ProtocolProbe probe;
        private boolean readPending, appStartSent;
        private Runnable timeout, probeTimeout, poll;
        Session(RadioProfile p){profile=p;}
    }
    private static final class Op {
        final int kind; final byte[] data;
        Op(int kind,byte[] data){this.kind=kind;this.data=data;}
    }
    public final EnumMap<RadioProfile,Session> sessions=new EnumMap<>(RadioProfile.class);
    public final LinkedHashMap<String,BluetoothDevice> devices=new LinkedHashMap<>();
    public boolean scanning;
    public String scanStatus="";
    public Runnable listener;
    private final Handler handler=new Handler(Looper.getMainLooper());
    private BluetoothAdapter adapter;
    private BluetoothLeScanner scanner;
    private final Runnable stopScanTask=this::stopScan;
    private final BroadcastReceiver adapterChanges=new BroadcastReceiver(){
        @Override public void onReceive(Context c,Intent i){
            if(i.getIntExtra(BluetoothAdapter.EXTRA_STATE,-1)!=BluetoothAdapter.STATE_ON){
                stopScan(); for(Session s:sessions.values()) close(s,"Bluetooth is off • enable it and reconnect");
            }
        }
    };
    @Override public void onCreate(){
        super.onCreate();
        for(RadioProfile p:RadioProfile.values()){
            Session s=new Session(p);s.address=getSharedPreferences("radios",MODE_PRIVATE).getString(p.name(),"");sessions.put(p,s);
        }
        BluetoothManager manager=getSystemService(BluetoothManager.class);adapter=manager==null?null:manager.getAdapter();
        registerReceiver(adapterChanges,new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED));
        getSystemService(NotificationManager.class).createNotificationChannel(new NotificationChannel("radios","Radio connections",NotificationManager.IMPORTANCE_LOW));
    }
    @Override public int onStartCommand(Intent intent,int flags,int id){
        if(intent!=null && "STOP".equals(intent.getAction())){
            stopScan();for(Session s:sessions.values())close(s,"Disconnected");stopForeground(STOP_FOREGROUND_REMOVE);stopSelf();return START_NOT_STICKY;
        }
        if(!allowed()){stopSelf();return START_NOT_STICKY;}
        Intent open=new Intent(this,MainActivity.class);
        PendingIntent content=PendingIntent.getActivity(this,0,open,PendingIntent.FLAG_IMMUTABLE|PendingIntent.FLAG_UPDATE_CURRENT);
        PendingIntent stop=PendingIntent.getService(this,1,new Intent(this,RadioService.class).setAction("STOP"),PendingIntent.FLAG_IMMUTABLE|PendingIntent.FLAG_UPDATE_CURRENT);
        Notification notification=new Notification.Builder(this,"radios").setSmallIcon(R.drawable.ic_launcher)
            .setContentTitle("MeshCrossway radio controller").setContentText("Two independent radios • tap to view connections")
            .setContentIntent(content).addAction(new Notification.Action.Builder(null,"Disconnect all",stop).build()).setOngoing(true).build();
        startForeground(1,notification);return START_NOT_STICKY;
    }
    @Override public IBinder onBind(Intent intent){return new LocalBinder();}
    public boolean allowed(){return Build.VERSION.SDK_INT>=31 ? checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN)==PackageManager.PERMISSION_GRANTED && checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT)==PackageManager.PERMISSION_GRANTED : checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)==PackageManager.PERMISSION_GRANTED;}
    private void changed(){if(listener!=null)listener.run();}
    public void scan(){
        stopScan();devices.clear();
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
            if(devices.size()<80 || devices.containsKey(addr))devices.put(addr,device);changed();
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
        Session s=sessions.get(profile);
        if(!allowed() || adapter==null || !adapter.isEnabled()){s.status="Enable Bluetooth and grant Nearby devices permission";changed();return;}
        for(Session other:sessions.values())if(other!=s && other.gatt!=null && address.equals(other.address)){s.status="This radio is already assigned to "+other.profile.title;changed();return;}
        stopScan();close(s,"Connecting…");s.address=address;s.frames=0;long nonce=(new SecureRandom().nextInt() & 0x7fffffffL)+1;
        if(nonce==69420 || nonce==69421)nonce+=2;s.probe=new ProtocolProbe(nonce);
        try{
            BluetoothDevice device=adapter.getRemoteDevice(address);s.name=deviceName(device);
            s.gatt=device.connectGatt(this,false,callback(s),BluetoothDevice.TRANSPORT_LE);
            if(s.gatt==null)close(s,"Connection could not start");else arm(s,"Connection timed out • retry or check pairing");
        }catch(SecurityException|IllegalArgumentException|IllegalStateException e){close(s,"Cannot connect • check address and permissions");}
        changed();
    }
    public void disconnect(RadioProfile profile){close(sessions.get(profile),"Disconnected");}
    private void arm(Session s,String reason){
        if(s.timeout!=null)handler.removeCallbacks(s.timeout);
        s.timeout=()->close(s,reason);handler.postDelayed(s.timeout,20000);
    }
    private void cancelTimeout(Session s){if(s.timeout!=null)handler.removeCallbacks(s.timeout);s.timeout=null;}
    private void close(Session s,String reason){
        cancelTimeout(s);if(s.probeTimeout!=null)handler.removeCallbacks(s.probeTimeout);if(s.poll!=null)handler.removeCallbacks(s.poll);
        BluetoothGatt old=s.gatt;s.gatt=null;s.queue.clear();s.active=null;s.connected=s.verified=s.readPending=s.appStartSent=false;
        s.tx=s.rx=s.notice=null;s.status=reason;
        if(old!=null)try{old.disconnect();old.close();}catch(SecurityException|IllegalStateException ignored){}
        changed();
    }
    private void enqueue(Session s,int kind,byte[] data){s.queue.add(new Op(kind,data));pump(s);}
    private void pump(Session s){
        if(s.gatt==null || s.active!=null || s.queue.isEmpty())return;
        s.active=s.queue.remove();boolean ok;
        try{
            if(s.active.kind==0)ok=s.gatt.requestMtu(517);
            else if(s.active.kind==1){
                BluetoothGattDescriptor cccd=s.notice.getDescriptor(RadioProfile.CCCD);
                if(cccd==null || !s.gatt.setCharacteristicNotification(s.notice,true)){close(s,"Required notification descriptor missing");return;}
                cccd.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);ok=s.gatt.writeDescriptor(cccd);
            }else if(s.active.kind==2){s.tx.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);s.tx.setValue(s.active.data);ok=s.gatt.writeCharacteristic(s.tx);}
            else ok=s.gatt.readCharacteristic(s.rx);
        }catch(SecurityException|IllegalStateException e){ok=false;}
        if(!ok){close(s,"Bluetooth operation failed • reconnect");return;}arm(s,"Bluetooth operation timed out • reconnect");
    }
    private void done(Session s){cancelTimeout(s);s.active=null;pump(s);}
    private void read(Session s){if(s.gatt!=null && !s.verified && !s.readPending){s.readPending=true;enqueue(s,3,null);}}
    private void beginProbe(Session s){
        s.status="BLE subscribed • checking "+s.profile.title;changed();
        s.probeTimeout=()->{if(!s.verified)close(s,"Protocol probe timed out • verify firmware and reconnect");};handler.postDelayed(s.probeTimeout,45000);
        enqueue(s,2,s.profile==RadioProfile.MESHTASTIC?s.probe.meshtasticRequest():ProtocolProbe.meshcoreQuery());
        if(s.profile==RadioProfile.MESHTASTIC){s.poll=new Runnable(){public void run(){if(s.gatt!=null){read(s);handler.postDelayed(this,2000);}}};handler.post(s.poll);}
    }
    private void received(Session s,byte[] data){
        if(data.length==0)return;s.frames++;
        try{
            if(s.profile==RadioProfile.MESHTASTIC)s.probe.meshtastic(data);
            else if(s.probe.meshcore(data) && !s.appStartSent){s.appStartSent=true;enqueue(s,2,ProtocolProbe.meshcoreAppStart());}
            s.verified=s.probe.complete;s.status=s.probe.detail;
            if(s.verified){handler.removeCallbacks(s.probeTimeout);if(s.poll!=null)handler.removeCallbacks(s.poll);getSharedPreferences("radios",MODE_PRIVATE).edit().putString(s.profile.name(),s.address).apply();}
        }catch(IllegalArgumentException e){close(s,e.getMessage());return;}changed();
    }
    private BluetoothGattCallback callback(Session s){return new BluetoothGattCallback(){
        private void onCurrent(BluetoothGatt g,Runnable action){handler.post(()->{if(g==s.gatt)action.run();});}
        @Override public void onConnectionStateChange(BluetoothGatt g,int status,int state){onCurrent(g,()->{
            if(status!=BluetoothGatt.GATT_SUCCESS || state==BluetoothProfile.STATE_DISCONNECTED){close(s,"Disconnected (GATT "+status+") • tap reconnect");return;}
            if(state==BluetoothProfile.STATE_CONNECTED){s.connected=true;s.status="Connected • discovering services";changed();try{if(!g.discoverServices())close(s,"Service discovery could not start");else arm(s,"Service discovery timed out");}catch(SecurityException e){close(s,"Bluetooth permission revoked");}}
        });}
        @Override public void onServicesDiscovered(BluetoothGatt g,int status){onCurrent(g,()->{
            cancelTimeout(s);BluetoothGattService service=g.getService(s.profile.service);
            if(status!=0 || service==null){close(s,"Selected radio does not expose "+s.profile.title+" BLE service");return;}
            s.tx=service.getCharacteristic(s.profile.write);s.rx=service.getCharacteristic(s.profile.read);s.notice=service.getCharacteristic(s.profile.notify);
            if(s.tx==null || s.rx==null || s.notice==null || (s.tx.getProperties()&BluetoothGattCharacteristic.PROPERTY_WRITE)==0 || (s.notice.getProperties()&BluetoothGattCharacteristic.PROPERTY_NOTIFY)==0 || (s.profile==RadioProfile.MESHTASTIC && (s.rx.getProperties()&BluetoothGattCharacteristic.PROPERTY_READ)==0)){close(s,"Required BLE characteristics missing");return;}
            enqueue(s,0,null);
        });}
        @Override public void onMtuChanged(BluetoothGatt g,int mtu,int status){onCurrent(g,()->{
            // Never allow a short-MTU write to silently truncate the protocol probe.
            if(status!=0 || mtu<32){close(s,"MTU negotiation failed • reconnect");return;}done(s);enqueue(s,1,null);
        });}
        @Override public void onDescriptorWrite(BluetoothGatt g,BluetoothGattDescriptor d,int status){onCurrent(g,()->{
            if(status!=0){close(s,"Notification subscription failed ("+status+") • pair and reconnect");return;}done(s);beginProbe(s);
        });}
        @Override public void onCharacteristicWrite(BluetoothGatt g,BluetoothGattCharacteristic c,int status){onCurrent(g,()->{
            if(status!=0){close(s,"Radio write failed ("+status+") • check system pairing");return;}done(s);
        });}
        @Override public void onCharacteristicRead(BluetoothGatt g,BluetoothGattCharacteristic c,int status){byte[] value=c.getValue();byte[] copy=value==null?new byte[0]:value.clone();onCurrent(g,()->{
            if(status!=0){close(s,"Radio read failed ("+status+") • reconnect");return;}
            s.readPending=false;received(s,copy);done(s);if(copy.length>0)read(s);
        });}
        @Override public void onCharacteristicChanged(BluetoothGatt g,BluetoothGattCharacteristic c){byte[] value=c.getValue();byte[] copy=value==null?new byte[0]:value.clone();onCurrent(g,()->{
            if(s.profile==RadioProfile.MESHTASTIC)read(s);else received(s,copy);
        });}
    };}
    @Override public void onDestroy(){listener=null;stopScan();for(Session s:sessions.values())close(s,"Disconnected");handler.removeCallbacksAndMessages(null);unregisterReceiver(adapterChanges);super.onDestroy();}
}
