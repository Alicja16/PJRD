/*
    SPDX-FileCopyrightText: 2019-2025 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <functional>
#include <utility>
#include <array>
#include "xTestUtils.h"
#include "xTimeUtils.h"
#include "xMemory.h"
#include "xCommonDefJPEG.h"
#include "xJPEG_EntropyHuffman.h"
#include "xJPEG_TestUtils.h"

using namespace PMBB_NAMESPACE;
using namespace PMBB_NAMESPACE::JPEG;

//===============================================================================================================================================================================================================

static constexpr int32 BA = xJPEG_Constants::c_BlockArea;

//===============================================================================================================================================================================================================

class xEntHuffEncTest : public xEntropyHuffEncoder
{
public:
  const xBitstreamWriter* getBitstream() const { return &m_Bitstream; }
};

class xEntHuffEncDefTest : public xEntropyHuffEncoderDefault
{
public:
  const xBitstreamWriter* getBitstream() const { return &m_Bitstream; }
};

//===============================================================================================================================================================================================================

void testEntropyHuffman(bool UseDefault)
{
  constexpr int32 NumIters = 64;
  constexpr int32 NumBlock = 4 * 1024;
  constexpr int32 NumPels  = NumBlock * BA;
  constexpr int64 BuffSize = NumPels * sizeof(int16);

  int16* Src = (int16*)xMemory::xAlignedMallocPageAuto(BuffSize);
  int16* Dst = (int16*)xMemory::xAlignedMallocPageAuto(BuffSize);

  xByteBuffer Buffer ; Buffer.resize(BuffSize * 4);
  
  std::vector<xJFIF::xHuffTable> HTs = xJFIF::xHuffTable::createDefaultHuffTables();

  xHuffEncoderBank    HuffEncBank; HuffEncBank.Init       (HTs        );
  xEntropyHuffEncoder EntropyEnc ; EntropyEnc .SetEncoders(HuffEncBank);

  xEntropyHuffEncoderDefault EntropyEncDef;

  xHuffDecoderBank    HuffDecBank; HuffDecBank.Init       (HTs        );
  xEntropyHuffDecoder EntropyDec ; EntropyDec .SetDecoders(HuffDecBank);

  uint32 State = xTestUtils::c_XorShiftSeed;

  for(int32 j = 0; j < NumIters; j++)
  {
    //generate
    for(int32 i = 0; i < NumBlock; i++) { State = fillRandomQuantizedTransformCoeffsBlock(Src + (i * BA), State); }

    for(int32 c = 0; c <= 1; c++)
    {
      //encode
      Buffer.reset();

      if(UseDefault)
      {
        EntropyEncDef.StartSlice(&Buffer);
        for(int32 i = 0; i < NumBlock; i++) { EntropyEncDef.EncodeBlock(Src + (i * BA), eCmp(c)); }
        EntropyEncDef.FinishSlice();
      }
      else
      {
        EntropyEnc.StartSlice(&Buffer);
        for(int32 i = 0; i < NumBlock; i++) { EntropyEnc.EncodeBlock(Src + (i * BA), eCmp(c), c, c); }
        EntropyEnc.FinishSlice();
      }

      //decode
      EntropyDec.StartSlice(&Buffer);
      for(int32 i = 0; i < NumBlock; i++) { EntropyDec.DecodeBlock(Dst + (i * BA), eCmp(c), c, c); }
      EntropyDec.FinishSlice();

      //compare
      CHECK(xTestUtils::isSameBuffer(Dst, Src, NumPels, true));
    }
  }

  xMemory::xAlignedFree(Src);
  xMemory::xAlignedFree(Dst);
}

void testEntropyHuffmanEstimator(bool UseDefault)
{
  constexpr int32 NumIters = 64;
  constexpr int32 NumBlock = 4 * 1024;
  constexpr int32 NumPels  = NumBlock * BA;
  constexpr int64 BuffSize = NumPels * sizeof(int16);

  int16* Src = (int16*)xMemory::xAlignedMallocPageAuto(BuffSize);

  xByteBuffer EntropyBuffer;
  EntropyBuffer.resize(BuffSize * 2);

  std::vector<xJFIF::xHuffTable> HTs = xJFIF::xHuffTable::createDefaultHuffTables();

  xHuffEncoderBank             HuffEncBank  ; HuffEncBank.Init(HTs);
  xEntHuffEncTest              EntropyEnc   ; EntropyEnc.SetEncoders(HuffEncBank);
  xEntHuffEncDefTest           EntropyEncDef;
  xEntropyHuffEstimator        EntropyEst   ; EntropyEst.Init(HTs);
  xEntropyHuffEstimatorDefault EntropyEstDef;

  uint32 State = xTestUtils::c_XorShiftSeed;

  for(int32 j = 0; j < NumIters; j++)
  {
    //generate
    for(int32 i = 0; i < NumBlock; i++) { State = fillRandomQuantizedTransformCoeffsBlock(Src + (i * BA), State); }

    for(int32 c = 0; c <= 1; c++)
    {
      //encode
      int32 EncBits = 0;
      EntropyBuffer.reset();
      if(UseDefault)
      {
        EntropyEncDef.StartSlice(&EntropyBuffer);
        for(int32 i = 0; i < NumBlock; i++) { EntropyEncDef.EncodeBlock(Src + (i * BA), eCmp(c)); }
        EncBits += EntropyEncDef.getBitstream()->getWrittenBits();
        EntropyEncDef.FinishSlice();
      }
      else
      {
        EntropyEnc.StartSlice(&EntropyBuffer);
        for(int32 i = 0; i < NumBlock; i++) { EntropyEnc.EncodeBlock(Src + (i * BA), eCmp(c), c, c); }
        EncBits += EntropyEnc.getBitstream()->getWrittenBits();
        EntropyEnc.FinishSlice();
      }

      //stimate
      int32 EstBits = 0;
      if(UseDefault)
      {
        for(int32 i = 0; i < NumBlock; i++)
        {
          int32 LastDC = i == 0 ? 0 : Src[(i - 1) * BA];
          EstBits += EntropyEstDef.EstimateBlock(Src + (i * BA), LastDC, eCmp(c));
        }
      }
      else
      {
        for(int32 i = 0; i < NumBlock; i++)
        { 
          int32 LastDC = i == 0 ? 0 : Src[(i - 1) * BA];
          EstBits += EntropyEst.EstimateBlock(Src + (i * BA), LastDC, c, c);
        }
      }

      //compare
      CHECK(EncBits == EstBits);
    }
  }

  xMemory::xAlignedFree(Src);
}

//===============================================================================================================================================================================================================

TEST_CASE("testEntropyHuffman")
{
  testEntropyHuffman(false);
}

TEST_CASE("testEntropyHuffmanDefault")
{
  testEntropyHuffman(true);
}

TEST_CASE("testEstimatorHuffman")
{
  testEntropyHuffmanEstimator(false);
}

TEST_CASE("testEstimatorHuffmanDefault")
{
  testEntropyHuffmanEstimator(true);
}

//===============================================================================================================================================================================================================
