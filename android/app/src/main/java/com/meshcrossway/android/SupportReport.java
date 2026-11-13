package com.meshcrossway.android;

import java.util.*;
import java.util.regex.*;

/** Bounded, allowlisted metadata only. Raw messages, packets, exception text and login data never enter this report. */
final class SupportReport {
    enum Event { CONNECTION_STATE, PHASE, WRITE_ACCEPTED, WRITE_COMPLETE, RX_CODE, TRANSPORT_ERROR }
    static final String[] PHASES={"Unknown","Connecting","Disconnected","Discovering","MTU","Pairing","Queued","Ready","Settings","Subscribing","Synchronizing"};
    static final class Session {
        RadioProfile profile;int phase,mtu,retries,protocol=-1,queued,protocolQueued,approved,discovered;long frames;
        boolean connected,ready,pending;String firmware="";
    }
    private final ArrayDeque<String> events=new ArrayDeque<>();private long origin=-1;
    static int phase(String value){for(int i=0;i<PHASES.length;i++)if(PHASES[i].equals(value))return i;return 0;}
    synchronized void record(RadioProfile profile,Event event,int code,long now){if(origin<0)origin=now;events.addLast(Math.max(0,now-origin)/1000+"s "+profile.name()+" "+event.name()+" code="+code);while(events.size()>200)events.removeFirst();}
    static String version(String raw){if(raw==null)return "unknown";Matcher m=Pattern.compile("(?<![0-9.])v?([0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3})(?![0-9.])").matcher(raw);return m.find()?m.group(1):"unknown";}
    static String device(String raw){if(raw==null)return "unknown";String safe=raw.replaceAll("(?i)([0-9a-f]{2}:){5}[0-9a-f]{2}|[0-9a-f]{16,}|[A-Z0-9._%+-]+@[A-Z0-9.-]+|[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+","[redacted]").replaceAll("[^A-Za-z0-9 _./()\\[\\]-]","_");return safe.substring(0,Math.min(80,safe.length()));}
    synchronized String render(String appVersion,int sdk,String phone,List<Session> sessions){
        StringBuilder out=new StringBuilder("MeshCrossway support report v1\nApp: ").append(version(appVersion)).append("\nAndroid API: ").append(sdk).append("\nPhone model: ").append(device(phone)).append("\nPrivacy: message content, coordinates, names, identifiers, keys, credentials and raw packets are excluded.\n");
        for(Session s:sessions){out.append("\nNetwork: ").append(s.profile.name()).append("\nPhase: ").append(PHASES[Math.max(0,Math.min(PHASES.length-1,s.phase))]).append("\nConnected: ").append(s.connected).append("; synchronized: ").append(s.ready).append("; pending verification: ").append(s.pending).append("\nFirmware: ").append(version(s.firmware)).append("; Companion protocol: ").append(s.protocol).append("\nMTU: ").append(s.mtu).append("; received frames: ").append(s.frames).append("; queued operations: ").append(s.queued).append("; protocol operations: ").append(s.protocolQueued).append("; reconnect attempts: ").append(s.retries).append("\nApproved/current contacts: ").append(s.approved).append("; discoveries: ").append(s.discovered).append('\n');}
        out.append("\nPhase codes: ");for(int i=0;i<PHASES.length;i++)out.append(i).append("=").append(PHASES[i]).append(" ");out.append("\nRecent event codes (relative seconds, at most 200):\n");for(String event:events)out.append(event).append('\n');return out.toString();
    }
}
