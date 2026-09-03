/*
    SPDX-FileCopyrightText: 2020-2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-FileCopyrightText: 2026 Kacper Winiecki <kacper.winiecki@student.put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#include "xJPEG_Encoder.h"
#include "xJPEG_Transform.h"0
#include "xJPEG_TransformConstants.h"
#include "xJPEG_Scan.h"
#include "xJPEG_EntropyHuffman.h"
#include "xJPEG_EntropyUtils.h"
#include "xJPEG_OptUtils.h"
#include "xMemory.h"
#include "xPixelOps.h"
#include "xDistortion.h"
#include "xColorSpace.h"
#include "xColorSpaceCoeff.h"

namespace PMBB_NAMESPACE::JPEG {

//=====================================================================================================================================================================================

void xAdvancedEncoder::create(int32V2 PictureSize, eCrF ChromaFormat, xThreadPool* ThreadPool)
{
  initCodecCommon(PictureSize, ChromaFormat);

  for(int32 CmpIdx = 0; CmpIdx < m_NumOfComponents; CmpIdx++)
  {
    const int32 Area = m_MCUsMulWidth[CmpIdx] * m_MCUsMulHeight[CmpIdx];
    m_CmpCoeffsTransOrg    [CmpIdx] = (int16*)xMemory::xAlignedMallocPageAuto(Area * sizeof(int16));
    m_CmpCoeffsTransRec    [CmpIdx] = (int16*)xMemory::xAlignedMallocPageAuto(Area * sizeof(int16)); //TODO only if ????
    m_CmpCoeffsQuantScan   [CmpIdx] = (int16*)xMemory::xAlignedMallocPageAuto(Area * sizeof(int16));
    m_CmpCoeffsQuantScanAux[CmpIdx] = (int16*)xMemory::xAlignedMallocPageAuto(Area * sizeof(int16)); //TODO only if RDOQ
    m_CmpCoeffsQuantScanOpt[CmpIdx] = (int16*)xMemory::xAlignedMallocPageAuto(Area * sizeof(int16)); //TODO only if RDOQ
  }

  m_PicRec.create(PictureSize, 8, ChromaFormat, 16);

  if(ThreadPool != nullptr)
  {
    m_ThPI.init(ThreadPool, m_PictureSize.getY(), m_PictureSize.getY());
  }
}
void xAdvancedEncoder::destroy()
{
  m_ThPI.uninit();

  for(int32 CmpIdx = 0; CmpIdx < m_NumOfComponents; CmpIdx++)
  {
    if(m_CmpCoeffsTransOrg    [CmpIdx] != nullptr) {xMemory::xAlignedFreeNull(m_CmpCoeffsTransOrg    [CmpIdx]); }
    if(m_CmpCoeffsTransRec    [CmpIdx] != nullptr) {xMemory::xAlignedFreeNull(m_CmpCoeffsTransRec    [CmpIdx]); }
    if(m_CmpCoeffsQuantScan   [CmpIdx] != nullptr) {xMemory::xAlignedFreeNull(m_CmpCoeffsQuantScan   [CmpIdx]); }
    if(m_CmpCoeffsQuantScanAux[CmpIdx] != nullptr) {xMemory::xAlignedFreeNull(m_CmpCoeffsQuantScanAux[CmpIdx]); }
    if(m_CmpCoeffsQuantScanOpt[CmpIdx] != nullptr) {xMemory::xAlignedFreeNull(m_CmpCoeffsQuantScanOpt[CmpIdx]); }
  }

  //m_EntropyBuffer.destroy();
  m_PicRec.destroy();
}
void xAdvancedEncoder::setMarkerEmit(bool EmitAPP0, bool EmitQuantTabs, bool EmitEntropyTabs)
{
  m_EmitAPP0        = EmitAPP0       ;
  m_EmitQuantTabs   = EmitQuantTabs  ;
  m_EmitEntropyTabs = EmitEntropyTabs;
}
void xAdvancedEncoder::setOptQuant(bool OptQuantCoeffs, bool ProcessZeroCoeffs, eLmbd LambdaEstMode)
{ 
  m_OptQuantCoeffs    = OptQuantCoeffs   ;
  m_ProcessZeroCoeffs = ProcessZeroCoeffs;
  m_LambdaEstMode     = LambdaEstMode    ;
}
void xAdvancedEncoder::setOptEntropy(bool OptEntropyTables)
{
  m_OptEntropyTables = OptEntropyTables;
}
void xAdvancedEncoder::setOptPass(int32 NumBlockOptPasses, int32 NumPicOptPasses)
{
  m_NumOptPassesBlock = NumBlockOptPasses;
  m_NumOptPassesPic   = NumPicOptPasses  ;
}
void xAdvancedEncoder::setOptClcMode(eCalkMd BlkOptDistCalcMode, eCalkMd BlkOptBitsCalcMode)
{
  m_BlkOptDistCalcMode = BlkOptDistCalcMode;
  m_BlkOptBitsCalcMode = BlkOptBitsCalcMode;
}
void xAdvancedEncoder::initBaseMarkers()
{
  int32 TypeSOF = 0;

  //init markers
  m_APP0.InitDefault();
  m_SOF .Init(TypeSOF, m_PictureHeight, m_PictureWidth, 8, m_ChromaFormat, 2);
  m_SOS.InitBaseline(m_SOF.getNumComponents(), 0, 1);
}
void xAdvancedEncoder::initQuant(int32 Quality, eQTLa QuantTabLayout)
{
  m_Quality        = Quality;
  m_QuantTabLayout = QuantTabLayout;

  //init markers
  m_QTs.resize(2);
  m_QTs[0].Init(0, eCmp::LM, Quality, QuantTabLayout);
  m_QTs[1].Init(1, eCmp::CB, Quality, QuantTabLayout); //any chroma so use CB

  if(m_VerboseLevel >= 6)
  {
    std::string Dump = fmt::format("QuantizationTablesMain Num={}\n", m_QTs.size());
    for(int32 i = 0; i < (int32)m_QTs.size(); i++) { Dump += fmt::format("  QuantTab_{:d}\n", i) + m_QTs[i].Format("    "); }
    fmt::print("{}", Dump);
  }

  //init toolbox
  m_QuantMain.Init(m_QTs);
  if(m_OptEntropyTables && m_LambdaEstMode == eLmbd::Exhaustive)
  {
    m_QuantAuxD.Init(0, eCmp::LM, Quality - 2, QuantTabLayout);
    m_QuantAuxD.Init(1, eCmp::CB, Quality - 2, QuantTabLayout); //any chroma so use CB
    m_QuantAuxI.Init(0, eCmp::LM, Quality + 2, QuantTabLayout);
    m_QuantAuxI.Init(1, eCmp::CB, Quality + 2, QuantTabLayout); //any chroma so use CB
  }

  if(m_VerboseLevel >= 6)
  {
    std::string Dump = fmt::format("QuantizerTablesMain\n");
    for(int32 i = 0; i < (int32)m_QTs.size(); i++)
    {
      Dump += fmt::format("  Table_{}\n", i) + m_QuantMain.getQuantizer(i).FormatCoeffs("    ");
    }
    fmt::print("{}", Dump);
  }
}
void xAdvancedEncoder::initEntropy(int32 RestartInterval)
{
  m_RestartInterval = RestartInterval;  

  m_HTs = xJFIF::xHuffTable::createDefaultHuffTables();

  m_NumOfSlices    = RestartInterval > 0 ? (int32)std::ceil((flt64)m_NumMCUsInArea / (flt64)RestartInterval) : 1;
  m_NumMCUsInSlice = RestartInterval != 0 ? RestartInterval : m_NumMCUsInArea;
  int32 NumBlocksInMCU       = std::accumulate(m_NumBlocksInMCU.cbegin(), m_NumBlocksInMCU.cend(), 0);
  int32 MaxEncodedSliceSize  = m_NumMCUsInSlice * NumBlocksInMCU * c_BA * 2;
  int32 MaxEncodedMCURowSize = m_NumMCUsInWidth * NumBlocksInMCU * c_BA * 2;
  
  bool  EncodeMCU_Rows = RestartInterval == 0 && m_ThPI.isActive();
  int32 NumEncoders    = EncodeMCU_Rows ? m_NumMCUsInHeight    : m_NumOfSlices      ;
  int32 EncBufferSize  = EncodeMCU_Rows ? MaxEncodedMCURowSize : MaxEncodedSliceSize;

  //buffers
  m_EncBuffers  .resize(NumEncoders);
  m_SliceBuffers.resize(NumEncoders);
  for(int32 i = 0; i < (int32)m_EncBuffers  .size(); i++) { m_EncBuffers  [i].resize(EncBufferSize); }
  for(int32 i = 0; i < (int32)m_SliceBuffers.size(); i++) { m_SliceBuffers[i].resize(EncBufferSize); }

  if(EncodeMCU_Rows)
  {
    m_NumAlignmentBits.resize(m_NumMCUsInHeight, 0);
    const int32 CombineBufferSize = m_NumMCUsInArea * NumBlocksInMCU * c_BA * 2;
    m_CombineBuffer.create(CombineBufferSize);
  }

  m_EntropyHuffEst .Init(m_HTs);
  m_EncoderHuffBank.Init(m_HTs);
  m_EntropyHuffEncs = std::vector<xEntropyHuffEncoder>(NumEncoders);
  for(int32 i = 0; i < (int32)m_EntropyHuffEncs.size(); i++) { m_EntropyHuffEncs[i].SetEncoders(m_EncoderHuffBank); }
  
  if(m_OptEntropyTables)
  {
    int32 NumHuffCounters = m_ThPI.isActive() ? m_ThPI.getNumThreads() : 1;
    m_EntropyHuffCnts.resize(NumHuffCounters);
  }
}
void xAdvancedEncoder::encode(const xPicYUV* InputPicture, const xPicP* InputPictureRGB,  xByteBuffer* OutputBuffer)
{
    bool useRGB = true;
    if (useRGB){ xEncodePictureWithRGB(OutputBuffer, InputPicture, InputPictureRGB); } //true
    else{ xEncodePicture(OutputBuffer, InputPicture); } // false
  
}
std::string xAdvancedEncoder::formatAndResetStats(const std::string Prefix, flt64 TicksPerMicroSec)
{
  if(!m_GatherTimeStats      ) { return "Time stats gathering is disabled!"; }
  if(m_TotalPictureIters == 0) { return "No time stats gathered!"; }

  flt64 InvDenom = TicksPerMicroSec == 0.0 ? (flt64)1.0 / ((flt64)m_TotalPictureIters) : (flt64)1.0 / ((flt64)m_TotalPictureIters * TicksPerMicroSec);

  std::string TimeStats; TimeStats.reserve(xMemory::getBestEffortSizePageBase());
  TimeStats += Prefix + fmt::format("Processing time {}\n", TicksPerMicroSec == 0.0 ? "[ticks]" : "[us]");

  TimeStats += Prefix + "  " + fmt::format("PicIters   = {}\n"    , m_TotalPictureIters);
  TimeStats += Prefix + "  " + fmt::format("Pic        = {:.2f}\n", m_Ticks___Picture * InvDenom);
  TimeStats += Prefix + "  " + fmt::format("Transform  = {:.2f}\n", m_Ticks_Transform * InvDenom);
  TimeStats += Prefix + "  " + fmt::format("QuantScan  = {:.2f}\n", m_Ticks_QuantScan * InvDenom);
  TimeStats += Prefix + "  " + fmt::format("LambdaEst  = {:.2f}\n", m_Ticks_LambdaEst * InvDenom);
  TimeStats += Prefix + "  " + fmt::format("OptQuant   = {:.2f}\n", m_Ticks__OptQuant * InvDenom);
  TimeStats += Prefix + "  " + fmt::format("OptEntropy = {:.2f}\n", m_TicksOptEntropy * InvDenom);
  TimeStats += Prefix + "  " + fmt::format("EntropyEnc = {:.2f}\n", m_TicksEntropyEnc * InvDenom);
  TimeStats += Prefix + "  " + fmt::format("Tables     = {:.2f}\n", m_Ticks____Tables * InvDenom);
  TimeStats += Prefix + "  " + fmt::format("WriteOut   = {:.2f}\n", m_Ticks__WriteOut * InvDenom);
  
  m_Ticks_Transform = 0;
  m_Ticks_QuantScan = 0;
  m_Ticks_LambdaEst = 0;
  m_Ticks__OptQuant = 0;
  m_TicksOptEntropy = 0;
  m_TicksEntropyEnc = 0;
  m_Ticks____Tables = 0;
  m_Ticks__WriteOut = 0;

  m_TotalPictureIters = 0;
  m_TotalSliceIters   = 0;

  return TimeStats;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void xAdvancedEncoder::xEncodePicture(xByteBuffer* OutputBuffer, const xPicYUV* Picture)
{
  const int16* ConstCmpCoeffsTransOrg    [] = { m_CmpCoeffsTransOrg    [0], m_CmpCoeffsTransOrg    [1], m_CmpCoeffsTransOrg    [2], m_CmpCoeffsTransOrg    [3]};
  const int16* ConstCmpCoeffsQuantScan   [] = { m_CmpCoeffsQuantScan   [0], m_CmpCoeffsQuantScan   [1], m_CmpCoeffsQuantScan   [2], m_CmpCoeffsQuantScan   [3]};
  const int16* ConstCmpCoeffsQuantScanOpt[] = { m_CmpCoeffsQuantScanOpt[0], m_CmpCoeffsQuantScanOpt[1], m_CmpCoeffsQuantScanOpt[2], m_CmpCoeffsQuantScanOpt[3]};

  if(m_OptEntropyTables)
  {
    m_HTs = xJFIF::xHuffTable::createDefaultHuffTables(); //make sure that new pics starts with m_HTs set to DefaultHuffTables
  }

  uint64 TP0 = m_GatherTimeStats ? xTSC() : 0;

  xFwdTransformPic(m_CmpCoeffsTransOrg, Picture);

  uint64 TP1 = m_GatherTimeStats ? xTSC() : 0;

  xFwdQuantScanPic(m_CmpCoeffsQuantScan, ConstCmpCoeffsTransOrg, m_QuantMain);

  uint64 TP2 = m_GatherTimeStats ? xTSC() : 0;

  if(m_OptQuantCoeffs) { xEstimateLambda(Picture); }

  uint64 TP3 = m_GatherTimeStats ? xTSC() : 0;

  for(int32 n = 0; n < m_NumOptPassesPic; n++)
  {
    uint64 TPo0 = m_GatherTimeStats ? xTSC() : 0;

    if(m_OptQuantCoeffs) { xOptQuantPic(m_CmpCoeffsQuantScanOpt, ConstCmpCoeffsQuantScan, ConstCmpCoeffsTransOrg, Picture); }

    uint64 TPo1 = m_GatherTimeStats ? xTSC() : 0;

    if(m_OptEntropyTables) { xOptEntropyPic(m_OptQuantCoeffs ? ConstCmpCoeffsQuantScanOpt : ConstCmpCoeffsQuantScan); }

    if(m_GatherTimeStats) { m_Ticks__OptQuant += TPo1 - TPo0; m_TicksOptEntropy += xTSC() - TPo1; }
  }

  uint64 TP4 = m_GatherTimeStats ? xTSC() : 0;

  xEncodePic(m_OptQuantCoeffs ? ConstCmpCoeffsQuantScanOpt : ConstCmpCoeffsQuantScan);

  uint64 TP5 = m_GatherTimeStats ? xTSC() : 0;

  xJFIF::WriteSOI(OutputBuffer);
  if(m_EmitAPP0       ) { xJFIF::WriteAPP0(OutputBuffer, m_APP0); }
  if(m_EmitQuantTabs  ) { xJFIF::WriteDQT (OutputBuffer, m_QTs); }
  if(m_RestartInterval) { xJFIF::WriteDRI (OutputBuffer, m_RestartInterval); }
  xJFIF::WriteSOF(OutputBuffer, m_SOF);
  if(m_EmitEntropyTabs) { xJFIF::WriteDHT(OutputBuffer, m_HTs); }

  uint64 TP6 = m_GatherTimeStats ? xTSC() : 0;

  xJFIF::WriteSOS(OutputBuffer, m_SOS);
  xWritePic(OutputBuffer);
  xJFIF::WriteEOI(OutputBuffer);

  uint64 TP7 = m_GatherTimeStats ? xTSC() : 0;
  
  m_TotalPictureIters += 1;
  if(m_GatherTimeStats)
  {
    m_Ticks___Picture += TP6 - TP0;
    m_Ticks_Transform += TP1 - TP0;
    m_Ticks_QuantScan += TP2 - TP1;
    m_Ticks_LambdaEst += TP3 - TP2;
    m_TicksEntropyEnc += TP5 - TP4;
    m_Ticks____Tables += TP6 - TP5;
    m_Ticks__WriteOut += TP7 - TP6;
  }
}



//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// RGB REFERENCES
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


void xAdvancedEncoder::xEncodePictureWithRGB(xByteBuffer* OutputBuffer, const xPicYUV* Picture, const xPicP* PictureRGB){

    //Picture
    //│
    //└── const xPicYUV*
    //      └── Y/Cb/Cr 4:4:4
    //
    // 
    //PictureRGB
    //│
    //└── const xPicP *
    //      └── R/G/B

    const int16* ConstCmpCoeffsTransOrg[] = { m_CmpCoeffsTransOrg[0], m_CmpCoeffsTransOrg[1], m_CmpCoeffsTransOrg[2], m_CmpCoeffsTransOrg[3] };
    const int16* ConstCmpCoeffsQuantScan[] = { m_CmpCoeffsQuantScan[0], m_CmpCoeffsQuantScan[1], m_CmpCoeffsQuantScan[2], m_CmpCoeffsQuantScan[3] };
    const int16* ConstCmpCoeffsQuantScanOpt[] = { m_CmpCoeffsQuantScanOpt[0], m_CmpCoeffsQuantScanOpt[1], m_CmpCoeffsQuantScanOpt[2], m_CmpCoeffsQuantScanOpt[3] };

    if (m_OptEntropyTables)
    {
        m_HTs = xJFIF::xHuffTable::createDefaultHuffTables(); //make sure that new pics starts with m_HTs set to DefaultHuffTables
    }

    uint64 TP0 = m_GatherTimeStats ? xTSC() : 0;

    xFwdTransformPic(m_CmpCoeffsTransOrg, Picture);

    uint64 TP1 = m_GatherTimeStats ? xTSC() : 0;

    xFwdQuantScanPic(m_CmpCoeffsQuantScan, ConstCmpCoeffsTransOrg, m_QuantMain);

    uint64 TP2 = m_GatherTimeStats ? xTSC() : 0;

    if (m_OptQuantCoeffs) { xEstimateLambdaRGB(Picture, PictureRGB); }

    uint64 TP3 = m_GatherTimeStats ? xTSC() : 0;

    for (int32 n = 0; n < m_NumOptPassesPic; n++)
    {
        uint64 TPo0 = m_GatherTimeStats ? xTSC() : 0;

        if (m_OptQuantCoeffs) { xOptQuantPicRGB(m_CmpCoeffsQuantScanOpt, ConstCmpCoeffsQuantScan, ConstCmpCoeffsTransOrg, PictureRGB); }

        uint64 TPo1 = m_GatherTimeStats ? xTSC() : 0;

        if (m_OptEntropyTables) { xOptEntropyPic(m_OptQuantCoeffs ? ConstCmpCoeffsQuantScanOpt : ConstCmpCoeffsQuantScan); }

        if (m_GatherTimeStats) { m_Ticks__OptQuant += TPo1 - TPo0; m_TicksOptEntropy += xTSC() - TPo1; }
    }

    uint64 TP4 = m_GatherTimeStats ? xTSC() : 0;

    xEncodePic(m_OptQuantCoeffs ? ConstCmpCoeffsQuantScanOpt : ConstCmpCoeffsQuantScan);

    uint64 TP5 = m_GatherTimeStats ? xTSC() : 0;

    xJFIF::WriteSOI(OutputBuffer);
    if (m_EmitAPP0) { xJFIF::WriteAPP0(OutputBuffer, m_APP0); }
    if (m_EmitQuantTabs) { xJFIF::WriteDQT(OutputBuffer, m_QTs); }
    if (m_RestartInterval) { xJFIF::WriteDRI(OutputBuffer, m_RestartInterval); }
    xJFIF::WriteSOF(OutputBuffer, m_SOF);
    if (m_EmitEntropyTabs) { xJFIF::WriteDHT(OutputBuffer, m_HTs); }

    uint64 TP6 = m_GatherTimeStats ? xTSC() : 0;

    xJFIF::WriteSOS(OutputBuffer, m_SOS);
    xWritePic(OutputBuffer);
    xJFIF::WriteEOI(OutputBuffer);

    uint64 TP7 = m_GatherTimeStats ? xTSC() : 0;

    m_TotalPictureIters += 1;
    if (m_GatherTimeStats)
    {
        m_Ticks___Picture += TP6 - TP0;
        m_Ticks_Transform += TP1 - TP0;
        m_Ticks_QuantScan += TP2 - TP1;
        m_Ticks_LambdaEst += TP3 - TP2;
        m_TicksEntropyEnc += TP5 - TP4;
        m_Ticks____Tables += TP6 - TP5;
        m_Ticks__WriteOut += TP7 - TP6;
    }
}



// -----------------------------------------------------------------------------------------------------------------

void xAdvancedEncoder::xOptQuantHuffPicRGB(int16* OptCoeffsQuantScanV[], const int16* CoeffsQuantScanV[], const int16* CoeffsTransOrgV[], const xPicP* PictureRGB){
    if (m_RestartInterval == 0)
    {
        for (int32 MCU_RowIdx = 0; MCU_RowIdx < m_NumMCUsInHeight; MCU_RowIdx++) //loop over MCUs rows
        {
            const int32 MCU_IdxFirst = MCU_RowIdx * m_NumMCUsInWidth;
            const int32 MCU_IdxLast = MCU_IdxFirst + m_NumMCUsInWidth - 1;
            m_ThPI.storeTask([this, &OptCoeffsQuantScanV, &CoeffsQuantScanV, &CoeffsTransOrgV, &PictureRGB, MCU_IdxFirst, MCU_IdxLast](int32 /*ThIdx*/)
                {
                    xOptQuantHuffSlcRGB(OptCoeffsQuantScanV, CoeffsQuantScanV, CoeffsTransOrgV, PictureRGB, MCU_IdxFirst, MCU_IdxLast);
                });
        }
    }
    else //divide picture into independent slices
    {
        for (int32 SliceIdx = 0; SliceIdx < m_NumOfSlices; SliceIdx++)
        {
            const int32 MCU_IdxFirst = SliceIdx * m_RestartInterval;
            const int32 MCU_IdxLast = xMin(m_NumMCUsInArea, MCU_IdxFirst + m_RestartInterval) - 1;
            m_ThPI.storeTask([this, &OptCoeffsQuantScanV, &CoeffsQuantScanV, &CoeffsTransOrgV, &PictureRGB, MCU_IdxFirst, MCU_IdxLast](int32 /*ThIdx*/)
                {
                    xOptQuantHuffSlcRGB(OptCoeffsQuantScanV, CoeffsQuantScanV, CoeffsTransOrgV, PictureRGB, MCU_IdxFirst, MCU_IdxLast);
                });
        }
    }
    m_ThPI.executeStoredTasks();
}


