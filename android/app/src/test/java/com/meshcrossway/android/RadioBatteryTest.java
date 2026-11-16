package com.meshcrossway.android;

/** Home card battery: MeshCore millivolts to percent, Meshtastic battery_level, and periodic MeshCore reads. */
public final class RadioBatteryTest {
    static void check(boolean yes,String label){if(!yes)throw new AssertionError(label);}
    public static void main(String[] args){
        check(RadioBattery.percentFromMillivolts(4250)==100&&RadioBattery.percentFromMillivolts(4200)==100,"full cell");
        check(RadioBattery.percentFromMillivolts(3800)==50&&RadioBattery.percentFromMillivolts(3300)==0&&RadioBattery.percentFromMillivolts(3000)==0,"curve anchors");
        check(RadioBattery.percentFromMillivolts(3960)==75,"interpolates between anchors");
        check(RadioBattery.percentFromMillivolts(0)==RadioBattery.UNKNOWN,"no reading");
        check(RadioBattery.label(87).equals("87%")&&RadioBattery.label(RadioBattery.POWERED).equals("USB")&&RadioBattery.label(RadioBattery.UNKNOWN).isEmpty(),"labels");

        ProtocolClient mt=ProtocolClientTest.mt(new ProtocolClientTest.Events());
        check(RadioBattery.level(mt)==RadioBattery.UNKNOWN,"no Meshtastic reading yet");
        byte[] metrics=new Wire.Out().scalar(1,64).array();
        byte[] telemetry=new Wire.Out().bytes(2,metrics).array();
        byte[] data=new Wire.Out().scalar(1,67).bytes(2,telemetry).array();
        mt.receive(ProtocolClientTest.wrapped(2,new Wire.Out().fixed(1,100).fixed(2,0xffffffffL).fixed(6,77).bytes(4,data).array()),50);
        check(RadioBattery.level(mt)==64,"own-node device telemetry");
        byte[] powered=new Wire.Out().scalar(1,67).bytes(2,new Wire.Out().bytes(2,new Wire.Out().scalar(1,101).array()).array()).array();
        mt.receive(ProtocolClientTest.wrapped(2,new Wire.Out().fixed(1,100).fixed(2,0xffffffffL).fixed(6,78).bytes(4,powered).array()),51);
        check(RadioBattery.level(mt)==RadioBattery.POWERED,"101 means external power");

        McIntegrationTest.clock=100;
        ProtocolClient mc=McIntegrationTest.fresh();
        McIntegrationTest.clock+=20001;ProtocolClient.Command q=McIntegrationTest.take(mc);check(q.data[0]==10,"first timed poll still drains backlog");
        mc.receive(new byte[]{10},McIntegrationTest.clock++);
        McIntegrationTest.clock+=20001;q=McIntegrationTest.take(mc);check(q.data[0]==20,"next idle poll reads battery");
        byte[] battery=new byte[11];battery[0]=12;Wire.put32(battery,1,3960);mc.receive(battery,McIntegrationTest.clock++);
        check(RadioBattery.level(mc)==75,"MeshCore battery shown as percent");
        McIntegrationTest.clock+=20001;q=McIntegrationTest.take(mc);check(q.data[0]==10,"battery is not re-read on every poll");
        System.out.println("Radio battery: MeshCore voltage curve, Meshtastic levels and periodic reads passed");
    }
}
