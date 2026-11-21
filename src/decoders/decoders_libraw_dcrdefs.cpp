/* -*- C++ -*-
 * Copyright 2019-2026 LibRaw LLC (info@libraw.org)
 *
 LibRaw is free software; you can redistribute it and/or modify
 it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#include "../../internal/dcraw_defs.h"
#include "../../internal/losslessjpeg_lut.h"
#include <algorithm>
#include <deque>
#include <vector>


void LibRaw::packed_tiled_dng_load_raw()
{
  ushort *rp;
  unsigned row, col;

  int ss = shot_select;
  shot_select = libraw_internal_data.unpacker_data.dng_frames[LIM(ss, 0, (LIBRAW_IFD_MAXCOUNT * 2 - 1))] & 0xff;
  std::vector<ushort> pixel;

  try
  {
    int ntiles = 1 + (raw_width) / tile_width;
    if ((unsigned)ntiles * tile_width > raw_width * 2u) throw LIBRAW_EXCEPTION_ALLOC;
    pixel.resize(tile_width * ntiles * tiff_samples);
  }
  catch (...)
  {
    throw LIBRAW_EXCEPTION_ALLOC; // rethrow
  }
  try
  {
      unsigned trow = 0, tcol = 0;
      INT64 save;
      while (trow < raw_height)
      {
        checkCancel();
        save = ftell(ifp);
        if (tile_length < INT_MAX)
          fseek(ifp, get4(), SEEK_SET);

        for (row = 0; row < tile_length && (row + trow) < raw_height; row++)
        {
          if (tiff_bps == 16)
            read_shorts(pixel.data(), tile_width * tiff_samples);
          else
          {
            getbits(-1);
            for (col = 0; col < tile_width * tiff_samples; col++)
              pixel[col] = getbits(tiff_bps);
          }
          for (rp = pixel.data(), col = 0; col < tile_width; col++)
            adobe_copy_pixel(trow+row, tcol+col, &rp);
        }
        fseek(ifp, save + 4, SEEK_SET);
        if ((tcol += tile_width) >= raw_width)
          trow += tile_length + (tcol = 0);
      }
  }
  catch (...)
  {
    shot_select = ss;
    throw;
  }
  shot_select = ss;
}


/*
   Sony compressed (tiled lossless JPEG) ARW: tile-parallel decoder.

   Tile headers and Huffman tables are read serially with ljpeg_start(). The
   entropy-coded data of each tile is then read into memory and decoded
   without the shared input stream and getbits() state, in parallel with
   OpenMP. Only streams that ljpeg_row() decodes without any warning are
   accepted (4 components, no subsampling, no restart markers, predictor
   1..7, valid codes, no value overflow, no bitstream overrun); otherwise
   sony_ljpeg_load_raw() runs the original serial decoder, so output and
   error reporting are unchanged.
*/

