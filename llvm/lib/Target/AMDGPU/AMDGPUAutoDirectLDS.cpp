//===- AMDGPUAutoDirectLDS.cpp - Safe staged-to-direct LDS fusion ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AMDGPU.h"
#include "AMDGPUDirectLDSLayout.h"
#include "AMDGPUDirectLDSProfitability.h"
#include "AMDGPUSubtarget.h"
#include "AMDGPUTargetMachine.h"
#include "GCNSubtarget.h"
#include "Utils/AMDGPUBaseInfo.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Analysis/UniformityAnalysis.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/ConstantRange.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsAMDGPU.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"

using namespace llvm;
using namespace llvm::AMDGPU;
using namespace llvm::PatternMatch;

#define DEBUG_TYPE "amdgpu-auto-direct-lds"

namespace {

class AMDGPUAutoDirectLDSLegacy : public FunctionPass {
public:
  static char ID;
  explicit AMDGPUAutoDirectLDSLegacy(const AMDGPUTargetMachine &TM)
      : FunctionPass(ID), TM(TM) {}
  bool runOnFunction(Function &F) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<ScalarEvolutionWrapperPass>();
    AU.addRequired<UniformityInfoWrapperPass>();
    AU.setPreservesCFG();
  }

private:
  const AMDGPUTargetMachine &TM;
};

struct Candidate {
  BasicBlock *Body = nullptr;
  LoadInst *Producer = nullptr;
  GlobalVariable *LDS = nullptr;
  Value *GlobalBase = nullptr;
  SmallVector<StoreInst *, 4> Stores;
  SmallVector<LoadInst *, 4> Consumers;
  SmallVector<unsigned, 4> Payloads;
  uint64_t TripCount = 0;
  Loop *CandidateLoop = nullptr;
};

static std::optional<int64_t> evalInt(Value *V, unsigned Lane) {
  if (auto *C = dyn_cast<ConstantInt>(V))
    return C->getSExtValue();
  if (auto *II = dyn_cast<IntrinsicInst>(V)) {
    if (II->getIntrinsicID() == Intrinsic::amdgcn_workitem_id_x)
      return Lane;
    return std::nullopt;
  }
  auto *I = dyn_cast<Instruction>(V);
  if (!I)
    return std::nullopt;
  if (auto *Cast = dyn_cast<CastInst>(I)) {
    auto X = evalInt(Cast->getOperand(0), Lane);
    if (!X)
      return std::nullopt;
    unsigned Bits = Cast->getType()->getIntegerBitWidth();
    return APInt(Bits, *X, true).getSExtValue();
  }
  if (auto *BO = dyn_cast<BinaryOperator>(I)) {
    auto A = evalInt(BO->getOperand(0), Lane);
    auto B = evalInt(BO->getOperand(1), Lane);
    if (!A || !B)
      return std::nullopt;
    switch (BO->getOpcode()) {
    case Instruction::Add:
      return *A + *B;
    case Instruction::Sub:
      return *A - *B;
    case Instruction::Mul:
      return *A * *B;
    case Instruction::Shl:
      return uint64_t(*A) << *B;
    case Instruction::LShr:
      return uint64_t(*A) >> *B;
    case Instruction::And:
      return uint64_t(*A) & uint64_t(*B);
    case Instruction::Or:
      return uint64_t(*A) | uint64_t(*B);
    default:
      return std::nullopt;
    }
  }
  return std::nullopt;
}

