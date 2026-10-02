; RUN: llc -mtriple=amdgpu9.0a-amd-amdhsa -mcpu=gfx90a -O2 -amdgpu-direct-lds-cfg-v2-diagnostic -filetype=obj %S/Inputs/direct-lds-cfg-v2-fixture.ll -o %t.one.o 2> %t.one.report
; RUN: llc -mtriple=amdgpu9.0a-amd-amdhsa -mcpu=gfx90a -O2 -amdgpu-direct-lds-cfg-v2-diagnostic -filetype=obj %S/Inputs/direct-lds-cfg-v2-fixture.ll -o %t.two.o 2> %t.two.report
; RUN: diff %t.one.report %t.two.report
; RUN: cmp %t.one.o %t.two.o
; RUN: llc -mtriple=amdgpu9.0a-amd-amdhsa -mcpu=gfx90a -O2 -filetype=obj %S/Inputs/direct-lds-cfg-v2-fixture.ll -o %t.off.o
; RUN: cmp %t.off.o %t.one.o
; RUN: FileCheck %s --input-file=%t.one.report
; RUN: llc -mtriple=amdgpu9.0a-amd-amdhsa -mcpu=gfx90a -O2 -amdgpu-direct-lds-cfg-v2-diagnostic -filetype=null %S/Inputs/direct-lds-cfg-v2-shifted.ll 2> %t.shifted.report
; RUN: FileCheck %s --check-prefix=SHIFTED --input-file=%t.shifted.report
; RUN: llc -mtriple=amdgpu9.0a-amd-amdhsa -mcpu=gfx90a -O2 -amdgpu-direct-lds-cfg-v2-diagnostic -filetype=null %S/Inputs/direct-lds-cfg-v2-three-live.ll 2> %t.three.report
; RUN: FileCheck %s --check-prefix=THREE --input-file=%t.three.report
; RUN: %python %S/direct-lds-cfg-v2-diagnostic-negatives.py %S/Inputs/direct-lds-cfg-v2-fixture.ll %t llc
;
; CHECK: AMDGPU-DIRECT-LDS-CFG-V2 {"function":"cfg_v2_queue_extract","status":"matched","target":"gfx90a","workgroup":256,"wave_count":4,"x4_width":16,"producer_count":24,"groups_per_stage":6,"stage_count":4,"queue_slots":2,"region_size":1024
; CHECK-SAME: "producer_complete":true,"store_complete":true,"consumer_complete":true,"mapping_words":6144,"mapping_proof":true,"region_base_proof":true,"alias_proof":true,"barrier_order_proof":true,"workgroup_fence_proof":true,"producer_ssa_wait_proof":true,"wait_hazard_proof":true,"overwrite_proof":true,"queue_order_proof":true,"queue_depth_proof":true
; CHECK-SAME: "transform_ready":false,"transform_reason":"resource-256-and-atomic-stage-lowering-not-implemented","reasons":[]}

; SHIFTED: AMDGPU-DIRECT-LDS-CFG-V2 {"function":"cfg_v2_queue_extract","status":"matched"
; SHIFTED-SAME: "groups_per_stage":6,"stage_count":4,"queue_slots":2,"region_size":1024,"base_displacement":4
; SHIFTED-SAME: "mapping_words":6144,"mapping_proof":true,"region_base_proof":true
; SHIFTED-SAME: "queue_depth_proof":true
; SHIFTED-SAME: "transform_ready":false
; SHIFTED-SAME: "reasons":[]}
;
; THREE: AMDGPU-DIRECT-LDS-CFG-V2 {"function":"cfg_v2_queue_extract","status":"rejected"
; THREE-SAME: "groups_per_stage":6,"stage_count":4,"queue_slots":3
; THREE-SAME: "queue_order_proof":true,"queue_depth_proof":false
; THREE-SAME: "transform_ready":false
; THREE-SAME: "reasons":["queue-depth-not-two"]}
