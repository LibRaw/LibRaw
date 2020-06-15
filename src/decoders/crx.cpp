/* -*- C++ -*-
 * File: libraw_crxdec.cpp
 * Copyright (C) 2018-2019 Alexey Danilchenko
 * Copyright (C) 2019 Alex Tutubalin, LibRaw LLC
 *
   Canon CR3 file decoder

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#include "../../internal/libraw_cxx_defs.h"

#ifdef _abs
#undef _abs
#undef _min
#undef _constrain
#endif
#define _abs(x) (((x) ^ ((int32_t)(x) >> 31)) - ((int32_t)(x) >> 31))
#define _min(a, b) ((a) < (b) ? (a) : (b))
#define _constrain(x, l, u) ((x) < (l) ? (l) : ((x) > (u) ? (u) : (x)))

#if defined(__clang__) || defined(__GNUG__)
#define libraw_inline inline __attribute__((always_inline))
#elif defined(_MSC_VER) && _MSC_VER > 1400
#define libraw_inline __forceinline
#else
#define libraw_inline inline
#endif

// this should be divisible by 4
#define CRX_BUF_SIZE 0x10000
#if !defined(_WIN32) || (defined (__GNUC__) && !defined(__INTRINSIC_SPECIAL__BitScanReverse))  
/* __INTRINSIC_SPECIAL__BitScanReverse found in MinGW32-W64 v7.30 headers, may be there is a better solution? */
typedef uint32_t DWORD;
libraw_inline void _BitScanReverse(DWORD *Index, unsigned long Mask)
{
  *Index = sizeof(unsigned long) * 8 - 1 - __builtin_clzl(Mask);
}
#if LibRawBigEndian
#define _byteswap_ulong(x) (x)
#else
#define _byteswap_ulong(x) __builtin_bswap32(x)
#endif
#endif

struct CrxBitstream
{
  uint8_t mdatBuf[CRX_BUF_SIZE];
  uint64_t mdatSize;
  uint64_t curBufOffset;
  uint32_t curPos;
  uint32_t curBufSize;
  uint32_t bitData;
  int32_t bitsLeft;
  LibRaw_abstract_datastream *input;
};

struct CrxBandParam
{
  CrxBitstream bitStream;
  int16_t subbandWidth;
  int16_t subbandHeight;
  int32_t roundedBitsMask;
  int32_t roundedBits;
  int16_t curLine;
  int32_t *lineBuf0;
  int32_t *lineBuf1;
  int32_t *lineBuf2;
  int32_t sParam;
  int32_t kParam;
  int32_t *paramData;
  int32_t *nonProgrData;
  int8_t supportsPartial;
};

struct CrxWaveletTransform
{
  int32_t *subband0Buf;
  int32_t *subband1Buf;
  int32_t *subband2Buf;
  int32_t *subband3Buf;
  int32_t *lineBuf[8];
  int16_t curLine;
  int16_t curH;
  int8_t fltTapH;
  int16_t height;
  int16_t width;
};

struct CrxSubband
{
  CrxBandParam *bandParam;
  uint64_t mdatOffset;
  uint8_t *bandBuf;
  int32_t bandSize;
  uint64_t dataSize;
  int8_t supportsPartial;
  int32_t quantValue;
  uint16_t width;
  uint16_t height;
  int32_t paramK;
  int64_t dataOffset;
};

struct CrxPlaneComp
{
  uint8_t *compBuf;
  CrxSubband *subBands;
  CrxWaveletTransform *waveletTransform;
  int8_t compNumber;
  int64_t dataOffset;
  int32_t compSize;
  int8_t supportsPartial;
  int32_t roundedBitsMask;
  int8_t tileFlag;
};

struct CrxTile
{
  CrxPlaneComp *comps;
  int8_t tileFlag;
  int8_t tileNumber;
  int64_t dataOffset;
  int32_t tileSize;
  uint16_t width;
  uint16_t height;
};

struct CrxImage
{
  uint8_t nPlanes;
  uint16_t planeWidth;
  uint16_t planeHeight;
  uint8_t samplePrecision;
  uint8_t subbandCount;
  uint8_t levels;
  uint8_t nBits;
  uint8_t encType;
  uint8_t tileCols;
  uint8_t tileRows;
  CrxTile *tiles;
  uint64_t mdatOffset;
  uint64_t mdatSize;
  int16_t *outBufs[4]; // one per plane
  int16_t *planeBuf;
  LibRaw_abstract_datastream *input;
#ifdef LIBRAW_CR3_MEMPOOL
  libraw_memmgr memmgr;
  CrxImage() : memmgr(0){}
#endif
};

enum TileFlags
{
  E_HAS_TILES_ON_THE_RIGHT = 1,
  E_HAS_TILES_ON_THE_LEFT = 2,
  E_HAS_TILES_ON_THE_BOTTOM = 4,
  E_HAS_TILES_ON_THE_TOP = 8
};

int32_t exCoefNumTbl[0x120] = {
    // level 1
    1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
    1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    // level 2
    1, 1, 3, 3, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 3, 2, 1, 0, 1, 0, 0, 0, 0, 0, 1,
    2, 4, 4, 2, 1, 2, 1, 0, 0, 0, 0, 1, 1, 4, 3, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1,
    3, 3, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 3, 2, 1, 0, 1, 0, 0, 0, 0, 0, 1, 2, 4,
    4, 2, 1, 2, 1, 0, 0, 0, 0, 1, 1, 4, 3, 1, 1, 1, 1, 0, 0, 0, 0,

    // level 3
    1, 1, 7, 7, 1, 1, 3, 3, 1, 1, 1, 1, 1, 0, 7, 6, 1, 0, 3, 2, 1, 0, 1, 0, 1,
    2, 10, 10, 2, 2, 5, 4, 2, 1, 2, 1, 1, 1, 10, 9, 1, 2, 4, 4, 2, 1, 2, 1, 1,
    1, 9, 9, 1, 2, 4, 4, 2, 1, 2, 1, 1, 0, 9, 8, 1, 1, 4, 3, 1, 1, 1, 1, 1, 2,
    8, 8, 2, 1, 4, 3, 1, 1, 1, 1, 1, 1, 8, 7, 1, 1, 3, 3, 1, 1, 1, 1};

uint32_t JS[32] = {1,     1,     1,     1,     2,      2,      2,      2,
                   4,     4,     4,     4,     8,      8,      8,      8,
                   0x10,  0x10,  0x20,  0x20,  0x40,   0x40,   0x80,   0x80,
                   0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000};

uint32_t J[32] = {0, 0, 0, 0, 1,    1,    1,    1,    2,    2,   2,
                  2, 3, 3, 3, 3,    4,    4,    5,    5,    6,   6,
                  7, 7, 8, 9, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};

static inline void crxFillBuffer(CrxBitstream *bitStrm)
{
  if (bitStrm->curPos >= bitStrm->curBufSize && bitStrm->mdatSize)
  {
    bitStrm->curPos = 0;
    bitStrm->curBufOffset += bitStrm->curBufSize;
#ifdef LIBRAW_USE_OPENMP
#pragma omp critical
#endif
    {
#ifndef LIBRAW_USE_OPENMP
      bitStrm->input->lock();
#endif
      bitStrm->input->seek(bitStrm->curBufOffset, SEEK_SET);
      bitStrm->curBufSize = bitStrm->input->read(
          bitStrm->mdatBuf, 1, _min(bitStrm->mdatSize, CRX_BUF_SIZE));
#ifndef LIBRAW_USE_OPENMP
      bitStrm->input->unlock();
#endif
      if (bitStrm->curBufSize < 1) // nothing read
        throw LIBRAW_EXCEPTION_IO_EOF;
      bitStrm->mdatSize -= bitStrm->curBufSize;
    }
  }
}

libraw_inline int crxBitstreamGetZeros(CrxBitstream *bitStrm)
{
  uint32_t nonZeroBit = 0;
  uint64_t nextData = 0;
  int32_t result = 0;

  if (bitStrm->bitData)
  {
    _BitScanReverse((DWORD *)&nonZeroBit, (DWORD)bitStrm->bitData);
    result = 31 - nonZeroBit;
    bitStrm->bitData <<= 32 - nonZeroBit;
    bitStrm->bitsLeft -= 32 - nonZeroBit;
  }
  else
  {
    uint32_t bitsLeft = bitStrm->bitsLeft;
    while (1)
    {
      while (bitStrm->curPos + 4 <= bitStrm->curBufSize)
      {
        nextData =
            _byteswap_ulong(*(uint32_t *)(bitStrm->mdatBuf + bitStrm->curPos));
        bitStrm->curPos += 4;
        crxFillBuffer(bitStrm);
        if (nextData)
        {
          _BitScanReverse((DWORD *)&nonZeroBit, (DWORD)nextData);
          result = bitsLeft + 31 - nonZeroBit;
          bitStrm->bitData = nextData << (32 - nonZeroBit);
          bitStrm->bitsLeft = nonZeroBit;
          return result;
        }
        bitsLeft += 32;
      }
      if (bitStrm->curBufSize < bitStrm->curPos + 1)
        break; // error
      nextData = bitStrm->mdatBuf[bitStrm->curPos++];
      crxFillBuffer(bitStrm);
      if (nextData)
        break;
      bitsLeft += 8;
    }
    _BitScanReverse((DWORD *)&nonZeroBit, (DWORD)nextData);
    result = (uint32_t)(bitsLeft + 7 - nonZeroBit);
    bitStrm->bitData = nextData << (32 - nonZeroBit);
    bitStrm->bitsLeft = nonZeroBit;
  }
  return result;
}

libraw_inline uint32_t crxBitstreamGetBits(CrxBitstream *bitStrm, int bits)
{
  int bitsLeft = bitStrm->bitsLeft;
  uint32_t bitData = bitStrm->bitData;
  uint32_t nextWord;
  uint8_t nextByte;
  uint32_t result;

  if (bitsLeft < bits)
  {
    // get them from stream
    if (bitStrm->curPos + 4 <= bitStrm->curBufSize)
    {
      nextWord =
          _byteswap_ulong(*(uint32_t *)(bitStrm->mdatBuf + bitStrm->curPos));
      bitStrm->curPos += 4;
      crxFillBuffer(bitStrm);
      bitStrm->bitsLeft = 32 - (bits - bitsLeft);
      result = ((nextWord >> bitsLeft) | bitData) >> (32 - bits);
      bitStrm->bitData = nextWord << (bits - bitsLeft);
      return result;
    }
    // less than a word left - read byte at a time
    do
    {
      if (bitStrm->curPos >= bitStrm->curBufSize)
        break; // error
      bitsLeft += 8;
      nextByte = bitStrm->mdatBuf[bitStrm->curPos++];
      crxFillBuffer(bitStrm);
      bitData |= nextByte << (32 - bitsLeft);
    } while (bitsLeft < bits);
  }
  result = bitData >> (32 - bits); // 32-bits
  bitStrm->bitData = bitData << bits;
  bitStrm->bitsLeft = bitsLeft - bits;
  return result;
}

libraw_inline int crxPredictKParameter(int32_t prevK, int32_t bitCode,
                                       int32_t maxVal = 0)
{
  int32_t newKParam = prevK - (bitCode < (1 << prevK >> 1)) +
                      ((bitCode >> prevK) > 2) + ((bitCode >> prevK) > 5);

  return !maxVal || newKParam < maxVal ? newKParam : maxVal;
}

