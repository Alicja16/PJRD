/*
    SPDX-FileCopyrightText: 2020-2024 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include "xCommonDefJPEG.h"
#include "xJPEG_CodecCommon.h"
#include "xJFIF.h"
#include "xPicYUV.h"
#include "xPic.h"
#include "xJPEG_Quant.h"
#include "xJPEG_EntropyHuffman.h"
#include <array>

namespace PMBB_NAMESPACE::JPEG {

//=====================================================================================================================================================================================

class xAdvancedEncoder : public xCodecImplCommon
{
public:
  using tDistBits = std::tuple<int64V4, int64V4>;

protected:
  int32 m_Quality;
  eQTLa m_QuantTabLayout = eQTLa::INVALID;

  //encoder behaviour
  bool    m_EmitAPP0        = true;
  bool    m_EmitQuantTabs   = true;
  bool    m_EmitEntropyTabs = true;
  //optimization
  bool    m_OptQuantCoeffs     = false; //Use RDQO
  bool    m_ProcessZeroCoeffs  = false;
  eLmbd   m_LambdaEstMode      = eLmbd::INVALID;
  bool    m_OptEntropyTables   = false;
  int32   m_NumOptPassesBlock  = 0;
  int32   m_NumOptPassesPic    = 0;
  eCalkMd m_BlkOptDistCalcMode = eCalkMd::Exact;
  eCalkMd m_BlkOptBitsCalcMode = eCalkMd::Exact;

  //lambdas
  flt64V4 m_Lambda = { 1.0, 1.0, 1.0, 1.0 };

  //Tools
  xQuantizerSet m_QuantMain;
  xQuantizerSet m_QuantAuxD;
  xQuantizerSet m_QuantAuxI;  

  //Huffman
  xEntropyHuffEstimator            m_EntropyHuffEst ; //no need for multiple instances - xEntropyHuffEstimator is stateless
  std::vector<xEntropyHuffCounter> m_EntropyHuffCnts;
  xHuffEncoderBank                 m_EncoderHuffBank;
  std::vector<xEntropyHuffEncoder> m_EntropyHuffEncs;

  //Buffers
  xPicYUV* m_PicYCbCr444 = nullptr;
  xPicYUV* m_PicYCbCr4XX = nullptr;

  int16*  m_CmpCoeffsTransOrg    [c_NC];
  int16*  m_CmpCoeffsTransRec    [c_NC];
  int16*  m_CmpCoeffsQuantScan   [c_NC];
  int16*  m_CmpCoeffsQuantScanAux[c_NC];
  int16*  m_CmpCoeffsQuantScanOpt[c_NC];
  xPicYUV m_PicRec;

  std::vector<xByteBuffer> m_EncBuffers      ;
  std::vector<xByteBuffer> m_SliceBuffers    ;
  std::vector<int32      > m_NumAlignmentBits;
  xByteBuffer              m_CombineBuffer   ;

  //Profiling
  uint64 m_Ticks_Transform = 0;
  uint64 m_Ticks_QuantScan = 0;
  uint64 m_Ticks_LambdaEst = 0;
  uint64 m_Ticks__OptQuant = 0;
  uint64 m_TicksOptEntropy = 0;
  uint64 m_TicksEntropyEnc = 0;
  uint64 m_Ticks____Tables = 0;
  uint64 m_Ticks__WriteOut = 0;

public:
  void create (int32V2 PictureSize, eCrF ChromaFormat, xThreadPool* ThreadPool = nullptr);
  void destroy();

  void setMarkerEmit(bool EmitAPP0, bool EmitQuantTabs, bool EmitEntropyTabs);
  void setOptQuant  (bool OptQuantCoeffs, bool ProcessZeroCoeffs, eLmbd LambdaEstMode);
  void setOptEntropy(bool OptEntropyTables);
  void setOptPass   (int32 NumBlockOptPasses, int32 NumPicOptPasses);
  void setOptClcMode(eCalkMd BlkOptDistCalcMode, eCalkMd BlkOptBitsCalcMode);

  void initBaseMarkers();
  void initQuant      (int32 Quality, eQTLa QuantTabLayout);
  void initEntropy    (int32 RestartInterval);
  
  void encode(const xPicYUV* InputPicture, const xPicP* InputPictureRGB,  xByteBuffer* OutputBuffer);

  std::string formatAndResetStats(const std::string Prefix, flt64 TicksPerMiliSec);

protected:
  void xEncodePicture(xByteBuffer* Buffer, const xPicYUV* Picture);
  void xEncodePictureWithRGB(xByteBuffer* OutputBuffer, const xPicYUV* Picture, const xPicP* PictureRGB);

  //Fwd/Inv transform + Quant/InvScale
  void xFwdTransformPic(int16* CoeffsTransV[], const xPicYUV* Picture);
  void xFwdTransformSlc(int16* CoeffsTransV[], const uint16* CmpPtrV[], const int32 CmpStrideV[], int32 MCU_IdxBeg, int32 MCU_IdxEnd);
  void xFwdTransformMCU(int16* CoeffsTransV[], const uint16* CmpPtrV[], const int32 CmpStrideV[], int32 MCU_Idx);
  void xInvTransformPic(xPicYUV* Picture, const int16* CoeffsTransV[]);
  void xInvTransformSlc(uint16* CmpPtrV[], const int32 CmpStrideV[], const int16* CoeffsTransV[], int32 MCU_IdxBeg, int32 MCU_IdxEnd);
  void xInvTransformMCU(uint16* CmpPtrV[], const int32 CmpStrideV[], const int16* CoeffsTransV[], int32 MCU_Idx);

  void        xFwdQuantScanPic(int16* CoeffQuantScanV [], const int16* CoeffTransV[]     , const xQuantizerSet& Quant                                   );
  static void xFwdQuantScanRng(int16* CoeffQuantScan    , const int16* CoeffTrans        , const xQuantizer& Quant, int32 BlockIdxBeg, int32 BlockIdxEnd);
  void        xInvScanQuantPic(int16* CoeffTransV[]     , const int16* CoeffQuantScanV[] , const xQuantizerSet& Quant                                   );
  static void xInvScanQuantRng(int16* CoeffTrans        , const int16* CoeffQuantScan    , const xQuantizer& Quant, int32 BlockIdxBeg, int32 BlockIdxEnd);

  //Lambda estimation
  int64V4 xCalcBitsHuffPic(const int16* CoeffsQuantScanV[]);
  int64V4 xCalcBitsHuffSlc(const int16* CoeffsQuantScanV[], int32 MCU_IdxFirst, int32 MCU_IdxLast);
  int64V4 xCalcBitsHuffMCU(const int16* CoeffsQuantScanV[], int32 MCU_Idx);
  int64V4 xCalcBitsPic    (const int16* CoeffsQuantScanV[]) { return xCalcBitsHuffPic(CoeffsQuantScanV); }
  int64V4 xCalcDistPicSSD (const xPicYUV* Tst, const xPicYUV* Ref);
  int64V4 xEstDistPicSSD  (const int16* CoeffsTransOrgV[], const int16* CoeffsTransRecV[]);
  int64V4 xCalcDistPic    (const int16* CoeffsQuantScanV[], const xQuantizerSet& Quant, const xPicYUV* PictureRef);
  void    xEstimateLambda (const xPicYUV* Picture);



  //RDOQ
  void   xOptQuantHuffPic(int16* OptCoeffsQuantScanV[], const int16* CoeffsQuantScanV[], const int16* CoeffsTransOrgV[], const xPicYUV* Picture);
  void   xOptQuantHuffSlc(int16* OptCoeffsQuantScanV[], const int16* CoeffsQuantScanV[], const int16* CoeffsTransOrgV[], const xPicYUV* Picture, int32 MCU_IdxFirst, int32 MCU_IdxLast);
  void   xOptQuantHuffMCU(int16* OptCoeffsQuantScanV[], const int16* CoeffsQuantScanV[], const int16* CoeffsTransOrgV[], const uint16* CmpPtrV[], const int32 CmpStrideV[], int32 MCU_Idx);
  void   xOptQuantHuffBLK(int16* OptCoeffQuantScan, const int16* CoeffsQuantScan, const int16* CoeffsTransOrg, const uint16* SamplesOrg, eCmp CmpId, int32 LastDC);
  uint64 xCalcDistBLK     (const int16* CoeffsQuantScan, const int16* CoeffsTransOrgScan, const uint16* SamplesOrg, const xQuantizer& Quantizer);
  uint64 xCalkExactDistBLK(const int16* CoeffsQuantScan, const uint16* SamplesOrg       , const xQuantizer& Quantizer);
  uint64 xCalkApprxDistBLK(const int16* CoeffsQuantScan, const int16* CoeffsTransOrgScan, const xQuantizer& Quantizer);
  void   xOptQuantPic     (int16* OptCoeffsQuantScanV[], const int16* CoeffsQuantScanV[], const int16* CoeffsTransOrgV[], const xPicYUV* Picture) { xOptQuantHuffPic(OptCoeffsQuantScanV, CoeffsQuantScanV, CoeffsTransOrgV, Picture); }
  

  // -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 private:
     struct xCmpCandtParams
     {
         const eCmp         Cmp;
         const int16*       CoeffsQuantScanBlockV;
         const xQuantizer*  Quantizer;
         int32              EntropyIdDC;
         int32              EntropyIdAC;
         flt64              Lambda;
         int32              LastDC;
     };
  
  //RDOQ-RGB
  void   xOptQuantHuffPicRGB(int16* OptCoeffsQuantScanV[], const int16* CoeffsQuantScanV[], const int16* CoeffsTransOrgV[], const xPicP* PictureRGB);
  void   xOptQuantHuffSlcRGB(int16* OptCoeffsQuantScanV[], const int16* CoeffsQuantScanV[], const int16* CoeffsTransOrgV[], const xPicP* PictureRGB, int32 MCU_IdxFirst, int32 MCU_IdxLast);
  void   xOptQuantHuffMCURGB(int16* OptCoeffsQuantScanV[], const int16* CoeffsQuantScanV[], const int16* CoeffsTransOrgV[], const uint16* RGBPtrV[], const int32 StrideRGB, int32 MCU_Idx);
  
  void   xOptQuantHuffTestCandtsRGB(int16* OptCoeffQuantScan, const std::pair<const uint16*, eCmp> RecSamplesBlockV[2], const uint16 SamplesOrgRGB[3][c_BA], const xCmpCandtParams& Params, const int32 BitsRecSamples);
  uint64 xCalcDistRGB(const std::pair<const uint16*, eCmp> SamplesRecYCbCr[3], const uint16 SamplesOrgRGB[3][c_BA]);
  
  void   xOptQuantPicRGB(int16* OptCoeffsQuantScanV[], const int16* CoeffsQuantScanV[], const int16* CoeffsTransOrgV[], const xPicP* PictureRGB) { xOptQuantHuffPicRGB(OptCoeffsQuantScanV, CoeffsQuantScanV, CoeffsTransOrgV, PictureRGB); }

  void   xInvProcess(uint16* TmpSamples, const int16* CoeffsQuantScanBlockV, const xQuantizer& Quantizer);
  // -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------



protected:
  //Huffman tables optimization
  void   xOptHuffPic(const int16* CoeffsQuantScanV[]);
  void   xCntHuffPic(const int16* CoeffsQuantScanV[]);
  void   xCntHuffRng(const int16* CoeffsQuantScan, int32 CounterIdx, int32 HuffTabIdDC, int32 HuffTabIdAC, int32 BlockIdxBeg, int32 BlockIdxEnd, int32 BlocksPerSlice);
  void   xOptEntropyPic(const int16* CoeffsQuantScanV[]) { xOptHuffPic(CoeffsQuantScanV); }

  //Entropy encode
  void xEncodeHuffPic(const int16* CoeffsQuantScanV[]);
  void xEncodeHuffSlc(const int16* CoeffsQuantScanV[], int32 SliceIdx, int32 MCU_IdxFirst, int32 MCU_IdxLast);
  void xEncodeHuffChk(const int16* CoeffsQuantScanV[], int32 ChunkIdx, int32 MCU_IdxFirst, int32 MCU_IdxLast);
  void xEncodeHuffMCU(const int16* CoeffsQuantScanV[], int32 SliceIdx, int32 MCU_Idx);
  void xEncodePic    (const int16* CoeffsQuantScanV[]) { xEncodeHuffPic(CoeffsQuantScanV); }

  //Writeout
  void xWritePic(xByteBuffer* OutputBuffer);
};

//=====================================================================================================================================================================================

} //end of namespace PMBB::JPEG