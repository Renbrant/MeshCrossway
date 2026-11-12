package com.meshcrossway.android;
import org.junit.Test;
import static org.junit.Assert.*;
import java.util.*;
import com.google.zxing.*;
import com.google.zxing.common.*;
public final class ChannelQrJUnitTest {
    @Test public void officialLinksSurviveQrRoundTrip() throws Exception {
        String[] links={new McChannelLink("Équipe + 1",McChannelLink.secret(McChannelLink.PUBLIC_KEY),"").link(),new MtChannelLink(Arrays.asList(MtChannelLink.privateSettings("Equipe")),new Wire.Out().scalar(1,1).array()).link()};
        for(String link:links){BitMatrix matrix=new MultiFormatWriter().encode(link,BarcodeFormat.QR_CODE,800,800);int[] pixels=new int[800*800];for(int y=0;y<800;y++)for(int x=0;x<800;x++)pixels[y*800+x]=matrix.get(x,y)?0xff000000:0xffffffff;String decoded=new MultiFormatReader().decode(new BinaryBitmap(new HybridBinarizer(new RGBLuminanceSource(800,800,pixels)))).getText();assertEquals(link,decoded);}
    }
}