libraw_inline void crxDecodeSymbolL1(CrxBandParam *param,
                                     int32_t doMedianPrediction,
                                     int32_t notEOL = 0)
{
  if (doMedianPrediction)
  {
    int32_t symb[4];

    int32_t delta = param->lineBuf0[1] - param->lineBuf0[0];
    symb[2] = param->lineBuf1[0];
    symb[0] = symb[1] = delta + symb[2];
    symb[3] = param->lineBuf0[1];

    param->lineBuf1[1] =
        symb[(((param->lineBuf0[0] < param->lineBuf1[0]) ^ (delta < 0)) << 1) +
             ((param->lineBuf1[0] < param->lineBuf0[1]) ^ (delta < 0))];
  }
  else
    param->lineBuf1[1] = param->lineBuf0[1];

  // get next error symbol
  uint32_t bitCode = crxBitstreamGetZeros(&param->bitStream);
  if (bitCode >= 41)
    bitCode = crxBitstreamGetBits(&param->bitStream, 21);
  else if (param->kParam)
    bitCode = crxBitstreamGetBits(&param->bitStream, param->kParam) |
              (bitCode << param->kParam);

  // add converted (+/-) error code to predicted value
  param->lineBuf1[1] += -(bitCode & 1) ^ (bitCode >> 1);

  // for not end of the line - use one symbol ahead to estimate next K
  if (notEOL)
  {
    int32_t nextDelta = (param->lineBuf0[2] - param->lineBuf0[1]) << 1;
    bitCode = (bitCode + _abs(nextDelta)) >> 1;
    ++param->lineBuf0;
  }

  // update K parameter
  param->kParam = crxPredictKParameter(param->kParam, bitCode, 15);

  ++param->lineBuf1;
}

int crxDecodeLine(CrxBandParam *param)
{
  int length = param->subbandWidth;

  param->lineBuf1[0] = param->lineBuf0[1];
  for (; length > 1; --length)
  {
    if (param->lineBuf1[0] != param->lineBuf0[1] ||
        param->lineBuf1[0] != param->lineBuf0[2])
    {
      crxDecodeSymbolL1(param, 1, 1);
    }
    else
    {
      int nSyms = 0;
      if (crxBitstreamGetBits(&param->bitStream, 1))
      {
        nSyms = 1;
        while (crxBitstreamGetBits(&param->bitStream, 1))
        {
          nSyms += JS[param->sParam];
          if (nSyms > length)
          {
            nSyms = length;
            break;
          }
          if (param->sParam < 31)
            ++param->sParam;
          if (nSyms == length)
            break;
        }

        if (nSyms < length)
        {
          if (J[param->sParam])
            nSyms += crxBitstreamGetBits(&param->bitStream, J[param->sParam]);
          if (param->sParam > 0)
            --param->sParam;
          if (nSyms > length)
            return -1;
        }

        length -= nSyms;

        // copy symbol nSyms times
        param->lineBuf0 += nSyms;

        // copy symbol nSyms times
        while (nSyms-- > 0)
        {
          param->lineBuf1[1] = param->lineBuf1[0];
          ++param->lineBuf1;
        }
      }

      if (length > 0)
        crxDecodeSymbolL1(param, 0, (length > 1));
    }
  }

  if (length == 1)
    crxDecodeSymbolL1(param, 1, 0);

  param->lineBuf1[1] = param->lineBuf1[0] + 1;

  return 0;
}

libraw_inline void crxDecodeSymbolL1Rounded(CrxBandParam *param,
                                            int32_t doSym = 1,
                                            int32_t doCode = 1)
{
  int32_t sym = param->lineBuf0[1];

  if (doSym)
  {
    // calculate the next symbol gradient
    int32_t symb[4];
    int32_t deltaH = param->lineBuf0[1] - param->lineBuf0[0];
    symb[2] = param->lineBuf1[0];
    symb[0] = symb[1] = deltaH + symb[2];
    symb[3] = param->lineBuf0[1];
    sym =
        symb[(((param->lineBuf0[0] < param->lineBuf1[0]) ^ (deltaH < 0)) << 1) +
             ((param->lineBuf1[0] < param->lineBuf0[1]) ^ (deltaH < 0))];
  }

  uint32_t bitCode = crxBitstreamGetZeros(&param->bitStream);
  if (bitCode >= 41)
    bitCode = crxBitstreamGetBits(&param->bitStream, 21);
  else if (param->kParam)
    bitCode = crxBitstreamGetBits(&param->bitStream, param->kParam) |
              (bitCode << param->kParam);
  int32_t code = -(bitCode & 1) ^ (bitCode >> 1);
  param->lineBuf1[1] = param->roundedBitsMask * 2 * code + (code >> 31) + sym;

  if (doCode)
  {
    if (param->lineBuf0[2] > param->lineBuf0[1])
      code = (param->lineBuf0[2] - param->lineBuf0[1] + param->roundedBitsMask -
              1) >>
             param->roundedBits;
    else
      code = -(
          (param->lineBuf0[1] - param->lineBuf0[2] + param->roundedBitsMask) >>
          param->roundedBits);

    param->kParam = crxPredictKParameter(param->kParam,
                                         (bitCode + 2 * _abs(code)) >> 1, 15);
  }
  else
    param->kParam = crxPredictKParameter(param->kParam, bitCode, 15);

  ++param->lineBuf1;
}

int crxDecodeLineRounded(CrxBandParam *param)
{
  int32_t valueReached = 0;

  param->lineBuf0[0] = param->lineBuf0[1];
  param->lineBuf1[0] = param->lineBuf0[1];
  int32_t length = param->subbandWidth;

  for (; length > 1; --length)
  {
    if (_abs(param->lineBuf0[2] - param->lineBuf0[1]) > param->roundedBitsMask)
    {
      crxDecodeSymbolL1Rounded(param);
      ++param->lineBuf0;
      valueReached = 1;
    }
    else if (valueReached || _abs(param->lineBuf0[0] - param->lineBuf1[0]) >
                                 param->roundedBitsMask)
    {
      crxDecodeSymbolL1Rounded(param);
      ++param->lineBuf0;
      valueReached = 0;
    }
    else
    {
      int nSyms = 0;
      if (crxBitstreamGetBits(&param->bitStream, 1))
      {
        nSyms = 1;
        while (crxBitstreamGetBits(&param->bitStream, 1))
        {
          nSyms += JS[param->sParam];
          if (nSyms > length)
          {
            nSyms = length;
            break;
          }
          if (param->sParam < 31)
            ++param->sParam;
          if (nSyms == length)
            break;
        }
        if (nSyms < length)
        {
          if (J[param->sParam])
            nSyms += crxBitstreamGetBits(&param->bitStream, J[param->sParam]);
          if (param->sParam > 0)
            --param->sParam;
        }
        if (nSyms > length)
          return -1;
      }
      length -= nSyms;

      // copy symbol nSyms times
      param->lineBuf0 += nSyms;

      // copy symbol nSyms times
      while (nSyms-- > 0)
      {
        param->lineBuf1[1] = param->lineBuf1[0];
        ++param->lineBuf1;
      }

      if (length > 1)
      {
        crxDecodeSymbolL1Rounded(param, 0);
        ++param->lineBuf0;
        valueReached = _abs(param->lineBuf0[1] - param->lineBuf0[0]) >
                       param->roundedBitsMask;
      }
      else if (length == 1)
        crxDecodeSymbolL1Rounded(param, 0, 0);
    }
  }
  if (length == 1)
    crxDecodeSymbolL1Rounded(param, 1, 0);

  param->lineBuf1[1] = param->lineBuf1[0] + 1;

  return 0;
}

int crxDecodeLineNoRefPrevLine(CrxBandParam *param)
{
  int32_t i = 0;

  for (; i < param->subbandWidth - 1; i++)
  {
    if (param->lineBuf0[i + 2] | param->lineBuf0[i + 1] | param->lineBuf1[i])
    {
      uint32_t bitCode = crxBitstreamGetZeros(&param->bitStream);
      if (bitCode >= 41)
        bitCode = crxBitstreamGetBits(&param->bitStream, 21);
      else if (param->kParam)
        bitCode = crxBitstreamGetBits(&param->bitStream, param->kParam) |
                  (bitCode << param->kParam);
      param->lineBuf1[i + 1] = -(bitCode & 1) ^ (bitCode >> 1);
      param->kParam = crxPredictKParameter(param->kParam, bitCode);
      if (param->lineBuf2[i + 1] - param->kParam <= 1)
      {
        if (param->kParam >= 15)
          param->kParam = 15;
      }
      else
        ++param->kParam;
    }
    else
    {
      int nSyms = 0;
      if (crxBitstreamGetBits(&param->bitStream, 1))
      {
        nSyms = 1;
        if (i != param->subbandWidth - 1)
        {
          while (crxBitstreamGetBits(&param->bitStream, 1))
          {
            nSyms += JS[param->sParam];
            if (i + nSyms > param->subbandWidth)
            {
              nSyms = param->subbandWidth - i;
              break;
            }
            if (param->sParam < 31)
              ++param->sParam;
            if (i + nSyms == param->subbandWidth)
              break;
          }
          if (i + nSyms < param->subbandWidth)
          {
            if (J[param->sParam])
              nSyms += crxBitstreamGetBits(&param->bitStream, J[param->sParam]);
            if (param->sParam > 0)
              --param->sParam;
          }
          if (i + nSyms > param->subbandWidth)
            return -1;
        }
      }
      else if (i > param->subbandWidth)
        return -1;

      if (nSyms > 0)
      {
        memset(param->lineBuf1 + i + 1, 0, nSyms * sizeof(int32_t));
        memset(param->lineBuf2 + i, 0, nSyms * sizeof(int32_t));
        i += nSyms;
      }

      if (i >= param->subbandWidth - 1)
      {
        if (i == param->subbandWidth - 1)
        {
          uint32_t bitCode = crxBitstreamGetZeros(&param->bitStream);
          if (bitCode >= 41)
            bitCode = crxBitstreamGetBits(&param->bitStream, 21);
          else if (param->kParam)
            bitCode = crxBitstreamGetBits(&param->bitStream, param->kParam) |
                      (bitCode << param->kParam);
          param->lineBuf1[i + 1] = -((bitCode + 1) & 1) ^ ((bitCode + 1) >> 1);
          param->kParam = crxPredictKParameter(param->kParam, bitCode, 15);
          param->lineBuf2[i] = param->kParam;
        }
        continue;
      }
      else
      {
        uint32_t bitCode = crxBitstreamGetZeros(&param->bitStream);
        if (bitCode >= 41)
          bitCode = crxBitstreamGetBits(&param->bitStream, 21);
        else if (param->kParam)
          bitCode = crxBitstreamGetBits(&param->bitStream, param->kParam) |
                    (bitCode << param->kParam);
        param->lineBuf1[i + 1] = -((bitCode + 1) & 1) ^ ((bitCode + 1) >> 1);
        param->kParam = crxPredictKParameter(param->kParam, bitCode);
        if (param->lineBuf2[i + 1] - param->kParam <= 1)
        {
          if (param->kParam >= 15)
            param->kParam = 15;
        }
        else
          ++param->kParam;
      }
    }
    param->lineBuf2[i] = param->kParam;
  }
  if (i == param->subbandWidth - 1)
  {
    int32_t bitCode = crxBitstreamGetZeros(&param->bitStream);
    if (bitCode >= 41)
      bitCode = crxBitstreamGetBits(&param->bitStream, 21);
    else if (param->kParam)
      bitCode = crxBitstreamGetBits(&param->bitStream, param->kParam) |
                (bitCode << param->kParam);
    param->lineBuf1[i + 1] = -(bitCode & 1) ^ (bitCode >> 1);
    param->kParam = crxPredictKParameter(param->kParam, bitCode, 15);
    param->lineBuf2[i] = param->kParam;
  }

  return 0;
}

