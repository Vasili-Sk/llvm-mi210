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
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Analysis/UniformityAnalysis.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/ConstantRange.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
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
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

using namespace llvm;
using namespace llvm::AMDGPU;
using namespace llvm::PatternMatch;

#define DEBUG_TYPE "amdgpu-auto-direct-lds"

static cl::opt<bool> DirectLDSDiagnostic(
    "amdgpu-direct-lds-cfg-v2-diagnostic", cl::Hidden, cl::init(false),
    cl::desc("Report diagnostic-only multi-wave direct LDS candidates"));

static cl::opt<bool> DirectLDSCFGV2Atomic(
    "amdgpu-direct-lds-cfg-v2-atomic", cl::Hidden, cl::init(false),
    cl::desc("Enable experimental atomic cfg_v2 direct LDS lowering"));

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

static std::optional<int64_t> evalInt(Value *V, unsigned Lane,
                                      Value *Scalar = nullptr,
                                      int64_t ScalarValue = 0) {
  if (V == Scalar)
    return ScalarValue;
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
    auto X = evalInt(Cast->getOperand(0), Lane, Scalar, ScalarValue);
    if (!X)
      return std::nullopt;
    unsigned Bits = Cast->getType()->getIntegerBitWidth();
    return APInt(Bits, *X, true).getSExtValue();
  }
  if (auto *BO = dyn_cast<BinaryOperator>(I)) {
    auto A = evalInt(BO->getOperand(0), Lane, Scalar, ScalarValue);
    auto B = evalInt(BO->getOperand(1), Lane, Scalar, ScalarValue);
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

static std::optional<int64_t> evalPointerOffset(Value *Ptr, const Value *Base,
                                                unsigned Lane,
                                                const DataLayout &DL,
                                                Value *Scalar = nullptr,
                                                int64_t ScalarValue = 0) {
  if (Ptr == Base)
    return 0;
  auto *GEP = dyn_cast<GetElementPtrInst>(Ptr);
  if (!GEP)
    return std::nullopt;
  auto Result = evalPointerOffset(GEP->getPointerOperand(), Base, Lane, DL,
                                  Scalar, ScalarValue);
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
      auto N = evalInt(Index, Lane, Scalar, ScalarValue);
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

struct CFGV2Access {
  Instruction *I = nullptr;
  int64_t RegionBase = 0;
  unsigned Group = 0;
  unsigned Payload = 0;
};

// Derive the base of one four-wave producer region from the access expression.
// The target layout supplies only the lane and payload placement. No absolute
// LDS coordinate is part of this proof.
static bool deriveCFGV2Address(Value *Ptr, const GlobalVariable *LDS,
                               const DataLayout &DL,
                               const DirectLDSLayout &Layout,
                               unsigned WaveCount, int64_t &RegionBase,
                               unsigned &Payload, int64_t &MinOffset,
                               int64_t &MaxOffset, int ExpectedPayload = -1) {
  const uint64_t WaveRegionBytes = Layout.getFootprintBytes();
  for (unsigned P = 0; P != Layout.getPayloadWords(); ++P) {
    if (ExpectedPayload >= 0 && P != unsigned(ExpectedPayload))
      continue;
    std::optional<int64_t> Base;
    int64_t Min = INT64_MAX, Max = INT64_MIN;
    bool Match = true;
    for (unsigned TID = 0; TID != WaveCount * Layout.getWaveSize(); ++TID) {
      auto Actual = evalPointerOffset(Ptr, LDS, TID, DL);
      auto Direct = Layout.getByteOffset(TID % Layout.getWaveSize(), P);
      if (!Actual || !Direct) {
        Match = false;
        break;
      }
      int64_t ThisBase =
          *Actual -
          int64_t(TID / Layout.getWaveSize()) * int64_t(WaveRegionBytes) -
          int64_t(*Direct);
      if (Base && *Base != ThisBase) {
        Match = false;
        break;
      }
      Base = ThisBase;
      Min = std::min(Min, *Actual);
      Max = std::max(Max, *Actual);
    }
    if (Match && Base) {
      RegionBase = *Base;
      Payload = P;
      MinOffset = Min;
      MaxOffset = Max;
      return true;
    }
  }
  return false;
}

static LoadInst *getX4Producer(Value *V, unsigned &Payload) {
  auto *EE = dyn_cast<ExtractElementInst>(V);
  if (!EE)
    return nullptr;
  auto *Index = dyn_cast<ConstantInt>(EE->getIndexOperand());
  auto *LI = dyn_cast<LoadInst>(EE->getVectorOperand());
  if (!Index || !LI || Index->getZExtValue() >= 4)
    return nullptr;
  auto *VT = dyn_cast<FixedVectorType>(LI->getType());
  if (!VT || VT->getNumElements() != 4 ||
      !VT->getElementType()->isIntegerTy(32) ||
      LI->getPointerAddressSpace() != 1)
    return nullptr;
  Payload = Index->getZExtValue();
  return LI;
}

static bool analyzeCFGV2(Function &F, ScalarEvolution &SE,
                         UniformityInfo &UI, DominatorTree &DT,
                         PostDominatorTree &PDT, bool EmitReport,
                         SmallVectorImpl<LoadInst *> *AtomicProducers = nullptr) {
  (void)SE;
  const DataLayout &DL = F.getDataLayout();
  StringRef Target = F.getFnAttribute("target-cpu").getValueAsString();
  constexpr unsigned WaveSize = 64;
  auto Layout = DirectLDSLayout::create(Target, WaveSize, 16);
  const auto WorkgroupRange = AMDGPU::getIntegerPairAttribute(
      F, "amdgpu-flat-work-group-size", {0, 0}, true);
  const unsigned Workgroup = WorkgroupRange.second;
  const unsigned WaveCount =
      Workgroup && Workgroup % WaveSize == 0 ? Workgroup / WaveSize : 0;
  const unsigned X4Width = Layout ? Layout->getWidthBytes() : 0;
  const unsigned PayloadCount = Layout ? Layout->getPayloadWords() : 0;
  const unsigned RegionSize = Layout ? Layout->getFootprintBytes() : 0;
  const uint64_t GroupSpan = uint64_t(WaveCount) * RegionSize;

  SmallVector<LoadInst *, 24> Producers;
  SmallVector<CFGV2Access, 96> Stores;
  SmallVector<CFGV2Access, 96> Consumers;
  SmallVector<Instruction *, 8> Barriers;
  SmallVector<Instruction *, 256> Ordered;
  SmallVector<std::string, 8> Reasons;
  DenseMap<const Instruction *, unsigned> Ordinal;
  GlobalVariable *LDS = nullptr;
  unsigned ExtraProducers = 0, ExtraStores = 0, ExtraConsumers = 0;
  unsigned ExtraReads = 0, Aliases = 0, Escapes = 0, UnsupportedFlags = 0;
  bool DivergentBase = false, MalformedCFG = false;
  bool InBounds = true;

  auto AddLDSAccess = [&](Instruction &I, Value *Ptr, bool IsStore) {
    auto *GV = dyn_cast<GlobalVariable>(getUnderlyingObject(Ptr));
    if (!GV) {
      ++Aliases;
      return;
    }
    if (!LDS)
      LDS = GV;
    if (GV != LDS) {
      ++Aliases;
      return;
    }
    if (!IsStore) {
      Consumers.push_back({&I, 0, 0, 0});
      return;
    }
    unsigned SourcePayload;
    LoadInst *Producer =
        getX4Producer(cast<StoreInst>(&I)->getValueOperand(), SourcePayload);
    int64_t Base, Min, Max;
    unsigned Payload;
    if (!Producer || !Layout || WaveCount != 4 ||
        !deriveCFGV2Address(Ptr, LDS, DL, *Layout, WaveCount, Base, Payload,
                            Min, Max, SourcePayload)) {
      ++ExtraStores;
      return;
    }
    TypeSize ObjectSize = DL.getTypeAllocSize(LDS->getValueType());
    if (ObjectSize.isScalable() || Min < 0 ||
        uint64_t(Max) + 4 > ObjectSize.getFixedValue())
      InBounds = false;
    Stores.push_back({&I, Base, 0, Payload});
  };

  for (Instruction &I : instructions(F)) {
    Ordinal[&I] = Ordered.size();
    Ordered.push_back(&I);
    if (auto *II = dyn_cast<IntrinsicInst>(&I))
      if (II->getIntrinsicID() == Intrinsic::amdgcn_s_barrier) {
        Barriers.push_back(&I);
        continue;
      }
    if (auto *LI = dyn_cast<LoadInst>(&I)) {
      if (LI->isVolatile() || LI->isAtomic() ||
          LI->getMetadata(LLVMContext::MD_nontemporal))
        ++UnsupportedFlags;
      if (auto *VT = dyn_cast<FixedVectorType>(LI->getType());
          VT && VT->getNumElements() == 4 &&
          VT->getElementType()->isIntegerTy(32) &&
          LI->getPointerAddressSpace() == 1) {
        Producers.push_back(LI);
        Value *Base = getUnderlyingObject(LI->getPointerOperand());
        if (UI.isDivergentAtDef(Base))
          DivergentBase = true;
        continue;
      }
      if (LI->getPointerAddressSpace() == 3) {
        AddLDSAccess(I, LI->getPointerOperand(), false);
        continue;
      }
      // Kernel argument loads are ABI materialization, not candidate memory.
      if (LI->getPointerAddressSpace() != 4 && LI->mayReadFromMemory())
        ++ExtraReads;
    }
    if (auto *SI = dyn_cast<StoreInst>(&I)) {
      if (SI->isVolatile() || SI->isAtomic() ||
          SI->getMetadata(LLVMContext::MD_nontemporal))
        ++UnsupportedFlags;
      if (SI->getPointerAddressSpace() == 3)
        AddLDSAccess(I, SI->getPointerOperand(), true);
    }
  }

  if (LDS) {
    SmallVector<Value *, 32> Worklist{LDS};
    SmallPtrSet<Value *, 32> Seen;
    while (!Worklist.empty()) {
      Value *V = Worklist.pop_back_val();
      if (!Seen.insert(V).second)
        continue;
      for (User *U : V->users()) {
        if (isa<GetElementPtrInst>(U) || isa<BitCastInst>(U) ||
            isa<AddrSpaceCastInst>(U) || isa<ConstantExpr>(U)) {
          Worklist.push_back(cast<Value>(U));
          continue;
        }
        if (auto *LI = dyn_cast<LoadInst>(U))
          if (LI->getPointerOperand() == V)
            continue;
        if (auto *SI = dyn_cast<StoreInst>(U))
          if (SI->getPointerOperand() == V)
            continue;
        ++Escapes;
      }
    }
  }

  SmallVector<int64_t, 8> StoreBases, ConsumerBases;
  for (const CFGV2Access &A : Stores)
    StoreBases.push_back(A.RegionBase);
  auto Unique = [](SmallVectorImpl<int64_t> &V) {
    llvm::sort(V);
    V.erase(std::unique(V.begin(), V.end()), V.end());
  };
  Unique(StoreBases);
  SmallVector<CFGV2Access, 96> MatchedConsumers;
  for (CFGV2Access &A : Consumers) {
    bool Found = false;
    for (unsigned P = 0; Layout && P != PayloadCount; ++P) {
      int64_t Base, Min, Max;
      unsigned Payload;
      if (!deriveCFGV2Address(cast<LoadInst>(A.I)->getPointerOperand(), LDS, DL,
                              *Layout, WaveCount, Base, Payload, Min, Max, P) ||
          !llvm::is_contained(StoreBases, Base))
        continue;
      if (Found) {
        Found = false;
        break;
      }
      Found = true;
      A.RegionBase = Base;
      A.Payload = Payload;
      TypeSize ObjectSize = DL.getTypeAllocSize(LDS->getValueType());
      if (ObjectSize.isScalable() || Min < 0 ||
          uint64_t(Max) + 4 > ObjectSize.getFixedValue())
        InBounds = false;
    }
    if (!Found)
      ++ExtraConsumers;
    else {
      ConsumerBases.push_back(A.RegionBase);
      MatchedConsumers.push_back(A);
    }
  }
  Consumers = std::move(MatchedConsumers);
  Unique(ConsumerBases);
  const bool EqualRegionBases = StoreBases == ConsumerBases;
  SmallVector<int64_t, 8> RegionBases = StoreBases;
  const unsigned GroupsPerStage = RegionBases.size();
  const int64_t BaseDisplacement =
      RegionBases.empty() ? 0 : RegionBases.front();
  bool RegionProof = EqualRegionBases && !RegionBases.empty() &&
                     (BaseDisplacement & 3) == 0 && GroupSpan != 0 && InBounds;
  for (unsigned G = 0; G != RegionBases.size(); ++G)
    RegionProof &=
        RegionBases[G] == BaseDisplacement + int64_t(uint64_t(G) * GroupSpan);
  auto AssignGroup = [&](CFGV2Access &A) {
    auto It = llvm::lower_bound(RegionBases, A.RegionBase);
    if (It == RegionBases.end() || *It != A.RegionBase) {
      RegionProof = false;
      return;
    }
    A.Group = It - RegionBases.begin();
  };
  for (CFGV2Access &A : Stores)
    AssignGroup(A);
  for (CFGV2Access &A : Consumers)
    AssignGroup(A);

  const unsigned AccessesPerStage = GroupsPerStage * PayloadCount;
  const unsigned StoreStageCount =
      AccessesPerStage && Stores.size() % AccessesPerStage == 0
          ? Stores.size() / AccessesPerStage
          : 0;
  const unsigned ConsumerStageCount =
      AccessesPerStage && Consumers.size() % AccessesPerStage == 0
          ? Consumers.size() / AccessesPerStage
          : 0;
  const unsigned StageCount =
      StoreStageCount == ConsumerStageCount ? StoreStageCount : 0;
  const uint64_t MappingWords =
      uint64_t(GroupsPerStage) * WaveCount * WaveSize * PayloadCount;

  if (!LDS)
    Reasons.push_back("missing-lds-base");
  if (Target != "gfx90a")
    Reasons.push_back("target-not-gfx90a");
  StringRef Features = F.getFnAttribute("target-features").getValueAsString();
  if (Features.contains("+wavefrontsize32"))
    Reasons.push_back("wave-not-64");
  if (Workgroup != 256 || WaveCount != 4)
    Reasons.push_back("workgroup-not-256");
  if (Producers.size() != 24)
    Reasons.push_back("producer-count");
  if (GroupsPerStage != 6 || StageCount != 4)
    Reasons.push_back("stage-or-group-count");
  if (Stores.size() != StageCount * AccessesPerStage)
    Reasons.push_back("store-count");
  if (Consumers.size() != StageCount * AccessesPerStage)
    Reasons.push_back("consumer-count");
  if (Barriers.size() != StageCount * 2)
    Reasons.push_back("barrier-count");
  if (DivergentBase)
    Reasons.push_back("divergent-base");
  if (Aliases || Escapes)
    Reasons.push_back("alias-or-escape");
  if (ExtraProducers || ExtraStores || ExtraConsumers || ExtraReads)
    Reasons.push_back("extra-memory-access");
  if (UnsupportedFlags)
    Reasons.push_back("unsupported-memory-flags");
  if (!RegionProof)
    Reasons.push_back("region-base-mismatch");

  DenseMap<LoadInst *, SmallVector<CFGV2Access, 4>> ProducerStores;
  for (CFGV2Access A : Stores) {
    unsigned P;
    LoadInst *Producer =
        getX4Producer(cast<StoreInst>(A.I)->getValueOperand(), P);
    if (!Producer || P != A.Payload) {
      ++ExtraStores;
      continue;
    }
    ProducerStores[Producer].push_back(A);
  }
  bool ProducerComplete = Producers.size() == 24;
  for (LoadInst *P : Producers) {
    auto It = ProducerStores.find(P);
    if (It == ProducerStores.end() || It->second.size() != PayloadCount) {
      ProducerComplete = false;
      ++ExtraProducers;
      continue;
    }
    BitVector Seen(PayloadCount);
    unsigned G = It->second.front().Group;
    for (const CFGV2Access &A : It->second) {
      if (A.Group != G || A.Payload >= PayloadCount || Seen.test(A.Payload))
        ProducerComplete = false;
      else
        Seen.set(A.Payload);
    }
    ProducerComplete &= Seen.count() == PayloadCount;
  }
  if (!ProducerComplete)
    Reasons.push_back("incomplete-producer-store");

  llvm::sort(Stores, [&](const CFGV2Access &A, const CFGV2Access &B) {
    return Ordinal[A.I] < Ordinal[B.I];
  });
  llvm::sort(Consumers, [&](const CFGV2Access &A, const CFGV2Access &B) {
    return Ordinal[A.I] < Ordinal[B.I];
  });

  bool MappingProof = Layout && ProducerComplete && RegionProof &&
                      GroupsPerStage == 6 && StageCount == 4;
  bool ConsumerComplete = Consumers.size() == StageCount * AccessesPerStage;
  bool BarrierOrderProof = Barriers.size() == StageCount * 2;
  bool FenceProof = BarrierOrderProof;
  bool OverwriteProof = BarrierOrderProof;
  bool CFGProof = F.size() == 1;
  bool ProducerSSAWaitProof = ProducerComplete;
  SmallVector<std::pair<unsigned, unsigned>, 4> LoadPublish;
  SmallVector<std::pair<unsigned, unsigned>, 4> PublishConsume;
  SmallVector<unsigned, 4> StageFirstLoad(StageCount, UINT_MAX);
  SmallVector<unsigned, 4> StageLastStore(StageCount, 0);

  const SyncScope::ID WorkgroupScope =
      F.getContext().getOrInsertSyncScopeID("workgroup");
  auto HasRelease = [&](Instruction *Barrier) {
    auto *FI = dyn_cast_or_null<FenceInst>(Barrier->getPrevNode());
    return FI && FI->getSyncScopeID() == WorkgroupScope &&
           (FI->getOrdering() == AtomicOrdering::Release ||
            FI->getOrdering() == AtomicOrdering::AcquireRelease ||
            FI->getOrdering() == AtomicOrdering::SequentiallyConsistent);
  };
  auto HasAcquire = [&](Instruction *Barrier) {
    auto *FI = dyn_cast_or_null<FenceInst>(Barrier->getNextNode());
    return FI && FI->getSyncScopeID() == WorkgroupScope &&
           (FI->getOrdering() == AtomicOrdering::Acquire ||
            FI->getOrdering() == AtomicOrdering::AcquireRelease ||
            FI->getOrdering() == AtomicOrdering::SequentiallyConsistent);
  };
  for (Instruction *Barrier : Barriers)
    FenceProof &= HasRelease(Barrier) && HasAcquire(Barrier);

  for (unsigned Stage = 0;
       Stage != StageCount && Stores.size() >= (Stage + 1) * AccessesPerStage &&
       Consumers.size() >= (Stage + 1) * AccessesPerStage;
       ++Stage) {
    ArrayRef<CFGV2Access> SS(Stores.data() + Stage * AccessesPerStage,
                             AccessesPerStage);
    ArrayRef<CFGV2Access> CC(Consumers.data() + Stage * AccessesPerStage,
                             AccessesPerStage);
    BitVector StoreKeys(AccessesPerStage), ConsumerKeys(AccessesPerStage);
    unsigned MinLoad = UINT_MAX, MaxLoad = 0, MinStore = UINT_MAX, MaxStore = 0;
    unsigned MinConsumer = UINT_MAX, MaxConsumer = 0;
    for (const CFGV2Access &A : SS) {
      unsigned Key = A.Group * PayloadCount + A.Payload;
      if (Key >= AccessesPerStage || StoreKeys.test(Key))
        MappingProof = false;
      else
        StoreKeys.set(Key);
      unsigned P;
      LoadInst *Prod =
          getX4Producer(cast<StoreInst>(A.I)->getValueOperand(), P);
      if (!Prod) {
        MappingProof = false;
        ProducerSSAWaitProof = false;
        continue;
      }
      MinLoad = std::min(MinLoad, Ordinal[Prod]);
      MaxLoad = std::max(MaxLoad, Ordinal[Prod]);
      MinStore = std::min(MinStore, Ordinal[A.I]);
      MaxStore = std::max(MaxStore, Ordinal[A.I]);
      if (!DT.dominates(Prod, A.I)) {
        CFGProof = false;
        ProducerSSAWaitProof = false;
      }
    }
    for (const CFGV2Access &A : CC) {
      unsigned Key = A.Group * PayloadCount + A.Payload;
      if (Key >= AccessesPerStage || ConsumerKeys.test(Key))
        ConsumerComplete = false;
      else
        ConsumerKeys.set(Key);
      MinConsumer = std::min(MinConsumer, Ordinal[A.I]);
      MaxConsumer = std::max(MaxConsumer, Ordinal[A.I]);
    }
    if (StoreKeys.count() != AccessesPerStage ||
        ConsumerKeys.count() != AccessesPerStage) {
      MappingProof = false;
      ConsumerComplete = false;
    }
    if (BarrierOrderProof) {
      Instruction *PublishBarrier = Barriers[Stage * 2];
      Instruction *ConsumeBarrier = Barriers[Stage * 2 + 1];
      if (!(MaxStore < Ordinal[PublishBarrier] &&
            Ordinal[PublishBarrier] < MinConsumer &&
            MaxConsumer < Ordinal[ConsumeBarrier]))
        BarrierOrderProof = false;
      for (const CFGV2Access &A : SS)
        if (!DT.dominates(A.I, PublishBarrier))
          CFGProof = false;
      for (const CFGV2Access &A : CC)
        if (!PDT.dominates(ConsumeBarrier->getParent(), A.I->getParent()))
          CFGProof = false;
      if (Stage + 1 < StageCount &&
          !(Ordinal[ConsumeBarrier] <
            Ordinal[Stores[(Stage + 1) * AccessesPerStage].I]))
        OverwriteProof = false;
    }
    StageFirstLoad[Stage] = MinLoad;
    StageLastStore[Stage] = MaxStore;
    LoadPublish.push_back({MinStore - MinLoad, MaxStore - MaxLoad});
    PublishConsume.push_back({MinConsumer - MaxStore, MaxConsumer - MinStore});

    SmallVector<DirectLDSWord, 256> ProducerWords, ConsumerWords;
    for (unsigned G = 0; G != GroupsPerStage; ++G) {
      ProducerWords.clear();
      ConsumerWords.clear();
      for (unsigned Lane = 0; Lane != WaveSize; ++Lane)
        for (unsigned P = 0; P != PayloadCount; ++P) {
          uint64_t O = *Layout->getByteOffset(Lane, P);
          ProducerWords.push_back({Lane, P, O});
          ConsumerWords.push_back({Lane, P, O});
        }
      DirectLDSLayoutProof Proof{ProducerWords, ConsumerWords, RegionSize,
                                 false, false};
      if (matchDirectLDSLayout(*Layout, Proof) != DirectLDSLayoutMatch::Match)
        MappingProof = false;
    }
  }

  if (!ConsumerComplete)
    Reasons.push_back("incomplete-or-duplicate-consumer");
  if (!MappingProof)
    Reasons.push_back("layout-or-inverse-mismatch");
  if (!CFGProof) {
    Reasons.push_back("control-flow-gap");
    MalformedCFG = true;
  }
  if (!BarrierOrderProof)
    Reasons.push_back("incomplete-barrier-order");
  if (!FenceProof)
    Reasons.push_back("workgroup-fence-order");
  if (!OverwriteProof)
    Reasons.push_back("early-overwrite");

  bool QueueOrderProof = ProducerComplete && StageCount == 4;
  Value *StageStride = nullptr;
  for (Argument &A : F.args())
    if (A.getType()->isIntegerTy()) {
      if (StageStride)
        StageStride = nullptr;
      else
        StageStride = &A;
    }
  Value *KernargScalar = nullptr;
  bool MultipleKernargScalars = false;
  for (Instruction &I : instructions(F))
    if (auto *LI = dyn_cast<LoadInst>(&I))
      if (LI->getType()->isIntegerTy(32) && LI->getPointerAddressSpace() == 4) {
        if (KernargScalar)
          MultipleKernargScalars = true;
        KernargScalar = LI;
      }
  if (KernargScalar && !MultipleKernargScalars)
    StageStride = KernargScalar;
  DenseMap<LoadInst *, unsigned> ProducerStage;
  SmallVector<unsigned, 4> StageCounts(StageCount, 0);
  if (!StageStride)
    QueueOrderProof = false;
  if (QueueOrderProof) {
    for (LoadInst *P : Producers) {
      Value *Base = getUnderlyingObject(P->getPointerOperand());
      auto O0 = evalPointerOffset(P->getPointerOperand(), Base, 0, DL,
                                  StageStride, 0);
      auto O1 = evalPointerOffset(P->getPointerOperand(), Base, 0, DL,
                                  StageStride, 1);
      if (!O0 || !O1 || *O1 < *O0 || ((*O1 - *O0) % X4Width) != 0) {
        QueueOrderProof = false;
        break;
      }
      unsigned Stage = (*O1 - *O0) / X4Width;
      if (Stage >= StageCount) {
        QueueOrderProof = false;
        break;
      }
      ProducerStage[P] = Stage;
      ++StageCounts[Stage];
    }
    for (unsigned S = 0; S != StageCount; ++S)
      QueueOrderProof &= StageCounts[S] == GroupsPerStage;
  }
  if (QueueOrderProof) {
    for (unsigned S = 0; S != StageCount; ++S) {
      unsigned PublishedStage = UINT_MAX;
      for (unsigned I = S * AccessesPerStage; I != (S + 1) * AccessesPerStage;
           ++I) {
        unsigned P;
        LoadInst *Prod =
            getX4Producer(cast<StoreInst>(Stores[I].I)->getValueOperand(), P);
        auto It = ProducerStage.find(Prod);
        if (It == ProducerStage.end() ||
            (PublishedStage != UINT_MAX && PublishedStage != It->second)) {
          QueueOrderProof = false;
          break;
        }
        PublishedStage = It->second;
      }
      QueueOrderProof &= PublishedStage == S;
    }
  }

  unsigned QueueSlots = 0;
  if (QueueOrderProof) {
    for (unsigned O = 0; O != Ordered.size(); ++O) {
      unsigned Live = 0;
      for (unsigned S = 0; S != StageCount; ++S)
        Live += StageFirstLoad[S] <= O && O <= StageLastStore[S];
      QueueSlots = std::max(QueueSlots, Live);
    }
  }
  const bool QueueDepthProof = QueueOrderProof && QueueSlots == 2;
  if (!QueueOrderProof)
    Reasons.push_back("queue-order");
  if (QueueOrderProof && !QueueDepthProof)
    Reasons.push_back("queue-depth-not-two");

  const bool StoreComplete = StageCount != 0 &&
                             Stores.size() == StageCount * AccessesPerStage &&
                             ExtraStores == 0;
  ConsumerComplete &= StageCount != 0 && ExtraConsumers == 0;
  bool AliasProof = LDS && !Aliases && !Escapes && !ExtraStores &&
                    !ExtraConsumers && !UnsupportedFlags && InBounds;
  bool WaitHazardProof = ProducerSSAWaitProof && BarrierOrderProof &&
                         FenceProof && OverwriteProof && CFGProof;
  bool Candidate = Reasons.empty();
  if (EmitReport) {
  errs() << "AMDGPU-DIRECT-LDS-CFG-V2 {\"function\":\"" << F.getName()
         << "\",\"status\":\"" << (Candidate ? "matched" : "rejected")
         << "\",\"target\":\"" << Target << "\",\"workgroup\":" << Workgroup
         << ",\"wave_count\":" << WaveCount << ",\"x4_width\":" << X4Width
         << ",\"producer_count\":" << Producers.size()
         << ",\"groups_per_stage\":" << GroupsPerStage
         << ",\"stage_count\":" << StageCount
         << ",\"queue_slots\":" << QueueSlots
         << ",\"region_size\":" << RegionSize
         << ",\"base_displacement\":" << BaseDisplacement
         << ",\"producer_complete\":" << (ProducerComplete ? "true" : "false")
         << ",\"store_complete\":" << (StoreComplete ? "true" : "false")
         << ",\"consumer_complete\":" << (ConsumerComplete ? "true" : "false")
         << ",\"mapping_words\":" << (MappingProof ? MappingWords : 0)
         << ",\"mapping_proof\":" << (MappingProof ? "true" : "false")
         << ",\"region_base_proof\":" << (RegionProof ? "true" : "false")
         << ",\"alias_proof\":" << (AliasProof ? "true" : "false")
         << ",\"barrier_order_proof\":"
         << (BarrierOrderProof ? "true" : "false")
         << ",\"workgroup_fence_proof\":" << (FenceProof ? "true" : "false")
         << ",\"producer_ssa_wait_proof\":"
         << (ProducerSSAWaitProof ? "true" : "false")
         << ",\"wait_hazard_proof\":" << (WaitHazardProof ? "true" : "false")
         << ",\"overwrite_proof\":" << (OverwriteProof ? "true" : "false")
         << ",\"queue_order_proof\":" << (QueueOrderProof ? "true" : "false")
         << ",\"queue_depth_proof\":" << (QueueDepthProof ? "true" : "false")
         << ",\"queue_live_intervals\":[";
  for (unsigned I = 0; I != StageCount; ++I) {
    if (I)
      errs() << ',';
    errs() << '[' << StageFirstLoad[I] << ',' << StageLastStore[I] << ']';
  }
  errs() << "]"
         << ",\"extra_producers\":" << ExtraProducers
         << ",\"extra_consumers\":" << ExtraConsumers
         << ",\"extra_stores\":" << ExtraStores
         << ",\"extra_reads\":" << ExtraReads << ",\"aliases\":" << Aliases
         << ",\"escapes\":" << Escapes
         << ",\"unsupported_memory_flags\":" << UnsupportedFlags
         << ",\"control_flow_gaps\":" << (MalformedCFG ? 1 : 0)
         << ",\"load_to_publish_ir_distance\":[";
  for (unsigned I = 0; I != LoadPublish.size(); ++I) {
    if (I)
      errs() << ',';
    errs() << '[' << LoadPublish[I].first << ',' << LoadPublish[I].second
           << ']';
  }
  errs() << "],\"publish_to_consume_ir_distance\":[";
  for (unsigned I = 0; I != PublishConsume.size(); ++I) {
    if (I)
      errs() << ',';
    errs() << '[' << PublishConsume[I].first << ',' << PublishConsume[I].second
           << ']';
  }
  errs() << "],\"transform_ready\":false,\"transform_reason\":"
            "\"resource-256-and-atomic-stage-lowering-not-implemented\","
            "\"reasons\":[";
  for (unsigned I = 0; I != Reasons.size(); ++I) {
    if (I)
      errs() << ',';
    errs() << '\"' << Reasons[I] << '\"';
  }
  errs() << "]}\n";
  }
  if (Candidate && AtomicProducers)
    AtomicProducers->append(Producers.begin(), Producers.end());
  return Candidate;
}

// The atomic path must reject before it builds IR when either the 32-bit
// source offset or the final-register occupancy contract is absent. The
// current cfg_v2 fixture intentionally reaches this gate with wrapping i32
// stage arithmetic and no waves-per-EU contract.
static bool validateCFGV2AtomicPrerequisites(Function &F, ScalarEvolution &SE,
                                             UniformityInfo &UI,
                                             const GCNSubtarget &ST,
                                             ArrayRef<LoadInst *> Producers) {
  const DataLayout &DL = F.getDataLayout();
  StringRef Reason;
  for (LoadInst *P : Producers) {
    Value *Base = getUnderlyingObject(P->getPointerOperand());
    if (!Base || !Base->getType()->isPointerTy() ||
        cast<PointerType>(Base->getType())->getAddressSpace() != 1 ||
        !UI.isUniformAtDef(Base)) {
      Reason = "missing-uniform-global-base";
      break;
    }
    const SCEV *Offset =
        SE.getMinusSCEV(SE.getSCEV(P->getPointerOperand()), SE.getSCEV(Base));
    if (isa<SCEVCouldNotCompute>(Offset) || !Offset->getType()->isIntegerTy()) {
      Reason = "unknown-global-offset";
      break;
    }
    ConstantRange Range = SE.getUnsignedRange(Offset);
    APInt MaxStart(Range.getBitWidth(), UINT32_MAX - 15ULL);
    if (Range.isWrappedSet() || Range.getUnsignedMax().ugt(MaxStart)) {
      Reason = "unproved-i32-global-offset-range";
      break;
    }
  }

  const bool OffsetProof = Reason.empty();
  StringRef ResourceReason;
  Attribute Waves = F.getFnAttribute("amdgpu-waves-per-eu");
  if (!Waves.isStringAttribute())
    ResourceReason = "missing-exact-occupancy-contract";
  if (ResourceReason.empty()) {
    auto Requested = AMDGPU::getIntegerPairAttribute(
        F, "amdgpu-waves-per-eu", {0, 0}, /*OnlyFirstRequired=*/false);
    if (!Requested.first || Requested.first != Requested.second) {
      ResourceReason = "missing-exact-occupancy-contract";
    } else {
      uint64_t StaticLDSBytes = 0;
      SmallPtrSet<GlobalVariable *, 4> Seen;
      for (Instruction &I : instructions(F))
        for (Value *Op : I.operands())
          if (Op->getType()->isPointerTy())
            if (auto *GV = dyn_cast<GlobalVariable>(getUnderlyingObject(Op)))
              if (GV->getAddressSpace() == 3 && Seen.insert(GV).second) {
                TypeSize Size = DL.getTypeAllocSize(GV->getValueType());
                if (Size.isScalable()) {
                  ResourceReason = "unknown-static-lds";
                  break;
                }
                StaticLDSBytes =
                    alignTo(StaticLDSBytes, GV->getAlign().valueOrOne());
                StaticLDSBytes += Size.getFixedValue();
              }
      auto Groups = ST.getFlatWorkGroupSizes(F);
      auto NonRegister =
          ST.getOccupancyWithWorkGroupSizes(StaticLDSBytes, Groups);
      auto Effective =
          ST.getEffectiveWavesPerEU(Requested, Groups, StaticLDSBytes);
      if (ResourceReason.empty() &&
          (Effective != Requested || NonRegister.second < Requested.second))
        ResourceReason = "unproved-nonregister-occupancy";
      if (ResourceReason.empty()) {
        unsigned MaxVGPR = ST.getMaxNumVGPRs(F);
        unsigned MaxSGPR = ST.getMaxNumSGPRs(F);
        auto Budget = ST.computeOccupancy(F, StaticLDSBytes, MaxSGPR, MaxVGPR);
        if (Budget.second < Requested.second)
          ResourceReason = "unproved-register-budget";
      }
    }
  }

  const bool ResourceProof = ResourceReason.empty();
  if (Reason.empty())
    Reason =
        ResourceReason.empty() ? "atomic-plan-not-implemented" : ResourceReason;
  errs() << "AMDGPU-DIRECT-LDS-CFG-V2-ATOMIC {\"function\":\"" << F.getName()
         << "\",\"status\":\"rejected\",\"reason\":\"" << Reason
         << "\",\"offset_proof\":" << (OffsetProof ? "true" : "false")
         << ",\"resource_proof\":" << (ResourceProof ? "true" : "false")
         << ",\"resource_reason\":\"" << ResourceReason
         << "\",\"ir_changed\":false}\n";
  return false;
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
  if (DirectLDSDiagnostic || DirectLDSCFGV2Atomic) {
    DominatorTree DT(F);
    PostDominatorTree PDT(F);
    SmallVector<LoadInst *, 24> AtomicProducers;
    bool Matched = analyzeCFGV2(F, SE, UI, DT, PDT, DirectLDSDiagnostic,
                                &AtomicProducers);
    if (DirectLDSCFGV2Atomic && Matched && hasWellFormedWavesPerEUSyntax(F))
      validateCFGV2AtomicPrerequisites(
          F, SE, UI, *static_cast<const GCNSubtarget *>(TM.getSubtargetImpl(F)),
          AtomicProducers);
  }
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
  if (DirectLDSDiagnostic || DirectLDSCFGV2Atomic) {
    SmallVector<LoadInst *, 24> AtomicProducers;
    bool Matched = analyzeCFGV2(
        F, SE, UI, FAM.getResult<DominatorTreeAnalysis>(F),
        FAM.getResult<PostDominatorTreeAnalysis>(F), DirectLDSDiagnostic,
        &AtomicProducers);
    if (DirectLDSCFGV2Atomic && Matched && hasWellFormedWavesPerEUSyntax(F))
      validateCFGV2AtomicPrerequisites(
          F, SE, UI, *static_cast<const GCNSubtarget *>(TM.getSubtargetImpl(F)),
          AtomicProducers);
  }
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
