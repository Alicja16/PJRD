/*
    SPDX-FileCopyrightText: 2019-2025 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include "xCommonDefJPEG.h"

namespace PMBB_NAMESPACE::JPEG {

//=====================================================================================================================================================================================

class xEntropyUtils
{
public:
#if X_SIMD_CAN_USE_AVX512
#define X_CAN_USE_AVX512 1
  static int32 findLastNonZeroAVX512(const int16* ScanCoeff);
#else //X_SIMD_CAN_USE_AVX512
#define X_CAN_USE_AVX512 0
#endif //X_SIMD_CAN_USE_AVX512

#if X_SIMD_CAN_USE_AVX
#define X_CAN_USE_AVX 1
  static int32 findLastNonZeroAVX(const int16* ScanCoeff);
#else //X_SIMD_CAN_USE_AVX
#define X_CAN_USE_AVX 0
#endif //X_SIMD_CAN_USE_AVX

#if X_SIMD_CAN_USE_SSE
#define X_CAN_USE_SSE 1
  static int32 findLastNonZeroSSE(const int16* ScanCoeff);
#else //X_SIMD_CAN_USE_SSE
#define X_CAN_USE_SSE 0
#endif //X_SIMD_CAN_USE_SSE

#if X_SIMD_CAN_USE_NEON
#define X_CAN_USE_NEON 1
  static int32 findLastNonZeroNEON(const int16* ScanCoeff);
#else //X_SIMD_CAN_USE_NEON
#define X_CAN_USE_NEON 0
#endif //X_SIMD_CAN_USE_NEON
    
  static int32 findLastNonZeroSTD(const int16* ScanCoeff);

public:
#if X_CAN_USE_AVX512
  static inline int32 findLastNonZero(const int16* ScanCoeff) { return findLastNonZeroAVX512(ScanCoeff); }
#elif X_CAN_USE_AVX
  static inline int32 findLastNonZero(const int16* ScanCoeff) { return findLastNonZeroAVX   (ScanCoeff); }
#elif X_CAN_USE_SSE
  static inline int32 findLastNonZero(const int16* ScanCoeff) { return findLastNonZeroSSE   (ScanCoeff); }
#else
  static inline int32 findLastNonZero(const int16* ScanCoeff) { return findLastNonZeroSTD   (ScanCoeff); }
#endif

#undef X_CAN_USE_AVX512
#undef X_CAN_USE_AVX
#undef X_CAN_USE_SSE
#undef X_CAN_USE_NEON
};

//=====================================================================================================================================================================================

} //end of namespace PMBB::JPEG