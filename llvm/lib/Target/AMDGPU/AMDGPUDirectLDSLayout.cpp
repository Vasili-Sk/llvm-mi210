//===- AMDGPUDirectLDSLayout.cpp - gfx90a direct LDS layout ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AMDGPUDirectLDSLayout.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;
using namespace llvm::AMDGPU;

std::optional<DirectLDSLayout>
DirectLDSLayout::create(StringRef GPU, unsigned WaveSize, unsigned WidthBytes) {
  if (GPU != "gfx90a" || WaveSize != 64 ||
      (WidthBytes != 4 && WidthBytes != 8 && WidthBytes != 12 &&
       WidthBytes != 16))
    return std::nullopt;
  return DirectLDSLayout(WaveSize, WidthBytes);
}

unsigned DirectLDSLayout::getFootprintBytes() const {
  switch (WidthBytes) {
  case 4:
    return 256;
  case 8:
    return 512;
  case 12:
  case 16:
    return 1024;
  default:
    llvm_unreachable("invalid direct LDS width");
  }
}

std::optional<uint64_t>
DirectLDSLayout::getByteOffset(unsigned Lane, unsigned PayloadWord) const {
  if (Lane >= WaveSize || PayloadWord >= getPayloadWords())
    return std::nullopt;

  unsigned WordOffset;
  switch (WidthBytes) {
  case 4:
    // Documented by the classic instruction and executed-confirmed on gfx90a.
    WordOffset = Lane;
    break;
  case 8:
    // Executed-confirmed full map. Eight lanes form a 16-word group. Payload
    // word 1 occupies the upper half of each group.
    WordOffset = 16 * (Lane >> 3) + (Lane & 7) + 8 * PayloadWord;
    break;
  case 12:
  case 16: {
    // Executed-confirmed full maps. Four lanes form a 16-word group. The
    // payload order is 0, 2, 1, 3. x3 leaves words 12..15 in each group empty.
    static constexpr unsigned PayloadPermutation[] = {0, 2, 1, 3};
    WordOffset =
        16 * (Lane >> 2) + (Lane & 3) + 4 * PayloadPermutation[PayloadWord];
    break;
  }
  default:
    llvm_unreachable("invalid direct LDS width");
  }
  return uint64_t(WordOffset) * 4;
}

std::optional<DirectLDSLayout::SourceWord>
DirectLDSLayout::getSourceWord(uint64_t ByteOffset) const {
  if ((ByteOffset & 3) != 0 || ByteOffset >= getFootprintBytes())
    return std::nullopt;

  for (unsigned Lane = 0; Lane != WaveSize; ++Lane)
    for (unsigned PayloadWord = 0; PayloadWord != getPayloadWords();
         ++PayloadWord)
      if (getByteOffset(Lane, PayloadWord) == ByteOffset)
        return SourceWord{Lane, PayloadWord};
  return std::nullopt;
}

static DirectLDSLayoutMatch matchWords(const DirectLDSLayout &Layout,
                                       ArrayRef<DirectLDSWord> Words,
                                       bool IsProducer) {
  const unsigned ExpectedCount =
      Layout.getWaveSize() * Layout.getPayloadWords();
  if (Words.size() != ExpectedCount)
    return IsProducer ? DirectLDSLayoutMatch::IncompleteProducer
                      : DirectLDSLayoutMatch::IncompleteConsumer;

  BitVector Sources(ExpectedCount);
  DenseSet<uint64_t> Destinations;
  for (const DirectLDSWord &Word : Words) {
    if (Word.Lane >= Layout.getWaveSize() ||
        Word.PayloadWord >= Layout.getPayloadWords())
      return IsProducer ? DirectLDSLayoutMatch::ProducerLayoutMismatch
                        : DirectLDSLayoutMatch::ConsumerLayoutMismatch;

    unsigned SourceIndex =
        Word.Lane * Layout.getPayloadWords() + Word.PayloadWord;
    if (Sources.test(SourceIndex) ||
        !Destinations.insert(Word.ByteOffset).second)
      return IsProducer ? DirectLDSLayoutMatch::DuplicateProducerDestination
                        : DirectLDSLayoutMatch::DuplicateConsumerAddress;
    Sources.set(SourceIndex);

    if (Layout.getByteOffset(Word.Lane, Word.PayloadWord) != Word.ByteOffset)
      return IsProducer ? DirectLDSLayoutMatch::ProducerLayoutMismatch
                        : DirectLDSLayoutMatch::ConsumerLayoutMismatch;
  }

  if (Sources.count() != ExpectedCount)
    return IsProducer ? DirectLDSLayoutMatch::IncompleteProducer
                      : DirectLDSLayoutMatch::IncompleteConsumer;
  return DirectLDSLayoutMatch::Match;
}

DirectLDSLayoutMatch
AMDGPU::matchDirectLDSLayout(const DirectLDSLayout &Layout,
                             const DirectLDSLayoutProof &Proof) {
  if (Proof.HasDynamicAddress)
    return DirectLDSLayoutMatch::DynamicAddress;
  if (Proof.HasUnknownAlias)
    return DirectLDSLayoutMatch::UnknownAlias;
  if ((Proof.TileStrideBytes & 3) != 0 ||
      Proof.TileStrideBytes < Layout.getFootprintBytes())
    return DirectLDSLayoutMatch::InvalidStride;

  DirectLDSLayoutMatch Producer =
      matchWords(Layout, Proof.ProducerDestinations, true);
  if (Producer != DirectLDSLayoutMatch::Match)
    return Producer;
  return matchWords(Layout, Proof.ConsumerAddresses, false);
}
