package com.meshcrossway.android;
import org.junit.Test;
public final class MtSettingsJUnitTest {
    @Test public void officialAppConformance(){MtConformanceTest.main(new String[0]);}
    @Test public void administrativeProtocolRegressions(){MtSettingsTest.time=100;MtSettingsTest.main(new String[0]);}
}
