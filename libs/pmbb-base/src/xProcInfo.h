/*
    SPDX-FileCopyrightText: 2019-2025 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include "xCommonDefBASE.h"
#include <vector>
#include <array>

namespace PMBB_BASE {

//=============================================================================================================================================================================
// xProcInfo
//=============================================================================================================================================================================

class xProcInfo
{
public:
  using tStr = std::string;

  enum class eFeat
  {
    //-----------------------------------------------------------------
#if defined(X_PMBB_ARCH_AMD64) 
    //Intel
    //x87
    FPU               , //Floating Point Unit (integrated since 80486)
    //486 - 1989
    CMPXCHG8B         ,
    //P55C - 1993                 
    MMX               ,
    CMOV              , //Conditional move
    PSE               , //Page Size Extension (PSE)
    TSC               , //Time Stamp Counter (TSC) 
    //P6 - 1995
    PAE               , //Physical Address Extension (PAE)
    //Klamath - 1997
    SEP               , //SYSENTER and SYSEXIT instructions
    //Deschutes              
    PSE36             , //36-bit Page Size Extension
    //Katmai - 1999
    SSE1              ,
    FXRS              , //FXSAVE, FXRESTOR instructions
    //Willamette - 2000
    SSE2              ,
    CLFLUSH           ,
    //Northwood              
    HT                , //Hyperthreading
    //Prescott - 2004
    SSE3              ,
    CMPXCHG16B        ,
    //Merom - 2006
    SSSE3             , //SupplementalSSE3
    //Clovertown - 2006
    LAHF_SAHF         , //Intel VT
    //Penryn - 2007
    SSE4_1            ,
    //Nehalem - 2008
    SSE4_2            ,
    POPCNT            ,
    XSAVE             ,
    OSXSAVE           ,
    //Westmere               
    AES               , //EAS encryption accelerator 
    CLMUL             , //Carry-less Multiplication
    //Sandy Bridge           
    AVX1              ,
    //Ivy Bridge             
    FP16C             , //Half float convertion
    RDRAND            , //SP 800-90A - Cryptographically secure pseudorandom number generator
    FSGSBASE          , //Allows applications to directly write to the FS and GS segment registers
    //Haswell                
    AVX2              ,
    LZCNT             , //Leading zero count instruction
    MOVBE             , //Load and Store of Big Endian forms
    ABM               , //Advanced Bit Manipulation (LZCNT + POPCNT)
    BMI1              , //Bit Manipulation Instructions 1
    BMI2              , //Bit Manipulation Instructions 2
    FMA3              , //Fused multiple-add
    RTM               , //Restricted Transactional Memory
    HLE               , //Hardware Lock Elision
    TSX               , //TSX=RTM+HLE Transactional Synchronization Extensions
    INVPCID           , //Invalidate processor context ID 
    //Broadwell
    ADX               , //Multi-Precision Add-Carry Instruction Extensions (ADOX, ADCX, MULX)
    RDSEED            , //SP 800-90B & C - Non-deterministic random bit generator
    PREFETCHW         , 
    //Skylake
    MPX               , //Memory Protection Extensions  
    SGX               , //Software Guard Extensions
    SHA               , //Intel SHA Extensions
    CLFLUSHOPT        ,
    //Skylake-X                
    AVX512_F          , //Foundation  
    AVX512_VL         , //Vector Length Extensions
    AVX512_BW         , //Byte and Word Instructions
    AVX512_DQ         , //Doubleword and Quadword Instructions
    AVX512_CD         , //Conflict Detection Instructions
    PKU               , //Memory Protection Keys for User-mode pages
    //Knights Landing
    AVX512_ER         , //Exponential and Reciprocal Instructions
    AVX512_PF         , //Prefetch Instructions    
    //Cannonlake
    UMIP              , //User-Mode Instruction Prevention
    AVX512_VBMI       , //Vector Byte Manipulation Instructions
    AVX512_IFMA       , //Integer Fused Multiply Add (52-bit Integer Multiply-Add)
    //Knights Mill            
    AVX512_4VNNIW     , //4-register Neural Network Instructions / Vector Neural Network Instructions Word variable precision
    AVX512_4FMAPS     , //4-register Multiply Accumulation Single precision
    //Sunny Cove (Ice Lake)
    CLWB               , //Cache Line Write Back
    RDPID              , //Read Processor ID
    AVX512_VNNI        , //Vector Neural Network Instructions
    AVX512_VBMI2       , //Vector Byte Manipulation Instructions 2
    AVX512_BITALG      , //Support for VPOPCNT[B,W] and VPSHUF-BITQMB
    AVX512_VPOPCNTDQ   , //Vector POPCNT
    AVX512_VP2INTERSECT,
    VPCLMULQDQ         , //Carry-Less Multiplication Quadword
    VAES               , //Vector AES
    GFNI               , //Galois Field New Instructions
    //Willow Cove (Tiger Lake)    
    AVX_IFMA           ,
    MOVDIRI            ,
    MOVDIR64B          ,
    //Willow Cove (Sapphire Rapids) 
    AVX512_BF16        ,
    AVX512_FP16        ,
    AMX_BF16           ,
    AMX_TILE           ,
    AMX_INT8           ,
    //Golden Cove (Alder Lake)
    AVX_VNNI           ,
    HYBRID             ,
    //Crestmont (Sierra Forest)
    AVX_VNNI_INT8      ,
    PREFETCHI          ,
    //Lion Cove/Skymont (Arrow Lake/Lunar Lake)
    AVX_NE_CONVERT     ,
    AVX_VNNI_INT16     ,
    //???
    AVX10              ,
    AVX10_128          ,
    AVX10_256          ,
    AVX10_512          ,
   
    //AMD
    //Chompers
    MMX_3DNow         ,
    //Thunderbird          
    MMX_3DNowExt      ,
    //Barcelona            
    SSE4_A            ,
    //Bulldozer            
    SSE_XOP           ,
    FMA4              ,
    //Piledriver           
    TBM               ,
    //Excavator
    MONITORX          ,
    //Zen1
    CLZERO            ,
    //Zen2
    WBNOINVD          , //Write Back and Do Not Invalidate Cache
    //Zen6
    AVX512_BMM        , 

    //ECR0
    ECR0_X87          , //x87 FPU/MMX support (must be 1) 
    ECR0_SSE          , //XSAVE support for MXCSR and XMM registers 
    ECR0_AVX          , //AVX enabled and XSAVE support for upper halves of YMM registers 
    ECR0_Opmask       , //AVX-512 enabled and XSAVE support for opmask registers k0-k7 
    ECR0_ZMM_Hi256    , //AVX-512 enabled and XSAVE support for upper halves of lower ZMM registers
    ECR0_Hi16_ZMM     , //AVX-512 enabled and XSAVE support for upper ZMM registers 
#endif //X_PMBB_ARCH_AMD64

    NUM_OF_FEATURES
  };

  class xFeats
  {
  public:
#if defined(X_PMBB_ARCH_AMD64) 
    static constexpr std::array c_FeaturesAMD64v1 = { eFeat::CMOV, eFeat::CMPXCHG8B, eFeat::FPU, eFeat::FXRS, eFeat::MMX, eFeat::SSE1, eFeat::SSE2 };
    static constexpr std::array c_FeatDiffAMD64v2 = { eFeat::CMPXCHG16B, eFeat::LAHF_SAHF, eFeat::POPCNT, eFeat::SSE3, eFeat::SSSE3, eFeat::SSE4_1, eFeat::SSE4_2 };
    static constexpr std::array c_FeatDiffAMD64v3 = { eFeat::AVX1, eFeat::AVX2, eFeat::BMI1, eFeat::BMI2, eFeat::FP16C, eFeat::FMA3, eFeat::LZCNT, eFeat::MOVBE };
    static constexpr std::array c_FeatDiffAMD64v4 = { eFeat::AVX512_F, eFeat::AVX512_BW, eFeat::AVX512_CD, eFeat::AVX512_DQ, eFeat::AVX512_VL };
#endif //X_PMBB_ARCH_AMD64
    
  protected:
    std::vector<bool> m_Exts = std::vector<bool>((int32_t)(eFeat::NUM_OF_FEATURES), false);

  public:
    inline void set(eFeat Feature, bool Val)       { m_Exts[(int32_t)Feature] = m_Exts[(int32_t)Feature] || Val; }
    inline bool has(eFeat Feature          ) const { return m_Exts[(int32_t)Feature]; }
    
    bool hasFeats(const std::vector<eFeat>& Features) const;

#if defined(X_PMBB_ARCH_AMD64) 
    inline bool hasSSEx() const { return has(eFeat::SSE1) && has(eFeat::SSE2) && has(eFeat::SSE3) && has(eFeat::SSSE3) && has(eFeat::SSE4_1) && has(eFeat::SSE4_2); }
    inline bool hasAVX1() const { return hasSSEx() && has(eFeat::AVX1); }
    inline bool hasAVX2() const { return hasAVX1() && has(eFeat::AVX2); }
    inline bool hasFMA () const { return hasAVX2() && has(eFeat::FMA3); }

    inline bool matchesAMD64v1() const { return hasFeats({ c_FeaturesAMD64v1.cbegin(), c_FeaturesAMD64v1.cend() }); }
    inline bool matchesAMD64v2() const { return matchesAMD64v1() && hasFeats({ c_FeatDiffAMD64v2.cbegin(), c_FeatDiffAMD64v2.cend() }); }
    inline bool matchesAMD64v3() const { return matchesAMD64v2() && hasFeats({ c_FeatDiffAMD64v3.cbegin(), c_FeatDiffAMD64v3.cend() }); }
    inline bool matchesAMD64v4() const { return matchesAMD64v3() && hasFeats({ c_FeatDiffAMD64v4.cbegin(), c_FeatDiffAMD64v4.cend() }); }
#endif //X_PMBB_ARCH_AMD64

    static std::string eFeatToName(eFeat Ext);
  };

public:
  enum class eMFL : int32
  {
    INVALID   = NOT_VALID,
    UNDEFINED = 0,
#if defined(X_PMBB_ARCH_AMD64) 
    AMD64v1  , //x86-64    : CMOV, CMPXCHG8B, FPU, FXSR, MMX, FXSR, SCE, SSE, SSE2
    AMD64v2  , //x86-64-v2 : x86-64 + CMPXCHG16B, LAHF-SAHF, POPCNT, SSE3, SSE4.1, SSE4.2, SSSE3
    AMD64v3  , //x86-64-v3 : x86-64-v2 + AVX, AVX2, BMI1, BMI2, F16C, FMA, LZCNT, MOVBE, XSAVE
    AMD64v4  , //x86-64-v4 : x86-64-v3 + AVX512F, AVX512BW, AVX512CD, AVX512DQ, AVX512VL
#endif //X_PMBB_ARCH_ARM64
  };

  using tMFLV = std::vector<eMFL>;

protected:
  bool   m_ProcInfoChecked    = false;
  tStr   m_ManufacturerID;
  tStr   m_BrandString   ;
#if defined(X_PMBB_ARCH_AMD64) 
  int32  m_Family  ;
  int32  m_Model   ;
  int32  m_Stepping;
#endif
  xFeats m_Feats         ;
  bool   m_OSAVX         = false; //OS level AVX support OSXSAVE
  flt64  m_TSC_Frequency = std::numeric_limits<flt64>::quiet_NaN(); //Hz

public:
  void  detectSysInfo();
  tStr  formatSysInfo() const;
  eMFL  determineMicroArchFeatureLevel () const;
  tMFLV determineMicroArchFeatureLevels() const;

public:
  const xFeats& getExts() const { return m_Feats; }

#if defined(X_PMBB_ARCH_AMD64) 
  inline bool hasSSEx() const { return m_Feats.hasSSEx()           ; }
  inline bool hasAVX1() const { return m_Feats.hasAVX1() && m_OSAVX; }
  inline bool hasAVX2() const { return m_Feats.hasAVX2() && m_OSAVX; }
  inline bool hasFMA () const { return m_Feats.hasFMA () && m_OSAVX; }

  inline bool matchesAMD64v1() const { return m_Feats.matchesAMD64v1()           ; }
  inline bool matchesAMD64v2() const { return m_Feats.matchesAMD64v2()           ; }
  inline bool matchesAMD64v3() const { return m_Feats.matchesAMD64v3() && m_OSAVX; }
  inline bool matchesAMD64v4() const { return m_Feats.matchesAMD64v4() && m_OSAVX; }
#endif //X_PMBB_ARCH_AMD64

  static eMFL xStrToMfl(const std::string_view Mfl);
  static tStr xMflToStr(eMFL Mfl);
  static tStr xMflToDescription(eMFL Mfl);

protected:  
  tStr xFormatProcInfo () const;
  tStr xFormatProcFeats() const;
  static tStr xFormatMemInfo();
  
  void xDetectProcInfo();

#if defined(X_PMBB_ARCH_AMD64)
  void xDetectProcInfoAMD64();
  void xDetectCPUID        ();
  void xDetectMSR0         ();
  void xDetectOSAVX        ();
#endif //X_PMBB_ARCH_AMD64
};

//=============================================================================================================================================================================

} //end of namespace PMBB