static std::optional<int64_t> evalPointerOffset(Value *Ptr,
                                                const GlobalVariable *Base,
                                                unsigned Lane,
                                                const DataLayout &DL) {
  if (Ptr == Base)
    return 0;
  auto *GEP = dyn_cast<GetElementPtrInst>(Ptr);
  if (!GEP)
    return std::nullopt;
  auto Result = evalPointerOffset(GEP->getPointerOperand(), Base, Lane, DL);
  if (!Result)
    return std::nullopt;
  auto GTI = gep_type_begin(GEP);
  for (Value *Index : GEP->indices()) {
    if (StructType *ST = GTI.getStructTypeOrNull()) {
      auto *CI = dyn_cast<ConstantInt>(Index);
      if (!CI)
        return std::nullopt;
      *Result += DL.getStructLayout(ST)->getElementOffset(CI->getZExtValue());
    } else {
      auto N = evalInt(Index, Lane);
      if (!N)
        return std::nullopt;
      TypeSize Size = DL.getTypeAllocSize(GTI.getIndexedType());
      if (Size.isScalable())
        return std::nullopt;
      *Result += *N * int64_t(Size.getFixedValue());
    }
    ++GTI;
  }
  return Result;
}

static bool isAllowedCall(const CallBase &CB) {
  auto *II = dyn_cast<IntrinsicInst>(&CB);
  if (!II)
    return false;
  switch (II->getIntrinsicID()) {
  case Intrinsic::amdgcn_s_barrier:
  case Intrinsic::amdgcn_wave_barrier:
  case Intrinsic::fshl:
  case Intrinsic::fshr:
    return true;
  default:
    return II->doesNotAccessMemory() && !II->isConvergent();
  }
}

static bool dependsOnWorkitemID(Value *V, SmallPtrSetImpl<Value *> &Visited) {
  if (!Visited.insert(V).second)
    return false;
  if (auto *II = dyn_cast<IntrinsicInst>(V)) {
    switch (II->getIntrinsicID()) {
    case Intrinsic::amdgcn_workitem_id_x:
    case Intrinsic::amdgcn_workitem_id_y:
    case Intrinsic::amdgcn_workitem_id_z:
      return true;
    default:
      break;
    }
  }
  auto *I = dyn_cast<Instruction>(V);
  if (!I)
    return false;
  for (Value *Op : I->operands())
    if (dependsOnWorkitemID(Op, Visited))
      return true;
  return false;
}

// Screen malformed syntax before subtarget construction. A successful screen
// establishes no resource fact. The canonical AMDGPU parser and GCNSubtarget
// checks remain mandatory for every accepted candidate.
static bool hasWellFormedWavesPerEUSyntax(const Function &F) {
  Attribute A = F.getFnAttribute("amdgpu-waves-per-eu");
  if (!A.isStringAttribute())
    return true;
  auto [First, Second] = A.getValueAsString().split(',');
  unsigned Ignored = 0;
  return !First.trim().empty() && !Second.trim().empty() &&
         !First.trim().getAsInteger(0, Ignored) &&
         !Second.trim().getAsInteger(0, Ignored);
}

static bool hasExactWaveAndGroup(const Function &F) {
  if (F.getFnAttribute("target-cpu").getValueAsString() != "gfx90a")
    return false;
  StringRef Features = F.getFnAttribute("target-features").getValueAsString();
  if (Features.contains("+wavefrontsize32"))
    return false;
  Attribute Group = F.getFnAttribute("amdgpu-flat-work-group-size");
  return Group.isStringAttribute() && Group.getValueAsString() == "64,64";
}

