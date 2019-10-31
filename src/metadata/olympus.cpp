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

void LibRaw::setOlympusBodyFeatures(unsigned long long id)
{
  ilm.CamID = id;

  if ((id == 0x4434303430ULL) || // E-1
      (id == 0x4434303431ULL) || // E-300
      ((id & 0x00ffff0000ULL) == 0x0030300000ULL))
  {
    ilm.CameraFormat = LIBRAW_FORMAT_FT;

    if ((id == 0x4434303430ULL) || // E-1
        (id == 0x4434303431ULL) || // E-330
        ((id >= 0x5330303033ULL) && (id <= 0x5330303138ULL)) || // E-330 to E-520
        (id == 0x5330303233ULL) || // E-620
        (id == 0x5330303239ULL) || // E-450
        (id == 0x5330303330ULL) || // E-600
        (id == 0x5330303333ULL))   // E-5
    {
      ilm.CameraMount = LIBRAW_MOUNT_FT;
    }
    else
    {
      ilm.CameraMount = LIBRAW_MOUNT_mFT;
    }
  }
  else
  {
    ilm.LensMount = ilm.CameraMount = LIBRAW_MOUNT_FixedLens;
  }
  return;
}

void LibRaw::getOlympus_CameraType2()
{

  if (OlyID != 0x0ULL)
    return;

  int i = 0;

  fread(imOly.CameraType2, 6, 1, ifp);
  imOly.CameraType2[5] = 0;
  while ((i < 6) && imOly.CameraType2[i])
  {
    OlyID = OlyID << 8 | imOly.CameraType2[i];
    if (i < 5 && isspace(imOly.CameraType2[i + 1]))
      imOly.CameraType2[i + 1] = '\0';
    i++;
  }
  unique_id = OlyID;
  setOlympusBodyFeatures(OlyID);
  return;
}

void LibRaw::getOlympus_SensorTemperature(unsigned len)
{
  if (OlyID != 0x0ULL)
  {
    short temp = get2();
    if ((OlyID == 0x4434303430ULL) || // E-1
        (OlyID == 0x5330303336ULL) || // E-M5
        (len != 1))
      imgdata.makernotes.common.SensorTemperature = (float)temp;
    else if ((temp != -32768) && (temp != 0))
    {
      if (temp > 199)
        imgdata.makernotes.common.SensorTemperature = 86.474958f - 0.120228f * (float)temp;
      else
        imgdata.makernotes.common.SensorTemperature = (float)temp;
    }
  }
  return;
}

