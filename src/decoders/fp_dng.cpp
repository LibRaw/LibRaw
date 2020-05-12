/* -*- C++ -*-
 * Copyright 2019-2020 LibRaw LLC (info@libraw.org)
 *
 LibRaw is free software; you can redistribute it and/or modify
 it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#include "../../internal/libraw_cxx_defs.h"

#ifdef USE_ZLIB
inline unsigned int __DNG_HalfToFloat(ushort halfValue)
{
  int sign = (halfValue >> 15) & 0x00000001;
  int exponent = (halfValue >> 10) & 0x0000001f;
  int mantissa = halfValue & 0x000003ff;
  if (exponent == 0)
  {
    if (mantissa == 0)
    {
      return (unsigned int)(sign << 31);
    }
    else
    {
      while (!(mantissa & 0x00000400))
      {
        mantissa <<= 1;
        exponent -= 1;
      }
      exponent += 1;
      mantissa &= ~0x00000400;
    }
  }
  else if (exponent == 31)
  {
    if (mantissa == 0)
    {
      return (unsigned int)((sign << 31) | ((0x1eL + 127 - 15) << 23) |
                            (0x3ffL << 13));
    }
    else
    {
      return 0;
    }
  }
  exponent += (127 - 15);
  mantissa <<= 13;
  return (unsigned int)((sign << 31) | (exponent << 23) | mantissa);
}

inline unsigned int __DNG_FP24ToFloat(const unsigned char *input)
{
  int sign = (input[0] >> 7) & 0x01;
  int exponent = (input[0]) & 0x7F;
  int mantissa = (((int)input[1]) << 8) | input[2];
  if (exponent == 0)
  {
    if (mantissa == 0)
    {
      return (unsigned int)(sign << 31);
    }
    else
    {
      while (!(mantissa & 0x00010000))
      {
        mantissa <<= 1;
        exponent -= 1;
      }
      exponent += 1;
      mantissa &= ~0x00010000;
    }
  }
  else if (exponent == 127)
  {
    if (mantissa == 0)
    {
      return (unsigned int)((sign << 31) | ((0x7eL + 128 - 64) << 23) |
                            (0xffffL << 7));
    }
    else
    {
      // Nan -- Just set to zero.
      return 0;
    }
  }
  exponent += (128 - 64);
  mantissa <<= 7;
  return (uint32_t)((sign << 31) | (exponent << 23) | mantissa);
}

inline void DecodeDeltaBytes(unsigned char *bytePtr, int cols, int channels)
{
  if (channels == 1)
  {
    unsigned char b0 = bytePtr[0];
    bytePtr += 1;
    for (uint32_t col = 1; col < cols; ++col)
    {
      b0 += bytePtr[0];
      bytePtr[0] = b0;
      bytePtr += 1;
    }
  }
  else if (channels == 3)
  {
    unsigned char b0 = bytePtr[0];
    unsigned char b1 = bytePtr[1];
    unsigned char b2 = bytePtr[2];
    bytePtr += 3;
    for (int col = 1; col < cols; ++col)
    {
      b0 += bytePtr[0];
      b1 += bytePtr[1];
      b2 += bytePtr[2];
      bytePtr[0] = b0;
      bytePtr[1] = b1;
      bytePtr[2] = b2;
      bytePtr += 3;
    }
  }
  else if (channels == 4)
  {
    unsigned char b0 = bytePtr[0];
    unsigned char b1 = bytePtr[1];
    unsigned char b2 = bytePtr[2];
    unsigned char b3 = bytePtr[3];
    bytePtr += 4;
    for (uint32_t col = 1; col < cols; ++col)
    {
      b0 += bytePtr[0];
      b1 += bytePtr[1];
      b2 += bytePtr[2];
      b3 += bytePtr[3];
      bytePtr[0] = b0;
      bytePtr[1] = b1;
      bytePtr[2] = b2;
      bytePtr[3] = b3;
      bytePtr += 4;
    }
  }
  else
  {
    for (int col = 1; col < cols; ++col)
    {
      for (int chan = 0; chan < channels; ++chan)
      {
        bytePtr[chan + channels] += bytePtr[chan];
      }
      bytePtr += channels;
    }
  }
}

static void DecodeFPDelta(unsigned char *input, unsigned char *output, int cols,
                          int channels, int bytesPerSample)
{
  DecodeDeltaBytes(input, cols * bytesPerSample, channels);
  int32_t rowIncrement = cols * channels;

  if (bytesPerSample == 2)
  {

#if LibRawBigEndian
    const unsigned char *input0 = input;
    const unsigned char *input1 = input + rowIncrement;
#else
    const unsigned char *input1 = input;
    const unsigned char *input0 = input + rowIncrement;
#endif
    for (int col = 0; col < rowIncrement; ++col)
    {
      output[0] = input0[col];
      output[1] = input1[col];
      output += 2;
    }
  }
  else if (bytesPerSample == 3)
  {
    const unsigned char *input0 = input;
    const unsigned char *input1 = input + rowIncrement;
    const unsigned char *input2 = input + rowIncrement * 2;
    for (int col = 0; col < rowIncrement; ++col)
    {
      output[0] = input0[col];
      output[1] = input1[col];
      output[2] = input2[col];
      output += 3;
    }
  }
  else
  {
#if LibRawBigEndian
    const unsigned char *input0 = input;
    const unsigned char *input1 = input + rowIncrement;
    const unsigned char *input2 = input + rowIncrement * 2;
    const unsigned char *input3 = input + rowIncrement * 3;
#else
    const unsigned char *input3 = input;
    const unsigned char *input2 = input + rowIncrement;
    const unsigned char *input1 = input + rowIncrement * 2;
    const unsigned char *input0 = input + rowIncrement * 3;
#endif
    for (int col = 0; col < rowIncrement; ++col)
    {
      output[0] = input0[col];
      output[1] = input1[col];
      output[2] = input2[col];
      output[3] = input3[col];
      output += 4;
    }
  }
}

static float expandFloats(unsigned char *dst, int tileWidth, int bytesps)
{
  float max = 0.f;
  if (bytesps == 2)
  {
    uint16_t *dst16 = (ushort *)dst;
    uint32_t *dst32 = (unsigned int *)dst;
    float *f32 = (float *)dst;
    for (int index = tileWidth - 1; index >= 0; --index)
    {
      dst32[index] = __DNG_HalfToFloat(dst16[index]);
      max = MAX(max, f32[index]);
    }
  }
  else if (bytesps == 3)
  {
    uint8_t *dst8 = ((unsigned char *)dst) + (tileWidth - 1) * 3;
    uint32_t *dst32 = (unsigned int *)dst;
    float *f32 = (float *)dst;
    for (int index = tileWidth - 1; index >= 0; --index, dst8 -= 3)
    {
      dst32[index] = __DNG_FP24ToFloat(dst8);
      max = MAX(max, f32[index]);
    }
  }
  else if (bytesps == 4)
  {
    float *f32 = (float *)dst;
    for (int index = 0; index < tileWidth; index++)
      max = MAX(max, f32[index]);
  }
  return max;
}

void LibRaw::deflate_dng_load_raw()
{
  int iifd = find_ifd_by_offset(libraw_internal_data.unpacker_data.data_offset);
  if(iifd < 0 || iifd > libraw_internal_data.identify_data.tiff_nifds)
      throw LIBRAW_EXCEPTION_DECODE_RAW;
  struct tiff_ifd_t *ifd = &tiff_ifd[iifd];

  float *float_raw_image = 0;
  float max = 0.f;

  if (ifd->samples != 1 && ifd->samples != 3 && ifd->samples != 4)
    throw LIBRAW_EXCEPTION_DECODE_RAW; // Only float deflated supported

  if (libraw_internal_data.unpacker_data.tiff_samples != ifd->samples)
    throw LIBRAW_EXCEPTION_DECODE_RAW; // Wrong IFD

  size_t tilesH = (imgdata.sizes.raw_width +
                   libraw_internal_data.unpacker_data.tile_width - 1) /
                  libraw_internal_data.unpacker_data.tile_width;
  size_t tilesV = (imgdata.sizes.raw_height +
                   libraw_internal_data.unpacker_data.tile_length - 1) /
                  libraw_internal_data.unpacker_data.tile_length;
  size_t tileCnt = tilesH * tilesV;

  if (ifd->sample_format == 3)
  { // Floating point data
    float_raw_image = (float *)calloc(
        tileCnt * libraw_internal_data.unpacker_data.tile_length *
            libraw_internal_data.unpacker_data.tile_width * ifd->samples,
        sizeof(float));
    // imgdata.color.maximum = 65535;
    // imgdata.color.black = 0;
    // memset(imgdata.color.cblack,0,sizeof(imgdata.color.cblack));
  }
  else
    throw LIBRAW_EXCEPTION_DECODE_RAW; // Only float deflated supported

  int xFactor;
  switch (ifd->predictor)
  {
  case 3:
  default:
    xFactor = 1;
    break;
  case 34894:
    xFactor = 2;
    break;
  case 34895:
    xFactor = 4;
    break;
  }

  if (libraw_internal_data.unpacker_data.tile_length < INT_MAX)
  {
    if (tileCnt < 1 || tileCnt > 1000000)
      throw LIBRAW_EXCEPTION_DECODE_RAW;

    size_t *tOffsets = (size_t *)malloc(tileCnt * sizeof(size_t));
    for (int t = 0; t < tileCnt; ++t)
      tOffsets[t] = get4();

    size_t *tBytes = (size_t *)malloc(tileCnt * sizeof(size_t));
    unsigned long maxBytesInTile = 0;
    if (tileCnt == 1)
      tBytes[0] = maxBytesInTile = ifd->bytes;
    else
    {
      libraw_internal_data.internal_data.input->seek(ifd->bytes, SEEK_SET);
      for (size_t t = 0; t < tileCnt; ++t)
      {
        tBytes[t] = get4();
        maxBytesInTile = MAX(maxBytesInTile, tBytes[t]);
      }
    }
    unsigned tilePixels = libraw_internal_data.unpacker_data.tile_width *
                          libraw_internal_data.unpacker_data.tile_length;
    unsigned pixelSize = sizeof(float) * ifd->samples;
    unsigned tileBytes = tilePixels * pixelSize;
    unsigned tileRowBytes =
        libraw_internal_data.unpacker_data.tile_width * pixelSize;

    unsigned char *cBuffer = (unsigned char *)malloc(maxBytesInTile);
    unsigned char *uBuffer = (unsigned char *)malloc(
        tileBytes + tileRowBytes); // extra row for decoding

    for (size_t y = 0, t = 0; y < imgdata.sizes.raw_height;
         y += libraw_internal_data.unpacker_data.tile_length)
    {
      for (size_t x = 0; x < imgdata.sizes.raw_width;
           x += libraw_internal_data.unpacker_data.tile_width, ++t)
      {
        libraw_internal_data.internal_data.input->seek(tOffsets[t], SEEK_SET);
        libraw_internal_data.internal_data.input->read(cBuffer, 1, tBytes[t]);
        unsigned long dstLen = tileBytes;
        int err =
            uncompress(uBuffer + tileRowBytes, &dstLen, cBuffer, tBytes[t]);
        if (err != Z_OK)
        {
          free(tOffsets);
          free(tBytes);
          free(cBuffer);
          free(uBuffer);
          throw LIBRAW_EXCEPTION_DECODE_RAW;
          return;
        }
        else
        {
          int bytesps = ifd->bps >> 3;
          size_t rowsInTile =
              y + libraw_internal_data.unpacker_data.tile_length >
                      imgdata.sizes.raw_height
                  ? imgdata.sizes.raw_height - y
                  : libraw_internal_data.unpacker_data.tile_length;
          size_t colsInTile =
              x + libraw_internal_data.unpacker_data.tile_width >
                      imgdata.sizes.raw_width
                  ? imgdata.sizes.raw_width - x
                  : libraw_internal_data.unpacker_data.tile_width;

          for (size_t row = 0; row < rowsInTile;
               ++row) // do not process full tile if not needed
          {
            unsigned char *dst =
                uBuffer + row * libraw_internal_data.unpacker_data.tile_width *
                              bytesps * ifd->samples;
            unsigned char *src = dst + tileRowBytes;
            DecodeFPDelta(src, dst,
                          libraw_internal_data.unpacker_data.tile_width /
                              xFactor,
                          ifd->samples * xFactor, bytesps);
            float lmax = expandFloats(
                dst,
                libraw_internal_data.unpacker_data.tile_width * ifd->samples,
                bytesps);
            max = MAX(max, lmax);
            unsigned char *dst2 = (unsigned char *)&float_raw_image
                [((y + row) * imgdata.sizes.raw_width + x) * ifd->samples];
            memmove(dst2, dst, colsInTile * ifd->samples * sizeof(float));
          }
        }
      }
    }
    free(tOffsets);
    free(tBytes);
    free(cBuffer);
    free(uBuffer);
  }
  imgdata.color.fmaximum = max;

  // Set fields according to data format

  imgdata.rawdata.raw_alloc = float_raw_image;
  if (ifd->samples == 1)
  {
    imgdata.rawdata.float_image = float_raw_image;
    imgdata.rawdata.sizes.raw_pitch = imgdata.sizes.raw_pitch =
        imgdata.sizes.raw_width * 4;
  }
  else if (ifd->samples == 3)
  {
    imgdata.rawdata.float3_image = (float(*)[3])float_raw_image;
    imgdata.rawdata.sizes.raw_pitch = imgdata.sizes.raw_pitch =
        imgdata.sizes.raw_width * 12;
  }
  else if (ifd->samples == 4)
  {
    imgdata.rawdata.float4_image = (float(*)[4])float_raw_image;
    imgdata.rawdata.sizes.raw_pitch = imgdata.sizes.raw_pitch =
        imgdata.sizes.raw_width * 16;
  }

  if (imgdata.params.raw_processing_options &
      LIBRAW_PROCESSING_CONVERTFLOAT_TO_INT)
    convertFloatToInt(); // with default settings
}
#else
void LibRaw::deflate_dng_load_raw() { throw LIBRAW_EXCEPTION_DECODE_RAW; }
#endif
void LibRaw::float_dng_load_raw_placeholder()
{
  // placeholder only, real decoding implemented in DNG SDK
  throw LIBRAW_EXCEPTION_DECODE_RAW;
}

int LibRaw::is_floating_point()
{
  struct tiff_ifd_t *ifd = &tiff_ifd[0];
  while (ifd < &tiff_ifd[libraw_internal_data.identify_data.tiff_nifds] &&
         ifd->offset != libraw_internal_data.unpacker_data.data_offset)
    ++ifd;
  if (ifd == &tiff_ifd[libraw_internal_data.identify_data.tiff_nifds])
    return 0;

  return ifd->sample_format == 3;
}

int LibRaw::have_fpdata()
{
  return imgdata.rawdata.float_image || imgdata.rawdata.float3_image ||
         imgdata.rawdata.float4_image;
}

void LibRaw::convertFloatToInt(float dmin /* =4096.f */,
                               float dmax /* =32767.f */,
                               float dtarget /*= 16383.f */)
{
  int samples = 0;
  float *data = 0;
  if (imgdata.rawdata.float_image)
  {
    samples = 1;
    data = imgdata.rawdata.float_image;
  }
  else if (imgdata.rawdata.float3_image)
  {
    samples = 3;
    data = (float *)imgdata.rawdata.float3_image;
  }
  else if (imgdata.rawdata.float4_image)
  {
    samples = 4;
    data = (float *)imgdata.rawdata.float4_image;
  }
  else
    return;

  ushort *raw_alloc = (ushort *)malloc(
      imgdata.sizes.raw_height * imgdata.sizes.raw_width *
      libraw_internal_data.unpacker_data.tiff_samples * sizeof(ushort));
  float tmax = MAX(imgdata.color.maximum, 1);
  float datamax = imgdata.color.fmaximum;

  tmax = MAX(tmax, datamax);
  tmax = MAX(tmax, 1.f);

  float multip = 1.f;
  if (tmax < dmin || tmax > dmax)
  {
    imgdata.rawdata.color.fnorm = imgdata.color.fnorm = multip = dtarget / tmax;
    imgdata.rawdata.color.maximum = imgdata.color.maximum = dtarget;
    imgdata.rawdata.color.black = imgdata.color.black =
        (float)imgdata.color.black * multip;
    for (int i = 0;
         i < int(sizeof(imgdata.color.cblack)/sizeof(imgdata.color.cblack[0]));
         i++)
      if (i != 4 && i != 5)
        imgdata.rawdata.color.cblack[i] = imgdata.color.cblack[i] =
            (float)imgdata.color.cblack[i] * multip;
  }
  else
    imgdata.rawdata.color.fnorm = imgdata.color.fnorm = 0.f;

  for (size_t i = 0; i < imgdata.sizes.raw_height * imgdata.sizes.raw_width *
                             libraw_internal_data.unpacker_data.tiff_samples;
       ++i)
  {
    float val = MAX(data[i], 0.f);
    raw_alloc[i] = (ushort)(val * multip);
  }

  if (samples == 1)
  {
    imgdata.rawdata.raw_alloc = imgdata.rawdata.raw_image = raw_alloc;
    imgdata.rawdata.sizes.raw_pitch = imgdata.sizes.raw_pitch =
        imgdata.sizes.raw_width * 2;
  }
  else if (samples == 3)
  {
    imgdata.rawdata.raw_alloc = imgdata.rawdata.color3_image =
        (ushort(*)[3])raw_alloc;
    imgdata.rawdata.sizes.raw_pitch = imgdata.sizes.raw_pitch =
        imgdata.sizes.raw_width * 6;
  }
  else if (samples == 4)
  {
    imgdata.rawdata.raw_alloc = imgdata.rawdata.color4_image =
        (ushort(*)[4])raw_alloc;
    imgdata.rawdata.sizes.raw_pitch = imgdata.sizes.raw_pitch =
        imgdata.sizes.raw_width * 8;
  }
  free(data); // remove old allocation
  imgdata.rawdata.float_image = 0;
  imgdata.rawdata.float3_image = 0;
  imgdata.rawdata.float4_image = 0;
}
