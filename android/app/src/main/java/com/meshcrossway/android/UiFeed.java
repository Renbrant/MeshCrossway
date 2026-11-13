package com.meshcrossway.android;

import java.util.*;
import static com.meshcrossway.android.MeshData.*;

/** A combined view retains each source network/radio identity all the way to the composer. */
final class UiFeed {
    static final long RECENT_NODE_SECONDS=15*60;
    /** Recently heard is evidence of activity, not a live reachability check. */
    static int recentlyHeard(List<Node> nodes,long now){int total=0;for(Node node:nodes)if(node.heard>0&&node.heard<=now&&node.heard>=now-RECENT_NODE_SECONDS)total++;return total;}
    static final class Item {final RadioProfile profile;final String radio;final Conversation conversation;Item(RadioProfile p,String r,Conversation c){profile=p;radio=r;conversation=c;}}
    static List<Item> conversations(Map<RadioProfile,Snapshot> snapshots,Map<RadioProfile,String> radios,int network,boolean unreadOnly,boolean pinnedOnly,String search){
        ArrayList<Item> result=new ArrayList<>();String q=search.toLowerCase(Locale.ROOT);
        for(RadioProfile p:RadioProfile.values()){if(network!=0&&network!=p.ordinal()+1)continue;Snapshot snapshot=snapshots.get(p);if(snapshot==null)continue;for(Conversation c:snapshot.conversations())if((!unreadOnly||c.unread>0)&&(!pinnedOnly||c.pinned)&&(c.target.title+" "+c.last.body).toLowerCase(Locale.ROOT).contains(q))result.add(new Item(p,radios.getOrDefault(p,""),c));}
        result.sort((a,b)->a.conversation.pinned!=b.conversation.pinned?(a.conversation.pinned?-1:1):Long.compare(b.conversation.last.timestamp,a.conversation.last.timestamp));return result;
    }
}
