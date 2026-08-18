/* -*- C++ -*-
 * Copyright 2019-2025 LibRaw LLC (info@libraw.org)
 *

 LibRaw is free software; you can redistribute it and/or modify
 it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).
 */

/* Library for accessing X3F Files
----------------------------------------------------------------
BSD-style License
----------------------------------------------------------------

* Copyright (c) 2010, Roland Karlsson (roland@proxel.se)
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the organization nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY ROLAND KARLSSON ''AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL ROLAND KARLSSON BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifdef USE_X3FTOOLS

#include "../../internal/libraw_cxx_defs.h"

#if defined __sun && defined DS
#undef DS
#endif
#ifdef ID
#undef ID /* used in x3f utils */
#endif

#include "../../internal/x3f_tools.h"

#define Sigma_X3F 22

void x3f_clear(void *p) { x3f_delete((x3f_t *)p); }

static void utf2char(utf16_t *str, char *buffer, unsigned bufsz)
{
  if (bufsz < 1)
    return;
  buffer[bufsz - 1] = 0;
  char *b = buffer;

  while (*str != 0x00 && --bufsz > 0)
  {
    char *chr = (char *)str;
    *b++ = *chr;
    str++;
  }
  *b = 0;
}

static void *lr_memmem(const void *l, size_t l_len, const void *s, size_t s_len)
{
  char *cur, *last;
  const char *cl = (const char *)l;
  const char *cs = (const char *)s;

  /* we need something to compare */
  if (l_len == 0 || s_len == 0)
    return NULL;

  /* "s" must be smaller or equal to "l" */
  if (l_len < s_len)
    return NULL;

  /* special case where s_len == 1 */
  if (s_len == 1)
    return (void *)memchr(l, (int)*cs, l_len);

  /* the last position where its possible to find "s" in "l" */
  last = (char *)cl + l_len - s_len;

  for (cur = (char *)cl; cur <= last; cur++)
    if (cur[0] == cs[0] && memcmp(cur, cs, s_len) == 0)
      return cur;
  return NULL;
}

/* Look up `key` inside the named CAMF PROP block and return its value,
   or NULL if either the block or the key is not present. */
static const char *x3f_camf_find_prop(camf_entry_t *entries, int n_entries,
                                      const char *block, const char *key)
{
  for (int i = 0; i < n_entries; i++)
  {
    if (entries[i].id != X3F_CMbP || !entries[i].name_address)
      continue;
    if (strcmp(entries[i].name_address, block))
      continue;
    for (uint32_t j = 0; j < entries[i].property_num; j++)
    {
      if (entries[i].property_name[j] &&
          !strcmp(entries[i].property_name[j], key))
        return (const char *)entries[i].property_value[j];
    }
  }
  return NULL;
}

/* Return the decoded payload of the named CAMF M_FLOAT matrix, or NULL
   if no matrix with that name and type exists. M_FLOAT matrices are
   stored as double[] (see get_matrix_copy). */
static const double *x3f_camf_find_float_matrix(camf_entry_t *entries,
                                                int n_entries,
                                                const char *name)
{
  for (int i = 0; i < n_entries; i++)
  {
    if (entries[i].id != X3F_CMbM || !entries[i].name_address)
      continue;
    if (strcmp(entries[i].name_address, name))
      continue;
    if (entries[i].matrix_decoded_type != M_FLOAT)
      continue;
    return (const double *)entries[i].matrix_decoded;
  }
  return NULL;
}

