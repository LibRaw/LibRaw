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

static ushort saneSonyCameraInfo(uchar a, uchar b, uchar c, uchar d, uchar e,
                                 uchar f)
{
  if ((a >> 4) > 9)
    return 0;
  else if ((a & 0x0f) > 9)
    return 0;
  else if ((b >> 4) > 9)
    return 0;
  else if ((b & 0x0f) > 9)
    return 0;
  else if ((c >> 4) > 9)
    return 0;
  else if ((c & 0x0f) > 9)
    return 0;
  else if ((d >> 4) > 9)
    return 0;
  else if ((d & 0x0f) > 9)
    return 0;
  else if ((e >> 4) > 9)
    return 0;
  else if ((e & 0x0f) > 9)
    return 0;
  else if ((f >> 4) > 9)
    return 0;
  else if ((f & 0x0f) > 9)
    return 0;
  return 1;
}
static float my_roundf(float x)
{
  float t;
  if (x >= 0.0)
  {
    t = ceilf(x);
    if (t - x > 0.5)
      t -= 1.0;
    return t;
  }
  else
  {
    t = ceilf(-x);
    if (t + x > 0.5)
      t -= 1.0;
    return -t;
  }
}

static ushort bcd2dec(uchar data)
{
  if ((data >> 4) > 9)
    return 0;
  else if ((data & 0x0f) > 9)
    return 0;
  else
    return (data >> 4) * 10 + (data & 0x0f);
}

static uchar SonySubstitution[257] =
    "\x00\x01\x32\xb1\x0a\x0e\x87\x28\x02\xcc\xca\xad\x1b\xdc\x08\xed\x64\x86"
    "\xf0\x4f\x8c\x6c\xb8\xcb\x69\xc4\x2c\x03"
    "\x97\xb6\x93\x7c\x14\xf3\xe2\x3e\x30\x8e\xd7\x60\x1c\xa1\xab\x37\xec\x75"
    "\xbe\x23\x15\x6a\x59\x3f\xd0\xb9\x96\xb5"
    "\x50\x27\x88\xe3\x81\x94\xe0\xc0\x04\x5c\xc6\xe8\x5f\x4b\x70\x38\x9f\x82"
    "\x80\x51\x2b\xc5\x45\x49\x9b\x21\x52\x53"
    "\x54\x85\x0b\x5d\x61\xda\x7b\x55\x26\x24\x07\x6e\x36\x5b\x47\xb7\xd9\x4a"
    "\xa2\xdf\xbf\x12\x25\xbc\x1e\x7f\x56\xea"
    "\x10\xe6\xcf\x67\x4d\x3c\x91\x83\xe1\x31\xb3\x6f\xf4\x05\x8a\x46\xc8\x18"
    "\x76\x68\xbd\xac\x92\x2a\x13\xe9\x0f\xa3"
    "\x7a\xdb\x3d\xd4\xe7\x3a\x1a\x57\xaf\x20\x42\xb2\x9e\xc3\x8b\xf2\xd5\xd3"
    "\xa4\x7e\x1f\x98\x9c\xee\x74\xa5\xa6\xa7"
    "\xd8\x5e\xb0\xb4\x34\xce\xa8\x79\x77\x5a\xc1\x89\xae\x9a\x11\x33\x9d\xf5"
    "\x39\x19\x65\x78\x16\x71\xd2\xa9\x44\x63"
    "\x40\x29\xba\xa0\x8f\xe4\xd6\x3b\x84\x0d\xc2\x4e\x58\xdd\x99\x22\x6b\xc9"
    "\xbb\x17\x06\xe5\x7d\x66\x43\x62\xf6\xcd"
    "\x35\x90\x2e\x41\x8d\x6d\xaa\x09\x73\x95\x0c\xf1\x1d\xde\x4c\x2f\x2d\xf7"
    "\xd1\x72\xeb\xef\x48\xc7\xf8\xf9\xfa\xfb"
    "\xfc\xfd\xfe\xff";

