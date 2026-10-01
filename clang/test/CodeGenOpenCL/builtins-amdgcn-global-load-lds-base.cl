// RUN: %clang_cc1 -triple amdgcn-amd-amdhsa -target-cpu gfx90a -O1 -emit-llvm -o - %s | FileCheck %s

// The LDS base is uniform. Hardware adds the implicit lane_id * 4 destination.
// The global byte offset is a separate per-lane operand.

// CHECK-LABEL: define{{.*}} void @explicit_base(
// CHECK: call void @llvm.amdgcn.global.load.lds.base(ptr addrspace(1) %{{.*}}, i32 %{{.*}}, ptr addrspace(3) %{{.*}}, i32 4, i32 0)
// CHECK: call void @llvm.amdgcn.global.load.lds.base(ptr addrspace(1) %{{.*}}, i32 %{{.*}}, ptr addrspace(3) %{{.*}}, i32 8, i32 1)
// CHECK: call void @llvm.amdgcn.global.load.lds.base(ptr addrspace(1) %{{.*}}, i32 %{{.*}}, ptr addrspace(3) %{{.*}}, i32 12, i32 2)
// CHECK: call void @llvm.amdgcn.global.load.lds.base(ptr addrspace(1) %{{.*}}, i32 %{{.*}}, ptr addrspace(3) %{{.*}}, i32 16, i32 3)
kernel void explicit_base(global unsigned char *base, unsigned int byte_offset,
                          local unsigned char *lds_base) {
  __builtin_amdgcn_global_load_lds_base(base, byte_offset, lds_base, 4, 0);
  __builtin_amdgcn_global_load_lds_base(base, byte_offset, lds_base, 8, 1);
  __builtin_amdgcn_global_load_lds_base(base, byte_offset, lds_base, 12, 2);
  __builtin_amdgcn_global_load_lds_base(base, byte_offset, lds_base, 16, 3);
}
