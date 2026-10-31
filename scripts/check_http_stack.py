#!/usr/bin/env python3
"""Conservative HTTP handler budget; SDK/FATFS/newlib calls need runtime testing."""
import argparse
from pathlib import Path
import re

def check(reports, health, navigation):
    match=re.search(r'HTTP_STACK_BYTES\s*=\s*(\d+)',health)
    if not match:raise ValueError('Missing HTTP task stack budget')
    if 'std::to_string(' in navigation or re.search(r'"[^"\n]*%[-+ #0-9.*]*[fFgGeE][^"\n]*"',navigation):
        raise ValueError('Unbounded/floating libc formatting in navigation HTTP')
    handlers=[];helpers=[];seen=set()
    for line in reports.splitlines():
        fields=line.split('\t')
        if len(fields)!=3:continue
        name=fields[0]
        if not any(n in name for n in ('mcxnavhttp::','mcxremoteradio::','mcxremotechat::','meshhealth::','meshcapture::')):continue
        if 'httpd_req_t' in name:
            if fields[2] not in ('static','dynamic,bounded'):raise ValueError('Unbounded HTTP handler: '+name)
            handlers.append(int(fields[1]));seen.add(name)
        elif any(n in name for n in ('mcxnavhttp::Json::','mcxnavhttp::quote(','mcxremotechat::token(')):
            if fields[2] not in ('static','dynamic,bounded'):raise ValueError('Unbounded HTTP helper: '+name)
            helpers.append(int(fields[1]))
    for needed in ('mcxnavhttp::state(','mcxnavhttp::geometry_from(','mcxnavhttp::command(','mcxnavhttp::download('):
        if not any(needed in name for name in seen):raise ValueError('Missing target HTTP frame '+needed)
    largest=max(handlers)
    required=largest+sum(sorted(helpers,reverse=True)[:3])+4096+768
    budget=int(match[1])
    if largest>2048 or required>budget:raise ValueError(f'HTTP stack headroom failed: reserve={required}, task={budget}, largest handler={largest}')
    return f'PASS: HTTP stack reserve {required}/{budget} bytes (4096 SDK/FATFS/newlib + 768 parser reserve); largest handler={largest}'

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('build',type=Path);p.add_argument('--health',type=Path,required=True);p.add_argument('--navigation',type=Path,required=True);a=p.parse_args()
    try:print(check('\n'.join(f.read_text() for f in a.build.rglob('*.su')),a.health.read_text(),a.navigation.read_text()))
    except ValueError as e:p.exit(1,str(e)+'\n')
