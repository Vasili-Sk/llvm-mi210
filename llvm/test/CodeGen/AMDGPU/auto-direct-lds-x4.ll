; RUN: llc -O2 -mcpu=gfx90a -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -O2 -mcpu=gfx90a -enable-new-pm -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -O2 -mcpu=gfx90a -global-isel=1 -global-isel-abort=1 -verify-machineinstrs < %s | FileCheck %s
; RUN: %python %S/Inputs/auto-direct-lds-negative.py opt %s | FileCheck --check-prefix=MATRIX %s
;
; The first RUN uses the legacy codegen pass manager and SelectionDAG.
; The second RUN uses the new codegen pass manager and SelectionDAG.
; The third RUN uses GlobalISel and rejects any fallback.
; CHECK-LABEL: auto_fuse_x4:
; CHECK-COUNT-1: global_load_dwordx4 {{.*}} lds
; CHECK-NOT: ds_write
;
; Each named negative is a separate generated module. The harness runs the
; standalone pass with -verify-each. It requires the staged producer and store
; to remain. It also requires no explicit-base intrinsic call.
; MATRIX: MATRIX PASS reject_non_gfx90a:
; MATRIX-NEXT: MATRIX PASS reject_wave32:
; MATRIX-NEXT: MATRIX PASS reject_missing_occupancy_contract:
; MATRIX-NEXT: MATRIX PASS reject_nonexact_occupancy_contract:
; MATRIX-NEXT: MATRIX PASS reject_occupancy_9:
; MATRIX-NEXT: MATRIX PASS reject_occupancy_99:
; MATRIX-NEXT: MATRIX PASS reject_occupancy_zero:
; MATRIX-NEXT: MATRIX PASS reject_occupancy_below_max:
; MATRIX-NEXT: MATRIX PASS reject_occupancy_malformed:
; MATRIX-NEXT: MATRIX PASS reject_excessive_static_lds:
; MATRIX-NEXT: MATRIX PASS reject_incompatible_workgroup:
; MATRIX-NEXT: MATRIX PASS reject_width_x1:
; MATRIX-NEXT: MATRIX PASS reject_width_x2:
; MATRIX-NEXT: MATRIX PASS reject_width_x3:
; MATRIX-NEXT: MATRIX PASS reject_width_x5:
; MATRIX-NEXT: MATRIX PASS reject_dynamic_trip:
; MATRIX-NEXT: MATRIX PASS accept_predicate_ne:
; MATRIX-NEXT: MATRIX PASS reject_predicate_ult_true_exit:
; MATRIX-NEXT: MATRIX PASS reject_predicate_ule_true_exit:
; MATRIX-NEXT: MATRIX PASS reject_predicate_slt_true_exit:
; MATRIX-NEXT: MATRIX PASS reject_reversed_successors:
; MATRIX-NEXT: MATRIX PASS reject_nonzero_start_one_trip:
; MATRIX-NEXT: MATRIX PASS reject_step_other_than_one:
; MATRIX-NEXT: MATRIX PASS reject_induction_wrap:
; MATRIX-NEXT: MATRIX PASS reject_multiple_exits:
; MATRIX-NEXT: MATRIX PASS accept_offset_u32_boundary:
; MATRIX-NEXT: MATRIX PASS reject_offset_u32_plus_one:
; MATRIX-NEXT: MATRIX PASS reject_negative_offset:
; MATRIX-NEXT: MATRIX PASS reject_offset_arithmetic_wrap:
; MATRIX-NEXT: MATRIX PASS accept_large_invariant_gep:
; MATRIX-NEXT: MATRIX PASS reject_4g_invariant_gep:
; MATRIX-NEXT: MATRIX PASS reject_zero_trip_bypass:
; MATRIX-NEXT: MATRIX PASS reject_one_stage:
; MATRIX-NEXT: MATRIX PASS reject_bad_payload_permutation:
; MATRIX-NEXT: MATRIX PASS reject_shifted_lds_base:
; MATRIX-NEXT: MATRIX PASS reject_partial_producer:
; MATRIX-NEXT: MATRIX PASS reject_duplicate_producer_destination:
; MATRIX-NEXT: MATRIX PASS reject_missing_payload:
; MATRIX-NEXT: MATRIX PASS reject_extra_producer_use:
; MATRIX-NEXT: MATRIX PASS reject_extra_lds_store:
; MATRIX-NEXT: MATRIX PASS reject_tile_pointer_escape:
; MATRIX-NEXT: MATRIX PASS reject_unknown_alias:
; MATRIX-NEXT: MATRIX PASS reject_extra_consumer:
; MATRIX-NEXT: MATRIX PASS reject_partial_consumer:
; MATRIX-NEXT: MATRIX PASS reject_duplicate_consumer:
; MATRIX-NEXT: MATRIX PASS reject_unsupported_ds_read:
; MATRIX-NEXT: MATRIX PASS reject_volatile_mismatch:
; MATRIX-NEXT: MATRIX PASS reject_atomic_access:
; MATRIX-NEXT: MATRIX PASS reject_nontemporal_cache:
; MATRIX-NEXT: MATRIX PASS reject_non_dominating_producer:
; MATRIX-NEXT: MATRIX PASS reject_conditional_store:
; MATRIX-NEXT: MATRIX PASS reject_branch_inside_loop:
; MATRIX-NEXT: MATRIX PASS reject_two_predecessor_merge:
; MATRIX-NEXT: MATRIX PASS reject_call_in_loop:
; MATRIX-NEXT: MATRIX PASS reject_changing_global_base:
; MATRIX-NEXT: MATRIX PASS reject_changing_lds_base:
; MATRIX-NEXT: MATRIX PASS reject_multiple_waves:
; MATRIX-NEXT: MATRIX PASS reject_wrong_workgroup:
; MATRIX-NEXT: MATRIX PASS reject_nested_loop:
; MATRIX-NEXT: MATRIX PASS reject_unrelated_classic:
; MATRIX-NEXT: MATRIX PASS positive_idempotent:

