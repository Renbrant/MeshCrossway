package com.meshcrossway.android;

import java.util.*;

/** Serialized Companion settings transactions. Only readback confirms a save; never replay a SET. */
final class McAdmin {
    interface Host {
        void enqueue(byte[] command,int response,Edit edit,boolean verification);
        void cancel(Edit edit);
        void refresh();
    }
    static final class Check {
        final byte[] request,expected;final int response,offset;final String label;final boolean missing,time;
        Check(byte[] request,int response,int offset,byte[] expected,String label){this(request,response,offset,expected,label,false,false);}
        Check(byte[] request,int response,int offset,byte[] expected,String label,boolean missing,boolean time){this.request=request;this.response=response;this.offset=offset;this.expected=expected;this.label=label;this.missing=missing;this.time=time;}
        boolean matches(byte[] frame){
            if(missing)return false;
            if(response==18&&offset==1&&expected.length==49){return frame.length>=50&&frame[1]==expected[0]&&Wire.fixedText(frame,2,32).equals(Wire.fixedText(expected,1,32))&&Arrays.equals(Arrays.copyOfRange(frame,34,50),Arrays.copyOfRange(expected,33,49));}
            if(time){if(frame.length<offset+4)return false;long actual=Wire.u32(frame,offset),wanted=Wire.u32(expected,0);return actual>=wanted&&actual<=wanted+60;}
            if(response==5&&offset==36&&expected.length==8){if(frame.length<44)return false;for(int i=0;i<8;i+=4)if(Math.abs((long)(int)Wire.u32(frame,offset+i)-(int)Wire.u32(expected,i))>1)return false;return true;}
            if(response==5&&offset==48&&expected.length==10){if(frame.length<58)return false;for(int i=0;i<8;i+=4)if(Math.abs(Wire.u32(frame,offset+i)-Wire.u32(expected,i))>1)return false;return frame[56]==expected[8]&&frame[57]==expected[9];}
            return frame.length>=offset+expected.length&&Arrays.equals(expected,Arrays.copyOfRange(frame,offset,offset+expected.length))&&(!(response==5&&offset==58||response==28)||frame.length==offset+expected.length);
        }
    }
    static final class Edit {
        final String label;final byte[][] writes;final List<Check> checks;final List<byte[]> reads=new ArrayList<>();final List<Integer> responses=new ArrayList<>();
        int writeIndex,readIndex,recoveries;boolean dispatched,verifying;long deadline;String difference="";
        Edit(String label,byte[][] writes,Check... checks){
            this.label=label;this.writes=writes;this.checks=Arrays.asList(checks);
            for(Check check:checks){boolean found=false;for(byte[] read:reads)if(Arrays.equals(read,check.request))found=true;if(!found){reads.add(check.request);responses.add(check.response);}}
        }
    }
    final Host host;Edit edit;String status="",error="";long revision,clock;boolean recovering;
    McAdmin(Host host){this.host=host;}
    void save(Edit next){if(edit!=null)throw Wire.bad("Wait for the pending MeshCore change to be verified");edit=next;error="";status="Sending "+next.label+" — verification pending";next.deadline=clock+30000;revision++;sendWrite();}
    private void sendWrite(){host.enqueue(edit.writes[edit.writeIndex],0,edit,false);}
    void written(Edit transaction){if(transaction!=edit||edit==null)return;edit.dispatched=true;revision++;}
    void reply(Edit transaction,boolean verification,byte[] frame){
        if(transaction!=edit||edit==null)return;int code=frame[0]&255;
        if(code==1||code==15){
            if(verification&&frame.length>1&&frame[1]==2&&acceptMissing())return;
            fail("Radio rejected "+edit.label+" (code "+(frame.length>1?frame[1]&255:code)+")");return;
        }
        if(!verification){
            edit.dispatched=true;
            if(++edit.writeIndex<edit.writes.length){edit.deadline=clock+30000;sendWrite();}
            else verify();
            return;
        }
        for(Check check:edit.checks)if(check.response==code&&Arrays.equals(check.request,edit.reads.get(edit.readIndex))&&!check.matches(frame)&&edit.difference.isEmpty())edit.difference=check.label+" differs from the requested value";
        nextRead();
    }
    private boolean acceptMissing(){
        boolean expected=false;for(Check check:edit.checks)if(check.missing&&Arrays.equals(check.request,edit.reads.get(edit.readIndex)))expected=true;
        if(expected)nextRead();return expected;
    }
    private void nextRead(){
        if(++edit.readIndex<edit.reads.size()){enqueueRead();return;}
        if(edit.difference.isEmpty()){status="Saved and verified by radio — "+edit.label;error="";}
        else {error="Change not confirmed — "+edit.difference+"; showing radio values";status=error;}
        edit=null;recovering=false;revision++;
    }
    void verify(){
        if(edit==null||!edit.dispatched)return;host.cancel(edit);edit.verifying=true;edit.readIndex=0;edit.difference="";recovering=false;
        error="";status="Reading the radio to verify "+edit.label;revision++;enqueueRead();
    }
    private void enqueueRead(){edit.deadline=clock+30000;host.enqueue(edit.reads.get(edit.readIndex),edit.responses.get(edit.readIndex),edit,true);}
    private void fail(String message){Edit previous=edit;edit=null;host.cancel(previous);error=message+"; refreshing current settings";status=error;recovering=false;revision++;host.refresh();}
    void tick(long now){clock=now;if(edit!=null&&edit.deadline!=Long.MAX_VALUE&&now>=edit.deadline)timeout(edit);}
    void timeout(Edit transaction){
        if(transaction!=edit||edit==null)return;host.cancel(edit);
        if(!edit.dispatched){edit=null;error="Save cancelled before transmission";}
        else{edit.deadline=Long.MAX_VALUE;error="Save not confirmed — check the radio again or use current settings";}
        status=error;revision++;
    }
    void restore(Edit previous){edit=previous;edit.dispatched=true;edit.deadline=Long.MAX_VALUE;recovering=++edit.recoveries<2;status=recovering?"Waiting for reconnection to verify the previous change":"Reconnected — check the previous change again or use current settings";error=recovering?"":status;revision++;}
    void ready(){if(recovering&&edit!=null)verify();}
    void discard(){if(edit!=null){host.cancel(edit);edit=null;}recovering=false;error="";status="Showing current radio settings";revision++;host.refresh();}
}
