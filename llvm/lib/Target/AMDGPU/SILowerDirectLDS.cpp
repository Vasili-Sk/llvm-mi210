//===- SILowerDirectLDS.cpp - Lower explicit-base direct LDS loads --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AMDGPU.h"
#include "GCNSubtarget.h"
#include "SIInstrInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/InitializePasses.h"

using namespace llvm;

#define DEBUG_TYPE "si-lower-direct-lds"

namespace {

struct M0Value {
  Register Reg;
  unsigned SubReg = 0;

  explicit operator bool() const { return Reg.isValid(); }
  bool operator==(const M0Value &Other) const {
    return Reg == Other.Reg && SubReg == Other.SubReg;
  }
};

class SILowerDirectLDSLegacy : public MachineFunctionPass {
public:
  static char ID;
  SILowerDirectLDSLegacy() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineLoopInfoWrapperPass>();
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
};

} // end anonymous namespace

char SILowerDirectLDSLegacy::ID = 0;
char &llvm::SILowerDirectLDSLegacyID = SILowerDirectLDSLegacy::ID;

INITIALIZE_PASS_BEGIN(SILowerDirectLDSLegacy, DEBUG_TYPE,
                      "SI lower explicit-base direct LDS loads", false, false)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfoWrapperPass)
INITIALIZE_PASS_END(SILowerDirectLDSLegacy, DEBUG_TYPE,
                    "SI lower explicit-base direct LDS loads", false, false)

static bool isDirectLDSPseudo(unsigned Opc) {
  switch (Opc) {
  case AMDGPU::GLOBAL_LOAD_LDS_DWORD_SADDR_BASE:
  case AMDGPU::GLOBAL_LOAD_LDS_DWORDX2_SADDR_BASE:
  case AMDGPU::GLOBAL_LOAD_LDS_DWORDX3_SADDR_BASE:
  case AMDGPU::GLOBAL_LOAD_LDS_DWORDX4_SADDR_BASE:
    return true;
  default:
    return false;
  }
}

static unsigned getRealOpcode(unsigned Opc) {
  switch (Opc) {
  case AMDGPU::GLOBAL_LOAD_LDS_DWORD_SADDR_BASE:
    return AMDGPU::GLOBAL_LOAD_LDS_DWORD_SADDR;
  case AMDGPU::GLOBAL_LOAD_LDS_DWORDX2_SADDR_BASE:
    return AMDGPU::GLOBAL_LOAD_LDS_DWORDX2_SADDR;
  case AMDGPU::GLOBAL_LOAD_LDS_DWORDX3_SADDR_BASE:
    return AMDGPU::GLOBAL_LOAD_LDS_DWORDX3_SADDR;
  case AMDGPU::GLOBAL_LOAD_LDS_DWORDX4_SADDR_BASE:
    return AMDGPU::GLOBAL_LOAD_LDS_DWORDX4_SADDR;
  default:
    llvm_unreachable("not an explicit-base direct LDS pseudo");
  }
}

static M0Value getBase(const MachineInstr &MI) {
  const MachineOperand &MO = MI.getOperand(2);
  return {MO.getReg(), MO.getSubReg()};
}

static bool modifies(const MachineInstr &MI, Register Reg,
                     const SIRegisterInfo &TRI) {
  return MI.modifiesRegister(Reg, &TRI);
}

static bool canHoistLoop(MachineLoop &L, const SIRegisterInfo &TRI,
                         M0Value &Base) {
  bool Found = false;
  for (MachineBasicBlock *MBB : L.blocks()) {
    for (MachineInstr &MI : MBB->instrs()) {
      if (isDirectLDSPseudo(MI.getOpcode())) {
        M0Value ThisBase = getBase(MI);
        if (!Found) {
          Base = ThisBase;
          Found = true;
        } else if (!(Base == ThisBase)) {
          return false;
        }
        continue;
      }
      if (modifies(MI, AMDGPU::M0, TRI))
        return false;
    }
  }
  if (!Found || !Base.Reg.isPhysical() || Base.Reg == AMDGPU::M0)
    return false;
  for (MachineBasicBlock *MBB : L.blocks())
    for (MachineInstr &MI : MBB->instrs())
      if (modifies(MI, Base.Reg, TRI))
        return false;
  return true;
}

static void hoistLoopSetups(MachineLoop &L, const SIInstrInfo &TII,
                            const SIRegisterInfo &TRI, bool ParentHoisted,
                            bool &Changed) {
  M0Value Base;
  MachineBasicBlock *Preheader = L.getLoopPreheader();
  bool Hoisted = !ParentHoisted && Preheader && canHoistLoop(L, TRI, Base);
  if (Hoisted) {
    BuildMI(*Preheader, Preheader->getFirstTerminator(), DebugLoc(),
            TII.get(AMDGPU::S_MOV_B32), AMDGPU::M0)
        .addReg(Base.Reg, RegState{}, Base.SubReg);
    Changed = true;
  }
  for (MachineLoop *SubLoop : L.getSubLoops())
    hoistLoopSetups(*SubLoop, TII, TRI, ParentHoisted || Hoisted, Changed);
}

