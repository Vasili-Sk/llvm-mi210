# Undocumented and under-documented MI210 instructions

This document describes instruction behavior on AMD Instinct MI210 GPUs.
The MI210 uses the `gfx90a` instruction set.

The main topic is a direct load from HBM into LDS.
This path can avoid a temporary vector register and an LDS write instruction.

This document also lists other instruction encodings that run on MI210 but have incomplete tool support or incomplete public documentation.
These extra instructions are not implemented by this compiler fork unless this document says otherwise.

## Terms

- **HBM** is the large external memory on the GPU card.
- **LDS** is fast memory shared by the threads in one workgroup.
- **VGPR** is a vector register. Each active lane has its own value.
- **SGPR** is a scalar register. A wave shares its value.
- **AGPR** is an accumulator register used mainly by matrix instructions.
- **Wave** means one group of 64 lanes on MI210.
- **Lane** means one thread inside a wave.
- **EXEC** is the mask that selects active lanes.
- **`m0`** is a special scalar register.
- **VMEM** means a vector memory operation.
- **DS** means an LDS operation.

## Evidence labels

- **Documented** means the AMD MI200 manual describes the operation.
- **Decode-confirmed** means the assembler or disassembler recognizes the encoding.
- **Executed-confirmed** means a test ran on MI210 and checked the result.
- **Compiler-implemented** means this compiler fork can generate or process the operation.
- **Not implemented** means this compiler fork has no automatic source transformation for the operation.
- **Rejected** means the gfx90a assembler rejects the source form.
- **Faults** means the raw encoding caused a GPU instruction fault in the test.

A decoded instruction is not proof that the hardware runs it correctly.
A completed kernel is not proof that the instruction wrote the expected value.
Executed-confirmed results in this document used output checks.

# 1. Direct HBM-to-LDS loads

## 1.1 What the instruction does

The FLAT global-memory form can load data from HBM and write it directly to LDS.
The data does not first enter a VGPR payload register.

A one-dword source form looks like this:

```asm
s_mov_b32 m0, lds_base
global_load_dword v_offset, s[global_base:global_base+1], lds
s_waitcnt vmcnt(0)
```

The operands have separate roles:

- `s[global_base:global_base+1]` holds the uniform 64-bit HBM base address.
- `v_offset` holds one HBM byte offset for each lane.
- `m0` selects the LDS destination region.
- The `lds` modifier requests the direct LDS destination.

The operation is executed-confirmed on MI210.

The HBM address is:

```text
uniform global base + per-lane byte offset
```

The LDS address does not use `v_offset`.
The LDS placement depends on `m0`, the lane number, the load width, and the payload word.

The instruction reads `m0` when it issues.
A later change to `m0` does not change an older outstanding load.
This behavior permits several direct loads to target different LDS regions at the same time.

Only the low 16 bits of `m0` select the LDS byte base.
Software must set `m0` before the load.
A missing `m0` setup can cause the write to disappear without a clear error.

## 1.2 Supported widths in this compiler fork

Upstream LLVM permits only the narrow gfx90a forms.
Upstream LLVM reserves the wide x2, x3, and x4 forms for newer targets.

This compiler fork enables the following gfx90a forms:

| Form | Bytes per lane | First encoding word | Status |
|---|---:|---:|---|
| `global_load_dword ... lds` | 4 | `0xDC50A000` | Executed-confirmed; upstream support exists |
| `global_load_dwordx2 ... lds` | 8 | `0xDC54A000` | Executed-confirmed; compiler-implemented here |
| `global_load_dwordx3 ... lds` | 12 | `0xDC58A000` | Executed-confirmed; compiler-implemented here |
| `global_load_dwordx4 ... lds` | 16 | `0xDC5CA000` | Executed-confirmed; compiler-implemented here |

Compiler-generated x2, x3, and x4 kernels passed MI210 result checks.

The width field is in bits 19 and 18 of the first instruction word.
The four values use the sequence `0x50`, `0x54`, `0x58`, and `0x5C` in that part of the word.

Do not use these words without correct operand fields.
The second instruction word selects registers and other operands.
A copied first word alone is not a complete instruction.

## 1.3 LDS placement

The wide forms do not place all words from one lane next to each other.
The hardware permutes the payload words across LDS.

Let:

- `L` be the lane number from 0 through 63.
- `j` be the payload word number.
- `w` be the LDS word offset from the wave-local `m0` base.

The placement rules are:

