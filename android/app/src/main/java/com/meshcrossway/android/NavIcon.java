package com.meshcrossway.android;

import android.content.Context;
import android.graphics.*;
import android.view.View;

/** Original line icons drawn at a consistent 24dp size, independent of system fonts. */
final class NavIcon extends View {
    private final String name;private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);private final int color;
    NavIcon(Context context,String name,int color){super(context);this.name=name;this.color=color;setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);}
    @Override protected void onDraw(Canvas c){super.onDraw(c);c.save();float size=Math.min(getWidth(),getHeight());c.translate((getWidth()-size)/2,(getHeight()-size)/2);c.scale(size/24,size/24);paint.setColor(color);paint.setStrokeWidth(1.7f);paint.setStrokeCap(Paint.Cap.ROUND);paint.setStrokeJoin(Paint.Join.ROUND);paint.setStyle(Paint.Style.STROKE);
        switch(name){
            case "home":{
                // Official orange symbol, preserving the approved vector geometry and color.
                android.graphics.drawable.Drawable brand=getContext().getDrawable(R.drawable.meshcrossway_symbol);
                if(brand!=null){brand.setBounds(0,4,24,21);brand.draw(c);}
                break;
            }
            case "contacts":c.drawCircle(10,8,3.5f,paint);c.drawArc(3,13,17,24,185,170,false,paint);c.drawArc(15,5,21,12,-90,180,false,paint);c.drawArc(16,14,23,23,220,100,false,paint);break;
            case "account":c.drawCircle(12,8,4,paint);c.drawArc(4,13,20,23,180,180,false,paint);break;
            case "channels":c.drawRoundRect(3,4,21,11,2,2,paint);c.drawRoundRect(3,15,21,22,2,2,paint);line(c,7,7.5f,7,7.5f);line(c,7,18.5f,7,18.5f);break;
            case "chats":c.drawRoundRect(3,3,21,18,3,3,paint);line(c,7,18,7,22);line(c,7,22,12,18);line(c,7,8,17,8);line(c,7,12,14,12);break;
            case "map":line(c,3,5,9,3);line(c,9,3,15,6);line(c,15,6,21,4);line(c,21,4,21,20);line(c,21,20,15,22);line(c,15,22,9,19);line(c,9,19,3,21);line(c,3,21,3,5);line(c,9,3,9,19);line(c,15,6,15,22);break;
            case "settings":c.drawCircle(12,12,7,paint);c.drawCircle(12,12,2.5f,paint);for(int i=0;i<8;i++){double a=i*Math.PI/4;line(c,12+(float)Math.cos(a)*7,12+(float)Math.sin(a)*7,12+(float)Math.cos(a)*10,12+(float)Math.sin(a)*10);}break;
            case "poi":c.drawCircle(12,9,5,paint);line(c,7,11,12,22);line(c,17,11,12,22);c.drawCircle(12,9,1.5f,paint);break;
            case "layers":for(int i=0;i<3;i++){Path p=new Path();p.moveTo(3,7+i*5);p.lineTo(12,2+i*5);p.lineTo(21,7+i*5);p.lineTo(12,12+i*5);p.close();c.drawPath(p,paint);}break;
            case "download":line(c,12,3,12,15);line(c,7,10,12,15);line(c,12,15,17,10);line(c,4,16,4,21);line(c,4,21,20,21);line(c,20,21,20,16);break;
            case "zoom+":line(c,5,12,19,12);line(c,12,5,12,19);break;
            case "zoom-":line(c,5,12,19,12);break;
            case "north":Path north=new Path();north.moveTo(12,2);north.lineTo(5,21);north.lineTo(12,16);north.lineTo(19,21);north.close();c.drawPath(north,paint);break;
            case "fit":line(c,3,9,3,3);line(c,3,3,9,3);line(c,15,3,21,3);line(c,21,3,21,9);line(c,21,15,21,21);line(c,21,21,15,21);line(c,9,21,3,21);line(c,3,21,3,15);break;
            case "back":line(c,20,12,4,12);line(c,4,12,10,6);line(c,4,12,10,18);break;
            case "more":paint.setStyle(Paint.Style.FILL);for(int y=5;y<=19;y+=7)c.drawCircle(12,y,1.5f,paint);break;
            case "arrow":line(c,9,6,15,12);line(c,15,12,9,18);break;
            case "radio":c.drawRoundRect(5,8,19,21,2,2,paint);line(c,8,8,15,2);c.drawCircle(12,15,3,paint);break;
            case "gps":c.drawCircle(12,12,7,paint);c.drawCircle(12,12,2,paint);line(c,12,2,12,5);line(c,12,19,12,22);line(c,2,12,5,12);line(c,19,12,22,12);break;
            case "tracks":c.drawCircle(5,19,2,paint);c.drawCircle(19,5,2,paint);Path path=new Path();path.moveTo(5,17);path.cubicTo(4,7,20,20,19,7);c.drawPath(path,paint);break;
            case "backup":c.drawRoundRect(4,3,20,21,2,2,paint);line(c,8,3,8,10);line(c,8,10,16,10);line(c,16,10,16,3);c.drawRect(8,15,16,21,paint);break;
            case "bell":c.drawArc(6,4,18,18,180,180,false,paint);line(c,6,10,6,17);line(c,18,10,18,17);line(c,4,18,20,18);c.drawArc(9,17,15,23,0,180,false,paint);break;
            case "info":c.drawCircle(12,12,9,paint);line(c,12,10,12,17);line(c,12,6,12,6);break;
            default:c.drawCircle(12,12,7,paint);line(c,8,12,16,12);
        }c.restore();
    }
    private void line(Canvas c,float x,float y,float a,float b){c.drawLine(x,y,a,b,paint);}
}
