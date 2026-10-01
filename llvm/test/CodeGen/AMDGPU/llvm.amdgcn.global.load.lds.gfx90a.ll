; RUN: llc -global-isel=0 -mtriple=amdgpu9.0a < %s | FileCheck --check-prefix=COMMON %s
; RUN: llc -global-isel=1 -mtriple=amdgpu9.0a < %s | FileCheck --check-prefix=COMMON %s

; gfx90a executes the unpublished x2, x3, and x4 direct global-to-LDS forms.
; Its first post-DMA DS read also needs one later DS operation before use.

declare void @llvm.amdgcn.global.load.lds(ptr addrspace(1) nocapture, ptr addrspace(3) nocapture, i32, i32, i32)

@lds = internal addrspace(3) global [256 x i32] poison, align 1024

define amdgpu_kernel void @wide(ptr addrspace(1) inreg %g, i32 %addr) {
; COMMON-LABEL: wide:
; COMMON: global_load_dwordx2
; COMMON: global_load_dwordx3
; COMMON: global_load_dwordx4
  %p = getelementptr i8, ptr addrspace(3) @lds, i32 %addr
  call void @llvm.amdgcn.global.load.lds(ptr addrspace(1) %g, ptr addrspace(3) %p, i32 8, i32 0, i32 0)
  call void @llvm.amdgcn.global.load.lds(ptr addrspace(1) %g, ptr addrspace(3) %p, i32 12, i32 0, i32 0)
  call void @llvm.amdgcn.global.load.lds(ptr addrspace(1) %g, ptr addrspace(3) %p, i32 16, i32 0, i32 0)
  ret void
}

define amdgpu_kernel void @single_read(ptr addrspace(1) inreg %g, ptr addrspace(1) inreg %out, i32 %idx) {
; COMMON-LABEL: single_read:
; COMMON: global_load_dword
; COMMON: v_mov_b32_e32 [[DUMMY:v[0-9]+]], [[ADDR:v[0-9]+]]
; COMMON: ds_read_b32 [[DST:v[0-9]+]], [[ADDR]]
; COMMON: ds_read_b32 [[DUMMY]], [[DUMMY]]
; COMMON: v_mov_b32_e32 [[DST]], [[DUMMY]]
; COMMON: global_store_dword {{.*}}[[DST]]
  %lp = getelementptr i32, ptr addrspace(3) @lds, i32 %idx
  call void @llvm.amdgcn.global.load.lds(ptr addrspace(1) %g, ptr addrspace(3) %lp, i32 4, i32 0, i32 0)
  %v = load volatile i32, ptr addrspace(3) %lp, align 4
  store volatile i32 %v, ptr addrspace(1) %out, align 4
  ret void
}

define amdgpu_kernel void @unrelated_read_before_own(ptr addrspace(1) inreg %g, ptr addrspace(1) inreg %out, i32 %idx) {
; COMMON-LABEL: unrelated_read_before_own:
; COMMON: global_load_dword
; COMMON: ds_read_b32 [[OTHER:v[0-9]+]], {{.*}}offset:256
; COMMON-NOT: ds_nop
; COMMON: v_mov_b32_e32 [[DUMMY:v[0-9]+]], [[OWNADDR:v[0-9]+]]
; COMMON: ds_read_b32 [[OWN:v[0-9]+]], [[OWNADDR]]
; COMMON: ds_read_b32 [[DUMMY]], [[DUMMY]]
; COMMON: v_mov_b32_e32 [[OWN]], [[DUMMY]]
; COMMON: global_store_dword {{.*}}[[OWN]]
  %lp0 = getelementptr i32, ptr addrspace(3) @lds, i32 %idx
  %idx1 = add i32 %idx, 64
  %lp1 = getelementptr i32, ptr addrspace(3) @lds, i32 %idx1
  call void @llvm.amdgcn.global.load.lds(ptr addrspace(1) %g, ptr addrspace(3) %lp0, i32 4, i32 0, i32 0)
  %other = load volatile i32, ptr addrspace(3) %lp1, align 4
  %own = load volatile i32, ptr addrspace(3) %lp0, align 4
  store volatile i32 %own, ptr addrspace(1) %out, align 4
  ret void
}

define amdgpu_kernel void @pipelined_reads(ptr addrspace(1) inreg %g, ptr addrspace(1) inreg %out, i32 %idx) {
; COMMON-LABEL: pipelined_reads:
; COMMON: global_load_dword
; COMMON: ds_read_b32 [[DST0:v[0-9]+]],
; COMMON-NEXT: ds_read_b32 [[DST1:v[0-9]+]],
; COMMON-NOT: ds_nop
; COMMON: global_store_dword
  %lp0 = getelementptr i32, ptr addrspace(3) @lds, i32 %idx
  %idx1 = add i32 %idx, 64
  %lp1 = getelementptr i32, ptr addrspace(3) @lds, i32 %idx1
  call void @llvm.amdgcn.global.load.lds(ptr addrspace(1) %g, ptr addrspace(3) %lp0, i32 4, i32 0, i32 0)
  %v0 = load volatile i32, ptr addrspace(3) %lp0, align 4
  %v1 = load volatile i32, ptr addrspace(3) %lp1, align 4
  %sum = add i32 %v0, %v1
  store volatile i32 %sum, ptr addrspace(1) %out, align 4
  ret void
}
