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

void LibRaw::processNikonLensData(uchar *LensData, unsigned len)
{

  ushort i;

  if (!(imgdata.lens.nikon.LensType & 0x01))
  {
    ilm.LensFeatures_pre[0] = 'A';
    ilm.LensFeatures_pre[1] = 'F';
  }
  else
  {
    ilm.LensFeatures_pre[0] = 'M';
    ilm.LensFeatures_pre[1] = 'F';
  }

  if (imgdata.lens.nikon.LensType & 0x02)
  {
    if (imgdata.lens.nikon.LensType & 0x04)
      ilm.LensFeatures_suf[0] = 'G';
    else
      ilm.LensFeatures_suf[0] = 'D';
    ilm.LensFeatures_suf[1] = ' ';
  }

  if (imgdata.lens.nikon.LensType & 0x08)
  {
    ilm.LensFeatures_suf[2] = 'V';
    ilm.LensFeatures_suf[3] = 'R';
  }

  if (imgdata.lens.nikon.LensType & 0x10)
  {
    ilm.LensMount = ilm.CameraMount = LIBRAW_MOUNT_Nikon_CX;
    ilm.CameraFormat = ilm.LensFormat = LIBRAW_FORMAT_1INCH;
  }
  else
    ilm.LensMount = ilm.CameraMount = LIBRAW_MOUNT_Nikon_F;

  if (imgdata.lens.nikon.LensType & 0x20)
  {
    strcpy(ilm.Adapter, "FT-1");
    ilm.LensMount = LIBRAW_MOUNT_Nikon_F;
    ilm.CameraMount = LIBRAW_MOUNT_Nikon_CX;
    ilm.CameraFormat = LIBRAW_FORMAT_1INCH;
  }

  imgdata.lens.nikon.LensType = imgdata.lens.nikon.LensType & 0xdf;

  if ((len < 20) || (len == 58))
  {
    switch (len)
    {
    case 9:
      i = 2;
      break;
    case 15:
      i = 7;
      break;
    case 16:
      i = 8;
      break;
    case 58: // "Z 6", "Z 7"
      ilm.CameraMount = LIBRAW_MOUNT_Nikon_Z;
      ilm.CameraFormat = LIBRAW_FORMAT_FF;
      i = 1;
      while ((LensData[i] == LensData[0]) && (i < 17))
        i++;
      if (i == 17)
      {
        ilm.LensMount = LIBRAW_MOUNT_Nikon_Z;
        ilm.LensFormat = LIBRAW_FORMAT_FF;
        ilm.LensID = sget2(LensData + 0x2c);
        if (ilm.MaxAp4CurFocal < 0.7f)
          ilm.MaxAp4CurFocal = libraw_powf64l(
              2.0f, (float)sget2(LensData + 0x32) / 384.0f - 1.0f);
        if (ilm.CurAp < 0.7f)
          ilm.CurAp = libraw_powf64l(
              2.0f, (float)sget2(LensData + 0x34) / 384.0f - 1.0f);
        if (fabsf(ilm.CurFocal) < 1.1f)
          ilm.CurFocal = sget2(LensData + 0x38);
        return;
      }
      i = 9;
      ilm.LensMount = LIBRAW_MOUNT_Nikon_F;
      strcpy(ilm.Adapter, "FTZ");
      break;
    }
    imgdata.lens.nikon.LensIDNumber = LensData[i];
    imgdata.lens.nikon.LensFStops = LensData[i + 1];
    ilm.LensFStops = (float)imgdata.lens.nikon.LensFStops / 12.0f;
    if (fabsf(ilm.MinFocal) < 1.1f)
    {
      if ((imgdata.lens.nikon.LensType ^ (uchar)0x01) || LensData[i + 2])
        ilm.MinFocal =
            5.0f * libraw_powf64l(2.0f, (float)LensData[i + 2] / 24.0f);
      if ((imgdata.lens.nikon.LensType ^ (uchar)0x01) || LensData[i + 3])
        ilm.MaxFocal =
            5.0f * libraw_powf64l(2.0f, (float)LensData[i + 3] / 24.0f);
      if ((imgdata.lens.nikon.LensType ^ (uchar)0x01) || LensData[i + 4])
        ilm.MaxAp4MinFocal =
            libraw_powf64l(2.0f, (float)LensData[i + 4] / 24.0f);
      if ((imgdata.lens.nikon.LensType ^ (uchar)0x01) || LensData[i + 5])
        ilm.MaxAp4MaxFocal =
            libraw_powf64l(2.0f, (float)LensData[i + 5] / 24.0f);
    }
    imgdata.lens.nikon.MCUVersion = LensData[i + 6];
    if (i != 2)
    {
      if ((LensData[i - 1]) && (fabsf(ilm.CurFocal) < 1.1f))
        ilm.CurFocal =
            5.0f * libraw_powf64l(2.0f, (float)LensData[i - 1] / 24.0f);
      if (LensData[i + 7])
        imgdata.lens.nikon.EffectiveMaxAp =
            libraw_powf64l(2.0f, (float)LensData[i + 7] / 24.0f);
    }
    ilm.LensID = (unsigned long long)LensData[i] << 56 |
                 (unsigned long long)LensData[i + 1] << 48 |
                 (unsigned long long)LensData[i + 2] << 40 |
                 (unsigned long long)LensData[i + 3] << 32 |
                 (unsigned long long)LensData[i + 4] << 24 |
                 (unsigned long long)LensData[i + 5] << 16 |
                 (unsigned long long)LensData[i + 6] << 8 |
                 (unsigned long long)imgdata.lens.nikon.LensType;
  }
  else if ((len == 459) || (len == 590))
  {
    memcpy(ilm.Lens, LensData + 390, 64);
  }
  else if (len == 509)
  {
    memcpy(ilm.Lens, LensData + 391, 64);
  }
  else if (len == 879)
  {
    memcpy(ilm.Lens, LensData + 680, 64);
  }

  return;
}

