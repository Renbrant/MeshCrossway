package com.meshcrossway.android;

import static com.meshcrossway.android.MeshData.*;

/** Distinguishes current radio membership from offline saved history. */
final class McContacts {
    static int state(Node node,ProtocolClient client){
        if(client!=null&&client.ready){if(client.nodes.containsKey(node.id))return ProtocolClient.MC_APPROVED;if(client.mcDiscoveries.containsKey(node.id))return ProtocolClient.MC_DISCOVERED;return node.mcState==ProtocolClient.MC_DISCOVERED?ProtocolClient.MC_DISCOVERED:ProtocolClient.MC_RETIRED;}
        return node.mcState>=1&&node.mcState<=3?node.mcState:ProtocolClient.MC_RETIRED;
    }
    static String label(int state){switch(state){case ProtocolClient.MC_APPROVED:return "Approved";case ProtocolClient.MC_DISCOVERED:return "Discovered — approval required";default:return "Historical — not in current radio contacts";}}
    static String label(Node node,ProtocolClient client){String value=label(state(node,client));return client==null||!client.ready?value+" · last saved state":value;}
}
