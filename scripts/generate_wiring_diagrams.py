"""Generate original, dependency-free SVGs for the proposed LoRa V3 bench.

Run from any directory. Labels use GPIO identities; photo positions are not
interchangeable with Rev2 schematic numbers. See docs/hardware-wiring.md.
"""
from pathlib import Path
from html import escape

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs' / 'images'
OUT.mkdir(parents=True, exist_ok=True)
INK='#182b42'; MUTED='#53657a'; BLUE='#1768b5'; GREEN='#087f68'; PURPLE='#8553b5'; RED='#c43c36'; GRAY='#596775'

def text(x,y,s,size=18,color=INK,bold=False,anchor='start'):
    return f'<text x="{x}" y="{y}" font-family="DejaVu Sans,Arial,sans-serif" font-size="{size}" fill="{color}" font-weight="{700 if bold else 400}" text-anchor="{anchor}">{escape(s)}</text>'

def start(w,h,title,sub):
    return [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}" role="img" aria-labelledby="title desc">',f'<title id="title">{escape(title)}</title><desc id="desc">{escape(sub)}</desc>',f'<rect width="{w}" height="{h}" fill="#f3f6fa"/>', '<defs><marker id="arrow" markerWidth="8" markerHeight="8" refX="7" refY="4" orient="auto-start-reverse"><path d="M0 0L8 4L0 8Z" fill="context-stroke"/></marker></defs>',text(40,50,title,28,INK,True),text(40,82,sub,16,MUTED)]