namespace
{
// LibRaw_ljpeg_lut::build() input from a make_decoder() table
struct sony_lj_dcraw_code
{
  const ushort *huff;
  unsigned operator()(unsigned prefix, int nb) const
  {
    const int max = huff[0];
    return nb >= max ? huff[1 + (prefix >> (nb - max))] : huff[1 + (prefix << (max - nb))];
  }
};

// Lookup tables for the distinct Huffman tables of a file (Sony uses one)
struct sony_lj_tables
{
  std::deque<LibRaw_ljpeg_lut> luts; // deque: addresses stay valid
  std::deque<std::vector<ushort>> src;
  const LibRaw_ljpeg_lut *get(const ushort *huff)
  {
    const size_t n = 1 + (size_t(1) << huff[0]);
    for (size_t i = 0; i < src.size(); i++)
      if (src[i].size() == n && !memcmp(src[i].data(), huff, n * sizeof(ushort)))
        return &luts[i];
    if (src.size() >= 64)
      return 0;
    sony_lj_dcraw_code code = {huff};
    luts.push_back(LibRaw_ljpeg_lut());
    luts.back().build(code);
    src.push_back(std::vector<ushort>(huff, huff + n));
    return &luts.back();
  }
};

struct sony_lj_tile
{
  INT64 data, size; // entropy-coded data in the file
  unsigned row, col;
  int bits, high, wide, psv;
  const LibRaw_ljpeg_lut *lut[4];
};

// Decode one tile into raw_image (same arithmetic as ljpeg_row_unrolled());
// returns 0 on success
static int sony_lj_decode_tile(const sony_lj_tile &t, const uchar *d, size_t size, std::vector<ushort> &rows,
                               ushort *raw, size_t pitch, unsigned rwidth, unsigned rheight)
{
  const int W = t.wide * 4;
  rows.resize(size_t(W) * 2);
  LibRaw_ljpeg_bits b(d, d + size);
  int vpred[4], err = 0, c;
  FORC4 vpred[c] = 1 << (t.bits - 1);
  for (unsigned jrow = 0, row = 0; jrow < (unsigned)t.high && t.row + row < rheight - 1; jrow++, row += 2)
  {
    ushort *cur = &rows[(jrow & 1) * W], *prev = &rows[((jrow + 1) & 1) * W];
    unsigned ovf = 0;
    FORC4
    {
      int diff = t.lut[c]->decode(b, err);
      int pred = (vpred[c] += diff) - diff;
      cur[c] = ushort(pred + diff);
      ovf |= cur[c];
    }
    if (!jrow || t.psv == 1)
    {
      for (int i = 4; i < W; i++)
      {
        cur[i] = ushort(cur[i - 4] + t.lut[i & 3]->decode(b, err));
        ovf |= cur[i];
      }
    }
    else
    {
      for (int i = 4; i < W; i++)
      {
        int diff = t.lut[i & 3]->decode(b, err);
        int pred = cur[i - 4], up = prev[i], ul = prev[i - 4];
        switch (t.psv)
        {
        case 2:
          pred = up;
          break;
        case 3:
          pred = ul;
          break;
        case 4:
          pred = pred + up - ul;
          break;
        case 5:
          pred = pred + ((up - ul) >> 1);
          break;
        case 6:
          pred = up + ((pred - ul) >> 1);
          break;
        case 7:
          pred = (pred + up) >> 1;
          break;
        }
        cur[i] = ushort(pred + diff);
        ovf |= cur[i];
      }
    }
    // invalid code, value overflow (derror() in ljpeg_row) or bitstream
    // overrun: leave it to the original decoder
    if (err || (ovf >> t.bits) || b.overrun())
      return 1;
    ushort(*rowp)[4] = (ushort(*)[4])cur;
    for (unsigned col = 0, jcol = 0; jcol < (unsigned)t.wide && t.col + col < rwidth - 1; jcol++, col += 2)
    {
      ushort *r0 = raw + size_t(t.row + row) * pitch + t.col + col;
      r0[0] = rowp[jcol][0];
      r0[1] = rowp[jcol][1];
      r0[pitch] = rowp[jcol][2];
      r0[pitch + 1] = rowp[jcol][3];
    }
  }
  return 0;
}
} // namespace