int crxDecodeTopLine(CrxBandParam *param)
{
  param->lineBuf1[0] = 0;

  int32_t length = param->subbandWidth;

  // read the line from bitstream
  for (; length > 1; --length)
  {
    if (param->lineBuf1[0])
      param->lineBuf1[1] = param->lineBuf1[0];
    else
    {
      int nSyms = 0;
      if (crxBitstreamGetBits(&param->bitStream, 1))
      {
        nSyms = 1;
        while (crxBitstreamGetBits(&param->bitStream, 1))
        {
          nSyms += JS[param->sParam];
          if (nSyms > length)
          {
            nSyms = length;
            break;
          }
          if (param->sParam < 31)
            ++param->sParam;
          if (nSyms == length)
            break;
        }
        if (nSyms < length)
        {
          if (J[param->sParam])
            nSyms += crxBitstreamGetBits(&param->bitStream, J[param->sParam]);
          if (param->sParam > 0)
            --param->sParam;
          if (nSyms > length)
            return -1;
        }

        length -= nSyms;

        // copy symbol nSyms times
        while (nSyms-- > 0)
        {
          param->lineBuf1[1] = param->lineBuf1[0];
          ++param->lineBuf1;
        }

        if (length <= 0)
          break;
      }

      param->lineBuf1[1] = 0;
    }

    uint32_t bitCode = crxBitstreamGetZeros(&param->bitStream);
    if (bitCode >= 41)
      bitCode = crxBitstreamGetBits(&param->bitStream, 21);
    else if (param->kParam)
      bitCode = crxBitstreamGetBits(&param->bitStream, param->kParam) |
                (bitCode << param->kParam);
    param->lineBuf1[1] += -(bitCode & 1) ^ (bitCode >> 1);
    param->kParam = crxPredictKParameter(param->kParam, bitCode, 15);
    ++param->lineBuf1;
  }

  if (length == 1)
  {
    param->lineBuf1[1] = param->lineBuf1[0];
    uint32_t bitCode = crxBitstreamGetZeros(&param->bitStream);
    if (bitCode >= 41)
      bitCode = crxBitstreamGetBits(&param->bitStream, 21);
    else if (param->kParam)
      bitCode = crxBitstreamGetBits(&param->bitStream, param->kParam) |
                (bitCode << param->kParam);
    param->lineBuf1[1] += -(bitCode & 1) ^ (bitCode >> 1);
    param->kParam = crxPredictKParameter(param->kParam, bitCode, 15);
    ++param->lineBuf1;
  }

  param->lineBuf1[1] = param->lineBuf1[0] + 1;

  return 0;
}

int crxDecodeTopLineRounded(CrxBandParam *param)
{
  param->lineBuf1[0] = 0;

  int32_t length = param->subbandWidth;

  // read the line from bitstream
  for (; length > 1; --length)
  {
    if (_abs(param->lineBuf1[0]) > param->roundedBitsMask)
      param->lineBuf1[1] = param->lineBuf1[0];
    else
    {
      int nSyms = 0;
      if (crxBitstreamGetBits(&param->bitStream, 1))
      {
        nSyms = 1;
        while (crxBitstreamGetBits(&param->bitStream, 1))
        {
          nSyms += JS[param->sParam];
          if (nSyms > length)
          {
            nSyms = length;
            break;
          }
          if (param->sParam < 31)
            ++param->sParam;
          if (nSyms == length)
            break;
        }
        if (nSyms < length)
        {
          if (J[param->sParam])
            nSyms += crxBitstreamGetBits(&param->bitStream, J[param->sParam]);
          if (param->sParam > 0)
            --param->sParam;
          if (nSyms > length)
            return -1;
        }
      }

      length -= nSyms;

      // copy symbol nSyms times
      while (nSyms-- > 0)
      {
        param->lineBuf1[1] = param->lineBuf1[0];
        ++param->lineBuf1;
      }

      if (length <= 0)
        break;

      param->lineBuf1[1] = 0;
    }

    uint32_t bitCode = crxBitstreamGetZeros(&param->bitStream);
    if (bitCode >= 41)
      bitCode = crxBitstreamGetBits(&param->bitStream, 21);
    else if (param->kParam)
      bitCode = crxBitstreamGetBits(&param->bitStream, param->kParam) |
                (bitCode << param->kParam);

    int32_t sVal = -(bitCode & 1) ^ (bitCode >> 1);
    param->lineBuf1[1] += param->roundedBitsMask * 2 * sVal + (sVal >> 31);
    param->kParam = crxPredictKParameter(param->kParam, bitCode, 15);
    ++param->lineBuf1;
  }

  if (length == 1)
  {
    uint32_t bitCode = crxBitstreamGetZeros(&param->bitStream);
    if (bitCode >= 41)
      bitCode = crxBitstreamGetBits(&param->bitStream, 21);
    else if (param->kParam)
      bitCode = crxBitstreamGetBits(&param->bitStream, param->kParam) |
                (bitCode << param->kParam);
    int32_t sVal = -(bitCode & 1) ^ (bitCode >> 1);
    param->lineBuf1[1] += param->roundedBitsMask * 2 * sVal + (sVal >> 31);
    param->kParam = crxPredictKParameter(param->kParam, bitCode, 15);
    ++param->lineBuf1;
  }

  param->lineBuf1[1] = param->lineBuf1[0] + 1;

  return 0;
}

int crxDecodeTopLineNoRefPrevLine(CrxBandParam *param)
{
  param->lineBuf0[0] = 0;
  param->lineBuf1[0] = 0;
  int32_t length = param->subbandWidth;
  for (; length > 1; --length)
  {
    if (param->lineBuf1[0])
    {
      uint32_t bitCode = crxBitstreamGetZeros(&param->bitStream);
      if (bitCode >= 41)
        bitCode = crxBitstreamGetBits(&param->bitStream, 21);
      else if (param->kParam)
        bitCode = crxBitstreamGetBits(&param->bitStream, param->kParam) |
                  (bitCode << param->kParam);
      param->lineBuf1[1] = -(bitCode & 1) ^ (bitCode >> 1);
      param->kParam = crxPredictKParameter(param->kParam, bitCode, 15);
    }
    else
    {
      int nSyms = 0;
      if (crxBitstreamGetBits(&param->bitStream, 1))
      {
        nSyms = 1;
        while (crxBitstreamGetBits(&param->bitStream, 1))
        {
          nSyms += JS[param->sParam];
          if (nSyms > length)
          {
            nSyms = length;
            break;
          }
          if (param->sParam < 31)
            ++param->sParam;
          if (nSyms == length)
            break;
        }
        if (nSyms < length)
        {
          if (J[param->sParam])
            nSyms += crxBitstreamGetBits(&param->bitStream, J[param->sParam]);
          if (param->sParam > 0)
            --param->sParam;
          if (nSyms > length)
            return -1;
        }
      }

      length -= nSyms;

      // copy symbol nSyms times
      while (nSyms-- > 0)
      {
        param->lineBuf2[0] = 0;
        param->lineBuf1[1] = 0;
        ++param->lineBuf1;
        ++param->lineBuf2;
      }

      if (length <= 0)
        break;
      uint32_t bitCode = crxBitstreamGetZeros(&param->bitStream);
      if (bitCode >= 41)
        bitCode = crxBitstreamGetBits(&param->bitStream, 21);
      else if (param->kParam)
        bitCode = crxBitstreamGetBits(&param->bitStream, param->kParam) |
                  (bitCode << param->kParam);
      param->lineBuf1[1] = -((bitCode + 1) & 1) ^ ((bitCode + 1) >> 1);
      param->kParam = crxPredictKParameter(param->kParam, bitCode, 15);
    }
    param->lineBuf2[0] = param->kParam;
    ++param->lineBuf2;
    ++param->lineBuf1;
  }

  if (length == 1)
  {
    uint32_t bitCode = crxBitstreamGetZeros(&param->bitStream);
    if (bitCode >= 41)
      bitCode = crxBitstreamGetBits(&param->bitStream, 21);
    else if (param->kParam)
      bitCode = crxBitstreamGetBits(&param->bitStream, param->kParam) |
                (bitCode << param->kParam);
    param->lineBuf1[1] = -(bitCode & 1) ^ (bitCode >> 1);
    param->kParam = crxPredictKParameter(param->kParam, bitCode, 15);
    param->lineBuf2[0] = param->kParam;
    ++param->lineBuf1;
  }

  param->lineBuf1[1] = 0;

  return 0;
}

int crxDecodeLine(CrxBandParam *param, uint8_t *bandBuf)
{
  if (!param || !bandBuf)
    return -1;
  if (param->curLine >= param->subbandHeight)
    return -1;

  if (param->curLine == 0)
  {
    int32_t lineLength = param->subbandWidth + 2;

    param->sParam = 0;
    param->kParam = 0;
    if (param->supportsPartial)
    {
      if (param->roundedBitsMask <= 0)
      {
        param->lineBuf0 = (int32_t *)param->paramData;
        param->lineBuf1 = param->lineBuf0 + lineLength;
        int32_t *lineBuf = param->lineBuf1 + 1;
        if (crxDecodeTopLine(param))
          return -1;
        memcpy(bandBuf, lineBuf, param->subbandWidth * sizeof(int32_t));
        ++param->curLine;
      }
      else
      {
        param->roundedBits = 1;
        if (param->roundedBitsMask & ~1)
        {
          while (param->roundedBitsMask >> param->roundedBits)
            ++param->roundedBits;
        }
        param->lineBuf0 = (int32_t *)param->paramData;
        param->lineBuf1 = param->lineBuf0 + lineLength;
        int32_t *lineBuf = param->lineBuf1 + 1;
        if (crxDecodeTopLineRounded(param))
          return -1;
        memcpy(bandBuf, lineBuf, param->subbandWidth * sizeof(int32_t));
        ++param->curLine;
      }
    }
    else
    {
      param->lineBuf2 = (int32_t *)param->nonProgrData;
      param->lineBuf0 = (int32_t *)param->paramData;
      param->lineBuf1 = param->lineBuf0 + lineLength;
      int32_t *lineBuf = param->lineBuf1 + 1;
      if (crxDecodeTopLineNoRefPrevLine(param))
        return -1;
      memcpy(bandBuf, lineBuf, param->subbandWidth * sizeof(int32_t));
      ++param->curLine;
    }
  }
  else if (!param->supportsPartial)
  {
    int32_t lineLength = param->subbandWidth + 2;
    param->lineBuf2 = (int32_t *)param->nonProgrData;
    if (param->curLine & 1)
    {
      param->lineBuf1 = (int32_t *)param->paramData;
      param->lineBuf0 = param->lineBuf1 + lineLength;
    }
    else
    {
      param->lineBuf0 = (int32_t *)param->paramData;
      param->lineBuf1 = param->lineBuf0 + lineLength;
    }
    int32_t *lineBuf = param->lineBuf1 + 1;
    if (crxDecodeLineNoRefPrevLine(param))
      return -1;
    memcpy(bandBuf, lineBuf, param->subbandWidth * sizeof(int32_t));
    ++param->curLine;
  }
  else if (param->roundedBitsMask <= 0)
  {
    int32_t lineLength = param->subbandWidth + 2;
    if (param->curLine & 1)
    {
      param->lineBuf1 = (int32_t *)param->paramData;
      param->lineBuf0 = param->lineBuf1 + lineLength;
    }
    else
    {
      param->lineBuf0 = (int32_t *)param->paramData;
      param->lineBuf1 = param->lineBuf0 + lineLength;
    }
    int32_t *lineBuf = param->lineBuf1 + 1;
    if (crxDecodeLine(param))
      return -1;
    memcpy(bandBuf, lineBuf, param->subbandWidth * sizeof(int32_t));
    ++param->curLine;
  }
  else
  {
    int32_t lineLength = param->subbandWidth + 2;
    if (param->curLine & 1)
    {
      param->lineBuf1 = (int32_t *)param->paramData;
      param->lineBuf0 = param->lineBuf1 + lineLength;
    }
    else
    {
      param->lineBuf0 = (int32_t *)param->paramData;
      param->lineBuf1 = param->lineBuf0 + lineLength;
    }
    int32_t *lineBuf = param->lineBuf1 + 1;
    if (crxDecodeLineRounded(param))
      return -1;
    memcpy(bandBuf, lineBuf, param->subbandWidth * sizeof(int32_t));
    ++param->curLine;
  }
  return 0;
}

