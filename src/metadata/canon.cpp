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
#include "../../internal/dcraw_defs.h"
#include "../../internal/libraw_cameraids.h"

float LibRaw::_CanonConvertAperture(ushort in)
{
  if ((in == (ushort)0xffe0) || (in == (ushort)0x7fff))
    return 0.0f;
  return LibRaw::libraw_powf64l(2.0, in / 64.0);
}

static float _CanonConvertEV(short in)
{
  short EV, Sign, Frac;
  float Frac_f;
  EV = in;
  if (EV < 0)
  {
    EV = -EV;
    Sign = -1;
  }
  else
  {
    Sign = 1;
  }
  Frac = EV & 0x1f;
  EV -= Frac; // remove fraction

  if (Frac == 0x0c)
  { // convert 1/3 and 2/3 codes
    Frac_f = 32.0f / 3.0f;
  }
  else if (Frac == 0x14)
  {
    Frac_f = 64.0f / 3.0f;
  }
  else
    Frac_f = (float)Frac;

  return ((float)Sign * ((float)EV + Frac_f)) / 32.0f;
}

void LibRaw::setCanonBodyFeatures(unsigned long long id)
{

  ilm.CamID = id;
  if ((id == CanonID_EOS_1D)           ||
      (id == CanonID_EOS_1D_Mark_II)   ||
      (id == CanonID_EOS_1D_Mark_II_N) ||
      (id == CanonID_EOS_1D_Mark_III)  ||
      (id == CanonID_EOS_1D_Mark_IV))
  {
    ilm.CameraFormat = LIBRAW_FORMAT_APSH;
    ilm.CameraMount = LIBRAW_MOUNT_Canon_EF;
  }
  else if ((id == CanonID_EOS_1DS)           ||
           (id == CanonID_EOS_1Ds_Mark_II)   ||
           (id == CanonID_EOS_1Ds_Mark_III)  ||
           (id == CanonID_EOS_1D_X)          ||
           (id == CanonID_EOS_1D_X_Mark_II)  ||
           (id == CanonID_EOS_1D_X_Mark_III) ||
           (id == CanonID_EOS_1D_C)          ||
           (id == CanonID_EOS_5D)            ||
           (id == CanonID_EOS_5D_Mark_II)    ||
           (id == CanonID_EOS_5D_Mark_III)   ||
           (id == CanonID_EOS_5D_Mark_IV)    ||
           (id == CanonID_EOS_5DS)           ||
           (id == CanonID_EOS_5DS_R)         ||
           (id == CanonID_EOS_6D)            ||
           (id == CanonID_EOS_6D_Mark_II))
  {
    ilm.CameraFormat = LIBRAW_FORMAT_FF;
    ilm.CameraMount = LIBRAW_MOUNT_Canon_EF;
  }
  else if ((id == CanonID_EOS_M)         ||
           (id == CanonID_EOS_M2)        ||
           (id == CanonID_EOS_M3)        ||
           (id == CanonID_EOS_M10)       ||
           (id == CanonID_EOS_M5)        ||
           (id == CanonID_EOS_M50)       ||
           (id == CanonID_EOS_M6)        ||
           (id == CanonID_EOS_M100)      ||
           (id == CanonID_EOS_M6_Mark_II))
  {
    ilm.CameraFormat = LIBRAW_FORMAT_APSC;
    ilm.CameraMount = LIBRAW_MOUNT_Canon_EF_M;
  }
  else if ((id == CanonID_EOS_R) ||
           (id == CanonID_EOS_RP))
  {
    ilm.CameraFormat = LIBRAW_FORMAT_FF;
    ilm.CameraMount = LIBRAW_MOUNT_Canon_RF;
  }
  else if ((id == CanonID_EOS_D30) ||
           (id == CanonID_EOS_D60) ||
           (id > 0x80000000ULL))
  {
    ilm.CameraFormat = LIBRAW_FORMAT_APSC;
    ilm.CameraMount = LIBRAW_MOUNT_Canon_EF;
  }
}