bool LibRaw::sony_ljpeg_load_raw_tiles()
{
  if (tile_width < 16 || tile_length < 16 || tile_width > 0x10000 || tile_length > 0x10000 || !raw_image ||
      raw_width < 2 || raw_height < 2)
    return false;
  const unsigned tiles_x = (raw_width + tile_width - 1) / tile_width;
  const unsigned tiles_y = (raw_height + tile_length - 1) / tile_length;
  if (INT64(tiles_x) * INT64(tiles_y) > 0x10000)
    return false;
  const int ntiles = int(tiles_x * tiles_y);
  const INT64 fsize = ifp->size();
  std::vector<INT64> offs(ntiles);
  for (int i = 0; i < ntiles; i++)
  {
    offs[i] = get4();
    if (offs[i] <= 0 || offs[i] >= fsize)
      return false;
  }
  std::vector<INT64> sorted(offs);
  std::sort(sorted.begin(), sorted.end());

  // Serial: tile headers and Huffman tables
  std::vector<sony_lj_tile> tiles(ntiles);
  sony_lj_tables tables;
  for (int i = 0; i < ntiles; i++)
  {
    sony_lj_tile &t = tiles[i];
    struct jhead jh;
    fseek(ifp, offs[i], SEEK_SET);
    if (!ljpeg_start(&jh, 0))
      return false;
    bool ok = jh.algo == 0xc3 && jh.clrs == 4 && !jh.sraw && jh.restart == INT_MAX && jh.bits >= 1 && jh.psv >= 1 &&
              jh.psv <= 7 && INT64(jh.high) * jh.wide < INT_MAX;
    try
    {
      for (int c = 0; ok && c < 4; c++)
        ok = (t.lut[c] = (c && jh.huff[c] == jh.huff[c - 1]) ? t.lut[c - 1] : tables.get(jh.huff[c])) != 0;
    }
    catch (...)
    {
      ok = false;
    }
    t.data = ftell(ifp);
    t.row = (i / tiles_x) * tile_length;
    t.col = (i % tiles_x) * tile_width;
    t.bits = jh.bits;
    t.high = jh.high;
    t.wide = jh.wide;
    t.psv = jh.psv;
    ljpeg_end(&jh);
    // Data ends at the next tile, capped at 4 bytes per pixel (more than any
    // real tile needs); a tile that does not fit is left to the original decoder
    std::vector<INT64>::iterator nx = std::upper_bound(sorted.begin(), sorted.end(), offs[i]);
    INT64 end = MIN(fsize, offs[i] + INT64(tile_width) * INT64(tile_length) * 4 + 0x10000);
    if (nx != sorted.end() && *nx < end)
      end = *nx;
    t.size = end - t.data;
    if (!ok || t.size <= 0)
      return false;
  }

  // Parallel: entropy-coded data
  checkCancel();
  std::vector<char> bad(ntiles, 0);
  const size_t pitch = raw_pitch / 2;
#ifdef LIBRAW_USE_OPENMP
#pragma omp parallel
#endif
  {
    std::vector<uchar> buf;
    std::vector<ushort> rows;
#ifdef LIBRAW_USE_OPENMP
#pragma omp for schedule(dynamic)
#endif
    for (int i = 0; i < ntiles; i++)
    {
      try
      {
        buf.resize(size_t(tiles[i].size));
        bad[i] = read_at(tiles[i].data, buf.data(), buf.size()) != int(buf.size()) ||
                 sony_lj_decode_tile(tiles[i], buf.data(), buf.size(), rows, raw_image, pitch, raw_width, raw_height);
      }
      catch (...)
      {
        bad[i] = 1;
      }
    }
  }
  checkCancel();
  return std::find(bad.begin(), bad.end(), 1) == bad.end();
}

void LibRaw::sony_ljpeg_load_raw()
{
  unsigned trow = 0, tcol = 0, jrow, jcol, row, col;
  INT64 save;
  struct jhead jh;

  const INT64 start = ftell(ifp);
  if (sony_ljpeg_load_raw_tiles())
    return;
  fseek(ifp, start, SEEK_SET);

  while (trow < raw_height)
  {
    checkCancel();
    save = ftell(ifp); // We're at
    if (tile_length < INT_MAX)
      fseek(ifp, get4(), SEEK_SET);
    if (!ljpeg_start(&jh, 0))
      break;
    try
    {
      for (row = jrow = 0; jrow < (unsigned)jh.high && trow+row < raw_height-1; jrow++, row += 2)
      {
        checkCancel();
        ushort(*rowp)[4] = (ushort(*)[4])ljpeg_row(jrow, &jh);
        for (col = jcol = 0; jcol < (unsigned)jh.wide && tcol+col < raw_width-1; jcol++, col += 2)
        {
          RAW(trow + row, tcol + col) = rowp[jcol][0];
          RAW(trow + row, tcol + col + 1) = rowp[jcol][1];
          RAW(trow + row + 1, tcol + col) = rowp[jcol][2];
          RAW(trow + row + 1, tcol + col + 1) = rowp[jcol][3];
        }
      }
    }
    catch (...)
    {
      ljpeg_end(&jh);
      throw;
    }
    fseek(ifp, save + 4, SEEK_SET);
    if ((tcol += tile_width) >= raw_width)
      trow += tile_length + (tcol = 0);
    ljpeg_end(&jh);
  }
}


