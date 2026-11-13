package com.meshcrossway.android;
import java.util.*;
/** Enum facts from meshtastic/protobufs 4c80682. Never infer wire IDs from displayed order. */
final class MtLora {
    static final class Option {final int value;final String label;Option(int v,String l){value=v;label=l;}}
    static final Option[] REGIONS={new Option(0,"UNSET"),new Option(1,"US"),new Option(2,"EU 433"),new Option(3,"EU 868"),new Option(4,"CN"),new Option(5,"JP"),new Option(6,"ANZ"),new Option(7,"KR"),new Option(8,"TW"),new Option(9,"RU"),new Option(10,"IN"),new Option(11,"NZ 865"),new Option(12,"TH"),new Option(13,"LORA 24"),new Option(14,"UA 433"),new Option(15,"UA 868"),new Option(16,"MY 433"),new Option(17,"MY 919"),new Option(18,"SG 923"),new Option(19,"PH 433"),new Option(20,"PH 868"),new Option(21,"PH 915"),new Option(22,"ANZ 433"),new Option(23,"KZ 433"),new Option(24,"KZ 863"),new Option(25,"NP 865"),new Option(26,"BR 902"),new Option(27,"ITU1 2M"),new Option(28,"ITU2 2M"),new Option(29,"EU 866"),new Option(30,"EU 874"),new Option(31,"EU 917"),new Option(32,"EU N 868"),new Option(33,"ITU3 2M"),new Option(34,"ITU1 70CM"),new Option(35,"ITU2 70CM"),new Option(36,"ITU3 70CM"),new Option(37,"ITU2 125CM")};
    static final Option[] PRESETS={new Option(0,"LONG FAST"),new Option(1,"LONG SLOW"),new Option(2,"VERY LONG SLOW"),new Option(3,"MEDIUM SLOW"),new Option(4,"MEDIUM FAST"),new Option(5,"SHORT SLOW"),new Option(6,"SHORT FAST"),new Option(7,"LONG MODERATE"),new Option(8,"SHORT TURBO"),new Option(9,"LONG TURBO"),new Option(10,"LITE FAST"),new Option(11,"LITE SLOW"),new Option(12,"NARROW FAST"),new Option(13,"NARROW SLOW"),new Option(14,"TINY FAST"),new Option(15,"TINY SLOW"),new Option(16,"MEDIUM TURBO")};
    static String hardwareName(int value){switch(value){case 0:return "UNSET";case 1:return "TLORA V2";case 2:return "TLORA V1";case 3:return "TLORA V2 1 1P6";case 4:return "TBEAM";case 5:return "HELTEC V2 0";case 6:return "TBEAM V0P7";case 7:return "T ECHO";case 8:return "TLORA V1 1P3";case 9:return "RAK4631";case 10:return "HELTEC V2 1";case 11:return "HELTEC V1";case 12:return "LILYGO TBEAM S3 CORE";case 13:return "RAK11200";case 14:return "NANO G1";case 15:return "TLORA V2 1 1P8";case 16:return "TLORA T3 S3";case 17:return "NANO G1 EXPLORER";case 18:return "NANO G2 ULTRA";case 19:return "LORA TYPE";case 20:return "WIPHONE";case 21:return "WIO WM1110";case 22:return "RAK2560";case 23:return "HELTEC HRU 3601";case 24:return "HELTEC WIRELESS BRIDGE";case 25:return "STATION G1";case 26:return "RAK11310";case 27:return "MAKERFABS TRACKER";case 28:return "MAKERFABS RESERVED";case 29:return "CANARYONE";case 30:return "RP2040 LORA";case 31:return "STATION G2";case 32:return "LORA RELAY V1";case 33:return "T ECHO PLUS";case 34:return "PPR";case 35:return "GENIEBLOCKS";case 36:return "NRF52 UNKNOWN";case 37:return "PORTDUINO";case 38:return "ANDROID SIM";case 39:return "DIY V1";case 40:return "NRF52840 PCA10059";case 41:return "DR DEV";case 42:return "M5STACK";case 43:return "HELTEC V3";case 44:return "HELTEC WSL V3";case 45:return "BETAFPV 2400 TX";case 46:return "BETAFPV 900 NANO TX";case 47:return "RPI PICO";case 48:return "HELTEC WIRELESS TRACKER";case 49:return "HELTEC WIRELESS PAPER";case 50:return "T DECK";case 51:return "T WATCH S3";case 52:return "PICOMPUTER S3";case 53:return "HELTEC HT62";case 54:return "EBYTE ESP32 S3";case 55:return "ESP32 S3 PICO";case 56:return "CHATTER 2";case 57:return "HELTEC WIRELESS PAPER V1 0";case 58:return "HELTEC WIRELESS TRACKER V1 0";case 59:return "UNPHONE";case 60:return "TD LORAC";case 61:return "CDEBYTE EORA S3";case 62:return "TWC MESH V4";case 63:return "NRF52 PROMICRO DIY";case 64:return "RADIOMASTER 900 BANDIT NANO";case 65:return "HELTEC CAPSULE SENSOR V3";case 66:return "HELTEC VISION MASTER T190";case 67:return "HELTEC VISION MASTER E213";case 68:return "HELTEC VISION MASTER E290";case 69:return "HELTEC MESH NODE T114";case 70:return "SENSECAP INDICATOR";case 71:return "TRACKER T1000 E";case 72:return "RAK3172";case 73:return "WIO E5";case 74:return "RADIOMASTER 900 BANDIT";case 75:return "ME25LS01 4Y10TD";case 76:return "RP2040 FEATHER RFM95";case 77:return "M5STACK COREBASIC";case 78:return "M5STACK CORE2";case 79:return "RPI PICO2";case 80:return "M5STACK CORES3";case 81:return "SEEED XIAO S3";case 82:return "MS24SF1";case 83:return "TLORA C6";case 84:return "WISMESH TAP";case 85:return "ROUTASTIC";case 86:return "MESH TAB";case 87:return "MESHLINK";case 88:return "XIAO NRF52 KIT";case 89:return "THINKNODE M1";case 90:return "THINKNODE M2";case 91:return "T ETH ELITE";case 92:return "HELTEC SENSOR HUB";case 93:return "MUZI BASE";case 94:return "HELTEC MESH POCKET";case 95:return "SEEED SOLAR NODE";case 96:return "NOMADSTAR METEOR PRO";case 97:return "CROWPANEL";case 98:return "LINK 32";case 99:return "SEEED WIO TRACKER L1";case 100:return "SEEED WIO TRACKER L1 EINK";case 101:return "MUZI R1 NEO";case 102:return "T DECK PRO";case 103:return "T LORA PAGER";case 104:return "M5STACK RESERVED";case 105:return "WISMESH TAG";case 106:return "RAK3312";case 107:return "THINKNODE M5";case 108:return "HELTEC MESH SOLAR";case 109:return "T ECHO LITE";case 110:return "HELTEC V4";case 111:return "M5STACK C6L";case 112:return "M5STACK CARDPUTER ADV";case 113:return "HELTEC WIRELESS TRACKER V2";case 114:return "T WATCH ULTRA";case 115:return "THINKNODE M3";case 116:return "WISMESH TAP V2";case 117:return "RAK3401";case 118:return "RAK6421";case 119:return "THINKNODE M4";case 120:return "THINKNODE M6";case 121:return "MESHSTICK 1262";case 122:return "TBEAM 1 WATT";case 123:return "T5 S3 EPAPER PRO";case 124:return "TBEAM BPF";case 125:return "MINI EPAPER S3";case 126:return "TDISPLAY S3 PRO";case 127:return "HELTEC MESH NODE T096";case 128:return "MESH TRACKER X1";case 129:return "THINKNODE M7";case 130:return "THINKNODE M8";case 131:return "THINKNODE M9";case 132:return "HELTEC V4 R8";case 133:return "HELTEC MESH NODE T1";case 134:return "STATION G3";case 135:return "T IMPULSE PLUS";case 136:return "T ECHO CARD";case 137:return "SEEED WIO TRACKER L2";case 138:return "CROWPANEL P4";case 139:return "HELTEC MESH TOWER V2";case 140:return "MESHNOLOGY W10";case 141:return "HELTEC RC32";case 142:return "HELTEC RC52";case 143:return "HELTEC RCC6";case 144:return "SEEED WIO TRACKER L1 PRO 1W";case 145:return "MESHNOLOGY W12";case 146:return "MESHPAGER X2";case 147:return "T CONNECT PRO";case 148:return "AXIOMETA GENESIS MINI";case 149:return "MAKERFABS NOMAD TERMINAL";case 150:return "THINKNODE MX";case 151:return "NODARA";case 152:return "HELTEC MESH POCKET V2";case 255:return "PRIVATE HW";default:return "Hardware #"+value;}}
    static boolean atLeast(String version,int major,int minor,int patch){java.util.regex.Matcher m=java.util.regex.Pattern.compile("(\\d+)\\.(\\d+)\\.(\\d+)").matcher(version);if(!m.find())return false;int[] target={major,minor,patch};for(int i=0;i<3;i++){int n=Integer.parseInt(m.group(i+1));if(n!=target[i])return n>target[i];}return true;}
    static String label(Option[] options,int value,String fallback){for(Option o:options)if(o.value==value)return o.label;return fallback+" #"+value;}
    static String presetName(int value){return label(PRESETS,value,"Unknown preset");}
    static String regionName(int value){return label(REGIONS,value,"Unknown region");}
    static List<Integer> packed(byte[] bytes){List<Integer> out=new ArrayList<>();int pos=0;while(pos<bytes.length){long n=0;int shift=0;boolean done=false;while(pos<bytes.length&&shift<64){int b=bytes[pos++]&255;n|=(long)(b&127)<<shift;if((b&128)==0){done=true;break;}shift+=7;}if(!done)throw Wire.bad("Invalid packed enum");out.add((int)n);}return out;}
    static byte[] constraint(ProtocolClient c,int region){List<byte[]> groups=new ArrayList<>();List<Wire.F> fs=Wire.fields(c.mtRegionMap);for(Wire.F f:fs)if(f.tag==1)groups.add(f.bytes);for(Wire.F f:fs)if(f.tag==2){List<Wire.F> r=Wire.fields(f.bytes);if(Wire.number(r,1,0)==region){int index=(int)Wire.number(r,2,0);if(index>=0&&index<groups.size())return groups.get(index);}}return null;}
    static Set<Integer> allowed(ProtocolClient c,int region){byte[] group=constraint(c,region);Set<Integer> out=new LinkedHashSet<>();if(group!=null)for(Wire.F f:Wire.fields(group))if(f.tag==1){if(f.wire==2)out.addAll(packed(f.bytes));else out.add((int)f.number);}return out;}
    static boolean selectable(ProtocolClient c,int region,int preset){boolean known=false;for(Option option:PRESETS)if(option.value==preset)known=true;if(!known||regionInfo(region)==null)return false;if(!c.mtAtLeast(8,0)&&preset>=10)return false;if(region==13&&(preset==8||preset==9||preset==16)&&allowed(c,region).isEmpty())return false;double span=span(region);if(Double.isFinite(span)&&presetBandwidth(preset,region)>span*1000+.001)return false;if(!c.mtAtLeast(7,17)&&preset==9)return false;if(c.mtAtLeast(6,5)&&preset==2)return false;Set<Integer> allowed=allowed(c,region);if(!allowed.isEmpty()&&!allowed.contains(preset))return false;byte[] group=constraint(c,region);if(group!=null&&Wire.number(Wire.fields(group),3,0)!=0&&!c.mtHam())return false;
        if(preset==10||preset==11)return regionName(region).equals("EU 866");if(preset==12||preset==13)return regionName(region).equals("EU N 868")||regionName(region).contains("70CM")||regionName(region).contains("125CM");if(preset==14||preset==15)return regionName(region).contains(" 2M")&&c.mtHam();return true;}
    static List<Option> presets(ProtocolClient c,int region){List<Option> result=new ArrayList<>();for(Option p:PRESETS)if(selectable(c,region,p.value))result.add(p);return result;}
    static List<Option> regions(ProtocolClient c){List<Option> result=new ArrayList<>();for(Option p:REGIONS)if((regionInfo(p.value)!=null&&(p.value<=26||c.mtAtLeast(8,0)))||p.value==c.region)result.add(p);return result;}
    static byte[] regionChange(ProtocolClient c,byte[] draft,int region){byte[] result=Wire.replace(draft,7,region);int current=(int)Wire.number(Wire.fields(draft),2,0);if(!selectable(c,region,current)){List<Option> options=presets(c,region);if(options.isEmpty())throw Wire.bad("No supported presets for this region; enable licensed mode if required");byte[] group=constraint(c,region);int preferred=group==null?options.get(0).value:(int)Wire.number(Wire.fields(group),2,options.get(0).value);if(!selectable(c,region,preferred))preferred=options.get(0).value;result=Wire.replace(result,2,preferred);}return result;}
    /** Region facts: official Android 2.8.4 ChannelOption and firmware d9c9916 RadioInterface.
     * Frequencies/spacing/padding are MHz. No generic fallback for reserved region IDs. */
    static final class Region {
        final int id,power;final double start,end,spacing,padding;final boolean licensed;
        Region(int id,double start,double end,int power,double spacing,double padding,boolean licensed){
            this.id=id;this.start=start;this.end=end;this.power=power;this.spacing=spacing;this.padding=padding;this.licensed=licensed;
        }
    }
    static final Region[] REGION_INFO={
        new Region(0,902,928,30,0,0,false),new Region(1,902,928,30,0,0,false),
        new Region(2,433,434,10,0,0,false),new Region(3,869.4,869.65,27,0,0,false),
        new Region(4,470,510,19,0,0,false),new Region(5,920.5,923.5,13,0,0,false),
        new Region(6,915,928,30,0,0,false),new Region(7,920,923,23,0,0,false),
        new Region(8,920,925,27,0,0,false),new Region(9,868.7,869.2,20,0,0,false),
        new Region(10,865,867,30,0,0,false),new Region(11,864,868,36,0,0,false),
        new Region(12,920,925,27,0,0,false),new Region(13,2400,2483.5,10,0,0,false),
        new Region(14,433,434.7,10,0,0,false),new Region(15,868,868.6,14,0,0,false),
        new Region(16,433,435,20,0,0,false),new Region(17,919,924,27,0,0,false),
        new Region(18,917,925,20,0,0,false),new Region(19,433,434.7,10,0,0,false),
        new Region(20,868,869.4,14,0,0,false),new Region(21,915,918,24,0,0,false),
        new Region(22,433.05,434.79,14,0,0,false),new Region(23,433.075,434.775,10,0,0,false),
        new Region(24,863,868,30,0,0,false),new Region(25,865,868,30,0,0,false),
        new Region(26,902,907.5,30,0,0,false),
        new Region(27,144,146,30,0,.0022,true),new Region(28,144,148,30,0,.0022,true),
        new Region(29,865.6,867.6,27,.4,.0375,false),new Region(32,869.4,869.65,27,0,.0104,false),
        new Region(33,144,148,30,0,.0022,true),new Region(34,430,440,30,0,.01875,true),
        new Region(35,420,450,30,0,.01875,true),new Region(36,430,450,30,0,.01875,true),
        new Region(37,220,225,30,0,.01875,true)
    };
    static Region regionInfo(int id){for(Region r:REGION_INFO)if(r.id==id)return r;return null;}
    static double span(int region){Region r=regionInfo(region);return r==null?Double.NaN:r.end-r.start;}
    static int powerLimit(int region){Region r=regionInfo(region);if(r==null)throw Wire.bad("This region has no supported frequency profile");return r.power;}
    static int slotCount(int region,double bandwidthKhz){
        Region r=regionInfo(region);if(r==null)throw Wire.bad("This region has no supported frequency profile");
        if(region==0)return 0;
        double slotWidth=bandwidthKhz/1000+r.spacing+2*r.padding;
        if(!Double.isFinite(slotWidth)||slotWidth<=0||slotWidth>r.end-r.start+.000001)throw Wire.bad("Bandwidth and regional spacing exceed this frequency band");
        // Official app and firmware round instead of truncating the frequency-slot count.
        return Math.max(1,(int)Math.round((r.end-r.start+r.spacing)/slotWidth));
    }
    static double presetBandwidth(int preset,int region){double bw=preset==1||preset==7||preset==10||preset==11?125:preset==2||preset==12||preset==13?62.5:preset==14||preset==15?15.625:preset==8||preset==9||preset==16?500:250;return region==13?bw*3.25:bw;}
    static double manualBandwidth(long code,int region){if(region==13){if(code==200)return 203.125;if(code==400)return 406.25;if(code==800)return 812.5;throw Wire.bad("Choose 2.4 GHz bandwidth code 200, 400 or 800");}switch((int)code){case 7:return 7.8125;case 10:return 10.416667;case 15:return 15.625;case 20:return 20.833333;case 31:return 31.25;case 41:return 41.666667;case 62:return 62.5;case 125:return 125;case 250:return 250;case 500:return 500;default:throw Wire.bad("Choose a supported bandwidth code: 7, 10, 15, 20, 31, 41, 62, 125, 250 or 500");}}
    static int presetCodingRate(int preset){switch(preset){case 1:case 2:case 7:case 9:return 8;case 12:case 13:case 15:return 6;default:return 5;}}
    // Firmware AdminModule clamps coding_rate=0 to 5. In preset mode a stored
    // rate below the preset's rate still uses the preset's rate on air.
    static int codingSelection(int preset,int stored){return stored==0||stored>=5&&stored<=presetCodingRate(preset)?0:stored;}
    static String currentPreset(ProtocolClient c){if(!c.mtConfigLoaded(5))return "Reading preset…";List<Wire.F> fs=Wire.fields(c.mtConfig(5));return Wire.number(fs,1,0)!=0?presetName((int)Wire.number(fs,2,0)):"Manual LoRa";}
    static boolean codingMatches(byte[] actual,byte[] desired){List<Wire.F> a=Wire.fields(actual),d=Wire.fields(desired);int preset=(int)Wire.number(d,2,0);return Wire.number(d,1,0)!=0&&Wire.number(a,1,0)!=0&&Wire.number(a,2,0)==preset&&codingSelection(preset,(int)Wire.number(a,5,0))==codingSelection(preset,(int)Wire.number(d,5,0));}
    static final int SF_UNKNOWN=0,SF_RF95=1,SF_MODERN=2;
    static int sfCapability(int hardware){
        // Only unambiguous modem models. TBEAM and other multi-modem variants remain unknown.
        switch(hardware){
            case 1:case 2:case 3:case 5:case 8:case 10:case 11:case 15:case 76:return SF_RF95;
            case 7:case 9:case 43:case 44:case 48:case 49:case 50:case 69:case 71:case 94:case 110:case 132:return SF_MODERN;
            default:return SF_UNKNOWN;
        }
    }
    static String manualSfHint(int hardware){
        if(sfCapability(hardware)==SF_RF95)return "This RF95 modem supports SF7–SF12.";
        if(sfCapability(hardware)==SF_MODERN)return "This modem family supports SF5–SF12; the radio verifies the saved values.";
        return "The radio does not report its modem variant. SF5/SF6 support will be checked by reading the saved values back.";
    }
    /** Compare effective LoRa state, including fields omitted because their proto3 value is zero. */
    static String readbackDifference(byte[] actual,byte[] desired){
        List<Wire.F> a=Wire.fields(actual),d=Wire.fields(desired);
        boolean preset=Wire.number(d,1,0)!=0;
        Set<Integer> tags=new LinkedHashSet<>(Arrays.asList(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,103,104,105,106));
        for(Wire.F f:d)tags.add(f.tag); // Preserve and verify unknown fields already held by this radio.
        for(int tag:tags){
            if(preset&&(tag==3||tag==4))continue; // Inactive manual parameters may be normalized by firmware.
            if(!preset&&tag==2)continue;
            if(tag==5&&preset&&codingMatches(actual,desired))continue;
            if(tag==10&&Wire.number(d,tag,0)==0){long power=Wire.number(a,tag,0);if(power>=0&&power<=36)continue;}
            if(tag==103){if(ignoreList(a).equals(ignoreList(d)))continue;}
            else if(equalField(a,d,tag))continue;
            if(tag==2)return "Preset differs (requested "+presetName((int)Wire.number(d,2,0))+"; radio "+presetName((int)Wire.number(a,2,0))+")";
            return loraFieldName(tag)+" differs from the requested settings";
        }
        return "";
    }
    private static boolean equalField(List<Wire.F> a,List<Wire.F> d,int tag){
        List<String> av=new ArrayList<>(),dv=new ArrayList<>();
        for(Wire.F f:a)if(f.tag==tag)av.add(fieldValue(f));
        for(Wire.F f:d)if(f.tag==tag)dv.add(fieldValue(f));
        // Omitted scalar/bytes defaults equal explicitly encoded zero/empty defaults.
        av.removeIf(v->v.equals("0")||v.equals("bytes:"));dv.removeIf(v->v.equals("0")||v.equals("bytes:"));
        return av.equals(dv);
    }
    private static String fieldValue(Wire.F f){return f.wire==2?"bytes:"+Wire.hex(f.bytes):f.number==0?"0":f.wire+":"+f.number;}
    private static List<Integer> ignoreList(List<Wire.F> fs){List<Integer> values=new ArrayList<>();for(Wire.F f:fs)if(f.tag==103){if(f.wire==2)values.addAll(packed(f.bytes));else values.add((int)f.number);}Collections.sort(values);return values;}
    private static String loraFieldName(int tag){switch(tag){case 1:return "Preset/manual mode";case 3:return "Bandwidth";case 4:return "Spreading factor";case 5:return "Coding rate";case 7:return "Region";case 8:return "Hop limit";case 9:return "Transmit enable";case 10:return "Transmit power";case 11:return "Frequency slot";case 14:return "Frequency override";default:return "LoRa field "+tag;}}
    static void validate(byte[] config,ProtocolClient c){List<Wire.F> fs=Wire.fields(config);int region=(int)Wire.number(fs,7,0),preset=(int)Wire.number(fs,2,0),cr=(int)Wire.number(fs,5,0);boolean usePreset=Wire.number(fs,1,0)!=0;
        if(regionInfo(region)==null||region>26&&!c.mtAtLeast(8,0)&&region!=c.region)throw Wire.bad("Unsupported region for this firmware");
        if(regionInfo(region).licensed&&!c.mtHam())throw Wire.bad("This region requires licensed amateur radio mode");
        long hop=Wire.number(fs,8,0),slot=Wire.number(fs,11,0),power=Wire.number(fs,10,0);if(hop<0||hop>7||slot<0||slot>65535||power<0||power>36)throw Wire.bad("Invalid hop limit, frequency slot or power");
        if(power>powerLimit(region)&&!c.mtHam())throw Wire.bad("Power exceeds the selected region ceiling; use 0 for automatic");double bw=usePreset?presetBandwidth(preset,region):manualBandwidth(Wire.number(fs,3,0),region);int slots=slotCount(region,bw);if(slot>slots)throw Wire.bad("Frequency slot exceeds this region/bandwidth (0 = automatic; maximum "+slots+")");
        if(usePreset){if(!selectable(c,region,preset))throw Wire.bad("Preset is unsupported for this firmware/region");if(cr!=0&&(cr<5||cr>8||cr>presetCodingRate(preset)&&!c.mtAtLeast(7,18)))throw Wire.bad("Use coding rate 0 or 5–8; custom overrides require firmware 2.7.18+");}
        else {long sf=Wire.number(fs,4,0),bandwidth=Wire.number(fs,3,0);if(sf<5||sf>12||cr<5||cr>8||bandwidth<1||bandwidth>500&&region!=13)throw Wire.bad("Set manual bandwidth, SF 5–12 and coding rate 5–8");if(region==13&&bandwidth!=200&&bandwidth!=400&&bandwidth!=800)throw Wire.bad("2.4 GHz bandwidth must be 200, 400 or 800");if((sfCapability(c.mtHardware)==SF_RF95)&&sf<7)throw Wire.bad("This or an unknown radio requires SF 7–12");}
        float freq=Float.intBitsToFloat((int)Wire.number(fs,14,0));if(!Float.isFinite(freq)||freq<0||freq>2500)throw Wire.bad("Invalid frequency override");
    }
}
