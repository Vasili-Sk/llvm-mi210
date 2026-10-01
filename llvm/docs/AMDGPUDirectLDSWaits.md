# gfx90a direct-to-LDS waits

This note applies only to gfx90a. It covers `global_load_dword{,x2,x3,x4} ... lds`.

## Counter model

All four widths increment `vmcnt` once. The width does not change the counter step. `vmcnt` tracks the global read and its write into LDS. The instruction does not increment `lgkmcnt`.

Use `vmcnt(N)` before an operation that must see a pending direct write. Use the largest safe `N`. Later ordinary VMEM loads increase `N`. For example, one direct load followed by three ordinary loads needs `vmcnt(3)` to complete the direct load.

An aliasing DS read or DS write needs the direct load's `vmcnt` dependency. A workgroup barrier also needs this dependency when waves will use the produced LDS data. The barrier does not complete VMEM by itself.

A proven non-aliasing DS operation does not need the direct load's data dependency. An ordinary global load does not need it. A branch does not need it. `s_endpgm` does not need an inserted wait.

A DS read increments `lgkmcnt`. An ALU or MFMA instruction that consumes its result needs the matching `lgkmcnt` wait. This wait is separate from the earlier `vmcnt` wait. A DS write can also require `lgkmcnt(0)` before a barrier.

## First-read hazard

Data completion does not resolve the gfx90a first-read direct-LDS hazard. After the first aliasing DS read, issue one more useful DS operation before consuming the first result. If no useful operation is available, use the proven reread repair. Do not replace this rule with a stronger wait.

## Verified hot loops

The offsets below are relative to the linked function symbol. The source was unchanged at compiler commit `44d44e8a0`.

### g10 `gemm16_direct_builtin_base`

The hot cycle is `[+0xe8,+0x234]`.

| Wait offsets | Wait | Dependency |
|---|---|---|
| `+0x11c` | `vmcnt(0)` | Complete the x4 direct A load before the cross-wave barrier. |
| `+0x134`, `+0x174`, `+0x1bc`, `+0x200` | `vmcnt(0)` | Complete the ordinary B load before its DS write. |
| `+0x140`, `+0x180`, `+0x1c8`, `+0x20c` | `lgkmcnt(0)` | Complete the B DS write before the barrier. |
| `+0x158`, `+0x198`, `+0x1e0`, `+0x224` | `lgkmcnt(0)` | Complete the paired A and B DS reads before MFMA consumes them. |

The wait at `+0x11c` is the only hot g10 wait tied directly to direct-load completion.

### g12 `gemm_direct_builtin_base_2w`

The hot cycle is `[+0x268,+0x4dc]`.

| Wait offset | Wait | Dependency |
|---|---|---|
| `+0x2bc` | `lgkmcnt(3)` | Release the first ready DS-read operands for MFMA. |
| `+0x2f8` | `lgkmcnt(0)` | Release the last result from the first DS-read group. |
| `+0x344`, `+0x3a4` | `lgkmcnt(1)` | Release the older read in each two-read group. |
| `+0x358`, `+0x3b8` | `lgkmcnt(0)` | Release the newer read in each two-read group. |
| `+0x3fc` | `lgkmcnt(0)` | Complete the final DS reads before the barrier. |
| `+0x4b4` | `vmcnt(1)` | Complete both older direct A loads and the first ordinary B load before the first B DS writes. |
| `+0x4c8` | `vmcnt(0)` | Complete the second ordinary B load before the second B DS writes. |

The two direct loads are older than both ordinary B loads. Therefore, `vmcnt(1)` is the minimal threshold that completes both direct loads and the first B load.

## Limits

The compiler uses memory operands for alias decisions. It keeps a conservative wait when it cannot prove that a DS access is separate. The tests do not define a rule for another processor or instruction family.
