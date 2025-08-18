/*
    SPDX-FileCopyrightText: 2020-2024 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include "xCommonDefJPEG.h"
#include "xJPEG_CodecCommon.h"
#include "xJFIF.h"
#include "xPicYUV.h"
#include "xJPEG_Quant.h"
#include "xJPEG_EntropyHuffman.h"

namespace PMBB_NAMESPACE::JPEG {

//=====================================================================================================================================================================================

class xCodecSimple : public xCodecImplCommon
{
protected:
  int32   m_Quality;
  int32   m_NumOfProcesors;

protected:
  xQuantizerSet            m_Quant;
  std::vector<xByteBuffer> m_SliceBuffers;

//Profiling
protected:
  uint64              m_Ticks__Picture = 0;
  std::vector<uint64> m_Ticks____Slice;
  std::vector<uint64> m_Ticks_____MCUs;
  std::vector<uint64> m_TicksTransform;
  std::vector<uint64> m_Ticks____Quant;
  std::vector<uint64> m_Ticks_____Scan;
  std::vector<uint64> m_Ticks__Entropy;
  std::vector<uint64> m_Ticks_Stuffing;


protected:
  void     xCreate (xThreadPool* ThreadPool);
  void     xDestroy();

public:
  std::string formatAndResetStats(const std::string Prefix, flt64 TicksPerMicroSec);
};

//=============================================================================================================================================================================

class xEncoderSimple : public xCodecSimple
{
protected:
  //encoder behaviour
  bool    m_EmitAPP0        = true;
  bool    m_EmitQuantTabs   = true;
  bool    m_EmitEntropyTabs = true;

protected:
  std::vector < xEntropyHuffEncoderDefault> m_EntropyHuffEncs;

public: 
  void   create (xThreadPool* ThreadPool = nullptr) { xCreate (ThreadPool); }
  void   destroy(                                 ) { xDestroy(          ); }       

  void   init   (int32V2 PictureSize, eCrF ChromaFormat, int32 Quality, int32 RestartInterval);
  void   setEmit(bool EmitAPP0, bool EmitQuantTabs, bool EmitEntropy);
  void   encode (const xPicYUV* InputPicture, xByteBuffer* OutputBuffer);

protected:
  void   xEncodePicture(xByteBuffer* OutputBuffer, const xPicYUV* InputPicture);
  void   xEncodeSlice  (xByteBuffer* SliceBuffer, const xPicYUV* InputPicture, int32 SliceIdx, int32 ProcesorIdx);
  void   xEncodeMCU    (const uint16* CmpPtrV[], const int32 CmpStrideV[], int32 MCU_Idx, int32 ProcesorIdx);
  void   xEncodeBlock  (const uint16* SamplesOrg, eCmp CmpId, int32 ProcesorIdx);
};

//=============================================================================================================================================================================

class xDecoderSimple : public xCodecSimple
{
protected:
  xHuffDecoderBank m_HuffDecoderBank;
  std::vector<xEntropyHuffDecoder> m_EntropyHuffDecs;

public: 
  void   create (xThreadPool* ThreadPool = nullptr) { xCreate (ThreadPool); }
  void   destroy(                                 ) { xDestroy(          ); }   

  void   init   (int32V2 PictureSize, eCrF ChromaFormat, int32 Quality, int32 RestartInterval);
  bool   init   (xByteBuffer* InputBuffer);
  void   decode (xByteBuffer* InputBuffer, xPicYUV* OutputPicture); //assumes same parameters as previous valid one - does not parse headers
  
protected:
  void   xInitProcesors();
  bool   xDecodePicture(xByteBuffer* InputBuffer, xPicYUV* OutputPicture);
  void   xDecodeSlice  (xPicYUV* OutputPicture, xByteBuffer* SliceBuffer, int32 SliceIdx, int32 ProcesorIdx);
  void   xDecodeMCU    (uint16* CmpPtrV[], const int32 CmpStrideV[], int32 MCU_Idx, int32 ProcesorIdx);
  void   xDecodeBlock  (uint16* SamplesDec, eCmp CmpId, int32 ProcesorIdx);
};

//=====================================================================================================================================================================================

} //end of namespace PMBB::JPEG