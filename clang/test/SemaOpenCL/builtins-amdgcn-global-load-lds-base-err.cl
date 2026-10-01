// RUN: %clang_cc1 -triple amdgcn-amd-amdhsa -target-cpu gfx900 -verify=gfx942 %s
// RUN: %clang_cc1 -triple amdgcn-amd-amdhsa -target-cpu gfx90a -verify=gfx90a %s
// RUN: %clang_cc1 -triple amdgcn-amd-amdhsa -target-cpu gfx942 -verify=gfx942 %s

void widths(global unsigned char *base, unsigned int lane_offset, local unsigned char *lds, unsigned int width) {
  __builtin_amdgcn_global_load_lds_base(base, lane_offset, lds, width, 0); // gfx90a-error {{argument to '__builtin_amdgcn_global_load_lds_base' must be a constant integer}} gfx942-error {{argument to '__builtin_amdgcn_global_load_lds_base' must be a constant integer}}
  __builtin_amdgcn_global_load_lds_base(base, lane_offset, lds, 1, 0); // gfx90a-error {{invalid size value}} gfx90a-note {{size must be 4, 8, 12, or 16}} gfx942-error {{explicit global-to-LDS base contract requires gfx90a}}
  __builtin_amdgcn_global_load_lds_base(base, lane_offset, lds, 4, 0); // gfx942-error {{explicit global-to-LDS base contract requires gfx90a}}
  __builtin_amdgcn_global_load_lds_base(base, lane_offset, lds, 8, 0); // gfx942-error {{explicit global-to-LDS base contract requires gfx90a}}
  __builtin_amdgcn_global_load_lds_base(base, lane_offset, lds, 12, 0); // gfx942-error {{explicit global-to-LDS base contract requires gfx90a}}
  __builtin_amdgcn_global_load_lds_base(base, lane_offset, lds, 16, 0); // gfx942-error {{explicit global-to-LDS base contract requires gfx90a}}
}

void wrong_global_base(local unsigned char *base, unsigned int lane_offset,
                       local unsigned char *lds) {
  __builtin_amdgcn_global_load_lds_base(base, lane_offset, lds, 4, 0); // gfx90a-error {{passing '__local unsigned char *__private' to parameter of type '__global void *' changes address space of pointer}} gfx942-error {{passing '__local unsigned char *__private' to parameter of type '__global void *' changes address space of pointer}}
}

void wrong_lds_base(global unsigned char *base, unsigned int lane_offset,
                    global unsigned char *lds) {
  __builtin_amdgcn_global_load_lds_base(base, lane_offset, lds, 4, 0); // gfx90a-error {{passing '__global unsigned char *__private' to parameter of type '__local void *' changes address space of pointer}} gfx942-error {{passing '__global unsigned char *__private' to parameter of type '__local void *' changes address space of pointer}}
}