; ModuleID = '/home/vasilisk/scratch/auto-fusion/auto.cu'
source_filename = "/home/vasilisk/scratch/auto-fusion/auto.cu"
target datalayout = "e-m:e-p:64:64-p1:64:64-p2:32:32-p3:32:32-p4:64:64-p5:32:32-p6:32:32-p7:160:256:256:32-p8:128:128:128:48-p9:192:256:256:32-p10:32:32-p11:32:32-p12:32:32-p13:32:32-p14:32:32-p15:32:32-i64:64-v16:16-v24:32-v32:32-v48:64-v96:128-v192:256-v256:256-v512:512-v1024:1024-v2048:2048-n32:64-S32-A5-G1-ni:7:8:9"
target triple = "amdgpu9.0a-amd-amdhsa"

@tile = internal unnamed_addr addrspace(3) global [256 x i32] undef, align 1024
@__hip_cuid_776fc4f6db00e5bc = addrspace(1) global i8 0
@llvm.compiler.used = appending addrspace(1) global [1 x ptr] [ptr addrspacecast (ptr addrspace(1) @__hip_cuid_776fc4f6db00e5bc to ptr)], section "llvm.metadata"

; Function Attrs: convergent mustprogress norecurse nounwind uwtable
define protected amdgpu_kernel void @auto_fuse_x4(ptr addrspace(1) noalias nofree noundef readonly captures(none) %Input.coerce, ptr addrspace(1) noalias nofree noundef writeonly captures(none) %Output.coerce) local_unnamed_addr #0 {
entry:
  %0 = tail call noundef range(i32 0, 1024) i32 @llvm.amdgcn.workitem.id.x()
  %and = and i32 %0, 63
  %xor = xor i32 %and, -1640531527
  %1 = tail call noundef i32 @llvm.amdgcn.workgroup.id.x()
  %conv = zext i32 %1 to i64
  %conv7 = zext nneg i32 %and to i64
  %.idx62 = shl nuw nsw i64 %conv, 13
  %2 = getelementptr inbounds nuw i8, ptr addrspace(1) %Input.coerce, i64 %.idx62
  %invariant.gep = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %2, i64 %conv7
  %and3.i = and i32 %0, 3
  %3 = shl nuw nsw i32 %0, 4
  %.idx85 = and i32 %3, 960
  %4 = getelementptr inbounds nuw i8, ptr addrspace(3) @tile, i32 %.idx85
  %arrayidx10 = getelementptr inbounds nuw [4 x i8], ptr addrspace(3) %4, i32 %and3.i
  %arrayidx13 = getelementptr inbounds nuw i8, ptr addrspace(3) %arrayidx10, i32 32
  %arrayidx16 = getelementptr inbounds nuw i8, ptr addrspace(3) %arrayidx10, i32 16
  %arrayidx19 = getelementptr inbounds nuw i8, ptr addrspace(3) %arrayidx10, i32 48
  br label %for.body