void LibRaw::parse_x3f()
{
  x3f_t *x3f = x3f_new_from_file(libraw_internal_data.internal_data.input);
  if (!x3f)
    return;
  _x3f_data = x3f;

  x3f_header_t *H = NULL;

  H = &x3f->header;
  // Parse RAW size from RAW section
  x3f_directory_entry_t *DE = x3f_get_raw(x3f);
  if (!DE)
    return;
  imgdata.sizes.flip = H->rotation;
  x3f_directory_entry_header_t *DEH = &DE->header;
  x3f_image_data_t *ID = &DEH->data_subsection.image_data;
  imgdata.sizes.raw_width = ID->columns;
  imgdata.sizes.raw_height = ID->rows;
  // Parse other params from property section

  DE = x3f_get_prop(x3f);
  if ((x3f_load_data(x3f, DE) == X3F_OK))
  {
    // Parse property list
    DEH = &DE->header;
    x3f_property_list_t *PL = &DEH->data_subsection.property_list;
    utf16_t *datap = (utf16_t *)PL->data;
    uint32_t maxitems = PL->data_size / sizeof(utf16_t);
    if (PL->property_table.size != 0)
    {
      int i;
      x3f_property_t *P = PL->property_table.element;
      for (i = 0; i < (int)PL->num_properties; i++)
      {
        char name[100], value[100];
        int noffset = int(P[i].name - datap);
        int voffset = int(P[i].value - datap);
        if (noffset < 0 || noffset > (int)maxitems || voffset < 0 ||
            voffset > (int)maxitems)
          throw LIBRAW_EXCEPTION_IO_CORRUPT;
        int maxnsize = maxitems - int(P[i].name - datap);
        int maxvsize = maxitems - int(P[i].value - datap);
        utf2char(P[i].name, name, MIN(maxnsize, ((int)sizeof(name))));
        utf2char(P[i].value, value, MIN(maxvsize, ((int)sizeof(value))));
        if (!strcmp(name, "ISO"))
          imgdata.other.iso_speed = float(atoi(value));
        if (!strcmp(name, "CAMMANUF"))
          strncpy(imgdata.idata.make, value, sizeof(imgdata.idata.make)-1);
        if (!strcmp(name, "CAMMODEL"))
          strncpy(imgdata.idata.model, value, sizeof(imgdata.idata.model) - 1);
        if (!strcmp(name, "CAMSERIAL"))
          strncpy(imgdata.shootinginfo.BodySerial, value, sizeof(imgdata.shootinginfo.BodySerial) - 1);
        if (!strcmp(name, "WB_DESC"))
          strncpy(imgdata.color.model2, value, sizeof(imgdata.color.model2) - 1);
        if (!strcmp(name, "TIME"))
          imgdata.other.timestamp = atoi(value);
        if (!strcmp(name, "SHUTTER"))
          imgdata.other.shutter = float(atof(value));
        if (!strcmp(name, "APERTURE"))
          imgdata.other.aperture = float(atof(value));
        if (!strcmp(name, "FLENGTH"))
          imgdata.other.focal_len = float(atof(value));
        if (!strcmp(name, "FLEQ35MM"))
          imgdata.lens.makernotes.FocalLengthIn35mmFormat = float(atof(value));
        if (!strcmp(name, "IMAGERTEMP"))
          MN.common.SensorTemperature = float(atof(value));
        if (!strcmp(name, "LENSARANGE"))
        {
          char *sp;
          imgdata.lens.makernotes.MaxAp4CurFocal =
              imgdata.lens.makernotes.MinAp4CurFocal = float(atof(value));
          sp = strrchr(value, ' ');
          if (sp)
          {
            imgdata.lens.makernotes.MinAp4CurFocal = float(atof(sp));
            if (imgdata.lens.makernotes.MaxAp4CurFocal >
                imgdata.lens.makernotes.MinAp4CurFocal)
              my_swap(float, imgdata.lens.makernotes.MaxAp4CurFocal,
                      imgdata.lens.makernotes.MinAp4CurFocal);
          }
        }
        if (!strcmp(name, "LENSFRANGE"))
        {
          char *sp;
          imgdata.lens.makernotes.MinFocal = imgdata.lens.makernotes.MaxFocal = float(atof(value));
          sp = strrchr(value, ' ');
          if (sp)
          {
            imgdata.lens.makernotes.MaxFocal = float(atof(sp));
            if ((imgdata.lens.makernotes.MaxFocal + 0.17f) <
                imgdata.lens.makernotes.MinFocal)
              my_swap(float, imgdata.lens.makernotes.MaxFocal,
                      imgdata.lens.makernotes.MinFocal);
          }
        }
        if (!strcmp(name, "LENSMODEL"))
        {
          char *sp;
          imgdata.lens.makernotes.LensID =
              strtol(value, &sp, 16); // atoi(value);
          if (imgdata.lens.makernotes.LensID)
            imgdata.lens.makernotes.LensMount = Sigma_X3F;
        }
      }
      imgdata.idata.raw_count = 1;
      load_raw = &LibRaw::x3f_load_raw;
      imgdata.sizes.raw_pitch = imgdata.sizes.raw_width * 6;
      imgdata.idata.is_foveon = 1;
      libraw_internal_data.internal_output_params.raw_color =
          1;                          // Force adobe coeff
      imgdata.color.maximum = 0x3fff; // To be reset by color table
      libraw_internal_data.unpacker_data.order = 0x4949;
    }
  }
  else
  {
    // No property list
     // sd Quattro H: 6656x4480, 3328x2240, 5504x3680, 2752x1840
    // dpN Quattro:  5888x3672, 2944x1836
    // sd Quattro:   5888x3776, 2944x1888
    if ((imgdata.sizes.raw_width == 5888) || // Quattro: dp0, dp1, dp2, sd
        (imgdata.sizes.raw_width == 2944) || // Quattro: dp0, dp1, dp2, sd
        (imgdata.sizes.raw_width == 6656) || // sd Quattro H
        (imgdata.sizes.raw_width == 3328) || // sd Quattro H
        (imgdata.sizes.raw_width == 5504) || // sd Quattro H APS-C
        (imgdata.sizes.raw_width == 2752))   // sd Quattro H APS-C
    {
      imgdata.idata.raw_count = 1;
      load_raw = &LibRaw::x3f_load_raw;
      imgdata.sizes.raw_pitch = imgdata.sizes.raw_width * 6;
      imgdata.idata.is_foveon = 1;
      libraw_internal_data.internal_output_params.raw_color =
          1; // Force adobe coeff
      libraw_internal_data.unpacker_data.order = 0x4949;
      strcpy(imgdata.idata.make, "SIGMA");
#if 1
      // Try to find model number in first 2048 bytes;
      INT64 pos = libraw_internal_data.internal_data.input->tell();
      libraw_internal_data.internal_data.input->seek(0, SEEK_SET);
      unsigned char buf[2048];
      libraw_internal_data.internal_data.input->read(buf, 2048, 1);
      libraw_internal_data.internal_data.input->seek(pos, SEEK_SET);
      unsigned char *fnd = (unsigned char *)lr_memmem(buf, 2048, "SIGMA dp", 8);
      unsigned char *fndsd =
          (unsigned char *)lr_memmem(buf, 2048, "sd Quatt", 8);
      if (fnd)
      {
        unsigned char *nm = fnd + 8;
        snprintf(imgdata.idata.model, 64, "dp%c Quattro",
                 *nm <= '9' && *nm >= '0' ? *nm : '2');
      }
      else if (fndsd)
      {
        unsigned char *fndsdQH = (unsigned char *)lr_memmem(fndsd, 20, "sd Quattro H", 12);
        if (fndsdQH)
            snprintf(imgdata.idata.model, 64, "%s", fndsdQH);
        else
            snprintf(imgdata.idata.model, 64, "%s", fndsd);
    }
      else
#endif
        if ((imgdata.sizes.raw_width == 6656) ||
            (imgdata.sizes.raw_width == 3328) ||
            (imgdata.sizes.raw_width == 5504) ||
            (imgdata.sizes.raw_width == 2752))
            strcpy(imgdata.idata.model, "sd Quattro H");
        else if ((imgdata.sizes.raw_height == 3776) ||
                 (imgdata.sizes.raw_height == 1888))
            strcpy(imgdata.idata.model, "sd Quattro");
        else // defaulting to dp2 Quattro
            strcpy(imgdata.idata.model, "dp2 Quattro");
    }
    // else
  }
  // Try to get thumbnail data
  LibRaw_thumbnail_formats format = LIBRAW_THUMBNAIL_UNKNOWN;
  if ((DE = x3f_get_thumb_jpeg(x3f)))
  {
    format = LIBRAW_THUMBNAIL_JPEG;
  }
  else if ((DE = x3f_get_thumb_plain(x3f)))
  {
    format = LIBRAW_THUMBNAIL_BITMAP;
  }
  if (DE)
  {
    x3f_directory_entry_header_t *_DEH = &DE->header;
    x3f_image_data_t *_ID = &_DEH->data_subsection.image_data;
    imgdata.thumbnail.twidth = _ID->columns;
    imgdata.thumbnail.theight = _ID->rows;
    imgdata.thumbnail.tcolors = 3;
    imgdata.thumbnail.tformat = format;
    libraw_internal_data.internal_data.toffset = DE->input.offset;
    libraw_internal_data.unpacker_data.thumb_format = LIBRAW_INTERNAL_THUMBNAIL_X3F;
  }
  DE = x3f_get_camf(x3f);
  if (DE && DE->input.size > 28)
  {
    libraw_internal_data.unpacker_data.meta_offset = DE->input.offset + 8;
    libraw_internal_data.unpacker_data.meta_length = DE->input.size - 28;

    // Decode CAMF and read the camera-recorded WB gains into cam_mul[],
    // so the as-shot multipliers are available instead of the
    // matrix-derived neutral default. Sigma CAMF stores per-preset
    // R/G/B gains as 3-element M_FLOAT matrices (e.g. SunlightWBGain,
    // AutoWBGain) named by the WhiteBalanceGains property block. CAMF
    // is reverse-engineered; the bundled X3F decoder is derived from
    // the Kalpanika x3f_tools project (https://github.com/Kalpanika/x3f),
    // which is the de facto reference for the format.
    try
    {
      if (x3f_load_data(x3f, DE) == X3F_OK)
      {
        x3f_camf_t *CAMF = &DE->header.data_subsection.camf;
        camf_entry_t *entries = CAMF->entry_table.element;
        const int n_entries = (int)CAMF->entry_table.size;

        // The WhiteBalance entry is the camera-menu enum (0=Auto,
        // 1=AutoLSP, 2=Daylight, ...) — NOT an index into the
        // WhiteBalanceGains PROP block (whose own order does not match
        // the menu). Map the enum to the CAMF preset name, then look
        // that name up to find the gain matrix (e.g. "AutoLSP" ->
        // "AutoLSPWBGain", 3 R/G/B multipliers).
        static const char *const kSigmaWBPresets[] = {
            "Auto",         // 0
            "AutoLSP",      // 1
            "Sunlight",     // 2  (camera UI label: "Daylight")
            "Shade",        // 3
            "Overcast",     // 4
            "Incandescent", // 5
            "Fluorescent",  // 6
            NULL,           // 7  Color Temperature — manual K, no preset
            "Flash",        // 8
            // All three Custom slots share the single "Custom" entry in
            // WhiteBalanceGains, which points at a single CustomWBGain
            // matrix holding the as-shot gain for whichever slot was
            // active at capture. There is no Custom1/2/3 distinction in
            // CAMF (verified on sd Quattro and DP Merrill samples).
            "Custom",       // 9  Custom 1
            "Custom",       // 10 Custom 2
            "Custom",       // 11 Custom 3
        };
        const int n_presets = (int)(sizeof(kSigmaWBPresets) /
                                    sizeof(kSigmaWBPresets[0]));
        const char *gain_name = NULL;
        for (int i = 0; i < n_entries && !gain_name; i++)
        {
          if (entries[i].id != X3F_CMbM || !entries[i].name_address)
            continue;
          if (strcmp(entries[i].name_address, "WhiteBalance"))
            continue;
          if (entries[i].matrix_decoded_type != M_UINT)
            continue;
          if (entries[i].matrix_elements < 1)
            continue;
          const uint32_t idx = ((uint32_t *)entries[i].matrix_decoded)[0];
          const char *preset = ((int)idx < n_presets) ? kSigmaWBPresets[idx]
                                                      : NULL;
          if (preset)
            gain_name = x3f_camf_find_prop(entries, n_entries,
                                           "WhiteBalanceGains", preset);
        }
        const double *gains =
            gain_name ? x3f_camf_find_float_matrix(entries, n_entries, gain_name)
                      : NULL;

        if (gains && gains[0] > 0.0 && gains[1] > 0.0 && gains[2] > 0.0)
        {
          imgdata.color.cam_mul[0] = (float)gains[0];
          imgdata.color.cam_mul[1] = (float)gains[1];
          imgdata.color.cam_mul[2] = (float)gains[2];
          imgdata.color.cam_mul[3] = (float)gains[1];
        }
      }
    }
    catch (...)
    {
      // CAMF WB extraction is best-effort; on any error, leave
      // cam_mul at its previous value.
    }
  }
}