static bool findCandidate(Function &F, LoopInfo &LI, ScalarEvolution &SE,
                          UniformityInfo &UI, Candidate &C) {
  LLVM_DEBUG(dbgs() << "auto-lds: inspect " << F.getName() << "\n");
  if (!hasExactWaveAndGroup(F) || F.size() != 3)
    return false;
  // Reject a large or negative constant global GEP before selecting a folded
  // invariant base. This prevents a large invariant displacement from being
  // hidden behind a later dynamic GEP.
  for (Instruction &I : instructions(F))
    if (auto *GEP = dyn_cast<GetElementPtrInst>(&I))
      for (Value *Index : GEP->indices())
        if (auto *CI = dyn_cast<ConstantInt>(Index)) {
          LLVM_DEBUG(dbgs() << "auto-lds: global constant index "
                            << CI->getValue() << "\n");
          if (CI->getValue().isNegative() ||
              CI->getValue().ugt(
                  APInt(CI->getValue().getBitWidth(), UINT32_MAX)))
            return false;
        }

  for (BasicBlock &BB : F) {
    Loop *L = LI.getLoopFor(&BB);
    if (!L || L->getHeader() != &BB || L->getLoopLatch() != &BB ||
        L->getNumBlocks() != 1 || L->getExitingBlock() != &BB ||
        L->getNumBackEdges() != 1 || pred_size(&BB) != 2 || succ_size(&BB) != 2)
      continue;
    BasicBlock *Preheader = L->getLoopPreheader();
    auto *PreheaderBranch =
        Preheader ? dyn_cast<UncondBrInst>(Preheader->getTerminator())
                  : nullptr;
    if (!PreheaderBranch || PreheaderBranch->getSuccessor(0) != &BB)
      continue;
    const SCEV *Backedges = SE.getBackedgeTakenCount(L);
    auto *BackedgeCount = dyn_cast<SCEVConstant>(Backedges);
    if (!BackedgeCount || BackedgeCount->getAPInt().isMaxValue())
      continue;
    APInt TripsAP = BackedgeCount->getAPInt() + 1;
    if (TripsAP.getActiveBits() > 64 || TripsAP.getZExtValue() < 2)
      continue;
    Candidate T;
    T.Body = &BB;
    T.CandidateLoop = L;
    T.TripCount = TripsAP.getZExtValue();

    for (Instruction &I : BB) {
      if (auto *LI = dyn_cast<LoadInst>(&I)) {
        unsigned AS = LI->getPointerAddressSpace();
        if (AS == 1 && LI->isSimple() &&
            !LI->getMetadata(LLVMContext::MD_nontemporal) &&
            LI->getAlign() >= Align(16) && LI->getType()->isVectorTy() &&
            cast<FixedVectorType>(LI->getType())->getNumElements() == 4 &&
            cast<VectorType>(LI->getType())
                ->getElementType()
                ->isIntegerTy(32)) {
          if (T.Producer)
            return false;
          T.Producer = LI;
        } else if (AS == 3 && LI->isSimple() &&
                   !LI->getMetadata(LLVMContext::MD_nontemporal) &&
                   LI->getType()->isIntegerTy(32)) {
          T.Consumers.push_back(LI);
        } else {
          return false;
        }
      } else if (auto *SI = dyn_cast<StoreInst>(&I)) {
        if (SI->getPointerAddressSpace() == 3 && SI->isSimple() &&
            !SI->getMetadata(LLVMContext::MD_nontemporal) &&
            SI->getValueOperand()->getType()->isIntegerTy(32))
          T.Stores.push_back(SI);
        else
          return false;
      } else if (isa<AtomicRMWInst, AtomicCmpXchgInst>(&I)) {
        return false;
      } else if (auto *CB = dyn_cast<CallBase>(&I)) {
        if (!isAllowedCall(*CB))
          return false;
      }
    }
    LLVM_DEBUG(dbgs() << "auto-lds: loop trips=" << T.TripCount << " stores="
                      << T.Stores.size() << " consumers=" << T.Consumers.size()
                      << " producer=" << !!T.Producer << "\n");
    if (!T.Producer || T.Stores.size() != 4 || T.Consumers.size() != 4)
      continue;

    auto *VT = cast<FixedVectorType>(T.Producer->getType());
    SmallPtrSet<StoreInst *, 4> SeenStores;
    for (StoreInst *SI : T.Stores) {
      auto *EE = dyn_cast<ExtractElementInst>(SI->getValueOperand());
      auto *Index = EE ? dyn_cast<ConstantInt>(EE->getIndexOperand()) : nullptr;
      if (!EE || EE->getVectorOperand() != T.Producer || !Index ||
          Index->getZExtValue() >= VT->getNumElements() || !EE->hasOneUse() ||
          !SeenStores.insert(SI).second)
        return false;
      T.Payloads.push_back(Index->getZExtValue());
    }
    if (!T.Producer->hasNUses(4))
      return false;

    Value *Obj = getUnderlyingObject(T.Stores[0]->getPointerOperand());
    T.LDS = dyn_cast<GlobalVariable>(Obj);
    if (!T.LDS || T.LDS->getAddressSpace() != 3)
      return false;
    for (StoreInst *SI : T.Stores)
      if (getUnderlyingObject(SI->getPointerOperand()) != T.LDS)
        return false;
    for (LoadInst *LI : T.Consumers)
      if (getUnderlyingObject(LI->getPointerOperand()) != T.LDS)
        return false;

    // The consumer set must read each producer destination once. This also
    // rejects partial, duplicate, and rewritten layouts before the full model.
    SmallPtrSet<Value *, 4> ConsumerPointers;
    for (LoadInst *LI : T.Consumers)
      if (!ConsumerPointers.insert(LI->getPointerOperand()).second)
        return false;
    for (StoreInst *SI : T.Stores)
      if (!ConsumerPointers.contains(SI->getPointerOperand()))
        return false;

    // Reject an escaping tile and every extra access to this LDS object.
    for (Instruction &I : instructions(F)) {
      bool RefersToTile = false;
      for (Value *Op : I.operands())
        if (Op->getType()->isPointerTy() && getUnderlyingObject(Op) == T.LDS)
          RefersToTile = true;
      if (!RefersToTile)
        continue;
      if (isa<GetElementPtrInst>(&I) || is_contained(T.Stores, &I) ||
          is_contained(T.Consumers, &I))
        continue;
      return false;
    }

    Value *Root = T.Producer->getPointerOperand();
    SmallPtrSet<Value *, 8> Visited;
    while (auto *Phi = dyn_cast<PHINode>(Root)) {
      if (!Visited.insert(Root).second)
        return false;
      Value *Entry = nullptr;
      Value *Backedge = nullptr;
      for (unsigned I = 0; I != Phi->getNumIncomingValues(); ++I) {
        if (Phi->getIncomingBlock(I) != &BB)
          Entry = Phi->getIncomingValue(I);
        else
          Backedge = Phi->getIncomingValue(I);
      }
      // Accept only the canonical linear pointer recurrence. A changing or
      // selected global base is not a proved scalar base contract.
      auto *StepGEP = dyn_cast_or_null<GetElementPtrInst>(Backedge);
      if (!Entry || !StepGEP || StepGEP->getPointerOperand() != Phi ||
          StepGEP->getNumIndices() != 1 ||
          !isa<ConstantInt>(StepGEP->idx_begin()->get()))
        return false;
      Root = Entry;
    }
    // Fold the deepest proved uniform, loop-invariant GEP into the scalar
    // base. This keeps block-scale offsets out of the 32-bit lane offset.
    Value *Search = Root;
    T.GlobalBase = nullptr;
    while (Search && Search->getType()->isPointerTy()) {
      SmallPtrSet<Value *, 16> WorkitemDeps;
      if (L->isLoopInvariant(Search) && UI.isUniformAtDef(Search) &&
          !dependsOnWorkitemID(Search, WorkitemDeps)) {
        Value *Ancestor = Search;
        while (auto *InvariantGEP = dyn_cast<GetElementPtrInst>(Ancestor)) {
          for (Value *Index : InvariantGEP->indices())
            if (auto *CI = dyn_cast<ConstantInt>(Index))
              if (CI->getValue().isNegative() ||
                  CI->getValue().ugt(
                      APInt(CI->getValue().getBitWidth(), UINT32_MAX)))
                return false;
          APInt ConstantOffset(
              F.getDataLayout().getIndexTypeSizeInBits(InvariantGEP->getType()),
              0);
          if (InvariantGEP->accumulateConstantOffset(F.getDataLayout(),
                                                     ConstantOffset) &&
              ConstantOffset.ugt(
                  APInt(ConstantOffset.getBitWidth(), UINT32_MAX - 15ULL)))
            return false;
          Ancestor = InvariantGEP->getPointerOperand();
        }
        T.GlobalBase = Search;
        break;
      }
      auto *GEP = dyn_cast<GetElementPtrInst>(Search);
      Search = GEP ? GEP->getPointerOperand() : nullptr;
    }
    if (!T.GlobalBase ||
        cast<PointerType>(T.GlobalBase->getType())->getAddressSpace() != 1)
      return false;

    const SCEV *Offset = SE.getMinusSCEV(
        SE.getSCEV(T.Producer->getPointerOperand()), SE.getSCEV(T.GlobalBase));
    if (isa<SCEVCouldNotCompute>(Offset) || !Offset->getType()->isIntegerTy())
      return false;
    ConstantRange Range = SE.getUnsignedRange(Offset);
    LLVM_DEBUG(dbgs() << "auto-lds: offset " << *Offset << " range " << Range
                      << "\n");
    APInt MaxStart(Range.getBitWidth(), UINT32_MAX - 15ULL);
    if (Range.isWrappedSet() || Range.getUnsignedMax().ugt(MaxStart))
      return false;
    LLVM_DEBUG(dbgs() << "auto-lds: structural and offset match\n");
    C = std::move(T);
    return true;
  }
  return false;
}

