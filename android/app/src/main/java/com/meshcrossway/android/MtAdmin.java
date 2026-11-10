package com.meshcrossway.android;

import java.util.*;

/** Local-node admin transactions. BLE submission never confirms a setting; only correlated readback does. */
final class MtAdmin {
    interface Host {void enqueue(byte[] admin,long id,boolean response);void apply(int kind,int index,byte[] body);}
    static final class Edit {
        final int kind,index,setField;final byte[] body,expected;final boolean restart;
        boolean dispatched;long writeId,deadline;
        Edit(int k,int i,int s,byte[] b,byte[] e,boolean r){kind=k;index=i;setField=s;body=b;expected=e;restart=r;}
    }
    private static final class Read {int kind,index;long deadline;Read(int k,int i,long d){kind=k;index=i;deadline=d;}}
    final Set<String> failedReads=new HashSet<>();
    final Host host;final Map<Long,Read> reads=new LinkedHashMap<>();
    byte[] key=Wire.EMPTY;long keyAt=-1,serial=(System.nanoTime()&0x7fffffffL)+1,revision;
    Edit edit;String status="",error="";long clock;boolean recovering;
    MtAdmin(Host h){host=h;}
    long id(){serial=(serial+1)&0xffffffffL;if(serial==0)serial=1;return serial;}
    void tick(long now){clock=now;Iterator<Map.Entry<Long,Read>> it=reads.entrySet().iterator();boolean expired=false;while(it.hasNext()){Read r=it.next().getValue();if(now>=r.deadline){failedReads.add(r.kind+":"+r.index);it.remove();expired=true;}}
        if(expired){error="Radio did not answer the settings request; refresh to retry";revision++;}
        if(edit!=null&&now>=edit.deadline){status=edit.dispatched?"Save not confirmed — refresh to verify":"Save cancelled — no current admin session";error=status;if(!edit.dispatched)edit=null;else edit.deadline=Long.MAX_VALUE;revision++;}
    }
    boolean fresh(){return key.length>0&&keyAt>=0&&clock-keyAt<240000;}
    void retryReads(){failedReads.clear();error="";revision++;}
    void read(int kind,int index){if(failedReads.contains(kind+":"+index))return;for(Read r:reads.values())if(r.kind==kind&&r.index==index)return;
        if(reads.size()>=16)throw Wire.bad("Wait for pending settings requests");
        Wire.Out msg=new Wire.Out();if(kind==4)msg.scalar(3,1);else if(kind==6)msg.scalar(5,index);else if(kind==8)msg.scalar(7,index);else if(kind==2)msg.scalar(1,index+1);else if(kind==13)msg.scalar(12,1);else throw Wire.bad("Unsupported admin read");
        if(fresh())msg.bytes(101,key);long id=id();reads.put(id,new Read(kind,index,clock+30000));try{host.enqueue(msg.array(),id,true);}catch(IllegalArgumentException e){reads.remove(id);failedReads.add(kind+":"+index);error=e.getMessage();status="Request could not be sent";revision++;}
    }
    void save(Edit next){if(edit!=null)throw Wire.bad("Verify the pending save before changing another setting");edit=next;error="";edit.deadline=clock+45000;status="Preparing save";revision++;if(fresh())dispatch();else{status="Refreshing admin session";read(13,0);}}
    private void dispatch(){if(edit==null||edit.dispatched||edit.writeId!=0)return;edit.writeId=id();byte[] value=edit.kind==6||edit.kind==8?new Wire.Out().bytes(edit.index+1,edit.body).array():edit.body;
        try{host.enqueue(new Wire.Out().bytes(edit.setField,value).bytes(101,key).array(),edit.writeId,false);}catch(IllegalArgumentException e){error=e.getMessage();status="Save cancelled before transmission";edit=null;revision++;return;}status=edit.restart?"Saving — radio may restart; verification pending":"Saving — waiting for radio";revision++;}
    void written(long id){if(edit!=null&&edit.writeId==id){edit.dispatched=true;revision++;}}
    void ack(long from,long to,long local,long request,long code){if(from!=local||to!=local)return;
        if(edit!=null&&edit.writeId==request){if(code!=0){error="Radio rejected settings (code "+code+")";status=error;edit=null;revision++;return;}edit.dispatched=true;verify();return;}
        Read r=reads.get(request);if(r!=null&&code!=0){reads.remove(request);failedReads.add(r.kind+":"+r.index);error="Radio rejected settings read (code "+code+")";if(edit!=null&&!edit.dispatched){edit=null;status="Save cancelled";}revision++;}
    }
    void response(long from,long to,long local,long request,byte[] payload){if(from!=local||to!=local)return;Read r=reads.get(request);if(r==null)return;
        List<Wire.F> fs=Wire.fields(payload);Wire.F f=Wire.field(fs,r.kind);if(f==null||f.wire!=2)return;
        byte[] body=f.bytes;if(r.kind==6||r.kind==8){Wire.F section=Wire.field(Wire.fields(body),r.index+1);if(section==null||section.wire!=2)return;body=section.bytes;}
        if(r.kind==2&&(int)Wire.number(Wire.fields(body),1,0)!=r.index)return;
        reads.remove(request);byte[] incoming=Wire.bytes(fs,101);if(incoming.length>0){key=incoming;keyAt=clock;}
        host.apply(r.kind,r.index,body);revision++;
        if(edit!=null&&edit.dispatched&&edit.kind==r.kind&&edit.index==r.index){if(matches(body,edit)){status="Saved and verified by radio";error="";edit=null;recovering=false;}else{status="Radio values differ — save not confirmed";error=status;edit.deadline=Long.MAX_VALUE;}revision++;}
        if(edit!=null&&!edit.dispatched&&edit.writeId==0&&fresh())dispatch();
    }
    private boolean matches(byte[] body,Edit edit){if(edit.kind==6&&edit.index==5){Wire.Out expected=new Wire.Out();for(Wire.F f:Wire.fields(edit.expected)){if(f.tag==10&&f.number==0){long actual=Wire.number(Wire.fields(body),10,0);if(actual<0||actual>36)return false;}else expected.add(f);}return Wire.containsFields(body,expected.array());}if(edit.kind!=2)return Wire.containsFields(body,edit.expected);for(Wire.F f:Wire.fields(edit.expected)){if(f.tag==2){if(!Wire.containsFields(Wire.bytes(Wire.fields(body),2),f.bytes))return false;}else if(!Wire.containsFields(body,new Wire.Out().add(f).array()))return false;}return true;}
    void discard(){if(edit!=null&&edit.dispatched&&edit.deadline!=Long.MAX_VALUE)throw Wire.bad("Wait for verification before discarding");edit=null;status="Pending edit discarded; radio values retained";error="";revision++;}
    void verify(){if(edit==null||!edit.dispatched)return;status="Reading radio to verify save";edit.deadline=clock+30000;read(edit.kind,edit.index);revision++;}
    void restore(Edit previous){edit=previous;edit.dispatched=true;recovering=true;status="Radio reconnected — checking previous save";edit.deadline=Long.MAX_VALUE;revision++;}
    void ready(){if(recovering&&edit!=null){recovering=false;verify();}}
}
