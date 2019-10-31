/* -*- C++ -*-
 * Copyright 2019 LibRaw LLC (info@libraw.org)
 *
 LibRaw uses code from dcraw.c -- Dave Coffin's raw photo decoder,
 dcraw.c is copyright 1997-2018 by Dave Coffin, dcoffin a cybercom o net.
 LibRaw do not use RESTRICTED code from dcraw.c

 LibRaw is free software; you can redistribute it and/or modify
 it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#include "../../internal/dcraw_defs.h"

/*
   CIFF block 0x1030 contains an 8x8 white sample.
   Load this into white[][] for use in scale_colors().
 */
void LibRaw::ciff_block_1030()
{
  static const ushort key[] = {0x410, 0x45f3};
  int i, bpp, row, col, vbits = 0;
  unsigned long bitbuf = 0;

  if ((get2(), get4()) != 0x80008 || !get4())
    return;
  bpp = get2();
  if (bpp != 10 && bpp != 12)
    return;
  for (i = row = 0; row < 8; row++)
    for (col = 0; col < 8; col++)
    {
      if (vbits < bpp)
      {
        bitbuf = bitbuf << 16 | (get2() ^ key[i++ & 1]);
        vbits += 16;
      }
      white[row][col] = bitbuf >> (vbits -= bpp) & ~(-1 << bpp);
    }
}

/*
   Parse a CIFF file, better known as Canon CRW format.
 */