int crxDecodeLineWithIQuantization(CrxSubband *subband)
{
  int32_t q_step_tbl[6] = {0x28, 0x2D, 0x33, 0x39, 0x40, 0x48};

  if (!subband->dataSize)
  {
    memset(subband->bandBuf, 0, subband->bandSize);
    return 0;
  }

  if (subband->supportsPartial)
  {
    uint32_t bitCode = crxBitstreamGetZeros(&subband->bandParam->bitStream);
    if (bitCode >= 23)
      bitCode = crxBitstreamGetBits(&subband->bandParam->bitStream, 8);
    else if (subband->paramK)
      bitCode =
          crxBitstreamGetBits(&subband->bandParam->bitStream, subband->paramK) |
          (bitCode << subband->paramK);

    subband->quantValue +=
        -(bitCode & 1) ^ (bitCode >> 1); // converting encoded to signed integer
    subband->paramK = crxPredictKParameter(subband->paramK, bitCode);
    if (subband->paramK > 7)
      return -1;
  }
  if (crxDecodeLine(subband->bandParam, subband->bandBuf))
    return -1;

  if (subband->width <= 0)
    return 0LL;

  // update subband buffers
  int32_t *bandBuf = (int32_t *)subband->bandBuf;
  int32_t qScale =
      q_step_tbl[subband->quantValue % 6] >> (6 - subband->quantValue / 6);
  if (subband->quantValue / 6 >= 6)
    qScale = q_step_tbl[subband->quantValue % 6] *
             (1 << (subband->quantValue / 6 + 26));

  if (qScale != 1)
    for (int32_t i = 0; i < subband->width; i++)
      bandBuf[i] *= qScale;

  return 0;
}

void crxHorizontal53(int32_t *lineBufLA, int32_t *lineBufLB,
                     CrxWaveletTransform *wavelet, uint32_t tileFlag)
{
  int32_t *band0Buf = wavelet->subband0Buf;
  int32_t *band1Buf = wavelet->subband1Buf;
  int32_t *band2Buf = wavelet->subband2Buf;
  int32_t *band3Buf = wavelet->subband3Buf;

  if (wavelet->width <= 1)
  {
    lineBufLA[0] = band0Buf[0];
    lineBufLB[0] = band2Buf[0];
  }
  else
  {
    if (tileFlag & E_HAS_TILES_ON_THE_LEFT)
    {
      lineBufLA[0] = band0Buf[0] - ((band1Buf[0] + band1Buf[1] + 2) >> 2);
      lineBufLB[0] = band2Buf[0] - ((band3Buf[0] + band3Buf[1] + 2) >> 2);
      ++band1Buf;
      ++band3Buf;
    }
    else
    {
      lineBufLA[0] = band0Buf[0] - ((band1Buf[0] + 1) >> 1);
      lineBufLB[0] = band2Buf[0] - ((band3Buf[0] + 1) >> 1);
    }
    ++band0Buf;
    ++band2Buf;

    for (int i = 0; i < wavelet->width - 3; i += 2)
    {
      int32_t delta = band0Buf[0] - ((band1Buf[0] + band1Buf[1] + 2) >> 2);
      lineBufLA[1] = band1Buf[0] + ((delta + lineBufLA[0]) >> 1);
      lineBufLA[2] = delta;

      delta = band2Buf[0] - ((band3Buf[0] + band3Buf[1] + 2) >> 2);
      lineBufLB[1] = band3Buf[0] + ((delta + lineBufLB[0]) >> 1);
      lineBufLB[2] = delta;

      ++band0Buf;
      ++band1Buf;
      ++band2Buf;
      ++band3Buf;
      lineBufLA += 2;
      lineBufLB += 2;
    }
    if (tileFlag & E_HAS_TILES_ON_THE_RIGHT)
    {
      int32_t deltaA = band0Buf[0] - ((band1Buf[0] + band1Buf[1] + 2) >> 2);
      lineBufLA[1] = band1Buf[0] + ((deltaA + lineBufLA[0]) >> 1);

      int32_t deltaB = band2Buf[0] - ((band3Buf[0] + band3Buf[1] + 2) >> 2);
      lineBufLB[1] = band3Buf[0] + ((deltaB + lineBufLB[0]) >> 1);

      if (wavelet->width & 1)
      {
        lineBufLA[2] = deltaA;
        lineBufLB[2] = deltaB;
      }
    }
    else if (wavelet->width & 1)
    {
      lineBufLA[1] =
          band1Buf[0] +
          ((lineBufLA[0] + band0Buf[0] - ((band1Buf[0] + 1) >> 1)) >> 1);
      lineBufLA[2] = band0Buf[0] - ((band1Buf[0] + 1) >> 1);

      lineBufLB[1] =
          band3Buf[0] +
          ((lineBufLB[0] + band2Buf[0] - ((band3Buf[0] + 1) >> 1)) >> 1);
      lineBufLB[2] = band2Buf[0] - ((band3Buf[0] + 1) >> 1);
    }
    else
    {
      lineBufLA[1] = lineBufLA[0] + band1Buf[0];
      lineBufLB[1] = lineBufLB[0] + band3Buf[0];
    }
  }
}

int32_t *crxIdwt53FilterGetLine(CrxPlaneComp *comp, int32_t level)
{
  int32_t *result = comp->waveletTransform[level]
                        .lineBuf[(comp->waveletTransform[level].fltTapH -
                                  comp->waveletTransform[level].curH + 5) %
                                     5 +
                                 3];
  comp->waveletTransform[level].curH--;
  return result;
}

int crxIdwt53FilterDecode(CrxPlaneComp *comp, int32_t level)
{
  if (comp->waveletTransform[level].curH)
    return 0;

  CrxSubband *sband = comp->subBands + 3 * level;

  if (comp->waveletTransform[level].height - 3 <=
          comp->waveletTransform[level].curLine &&
      !(comp->tileFlag & E_HAS_TILES_ON_THE_BOTTOM))
  {
    if (comp->waveletTransform[level].height & 1)
    {
      if (level)
      {
        if (crxIdwt53FilterDecode(comp, level - 1))
          return -1;
      }
      else if (crxDecodeLineWithIQuantization(sband))
        return -1;

      if (crxDecodeLineWithIQuantization(sband + 1))
        return -1;
    }
  }
  else
  {
    if (level)
    {
      if (crxIdwt53FilterDecode(comp, level - 1))
        return -1;
    }
    else if (crxDecodeLineWithIQuantization(sband)) // LL band
      return -1;

    if (crxDecodeLineWithIQuantization(sband + 1) || // HL band
        crxDecodeLineWithIQuantization(sband + 2) || // LH band
        crxDecodeLineWithIQuantization(sband + 3))   // HH band
      return -1;
  }

  return 0;
}

