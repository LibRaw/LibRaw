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

void LibRaw::sony_arq_load_raw()
{
  int row, col;
  read_shorts(imgdata.rawdata.raw_image,
              imgdata.sizes.raw_width * imgdata.sizes.raw_height * 4);
  libraw_internal_data.internal_data.input->seek(
      -2, SEEK_CUR); // avoid wrong eof error

  if(imgdata.params.raw_processing_options & LIBRAW_PROCESSING_ARQ_SKIP_CHANNEL_SWAP)
    return;

  for (row = 0; row < imgdata.sizes.raw_height; row++)
  {
    unsigned short(*rowp)[4] =
        (unsigned short(*)[4]) &
        imgdata.rawdata.raw_image[row * imgdata.sizes.raw_width * 4];
    for (col = 0; col < imgdata.sizes.raw_width; col++)
    {
      unsigned short g2 = rowp[col][2];
      rowp[col][2] = rowp[col][3];
      rowp[col][3] = g2;
      if (((unsigned)(row - imgdata.sizes.top_margin) < imgdata.sizes.height) &&
          ((unsigned)(col - imgdata.sizes.left_margin) < imgdata.sizes.width) &&
          (MAX(MAX(rowp[col][0], rowp[col][1]),
               MAX(rowp[col][2], rowp[col][3])) > imgdata.color.maximum))
        derror();
    }
  }
}

void LibRaw::pentax_4shot_load_raw()
{
  ushort *plane = (ushort *)malloc(imgdata.sizes.raw_width *
                                   imgdata.sizes.raw_height * sizeof(ushort));
  int alloc_sz = imgdata.sizes.raw_width * (imgdata.sizes.raw_height + 16) * 4 *
                 sizeof(ushort);
  ushort(*result)[4] = (ushort(*)[4])malloc(alloc_sz);
  struct movement_t
  {
    int row, col;
  } _move[4] = {
      {1, 1},
      {0, 1},
      {0, 0},
      {1, 0},
  };

  int tidx = 0;
  for (int i = 0; i < 4; i++)
  {
    int move_row, move_col;
    if (imgdata.params.p4shot_order[i] >= '0' &&
        imgdata.params.p4shot_order[i] <= '3')
    {
      move_row = ((imgdata.params.p4shot_order[i] - '0') & 2) ? 1 : 0;
      move_col = ((imgdata.params.p4shot_order[i] - '0') & 1) ? 1 : 0;
    }
    else
    {
      move_row = _move[i].row;
      move_col = _move[i].col;
    }
    for (; tidx < 16; tidx++)
      if (tiff_ifd[tidx].t_width == imgdata.sizes.raw_width &&
          tiff_ifd[tidx].t_height == imgdata.sizes.raw_height &&
          tiff_ifd[tidx].bps > 8 && tiff_ifd[tidx].samples == 1)
        break;
    if (tidx >= 16)
      break;
    imgdata.rawdata.raw_image = plane;
    ID.input->seek(tiff_ifd[tidx].offset, SEEK_SET);
    imgdata.idata.filters = 0xb4b4b4b4;
    libraw_internal_data.unpacker_data.data_offset = tiff_ifd[tidx].offset;
    (this->*pentax_component_load_raw)();
    for (int row = 0; row < imgdata.sizes.raw_height - move_row; row++)
    {
      int colors[2];
      for (int c = 0; c < 2; c++)
        colors[c] = COLOR(row, c);
      ushort *srcrow = &plane[imgdata.sizes.raw_width * row];
      ushort(*dstrow)[4] =
          &result[(imgdata.sizes.raw_width) * (row + move_row) + move_col];
      for (int col = 0; col < imgdata.sizes.raw_width - move_col; col++)
        dstrow[col][colors[col % 2]] = srcrow[col];
    }
    tidx++;
  }

  if (imgdata.color.cblack[4] == 2 && imgdata.color.cblack[5] == 2)
    for (int c = 0; c < 4; c++)
      imgdata.color.cblack[FC(c / 2, c % 2)] +=
          imgdata.color.cblack[6 +
                               c / 2 % imgdata.color.cblack[4] *
                                   imgdata.color.cblack[5] +
                               c % 2 % imgdata.color.cblack[5]];
  imgdata.color.cblack[4] = imgdata.color.cblack[5] = 0;

  // assign things back:
  imgdata.sizes.raw_pitch = imgdata.sizes.raw_width * 8;
  imgdata.idata.filters = 0;
  imgdata.rawdata.raw_alloc = imgdata.rawdata.color4_image = result;
  free(plane);
  imgdata.rawdata.raw_image = 0;
}