void LibRaw::parseOlympus_Equipment(unsigned tag, unsigned type, unsigned len,
                                    unsigned dng_writer)
{
  // uptag 2010

  int i;

  switch (tag)
  {
  case 0x0100:
    getOlympus_CameraType2();
    break;
  case 0x0101:
    if ((!imgdata.shootinginfo.BodySerial[0]) && (dng_writer == nonDNG))
      stmread(imgdata.shootinginfo.BodySerial, len, ifp);
    break;
  case 0x0102:
    stmread(imgdata.shootinginfo.InternalBodySerial, len, ifp);
    break;
  case 0x0201:
  {
    unsigned char bits[4];
    fread(bits, 1, 4, ifp);
    ilm.LensID = (unsigned long long)bits[0] << 16 |
                 (unsigned long long)bits[2] << 8 | (unsigned long long)bits[3];
    ilm.LensMount = LIBRAW_MOUNT_FT;
    ilm.LensFormat = LIBRAW_FORMAT_FT;
    if (((ilm.LensID < 0x20000) || (ilm.LensID > 0x4ffff)) &&
        (ilm.LensID & 0x10))
      ilm.LensMount = LIBRAW_MOUNT_mFT;
  }
  break;
  case 0x0202:
    if ((!imgdata.lens.LensSerial[0]))
      stmread(imgdata.lens.LensSerial, len, ifp);
    break;
  case 0x0203:
    stmread(ilm.Lens, len, ifp);
    break;
  case 0x0205:
    ilm.MaxAp4MinFocal = libraw_powf64l(sqrt(2.0f), get2() / 256.0f);
    break;
  case 0x0206:
    ilm.MaxAp4MaxFocal = libraw_powf64l(sqrt(2.0f), get2() / 256.0f);
    break;
  case 0x0207:
    ilm.MinFocal = (float)get2();
    break;
  case 0x0208:
    ilm.MaxFocal = (float)get2();
    if (ilm.MaxFocal > 1000.0f)
      ilm.MaxFocal = ilm.MinFocal;
    break;
  case 0x020a:
    ilm.MaxAp4CurFocal = libraw_powf64l(sqrt(2.0f), get2() / 256.0f);
    break;
  case 0x0301:
    ilm.TeleconverterID = fgetc(ifp) << 8;
    fgetc(ifp);
    ilm.TeleconverterID = ilm.TeleconverterID | fgetc(ifp);
    break;
  case 0x0303:
    stmread(ilm.Teleconverter, len, ifp);
    break;
  case 0x0403:
    stmread(ilm.Attachment, len, ifp);
    break;
  }

  return;
}
void LibRaw::parseOlympus_CameraSettings(int base, unsigned tag, unsigned type,
                                         unsigned len, unsigned dng_writer)
{
  // uptag 0x2020

  int c;
  uchar uc;
  switch (tag)
  {
  case 0x0101:
    if (dng_writer == nonDNG)
    {
      thumb_offset = get4() + base;
    }
    break;
  case 0x0102:
    if (dng_writer == nonDNG)
    {
      thumb_length = get4();
    }
    break;
  case 0x0200:
    imgdata.shootinginfo.ExposureMode = get2();
    break;
  case 0x0202:
    imgdata.shootinginfo.MeteringMode = get2();
    break;
  case 0x0301:
    imgdata.shootinginfo.FocusMode = imOly.FocusMode[0] = get2();
    if (len == 2)
    {
      imOly.FocusMode[1] = get2();
    }
    break;
  case 0x0304:
    for (c = 0; c < 64; c++)
    {
      imOly.AFAreas[c] = get4();
    }
    break;
  case 0x0305:
    for (c = 0; c < 5; c++)
    {
      imOly.AFPointSelected[c] = getreal(type);
    }
    break;
  case 0x0306:
    fread(&uc, 1, 1, ifp);
    imOly.AFFineTune = uc;
    break;
  case 0x0307:
    FORC3 imOly.AFFineTuneAdj[c] = get2();
    break;
  case 0x0401:
    imgdata.makernotes.common.FlashEC = getreal(type);
    break;
  case 0x0507:
    imOly.ColorSpace = get2();
    break;
  case 0x0600:
    imgdata.shootinginfo.DriveMode = imOly.DriveMode[0] = get2();
    for (c = 1; c < len && c < 5; c++)
    {
      imOly.DriveMode[c] = get2();
    }
    break;
  case 0x0604:
    imgdata.shootinginfo.ImageStabilization = get4();
    break;
  }

  return;
}

