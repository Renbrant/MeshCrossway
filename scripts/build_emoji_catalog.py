#!/usr/bin/env python3
"""Generate original flash-resident display sprites from a pinned OFL Noto font.
No third-party renderer source or artwork from the reference app is copied.
Generation needs Pillow/RAQM; normal firmware/CI never downloads assets.
"""
from pathlib import Path
import argparse,ast,base64,hashlib,json,re,unicodedata,urllib.request
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'firmware/display-prototype/emoji'
RESEARCH=ROOT/'scripts/emoji'
FONT_REF='22e564626297b4df0a40570ad81d6c05cc7c38bd'
INPUTS={
 'NotoColorEmoji.ttf':('https://raw.githubusercontent.com/googlefonts/noto-emoji/'+FONT_REF+'/fonts/NotoColorEmoji.ttf','39ee3c587e10e89669b9ff32703261d10d5f9c4dd5ad147b6b5a1c5200591817'),
 'emoji-sequences.txt':('https://www.unicode.org/Public/emoji/15.1/emoji-sequences.txt','eb72c9115e3504fbbe1c8621b619f879471a46ccc56e2f445417b7c1cad050d1'),
 'emoji-data.txt':('https://www.unicode.org/Public/15.1.0/ucd/emoji/emoji-data.txt','d7aef489c8fe4c14f09ea5695200277c6b93ac82ac60845cdd2161b0d6835cc1'),
}
def digest(b):return hashlib.sha256(b).hexdigest()
def normalize(s):return s.replace('\ufe0f','')
def registry():
 return [(ast.literal_eval('"'+s+'"'),int(p,16),d) for s,p,d in re.findall(r'\{"(.*?)", 0x([0-9A-F]+)u, "(.*?)"\}',(OUT/'emoji_registry.inc').read_text())]
def sequence(codes):return ''.join(chr(int(c.removeprefix('U+'),16)) for c in codes)
def legacy():
 return [(sequence(v['sequence']),int(v['pua'][2:],16),v['description']) for v in json.loads((ROOT/'tests/fixtures/emoji_legacy_registry.json').read_text())]
