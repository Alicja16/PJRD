/*
    SPDX-FileCopyrightText: 2024-2025 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include "xCommonDefJPEG.h"
#include "xJPEG_Constants.h"
#include "xHelpersSIMD.h"


namespace PMBB_NAMESPACE::JPEG {

//=====================================================================================================================================================================================

class xOptUtils
{
public:
  static constexpr int32 c_BA           = xJPEG_Constants::c_BlockArea;
  static constexpr int32 c_HeadRoom     = xJPEG_Constants::c_FwdTransformHeadroom;
  static constexpr int32 c_HeadRoomShft = xJPEG_Constants::c_FwdTransformHeadroom<<1;
  static constexpr int32 c_HeadRoomAdd  = 1 << (c_HeadRoomShft - 1);

#if X_SIMD_CAN_USE_AVX512
#define X_CAN_USE_AVX512 1
  static uint64 approxSSDfromCoeffsAVX512(const int16* TstCoeffs, const int16* RefCoeffs);
#else //X_SIMD_CAN_USE_AVX512
#define X_CAN_USE_AVX512 0
#endif //X_SIMD_CAN_USE_AVX512

#if X_SIMD_CAN_USE_AVX
#define X_CAN_USE_AVX 1
  static uint64 approxSSDfromCoeffsAVX(const int16* TstCoeffs, const int16* RefCoeffs);
#else //X_SIMD_CAN_USE_AVX
#define X_CAN_USE_AVX 0
#endif //X_SIMD_CAN_USE_AVX

#if X_SIMD_CAN_USE_SSE
#define X_CAN_USE_SSE 1
  static uint64 approxSSDfromCoeffsSSE(const int16* TstCoeffs, const int16* RefCoeffs);
#else //X_SIMD_CAN_USE_SSE
#define X_CAN_USE_SSE 0
#endif //X_SIMD_CAN_USE_SSE
    
  static uint64 approxSSDfromCoeffsSTD(const int16* TstCoeffs, const int16* RefCoeffs);

public:
#if X_CAN_USE_AVX512
  static inline uint64 approxSSDfromCoeffs(const int16* TstCoeffs, const int16* RefCoeffs) { return approxSSDfromCoeffsAVX512(TstCoeffs, RefCoeffs); }
#elif X_CAN_USE_AVX
  static inline uint64 approxSSDfromCoeffs(const int16* TstCoeffs, const int16* RefCoeffs) { return approxSSDfromCoeffsAVX   (TstCoeffs, RefCoeffs); }
#elif X_CAN_USE_SSE
  static inline uint64 approxSSDfromCoeffs(const int16* TstCoeffs, const int16* RefCoeffs) { return approxSSDfromCoeffsSSE   (TstCoeffs, RefCoeffs); }
#else
  static inline uint64 approxSSDfromCoeffs(const int16* TstCoeffs, const int16* RefCoeffs) { return approxSSDfromCoeffsSTD   (TstCoeffs, RefCoeffs); }
#endif

  static flt64 ExpectedEstimationError(int32 Q);

#undef X_CAN_USE_AVX512
#undef X_CAN_USE_AVX
#undef X_CAN_USE_SSE
};

//=====================================================================================================================================================================================

} //end of namespace PMBB::JPEG