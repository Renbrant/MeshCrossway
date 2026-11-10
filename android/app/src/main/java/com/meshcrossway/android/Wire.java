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
    static int u16(byte[] data,int pos){if(pos<0||pos+2>data.length)throw bad("Truncated integer");return (data[pos]&255)|((data[pos+1]&255)<<8);}
    static void put32(byte[] data,int pos,long value){for(int i=0;i<4;i++)data[pos+i]=(byte)(value>>>(i*8));}
    static String hex(byte[] bytes){char[] digits="0123456789abcdef".toCharArray(),result=new char[bytes.length*2];for(int i=0;i<bytes.length;i++){result[i*2]=digits[(bytes[i]&255)>>>4];result[i*2+1]=digits[bytes[i]&15];}return new String(result);}
    static byte[] unhex(String hex){if(hex.length()%2!=0)throw bad("Invalid public key");byte[] result=new byte[hex.length()/2];for(int i=0;i<result.length;i++){int a=Character.digit(hex.charAt(i*2),16),b=Character.digit(hex.charAt(i*2+1),16);if(a<0||b<0)throw bad("Invalid public key");result[i]=(byte)(a*16+b);}return result;}
    static String hash(byte[] data){try{return hex(MessageDigest.getInstance("SHA-256").digest(data)).substring(0,24);}catch(NoSuchAlgorithmException e){throw new AssertionError(e);}}
    static final class Out {
        final ByteArrayOutputStream out=new ByteArrayOutputStream();
        void v(long n){while((n&~127L)!=0){out.write((int)n&127|128);n>>>=7;}out.write((int)n);}
        Out scalar(int tag,long n){v((long)tag<<3);v(n);return this;}
        Out fixed(int tag,long n){v(((long)tag<<3)|5);for(int i=0;i<4;i++)out.write((int)(n>>>(8*i)));return this;}
        Out bytes(int tag,byte[] bytes){v(((long)tag<<3)|2);v(bytes.length);out.write(bytes,0,bytes.length);return this;}
        Out add(F f){if(f.wire==0)return scalar(f.tag,f.number);if(f.wire==2)return bytes(f.tag,f.bytes);if(f.wire==5)return fixed(f.tag,f.number);if(f.wire==1){v(((long)f.tag<<3)|1);out.write(f.bytes,0,f.bytes.length);return this;}throw bad("Unsupported protobuf field");}
        byte[] array(){return out.toByteArray();}
    }
    static byte[] replace(byte[] message,int tag,long value){Out out=new Out();boolean found=false;for(F f:fields(message)){if(f.tag==tag){if(!found)out.scalar(tag,value);found=true;}else out.add(f);}if(!found)out.scalar(tag,value);return out.array();}
    static byte[] replaceBytes(byte[] message,int tag,byte[] value){Out out=new Out();boolean found=false;for(F f:fields(message)){if(f.tag==tag){if(!found)out.bytes(tag,value);found=true;}else out.add(f);}if(!found)out.bytes(tag,value);return out.array();}
    static boolean containsFields(byte[] actual,byte[] expected){List<F> a=fields(actual);for(F e:fields(expected)){F f=field(a,e.tag);if(f!=null&&f.wire!=e.wire)return false;if(e.wire==2){if(!Arrays.equals(e.bytes,f==null?EMPTY:f.bytes))return false;}else if(e.number!=(f==null?0:f.number))return false;}return true;}
    static byte[] changes(byte[] before,byte[] after){List<F> old=fields(before);Out out=new Out();for(F f:fields(after)){F a=field(old,f.tag);if(a==null||a.wire!=f.wire||a.number!=f.number||!Arrays.equals(a.bytes,f.bytes))out.add(f);}return out.array();}
    static byte[] floatField(byte[] message,int tag,float value){if(!Float.isFinite(value))throw bad("Enter a finite value");Out out=new Out();for(F f:fields(message))if(f.tag!=tag)out.add(f);return append(out.array(),new Out().fixed(tag,Float.floatToIntBits(value)).array());}
    static byte[] append(byte[] a,byte[] b){byte[] result=Arrays.copyOf(a,a.length+b.length);System.arraycopy(b,0,result,a.length,b.length);return result;}
    static byte[] optionalText(String value,int max){if(value.indexOf('\0')>=0||value.getBytes(StandardCharsets.UTF_8).length>max)throw bad("Use at most "+max+" UTF-8 bytes");return value.getBytes(StandardCharsets.UTF_8);}
}
