/*
    SPDX-FileCopyrightText: 2019-2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#pragma once

#include "xCommonDefCORE.h"
#include <chrono>

namespace PMBB_NAMESPACE {

//===============================================================================================================================================================================================================
// Time is money
//===============================================================================================================================================================================================================
using tClock      = std::chrono::high_resolution_clock       ;
using tTimePoint  = tClock::time_point                       ;
using tDuration   = tClock::duration                         ;
using tDurationUS = std::chrono::duration<double, std::micro>;
using tDurationMS = std::chrono::duration<double, std::milli>;
using tDurationS  = std::chrono::duration<double            >;

// Time Stamp Counter
#if defined(X_PMBB_ARCH_AMD64)

  #if defined(__SSE4_2__)
    #define X_TSC_CAN_USE_RDTSCP 1
  #else
    #define X_TSC_CAN_USE_RDTSCP 0
  #endif

  #if X_TSC_CAN_USE_RDTSCP && 0
    static inline uint64 xTSC() { uint32 T;  return __rdtscp(&T); }
    #define X_IMPLEMENTATION_TSC "RDTSCP"
  #else
    static inline uint64 xTSC() { return __rdtsc(); }
    #define X_IMPLEMENTATION_TSC "RDTSC"
  #endif

  #if X_TSC_CAN_USE_RDTSCP
    static inline uint64 xExactTSC() { uint32 T; uint64 TSC = __rdtscp(&T); _mm_lfence(); return TSC; }
    #define X_IMPLEMENTATION_TSC_EXACT "RDTSCP+LFENCE"
  #else //X_TSC_CAN_USE_RDTSCP
    static inline uint64 xExactTSC() { _mm_sfence(); uint64 TSC = __rdtsc(); _mm_lfence(); return TSC; }
    #define X_IMPLEMENTATION_TSC_EXACT "SFENCE+RDTSC+LFENCE"
  #endif//X_TSC_CAN_USE_RDTSCP

  #undef X_TSC_CAN_USE_RDTSCP

#else

  static inline uint64 xExactTSC() { return (uint64)(tClock::now().time_since_epoch().count()); }
  static inline uint64 xTSC     () { return (uint64)(tClock::now().time_since_epoch().count()); }
  #define X_IMPLEMENTATION_TSC       "std::chrono::high_resolution_clock"
  #define X_IMPLEMENTATION_TSC_EXACT "std::chrono::high_resolution_clock"

#endif


//===============================================================================================================================================================================================================

class xTimeStamp
{
protected:
  tTimePoint m_ProcBegTime  = tTimePoint::min();
  tTimePoint m_ProcEndTime  = tTimePoint::min();
  uint64     m_ProcBegTicks = 0;
  uint64     m_ProcEndTicks = 0;

  flt64 m_TicksPerSec      = 0.0;
  flt64 m_TicksPerMiliSec  = 0.0;
  flt64 m_TicksPerMicroSec = 0.0;

public:
  void sampleBeg() { m_ProcBegTime = tClock::now(); m_ProcBegTicks = xExactTSC(); }
  void sampleEnd() { m_ProcEndTime = tClock::now(); m_ProcEndTicks = xExactTSC(); }

  void        calibrateTimeStamp();
  void        setupCalibration  (flt64 TicksPerSec);
  std::string formatCalibration () const;

  flt64 getTicksPerMicroSec() const { return m_TicksPerMicroSec; }
  flt64 getTicksPerMiliSec () const { return m_TicksPerMiliSec ; }
  flt64 getTicksPerSec     () const { return m_TicksPerSec     ; }

  static flt64 calibrateTicksPerSec(tDurationMS Miliseconds);
};

//===============================================================================================================================================================================================================

} //end of namespace PMBB