void LibRaw::processCanonCameraInfo(unsigned long long id, uchar *CameraInfo,
                                    unsigned maxlen, unsigned type, unsigned dng_writer)
{
  ushort iCanonLensID = 0, iCanonMaxFocal = 0, iCanonMinFocal = 0,
         iCanonLens = 0, iCanonCurFocal = 0, iCanonFocalType = 0;

  if (maxlen < 16)
    return; // too short

 if (tagtypeIs(LIBRAW_EXIFTAG_TYPE_UNDEFINED) &&
     (sget2(CameraInfo) == 0xaaaa) && (dng_writer == nonDNG)) { // CameraOrientation
    int c, i;
    for (c = i = 2; (ushort)c != 0xbbbb && i < (int)maxlen; i++)
      c = c << 8 | CameraInfo[i];
    while (i < int(maxlen - 5))
      if ((sget4(CameraInfo+i) == 257) && ((c = CameraInfo[i+8]) < 3)) {
        imCanon.MakernotesFlip = "065"[c] - '0';
        break;
      } else i+=4;
  }

  CameraInfo[0] = 0;
  CameraInfo[1] = 0;
  if (tagtypeIs(LIBRAW_EXIFTAG_TYPE_LONG)) {
    if ((maxlen == 94)  || (maxlen == 138) || (maxlen == 148) ||
        (maxlen == 156) || (maxlen == 162) || (maxlen == 167) ||
        (maxlen == 171) || (maxlen == 264) || (maxlen > 400))
      imCommon.CameraTemperature = sget4(CameraInfo + ((maxlen - 3) << 2));
    else if (maxlen == 72)
      imCommon.CameraTemperature = sget4(CameraInfo + ((maxlen - 1) << 2));
    else if ((maxlen == 85) || (maxlen == 93))
      imCommon.CameraTemperature = sget4(CameraInfo + ((maxlen - 2) << 2));
    else if ((maxlen == 96) || (maxlen == 104))
      imCommon.CameraTemperature = sget4(CameraInfo + ((maxlen - 4) << 2));
  }

  switch (id)
  {
  case CanonID_EOS_1D:
  case CanonID_EOS_1DS:
    iCanonCurFocal = 10;
    iCanonLensID = 13;
    iCanonMinFocal = 14;
    iCanonMaxFocal = 16;
    if (!ilm.CurFocal)
      ilm.CurFocal = sget2(CameraInfo + iCanonCurFocal);
    if (!ilm.MinFocal)
      ilm.MinFocal = sget2(CameraInfo + iCanonMinFocal);
    if (!ilm.MaxFocal)
      ilm.MaxFocal = sget2(CameraInfo + iCanonMaxFocal);
    imCommon.CameraTemperature = 0.0f;
    break;
  case CanonID_EOS_1D_Mark_II:
  case CanonID_EOS_1Ds_Mark_II:
    iCanonCurFocal = 9;
    iCanonLensID = 12;
    iCanonMinFocal = 17;
    iCanonMaxFocal = 19;
    iCanonFocalType = 45;
    break;
  case CanonID_EOS_1D_Mark_II_N:
    iCanonCurFocal = 9;
    iCanonLensID = 12;
    iCanonMinFocal = 17;
    iCanonMaxFocal = 19;
    break;
  case CanonID_EOS_1D_Mark_III:
  case CanonID_EOS_1Ds_Mark_III:
    iCanonCurFocal = 29;
    iCanonLensID = 273;
    iCanonMinFocal = 275;
    iCanonMaxFocal = 277;
    break;
  case CanonID_EOS_1D_Mark_IV:
    iCanonCurFocal = 30;
    iCanonLensID = 335;
    iCanonMinFocal = 337;
    iCanonMaxFocal = 339;
    break;
  case CanonID_EOS_1D_X:
    iCanonCurFocal = 35;
    iCanonLensID = 423;
    iCanonMinFocal = 425;
    iCanonMaxFocal = 427;
    break;
  case CanonID_EOS_5D:
    iCanonCurFocal = 40;
    if (!sget2Rev(CameraInfo + 12))
      iCanonLensID = 151;
    else
      iCanonLensID = 12;
    iCanonMinFocal = 147;
    iCanonMaxFocal = 149;
    break;
  case CanonID_EOS_5D_Mark_II:
    iCanonCurFocal = 30;
    iCanonLensID = 230;
    iCanonMinFocal = 232;
    iCanonMaxFocal = 234;
    break;
  case CanonID_EOS_5D_Mark_III:
    iCanonCurFocal = 35;
    iCanonLensID = 339;
    iCanonMinFocal = 341;
    iCanonMaxFocal = 343;
    break;
  case CanonID_EOS_6D:
    iCanonCurFocal = 35;
    iCanonLensID = 353;
    iCanonMinFocal = 355;
    iCanonMaxFocal = 357;
    break;
  case CanonID_EOS_7D:
    iCanonCurFocal = 30;
    iCanonLensID = 274;
    iCanonMinFocal = 276;
    iCanonMaxFocal = 278;
    break;
  case CanonID_EOS_40D:
    iCanonCurFocal = 29;
    iCanonLensID = 214;
    iCanonMinFocal = 216;
    iCanonMaxFocal = 218;
    iCanonLens = 2347;
    break;
  case CanonID_EOS_50D:
    iCanonCurFocal = 30;
    iCanonLensID = 234;
    iCanonMinFocal = 236;
    iCanonMaxFocal = 238;
    break;
  case CanonID_EOS_60D:
    iCanonCurFocal = 30;
    iCanonLensID = 232;
    iCanonMinFocal = 234;
    iCanonMaxFocal = 236;
    break;
  case CanonID_EOS_70D:
    iCanonCurFocal = 35;
    iCanonLensID = 358;
    iCanonMinFocal = 360;
    iCanonMaxFocal = 362;
    break;
  case CanonID_EOS_450D:
    iCanonCurFocal = 29;
    iCanonLensID = 222;
    iCanonLens = 2355;
    break;
  case CanonID_EOS_500D:
    iCanonCurFocal = 30;
    iCanonLensID = 246;
    iCanonMinFocal = 248;
    iCanonMaxFocal = 250;
    break;
  case CanonID_EOS_550D:
    iCanonCurFocal = 30;
    iCanonLensID = 255;
    iCanonMinFocal = 257;
    iCanonMaxFocal = 259;
    break;
  case CanonID_EOS_600D:
  case CanonID_EOS_1100D:
    iCanonCurFocal = 30;
    iCanonLensID = 234;
    iCanonMinFocal = 236;
    iCanonMaxFocal = 238;
    break;
  case CanonID_EOS_650D:
  case CanonID_EOS_700D:
    iCanonCurFocal = 35;
    iCanonLensID = 295;
    iCanonMinFocal = 297;
    iCanonMaxFocal = 299;
    break;
  case CanonID_EOS_1000D:
    iCanonCurFocal = 29;
    iCanonLensID = 226;
    iCanonMinFocal = 228;
    iCanonMaxFocal = 230;
    iCanonLens = 2359;
    break;
  }
  if (iCanonFocalType)
  {
    if (iCanonFocalType >= maxlen)
      return; // broken;
    ilm.FocalType = CameraInfo[iCanonFocalType];
    if (!ilm.FocalType) // zero means 'prime' here, replacing with standard '1'
      ilm.FocalType = LIBRAW_FT_PRIME_LENS;
  }
  if (!ilm.CurFocal)
  {
    if (iCanonCurFocal >= maxlen)
      return; // broken;
    ilm.CurFocal = sget2Rev(CameraInfo + iCanonCurFocal);
  }
  if (!ilm.LensID)
  {
    if (iCanonLensID >= maxlen)
      return; // broken;
    ilm.LensID = sget2Rev(CameraInfo + iCanonLensID);
  }
  if (!ilm.MinFocal)
  {
    if (iCanonMinFocal >= maxlen)
      return; // broken;
    ilm.MinFocal = sget2Rev(CameraInfo + iCanonMinFocal);
  }
  if (!ilm.MaxFocal)
  {
    if (iCanonMaxFocal >= maxlen)
      return; // broken;
    ilm.MaxFocal = sget2Rev(CameraInfo + iCanonMaxFocal);
  }
  if (!ilm.Lens[0] && iCanonLens)
  {
    if (iCanonLens + 64 >= (int)maxlen) // broken;
      return;

    char *pl = (char *)CameraInfo + iCanonLens;
    if (!strncmp(pl, "EF-S", 4))
    {
      memcpy(ilm.Lens, pl, 4);
      ilm.Lens[4] = ' ';
      memcpy(ilm.LensFeatures_pre, pl, 4);
      ilm.LensMount = LIBRAW_MOUNT_Canon_EF_S;
      ilm.LensFormat = LIBRAW_FORMAT_APSC;
      memcpy(ilm.Lens + 5, pl + 4, 60);
    }
    else if (!strncmp(pl, "EF-M", 4))
    {
      memcpy(ilm.Lens, pl, 4);
      ilm.Lens[4] = ' ';
      memcpy(ilm.LensFeatures_pre, pl, 4);
      ilm.LensMount = LIBRAW_MOUNT_Canon_EF_M;
      ilm.LensFormat = LIBRAW_FORMAT_APSC;
      memcpy(ilm.Lens + 5, pl + 4, 60);
    }
    else if (!strncmp(pl, "EF", 2))
    {
      memcpy(ilm.Lens, pl, 2);
      ilm.Lens[2] = ' ';
      memcpy(ilm.LensFeatures_pre, pl, 2);
      ilm.LensMount = LIBRAW_MOUNT_Canon_EF;
      ilm.LensFormat = LIBRAW_FORMAT_FF;
      memcpy(ilm.Lens + 3, pl + 2, 62);
    }
    else if (!strncmp(ilm.Lens, "CN-E", 4))
    {
      memmove(ilm.Lens + 5, ilm.Lens + 4, 60);
      ilm.Lens[4] = ' ';
      memcpy(ilm.LensFeatures_pre, ilm.Lens, 4);
      ilm.LensMount = LIBRAW_MOUNT_Canon_EF;
      ilm.LensFormat = LIBRAW_FORMAT_FF;
    }
    else if (!strncmp(pl, "TS-E", 4))
    {
      memcpy(ilm.Lens, pl, 4);
      ilm.Lens[4] = ' ';
      memcpy(ilm.LensFeatures_pre, pl, 4);
      ilm.LensMount = LIBRAW_MOUNT_Canon_EF;
      ilm.LensFormat = LIBRAW_FORMAT_FF;
      memcpy(ilm.Lens + 5, pl + 4, 60);
    }
    else if (!strncmp(pl, "MP-E", 4))
    {
      memcpy(ilm.Lens, pl, 4);
      ilm.Lens[4] = ' ';
      memcpy(ilm.LensFeatures_pre, pl, 4);
      ilm.LensMount = LIBRAW_MOUNT_Canon_EF;
      ilm.LensFormat = LIBRAW_FORMAT_FF;
      memcpy(ilm.Lens + 5, pl + 4, 60);
    }
    else // non-Canon lens
      memcpy(ilm.Lens, pl, 64);
  }
  return;
}

