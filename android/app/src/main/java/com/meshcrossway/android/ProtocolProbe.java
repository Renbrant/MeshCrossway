package com.meshcrossway.android;

import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;

/** Read-only compatibility probe, referenced against the existing firmware's wire formats.
 * This alpha does not decode/store messages or transmit mesh packets. */
public final class ProtocolProbe {
    private final long nonce;
    private boolean myInfo, metadata, lora;
    public boolean complete;
    public String detail="Waiting for protocol response";
    public ProtocolProbe(long nonce) { this.nonce=nonce & 0xffffffffL; }
    public byte[] meshtasticRequest() {
        ByteArrayOutputStream out=new ByteArrayOutputStream(); out.write(0x18);
        long n=nonce;
        while(n>127) { out.write((int)(n & 127)|128); n>>>=7; }
        out.write((int)n); return out.toByteArray();
    }
    public static byte[] meshcoreQuery() { return new byte[]{22,14}; }
    public static byte[] meshcoreAppStart() {
        byte[] name="MeshCrossway".getBytes(StandardCharsets.UTF_8), result=new byte[8+name.length];
        result[0]=1; System.arraycopy(name,0,result,8,name.length); return result;
    }
    public void meshtastic(byte[] frame) {
        if(frame.length>512) throw new IllegalArgumentException("Oversized PhoneAPI frame");
        Cursor c=new Cursor(frame);
        while(c.pos<frame.length) {
            long key=c.varint(); int tag=(int)(key>>>3), wire=(int)(key&7);
            if(tag==0) throw new IllegalArgumentException("Invalid protobuf tag");
            long value=0; byte[] bytes=null;
            if(wire==0) value=c.varint();
            else if(wire==2) bytes=c.bytes();
            else if(wire==1) c.skip(8);
            else if(wire==5) c.skip(4);
            else throw new IllegalArgumentException("Invalid protobuf wire type");
            if(tag==3 && bytes!=null) myInfo=hasField(bytes,1,0);
            if(tag==13 && bytes!=null) metadata=true;
            if(tag==5 && bytes!=null && hasField(bytes,6,2)) lora=true;
            if(tag==7 && wire==0 && value==nonce) {
                if(!myInfo || !metadata || !lora) throw new IllegalArgumentException("Incomplete PhoneAPI configuration");
                complete=true; detail="PhoneAPI configuration received • messaging not enabled in alpha01";
            }
            if(tag==8 && value!=0) throw new IllegalArgumentException("Radio restarted; reconnect");
        }
    }
    private static boolean hasField(byte[] data, int wanted, int wantedWire) {
        Cursor c=new Cursor(data); boolean found=false;
        while(c.pos<data.length) {
            long key=c.varint(); int tag=(int)(key>>>3), wire=(int)(key&7);
            if(tag==0) throw new IllegalArgumentException("Invalid nested tag");
            if(tag==wanted && wire==wantedWire) found=true;
            if(wire==0)c.varint(); else if(wire==2)c.bytes(); else if(wire==1)c.skip(8); else if(wire==5)c.skip(4);
            else throw new IllegalArgumentException("Invalid nested wire type");
        }
        return found;
    }
    public boolean meshcore(byte[] frame) {
        if(frame.length==0 || frame.length>176) throw new IllegalArgumentException("Invalid Companion frame length");
        int code=frame[0]&255;
        if(code==1 || code==15) throw new IllegalArgumentException("Companion rejected compatibility probe");
        if(code==13) {
            if(frame.length<8 || (frame[1]&255)<3) throw new IllegalArgumentException("Companion protocol v3 or newer required");
            detail="Companion v"+(frame[1]&255)+" detected • reading identity"; return true;
        }
        if(code==5) {
            if(frame.length<58) throw new IllegalArgumentException("Truncated Companion identity");
            complete=true; detail="Companion identity received • messaging not enabled in alpha01";
        }
        return false;
    }
    private static final class Cursor {
        final byte[] data; int pos;
        Cursor(byte[] data){this.data=data;}
        void skip(int n){if(n<0 || n>data.length-pos)throw new IllegalArgumentException("Truncated protobuf"); pos+=n;}
        byte[] bytes(){long n=varint(); if(n<0 || n>data.length-pos)throw new IllegalArgumentException("Truncated protobuf field"); int p=pos; skip((int)n); return java.util.Arrays.copyOfRange(data,p,pos);}
        long varint(){long v=0;for(int i=0;i<10;i++){if(pos==data.length)break;int b=data[pos++]&255;if(i==9 && b>1)break;v|=(long)(b&127)<<(7*i);if((b&128)==0)return v;}throw new IllegalArgumentException("Invalid protobuf varint");}
    }
}