void xAdvancedEncoder::xOptQuantHuffSlcRGB(int16* OptCoeffsQuantScanV[], const int16* CoeffsQuantScanV[], const int16* CoeffsTransOrgV[], const xPicP* PictureRGB, int32 MCU_IdxFirst, int32 MCU_IdxLast){
    // RGB
    const uint16* RGBPtrV[] = { PictureRGB->getAddr(eCmp::LM), PictureRGB->getAddr(eCmp::CB), PictureRGB->getAddr(eCmp::CR), nullptr };
    const int32   StrideRGB = PictureRGB->getStride(); // last one


    //loop over MCUs
    for (int32 MCU_Idx = MCU_IdxFirst; MCU_Idx <= MCU_IdxLast; MCU_Idx++)
    {
        xOptQuantHuffMCURGB(OptCoeffsQuantScanV, CoeffsQuantScanV, CoeffsTransOrgV, RGBPtrV, StrideRGB, MCU_Idx);
    }
}


void xAdvancedEncoder::xOptQuantHuffMCURGB(int16* OptCoeffsQuantScanV[], const int16* CoeffsQuantScanV[], const int16* CoeffsTransOrgV[] /*apprx*/, const uint16* RGBPtrV[], const int32 StrideRGB, int32 MCU_Idx)
{
    //calculate MCU position
    const int32 MCU_PosV = MCU_Idx / m_NumMCUsInWidth; // index / amount_in_row -> pos in MCU_rows
    const int32 MCU_PosH = MCU_Idx % m_NumMCUsInWidth; // index % amount_in_row -> pos in MCU_columns
      
    //     H =  0   1   2   3

    //V = 0     0   1   2   3
    //V = 1     4   5   6   7
    //V = 2     8   9  10  11


    const int32 MCU_Width = c_BS * m_SampFactorHor[0];
    const int32 MCU_Height = c_BS * m_SampFactorVer[0];

    // real position in picture:
    const int32 MCU_InPic_PosV = MCU_PosV * MCU_Height; // starting_sample_y
    const int32 MCU_InPic_PosH = MCU_PosH * MCU_Width; // starting_sample_x
    const int32 MCU_InPic_ResV = m_CmpHeight[0] - MCU_InPic_PosV; // from starting_to_edge y
    const int32 MCU_InPic_ResH = m_CmpWidth[0] - MCU_InPic_PosH; // from starting_to_edge x


    //org samples buffer
    PMBB_ALIGN_JPEG_BLK uint16 SamplesOrgRGB[3][4*c_BA];


    for (int32 RGBIdx = 0; RGBIdx < m_NumOfComponents; RGBIdx++){
        const uint16* RGBPtr = RGBPtrV[RGBIdx];
        // (x0, y0) = (0, 0) + starting_sample_y * margin + starting_sample_x
        const uint16* MCUPtr = RGBPtr + MCU_InPic_PosV * StrideRGB + MCU_InPic_PosH;


        if (MCU_InPic_ResV >= MCU_Height && MCU_InPic_ResH >= MCU_Width) {loadEntireArea(SamplesOrgRGB[RGBIdx], MCUPtr, StrideRGB, MCU_Width, MCU_Height);}
        else if (MCU_InPic_ResV > 0 && MCU_InPic_ResH > 0) {loadExtendArea(SamplesOrgRGB[RGBIdx], MCUPtr, StrideRGB, MCU_Width, MCU_Height, xMin(MCU_InPic_ResH, MCU_Width), xMin(MCU_InPic_ResV, MCU_Height));}
        else { zeroEntireArea(SamplesOrgRGB[RGBIdx], MCU_Width, MCU_Height); }
    }

    // --------------------- Planes Params ---------------------------------------------------------------------------
    const xQuantizer& Quantizer_Y = m_QuantMain.getQuantizer(m_SOF.getQuantTableId(eCmp::LM));
    const xQuantizer& Quantizer_Cb = m_QuantMain.getQuantizer(m_SOF.getQuantTableId(eCmp::CB));
    const xQuantizer& Quantizer_Cr = m_QuantMain.getQuantizer(m_SOF.getQuantTableId(eCmp::CR));

    const int32 EntropyIdDC_Y = m_SOS.getEntropyIdDC(eCmp::LM);
    const int32 EntropyIdDC_Cb = m_SOS.getEntropyIdDC(eCmp::CB);
    const int32 EntropyIdDC_Cr = m_SOS.getEntropyIdDC(eCmp::CR);

    const int32 EntropyIdAC_Y = m_SOS.getEntropyIdAC(eCmp::LM);
    const int32 EntropyIdAC_Cb = m_SOS.getEntropyIdAC(eCmp::CB);
    const int32 EntropyIdAC_Cr = m_SOS.getEntropyIdAC(eCmp::CR);

    // Y - prep
    const int32 NumYBlocks = m_SampFactorHor[(int32)eCmp::LM] * m_SampFactorVer[(int32)eCmp::LM];
    const int32 CoeffOffset = MCU_Idx << c_L2BA;

    const int16* CoeffsQuantScanBlockV[3] =
    {
        CoeffsQuantScanV[(int32)eCmp::LM] + NumYBlocks * CoeffOffset,
        CoeffsQuantScanV[(int32)eCmp::CB] + CoeffOffset,
        CoeffsQuantScanV[(int32)eCmp::CR] + CoeffOffset
    };

    int16* OptCoeffsQuantScanBlockV[3] =
    {
        OptCoeffsQuantScanV[(int32)eCmp::LM] + NumYBlocks * CoeffOffset,
        OptCoeffsQuantScanV[(int32)eCmp::CB] + CoeffOffset,
        OptCoeffsQuantScanV[(int32)eCmp::CR] + CoeffOffset
    };

    const bool  FirstInSlc = MCU_Idx == 0 || (m_RestartInterval > 0 && MCU_Idx % m_RestartInterval == 0);
    const int32 LastDC_Cb = FirstInSlc ? 0 : CoeffsQuantScanV[1][CoeffOffset - c_BA];
    const int32 LastDC_Cr = FirstInSlc ? 0 : CoeffsQuantScanV[2][CoeffOffset - c_BA];

    const int32 NumberBlockInMCU = 0;

    const xCmpCandtParams Params_Cb{
        eCmp::CB,
        CoeffsQuantScanBlockV[(int32)eCmp::CB],
        NumberBlockInMCU,
        &Quantizer_Cb,
        EntropyIdDC_Cb,
        EntropyIdAC_Cb,
        m_LambdaRGB,
        LastDC_Cb
    };

    const xCmpCandtParams Params_Cr{
        eCmp::CR,
        CoeffsQuantScanBlockV[(int32)eCmp::CR],
        NumberBlockInMCU,
        &Quantizer_Cr,
        EntropyIdDC_Cr,
        EntropyIdAC_Cr,
        m_LambdaRGB,
        LastDC_Cr
    };

    //ready to -> RGB
    PMBB_ALIGN_JPEG_BLK uint16  TmpSamples_Y[4 * c_BA];
    PMBB_ALIGN_JPEG_BLK uint16  TmpSamples_Cb[c_BA];
    PMBB_ALIGN_JPEG_BLK uint16  TmpSamples_Cr[c_BA];
    // ------------------------------------------------------------------------------------------------

    // # 1. Y optimization
    xInvProcess(TmpSamples_Cb, CoeffsQuantScanBlockV[(int32)eCmp::CB], Quantizer_Cb);
    xInvProcess(TmpSamples_Cr, CoeffsQuantScanBlockV[(int32)eCmp::CR], Quantizer_Cr);

    std::pair<const uint16*, eCmp> RecSamplesBlockV[2] = {
        { TmpSamples_Cb, eCmp::CB },
        { TmpSamples_Cr, eCmp::CR }
    };


    memcpy(OptCoeffsQuantScanBlockV[(int32)eCmp::LM], CoeffsQuantScanBlockV[(int32)eCmp::LM], NumYBlocks * c_BA * sizeof(int16));

    int32 BlockIdx = MCU_Idx * m_SampFactorVer[(int32)eCmp::LM] * m_SampFactorHor[(int32)eCmp::LM];
    for (int32 V = 0; V < m_SampFactorVer[(int32)eCmp::LM]; V++)
    {
        for (int32 H = 0; H < m_SampFactorHor[(int32)eCmp::LM]; H++)
        {
            const int32 CoeffOffset = BlockIdx << c_L2BA;
            const bool  FirstInSlc = BlockIdx == 0 || (m_RestartInterval > 0 && MCU_Idx % m_RestartInterval == 0);
            const int32 LastDC_Y = FirstInSlc ? 0 : CoeffsQuantScanV[0][CoeffOffset - c_BA];

            const int32 YBlockInMCU = V * m_SampFactorHor[(int32)eCmp::LM] + H;

            
            int16* OptCoeffsQuantScanY = OptCoeffsQuantScanBlockV[(int32)eCmp::LM] + YBlockInMCU * c_BA;

            xCmpCandtParams Params_Y{
                eCmp::LM,
                OptCoeffsQuantScanBlockV[(int32)eCmp::LM],
                YBlockInMCU,
                &Quantizer_Y,
                EntropyIdDC_Y,
                EntropyIdAC_Y,
                m_LambdaRGB,
                LastDC_Y
             };

            xOptQuantHuffTestCandtsBlockRGB(OptCoeffsQuantScanY, RecSamplesBlockV, SamplesOrgRGB, Params_Y);

            uint16* TmpSamplesYBlock = TmpSamples_Y + YBlockInMCU * c_BA;
            xInvProcess(TmpSamplesYBlock, OptCoeffsQuantScanY, Quantizer_Y);

            BlockIdx++;
        }
    }


    // # 2. Cb optimization
    // xInvProcess(TmpSamples_Cr, CoeffsQuantScanBlockV[2], Quantizer_Cr);  - already have that

    RecSamplesBlockV[0] = { TmpSamples_Y, eCmp::LM }; 
    RecSamplesBlockV[1] = { TmpSamples_Cr, eCmp::CR };

    xOptQuantHuffTestCandtsBlockRGB(OptCoeffsQuantScanBlockV[(int32)eCmp::CB], RecSamplesBlockV, SamplesOrgRGB, Params_Cb);


    // # 3. Cr optimization
    // xInvProcess(TmpSamples_Y, OptCoeffsQuantScanBlockV[0], Quantizer_Y); - already have that
    xInvProcess(TmpSamples_Cb, OptCoeffsQuantScanBlockV[(int32)eCmp::CB], Quantizer_Cb);

    RecSamplesBlockV[0] = { TmpSamples_Y, eCmp::LM };
    RecSamplesBlockV[1] = { TmpSamples_Cb, eCmp::CB };


    xOptQuantHuffTestCandtsBlockRGB(OptCoeffsQuantScanBlockV[(int32)eCmp::CR], RecSamplesBlockV, SamplesOrgRGB, Params_Cr);
}



