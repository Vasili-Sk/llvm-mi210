//===- AMDGPUDirectLDSProfitability.h - Direct LDS cost model -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_AMDGPU_AMDGPUDIRECTLDSPROFITABILITY_H
#define LLVM_LIB_TARGET_AMDGPU_AMDGPUDIRECTLDSPROFITABILITY_H

#include "AMDGPUDirectLDSLayout.h"
#include "llvm/ADT/StringRef.h"
#include <cstdint>
#include <optional>

namespace llvm {
namespace AMDGPU {

/// Facts for one candidate region. Per-trip values use executed instruction
/// units. SetupInstructions executes once for the region. Removed and added
/// instruction classes must reconcile with the per-trip totals. The caller
/// must provide every fact. The model does not infer a missing value.

enum class DirectLDSResourceProofMode {
  ExactPostRAFacts,
  GuaranteedPreRAOccupancy,
};

struct DirectLDSCostFacts {
  DirectLDSResourceProofMode ResourceProofMode =
      DirectLDSResourceProofMode::ExactPostRAFacts;
  std::optional<uint64_t> LoopTripCount;
  std::optional<uint64_t> ClassicInstructionsPerTrip;
  std::optional<uint64_t> DirectInstructionsPerTrip;
  std::optional<uint64_t> SetupInstructions;
  std::optional<uint64_t> OneTimeM0SetupInstructions;
  std::optional<uint64_t> OneTimeOtherSetupInstructions;

  std::optional<uint64_t> DirectLoadsPerTrip;
  std::optional<uint64_t> ReplacedGlobalLoadInstructionsPerTrip;
  std::optional<uint64_t> ReplacedGlobalLoadBytesPerTrip;
  std::optional<uint64_t> RemovedDSWritesPerTrip;
  std::optional<uint64_t> RemovedGlobalAddressInstructionsPerTrip;
  std::optional<uint64_t> RemovedLDSAddressInstructionsPerTrip;
  std::optional<uint64_t> RemovedWaitInstructionsPerTrip;
  std::optional<uint64_t> RemovedOtherInstructionsPerTrip;
  std::optional<uint64_t> AddedDirectLoadInstructionsPerTrip;
  std::optional<uint64_t> AddedGlobalAddressInstructionsPerTrip;
  std::optional<uint64_t> AddedLDSAddressInstructionsPerTrip;
  std::optional<uint64_t> AddedWaitInstructionsPerTrip;
  std::optional<uint64_t> AddedHazardRepairInstructionsPerTrip;
  std::optional<uint64_t> AddedM0SetupInstructionsPerTrip;
  std::optional<uint64_t> AddedOtherInstructionsPerTrip;

  std::optional<uint64_t> RemovedStagingVGPRs;
  std::optional<uint64_t> ClassicVGPRs;
  std::optional<uint64_t> DirectVGPRs;
  std::optional<uint64_t> ClassicOccupancyWaves;
  std::optional<uint64_t> DirectOccupancyWaves;

  /// A caller must quantify an occupancy loss in instruction units. Zero is
  /// valid only when occupancy does not decrease. The region instruction
  /// saving must be greater than this cost.
  std::optional<uint64_t> OccupancyLossInstructionCost;

  /// Pre-RA proof facts. This mode does not predict final register counts.
  /// The exact occupancy value must come from a compiler-enforced equal
  /// minimum and maximum occupancy contract on the function.
  std::optional<uint64_t> RemovedExclusivePayloadDwords;
  std::optional<uint64_t> AddedDivergentOffsetDwords;
  std::optional<int64_t> NetDivergentDwordDelta;
  std::optional<uint64_t> TargetMaxOccupancyWaves;
  std::optional<uint64_t> EffectiveOccupancyWaves;
  std::optional<uint64_t> NonRegisterOccupancyWaves;
  std::optional<uint64_t> RegisterBudgetOccupancyWaves;
  std::optional<uint64_t> MaxVGPRsAtTargetOccupancy;
  std::optional<uint64_t> MaxSGPRsAtTargetOccupancy;
};

enum class DirectLDSProfitReason {
  Profitable,
  UnsupportedLayout,
  IncompleteLayout,
  UnknownFact,
  InvalidInput,
  Overflow,
  ZeroLoopTrips,
  NoDirectLoads,
  NoRemovedDSWrites,
  NoRemovedStagingVGPRs,
  RegisterFactsInconsistent,
  OccupancyFactsInconsistent,
  OccupancyLossUnpriced,
  NotProfitable,
  OccupancyLossNotPaid,
  MissingResourceProof,
  UnenforcedOccupancyProof,
};

/// Return a stable text code for diagnostics.
StringRef getDirectLDSProfitReasonName(DirectLDSProfitReason Reason);

struct DirectLDSProfitResult {
  bool IsProfitable = false;
  DirectLDSProfitReason Reason = DirectLDSProfitReason::InvalidInput;
  DirectLDSLayoutMatch LayoutMatch = DirectLDSLayoutMatch::Match;
  uint64_t ClassicDynamicInstructions = 0;
  uint64_t DirectDynamicInstructions = 0;
  uint64_t InstructionSaving = 0;
};

/// Evaluate a complete staged-to-direct candidate. This function calls the
/// layout matcher. It does not reproduce placement rules. It supports exact
/// gfx90a wave64 layouts and direct widths of 4, 8, 12, and 16 bytes.
DirectLDSProfitResult evaluateDirectLDSProfitability(
    StringRef GPU, unsigned WaveSize, unsigned WidthBytes,
    const DirectLDSLayoutProof &Proof, const DirectLDSCostFacts &Facts);

} // namespace AMDGPU
} // namespace llvm

#endif
