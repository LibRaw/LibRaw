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

void LibRaw::setPentaxBodyFeatures(unsigned long long id)
{

  ilm.CamID = id;

  switch (id)
  {
  case 0x12994:
  case 0x12aa2:
  case 0x12b1a:
  case 0x12b60:
  case 0x12b62:
  case 0x12b7e:
  case 0x12b80:
  case 0x12b9c:
  case 0x12b9d:
  case 0x12ba2:
  case 0x12c1e:
  case 0x12c20:
  case 0x12cd2:
  case 0x12cd4:
  case 0x12cfa:
  case 0x12d72:
  case 0x12d73:
  case 0x12db8:
  case 0x12dfe:
  case 0x12e6c:
  case 0x12e76:
  case 0x12ef8:
  case 0x12f52:
  case 0x12f70:
  case 0x12f71:
  case 0x12fb6:
  case 0x12fc0:
  case 0x12fca:
  case 0x1301a:
  case 0x13024:
  case 0x1309c:
  case 0x13222:
  case 0x1322c:
    ilm.CameraMount = LIBRAW_MOUNT_Pentax_K;
    ilm.CameraFormat = LIBRAW_FORMAT_APSC;
    break;
  case 0x13092: // K-1
  case 0x13240: // K-1 Mark II
    ilm.CameraMount = LIBRAW_MOUNT_Pentax_K;
    ilm.CameraFormat = LIBRAW_FORMAT_FF;
    break;
  case 0x12e08: // 645D
  case 0x13010: // 645Z
    ilm.CameraMount = LIBRAW_MOUNT_Pentax_645;
    ilm.CameraFormat = LIBRAW_FORMAT_CROP645;
    break;
  case 0x12ee4: // Q
  case 0x12f66: // Q10
    ilm.CameraMount = LIBRAW_MOUNT_Pentax_Q;
    ilm.CameraFormat = LIBRAW_FORMAT_1div2p3INCH;
    break;
  case 0x12f7a: // Q7
  case 0x1302e: // Q-S1
    ilm.CameraMount = LIBRAW_MOUNT_Pentax_Q;
    ilm.CameraFormat = LIBRAW_FORMAT_1div1p7INCH;
    break;
  case 0x12f84: // MX-1
    ilm.LensMount = LIBRAW_MOUNT_FixedLens;
    ilm.CameraMount = LIBRAW_MOUNT_FixedLens;
    ilm.CameraFormat = LIBRAW_FORMAT_1div1p7INCH;
    ilm.FocalType = LIBRAW_FT_ZOOM_LENS;
    break;
  case 0x1320e: // GR III
    ilm.CameraMount = LIBRAW_MOUNT_FixedLens;
    ilm.LensMount = LIBRAW_MOUNT_FixedLens;
    ilm.CameraFormat = LIBRAW_FORMAT_APSC;
    ilm.LensFormat = LIBRAW_FORMAT_APSC;
    ilm.FocalType = LIBRAW_FT_PRIME_LENS;
    break;
  default:
    ilm.LensMount = LIBRAW_MOUNT_FixedLens;
    ilm.CameraMount = LIBRAW_MOUNT_FixedLens;
  }
  return;
}

