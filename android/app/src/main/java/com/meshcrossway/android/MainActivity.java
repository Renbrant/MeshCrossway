package com.meshcrossway.android;

import android.Manifest;
import android.app.*;
import android.bluetooth.BluetoothDevice;
import android.content.*;
import android.content.pm.PackageManager;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.*;
import android.provider.Settings;
import android.text.*;
import android.view.*;
import android.widget.*;
import java.nio.charset.StandardCharsets;
import java.text.DateFormat;
import java.util.*;
import static com.meshcrossway.android.MeshData.*;
import com.meshcrossway.android.MeshData.Message;

public final class MainActivity extends Activity {
    // Network identity colors never change with the app appearance.
    private static final int RADIO_MT=0xffff8a24,RADIO_MC=0xff6cbde6;
    private int BG=0xff090c10,PANEL=0xff181d24,PANEL2=0xff242b34,ORANGE=0xffff8a24,WHITE=0xfff2f4f7,MUTED=0xff9aa6b4,GREEN=0xff72d8a5,RED=0xffff7777;
    private Appearance appearance;
    private final String[] pages={"Home","Contacts","Channels","Chats","Map","Settings"},symbols={"⌂","⋮","☷","▤","◇","⚙"};
    private final Handler handler=new Handler(Looper.getMainLooper());
    private RadioService service;private LinearLayout root,body,nav,results;private ScrollView scroll;private EditText composer,search;private TextView count,chatStatus;private Button sendButton;
    private int selected,filter,listLimit=100,chatLimit=100,nodeOrder;private boolean favoritesOnly,unreadOnly,pinnedOnly,resumed,refreshQueued;
    private TrackService trackService;private boolean tracksBound;private Intent pendingTrack;private String exportingTrack="";private int nodeType;
    private boolean backupBusy;
    private byte[] mtLoraDraft,mtLoraBase;private boolean mtLoraSaving;private String mtDraftRadio="";private String settingsSection="";private RadioProfile settingsRadio;private final ArrayDeque<Screen> backStack=new ArrayDeque<>();private final HashMap<String,Integer> scrollPositions=new HashMap<>();private String renderedRoute="";private LinearLayout header;private boolean rendering;private String settingsFingerprint="",homeFingerprint="";private final HashMap<Integer,String> pageQueries=new HashMap<>();
    private static final class Screen{int selected,filter,y;String query,section,radio;RadioProfile profile;Target chat;RadioProfile chatProfile;}
    private TextView dualStatus;private View statusSeparator;MapPanel mapPanel;static String mapStyleOverride;private org.maplibre.android.geometry.LatLng mapFocus;PhonePosition position;

