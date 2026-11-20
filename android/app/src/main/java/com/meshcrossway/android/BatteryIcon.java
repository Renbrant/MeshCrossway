package com.meshcrossway.android;

import android.content.Context;
import android.graphics.*;
import android.view.View;

/** Battery outline filled to the charge level, in the radio's colour (red when low). */
final class BatteryIcon extends View {
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);private final int level,color,outline;
    /** level: 0–100, RadioBattery.POWERED or RadioBattery.UNKNOWN. */
    BatteryIcon(Context context,int level,int color,int outline){super(context);this.level=level;this.color=color;this.outline=outline;setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);}
    @Override protected void onDraw(Canvas c){
        super.onDraw(c);float w=getWidth(),h=getHeight(),d=getResources().getDisplayMetrics().density,stroke=1.5f*d,nub=2.5f*d,r=2.5f*d;
        RectF body=new RectF(stroke/2,stroke/2,w-nub-stroke/2,h-stroke/2);
        paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(stroke);paint.setColor(outline);c.drawRoundRect(body,r,r,paint);
        paint.setStyle(Paint.Style.FILL);c.drawRoundRect(new RectF(w-nub,h*0.32f,w,h*0.68f),d,d,paint);
        float inset=stroke+1.2f*d;RectF inner=new RectF(body.left+inset-stroke/2,body.top+inset-stroke/2,body.right-inset+stroke/2,body.bottom-inset+stroke/2);
        if(level==RadioBattery.POWERED){paint.setColor(color);Path bolt=new Path();float cx=inner.centerX(),cy=inner.centerY(),s=inner.height()*0.55f;bolt.moveTo(cx+s*0.2f,cy-s);bolt.lineTo(cx-s*0.55f,cy+s*0.15f);bolt.lineTo(cx,cy+s*0.15f);bolt.lineTo(cx-s*0.2f,cy+s);bolt.lineTo(cx+s*0.55f,cy-s*0.15f);bolt.lineTo(cx,cy-s*0.15f);bolt.close();c.drawPath(bolt,paint);return;}
        if(level<0)return;
        float fill=Math.max(0,Math.min(100,level))/100f;if(fill<=0)return;
        paint.setColor(color);c.drawRoundRect(new RectF(inner.left,inner.top,inner.left+Math.max(1.5f*d,inner.width()*fill),inner.bottom),1.2f*d,1.2f*d,paint);
    }
}
