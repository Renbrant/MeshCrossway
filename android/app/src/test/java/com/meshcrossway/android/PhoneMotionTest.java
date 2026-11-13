package com.meshcrossway.android;

import org.junit.Test;
import static org.junit.Assert.*;

public class PhoneMotionTest {
    private PhoneMotion.Fix fix(double lat,double lon,long t,double speed,double bearing){return new PhoneMotion.Fix(lat,lon,t,3,speed,.2,bearing,5);}
    @Test public void travelCourseAndStop(){PhoneMotion m=new PhoneMotion();assertTrue(m.update(fix(40,-105,1000,8,90),1000));assertEquals(90,m.bearing,0);assertTrue(m.update(fix(40,-104.999,2000,8,270),2000));assertEquals(270,m.bearing,0);assertFalse(m.update(fix(40,-104.999,3000,0,270),3000));}
    @Test public void walkingHysteresis(){PhoneMotion m=new PhoneMotion();assertFalse(m.update(fix(40,-105,1000,.7,0),1000));assertTrue(m.update(fix(40,-105,2000,1.2,0),2000));assertTrue(m.update(fix(40,-105,3000,.7,0),3000));assertFalse(m.update(fix(40,-105,4000,.3,0),4000));}
    @Test public void uncertainSpeedOrBearingDoesNotInventArrow(){PhoneMotion m=new PhoneMotion();assertFalse(m.update(new PhoneMotion.Fix(40,-105,1000,3,1,2,90,5),1000));assertFalse(m.update(new PhoneMotion.Fix(40,-105,2000,3,8,.2,90,80),2000));assertFalse(m.update(fix(40,-105,3000,8,Double.NaN),3000));}
    @Test public void expiresEvenWithoutAnotherFix(){PhoneMotion m=new PhoneMotion();PhoneMotion.Fix f=fix(40,-105,1000,8,90);assertTrue(m.update(f,1000));assertFalse(m.update(f,17000));}
    @Test public void stationaryDriftIsNotTravel(){PhoneMotion m=new PhoneMotion();assertFalse(m.update(fix(40,-105,1000,0,90),1000));assertFalse(m.update(fix(40.0001,-105,4000,0,90),4000));}
    @Test public void fallbackAccumulatesConfidentDisplacement(){PhoneMotion m=new PhoneMotion();assertFalse(m.update(fix(40,-105,1000,Double.NaN,Double.NaN),1000));assertFalse(m.update(fix(40,-104.99998,4000,Double.NaN,Double.NaN),4000));assertTrue(m.update(fix(40,-104.9998,11000,Double.NaN,Double.NaN),11000));assertEquals(90,m.bearing,.01);}
    @Test public void rejectsPoorFixAndTeleport(){PhoneMotion m=new PhoneMotion();assertFalse(m.update(new PhoneMotion.Fix(40,-105,1000,100,8,.2,90,5),1000));assertFalse(m.update(fix(40,-105,2000,Double.NaN,Double.NaN),2000));assertFalse(m.update(fix(41,-105,5000,Double.NaN,Double.NaN),5000));}
    @Test public void handlesDatelineAndOutOfOrderSamples(){PhoneMotion m=new PhoneMotion();assertFalse(m.update(fix(0,179.99995,1000,Double.NaN,Double.NaN),1000));assertTrue(m.update(fix(0,-179.99995,5000,Double.NaN,Double.NaN),5000));assertEquals(90,m.bearing,.01);assertTrue(m.update(fix(0,179.9999,4000,0,270),5000));assertEquals(90,m.bearing,.01);}
    @Test public void missingLocationClearsMotion(){PhoneMotion m=new PhoneMotion();assertTrue(m.update(fix(40,-105,1000,10,0),1000));assertFalse(m.update(null,2000));}
}
