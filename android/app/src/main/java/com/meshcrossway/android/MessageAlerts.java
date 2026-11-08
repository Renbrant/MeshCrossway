package com.meshcrossway.android;

import android.Manifest;
import android.app.*;
import android.content.*;
import android.content.pm.PackageManager;
import android.os.Build;
import static com.meshcrossway.android.MeshData.*;

final class MessageAlerts {
    private final Context context;private final NotificationManager manager;
    MessageAlerts(Context context){this.context=context;manager=context.getSystemService(NotificationManager.class);for(RadioProfile p:RadioProfile.values())manager.createNotificationChannel(new NotificationChannel("messages-"+p.name(),p.title+" messages",NotificationManager.IMPORTANCE_DEFAULT));}
    void show(RadioProfile profile,String radio,Message message){
        if(Build.VERSION.SDK_INT>=33&&context.checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)!=PackageManager.PERMISSION_GRANTED)return;
        Intent open=new Intent(context,MainActivity.class).setFlags(Intent.FLAG_ACTIVITY_NEW_TASK|Intent.FLAG_ACTIVITY_CLEAR_TOP|Intent.FLAG_ACTIVITY_SINGLE_TOP);
        open.setData(android.net.Uri.parse("meshcrossway://conversation/"+profile.name()+"/"+android.net.Uri.encode(radio)+"/"+android.net.Uri.encode(message.topic)));
        open.putExtra("network",profile.name()).putExtra("radio",radio).putExtra("topic",message.topic).putExtra("title",message.title).putExtra("peer",message.peer).putExtra("identity",message.identity).putExtra("slot",message.slot).putExtra("group",message.group);
        int id=(profile.name()+"|"+radio+"|"+message.topic).hashCode()&0x7fffffff;PendingIntent content=PendingIntent.getActivity(context,id,open,PendingIntent.FLAG_IMMUTABLE|PendingIntent.FLAG_UPDATE_CURRENT);
        boolean preview=context.getSharedPreferences("ui",Context.MODE_PRIVATE).getBoolean("messagePreviews",false);
        Notification publicVersion=new Notification.Builder(context,"messages-"+profile.name()).setSmallIcon(R.drawable.ic_launcher).setContentTitle("MeshCrossway").setContentText("New mesh message").build();
        Notification notification=new Notification.Builder(context,"messages-"+profile.name()).setSmallIcon(R.drawable.ic_launcher).setContentTitle(profile.title+" · "+message.title).setContentText(preview?message.sender+": "+message.body:"Open conversation").setContentIntent(content).setAutoCancel(true).setVisibility(Notification.VISIBILITY_PRIVATE).setPublicVersion(publicVersion).build();manager.notify("mesh-chat",id,notification);
    }
}