void LibRaw::nikon_coolscan_load_raw()
{
  int clrs = colors == 3 ? 3 : 1;

  if (clrs == 3 && !image)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;

  if(clrs == 1 && !raw_image)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;

  int bypp = tiff_bps <= 8 ? 1 : 2;
  int bufsize = width * clrs * bypp;
  unsigned char *buf = (unsigned char *)calloc(bufsize,1);
  unsigned short *ubuf = (unsigned short *)buf;

  if (tiff_bps <= 8)
    gamma_curve(1.0 / imgdata.rawparams.coolscan_nef_gamma, 0., 1, 255);
  else
    gamma_curve(1.0 / imgdata.rawparams.coolscan_nef_gamma, 0., 1, 65535);
  fseek(ifp, data_offset, SEEK_SET);
  for (int row = 0; row < raw_height; row++)
  {
	  if (tiff_bps <= 8)
	  {
		  if (fread(buf, 1, bufsize, ifp) < bufsize)
			  derror(); // will raise EOF exception on eof
	  }
      else
          read_shorts(ubuf,width*clrs);

    unsigned short(*ip)[4] = (unsigned short(*)[4])image + row * width;
    unsigned short *rp =  raw_image + row * raw_width;

    if (is_NikonTransfer == 2)
    { // it is also (tiff_bps == 8)
        if (clrs == 3)
        {
          for (int col = 0; col < width; col++)
          {
            ip[col][0] = ushort(((float)curve[buf[col * 3]]) / 255.0f);
            ip[col][1] = ushort(((float)curve[buf[col * 3 + 1]]) / 255.0f);
            ip[col][2] = ushort(((float)curve[buf[col * 3 + 2]]) / 255.0f);
            ip[col][3] = 0;
          }
        }
        else
        {
          for (int col = 0; col < width; col++)
            rp[col] = ushort(((float)curve[buf[col]]) / 255.0f);
        }
    }
    else if (tiff_bps <= 8)
    {
        if (clrs == 3)
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
            rp[col] = curve[buf[col]];
        }
    }
    else
    {
        if (clrs == 3)
        {
          for (int col = 0; col < width; col++)
          {
            ip[col][0] = curve[ubuf[col * 3]];
            ip[col][1] = curve[ubuf[col * 3 + 1]];
            ip[col][2] = curve[ubuf[col * 3 + 2]];
            ip[col][3] = 0;
          }
        }
        else
        {
          for (int col = 0; col < width; col++)
            rp[col] = curve[ubuf[col]];
        }
    }
  }
  free(buf);
}

void LibRaw::broadcom_load_raw()
{
  uchar *dp;
  int rev, row, col, c;
  rev = 3 * (order == 0x4949);
  std::vector<uchar> data(raw_stride * 2);

  for (row = 0; row < raw_height; row++)
  {
    if (fread(data.data() + raw_stride, 1, raw_stride, ifp) < raw_stride)
      derror();
    FORC(raw_stride) data[c] = data[raw_stride + (c ^ rev)];
    for (dp = data.data(), col = 0; col < raw_width; dp += 5, col += 4)
      FORC4 RAW(row, col + c) = (dp[c] << 2) | (dp[4] >> (c << 1) & 3);
  }
}

