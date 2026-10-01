// RUN: not llvm-mc -triple=amdgcn-amd-amdhsa -mcpu=gfx90a %s 2>&1 | FileCheck %s

.amdgpu_gfx90a_direct_lds_read v1
// CHECK: error: .amdgpu_gfx90a_direct_lds_read must be followed by a DS read instruction
v_mov_b32_e32 v0, 0
