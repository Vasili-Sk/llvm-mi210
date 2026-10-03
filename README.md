# LLVM for AMD CDNA2

This experimental AMD LLVM fork targets CDNA2 GPUs, with a focus on `gfx90a` and AMD Instinct MI210.
Its main addition supports direct HBM-to-LDS loads for matrix kernels.
These loads move data from global memory into shared LDS without a temporary VGPR payload or a separate `ds_write` instruction.

## Main target

- Architecture family: AMD CDNA2.
- LLVM target: `gfx90a`.
- Test GPU: AMD Instinct MI210.
- Wave size: 64 lanes.
- HBM capacity: 64 GB.
- LDS capacity: 64 KB per compute unit.

The scope is intentionally narrow.
Changes use explicit `gfx90a` checks and preserve normal code generation for other targets.

## Direct HBM-to-LDS loads

MI210 executes the following FLAT global-load forms:

| Form | Bytes per lane | First encoding word |
|---|---:|---:|
| `global_load_dword ... lds` | 4 | `0xDC50A000` |
| `global_load_dwordx2 ... lds` | 8 | `0xDC54A000` |
| `global_load_dwordx3 ... lds` | 12 | `0xDC58A000` |
| `global_load_dwordx4 ... lds` | 16 | `0xDC5CA000` |

Standard `gfx90a` toolchains do not expose all the x2, x3, and x4 forms, but MI210 executes them.
Wide loads distribute data through a hardware LDS permutation, so each lane payload does not occupy one contiguous block.

Correct use requires precise addressing and synchronization:

- Set `m0` to the correct LDS base.
- Wait on `vmcnt` before reading the new LDS data.
- Add a workgroup barrier before a different wave reads the data.
- Use a safe reissue sequence when an isolated first DS read has the direct-load hazard.

See [undocumented.md](undocumented.md) for detailed results:

- Exact x1, x2, x3, and x4 LDS placement.
- Narrow byte and 16-bit direct loads.
- VMEM wait rules.
- Direct-load to DS-read hazards.
- Cache-field behavior.
- MUBUF and FLAT differences.
- Other executed undocumented MI210 instructions.
- AGPR LDS operations.
- Unsupported barrier and queue operations.

## Performance against classic staging

Classic staging routes data through a VGPR and an explicit LDS write:

```text
HBM -> VGPR -> ds_write -> LDS
```

Direct LDS removes both intermediate steps:

```text
HBM -> LDS
```

The measured direct-LDS GEMM replaces the complete classic producer with four direct x4 loads per K-step.
Geometry, MFMA work, LDS size, and epilogue remain unchanged.
Both versions run from one binary on the same buffers and receive separate warm-ups before timing.

| GEMM shape | Best direct LDS | Best classic staging | Maximum boost |
|---|---:|---:|---:|
| `4096 x 4096 x 8192` | **135.79 TOPS** | 132.20 TOPS | **+2.72%** |
| `4096 x 4096 x 2048` | **134.31 TOPS** | 130.31 TOPS | **+3.06%** |
| `2048 x 2048 x 2048` | **99.15 TOPS** | 96.39 TOPS | **+2.86%** |

### Compute-path reference

Compute-path tests provide separate upper bounds:

| Compute path | Best result |
|---|---:|
| Pure in-register MFMA | **171.8 TOPS** |
| LDS-fed MFMA at 24 wavefronts per compute unit | **164.9 TOPS** |
| LDS-read and MFMA path without global loads or a barrier | **156.7 TOPS** |

These reference ceilings are not complete GEMMs.
They exclude the global-load producer path or other full-kernel work and do not contribute to the direct-LDS boost values.

Direct and classic kernels produced bit-exact results on the tested correctness shapes at `-O1`, `-O2`, and `-O3`.
Direct loading reduced the hot loop from 137 to 123 instructions per K-step.
It also removed all eight producer `ds_write` instructions and reduced wait instructions from 18 to three.
Both paths use 224 unified vector and accumulator registers with 16 KiB of LDS.

## Safety requirements

A direct-LDS transformation must prove the complete producer and consumer layout.
Required checks cover waits, barriers, aliases, queue lifetime, register use, and overwrite safety.
Partial producer rewrites are unsafe.
Kernels without direct-LDS operations must retain their original generated code.

## Source layout

The fork follows the standard LLVM monorepo layout.
Compiler sources are under `llvm/` and `clang/`.

## Build example

Use an AMDGPU-only configuration for a focused development build:

```bash
cmake -G Ninja -S llvm -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_PROJECTS=clang \
  -DLLVM_TARGETS_TO_BUILD=AMDGPU \
  -DLLVM_ENABLE_ASSERTIONS=ON

cmake --build build --target clang llc -j8
```