void LibRaw::hasselblad_full_load_raw()
{
  int row, col;

  for (row = 0; row < S.height; row++)
    for (col = 0; col < S.width; col++)
    {
      read_shorts(&imgdata.image[row * S.width + col][2], 1); // B
      read_shorts(&imgdata.image[row * S.width + col][1], 1); // G
      read_shorts(&imgdata.image[row * S.width + col][0], 1); // R
    }
}

static inline void unpack7bytesto4x16(unsigned char *src, unsigned short *dest)
{
  dest[0] = (src[0] << 6) | (src[1] >> 2);
  dest[1] = ((src[1] & 0x3) << 12) | (src[2] << 4) | (src[3] >> 4);
  dest[2] = (src[3] & 0xf) << 10 | (src[4] << 2) | (src[5] >> 6);
  dest[3] = ((src[5] & 0x3f) << 8) | src[6];
}

static inline void unpack28bytesto16x16ns(unsigned char *src,
                                          unsigned short *dest)
{
  dest[0] = (src[3] << 6) | (src[2] >> 2);
  dest[1] = ((src[2] & 0x3) << 12) | (src[1] << 4) | (src[0] >> 4);
  dest[2] = (src[0] & 0xf) << 10 | (src[7] << 2) | (src[6] >> 6);
  dest[3] = ((src[6] & 0x3f) << 8) | src[5];
  dest[4] = (src[4] << 6) | (src[11] >> 2);
  dest[5] = ((src[11] & 0x3) << 12) | (src[10] << 4) | (src[9] >> 4);
  dest[6] = (src[9] & 0xf) << 10 | (src[8] << 2) | (src[15] >> 6);
  dest[7] = ((src[15] & 0x3f) << 8) | src[14];
  dest[8] = (src[13] << 6) | (src[12] >> 2);
  dest[9] = ((src[12] & 0x3) << 12) | (src[19] << 4) | (src[18] >> 4);
  dest[10] = (src[18] & 0xf) << 10 | (src[17] << 2) | (src[16] >> 6);
  dest[11] = ((src[16] & 0x3f) << 8) | src[23];
  dest[12] = (src[22] << 6) | (src[21] >> 2);
  dest[13] = ((src[21] & 0x3) << 12) | (src[20] << 4) | (src[27] >> 4);
  dest[14] = (src[27] & 0xf) << 10 | (src[26] << 2) | (src[25] >> 6);
  dest[15] = ((src[25] & 0x3f) << 8) | src[24];
}

#define swab32(x)                                                              \
  ((unsigned int)((((unsigned int)(x) & (unsigned int)0x000000ffUL) << 24) |   \
                  (((unsigned int)(x) & (unsigned int)0x0000ff00UL) << 8) |    \
                  (((unsigned int)(x) & (unsigned int)0x00ff0000UL) >> 8) |    \
                  (((unsigned int)(x) & (unsigned int)0xff000000UL) >> 24)))

static inline void swab32arr(unsigned *arr, unsigned len)
{
  for (unsigned i = 0; i < len; i++)
    arr[i] = swab32(arr[i]);
}
#undef swab32

static inline void unpack7bytesto4x16_nikon(unsigned char *src,
                                            unsigned short *dest)
{
  dest[3] = (src[6] << 6) | (src[5] >> 2);
  dest[2] = ((src[5] & 0x3) << 12) | (src[4] << 4) | (src[3] >> 4);
  dest[1] = (src[3] & 0xf) << 10 | (src[2] << 2) | (src[1] >> 6);
  dest[0] = ((src[1] & 0x3f) << 8) | src[0];
}