int LibRaw::x3f_thumb_size()
{
  try
  {
    x3f_t *x3f = (x3f_t *)_x3f_data;
    if (!x3f)
      return -1; // No data pointer set
    x3f_directory_entry_t *DE = x3f_get_thumb_jpeg(x3f);
    if (!DE)
      DE = x3f_get_thumb_plain(x3f);
    if (!DE)
      return -1;
    int32_t p = x3f_load_data_size(x3f, DE);
    if (p < 0 || p > 0xffffffff)
      return -1;
    return p;
  }
  catch (...)
  {
    return -1;
  }
}

void LibRaw::x3f_thumb_loader()
{
  try
  {
    INT64 checked_size = x3f_thumb_size(); // This value was checked at upper level?
    x3f_t *x3f = (x3f_t *)_x3f_data;
    if (!x3f)
      return; // No data pointer set
    x3f_directory_entry_t *DE = x3f_get_thumb_jpeg(x3f);
    if (!DE)
      DE = x3f_get_thumb_plain(x3f);
    if (!DE)
      return;
    if (X3F_OK != x3f_load_data(x3f, DE))
      throw LIBRAW_EXCEPTION_IO_CORRUPT;
    x3f_directory_entry_header_t *DEH = &DE->header;
    x3f_image_data_t *ID = &DEH->data_subsection.image_data;
    imgdata.thumbnail.twidth = ID->columns;
    imgdata.thumbnail.theight = ID->rows;
    imgdata.thumbnail.tcolors = 3;
    if (imgdata.thumbnail.tformat == LIBRAW_THUMBNAIL_JPEG)
    {
	  INT64 alloc_size = ID->data_size;
	  if ((alloc_size > 2 * checked_size) || (alloc_size > 1024LL * 1024LL * LIBRAW_MAX_THUMBNAIL_MB))
		  throw LIBRAW_EXCEPTION_TOOBIG;
	  if(alloc_size < 64LL)
        throw LIBRAW_EXCEPTION_IO_CORRUPT;

	  imgdata.thumbnail.thumb = (char *)malloc(ID->data_size);
      memmove(imgdata.thumbnail.thumb, ID->data, ID->data_size);
      imgdata.thumbnail.tlength = ID->data_size;
    }
    else if (imgdata.thumbnail.tformat == LIBRAW_THUMBNAIL_BITMAP)
    {
      INT64 alloc_size = INT64(ID->columns) * INT64(ID->rows) * 3LL;
	  if ((alloc_size > 2 * checked_size) ||
          (alloc_size > 1024LL * 1024LL * LIBRAW_MAX_THUMBNAIL_MB)) throw LIBRAW_EXCEPTION_TOOBIG;
      if (alloc_size < 64LL)
        throw LIBRAW_EXCEPTION_IO_CORRUPT;

      imgdata.thumbnail.tlength = ID->columns * ID->rows * 3;
      imgdata.thumbnail.thumb = (char *)malloc(ID->columns * ID->rows * 3);
      char *src0 = (char *)ID->data;
      for (int row = 0; row < (int)ID->rows; row++)
      {
        int offset = row * ID->row_stride;
        if (offset + ID->columns * 3 > ID->data_size)
          break;
        char *dest = &imgdata.thumbnail.thumb[row * ID->columns * 3];
        char *src = &src0[offset];
        memmove(dest, src, ID->columns * 3);
      }
    }
  }
  catch (...)
  {
    // no rethrow: handled at upper level
    imgdata.thumbnail.twidth = 0;
    imgdata.thumbnail.theight = 0;
    imgdata.thumbnail.tcolors = 0;
  }
}

