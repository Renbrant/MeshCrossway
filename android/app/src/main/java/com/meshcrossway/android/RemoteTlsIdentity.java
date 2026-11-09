package com.meshcrossway.android;

import android.content.Context;
import android.content.SharedPreferences;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import java.io.*;
import java.math.BigInteger;
import java.net.InetAddress;
import java.nio.charset.StandardCharsets;
import java.security.*;
import java.security.cert.Certificate;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.security.spec.PKCS8EncodedKeySpec;
import java.text.SimpleDateFormat;
import java.util.*;
import javax.crypto.*;
import javax.crypto.spec.GCMParameterSpec;
import javax.net.ssl.*;
import javax.security.auth.x500.X500Principal;
import org.json.JSONObject;

/**
 * Local HTTPS identity. RSA TLS keys are ordinary JCA keys in memory and are
 * persisted ONLY inside AES-256-GCM encryption; the AES key itself is a
 * non-exportable AndroidKeyStore secret. No PEM/private key is stored on disk.
 *
 * This avoids Android Conscrypt server handshake incompatibilities with an
 * AndroidKeyStore RSA PrivateKey, while preserving hardware-backed encryption
 * at rest on phones that support it.
 *
 * ASN.1 below only encodes a fixed X.509v3 certificate; it does not parse
 * untrusted ASN.1. Android CertificateFactory validates the constructed DER.
 */
final class RemoteTlsIdentity {
    private static final String WRAP_ALIAS="meshcrossway-remote-tls-wrap-v1";
    private static final String PREFS="meshcrossway-remote-tls";
    private static final String PRIVATE_DATA="sealed-key";
    private static final String PRIVATE_IV="sealed-iv";
    private static final SecureRandom RANDOM=new SecureRandom();
    private static final byte[] SHA256_RSA_ALGORITHM=new byte[]{
        0x06,0x09,0x2a,(byte)0x86,0x48,(byte)0x86,(byte)0xf7,0x0d,0x01,0x01,0x0b
    };
    final SSLContext context;
    final X509Certificate certificate;

    private RemoteTlsIdentity(SSLContext context,X509Certificate certificate){
        this.context=context;this.certificate=certificate;
    }
    static RemoteTlsIdentity load(Context context,String ip)throws Exception{
        KeyStore keystore=KeyStore.getInstance("AndroidKeyStore");
        keystore.load(null);
        SecretKey wrappingKey;
        if(!keystore.containsAlias(WRAP_ALIAS)){
            KeyGenerator generator=KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES,"AndroidKeyStore");
            generator.init(new KeyGenParameterSpec.Builder(WRAP_ALIAS,
                KeyProperties.PURPOSE_ENCRYPT|KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setKeySize(256)
                .setRandomizedEncryptionRequired(true).build());
            wrappingKey=generator.generateKey();
        }else wrappingKey=(SecretKey)keystore.getKey(WRAP_ALIAS,null);
        if(wrappingKey==null)throw new GeneralSecurityException("Private TLS storage unavailable");

        SharedPreferences prefs=context.getSharedPreferences(PREFS,Context.MODE_PRIVATE);
        PrivateKey privateKey;
        X509Certificate cert;
        if(prefs.contains(PRIVATE_DATA)){
            byte[] iv=Base64.getDecoder().decode(prefs.getString(PRIVATE_IV,""));
            byte[] sealed=Base64.getDecoder().decode(prefs.getString(PRIVATE_DATA,""));
            if(iv.length!=12||sealed.length>16384)throw new GeneralSecurityException("TLS identity storage corrupt");
            Cipher cipher=Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.DECRYPT_MODE,wrappingKey,new GCMParameterSpec(128,iv));
            byte[] clear=cipher.doFinal(sealed);
            JSONObject decoded=new JSONObject(new String(clear,StandardCharsets.UTF_8));
            byte[] key=Base64.getDecoder().decode(decoded.getString("private"));
            privateKey=KeyFactory.getInstance("RSA").generatePrivate(new PKCS8EncodedKeySpec(key));
            cert=readCertificate(Base64.getDecoder().decode(decoded.getString("certificate")));
        }else{
            KeyPairGenerator generator=KeyPairGenerator.getInstance("RSA");
            generator.initialize(2048,RANDOM);
            KeyPair pair=generator.generateKeyPair();
            privateKey=pair.getPrivate();
            cert=selfSigned(pair.getPrivate(),pair.getPublic(),ip);
            seal(prefs,wrappingKey,privateKey,cert);
        }

        // Wi-Fi IP may change between activations; issue a new certificate
        // for the new local address, using the existing encrypted private key.
        if(!hasSan(cert,ip)){
            cert=selfSigned(privateKey,cert.getPublicKey(),ip);
            seal(prefs,wrappingKey,privateKey,cert);
        }
        cert.checkValidity();
        cert.verify(cert.getPublicKey());
        Signature signer=Signature.getInstance("SHA256withRSA");
        signer.initSign(privateKey);
        signer.update(new byte[]{77,67,88});
        byte[] proof=signer.sign();
        Signature verifier=Signature.getInstance("SHA256withRSA");
        verifier.initVerify(cert.getPublicKey());
        verifier.update(new byte[]{77,67,88});
        if(!verifier.verify(proof))throw new GeneralSecurityException("TLS identity key mismatch");

