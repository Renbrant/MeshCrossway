package com.meshcrossway.android;

import java.util.*;
import static com.meshcrossway.android.MeshData.*;

/**
 * Read-only repeater inventory from the MeshCore radio's saved contacts.
 * Does not issue remote status requests or turn stored contacts into live links.
 */
final class RepeaterInventory {
    static final int MAX_VISIBLE=60;
    private RepeaterInventory(){}

    static boolean isRepeater(Node node){
        return node!=null&&node.type==2&&node.id!=null
            &&node.id.matches("(?i)[0-9a-f]{64}");
    }

    static List<Node> visible(Snapshot snapshot){
        if(snapshot==null)return Collections.emptyList();
        ArrayList<Node> matches=new ArrayList<>();
        for(Node node:snapshot.nodes)if(isRepeater(node))matches.add(node);
        // mcReceived is a locally observed arrival time; heard may also
        // reflect a remote modification timestamp. Neither proves online status.
        matches.sort((a,b)->{
            int order=Long.compare(b.mcReceived,a.mcReceived);
            if(order!=0)return order;
            order=Long.compare(b.heard,a.heard);
            return order!=0?order:a.id.compareToIgnoreCase(b.id);
        });
        return Collections.unmodifiableList(new ArrayList<>(
            matches.subList(0,Math.min(MAX_VISIBLE,matches.size()))));
    }

    static int total(Snapshot snapshot){
        if(snapshot==null)return 0;
        int count=0;
        for(Node node:snapshot.nodes)if(isRepeater(node))count++;
        return count;
    }

    static String activity(Node node,long nowSeconds){
        if(!isRepeater(node))return "Not a verified repeater contact";
        long received=node.mcReceived;
        if(received<=0||received>nowSeconds+60)return "No confirmed local reception timestamp";
        long age=Math.max(0,nowSeconds-received);
        if(age<=15*60)return "Observed locally within 15 minutes";
        if(age<=24*3600)return "Last local observation "+Math.max(1,age/3600)+" h ago";
        return "Last local observation "+Math.max(1,age/86400)+" d ago · historical";
    }
}
