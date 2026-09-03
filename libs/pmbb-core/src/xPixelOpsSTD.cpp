/*
    SPDX-FileCopyrightText: 2019-2023 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#include "xPixelOpsSTD.h"

namespace PMBB_NAMESPACE {

//===============================================================================================================================================================================================================

void xPixelOpsSTD::Cvt(uint16* restrict Dst, const uint8* Src, int32 DstStride, int32 SrcStride, int32 Width, int32 Height)
{
  for(int32 y = 0; y < Height; y++)
  {
    for(int32 x = 0; x < Width; x++) { Dst[x] = Src[x]; }
    Src += SrcStride;
    Dst += DstStride;
  }
}
void xPixelOpsSTD::Cvt(uint8* restrict Dst, const uint16* Src, int32 DstStride, int32 SrcStride, int32 Width, int32 Height)
{
  for(int32 y = 0; y < Height; y++)
  {
    for(int32 x = 0; x < Width; x++) { Dst[x] = (uint8)xClipU8<uint16>(Src[x]); }
    Src += SrcStride;
    Dst += DstStride;
  }
}
void xPixelOpsSTD::UpsampleHV(uint16* restrict Dst, const uint16* Src, int32 DstStride, int32 SrcStride, int32 DstWidth, int32 DstHeight)
{
  uint16* restrict DstL0 = Dst;
  uint16* restrict DstL1 = Dst + DstStride;

  for(int32 y=0; y<DstHeight; y+=2)
  {
    for(int32 x=0; x<DstWidth; x+=2)
    {
      const uint16 S = Src[x>>1];
      DstL0[x  ] = S;
      DstL0[x+1] = S;
      DstL1[x  ] = S;
      DstL1[x+1] = S;
    }
    Src   += SrcStride;
    DstL0 += (DstStride << 1);
    DstL1 += (DstStride << 1);
  }
}
void xPixelOpsSTD::DownsampleHV(uint16* restrict Dst, const uint16* Src, int32 DstStride, int32 SrcStride, int32 DstWidth, int32 DstHeight)
{
  const uint16* SrcL0 = Src;
  const uint16* SrcL1 = Src + SrcStride;

  for(int32 y=0; y<DstHeight; y++)
  {
    for(int32 x=0; x<DstWidth; x++)
    {
      const int32 SrcX = x << 1;
      int32 D = ((int32)SrcL0[SrcX] + (int32)SrcL0[SrcX + 1] + (int32)SrcL1[SrcX] + (int32)SrcL1[SrcX + 1] + 2) >> 2;
      Dst[x] = (uint16)D;
    }
    Dst   += DstStride;
    SrcL0 += (SrcStride << 1);
    SrcL1 += (SrcStride << 1);
  }
}
void xPixelOpsSTD::CvtUpsampleHV(uint16* restrict Dst, const uint8* Src, int32 DstStride, int32 SrcStride, int32 DstWidth, int32 DstHeight)
{
  uint16* restrict DstL0 = Dst;
  uint16* restrict DstL1 = Dst + DstStride;

  for(int32 y=0; y<DstHeight; y+=2)
  {
    for(int32 x=0; x<DstWidth; x+=2)
    {
      uint16 S = Src[x>>1];
      DstL0[x  ] = S;
      DstL0[x+1] = S;
      DstL1[x  ] = S;
      DstL1[x+1] = S;
    }
    Src   += SrcStride;
    DstL0 += (DstStride << 1);
    DstL1 += (DstStride << 1);
  }
}
void xPixelOpsSTD::CvtDownsampleHV(uint8* restrict Dst, const uint16* Src, int32 DstStride, int32 SrcStride, int32 DstWidth, int32 DstHeight)
{
  const uint16* SrcL0 = Src;
  const uint16* SrcL1 = Src + SrcStride;

  for(int32 y=0; y<DstHeight; y++)
  {
    for(int32 x=0; x<DstWidth; x++)
    {
      const int32 SrcX = x << 1;
      int32 D = ((int32)SrcL0[SrcX] + (int32)SrcL0[SrcX + 1] + (int32)SrcL1[SrcX] + (int32)SrcL1[SrcX + 1] + 2) >> 2;
      Dst[x] = (uint8)xClip<int32>(D, 0, 255);
    }
    Dst   += DstStride;
    const int32 SrcStrideMul2 = SrcStride << 1;
    SrcL0 += SrcStrideMul2;
    SrcL1 += SrcStrideMul2;
  }
}
void xPixelOpsSTD::UpsampleH(uint16* restrict Dst, const uint16* Src, int32 DstStride, int32 SrcStride, int32 DstWidth, int32 DstHeight)
{
  for(int32 y=0; y<DstHeight; y++)
  {
    for(int32 x=0; x<DstWidth; x+=2)
    {
      const uint16 S = Src[x>>1];
      Dst[x  ] = S;
      Dst[x+1] = S;
    }
    Src += SrcStride;
    Dst += DstStride;
  }
}
void xPixelOpsSTD::DownsampleH(uint16* restrict Dst, const uint16* Src, int32 DstStride, int32 SrcStride, int32 DstWidth, int32 DstHeight)
{
  for(int32 y=0; y<DstHeight; y++)
  {
    for(int32 x=0; x<DstWidth; x++)
    {
      const int32 SrcX = x << 1;
      int32 D = ((int32)Src[SrcX] + (int32)Src[SrcX + 1] + 1) >> 1;
      Dst[x] = (uint16)D;
    }
    Dst += DstStride;
    Src += SrcStride;
  }
}
void xPixelOpsSTD::CvtUpsampleH(uint16* restrict Dst, const uint8* Src, int32 DstStride, int32 SrcStride, int32 DstWidth, int32 DstHeight)
{
  for(int32 y=0; y<DstHeight; y++)
  {
    for(int32 x=0; x<DstWidth; x+=2)
    {
      uint16 S = Src[x>>1];
      Dst[x  ] = S;
      Dst[x+1] = S;
    }
    Src += SrcStride;
    Dst += DstStride;
  }
}
void xPixelOpsSTD::CvtDownsampleH(uint8* restrict Dst, const uint16* Src, int32 DstStride, int32 SrcStride, int32 DstWidth, int32 DstHeight)
{
  for(int32 y=0; y<DstHeight; y++)
  {
    for(int32 x=0; x<DstWidth; x++)
    {
      const int32 SrcX = x << 1;
      int32 D = ((int32)Src[SrcX] + (int32)Src[SrcX + 1] + 1) >> 1;
      Dst[x] = (uint8)xClip<int32>(D, 0, 255);
    }
    Dst += DstStride;
    Src += SrcStride;
  }
}
bool xPixelOpsSTD::CheckIfInRange(const uint16* Src, int32 Stride, int32 Width, int32 Height, int32 BitDepth)
{
  if(BitDepth == 16) { return true; }

  const int32 MaxValue = xBitDepth2MaxValue(BitDepth);
  for(int32 y = 0; y < Height; y++)
  {
    for(int32 x = 0; x < Width; x++) { if(Src[x] > MaxValue) { return false; } }
    Src += Stride;
  }
  return true;
}
xPixelOpsSTD::tStr xPixelOpsSTD::FindOutOfRange(const uint16* Src, int32 Stride, int32 Width, int32 Height, int32 BitDepth, int32 MsgNumLimit)
{
  const int32 MaxValue = xBitDepth2MaxValue(BitDepth);
  tStr  Message  = "";
  int32 NumFound = 0;
  for(int32 y = 0; y < Height; y++)
  {
    for(int32 x = 0; x < Width; x++)
    {
      if(Src[x] > MaxValue)
      { 
        Message += fmt::format("DETECTED out-of-range value (y={:d}, x={:d}, VALUE={:d}, Expected=[0-{:d}])\n", y, x, Src[x], MaxValue);
        if(MsgNumLimit>0 && NumFound++ >= MsgNumLimit)
        { 
          Message += fmt::format("DETECTED number of out-of-range values exceeds limit (MsgNumLimit={:d})\n", MsgNumLimit);
          break;
        }
      }
    }
    Src += Stride;
  }
  return Message;
}
void xPixelOpsSTD::ClipToRange(uint16* restrict Ptr, int32 SrcStride, int32 Width, int32 Height, int32 BitDepth)
{
  const uint16 MaxValue = (uint16)xBitDepth2MaxValue(BitDepth);
  for(int32 y = 0; y < Height; y++)
  {
    for(int32 x = 0; x < Width; x++)
    {
      if(Ptr[x] > MaxValue) { Ptr[x] = MaxValue; }
    }
    Ptr += SrcStride;
  }
}
void xPixelOpsSTD::ExtendMargin(uint16* Addr, int32 Stride, int32 Width, int32 Height, int32 Margin)
{
  //left/right
  for(int32 y = 0; y < Height; y++)
  {
    uint16 Left  = Addr[0];
    uint16 Right = Addr[Width - 1];
    for(int32 x = 0; x < Margin; x++)
    {
      Addr[x - Margin] = Left;
      Addr[x + Width ] = Right;
    }
    Addr += Stride;
  }
  //below
  Addr -= (Stride + Margin);
  for(int32 y = 0; y < Margin; y++)
  {
    ::memcpy(Addr + (y + 1) * Stride, Addr, sizeof(uint16) * (Width + (Margin << 1)));
  }
  //above
  Addr -= ((Height - 1) * Stride);
  for(int32 y = 0; y < Margin; y++)
  {
    ::memcpy(Addr - (y + 1) * Stride, Addr, sizeof(uint16) * (Width + (Margin << 1)));
  }
}




// -------------------------------------------------------------------------------------------------------------------------------------
// RDOQ-RGB: decimation and interpolation FIR
// -------------------------------------------------------------------------------------------------------------------------------------

void xPixelOpsSTD::DownsampleH_FIR(uint16* restrict Dst, const uint16* Src, int32 DstStride, int32 SrcStride, int32 DstWidth, int32 DstHeight, int32 BitDepth)
{
    const int32 MaxValue = xBitDepth2MaxValue(BitDepth);

    for (int32 y = 0; y < DstHeight; y++)
    {
        for (int32 x = 0; x < DstWidth; x++)
        {
            const int32 SrcX = x << 1;

            const int32 Sum =
                5 * (int32)Src[SrcX - 5]
                + 11 * (int32)Src[SrcX - 4]
                - 21 * (int32)Src[SrcX - 3]
                - 37 * (int32)Src[SrcX - 2]
                + 70 * (int32)Src[SrcX - 1]
                + 228 * (int32)Src[SrcX]
                + 228 * (int32)Src[SrcX + 1]
                + 70 * (int32)Src[SrcX + 2]
                - 37 * (int32)Src[SrcX + 3]
                - 21 * (int32)Src[SrcX + 4]
                + 11 * (int32)Src[SrcX + 5]
                + 5 * (int32)Src[SrcX + 6];

            const int32 D = (Sum + 256) >> 9;

            Dst[x] = (uint16)xClip<int32>(D, 0, MaxValue);
        }

        Src += SrcStride;
        Dst += DstStride;
    }
}


void xPixelOpsSTD::DownsampleV_FIR(uint16* restrict Dst, const uint16* Src, int32 DstStride, int32 SrcStride, int32 DstWidth, int32 DstHeight, int32 BitDepth)
{
    const int32 MaxValue = xBitDepth2MaxValue(BitDepth);

    for (int32 y = 0; y < DstHeight; y++)
    {
        const int32 SrcY = y << 1;

        const uint16* SrcRow = Src + SrcY * SrcStride;
        uint16* DstRow = Dst + y * DstStride;

        for (int32 x = 0; x < DstWidth; x++)
        {
            const int32 Sum =
                5 * (int32)SrcRow[x - 5 * SrcStride]
                + 11 * (int32)SrcRow[x - 4 * SrcStride]
                - 21 * (int32)SrcRow[x - 3 * SrcStride]
                - 37 * (int32)SrcRow[x - 2 * SrcStride]
                + 70 * (int32)SrcRow[x - 1 * SrcStride]
                + 228 * (int32)SrcRow[x]
                + 228 * (int32)SrcRow[x + 1 * SrcStride]
                + 70 * (int32)SrcRow[x + 2 * SrcStride]
                - 37 * (int32)SrcRow[x + 3 * SrcStride]
                - 21 * (int32)SrcRow[x + 4 * SrcStride]
                + 11 * (int32)SrcRow[x + 5 * SrcStride]
                + 5 * (int32)SrcRow[x + 6 * SrcStride];

            const int32 D = (Sum + 256) >> 9;

            DstRow[x] = (uint16)xClip<int32>(D, 0, MaxValue);
        }
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


void xPixelOpsSTD::UpsampleH_FIR(uint16* restrict Dst, const uint16* Src, int32 DstStride, int32 SrcStride, int32 DstWidth, int32 DstHeight, int32 BitDepth)
{
    const int32 MaxValue = xBitDepth2MaxValue(BitDepth);
    const int32 SrcWidth = DstWidth >> 1;

    for (int32 y = 0; y < DstHeight; y++)
    {
        for (int32 x = 0; x < SrcWidth; x++)
        {
            const int32 DstX = x << 1;

            const int32 LeftSum =
                5 * (int32)Src[x - 3]
                - 21 * (int32)Src[x - 2]
                + 70 * (int32)Src[x - 1]
                + 228 * (int32)Src[x]
                - 37 * (int32)Src[x + 1]
                + 11 * (int32)Src[x + 2];

            const int32 RightSum =
                5 * (int32)Src[x + 3]
                - 21 * (int32)Src[x + 2]
                + 70 * (int32)Src[x + 1]
                + 228 * (int32)Src[x]
                - 37 * (int32)Src[x - 1]
                + 11 * (int32)Src[x - 2];

            const int32 LeftPixel = (LeftSum + 128) >> 8;
            const int32 RightPixel = (RightSum + 128) >> 8;

            Dst[DstX] = (uint16)xClip<int32>(LeftPixel, 0, MaxValue);
            Dst[DstX + 1] = (uint16)xClip<int32>(RightPixel, 0, MaxValue);
        }

        Src += SrcStride;
        Dst += DstStride;
    }
}

void xPixelOpsSTD::UpsampleV_FIR(uint16* restrict Dst, const uint16* Src, int32 DstStride, int32 SrcStride, int32 DstWidth, int32 DstHeight, int32 BitDepth)
{
    const int32 MaxValue = xBitDepth2MaxValue(BitDepth);
    const int32 SrcHeight = DstHeight >> 1;

    for (int32 y = 0; y < SrcHeight; y++)
    {
        const int32 DstY = y << 1;

        const uint16* SrcRow = Src + y * SrcStride;
        uint16* DstTopRow = Dst + DstY * DstStride;
        uint16* DstBottomRow = Dst + (DstY + 1) * DstStride;

        for (int32 x = 0; x < DstWidth; x++)
        {
            const int32 TopSum =
                3 * (int32)SrcRow[x - 3 * SrcStride]
                - 16 * (int32)SrcRow[x - 2 * SrcStride]
                + 67 * (int32)SrcRow[x - 1 * SrcStride]
                + 227 * (int32)SrcRow[x]
                - 32 * (int32)SrcRow[x + 1 * SrcStride]
                + 7 * (int32)SrcRow[x + 2 * SrcStride];

            const int32 BottomSum =
                3 * (int32)SrcRow[x + 3 * SrcStride]
                - 16 * (int32)SrcRow[x + 2 * SrcStride]
                + 67 * (int32)SrcRow[x + 1 * SrcStride]
                + 227 * (int32)SrcRow[x]
                - 32 * (int32)SrcRow[x - 1 * SrcStride]
                + 7 * (int32)SrcRow[x - 2 * SrcStride];

            const int32 TopPixel = (TopSum + 128) >> 8;
            const int32 BottomPixel = (BottomSum + 128) >> 8;

            DstTopRow[x] = (uint16)xClip<int32>(TopPixel, 0, MaxValue);

            DstBottomRow[x] = (uint16)xClip<int32>(BottomPixel, 0, MaxValue);
        }
    }
}


// -------------------------------------------------------------------------------------------------------------------------------------








void xPixelOpsSTD::AOS4fromSOA3(uint16* restrict DstABCD, const uint16* SrcA, const uint16* SrcB, const uint16* SrcC, uint16 ValueD, int32 DstStride, int32 SrcStride, int32 Width, int32 Height)
{
  for(int32 y=0; y<Height; y++)
  {
    for(int32 x=0; x<Width; x++)
    {
      DstABCD[(x<<2)+0] = SrcA[x];
      DstABCD[(x<<2)+1] = SrcB[x];
      DstABCD[(x<<2)+2] = SrcC[x];
      DstABCD[(x<<2)+3] = ValueD;
    }
    SrcA    += SrcStride;
    SrcB    += SrcStride;
    SrcC    += SrcStride;
    DstABCD += DstStride;
  }
}
void xPixelOpsSTD::SOA3fromAOS4(uint16* restrict DstA, uint16* restrict DstB, uint16* restrict DstC, const uint16* SrcABCD, int32 DstStride, int32 SrcStride, int32 Width, int32 Height)
{
  for(int32 y = 0; y < Height; y++)
  {
    for(int32 x = 0; x < Width; x++)
    {
      const uint16 a = SrcABCD[(x << 2) + 0];
      const uint16 b = SrcABCD[(x << 2) + 1];
      const uint16 c = SrcABCD[(x << 2) + 2];
      DstA[x] = a;
      DstB[x] = b;
      DstC[x] = c;
    }
    SrcABCD += SrcStride;
    DstA    += DstStride;
    DstB    += DstStride;
    DstC    += DstStride;
  }
}
int32 xPixelOpsSTD::CountNonZero(const uint16* Src, int32 SrcStride, int32 Width, int32 Height)
{
  int32 NumNonZero = 0;

  for(int32 y = 0; y < Height; y++)
  {
    for(int32 x = 0; x < Width; x++) { if(Src[x] != 0) { NumNonZero++; } }
    Src += SrcStride;
  }

  return NumNonZero;
}
uint64 xPixelOpsSTD::CountEqual(const uint16* Tst, const uint16* Ref, int32 TstStride, int32 RefStride, int32 Width, int32 Height)
{
  uint64 NumEqual = 0;
  for(int32 y = 0; y < Height; y++)
  {
    for(int32 x = 0; x < Width; x++) { if(Tst[x] == Ref[x]) { NumEqual++; } }
    Tst += TstStride;
    Ref += RefStride;
  }
  return NumEqual;
}
uint64 xPixelOpsSTD::CountEqualMask(const uint16* Tst, const uint16* Ref, const uint16* Msk, int32 TstStride, int32 RefStride, int32 MskStride, int32 Width, int32 Height)
{
  uint64 NumEqual = 0;
  for(int32 y = 0; y < Height; y++)
  {
    for(int32 x = 0; x < Width; x++) { if(Msk[x] && Tst[x] == Ref[x]) { NumEqual++; } }
    Tst += TstStride;
    Ref += RefStride;
    Msk += MskStride;
  }
  return NumEqual;
}
bool xPixelOpsSTD::CompareEqual(const uint16* Tst, const uint16* Ref, int32 TstStride, int32 RefStride, int32 Width, int32 Height)
{
  for(int32 y = 0; y < Height; y++)
  {
    for(int32 x = 0; x < Width; x++) { if(Tst[x] != Ref[x]) { return false; } }
    Tst += TstStride;
    Ref += RefStride;
  }

  return true;
}
xPixelOpsSTD::tStr xPixelOpsSTD::FindDiscrepancy(const uint16* Tst, const uint16* Ref, int32 TstStride, int32 RefStride, int32 Width, int32 Height, int32 MsgNumLimit)
{
  tStr  Message  = "";
  int32 NumFound = 0;
  for(int32 y = 0; y < Height; y++)
  {
    for(int32 x = 0; x < Width; x++)
    {
      if(Tst[x] != Ref[x])
      { 
        Message += fmt::format("DETECTED out-of-range value (y={:d}, x={:d}, VALUE Tst={:d} Ref={:d})\n", y, x, Tst[x], Ref[x]);
        if(MsgNumLimit>0 && NumFound++ >= MsgNumLimit)
        { 
          Message += fmt::format("DETECTED number of out-of-range values exceeds limit (MsgNumLimit={:d})\n", MsgNumLimit);
          break;
        }
      }
    }
    Tst += TstStride;
    Ref += RefStride;
  }
  return Message;
}

//===============================================================================================================================================================================================================

} //end of namespace PMBB
