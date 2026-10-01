; RUN: not llvm-as < %s 2>&1 | FileCheck %s

target triple = "amdgcn-amd-amdhsa"

declare void @llvm.amdgcn.global.load.lds.base(ptr, i32, ptr addrspace(1), i32 immarg, i32 immarg)

; CHECK: intrinsic argument 0 type expected ptr addrspace(1), but got ptr
define void @bad_address_spaces(ptr %base, i32 %offset,
                                ptr addrspace(1) %lds) {
  call void @llvm.amdgcn.global.load.lds.base(ptr %base, i32 %offset,
                                              ptr addrspace(1) %lds,
                                              i32 4, i32 0)
  ret void
}
