/*
    SPDX-FileCopyrightText: 2020-2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include "xCommonDefJPEG.h"
#include "xByteBuffer.h"

namespace PMBB_NAMESPACE::JPEG {

//=============================================================================================================================================================================

class xMarkerUtils
{
public:
#if X_SIMD_CAN_USE_AVX512_ZEN4
#define X_CAN_USE_AVX512_ZEN4 1
  static void xAddStuffingAVX512_VBMI2   (xByteBuffer* Output, xByteBuffer* Input);
  static void xRemoveStuffingAVX512_VBMI2(xByteBuffer* Output, xByteBuffer* Input);
#else //X_SIMD_CAN_USE_AVX512_ZEN4
#define X_CAN_USE_AVX512_ZEN4 0
#endif //X_SIMD_CAN_USE_AVX512_ZEN4

#if X_SIMD_CAN_USE_AVX512
#define X_CAN_USE_AVX512 1
  static void xAddStuffingAVX512(xByteBuffer* Output, xByteBuffer* Input);
#else //X_SIMD_CAN_USE_AVX512
#define X_CAN_USE_AVX512 0
#endif //X_SIMD_CAN_USE_AVX512


#if X_SIMD_CAN_USE_AVX
#define X_CAN_USE_AVX 1
  static void xAddStuffingAVX   (xByteBuffer* Output, xByteBuffer* Input);
  static void xRemoveStuffingAVX(xByteBuffer* Output, xByteBuffer* Input);
#else //X_SIMD_CAN_USE_AVX
#define X_CAN_USE_AVX 0
#endif //X_SIMD_CAN_USE_AVX

#if X_SIMD_CAN_USE_SSE
#define X_CAN_USE_SSE 1
  static void xAddStuffingSSE   (xByteBuffer* Output, xByteBuffer* Input);
  static void xRemoveStuffingSSE(xByteBuffer* Output, xByteBuffer* Input);
#else //X_SIMD_CAN_USE_SSE
#define X_CAN_USE_SSE 0
#endif //X_SIMD_CAN_USE_SSE

  static void xAddStuffingSTD   (xByteBuffer* Output, xByteBuffer* Input);
  static void xRemoveStuffingSTD(xByteBuffer* Output, xByteBuffer* Input);

#if X_SIMD_CAN_USE_AVX512_ZEN4
  static void AddStuffing(xByteBuffer* Output, xByteBuffer* Input) { xAddStuffingAVX512_VBMI2(Output, Input); }
#elif X_CAN_USE_AVX512
  static void AddStuffing(xByteBuffer* Output, xByteBuffer* Input) { xAddStuffingAVX512(Output, Input); }
#elif X_CAN_USE_AVX
  static void AddStuffing(xByteBuffer* Output, xByteBuffer* Input) { xAddStuffingAVX   (Output, Input); }
#elif X_CAN_USE_SSE
  static void AddStuffing(xByteBuffer* Output, xByteBuffer* Input) { xAddStuffingSSE   (Output, Input); }
#else
  static void AddStuffing(xByteBuffer* Output, xByteBuffer* Input) { xAddStuffingSTD   (Output, Input); }
#endif

#if X_SIMD_CAN_USE_AVX512_ZEN4
  static void RemoveStuffing(xByteBuffer* Output, xByteBuffer* Input) { xRemoveStuffingAVX512_VBMI2(Output, Input); }
//#elif X_CAN_USE_AVX512
//  static void RemoveStuffing(xByteBuffer* Output, xByteBuffer* Input) { xRemoveStuffingAVX512(Output, Input); }
#elif X_CAN_USE_AVX
  static void RemoveStuffing(xByteBuffer* Output, xByteBuffer* Input) { xRemoveStuffingAVX(Output, Input); }
#elif X_CAN_USE_SSE
  static void RemoveStuffing(xByteBuffer* Output, xByteBuffer* Input) { xRemoveStuffingSSE(Output, Input); }
#else
  static void RemoveStuffing(xByteBuffer* Output, xByteBuffer* Input) { xRemoveStuffingSTD(Output, Input); }
#endif

#undef X_CAN_USE_AVX512_ZEN4
#undef X_CAN_USE_AVX512
#undef X_CAN_USE_AVX
#undef X_CAN_USE_SSE
};

//=============================================================================================================================================================================

} //end of namespace PMBB_NAMESPACE::JPEG