void LibRaw::Canon_CameraSettings(unsigned len)
{
  fseek(ifp, 10, SEEK_CUR);
  imgdata.shootinginfo.DriveMode = get2(); // 5
  get2();
  imgdata.shootinginfo.FocusMode = get2(); // 7
  imCanon.RecordMode = (get2(), get2()); // 9, format
  fseek(ifp, 14, SEEK_CUR);
  imgdata.shootinginfo.MeteringMode = get2(); // 17
  get2();
  imgdata.shootinginfo.AFPoint = get2();      // 19
  imgdata.shootinginfo.ExposureMode = get2(); // 20
  get2();
  ilm.LensID = get2();          // 22
  ilm.MaxFocal = get2();        // 23
  ilm.MinFocal = get2();        // 24
  ilm.FocalUnits = get2(); // 25
  if (ilm.FocalUnits > 1)
  {
    ilm.MaxFocal /= (float)ilm.FocalUnits;
    ilm.MinFocal /= (float)ilm.FocalUnits;
  }
  ilm.MaxAp = _CanonConvertAperture(get2()); // 26
  ilm.MinAp = _CanonConvertAperture(get2()); // 27
  if (len >= 36)
  {
    fseek(ifp, 12, SEEK_CUR);
    imgdata.shootinginfo.ImageStabilization = get2(); // 34
  }
  else
    return;
  if (len >= 48)
  {
    fseek(ifp, 22, SEEK_CUR);
    imCanon.SRAWQuality = get2(); // 46
  }
}

void LibRaw::Canon_WBpresets(int skip1, int skip2)
{
  int c;
  FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Daylight][RGGB_2_RGBG(c)] = get2();

  if (skip1)
    fseek(ifp, skip1, SEEK_CUR);
  FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Shade][RGGB_2_RGBG(c)] = get2();

  if (skip1)
    fseek(ifp, skip1, SEEK_CUR);
  FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Cloudy][RGGB_2_RGBG(c)] = get2();

  if (skip1)
    fseek(ifp, skip1, SEEK_CUR);
  FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Tungsten][RGGB_2_RGBG(c)] = get2();

  if (skip1)
    fseek(ifp, skip1, SEEK_CUR);
  FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_W][RGGB_2_RGBG(c)] = get2();

  if (skip2)
    fseek(ifp, skip2, SEEK_CUR);
  FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][RGGB_2_RGBG(c)] = get2();

  return;
}