for.cond.cleanup:                                 ; preds = %for.body
  %.idx = shl nuw nsw i64 %conv, 8
  %5 = getelementptr inbounds nuw i8, ptr addrspace(1) %Output.coerce, i64 %.idx
  %arrayidx45 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %5, i64 %conv7
  store i32 %xor39, ptr addrspace(1) %arrayidx45, align 4
  ret void

for.body:                                         ; preds = %entry, %for.body
  %Acc.090 = phi i32 [ %xor, %entry ], [ %xor39, %for.body ]
  %Trip.089 = phi i32 [ 0, %entry ], [ %inc, %for.body ]
  %6 = shl nuw nsw i32 %Trip.089, 6
  %mul6 = zext nneg i32 %6 to i64
  %gep = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %invariant.gep, i64 %mul6
  %7 = load <4 x i32>, ptr addrspace(1) %gep, align 16
  %8 = extractelement <4 x i32> %7, i64 0
  store i32 %8, ptr addrspace(3) %arrayidx10, align 4
  %9 = extractelement <4 x i32> %7, i64 1
  store i32 %9, ptr addrspace(3) %arrayidx13, align 4
  %10 = extractelement <4 x i32> %7, i64 2
  store i32 %10, ptr addrspace(3) %arrayidx16, align 4
  %11 = extractelement <4 x i32> %7, i64 3
  store i32 %11, ptr addrspace(3) %arrayidx19, align 4
  fence syncscope("workgroup") release
  tail call void @llvm.amdgcn.s.barrier()
  fence syncscope("workgroup") acquire
  %12 = load i32, ptr addrspace(3) %arrayidx10, align 4
  %13 = load i32, ptr addrspace(3) %arrayidx13, align 4
  %14 = load i32, ptr addrspace(3) %arrayidx16, align 4
  %15 = load i32, ptr addrspace(3) %arrayidx19, align 4
  %add32 = add i32 %12, %Acc.090
  %or.i = tail call noundef i32 @llvm.fshl.i32(i32 %add32, i32 %add32, i32 5)
  %mul34 = mul i32 %13, -2048144789
  %xor35 = xor i32 %or.i, %mul34
  %add36 = add i32 %xor35, %14
  %or.i84 = tail call noundef i32 @llvm.fshl.i32(i32 %add36, i32 %add36, i32 11)
  %mul38 = mul i32 %15, -1028477387
  %xor39 = xor i32 %or.i84, %mul38
  fence syncscope("workgroup") release
  tail call void @llvm.amdgcn.s.barrier()
  fence syncscope("workgroup") acquire
  %inc = add nuw nsw i32 %Trip.089, 1
  %exitcond.not = icmp eq i32 %inc, 8
  br i1 %exitcond.not, label %for.cond.cleanup, label %for.body
}

; Function Attrs: convergent mustprogress nocallback nofree nounwind willreturn
declare void @llvm.amdgcn.s.barrier() #1

; Function Attrs: mustprogress nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare noundef range(i32 0, 1024) i32 @llvm.amdgcn.workitem.id.x() #2

; Function Attrs: mustprogress nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare noundef i32 @llvm.amdgcn.workgroup.id.x() #2

; Function Attrs: nocallback nocreateundeforpoison nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.fshl.i32(i32, i32, i32) #3

attributes #0 = { convergent mustprogress norecurse nounwind uwtable "amdgpu-agpr-alloc"="0" "amdgpu-flat-work-group-size"="64,64" "amdgpu-waves-per-eu"="8,8" "amdgpu-no-cluster-id-x" "amdgpu-no-cluster-id-y" "amdgpu-no-cluster-id-z" "amdgpu-no-completion-action" "amdgpu-no-default-queue" "amdgpu-no-dispatch-id" "amdgpu-no-dispatch-ptr" "amdgpu-no-flat-scratch-init" "amdgpu-no-heap-ptr" "amdgpu-no-hostcall-ptr" "amdgpu-no-implicitarg-ptr" "amdgpu-no-lds-kernel-id" "amdgpu-no-multigrid-sync-arg" "amdgpu-no-queue-ptr" "amdgpu-no-workgroup-id-x" "amdgpu-no-workgroup-id-y" "amdgpu-no-workgroup-id-z" "amdgpu-no-workitem-id-x" "amdgpu-no-workitem-id-y" "amdgpu-no-workitem-id-z" "amdgpu-no-wwm" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="gfx90a" "uniform-work-group-size" }
attributes #1 = { convergent mustprogress nocallback nofree nounwind willreturn }
attributes #2 = { mustprogress nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #3 = { nocallback nocreateundeforpoison nofree nosync nounwind speculatable willreturn memory(none) }