def legacy_sprites():
 text=(OUT/'emoji_assets_data.h').read_text();b=base64.b64decode(''.join(re.findall(r'^\s*"([A-Za-z0-9+/]+)"',text,re.M)))
 assert len(b)==92*264 and digest(b)=='2b1a3721dfb0a5c0f76c534fff071b1fe40f3b149b1a0e61d9a611d90c02fa5a'
 flag=base64.b64decode(re.search(r'kCheckeredFlagBase64\[\]\s*=\s*"([A-Za-z0-9+/]+)"',(OUT/'emoji_flag_asset.h').read_text())[1]);b+=flag
 out=[]
 for i in range(93):
  src=b[i*264:(i+1)*264];dst=bytearray(src[:64]+b'\0'*98)
  for y in range(14):
   sy=((2*y+1)*20)//28
   for x in range(14):
    sx=((2*x+1)*20)//28;v=src[64+sy*10+sx//2];idx=(v&15) if sx&1 else v>>4
    dst[64+y*7+x//2]|=idx if x&1 else idx<<4
  out.append(bytes(dst))
 return out

def audit(rows,sprites=None):
 ref=json.loads((RESEARCH/'reference_catalog.json').read_text());keys={normalize(s) for s,_,_ in rows};old={normalize(s) for s,_,_ in legacy()}
 visible=[v for v in ref['entries'] if v['visible']];missing=[v['codepoint'] for v in visible if normalize(sequence([v['codepoint']])) not in keys]
 before=[v['codepoint'] for v in visible if normalize(sequence([v['codepoint']])) not in old]
 report={'reference_commit':ref['commit'],'reference_source_sha256':ref['source_sha256'],'reference_declared_images':ref['declared_images'],'reference_codepoint_entries':len(ref['entries']),'reference_visible_mappings':len(visible),'legacy_glyphs':93,'current_glyphs':len(rows),'missing_visible_before':before,'missing_visible_after':missing,'zero_reference_mappings':[v['codepoint'] for v in ref['entries'] if not v['visible']],'flags':sum(len(s)==2 and all(0x1f1e6<=ord(c)<=0x1f1ff for c in s) for s,_,_ in rows),'tone_sequences':sum(any(0x1f3fb<=ord(c)<=0x1f3ff for c in s) for s,_,_ in rows),'zwj_sequences':sum('\u200d' in s for s,_,_ in rows),'sprite_bytes':len(rows)*162,'sprite_storage_bytes':len(rows)*164}
 if sprites is not None:report['sprite_sha256']=digest(b''.join(sprites))
 return report

def generate(cache):
 from PIL import Image,ImageDraw,ImageFont,features,__version__
 assert features.check_feature('raqm'),'RAQM shaping is required for flags/ZWJ'
 cache.mkdir(parents=True,exist_ok=True)
 for name,(url,sha) in INPUTS.items():
  p=cache/name
  if not p.exists():p.write_bytes(urllib.request.urlopen(url,timeout=60).read())
  assert digest(p.read_bytes())==sha, name+' input digest mismatch'
 rows=legacy();known={normalize(s) for s,_,_ in rows}
 def add(s,description=None):
  if normalize(s) in known:return
  known.add(normalize(s));rows.append((s,0xe000+len(rows),description or ' + '.join(unicodedata.name(c,f'U+{ord(c):04X}') for c in s if ord(c)!=0xfe0f)))
 ref=json.loads((RESEARCH/'reference_catalog.json').read_text())
 for v in ref['entries']:
  if v['visible']:add(sequence([v['codepoint']]))
 # Include every Unicode 15.1 RGI flag/tag flag and standard keycap sequence.
 for line in (cache/'emoji-sequences.txt').read_text().splitlines():
  fields=line.split('#')[0].split(';')
  if len(fields)>2 and fields[1].strip() in ('RGI_Emoji_Flag_Sequence','RGI_Emoji_Tag_Sequence','Emoji_Keycap_Sequence'):
   add(sequence(fields[0].split()),fields[2].strip())
 modifiers=set()
 for line in (cache/'emoji-data.txt').read_text().splitlines():
  fields=line.split('#')[0].split(';')
  if len(fields)>1 and fields[1].strip()=='Emoji_Modifier_Base':
   span=[int(v,16) for v in fields[0].strip().split('..')];modifiers.update(range(span[0],span[-1]+1))
 for s,_,_ in list(rows):
  base=normalize(s)
  if len(base)==1 and ord(base) in modifiers:
   for tone in range(0x1f3fb,0x1f400):add(base+chr(tone))
 for s in ['👨\u200d👩\u200d👧','👨\u200d👩\u200d👧\u200d👦','👩\u200d👩\u200d👦','👨\u200d👨\u200d👧','🏳️\u200d🌈','🏴\u200d☠️','🏳️\u200d⚧️','👩\u200d🚒','👨\u200d🚒','👩\u200d⚕️','👨\u200d⚕️','👩\u200d🔧','👨\u200d🔧','🧑\u200d💻','🧑\u200d🚒','🧑\u200d⚕️']:
  add(s)
 font=ImageFont.truetype(str(cache/'NotoColorEmoji.ttf'),109)
 sprites=legacy_sprites()
 for s,_,_ in rows[93:]:
  im=Image.new('RGBA',(320,160));ImageDraw.Draw(im).text((160,80),s,font=font,embedded_color=True,anchor='mm')
  box=im.getbbox();assert box,'Blank Noto glyph '+repr(s)
  glyph=im.crop(box);glyph.thumbnail((12,12),Image.Resampling.LANCZOS)
  small=Image.new('RGBA',(14,14));small.alpha_composite(glyph,((14-glyph.width)//2,(14-glyph.height)//2))
  q=small.quantize(colors=16,method=Image.Quantize.FASTOCTREE,dither=Image.Dither.NONE)
  pal=q.getpalette('RGBA');palette=bytearray()
  for i in range(16):
   r,g,b,a=pal[4*i:4*i+4];palette.extend((b,g,r,a))
  pix=q.tobytes();packed=bytes((pix[i]<<4)|pix[i+1] for i in range(0,196,2));sprites.append(bytes(palette)+packed)
 assert len(sprites)==len(rows)
 # Retain legacy lines, descriptions, PUA values and the original source assets.
 old=(OUT/'emoji_registry.inc').read_text().splitlines();old=old[:next(i for i,v in enumerate(old) if '0xE05Cu' in v)+1]
 def escaped(s):return ''.join(f'\\U{ord(c):08X}' for c in s)
 lines=old+['// v0.9.29 additions; append-only, never transmitted.']+[f'{{"{escaped(s)}", 0x{p:04X}u, {json.dumps(d)}}},' for s,p,d in rows[93:]]
 (OUT/'emoji_registry.inc').write_text('\n'.join(lines)+'\n')
 order=sorted(range(len(rows)),key=lambda i:normalize(rows[i][0]).encode())
 (OUT/'emoji_lookup_order.inc').write_text('// Unicode keys normalized only for FE0F; PUA registry order stays stable.\n'+','.join(map(str,order))+'\n')
 chunks=[]
 for chunk,start in enumerate(range(0,len(sprites),64)):
  name=f'emoji_sprites_{chunk:02d}.inc';chunks.append(name);b=b''.join(v+b'\0\0' for v in sprites[start:start+64])
  (OUT/name).write_text('// Generated indexed-4 Noto sprites; read-only flash bytes.\n'+''.join('"'+''.join(f'\\x{x:02x}' for x in b[i:i+24])+'"\n' for i in range(0,len(b),24))+'\n')
 header='#pragma once\n#include <stdint.h>\n#include <stddef.h>\nnamespace mcxemoji {\nstatic constexpr size_t kEmojiSpriteCount='+str(len(rows))+';\nstatic constexpr size_t kEmojiSpriteBytes=162;\nstatic constexpr size_t kEmojiSpriteStride=164;\n'
 for i,name in enumerate(chunks):header+=f'alignas(4) static const char kEmojiSpriteBlock{i}[]=\n#include "{name}"\n;\n'
 header+='static const char *const kEmojiSpriteChunks[]={'+','.join('kEmojiSpriteBlock'+str(i) for i in range(len(chunks)))+'};\ninline const uint8_t *sprite_data(size_t i){return i<kEmojiSpriteCount?reinterpret_cast<const uint8_t *>(kEmojiSpriteChunks[i/64])+i%64*kEmojiSpriteStride:nullptr;}\n}\n'
 (OUT/'emoji_sprite_data.h').write_text(header)

 report=audit(rows,sprites);report['generator']={'pillow':__version__,'freetype':features.version('freetype2'),'raqm':features.version_feature('raqm')};report['inputs']={n:{'url':url,'sha256':sha} for n,(url,sha) in INPUTS.items()}
 (RESEARCH/'coverage.json').write_text(json.dumps(report,indent=2)+'\n')
 body=['# Emoji coverage audit — v0.9.29','',f"Reference: `{ref['repository']}` at `{ref['commit']}`. Only Unicode mapping facts were extracted; no implementation or bitmap bytes are copied.",'',f"The reference declares 272 images but has {len(ref['entries'])} codepoint mappings: {len(ref['entries'])-len(report['zero_reference_mappings'])} visible entries and {len(report['zero_reference_mappings'])} transparent placeholders.",'',f"Legacy: 93 glyphs. New catalog: **{len(rows)} glyphs**, including {report['flags']} flag pairs, {report['tone_sequences']} tone sequences and {report['zwj_sequences']} ZWJ sequences. Missing visible reference mappings: **{len(report['missing_visible_after'])}**.",'',f"Legacy missing mappings: {len(report['missing_visible_before'])}. The original U+E000..U+E05C assignments and exact existing 14px pixels are retained.",'','Transparent reference entries: '+', '.join(report['zero_reference_mappings'])+'. ZWJ/VS are sequence controls; the zero-mapped transgender sign is supported through the composed flag. Standalone regional indicators do not identify a country; the visible reference U+1F1F1 is covered as that Unicode letter, not mislabeled as a country flag.','',f"All indexed-4 sprites occupy {report['sprite_bytes']:,} pixel/palette flash bytes; aligned storage is {report['sprite_storage_bytes']:,} bytes (+ one string terminator per 64-sprite block). Two padding bytes align each palette to four bytes. No full atlas or descriptor table is allocated in RAM. The old runtime atlas used 24,552 PSRAM bytes; the new shared display scratch uses at most 1,200 PSRAM bytes and one image font. Firmware-level flash/DRAM changes must be measured from the CI ELF, not inferred from this asset count.",'','## Reference mapping coverage','','| Unicode | Previously supported | Now supported |','|---|---|---|']
 before=set(report['missing_visible_before']);after=set(report['missing_visible_after'])
 body += [f"| {v['codepoint']} | {'No' if v['codepoint'] in before else 'Yes'} | {'No' if v['codepoint'] in after else 'Yes'} |" for v in ref['entries'] if v['visible']]
 body+=['','## Reproduction','','`python scripts/build_emoji_catalog.py --cache /path/to/input-cache` downloads only missing pinned build-time inputs, checks their SHA-256 and regenerates the append-only registry, sort order, sprites and report. Reproduce with the Pillow/FreeType/RAQM versions recorded in `scripts/emoji/coverage.json`. Normal CI verifies the committed bytes offline using `scripts/test_emoji.py`; no font/image fetch happens on the device.','','This is explicit catalog coverage, not a claim to render every Unicode emoji. Unlisted clusters, text-presentation variants, malformed UTF-8 and allocation failure have tested fallback behavior.']
 (ROOT/'docs/emoji-coverage-v0929.md').write_text('\n'.join(body)+'\n')
 print(json.dumps({k:report[k] for k in ['current_glyphs','reference_visible_mappings','missing_visible_after','flags','tone_sequences','zwj_sequences','sprite_bytes','sprite_sha256']}))
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--cache',type=Path,required=True);generate(p.parse_args().cache)