void LibRaw::nikon_14bit_load_raw()
{
  const unsigned linelen =
      (unsigned)(ceilf((float)(S.raw_width * 7 / 4) / 16.0)) *
      16; // 14512; // S.raw_width * 7 / 4;
  const unsigned pitch = S.raw_pitch ? S.raw_pitch / 2 : S.raw_width;
  unsigned char *buf = (unsigned char *)malloc(linelen);
  merror(buf, "nikon_14bit_load_raw()");
  for (int row = 0; row < S.raw_height; row++)
  {
    unsigned bytesread =
        libraw_internal_data.internal_data.input->read(buf, 1, linelen);
    unsigned short *dest = &imgdata.rawdata.raw_image[pitch * row];
    // swab32arr((unsigned *)buf, bytesread / 4);
    for (unsigned int sp = 0, dp = 0;
         dp < pitch - 3 && sp < linelen - 6 && sp < bytesread - 6;
         sp += 7, dp += 4)
      unpack7bytesto4x16_nikon(buf + sp, dest + dp);
  }
  free(buf);
}

void LibRaw::fuji_14bit_load_raw()
{
  const unsigned linelen = S.raw_width * 7 / 4;
  const unsigned pitch = S.raw_pitch ? S.raw_pitch / 2 : S.raw_width;
  unsigned char *buf = (unsigned char *)malloc(linelen);
  merror(buf, "fuji_14bit_load_raw()");

  for (int row = 0; row < S.raw_height; row++)
  {
    unsigned bytesread =
        libraw_internal_data.internal_data.input->read(buf, 1, linelen);
    unsigned short *dest = &imgdata.rawdata.raw_image[pitch * row];
    if (bytesread % 28)
    {
      swab32arr((unsigned *)buf, bytesread / 4);
      for (unsigned int sp = 0, dp = 0;
           dp < pitch - 3 && sp < linelen - 6 && sp < bytesread - 6;
           sp += 7, dp += 4)
        unpack7bytesto4x16(buf + sp, dest + dp);
    }
    else
      for (unsigned int sp = 0, dp = 0;
           dp < pitch - 15 && sp < linelen - 27 && sp < bytesread - 27;
           sp += 28, dp += 16)
        unpack28bytesto16x16ns(buf + sp, dest + dp);
  }
  free(buf);
}
void LibRaw::nikon_load_padded_packed_raw() // 12 bit per pixel, padded to 16
                                            // bytes
{
  // libraw_internal_data.unpacker_data.load_flags -> row byte count
  if (libraw_internal_data.unpacker_data.load_flags < 2000 ||
      libraw_internal_data.unpacker_data.load_flags > 64000)
    return;
  unsigned char *buf =
      (unsigned char *)malloc(libraw_internal_data.unpacker_data.load_flags);
  for (int row = 0; row < S.raw_height; row++)
  {
    checkCancel();
    libraw_internal_data.internal_data.input->read(
        buf, libraw_internal_data.unpacker_data.load_flags, 1);
    for (int icol = 0; icol < S.raw_width / 2; icol++)
    {
      imgdata.rawdata.raw_image[(row)*S.raw_width + (icol * 2)] =
          ((buf[icol * 3 + 1] & 0xf) << 8) | buf[icol * 3];
      imgdata.rawdata.raw_image[(row)*S.raw_width + (icol * 2 + 1)] =
          buf[icol * 3 + 2] << 4 | ((buf[icol * 3 + 1] & 0xf0) >> 4);
    }
  }
  free(buf);
}

