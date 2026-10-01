//===- DirectLDSLayoutTest.cpp --------------------------------------------===//

#include "AMDGPUDirectLDSLayout.h"
#include "llvm/Support/ErrorHandling.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <vector>

using namespace llvm;
using namespace llvm::AMDGPU;

namespace {

static uint64_t expectedOffset(unsigned Width, unsigned Lane,
                               unsigned Payload) {
  switch (Width) {
  case 4:
    return 4 * Lane;
  case 8:
    return 4 * (16 * (Lane / 8) + Lane % 8 + 8 * Payload);
  case 12:
  case 16: {
    const unsigned Permutation[] = {0, 2, 1, 3};
    return 4 * (16 * (Lane / 4) + Lane % 4 + 4 * Permutation[Payload]);
  }
  }
  llvm_unreachable("bad test width");
}

static std::vector<DirectLDSWord> completeWords(const DirectLDSLayout &Layout) {
  std::vector<DirectLDSWord> Words;
  for (unsigned Lane = 0; Lane != 64; ++Lane)
    for (unsigned Payload = 0; Payload != Layout.getPayloadWords(); ++Payload)
      Words.push_back({Lane, Payload, *Layout.getByteOffset(Lane, Payload)});
  return Words;
}

TEST(DirectLDSLayoutTest, RejectsUnsupportedConfigurations) {
  EXPECT_FALSE(DirectLDSLayout::create("gfx900", 64, 4));
  EXPECT_FALSE(DirectLDSLayout::create("gfx90a:xnack-", 64, 4));
  EXPECT_FALSE(DirectLDSLayout::create("gfx90a", 32, 4));
  EXPECT_FALSE(DirectLDSLayout::create("gfx90a", 64, 0));
  EXPECT_FALSE(DirectLDSLayout::create("gfx90a", 64, 1));
  EXPECT_FALSE(DirectLDSLayout::create("gfx90a", 64, 20));
}

TEST(DirectLDSLayoutTest, ExhaustiveForwardInverseAndInjective) {
  for (unsigned Width : {4u, 8u, 12u, 16u}) {
    auto Layout = DirectLDSLayout::create("gfx90a", 64, Width);
    ASSERT_TRUE(Layout);
    std::vector<uint64_t> Seen;
    for (unsigned Lane = 0; Lane != 64; ++Lane) {
      for (unsigned Payload = 0; Payload != Width / 4; ++Payload) {
        SCOPED_TRACE("width=" + std::to_string(Width) +
                     " lane=" + std::to_string(Lane) +
                     " payload=" + std::to_string(Payload));
        uint64_t Expected = expectedOffset(Width, Lane, Payload);
        auto Offset = Layout->getByteOffset(Lane, Payload);
        ASSERT_TRUE(Offset);
        EXPECT_EQ(*Offset, Expected) << "expected byte offset " << Expected;
        EXPECT_EQ(std::count(Seen.begin(), Seen.end(), *Offset), 0);
        Seen.push_back(*Offset);
        auto Source = Layout->getSourceWord(*Offset);
        ASSERT_TRUE(Source);
        EXPECT_EQ(Source->Lane, Lane);
        EXPECT_EQ(Source->PayloadWord, Payload);
      }
    }
    EXPECT_EQ(Seen.size(), 64u * (Width / 4));
    EXPECT_FALSE(Layout->getByteOffset(64, 0));
    EXPECT_FALSE(Layout->getByteOffset(0, Width / 4));
    EXPECT_FALSE(Layout->getSourceWord(1));
    EXPECT_FALSE(Layout->getSourceWord(Layout->getFootprintBytes()));
  }
}

TEST(DirectLDSLayoutTest, ExecutedProbeAnchorsAndHoles) {
  auto X1 = *DirectLDSLayout::create("gfx90a", 64, 4);
  auto X2 = *DirectLDSLayout::create("gfx90a", 64, 8);
  auto X3 = *DirectLDSLayout::create("gfx90a", 64, 12);
  auto X4 = *DirectLDSLayout::create("gfx90a", 64, 16);

  EXPECT_EQ(X1.getByteOffset(63, 0), 252u);
  EXPECT_EQ(X2.getByteOffset(0, 1), 32u);
  EXPECT_EQ(X2.getByteOffset(8, 0), 64u);
  EXPECT_EQ(X3.getByteOffset(0, 1), 32u);
  EXPECT_EQ(X3.getByteOffset(0, 2), 16u);
  EXPECT_EQ(X3.getByteOffset(63, 2), 988u);
  EXPECT_FALSE(X3.getSourceWord(48));
  EXPECT_FALSE(X3.getSourceWord(60));
  EXPECT_EQ(X4.getByteOffset(0, 1), 32u);
  EXPECT_EQ(X4.getByteOffset(0, 2), 16u);
  EXPECT_EQ(X4.getByteOffset(0, 3), 48u);
  EXPECT_EQ(X4.getByteOffset(63, 3), 1020u);
}

TEST(DirectLDSLayoutTest, AcceptsEveryCompleteLayout) {
  for (unsigned Width : {4u, 8u, 12u, 16u}) {
    auto Layout = *DirectLDSLayout::create("gfx90a", 64, Width);
    auto Words = completeWords(Layout);
    std::reverse(Words.begin(), Words.end());
    DirectLDSLayoutProof Proof{Words, Words, Layout.getFootprintBytes()};
    EXPECT_EQ(matchDirectLDSLayout(Layout, Proof), DirectLDSLayoutMatch::Match)
        << "width=" << Width;
  }
}

TEST(DirectLDSLayoutTest, RejectsIncompleteAndUnprovableLayouts) {
  auto Layout = *DirectLDSLayout::create("gfx90a", 64, 16);
  auto Words = completeWords(Layout);

  DirectLDSLayoutProof Proof{Words, Words, Layout.getFootprintBytes()};
  Proof.HasDynamicAddress = true;
  EXPECT_EQ(matchDirectLDSLayout(Layout, Proof),
            DirectLDSLayoutMatch::DynamicAddress);
  Proof.HasDynamicAddress = false;
  Proof.HasUnknownAlias = true;
  EXPECT_EQ(matchDirectLDSLayout(Layout, Proof),
            DirectLDSLayoutMatch::UnknownAlias);
  Proof.HasUnknownAlias = false;
  Proof.TileStrideBytes = Layout.getFootprintBytes() - 4;
  EXPECT_EQ(matchDirectLDSLayout(Layout, Proof),
            DirectLDSLayoutMatch::InvalidStride);
  Proof.TileStrideBytes = Layout.getFootprintBytes() + 2;
  EXPECT_EQ(matchDirectLDSLayout(Layout, Proof),
            DirectLDSLayoutMatch::InvalidStride);

  Proof.TileStrideBytes = Layout.getFootprintBytes();
  Proof.ProducerDestinations = ArrayRef<DirectLDSWord>(Words).drop_back();
  EXPECT_EQ(matchDirectLDSLayout(Layout, Proof),
            DirectLDSLayoutMatch::IncompleteProducer);
  Proof.ProducerDestinations = Words;
  Proof.ConsumerAddresses = ArrayRef<DirectLDSWord>(Words).drop_back();
  EXPECT_EQ(matchDirectLDSLayout(Layout, Proof),
            DirectLDSLayoutMatch::IncompleteConsumer);

  auto Extra = Words;
  Extra.push_back(Words.front());
  Proof.ConsumerAddresses = Extra;
  EXPECT_EQ(matchDirectLDSLayout(Layout, Proof),
            DirectLDSLayoutMatch::IncompleteConsumer);
}

TEST(DirectLDSLayoutTest, RejectsDuplicatesAndLayoutMismatches) {
  auto Layout = *DirectLDSLayout::create("gfx90a", 64, 12);
  auto Words = completeWords(Layout);
  DirectLDSLayoutProof Proof{Words, Words, Layout.getFootprintBytes()};

  auto Bad = Words;
  Bad[1].ByteOffset = Bad[0].ByteOffset;
  Proof.ProducerDestinations = Bad;
  EXPECT_EQ(matchDirectLDSLayout(Layout, Proof),
            DirectLDSLayoutMatch::DuplicateProducerDestination);

  Proof.ProducerDestinations = Words;
  Proof.ConsumerAddresses = Bad;
  EXPECT_EQ(matchDirectLDSLayout(Layout, Proof),
            DirectLDSLayoutMatch::DuplicateConsumerAddress);

  Bad = Words;
  Bad[0].ByteOffset += 4;
  Proof.ProducerDestinations = Bad;
  Proof.ConsumerAddresses = Words;
  EXPECT_EQ(matchDirectLDSLayout(Layout, Proof),
            DirectLDSLayoutMatch::ProducerLayoutMismatch);

  Proof.ProducerDestinations = Words;
  Proof.ConsumerAddresses = Bad;
  EXPECT_EQ(matchDirectLDSLayout(Layout, Proof),
            DirectLDSLayoutMatch::ConsumerLayoutMismatch);

  auto X4 = *DirectLDSLayout::create("gfx90a", 64, 16);
  auto X4Words = completeWords(X4);
  Proof.ProducerDestinations = X4Words;
  Proof.ConsumerAddresses = X4Words;
  Proof.TileStrideBytes = X4.getFootprintBytes();
  EXPECT_EQ(matchDirectLDSLayout(Layout, Proof),
            DirectLDSLayoutMatch::IncompleteProducer);
}

} // namespace
