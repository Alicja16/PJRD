/*
    SPDX-FileCopyrightText: 2020-2024 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#include "xJPEG_Quant.h"
#include "xHelpersSIMD.h"

namespace PMBB_NAMESPACE::JPEG {

//=============================================================================================================================================================================
// xQuantFLT
//=============================================================================================================================================================================
void xQuantFLT::QuantScale(int16* restrict Dst, const int16* Src, const uint16* Quant, const uint16*, const uint16*)
{
  for(int32 i=0; i < 64; i++)
  {
    flt32 CoeffSrc = Src[i];
    flt32 CoeffDst = CoeffSrc / (flt32)Quant[i];
    Dst[i] = (int16)round(CoeffDst);
  }
}
void xQuantFLT::InvScale(int16* restrict Dst, const int16* Src, const uint16* Quant)
{
  for(int32 i=0; i < 64; i++)
  {
    flt32 CoeffSrc = Src[i];
    flt32 CoeffDst = CoeffSrc * (flt32)Quant[i];
    Dst[i] = (int16)CoeffDst;
  }
}
//=============================================================================================================================================================================
// xQuantSTD
//=============================================================================================================================================================================
void xQuantSTD::QuantScale(int16* restrict Dst, const int16* Src, const uint16* Correction, const uint16* Reciprocal, const uint16* Shift)
{
  for(int32 i=0; i < 64; i++)
  {
    int32 CoeffSrc = Src[i];
    int32 SignMask = CoeffSrc >> 31;
    int32 CoeffAbs = (CoeffSrc ^ SignMask) - SignMask; //make abs
    int32 CoeffQnt = (int32)((((int64)CoeffAbs + Correction[i]) * Reciprocal[i]) >> Shift[i]);
    int32 CoeffDst = (CoeffQnt ^ SignMask) - SignMask; //unmake abs
    Dst[i] = (int16)CoeffDst;
  }
}
void xQuantSTD::InvScale(int16* restrict Dst, const int16* Src, const uint16* Quant)
{
  for(int32 i=0; i < 64; i++)
  {
    int32 CoeffSrc = Src[i];
    int32 CoeffDst = CoeffSrc * Quant[i];
    Dst[i] = (int16)CoeffDst;
  }
}

//=============================================================================================================================================================================
// xQuantSSE
//=============================================================================================================================================================================
#if X_SIMD_CAN_USE_SSE
void xQuantSSE::QuantScale(int16* restrict Dst, const int16* Src, const uint16* Correction, const uint16* Reciprocal, const uint16* Scale)
{
  const __m128i OneV = _mm_set1_epi16(1);

  for(int32 i=0; i < 64; i+=8)
  {
    //load
    __m128i CoeffV = _mm_loadu_si128((__m128i*)(Src        + i));
    __m128i CorrcV = _mm_loadu_si128((__m128i*)(Correction + i));
    __m128i RecipV = _mm_loadu_si128((__m128i*)(Reciprocal + i));
    __m128i ScaleV = _mm_loadu_si128((__m128i*)(Scale      + i));
    //extract sign
    __m128i SignV  = _mm_sign_epi16(OneV, CoeffV);
    CoeffV = _mm_abs_epi16(CoeffV);
    //quant
    CoeffV = _mm_mulhi_epu16(_mm_mulhi_epu16(_mm_add_epi16(CoeffV, CorrcV), RecipV), ScaleV);
    //restore sign
    CoeffV = _mm_sign_epi16(CoeffV, SignV);
    //write
    _mm_storeu_si128((__m128i*)(Dst + i), CoeffV);
  }
}
void xQuantSSE::InvScale(int16* restrict Dst, const int16* Src, const uint16* QuantCoeff)
{
  for(int32 i=0; i < 64; i+=8)
  {
    __m128i CoeffV = _mm_loadu_si128((__m128i*)(Src        + i));
    __m128i QuantV = _mm_loadu_si128((__m128i*)(QuantCoeff + i));
    CoeffV = _mm_mullo_epi16(CoeffV, QuantV);
    _mm_storeu_si128((__m128i*)(Dst + i), CoeffV);
  }
}
#endif //X_SIMD_CAN_USE_SSE

//=============================================================================================================================================================================
// xQuantAVX
//=============================================================================================================================================================================
#if X_SIMD_CAN_USE_AVX
void xQuantAVX::QuantScale(int16* restrict Dst, const int16* Src, const uint16* Correction, const uint16* Reciprocal, const uint16* Scale)
{
  const __m256i OneV = _mm256_set1_epi16(1);

  for(int32 i=0; i < 64; i+=16)
  {
    //load
    __m256i CoeffV = _mm256_loadu_si256((__m256i*)(Src        + i));
    __m256i CorrcV = _mm256_loadu_si256((__m256i*)(Correction + i));
    __m256i RecipV = _mm256_loadu_si256((__m256i*)(Reciprocal + i));
    __m256i ScaleV = _mm256_loadu_si256((__m256i*)(Scale      + i));
    //extract sign
    __m256i SignV  = _mm256_sign_epi16(OneV, CoeffV);
    CoeffV = _mm256_abs_epi16(CoeffV);
    //quant
    CoeffV = _mm256_mulhi_epu16(_mm256_mulhi_epu16(_mm256_add_epi16(CoeffV, CorrcV), RecipV), ScaleV); //try _mm_sllv_epi32
    //restore sign
    CoeffV = _mm256_sign_epi16(CoeffV, SignV);
    //write
    _mm256_storeu_si256((__m256i*)(Dst + i), CoeffV);
  }
}
void xQuantAVX::InvScale(int16* restrict Dst, const int16* Src, const uint16* QuantCoeff)
{
  for(int32 i=0; i < 64; i+=16)
  {
    __m256i CoeffV = _mm256_loadu_si256((__m256i*)(Src        + i));
    __m256i QuantV = _mm256_loadu_si256((__m256i*)(QuantCoeff + i));
    CoeffV = _mm256_mullo_epi16(CoeffV, QuantV);
    _mm256_storeu_si256((__m256i*)(Dst + i), CoeffV);
  }
}
#endif //X_SIMD_CAN_USE_AVX

//=============================================================================================================================================================================
// xQuantAVX
//=============================================================================================================================================================================
#if X_SIMD_CAN_USE_AVX512
void xQuantAVX512::QuantScale(int16* Dst, const int16* Src, const uint16* Correction, const uint16* Reciprocal, const uint16* Scale)
{
  for(int32 i=0; i < 64; i+=32)
  {
    //load
    __m512i CoeffSrcV   = _mm512_loadu_si512((__m512i*)(Src        + i));
    __m512i CorrectionV = _mm512_loadu_si512((__m512i*)(Correction + i));
    __m512i ReciprocalV = _mm512_loadu_si512((__m512i*)(Reciprocal + i));
    __m512i ScaleV      = _mm512_loadu_si512((__m512i*)(Scale      + i));
    //extract sign
    uint32  SignMask  = _mm512_cmpgt_epi16_mask(CoeffSrcV, _mm512_setzero_si512());
    __m512i CoeffAbsV = _mm512_abs_epi16(CoeffSrcV);
    //quant
    __m512i CoeffQntV = _mm512_mulhi_epu16(_mm512_mulhi_epu16(_mm512_add_epi16(CoeffAbsV, CorrectionV), ReciprocalV), ScaleV);
    //restore sign
    __m512i CoeffNegV = _mm512_sub_epi16(_mm512_setzero_si512(), CoeffQntV); //create negative
    __m512i CoeffDstV = _mm512_mask_blend_epi16(SignMask, CoeffNegV, CoeffQntV); //select positive/negative regardless to initial sign
    //write
    _mm512_storeu_si512((__m512i*)(Dst + i), CoeffDstV);
  }
}
void xQuantAVX512::InvScale(int16* Dst, const int16* Src, const uint16* Quant)
{
  for(int32 i=0; i < 64; i+=32)
  {
    __m512i CoeffSrcV = _mm512_loadu_si512((__m512i*)(Src   + i));
    __m512i QuantV    = _mm512_loadu_si512((__m512i*)(Quant + i));
    __m512i CoeffDstV = _mm512_mullo_epi16(CoeffSrcV, QuantV);
    _mm512_storeu_si512((__m512i*)(Dst + i), CoeffDstV);
  }
}
#endif //X_SIMD_CAN_USE_AVX

//=============================================================================================================================================================================
// xQuantNEON
//=============================================================================================================================================================================
#if X_SIMD_CAN_USE_NEON
void xQuantNEON::QuantScale(int16* restrict Dst, const int16* Src, const uint16* Correction, const uint16* Reciprocal, const uint16* Shift)
{
  for(int32 i=0; i < 64; i+=8)
  {
    //load
    int16x8_t  CoeffV = vld1q_s16((int16*)(Src        + i));
    uint16x8_t CorrcV = vld1q_u16(        (Correction + i));
    uint16x8_t RecipV = vld1q_u16(        (Reciprocal + i));
    int16x8_t  ShiftV = vnegq_s16(vld1q_s16((int16*)(Shift + i))); //negate shift since right shift wil be performed as "negative" left shift
    //extract sign
    int16x8_t  SignMaskV  = vshrq_n_s16(CoeffV, 31);
    uint16x8_t CoeffAbsV  = vreinterpretq_u16_s16(vabsq_s16(CoeffV));
    //quant
    uint16x8_t CoeffV_S1  = vaddq_u16(CoeffAbsV, CorrcV);
    uint32x4_t CoeffV_S2A = vmull_u16     (vget_low_u16(CoeffV_S1), vget_low_u16(RecipV));
    uint32x4_t CoeffV_S2B = vmull_high_u16(CoeffV_S1              , RecipV              );
    uint32x4_t CoeffV_S3A = vshlq_u32(CoeffV_S2A, vmovl_s16(vget_low_s16(ShiftV)));
    uint32x4_t CoeffV_S3B = vshlq_u32(CoeffV_S2B, vmovl_high_s16(ShiftV));
    uint16x8_t QuantV     = vqmovn_high_u32(vqmovn_u32(CoeffV_S3A), CoeffV_S3B);
    //restore sign
    int16x8_t  CoeffVr    = vsubq_s16(veorq_s16(vreinterpretq_s16_u16(QuantV), SignMaskV), SignMaskV); //Mask0: (Val ^ 0) - 0 = Val; Mask0x1111: (Val ^ -1) - -1 = (~Val) + 1 = -Val
    //write
    vst1q_s16((Dst + i), CoeffVr);
  }
}
void xQuantNEON::InvScale(int16* restrict Dst, const int16* Src, const uint16* QuantCoeff)
{
  for(int32 i=0; i < 64; i+=8)
  {
    int16x8_t  CoeffV = vld1q_s16((Src        + i));
    uint16x8_t QuantV = vld1q_u16((QuantCoeff + i));
    CoeffV = vmulq_s16(CoeffV, vreinterpretq_s16_u16(QuantV));
    vst1q_s16((Dst + i), CoeffV);
  }
}
#endif //X_SIMD_CAN_USE_NEON

//=============================================================================================================================================================================
// xQuantizer
//=============================================================================================================================================================================
void xQuantizer::Init(eCmp Cmp, int32 Quality, eQTLa QuantTabLayout)
{
  byte QuantTable[xJPEG_Constants::c_BlockArea];
  xJPEG_Constants::GenerateQuantTable(QuantTable, Cmp, Quality, QuantTabLayout);
  xInit(QuantTable);
}
void xQuantizer::Init(const xJFIF::xQuantTable& QuantTable)
{
  assert(QuantTable.getPrecision() == 0);
  xInit((uint8*)(QuantTable.getTableData().data()));
}
void xQuantizer::xInit(const uint8* QuantTable)
{
  for(int32 i=0; i < 64; i++)
  {
    int32  ZigZagIdx = xJPEG_Constants::m_InvScanZigZag[i];
    uint16 QuantCoeff = QuantTable[ZigZagIdx];
    m_QuantCoeffR[i] = QuantCoeff;
    xComputeReciprocal(QuantCoeff<<xJPEG_Constants::c_FwdTransformHeadroom, m_ReciprocalR[i], m_CorrectionR[i], m_ScaleR[i], m_ShiftR[i]);
  }
}
int32 xQuantizer::xComputeReciprocal(uint16 Divisor, uint16& Reciprocal, uint16& Correction, uint16& Scale, uint16& Shift)
{
  if(Divisor == 1) //no division
  {
    Reciprocal = 1;
    Correction = 0;
    Scale      = 1;
    Shift      = 0; //no shift
    return 0;
  }
  else
  {
    int32 b = xNumSignificantBits(Divisor) - 1;
    int32 r  = 16 + b;
    int32 fq = (1 << r) / Divisor;
    int32 fr = (1 << r) % Divisor;

    int32 c = Divisor / 2; /* for rounding */

    if      (fr == 0            ) { fq >>= 1; r--; } //original divisor is power of two - Reciprocal will be one bit too large to fit in uint16
    else if (fr <= (Divisor / 2)) { c++;           } //fractional part is < 0.5 
    else                          { fq++;          } //fractional part is > 0.5

    assert(r >= 0);

    Reciprocal = (uint16)fq;
    Correction = (uint16)c;
    Scale      = (uint16)(1 << (32 - r));
    Shift      = (uint16)r;
    

    if(r <= 16) { return 0; }
    else        { return 1; }
  }
}
std::string xQuantizer::FormatCoeffs(const std::string& Prefix) const
{
  std::string Result = fmt::format("{}QuantizerCoeff\n", Prefix);

  for(int32 y = 0; y < xJPEG_Constants::c_BlockSize; y++)
  {
    Result += Prefix;
    for(int32 x = 0; x < xJPEG_Constants::c_BlockSize; x++)
    {
      Result += fmt::format("{:>3d} ", m_QuantCoeffR[(y << xJPEG_Constants::c_Log2BlockSize) + x]);
    }
    Result += "\n";
  }
  return Result;

}

//=============================================================================================================================================================================
// xQuantizerSet
//=============================================================================================================================================================================

void xQuantizerSet::Init(std::vector<xJFIF::xQuantTable>& QuantTables)
{
  for(xJFIF::xQuantTable& QuantTable : QuantTables)
  {
    int32 QuantTableIdx = QuantTable.getIdx();
    assert(QuantTableIdx >= 0 && QuantTableIdx < xJPEG_Constants::c_MaxQuantTabs);
    m_Quantizers[QuantTableIdx].Init(QuantTable);
  }
}
void xQuantizerSet::Init(int32 QuantTableIdx, eCmp Cmp, int32 Quality, eQTLa QuantTabLayout)
{
  assert(QuantTableIdx >= 0 && QuantTableIdx < xJPEG_Constants::c_MaxQuantTabs);
  m_Quantizers[QuantTableIdx].Init(Cmp, Quality, QuantTabLayout);
}

//=====================================================================================================================================================================================

} //end of namespace PMBB::JPEG