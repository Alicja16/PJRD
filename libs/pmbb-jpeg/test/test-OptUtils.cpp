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
#include "xJPEG_TestUtils.h"
#include "xJPEG_Constants.h"
#include "xPlane.h"
#include "xJPEG_Transform.h"
#include "xJPEG_TransformConstants.h"
#include "xJPEG_Quant.h"
#include "xJPEG_CodecSimple.h"
#include "xJPEG_OptUtils.h"
#include "xDistortion.h"

using namespace PMBB_NAMESPACE;
using namespace PMBB_NAMESPACE::JPEG;

//===============================================================================================================================================================================================================

static constexpr int32 BA = xJPEG_Constants::c_BlockArea;

static constexpr int32 c_Dimms = 512;
static constexpr int32 c_BitD = 8;

constexpr int32 c_BA = xJPEG_Constants::c_BlockArea;
constexpr int32 c_BS = xJPEG_Constants::c_BlockSize;

static constexpr int32 c_NumBlocks = (c_Dimms / c_BS) * (c_Dimms / c_BS);

void testapproxSSDfromCoeffs(std::function <uint64(const int16*, const int16*)>approxSSDfromCoeffs)
{
  uint32 State = xTestUtils::c_XorShiftSeed;

  xPlane<uint16>* Src = new xPlane<uint16>({ c_Dimms , c_Dimms }, c_BitD, 0);
  xPerlinNoise::fillPerlinNoiseI(Src->getAddr(), Src->getStride(), Src->getWidth(), Src->getHeight(), Src->getBitDepth(), 8, State);

  xQuantizer Quantizer;

  PMBB_ALIGN_JPEG_BLK uint16 SamplesOrg[c_BA];
  PMBB_ALIGN_JPEG_BLK int16 CoeffsTrans[c_BA];
  PMBB_ALIGN_JPEG_BLK int16 CoeffsQuant[c_BA];
  PMBB_ALIGN_JPEG_BLK int16 CoeffsScale[c_BA];
  PMBB_ALIGN_JPEG_BLK uint16 SamplesRec[c_BA];

  for(int32 Q = 100; Q >= 0; Q--)
  {
    Quantizer.Init(eCmp::LM, Q);
    flt64 EstErr = xOptUtils::ExpectedEstimationError(Q);
    flt64 RelDiffSSD = 0.0;

    for(int32 y = 0; y < c_Dimms; y += c_BS)
    {
      const uint16* SrcPtr = Src->getAddr() + y * Src->getStride();
      xCodecCommon::loadEntireBlock(SamplesOrg, SrcPtr, Src->getStride());

      for(int32 x = 0; x < c_Dimms; x += c_BS)
      {
        xTransform::FwdTransformDCT_8x8(CoeffsTrans, SamplesOrg);
        CoeffsTrans[0] -= xTransformConstants::c_FwdDcCorr; //DC correction - JPEG requires 128 to be subtracted from every input sample - could be done be DC -= 
        Quantizer.QuantScale(CoeffsQuant, CoeffsTrans);
        Quantizer.InvScale(CoeffsScale, CoeffsQuant);
        CoeffsScale[0] += xTransformConstants::c_InvDcCorr; //DC correction - JPEG requires 128 to be subtracted from every input sample - could be done be DC -= 
        xTransform::InvTransformDCT_8x8(SamplesRec, CoeffsScale);
        int64 RefSSD = xDistortion::CalcSSD(SamplesRec, SamplesOrg, c_BA, c_BitD);
        CoeffsScale[0] -= xTransformConstants::c_InvDcCorr; //DC correction - JPEG requires 128 to be subtracted from every input sample - could be done be DC -= 
        int64 EstSSD = xOptUtils::approxSSDfromCoeffsSTD(CoeffsScale, CoeffsTrans);
        int64 TstSSD = approxSSDfromCoeffs              (CoeffsScale, CoeffsTrans);
        CHECK(EstSSD == TstSSD);
        RelDiffSSD += xAbs((flt64)(RefSSD - EstSSD) / (flt64)RefSSD);        
      } //x
    }// y
    CHECK(RelDiffSSD / (flt64)(c_NumBlocks) < (0.1 + 1.5 * EstErr));
  } //Q

}

//===============================================================================================================================================================================================================

TEST_CASE("approxSSDfromCoeffsSTD")
{
  testapproxSSDfromCoeffs(xOptUtils::approxSSDfromCoeffsSTD);
}

#if X_SIMD_CAN_USE_SSE
TEST_CASE("approxSSDfromCoeffsSSE")
{
  testapproxSSDfromCoeffs(xOptUtils::approxSSDfromCoeffsSSE);
}
#endif

#if X_SIMD_CAN_USE_AVX
TEST_CASE("approxSSDfromCoeffsAVX")
{
  testapproxSSDfromCoeffs(xOptUtils::approxSSDfromCoeffsAVX);
}
#endif

#if X_SIMD_CAN_USE_AVX512
TEST_CASE("approxSSDfromCoeffsAVX512")
{
  testapproxSSDfromCoeffs(xOptUtils::approxSSDfromCoeffsAVX512);
}
#endif

//===============================================================================================================================================================================================================
