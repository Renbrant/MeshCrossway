package com.meshcrossway.android;

public final class LinkStyleTest {
    static int checks;
    static void check(boolean condition,String message){checks++;if(!condition)throw new AssertionError(message);}
    public static void main(String[] args){
        LinkStyle d=new LinkStyle();
        check(d.enabled&&d.reported&&d.width==LinkStyle.DEFAULT_WIDTH&&d.opacity==LinkStyle.DEFAULT_OPACITY&&d.isDefault(),"defaults");
        check(d.color(MeshLinks.Quality.GOOD)==MeshLinks.Quality.GOOD.color&&d.color(MeshLinks.Quality.UNKNOWN)==MeshLinks.Quality.UNKNOWN.color,"default colors follow quality");

        LinkStyle s=new LinkStyle();s.enabled=false;s.reported=false;s.width=6;s.opacity=40;s.color(MeshLinks.Quality.WEAK,0x3d8bfd);
        LinkStyle back=LinkStyle.decode(s.encode());
        check(!back.enabled&&!back.reported&&back.width==6&&back.opacity==40&&back.color(MeshLinks.Quality.WEAK)==0xff3d8bfd&&!back.isDefault(),"round trip: "+s.encode());
        check(back.color(MeshLinks.Quality.GOOD)==MeshLinks.Quality.GOOD.color,"untouched colors kept");

        check(LinkStyle.decode(null).isDefault()&&LinkStyle.decode("garbage").isDefault(),"missing or unknown value is default");
        LinkStyle clamped=LinkStyle.decode("v1;1;1;99;0;zz,e5484d");
        check(clamped.width==LinkStyle.MAX_WIDTH&&clamped.opacity==LinkStyle.MIN_OPACITY,"width and opacity clamped");
        check(clamped.color(MeshLinks.Quality.GOOD)==MeshLinks.Quality.GOOD.color&&clamped.color(MeshLinks.Quality.FAIR)==0xffe5484d,"damaged color falls back, valid one kept");
        LinkStyle partial=LinkStyle.decode("v1;0;1;x");
        check(!partial.enabled&&partial.width==LinkStyle.DEFAULT_WIDTH,"fields read before damage are kept");

        LinkStyle w=new LinkStyle();w.width=1;check(w.dashedWidth()==1f,"dashed never thinner than 1 dp");w.width=8;check(w.dashedWidth()==6f&&w.solidWidth()==8f,"dashed is 3/4 of solid");
        w.opacity=50;check(Math.abs(w.opacity(.95f)-.475f)<1e-6&&w.opacity(2f)==.5f,"opacity multiplies the age fade");
        check(LinkStyle.hex(0xff3fbf6f).equals("#3fbf6f"),"hex color");
        check(LinkStyle.PALETTE.length==LinkStyle.PALETTE_NAMES.length,"every palette color is named");
        System.out.println("Link style: "+checks+" checks passed");
    }
}