void LibRaw::nikon_load_striped_packed_raw()
{
  int vbits = 0, bwide, rbits, bite, row, col, i;

  UINT64 bitbuf = 0;
  unsigned load_flags = 24; // libraw_internal_data.unpacker_data.load_flags;
  unsigned tiff_bps = libraw_internal_data.unpacker_data.tiff_bps;

  struct tiff_ifd_t *ifd = &tiff_ifd[0];
  while (ifd < &tiff_ifd[libraw_internal_data.identify_data.tiff_nifds] &&
         ifd->offset != libraw_internal_data.unpacker_data.data_offset)
    ++ifd;
  if (ifd == &tiff_ifd[libraw_internal_data.identify_data.tiff_nifds])
    throw LIBRAW_EXCEPTION_DECODE_RAW;

  if (!ifd->rows_per_strip || !ifd->strip_offsets_count)
    return; // not unpacked
  int stripcnt = 0;

  bwide = S.raw_width * tiff_bps / 8;
  bwide += bwide & load_flags >> 7;
  rbits = bwide * 8 - S.raw_width * tiff_bps;
  if (load_flags & 1)
    bwide = bwide * 16 / 15;
  bite = 8 + (load_flags & 24);
  for (row = 0; row < S.raw_height; row++)
  {
    checkCancel();
    if (!(row % ifd->rows_per_strip))
    {
      if (stripcnt >= ifd->strip_offsets_count)
        return; // run out of data
      libraw_internal_data.internal_data.input->seek(
          ifd->strip_offsets[stripcnt], SEEK_SET);
      stripcnt++;
    }
    for (col = 0; col < S.raw_width; col++)
    {
      for (vbits -= tiff_bps; vbits < 0; vbits += bite)
      {
        bitbuf <<= bite;
        for (i = 0; i < bite; i += 8)
          bitbuf |=
              (unsigned)(libraw_internal_data.internal_data.input->get_char()
                         << i);
      }
      imgdata.rawdata.raw_image[(row)*S.raw_width + (col)] =
          bitbuf << (64 - tiff_bps - vbits) >> (64 - tiff_bps);
    }
    vbits -= rbits;
  }
}

struct pana_cs6_page_decoder
{
  unsigned int pixelbuffer[14], lastoffset, maxoffset;
  unsigned char current, *buffer;
  pana_cs6_page_decoder(unsigned char *_buffer, unsigned int bsize)
      : lastoffset(0), maxoffset(bsize), current(0), buffer(_buffer)
  {
  }
  void read_page(); // will throw IO error if not enough space in buffer
  unsigned int nextpixel() { return current < 14 ? pixelbuffer[current++] : 0; }
};

void pana_cs6_page_decoder::read_page()
{
  if (!buffer || (maxoffset - lastoffset < 16))
    throw LIBRAW_EXCEPTION_IO_EOF;
#define wbuffer(i) ((unsigned short)buffer[lastoffset + 15 - i])
  pixelbuffer[0] = (wbuffer(0) << 6) | (wbuffer(1) >> 2); // 14 bit
  pixelbuffer[1] =
      (((wbuffer(1) & 0x3) << 12) | (wbuffer(2) << 4) | (wbuffer(3) >> 4)) &
      0x3fff;
  pixelbuffer[2] = (wbuffer(3) >> 2) & 0x3;
  pixelbuffer[3] = ((wbuffer(3) & 0x3) << 8) | wbuffer(4);
  pixelbuffer[4] = (wbuffer(5) << 2) | (wbuffer(6) >> 6);
  pixelbuffer[5] = ((wbuffer(6) & 0x3f) << 4) | (wbuffer(7) >> 4);
  pixelbuffer[6] = (wbuffer(7) >> 2) & 0x3;
  pixelbuffer[7] = ((wbuffer(7) & 0x3) << 8) | wbuffer(8);
  pixelbuffer[8] = ((wbuffer(9) << 2) & 0x3fc) | (wbuffer(10) >> 6);
  pixelbuffer[9] = ((wbuffer(10) << 4) | (wbuffer(11) >> 4)) & 0x3ff;
  pixelbuffer[10] = (wbuffer(11) >> 2) & 0x3;
  pixelbuffer[11] = ((wbuffer(11) & 0x3) << 8) | wbuffer(12);
  pixelbuffer[12] = (((wbuffer(13) << 2) & 0x3fc) | wbuffer(14) >> 6) & 0x3ff;
  pixelbuffer[13] = ((wbuffer(14) << 4) | (wbuffer(15) >> 4)) & 0x3ff;
#undef wbuffer
  current = 0;
  lastoffset += 16;
}