int crxIdwt53FilterTransform(CrxPlaneComp *comp, uint32_t level)
{
  CrxWaveletTransform *wavelet = comp->waveletTransform + level;

  if (wavelet->curH)
    return 0;

  if (wavelet->curLine >= wavelet->height - 3)
  {
    if (!(comp->tileFlag & E_HAS_TILES_ON_THE_BOTTOM))
    {
      if (wavelet->height & 1)
      {
        if (level)
        {
          if (!wavelet[-1].curH)
            if (crxIdwt53FilterTransform(comp, level - 1))
              return -1;
          wavelet->subband0Buf = crxIdwt53FilterGetLine(comp, level - 1);
        }
        int32_t *band0Buf = wavelet->subband0Buf;
        int32_t *band1Buf = wavelet->subband1Buf;
        int32_t *lineBufH0 = wavelet->lineBuf[wavelet->fltTapH + 3];
        int32_t *lineBufH1 = wavelet->lineBuf[(wavelet->fltTapH + 1) % 5 + 3];
        int32_t *lineBufH2 = wavelet->lineBuf[(wavelet->fltTapH + 2) % 5 + 3];

        int32_t *lineBufL0 = wavelet->lineBuf[0];
        int32_t *lineBufL1 = wavelet->lineBuf[1];
        wavelet->lineBuf[1] = wavelet->lineBuf[2];
        wavelet->lineBuf[2] = lineBufL1;

        // process L bands
        if (wavelet->width <= 1)
        {
          lineBufL0[0] = band0Buf[0];
        }
        else
        {
          if (comp->tileFlag & E_HAS_TILES_ON_THE_LEFT)
          {
            lineBufL0[0] = band0Buf[0] - ((band1Buf[0] + band1Buf[1] + 2) >> 2);
            ++band1Buf;
          }
          else
          {
            lineBufL0[0] = band0Buf[0] - ((band1Buf[0] + 1) >> 1);
          }
          ++band0Buf;
          for (int i = 0; i < wavelet->width - 3; i += 2)
          {
            int32_t delta =
                band0Buf[0] - ((band1Buf[0] + band1Buf[1] + 2) >> 2);
            lineBufL0[1] = band1Buf[0] + ((lineBufL0[0] + delta) >> 1);
            lineBufL0[2] = delta;
            ++band0Buf;
            ++band1Buf;
            lineBufL0 += 2;
          }
          if (comp->tileFlag & E_HAS_TILES_ON_THE_RIGHT)
          {
            int32_t delta =
                band0Buf[0] - ((band1Buf[0] + band1Buf[1] + 2) >> 2);
            lineBufL0[1] = band1Buf[0] + ((lineBufL0[0] + delta) >> 1);
            if (wavelet->width & 1)
              lineBufL0[2] = delta;
          }
          else if (wavelet->width & 1)
          {
            int32_t delta = band0Buf[0] - ((band1Buf[0] + 1) >> 1);
            lineBufL0[1] = band1Buf[0] + ((lineBufL0[0] + delta) >> 1);
            lineBufL0[2] = delta;
          }
          else
            lineBufL0[1] = band1Buf[0] + lineBufL0[0];
        }

        // process H bands
        lineBufL0 = wavelet->lineBuf[0];
        lineBufL1 = wavelet->lineBuf[1];
        for (int32_t i = 0; i < wavelet->width; i++)
        {
          int32_t delta = lineBufL0[i] - ((lineBufL1[i] + 1) >> 1);
          lineBufH1[i] = lineBufL1[i] + ((delta + lineBufH0[i]) >> 1);
          lineBufH2[i] = delta;
        }
        wavelet->curH += 3;
        wavelet->curLine += 3;
        wavelet->fltTapH = (wavelet->fltTapH + 3) % 5;
      }
      else
      {
        int32_t *lineBufL2 = wavelet->lineBuf[2];
        int32_t *lineBufH0 = wavelet->lineBuf[wavelet->fltTapH + 3];
        int32_t *lineBufH1 = wavelet->lineBuf[(wavelet->fltTapH + 1) % 5 + 3];
        wavelet->lineBuf[1] = lineBufL2;
        wavelet->lineBuf[2] = wavelet->lineBuf[1];

        for (int32_t i = 0; i < wavelet->width; i++)
          lineBufH1[i] = lineBufH0[i] + lineBufL2[i];

        wavelet->curH += 2;
        wavelet->curLine += 2;
        wavelet->fltTapH = (wavelet->fltTapH + 2) % 5;
      }
    }
  }
  else
  {
    if (level)
    {
      if (!wavelet[-1].curH && crxIdwt53FilterTransform(comp, level - 1))
        return -1;
      wavelet->subband0Buf = crxIdwt53FilterGetLine(comp, level - 1);
    }

    int32_t *band0Buf = wavelet->subband0Buf;
    int32_t *band1Buf = wavelet->subband1Buf;
    int32_t *band2Buf = wavelet->subband2Buf;
    int32_t *band3Buf = wavelet->subband3Buf;

    int32_t *lineBufL0 = wavelet->lineBuf[0];
    int32_t *lineBufL1 = wavelet->lineBuf[1];
    int32_t *lineBufL2 = wavelet->lineBuf[2];
    int32_t *lineBufH0 = wavelet->lineBuf[wavelet->fltTapH + 3];
    int32_t *lineBufH1 = wavelet->lineBuf[(wavelet->fltTapH + 1) % 5 + 3];
    int32_t *lineBufH2 = wavelet->lineBuf[(wavelet->fltTapH + 2) % 5 + 3];

    wavelet->lineBuf[1] = wavelet->lineBuf[2];
    wavelet->lineBuf[2] = lineBufL1;

    // process L bands
    if (wavelet->width <= 1)
    {
      lineBufL0[0] = band0Buf[0];
      lineBufL1[0] = band2Buf[0];
    }
    else
    {
      if (comp->tileFlag & E_HAS_TILES_ON_THE_LEFT)
      {
        lineBufL0[0] = band0Buf[0] - ((band1Buf[0] + band1Buf[1] + 2) >> 2);
        lineBufL1[0] = band2Buf[0] - ((band3Buf[0] + band3Buf[1] + 2) >> 2);
        ++band1Buf;
        ++band3Buf;
      }
      else
      {
        lineBufL0[0] = band0Buf[0] - ((band1Buf[0] + 1) >> 1);
        lineBufL1[0] = band2Buf[0] - ((band3Buf[0] + 1) >> 1);
      }
      ++band0Buf;
      ++band2Buf;
      for (int i = 0; i < wavelet->width - 3; i += 2)
      {
        int32_t delta = band0Buf[0] - ((band1Buf[0] + band1Buf[1] + 2) >> 2);
        lineBufL0[1] = band1Buf[0] + ((delta + lineBufL0[0]) >> 1);
        lineBufL0[2] = delta;

        delta = band2Buf[0] - ((band3Buf[0] + band3Buf[1] + 2) >> 2);
        lineBufL1[1] = band3Buf[0] + ((delta + lineBufL1[0]) >> 1);
        lineBufL1[2] = delta;

        ++band0Buf;
        ++band1Buf;
        ++band2Buf;
        ++band3Buf;
        lineBufL0 += 2;
        lineBufL1 += 2;
      }
      if (comp->tileFlag & E_HAS_TILES_ON_THE_RIGHT)
      {
        int32_t deltaA = band0Buf[0] - ((band1Buf[0] + band1Buf[1] + 2) >> 2);
        lineBufL0[1] = band1Buf[0] + ((deltaA + lineBufL0[0]) >> 1);

        int32_t deltaB = band2Buf[0] - ((band3Buf[0] + band3Buf[1] + 2) >> 2);
        lineBufL1[1] = band3Buf[0] + ((deltaB + lineBufL1[0]) >> 1);

        if (wavelet->width & 1)
        {
          lineBufL0[2] = deltaA;
          lineBufL1[2] = deltaB;
        }
      }
      else if (wavelet->width & 1)
      {
        int32_t delta = band0Buf[0] - ((band1Buf[0] + 1) >> 1);
        lineBufL0[1] = band1Buf[0] + ((delta + lineBufL0[0]) >> 1);
        lineBufL0[2] = delta;

        delta = band2Buf[0] - ((band3Buf[0] + 1) >> 1);
        lineBufL1[1] = band3Buf[0] + ((delta + lineBufL1[0]) >> 1);
        lineBufL1[2] = delta;
      }
      else
      {
        lineBufL0[1] = lineBufL0[0] + band1Buf[0];
        lineBufL1[1] = lineBufL1[0] + band3Buf[0];
      }
    }

    // process H bands
    lineBufL0 = wavelet->lineBuf[0];
    lineBufL1 = wavelet->lineBuf[1];
    lineBufL2 = wavelet->lineBuf[2];
    for (int32_t i = 0; i < wavelet->width; i++)
    {
      int32_t delta = lineBufL0[i] - ((lineBufL2[i] + lineBufL1[i] + 2) >> 2);
      lineBufH1[i] = lineBufL1[i] + ((delta + lineBufH0[i]) >> 1);
      lineBufH2[i] = delta;
    }
    if (wavelet->curLine >= wavelet->height - 3 && wavelet->height & 1)
    {
      wavelet->curH += 3;
      wavelet->curLine += 3;
      wavelet->fltTapH = (wavelet->fltTapH + 3) % 5;
    }
    else
    {
      wavelet->curH += 2;
      wavelet->curLine += 2;
      wavelet->fltTapH = (wavelet->fltTapH + 2) % 5;
    }
  }

  return 0;
}

int crxIdwt53FilterInitialize(CrxPlaneComp *comp, int32_t prevLevel)
{
  if (prevLevel < 0)
    return 0;

  for (int curLevel = 0, curBand = 0; curLevel < prevLevel + 1;
       curLevel++, curBand += 3)
  {
    CrxWaveletTransform *wavelet = comp->waveletTransform + curLevel;
    if (curLevel)
      wavelet[0].subband0Buf = crxIdwt53FilterGetLine(comp, curLevel - 1);
    else if (crxDecodeLineWithIQuantization(comp->subBands + curBand))
      return -1;

    int32_t *lineBufH0 = wavelet->lineBuf[wavelet->fltTapH + 3];
    if (wavelet->height > 1)
    {
      if (crxDecodeLineWithIQuantization(comp->subBands + curBand + 1) ||
          crxDecodeLineWithIQuantization(comp->subBands + curBand + 2) ||
          crxDecodeLineWithIQuantization(comp->subBands + curBand + 3))
        return -1;

      int32_t *lineBufL0 = wavelet->lineBuf[0];
      int32_t *lineBufL1 = wavelet->lineBuf[1];
      int32_t *lineBufL2 = wavelet->lineBuf[2];

      if (comp->tileFlag & E_HAS_TILES_ON_THE_TOP)
      {
        crxHorizontal53(lineBufL0, wavelet->lineBuf[1], wavelet,
                        comp->tileFlag);
        if (crxDecodeLineWithIQuantization(comp->subBands + curBand + 3) ||
            crxDecodeLineWithIQuantization(comp->subBands + curBand + 2))
          return -1;

        int32_t *band2Buf = wavelet->subband2Buf;
        int32_t *band3Buf = wavelet->subband3Buf;

        // process L band
        if (wavelet->width <= 1)
          lineBufL2[0] = band2Buf[0];
        else
        {
          if (comp->tileFlag & E_HAS_TILES_ON_THE_LEFT)
          {
            lineBufL2[0] = band2Buf[0] - ((band3Buf[0] + band3Buf[1] + 2) >> 2);
            ++band3Buf;
          }
          else
            lineBufL2[0] = band2Buf[0] - ((band3Buf[0] + 1) >> 1);

          ++band2Buf;

          for (int i = 0; i < wavelet->width - 3; i += 2)
          {
            int32_t delta =
                band2Buf[0] - ((band3Buf[0] + band3Buf[1] + 2) >> 2);
            lineBufL2[1] = band3Buf[0] + ((lineBufL2[0] + delta) >> 1);
            lineBufL2[2] = delta;

            ++band2Buf;
            ++band3Buf;
            lineBufL2 += 2;
          }
          if (comp->tileFlag & E_HAS_TILES_ON_THE_RIGHT)
          {
            int32_t delta =
                band2Buf[0] - ((band3Buf[0] + band3Buf[1] + 2) >> 2);
            lineBufL2[1] = band3Buf[0] + ((lineBufL2[0] + delta) >> 1);
            if (wavelet->width & 1)
              lineBufL2[2] = delta;
          }
          else if (wavelet->width & 1)
          {
            int32_t delta = band2Buf[0] - ((band3Buf[0] + 1) >> 1);

            lineBufL2[1] = band3Buf[0] + ((lineBufL2[0] + delta) >> 1);
            lineBufL2[2] = delta;
          }
          else
          {
            lineBufL2[1] = band3Buf[0] + lineBufL2[0];
          }
        }

        // process H band
        for (int32_t i = 0; i < wavelet->width; i++)
          lineBufH0[i] =
              lineBufL0[i] - ((lineBufL1[i] + lineBufL2[i] + 2) >> 2);
      }
      else
      {
        crxHorizontal53(lineBufL0, wavelet->lineBuf[2], wavelet,
                        comp->tileFlag);
        for (int i = 0; i < wavelet->width; i++)
          lineBufH0[i] = lineBufL0[i] - ((lineBufL2[i] + 1) >> 1);
      }

      if (crxIdwt53FilterDecode(comp, curLevel) ||
          crxIdwt53FilterTransform(comp, curLevel))
        return -1;
    }
    else
    {
      if (crxDecodeLineWithIQuantization(comp->subBands + curBand + 1))
        return -1;

      int32_t *band0Buf = wavelet->subband0Buf;
      int32_t *band1Buf = wavelet->subband1Buf;

      // process H band
      if (wavelet->width <= 1)
        lineBufH0[0] = band0Buf[0];
      else
      {
        if (comp->tileFlag & E_HAS_TILES_ON_THE_LEFT)
        {
          lineBufH0[0] = band0Buf[0] - ((band1Buf[0] + band1Buf[1] + 2) >> 2);
          ++band1Buf;
        }
        else
          lineBufH0[0] = band0Buf[0] - ((band1Buf[0] + 1) >> 1);

        ++band0Buf;

        for (int i = 0; i < wavelet->width - 3; i += 2)
        {
          int32_t delta = band0Buf[0] - ((band1Buf[0] + band1Buf[1] + 2) >> 2);
          lineBufH0[1] = band1Buf[0] + ((lineBufH0[0] + delta) >> 1);
          lineBufH0[2] = delta;

          ++band0Buf;
          ++band1Buf;
          lineBufH0 += 2;
        }

        if (comp->tileFlag & E_HAS_TILES_ON_THE_RIGHT)
        {
          int32_t delta = band0Buf[0] - ((band1Buf[0] + band1Buf[1] + 2) >> 2);
          lineBufH0[1] = band1Buf[0] + ((lineBufH0[0] + delta) >> 1);
          lineBufH0[2] = delta;
        }
        else if (wavelet->width & 1)
        {
          int32_t delta = band0Buf[0] - ((band1Buf[0] + 1) >> 1);
          lineBufH0[1] = band1Buf[0] + ((lineBufH0[0] + delta) >> 1);
          lineBufH0[2] = delta;
        }
        else
        {
          lineBufH0[1] = band1Buf[0] + lineBufH0[0];
        }
      }
      ++wavelet->curLine;
      ++wavelet->curH;
      wavelet->fltTapH = (wavelet->fltTapH + 1) % 5;
    }
  }

  return 0;
}