void LibRaw::sony_decrypt(unsigned *data, int len, int start, int key)
{
#ifndef LIBRAW_NOTHREADS
#define pad tls->sony_decrypt.pad
#define p tls->sony_decrypt.p
#else
  static unsigned pad[128], p;
#endif
  if (start)
  {
    for (p = 0; p < 4; p++)
      pad[p] = key = key * 48828125ULL + 1;
    pad[3] = pad[3] << 1 | (pad[0] ^ pad[2]) >> 31;
    for (p = 4; p < 127; p++)
      pad[p] = (pad[p - 4] ^ pad[p - 2]) << 1 | (pad[p - 3] ^ pad[p - 1]) >> 31;
    for (p = 0; p < 127; p++)
      pad[p] = htonl(pad[p]);
  }
  while (len--)
  {
    *data++ ^= pad[p & 127] = pad[(p + 1) & 127] ^ pad[(p + 65) & 127];
    p++;
  }
#ifndef LIBRAW_NOTHREADS
#undef pad
#undef p
#endif
}
void LibRaw::setSonyBodyFeatures(unsigned long long id)
{
  ushort idx;
  static const struct
  {
    ushort scf[11];
    /*
    scf[0]  camera id
    scf[1]  camera format
    scf[2]  camera mount: Minolta A, Sony E, fixed,
    scf[3]  camera type: DSLR, NEX, SLT, ILCE, ILCA, DSC
    scf[4]  lens mount
    scf[5]  tag 0x2010 group (0 if not used)
    scf[6]  offset of Sony ISO in 0x2010 table, 0xffff if not valid
    scf[7]  offset of ShutterCount3 in 0x9050 table, 0xffff if not valid
    scf[8]  offset of MeteringMode in 0x2010 table, 0xffff if not valid
    scf[9]  offset of ExposureProgram in 0x2010 table, 0xffff if not valid
    scf[10] offset of ReleaseMode2 in 0x2010 table, 0xffff if not valid
    */
  } SonyCamFeatures[] = {
      {SonyID_DSLR_A100, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0,
       0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_DSLR_A900, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0, 0,
       0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_DSLR_A700, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0,
       0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_DSLR_A200, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0,
       0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_DSLR_A350, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0,
       0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_DSLR_A300, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0,
       0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_DSLR_A900, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0,
       0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_DSLR_A380, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0,
       0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_DSLR_A330, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0,
       0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_DSLR_A230, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0,
       0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_DSLR_A290, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0,
       0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x10b, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x10c, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_DSLR_A850, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0, 0,
       0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_DSLR_A850, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0,
       0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x10f, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x110, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_DSLR_A550, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0,
       0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_DSLR_A500, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0,
       0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_DSLR_A450, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0,
       0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x114, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x115, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_NEX_5, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 0,
       0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_NEX_3, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 0,
       0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_SLT_A33, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_SLT, 0, 0,
       0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_SLT_A55, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_SLT, 0, 0,
       0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_DSLR_A560, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0,
       0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_DSLR_A580, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0,
       0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_NEX_C3, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 0,
       0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_SLT_A35, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_SLT, 0, 0,
       0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_SLT_A65, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_SLT, 0, 2,
       0x1218, 0x01bd, 0x1178, 0x1179, 0x112c},
      {SonyID_SLT_A77, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_SLT, 0, 2,
       0x1218, 0x01bd, 0x1178, 0x1179, 0x112c},
      {SonyID_NEX_5N, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 1,
       0x113e, 0x01bd, 0x1174, 0x1175, 0x112c},
      {SonyID_NEX_7, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 2,
       0x1218, 0x01bd, 0x1178, 0x1179, 0x112c},
      {SonyID_NEX_VG20, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 2,
       0x1218, 0x01bd, 0x1178, 0x1179, 0x112c},
      {SonyID_SLT_A37, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_SLT, 0, 3,
       0x11f4, 0x01bd, 0x1154, 0x1155, 0x1108},
      {SonyID_SLT_A57, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_SLT, 0, 3,
       0x11f4, 0x01bd, 0x1154, 0x1155, 0x1108},
      {SonyID_NEX_F3, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 3,
       0x11f4, 0x01bd, 0x1154, 0x1155, 0x1108},
      {SonyID_SLT_A99, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_SLT, 0, 5,
       0x1254, 0x01aa, 0x11ac, 0x11ad, 0x1160},
      {SonyID_NEX_6, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 5,
       0x1254, 0x01aa, 0x11ac, 0x11ad, 0x1160},
      {SonyID_NEX_5R, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 5,
       0x1254, 0x01aa, 0x11ac, 0x11ad, 0x1160},
      {SonyID_DSC_RX100, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC,
       LIBRAW_MOUNT_FixedLens, 5, 0x1254, 0xffff, 0x11ac, 0x11ad, 0x1160},
      {SonyID_DSC_RX1, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC,
       LIBRAW_MOUNT_FixedLens, 5, 0x1258, 0xffff, 0x11ac, 0x11ad, 0x1160},
      {SonyID_NEX_VG900, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 5,
       0x1254, 0x01aa, 0x11ac, 0x11ad, 0x1160},
      {SonyID_NEX_VG30, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 5,
       0x1254, 0x01aa, 0x11ac, 0x11ad, 0x1160},
      {0x12d, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_ILCE_3000, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 5,
       0x1280, 0x01aa, 0x11ac, 0x11ad, 0x1160},
      {SonyID_SLT_A58, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_SLT, 0, 5,
       0x1280, 0x01aa, 0x11ac, 0x11ad, 0x1160},
      {0x130, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_NEX_3N, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 5,
       0x1280, 0x01aa, 0x11ac, 0x11ad, 0x1160},
      {SonyID_ILCE_7, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 7,
       0x0344, 0xffff, 0x025c, 0x025d, 0x0210},
      {SonyID_NEX_5T, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 5,
       0x1254, 0x01aa, 0x11ac, 0x11ad, 0x1160},
      {SonyID_DSC_RX100M2, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC,
       LIBRAW_MOUNT_FixedLens, 6, 0x113c, 0xffff, 0x1064, 0x1065, 0x1018},
      {SonyID_DSC_RX10, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC,
       LIBRAW_MOUNT_FixedLens, 7, 0x0344, 0xffff, 0x025c, 0x025d, 0x0210},
      {SonyID_DSC_RX1R, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC,
       LIBRAW_MOUNT_FixedLens, 5, 0x1258, 0xffff, 0x11ac, 0x11ad, 0x1160},
      {SonyID_ILCE_7R, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 7,
       0x0344, 0xffff, 0x025c, 0x025d, 0x0210},
      {SonyID_ILCE_6000, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 7,
       0x0344, 0xffff, 0x025c, 0x025d, 0x0210},
      {SonyID_ILCE_5000, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 7,
       0x0344, 0x01aa, 0x025c, 0x025d, 0x0210},
      {0x13a, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x13b, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x13c, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_DSC_RX100M3, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC,
       LIBRAW_MOUNT_FixedLens, 7, 0x0344, 0xffff, 0x025c, 0x025d, 0x0210},
      {SonyID_ILCE_7S, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 7,
       0x0344, 0xffff, 0x025c, 0x025d, 0x0210},
      {SonyID_ILCA_77M2, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_ILCA, 0,
       7, 0x0344, 0x01a0, 0x025c, 0x025d, 0x0210},
      {0x140, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x141, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x142, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x143, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x144, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x145, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x146, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x147, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x148, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x149, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x14a, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x14b, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x14c, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x14d, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x14e, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x14f, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x150, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x151, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x152, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_ILCE_5100, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 7,
       0x0344, 0x01a0, 0x025c, 0x025d, 0x0210},
      {SonyID_ILCE_7M2, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 7,
       0x0344, 0xffff, 0x025c, 0x025d, 0x0210},
      {SonyID_DSC_RX100M4, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC,
       LIBRAW_MOUNT_FixedLens, 8, 0x0346, 0xffff, 0x025c, 0x025d, 0x0210},
      {SonyID_DSC_RX10M2, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC,
       LIBRAW_MOUNT_FixedLens, 8, 0x0346, 0xffff, 0x025c, 0x025d, 0x0210},
      {0x157, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_DSC_RX1RM2, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC,
       LIBRAW_MOUNT_FixedLens, 8, 0x0346, 0xffff, 0x025c, 0x025d, 0x0210},
      {0x159, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_ILCE_QX1, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 7,
       0x0344, 0x01a0, 0x025c, 0x025d, 0x0210},
      {SonyID_ILCE_7RM2, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 8,
       0x0346, 0x01cb, 0x025c, 0x025d, 0x0210},
      {0x15c, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x15d, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_ILCE_7SM2, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 8,
       0x0346, 0x01cb, 0x025c, 0x025d, 0x0210},
      {0x15f, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {0x160, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_ILCA_68, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_ILCA, 0,
       7, 0x0344, 0x01a0, 0x025c, 0x025d, 0x0210},
      {SonyID_ILCA_99M2, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_ILCA, 0, 8,
       0x0346, 0x01cd, 0x025c, 0x025d, 0x0210},
      {SonyID_DSC_RX10M3, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC,
       LIBRAW_MOUNT_FixedLens, 8, 0x0346, 0xffff, 0x025c, 0x025d, 0x0210},
      {SonyID_DSC_RX100M5, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC,
       LIBRAW_MOUNT_FixedLens, 8, 0x0346, 0xffff, 0x025c, 0x025d, 0x0210},
      {SonyID_ILCE_6300, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 8,
       0x0346, 0x01cd, 0x025c, 0x025d, 0x0210},
      {SonyID_ILCE_9, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 9,
       0x0320, 0x019f, 0x024b, 0x024c, 0x0208},
      {0x167, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_ILCE_6500, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 8,
       0x0346, 0x01cd, 0x025c, 0x025d, 0x0210},
      {0x169, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_ILCE_7RM3, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 9,
       0x0320, 0x019f, 0x024b, 0x024c, 0x0208},
      {SonyID_ILCE_7M3, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 9,
       0x0320, 0x019f, 0x024b, 0x024c, 0x0208},
      {SonyID_DSC_RX0, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC,
       LIBRAW_MOUNT_FixedLens, 8, 0x0346, 0xffff, 0x025c, 0x025d, 0x0210},
      {SonyID_DSC_RX10M4, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC,
       LIBRAW_MOUNT_FixedLens, 9, 0x0320, 0xffff, 0x024b, 0x024c, 0x0208},
      {SonyID_DSC_RX100M6, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC,
       LIBRAW_MOUNT_FixedLens, 9, 0x0320, 0xffff, 0x024b, 0x024c, 0x0208},
      {SonyID_DSC_HX99, LIBRAW_FORMAT_1div2p3INCH, LIBRAW_MOUNT_FixedLens,
       LIBRAW_SONY_DSC, LIBRAW_MOUNT_FixedLens, 9, 0x0320, 0xffff, 0x024b,
       0x024c, 0x0208},
      {0x170, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_DSC_RX100M5A, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC,
       LIBRAW_MOUNT_FixedLens, 9, 0x0320, 0xffff, 0x024b, 0x024c, 0x0208},
      {0x172, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_ILCE_6400, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 9,
       0x0320, 0x019f, 0x024b, 0x024c, 0x0208},
      {SonyID_DSC_RX0M2, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC,
       LIBRAW_MOUNT_FixedLens, 9, 0x0320, 0xffff, 0x024b, 0x024c, 0x0208},
      {0x175, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_DSC_RX100M7, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC,
       LIBRAW_MOUNT_FixedLens, 9, 0x0320, 0xffff, 0x024b, 0x024c, 0x0208},
      {SonyID_ILCE_7RM4, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 9,
       0x0320, 0x019f, 0x024b, 0x024c, 0x0208},
      {SonyID_ILCE_9M2, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 9,
       0x0320, 0x019f, 0x024b, 0x024c, 0x0208},
      {0x179, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {SonyID_ILCE_6600, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 9,
       0x0320, 0x019f, 0x024b, 0x024c, 0x0208},
      {SonyID_ILCE_6100, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 9,
       0x0320, 0x019f, 0x024b, 0x024c, 0x0208},

  };
  ilm.CamID = id;

  if (id == SonyID_DSC_R1)
  {
    ilm.CameraMount = ilm.LensMount = LIBRAW_MOUNT_FixedLens;
    imSony.CameraType = LIBRAW_SONY_DSC;
    imSony.group2010 = 0;
    imSony.real_iso_offset = 0xffff;
    imSony.ImageCount3_offset = 0xffff;
    return;
  }
  else
    idx = id - 0x100ULL;

  if ((idx >= 0) && (idx < sizeof SonyCamFeatures / sizeof *SonyCamFeatures))
  {
    if (!SonyCamFeatures[idx].scf[2])
      return;
    ilm.CameraFormat = SonyCamFeatures[idx].scf[1];
    ilm.CameraMount = SonyCamFeatures[idx].scf[2];
    imSony.CameraType = SonyCamFeatures[idx].scf[3];
    if (SonyCamFeatures[idx].scf[4])
      ilm.LensMount = SonyCamFeatures[idx].scf[4];
    imSony.group2010 = SonyCamFeatures[idx].scf[5];
    imSony.real_iso_offset = SonyCamFeatures[idx].scf[6];
    imSony.ImageCount3_offset = SonyCamFeatures[idx].scf[7];
    imSony.MeteringMode_offset = SonyCamFeatures[idx].scf[8];
    imSony.ExposureProgram_offset = SonyCamFeatures[idx].scf[9];
    imSony.ReleaseMode2_offset = SonyCamFeatures[idx].scf[10];
  }

  char *sbstr = strstr(software, " v");
  if (sbstr != NULL)
  {
    sbstr += 2;
    imSony.firmware = atof(sbstr);

    if ((id == SonyID_ILCE_7) ||
        (id == SonyID_ILCE_7R))
    {
      if (imSony.firmware < 1.2f)
        imSony.ImageCount3_offset = 0x01aa;
      else
        imSony.ImageCount3_offset = 0x01c0;
    }
    else if (id == SonyID_ILCE_6000)
    {
      if (imSony.firmware < 2.0f)
        imSony.ImageCount3_offset = 0x01aa;
      else
        imSony.ImageCount3_offset = 0x01c0;
    }
    else if ((id == SonyID_ILCE_7S) ||
             (id == SonyID_ILCE_7M2))
    {
      if (imSony.firmware < 1.2f)
        imSony.ImageCount3_offset = 0x01a0;
      else
        imSony.ImageCount3_offset = 0x01b6;
    }
  }
}

void LibRaw::parseSonyLensType2(uchar a, uchar b)
{
  ushort lid2;
  lid2 = (((ushort)a) << 8) | ((ushort)b);
//  printf ("==>> 2: lid2 %d\n", lid2);
  if (!lid2)
    return;
  if (lid2 < 0x100)
  {
    if ((ilm.AdapterID != 0x4900) && (ilm.AdapterID != 0xef00))
    {
      ilm.AdapterID = lid2;
      switch (lid2)
      {
      case 1:
      case 2:
      case 3:
      case 6:
        ilm.LensMount = LIBRAW_MOUNT_Minolta_A;
        break;
      case 44:
      case 78:
      case 184:
      case 234:
      case 239:
        ilm.LensMount = LIBRAW_MOUNT_Canon_EF;
        break;
      }
    }
  }
  else
    ilm.LensID = lid2;

  if ((lid2 >= 50481) &&
      (lid2 < 50500)) {
    strcpy(ilm.Adapter, "MC-11");
    ilm.AdapterID = 0x4900;
  } else if ((lid2 > 0xef00) &&
             (lid2 < 0xffff) &&
             (lid2 != 0xff00)) {
    ilm.AdapterID = 0xef00;
    ilm.LensID -= ilm.AdapterID;
    ilm.LensMount = LIBRAW_MOUNT_Canon_EF;
  }

  return;
}

void LibRaw::parseSonyLensFeatures(uchar a, uchar b)
{

  ushort features;
  features = (((ushort)a) << 8) | ((ushort)b);

  if ((ilm.LensMount == LIBRAW_MOUNT_Canon_EF) ||
      (ilm.LensMount != LIBRAW_MOUNT_Sigma_X3F) || !features)
    return;

  ilm.LensFeatures_pre[0] = 0;
  ilm.LensFeatures_suf[0] = 0;
  if ((features & 0x0200) && (features & 0x0100))
    strcpy(ilm.LensFeatures_pre, "E");
  else if (features & 0x0200)
    strcpy(ilm.LensFeatures_pre, "FE");
  else if (features & 0x0100)
    strcpy(ilm.LensFeatures_pre, "DT");

  if (!ilm.LensFormat && !ilm.LensMount)
  {
    ilm.LensFormat = LIBRAW_FORMAT_FF;
    ilm.LensMount = LIBRAW_MOUNT_Minolta_A;

    if ((features & 0x0200) && (features & 0x0100))
    {
      ilm.LensFormat = LIBRAW_FORMAT_APSC;
      ilm.LensMount = LIBRAW_MOUNT_Sony_E;
    }
    else if (features & 0x0200)
    {
      ilm.LensMount = LIBRAW_MOUNT_Sony_E;
    }
    else if (features & 0x0100)
    {
      ilm.LensFormat = LIBRAW_FORMAT_APSC;
    }
  }

  if (features & 0x4000)
    strnXcat(ilm.LensFeatures_pre, " PZ");

  if (features & 0x0008)
    strnXcat(ilm.LensFeatures_suf, " G");
  else if (features & 0x0004)
    strnXcat(ilm.LensFeatures_suf, " ZA");

  if ((features & 0x0020) && (features & 0x0040))
    strnXcat(ilm.LensFeatures_suf, " Macro");
  else if (features & 0x0020)
    strnXcat(ilm.LensFeatures_suf, " STF");
  else if (features & 0x0040)
    strnXcat(ilm.LensFeatures_suf, " Reflex");
  else if (features & 0x0080)
    strnXcat(ilm.LensFeatures_suf, " Fisheye");

  if (features & 0x0001)
    strnXcat(ilm.LensFeatures_suf, " SSM");
  else if (features & 0x0002)
    strnXcat(ilm.LensFeatures_suf, " SAM");

  if (features & 0x8000)
    strnXcat(ilm.LensFeatures_suf, " OSS");

  if (features & 0x2000)
    strnXcat(ilm.LensFeatures_suf, " LE");

  if (features & 0x0800)
    strnXcat(ilm.LensFeatures_suf, " II");

  if (ilm.LensFeatures_suf[0] == ' ')
    memmove(ilm.LensFeatures_suf, ilm.LensFeatures_suf + 1,
            strbuflen(ilm.LensFeatures_suf) - 1);

  return;
}

void LibRaw::process_Sony_0x0116(uchar *buf, ushort len, unsigned long long id)
{
  int i = 0;

  if (((id == SonyID_DSLR_A900)      ||
       (id == SonyID_DSLR_A900_APSC) ||
       (id == SonyID_DSLR_A850)      ||
       (id == SonyID_DSLR_A850_APSC)) &&
      (len >= 2))
    i = 1;
  else if ((id >= SonyID_DSLR_A550) && (len >= 3))
    i = 2;
  else
    return;

  imCommon.BatteryTemperature = (float)(buf[i] - 32) / 1.8f;
}

void LibRaw::process_Sony_0x2010(uchar *buf, ushort len)
{
  if (!imSony.group2010)
    return;

  if ((imSony.real_iso_offset != 0xffff) &&
      (len >= (imSony.real_iso_offset + 2)) && (imCommon.real_ISO < 0.1f))
  {
    uchar s[2];
    s[0] = SonySubstitution[buf[imSony.real_iso_offset]];
    s[1] = SonySubstitution[buf[imSony.real_iso_offset + 1]];
    imCommon.real_ISO =
        100.0f * libraw_powf64l(2.0f, (16 - ((float)sget2(s)) / 256.0f));
  }

  if (len >= (imSony.MeteringMode_offset + 2))
  {
    imgdata.shootinginfo.MeteringMode =
        SonySubstitution[buf[imSony.MeteringMode_offset]];
    imgdata.shootinginfo.ExposureProgram =
        SonySubstitution[buf[imSony.ExposureProgram_offset]];
  }

  if (len >= (imSony.ReleaseMode2_offset + 2))
  {
    imgdata.shootinginfo.DriveMode =
        SonySubstitution[buf[imSony.ReleaseMode2_offset]];
  }
}

void LibRaw::process_Sony_0x9050(uchar *buf, ushort len, unsigned long long id)
{
  ushort lid;
  uchar s[4];
  int c;

  if ((ilm.CameraMount != LIBRAW_MOUNT_Sony_E) &&
      (ilm.CameraMount != LIBRAW_MOUNT_FixedLens))
  {
    if (len < 2)
      return;
    if (buf[0])
      ilm.MaxAp4CurFocal =
        my_roundf(
          libraw_powf64l(2.0f, ((float)SonySubstitution[buf[0]] / 8.0 - 1.06f) / 2.0f) *
             10.0f) / 10.0f;

    if (buf[1])
      ilm.MinAp4CurFocal =
        my_roundf(
          libraw_powf64l(2.0f, ((float)SonySubstitution[buf[1]] / 8.0 - 1.06f) / 2.0f) *
             10.0f) / 10.0f;
  }

  if (ilm.CameraMount != LIBRAW_MOUNT_FixedLens)
  {
    if (len <= 0x106)
      return;
    if (buf[0x3d] | buf[0x3c])
    {
      lid = SonySubstitution[buf[0x3d]] << 8 | SonySubstitution[buf[0x3c]];
      ilm.CurAp = libraw_powf64l(2.0f, ((float)lid / 256.0f - 16.0f) / 2.0f);
    }
    if (buf[0x105] &&
        (ilm.LensMount != LIBRAW_MOUNT_Canon_EF) &&
        (ilm.LensMount != LIBRAW_MOUNT_Sigma_X3F)) {
      switch (SonySubstitution[buf[0x105]]) {
        case 1:
          ilm.LensMount = LIBRAW_MOUNT_Minolta_A;
          break;
        case 2:
          ilm.LensMount = LIBRAW_MOUNT_Sony_E;
        break;
      }
    }
    if (buf[0x106]) {
      switch (SonySubstitution[buf[0x106]]) {
        case 1:
          ilm.LensFormat = LIBRAW_FORMAT_APSC;
          break;
        case 2:
          ilm.LensFormat = LIBRAW_FORMAT_FF;
        break;
      }
    }
  }

  if (ilm.CameraMount == LIBRAW_MOUNT_Sony_E)
  {
    if (len <= 0x108)
      return;
    parseSonyLensType2(
        SonySubstitution[buf[0x0108]], // LensType2 - Sony lens ids
        SonySubstitution[buf[0x0107]]);
  }

  if (len <= 0x10a)
    return;
  if ((ilm.LensID == LIBRAW_LENS_NOT_SET) && (ilm.CameraMount == LIBRAW_MOUNT_Minolta_A) &&
      (buf[0x010a] | buf[0x0109]))
  {
    ilm.LensID = // LensType - Minolta/Sony lens ids
        SonySubstitution[buf[0x010a]] << 8 | SonySubstitution[buf[0x0109]];

    if ((ilm.LensID > 0x4900) && (ilm.LensID <= 0x5900))
    {
      ilm.AdapterID = 0x4900;
      ilm.LensID -= ilm.AdapterID;
      ilm.LensMount = LIBRAW_MOUNT_Sigma_X3F;
      strcpy(ilm.Adapter, "MC-11");
    }

    else if ((ilm.LensID > 0xef00) && (ilm.LensID < 0xffff) &&
             (ilm.LensID != 0xff00))
    {
      ilm.AdapterID = 0xef00;
      ilm.LensID -= ilm.AdapterID;
      ilm.LensMount = LIBRAW_MOUNT_Canon_EF;
    }
  }

  if ((id >= SonyID_SLT_A65) && (id <= SonyID_NEX_F3))
  {
    if (len <= 0x116)
      return;
    // "SLT-A65", "SLT-A77", "NEX-7", "NEX-VG20",
    // "SLT-A37", "SLT-A57", "NEX-F3", "Lunar"
    parseSonyLensFeatures(SonySubstitution[buf[0x115]],
                          SonySubstitution[buf[0x116]]);
  }
  else if (ilm.CameraMount != LIBRAW_MOUNT_FixedLens)
  {
    if (len <= 0x117)
      return;
    parseSonyLensFeatures(SonySubstitution[buf[0x116]],
                          SonySubstitution[buf[0x117]]);
  }

  if ((id == SonyID_ILCE_7RM2) ||
      (id == SonyID_ILCE_7SM2) ||
      (id == SonyID_ILCA_99M2) ||
      (id == SonyID_ILCE_6300) ||
      (id == SonyID_ILCE_9)    ||
      (id == SonyID_ILCE_6500) ||
      (id == SonyID_ILCE_7RM3) ||
      (id == SonyID_ILCE_7M3)  ||
      (id == SonyID_ILCE_6400) ||
      (id == SonyID_ILCE_7RM4) ||
      (id == SonyID_ILCE_9M2)  ||
      (id == SonyID_ILCE_6600) ||
      (id == SonyID_ILCE_6100))
  {
    if (len <= 0x8d)
      return;
    unsigned long long b88 = SonySubstitution[buf[0x88]];
    unsigned long long b89 = SonySubstitution[buf[0x89]];
    unsigned long long b8a = SonySubstitution[buf[0x8a]];
    unsigned long long b8b = SonySubstitution[buf[0x8b]];
    unsigned long long b8c = SonySubstitution[buf[0x8c]];
    unsigned long long b8d = SonySubstitution[buf[0x8d]];
    sprintf(imgdata.shootinginfo.InternalBodySerial, "%06llx",
            (b88 << 40) + (b89 << 32) + (b8a << 24) + (b8b << 16) + (b8c << 8) +
                b8d);
  }
  else if (ilm.CameraMount == LIBRAW_MOUNT_Minolta_A)
  {
    if (len <= 0xf4)
      return;
    unsigned long long bf0 = SonySubstitution[buf[0xf0]];
    unsigned long long bf1 = SonySubstitution[buf[0xf1]];
    unsigned long long bf2 = SonySubstitution[buf[0xf2]];
    unsigned long long bf3 = SonySubstitution[buf[0xf3]];
    unsigned long long bf4 = SonySubstitution[buf[0xf4]];
    sprintf(imgdata.shootinginfo.InternalBodySerial, "%05llx",
            (bf0 << 32) + (bf1 << 24) + (bf2 << 16) + (bf3 << 8) + bf4);
  }
  else if ((ilm.CameraMount == LIBRAW_MOUNT_Sony_E) &&
           (id != SonyID_NEX_5N) &&
           (id != SonyID_NEX_7)  &&
           (id != SonyID_NEX_VG20))
  {
    if (len <= 0x7f)
      return;
    unsigned b7c = SonySubstitution[buf[0x7c]];
    unsigned b7d = SonySubstitution[buf[0x7d]];
    unsigned b7e = SonySubstitution[buf[0x7e]];
    unsigned b7f = SonySubstitution[buf[0x7f]];
    sprintf(imgdata.shootinginfo.InternalBodySerial, "%04x",
            (b7c << 24) + (b7d << 16) + (b7e << 8) + b7f);
  }

  if ((imSony.ImageCount3_offset != 0xffff) &&
      (len >= (imSony.ImageCount3_offset + 4)))
  {
    FORC4 s[c] = SonySubstitution[buf[imSony.ImageCount3_offset + c]];
    imSony.ImageCount3 = sget4(s);
  }

  return;
}

void LibRaw::process_Sony_0x9400(uchar *buf, ushort len, unsigned long long id)
{

  uchar s[4];
  int c;
  uchar bufx = buf[0];

  if (((bufx == 0x23) || (bufx == 0x24) || (bufx == 0x26) || (bufx == 0x28)) &&
      (len >= 0x1f))
  { // 0x9400 'c' version

    if ((id == SonyID_ILCE_9)       ||
        (id == SonyID_ILCE_7RM3)    ||
        (id == SonyID_ILCE_7M3)     ||
        (id == SonyID_DSC_RX10M4)   ||
        (id == SonyID_DSC_RX100M6)  ||
        (id == SonyID_DSC_HX99)     ||
        (id == SonyID_DSC_RX100M5A) ||
        (id == SonyID_ILCE_6400)    ||
        (id == SonyID_DSC_RX0M2)    ||
        (id == SonyID_DSC_RX100M7)  ||
        (id == SonyID_ILCE_7RM4)    ||
        (id == SonyID_ILCE_9M2)     ||
        (id == SonyID_ILCE_6600)    ||
        (id == SonyID_ILCE_6100))
    {
      imSony.ShotNumberSincePowerUp = SonySubstitution[buf[0x0a]];
    }
    else
    {
      FORC4 s[c] = SonySubstitution[buf[0x0a + c]];
      imSony.ShotNumberSincePowerUp = sget4(s);
    }

    imSony.Sony0x9400_version = 0xc;

    imSony.Sony0x9400_ReleaseMode2 = SonySubstitution[buf[0x09]];

    FORC4 s[c] = SonySubstitution[buf[0x12 + c]];
    imSony.Sony0x9400_SequenceImageNumber = sget4(s);

    imSony.Sony0x9400_SequenceLength1 = SonySubstitution[buf[0x16]]; // shots

    FORC4 s[c] = SonySubstitution[buf[0x1a + c]];
    imSony.Sony0x9400_SequenceFileNumber = sget4(s);

    imSony.Sony0x9400_SequenceLength2 = SonySubstitution[buf[0x1e]]; // files
  }

  else if ((bufx == 0x0c) && (len >= 0x1f))
  { // 0x9400 'b' version
    imSony.Sony0x9400_version = 0xb;

    FORC4 s[c] = SonySubstitution[buf[0x08 + c]];
    imSony.Sony0x9400_SequenceImageNumber = sget4(s);

    FORC4 s[c] = SonySubstitution[buf[0x0c + c]];
    imSony.Sony0x9400_SequenceFileNumber = sget4(s);

    imSony.Sony0x9400_ReleaseMode2 = SonySubstitution[buf[0x10]];

    imSony.Sony0x9400_SequenceLength1 = SonySubstitution[buf[0x1e]];
  }

  else if ((bufx == 0x0a) && (len >= 0x23))
  { // 0x9400 'a' version
    imSony.Sony0x9400_version = 0xa;

    FORC4 s[c] = SonySubstitution[buf[0x08 + c]];
    imSony.Sony0x9400_SequenceImageNumber = sget4(s);

    FORC4 s[c] = SonySubstitution[buf[0x0c + c]];
    imSony.Sony0x9400_SequenceFileNumber = sget4(s);

    imSony.Sony0x9400_ReleaseMode2 = SonySubstitution[buf[0x10]];

    imSony.Sony0x9400_SequenceLength1 = SonySubstitution[buf[0x22]];
  }

  else
    return;
}

void LibRaw::process_Sony_0x9402(uchar *buf, ushort len)
{

  if (len < 23)
    return;

  imgdata.shootinginfo.FocusMode = SonySubstitution[buf[0x16]];

  if ((imSony.CameraType == LIBRAW_SONY_SLT) ||
      (imSony.CameraType == LIBRAW_SONY_ILCA))
    return;

  uchar bufx = buf[0x00];
  if ((bufx == 0x05) || (bufx == 0xff) || (buf[0x02] != 0xff))
    return;

  imCommon.AmbientTemperature =
      (float)((short)SonySubstitution[buf[0x04]]);

  return;
}

void LibRaw::process_Sony_0x9403(uchar *buf, ushort len)
{
  if (len < 6)
    return;
  uchar bufx = SonySubstitution[buf[4]];
  if ((bufx == 0x00) || (bufx == 0x94))
    return;

  imCommon.SensorTemperature = (float)((short)SonySubstitution[buf[5]]);

  return;
}

void LibRaw::process_Sony_0x9406(uchar *buf, ushort len)
{
  if (len < 6)
    return;
  uchar bufx = buf[0];
  if ((bufx != 0x01) && (bufx != 0x08) && (bufx != 0x1b))
    return;
  bufx = buf[2];
  if ((bufx != 0x08) && (bufx != 0x1b))
    return;

  imCommon.BatteryTemperature =
      (float)(SonySubstitution[buf[5]] - 32) / 1.8f;

  return;
}

void LibRaw::process_Sony_0x940c(uchar *buf, ushort len)
{
  if ((imSony.CameraType != LIBRAW_SONY_ILCE) &&
      (imSony.CameraType != LIBRAW_SONY_NEX))
    return;
  if (len <= 0x000a)
    return;

  ushort lid2;
  if ((ilm.LensMount != LIBRAW_MOUNT_Canon_EF) &&
      (ilm.LensMount != LIBRAW_MOUNT_Sigma_X3F))
  {
    switch (SonySubstitution[buf[0x0008]])
    {
    case 1:
    case 5:
      ilm.LensMount = LIBRAW_MOUNT_Minolta_A;
      break;
    case 4:
      ilm.LensMount = LIBRAW_MOUNT_Sony_E;
      break;
    }
  }
  lid2 = (((ushort)SonySubstitution[buf[0x000a]]) << 8) |
         ((ushort)SonySubstitution[buf[0x0009]]);
  if ((lid2 > 0) &&
      ((lid2 < 32784) || (ilm.LensID == 0x1999) || (ilm.LensID == 0xffff)))
    parseSonyLensType2(
        SonySubstitution[buf[0x000a]], // LensType2 - Sony lens ids
        SonySubstitution[buf[0x0009]]);
  if ((lid2 == 44) || (lid2 == 78) || (lid2 == 184) || (lid2 == 234) ||
      (lid2 == 239))
    ilm.AdapterID = lid2;
  return;
}

void LibRaw::process_Sony_0x940e(uchar *buf, ushort len, unsigned long long id)
{
  if (((imSony.CameraType != LIBRAW_SONY_SLT) &&
       (imSony.CameraType != LIBRAW_SONY_ILCA)) ||
      (id == SonyID_SLT_A33)  ||
      (id == SonyID_SLT_A55) ||
      (id == SonyID_SLT_A35)  ||
      (len < 3))
    return;

  imSony.AFType = SonySubstitution[buf[0x02]];

  if (imSony.CameraType == LIBRAW_SONY_ILCA)
  {
    if (len >= 0x06)
    {
      imgdata.shootinginfo.FocusMode = SonySubstitution[buf[0x05]];
    }
    if (len >= 0x0051)
    {
      imSony.AFMicroAdjValue = SonySubstitution[buf[0x0050]];
    }
  }
  else
  {
    if (len >= 0x0c)
    {
      imgdata.shootinginfo.FocusMode = SonySubstitution[buf[0x0b]];
    }
    if (len >= 0x017e)
    {
      imSony.AFMicroAdjValue = SonySubstitution[buf[0x017d]];
    }
  }

  if (imSony.AFMicroAdjValue != 0)
    imSony.AFMicroAdjOn = 1;
}

void LibRaw::parseSonyMakernotes(
    int base, unsigned tag, unsigned type, unsigned len, unsigned dng_writer,
    uchar *&table_buf_0x0116, ushort &table_buf_0x0116_len,
    uchar *&table_buf_0x2010, ushort &table_buf_0x2010_len,
    uchar *&table_buf_0x9050, ushort &table_buf_0x9050_len,
    uchar *&table_buf_0x9400, ushort &table_buf_0x9400_len,
    uchar *&table_buf_0x9402, ushort &table_buf_0x9402_len,
    uchar *&table_buf_0x9403, ushort &table_buf_0x9403_len,
    uchar *&table_buf_0x9406, ushort &table_buf_0x9406_len,
    uchar *&table_buf_0x940c, ushort &table_buf_0x940c_len,
    uchar *&table_buf_0x940e, ushort &table_buf_0x940e_len)
{

  ushort lid, a, c, d;
  uchar *table_buf;
  uchar uc;
  uchar s[2];
  int LensDataValid = 0;
  unsigned uitemp;

  if (tag == 0xb001)
  { // Sony ModelID
    unique_id = get2();
    setSonyBodyFeatures(unique_id);

    if (table_buf_0x0116_len)
    {
      process_Sony_0x0116(table_buf_0x0116, table_buf_0x0116_len, unique_id);
      free(table_buf_0x0116);
      table_buf_0x0116_len = 0;
    }

    if (table_buf_0x2010_len)
    {
      process_Sony_0x2010(table_buf_0x2010, table_buf_0x2010_len);
      free(table_buf_0x2010);
      table_buf_0x2010_len = 0;
    }

    if (table_buf_0x9050_len)
    {
      process_Sony_0x9050(table_buf_0x9050, table_buf_0x9050_len, unique_id);
      free(table_buf_0x9050);
      table_buf_0x9050_len = 0;
    }

    if (table_buf_0x9400_len)
    {
      process_Sony_0x9400(table_buf_0x9400, table_buf_0x9400_len, unique_id);
      free(table_buf_0x9400);
      table_buf_0x9400_len = 0;
    }

    if (table_buf_0x9402_len)
    {
      process_Sony_0x9402(table_buf_0x9402, table_buf_0x9402_len);
      free(table_buf_0x9402);
      table_buf_0x9402_len = 0;
    }

    if (table_buf_0x9403_len)
    {
      process_Sony_0x9403(table_buf_0x9403, table_buf_0x9403_len);
      free(table_buf_0x9403);
      table_buf_0x9403_len = 0;
    }

    if (table_buf_0x9406_len)
    {
      process_Sony_0x9406(table_buf_0x9406, table_buf_0x9406_len);
      free(table_buf_0x9406);
      table_buf_0x9406_len = 0;
    }

    if (table_buf_0x940c_len)
    {
      process_Sony_0x940c(table_buf_0x940c, table_buf_0x940c_len);
      free(table_buf_0x940c);
      table_buf_0x940c_len = 0;
    }

    if (table_buf_0x940e_len)
    {
      process_Sony_0x940e(table_buf_0x940e, table_buf_0x940e_len, unique_id);
      free(table_buf_0x940e);
      table_buf_0x940e_len = 0;
    }
  }
  else if (tag == 0xb000)
  {
    FORC4 imSony.FileFormat = imSony.FileFormat * 10 + fgetc(ifp);
  }
  else if (tag == 0xb026)
  {
    uitemp = get4();
    if (uitemp != 0xffffffff)
      imgdata.shootinginfo.ImageStabilization = uitemp;
  }
  else if (((tag == 0x0001) || // Minolta CameraSettings, big endian
            (tag == 0x0003)) &&
           (len >= 196))
  {
    table_buf = (uchar *)malloc(len);
    fread(table_buf, len, 1, ifp);

    lid = 0x01 << 2;
    imgdata.shootinginfo.ExposureMode =
        (unsigned)table_buf[lid] << 24 | (unsigned)table_buf[lid + 1] << 16 |
        (unsigned)table_buf[lid + 2] << 8 | (unsigned)table_buf[lid + 3];

    lid = 0x06 << 2;
    imgdata.shootinginfo.DriveMode =
        (unsigned)table_buf[lid] << 24 | (unsigned)table_buf[lid + 1] << 16 |
        (unsigned)table_buf[lid + 2] << 8 | (unsigned)table_buf[lid + 3];

    lid = 0x07 << 2;
    imgdata.shootinginfo.MeteringMode =
        (unsigned)table_buf[lid] << 24 | (unsigned)table_buf[lid + 1] << 16 |
        (unsigned)table_buf[lid + 2] << 8 | (unsigned)table_buf[lid + 3];

    lid = 0x25 << 2;
    imSony.MinoltaCamID =
        (unsigned)table_buf[lid] << 24 | (unsigned)table_buf[lid + 1] << 16 |
        (unsigned)table_buf[lid + 2] << 8 | (unsigned)table_buf[lid + 3];
    if (imSony.MinoltaCamID != 0xffffffff)
      ilm.CamID = imSony.MinoltaCamID;

    lid = 0x30 << 2;
    imgdata.shootinginfo.FocusMode =
        (unsigned)table_buf[lid] << 24 | (unsigned)table_buf[lid + 1] << 16 |
        (unsigned)table_buf[lid + 2] << 8 | (unsigned)table_buf[lid + 3];

    free(table_buf);
  }
  else if ((tag == 0x0004) && // Minolta CameraSettings7D, big endian
           (len >= 227))
  {
    table_buf = (uchar *)malloc(len);
    fread(table_buf, len, 1, ifp);

    lid = 0x0;
    imgdata.shootinginfo.ExposureMode =
        (ushort)table_buf[lid] << 8 | (ushort)table_buf[lid + 1];

    lid = 0x0e << 1;
    imgdata.shootinginfo.FocusMode =
        (ushort)table_buf[lid] << 8 | (ushort)table_buf[lid + 1];

    lid = 0x10 << 1;
    imgdata.shootinginfo.AFPoint =
        (ushort)table_buf[lid] << 8 | (ushort)table_buf[lid + 1];

    lid = 0x25 << 1;
    switch ((ushort)table_buf[lid] << 8 | (ushort)table_buf[lid + 1]) {
    case 0:
    case 1:
      imCommon.ColorSpace = LIBRAW_COLORSPACE_sRGB;
      break;
    case 4:
      imCommon.ColorSpace = LIBRAW_COLORSPACE_AdobeRGB;
      break;
    default:
      imCommon.ColorSpace = LIBRAW_COLORSPACE_Unknown;
      break;
    }

    lid = 0x71 << 1;
    imgdata.shootinginfo.ImageStabilization =
        (ushort)table_buf[lid] << 8 | (ushort)table_buf[lid + 1];

    free(table_buf);
  }
  else if ((tag == 0x0010) && // CameraInfo
           strncasecmp(model, "DSLR-A100", 9) &&
           !strncasecmp(make, "SONY", 4) &&
           ((len == 368) ||  // a700                         : CameraInfo
            (len == 5478) || // a850, a900                   : CameraInfo
            (len == 5506) || // a200, a300, a350             : CameraInfo2
            (len == 6118) || // a230, a290, a330, a380, a390 : CameraInfo2
            (len == 15360))  // a450, a500, a550, a560, a580 : CameraInfo3
                             // a33, a35, a55
                             // NEX-3, NEX-5, NEX-5C, NEX-C3, NEX-VG10E

  )
  {
    table_buf = (uchar *)malloc(len);
    fread(table_buf, len, 1, ifp);
    if (memcmp(table_buf, "\xff\xff\xff\xff\xff\xff\xff\xff", 8) &&
        memcmp(table_buf, "\x00\x00\x00\x00\x00\x00\x00\x00", 8))
    {
      LensDataValid = 1;
    }
    switch (len)
    {
    case 368:  // a700: CameraInfo
    case 5478: // a850, a900: CameraInfo
      if ((!dng_writer) ||
          (saneSonyCameraInfo(table_buf[0], table_buf[3], table_buf[2],
                              table_buf[5], table_buf[4], table_buf[7])))
      {
        if (LensDataValid)
        {
          if (table_buf[0] | table_buf[3])
            ilm.MinFocal = bcd2dec(table_buf[0]) * 100 + bcd2dec(table_buf[3]);
          if (table_buf[2] | table_buf[5])
            ilm.MaxFocal = bcd2dec(table_buf[2]) * 100 + bcd2dec(table_buf[5]);
          if (table_buf[4])
            ilm.MaxAp4MinFocal = bcd2dec(table_buf[4]) / 10.0f;
          if (table_buf[4])
            ilm.MaxAp4MaxFocal = bcd2dec(table_buf[7]) / 10.0f;
          parseSonyLensFeatures(table_buf[1], table_buf[6]);
        }

        imSony.AFPointSelected = table_buf[21];
        imgdata.shootinginfo.AFPoint = (ushort)table_buf[25];

        if (len == 5478)
        {
          imSony.AFMicroAdjValue = table_buf[304] - 20;
          imSony.AFMicroAdjOn = (((table_buf[305] & 0x80) == 0x80) ? 1 : 0);
          imSony.AFMicroAdjRegisteredLenses = table_buf[305] & 0x7f;
        }
      }
      break;
    default:
      // CameraInfo2 & 3
      if ((!dng_writer) ||
          (saneSonyCameraInfo(table_buf[1], table_buf[2], table_buf[3],
                              table_buf[4], table_buf[5], table_buf[6])))
      {
        if ((LensDataValid) && strncasecmp(model, "NEX-5C", 6))
        {
          if (table_buf[1] | table_buf[2])
            ilm.MinFocal = bcd2dec(table_buf[1]) * 100 + bcd2dec(table_buf[2]);
          if (table_buf[3] | table_buf[4])
            ilm.MaxFocal = bcd2dec(table_buf[3]) * 100 + bcd2dec(table_buf[4]);
          if (table_buf[5])
            ilm.MaxAp4MinFocal = bcd2dec(table_buf[5]) / 10.0f;
          if (table_buf[6])
            ilm.MaxAp4MaxFocal = bcd2dec(table_buf[6]) / 10.0f;
          parseSonyLensFeatures(table_buf[0], table_buf[7]);
        }

        if (!strncasecmp(model, "DSLR-A450", 9) ||
            !strncasecmp(model, "DSLR-A500", 9) ||
            !strncasecmp(model, "DSLR-A550", 9))
        {
          imSony.AFPointSelected = table_buf[0x14];
          imgdata.shootinginfo.FocusMode = table_buf[0x15];
          imgdata.shootinginfo.AFPoint = (ushort)table_buf[0x18];
        }
        else if (!strncasecmp(model, "SLT-", 4) ||
                 !strncasecmp(model, "DSLR-A560", 9) ||
                 !strncasecmp(model, "DSLR-A580", 9))
        {
          imSony.AFPointSelected = table_buf[0x1c];
          imgdata.shootinginfo.FocusMode = table_buf[0x1d];
          imgdata.shootinginfo.AFPoint = (ushort)table_buf[0x20];
        }
      }
    }
    free(table_buf);
  }
  else if ((!dng_writer) && ((tag == 0x0020) || (tag == 0xb0280020)))
  {
    if (!strncasecmp(model, "DSLR-A100", 9))
    { // WBInfoA100
      fseek(ifp, 0x49dc, SEEK_CUR);
      stmread(imgdata.shootinginfo.InternalBodySerial, 13, ifp);
    }
    else if ((len ==
              19154) || // a200 a230 a290 a300 a330 a350 a380 a390 : FocusInfo
             (len == 19148))
    { // a700 a850 a900                          : FocusInfo
      table_buf = (uchar *)malloc(128);
      fread(table_buf, 128, 1, ifp);
      imgdata.shootinginfo.DriveMode = table_buf[14];
      imgdata.shootinginfo.ExposureProgram = table_buf[63];
      free(table_buf);
    }
    else if (len == 20480) // a450 a500 a550 a560 a580 a33 a35 a55 : MoreInfo
                           // NEX-3 NEX-5 NEX-C3 NEX-VG10E         : MoreInfo
    {
      a = get2();
      /*b =*/ get2();
      c = get2();
      d = get2();
      if ((a) && (c == 1))
      {
        fseek(ifp, d - 8, SEEK_CUR);
        table_buf = (uchar *)malloc(256);
        fread(table_buf, 256, 1, ifp);
        imgdata.shootinginfo.DriveMode = table_buf[1];
        imgdata.shootinginfo.ExposureProgram = table_buf[2];
        imgdata.shootinginfo.MeteringMode = table_buf[3];
        switch (table_buf[6]) {
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
        if (strncasecmp(model, "DSLR-A450", 9) &&
            strncasecmp(model, "DSLR-A500", 9) &&
            strncasecmp(model, "DSLR-A550", 9))
          imgdata.shootinginfo.FocusMode = table_buf[0x13];
        else
          imgdata.shootinginfo.FocusMode = table_buf[0x2c];
        free(table_buf);
      }
    }
  }
  else if (tag == 0x0102)
  {
    imSony.Quality = get4();
  }
  else if (tag == 0x0104)
  {
    imCommon.FlashEC = getreal(type);
  }
  else if (tag == 0x0105)
  { // Teleconverter
    ilm.TeleconverterID = get4();
  }
  else if (tag == 0x0107)
  {
    uitemp = get4();
    if (uitemp == 1)
      imgdata.shootinginfo.ImageStabilization = 0;
    else if (uitemp == 5)
      imgdata.shootinginfo.ImageStabilization = 1;
    else
      imgdata.shootinginfo.ImageStabilization = uitemp;
  }
  else if ((tag == 0xb0280088) && (dng_writer == nonDNG))
  {
    thumb_offset = get4() + base;
  }
  else if ((tag == 0xb0280089) && (dng_writer == nonDNG))
  {
    thumb_length = get4();
  }
  else if (((tag == 0x0114) || // CameraSettings
            (tag == 0xb0280114)) &&
           (len < 256000))
  {
    table_buf = (uchar *)malloc(len);
    fread(table_buf, len, 1, ifp);
    switch (len)
    {
    case 260: // Sony a100, big endian
      imgdata.shootinginfo.ExposureMode =
          ((ushort)table_buf[0]) << 8 | ((ushort)table_buf[1]);
      lid = 0x0a << 1;
      imgdata.shootinginfo.DriveMode =
          ((ushort)table_buf[lid]) << 8 | ((ushort)table_buf[lid + 1]);
      lid = 0x0c << 1;
      imgdata.shootinginfo.FocusMode =
          ((ushort)table_buf[lid]) << 8 | ((ushort)table_buf[lid + 1]);
      lid = 0x0d << 1;
      imSony.AFPointSelected = table_buf[lid + 1];
      lid = 0x0e << 1;
      imSony.AFAreaModeSetting = table_buf[lid + 1];
      lid = 0x12 << 1;
      imgdata.shootinginfo.MeteringMode =
          ((ushort)table_buf[lid]) << 8 | ((ushort)table_buf[lid + 1]);

      lid = 0x17 << 1;
      switch ((ushort)table_buf[lid] << 8 | (ushort)table_buf[lid + 1]) {
      case 0:
        imCommon.ColorSpace = LIBRAW_COLORSPACE_sRGB;
        break;
      case 2:
        imCommon.ColorSpace = LIBRAW_COLORSPACE_MonochromeGamma;
        break;
      case 5:
        imCommon.ColorSpace = LIBRAW_COLORSPACE_AdobeRGB;
        break;
      default:
        imCommon.ColorSpace = LIBRAW_COLORSPACE_Unknown;
        break;
      }

      break;
    case 448: // Minolta "DYNAX 5D" and its aliases, big endian
      lid = 0x0a << 1;
      imgdata.shootinginfo.ExposureMode =
          ((ushort)table_buf[lid]) << 8 | ((ushort)table_buf[lid + 1]);
      lid = 0x25 << 1;
      imgdata.shootinginfo.MeteringMode =
          ((ushort)table_buf[lid]) << 8 | ((ushort)table_buf[lid + 1]);

      lid = 0x2f << 1;
      switch ((ushort)table_buf[lid] << 8 | (ushort)table_buf[lid + 1]) {
      case 0:
      case 1:
        imCommon.ColorSpace = LIBRAW_COLORSPACE_sRGB;
        break;
      case 2:
        imCommon.ColorSpace = LIBRAW_COLORSPACE_MonochromeGamma;
        break;
      case 4:
      case 5:
        imCommon.ColorSpace = LIBRAW_COLORSPACE_AdobeRGB;
        break;
      default:
        imCommon.ColorSpace = LIBRAW_COLORSPACE_Unknown;
        break;
      }

      lid = 0xbd << 1;
      imgdata.shootinginfo.ImageStabilization =
          ((ushort)table_buf[lid]) << 8 | ((ushort)table_buf[lid + 1]);
      break;
    case 280: // a200 a300 a350 a700
    case 364: // a850 a900
      // CameraSettings and CameraSettings2 are big endian
      if (table_buf[2] | table_buf[3])
      {
        lid = (((ushort)table_buf[2]) << 8) | ((ushort)table_buf[3]);
        ilm.CurAp = libraw_powf64l(2.0f, ((float)lid / 8.0f - 1.0f) / 2.0f);
      }
      lid = 0x04 << 1;
      imgdata.shootinginfo.DriveMode = table_buf[lid + 1];
      lid = 0x1b << 1;
      switch (((ushort)table_buf[lid]) << 8 | ((ushort)table_buf[lid + 1])) {
      case 0:
        imCommon.ColorSpace = LIBRAW_COLORSPACE_sRGB;
        break;
      case 1:
      case 5:
        imCommon.ColorSpace = LIBRAW_COLORSPACE_AdobeRGB;
        break;
      default:
        imCommon.ColorSpace = LIBRAW_COLORSPACE_Unknown;
        break;
      }
      lid = 0x4d << 1;
      imgdata.shootinginfo.FocusMode =
          ((ushort)table_buf[lid]) << 8 | ((ushort)table_buf[lid + 1]);
      if (!imCommon.ColorSpace ||
          (imCommon.ColorSpace == LIBRAW_COLORSPACE_Unknown)) {
        lid = 0x83 << 1;
        switch (((ushort)table_buf[lid]) << 8 | ((ushort)table_buf[lid + 1])) {
        case 6:
          imCommon.ColorSpace = LIBRAW_COLORSPACE_sRGB;
          break;
        case 5:
          imCommon.ColorSpace = LIBRAW_COLORSPACE_AdobeRGB;
          break;
        default:
          imCommon.ColorSpace = LIBRAW_COLORSPACE_Unknown;
          break;
        }
      }
      break;
    case 332: // a230 a290 a330 a380 a390
      // CameraSettings and CameraSettings2 are big endian
      if (table_buf[2] | table_buf[3])
      {
        lid = (((ushort)table_buf[2]) << 8) | ((ushort)table_buf[3]);
        ilm.CurAp = libraw_powf64l(2.0f, ((float)lid / 8.0f - 1.0f) / 2.0f);
      }
      lid = 0x4d << 1;
      imgdata.shootinginfo.FocusMode =
          ((ushort)table_buf[lid]) << 8 | ((ushort)table_buf[lid + 1]);
      lid = 0x7e << 1;
      imgdata.shootinginfo.DriveMode = table_buf[lid + 1];
      break;
    case 1536: // a560 a580 a33 a35 a55 NEX-3 NEX-5 NEX-5C NEX-C3 NEX-VG10E
    case 2048: // a450 a500 a550
      // CameraSettings3 are little endian
      switch (table_buf[0x0e]) {
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
      imgdata.shootinginfo.DriveMode = table_buf[0x34];
      parseSonyLensType2(table_buf[1016], table_buf[1015]);
      if (ilm.LensMount != LIBRAW_MOUNT_Canon_EF)
      {
        switch (table_buf[153])
        {
        case 16:
          ilm.LensMount = LIBRAW_MOUNT_Minolta_A;
          break;
        case 17:
          ilm.LensMount = LIBRAW_MOUNT_Sony_E;
          break;
        }
      }
      break;
    }
    free(table_buf);
  }
  else if ((tag == 0x3000) && (len < 256000))
  {
    table_buf = (uchar *)malloc(len);
    fread(table_buf, len, 1, ifp);
    for (int i = 0; i < 20; i++)
      imSony.SonyDateTime[i] = table_buf[6 + i];
    free(table_buf);
  }
  else if (tag == 0x0116 && len < 256000)
  {
    table_buf_0x0116 = (uchar *)malloc(len);
    table_buf_0x0116_len = len;
    fread(table_buf_0x0116, len, 1, ifp);
    if (ilm.CamID)
    {
      process_Sony_0x0116(table_buf_0x0116, table_buf_0x0116_len, ilm.CamID);
      free(table_buf_0x0116);
      table_buf_0x0116_len = 0;
    }
  }
  else if (tag == 0x2008)
  {
    imSony.LongExposureNoiseReduction = get4();
  }
  else if (tag == 0x2009)
  {
    imSony.HighISONoiseReduction = get2();
  }
  else if (tag == 0x200a)
  {
    imSony.HDR[0] = get2();
    imSony.HDR[1] = get2();
  }
  else if (tag == 0x2010 && len < 256000)
  {
    table_buf_0x2010 = (uchar *)malloc(len);
    table_buf_0x2010_len = len;
    fread(table_buf_0x2010, len, 1, ifp);
    if (ilm.CamID)
    {
      process_Sony_0x2010(table_buf_0x2010, table_buf_0x2010_len);
      free(table_buf_0x2010);
      table_buf_0x2010_len = 0;
    }
  }
  else if (tag == 0x201a)
  {
    imSony.ElectronicFrontCurtainShutter = get4();
  }
  else if (tag == 0x201b)
  {
    if ((imSony.CameraType != LIBRAW_SONY_DSC) ||
        (ilm.CamID == SonyID_DSC_RX10M4)   ||
        (ilm.CamID == SonyID_DSC_RX100M6)  ||
        (ilm.CamID == SonyID_DSC_RX100M5A) ||
        (ilm.CamID == SonyID_DSC_RX0M2)    ||
        (ilm.CamID == SonyID_DSC_RX100M7))
    {
      fread(&uc, 1, 1, ifp);
      imgdata.shootinginfo.FocusMode = (short)uc;
    }
  }
  else if (tag == 0x201c)
  {
    if ((imSony.CameraType != LIBRAW_SONY_DSC) ||
        (ilm.CamID == SonyID_DSC_RX10M4)   ||
        (ilm.CamID == SonyID_DSC_RX100M6)  ||
        (ilm.CamID == SonyID_DSC_RX100M5A) ||
        (ilm.CamID == SonyID_DSC_RX0M2)    ||
        (ilm.CamID == SonyID_DSC_RX100M7))
    {
      imSony.AFAreaModeSetting = fgetc(ifp);
    }
  }
  else if (tag == 0x201d)
  {
    if (((imSony.AFAreaModeSetting == 3) &&
         ((imSony.CameraType == LIBRAW_SONY_ILCE) ||
          (imSony.CameraType == LIBRAW_SONY_NEX) ||
          (ilm.CamID == SonyID_DSC_RX10M4)    ||
          (ilm.CamID == SonyID_DSC_RX100M6)   ||
          (ilm.CamID == SonyID_DSC_RX100M5A)  ||
          (ilm.CamID == SonyID_DSC_RX0M2)     ||
          (ilm.CamID == SonyID_DSC_RX100M7))) ||
        ((imSony.AFAreaModeSetting == 4) &&
         (imSony.CameraType == LIBRAW_SONY_ILCA)))
    {
      imSony.FlexibleSpotPosition[0] = get2();
      imSony.FlexibleSpotPosition[1] = get2();
    }
  }
  else if (tag == 0x201e)
  {
    if (imSony.CameraType != LIBRAW_SONY_DSC)
    {
      imSony.AFPointSelected = fgetc(ifp);
    }
  }
  else if (tag == 0x2020)
  {
    if (imSony.CameraType != LIBRAW_SONY_DSC)
    {
      fread(imSony.AFPointsUsed, 1, 10, ifp);
    }
  }
  else if (tag == 0x2021)
  {
    if ((imSony.CameraType != LIBRAW_SONY_DSC) ||
        (ilm.CamID == SonyID_DSC_RX10M4)   ||
        (ilm.CamID == SonyID_DSC_RX100M6)  ||
        (ilm.CamID == SonyID_DSC_RX100M5A) ||
        (ilm.CamID == SonyID_DSC_RX0M2)    ||
        (ilm.CamID == SonyID_DSC_RX100M7))
    {
      imSony.AFTracking = fgetc(ifp);
    }
  }
  else if (tag == 0x2027)
  {
    FORC4 imSony.FocusLocation[c] = get2();
  }
  else if (tag == 0x2028)
  {
    if (get2())
    {
      imSony.VariableLowPassFilter = get2();
    }
  }
  else if (tag == 0x2029)
  {
    imSony.RAWFileType = get2();
  }
  else if (tag == 0x202c)
  {
    imSony.MeteringMode2 = get2();
  }
  else if (tag == 0x202f)
  {
    imSony.PixelShiftGroupID = get4();
    imSony.PixelShiftGroupPrefix = imSony.PixelShiftGroupID >> 22;
    imSony.PixelShiftGroupID =
        ((imSony.PixelShiftGroupID >> 17) & (unsigned)0x1f) *
            (unsigned)1000000 +
        ((imSony.PixelShiftGroupID >> 12) & (unsigned)0x1f) * (unsigned)10000 +
        ((imSony.PixelShiftGroupID >> 6) & (unsigned)0x3f) * (unsigned)100 +
        (imSony.PixelShiftGroupID & (unsigned)0x3f);

    imSony.numInPixelShiftGroup = fgetc(ifp);
    imSony.nShotsInPixelShiftGroup = fgetc(ifp);
  }
  else if (tag == 0x9050 && len < 256000)
  { // little endian
    table_buf_0x9050 = (uchar *)malloc(len);
    table_buf_0x9050_len = len;
    fread(table_buf_0x9050, len, 1, ifp);

    if (ilm.CamID)
    {
      process_Sony_0x9050(table_buf_0x9050, table_buf_0x9050_len, ilm.CamID);
      free(table_buf_0x9050);
      table_buf_0x9050_len = 0;
    }
  }
  else if (tag == 0x9400 && len < 256000)
  {
    table_buf_0x9400 = (uchar *)malloc(len);
    table_buf_0x9400_len = len;
    fread(table_buf_0x9400, len, 1, ifp);
    if (ilm.CamID)
    {
      process_Sony_0x9400(table_buf_0x9400, table_buf_0x9400_len, unique_id);
      free(table_buf_0x9400);
      table_buf_0x9400_len = 0;
    }
  }
  else if (tag == 0x9402 && len < 256000)
  {
    table_buf_0x9402 = (uchar *)malloc(len);
    table_buf_0x9402_len = len;
    fread(table_buf_0x9402, len, 1, ifp);
    if (ilm.CamID)
    {
      process_Sony_0x9402(table_buf_0x9402, table_buf_0x9402_len);
      free(table_buf_0x9402);
      table_buf_0x9402_len = 0;
    }
  }
  else if (tag == 0x9403 && len < 256000)
  {
    table_buf_0x9403 = (uchar *)malloc(len);
    table_buf_0x9403_len = len;
    fread(table_buf_0x9403, len, 1, ifp);
    if (ilm.CamID)
    {
      process_Sony_0x9403(table_buf_0x9403, table_buf_0x9403_len);
      free(table_buf_0x9403);
      table_buf_0x9403_len = 0;
    }
  }
  else if ((tag == 0x9405) && (len < 256000) && (len > 0x64))
  {
    table_buf = (uchar *)malloc(len);
    fread(table_buf, len, 1, ifp);
    uc = table_buf[0x0];
    if (imCommon.real_ISO < 0.1f)
    {
      if ((uc == 0x25) || (uc == 0x3a) || (uc == 0x76) || (uc == 0x7e) ||
          (uc == 0x8b) || (uc == 0x9a) || (uc == 0xb3) || (uc == 0xe1))
      {
        s[0] = SonySubstitution[table_buf[0x04]];
        s[1] = SonySubstitution[table_buf[0x05]];
        imCommon.real_ISO =
            100.0f * libraw_powf64l(2.0f, (16 - ((float)sget2(s)) / 256.0f));
      }
    }
    free(table_buf);
  }
  else if (tag == 0x9406 && len < 256000)
  {
    table_buf_0x9406 = (uchar *)malloc(len);
    table_buf_0x9406_len = len;
    fread(table_buf_0x9406, len, 1, ifp);
    if (ilm.CamID)
    {
      process_Sony_0x9406(table_buf_0x9406, table_buf_0x9406_len);
      free(table_buf_0x9406);
      table_buf_0x9406_len = 0;
    }
  }
  else if (tag == 0x940c && len < 256000)
  {
    table_buf_0x940c = (uchar *)malloc(len);
    table_buf_0x940c_len = len;
    fread(table_buf_0x940c, len, 1, ifp);
    if (ilm.CamID)
    {
      process_Sony_0x940c(table_buf_0x940c, table_buf_0x940c_len);
      free(table_buf_0x940c);
      table_buf_0x940c_len = 0;
    }
  }
  else if (tag == 0x940e && len < 256000)
  {
    table_buf_0x940e = (uchar *)malloc(len);
    table_buf_0x940e_len = len;
    fread(table_buf_0x940e, len, 1, ifp);
    if (ilm.CamID)
    {
      process_Sony_0x940e(table_buf_0x940e, table_buf_0x940e_len, ilm.CamID);
      free(table_buf_0x940e);
      table_buf_0x940e_len = 0;
    }
  }
  else if (((tag == 0xb027) || (tag == 0x010c)) && (ilm.LensID == LIBRAW_LENS_NOT_SET))
  {
    ilm.LensID = get4();
//    printf ("==>> 1: ilm.LensID %lld\n", ilm.LensID);
    if ((ilm.LensID > 0x4900) && (ilm.LensID <= 0x5900))
    {
      ilm.AdapterID = 0x4900;
      ilm.LensID -= ilm.AdapterID;
      ilm.LensMount = LIBRAW_MOUNT_Sigma_X3F;
      strcpy(ilm.Adapter, "MC-11");
    }

    else if ((ilm.LensID > 0xef00) && (ilm.LensID < 0xffff) &&
             (ilm.LensID != 0xff00))
    {
      ilm.AdapterID = 0xef00;
      ilm.LensID -= ilm.AdapterID;
      ilm.LensMount = LIBRAW_MOUNT_Canon_EF;
    }

    else if (((ilm.LensID != LIBRAW_LENS_NOT_SET) && (ilm.LensID < 0xef00)) ||
             (ilm.LensID == 0xff00))
      ilm.LensMount = LIBRAW_MOUNT_Minolta_A;
    /*
        if (tag == 0x010c)
          ilm.CameraMount = LIBRAW_MOUNT_Minolta_A;
    */
  }
  else if (tag == 0xb02a && len < 256000)
  { // Sony LensSpec
    table_buf = (uchar *)malloc(len);
    fread(table_buf, len, 1, ifp);
    if ((!dng_writer) ||
        (saneSonyCameraInfo(table_buf[1], table_buf[2], table_buf[3],
                            table_buf[4], table_buf[5], table_buf[6])))
    {
      if (table_buf[1] | table_buf[2])
        ilm.MinFocal = bcd2dec(table_buf[1]) * 100 + bcd2dec(table_buf[2]);
      if (table_buf[3] | table_buf[4])
        ilm.MaxFocal = bcd2dec(table_buf[3]) * 100 + bcd2dec(table_buf[4]);
      if (table_buf[5])
        ilm.MaxAp4MinFocal = bcd2dec(table_buf[5]) / 10.0f;
      if (table_buf[6])
        ilm.MaxAp4MaxFocal = bcd2dec(table_buf[6]) / 10.0f;
      parseSonyLensFeatures(table_buf[0], table_buf[7]);
    }
    free(table_buf);
  }
  else if ((tag == 0xb02b) && !imgdata.sizes.raw_inset_crop.cwidth &&
           (len == 2))
  {
    imgdata.sizes.raw_inset_crop.cheight = get4();
    imgdata.sizes.raw_inset_crop.cwidth = get4();
  }
  else if (tag == 0xb041)
  {
    imgdata.shootinginfo.ExposureMode = get2();
  }

  // MetaVersion: (unique_id >= 286)
}

void LibRaw::parseSonySR2(uchar *cbuf_SR2, unsigned SR2SubIFDOffset,
                          unsigned SR2SubIFDLength, unsigned dng_writer)
{
  unsigned c;
  unsigned entries, tag_id, tag_type, tag_datalen;
  INT64 tag_offset, tag_dataoffset;
  int TagProcessed;
  int tag_dataunitlen;
  float num;
  int i;
  int WBCTC_count;
#define CHECKBUFFER_N(offset,N)                                     \
  do                                                                \
  {                                                                 \
    if ((((offset) + (N)) > SR2SubIFDLength) || ((offset) < 0))     \
      return;														\
  } while (0)

  CHECKBUFFER_N(0, 2);
  entries = sget2(cbuf_SR2);
  if (entries > 1000)
    return;
  tag_offset = 2;
  WBCTC_count = 0;
  while (entries--) {
    if (tiff_sget (SR2SubIFDOffset, cbuf_SR2, SR2SubIFDLength,
                   &tag_offset, &tag_id, &tag_type, &tag_dataoffset,
                   &tag_datalen, &tag_dataunitlen) == 0) {
      TagProcessed = 0;
      if (dng_writer == nonDNG) {
        switch (tag_id) {
        case 0x7300:
			CHECKBUFFER_N(tag_dataoffset + tag_dataunitlen * 4,0);
			FORC4 cblack[c] = sget2(cbuf_SR2 + tag_dataoffset + tag_dataunitlen * c);
          TagProcessed = 1;
          break;
        case 0x7303:
			CHECKBUFFER_N(tag_dataoffset + tag_dataunitlen * 4, 0);
			FORC4 cam_mul[GRBG_2_RGBG(c)] = sget2(cbuf_SR2 + tag_dataoffset + tag_dataunitlen * c);
          TagProcessed = 1;
          break;
        case 0x7310:
			CHECKBUFFER_N(tag_dataoffset + tag_dataunitlen * 4, 0);
			FORC4 cblack[RGGB_2_RGBG(c)] = sget2(cbuf_SR2 + tag_dataoffset + tag_dataunitlen * c);
          i = cblack[3];
          FORC3 if (i > (int)cblack[c]) i = cblack[c];
          FORC4 cblack[c] -= i;
          black = i;
          TagProcessed = 1;
          break;
        case 0x7313:
			CHECKBUFFER_N(tag_dataoffset + tag_dataunitlen * 4, 0);
			FORC4 cam_mul[RGGB_2_RGBG(c)] = sget2(cbuf_SR2 + tag_dataoffset + tag_dataunitlen * c);
          TagProcessed = 1;
          break;
        case 0x74a0:
			CHECKBUFFER_N(tag_dataoffset, 4);
			ilm.MaxAp4MaxFocal = sgetreal(tag_type, cbuf_SR2 + tag_dataoffset);
          TagProcessed = 1;
          break;
        case 0x74a1:
			CHECKBUFFER_N(tag_dataoffset, 4);
			ilm.MaxAp4MinFocal = sgetreal(tag_type, cbuf_SR2 + tag_dataoffset);
          TagProcessed = 1;
          break;
        case 0x74a2:
			CHECKBUFFER_N(tag_dataoffset, 4);
			ilm.MaxFocal = sgetreal(tag_type, cbuf_SR2 + tag_dataoffset);
          TagProcessed = 1;
          break;
        case 0x74a3:
			CHECKBUFFER_N(tag_dataoffset, 4);
			ilm.MinFocal = sgetreal(tag_type, cbuf_SR2 + tag_dataoffset);
          TagProcessed = 1;
          break;
        case 0x7800:
			CHECKBUFFER_N(tag_dataoffset + tag_dataunitlen * 8, 2);
		  for (i = 0; i < 3; i++)
          {
            num = 0.0;
            for (c = 0; c < 3; c++)
            {
              imgdata.color.ccm[i][c] =
                  (float)((short)sget2(cbuf_SR2 + tag_dataoffset + tag_dataunitlen * (i * 3 + c)));
              num += imgdata.color.ccm[i][c];
            }
            if (num > 0.01)
              FORC3 imgdata.color.ccm[i][c] = imgdata.color.ccm[i][c] / num;
          }
          TagProcessed = 1;
          break;
        case 0x787f:
          if (tag_datalen == 3)
          {
			  CHECKBUFFER_N(tag_dataoffset + tag_dataunitlen * 2, 2);
			  FORC3 imgdata.color.linear_max[c] = sget2(cbuf_SR2 + tag_dataoffset + tag_dataunitlen * c);
            imgdata.color.linear_max[3] = imgdata.color.linear_max[1];
          }
          else if (tag_datalen == 1)
          {
			  CHECKBUFFER_N(tag_dataoffset, 2);
			  imgdata.color.linear_max[0] = imgdata.color.linear_max[1] =
                imgdata.color.linear_max[2] = imgdata.color.linear_max[3] =
                    sget2(cbuf_SR2 + tag_dataoffset);
          }
          TagProcessed = 1;
          break;
        }
      }

      if (!TagProcessed) {
        if ((tag_id >= 0x7480) && (tag_id <= 0x7486)) {
          i = tag_id - 0x7480;
          if (Sony_SR2_wb_list[i] > 255) {
            icWBCCTC[WBCTC_count][0] = Sony_SR2_wb_list[i];
			CHECKBUFFER_N(tag_dataoffset + tag_dataunitlen * 2, 2);
			FORC3 icWBCCTC[WBCTC_count][c + 1] = sget2(cbuf_SR2 + tag_dataoffset + tag_dataunitlen * c);
            icWBCCTC[WBCTC_count][4] = icWBCCTC[WBCTC_count][2];
            WBCTC_count++;
          } else {
			  CHECKBUFFER_N(tag_dataoffset + tag_dataunitlen * 2, 2);
			  FORC3 icWBC[Sony_SR2_wb_list[i]][c] = sget2(cbuf_SR2 + tag_dataoffset + tag_dataunitlen * c);
            icWBC[Sony_SR2_wb_list[i]][3] = icWBC[Sony_SR2_wb_list[i]][1];
          }
        } else if ((tag_id >= 0x7820) && (tag_id <= 0x782d)) {
          i = tag_id - 0x7820;
          if (Sony_SR2_wb_list1[i] > 255) {
            icWBCCTC[WBCTC_count][0] = Sony_SR2_wb_list1[i];
			CHECKBUFFER_N(tag_dataoffset + tag_dataunitlen * 2, 2);
			FORC3 icWBCCTC[WBCTC_count][c + 1] = sget2(cbuf_SR2 + tag_dataoffset + tag_dataunitlen * c);
            icWBCCTC[WBCTC_count][4] = icWBCCTC[WBCTC_count][2];
            if (Sony_SR2_wb_list1[i] == 3200) {
              FORC3 icWBC[LIBRAW_WBI_StudioTungsten][c] = icWBCCTC[WBCTC_count][c + 1];
              icWBC[LIBRAW_WBI_StudioTungsten][3] = icWBC[LIBRAW_WBI_StudioTungsten][1];
            }
            WBCTC_count++;
          } else {
			  CHECKBUFFER_N(tag_dataoffset + tag_dataunitlen * 2, 2);
			  FORC3 icWBC[Sony_SR2_wb_list1[i]][c] = sget2(cbuf_SR2 + tag_dataoffset + tag_dataunitlen * c);
            icWBC[Sony_SR2_wb_list1[i]][3] = icWBC[Sony_SR2_wb_list1[i]][1];
          }
        } else if (tag_id == 0x7302) {
			CHECKBUFFER_N(tag_dataoffset + tag_dataunitlen * 3, 2);
			FORC4 icWBC[LIBRAW_WBI_Auto][GRBG_2_RGBG(c)] = sget2(cbuf_SR2 + tag_dataoffset + tag_dataunitlen * c);
        } else if (tag_id == 0x7312) {
			CHECKBUFFER_N(tag_dataoffset + tag_dataunitlen * 3, 2);
			FORC4 icWBC[LIBRAW_WBI_Auto][RGGB_2_RGBG(c)] = sget2(cbuf_SR2 + tag_dataoffset + tag_dataunitlen * c);
        }
      }
    }
  }
}
#undef CHECKBUFFER_N

void LibRaw::parseSonySRF(unsigned len)
{

  if ((len > 0xfffff) || (len == 0))
    return;

  INT64 save = ftell(ifp);
  INT64 offset =
      0x0310c0 - save; /* for non-DNG this value normally is 0x8ddc */
  if (len < offset || offset < 0)
    return;
  INT64 decrypt_len = offset >> 2; /* master key offset value is the next
                                      un-encrypted metadata field after SRF0 */

  unsigned i, nWB;
  unsigned MasterKey, SRF2Key;
  INT64 srf_offset, tag_offset, tag_dataoffset;
  int tag_dataunitlen;
  uchar *srf_buf;
  ushort entries;
  unsigned tag_id, tag_type, tag_datalen;

  srf_buf = (uchar *)malloc(len+64);
  fread(srf_buf, len, 1, ifp);

  offset += srf_buf[offset] << 2;

#define CHECKBUFFER_SGET4(offset)                                              \
  do                                                                           \
  {                                                                            \
    if ((((offset) + 4) > len) || ((offset) < 0))                              \
      goto restore_after_parseSonySRF;                                         \
  } while (0)

#define CHECKBUFFER_SGET2(offset)                                              \
  do                                                                           \
  {                                                                            \
    if ( ((offset + 2) > len) || ((offset) < 0))                               \
      goto restore_after_parseSonySRF;                                         \
  } while (0)

  CHECKBUFFER_SGET4(offset);

  /* master key is stored in big endian */
  MasterKey = ((unsigned)srf_buf[offset] << 24) |
              ((unsigned)srf_buf[offset + 1] << 16) |
              ((unsigned)srf_buf[offset + 2] << 8) |
              (unsigned)srf_buf[offset + 3];

  /* skip SRF0 */
  srf_offset = 0;
  CHECKBUFFER_SGET2(srf_offset);
  entries = sget2(srf_buf + srf_offset);
  if (entries > 1000)
    goto restore_after_parseSonySRF;
  offset = srf_offset + 2;
  CHECKBUFFER_SGET4(offset);
  CHECKBUFFER_SGET4(offset + 12 * entries);
  srf_offset = sget4(srf_buf + offset + 12 * entries) -
               save; /* SRF0 ends with SRF1 abs. position */

  /* get SRF1, it has fixed 40 bytes length and contains keys to decode metadata
   * and raw data */
  if (srf_offset < 0 || decrypt_len < srf_offset / 4)
    goto restore_after_parseSonySRF;
  sony_decrypt((unsigned *)(srf_buf + srf_offset), decrypt_len - srf_offset / 4,
               1, MasterKey);
  CHECKBUFFER_SGET2(srf_offset);
  entries = sget2(srf_buf + srf_offset);
  if (entries > 1000)
    goto restore_after_parseSonySRF;
  offset = srf_offset + 2;
  tag_offset = offset;

  while (entries--) {
    if (tiff_sget (save, srf_buf, len,
                   &tag_offset, &tag_id, &tag_type, &tag_dataoffset,
                   &tag_datalen, &tag_dataunitlen) == 0) {
      if (tag_id == 0x0000) {
		  CHECKBUFFER_SGET4(tag_dataoffset);
		  SRF2Key = sget4(srf_buf + tag_dataoffset);
      } else if (tag_id == 0x0001) {
		  CHECKBUFFER_SGET4(tag_dataoffset);
		  /*RawDataKey =*/ sget4(srf_buf + tag_dataoffset);
      }
    } else goto restore_after_parseSonySRF;
  }
  offset = tag_offset;

  /* get SRF2 */
  CHECKBUFFER_SGET4(offset);
  srf_offset =
      sget4(srf_buf + offset) - save; /* SRFn ends with SRFn+1 position */
  if (srf_offset < 0 || decrypt_len < srf_offset / 4)
    goto restore_after_parseSonySRF;
  sony_decrypt((unsigned *)(srf_buf + srf_offset), decrypt_len - srf_offset / 4,
               1, SRF2Key);
  CHECKBUFFER_SGET2(srf_offset);
  entries = sget2(srf_buf + srf_offset);
  if (entries > 1000)
    goto restore_after_parseSonySRF;
  offset = srf_offset + 2;
  tag_offset = offset;

  while (entries--) {
	  if (tiff_sget(save, srf_buf, len,
                   &tag_offset, &tag_id, &tag_type, &tag_dataoffset,
                   &tag_datalen, &tag_dataunitlen) == 0) {
      if ((tag_id >= 0x00c0) && (tag_id <= 0x00ce)) {
        i = (tag_id - 0x00c0) % 3;
        nWB = (tag_id - 0x00c0) / 3;
		CHECKBUFFER_SGET4(tag_dataoffset);
		icWBC[Sony_SRF_wb_list[nWB]][i] = sget4(srf_buf + tag_dataoffset);
        if (i == 1) {
          icWBC[Sony_SRF_wb_list[nWB]][3] =
            icWBC[Sony_SRF_wb_list[nWB]][i];
        }
      } else if ((tag_id >= 0x00d0) && (tag_id <= 0x00d2)) {
        i = (tag_id - 0x00d0) % 3;
		CHECKBUFFER_SGET4(tag_dataoffset);
		cam_mul[i] = sget4(srf_buf + tag_dataoffset);
        if (i == 1) {
          cam_mul[3] = cam_mul[i];
        }
      } else switch (tag_id) {
        /*
        0x0002  SRF6Offset
        0x0003  SRFDataOffset (?)
        0x0004  RawDataOffset
        0x0005  RawDataLength
        */
      case 0x0043:
		  CHECKBUFFER_SGET4(tag_dataoffset); // need to add extra space
		  ilm.MaxAp4MaxFocal = sgetreal(tag_type, srf_buf + tag_dataoffset);
        break;
      case 0x0044:
		  CHECKBUFFER_SGET4(tag_dataoffset);
		  ilm.MaxAp4MinFocal = sgetreal(tag_type, srf_buf + tag_dataoffset);
        break;
      case 0x0045:
		  CHECKBUFFER_SGET4(tag_dataoffset);
		  ilm.MinFocal = sgetreal(tag_type, srf_buf + tag_dataoffset);
        break;
      case 0x0046:
		  CHECKBUFFER_SGET4(tag_dataoffset);
		  ilm.MaxFocal = sgetreal(tag_type, srf_buf + tag_dataoffset);
        break;
      }
    } else goto restore_after_parseSonySRF;
  }
  offset = tag_offset;

restore_after_parseSonySRF:
  free(srf_buf);
  fseek(ifp, save, SEEK_SET);
#undef CHECKBUFFER_SGET4
#undef CHECKBUFFER_SGET2
}
