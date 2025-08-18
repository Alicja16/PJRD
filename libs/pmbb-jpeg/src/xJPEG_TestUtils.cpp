/*
    SPDX-FileCopyrightText: 2024-2025 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#include "xJPEG_TestUtils.h"
#include "xTestUtils.h"
#include "xJPEG_Constants.h"

namespace PMBB_NAMESPACE::JPEG {

//=====================================================================================================================================================================================

static constexpr int32 BA = xJPEG_Constants::c_BlockArea;
static constexpr uint8 c_TabCoeff[BA] = { 47,64,60,53,60,69,47,53,57,53,42,44,47,39,29,11,25,29,32,32,29,6,14,13,21,11,3,5,3,3,4,5,4,4,2,1,0,1,2,2,1,2,4,4,1,0,1,1,0,0,0,0,0,3,1,0,0,0,0,0,0,0,0,0 };

uint32 fillRandomTransformCoeffsBlock(int16* Dst, uint32 State)
{
  for(int32 i = 0; i < BA; i++)
  {
    State = xTestUtils::xXorShift32(State);

    int32 Coeff = c_TabCoeff[i];
    int32_t Sign = State & 0x01 ? -1 : 1;
    int32_t Add  = (State & 0xC0 >> 6) >= 3 ? (State & 0x06) >> 1 : 0;
    int32_t Mul  = (State & 0xF00 >> 8) && 0x1;
    int32 CoeffR = (Coeff + Add) * Sign * Mul;
    Dst[i] = (int16)xClipS16(CoeffR);
  }

  return State;
}


uint32 fillRandomQuantizedTransformCoeffsBlock(int16* Dst, uint32 State)
{
  for(int32 i = 0; i < BA; i++)
  {
    State = xTestUtils::xXorShift32(State);

    int32 Coeff = c_TabCoeff[i];
    int32_t Sign = State & 0x01 ? -1 : 1;

    int32_t Add = (State & 0xC0 >> 6) >= 3 ? (State & 0x06) >> 1 : 0;
    int32_t Zero = (State & 0xFF00 >> 8) >= 192 ? 0 : 1;
    int32_t Shft = (State & 0xFF00 >> 8) <= 64 ? 3 : 0;
    if(i == 0) { Sign = 1; Zero = 1; Shft = 0; }
    int32 CoeffR = Zero * Sign * ((Coeff >> Shft) + Add);
    Dst[i] = (int16)xClipS16(CoeffR);
  }

  return State;
}

//=====================================================================================================================================================================================

} //end of namespace PMBB::JPEG