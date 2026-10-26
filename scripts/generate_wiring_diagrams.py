from pathlib import Path
from html import escape
from xml.etree import ElementTree
OUT=Path(__file__).resolve().parents[1] / "docs" / "images"
OUT.mkdir(parents=True,exist_ok=True)
C=dict(bg='#071628',panel='#11243a',panel2='#0b2034',cyan='#36c8f4',teal='#34d4ba',amber='#ffba65',purple='#9a9afc',white='#edf5ff',muted='#a6bfd4',line='#38516d',warn='#f4bc58',red='#fa7171',blue='#328bef')
class Art:
 def __init__(self,title,subtitle,footer):
  self.a=['<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="720" viewBox="0 0 1280 720" role="img" aria-label="'+escape(title,quote=True)+'">','<defs><linearGradient id="bg" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#051325"/><stop offset="1" stop-color="#112a45"/></linearGradient><linearGradient id="glass" x1="0" x2="1"><stop offset="0" stop-color="#d9efff" stop-opacity="0.21"/><stop offset="1" stop-color="#8bd0f5" stop-opacity="0.07"/></linearGradient><marker id="arr" markerWidth="7" markerHeight="7" refX="6" refY="3.5" orient="auto" markerUnits="strokeWidth"><path d="M0 0 L7 3.5 L0 7 z" fill="#85dbff"/></marker><marker id="arr2" markerWidth="7" markerHeight="7" refX="6" refY="3.5" orient="auto" markerUnits="strokeWidth"><path d="M0 0 L7 3.5 L0 7 z" fill="#ffba65"/></marker></defs>']
  self.rect(0,0,1280,720,fill='url(#bg)')
  self.rect(14,14,1252,692,rx=22,fill='none',stroke='#244260',sw=2)
  self.text(40,62,'MeshCrossway',34,C['cyan'],weight='bold')
  self.text(325,61,'| '+title,28,C['white'],weight='bold')
  self.text(40,91,subtitle,15,C['muted'])
  self.rect(30,669,1220,27,rx=8,fill='#162a40',stroke='#253f57')
  self.text(44,688,footer,13,C['warn'])
 def attr(self,d):return ' '.join(f'{k.replace("_","-")}="{escape(str(v),quote=True)}"' for k,v in d.items() if v is not None)
 def rect(self,x,y,w,h,rx=0,fill=None,stroke=None,sw=1,opacity=None):self.a.append('<rect '+self.attr(dict(x=x,y=y,width=w,height=h,rx=rx,fill=fill or 'none',stroke=stroke or 'none',stroke_width=sw,opacity=opacity))+'/>')
 def text(self,x,y,t,size=18,color=None,weight='normal',anchor=None,opacity=None):self.a.append('<text '+self.attr(dict(x=x,y=y,font_family='DejaVu Sans,Arial,sans-serif',font_size=size,fill=color or C['white'],font_weight=weight,text_anchor=anchor,opacity=opacity))+'>'+escape(t)+'</text>')
 def line(self,x1,y1,x2,y2,color=None,sw=2,arrow=False,dash=None):self.a.append('<line '+self.attr(dict(x1=x1,y1=y1,x2=x2,y2=y2,stroke=color or C['cyan'],stroke_width=sw,stroke_dasharray=dash,marker_end='url(#arr)' if arrow else None))+'/>')
 def path(self,d,color=None,sw=2,arrow=False,dash=None):self.a.append('<path '+self.attr(dict(d=d,fill='none',stroke=color or C['cyan'],stroke_width=sw,stroke_dasharray=dash,stroke_linejoin='round',marker_end='url(#arr)' if arrow else None))+'/>')
 def circ(self,x,y,r,fill=None,stroke=None,sw=1):self.a.append('<circle '+self.attr(dict(cx=x,cy=y,r=r,fill=fill or 'none',stroke=stroke or 'none',stroke_width=sw))+'/>')
 def card(self,x,y,w,h,title,sub='',edge=None,fs=20):
  edge=edge or C['cyan'];self.rect(x,y,w,h,rx=14,fill=C['panel'],stroke=edge,sw=1.7)
  self.text(x+18,y+32,title,fs,C['white'],'bold')
  if sub:self.text(x+18,y+53,sub,13,C['muted'])
 def lines(self,x,y,rows,step=25,size=15,color=None):
  for i,row in enumerate(rows):self.text(x,y+i*step,row,size,color or C['white'])
 def chip(self,x,y,w,txt,edge=None):
  edge=edge or C['cyan'];self.rect(x,y,w,24,rx=12,fill='#092439',stroke=edge);self.text(x+w/2,y+17,txt,12,edge,'bold','middle')
 def xiao(self,x,y,scale=1,kind='MT'):
  w=150*scale;h=90*scale;clr=C['cyan'] if kind=='MT' else C['amber']
  self.rect(x,y,w,h,rx=7,fill='#07121c',stroke=clr,sw=2)
  for i in range(7):
   bx=x+(11+i*20)*scale;self.rect(bx,y-4*scale,8*scale,5*scale,rx=1,fill='#d7b86a');self.rect(bx,y+h-1*scale,8*scale,5*scale,rx=1,fill='#d7b86a')
  self.rect(x+17*scale,y+13*scale,86*scale,55*scale,rx=5,fill='#c4d3dc',stroke='#7e99aa')
  self.text(x+60*scale,y+36*scale,'Wio-SX1262',11*scale,'#122939','bold','middle')
  self.text(x+60*scale,y+51*scale,'LoRa RF',9*scale,'#34465a',anchor='middle')
  self.rect(x+115*scale,y+22*scale,23*scale,44*scale,rx=2,fill='#224259',stroke='#86b4d1')
  self.text(x+126*scale,y+48*scale,'X',10*scale,C['white'],'bold','middle')
  self.text(x+w/2,y+h+20*scale,'XIAO + Wio-SX1262',12*scale,clr,'bold','middle')
 def display(self,x,y,w=370,h=225):
  self.rect(x-10,y-10,w+20,h+20,rx=17,fill='url(#glass)',stroke='#bdd9ea',sw=3)
  self.rect(x,y,w,h,rx=8,fill='#061423',stroke='#456277',sw=3)
  self.rect(x+12,y+12,w-24,27,rx=5,fill='#0c2d46')
  self.text(x+22,y+30,'MeshCrossway',15,C['cyan'],'bold')
  self.text(x+w-16,y+29,'MT   MC   GNSS',11,C['teal'],anchor='end')
  self.rect(x+14,y+48,w*.52,h-65,rx=4,fill='#1b525d')
  for j in range(4):self.path(f'M {x+20} {y+75+j*29} q 44 -24 93 0 t 93 0',color='#6ba2a3',sw=1)
  for dx,dy,clr in [(45,76,C['cyan']),(120,129,C['teal']),(165,83,C['amber'])]:self.circ(x+dx,y+dy,6,fill=clr,stroke='#edf5ff',sw=2)
  self.rect(x+20+w*.52,y+48,w*.43,h-65,rx=5,fill='#101e30')
  for j in range(3):
   yy=y+74+j*42;self.circ(x+34+w*.52,yy,7,fill=[C['cyan'],C['teal'],C['amber']][j]);self.line(x+48+w*.52,yy-3,x+w-29,yy-3,color='#527289',sw=3);self.line(x+48+w*.52,yy+7,x+w-50,yy+7,color='#395975',sw=2)
  self.text(x+w/2,y+h+36,'Waveshare ESP32-S3-Touch-LCD-3.5',13,C['muted'],anchor='middle')
 def battery(self,x,y,w=180,h=104):
  self.rect(x,y,w,h,rx=14,fill='#d1d8d9',stroke='#e8b353',sw=5)
  self.rect(x+8,y+5,w-16,h-10,rx=8,fill='#cfd8d8')
  self.text(x+w/2,y+40,'REAR BATTERY',16,'#293746','bold','middle')
  self.text(x+w/2,y+63,'1S LiPo candidate',13,'#293746',anchor='middle')
  self.text(x+w/2,y+82,'10,000 mAh',12,'#293746',anchor='middle')
 def save(self,path):
  self.a.append('</svg>');p=OUT/path;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('\n'.join(self.a),encoding='utf8')
  return p

