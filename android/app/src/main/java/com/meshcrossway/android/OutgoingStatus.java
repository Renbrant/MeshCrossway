package com.meshcrossway.android;

import java.util.Objects;
import static com.meshcrossway.android.MeshData.*;

/**
 * Read-only presentation of durable outgoing message states.
 * This class never schedules transmissions or converts a saved message into a
 * queued packet. All counts are scoped to one radio snapshot and its protocol.
 */
final class OutgoingStatus {
    private OutgoingStatus() {}

    static final class Counts {
        final int pending,review;
        Counts(int pending,int review){this.pending=pending;this.review=review;}
        boolean hasAttention(){return pending>0||review>0;}
        String brief(){
            if(!hasAttention())return "";
            StringBuilder out=new StringBuilder("Recent: ");
            if(pending>0)out.append(pending).append(" awaiting ACK/send");
            if(review>0){
                if(pending>0)out.append(" · ");
                out.append(review).append(" need review");
            }
            return out.toString();
        }
    }

    static boolean pending(Message message){
        if(message==null||!message.outgoing)return false;
        switch(message.state){
            case "queued":
            case "submitted":return true;
            case "radio accepted":return !message.group;
            default:return false;
        }
    }

    static boolean needsReview(Message message){
        if(message==null||!message.outgoing)return false;
        return "uncertain".equals(message.state)||"rejected".equals(message.state);
    }

    /** Never make a retransmission action available for queued or ACKed packets. */
    static boolean canPrepareNewDraft(Message message){
        return needsReview(message)&&message.body!=null&&!message.body.isEmpty();
    }

    /**
     * A saved message cannot be manually redrafted for another protocol, radio,
     * recipient, or channel key just because the displayed name is the same.
     */
    static boolean sameComposer(RadioProfile selectedNetwork,RadioProfile sourceNetwork,
                                String selectedRadio,String sourceRadio,Target selected,Message message){
        if(selectedNetwork!=sourceNetwork||!Objects.equals(selectedRadio,sourceRadio)
                ||selected==null||message==null||!message.outgoing)return false;
        return Objects.equals(selected.topic,message.topic)
                &&Objects.equals(selected.identity,message.identity)
                &&Objects.equals(selected.peer,message.peer)
                &&selected.slot==message.slot&&selected.group==message.group;
    }

    static Counts counts(Snapshot snapshot){
        int pending=0,review=0;
        if(snapshot!=null)for(Message message:snapshot.messages){
            if(pending(message))pending++;
            else if(needsReview(message))review++;
        }
        return new Counts(pending,review);
    }

    static String label(Message message){
        if(message==null||!message.outgoing)return "";
        switch(message.state){
            case "queued":return "Saved locally · waiting for radio submission";
            case "submitted":return "Submitted over Bluetooth · radio/peer delivery not confirmed";
            case "radio accepted":
                return message.group
                        ?"Accepted by radio · other participants not confirmed"
                        :"Accepted by radio · awaiting peer acknowledgement";
            case "delivered":return "Peer acknowledged · not a read receipt";
            case "rejected":return "Rejected · use a new draft only after review";
            case "uncertain":return "Delivery unknown · no automatic resend";
            default:return "Status unavailable";
        }
    }
}
