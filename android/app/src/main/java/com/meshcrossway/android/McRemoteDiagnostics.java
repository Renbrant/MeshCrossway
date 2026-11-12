package com.meshcrossway.android;

import java.security.SecureRandom;
import java.util.*;
import static com.meshcrossway.android.MeshData.*;

/**
 * One user-triggered MeshCore binary request per Companion session. The radio
 * acknowledges local submission with RESP_CODE_SENT (6), while a later tagged
 * PUSH_CODE_BINARY_RESPONSE (0x8c) carries the remote repeater reply.
 *
 * Never auto-retry: after disconnect the whole ProtocolClient is discarded.
 */
final class McRemoteDiagnostics {
    enum Kind { STATUS, NEIGHBORS }
    static final int CMD_BINARY=50, RESP_SENT=6, PUSH_BINARY=0x8c;
    static final long COOLDOWN_MS=30_000L, MAX_WAIT_MS=120_000L;
    private static final int MAX_RECORDS=24, NEIGHBOR_PAGE=8, PREFIX_BYTES=6;
    private final SecureRandom random=new SecureRandom();
    private final LinkedHashMap<String,Result> entries=new LinkedHashMap<>();
    private final LinkedHashMap<String,Long> lastRequested=new LinkedHashMap<>();
    Result pending;
    long revision;

    static final class Status {
        final int batteryMv,queue,noiseFloor,lastRssi,fullEvents;
        final double lastSnr;
        final long received,sent,txAirtime,rxAirtime,uptime,sentFlood,sentDirect,recvFlood,recvDirect,directDup,floodDup,errors;
        final boolean errorsKnown;
        final int extensionBytes;
        Status(byte[] d){
            // MeshCore's maintained clients decode the fixed fields by offset
            // and tolerate later firmware extensions. Reject truncated frames,
            // but do not reject a 60-byte response merely for extending 56.
            if(d.length<52||d.length>170)
                throw Wire.bad("Truncated or oversized repeater status");
            extensionBytes=d.length-Math.min(d.length,56);
            batteryMv=Wire.u16(d,0);
            queue=Wire.u16(d,2);
            noiseFloor=(short)Wire.u16(d,4);
            lastRssi=(short)Wire.u16(d,6);
            received=Wire.u32(d,8);sent=Wire.u32(d,12);
            txAirtime=Wire.u32(d,16);uptime=Wire.u32(d,20);
            sentFlood=Wire.u32(d,24);sentDirect=Wire.u32(d,28);
            recvFlood=Wire.u32(d,32);recvDirect=Wire.u32(d,36);
            fullEvents=Wire.u16(d,40);
            lastSnr=((short)Wire.u16(d,42))/4.0;
            directDup=Wire.u16(d,44);floodDup=Wire.u16(d,46);
            rxAirtime=Wire.u32(d,48);errorsKnown=d.length>=56;
            errors=errorsKnown?Wire.u32(d,52):-1;
        }
    }

    static final class Neighbor {
        final String prefix;
        final long secondsAgo;
        final double snr;
        Neighbor(String prefix,long secondsAgo,double snr){
            this.prefix=prefix;this.secondsAgo=secondsAgo;this.snr=snr;
        }
    }
    static final class Neighbors {
        final int total,offset,extensionBytes;
        final List<Neighbor> list;
        Neighbors(int total,int offset,List<Neighbor> list,int extensionBytes){
            this.total=total;this.offset=offset;this.extensionBytes=extensionBytes;
            this.list=Collections.unmodifiableList(new ArrayList<>(list));
        }
    }
    static final class Result {
        final String fullKey;
        final Kind kind;
        final long requestedAt;
        final int offset;
        String state="Queued for local radio",error="";
        long tag=-1,deadline,completedAt;
        Boolean usedFlood; // Reported by RESP_CODE_SENT byte 1, not a prediction.
        String radioRoute(){
            if(usedFlood==null)return "Radio transmission mode not yet reported";
            return usedFlood?"Local radio used flood routing; multiple mesh relays may transmit":
                "Local radio reported a known path; relays may still transmit";
        }
        Status stats;
        Neighbors neighbors;
        Result(String fullKey,Kind kind,int offset,long now){
            this.fullKey=fullKey;this.kind=kind;this.offset=offset;requestedAt=now;
        }
        boolean inFlight(){return "Queued for local radio".equals(state)
                ||"Awaiting remote repeater response".equals(state);}
        String freshness(long now){
            if(completedAt<=0)return state;
            return now-completedAt<=120000?state+" · recent":state+" · historical";
        }
    }