# 1. System overview
s=Art('System architecture','V1: one shared carrier PCB, two independent XIAO/Wio radios and a rear LiPo','Conceptual only. Actual board placement, antenna isolation and firmware integration remain unvalidated.')
s.card(45,194,255,265,'Radio A: Meshtastic','XIAO ESP32S3 + Wio-SX1262',C['cyan']);s.xiao(86,275,.95,'MT');s.chip(101,422,140,'LoRa • independent',C['cyan'])
s.display(453,217,366,230)
s.card(980,194,255,265,'Radio B: MeshCore','XIAO ESP32S3 + Wio-SX1262',C['amber']);s.xiao(1018,275,.95,'MC');s.chip(1039,422,142,'LoRa • independent',C['amber'])
s.line(300,334,446,334,C['cyan'],3,True);s.chip(327,294,95,'UART1',C['cyan']);s.line(832,334,974,334,C['amber'],3,True);s.chip(861,294,95,'UART2',C['amber'])
s.card(55,528,250,103,'u-blox MAX-M10S','GNSS breakout + active antenna',C['teal']);s.lines(74,599,['Proposed I2C GPIO21 / GPIO38'],24,14)
s.card(428,528,406,103,'ONE custom carrier PCB','Display + both radios + GNSS + service connectors',C['teal']);s.lines(450,598,['Battery mounted on the rear of the carrier'],23,14)
s.card(969,528,265,103,'Rear 1S LiPo battery','Protected cell + Waveshare AXP2101',C['amber']);s.lines(987,598,['Capacity / power path: unverified'],23,13)
s.path('M 287 528 V 491 H 520 V 468',C['teal'],2,True)
s.path('M 1110 528 V 494 H 759 V 467',C['amber'],2,True)
s.save('architecture/system-overview.svg')

