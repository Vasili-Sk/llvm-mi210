; RUN: not llvm-as < %s 2>&1 | FileCheck %s

target triple = "amdgcn-amd-amdhsa"

declare void @llvm.amdgcn.global.load.lds.base(ptr addrspace(1), i32, ptr addrspace(3), i32 immarg, i32 immarg)

define void @bad_width(ptr addrspace(1) %base, i32 %offset,
                       ptr addrspace(3) %lds) {
  ; CHECK: invalid data size for gfx90a explicit load-to-LDS intrinsic; must be 4, 8, 12, or 16
  call void @llvm.amdgcn.global.load.lds.base(ptr addrspace(1) %base, i32 %offset,
                                              ptr addrspace(3) %lds, i32 5, i32 0)
  ret void
}
