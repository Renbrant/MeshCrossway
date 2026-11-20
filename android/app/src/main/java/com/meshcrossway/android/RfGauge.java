package com.meshcrossway.android;

import android.content.Context;
import android.graphics.*;
import android.view.View;

/** Half-circle gauge for the RF coexistence dashboard: green, amber and red zones with a marker. */
final class RfGauge extends View {
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
    private final float value;private final String big,caption;private final int color,track,text,muted;
    /** value: 0..1 position of the needle (NaN hides it). */
    RfGauge(Context context,float value,String big,String caption,int color,int track,int text,int muted){
        super(context);this.value=value;this.big=big;this.caption=caption;this.color=color;this.track=track;this.text=text;this.muted=muted;
        setContentDescription(caption+": "+big);
    }
    @Override protected void onMeasure(int w,int h){int width=MeasureSpec.getSize(w);setMeasuredDimension(width,Math.round(width*0.68f));}
    @Override protected void onDraw(Canvas c){
        super.onDraw(c);float w=getWidth(),h=getHeight(),stroke=w*0.075f;float r=Math.min(w/2-stroke,h*0.82f-stroke);float cx=w/2,cy=h*0.82f;
        RectF arc=new RectF(cx-r,cy-r,cx+r,cy+r);paint.setStyle(Paint.Style.STROKE);paint.setStrokeCap(Paint.Cap.BUTT);paint.setStrokeWidth(stroke);
        int[] zones={0xff3fae7a,0xffe0a43a,0xffe2533b};float[] spans={60,60,60};float start=180;
        for(int i=0;i<3;i++){paint.setColor(zones[i]);paint.setAlpha(70);c.drawArc(arc,start,spans[i]-1.5f,false,paint);start+=spans[i];}
        paint.setAlpha(255);
        if(!Float.isNaN(value)){float v=Math.max(0,Math.min(1,value));paint.setColor(color);paint.setStrokeCap(Paint.Cap.ROUND);c.drawArc(arc,180,Math.max(1,180*v),false,paint);
            double a=Math.PI*(1+v);paint.setStyle(Paint.Style.FILL);paint.setColor(text);c.drawCircle(cx+(float)Math.cos(a)*r,cy+(float)Math.sin(a)*r,stroke*0.32f,paint);}
        paint.setStyle(Paint.Style.FILL);paint.setTextAlign(Paint.Align.CENTER);paint.setColor(text);paint.setTypeface(Typeface.DEFAULT_BOLD);paint.setTextSize(w*0.12f);
        c.drawText(big,cx,cy-r*0.12f,paint);
        paint.setTypeface(Typeface.DEFAULT);paint.setColor(muted);paint.setTextSize(w*0.065f);c.drawText(caption,cx,h-paint.descent()-2,paint);
    }
}
