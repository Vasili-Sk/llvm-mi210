#!/usr/bin/env python3
"""Count an AMDGPU symbol without counting alignment or linker padding."""

import argparse
import collections
import json
import re

SYMBOL_RE = re.compile(r"^([0-9A-Fa-f]+) <([^>]+)>:$")
INST_RE = re.compile(
    r"^\s*([.A-Za-z0-9_]+).*// ([0-9A-Fa-f]+): ((?:[0-9A-Fa-f]{8} ?)+)(?: <[^>]+>)?$"
)
ADDRESS_OPS = {
    "s_add_i32", "s_add_u32", "s_addc_u32", "s_lshl_b32", "s_lshl_b64",
    "v_add_u32_e32", "v_add_co_u32_e32", "v_add_co_u32_e64",
    "v_addc_co_u32_e32", "v_addc_co_u32_e64", "v_lshlrev_b32_e32",
    "v_lshlrev_b64", "v_lshl_or_b32", "v_mad_i64_i32", "v_mad_u64_u32",
}


def parse_region(text, symbol, first_offset=None, last_offset=None):
    lines = text.splitlines()
    header = next(
        ((i, int(match.group(1), 16))
         for i, line in enumerate(lines)
         if (match := SYMBOL_RE.match(line)) and match.group(2) == symbol),
        None,
    )
    if header is None:
        raise ValueError(f"symbol not found: {symbol}")
    start_line, base = header
    instructions = []
    found_end = False
    for line in lines[start_line + 1:]:
        if SYMBOL_RE.match(line):
            break
        match = INST_RE.match(line)
        if not match:
            continue
        opcode, address, words = match.groups()
        address = int(address, 16)
        instructions.append((address, opcode, len(words.split()), line.strip()))
        if opcode == "s_endpgm":
            found_end = True
            break
    if not found_end:
        raise ValueError(f"no inclusive s_endpgm for symbol: {symbol}")

    function_first = instructions[0][0]
    function_last = instructions[-1][0]
    if first_offset is not None or last_offset is not None:
        if first_offset is None or last_offset is None or first_offset > last_offset:
            raise ValueError("a region needs ordered first and last offsets")
        first = base + first_offset
        last = base + last_offset
        if first < function_first or last > function_last:
            raise ValueError("region is outside the symbol-through-s_endpgm range")
        instructions = [item for item in instructions if first <= item[0] <= last]
        if not instructions or instructions[0][0] != first or instructions[-1][0] != last:
            raise ValueError("region boundaries must name instruction addresses")

    opcodes = collections.Counter(item[1] for item in instructions)
    categories = collections.Counter()
    for _, opcode, _, line in instructions:
        if opcode.startswith("global_load_"):
            if " lds " in f" {line} ":
                categories["direct_load"] += 1
            else:
                categories["ordinary_global_load"] += 1
        if opcode.startswith("ds_write"):
            categories["ds_write"] += 1
        if opcode == "s_waitcnt":
            categories["wait"] += 1
        if opcode == "s_mov_b32" and re.search(r"^s_mov_b32\s+m0,", line):
            categories["m0_setup"] += 1
        if opcode in ADDRESS_OPS:
            categories["address_work"] += 1
        if opcode == "s_nop":
            categories["s_nop"] += 1

    return {
        "symbol": symbol,
        "region": (
            "symbol-through-inclusive-s_endpgm"
            if first_offset is None
            else f"{symbol}+0x{first_offset:x}..{symbol}+0x{last_offset:x}-inclusive"
        ),
        "instructions": len(instructions),
        "code_bytes": sum(item[2] * 4 for item in instructions),
        "categories": dict(sorted(categories.items())),
        "opcodes": dict(sorted(opcodes.items())),
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("disassembly")
    parser.add_argument("symbol")
    parser.add_argument("--first-offset", type=lambda value: int(value, 0))
    parser.add_argument("--last-offset", type=lambda value: int(value, 0))
    args = parser.parse_args()
    with open(args.disassembly, encoding="utf-8") as source:
        result = parse_region(source.read(), args.symbol, args.first_offset,
                              args.last_offset)
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
