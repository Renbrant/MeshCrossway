package com.meshcrossway.android;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.RadialGradient;
import android.graphics.Shader;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import java.io.*;

/** Phone-only UI appearance. No BLE, radio identity, or protocol state changes. */
final class Appearance {
    static final String SYSTEM="system",COLOR="color",GRADIENT="gradient",PHOTO="photo";
    static final String LINEAR="linear",MESH="mesh";
    static final int DEFAULT_HUE=28;
    static final String PHOTO_NAME="appearance-background.jpg";

    static final class Palette {
        final int bg,panel,panel2,accent,text,muted,navActive;
        final boolean light;
        Palette(int bg,int panel,int panel2,int accent,int text,int muted,int navActive,boolean light){
            this.bg=bg;this.panel=panel;this.panel2=panel2;this.accent=accent;
            this.text=text;this.muted=muted;this.navActive=navActive;this.light=light;
        }
    }
    final String mode,gradientType;
    final int hue,dim,blur;
    final boolean hasPhoto;
    final Palette colors;

    private Appearance(Context context,String mode,String gradientType,int hue,int dim,int blur){
        this.mode=validMode(mode)?mode:SYSTEM;
        this.gradientType=MESH.equals(gradientType)?MESH:LINEAR;
        this.hue=Math.max(0,Math.min(359,hue));
        this.dim=Math.max(30,Math.min(90,dim));
        this.blur=Math.max(0,Math.min(24,blur));
        this.hasPhoto=photoFile(context).isFile() && photoFile(context).length()>0;
        this.colors=palette(context,this.mode,this.hue);
    }
    static boolean validMode(String value){
        return SYSTEM.equals(value)||COLOR.equals(value)||GRADIENT.equals(value)||PHOTO.equals(value);
    }
    static boolean validHue(int hue){return hue>=0&&hue<360;}
    static Appearance load(Context context){
        SharedPreferences pref=context.getSharedPreferences("ui",Context.MODE_PRIVATE);
        return new Appearance(context,pref.getString("appearance.mode",SYSTEM),
            pref.getString("appearance.gradient",LINEAR),
            pref.getInt("appearance.hue",DEFAULT_HUE),
            pref.getInt("appearance.dim",65),
            pref.getInt("appearance.blur",8));
    }
    static void setMode(Context context,String mode){
        if(!validMode(mode))throw new IllegalArgumentException("Unknown appearance mode");
        context.getSharedPreferences("ui",Context.MODE_PRIVATE).edit()
            .putString("appearance.mode",mode).apply();
    }
    static void setHue(Context context,int hue){
        if(!validHue(hue))throw new IllegalArgumentException("Hue outside 0-359");
        context.getSharedPreferences("ui",Context.MODE_PRIVATE).edit()
            .putInt("appearance.hue",hue).apply();
    }
    static void setGradient(Context context,String type){
        if(!LINEAR.equals(type)&&!MESH.equals(type))
            throw new IllegalArgumentException("Unknown gradient type");
        context.getSharedPreferences("ui",Context.MODE_PRIVATE).edit()
            .putString("appearance.gradient",type).apply();
    }
    static void setDim(Context context,int value){
        context.getSharedPreferences("ui",Context.MODE_PRIVATE).edit()
            .putInt("appearance.dim",Math.max(30,Math.min(90,value))).apply();
    }
    static void setBlur(Context context,int value){
        context.getSharedPreferences("ui",Context.MODE_PRIVATE).edit()
            .putInt("appearance.blur",Math.max(0,Math.min(24,value))).apply();
    }
    static int hueColor(int hue,float sat,float value){
        return Color.HSVToColor(new float[]{((hue%360)+360)%360,
            Math.max(0,Math.min(1,sat)),Math.max(0,Math.min(1,value))});
    }
    static Palette palette(Context context,String mode,int hue){
        if(SYSTEM.equals(mode)){
            boolean night=(context.getResources().getConfiguration().uiMode&
                Configuration.UI_MODE_NIGHT_MASK)==Configuration.UI_MODE_NIGHT_YES;
            if(!night) return new Palette(0xfff2f5f8,0xffffffff,0xffe6ecf3,
                0xffb54b00,0xff142332,0xff526373,0xffffdbbd,true);
            return new Palette(0xff090c10,0xff181d24,0xff242b34,
                0xffff8a24,0xfff2f4f7,0xff9aa6b4,0xff53321c,false);
        }
        if(COLOR.equals(mode)){
            int accent=hueColor(hue,.85f,.96f);
            return new Palette(hueColor(hue,.58f,.115f),hueColor(hue,.43f,.19f),
                hueColor(hue,.35f,.26f),accent,
                0xfff4f6fa,0xffb4c0cb,hueColor(hue,.57f,.29f),false);
        }
        if(GRADIENT.equals(mode)){
            return new Palette(hueColor(hue,.65f,.11f),0xd91a202c,0xe62b3342,
                hueColor(hue,.83f,.98f),0xfff4f6fa,0xffb7c1ce,
                hueColor(hue,.53f,.29f),false);
        }
        return new Palette(0xff0a1019,0xe31a2029,0xe934404c,
            0xffff8a24,0xfff6f8fb,0xffb8c7d4,0xff49321d,false);
    }
    Drawable background(){
        if(GRADIENT.equals(mode))return gradientDrawable(hue,gradientType);
        return new android.graphics.drawable.ColorDrawable(colors.bg);
    }
    static Drawable previewBackground(String mode,int hue,String gradient){
        if(GRADIENT.equals(mode))return gradientDrawable(hue,gradient);
        return new android.graphics.drawable.ColorDrawable(
            COLOR.equals(mode)?hueColor(hue,.59f,.18f):
            PHOTO.equals(mode)?0xff19252d:0xff181d24);
    }
    static Drawable gradientDrawable(int hue,String type){
        if(MESH.equals(type))return new MeshGradient(hue);
        return new GradientDrawable(GradientDrawable.Orientation.TL_BR,
            new int[]{hueColor(hue,.66f,.12f),hueColor(hue+40,.57f,.22f),
                hueColor(hue+108,.59f,.12f)});
    }
    /** Simple canvas-only radial blend, no third-party image library/network. */
    private static final class MeshGradient extends Drawable {
        private final int hue;
        private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
        MeshGradient(int hue){this.hue=hue;}
        @Override public void draw(Canvas canvas){
            android.graphics.Rect bounds=getBounds();
            int w=bounds.width(),h=bounds.height();
            if(w==0||h==0)return;
            canvas.save();canvas.translate(bounds.left,bounds.top);
            paint.setShader(new LinearGradient(0,0,w,h,
                hueColor(hue,.67f,.105f),hueColor(hue+140,.66f,.105f),Shader.TileMode.CLAMP));
            canvas.drawRect(0,0,w,h,paint);
            radial(canvas,w,h,w*.1f,h*.18f,Math.max(w,h)*.85f,
                hueColor(hue+35,.63f,.34f));
            radial(canvas,w,h,w*.97f,h*.79f,Math.max(w,h)*.8f,
                hueColor(hue+190,.69f,.35f));
            paint.setShader(null);
            canvas.restore();
        }
        private void radial(Canvas canvas,int w,int h,float x,float y,float r,int color){
            paint.setShader(new RadialGradient(x,y,r,
                new int[]{Color.argb(180,Color.red(color),Color.green(color),Color.blue(color)),
                    Color.TRANSPARENT},null,Shader.TileMode.CLAMP));
            canvas.drawRect(0,0,w,h,paint);
        }
        @Override public void setAlpha(int alpha){paint.setAlpha(alpha);}
        @Override public void setColorFilter(android.graphics.ColorFilter filter){paint.setColorFilter(filter);}
        @Override public int getOpacity(){return PixelFormat.OPAQUE;}
    }
    static File photoFile(Context context){return new File(context.getFilesDir(),PHOTO_NAME);}

