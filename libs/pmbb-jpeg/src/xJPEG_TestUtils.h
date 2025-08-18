/*
    SPDX-FileCopyrightText: 2024-2025 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include "xCommonDefJPEG.h"

namespace PMBB_NAMESPACE::JPEG {

//=====================================================================================================================================================================================

uint32 fillRandomTransformCoeffsBlock(int16* Dst, uint32 State);

uint32 fillRandomQuantizedTransformCoeffsBlock(int16* Dst, uint32 State);

//=====================================================================================================================================================================================

} //end of namespace PMBB::JPEG