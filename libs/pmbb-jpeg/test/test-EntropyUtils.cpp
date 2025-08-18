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
#include "xJPEG_Constants.h"
#include "xJPEG_EntropyUtils.h"

using namespace PMBB_NAMESPACE;
using namespace PMBB_NAMESPACE::JPEG;

//===============================================================================================================================================================================================================

static constexpr int32 BA = xJPEG_Constants::c_BlockArea;

//===============================================================================================================================================================================================================

void testFindLastNonZero(std::function <int32(const int16*)>findLastNonZero)
{
  int16 Block[BA];
  //deterministic empty 
  {
    memset(Block, 0, BA * sizeof(int16));
    int32 N = findLastNonZero(Block);
    CHECK(N == 0);
  }  
  //deterministic single 
  for(int32 n = 0; n < BA; n++)
  {
    memset(Block, 0, BA * sizeof(int16));
    Block[n] = 1;
    int32 N = findLastNonZero(Block);
    CHECK(N == n);
  }
  //deterministic n at begin
  for(int32 n = 0; n < BA; n++)
  {
    memset(Block, 0, BA * sizeof(int16));
    for(int32 m = 0; m <= n; m++) { Block[m] = 1; }
    int32 N = findLastNonZero(Block);
    CHECK(N == n);
  }
  //deterministic n at begin
  for(int32 n = 0; n < BA; n++)
  {
    memset(Block, 0, BA * sizeof(int16));
    for(int32 m = 63; m >= n; m--) { Block[m] = 1; }
    int32 N = findLastNonZero(Block);
    CHECK(N == BA-1);
  }
}

//===============================================================================================================================================================================================================

TEST_CASE("testFindLastNonZeroSTD")
{
  testFindLastNonZero(xEntropyUtils::findLastNonZeroSTD);
}

#if X_SIMD_CAN_USE_SSE
TEST_CASE("testFindLastNonZeroSSE")
{
  testFindLastNonZero(xEntropyUtils::findLastNonZeroSSE);
}
#endif

#if X_SIMD_CAN_USE_AVX
TEST_CASE("testFindLastNonZeroAVX")
{
  testFindLastNonZero(xEntropyUtils::findLastNonZeroAVX);
}
#endif

#if X_SIMD_CAN_USE_AVX512
TEST_CASE("testFindLastNonZeroAVX512")
{
  testFindLastNonZero(xEntropyUtils::findLastNonZeroAVX512);
}
#endif

#if X_SIMD_CAN_USE_NEON
TEST_CASE("testFindLastNonZeroNEON")
{
  testFindLastNonZero(xEntropyUtils::findLastNonZeroNEON);
}
#endif

//===============================================================================================================================================================================================================