void crxFreeSubbandData(CrxImage *image, CrxPlaneComp *comp)
{
  if (comp->compBuf)
  {
    free(comp->compBuf);
    comp->compBuf = 0;
  }

  if (!comp->subBands)
    return;

  for (int32_t i = 0; i < image->subbandCount; i++)
  {
    if (comp->subBands[i].bandParam)
    {
      free(comp->subBands[i].bandParam);
      comp->subBands[i].bandParam = 0LL;
    }
    comp->subBands[i].bandBuf = 0;
    comp->subBands[i].bandSize = 0;
  }
}

void crxConvertPlaneLine(CrxImage *img, int imageRow, int imageCol = 0,
                         int plane = 0, int32_t *lineData = 0,
                         int lineLength = 0)
{
  if (lineData)
  {
    uint64_t rawOffset = 4 * img->planeWidth * imageRow + 2 * imageCol;
    if (img->encType == 1)
    {
      int32_t maxVal = 1 << (img->nBits - 1);
      int32_t minVal = -maxVal;
      --maxVal;
      for (int i = 0; i < lineLength; i++)
        img->outBufs[plane][rawOffset + 2 * i] =
            _constrain(lineData[i], minVal, maxVal);
    }
    else if (img->encType == 3)
    {
      // copy to intermediate planeBuf
      rawOffset = plane * img->planeWidth * img->planeHeight +
                  img->planeWidth * imageRow + imageCol;
      for (int i = 0; i < lineLength; i++)
        img->planeBuf[rawOffset + i] = lineData[i];
    }
    else if (img->nPlanes == 4)
    {
      int32_t median = 1 << (img->nBits - 1);
      int32_t maxVal = (1 << img->nBits) - 1;
      for (int i = 0; i < lineLength; i++)
        img->outBufs[plane][rawOffset + 2 * i] =
            _constrain(median + lineData[i], 0, maxVal);
    }
    else if (img->nPlanes == 1)
    {
      int32_t maxVal = (1 << img->nBits) - 1;
      int32_t median = 1 << (img->nBits - 1);
      rawOffset = img->planeWidth * imageRow + imageCol;
      for (int i = 0; i < lineLength; i++)
        img->outBufs[0][rawOffset + i] =
            _constrain(median + lineData[i], 0, maxVal);
    }
  }
  else if (img->encType == 3 && img->planeBuf)
  {
    int32_t planeSize = img->planeWidth * img->planeHeight;
    int16_t *plane0 = img->planeBuf + imageRow * img->planeWidth;
    int16_t *plane1 = plane0 + planeSize;
    int16_t *plane2 = plane1 + planeSize;
    int16_t *plane3 = plane2 + planeSize;

    int32_t median = 1 << (img->nBits - 1) << 10;
    int32_t maxVal = (1 << img->nBits) - 1;
    uint32_t rawLineOffset = 4 * img->planeWidth * imageRow;

    // for this stage - all except imageRow is ignored
    for (int i = 0; i < img->planeWidth; i++)
    {
      int32_t gr =
          median + (plane0[i] << 10) - 168 * plane1[i] - 585 * plane3[i];
      int32_t val = 0;
      if (gr < 0)
        gr = -(((_abs(gr) + 512) >> 9) & ~1);
      else
        gr = ((_abs(gr) + 512) >> 9) & ~1;

      // Essentially R = round(median + P0 + 1.474*P3)
      val = (median + (plane0[i] << 10) + 1510 * plane3[i] + 512) >> 10;
      img->outBufs[0][rawLineOffset + 2 * i] = _constrain(val, 0, maxVal);
      // Essentially G1 = round(median + P0 + P2 - 0.164*P1 - 0.571*P3)
      val = (plane2[i] + gr + 1) >> 1;
      img->outBufs[1][rawLineOffset + 2 * i] = _constrain(val, 0, maxVal);
      // Essentially G1 = round(median + P0 - P2 - 0.164*P1 - 0.571*P3)
      val = (gr - plane2[i] + 1) >> 1;
      img->outBufs[2][rawLineOffset + 2 * i] = _constrain(val, 0, maxVal);
      // Essentially B = round(median + P0 + 1.881*P1)
      val = (median + (plane0[i] << 10) + 1927 * plane1[i] + 512) >> 10;
      img->outBufs[3][rawLineOffset + 2 * i] = _constrain(val, 0, maxVal);
    }
  }
}

int crxParamInit(
#ifdef LIBRAW_CR3_MEMPOOL
	libraw_memmgr&  mm,
#endif	
	CrxBandParam **param, uint64_t subbandMdatOffset,
                 uint64_t subbandDataSize, uint32_t subbandWidth,
                 uint32_t subbandHeight, int32_t supportsPartial,
                 uint32_t roundedBitsMask, LibRaw_abstract_datastream *input)
{
  int32_t progrDataSize = supportsPartial ? 0 : sizeof(int32_t) * subbandWidth;
  int32_t paramLength = 2 * subbandWidth + 4;
  uint8_t *paramBuf = (uint8_t *)
#ifdef LIBRAW_CR3_MEMPOOL
	  mm.
#endif
	  calloc(
      1, sizeof(CrxBandParam) + sizeof(int32_t) * paramLength + progrDataSize);

  if (!paramBuf)
    return -1;

  *param = (CrxBandParam *)paramBuf;

  paramBuf += sizeof(CrxBandParam);

  (*param)->paramData = (int32_t *)paramBuf;
  (*param)->nonProgrData =
      progrDataSize ? (*param)->paramData + paramLength : 0;
  (*param)->subbandWidth = subbandWidth;
  (*param)->subbandHeight = subbandHeight;
  (*param)->roundedBits = 0;
  (*param)->curLine = 0;
  (*param)->roundedBitsMask = roundedBitsMask;
  (*param)->supportsPartial = supportsPartial;
  (*param)->bitStream.bitData = 0;
  (*param)->bitStream.bitsLeft = 0;
  (*param)->bitStream.mdatSize = subbandDataSize;
  (*param)->bitStream.curPos = 0;
  (*param)->bitStream.curBufSize = 0;
  (*param)->bitStream.curBufOffset = subbandMdatOffset;
  (*param)->bitStream.input = input;

  crxFillBuffer(&(*param)->bitStream);

  return 0;
}

int crxSetupSubbandData(CrxImage *img, CrxPlaneComp *planeComp,
                        const CrxTile *tile, uint32_t mdatOffset)
{
  long compDataSize = 0;
  long waveletDataOffset = 0;
  long compCoeffDataOffset = 0;
  int32_t toSubbands = 3 * img->levels + 1;
  int32_t transformWidth = 0;

  CrxSubband *subbands = planeComp->subBands;

  // calculate sizes
  for (int32_t subbandNum = 0; subbandNum < toSubbands; subbandNum++)
  {
    subbands[subbandNum].bandSize =
        subbands[subbandNum].width * sizeof(int32_t); // 4bytes
    compDataSize += subbands[subbandNum].bandSize;
  }

  if (img->levels)
  {
    int32_t encLevels = img->levels ? img->levels : 1;
    waveletDataOffset = (compDataSize + 7) & ~7;
    compDataSize =
        (sizeof(CrxWaveletTransform) * encLevels + waveletDataOffset + 7) & ~7;
    compCoeffDataOffset = compDataSize;

    // calc wavelet line buffer sizes (always at one level up from current)
    for (int level = 0; level < img->levels; ++level)
      if (level < img->levels - 1)
        compDataSize += 8 * sizeof(int32_t) *
                        planeComp->subBands[3 * (level + 1) + 2].width;
      else
        compDataSize += 8 * sizeof(int32_t) * tile->width;
  }

  // buffer allocation
  planeComp->compBuf = (uint8_t *)
#ifdef LIBRAW_CR3_MEMPOOL
	  img->memmgr.
#endif
	  malloc(compDataSize);
  if (!planeComp->compBuf)
    return -1;

  // subbands buffer and sizes initialisation
  uint64_t subbandMdatOffset = img->mdatOffset + mdatOffset;
  uint8_t *subbandBuf = planeComp->compBuf;

  for (int32_t subbandNum = 0; subbandNum < toSubbands; subbandNum++)
  {
    subbands[subbandNum].bandBuf = subbandBuf;
    subbandBuf += subbands[subbandNum].bandSize;
    subbands[subbandNum].mdatOffset =
        subbandMdatOffset + subbands[subbandNum].dataOffset;
  }

  // wavelet data initialisation
  if (img->levels)
  {
    CrxWaveletTransform *waveletTransforms =
        (CrxWaveletTransform *)(planeComp->compBuf + waveletDataOffset);
    int32_t *paramData = (int32_t *)(planeComp->compBuf + compCoeffDataOffset);

    planeComp->waveletTransform = waveletTransforms;
    waveletTransforms[0].subband0Buf = (int32_t *)subbands->bandBuf;

    for (int level = 0; level < img->levels; ++level)
    {
      int32_t band = 3 * level + 1;

      if (level >= img->levels - 1)
      {
        waveletTransforms[level].height = tile->height;
        transformWidth = tile->width;
      }
      else
      {
        waveletTransforms[level].height = subbands[band + 3].height;
        transformWidth = subbands[band + 4].width;
      }
      waveletTransforms[level].width = transformWidth;
      waveletTransforms[level].lineBuf[0] = paramData;
      waveletTransforms[level].lineBuf[1] =
          waveletTransforms[level].lineBuf[0] + transformWidth;
      waveletTransforms[level].lineBuf[2] =
          waveletTransforms[level].lineBuf[1] + transformWidth;
      waveletTransforms[level].lineBuf[3] =
          waveletTransforms[level].lineBuf[2] + transformWidth;
      waveletTransforms[level].lineBuf[4] =
          waveletTransforms[level].lineBuf[3] + transformWidth;
      waveletTransforms[level].lineBuf[5] =
          waveletTransforms[level].lineBuf[4] + transformWidth;
      waveletTransforms[level].lineBuf[6] =
          waveletTransforms[level].lineBuf[5] + transformWidth;
      waveletTransforms[level].lineBuf[7] =
          waveletTransforms[level].lineBuf[6] + transformWidth;
      waveletTransforms[level].curLine = 0;
      waveletTransforms[level].curH = 0;
      waveletTransforms[level].fltTapH = 0;
      waveletTransforms[level].subband1Buf = (int32_t *)subbands[band].bandBuf;
      waveletTransforms[level].subband2Buf =
          (int32_t *)subbands[band + 1].bandBuf;
      waveletTransforms[level].subband3Buf =
          (int32_t *)subbands[band + 2].bandBuf;

      paramData = waveletTransforms[level].lineBuf[7] + transformWidth;
    }
  }

  // decoding params and bitstream initialisation
  for (int32_t subbandNum = 0; subbandNum < toSubbands; subbandNum++)
  {
    if (subbands[subbandNum].dataSize)
    {
      int32_t supportsPartial = 0;
      uint32_t roundedBitsMask = 0;

      if (planeComp->supportsPartial && subbandNum == 0)
      {
        roundedBitsMask = planeComp->roundedBitsMask;
        supportsPartial = 1;
      }
      if (crxParamInit(
#ifdef LIBRAW_CR3_MEMPOOL
		  img->memmgr,
#endif
		  &subbands[subbandNum].bandParam,
                       subbands[subbandNum].mdatOffset,
                       subbands[subbandNum].dataSize,
                       subbands[subbandNum].width, subbands[subbandNum].height,
                       supportsPartial, roundedBitsMask, img->input))
        return -1;
    }
  }

  return 0;
}

