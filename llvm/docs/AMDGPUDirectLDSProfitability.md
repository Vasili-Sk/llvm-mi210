# gfx90a direct-to-LDS profitability

This API plans a future transform. It does not change emitted code. A caller can use `evaluateDirectLDSProfitability` only after it has a complete `DirectLDSLayoutProof`.

## Input contract

The model supports gfx90a wave64. It supports direct widths of 4, 8, 12, and 16 bytes. It calls `DirectLDSLayout::create` and `matchDirectLDSLayout`. It does not copy layout formulas.

All costs use executed instruction units for one consistent region. The caller supplies these facts:

- classic and direct instructions per trip;
- trip count and one-time setup instructions;
- one-time `m0` and other setup instructions;
- direct-load count;
- removed global-load instruction count and byte count;
- removed DS writes;
- removed global and LDS address work;
- removed waits and other instructions;
- added direct loads, address work, waits, hazard repairs, `m0` setup, and other instructions;
- removed staging VGPRs;
- classic and direct VGPR use;
- classic and direct occupancy;
- a separate instruction cost for an occupancy loss.

The direct byte count is `direct loads * direct width`. It must equal the removed global-load byte count. One x4 direct load can replace four dword loads. The removed and added details must reconcile with the per-trip totals. The one-time setup details must reconcile with the setup total. An absent fact returns `unknown-fact`. Unsigned types exclude negative counts. Checked arithmetic rejects overflow.

The candidate must remove at least one DS write and one staging VGPR. A comparison between two already-direct forms fails this rule.

## Decision rule

```text
classic dynamic = classic instructions per trip * loop trips
direct dynamic  = direct instructions per trip * loop trips + setup instructions
saving          = classic dynamic - direct dynamic
```

The model accepts only when `saving > 0`. Equality rejects. This rule makes setup amortization explicit.

If occupancy decreases, the caller must provide a nonzero occupancy-loss cost. The saving must be greater than this cost. Equality rejects.

## Counting helper

Use `llvm/utils/amdgpu-direct-lds-count.py` on `llvm-objdump -d` output. By default, it starts at the exact function symbol. It stops at the inclusive `s_endpgm`. It excludes alignment and linker padding. Optional offsets select an executed loop region inside that checked function range. The helper reports code bytes, opcode counts, direct loads, ordinary global loads, DS writes, waits, `m0` setup, address work, and `s_nop` instructions.

The repair evidence is under `/home/vasilisk/scratch/direct-lds-profit-repair-evidence/`. These files are reproducible scratch evidence. They are not compiler inputs.

## Calibration 1: one-stage reduction

The functions are `block_reduce_classic` and `block_reduce_direct`. The region is each full symbol through its inclusive `s_endpgm`.

| fact | classic | direct |
|---|---:|---:|
| instructions | 87 | 87 |
| code bytes | 436 | 428 |
| ordinary global loads | 1 | 0 |
| direct x4 loads | 0 | 1 |
| DS writes | 9 | 7 |
| waits | 11 | 11 |
| `m0` setup | 0 | 1 |
| VGPRs | 7 | 6 |
| occupancy waves | 8 | 8 |

The direct path removes one ordinary x4 load and two `ds_write2_b32` instructions. It adds one direct x4 load. Its two one-time setup instructions are one `v_readfirstlane_b32` and one `s_mov_b32 m0`. Therefore:

```text
classic recurring = 87
direct recurring  = 85
one-time setup    = 2
classic dynamic   = 87
direct dynamic    = 85 + 2 = 87
saving            = 0
```

The model returns `not-profitable`. The one saved VGPR and eight saved code bytes do not override the lack of an executed-instruction saving.

Evidence:

- `reduction-classic.json`
- `reduction-direct.json`

## Calibration 2: committed tiled fixture

The accepted calibration is in spec-test commit `fc28b24e0bdfd0433618370c4116d5d418688106`.

Source files:

- `asm/micro/direct_lds_profitability/kernels.cu`;
- `asm/micro/direct_lds_profitability/runner.cpp`;
- `asm/micro/direct_lds_profitability/run.sh`;
- `asm/notes/direct-lds-profitability-fixture.md`.

Reproduce it with the final patched compiler:

```bash
cd ~/llama_build/spec-test
DIRECT_LDS_PROFIT_ID=direct-lds-profitability-fixture-final \
  asm/micro/direct_lds_profitability/run.sh --run
```

The script puts generated files under `~/scratch/direct-lds-profitability-fixture-final/work/`. It uses `~/mi210_lock` for the runtime gate.

The compared functions are `tiled_staged_x4` and `tiled_direct_x4`. They perform the same tiled load, LDS placement, four-word LDS consume, and deterministic ALU reduction. The staged function uses an ordinary compiler-visible x4 load and normal LDS stores. The direct function uses `__builtin_amdgcn_global_load_lds_base`. Neither function uses inline assembly or raw instruction words.