```text
x1: w = L

x2: w = 16 * (L >> 3) + (L & 7) + 8 * j

x3 and x4:
w = 16 * (L >> 2) + (L & 3) + 4 * bitswap2(j)

bitswap2(j) = ((j & 1) << 1) | (j >> 1)
```

The x1 footprint is 64 dwords, or 256 bytes, per wave.
The x2 footprint is 128 dwords, or 512 bytes, per wave.
The x4 footprint is 256 dwords, or 1,024 bytes, per wave.
The x3 form uses the x4 footprint but leaves 64 dword holes.

A consumer must use the inverse placement.
A normal linear LDS read is not automatically correct for x2, x3, or x4 data.

This compiler fork contains one shared placement model.
The legality checks and consumer checks use that model.
They do not use separate copied formulas.

## 1.4 Narrow loads

The following forms also execute on MI210:

| Form | First encoding word | LDS result per active lane |
|---|---:|---|
| `global_load_ubyte ... lds` | `0xDC40A000` | One zero-extended byte in one 32-bit LDS word |
| `global_load_sbyte ... lds` | `0xDC44A000` | One sign-extended byte in one 32-bit LDS word |
| `global_load_ushort ... lds` | `0xDC48A000` | One zero-extended 16-bit value in one 32-bit LDS word |
| `global_load_sshort ... lds` | `0xDC4CA000` | One sign-extended 16-bit value in one 32-bit LDS word |

Each active lane writes one complete 32-bit LDS word.
The narrow forms do not create a packed byte or packed 16-bit LDS tile.
Their destination is `m0 + 4 * lane`.

## 1.5 Wait rule

A direct global-to-LDS load is a VMEM operation on gfx90a.
Each direct load adds one `vmcnt` entry.
It does not add an `lgkmcnt` entry.

Use a VMEM wait before a lane reads the new LDS data:

```asm
s_waitcnt vmcnt(0)
```

An `lgkmcnt(0)` wait alone does not make the direct-load result ready.
Do not use `lgkmcnt` as a replacement for the required VMEM wait.

Use a workgroup barrier after the VMEM wait when another wave will read the data.
Cross-wave handoff passed tests with two and four waves.

An outstanding direct load can finish after a later DS write to the same address.
The direct load can then overwrite the later DS write.
Do not overwrite a destination while a direct load to that destination is active.

## 1.6 Direct-load to DS-read hazard

The MI210 has an additional hazard after a direct LDS load.
The first DS read result can be wrong if code consumes it too early.
Normal elapsed time does not fix this hazard.
Extra `s_nop` instructions do not fix it.
A barrier does not fix it.
A second wait does not fix it.

Useful later DS work can protect an earlier read.
For example, a pipeline can issue another live DS read before it consumes the first result.

An isolated read needs this executed-confirmed repair:

```asm
v_mov_b32_e32 v_scratch, v_addr
ds_read_b32 v_result, v_addr
ds_read_b32 v_scratch, v_scratch
v_mov_b32_e32 v_result, v_scratch
```

The repair has four requirements:

1. Save the address in a separate scratch VGPR.
2. Issue the first read.
3. Reissue the read into the distinct scratch VGPR.
4. Use the second result.

Two reads into the same destination did not work.
A dead scratch read did not work.
A read from another address before the affected read did not work.

This compiler fork models the hazard with LDS alias information.
It keeps useful DS pipelines when they already provide the required ordering.
It uses the isolated repair only for supported cases.
It reports an error when it cannot allocate a safe function-wide scratch VGPR.

Text assembly does not contain enough alias information for every runtime LDS address.
Text assembly therefore needs explicit direct-LDS read annotations.
A raw `.long` instruction is opaque to the semantic hazard pass.

The isolated repair is not yet proved for every DS read form.
Do not assume that the b32 repair also proves b64, b128, `ds_read2_b32`, or `ds_read2st64_b32` behavior.

## 1.7 Cache fields

GLC, SLC, and combined GLC+SLC direct loads execute correctly on MI210.
The modifiers must appear before `lds` and must not use commas.

Example:

```asm
global_load_dword v0, s[0:1], off glc slc lds
```

The tests proved correct values.
They did not prove a specific cache-bypass or coherence performance effect.
Benchmark the policy before using it for performance work.

## 1.8 MUBUF is a different path

Do not confuse the working FLAT form with the MUBUF form.

The following MUBUF spelling remains rejected on gfx90a:

```asm
buffer_load_dword ... lds
```

