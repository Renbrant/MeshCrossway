package com.meshcrossway.android;

import java.util.Locale;

/** How one network's connection lines are drawn: on/off, reported lines, width, opacity and colors. */
final class LinkStyle {
    static final int MIN_WIDTH=1,MAX_WIDTH=8,DEFAULT_WIDTH=3,MIN_OPACITY=10,MAX_OPACITY=100,DEFAULT_OPACITY=95;
    /** Colors offered by the picker; the first five include the defaults. */
    static final int[] PALETTE={0xff3fbf6f,0xffe8c547,0xffe5484d,0xff8a94a3,0xffa3d94a,0xfff08c3a,0xffd14fa8,0xff8b6cf0,0xff3d8bfd,0xff2ec4d6,0xfff2f4f7,0xff1c1f24};
    static final String[] PALETTE_NAMES={"Green","Yellow","Red","Grey","Lime","Orange","Magenta","Purple","Blue","Cyan","White","Black"};

    boolean enabled=true,reported=true;
    int width=DEFAULT_WIDTH,opacity=DEFAULT_OPACITY;
    /** Indexed by MeshLinks.Quality ordinal: strong, fair, weak, no value. */
    final int[] colors=new int[MeshLinks.Quality.values().length];

    LinkStyle(){for(MeshLinks.Quality q:MeshLinks.Quality.values())colors[q.ordinal()]=q.color;}

    int color(MeshLinks.Quality q){return colors[q.ordinal()];}
    void color(MeshLinks.Quality q,int argb){colors[q.ordinal()]=0xff000000|argb;}
    /** Lines measured by your radio are drawn at the chosen width; reported (dashed) ones a little thinner. */
    float solidWidth(){return width;}
    float dashedWidth(){return Math.max(1f,width*.75f);}
    float opacity(float ageFade){return Math.max(0f,Math.min(1f,ageFade))*opacity/100f;}
    boolean isDefault(){LinkStyle d=new LinkStyle();return encode().equals(d.encode());}

    static String hex(int argb){return String.format(Locale.ROOT,"#%06x",argb&0xffffff);}
    String encode(){StringBuilder s=new StringBuilder("v1;").append(enabled?1:0).append(';').append(reported?1:0).append(';').append(width).append(';').append(opacity).append(';');for(int i=0;i<colors.length;i++){if(i>0)s.append(',');s.append(Integer.toHexString(colors[i]&0xffffff));}return s.toString();}
    /** Unknown or damaged values fall back to the defaults field by field. */
    static LinkStyle decode(String value){
        LinkStyle style=new LinkStyle();if(value==null||!value.startsWith("v1;"))return style;
        String[] parts=value.split(";",-1);
        try{
            if(parts.length>1)style.enabled=!"0".equals(parts[1]);
            if(parts.length>2)style.reported=!"0".equals(parts[2]);
            if(parts.length>3)style.width=clamp(Integer.parseInt(parts[3]),MIN_WIDTH,MAX_WIDTH);
            if(parts.length>4)style.opacity=clamp(Integer.parseInt(parts[4]),MIN_OPACITY,MAX_OPACITY);
        }catch(NumberFormatException damaged){/* keep the defaults that were not read */}
        if(parts.length>5){String[] cs=parts[5].split(",");for(int i=0;i<cs.length&&i<style.colors.length;i++){try{style.colors[i]=0xff000000|Integer.parseInt(cs[i],16);}catch(NumberFormatException damaged){/* default color */}}}
        return style;
    }
    static int clamp(int v,int lo,int hi){return Math.max(lo,Math.min(hi,v));}
}
