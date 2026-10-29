#!/usr/bin/env python3
"""Keep chat/history static objects out of the SRAM needed by BLE and DMA."""
import argparse
import re
import subprocess

def check(symbols):
    objects=[]
    for line in symbols.splitlines():
        match=re.match(r'\s*\d+:\s+[0-9a-fA-F]+\s+(\d+)\s+OBJECT\s+\S+\s+\S+\s+\S+\s+(.+)',line)
        if not match:
            continue
        size,name=int(match[1]),match[2]
        if any(scope in name for scope in ('mcxhistory::','meshcrossway::mcui::','meshmt::','meshcrossway::mtui::')):
            objects.append((size,name))
    if not objects:
        raise ValueError('No chat/history objects found in unstripped ELF')
    oversized=[(size,name) for size,name in objects if size>1024]
    total=sum(size for size,name in objects)
    if oversized or total>2048:
        raise ValueError(f'Chat/history static RAM regression: total={total}, oversized={oversized}')
    return total

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('elf')
    args=parser.parse_args()
    try:
        total=check(subprocess.check_output(['readelf','--syms','--wide','--demangle',args.elf],text=True))
    except (ValueError,subprocess.CalledProcessError) as error:
        parser.exit(1,f'{error}\n')
    print(f'PASS: chat/history static objects total {total} bytes (budget 2048); bulk state uses PSRAM')