void LibRaw::parse_ciff(int offset, int length, int depth)
{

  int tboff, nrecs, c, type, len, save, wbi = -1;
  ushort key[] = {0x410, 0x45f3};
  INT64 fsize = ifp->size();
  if (metadata_blocks++ > LIBRAW_MAX_METADATA_BLOCKS)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;

  fseek(ifp, offset + length - 4, SEEK_SET);
  tboff = get4() + offset;
  fseek(ifp, tboff, SEEK_SET);
  nrecs = get2();
  if (nrecs < 1)
    return;
  if ((nrecs | depth) > 127)
    return;

  if (nrecs * 10 + offset > fsize)
    return;

  while (nrecs--)
  {
    type = get2();
    len = get4();
    INT64 see = offset + get4();
    save = ftell(ifp);

    /* the following tags are not sub-tables
     * they contain the value in the "len" field
     * for such tags skip the check against filesize
     */
    if ((type != 0x2007) && (type != 0x580b) && (type != 0x501c) &&
        (type != 0x5029) && (type != 0x5813) && (type != 0x5814) &&
        (type != 0x5817) && (type != 0x5834) && (type != 0x580e))
    {

      if (see >= fsize)
      { // At least one byte
        fseek(ifp, save, SEEK_SET);
        continue;
      }
      fseek(ifp, see, SEEK_SET);
      if ((((type >> 8) + 8) | 8) == 0x38)
      {
        parse_ciff(ftell(ifp), len, depth + 1); /* Parse a sub-table */
      }
    }

    if (type == 0x3004)
    {
      parse_ciff(ftell(ifp), len, depth + 1);
    }
    else if (type == 0x0810)
    {
      fread(artist, 64, 1, ifp);
    }
    else if (type == 0x080a)
    {
      fread(make, 64, 1, ifp);
      fseek(ifp, strbuflen(make) - 63, SEEK_CUR);
      fread(model, 64, 1, ifp);
    }
    else if (type == 0x1810)
    {
      width = get4();
      height = get4();
      pixel_aspect = int_to_float(get4());
      flip = get4();
    }
    else if (type == 0x1835)
    { /* Get the decoder table */
      tiff_compress = get4();
    }
    else if (type == 0x2007)
    {
      thumb_offset = see;
      thumb_length = len;
    }
    else if (type == 0x1818)
    {
      shutter = libraw_powf64l(2.0f, -int_to_float((get4(), get4())));
      ilm.CurAp = aperture = libraw_powf64l(2.0f, int_to_float(get4()) / 2);
    }
    else if (type == 0x102a)
    {
      //      iso_speed = pow (2.0, (get4(),get2())/32.0 - 4) * 50;
      get2(); // skip one
      iso_speed =
          libraw_powf64l(2.0f, (get2() + get2()) / 32.0f - 5.0f) * 100.0f;
      ilm.CurAp = aperture = _CanonConvertAperture((get2(), get2()));
      shutter = libraw_powf64l(2.0, -((short)get2()) / 32.0);
      wbi = (get2(), get2());
      if (wbi > 17)
        wbi = 0;
      fseek(ifp, 32, SEEK_CUR);
      if (shutter > 1e6)
        shutter = get2() / 10.0;
    }
    else if (type == 0x102c)
    {
      if (get2() > 512)
      { /* Pro90, G1 */
        fseek(ifp, 118, SEEK_CUR);
        FORC4 cam_mul[c ^ 2] = get2();
      }
      else
      { /* G2, S30, S40 */
        fseek(ifp, 98, SEEK_CUR);
        FORC4 cam_mul[c ^ (c >> 1) ^ 1] = get2();
      }
    }
    else if (type == 0x10a9)
    { /* Canon D60, 10D, 300D, and clones */

      int bls = 0;
      int table[] = {
          LIBRAW_WBI_Auto,     // 0
          LIBRAW_WBI_Daylight, // 1
          LIBRAW_WBI_Cloudy,   // 2
          LIBRAW_WBI_Tungsten, // 3
          LIBRAW_WBI_FL_W,     // 4
          LIBRAW_WBI_Flash,    // 5
          LIBRAW_WBI_Custom,   // 6, absent in Canon D60
          LIBRAW_WBI_Auto,     // 7, use this if camera is set to b/w JPEG
          LIBRAW_WBI_Shade,    // 8
          LIBRAW_WBI_Kelvin    // 9, absent in Canon D60
      };
      int nWB =
          ((get2() - 2) / 8) -
          1; // 2 bytes this, N recs 4*2bytes each, last rec is black level
      if (nWB)
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][c ^ (c >> 1)] = get2();
      if (nWB >= 7)
        Canon_WBpresets(0, 0);
      else
        FORC4 cam_mul[c] = imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][c];
      if (nWB == 7)
      { /* mostly Canon EOS D60 + some fw#s for 300D; check for 0x1668000 is
           unreliable */
        if ((wbi >= 0) && (wbi < 9) && (wbi != 6))
        {
          FORC4 cam_mul[c] = imgdata.color.WB_Coeffs[wbi][c];
        }
        else
        {
          FORC4 cam_mul[c] = imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][c];
        }
      }
      else if (nWB == 9)
      { /* Canon 10D, 300D */
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Custom][c ^ (c >> 1)] = get2();
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Kelvin][c ^ (c >> 1)] = get2();
        if ((wbi >= 0) && (wbi < 10))
        {
          FORC4 cam_mul[c] = imgdata.color.WB_Coeffs[wbi][c];
        }
        else
        {
          FORC4 cam_mul[c] = imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][c];
        }
      }
      FORC4
      bls += (imCanon.ChannelBlackLevel[c ^ (c >> 1)] = get2());
      imCanon.AverageBlackLevel = bls / 4;
    }
    else if (type == 0x102d)
    {
      Canon_CameraSettings(len >> 1);
    }
    else if (type == 0x580b)
    {
      if (strcmp(model, "Canon EOS D30"))
        sprintf(imgdata.shootinginfo.BodySerial, "%d", len);
      else
        sprintf(imgdata.shootinginfo.BodySerial, "%0x-%05d", len >> 16,
                len & 0xffff);
    }
    else if (type == 0x0032)
    {
      if (len == 768)
      { /* EOS D30 */
        fseek(ifp, 72, SEEK_CUR);
        FORC4
        {
          ushort q = get2();
          cam_mul[c ^ (c >> 1)] = 1024.0 / MAX(1, q);
        }
        if (!wbi)
          cam_mul[0] = -1; /* use my auto white balance */
      }
      else if (cam_mul[0] <= 0.001f)
      {
        if (get2() == key[0]) /* Pro1, G6, S60, S70 */
          c = (strstr(model, "Pro1") ? "012346000000000000"
                                     : "01345:000000006008")[LIM(0, wbi, 17)] -
              '0' + 2;
        else
        { /* G3, G5, S45, S50 */
          c = "023457000000006000"[LIM(0, wbi, 17)] - '0';
          key[0] = key[1] = 0;
        }
        fseek(ifp, 78 + c * 8, SEEK_CUR);
        FORC4 cam_mul[c ^ (c >> 1) ^ 1] = get2() ^ key[c & 1];
        if (!wbi)
          cam_mul[0] = -1;
      }
    }
    else if (type == 0x1030 && wbi >= 0 && (0x18040 >> wbi & 1))
    {
      ciff_block_1030(); /* all that don't have 0x10a9 */
    }
    else if (type == 0x1031)
    {
      raw_width = (get2(), get2());
      raw_height = get2();
    }
    else if (type == 0x501c)
    {
      iso_speed = len & 0xffff;
    }
    else if (type == 0x5029)
    {
      ilm.CurFocal = len >> 16;
      ilm.FocalType = len & 0xffff;
      if (ilm.FocalType == LIBRAW_FT_ZOOM_LENS)
      {
        ilm.FocalUnits = 32;
        if (ilm.FocalUnits > 1)
          ilm.CurFocal /= (float)ilm.FocalUnits;
      }
      focal_len = ilm.CurFocal;
    }
    else if (type == 0x5813)
    {
      flash_used = int_to_float(len);
    }
    else if (type == 0x5814)
    {
      canon_ev = int_to_float(len);
    }
    else if (type == 0x5817)
    {
      shot_order = len;
    }
    else if (type == 0x5834)
    {
      unique_id = ((unsigned long long)len << 32) >> 32;
      setCanonBodyFeatures(unique_id);
    }
    else if (type == 0x580e)
    {
      timestamp = len;
    }
    else if (type == 0x180e)
    {
      timestamp = get4();
    }

#ifdef LOCALTIME
    if ((type | 0x4000) == 0x580e)
      timestamp = mktime(gmtime(&timestamp));
#endif
    fseek(ifp, save, SEEK_SET);
  }
}