static bool runAutoDirectLDS(Function &F, LoopInfo &LI, ScalarEvolution &SE,
                             UniformityInfo &UI, const GCNSubtarget &ST) {
  Candidate C;
  if (!findCandidate(F, LI, SE, UI, C))
    return false;
  LLVM_DEBUG(dbgs() << "auto-lds: candidate\n");

  const DataLayout &DL = F.getParent()->getDataLayout();
  SmallVector<DirectLDSWord, 256> Producers;
  SmallVector<DirectLDSWord, 256> Consumers;
  for (unsigned Lane = 0; Lane != 64; ++Lane) {
    for (unsigned I = 0; I != 4; ++I) {
      auto PO =
          evalPointerOffset(C.Stores[I]->getPointerOperand(), C.LDS, Lane, DL);
      if (!PO || *PO < 0)
        return false;
      unsigned Payload = C.Payloads[I];
      Producers.push_back({Lane, Payload, uint64_t(*PO)});
      auto It = find_if(C.Consumers, [&](LoadInst *LI) {
        return LI->getPointerOperand() == C.Stores[I]->getPointerOperand();
      });
      if (It == C.Consumers.end())
        return false;
      auto CO = evalPointerOffset((*It)->getPointerOperand(), C.LDS, Lane, DL);
      if (!CO || *CO < 0)
        return false;
      Consumers.push_back({Lane, Payload, uint64_t(*CO)});
    }
  }

  DirectLDSLayoutProof Proof{Producers, Consumers, 1024, false, false};
  uint64_t Classic = 0;
  for (Instruction &I : *C.Body)
    if (!isa<PHINode, DbgInfoIntrinsic>(&I))
      ++Classic;
  DirectLDSCostFacts Facts;
  Facts.LoopTripCount = C.TripCount;
  Facts.ClassicInstructionsPerTrip = Classic;
  // The semantic pointer split adds one per-trip offset operation. Count it
  // even when later address selection can fold part of the expression.
  Facts.DirectInstructionsPerTrip = Classic - C.Stores.size() + 1;
  Facts.SetupInstructions = 1;
  Facts.OneTimeM0SetupInstructions = 1;
  Facts.OneTimeOtherSetupInstructions = 0;
  Facts.DirectLoadsPerTrip = 1;
  Facts.ReplacedGlobalLoadInstructionsPerTrip = 1;
  Facts.ReplacedGlobalLoadBytesPerTrip = 16;
  Facts.RemovedDSWritesPerTrip = C.Stores.size();
  Facts.RemovedGlobalAddressInstructionsPerTrip = 0;
  Facts.RemovedLDSAddressInstructionsPerTrip = 0;
  Facts.RemovedWaitInstructionsPerTrip = 0;
  Facts.RemovedOtherInstructionsPerTrip = 0;
  Facts.AddedDirectLoadInstructionsPerTrip = 1;
  Facts.AddedGlobalAddressInstructionsPerTrip = 1;
  Facts.AddedLDSAddressInstructionsPerTrip = 0;
  Facts.AddedWaitInstructionsPerTrip = 0;
  Facts.AddedHazardRepairInstructionsPerTrip = 0;
  Facts.AddedM0SetupInstructionsPerTrip = 0;
  Facts.AddedOtherInstructionsPerTrip = 0;
  Facts.ResourceProofMode =
      DirectLDSResourceProofMode::GuaranteedPreRAOccupancy;
  const unsigned RemovedPayloadDwords =
      cast<FixedVectorType>(C.Producer->getType())->getNumElements();
  // Subtracting the proved uniform base from the lane-varying producer
  // pointer creates one divergent i32 offset value.
  if (!UI.isDivergentAtDef(C.Producer->getPointerOperand()) ||
      !UI.isUniformAtDef(C.GlobalBase))
    return false;
  const unsigned AddedDivergentOffsetDwords = 1;
  Facts.RemovedExclusivePayloadDwords = RemovedPayloadDwords;
  Facts.AddedDivergentOffsetDwords = AddedDivergentOffsetDwords;
  Facts.NetDivergentDwordDelta =
      int64_t(AddedDivergentOffsetDwords) - int64_t(RemovedPayloadDwords);

  const unsigned TargetMaxWaves = ST.getMaxWavesPerEU();
  const auto Requested = AMDGPU::getIntegerPairAttribute(
      F, "amdgpu-waves-per-eu", {0, 0}, /*OnlyFirstRequired=*/true);
  const auto FlatWorkGroupSizes = ST.getFlatWorkGroupSizes(F);
  if (Requested != std::pair(TargetMaxWaves, TargetMaxWaves) ||
      FlatWorkGroupSizes != std::pair(64U, 64U))
    return false;

  uint64_t StaticLDSBytes = 0;
  SmallPtrSet<GlobalVariable *, 4> LDSGlobals;
  for (Instruction &I : instructions(F))
    for (Value *Op : I.operands())
      if (Op->getType()->isPointerTy())
        if (auto *GV = dyn_cast<GlobalVariable>(getUnderlyingObject(Op)))
          if (GV->getAddressSpace() == 3 && LDSGlobals.insert(GV).second) {
            TypeSize Size = DL.getTypeAllocSize(GV->getValueType());
            if (Size.isScalable())
              return false;
            StaticLDSBytes =
                alignTo(StaticLDSBytes, GV->getAlign().valueOrOne());
            StaticLDSBytes += Size.getFixedValue();
          }
  if (StaticLDSBytes > UINT32_MAX)
    return false;
  const auto NonRegisterOccupancy =
      ST.getOccupancyWithWorkGroupSizes(StaticLDSBytes, FlatWorkGroupSizes);
  const auto Effective =
      ST.getEffectiveWavesPerEU(Requested, FlatWorkGroupSizes, StaticLDSBytes);
  if (NonRegisterOccupancy.second != TargetMaxWaves || Effective != Requested)
    return false;

  Facts.TargetMaxOccupancyWaves = TargetMaxWaves;
  Facts.EffectiveOccupancyWaves = Effective.first;
  Facts.NonRegisterOccupancyWaves = NonRegisterOccupancy.second;
  Facts.MaxVGPRsAtTargetOccupancy = ST.getMaxNumVGPRs(F);
  Facts.MaxSGPRsAtTargetOccupancy = ST.getMaxNumSGPRs(F);
  const auto RegisterBudgetOccupancy =
      ST.computeOccupancy(F, StaticLDSBytes, *Facts.MaxSGPRsAtTargetOccupancy,
                          *Facts.MaxVGPRsAtTargetOccupancy);
  if (RegisterBudgetOccupancy.second != TargetMaxWaves)
    return false;
  Facts.RegisterBudgetOccupancyWaves = RegisterBudgetOccupancy.second;
  LLVM_DEBUG(
      dbgs() << "auto-lds: resources max-waves=" << TargetMaxWaves
             << " effective=" << Effective.first << ',' << Effective.second
             << " nonreg=" << NonRegisterOccupancy.first << ','
             << NonRegisterOccupancy.second
             << " budget=" << RegisterBudgetOccupancy.first << ','
             << RegisterBudgetOccupancy.second << " lds=" << StaticLDSBytes
             << " max-vgpr=" << *Facts.MaxVGPRsAtTargetOccupancy
             << " max-sgpr=" << *Facts.MaxSGPRsAtTargetOccupancy
             << " removed-payload-dwords=" << RemovedPayloadDwords
             << " added-divergent-dwords=" << AddedDivergentOffsetDwords
             << "\n");
  auto Profit = evaluateDirectLDSProfitability("gfx90a", 64, 16, Proof, Facts);
  LLVM_DEBUG(dbgs() << "auto-lds: profit "
                    << getDirectLDSProfitReasonName(Profit.Reason)
                    << " layout=" << unsigned(Profit.LayoutMatch) << "\n");
  if (!Profit.IsProfitable)
    return false;

  IRBuilder<> B(C.Producer);
  Value *PointerInt =
      B.CreatePtrToInt(C.Producer->getPointerOperand(), B.getInt64Ty());
  Value *BaseInt = B.CreatePtrToInt(C.GlobalBase, B.getInt64Ty());
  Value *Offset32 =
      B.CreateTrunc(B.CreateSub(PointerInt, BaseInt), B.getInt32Ty());
  Function *Intr = Intrinsic::getOrInsertDeclaration(
      F.getParent(), Intrinsic::amdgcn_global_load_lds_base);
  B.CreateCall(Intr,
               {C.GlobalBase, Offset32, C.LDS, B.getInt32(16), B.getInt32(0)});

  SmallVector<Instruction *, 4> Extracts;
  for (User *U : C.Producer->users())
    Extracts.push_back(cast<Instruction>(U));
  for (StoreInst *SI : C.Stores)
    SI->eraseFromParent();
  for (Instruction *I : Extracts)
    I->eraseFromParent();
  C.Producer->eraseFromParent();
  return true;
}

} // end anonymous namespace

