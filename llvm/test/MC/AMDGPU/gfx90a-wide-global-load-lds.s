// RUN: llvm-mc -triple=amdgcn-amd-amdhsa -mcpu=gfx90a -show-encoding %s | FileCheck --check-prefix=GFX90A %s
// RUN: llvm-mc -triple=amdgcn-amd-amdhsa -mcpu=gfx90a -filetype=obj %s -o - | llvm-objdump -d --mcpu=gfx90a - | FileCheck --check-prefix=DIS %s
// RUN: not llvm-mc -triple=amdgcn-amd-amdhsa -mcpu=gfx908 -filetype=null %s 2>&1 | FileCheck --check-prefix=GFX908 %s

// The published MI200 ISA does not list these wide direct-to-LDS forms.
// Hardware probes on gfx90a confirmed all three encodings.

// GFX90A: global_load_dwordx2 v2, s[4:5] lds ; encoding: [0x00,0xa0,0x54,0xdc,0x02,0x00,0x04,0x00]
// DIS: global_load_dwordx2 v2, s[4:5] lds
// GFX908: error: invalid operand for instruction
 global_load_dwordx2 v2, s[4:5], lds

// GFX90A: global_load_dwordx3 v2, s[4:5] lds ; encoding: [0x00,0xa0,0x58,0xdc,0x02,0x00,0x04,0x00]
// DIS: global_load_dwordx3 v2, s[4:5] lds
// GFX908: error: invalid operand for instruction
 global_load_dwordx3 v2, s[4:5], lds

// GFX90A: global_load_dwordx4 v2, s[4:5] lds ; encoding: [0x00,0xa0,0x5c,0xdc,0x02,0x00,0x04,0x00]
// DIS: global_load_dwordx4 v2, s[4:5] lds
// GFX908: error: invalid operand for instruction
 global_load_dwordx4 v2, s[4:5], lds
