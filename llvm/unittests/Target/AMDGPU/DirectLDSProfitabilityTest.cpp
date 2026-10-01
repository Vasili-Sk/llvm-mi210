//===- DirectLDSProfitabilityTest.cpp -------------------------------------===//

#include "AMDGPUDirectLDSProfitability.h"
#include "gtest/gtest.h"
#include <limits>
#include <vector>

using namespace llvm;
using namespace llvm::AMDGPU;

namespace {

struct Candidate {
  DirectLDSLayout Layout;
  std::vector<DirectLDSWord> Words;
  DirectLDSLayoutProof Proof;

  explicit Candidate(unsigned Width)
      : Layout(*DirectLDSLayout::create("gfx90a", 64, Width)),
        Proof{Words, Words, Layout.getFootprintBytes()} {
    for (unsigned Lane = 0; Lane != 64; ++Lane)
      for (unsigned Word = 0; Word != Width / 4; ++Word)
        Words.push_back({Lane, Word, *Layout.getByteOffset(Lane, Word)});
    Proof.ProducerDestinations = Words;
    Proof.ConsumerAddresses = Words;
  }
};

static DirectLDSCostFacts baseFacts() {
  DirectLDSCostFacts F;
  F.LoopTripCount = 2;
  F.ClassicInstructionsPerTrip = 10;
  F.DirectInstructionsPerTrip = 9;
  F.SetupInstructions = 1;
  F.DirectLoadsPerTrip = 1;
  F.ReplacedGlobalLoadInstructionsPerTrip = 1;
  F.ReplacedGlobalLoadBytesPerTrip = 16;
  F.RemovedDSWritesPerTrip = 1;
  F.RemovedWaitInstructionsPerTrip = 0;
  F.RemovedGlobalAddressInstructionsPerTrip = 0;
  F.RemovedLDSAddressInstructionsPerTrip = 0;
  F.RemovedOtherInstructionsPerTrip = 1;
  F.AddedGlobalAddressInstructionsPerTrip = 0;
  F.AddedLDSAddressInstructionsPerTrip = 0;
  F.AddedWaitInstructionsPerTrip = 1;
  F.AddedHazardRepairInstructionsPerTrip = 0;
  F.AddedOtherInstructionsPerTrip = 0;
  F.AddedDirectLoadInstructionsPerTrip = 1;
  F.AddedM0SetupInstructionsPerTrip = 0;
  F.OneTimeM0SetupInstructions = 1;
  F.OneTimeOtherSetupInstructions = 0;
  F.RemovedStagingVGPRs = 1;
  F.ClassicVGPRs = 16;
  F.DirectVGPRs = 16;
  F.ClassicOccupancyWaves = 4;
  F.DirectOccupancyWaves = 4;
  F.OccupancyLossInstructionCost = 0;
  return F;
}

static DirectLDSProfitResult evaluate(Candidate &C, DirectLDSCostFacts F) {
  if (F.DirectLoadsPerTrip == 1 && F.ReplacedGlobalLoadBytesPerTrip == 16)
    F.ReplacedGlobalLoadBytesPerTrip = C.Layout.getWidthBytes();
  return evaluateDirectLDSProfitability("gfx90a", 64, C.Layout.getWidthBytes(),
                                        C.Proof, F);
}

TEST(DirectLDSProfitabilityTest, SupportsOnlyExactGfx90aWave64Widths) {
  for (unsigned Width : {4u, 8u, 12u, 16u}) {
    Candidate C(Width);
    EXPECT_EQ(evaluate(C, baseFacts()).Reason,
              DirectLDSProfitReason::Profitable);
  }
  Candidate C(16);
  EXPECT_EQ(
      evaluateDirectLDSProfitability("gfx900", 64, 16, C.Proof, baseFacts())
          .Reason,
      DirectLDSProfitReason::UnsupportedLayout);
  EXPECT_EQ(
      evaluateDirectLDSProfitability("gfx90a", 32, 16, C.Proof, baseFacts())
          .Reason,
      DirectLDSProfitReason::UnsupportedLayout);
  EXPECT_EQ(
      evaluateDirectLDSProfitability("gfx90a", 64, 20, C.Proof, baseFacts())
          .Reason,
      DirectLDSProfitReason::UnsupportedLayout);
}

TEST(DirectLDSProfitabilityTest, RequiresCompleteLayoutProof) {
  Candidate C(16);
  C.Proof.ConsumerAddresses = ArrayRef<DirectLDSWord>(C.Words).drop_back();
  auto R = evaluate(C, baseFacts());
  EXPECT_EQ(R.Reason, DirectLDSProfitReason::IncompleteLayout);
  EXPECT_EQ(R.LayoutMatch, DirectLDSLayoutMatch::IncompleteConsumer);
}

TEST(DirectLDSProfitabilityTest, RejectsVerifiedOneStageReduction) {
  Candidate C(16);
  auto F = baseFacts();
  F.LoopTripCount = 1;
  F.ClassicInstructionsPerTrip = 87;
  F.DirectInstructionsPerTrip = 85;
  F.SetupInstructions = 2;
  F.OneTimeM0SetupInstructions = 1;
  F.OneTimeOtherSetupInstructions = 1;
  F.RemovedDSWritesPerTrip = 2;
  F.ReplacedGlobalLoadInstructionsPerTrip = 1;
  F.RemovedOtherInstructionsPerTrip = 0;
  F.AddedWaitInstructionsPerTrip = 0;
  F.ClassicVGPRs = 7;
  F.DirectVGPRs = 6;
  F.ClassicOccupancyWaves = 8;
  F.DirectOccupancyWaves = 8;
  auto R = evaluate(C, F);
  EXPECT_EQ(R.Reason, DirectLDSProfitReason::NotProfitable);
  EXPECT_EQ(R.ClassicDynamicInstructions, 87u);
  EXPECT_EQ(R.DirectDynamicInstructions, 87u);
}

TEST(DirectLDSProfitabilityTest, AcceptsCommittedTiledFixture) {
  Candidate C(16);
  auto F = baseFacts();
  F.LoopTripCount = 1;
  F.ClassicInstructionsPerTrip = 22;
  F.DirectInstructionsPerTrip = 20;
  F.SetupInstructions = 1;
  F.OneTimeM0SetupInstructions = 1;
  F.OneTimeOtherSetupInstructions = 0;
  F.DirectLoadsPerTrip = 1;
  F.ReplacedGlobalLoadInstructionsPerTrip = 1;
  F.ReplacedGlobalLoadBytesPerTrip = 16;
  F.RemovedDSWritesPerTrip = 2;
  F.RemovedGlobalAddressInstructionsPerTrip = 2;
  F.RemovedLDSAddressInstructionsPerTrip = 0;
  F.RemovedWaitInstructionsPerTrip = 0;
  F.RemovedOtherInstructionsPerTrip = 0;
  F.AddedDirectLoadInstructionsPerTrip = 1;
  F.AddedGlobalAddressInstructionsPerTrip = 1;
  F.AddedLDSAddressInstructionsPerTrip = 0;
  F.AddedWaitInstructionsPerTrip = 0;
  F.AddedHazardRepairInstructionsPerTrip = 1;
  F.AddedM0SetupInstructionsPerTrip = 0;
  F.AddedOtherInstructionsPerTrip = 0;
  F.RemovedStagingVGPRs = 4;
  F.ClassicVGPRs = 10;
  F.DirectVGPRs = 8;
  F.ClassicOccupancyWaves = 8;
  F.DirectOccupancyWaves = 8;
  auto OneTrip = evaluate(C, F);
  EXPECT_TRUE(OneTrip.IsProfitable);
  EXPECT_EQ(OneTrip.ClassicDynamicInstructions, 22u);
  EXPECT_EQ(OneTrip.DirectDynamicInstructions, 21u);
  EXPECT_EQ(OneTrip.InstructionSaving, 1u);

  F.LoopTripCount = 32;
  auto ManyTrips = evaluate(C, F);
  EXPECT_TRUE(ManyTrips.IsProfitable);
  EXPECT_EQ(ManyTrips.InstructionSaving, 63u);
}

TEST(DirectLDSProfitabilityTest, RejectsVerifiedStagedToDirectG12Tie) {
  Candidate C(16);
  auto F = baseFacts();
  F.LoopTripCount = 1;
  F.ClassicInstructionsPerTrip = 101;
  F.DirectInstructionsPerTrip = 101;
  F.SetupInstructions = 0;
  F.OneTimeM0SetupInstructions = 0;
  F.OneTimeOtherSetupInstructions = 0;
  F.DirectLoadsPerTrip = 2;
  F.ReplacedGlobalLoadInstructionsPerTrip = 2;
  F.ReplacedGlobalLoadBytesPerTrip = 32;
  F.RemovedDSWritesPerTrip = 4;
  F.RemovedWaitInstructionsPerTrip = 1;
  F.RemovedOtherInstructionsPerTrip = 6;
  F.AddedDirectLoadInstructionsPerTrip = 2;
  F.AddedWaitInstructionsPerTrip = 0;
  F.AddedHazardRepairInstructionsPerTrip = 1;
  F.AddedM0SetupInstructionsPerTrip = 2;
  F.AddedOtherInstructionsPerTrip = 8;
  F.RemovedStagingVGPRs = 6;
  F.ClassicVGPRs = 66;
  F.DirectVGPRs = 60;
  F.ClassicOccupancyWaves = 2;
  F.DirectOccupancyWaves = 2;
  auto R = evaluate(C, F);
  EXPECT_EQ(R.Reason, DirectLDSProfitReason::NotProfitable);
  EXPECT_EQ(R.ClassicDynamicInstructions, 101u);
  EXPECT_EQ(R.DirectDynamicInstructions, 101u);
}

TEST(DirectLDSProfitabilityTest, RejectsAlreadyDirectG10AndG12Misuse) {
  Candidate C(16);
  for (auto Pair : {std::pair<uint64_t, uint64_t>{69, 63},
                    std::pair<uint64_t, uint64_t>{537, 521}}) {
    auto F = baseFacts();
    F.ClassicInstructionsPerTrip = Pair.first;
    F.DirectInstructionsPerTrip = Pair.second;
    F.RemovedDSWritesPerTrip = 0;
    EXPECT_EQ(evaluate(C, F).Reason, DirectLDSProfitReason::NoRemovedDSWrites);
  }
}

TEST(DirectLDSProfitabilityTest, ChecksThresholdAndTripAmortization) {
  Candidate C(8);
  auto F = baseFacts();
  F.ClassicInstructionsPerTrip = 10;
  F.DirectInstructionsPerTrip = 9;
  F.SetupInstructions = 2;
  F.OneTimeM0SetupInstructions = 1;
  F.OneTimeOtherSetupInstructions = 1;
  F.LoopTripCount = 1;
  EXPECT_EQ(evaluate(C, F).Reason, DirectLDSProfitReason::NotProfitable);
  F.LoopTripCount = 2;
  EXPECT_EQ(evaluate(C, F).Reason, DirectLDSProfitReason::NotProfitable);
  F.LoopTripCount = 3;
  EXPECT_EQ(evaluate(C, F).Reason, DirectLDSProfitReason::Profitable);
  F.LoopTripCount = 0;
  EXPECT_EQ(evaluate(C, F).Reason, DirectLDSProfitReason::ZeroLoopTrips);
  F.LoopTripCount = 1000;
  EXPECT_EQ(evaluate(C, F).Reason, DirectLDSProfitReason::Profitable);
}

TEST(DirectLDSProfitabilityTest, RequiresStructuralSavings) {
  Candidate C(4);
  auto F = baseFacts();
  F.DirectLoadsPerTrip = 0;
  EXPECT_EQ(evaluate(C, F).Reason, DirectLDSProfitReason::NoDirectLoads);
  F = baseFacts();
  F.RemovedDSWritesPerTrip = 0;
  EXPECT_EQ(evaluate(C, F).Reason, DirectLDSProfitReason::NoRemovedDSWrites);
  F = baseFacts();
  F.RemovedStagingVGPRs = 0;
  EXPECT_EQ(evaluate(C, F).Reason,
            DirectLDSProfitReason::NoRemovedStagingVGPRs);
}

TEST(DirectLDSProfitabilityTest, AccountsForAllAddedInstructionClasses) {
  Candidate C(12);
  auto F = baseFacts();
  F.ClassicInstructionsPerTrip = 12;
  F.DirectInstructionsPerTrip = 12;
  F.SetupInstructions = 1;
  F.RemovedGlobalAddressInstructionsPerTrip = 2;
  F.AddedGlobalAddressInstructionsPerTrip = 1;
  F.AddedLDSAddressInstructionsPerTrip = 1;
  F.AddedWaitInstructionsPerTrip = 1;
  F.AddedHazardRepairInstructionsPerTrip = 1;
  F.AddedDirectLoadInstructionsPerTrip = 1;
  F.AddedM0SetupInstructionsPerTrip = 0;
  F.OneTimeM0SetupInstructions = 1;
  F.OneTimeOtherSetupInstructions = 0;
  EXPECT_EQ(evaluate(C, F).Reason, DirectLDSProfitReason::NotProfitable);
}

TEST(DirectLDSProfitabilityTest, ValidatesRegisterAndOccupancyFacts) {
  Candidate C(16);
  auto F = baseFacts();
  F.ClassicVGPRs = 16;
  F.DirectVGPRs = 15;
  F.DirectOccupancyWaves = 5;
  EXPECT_EQ(evaluate(C, F).Reason, DirectLDSProfitReason::Profitable);

  F = baseFacts();
  F.DirectVGPRs = 17;
  F.DirectOccupancyWaves = 3;
  EXPECT_EQ(evaluate(C, F).Reason,
            DirectLDSProfitReason::OccupancyLossUnpriced);
  F.OccupancyLossInstructionCost = 1;
  EXPECT_EQ(evaluate(C, F).Reason, DirectLDSProfitReason::OccupancyLossNotPaid);
  F.LoopTripCount = 3;
  EXPECT_EQ(evaluate(C, F).Reason, DirectLDSProfitReason::Profitable);

  F = baseFacts();
  F.ClassicVGPRs = 18;
  F.DirectVGPRs = 16;
  EXPECT_EQ(evaluate(C, F).Reason,
            DirectLDSProfitReason::RegisterFactsInconsistent);
  F = baseFacts();
  F.DirectOccupancyWaves = 3;
  EXPECT_EQ(evaluate(C, F).Reason,
            DirectLDSProfitReason::OccupancyLossUnpriced);
  F = baseFacts();
  F.OccupancyLossInstructionCost = 1;
  EXPECT_EQ(evaluate(C, F).Reason,
            DirectLDSProfitReason::OccupancyFactsInconsistent);
}

TEST(DirectLDSProfitabilityTest, RejectsEveryUnknownFact) {
  Candidate C(16);
  using Member = std::optional<uint64_t> DirectLDSCostFacts::*;
  const Member Members[] = {
      &DirectLDSCostFacts::LoopTripCount,
      &DirectLDSCostFacts::ClassicInstructionsPerTrip,
      &DirectLDSCostFacts::DirectInstructionsPerTrip,
      &DirectLDSCostFacts::SetupInstructions,
      &DirectLDSCostFacts::OneTimeM0SetupInstructions,
      &DirectLDSCostFacts::OneTimeOtherSetupInstructions,
      &DirectLDSCostFacts::DirectLoadsPerTrip,
      &DirectLDSCostFacts::ReplacedGlobalLoadInstructionsPerTrip,
      &DirectLDSCostFacts::ReplacedGlobalLoadBytesPerTrip,
      &DirectLDSCostFacts::RemovedDSWritesPerTrip,
      &DirectLDSCostFacts::RemovedGlobalAddressInstructionsPerTrip,
      &DirectLDSCostFacts::RemovedLDSAddressInstructionsPerTrip,
      &DirectLDSCostFacts::RemovedWaitInstructionsPerTrip,
      &DirectLDSCostFacts::RemovedOtherInstructionsPerTrip,
      &DirectLDSCostFacts::AddedDirectLoadInstructionsPerTrip,
      &DirectLDSCostFacts::AddedGlobalAddressInstructionsPerTrip,
      &DirectLDSCostFacts::AddedLDSAddressInstructionsPerTrip,
      &DirectLDSCostFacts::AddedWaitInstructionsPerTrip,
      &DirectLDSCostFacts::AddedHazardRepairInstructionsPerTrip,
      &DirectLDSCostFacts::AddedOtherInstructionsPerTrip,
      &DirectLDSCostFacts::AddedM0SetupInstructionsPerTrip,
      &DirectLDSCostFacts::RemovedStagingVGPRs,
      &DirectLDSCostFacts::ClassicVGPRs,
      &DirectLDSCostFacts::DirectVGPRs,
      &DirectLDSCostFacts::ClassicOccupancyWaves,
      &DirectLDSCostFacts::DirectOccupancyWaves,
      &DirectLDSCostFacts::OccupancyLossInstructionCost,
  };
  for (Member M : Members) {
    auto F = baseFacts();
    (F.*M).reset();
    EXPECT_EQ(evaluate(C, F).Reason, DirectLDSProfitReason::UnknownFact);
  }
}

TEST(DirectLDSProfitabilityTest, HasStableDeterministicReasonCodes) {
  EXPECT_EQ(getDirectLDSProfitReasonName(DirectLDSProfitReason::Profitable),
            "profitable");
  EXPECT_EQ(getDirectLDSProfitReasonName(DirectLDSProfitReason::UnknownFact),
            "unknown-fact");
  EXPECT_EQ(getDirectLDSProfitReasonName(DirectLDSProfitReason::NotProfitable),
            "not-profitable");
  EXPECT_EQ(
      getDirectLDSProfitReasonName(DirectLDSProfitReason::OccupancyLossNotPaid),
      "occupancy-loss-not-paid");

  Candidate C(16);
  auto F = baseFacts();
  F.LoopTripCount.reset();
  F.RemovedDSWritesPerTrip = 0;
  EXPECT_EQ(evaluate(C, F).Reason, DirectLDSProfitReason::UnknownFact);
  C.Proof.HasUnknownAlias = true;
  EXPECT_EQ(evaluate(C, F).Reason, DirectLDSProfitReason::IncompleteLayout);
  EXPECT_EQ(evaluateDirectLDSProfitability("gfx900", 64, 16, C.Proof, F).Reason,
            DirectLDSProfitReason::UnsupportedLayout);
}

TEST(DirectLDSProfitabilityTest, RejectsUnknownInconsistentAndOverflowFacts) {
  Candidate C(16);
  auto F = baseFacts();
  F.AddedWaitInstructionsPerTrip.reset();
  EXPECT_EQ(evaluate(C, F).Reason, DirectLDSProfitReason::UnknownFact);
  F = baseFacts();
  F.ReplacedGlobalLoadInstructionsPerTrip = 2;
  EXPECT_EQ(evaluate(C, F).Reason, DirectLDSProfitReason::InvalidInput);
  F = baseFacts();
  F.AddedM0SetupInstructionsPerTrip = 2;
  EXPECT_EQ(evaluate(C, F).Reason, DirectLDSProfitReason::InvalidInput);
  F = baseFacts();
  F.LoopTripCount = std::numeric_limits<uint64_t>::max();
  EXPECT_EQ(evaluate(C, F).Reason, DirectLDSProfitReason::Overflow);
}

TEST(DirectLDSProfitabilityTest, IsMonotonic) {
  Candidate C(16);
  auto F = baseFacts();
  auto Accepted = evaluate(C, F);
  ASSERT_TRUE(Accepted.IsProfitable);
  F.ClassicInstructionsPerTrip = *F.ClassicInstructionsPerTrip + 1;
  F.RemovedOtherInstructionsPerTrip = *F.RemovedOtherInstructionsPerTrip + 1;
  EXPECT_TRUE(evaluate(C, F).IsProfitable);
  F.RemovedStagingVGPRs = *F.RemovedStagingVGPRs + 1;
  EXPECT_TRUE(evaluate(C, F).IsProfitable);

  F = baseFacts();
  F.DirectInstructionsPerTrip = 11;
  F.AddedGlobalAddressInstructionsPerTrip = 2;
  ASSERT_EQ(evaluate(C, F).Reason, DirectLDSProfitReason::NotProfitable);
  F.DirectInstructionsPerTrip = 12;
  F.SetupInstructions = 2;
  F.OneTimeM0SetupInstructions = 1;
  F.OneTimeOtherSetupInstructions = 1;
  F.AddedWaitInstructionsPerTrip = 2;
  EXPECT_EQ(evaluate(C, F).Reason, DirectLDSProfitReason::NotProfitable);
}

} // namespace