The x2, x3, and x4 MUBUF LDS pseudo-operations found in old LLVM source are not proof of MI210 hardware support.
Old VI-era bytes do not decode as these instructions on gfx90a.

Use the `global_load_* ... lds` FLAT path described above.

## 1.9 Current compiler limits

This fork implements wide instruction parsing, verification, selection, scheduling, wait tracking, and hazard handling.
It also provides an explicit-base source contract for compiler tests and controlled use.

The automatic ordinary-source fusion is conservative.
It currently supports only a fully proved gfx90a wave64 x4 layout.
It requires complete producer and consumer matching.
It rejects partial coverage, duplicate consumers, missing consumers, unknown aliases, dynamic LDS addresses, invalid stride, and unsupported resource facts.

The compiler has a diagnostic matcher for a larger CK `cfg_v2` queue.
That matcher proves four waves, six producer groups per stage, four stages, two live queue slots, and 6,144 mapped LDS words.
It does not generate code for that large queue.
The report keeps `transform_ready` false.

The real CK queue still needs one atomic all-producer rewrite.
It must rewrite every producer and every affected consumer together.
A producer-only change is unsafe.

# 2. Other executed undocumented instructions

The instructions in this section are not part of the direct-load compiler work.
Most have no useful compiler implementation in this fork.
Some only copy, clear, or preserve registers.
They are still listed because raw opcode experiments can otherwise produce misleading results.

## 2.1 Unassigned VOP3P opcode IDs

VOP3P is a packed vector and matrix instruction format.
LLVM assigns 65 VOP3P opcode IDs on gfx90a.
Tests executed all 63 remaining IDs.

Fifty-seven IDs faulted.
Six IDs completed:

| Opcode ID | First word with `vdst=v16` | Measured VGPR result |
|---:|---:|---|
| `0x43` | `0xD3C30010` | Clear one aligned 64-bit VGPR pair |
| `0x4B` | `0xD3CB0010` | Clear one aligned 64-bit VGPR pair |
| `0x53` | `0xD3D30010` | Clear one aligned 64-bit VGPR pair |
| `0x5A` | `0xD3DA0010` | Clear the selected 32-bit VGPR |
| `0x5B` | `0xD3DB0010` | Clear the selected 32-bit VGPR |
| `0x5C` | `0xD3DC0010` | Clear the selected 32-bit VGPR |

The 64-bit group ignores destination bit zero.
For example, destinations v16 and v17 both select v16:v17.
The 32-bit group clears the exact destination register.

The operations obey EXEC.
Inactive lanes keep their previous values.
Source data did not affect the result in the tested valid forms.

Five IDs have sparse matrix meanings on gfx950.
Those gfx950 meanings do not transfer to MI210.
Exact gfx950-style encodings still produced only the clear behavior on MI210.

AGPR behavior differs:

- IDs `0x43`, `0x4B`, and `0x53` clear an aligned AGPR pair.
- IDs `0x5A`, `0x5B`, and `0x5C` caused no observed AGPR write.

These are measured aliases.
They do not have approved MI210 instruction names.
This fork does not expose them as compiler operations.

## 2.2 Unassigned VOP3 opcode IDs

VOP3 is a three-operand vector instruction format.
The gfx90a opcode space has 896 IDs.
LLVM assigns 434 IDs.
Tests executed all 462 unassigned IDs with three register layouts.

Fifteen IDs completed:

```text
0x140 0x149 0x175 0x176 0x190 0x193 0x270 0x271
0x272 0x274 0x275 0x276 0x277 0x28E 0x29B
```

Measured behavior:

- `0x149` copies source 0 to the selected 32-bit destination.
- `0x190`, `0x193`, `0x272`, `0x274`, `0x275`, and `0x276` clear the selected 32-bit destination.
- `0x277` clears only the low 16 bits of the destination.
- `0x28E` performs a non-fused FP32 multiply-accumulate.
- `0x140`, `0x175`, `0x176`, and `0x29B` did not write the tested vector destination.
- `0x270` behaves like a dormant legacy `v_interp_p1_f32_e64` decode.
- `0x271` behaves like a dormant legacy `v_interp_p2_f32_e64` decode.

All observed vector writes followed EXEC.
The `0x28E` rounding matched documented `v_mac_f32` behavior.
It differed from fused `v_fmac_f32` behavior by one unit in the tested discriminator.

The interpolation operations did not expose useful parameter data in a HIP compute kernel.
All tested P10, P20, and P0 parameter reads returned zero.
Filling ordinary LDS did not activate the parameter store.