    Result find(String fullKey){
        if(fullKey==null)return null;
        Result last=null;
        for(Result result:entries.values())
            if(fullKey.equalsIgnoreCase(result.fullKey))last=result;
        return last;
    }
    Result find(String fullKey,Kind kind){
        if(fullKey==null||kind==null)return null;
        return entries.get(fullKey.toLowerCase(Locale.ROOT)+"|"+kind);
    }

    /** Remaining time for this exact repeater, shared by status and neighbor pages. */
    long remainingCooldownMs(String key,long now){
        if(key==null)return 0;
        Long last=lastRequested.get(key.toLowerCase(Locale.ROOT));
        if(last==null)return 0;
        long elapsed=now-last;
        if(elapsed<0)return COOLDOWN_MS; // monotonic clock discontinuity
        return Math.max(0,COOLDOWN_MS-elapsed);
    }
    static long remainingFromWallClock(long last,long now){
        if(last<=0)return 0;
        if(now<last)return COOLDOWN_MS;
        return Math.max(0,COOLDOWN_MS-(now-last));
    }
    static String remainingLabel(long remainingMs){
        long seconds=Math.max(1,(remainingMs+999L)/1000L);
        return "Wait "+seconds+" second"+(seconds==1?"":"s")+
            " before querying this repeater again";
    }

    /**
     * Request frames use the current firmware's CMD_SEND_BINARY_REQ (50).
     * req_status payload = 0x01; req_neighbours payload =
     * 0x06,version0,count,offsetLE,sort0,prefixLen6,nonceLE.
     */
    byte[] request(Kind kind,Node repeater,int offset,long now){
        if(!RepeaterInventory.isRepeater(repeater)||repeater.mcState!=ProtocolClient.MC_APPROVED)
            throw Wire.bad("Select an approved MeshCore repeater with a full public key");
        if(pending!=null&&pending.inFlight())throw Wire.bad("Wait for the current remote repeater request");
        if(offset<0||offset>1024||(kind==Kind.STATUS&&offset!=0))
            throw Wire.bad("Invalid repeater request page");
        String key=repeater.id.toLowerCase(Locale.ROOT);
        long remaining=remainingCooldownMs(key,now);
        if(remaining>0)throw Wire.bad(remainingLabel(remaining));
        byte[] suffix;
        if(kind==Kind.STATUS)suffix=new byte[]{1};
        else{
            suffix=new byte[11];
            suffix[0]=6; // binary neighbours request
            suffix[1]=0; // format v0
            suffix[2]=(byte)NEIGHBOR_PAGE;
            suffix[3]=(byte)offset;suffix[4]=(byte)(offset>>>8);
            suffix[5]=0; // newest first
            suffix[6]=PREFIX_BYTES;
            Wire.put32(suffix,7,random.nextInt()&0xffffffffL);
        }
        byte[] raw=new byte[1+32+suffix.length];
        raw[0]=CMD_BINARY;
        System.arraycopy(Wire.unhex(key),0,raw,1,32);
        System.arraycopy(suffix,0,raw,33,suffix.length);
        Result entry=new Result(key,kind,offset,now);
        String entryKey=key+"|"+kind;
        entries.remove(entryKey);entries.put(entryKey,entry);
        // One shared per-repeater limit: status, first neighbors page, and every next page.
        lastRequested.put(key,now);
        while(entries.size()>MAX_RECORDS)entries.remove(entries.keySet().iterator().next());
        while(lastRequested.size()>MAX_RECORDS*2)lastRequested.remove(lastRequested.keySet().iterator().next());
        pending=entry;revision++;
        return raw;
    }