void LibRaw::Nikon_NRW_WBtag(int wb, int skip)
{

#define icWB imgdata.color.WB_Coeffs
  int r, g0, g1, b;
  if (skip)
    get4(); // skip wb "CCT", it is not unique
  r = get4();
  g0 = get4();
  g1 = get4();
  b = get4();
  if (r && g0 && g1 && b)
  {
    icWB[wb][0] = r << 1;
    icWB[wb][1] = g0;
    icWB[wb][2] = b << 1;
    icWB[wb][3] = g1;
  }
  return;
#undef icWB
}
void LibRaw::parseNikonMakernote(int base, int uptag, unsigned dng_writer)
{
#define icWB imgdata.color.WB_Coeffs

  unsigned offset = 0, entries, tag, type, len, save;

  unsigned c, i;
  uchar *LensData_buf;
  uchar ColorBalanceData_buf[324];
  int ColorBalanceData_ready = 0;
  uchar ci, cj, ck;
  unsigned serial = 0;
  unsigned custom_serial = 0;
  unsigned LensData_len = 0;

  short morder, sorder = order;
  char buf[10];
  INT64 fsize = ifp->size();

  fread(buf, 1, 10, ifp);

  if (!strcmp(buf, "Nikon"))
  {
    if (buf[6] != '\2')
      return;
    base = ftell(ifp);
    order = get2();
    if (get2() != 42)
      goto quit;
    offset = get4();
    fseek(ifp, offset - 8, SEEK_CUR);
  }
  else
  {
    fseek(ifp, -10, SEEK_CUR);
  }

  entries = get2();
  if (entries > 1000)
    return;
  morder = order;

  while (entries--)
  {
    order = morder;
    tiff_get(base, &tag, &type, &len, &save);

    INT64 pos = ifp->tell();
    if (len > 8 && pos + len > 2 * fsize)
    {
      fseek(ifp, save, SEEK_SET); // Recover tiff-read position!!
      continue;
    }
    tag |= uptag << 16;
    if (len > 100 * 1024 * 1024)
      goto next; // 100Mb tag? No!

    if (tag == 0x0002)
    {
      if (!iso_speed)
        iso_speed = (get2(), get2());
    }
    else if (tag == 0x000a)
    {
      ilm.LensMount = ilm.CameraMount = LIBRAW_MOUNT_FixedLens;
      ilm.FocalType = LIBRAW_FT_ZOOM_LENS;
    }
    else if ((tag == 0x000c) && (len == 4) && (type == 5))
    {
      cam_mul[0] = getreal(type);
      cam_mul[2] = getreal(type);
      cam_mul[1] = getreal(type);
      cam_mul[3] = getreal(type);
    }
    else if (tag == 0x0011)
    {
      if (is_raw)
      {
        fseek(ifp, get4() + base, SEEK_SET);
        parse_tiff_ifd(base);
      }
    }
    else if (tag == 0x0012)
    {
      ci = fgetc(ifp);
      cj = fgetc(ifp);
      ck = fgetc(ifp);
      if (ck)
        imgdata.makernotes.common.FlashEC = (float)(ci * cj) / (float)ck;
    }
    else if (tag == 0x0014)
    {
      if (type == 7)
      {
        if (len == 2560)
        { // E5400, E8400, E8700, E8800
          fseek(ifp, 0x4e0L, SEEK_CUR);
          order = 0x4d4d;
          cam_mul[0] = get2() / 256.0;
          cam_mul[2] = get2() / 256.0;
          cam_mul[1] = cam_mul[3] = 1.0;
          icWB[LIBRAW_WBI_Auto][0] = get2();
          icWB[LIBRAW_WBI_Auto][2] = get2();
          icWB[LIBRAW_WBI_Daylight][0] = get2();
          icWB[LIBRAW_WBI_Daylight][2] = get2();
          fseek(ifp, 0x18L, SEEK_CUR);
          icWB[LIBRAW_WBI_Tungsten][0] = get2();
          icWB[LIBRAW_WBI_Tungsten][2] = get2();
          fseek(ifp, 0x18L, SEEK_CUR);
          icWB[LIBRAW_WBI_FL_W][0] = get2();
          icWB[LIBRAW_WBI_FL_W][2] = get2();
          icWB[LIBRAW_WBI_FL_N][0] = get2();
          icWB[LIBRAW_WBI_FL_N][2] = get2();
          icWB[LIBRAW_WBI_FL_D][0] = get2();
          icWB[LIBRAW_WBI_FL_D][2] = get2();
          icWB[LIBRAW_WBI_Cloudy][0] = get2();
          icWB[LIBRAW_WBI_Cloudy][2] = get2();
          fseek(ifp, 0x18L, SEEK_CUR);
          icWB[LIBRAW_WBI_Flash][0] = get2();
          icWB[LIBRAW_WBI_Flash][2] = get2();

          icWB[LIBRAW_WBI_Auto][1] = icWB[LIBRAW_WBI_Auto][3] =
              icWB[LIBRAW_WBI_Daylight][1] = icWB[LIBRAW_WBI_Daylight][3] =
                  icWB[LIBRAW_WBI_Tungsten][1] = icWB[LIBRAW_WBI_Tungsten][3] =
                      icWB[LIBRAW_WBI_FL_W][1] = icWB[LIBRAW_WBI_FL_W][3] =
                          icWB[LIBRAW_WBI_FL_N][1] = icWB[LIBRAW_WBI_FL_N][3] =
                              icWB[LIBRAW_WBI_FL_D][1] =
                                  icWB[LIBRAW_WBI_FL_D][3] =
                                      icWB[LIBRAW_WBI_Cloudy][1] =
                                          icWB[LIBRAW_WBI_Cloudy][3] =
                                              icWB[LIBRAW_WBI_Flash][1] =
                                                  icWB[LIBRAW_WBI_Flash][3] =
                                                      256;

          if (strncmp(model, "E8700", 5))
          {
            fseek(ifp, 0x18L, SEEK_CUR);
            icWB[LIBRAW_WBI_Shade][0] = get2();
            icWB[LIBRAW_WBI_Shade][2] = get2();
            icWB[LIBRAW_WBI_Shade][1] = icWB[LIBRAW_WBI_Shade][3] = 256;
          }
        }
        else if (len == 1280)
        { // E5000, E5700
          cam_mul[0] = cam_mul[1] = cam_mul[2] = cam_mul[3] = 1.0;
        }
        else
        {
          fread(buf, 1, 10, ifp);
          if (!strncmp(buf, "NRW ", 4))
          { // P6000, P7000, P7100, B700, P1000
            if (!strcmp(buf + 4, "0100"))
            { // P6000
              fseek(ifp, 0x13deL, SEEK_CUR);
              cam_mul[0] = get4() << 1;
              cam_mul[1] = get4();
              cam_mul[3] = get4();
              cam_mul[2] = get4() << 1;
              Nikon_NRW_WBtag(LIBRAW_WBI_Daylight, 0);
              Nikon_NRW_WBtag(LIBRAW_WBI_Cloudy, 0);
              fseek(ifp, 0x10L, SEEK_CUR);
              Nikon_NRW_WBtag(LIBRAW_WBI_Tungsten, 0);
              Nikon_NRW_WBtag(LIBRAW_WBI_FL_W, 0);
              Nikon_NRW_WBtag(LIBRAW_WBI_Flash, 0);
              fseek(ifp, 0x10L, SEEK_CUR);
              Nikon_NRW_WBtag(LIBRAW_WBI_Custom, 0);
              Nikon_NRW_WBtag(LIBRAW_WBI_Auto, 0);
            }
            else
            { // P7000, P7100, B700, P1000
              fseek(ifp, 0x16L, SEEK_CUR);
              black = get2();
              if (cam_mul[0] < 0.1f)
              {
                fseek(ifp, 0x16L, SEEK_CUR);
                cam_mul[0] = get4() << 1;
                cam_mul[1] = get4();
                cam_mul[3] = get4();
                cam_mul[2] = get4() << 1;
              }
              else
              {
                fseek(ifp, 0x26L, SEEK_CUR);
              }
              if (len != 332)
              { // not A1000
                Nikon_NRW_WBtag(LIBRAW_WBI_Daylight, 1);
                Nikon_NRW_WBtag(LIBRAW_WBI_Cloudy, 1);
                Nikon_NRW_WBtag(LIBRAW_WBI_Shade, 1);
                Nikon_NRW_WBtag(LIBRAW_WBI_Tungsten, 1);
                Nikon_NRW_WBtag(LIBRAW_WBI_FL_W, 1);
                Nikon_NRW_WBtag(LIBRAW_WBI_FL_N, 1);
                Nikon_NRW_WBtag(LIBRAW_WBI_FL_D, 1);
                Nikon_NRW_WBtag(LIBRAW_WBI_HT_Mercury, 1);
                fseek(ifp, 0x14L, SEEK_CUR);
                Nikon_NRW_WBtag(LIBRAW_WBI_Custom, 1);
                Nikon_NRW_WBtag(LIBRAW_WBI_Auto, 1);
              }
              else
              {
                fseek(ifp, 0xc8L, SEEK_CUR);
                Nikon_NRW_WBtag(LIBRAW_WBI_Auto, 1);
              }
            }
          }
        }
      }
    }
    else if (tag == 0x001b)
    {
      imNikon.HighSpeedCropFormat = get2();
      imNikon.SensorHighSpeedCrop.cwidth = get2();
      imNikon.SensorHighSpeedCrop.cheight = get2();
      imNikon.SensorWidth = get2();
      imNikon.SensorHeight = get2();
      imNikon.SensorHighSpeedCrop.cleft = get2();
      imNikon.SensorHighSpeedCrop.ctop = get2();
      switch (imNikon.HighSpeedCropFormat)
      {
      case 0:
      case 1:
      case 2:
      case 4:
        imgdata.sizes.raw_inset_crop.aspect = LIBRAW_IMAGE_ASPECT_3to2;
        break;
      case 11:
        ilm.CameraFormat = LIBRAW_FORMAT_FF;
        imgdata.sizes.raw_inset_crop.aspect = LIBRAW_IMAGE_ASPECT_3to2;
        break;
      case 12:
        ilm.CameraFormat = LIBRAW_FORMAT_APSC;
        imgdata.sizes.raw_inset_crop.aspect = LIBRAW_IMAGE_ASPECT_3to2;
        break;
      case 3:
        imgdata.sizes.raw_inset_crop.aspect = LIBRAW_IMAGE_ASPECT_5to4;
        break;
      case 6:
        imgdata.sizes.raw_inset_crop.aspect = LIBRAW_IMAGE_ASPECT_16to9;
        break;
      case 17:
        imgdata.sizes.raw_inset_crop.aspect = LIBRAW_IMAGE_ASPECT_1to1;
        break;
      default:
        imgdata.sizes.raw_inset_crop.aspect = LIBRAW_IMAGE_ASPECT_OTHER;
        break;
      }
    }
    else if (tag == 0x001d)
    { // serial number
      if (len > 0)
      {
        int model_len = (int)strbuflen(model);
        while ((c = fgetc(ifp)) && (len-- > 0) && (c != EOF))
        {
          if ((!custom_serial) && (!isdigit(c)))
          {
            if (((model_len == 3) && !strcmp(model, "D50")) ||
                ((model_len >= 4) && !isalnum(model[model_len - 4]) &&
                 !strncmp(&model[model_len - 3], "D50", 3)))
            {
              custom_serial = 34;
            }
            else
            {
              custom_serial = 96;
            }
            break;
          }
          serial = serial * 10 + (isdigit(c) ? c - '0' : c % 10);
        }
        if (!imgdata.shootinginfo.BodySerial[0])
          sprintf(imgdata.shootinginfo.BodySerial, "%d", serial);
      }
    }
    else if (tag == 0x001e)
    { // ColorSpace
      imCommon.ColorSpace = get2();
    }
    else if (tag == 0x0025)
    {
      if (!iso_speed || (iso_speed == 65535))
      {
        iso_speed =
            int(100.0 *
                libraw_powf64l(2.0f, double((uchar)fgetc(ifp)) / 12.0 - 5.0));
      }
    }
    else if (tag == 0x003b)
    { // WB for multi-exposure (ME); all 1s for regular exposures
      imNikon.ME_WB[0] = getreal(type);
      imNikon.ME_WB[2] = getreal(type);
      imNikon.ME_WB[1] = getreal(type);
      imNikon.ME_WB[3] = getreal(type);
    }
    else if (tag == 0x003d)
    { // not corrected for file bitcount, to be patched in open_datastream
      FORC4 cblack[c ^ c >> 1] = get2();
      i = cblack[3];
      FORC3 if (i > cblack[c]) i = cblack[c];
      FORC4 cblack[c] -= i;
      black += i;
    }
    else if (tag == 0x0045)
    { /* upper left pixel (x,y), size (width,height) */
      imgdata.sizes.raw_inset_crop.cleft = get2();
      imgdata.sizes.raw_inset_crop.ctop = get2();
      imgdata.sizes.raw_inset_crop.cwidth = get2();
      imgdata.sizes.raw_inset_crop.cheight = get2();
    }
    else if (tag == 0x0082)
    { // lens attachment
      stmread(ilm.Attachment, len, ifp);
    }
    else if (tag == 0x0083)
    { // lens type
      imgdata.lens.nikon.LensType = fgetc(ifp);
    }
    else if (tag == 0x0084)
    { // lens
      ilm.MinFocal = getreal(type);
      ilm.MaxFocal = getreal(type);
      ilm.MaxAp4MinFocal = getreal(type);
      ilm.MaxAp4MaxFocal = getreal(type);
    }
    else if (tag == 0x008b)
    { // lens f-stops
      ci = fgetc(ifp);
      cj = fgetc(ifp);
      ck = fgetc(ifp);
      if (ck)
      {
        imgdata.lens.nikon.LensFStops = ci * cj * (12 / ck);
        ilm.LensFStops = (float)imgdata.lens.nikon.LensFStops / 12.0f;
      }
    }
    else if ((tag == 0x008c) || (tag == 0x0096))
    {
      meta_offset = ftell(ifp);
    }
    else if (tag == 0x0093)
    {
      imNikon.NEFCompression = i = get2();
      if ((i == 7) || (i == 9))
      {
        ilm.LensMount = LIBRAW_MOUNT_FixedLens;
        ilm.CameraMount = LIBRAW_MOUNT_FixedLens;
      }
    }
    else if (tag == 0x0097)
    { // ver97
      FORC4 imNikon.ColorBalanceVersion =
          imNikon.ColorBalanceVersion * 10 + fgetc(ifp) - '0';
      switch (imNikon.ColorBalanceVersion)
      {
      case 100: // NIKON D100
        fseek(ifp, 0x44L, SEEK_CUR);
        FORC4 cam_mul[(c >> 1) | ((c & 1) << 1)] = get2();
        break;
      case 102: // NIKON D2H
        fseek(ifp, 0x6L, SEEK_CUR);
        FORC4 cam_mul[c ^ (c >> 1)] = get2();
        break;
      case 103: // NIKON D70, D70s
        fseek(ifp, 0x10L, SEEK_CUR);
        FORC4 cam_mul[c] = get2();
      }
      if (imNikon.ColorBalanceVersion >= 200)
      {
        /*
        204: NIKON D2X, D2Xs
        205: NIKON D50
        206: NIKON D2Hs
        207: NIKON D200
        208: NIKON D40, D40X, D80
        209: NIKON D3, D3X, D300, D700
        210: NIKON D60
        211: NIKON D90, D5000
        212: NIKON D300S
        213: NIKON D3000
        214: NIKON D3S
        215: NIKON D3100
        216: NIKON D5100, D7000
        217: NIKON D4, D600, D800, D800E, D3200
        -= unknown =-
        218: NIKON D5200, D7100
        219: NIKON D5300
        220: NIKON D610, Df
        221: NIKON D3300
        222: NIKON D4S
        223: NIKON D750, D810
        224: NIKON D3400, D3500, D5500, D5600, D7200
        225: NIKON D5, D500
        226: NIKON D7500
        227: NIKON D850
         */
        if (imNikon.ColorBalanceVersion != 205)
        {
          fseek(ifp, 0x118L, SEEK_CUR);
        }
        ColorBalanceData_ready =
            (fread(ColorBalanceData_buf, 324, 1, ifp) == 1);
      }
      if ((imNikon.ColorBalanceVersion >= 400) &&
          (imNikon.ColorBalanceVersion <= 405))
      { // 1 J1, 1 V1, 1 J2, 1 V2, 1 J3, 1 S1, 1 AW1, 1 S2, 1 J4, 1 V3, 1 J5
        ilm.CameraFormat = LIBRAW_FORMAT_1INCH;
        ilm.CameraMount = LIBRAW_MOUNT_Nikon_CX;
      }
      else if ((imNikon.ColorBalanceVersion >= 500) &&
               (imNikon.ColorBalanceVersion <= 502))
      { // P7700, P7800, P330, P340
        ilm.CameraMount = ilm.LensMount = LIBRAW_MOUNT_FixedLens;
        ilm.FocalType = LIBRAW_FT_ZOOM_LENS;
      }
      else if (imNikon.ColorBalanceVersion == 601)
      { // Coolpix A
        ilm.CameraFormat = ilm.LensFormat = LIBRAW_FORMAT_APSC;
        ilm.CameraMount = ilm.LensMount = LIBRAW_MOUNT_FixedLens;
        ilm.FocalType = LIBRAW_FT_PRIME_LENS;
      }
    }
    else if (tag == 0x0098)
    { // contains lens data
      FORC4 imNikon.LensDataVersion =
          imNikon.LensDataVersion * 10 + fgetc(ifp) - '0';
      switch (imNikon.LensDataVersion)
      {
      case 100:
        LensData_len = 9;
        break;
      case 101:
      case 201: // encrypted, starting from v.201
      case 202:
      case 203:
        LensData_len = 15;
        break;
      case 204:
        LensData_len = 16;
        break;
      case 400:
        LensData_len = 459;
        break;
      case 401:
        LensData_len = 590;
        break;
      case 402:
        LensData_len = 509;
        break;
      case 403:
        LensData_len = 879;
        break;
      case 800:
        LensData_len = 58;
        break;
      }
      if (LensData_len)
      {
        LensData_buf = (uchar *)malloc(LensData_len);
        fread(LensData_buf, LensData_len, 1, ifp);
      }
    }
    else if (tag == 0x00a0)
    {
      stmread(imgdata.shootinginfo.BodySerial, len, ifp);
    }
    else if (tag == 0x00a7)
    { // shutter count
      imNikon.key = fgetc(ifp) ^ fgetc(ifp) ^ fgetc(ifp) ^ fgetc(ifp);
      if (custom_serial)
      {
        ci = xlat[0][custom_serial];
      }
      else
      {
        ci = xlat[0][serial & 0xff];
      }
      cj = xlat[1][imNikon.key];
      ck = 0x60;
      if (((unsigned)(imNikon.ColorBalanceVersion - 200) < 18) &&
          ColorBalanceData_ready)
      {
        for (i = 0; i < 324; i++)
          ColorBalanceData_buf[i] ^= (cj += ci * ck++);
        i = "66666>666;6A;:;555"[imNikon.ColorBalanceVersion - 200] - '0';
        FORC4 cam_mul[c ^ (c >> 1) ^ (i & 1)] =
            sget2(ColorBalanceData_buf + (i & -2) + c * 2);
      }

      if (LensData_len)
      {
        if (imNikon.LensDataVersion > 200)
        {
          for (i = 0; i < LensData_len; i++)
          {
            LensData_buf[i] ^= (cj += ci * ck++);
          }
        }
        processNikonLensData(LensData_buf, LensData_len);
        LensData_len = 0;
        free(LensData_buf);
      }
    }
    else if (tag == 0x00a8)
    { // contains flash data
      FORC4 imNikon.FlashInfoVersion =
          imNikon.FlashInfoVersion * 10 + fgetc(ifp) - '0';
    }
    else if (tag == 0x00b0)
    {
      get4(); // ME (multi-exposure) tag version, 4 symbols
      imNikon.ExposureMode = get4();
      imNikon.nMEshots = get4();
      imNikon.MEgainOn = get4();
    }
    else if (tag == 0x00b9)
    {
      imNikon.AFFineTune = fgetc(ifp);
      imNikon.AFFineTuneIndex = fgetc(ifp);
      imNikon.AFFineTuneAdj = (int8_t)fgetc(ifp);
    }
    else if ((tag == 0x0100) && (type == 7))
    {
      thumb_offset = ftell(ifp);
      thumb_length = len;
    }
    else if (tag == 0x0e01)
    { /* Nikon Software / in-camera edit Note */
      int loopc = 0;
      int WhiteBalanceAdj_active = 0;
      order = 0x4949;
      fseek(ifp, 22, SEEK_CUR);
      for (offset = 22; offset + 22 < len; offset += 22 + i)
      {
        if (loopc++ > 1024)
          throw LIBRAW_EXCEPTION_IO_CORRUPT;
        tag = get4();
        fseek(ifp, 14, SEEK_CUR);
        i = get4() - 4;

        if (tag == 0x76a43204)
        {
          WhiteBalanceAdj_active = fgetc(ifp);
        }
        else if (tag == 0xbf3c6c20)
        {
          if (WhiteBalanceAdj_active)
          {
            union {
              double dbl;
              unsigned long long lng;
            } un;
            un.dbl = getreal(12);
            if ((un.lng != 0x3FF0000000000000ULL) &&
                (un.lng != 0x000000000000F03FULL))
            {
              cam_mul[0] = un.dbl;
              cam_mul[2] = getreal(12);
              cam_mul[1] = cam_mul[3] = 1.0;
              i -= 16;
            }
            else
              i -= 8;
          }
          fseek(ifp, i, SEEK_CUR);
        }
        else if (tag == 0x76a43207)
        {
          flip = get2();
        }
        else
        {
          fseek(ifp, i, SEEK_CUR);
        }
      }
    }
    else if (tag == 0x0e22)
    {
      FORC4 imNikon.NEFBitDepth[c] = get2();
    }
  next:
    fseek(ifp, save, SEEK_SET);
  }
quit:
  order = sorder;
#undef icWB
}
