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

void LibRaw::parseSamsungMakernotes(int base, unsigned tag, unsigned type,
                                    unsigned len, unsigned dng_writer)
{
  int i, c;
  if (tag == 0x0002)
  {
    imSamsung.DeviceType = get4();
    if (imSamsung.DeviceType == 0x2000)
    {
      ilm.CameraMount = LIBRAW_MOUNT_Samsung_NX;
      ilm.CameraFormat = LIBRAW_FORMAT_APSC;
    }
    else if (!strncmp(model, "NX mini", 7))
    { // device type 0x1000: 'NX mini', EX2F, EX1, WB2000
      ilm.CameraMount = LIBRAW_MOUNT_Samsung_NX_M;
      ilm.CameraFormat = LIBRAW_FORMAT_1INCH;
    }
    else
    {
      ilm.CameraMount = LIBRAW_MOUNT_FixedLens;
      ilm.LensMount = LIBRAW_MOUNT_FixedLens;
    }
  }
  else if (tag == 0x0003)
  {
    ilm.CamID = unique_id = get4();
  }
  else if (tag == 0x0043)
  {
    if ((i = get4()))
    {
      imgdata.makernotes.common.CameraTemperature = (float)i;
      if (get4() == 10)
        imgdata.makernotes.common.CameraTemperature /= 10.0f;
    }
  }
  else if ((tag == 0xa002) && (dng_writer != AdobeDNG))
  {
    stmread(imgdata.shootinginfo.BodySerial, len, ifp);
  }
  else if (tag == 0xa003)
  {
    ilm.LensID = get2();
    if (ilm.LensID)
      ilm.LensMount = LIBRAW_MOUNT_Samsung_NX;
  }
  else if (tag == 0xa004)
  { // LensFirmware
    stmread(imSamsung.LensFirmware, len, ifp);
  }
  else if (tag == 0xa005)
  {
    stmread(imgdata.lens.InternalLensSerial, len, ifp);
  }
  else if (tag == 0xa010)
  {
    FORC4 imSamsung.ImageSizeFull[c] = get4();
    FORC4 imSamsung.ImageSizeCrop[c] = get4();
  }
  else if ((tag == 0xa011) && ((len == 1) || (len == 2)) && (type == 3))
  {
    imSamsung.ColorSpace[0] = (int)get2();
    if (len == 2)
      imSamsung.ColorSpace[1] = (int)get2();
  }
  else if (tag == 0xa019)
  {
    ilm.CurAp = getreal(type);
  }
  else if ((tag == 0xa01a) && (unique_id != 0x5000000) &&
           (!imgdata.lens.FocalLengthIn35mmFormat))
  {
    ilm.FocalLengthIn35mmFormat = get4();
    if (ilm.FocalLengthIn35mmFormat >= 160)
      ilm.FocalLengthIn35mmFormat /= 10.0f;
    if ((ilm.CameraMount == LIBRAW_MOUNT_Samsung_NX_M) &&
        (imSamsung.LensFirmware[10] < '6'))
      ilm.FocalLengthIn35mmFormat *= 1.6f;
  }
  else if (tag == 0xa020)
  {
    FORC(11) imSamsung.key[c] = get4();
  }
  else if ((tag == 0xa021) && (dng_writer == nonDNG))
  {
    FORC4 cam_mul[c ^ (c >> 1)] = get4() - imSamsung.key[c];
  }
  else if (tag == 0xa022)
  {
    FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][c ^ (c >> 1)] =
        get4() - imSamsung.key[c + 4];
    if (imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][0] <
        (imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][1] >> 1))
    {
      imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][1] =
          imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][1] >> 4;
      imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][3] =
          imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][3] >> 4;
    }
  }
  else if (tag == 0xa023)
  {
    imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][0] =
        get4() - imSamsung.key[8];
    imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][1] =
        get4() - imSamsung.key[9];
    imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][3] =
        get4() - imSamsung.key[10];
    imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][2] =
        get4() - imSamsung.key[0];
    if (imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][0] <
        (imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][1] >> 1))
    {
      imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][1] =
          imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][1] >> 4;
      imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][3] =
          imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][3] >> 4;
    }
  }
  else if (tag == 0xa024)
  {
    FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_D65][c ^ (c >> 1)] =
        get4() - imSamsung.key[c + 1];
    if (imgdata.color.WB_Coeffs[LIBRAW_WBI_D65][0] <
        (imgdata.color.WB_Coeffs[LIBRAW_WBI_D65][1] >> 1))
    {
      imgdata.color.WB_Coeffs[LIBRAW_WBI_D65][1] =
          imgdata.color.WB_Coeffs[LIBRAW_WBI_D65][1] >> 4;
      imgdata.color.WB_Coeffs[LIBRAW_WBI_D65][3] =
          imgdata.color.WB_Coeffs[LIBRAW_WBI_D65][3] >> 4;
    }
  }
  else if (tag == 0xa025)
  {
    unsigned t = get4() + imSamsung.key[0];
    if (t == 4096)
      imSamsung.DigitalGain = 1.0;
    else
      imSamsung.DigitalGain = ((double)t) / 4096.0;
  }
  else if ((tag == 0xa028) && (dng_writer == nonDNG))
  {
    FORC4 cblack[c ^ (c >> 1)] = get4() - imSamsung.key[c];
  }
  else if ((tag == 0xa030) && (len == 9))
  {
    for (i = 0; i < 3; i++)
      FORC3 imgdata.color.ccm[i][c] =
          (float)((short)((get4() + imSamsung.key[i * 3 + c]))) / 256.0;
  }
  else if ((tag == 0xa032) && (len == 9) && (dng_writer == nonDNG))
  {
    double aRGB_cam[3][3];
    FORC(9)
    ((double *)aRGB_cam)[c] =
        ((double)((short)((get4() + imSamsung.key[c])))) / 256.0;
    aRGB_coeff(aRGB_cam);
  }
}
