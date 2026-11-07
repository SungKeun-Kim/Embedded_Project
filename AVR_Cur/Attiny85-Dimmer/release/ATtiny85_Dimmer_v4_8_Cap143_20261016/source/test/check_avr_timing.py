"""avr-objdump 출력에서 Timer1 ISR 및 원자 구간의 최장 경로를 보수적으로 계산한다.

사용: python test/check_avr_timing.py .pio/build/attiny85/firmware.disasm
분석 대상에 루프/호출/미지원 명령이 생기면 실패하여 수동 검토를 요구한다.
"""
import functools
import re
import sys

source = open(sys.argv[1], encoding="utf-8-sig").read()
instructions = {}
for line in source.splitlines():
    m = re.match(r"\s*([0-9a-f]+):\s+((?:[0-9a-f]{2}\s+)+)\s*(\w+)\s*(.*)", line)
    if m:
        address, raw, op, args = m.groups()
        instructions[int(address, 16)] = (len(raw.split()), op, args)

one = set("in out ldi mov movw and andi or ori eor cp cpc cpi sub subi sbc sbci add adc inc dec lsl lsr asr rol ror swap com neg nop cli sei clr tst ser".split())
two = set("push pop lds sts ld st ldd std adiw sbiw sbi cbi mul".split())


def longest(start, atomic=False):
    visiting = set()

    @functools.lru_cache(None)
    def walk(addr):
        if addr in visiting:
            raise AssertionError(f"Loop at {addr:x}: manual review required")
        visiting.add(addr)
        size, op, args = instructions[addr]
        nxt = addr + size
        if atomic and op == "out" and args.startswith("0x3f,"):
            result = 1
        elif op in ("ret", "reti"):
            assert not atomic, "Atomic block exits without SREG restore"
            result = 4
        elif op in ("rjmp", "jmp") or op.startswith("br"):
            target = int(re.search(r";\s*0x([0-9a-f]+)", args)[1], 16)
            if op.startswith("br"):
                result = max(1 + walk(nxt), 2 + walk(target))
            else:
                result = (2 if op == "rjmp" else 3) + walk(target)
        elif op in ("cpse", "sbrc", "sbrs", "sbic", "sbis"):
            skip_size = instructions[nxt][0]
            result = max(1 + walk(nxt), 1 + skip_size // 2 + walk(nxt + skip_size))
        else:
            assert op in one | two, f"Unsupported instruction {op} at {addr:x}"
            result = (2 if op in two else 1) + walk(nxt)
        visiting.remove(addr)
        return result

    return walk(start)


entry = int(re.search(r"^([0-9a-f]+) <__vector_3>:", source, re.M)[1], 16)
isr = longest(entry)
atomic_lengths = []
previous = None
for address, (_, op, args) in sorted(instructions.items()):
    if op == "cli" and previous and previous[1] == "in" and "0x3f" in previous[2]:
        atomic_lengths.append(longest(address, atomic=True))
    previous = (address, op, args)
blocked = max(atomic_lengths)
# ISR 진입 4 + 벡터 RJMP 2 + 실행 중 최장 명령 잔여 4클럭을 추가한다.
total = isr + blocked + 10
print(f"ISR body <= {isr} cycles; atomic block <= {blocked} cycles")
print(f"Conservative combined bound <= {total}/200 cycles ({total / 8:.3f} us @ 8MHz)")
assert total < 200, "25us timer budget exceeded"
print("PASS: Timer1 ISR cycle budget")