Modifier, wait, repetition, and ordered-pair tests did not enable another operation.
The surviving IDs do not provide a useful new compute path.
This fork does not expose them as compiler operations.

## 2.3 Unassigned VOP1 opcode IDs

VOP1 is a one-source vector instruction format.
LLVM assigns 79 of its 256 IDs on gfx90a.
Tests executed all 177 holes with VGPR and SGPR source layouts.

Four IDs completed:

```text
0x09 0x36 0x50 0x53
```

Measured behavior:

- `0x09` copies arbitrary source bits to active destination lanes.
- `0x36` preserves the tested destination.
- `0x53` preserves the tested destination.
- `0x50` performs a register-write operation that ignores EXEC.

Old GCN tables call `0x09` `V_MOV_FED_B32`.
The VOP3 ID `0x149` has the same measured copy behavior.

Old GCN tables call `0x50` `V_WRITELANE_REGWR_B32`.
The MI200 manual omits it.
LLVM rejects it for gfx90a.
MI210 executes it.

For source selectors s0 through s95, ID `0x50` writes the selected SGPR value to lane 0 of the destination VGPR.
Other lanes keep their previous values.

For inline integer selectors 0 through 63, ID `0x50` writes the following 32-bit instruction word to the selected lane.
The following instruction also executes.
The result does not depend on EXEC.
The result did not depend on `m0` in the tests.

Reserved selectors produced fixed or state-derived values in some cases.
Their meanings are not assigned.
Do not use those selector forms in production code.

This fork does not expose these VOP1 IDs as compiler operations.

## 2.4 VOP2 has no free opcode ID

VOP2 is a two-source vector instruction format.
It has 63 independent IDs from `0x00` through `0x3E`.
LLVM assigns all 63 IDs on gfx90a.
ID `0x3F` starts a VOP1 encoding and is not a free VOP2 ID.

Tests also checked 25 unusual source selectors with assigned XOR and addition operations.
All 25 selectors supplied visible zero in those tests.
Selector `0x0FE`, named `src_lds_direct` by LLVM, did not expose initialized LDS data in this compute setup.
SDWA selector `0x0F9` and DPP selector `0x0FA` are extension prefixes.
They are not free source values.

There is no unassigned VOP2 opcode to implement.

## 2.5 Unassigned SOP2 opcode IDs

SOP2 is a two-source scalar instruction format.
It has 64 IDs.
LLVM assigns 53 IDs on gfx90a.

The 11 holes are `0x35` through `0x3F`.
Every hole faulted in an execution test.
A known-good scalar operation passed after each fault.

There is no surviving SOP2 operation to implement.

## 2.6 Unassigned SOP1 opcode IDs

SOP1 is a one-source scalar instruction format.
LLVM assigns 54 of its 256 IDs on gfx90a.
A safe bounded test executed 33 selected holes.

Two IDs completed:

- `0x2F` preserved all visible scalar state in the tested cases.
- `0x31` copied one selected 32-bit scalar source to the destination.

The other 31 tested IDs faulted.
The scalar floating-point and conversion range `0x60` through `0x6E` did not execute.

The remaining holes were not raw-executed when adjacent architectures assigned unsafe control, message, barrier, register-allocation, or sleep meanings.

Do not assign an official mnemonic to `0x2F` or `0x31` from this evidence.
ID `0x31` gives no known benefit over `s_mov_b32`.
This fork does not expose either ID.

# 3. Under-documented AGPR paths

## 3.1 DS operations can use AGPRs

Basic LDS reads and writes can use AGPR data and destination operands on MI210.
Executed widths include 8, 16, 32, 64, 96, and 128 bits.

Examples:

```asm
ds_write_b32 v1, a0
ds_read_b32 a8, v1
ds_write_b128 v1, a[0:3]
ds_read_b128 a[8:11], v1
```

These forms can remove a VGPR copy around LDS.
They still use a DS instruction.
They still need the normal `lgkmcnt` wait for DS data.

The following AGPR paths also passed output checks:

- Returning 32-bit addition and subtraction.
- Returning signed minimum and unsigned maximum.
- Returning AND, OR, and XOR.
- Matching and nonmatching 32-bit compare-and-store.
- Returning 64-bit addition with aligned AGPR pairs.
- Matching and nonmatching 64-bit compare-and-store.
- `ds_write2_b32` and `ds_read2_b32`.

Returning operations wrote the old LDS value directly to AGPRs.
Wider AGPR tuples still need normal register alignment.