char AMDGPUAutoDirectLDSLegacy::ID = 0;

INITIALIZE_PASS_BEGIN(AMDGPUAutoDirectLDSLegacy, DEBUG_TYPE,
                      "AMDGPU automatic staged-to-direct LDS fusion", false,
                      false)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(ScalarEvolutionWrapperPass)
INITIALIZE_PASS_DEPENDENCY(UniformityInfoWrapperPass)
INITIALIZE_PASS_END(AMDGPUAutoDirectLDSLegacy, DEBUG_TYPE,
                    "AMDGPU automatic staged-to-direct LDS fusion", false,
                    false)

FunctionPass *
llvm::createAMDGPUAutoDirectLDSLegacyPass(const AMDGPUTargetMachine &TM) {
  return new AMDGPUAutoDirectLDSLegacy(TM);
}

bool AMDGPUAutoDirectLDSLegacy::runOnFunction(Function &F) {
  LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  ScalarEvolution &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE();
  UniformityInfo &UI =
      getAnalysis<UniformityInfoWrapperPass>().getUniformityInfo();
  if (!hasWellFormedWavesPerEUSyntax(F))
    return false;
  return runAutoDirectLDS(
      F, LI, SE, UI,
      *static_cast<const GCNSubtarget *>(TM.getSubtargetImpl(F)));
}

PreservedAnalyses AMDGPUAutoDirectLDSPass::run(Function &F,
                                               FunctionAnalysisManager &FAM) {
  LoopInfo &LI = FAM.getResult<LoopAnalysis>(F);
  ScalarEvolution &SE = FAM.getResult<ScalarEvolutionAnalysis>(F);
  UniformityInfo &UI = FAM.getResult<UniformityInfoAnalysis>(F);
  if (!hasWellFormedWavesPerEUSyntax(F))
    return PreservedAnalyses::all();
  if (!runAutoDirectLDS(
          F, LI, SE, UI,
          *static_cast<const GCNSubtarget *>(TM.getSubtargetImpl(F))))
    return PreservedAnalyses::all();
  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>();
  return PA;
}
