"""IAR AVR 목록의 알려진 호출 경로와 ISR 1회의 스택 예산을 검사한다.

튜닝 측정 콜백을 명시적으로 연결한다. 실물 최대 사용량 측정을 대신하지 않는다.
"""
from pathlib import Path
import argparse
import re
import sys

parser = argparse.ArgumentParser()
parser.add_argument('listing_dir', type=Path)
parser.add_argument('--margin', type=int, default=16)
args = parser.parse_args()
functions = {}
for listing in args.listing_dir.glob('*.lst'):
    content = listing.read_bytes().decode('latin1').replace('\x00', '')
    if 'Maximum stack usage in bytes:' not in content:
        continue
    table = content.split('Maximum stack usage in bytes:', 1)[1].split('Segment part sizes:', 1)[0]
    current = None
    pending = ''
    for line in table.splitlines():
        if re.fullmatch(r'\s*\d+\s+\d+\s*',line) and pending:
            line = pending + ' ' + line.strip()
        row = re.fullmatch(r'\s*(->\s*)?(Indirect call|\S+)\s+(\d+)\s+(\d+)\s*', line)
        if not row:
            pending = line.strip()
            continue
        pending = ''
        arrow, name, cstack, rstack = row.groups()
        cost = (int(cstack), int(rstack))
        if arrow and current:
            if name == 'Indirect call':
                if current not in ('phase_find_minimum','phase_track_once'):
                    raise RuntimeError('Unknown indirect call from ' + current)
                name = 'measure_code'
            functions[current]['calls'].append((name, cost))
        elif not arrow:
            current = name
            functions[name] = {'local': cost, 'calls': []}

maps = list(args.listing_dir.glob('*.map'))
if len(maps) != 1:
    raise RuntimeError('Exactly one linker map is required')
map_text = maps[0].read_bytes().decode('latin1')
library_leaves = set()

def peak(name, kind, trail=()):
    if name in trail:
        raise RuntimeError(f'Recursive call path: {trail} -> {name}')
    if name not in functions:
        # IAR memset 등의 말단 라이브러리는 링커의 스택 기재값으로 확장한다.
        leaf = re.search(r'^\s+' + re.escape(name) + r'\s+[0-9A-F]{8}[^\n]*\n'
            r'\s+stack 1 = [0-9A-F]+ \( ([0-9A-F]+) \)\s*\n'
            r'\s+stack 2 = [0-9A-F]+ \( ([0-9A-F]+) \)',map_text,re.M)
        if not leaf:
            raise RuntimeError('Unresolved stack path: ' + name)
        library_leaves.add(name)
        return int(leaf[kind+1],16), [name + ' [linker]']
    item = functions[name]
    best = item['local'][kind], [name]
    for callee, cost in item['calls']:
        usage, path = peak(callee, kind, trail + (name,))
        candidate = cost[kind] + usage
        if candidate > best[0]:
            best = candidate, [name] + path
    return best

is_mega8 = 'Timer0_OverFlow' in functions
interrupts = (['Timer0_OverFlow','USART_RX_Complet','int1_interrupt'] if is_mega8 else
              ['TIMER0_COMP_interrupt','USART_RX_Complet','int0_interrupt'])
if 'main' not in functions or any(name not in functions for name in interrupts):
    raise RuntimeError('Main/enabled ISR stack tables are missing')

passed = True
for kind, segment in enumerate(('CSTACK', 'RSTACK')):
    match = re.search(r'^' + segment + r'\s+DATA\s+[0-9A-F]+\s+-\s+[0-9A-F]+\s+([0-9A-F]+)\s', map_text, re.M)
    if not match:
        raise RuntimeError(f'{segment} reservation not found')
    reserved = int(match[1], 16)
    normal, path = peak('main', kind)
    irq, irq_path = max((peak(name,kind) for name in interrupts), key=lambda x: x[0])
    required = normal + irq
    okay = required + args.margin <= reserved
    passed &= okay
    print(f'{segment}: normal={normal}, ISR={irq}, known path={required}, '
          f'margin={args.margin}, reserved={reserved}: {"PASS" if okay else "FAIL"}')
    print('  ' + ' -> '.join(path) + ' + ISR ' + ' -> '.join(irq_path))
if library_leaves:
    print('Library leaves expanded from linker map: ' + ', '.join(sorted(library_leaves)))
sys.exit(0 if passed else 1)