// The Foveon Quattro top (blue-sensitive) layer is sampled at twice the
// linear density of the R/G layers below it — Sigma's 1:1:4 R:G:Y
// design (sd/dp Quattro: T 5424x3616 vs M/B 2712x1808 each, see
// https://press.sigmaphoto.com/corporate/02/sigma-dp-quattro/).
// Bilinearly upscale the color-difference signals (R - B) and (G - B)
// so R and G inherit the top layer's detail for free; luma cancels in
// the difference. Plain 2x2 nearest-neighbor replication of the R/G
// samples instead leaves a visible chroma maze in flat regions.
void LibRaw::x3f_dpq_interpolate_rg()
{
  const int W = imgdata.sizes.raw_width;
  const int H = imgdata.sizes.raw_height;
  const int rowStride = W * 3;
  unsigned short *image = (unsigned short *)imgdata.rawdata.color3_image;

  for (int Y = 0; Y < H; Y++)
  {
    const int Y_lo = Y & ~1;
    const int Y_hi = (Y_lo + 2 < H) ? (Y_lo + 2) : Y_lo;
    const float fy = (Y_hi == Y_lo) ? 0.f : 0.5f * float(Y - Y_lo);

    for (int X = 0; X < W; X++)
    {
      if (((Y | X) & 1) == 0) continue;
      const int X_lo = X & ~1;
      const int X_hi = (X_lo + 2 < W) ? (X_lo + 2) : X_lo;
      const float fx = (X_hi == X_lo) ? 0.f : 0.5f * float(X - X_lo);

      const int p_TL = Y_lo * rowStride + X_lo * 3;
      const int p_TR = Y_lo * rowStride + X_hi * 3;
      const int p_BL = Y_hi * rowStride + X_lo * 3;
      const int p_BR = Y_hi * rowStride + X_hi * 3;
      const int p_C  = Y    * rowStride + X    * 3;

      const float wTL = (1.f - fy) * (1.f - fx);
      const float wTR = (1.f - fy) * fx;
      const float wBL = fy * (1.f - fx);
      const float wBR = fy * fx;

      const float B_C  = float(image[p_C  + 2]);
      const float B_TL = float(image[p_TL + 2]);
      const float B_TR = float(image[p_TR + 2]);
      const float B_BL = float(image[p_BL + 2]);
      const float B_BR = float(image[p_BR + 2]);

      for (int color = 0; color < 2; color++)
      {
        const float d = wTL * (float(image[p_TL + color]) - B_TL) +
                        wTR * (float(image[p_TR + color]) - B_TR) +
                        wBL * (float(image[p_BL + color]) - B_BL) +
                        wBR * (float(image[p_BR + color]) - B_BR);
        const float v = B_C + d;
        const float vC = v > 16383.f ? 16383.f : (v < 0.f ? 0.f : v);
        image[p_C + color] = (unsigned short)(vC + 0.5f);
      }
    }
  }
}

