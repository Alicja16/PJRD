/*
    SPDX-FileCopyrightText: 2020-2025 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#include "xJPEG_OptUtils.h"

namespace PMBB_NAMESPACE::JPEG {

//=====================================================================================================================================================================================

#if X_SIMD_CAN_USE_AVX512
uint64 xOptUtils::approxSSDfromCoeffsAVX512(const int16* TstCoeffs, const int16* RefCoeffs)
{
  __m512i Tst_I16_V0   = _mm512_loadu_si512(TstCoeffs     );
  __m512i Tst_I16_V1   = _mm512_loadu_si512(TstCoeffs + 32);
  __m512i Ref_I16_V0   = _mm512_loadu_si512(RefCoeffs     );
  __m512i Ref_I16_V1   = _mm512_loadu_si512(RefCoeffs + 32);
  __m512i Tst_I16_V0A  = _mm512_cvtepi16_epi32(_mm512_castsi512_si256   (Tst_I16_V0   ));
  __m512i Tst_I16_V0B  = _mm512_cvtepi16_epi32(_mm512_extracti64x4_epi64(Tst_I16_V0, 1));
  __m512i Tst_I16_V1A  = _mm512_cvtepi16_epi32(_mm512_castsi512_si256   (Tst_I16_V1   ));
  __m512i Tst_I16_V1B  = _mm512_cvtepi16_epi32(_mm512_extracti64x4_epi64(Tst_I16_V1, 1));
  __m512i Ref_I16_V0A  = _mm512_cvtepi16_epi32(_mm512_castsi512_si256   (Ref_I16_V0   ));
  __m512i Ref_I16_V0B  = _mm512_cvtepi16_epi32(_mm512_extracti64x4_epi64(Ref_I16_V0, 1));
  __m512i Ref_I16_V1A  = _mm512_cvtepi16_epi32(_mm512_castsi512_si256   (Ref_I16_V1   ));
  __m512i Ref_I16_V1B  = _mm512_cvtepi16_epi32(_mm512_extracti64x4_epi64(Ref_I16_V1, 1));
  __m512i Diff_I32_V0A = _mm512_sub_epi32(_mm512_slli_epi32(Tst_I16_V0A, c_HeadRoom), Ref_I16_V0A);
  __m512i Diff_I32_V0B = _mm512_sub_epi32(_mm512_slli_epi32(Tst_I16_V0B, c_HeadRoom), Ref_I16_V0B);
  __m512i Diff_I32_V1A = _mm512_sub_epi32(_mm512_slli_epi32(Tst_I16_V1A, c_HeadRoom), Ref_I16_V1A);
  __m512i Diff_I32_V1B = _mm512_sub_epi32(_mm512_slli_epi32(Tst_I16_V1B, c_HeadRoom), Ref_I16_V1B);
  __m512i Dist_I32_V0A = _mm512_mullo_epi32(Diff_I32_V0A, Diff_I32_V0A);
  __m512i Dist_I32_V0B = _mm512_mullo_epi32(Diff_I32_V0B, Diff_I32_V0B);
  __m512i Dist_I32_V1A = _mm512_mullo_epi32(Diff_I32_V1A, Diff_I32_V1A);
  __m512i Dist_I32_V1B = _mm512_mullo_epi32(Diff_I32_V1B, Diff_I32_V1B);
  __m512i SSD_I32_V    = _mm512_add_epi32(_mm512_add_epi32(Dist_I32_V0A, Dist_I32_V0B), _mm512_add_epi32(Dist_I32_V1A, Dist_I32_V1B));

  uint64 TmpSSD = xHorVecSumI32_epi32(SSD_I32_V);
  uint64 SSD    = (TmpSSD + c_HeadRoomAdd) >> c_HeadRoomShft;
  return SSD;
}
#endif //X_SIMD_CAN_USE_AVX512

#if X_SIMD_CAN_USE_AVX
uint64 xOptUtils::approxSSDfromCoeffsAVX(const int16* TstCoeffs, const int16* RefCoeffs)
{
  __m256i SSD_I32_V = _mm256_setzero_si256();
  for(int32 i = 0; i < c_BA; i += 16)
  {
    __m256i Tst_I16_V   = _mm256_loadu_si256((__m256i*)(TstCoeffs + i));
    __m256i Ref_I16_V   = _mm256_loadu_si256((__m256i*)(RefCoeffs + i));
    __m256i Tst_I32_VA  = _mm256_cvtepi16_epi32(_mm256_castsi256_si128  (Tst_I16_V   ));
    __m256i Tst_I32_VB  = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(Tst_I16_V, 1));
    __m256i Ref_I32_VA  = _mm256_cvtepi16_epi32(_mm256_castsi256_si128  (Ref_I16_V   ));
    __m256i Ref_I32_VB  = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(Ref_I16_V, 1));
    __m256i Diff_I32_VA = _mm256_sub_epi32(_mm256_slli_epi32(Tst_I32_VA, c_HeadRoom), Ref_I32_VA);
    __m256i Diff_I32_VB = _mm256_sub_epi32(_mm256_slli_epi32(Tst_I32_VB, c_HeadRoom), Ref_I32_VB);
    __m256i Dist_I32_VA = _mm256_mullo_epi32(Diff_I32_VA, Diff_I32_VA);
    __m256i Dist_I32_VB = _mm256_mullo_epi32(Diff_I32_VB, Diff_I32_VB);
    SSD_I32_V = _mm256_add_epi32(SSD_I32_V, _mm256_add_epi32(Dist_I32_VA, Dist_I32_VB));
  }

  uint64 TmpSSD = xHorVecSumI32_epi32(SSD_I32_V);
  uint64 SSD    = (TmpSSD + c_HeadRoomAdd) >> c_HeadRoomShft;
  return SSD;
}
#endif //X_SIMD_CAN_USE_AVX

#if X_SIMD_CAN_USE_SSE
uint64 xOptUtils::approxSSDfromCoeffsSSE(const int16* TstCoeffs, const int16* RefCoeffs)
{
  __m128i SSD_I32_V = _mm_setzero_si128();
  for(int32 i = 0; i < c_BA; i += 8)
  {
    __m128i Tst_I16_V   = _mm_loadu_si128((__m128i*)(TstCoeffs + i));
    __m128i Ref_I16_V   = _mm_loadu_si128((__m128i*)(RefCoeffs + i));
    __m128i Tst_I32_VA  = _mm_cvtepi16_epi32(               Tst_I16_V    );
    __m128i Tst_I32_VB  = _mm_cvtepi16_epi32(_mm_srli_si128(Tst_I16_V, 8));
    __m128i Ref_I32_VA  = _mm_cvtepi16_epi32(               Ref_I16_V    );
    __m128i Ref_I32_VB  = _mm_cvtepi16_epi32(_mm_srli_si128(Ref_I16_V, 8));
    __m128i Diff_I32_VA = _mm_sub_epi32(_mm_slli_epi32(Tst_I32_VA, c_HeadRoom), Ref_I32_VA);
    __m128i Diff_I32_VB = _mm_sub_epi32(_mm_slli_epi32(Tst_I32_VB, c_HeadRoom), Ref_I32_VB);
    __m128i Dist_I32_VA = _mm_mullo_epi32(Diff_I32_VA, Diff_I32_VA);
    __m128i Dist_I32_VB = _mm_mullo_epi32(Diff_I32_VB, Diff_I32_VB);
    SSD_I32_V = _mm_add_epi32(SSD_I32_V, _mm_add_epi32(Dist_I32_VA, Dist_I32_VB));
  }

  uint64 TmpSSD = xHorVecSumI32_epi32(SSD_I32_V);
  uint64 SSD    = (TmpSSD + c_HeadRoomAdd) >> c_HeadRoomShft;
  return SSD;
}
#endif //X_SIMD_CAN_USE_SSE

uint64 xOptUtils::approxSSDfromCoeffsSTD(const int16* TstCoeffs, const int16* RefCoeffs)
{
  uint64 TmpSSD = 0;
  for(int32 i = 0; i < c_BA; i++)
  {
    TmpSSD += (uint64)xPow2(((int32)(TstCoeffs[i]) << c_HeadRoom) - (int32)(RefCoeffs[i]));
  }
  uint64 SSD = (TmpSSD + c_HeadRoomAdd) >> c_HeadRoomShft;

  return SSD;
}
flt64 xOptUtils::ExpectedEstimationError(int32 Q)
{
  flt64 LinModel = 0.000323 * (flt64)Q + 0.0040;
  flt64 ExpModel = 0.00425 * (exp(0.203 * ((flt64)Q - 80.0)) - 1.0) + 0.0298;
  flt64 Model = Q <= 80 ? LinModel : ExpModel;
  return Model;
}

//=====================================================================================================================================================================================

} //end of namespace PMBB::JPEG