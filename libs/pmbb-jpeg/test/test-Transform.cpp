/*
    SPDX-FileCopyrightText: 2019-2023 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <functional>
#include <utility>
#include <array>
#include "xTestUtils.h"
#include "xTimeUtils.h"
#include "xMemory.h"
#include "xCommonDefJPEG.h"
#include "xJPEG_Transform.h"
#include "xJPEG_TransformConstants.h"

using namespace PMBB_NAMESPACE;
using namespace PMBB_NAMESPACE::JPEG;

//===============================================================================================================================================================================================================

constexpr int32 BA = xJPEG_Constants::c_BlockArea;

void testTransformSTD()
{
  constexpr int32 NumIters = 1024;

  std::array<uint16, BA> Src;
  std::array< int16, BA> TrC_FMFL;
  std::array< int16, BA> TrC_FBTF;
  std::array< int16, BA> TrC_IM16;
  std::array< int16, BA> TrC_IBTF;
  std::array<uint16, BA> Rec_FMFL;
  std::array<uint16, BA> Rec_FBTF;
  std::array<uint16, BA> Rec_IM16;
  std::array<uint16, BA> Rec_IBTF;

  uint32 State = xTestUtils::c_XorShiftSeed;
  for(int32 j = 0; j < NumIters; j++)
  {
    State = xTestUtils::fillRandom(Src.data(), NOT_VALID, BA, 1, 8, State);

    xTransformFLT::FwdTransformDCT_8x8_MFL(TrC_FMFL.data(), Src.data());
    xTransformFLT::FwdTransformDCT_8x8_BTF(TrC_FBTF.data(), Src.data());
    xTransformSTD::FwdTransformDCT_8x8_M16(TrC_IM16.data(), Src.data());
    xTransformSTD::FwdTransformDCT_8x8_BTF(TrC_IBTF.data(), Src.data());
    CHECK(xTestUtils::isSimilarBuffer(TrC_IBTF.data(), TrC_FMFL.data(), BA, (int16)2, true));
    CHECK(xTestUtils::isSimilarBuffer(TrC_IM16.data(), TrC_FMFL.data(), BA, (int16)2, true));
    CHECK(xTestUtils::isSimilarBuffer(TrC_IBTF.data(), TrC_FMFL.data(), BA, (int16)2, true));

    for(int32 i = 0; i < BA; i++) { TrC_FMFL[i] = TrC_FMFL[i] / 16; }
    for(int32 i = 0; i < BA; i++) { TrC_FBTF[i] = TrC_FBTF[i] / 16; }
    for(int32 i = 0; i < BA; i++) { TrC_IM16[i] = TrC_IM16[i] / 16; }
    for(int32 i = 0; i < BA; i++) { TrC_IBTF[i] = TrC_IBTF[i] / 16; }

    xTransformFLT::InvTransformDCT_8x8_MFL(Rec_FMFL.data(), TrC_IM16.data());
    xTransformFLT::InvTransformDCT_8x8_BTF(Rec_FBTF.data(), TrC_IM16.data());
    xTransformSTD::InvTransformDCT_8x8_M16(Rec_IM16.data(), TrC_IM16.data());
    xTransformSTD::InvTransformDCT_8x8_BTF(Rec_IBTF.data(), TrC_IM16.data());
    CHECK(xTestUtils::isSimilarBuffer(Rec_FBTF.data(), Rec_FMFL.data(), BA, (uint16)1 , true));
    CHECK(xTestUtils::isSimilarBuffer(Rec_IM16.data(), Rec_FMFL.data(), BA, (uint16)1 , true));
    CHECK(xTestUtils::isSimilarBuffer(Rec_IBTF.data(), Rec_FMFL.data(), BA, (uint16)1 , true));

    xTransformFLT::InvTransformDCT_8x8_MFL(Rec_FMFL.data(), TrC_FMFL.data());
    xTransformFLT::InvTransformDCT_8x8_BTF(Rec_FBTF.data(), TrC_FBTF.data());
    xTransformSTD::InvTransformDCT_8x8_M16(Rec_IM16.data(), TrC_IM16.data());
    xTransformSTD::InvTransformDCT_8x8_BTF(Rec_IBTF.data(), TrC_IBTF.data());

    CHECK(xTestUtils::isSimilarBuffer(Rec_FMFL.data(), Src.data(), BA, (uint16)2, true));
    CHECK(xTestUtils::isSimilarBuffer(Rec_IM16.data(), Src.data(), BA, (uint16)2, true));
    CHECK(xTestUtils::isSimilarBuffer(Rec_IBTF.data(), Src.data(), BA, (uint16)2, true));
  }
}

void testTransformSIMD(std::function <void(int16*, const uint16*)>RefTr, std::function <void(uint16*, const int16*)>RefInvTr,
                       std::function <void(int16*, const uint16*)>TstTr, std::function <void(uint16*, const int16*)>TstInvTr)
{
  constexpr int32 NumIters = 1024;

  std::array<uint16, BA> Src;
  std::array< int16, BA> TrC_Ref;
  std::array< int16, BA> TrC_Tst;
  std::array<uint16, BA> Rec_Ref;
  std::array<uint16, BA> Rec_Tst;

  uint32 State = xTestUtils::c_XorShiftSeed;
  for(int32 j = 0; j < NumIters; j++)
  {
    State = xTestUtils::fillRandom(Src.data(), NOT_VALID, BA, 1, 8, State);

    RefTr(TrC_Ref.data(), Src.data());
    TstTr(TrC_Tst.data(), Src.data());
    CHECK(xTestUtils::isSameBuffer(TrC_Ref.data(), TrC_Tst.data(), BA, true));

    for(int32 i = 0; i < BA; i++) { TrC_Ref[i] = TrC_Ref[i] / 16; }
    for(int32 i = 0; i < BA; i++) { TrC_Tst[i] = TrC_Tst[i] / 16; }

    RefInvTr(Rec_Ref.data(), TrC_Ref.data());
    TstInvTr(Rec_Tst.data(), TrC_Tst.data());
    CHECK(xTestUtils::isSameBuffer(Rec_Ref.data(), Rec_Tst.data(), BA, true));
  }
}

//===============================================================================================================================================================================================================

TEST_CASE("xTransformSTD")
{
  testTransformSTD();
}

#if X_SIMD_CAN_USE_SSE
TEST_CASE("xTransformSSE_M16")
{
  testTransformSIMD(xTransformSTD::FwdTransformDCT_8x8_M16, xTransformSTD::InvTransformDCT_8x8_M16, xTransformSSE::FwdTransformDCT_8x8_M16, xTransformSSE::InvTransformDCT_8x8_M16);
}
#endif //X_SIMD_CAN_USE_SSE

#if X_SIMD_CAN_USE_AVX
TEST_CASE("xTransformAVX_M16")
{
  testTransformSIMD(xTransformSTD::FwdTransformDCT_8x8_M16, xTransformSTD::InvTransformDCT_8x8_M16, xTransformAVX::FwdTransformDCT_8x8_M16, xTransformAVX::InvTransformDCT_8x8_M16);
}
#endif //X_SIMD_CAN_USE_AVC

#if X_SIMD_CAN_USE_AVX512
TEST_CASE("xTransformAVX512_M16")
{
  testTransformSIMD(xTransformSTD::FwdTransformDCT_8x8_M16, xTransformSTD::InvTransformDCT_8x8_M16, xTransformAVX512::FwdTransformDCT_8x8_M16, xTransformAVX512::InvTransformDCT_8x8_M16);
}
#endif //X_SIMD_CAN_USE_AVX512

//===============================================================================================================================================================================================================