The `.amdhsa_accum_offset` field uses units of four VGPRs.
For example, code that reserves 64 VGPRs must use an accumulator offset of 16.

This compiler fork does not add a new automatic transformation for these AGPR forms.

## 3.2 `ds_append` and `ds_consume` with AGPR destinations

`ds_append a5` and `ds_consume a5` execute on MI210.
The LDS counter changes by the number of active lanes in EXEC.
Every active lane receives the same old counter value.

Software needs a lane-prefix operation such as `mbcnt` when each lane needs a unique index.

These instructions can support a wave-level reservation counter.
They do not replace a complete producer and consumer queue protocol.
This fork does not use them for the direct-LDS queue.

## 3.3 Direct SGPR-to-AGPR write

The following path works on gfx90a:

```asm
v_accvgpr_write_b32 a0, s4
```

It moves one shared scalar value directly into an accumulator register for active lanes.
It can avoid an SGPR-to-VGPR-to-AGPR sequence.

Tests checked direct writes and a following matrix instruction.
This path is useful for accumulator initialization and shared constants.
LLVM already has target support for this operation.
This fork adds no new optimization for it.

The reverse AGPR-to-VGPR path uses `v_accvgpr_read_b32`.
Do not treat it as a cheap spill path.
Existing performance reports show a large cost when register limits force matrix accumulators through this path.

# 4. Synchronization and queue limits

## 4.1 Split barriers are not available

The gfx90a assembler rejects:

```asm
s_barrier_signal
s_barrier_wait
```

These split-barrier forms exist on gfx908.
MI210 software must use the full `s_barrier` for normal workgroup synchronization.

A compiler cannot safely schedule gfx908 split barriers on gfx90a.
Reduce barrier count by changing the tile or queue design instead.

## 4.2 GWS operations remain unproved here

LLVM exposes global-wave synchronization instructions such as:

- `ds_gws_init`
- `ds_gws_barrier`
- `ds_gws_sema_br`

The gfx90a validation requires an even-aligned data register for relevant VGPR and AGPR forms.
The MI210 tests for this project did not execute these operations.

Do not use them as a production cross-workgroup synchronization path without a separate correctness and recovery test.

## 4.3 Ordered count is unavailable

The gfx90a assembler rejects `ds_ordered_count`.
No raw execution result proves that it is safe on MI210.
This fork does not use it.

## 4.4 `lds_direct` is not direct global-to-LDS

LLVM rejects the newer `lds_direct` vector source mechanism on gfx90a.
For example, do not expect this newer-architecture source form to work:

```asm
v_add_f32 ..., lds_direct
```

This mechanism is different from `global_load_* ... lds`.
The direct HBM-to-LDS path in section 1 does not enable `lds_direct` vector operands.

# 5. Safety rules for raw instruction tests

Raw instruction tests can fault a GPU queue.
Some control-flow or synchronization encodings can hang more than one wave.
Use isolated processes and a known-good recovery control.

Use this sequence:

1. Check the official MI200 manual.
2. Check the LLVM gfx90a decoder and instruction tables.
3. Assemble and disassemble the candidate when possible.
4. Use a raw word only when the normal assembler cannot express the candidate.
5. Put one candidate in one small kernel.
6. Run the kernel under the MI210 GPU lock.
7. Check exact output values.
8. Run a known-good kernel after a fault.
9. Record whether the result was decoded, executed, correct, rejected, or faulting.

Never infer a useful operation only because a kernel completed.
A no-write instruction can look successful when the destination already contains the expected value.
Poison destinations before the candidate instruction.
Use several source patterns and EXEC masks.

# 6. Summary

The most useful undocumented MI210 path is the wide FLAT direct HBM-to-LDS load.
The x2, x3, and x4 forms execute correctly on MI210.
This compiler fork implements their basic compiler and assembler support.

The path has strict limits:

- The LDS placement is width-specific and permuted.
- The code must set `m0`.
- The code must wait on `vmcnt`.
- Cross-wave consumers need a barrier after the VMEM wait.
- Same-address overwrite while the load is active is unsafe.
- The first DS read has a special result hazard.
- Automatic source fusion needs complete producer, consumer, alias, resource, and queue proofs.
- The large CK queue is diagnostic-only today.

Most other unassigned opcode survivors only copy, clear, preserve, or fault.
They do not provide hidden sparse matrix hardware on MI210.
The AGPR DS paths and direct SGPR-to-AGPR write are useful, but this fork does not yet optimize ordinary source to use them.