    /**
     * Copy selected image into app-private storage so it survives future in-place
     * APK upgrades and does not depend on URI permissions or cloud availability.
     * Re-encoding strips embedded EXIF location metadata.
     */
    static void storePhoto(Context context,Uri uri)throws IOException {
        File target=photoFile(context);
        File temporary=new File(context.getFilesDir(),PHOTO_NAME+".tmp");
        Bitmap source=null,scaled=null;
        try{
            BitmapFactory.Options size=new BitmapFactory.Options();
            size.inJustDecodeBounds=true;
            try(InputStream stream=context.getContentResolver().openInputStream(uri)){
                if(stream==null)throw new IOException("Photo is unavailable");
                BitmapFactory.decodeStream(stream,null,size);
            }
            if(size.outWidth<1||size.outHeight<1||
                size.outWidth>32000||size.outHeight>32000)
                throw new IOException("Invalid image dimensions");
            long pixels=(long)size.outWidth*size.outHeight;
            if(pixels>150_000_000L)throw new IOException("Image resolution is too large");
            BitmapFactory.Options opts=new BitmapFactory.Options();
            opts.inSampleSize=1;
            while(Math.max(size.outWidth/opts.inSampleSize,size.outHeight/opts.inSampleSize)>1800)
                opts.inSampleSize*=2;
            opts.inPreferredConfig=Bitmap.Config.ARGB_8888;
            try(InputStream stream=context.getContentResolver().openInputStream(uri)){
                if(stream==null)throw new IOException("Photo could not be opened");
                source=BitmapFactory.decodeStream(stream,null,opts);
            }
            if(source==null)throw new IOException("Cannot decode photo");
            float scale=Math.min(1f,1600f/Math.max(source.getWidth(),source.getHeight()));
            scaled=scale<1?Bitmap.createScaledBitmap(source,
                Math.max(1,Math.round(source.getWidth()*scale)),
                Math.max(1,Math.round(source.getHeight()*scale)),true):source;
            try(FileOutputStream output=new FileOutputStream(temporary)){
                if(!scaled.compress(Bitmap.CompressFormat.JPEG,84,output))
                    throw new IOException("Cannot save photo");
                output.getFD().sync();
            }
            if(!temporary.renameTo(target))throw new IOException("Cannot store photo on phone");
        }finally{
            if(scaled!=null&&scaled!=source)scaled.recycle();
            if(source!=null)source.recycle();
            if(temporary.exists())temporary.delete();
        }
    }
    Bitmap loadPhoto(Context context){
        if(!hasPhoto)return null;
        BitmapFactory.Options options=new BitmapFactory.Options();
        options.inPreferredConfig=Bitmap.Config.RGB_565;
        try{return BitmapFactory.decodeFile(photoFile(context).getAbsolutePath(),options);}
        catch(RuntimeException e){return null;}
    }
    /** Lightweight legacy-device blur fallback (API 26-30), high-quality GPU blur is used on 31+. */
    static Bitmap approximateBlur(Bitmap source,int radius){
        if(radius<=0)return source;
        int div=2+radius*2;
        int w=Math.max(24,source.getWidth()/div),h=Math.max(24,source.getHeight()/div);
        return Bitmap.createScaledBitmap(source,w,h,true);
    }
}