# 2. Hardware overview
s=Art('Integrated PCB + transparent case','Layered mechanical concept; an exploded illustration, not dimensional CAD','Mechanical clearances, battery thickness, antenna mounts and USB access must be measured on physical parts.')
s.card(50,193,355,408,'01  Transparent front shell','Almost invisible protective polycarbonate',C['cyan']);s.rect(81,276,289,226,rx=26,fill='url(#glass)',stroke='#a8d4f6',sw=4);s.rect(106,311,239,165,rx=10,fill='#0c233b',stroke='#437f9e',sw=2);s.text(226,375,'3.5-inch',30,C['white'],'bold','middle');s.text(226,409,'touch display',18,C['muted'],anchor='middle');s.circ(92,290,7,fill='#99c8e0');s.circ(355,488,7,fill='#99c8e0')
s.card(445,193,425,408,'02  One custom carrier PCB','Both LoRa modules removable for service',C['teal']);s.rect(471,262,371,262,rx=14,fill='#071e27',stroke=C['teal'],sw=3);s.xiao(496,293,.9,'MT');s.xiao(690,293,.9,'MC');s.rect(555,442,130,49,rx=8,fill='#112e3b',stroke=C['cyan']);s.text(620,464,'Waveshare',14,C['white'],'bold','middle');s.text(620,482,'3.5-inch host',12,C['muted'],anchor='middle');s.rect(700,441,116,47,rx=7,fill='#193e40',stroke=C['teal']);s.text(758,462,'MAX-M10S',12,C['white'],'bold','middle');s.text(758,480,'GNSS',11,C['muted'],anchor='middle');s.text(652,559,'MicroSD uses Waveshare onboard slot',13,C['muted'],anchor='middle')
s.card(912,193,317,408,'03  Battery at rear','Behind the PCB, inside the back shell',C['amber']);s.battery(968,288,206,123);s.text(1072,464,'Clear rear enclosure',16,C['white'],'bold','middle');s.lines(952,496,['• LiPo protection required','• Charger managed by Waveshare','• Battery dimensions unverified'],27,14)
s.save('hardware-overview.svg')