void LibRaw::android_tight_load_raw()
{
  uchar *data, *dp;
  int bwide, row, col, c;

  bwide = -(-5 * raw_width >> 5) << 3;
  data = (uchar *)calloc(bwide,1);
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
  data = (uchar *)calloc(bwide,1);
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
  while (1 << ++bits < (int)maximum)
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

#ifdef USE_6BY9RPI

void LibRaw::rpi_load_raw8()
{
	uchar  *data, *dp;
	int rev, dwide, row, col, c;
	double sum[] = { 0,0 };
	rev = 3 * (order == 0x4949);
	if (raw_stride == 0)
		dwide = raw_width;
	else
		dwide = raw_stride;
    data = (uchar *)calloc(dwide, 2);
	for (row = 0; row < raw_height; row++) {
		if (fread(data + dwide, 1, dwide, ifp) < dwide) derror();
		FORC(dwide) data[c] = data[dwide + (c ^ rev)];
		for (dp = data, col = 0; col < raw_width && col < dwide; dp++, col++)
			RAW(row, col) = *dp;
	}
	free(data);
	maximum = 0xff;
	if (!strcmp(make, "OmniVision") ||
		!strcmp(make, "Sony") ||
		!strcmp(make, "RaspberryPi")) return;

	row = raw_height / 2;
	FORC(width - 1) {
		sum[c & 1] += SQR(RAW(row, c) - RAW(row + 1, c + 1));
		sum[~c & 1] += SQR(RAW(row + 1, c) - RAW(row, c + 1));
	}
	if (sum[1] > sum[0]) filters = 0x4b4b4b4b;
}

void LibRaw::rpi_load_raw12()
{
	uchar  *data, *dp;
	int rev, dwide, row, col, c;
	double sum[] = { 0,0 };
	rev = 3 * (order == 0x4949);
	if (raw_stride == 0)
		dwide = (raw_width * 3 + 1) / 2;
	else
		dwide = raw_stride;
    data = (uchar *)calloc(dwide, 2);
	for (row = 0; row < raw_height; row++) {
		if (fread(data + dwide, 1, dwide, ifp) < dwide) derror();
		FORC(dwide) data[c] = data[dwide + (c ^ rev)];
		for (dp = data, col = 0; col < raw_width; dp += 3, col += 2)
			FORC(2) RAW(row, col + c) = (dp[c] << 4) | (dp[2] >> (c << 2) & 0xF);
	}
	free(data);
	maximum = 0xfff;
	if (!strcmp(make, "OmniVision") ||
		!strcmp(make, "Sony") ||
		!strcmp(make, "RaspberryPi")) return;

	row = raw_height / 2;
	FORC(width - 1) {
		sum[c & 1] += SQR(RAW(row, c) - RAW(row + 1, c + 1));
		sum[~c & 1] += SQR(RAW(row + 1, c) - RAW(row, c + 1));
	}
	if (sum[1] > sum[0]) filters = 0x4b4b4b4b;
}

void LibRaw::rpi_load_raw14()
{
	uchar  *data, *dp;
	int rev, dwide, row, col, c;
	double sum[] = { 0,0 };
	rev = 3 * (order == 0x4949);
	if (raw_stride == 0)
		dwide = ((raw_width * 7) + 3) >> 2;
	else
		dwide = raw_stride;
    data = (uchar *)calloc(dwide, 2);
	for (row = 0; row < raw_height; row++) {
		if (fread(data + dwide, 1, dwide, ifp) < dwide) derror();
		FORC(dwide) data[c] = data[dwide + (c ^ rev)];
		for (dp = data, col = 0; col < raw_width; dp += 7, col += 4) {
			RAW(row, col + 0) = (dp[0] << 6) | (dp[4] >> 2);
			RAW(row, col + 1) = (dp[1] << 6) | ((dp[4] & 0x3) << 4) | ((dp[5] & 0xf0) >> 4);
			RAW(row, col + 2) = (dp[2] << 6) | ((dp[5] & 0xf) << 2) | ((dp[6] & 0xc0) >> 6);
			RAW(row, col + 3) = (dp[3] << 6) | ((dp[6] & 0x3f) << 2);
		}
	}
	free(data);
	maximum = 0x3fff;
	if (!strcmp(make, "OmniVision") ||
		!strcmp(make, "Sony") ||
		!strcmp(make, "RaspberryPi")) return;

	row = raw_height / 2;
	FORC(width - 1) {
		sum[c & 1] += SQR(RAW(row, c) - RAW(row + 1, c + 1));
		sum[~c & 1] += SQR(RAW(row + 1, c) - RAW(row, c + 1));
	}
	if (sum[1] > sum[0]) filters = 0x4b4b4b4b;
}

void LibRaw::rpi_load_raw16()
{
	uchar  *data, *dp;
	int rev, dwide, row, col, c;
	double sum[] = { 0,0 };
	rev = 3 * (order == 0x4949);
	if (raw_stride == 0)
		dwide = (raw_width * 2);
	else
		dwide = raw_stride;
    data = (uchar *)calloc(dwide, 2);
	for (row = 0; row < raw_height; row++) {
		if (fread(data + dwide, 1, dwide, ifp) < dwide) derror();
		FORC(dwide) data[c] = data[dwide + (c ^ rev)];
		for (dp = data, col = 0; col < raw_width && col < dwide/2; dp += 2, col++)
			RAW(row, col) = (dp[1] << 8) | dp[0];
	}
	free(data);
	maximum = 0xffff;
	if (!strcmp(make, "OmniVision") ||
		!strcmp(make, "Sony") ||
		!strcmp(make, "RaspberryPi")) return;

	row = raw_height / 2;
	FORC(width - 1) {
		sum[c & 1] += SQR(RAW(row, c) - RAW(row + 1, c + 1));
		sum[~c & 1] += SQR(RAW(row + 1, c) - RAW(row, c + 1));
	}
	if (sum[1] > sum[0]) filters = 0x4b4b4b4b;
}

#endif
