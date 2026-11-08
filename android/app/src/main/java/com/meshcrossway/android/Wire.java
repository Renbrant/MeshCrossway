package com.meshcrossway.android;

import java.io.ByteArrayOutputStream;
import java.nio.*;
import java.nio.charset.*;
import java.security.*;
import java.util.*;

/** Small bounded protobuf / Companion primitives. No generated upstream code. */
final class Wire {
    static final byte[] EMPTY=new byte[0];
    static final class F {int tag,wire;long number;byte[] bytes=EMPTY;}
    static List<F> fields(byte[] input){
        if(input.length>512)throw bad("Oversized protobuf");
        ArrayList<F> result=new ArrayList<>();int[] pos={0};
        while(pos[0]<input.length){
            long key=varint(input,pos);if(key<8 || key>0xffffffffL)throw bad("Invalid protobuf tag");
            F f=new F();f.tag=(int)(key>>>3);f.wire=(int)(key&7);
            if(f.wire==0)f.number=varint(input,pos);
            else {long length=f.wire==2?varint(input,pos):f.wire==5?4:f.wire==1?8:-1;
                if(length<0 || length>input.length-pos[0])throw bad("Truncated protobuf");
                int end=pos[0]+(int)length;f.bytes=Arrays.copyOfRange(input,pos[0],end);
                if(f.wire!=2)for(int i=0;i<length;i++)f.number|=(long)(f.bytes[i]&255)<<(8*i);
                pos[0]=end;
            }result.add(f);
        }return result;
    }
    private static long varint(byte[] in,int[] pos){long n=0;for(int i=0;i<10;i++){if(pos[0]>=in.length)throw bad("Truncated varint");int b=in[pos[0]++]&255;if(i==9&&b>1)throw bad("Varint overflow");n|=(long)(b&127)<<(7*i);if((b&128)==0)return n;}throw bad("Invalid varint");}
    static F field(List<F> fields,int tag){for(F f:fields)if(f.tag==tag)return f;return null;}
    static long number(List<F> fields,int tag,long fallback){F f=field(fields,tag);return f==null?fallback:f.number;}
    static byte[] bytes(List<F> fields,int tag){F f=field(fields,tag);return f==null?EMPTY:f.bytes;}
    static String utf8(byte[] bytes){try{return StandardCharsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT).onUnmappableCharacter(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(bytes)).toString();}catch(CharacterCodingException e){throw bad("Invalid UTF-8");}}
    static String fixedText(byte[] bytes,int offset,int length){if(offset<0||length<0||offset+length>bytes.length)throw bad("Truncated text");int end=offset;while(end<offset+length&&bytes[end]!=0)end++;return utf8(Arrays.copyOfRange(bytes,offset,end));}
    static byte[] text(String value,int max){byte[] bytes=value.getBytes(StandardCharsets.UTF_8);if(value.trim().isEmpty()||value.indexOf('\0')>=0||bytes.length>max)throw bad("Text must contain 1–"+max+" UTF-8 bytes");utf8(bytes);return bytes;}
    static IllegalArgumentException bad(String message){return new IllegalArgumentException(message);}
    static long u32(byte[] data,int pos){if(pos<0||pos+4>data.length)throw bad("Truncated integer");return (data[pos]&255L)|((data[pos+1]&255L)<<8)|((data[pos+2]&255L)<<16)|((data[pos+3]&255L)<<24);}
    static void put32(byte[] data,int pos,long value){for(int i=0;i<4;i++)data[pos+i]=(byte)(value>>>(i*8));}
    static String hex(byte[] bytes){StringBuilder b=new StringBuilder();for(byte v:bytes)b.append(String.format(Locale.ROOT,"%02x",v&255));return b.toString();}
    static byte[] unhex(String hex){if(hex.length()%2!=0)throw bad("Invalid public key");byte[] result=new byte[hex.length()/2];for(int i=0;i<result.length;i++){int a=Character.digit(hex.charAt(i*2),16),b=Character.digit(hex.charAt(i*2+1),16);if(a<0||b<0)throw bad("Invalid public key");result[i]=(byte)(a*16+b);}return result;}
    static String hash(byte[] data){try{return hex(MessageDigest.getInstance("SHA-256").digest(data)).substring(0,24);}catch(NoSuchAlgorithmException e){throw new AssertionError(e);}}
    static final class Out {
        final ByteArrayOutputStream out=new ByteArrayOutputStream();
        void v(long n){while((n&~127L)!=0){out.write((int)n&127|128);n>>>=7;}out.write((int)n);}
        Out scalar(int tag,long n){v((long)tag<<3);v(n);return this;}
        Out fixed(int tag,long n){v(((long)tag<<3)|5);for(int i=0;i<4;i++)out.write((int)(n>>>(8*i)));return this;}
        Out bytes(int tag,byte[] bytes){v(((long)tag<<3)|2);v(bytes.length);out.write(bytes,0,bytes.length);return this;}
        byte[] array(){return out.toByteArray();}
    }
}