        // PKCS12 is only constructed in memory. No unencrypted key store file.
        char[] password=new char[24];
        for(int i=0;i<password.length;i++)password[i]=(char)('a'+RANDOM.nextInt(26));
        KeyStore identity=KeyStore.getInstance("PKCS12");
        identity.load(null,password);
        identity.setKeyEntry("meshcrossway-local",privateKey,password,new Certificate[]{cert});
        KeyManagerFactory manager=KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm());
        manager.init(identity,password);
        Arrays.fill(password,'\0');
        SSLContext tls=SSLContext.getInstance("TLS");
        tls.init(manager.getKeyManagers(),null,RANDOM);
        return new RemoteTlsIdentity(tls,cert);
    }
    private static void seal(SharedPreferences prefs,SecretKey key,PrivateKey privateKey,
                             X509Certificate cert)throws Exception{
        JSONObject material=new JSONObject();
        material.put("private",Base64.getEncoder().encodeToString(privateKey.getEncoded()));
        material.put("certificate",Base64.getEncoder().encodeToString(cert.getEncoded()));
        Cipher cipher=Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.ENCRYPT_MODE,key);
        byte[] sealed=cipher.doFinal(material.toString().getBytes(StandardCharsets.UTF_8));
        if(!prefs.edit()
            .putString(PRIVATE_IV,Base64.getEncoder().encodeToString(cipher.getIV()))
            .putString(PRIVATE_DATA,Base64.getEncoder().encodeToString(sealed)).commit())
            throw new IOException("Unable to store encrypted TLS identity");
    }
    private static X509Certificate readCertificate(byte[] der)throws Exception{
        CertificateFactory parser=CertificateFactory.getInstance("X.509");
        X509Certificate cert=(X509Certificate)parser.generateCertificate(new ByteArrayInputStream(der));
        cert.checkValidity();
        return cert;
    }
    private static boolean hasSan(X509Certificate cert,String ip){
        try{
            Collection<List<?>> sans=cert.getSubjectAlternativeNames();
            if(sans==null)return false;
            for(List<?> entry:sans)if(entry.size()>=2&&Integer.valueOf(7).equals(entry.get(0))
                    &&ip.equals(entry.get(1)))return true;
        }catch(Exception ignored){}
        return false;
    }
    private static X509Certificate selfSigned(PrivateKey privateKey,PublicKey publicKey,
                                               String ip)throws Exception{
        byte[] serialBytes=new byte[16];RANDOM.nextBytes(serialBytes);
        serialBytes[0]&=0x7f;serialBytes[0]|=1;
        byte[] algorithm=seq(SHA256_RSA_ALGORITHM,tag(0x05,new byte[0]));
        byte[] subject=new X500Principal("CN=MeshCrossway Local Access").getEncoded();
        long now=System.currentTimeMillis();
        byte[] validity=seq(utc(now-86_400_000L),utc(now+5L*365*86_400_000L));
        byte[] san=seq(tag(0x87,InetAddress.getByName(ip).getAddress()));
        byte[] extension=seq(new byte[]{0x06,0x03,0x55,0x1d,0x11},tag(0x04,san));
        byte[] tbs=seq(
            tag(0xa0,tag(0x02,new byte[]{0x02})), // X.509 version 3
            tag(0x02,new BigInteger(1,serialBytes).toByteArray()),
            algorithm,subject,validity,subject,publicKey.getEncoded(),
            tag(0xa3,seq(extension)));
        Signature signer=Signature.getInstance("SHA256withRSA");
        signer.initSign(privateKey);
        signer.update(tbs);
        byte[] signature=signer.sign();
        byte[] bitstring=new byte[signature.length+1];
        System.arraycopy(signature,0,bitstring,1,signature.length);
        return readCertificate(seq(tbs,algorithm,tag(0x03,bitstring)));
    }
    private static byte[] utc(long timestamp){
        SimpleDateFormat date=new SimpleDateFormat("yyMMddHHmmss'Z'",Locale.US);
        date.setTimeZone(TimeZone.getTimeZone("UTC"));
        return tag(0x17,date.format(new Date(timestamp)).getBytes(StandardCharsets.US_ASCII));
    }
    private static byte[] seq(byte[]... items){return tag(0x30,concat(items));}
    private static byte[] concat(byte[]... bytes){
        int length=0;for(byte[] part:bytes)length+=part.length;
        byte[] out=new byte[length];int offset=0;
        for(byte[] part:bytes){System.arraycopy(part,0,out,offset,part.length);offset+=part.length;}
        return out;
    }
    private static byte[] tag(int tag,byte[] value){
        int length=value.length,bytes=1,n=length;
        if(length>=128){bytes=0;while(n>0){n>>>=8;bytes++;}}
        byte[] result=new byte[1+(length<128?1:bytes+1)+length];
        int index=0;result[index++]=(byte)tag;
        if(length<128)result[index++]=(byte)length;
        else{
            result[index++]=(byte)(0x80|bytes);
            for(int j=bytes-1;j>=0;j--)result[index++]=(byte)(length>>>(j*8));
        }
        System.arraycopy(value,0,result,index,length);
        return result;
    }
}
