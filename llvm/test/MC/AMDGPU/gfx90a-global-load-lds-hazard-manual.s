// RUN: llvm-mc -triple=amdgcn-amd-amdhsa -mcpu=gfx90a -show-encoding %s 2>/dev/null | FileCheck --check-prefix=ASM %s
// RUN: llvm-mc -triple=amdgcn-amd-amdhsa -mcpu=gfx90a %s -o /dev/null 2>&1 | FileCheck --check-prefix=WARN %s

.amdgpu_gfx90a_direct_lds_hazard manual
.amdgpu_gfx90a_direct_lds_read v31
// WARN: warning: gfx90a direct-LDS read hazard is not resolved before its result is consumed; assembly is unchanged in manual mode
// WARN: warning: the marked direct-LDS read requiring a later DS operation is here
// ASM: ds_read_b32 v1, v2
// ASM-NEXT: v_add_u32_e32 v3, v1, v4
ds_read_b32 v1, v2
v_add_u32_e32 v3, v1, v4

// Manual mode emits no warning when a useful DS operation follows the read.
.amdgpu_gfx90a_direct_lds_read v31
// ASM: ds_read_b32 v5, v6
// ASM-NEXT: ds_read_b32 v7, v8
// ASM-NEXT: v_add_u32_e32 v9, v5, v7
ds_read_b32 v5, v6
ds_read_b32 v7, v8
v_add_u32_e32 v9, v5, v7

// Quiet manual mode preserves an unsafe sequence without another warning.
.amdgpu_gfx90a_direct_lds_hazard manual_no_warn
.amdgpu_gfx90a_direct_lds_read v31
// ASM: ds_read_b32 v10, v11
// ASM-NEXT: v_add_u32_e32 v12, v10, v13
ds_read_b32 v10, v11
v_add_u32_e32 v12, v10, v13
