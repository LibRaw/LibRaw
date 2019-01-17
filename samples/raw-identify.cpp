/* -*- C++ -*-
 * File: identify.cpp
 * Copyright 2008-2019 LibRaw LLC (info@libraw.org)
 * Created: Sat Mar  8, 2008
 *
 * LibRaw C++ demo: emulates dcraw -i [-v]
 *

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).


 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "libraw/libraw.h"

#ifdef WIN32
#define snprintf _snprintf
#define strcasecmp stricmp
#define strncasecmp strnicmp
#endif

#define P1 MyCoolRawProcessor.imgdata.idata
#define P2 MyCoolRawProcessor.imgdata.other

#define mnLens MyCoolRawProcessor.imgdata.lens.makernotes
#define exifLens MyCoolRawProcessor.imgdata.lens
#define ShootingInfo MyCoolRawProcessor.imgdata.shootinginfo

#define S MyCoolRawProcessor.imgdata.sizes
#define O MyCoolRawProcessor.imgdata.params
#define C MyCoolRawProcessor.imgdata.color
#define T MyCoolRawProcessor.imgdata.thumbnail

#define Canon MyCoolRawProcessor.imgdata.makernotes.canon
#define Hasselblad MyCoolRawProcessor.imgdata.makernotes.hasselblad
#define Fuji MyCoolRawProcessor.imgdata.makernotes.fuji
#define Nikon MyCoolRawProcessor.imgdata.makernotes.nikon
#define Oly MyCoolRawProcessor.imgdata.makernotes.olympus
#define Sony MyCoolRawProcessor.imgdata.makernotes.sony

const char *EXIF_LightSources[] = {
    "Unknown",
    "Daylight",
    "Fluorescent",
    "Tungsten",
    "Flash",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Fine Weather",
    "Cloudy",
    "Shade",
    "Daylight Fluorescent D",
    "Day White Fluorescent N",
    "Cool White Fluorescent W",
    "White Fluorescent WW",
    "Warm White Fluorescent L",
    "Illuminant A",
    "Illuminant B",
    "Illuminant C",
    "D55",
    "D65",
    "D75",
    "D50",
    "ISO Studio Tungsten",
};
/*
table of fluorescents:
12 = FL-D; Daylight fluorescent (D 5700K – 7100K) (F1,F5)
13 = FL-N; Day white fluorescent (N 4600K – 5400K) (F7,F8)
14 = FL-W; Cool white fluorescent (W 3900K – 4500K) (F2,F6, office, store, warehouse)
15 = FL-WW; White fluorescent (WW 3200K – 3700K) (F3, residential)
16 = FL-L; Soft/Warm white fluorescent (L 2600K - 3250K) (F4, kitchen, bath)
*/
const char *WB_LightSources[] = {
    "Unknown",  "Daylight", "Fluorescent",  "Tungsten",        "Flash",  "Reserved", "Reserved",
    "Reserved", "Reserved", "Fine Weather", "Cloudy",          "Shade",  "FL-D",     "FL-N",
    "FL-W",     "FL-WW",    "FL-L",         "Ill. A",          "Ill. B", "Ill. C",   "D55",
    "D65",      "D75",      "D50",          "Studio Tungsten",
};

void trimSpaces(char *s)
{
  char *p = s;
  if (!strncasecmp(p, "NO=", 3))
    p = p + 3; /* fix for Nikon D70, D70s */
  int l = strlen(p);
  if (!l)
    return;
  while (isspace(p[l - 1]))
    p[--l] = 0; /* trim trailing spaces */
  while (*p && isspace(*p))
    ++p, --l; /* trim leading spaces */
  memmove(s, p, l + 1);
}

