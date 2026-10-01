//===- AMDGPUDirectLDSProfitability.cpp - Direct LDS cost model -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AMDGPUDirectLDSProfitability.h"
#include "llvm/Support/ErrorHandling.h"
#include <limits>

using namespace llvm;
using namespace llvm::AMDGPU;

StringRef AMDGPU::getDirectLDSProfitReasonName(DirectLDSProfitReason Reason) {
  switch (Reason) {
  case DirectLDSProfitReason::Profitable:
    return "profitable";
  case DirectLDSProfitReason::UnsupportedLayout:
    return "unsupported-layout";
  case DirectLDSProfitReason::IncompleteLayout:
    return "incomplete-layout";
  case DirectLDSProfitReason::UnknownFact:
    return "unknown-fact";
  case DirectLDSProfitReason::InvalidInput:
    return "invalid-input";
  case DirectLDSProfitReason::Overflow:
    return "overflow";
  case DirectLDSProfitReason::ZeroLoopTrips:
    return "zero-loop-trips";
  case DirectLDSProfitReason::NoDirectLoads:
    return "no-direct-loads";
  case DirectLDSProfitReason::NoRemovedDSWrites:
    return "no-removed-ds-writes";
  case DirectLDSProfitReason::NoRemovedStagingVGPRs:
    return "no-removed-staging-vgprs";
  case DirectLDSProfitReason::RegisterFactsInconsistent:
    return "register-facts-inconsistent";
  case DirectLDSProfitReason::OccupancyFactsInconsistent:
    return "occupancy-facts-inconsistent";
  case DirectLDSProfitReason::OccupancyLossUnpriced:
    return "occupancy-loss-unpriced";
  case DirectLDSProfitReason::NotProfitable:
    return "not-profitable";
  case DirectLDSProfitReason::OccupancyLossNotPaid:
    return "occupancy-loss-not-paid";
  }
  llvm_unreachable("invalid direct LDS profit reason");
}

static DirectLDSProfitResult
result(DirectLDSProfitReason Reason,
       DirectLDSLayoutMatch Match = DirectLDSLayoutMatch::Match) {
  DirectLDSProfitResult Result;
  Result.Reason = Reason;
  Result.LayoutMatch = Match;
  return Result;
}

