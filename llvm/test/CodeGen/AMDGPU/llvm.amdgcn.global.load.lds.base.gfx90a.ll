; RUN: llc -mtriple=amdgcn-amd-amdhsa -mcpu=gfx90a -verify-machineinstrs < %s | FileCheck %s --check-prefix=GCN
; RUN: llc -mtriple=amdgcn-amd-amdhsa -mcpu=gfx90a -global-isel -verify-machineinstrs < %s | FileCheck %s --check-prefix=GCN
; RUN: llc -mtriple=amdgcn-amd-amdhsa -mcpu=gfx90a -stop-after=finalize-isel < %s | FileCheck %s --check-prefix=MEM
; RUN: llc -mtriple=amdgcn-amd-amdhsa -mcpu=gfx90a -global-isel -stop-after=instruction-select < %s | FileCheck %s --check-prefix=MEM
; RUN: llc -mtriple=amdgcn-amd-amdhsa -mcpu=gfx90a -global-isel -stop-after=amdgpu-reg-bank-legalize < %s | FileCheck %s --check-prefix=BANK

@lds = external addrspace(3) global [4096 x i8], align 16

declare void @llvm.amdgcn.global.load.lds.base(ptr addrspace(1), i32, ptr addrspace(3), i32 immarg, i32 immarg)

; The machine destination is m0 + lane_id * 4. It is not an arbitrary local
; pointer store. All widths keep the scalar global base and lane VGPR offset.
; GCN-LABEL: explicit_widths:
; GCN-NOT: v_readfirstlane
; GCN: s_mov_b32 m0,
; GCN: global_load_dword v{{[0-9]+}}, s[{{[0-9]+}}:{{[0-9]+}}] lds
; GCN: global_load_dwordx2 v{{[0-9]+}}, s[{{[0-9]+}}:{{[0-9]+}}] {{.*}}lds
; GCN: global_load_dwordx3 v{{[0-9]+}}, s[{{[0-9]+}}:{{[0-9]+}}] {{.*}}lds
; GCN: global_load_dwordx4 v{{[0-9]+}}, s[{{[0-9]+}}:{{[0-9]+}}] {{.*}}lds
; GCN-NOT: v_readfirstlane
; GCN: s_endpgm
;
; MEM-LABEL: name: explicit_widths
; MEM: GLOBAL_LOAD_LDS_DWORD_SADDR {{.*}} :: (load (s32) from %ir.base, align 1, addrspace 1), (store (s2048) into @llvm.amdgcn.kernel.explicit_widths.lds, align 1, addrspace 3)
; MEM: GLOBAL_LOAD_LDS_DWORDX2_SADDR {{.*}} :: (load (s64) from %ir.base, align 1, addrspace 1), (store (s4096) into @llvm.amdgcn.kernel.explicit_widths.lds, align 1, addrspace 3)
; MEM: GLOBAL_LOAD_LDS_DWORDX3_SADDR {{.*}} :: (load (s96) from %ir.base, align 1, addrspace 1), (store (s6144) into @llvm.amdgcn.kernel.explicit_widths.lds, align 1, addrspace 3)
; MEM: GLOBAL_LOAD_LDS_DWORDX4_SADDR {{.*}} :: (load (s128) from %ir.base, align 1, addrspace 1), (store (s8192) into @llvm.amdgcn.kernel.explicit_widths.lds, align 1, addrspace 3)
; BANK-LABEL: name: explicit_widths
; BANK: %[[BASE:[0-9]+]]:sgpr(p1) = G_LOAD
; BANK: %[[LDS:[0-9]+]]:sgpr(p3) = G_CONSTANT i32 0
; BANK: %[[OFFSET:[0-9]+]]:vgpr(i32) = COPY
; BANK: G_INTRINSIC_W_SIDE_EFFECTS intrinsic(@llvm.amdgcn.global.load.lds.base), %[[BASE]](p1), %[[OFFSET]](i32), %[[LDS]](p3), 4, 0
define amdgpu_kernel void @explicit_widths(ptr addrspace(1) inreg %base,
                                            i32 %lane_offset) #0 {
  call void @llvm.amdgcn.global.load.lds.base(ptr addrspace(1) %base, i32 %lane_offset, ptr addrspace(3) @lds, i32 4, i32 0)
  call void @llvm.amdgcn.global.load.lds.base(ptr addrspace(1) %base, i32 %lane_offset, ptr addrspace(3) @lds, i32 8, i32 1)
  call void @llvm.amdgcn.global.load.lds.base(ptr addrspace(1) %base, i32 %lane_offset, ptr addrspace(3) @lds, i32 12, i32 2)
  call void @llvm.amdgcn.global.load.lds.base(ptr addrspace(1) %base, i32 %lane_offset, ptr addrspace(3) @lds, i32 16, i32 3)
  ret void
}

; GCN-LABEL: classic:
; GCN-NOT: global_load{{.*}}lds
; GCN: global_load_dword
; GCN: global_store_dword
; GCN-NOT: global_load{{.*}}lds
; GCN: s_endpgm
define amdgpu_kernel void @classic(ptr addrspace(1) %in,
                                   ptr addrspace(1) %out) #0 {
  %x = load volatile i32, ptr addrspace(1) %in
  store volatile i32 %x, ptr addrspace(1) %out
  ret void
}

attributes #0 = { nounwind "target-cpu"="gfx90a" }

; MEM-LABEL: name: explicit_volatile
; MEM: GLOBAL_LOAD_LDS_DWORD_SADDR {{.*}} :: (volatile load (s32) from %ir.base, align 1, addrspace 1), (volatile store (s2048) into @llvm.amdgcn.kernel.explicit_volatile.lds, align 1, addrspace 3)
define amdgpu_kernel void @explicit_volatile(ptr addrspace(1) inreg %base,
                                               i32 %lane_offset) #0 {
  call void @llvm.amdgcn.global.load.lds.base(ptr addrspace(1) %base,
                                              i32 %lane_offset,
                                              ptr addrspace(3) @lds,
                                              i32 4, i32 -2147483648)
  ret void
}