void LibRaw::panasonicC6_load_raw()
{
  const int rowstep = 16;
  const int blocksperrow = imgdata.sizes.raw_width / 11;
  const int rowbytes = blocksperrow * 16;
  unsigned char *iobuf = (unsigned char *)malloc(rowbytes * rowstep);
  merror(iobuf, "panasonicC6_load_raw()");

  for (int row = 0; row < imgdata.sizes.raw_height - rowstep + 1;
       row += rowstep)
  {
    int rowstoread = MIN(rowstep, imgdata.sizes.raw_height - row);
    if (libraw_internal_data.internal_data.input->read(
            iobuf, rowbytes, rowstoread) != rowstoread)
      throw LIBRAW_EXCEPTION_IO_EOF;
    pana_cs6_page_decoder page(iobuf, rowbytes * rowstoread);
    for (int crow = 0, col = 0; crow < rowstoread; crow++, col = 0)
    {
      unsigned short *rowptr =
          &imgdata.rawdata
               .raw_image[(row + crow) * imgdata.sizes.raw_pitch / 2];
      for (int rblock = 0; rblock < blocksperrow; rblock++)
      {
        page.read_page();
        unsigned oddeven[2] = {0, 0}, nonzero[2] = {0, 0};
        unsigned pmul = 0, pixel_base = 0;
        for (int pix = 0; pix < 11; pix++)
        {
          if (pix % 3 == 2)
          {
            unsigned base = page.nextpixel();
            if (base > 3)
              throw LIBRAW_EXCEPTION_IO_CORRUPT; // not possible b/c of 2-bit
                                                 // field, but....
            if (base == 3)
              base = 4;
            pixel_base = 0x200 << base;
            pmul = 1 << base;
          }
          unsigned epixel = page.nextpixel();
          if (oddeven[pix % 2])
          {
            epixel *= pmul;
            if (pixel_base < 0x2000 && nonzero[pix % 2] > pixel_base)
              epixel += nonzero[pix % 2] - pixel_base;
            nonzero[pix % 2] = epixel;
          }
          else
          {
            oddeven[pix % 2] = epixel;
            if (epixel)
              nonzero[pix % 2] = epixel;
            else
              epixel = nonzero[pix % 2];
          }
          unsigned spix = epixel - 0xf;
          if (spix <= 0xffff)
            rowptr[col++] = spix & 0xffff;
          else
          {
            epixel = (((signed int)(epixel + 0x7ffffff1)) >> 0x1f);
            rowptr[col++] = epixel & 0x3fff;
          }
        }
      }
    }
  }
  free(iobuf);
}