# 3. Radio UART wiring
s=Art('Radio UART wiring','Both radios are 3.3 V serial peers; host TX must cross to radio RX','Candidate GPIO assignments require firmware-target and physical pinout verification on delivered boards.')
for x,title,sub,edge,rows in [
 (48,'A  Meshtastic','Serial Module: PROTO',C['cyan'],['Host GPIO39 TX  →  XIAO D3 / GPIO4 RX','Host GPIO40 RX  ←  XIAO D2 / GPIO3 TX','GND                 ↔  GND']),
 (667,'B  MeshCore','Serial Companion binary protocol',C['amber'],['Host GPIO17 TX  →  XIAO D7 / GPIO44 RX','Host GPIO18 RX  ←  XIAO D6 / GPIO43 TX','GND                 ↔  GND'])]:
 s.card(x,181,565,414,title,sub,edge,22)
 s.card(x+22,258,204,206,'Waveshare host','ESP32-S3 3.5-inch',edge,17)
 s.card(x+337,258,204,206,'XIAO / Wio','SX1262 radio module',edge,17)
 for i,row in enumerate(rows):
  yy=321+53*i
  src,dst=(('GPIO39','D3 / GPIO4'),('GPIO40','D2 / GPIO3'),('GND','GND'))[i] if x==48 else (('GPIO17','D7 / GPIO44'),('GPIO18','D6 / GPIO43'),('GND','GND'))[i]
  s.chip(x+37,yy,166,src,edge);s.chip(x+354,yy,166,dst,edge)
  s.line(x+226,yy+12,x+332,yy+12,edge,2.5,i!=1)
  if i==1:s.path(f'M {x+332} {yy+18} H {x+226}',edge,2.5,True)
 s.lines(x+34,511,rows,28,15)
 s.chip(x+204,567,157,'115200 • 8N1',edge)
s.save('radio-wiring.svg')

# 4. Connector reference
s=Art('Connector reference','Physical signal identity only: do not infer J8 position numbering from marketing photos','Use actual PCB revision/schematic. Meshtastic D2/D3 and MeshCore D6/D7 need confirmed free GPIOs.')
s.card(50,185,530,415,'Waveshare host expansion (J8)','Board revision and pin numbering must be inspected',C['cyan'])
s.lines(77,277,['GPIO39   →  UART1 TX → Meshtastic RX','GPIO40   ←  UART1 RX ← Meshtastic TX','GPIO17   →  UART2 TX → MeshCore RX','GPIO18   ←  UART2 RX ← MeshCore TX','GPIO21   ↔  GNSS SDA (proposed)','GPIO38   ↔  GNSS SCL (proposed)','3V3/GND: POWER PENDING CURRENT TEST'],42,17)
s.card(625,185,604,183,'Radio A  •  Meshtastic','XIAO D3 / GPIO4 RX      XIAO D2 / GPIO3 TX',C['cyan']);s.xiao(669,256,.75,'MT');s.lines(867,280,['Serial PROTO','115200 baud (verify)','Shared GND'],28,15)
s.card(625,402,604,198,'Radio B  •  MeshCore','XIAO D7 / GPIO44 RX     XIAO D6 / GPIO43 TX',C['amber']);s.xiao(670,480,.75,'MC');s.lines(868,503,['Companion Serial','115200 baud (verify)','Shared GND'],27,15)
s.save('connector-reference.svg')

# 5. Full wiring
s=Art('Wiring diagram','UART, GNSS I2C and grounds — a proposed bench pin contract, NOT a fabrication schematic','No 3.3 V source connection is approved until Waveshare J8 and XIAO external-feed checks pass.')
s.card(45,175,337,437,'Waveshare ESP32-S3 host','No camera; two UARTs + GNSS I2C',C['cyan'])
s.lines(63,260,['GPIO39  •  UART1 TX','GPIO40  •  UART1 RX','GPIO17  •  UART2 TX','GPIO18  •  UART2 RX','GPIO21  •  GNSS SDA','GPIO38  •  GNSS SCL','GND  •  common reference','3V3  •  conditional only'],42,17)
s.card(537,175,698,140,'Radio A  /  Meshtastic','Seeed XIAO ESP32S3 + Wio-SX1262',C['cyan'])
s.lines(563,243,['RX  D3/GPIO4 ← Host GPIO39 TX','TX  D2/GPIO3 → Host GPIO40 RX','GND ↔ Host GND  •  PROTO 115200'],22,16)
s.card(537,331,698,140,'Radio B  /  MeshCore','Seeed XIAO ESP32S3 + Wio-SX1262',C['amber'])
s.lines(563,400,['RX  D7/GPIO44 ← Host GPIO17 TX','TX  D6/GPIO43 → Host GPIO18 RX','GND ↔ Host GND  •  Companion 115200'],22,16)
s.card(537,487,698,125,'G1  /  u-blox MAX-M10S','Selected 3 V breakout with active GNSS antenna (bias to verify)',C['teal'])
s.lines(563,557,['SDA ↔ Host GPIO21   |   SCL ↔ Host GPIO38   |   GND ↔ Host GND'],24,15)
for y,cl in [(259,C['cyan']),(416,C['amber']),(554,C['teal'])]:s.path(f'M 382 {y} H 500 V {y-15} H 530',cl,2.5,True)
s.save('architecture/wiring-diagram.svg')

