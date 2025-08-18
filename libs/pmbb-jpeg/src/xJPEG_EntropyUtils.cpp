/*
    SPDX-FileCopyrightText: 2020-2025 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#include "xJPEG_EntropyUtils.h"

namespace PMBB_NAMESPACE::JPEG {

//=====================================================================================================================================================================================

#if X_SIMD_CAN_USE_AVX512
int32 xEntropyUtils::findLastNonZeroAVX512(const int16* ScanCoeff)
{
  //begin with 2nd half
  uint32 MaskV1 = _mm512_cmpneq_epi16_mask(_mm512_loadu_si512((__m512i*)(ScanCoeff + 32)), _mm512_setzero_si512());
  if(MaskV1) { return (63 - xLZCNT(MaskV1)); }
  //continue with 1st half
  uint32 MaskV0 = _mm512_cmpneq_epi16_mask(_mm512_loadu_si512((__m512i*)(ScanCoeff     )), _mm512_setzero_si512());
  if(MaskV0) { return (31 - xLZCNT(MaskV0)); }
  //empty block but treeat DC as always existing
  return 0;
}
#endif //X_SIMD_CAN_USE_AVX512

#if X_SIMD_CAN_USE_AVX
int32 xEntropyUtils::findLastNonZeroAVX(const int16* ScanCoeff)
{
  for(int32 i = 64 - 32; i >= 0; i -= 32)
  {
    __m256i CoeffsA = _mm256_loadu_si256((__m256i*) & ScanCoeff[i     ]);
    __m256i CoeffsB = _mm256_loadu_si256((__m256i*) & ScanCoeff[i + 16]);
    __m256i Coeffs  = _mm256_packs_epi16(CoeffsA, CoeffsB);
    __m256i MaskErV = _mm256_cmpeq_epi8 (Coeffs, _mm256_setzero_si256());    
    uint32  MaskEr  = (~_mm256_movemask_epi8(MaskErV)) & 0xFFFFFFFF;
    if(MaskEr)
    {
      uint32 Mask = ((MaskEr & 0xFF0000FF) | ((MaskEr & 0x00FF0000) >> 8) | ((MaskEr & 0x0000FF00) << 8)); //swap central bytes
      return (31 - (int32)xLZCNT(Mask)) + i;
    }
  }
  //empty block but treeat DC as always existing
  return 0;
}
#endif //X_SIMD_CAN_USE_AVX

#if X_SIMD_CAN_USE_SSE
int32 xEntropyUtils::findLastNonZeroSSE(const int16* ScanCoeff)
{
  for(int32 i = 64 - 16; i >= 0; i-=16)
  {
    __m128i CoeffsA = _mm_loadu_si128((__m128i*) & ScanCoeff[i    ]);
    __m128i CoeffsB = _mm_loadu_si128((__m128i*) & ScanCoeff[i + 8]);
    __m128i Coeffs  = _mm_packs_epi16(CoeffsA, CoeffsB);
    __m128i MaskV   = _mm_cmpeq_epi8 (Coeffs, _mm_setzero_si128());
    uint32  Mask    = (~_mm_movemask_epi8(MaskV)) & 0xFFFF;
    if(Mask) { return (31 - (int32)xLZCNT(Mask)) + i; }
  }
  //empty block but treeat DC as always existing
  return 0;
}
#endif //X_SIMD_CAN_USE_SSE

int32 xEntropyUtils::findLastNonZeroSTD(const int16* ScanCoeff)
{
  for(int32 i = 63; i >= 0; i--) { if(ScanCoeff[i] != 0) { return i; } }
  //empty block but treeat DC as always existing
  return 0;
}

#if X_SIMD_CAN_USE_NEON
int32 xEntropyUtils::findLastNonZeroNEON(const int16* ScanCoeff)
{
  //https://developer.arm.com/community/arm-community-blogs/b/servers-and-cloud-computing-blog/posts/porting-x86-vector-bitmask-optimizations-to-arm-neon
  for(int32 i = 64 - 8; i >= 0; i-=8)
  {
    int16x8_t  SrcV  = vld1q_s16(ScanCoeff + i);
    uint16x8_t CmpV  = vtstq_s16(SrcV, SrcV); // 0x0000 or 0x1111
    uint8x8_t  ResV  = vshrn_n_u16(CmpV, 1);
    uint64     Mask8 = vget_lane_u64(vreinterpret_u64_u8(ResV), 0);
    if(Mask8)    
    { 
      uint64     R = __builtin_clzll(Mask8) >> 3;
      uint64     r = (15 - (int32)R) + i;
      return r;
    }
  }
  //empty block but treeat DC as always existing
  return 0;
}
#endif //X_SIMD_CAN_USE_NEON

//=====================================================================================================================================================================================

} //end of namespace PMBB::JPEG