int main(int ac, char *av[])
{
  int ret;
  int verbose = 0, print_sz = 0, print_unpack = 0, print_frame = 0, print_wb = 0;
  int compact = 0, print_1 = 0, print_2 = 0;
  LibRaw MyCoolRawProcessor;

  for (int i = 1; i < ac; i++)
  {
    if (av[i][0] == '-')
    {
      if (av[i][1] == 'c' && av[i][2] == 0)
        compact++;
      if (av[i][1] == 'v' && av[i][2] == 0)
        verbose++;
      if (av[i][1] == 'w' && av[i][2] == 0)
        print_wb++;
      if (av[i][1] == 'u' && av[i][2] == 0)
        print_unpack++;
      if (av[i][1] == 's' && av[i][2] == 0)
        print_sz++;
      if (av[i][1] == 'h' && av[i][2] == 0)
        O.half_size = 1;
      if (av[i][1] == 'f' && av[i][2] == 0)
        print_frame++;
      if (av[i][1] == '1' && av[i][2] == 0)
        print_1++;
      if (av[i][1] == '2' && av[i][2] == 0)
        print_2++;
      continue;
    }

    if ((ret = MyCoolRawProcessor.open_file(av[i])) != LIBRAW_SUCCESS)
    {
      printf("Cannot decode %s: %s\n", av[i], libraw_strerror(ret));
      continue; // no recycle, open_file will recycle
    }

    if (print_sz)
    {
      printf("%s\t%s\t%s\t%d\t%d\n", av[i], P1.make, P1.model, S.width, S.height);
    }
    else if (print_1)
    {
      char frame[64] = "";
      snprintf(frame, 64, "rw %d rh %d lm %d tm %d",
               S.raw_width, S.raw_height, S.left_margin, S.top_margin);
      printf("%s=%s=nFms %02d=%s=bps %02d=%s", P1.make, P1.model, P1.raw_count, MyCoolRawProcessor.unpack_function_name(), C.raw_bps, frame);
      printf("\n");
    }
    else if (print_2)
    {
      printf("// %s %s", P1.make, P1.model);
      if (C.cam_mul[0] > 0)
      {
        printf("\n'As shot' WB:");
        for (int c = 0; c < 4; c++)
          printf(" %.3f", C.cam_mul[c]);
      }
      if (C.WB_Coeffs[LIBRAW_WBI_Auto][0] > 0)
      {
        printf("\n'Camera Auto' WB:");
        for (int c = 0; c < 4; c++)
          printf(" %d", C.WB_Coeffs[LIBRAW_WBI_Auto][c]);
      }
      if (C.WB_Coeffs[LIBRAW_WBI_Measured][0] > 0)
      {
        printf("\n'Camera Measured' WB:");
        for (int c = 0; c < 4; c++)
          printf(" %d", C.WB_Coeffs[LIBRAW_WBI_Measured][c]);
      }
      printf("\n\n");
    }
    else if (verbose)
    {
      if ((ret = MyCoolRawProcessor.adjust_sizes_info_only()))
      {
        printf("Cannot decode %s: %s\n", av[i], libraw_strerror(ret));
        continue; // no recycle, open_file will recycle
      }

      printf("\nFilename: %s\n", av[i]);
      printf("Timestamp: %s", ctime(&(P2.timestamp)));
      printf("Camera: %s %s ID: 0x%llx\n", P1.make, P1.model, mnLens.CamID);

      if (ShootingInfo.BodySerial[0] && strcmp(ShootingInfo.BodySerial, "0")) {
        trimSpaces(ShootingInfo.BodySerial);
        printf("Body#: %s", ShootingInfo.BodySerial);
      } else if (C.model2[0] && (!strncasecmp(P1.make, "Kodak", 5) || !strcmp(P1.model, "EOS D2000C"))) {
        trimSpaces(C.model2);
        printf("Body#: %s", C.model2);
      }
      if (ShootingInfo.InternalBodySerial[0]) {
        trimSpaces(ShootingInfo.InternalBodySerial);
        printf(" BodyAssy#: %s", ShootingInfo.InternalBodySerial);
      }
      if (exifLens.LensSerial[0]) {
        trimSpaces(exifLens.LensSerial);
        printf(" Lens#: %s", exifLens.LensSerial);
      }
      if (exifLens.InternalLensSerial[0]) {
        trimSpaces(exifLens.InternalLensSerial);
        printf(" LensAssy#: %s", exifLens.InternalLensSerial);
      }
      if (P2.artist[0])
        printf(" Owner: %s\n", P2.artist);
      if (P1.dng_version)
      {
        printf(" DNG Version: ");
        for (int i = 24; i >= 0; i -= 8)
          printf("%d%c", P1.dng_version >> i & 255, i ? '.' : '\n');
      }
      printf("\nEXIF:\n");
      printf("\tMinFocal: %0.1f mm\n", exifLens.MinFocal);
      printf("\tMaxFocal: %0.1f mm\n", exifLens.MaxFocal);
      printf("\tMaxAp @MinFocal: f/%0.1f\n", exifLens.MaxAp4MinFocal);
      printf("\tMaxAp @MaxFocal: f/%0.1f\n", exifLens.MaxAp4MaxFocal);
      printf("\tMaxAperture @CurFocal: f/%0.1f\n", exifLens.EXIF_MaxAp);
      printf("\tFocalLengthIn35mmFormat: %d mm\n", exifLens.FocalLengthIn35mmFormat);
      printf("\tLensMake: %s\n", exifLens.LensMake);
      printf("\tLens: %s\n", exifLens.Lens);
      printf("\n");

      printf("\nMakernotes:\n");
      printf("\tDriveMode: %d\n" ,ShootingInfo.DriveMode);
      printf("\tFocusMode: %d\n", ShootingInfo.FocusMode);
      printf("\tMeteringMode: %d\n", ShootingInfo.MeteringMode);
      printf("\tAFPoint: %d\n", ShootingInfo.AFPoint);
      printf("\tExposureMode: %d\n", ShootingInfo.ExposureMode);
      printf("\tExposureProgram: %d\n", ShootingInfo.ExposureProgram);
      printf("\tImageStabilization: %d\n", ShootingInfo.ImageStabilization);
      if (mnLens.body[0])
      {
        printf("\tMF Camera Body: %s\n", mnLens.body);
      }
      printf("\tCameraFormat: %d, ", mnLens.CameraFormat);
      switch (mnLens.CameraFormat)
      {
      case 0:
        printf("Undefined\n");
        break;
      case 1:
        printf("APS-C\n");
        break;
      case 2:
        printf("FF\n");
        break;
      case 3:
        printf("MF\n");
        break;
      case 4:
        printf("APS-H\n");
        break;
      case 5:
        printf("1\"\n");
        break;
      case 6:
        printf("1/2.3\"\n");
        break;
      case 8:
        printf("4/3\n");
        break;
      case 15:
        printf("Leica DMR\n");
        break;
      default:
        printf("Unknown\n");
        break;
      }

      if (!strncasecmp(P1.make, "Nikon", 5) && Nikon.CropData[0]) {
        printf("\tNikon crop: %d, ", Nikon.CropFormat);
        switch (Nikon.CropFormat) {
        case 0:
          printf("Off\n");
          break;
        case 1:
          printf("1.3x\n");
          break;
        case 2:
          printf("DX\n");
          break;
        case 3:
          printf("5:4\n");
          break;
        case 4:
          printf("3:2\n");
          break;
        case 6:
          printf("16:9\n");
          break;
        case 11:
          printf("FX uncropped\n");
          break;
        case 12:
          printf("DX uncropped\n");
          break;
        case 17:
          printf("1:1\n");
          break;
        default:
          printf("Unknown\n");
          break;
        }
        printf("\tSensor %d x %d; crop: %d x %d; top left pixel: (%d, %d)\n",
          Nikon.CropData[0], Nikon.CropData[1], Nikon.CropData[2], Nikon.CropData[3], Nikon.CropData[4], Nikon.CropData[5]);
      }

      printf("\tCameraMount: %d, ", mnLens.CameraMount);
      switch (mnLens.CameraMount)
      {
      case 0:
        printf("Undefined or Fixed Lens\n");
        break;
      case 1:
        printf("Sony/Minolta A\n");
        break;
      case 2:
        printf("Sony E\n");
        break;
      case 3:
        printf("Canon EF\n");
        break;
      case 4:
        printf("Canon EF-S\n");
        break;
      case 5:
        printf("Canon EF-M\n");
        break;
      case 6:
        printf("Nikon F\n");
        break;
      case 7:
        printf("Nikon CX\n");
        break;
      case 8:
        printf("4/3\n");
        break;
      case 9:
        printf("m4/3\n");
        break;
      case 10:
        printf("Pentax K\n");
        break;
      case 11:
        printf("Pentax Q\n");
        break;
      case 12:
        printf("Pentax 645\n");
        break;
      case 13:
        printf("Fuji X\n");
        break;
      case 14:
        printf("Leica M\n");
        break;
      case 15:
        printf("Leica R\n");
        break;
      case 16:
        printf("Leica S\n");
        break;
      case 17:
        printf("Samsung NX\n");
        break;
      case 19:
        printf("Samsung NX-M\n");
        break;
      case 20:
        printf("Leica L\n");
        break;
      case 21:
        printf ("Contax N\n");
        break;
      case 22:
        printf ("Sigma X3F\n");
        break;
      case 23:
        printf ("Leica TL\n");
        break;
      case 24:
        printf ("Leica SL\n");
        break;
      case 25:
        printf ("Nikon Z\n");
        break;
      case 99:
        printf("Fixed Lens\n");
        break;
      default:
        printf("Unknown\n");
        break;
      }

      if (mnLens.LensID == -1)
      {
        printf("\tLensID: n/a\n");
      }
      else
      {
        printf("\tLensID: %llu 0x%0llx\n", mnLens.LensID, mnLens.LensID);
      }
      printf("\tLens: %s\n", mnLens.Lens);
      printf("\tLensFormat: %d, ", mnLens.LensFormat);
      switch (mnLens.LensFormat)
      {
      case 0:
        printf("Undefined\n");
        break;
      case 1:
        printf("APS-C\n");
        break;
      case 2:
        printf("FF\n");
        break;
      case 3:
        printf("MF\n");
        break;
      case 8:
        printf("4/3\n");
        break;
      default:
        printf("Unknown\n");
        break;
      }
      printf("\tLensMount: %d, ", mnLens.LensMount);
      switch (mnLens.LensMount)
      {
      case 0:
        printf("Undefined or Fixed Lens\n");
        break;
      case 1:
        printf("Sony/Minolta A\n");
        break;
      case 2:
        printf("Sony E\n");
        break;
      case 3:
        printf("Canon EF\n");
        break;
      case 4:
        printf("Canon EF-S\n");
        break;
      case 5:
        printf("Canon EF-M\n");
        break;
      case 6:
        printf("Nikon F\n");
        break;
      case 7:
        printf("Nikon CX\n");
        break;
      case 8:
        printf("4/3\n");
        break;
      case 9:
        printf("m4/3\n");
        break;
      case 10:
        printf("Pentax K\n");
        break;
      case 11:
        printf("Pentax Q\n");
        break;
      case 12:
        printf("Pentax 645\n");
        break;
      case 13:
        printf("Fuji X\n");
        break;
      case 14:
        printf("Leica M\n");
        break;
      case 15:
        printf("Leica R\n");
        break;
      case 16:
        printf("Leica S\n");
        break;
      case 17:
        printf("Samsung NX\n");
        break;
      case 18:
        printf("Ricoh module\n");
        break;
      case 20:
        printf("Leica L\n");
        break;
      case 21:
        printf ("Contax N\n");
        break;
      case 22:
        printf ("Sigma X3F\n");
        break;
      case 23:
        printf ("Leica TL\n");
        break;
      case 24:
        printf ("Leica SL\n");
        break;
      case 99:
        printf("Fixed Lens\n");
        break;
      default:
        printf("Unknown\n");
        break;
      }
      printf("\tFocalType: %d, ", mnLens.FocalType);
      switch (mnLens.FocalType)
      {
      case 0:
        printf("Undefined\n");
        break;
      case 1:
        printf("Fixed Focal\n");
        break;
      case 2:
        printf("Zoom\n");
        break;
      default:
        printf("Unknown\n");
        break;
      }
      printf("\tLensFeatures_pre: %s\n", mnLens.LensFeatures_pre);
      printf("\tLensFeatures_suf: %s\n", mnLens.LensFeatures_suf);
      printf("\tMinFocal: %0.1f mm\n", mnLens.MinFocal);
      printf("\tMaxFocal: %0.1f mm\n", mnLens.MaxFocal);
      printf("\tMaxAp @MinFocal: f/%0.1f\n", mnLens.MaxAp4MinFocal);
      printf("\tMaxAp @MaxFocal: f/%0.1f\n", mnLens.MaxAp4MaxFocal);
      printf("\tMinAp @MinFocal: f/%0.1f\n", mnLens.MinAp4MinFocal);
      printf("\tMinAp @MaxFocal: f/%0.1f\n", mnLens.MinAp4MaxFocal);
      printf("\tMaxAp: f/%0.1f\n", mnLens.MaxAp);
      printf("\tMinAp: f/%0.1f\n", mnLens.MinAp);
      printf("\tCurFocal: %0.1f mm\n", mnLens.CurFocal);
      printf("\tCurAp: f/%0.1f\n", mnLens.CurAp);
      printf("\tMaxAp @CurFocal: f/%0.1f\n", mnLens.MaxAp4CurFocal);
      printf("\tMinAp @CurFocal: f/%0.1f\n", mnLens.MinAp4CurFocal);

      if (exifLens.makernotes.FocalLengthIn35mmFormat > 1.0f)
        printf("\tFocalLengthIn35mmFormat: %0.1f mm\n", exifLens.makernotes.FocalLengthIn35mmFormat);

      if (exifLens.nikon.NikonEffectiveMaxAp > 0.1f)
        printf("\tNikonEffectiveMaxAp: f/%0.1f\n", exifLens.nikon.NikonEffectiveMaxAp);

      if (exifLens.makernotes.LensFStops > 0.1f)
        printf("\tLensFStops @CurFocal: %0.2f\n", exifLens.makernotes.LensFStops);

      printf("\tTeleconverterID: %lld\n", mnLens.TeleconverterID);
      printf("\tTeleconverter: %s\n", mnLens.Teleconverter);
      printf("\tAdapterID: %lld\n", mnLens.AdapterID);
      printf("\tAdapter: %s\n", mnLens.Adapter);
      printf("\tAttachmentID: %lld\n", mnLens.AttachmentID);
      printf("\tAttachment: %s\n", mnLens.Attachment);
      printf("\n");

      printf("ISO speed: %d\n", (int)P2.iso_speed);
      if (P2.real_ISO > 0.1f)
        printf("real ISO speed: %d\n", (int)P2.real_ISO);
      printf("Shutter: ");
      if (P2.shutter > 0 && P2.shutter < 1)
        P2.shutter = (printf("1/"), 1.0f / P2.shutter);
      printf("%0.1f sec\n", P2.shutter);
      printf("Aperture: f/%0.1f\n", P2.aperture);
      printf("Focal length: %0.1f mm\n", P2.focal_len);
      if (P2.exifAmbientTemperature > -273.15f)
        printf("Ambient temperature (exif data): %6.2f° C\n", P2.exifAmbientTemperature);
      if (P2.CameraTemperature > -273.15f)
        printf("Camera temperature: %6.2f° C\n", P2.CameraTemperature);
      if (P2.SensorTemperature > -273.15f)
        printf("Sensor temperature: %6.2f° C\n", P2.SensorTemperature);
      if (P2.SensorTemperature2 > -273.15f)
        printf("Sensor temperature2: %6.2f° C\n", P2.SensorTemperature2);
      if (P2.LensTemperature > -273.15f)
        printf("Lens temperature: %6.2f° C\n", P2.LensTemperature);
      if (P2.AmbientTemperature > -273.15f)
        printf("Ambient temperature: %6.2f° C\n", P2.AmbientTemperature);
      if (P2.BatteryTemperature > -273.15f)
        printf("Battery temperature: %6.2f° C\n", P2.BatteryTemperature);
      if (P2.FlashGN > 1.0f)
        printf("Flash Guide Number: %6.2f\n", P2.FlashGN);
      printf("Flash exposure compensation: %0.2f EV\n", P2.FlashEC);
      if (C.profile)
        printf("Embedded ICC profile: yes, %d bytes\n", C.profile_length);
      else
        printf("Embedded ICC profile: no\n");

      if (C.dng_levels.baseline_exposure > -999.f)
        printf("Baseline exposure: %04.3f\n", C.dng_levels.baseline_exposure);

      printf("Number of raw images: %d\n", P1.raw_count);
      if (Fuji.FujiExpoMidPointShift > -999.f)
        printf("Fuji Exposure shift: %04.3f\n", Fuji.FujiExpoMidPointShift);

      if (Fuji.FujiDynamicRange != 0xffff)
        printf("Fuji Dynamic Range: %d\n", Fuji.FujiDynamicRange);
      if (Fuji.FujiFilmMode != 0xffff)
        printf("Fuji Film Mode: %d\n", Fuji.FujiFilmMode);
      if (Fuji.FujiDynamicRangeSetting != 0xffff)
        printf("Fuji Dynamic Range Setting: %d\n", Fuji.FujiDynamicRangeSetting);
      if (Fuji.FujiDevelopmentDynamicRange != 0xffff)
        printf("Fuji Development Dynamic Range: %d\n", Fuji.FujiDevelopmentDynamicRange);
      if (Fuji.FujiAutoDynamicRange != 0xffff)
        printf("Fuji Auto Dynamic Range: %d\n", Fuji.FujiAutoDynamicRange);

      if (S.pixel_aspect != 1)
        printf("Pixel Aspect Ratio: %0.6f\n", S.pixel_aspect);
      if (T.tlength)
        printf("Thumb size:  %4d x %d\n", T.twidth, T.theight);
      printf("Full size:   %4d x %d\n", S.raw_width, S.raw_height);

      if (S.raw_crop.cwidth)
      {
        printf("Raw crop, width x height: %4d x %d ", S.raw_crop.cwidth, S.raw_crop.cheight);
        if (S.raw_crop.cleft != 0xffff)
          printf("left: %d ", S.raw_crop.cleft);
        if (S.raw_crop.ctop != 0xffff)
          printf("top: %d", S.raw_crop.ctop);
        printf("\n");
      }

      printf("Image size:  %4d x %d\n", S.width, S.height);
      printf("Output size: %4d x %d\n", S.iwidth, S.iheight);

      if (Canon.SensorWidth)
        printf("SensorWidth          = %d\n", Canon.SensorWidth);
      if (Canon.SensorHeight)
        printf("SensorHeight         = %d\n", Canon.SensorHeight);
      if (Canon.SensorLeftBorder)
        printf("SensorLeftBorder     = %d\n", Canon.SensorLeftBorder);
      if (Canon.SensorTopBorder)
        printf("SensorTopBorder      = %d\n", Canon.SensorTopBorder);
      if (Canon.SensorRightBorder)
        printf("SensorRightBorder    = %d\n", Canon.SensorRightBorder);
      if (Canon.SensorBottomBorder)
        printf("SensorBottomBorder   = %d\n", Canon.SensorBottomBorder);
      if (Canon.BlackMaskLeftBorder)
        printf("BlackMaskLeftBorder  = %d\n", Canon.BlackMaskLeftBorder);
      if (Canon.BlackMaskTopBorder)
        printf("BlackMaskTopBorder   = %d\n", Canon.BlackMaskTopBorder);
      if (Canon.BlackMaskRightBorder)
        printf("BlackMaskRightBorder = %d\n", Canon.BlackMaskRightBorder);
      if (Canon.BlackMaskBottomBorder)
        printf("BlackMaskBottomBorder= %d\n", Canon.BlackMaskBottomBorder);
      if (Canon.ChannelBlackLevel[0])
        printf("ChannelBlackLevel (from makernotes): %d %d %d %d\n", Canon.ChannelBlackLevel[0],
               Canon.ChannelBlackLevel[1], Canon.ChannelBlackLevel[2], Canon.ChannelBlackLevel[3]);

      if (Hasselblad.BaseISO)
        printf("Hasselblad base ISO: %d\n", Hasselblad.BaseISO);
      if (Hasselblad.Gain)
        printf("Hasselblad gain: %g\n", Hasselblad.Gain);

      if (Oly.OlympusCropID != -1)
      {
        printf("Olympus aspect ID: %d\nOlympus crop", Oly.OlympusCropID);
        for (int c = 0; c < 4; c++)
          printf(" %d", Oly.OlympusFrame[c]);
        printf("\n");
      }

      printf("Raw colors: %d", P1.colors);
      if (P1.filters)
      {
        printf("\nFilter pattern: ");
        if (!P1.cdesc[3])
          P1.cdesc[3] = 'G';
        for (int i = 0; i < 16; i++)
          putchar(P1.cdesc[MyCoolRawProcessor.fcol(i >> 1, i & 1)]);
      }
      if (C.linear_max[0] != 0)
      {
        printf("\nHighlight linearity limits:");
        for (int c = 0; c < 4; c++)
          printf(" %ld", C.linear_max[c]);
      }
      if (C.cam_mul[0] > 0)
      {
        printf("\nMakernotes 'As shot' WB multipliers:");
        for (int c = 0; c < 4; c++)
          printf(" %f", C.cam_mul[c]);
      }

      for (int cnt = 0; cnt < 25; cnt++)
      {
        if (C.WB_Coeffs[cnt][0] > 0)
        {
          printf("\nMakernotes '%s' WB multipliers:", EXIF_LightSources[cnt]);
          for (int c = 0; c < 4; c++)
            printf(" %d", C.WB_Coeffs[cnt][c]);
        }
      }
      if (C.WB_Coeffs[LIBRAW_WBI_Sunset][0] > 0)
      {
        printf("\nMakernotes 'Sunset' multipliers:");
        for (int c = 0; c < 4; c++)
          printf(" %d", C.WB_Coeffs[LIBRAW_WBI_Sunset][c]);
      }

      if (C.WB_Coeffs[LIBRAW_WBI_Other][0] > 0)
      {
        printf("\nMakernotes 'Other' multipliers:");
        for (int c = 0; c < 4; c++)
          printf(" %d", C.WB_Coeffs[LIBRAW_WBI_Other][c]);
      }

      if (C.WB_Coeffs[LIBRAW_WBI_Auto][0] > 0)
      {
        printf("\nMakernotes 'Camera Auto' WB multipliers:");
        for (int c = 0; c < 4; c++)
          printf(" %d", C.WB_Coeffs[LIBRAW_WBI_Auto][c]);
      }
      if (C.WB_Coeffs[LIBRAW_WBI_Measured][0] > 0)
      {
        printf("\nMakernotes 'Camera Measured' WB multipliers:");
        for (int c = 0; c < 4; c++)
          printf(" %d", C.WB_Coeffs[LIBRAW_WBI_Measured][c]);
      }

      if (C.WB_Coeffs[LIBRAW_WBI_Custom][0] > 0)
      {
        printf("\nMakernotes 'Custom' WB multipliers:");
        for (int c = 0; c < 4; c++)
          printf(" %d", C.WB_Coeffs[LIBRAW_WBI_Custom][c]);
      }
      if (C.WB_Coeffs[LIBRAW_WBI_Custom1][0] > 0)
      {
        printf("\nMakernotes 'Custom1' WB multipliers:");
        for (int c = 0; c < 4; c++)
          printf(" %d", C.WB_Coeffs[LIBRAW_WBI_Custom1][c]);
      }
      if (C.WB_Coeffs[LIBRAW_WBI_Custom2][0] > 0)
      {
        printf("\nMakernotes 'Custom2' WB multipliers:");
        for (int c = 0; c < 4; c++)
          printf(" %d", C.WB_Coeffs[LIBRAW_WBI_Custom2][c]);
      }
      if (C.WB_Coeffs[LIBRAW_WBI_Custom3][0] > 0)
      {
        printf("\nMakernotes 'Custom3' WB multipliers:");
        for (int c = 0; c < 4; c++)
          printf(" %d", C.WB_Coeffs[LIBRAW_WBI_Custom3][c]);
      }
      if (C.WB_Coeffs[LIBRAW_WBI_Custom4][0] > 0)
      {
        printf("\nMakernotes 'Custom4' WB multipliers:");
        for (int c = 0; c < 4; c++)
          printf(" %d", C.WB_Coeffs[LIBRAW_WBI_Custom4][c]);
      }
      if (C.WB_Coeffs[LIBRAW_WBI_Custom5][0] > 0)
      {
        printf("\nMakernotes 'Custom5' WB multipliers:");
        for (int c = 0; c < 4; c++)
          printf(" %d", C.WB_Coeffs[LIBRAW_WBI_Custom5][c]);
      }
      if (C.WB_Coeffs[LIBRAW_WBI_Custom6][0] > 0)
      {
        printf("\nMakernotes 'Custom6' WB multipliers:");
        for (int c = 0; c < 4; c++)
          printf(" %d", C.WB_Coeffs[LIBRAW_WBI_Custom6][c]);
      }

      if ((Nikon.ME_WB[0] != 0.0f) && (Nikon.ME_WB[0] != 1.0f))
      {
        printf("\nNikon multi-exposure WB multipliers:");
        for (int c = 0; c < 4; c++)
          printf(" %f", Nikon.ME_WB[c]);
      }

      if (C.rgb_cam[0][0] > 0.0001 && P1.colors > 1)
      {
        printf("\nCamera2RGB matrix:\n");
        for (int i = 0; i < P1.colors; i++)
          printf("%6.4f\t%6.4f\t%6.4f\n", C.rgb_cam[i][0], C.rgb_cam[i][1], C.rgb_cam[i][2]);
      }
      printf("\nXYZ->CamRGB matrix:\n");
      for (int i = 0; i < P1.colors; i++)
        printf("%6.4f\t%6.4f\t%6.4f\n", C.cam_xyz[i][0], C.cam_xyz[i][1], C.cam_xyz[i][2]);

      if (C.dng_color[0].illuminant < 0xffff)
        printf("\nDNG Illuminant 1: %s", EXIF_LightSources[C.dng_color[0].illuminant]);
      if (C.dng_color[1].illuminant < 0xffff)
        printf("\nDNG Illuminant 2: %s", EXIF_LightSources[C.dng_color[1].illuminant]);

      if (fabsf(C.P1_color[0].romm_cam[0]) > 0)
      {
        printf("\nPhaseOne Matrix1:\n");
        for (int i = 0; i < 3; i++)
          printf("%6.4f\t%6.4f\t%6.4f\n", C.P1_color[0].romm_cam[i * 3], C.P1_color[0].romm_cam[i * 3 + 1],
                 C.P1_color[0].romm_cam[i * 3 + 2]);
      }

      if (fabsf(C.P1_color[1].romm_cam[0]) > 0)
      {
        printf("\nPhaseOne Matrix2:\n");
        for (int i = 0; i < 3; i++)
          printf("%6.4f\t%6.4f\t%6.4f\n", C.P1_color[1].romm_cam[i * 3], C.P1_color[1].romm_cam[i * 3 + 1],
                 C.P1_color[1].romm_cam[i * 3 + 2]);
      }

      if (fabsf(C.cmatrix[0][0]) > 0)
      {
        printf("\ncamRGB -> sRGB Matrix:\n");
        for (int i = 0; i < P1.colors; i++)
        {
          for (int j = 0; j < P1.colors; j++)
            printf("%6.4f\t", C.cmatrix[i][j]);
          printf("\n");
        }
      }

      if (fabsf(C.ccm[0][0]) > 0)
      {
        printf("\nColor Correction Matrix:\n");
        for (int i = 0; i < P1.colors; i++)
        {
          for (int j = 0; j < P1.colors; j++)
            printf("%6.4f\t", C.ccm[i][j]);
          printf("\n");
        }
      }
      if (fabsf(C.dng_color[0].colormatrix[0][0]) > 0)
      {
        printf("\nDNG color matrix 1:\n");
        for (int i = 0; i < P1.colors; i++)
          printf("%6.4f\t%6.4f\t%6.4f\n", C.dng_color[0].colormatrix[i][0], C.dng_color[0].colormatrix[i][1],
                 C.dng_color[0].colormatrix[i][2]);
      }
      if (fabsf(C.dng_color[1].colormatrix[0][0]) > 0)
      {
        printf("\nDNG color matrix 2:\n");
        for (int i = 0; i < P1.colors; i++)
          printf("%6.4f\t%6.4f\t%6.4f\n", C.dng_color[1].colormatrix[i][0], C.dng_color[1].colormatrix[i][1],
                 C.dng_color[1].colormatrix[i][2]);
      }

      if (fabsf(C.dng_color[0].calibration[0][0]) > 0)
      {
        printf("\nDNG calibration matrix 1:\n");
        for (int i = 0; i < P1.colors; i++)
        {
          for (int j = 0; j < P1.colors; j++)
            printf("%6.4f\t", C.dng_color[0].calibration[j][i]);
          printf("\n");
        }
      }
      if (fabsf(C.dng_color[1].calibration[0][0]) > 0)
      {
        printf("\nDNG calibration matrix 2:\n");
        for (int i = 0; i < P1.colors; i++)
        {
          for (int j = 0; j < P1.colors; j++)
            printf("%6.4f\t", C.dng_color[1].calibration[j][i]);
          printf("\n");
        }
      }

      if (fabsf(C.dng_color[0].forwardmatrix[0][0]) > 0)
      {
        printf("\nDNG forward matrix 1:\n");
        for (int i = 0; i < P1.colors; i++)
          printf("%6.4f\t%6.4f\t%6.4f\n", C.dng_color[0].forwardmatrix[0][i], C.dng_color[0].forwardmatrix[1][i],
                 C.dng_color[0].forwardmatrix[2][i]);
      }
      if (fabsf(C.dng_color[1].forwardmatrix[0][0]) > 0)
      {
        printf("\nDNG forward matrix 2:\n");
        for (int i = 0; i < P1.colors; i++)
          printf("%6.4f\t%6.4f\t%6.4f\n", C.dng_color[1].forwardmatrix[0][i], C.dng_color[1].forwardmatrix[1][i],
                 C.dng_color[1].forwardmatrix[2][i]);
      }

      printf("\nDerived D65 multipliers:");
      for (int c = 0; c < P1.colors; c++)
        printf(" %f", C.pre_mul[c]);
      printf("\n");

      if (Sony.PixelShiftGroupID) {
        printf("\nSony PixelShiftGroupPrefix 0x%x PixelShiftGroupID %d, ",
                Sony.PixelShiftGroupPrefix, Sony.PixelShiftGroupID);
        if (Sony.numInPixelShiftGroup) {
             printf("shot# %d (starts at 1) of total %d\n", Sony.numInPixelShiftGroup, Sony.nShotsInPixelShiftGroup);
        } else {
           printf("shots in PixelShiftGroup %d, already ARQ\n", Sony.nShotsInPixelShiftGroup);
        }
      }

      if (Sony.Sony0x9400_version)
        printf("\nSONY Sequence data, tag 0x9400 version %x\n\
\tReleaseMode2: %d\n\
\tSequenceImageNumber: %d (starts at zero)\n\
\tSequenceLength1: %d shot(s)\n\
\tSequenceFileNumber: %d (starts at zero, exiftool starts at 1)\n\
\tSequenceLength2: %d file(s)\n",
               Sony.Sony0x9400_version, Sony.Sony0x9400_ReleaseMode2, Sony.Sony0x9400_SequenceImageNumber,
               Sony.Sony0x9400_SequenceLength1, Sony.Sony0x9400_SequenceFileNumber, Sony.Sony0x9400_SequenceLength2);
    }
    else
    {
      if (print_unpack)
      {
        char frame[48] = "";
        if (print_frame)
        {
          ushort right_margin = S.raw_width - S.width - S.left_margin;
          ushort bottom_margin = S.raw_height - S.height - S.top_margin;
          snprintf(frame, 48, "F=%dx%dx%dx%d RS=%dx%d", S.left_margin, S.top_margin, right_margin, bottom_margin,
                   S.raw_width, S.raw_height);
          printf("%s\t%s\t%s\t%s/%s\n", av[i], MyCoolRawProcessor.unpack_function_name(), frame, P1.make, P1.model);
        }
      }
      else if (print_wb)
      {
        printf("// %s %s\n", P1.make, P1.model);
        for (int cnt = 0; cnt < 25; cnt++)
          if (C.WB_Coeffs[cnt][0])
          {
            printf("{\"%s\", \"%s\", %d, {%6.5ff, 1.0f, %6.5ff, ", P1.make, P1.model, cnt,
                   C.WB_Coeffs[cnt][0] / (float)C.WB_Coeffs[cnt][1], C.WB_Coeffs[cnt][2] / (float)C.WB_Coeffs[cnt][1]);
            if (C.WB_Coeffs[cnt][1] == C.WB_Coeffs[cnt][3])
              printf("1.0f}},\n");
            else
              printf("%6.5ff}},\n", C.WB_Coeffs[cnt][3] / (float)C.WB_Coeffs[cnt][1]);
          }
        if (C.WB_Coeffs[LIBRAW_WBI_Sunset][0])
        {
          printf("{\"%s\", \"%s\", %d, {%6.5ff, 1.0f, %6.5ff, ", P1.make, P1.model, LIBRAW_WBI_Sunset,
                 C.WB_Coeffs[LIBRAW_WBI_Sunset][0] / (float)C.WB_Coeffs[LIBRAW_WBI_Sunset][1],
                 C.WB_Coeffs[LIBRAW_WBI_Sunset][2] / (float)C.WB_Coeffs[LIBRAW_WBI_Sunset][1]);
          if (C.WB_Coeffs[LIBRAW_WBI_Sunset][1] == C.WB_Coeffs[LIBRAW_WBI_Sunset][3])
            printf("1.0f}},\n");
          else
            printf("%6.5ff}},\n", C.WB_Coeffs[LIBRAW_WBI_Sunset][3] / (float)C.WB_Coeffs[LIBRAW_WBI_Sunset][1]);
        }

        for (int cnt = 0; cnt < 64; cnt++)
          if (C.WBCT_Coeffs[cnt][0])
          {
            printf("{\"%s\", \"%s\", %d, {%6.5ff, 1.0f, %6.5ff, ", P1.make, P1.model, (int)C.WBCT_Coeffs[cnt][0],
                   C.WBCT_Coeffs[cnt][1] / C.WBCT_Coeffs[cnt][2], C.WBCT_Coeffs[cnt][3] / C.WBCT_Coeffs[cnt][2]);
            if (C.WBCT_Coeffs[cnt][2] == C.WBCT_Coeffs[cnt][4])
              printf("1.0f}},\n");
            else
              printf("%6.5ff}},\n", C.WBCT_Coeffs[cnt][4] / C.WBCT_Coeffs[cnt][2]);
          }
          else
            break;
        printf("\n");
      }
      else if (compact)
      {
        trimSpaces(P1.make);
        trimSpaces(P1.model);
        trimSpaces(C.model2);
        trimSpaces(ShootingInfo.BodySerial);
        trimSpaces(ShootingInfo.InternalBodySerial);
        printf("%s=%s", P1.make, P1.model);
        if (ShootingInfo.BodySerial[0] && !(ShootingInfo.BodySerial[0] == 48 && !ShootingInfo.BodySerial[1]))
          printf("=Body#: %s", ShootingInfo.BodySerial);
        else if (C.model2[0] && (!strncasecmp(P1.make, "Kodak", 5) || !strcmp(P1.model, "EOS D2000C")))
          printf("=Body#: %s", C.model2);
        if (ShootingInfo.InternalBodySerial[0])
          printf("=Assy#: %s", ShootingInfo.InternalBodySerial);
        if (exifLens.LensSerial[0])
          printf("=Lens#: %s", exifLens.LensSerial);
        if (exifLens.InternalLensSerial[0])
          printf("=LensAssy#: %s", exifLens.InternalLensSerial);
        printf("=\n");
      }
      else
        printf("%s is a %s %s image.\n", av[i], P1.make, P1.model);
    }

    MyCoolRawProcessor.recycle();
  } // endfor

  return 0;
}