# 6. GNSS and power
s=Art('GNSS + battery layout','MAX-M10S I2C remains central; battery moves behind the carrier and screen','Confirm GNSS antenna bias, LiPo protection/connector polarity, battery fit, and power-source isolation.')
s.card(58,187,342,420,'Waveshare / AXP2101','Host charger and power management',C['cyan'])
s.lines(81,291,['USB-C 5 V → internal power path','Protected 1S LiPo → BAT port','Host GNSS SDA: GPIO21','Host GNSS SCL: GPIO38','Ground: common reference'],43,17)
s.card(454,187,332,420,'u-blox MAX-M10S','Breakout with active GNSS antenna',C['teal']);s.rect(551,288,135,135,rx=15,fill='#d1e1df',stroke=C['teal'],sw=4);s.text(619,343,'u-blox',21,'#15344d','bold','middle');s.text(619,373,'MAX-M10S',15,'#15344d',anchor='middle');s.lines(476,480,['I2C: SDA / SCL at 3.3 V','Antenna bias: MUST VERIFY','Separate from LoRa antenna feeds'],27,15)
s.card(845,187,379,420,'Rear mounted LiPo','Physically behind the screen/carrier',C['amber']);s.battery(923,295,215,124);s.lines(868,469,['3.7 V nominal protected cell','Do not parallel XIAO charger systems','Battery thickness and safety checks','J8 3.3 V power still CONDITIONAL'],29,15)
s.line(400,360,448,360,C['teal'],3,True);s.path('M 784 420 H 824 V 448 H 844',C['amber'],2,True)
s.save('gnss-and-power.svg')

# 7. Power architecture
s=Art('Power architecture','The Waveshare AXP2101 remains the intended single charger/power controller','HOLD: external 3.3 V feed, J8 spare current, transient loads and UART backfeeding are NOT verified.')
s.card(44,187,225,128,'USB-C input','5 V source into Waveshare',C['cyan']);s.card(44,368,225,131,'Protected 1S LiPo','Battery behind carrier PCB',C['amber']);s.card(360,219,357,283,'Waveshare / AXP2101','Battery charge + host rail',C['cyan']);s.lines(387,322,['Display and ESP32-S3 load','J8 3V3 rail: no rated spare output','XIAO external-3V3 input: check','Independent UART power sequencing'],42,17)
s.card(860,173,366,135,'Radio A + Radio B','2 × XIAO ESP32S3 + Wio-SX1262',C['amber']);s.card(860,354,366,127,'u-blox MAX-M10S','3 V breakout + active GNSS antenna',C['teal']);s.chip(929,533,217,'J8 3V3 • ON HOLD',C['warn'])
s.line(269,249,351,249,C['cyan'],2.5,True);s.path('M 270 433 H 324 V 441 H 353',C['amber'],2.5,True);s.path('M 718 325 H 802 V 240 H 855',C['amber'],2.5,True);s.path('M 718 399 H 802 V 422 H 855',C['teal'],2.5,True)
s.save('architecture/power-architecture.svg')