void LibRaw::PentaxISO(ushort c)
{
  int code[] = {3,    4,    5,   6,   7,   8,   9,   10,  11,  12,  13,  14,
                15,   16,   17,  18,  19,  20,  21,  22,  23,  24,  25,  26,
                27,   28,   29,  30,  31,  32,  33,  34,  35,  36,  37,  38,
                39,   40,   41,  42,  43,  44,  45,  50,  100, 200, 400, 800,
                1600, 3200, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267,
                268,  269,  270, 271, 272, 273, 274, 275, 276, 277, 278};
  double value[] = {
      50,     64,     80,     100,    125,    160,    200,    250,    320,
      400,    500,    640,    800,    1000,   1250,   1600,   2000,   2500,
      3200,   4000,   5000,   6400,   8000,   10000,  12800,  16000,  20000,
      25600,  32000,  40000,  51200,  64000,  80000,  102400, 128000, 160000,
      204800, 258000, 325000, 409600, 516000, 650000, 819200, 50,     100,
      200,    400,    800,    1600,   3200,   50,     70,     100,    140,
      200,    280,    400,    560,    800,    1100,   1600,   2200,   3200,
      4500,   6400,   9000,   12800,  18000,  25600,  36000,  51200};
#define numel (sizeof(code) / sizeof(code[0]))
  int i;
  for (i = 0; i < numel; i++)
  {
    if (code[i] == c)
    {
      iso_speed = value[i];
      return;
    }
  }
  if (i == numel)
    iso_speed = 65535.0f;
}
#undef numel

void LibRaw::PentaxLensInfo(unsigned long long id, unsigned len) // tag 0x0207
{
  ushort iLensData = 0;
  uchar *table_buf;
  table_buf = (uchar *)malloc(MAX(len, 128));
  fread(table_buf, len, 1, ifp);
  if ((id < 0x12b9c) || (((id == 0x12b9cULL) ||  // K100D
                          (id == 0x12b9dULL) ||  // K110D
                          (id == 0x12ba2ULL)) && // K100D Super
                         ((!table_buf[20] || (table_buf[20] == 0xff)))))
  {
    iLensData = 3;
    if (ilm.LensID == -1)
      ilm.LensID = (((unsigned)table_buf[0]) << 8) + table_buf[1];
  }
  else
    switch (len)
    {
    case 90: // LensInfo3
      iLensData = 13;
      if (ilm.LensID == -1)
        ilm.LensID = ((unsigned)((table_buf[1] & 0x0f) + table_buf[3]) << 8) +
                     table_buf[4];
      break;
    case 91: // LensInfo4
      iLensData = 12;
      if (ilm.LensID == -1)
        ilm.LensID = ((unsigned)((table_buf[1] & 0x0f) + table_buf[3]) << 8) +
                     table_buf[4];
      break;
    case 80: // LensInfo5
    case 128:
      iLensData = 15;
      if (ilm.LensID == -1)
        ilm.LensID = ((unsigned)((table_buf[1] & 0x0f) + table_buf[4]) << 8) +
                     table_buf[5];
      break;
    case 168: // Ricoh GR III, id 0x1320e
      break;
    default:
      if (id >= 0x12b9cULL) // LensInfo2
      {
        iLensData = 4;
        if (ilm.LensID == -1)
          ilm.LensID = ((unsigned)((table_buf[0] & 0x0f) + table_buf[2]) << 8) +
                       table_buf[3];
      }
    }
  if (iLensData)
  {
    if (table_buf[iLensData + 9] && (fabs(ilm.CurFocal) < 0.1f))
      ilm.CurFocal = 10 * (table_buf[iLensData + 9] >> 2) *
                     libraw_powf64l(4, (table_buf[iLensData + 9] & 0x03) - 2);
    if (table_buf[iLensData + 10] & 0xf0)
      ilm.MaxAp4CurFocal = libraw_powf64l(
          2.0f, (float)((table_buf[iLensData + 10] & 0xf0) >> 4) / 4.0f);
    if (table_buf[iLensData + 10] & 0x0f)
      ilm.MinAp4CurFocal = libraw_powf64l(
          2.0f, (float)((table_buf[iLensData + 10] & 0x0f) + 10) / 4.0f);

    if (iLensData != 12)
    {
      switch (table_buf[iLensData] & 0x06)
      {
      case 0:
        ilm.MinAp4MinFocal = 22.0f;
        break;
      case 2:
        ilm.MinAp4MinFocal = 32.0f;
        break;
      case 4:
        ilm.MinAp4MinFocal = 45.0f;
        break;
      case 6:
        ilm.MinAp4MinFocal = 16.0f;
        break;
      }
      if (table_buf[iLensData] & 0x70)
        ilm.LensFStops =
            ((float)(((table_buf[iLensData] & 0x70) >> 4) ^ 0x07)) / 2.0f +
            5.0f;

      ilm.MinFocusDistance = (float)(table_buf[iLensData + 3] & 0xf8);
      ilm.FocusRangeIndex = (float)(table_buf[iLensData + 3] & 0x07);

      if ((table_buf[iLensData + 14] > 1) && (fabs(ilm.MaxAp4CurFocal) < 0.7f))
        ilm.MaxAp4CurFocal = libraw_powf64l(
            2.0f, (float)((table_buf[iLensData + 14] & 0x7f) - 1) / 32.0f);
    }
    else if ((id != 0x12e76ULL) && // K-5
             (table_buf[iLensData + 15] > 1) &&
             (fabs(ilm.MaxAp4CurFocal) < 0.7f))
    {
      ilm.MaxAp4CurFocal = libraw_powf64l(
          2.0f, (float)((table_buf[iLensData + 15] & 0x7f) - 1) / 32.0f);
    }
  }
  free(table_buf);
  return;
}