void LibRaw::panasonicC7_load_raw()
{
  const int rowstep = 16;
  int pixperblock = libraw_internal_data.unpacker_data.pana_bpp == 14 ? 9 : 10;
  int rowbytes = imgdata.sizes.raw_width / pixperblock * 16;
  unsigned char *iobuf = (unsigned char *)malloc(rowbytes * rowstep);
  merror(iobuf, "panasonicC7_load_raw()");
  for (int row = 0; row < imgdata.sizes.raw_height - rowstep + 1;
       row += rowstep)
  {
    int rowstoread = MIN(rowstep, imgdata.sizes.raw_height - row);
    if (libraw_internal_data.internal_data.input->read(
            iobuf, rowbytes, rowstoread) != rowstoread)
      throw LIBRAW_EXCEPTION_IO_EOF;
    unsigned char *bytes = iobuf;
    for (int crow = 0; crow < rowstoread; crow++)
    {
      unsigned short *rowptr =
          &imgdata.rawdata
               .raw_image[(row + crow) * imgdata.sizes.raw_pitch / 2];
      for (int col = 0; col < imgdata.sizes.raw_width - pixperblock + 1;
           col += pixperblock, bytes += 16)
      {
        if (libraw_internal_data.unpacker_data.pana_bpp == 14)
        {
          rowptr[col] = bytes[0] + ((bytes[1] & 0x3F) << 8);
          rowptr[col + 1] =
              (bytes[1] >> 6) + 4 * (bytes[2]) + ((bytes[3] & 0xF) << 10);
          rowptr[col + 2] =
              (bytes[3] >> 4) + 16 * (bytes[4]) + ((bytes[5] & 3) << 12);
          rowptr[col + 3] = ((bytes[5] & 0xFC) >> 2) + (bytes[6] << 6);
          rowptr[col + 4] = bytes[7] + ((bytes[8] & 0x3F) << 8);
          rowptr[col + 5] =
              (bytes[8] >> 6) + 4 * bytes[9] + ((bytes[10] & 0xF) << 10);
          rowptr[col + 6] =
              (bytes[10] >> 4) + 16 * bytes[11] + ((bytes[12] & 3) << 12);
          rowptr[col + 7] = ((bytes[12] & 0xFC) >> 2) + (bytes[13] << 6);
          rowptr[col + 8] = bytes[14] + ((bytes[15] & 0x3F) << 8);
        }
        else if (libraw_internal_data.unpacker_data.pana_bpp ==
                 12) // have not seen in the wild yet
        {
          rowptr[col] = ((bytes[1] & 0xF) << 8) + bytes[0];
          rowptr[col + 1] = 16 * bytes[2] + (bytes[1] >> 4);
          rowptr[col + 2] = ((bytes[4] & 0xF) << 8) + bytes[3];
          rowptr[col + 3] = 16 * bytes[5] + (bytes[4] >> 4);
          rowptr[col + 4] = ((bytes[7] & 0xF) << 8) + bytes[6];
          rowptr[col + 5] = 16 * bytes[8] + (bytes[7] >> 4);
          rowptr[col + 6] = ((bytes[10] & 0xF) << 8) + bytes[9];
          rowptr[col + 7] = 16 * bytes[11] + (bytes[10] >> 4);
          rowptr[col + 8] = ((bytes[13] & 0xF) << 8) + bytes[12];
          rowptr[col + 9] = 16 * bytes[14] + (bytes[13] >> 4);
        }
      }
    }
  }
  free(iobuf);
}

void LibRaw::unpacked_load_raw_fuji_f700s20()
{
  int base_offset = 0;
  int row_size = imgdata.sizes.raw_width * 2; // in bytes
  if (imgdata.idata.raw_count == 2 && imgdata.params.shot_select)
  {
    libraw_internal_data.internal_data.input->seek(-row_size, SEEK_CUR);
    base_offset = row_size; // in bytes
  }
  unsigned char *buffer = (unsigned char *)malloc(row_size * 2);
  for (int row = 0; row < imgdata.sizes.raw_height; row++)
  {
    read_shorts((ushort *)buffer, imgdata.sizes.raw_width * 2);
    memmove(&imgdata.rawdata.raw_image[row * imgdata.sizes.raw_pitch / 2],
            buffer + base_offset, row_size);
  }
  free(buffer);
}

