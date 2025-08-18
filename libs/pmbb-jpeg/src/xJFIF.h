/*
    SPDX-FileCopyrightText: 2020-2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include "xCommonDefJPEG.h"
#include "xJPEG_Constants.h"
#include "xJPEG_MarkerUtils.h"
#include "xByteBuffer.h"
#include "xVec.h"
#include <numeric>
#include <vector>
#include <array>

namespace PMBB_NAMESPACE::JPEG {

//=============================================================================================================================================================================

class xJFIF
{
public:
  using tStr   = std::string;
  using tByteV = std::vector<byte>;
  using tPosV  = std::vector<int64>;

  static constexpr int32  c_SeekBufferSize = 4194304; //4MB = 1024 pages

  static constexpr uint32 c_JFIF = xMakeFourCC('J', 'F', 'I', 'F');

  enum class eMarker
  {
    //Start Of Frame markers, non-differential, Huffman coding
    SOF0  = 0xC0, //Start Of Frame - Baseline DCT           , Huffman coding
    SOF1  = 0xC1, //Start Of Frame - Extended sequential DCT, Huffman coding
    SOF2  = 0xC2, //Start Of Frame - Progressive DCT        , Huffman coding
    SOF3  = 0xC3, //Start Of Frame - Lossless (sequential)  , Huffman coding
    //Start Of Frame markers, differential, Huffman coding
    SOF5  = 0xC5, //Start Of Frame - Differential sequential DCT
    SOF6  = 0xC6, //Start Of Frame - Differential progressive DCT
    SOF7  = 0xC7, //Start Of Frame - Differential lossless (sequential)
    //Start Of Frame markers, non-differential, arithmetic coding
    JPG   = 0xC8, //Start Of Frame - Reserved for JPEG extensions
    SOF9  = 0xC9, //Start Of Frame - Extended sequential DCT, arithmetic coding
    SOF10 = 0xCA, //Start Of Frame - Progressive DCT        , arithmetic coding
    SOF11 = 0xCB, //Start Of Frame - Lossless (sequential)  , arithmetic coding
    //Start Of Frame markers, differential, arithmetic coding
    SOF13 = 0xCD, //Start Of Frame - Differential sequential DCT
    SOF14 = 0xCE, //Start Of Frame - Differential progressive DCT
    SOF15 = 0xCF, //Start Of Frame - Differential lossless (sequential)

    DHT   = 0xC4, //Define Huffman Table 
    DAC   = 0xCC, //Define Arithmetic Table
         
    RST0  = 0xD0, //Restart(s)
    RST1  = 0xD1,
    RST2  = 0xD2,
    RST3  = 0xD3,
    RST4  = 0xD4,
    RST5  = 0xD5,
    RST6  = 0xD6,
    RST7  = 0xD7,

    SOI   = 0xD8, //Start Of Image 
    EOI   = 0xD9, //End Of Image
    SOS   = 0xDA, //Start Of Scan
    DQT   = 0xDB, //Define Quantization Table
    DNL   = 0xDC, //Define number of lines
    DRI   = 0xDD, //Define Restart Interval
    DHP   = 0xDE, //Define hierarchical progression
    EXP   = 0xDF, //Expand reference component(s)

    APP0  = 0xE0, //JFIF APP0 segment marker 
    //.....
    APP15 = 0xEF, //JFIF APP15 segment marker

    JPG0  = 0xF0, //Reserved for JPEG extensions
    //.....
    JPG13 = 0xFD, //Reserved for JPEG extensions

    COM   = 0xFE, //Comment
    TEM   = 0x01, //For temporary private use in arithmetic coding (???)

    RES02 = 0x02, //Reserved First
    //.....
    RESBF = 0xBF, //Reserved Last

    ERR   = 0x00,
  };

  enum class eEntropyTableClass : int8
  {
    Invalid = NOT_VALID,
    DC = 0,
    AC = 1,
  };

  static tStr xEntropyTableClass2Str(eEntropyTableClass Class)
  {
    switch(Class)
    {
    case eEntropyTableClass::Invalid: return "Invalid";
    case eEntropyTableClass::DC     : return "DC"     ;
    case eEntropyTableClass::AC     : return "AC"     ;
    default                         : return "Unknown";
    }
  }

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

  class xAPP0
  {
  protected:
    int32  m_VersionMajor   = 0;
    int32  m_VersionMinor   = 0;
    int32  m_DensityUnits   = 0; // 0 = no units, x/y-density specify the aspect ratio instead; 1 = x/y-density are dots/inch; 2 = x/y-density are dots/cm
    int32  m_DensityX       = 0;
    int32  m_DensityY       = 0;
    int32  m_ThumbnailSizeX = 0;
    int32  m_ThumbnailSizeY = 0;
    tByteV m_ThumbnailData;

  public:
    int32 Absorb(xByteBuffer* Input , int32 SegmentLength);
    int32 Emit  (xByteBuffer* Output) const; 
    void  InitDefault();
    bool  Validate   ();
    int32 getLength  () const { return 16 + m_ThumbnailSizeX * m_ThumbnailSizeY * 3; }
  };

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  
  class xQuantTable
  {
  protected:
    int32  m_Idx       = NOT_VALID;
    int32  m_Precision = NOT_VALID; //008bit, 1=16bit
    tByteV m_Table;

  public:
    xQuantTable() : m_Table(64) { }
    int32 Absorb   (xByteBuffer* Input );
    int32 Emit     (xByteBuffer* Output) const; 
    void  Init     (uint8 Idx, eCmp Cmp, int32 Quality, eQTLa QuantTabLayout = eQTLa::Default);
    bool  Validate () const;
    tStr  Format   (const tStr& Prefix = "  ") const;

    int32  getLength   () const { return 1 + (m_Precision == 1 ? 128 : 64); }
    int32  getIdx      (         ) const { return m_Idx; }
    void   setIdx      (int32 Idx)       {m_Idx = (int8)Idx; }
    int32  getPrecision(               ) const { return m_Precision; }
    void   setPrecision(int32 Precision)       { m_Precision = (int8)Precision; }
    
    const tByteV& getTableData(                 ) const { return m_Table; }
    void          setTableData(const tByteV& QTD)       { m_Table = QTD; }
  };

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

  class xSOF
  {
  protected:
    int32  m_Type          = NOT_VALID;
    int32  m_Width         = NOT_VALID;
    int32  m_Height        = NOT_VALID;
    int32  m_BitDepth      = NOT_VALID;
    int32  m_NumComponents = NOT_VALID;
    eCmp   m_CmpId         [xJPEG_Constants::c_MaxComponents];
    int32  m_SamplingFactor[xJPEG_Constants::c_MaxComponents];
    int32  m_QuantTableId  [xJPEG_Constants::c_MaxComponents];

  public:
    int32  Absorb   (xByteBuffer* Input );
    int32  Emit     (xByteBuffer* Output) const; 
    void   Init     (int32 Type, int32 Height, int32 Width, int32 BitDepth, eCrF ChromaFormat, int32 NumQuantTables);
    bool   Validate () const;
    int32  getLength() const { return 8 + m_NumComponents * 3; }


    eCrF   DetermineChromaFormat() const;
    int8   GetSamplingFactorH(int32 Idx) const { return ((m_SamplingFactor[Idx] & 0xF0) >> 4); }
    int8   GetSamplingFactorV(int32 Idx) const { return ( m_SamplingFactor[Idx] & 0x0F);       }
           
  public:
    void  setType          (int32 Type         )       { m_Type = Type; }
    int32 getType          (                   ) const { return m_Type; }
    void  setWidth         (int32 Width        )       { m_Width = Width; }
    int32 getWidth         (                   ) const { return m_Width; }
    void  setHeight        (int32 Height       )       { m_Height = Height; }
    int32 getHeight        (                   ) const { return m_Height; }
    void  setBitDepth      (int32 BitDepth     )       { m_BitDepth = BitDepth; }
    int32 getBitDepth      (                   ) const { return m_BitDepth; }
    void  setNumComponents (int32 NumComponents)       { m_NumComponents = NumComponents; }
    int32 getNumComponents (                   ) const { return m_NumComponents; }
    void  setCmpId         (eCmp CmpId,           eCmp  Cmp)       { m_CmpId[(int32)Cmp] = CmpId; }
    eCmp  getCmpId         (                      eCmp  Cmp) const { return m_CmpId[(int32)Cmp]; }
    void  setSamplingFactor(int32 SamplingFactor, eCmp  Cmp)       { m_SamplingFactor[(int32)Cmp] = SamplingFactor; }
    int32 getSamplingFactor(                      eCmp  Cmp) const { return m_SamplingFactor[(int32)Cmp]; }
    void  setQuantTableId  (int32 QuantTableId,   eCmp  Cmp)       { m_QuantTableId[(int32)Cmp] = QuantTableId; }
    int32 getQuantTableId  (                      eCmp  Cmp) const { return m_QuantTableId[(int32)Cmp]; }

    static bool isSOF(eMarker Marker)
    {
      return (((int32)Marker >= (int32)eMarker::SOF0  && (int32)Marker <= (int32)eMarker::SOF3 ) ||
              ((int32)Marker >= (int32)eMarker::SOF5  && (int32)Marker <= (int32)eMarker::SOF7 ) ||
              ((int32)Marker >= (int32)eMarker::SOF9  && (int32)Marker <= (int32)eMarker::SOF11) ||
              ((int32)Marker >= (int32)eMarker::SOF13 && (int32)Marker <= (int32)eMarker::SOF15));
    }

    bool isBaseline() const { return m_Type == (int32)eMarker::SOF0                                  ; }
    bool isExtended() const { return m_Type == (int32)eMarker::SOF1 || m_Type == (int32)eMarker::SOF9; }
  };

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

  class xHuffTable
  {
  public:
    using tCodeL = std::array<byte, xJPEG_Constants::c_NumCodeLenghts>;
    using tClass = eEntropyTableClass;

  protected:
    uint8  m_Idx         = 0              ; //Th
    tClass m_Class       = tClass::Invalid; //Tc
    tCodeL m_CodeLengths = { 0 }          ; //Li
    tByteV m_CodeSymbols                  ; //Vij

  public:
    int32 Absorb     (xByteBuffer* Input );
    int32 Emit       (xByteBuffer* Output) const; 
    void  InitDefault(uint8 Idx, tClass Class, eCmp Cmp);
    void  InitCustom (uint8 Idx, tClass Class, const uint8* LengthTable);
    bool  Validate   () const;
    tStr  Format     (const tStr& Prefix = "  ") const;
    int32 getLength  () const { return 1 + xJPEG_Constants::c_NumCodeLenghts + (int32)m_CodeSymbols.size(); }

  public:
    int32  getIdx  (         ) const { return m_Idx;   }
    void   setIdx  (int32 Idx)       { m_Idx = (uint8)Idx;   }
    tClass getClass(         ) const { return m_Class; }
    void   setClass(tClass HC)       { m_Class = HC; }
    
    const tCodeL& getCodeLengths() const { return m_CodeLengths; }
    const tByteV& getCodeSymbols() const { return m_CodeSymbols; }

    bool isDC() const { return (m_Class == tClass::DC);}
    bool isAC() const { return (m_Class == tClass::AC);}

    int32        getMaxNumCodeSymbols() const { return isDC() ? xJPEG_Constants::c_MaxNumCodeSymbolsDC : isAC() ? xJPEG_Constants::c_MaxNumCodeSymbolsAC : NOT_VALID; }
    static int32 getMaxNumCodeSymbols(tClass Class) { return Class == tClass::DC ? xJPEG_Constants::c_MaxNumCodeSymbolsDC : Class == tClass::AC ? xJPEG_Constants::c_MaxNumCodeSymbolsAC : NOT_VALID; }

    static std::vector<xHuffTable> createDefaultHuffTables();
  };

  using tHuffTablesV = std::vector<xHuffTable>;

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

  class xSOS
  {
  protected:
    int32  m_NumComponents;
    eCmp   m_CmpId    [xJPEG_Constants::c_MaxComponents];
    int32  m_EntropyId[xJPEG_Constants::c_MaxComponents];
    int32  m_SpectralSelectionStart;
    int32  m_SpectralSelectionEnd;
    int32  m_SuccessiveApproximation;

  public:
    int32  Absorb(xByteBuffer* Input );
    int32  Emit  (xByteBuffer* Output)  const;
    void   InitBaseline(int32 NumComponents, int32 EntropyIdL, int32 EntropyIdC);
    void   InitExtended(int32 NumComponents, int32V4 EntropyId);
    bool   Validate () const;
    int32  getLength() const { return 6 + m_NumComponents * 2; }

  public:
    void   setNumComponents(int32 NumComponents)       { m_NumComponents = NumComponents; }
    int32  getNumComponents(                   ) const { return m_NumComponents; }
    void   setCmpId        (eCmp CmpId, eCmp  Cmp)       { m_CmpId[(int32)Cmp] = CmpId; }
    eCmp   getCmpId        (            eCmp  Cmp) const { return m_CmpId[(int32)Cmp]; }
    void   setEntropyId    (int32 EntropyIdDC, int32 EntropyIdAC, eCmp Cmp)       { m_EntropyId[(int32)Cmp] = (((EntropyIdDC & 0x03) << 4) + (EntropyIdAC & 0x03)); }
    int32  getEntropyIdDC  (                                      eCmp Cmp) const { return ((m_EntropyId[(int32)Cmp] >> 4) & 0x03); }
    int32  getEntropyIdAC  (                                      eCmp Cmp) const { return ((m_EntropyId[(int32)Cmp]     ) & 0x03); }
  };

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  
public:
  static bool    ReadSOI         (xByteBuffer* Input ) { return (xReadMarker(Input )==eMarker::SOI); }
  static void    WriteSOI        (xByteBuffer* Output) { xWriteMarker(Output, eMarker::SOI); }

  static bool    ReadAPP0        (xByteBuffer* Input ,       xAPP0& APP0);
  static void    WriteAPP0       (xByteBuffer* Output, const xAPP0& APP0);
  static void    WriteDefaultAPP0(xByteBuffer* Output);

  static bool    ReadDQT         (xByteBuffer* Input ,       std::vector<xQuantTable>& QuantTables);
  static void    WriteDQT        (xByteBuffer* Output, const std::vector<xQuantTable>& QuantTables);

  static bool    ReadDRI         (xByteBuffer* Input , int32& RestartInterval);
  static void    WriteDRI        (xByteBuffer* Output, int32  RestartInterval);

  static bool    ReadSOF         (xByteBuffer* Input ,       xSOF& SOF);
  static void    WriteSOF        (xByteBuffer* Output, const xSOF& SOF);
  static void    WriteSOF        (xByteBuffer* Output, int32 Type, int32 Height, int32 Width, int32 BitDepth, eCrF ChromaFormat, int32 NumQuantTables);
  
  static bool    ReadDHT         (xByteBuffer* Input ,       std::vector<xHuffTable>& HuffTables);
  static void    WriteDHT        (xByteBuffer* Output, const std::vector<xHuffTable>& HuffTables);
  static void    WriteDefaultDHT (xByteBuffer* Output);

  static bool    ReadSOS         (xByteBuffer* Input ,       xSOS& SOS);
  static bool    SkipSOS         (xByteBuffer* Input                  );
  static void    WriteSOS        (xByteBuffer* Output, const xSOS& SOS);
  static void    WriteSOS        (xByteBuffer* Output, int32 NumComponents, int32 LumaHuffTabIdx, int32 ChromaHuffTabIdx);
    
  static bool    ReadData        (byte* Payload, int32 Size, xByteBuffer* Input ) { return xReadMemmory (Payload, Size, Input ); }
  static void    WriteData       (xByteBuffer* Output, xByteBuffer* Payload     ) { xWriteBuffer (Output, Payload      ); }
  static void    WriteData       (xByteBuffer* Output, byte* Payload, int32 Size) { xWriteMemmory(Output, Payload, Size); }

  static int8    ReadRST         (xByteBuffer* Input);
  static void    WriteRST        (xByteBuffer* Output, uint8 RestartIdx);
  
  static bool    ReadEOI         (xByteBuffer* Input ) { return (xReadMarker(Input )==eMarker::EOI); }
  static void    WriteEOI        (xByteBuffer* Output) { xWriteMarker(Output, eMarker::EOI); }

public:
  static eMarker IdentifyMarker  (xByteBuffer* Input ) { return xPeekMarker(Input ); }
  static int32   FindMarker      (xByteBuffer* Input , eMarker Marker);
  static tPosV   SeekAllMarkers  (xStream* File, eMarker Marker, int32 SeekBufferSize = c_SeekBufferSize);

public: 
  static void    AddStuffing     (xByteBuffer* Output, xByteBuffer* Input) { xMarkerUtils::AddStuffing   (Output, Input); }
  static void    RemoveStuffing  (xByteBuffer* Output, xByteBuffer* Input) { xMarkerUtils::RemoveStuffing(Output, Input); }
      
protected:
  static uint8   xPeek8          (xByteBuffer* Input ) { return Input ->peekU8    ();  }
  static uint16  xPeek16         (xByteBuffer* Input ) { return Input ->peekU16_BE(); }
  static eMarker xPeekMarker     (xByteBuffer* Input ) { uint16 D = xPeek16(Input ); if((D>>8)==0xFF) { uint8 M = D & 0x00FF; if(M >= 0xC0) { return eMarker(M); } } return eMarker::ERR; }
                                
  static uint8   xRead8          (xByteBuffer* Input ) { return Input ->extractU8    ();  }
  static uint16  xRead16         (xByteBuffer* Input ) { return Input ->extractU16_BE(); }
  static uint32  xReadFourCC     (xByteBuffer* Input ) { return Input ->extractU32_LE(); }
  static bool    xReadMemmory    (byte* DstPtr, int32 Size,  xByteBuffer* Input) { return Input ->extractBytes(DstPtr, Size); }
  static bool    xReadVector     (tByteV& DstVec, xByteBuffer* Input) { return Input ->extractBytes(DstVec.data(), (int32)DstVec.size()); }
  static eMarker xReadMarker     (xByteBuffer* Input ) { uint8 FF = xRead8(Input ); if(FF==0xFF) { uint8 M = xRead8(Input ); if(M >= 0xC0) { return eMarker(M); } } return eMarker::ERR; }

  static void    xSkip           (xByteBuffer* Input , int32 Size) { Input ->modifyRead(Size); }

  static void    xDispose8       (xByteBuffer* Output, uint8  Val) { Output->disposeU8    (Val); }
  static void    xDispose16      (xByteBuffer* Output, uint16 Val) { Output->disposeU16_BE(Val); }
  static void    xDisposeMarker  (xByteBuffer* Output, eMarker Marker) { xDispose16(Output, (uint16)0xFF00 | (uint16)Marker); }
                         
  static void    xWrite8         (xByteBuffer* Output, uint8  Val) { Output->appendU8    (Val); }
  static void    xWrite16        (xByteBuffer* Output, uint16 Val) { Output->appendU16_BE(Val); }
  static void    xWriteFourCC    (xByteBuffer* Output, uint32 Val) { Output->appendU32_LE(Val); }
  static void    xWriteMemmory   (xByteBuffer* Output, const byte* SrcPtr, int32 Size) { Output->appendBytes(SrcPtr, Size); }                
  static void    xWriteBuffer    (xByteBuffer* Output, xByteBuffer* Input ) { Output->append(Input ); }
  static void    xWriteVector    (xByteBuffer* Output, const tByteV& SrcVec) { Output->appendBytes(SrcVec.data(), (int32)SrcVec.size()); }
  static void    xWriteMarker    (xByteBuffer* Output, eMarker Marker) { xWrite8(Output, 0xFF); xWrite8(Output, (uint8)Marker); }

  static bool    xFitsU8 (int32 V) { return V >= (int32)std::numeric_limits<uint8 >::min() && V <= (int32)std::numeric_limits<uint8 >::max(); }
  static bool    xFitsU16(int32 V) { return V >= (int32)std::numeric_limits<uint16>::min() && V <= (int32)std::numeric_limits<uint16>::max(); }
};

//=============================================================================================================================================================================

} //end of namespace PMBB_NAMESPACE::JPEG

