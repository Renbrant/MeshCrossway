package com.meshcrossway.android;

import android.Manifest;
import android.app.*;
import android.bluetooth.BluetoothDevice;
import android.content.*;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.*;
import android.provider.Settings;
import android.view.*;
import android.widget.*;
import java.util.ArrayList;

public final class MainActivity extends Activity {
    // Exact palette from firmware/display-prototype/meshcrossway_ui.h.
    private static final int BG=0xff090c10,PANEL=0xff181d24,PANEL2=0xff242b34,ORANGE=0xffff8a24,WHITE=0xfff2f4f7,MUTED=0xff9aa6b4,GREEN=0xff72d8a5;
    private final String[] pages={"Home","Nodes","Groups","Chats","Map","Settings"};
    private final String[] symbols={"⌂","⋮","☷","▤","◇","⚙"};
    private final Handler handler=new Handler(Looper.getMainLooper());
    private RadioService service;
    private LinearLayout root,body,nav;
    private int selected;
    private boolean bound,visible;
    private RadioProfile choosing;
    private Runnable pending;
    private ScrollView scroll;
    private final Runnable refresh=this::render;
    private final ServiceConnection connection=new ServiceConnection(){
        @Override public void onServiceConnected(ComponentName name,IBinder binder){service=((RadioService.LocalBinder)binder).service();if(visible)service.listener=MainActivity.this::scheduleRender;render();}
        @Override public void onServiceDisconnected(ComponentName name){service=null;render();}
    };
    @Override public void onCreate(Bundle state){
        super.onCreate(state);if(state!=null)selected=state.getInt("page",0);
        getWindow().setStatusBarColor(BG);getWindow().setNavigationBarColor(PANEL);
        root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setBackgroundColor(BG);
        root.setOnApplyWindowInsetsListener((view,insets)->{
            view.setPadding(insets.getSystemWindowInsetLeft(),insets.getSystemWindowInsetTop(),insets.getSystemWindowInsetRight(),insets.getSystemWindowInsetBottom());return insets.consumeSystemWindowInsets();
        });
        setContentView(root);root.requestApplyInsets();
        LinearLayout header=row();header.setPadding(dp(18),dp(12),dp(18),dp(12));
        ImageView logo=new ImageView(this);logo.setImageResource(R.drawable.meshcrossway_logo);logo.setContentDescription("MeshCrossway");logo.setAdjustViewBounds(true);logo.setScaleType(ImageView.ScaleType.FIT_START);
        header.addView(logo,new LinearLayout.LayoutParams(0,dp(32),1));TextView alpha=text("α 0.1.0",11,ORANGE);header.addView(alpha);root.addView(header);
        View divider=new View(this);divider.setBackgroundColor(PANEL2);root.addView(divider,new LinearLayout.LayoutParams(-1,dp(1)));
        body=new LinearLayout(this);body.setOrientation(LinearLayout.VERTICAL);root.addView(body,new LinearLayout.LayoutParams(-1,0,1));
        nav=row();nav.setBackgroundColor(PANEL);root.addView(nav,new LinearLayout.LayoutParams(-1,dp(64)));
        bound=bindService(new Intent(this,RadioService.class),connection,BIND_AUTO_CREATE);render();
    }
    @Override protected void onStart(){super.onStart();visible=true;if(service!=null)service.listener=this::scheduleRender;render();}
    @Override protected void onStop(){visible=false;if(service!=null)service.listener=null;handler.removeCallbacks(refresh);super.onStop();}
    @Override protected void onDestroy(){if(service!=null)service.listener=null;if(bound)unbindService(connection);handler.removeCallbacksAndMessages(null);super.onDestroy();}
    @Override protected void onSaveInstanceState(Bundle out){out.putInt("page",selected);super.onSaveInstanceState(out);}
    private void scheduleRender(){handler.removeCallbacks(refresh);handler.postDelayed(refresh,150);}
    private int dp(float value){return Math.round(value*getResources().getDisplayMetrics().density);}
    private TextView text(String value,int size,int color){TextView t=new TextView(this);t.setText(value);t.setTextSize(size);t.setTextColor(color);t.setGravity(Gravity.CENTER_VERTICAL);return t;}
    private LinearLayout column(){LinearLayout l=new LinearLayout(this);l.setOrientation(LinearLayout.VERTICAL);return l;}
    private LinearLayout row(){LinearLayout l=new LinearLayout(this);l.setOrientation(LinearLayout.HORIZONTAL);l.setGravity(Gravity.CENTER_VERTICAL);return l;}
    private GradientDrawable surface(int color){GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(dp(16));return d;}
    private void space(LinearLayout l,int height){l.addView(new View(this),new LinearLayout.LayoutParams(1,dp(height)));}
    private void title(LinearLayout l,String value,String description){TextView h=text(value,28,WHITE);h.setTypeface(null,Typeface.BOLD);l.addView(h);space(l,8);l.addView(text(description,14,MUTED));space(l,20);}
    private Button button(String label,Runnable action){Button b=new Button(this);b.setText(label);b.setTextSize(13);b.setAllCaps(false);b.setTextColor(ORANGE);b.setMinHeight(dp(48));b.setBackground(surface(PANEL2));b.setPadding(dp(12),dp(4),dp(12),dp(4));b.setOnClickListener(v->action.run());return b;}
    private void addButton(LinearLayout parent,String label,Runnable action){Button b=button(label,action);LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,dp(48));p.topMargin=dp(10);parent.addView(b,p);}
    private LinearLayout card(LinearLayout parent){LinearLayout c=column();c.setPadding(dp(18),dp(18),dp(18),dp(18));c.setBackground(surface(PANEL));LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,-2);p.bottomMargin=dp(14);parent.addView(c,p);return c;}
    private void page(int index){selected=index;choosing=null;scroll=null;render();}
    private void render(){
        if(body==null)return;
        int oldY=scroll==null?0:scroll.getScrollY();body.removeAllViews();nav.removeAllViews();
        for(int i=0;i<pages.length;i++){
            final int target=i;LinearLayout item=column();item.setGravity(Gravity.CENTER);item.setMinimumHeight(dp(56));
            TextView icon=text(symbols[i],23,i==selected?ORANGE:MUTED);icon.setGravity(Gravity.CENTER);item.addView(icon);
            TextView label=text(pages[i],10,i==selected?ORANGE:MUTED);label.setGravity(Gravity.CENTER);item.addView(label);
            item.setContentDescription(pages[i]);item.setFocusable(true);item.setOnClickListener(v->page(target));nav.addView(item,new LinearLayout.LayoutParams(0,-1,1));
        }
        if(selected==4){map();return;}
        scroll=new ScrollView(this);scroll.setFillViewport(true);LinearLayout content=column();content.setPadding(dp(18),dp(22),dp(18),dp(24));scroll.addView(content);body.addView(scroll,new LinearLayout.LayoutParams(-1,-1));
        if(selected==0)home(content);else if(selected==5)settings(content);else empty(content);
        scroll.post(()->{if(scroll!=null)scroll.scrollTo(0,oldY);});
    }
    private void home(LinearLayout content){
        title(content,"Two networks. One view.","Your independent off-grid radio connections.");
        for(RadioProfile p:RadioProfile.values()){
            RadioService.Session s=service==null?null:service.sessions.get(p);LinearLayout c=card(content);
            TextView name=text(p.title,21,WHITE);name.setTypeface(null,Typeface.BOLD);c.addView(name);space(c,10);
            c.addView(text(s==null?"Disconnected":s.verified?"● Protocol verified":s.connected?"● BLE connected":"○ Offline",14,s!=null&&s.verified?GREEN:ORANGE));space(c,8);
            c.addView(text(s==null?"Choose a compatible radio to begin.":s.status,13,MUTED));
            if(s!=null&&!s.name.isEmpty()){space(c,8);c.addView(text(s.name+" • "+s.frames+" frames received",12,MUTED));}
            addButton(c,"Manage "+p.title,()->{selected=5;choosing=p;scroll=null;render();});
        }
        LinearLayout note=card(content);note.addView(text("Connection test • alpha01",16,ORANGE));space(note,8);
        note.addView(text("This first APK tests two BLE links and protocol identification. Chat, nodes, phone GPS and maps are the next development stages.",14,MUTED));
    }
    private void settings(LinearLayout content){
        title(content,"Radio connections","Meshtastic and MeshCore remain independent.");
        if(choosing!=null){
            LinearLayout chooser=card(content);chooser.addView(text("Choose "+choosing.title+" radio",20,WHITE));space(chooser,8);
            chooser.addView(text("Close other mesh apps before connecting. Android handles Bluetooth PIN and pairing prompts.",13,MUTED));
            final RadioProfile target=choosing;
            addButton(chooser,service!=null&&service.scanning?"Stop scan":"Scan nearby radios",()->withPermission(()->{if(service.scanning)service.stopScan();else service.scan();}));
            space(chooser,10);chooser.addView(text(service==null?"Starting controller…":service.scanStatus,13,MUTED));
            if(service!=null)for(BluetoothDevice device:new ArrayList<>(service.devices.values())){
                String address=device.getAddress();addButton(chooser,service.deviceName(device)+"\n"+address,()->withPermission(()->{service.connect(target,address);choosing=null;render();}));
            }
            addButton(chooser,"Cancel selection",()->{if(service!=null)service.stopScan();choosing=null;render();});
        }
        for(RadioProfile p:RadioProfile.values()){
            RadioService.Session s=service==null?null:service.sessions.get(p);LinearLayout c=card(content);c.addView(text(p.title,20,WHITE));space(c,8);
            c.addView(text(s==null?"Controller starting…":s.status,13,MUTED));
            if(s!=null&&!s.address.isEmpty()){space(c,8);c.addView(text(s.address,12,MUTED));}
            addButton(c,"Choose radio",()->withPermission(()->{choosing=p;service.scan();render();}));
            if(s!=null&&!s.address.isEmpty())addButton(c,"Reconnect selected radio",()->withPermission(()->service.connect(p,s.address)));
            if(s!=null&&s.connected)addButton(c,"Disconnect",()->service.disconnect(p));
        }
        addButton(content,"Android Bluetooth settings",()->startActivity(new Intent(Settings.ACTION_BLUETOOTH_SETTINGS)));
        addButton(content,"Disconnect all and stop controller",()->{if(service!=null){service.stopScan();for(RadioProfile p:RadioProfile.values())service.disconnect(p);startService(new Intent(this,RadioService.class).setAction("STOP"));}});
        space(content,18);content.addView(text("MeshCrossway Android 0.1.0-alpha01\nNo forwarding between networks. Saved radio addresses stay in this app's private storage.",12,MUTED));
    }
    private void empty(LinearLayout content){
        String[] descriptions={"","Nearby and saved nodes from both networks.","Channels and groups, separated by network.","Conversations from both networks in one place."};
        title(content,pages[selected],descriptions[selected]);LinearLayout c=card(content);
        c.addView(text("Coming in the next stages",19,ORANGE));space(c,12);
        c.addView(text("Alpha01 validates radio connections first. This screen will use real synchronized data as the protocol clients are implemented.",15,MUTED));
        addButton(c,"Manage radio connections",()->page(5));
    }
    private void map(){
        LinearLayout layout=row();layout.setGravity(Gravity.TOP);body.addView(layout,new LinearLayout.LayoutParams(-1,-1));
        ScrollView railScroll=new ScrollView(this);LinearLayout rail=column();rail.setPadding(dp(6),dp(12),dp(6),dp(12));rail.setBackgroundColor(PANEL);railScroll.addView(rail);layout.addView(railScroll,new LinearLayout.LayoutParams(dp(76),-1));
        String[] tools={"View","Zoom +","Zoom −","GPS","Track","POIs","Offline","Details"};
        for(String tool:tools){Button b=button(tool,()->new AlertDialog.Builder(this).setTitle(tool).setMessage("Map tools are planned for a later alpha. This APK validates the independent Bluetooth connections first.").setPositiveButton("OK",null).show());LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,dp(52));p.bottomMargin=dp(8);rail.addView(b,p);}
        LinearLayout panel=column();panel.setGravity(Gravity.CENTER);panel.setPadding(dp(20),dp(20),dp(20),dp(20));layout.addView(panel,new LinearLayout.LayoutParams(0,-1,1));
        TextView icon=text("◇",56,ORANGE);icon.setGravity(Gravity.CENTER);panel.addView(icon);space(panel,16);
        TextView heading=text("Map workspace",23,WHITE);heading.setGravity(Gravity.CENTER);panel.addView(heading);space(panel,12);
        TextView note=text("Map rendering, phone location, tracks and offline areas will arrive in a later alpha.\n\nContext tools stay on the left; primary navigation stays below.",14,MUTED);note.setGravity(Gravity.CENTER);panel.addView(note);
    }
    private void withPermission(Runnable action){
        if(service==null){Toast.makeText(this,"Controller is starting; try again",Toast.LENGTH_SHORT).show();return;}
        ArrayList<String> missing=new ArrayList<>();
        if(Build.VERSION.SDK_INT>=31){for(String p:new String[]{Manifest.permission.BLUETOOTH_SCAN,Manifest.permission.BLUETOOTH_CONNECT})if(checkSelfPermission(p)!=PackageManager.PERMISSION_GRANTED)missing.add(p);}
        else if(checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)!=PackageManager.PERMISSION_GRANTED)missing.add(Manifest.permission.ACCESS_FINE_LOCATION);
        if(Build.VERSION.SDK_INT>=33&&checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)!=PackageManager.PERMISSION_GRANTED)missing.add(Manifest.permission.POST_NOTIFICATIONS);
        if(!missing.isEmpty()){pending=action;requestPermissions(missing.toArray(new String[0]),7);return;}
        startForegroundService(new Intent(this,RadioService.class));action.run();
    }
    @Override public void onRequestPermissionsResult(int request,String[] permissions,int[] results){
        super.onRequestPermissionsResult(request,permissions,results);
        if(request==7){Runnable action=pending;pending=null;if(service!=null&&service.allowed()&&action!=null){startForegroundService(new Intent(this,RadioService.class));action.run();}else Toast.makeText(this,"Nearby devices permission is required. Enable it in Android app settings if denied.",Toast.LENGTH_LONG).show();}
    }
}
