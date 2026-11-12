package com.meshcrossway.android;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.view.View;
import java.util.List;

/** Small, on-demand chart for reported message metadata; no background polling. */
final class RfHistoryChart extends View {
    private final List<RfHistory.Sample> samples;
    private final boolean snr;
    private final int accent;
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);

    RfHistoryChart(Context context,List<RfHistory.Sample> samples,boolean snr,int accent){
        super(context);this.samples=samples;this.snr=snr;this.accent=accent;
        setContentDescription((snr?"SNR":"RSSI")+" reported by saved incoming messages, oldest to newest. Not a live link measurement.");
    }
    private float dp(float n){return n*getResources().getDisplayMetrics().density;}
    private double value(RfHistory.Sample s){return snr?s.snr:(s.hasRssi()?s.rssi:Double.NaN);}
    @Override protected void onDraw(Canvas canvas){
        super.onDraw(canvas);
        float left=dp(55),right=getWidth()-dp(12),top=dp(15),bottom=getHeight()-dp(29);
        if(right<=left||bottom<=top||samples.isEmpty())return;
        double min=Double.POSITIVE_INFINITY,max=Double.NEGATIVE_INFINITY;
        for(RfHistory.Sample s:samples){
            double v=value(s);
            if(Double.isFinite(v)){min=Math.min(min,v);max=Math.max(max,v);}
        }
        if(!Double.isFinite(min))return;
        double spread=Math.max(4,max-min);
        double low=min-spread*0.12,high=max+spread*0.12;
        if(high<=low)return;
        paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(dp(1));
        paint.setColor(0x557d8795);
        for(int i=0;i<=2;i++){
            float y=top+(bottom-top)*i/2f;
            canvas.drawLine(left,y,right,y,paint);
            paint.setStyle(Paint.Style.FILL);paint.setTextSize(dp(10));paint.setColor(0xffa7b2be);
            canvas.drawText(String.format(java.util.Locale.ROOT,"%.0f",high-(high-low)*i/2.0),dp(3),y+dp(3),paint);
            paint.setStyle(Paint.Style.STROKE);paint.setColor(0x557d8795);
        }
        paint.setStyle(Paint.Style.FILL);paint.setColor(0xffa7b2be);paint.setTextSize(dp(10));
        canvas.drawText("Older",left,bottom+dp(18),paint);
        canvas.drawText("Newer",right-dp(32),bottom+dp(18),paint);
        boolean hasPrevious=false;float px=0,py=0;
        for(int i=0;i<samples.size();i++){
            double v=value(samples.get(i));
            if(!Double.isFinite(v)){hasPrevious=false;continue;}
            float x=left+(right-left)*(samples.size()==1?0.5f:(float)i/(samples.size()-1));
            float y=(float)(bottom-(v-low)/(high-low)*(bottom-top));
            paint.setColor(accent);paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(dp(2));
            if(hasPrevious)canvas.drawLine(px,py,x,y,paint);
            paint.setStyle(Paint.Style.FILL);canvas.drawCircle(x,y,dp(2.5f),paint);
            px=x;py=y;hasPrevious=true;
        }
    }
}