void LibRaw::nikon_load_sraw()
{
  // We're already seeked to data!
  unsigned char *rd =
      (unsigned char *)malloc(3 * (imgdata.sizes.raw_width + 2));
  if (!rd)
    throw LIBRAW_EXCEPTION_ALLOC;
  try
  {
    int row, col;
    for (row = 0; row < imgdata.sizes.raw_height; row++)
    {
      checkCancel();
      libraw_internal_data.internal_data.input->read(rd, 3,
                                                     imgdata.sizes.raw_width);
      for (col = 0; col < imgdata.sizes.raw_width - 1; col += 2)
      {
        int bi = col * 3;
        ushort bits1 = (rd[bi + 1] & 0xf) << 8 | rd[bi];            // 3,0,1
        ushort bits2 = rd[bi + 2] << 4 | ((rd[bi + 1] >> 4) & 0xf); // 452
        ushort bits3 = ((rd[bi + 4] & 0xf) << 8) | rd[bi + 3];      // 967
        ushort bits4 = rd[bi + 5] << 4 | ((rd[bi + 4] >> 4) & 0xf); // ab8
        imgdata.image[row * imgdata.sizes.raw_width + col][0] = bits1;
        imgdata.image[row * imgdata.sizes.raw_width + col][1] = bits3;
        imgdata.image[row * imgdata.sizes.raw_width + col][2] = bits4;
        imgdata.image[row * imgdata.sizes.raw_width + col + 1][0] = bits2;
        imgdata.image[row * imgdata.sizes.raw_width + col + 1][1] = 2048;
        imgdata.image[row * imgdata.sizes.raw_width + col + 1][2] = 2048;
      }
    }
  }
  catch (...)
  {
    free(rd);
    throw;
  }
  free(rd);
  C.maximum = 0xfff; // 12 bit?
  if (imgdata.params.raw_processing_options &
      LIBRAW_PROCESSING_SRAW_NO_INTERPOLATE)
  {
    return; // no CbCr interpolation
  }
  // Interpolate CC channels
  int row, col;
  for (row = 0; row < imgdata.sizes.raw_height; row++)
  {
    checkCancel(); // will throw out
    for (col = 0; col < imgdata.sizes.raw_width; col += 2)
    {
      int col2 = col < imgdata.sizes.raw_width - 2 ? col + 2 : col;
      imgdata.image[row * imgdata.sizes.raw_width + col + 1][1] =
          (unsigned short)(int(imgdata.image[row * imgdata.sizes.raw_width +
                                             col][1] +
                               imgdata.image[row * imgdata.sizes.raw_width +
                                             col2][1]) /
                           2);
      imgdata.image[row * imgdata.sizes.raw_width + col + 1][2] =
          (unsigned short)(int(imgdata.image[row * imgdata.sizes.raw_width +
                                             col][2] +
                               imgdata.image[row * imgdata.sizes.raw_width +
                                             col2][2]) /
                           2);
    }
  }
  if (imgdata.params.raw_processing_options & LIBRAW_PROCESSING_SRAW_NO_RGB)
    return;

  for (row = 0; row < imgdata.sizes.raw_height; row++)
  {
    checkCancel(); // will throw out
    for (col = 0; col < imgdata.sizes.raw_width; col++)
    {
      float Y =
          float(imgdata.image[row * imgdata.sizes.raw_width + col][0]) / 2549.f;
      float Ch2 =
          float(imgdata.image[row * imgdata.sizes.raw_width + col][1] - 1280) /
          1536.f;
      float Ch3 =
          float(imgdata.image[row * imgdata.sizes.raw_width + col][2] - 1280) /
          1536.f;
      if (Y > 1.f)
        Y = 1.f;
      if (Y > 0.803f)
        Ch2 = Ch3 = 0.5f;
      float r = Y + 1.40200f * (Ch3 - 0.5f);
      if (r < 0.f)
        r = 0.f;
      if (r > 1.f)
        r = 1.f;
      float g = Y - 0.34414f * (Ch2 - 0.5f) - 0.71414 * (Ch3 - 0.5f);
      if (g > 1.f)
        g = 1.f;
      if (g < 0.f)
        g = 0.f;
      float b = Y + 1.77200 * (Ch2 - 0.5f);
      if (b > 1.f)
        b = 1.f;
      if (b < 0.f)
        b = 0.f;
      imgdata.image[row * imgdata.sizes.raw_width + col][0] =
          imgdata.color.curve[int(r * 3072.f)];
      imgdata.image[row * imgdata.sizes.raw_width + col][1] =
          imgdata.color.curve[int(g * 3072.f)];
      imgdata.image[row * imgdata.sizes.raw_width + col][2] =
          imgdata.color.curve[int(b * 3072.f)];
    }
  }
  C.maximum = 16383;
}
