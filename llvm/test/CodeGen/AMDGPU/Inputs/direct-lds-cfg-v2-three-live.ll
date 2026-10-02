; ModuleID = '/home/vasilisk/llama_build/spec-test/asm/ck_lab/direct_lds_v2/cfg_v2_queue_extract.cu'
source_filename = "/home/vasilisk/llama_build/spec-test/asm/ck_lab/direct_lds_v2/cfg_v2_queue_extract.cu"
target datalayout = "e-m:e-p:64:64-p1:64:64-p2:32:32-p3:32:32-p4:64:64-p5:32:32-p6:32:32-p7:160:256:256:32-p8:128:128:128:48-p9:192:256:256:32-p10:32:32-p11:32:32-p12:32:32-p13:32:32-p14:32:32-p15:32:32-i64:64-v16:16-v24:32-v32:32-v48:64-v96:128-v192:256-v256:256-v512:512-v1024:1024-v2048:2048-n32:64-S32-A5-G1-ni:7:8:9"
target triple = "amdgpu9.0a-amd-amdhsa"

@_ZZ20cfg_v2_queue_extractE4tile = internal unnamed_addr addrspace(3) global [6144 x i32] undef, align 1024
@__hip_cuid_4dcc32585d74b32d = addrspace(1) global i8 0
@llvm.compiler.used = appending addrspace(1) global [1 x ptr] [ptr addrspacecast (ptr addrspace(1) @__hip_cuid_4dcc32585d74b32d to ptr)], section "llvm.metadata"