void LibRaw::parsePentaxMakernotes(int base, unsigned tag, unsigned type,
                                   unsigned len, unsigned dng_writer)
{

  int c;
  if (tag == 0x0005)
  {
    unique_id = get4();
    setPentaxBodyFeatures(unique_id);
  }
  else if (tag == 0x0008)
  { /* 4 is raw, 7 is raw w/ pixel shift, 8 is raw w/ dynamic pixel shift */
    imPentax.Quality = get2();
  }
  else if (tag == 0x000d)
  {
    imgdata.shootinginfo.FocusMode = imPentax.FocusMode = get2();
  }
  else if (tag == 0x000e)
  {
    imgdata.shootinginfo.AFPoint = imPentax.AFPointSelected = get2();
  }
  else if (tag == 0x000f)
  {
    imPentax.AFPointsInFocus = getint(type);
  }
  else if (tag == 0x0010)
  {
    imPentax.FocusPosition = get2();
  }
  else if (tag == 0x0013)
  {
    ilm.CurAp = (float)get2() / 10.0f;
  }
  else if (tag == 0x0014)
  {
    PentaxISO(get2());
  }
  else if (tag == 0x0017)
  {
    imgdata.shootinginfo.MeteringMode = get2();
  }
  else if (tag == 0x001b) {
    cam_mul[2] = get2() / 256.0;
  }
  else if (tag == 0x001c) {
    cam_mul[0] = get2() / 256.0;
  }
  else if (tag == 0x001d)
  {
    ilm.CurFocal = (float)get4() / 100.0f;
  }
  else if (tag == 0x0034)
  {
    uchar uc;
    FORC4
    {
      fread(&uc, 1, 1, ifp);
      imPentax.DriveMode[c] = uc;
    }
    imgdata.shootinginfo.DriveMode = imPentax.DriveMode[0];
  }
  else if (tag == 0x0037)
  { // ColorSpace
    imCommon.ColorSpace = get2()+1;
  }
  else if (tag == 0x0038)
  {
    imgdata.sizes.raw_inset_crop.cleft = get2();
    imgdata.sizes.raw_inset_crop.ctop = get2();
  }
  else if (tag == 0x0039)
  {
    imgdata.sizes.raw_inset_crop.cwidth = get2();
    imgdata.sizes.raw_inset_crop.cheight = get2();
  }
  else if (tag == 0x003f)
  {
    unsigned a = fgetc(ifp) << 8;
    ilm.LensID = a | fgetc(ifp);
  }
  else if (tag == 0x0047)
  {
    imgdata.makernotes.common.CameraTemperature = (float)fgetc(ifp);
  }
  else if (tag == 0x004d)
  {
    if (type == 9)
      imgdata.makernotes.common.FlashEC = getreal(type) / 256.0f;
    else
      imgdata.makernotes.common.FlashEC = (float)((signed short)fgetc(ifp)) / 6.0f;
  }
  else if (tag == 0x005c)
  {
    fgetc(ifp);
    imgdata.shootinginfo.ImageStabilization = (short)fgetc(ifp);
  }
  else if (tag == 0x0072)
  {
    imPentax.AFAdjustment = get2();
  }
  else if ((tag == 0x007e) && (dng_writer == nonDNG))
  {
    imgdata.color.linear_max[0] = imgdata.color.linear_max[1] =
        imgdata.color.linear_max[2] = imgdata.color.linear_max[3] =
            get4();
  }
  else if (tag == 0x0080)
  {
    short a = (short)fgetc(ifp);
    switch (a)
    {
    case 0:
      imgdata.sizes.raw_inset_crop.aspect = LIBRAW_IMAGE_ASPECT_4to3;
      break;
    case 1:
      imgdata.sizes.raw_inset_crop.aspect = LIBRAW_IMAGE_ASPECT_3to2;
      break;
    case 2:
      imgdata.sizes.raw_inset_crop.aspect = LIBRAW_IMAGE_ASPECT_16to9;
      break;
    case 3:
      imgdata.sizes.raw_inset_crop.aspect = LIBRAW_IMAGE_ASPECT_1to1;
      break;
    }
  }

  else if ((tag == 0x0200) && (dng_writer == nonDNG)) { // Pentax black level
    FORC4 cblack[c ^ c >> 1] = get2();
  }

  else if ((tag == 0x0201) && (dng_writer == nonDNG)) { // Pentax As Shot WB
    FORC4 cam_mul[c ^ (c >> 1)] = get2();
  }

  else if ((tag == 0x0203) && (dng_writer == nonDNG))
  {
    for (int i = 0; i < 3; i++)
      FORC3 cmatrix[i][c] = ((short)get2()) / 8192.0;
  }
  else if (tag == 0x0205)
  { // Pentax K-m has multiexposure set to 8 when no multi-exposure is in effect
    if (len < 25)
    {
      fseek(ifp, 10, SEEK_CUR);
      imPentax.MultiExposure = fgetc(ifp) & 0x0f;
    }
  }
  else if (tag == 0x0207)
  {
    if (len < 65535) // Safety belt
      PentaxLensInfo(ilm.CamID, len);
  }
  else if ((tag >= 0x020d) && (tag <= 0x0214))
  {
    FORC4 imgdata.color.WB_Coeffs[Pentax_wb_list1[tag - 0x020d]][c ^ (c >> 1)] =
        get2();
  }

  else if ((tag == 0x0220) && (dng_writer == nonDNG)) {
    meta_offset = ftell(ifp);
  }

  else if (tag == 0x0221)
  {
    int nWB = get2();
    if (nWB <= sizeof(imgdata.color.WBCT_Coeffs) /
                   sizeof(imgdata.color.WBCT_Coeffs[0]))
      FORC(nWB)
      {
        imgdata.color.WBCT_Coeffs[c][0] = (unsigned)0xcfc6 - get2();
        fseek(ifp, 2, SEEK_CUR);
        imgdata.color.WBCT_Coeffs[c][1] = get2();
        imgdata.color.WBCT_Coeffs[c][2] = imgdata.color.WBCT_Coeffs[c][4] =
            0x2000;
        imgdata.color.WBCT_Coeffs[c][3] = get2();
      }
  }
  else if (tag == 0x0215)
  {
    fseek(ifp, 16, SEEK_CUR);
    sprintf(imgdata.shootinginfo.InternalBodySerial, "%d", get4());
  }
  else if (tag == 0x0229)
  {
    stmread(imgdata.shootinginfo.BodySerial, len, ifp);
  }
  else if (tag == 0x022d)
  {
    int wb_ind;
    getc(ifp);
    for (int wb_cnt = 0; wb_cnt < nPentax_wb_list2; wb_cnt++)
    {
      wb_ind = getc(ifp);
      if (wb_ind >= 0 && wb_ind < nPentax_wb_list2)
        FORC4 imgdata.color.WB_Coeffs[Pentax_wb_list2[wb_ind]][c ^ (c >> 1)] =
            get2();
    }
  }
  else if (tag == 0x0239)
  { // Q-series lens info (LensInfoQ)
    char LensInfo[20];
    fseek(ifp, 12, SEEK_CUR);
    stread(ilm.Lens, 30, ifp);
    strcat(ilm.Lens, " ");
    stread(LensInfo, 20, ifp);
    strcat(ilm.Lens, LensInfo);
  }
}