void LibRaw::Canon_WBCTpresets(short WBCTversion)
{

  int i;
  float norm;

  if (WBCTversion == 0)
  { // tint, as shot R, as shot B, CСT
    for (i = 0; i < 15; i++)
    {
      icWBCCTC[i][2] = icWBCCTC[i][4] = 1.0f;
      fseek(ifp, 2, SEEK_CUR);
      icWBCCTC[i][1] = 1024.0f / fMAX(get2(), 1.f);
      icWBCCTC[i][3] = 1024.0f / fMAX(get2(), 1.f);
      icWBCCTC[i][0] = get2();
    }
  }
  else if (WBCTversion == 1)
  { // as shot R, as shot B, tint, CСT
    for (i = 0; i < 15; i++)
    {
      icWBCCTC[i][2] = icWBCCTC[i][4] = 1.0f;
      icWBCCTC[i][1] = 1024.0f / fMAX(get2(), 1.f);
      icWBCCTC[i][3] = 1024.0f / fMAX(get2(), 1.f);
      fseek(ifp, 2, SEEK_CUR);
      icWBCCTC[i][0] = get2();
    }
  }
  else if (WBCTversion == 2)
  { // tint, offset, as shot R, as shot B, CСT
    if ((unique_id == CanonID_EOS_M3)  ||
        (unique_id == CanonID_EOS_M10) ||
        (imCanon.ColorDataSubVer == 0xfffc))
    {
      for (i = 0; i < 15; i++)
      {
        fseek(ifp, 4, SEEK_CUR);
        icWBCCTC[i][2] = icWBCCTC[i][4] =
            1.0f;
        icWBCCTC[i][1] = 1024.0f / fMAX(1.f, get2());
        icWBCCTC[i][3] = 1024.0f / fMAX(1.f, get2());
        icWBCCTC[i][0] = get2();
      }
    }
    else if (imCanon.ColorDataSubVer == 0xfffd)
    {
      for (i = 0; i < 15; i++)
      {
        fseek(ifp, 2, SEEK_CUR);
        norm = (signed short)get2();
        norm = 512.0f + norm / 8.0f;
        icWBCCTC[i][2] = icWBCCTC[i][4] =
            1.0f;
        icWBCCTC[i][1] = (float)get2();
        if (norm > 0.001f)
          icWBCCTC[i][1] /= norm;
        icWBCCTC[i][3] = (float)get2();
        if (norm > 0.001f)
          icWBCCTC[i][3] /= norm;
        icWBCCTC[i][0] = get2();
      }
    }
  }
  return;
}

