/*
    SPDX-FileCopyrightText: 2019-2025 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "xCommonDefCMPR.h"

using namespace PMBB_NAMESPACE;

//===============================================================================================================================================================================================================

uint32 xRefLZCNT64(uint64 Value)
{
  for(int32 i = 63; i >= 0; i--) { if((Value >> i) & 1) { return 63 - i; } }
  return 64;
}

uint32 xRefLZCNT32(uint32 Value)
{
  for(int32 i = 31; i >= 0; i--) { if((Value >> i) & 1) { return 31 - i; } }
  return 32;
}

uint32 xRefLZCNT16(uint16 Value)
{
  for(int32 i = 15; i >= 0; i--) { if((Value >> i) & 1) { return 15 - i; } }
  return 16;
}

void testLZCNT()
{
  for(uint32 i = 0; i < 65536; i++)
  {
    uint32 Ref64 = xRefLZCNT64((uint64)i);
    uint32 Ref32 = xRefLZCNT32((uint32)i);
    uint32 Ref16 = xRefLZCNT16((uint16)i);
    uint32 Tst64 = (uint32)xLZCNT((uint64)i);
    uint32 Tst32 = (uint32)xLZCNT((uint32)i);
    uint32 Tst16 = (uint32)xLZCNT((uint16)i);
    CHECK(Ref64 == Tst64);
    CHECK(Ref32 == Tst32);
    CHECK(Ref16 == Tst16);
  }

  for(uint32 p = 0; p < 64; p++)
  {
    uint64 i   = (uint64)1 << p;
    uint32 Ref = xRefLZCNT64((uint64)i);
    uint32 Tst = (uint32)xLZCNT((uint64)i);
    CHECK(Ref == Tst);
  }

  for(uint32 p = 0; p < 32; p++)
  {
    uint64 i   = (uint64)1 << p;
    uint32 Ref = xRefLZCNT32((uint32)i);
    uint32 Tst = (uint32)xLZCNT((uint32)i);
    CHECK(Ref == Tst);
  }

  for(uint32 p = 0; p < 16; p++)
  {
    uint64 i   = (uint64)1 << p;
    uint32 Ref = xRefLZCNT16((uint16)i);
    uint32 Tst = (uint32)xLZCNT((uint16)i);
    CHECK(Ref == Tst);
  }
}

//===============================================================================================================================================================================================================

uint32 xRefTZCNT64(uint64 Value)
{
  for(int32 i = 0; i < 64; i++) { if((Value >> i) & 1) { return i; } }
  return 64;
}

uint32 xRefTZCNT32(uint32 Value)
{
  for(int32 i = 0; i < 32; i++) { if((Value >> i) & 1) { return i; } }
  return 32;
}

uint32 xRefTZCNT16(uint16 Value)
{
  for(int32 i = 0; i < 16; i++) { if((Value >> i) & 1) { return i; } }
  return 16;
}

void testTZCNT()
{
  for(uint32 i = 0; i < 65536; i++)
  {
    uint32 Ref64 = xRefTZCNT64((uint64)i);
    uint32 Ref32 = xRefTZCNT32((uint32)i);
  //uint32 Ref16 = xRefTZCNT16((uint16)i);
    uint32 Tst64 = (uint32)xTZCNT((uint64)i);
    uint32 Tst32 = (uint32)xTZCNT((uint32)i);
  //uint32 Tst16 = (uint32)xTZCNT((uint16)i);
    CHECK(Ref64 == Tst64);
    CHECK(Ref32 == Tst32);
  //CHECK(Ref16 == Tst16);
  }

  for(uint32 p = 0; p < 64; p++)
  {
    uint64 i   = (uint64)1 << p;
    uint32 Ref = xRefTZCNT64((uint64)i);
    uint32 Tst = (uint32)xTZCNT((uint64)i);
    CHECK(Ref == Tst);
  }

  for(uint32 p = 0; p < 32; p++)
  {
    uint64 i   = (uint64)1 << p;
    uint32 Ref = xRefTZCNT32((uint32)i);
    uint32 Tst = (uint32)xTZCNT((uint32)i);
    CHECK(Ref == Tst);
  }

  //for(uint32 p = 0; p < 16; p++)
  //{
  //  uint64 i   = (uint64)1 << p;
  //  uint32 Ref = xRefTZCNT16((uint16)i);
  //  uint32 Tst = (uint32)xTZCNT((uint16)i);
  //  CHECK(Ref == Tst);
  //}
}

//===============================================================================================================================================================================================================


TEST_CASE("testLZCNT")
{
  testLZCNT();
}

TEST_CASE("testTZCNT")
{
  testTZCNT();
}

//===============================================================================================================================================================================================================