int LibRaw::crxDecodePlane(void *p, uint32_t planeNumber)
{
  CrxImage *img = (CrxImage *)p;
  int imageRow = 0;
  for (int tRow = 0; tRow < img->tileRows; tRow++)
  {
    int imageCol = 0;
    for (int tCol = 0; tCol < img->tileCols; tCol++)
    {
      CrxTile *tile = img->tiles + tRow * img->tileCols + tCol;
      CrxPlaneComp *planeComp = tile->comps + planeNumber;
      uint64_t tileMdatOffset = tile->dataOffset + planeComp->dataOffset;

      // decode single tile
      if (crxSetupSubbandData(img, planeComp, tile, tileMdatOffset))
        return -1;

      if (img->levels)
      {
        if (crxIdwt53FilterInitialize(planeComp, img->levels - 1))
          return -1;
        for (int i = 0; i < tile->height; ++i)
        {
          if (crxIdwt53FilterDecode(planeComp, img->levels - 1) ||
              crxIdwt53FilterTransform(planeComp, img->levels - 1))
            return -1;
          int32_t *lineData =
              crxIdwt53FilterGetLine(planeComp, img->levels - 1);
          crxConvertPlaneLine(img, imageRow + i, imageCol, planeNumber,
                              lineData, tile->width);
        }
      }
      else
      {
        // we have the only subband in this case
        if (!planeComp->subBands->dataSize)
        {
          memset(planeComp->subBands->bandBuf, 0,
                 planeComp->subBands->bandSize);
          return 0;
        }

        for (int i = 0; i < tile->height; ++i)
        {
          if (crxDecodeLine(planeComp->subBands->bandParam,
                            planeComp->subBands->bandBuf))
            return -1;
          int32_t *lineData = (int32_t *)planeComp->subBands->bandBuf;
          crxConvertPlaneLine(img, imageRow + i, imageCol, planeNumber,
                              lineData, tile->width);
        }
      }
      imageCol += tile->width;
    }
    imageRow += img->tiles[tRow * img->tileCols].height;
  }

  return 0;
}

int crxReadSubbandHeaders(crx_data_header_t *hdr, CrxImage *img, CrxTile *tile,
                          CrxPlaneComp *comp, uint8_t **subbandMdatPtr,
                          int32_t *hdrSize)
{
  CrxSubband *band = comp->subBands + img->subbandCount - 1; // set to last band
  uint32_t bandHeight = tile->height;
  uint32_t bandWidth = tile->width;
  int32_t bandWidthExCoef = 0;
  int32_t bandHeightExCoef = 0;
  if (img->levels)
  {
    // Build up subband sequences to crxDecode to a level in a header

    // Coefficient structure is a bit unclear and convoluted:
    //   3 levels max - 8 groups (for tile width rounded to 8 bytes)
    //                  of 3 band per level 4 sets of coefficients for each
    int32_t *rowExCoef =
        exCoefNumTbl + 0x60 * (img->levels - 1) + 12 * (tile->width & 7);
    int32_t *colExCoef =
        exCoefNumTbl + 0x60 * (img->levels - 1) + 12 * (tile->height & 7);
    for (int level = 0; level < img->levels; ++level)
    {
      int32_t widthOddPixel = bandWidth & 1;
      int32_t heightOddPixel = bandHeight & 1;
      bandWidth = (widthOddPixel + bandWidth) >> 1;
      bandHeight = (heightOddPixel + bandHeight) >> 1;

      int32_t bandWidthExCoef0 = 0;
      int32_t bandWidthExCoef1 = 0;
      int32_t bandHeightExCoef0 = 0;
      int32_t bandHeightExCoef1 = 0;
      if (tile->tileFlag & E_HAS_TILES_ON_THE_RIGHT)
      {
        bandWidthExCoef0 = rowExCoef[0];
        bandWidthExCoef1 = rowExCoef[1];
      }
      if (tile->tileFlag & E_HAS_TILES_ON_THE_LEFT)
        ++bandWidthExCoef0;
      if (tile->tileFlag & E_HAS_TILES_ON_THE_BOTTOM)
      {
        bandHeightExCoef0 = colExCoef[0];
        bandHeightExCoef1 = colExCoef[1];
      }
      if (tile->tileFlag & E_HAS_TILES_ON_THE_TOP)
        ++bandHeightExCoef0;

      band[0].width = bandWidth + bandWidthExCoef0 - widthOddPixel;
      band[0].height = bandHeight + bandHeightExCoef0 - heightOddPixel;

      band[-1].width = bandWidth + bandWidthExCoef1;
      band[-1].height = bandHeight + bandHeightExCoef0 - heightOddPixel;

      band[-2].width = bandWidth + bandWidthExCoef0 - widthOddPixel;
      band[-2].height = bandHeight + bandHeightExCoef1;

      rowExCoef += 4;
      colExCoef += 4;
      band -= 3;
    }
    bandWidthExCoef = bandHeightExCoef = 0;
    if (tile->tileFlag & E_HAS_TILES_ON_THE_RIGHT)
      bandWidthExCoef =
          exCoefNumTbl[0x60 * (img->levels - 1) + 12 * (tile->width & 7) +
                       4 * (img->levels - 1) + 1];
    if (tile->tileFlag & E_HAS_TILES_ON_THE_BOTTOM)
      bandHeightExCoef =
          exCoefNumTbl[0x60 * (img->levels - 1) + 12 * (tile->height & 7) +
                       4 * (img->levels - 1) + 1];
  }
  band->width = bandWidthExCoef + bandWidth;
  band->height = bandHeightExCoef + bandHeight;

  if (!img->subbandCount)
    return 0;
  int32_t subbandOffset = 0;
  band = comp->subBands;
  for (int curSubband = 0; curSubband < img->subbandCount; curSubband++, band++)
  {
    if (*hdrSize < 0xC)
      return -1;

    if (LibRaw::sgetn(2, *subbandMdatPtr) != 0xFF03)
      return -1;

    uint32_t bitData = LibRaw::sgetn(4, *subbandMdatPtr + 8);
    uint32_t subbandSize = LibRaw::sgetn(4, *subbandMdatPtr + 4);

    if ((unsigned)curSubband != bitData >> 28)
    {
      band->dataSize = subbandSize;
      return -1;
    }
    band->dataSize = subbandSize - (bitData & 0x7FF);
    band->supportsPartial = bitData & 0x8000 ? 1 : 0;
    band->dataOffset = subbandOffset;
    band->quantValue = (bitData >> 19) & 0xFF;
    band->paramK = 0;
    band->bandParam = 0;
    band->bandBuf = 0;
    band->bandSize = 0;

    subbandOffset += subbandSize;

    *subbandMdatPtr += 0xC;
    *hdrSize -= 0xC;
  }
  return 0;
}

int crxReadImageHeaders(crx_data_header_t *hdr, CrxImage *img, uint8_t *mdatPtr,
                        int32_t hdrBufSize)
{
  int nTiles = img->tileRows * img->tileCols;

  if (!nTiles)
    return -1;

  if (!img->tiles)
  {
    img->tiles = (CrxTile *)
#ifdef LIBRAW_CR3_MEMPOOL
		img->memmgr.
#endif
		calloc(
        sizeof(CrxTile) * nTiles +
        sizeof(CrxPlaneComp) * nTiles * img->nPlanes +
        sizeof(CrxSubband) * nTiles * img->nPlanes * img->subbandCount,1);
    if (!img->tiles)
      return -1;

    // memory areas in allocated chunk
    CrxTile *tile = img->tiles;
    CrxPlaneComp *comps = (CrxPlaneComp *)(tile + nTiles);
    CrxSubband *bands = (CrxSubband *)(comps + img->nPlanes * nTiles);

    for (int curTile = 0; curTile < nTiles; curTile++, tile++)
    {
      tile->tileFlag = 0; // tile neighbouring flags
      tile->tileNumber = curTile;
      tile->tileSize = 0;
      tile->comps = comps + curTile * img->nPlanes;

      if ((curTile + 1) % img->tileCols)
      {
        // not the last tile in a tile row
        tile->width = hdr->tileWidth;
        if (img->tileCols > 1)
        {
          tile->tileFlag = E_HAS_TILES_ON_THE_RIGHT;
          if (curTile % img->tileCols)
            // not the first tile in tile row
            tile->tileFlag |= E_HAS_TILES_ON_THE_LEFT;
        }
      }
      else
      {
        // last tile in a tile row
        tile->width = img->planeWidth - hdr->tileWidth * (img->tileCols - 1);
        if (img->tileCols > 1)
          tile->tileFlag = E_HAS_TILES_ON_THE_LEFT;
      }
      if (curTile < nTiles - img->tileCols)
      {
        // in first tile row
        tile->height = hdr->tileHeight;
        if (img->tileRows > 1)
        {
          tile->tileFlag |= E_HAS_TILES_ON_THE_BOTTOM;
          if (curTile >= img->tileCols)
            tile->tileFlag |= E_HAS_TILES_ON_THE_TOP;
        }
      }
      else
      {
        // non first tile row
        tile->height = img->planeHeight - hdr->tileHeight * (img->tileRows - 1);
        if (img->tileRows > 1)
          tile->tileFlag |= E_HAS_TILES_ON_THE_TOP;
      }
      if (img->nPlanes)
      {
        CrxPlaneComp *comp = tile->comps;
        CrxSubband *band = bands + curTile * img->nPlanes * img->subbandCount;

        for (int curComp = 0; curComp < img->nPlanes; curComp++, comp++)
        {
          comp->compNumber = curComp;
          comp->supportsPartial = 1;
          comp->tileFlag = tile->tileFlag;
          comp->subBands = band;
          comp->compBuf = 0;
          comp->waveletTransform = 0;
          if (img->subbandCount)
          {
            for (int curBand = 0; curBand < img->subbandCount;
                 curBand++, band++)
            {
              band->supportsPartial = 0;
              band->quantValue = 4;
              band->bandParam = 0;
              band->dataSize = 0;
            }
          }
        }
      }
    }
  }

  uint32_t tileOffset = 0;
  int32_t dataSize = hdrBufSize;
  uint8_t *dataPtr = mdatPtr;
  CrxTile *tile = img->tiles;

  for (int curTile = 0; curTile < nTiles; curTile++, tile++)
  {
    if (dataSize < 0xC)
      return -1;

    if (LibRaw::sgetn(2, dataPtr) != 0xFF01)
      return -1;
    if (LibRaw::sgetn(2, dataPtr + 8) != (unsigned)curTile)
      return -1;

    dataSize -= 0xC;

    tile->tileSize = LibRaw::sgetn(4, dataPtr + 4);
    tile->dataOffset = tileOffset;

    int32_t hdrExtraBytes = LibRaw::sgetn(2, dataPtr + 2) - 8;
    tileOffset += tile->tileSize;
    dataPtr += hdrExtraBytes + 0xC;
    dataSize -= hdrExtraBytes;

    uint32_t compOffset = 0;
    CrxPlaneComp *comp = tile->comps;

    for (int compNum = 0; compNum < img->nPlanes; compNum++, comp++)
    {
      if (dataSize < 0xC)
        return -1;

      if (LibRaw::sgetn(2, dataPtr) != 0xFF02)
        return -1;
      if (compNum != dataPtr[8] >> 4)
        return -1;

      comp->compSize = LibRaw::sgetn(4, dataPtr + 4);

      int32_t compHdrRoundedBits = (dataPtr[8] >> 1) & 3;
      comp->supportsPartial = (dataPtr[8] & 8) != 0;

      comp->dataOffset = compOffset;
      comp->tileFlag = tile->tileFlag;

      compOffset += comp->compSize;
	  dataSize -= 0xC;
	  dataPtr += 0xC;

      comp->roundedBitsMask = 0;

      if (compHdrRoundedBits)
      {
        if (img->levels || !comp->supportsPartial)
          return -1;

        comp->roundedBitsMask = 1 << (compHdrRoundedBits - 1);
      }

      if (crxReadSubbandHeaders(hdr, img, tile, comp, &dataPtr, &dataSize))
        return -1;
    }
  }
  return 0;
}

