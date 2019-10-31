/* -*- C++ -*-
 * Copyright 2019 LibRaw LLC (info@libraw.org)
 *
 LibRaw is free software; you can redistribute it and/or modify
 it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#include "../../internal/dcraw_defs.h"

void LibRaw::nikon_coolscan_load_raw()
{
  if (!image)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;

  int bypp = tiff_bps <= 8 ? 1 : 2;
  int bufsize = width * 3 * bypp;
  unsigned char *buf = (unsigned char *)malloc(bufsize);
  unsigned short *ubuf = (unsigned short *)buf;

  if (tiff_bps <= 8)
    gamma_curve(1.0 / imgdata.params.coolscan_nef_gamma, 0., 1, 255);
  else
    gamma_curve(1.0 / imgdata.params.coolscan_nef_gamma, 0., 1, 65535);
  fseek(ifp, data_offset, SEEK_SET);
  for (int row = 0; row < raw_height; row++)
  {
      if(tiff_bps <=8)
        fread(buf, 1, bufsize, ifp);
      else
          read_shorts(ubuf,width*3);
    unsigned short(*ip)[4] = (unsigned short(*)[4])image + row * width;
    if (is_NikonTransfer == 2)
    { // it is also (tiff_bps == 8)
      for (int col = 0; col < width; col++)
      {
        ip[col][0] = ((float)curve[buf[col * 3]]) / 255.0f;
        ip[col][1] = ((float)curve[buf[col * 3 + 1]]) / 255.0f;
        ip[col][2] = ((float)curve[buf[col * 3 + 2]]) / 255.0f;
        ip[col][3] = 0;
      }
    }
    else if (tiff_bps <= 8)
    {
      for (int col = 0; col < width; col++)
      {
        ip[col][0] = curve[buf[col * 3]];
        ip[col][1] = curve[buf[col * 3 + 1]];
        ip[col][2] = curve[buf[col * 3 + 2]];
        ip[col][3] = 0;
      }
    }
    else
    {
      for (int col = 0; col < width; col++)
      {
        ip[col][0] = curve[ubuf[col * 3]];
        ip[col][1] = curve[ubuf[col * 3 + 1]];
        ip[col][2] = curve[ubuf[col * 3 + 2]];
        ip[col][3] = 0;
      }
    }
  }
  free(buf);
}

void LibRaw::broadcom_load_raw()
{

  uchar *data, *dp;
  int rev, row, col, c;
  ushort raw_stride = (ushort)load_flags;
  rev = 3 * (order == 0x4949);
  data = (uchar *)malloc(raw_stride * 2);
  merror(data, "broadcom_load_raw()");

  for (row = 0; row < raw_height; row++)
  {
    if (fread(data + raw_stride, 1, raw_stride, ifp) < raw_stride)
      derror();
    FORC(raw_stride) data[c] = data[raw_stride + (c ^ rev)];
    for (dp = data, col = 0; col < raw_width; dp += 5, col += 4)
      FORC4 RAW(row, col + c) = (dp[c] << 2) | (dp[4] >> (c << 1) & 3);
  }
  free(data);
}

void LibRaw::android_tight_load_raw()
{
  uchar *data, *dp;
  int bwide, row, col, c;

  bwide = -(-5 * raw_width >> 5) << 3;
  data = (uchar *)malloc(bwide);
  merror(data, "android_tight_load_raw()");
  for (row = 0; row < raw_height; row++)
  {
    if (fread(data, 1, bwide, ifp) < bwide)
      derror();
    for (dp = data, col = 0; col < raw_width; dp += 5, col += 4)
      FORC4 RAW(row, col + c) = (dp[c] << 2) | (dp[4] >> (c << 1) & 3);
  }
  free(data);
}

void LibRaw::android_loose_load_raw()
{
  uchar *data, *dp;
  int bwide, row, col, c;
  UINT64 bitbuf = 0;

  bwide = (raw_width + 5) / 6 << 3;
  data = (uchar *)malloc(bwide);
  merror(data, "android_loose_load_raw()");
  for (row = 0; row < raw_height; row++)
  {
    if (fread(data, 1, bwide, ifp) < bwide)
      derror();
    for (dp = data, col = 0; col < raw_width; dp += 8, col += 6)
    {
      FORC(8) bitbuf = (bitbuf << 8) | dp[c ^ 7];
      FORC(6) RAW(row, col + c) = (bitbuf >> c * 10) & 0x3ff;
    }
  }
  free(data);
}

void LibRaw::unpacked_load_raw_reversed()
{
  int row, col, bits = 0;
  while (1 << ++bits < maximum)
    ;
  for (row = raw_height - 1; row >= 0; row--)
  {
    checkCancel();
    read_shorts(&raw_image[row * raw_width], raw_width);
    for (col = 0; col < raw_width; col++)
      if ((RAW(row, col) >>= load_flags) >> bits &&
          (unsigned)(row - top_margin) < height &&
          (unsigned)(col - left_margin) < width)
        derror();
  }
}
