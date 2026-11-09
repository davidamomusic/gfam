#!/usr/bin/env python3
"""Static worst-case cycle estimate of the synth_render inner loop (ARM7TDMI, IWRAM, 0 wait states).
usage: llvm-objdump -d --disassemble-symbols=synth_render build/gfam.elf | python3 test/cycles.py"""
import re, sys

rows = []
for l in sys.stdin:
    m = re.match(r'\s*([0-9a-f]+):\s+([0-9a-f]{8})\s+(\S+)?\s*(.*)', l)
    if m:
        rows.append((int(m.group(1), 16), int(m.group(2), 16), m.group(3) or '', m.group(4)))

def cost(enc, mn):
    if mn.startswith(('ldr', 'pop')): return 3
    if mn.startswith(('str', 'push')): return 2
    if (enc >> 22) & 0x3F == 0 and (enc >> 4) & 0xF == 9: return 4    # mul / mla
    if (enc >> 23) & 0x1F == 1 and (enc >> 4) & 0xF == 9: return 5    # smull / umull / smlal
    if mn.startswith('b') and not mn.startswith('bic'): return 3
    return 1

back = []
for a, e, mn, d in rows:
    if mn.startswith('b') and not mn.startswith('bic'):
        t = re.search(r'0x([0-9a-f]+)', d)
        if t and int(t.group(1), 16) < a:
            back.append((a, int(t.group(1), 16)))
last, start = max(back)
body = [(a, e, mn) for a, e, mn, d in rows if start <= a <= last]
cyc = sum(cost(e, mn) for a, e, mn in body)
per_frame = cyc * 308
print(f"inner loop: {len(body)} instructions, worst-case {cyc} cycles/sample")
print(f"x308 samples = {per_frame} of 280896 cycles/frame = {per_frame / 280896 * 100:.0f}% CPU for audio")