void LibRaw::parseCanonMakernotes(unsigned tag, unsigned type, unsigned len, unsigned dng_writer)
{
  int c;
  unsigned i;

  if (tag == 0x0001) {
    Canon_CameraSettings(len);

  } else if (tag == 0x0002) { // focal length
    ilm.FocalType = get2();
    ilm.CurFocal = get2();
    if (ilm.FocalUnits > 1) {
      ilm.CurFocal /= (float)ilm.FocalUnits;
    }

  } else if (tag == 0x0004) { // subdir, ShotInfo
    short tempAp;
    if (dng_writer == nonDNG) {
      if ((i = (get4(), get2())) != 0x7fff &&
          (!iso_speed || iso_speed == 65535)) {
        iso_speed = 50 * libraw_powf64l(2.0, i / 32.0 - 4);
      }
      get4();
      if (((i = get2()) != 0xffff) && !shutter) {
        shutter = libraw_powf64l(2.0, (short)i / -32.0);
      }
      imCanon.wbi = (get2(), get2());
      shot_order = (get2(), get2());
      fseek(ifp, 4, SEEK_CUR);
    } else
      fseek(ifp, 24, SEEK_CUR);
    tempAp = get2();
    if (tempAp != 0)
      imCommon.CameraTemperature = (float)(tempAp - 128);
    tempAp = get2();
    if (tempAp != -1)
      imCommon.FlashGN = ((float)tempAp) / 32;
    get2();

    imCommon.FlashEC = _CanonConvertEV((signed short)get2());
    fseek(ifp, 8 - 32, SEEK_CUR);
    if ((tempAp = get2()) != 0x7fff)
      ilm.CurAp = _CanonConvertAperture(tempAp);
    if (ilm.CurAp < 0.7f) {
      fseek(ifp, 32, SEEK_CUR);
      ilm.CurAp = _CanonConvertAperture(get2());
    }
    if (!aperture)
      aperture = ilm.CurAp;

  } else if ((tag == 0x0007) && (dng_writer == nonDNG)) {
    fgets(model2, 64, ifp);

  } else if ((tag == 0x0008) && (dng_writer == nonDNG)) {
    shot_order = get4();

  } else if ((tag == 0x0009)  && (dng_writer == nonDNG)) {
    fread(artist, 64, 1, ifp);

  } else if (tag == 0x000c) {
    unsigned tS = get4();
    sprintf(imgdata.shootinginfo.BodySerial, "%d", tS);

  } else if ((tag == 0x0029) && (dng_writer == nonDNG)) { // PowerShot G9
    int Got_AsShotWB = 0;
    fseek(ifp, 8, SEEK_CUR);
    for (int linenum = 0; linenum < Canon_G9_linenums_2_StdWBi.size(); linenum++) {
      if (Canon_G9_linenums_2_StdWBi[linenum] != LIBRAW_WBI_Unknown ) {
        FORC4 icWBC[Canon_G9_linenums_2_StdWBi[linenum]][GRBG_2_RGBG(c)] = get4();
        if (Canon_wbi2std[imCanon.wbi] == Canon_G9_linenums_2_StdWBi[linenum]) {
          FORC4 cam_mul[c] = icWBC[Canon_G9_linenums_2_StdWBi[linenum]][c];
          Got_AsShotWB = 1;
        }
      }
      fseek(ifp, 16, SEEK_CUR);
    }
    if (!Got_AsShotWB)
      FORC4 cam_mul[c] = icWBC[LIBRAW_WBI_Auto][c];

  } else if ((tag == 0x0081) && (dng_writer == nonDNG)) { // EOS-1D, EOS-1DS
    data_offset = get4();
    fseek(ifp, data_offset + 41, SEEK_SET);
    raw_height = get2() * 2;
    raw_width = get2();
    filters = 0x61616161;

  } else if (tag == 0x0093) {
    if (!imCanon.RF_lensID) {
      fseek(ifp, 0x03d<<1, SEEK_CUR);
      imCanon.RF_lensID = get2();
    }

  } else if (tag == 0x0095 && !ilm.Lens[0])
  { // lens model tag
    fread(ilm.Lens, 64, 1, ifp);
    if (!strncmp(ilm.Lens, "EF-S", 4))
    {
      memmove(ilm.Lens + 5, ilm.Lens + 4, 60);
      ilm.Lens[4] = ' ';
      memcpy(ilm.LensFeatures_pre, ilm.Lens, 4);
      ilm.LensMount = LIBRAW_MOUNT_Canon_EF_S;
      ilm.LensFormat = LIBRAW_FORMAT_APSC;
    }
    else if (!strncmp(ilm.Lens, "EF-M", 4))
    {
      memmove(ilm.Lens + 5, ilm.Lens + 4, 60);
      ilm.Lens[4] = ' ';
      memcpy(ilm.LensFeatures_pre, ilm.Lens, 4);
      ilm.LensMount = LIBRAW_MOUNT_Canon_EF_M;
      ilm.LensFormat = LIBRAW_FORMAT_APSC;
    }
    else if (!strncmp(ilm.Lens, "EF", 2))
    {
      memmove(ilm.Lens + 3, ilm.Lens + 2, 62);
      ilm.Lens[2] = ' ';
      memcpy(ilm.LensFeatures_pre, ilm.Lens, 2);
      ilm.LensMount = LIBRAW_MOUNT_Canon_EF;
      ilm.LensFormat = LIBRAW_FORMAT_FF;
    }
    else if (!strncmp(ilm.Lens, "CN-E", 4))
    {
      memmove(ilm.Lens + 5, ilm.Lens + 4, 60);
      ilm.Lens[4] = ' ';
      memcpy(ilm.LensFeatures_pre, ilm.Lens, 4);
      ilm.LensMount = LIBRAW_MOUNT_Canon_EF;
      ilm.LensFormat = LIBRAW_FORMAT_FF;
    }
    else if (!strncmp(ilm.Lens, "TS-E", 4))
    {
      memmove(ilm.Lens + 5, ilm.Lens + 4, 60);
      ilm.Lens[4] = ' ';
      memcpy(ilm.LensFeatures_pre, ilm.Lens, 4);
      ilm.LensMount = LIBRAW_MOUNT_Canon_EF;
      ilm.LensFormat = LIBRAW_FORMAT_FF;
    }
    else if (!strncmp(ilm.Lens, "MP-E", 4))
    {
      memmove(ilm.Lens + 5, ilm.Lens + 4, 60);
      ilm.Lens[4] = ' ';
      memcpy(ilm.LensFeatures_pre, ilm.Lens, 4);
      ilm.LensMount = LIBRAW_MOUNT_Canon_EF;
      ilm.LensFormat = LIBRAW_FORMAT_FF;
    }
    else if (!strncmp(ilm.Lens, "RF", 2))
    {
      memmove(ilm.Lens + 3, ilm.Lens + 2, 62);
      ilm.Lens[2] = ' ';
      memcpy(ilm.LensFeatures_pre, ilm.Lens, 2);
      ilm.LensMount = LIBRAW_MOUNT_Canon_RF;
      ilm.LensFormat = LIBRAW_FORMAT_FF;
    }
  }
  else if (tag == 0x009a)
  { // AspectInfo
    i = get4();
    switch (i)
    {
    case 0:
    case 12: /* APS-H crop */
    case 13: /* APS-C crop */
      imgdata.sizes.raw_inset_crop.aspect = LIBRAW_IMAGE_ASPECT_3to2;
      break;
    case 1:
      imgdata.sizes.raw_inset_crop.aspect = LIBRAW_IMAGE_ASPECT_1to1;
      break;
    case 2:
      imgdata.sizes.raw_inset_crop.aspect = LIBRAW_IMAGE_ASPECT_4to3;
      break;
    case 7:
      imgdata.sizes.raw_inset_crop.aspect = LIBRAW_IMAGE_ASPECT_16to9;
      break;
    case 8:
      imgdata.sizes.raw_inset_crop.aspect = LIBRAW_IMAGE_ASPECT_5to4;
      break;
    default:
      imgdata.sizes.raw_inset_crop.aspect = LIBRAW_IMAGE_ASPECT_OTHER;
      break;
    }
    imgdata.sizes.raw_inset_crop.cwidth = get4();
    imgdata.sizes.raw_inset_crop.cheight = get4();
    imgdata.sizes.raw_inset_crop.cleft = get4();
    imgdata.sizes.raw_inset_crop.ctop = get4();

  } else if ((tag == 0x00a4) && (dng_writer == nonDNG)) { // EOS-1D, EOS-1DS
    fseek(ifp, imCanon.wbi * 48, SEEK_CUR);
    FORC3 cam_mul[c] = get2();

  } else if (tag == 0x00a9) {
    long int save1 = ftell(ifp);
    fseek(ifp, (0x1 << 1), SEEK_CUR);
    FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][RGGB_2_RGBG(c)] = get2();
    Canon_WBpresets(0, 0);
    fseek(ifp, save1, SEEK_SET);
  }
  else if (tag == 0x00b4)
  {
    switch (get2()) {
    case 1:
      imCommon.ColorSpace = LIBRAW_COLORSPACE_sRGB;
      break;
    case 2:
      imCommon.ColorSpace = LIBRAW_COLORSPACE_AdobeRGB;
      break;
    default:
      imCommon.ColorSpace = LIBRAW_COLORSPACE_Unknown;
      break;
    }
  }
  else if (tag == 0x00e0)
  { // SensorInfo
    imCanon.SensorWidth = (get2(), get2());
    imCanon.SensorHeight = get2();
    imCanon.SensorLeftBorder = (get2(), get2(), get2());
    imCanon.SensorTopBorder = get2();
    imCanon.SensorRightBorder = get2();
    imCanon.SensorBottomBorder = get2();
    imCanon.BlackMaskLeftBorder = get2();
    imCanon.BlackMaskTopBorder = get2();
    imCanon.BlackMaskRightBorder = get2();
    imCanon.BlackMaskBottomBorder = get2();
  }
  else if (tag == 0x4001 && len > 500)
  {
    int bls = 0;
    long int offsetChannelBlackLevel = 0L;
    long int offsetChannelBlackLevel2 = 0L;
    long int offsetWhiteLevels = 0L;
    long int save1 = ftell(ifp);

    switch (len)
    {

    case 582:
      imCanon.ColorDataVer = 1; // 20D / 350D

      fseek(ifp, save1 + (0x0019 << 1), SEEK_SET);
      FORC4 cam_mul[RGGB_2_RGBG(c)] = (float)get2();
      fseek(ifp, save1 + (0x001e << 1), SEEK_SET);
      FORC4 icWBC[LIBRAW_WBI_Auto][RGGB_2_RGBG(c)] = get2();
      fseek(ifp, save1 + (0x0041 << 1), SEEK_SET);
      FORC4 icWBC[LIBRAW_WBI_Custom1][RGGB_2_RGBG(c)] = get2();
      fseek(ifp, save1 + (0x0046 << 1), SEEK_SET);
      FORC4 icWBC[LIBRAW_WBI_Custom2][RGGB_2_RGBG(c)] = get2();

      fseek(ifp, save1 + (0x0023 << 1), SEEK_SET);
      Canon_WBpresets(2, 2);
      fseek(ifp, save1 + (0x004b << 1), SEEK_SET);
      Canon_WBCTpresets(1); // ABCT
      offsetChannelBlackLevel = save1 + (0x00a6 << 1);
      break;

    case 653:
      imCanon.ColorDataVer = 2; // 1Dmk2 / 1DsMK2

      fseek(ifp, save1 + (0x0018 << 1), SEEK_SET);
      FORC4 icWBC[LIBRAW_WBI_Auto][RGGB_2_RGBG(c)] = get2();
      fseek(ifp, save1 + (0x0022 << 1), SEEK_SET);
      FORC4 cam_mul[RGGB_2_RGBG(c)] = (float)get2();
      fseek(ifp, save1 + (0x0090 << 1), SEEK_SET);
      FORC4 icWBC[LIBRAW_WBI_Custom1][RGGB_2_RGBG(c)] = get2();
      fseek(ifp, save1 + (0x0095 << 1), SEEK_SET);
      FORC4 icWBC[LIBRAW_WBI_Custom2][RGGB_2_RGBG(c)] = get2();
      fseek(ifp, save1 + (0x009a << 1), SEEK_SET);
      FORC4 icWBC[LIBRAW_WBI_Custom3][RGGB_2_RGBG(c)] = get2();

      fseek(ifp, save1 + (0x0027 << 1), SEEK_SET);
      Canon_WBpresets(2, 12);
      fseek(ifp, save1 + (0x00a4 << 1), SEEK_SET);
      Canon_WBCTpresets(1); // ABCT
      offsetChannelBlackLevel = save1 + (0x011e << 1);
      break;

    case 796:
      imCanon.ColorDataVer = 3; // 1DmkIIN / 5D / 30D / 400D
      imCanon.ColorDataSubVer = get2();

      fseek(ifp, save1 + (0x003f << 1), SEEK_SET);
      FORC4 cam_mul[RGGB_2_RGBG(c)] = (float)get2();
      fseek(ifp, save1 + (0x0044 << 1), SEEK_SET);
      FORC4 icWBC[LIBRAW_WBI_Auto][RGGB_2_RGBG(c)] = get2();
      fseek(ifp, save1 + (0x0049 << 1), SEEK_SET);
      FORC4 icWBC[LIBRAW_WBI_Measured][RGGB_2_RGBG(c)] = get2();
      fseek(ifp, save1 + (0x0071 << 1), SEEK_SET);
      FORC4 icWBC[LIBRAW_WBI_Custom1][RGGB_2_RGBG(c)] = get2();
      fseek(ifp, save1 + (0x0076 << 1), SEEK_SET);
      FORC4 icWBC[LIBRAW_WBI_Custom2][RGGB_2_RGBG(c)] = get2();
      fseek(ifp, save1 + (0x007b << 1), SEEK_SET);
      FORC4 icWBC[LIBRAW_WBI_Custom3][RGGB_2_RGBG(c)] = get2();
      fseek(ifp, save1 + (0x0080 << 1), SEEK_SET);
      FORC4 icWBC[LIBRAW_WBI_Custom][RGGB_2_RGBG(c)] = get2();

      fseek(ifp, save1 + (0x004e << 1), SEEK_SET);
      Canon_WBpresets(2, 12);
      fseek(ifp, save1 + (0x0085 << 1), SEEK_SET);
      Canon_WBCTpresets(0); // BCAT
      offsetChannelBlackLevel = save1 + (0x00c4 << 1);
      break;

    // 1DmkIII / 1DSmkIII / 1DmkIV / 5DmkII
    // 7D / 40D / 50D / 60D / 450D / 500D
    // 550D / 1000D / 1100D
    case 674:
    case 692:
    case 702:
    case 1227:
    case 1250:
    case 1251:
    case 1337:
    case 1338:
    case 1346:
      imCanon.ColorDataVer = 4;
      imCanon.ColorDataSubVer = get2();

      fseek(ifp, save1 + (0x003f << 1), SEEK_SET);
      FORC4 cam_mul[RGGB_2_RGBG(c)] = (float)get2();
      fseek(ifp, save1 + (0x0044 << 1), SEEK_SET);
      FORC4 icWBC[LIBRAW_WBI_Auto][RGGB_2_RGBG(c)] = get2();
      fseek(ifp, save1 + (0x0049 << 1), SEEK_SET);
      FORC4 icWBC[LIBRAW_WBI_Measured][RGGB_2_RGBG(c)] = get2();

      fseek(ifp, save1 + (0x004e << 1), SEEK_SET);
      FORC4 sraw_mul[RGGB_2_RGBG(c)] = get2();
      fseek(ifp, save1 + (0x0053 << 1), SEEK_SET);
      Canon_WBpresets(2, 12);
      fseek(ifp, save1 + (0x00a8 << 1), SEEK_SET);
      Canon_WBCTpresets(0); // BCAT

      if ((imCanon.ColorDataSubVer == 4) ||
          (imCanon.ColorDataSubVer == 5))
      {
        offsetChannelBlackLevel = save1 + (0x02b4 << 1);
        offsetWhiteLevels = save1 + (0x02b8 << 1);
      }
      else if ((imCanon.ColorDataSubVer == 6) ||
               (imCanon.ColorDataSubVer == 7))
      {
        offsetChannelBlackLevel = save1 + (0x02cb << 1);
        offsetWhiteLevels = save1 + (0x02cf << 1);
      }
      else if (imCanon.ColorDataSubVer == 9)
      {
        offsetChannelBlackLevel = save1 + (0x02cf << 1);
        offsetWhiteLevels = save1 + (0x02d3 << 1);
      }
      else
        offsetChannelBlackLevel = save1 + (0x00e7 << 1);
      break;

    case 5120:  // PowerShot G10, G12, G5 X, G7 X, G9 X, EOS M3, EOS M5, EOS M6
      imCanon.ColorDataVer = 5;
      imCanon.ColorDataSubVer = get2();

      fseek(ifp, save1 + (0x0047 << 1), SEEK_SET);
      FORC4 cam_mul[RGGB_2_RGBG(c)] = (float)get2();

      if (imCanon.ColorDataSubVer == 0xfffc)
      { // -4: G7 X Mark II, G9 X Mark II, G1 X Mark III, M5, M100, M6
        fseek(ifp, save1 + (0x004f << 1), SEEK_SET);
        FORC4 icWBC[LIBRAW_WBI_Auto][RGGB_2_RGBG(c)] = get2();
        fseek(ifp, 8, SEEK_CUR);
        FORC4 icWBC[LIBRAW_WBI_Measured][RGGB_2_RGBG(c)] =
            get2();
        fseek(ifp, 8, SEEK_CUR);
        FORC4 icWBC[LIBRAW_WBI_Other][RGGB_2_RGBG(c)] = get2();
        fseek(ifp, 8, SEEK_CUR);
        Canon_WBpresets(8, 24);
        fseek(ifp, 168, SEEK_CUR);
        FORC4 icWBC[LIBRAW_WBI_FL_WW][RGGB_2_RGBG(c)] = get2();
        fseek(ifp, 24, SEEK_CUR);
        Canon_WBCTpresets(2); // BCADT
        offsetChannelBlackLevel = save1 + (0x014d << 1);
        offsetWhiteLevels = save1 + (0x0569 << 1);
      }
      else if (imCanon.ColorDataSubVer == 0xfffd)
      { // -3: M10/M3/G1 X/G1 X II/G10/G11/G12/G15/G16/G3 X/G5 X/G7 X/G9
        // X/S100/S110/S120/S90/S95/SX1 IX/SX50 HS/SX60 HS
        fseek(ifp, save1 + (0x004c << 1), SEEK_SET);
        FORC4 icWBC[LIBRAW_WBI_Auto][RGGB_2_RGBG(c)] = get2();
        get2();
        FORC4 icWBC[LIBRAW_WBI_Measured][RGGB_2_RGBG(c)] =
            get2();
        get2();
        FORC4 icWBC[LIBRAW_WBI_Other][RGGB_2_RGBG(c)] = get2();
        get2();
        Canon_WBpresets(2, 12);
        fseek(ifp, save1 + (0x00ba << 1), SEEK_SET);
        Canon_WBCTpresets(2); // BCADT
        offsetChannelBlackLevel = save1 + (0x0108 << 1);
      }
      break;

    case 1273:
    case 1275:
      imCanon.ColorDataVer = 6; // 600D / 1200D
      imCanon.ColorDataSubVer = get2();

      fseek(ifp, save1 + (0x003f << 1), SEEK_SET);
      FORC4 cam_mul[RGGB_2_RGBG(c)] = (float)get2();
      fseek(ifp, save1 + (0x0044 << 1), SEEK_SET);
      FORC4 icWBC[LIBRAW_WBI_Auto][RGGB_2_RGBG(c)] = get2();
      fseek(ifp, save1 + (0x0049 << 1), SEEK_SET);
      FORC4 icWBC[LIBRAW_WBI_Measured][RGGB_2_RGBG(c)] = get2();

      fseek(ifp, save1 + (0x0062 << 1), SEEK_SET);
      FORC4 sraw_mul[RGGB_2_RGBG(c)] = get2();
      fseek(ifp, save1 + (0x0067 << 1), SEEK_SET);
      Canon_WBpresets(2, 12);
      fseek(ifp, save1 + (0x00bc << 1), SEEK_SET);
      Canon_WBCTpresets(0); // BCAT
      offsetChannelBlackLevel = save1 + (0x01df << 1);
      offsetWhiteLevels = save1 + (0x01e3 << 1);
      break;

    // 1DX / 5DmkIII / 6D / 100D / 650D / 700D / EOS M / 7DmkII / 750D / 760D
    case 1312:
    case 1313:
    case 1316:
    case 1506:
      imCanon.ColorDataVer = 7;
      imCanon.ColorDataSubVer = get2();

      fseek(ifp, save1 + (0x003f << 1), SEEK_SET);
      FORC4 cam_mul[RGGB_2_RGBG(c)] = (float)get2();
      fseek(ifp, save1 + (0x0044 << 1), SEEK_SET);
      FORC4 icWBC[LIBRAW_WBI_Auto][RGGB_2_RGBG(c)] = get2();
      fseek(ifp, save1 + (0x0049 << 1), SEEK_SET);
      FORC4 icWBC[LIBRAW_WBI_Measured][RGGB_2_RGBG(c)] = get2();

      fseek(ifp, save1 + (0x007b << 1), SEEK_SET);
      FORC4 sraw_mul[RGGB_2_RGBG(c)] = get2();
      fseek(ifp, save1 + (0x0080 << 1), SEEK_SET);
      Canon_WBpresets(2, 12);
      fseek(ifp, save1 + (0x00d5 << 1), SEEK_SET);
      Canon_WBCTpresets(0); // BCAT

      if (imCanon.ColorDataSubVer == 10)
      {
        offsetChannelBlackLevel = save1 + (0x01f8 << 1);
        offsetWhiteLevels = save1 + (0x01fc << 1);
      }
      else if (imCanon.ColorDataSubVer == 11)
      {
        offsetChannelBlackLevel = save1 + (0x02d8 << 1);
        offsetWhiteLevels = save1 + (0x02dc << 1);
      }
      break;

    // 5DS / 5DS R / 80D / 1300D / 1500D / 3000D / 5D4 / 800D / 77D / 6D II /
    // 200D
    case 1560:
    case 1592:
    case 1353:
    case 1602:
      imCanon.ColorDataVer = 8;
      imCanon.ColorDataSubVer = get2();

      fseek(ifp, save1 + (0x003f << 1), SEEK_SET);
      FORC4 cam_mul[RGGB_2_RGBG(c)] = (float)get2();
      fseek(ifp, save1 + (0x0044 << 1), SEEK_SET);
      FORC4 icWBC[LIBRAW_WBI_Auto][RGGB_2_RGBG(c)] = get2();
      fseek(ifp, save1 + (0x0049 << 1), SEEK_SET);
      FORC4 icWBC[LIBRAW_WBI_Measured][RGGB_2_RGBG(c)] = get2();

      fseek(ifp, save1 + (0x0080 << 1), SEEK_SET);
      FORC4 sraw_mul[RGGB_2_RGBG(c)] = get2();
      fseek(ifp, save1 + (0x0085 << 1), SEEK_SET);
      Canon_WBpresets(2, 12);
      fseek(ifp, save1 + (0x0107 << 1), SEEK_SET);
      Canon_WBCTpresets(0); // BCAT

      if (imCanon.ColorDataSubVer == 14)
      { // 1300D / 1500D / 3000D
        offsetChannelBlackLevel = save1 + (0x022c << 1);
        offsetWhiteLevels = save1 + (0x0230 << 1);
      }
      else
      {
        offsetChannelBlackLevel = save1 + (0x030a << 1);
        offsetWhiteLevels = save1 + (0x030e << 1);
      }
      break;

    case 1820: // M50, ColorDataSubVer 16
    case 1824: // EOS R, SX740HS, ColorDataSubVer 17
    case 1816: // EOS RP, SX70HS, ColorDataSubVer 18;
               // EOS M6 Mark II, EOS 90D, G7XmkIII, ColorDataSubVer 19
      imCanon.ColorDataVer = 9;
      imCanon.ColorDataSubVer = get2();

      fseek(ifp, save1 + (0x0047 << 1), SEEK_SET);
      FORC4 cam_mul[RGGB_2_RGBG(c)] = (float)get2();
      get2();
      FORC4 icWBC[LIBRAW_WBI_Auto][RGGB_2_RGBG(c)] = get2();
      get2();
      FORC4 icWBC[LIBRAW_WBI_Measured][RGGB_2_RGBG(c)] = get2();
      fseek(ifp, save1 + (0x0088 << 1), SEEK_SET);
      Canon_WBpresets(2, 12);
      fseek(ifp, save1 + (0x010a << 1), SEEK_SET);
      Canon_WBCTpresets(0);
      offsetChannelBlackLevel = save1 + (0x0318 << 1);
      offsetChannelBlackLevel2 = save1 + (0x0149 << 1);
      offsetWhiteLevels = save1 + (0x031c << 1);
      break;

    case 2024: // 1D X Mark III, ColorDataSubVer 32
      imCanon.ColorDataVer = 10;
      imCanon.ColorDataSubVer = get2();
      fseek(ifp, save1 + (0x0055 << 1), SEEK_SET);
      FORC4 cam_mul[RGGB_2_RGBG(c)] = (float)get2();
      get2();
      FORC4 icWBC[LIBRAW_WBI_Auto][RGGB_2_RGBG(c)] = get2();
      get2();
      FORC4 icWBC[LIBRAW_WBI_Measured][RGGB_2_RGBG(c)] = get2();
      fseek(ifp, save1 + (0x0096 << 1), SEEK_SET);
      Canon_WBpresets(2, 12);
      fseek(ifp, save1 + (0x0118 << 1), SEEK_SET);
      Canon_WBCTpresets(0);
      offsetChannelBlackLevel = save1 + (0x0326 << 1);
      offsetChannelBlackLevel2 = save1 + (0x0157 << 1);
      offsetWhiteLevels = save1 + (0x032a << 1);
      break;

   default:
   	imCanon.ColorDataSubVer = get2();
      break;
    }

    if (offsetChannelBlackLevel)
    {
      fseek(ifp, offsetChannelBlackLevel, SEEK_SET);
      FORC4
        bls += (imCanon.ChannelBlackLevel[RGGB_2_RGBG(c)] = get2());
      imCanon.AverageBlackLevel = bls / 4;
    }
    if (offsetWhiteLevels)
    {
      if ((offsetWhiteLevels - offsetChannelBlackLevel) != 8L)
        fseek(ifp, offsetWhiteLevels, SEEK_SET);
      imCanon.NormalWhiteLevel = get2();
      imCanon.SpecularWhiteLevel = get2();
      FORC4
        imgdata.color.linear_max[c] = imCanon.SpecularWhiteLevel;
    }

    if(!imCanon.AverageBlackLevel && offsetChannelBlackLevel2)
    {
        fseek(ifp, offsetChannelBlackLevel2, SEEK_SET);
        FORC4
            bls += (imCanon.ChannelBlackLevel[RGGB_2_RGBG(c)] = get2());
        imCanon.AverageBlackLevel = bls / 4;
    }
    fseek(ifp, save1, SEEK_SET);

  } else if (tag == 0x4013) {
    get4();
    imCanon.AFMicroAdjMode = get4();
    float a = get4();
    float b = get4();
    if (fabsf(b) > 0.001f)
      imCanon.AFMicroAdjValue = a / b;

  } else if ((tag == 0x4021) && (dng_writer == nonDNG) &&
             (imCanon.multishot[0] = get4()) &&
             (imCanon.multishot[1] = get4())) {
    if (len >= 4) {
      imCanon.multishot[2] = get4();
      imCanon.multishot[3] = get4();
    }
    FORC4 cam_mul[c] = 1024;
  }

}
