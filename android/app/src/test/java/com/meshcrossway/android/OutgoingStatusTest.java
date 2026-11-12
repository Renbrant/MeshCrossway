package com.meshcrossway.android;

import org.junit.Test;
import static org.junit.Assert.*;
import static com.meshcrossway.android.MeshData.*;
import java.util.*;

public class OutgoingStatusTest {
    private static Message outgoing(String state,boolean group){
        Message m=new Message();m.uid=UUID.randomUUID().toString();
        m.topic="c:2:one-key";m.identity="one-key";m.peer="";
        m.slot=2;m.group=group;m.body="Original message";m.state=state;
        m.outgoing=true;return m;
    }

    @Test public void pendingAndReviewDoNotConfuseGroupAndDirectAck(){
        Message queued=outgoing("queued",true);
        Message submitted=outgoing("submitted",true);
        Message groupAccepted=outgoing("radio accepted",true);
        Message directAccepted=outgoing("radio accepted",false);
        Message delivered=outgoing("delivered",false);
        Message rejected=outgoing("rejected",true);
        Message uncertain=outgoing("uncertain",false);
        assertTrue(OutgoingStatus.pending(queued));
        assertTrue(OutgoingStatus.pending(submitted));
        assertFalse(OutgoingStatus.pending(groupAccepted));
        assertTrue(OutgoingStatus.pending(directAccepted));
        assertFalse(OutgoingStatus.pending(delivered));
        assertFalse(OutgoingStatus.pending(rejected));
        assertTrue(OutgoingStatus.needsReview(rejected));
        assertTrue(OutgoingStatus.needsReview(uncertain));
        assertFalse(OutgoingStatus.canPrepareNewDraft(groupAccepted));
        assertFalse(OutgoingStatus.canPrepareNewDraft(delivered));
        assertTrue(OutgoingStatus.canPrepareNewDraft(uncertain));
        assertTrue(OutgoingStatus.label(groupAccepted).contains("not confirmed"));
        assertTrue(OutgoingStatus.label(delivered).contains("not a read receipt"));
    }

    @Test public void countsIncludeOnlyOutgoingRecordsFromOneSnapshot(){
        Message received=outgoing("uncertain",false);received.outgoing=false;
        Snapshot one=new Snapshot(new ArrayList<>(),new ArrayList<>(),Arrays.asList(
                outgoing("queued",true),outgoing("radio accepted",false),
                outgoing("radio accepted",true),outgoing("uncertain",false),
                outgoing("rejected",true),outgoing("delivered",false),received));
        OutgoingStatus.Counts counts=OutgoingStatus.counts(one);
        assertEquals(2,counts.pending);
        assertEquals(2,counts.review);
        assertTrue(counts.brief().contains("2 awaiting ACK/send"));
        assertTrue(counts.brief().contains("2 need review"));
        Snapshot empty=Snapshot.empty();
        assertFalse(OutgoingStatus.counts(empty).hasAttention());
    }

    @Test public void draftPreparationRequiresExactRadioNetworkRecipientAndKey(){
        Message message=outgoing("uncertain",true);
        Target expected=message.target();
        assertTrue(OutgoingStatus.sameComposer(RadioProfile.MESHCORE,RadioProfile.MESHCORE,
                "radio-1","radio-1",expected,message));
        assertFalse(OutgoingStatus.sameComposer(RadioProfile.MESHTASTIC,RadioProfile.MESHCORE,
                "radio-1","radio-1",expected,message));
        assertFalse(OutgoingStatus.sameComposer(RadioProfile.MESHCORE,RadioProfile.MESHCORE,
                "radio-2","radio-1",expected,message));
        Target differentKey=new Target(message.topic,message.title,message.peer,
                "changed-key",message.slot,message.group);
        assertFalse(OutgoingStatus.sameComposer(RadioProfile.MESHCORE,RadioProfile.MESHCORE,
                "radio-1","radio-1",differentKey,message));
        Target differentRecipient=new Target(message.topic,message.title,"other-recipient",
                message.identity,message.slot,message.group);
        assertFalse(OutgoingStatus.sameComposer(RadioProfile.MESHCORE,RadioProfile.MESHCORE,
                "radio-1","radio-1",differentRecipient,message));
        message.outgoing=false;
        assertFalse(OutgoingStatus.sameComposer(RadioProfile.MESHCORE,RadioProfile.MESHCORE,
                "radio-1","radio-1",expected,message));
    }

    @Test public void notSentAndDeliveredMessagesCannotBePreparedAsRetries(){
        Message m=outgoing("uncertain",false);
        assertTrue(OutgoingStatus.canPrepareNewDraft(m));
        m.state="submitted";assertFalse(OutgoingStatus.canPrepareNewDraft(m));
        m.state="radio accepted";assertFalse(OutgoingStatus.canPrepareNewDraft(m));
        m.state="delivered";assertFalse(OutgoingStatus.canPrepareNewDraft(m));
        m.state="rejected";m.body="";assertFalse(OutgoingStatus.canPrepareNewDraft(m));
        m.state="uncertain";m.body="content";m.outgoing=false;
        assertFalse(OutgoingStatus.canPrepareNewDraft(m));
    }
}
