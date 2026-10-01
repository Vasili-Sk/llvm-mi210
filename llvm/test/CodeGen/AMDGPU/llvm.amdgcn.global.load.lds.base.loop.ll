; RUN: llc -O2 -mtriple=amdgcn-amd-amdhsa -mcpu=gfx90a -verify-machineinstrs < %s | FileCheck %s --check-prefix=SDAG
; RUN: llc -O2 -mtriple=amdgcn-amd-amdhsa -mcpu=gfx90a -global-isel -verify-machineinstrs < %s | FileCheck %s --check-prefix=GISEL
; RUN: llc -enable-new-pm -O2 -mtriple=amdgcn-amd-amdhsa -mcpu=gfx90a -verify-machineinstrs < %s | FileCheck %s --check-prefix=SDAG
; RUN: llc -enable-new-pm -O2 -mtriple=amdgcn-amd-amdhsa -mcpu=gfx90a -global-isel -verify-machineinstrs < %s | FileCheck %s --check-prefix=GISEL

@lds = external addrspace(3) global [4096 x i8], align 16

declare void @llvm.amdgcn.global.load.lds.base(ptr addrspace(1), i32, ptr addrspace(3), i32 immarg, i32 immarg)
declare i32 @llvm.amdgcn.workitem.id.x()

; Both selectors keep the invariant LDS base until the late pass. The pass puts
; one m0 setup before the loop and reuses it for both direct loads.
; SDAG-LABEL: invariant_loop:
; SDAG: s_mov_b32 m0,
; SDAG: [[LOOP:\.LBB[0-9_]+]]:
; SDAG-NOT: s_mov_b32 m0
; SDAG: global_load_dwordx4
; SDAG-NOT: s_mov_b32 m0
; SDAG: global_load_dwordx4
; SDAG-NOT: s_mov_b32 m0
; SDAG: s_cbranch_{{.*}} [[LOOP]]
; GISEL-LABEL: invariant_loop:
; GISEL: s_mov_b32 m0,
; GISEL: [[LOOP:\.LBB[0-9_]+]]:
; GISEL-NOT: s_mov_b32 m0
; GISEL: global_load_dwordx4
; GISEL-NOT: s_mov_b32 m0
; GISEL: global_load_dwordx4
; GISEL-NOT: s_mov_b32 m0
; GISEL: s_cbranch_{{.*}} [[LOOP]]
define amdgpu_kernel void @invariant_loop(ptr addrspace(1) inreg %base, i32 %n) #0 {
entry:
  %lane = call i32 @llvm.amdgcn.workitem.id.x()
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %loop ]
  %off0 = add i32 %lane, %i
  %off1 = add i32 %off0, 16
  call void @llvm.amdgcn.global.load.lds.base(ptr addrspace(1) %base, i32 %off0, ptr addrspace(3) @lds, i32 16, i32 0)
  call void @llvm.amdgcn.global.load.lds.base(ptr addrspace(1) %base, i32 %off1, ptr addrspace(3) @lds, i32 16, i32 0)
  %next = add nuw i32 %i, 32
  %more = icmp ult i32 %next, %n
  br i1 %more, label %loop, label %exit
exit:
  ret void
}

attributes #0 = { nounwind "target-cpu"="gfx90a" }
