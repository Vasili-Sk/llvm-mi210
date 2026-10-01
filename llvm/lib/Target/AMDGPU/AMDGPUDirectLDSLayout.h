//===- AMDGPUDirectLDSLayout.h - gfx90a direct LDS layout ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_AMDGPU_AMDGPUDIRECTLDSLAYOUT_H
#define LLVM_LIB_TARGET_AMDGPU_AMDGPUDIRECTLDSLAYOUT_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include <cstdint>
#include <optional>

namespace llvm {
namespace AMDGPU {

struct DirectLDSWord {
  unsigned Lane;
  unsigned PayloadWord;
  uint64_t ByteOffset;
};

/// A target-specific model of one wave's direct global-to-LDS placement.
/// Tile meaning is deliberately absent. A caller must supply that meaning when
/// it builds a complete DirectLDSLayoutProof.
class DirectLDSLayout {
public:
  struct SourceWord {
    unsigned Lane;
    unsigned PayloadWord;
  };

  /// Return no layout unless GPU is exactly gfx90a, WaveSize is 64, and
  /// WidthBytes is one of 4, 8, 12, or 16.
  static std::optional<DirectLDSLayout> create(StringRef GPU, unsigned WaveSize,
                                               unsigned WidthBytes);

  unsigned getWaveSize() const { return WaveSize; }
  unsigned getWidthBytes() const { return WidthBytes; }
  unsigned getPayloadWords() const { return WidthBytes / 4; }
  unsigned getFootprintBytes() const;

  /// Map a source lane and payload word to a byte offset relative to m0.
  std::optional<uint64_t> getByteOffset(unsigned Lane,
                                        unsigned PayloadWord) const;

  /// Return no source for an out-of-range offset, an unaligned offset, or a
  /// hole in a non-contiguous layout.
  std::optional<SourceWord> getSourceWord(uint64_t ByteOffset) const;

private:
  DirectLDSLayout(unsigned WaveSize, unsigned WidthBytes)
      : WaveSize(WaveSize), WidthBytes(WidthBytes) {}

  unsigned WaveSize;
  unsigned WidthBytes;
};

enum class DirectLDSLayoutMatch {
  Match,
  DynamicAddress,
  UnknownAlias,
  InvalidStride,
  IncompleteProducer,
  IncompleteConsumer,
  DuplicateProducerDestination,
  DuplicateConsumerAddress,
  ProducerLayoutMismatch,
  ConsumerLayoutMismatch,
};

/// A complete proof for replacing a staged tile layout. ProducerDestinations
/// and ConsumerAddresses must each contain every (lane, payload word) exactly
/// once. Their byte offsets are relative to the same tile base. TileStrideBytes
/// is the distance to the next tile and must contain this layout's footprint.
struct DirectLDSLayoutProof {
  ArrayRef<DirectLDSWord> ProducerDestinations;
  ArrayRef<DirectLDSWord> ConsumerAddresses;
  uint64_t TileStrideBytes;
  bool HasDynamicAddress = false;
  bool HasUnknownAlias = false;
};

/// Prove that all producer destinations and all consumer addresses exactly
/// implement Layout. This routine rejects incomplete information.
DirectLDSLayoutMatch matchDirectLDSLayout(const DirectLDSLayout &Layout,
                                          const DirectLDSLayoutProof &Proof);

} // namespace AMDGPU
} // namespace llvm

#endif
