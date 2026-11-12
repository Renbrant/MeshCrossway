package com.meshcrossway.android;

import java.util.*;

/** One bounded, view-scoped snapshot of public configuration. Never schedules a write. */
final class McRepeaterSettings {
    static final long GAP_MS=1000;
    enum Field {
        NAME("name","Name",""), LAT("lat","Latitude","°"), LON("lon","Longitude","°"),
        RADIO("radio","LoRa parameters",""), TX("tx","TX power","dBm"), REPEAT("repeat","Repeating",""),
        LOCAL_ADVERT("advert.interval","Local announcements","min"), FLOOD_ADVERT("flood.advert.interval","Flood announcements","h"),
        FLOOD_MAX("flood.max","Maximum flood hops","hops"), READ_ONLY("allow.read.only","Guest read-only access","");
        final String key,title,unit;
        Field(String key,String title,String unit){this.key=key;this.title=title;this.unit=unit;}
        boolean toggle(){return this==REPEAT||this==READ_ONLY;}
        boolean decimal(){return this==LAT||this==LON;}
        boolean integer(){return this==TX||this==LOCAL_ADVERT||this==FLOOD_ADVERT||this==FLOOD_MAX;}
        boolean disruptive(){return this==RADIO||this==REPEAT||this==FLOOD_MAX||this==READ_ONLY;}
    }
    static final class Value {
        String raw="",state="Waiting to read",expected="";
        long readAt;
        boolean valid;
    }
    static final class Operation {
        final Field field;final String command,key;final boolean write;
        Operation(Field field,String key,String command,boolean write){this.field=field;this.key=key;this.command=command;this.write=write;}
    }
    final EnumMap<Field,Value> values=new EnumMap<>(Field.class);
    private final ArrayDeque<Field> reads=new ArrayDeque<>();
    String key="",status="Sign in to load settings";
    boolean visible,editing,started;
    long nextAt,revision;
    Operation current;
    McRepeaterSettings(){for(Field f:Field.values())values.put(f,new Value());}
    void open(String id){
        if(visible&&key.equals(id))return;
        if(!key.equals(id)){clear();key=id;}
        visible=true;started=false;revision++;
    }
    void close(){if(!visible)return;visible=false;editing=false;reads.clear();started=false;revision++;}
    void clear(){reads.clear();current=null;key="";visible=false;editing=false;started=false;nextAt=0;
        for(Field f:Field.values())values.put(f,new Value());status="Sign in to load settings";revision++;}
    void resetLogin(String id){boolean shown=visible&&key.equals(id);clear();key=id;visible=shown;}
    void refresh(){
        if(current!=null)throw Wire.bad("Wait for the current repeater response");
        reads.clear();started=false;revision++;
    }
    boolean loading(){return current!=null||!reads.isEmpty()||visible&&!started;}
    boolean hasReads(){return !reads.isEmpty();}
    int loaded(){int n=0;for(Value v:values.values())if(v.valid)n++;return n;}
    Operation next(long now){
        if(!visible||editing||current!=null||now<nextAt)return null;
        if(!started){
            reads.clear();reads.addAll(Arrays.asList(Field.values()));started=true;
            for(Value value:values.values())value.state=value.valid?"Refreshing…":"Waiting to read";
            status="Reading repeater settings…";revision++;
        }
        Field field=reads.poll();if(field==null)return null;
        return new Operation(field,key,"get "+field.key,false);
    }
    void dispatched(Operation operation){
        current=operation;Value value=values.get(operation.field);
        if(operation.write)value.expected=operation.command.substring(("set "+operation.field.key+" ").length());
        value.state=operation.write?"Saving…":"Reading…";
        status=operation.write?"Saving "+operation.field.title.toLowerCase(Locale.ROOT)+"…":"Reading "+operation.field.title.toLowerCase(Locale.ROOT)+"…";
        revision++;
    }
    Operation edit(Field field,String input,long now){
        if(current!=null)throw Wire.bad("Waiting for the current read; tap Save again shortly");
        Value value=values.get(field);
        if(!value.valid)throw Wire.bad("Read this setting before editing");
        String command=McRepeaterAdmin.setting(field.key,input);
        return new Operation(field,key,command,true); // Returned only to an explicit Save handler.
    }
    void received(String response,long now){
        Operation op=current;if(op==null)return;
        current=null;nextAt=now+GAP_MS;
        if(!op.key.equals(key))return;
        Value value=values.get(op.field);
        if(op.write){
            String r=response.trim().toUpperCase(Locale.ROOT);
            if(r.startsWith("OK")||r.startsWith("(OK")){
                value.state="Saved; checking repeater value…";
                // Only a read may be scheduled after a user-triggered write.
                if(visible){reads.remove(op.field);reads.addFirst(op.field);}
                status="Checking the saved value…";
            }else{value.state="Change was not accepted";value.expected="";status=response;}
        }else{
            try{
                String raw=parse(op.field,response);
                value.raw=raw;value.valid=true;value.readAt=now;
                value.state=value.expected.isEmpty()?"":equivalent(op.field,value.expected,raw)?"Confirmed by readback":"Repeater reports a different value";
                value.expected="";
            }catch(IllegalArgumentException invalid){value.valid=false;value.state="Unavailable on this firmware";value.expected="";}
            status=reads.isEmpty()?"Settings loaded · tap a card to edit":"Reading repeater settings…";
        }
        revision++;
    }
    void interrupted(){
        if(current!=null){Value value=values.get(current.field);value.state=current.write?"Change not confirmed":"No reply";value.valid=false;}
        current=null;reads.clear();started=true;editing=false;
        status="Reading stopped. Sign in again to refresh; no command was retried.";revision++;
    }
    static String parse(Field field,String response){
        String r=response.trim();
        if(!r.startsWith(">"))throw Wire.bad("Not a configuration value");
        String value=r.substring(1).trim();
        if(value.contains("\n")||value.contains("\r")||value.indexOf('\0')>=0)throw Wire.bad("Invalid value");
        if(field==Field.NAME){if(value.isEmpty()||value.length()>128)throw Wire.bad("Invalid name");return value;}
        if(field.toggle()){value=value.toLowerCase(Locale.ROOT);if(!value.equals("on")&&!value.equals("off"))throw Wire.bad("Invalid toggle");return value;}
        if(field==Field.RADIO){String[] p=value.split(",",-1);if(p.length!=4)throw Wire.bad("Invalid radio parameters");for(String part:p)finite(part);return value;}
        finite(value);
        if(field.integer())try{Integer.parseInt(value);}catch(NumberFormatException e){throw Wire.bad("Invalid integer");}
        return value;
    }
    private static double finite(String value){try{double n=Double.parseDouble(value);if(!Double.isFinite(n))throw new NumberFormatException();return n;}catch(NumberFormatException e){throw Wire.bad("Invalid number");}}
    static boolean equivalent(Field field,String a,String b){
        try{
            if(field==Field.NAME)return a.equals(b);
            if(field.toggle())return a.equalsIgnoreCase(b);
            if(field==Field.RADIO){String[] x=a.split(","),y=b.split(",");if(x.length!=4||y.length!=4)return false;for(int i=0;i<4;i++)if(Math.abs(finite(x[i])-finite(y[i]))>0.0001)return false;return true;}
            return Math.abs(finite(a)-finite(b))<0.00001;
        }catch(IllegalArgumentException invalid){return false;}
    }
    String display(Field field){
        Value value=values.get(field);
        if(!value.valid)return value.state;
        if(field.toggle())return value.raw.equals("on")?"On":"Off";
        if(field==Field.RADIO){String[] p=value.raw.split(",");return p[0].trim()+" MHz · "+p[1].trim()+" kHz\nSF "+p[2].trim()+" · CR 4/"+p[3].trim();}
        if((field==Field.LOCAL_ADVERT||field==Field.FLOOD_ADVERT)&&value.raw.equals("0"))return "Off";
        return value.raw+(field.unit.isEmpty()?"":" "+field.unit);
    }
}