void xAdvancedEncoder::xInvProcess(uint16* TmpSamples, const int16* CoeffsQuantScanBlockV, const xQuantizer& Quantizer)
{
    PMBB_ALIGN_JPEG_BLK int16  TmpQuantCoeffs[c_BA];
    PMBB_ALIGN_JPEG_BLK int16  TmpTransCoeffs[c_BA];

    xScan::InvScan(TmpQuantCoeffs, CoeffsQuantScanBlockV);
    Quantizer.InvScale(TmpTransCoeffs, TmpQuantCoeffs);
    TmpTransCoeffs[0] += xTransformConstants::c_InvDcCorr; //DC correction - JPEG requires 128 to be subtracted from every input sample - could be done be DC -= 
    xTransform::InvTransformDCT_8x8(TmpSamples, TmpTransCoeffs);
}




void xAdvancedEncoder::xOptQuantHuffTestCandtsBlockRGB(int16* OptCoeffQuantScan, const std::pair<const uint16*, eCmp> RecSamplesBlockV[2], const uint16 SamplesOrgRGB[3][4*c_BA], const xCmpCandtParams& Params) {
    const int16* CoeffsQuantScanBlock = Params.CoeffsQuantScanBlockV + Params.NumberBlockInMCU * c_BA;

    int32 LastNonZero = xEntropyUtils::findLastNonZero(CoeffsQuantScanBlock);
    if (LastNonZero == 0) { memcpy(OptCoeffQuantScan, CoeffsQuantScanBlock, c_BA * sizeof(int16)); return; } //only DC - nothing to do here

    const int32 BitsCandtCmp_DC = m_EntropyHuffEst.EstimateBlockDC(CoeffsQuantScanBlock, Params.LastDC, Params.EntropyIdDC);
    const int32 BitsCandtCmp_AC = m_EntropyHuffEst.EstimateBlockAC(CoeffsQuantScanBlock, Params.EntropyIdAC);
    int32 BestBits = BitsCandtCmp_DC + BitsCandtCmp_AC;

    PMBB_ALIGN_JPEG_BLK uint16 TmpSamples[4 * c_BA];

    if (Params.Cmp == eCmp::LM)
    {
        const int32 NumYBlocks = m_SampFactorHor[(int32)eCmp::LM] * m_SampFactorVer[(int32)eCmp::LM];

        for (int32 YBlockIdx = 0; YBlockIdx < NumYBlocks; YBlockIdx++)
        {
            const int16* CoeffsYBlock = Params.CoeffsQuantScanBlockV + YBlockIdx * c_BA;

            uint16* TmpSamplesYBlock = TmpSamples + YBlockIdx * c_BA;

            xInvProcess(TmpSamplesYBlock, CoeffsYBlock, *Params.Quantizer);
        }
    }
    else
    {
        xInvProcess(TmpSamples,CoeffsQuantScanBlock,*Params.Quantizer);
    }

    uint16* const TmpSamplesTestedBlock = TmpSamples + Params.NumberBlockInMCU * c_BA;
    

    std::pair<const uint16*, eCmp> SamplesRecYCbCr[3] = {
        RecSamplesBlockV[0],
        RecSamplesBlockV[1],
        { TmpSamples, Params.Cmp }
    };

    uint64 BestDist = xCalcDistRGB(SamplesRecYCbCr, SamplesOrgRGB);
    flt64  BestCost = (flt64)BestDist + Params.Lambda * (flt64)BestBits;


    PMBB_ALIGN_JPEG_BLK int16 TmpCoeffsQuantScan[c_BA];
    memcpy(TmpCoeffsQuantScan, CoeffsQuantScanBlock, c_BA * sizeof(int16));

    for (int32 PassIdx = 0; PassIdx < m_NumOptPassesBlock; PassIdx++)
    {
        for (int32 i = LastNonZero; i >= 1; i--)
        {
            const int16 OrgCoeff = TmpCoeffsQuantScan[i];
            if (!m_ProcessZeroCoeffs && OrgCoeff == 0) { continue; }

            int16 BestCoeff = TmpCoeffsQuantScan[i];
            //try zero
            if (OrgCoeff != 0)
            {
                TmpCoeffsQuantScan[i] = 0;
                int32  CurrBits = BitsCandtCmp_DC + m_EntropyHuffEst.EstimateBlockAC(TmpCoeffsQuantScan, Params.EntropyIdAC);

                xInvProcess(TmpSamplesTestedBlock, TmpCoeffsQuantScan, *Params.Quantizer);

                uint64 CurrDist = xCalcDistRGB(SamplesRecYCbCr, SamplesOrgRGB);
                flt64  CurrCost = (double)CurrDist + Params.Lambda * (flt64)CurrBits;

                if (CurrCost < BestCost)
                {
                    BestBits = CurrBits;
                    BestCost = CurrCost;
                    BestCoeff = 0;
                }
            }

            //try +1
            if (OrgCoeff != -1)
            {
                TmpCoeffsQuantScan[i] = OrgCoeff + 1;
                int32  CurrBits = BitsCandtCmp_DC + m_EntropyHuffEst.EstimateBlockAC(TmpCoeffsQuantScan, Params.EntropyIdAC);

                xInvProcess(TmpSamplesTestedBlock, TmpCoeffsQuantScan, *Params.Quantizer);

                uint64 CurrDist = xCalcDistRGB(SamplesRecYCbCr, SamplesOrgRGB);
                flt64  CurrCost = (double)CurrDist + Params.Lambda * (flt64)CurrBits;

                if (CurrCost < BestCost)
                {
                    BestBits = CurrBits;
                    BestCost = CurrCost;
                    BestCoeff = OrgCoeff + 1;
                }
            }

            //try -1  
            if (OrgCoeff != 1)
            {
                TmpCoeffsQuantScan[i] = OrgCoeff - 1;
                int32  CurrBits = BitsCandtCmp_DC + m_EntropyHuffEst.EstimateBlockAC(TmpCoeffsQuantScan, Params.EntropyIdAC);

                xInvProcess(TmpSamplesTestedBlock, TmpCoeffsQuantScan, *Params.Quantizer);

                uint64 CurrDist = xCalcDistRGB(SamplesRecYCbCr, SamplesOrgRGB);
                flt64  CurrCost = (double)CurrDist + Params.Lambda * (flt64)CurrBits;

                if (CurrCost < BestCost)
                {
                    BestBits = CurrBits;
                    BestCost = CurrCost;
                    BestCoeff = OrgCoeff - 1;
                }
            }

            TmpCoeffsQuantScan[i] = BestCoeff;
        }
    }
    memcpy(OptCoeffQuantScan, TmpCoeffsQuantScan, c_BA * sizeof(int16));
}


