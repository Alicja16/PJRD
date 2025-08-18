/*
    SPDX-FileCopyrightText: 2020-2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#include "xJPEG_MarkerUtils.h"

namespace PMBB_NAMESPACE::JPEG {

//=====================================================================================================================================================================================

static inline void xAddStuffingTail(byte* restrict & Dst, const byte*& Src, const byte* LastSrc)
{
  while(Src < LastSrc)
  {
    *(Dst++) = *(Src++);
    if(*(Src - 1) == 0xFF) { *(Dst++) = 0x00; }
  }
}
static inline void xAddStuffingCount(byte* restrict & Dst, const byte*& Src, const int32 Beg, const int32 End)
{
  for(int32 i = Beg; i < End; i++)
  {
    *(Dst++) = *(Src++);
    if(*(Src - 1) == 0xFF) { *(Dst++) = 0x00; }
  }
}
void xMarkerUtils::xAddStuffingSTD(xByteBuffer* Output, xByteBuffer* Input)
{
  const byte* Src     = Input->getReadPtr();
  const byte* LastSrc = Src + Input->getDataSize();
  byte* restrict Dst = Output->getWritePtr();

  xAddStuffingTail(Dst, Src, LastSrc);

  int32 OutputLength = (int32)(Dst - Output->getWritePtr());
  Output->modifyWritten(OutputLength);
}
#if X_SIMD_CAN_USE_SSE
void xMarkerUtils::xAddStuffingSSE(xByteBuffer* Output, xByteBuffer* Input)
{
  const __m128i FF_V = _mm_set1_epi8((uint8)0xFF);

  const byte* Src     = Input->getReadPtr();
  const byte* LastSrc = Src + Input->getDataSize();
  byte* restrict Dst  = Output->getWritePtr();

  //process 16 bytes at once
  while(LastSrc - Src > 16)
  {
    __m128i SrcV = _mm_loadu_si128((__m128i*)Src);
    //check if contains any 0xFF
    __m128i EqFF_U8V = _mm_cmpeq_epi8   (SrcV, FF_V);
    uint32  EqMask   = _mm_movemask_epi8(EqFF_U8V  );
    _mm_storeu_si128((__m128i*)Dst, SrcV);
    if(!EqMask)
    {
      Dst += 16; Src += 16;
    }
    else
    {
      uint32 LocationFirst = xTZCNT(EqMask);
      Dst += LocationFirst; Src += LocationFirst;
      xAddStuffingCount(Dst, Src, LocationFirst, 16);
    }
  }
  xAddStuffingTail(Dst, Src, LastSrc);

  int32 OutputLength = (int32)(Dst - Output->getWritePtr());
  Output->modifyWritten(OutputLength);
}
#endif //X_SIMD_CAN_USE_SSE
#if X_SIMD_CAN_USE_AVX
inline void xAddStuffingBlock256(byte* restrict & Dst, const byte*& Src, uint32 EqMask, __m256i& Src_U8V)
{
  if(!EqMask)
  {
    _mm256_storeu_si256((__m256i*)Dst, Src_U8V); Dst += 32; Src += 32;
  }
  else if((EqMask & 0x0000FFFF) == 0) //only 1st half contains 0xFFs
  {
    _mm_storeu_si128((__m128i*)Dst, _mm256_castsi256_si128(Src_U8V)); Dst += 16; Src += 16;
    xAddStuffingCount(Dst, Src, 0, 16);
  }
  else if((EqMask & 0xFFFF0000) == 0) //only 2nd half contains 0xFFs
  {
    xAddStuffingCount(Dst, Src, 0, 16);
    _mm_storeu_si128((__m128i*)Dst, _mm256_extracti128_si256(Src_U8V, 1)); Dst += 16; Src += 16;
  }
  else //both halfs contains 0xFFs
  {
    xAddStuffingCount(Dst, Src, 0, 32);
  }  
}
void xMarkerUtils::xAddStuffingAVX(xByteBuffer* Output, xByteBuffer* Input)
{
  const __m256i FF_V = _mm256_set1_epi8((uint8)0xFF);

  const byte* Src     = Input->getReadPtr();
  const byte* LastSrc = Src + Input->getDataSize();
  byte* restrict Dst  = Output->getWritePtr();

  //process 32 bytes at once
  while(LastSrc - Src > 32)
  {
    __m256i Src_U8V  = _mm256_loadu_si256((__m256i*)Src);
    //check if contains any 0xFF
    __m256i ExFF_U8V = _mm256_cmpeq_epi8   (Src_U8V, FF_V);
    uint32  EqMask   = _mm256_movemask_epi8(ExFF_U8V     );
    xAddStuffingBlock256(Dst, Src, EqMask, Src_U8V);
  }
  //process tail
  xAddStuffingTail(Dst, Src, LastSrc);

  int32 OutputLength = (int32)(Dst - Output->getWritePtr());
  Output->modifyWritten(OutputLength);
}
#endif //X_SIMD_CAN_USE_AVX
#if X_SIMD_CAN_USE_AVX512
void xMarkerUtils::xAddStuffingAVX512(xByteBuffer* Output, xByteBuffer* Input)
{
  const __m512i FF_V = _mm512_set1_epi8((uint8)0xFF);

  const byte* Src     = Input->getReadPtr();
  const byte* LastSrc = Src + Input->getDataSize();
  byte* restrict Dst  = Output->getWritePtr();

  //process 64 bytes at once
  while(LastSrc - Src > 64)
  {
    __m512i Src_U8V = _mm512_loadu_si512((__m512i*)Src);    
    uint64  EqMask  = _mm512_cmpeq_epi8_mask(Src_U8V, FF_V); //check if contains any 0xFF
    if(!EqMask)
    {
      _mm512_storeu_si512((__m512i*)Dst, Src_U8V);
      Dst += 64; Src += 64;
    }
    else 
    {
      if(EqMask == 0x8000000000000000ul) //edge case - 0xFF was in last byte
      {
        _mm512_storeu_si512((__m512i*)Dst, Src_U8V); Src += 64; Dst += 64;
        *(Dst++) = 0x00;
      }
      else
      {
        uint32  HalfEqMask = (uint32)(EqMask & 0x00000000FFFFFFFFul);
        __m256i HalfSrc_U8V = _mm512_extracti64x4_epi64(Src_U8V, 0);
        xAddStuffingBlock256(Dst, Src, HalfEqMask, HalfSrc_U8V);

        HalfEqMask = (uint32)((EqMask & 0xFFFFFFFF00000000ul) >> 32);
        HalfSrc_U8V = _mm512_extracti64x4_epi64(Src_U8V, 1);
        xAddStuffingBlock256(Dst, Src, HalfEqMask, HalfSrc_U8V);
      }
    }
  }
  //process tail
  xAddStuffingTail(Dst, Src, LastSrc);

  int32 OutputLength = (int32)(Dst - Output->getWritePtr());
  Output->modifyWritten(OutputLength);
}
#endif //X_SIMD_CAN_USE_AVX512
#if X_SIMD_CAN_USE_AVX512_ZEN4
void xMarkerUtils::xAddStuffingAVX512_VBMI2(xByteBuffer* Output, xByteBuffer* Input)
{
  const __m512i FF_V = _mm512_set1_epi8((uint8)0xFF);

  const byte* Src     = Input->getReadPtr();
  const byte* LastSrc = Src + Input->getDataSize();
  byte* restrict Dst  = Output->getWritePtr();

  //process 64 bytes at once
  while(LastSrc - Src > 64)
  {
    __m512i Src_U8V = _mm512_loadu_si512((__m512i*)Src);    
    uint64  EqMask  = _mm512_cmpeq_epi8_mask(Src_U8V, FF_V); //check if contains any 0xFF
    if(!EqMask)
    {
      _mm512_storeu_si512((__m512i*)Dst, Src_U8V); Dst += 64; Src += 64;
    }
    else 
    {      
      if(EqMask == 0x8000000000000000) //edge case - 0xFF was in last byte
      {
        _mm512_storeu_si512((__m512i*)Dst, Src_U8V); Src += 64; Dst += 64;
        *(Dst++) = 0x00;        
      }
      else
      {
        uint64 ExpandMask = ~(EqMask << 1);
        uint32 Num = (uint32)_mm_popcnt_u64(~ExpandMask);
        if(Num == 1)
        {
          __m512i Dst_U8V = _mm512_maskz_expand_epi8(ExpandMask, Src_U8V);
          _mm512_storeu_si512((__m512i*)Dst, Dst_U8V);
          Src += 63; Dst += 64;
        }
        else
        {
          uint32  HalfEqMask = (uint32)(EqMask & 0x00000000FFFFFFFFul);
          __m256i HalfSrc_U8V = _mm512_extracti64x4_epi64(Src_U8V, 0);
          xAddStuffingBlock256(Dst, Src, HalfEqMask, HalfSrc_U8V);

          HalfEqMask = (uint32)((EqMask & 0xFFFFFFFF00000000ul) >> 32);
          HalfSrc_U8V = _mm512_extracti64x4_epi64(Src_U8V, 1);
          xAddStuffingBlock256(Dst, Src, HalfEqMask, HalfSrc_U8V);
        }
      }
    }
  }
  //process tail
  xAddStuffingTail(Dst, Src, LastSrc);

  int32 OutputLength = (int32)(Dst - Output->getWritePtr());
  Output->modifyWritten(OutputLength);
}
#endif //X_SIMD_CAN_USE_AVX512_ZEN4

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

static inline void xRemoveStuffingTail(byte* restrict & Dst, const byte*& Src, const byte* LastSrc)
{
  while(Src < LastSrc)
  {
    *(Dst++) = *(Src++);
    if(*(Src - 1) == 0xFF)
    {
      if(*Src == 0x00) { Src++; } //de-stuffing
      else { Src--; Dst--; break; } //marker
    }
  }
}
static inline bool xRemoveStuffingCount(byte* restrict & Dst, const byte*& Src, const int32 Beg, const int32 End)
{
  for(int32 i = Beg; i < End; i++)
  {
    uint8 CurrSrcVal = *(Src++);
    *(Dst++) = CurrSrcVal;
    if(CurrSrcVal == 0xFF)
    {
      if(*Src == 0x00) { Src++;                     } //de-stuffing
      else             { Src--; Dst--; return true; } //marker
    }
  }
  return false;
}
void xMarkerUtils::xRemoveStuffingSTD(xByteBuffer* Output, xByteBuffer* Input)
{
  const byte*    Src     = Input->getReadPtr();
  const byte*    LastSrc = Src + Input->getDataSize();
  byte* restrict Dst     = Output->getWritePtr();

  xRemoveStuffingTail(Dst, Src, LastSrc);

  int32 InputLength  = (int32)(Src - Input->getReadPtr());
  Input->modifyRead(InputLength);
  int32 OutputLength = (int32)(Dst - Output->getWritePtr());
  Output->modifyWritten(OutputLength);
}
#if X_SIMD_CAN_USE_SSE
void xMarkerUtils::xRemoveStuffingSSE(xByteBuffer* Output, xByteBuffer* Input)
{
  const __m128i FF_U8V = _mm_set1_epi8((uint8)0xFF);

  const byte*    Src     = Input->getReadPtr();
  const byte*    LastSrc = Src + Input->getDataSize();
  byte* restrict Dst     = Output->getWritePtr();

  //process 16 bytes at once
  while(LastSrc - Src > 16)
  {
    __m128i Src_U8V  = _mm_loadu_si128((__m128i*)Src);    
    __m128i EqFF_U8V = _mm_cmpeq_epi8(Src_U8V, FF_U8V); //check if contains any 0xFF
    uint32  EqMaskFF = _mm_movemask_epi8(EqFF_U8V);
    if(!EqMaskFF) { _mm_storeu_si128((__m128i*)Dst, Src_U8V); Dst += 16; Src += 16; }
    else          { if(xRemoveStuffingCount(Dst, Src, 0, 16)) { break; } }
  }

  xRemoveStuffingTail(Dst, Src, LastSrc);
  int32 InputLength  = (int32)(Src - Input->getReadPtr());
  Input->modifyRead(InputLength);
  int32 OutputLength = (int32)(Dst - Output->getWritePtr());
  Output->modifyWritten(OutputLength);
}
#endif //X_SIMD_CAN_USE_SSE

#if X_SIMD_CAN_USE_AVX
void xMarkerUtils::xRemoveStuffingAVX(xByteBuffer* Output, xByteBuffer* Input)
{
  const __m256i FF_U8V = _mm256_set1_epi8((uint8)0xFF);

  const byte* Src     = Input->getReadPtr();
  const byte* LastSrc = Src + Input->getDataSize();
  byte* restrict Dst  = Output->getWritePtr();

  //process 32 bytes at once
  while(LastSrc - Src > 32)
  {
    __m256i Src_U8V  = _mm256_loadu_si256((__m256i*)Src);
    __m256i ExFF_U8V = _mm256_cmpeq_epi8(Src_U8V, FF_U8V); //check if contains any 0xFF
    uint32  EqMaskFF = _mm256_movemask_epi8(ExFF_U8V);
    if(!EqMaskFF)
    { 
      _mm256_storeu_si256((__m256i*)Dst, Src_U8V); Dst += 32; Src += 32;
    }
    else if((EqMaskFF & 0x0000FFFF) == 0) //only 2nd half contains 0xFFs
    {
      _mm_storeu_si128((__m128i*)Dst, _mm256_castsi256_si128(Src_U8V)); Dst += 16; Src += 16;
      if(xRemoveStuffingCount(Dst, Src, 0, 16)) { break; }
    }
    else
    {
      if(xRemoveStuffingCount(Dst, Src, 0, 32)) { break; } 
    }
  }

  xRemoveStuffingTail(Dst, Src, LastSrc);
  int32 InputLength = (int32)(Src - Input->getReadPtr());
  Input->modifyRead(InputLength);
  int32 OutputLength = (int32)(Dst - Output->getWritePtr());
  Output->modifyWritten(OutputLength);
}
#endif //X_SIMD_CAN_USE_AVX

#if X_SIMD_CAN_USE_AVX512_ZEN4
void xMarkerUtils::xRemoveStuffingAVX512_VBMI2(xByteBuffer* Output, xByteBuffer* Input)
{
  const __m512i FF_V = _mm512_set1_epi8((uint8)0xFF);
  const byte* Src     = Input->getReadPtr();
  const byte* LastSrc = Src + Input->getDataSize();
  byte* restrict Dst  = Output->getWritePtr();

  //process 64 bytes at once
  while(LastSrc - Src > 64)
  {
    __m512i Src_U8V = _mm512_loadu_si512((__m512i*)Src);
    uint64  EqMaskFF = _mm512_cmpeq_epi8_mask(Src_U8V, FF_V); //check if contains any 0xFF
    if(!EqMaskFF)
    {
      _mm512_storeu_si512((__m512i*)Dst, Src_U8V); Dst += 64; Src += 64;
    }
    else if(EqMaskFF == 0x8000000000000000) //edge case - 0xFF was in last byte
    {
      _mm512_storeu_si512((__m512i*)Dst, Src_U8V); Dst += 64; Src += 64;
      if(*Src == 0x00) { Src++;               } //de-stuffing
      else             { Src--; Dst--; break; } //marker
    }
    else
    {
      uint64 EqMask00 = _mm512_cmpeq_epi8_mask(Src_U8V, _mm512_setzero_si512()); //check if contains any 0x00
      uint64 EqMaskRR = EqMaskFF << 1;
      if((EqMaskRR & EqMask00) == EqMaskRR) //check if all FFs are followed by 00
      {
        uint64 CompressMask = ~EqMaskRR;
        __m512i Dst_U8V = _mm512_maskz_compress_epi8(CompressMask, Src_U8V);
        uint64 NumToWrite = _mm_popcnt_u64(CompressMask);
        const uint64 StoreMask = ((uint64)1 << NumToWrite) - 1;
        _mm512_mask_storeu_epi8((__m512i*)Dst, StoreMask, Dst_U8V);
        Dst += NumToWrite; Src += 64;
        if(EqMaskFF & 0x8000000000000000ul) //edge case - 0xFF was in last byte
        {
          if(*Src == 0x00) { Src++;               } //de-stuffing
          else             { Src--; Dst--; break; } //marker
        }
      }
      else
      {
        if(xRemoveStuffingCount(Dst, Src, 0, 64)) { break; }
      }
    }  
  }

  xRemoveStuffingTail(Dst, Src, LastSrc);
  int32 InputLength = (int32)(Src - Input->getReadPtr());
  Input->modifyRead(InputLength);
  int32 OutputLength = (int32)(Dst - Output->getWritePtr());
  Output->modifyWritten(OutputLength);
}
#endif //X_SIMD_CAN_USE_AVX512_ZEN4

//=====================================================================================================================================================================================

} //end of namespace PMBB::JPEG