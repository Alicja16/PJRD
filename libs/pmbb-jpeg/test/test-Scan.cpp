/*
    SPDX-FileCopyrightText: 2019-2023 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <functional>
#include <utility>
#include <array>
#include <tuple>
#include "xTestUtils.h"
#include "xTimeUtils.h"
#include "xMemory.h"
#include "xCommonDefJPEG.h"
#include "xJPEG_Scan.h"
#include "xJPEG_Constants.h"

using namespace PMBB_NAMESPACE;
using namespace PMBB_NAMESPACE::JPEG;

constexpr int32 BA = xJPEG_Constants::c_BlockArea;

//===============================================================================================================================================================================================================

void testScan(std::function <void(int16*, const int16*)>Scan, std::function <void(int16*, const int16*)>InvScan)
{  
  const std::array<int16, BA> TmpSequence   = [] { std::array<int16, BA> R{}; for(size_t i = 0; i < R.size(); i++) { R[i] = (int16)i                        ; } return R; }();
  const std::array<int16, BA> TmpScanZigZag = [] { std::array<int16, BA> R{}; for(size_t i = 0; i < R.size(); i++) { R[i] = xJPEG_Constants::m_ScanZigZag[i]; } return R; }();

  std::array<int16, BA> Src = { 0 };
  std::array<int16, BA> Dst = { 0 };

  CAPTURE("Forward Scan");
  Src = TmpSequence;
  Scan(Dst.data(), Src.data());
  CHECK(xTestUtils::isSameBuffer(Dst.data(), TmpScanZigZag.data(), BA, true));

  CAPTURE("Inverse Scan");
  Src = TmpScanZigZag;
  InvScan(Dst.data(), Src.data());
  CHECK(xTestUtils::isSameBuffer(Dst.data(), TmpSequence.data(), BA, true));
}

//===============================================================================================================================================================================================================

TEST_CASE("xScanSTD")
{
  testScan(xScanSTD::Scan, xScanSTD::InvScan);
}

#if X_SIMD_CAN_USE_AVX512
TEST_CASE("xScanAVX512")
{
  testScan(xScanAVX512::Scan, xScanAVX512::InvScan);
}
#endif

//===============================================================================================================================================================================================================


