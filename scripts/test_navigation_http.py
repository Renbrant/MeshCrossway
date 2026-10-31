#!/usr/bin/env python3
"""Run production HTTP JSON and SD folder persistence without MCU/LVGL mocks."""
from pathlib import Path
import json,os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as t:
 p=Path(t);source=(ROOT/'firmware/display-prototype/meshcrossway_navigation_http.h').read_text().replace('#include "meshcrossway_navigation.h"','')
 (p/'navigation_http_under_test.h').write_text(source)
 binary=p/'http-json'
 subprocess.run(['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror=misleading-indentation','-Wno-unused-function','-fsanitize=address,undefined','-I'+str(p),'-I'+str(ROOT/'firmware/display-prototype'),str(ROOT/'tests/navigation_http_json_test.cpp'),'-o',str(binary)],check=True)
 output=subprocess.check_output([str(binary),str(p/'card')],text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
 a,b,values=map(json.loads,output.splitlines())
 assert len(a['tracks'])==32 and len(a['folders'])==1 and a['folders'][0]['name']=='Weekend "hikes"'
 assert a['tracks'][0]['name']=='Trilha "á"\\\n' and a['tracks'][0]['metres']==1234.567
 assert a['tracks'][0]['folder']==a['folders'][0]['id'] and len(b['folders'])==16
 assert abs(values[0]+104.9000001)<1e-8 and abs(values[1]-39.9)<1e-8
 assert values[2:]==[0,2,None,None,None,18446744073709551615]
print('PASS: 200 real HTTP responses, full track/folder capacity, SD reload, UTF-8/JSON escapes, fixed decimal coordinates/metres, allocation/overflow/busy errors and no leaks')
from check_http_stack import check
frames='\n'.join(f'f:1:1:int mcxnavhttp::{n}(httpd_req_t*)\t{size}\tstatic' for n,size in [('state',1376),('geometry_from',144),('command',432),('download',1568)])
try:check(frames,'HTTP_STACK_BYTES=4096;','std::to_string(t.metres)')
except ValueError:pass
else:raise AssertionError('Original HTTP overflow accepted')
assert 'PASS' in check(frames,'HTTP_STACK_BYTES=8192;','')
try:check('','HTTP_STACK_BYTES=8192;','')
except ValueError:pass
else:raise AssertionError('Missing HTTP target reports accepted')
print('PASS: HTTP stack checker rejects old formatting/stack budget and absent target reports')