# 8. Communication stack
s=Art('Communication stack','One shared UI; two independent native mesh protocols','Never silently bridge Meshtastic to MeshCore, share keys or imply inter-protocol routing.')
s.card(314,130,652,92,'Touchscreen application','Waveshare LVGL: chat / map / contacts / settings',C['cyan']);s.card(314,250,652,96,'Core host services','Message history • GNSS • protocol adapters • queues',C['purple']);s.line(640,222,640,245,C['cyan'],3,True)
s.card(54,388,547,240,'Meshtastic path  /  UART1','PROTO binary frames, ToRadio / FromRadio',C['cyan']);s.lines(78,466,['XIAO ESP32S3 + Wio-SX1262','Native Meshtastic routing and channel model','LoRa RF → Meshtastic network','Host GPIO39/40 ↔ XIAO GPIO4/3'],36,17)
s.card(677,388,547,240,'MeshCore path  /  UART2','Companion binary command / response / events',C['amber']);s.lines(705,466,['XIAO ESP32S3 + Wio-SX1262','Native MeshCore contacts and path model','LoRa RF → MeshCore network','Host GPIO17/18 ↔ XIAO GPIO44/43'],36,17)
s.path('M 539 346 V 369 H 329 V 383',C['cyan'],2.5,True);s.path('M 744 346 V 369 H 950 V 383',C['amber'],2.5,True)
s.save('architecture/communication-stack.svg')

# 9. Firmware responsibilities
s=Art('Firmware responsibilities','Native firmware on each XIAO; application behavior belongs on the Waveshare host','Design principle: unify the user experience, not the radio networking protocols.')
for x,w,title,sub,color,rows in [
(41,388,'Waveshare host','LVGL + MeshCrossway application',C['cyan'],['480 × 320 touchscreen UI','Map / GPX / waypoint tools','Local message history and microSD','GNSS integration, status and policy','Two native serial protocol adapters','Battery and dual-radio status']),
(450,383,'XIAO A • Meshtastic','ESP32-S3 + SX1262 native firmware',C['teal'],['Meshtastic protocol, radio settings','Node / channel data and messages','Protobuf serial PROTO API','Radio telemetry as supported','TX state and ACK as supported','Own native mesh forwarding']),
(854,384,'XIAO B • MeshCore','ESP32-S3 + SX1262 native firmware',C['amber'],['MeshCore Companion firmware','Contacts and encrypted messages','Companion binary UART API','Network-specific discovery/trace','Own native path and relay model','Independent radio recovery'])]:
 s.card(x,173,w,438,title,sub,color,19)
 for i,row in enumerate(rows):s.circ(x+22,273+i*48,4,fill=color);s.text(x+39,279+i*48,row,15,C['white'])
s.save('architecture/firmware-responsibilities.svg')

# 10. Tracking modes
s=Art('Direct Tracking modes','Two protocol-specific forwarding policies, still unverified on selected firmware','Zero-hop behavior must be tested separately for MT and MC with a third relay and RF packet capture.')
for x,title,sub,col,direct in [(47,'Standard mesh forwarding','Depending on network routes and hop policy',C['amber'],False),(664,'Proposed direct-only tracking','One-hop update, intended zero relay forwarding',C['teal'],True)]:
 s.card(x,181,568,432,title,sub,col,19)
 for xx,name,role in [(x+40,'Device A','TX'),(x+224,'Repeater','RELAY'),(x+409,'Device B','RX')]:
  s.card(xx,311,118,91,name,role,C['line'],16)
 s.line(x+158,355,x+217,355,col,2,True);s.line(x+341,355,x+401,355,col,2,True)
 if direct:
  s.rect(x+224,311,118,91,rx=14,fill='#182a38',stroke=C['red'],sw=2)
  s.text(x+282,342,'Repeater',16,C['white'],'bold','middle');s.text(x+282,370,'NO RE-TX',14,C['red'],'bold','middle');s.line(x+201,421,x+365,291,C['red'],3)
  s.lines(x+46,491,['Individual position packet with zero forwarding','No repeater transmission intended','Requires native MT and MC capability tests'],31,16)
 else:s.lines(x+46,491,['Repeater transmits when network config allows','Route and flood behavior are protocol-specific','A direct message may traverse repeaters'],31,16)
s.save('architecture/direct-tracking-modes.svg')

print("Generated hardware SVGs:")
for p in sorted(OUT.rglob("*.svg")):
 if p.name not in {
  "system-overview.svg", "hardware-overview.svg", "wiring-diagram.svg",
  "radio-wiring.svg", "connector-reference.svg", "gnss-and-power.svg",
  "power-architecture.svg", "communication-stack.svg", "firmware-responsibilities.svg",
  "direct-tracking-modes.svg"
 }: continue
 ElementTree.parse(str(p))
 print(p.relative_to(OUT))