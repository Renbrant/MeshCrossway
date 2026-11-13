package com.meshcrossway.android;

import java.nio.charset.StandardCharsets;
import java.util.*;
import static com.meshcrossway.android.MeshData.*;

/** Session-only remote CLI. Wire format follows meshcore_py messaging and Companion firmware.
 * Remote CLI has no request tag: only one operation, unique contact prefix and a drained
 * backlog are allowed. A reply is displayed as a remote response, never a verified readback.
 */
final class McRepeaterAdmin {
    static final long SESSION_MS=600_000, WAIT_MS=120_000;
    String key="",state="Sign in to administer this repeater",reply="",label="";
    boolean pending,login,admin,sensitive,submitted;
    long deadline,expires,lastRemoteStamp,revision;
    private final Set<Long> seenReplies=new HashSet<>();

    boolean authorized(String id,long now){return admin&&key.equalsIgnoreCase(id)&&now<expires;}
    void tick(long now){
        if(pending&&now>=deadline){pending=false;admin=false;reply="";
            state="No remote reply. Delivery or change is unknown; nothing was retried.";revision++;}
        if(admin&&now>=expires){admin=false;state="Administration session expired; sign in again";revision++;}
    }
    private void select(Node n,Collection<Node> contacts,long now){
        tick(now);
        if(pending)throw Wire.bad("Wait for the pending repeater operation");
        if(!RepeaterInventory.isRepeater(n)||n.mcState!=ProtocolClient.MC_APPROVED)
            throw Wire.bad("Select an approved repeater");
        int matches=0;
        for(Node c:contacts)if(c.id.startsWith(n.id.substring(0,12)))matches++;
        if(matches!=1)throw Wire.bad("Ambiguous repeater prefix; administration unavailable");
    }
    byte[] login(Node n,Collection<Node> contacts,String password,long now,int limit){
        select(n,contacts,now);
        byte[] secret=password.getBytes(StandardCharsets.UTF_8);
        if(secret.length>15||password.indexOf('\0')>=0||password.contains("\n")||password.contains("\r"))
            throw Wire.bad("Password must be at most 15 UTF-8 bytes, on one line");
        if(33+secret.length>limit)throw Wire.bad("Login exceeds radio payload limit");
        byte[] frame=new byte[33+secret.length];frame[0]=26;
        System.arraycopy(Wire.unhex(n.id),0,frame,1,32);
        System.arraycopy(secret,0,frame,33,secret.length);Arrays.fill(secret,(byte)0);
        key=n.id;admin=false;seenReplies.clear();lastRemoteStamp=0;
        begin(true,true,"Sign in",now);return frame;
    }
    byte[] command(Node n,Collection<Node> contacts,String command,long now,long epoch,int limit){
        select(n,contacts,now);
        if(!authorized(n.id,now))throw Wire.bad("Sign in with administrator permission first");
        String value=command.trim();
        byte[] text=value.getBytes(StandardCharsets.UTF_8);
        if(text.length==0||text.length>150||text.length+13>limit||value.indexOf('\0')>=0
                ||value.contains("\n")||value.contains("\r"))throw Wire.bad("Enter one command, up to 150 UTF-8 bytes");
        byte[] frame=new byte[13+text.length];frame[0]=2;frame[1]=1; // CLI_DATA, attempt zero
        Wire.put32(frame,3,epoch);
        System.arraycopy(Wire.unhex(n.id.substring(0,12)),0,frame,7,6);
        System.arraycopy(text,0,frame,13,text.length);
        Arrays.fill(text,(byte)0);
        String lower=value.toLowerCase(Locale.ROOT);
        boolean secret=lower.contains("password")||lower.contains("key")||lower.contains("secret");
        begin(false,secret,secret?"Private command":value,now);return frame;
    }
    private void begin(boolean isLogin,boolean secret,String title,long now){
        login=isLogin;sensitive=secret;label=title;reply="";submitted=false;pending=true;
        deadline=now+WAIT_MS;state="Queued for radio";revision++;
    }
    void accepted(byte[] frame,long now){
        if(!pending)return;
        if(frame.length!=10){rejected();return;}
        submitted=true;deadline=now+Math.min(WAIT_MS,Math.max(30_000,Wire.u32(frame,6)*2));
        state=login?"Waiting for repeater login":"Waiting for repeater reply; change not yet confirmed";revision++;
    }
    void rejected(){pending=false;admin=false;reply="";state="Radio rejected the operation; sign in again";revision++;}
    boolean loginResponse(byte[] frame,long now){
        tick(now);
        if(!pending||!login||!submitted||frame.length<8
                ||!key.startsWith(Wire.hex(Arrays.copyOfRange(frame,2,8))))return false;
        pending=false;
        if((frame[0]&255)==0x85){
            admin=(frame[1]&1)!=0;expires=now+SESSION_MS;
            if(frame.length>=12)lastRemoteStamp=Wire.u32(frame,8);
            state=admin?"Administrator access granted":"Login accepted without administrator permission";
        }else{admin=false;state="Login failed. Check the repeater password.";}
        revision++;return true;
    }
    boolean cliResponse(byte[] frame,int start,int offset,long now){
        tick(now);
        if(!pending||login||!submitted||frame.length<=offset
                ||!key.startsWith(Wire.hex(Arrays.copyOfRange(frame,start,start+6))))return false;
        long stamp=Wire.u32(frame,start+8);
        if(stamp<lastRemoteStamp||seenReplies.contains(stamp))return false;
        seenReplies.add(stamp);lastRemoteStamp=stamp;
        reply=sensitive?"Private response received (content hidden)":
            Wire.utf8(Arrays.copyOfRange(frame,offset,frame.length));
        pending=false;expires=now+SESSION_MS;
        state="Remote response received. Read the setting again to verify changes.";
        revision++;return true;
    }
    void forget(){key="";pending=false;admin=false;reply="";label="";seenReplies.clear();
        state="Administration session closed on this phone";revision++;}

