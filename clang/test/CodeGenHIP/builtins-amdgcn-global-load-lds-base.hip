// REQUIRES: amdgpu-registered-target
// RUN: %clang_cc1 -triple amdgcn-amd-amdhsa -target-cpu gfx90a -fcuda-is-device -O1 -emit-llvm -o - %s | FileCheck %s

#define __device__ __attribute__((device))
#define __global__ __attribute__((global))
#define __shared__ __attribute__((shared))
#define __forceinline__ __attribute__((always_inline)) inline

// A shared-qualified helper parameter gives HIP source a local-address-space
// pointer. A shared array can bind to it without an address-space cast.
__device__ __forceinline__ void load_x4(unsigned char *base,
                                        unsigned int byte_offset,
                                        __shared__ void *lds_base) {
  __builtin_amdgcn_global_load_lds_base(base, byte_offset, lds_base, 16, 0);
}

// CHECK-LABEL: define{{.*}} void @hip_shared_array(
// CHECK: call void @llvm.amdgcn.global.load.lds.base(ptr addrspace(1) %{{.*}}, i32 %{{.*}}, ptr addrspace(3) @{{.*}}lds, i32 16, i32 0)
extern "C" __global__ void hip_shared_array(unsigned char *base,
                                 unsigned int byte_offset) {
  __shared__ unsigned char lds[4096];
  load_x4(base, byte_offset, lds);
}
