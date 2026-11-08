package com.meshcrossway.android;

import android.app.*;
import android.content.*;
import android.database.sqlite.SQLiteDatabase;
import android.os.Bundle;
import android.view.*;
import android.widget.*;
import java.lang.reflect.Method;
import java.util.concurrent.*;
import static com.meshcrossway.android.MeshData.*;

/** Device/emulator tests with no radio and no third-party runner dependency. */
public final class SmokeInstrumentation extends Instrumentation {
    @Override public void onCreate(Bundle args){super.onCreate(args);start();}
    private static void check(boolean condition,String message){if(!condition)throw new AssertionError(message);}
    private static void await(CountDownLatch latch)throws Exception{check(latch.await(10,TimeUnit.SECONDS),"Async storage timed out");}
    private static Message message(String id,String body){Message m=new Message();m.uid=id;m.body=body;m.topic="c:0:hash";m.title="Test";m.timestamp=1700000000;m.group=true;return m;}
    private void barrier(HistoryStore store,int id)throws Exception{CountDownLatch done=new CountDownLatch(1);store.outgoing(RadioProfile.MESHCORE,"barrier",message("barrier"+id,"test"),done::countDown,()->{throw new AssertionError("Barrier failed");});await(done);}
    private void database()throws Exception{
        Context context=getTargetContext();String name="instrumentation-only.db";context.deleteDatabase(name);HistoryStore store=new HistoryStore(context,name);
        Message inbound=message("same-id","hello");store.message(RadioProfile.MESHTASTIC,"radio-a",inbound);store.message(RadioProfile.MESHTASTIC,"radio-a",inbound);store.message(RadioProfile.MESHCORE,"radio-a",message("same-id","other network"));store.message(RadioProfile.MESHTASTIC,"radio-b",message("same-id","other radio"));store.draft(RadioProfile.MESHTASTIC,"radio-a",inbound.topic,"draft 👋");barrier(store,1);
        SQLiteDatabase db=store.getReadableDatabase();check(HistoryStore.load(db,RadioProfile.MESHTASTIC,"radio-a").messages.size()==1,"Duplicate message inserted");check(HistoryStore.load(db,RadioProfile.MESHCORE,"radio-a").messages.get(0).body.equals("other network"),"Network isolation failed");check(HistoryStore.load(db,RadioProfile.MESHTASTIC,"radio-b").messages.get(0).body.equals("other radio"),"Radio isolation failed");
        store.read(RadioProfile.MESHTASTIC,"radio-a",inbound.topic);store.message(RadioProfile.MESHTASTIC,"radio-a",inbound);Message out=message("out","outgoing");out.outgoing=true;out.state="queued";store.message(RadioProfile.MESHTASTIC,"radio-a",out);store.delivery(RadioProfile.MESHTASTIC,"radio-a",out.uid,"delivered");store.delivery(RadioProfile.MESHTASTIC,"radio-a",out.uid,"submitted");store.uncertain(RadioProfile.MESHTASTIC,"radio-a");barrier(store,2);
        Snapshot snapshot=HistoryStore.load(db,RadioProfile.MESHTASTIC,"radio-a");check(snapshot.unread()==0,"Replay reset read state");check(snapshot.messages.stream().anyMatch(m->m.uid.equals("out")&&m.state.equals("delivered")),"Late write downgraded delivery");store.close();
        HistoryStore reopened=new HistoryStore(context,name);check(HistoryStore.load(reopened.getReadableDatabase(),RadioProfile.MESHTASTIC,"radio-a").messages.size()==2,"History lost after reopen");CountDownLatch draft=new CountDownLatch(1);reopened.loadDraft(RadioProfile.MESHTASTIC,"radio-a",inbound.topic,value->{check(value.equals("draft 👋"),"Draft lost");draft.countDown();});await(draft);reopened.close();
    }
    private View find(View root,String label){if(root instanceof TextView&&((TextView)root).getText().toString().equals(label))return root;if(root instanceof ViewGroup){ViewGroup group=(ViewGroup)root;for(int i=0;i<group.getChildCount();i++){View found=find(group.getChildAt(i),label);if(found!=null)return found;}}return null;}
    private void click(MainActivity activity,String label){runOnMainSync(()->{View view=find(activity.getWindow().getDecorView(),label);check(view!=null,"Missing navigation: "+label);while(!view.isClickable()&&view.getParent() instanceof View)view=(View)view.getParent();check(view.performClick(),"Cannot click "+label);});waitForIdleSync();}
    private EditText composer(View root){if(root instanceof EditText&&"Write a message…".contentEquals(((EditText)root).getHint()))return (EditText)root;if(root instanceof ViewGroup){ViewGroup g=(ViewGroup)root;for(int i=0;i<g.getChildCount();i++){EditText e=composer(g.getChildAt(i));if(e!=null)return e;}}return null;}
    private void activity()throws Exception{
        Intent intent=new Intent(getTargetContext(),MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);MainActivity activity=(MainActivity)startActivitySync(intent);waitForIdleSync();
        for(String page:new String[]{"Nodes","Groups","Chats","Map","Settings","Home"})click(activity,page);
        Method open=MainActivity.class.getDeclaredMethod("openChat",RadioProfile.class,String.class,Target.class);open.setAccessible(true);runOnMainSync(()->{try{open.invoke(activity,RadioProfile.MESHTASTIC,"offline-test",new Target("c:0:test","Test channel","","test",0,true));EditText entry=composer(activity.getWindow().getDecorView());check(entry!=null,"Composer missing");entry.setText("Draft survives rotation 👋");}catch(Exception e){throw new RuntimeException(e);}});
        ActivityMonitor monitor=addMonitor(MainActivity.class.getName(),null,false);runOnMainSync(activity::recreate);MainActivity recreated=(MainActivity)waitForMonitorWithTimeout(monitor,10000);check(recreated!=null,"Activity recreation timed out");waitForIdleSync();runOnMainSync(()->{EditText entry=composer(recreated.getWindow().getDecorView());check(entry!=null&&entry.getText().toString().equals("Draft survives rotation 👋"),"Rotation lost composer draft");recreated.finish();});removeMonitor(monitor);
    }
    @Override public void onStart(){Bundle status=new Bundle();status.putString("id","MeshCrossway");status.putString("class",getClass().getName());status.putString("test","databaseAndNavigation");status.putInt("numtests",1);status.putInt("current",1);sendStatus(1,status);Bundle result=new Bundle();try{database();activity();sendStatus(0,status);result.putString("stream","\nOK (1 test)\n");finish(Activity.RESULT_OK,result);}catch(Throwable error){status.putString("stack",android.util.Log.getStackTraceString(error));sendStatus(-2,status);result.putString("stream",status.getString("stack"));finish(Activity.RESULT_CANCELED,result);}}
}