; Function Attrs: convergent mustprogress norecurse nounwind willreturn uwtable
define protected amdgpu_kernel void @cfg_v2_queue_extract(ptr addrspace(1) noalias nofree noundef readonly captures(none) %a.coerce, ptr addrspace(1) noalias nofree noundef readonly captures(none) %b.coerce, ptr addrspace(1) noalias nofree noundef writeonly %out.coerce, i32 noundef %stage_stride_x4) local_unnamed_addr #0 {
entry:
  %0 = tail call noundef range(i32 0, 1024) i32 @llvm.amdgcn.workitem.id.x()
  %idxprom = zext nneg i32 %0 to i64
  %arrayidx = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %a.coerce, i64 %idxprom
  %1 = load <4 x i32>, ptr addrspace(1) %arrayidx, align 16, !tbaa !13
  %add9.1 = add nuw nsw i32 %0, 256
  %idxprom.1 = zext nneg i32 %add9.1 to i64
  %arrayidx.1 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %a.coerce, i64 %idxprom.1
  %2 = load <4 x i32>, ptr addrspace(1) %arrayidx.1, align 16, !tbaa !13
  %3 = zext nneg i32 %0 to i64
  %4 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %a.coerce, i64 %3
  %arrayidx.2 = getelementptr inbounds nuw i8, ptr addrspace(1) %4, i64 8192
  %5 = load <4 x i32>, ptr addrspace(1) %arrayidx.2, align 16, !tbaa !13
  %6 = zext nneg i32 %0 to i64
  %7 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %a.coerce, i64 %6
  %arrayidx.3 = getelementptr inbounds nuw i8, ptr addrspace(1) %7, i64 12288
  %8 = load <4 x i32>, ptr addrspace(1) %arrayidx.3, align 16, !tbaa !13
  %arrayidx24 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %b.coerce, i64 %idxprom
  %9 = load <4 x i32>, ptr addrspace(1) %arrayidx24, align 16, !tbaa !13
  %arrayidx24.1 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %b.coerce, i64 %idxprom.1
  %10 = load <4 x i32>, ptr addrspace(1) %arrayidx24.1, align 16, !tbaa !13
  %add.1 = add i32 %0, %stage_stride_x4
  %idxprom.1504 = zext i32 %add.1 to i64
  %arrayidx.1505 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %a.coerce, i64 %idxprom.1504
  %11 = load <4 x i32>, ptr addrspace(1) %arrayidx.1505, align 16, !tbaa !13
  %add9.1.1 = add i32 %add.1, 256
  %idxprom.1.1 = zext i32 %add9.1.1 to i64
  %arrayidx.1.1 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %a.coerce, i64 %idxprom.1.1
  %12 = load <4 x i32>, ptr addrspace(1) %arrayidx.1.1, align 16, !tbaa !13
  %add9.2.1 = add i32 %add.1, 512
  %idxprom.2.1 = zext i32 %add9.2.1 to i64
  %arrayidx.2.1 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %a.coerce, i64 %idxprom.2.1
  %13 = load <4 x i32>, ptr addrspace(1) %arrayidx.2.1, align 16, !tbaa !13
  %add9.3.1 = add i32 %add.1, 768
  %idxprom.3.1 = zext i32 %add9.3.1 to i64
  %arrayidx.3.1 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %a.coerce, i64 %idxprom.3.1
  %14 = load <4 x i32>, ptr addrspace(1) %arrayidx.3.1, align 16, !tbaa !13
  %arrayidx24.1507 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %b.coerce, i64 %idxprom.1504
  %15 = load <4 x i32>, ptr addrspace(1) %arrayidx24.1507, align 16, !tbaa !13
  %arrayidx24.1.1 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %b.coerce, i64 %idxprom.1.1
  %16 = load <4 x i32>, ptr addrspace(1) %arrayidx24.1.1, align 16, !tbaa !13
  %17 = shl nuw nsw i32 %0, 2
  %mul1.i = and i32 %17, 240
  %and.i = and i32 %0, 3
  %invariant.gep = getelementptr inbounds nuw [4 x i8], ptr addrspace(3) @_ZZ20cfg_v2_queue_extractE4tile, i32 %mul1.i
  %18 = shl nuw nsw i32 %0, 4
  %.idx435 = and i32 %18, 15360
  %invariant.gep445 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep, i32 %.idx435
  %invariant.gep446 = getelementptr inbounds nuw [4 x i8], ptr addrspace(3) %invariant.gep445, i32 %and.i
  %vecext = extractelement <4 x i32> %1, i64 0
  store i32 %vecext, ptr addrspace(3) %invariant.gep446, align 4, !tbaa !14
  %vecext.1 = extractelement <4 x i32> %1, i64 1
  %gep443.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 32
  store i32 %vecext.1, ptr addrspace(3) %gep443.1, align 4, !tbaa !14
  %vecext.2 = extractelement <4 x i32> %1, i64 2
  %gep443.2 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 16
  store i32 %vecext.2, ptr addrspace(3) %gep443.2, align 4, !tbaa !14
  %vecext.3 = extractelement <4 x i32> %1, i64 3
  %gep443.3 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 48
  store i32 %vecext.3, ptr addrspace(3) %gep443.3, align 4, !tbaa !14
  %gep447.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 4096
  %vecext.1508 = extractelement <4 x i32> %2, i64 0
  store i32 %vecext.1508, ptr addrspace(3) %gep447.1, align 4, !tbaa !14
  %vecext.1.1 = extractelement <4 x i32> %2, i64 1
  %gep443.1.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 4128
  store i32 %vecext.1.1, ptr addrspace(3) %gep443.1.1, align 4, !tbaa !14
  %vecext.2.1 = extractelement <4 x i32> %2, i64 2
  %gep443.2.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 4112
  store i32 %vecext.2.1, ptr addrspace(3) %gep443.2.1, align 4, !tbaa !14
  %vecext.3.1 = extractelement <4 x i32> %2, i64 3
  %gep443.3.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 4144
  store i32 %vecext.3.1, ptr addrspace(3) %gep443.3.1, align 4, !tbaa !14
  %gep447.2 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 8192
  %vecext.2509 = extractelement <4 x i32> %5, i64 0
  store i32 %vecext.2509, ptr addrspace(3) %gep447.2, align 4, !tbaa !14
  %vecext.1.2 = extractelement <4 x i32> %5, i64 1
  %gep443.1.2 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 8224
  store i32 %vecext.1.2, ptr addrspace(3) %gep443.1.2, align 4, !tbaa !14
  %vecext.2.2 = extractelement <4 x i32> %5, i64 2
  %gep443.2.2 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 8208
  store i32 %vecext.2.2, ptr addrspace(3) %gep443.2.2, align 4, !tbaa !14
  %vecext.3.2 = extractelement <4 x i32> %5, i64 3
  %gep443.3.2 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 8240
  store i32 %vecext.3.2, ptr addrspace(3) %gep443.3.2, align 4, !tbaa !14
  %gep447.3 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 12288
  %vecext.3510 = extractelement <4 x i32> %8, i64 0
  store i32 %vecext.3510, ptr addrspace(3) %gep447.3, align 4, !tbaa !14
  %vecext.1.3 = extractelement <4 x i32> %8, i64 1
  %gep443.1.3 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 12320
  store i32 %vecext.1.3, ptr addrspace(3) %gep443.1.3, align 4, !tbaa !14
  %vecext.2.3 = extractelement <4 x i32> %8, i64 2
  %gep443.2.3 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 12304
  store i32 %vecext.2.3, ptr addrspace(3) %gep443.2.3, align 4, !tbaa !14
  %vecext.3.3 = extractelement <4 x i32> %8, i64 3
  %gep443.3.3 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 12336
  store i32 %vecext.3.3, ptr addrspace(3) %gep443.3.3, align 4, !tbaa !14
  %vecext71 = extractelement <4 x i32> %9, i64 0
  %arrayidx77 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 16384
  store i32 %vecext71, ptr addrspace(3) %arrayidx77, align 4, !tbaa !14
  %vecext71.1 = extractelement <4 x i32> %9, i64 1
  %arrayidx77.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 16416
  store i32 %vecext71.1, ptr addrspace(3) %arrayidx77.1, align 4, !tbaa !14
  %vecext71.2 = extractelement <4 x i32> %9, i64 2
  %arrayidx77.2 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 16400
  store i32 %vecext71.2, ptr addrspace(3) %arrayidx77.2, align 4, !tbaa !14
  %vecext71.3 = extractelement <4 x i32> %9, i64 3
  %arrayidx77.3 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 16432
  store i32 %vecext71.3, ptr addrspace(3) %arrayidx77.3, align 4, !tbaa !14
  %vecext71.1511 = extractelement <4 x i32> %10, i64 0
  %arrayidx77.1512 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 20480
  store i32 %vecext71.1511, ptr addrspace(3) %arrayidx77.1512, align 4, !tbaa !14
  %vecext71.1.1 = extractelement <4 x i32> %10, i64 1
  %arrayidx77.1.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 20512
  store i32 %vecext71.1.1, ptr addrspace(3) %arrayidx77.1.1, align 4, !tbaa !14
  %vecext71.2.1 = extractelement <4 x i32> %10, i64 2
  %arrayidx77.2.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 20496
  store i32 %vecext71.2.1, ptr addrspace(3) %arrayidx77.2.1, align 4, !tbaa !14
  %vecext71.3.1 = extractelement <4 x i32> %10, i64 3
  %arrayidx77.3.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 20528
  store i32 %vecext71.3.1, ptr addrspace(3) %arrayidx77.3.1, align 4, !tbaa !14
  %mul89 = shl i32 %stage_stride_x4, 1
  %add91 = add i32 %mul89, %0
  %idxprom93 = zext i32 %add91 to i64
  %arrayidx94 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %a.coerce, i64 %idxprom93
  %19 = load <4 x i32>, ptr addrspace(1) %arrayidx94, align 16, !tbaa !13
  %add92.1 = add i32 %add91, 256
  %idxprom93.1 = zext i32 %add92.1 to i64
  %arrayidx94.1 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %a.coerce, i64 %idxprom93.1
  %20 = load <4 x i32>, ptr addrspace(1) %arrayidx94.1, align 16, !tbaa !13
  %add92.2 = add i32 %add91, 512
  %idxprom93.2 = zext i32 %add92.2 to i64
  %arrayidx94.2 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %a.coerce, i64 %idxprom93.2
  %21 = load <4 x i32>, ptr addrspace(1) %arrayidx94.2, align 16, !tbaa !13
  %add92.3 = add i32 %add91, 768
  %idxprom93.3 = zext i32 %add92.3 to i64
  %arrayidx94.3 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %a.coerce, i64 %idxprom93.3
  %22 = load <4 x i32>, ptr addrspace(1) %arrayidx94.3, align 16, !tbaa !13
  %arrayidx111 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %b.coerce, i64 %idxprom93
  %23 = load <4 x i32>, ptr addrspace(1) %arrayidx111, align 16, !tbaa !13
  %arrayidx111.1 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %b.coerce, i64 %idxprom93.1
  %24 = load <4 x i32>, ptr addrspace(1) %arrayidx111.1, align 16, !tbaa !13
  %invariant.gep463 = getelementptr inbounds nuw i8, ptr addrspace(3) @_ZZ20cfg_v2_queue_extractE4tile, i32 %.idx435
  %invariant.gep464 = getelementptr inbounds nuw [4 x i8], ptr addrspace(3) %invariant.gep463, i32 %and.i
  %invariant.gep466 = getelementptr inbounds nuw [4 x i8], ptr addrspace(3) %invariant.gep464, i32 %mul1.i
  %mul244 = mul i32 %stage_stride_x4, 3
  %add246 = add i32 %0, %mul244
  %idxprom248 = zext i32 %add246 to i64
  %arrayidx249 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %a.coerce, i64 %idxprom248
  %early89 = load <4 x i32>, ptr addrspace(1) %arrayidx249, align 16, !tbaa !13
  %add247.1 = add i32 %add246, 256
  %idxprom248.1 = zext i32 %add247.1 to i64
  %arrayidx249.1 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %a.coerce, i64 %idxprom248.1
  %early90 = load <4 x i32>, ptr addrspace(1) %arrayidx249.1, align 16, !tbaa !13
  %add247.2 = add i32 %add246, 512
  %idxprom248.2 = zext i32 %add247.2 to i64
  %arrayidx249.2 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %a.coerce, i64 %idxprom248.2
  %early91 = load <4 x i32>, ptr addrspace(1) %arrayidx249.2, align 16, !tbaa !13
  %add247.3 = add i32 %add246, 768
  %idxprom248.3 = zext i32 %add247.3 to i64
  %arrayidx249.3 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %a.coerce, i64 %idxprom248.3
  %early92 = load <4 x i32>, ptr addrspace(1) %arrayidx249.3, align 16, !tbaa !13
  %arrayidx267 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %b.coerce, i64 %idxprom248
  %early93 = load <4 x i32>, ptr addrspace(1) %arrayidx267, align 16, !tbaa !13
  %arrayidx267.1 = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %b.coerce, i64 %idxprom248.1
  %early94 = load <4 x i32>, ptr addrspace(1) %arrayidx267.1, align 16, !tbaa !13
  fence syncscope("workgroup") release
  tail call void @llvm.amdgcn.s.barrier()
  fence syncscope("workgroup") acquire
  %25 = load i32, ptr addrspace(3) %invariant.gep466, align 4, !tbaa !14
  %idxprom141 = zext nneg i32 %17 to i64
  %arrayidx142 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %idxprom141
  %arrayidx138.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep466, i32 32
  %26 = load i32, ptr addrspace(3) %arrayidx138.1, align 4, !tbaa !14
  %arrayidx138.2 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep466, i32 16
  %27 = load i32, ptr addrspace(3) %arrayidx138.2, align 4, !tbaa !14
  %arrayidx138.3 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep466, i32 48
  %28 = load i32, ptr addrspace(3) %arrayidx138.3, align 4, !tbaa !14
  %29 = insertelement <4 x i32> poison, i32 %25, i64 0
  %30 = insertelement <4 x i32> %29, i32 %26, i64 1
  %31 = insertelement <4 x i32> %30, i32 %27, i64 2
  %32 = insertelement <4 x i32> %31, i32 %28, i64 3
  store <4 x i32> %32, ptr addrspace(1) %arrayidx142, align 4, !tbaa !14
  %gep.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep466, i32 4096
  %33 = load i32, ptr addrspace(3) %gep.1, align 4, !tbaa !14
  %34 = zext nneg i32 %17 to i64
  %35 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %34
  %arrayidx142.1514 = getelementptr inbounds nuw i8, ptr addrspace(1) %35, i64 4096
  %arrayidx138.1.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep466, i32 4128
  %36 = load i32, ptr addrspace(3) %arrayidx138.1.1, align 4, !tbaa !14
  %arrayidx138.2.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep466, i32 4112
  %37 = load i32, ptr addrspace(3) %arrayidx138.2.1, align 4, !tbaa !14
  %arrayidx138.3.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep466, i32 4144
  %38 = load i32, ptr addrspace(3) %arrayidx138.3.1, align 4, !tbaa !14
  %39 = insertelement <4 x i32> poison, i32 %33, i64 0
  %40 = insertelement <4 x i32> %39, i32 %36, i64 1
  %41 = insertelement <4 x i32> %40, i32 %37, i64 2
  %42 = insertelement <4 x i32> %41, i32 %38, i64 3
  store <4 x i32> %42, ptr addrspace(1) %arrayidx142.1514, align 4, !tbaa !14
  %gep.2 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep466, i32 8192
  %43 = load i32, ptr addrspace(3) %gep.2, align 4, !tbaa !14
  %44 = zext nneg i32 %17 to i64
  %45 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %44
  %arrayidx142.2516 = getelementptr inbounds nuw i8, ptr addrspace(1) %45, i64 8192
  %arrayidx138.1.2 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep466, i32 8224
  %46 = load i32, ptr addrspace(3) %arrayidx138.1.2, align 4, !tbaa !14
  %arrayidx138.2.2 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep466, i32 8208
  %47 = load i32, ptr addrspace(3) %arrayidx138.2.2, align 4, !tbaa !14
  %arrayidx138.3.2 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep466, i32 8240
  %48 = load i32, ptr addrspace(3) %arrayidx138.3.2, align 4, !tbaa !14
  %49 = insertelement <4 x i32> poison, i32 %43, i64 0
  %50 = insertelement <4 x i32> %49, i32 %46, i64 1
  %51 = insertelement <4 x i32> %50, i32 %47, i64 2
  %52 = insertelement <4 x i32> %51, i32 %48, i64 3
  store <4 x i32> %52, ptr addrspace(1) %arrayidx142.2516, align 4, !tbaa !14
  %gep.3 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep466, i32 12288
  %53 = load i32, ptr addrspace(3) %gep.3, align 4, !tbaa !14
  %54 = zext nneg i32 %17 to i64
  %55 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %54
  %arrayidx142.3518 = getelementptr inbounds nuw i8, ptr addrspace(1) %55, i64 12288
  %arrayidx138.1.3 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep466, i32 12320
  %56 = load i32, ptr addrspace(3) %arrayidx138.1.3, align 4, !tbaa !14
  %arrayidx138.2.3 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep466, i32 12304
  %57 = load i32, ptr addrspace(3) %arrayidx138.2.3, align 4, !tbaa !14
  %arrayidx138.3.3 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep466, i32 12336
  %58 = load i32, ptr addrspace(3) %arrayidx138.3.3, align 4, !tbaa !14
  %59 = insertelement <4 x i32> poison, i32 %53, i64 0
  %60 = insertelement <4 x i32> %59, i32 %56, i64 1
  %61 = insertelement <4 x i32> %60, i32 %57, i64 2
  %62 = insertelement <4 x i32> %61, i32 %58, i64 3
  store <4 x i32> %62, ptr addrspace(1) %arrayidx142.3518, align 4, !tbaa !14
  %63 = getelementptr inbounds nuw i8, ptr addrspace(3) getelementptr inbounds nuw (i8, ptr addrspace(3) @_ZZ20cfg_v2_queue_extractE4tile, i32 16384), i32 %.idx435
  %64 = getelementptr inbounds nuw [4 x i8], ptr addrspace(3) %63, i32 %and.i
  %65 = getelementptr inbounds nuw [4 x i8], ptr addrspace(3) %64, i32 %mul1.i
  %66 = load i32, ptr addrspace(3) %65, align 4, !tbaa !14
  %67 = zext nneg i32 %17 to i64
  %68 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %67
  %arrayidx172 = getelementptr inbounds nuw i8, ptr addrspace(1) %68, i64 16384
  %arrayidx167.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %65, i32 32
  %69 = load i32, ptr addrspace(3) %arrayidx167.1, align 4, !tbaa !14
  %arrayidx167.2 = getelementptr inbounds nuw i8, ptr addrspace(3) %65, i32 16
  %70 = load i32, ptr addrspace(3) %arrayidx167.2, align 4, !tbaa !14
  %arrayidx167.3 = getelementptr inbounds nuw i8, ptr addrspace(3) %65, i32 48
  %71 = load i32, ptr addrspace(3) %arrayidx167.3, align 4, !tbaa !14
  %72 = insertelement <4 x i32> poison, i32 %66, i64 0
  %73 = insertelement <4 x i32> %72, i32 %69, i64 1
  %74 = insertelement <4 x i32> %73, i32 %70, i64 2
  %75 = insertelement <4 x i32> %74, i32 %71, i64 3
  store <4 x i32> %75, ptr addrspace(1) %arrayidx172, align 4, !tbaa !14
  %76 = getelementptr inbounds nuw i8, ptr addrspace(3) getelementptr inbounds nuw (i8, ptr addrspace(3) @_ZZ20cfg_v2_queue_extractE4tile, i32 20480), i32 %.idx435
  %77 = getelementptr inbounds nuw [4 x i8], ptr addrspace(3) %76, i32 %and.i
  %78 = getelementptr inbounds nuw [4 x i8], ptr addrspace(3) %77, i32 %mul1.i
  %79 = load i32, ptr addrspace(3) %78, align 4, !tbaa !14
  %80 = zext nneg i32 %17 to i64
  %81 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %80
  %arrayidx172.1520 = getelementptr inbounds nuw i8, ptr addrspace(1) %81, i64 20480
  %arrayidx167.1.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %78, i32 32
  %82 = load i32, ptr addrspace(3) %arrayidx167.1.1, align 4, !tbaa !14
  %arrayidx167.2.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %78, i32 16
  %83 = load i32, ptr addrspace(3) %arrayidx167.2.1, align 4, !tbaa !14
  %arrayidx167.3.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %78, i32 48
  %84 = load i32, ptr addrspace(3) %arrayidx167.3.1, align 4, !tbaa !14
  %85 = insertelement <4 x i32> poison, i32 %79, i64 0
  %86 = insertelement <4 x i32> %85, i32 %82, i64 1
  %87 = insertelement <4 x i32> %86, i32 %83, i64 2
  %88 = insertelement <4 x i32> %87, i32 %84, i64 3
  store <4 x i32> %88, ptr addrspace(1) %arrayidx172.1520, align 4, !tbaa !14
  fence syncscope("workgroup") release
  tail call void @llvm.amdgcn.s.barrier()
  fence syncscope("workgroup") acquire
  %vecext197 = extractelement <4 x i32> %11, i64 0
  store i32 %vecext197, ptr addrspace(3) %invariant.gep446, align 4, !tbaa !14
  %vecext197.1 = extractelement <4 x i32> %11, i64 1
  store i32 %vecext197.1, ptr addrspace(3) %gep443.1, align 4, !tbaa !14
  %vecext197.2 = extractelement <4 x i32> %11, i64 2
  store i32 %vecext197.2, ptr addrspace(3) %gep443.2, align 4, !tbaa !14
  %vecext197.3 = extractelement <4 x i32> %11, i64 3
  store i32 %vecext197.3, ptr addrspace(3) %gep443.3, align 4, !tbaa !14
  %vecext197.1521 = extractelement <4 x i32> %12, i64 0
  store i32 %vecext197.1521, ptr addrspace(3) %gep447.1, align 4, !tbaa !14
  %vecext197.1.1 = extractelement <4 x i32> %12, i64 1
  store i32 %vecext197.1.1, ptr addrspace(3) %gep443.1.1, align 4, !tbaa !14
  %vecext197.2.1 = extractelement <4 x i32> %12, i64 2
  store i32 %vecext197.2.1, ptr addrspace(3) %gep443.2.1, align 4, !tbaa !14
  %vecext197.3.1 = extractelement <4 x i32> %12, i64 3
  store i32 %vecext197.3.1, ptr addrspace(3) %gep443.3.1, align 4, !tbaa !14
  %vecext197.2522 = extractelement <4 x i32> %13, i64 0
  store i32 %vecext197.2522, ptr addrspace(3) %gep447.2, align 4, !tbaa !14
  %vecext197.1.2 = extractelement <4 x i32> %13, i64 1
  store i32 %vecext197.1.2, ptr addrspace(3) %gep443.1.2, align 4, !tbaa !14
  %vecext197.2.2 = extractelement <4 x i32> %13, i64 2
  store i32 %vecext197.2.2, ptr addrspace(3) %gep443.2.2, align 4, !tbaa !14
  %vecext197.3.2 = extractelement <4 x i32> %13, i64 3
  store i32 %vecext197.3.2, ptr addrspace(3) %gep443.3.2, align 4, !tbaa !14
  %vecext197.3523 = extractelement <4 x i32> %14, i64 0
  store i32 %vecext197.3523, ptr addrspace(3) %gep447.3, align 4, !tbaa !14
  %vecext197.1.3 = extractelement <4 x i32> %14, i64 1
  store i32 %vecext197.1.3, ptr addrspace(3) %gep443.1.3, align 4, !tbaa !14
  %vecext197.2.3 = extractelement <4 x i32> %14, i64 2
  store i32 %vecext197.2.3, ptr addrspace(3) %gep443.2.3, align 4, !tbaa !14
  %vecext197.3.3 = extractelement <4 x i32> %14, i64 3
  store i32 %vecext197.3.3, ptr addrspace(3) %gep443.3.3, align 4, !tbaa !14
  %vecext223 = extractelement <4 x i32> %15, i64 0
  store i32 %vecext223, ptr addrspace(3) %arrayidx77, align 4, !tbaa !14
  %vecext223.1 = extractelement <4 x i32> %15, i64 1
  store i32 %vecext223.1, ptr addrspace(3) %arrayidx77.1, align 4, !tbaa !14
  %vecext223.2 = extractelement <4 x i32> %15, i64 2
  store i32 %vecext223.2, ptr addrspace(3) %arrayidx77.2, align 4, !tbaa !14
  %vecext223.3 = extractelement <4 x i32> %15, i64 3
  store i32 %vecext223.3, ptr addrspace(3) %arrayidx77.3, align 4, !tbaa !14
  %vecext223.1524 = extractelement <4 x i32> %16, i64 0
  store i32 %vecext223.1524, ptr addrspace(3) %arrayidx77.1512, align 4, !tbaa !14
  %vecext223.1.1 = extractelement <4 x i32> %16, i64 1
  store i32 %vecext223.1.1, ptr addrspace(3) %arrayidx77.1.1, align 4, !tbaa !14
  %vecext223.2.1 = extractelement <4 x i32> %16, i64 2
  store i32 %vecext223.2.1, ptr addrspace(3) %arrayidx77.2.1, align 4, !tbaa !14
  %vecext223.3.1 = extractelement <4 x i32> %16, i64 3
  store i32 %vecext223.3.1, ptr addrspace(3) %arrayidx77.3.1, align 4, !tbaa !14
  fence syncscope("workgroup") release
  tail call void @llvm.amdgcn.s.barrier()
  fence syncscope("workgroup") acquire
  %95 = load i32, ptr addrspace(3) %invariant.gep466, align 4, !tbaa !14
  %96 = zext nneg i32 %17 to i64
  %97 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %96
  %arrayidx142.1527 = getelementptr inbounds nuw i8, ptr addrspace(1) %97, i64 24576
  %98 = load i32, ptr addrspace(3) %arrayidx138.1, align 4, !tbaa !14
  %99 = load i32, ptr addrspace(3) %arrayidx138.2, align 4, !tbaa !14
  %100 = load i32, ptr addrspace(3) %arrayidx138.3, align 4, !tbaa !14
  %101 = insertelement <4 x i32> poison, i32 %95, i64 0
  %102 = insertelement <4 x i32> %101, i32 %98, i64 1
  %103 = insertelement <4 x i32> %102, i32 %99, i64 2
  %104 = insertelement <4 x i32> %103, i32 %100, i64 3
  store <4 x i32> %104, ptr addrspace(1) %arrayidx142.1527, align 4, !tbaa !14
  %105 = load i32, ptr addrspace(3) %gep.1, align 4, !tbaa !14
  %106 = zext nneg i32 %17 to i64
  %107 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %106
  %arrayidx142.1514.1 = getelementptr inbounds nuw i8, ptr addrspace(1) %107, i64 28672
  %108 = load i32, ptr addrspace(3) %arrayidx138.1.1, align 4, !tbaa !14
  %109 = load i32, ptr addrspace(3) %arrayidx138.2.1, align 4, !tbaa !14
  %110 = load i32, ptr addrspace(3) %arrayidx138.3.1, align 4, !tbaa !14
  %111 = insertelement <4 x i32> poison, i32 %105, i64 0
  %112 = insertelement <4 x i32> %111, i32 %108, i64 1
  %113 = insertelement <4 x i32> %112, i32 %109, i64 2
  %114 = insertelement <4 x i32> %113, i32 %110, i64 3
  store <4 x i32> %114, ptr addrspace(1) %arrayidx142.1514.1, align 4, !tbaa !14
  %115 = load i32, ptr addrspace(3) %gep.2, align 4, !tbaa !14
  %116 = zext nneg i32 %17 to i64
  %117 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %116
  %arrayidx142.2516.1 = getelementptr inbounds nuw i8, ptr addrspace(1) %117, i64 32768
  %118 = load i32, ptr addrspace(3) %arrayidx138.1.2, align 4, !tbaa !14
  %119 = load i32, ptr addrspace(3) %arrayidx138.2.2, align 4, !tbaa !14
  %120 = load i32, ptr addrspace(3) %arrayidx138.3.2, align 4, !tbaa !14
  %121 = insertelement <4 x i32> poison, i32 %115, i64 0
  %122 = insertelement <4 x i32> %121, i32 %118, i64 1
  %123 = insertelement <4 x i32> %122, i32 %119, i64 2
  %124 = insertelement <4 x i32> %123, i32 %120, i64 3
  store <4 x i32> %124, ptr addrspace(1) %arrayidx142.2516.1, align 4, !tbaa !14
  %125 = load i32, ptr addrspace(3) %gep.3, align 4, !tbaa !14
  %126 = zext nneg i32 %17 to i64
  %127 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %126
  %arrayidx142.3518.1 = getelementptr inbounds nuw i8, ptr addrspace(1) %127, i64 36864
  %128 = load i32, ptr addrspace(3) %arrayidx138.1.3, align 4, !tbaa !14
  %129 = load i32, ptr addrspace(3) %arrayidx138.2.3, align 4, !tbaa !14
  %130 = load i32, ptr addrspace(3) %arrayidx138.3.3, align 4, !tbaa !14
  %131 = insertelement <4 x i32> poison, i32 %125, i64 0
  %132 = insertelement <4 x i32> %131, i32 %128, i64 1
  %133 = insertelement <4 x i32> %132, i32 %129, i64 2
  %134 = insertelement <4 x i32> %133, i32 %130, i64 3
  store <4 x i32> %134, ptr addrspace(1) %arrayidx142.3518.1, align 4, !tbaa !14
  %135 = load i32, ptr addrspace(3) %65, align 4, !tbaa !14
  %136 = zext nneg i32 %17 to i64
  %137 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %136
  %arrayidx172.1542 = getelementptr inbounds nuw i8, ptr addrspace(1) %137, i64 40960
  %138 = load i32, ptr addrspace(3) %arrayidx167.1, align 4, !tbaa !14
  %139 = load i32, ptr addrspace(3) %arrayidx167.2, align 4, !tbaa !14
  %140 = load i32, ptr addrspace(3) %arrayidx167.3, align 4, !tbaa !14
  %141 = insertelement <4 x i32> poison, i32 %135, i64 0
  %142 = insertelement <4 x i32> %141, i32 %138, i64 1
  %143 = insertelement <4 x i32> %142, i32 %139, i64 2
  %144 = insertelement <4 x i32> %143, i32 %140, i64 3
  store <4 x i32> %144, ptr addrspace(1) %arrayidx172.1542, align 4, !tbaa !14
  %145 = load i32, ptr addrspace(3) %78, align 4, !tbaa !14
  %146 = zext nneg i32 %17 to i64
  %147 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %146
  %arrayidx172.1520.1 = getelementptr inbounds nuw i8, ptr addrspace(1) %147, i64 45056
  %148 = load i32, ptr addrspace(3) %arrayidx167.1.1, align 4, !tbaa !14
  %149 = load i32, ptr addrspace(3) %arrayidx167.2.1, align 4, !tbaa !14
  %150 = load i32, ptr addrspace(3) %arrayidx167.3.1, align 4, !tbaa !14
  %151 = insertelement <4 x i32> poison, i32 %145, i64 0
  %152 = insertelement <4 x i32> %151, i32 %148, i64 1
  %153 = insertelement <4 x i32> %152, i32 %149, i64 2
  %154 = insertelement <4 x i32> %153, i32 %150, i64 3
  store <4 x i32> %154, ptr addrspace(1) %arrayidx172.1520.1, align 4, !tbaa !14
  fence syncscope("workgroup") release
  tail call void @llvm.amdgcn.s.barrier()
  fence syncscope("workgroup") acquire
  %vecext197.1556 = extractelement <4 x i32> %19, i64 0
  store i32 %vecext197.1556, ptr addrspace(3) %invariant.gep446, align 4, !tbaa !14
  %vecext197.1.1557 = extractelement <4 x i32> %19, i64 1
  store i32 %vecext197.1.1557, ptr addrspace(3) %gep443.1, align 4, !tbaa !14
  %vecext197.2.1559 = extractelement <4 x i32> %19, i64 2
  store i32 %vecext197.2.1559, ptr addrspace(3) %gep443.2, align 4, !tbaa !14
  %vecext197.3.1561 = extractelement <4 x i32> %19, i64 3
  store i32 %vecext197.3.1561, ptr addrspace(3) %gep443.3, align 4, !tbaa !14
  %vecext197.1521.1 = extractelement <4 x i32> %20, i64 0
  store i32 %vecext197.1521.1, ptr addrspace(3) %gep447.1, align 4, !tbaa !14
  %vecext197.1.1.1 = extractelement <4 x i32> %20, i64 1
  store i32 %vecext197.1.1.1, ptr addrspace(3) %gep443.1.1, align 4, !tbaa !14
  %vecext197.2.1.1 = extractelement <4 x i32> %20, i64 2
  store i32 %vecext197.2.1.1, ptr addrspace(3) %gep443.2.1, align 4, !tbaa !14
  %vecext197.3.1.1 = extractelement <4 x i32> %20, i64 3
  store i32 %vecext197.3.1.1, ptr addrspace(3) %gep443.3.1, align 4, !tbaa !14
  %vecext197.2522.1 = extractelement <4 x i32> %21, i64 0
  store i32 %vecext197.2522.1, ptr addrspace(3) %gep447.2, align 4, !tbaa !14
  %vecext197.1.2.1 = extractelement <4 x i32> %21, i64 1
  store i32 %vecext197.1.2.1, ptr addrspace(3) %gep443.1.2, align 4, !tbaa !14
  %vecext197.2.2.1 = extractelement <4 x i32> %21, i64 2
  store i32 %vecext197.2.2.1, ptr addrspace(3) %gep443.2.2, align 4, !tbaa !14
  %vecext197.3.2.1 = extractelement <4 x i32> %21, i64 3
  store i32 %vecext197.3.2.1, ptr addrspace(3) %gep443.3.2, align 4, !tbaa !14
  %vecext197.3523.1 = extractelement <4 x i32> %22, i64 0
  store i32 %vecext197.3523.1, ptr addrspace(3) %gep447.3, align 4, !tbaa !14
  %vecext197.1.3.1 = extractelement <4 x i32> %22, i64 1
  store i32 %vecext197.1.3.1, ptr addrspace(3) %gep443.1.3, align 4, !tbaa !14
  %vecext197.2.3.1 = extractelement <4 x i32> %22, i64 2
  store i32 %vecext197.2.3.1, ptr addrspace(3) %gep443.2.3, align 4, !tbaa !14
  %vecext197.3.3.1 = extractelement <4 x i32> %22, i64 3
  store i32 %vecext197.3.3.1, ptr addrspace(3) %gep443.3.3, align 4, !tbaa !14
  %vecext223.1564 = extractelement <4 x i32> %23, i64 0
  store i32 %vecext223.1564, ptr addrspace(3) %arrayidx77, align 4, !tbaa !14
  %vecext223.1.1566 = extractelement <4 x i32> %23, i64 1
  store i32 %vecext223.1.1566, ptr addrspace(3) %arrayidx77.1, align 4, !tbaa !14
  %vecext223.2.1569 = extractelement <4 x i32> %23, i64 2
  store i32 %vecext223.2.1569, ptr addrspace(3) %arrayidx77.2, align 4, !tbaa !14
  %vecext223.3.1572 = extractelement <4 x i32> %23, i64 3
  store i32 %vecext223.3.1572, ptr addrspace(3) %arrayidx77.3, align 4, !tbaa !14
  %vecext223.1524.1 = extractelement <4 x i32> %24, i64 0
  store i32 %vecext223.1524.1, ptr addrspace(3) %arrayidx77.1512, align 4, !tbaa !14
  %vecext223.1.1.1 = extractelement <4 x i32> %24, i64 1
  store i32 %vecext223.1.1.1, ptr addrspace(3) %arrayidx77.1.1, align 4, !tbaa !14
  %vecext223.2.1.1 = extractelement <4 x i32> %24, i64 2
  store i32 %vecext223.2.1.1, ptr addrspace(3) %arrayidx77.2.1, align 4, !tbaa !14
  %vecext223.3.1.1 = extractelement <4 x i32> %24, i64 3
  store i32 %vecext223.3.1.1, ptr addrspace(3) %arrayidx77.3.1, align 4, !tbaa !14
  fence syncscope("workgroup") release
  tail call void @llvm.amdgcn.s.barrier()
  fence syncscope("workgroup") acquire
  %155 = load i32, ptr addrspace(3) %invariant.gep466, align 4, !tbaa !14
  %156 = zext nneg i32 %17 to i64
  %157 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %156
  %arrayidx142.2581 = getelementptr inbounds nuw i8, ptr addrspace(1) %157, i64 49152
  %158 = load i32, ptr addrspace(3) %arrayidx138.1, align 4, !tbaa !14
  %159 = load i32, ptr addrspace(3) %arrayidx138.2, align 4, !tbaa !14
  %160 = load i32, ptr addrspace(3) %arrayidx138.3, align 4, !tbaa !14
  %161 = insertelement <4 x i32> poison, i32 %155, i64 0
  %162 = insertelement <4 x i32> %161, i32 %158, i64 1
  %163 = insertelement <4 x i32> %162, i32 %159, i64 2
  %164 = insertelement <4 x i32> %163, i32 %160, i64 3
  store <4 x i32> %164, ptr addrspace(1) %arrayidx142.2581, align 4, !tbaa !14
  %165 = load i32, ptr addrspace(3) %gep.1, align 4, !tbaa !14
  %166 = zext nneg i32 %17 to i64
  %167 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %166
  %arrayidx142.1514.2 = getelementptr inbounds nuw i8, ptr addrspace(1) %167, i64 53248
  %168 = load i32, ptr addrspace(3) %arrayidx138.1.1, align 4, !tbaa !14
  %169 = load i32, ptr addrspace(3) %arrayidx138.2.1, align 4, !tbaa !14
  %170 = load i32, ptr addrspace(3) %arrayidx138.3.1, align 4, !tbaa !14
  %171 = insertelement <4 x i32> poison, i32 %165, i64 0
  %172 = insertelement <4 x i32> %171, i32 %168, i64 1
  %173 = insertelement <4 x i32> %172, i32 %169, i64 2
  %174 = insertelement <4 x i32> %173, i32 %170, i64 3
  store <4 x i32> %174, ptr addrspace(1) %arrayidx142.1514.2, align 4, !tbaa !14
  %175 = load i32, ptr addrspace(3) %gep.2, align 4, !tbaa !14
  %176 = zext nneg i32 %17 to i64
  %177 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %176
  %arrayidx142.2516.2 = getelementptr inbounds nuw i8, ptr addrspace(1) %177, i64 57344
  %178 = load i32, ptr addrspace(3) %arrayidx138.1.2, align 4, !tbaa !14
  %179 = load i32, ptr addrspace(3) %arrayidx138.2.2, align 4, !tbaa !14
  %180 = load i32, ptr addrspace(3) %arrayidx138.3.2, align 4, !tbaa !14
  %181 = insertelement <4 x i32> poison, i32 %175, i64 0
  %182 = insertelement <4 x i32> %181, i32 %178, i64 1
  %183 = insertelement <4 x i32> %182, i32 %179, i64 2
  %184 = insertelement <4 x i32> %183, i32 %180, i64 3
  store <4 x i32> %184, ptr addrspace(1) %arrayidx142.2516.2, align 4, !tbaa !14
  %185 = load i32, ptr addrspace(3) %gep.3, align 4, !tbaa !14
  %186 = zext nneg i32 %17 to i64
  %187 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %186
  %arrayidx142.3518.2 = getelementptr inbounds nuw i8, ptr addrspace(1) %187, i64 61440
  %188 = load i32, ptr addrspace(3) %arrayidx138.1.3, align 4, !tbaa !14
  %189 = load i32, ptr addrspace(3) %arrayidx138.2.3, align 4, !tbaa !14
  %190 = load i32, ptr addrspace(3) %arrayidx138.3.3, align 4, !tbaa !14
  %191 = insertelement <4 x i32> poison, i32 %185, i64 0
  %192 = insertelement <4 x i32> %191, i32 %188, i64 1
  %193 = insertelement <4 x i32> %192, i32 %189, i64 2
  %194 = insertelement <4 x i32> %193, i32 %190, i64 3
  store <4 x i32> %194, ptr addrspace(1) %arrayidx142.3518.2, align 4, !tbaa !14
  %195 = load i32, ptr addrspace(3) %65, align 4, !tbaa !14
  %196 = zext nneg i32 %17 to i64
  %197 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %196
  %arrayidx172.2596 = getelementptr inbounds nuw i8, ptr addrspace(1) %197, i64 65536
  %198 = load i32, ptr addrspace(3) %arrayidx167.1, align 4, !tbaa !14
  %199 = load i32, ptr addrspace(3) %arrayidx167.2, align 4, !tbaa !14
  %200 = load i32, ptr addrspace(3) %arrayidx167.3, align 4, !tbaa !14
  %201 = insertelement <4 x i32> poison, i32 %195, i64 0
  %202 = insertelement <4 x i32> %201, i32 %198, i64 1
  %203 = insertelement <4 x i32> %202, i32 %199, i64 2
  %204 = insertelement <4 x i32> %203, i32 %200, i64 3
  store <4 x i32> %204, ptr addrspace(1) %arrayidx172.2596, align 4, !tbaa !14
  %205 = load i32, ptr addrspace(3) %78, align 4, !tbaa !14
  %206 = zext nneg i32 %17 to i64
  %207 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %206
  %arrayidx172.1520.2 = getelementptr inbounds nuw i8, ptr addrspace(1) %207, i64 69632
  %208 = load i32, ptr addrspace(3) %arrayidx167.1.1, align 4, !tbaa !14
  %209 = load i32, ptr addrspace(3) %arrayidx167.2.1, align 4, !tbaa !14
  %210 = load i32, ptr addrspace(3) %arrayidx167.3.1, align 4, !tbaa !14
  %211 = insertelement <4 x i32> poison, i32 %205, i64 0
  %212 = insertelement <4 x i32> %211, i32 %208, i64 1
  %213 = insertelement <4 x i32> %212, i32 %209, i64 2
  %214 = insertelement <4 x i32> %213, i32 %210, i64 3
  store <4 x i32> %214, ptr addrspace(1) %arrayidx172.1520.2, align 4, !tbaa !14
  fence syncscope("workgroup") release
  tail call void @llvm.amdgcn.s.barrier()
  fence syncscope("workgroup") acquire
  %vecext197.2597 = extractelement <4 x i32> %early89, i64 0
  store i32 %vecext197.2597, ptr addrspace(3) %invariant.gep446, align 4, !tbaa !14
  %vecext197.1.2598 = extractelement <4 x i32> %early89, i64 1
  store i32 %vecext197.1.2598, ptr addrspace(3) %gep443.1, align 4, !tbaa !14
  %vecext197.2.2600 = extractelement <4 x i32> %early89, i64 2
  store i32 %vecext197.2.2600, ptr addrspace(3) %gep443.2, align 4, !tbaa !14
  %vecext197.3.2602 = extractelement <4 x i32> %early89, i64 3
  store i32 %vecext197.3.2602, ptr addrspace(3) %gep443.3, align 4, !tbaa !14
  %vecext197.1521.2 = extractelement <4 x i32> %early90, i64 0
  store i32 %vecext197.1521.2, ptr addrspace(3) %gep447.1, align 4, !tbaa !14
  %vecext197.1.1.2 = extractelement <4 x i32> %early90, i64 1
  store i32 %vecext197.1.1.2, ptr addrspace(3) %gep443.1.1, align 4, !tbaa !14
  %vecext197.2.1.2 = extractelement <4 x i32> %early90, i64 2
  store i32 %vecext197.2.1.2, ptr addrspace(3) %gep443.2.1, align 4, !tbaa !14
  %vecext197.3.1.2 = extractelement <4 x i32> %early90, i64 3
  store i32 %vecext197.3.1.2, ptr addrspace(3) %gep443.3.1, align 4, !tbaa !14
  %vecext197.2522.2 = extractelement <4 x i32> %early91, i64 0
  store i32 %vecext197.2522.2, ptr addrspace(3) %gep447.2, align 4, !tbaa !14
  %vecext197.1.2.2 = extractelement <4 x i32> %early91, i64 1
  store i32 %vecext197.1.2.2, ptr addrspace(3) %gep443.1.2, align 4, !tbaa !14
  %vecext197.2.2.2 = extractelement <4 x i32> %early91, i64 2
  store i32 %vecext197.2.2.2, ptr addrspace(3) %gep443.2.2, align 4, !tbaa !14
  %vecext197.3.2.2 = extractelement <4 x i32> %early91, i64 3
  store i32 %vecext197.3.2.2, ptr addrspace(3) %gep443.3.2, align 4, !tbaa !14
  %vecext197.3523.2 = extractelement <4 x i32> %early92, i64 0
  store i32 %vecext197.3523.2, ptr addrspace(3) %gep447.3, align 4, !tbaa !14
  %vecext197.1.3.2 = extractelement <4 x i32> %early92, i64 1
  store i32 %vecext197.1.3.2, ptr addrspace(3) %gep443.1.3, align 4, !tbaa !14
  %vecext197.2.3.2 = extractelement <4 x i32> %early92, i64 2
  store i32 %vecext197.2.3.2, ptr addrspace(3) %gep443.2.3, align 4, !tbaa !14
  %vecext197.3.3.2 = extractelement <4 x i32> %early92, i64 3
  store i32 %vecext197.3.3.2, ptr addrspace(3) %gep443.3.3, align 4, !tbaa !14
  %vecext223.2605 = extractelement <4 x i32> %early93, i64 0
  store i32 %vecext223.2605, ptr addrspace(3) %arrayidx77, align 4, !tbaa !14
  %vecext223.1.2 = extractelement <4 x i32> %early93, i64 1
  store i32 %vecext223.1.2, ptr addrspace(3) %arrayidx77.1, align 4, !tbaa !14
  %vecext223.2.2 = extractelement <4 x i32> %early93, i64 2
  store i32 %vecext223.2.2, ptr addrspace(3) %arrayidx77.2, align 4, !tbaa !14
  %vecext223.3.2 = extractelement <4 x i32> %early93, i64 3
  store i32 %vecext223.3.2, ptr addrspace(3) %arrayidx77.3, align 4, !tbaa !14
  %vecext223.1524.2 = extractelement <4 x i32> %early94, i64 0
  store i32 %vecext223.1524.2, ptr addrspace(3) %arrayidx77.1512, align 4, !tbaa !14
  %vecext223.1.1.2 = extractelement <4 x i32> %early94, i64 1
  store i32 %vecext223.1.1.2, ptr addrspace(3) %arrayidx77.1.1, align 4, !tbaa !14
  %vecext223.2.1.2 = extractelement <4 x i32> %early94, i64 2
  store i32 %vecext223.2.1.2, ptr addrspace(3) %arrayidx77.2.1, align 4, !tbaa !14
  %vecext223.3.1.2 = extractelement <4 x i32> %early94, i64 3
  store i32 %vecext223.3.1.2, ptr addrspace(3) %arrayidx77.3.1, align 4, !tbaa !14
  fence syncscope("workgroup") release
  tail call void @llvm.amdgcn.s.barrier()
  fence syncscope("workgroup") acquire
  %215 = load i32, ptr addrspace(3) %invariant.gep466, align 4, !tbaa !14
  %216 = zext nneg i32 %17 to i64
  %217 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %216
  %arrayidx142.3610 = getelementptr inbounds nuw i8, ptr addrspace(1) %217, i64 73728
  %218 = load i32, ptr addrspace(3) %arrayidx138.1, align 4, !tbaa !14
  %219 = load i32, ptr addrspace(3) %arrayidx138.2, align 4, !tbaa !14
  %220 = load i32, ptr addrspace(3) %arrayidx138.3, align 4, !tbaa !14
  %221 = insertelement <4 x i32> poison, i32 %215, i64 0
  %222 = insertelement <4 x i32> %221, i32 %218, i64 1
  %223 = insertelement <4 x i32> %222, i32 %219, i64 2
  %224 = insertelement <4 x i32> %223, i32 %220, i64 3
  store <4 x i32> %224, ptr addrspace(1) %arrayidx142.3610, align 4, !tbaa !14
  %225 = load i32, ptr addrspace(3) %gep.1, align 4, !tbaa !14
  %226 = zext nneg i32 %17 to i64
  %227 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %226
  %arrayidx142.1514.3 = getelementptr inbounds nuw i8, ptr addrspace(1) %227, i64 77824
  %228 = load i32, ptr addrspace(3) %arrayidx138.1.1, align 4, !tbaa !14
  %229 = load i32, ptr addrspace(3) %arrayidx138.2.1, align 4, !tbaa !14
  %230 = load i32, ptr addrspace(3) %arrayidx138.3.1, align 4, !tbaa !14
  %231 = insertelement <4 x i32> poison, i32 %225, i64 0
  %232 = insertelement <4 x i32> %231, i32 %228, i64 1
  %233 = insertelement <4 x i32> %232, i32 %229, i64 2
  %234 = insertelement <4 x i32> %233, i32 %230, i64 3
  store <4 x i32> %234, ptr addrspace(1) %arrayidx142.1514.3, align 4, !tbaa !14
  %235 = load i32, ptr addrspace(3) %gep.2, align 4, !tbaa !14
  %236 = zext nneg i32 %17 to i64
  %237 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %236
  %arrayidx142.2516.3 = getelementptr inbounds nuw i8, ptr addrspace(1) %237, i64 81920
  %238 = load i32, ptr addrspace(3) %arrayidx138.1.2, align 4, !tbaa !14
  %239 = load i32, ptr addrspace(3) %arrayidx138.2.2, align 4, !tbaa !14
  %240 = load i32, ptr addrspace(3) %arrayidx138.3.2, align 4, !tbaa !14
  %241 = insertelement <4 x i32> poison, i32 %235, i64 0
  %242 = insertelement <4 x i32> %241, i32 %238, i64 1
  %243 = insertelement <4 x i32> %242, i32 %239, i64 2
  %244 = insertelement <4 x i32> %243, i32 %240, i64 3
  store <4 x i32> %244, ptr addrspace(1) %arrayidx142.2516.3, align 4, !tbaa !14
  %245 = load i32, ptr addrspace(3) %gep.3, align 4, !tbaa !14
  %246 = zext nneg i32 %17 to i64
  %247 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %246
  %arrayidx142.3518.3 = getelementptr inbounds nuw i8, ptr addrspace(1) %247, i64 86016
  %248 = load i32, ptr addrspace(3) %arrayidx138.1.3, align 4, !tbaa !14
  %249 = load i32, ptr addrspace(3) %arrayidx138.2.3, align 4, !tbaa !14
  %250 = load i32, ptr addrspace(3) %arrayidx138.3.3, align 4, !tbaa !14
  %251 = insertelement <4 x i32> poison, i32 %245, i64 0
  %252 = insertelement <4 x i32> %251, i32 %248, i64 1
  %253 = insertelement <4 x i32> %252, i32 %249, i64 2
  %254 = insertelement <4 x i32> %253, i32 %250, i64 3
  store <4 x i32> %254, ptr addrspace(1) %arrayidx142.3518.3, align 4, !tbaa !14
  %255 = load i32, ptr addrspace(3) %65, align 4, !tbaa !14
  %256 = zext nneg i32 %17 to i64
  %257 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %256
  %arrayidx172.3625 = getelementptr inbounds nuw i8, ptr addrspace(1) %257, i64 90112
  %258 = load i32, ptr addrspace(3) %arrayidx167.1, align 4, !tbaa !14
  %259 = load i32, ptr addrspace(3) %arrayidx167.2, align 4, !tbaa !14
  %260 = load i32, ptr addrspace(3) %arrayidx167.3, align 4, !tbaa !14
  %261 = insertelement <4 x i32> poison, i32 %255, i64 0
  %262 = insertelement <4 x i32> %261, i32 %258, i64 1
  %263 = insertelement <4 x i32> %262, i32 %259, i64 2
  %264 = insertelement <4 x i32> %263, i32 %260, i64 3
  store <4 x i32> %264, ptr addrspace(1) %arrayidx172.3625, align 4, !tbaa !14
  %265 = load i32, ptr addrspace(3) %78, align 4, !tbaa !14
  %266 = zext nneg i32 %17 to i64
  %267 = getelementptr inbounds nuw [4 x i8], ptr addrspace(1) %out.coerce, i64 %266
  %arrayidx172.1520.3 = getelementptr inbounds nuw i8, ptr addrspace(1) %267, i64 94208
  %268 = load i32, ptr addrspace(3) %arrayidx167.1.1, align 4, !tbaa !14
  %269 = load i32, ptr addrspace(3) %arrayidx167.2.1, align 4, !tbaa !14
  %270 = load i32, ptr addrspace(3) %arrayidx167.3.1, align 4, !tbaa !14
  %271 = insertelement <4 x i32> poison, i32 %265, i64 0
  %272 = insertelement <4 x i32> %271, i32 %268, i64 1
  %273 = insertelement <4 x i32> %272, i32 %269, i64 2
  %274 = insertelement <4 x i32> %273, i32 %270, i64 3
  store <4 x i32> %274, ptr addrspace(1) %arrayidx172.1520.3, align 4, !tbaa !14
  fence syncscope("workgroup") release
  tail call void @llvm.amdgcn.s.barrier()
  fence syncscope("workgroup") acquire
  ret void
}

