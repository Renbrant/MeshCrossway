package com.meshcrossway.android;

/** Audit-only conformance probes. No Bluetooth, Android UI, or radio mutations. */
public final class MeshtasticSettings037Probe {
    static int checks, failures;
    static void expect(String id, boolean ok, String details) {
        checks++; if (!ok) failures++;
        System.out.println((ok ? "PASS " : "FAIL ") + id + " | " + details);
    }
    static ProtocolClient client() {
        ProtocolClient c=ProtocolClientTest.mt(new ProtocolClientTest.Events());
        c.firmware="2.8.1";c.admin.clock=100;return c;
    }
    static byte[] lora(int region,int preset,int cr,int slot) {
        return new Wire.Out().scalar(1,1).scalar(2,preset).scalar(5,cr)
            .scalar(7,region).scalar(8,3).scalar(9,1).scalar(11,slot).array();
    }
    static boolean accepts(ProtocolClient c, byte[] config) {
        try {MtLora.validate(config,c);return true;}catch(IllegalArgumentException e){return false;}
    }
    static boolean confirms(byte[] desired,byte[] expected,byte[] actual) {
        final long[] readId={0};
        MtAdmin admin=new MtAdmin(new MtAdmin.Host(){
            public void enqueue(byte[] value,long id,boolean response){if(response)readId[0]=id;}
            public void apply(int kind,int index,byte[] body){}
        });
        MtAdmin.Edit edit=new MtAdmin.Edit(6,5,34,desired,expected,true);
        edit.dispatched=true;admin.restore(edit);admin.ready();
        admin.response(100,100,100,readId[0],new Wire.Out().bytes(6,new Wire.Out().bytes(6,actual).array()).array());
        return admin.edit==null&&admin.error.isEmpty()&&admin.status.startsWith("Saved");
    }
    public static void main(String[] args) {
        for(int[] pair:new int[][]{{0,5},{9,8},{12,6},{13,6},{15,6}})
            expect("CR-"+pair[0],MtLora.presetCodingRate(pair[0])==pair[1],
                "preset="+pair[0]+", expected CR="+pair[1]+", actual="+MtLora.presetCodingRate(pair[0]));
        ProtocolClient c=client();c.firmware="2.7.0";
        expect("FW-LONG-TURBO",!MtLora.selectable(c,1,9),"LONG_TURBO must be hidden before firmware 2.7.17; selectable="+MtLora.selectable(c,1,9));
        c.firmware="2.7.17";
        expect("FW-LONG-TURBO-ARRIVED",MtLora.selectable(c,1,9),"LONG_TURBO available at 2.7.17");
        c.firmware="2.7.23";
        expect("FW-MEDIUM-TURBO",!MtLora.selectable(c,1,16),"MEDIUM_TURBO remains hidden before 2.8.0");
        c.firmware="2.8.1";
        expect("EU866-SLOT4",accepts(c,lora(29,10,0,4)),"EU_866 LITE_FAST slot 4 is valid");
        expect("EU866-SLOT5",!accepts(c,lora(29,10,0,5)),"EU_866 LITE_FAST has 4 slots including spacing/padding; slot 5 must be rejected");
        byte[] manual=new Wire.Out().scalar(1,0).scalar(3,250).scalar(4,6).scalar(5,5).scalar(7,1).scalar(9,1).array();
        c.mtHardware=12;c.hardware=MtLora.hardwareName(12);
        expect("SF6-SX1262-TBEAM-S3",accepts(c,manual),"A supported SX1262 TBEAM S3 CORE configuration must not be rejected solely because its hardware name contains TBEAM");
        byte[] desired=lora(1,0,0,0),normalized=Wire.replace(desired,5,5);
        expect("READBACK-DEFAULT-NORMALIZED",confirms(desired,new Wire.Out().scalar(5,0).array(),normalized),"default CR 0 read back as 5 confirms LONG_FAST");
        expect("READBACK-WRONG-PRESET",!confirms(lora(1,4,0,0),new Wire.Out().scalar(2,4).array(),normalized),"changed preset mismatch remains rejected");
        byte[] hopChange=Wire.replace(desired,8,4),unexpectedPreset=Wire.replace(hopChange,2,4);
        expect("READBACK-UNCHANGED-PRESET-DRIFT",!confirms(hopChange,new Wire.Out().scalar(8,4).array(),unexpectedPreset),"hop change must not imply the whole LoRa draft was confirmed when preset drifted");
        byte[] longTurbo=lora(1,9,0,0),longTurboActual=Wire.replace(longTurbo,5,8);
        expect("READBACK-LONG-TURBO-CR8",confirms(longTurbo,new Wire.Out().scalar(5,0).array(),longTurboActual),"preset-default CR 0 and LONG_TURBO's stored CR8 are semantically equivalent");
        boolean displayedBt=Wire.number(Wire.fields(Wire.EMPTY),1,1)!=0;
        boolean decodedBt=Wire.number(Wire.fields(Wire.EMPTY),1,0)!=0;
        expect("UI-BLUETOOTH-DEFAULT",displayedBt==decodedBt,"UI expression gives "+displayedBt+" for omitted enabled=false; setter sees "+decodedBt);
        boolean displayedSerial=Wire.number(Wire.fields(Wire.EMPTY),5,1)!=0;
        expect("UI-SERIAL-DEFAULT",!displayedSerial,"UI expression gives serial_enabled="+displayedSerial+" for omitted false");
        c=client();c.mtOwnerLoaded=true;c.mtOwner=new Wire.Out().bytes(2,"Old".getBytes()).bytes(3,"AB".getBytes()).array();
        String call="K1ABCDE",name="1234567890123456";boolean admitted=true;
        try{c.setMtHam(call,name);}catch(IllegalArgumentException e){admitted=false;}
        expect("HAM-COMPOSED-NAME-LIMIT",!admitted,"2.8.x composed CALLSIGN//name is "+(call+"//"+name).getBytes(java.nio.charset.StandardCharsets.UTF_8).length+" bytes (24 limit); setter accepted="+admitted);
        System.out.println("SUMMARY checks="+checks+" failures="+failures);
        if(failures>0)System.exit(1);
    }
}
