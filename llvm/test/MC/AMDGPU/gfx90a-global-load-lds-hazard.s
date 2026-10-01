// RUN: llvm-mc -triple=amdgcn-amd-amdhsa -mcpu=gfx90a -show-encoding %s | FileCheck --check-prefix=ASM %s
// RUN: llvm-mc -triple=amdgcn-amd-amdhsa -mcpu=gfx90a -filetype=obj %s -o - | llvm-objdump -d --mcpu=gfx90a - | FileCheck --check-prefix=DIS %s

// An isolated marked read is reissued into the allocated, dead scratch VGPR
// named by the marker. The repaired code uses the reissued result. This is the
// executed-confirmed gfx90a workaround.

// ASM: global_load_dwordx4 v2, s[4:5] lds
// ASM: ds_read_b32 v3, v4
// ASM-NEXT: ds_read_b32 v24, v4
// ASM-NEXT: v_mov_b32_e32 v3, v24
// ASM-NEXT: v_add_u32_e32 v5, v3, v6
// DIS: global_load_dwordx4 v2, s[4:5] lds
// DIS: ds_read_b32 v3, v4
// DIS-NEXT: ds_read_b32 v24, v4
// DIS-NEXT: v_mov_b32_e32 v3, v24
// DIS-NEXT: v_add_u32_e32 v5, v3, v6
global_load_dwordx4 v2, s[4:5] lds
s_waitcnt vmcnt(0)
.amdgpu_gfx90a_direct_lds_read v24
ds_read_b32 v3, v4
v_add_u32_e32 v5, v3, v6

// A useful later DS read satisfies the marked read. No repair is emitted.

// ASM: global_load_dwordx3 v2, s[4:5] lds
// ASM: ds_read_b32 v7, v8
// ASM-NEXT: ds_read_b32 v9, v10
// ASM-NOT: ds_read_b32 v7, v8
// ASM: v_add_u32_e32 v11, v7, v9
// DIS: global_load_dwordx3 v2, s[4:5] lds
// DIS: ds_read_b32 v7, v8
// DIS-NEXT: ds_read_b32 v9, v10
// DIS-NOT: ds_read_b32 v7, v8
// DIS: v_add_u32_e32 v11, v7, v9
global_load_dwordx3 v2, s[4:5] lds
s_waitcnt vmcnt(0)
.amdgpu_gfx90a_direct_lds_read v24
ds_read_b32 v7, v8
ds_read_b32 v9, v10
v_add_u32_e32 v11, v7, v9

// An unrelated read before the marked target read does not satisfy it.

// ASM: global_load_dwordx2 v2, s[4:5] lds
// ASM: ds_read_b32 v12, v14
// ASM-NEXT: ds_read_b32 v13, v15
// ASM-NEXT: ds_read_b32 v24, v15
// ASM-NEXT: v_mov_b32_e32 v13, v24
// ASM-NEXT: v_add_u32_e32 v16, v13, v17
// DIS: global_load_dwordx2 v2, s[4:5] lds
// DIS: ds_read_b32 v12, v14
// DIS-NEXT: ds_read_b32 v13, v15
// DIS-NEXT: ds_read_b32 v24, v15
// DIS-NEXT: v_mov_b32_e32 v13, v24
// DIS-NEXT: v_add_u32_e32 v16, v13, v17
global_load_dwordx2 v2, s[4:5] lds
ds_read_b32 v12, v14
.amdgpu_gfx90a_direct_lds_read v24
ds_read_b32 v13, v15
v_add_u32_e32 v16, v13, v17

// A branch closes a pending marked read before control flow changes.

// ASM: ds_read_b32 v18, v19
// ASM-NEXT: ds_read_b32 v24, v19
// ASM-NEXT: v_mov_b32_e32 v18, v24
// ASM-NEXT: s_branch
.amdgpu_gfx90a_direct_lds_read v24
ds_read_b32 v18, v19
s_branch .Lafter_branch
.Lafter_branch:

// An exec-mask change closes a pending marked read first.

// ASM: ds_read_b32 v20, v21
// ASM-NEXT: ds_read_b32 v24, v21
// ASM-NEXT: v_mov_b32_e32 v20, v24
// ASM-NEXT: s_mov_b64 exec, -1
.amdgpu_gfx90a_direct_lds_read v24
ds_read_b32 v20, v21
s_mov_b64 exec, -1

// A DS operation that consumes the marked result cannot protect that result.
// ASM: ds_read_b32 v25, v26
// ASM-NEXT: ds_read_b32 v24, v26
// ASM-NEXT: v_mov_b32_e32 v25, v24
// ASM-NEXT: ds_write_b32 v25, v27
.amdgpu_gfx90a_direct_lds_read v24
ds_read_b32 v25, v26
ds_write_b32 v25, v27

// A dead result overwritten before use needs no repair.

// ASM: ds_read_b32 v22, v23
// ASM-NEXT: v_mov_b32_e32 v22, 0
// ASM-NOT: ds_read_b32 v22, v23
.amdgpu_gfx90a_direct_lds_read v24
ds_read_b32 v22, v23
v_mov_b32_e32 v22, 0

// Overwriting the saved LDS address closes the read first.
// ASM: ds_read_b32 v28, v29
// ASM-NEXT: ds_read_b32 v24, v29
// ASM-NEXT: v_mov_b32_e32 v28, v24
// ASM-NEXT: v_mov_b32_e32 v29, 0
.amdgpu_gfx90a_direct_lds_read v24
ds_read_b32 v28, v29
v_mov_b32_e32 v29, 0

// A label closes a pending read before it creates a control-flow boundary.
// ASM: ds_read_b32 v30, v31
// ASM-NEXT: ds_read_b32 v24, v31
// ASM-NEXT: v_mov_b32_e32 v30, v24
.amdgpu_gfx90a_direct_lds_read v24
ds_read_b32 v30, v31
.Lafter_pending_read:

// End of input also closes a pending read.
// ASM: ds_read_b32 v32, v33
// ASM-NEXT: ds_read_b32 v24, v33
// ASM-NEXT: v_mov_b32_e32 v32, v24
.amdgpu_gfx90a_direct_lds_read v24
ds_read_b32 v32, v33