uint64 xAdvancedEncoder::xCalcDistRGB(const std::pair<const uint16*, eCmp> SamplesRecYCbCr[3], const uint16 SamplesOrgRGB[3][4*c_BA]) {
    const uint16* RecSamples[3] = {};

    for (int32 i = 0; i < 3; i++)
    {
        RecSamples[(int32)SamplesRecYCbCr[i].second] = SamplesRecYCbCr[i].first;
    }

    const uint16* Y = RecSamples[(int32)eCmp::LM];
    const uint16* Cb = RecSamples[(int32)eCmp::CB];
    const uint16* Cr = RecSamples[(int32)eCmp::CR];


    const int32 SampHorY = m_SampFactorHor[(int32)eCmp::LM];
    const int32 SampVerY = m_SampFactorVer[(int32)eCmp::LM];

    const int32 MCU_Width = c_BS * SampHorY;
    const int32 MCU_Height = c_BS * SampVerY;


    PMBB_ALIGN_JPEG_BLK uint16 Y_MCU[4 * c_BA];

    for (int32 V = 0; V < SampVerY; V++)
    {
        for (int32 H = 0; H < SampHorY; H++)
        {
            const int32 YBlockIdx = V * SampHorY + H;
            const uint16* SrcYBlock = Y + YBlockIdx * c_BA;

            uint16* DstYBlock = Y_MCU + V * c_BS * MCU_Width + H * c_BS;

            storeEntireArea(DstYBlock, SrcYBlock, c_BS, c_BS, MCU_Width);
        }
    }


    PMBB_ALIGN_JPEG_BLK uint16 Cb_MCU[4 * c_BA]; // result
    PMBB_ALIGN_JPEG_BLK uint16 Cr_MCU[4 * c_BA]; // result

    // chromas + margins -----------------------------------------

    // ----------------------------------------------------------

    if (SampHorY == 1 && SampVerY == 1)
    {
        memcpy(Cb_MCU, Cb, c_BA * sizeof(uint16));
        memcpy(Cr_MCU, Cr, c_BA * sizeof(uint16));
    }
    else if (SampHorY == 2 && SampVerY == 1)
    {
        xPixelOpsSTD::UpsampleH_FIR( Cb_MCU, Cb, MCU_Width, c_BS, MCU_Width, MCU_Height);
        xPixelOpsSTD::UpsampleH_FIR( Cr_MCU, Cr, MCU_Width, c_BS, MCU_Width, MCU_Height);
    }
    else if (SampHorY == 2 && SampVerY == 2)
    {
        xPixelOpsSTD::UpsampleH_FIR( Cb_MCU, Cb, MCU_Width, c_BS, MCU_Width, MCU_Height);

        xPixelOpsSTD::UpsampleH_FIR( Cr_MCU, Cr, MCU_Width, c_BS, MCU_Width, MCU_Height);
    }

    PMBB_ALIGN_JPEG_BLK uint16 RecR[4*c_BA];
    PMBB_ALIGN_JPEG_BLK uint16 RecG[4*c_BA];
    PMBB_ALIGN_JPEG_BLK uint16 RecB[4*c_BA];

    xColorSpace::ConvertYCbCr2RGB(RecR, RecG, RecB, Y_MCU, Cb_MCU, Cr_MCU, MCU_Width, MCU_Width, MCU_Width, MCU_Height, 8, eClrSpcLC::JPEG);

    uint64 DistRGB = 0;

    const uint16* SamplesRecRGB[3] = { RecR, RecG, RecB };

    for (int32 CmpIdx = 0; CmpIdx < 3; CmpIdx++)
    {
        DistRGB += RGB_weights[CmpIdx] * xDistortion::CalcSSD(
            SamplesOrgRGB[CmpIdx],
            SamplesRecRGB[CmpIdx],
            MCU_Width,
            MCU_Width,
            MCU_Width,
            MCU_Height,
            8
        );
    }
    return DistRGB;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// RGB REFERENCES - TEST
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------




//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Fwd/Inv transform + Quant/InvScale
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void xAdvancedEncoder::xFwdTransformPic(int16* CoeffsTransV[], const xPicYUV* Picture)
{
  const uint16* CmpPtrV   [] = {Picture->getAddr  (eCmp::LM), Picture->getAddr  (eCmp::CB), Picture->getAddr  (eCmp::CR), nullptr};
  const int32   CmpStrideV[] = {Picture->getStride(eCmp::LM), Picture->getStride(eCmp::CB), Picture->getStride(eCmp::CR),       0};

  for(int32 MCU_RowIdx = 0; MCU_RowIdx < m_NumMCUsInHeight; MCU_RowIdx++) //loop over MCUs rows
  {
    const int32 MCU_IdxBeg = MCU_RowIdx * m_NumMCUsInWidth;
    const int32 MCU_IdxEnd = MCU_IdxBeg + m_NumMCUsInWidth;
    m_ThPI.storeTask([this, &CoeffsTransV, &CmpPtrV, &CmpStrideV, MCU_IdxBeg, MCU_IdxEnd](int32 /*ThIdx*/) { xFwdTransformSlc(CoeffsTransV, CmpPtrV, CmpStrideV, MCU_IdxBeg, MCU_IdxEnd); });
  }
  m_ThPI.executeStoredTasks();
}
void xAdvancedEncoder::xFwdTransformSlc(int16* CoeffsTransV[], const uint16* CmpPtrV[], const int32 CmpStrideV[], int32 MCU_IdxBeg, int32 MCU_IdxEnd)
{
  //fmt::print("MCU_IdxBeg={}\n", MCU_IdxBeg);
  for(int32 MCU_Idx = MCU_IdxBeg; MCU_Idx < MCU_IdxEnd; MCU_Idx++) //loop over MCUs
  {
    xFwdTransformMCU(CoeffsTransV, CmpPtrV, CmpStrideV, MCU_Idx);
  }
}
void xAdvancedEncoder::xFwdTransformMCU(int16* CoeffsTransV[], const uint16* CmpPtrV[], const int32 CmpStrideV[], int32 MCU_Idx)
{
  //calculate MCU position
  const int32 MCU_PosV = MCU_Idx / m_NumMCUsInWidth;
  const int32 MCU_PosH = MCU_Idx % m_NumMCUsInWidth;

  //org samples buffer
  PMBB_ALIGN_JPEG_BLK uint16 SamplesOrg[c_BA];

  //transform blocks
  for(int32 CmpIdx = 0; CmpIdx < m_NumOfComponents; CmpIdx++)
  {
    const int32 MCU_PelPosV = MCU_PosV << (2 + m_SampFactorVer[CmpIdx]);
    const int32 MCU_PelPosH = MCU_PosH << (2 + m_SampFactorHor[CmpIdx]);

    const uint16* CmpPtr    = CmpPtrV   [CmpIdx];
    const int32   CmpStride = CmpStrideV[CmpIdx];

    int32 BlockIdx = MCU_Idx * m_SampFactorVer[CmpIdx] * m_SampFactorHor[CmpIdx];
    for(int32 V = 0; V < m_SampFactorVer[CmpIdx]; V++)
    {
      const int32 BlockPosV = MCU_PelPosV + V * c_BS;
      const int32 BlockResV = m_CmpHeight[CmpIdx] - BlockPosV;
      for(int32 H = 0; H < m_SampFactorHor[CmpIdx]; H++)
      {        
        const int32   BlockPosH = MCU_PelPosH + H * c_BS;
        const int32   BlockResH = m_CmpWidth[CmpIdx] - BlockPosH;
        const uint16* BlockPtr  = CmpPtr + BlockPosV * CmpStride + BlockPosH;     
        if     (BlockResV >= 8 && BlockResH >= 8) { loadEntireBlock(SamplesOrg, BlockPtr, CmpStride); } //C++20 TODO use [[likely]]
        else if(BlockResV >  0 && BlockResH >  0) { loadExtendBlock(SamplesOrg, BlockPtr, CmpStride, xMin(BlockResH, c_BS), xMin(BlockResV, c_BS)); }
        else                                      { zeroEntireBlock(SamplesOrg); }

        const int32 CoeffTransOffset = BlockIdx << c_L2BA;
        xTransform::FwdTransformDCT_8x8(CoeffsTransV[CmpIdx] + CoeffTransOffset, SamplesOrg);
        CoeffsTransV[CmpIdx][CoeffTransOffset] -= xTransformConstants::c_FwdDcCorr; //DC correction - JPEG requires 128 to be subtracted from every input sample - could be done be DC -= 
        BlockIdx++;
      }
    }
  }
}
void xAdvancedEncoder::xInvTransformPic(xPicYUV* Picture, const int16* CoeffsTransV[])
{
        uint16* CmpPtrV   [] = {Picture->getAddr  (eCmp::LM), Picture->getAddr  (eCmp::CB), Picture->getAddr  (eCmp::CR), nullptr};
  const int32   CmpStrideV[] = {Picture->getStride(eCmp::LM), Picture->getStride(eCmp::CB), Picture->getStride(eCmp::CR),       0};

  for(int32 MCU_RowIdx = 0; MCU_RowIdx < m_NumMCUsInHeight; MCU_RowIdx++) //loop over MCUs rows
  {
    const int32 MCU_IdxBeg = MCU_RowIdx * m_NumMCUsInWidth;
    const int32 MCU_IdxEnd = MCU_IdxBeg + m_NumMCUsInWidth;
    m_ThPI.storeTask([this, &CmpPtrV, &CmpStrideV, &CoeffsTransV, MCU_IdxBeg, MCU_IdxEnd](int32 /*ThIdx*/) { xInvTransformSlc(CmpPtrV, CmpStrideV, CoeffsTransV, MCU_IdxBeg, MCU_IdxEnd); });
  }
  m_ThPI.executeStoredTasks();
}
void xAdvancedEncoder::xInvTransformSlc(uint16* CmpPtrV[], const int32 CmpStrideV[], const int16* CoeffsTransV[], int32 MCU_IdxBeg, int32 MCU_IdxEnd)
{
  for(int32 MCU_Idx = MCU_IdxBeg; MCU_Idx < MCU_IdxEnd; MCU_Idx++) //loop over MCUs
  {
    xInvTransformMCU(CmpPtrV, CmpStrideV, CoeffsTransV, MCU_Idx);
  }
}
void xAdvancedEncoder::xInvTransformMCU(uint16* CmpPtrV[], const int32 CmpStrideV[], const int16* CoeffsTransV[], int32 MCU_Idx)
{
  //calculate MCU position
  const int32 MCU_PosV = MCU_Idx / m_NumMCUsInWidth;
  const int32 MCU_PosH = MCU_Idx % m_NumMCUsInWidth;

  //org samples buffer
  PMBB_ALIGN_JPEG_BLK int16  CoeffsTrans[c_BA];
  PMBB_ALIGN_JPEG_BLK uint16 SamplesRec [c_BA];

  //transform blocks
  for(int32 CmpIdx = 0; CmpIdx < m_NumOfComponents; CmpIdx++)
  {
    const int32 MCU_PelPosV = MCU_PosV << (2 + m_SampFactorVer[CmpIdx]);
    const int32 MCU_PelPosH = MCU_PosH << (2 + m_SampFactorHor[CmpIdx]);

          uint16* CmpPtr    = CmpPtrV   [CmpIdx];
    const int32   CmpStride = CmpStrideV[CmpIdx];

    int32 BlockIdx = MCU_Idx * m_SampFactorVer[CmpIdx] * m_SampFactorHor[CmpIdx];
    for(int32 V = 0; V < m_SampFactorVer[CmpIdx]; V++)
    {
      const int32 BlockPosV = MCU_PelPosV + V * c_BS;
      const int32 BlockResV = m_CmpHeight[CmpIdx] - BlockPosV;
      for(int32 H = 0; H < m_SampFactorHor[CmpIdx]; H++)
      {
        const int32 BlockPosH = MCU_PelPosH + H * c_BS;
        const int32 BlockResH = m_CmpWidth[CmpIdx] - BlockPosH;
        uint16* restrict BlockPtr = CmpPtr + BlockPosV * CmpStride + BlockPosH;
        const int32 CoeffTransOffset = BlockIdx << c_L2BA;

        memcpy(CoeffsTrans, CoeffsTransV[CmpIdx] + CoeffTransOffset, c_BA * sizeof(int16));
        CoeffsTrans[0] += xTransformConstants::c_InvDcCorr; //DC correction - JPEG requires 128 to be subtracted from every input sample - could be done be DC -= 
        xTransform::InvTransformDCT_8x8(SamplesRec, CoeffsTrans);

        if     (BlockResV >= 8 && BlockResH >= 8) { storeEntireBlock (BlockPtr, SamplesRec, CmpStride); } //C++20 TODO use [[likely]]
        else if(BlockResV >  0 && BlockResH >  0) { storePartialBlock(BlockPtr, SamplesRec, CmpStride, xMin(BlockResH, c_BS), xMin(BlockResV, c_BS)); }
        else                                      { /* do nothing */ }

        BlockIdx++;
      }
    }
  }
}
void xAdvancedEncoder::xFwdQuantScanPic(int16* CoeffsQuantScanV[], const int16* CoeffsTransV[], const xQuantizerSet& Quant)
{
  for(int32 CmpIdx = 0; CmpIdx < m_NumOfComponents; CmpIdx++)
  {
    const int32       QuantTabIdx       = m_SOF.getQuantTableId(eCmp(CmpIdx));
    const int32       NumBlocksInWidth  = m_NumBlocksInWidth [CmpIdx];
    const int32       NumBlocksInHeight = m_NumBlocksInHeight[CmpIdx];
    const xQuantizer* Quantizer         = &Quant.getQuantizer(QuantTabIdx);

    int16*       CoeffsQuantScan = CoeffsQuantScanV[CmpIdx];
    const int16* CoeffsTrans     = CoeffsTransV    [CmpIdx];

    for(int32 BlockRowIdx = 0; BlockRowIdx < NumBlocksInHeight; BlockRowIdx++) //loop over block rows
    {
      const int32 BlockIdxBeg = BlockRowIdx * NumBlocksInWidth;
      const int32 BlockIdxEnd = BlockIdxBeg + NumBlocksInWidth;
      m_ThPI.storeTask([CoeffsQuantScan, CoeffsTrans, Quantizer, BlockIdxBeg, BlockIdxEnd](int32 /*ThIdx*/) { xFwdQuantScanRng(CoeffsQuantScan, CoeffsTrans, *Quantizer, BlockIdxBeg, BlockIdxEnd); });
    }
  }
  m_ThPI.executeStoredTasks();
}
void xAdvancedEncoder::xFwdQuantScanRng(int16* CoeffQuantScan, const int16* CoeffTrans, const xQuantizer& Quant, int32 BlockIdxBeg, int32 BlockIdxEnd)
{
  PMBB_ALIGN_JPEG_BLK int16 CoeffQuant[c_BA];

  for(int32 BlockIdx = BlockIdxBeg; BlockIdx < BlockIdxEnd; BlockIdx++)
  {
    const int32 BlockOffset = BlockIdx << c_L2BA;
    Quant.QuantScale(CoeffQuant, CoeffTrans + BlockOffset);
    xScan::Scan(CoeffQuantScan + BlockOffset, CoeffQuant);
  }
}
void xAdvancedEncoder::xInvScanQuantPic(int16* CoeffsTransV[], const int16* CoeffsQuantScanV[], const xQuantizerSet& Quant)
{
  for(int32 CmpIdx = 0; CmpIdx < m_NumOfComponents; CmpIdx++)
  {
    const int32       QuantTabIdx       = m_SOF.getQuantTableId(eCmp(CmpIdx));
    const int32       NumBlocksInWidth  = m_NumBlocksInWidth [CmpIdx];
    const int32       NumBlocksInHeight = m_NumBlocksInHeight[CmpIdx];
    const xQuantizer* Quantizer         = &Quant.getQuantizer(QuantTabIdx);

    int16*       CoeffsTrans     = CoeffsTransV    [CmpIdx];
    const int16* CoeffsQuantScan = CoeffsQuantScanV[CmpIdx];

    for(int32 BlockRowIdx = 0; BlockRowIdx < NumBlocksInHeight; BlockRowIdx++) //loop over block rows
    {
      const int32 BlockIdxBeg = BlockRowIdx * NumBlocksInWidth;
      const int32 BlockIdxEnd = BlockIdxBeg + NumBlocksInWidth;

      m_ThPI.storeTask([CoeffsTrans, CoeffsQuantScan, Quantizer, BlockIdxBeg, BlockIdxEnd](int32 /*ThIdx*/) {xInvScanQuantRng(CoeffsTrans, CoeffsQuantScan, *Quantizer, BlockIdxBeg, BlockIdxEnd); });
    }
  }
  m_ThPI.executeStoredTasks();
}
void xAdvancedEncoder::xInvScanQuantRng(int16* CoeffTrans, const int16* CoeffQuantScan, const xQuantizer& Quant, int32 BlockIdxBeg, int32 BlockIdxEnd)
{
  PMBB_ALIGN_JPEG_BLK int16 CoeffQuant[c_BA];

  for(int32 BlockIdx = BlockIdxBeg; BlockIdx < BlockIdxEnd; BlockIdx++)
  {
    const int32 BlockOffset = BlockIdx << c_L2BA;
    xScan::InvScan(CoeffQuant, CoeffQuantScan + BlockOffset);
    Quant.InvScale(CoeffTrans + BlockOffset, CoeffQuant);
  }
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Lambda estimation
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int64V4 xAdvancedEncoder::xCalcBitsHuffPic(const int16* CoeffsQuantScanV[])
{
  std::vector<int64V4> EstNumBits;
  if(m_RestartInterval == 0) //no division - however, process picture in MCU rows
  {
    EstNumBits.resize(m_NumMCUsInHeight);
    for(int32 MCU_RowIdx = 0; MCU_RowIdx < m_NumMCUsInHeight; MCU_RowIdx++) //loop over MCUs rows
    {
      const int32 MCU_IdxFirst = MCU_RowIdx   * m_NumMCUsInWidth    ;
      const int32 MCU_IdxLast  = MCU_IdxFirst + m_NumMCUsInWidth - 1;
      m_ThPI.storeTask([this, &EstNumBits, MCU_RowIdx, &CoeffsQuantScanV, MCU_IdxFirst, MCU_IdxLast](int32 /*ThIdx*/)
        { 
          EstNumBits[MCU_RowIdx] = xCalcBitsHuffSlc(CoeffsQuantScanV, MCU_IdxFirst, MCU_IdxLast);
        });
    }
  }
  else //divide picture into independent slices
  {
    EstNumBits.resize(m_NumOfSlices);
    for(int32 SliceIdx = 0; SliceIdx < m_NumOfSlices; SliceIdx++) //loop over slices
    {
      const int32 MCU_IdxFirst = SliceIdx * m_RestartInterval;
      const int32 MCU_IdxLast  = xMin(m_NumMCUsInArea, MCU_IdxFirst + m_NumMCUsInSlice) - 1;
      m_ThPI.storeTask([this, &EstNumBits, SliceIdx, &CoeffsQuantScanV, MCU_IdxFirst, MCU_IdxLast](int32 /*ThIdx*/)
        { 
          EstNumBits[SliceIdx] = xCalcBitsHuffSlc(CoeffsQuantScanV, MCU_IdxFirst, MCU_IdxLast);
        });
    }
  }
  m_ThPI.executeStoredTasks();

  int64V4 TotalEstNumBits = std::accumulate(EstNumBits.cbegin(), EstNumBits.cend(), xMakeVec4<int64>(0));
  return TotalEstNumBits;
}
int64V4 xAdvancedEncoder::xCalcBitsHuffSlc(const int16* CoeffsQuantScanV[], int32 MCU_IdxFirst, int32 MCU_IdxLast)
{
  int64V4 EstNumBits = xMakeVec4<int64>(0);  
  for(int32 MCU_Idx = MCU_IdxFirst; MCU_Idx <= MCU_IdxLast; MCU_Idx++) //loop over MCUs
  {
    EstNumBits += xCalcBitsHuffMCU(CoeffsQuantScanV, MCU_Idx);
  }
  return EstNumBits;
}
int64V4 xAdvancedEncoder::xCalcBitsHuffMCU(const int16* CoeffsQuantScanV[], int32 MCU_Idx)
{
  int64V4 EstNumBits = xMakeVec4<int64>(0);

  //estimate blocks
  for(int32 CmpIdx = 0; CmpIdx < m_NumOfComponents; CmpIdx++)
  {
    int32 EntropyIdDC = m_SOS.getEntropyIdDC(eCmp(CmpIdx));
    int32 EntropyIdAC = m_SOS.getEntropyIdAC(eCmp(CmpIdx));
    int32 BlockIdx    = MCU_Idx * m_SampFactorVer[CmpIdx] * m_SampFactorHor[CmpIdx];
    
    for(int32 V = 0; V < m_SampFactorVer[CmpIdx]; V++)
    {
      for(int32 H = 0; H < m_SampFactorHor[CmpIdx]; H++)
      {
        const int32 CoeffOffset = BlockIdx << c_L2BA;
        const bool  FirstInSlc  = BlockIdx == 0 || (m_RestartInterval > 0 && MCU_Idx % m_RestartInterval == 0);
        const int32 LastDC      = FirstInSlc ? 0 : CoeffsQuantScanV[CmpIdx][CoeffOffset - c_BA];

        EstNumBits[CmpIdx] += m_EntropyHuffEst.EstimateBlock(CoeffsQuantScanV[CmpIdx] + CoeffOffset, LastDC, EntropyIdDC, EntropyIdAC);
        BlockIdx++;                                                                                 
      }
    }
  }
  return EstNumBits;
}
int64V4 xAdvancedEncoder::xCalcDistPicSSD(const xPicYUV* Tst, const xPicYUV* Ref)
{
  int64V4 SSDs = xMakeVec4<int64>(0);
  for(int32 CmpIdx = 0; CmpIdx < m_NumOfComponents; CmpIdx++)
  {    
    m_ThPI.storeTask([&SSDs, &Tst, &Ref, CmpIdx] (int32 /*ThIdx*/) { eCmp c = (eCmp)CmpIdx; SSDs[CmpIdx] = xDistortion::CalcSSD(Tst->getAddr(c), Ref->getAddr(c), Tst->getStride(c), Ref->getStride(c), Tst->getWidth(c), Tst->getHeight(c), Ref->getBitDepth()); });
  }
  m_ThPI.executeStoredTasks();

  return SSDs;
}
int64V4 xAdvancedEncoder::xEstDistPicSSD(const int16* CoeffsTransRecV[], const int16* CoeffsTransOrgV[])
{
  auto EstDistCmp = [](const int16* CoeffsTransRec, const int16* CoeffsTransOrg, int32 NumBlocks)
    {
      uint64 EstSSD = 0;
      for(int32 BlockIdx = 0; BlockIdx < NumBlocks; BlockIdx++)
      {
        const int32 BlockOffset = BlockIdx << c_L2BA;
        EstSSD += xOptUtils::approxSSDfromCoeffs(CoeffsTransRec + BlockOffset, CoeffsTransOrg + BlockOffset);
      }
      return EstSSD;
    };

  int64V4 EstSSDs = xMakeVec4<int64>(0);

  for(int32 CmpIdx = 0; CmpIdx < m_NumOfComponents; CmpIdx++)
  {
    const int32 NumBlocksInArea = m_NumBlocksInArea[CmpIdx];

    const int16* CoeffsTransOrg = CoeffsTransOrgV[CmpIdx];
    const int16* CoeffsTransRec = CoeffsTransRecV[CmpIdx];

    m_ThPI.storeTask([&EstSSDs, &EstDistCmp, CmpIdx, CoeffsTransOrg, CoeffsTransRec, NumBlocksInArea](int32 /*ThIdx*/)
      { EstSSDs[CmpIdx] = EstDistCmp(CoeffsTransRec, CoeffsTransOrg, NumBlocksInArea); });

  }
  m_ThPI.executeStoredTasks();
  return EstSSDs;
}
int64V4 xAdvancedEncoder::xCalcDistPic(const int16* CoeffsQuantScanV[], const xQuantizerSet& Quant, const xPicYUV* PictureRef)
{
  const int16* ConstCmpCoeffsTransOrg[] = { m_CmpCoeffsTransOrg[0], m_CmpCoeffsTransOrg[1], m_CmpCoeffsTransOrg[2], m_CmpCoeffsTransOrg[3] };
  const int16* ConstCmpCoeffsTransRec[] = { m_CmpCoeffsTransRec[0], m_CmpCoeffsTransRec[1], m_CmpCoeffsTransRec[2], m_CmpCoeffsTransRec[3] };

  xInvScanQuantPic(m_CmpCoeffsTransRec, CoeffsQuantScanV, Quant);

  int64V4 SSDs = xMakeVec4<int64>(0);

  if(m_BlkOptDistCalcMode == eCalkMd::Exact)
  {
    xInvTransformPic(&m_PicRec, ConstCmpCoeffsTransRec);
    SSDs = xCalcDistPicSSD(PictureRef, &m_PicRec);
  }
  else //m_BlkOptDistCalcMode == eCalkMd::Approx
  {
    SSDs = xEstDistPicSSD(ConstCmpCoeffsTransRec, ConstCmpCoeffsTransOrg);
  }
  return SSDs;
}


// ====================================================================================================
// LAMBDA
// ====================================================================================================
void xAdvancedEncoder::xEstimateLambda(const xPicYUV* Picture)
{
  static constexpr flt64 NaN = std::numeric_limits<flt64>::quiet_NaN();

  if(m_LambdaEstMode == eLmbd::ApproxFast)
  {    
    const int32 Q   = xClip(m_Quality, 1, 100);
    const flt64 LnQ = std::log((flt64)Q);

    flt64 a = NaN, b = NaN, c = NaN, d = NaN;
    switch(m_QuantTabLayout)
    {
    case eQTLa::Default : a = NaN; b = -0.312666; c = 0.080179; d = 7.530120; break;
    case eQTLa::Flat    : a = NaN; b = -0.889040; c = 4.250010; d = 2.699890; break;
    case eQTLa::SemiFlat: a = NaN; b = -0.735792; c = 3.083170; d = 3.960580; break;
    default             : assert(0); break;
    }    
    flt64 LambdaL = std::exp(b * xPow2(LnQ) + c * LnQ + d);

    switch(m_QuantTabLayout)
    {
    case eQTLa::Default : a =  0.462055; b = -4.42234; c =  10.86430; d =  0.837368; break; 
    case eQTLa::Flat    : a = -0.604092; b =  5.00975; c = -14.26030; d = 23.097600; break;
    case eQTLa::SemiFlat: a = -0.503516; b =  4.23209; c = -13.10200; d = 23.033500; break;
    default             : assert(0); break;
    }
    flt64 LambdaC = std::exp(a * xPow3(LnQ) + b * xPow2(LnQ) + c * LnQ + d);
    
    m_Lambda = { LambdaL, LambdaC, LambdaC, NaN };
  }
  else if(m_LambdaEstMode == eLmbd::ApproxExact)
  {
    const int16* ConstCmpCoeffsQuantScan[] = { m_CmpCoeffsQuantScan[0], m_CmpCoeffsQuantScan[1], m_CmpCoeffsQuantScan[2], m_CmpCoeffsQuantScan[3] };

    //current point
    int64V4 EstNumBits = xCalcBitsPic(ConstCmpCoeffsQuantScan);
    int64V4 Distortion = xCalcDistPic(ConstCmpCoeffsQuantScan, m_QuantMain, Picture);

    const int64 Area = m_PictureSize.getMul();

    flt64 TotalEstNumBits = (flt64)(EstNumBits.getSum());
    flt64 BitsPerPixel    = TotalEstNumBits / Area;
    flt64 TotalDistSSD    = (flt64)(Distortion.getSum());
    flt64 MSE             = TotalDistSSD / Area;
    if(MSE < 0.001) { MSE = 0.001; } // do we need this

    const int32 Q   = xClip(m_Quality, 1, 100);
    const flt64 LnQ = std::log((flt64)Q    );
    const flt64 LnB = std::log(BitsPerPixel);
    const flt64 LnM = std::log(MSE         );

    flt64 a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
    switch(m_QuantTabLayout)
    {
      case eQTLa::Default : a = -1.19987; b = 1.98030; c = -2.62476; d = 0.72908; e = 0.54911; f = -0.37379; break;
      case eQTLa::Flat    : a =  2.66954; b = 0.78743; c = -1.96582; d = 0.50298; e = 0.19647; f = -0.24829; break;
      case eQTLa::SemiFlat: a =  1.30210; b = 1.16094; c = -2.00048; d = 0.58254; e = 0.26535; f = -0.27819; break;
      default             : assert(0); break;
    }
    flt64 LambdaL = std::exp(a + b * LnQ + c * LnB + d * LnM + e * (LnQ * LnB) + f * (LnQ * LnQ));
    
    switch(m_QuantTabLayout)
    {
    case eQTLa::Default : a = 3.43417; b = 0.34002; c = -3.04510; d = 0.55147; e =  0.50961; f = -0.16052; break;
    case eQTLa::Flat    : a = 3.75927; b = 3.20834; c = -0.38154; d = 0.47452; e = -0.19072; f = -0.76296; break;
    case eQTLa::SemiFlat: a = 1.23927; b = 2.72368; c = -2.18054; d = 0.55767; e =  0.24089; f = -0.55437; break;
    default             : assert(0); break;
    }
    flt64 LambdaC = std::exp(a + b * LnQ + c * LnB + d * LnM + e * (LnQ * LnB) + f * (LnQ * LnQ)) / 2.0;

    m_Lambda = { LambdaL, LambdaC, LambdaC, NaN };
  }
  else if(m_LambdaEstMode == eLmbd::Exhaustive)
  {
    const int16* ConstCmpCoeffsTransOrg    [] = { m_CmpCoeffsTransOrg    [0], m_CmpCoeffsTransOrg    [1], m_CmpCoeffsTransOrg    [2], m_CmpCoeffsTransOrg    [3] };
    const int16* ConstCmpCoeffsQuantScan   [] = { m_CmpCoeffsQuantScan   [0], m_CmpCoeffsQuantScan   [1], m_CmpCoeffsQuantScan   [2], m_CmpCoeffsQuantScan   [3] };
    const int16* ConstCmpCoeffsQuantScanAux[] = { m_CmpCoeffsQuantScanAux[0], m_CmpCoeffsQuantScanAux[1], m_CmpCoeffsQuantScanAux[2], m_CmpCoeffsQuantScanAux[3] };

    //current point
    int64V4 EstNumBitsMain = xCalcBitsPic(ConstCmpCoeffsQuantScan);
    int64V4 DistortionMain = xCalcDistPic(ConstCmpCoeffsQuantScan, m_QuantMain, Picture);

    //lower point
    int64V4 EstNumBitsAuxD = { 0,0,0,0 };
    int64V4 DistortionAuxD = { 0,0,0,0 };
    if(m_Quality > 1)
    {      
      xFwdQuantScanPic(m_CmpCoeffsQuantScanAux , ConstCmpCoeffsTransOrg, m_QuantAuxD);
      EstNumBitsAuxD = xCalcBitsPic(ConstCmpCoeffsQuantScanAux);
      DistortionAuxD = xCalcDistPic(ConstCmpCoeffsQuantScanAux, m_QuantAuxD, Picture);
    }
  
    //higher point
    int64V4 EstNumBitsAuxI = { 0,0,0,0 };
    int64V4 DistortionAuxI = { 0,0,0,0 };
    if(m_Quality < 100)
    {      
      xFwdQuantScanPic(m_CmpCoeffsQuantScanAux , ConstCmpCoeffsTransOrg, m_QuantAuxI);
      EstNumBitsAuxI = xCalcBitsPic(ConstCmpCoeffsQuantScanAux);
      DistortionAuxI = xCalcDistPic(ConstCmpCoeffsQuantScanAux, m_QuantAuxI, Picture);
    }

    //local lambda
    int64V4 DeltaEstNumBitsD = EstNumBitsMain - EstNumBitsAuxD;
    int64V4 DeltaDistortionD = DistortionMain - DistortionAuxD;
    int64V4 DeltaEstNumBitsI = EstNumBitsMain - EstNumBitsAuxI;
    int64V4 DeltaDistortionI = DistortionMain - DistortionAuxI;

    flt64V4 LambdaD = -(flt64V4)DeltaDistortionD / (flt64V4)DeltaEstNumBitsD;
    flt64V4 LambdaI = -(flt64V4)DeltaDistortionI / (flt64V4)DeltaEstNumBitsI;
    if     (m_Quality > 1 && m_Quality < 100) { m_Lambda = (LambdaD + LambdaI) / 2.0; }
    else if(m_Quality > 1                   ) { m_Lambda = LambdaD; }
    else if(m_Quality < 100                 ) { m_Lambda = LambdaI; }

    if(m_VerboseLevel >= 8)
    {
      std::string Dump = "LambdaEstimation\n";
      Dump += fmt::format("QuantMain EstNumBits={:d} {:d} {:d}    Distortion={:d} {:d} {:d}\n", EstNumBitsMain[0], EstNumBitsMain[1], EstNumBitsMain[2], DistortionMain[0], DistortionMain[1], DistortionMain[2]);
      Dump += fmt::format("QuantAuxD EstNumBits={:d} {:d} {:d}    Distortion={:d} {:d} {:d}\n", EstNumBitsAuxD[0], EstNumBitsAuxD[1], EstNumBitsAuxD[2], DistortionAuxD[0], DistortionAuxD[1], DistortionAuxD[2]);
      Dump += fmt::format("QuantAuxI EstNumBits={:d} {:d} {:d}    Distortion={:d} {:d} {:d}\n", EstNumBitsAuxI[0], EstNumBitsAuxI[1], EstNumBitsAuxI[2], DistortionAuxI[0], DistortionAuxI[1], DistortionAuxI[2]);
      Dump += fmt::format("LambdaD = {} {} {}\n", LambdaD[0], LambdaD[1], LambdaD[2]);
      Dump += fmt::format("LambdaI = {} {} {}\n", LambdaI[0], LambdaI[1], LambdaI[2]);
      fmt::print("{}", Dump);
    }
  }
  else { assert(0); }

  if(m_VerboseLevel >= 6)
  {
    fmt::print("Lambda = {} {} {}\n", m_Lambda[0], m_Lambda[1], m_Lambda[2]);
  }
}

// ====================================================================================================
// LAMBDA - RGB
// ====================================================================================================

int64V4 xAdvancedEncoder::xCalcDistPicSSDRGB(const xPicYUV* Tst, const xPicP* Ref)
{
    const uint16* Y = Tst->getAddr(eCmp::LM);
    const uint16* Cb = Tst->getAddr(eCmp::CB);
    const uint16* Cr = Tst->getAddr(eCmp::CR);

    const int32 Width = Tst->getWidth(eCmp::LM);
    const int32 Height = Tst->getHeight(eCmp::LM);
    const int32 PlaneSize = Width * Height;

    std::vector<uint16> RecSamples(3 * PlaneSize);
    uint16* RecR = RecSamples.data();
    uint16* RecG = RecSamples.data() + PlaneSize;
    uint16* RecB = RecSamples.data() + 2 * PlaneSize;


    // ------------------------------------------------------------
    // ------------------------------------------------------------
    // UPSAMPLING CB CR 
    // ------------------------------------------------------------
    const int32 Y_Stride = Tst->getStride(eCmp::LM);
    const int32 Cb_Stride = Tst->getStride(eCmp::CB);
    const int32 Cr_Stride = Tst->getStride(eCmp::CR);

    std::vector<uint16> CbBuffer(Y_Stride * Height);
    std::vector<uint16> CrBuffer((Y_Stride * Height));

    uint16* CbBufferPtr = CbBuffer.data();
    uint16* CrBufferPtr = CrBuffer.data();

    const uint16* CbFull = nullptr;
    const uint16* CrFull = nullptr;

    

    //xPixelOps::UpsampleH(
    //    Dst,
    //    Src,
    //    DstStride,
    //    SrcStride,
    //    DstWidth,
    //    DstHeight
    //);

    const eCrF ChromaFormat = Tst->getChromaFormat();


    if (ChromaFormat == eCrF::CF444)
    {
        CbFull = Cb;
        CrFull = Cr;
    }
    else if (ChromaFormat == eCrF::CF422)
    {
        xPixelOpsSTD::UpsampleH_FIR(CbBufferPtr, Cb, Width, Cb_Stride, Width, Height, Tst->getBitDepth());
        xPixelOpsSTD::UpsampleH_FIR(CrBufferPtr, Cr, Width, Cr_Stride, Width, Height, Tst->getBitDepth());

        CbFull = CbBufferPtr;
        CrFull = CrBufferPtr;
    }
    else if (ChromaFormat == eCrF::CF420)
    {

        xPicYUV Temp422(Tst->getSize(eCmp::LM), Tst->getBitDepth(), eCrF::CF422, Tst->getMargin());
        Temp422.fill(0, eCmp::LM);

        xPixelOpsSTD::UpsampleV_FIR(Temp422.getAddr(eCmp::CB), Cb, Width, Cb_Stride, Width, Height, Tst->getBitDepth());
        xPixelOpsSTD::UpsampleV_FIR(Temp422.getAddr(eCmp::CR), Cr, Width, Cr_Stride, Width, Height, Tst->getBitDepth());

        Temp422.extend(eMrgExt::Nearest);
        
        xPixelOpsSTD::UpsampleH_FIR(CbBufferPtr, Temp422.getAddr(eCmp::CB), Width, Cb_Stride, Width, Height, Tst->getBitDepth());
        xPixelOpsSTD::UpsampleH_FIR(CrBufferPtr, Temp422.getAddr(eCmp::CR), Width, Cr_Stride, Width, Height, Tst->getBitDepth());

        CbFull = CbBufferPtr;
        CrFull = CrBufferPtr;
    }

    // ------------------------------------------------------------
    // ------------------------------------------------------------

    xColorSpace::ConvertYCbCr2RGB(RecR, RecG, RecB, Y, CbFull, CrFull, Width, Tst->getStride(eCmp::LM), Width, Height, 8, eClrSpcLC::JPEG);

    int64V4 DistRGB = xMakeVec4<int64>(0);

    const uint16* RecRGB[3] = { RecR, RecG, RecB };
    for (int32 CmpIdx = 0; CmpIdx < 3; CmpIdx++)
    {
        DistRGB[CmpIdx] = xDistortion::CalcSSD(
            RecRGB[CmpIdx],
            Ref->getAddr(eCmp(CmpIdx)),
            Width,
            Ref->getStride(),
            Width,
            Height,
            8
        );
    }
    return DistRGB;
}


int64V4 xAdvancedEncoder::xCalcDistPicRGB(const int16* CoeffsQuantScanV[], const xQuantizerSet& Quant, const xPicP* PictureRef)
{
    const int16* ConstCmpCoeffsTransOrg[] = { m_CmpCoeffsTransOrg[0], m_CmpCoeffsTransOrg[1], m_CmpCoeffsTransOrg[2], m_CmpCoeffsTransOrg[3] };
    const int16* ConstCmpCoeffsTransRec[] = { m_CmpCoeffsTransRec[0], m_CmpCoeffsTransRec[1], m_CmpCoeffsTransRec[2], m_CmpCoeffsTransRec[3] };

    xInvScanQuantPic(m_CmpCoeffsTransRec, CoeffsQuantScanV, Quant);

    int64V4 SSDs = xMakeVec4<int64>(0);

    if (m_BlkOptDistCalcMode == eCalkMd::Exact)
    {
        xInvTransformPic(&m_PicRec, ConstCmpCoeffsTransRec);

        m_PicRec.extend(eMrgExt::Nearest); // fullfilling margins

        SSDs = xCalcDistPicSSDRGB(&m_PicRec, PictureRef);
    }
    else //m_BlkOptDistCalcMode == eCalkMd::Approx
    {
        SSDs = xEstDistPicSSD(ConstCmpCoeffsTransRec, ConstCmpCoeffsTransOrg);
    }
    return SSDs;
}


void xAdvancedEncoder::xDetermineLambaWeightsRGB(const int32 YCbCr_factors[3])
{
    const auto& Coeffs = xColorSpaceCoeffYCbCr::c_YCbCr2RGB_I32[(int32)eClrSpcLC::JPEG];

    int64 WeightsTmp[3] = { 0, 0, 0 };

    for (int32 RGBCmpIdx = 0; RGBCmpIdx < 3; RGBCmpIdx++)
    {
        const auto& RGB_coeffs = Coeffs[RGBCmpIdx];

        for (int32 CmpIdx = 0; CmpIdx < 3; CmpIdx++)
        {
            const int64 Coeff = RGB_coeffs[CmpIdx];
            WeightsTmp[CmpIdx] += Coeff * Coeff;
        }
    }

    const int64 Scale = 65536LL * 65536LL;

    for (int32 CmpIdx = 0; CmpIdx < 3; CmpIdx++)
    {
        RGB_weights[CmpIdx] =(int32)((WeightsTmp[CmpIdx] * 1000) / Scale);
        RGB_weights[CmpIdx] *= YCbCr_factors[CmpIdx];
    }
}




void xAdvancedEncoder::xEstimateLambdaRGB(const xPicYUV* Picture, const xPicP* PictureRGB)
{
    static constexpr flt64 NaN = std::numeric_limits<flt64>::quiet_NaN();

    // const int32 YCbCr_factors[3] = { 1,2,1 };

    const int32 wr = RGB_weights[0];
    const int32 wg = RGB_weights[1];
    const int32 wb = RGB_weights[2];
    // xDetermineLambaWeightsRGB(YCbCr_factors);

    if (m_LambdaEstMode == eLmbd::Exhaustive)
    {
        const int16* ConstCmpCoeffsTransOrg[] = { m_CmpCoeffsTransOrg[0], m_CmpCoeffsTransOrg[1], m_CmpCoeffsTransOrg[2]};
        const int16* ConstCmpCoeffsQuantScan[] = { m_CmpCoeffsQuantScan[0], m_CmpCoeffsQuantScan[1], m_CmpCoeffsQuantScan[2]};
        const int16* ConstCmpCoeffsQuantScanAux[] = { m_CmpCoeffsQuantScanAux[0], m_CmpCoeffsQuantScanAux[1], m_CmpCoeffsQuantScanAux[2]};

        //current point
        int64V4 EstNumBitsMain = xCalcBitsPic(ConstCmpCoeffsQuantScan);
        int64 EstNumBitsMainTotal = EstNumBitsMain[0] + EstNumBitsMain[1] + EstNumBitsMain[2];
        int64V4 DistortionMain = xCalcDistPicRGB(ConstCmpCoeffsQuantScan, m_QuantMain, PictureRGB);
        int64 DistortionMainTotal = wr * DistortionMain[0] + wg * DistortionMain[1] + wb * DistortionMain[2];

        //lower point
        int64V4 EstNumBitsAuxD = { 0,0,0,0 };
        int64 EstNumBitsAuxDTotal = 0;
        int64V4 DistortionAuxD = { 0,0,0,0 };
        int64 DistortionAuxDTotal = 0;
        if (m_Quality > 1)
        {
            xFwdQuantScanPic(m_CmpCoeffsQuantScanAux, ConstCmpCoeffsTransOrg, m_QuantAuxD);
            EstNumBitsAuxD = xCalcBitsPic(ConstCmpCoeffsQuantScanAux);
            EstNumBitsAuxDTotal = EstNumBitsAuxD[0] + EstNumBitsAuxD[1] + EstNumBitsAuxD[2];
            DistortionAuxD = xCalcDistPicRGB(ConstCmpCoeffsQuantScanAux, m_QuantAuxD, PictureRGB);
            DistortionAuxDTotal = wr * DistortionAuxD[0] + wg * DistortionAuxD[1] + wb * DistortionAuxD[2];
        }

        //higher point
        int64V4 EstNumBitsAuxI = { 0,0,0,0 };
        int64 EstNumBitsAuxITotal = 0;
        int64V4 DistortionAuxI = { 0,0,0,0 };
        int64 DistortionAuxITotal = 0;
        if (m_Quality < 100)
        {
            xFwdQuantScanPic(m_CmpCoeffsQuantScanAux, ConstCmpCoeffsTransOrg, m_QuantAuxI);
            EstNumBitsAuxI = xCalcBitsPic(ConstCmpCoeffsQuantScanAux);
            EstNumBitsAuxITotal = EstNumBitsAuxI[0] + EstNumBitsAuxI[1] + EstNumBitsAuxI[2];
            DistortionAuxI = xCalcDistPicRGB(ConstCmpCoeffsQuantScanAux, m_QuantAuxI, PictureRGB);
            DistortionAuxITotal = wr * DistortionAuxI[0] + wg * DistortionAuxI[1] + wb * DistortionAuxI[2];
        }

        //local lambda
        //int64V4 DeltaEstNumBitsD = EstNumBitsMain - EstNumBitsAuxD;
        //int64V4 DeltaDistortionD = DistortionMain - DistortionAuxD;
        //int64V4 DeltaEstNumBitsI = EstNumBitsMain - EstNumBitsAuxI;
        //int64V4 DeltaDistortionI = DistortionMain - DistortionAuxI;

        int64 DeltaEstNumBitsD = EstNumBitsMainTotal - EstNumBitsAuxDTotal;
        int64 DeltaDistortionD = DistortionMainTotal - DistortionAuxDTotal;
        int64 DeltaEstNumBitsI = EstNumBitsMainTotal - EstNumBitsAuxITotal;
        int64 DeltaDistortionI = DistortionMainTotal - DistortionAuxITotal;


        flt64 LambdaD = -(flt64)DeltaDistortionD / (flt64)DeltaEstNumBitsD;
        flt64 LambdaI = -(flt64)DeltaDistortionI / (flt64)DeltaEstNumBitsI;
        if (m_Quality > 1 && m_Quality < 100) { m_LambdaRGB = (LambdaD + LambdaI) / 2.0; }
        else if (m_Quality > 1) { m_LambdaRGB = LambdaD; }
        else if (m_Quality < 100) { m_LambdaRGB = LambdaI; }

        //if (m_VerboseLevel >= 8)
        //{
        //    std::string Dump = "LambdaEstimation\n";
        //    Dump += fmt::format("QuantMain EstNumBits={:d} {:d} {:d}    Distortion={:d} {:d} {:d}\n", EstNumBitsMain[0], EstNumBitsMain[1], EstNumBitsMain[2], DistortionMain[0], DistortionMain[1], DistortionMain[2]);
        //    Dump += fmt::format("QuantAuxD EstNumBits={:d} {:d} {:d}    Distortion={:d} {:d} {:d}\n", EstNumBitsAuxD[0], EstNumBitsAuxD[1], EstNumBitsAuxD[2], DistortionAuxD[0], DistortionAuxD[1], DistortionAuxD[2]);
        //    Dump += fmt::format("QuantAuxI EstNumBits={:d} {:d} {:d}    Distortion={:d} {:d} {:d}\n", EstNumBitsAuxI[0], EstNumBitsAuxI[1], EstNumBitsAuxI[2], DistortionAuxI[0], DistortionAuxI[1], DistortionAuxI[2]);
        //    Dump += fmt::format("LambdaD = {} {} {}\n", LambdaD[0], LambdaD[1], LambdaD[2]);
        //    Dump += fmt::format("LambdaI = {} {} {}\n", LambdaI[0], LambdaI[1], LambdaI[2]);
        //    fmt::print("{}", Dump);
        //}

        if (m_VerboseLevel >= 8)
        {
            std::string Dump = "LambdaEstimation\n";
            Dump += fmt::format("QuantMain EstNumBits={:d}    Distortion={:d}\n", EstNumBitsMainTotal, DistortionMainTotal);
            Dump += fmt::format("QuantAuxD EstNumBits={:d}    Distortion={:d}\n", EstNumBitsAuxDTotal, DistortionAuxDTotal);
            Dump += fmt::format("QuantAuxI EstNumBits={:d}    Distortion={:d}\n", EstNumBitsAuxITotal, DistortionAuxITotal);
            Dump += fmt::format("LambdaD = {}\n", LambdaD);
            Dump += fmt::format("LambdaI = {}\n", LambdaI);
            fmt::print("{}", Dump);
        }
    }
    //else { assert(0); }

    //if (m_VerboseLevel >= 6)
    //{
    //    fmt::print("Lambda = {} {} {}\n", m_Lambda[0], m_Lambda[1], m_Lambda[2]);
    //}

    else { assert(0); }

    if (m_VerboseLevel >= 6)
    {
        fmt::print("Lambda = {}\n", m_LambdaRGB);
    }
}



//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// RDOQ
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void xAdvancedEncoder::xOptQuantHuffPic(int16* OptCoeffsQuantScanV[], const int16* CoeffsQuantScanV[], const int16* CoeffsTransOrgV[], const xPicYUV* Picture)
{
  if(m_RestartInterval == 0) 
  {
    for(int32 MCU_RowIdx = 0; MCU_RowIdx < m_NumMCUsInHeight; MCU_RowIdx++) //loop over MCUs rows
    {
      const int32 MCU_IdxFirst = MCU_RowIdx   * m_NumMCUsInWidth    ;
      const int32 MCU_IdxLast  = MCU_IdxFirst + m_NumMCUsInWidth - 1;
      m_ThPI.storeTask([this, &OptCoeffsQuantScanV, &CoeffsQuantScanV, &CoeffsTransOrgV, &Picture, MCU_IdxFirst, MCU_IdxLast](int32 /*ThIdx*/)
        {
          xOptQuantHuffSlc(OptCoeffsQuantScanV, CoeffsQuantScanV, CoeffsTransOrgV, Picture, MCU_IdxFirst, MCU_IdxLast);
        });
    }
  }
  else //divide picture into independent slices
  {
    for(int32 SliceIdx = 0; SliceIdx < m_NumOfSlices; SliceIdx++)
    {
      const int32 MCU_IdxFirst = SliceIdx * m_RestartInterval;
      const int32 MCU_IdxLast  = xMin(m_NumMCUsInArea, MCU_IdxFirst + m_RestartInterval) - 1;
      m_ThPI.storeTask([this, &OptCoeffsQuantScanV, &CoeffsQuantScanV, &CoeffsTransOrgV, &Picture, MCU_IdxFirst, MCU_IdxLast](int32 /*ThIdx*/)
        {
          xOptQuantHuffSlc(OptCoeffsQuantScanV, CoeffsQuantScanV, CoeffsTransOrgV, Picture, MCU_IdxFirst, MCU_IdxLast);
        });
    }
  }
  m_ThPI.executeStoredTasks();
}
void xAdvancedEncoder::xOptQuantHuffSlc(int16* OptCoeffsQuantScanV[], const int16* CoeffsQuantScanV[], const int16* CoeffsTransOrgV[], const xPicYUV* Picture, int32 MCU_IdxFirst, int32 MCU_IdxLast)
{
  const uint16* CmpPtrV   [] = {Picture->getAddr  (eCmp::LM), Picture->getAddr  (eCmp::CB), Picture->getAddr  (eCmp::CR), nullptr};
  const int32   CmpStrideV[] = {Picture->getStride(eCmp::LM), Picture->getStride(eCmp::CB), Picture->getStride(eCmp::CR),       0};

  //loop over MCUs
  for(int32 MCU_Idx = MCU_IdxFirst; MCU_Idx <= MCU_IdxLast; MCU_Idx++)
  {
    xOptQuantHuffMCU(OptCoeffsQuantScanV, CoeffsQuantScanV, CoeffsTransOrgV, CmpPtrV, CmpStrideV, MCU_Idx);
  }
}
void xAdvancedEncoder::xOptQuantHuffMCU(int16* OptCoeffsQuantScanV[], const int16* CoeffsQuantScanV[], const int16* CoeffsTransOrgV[], const uint16* CmpPtrV[], const int32 CmpStrideV[], int32 MCU_Idx)
{
  //calculate MCU position
  int32 MCU_PosV = MCU_Idx / m_NumMCUsInWidth;
  int32 MCU_PosH = MCU_Idx % m_NumMCUsInWidth;

  //org samples buffer
  PMBB_ALIGN_JPEG_BLK uint16 SamplesOrg[c_BA];

  //transform blocks
  for(int32 CmpIdx = 0; CmpIdx < m_NumOfComponents; CmpIdx++)
  {
    const int32 MCU_PelPosV = MCU_PosV << (2 + m_SampFactorVer[CmpIdx]);
    const int32 MCU_PelPosH = MCU_PosH << (2 + m_SampFactorHor[CmpIdx]);

    const uint16* CmpPtr    = CmpPtrV   [CmpIdx];
    const int32   CmpStride = CmpStrideV[CmpIdx];

    int32 BlockIdx = MCU_Idx * m_SampFactorVer[CmpIdx] * m_SampFactorHor[CmpIdx];
    for(int32 V = 0; V < m_SampFactorVer[CmpIdx]; V++)
    {
      const int32 BlockPosV = MCU_PelPosV + V * c_BS;
      const int32 BlockResV = m_CmpHeight[CmpIdx] - BlockPosV;

      for(int32 H = 0; H < m_SampFactorHor[CmpIdx]; H++)
      {
        const int32 BlockPosH = MCU_PelPosH + H * c_BS;
        const int32 BlockResH = m_CmpWidth[CmpIdx] - BlockPosH;

        const uint16* BlockPtr = CmpPtr + BlockPosV * CmpStride + BlockPosH;          

        if     (BlockResV >= 8 && BlockResH >= 8) { loadEntireBlock(SamplesOrg, BlockPtr, CmpStride); } //C++20 TODO use [[likely]]
        else if(BlockResV >  0 && BlockResH >  0) { loadExtendBlock(SamplesOrg, BlockPtr, CmpStride, xMin(BlockResH, c_BS), xMin(BlockResV, c_BS)); }
        else                                      { zeroEntireBlock(SamplesOrg); }

        const int32 CoeffOffset = BlockIdx << c_L2BA;
        const bool  FirstInSlc  = BlockIdx == 0 || (m_RestartInterval > 0 && MCU_Idx % m_RestartInterval == 0);
        const int32 LastDC      = FirstInSlc ? 0 : CoeffsQuantScanV[CmpIdx][CoeffOffset - c_BA];
        xOptQuantHuffBLK(OptCoeffsQuantScanV[CmpIdx] + CoeffOffset, CoeffsQuantScanV[CmpIdx] + CoeffOffset, CoeffsTransOrgV[CmpIdx] + CoeffOffset, SamplesOrg, eCmp(CmpIdx), LastDC);
        BlockIdx++;
      }
    }
  }
}
void xAdvancedEncoder::xOptQuantHuffBLK(int16* OptCoeffQuantScan, const int16* CoeffsQuantScan, const int16* CoeffsTransOrg, const uint16* SamplesOrg, eCmp CmpId, int32 LastDC)
{
  const int32 QuantTabId  = m_SOF.getQuantTableId(CmpId);
  const int32 EntropyIdDC = m_SOS.getEntropyIdDC (CmpId);
  const int32 EntropyIdAC = m_SOS.getEntropyIdAC (CmpId);
  const flt64 Lambda      = m_Lambda[(int32)CmpId];

  const xQuantizer& Quantizer = m_QuantMain.getQuantizer(QuantTabId);

  int32 LastNonZero = xEntropyUtils::findLastNonZero(CoeffsQuantScan);
  if(LastNonZero == 0) { memcpy(OptCoeffQuantScan, CoeffsQuantScan, c_BA * sizeof(int16)); return; } //only DC - nothing to do here

  const int32 OrgBitsDC = m_EntropyHuffEst.EstimateBlockDC(CoeffsQuantScan, LastDC, EntropyIdDC);
  const int32 OrgBitsAC = m_EntropyHuffEst.EstimateBlockAC(CoeffsQuantScan,         EntropyIdAC);

  int32  BestBits = OrgBitsDC + OrgBitsAC;
  uint64 BestDist = xCalcDistBLK(CoeffsQuantScan, CoeffsTransOrg, SamplesOrg, Quantizer);
  flt64  BestCost = (flt64)BestDist + Lambda * (flt64)BestBits;

  PMBB_ALIGN_JPEG_BLK int16 TmpCoeffsQuantScan[c_BA];
  memcpy(TmpCoeffsQuantScan, CoeffsQuantScan, c_BA * sizeof(int16));

  for (int32 PassIdx = 0; PassIdx < m_NumOptPassesBlock; PassIdx++)
  {    
    for (int32 i = LastNonZero; i >= 1; i--)
    {
      const int16 OrgCoeff  = TmpCoeffsQuantScan[i];
      if (!m_ProcessZeroCoeffs && OrgCoeff == 0) { continue; }
      
      int16 BestCoeff = TmpCoeffsQuantScan[i];
      //try zero
      if(OrgCoeff != 0)
      {
        TmpCoeffsQuantScan[i] = 0;
        int32  CurrBits = OrgBitsDC + m_EntropyHuffEst.EstimateBlockAC(TmpCoeffsQuantScan, EntropyIdAC);
        uint64 CurrDist = xCalcDistBLK(TmpCoeffsQuantScan, CoeffsTransOrg, SamplesOrg, Quantizer);
        flt64  CurrCost = (double)CurrDist + Lambda * (flt64)CurrBits;
        if (CurrCost < BestCost)
        {
          BestBits  = CurrBits;
          BestCost  = CurrCost;
          BestCoeff = 0;
        }
      }

      //try +1
      if(OrgCoeff != -1)
      {
        TmpCoeffsQuantScan[i] = OrgCoeff + 1;
        int32  CurrBits = OrgBitsDC + m_EntropyHuffEst.EstimateBlockAC(TmpCoeffsQuantScan, EntropyIdAC);
        uint64 CurrDist = xCalcDistBLK(TmpCoeffsQuantScan, CoeffsTransOrg, SamplesOrg, Quantizer);
        flt64  CurrCost = (flt64)CurrDist + Lambda * (flt64)CurrBits;
        if (CurrCost < BestCost)
        {
          BestBits = CurrBits;
          BestCost  = CurrCost;
          BestCoeff = OrgCoeff + 1;
        }
      }

      //try -1  
      if(OrgCoeff != 1)
      {
        TmpCoeffsQuantScan[i] = OrgCoeff - 1;
        int32  CurrBits = OrgBitsDC + m_EntropyHuffEst.EstimateBlockAC(TmpCoeffsQuantScan, EntropyIdAC);
        uint64 CurrDist = xCalcDistBLK(TmpCoeffsQuantScan, CoeffsTransOrg, SamplesOrg, Quantizer);
        flt64  CurrCost = (flt64)CurrDist + Lambda * (flt64)CurrBits;
        if (CurrCost < BestCost)
        {
          BestBits = CurrBits;
          BestCost  = CurrCost;
          BestCoeff = OrgCoeff - 1;
        }
      }

      TmpCoeffsQuantScan[i] = BestCoeff;
    }
  }

  //int32 TestBits = m_EntropyHuffEst.EstimateBlock(TmpCoeffsScan, CmpId, HuffTabIdDC, HuffTabIdAC);
  memcpy(OptCoeffQuantScan, TmpCoeffsQuantScan, c_BA * sizeof(int16));
}
uint64 xAdvancedEncoder::xCalcDistBLK(const int16* CoeffsQuantScan, const int16* CoeffsTransOrgScan, const uint16* SamplesOrg, const xQuantizer& Quantizer)
{
  if     (m_BlkOptDistCalcMode == eCalkMd::Exact ) { return xCalkExactDistBLK(CoeffsQuantScan, SamplesOrg        , Quantizer); }
  else if(m_BlkOptDistCalcMode == eCalkMd::Approx) { return xCalkApprxDistBLK(CoeffsQuantScan, CoeffsTransOrgScan, Quantizer); }
  else                                             { assert(0); return 0; }
}
uint64 xAdvancedEncoder::xCalkExactDistBLK(const int16* CoeffsQuantScan, const uint16* SamplesOrg, const xQuantizer& Quantizer)
{
  PMBB_ALIGN_JPEG_BLK int16  TmpQuantCoeffs[c_BA];
  PMBB_ALIGN_JPEG_BLK int16  TmpTransCoeffs[c_BA];
  PMBB_ALIGN_JPEG_BLK uint16 TmpSamples    [c_BA];

  xScan::InvScan(TmpQuantCoeffs, CoeffsQuantScan);
  Quantizer.InvScale(TmpTransCoeffs, TmpQuantCoeffs);
  TmpTransCoeffs[0] += xTransformConstants::c_InvDcCorr; //DC correction - JPEG requires 128 to be subtracted from every input sample - could be done be DC -= 
  xTransform::InvTransformDCT_8x8(TmpSamples, TmpTransCoeffs);
  uint64 SSD = xDistortion::CalcSSD(SamplesOrg, TmpSamples, c_BS, c_BS, c_BS, c_BS, 8);
  return SSD;
}
uint64 xAdvancedEncoder::xCalkApprxDistBLK(const int16* CoeffsQuantScan, const int16* CoeffsTransOrg, const xQuantizer& Quantizer)
{
  PMBB_ALIGN_JPEG_BLK int16 RecQuantCoeffs[c_BA];
  PMBB_ALIGN_JPEG_BLK int16 RecTransCoeffs[c_BA];

  xScan::InvScan(RecQuantCoeffs, CoeffsQuantScan);
  Quantizer.InvScale(RecTransCoeffs, RecQuantCoeffs);
  uint64 SSD = xOptUtils::approxSSDfromCoeffs(RecTransCoeffs, CoeffsTransOrg);
  return SSD;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  //Huffman tables optimization
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void xAdvancedEncoder::xOptHuffPic(const int16* CoeffsQuantScanV[])
{
  //calc symbol symbols
  xCntHuffPic(CoeffsQuantScanV);

  //build huffman tables
  std::vector<xJFIF::xHuffTable> NewHT = m_HTs;

  for(xJFIF::xHuffTable& HuffTable : NewHT)
  {
    xJFIF::xHuffTable::tClass HuffTableClass = HuffTable.getClass();
    int32                     HuffTableId    = HuffTable.getIdx  ();
    switch(HuffTableClass)
    {
    case xJFIF::xHuffTable::tClass::DC:
    {
      const uint32* SymbolCount = m_EntropyHuffCnts[0].getSymbolCountDC(HuffTableId);
      uint8 LengthTable[xJPEG_Constants::c_MaxNumCodeSymbolsDC+1];
      xHuffmanTabBuilder::buildLengthTable(LengthTable, SymbolCount, xJPEG_Constants::c_MaxNumCodeSymbolsDC);
      HuffTable.InitCustom((uint8)HuffTableId, HuffTableClass, LengthTable);
      break;
    }
    case xJFIF::xHuffTable::tClass::AC:
    {
      const uint32* SymbolCount = m_EntropyHuffCnts[0].getSymbolCountAC(HuffTableId);
      uint8 LengthTable[xJPEG_Constants::c_MaxNumCodeSymbolsAC+1];
      xHuffmanTabBuilder::buildLengthTable(LengthTable, SymbolCount, xJPEG_Constants::c_MaxNumCodeSymbolsAC);
      HuffTable.InitCustom((uint8)HuffTableId, HuffTableClass, LengthTable);
      break;
    }
    default: break;
    }
  }

  //print tables
  if(m_VerboseLevel >= 8)
  {
    std::string Dump = fmt::format("HuffmanTablesPrev\n");
    for(int32 i = 0; i < (int32)m_HTs.size(); i++) { Dump += fmt::format("  Table_{}\n", i) + m_HTs[i].Format("    "); }
    Dump += fmt::format("HuffmanTablesNext\n");
    for(int32 i = 0; i < (int32)NewHT.size(); i++) { Dump += fmt::format("  Table_{}\n", i) + NewHT[i].Format("    "); }
    fmt::print("{}\n", Dump);
  }

  if(m_VerboseLevel >= 6)
  {
    std::string Dump = fmt::format("HuffmanTablesPrev\n");
    for(xJFIF::xHuffTable& HuffTable : m_HTs)
    {
      const uint32* SymbolCount = m_EntropyHuffCnts[0].getSymbolCount(HuffTable.getClass(), HuffTable.getIdx());
      flt64 AvgCodeLength = xHuffmanTabBuilder::calcAvgCodeLength(HuffTable, SymbolCount);
      Dump += fmt::format("  Table Idx={} Class={} AvgCodeLength={:7.5f}\n", HuffTable.getIdx(), xJFIF::xEntropyTableClass2Str(HuffTable.getClass()), AvgCodeLength);
    }
    Dump += fmt::format("HuffmanTablesNext\n");
    for(xJFIF::xHuffTable& HuffTable : NewHT)
    {
      const uint32* SymbolCount = m_EntropyHuffCnts[0].getSymbolCount(HuffTable.getClass(), HuffTable.getIdx());
      flt64 AvgCodeLength = xHuffmanTabBuilder::calcAvgCodeLength(HuffTable, SymbolCount);
      Dump += fmt::format("  Table Idx={} Class={} AvgCodeLength={:7.5f}\n", HuffTable.getIdx(), xJFIF::xEntropyTableClass2Str(HuffTable.getClass()), AvgCodeLength);
    }
    fmt::print("{}\n", Dump);
  }

  m_HTs = NewHT;
  
  if(m_NumOptPassesPic > 1) { m_EntropyHuffEst.Init(m_HTs); }
  m_EncoderHuffBank.Init(m_HTs);
  for(int32 i = 0; i < (int32)m_EntropyHuffEncs.size(); i++) { m_EntropyHuffEncs[i].SetEncoders(m_EncoderHuffBank); }
}
void xAdvancedEncoder::xCntHuffPic(const int16* CoeffsQuantScanV[])
{
  const int32 NumCounters = (int32)m_EntropyHuffCnts.size();
  for(int32 i = 0; i < NumCounters; i++) { m_EntropyHuffCnts[i].Init(m_HTs); }

  //count symbols
  for(int32 CmpIdx = 0; CmpIdx < m_NumOfComponents; CmpIdx++)
  {
    const int32  EntropyIdDC       = m_SOS.getEntropyIdDC(eCmp(CmpIdx));
    const int32  EntropyIdAC       = m_SOS.getEntropyIdAC(eCmp(CmpIdx));
    const int32  BlocksPerSlice    = m_RestartInterval * m_NumBlocksInMCU[CmpIdx];
    const int32  NumBlocksInWidth  = m_NumBlocksInWidth [CmpIdx];
    const int32  NumBlocksInHeight = m_NumBlocksInHeight[CmpIdx];
    const int16* CoeffsQuantScan   = CoeffsQuantScanV[CmpIdx];

    for(int32 BlockRowIdx = 0; BlockRowIdx < NumBlocksInHeight; BlockRowIdx++) //loop over block rows
    {
      const int32 BlockIdxBeg = BlockRowIdx * NumBlocksInWidth;
      const int32 BlockIdxEnd = BlockIdxBeg + NumBlocksInWidth;
      m_ThPI.storeTask([this, CoeffsQuantScan, EntropyIdDC, EntropyIdAC, BlockIdxBeg, BlockIdxEnd, BlocksPerSlice](int32 ThIdx) { xCntHuffRng(CoeffsQuantScan, ThIdx, EntropyIdDC, EntropyIdAC, BlockIdxBeg, BlockIdxEnd, BlocksPerSlice); });
    }
  }
  m_ThPI.executeStoredTasks();

  //agregate counters
  for(int32 i = 1; i < NumCounters; i++)
  {
    m_EntropyHuffCnts[0].AddCounters(m_EntropyHuffCnts[i]);
  }
}
void xAdvancedEncoder::xCntHuffRng(const int16* CoeffsQuantScan, int32 CounterIdx, int32 HuffTabIdDC, int32 HuffTabIdAC, int32 BlockIdxBeg, int32 BlockIdxEnd, int32 BlocksPerSlice)
{
  for(int32 BlockIdx = BlockIdxBeg; BlockIdx < BlockIdxEnd; BlockIdx++)
  {
    const int32 CoeffOffset = BlockIdx << c_L2BA;
    const bool  FirstInSlc  = BlockIdx == 0 || (m_RestartInterval > 0 && BlockIdx % BlocksPerSlice == 0);
    const int32 LastDC      = FirstInSlc ? 0 : CoeffsQuantScan[CoeffOffset - c_BA];
    m_EntropyHuffCnts[CounterIdx].CountBlock(CoeffsQuantScan + CoeffOffset, LastDC, HuffTabIdDC, HuffTabIdAC);
  }
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Huffman
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void xAdvancedEncoder::xEncodeHuffPic(const int16* CoeffsQuantScanV[])
{
  if(m_RestartInterval == 0) //no division - however, try process picture in MCU rows
  {
    if(!m_ThPI.isActive()) //no active thread pool - encode entire picture at once
    {
      xEncodeHuffSlc(CoeffsQuantScanV, 0, 0, m_NumMCUsInArea - 1);
    }
    else //encode picture in MCU 
    {
      for(int32 MCU_RowIdx = 0; MCU_RowIdx < m_NumMCUsInHeight; MCU_RowIdx++) //loop over MCUs rows
      {
        const int32 MCU_IdxFirst = MCU_RowIdx   * m_NumMCUsInWidth;
        const int32 MCU_IdxLast  = MCU_IdxFirst + m_NumMCUsInWidth - 1;        
        m_ThPI.storeTask([this, &CoeffsQuantScanV, MCU_RowIdx, MCU_IdxFirst, MCU_IdxLast](int32 /*ThIdx*/) { xEncodeHuffChk(CoeffsQuantScanV, MCU_RowIdx, MCU_IdxFirst, MCU_IdxLast); });
      }
    }
  }
  else //divide picture into independent slices
  {
    for(int32 SliceIdx = 0; SliceIdx < m_NumOfSlices; SliceIdx++) //loop over slices
    {
      const int32 MCU_IdxFirst = SliceIdx * m_RestartInterval;
      const int32 MCU_IdxLast  = xMin(m_NumMCUsInArea, MCU_IdxFirst + m_RestartInterval) - 1;
      m_ThPI.storeTask([this, &CoeffsQuantScanV, SliceIdx, MCU_IdxFirst, MCU_IdxLast](int32 /*ThIdx*/) { xEncodeHuffSlc(CoeffsQuantScanV, SliceIdx, MCU_IdxFirst, MCU_IdxLast); });
    }
  }
  m_ThPI.executeStoredTasks();
}
void xAdvancedEncoder::xEncodeHuffSlc(const int16* CoeffsQuantScanV[], int32 SliceIdx, int32 MCU_IdxFirst, int32 MCU_IdxLast)
{
  m_EncBuffers     [SliceIdx].reset();
  m_EntropyHuffEncs[SliceIdx].StartSlice(&m_EncBuffers[SliceIdx]);
  for(int32 MCU_Idx = MCU_IdxFirst; MCU_Idx <= MCU_IdxLast; MCU_Idx++) { xEncodeHuffMCU(CoeffsQuantScanV, SliceIdx, MCU_Idx); }
  m_EntropyHuffEncs[SliceIdx].FinishSlice();
  m_SliceBuffers   [SliceIdx].reset();
  xJFIF::AddStuffing(&m_SliceBuffers[SliceIdx], &m_EncBuffers[SliceIdx]);
}
void xAdvancedEncoder::xEncodeHuffChk(const int16* CoeffsQuantScanV[], int32 ChunkIdx, int32 MCU_IdxFirst, int32 MCU_IdxLast)
{
  xEntropyHuffCommon::tLDCs LastDCs = { 0,0,0,0 };
  if(MCU_IdxFirst != 0)
  {
    for(int32 CmpIdx = 0; CmpIdx < m_NumOfComponents; CmpIdx++)
    {
      const int32 BlockIdx    = MCU_IdxFirst * m_SampFactorVer[CmpIdx] * m_SampFactorHor[CmpIdx];
      const int32 CoeffOffset = BlockIdx << c_L2BA;
      const int16 LastDC      = CoeffsQuantScanV[CmpIdx][CoeffOffset - c_BA];
      LastDCs[CmpIdx] = LastDC;
    }
  }

  m_EncBuffers     [ChunkIdx].reset();
  m_EntropyHuffEncs[ChunkIdx].StartChunk(&m_EncBuffers[ChunkIdx], LastDCs);
  for(int32 MCU_Idx = MCU_IdxFirst; MCU_Idx <= MCU_IdxLast; MCU_Idx++) { xEncodeHuffMCU(CoeffsQuantScanV, ChunkIdx, MCU_Idx); }
  m_NumAlignmentBits[ChunkIdx] = m_EntropyHuffEncs[ChunkIdx].FinishChunk();
}
void xAdvancedEncoder::xEncodeHuffMCU(const int16* CoeffsQuantScanV[], int32 SliceIdx, int32 MCU_Idx)
{
  for(int32 CmpIdx = 0; CmpIdx < m_NumOfComponents; CmpIdx++)
  {
    int32 EntropyIdDC = m_SOS.getEntropyIdDC(eCmp(CmpIdx));
    int32 EntropyIdAC = m_SOS.getEntropyIdAC(eCmp(CmpIdx));

    int32 BlockIdx = MCU_Idx * m_SampFactorVer[CmpIdx] * m_SampFactorHor[CmpIdx];
    for(int32 V = 0; V < m_SampFactorVer[CmpIdx]; V++)
    {
      for(int32 H = 0; H < m_SampFactorHor[CmpIdx]; H++)
      {
        const int32 CoeffScanOffset = BlockIdx << c_L2BA;        
        m_EntropyHuffEncs[SliceIdx].EncodeBlock(CoeffsQuantScanV[CmpIdx] + CoeffScanOffset, eCmp(CmpIdx), EntropyIdDC, EntropyIdAC);
        BlockIdx++;
      }
    }
  }
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Writeout
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void xAdvancedEncoder::xWritePic(xByteBuffer* OutputBuffer)
{
  if(m_RestartInterval == 0)
  {
    if(!m_ThPI.isActive())
    {
      OutputBuffer->transfer(&m_SliceBuffers[0]);
    }
    else
    {
      m_CombineBuffer.reset();
      xBitstreamWriter Bitstream;
      Bitstream.bindByteBuffer(&m_CombineBuffer);
      Bitstream.init();
      for(int32 MCU_RowIdx = 0; MCU_RowIdx < m_NumMCUsInHeight; MCU_RowIdx++)
      {
        Bitstream.writeBuffer(&m_EncBuffers[MCU_RowIdx], m_NumAlignmentBits[MCU_RowIdx]);
      }
      Bitstream.writeAlign(1);
      Bitstream.uninit();
      Bitstream.unbindByteBuffer();
      xJFIF::AddStuffing(OutputBuffer, &m_CombineBuffer);
    }
  }
  else
  {
    for(int32 SliceIdx = 0; SliceIdx < m_NumOfSlices; SliceIdx++)
    {
      const int32 MCU_IdxFirst = SliceIdx * m_RestartInterval;
      const int32 MCU_IdxLast = xMin(m_NumMCUsInArea, MCU_IdxFirst + m_RestartInterval) - 1;
      //copy to output and add stuffing
      OutputBuffer->transfer(&m_SliceBuffers[SliceIdx]);
      if(m_RestartInterval >= 0)
      {
        if(MCU_IdxLast != m_NumMCUsInArea - 1)
        {
          xJFIF::WriteRST(OutputBuffer, (uint8)((uint32)SliceIdx & (uint32)0x07));
        }
      }
    }
  }
}

//=====================================================================================================================================================================================

} //end of namespace PMBB::JPEG