void LibRaw::parseRicohMakernotes(int base, unsigned tag, unsigned type,
                                  unsigned len, unsigned dng_writer)
{
  char buffer[17];
  if (tag == 0x0005)
  {
    int c;
    int count = 0;
    fread(buffer, 16, 1, ifp);
    buffer[16] = 0;
    FORC(16)
    {
      if ((isspace(buffer[c])) || (buffer[c] == 0x2D) || (isalnum(buffer[c])))
        count++;
      else
        break;
    }
    if (count == 16)
    {
      if (strncmp(model, "GXR", 3))
      {
        sprintf(imgdata.shootinginfo.BodySerial, "%8s", buffer + 8);
      }
      buffer[8] = 0;
      sprintf(imgdata.shootinginfo.InternalBodySerial, "%8s", buffer);
    }
    else
    {
      sprintf(imgdata.shootinginfo.BodySerial, "%02x%02x%02x%02x", buffer[4],
              buffer[5], buffer[6], buffer[7]);
      sprintf(imgdata.shootinginfo.InternalBodySerial, "%02x%02x%02x%02x",
              buffer[8], buffer[9], buffer[10], buffer[11]);
    }
  }
  else if ((tag == 0x1001) && (type == 3))
  {
    ilm.CameraMount = LIBRAW_MOUNT_FixedLens;
    ilm.LensMount = LIBRAW_MOUNT_FixedLens;
    ilm.CameraFormat = LIBRAW_FORMAT_APSC;
    ilm.LensID = -1;
    ilm.FocalType = LIBRAW_FT_PRIME_LENS;
    imgdata.shootinginfo.ExposureProgram = get2();
  }
  else if (tag == 0x1002)
  {
    imgdata.shootinginfo.DriveMode = get2();
  }
  else if (tag == 0x1006)
  {
    imgdata.shootinginfo.FocusMode = get2();
  }
  else if ((tag == 0x100b) && (type == 10))
  {
    imgdata.makernotes.common.FlashEC = getreal(type);
  }
  else if ((tag == 0x1017) && (get2() == 2))
  {
    strcpy(ilm.Attachment, "Wide-Angle Adapter");
  }
  else if (tag == 0x1500)
  {
    ilm.CurFocal = getreal(type);
  }
  else if ((tag == 0x2001) && !strncmp(model, "GXR", 3))
  {
    short ntags, cur_tag;
    fseek(ifp, 20, SEEK_CUR);
    ntags = get2();
    cur_tag = get2();
    while (cur_tag != 0x002c)
    {
      fseek(ifp, 10, SEEK_CUR);
      cur_tag = get2();
    }
    fseek(ifp, 6, SEEK_CUR);
    fseek(ifp, get4(), SEEK_SET);
    for (int i=0; i<4; i++) {
      stread(buffer, 16, ifp);
      if ((buffer[0] == 'S') && (buffer[1] == 'I') && (buffer[2] == 'D'))
			  memcpy(imgdata.shootinginfo.BodySerial, buffer+4, 12);
      else if ((buffer[0] == 'R') && (buffer[1] == 'L'))
			  ilm.LensID = buffer[2] - '0';
      else if ((buffer[0] == 'L') && (buffer[1] == 'I') && (buffer[2] == 'D'))
			  memcpy(imgdata.lens.LensSerial, buffer+4, 12);
    }
  }
}
