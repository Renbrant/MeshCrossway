#!/usr/bin/env python3
"""Keep chat/history static objects out of the SRAM needed by BLE and DMA."""
import argparse
import re
import subprocess

def check(symbols, writable_sections=None):
    objects=[]
    for line in symbols.splitlines():
        match=re.match(r'\s*\d+:\s+[0-9a-fA-F]+\s+(\d+)\s+OBJECT\s+\S+\s+\S+\s+(\S+)\s+(.+)',line)
        if not match:
            continue
        size,section,name=int(match[1]),match[2],match[3]
        if writable_sections is not None and section not in writable_sections:
            continue
        if any(scope in name for scope in ('mcxhistory::','meshcrossway::mcui::','meshmt::','meshcrossway::mtui::')):
            objects.append((size,name))
    if not objects:
        raise ValueError('No chat/history objects found in unstripped ELF')
    oversized=[(size,name) for size,name in objects if size>1024]
    total=sum(size for size,name in objects)
    if oversized or total>2048:
        raise ValueError(f'Chat/history static RAM regression: total={total}, oversized={oversized}')
    return total

def writable_ram_sections(sections):
    writable=set()
    for line in sections.splitlines():
        row=re.match(r'\s*\[\s*(\d+)\]\s+(\S+)\s+\S+\s+\S+\s+\S+\s+\S+\s+\S+\s+(\S+)',line)
        # ESP-IDF's linker marks mapped flash rodata WA despite its physical
        # placement in flash. ELF flags alone therefore overcount SRAM.
        if row and 'W' in row[3] and 'A' in row[3] and not row[2].startswith('.flash'):
            writable.add(row[1])
    if not writable:raise ValueError('Cannot identify writable allocated RAM sections')
    return writable

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('elf')
    args=parser.parse_args()
    try:
        sections=subprocess.check_output(['readelf','-SW',args.elf],text=True)
        writable=writable_ram_sections(sections)
        total=check(subprocess.check_output(['readelf','--syms','--wide','--demangle',args.elf],text=True),writable)
    except (ValueError,subprocess.CalledProcessError) as error:
        parser.exit(1,f'{error}\n')
    print(f'PASS: chat/history static objects total {total} bytes (budget 2048); bulk state uses PSRAM')