void LibRaw::parseOlympus_ImageProcessing(unsigned tag, unsigned type,
                                          unsigned len, unsigned dng_writer)
{
  // uptag 0x2040

  int i, c, wb[4], nWB, tWB, wbG;
  ushort CT;
  short sorder;

  if ((tag == 0x0100) && (dng_writer == nonDNG))
  {
    cam_mul[0] = get2() / 256.0;
    cam_mul[2] = get2() / 256.0;
  }
  else if ((tag == 0x0101) && (len == 2) &&
           (!strncasecmp(model, "E-410", 5) || !strncasecmp(model, "E-510", 5)))
  {
    for (i = 0; i < 64; i++)
    {
      imgdata.color.WBCT_Coeffs[i][2] = imgdata.color.WBCT_Coeffs[i][4] =
          imgdata.color.WB_Coeffs[i][1] = imgdata.color.WB_Coeffs[i][3] = 0x100;
    }
    for (i = 64; i < 256; i++)
    {
      imgdata.color.WB_Coeffs[i][1] = imgdata.color.WB_Coeffs[i][3] = 0x100;
    }
  }
  else if ((tag > 0x0101) && (tag <= 0x0111))
  {
    nWB = tag - 0x0101;
    tWB = Oly_wb_list2[nWB << 1];
    CT = Oly_wb_list2[(nWB << 1) | 1];
    wb[0] = get2();
    wb[2] = get2();
    if (tWB != 0x100)
    {
      imgdata.color.WB_Coeffs[tWB][0] = wb[0];
      imgdata.color.WB_Coeffs[tWB][2] = wb[2];
    }
    if (CT)
    {
      imgdata.color.WBCT_Coeffs[nWB - 1][0] = CT;
      imgdata.color.WBCT_Coeffs[nWB - 1][1] = wb[0];
      imgdata.color.WBCT_Coeffs[nWB - 1][3] = wb[2];
    }
    if (len == 4)
    {
      wb[1] = get2();
      wb[3] = get2();
      if (tWB != 0x100)
      {
        imgdata.color.WB_Coeffs[tWB][1] = wb[1];
        imgdata.color.WB_Coeffs[tWB][3] = wb[3];
      }
      if (CT)
      {
        imgdata.color.WBCT_Coeffs[nWB - 1][2] = wb[1];
        imgdata.color.WBCT_Coeffs[nWB - 1][4] = wb[3];
      }
    }
  }
  else if ((tag >= 0x0112) && (tag <= 0x011e))
  {
    nWB = tag - 0x0112;
    wbG = get2();
    tWB = Oly_wb_list2[nWB << 1];
    if (nWB)
      imgdata.color.WBCT_Coeffs[nWB - 1][2] =
          imgdata.color.WBCT_Coeffs[nWB - 1][4] = wbG;
    if (tWB != 0x100)
      imgdata.color.WB_Coeffs[tWB][1] = imgdata.color.WB_Coeffs[tWB][3] = wbG;
  }
  else if (tag == 0x011f)
  {
    wbG = get2();
    if (imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][0])
      imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][1] =
          imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][3] = wbG;
    FORC4 if (imgdata.color.WB_Coeffs[LIBRAW_WBI_Custom1 + c][0])
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Custom1 + c][1] =
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Custom1 + c][3] = wbG;
  }
  else if (tag == 0x0121)
  {
    imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][0] = get2();
    imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][2] = get2();
    if (len == 4)
    {
      imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][1] = get2();
      imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][3] = get2();
    }
  }
  else if ((tag == 0x0200) && (dng_writer == nonDNG) &&
           strcmp(software, "v757-71"))
  {
    for (i = 0; i < 3; i++)
    {
      if (!imOly.ColorSpace)
      {
        FORC3 cmatrix[i][c] = ((short)get2()) / 256.0;
      }
      else
      {
        FORC3 imgdata.color.ccm[i][c] = ((short)get2()) / 256.0;
      }
    }
  }
  else if ((tag == 0x0600) && (dng_writer == nonDNG))
  {
    FORC4 cblack[c ^ c >> 1] = get2();
  }
  else if ((tag == 0x0612) && (dng_writer == nonDNG))
  {
    imgdata.sizes.raw_inset_crop.cleft = get2();
  }
  else if ((tag == 0x0613) && (dng_writer == nonDNG))
  {
    imgdata.sizes.raw_inset_crop.ctop = get2();
  }
  else if ((tag == 0x0614) && (dng_writer == nonDNG))
  {
    imgdata.sizes.raw_inset_crop.cwidth = get2();
  }
  else if ((tag == 0x0615) && (dng_writer == nonDNG))
  {
    imgdata.sizes.raw_inset_crop.cheight = get2();
  }
  else if ((tag == 0x0805) && (len == 2))
  {
    imOly.SensorCalibration[0] = getreal(type);
    imOly.SensorCalibration[1] = getreal(type);
    if (dng_writer == nonDNG)
      FORC4 imgdata.color.linear_max[c] = imOly.SensorCalibration[0];
  }
  else if (tag == 0x1112)
  {
    sorder = order;
    order = 0x4d4d;
    c = get2();
    order = sorder;
    switch (c)
    {
    case 0x21:
      imgdata.sizes.raw_inset_crop.aspect = LIBRAW_IMAGE_ASPECT_3to2;
      break;
    case 0x31:
      imgdata.sizes.raw_inset_crop.aspect = LIBRAW_IMAGE_ASPECT_16to9;
      break;
    case 0x41:
      imgdata.sizes.raw_inset_crop.aspect = LIBRAW_IMAGE_ASPECT_1to1;
      break;
    case 0x91:
      imgdata.sizes.raw_inset_crop.aspect = LIBRAW_IMAGE_ASPECT_4to3;
      break;
    }
  }
  else if (tag == 0x1113)
  {
    imgdata.sizes.raw_inset_crop.cleft = get2();
    imgdata.sizes.raw_inset_crop.ctop = get2();
    imgdata.sizes.raw_inset_crop.cwidth =
        get2() - imgdata.sizes.raw_inset_crop.cleft;
    imgdata.sizes.raw_inset_crop.cheight =
        get2() - imgdata.sizes.raw_inset_crop.ctop;
  }
  else if (tag == 0x1306)
  {
    c = get2();
    if ((c != 0) && (c != 100))
    {
      if (c < 61)
        imgdata.makernotes.common.CameraTemperature = (float)c;
      else
        imgdata.makernotes.common.CameraTemperature = (float)(c - 32) / 1.8f;
      if ((imgdata.makernotes.common.exifAmbientTemperature > -273.15f) &&
          ((OlyID == 0x4434353933ULL) || // TG-5
           (OlyID == 0x4434363033ULL))   // TG-6
      )
        imgdata.makernotes.common.CameraTemperature += imgdata.makernotes.common.exifAmbientTemperature;
    }
  }

  return;
}

