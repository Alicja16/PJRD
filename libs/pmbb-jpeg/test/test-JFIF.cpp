/*
    SPDX-FileCopyrightText: 2019-2025 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <functional>
#include <utility>
#include <array>
#include "xTestUtils.h"
#include "xTimeUtils.h"
#include "xMemory.h"
#include "xCommonDefJPEG.h"
#include "xJPEG_MarkerUtils.h"
#include "xJFIF.h"
#include <functional>
#include <sstream>

using namespace PMBB_NAMESPACE;
using namespace PMBB_NAMESPACE::JPEG;

//===============================================================================================================================================================================================================

static constexpr uint64 c_Filler = 0x2121212121212121ul;

//===============================================================================================================================================================================================================

void testStuffing(std::function<void(xByteBuffer*, xByteBuffer*)>AddStuffing, std::function<void(xByteBuffer*, xByteBuffer*)>RemoveStuffing)
{
  constexpr int32 NumIters  = 128;
  constexpr int32 BatchSize = 64 * 1024;
  constexpr int64 BuffSize  = 2 * BatchSize;

  xByteBuffer SrcBuffer; SrcBuffer.resize(BuffSize);
  xByteBuffer RefBuffer; RefBuffer.resize(BuffSize);
  xByteBuffer MidBuffer; MidBuffer.resize(BuffSize);
  xByteBuffer DstBuffer; DstBuffer.resize(BuffSize);

  uint32 State = xTestUtils::c_XorShiftSeed;

  for(int32 j = 0; j < NumIters; j++)
  {
    SrcBuffer.reset();
    RefBuffer.reset();
    MidBuffer.reset();
    DstBuffer.reset();

    State = xTestUtils::fillRandom<uint8>(SrcBuffer.getWritePtr(), NOT_VALID, BatchSize, 1, 8, State);
    SrcBuffer.modifyWritten(BatchSize);

    xMarkerUtils::xAddStuffingSTD(&RefBuffer, &SrcBuffer);
    SrcBuffer.reset(); SrcBuffer.modifyWritten(BatchSize);

    //copy to output and add stuffing      
    AddStuffing(&MidBuffer, &SrcBuffer);

    CHECK(MidBuffer.getDataSize() == RefBuffer.getDataSize());
    bool IsSame = xTestUtils::isSameBuffer<uint8>(RefBuffer.getBufferPtr(), MidBuffer.getBufferPtr(), RefBuffer.getDataSize(), true);
    CHECK(IsSame);

    //copy from input and remove stuffing until next marker
    RemoveStuffing(&DstBuffer, &MidBuffer);

    //CHECK(SrcBuffer.getDataSize() == DstBuffer.getDataSize());

    IsSame = xTestUtils::isSameBuffer<uint8>(SrcBuffer.getBufferPtr(), DstBuffer.getBufferPtr(), BatchSize, true);
    CHECK(IsSame);
  }
}

void testFindSegment()
{
  xByteBuffer ByteBuffer(4096);
  xJFIF::WriteRST(&ByteBuffer, 1);
  ByteBuffer.appendU64_LE(c_Filler);
  ByteBuffer.appendU64_LE(c_Filler);
  xJFIF::WriteRST(&ByteBuffer, 2);
  for(int32 i = 0; i < 12; i++)
  {    
    for(int32 j = 0; j <= i; j++) { ByteBuffer.appendU64_LE(c_Filler); }
    xJFIF::WriteRST(&ByteBuffer, 3);
  }
  const int32 TestDataSize = ByteBuffer.getDataSize();
  
  //test known markers
  int32 Pos = xJFIF::FindMarker(&ByteBuffer, xJFIF::eMarker::RST1);
  CHECK(Pos == 0);
  ByteBuffer.modifyRead(2+Pos);
  
  Pos = xJFIF::FindMarker(&ByteBuffer, xJFIF::eMarker::RST2);
  CHECK(Pos == 16);
  ByteBuffer.modifyRead(2 + Pos);

  for(int32 i = 0; i < 12; i++)
  {
    Pos = xJFIF::FindMarker(&ByteBuffer, xJFIF::eMarker::RST3);
    CHECK(Pos == 8+i*8);
    ByteBuffer.modifyRead(2 + Pos);
  }

  CHECK(ByteBuffer.getDataSize() == 0);

  ByteBuffer.modifyRead(-TestDataSize);

  //test any markers
  Pos = xJFIF::FindMarker(&ByteBuffer, xJFIF::eMarker::ERR);
  CHECK(Pos == 0);
  ByteBuffer.modifyRead(2 + Pos);

  Pos = xJFIF::FindMarker(&ByteBuffer, xJFIF::eMarker::ERR);
  CHECK(Pos == 16);
  ByteBuffer.modifyRead(2 + Pos);

  for(int32 i = 0; i < 12; i++)
  {
    Pos = xJFIF::FindMarker(&ByteBuffer, xJFIF::eMarker::ERR);
    CHECK(Pos == 8 + i * 8);
    ByteBuffer.modifyRead(2 + Pos);
  }

  CHECK(ByteBuffer.getDataSize() == 0);

  //test no markers
  ByteBuffer.reset();
  for(int32 j = 0; j < 512; j++) { ByteBuffer.appendU64_LE(c_Filler); }
  Pos = xJFIF::FindMarker(&ByteBuffer, xJFIF::eMarker::ERR);
  CHECK(Pos == -1);

  //test edge marker
  ByteBuffer.reset();
  for(int32 j = 0; j < 511; j++) { ByteBuffer.appendU64_LE(c_Filler); }
  for(int32 j = 0; j < 7; j++) { ByteBuffer.appendU8((uint8)(c_Filler & 0xFF)); }
  ByteBuffer.appendU8(0xFF);
  Pos = xJFIF::FindMarker(&ByteBuffer, xJFIF::eMarker::ERR);
  CHECK(Pos == -2);
}

void testFileParser()
{
  xByteBuffer ByteBuffer(4096);

  xJFIF::WriteRST(&ByteBuffer, 2);
  for(int32 i = 0; i < 12; i++)
  {
    for(int32 j = 0; j <= i; j++) { ByteBuffer.appendU64_LE(c_Filler); }
    xJFIF::WriteRST(&ByteBuffer, 3);
  }

  //test any markers
  {
    std::vector<int64> Expectations = { 0 };
    int64 ExpAcc = 0;
    for(int32 i = 0; i < 12; i++)
    {
      ExpAcc += 8 * (i + 1) + 2;
      Expectations.push_back(ExpAcc);
    }

    std::stringstream StringStream;
    ByteBuffer.write(&StringStream);
    xStream File; File.bindStream(&StringStream, xStream::eDirF::Read);
    std::vector<int64> Positions = xJFIF::SeekAllMarkers(&File, xJFIF::eMarker::ERR, 11);

    CHECK(Positions.size() == Expectations.size());
    for(size_t i = 0; i < Positions.size(); i++)
    {
      CHECK(Positions[i] == Expectations[i]);
    }
  }

  //test known markers
  {
    std::vector<int64> Expectations = { };
    int64 ExpAcc = 0;
    for(int32 i = 0; i < 12; i++)
    {
      ExpAcc += 8 * (i + 1) + 2;
      Expectations.push_back(ExpAcc);
    }

    std::stringstream StringStream;
    ByteBuffer.write(&StringStream);
    xStream File; File.bindStream(&StringStream, xStream::eDirF::Read);
    std::vector<int64> Positions = xJFIF::SeekAllMarkers(&File, xJFIF::eMarker::RST3, 11);

    CHECK(Positions.size() == Expectations.size());
    for(size_t i = 0; i < Positions.size(); i++)
    {
      CHECK(Positions[i] == Expectations[i]);
    }
  }
}

//===============================================================================================================================================================================================================

TEST_CASE("xMarkerUtils::StuffingSTD")
{
  testStuffing(xMarkerUtils::xAddStuffingSTD, xMarkerUtils::xRemoveStuffingSTD);
}

#if X_SIMD_CAN_USE_SSE
TEST_CASE("xMarkerUtils::StuffingSSE")
{
  testStuffing(xMarkerUtils::xAddStuffingSSE, xMarkerUtils::xRemoveStuffingSSE);
}
#endif

#if X_SIMD_CAN_USE_AVX
TEST_CASE("xMarkerUtils::StuffingAVX")
{
  testStuffing(xMarkerUtils::xAddStuffingAVX, xMarkerUtils::xRemoveStuffingAVX);
}
#endif

#if X_SIMD_CAN_USE_AVX512
TEST_CASE("xMarkerUtils::StuffingAVX512")
{
  testStuffing(xMarkerUtils::xAddStuffingAVX512, xMarkerUtils::RemoveStuffing);
}
#endif

#if X_SIMD_CAN_USE_AVX512_ZEN4
TEST_CASE("xMarkerUtils::StuffingAVX512_VBMI2")
{
  testStuffing(xMarkerUtils::xAddStuffingAVX512_VBMI2, xMarkerUtils::xRemoveStuffingAVX512_VBMI2);
}
#endif

TEST_CASE("xJFIF::FindSegment")
{
  testFindSegment();
}

TEST_CASE("xJFIF::xFileParser")
{
  testFileParser();
}

//===============================================================================================================================================================================================================