static M0Value transferBlock(const MachineBasicBlock &MBB, M0Value Value,
                             const SIRegisterInfo &TRI) {
  for (const MachineInstr &MI : MBB.instrs()) {
    if (isDirectLDSPseudo(MI.getOpcode())) {
      Value = getBase(MI);
      continue;
    }
    if (Value && modifies(MI, Value.Reg, TRI))
      Value = {};
    if (!modifies(MI, AMDGPU::M0, TRI))
      continue;
    if ((MI.isCopy() || MI.getOpcode() == AMDGPU::S_MOV_B32) &&
        MI.getOperand(0).isReg() &&
        MI.getOperand(0).getReg() == AMDGPU::M0 &&
        MI.getOperand(1).isReg())
      Value = {MI.getOperand(1).getReg(), MI.getOperand(1).getSubReg()};
    else
      Value = {};
  }
  return Value;
}

static bool lowerDirectLDS(MachineFunction &MF, MachineLoopInfo &MLI) {
  const GCNSubtarget &ST = MF.getSubtarget<GCNSubtarget>();
  const SIInstrInfo &TII = *ST.getInstrInfo();
  const SIRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineLoop *L : MLI)
    hoistLoopSetups(*L, TII, TRI, false, Changed);

  // Compute a conservative reaching value for m0. An empty value means that
  // predecessors disagree or that an instruction can clobber m0.
  DenseMap<const MachineBasicBlock *, M0Value> In, Out;
  bool DataflowChanged = true;
  while (DataflowChanged) {
    DataflowChanged = false;
    for (MachineBasicBlock &MBB : MF) {
      M0Value NewIn;
      bool First = true;
      if (&MBB == &MF.front())
        First = false;
      for (MachineBasicBlock *Pred : MBB.predecessors()) {
        M0Value PredOut = Out.lookup(Pred);
        if (First) {
          NewIn = PredOut;
          First = false;
        } else if (!(NewIn == PredOut)) {
          NewIn = {};
        }
      }
      M0Value NewOut = transferBlock(MBB, NewIn, TRI);
      if (!(In.lookup(&MBB) == NewIn) || !(Out.lookup(&MBB) == NewOut)) {
        In[&MBB] = NewIn;
        Out[&MBB] = NewOut;
        DataflowChanged = true;
      }
    }
  }

  for (MachineBasicBlock &MBB : MF) {
    M0Value Value = In.lookup(&MBB);
    for (MachineInstr &MI : make_early_inc_range(MBB.instrs())) {
      if (!isDirectLDSPseudo(MI.getOpcode())) {
        if (Value && modifies(MI, Value.Reg, TRI))
          Value = {};
        // Track explicit m0 definitions and invalidate on all other clobbers.
        if (modifies(MI, AMDGPU::M0, TRI)) {
          if ((MI.isCopy() || MI.getOpcode() == AMDGPU::S_MOV_B32) &&
              MI.getOperand(0).isReg() &&
              MI.getOperand(0).getReg() == AMDGPU::M0 &&
              MI.getOperand(1).isReg())
            Value = {MI.getOperand(1).getReg(), MI.getOperand(1).getSubReg()};
          else
            Value = {};
        }
        continue;
      }

      M0Value Base = getBase(MI);
      if (!(Value == Base))
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(AMDGPU::S_MOV_B32),
                AMDGPU::M0)
            .addReg(Base.Reg, RegState{}, Base.SubReg);

      MI.setDesc(TII.get(getRealOpcode(MI.getOpcode())));
      MI.removeOperand(2);
      while (MI.getNumOperands() > 5)
        MI.removeOperand(5);
      MI.addImplicitDefUseOperands(MF);
      Value = Base;
      Changed = true;
    }
  }
  return Changed;
}

bool SILowerDirectLDSLegacy::runOnMachineFunction(MachineFunction &MF) {
  MachineLoopInfo &MLI = getAnalysis<MachineLoopInfoWrapperPass>().getLI();
  return lowerDirectLDS(MF, MLI);
}

PreservedAnalyses
llvm::SILowerDirectLDSPass::run(MachineFunction &MF,
                                MachineFunctionAnalysisManager &MFAM) {
  MachineLoopInfo &MLI = MFAM.getResult<MachineLoopAnalysis>(MF);
  if (!lowerDirectLDS(MF, MLI))
    return PreservedAnalyses::all();

  auto PA = getMachineFunctionPassPreservedAnalyses();
  PA.preserveSet<CFGAnalyses>();
  PA.preserve<MachineLoopAnalysis>();
  return PA;
}