void LibRaw::parseOlympus_RawInfo(unsigned tag, unsigned type, unsigned len,
                                  unsigned dng_writer)
{
  // uptag 0x3000

  int wb_ind, c, i;

  if ((tag == 0x0110) && strcmp(software, "v757-71"))
  {
    imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][0] = get2();
    imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][2] = get2();
    if (len == 2)
    {
      for (i = 0; i < 256; i++)
        imgdata.color.WB_Coeffs[i][1] = imgdata.color.WB_Coeffs[i][3] = 0x100;
    }
  }
  else if ((((tag >= 0x0120) && (tag <= 0x0124)) ||
            ((tag >= 0x0130) && (tag <= 0x0133))) &&
           strcmp(software, "v757-71"))
  {
    if (tag <= 0x0124)
      wb_ind = tag - 0x0120;
    else
      wb_ind = tag - 0x0130 + 5;

    imgdata.color.WB_Coeffs[Oly_wb_list1[wb_ind]][0] = get2();
    imgdata.color.WB_Coeffs[Oly_wb_list1[wb_ind]][2] = get2();
  }
  else if ((tag == 0x0200) && (dng_writer == nonDNG))
  {
    for (i = 0; i < 3; i++)
    {
      if (!imOly.ColorSpace)
      {
        FORC3 cmatrix[i][c] = ((short)get2()) / 256.0;
      }
      else
      {
        FORC3 imgdata.color.ccm[i][c] = ((short)get2()) / 256.0;
      }
    }
  }
  else if ((tag == 0x0600) && (dng_writer == nonDNG))
  {
    FORC4 cblack[c ^ c >> 1] = get2();
  }
  else if ((tag == 0x0612) && (dng_writer == nonDNG))
  {
    imgdata.sizes.raw_inset_crop.cleft = get2();
  }
  else if ((tag == 0x0613) && (dng_writer == nonDNG))
  {
    imgdata.sizes.raw_inset_crop.ctop = get2();
  }
  else if ((tag == 0x0614) && (dng_writer == nonDNG))
  {
    imgdata.sizes.raw_inset_crop.cwidth = get2();
  }
  else if ((tag == 0x0615) && (dng_writer == nonDNG))
  {
    imgdata.sizes.raw_inset_crop.cheight = get2();
  }
  return;
}
