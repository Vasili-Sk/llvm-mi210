; The hidden atomic path must make no change when the source offset or final
; register occupancy contract is absent. Test both pass-manager pipelines.
;
; RUN: llc -mtriple=amdgpu9.0a-amd-amdhsa -mcpu=gfx90a -O2 -filetype=obj %S/Inputs/direct-lds-cfg-v2-fixture.ll -o %t.default.o
; RUN: llc -mtriple=amdgpu9.0a-amd-amdhsa -mcpu=gfx90a -O2 -amdgpu-direct-lds-cfg-v2-atomic -filetype=obj %S/Inputs/direct-lds-cfg-v2-fixture.ll -o %t.atomic.o 2> %t.report
; RUN: cmp %t.default.o %t.atomic.o
; RUN: FileCheck %s --input-file=%t.report
; RUN: llc -enable-new-pm=0 -mtriple=amdgpu9.0a-amd-amdhsa -mcpu=gfx90a -O2 -filetype=obj %S/Inputs/direct-lds-cfg-v2-fixture.ll -o %t.legacy-default.o
; RUN: llc -enable-new-pm=0 -mtriple=amdgpu9.0a-amd-amdhsa -mcpu=gfx90a -O2 -amdgpu-direct-lds-cfg-v2-atomic -filetype=obj %S/Inputs/direct-lds-cfg-v2-fixture.ll -o %t.legacy-atomic.o 2> %t.legacy-report
; RUN: cmp %t.legacy-default.o %t.legacy-atomic.o
; RUN: diff %t.report %t.legacy-report
;
; CHECK: AMDGPU-DIRECT-LDS-CFG-V2-ATOMIC {"function":"cfg_v2_queue_extract","status":"rejected","reason":"unproved-i32-global-offset-range","offset_proof":false,"resource_proof":false,"resource_reason":"missing-exact-occupancy-contract","ir_changed":false}
