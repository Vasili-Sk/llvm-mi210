; RUN: not llc -mtriple=amdgcn-amd-amdhsa -mcpu=gfx900 < %s 2>&1 | FileCheck %s
; RUN: not llc -mtriple=amdgcn-amd-amdhsa -mcpu=gfx900 -global-isel < %s 2>&1 | FileCheck %s
; RUN: not llc -mtriple=amdgcn-amd-amdhsa -mcpu=gfx942 < %s 2>&1 | FileCheck %s
; RUN: not llc -mtriple=amdgcn-amd-amdhsa -mcpu=gfx942 -global-isel < %s 2>&1 | FileCheck %s

target triple = "amdgcn-amd-amdhsa"
@lds = external addrspace(3) global [4096 x i8], align 16
declare void @llvm.amdgcn.global.load.lds.base(ptr addrspace(1), i32, ptr addrspace(3), i32 immarg, i32 immarg)

; CHECK: explicit global-to-LDS base contract requires gfx90a
define amdgpu_kernel void @wrong_target(ptr addrspace(1) inreg %base,
                                         i32 %lane_offset) {
  call void @llvm.amdgcn.global.load.lds.base(ptr addrspace(1) %base,
                                              i32 %lane_offset,
                                              ptr addrspace(3) @lds,
                                              i32 4, i32 0)
  ret void
}