#ifdef _ABS
#undef _ABS
#endif
#define _ABS(a) ((a) < 0 ? -(a) : (a))

#undef CLIP
#define CLIP(value, high) ((value) > (high) ? (high) : (value))

void LibRaw::x3f_dpq_interpolate_af(int xstep, int ystep, int scale)
{
  unsigned short *image = (ushort *)imgdata.rawdata.color3_image;
  for (int y = 0;
       y < imgdata.rawdata.sizes.height + imgdata.rawdata.sizes.top_margin;
       y += ystep)
  {
    if (y < imgdata.rawdata.sizes.top_margin)
      continue;
    if (y < scale)
      continue;
    if (y > imgdata.rawdata.sizes.raw_height - scale)
      break;
    uint16_t *row0 = &image[imgdata.sizes.raw_width * 3 * y]; // Наша строка
    uint16_t *row_minus =
        &image[imgdata.sizes.raw_width * 3 * (y - scale)]; // Строка выше
    uint16_t *row_plus =
        &image[imgdata.sizes.raw_width * 3 * (y + scale)]; // Строка ниже
    for (int x = 0;
         x < imgdata.rawdata.sizes.width + imgdata.rawdata.sizes.left_margin;
         x += xstep)
    {
      if (x < imgdata.rawdata.sizes.left_margin)
        continue;
      if (x < scale)
        continue;
      if (x > imgdata.rawdata.sizes.raw_width - scale)
        break;
      uint16_t *pixel0 = &row0[x * 3];
      uint16_t *pixel_top = &row_minus[x * 3];
      uint16_t *pixel_bottom = &row_plus[x * 3];
      uint16_t *pixel_left = &row0[(x - scale) * 3];
      uint16_t *pixel_right = &row0[(x + scale) * 3];
      uint16_t *pixf = pixel_top;
      if (_ABS(pixf[2] - pixel0[2]) > _ABS(pixel_bottom[2] - pixel0[2]))
        pixf = pixel_bottom;
      if (_ABS(pixf[2] - pixel0[2]) > _ABS(pixel_left[2] - pixel0[2]))
        pixf = pixel_left;
      if (_ABS(pixf[2] - pixel0[2]) > _ABS(pixel_right[2] - pixel0[2]))
        pixf = pixel_right;
      int blocal = pixel0[2], bnear = pixf[2];
      if (blocal < (int)imgdata.color.black + 16 || bnear < (int)imgdata.color.black + 16)
      {
        if (pixel0[0] < imgdata.color.black)
          pixel0[0] = imgdata.color.black;
        if (pixel0[1] < imgdata.color.black)
          pixel0[1] = imgdata.color.black;
        pixel0[0] = CLIP(
            (pixel0[0] - imgdata.color.black) * 4 + imgdata.color.black, 16383);
        pixel0[1] = CLIP(
            (pixel0[1] - imgdata.color.black) * 4 + imgdata.color.black, 16383);
      }
      else
      {
        float multip = float(bnear - imgdata.color.black) /
                       float(blocal - imgdata.color.black);
        if (pixel0[0] < imgdata.color.black)
          pixel0[0] = imgdata.color.black;
        if (pixel0[1] < imgdata.color.black)
          pixel0[1] = imgdata.color.black;
        float pixf0 = pixf[0];
        if (pixf0 < imgdata.color.black)
          pixf0 = float(imgdata.color.black);
        float pixf1 = pixf[1];
        if (pixf1 < imgdata.color.black)
          pixf1 = float(imgdata.color.black);

        pixel0[0] = uint16_t(CLIP(
            ((float(pixf0 - imgdata.color.black) * multip +
              imgdata.color.black) +
             ((pixel0[0] - imgdata.color.black) * 3.75 + imgdata.color.black)) /
                2,
            16383));
        pixel0[1] = uint16_t(CLIP(
            ((float(pixf1 - imgdata.color.black) * multip +
              imgdata.color.black) +
             ((pixel0[1] - imgdata.color.black) * 3.75 + imgdata.color.black)) /
                2,
            16383));
        // pixel0[1] = float(pixf[1]-imgdata.color.black)*multip +
        // imgdata.color.black;
      }
    }
  }
}