int crxSetupImageData(crx_data_header_t *hdr, CrxImage *img, int16_t *outBuf,
                      uint64_t mdatOffset, uint32_t mdatSize, int32_t hdrBufSize,
                      uint8_t *mdatHdrPtr)
{
  int IncrBitTable[32] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0,
                          0, 0, 1, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0};

  img->planeWidth = hdr->f_width;
  img->planeHeight = hdr->f_height;

  if (hdr->tileWidth < 0x16 || hdr->tileHeight < 0x16 ||
      img->planeWidth > 0x7FFF || img->planeHeight > 0x7FFF)
    return -1;

  img->tileCols = (img->planeWidth + hdr->tileWidth - 1) / hdr->tileWidth;
  img->tileRows = (img->planeHeight + hdr->tileHeight - 1) / hdr->tileHeight;

  if (img->tileCols > 0xFF || img->tileRows > 0xFF ||
      img->planeWidth - hdr->tileWidth * (img->tileCols - 1) < 0x16 ||
      img->planeHeight - hdr->tileHeight * (img->tileRows - 1) < 0x16)
    return -1;

  img->tiles = 0;
  img->levels = hdr->imageLevels;
  img->subbandCount = 3 * img->levels + 1; // 3 bands per level + one last LL
  img->nPlanes = hdr->nPlanes;
  img->nBits = hdr->nBits;
  img->encType = hdr->encType;
  img->samplePrecision = hdr->nBits + IncrBitTable[4 * hdr->encType + 2] + 1;
  img->mdatOffset = mdatOffset + hdr->mdatHdrSize;
  img->mdatSize = mdatSize;
  img->planeBuf = 0;
  img->outBufs[0] = img->outBufs[1] = img->outBufs[2] = img->outBufs[3] = 0;

  // The encoding type 3 needs all 4 planes to be decoded to generate row of
  // RGGB values. It seems to be using some other colour space for raw encoding
  // It is a massive buffer so ideallly it will need a different approach:
  // decode planes line by line and convert single line then without
  // intermediate plane buffer. At the moment though it's too many changes so
  // left as is.
  if (img->encType == 3 && img->nPlanes == 4 && img->nBits > 8)
  {
    img->planeBuf =
        (int16_t *)
#ifdef LIBRAW_CR3_MEMPOOL
		img->memmgr.
#endif
		malloc(img->planeHeight * img->planeWidth * img->nPlanes *
                          ((img->samplePrecision + 7) >> 3));
    if (!img->planeBuf)
      return -1;
  }

  int32_t rowSize = 2 * img->planeWidth;

  if (img->nPlanes == 1)
    img->outBufs[0] = outBuf;
  else
    switch (hdr->cfaLayout)
    {
    case 0:
      // R G
      // G B
      img->outBufs[0] = outBuf;
      img->outBufs[1] = outBuf + 1;
      img->outBufs[2] = outBuf + rowSize;
      img->outBufs[3] = img->outBufs[2] + 1;
      break;
    case 1:
      // G R
      // B G
      img->outBufs[1] = outBuf;
      img->outBufs[0] = outBuf + 1;
      img->outBufs[3] = outBuf + rowSize;
      img->outBufs[2] = img->outBufs[3] + 1;
      break;
    case 2:
      // G B
      // R G
      img->outBufs[2] = outBuf;
      img->outBufs[3] = outBuf + 1;
      img->outBufs[0] = outBuf + rowSize;
      img->outBufs[1] = img->outBufs[0] + 1;
      break;
    case 3:
      // B G
      // G R
      img->outBufs[3] = outBuf;
      img->outBufs[2] = outBuf + 1;
      img->outBufs[1] = outBuf + rowSize;
      img->outBufs[0] = img->outBufs[1] + 1;
      break;
    }

  // read header
  return crxReadImageHeaders(hdr, img, mdatHdrPtr, hdrBufSize);
}

int crxFreeImageData(CrxImage *img)
{
#ifdef LIBRAW_CR3_MEMPOOL
	img->memmgr.cleanup();
#else
  CrxTile *tile = img->tiles;
  int nTiles = img->tileRows * img->tileCols;

  if (img->tiles)
  {
    for (int32_t curTile = 0; curTile < nTiles; curTile++)
      if (tile[curTile].comps)
        for (int32_t curPlane = 0; curPlane < img->nPlanes; curPlane++)
          crxFreeSubbandData(img, tile[curTile].comps + curPlane);
    free(img->tiles);
    img->tiles = 0;
  }

  if (img->planeBuf)
  {
    free(img->planeBuf);
    img->planeBuf = 0;
  }
#endif
  return 0;
}

void LibRaw::crxLoadDecodeLoop(void *img, int nPlanes)
{
#ifdef LIBRAW_USE_OPENMP
  int results[4]; // nPlanes is always <= 4
#pragma omp parallel for
  for (int32_t plane = 0; plane < nPlanes; ++plane)
    results[plane] = crxDecodePlane(img, plane);

  for (int32_t plane = 0; plane < nPlanes; ++plane)
    if (results[plane])
      derror();
#else
  for (int32_t plane = 0; plane < nPlanes; ++plane)
    if (crxDecodePlane(img, plane))
      derror();
#endif
}

void LibRaw::crxConvertPlaneLineDf(void *p, int imageRow)
{
  crxConvertPlaneLine((CrxImage *)p, imageRow);
}

void LibRaw::crxLoadFinalizeLoopE3(void *p, int planeHeight)
{
#ifdef LIBRAW_USE_OPENMP
#pragma omp parallel for
#endif
  for (int i = 0; i < planeHeight; ++i)
    crxConvertPlaneLineDf(p, i);
}

void LibRaw::crxLoadRaw()
{
	if (libraw_internal_data.unpacker_data.CR3_Version != 0x100)
		throw LIBRAW_EXCEPTION_DECODE_RAW;
  CrxImage img;
  if (libraw_internal_data.unpacker_data.crx_track_selected < 0 ||
      libraw_internal_data.unpacker_data.crx_track_selected >=
          LIBRAW_CRXTRACKS_MAXCOUNT)
    derror();
  crx_data_header_t hdr =
      libraw_internal_data.unpacker_data
          .crx_header[libraw_internal_data.unpacker_data.crx_track_selected];

  img.input = libraw_internal_data.internal_data.input;

  // update sizes for the planes
  if (hdr.nPlanes == 4)
  {
    hdr.f_width >>= 1;
    hdr.f_height >>= 1;
    hdr.tileWidth >>= 1;
    hdr.tileHeight >>= 1;
  }

  imgdata.color.maximum = (1 << hdr.nBits) - 1;

  uint8_t *hdrBuf = (uint8_t *)malloc(hdr.mdatHdrSize * 2);

  // read image header
#ifdef LIBRAW_USE_OPENMP
#pragma omp critical
#endif
  {
#ifndef LIBRAW_USE_OPENMP
    libraw_internal_data.internal_data.input->lock();
#endif
    libraw_internal_data.internal_data.input->seek(
        libraw_internal_data.unpacker_data.data_offset, SEEK_SET);
    libraw_internal_data.internal_data.input->read(hdrBuf, 1, hdr.mdatHdrSize);
#ifndef LIBRAW_USE_OPENMP
    libraw_internal_data.internal_data.input->unlock();
#endif
  }

  // parse and setup the image data
  if (crxSetupImageData(&hdr, &img, (int16_t *)imgdata.rawdata.raw_image,
                        libraw_internal_data.unpacker_data.data_offset,
                        libraw_internal_data.unpacker_data.data_size, hdr.mdatHdrSize*2, hdrBuf))
    derror();
  free(hdrBuf);

  crxLoadDecodeLoop(&img, hdr.nPlanes);

  if (img.encType == 3)
    crxLoadFinalizeLoopE3(&img, img.planeHeight);

  crxFreeImageData(&img);
}

int LibRaw::crxParseImageHeader(uchar *cmp1TagData, int nTrack)
{
  if (nTrack < 0 || nTrack >= LIBRAW_CRXTRACKS_MAXCOUNT)
    return -1;
  if (!cmp1TagData)
    return -1;

  crx_data_header_t *hdr =
      &libraw_internal_data.unpacker_data.crx_header[nTrack];

  hdr->version = sgetn(2, cmp1TagData + 4);
  hdr->f_width = sgetn(4, cmp1TagData + 8);
  hdr->f_height = sgetn(4, cmp1TagData + 12);
  hdr->tileWidth = sgetn(4, cmp1TagData + 16);
  hdr->tileHeight = sgetn(4, cmp1TagData + 20);
  hdr->nBits = cmp1TagData[24];
  hdr->nPlanes = cmp1TagData[25] >> 4;
  hdr->cfaLayout = cmp1TagData[25] & 0xF;
  hdr->encType = cmp1TagData[26] >> 4;
  hdr->imageLevels = cmp1TagData[26] & 0xF;
  hdr->hasTileCols = cmp1TagData[27] >> 7;
  hdr->hasTileRows = (cmp1TagData[27] >> 6) & 1;
  hdr->mdatHdrSize = sgetn(4, cmp1TagData + 28);

  // validation
  if ((hdr->version != 0x100 && hdr->version != 0x200) || !hdr->mdatHdrSize)
    return -1;
  libraw_internal_data.unpacker_data.CR3_Version = hdr->version;
  if (hdr->encType == 1)
  {
    if (hdr->nBits > 15)
      return -1;
  }
  else
  {
    if (hdr->encType && hdr->encType != 3)
      return -1;
    if (hdr->nBits > 14)
      return -1;
  }

  if (hdr->nPlanes == 1)
  {
    if (hdr->cfaLayout || hdr->encType)
      return -1;
    if (hdr->nBits != 8)
      return -1;
  }
  else if (hdr->nPlanes != 4 || hdr->f_width & 1 || hdr->f_height & 1 ||
           hdr->tileWidth & 1 || hdr->tileHeight & 1 || hdr->cfaLayout > 3 ||
           (hdr->encType && hdr->encType != 1 && hdr->encType != 3) ||
           hdr->nBits == 8)
    return -1;

  if (hdr->tileWidth > hdr->f_width || hdr->tileHeight > hdr->f_height)
    return -1;

  if (hdr->imageLevels > 3 || hdr->hasTileCols > 1 || hdr->hasTileRows > 1)
    return -1;
  return 0;
}

#undef _abs
#undef _min
#undef _constrain
#undef libraw_inline