    private final DateFormat timestampFormat=DateFormat.getDateTimeInstance(DateFormat.SHORT,DateFormat.SHORT);private String query="",chatRadio="";private RadioProfile choosing,chatProfile;private Target chat;private boolean bound,visible,sending,applyingDraft;private int composerRevision;private Runnable pending;
    private final HashMap<String,String> drafts=new HashMap<>();
    private final Runnable refresh=()->{refreshQueued=false;refreshContent();},saveDraftTask=this::saveDraft;
    private final ServiceConnection connection=new ServiceConnection(){
        @Override public void onServiceConnected(ComponentName name,IBinder binder){service=((RadioService.LocalBinder)binder).service();if(visible){service.uiVisible=true;service.listener=MainActivity.this::scheduleRefresh;if(service.allowed()&&service.shouldResume())startForegroundService(new Intent(MainActivity.this,RadioService.class));}loadDraft();refreshContent();}
        @Override public void onServiceDisconnected(ComponentName name){service=null;refreshContent();}
    };
    private final ServiceConnection trackConnection=new ServiceConnection(){
        @Override public void onServiceConnected(ComponentName name,IBinder binder){trackService=((TrackService.LocalBinder)binder).service();if(visible)trackService.listener=MainActivity.this::scheduleRefresh;scheduleRefresh();}
        @Override public void onServiceDisconnected(ComponentName name){trackService=null;scheduleRefresh();}
    };
    @Override public void onCreate(Bundle state){
        Appearance chosenAppearance=Appearance.load(this);
        if(chosenAppearance.colors.light)setTheme(R.style.AppThemeLight);
        super.onCreate(state);appearance=chosenAppearance;applyAppearancePalette();position=new PhonePosition(this);position.listener=()->{if(mapPanel!=null)mapPanel.update();if(selected==0||(selected==1&&nodeOrder==2))scheduleRefresh();};if(state!=null){ArrayList<Bundle> history=state.getParcelableArrayList("navigation");if(history!=null)for(Bundle entry:history)backStack.addLast(decodeScreen(entry));exportingTrack=state.getString("exportingTrack","");nodeOrder=state.getInt("nodeOrder");nodeType=state.getInt("nodeType");settingsSection=state.getString("settingsSection","");if(state.containsKey("settingsRadio"))settingsRadio=RadioProfile.valueOf(state.getString("settingsRadio"));selected=state.getInt("page");filter=state.getInt("filter");query=state.getString("query","");if(state.containsKey("topic")){chatProfile=RadioProfile.valueOf(state.getString("profile"));chatRadio=state.getString("radio","");chat=new Target(state.getString("topic"),state.getString("title"),state.getString("peer"),state.getString("identity"),state.getInt("slot"),state.getBoolean("group")).reply(state.getLong("replyId",0),false);drafts.put(draftKey(),state.getString("draft",""));}}
        getWindow().setStatusBarColor(BG);getWindow().setNavigationBarColor(PANEL);
        int systemUiFlags=getWindow().getDecorView().getSystemUiVisibility();
        if(appearance.colors.light)systemUiFlags|=View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR|View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
        else systemUiFlags&=~(View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR|View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR);
        getWindow().getDecorView().setSystemUiVisibility(systemUiFlags);
        getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
        root=column();root.setBackgroundColor(android.graphics.Color.TRANSPARENT);
        android.widget.FrameLayout appearanceHost=new android.widget.FrameLayout(this);
        installAppearanceBackground(appearanceHost);
        appearanceHost.addView(root,new android.widget.FrameLayout.LayoutParams(-1,-1));
        root.setOnApplyWindowInsetsListener((view,insets)->{
            if(Build.VERSION.SDK_INT>=30){android.graphics.Insets bars=insets.getInsets(WindowInsets.Type.systemBars()),ime=insets.getInsets(WindowInsets.Type.ime());view.setPadding(bars.left,bars.top,bars.right,Math.max(bars.bottom,ime.bottom));}
            else view.setPadding(insets.getSystemWindowInsetLeft(),insets.getSystemWindowInsetTop(),insets.getSystemWindowInsetRight(),insets.getSystemWindowInsetBottom());return insets;
        });setContentView(appearanceHost);root.requestApplyInsets();
        header=row();header.setPadding(dp(12),dp(4),dp(8),dp(4));root.addView(header,new LinearLayout.LayoutParams(-1,dp(52)));
        statusSeparator=new View(this);statusSeparator.setBackgroundColor(PANEL2);
        root.addView(statusSeparator,new LinearLayout.LayoutParams(-1,dp(1)));
        dualStatus=text("MT offline · MC offline",11,MUTED);dualStatus.setPadding(dp(16),dp(5),dp(16),dp(5));dualStatus.setContentDescription("Radio connections");dualStatus.setOnClickListener(v->openSettings("connections",null));root.addView(dualStatus);
        body=column();root.addView(body,new LinearLayout.LayoutParams(-1,0,1));nav=row();nav.setBackgroundColor(PANEL);root.addView(nav,new LinearLayout.LayoutParams(-1,dp(64)));
        if(Build.VERSION.SDK_INT>=33)getOnBackInvokedDispatcher().registerOnBackInvokedCallback(android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT,this::handleBack);
        if(state==null)readConversationIntent(getIntent());buildPage();bound=bindService(new Intent(this,RadioService.class),connection,BIND_AUTO_CREATE);tracksBound=bindService(new Intent(this,TrackService.class),trackConnection,BIND_AUTO_CREATE);
    }
    @Override protected void onStart(){super.onStart();visible=true;if(trackService!=null)trackService.listener=this::scheduleRefresh;if(service!=null){service.uiVisible=true;service.listener=this::scheduleRefresh;}if(mapPanel!=null)mapPanel.onStart();refreshContent();}
    @Override protected void onStop(){saveDraft();visible=false;if(trackService!=null)trackService.listener=null;if(service!=null){service.uiVisible=false;service.listener=null;}if(mapPanel!=null)mapPanel.onStop();handler.removeCallbacks(refresh);refreshQueued=false;if(service==null||service.remoteWorkspace==null)MapEngine.pauseDownloads();super.onStop();}
    @Override protected void onDestroy(){if(mapPanel!=null){mapPanel.destroy();mapPanel=null;}position.stop();handler.removeCallbacksAndMessages(null);if(service!=null)service.listener=null;if(bound)unbindService(connection);if(trackService!=null)trackService.listener=null;if(tracksBound)unbindService(trackConnection);super.onDestroy();}
    @Override protected void onResume(){super.onResume();resumed=true;if(phoneEnabled())position.start();if(mapPanel!=null)mapPanel.onResume();}
    @Override protected void onPause(){resumed=false;position.stop();if(mapPanel!=null)mapPanel.onPause();super.onPause();}
    @Override public void onLowMemory(){if(mapPanel!=null)mapPanel.mapView.onLowMemory();super.onLowMemory();}
    @Override protected void onNewIntent(Intent intent){super.onNewIntent(intent);saveDraft();setIntent(intent);if(readConversationIntent(intent))buildPage();}
    private boolean readConversationIntent(Intent intent){try{String network=intent.getStringExtra("network"),radio=intent.getStringExtra("radio"),topic=intent.getStringExtra("topic"),title=intent.getStringExtra("title"),peer=intent.getStringExtra("peer"),identity=intent.getStringExtra("identity");if(network==null||radio==null||topic==null||title==null||peer==null||identity==null||radio.length()>64||topic.length()>512||title.length()>256||peer.length()>128||identity.length()>128)return false;chatProfile=RadioProfile.valueOf(network);chatRadio=radio;chat=new Target(topic,title,peer,identity,intent.getIntExtra("slot",0),intent.getBooleanExtra("group",false));selected=3;return true;}catch(IllegalArgumentException e){return false;}}
    @Override protected void onSaveInstanceState(Bundle out){saveDraft();ArrayList<Bundle> navigation=new ArrayList<>();for(Screen screen:backStack)navigation.add(encodeScreen(screen));out.putParcelableArrayList("navigation",navigation);out.putString("settingsSection",settingsSection);if(settingsRadio!=null)out.putString("settingsRadio",settingsRadio.name());out.putString("exportingTrack",exportingTrack);out.putInt("nodeOrder",nodeOrder);out.putInt("nodeType",nodeType);out.putInt("page",selected);out.putInt("filter",filter);out.putString("query",query);if(chat!=null){out.putString("profile",chatProfile.name());out.putString("radio",chatRadio);out.putString("topic",chat.topic);out.putString("title",chat.title);out.putString("peer",chat.peer);out.putString("identity",chat.identity);out.putInt("slot",chat.slot);out.putBoolean("group",chat.group);out.putLong("replyId",chat.replyId);out.putString("draft",drafts.getOrDefault(draftKey(),""));}super.onSaveInstanceState(out);}
    @Override public void onBackPressed(){handleBack();}
    private void handleBack(){
        if(mapPanel!=null&&mapPanel.closePanel())return;
        if(choosing!=null){if(service!=null)service.stopScan();choosing=null;buildPage();return;}
        if(!backStack.isEmpty()){saveDraft();Screen previous=backStack.pop();selected=previous.selected;filter=previous.filter;query=previous.query;settingsSection=previous.section;settingsRadio=previous.profile;chat=previous.chat;chatProfile=previous.chatProfile;chatRadio=previous.radio;buildPage();if(scroll!=null){ScrollView target=scroll;target.post(()->target.scrollTo(0,previous.y));}return;}
        if(chat!=null){saveDraft();chat=null;buildPage();}else if(selected==5&&!settingsSection.isEmpty()){settingsSection="";settingsRadio=null;buildPage();}else if(selected!=0)page(0);else finish();
    }
    private void remember(){Screen screen=new Screen();screen.selected=selected;screen.filter=filter;screen.query=query;screen.section=settingsSection;screen.profile=settingsRadio;screen.chat=chat;screen.chatProfile=chatProfile;screen.radio=chatRadio;screen.y=scroll==null?0:scroll.getScrollY();backStack.push(screen);}
    private void openSettings(String section,RadioProfile radio){saveDraft();remember();selected=5;chat=null;settingsSection=section;settingsRadio=radio;choosing=null;buildPage();}
    private Bundle encodeScreen(Screen screen){Bundle b=new Bundle();b.putInt("selected",screen.selected);b.putInt("filter",screen.filter);b.putInt("y",screen.y);b.putString("query",screen.query);b.putString("section",screen.section);b.putString("radio",screen.radio);if(screen.profile!=null)b.putString("profile",screen.profile.name());if(screen.chatProfile!=null)b.putString("chatProfile",screen.chatProfile.name());if(screen.chat!=null){Target t=screen.chat;b.putString("topic",t.topic);b.putString("title",t.title);b.putString("peer",t.peer);b.putString("identity",t.identity);b.putInt("slot",t.slot);b.putBoolean("group",t.group);b.putLong("replyId",t.replyId);}return b;}
    private Screen decodeScreen(Bundle b){Screen screen=new Screen();screen.selected=b.getInt("selected");screen.filter=b.getInt("filter");screen.y=b.getInt("y");screen.query=b.getString("query","");screen.section=b.getString("section","");screen.radio=b.getString("radio","");if(b.containsKey("profile"))screen.profile=RadioProfile.valueOf(b.getString("profile"));if(b.containsKey("chatProfile"))screen.chatProfile=RadioProfile.valueOf(b.getString("chatProfile"));if(b.containsKey("topic"))screen.chat=new Target(b.getString("topic"),b.getString("title"),b.getString("peer"),b.getString("identity"),b.getInt("slot"),b.getBoolean("group")).reply(b.getLong("replyId",0),false);return screen;}
    private String route(){return selected+":"+settingsSection+":"+settingsRadio+":"+(chat==null?"":chatRadio+chat.topic);}
    private void applyAppearancePalette(){
        Appearance.Palette color=appearance.colors;
        BG=color.bg;PANEL=color.panel;PANEL2=color.panel2;
        ORANGE=color.accent;WHITE=color.text;MUTED=color.muted;
    }
    private void installAppearanceBackground(android.widget.FrameLayout parent){
        parent.setBackground(appearance.background());
        if(!Appearance.PHOTO.equals(appearance.mode)||!appearance.hasPhoto)return;
        android.graphics.Bitmap photo=appearance.loadPhoto(this);
        if(photo==null)return;
        android.widget.ImageView background=new android.widget.ImageView(this);
        background.setScaleType(android.widget.ImageView.ScaleType.CENTER_CROP);
        if(Build.VERSION.SDK_INT>=31&&appearance.blur>0){
            background.setImageBitmap(photo);
            float pixels=dp(Math.max(1,appearance.blur));
            background.setRenderEffect(android.graphics.RenderEffect.createBlurEffect(
                pixels,pixels,android.graphics.Shader.TileMode.CLAMP));
        }else if(appearance.blur>0){
            android.graphics.Bitmap blurred=Appearance.approximateBlur(photo,appearance.blur);
            background.setImageBitmap(blurred);
            if(blurred!=photo)photo.recycle();
        }else background.setImageBitmap(photo);
        parent.addView(background,new android.widget.FrameLayout.LayoutParams(-1,-1));
        View darken=new View(this);
        darken.setBackgroundColor(android.graphics.Color.argb(
            Math.round(appearance.dim*2.55f),0,0,0));
        parent.addView(darken,new android.widget.FrameLayout.LayoutParams(-1,-1));
    }
    private String installedVersion(){
        try{return getPackageManager().getPackageInfo(getPackageName(),0).versionName;}
        catch(android.content.pm.PackageManager.NameNotFoundException e){return "beta";}
    }
    private int dp(float n){return Math.round(n*getResources().getDisplayMetrics().density);}
    private LinearLayout column(){LinearLayout l=new LinearLayout(this);l.setOrientation(LinearLayout.VERTICAL);return l;}
    private LinearLayout row(){LinearLayout l=new LinearLayout(this);l.setGravity(Gravity.CENTER_VERTICAL);return l;}
    private TextView text(String value,int size,int color){TextView t=new TextView(this);t.setText(value);t.setTextSize(size);t.setTextColor(color);t.setGravity(Gravity.CENTER_VERTICAL);return t;}
    private GradientDrawable surface(int color){GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(dp(16));return d;}
    private void space(LinearLayout l,int n){l.addView(new View(this),new LinearLayout.LayoutParams(1,dp(n)));}
    private void title(LinearLayout l,String h,String description){TextView t=text(h,25,WHITE);t.setTypeface(null,Typeface.BOLD);l.addView(t);space(l,8);l.addView(text(description,14,MUTED));space(l,18);}
    private Button button(String label,Runnable action){Button b=new Button(this);b.setText(label);b.setAllCaps(false);b.setTextSize(13);b.setTextColor(ORANGE);b.setMinHeight(dp(48));b.setPadding(dp(10),dp(4),dp(10),dp(4));b.setBackground(surface(PANEL2));b.setOnClickListener(v->action.run());return b;}
    private void addButton(LinearLayout l,String label,Runnable action){LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,-2);p.topMargin=dp(10);l.addView(button(label,action),p);}
    private LinearLayout card(LinearLayout l){LinearLayout c=column();c.setPadding(dp(14),dp(12),dp(14),dp(12));c.setBackground(surface(PANEL));LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,-2);p.bottomMargin=dp(8);l.addView(c,p);return c;}
    private String homeFingerprint(){
        StringBuilder key=new StringBuilder();
        for(RadioProfile p:RadioProfile.values()){
            Snapshot data=snapshot(p);
            key.append(data.nodes.size()).append(':').append(data.channels.size()).append(':')
                .append(data.messages.size()).append(':').append(data.unread());
            long latest=0,heard=0;
            for(Message message:data.messages)latest=Math.max(latest,message.timestamp);
            for(Node node:data.nodes)heard=Math.max(heard,node.heard);
            key.append(':').append(latest).append(':').append(heard);
            if(service!=null){
                RadioService.Session session=service.sessions.get(p);
                key.append(':').append(session.verified).append(':').append(session.desired)
                    .append(':').append(session.phase).append(':').append(session.name)
                    .append(':').append(session.address).append(':')
                    .append(session.rssiDbm).append(':').append(session.rssiCurrentLink);
            }
        }
        key.append(':').append(phoneEnabled());
        key.append(':').append(service==null?"no-service":service.remoteUrl());
        if(service!=null)key.append(':').append(service.remoteError);
        if(position.location!=null){
            key.append(':').append(Math.round(position.location.getLatitude()*10000))
                .append(':').append(Math.round(position.location.getLongitude()*10000))
                .append(':').append(Math.round(position.location.getAccuracy()))
                .append(':').append(position.location.getTime()/30000);
        }
        if(trackService!=null){
            TrackStore.Track track=trackService.store.current();
            key.append(':').append(trackService.store.tracks.size());
            if(track!=null)key.append(':').append(track.id).append(':').append(track.state).append(':').append(track.count);
        }
        return key.toString();
    }
    private String settingsFingerprint(){StringBuilder value=new StringBuilder(settingsSection);if(service!=null&&(settingsSection.isEmpty()||settingsSection.equals("radio")||settingsSection.equals("connections")||settingsSection.startsWith("mc_")||settingsSection.startsWith("mt_"))){for(RadioService.Session session:service.sessions.values()){value.append(session.address).append(session.phase).append(session.status);if(session.client!=null){ProtocolClient c=session.client;value.append(c.mtRevision).append(c.admin.revision).append(c.admin.status).append(c.admin.error).append(c.localName).append(c.firmware).append(c.mtOwner.hashCode()).append(c.mtConfigs.hashCode()).append(c.mtModules.hashCode()).append(c.region).append(c.hopLimit).append(c.txEnabled).append(c.mtSessionKey.length).append(c.frequencyMhz).append(c.bandwidthKhz).append(c.spreadingFactor).append(c.codingRate).append(c.txPower).append(c.pathHashMode).append(c.hasRadioStats).append(c.noiseFloor).append(c.lastRssi).append(c.lastSnrQuarter).append(c.txAirSecs).append(c.rxAirSecs).append(c.hasBattery).append(c.batteryMv).append(c.usedKb).append(c.totalKb).append(c.uptimeSecs).append(c.defaultFloodScope).append(c.manualAddContacts).append(c.telemetryModes).append(c.advertLocationPolicy).append(c.multiAcks).append(c.hasLocation).append(c.latitude).append(c.longitude);}}if(choosing!=null)value.append(service.devices.keySet()).append(service.scanning).append(service.showOtherDevices);}return value.toString();}
    private void rebuildContent(){rendering=true;refreshContent();rendering=false;}
    private Button chip(String label,boolean active,Runnable action){Button b=button(label,action);b.setTextSize(12);b.setTextColor(active?ORANGE:MUTED);b.setBackground(surface(active?appearance.colors.navActive:BG));b.setPadding(dp(5),0,dp(5),0);return b;}
    private void section(LinearLayout parent,String label){TextView view=text(label,11,MUTED);view.setTypeface(null,Typeface.BOLD);view.setLetterSpacing(.1f);view.setPadding(dp(6),dp(14),0,dp(8));parent.addView(view);}
    private LinearLayout group(LinearLayout parent){LinearLayout group=column();group.setBackground(surface(PANEL));LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,-2);p.bottomMargin=dp(6);parent.addView(group,p);return group;}
    private void menuRow(LinearLayout parent,String icon,String title,String subtitle,Runnable action,int color){if(parent.getChildCount()>0){View divider=new View(this);divider.setBackgroundColor(PANEL2);LinearLayout.LayoutParams line=new LinearLayout.LayoutParams(-1,dp(1));line.leftMargin=dp(54);line.rightMargin=dp(14);parent.addView(divider,line);}LinearLayout row=row();row.setPadding(dp(14),dp(12),dp(14),dp(12));row.setMinimumHeight(dp(64));row.addView(new NavIcon(this,icon,color),new LinearLayout.LayoutParams(dp(24),dp(24)));LinearLayout labels=column();labels.setPadding(dp(14),0,dp(8),0);TextView heading=text(title,15,WHITE);heading.setTypeface(null,Typeface.BOLD);labels.addView(heading);if(!subtitle.isEmpty()){TextView detail=text(subtitle,12,MUTED);detail.setPadding(0,dp(4),0,0);detail.setMaxLines(3);labels.addView(detail);}row.addView(labels,new LinearLayout.LayoutParams(0,-2,1));if(action!=null){row.addView(new NavIcon(this,"arrow",MUTED),new LinearLayout.LayoutParams(dp(16),dp(16)));row.setBackground(clickSurface());row.setFocusable(true);row.setOnClickListener(v->action.run());}row.setContentDescription(title+". "+subtitle);parent.addView(row,new LinearLayout.LayoutParams(-1,-2));}
    private android.graphics.drawable.Drawable clickSurface(){return new android.graphics.drawable.RippleDrawable(android.content.res.ColorStateList.valueOf((ORANGE&0x00ffffff)|0x33000000),null,new android.graphics.drawable.ColorDrawable(WHITE));}
    private void toggleRow(LinearLayout parent,String title,String subtitle,boolean checked,java.util.function.Consumer<Boolean> changed){LinearLayout row=row();row.setPadding(dp(16),dp(12),dp(12),dp(12));LinearLayout labels=column();labels.addView(text(title,15,WHITE));TextView hint=text(subtitle,12,MUTED);hint.setPadding(0,dp(4),dp(8),0);labels.addView(hint);row.addView(labels,new LinearLayout.LayoutParams(0,-2,1));Switch toggle=new Switch(this);toggle.setContentDescription(title);toggle.setChecked(checked);toggle.setMinimumHeight(dp(48));toggle.setOnCheckedChangeListener((b,on)->changed.accept(on));row.addView(toggle);row.setOnClickListener(v->toggle.setChecked(!toggle.isChecked()));parent.addView(row);}
    private void toolbar(){header.removeAllViews();boolean back=chat!=null||(selected==5&&!settingsSection.isEmpty())||!backStack.isEmpty();if(back)header.addView(iconButton("back","Back",this::handleBack));if(selected==0&&!back){ImageView logo=new ImageView(this);logo.setImageResource(R.drawable.meshcrossway_logo);logo.setContentDescription("MeshCrossway");logo.setScaleType(ImageView.ScaleType.FIT_START);header.addView(logo,new LinearLayout.LayoutParams(0,dp(27),1));}else{String title=chat!=null?chat.title:selected==5?settingsTitle():pages[selected];TextView label=text(title,20,WHITE);label.setTypeface(null,Typeface.BOLD);label.setMaxLines(1);label.setEllipsize(android.text.TextUtils.TruncateAt.END);header.addView(label,new LinearLayout.LayoutParams(0,-1,1));}if(chat!=null)header.addView(iconButton("settings","Settings",()->openSettings("",null)));header.addView(iconButton("more","More options",this::moreMenu));}
    private View iconButton(String icon,String label,Runnable action){android.widget.FrameLayout target=new android.widget.FrameLayout(this);target.setContentDescription(label);target.setFocusable(true);target.setBackground(clickSurface());NavIcon image=new NavIcon(this,icon,MUTED);android.widget.FrameLayout.LayoutParams size=new android.widget.FrameLayout.LayoutParams(dp(24),dp(24),Gravity.CENTER);target.addView(image,size);target.setLayoutParams(new LinearLayout.LayoutParams(dp(44),dp(44)));target.setOnClickListener(v->action.run());return target;}
    private String settingsTitle(){switch(settingsSection){case "appearance":return "Appearance";case "radio":return settingsRadio==null?"Radio":settingsRadio.title;case "connections":return "Connections";case "messages":return "Notifications & messages";case "location":return "Map & location";case "data":return "Data & backup";case "about":return "About";case "mc_name":return "Node Name";case "mc_radio":return "Radio Settings";case "mc_regions":return "Regions";case "mc_stats":return "Radio stats";case "mc_telemetry":return "Telemetry";case "mc_path":return "Node ID size in paths";case "mc_location":return "Location";case "mc_contacts":return "Contact Settings";case "mc_privacy":return "Privacy Settings";case "mc_device":return "Device Info";case "mt_user":return "User";case "mt_lora":return "LoRa";case "mt_channels":return "Channels";case "mt_device":return "Device configuration";case "mt_security":return "Security";case "mt_modules":return "Module configuration";case "mt_admin":return "Administration";default:return "Settings";}}
    private void moreMenu(){PopupMenu menu=new PopupMenu(this,header.getChildAt(header.getChildCount()-1));if(chat!=null){menu.getMenu().add("Conversation options").setOnMenuItemClickListener(item->{conversationOptions(chatProfile,chatRadio,chat);return true;});}else{menu.getMenu().add("Conversations").setOnMenuItemClickListener(item->{page(3);return true;});menu.getMenu().add("Channels").setOnMenuItemClickListener(item->{page(2);return true;});menu.getMenu().add("Tracks").setOnMenuItemClickListener(item->{manageTracks();return true;});}menu.getMenu().add("Radio connections").setOnMenuItemClickListener(item->{openSettings("connections",null);return true;});menu.show();}
    private void sortMenu(){new AlertDialog.Builder(this).setTitle("Sort contacts").setSingleChoiceItems(new String[]{"Recently heard","Name","Distance"},nodeOrder,(d,index)->{nodeOrder=index;if(index==2&&position.location==null)requestPhonePosition();d.dismiss();buildPage();}).setNegativeButton("Back",null).show();}
    private void typeMenu(){new AlertDialog.Builder(this).setTitle("Contact type").setSingleChoiceItems(new String[]{"All types","Clients","Repeaters","Rooms"},nodeType,(d,index)->{nodeType=index;d.dismiss();buildPage();}).setNegativeButton("Back",null).show();}
    private void openMapTool(String tool){saveDraft();remember();selected=4;chat=null;settingsSection="";settingsRadio=null;buildPage();if(mapPanel!=null)mapPanel.openTool(tool);}
    private void scheduleRefresh(){if(!refreshQueued){refreshQueued=true;handler.postDelayed(refresh,350);}}
    private void page(int index){saveDraft();if(service!=null&&choosing!=null)service.stopScan();backStack.clear();pageQueries.put(selected,query);selected=index;chat=null;choosing=null;settingsSection="";settingsRadio=null;query=pageQueries.getOrDefault(index,"");listLimit=100;buildPage();}
    private void buildPage(){
        if(scroll!=null&&!renderedRoute.isEmpty())scrollPositions.put(renderedRoute,scroll.getScrollY());renderedRoute=route();
        if(mapPanel!=null){mapPanel.destroy();mapPanel=null;}body.removeAllViews();nav.removeAllViews();composer=null;search=null;results=null;scroll=null;chatStatus=null;sendButton=null;
        // Home's radio cards replace the redundant global status strip.
        statusSeparator.setVisibility(selected==0?View.GONE:View.VISIBLE);
        dualStatus.setVisibility(selected==0?View.GONE:View.VISIBLE);
        toolbar();int[] destinations={0,3,1,4,5};String[] icons={"home","chats","contacts","map","settings"};for(int i=0;i<destinations.length;i++){final int index=destinations[i];boolean active=selected==index||(index==3&&selected==2);LinearLayout item=column();item.setGravity(Gravity.CENTER);LinearLayout marker=row();marker.setGravity(Gravity.CENTER);if(active)marker.setBackground(surface(appearance.colors.navActive));marker.addView(new NavIcon(this,icons[i],active?ORANGE:MUTED),new LinearLayout.LayoutParams(dp(23),dp(23)));item.addView(marker,new LinearLayout.LayoutParams(dp(54),dp(30)));TextView label=text(pages[index],11,active?WHITE:MUTED);label.setGravity(Gravity.CENTER);item.addView(label);item.setContentDescription(pages[index]);item.setFocusable(true);item.setOnClickListener(v->page(index));nav.addView(item,new LinearLayout.LayoutParams(0,-1,1));}
        if(selected==4){map();return;}
        if(chat!=null&&selected==3){chatPage();return;}
        if(selected>=1&&selected<=3){LinearLayout tools=column();tools.setPadding(dp(16),dp(4),dp(16),dp(4));
            if(selected==2||selected==3){LinearLayout tabs=row();tabs.addView(chip("Conversations",selected==3,()->page(3)),new LinearLayout.LayoutParams(0,dp(44),1));tabs.addView(chip("Channels",selected==2,()->page(2)),new LinearLayout.LayoutParams(0,dp(44),1));tools.addView(tabs);}
            LinearLayout filters=row();String[] labels={"All networks","Meshtastic","MeshCore"};for(int i=0;i<3;i++){final int choice=i;filters.addView(chip(labels[i],i==filter,()->{filter=choice;buildPage();}),new LinearLayout.LayoutParams(0,dp(44),1));}tools.addView(filters);
            search=new EditText(this);search.setTextColor(WHITE);search.setHintTextColor(MUTED);search.setTextSize(14);search.setHint("Search "+pages[selected].toLowerCase(Locale.ROOT));search.setSingleLine(true);search.setText(query);search.setPadding(dp(12),0,dp(12),0);search.setBackground(surface(PANEL));search.addTextChangedListener(watcher(()->{query=search.getText().toString();listLimit=100;refreshContent();}));tools.addView(search,new LinearLayout.LayoutParams(-1,dp(44)));
            if(selected==1){LinearLayout options=row();options.addView(chip(favoritesOnly?"Favorites ✓":"Favorites",favoritesOnly,()->{favoritesOnly=!favoritesOnly;buildPage();}),new LinearLayout.LayoutParams(0,dp(44),1));options.addView(chip("Sort: "+new String[]{"Recent","Name","Distance"}[nodeOrder],false,this::sortMenu),new LinearLayout.LayoutParams(0,dp(44),1));options.addView(chip(new String[]{"All types","Clients","Repeaters","Rooms"}[nodeType],nodeType!=0,this::typeMenu),new LinearLayout.LayoutParams(0,dp(44),1));tools.addView(options);}
            if(selected==3){LinearLayout options=row();options.addView(chip("Unread",unreadOnly,()->{unreadOnly=!unreadOnly;buildPage();}),new LinearLayout.LayoutParams(0,dp(40),1));options.addView(chip("Pinned",pinnedOnly,()->{pinnedOnly=!pinnedOnly;buildPage();}),new LinearLayout.LayoutParams(0,dp(40),1));tools.addView(options);}body.addView(tools);}
        scroll=new ScrollView(this);results=column();results.setPadding(dp(selected==0?10:16),dp(selected==0?8:16),dp(selected==0?10:16),dp(20));scroll.addView(results);body.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));refreshContent();int restore=scrollPositions.getOrDefault(renderedRoute,0);ScrollView target=scroll;target.post(()->target.scrollTo(0,restore));
    }
    private boolean included(RadioProfile p){return filter==0||filter==p.ordinal()+1;}
    private boolean matches(String value){return value.toLowerCase(Locale.ROOT).contains(query.toLowerCase(Locale.ROOT));}
    private Snapshot snapshot(RadioProfile p){return service==null?Snapshot.empty():service.data(p);}
    private String radio(RadioProfile p){return service==null?"":service.sessions.get(p).address;}
    private boolean ready(RadioProfile p,String address){return service!=null&&service.sessions.get(p).verified&&radio(p).equals(address);}
    private void refreshContent(){
        if(dualStatus!=null&&service!=null){android.text.SpannableStringBuilder summary=new android.text.SpannableStringBuilder();for(RadioProfile p:RadioProfile.values()){if(summary.length()>0)summary.append("    ·    ");int begin=summary.length();RadioService.Session session=service.sessions.get(p);summary.append(p==RadioProfile.MESHTASTIC?"MT ":"MC ").append(session.verified?"ready":session.desired?session.phase.toLowerCase(Locale.ROOT):"offline");summary.setSpan(new android.text.style.ForegroundColorSpan(p==RadioProfile.MESHTASTIC?RADIO_MT:RADIO_MC),begin,summary.length(),0);}dualStatus.setText(summary);}
        if(mapPanel!=null)mapPanel.update();
        if(results==null)return;if(selected==0&&results.getChildCount()>0&&!rendering&&homeFingerprint().equals(homeFingerprint))return;homeFingerprint=homeFingerprint();if(selected==5&&results.getChildCount()>0&&!rendering){String fingerprint=settingsFingerprint();if(fingerprint.equals(settingsFingerprint))return;}settingsFingerprint=settingsFingerprint();int y=scroll==null?0:scroll.getScrollY();boolean bottom=scroll!=null&&(results.getHeight()-scroll.getHeight()-y<dp(80));results.removeAllViews();
        if(service!=null&&!service.history.error.isEmpty())results.addView(text(service.history.error,14,RED));
        if(chat!=null&&selected==3){messages();ScrollView target=scroll;LinearLayout contents=results;if(target!=null)target.post(()->target.scrollTo(0,bottom?contents.getHeight():y));return;}
        if(selected==0)home();else if(selected==5)settings();else lists();
        ScrollView target=scroll;if(target!=null)target.post(()->target.scrollTo(0,y));
    }
    private void remotePanel(){
        if(service==null){
            Toast.makeText(this,"Radio service is starting",Toast.LENGTH_SHORT).show();
            return;
        }
        if(service.remoteWorkspace==null)showRemotePinEditor(false);
        else showRemoteCredentials();
    }
    /** Numeric PIN is intentionally visible on the phone so it is easy to copy to a PC. */
    private void showRemotePinEditor(boolean changing){
        if(service==null)return;
        LinearLayout container=column();
        container.setPadding(dp(22),dp(4),dp(22),dp(4));
        EditText pin=new EditText(this);
        pin.setInputType(android.text.InputType.TYPE_CLASS_NUMBER);
        pin.setSingleLine(true);
        pin.setTextSize(24);
        pin.setTextColor(WHITE);
        pin.setHintTextColor(MUTED);
        pin.setHint("4–8 digit PIN");
        pin.setFilters(new android.text.InputFilter[]{new android.text.InputFilter.LengthFilter(8)});
        pin.setText(changing?service.remoteCode():RemoteWorkspace.newShortPin());
        pin.setSelectAllOnFocus(true);
        pin.setContentDescription("Remote Access PIN, four to eight digits");
        container.addView(pin,new LinearLayout.LayoutParams(-1,dp(56)));
        String guidance=changing?
            "Choose a new 4–8 digit numeric PIN. Existing browser sessions will sign out immediately.":
            "Choose your own 4–8 digit numeric PIN, or keep the suggested 6-digit PIN. "+
            "Type the same PIN in your computer browser. Use a trusted local Wi-Fi network.";
        AlertDialog dialog=new AlertDialog.Builder(this)
            .setTitle(changing?"Change Remote Access PIN":"Enable Remote Access")
            .setMessage(guidance)
            .setView(container)
            .setNegativeButton("Cancel",null)
            .setPositiveButton(changing?"Save PIN":"Enable",null)
            .create();
        dialog.setOnShowListener(ignored->dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(view->{
            String entered=pin.getText().toString();
            if(!RemoteWorkspace.validPin(entered)){
                pin.setError("Enter 4 to 8 numbers (0–9)");
                return;
            }
            if(service==null){dialog.dismiss();return;}
            if(changing){
                if(service.changeRemotePin(entered)){
                    dialog.dismiss();showRemoteCredentials();rebuildContent();
                }else pin.setError("Could not change PIN");
            }else{
                if(service.allowed())startForegroundService(new Intent(this,RadioService.class));
                if(service.startRemote(entered)){
                    dialog.dismiss();showRemoteCredentials();rebuildContent();
                }else Toast.makeText(this,service.remoteError,Toast.LENGTH_LONG).show();
            }
        }));
        dialog.show();
    }
    private void showRemoteCredentials(){
        if(service==null||service.remoteWorkspace==null)return;
        LinearLayout details=column();
        details.setPadding(dp(22),dp(8),dp(22),dp(4));
        details.addView(text("Open this address on your computer:",13,MUTED));
        space(details,4);
        TextView url=text(service.remoteUrl(),16,WHITE);
        url.setTextIsSelectable(true);
        details.addView(url);
        space(details,15);
        details.addView(text("ACCESS PIN",12,MUTED));
        space(details,3);
        TextView displayedPin=text(service.remoteCode(),30,ORANGE);
        displayedPin.setTypeface(null,Typeface.BOLD);
        displayedPin.setTextIsSelectable(true);
        details.addView(displayedPin);
        space(details,9);
        details.addView(text("Enter this same PIN in the browser. Only use on trusted local Wi-Fi.",13,MUTED));
        space(details,13);
        TextView fingerprintLink=text("View HTTPS certificate fingerprint",13,WHITE);
        fingerprintLink.setPadding(dp(8),dp(10),dp(8),dp(10));
        fingerprintLink.setBackground(surface(PANEL));
        fingerprintLink.setOnClickListener(v->{
            if(service==null||service.remoteWorkspace==null)return;
            new AlertDialog.Builder(this).setTitle("HTTPS certificate fingerprint")
                .setMessage("Confirm that the certificate fingerprint shown by your browser matches this value:\n\n"+
                    service.remoteFingerprint())
                .setPositiveButton("Back",null).show();
        });
        details.addView(fingerprintLink);
        new AlertDialog.Builder(this).setTitle("Remote Access · Active")
            .setView(details)
            .setNeutralButton("Copy address",(dialog,which)->{
                ((android.content.ClipboardManager)getSystemService(CLIPBOARD_SERVICE))
                    .setPrimaryClip(ClipData.newPlainText("MeshCrossway remote URL",service.remoteUrl()));
            })
            .setNegativeButton("Stop server",(dialog,which)->{service.stopRemote();rebuildContent();})
            .setPositiveButton("Change PIN",(dialog,which)->showRemotePinEditor(true))
            .show();
    }
    // Home reads the same independent per-radio snapshots as Contacts, Chats and Channels.
    private LinearLayout dashboardRow(){
        LinearLayout row=row();
        row.setGravity(Gravity.TOP);
        LinearLayout.LayoutParams params=new LinearLayout.LayoutParams(-1,-2);
        params.bottomMargin=dp(5);
        results.addView(row,params);
        return row;
    }
    private LinearLayout dashboardTile(LinearLayout parent,String heading,String value,String detail,int accent,Runnable action,boolean featured){
        LinearLayout tile=column();
        tile.setPadding(dp(12),dp(11),dp(12),dp(11));
        tile.setMinimumHeight(dp(featured?110:94));
        GradientDrawable background=surface(PANEL);
        background.setStroke(dp(1),featured?accent:PANEL2);
        tile.setBackground(new android.graphics.drawable.RippleDrawable(
            android.content.res.ColorStateList.valueOf(0x33ffffff),background,null));
        LinearLayout headingRow=row();
        TextView label=text(heading,11,accent);
        label.setTypeface(null,Typeface.BOLD);
        label.setLetterSpacing(.05f);
        headingRow.addView(label,new LinearLayout.LayoutParams(0,-2,1));
        headingRow.addView(new NavIcon(this,"arrow",MUTED),new LinearLayout.LayoutParams(dp(14),dp(14)));
        tile.addView(headingRow);
        space(tile,featured?8:6);
        TextView main=text(value,featured?20:21,WHITE);
        main.setTypeface(null,Typeface.BOLD);
        main.setMaxLines(2);
        main.setEllipsize(android.text.TextUtils.TruncateAt.END);
        tile.addView(main);
        space(tile,4);
        TextView subtitle=text(detail,12,MUTED);
        subtitle.setMaxLines(2);
        subtitle.setEllipsize(android.text.TextUtils.TruncateAt.END);
        tile.addView(subtitle);
        tile.setContentDescription(heading+". "+value+". "+detail);
        tile.setFocusable(true);
        tile.setOnClickListener(v->action.run());
        LinearLayout.LayoutParams params=new LinearLayout.LayoutParams(0,-2,1);
        params.leftMargin=dp(2);
        params.rightMargin=dp(2);
        parent.addView(tile,params);
        return tile;
    }
    /** Four BLE bars reflect measured phone-to-radio RSSI, never a LoRa signal report. */
    private void signalIndicator(LinearLayout tile,RadioService.Session session,int accent){
        final int rssi=session==null?BleSignal.UNKNOWN:session.rssiDbm;
        final boolean current=session!=null&&session.verified&&session.rssiCurrentLink;
        final int bars=BleSignal.bars(rssi);
        LinearLayout indicator=row();
        indicator.setGravity(Gravity.BOTTOM|Gravity.START);
        indicator.setPadding(0,dp(9),0,0);
        for(int index=0;index<4;index++){
            View bar=new View(this);
            int color=!BleSignal.valid(rssi)?PANEL2:
                index<bars?(current?accent:MUTED):PANEL2;
            GradientDrawable shape=new GradientDrawable();
            shape.setColor(color);
            shape.setCornerRadius(dp(2));
            bar.setBackground(shape);
            LinearLayout.LayoutParams params=new LinearLayout.LayoutParams(
                dp(4),dp(5+index*4));
            params.rightMargin=dp(3);
            indicator.addView(bar,params);
        }
        TextView reading=text(BleSignal.label(rssi,current),11,MUTED);
        reading.setMaxLines(1);
        reading.setEllipsize(android.text.TextUtils.TruncateAt.END);
        LinearLayout.LayoutParams textParams=new LinearLayout.LayoutParams(0,dp(19),1);
        textParams.leftMargin=dp(5);
        indicator.addView(reading,textParams);
        indicator.setContentDescription("Bluetooth signal. "+
            BleSignal.label(rssi,current)+". "+bars+" of four bars.");
        tile.addView(indicator);
    }
    private void openDashboardNetwork(RadioProfile profile,int destination){
        filter=profile.ordinal()+1;
        page(destination);
    }
    private int recentlyHeard(Snapshot data){
        int total=0;
        long cutoff=System.currentTimeMillis()/1000-3600;
        for(Node node:data.nodes)if(node.heard>=cutoff&&node.heard>0)total++;
        return total;
    }
    private String latestPreview(Snapshot data){
        Message last=null;
        for(Message message:data.messages)
            if(last==null||message.timestamp>last.timestamp)last=message;
        if(last==null)return "No conversations yet";
        String sender=last.outgoing?"You":last.sender==null||last.sender.trim().isEmpty()?"Received":last.sender.trim();
        String body=last.body==null?"":last.body.replace('\n',' ').replace('\r',' ').trim();
        if(body.length()>56)body=body.substring(0,56)+"…";
        return sender+": "+(body.isEmpty()?"Message":body);
    }
    private void home(){
        // First screen content: the two radios. No duplicate ready line,
        // global summary card, or section headings between card rows.
        LinearLayout radios=dashboardRow();
        for(RadioProfile profile:RadioProfile.values()){
            RadioService.Session session=service==null?null:service.sessions.get(profile);
            boolean ready=session!=null&&session.verified;
            String state=ready?"Ready":session!=null&&session.desired?"Connecting":"Offline";
            String device=session==null?"Connect a radio":session.name!=null&&!session.name.isEmpty()?session.name:
                session.address.isEmpty()?"Choose a device":"Saved device";
            if(session!=null&&session.desired&&!ready)device+=" · "+session.phase;
            int accent=profile==RadioProfile.MESHTASTIC?RADIO_MT:RADIO_MC;
            LinearLayout tile=dashboardTile(radios,profile.title.toUpperCase(Locale.ROOT),state,device,
                accent,()->openSettings("radio",profile),true);
            signalIndicator(tile,session,accent);
        }
        LinearLayout nodes=dashboardRow();
        for(RadioProfile profile:RadioProfile.values()){
            Snapshot data=snapshot(profile);
            int recent=recentlyHeard(data);
            dashboardTile(nodes,profile==RadioProfile.MESHTASTIC?"MT · NODES":"MC · NODES",
                String.valueOf(data.nodes.size()),recent+" heard in the last hour",
                profile==RadioProfile.MESHTASTIC?RADIO_MT:RADIO_MC,
                ()->openDashboardNetwork(profile,1),false);
        }
        LinearLayout chats=dashboardRow();
        for(RadioProfile profile:RadioProfile.values()){
            Snapshot data=snapshot(profile);
            dashboardTile(chats,profile==RadioProfile.MESHTASTIC?"MT · CHATS":"MC · CHATS",
                data.unread()+" unread",latestPreview(data),
                profile==RadioProfile.MESHTASTIC?RADIO_MT:RADIO_MC,
                ()->openDashboardNetwork(profile,3),false);
        }
        LinearLayout channels=dashboardRow();
        for(RadioProfile profile:RadioProfile.values()){
            int configured=0;
            for(Channel channel:snapshot(profile).channels)if(channel.enabled)configured++;
            dashboardTile(channels,profile==RadioProfile.MESHTASTIC?"MT · CHANNELS":"MC · CHANNELS",
                String.valueOf(configured),"Configured channels",
                profile==RadioProfile.MESHTASTIC?RADIO_MT:RADIO_MC,
                ()->openDashboardNetwork(profile,2),false);
        }
        // Remote Access remains directly available without occupying the top of Home.
        LinearLayout remote=group(results);
        String remoteUrl=service==null?"":service.remoteUrl();
        String wifi=RemoteWorkspace.wifiName(this);
        String remoteDetail=remoteUrl.isEmpty()?
            "Off · "+wifi+" · tap to enable private HTTPS access":
            wifi+" · "+remoteUrl+" · tap for access code";
        menuRow(remote,"settings","Remote Access",remoteDetail,
            this::remotePanel,remoteUrl.isEmpty()?MUTED:GREEN);
        if(service!=null&&!service.remoteError.isEmpty())
            remote.addView(text(service.remoteError,12,RED));
        LinearLayout tools=dashboardRow();
        android.location.Location fix=position.location;
        boolean gpsOn=phoneEnabled();
        boolean fresh=gpsOn&&fix!=null&&System.currentTimeMillis()-fix.getTime()<=120000;
        String gpsValue=!gpsOn?"Off":fresh?String.format(Locale.ROOT,"±%.0f m",fix.getAccuracy()):"Waiting";
        String gpsDetail=fresh?String.format(Locale.ROOT,"%.4f, %.4f · phone fix",fix.getLatitude(),fix.getLongitude()):
            "Phone location · never auto-shared";
        dashboardTile(tools,"PHONE GPS",gpsValue,gpsDetail,GREEN,
            ()->openSettings("location",null),false);
        TrackStore.Track current=trackService==null?null:trackService.store.current();
        String trackValue=current==null?"No active track":current.state;
        String trackDetail=current==null?"Record and export GPX":current.name+" · "+current.summary();
        dashboardTile(tools,"TRACKS",trackValue,trackDetail,ORANGE,this::manageTracks,false);
        LinearLayout shortcuts=group(results);
        menuRow(shortcuts,"map","Open unified map","Meshtastic + MeshCore · offline areas · POIs",
            ()->page(4),ORANGE);
    }
    private void lists(){
        int shown=0;
        if(selected==1){ArrayList<MapPanel.Located> nodes=mapNodes();nodes.removeIf(ref->!included(ref.profile)||!matches(nodeName(ref.profile,ref.node)+" "+ref.node.id)||(favoritesOnly&&!ref.favorite)||(nodeType!=0&&contactType(ref.profile,ref.node)!=nodeType));
            nodes.sort((a,b)->{if(a.favorite!=b.favorite)return a.favorite?-1:1;if(nodeOrder==1)return nodeName(a.profile,a.node).compareToIgnoreCase(nodeName(b.profile,b.node));if(nodeOrder==2)return Double.compare(nodeDistance(a.node),nodeDistance(b.node));return Long.compare(b.node.heard,a.node.heard);});
            for(MapPanel.Located ref:nodes){if(shown++>=listLimit)continue;Node n=ref.node;LinearLayout c=card(results);c.addView(text((ref.favorite?"★ ":"")+nodeName(ref.profile,n),18,WHITE));space(c,6);c.addView(text(ref.profile.title+" · "+contactTypeName(ref.profile,n)+" · "+ago(n.heard)+distanceText(n)+(n.battery>=0?" · battery "+n.battery+"%":""),12,ref.profile==RadioProfile.MESHTASTIC?RADIO_MT:RADIO_MC));c.setOnClickListener(v->nodeDetails(ref.profile,n));c.setFocusable(true);}
        }else if(selected==2){ArrayList<ChannelRow> channels=new ArrayList<>();for(RadioProfile p:RadioProfile.values())if(included(p))for(Channel channel:snapshot(p).channels)if(channel.enabled&&matches(channel.name)){ChannelRow entry=new ChannelRow(p,channel);for(Message m:snapshot(p).messages)if(m.topic.equals(channel.topic())){entry.last=Math.max(entry.last,m.timestamp);if(!m.read&&!m.outgoing)entry.unread++;}channels.add(entry);}channels.sort((a,b)->a.last==b.last?a.channel.name.compareToIgnoreCase(b.channel.name):Long.compare(b.last,a.last));
            for(ChannelRow entry:channels){if(shown++>=listLimit)continue;RadioProfile p=entry.profile;Channel channel=entry.channel;LinearLayout c=card(results);TextView name=text(channel.name+(entry.unread>0?" · "+entry.unread+" unread":""),18,entry.unread>0?ORANGE:WHITE);name.setTypeface(null,Typeface.BOLD);c.addView(name);space(c,6);c.addView(text(p.title+" · channel "+channel.slot+(entry.last>0?" · "+when(entry.last):"")+(ready(p,radio(p))?"":" · saved"),12,p==RadioProfile.MESHTASTIC?RADIO_MT:RADIO_MC));c.setOnClickListener(v->openChat(p,radio(p),Target.channel(channel)));c.setFocusable(true);}
        }else{EnumMap<RadioProfile,Snapshot> snapshots=new EnumMap<>(RadioProfile.class);EnumMap<RadioProfile,String> radios=new EnumMap<>(RadioProfile.class);for(RadioProfile p:RadioProfile.values()){snapshots.put(p,snapshot(p));radios.put(p,radio(p));}
            for(UiFeed.Item item:UiFeed.conversations(snapshots,radios,filter,unreadOnly,pinnedOnly,query)){Conversation conversation=item.conversation;if(shown++>=listLimit)continue;LinearLayout c=card(results);c.addView(text((conversation.pinned?"★ ":"")+conversation.target.title+(conversation.unread>0?" · "+conversation.unread+" unread":"")+(conversation.muted?" · muted":""),18,conversation.unread>0?ORANGE:WHITE));space(c,6);TextView preview=text(MessageActions.displayBody(item.profile,conversation.last),14,MUTED);preview.setMaxLines(2);c.addView(preview);space(c,6);c.addView(text(item.profile.title+" · "+when(conversation.last.timestamp),12,item.profile==RadioProfile.MESHTASTIC?RADIO_MT:RADIO_MC));c.setOnClickListener(v->openChat(item.profile,item.radio,conversation.target));c.setOnLongClickListener(v->{conversationOptions(item.profile,item.radio,conversation.target);return true;});c.setFocusable(true);}
        }
        if(shown>listLimit)addButton(results,"Show more results ("+shown+" total)",()->{listLimit+=100;refreshContent();});
        if(shown==0){LinearLayout c=card(results);c.addView(text("No matching "+pages[selected].toLowerCase(Locale.ROOT),20,ORANGE));space(c,8);c.addView(text(selected==3?"Open a channel in Groups or a client in Nodes to start a conversation.":"Connect a radio and wait for synchronization. Saved data remains available offline.",14,MUTED));}
        if(selected==3&&service!=null){results.addView(text("Showing up to "+service.history.historyLimit+" recent messages per radio.",12,MUTED));if(service.history.historyLimit<20000)addButton(results,"Load older messages",()->service.history.older());}
    }
    private static final class ChannelRow{final RadioProfile profile;final Channel channel;long last;int unread;ChannelRow(RadioProfile p,Channel c){profile=p;channel=c;}}
    private static int contactType(RadioProfile p,Node n){return p==RadioProfile.MESHCORE?n.type:n.unmessagable?0:1;}
    private static String contactTypeName(RadioProfile p,Node n){if(p==RadioProfile.MESHTASTIC)return "Node";switch(n.type){case 1:return "Client";case 2:return "Repeater";case 3:return "Room";default:return "Other";}}
    ArrayList<MapPanel.Located> mapNodes(){ArrayList<MapPanel.Located> nodes=new ArrayList<>();for(RadioProfile p:RadioProfile.values())for(Node n:snapshot(p).nodes)nodes.add(new MapPanel.Located(p,radio(p),n,snapshot(p).flags("node",n.id).favorite));return nodes;}
    private double nodeDistance(Node n){return position.location==null||System.currentTimeMillis()-position.location.getTime()>120000||!n.position?Double.POSITIVE_INFINITY:GeoMath.distance(position.location.getLatitude(),position.location.getLongitude(),n.lat,n.lon);}
    private String distanceText(Node n){double meters=nodeDistance(n);return Double.isFinite(meters)?String.format(Locale.ROOT," · %.1f km · %03.0f° %s",meters/1000,FieldTools.bearing(position.location.getLatitude(),position.location.getLongitude(),n.lat,n.lon),FieldTools.direction(FieldTools.bearing(position.location.getLatitude(),position.location.getLongitude(),n.lat,n.lon))):"";}
    private String nodeName(RadioProfile p,Node n){if(!n.name.isEmpty())return n.name;return p==RadioProfile.MESHTASTIC?String.format(Locale.ROOT,"!%08x",Long.parseLong(n.id)):n.id.substring(0,Math.min(12,n.id.length()));}
    private void nodeDetails(RadioProfile p,Node n){String source=radio(p);boolean favorite=snapshot(p).flags("node",n.id).favorite;ArrayList<String> actions=new ArrayList<>(Arrays.asList("Node information","Message",favorite?"Remove favorite":"Add favorite"));if(n.position){actions.add("Show on map");actions.add("Distance and bearing");}if(p==RadioProfile.MESHTASTIC)actions.add("Metrics & radio tools");new AlertDialog.Builder(this).setTitle(p.title+" · "+nodeName(p,n)).setItems(actions.toArray(new String[0]),(dialog,index)->{String action=actions.get(index);if(index==0)new AlertDialog.Builder(this).setTitle(nodeName(p,n)).setMessage(p.title+"\n"+n.id+"\nLast heard: "+when(n.heard)+distanceText(n)+(n.battery>=0?"\nBattery: "+n.battery+"%":"")+(p==RadioProfile.MESHTASTIC?"\nShort name: "+n.shortName+"\nHardware: "+MtLora.hardwareName(n.hardwareModel)+"\nRole: "+n.role+"\nStatus: "+n.status:"")+(n.position?String.format(Locale.ROOT,"\nPosition: %.6f, %.6f",n.lat,n.lon):"")).setPositiveButton("Back",null).show();else if(index==1){if(n.unmessagable||(p==RadioProfile.MESHCORE&&n.type!=1))Toast.makeText(this,"Choose a message-capable client contact",Toast.LENGTH_LONG).show();else openChat(p,source,Target.node(n));}else if(index==2&&service!=null)service.history.flag(p,source,"node",n.id,"favorite",!favorite);else if(action.equals("Show on map"))showMap(n.lat,n.lon);else if(action.equals("Distance and bearing"))navigateTo(nodeName(p,n),n.lat,n.lon);else if(action.equals("Metrics & radio tools"))mtNodeMetrics(n);}).setNegativeButton("Back",null).show();}
    private void mtNodeMetrics(Node saved){ProtocolClient c=mtClient();Node live=c==null?null:c.nodes.get(saved.id);Node n=live==null?saved:live;StringBuilder content=new StringBuilder("Latest reported values\n");for(Map.Entry<String,String> entry:n.metrics.entrySet())content.append("\n").append(entry.getKey()).append(": ").append(entry.getValue()).append("\n");if(n.metrics.isEmpty())content.append("No telemetry received yet.\n");content.append("\nRecent samples\n");for(int i=n.metricHistory.size()-1;i>=0;i--)content.append(n.metricHistory.get(i)).append("\n\n");TextView data=text(content.toString(),13,WHITE);data.setPadding(dp(18),dp(12),dp(18),dp(12));ScrollView scroll=new ScrollView(this);scroll.addView(data);new AlertDialog.Builder(this).setTitle(nodeName(RadioProfile.MESHTASTIC,n)).setView(scroll).setPositiveButton("Request telemetry",(d,w)->mtCommand(client->client.requestMtMetrics(n.id,false))).setNeutralButton("Traceroute",(d,w)->mtConfirm("Request a new traceroute",false,()->mtCommand(client->client.requestMtMetrics(n.id,true)))).setNegativeButton("Back",null).show();}
    void showMap(double lat,double lon){mapFocus=new org.maplibre.android.geometry.LatLng(lat,lon);page(4);}
    private void conversationOptions(RadioProfile p,String radio,Target target){Flags flags=snapshot(p).flags("chat",target.topic);new AlertDialog.Builder(this).setTitle(p.title+" · "+target.title).setItems(new String[]{flags.pinned?"Unpin conversation":"Pin conversation",flags.muted?"Unmute notifications":"Mute notifications"},(d,index)->{if(service!=null)service.history.flag(p,radio,"chat",target.topic,index==0?"pinned":"muted",index==0?!flags.pinned:!flags.muted);}).setNegativeButton("Close",null).show();}
    private void openChat(RadioProfile p,String radio,Target target){saveDraft();remember();selected=3;chatProfile=p;chatRadio=radio;chat=target;chatLimit=100;buildPage();}
    private String draftKey(){return chatProfile+"|"+chatRadio+"|"+(chat==null?"":chat.topic);}
    private void saveDraft(){handler.removeCallbacks(saveDraftTask);if(chat==null||composer==null)return;String value=composer.getText().toString();drafts.put(draftKey(),value);if(service!=null)service.history.draft(chatProfile,chatRadio,chat.topic,value);}
    private void applyDraft(String value){applyingDraft=true;composer.setText(value);applyingDraft=false;}
    private void loadDraft(){
        if(chat==null||composer==null||service==null)return;final String key=draftKey();
        if(drafts.containsKey(key)){applyDraft(drafts.get(key));return;}
        final EditText editor=composer;final int revision=composerRevision;
        service.history.loadDraft(chatProfile,chatRadio,chat.topic,value->{
            if(chat!=null&&key.equals(draftKey())&&composer==editor&&composerRevision==revision&&!drafts.containsKey(key))applyDraft(value);
        });
    }
    private void chatPage(){LinearLayout h=column();h.setPadding(dp(16),0,dp(16),dp(6));chatStatus=text(chatProfile.title,12,MUTED);h.addView(chatStatus);body.addView(h);
        scroll=new ScrollView(this);results=column();results.setPadding(dp(16),dp(12),dp(16),dp(12));scroll.addView(results);body.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        LinearLayout entry=column();entry.setPadding(dp(12),dp(6),dp(12),dp(8));entry.setBackgroundColor(PANEL);composer=new EditText(this);composer.setTextColor(WHITE);composer.setHintTextColor(MUTED);composer.setHint("Write a message…");composer.setTextSize(15);composer.setMinLines(1);composer.setMaxLines(4);composer.setInputType(android.text.InputType.TYPE_CLASS_TEXT|android.text.InputType.TYPE_TEXT_FLAG_MULTI_LINE|android.text.InputType.TYPE_TEXT_FLAG_CAP_SENTENCES);composer.setFilters(new InputFilter[]{new InputFilter.LengthFilter(1000)});entry.addView(composer,new LinearLayout.LayoutParams(-1,-2));LinearLayout controls=row();count=text("0 bytes",12,MUTED);controls.addView(count,new LinearLayout.LayoutParams(0,-2,1));sendButton=button("Send",this::send);controls.addView(sendButton,new LinearLayout.LayoutParams(dp(80),dp(48)));controls.addView(button("Replies",this::quickReply));entry.addView(controls);body.addView(entry);
        composer.addTextChangedListener(watcher(()->{composerRevision++;drafts.put(draftKey(),composer.getText().toString());updateComposer();if(!applyingDraft){handler.removeCallbacks(saveDraftTask);handler.postDelayed(saveDraftTask,500);}}));if(drafts.containsKey(draftKey()))applyDraft(drafts.get(draftKey()));loadDraft();refreshContent();scroll.post(()->scroll.fullScroll(View.FOCUS_DOWN));}
    private void updateComposer(){if(composer==null)return;int size=composer.getText().toString().getBytes(StandardCharsets.UTF_8).length;count.setText(size+" UTF-8 bytes"+(chat!=null&&chat.replyId!=0?" · replying #"+Long.toUnsignedString(chat.replyId):""));sendButton.setEnabled(!sending&&ready(chatProfile,chatRadio)&&size>0);sendButton.setText(sending?"Saving…":"Send");}
    private void send(){if(service==null||chat==null||sending)return;final String value=composer.getText().toString(),key=draftKey();saveDraft();sending=true;updateComposer();withPermission(()->service.send(chatProfile,chatRadio,chat,value,error->{sending=false;if(!error.isEmpty())Toast.makeText(this,error,Toast.LENGTH_LONG).show();else if(chat!=null&&key.equals(draftKey())&&composer!=null&&composer.getText().toString().equals(value)){composer.setText("");chat=chat.reply(0,false);saveDraft();}else if(chat!=null&&key.equals(draftKey()))saveDraft();updateComposer();scheduleRefresh();}));}
    private void messages(){
        ArrayList<Message> visibleMessages=new ArrayList<>();if(radio(chatProfile).equals(chatRadio))for(Message m:snapshot(chatProfile).messages)if(m.topic.equals(chat.topic))visibleMessages.add(m);
        if(visibleMessages.size()>chatLimit)addButton(results,"Show earlier messages",()->{chatLimit+=100;refreshContent();});
        boolean connected=ready(chatProfile,chatRadio);chatStatus.setText(chatProfile.title+" • "+(connected?"ready":"offline / synchronizing • draft stays here"));updateComposer();int shown=0;boolean unread=false;
        for(Message m:visibleMessages.subList(Math.max(0,visibleMessages.size()-chatLimit),visibleMessages.size())){shown++;LinearLayout c=card(results);c.setBackground(surface(m.outgoing?PANEL2:PANEL));TextView author=text(m.outgoing?"You":m.sender,13,m.outgoing?ORANGE:GREEN);author.setTypeface(null,Typeface.BOLD);c.addView(author);space(c,6);
            RadioProfile network=chatProfile;String sourceRadio=chatRadio;
            View.OnLongClickListener actions=v->{v.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);messageActions(network,sourceRadio,m);return true;};
            c.setOnLongClickListener(actions);author.setOnLongClickListener(actions);
            String association="";if(network==RadioProfile.MESHTASTIC&&m.rx.replyId!=0){Message original=null;int matches=0;for(Message candidate:visibleMessages)if(candidate.rx.packetId==m.rx.replyId&&!candidate.rx.emoji){original=candidate;matches++;}association=(m.rx.emoji?"Reaction to ":"Reply to ")+(matches==1?original.sender+": "+original.body.substring(0,Math.min(60,original.body.length())):"message #"+Long.toUnsignedString(m.rx.replyId))+"\n";}TextView body=text(association+MessageActions.displayBody(network,m),16,WHITE);body.setOnLongClickListener(actions);c.addView(body);
            if(!m.outgoing){Runnable details=()->messageDetails(network,sourceRadio,m);c.setOnClickListener(v->details.run());body.setOnClickListener(v->details.run());TextView reception=text(m.rx.summary()+" · Details ›",12,network==RadioProfile.MESHTASTIC?RADIO_MT:RADIO_MC);reception.setPadding(0,dp(8),0,dp(4));reception.setOnClickListener(v->details.run());reception.setOnLongClickListener(actions);c.addView(reception);}
            double[] coordinates=FieldTools.coordinates(m.body);if(coordinates!=null){TextView link=text("◇ Show coordinates on map",13,ORANGE);link.setPadding(0,dp(8),0,dp(8));link.setOnClickListener(v->showMap(coordinates[0],coordinates[1]));link.setOnLongClickListener(actions);c.addView(link);}space(c,8);TextView stamp=text(when(m.timestamp)+(m.outgoing?" • "+deliveryLabel(m.state):""),11,m.state.equals("delivered")?GREEN:m.state.equals("rejected")?RED:MUTED);stamp.setOnLongClickListener(actions);c.addView(stamp);if(!m.read&&!m.outgoing)unread=true;}
        if(shown==0)results.addView(text("No messages in this conversation yet. Your draft is saved automatically.",14,MUTED));
        if(unread&&visible&&service!=null)service.history.read(chatProfile,chatRadio,chat.topic);
    }
    /** Plain-text drafts only; a menu action never sends a packet automatically. */
    private void insertMessageDraft(String value){
        if(composer==null||chat==null||value==null||value.isEmpty())return;
        int caret=composer.getSelectionStart();if(caret<0)caret=composer.length();
        boolean needsSpace=caret>0&&!Character.isWhitespace(composer.getText().charAt(caret-1));
        String insertion=(needsSpace?" ":"")+value+" ";
        composer.getText().insert(caret,insertion);composer.setSelection(caret+insertion.length());
        composer.requestFocus();
        android.view.inputmethod.InputMethodManager keyboard=(android.view.inputmethod.InputMethodManager)getSystemService(INPUT_METHOD_SERVICE);
        if(keyboard!=null)keyboard.showSoftInput(composer,android.view.inputmethod.InputMethodManager.SHOW_IMPLICIT);
    }

    /**
     * A radio-scoped exact ID is preferred; a displayed name is accepted only
     * when exactly one saved contact matches. Channel names are not authenticated.
     */
    private Node messageSenderNode(RadioProfile network,String sourceRadio,Message message){
        if(message.outgoing||!sourceRadio.equals(radio(network)))return null;
        List<Node> nodes=snapshot(network).nodes;if(network==RadioProfile.MESHTASTIC&&message.rx.source>0){for(Node node:nodes)if(node.id.equals(Long.toString(message.rx.source)))return node;return null;}
        String peer=message.peer==null?"":message.peer;
        if(!peer.isEmpty()){
            for(Node node:nodes)if(node.id.equalsIgnoreCase(peer))return node;
            Node partial=null;int matches=0;
            for(Node node:nodes)if(node.id.startsWith(peer)){partial=node;matches++;}
            if(matches==1)return partial;
            if(matches>1)return null;
        }
        String sender=message.sender==null?"":message.sender.trim();
        if(sender.isEmpty()||sender.equalsIgnoreCase("Channel participant"))return null;
        Node found=null;int matches=0;
        for(Node node:nodes)if(!node.name.isEmpty()&&node.name.equalsIgnoreCase(sender)){found=node;matches++;}
        return matches==1?found:null;
    }

    private void favoriteMessageSender(RadioProfile network,String sourceRadio,Message message,Node node){
        if(service==null||!sourceRadio.equals(radio(network)))return;
        boolean saved=snapshot(network).flags("node",node.id).favorite;
        String description=(message.group?"This is a displayed-name match, not proof of the sender's radio identity. Verify the node before saving.\n\n":"")
                +network.title+"\n"+nodeName(network,node)+"\n"+node.id;
        new AlertDialog.Builder(this).setTitle(saved?"Remove favorite contact?":"Favorite this contact?")
                .setMessage(description)
                .setPositiveButton(saved?"Remove favorite":"Add favorite",
                    (dialog,button)->service.history.flag(network,sourceRadio,"node",node.id,"favorite",!saved))
                .setNegativeButton("Cancel",null).show();
    }

    private void copyMessageText(String label,String content){
        android.content.ClipboardManager clip=(android.content.ClipboardManager)getSystemService(CLIPBOARD_SERVICE);
        if(clip!=null){clip.setPrimaryClip(ClipData.newPlainText(label,content));Toast.makeText(this,"Copied",Toast.LENGTH_SHORT).show();}
    }

    private void shareMessageText(String content){
        Intent share=new Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT,content);
        startActivity(Intent.createChooser(share,"Share message"));
    }

    private void messageActions(RadioProfile network,String sourceRadio,Message message){
        String visibleText=MessageActions.displayBody(network,message);
        String mention=MessageActions.mention(message.sender);
        Node contact=messageSenderNode(network,sourceRadio,message);
        ArrayList<String> choices=new ArrayList<>();
        ArrayList<Runnable> tasks=new ArrayList<>();
        if(network==RadioProfile.MESHTASTIC&&message.rx.packetId!=0&&!message.rx.emoji){choices.add("Reply to this message");tasks.add(()->{if(chat!=null&&chatProfile==network&&chatRadio.equals(sourceRadio)&&chat.topic.equals(message.topic)){chat=chat.reply(message.rx.packetId,false);updateComposer();composer.requestFocus();}});choices.add("React to this message");tasks.add(()->{String[] emojis={"👍","❤️","✅","❓"};new AlertDialog.Builder(this).setTitle("Reaction").setItems(emojis,(d,i)->{Target target=message.target().reply(message.rx.packetId,true);if(service!=null)service.send(network,sourceRadio,target,emojis[i],error->{if(!error.isEmpty())Toast.makeText(this,error,Toast.LENGTH_LONG).show();scheduleRefresh();});}).setNegativeButton("Cancel",null).show();});}
        if(network==RadioProfile.MESHTASTIC&&chat!=null&&chat.replyId!=0){choices.add("Cancel reply");tasks.add(()->{chat=chat.reply(0,false);updateComposer();});}

        if(!message.outgoing&&message.group&&!mention.isEmpty()){
            choices.add("Reply mentioning sender");
            tasks.add(()->insertMessageDraft(mention));
        }
        if(!message.outgoing){
            choices.add("Reply with reception (ACK)");
            tasks.add(()->insertMessageDraft(MessageActions.receptionReply(message)));
        }
        choices.add("Quote message in reply");
        tasks.add(()->{
            String compact=visibleText.replace('\n',' ').replace('\r',' ').trim();
            if(compact.length()>96)compact=compact.substring(0,96)+"…";
            String address=!message.outgoing&&message.group?mention:"";
            insertMessageDraft((address.isEmpty()?"":address+" ")+"“"+compact+"”");
        });
        choices.add("Copy message");
        tasks.add(()->copyMessageText("MeshCrossway message",visibleText));

        if(!message.outgoing&&MessageActions.hasReception(message.rx)){
            choices.add("Copy reception information");
            tasks.add(()->copyMessageText("LoRa reception",message.rx.summary()));
        }
        choices.add("Message information");
        tasks.add(()->{
            if(message.outgoing)new AlertDialog.Builder(this).setTitle("Sent message")
                    .setMessage(when(message.timestamp)+"\n"+deliveryLabel(message.state))
                    .setPositiveButton("Close",null).show();
            else messageDetails(network,sourceRadio,message);
        });
        if(contact!=null){
            choices.add(snapshot(network).flags("node",contact.id).favorite?"Remove sender from favorites":"Add sender to favorites");
            tasks.add(()->favoriteMessageSender(network,sourceRadio,message,contact));
            choices.add("Sender contact details");
            tasks.add(()->nodeDetails(network,contact));
        }
        double[] coordinates=FieldTools.coordinates(message.body);
        if(coordinates!=null){
            choices.add("Show coordinates on map");
            tasks.add(()->showMap(coordinates[0],coordinates[1]));
        }
        choices.add("Share message");
        tasks.add(()->shareMessageText(visibleText));
        new AlertDialog.Builder(this).setTitle(message.outgoing?"Your message":network.title+" · "+message.sender)
                .setItems(choices.toArray(new String[0]),(dialog,index)->tasks.get(index).run())
                .setNegativeButton("Cancel",null).show();
    }

    private void messageDetails(RadioProfile network,String sourceRadio,Message message){RxInfo rx=message.rx;LinearLayout details=column();details.setPadding(dp(20),dp(12),dp(20),dp(16));details.addView(text(network.title+" · "+message.sender,17,WHITE));details.addView(text(when(message.timestamp),12,MUTED));space(details,12);details.addView(text(rx.summary(),14,network==RadioProfile.MESHTASTIC?RADIO_MT:RADIO_MC));space(details,14);details.addView(text("Received path",17,WHITE));
        if(rx.observed){details.addView(text("Observed channel copy · "+rx.width+"-byte repeater hashes",12,MUTED));details.addView(text("Sender: "+message.sender,14,WHITE));for(int i=0;i<rx.path.length();i+=rx.width*2){String hash=rx.path.substring(i,i+rx.width*2);ArrayList<Node> matches=new ArrayList<>();if(radio(network).equals(sourceRadio))for(Node node:snapshot(network).nodes)if(node.id.startsWith(hash))matches.add(node);String name=matches.isEmpty()?"Unknown repeater":matches.size()==1?(matches.get(0).name.isEmpty()?"Saved contact":matches.get(0).name):"Ambiguous · "+matches.size()+" saved contacts";TextView hop=text((i/(rx.width*2)+1)+". "+name+"  ["+hash+"]",14,WHITE);hop.setPadding(0,dp(6),0,dp(6));details.addView(hop);}if(rx.path.isEmpty())details.addView(text("No repeaters in this observed flood path",14,WHITE));details.addView(text("Receiver: this radio",14,WHITE));space(details,12);details.addView(text("Names are matches to saved contacts, not unique proof of identity. This is one observed copy; repeats may take other routes.",12,MUTED));}
        else details.addView(text(network==RadioProfile.MESHCORE?"The radio reports the hop count for flood messages, but this message has no captured full path. Older backlog, direct routing and private messages may not include repeater identities.":"Meshtastic text packets do not contain a complete repeater list. A new traceroute would measure a new route, not the path this message travelled.",14,MUTED));
        if(network==RadioProfile.MESHTASTIC&&rx.relay>=0&&rx.relay<=255){String hash=String.format(Locale.ROOT,"%02x",rx.relay);ArrayList<String> names=new ArrayList<>();if(radio(network).equals(sourceRadio))for(Node node:snapshot(network).nodes)if((Long.parseLong(node.id)&255)==rx.relay)names.add(nodeName(network,node));space(details,10);details.addView(text("Reported relay suffix: "+hash+(names.size()==1?" · "+names.get(0):names.size()>1?" · ambiguous ("+names.size()+" contacts)":" · unknown contact"),13,MUTED));}if(rx.mqtt)details.addView(text("MQTT was used somewhere along this packet's route.",13,MUTED));ScrollView scroll=new ScrollView(this);scroll.addView(details);new AlertDialog.Builder(this).setTitle("Message details").setView(scroll).setPositiveButton("Back",null).show();}
    static String deliveryLabel(String state){switch(state){case "queued":return "Saved / queued";case "submitted":return "Submitted over Bluetooth";case "radio accepted":return "Accepted by radio";case "delivered":return "Recipient acknowledged";case "rejected":return "Rejected";case "uncertain":return "Delivery unknown • no automatic resend";default:return state;}}
    private void settings(){
        if(settingsSection.isEmpty()){
            section(results,"RADIOS");LinearLayout radios=group(results);for(RadioProfile p:RadioProfile.values())menuRow(radios,"radio",p.title,ready(p,radio(p))?"Connected · manage this radio":"Connection and saved device",()->openSettings("radio",p),p==RadioProfile.MESHTASTIC?RADIO_MT:RADIO_MC);menuRow(radios,"settings","Connections","Automatic reconnect and Bluetooth",()->openSettings("connections",null),ORANGE);
            section(results,"APPLICATION");LinearLayout app=group(results);menuRow(app,"settings","Appearance & colors","System, Color, Gradient or Photo background",()->openSettings("appearance",null),ORANGE);menuRow(app,"bell","Notifications & messages","Message previews and quick replies",()->openSettings("messages",null),ORANGE);menuRow(app,"map","Map & location","Map layers, offline areas, GPS and tracks",()->openSettings("location",null),ORANGE);menuRow(app,"backup","Data & backup","Export and import private history",()->openSettings("data",null),ORANGE);
            section(results,"INFORMATION");LinearLayout info=group(results);menuRow(info,"info","About MeshCrossway","Version "+installedVersion()+" · licenses and map attribution",()->openSettings("about",null),ORANGE);return;
        }
        if(settingsSection.equals("radio")){radioPage();return;}
        if(settingsSection.startsWith("mc_")){meshCoreSettingsDetail();return;}
        if(settingsSection.startsWith("mt_")){meshtasticSettingsDetail();return;}
        if(settingsSection.equals("appearance")){appearancePage();return;}
        if(settingsSection.equals("connections")){section(results,"CONNECTIONS");LinearLayout c=group(results);toggleRow(c,"Automatic reconnection","Reconnect each saved radio independently",service!=null&&service.autoReconnect,on->{if(service!=null)service.setAutoReconnect(on);});menuRow(c,"radio","Connect saved radios","Start both saved connections",()->withPermission(()->{service.connectSaved();rebuildContent();}),ORANGE);menuRow(c,"settings","Android Bluetooth settings","System pairing and paired devices",()->startActivity(new Intent(Settings.ACTION_BLUETOOTH_SETTINGS)),ORANGE);menuRow(c,"radio","Disconnect both radios","Stops radio connections; track recording is separate",()->new AlertDialog.Builder(this).setTitle("Disconnect both radios?").setPositiveButton("Disconnect",(d,w)->{if(service!=null)startService(new Intent(this,RadioService.class).setAction("STOP"));rebuildContent();}).setNegativeButton("Cancel",null).show(),RED);return;}
        if(settingsSection.equals("messages")){section(results,"NOTIFICATIONS");LinearLayout c=group(results);toggleRow(c,"Message previews","Show message contents in notifications",getSharedPreferences("ui",MODE_PRIVATE).getBoolean("messagePreviews",false),on->getSharedPreferences("ui",MODE_PRIVATE).edit().putBoolean("messagePreviews",on).apply());menuRow(c,"bell","Android notification settings","Sound, vibration and notification permission",()->startActivity(new Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS).putExtra(Settings.EXTRA_APP_PACKAGE,getPackageName())),ORANGE);section(results,"MESSAGES");menuRow(group(results),"chats","Quick replies","Edit the phrases available in conversations",this::editQuickReplies,ORANGE);return;}
        if(settingsSection.equals("location")){section(results,"PHONE LOCATION");LinearLayout c=group(results);toggleRow(c,"Use phone location","Show your position on the map and calculate distances",phoneEnabled(),on->{if(on)requestPhonePosition();else{getSharedPreferences("ui",MODE_PRIVATE).edit().putBoolean("gpsEnabled",false).apply();position.stop();position.location=null;}rebuildContent();});if(position.location!=null&&phoneEnabled())c.addView(text(String.format(Locale.ROOT,"  %.5f, %.5f · ±%.0f m",position.location.getLatitude(),position.location.getLongitude(),position.location.getAccuracy()),13,MUTED));section(results,"MAP & FIELD TOOLS");LinearLayout tools=group(results);menuRow(tools,"map","Map layers","Basemap, network layers and labels",()->openMapTool("layers"),ORANGE);menuRow(tools,"backup","Offline maps","Download or manage saved areas",()->openMapTool("offline"),ORANGE);menuRow(tools,"tracks","Tracks","Record, manage and export GPX",this::manageTracks,ORANGE);menuRow(tools,"gps","Points of interest","Saved local places",()->openMapTool("pois"),ORANGE);return;}
        if(settingsSection.equals("data")){
            section(results,"COMPLETE DEVICE BACKUP");
            LinearLayout backup=group(results);
            menuRow(backup,"backup","Export complete backup","One ZIP: messages, favorites, radio selections, settings and every GPS track/folder",this::exportFullBackup,ORANGE);
            menuRow(backup,"backup","Restore complete backup","Merge a saved ZIP into this phone without deleting existing data",this::importFullBackup,ORANGE);
            TextView note=text("Keep your backup ZIP outside the app (Files or secure storage). It contains private messages and locations and is NOT encrypted. Offline map tiles and Android permission grants cannot be restored from the ZIP.",13,MUTED);
            note.setPadding(dp(4),dp(8),dp(4),0);results.addView(note);
            section(results,"INDIVIDUAL EXPORTS");
            LinearLayout individual=group(results);
            menuRow(individual,"backup","Export history JSON","Messages, contacts, drafts, favorites and POIs",this::exportHistory,ORANGE);
            menuRow(individual,"backup","Import history JSON","Merge a saved history backup",()->startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT).setType("application/json").addCategory(Intent.CATEGORY_OPENABLE),41),ORANGE);
            menuRow(individual,"tracks","Export an individual GPX track","Your track recordings remain on the phone during normal updates",this::manageTracks,ORANGE);
            menuRow(individual,"tracks","Import GPX track","Restore GPX files exported by earlier incompatible APKs",()->
                startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT)
                    .setType("*/*").addCategory(Intent.CATEGORY_OPENABLE),52),ORANGE);
            results.addView(text("Normal signed updates keep all app data. Uninstalling erases the private app database; use Export complete backup BEFORE uninstalling a differently signed APK.",13,MUTED));
            return;
        }
        section(results,"MESHCROSSWAY");LinearLayout c=group(results);menuRow(c,"info","Android "+installedVersion(),"Two independent networks. No automatic forwarding.",null,ORANGE);menuRow(c,"info","Software licenses","MapLibre Native and bundled software",this::showMapLicenses,ORANGE);menuRow(c,"map","Map attribution","OpenFreeMap · OpenMapTiles · OpenStreetMap",()->new AlertDialog.Builder(this).setTitle("Map attribution").setMessage("© OpenStreetMap contributors · OpenMapTiles · OpenFreeMap. Basemaps use the internet or saved areas.").setPositiveButton("OpenFreeMap",(d,w)->startActivity(new Intent(Intent.ACTION_VIEW,android.net.Uri.parse("https://openfreemap.org/")))).setNegativeButton("Back",null).show(),ORANGE);
    }

    private void chooseAppearanceMode(String mode){
        if(Appearance.PHOTO.equals(mode)&&!appearance.hasPhoto){
            chooseAppearancePhoto();return;
        }
        Appearance.setMode(this,mode);
        recreate();
    }
    private void chooseAppearancePhoto(){
        Intent document=new Intent(Intent.ACTION_OPEN_DOCUMENT);
        document.setType("image/*");
        document.addCategory(Intent.CATEGORY_OPENABLE);
        startActivityForResult(document,53);
    }
    private void appearanceOption(LinearLayout parent,String mode,String name,String subtitle){
        boolean active=appearance.mode.equals(mode);
        LinearLayout tile=column();
        tile.setPadding(dp(10),dp(10),dp(10),dp(10));
        GradientDrawable border=surface(PANEL);
        border.setStroke(dp(active?2:1),active?ORANGE:PANEL2);
        tile.setBackground(border);
        android.widget.FrameLayout preview=new android.widget.FrameLayout(this);
        preview.setBackground(Appearance.previewBackground(mode,appearance.hue,appearance.gradientType));
        if(Appearance.PHOTO.equals(mode)&&appearance.hasPhoto){
            android.graphics.Bitmap image=appearance.loadPhoto(this);
            if(image!=null){
                android.widget.ImageView picture=new android.widget.ImageView(this);
                picture.setImageBitmap(image);
                picture.setScaleType(android.widget.ImageView.ScaleType.CENTER_CROP);
                preview.addView(picture,new android.widget.FrameLayout.LayoutParams(-1,-1));
                View veil=new View(this);
                veil.setBackgroundColor(0x88000000);
                preview.addView(veil,new android.widget.FrameLayout.LayoutParams(-1,-1));
            }
        }
        TextView miniature=text("●  ▰  ●\n━━━━━━",16,
            Appearance.SYSTEM.equals(mode)&&appearance.colors.light?0xfff2f4f7:0xfff2f4f7);
        miniature.setGravity(Gravity.CENTER);
        miniature.setTypeface(null,Typeface.BOLD);
        preview.addView(miniature,new android.widget.FrameLayout.LayoutParams(-1,-1));
        tile.addView(preview,new LinearLayout.LayoutParams(-1,dp(70)));
        space(tile,10);
        TextView label=text((active?"✓  ":"")+name,15,WHITE);
        label.setTypeface(null,Typeface.BOLD);
        tile.addView(label);
        tile.addView(text(subtitle,11,MUTED));
        tile.setFocusable(true);
        tile.setContentDescription(name+" theme"+(active?", selected":"")+". "+subtitle);
        tile.setOnClickListener(v->chooseAppearanceMode(mode));
        parent.addView(tile,new LinearLayout.LayoutParams(0,-2,1));
    }
    private void appearanceSlider(LinearLayout parent,String label,String detail,
                                  int initial,int maximum,
                                  java.util.function.IntConsumer saved){
        LinearLayout wrapper=column();
        wrapper.setPadding(dp(14),dp(12),dp(14),dp(12));
        TextView title=text(label,15,WHITE);
        title.setTypeface(null,Typeface.BOLD);
        wrapper.addView(title);
        wrapper.addView(text(detail,12,MUTED));
        TextView value=text(String.valueOf(initial),12,ORANGE);
        wrapper.addView(value);
        SeekBar slider=new SeekBar(this);
        slider.setMax(maximum);
        slider.setProgress(initial);
        slider.setProgressTintList(android.content.res.ColorStateList.valueOf(ORANGE));
        slider.setThumbTintList(android.content.res.ColorStateList.valueOf(ORANGE));
        slider.setContentDescription(label);
        slider.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){
            public void onStartTrackingTouch(SeekBar seek){}
            public void onProgressChanged(SeekBar seek,int progress,boolean fromUser){
                value.setText(String.valueOf(progress));
            }
            public void onStopTrackingTouch(SeekBar seek){
                int changed=seek.getProgress();
                if(changed!=initial){saved.accept(changed);recreate();}
            }
        });
        wrapper.addView(slider);
        parent.addView(wrapper);
    }
    private void appearancePage(){
        section(results,"DISPLAY STYLE");
        results.addView(text("Choose how MeshCrossway looks. Your selection is saved on this phone and survives normal app updates.",13,MUTED));
        space(results,12);
        LinearLayout top=row();top.setGravity(Gravity.TOP);
        appearanceOption(top,Appearance.SYSTEM,"System","Light / dark");
        appearanceOption(top,Appearance.COLOR,"Color","Single hue");
        results.addView(top);
        space(results,7);
        LinearLayout bottom=row();bottom.setGravity(Gravity.TOP);
        appearanceOption(bottom,Appearance.GRADIENT,"Gradient","Linear / mesh");
        appearanceOption(bottom,Appearance.PHOTO,"Photo","Dim & blur");
        results.addView(bottom);
        space(results,12);
        if(Appearance.COLOR.equals(appearance.mode)||Appearance.GRADIENT.equals(appearance.mode)){
            section(results,"COLOR");
            LinearLayout settings=group(results);
            appearanceSlider(settings,"Hue","Slide to choose any color (0–359°).",
                appearance.hue,359,value->Appearance.setHue(this,value));
            section(results,"QUICK COLORS");
            LinearLayout colors=row();
            for(int hue:new int[]{28,195,255,145,285,345}){
                LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(0,dp(44),1);
                lp.setMargins(dp(3),0,dp(3),0);
                android.widget.FrameLayout swatch=new android.widget.FrameLayout(this);
                GradientDrawable surface=surface(Appearance.hueColor(hue,.84f,.84f));
                surface.setStroke(dp(appearance.hue==hue?3:1),
                    appearance.hue==hue?WHITE:PANEL2);
                swatch.setBackground(surface);
                swatch.setContentDescription("Hue "+hue);
                swatch.setFocusable(true);
                swatch.setOnClickListener(v->{Appearance.setHue(this,hue);recreate();});
                colors.addView(swatch,lp);
            }
            results.addView(colors);
            if(Appearance.GRADIENT.equals(appearance.mode)){
                section(results,"GRADIENT STYLE");
                LinearLayout types=row();
                types.addView(chip("Linear",Appearance.LINEAR.equals(appearance.gradientType),
                    ()->{Appearance.setGradient(this,Appearance.LINEAR);recreate();}),
                    new LinearLayout.LayoutParams(0,dp(46),1));
                types.addView(chip("Mesh",Appearance.MESH.equals(appearance.gradientType),
                    ()->{Appearance.setGradient(this,Appearance.MESH);recreate();}),
                    new LinearLayout.LayoutParams(0,dp(46),1));
                results.addView(types);
            }
        }else if(Appearance.PHOTO.equals(appearance.mode)){
            section(results,"YOUR BACKGROUND");
            LinearLayout group=group(results);
            menuRow(group,"settings","Choose photo","Pick a picture from your phone; stored privately for future updates",
                this::chooseAppearancePhoto,ORANGE);
            appearanceSlider(group,"Dim","Darken the photo so message text remains readable (30–90%).",
                appearance.dim,90,value->Appearance.setDim(this,value));
            appearanceSlider(group,"Blur","Soften the photo behind the interface (0–24).",
                appearance.blur,24,value->Appearance.setBlur(this,value));
            menuRow(group,"settings","Remove background photo","Return to System appearance",
                ()->{Appearance.setMode(this,Appearance.SYSTEM);
                    Appearance.photoFile(this).delete();recreate();},MUTED);
        }else{
            section(results,"SYSTEM COLORS");
            LinearLayout info=group(results);
            menuRow(info,"settings",appearance.colors.light?"Light appearance":"Dark appearance",
                "Follows the Android system appearance automatically",null,ORANGE);
        }
        section(results,"NETWORK IDENTITIES");
        results.addView(text("Meshtastic stays orange and MeshCore stays blue in radio indicators and map nodes. Appearance affects the surrounding app interface, not network identity.",13,MUTED));
    }
    private void radioPage(){RadioProfile p=settingsRadio==null?RadioProfile.MESHCORE:settingsRadio;RadioService.Session session=service==null?null:service.sessions.get(p);section(results,p.title.toUpperCase(Locale.ROOT));LinearLayout info=group(results);TextView name=text(session!=null&&!session.name.isEmpty()?session.name:"No radio selected",18,WHITE);name.setTypeface(null,Typeface.BOLD);name.setPadding(dp(16),dp(14),dp(16),dp(4));info.addView(name);TextView status=text(session==null?"Controller starting":session.status,13,session!=null&&session.verified?GREEN:MUTED);status.setPadding(dp(16),dp(4),dp(16),dp(14));info.addView(status);
        if(choosing!=null){section(results,"CHOOSE RADIO");LinearLayout scan=group(results);menuRow(scan,"radio",service!=null&&service.scanning?"Stop scan":"Scan nearby radios",service==null?"Starting":service.scanStatus,()->withPermission(()->{if(service.scanning)service.stopScan();else service.scan(p);rebuildContent();}),ORANGE);if(service!=null)for(BluetoothDevice device:service.discovered(p)){String address=device.getAddress();menuRow(scan,"radio",service.deviceName(device),service.discoveryLabel(device),()->withPermission(()->{service.connect(p,address);choosing=null;rebuildContent();}),p==RadioProfile.MESHTASTIC?RADIO_MT:RADIO_MC);}menuRow(scan,"settings",service!=null&&service.showOtherDevices?"Show matching radios":"Show other BLE devices","",()->{if(service!=null)service.showOtherDevices=!service.showOtherDevices;rebuildContent();},MUTED);return;}
        if(p==RadioProfile.MESHCORE)meshCoreSettingsRows(session);
        else {meshtasticSettingsRows(session);section(results,"CONNECTION");LinearLayout mtActions=group(results);menuRow(mtActions,"radio","Choose radio","Find a different Meshtastic device",()->withPermission(()->{choosing=p;service.scan(p);buildPage();}),RADIO_MT);if(session!=null&&!session.address.isEmpty())menuRow(mtActions,"radio",session.connected?"Disconnect":"Reconnect / synchronize",session.address,()->{if(session.connected)service.disconnect(p);else withPermission(()->service.connect(p,session.address));rebuildContent();},session.connected?RED:RADIO_MT);return;}
        section(results,"CONNECTION");LinearLayout actions=group(results);menuRow(actions,"radio","Choose radio","Find a different "+p.title+" device",()->withPermission(()->{choosing=p;service.scan(p);buildPage();}),ORANGE);if(session!=null&&!session.address.isEmpty()){menuRow(actions,"radio","Reconnect / synchronize",session.address,()->withPermission(()->{service.connect(p,session.address);rebuildContent();}),ORANGE);if(session.connected||session.desired)menuRow(actions,"radio",session.connected?"Disconnect":"Cancel connection","Only this radio is affected",()->{service.disconnect(p);rebuildContent();},RED);}
        section(results,"ADVANCED");LinearLayout advanced=group(results);menuRow(advanced,"info","Connection details","Connection phases, MTU and diagnostics",()->{if(service!=null)new AlertDialog.Builder(this).setTitle(p.title).setMessage(service.diagnostics(p)).setPositiveButton("Back",null).show();},MUTED);if(session!=null&&!session.address.isEmpty())menuRow(advanced,"settings","Pair in Android","Open the system pairing process",()->withPermission(()->service.pair(p)),MUTED);
    }
    private ProtocolClient mtClient(){RadioService.Session s=service==null?null:service.sessions.get(RadioProfile.MESHTASTIC);return s==null?null:s.client;}
    private void mtCommand(java.util.function.Consumer<ProtocolClient> action){RadioService.Session s=service==null?null:service.sessions.get(RadioProfile.MESHTASTIC);if(s==null||!s.verified){Toast.makeText(this,"Connect and synchronize the Meshtastic radio first",Toast.LENGTH_LONG).show();return;}service.configureMeshtastic(s.address,action);scheduleRefresh();}
    private void meshtasticSettingsRows(RadioService.Session session){
        ProtocolClient c=mtClient();boolean online=session!=null&&session.verified;if(c!=null&&c.ready&&!c.mtAdminRequested){c.mtAdminRequested=true;mtCommand(ProtocolClient::requestMtSessionKey);}
        section(results,"CONFIGURATION");LinearLayout conf=group(results);
        menuRow(conf,"info","User",c!=null&&!c.mtOwnerIsEmpty()?c.mtOwnerName():"Node identity and status message",()->openSettings("mt_user",RadioProfile.MESHTASTIC),RADIO_MT);
        menuRow(conf,"radio","LoRa",c==null?"Region, preset and radio behavior":c.regionName()+" · hop limit "+c.hopLimit,()->openSettings("mt_lora",RadioProfile.MESHTASTIC),RADIO_MT);
        menuRow(conf,"channels","Channels",c==null?"Primary and secondary channels":c.channels.size()+" synchronized channel slots",()->openSettings("mt_channels",RadioProfile.MESHTASTIC),RADIO_MT);
        menuRow(conf,"info","Security","Packet authenticity and key management",()->openSettings("mt_security",RadioProfile.MESHTASTIC),RADIO_MT);
        menuRow(conf,"settings","Device configuration","Device, Position, Power, Network, Display, Bluetooth",()->openSettings("mt_device",RadioProfile.MESHTASTIC),RADIO_MT);
        menuRow(conf,"settings","Module configuration","MQTT, telemetry, canned messages and more",()->openSettings("mt_modules",RadioProfile.MESHTASTIC),RADIO_MT);
        menuRow(conf,"settings","Administration","Admin keys, logs and managed-device controls",()->openSettings("mt_admin",RadioProfile.MESHTASTIC),RADIO_MT);
        section(results,"BACKUP & RESTORE");LinearLayout backup=group(results);menuRow(backup,"backup","Import configuration","Restore a Meshtastic configuration file",()->Toast.makeText(this,"Configuration import is not supported by this radio transport yet",Toast.LENGTH_LONG).show(),MUTED);menuRow(backup,"backup","Export configuration","Save a Meshtastic configuration file",()->Toast.makeText(this,"Configuration export is not supported by this radio transport yet",Toast.LENGTH_LONG).show(),MUTED);
        if(!online)results.addView(text("Connect to the Meshtastic radio to read and change its settings.",13,MUTED));
    }
    private void meshtasticSettingsDetail(){ProtocolClient c=mtClient();if(c==null){results.addView(text("Connect to the Meshtastic radio to load this configuration.",14,MUTED));return;}mtAdminStatus(c);switch(settingsSection){case "mt_user":mtUserSettings(c);return;case "mt_lora":mtLoraSettings(c);return;case "mt_channels":mtChannelSettings(c);return;case "mt_device":mtDeviceSettings(c);return;case "mt_security":mtSecuritySettings(c);return;case "mt_modules":mtModuleSettings(c);return;case "mt_admin":section(results,"ADMINISTRATION");results.addView(text("Admin-key management and device logs are not yet available through MeshCrossway.",14,MUTED));return;}}
    private void mtAdminStatus(ProtocolClient c){LinearLayout info=group(results);if(c.admin.edit!=null&&c.admin.edit.deadline==Long.MAX_VALUE)menuRow(info,"settings","Discard pending edit","Keep the values last read from the radio",()->mtCommand(client->client.admin.discard()),RADIO_MT);menuRow(info,"info","Radio settings",!c.admin.error.isEmpty()?c.admin.error:!c.admin.status.isEmpty()?c.admin.status:"Firmware "+c.firmware,null,c.admin.error.isEmpty()?RADIO_MT:RED);menuRow(info,"refresh","Refresh from radio","Read the device again; a submitted save is never resent",()->mtCommand(client->{client.admin.retryReads();if(client.admin.edit!=null&&client.admin.edit.dispatched)client.admin.verify();else{client.requestMtSessionKey();if(settingsSection.equals("mt_user")){client.requestMtOwner();if(client.mtModuleSupported(13))client.requestMtModule(13);}else if(settingsSection.equals("mt_lora"))client.requestMtConfig(5);else if(settingsSection.equals("mt_security"))client.requestMtConfig(7);else if(settingsSection.equals("mt_modules"))client.requestMtModule(0);else if(settingsSection.equals("mt_device")){client.requestMtConfig(0);client.requestMtConfig(6);}}}),RADIO_MT);}
    private boolean mtLoaded(ProtocolClient c,boolean loaded,Runnable request){if(!loaded){request.run();results.addView(text("Reading settings from radio. Controls become available after synchronization.",14,MUTED));return false;}if(c.admin.edit!=null){results.addView(text(c.admin.status,14,MUTED));return false;}return true;}
    private void mtUserSettings(ProtocolClient c){section(results,"USER CONFIG");if(!mtLoaded(c,!c.mtOwnerIsEmpty(),()->mtCommand(ProtocolClient::requestMtOwner)))return;LinearLayout info=group(results);menuRow(info,"info","Node ID","!"+String.format(Locale.ROOT,"%08x",c.myNum),null,RADIO_MT);menuRow(info,"info","Long Name",c.mtOwnerValue(2),()->editMtOwner(c,false),RADIO_MT);menuRow(info,"info","Short Name",c.mtOwnerValue(3),()->editMtOwner(c,true),RADIO_MT);menuRow(info,"info","Hardware model",c.hardware,null,RADIO_MT);
        if(c.mtAtLeast(6,9))toggleRow(info,"Unmessageable","Unmonitored or infrastructure",c.mtOwnerNumber(9)!=0,on->mtConfirm("Unmessageable",false,()->mtCommand(client->client.setMtOwnerField(9,on?1:0))));
        menuRow(info,"info","Licensed amateur radio (Ham)",c.mtHam()?"Enabled":"Disabled",()->{if(c.mtHam())mtConfirm("Disable licensed mode",false,()->mtCommand(client->client.setMtOwnerField(6,0)));else editMtHam(c);},RADIO_MT);
        if(c.mtModuleSupported(13)){if(!c.mtModuleLoaded(13))mtCommand(client->client.requestMtModule(13));else menuRow(info,"info","Status Message",c.mtStatus().isEmpty()?"Not set":c.mtStatus(),()->editMtStatus(c),RADIO_MT);}}
    private void mtConfirm(String title,boolean restart,Runnable save){new AlertDialog.Builder(this).setTitle(title).setMessage(restart?"Save this change? The radio may restart; MeshCrossway will verify its settings after reconnection.":"Save this change to the radio? The result will be checked by reading it back.").setPositiveButton("Save",(d,w)->save.run()).setNegativeButton("Discard",(d,w)->rebuildContent()).show();}
    private void editMtHam(ProtocolClient c){LinearLayout form=column();form.setPadding(dp(20),dp(8),dp(20),dp(8));EditText call=mcInput(form,"Licensed callsign (3–7 characters)","",android.text.InputType.TYPE_CLASS_TEXT|android.text.InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS);EditText name=mcInput(form,"Optional long name","",android.text.InputType.TYPE_CLASS_TEXT);AlertDialog dialog=new AlertDialog.Builder(this).setTitle("Enable licensed mode").setMessage("The radio will remove channel encryption keys and restart. Enter your callsign to continue.").setView(form).setPositiveButton("Save & restart",null).setNegativeButton("Discard",null).create();dialog.setOnShowListener(v->dialog.getButton(-1).setOnClickListener(w->{try{String callsign=call.getText().toString().trim().toUpperCase(Locale.ROOT),longName=name.getText().toString().trim();if(!callsign.matches("[A-Z0-9/]{3,7}"))throw Wire.bad("Enter a valid callsign");mtCommand(client->client.setMtHam(callsign,longName));dialog.dismiss();}catch(IllegalArgumentException e){call.setError(e.getMessage());}}));dialog.show();}
    private void editMtStatus(ProtocolClient c){editMtText("Status Message",c.mtStatus(),80,true,value->mtCommand(client->client.setMtStatus(value)));}
    private void editMtOwner(ProtocolClient c,boolean shortName){editMtText(shortName?"Short Name":"Long Name",c.mtOwnerValue(shortName?3:2),shortName?4:c.mtAtLeast(8,0)?24:39,false,value->mtCommand(client->client.setMtOwnerName(shortName,value)));}
    private void editMtText(String title,String current,int max,boolean empty,java.util.function.Consumer<String> save){EditText input=new EditText(this);input.setSingleLine(true);input.setText(current);input.setTextColor(WHITE);AlertDialog dialog=new AlertDialog.Builder(this).setTitle(title).setView(input).setNegativeButton("Discard",null).setPositiveButton("Save",null).create();dialog.setOnShowListener(v->dialog.getButton(-1).setOnClickListener(w->{try{String value=input.getText().toString().trim();if(empty)Wire.optionalText(value,max);else Wire.text(value,max);save.accept(value);dialog.dismiss();}catch(IllegalArgumentException e){input.setError(e.getMessage());}}));dialog.show();}
    private void mtLoraSettings(ProtocolClient c){if(!mtLoaded(c,c.mtConfigLoaded(5),()->mtCommand(client->client.requestMtConfig(5))))return;String address=radio(RadioProfile.MESHTASTIC);if(!address.equals(mtDraftRadio)){mtLoraDraft=null;mtDraftRadio=address;}if(mtLoraSaving&&c.admin.edit==null){mtLoraSaving=false;if(c.admin.status.startsWith("Saved"))mtLoraDraft=null;}if(mtLoraDraft==null){mtLoraDraft=c.mtConfig(5).clone();mtLoraBase=mtLoraDraft.clone();}List<Wire.F> fields=Wire.fields(mtLoraDraft);int region=(int)Wire.number(fields,7,0),preset=(int)Wire.number(fields,2,0);boolean use=Wire.number(fields,1,0)!=0;section(results,"OPTIONS");LinearLayout options=group(results);
        menuRow(options,"radio","Region",MtLora.regionName(region),()->mtOption("Region",MtLora.regions(c),region,value->{try{mtLoraDraft=MtLora.regionChange(c,mtLoraDraft,value);rebuildContent();}catch(IllegalArgumentException e){Toast.makeText(this,e.getMessage(),Toast.LENGTH_LONG).show();}}),RADIO_MT);
        toggleRow(options,"Use preset","Changes are staged until Save",use,on->{mtLoraDraft=Wire.replace(mtLoraDraft,1,on?1:0);mtLoraDraft=Wire.replace(mtLoraDraft,5,on?0:5);if(!on){if(Wire.number(fields,3,0)==0)mtLoraDraft=Wire.replace(mtLoraDraft,3,250);if(Wire.number(fields,4,0)==0)mtLoraDraft=Wire.replace(mtLoraDraft,4,11);}rebuildContent();});
        if(use){menuRow(options,"radio","Preset",MtLora.presetName(preset),()->mtOption("Preset",MtLora.presets(c,region),preset,value->{mtLoraDraft=Wire.replace(Wire.replace(mtLoraDraft,2,value),5,0);rebuildContent();}),RADIO_MT);if(c.mtAtLeast(7,18)){List<MtLora.Option> rates=new ArrayList<>();rates.add(new MtLora.Option(0,"Preset default"));for(int rate=MtLora.presetCodingRate(preset)+1;rate<=8;rate++)rates.add(new MtLora.Option(rate,"4/"+rate));int cr=(int)Wire.number(fields,5,0);menuRow(options,"radio","Coding rate override",cr==0?"Preset default":"4/"+cr,()->mtOption("Coding rate override",rates,cr,v->mtDraftNumber(5,v)),RADIO_MT);}}
        else {mtDraftNumeric(options,fields,3,"Bandwidth (kHz; 2.4 GHz codes 200/400/800)",1,region==13?800:500);mtDraftNumeric(options,fields,4,"Spreading factor",5,12);mtDraftNumeric(options,fields,5,"Coding rate denominator (4/n)",5,8);}
        section(results,"ADVANCED");LinearLayout adv=group(results);mtDraftToggle(adv,fields,104,"Ignore MQTT");mtDraftToggle(adv,fields,105,"OK to MQTT");mtDraftToggle(adv,fields,9,"Transmit enabled");mtDraftToggle(adv,fields,12,"Override duty cycle");mtDraftToggle(adv,fields,13,"RX boosted gain");mtDraftNumeric(adv,fields,8,"Hop limit",0,7);mtDraftNumeric(adv,fields,11,"Frequency slot (0 = automatic)",0,65535);mtDraftNumeric(adv,fields,10,"Transmit power (0 = region default)",0,36);menuRow(adv,"radio","Frequency override (0 = automatic)",Float.toString(Float.intBitsToFloat((int)Wire.number(fields,14,0)))+" MHz",()->editMtFloat("Frequency override (MHz)",Wire.number(fields,14,0),v->{if(!Double.isFinite(v)||v<0||v>2500)throw Wire.bad("Frequency must be finite and within 0–2500 MHz");mtLoraDraft=Wire.floatField(mtLoraDraft,14,(float)v);rebuildContent();}),RADIO_MT);
        LinearLayout actions=row();actions.addView(button("Discard",()->{mtLoraDraft=null;rebuildContent();}),new LinearLayout.LayoutParams(0,dp(48),1));actions.addView(button("Save",()->{try{if(!Arrays.equals(mtLoraBase,c.mtConfig(5)))throw Wire.bad("Radio settings changed while editing. Discard this draft and review the current values.");MtLora.validate(mtLoraDraft,c);byte[] draft=mtLoraDraft.clone();mtConfirm("Save LoRa settings",false,()->{mtCommand(client->client.setMtConfig(5,draft));mtLoraSaving=c.admin.edit!=null;});}catch(IllegalArgumentException e){Toast.makeText(this,e.getMessage(),Toast.LENGTH_LONG).show();}}),new LinearLayout.LayoutParams(0,dp(48),1));results.addView(actions);}
    private void mtOption(String title,List<MtLora.Option> options,int selected,java.util.function.IntConsumer save){String[] names=new String[options.size()];int current=-1;for(int i=0;i<options.size();i++){names[i]=options.get(i).label;if(options.get(i).value==selected)current=i;}choice(title,names,current,i->save.accept(options.get(i).value));}
    private void mtDraftNumber(int field,long value){mtLoraDraft=Wire.replace(mtLoraDraft,field,value);rebuildContent();}
    private void mtDraftToggle(LinearLayout parent,List<Wire.F> fields,int field,String label){toggleRow(parent,label,"Changes are staged until Save",Wire.number(fields,field,0)!=0,on->mtDraftNumber(field,on?1:0));}
    private void mtDraftNumeric(LinearLayout parent,List<Wire.F> fields,int field,String label,int min,int max){long current=Wire.number(fields,field,0);menuRow(parent,"radio",label,Long.toString(current),()->editMtNumber(label,current,min,max,v->mtDraftNumber(field,v)),RADIO_MT);}
    private void mtChannelSettings(ProtocolClient c){section(results,"CHANNELS");for(Map.Entry<Integer,byte[]> entry:c.mtChannels.entrySet()){List<Wire.F> ch=Wire.fields(entry.getValue());int role=(int)Wire.number(ch,3,0);byte[] settings=Wire.bytes(ch,2);String name=Wire.utf8(Wire.bytes(Wire.fields(settings),3));if(name.isEmpty())name="Channel "+entry.getKey();final String channelName=name;int index=entry.getKey();menuRow(group(results),"channels",role==1?"Primary · "+channelName:channelName,Wire.number(ch,3,0)>0?"Enabled · edit channel":"Disabled",()->{if(role>0)editMtChannel(c,index,channelName);else Toast.makeText(this,"This channel slot is disabled",Toast.LENGTH_SHORT).show();},RADIO_MT);}results.addView(text("Channel reorder, encryption-key changes, and channel add/remove require the complete channel editor. Existing channel names can be edited while retaining their current key.",13,MUTED));}
    private void editMtChannel(ProtocolClient c,int slot,String current){editMtText("Channel name (automatic slot can change frequency)",current,11,true,value->mtCommand(client->client.setMtChannelName(slot,value)));}
    private void mtDeviceSettings(ProtocolClient c){section(results,"DEVICE CONFIGURATION");if(!mtLoaded(c,c.mtConfigLoaded(0)&&c.mtConfigLoaded(6),()->mtCommand(client->{client.requestMtConfig(0);client.requestMtConfig(6);})))return;if(!c.mtConfigLoaded(0))mtCommand(client->client.requestMtConfig(0));if(!c.mtConfigLoaded(6))mtCommand(client->client.requestMtConfig(6));LinearLayout rows=group(results);byte[] device=c.mtConfig(0);List<Wire.F> df=Wire.fields(device);String[] roles={"Client","Client Mute","Router","Router Client (deprecated)","Repeater (deprecated)","Tracker","Sensor","TAK","Client Hidden","Lost and Found","TAK Tracker","Router Late","Client Base"};int role=(int)Wire.number(df,1,0);menuRow(rows,"settings","Device","Role: "+roles[Math.min(roles.length-1,Math.max(0,role))],()->{List<MtLora.Option> items=new ArrayList<>();for(int i=0;i<roles.length;i++)if(i!=3&&i!=4&&(i!=12||c.mtAtLeast(7,0)))items.add(new MtLora.Option(i,roles[i]));mtOption("Device role",items,role,i->mtConfirm("Change role to "+roles[i],false,()->mtCommand(client->client.setMtConfigField(0,1,i))));},RADIO_MT);menuRow(rows,"settings","Position","Location broadcast and GPS behavior",()->Toast.makeText(this,"Position controls are read from firmware settings but not editable in this release",Toast.LENGTH_LONG).show(),RADIO_MT);menuRow(rows,"settings","Power","Power saving and battery behavior",()->Toast.makeText(this,"Power controls are read from firmware settings but not editable in this release",Toast.LENGTH_LONG).show(),RADIO_MT);menuRow(rows,"settings","Network","Wi-Fi and network behavior",()->Toast.makeText(this,"Network controls are read from firmware settings but not editable in this release",Toast.LENGTH_LONG).show(),RADIO_MT);menuRow(rows,"settings","Display","Screen behavior and display preferences",()->Toast.makeText(this,"Display controls are read from firmware settings but not editable in this release",Toast.LENGTH_LONG).show(),RADIO_MT);byte[] bt=c.mtConfig(6);menuRow(rows,"settings","Bluetooth","Enabled: "+(Wire.number(Wire.fields(bt),1,1)!=0),()->toggleMtConfig(c,6,1,"Bluetooth enabled"),RADIO_MT);}
    private void toggleMtConfig(ProtocolClient c,int type,int field,String title){boolean current=Wire.number(Wire.fields(c.mtConfig(type)),field,0)!=0;new AlertDialog.Builder(this).setTitle(title).setMessage("Set this setting to "+(current?"off":"on")+"?"+(type==6&&current?" Bluetooth will disconnect and must be re-enabled on the radio or through USB to reconnect.":" The radio may restart; settings will be verified after reconnection.")).setPositiveButton("Save",(d,w)->mtCommand(client->client.setMtConfigField(type,field,current?0:1))).setNegativeButton("Cancel",null).show();}
    private void mtSecuritySettings(ProtocolClient c){if(!mtLoaded(c,c.mtConfigLoaded(7),()->mtCommand(client->client.requestMtConfig(7))))return;if(!c.mtConfigLoaded(7))mtCommand(client->client.requestMtConfig(7));byte[] sec=c.mtConfig(7);List<Wire.F> fields=Wire.fields(sec);section(results,"PACKET AUTHENTICITY");String[] policy={"Compatible · accept unsigned","Balanced · sign, accept unsigned","Strict · require signatures"};int selected=(int)Wire.number(fields,9,0);menuRow(group(results),"settings","Protection level",(c.mtHasSignatures?policy[Math.min(2,Math.max(0,selected))]:"Unsupported by connected firmware"),c.mtHasSignatures?()->choice("Protection level",policy,selected,i->mtConfirm("Packet protection",true,()->mtCommand(client->client.setMtConfigField(7,9,i)))):null,RADIO_MT);section(results,"KEYS & LOGS");LinearLayout logs=group(results);menuRow(logs,"info","Admin keys",Long.toString(fields.stream().filter(f->f.tag==3).count())+" configured",null,RADIO_MT);toggleRow(logs,"Serial Console","Serial Console over the Stream API",Wire.number(fields,5,1)!=0,on->mtConfirm("Serial Console",true,()->mtCommand(client->client.setMtConfigField(7,5,on?1:0))));toggleRow(logs,"Debug Logs","Output live debug logging over serial and Bluetooth",Wire.number(fields,6,0)!=0,on->mtConfirm("Debug Logs",true,()->mtCommand(client->client.setMtConfigField(7,6,on?1:0))));menuRow(logs,"info","Managed Device",Wire.number(fields,4,0)!=0?"Enabled · configured remotely":"Disabled",null,RADIO_MT);results.addView(text("Private keys are not displayed here. Key generation, backup and restoration are not yet implemented.",13,MUTED));}
    private void mtModuleSettings(ProtocolClient c){section(results,"MODULE CONFIGURATION");if(!c.mtModuleSupported(0)){results.addView(text("MQTT is excluded by this radio firmware.",14,MUTED));return;}if(!mtLoaded(c,c.mtModuleLoaded(0),()->mtCommand(client->client.requestMtModule(0))))return;if(!c.mtModuleLoaded(0)&&c.mtModuleSupported(0))mtCommand(client->client.requestMtModule(0));byte[] mqtt=c.mtModule(0);LinearLayout mods=group(results);toggleRow(mods,"MQTT Enabled","Allow the radio to gateway marked channels",Wire.number(Wire.fields(mqtt),1,0)!=0,on->mtConfirm("MQTT",true,()->mtCommand(client->client.setMtModuleField(0,1,on?1:0))));menuRow(mods,"info","Status Message",c.mtStatus().isEmpty()?"Not set":c.mtStatus(),()->editMtStatus(c),RADIO_MT);results.addView(text("The remaining module pages (telemetry, canned messages, store and forward, range test and others) will be exposed as their editors are added.",13,MUTED));}
    private void choice(String title,String[] options,int selected,java.util.function.IntConsumer action){new AlertDialog.Builder(this).setTitle(title).setSingleChoiceItems(options,selected,(d,i)->{action.accept(i);d.dismiss();}).setNegativeButton("Cancel",null).show();}
    private int indexOf(int value,int[] options){for(int i=0;i<options.length;i++)if(options[i]==value)return i;return 0;}
    private void numberChoice(String title,int min,int max,int selected,java.util.function.IntConsumer action){String[] values=new String[max-min+1];for(int i=min;i<=max;i++)values[i-min]=Integer.toString(i);choice(title,values,Math.max(0,Math.min(values.length-1,selected-min)),i->action.accept(i+min));}
    private void editMtNumber(String title,long current,long min,long max,java.util.function.LongConsumer save){EditText field=new EditText(this);field.setInputType(android.text.InputType.TYPE_CLASS_NUMBER|android.text.InputType.TYPE_NUMBER_FLAG_SIGNED);field.setText(Long.toString(current));field.setTextColor(WHITE);new AlertDialog.Builder(this).setTitle(title).setView(field).setNegativeButton("Cancel",null).setPositiveButton("Save",(d,w)->{try{long value=Long.parseLong(field.getText().toString());if(value<min||value>max)throw new IllegalArgumentException();save.accept(value);}catch(Exception e){Toast.makeText(this,"Enter a value between "+min+" and "+max,Toast.LENGTH_LONG).show();}}).show();}
    private void editMtFloat(String title,long bits,java.util.function.DoubleConsumer save){EditText field=new EditText(this);field.setInputType(android.text.InputType.TYPE_CLASS_NUMBER|android.text.InputType.TYPE_NUMBER_FLAG_DECIMAL);field.setText(String.format(Locale.ROOT,"%.3f",Float.intBitsToFloat((int)bits)));field.setTextColor(WHITE);new AlertDialog.Builder(this).setTitle(title).setView(field).setNegativeButton("Cancel",null).setPositiveButton("Save",(d,w)->{try{save.accept(Double.parseDouble(field.getText().toString()));}catch(Exception e){Toast.makeText(this,"Enter a valid frequency",Toast.LENGTH_SHORT).show();}}).show();}
    private ProtocolClient mcClient(){RadioService.Session s=service==null?null:service.sessions.get(RadioProfile.MESHCORE);return s==null?null:s.client;}
    private void mcCommand(java.util.function.Consumer<ProtocolClient> action){RadioService.Session s=service==null?null:service.sessions.get(RadioProfile.MESHCORE);if(s==null||!s.verified){Toast.makeText(this,"Connect and synchronize the MeshCore radio first",Toast.LENGTH_LONG).show();return;}service.configureMeshCore(s.address,action);scheduleRefresh();}
    private void meshCoreSettingsRows(RadioService.Session session){
        ProtocolClient c=mcClient();boolean online=session!=null&&session.verified;
        section(results,"DEVICE INFO");LinearLayout device=group(results);menuRow(device,"info",c!=null&&!c.localName.isEmpty()?c.localName:"MeshCore radio",online?"Connected":"Not synchronized",()->openSettings("mc_device",RadioProfile.MESHCORE),RADIO_MC);
        section(results,"NODE SETTINGS");LinearLayout node=group(results);
        menuRow(node,"info","Node Name",c==null?"":c.localName,()->openSettings("mc_name",RadioProfile.MESHCORE),RADIO_MC);
        menuRow(node,"radio","Radio Settings",c==null||c.frequencyMhz==0?"Frequency, bandwidth, spreading factor, power":String.format(Locale.ROOT,"%.3f MHz · %.1f kHz · SF%d",c.frequencyMhz,c.bandwidthKhz,c.spreadingFactor),()->openSettings("mc_radio",RadioProfile.MESHCORE),RADIO_MC);
        menuRow(node,"map","Regions","Default flood region and scope",()->openSettings("mc_regions",RadioProfile.MESHCORE),RADIO_MC);
        menuRow(node,"radio","Radio stats","Noise floor, RSSI, SNR and airtime",()->openSettings("mc_stats",RadioProfile.MESHCORE),RADIO_MC);
        menuRow(node,"info","Telemetry","Battery, storage and device status",()->openSettings("mc_telemetry",RadioProfile.MESHCORE),RADIO_MC);
        menuRow(node,"tracks","Node ID size in paths",c==null||c.pathHashMode<0?"Firmware default":(c.pathHashMode+1)+" byte"+(c.pathHashMode==0?"":"s"),()->openSettings("mc_path",RadioProfile.MESHCORE),RADIO_MC);
        section(results,"LOCATION");LinearLayout location=group(results);
        menuRow(location,"gps","Location",c!=null&&c.hasLocation?String.format(Locale.ROOT,"%.6f, %.6f",c.latitude,c.longitude):"GPS coordinates not set",()->openSettings("mc_location",RadioProfile.MESHCORE),RADIO_MC);
        menuRow(location,"contacts","Contact Settings","How contacts are added",()->openSettings("mc_contacts",RadioProfile.MESHCORE),RADIO_MC);
        menuRow(location,"info","Privacy Settings","Control location and telemetry sharing",()->openSettings("mc_privacy",RadioProfile.MESHCORE),RADIO_MC);
        section(results,"APP SETTINGS");LinearLayout app=group(results);
        menuRow(app,"settings","App Settings","Notifications, messaging, and map preferences",()->openSettings("messages",null),ORANGE);
        menuRow(app,"filter","Message filter","Hide spam and repeated messages",()->openSettings("messages",null),ORANGE);
        section(results,"ACTIONS");LinearLayout actions=group(results);
        menuRow(actions,"sync","Sync Time","Set device clock to phone time",()->mcCommand(client->client.setDeviceTime(System.currentTimeMillis()/1000)),RADIO_MC);
        menuRow(actions,"refresh","Refresh Contacts","Reload contact list from device",()->mcCommand(ProtocolClient::refreshContacts),RADIO_MC);
        menuRow(actions,"radio","Reboot Device","Restart the MeshCore device",()->new AlertDialog.Builder(this).setTitle("Reboot MeshCore device?").setMessage("The radio will disconnect and reconnect after it restarts.").setPositiveButton("Reboot",(d,w)->mcCommand(ProtocolClient::reboot)).setNegativeButton("Cancel",null).show(),ORANGE);
    }
    private void meshCoreSettingsDetail(){
        ProtocolClient c=mcClient();
        switch(settingsSection){
            case "mc_name":section(results,"NODE NAME");menuRow(group(results),"info","Change Node Name",c==null?"":c.localName,this::editMcName,RADIO_MC);return;
            case "mc_radio":meshCoreRadioSettings(c);return;
            case "mc_path":meshCorePathSettings(c);return;
            case "mc_location":meshCoreLocationSettings(c);return;
            case "mc_contacts":meshCoreContactSettings(c);return;
            case "mc_privacy":meshCorePrivacySettings(c);return;
            case "mc_stats":meshCoreStats(c);return;
            case "mc_telemetry":meshCoreTelemetry(c);return;
            case "mc_regions":meshCoreRegions(c);return;
            case "mc_device":meshCoreDeviceInfo(c);return;
        }
    }
    private EditText mcInput(LinearLayout parent,String label,String value,int inputType){TextView title=text(label,13,MUTED);title.setPadding(0,dp(8),0,dp(3));parent.addView(title);EditText field=new EditText(this);field.setSingleLine(true);field.setText(value);field.setTextSize(16);field.setTextColor(WHITE);field.setInputType(inputType);field.setPadding(dp(12),dp(8),dp(12),dp(8));field.setBackground(surface(PANEL2));parent.addView(field,new LinearLayout.LayoutParams(-1,dp(50)));return field;}
    private void editMcName(){ProtocolClient c=mcClient();EditText field=new EditText(this);field.setSingleLine(true);field.setText(c==null?"":c.localName);field.setTextColor(WHITE);field.setFilters(new InputFilter[]{new InputFilter.LengthFilter(32)});new AlertDialog.Builder(this).setTitle("Node Name").setView(field).setNegativeButton("Cancel",null).setPositiveButton("Save",(d,w)->{String value=field.getText().toString().trim();if(value.isEmpty()){Toast.makeText(this,"Name cannot be empty",Toast.LENGTH_SHORT).show();return;}mcCommand(client->client.setName(value));}).show();}
    private void meshCoreRadioSettings(ProtocolClient c){
        section(results,"RADIO SETTINGS");
        if(c==null){results.addView(text("Connect to the MeshCore radio to read or change its radio parameters.",14,MUTED));return;}
        LinearLayout info=group(results);menuRow(info,"radio","Frequency, bandwidth, SF and coding rate",String.format(Locale.ROOT,"%.3f MHz · %.1f kHz · SF%d · CR%d",c.frequencyMhz,c.bandwidthKhz,c.spreadingFactor,c.codingRate),()->editMcRadio(c),RADIO_MC);
        menuRow(info,"radio","TX Power",c.txPower<0?"Unknown":c.txPower+" dBm"+(c.maxTxPower>0?" · max "+c.maxTxPower+" dBm":""),()->editMcPower(c),RADIO_MC);
        results.addView(text("Changing radio parameters may require a reboot on some firmware. Use the frequency and power permitted in your country.",13,MUTED));
    }
    private void editMcRadio(ProtocolClient c){LinearLayout form=column();form.setPadding(dp(20),dp(4),dp(20),dp(4));EditText freq=mcInput(form,"Frequency (MHz)",String.format(Locale.ROOT,"%.3f",c.frequencyMhz),android.text.InputType.TYPE_CLASS_NUMBER|android.text.InputType.TYPE_NUMBER_FLAG_DECIMAL);EditText bw=mcInput(form,"Bandwidth (kHz)",String.format(Locale.ROOT,"%.1f",c.bandwidthKhz),android.text.InputType.TYPE_CLASS_NUMBER|android.text.InputType.TYPE_NUMBER_FLAG_DECIMAL);EditText sf=mcInput(form,"Spreading Factor",Integer.toString(c.spreadingFactor),android.text.InputType.TYPE_CLASS_NUMBER);EditText cr=mcInput(form,"Coding Rate (5 = 4/5)",Integer.toString(c.codingRate),android.text.InputType.TYPE_CLASS_NUMBER);final int[] presetPathMode={-1};final Button[] presetsRef={null};Button presets=button("Choose preset",()->{presetsRef[0].setText("Loading presets…");MeshCorePresets.fetch("MeshCrossway/"+installedVersion()+" (Android)",(items,error)->{presetsRef[0].setText("Choose preset");if(items.isEmpty()){Toast.makeText(this,error,Toast.LENGTH_LONG).show();return;}String[] names=new String[items.size()];for(int i=0;i<items.size();i++)names[i]=items.get(i).title+" · "+items.get(i).frequency+" MHz / SF"+items.get(i).spreadingFactor+" / BW"+items.get(i).bandwidth+" / CR"+items.get(i).codingRate;new AlertDialog.Builder(this).setTitle("MeshCore radio presets").setItems(names,(dialog,index)->{MeshCorePresets.Preset p=items.get(index);freq.setText(Double.toString(p.frequency));bw.setText(Double.toString(p.bandwidth));sf.setText(Integer.toString(p.spreadingFactor));cr.setText(Integer.toString(p.codingRate));presetPathMode[0]=p.pathHashBytes>0?p.pathHashBytes-1:-1;}).setNegativeButton("Cancel",null).show();});});presetsRef[0]=presets;LinearLayout.LayoutParams bp=new LinearLayout.LayoutParams(-1,-2);bp.topMargin=dp(10);form.addView(presets,bp);TextView note=text("Presets are community suggestions. Verify the right frequency for your local mesh and location.",12,MUTED);note.setPadding(0,dp(8),0,0);form.addView(note);new AlertDialog.Builder(this).setTitle("Radio Settings").setView(form).setNegativeButton("Cancel",null).setPositiveButton("Save",(d,w)->{try{double f=Double.parseDouble(freq.getText().toString()),b=Double.parseDouble(bw.getText().toString());int s=Integer.parseInt(sf.getText().toString()),r=Integer.parseInt(cr.getText().toString());if(f<150||f>2500||b<7||b>500||s<5||s>12||r<5||r>8)throw new IllegalArgumentException();mcCommand(client->{client.setRadio(f,b,s,r);if(presetPathMode[0]>=0&&client.firmwareCode>=10)client.setPathHashMode(presetPathMode[0]);});}catch(Exception e){Toast.makeText(this,"Enter valid radio values (SF 5–12, CR 5–8)",Toast.LENGTH_LONG).show();}}).show();}
    private void editMcPower(ProtocolClient c){EditText field=new EditText(this);field.setInputType(android.text.InputType.TYPE_CLASS_NUMBER|android.text.InputType.TYPE_NUMBER_FLAG_SIGNED);field.setText(Integer.toString(Math.max(0,c.txPower)));field.setTextColor(WHITE);new AlertDialog.Builder(this).setTitle("TX Power (dBm)").setView(field).setNegativeButton("Cancel",null).setPositiveButton("Save",(d,w)->{try{int value=Integer.parseInt(field.getText().toString());if(value < -9||value>22)throw new IllegalArgumentException();mcCommand(client->client.setTxPower(value));}catch(Exception e){Toast.makeText(this,"Power must be between -9 and 22 dBm",Toast.LENGTH_LONG).show();}}).show();}
    private void meshCorePathSettings(ProtocolClient c){section(results,"NODE ID SIZE IN PATHS");int current=c==null?0:Math.max(0,c.pathHashMode);String[] sizes={"1 byte","2 bytes","3 bytes"};menuRow(group(results),"tracks","Path ID size",sizes[Math.min(2,current)],()->new AlertDialog.Builder(this).setTitle("Node ID size in paths").setSingleChoiceItems(sizes,current,(d,i)->{if(c!=null&&c.firmwareCode>=10)mcCommand(client->client.setPathHashMode(i));else Toast.makeText(this,"This setting needs MeshCore companion firmware v1.14 or newer",Toast.LENGTH_LONG).show();d.dismiss();}).setNegativeButton("Cancel",null).show(),RADIO_MC);results.addView(text("Larger IDs reduce collisions. Older repeaters can drop packets using 2- or 3-byte IDs.",13,MUTED));}
    private void meshCoreLocationSettings(ProtocolClient c){section(results,"LOCATION");menuRow(group(results),"gps","Set GPS coordinates",c!=null&&c.hasLocation?String.format(Locale.ROOT,"%.6f, %.6f",c.latitude,c.longitude):"Not set",()->editMcLocation(c),RADIO_MC);}
    private void editMcLocation(ProtocolClient c){LinearLayout form=column();form.setPadding(dp(20),dp(4),dp(20),dp(4));EditText lat=mcInput(form,"Latitude",c!=null&&c.hasLocation?Double.toString(c.latitude):"",android.text.InputType.TYPE_CLASS_NUMBER|android.text.InputType.TYPE_NUMBER_FLAG_SIGNED|android.text.InputType.TYPE_NUMBER_FLAG_DECIMAL);EditText lon=mcInput(form,"Longitude",c!=null&&c.hasLocation?Double.toString(c.longitude):"",android.text.InputType.TYPE_CLASS_NUMBER|android.text.InputType.TYPE_NUMBER_FLAG_SIGNED|android.text.InputType.TYPE_NUMBER_FLAG_DECIMAL);new AlertDialog.Builder(this).setTitle("Location").setView(form).setMessage("Leave both fields empty to remove your location.").setNegativeButton("Cancel",null).setPositiveButton("Save",(d,w)->{try{String a=lat.getText().toString().trim(),b=lon.getText().toString().trim();double x=a.isEmpty()?0:Double.parseDouble(a),y=b.isEmpty()?0:Double.parseDouble(b);if(a.isEmpty()!=b.isEmpty()||Math.abs(x)>90||Math.abs(y)>180)throw new IllegalArgumentException();mcCommand(client->client.setLocation(x,y));}catch(Exception e){Toast.makeText(this,"Enter valid latitude and longitude",Toast.LENGTH_LONG).show();}}).show();}
    private void meshCoreContactSettings(ProtocolClient c){section(results,"CONTACT SETTINGS");if(c==null)return;toggleRow(group(results),"Manual contact approval","Do not add newly heard contacts automatically",c.manualAddContacts,on->mcCommand(client->client.setOtherParams(on,client.telemetryModes,client.advertLocationPolicy,client.multiAcks)));}
    private void meshCorePrivacySettings(ProtocolClient c){section(results,"PRIVACY SETTINGS");if(c==null)return;toggleRow(group(results),"Share location in adverts","Include coordinates in your node advertisement",c.advertLocationPolicy!=0,on->mcCommand(client->client.setOtherParams(client.manualAddContacts,client.telemetryModes,on?1:0,client.multiAcks)));String[] modes={"Deny","Follow contact permissions","Allow all"};int loc=(c.telemetryModes>>2)&3,env=(c.telemetryModes>>4)&3;LinearLayout group=group(results);menuRow(group,"info","Location telemetry",modes[Math.min(2,loc)],()->singleMcChoice("Location telemetry",modes,loc,i->mcCommand(client->client.setOtherParams(client.manualAddContacts,(client.telemetryModes&0xF3)|(i<<2),client.advertLocationPolicy,client.multiAcks))),RADIO_MC);menuRow(group,"info","Sensor telemetry",modes[Math.min(2,env)],()->singleMcChoice("Sensor telemetry",modes,env,i->mcCommand(client->client.setOtherParams(client.manualAddContacts,(client.telemetryModes&0xCF)|(i<<4),client.advertLocationPolicy,client.multiAcks))),RADIO_MC);toggleRow(group,"Extra acknowledgements","Send additional ACK packets",c.multiAcks,on->mcCommand(client->client.setOtherParams(client.manualAddContacts,client.telemetryModes,client.advertLocationPolicy,on)));}
    private void singleMcChoice(String title,String[] options,int selected,java.util.function.IntConsumer save){new AlertDialog.Builder(this).setTitle(title).setSingleChoiceItems(options,selected,(d,i)->{save.accept(i);d.dismiss();}).setNegativeButton("Cancel",null).show();}
    private void meshCoreStats(ProtocolClient c){section(results,"SIGNAL");if(c==null){results.addView(text("Connect to the MeshCore radio to load live radio statistics.",14,MUTED));return;}if(c.hasRadioStats){LinearLayout group=group(results);menuRow(group,"radio","Noise floor",c.noiseFloor+" dBm",null,RADIO_MC);menuRow(group,"radio","Last RSSI",c.lastRssi+" dBm",null,RADIO_MC);menuRow(group,"radio","Last SNR",String.format(Locale.ROOT,"%.2f dB",c.lastSnrQuarter/4.0),null,RADIO_MC);section(results,"AIRTIME");LinearLayout air=group(results);menuRow(air,"radio","TX airtime (total)",c.txAirSecs+" s",null,RADIO_MC);menuRow(air,"radio","RX airtime (total)",c.rxAirSecs+" s",null,RADIO_MC);}addButton(results,"Refresh radio stats",()->mcCommand(client->client.requestStats(1)));mcCommand(client->client.requestStats(1));}
    private void meshCoreTelemetry(ProtocolClient c){section(results,"DEVICE TELEMETRY");if(c==null)return;if(c.hasBattery){LinearLayout data=group(results);menuRow(data,"info","Battery",c.batteryMv+" mV",null,RADIO_MC);if(c.usedKb>=0)menuRow(data,"info","Storage",c.usedKb+" / "+c.totalKb+" KB",null,RADIO_MC);if(c.uptimeSecs>0)menuRow(data,"info","Uptime",(c.uptimeSecs/3600)+" h "+((c.uptimeSecs/60)%60)+" min",null,RADIO_MC);}results.addView(text("Sensor values such as MCU temperature depend on the hardware and firmware telemetry channel.",13,MUTED));addButton(results,"Refresh telemetry",()->mcCommand(client->{client.requestBattery();client.requestStats(0);}));mcCommand(client->{client.requestBattery();client.requestStats(0);});}
    private void meshCoreRegions(ProtocolClient c){section(results,"REGION MANAGEMENT");if(c==null){results.addView(text("Connect to the MeshCore radio to read its default flood region.",14,MUTED));return;}mcCommand(ProtocolClient::requestDefaultFloodScope);String current=c.defaultFloodScope.isEmpty()?"None":c.defaultFloodScope;menuRow(group(results),"map","Default region",current,null,RADIO_MC);addButton(results,"Set default region",()->editMcDefaultRegion());if(!c.defaultFloodScope.isEmpty())addButton(results,"Clear default region",()->new AlertDialog.Builder(this).setTitle("Clear default region?").setPositiveButton("Clear",(d,w)->mcCommand(ProtocolClient::clearDefaultFloodScope)).setNegativeButton("Cancel",null).show());results.addView(text("MeshCore Companion firmware stores the default flood scope name and key. Fetching and managing the repeater region tree requires repeater region commands; this screen can set a default scope when its name and key are known.",13,MUTED));}
    private void editMcDefaultRegion(){LinearLayout form=column();form.setPadding(dp(20),dp(4),dp(20),dp(4));EditText name=mcInput(form,"Region name", "",android.text.InputType.TYPE_CLASS_TEXT);EditText key=mcInput(form,"Region key (32 hexadecimal characters)","",android.text.InputType.TYPE_CLASS_TEXT);new AlertDialog.Builder(this).setTitle("Default region").setView(form).setMessage("Enter the region name and key provided by your MeshCore repeater network.").setNegativeButton("Cancel",null).setPositiveButton("Save",(d,w)->{try{String n=name.getText().toString().trim(),hex=key.getText().toString().trim().replace(" ","");if(hex.length()!=32||!hex.matches("(?i)[0-9a-f]{32}"))throw new IllegalArgumentException();byte[] raw=new byte[16];for(int i=0;i<16;i++)raw[i]=(byte)Integer.parseInt(hex.substring(i*2,i*2+2),16);mcCommand(client->client.setDefaultFloodScope(n,raw));}catch(Exception e){Toast.makeText(this,"Enter a region name and 32-character hex key",Toast.LENGTH_LONG).show();}}).show();}
    private void meshCoreDeviceInfo(ProtocolClient c){section(results,"DEVICE INFO");if(c==null){results.addView(text("Connect to a MeshCore radio to view device details.",14,MUTED));return;}LinearLayout info=group(results);menuRow(info,"radio","Connection",service.sessions.get(RadioProfile.MESHCORE).status,null,RADIO_MC);menuRow(info,"info","Hardware",c.hardware.isEmpty()?"Unknown":c.hardware,null,RADIO_MC);menuRow(info,"info","Firmware",c.firmware.isEmpty()?"Unknown":c.firmware,null,RADIO_MC);menuRow(info,"info","Node Name",c.localName,null,RADIO_MC);menuRow(info,"info","Public Key",c.publicKey.isEmpty()?"Unknown":c.publicKey,null,RADIO_MC);menuRow(info,"contacts","Contacts",Integer.toString(c.nodes.size()),null,RADIO_MC);menuRow(info,"channels","Channels",Integer.toString(c.channels.size()),null,RADIO_MC);if(c.pathHashMode>=0)menuRow(info,"tracks","Path ID size",(c.pathHashMode+1)+" byte",null,RADIO_MC);}
    private void exportFullBackup(){
        if(backupBusy)return;
        if(service==null||trackService==null){
            Toast.makeText(this,"Radio and track databases are still starting",Toast.LENGTH_LONG).show();
            return;
        }
        new AlertDialog.Builder(this).setTitle("Export complete phone backup")
            .setMessage("The backup ZIP includes both mesh histories, favorites, selected radios, app settings, track folders and every recorded GPS point.\n\n"+
                "Save it in Documents or another location outside MeshCrossway before uninstalling any APK. "+
                "This ZIP contains private messages and locations and is NOT encrypted. Store it securely. "+
                "Offline map tiles and Android permission grants are not included.")
            .setPositiveButton("Choose backup file",(d,w)->
                startActivityForResult(new Intent(Intent.ACTION_CREATE_DOCUMENT)
                    .setType("application/zip").addCategory(Intent.CATEGORY_OPENABLE)
                    .putExtra(Intent.EXTRA_TITLE,"MeshCrossway-complete-backup.zip"),50))
            .setNegativeButton("Cancel",null).show();
    }
    private void importFullBackup(){
        if(backupBusy)return;
        if(service==null||trackService==null){
            Toast.makeText(this,"Radio and track databases are still starting",Toast.LENGTH_LONG).show();
            return;
        }
        new AlertDialog.Builder(this).setTitle("Restore complete phone backup")
            .setMessage("This merges saved messages, favorites, radio selections, settings, "+
                "GPS tracks and track folders. It will not delete existing records. "+
                "You may need to grant Bluetooth, location and notification permissions again. "+
                "Offline maps are not contained in the backup and must be downloaded again.")
            .setPositiveButton("Select ZIP",(d,w)->
                startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT)
                    .setType("application/zip").addCategory(Intent.CATEGORY_OPENABLE),51))
            .setNegativeButton("Cancel",null).show();
    }
    private void handleFullBackup(int request,android.net.Uri file){
        if(backupBusy)return;
        if(service==null||trackService==null){
            Toast.makeText(this,"App data services not ready",Toast.LENGTH_LONG).show();return;
        }
        backupBusy=true;
        AlertDialog progress=new AlertDialog.Builder(this)
            .setTitle(request==50?"Exporting complete backup":"Restoring complete backup")
            .setMessage("Copying saved messages, favorites, settings, and all GPS track points. Please keep MeshCrossway installed.")
            .setCancelable(false).create();
        progress.show();
        java.util.function.Consumer<String> done=error->{
            backupBusy=false;progress.dismiss();
            if(error.isEmpty()){
                if(request==51)service.reloadSavedSelection();
                new AlertDialog.Builder(this).setTitle(request==50?"Backup saved":"Backup restored")
                    .setMessage(request==50?
                        "Your complete ZIP was saved to the location you selected. "+
                        "Verify that the file is present before uninstalling any app version.":
                        "Messages, settings and all track data were merged. "+
                        "Reopen/authorize your radios as needed. Offline map tiles may require new downloads.")
                    .setPositiveButton("Done",null).show();
                rebuildContent();
            }else{
                new AlertDialog.Builder(this).setTitle("Backup needs attention")
                    .setMessage(error+"\n\nDo not uninstall the current app until a complete backup is saved and verified.")
                    .setPositiveButton("OK",null).show();
            }
        };
        if(request==50)FullBackup.export(this,service.history,trackService.store,file,done);
        else FullBackup.restore(this,service.history,trackService.store,file,done);
    }
    private void exportHistory(){new AlertDialog.Builder(this).setTitle("Export private history").setMessage("Includes messages, drafts, contacts, favorites and local POIs. Tracks are exported separately as GPX.").setPositiveButton("Choose file",(d,w)->startActivityForResult(new Intent(Intent.ACTION_CREATE_DOCUMENT).setType("application/json").addCategory(Intent.CATEGORY_OPENABLE).putExtra(Intent.EXTRA_TITLE,"MeshCrossway-backup.json"),40)).setNegativeButton("Cancel",null).show();}
    private void showMapLicenses(){try(java.io.InputStream in=getAssets().open("MAPLIBRE_LICENSE.txt")){java.io.ByteArrayOutputStream out=new java.io.ByteArrayOutputStream();byte[] buffer=new byte[4096];int n;while((n=in.read(buffer))!=-1)out.write(buffer,0,n);ScrollView scroll=new ScrollView(this);TextView content=text(out.toString("UTF-8"),12,WHITE);content.setPadding(dp(16),dp(12),dp(16),dp(12));content.setTextIsSelectable(true);scroll.addView(content);new AlertDialog.Builder(this).setTitle("Software licenses").setView(scroll).setPositiveButton("Close",null).show();}catch(java.io.IOException e){Toast.makeText(this,"Licenses could not be opened",Toast.LENGTH_LONG).show();}}
    private void map(){mapPanel=new MapPanel(this,new MapPanel.Host(){public List<MapPanel.Located> nodes(){return mapNodes();}public List<Poi> pois(){return service==null?Collections.emptyList():new ArrayList<>(service.history.pois);}public android.location.Location phone(){return position.location;}public boolean gpsEnabled(){return phoneEnabled();}public void gps(){requestPhonePosition();}public void node(MapPanel.Located node){if(radio(node.profile).equals(node.radio))nodeDetails(node.profile,node.node);}public void poi(String name,double lat,double lon){if(service!=null)service.history.poi(name,lat,lon);}public void deletePoi(String id){if(service!=null)service.history.deletePoi(id);}public List<TrackStore.Track> tracks(){return trackService==null?Collections.emptyList():new ArrayList<>(trackService.store.tracks);}public void manageTracks(){MainActivity.this.manageTracks();}public void navigate(String name,double lat,double lon){navigateTo(name,lat,lon);}public void startTrack(String name){MainActivity.this.startTrack(new Intent(MainActivity.this,TrackService.class).setAction("START").putExtra("name",name));}public void pauseTrack(boolean finish){if(trackService!=null)trackService.pause(finish);}public void resumeTrack(String id){MainActivity.this.startTrack(new Intent(MainActivity.this,TrackService.class).setAction("RESUME").putExtra("track",id));}public void updateTrack(TrackStore.Track t,String name,int color,boolean visible){if(trackService!=null)trackService.store.metadata(t.id,name,color,visible);}public void deleteTrack(TrackStore.Track t){if(trackService!=null)trackService.store.delete(t.id);}public void exportTrack(TrackStore.Track t){exportingTrack=t.id;startActivityForResult(new Intent(Intent.ACTION_CREATE_DOCUMENT).setType("application/gpx+xml").addCategory(Intent.CATEGORY_OPENABLE).putExtra(Intent.EXTRA_TITLE,"MeshCrossway-track.gpx"),42);}},mapStyleOverride,mapFocus);mapFocus=null;body.addView(mapPanel.root,new LinearLayout.LayoutParams(-1,-1));if(visible)mapPanel.onStart();if(resumed)mapPanel.onResume();}
    private boolean phoneEnabled(){return getSharedPreferences("ui",MODE_PRIVATE).getBoolean("gpsEnabled",false);}
    private void requestPhonePosition(){if(!position.permitted()){requestPermissions(new String[]{Manifest.permission.ACCESS_COARSE_LOCATION,Manifest.permission.ACCESS_FINE_LOCATION},8);return;}getSharedPreferences("ui",MODE_PRIVATE).edit().putBoolean("gpsEnabled",true).apply();if(resumed)position.start();if(position.location==null)Toast.makeText(this,"Waiting for phone location • check Android Location settings",Toast.LENGTH_LONG).show();if(mapPanel!=null)mapPanel.update();if(selected==5)rebuildContent();}
    private void quickReply(){ArrayList<String> templates=new ArrayList<>();for(String line:getSharedPreferences("ui",MODE_PRIVATE).getString("quickReplies","OK\nOn my way\nNeed assistance").split("\n")){String trimmed=line.trim();if(!trimmed.isEmpty()&&templates.size()<8)templates.add(trimmed.substring(0,Math.min(120,trimmed.length())));}ArrayList<String> choices=new ArrayList<>(templates);int locationIndex=-1;if(position.location!=null&&phoneEnabled()&&System.currentTimeMillis()-position.location.getTime()<120000){locationIndex=choices.size();choices.add("Insert phone coordinates");}int editIndex=choices.size();choices.add("Edit saved replies…");final int coordinateChoice=locationIndex;final RadioProfile network=chatProfile;final String key=draftKey();new AlertDialog.Builder(this).setTitle("Insert into "+network.title+" draft").setItems(choices.toArray(new String[0]),(d,index)->{if(index==editIndex){editQuickReplies();return;}if(chat==null||composer==null||!key.equals(draftKey()))return;String value=index==coordinateChoice?String.format(Locale.ROOT,"%.5f,%.5f",position.location.getLatitude(),position.location.getLongitude()):templates.get(index);if(composer.length()>0)value=composer.getText()+" "+value;composer.setText(value);composer.setSelection(composer.length());}).setNegativeButton("Cancel",null).show();}
    private void editQuickReplies(){EditText editor=new EditText(this);editor.setText(getSharedPreferences("ui",MODE_PRIVATE).getString("quickReplies","OK\nOn my way\nNeed assistance"));editor.setMinLines(4);editor.setTextColor(WHITE);new AlertDialog.Builder(this).setTitle("Quick replies").setMessage("One reply per line. Up to 8 replies, 120 characters each.").setView(editor).setPositiveButton("Save",(a,b)->getSharedPreferences("ui",MODE_PRIVATE).edit().putString("quickReplies",editor.getText().toString().substring(0,Math.min(2000,editor.length()))).apply()).setNegativeButton("Cancel",null).show();}
    @Override protected void onActivityResult(int request,int result,Intent data){super.onActivityResult(request,result,data);if(result!=RESULT_OK||data==null||data.getData()==null)return;if(request==50||request==51){handleFullBackup(request,data.getData());return;}
        if(request==53){
            android.net.Uri photo=data.getData();
            Toast.makeText(this,"Applying photo background…",Toast.LENGTH_SHORT).show();
            new Thread(()->{
                String failure="";
                try{Appearance.storePhoto(getApplicationContext(),photo);}
                catch(java.io.IOException e){failure="Could not load this picture. Choose a different photo.";}
                final String error=failure;
                runOnUiThread(()->{
                    if(error.isEmpty()){
                        Appearance.setMode(this,Appearance.PHOTO);
                        recreate();
                    }else Toast.makeText(this,error,Toast.LENGTH_LONG).show();
                });
            },"meshcrossway-appearance-photo").start();
            return;
        }
        if(request==52){
            if(trackService==null){Toast.makeText(this,"Tracks are not ready",Toast.LENGTH_LONG).show();return;}
            try{
                trackService.store.importGpx(getContentResolver().openInputStream(data.getData()),error->{
                    Toast.makeText(this,error.isEmpty()?"GPX track imported into phone":error,Toast.LENGTH_LONG).show();
                    rebuildContent();
                });
            }catch(java.io.IOException e){
                Toast.makeText(this,"Could not read GPX file",Toast.LENGTH_LONG).show();
            }
            return;
        }
        if(request==42){if(trackService!=null)try{trackService.store.export(exportingTrack,getContentResolver().openOutputStream(data.getData()),error->Toast.makeText(this,error.isEmpty()?"GPX exported":error,Toast.LENGTH_LONG).show());}catch(java.io.IOException e){Toast.makeText(this,"Could not open GPX file",Toast.LENGTH_LONG).show();}return;}if(service==null)return;try{java.util.function.Consumer<String> done=error->{if(error.isEmpty()){service.reloadSavedSelection();Toast.makeText(this,request==40?"History exported":"History imported • existing messages retained",Toast.LENGTH_LONG).show();}else Toast.makeText(this,error,Toast.LENGTH_LONG).show();};if(request==40)service.history.exportBackup(getContentResolver().openOutputStream(data.getData()),done);else if(request==41)service.history.importBackup(getContentResolver().openInputStream(data.getData()),done);}catch(java.io.IOException e){Toast.makeText(this,"Could not open the selected file",Toast.LENGTH_LONG).show();}}
    private void fieldHome(){
        LinearLayout c=card(results);c.addView(text("Phone GPS & tracks",18,WHITE));android.location.Location fix=position.location;
        if(fix==null||!phoneEnabled())c.addView(text("Phone location is off or waiting for a fix",13,MUTED));else{long age=Math.max(0,(System.currentTimeMillis()-fix.getTime())/1000);c.addView(text(String.format(Locale.ROOT,"%.5f, %.5f · ±%.0f m · %d s old",fix.getLatitude(),fix.getLongitude(),fix.getAccuracy(),age),13,MUTED));if(fix.hasAltitude())c.addView(text(String.format(Locale.ROOT,"Altitude %.0f m%s",fix.getAltitude(),fix.hasSpeed()?String.format(Locale.ROOT," · %.1f km/h",fix.getSpeed()*3.6):""),13,MUTED));}
        if(trackService!=null){TrackStore.Track active=trackService.store.current();if(trackService.recording)c.addView(text(trackService.status,12,MUTED));c.addView(text(active==null?"No active track":active.name+" · "+active.summary(),13,GREEN));if(!trackService.store.error.isEmpty())c.addView(text(trackService.store.error,13,RED));}
        LinearLayout actions=row();actions.addView(button("Phone GPS",this::requestPhonePosition),new LinearLayout.LayoutParams(0,dp(48),1));actions.addView(button("Tracks",this::manageTracks),new LinearLayout.LayoutParams(0,dp(48),1));c.addView(actions);
    }
    private void navigateTo(String name,double lat,double lon){if(position.location==null||!phoneEnabled()||System.currentTimeMillis()-position.location.getTime()>120000){requestPhonePosition();Toast.makeText(this,"Wait for a fresh phone position, then open distance and bearing again",Toast.LENGTH_LONG).show();return;}double distance=GeoMath.distance(position.location.getLatitude(),position.location.getLongitude(),lat,lon),bearing=FieldTools.bearing(position.location.getLatitude(),position.location.getLongitude(),lat,lon);new AlertDialog.Builder(this).setTitle(name).setMessage(String.format(Locale.ROOT,"Straight-line distance: %.2f km\nTrue-north bearing: %03.0f° %s\nPhone accuracy: ±%.0f m\nTarget: %.6f, %.6f\nBased on the saved target position; this is not a road route.",distance/1000,bearing,FieldTools.direction(bearing),position.location.getAccuracy(),lat,lon)).setPositiveButton("Show on map",(d,w)->showMap(lat,lon)).setNegativeButton("Back",null).show();}
    private void startTrack(Intent action){
        if(trackService==null){Toast.makeText(this,"Track manager is starting • try again",Toast.LENGTH_SHORT).show();return;}
        if(checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)!=PackageManager.PERMISSION_GRANTED){pendingTrack=action;ArrayList<String> permissions=new ArrayList<>(Arrays.asList(Manifest.permission.ACCESS_COARSE_LOCATION,Manifest.permission.ACCESS_FINE_LOCATION));if(Build.VERSION.SDK_INT>=33&&checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)!=PackageManager.PERMISSION_GRANTED)permissions.add(Manifest.permission.POST_NOTIFICATIONS);requestPermissions(permissions.toArray(new String[0]),9);return;}
        android.location.LocationManager manager=getSystemService(android.location.LocationManager.class);if(manager==null||!manager.isProviderEnabled(android.location.LocationManager.GPS_PROVIDER)){Toast.makeText(this,"Enable phone GPS in Android settings before recording",Toast.LENGTH_LONG).show();startActivity(new Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS));return;}
        startForegroundService(action);Toast.makeText(this,"Track recording started • use Tracks or the notification to pause",Toast.LENGTH_LONG).show();
    }
    void manageTracks(){if(trackService==null)return;ArrayList<TrackStore.Track> entries=new ArrayList<>(trackService.store.tracks);String[] choices=new String[entries.size()+1];choices[0]="Start a new track…";for(int i=0;i<entries.size();i++)choices[i+1]=entries.get(i).name+" · "+entries.get(i).summary();new AlertDialog.Builder(this).setTitle("Local tracks").setItems(choices,(d,index)->{if(index==0){TrackStore.Track active=trackService.store.current();if(active!=null){trackDetails(active);return;}EditText name=new EditText(this);name.setHint("Track name");name.setText("Track "+new java.text.SimpleDateFormat("yyyy-MM-dd HH:mm",Locale.ROOT).format(new Date()));new AlertDialog.Builder(this).setTitle("Record phone GPS track").setMessage("Recording continues with the screen off while its notification is active. Saved locally; no coordinates are sent to either mesh. Export GPX before uninstalling.").setView(name).setPositiveButton("Start",(a,b)->startTrack(new Intent(this,TrackService.class).setAction("START").putExtra("name",name.getText().toString()))).setNegativeButton("Cancel",null).show();}else trackDetails(entries.get(index-1));}).setNegativeButton("Back",null).show();}
    private void trackDetails(TrackStore.Track t){ArrayList<String> choices=new ArrayList<>();if(t.state.equals("recording"))choices.add("Pause recording");if(t.state.equals("paused"))choices.add("Resume recording");if(!t.state.equals("saved"))choices.add("Finish and save");choices.add("Show on map");choices.add(t.visible?"Hide on map":"Show track layer");choices.add("Rename");choices.add("Color");choices.add("Export GPX");if(t.state.equals("saved"))choices.add("Delete…");new AlertDialog.Builder(this).setTitle(t.name+" · "+t.summary()).setItems(choices.toArray(new String[0]),(d,index)->{String choice=choices.get(index);if(choice.equals("Pause recording"))trackService.pause(false);else if(choice.equals("Resume recording"))startTrack(new Intent(this,TrackService.class).setAction("RESUME").putExtra("track",t.id));else if(choice.equals("Finish and save"))trackService.pause(true);else if(choice.equals("Show on map")){trackService.store.locate(t.id,fix->{if(fix!=null)showMap(fix.lat,fix.lon);else{page(4);Toast.makeText(this,"Track is empty • no fresh GPS points yet",Toast.LENGTH_LONG).show();}});}else if(choice.equals("Hide on map")||choice.equals("Show track layer"))trackService.store.metadata(t.id,t.name,t.color,!t.visible);else if(choice.equals("Rename")){EditText name=new EditText(this);name.setText(t.name);new AlertDialog.Builder(this).setTitle("Track name").setView(name).setPositiveButton("Save",(a,b)->trackService.store.metadata(t.id,name.getText().toString(),t.color,t.visible)).setNegativeButton("Cancel",null).show();}else if(choice.equals("Color"))new AlertDialog.Builder(this).setTitle("Track color").setItems(new String[]{"Green","Orange","Blue","Pink"},(a,b)->trackService.store.metadata(t.id,t.name,TrackStore.COLORS[b],t.visible)).show();else if(choice.equals("Export GPX")){exportingTrack=t.id;startActivityForResult(new Intent(Intent.ACTION_CREATE_DOCUMENT).setType("application/gpx+xml").addCategory(Intent.CATEGORY_OPENABLE).putExtra(Intent.EXTRA_TITLE,"MeshCrossway-track.gpx"),42);}else if(choice.equals("Delete…"))new AlertDialog.Builder(this).setTitle("Delete saved track?").setMessage(t.name).setPositiveButton("Delete",(a,b)->trackService.store.delete(t.id)).setNegativeButton("Cancel",null).show();}).setNegativeButton("Back",null).show();}
    private String when(long seconds){return seconds<=0?"Unknown":timestampFormat.format(new Date(seconds*1000));}
    private static String ago(long seconds){if(seconds<=0)return "last heard unknown";long delta=Math.max(0,System.currentTimeMillis()/1000-seconds);return delta<60?"heard just now":delta<3600?"heard "+delta/60+" min ago":delta<86400?"heard "+delta/3600+" h ago":"heard "+delta/86400+" days ago";}
    private TextWatcher watcher(Runnable changed){return new TextWatcher(){public void beforeTextChanged(CharSequence s,int start,int count,int after){}public void onTextChanged(CharSequence s,int start,int before,int count){changed.run();}public void afterTextChanged(Editable e){}};}
    private void withPermission(Runnable action){if(service==null){sending=false;Toast.makeText(this,"Controller is starting; try again",Toast.LENGTH_SHORT).show();return;}ArrayList<String> missing=new ArrayList<>();if(Build.VERSION.SDK_INT>=31){for(String p:new String[]{Manifest.permission.BLUETOOTH_SCAN,Manifest.permission.BLUETOOTH_CONNECT})if(checkSelfPermission(p)!=PackageManager.PERMISSION_GRANTED)missing.add(p);}else if(checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)!=PackageManager.PERMISSION_GRANTED)missing.add(Manifest.permission.ACCESS_FINE_LOCATION);if(Build.VERSION.SDK_INT>=33&&checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)!=PackageManager.PERMISSION_GRANTED)missing.add(Manifest.permission.POST_NOTIFICATIONS);if(!missing.isEmpty()){pending=action;requestPermissions(missing.toArray(new String[0]),7);return;}startForegroundService(new Intent(this,RadioService.class));action.run();}
    @Override public void onRequestPermissionsResult(int request,String[] permissions,int[] grants){super.onRequestPermissionsResult(request,permissions,grants);if(request==9){Intent action=pendingTrack;pendingTrack=null;if(checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)==PackageManager.PERMISSION_GRANTED&&action!=null)startTrack(action);else Toast.makeText(this,"Precise location is required to record tracks",Toast.LENGTH_LONG).show();return;}if(request==8){if(position.permitted())requestPhonePosition();else Toast.makeText(this,"Phone location is optional; radio messaging remains available",Toast.LENGTH_LONG).show();if(selected==5)rebuildContent();return;}if(request==7){Runnable action=pending;pending=null;if(service!=null&&service.allowed()&&action!=null){startForegroundService(new Intent(this,RadioService.class));action.run();}else{sending=false;updateComposer();Toast.makeText(this,"Nearby devices permission is required. Draft kept.",Toast.LENGTH_LONG).show();}}}
}
