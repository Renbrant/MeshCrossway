package com.meshcrossway.android;

import java.net.*;
import java.nio.charset.StandardCharsets;
import java.security.*;
import java.util.*;

/** Official meshcore://channel/add link; never logged or included in diagnostics. */
final class McChannelLink {
    static final String PUBLIC_KEY="8b3387e9c5cdea6ac9e5edbaa115cd72";
    final String name,scope;final byte[] secret;
    McChannelLink(String name,byte[] secret,String scope){
        if(name==null)throw Wire.bad("Enter a channel name");Wire.text(name,31);if(name.contains("\uFFFD")||name.chars().anyMatch(Character::isISOControl))throw Wire.bad("Use a single-line channel name");if(!name.equals(name.trim()))throw Wire.bad("Remove leading or trailing spaces from the channel name");
        if(secret==null||secret.length!=16)throw Wire.bad("Channel key must contain 32 hexadecimal characters");
        boolean zero=true;for(byte b:secret)if(b!=0)zero=false;if(zero)throw Wire.bad("The all-zero key is reserved for an empty channel slot");
        if(scope==null)scope="";Wire.optionalText(scope,30);this.name=name;this.secret=secret.clone();this.scope=scope;
    }
    static byte[] secret(String hex){if(hex==null||!hex.matches("[0-9a-fA-F]{32}"))throw Wire.bad("Channel key must contain 32 hexadecimal characters");return Wire.unhex(hex);}
    static McChannelLink privateChannel(String name){byte[] key=new byte[16];new SecureRandom().nextBytes(key);return new McChannelLink(name,key,"");}
    static McChannelLink hashtag(String name){
        if(!name.startsWith("#")||name.length()<2)throw Wire.bad("Hashtag channel names must start with # and include a topic");
        try{return new McChannelLink(name,Arrays.copyOf(MessageDigest.getInstance("SHA-256").digest(Wire.text(name,31)),16),"");}catch(NoSuchAlgorithmException e){throw new IllegalStateException(e);}
    }
    String link(){return "meshcore://channel/add?name="+encode(name)+"&secret="+Wire.hex(secret)+(scope.isEmpty()?"":"&region_scope="+encode(scope));}
    static String encode(String text){try{return URLEncoder.encode(text,"UTF-8");}catch(java.io.UnsupportedEncodingException e){throw new IllegalStateException(e);}}
    static McChannelLink parse(String text){
        if(text==null||text.length()>2048)throw Wire.bad("Invalid MeshCore channel link");
        try{
            URI uri=new URI(text.trim());if(!"meshcore".equalsIgnoreCase(uri.getScheme())||!"channel".equals(uri.getHost())||!"/add".equals(uri.getPath())||uri.getFragment()!=null||uri.getUserInfo()!=null||uri.getPort()!=-1)throw Wire.bad("Paste a meshcore://channel/add link");
            Map<String,String> fields=new HashMap<>();String query=uri.getRawQuery();if(query==null)throw Wire.bad("Channel link has no name or key");
            for(String part:query.split("&",-1)){int equals=part.indexOf('=');if(equals<1)throw Wire.bad("Invalid channel link parameter");String key=URLDecoder.decode(part.substring(0,equals),"UTF-8"),value=URLDecoder.decode(part.substring(equals+1),"UTF-8");if(!Arrays.asList("name","secret","region_scope").contains(key)||fields.put(key,value)!=null)throw Wire.bad("Unknown or duplicate channel link parameter");}
            return new McChannelLink(fields.getOrDefault("name",""),secret(fields.get("secret")),fields.getOrDefault("region_scope",""));
        }catch(URISyntaxException|java.io.UnsupportedEncodingException e){throw Wire.bad("Invalid MeshCore channel link");}
    }
}
