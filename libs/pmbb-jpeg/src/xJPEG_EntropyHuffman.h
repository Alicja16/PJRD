/*
    SPDX-FileCopyrightText: 2019-2023 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include "xCommonDefJPEG.h"
#include "xJFIF.h"
#include "xJPEG_Huffman.h"
#include "xBitstream.h"

namespace PMBB_NAMESPACE::JPEG {

//=====================================================================================================================================================================================

class xEntropyHuffCommon
{
public:
  using tLDCs = std::array<int16, xJPEG_Constants::c_MaxComponents>;
  static constexpr tLDCs c_InitLastDCs = { 0, 0, 0, 0 };
  static constexpr tLDCs c_NopeLastDCs = { NOT_VALID, NOT_VALID, NOT_VALID, NOT_VALID };
};

//=====================================================================================================================================================================================

class PMBB_ALIGN_CACHE xEntropyHuffDecoder : public xEntropyHuffCommon
{
protected:
  const xHuffDecoder* m_HuffDecoderDC[xJPEG_Constants::c_MaxHuffTabs] = { nullptr };
  const xHuffDecoder* m_HuffDecoderAC[xJPEG_Constants::c_MaxHuffTabs] = { nullptr };
  xBitstreamReader    m_Bitstream;
  tLDCs               m_LastDC = c_NopeLastDCs;

public:
  void SetDecoders(const xHuffDecoderBank& HuffDecoderBank);

  void StartSlice (xByteBuffer* ByteBuffer);
  void FinishSlice();
  void DecodeBlock(int16* ScanCoeff, eCmp Cmp, int32 HuffTableIdDC, int32 HuffTableIdAC);
};

//=====================================================================================================================================================================================

class PMBB_ALIGN_CACHE xEntropyHuffEncoder : public xEntropyHuffCommon
{
protected:
  const xHuffEncoderDC* m_HuffEncoderDC[xJPEG_Constants::c_MaxHuffTabs];
  const xHuffEncoderAC* m_HuffEncoderAC[xJPEG_Constants::c_MaxHuffTabs];
  xBitstreamWriter      m_Bitstream;
  tLDCs                 m_LastDC = c_NopeLastDCs;

public:
  void  SetEncoders(const xHuffEncoderBank& HuffEncoderBank);

  void  StartSlice (xByteBuffer* ByteBuffer);
  void  FinishSlice();
  void  StartChunk (xByteBuffer* ByteBuffer, const tLDCs& LastDCs);
  int32 FinishChunk();
  void  EncodeBlock(const int16* ScanCoeff, eCmp Cmp, int32 HuffTableIdDC, int32 HuffTableIdAC);
};

//=====================================================================================================================================================================================

class PMBB_ALIGN_CACHE xEntropyHuffEncoderDefault : public xEntropyHuffCommon
{
protected:
  xBitstreamWriter m_Bitstream;
  tLDCs            m_LastDC = c_NopeLastDCs;

public:
  void  StartSlice (xByteBuffer* ByteBuffer);
  void  FinishSlice();
  void  EncodeBlock(int16* ScanCoeff, eCmp Cmp) { Cmp == eCmp::LM ? xEncodeBlockL(ScanCoeff) : xEncodeBlockC(ScanCoeff, Cmp); }

protected:
  void  xEncodeBlockL(const int16* ScanCoeff          );
  void  xEncodeBlockC(const int16* ScanCoeff, eCmp Cmp);
};

//=====================================================================================================================================================================================

class PMBB_ALIGN_CACHE xEntropyHuffEstimator : public xEntropyHuffCommon
{
protected:
  xHuffEstimatorDC* m_HuffEstimatorDC[xJPEG_Constants::c_MaxHuffTabs];
  xHuffEstimatorAC* m_HuffEstimatorAC[xJPEG_Constants::c_MaxHuffTabs];

public:
  xEntropyHuffEstimator () { memset(m_HuffEstimatorDC, 0, sizeof(m_HuffEstimatorDC)); memset(m_HuffEstimatorAC, 0, sizeof(m_HuffEstimatorAC)); }
  ~xEntropyHuffEstimator() { UnInit(); }
  bool  Init  (const std::vector<xJFIF::xHuffTable>& HuffTables);
  void  UnInit();

  int32 EstimateBlock  (const int16* ScanCoeff, int32 LastDC, int32 HuffTableIdDC, int32 HuffTableIdAC) const;
  int32 EstimateBlockDC(const int16* ScanCoeff, int32 LastDC, int32 HuffTableIdDC                     ) const;
  int32 EstimateBlockAC(const int16* ScanCoeff,                                    int32 HuffTableIdAC) const;
};

//=====================================================================================================================================================================================

class PMBB_ALIGN_CACHE xEntropyHuffEstimatorDefault : public xEntropyHuffCommon
{
public:
  bool  Init  (const std::vector<xJFIF::xHuffTable>& /*HuffTables*/) { return true; };

  static int32 EstimateBlock(const int16* ScanCoeff, int32 LastDC, eCmp Cmp);
protected:
  static int32 xEstimateBlockL(const int16* ScanCoeff, int32 LastDC);
  static int32 xEstimateBlockC(const int16* ScanCoeff, int32 LastDC);
};

//=====================================================================================================================================================================================

class PMBB_ALIGN_CACHE xEntropyHuffCounter : public xEntropyHuffCommon
{
protected:
  std::array<xHuffCounterDC, xJPEG_Constants::c_MaxHuffTabs> m_HuffCounterDC;
  std::array<xHuffCounterAC, xJPEG_Constants::c_MaxHuffTabs> m_HuffCounterAC;
  std::array<bool          , xJPEG_Constants::c_MaxHuffTabs> m_ActiveDC = { false };
  std::array<bool          , xJPEG_Constants::c_MaxHuffTabs> m_ActiveAC = { false };

public:
  void  Init  (const std::vector<xJFIF::xHuffTable>& HuffTables);
  void  UnInit();

  void  ZeroCounters();
  void  CountBlock  (const int16* ScanCoeff, int32 LastDC, int32 HuffTableIdDC, int32 HuffTableIdAC);
  void  AddCounters (const xEntropyHuffCounter& Other);

  const uint32* getSymbolCountDC(int32 HuffTableIdDC) { return m_HuffCounterDC[HuffTableIdDC].getSymbolCount(); }
  const uint32* getSymbolCountAC(int32 HuffTableIdAC) { return m_HuffCounterAC[HuffTableIdAC].getSymbolCount(); }

  const uint32* getSymbolCount(xJFIF::xHuffTable::tClass HuffClass, int32 HuffTableId)
  {
    if(HuffClass == xJFIF::xHuffTable::tClass::DC) { return m_HuffCounterDC[HuffTableId].getSymbolCount(); }
    if(HuffClass == xJFIF::xHuffTable::tClass::AC) { return m_HuffCounterAC[HuffTableId].getSymbolCount(); }
    return nullptr;
  }
};

//=====================================================================================================================================================================================

} //end of namespace PMBB::JPEG