    static String duration(long seconds){
        long s=Math.max(0,seconds),d=s/86400,h=s%86400/3600,m=s%3600/60;
        return d>0?d+"d "+h+"h":h>0?h+"h "+m+"m":m>0?m+"m":s+"s";
    }
    static String setting(String name,String input){
        String value=input.trim();
        if(value.isEmpty()||value.indexOf('\0')>=0||value.contains("\n")||value.contains("\r"))
            throw Wire.bad("Enter a value on one line");
        switch(name){
            case "name": if(value.getBytes(StandardCharsets.UTF_8).length>31)throw Wire.bad("Name: maximum 31 UTF-8 bytes");break;
            case "lat": number(value,-90,90);break;
            case "lon": number(value,-180,180);break;
            case "tx": integer(value,-9,30);break;
            case "repeat":case "allow.read.only": if(!value.equals("on")&&!value.equals("off"))throw Wire.bad("Use on or off");break;
            case "advert.interval":integer(value,0,240);if(Integer.parseInt(value)%2!=0||Integer.parseInt(value)>0&&Integer.parseInt(value)<2)throw Wire.bad("Use 0 or an even number of minutes (2–240)");break;
            case "flood.advert.interval":integer(value,0,168);if(value.equals("1")||value.equals("2"))throw Wire.bad("Use 0 or 3–168 hours");break;
            case "flood.max": integer(value,0,64);break;
            case "radio":String[] parts=value.split(",",-1);if(parts.length!=4)throw Wire.bad("Use frequency MHz,bandwidth kHz,SF,CR");number(parts[0],150,2500);number(parts[1],7,500);integer(parts[2],5,12);integer(parts[3],5,8);break;
            default:throw Wire.bad("Unsupported setting");
        }
        return "set "+name+" "+value;
    }
    private static void number(String v,double min,double max){try{double n=Double.parseDouble(v);if(!Double.isFinite(n)||n<min||n>max)throw new NumberFormatException();}catch(NumberFormatException e){throw Wire.bad("Value must be between "+min+" and "+max);}}
    private static void integer(String v,int min,int max){try{int n=Integer.parseInt(v);if(n<min||n>max)throw new NumberFormatException();}catch(NumberFormatException e){throw Wire.bad("Value must be an integer between "+min+" and "+max);}}
}