def box(x,y,w,h,title,color=BLUE):
    return [f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="16" fill="white" stroke="#ced8e4" stroke-width="2"/>',f'<path d="M{x+16} {y}H{x+w-16}Q{x+w} {y} {x+w} {y+16}V{y+52}H{x}V{y+16}Q{x} {y} {x+16} {y}" fill="{color}"/>',text(x+20,y+34,title,20,'white',True)]

def wire(x1,y1,x2,y2,color=BLUE,arrow=True):
    return f'<path d="M{x1} {y1}L{x2} {y2}" fill="none" stroke="{color}" stroke-width="3"'+(' marker-end="url(#arrow)"' if arrow else '')+'/>'

def save(name,s):
    (OUT/name).write_text('\n'.join(s+['</svg>']))

s=start(1280,950,'MeshCrossway | LoRa V3 radio wiring','Proposed, not bench-tested. U1 photo numbers follow the Amazon illustration. Check actual PCB GPIO labels.')
s+=box(40,120,480,650,'Waveshare ESP32-S3 controller')
s+=box(770,120,470,290,'Radio A | Meshtastic',BLUE)
s+=box(770,450,470,290,'Radio B | MeshCore',PURPLE)
for y,left,right,col,rev in [(230,'Photo 9 / GPIO39 / TX','GPIO4 / RX / J3.15',BLUE,False),(300,'Photo 11 / GPIO40 / RX','GPIO5 / TX / J3.16',GREEN,True),(560,'Photo 16 / GPIO17 / TX','GPIO4 / RX / J3.15',PURPLE,False),(630,'Photo 18 / GPIO18 / RX','GPIO5 / TX / J3.16',GREEN,True)]:
    s += [text(65,y-12,left,19,INK,True),text(795,y-12,right,19,INK,True),wire(770 if rev else 520,y,520 if rev else 770,y,col)]
s += [text(795,195,'LoRa V3: ESP32-S3 + SX1262',17),text(795,370,'PROTO: RX4 / TX5, 115200 8N1',17),text(795,525,'Second board from the two-pack',17),text(795,700,'Build flags: SERIAL_RX=4 / SERIAL_TX=5',16,PURPLE,True),text(65,710,'UART0 GPIO43/44 reserved for diagnostics',17,MUTED),text(65,745,'Camera absent; camera driver disabled',17,MUTED)]
s+=box(40,795,1200,110,'3.3 V signals + common ground + separate USB power',GRAY)
s += [text(65,877,'U1 GND -> A GND + B GND. No shared 3V3, 5V or battery power outputs.',19),text(40,932,'Radio GPIO4/5 routing must match the Heltec V3 reference. Isolate UART wires before independent power-off.',16,MUTED)]
save('radio-wiring.svg',s)

s=start(1500,1080,'MeshCrossway | Connector and GPIO reference','Left: numbering from the supplied listing photo. Verify orientation and signals; not Rev2 PDF J8 numbering.')
s+=box(40,120,810,880,'Waveshare | 32-pin listing illustration',BLUE)
left=['BAT - unused','GND','GNSS SDA / GPIO21','GNSS SCL / GPIO38','MT TX / GPIO39','MT RX / GPIO40','GPIO41','GPIO42','GPIO45','GPIO46','GPIO47','GPIO48','GPIO43 - reserved','GPIO44 - reserved','GND (used)','3.3 V -> GNSS']
right=['5V - unused','GND','USB D- / GPIO19','USB D+ / GPIO20','SD CLK / GPIO11','SD CMD / GPIO10','SD D0 / GPIO9','MC TX / GPIO17','MC RX / GPIO18','BOOT / GPIO0','RESET','PWR','Onboard SCL / GPIO7','Onboard SDA / GPIO8','GND','3.3 V (unused)']
used={5:GREEN,7:GREEN,9:BLUE,11:BLUE,16:PURPLE,18:PURPLE,29:GRAY,31:RED}
for row,(a,b) in enumerate(zip(left,right)):
    y=214+46*row
    for num,cx,label,lx,anch in [(row*2+1,405,a,380,'end'),(row*2+2,475,b,500,'start')]:
        c=used.get(num,'#cdd6e2')
        s += [f'<circle cx="{cx}" cy="{y-6}" r="16" fill="{c}"/>',text(cx,y-1,str(num),13,'white' if num in used else INK,True,'middle'),text(lx,y,label,17,used.get(num,INK),num in used,anch)]
s+=box(890,120,570,415,'Each radio | connect by GPIO label',PURPLE)
for i,label in enumerate(['GPIO4 / J3.15 = RX from U1','GPIO5 / J3.16 = TX to U1','GND / J3.1 or J2.1 = common ground','USB-C = independent power','GPIO43/44 = reserve for USB UART','3V3 / 5V = no jumper to U1','Other GPIOs = leave disconnected']):
    s.append(text(915,212+i*44,label,18))
s+=box(890,570,570,430,'Numbering correction',GRAY)
for i,line in enumerate(['MT = Meshtastic; MC = MeshCore.','U1 GPIO39: photo 9 / PDF J8.10','U1 GPIO40: photo 11 / PDF J8.12','U1 GPIO17: photo 16 / PDF J8.15','U1 GPIO18: photo 18 / PDF J8.17','U1 GPIO21: photo 5 / PDF J8.6','U1 GPIO38: photo 7 / PDF J8.8','Do not substitute one numbering','system for the other. Check the PCB.']):
    s.append(text(915,660+i*34,line,18))
s.append(text(40,1044,'Aideepen listing does not establish Heltec originality, exact routing or pre-soldered headers. No camera in this build.',18,MUTED))
save('connector-reference.svg',s)

s=start(1280,920,'MeshCrossway | GNSS and bench power','All positions below follow the supplied listing photo. GPIO names identify the electrical connections.')
s+=box(40,120,480,320,'Waveshare expansion connector',BLUE)
s+=box(770,120,470,320,'SparkFun SAM-M10Q | Qwiic',GREEN)
rows=[(225,'Photo 31 / 3V3','3.3 V / red',RED),(280,'Photo 29 / GND','GND / black',GRAY),(335,'Photo 5 / GPIO21','SDA / blue',BLUE),(390,'Photo 7 / GPIO38','SCL / yellow','#9a7800')]
for y,a,b,c in rows:
    s += [text(65,y,a,20,INK,True),text(795,y,b,20,INK,True),wire(520,y-7,770,y-7,c,False)]
s += [text(40,485,'Qwiic-to-male-jumper cable. Verify signal names and continuity; do not use only cable colors.',18),text(40,518,'Dedicated I2C bus: SDA21 / SCL38, initially 100 kHz. Default 7-bit address: 0x42.',18),text(40,551,'Keep onboard I2C on GPIO8/7. Verify the 3.3 V rail and its available load margin.',18)]
s+=box(40,590,1200,265,'Three independent USB power branches',GRAY)
for x,title,sub in [(70,'USB 5 V branch 1','Waveshare USB-C'),(465,'USB 5 V branch 2','LoRa V3 A USB-C'),(860,'USB 5 V branch 3','LoRa V3 B USB-C')]:
    s += [text(x,695,title,21,INK,True),wire(x+135,718,x+135,755,RED),text(x,790,sub,20)]
s += [text(65,833,'No batteries. Do not parallel board power outputs. All UART grounds are common.',17),text(40,892,'Power and signal proposal only; firmware, load margin and complete assembly remain untested.',17,MUTED)]
save('gnss-and-power.svg',s)

s=start(1280,780,'MeshCrossway | Standalone bench prototype','Waveshare touch controller + two LoRa V3 radios + u-blox GNSS. Proposed hardware; not a working product photograph.')
s+=box(40,120,520,380,'Waveshare ESP32-S3-Touch-LCD-3.5',BLUE)
s += [f'<rect x="75" y="195" width="450" height="160" rx="12" fill="{INK}"/>',text(105,240,'MeshCrossway',27,'white',True),text(105,282,'Meshtastic  |  MeshCore',22,'#91e4ef'),text(105,320,'Touch UI + queues + local history',19,'white'),text(75,402,'3.5 inch touch | 8 MB PSRAM | 16 MB flash',19),text(75,447,'Onboard microSD slot | independent USB power',18)]
for y,title,col,detail in [(120,'LoRa V3 A | Meshtastic',BLUE,'UART PROTO on configured RX4 / TX5'),(330,'LoRa V3 B | MeshCore',PURPLE,'Companion UART build: RX4 / TX5')]:
    s+=box(720,y,520,170,title,col)
    s += [text(745,y+91,'ESP32-S3 + SX1262 + OLED + USB-C',19),text(745,y+131,detail,18),wire(560,y+110,720,y+110,col)]
s+=box(40,550,520,150,'u-blox GNSS | SparkFun SAM-M10Q',GREEN)
s += [text(65,640,'Position + fix validity | Qwiic I2C | 3.3 V',19),wire(300,550,300,500,GREEN)]
s+=box(720,550,520,150,'Purchases and integration',GRAY)
s += [text(745,632,'One Aideepen two-pack supplies A and B.',18),text(745,666,'No separate radio base or IO adapters.',18),text(40,746,'The seller board revision, GPIO routing and header assembly must be checked. The UI simulator uses demo data.',17,MUTED)]
save('hardware-overview.svg',s)
print('Generated four SVG diagrams.')
