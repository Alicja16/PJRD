/*
    SPDX-FileCopyrightText: 2020-2025 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#include "xJPEG_EntropyHuffman.h"
#include "xJPEG_HuffmanDefault.h"
#include "xJPEG_EntropyUtils.h"

namespace PMBB_NAMESPACE::JPEG {

//=====================================================================================================================================================================================

void xEntropyHuffDecoder::SetDecoders(const xHuffDecoderBank& HuffDecoderBank)
{
  for(int32 HuffTableId = 0; HuffTableId < xJPEG_Constants::c_MaxHuffTabs; HuffTableId++)
  {
    m_HuffDecoderDC[HuffTableId] = HuffDecoderBank.getHuffDecoderDC(HuffTableId);
    m_HuffDecoderAC[HuffTableId] = HuffDecoderBank.getHuffDecoderAC(HuffTableId);
  }
}
void xEntropyHuffDecoder::StartSlice(xByteBuffer* ByteBuffer)
{
  m_LastDC = c_InitLastDCs;
  m_Bitstream.bindByteBuffer(ByteBuffer);
  m_Bitstream.init();
}
void xEntropyHuffDecoder::FinishSlice()
{
  m_Bitstream.readAlign();
  m_Bitstream.uninit();
  m_Bitstream.unbindByteBuffer();
}
void xEntropyHuffDecoder::DecodeBlock(int16* ScanCoeff, eCmp Cmp, int32 HuffTableIdDC, int32 HuffTableIdAC)
{
  memset(ScanCoeff, 0, xJPEG_Constants::c_BlockArea * sizeof(int16));

  //DC coefficient
  int32 DeltaDC        = m_HuffDecoderDC[HuffTableIdDC]->readDC(&m_Bitstream);
  int16 DC             = (int16)(DeltaDC + (int32)m_LastDC[(int32)Cmp]);
  m_LastDC[(int32)Cmp] = (int16)DC;
  ScanCoeff[0] = DC;

  //AC coefficients
  const xHuffDecoder* HD = m_HuffDecoderAC[HuffTableIdAC];
  for(int32 i=1; i < 64; i++)
  {
    int32 AC = HD->readPrefix(&m_Bitstream);
    int32 R = AC >> 4;
    int32 V = AC & 0x0F;

    if(V)
    {
      i += R;
      AC = HD->readSufix(&m_Bitstream, V);
      ScanCoeff[i] = (int16)AC;
    }
    else
    {
      if(R != 15) break;
      i += 15;
    }
  }
}

//=====================================================================================================================================================================================

void xEntropyHuffEncoder::SetEncoders(const xHuffEncoderBank& HuffEncoderBank)
{
  for(int32 HuffTableId = 0; HuffTableId < xJPEG_Constants::c_MaxHuffTabs; HuffTableId++)
  {
    m_HuffEncoderDC[HuffTableId] = HuffEncoderBank.getHuffEncoderDC(HuffTableId);
    m_HuffEncoderAC[HuffTableId] = HuffEncoderBank.getHuffEncoderAC(HuffTableId);
  }
}
void xEntropyHuffEncoder::StartSlice(xByteBuffer* ByteBuffer)
{
  m_LastDC = c_InitLastDCs;
  m_Bitstream.bindByteBuffer(ByteBuffer);
  m_Bitstream.init();
}
void xEntropyHuffEncoder::FinishSlice()
{
  m_Bitstream.writeAlign(1);
  m_Bitstream.uninit();
  m_Bitstream.unbindByteBuffer();
}
void xEntropyHuffEncoder::StartChunk(xByteBuffer* ByteBuffer, const tLDCs& LastDCs)
{
  m_LastDC = LastDCs;
  m_Bitstream.bindByteBuffer(ByteBuffer);
  m_Bitstream.init();
}
int32 xEntropyHuffEncoder::FinishChunk()
{
  int32 NumAlignmentBits = m_Bitstream.writeAlign(0);
  m_Bitstream.uninit();
  m_Bitstream.unbindByteBuffer();
  return NumAlignmentBits;
}
void xEntropyHuffEncoder::EncodeBlock(const int16* ScanCoeff, eCmp Cmp, int32 HuffTableIdDC, int32 HuffTableIdAC)
{
  //DC coefficient
  int32 DC             = ScanCoeff[0];
  int32 DeltaDC        = DC - (int32)m_LastDC[(int32)Cmp];
  m_LastDC[(int32)Cmp] = (int16)DC;

  int32 SignMaskDC = DeltaDC >> 31;                       // make a mask of the sign bit
  int32 AbsDeltaDC = (DeltaDC ^ SignMaskDC) - SignMaskDC; // toggle the bits and add one if value is negative
  int32 NumBitsDC  = xNumSignificantBits((uint32)AbsDeltaDC);
  int32 RemainDC   = (DeltaDC + SignMaskDC) & ((1 << NumBitsDC) - 1);  // subtract one if value was negative and mask off any extra bits in code
  m_HuffEncoderDC[HuffTableIdDC]->writeDC(&m_Bitstream, NumBitsDC, RemainDC);

  //AC coefficients
  const xHuffEncoderAC* HE = m_HuffEncoderAC[HuffTableIdAC];
  int32 LastNonZero = xEntropyUtils::findLastNonZero(ScanCoeff);
  int32 RunLength   = 0;
  for(int32 i=1; i <= LastNonZero; i++)
  {
    int32 AC = ScanCoeff[i];

    //nothing to encode
    if(AC == 0) { RunLength++; continue; }

    //if run length > 15, must emit special run-length-16 codes (0xF0)
    while (RunLength > 15) { HE->writeZRL(&m_Bitstream); RunLength -= 16; }

    //Emit Huffman symbol for run length / number of bits
    int32 SignMaskAC = AC >> 31;                       // make a mask of the sign bit
    int32 AbsAC      = (AC ^ SignMaskAC) - SignMaskAC; // toggle the bits and add one if value is negative
    int32 NumBitsAC  = xNumSignificantBits((uint32)AbsAC);
    int32 CodeAC     = (RunLength << 4) + NumBitsAC;
    int32 RemainAC   = (AC + SignMaskAC) & ((1 << NumBitsAC) - 1);  // subtract one if value was negative and mask off any extra bits in code
    HE->writeAC(&m_Bitstream, CodeAC, NumBitsAC, RemainAC);

    //reset run length
    RunLength = 0;
  }
  //If the last coef(s) were zero, emit an end-of-block code
  if (LastNonZero < 63) { HE->writeEOB(&m_Bitstream); }
}

//=====================================================================================================================================================================================

void xEntropyHuffEncoderDefault::StartSlice(xByteBuffer* ByteBuffer)
{
  m_LastDC = c_InitLastDCs;
  m_Bitstream.bindByteBuffer(ByteBuffer);
  m_Bitstream.init();
}
void xEntropyHuffEncoderDefault::FinishSlice()
{
  m_Bitstream.writeAlign(1);
  m_Bitstream.uninit();
  m_Bitstream.unbindByteBuffer();
}
//void xEntropyHuffEncoderDefault::EncodeBlock(int16* ScanCoeff, eCmp Cmp)
//{
//  //DC coefficient
//  int32 DC             = ScanCoeff[0];
//  int32 DeltaDC        = DC - (int32)m_LastDC[(int32)Cmp];
//  m_LastDC[(int32)Cmp] = (int16)DC;
//
//  int32 SignMaskDC = DeltaDC >> 31;                       // make a mask of the sign bit
//  int32 AbsDeltaDC = (DeltaDC ^ SignMaskDC) - SignMaskDC; // toggle the bits and add one if value is negative
//  int32 NumBitsDC  = xNumBits(AbsDeltaDC);
//  int32 RemainDC   = (DeltaDC + SignMaskDC) & ((1 << NumBitsDC) - 1);  // subtract one if value was negative and mask off any extra bits in code
//  if(Cmp == eCmp::LM) { xHuffDefaultEncoder::writeLumaDC  (&m_Bitstream, NumBitsDC, RemainDC); }
//  else                { xHuffDefaultEncoder::writeChromaDC(&m_Bitstream, NumBitsDC, RemainDC); }
//
//  //AC coefficients
//  int32 LastNonZero = findLastNonZero(ScanCoeff);
//  int32 RunLength   = 0;
//  for(int32 i=1; i <= LastNonZero; i++)
//  {
//    int32 AC = ScanCoeff[i];
//
//    //nothing to encode
//    if(AC == 0) { RunLength++; continue; }
//
//    //if run length > 15, must emit special run-length-16 codes (0xF0)
//    while (RunLength > 15)
//    { 
//      if(Cmp == eCmp::LM) { xHuffDefaultEncoder::writeLumaZRL  (&m_Bitstream); }
//      else                { xHuffDefaultEncoder::writeChromaZRL(&m_Bitstream); }
//      RunLength -= 16;
//    }
//
//    //Emit Huffman symbol for run length / number of bits
//    int32 SignMaskAC = AC >> 31;                       // make a mask of the sign bit
//    int32 AbsAC      = (AC ^ SignMaskAC) - SignMaskAC; // toggle the bits and add one if value is negative
//    int32 NumBitsAC  = xNumBits(AbsAC);
//    int32 CodeAC     = (RunLength << 4) + NumBitsAC;
//    int32 RemainAC   = (AC + SignMaskAC) & ((1 << NumBitsAC) - 1);  // subtract one if value was negative and mask off any extra bits in code
//    if(Cmp == eCmp::LM) { xHuffDefaultEncoder::writeLumaAC  (&m_Bitstream, RunLength, NumBitsAC, RemainAC); }
//    else                { xHuffDefaultEncoder::writeChromaAC(&m_Bitstream, RunLength, NumBitsAC, RemainAC); }
//
//    //reset run length
//    RunLength = 0;
//  }
//  //If the last coef(s) were zero, emit an end-of-block code
//  if (LastNonZero < 63)
//  {
//    if(Cmp == eCmp::LM) { xHuffDefaultEncoder::writeLumaEOB  (&m_Bitstream); }
//    else                { xHuffDefaultEncoder::writeChromaEOB(&m_Bitstream); }
//  }
//}

void xEntropyHuffEncoderDefault::xEncodeBlockL(const int16* ScanCoeff)
{
  //DC coefficient
  int32 DC                  = ScanCoeff[0];
  int32 DeltaDC             = DC - (int32)m_LastDC[(int32)eCmp::LM];
  m_LastDC[(int32)eCmp::LM] = (int16)DC;

  int32 SignMaskDC = DeltaDC >> 31;                       // make a mask of the sign bit
  int32 AbsDeltaDC = (DeltaDC ^ SignMaskDC) - SignMaskDC; // toggle the bits and add one if value is negative
  int32 NumBitsDC  = xNumSignificantBits((uint32)AbsDeltaDC);
  int32 RemainDC   = (DeltaDC + SignMaskDC) & ((1 << NumBitsDC) - 1);  // subtract one if value was negative and mask off any extra bits in code
  xHuffDefaultEncoder::writeLumaDC(&m_Bitstream, NumBitsDC, RemainDC);

  //AC coefficients
  int32 LastNonZero = xEntropyUtils::findLastNonZero(ScanCoeff);
  int32 RunLength   = 0;
  for(int32 i=1; i <= LastNonZero; i++)
  {
    int32 AC = ScanCoeff[i];

    //nothing to encode
    if(AC == 0) { RunLength++; continue; }

    //if run length > 15, must emit special run-length-16 codes (0xF0)
    while (RunLength > 15) { xHuffDefaultEncoder::writeLumaZRL(&m_Bitstream); RunLength -= 16; }

    //Emit Huffman symbol for run length / number of bits
    int32 SignMaskAC = AC >> 31;                       // make a mask of the sign bit
    int32 AbsAC      = (AC ^ SignMaskAC) - SignMaskAC; // toggle the bits and add one if value is negative
    int32 NumBitsAC  = xNumSignificantBits((uint32)AbsAC);
    int32 RemainAC   = (AC + SignMaskAC) & ((1 << NumBitsAC) - 1);  // subtract one if value was negative and mask off any extra bits in code
    xHuffDefaultEncoder::writeLumaAC(&m_Bitstream, RunLength, NumBitsAC, RemainAC);

    //reset run length
    RunLength = 0;
  }
  //If the last coef(s) were zero - emit an end-of-block code
  if(LastNonZero < 63) { xHuffDefaultEncoder::writeLumaEOB(&m_Bitstream); }
}
void xEntropyHuffEncoderDefault::xEncodeBlockC(const int16* ScanCoeff, eCmp Cmp)
{
  //DC coefficient
  int32 DC             = ScanCoeff[0];
  int32 DeltaDC        = DC - (int32)m_LastDC[(int32)Cmp];
  m_LastDC[(int32)Cmp] = (int16)DC;

  int32 SignMaskDC = DeltaDC >> 31;                       // make a mask of the sign bit
  int32 AbsDeltaDC = (DeltaDC ^ SignMaskDC) - SignMaskDC; // toggle the bits and add one if value is negative
  int32 NumBitsDC  = xNumSignificantBits((uint32)AbsDeltaDC);
  int32 RemainDC   = (DeltaDC + SignMaskDC) & ((1 << NumBitsDC) - 1);  // subtract one if value was negative and mask off any extra bits in code
  xHuffDefaultEncoder::writeChromaDC(&m_Bitstream, NumBitsDC, RemainDC);

  //AC coefficients
  int32 LastNonZero = xEntropyUtils::findLastNonZero(ScanCoeff);
  int32 RunLength   = 0;
  for(int32 i=1; i <= LastNonZero; i++)
  {
    int32 AC = ScanCoeff[i];

    //nothing to encode
    if(AC == 0) { RunLength++; continue; }

    //if run length > 15, must emit special run-length-16 codes (0xF0)
    while (RunLength > 15) { xHuffDefaultEncoder::writeChromaZRL(&m_Bitstream); RunLength -= 16; }

    //Emit Huffman symbol for run length / number of bits
    int32 SignMaskAC = AC >> 31;                       // make a mask of the sign bit
    int32 AbsAC      = (AC ^ SignMaskAC) - SignMaskAC; // toggle the bits and add one if value is negative
    int32 NumBitsAC  = xNumSignificantBits((uint32)AbsAC);
    int32 RemainAC   = (AC + SignMaskAC) & ((1 << NumBitsAC) - 1);  // subtract one if value was negative and mask off any extra bits in code
    xHuffDefaultEncoder::writeChromaAC(&m_Bitstream, RunLength, NumBitsAC, RemainAC);

    //reset run length
    RunLength = 0;
  }
  //If the last coef(s) were zero, emit an end-of-block code
  if (LastNonZero < 63) { xHuffDefaultEncoder::writeChromaEOB(&m_Bitstream); }
}


//=====================================================================================================================================================================================

bool xEntropyHuffEstimator::Init(const std::vector<xJFIF::xHuffTable>& HuffTables)
{
  bool Result = true;
  for(const xJFIF::xHuffTable& HuffTable : HuffTables)
  {
    xJFIF::xHuffTable::tClass HuffTableClass = HuffTable.getClass();
    int32                     HuffTableId    = HuffTable.getIdx  ();
    switch(HuffTableClass)
    {
      case xJFIF::xHuffTable::tClass::DC:
        if(m_HuffEstimatorDC[HuffTableId] == nullptr) { m_HuffEstimatorDC[HuffTableId] = new xHuffEstimatorDC; }
        Result &= m_HuffEstimatorDC[HuffTableId]->init(HuffTable);
        break;
      case xJFIF::xHuffTable::tClass::AC:
        if(m_HuffEstimatorAC[HuffTableId] == nullptr) { m_HuffEstimatorAC[HuffTableId] = new xHuffEstimatorAC; }
        Result &= m_HuffEstimatorAC[HuffTableId]->init(HuffTable);
        break;
      default: Result = false; break;
    }
  }
  return Result;
}
void xEntropyHuffEstimator::UnInit()
{
  for(int32 HuffTableId=0; HuffTableId < xJPEG_Constants::c_MaxHuffTabs; HuffTableId++)
  {
    if(m_HuffEstimatorDC[HuffTableId] != nullptr) { delete m_HuffEstimatorDC[HuffTableId]; m_HuffEstimatorDC[HuffTableId] = nullptr; }
    if(m_HuffEstimatorAC[HuffTableId] != nullptr) { delete m_HuffEstimatorAC[HuffTableId]; m_HuffEstimatorAC[HuffTableId] = nullptr; }
  }
}
int32 xEntropyHuffEstimator::EstimateBlock(const int16* ScanCoeff, int32 LastDC, int32 HuffTableIdDC, int32 HuffTableIdAC) const
{
  //DC coefficient
  int32 DC          = ScanCoeff[0];
  int32 DeltaDC     = DC - LastDC;
  int32 SignMaskDC  = DeltaDC >> 31;                       // make a mask of the sign bit
  int32 AbsDeltaDC  = (DeltaDC ^ SignMaskDC) - SignMaskDC; // toggle the bits and add one if value is negative
  int32 NumBitsDC   = xNumSignificantBits((uint32)AbsDeltaDC);
  int32 CalcNumBits = m_HuffEstimatorDC[HuffTableIdDC]->calcDC(NumBitsDC);

  //AC coefficients
  xHuffEstimatorAC* HE = m_HuffEstimatorAC[HuffTableIdAC];
  int32 LastNonZero = xEntropyUtils::findLastNonZero(ScanCoeff);
  int32 RunLength   = 0;
  for(int32 i=1; i <= LastNonZero; i++)
  {
    int32 AC = ScanCoeff[i];

    //nothing to encode
    if(AC == 0) { RunLength++; continue; }

    //if run length > 15, must emit special run-length-16 codes (0xF0)
    while (RunLength > 15) { CalcNumBits += HE->calcZRL(); RunLength -= 16; }

    //Emit Huffman symbol for run length / number of bits
    int32 SignMaskAC = AC >> 31;                       // make a mask of the sign bit
    int32 AbsAC      = (AC ^ SignMaskAC) - SignMaskAC; // toggle the bits and add one if value is negative
    int32 NumBitsAC  = xNumSignificantBits((uint32)AbsAC);
    int32 CodeAC     = (RunLength << 4) + NumBitsAC;
    CalcNumBits += HE->calcAC(CodeAC, NumBitsAC);

    //reset run length
    RunLength = 0;
  }
  //If the last coef(s) were zero, emit an end-of-block code
  if (LastNonZero < 63) { CalcNumBits += HE->calcEOB(); }

  return CalcNumBits;
}
int32 xEntropyHuffEstimator::EstimateBlockDC(const int16* ScanCoeff, int32 LastDC, int32 HuffTableIdDC) const
{
  //DC coefficient
  int32 DC          = ScanCoeff[0];
  int32 DeltaDC     = DC - LastDC;
  int32 SignMaskDC  = DeltaDC >> 31;                       // make a mask of the sign bit
  int32 AbsDeltaDC  = (DeltaDC ^ SignMaskDC) - SignMaskDC; // toggle the bits and add one if value is negative
  int32 NumBitsDC   = xNumSignificantBits((uint32)AbsDeltaDC);
  int32 CalcNumBits = m_HuffEstimatorDC[HuffTableIdDC]->calcDC(NumBitsDC);
  return CalcNumBits;
}
int32 xEntropyHuffEstimator::EstimateBlockAC(const int16* ScanCoeff, int32 HuffTableIdAC) const
{
  int32 CalcNumBits = 0;

  //AC coefficients
  xHuffEstimatorAC* HE = m_HuffEstimatorAC[HuffTableIdAC];
  int32 LastNonZero = xEntropyUtils::findLastNonZero(ScanCoeff);
  int32 RunLength   = 0;
  for(int32 i=1; i <= LastNonZero; i++)
  {
    int32 AC = ScanCoeff[i];

    //nothing to encode
    if(AC == 0) { RunLength++; continue; }

    //if run length > 15, must emit special run-length-16 codes (0xF0)
    while (RunLength > 15) { CalcNumBits += HE->calcZRL(); RunLength -= 16; }

    //Emit Huffman symbol for run length / number of bits
    int32 SignMaskAC = AC >> 31;                       // make a mask of the sign bit
    int32 AbsAC      = (AC ^ SignMaskAC) - SignMaskAC; // toggle the bits and add one if value is negative
    int32 NumBitsAC  = xNumSignificantBits((uint32)AbsAC);
    int32 CodeAC     = (RunLength << 4) + NumBitsAC;
    CalcNumBits += HE->calcAC(CodeAC, NumBitsAC);

    //reset run length
    RunLength = 0;
  }
  //If the last coef(s) were zero, emit an end-of-block code
  if (LastNonZero < 63) { CalcNumBits += HE->calcEOB(); }

  return CalcNumBits;
}

//=====================================================================================================================================================================================
// xEntropyHuffEstimatorDefault
//=====================================================================================================================================================================================

int32 xEntropyHuffEstimatorDefault::EstimateBlock(const int16* ScanCoeff, int32 LastDC, eCmp Cmp)
{
  if(Cmp == eCmp::LM) { return xEstimateBlockL(ScanCoeff, ScanCoeff[0] - LastDC); }
  else                { return xEstimateBlockC(ScanCoeff, ScanCoeff[0] - LastDC); }
}
int32 xEntropyHuffEstimatorDefault::xEstimateBlockL(const int16* ScanCoeff, int32 DeltaDC)
{
  //DC coefficient
  int32 SignMaskDC = DeltaDC >> 31;                       // make a mask of the sign bit
  int32 AbsDeltaDC = (DeltaDC ^ SignMaskDC) - SignMaskDC; // toggle the bits and add one if value is negative
  int32 NumBitsDC  = xNumSignificantBits((uint32)(uint32)AbsDeltaDC);
  int32 CalcNumBits = xHuffDefaultEstimator::calcLumaDC(NumBitsDC);

  //AC coefficients
  int32 LastNonZero = xEntropyUtils::findLastNonZero(ScanCoeff);
  int32 RunLength   = 0;  
  for(int32 i=1; i <= LastNonZero; i++)
  {
    int32 AC = ScanCoeff[i];

    //nothing to encode
    if(AC == 0) { RunLength++; continue; }

    //if run length > 15, must emit special run-length-16 codes (0xF0)
    while (RunLength > 15) { CalcNumBits += xHuffDefaultEstimator::calcLumaZRL(); RunLength -= 16; }

    //Emit Huffman symbol for run length / number of bits
    int32 SignMaskAC = AC >> 31;                       // make a mask of the sign bit
    int32 AbsAC      = (AC ^ SignMaskAC) - SignMaskAC; // toggle the bits and add one if value is negative
    int32 NumBitsAC  = xNumSignificantBits((uint32)AbsAC);
    CalcNumBits += xHuffDefaultEstimator::calcLumaAC(RunLength, NumBitsAC);

    //reset run length
    RunLength = 0;
  }
  //If the last coef(s) were zero, emit an end-of-block code
  if (LastNonZero < 63) { CalcNumBits += xHuffDefaultEstimator::calcLumaEOB(); }

  return CalcNumBits;
}
int32 xEntropyHuffEstimatorDefault::xEstimateBlockC(const int16* ScanCoeff, int32 DeltaDC)
{
  //DC coefficient
  int32 SignMaskDC = DeltaDC >> 31;                       // make a mask of the sign bit
  int32 AbsDeltaDC = (DeltaDC ^ SignMaskDC) - SignMaskDC; // toggle the bits and add one if value is negative
  int32 NumBitsDC  = xNumSignificantBits((uint32)AbsDeltaDC);
  int32 CalcNumBits = xHuffDefaultEstimator::calcChromaDC(NumBitsDC);

  //AC coefficients
  int32 LastNonZero = xEntropyUtils::findLastNonZero(ScanCoeff);
  int32 RunLength   = 0;
  for(int32 i=1; i <= LastNonZero; i++)
  {
    int32 AC = ScanCoeff[i];

    //nothing to encode
    if(AC == 0) { RunLength++; continue; }

    //if run length > 15, must emit special run-length-16 codes (0xF0)
    while (RunLength > 15) { CalcNumBits += xHuffDefaultEstimator::calcChromaZRL(); RunLength -= 16; }

    //Emit Huffman symbol for run length / number of bits
    int32 SignMaskAC = AC >> 31;                       // make a mask of the sign bit
    int32 AbsAC      = (AC ^ SignMaskAC) - SignMaskAC; // toggle the bits and add one if value is negative
    int32 NumBitsAC  = xNumSignificantBits((uint32)AbsAC);
    CalcNumBits += xHuffDefaultEstimator::calcChromaAC(RunLength, NumBitsAC);

    //reset run length
    RunLength = 0;
  }
  //If the last coef(s) were zero, emit an end-of-block code
  if (LastNonZero < 63) { CalcNumBits += xHuffDefaultEstimator::calcChromaEOB(); }

  return CalcNumBits;
}

//=====================================================================================================================================================================================
// xEntropyHuffCounter
//=====================================================================================================================================================================================
void xEntropyHuffCounter::Init(const std::vector<xJFIF::xHuffTable>& HuffTables)
{
  for(const xJFIF::xHuffTable& HuffTable : HuffTables)
  {
    xJFIF::xHuffTable::tClass HuffTableClass = HuffTable.getClass();
    int32                     HuffTableId    = HuffTable.getIdx  ();
    switch(HuffTableClass)
    {
      case xJFIF::xHuffTable::tClass::DC: m_HuffCounterDC[HuffTableId].init(); m_ActiveDC[HuffTableId] = true; break;
      case xJFIF::xHuffTable::tClass::AC: m_HuffCounterAC[HuffTableId].init(); m_ActiveAC[HuffTableId] = true; break;
      default: break;
    }
  }
}
void xEntropyHuffCounter::UnInit()
{
  m_ActiveDC.fill(false);
  m_ActiveAC.fill(false);
}
void xEntropyHuffCounter::ZeroCounters()
{
  for(int32 i = 0; i < xJPEG_Constants::c_MaxHuffTabs; i++) { if(m_ActiveDC[i]) { m_HuffCounterDC[i].init(); } }
  for(int32 i = 0; i < xJPEG_Constants::c_MaxHuffTabs; i++) { if(m_ActiveAC[i]) { m_HuffCounterAC[i].init(); } }
}
void xEntropyHuffCounter::CountBlock(const int16* ScanCoeff, int32 LastDC, int32 HuffTableIdDC, int32 HuffTableIdAC)
{
  //DC coefficient
  int32 DC         = ScanCoeff[0];
  int32 DeltaDC    = DC - LastDC;
  int32 SignMaskDC = DeltaDC >> 31;                       // make a mask of the sign bit
  int32 AbsDeltaDC = (DeltaDC ^ SignMaskDC) - SignMaskDC; // toggle the bits and add one if value is negative
  int32 NumBitsDC  = xNumSignificantBits((uint32)AbsDeltaDC);
  m_HuffCounterDC[HuffTableIdDC].countDC(NumBitsDC);

  //AC coefficients
  xHuffCounterAC& HE = m_HuffCounterAC[HuffTableIdAC];
  int32 LastNonZero = xEntropyUtils::findLastNonZero(ScanCoeff);
  int32 RunLength   = 0;
  for(int32 i=1; i <= LastNonZero; i++)
  {
    int32 AC = ScanCoeff[i];

    //nothing to encode
    if(AC == 0) { RunLength++; continue; }

    //if run length > 15, must emit special run-length-16 codes (0xF0)
    while (RunLength > 15) { HE.countZRL(); RunLength -= 16; }

    //Emit Huffman symbol for run length / number of bits
    int32 SignMaskAC = AC >> 31;                       // make a mask of the sign bit
    int32 AbsAC      = (AC ^ SignMaskAC) - SignMaskAC; // toggle the bits and add one if value is negative
    int32 NumBitsAC  = xNumSignificantBits((uint32)AbsAC);
    int32 CodeAC     = (RunLength << 4) + NumBitsAC;
    HE.countAC(CodeAC);

    //reset run length
    RunLength = 0;
  }
  //If the last coef(s) were zero, emit an end-of-block code
  if (LastNonZero < 63) { HE.countEOB(); }
}
void xEntropyHuffCounter::AddCounters(const xEntropyHuffCounter& Other)
{
  for(int32 i = 0; i < xJPEG_Constants::c_MaxHuffTabs; i++) { if(m_ActiveDC[i]) { m_HuffCounterDC[i].acc(Other.m_HuffCounterDC[i]); } }
  for(int32 i = 0; i < xJPEG_Constants::c_MaxHuffTabs; i++) { if(m_ActiveAC[i]) { m_HuffCounterAC[i].acc(Other.m_HuffCounterAC[i]); } }
}

//=====================================================================================================================================================================================

} //end of namespace PMBB::JPEG