The equal-work hot regions are:

- staged: `tiled_staged_x4+0x78` through inclusive `+0xf8`;
- direct: `tiled_direct_x4+0x68` through inclusive `+0xd8`.

Each region processes one 16-byte payload per lane.

| Fact per trip | Staged | Direct |
|---|---:|---:|
| Instructions | 22 | 20 |
| Code bytes | 132 | 116 |
| Ordinary global x4 loads | 1 | 0 |
| Direct x4 loads | 0 | 1 |
| DS-write instructions | 2 | 0 |
| DS-read instructions | 2 | 2 |
| Waits | 3 | 3 |
| Barriers | 2 | 2 |
| Recurring `m0` setup | 0 | 0 |
| Hazard `s_nop` | 0 | 1 |
| Transform-specific global address instructions | 2 | 1 |

The direct `s_mov_b32 m0` is in the preheader at `tiled_direct_x4+0x64`. It executes once per invocation.

Complete symbol facts are:

| Complete fact | Staged | Direct |
|---|---:|---:|
| Instructions | 52 | 46 |
| Code bytes | 280 | 252 |
| VGPRs | 10 | 8 |
| SGPRs | 9 | 9 |
| Fixed LDS | 1024 bytes | 1024 bytes |
| Blocks per CU | 32 | 32 |
| Waves per EU | 8 | 8 |

The staged load places four payload words in four staging VGPRs. The direct load has no payload destination VGPR. Other live values make the complete net VGPR change 10 to 8.

The exact same-region equation is:

```text
removed = 1 ordinary global load
        + 2 DS-write instructions
        + 2 global address instructions
        = 5
added   = 1 direct load
        + 1 global address instruction
        + 1 hazard repair
        = 3
recurring saving = 5 - 3 = 2
one-time setup   = 1 m0 instruction
```

For `T` trips:

```text
classic dynamic = 22*T
direct dynamic  = 20*T + 1
saving          = 2*T - 1
```

One trip saves one instruction. Thirty-two trips save 63 instructions. Both cases return `profitable`.

The runtime gate covers 72 cases. It uses trip counts 1, 2, 7, and 32. It uses block counts 1 and 3. It uses three seeds and three repeats. Both kernels match the host oracle and each other:

```text
TOTAL values=9216 staged_wrong=0 direct_wrong=0 pair_wrong=0
```

This fixture is tiled. It is not a performance model for a full GEMM.

## Calibration 3: g12 limit

The functions are `gemm_staged_2w` and `gemm_direct_builtin_base_2w`. They perform the same two-wave GEMM and passed the same correctness oracle. The compared region is one K=64 hot cycle on each side:

- staged: `gemm_staged_2w+0x258` through inclusive `+0x4ec`;
- direct: `gemm_direct_builtin_base_2w+0x268` through inclusive `+0x4dc`.

| fact per K=64 cycle | staged | direct |
|---|---:|---:|
| instructions | 101 | 101 |
| ordinary global loads | 4 | 2 |
| direct x4 loads | 0 | 2 |
| DS writes | 8 | 4 |
| waits | 10 | 9 |
| `m0` setup | 0 | 2 |
| `s_nop` | 1 | 2 |
| VGPRs | 66 | 60 |
| occupancy waves | 2 | 2 |

The exact opcode subtraction has 13 removed and 13 added instructions:

```text
removed = 2 global loads + 4 DS writes + 1 wait + 6 exact other opcodes = 13
added   = 2 direct loads + 2 m0 setup + 1 hazard s_nop + 8 exact other opcodes = 13
saving  = 0
```

The model returns `not-profitable`. This is a valid staged-to-direct comparison. It is not an accepted static candidate. The six saved VGPRs do not override the instruction tie. The complete functions are 509 staged instructions and 521 direct instructions. Those complete totals are not mixed with hot-cycle facts.

The prior 537-to-521 local-pointer-versus-explicit comparison is not a staged-to-direct calibration. Both forms contain direct loads and remove zero DS writes. The model returns `no-removed-ds-writes` for that misuse. The same rule rejects the prior 69-to-63 raw-direct-versus-explicit g10 comparison.

Evidence:

- `g12-staged-k64.json`
- `g12-direct-k64.json`
- `g12-staged-complete.json`
- `g12-direct-complete.json`

## Limits

Only the committed small tiled fixture is a profitable staged-to-direct calibration. It is not a full GEMM. The verified g12 staged/direct hot cycle ties in executed instruction count and rejects. The model does not use timing to override this result.

The model does not prove tile meaning. It does not predict cache effects, bandwidth, or timing. It does not perform fusion, selection, lowering, scheduling, wait insertion, or hazard repair.