    void sentToRadio(byte[] frame,long now){
        if(pending==null||!pending.inFlight())return;
        if(frame.length!=10||(frame[0]&255)!=RESP_SENT){
            fail("Invalid local radio submission acknowledgement");return;
        }
        long tag=Wire.u32(frame,2);
        if(tag==0){fail("Invalid radio request identifier");return;}
        pending.tag=tag;
        pending.usedFlood=(frame[1]&1)!=0;
        // Radio estimated_timeout is approximate; allow time for a multi-hop route
        // but never run a background retry or indefinite pending request.
        pending.deadline=now+Math.min(MAX_WAIT_MS,Math.max(30000L,Wire.u32(frame,6)*2L));
        pending.state="Awaiting remote repeater response";
        revision++;
    }
    void rejected(int code){
        if(pending!=null){fail("Local radio rejected request (code "+code+")");}
    }
    private void fail(String message){
        if(pending==null)return;
        pending.state="Error";pending.error=message;pending.completedAt=0;
        pending=null;revision++;
    }
    void tick(long now){
        if(pending!=null&&pending.inFlight()&&
           (pending.deadline>0&&now>=pending.deadline||
            pending.deadline==0&&now-pending.requestedAt>30000)){
            pending.state="Timed out (delivery to repeater unknown)";
            pending=null;revision++;
        }
    }

    /**
     * Caller supplies only 0x8c frames; tag correlation is mandatory.
     * Firmware does not put the sender's full pubkey in this push, therefore
     * trust only the one pending request for this Companion session. The
     * remote response is session/tag-correlated, not a cryptographic proof.
     */
    boolean receive(byte[] frame,long now){
        if(frame.length<6||(frame[0]&255)!=PUSH_BINARY||pending==null
                ||pending.tag<=0||!"Awaiting remote repeater response".equals(pending.state)
                ||Wire.u32(frame,2)!=pending.tag)return false;
        if(frame[1]!=0){fail("Unsupported MeshCore response header (reserved byte "+(frame[1]&255)+")");return true;}
        try{
            byte[] data=Arrays.copyOfRange(frame,6,frame.length);
            if(pending.kind==Kind.STATUS)pending.stats=new Status(data);
            else pending.neighbors=parseNeighbors(data,pending.offset);
            pending.state="Received from repeater (tag-correlated)";
            pending.completedAt=now;
            pending=null;revision++;
        }catch(IllegalArgumentException malformed){
            // Only expose envelope dimensions, never raw response bytes, public keys,
            // packet identifiers or private payloads in a support screenshot.
            int payloadBytes=frame.length-6;
            if(pending.kind==Kind.STATUS)
                fail("Unexpected status reply: "+payloadBytes+
                    " data bytes (requires at least 52; optional extensions are supported). Verify remote repeater firmware and access.");
            else
                fail("Unexpected neighbors reply: "+payloadBytes+
                    " data bytes (invalid count or truncated records; max 8 records per page). Verify repeater firmware.");
        }
        return true;
    }

    static Neighbors parseNeighbors(byte[] frame,int offset){
        if(frame.length<4)throw Wire.bad("Truncated neighbors header");
        int total=Wire.u16(frame,0),count=Wire.u16(frame,2);
        // The official meshcore.js and meshcore_py decoders consume the
        // advertised number of records rather than demanding exact length.
        // Allow at most a small bounded suffix for firmware extensions, never
        // accept an empty page with a large unrelated response attached.
        int expected=4+count*(PREFIX_BYTES+5);
        int suffix=frame.length-expected;
        if(total>4096||count>NEIGHBOR_PAGE||count>total
           ||(count>0&&offset+count>total)
           ||suffix<0||suffix>4
           ||(count==0&&suffix!=0))
            throw Wire.bad("Invalid neighbors response");
        ArrayList<Neighbor> list=new ArrayList<>();
        for(int i=0;i<count;i++){
            int at=4+i*(PREFIX_BYTES+5);
            list.add(new Neighbor(Wire.hex(Arrays.copyOfRange(frame,at,at+PREFIX_BYTES)),
                Wire.u32(frame,at+PREFIX_BYTES),
                frame[at+PREFIX_BYTES+4]/4.0));
        }
        return new Neighbors(total,offset,list,suffix);
    }
}