void LibRaw::x3f_dpq_interpolate_af_sd(int xstart, int ystart, int xend,
                                       int yend, int xstep, int ystep,
                                       int scale)
{
  unsigned short *image = (ushort *)imgdata.rawdata.color3_image;
  for (int y = ystart; y <= yend && y < imgdata.rawdata.sizes.height +
                                           imgdata.rawdata.sizes.top_margin;
       y += ystep)
  {
    uint16_t *row0 = &image[imgdata.sizes.raw_width * 3 * y]; // Наша строка
    uint16_t *row1 =
        &image[imgdata.sizes.raw_width * 3 * (y + 1)]; // Следующая строка
    uint16_t *row_minus =
        &image[imgdata.sizes.raw_width * 3 * (y - scale)]; // Строка выше
    uint16_t *row_plus =
        &image[imgdata.sizes.raw_width * 3 *
               (y + scale)]; // Строка ниже AF-point (scale=2 -> ниже row1
    uint16_t *row_minus1 = &image[imgdata.sizes.raw_width * 3 * (y - 1)];
    for (int x = xstart; x < xend && x < imgdata.rawdata.sizes.width +
                                             imgdata.rawdata.sizes.left_margin;
         x += xstep)
    {
      uint16_t *pixel00 = &row0[x * 3]; // Current pixel
      float sumR = 0.f, sumG = 0.f;
      float cnt = 0.f;
      for (int xx = -scale; xx <= scale; xx += scale)
      {
        sumR += row_minus[(x + xx) * 3];
        sumR += row_plus[(x + xx) * 3];
        sumG += row_minus[(x + xx) * 3 + 1];
        sumG += row_plus[(x + xx) * 3 + 1];
        cnt += 1.f;
        if (xx)
        {
          cnt += 1.f;
          sumR += row0[(x + xx) * 3];
          sumG += row0[(x + xx) * 3 + 1];
        }
      }
      pixel00[0] = uint16_t(sumR / 8.f);
      pixel00[1] = uint16_t(sumG / 8.f);

      if (scale == 2)
      {
        uint16_t *pixel0B = &row0[x * 3 + 3]; // right pixel
        uint16_t *pixel1B = &row1[x * 3 + 3]; // right pixel
        float sumG0 = 0, sumG1 = 0.f;
        float _cnt = 0.f;
        for (int xx = -scale; xx <= scale; xx += scale)
        {
          sumG0 += row_minus1[(x + xx) * 3 + 2];
          sumG1 += row_plus[(x + xx) * 3 + 2];
          _cnt += 1.f;
          if (xx)
          {
            sumG0 += row0[(x + xx) * 3 + 2];
            sumG1 += row1[(x + xx) * 3 + 2];
            _cnt += 1.f;
          }
        }
        if (_cnt > 1.0)
        {
          pixel0B[2] = uint16_t(sumG0 / _cnt);
          pixel1B[2] = uint16_t(sumG1 / _cnt);
        }
      }

      //			uint16_t* pixel10 = &row1[x*3]; // Pixel below current
      //			uint16_t* pixel_bottom = &row_plus[x*3];
    }
  }
}