; Function Attrs: convergent mustprogress nocallback nofree nounwind willreturn
declare void @llvm.amdgcn.s.barrier() #1

; Function Attrs: mustprogress nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare noundef range(i32 0, 1024) i32 @llvm.amdgcn.workitem.id.x() #2

attributes #0 = { convergent mustprogress norecurse nounwind willreturn uwtable "amdgpu-agpr-alloc"="0" "amdgpu-flat-work-group-size"="1,256" "amdgpu-no-cluster-id-x" "amdgpu-no-cluster-id-y" "amdgpu-no-cluster-id-z" "amdgpu-no-completion-action" "amdgpu-no-default-queue" "amdgpu-no-dispatch-id" "amdgpu-no-dispatch-ptr" "amdgpu-no-flat-scratch-init" "amdgpu-no-heap-ptr" "amdgpu-no-hostcall-ptr" "amdgpu-no-implicitarg-ptr" "amdgpu-no-lds-kernel-id" "amdgpu-no-multigrid-sync-arg" "amdgpu-no-queue-ptr" "amdgpu-no-workgroup-id-x" "amdgpu-no-workgroup-id-y" "amdgpu-no-workgroup-id-z" "amdgpu-no-workitem-id-x" "amdgpu-no-workitem-id-y" "amdgpu-no-workitem-id-z" "amdgpu-no-wwm" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="gfx90a" "uniform-work-group-size" }
attributes #1 = { convergent mustprogress nocallback nofree nounwind willreturn }
attributes #2 = { mustprogress nocallback nofree nosync nounwind speculatable willreturn memory(none) }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5, !6}
!llvm.errno.tbaa = !{!7}
!opencl.ocl.version = !{!12}

!0 = !{i32 1, !"amdhsa_code_object_version", i32 600}
!1 = !{i32 1, !"amdgpu_printf_kind", !"hostcall"}
!2 = !{i32 8, !"PIC Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 1, !"wchar_size", i32 4}
!5 = !{!"clang version 24.0.0git (git@github-llvm-mi210:Vasili-Sk/llvm-mi210.git 010dc707b7a05cf4c402a4ed837a41310ec84cbb)"}
!6 = !{!"AMD clang version 22.0.0git (https://github.com/RadeonOpenCompute/llvm-project roc-7.2.1 26084 f58b06dce1f9c15707c5f808fd002e18c2accf7e)"}
!7 = !{!8, !9, i64 0}
!8 = !{!"__libc_errno", !9, i64 0}
!9 = !{!"int", !10, i64 0}
!10 = !{!"omnipotent char", !11, i64 0}
!11 = !{!"Simple C++ TBAA"}
!12 = !{i32 2, i32 0}
!13 = !{!10, !10, i64 0}
!14 = !{!9, !9, i64 0}