DirectLDSProfitResult AMDGPU::evaluateDirectLDSProfitability(
    StringRef GPU, unsigned WaveSize, unsigned WidthBytes,
    const DirectLDSLayoutProof &Proof, const DirectLDSCostFacts &F) {
  std::optional<DirectLDSLayout> Layout =
      DirectLDSLayout::create(GPU, WaveSize, WidthBytes);
  if (!Layout)
    return result(DirectLDSProfitReason::UnsupportedLayout);

  DirectLDSLayoutMatch Match = matchDirectLDSLayout(*Layout, Proof);
  if (Match != DirectLDSLayoutMatch::Match)
    return result(DirectLDSProfitReason::IncompleteLayout, Match);

#define REQUIRE_FACT(Name)                                                     \
  if (!F.Name)                                                                 \
  return result(DirectLDSProfitReason::UnknownFact)
  REQUIRE_FACT(LoopTripCount);
  REQUIRE_FACT(ClassicInstructionsPerTrip);
  REQUIRE_FACT(DirectInstructionsPerTrip);
  REQUIRE_FACT(SetupInstructions);
  REQUIRE_FACT(OneTimeM0SetupInstructions);
  REQUIRE_FACT(OneTimeOtherSetupInstructions);
  REQUIRE_FACT(DirectLoadsPerTrip);
  REQUIRE_FACT(ReplacedGlobalLoadInstructionsPerTrip);
  REQUIRE_FACT(ReplacedGlobalLoadBytesPerTrip);
  REQUIRE_FACT(RemovedDSWritesPerTrip);
  REQUIRE_FACT(RemovedGlobalAddressInstructionsPerTrip);
  REQUIRE_FACT(RemovedLDSAddressInstructionsPerTrip);
  REQUIRE_FACT(RemovedWaitInstructionsPerTrip);
  REQUIRE_FACT(RemovedOtherInstructionsPerTrip);
  REQUIRE_FACT(AddedDirectLoadInstructionsPerTrip);
  REQUIRE_FACT(AddedGlobalAddressInstructionsPerTrip);
  REQUIRE_FACT(AddedLDSAddressInstructionsPerTrip);
  REQUIRE_FACT(AddedWaitInstructionsPerTrip);
  REQUIRE_FACT(AddedHazardRepairInstructionsPerTrip);
  REQUIRE_FACT(AddedM0SetupInstructionsPerTrip);
  REQUIRE_FACT(AddedOtherInstructionsPerTrip);
  REQUIRE_FACT(RemovedStagingVGPRs);
  REQUIRE_FACT(ClassicVGPRs);
  REQUIRE_FACT(DirectVGPRs);
  REQUIRE_FACT(ClassicOccupancyWaves);
  REQUIRE_FACT(DirectOccupancyWaves);
  REQUIRE_FACT(OccupancyLossInstructionCost);
#undef REQUIRE_FACT

  if (*F.LoopTripCount == 0)
    return result(DirectLDSProfitReason::ZeroLoopTrips);
  if (*F.DirectLoadsPerTrip == 0)
    return result(DirectLDSProfitReason::NoDirectLoads);
  uint64_t DirectLoadBytes;
  if (*F.DirectLoadsPerTrip > std::numeric_limits<uint64_t>::max() / WidthBytes)
    return result(DirectLDSProfitReason::Overflow);
  DirectLoadBytes = *F.DirectLoadsPerTrip * WidthBytes;
  if (*F.AddedDirectLoadInstructionsPerTrip != *F.DirectLoadsPerTrip ||
      DirectLoadBytes != *F.ReplacedGlobalLoadBytesPerTrip ||
      *F.OneTimeM0SetupInstructions > *F.SetupInstructions ||
      *F.OneTimeOtherSetupInstructions !=
          *F.SetupInstructions - *F.OneTimeM0SetupInstructions)
    return result(DirectLDSProfitReason::InvalidInput);
  if (*F.RemovedDSWritesPerTrip == 0)
    return result(DirectLDSProfitReason::NoRemovedDSWrites);
  if (*F.RemovedStagingVGPRs == 0)
    return result(DirectLDSProfitReason::NoRemovedStagingVGPRs);
  if (*F.ClassicVGPRs == 0 || *F.DirectVGPRs == 0 ||
      (*F.ClassicVGPRs > *F.DirectVGPRs &&
       *F.ClassicVGPRs - *F.DirectVGPRs > *F.RemovedStagingVGPRs))
    return result(DirectLDSProfitReason::RegisterFactsInconsistent);
  if (*F.ClassicOccupancyWaves == 0 || *F.DirectOccupancyWaves == 0 ||
      (*F.DirectOccupancyWaves >= *F.ClassicOccupancyWaves &&
       *F.OccupancyLossInstructionCost != 0))
    return result(DirectLDSProfitReason::OccupancyFactsInconsistent);
  if (*F.DirectOccupancyWaves < *F.ClassicOccupancyWaves &&
      *F.OccupancyLossInstructionCost == 0)
    return result(DirectLDSProfitReason::OccupancyLossUnpriced);

  auto CheckedAdd = [](uint64_t A, uint64_t B, uint64_t &Sum) {
    if (B > std::numeric_limits<uint64_t>::max() - A)
      return false;
    Sum = A + B;
    return true;
  };
  uint64_t Removed = 0;
  uint64_t Added = 0;
  if (!CheckedAdd(*F.RemovedDSWritesPerTrip,
                  *F.ReplacedGlobalLoadInstructionsPerTrip, Removed) ||
      !CheckedAdd(Removed, *F.RemovedGlobalAddressInstructionsPerTrip,
                  Removed) ||
      !CheckedAdd(Removed, *F.RemovedLDSAddressInstructionsPerTrip, Removed) ||
      !CheckedAdd(Removed, *F.RemovedWaitInstructionsPerTrip, Removed) ||
      !CheckedAdd(Removed, *F.RemovedOtherInstructionsPerTrip, Removed) ||
      !CheckedAdd(*F.AddedDirectLoadInstructionsPerTrip,
                  *F.AddedGlobalAddressInstructionsPerTrip, Added) ||
      !CheckedAdd(Added, *F.AddedLDSAddressInstructionsPerTrip, Added) ||
      !CheckedAdd(Added, *F.AddedWaitInstructionsPerTrip, Added) ||
      !CheckedAdd(Added, *F.AddedHazardRepairInstructionsPerTrip, Added) ||
      !CheckedAdd(Added, *F.AddedM0SetupInstructionsPerTrip, Added) ||
      !CheckedAdd(Added, *F.AddedOtherInstructionsPerTrip, Added))
    return result(DirectLDSProfitReason::Overflow);
  uint64_t ClassicPlusAdded;
  uint64_t DirectPlusRemoved;
  if (!CheckedAdd(*F.ClassicInstructionsPerTrip, Added, ClassicPlusAdded) ||
      !CheckedAdd(*F.DirectInstructionsPerTrip, Removed, DirectPlusRemoved))
    return result(DirectLDSProfitReason::Overflow);
  if (ClassicPlusAdded != DirectPlusRemoved)
    return result(DirectLDSProfitReason::InvalidInput);

  auto CheckedMultiply = [](uint64_t A, uint64_t B, uint64_t &Product) {
    if (A != 0 && B > std::numeric_limits<uint64_t>::max() / A)
      return false;
    Product = A * B;
    return true;
  };
  uint64_t ClassicDynamic;
  uint64_t DirectLoop;
  uint64_t DirectDynamic;
  if (!CheckedMultiply(*F.ClassicInstructionsPerTrip, *F.LoopTripCount,
                       ClassicDynamic) ||
      !CheckedMultiply(*F.DirectInstructionsPerTrip, *F.LoopTripCount,
                       DirectLoop) ||
      *F.SetupInstructions > std::numeric_limits<uint64_t>::max() - DirectLoop)
    return result(DirectLDSProfitReason::Overflow);
  DirectDynamic = DirectLoop + *F.SetupInstructions;

  DirectLDSProfitResult Result;
  Result.ClassicDynamicInstructions = ClassicDynamic;
  Result.DirectDynamicInstructions = DirectDynamic;
  if (DirectDynamic >= ClassicDynamic) {
    Result.Reason = DirectLDSProfitReason::NotProfitable;
    return Result;
  }

  Result.InstructionSaving = ClassicDynamic - DirectDynamic;
  if (*F.DirectOccupancyWaves < *F.ClassicOccupancyWaves &&
      Result.InstructionSaving <= *F.OccupancyLossInstructionCost) {
    Result.Reason = DirectLDSProfitReason::OccupancyLossNotPaid;
    return Result;
  }

  Result.IsProfitable = true;
  Result.Reason = DirectLDSProfitReason::Profitable;
  return Result;
}