void LibRaw::x3f_load_raw()
{
  // already in try/catch
  int raise_error = 0;
  x3f_t *x3f = (x3f_t *)_x3f_data;
  if (!x3f)
    return; // No data pointer set
  if (X3F_OK == x3f_load_data(x3f, x3f_get_raw(x3f)))
  {
    x3f_directory_entry_t *DE = x3f_get_raw(x3f);
    x3f_directory_entry_header_t *DEH = &DE->header;
    x3f_image_data_t *ID = &DEH->data_subsection.image_data;
    if (!ID)
      throw LIBRAW_EXCEPTION_IO_CORRUPT;
    x3f_quattro_t *Q = ID->quattro;
    x3f_huffman_t *HUF = ID->huffman;
    x3f_true_t *TRU = ID->tru;
    uint16_t *data = NULL;
    if (ID->rows != S.raw_height || ID->columns != S.raw_width)
    {
      raise_error = 1;
      goto end;
    }
    if (HUF != NULL)
      data = HUF->x3rgb16.data;
    if (TRU != NULL)
      data = TRU->x3rgb16.data;
    if (data == NULL)
    {
      raise_error = 1;
      goto end;
    }

    size_t datasize = S.raw_height * S.raw_width * 3 * sizeof(unsigned short);
    S.raw_pitch = S.raw_width * 3 * sizeof(unsigned short);
    if (!(imgdata.rawdata.raw_alloc = malloc(datasize)))
      throw LIBRAW_EXCEPTION_ALLOC;

    imgdata.rawdata.color3_image = (ushort(*)[3])imgdata.rawdata.raw_alloc;
    // swap R/B channels for known old cameras
    if (!strcasecmp(P1.make, "Polaroid") && !strcasecmp(P1.model, "x530"))
    {
      ushort(*src)[3] = (ushort(*)[3])data;
      for (int p = 0; p < S.raw_height * S.raw_width; p++)
      {
        imgdata.rawdata.color3_image[p][0] = src[p][2];
        imgdata.rawdata.color3_image[p][1] = src[p][1];
        imgdata.rawdata.color3_image[p][2] = src[p][0];
      }
    }
    else if (HUF)
      memmove(imgdata.rawdata.raw_alloc, data, datasize);
    else if (TRU && (!Q || !Q->quattro_layout))
      memmove(imgdata.rawdata.raw_alloc, data, datasize);
    else if (TRU && Q)
    {
      // Move quattro data in place
      // R/B plane
      for (int prow = 0; prow < (int)TRU->x3rgb16.rows && prow < S.raw_height / 2;
           prow++)
      {
        ushort(*destrow)[3] =
            (unsigned short(*)[3]) &
            imgdata.rawdata
                .color3_image[prow * 2 * S.raw_pitch / 3 / sizeof(ushort)][0];
        ushort(*srcrow)[3] =
            (unsigned short(*)[3]) & data[prow * TRU->x3rgb16.row_stride];
        for (int pcol = 0;
             pcol < (int)TRU->x3rgb16.columns && pcol < S.raw_width / 2; pcol++)
        {
          destrow[pcol * 2][0] = srcrow[pcol][0];
          destrow[pcol * 2][1] = srcrow[pcol][1];
        }
      }
      for (int row = 0; row < (int)Q->top16.rows && row < S.raw_height; row++)
      {
        ushort(*destrow)[3] =
            (unsigned short(*)[3]) &
            imgdata.rawdata
                .color3_image[row * S.raw_pitch / 3 / sizeof(ushort)][0];
        ushort *srcrow =
            (unsigned short *)&Q->top16.data[row * Q->top16.columns];
        for (int col = 0; col < (int)Q->top16.columns && col < S.raw_width; col++)
          destrow[col][2] = srcrow[col];
      }
    }

#if 1
    if (TRU && Q &&
        !(imgdata.rawparams.specials & LIBRAW_RAWSPECIAL_NODP2Q_INTERPOLATEAF))
    {
      if (imgdata.sizes.raw_width == 5888 &&
          imgdata.sizes.raw_height == 3672) // dpN Quattro normal
      {
        x3f_dpq_interpolate_af(32, 8, 2);
      }
      else if (imgdata.sizes.raw_width == 5888 &&
               imgdata.sizes.raw_height == 3776) // sd Quattro normal raw
      {
        x3f_dpq_interpolate_af_sd(216, 464, imgdata.sizes.raw_width - 1, 3312,
                                  16, 32, 2);
      }
      else if (imgdata.sizes.raw_width == 6656 &&
               imgdata.sizes.raw_height == 4480) // sd Quattro H normal raw
      {
        x3f_dpq_interpolate_af_sd(232, 592, imgdata.sizes.raw_width - 1, 3920,
                                  16, 32, 2);
      }
      else if (imgdata.sizes.raw_width == 3328 &&
               imgdata.sizes.raw_height == 2240) // sd Quattro H half size
      {
        x3f_dpq_interpolate_af_sd(116, 296, imgdata.sizes.raw_width - 1, 2200,
                                  8, 16, 1);
      }
      else if (imgdata.sizes.raw_width == 5504 &&
               imgdata.sizes.raw_height == 3680) // sd Quattro H APS-C raw
      {
        x3f_dpq_interpolate_af_sd(8, 192, imgdata.sizes.raw_width - 1, 3185, 16,
                                  32, 2);
      }
      else if (imgdata.sizes.raw_width == 2752 &&
               imgdata.sizes.raw_height == 1840) // sd Quattro H APS-C half size
      {
        x3f_dpq_interpolate_af_sd(4, 96, imgdata.sizes.raw_width - 1, 1800, 8,
                                  16, 1);
      }
      else if (imgdata.sizes.raw_width == 2944 &&
               imgdata.sizes.raw_height == 1836) // dpN Quattro small raw
      {
        x3f_dpq_interpolate_af(16, 4, 1);
      }
      else if (imgdata.sizes.raw_width == 2944 &&
               imgdata.sizes.raw_height == 1888) // sd Quattro small
      {
        x3f_dpq_interpolate_af_sd(108, 232, imgdata.sizes.raw_width - 1, 1656,
                                  8, 16, 1);
      }
    }
#endif
    if (TRU && Q && Q->quattro_layout &&
        !(imgdata.rawparams.specials & LIBRAW_RAWSPECIAL_NODP2Q_INTERPOLATERG))
      x3f_dpq_interpolate_rg();
  }
  else
    raise_error = 1;
end:
  if (raise_error)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
}
#endif
