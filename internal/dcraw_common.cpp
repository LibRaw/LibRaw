/* 
  Copyright 2008-2010 LibRaw LLC (info@libraw.org)

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of three licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

3. LibRaw Software License 27032010
   (See file LICENSE.LibRaw.pdf provided in LibRaw distribution archive for details).

   This file is generated from Dave Coffin's dcraw.c
   dcraw.c -- Dave Coffin's raw photo decoder
   Copyright 1997-2010 by Dave Coffin, dcoffin a cybercom o net

   Look into dcraw homepage (probably http://cybercom.net/~dcoffin/dcraw/)
   for more information
*/

#line 256 "dcraw/dcraw.c"
#include <math.h>
#define CLASS LibRaw::
#include "libraw/libraw_types.h"
#define LIBRAW_LIBRARY_BUILD
#define LIBRAW_IO_REDEFINED
#include "libraw/libraw.h"
#include "internal/defines.h"
#include "internal/var_defines.h"
#include "internal/libraw_bytebuffer.h"
#line 268 "dcraw/dcraw.c"
int CLASS fcol (int row, int col)
{
  static const char filter[16][16] =
  { { 2,1,1,3,2,3,2,0,3,2,3,0,1,2,1,0 },
    { 0,3,0,2,0,1,3,1,0,1,1,2,0,3,3,2 },
    { 2,3,3,2,3,1,1,3,3,1,2,1,2,0,0,3 },
    { 0,1,0,1,0,2,0,2,2,0,3,0,1,3,2,1 },
    { 3,1,1,2,0,1,0,2,1,3,1,3,0,1,3,0 },
    { 2,0,0,3,3,2,3,1,2,0,2,0,3,2,2,1 },
    { 2,3,3,1,2,1,2,1,2,1,1,2,3,0,0,1 },
    { 1,0,0,2,3,0,0,3,0,3,0,3,2,1,2,3 },
    { 2,3,3,1,1,2,1,0,3,2,3,0,2,3,1,3 },
    { 1,0,2,0,3,0,3,2,0,1,1,2,0,1,0,2 },
    { 0,1,1,3,3,2,2,1,1,3,3,0,2,1,3,2 },
    { 2,3,2,0,0,1,3,0,2,0,1,2,3,0,1,0 },
    { 1,3,1,2,3,2,3,2,0,2,0,1,1,0,3,0 },
    { 0,2,0,3,1,0,0,1,1,3,3,2,3,2,2,1 },
    { 2,1,3,2,3,1,2,1,0,3,0,2,0,2,0,2 },
    { 0,3,1,0,0,2,0,3,2,1,3,1,1,3,1,3 } };
  static const char filter2[6][6] =
  { { 1,1,0,1,1,2 },
    { 1,1,2,1,1,0 },
    { 2,0,1,0,2,1 },
    { 1,1,2,1,1,0 },
    { 1,1,0,1,1,2 },
    { 0,2,1,2,0,1 } };

  if (filters == 1) return filter[(row+top_margin)&15][(col+left_margin)&15];
  if (filters == 2) return filter2[(row+6) % 6][(col+6) % 6];
  return FC(row,col);
}

#ifndef __GLIBC__
char *my_memmem (char *haystack, size_t haystacklen,
	      char *needle, size_t needlelen)
{
  char *c;
  for (c = haystack; c <= haystack + haystacklen - needlelen; c++)
    if (!memcmp (c, needle, needlelen))
      return c;
  return 0;
}
#define memmem my_memmem
#endif
#line 334 "dcraw/dcraw.c"
ushort CLASS sget2 (uchar *s)
{
  if (order == 0x4949)		/* "II" means little-endian */
    return s[0] | s[1] << 8;
  else				/* "MM" means big-endian */
    return s[0] << 8 | s[1];
}

ushort CLASS get2()
{
  uchar str[2] = { 0xff,0xff };
  fread (str, 1, 2, ifp);
  return sget2(str);
}

unsigned CLASS sget4 (uchar *s)
{
  if (order == 0x4949)
    return s[0] | s[1] << 8 | s[2] << 16 | s[3] << 24;
  else
    return s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3];
}
#define sget4(s) sget4((uchar *)s)

unsigned CLASS get4()
{
  uchar str[4] = { 0xff,0xff,0xff,0xff };
  fread (str, 1, 4, ifp);
  return sget4(str);
}

unsigned CLASS getint (int type)
{
  return type == 3 ? get2() : get4();
}

float CLASS int_to_float (int i)
{
  union { int i; float f; } u;
  u.i = i;
  return u.f;
}

double CLASS getreal (int type)
{
  union { char c[8]; double d; } u;
  int i, rev;

  switch (type) {
    case 3: return (unsigned short) get2();
    case 4: return (unsigned int) get4();
    case 5:  u.d = (unsigned int) get4();
      return u.d / (unsigned int) get4();
    case 8: return (signed short) get2();
    case 9: return (signed int) get4();
    case 10: u.d = (signed int) get4();
      return u.d / (signed int) get4();
    case 11: return int_to_float (get4());
    case 12:
      rev = 7 * ((order == 0x4949) == (ntohs(0x1234) == 0x1234));
      for (i=0; i < 8; i++)
	u.c[i ^ rev] = fgetc(ifp);
      return u.d;
    default: return fgetc(ifp);
  }
}

void CLASS read_shorts (ushort *pixel, int count)
{
  if (fread (pixel, 2, count, ifp) < count) derror();
  if ((order == 0x4949) == (ntohs(0x1234) == 0x1234))
    swab (pixel, pixel, count*2);
}

void CLASS canon_600_fixed_wb (int temp)
{
  static const short mul[4][5] = {
    {  667, 358,397,565,452 },
    {  731, 390,367,499,517 },
    { 1119, 396,348,448,537 },
    { 1399, 485,431,508,688 } };
  int lo, hi, i;
  float frac=0;

  for (lo=4; --lo; )
    if (*mul[lo] <= temp) break;
  for (hi=0; hi < 3; hi++)
    if (*mul[hi] >= temp) break;
  if (lo != hi)
    frac = (float) (temp - *mul[lo]) / (*mul[hi] - *mul[lo]);
  for (i=1; i < 5; i++)
    pre_mul[i-1] = 1 / (frac * mul[hi][i] + (1-frac) * mul[lo][i]);
}

/* Return values:  0 = white  1 = near white  2 = not white */
int CLASS canon_600_color (int ratio[2], int mar)
{
  int clipped=0, target, miss;

  if (flash_used) {
    if (ratio[1] < -104)
      { ratio[1] = -104; clipped = 1; }
    if (ratio[1] >   12)
      { ratio[1] =   12; clipped = 1; }
  } else {
    if (ratio[1] < -264 || ratio[1] > 461) return 2;
    if (ratio[1] < -50)
      { ratio[1] = -50; clipped = 1; }
    if (ratio[1] > 307)
      { ratio[1] = 307; clipped = 1; }
  }
  target = flash_used || ratio[1] < 197
	? -38 - (398 * ratio[1] >> 10)
	: -123 + (48 * ratio[1] >> 10);
  if (target - mar <= ratio[0] &&
      target + 20  >= ratio[0] && !clipped) return 0;
  miss = target - ratio[0];
  if (abs(miss) >= mar*4) return 2;
  if (miss < -20) miss = -20;
  if (miss > mar) miss = mar;
  ratio[0] = target - miss;
  return 1;
}

void CLASS canon_600_auto_wb()
{
  int mar, row, col, i, j, st, count[] = { 0,0 };
  int test[8], total[2][8], ratio[2][2], stat[2];

  memset (&total, 0, sizeof total);
  i = canon_ev + 0.5;
  if      (i < 10) mar = 150;
  else if (i > 12) mar = 20;
  else mar = 280 - 20 * i;
  if (flash_used) mar = 80;
  for (row=14; row < height-14; row+=4)
    for (col=10; col < width; col+=2) {
      for (i=0; i < 8; i++)
	test[(i & 4) + FC(row+(i >> 1),col+(i & 1))] =
		    BAYER(row+(i >> 1),col+(i & 1));
      for (i=0; i < 8; i++)
	if (test[i] < 150 || test[i] > 1500) goto next;
      for (i=0; i < 4; i++)
	if (abs(test[i] - test[i+4]) > 50) goto next;
      for (i=0; i < 2; i++) {
	for (j=0; j < 4; j+=2)
	  ratio[i][j >> 1] = ((test[i*4+j+1]-test[i*4+j]) << 10) / test[i*4+j];
	stat[i] = canon_600_color (ratio[i], mar);
      }
      if ((st = stat[0] | stat[1]) > 1) goto next;
      for (i=0; i < 2; i++)
	if (stat[i])
	  for (j=0; j < 2; j++)
	    test[i*4+j*2+1] = test[i*4+j*2] * (0x400 + ratio[i][j]) >> 10;
      for (i=0; i < 8; i++)
	total[st][i] += test[i];
      count[st]++;
next: ;
    }
  if (count[0] | count[1]) {
    st = count[0]*200 < count[1];
    for (i=0; i < 4; i++)
      pre_mul[i] = 1.0 / (total[st][i] + total[st][i+4]);
  }
}

void CLASS canon_600_coeff()
{
  static const short table[6][12] = {
    { -190,702,-1878,2390,   1861,-1349,905,-393, -432,944,2617,-2105  },
    { -1203,1715,-1136,1648, 1388,-876,267,245,  -1641,2153,3921,-3409 },
    { -615,1127,-1563,2075,  1437,-925,509,3,     -756,1268,2519,-2007 },
    { -190,702,-1886,2398,   2153,-1641,763,-251, -452,964,3040,-2528  },
    { -190,702,-1878,2390,   1861,-1349,905,-393, -432,944,2617,-2105  },
    { -807,1319,-1785,2297,  1388,-876,769,-257,  -230,742,2067,-1555  } };
  int t=0, i, c;
  float mc, yc;

  mc = pre_mul[1] / pre_mul[2];
  yc = pre_mul[3] / pre_mul[2];
  if (mc > 1 && mc <= 1.28 && yc < 0.8789) t=1;
  if (mc > 1.28 && mc <= 2) {
    if  (yc < 0.8789) t=3;
    else if (yc <= 2) t=4;
  }
  if (flash_used) t=5;
  for (raw_color = i=0; i < 3; i++)
    FORCC rgb_cam[i][c] = table[t][i*4 + c] / 1024.0;
}

void CLASS canon_600_load_raw()
{
  uchar  data[1120], *dp;
  ushort *pix;
  int irow, row;

  for (irow=row=0; irow < height; irow++) {
    if (fread (data, 1, 1120, ifp) < 1120) derror();
    pix = raw_image + row*raw_width;
    for (dp=data; dp < data+1120;  dp+=10, pix+=8) {
      pix[0] = (dp[0] << 2) + (dp[1] >> 6    );
      pix[1] = (dp[2] << 2) + (dp[1] >> 4 & 3);
      pix[2] = (dp[3] << 2) + (dp[1] >> 2 & 3);
      pix[3] = (dp[4] << 2) + (dp[1]      & 3);
      pix[4] = (dp[5] << 2) + (dp[9]      & 3);
      pix[5] = (dp[6] << 2) + (dp[9] >> 2 & 3);
      pix[6] = (dp[7] << 2) + (dp[9] >> 4 & 3);
      pix[7] = (dp[8] << 2) + (dp[9] >> 6    );
    }
    if ((row+=2) > height) row = 1;
  }
}

void CLASS canon_600_correct()
{
  int row, col, val;
  static const short mul[4][2] =
  { { 1141,1145 }, { 1128,1109 }, { 1178,1149 }, { 1128,1109 } };

  for (row=0; row < height; row++)
    for (col=0; col < width; col++) {
      if ((val = BAYER(row,col) - black) < 0) val = 0;
      val = val * mul[row & 3][col & 1] >> 9;
      BAYER(row,col) = val;
    }
  canon_600_fixed_wb(1311);
  canon_600_auto_wb();
  canon_600_coeff();
  maximum = (0x3ff - black) * 1109 >> 9;
  black = 0;
}

int CLASS canon_s2is()
{
  unsigned row;

  for (row=0; row < 100; row++) {
    fseek (ifp, row*3340 + 3284, SEEK_SET);
    if (getc(ifp) > 15) return 1;
  }
  return 0;
}

/*
   getbits(-1) initializes the buffer
   getbits(n) where 0 <= n <= 25 returns an n-bit integer
 */
unsigned CLASS getbithuff (int nbits, ushort *huff)
{
#ifdef LIBRAW_NOTHREADS
  static unsigned bitbuf=0;
  static int vbits=0, reset=0;
#else
#define bitbuf tls->getbits.bitbuf
#define vbits  tls->getbits.vbits
#define reset  tls->getbits.reset
#endif
  unsigned c;

  if (nbits == -1)
    return bitbuf = vbits = reset = 0;
  if (nbits == 0 || vbits < 0) return 0;
  while (!reset && vbits < nbits && (c = fgetc(ifp)) != EOF &&
    !(reset = zero_after_ff && c == 0xff && fgetc(ifp))) {
    bitbuf = (bitbuf << 8) + (uchar) c;
    vbits += 8;
  }
  c = bitbuf << (32-vbits) >> (32-nbits);
  if (huff) {
    vbits -= huff[c] >> 8;
    c = (uchar) huff[c];
  } else
    vbits -= nbits;
  if (vbits < 0) derror();
  return c;
#ifndef LIBRAW_NOTHREADS
#undef bitbuf
#undef vbits
#undef reset
#endif
}

#define getbits(n) getbithuff(n,0)
#define gethuff(h) getbithuff(*h,h+1)

/*
   Construct a decode tree according the specification in *source.
   The first 16 bytes specify how many codes should be 1-bit, 2-bit
   3-bit, etc.  Bytes after that are the leaf values.

   For example, if the source is

    { 0,1,4,2,3,1,2,0,0,0,0,0,0,0,0,0,
      0x04,0x03,0x05,0x06,0x02,0x07,0x01,0x08,0x09,0x00,0x0a,0x0b,0xff  },

   then the code is

	00		0x04
	010		0x03
	011		0x05
	100		0x06
	101		0x02
	1100		0x07
	1101		0x01
	11100		0x08
	11101		0x09
	11110		0x00
	111110		0x0a
	1111110		0x0b
	1111111		0xff
 */
ushort * CLASS make_decoder_ref (const uchar **source)
{
  int max, len, h, i, j;
  const uchar *count;
  ushort *huff;

  count = (*source += 16) - 17;
  for (max=16; max && !count[max]; max--);
  huff = (ushort *) calloc (1 + (1 << max), sizeof *huff);
  merror (huff, "make_decoder()");
  huff[0] = max;
  for (h=len=1; len <= max; len++)
    for (i=0; i < count[len]; i++, ++*source)
      for (j=0; j < 1 << (max-len); j++)
	if (h <= 1 << max)
	  huff[h++] = len << 8 | **source;
  return huff;
}

ushort * CLASS make_decoder (const uchar *source)
{
  return make_decoder_ref (&source);
}

void CLASS crw_init_tables (unsigned table, ushort *huff[2])
{
  static const uchar first_tree[3][29] = {
    { 0,1,4,2,3,1,2,0,0,0,0,0,0,0,0,0,
      0x04,0x03,0x05,0x06,0x02,0x07,0x01,0x08,0x09,0x00,0x0a,0x0b,0xff  },
    { 0,2,2,3,1,1,1,1,2,0,0,0,0,0,0,0,
      0x03,0x02,0x04,0x01,0x05,0x00,0x06,0x07,0x09,0x08,0x0a,0x0b,0xff  },
    { 0,0,6,3,1,1,2,0,0,0,0,0,0,0,0,0,
      0x06,0x05,0x07,0x04,0x08,0x03,0x09,0x02,0x00,0x0a,0x01,0x0b,0xff  },
  };
  static const uchar second_tree[3][180] = {
    { 0,2,2,2,1,4,2,1,2,5,1,1,0,0,0,139,
      0x03,0x04,0x02,0x05,0x01,0x06,0x07,0x08,
      0x12,0x13,0x11,0x14,0x09,0x15,0x22,0x00,0x21,0x16,0x0a,0xf0,
      0x23,0x17,0x24,0x31,0x32,0x18,0x19,0x33,0x25,0x41,0x34,0x42,
      0x35,0x51,0x36,0x37,0x38,0x29,0x79,0x26,0x1a,0x39,0x56,0x57,
      0x28,0x27,0x52,0x55,0x58,0x43,0x76,0x59,0x77,0x54,0x61,0xf9,
      0x71,0x78,0x75,0x96,0x97,0x49,0xb7,0x53,0xd7,0x74,0xb6,0x98,
      0x47,0x48,0x95,0x69,0x99,0x91,0xfa,0xb8,0x68,0xb5,0xb9,0xd6,
      0xf7,0xd8,0x67,0x46,0x45,0x94,0x89,0xf8,0x81,0xd5,0xf6,0xb4,
      0x88,0xb1,0x2a,0x44,0x72,0xd9,0x87,0x66,0xd4,0xf5,0x3a,0xa7,
      0x73,0xa9,0xa8,0x86,0x62,0xc7,0x65,0xc8,0xc9,0xa1,0xf4,0xd1,
      0xe9,0x5a,0x92,0x85,0xa6,0xe7,0x93,0xe8,0xc1,0xc6,0x7a,0x64,
      0xe1,0x4a,0x6a,0xe6,0xb3,0xf1,0xd3,0xa5,0x8a,0xb2,0x9a,0xba,
      0x84,0xa4,0x63,0xe5,0xc5,0xf3,0xd2,0xc4,0x82,0xaa,0xda,0xe4,
      0xf2,0xca,0x83,0xa3,0xa2,0xc3,0xea,0xc2,0xe2,0xe3,0xff,0xff  },
    { 0,2,2,1,4,1,4,1,3,3,1,0,0,0,0,140,
      0x02,0x03,0x01,0x04,0x05,0x12,0x11,0x06,
      0x13,0x07,0x08,0x14,0x22,0x09,0x21,0x00,0x23,0x15,0x31,0x32,
      0x0a,0x16,0xf0,0x24,0x33,0x41,0x42,0x19,0x17,0x25,0x18,0x51,
      0x34,0x43,0x52,0x29,0x35,0x61,0x39,0x71,0x62,0x36,0x53,0x26,
      0x38,0x1a,0x37,0x81,0x27,0x91,0x79,0x55,0x45,0x28,0x72,0x59,
      0xa1,0xb1,0x44,0x69,0x54,0x58,0xd1,0xfa,0x57,0xe1,0xf1,0xb9,
      0x49,0x47,0x63,0x6a,0xf9,0x56,0x46,0xa8,0x2a,0x4a,0x78,0x99,
      0x3a,0x75,0x74,0x86,0x65,0xc1,0x76,0xb6,0x96,0xd6,0x89,0x85,
      0xc9,0xf5,0x95,0xb4,0xc7,0xf7,0x8a,0x97,0xb8,0x73,0xb7,0xd8,
      0xd9,0x87,0xa7,0x7a,0x48,0x82,0x84,0xea,0xf4,0xa6,0xc5,0x5a,
      0x94,0xa4,0xc6,0x92,0xc3,0x68,0xb5,0xc8,0xe4,0xe5,0xe6,0xe9,
      0xa2,0xa3,0xe3,0xc2,0x66,0x67,0x93,0xaa,0xd4,0xd5,0xe7,0xf8,
      0x88,0x9a,0xd7,0x77,0xc4,0x64,0xe2,0x98,0xa5,0xca,0xda,0xe8,
      0xf3,0xf6,0xa9,0xb2,0xb3,0xf2,0xd2,0x83,0xba,0xd3,0xff,0xff  },
    { 0,0,6,2,1,3,3,2,5,1,2,2,8,10,0,117,
      0x04,0x05,0x03,0x06,0x02,0x07,0x01,0x08,
      0x09,0x12,0x13,0x14,0x11,0x15,0x0a,0x16,0x17,0xf0,0x00,0x22,
      0x21,0x18,0x23,0x19,0x24,0x32,0x31,0x25,0x33,0x38,0x37,0x34,
      0x35,0x36,0x39,0x79,0x57,0x58,0x59,0x28,0x56,0x78,0x27,0x41,
      0x29,0x77,0x26,0x42,0x76,0x99,0x1a,0x55,0x98,0x97,0xf9,0x48,
      0x54,0x96,0x89,0x47,0xb7,0x49,0xfa,0x75,0x68,0xb6,0x67,0x69,
      0xb9,0xb8,0xd8,0x52,0xd7,0x88,0xb5,0x74,0x51,0x46,0xd9,0xf8,
      0x3a,0xd6,0x87,0x45,0x7a,0x95,0xd5,0xf6,0x86,0xb4,0xa9,0x94,
      0x53,0x2a,0xa8,0x43,0xf5,0xf7,0xd4,0x66,0xa7,0x5a,0x44,0x8a,
      0xc9,0xe8,0xc8,0xe7,0x9a,0x6a,0x73,0x4a,0x61,0xc7,0xf4,0xc6,
      0x65,0xe9,0x72,0xe6,0x71,0x91,0x93,0xa6,0xda,0x92,0x85,0x62,
      0xf3,0xc5,0xb2,0xa4,0x84,0xba,0x64,0xa5,0xb3,0xd2,0x81,0xe5,
      0xd3,0xaa,0xc4,0xca,0xf2,0xb1,0xe4,0xd1,0x83,0x63,0xea,0xc3,
      0xe2,0x82,0xf1,0xa3,0xc2,0xa1,0xc1,0xe3,0xa2,0xe1,0xff,0xff  }
  };
  if (table > 2) table = 2;
  huff[0] = make_decoder ( first_tree[table]);
  huff[1] = make_decoder (second_tree[table]);
}

/*
   Return 0 if the image starts with compressed data,
   1 if it starts with uncompressed low-order bits.

   In Canon compressed data, 0xff is always followed by 0x00.
 */
int CLASS canon_has_lowbits()
{
  uchar test[0x4000];
  int ret=1, i;

  fseek (ifp, 0, SEEK_SET);
  fread (test, 1, sizeof test, ifp);
  for (i=540; i < sizeof test - 1; i++)
    if (test[i] == 0xff) {
      if (test[i+1]) return 1;
      ret=0;
    }
  return ret;
}

void CLASS canon_load_raw()
{
  ushort *pixel, *prow, *huff[2];
  int nblocks, lowbits, i, c, row, r, save, val;
  int block, diffbuf[64], leaf, len, diff, carry=0, pnum=0, base[2];

  crw_init_tables (tiff_compress, huff);
  lowbits = canon_has_lowbits();
  if (!lowbits) maximum = 0x3ff;
  fseek (ifp, 540 + lowbits*raw_height*raw_width/4, SEEK_SET);
  zero_after_ff = 1;
  getbits(-1);
  for (row=0; row < raw_height; row+=8) {
    pixel = raw_image + row*raw_width;
    nblocks = MIN (8, raw_height-row) * raw_width >> 6;
    for (block=0; block < nblocks; block++) {
      memset (diffbuf, 0, sizeof diffbuf);
      for (i=0; i < 64; i++ ) {
	leaf = gethuff(huff[i > 0]);
	if (leaf == 0 && i) break;
	if (leaf == 0xff) continue;
	i  += leaf >> 4;
	len = leaf & 15;
	if (len == 0) continue;
	diff = getbits(len);
	if ((diff & (1 << (len-1))) == 0)
	  diff -= (1 << len) - 1;
	if (i < 64) diffbuf[i] = diff;
      }
      diffbuf[0] += carry;
      carry = diffbuf[0];
      for (i=0; i < 64; i++ ) {
	if (pnum++ % raw_width == 0)
	  base[0] = base[1] = 512;
	if ((pixel[(block << 6) + i] = base[i & 1] += diffbuf[i]) >> 10)
	  derror();
      }
    }
    if (lowbits) {
      save = ftell(ifp);
      fseek (ifp, 26 + row*raw_width/4, SEEK_SET);
      for (prow=pixel, i=0; i < raw_width*2; i++) {
	c = fgetc(ifp);
	for (r=0; r < 8; r+=2, prow++) {
	  val = (*prow << 2) + ((c >> r) & 3);
	  if (raw_width == 2672 && val < 512) val += 2;
	  *prow = val;
	}
      }
      fseek (ifp, save, SEEK_SET);
    }
  }
  FORC(2) free (huff[c]);
}
#line 818 "dcraw/dcraw.c"
int CLASS ljpeg_start (struct jhead *jh, int info_only)
{
  int c, tag, len;
  uchar data[0x10000];
  const uchar *dp;

  memset (jh, 0, sizeof *jh);
  jh->restart = INT_MAX;
  fread (data, 2, 1, ifp);
  if (data[1] != 0xd8) return 0;
  do {
    fread (data, 2, 2, ifp);
    tag =  data[0] << 8 | data[1];
    len = (data[2] << 8 | data[3]) - 2;
    if (tag <= 0xff00) return 0;
    fread (data, 1, len, ifp);
    switch (tag) {
      case 0xffc3:
	jh->sraw = ((data[7] >> 4) * (data[7] & 15) - 1) & 3;
      case 0xffc0:
	jh->bits = data[0];
	jh->high = data[1] << 8 | data[2];
	jh->wide = data[3] << 8 | data[4];
	jh->clrs = data[5] + jh->sraw;
	if (len == 9 && !dng_version) getc(ifp);
	break;
      case 0xffc4:
	if (info_only) break;
	for (dp = data; dp < data+len && (c = *dp++) < 4; )
	  jh->free[c] = jh->huff[c] = make_decoder_ref (&dp);
	break;
      case 0xffda:
	jh->psv = data[1+data[0]*2];
	jh->bits -= data[3+data[0]*2] & 15;
	break;
      case 0xffdd:
	jh->restart = data[0] << 8 | data[1];
    }
  } while (tag != 0xffda);
  if (info_only) return 1;
  FORC(5) if (!jh->huff[c+1]) jh->huff[c+1] = jh->huff[c];
  if (jh->sraw) {
    FORC(4)        jh->huff[2+c] = jh->huff[1];
    FORC(jh->sraw) jh->huff[1+c] = jh->huff[0];
  }
  jh->row = (ushort *) calloc (jh->wide*jh->clrs, 4);
  merror (jh->row, "ljpeg_start()");
  return zero_after_ff = 1;
}

void CLASS ljpeg_end (struct jhead *jh)
{
  int c;
  FORC4 if (jh->free[c]) free (jh->free[c]);
  free (jh->row);
}

int CLASS ljpeg_diff (ushort *huff)
{
  int len, diff;

  len = gethuff(huff);
  if (len == 16 && (!dng_version || dng_version >= 0x1010000))
    return -32768;
  diff = getbits(len);
  if ((diff & (1 << (len-1))) == 0)
    diff -= (1 << len) - 1;
  return diff;
}

#ifdef LIBRAW_LIBRARY_BUILD
int CLASS ljpeg_diff_new (LibRaw_bit_buffer& bits, LibRaw_byte_buffer* buf,ushort *huff)
{
  int len, diff;

  len = bits._gethuff_lj(buf,*huff,huff+1);
  if (len == 16 && (!dng_version || dng_version >= 0x1010000))
    return -32768;
  diff = bits._getbits_lj(buf,len);
  if ((diff & (1 << (len-1))) == 0)
    diff -= (1 << len) - 1;
  return diff;
}

int CLASS ljpeg_diff_pef (LibRaw_bit_buffer& bits, LibRaw_byte_buffer* buf,ushort *huff)
{
  int len, diff;

  len = bits._gethuff(buf,*huff,huff+1,zero_after_ff);
  if (len == 16 && (!dng_version || dng_version >= 0x1010000))
    return -32768;
  diff = bits._getbits(buf,len,zero_after_ff);
  if ((diff & (1 << (len-1))) == 0)
    diff -= (1 << len) - 1;
  return diff;
}
#endif

ushort * CLASS ljpeg_row (int jrow, struct jhead *jh)
{
  int col, c, diff, pred, spred=0;
  ushort mark=0, *row[3];

  if (jrow * jh->wide % jh->restart == 0) {
    FORC(6) jh->vpred[c] = 1 << (jh->bits-1);
    if (jrow) {
      fseek (ifp, -2, SEEK_CUR);
      do mark = (mark << 8) + (c = fgetc(ifp));
      while (c != EOF && mark >> 4 != 0xffd);
    }
    getbits(-1);
  }
  FORC3 row[c] = jh->row + jh->wide*jh->clrs*((jrow+c) & 1);
  for (col=0; col < jh->wide; col++)
    FORC(jh->clrs) {
      diff = ljpeg_diff (jh->huff[c]);
      if (jh->sraw && c <= jh->sraw && (col | c))
		    pred = spred;
      else if (col) pred = row[0][-jh->clrs];
      else	    pred = (jh->vpred[c] += diff) - diff;
      if (jrow && col) switch (jh->psv) {
	case 1:	break;
	case 2: pred = row[1][0];					break;
	case 3: pred = row[1][-jh->clrs];				break;
	case 4: pred = pred +   row[1][0] - row[1][-jh->clrs];		break;
	case 5: pred = pred + ((row[1][0] - row[1][-jh->clrs]) >> 1);	break;
	case 6: pred = row[1][0] + ((pred - row[1][-jh->clrs]) >> 1);	break;
	case 7: pred = (pred + row[1][0]) >> 1;				break;
	default: pred = 0;
      }
      if ((**row = pred + diff) >> jh->bits) derror();
      if (c <= jh->sraw) spred = **row;
      row[0]++; row[1]++;
    }
  return row[2];
}
#ifdef LIBRAW_LIBRARY_BUILD
ushort * CLASS ljpeg_row_new (int jrow, struct jhead *jh, LibRaw_bit_buffer& bits,LibRaw_byte_buffer* bytes)
{
  int col, c, diff, pred, spred=0;
  ushort mark=0, *row[3];

  if (jrow * jh->wide % jh->restart == 0) {
    FORC(6) jh->vpred[c] = 1 << (jh->bits-1);
    if (jrow) {
        bytes->unseek2();
        do mark = (mark << 8) + (c = bytes->get_byte());
        while (c != EOF && mark >> 4 != 0xffd);
    }
    bits.reset();
  }
  FORC3 row[c] = jh->row + jh->wide*jh->clrs*((jrow+c) & 1);
  for (col=0; col < jh->wide; col++)
    FORC(jh->clrs) {
        diff = ljpeg_diff_new (bits,bytes,jh->huff[c]);
      if (jh->sraw && c <= jh->sraw && (col | c))
		    pred = spred;
      else if (col) pred = row[0][-jh->clrs];
      else	    pred = (jh->vpred[c] += diff) - diff;
      if (jrow && col) switch (jh->psv) {
	case 1:	break;
	case 2: pred = row[1][0];					break;
	case 3: pred = row[1][-jh->clrs];				break;
	case 4: pred = pred +   row[1][0] - row[1][-jh->clrs];		break;
	case 5: pred = pred + ((row[1][0] - row[1][-jh->clrs]) >> 1);	break;
	case 6: pred = row[1][0] + ((pred - row[1][-jh->clrs]) >> 1);	break;
	case 7: pred = (pred + row[1][0]) >> 1;				break;
	default: pred = 0;
      }
      if ((**row = pred + diff) >> jh->bits) derror();
      if (c <= jh->sraw) spred = **row;
      row[0]++; row[1]++;
    }
  return row[2];
}
#endif

void CLASS lossless_jpeg_load_raw()
{
  int jwide, jrow, jcol, val, jidx, i, j, row=0, col=0;
  struct jhead jh;
  ushort *rp;

  if (!ljpeg_start (&jh, 0)) return;
  jwide = jh.wide * jh.clrs;

  for (jrow=0; jrow < jh.high; jrow++) {
    rp = ljpeg_row (jrow, &jh);
    if (load_flags & 1)
      row = jrow & 1 ? height-1-jrow/2 : jrow/2;
    for (jcol=0; jcol < jwide; jcol++) {
      val = curve[*rp++];
      if (cr2_slice[0]) {
	jidx = jrow*jwide + jcol;
	i = jidx / (cr2_slice[1]*jh.high);
	if ((j = i >= cr2_slice[0]))
		 i  = cr2_slice[0];
	jidx -= i * (cr2_slice[1]*jh.high);
	row = jidx / cr2_slice[1+j];
	col = jidx % cr2_slice[1+j] + i*cr2_slice[1];
      }
      if (raw_width == 3984 && (col -= 2) < 0)
	col += (row--,raw_width);
      if (row >= 0) RAW(row,col) = val;
      if (++col >= raw_width)
	col = (row++,0);
    }
  }
  ljpeg_end (&jh);
}

void CLASS canon_sraw_load_raw()
{
  struct jhead jh;
  short *rp=0, (*ip)[4];
  int jwide, slice, scol, ecol, row, col, jrow=0, jcol=0, pix[3], c;
  int v[3]={0,0,0}, ver, hue;
  char *cp;

  if (!ljpeg_start (&jh, 0)) return;
  jwide = (jh.wide >>= 1) * jh.clrs;

  for (ecol=slice=0; slice <= cr2_slice[0]; slice++) {
    scol = ecol;
    ecol += cr2_slice[1] * 2 / jh.clrs;
    if (!cr2_slice[0] || ecol > raw_width-1) ecol = raw_width & -2;
    for (row=0; row < height; row += (jh.clrs >> 1) - 1) {
      ip = (short (*)[4]) image + row*width;
      for (col=scol; col < ecol; col+=2, jcol+=jh.clrs) {
	if ((jcol %= jwide) == 0)
	  rp = (short *) ljpeg_row (jrow++, &jh);
	if (col >= width) continue;
	FORC (jh.clrs-2)
	  ip[col + (c >> 1)*width + (c & 1)][0] = rp[jcol+c];
	ip[col][1] = rp[jcol+jh.clrs-2] - 16384;
	ip[col][2] = rp[jcol+jh.clrs-1] - 16384;
      }
    }
  }
  for (cp=model2; *cp && !isdigit(*cp); cp++);
  sscanf (cp, "%d.%d.%d", v, v+1, v+2);
  ver = (v[0]*1000 + v[1])*1000 + v[2];
  hue = (jh.sraw+1) << 2;
  if (unique_id >= 0x80000281 || (unique_id == 0x80000218 && ver > 1000006))
    hue = jh.sraw << 1;
  ip = (short (*)[4]) image;
  rp = ip[0];
  for (row=0; row < height; row++, ip+=width) {
    if (row & (jh.sraw >> 1))
      for (col=0; col < width; col+=2)
	for (c=1; c < 3; c++)
	  if (row == height-1)
	       ip[col][c] =  ip[col-width][c];
	  else ip[col][c] = (ip[col-width][c] + ip[col+width][c] + 1) >> 1;
    for (col=1; col < width; col+=2)
      for (c=1; c < 3; c++)
	if (col == width-1)
	     ip[col][c] =  ip[col-1][c];
	else ip[col][c] = (ip[col-1][c] + ip[col+1][c] + 1) >> 1;
  }
  for ( ; rp < ip[0]; rp+=4) {
    if (unique_id < 0x80000218) {
      rp[0] -= 512;
      goto next;
    } else if (unique_id == 0x80000285) {
next: pix[0] = rp[0] + rp[2];
      pix[2] = rp[0] + rp[1];
      pix[1] = rp[0] + ((-778*rp[1] - (rp[2] << 11)) >> 12);
    } else {
      rp[1] = (rp[1] << 2) + hue;
      rp[2] = (rp[2] << 2) + hue;
      pix[0] = rp[0] + ((   50*rp[1] + 22929*rp[2]) >> 14);
      pix[1] = rp[0] + ((-5640*rp[1] - 11751*rp[2]) >> 14);
      pix[2] = rp[0] + ((29040*rp[1] -   101*rp[2]) >> 14);
    }
    FORC3 rp[c] = CLIP(pix[c] * sraw_mul[c] >> 10);
  }
  ljpeg_end (&jh);
  maximum = 0x3fff;
}

void CLASS adobe_copy_pixel (unsigned row, unsigned col, ushort **rp)
{
  int c;

  if (is_raw == 2 && shot_select) (*rp)++;
  if (raw_image) {
    if (row < raw_height && col < raw_width)
      RAW(row,col) = curve[**rp];
    *rp += is_raw;
  } else {
    if (row < height && col < width)
      FORC(tiff_samples)
	image[row*width+col][c] = curve[(*rp)[c]];
    *rp += tiff_samples;
  }
  if (is_raw == 2 && shot_select) (*rp)--;
}

void CLASS lossless_dng_load_raw()
{
  unsigned save, trow=0, tcol=0, jwide, jrow, jcol, row, col;
  struct jhead jh;
  ushort *rp;

  while (trow < raw_height) {
    save = ftell(ifp);
    if (tile_length < INT_MAX)
      fseek (ifp, get4(), SEEK_SET);
    if (!ljpeg_start (&jh, 0)) break;
    jwide = jh.wide;
    if (filters) jwide *= jh.clrs;
    jwide /= is_raw;
    for (row=col=jrow=0; jrow < jh.high; jrow++) {
      rp = ljpeg_row (jrow, &jh);
      for (jcol=0; jcol < jwide; jcol++) {
	adobe_copy_pixel (trow+row, tcol+col, &rp);
	if (++col >= tile_width || col >= raw_width)
	  row += 1 + (col = 0);
      }
    }
    fseek (ifp, save+4, SEEK_SET);
    if ((tcol += tile_width) >= raw_width)
      trow += tile_length + (tcol = 0);
    ljpeg_end (&jh);
  }
}

void CLASS packed_dng_load_raw()
{
  ushort *pixel, *rp;
  int row, col;

  pixel = (ushort *) calloc (raw_width * tiff_samples, sizeof *pixel);
  merror (pixel, "packed_dng_load_raw()");
  for (row=0; row < raw_height; row++) {
    if (tiff_bps == 16)
      read_shorts (pixel, raw_width * tiff_samples);
    else {
      getbits(-1);
      for (col=0; col < raw_width * tiff_samples; col++)
	pixel[col] = getbits(tiff_bps);
    }
    for (rp=pixel, col=0; col < raw_width; col++)
      adobe_copy_pixel (row, col, &rp);
  }
  free (pixel);
}

void CLASS pentax_load_raw()
{
  ushort bit[2][15], huff[4097];
  int dep, row, col, diff, c, i;
  ushort vpred[2][2] = {{0,0},{0,0}}, hpred[2];

  fseek (ifp, meta_offset, SEEK_SET);
  dep = (get2() + 12) & 15;
  fseek (ifp, 12, SEEK_CUR);
  FORC(dep) bit[0][c] = get2();
  FORC(dep) bit[1][c] = fgetc(ifp);
  FORC(dep)
    for (i=bit[0][c]; i <= ((bit[0][c]+(4096 >> bit[1][c])-1) & 4095); )
      huff[++i] = bit[1][c] << 8 | c;
  huff[0] = 12;
  fseek (ifp, data_offset, SEEK_SET);
  getbits(-1);
  for (row=0; row < raw_height; row++)
    for (col=0; col < raw_width; col++) {
      diff = ljpeg_diff (huff);
      if (col < 2) hpred[col] = vpred[row & 1][col] += diff;
      else	   hpred[col & 1] += diff;
      RAW(row,col) = hpred[col & 1];
      if (hpred[col & 1] >> tiff_bps) derror();
    }
}

void CLASS nikon_load_raw()
{
  static const uchar nikon_tree[][32] = {
    { 0,1,5,1,1,1,1,1,1,2,0,0,0,0,0,0,	/* 12-bit lossy */
      5,4,3,6,2,7,1,0,8,9,11,10,12 },
    { 0,1,5,1,1,1,1,1,1,2,0,0,0,0,0,0,	/* 12-bit lossy after split */
      0x39,0x5a,0x38,0x27,0x16,5,4,3,2,1,0,11,12,12 },
    { 0,1,4,2,3,1,2,0,0,0,0,0,0,0,0,0,  /* 12-bit lossless */
      5,4,6,3,7,2,8,1,9,0,10,11,12 },
    { 0,1,4,3,1,1,1,1,1,2,0,0,0,0,0,0,	/* 14-bit lossy */
      5,6,4,7,8,3,9,2,1,0,10,11,12,13,14 },
    { 0,1,5,1,1,1,1,1,1,1,2,0,0,0,0,0,	/* 14-bit lossy after split */
      8,0x5c,0x4b,0x3a,0x29,7,6,5,4,3,2,1,0,13,14 },
    { 0,1,4,2,2,3,1,2,0,0,0,0,0,0,0,0,	/* 14-bit lossless */
      7,6,8,5,9,4,10,3,11,12,2,0,1,13,14 } };
  ushort *huff, ver0, ver1, vpred[2][2], hpred[2], csize;
  int i, min, max, step=0, tree=0, split=0, row, col, len, shl, diff;

  fseek (ifp, meta_offset, SEEK_SET);
  ver0 = fgetc(ifp);
  ver1 = fgetc(ifp);
  if (ver0 == 0x49 || ver1 == 0x58)
    fseek (ifp, 2110, SEEK_CUR);
  if (ver0 == 0x46) tree = 2;
  if (tiff_bps == 14) tree += 3;
  read_shorts (vpred[0], 4);
  max = 1 << tiff_bps & 0x7fff;
  if ((csize = get2()) > 1)
    step = max / (csize-1);
  if (ver0 == 0x44 && ver1 == 0x20 && step > 0) {
    for (i=0; i < csize; i++)
      curve[i*step] = get2();
    for (i=0; i < max; i++)
      curve[i] = ( curve[i-i%step]*(step-i%step) +
		   curve[i-i%step+step]*(i%step) ) / step;
    fseek (ifp, meta_offset+562, SEEK_SET);
    split = get2();
  } else if (ver0 != 0x46 && csize <= 0x4001)
    read_shorts (curve, max=csize);
  while (curve[max-2] == curve[max-1]) max--;
  huff = make_decoder (nikon_tree[tree]);
  fseek (ifp, data_offset, SEEK_SET);
  getbits(-1);
  for (min=row=0; row < height; row++) {
    if (split && row == split) {
      free (huff);
      huff = make_decoder (nikon_tree[tree+1]);
      max += (min = 16) << 1;
    }
    for (col=0; col < raw_width; col++) {
      i = gethuff(huff);
      len = i & 15;
      shl = i >> 4;
      diff = ((getbits(len-shl) << 1) + 1) << shl >> 1;
      if ((diff & (1 << (len-1))) == 0)
	diff -= (1 << len) - !shl;
      if (col < 2) hpred[col] = vpred[row & 1][col] += diff;
      else	   hpred[col & 1] += diff;
      if ((ushort)(hpred[col & 1] + min) >= max) derror();
      RAW(row,col) = curve[LIM((short)hpred[col & 1],0,0x3fff)];
    }
  }
  free (huff);
}

/*
   Figure out if a NEF file is compressed.  These fancy heuristics
   are only needed for the D100, thanks to a bug in some cameras
   that tags all images as "compressed".
 */
int CLASS nikon_is_compressed()
{
  uchar test[256];
  int i;

  fseek (ifp, data_offset, SEEK_SET);
  fread (test, 1, 256, ifp);
  for (i=15; i < 256; i+=16)
    if (test[i]) return 1;
  return 0;
}

/*
   Returns 1 for a Coolpix 995, 0 for anything else.
 */
int CLASS nikon_e995()
{
  int i, histo[256];
  const uchar often[] = { 0x00, 0x55, 0xaa, 0xff };

  memset (histo, 0, sizeof histo);
  fseek (ifp, -2000, SEEK_END);
  for (i=0; i < 2000; i++)
    histo[fgetc(ifp)]++;
  for (i=0; i < 4; i++)
    if (histo[often[i]] < 200)
      return 0;
  return 1;
}

/*
   Returns 1 for a Coolpix 2100, 0 for anything else.
 */
int CLASS nikon_e2100()
{
  uchar t[12];
  int i;

  fseek (ifp, 0, SEEK_SET);
  for (i=0; i < 1024; i++) {
    fread (t, 1, 12, ifp);
    if (((t[2] & t[4] & t[7] & t[9]) >> 4
	& t[1] & t[6] & t[8] & t[11] & 3) != 3)
      return 0;
  }
  return 1;
}

void CLASS nikon_3700()
{
  int bits, i;
  uchar dp[24];
  static const struct {
    int bits;
    char t_make[12], t_model[15];
  } table[] = {
    { 0x00, "PENTAX",  "Optio 33WR" },
    { 0x03, "NIKON",   "E3200" },
    { 0x32, "NIKON",   "E3700" },
    { 0x33, "OLYMPUS", "C740UZ" } };

  fseek (ifp, 3072, SEEK_SET);
  fread (dp, 1, 24, ifp);
  bits = (dp[8] & 3) << 4 | (dp[20] & 3);
  for (i=0; i < sizeof table / sizeof *table; i++)
    if (bits == table[i].bits) {
      strcpy (make,  table[i].t_make );
      strcpy (model, table[i].t_model);
    }
}

/*
   Separates a Minolta DiMAGE Z2 from a Nikon E4300.
 */
int CLASS minolta_z2()
{
  int i, nz;
  char tail[424];

  fseek (ifp, -sizeof tail, SEEK_END);
  fread (tail, 1, sizeof tail, ifp);
  for (nz=i=0; i < sizeof tail; i++)
    if (tail[i]) nz++;
  return nz > 20;
}
#line 1354 "dcraw/dcraw.c"
void CLASS ppm_thumb()
{
  char *thumb;
  thumb_length = thumb_width*thumb_height*3;
  thumb = (char *) malloc (thumb_length);
  merror (thumb, "ppm_thumb()");
  fprintf (ofp, "P6\n%d %d\n255\n", thumb_width, thumb_height);
  fread  (thumb, 1, thumb_length, ifp);
  fwrite (thumb, 1, thumb_length, ofp);
  free (thumb);
}

void CLASS ppm16_thumb()
{
  int i;
  char *thumb;
  thumb_length = thumb_width*thumb_height*3;
  thumb = (char *) calloc (thumb_length,2);
  merror (thumb, "ppm16_thumb()");
  read_shorts ((ushort *) thumb, thumb_length);
  for (i=0; i < thumb_length; i++)
    thumb[i] = ((ushort *) thumb)[i] >> 8;
  fprintf (ofp, "P6\n%d %d\n255\n", thumb_width, thumb_height);
  fwrite (thumb, 1, thumb_length, ofp);
  free (thumb);
}

void CLASS layer_thumb()
{
  int i, c;
  char *thumb, map[][4] = { "012","102" };

  colors = thumb_misc >> 5 & 7;
  thumb_length = thumb_width*thumb_height;
  thumb = (char *) calloc (colors, thumb_length);
  merror (thumb, "layer_thumb()");
  fprintf (ofp, "P%d\n%d %d\n255\n",
	5 + (colors >> 1), thumb_width, thumb_height);
  fread (thumb, thumb_length, colors, ifp);
  for (i=0; i < thumb_length; i++)
    FORCC putc (thumb[i+thumb_length*(map[thumb_misc >> 8][c]-'0')], ofp);
  free (thumb);
}

void CLASS rollei_thumb()
{
  unsigned i;
  ushort *thumb;

  thumb_length = thumb_width * thumb_height;
  thumb = (ushort *) calloc (thumb_length, 2);
  merror (thumb, "rollei_thumb()");
  fprintf (ofp, "P6\n%d %d\n255\n", thumb_width, thumb_height);
  read_shorts (thumb, thumb_length);
  for (i=0; i < thumb_length; i++) {
    putc (thumb[i] << 3, ofp);
    putc (thumb[i] >> 5  << 2, ofp);
    putc (thumb[i] >> 11 << 3, ofp);
  }
  free (thumb);
}

void CLASS rollei_load_raw()
{
  uchar pixel[10];
  unsigned iten=0, isix, i, buffer=0, todo[16];

  isix = raw_width * raw_height * 5 / 8;
  while (fread (pixel, 1, 10, ifp) == 10) {
    for (i=0; i < 10; i+=2) {
      todo[i]   = iten++;
      todo[i+1] = pixel[i] << 8 | pixel[i+1];
      buffer    = pixel[i] >> 2 | buffer << 6;
    }
    for (   ; i < 16; i+=2) {
      todo[i]   = isix++;
      todo[i+1] = buffer >> (14-i)*5;
    }
    for (i=0; i < 16; i+=2)
      raw_image[todo[i]] = (todo[i+1] & 0x3ff);
  }
  maximum = 0x3ff;
}

int CLASS raw (unsigned row, unsigned col)
{
  return (row < raw_height && col < raw_width) ? RAW(row,col) : 0;
}

void CLASS phase_one_flat_field (int is_float, int nc)
{
  ushort head[8];
  unsigned wide, y, x, c, rend, cend, row, col;
  float *mrow, num, mult[4];

  read_shorts (head, 8);
  wide = head[2] / head[4];
  mrow = (float *) calloc (nc*wide, sizeof *mrow);
  merror (mrow, "phase_one_flat_field()");
  for (y=0; y < head[3] / head[5]; y++) {
    for (x=0; x < wide; x++)
      for (c=0; c < nc; c+=2) {
	num = is_float ? getreal(11) : get2()/32768.0;
	if (y==0) mrow[c*wide+x] = num;
	else mrow[(c+1)*wide+x] = (num - mrow[c*wide+x]) / head[5];
      }
    if (y==0) continue;
    rend = head[1] + y*head[5];
    for (row = rend-head[5]; row < raw_height && row < rend; row++) {
      for (x=1; x < wide; x++) {
	for (c=0; c < nc; c+=2) {
	  mult[c] = mrow[c*wide+x-1];
	  mult[c+1] = (mrow[c*wide+x] - mult[c]) / head[4];
	}
	cend = head[0] + x*head[4];
	for (col = cend-head[4]; col < raw_width && col < cend; col++) {
	  c = nc > 2 ? FC(row-top_margin,col-left_margin) : 0;
	  if (!(c & 1)) {
	    c = RAW(row,col) * mult[c];
	    RAW(row,col) = LIM(c,0,65535);
	  }
	  for (c=0; c < nc; c+=2)
	    mult[c] += mult[c+1];
	}
      }
      for (x=0; x < wide; x++)
	for (c=0; c < nc; c+=2)
	  mrow[c*wide+x] += mrow[(c+1)*wide+x];
    }
  }
  free (mrow);
}

void CLASS phase_one_correct()
{
  unsigned entries, tag, data, save, col, row, type;
  int len, i, j, k, cip, val[4], dev[4], sum, max;
  int head[9], diff, mindiff=INT_MAX, off_412=0;
  static const signed char dir[12][2] =
    { {-1,-1}, {-1,1}, {1,-1}, {1,1}, {-2,0}, {0,-2}, {0,2}, {2,0},
      {-2,-2}, {-2,2}, {2,-2}, {2,2} };
  float poly[8], num, cfrac, frac, mult[2], *yval[2];
  ushort *xval[2];

  if (half_size || !meta_length) return;
  if (verbose) fprintf (stderr,_("Phase One correction...\n"));
  fseek (ifp, meta_offset, SEEK_SET);
  order = get2();
  fseek (ifp, 6, SEEK_CUR);
  fseek (ifp, meta_offset+get4(), SEEK_SET);
  entries = get4();  get4();
  while (entries--) {
    tag  = get4();
    len  = get4();
    data = get4();
    save = ftell(ifp);
    fseek (ifp, meta_offset+data, SEEK_SET);
    if (tag == 0x419) {				/* Polynomial curve */
      for (get4(), i=0; i < 8; i++)
	poly[i] = getreal(11);
      poly[3] += (ph1.tag_210 - poly[7]) * poly[6] + 1;
      for (i=0; i < 0x10000; i++) {
	num = (poly[5]*i + poly[3])*i + poly[1];
	curve[i] = LIM(num,0,65535);
      } goto apply;				/* apply to right half */
    } else if (tag == 0x41a) {			/* Polynomial curve */
      for (i=0; i < 4; i++)
	poly[i] = getreal(11);
      for (i=0; i < 0x10000; i++) {
	for (num=0, j=4; j--; )
	  num = num * i + poly[j];
	curve[i] = LIM(num+i,0,65535);
      } apply:					/* apply to whole image */
      for (row=0; row < raw_height; row++)
	for (col = (tag & 1)*ph1.split_col; col < raw_width; col++)
	  RAW(row,col) = curve[RAW(row,col)];
    } else if (tag == 0x400) {			/* Sensor defects */
      while ((len -= 8) >= 0) {
	col  = get2();
	row  = get2();
	type = get2(); get2();
	if (col >= raw_width) continue;
	if (type == 131)			/* Bad column */
	  for (row=0; row < raw_height; row++)
	    if (FC(row-top_margin,col-left_margin) == 1) {
	      for (sum=i=0; i < 4; i++)
		sum += val[i] = raw (row+dir[i][0], col+dir[i][1]);
	      for (max=i=0; i < 4; i++) {
		dev[i] = abs((val[i] << 2) - sum);
		if (dev[max] < dev[i]) max = i;
	      }
	      RAW(row,col) = (sum - val[max])/3.0 + 0.5;
	    } else {
	      for (sum=0, i=8; i < 12; i++)
		sum += raw (row+dir[i][0], col+dir[i][1]);
	      RAW(row,col) = 0.5 + sum * 0.0732233 +
		(raw(row,col-2) + raw(row,col+2)) * 0.3535534;
	    }
	else if (type == 129) {			/* Bad pixel */
	  if (row >= raw_height) continue;
	  j = (FC(row-top_margin,col-left_margin) != 1) * 4;
	  for (sum=0, i=j; i < j+8; i++)
	    sum += raw (row+dir[i][0], col+dir[i][1]);
	  RAW(row,col) = (sum + 4) >> 3;
	}
      }
    } else if (tag == 0x401) {			/* All-color flat fields */
      phase_one_flat_field (1, 2);
    } else if (tag == 0x416 || tag == 0x410) {
      phase_one_flat_field (0, 2);
    } else if (tag == 0x40b) {			/* Red+blue flat field */
      phase_one_flat_field (0, 4);
    } else if (tag == 0x412) {
      fseek (ifp, 36, SEEK_CUR);
      diff = abs (get2() - ph1.tag_21a);
      if (mindiff > diff) {
	mindiff = diff;
	off_412 = ftell(ifp) - 38;
      }
    }
    fseek (ifp, save, SEEK_SET);
  }
  if (off_412) {
    fseek (ifp, off_412, SEEK_SET);
    for (i=0; i < 9; i++) head[i] = get4() & 0x7fff;
    yval[0] = (float *) calloc (head[1]*head[3] + head[2]*head[4], 6);
    merror (yval[0], "phase_one_correct()");
    yval[1] = (float  *) (yval[0] + head[1]*head[3]);
    xval[0] = (ushort *) (yval[1] + head[2]*head[4]);
    xval[1] = (ushort *) (xval[0] + head[1]*head[3]);
    get2();
    for (i=0; i < 2; i++)
      for (j=0; j < head[i+1]*head[i+3]; j++)
	yval[i][j] = getreal(11);
    for (i=0; i < 2; i++)
      for (j=0; j < head[i+1]*head[i+3]; j++)
	xval[i][j] = get2();
    for (row=0; row < raw_height; row++)
      for (col=0; col < raw_width; col++) {
	cfrac = (float) col * head[3] / raw_width;
	cfrac -= cip = cfrac;
	num = RAW(row,col) * 0.5;
	for (i=cip; i < cip+2; i++) {
	  for (k=j=0; j < head[1]; j++)
	    if (num < xval[0][k = head[1]*i+j]) break;
	  frac = (j == 0 || j == head[1]) ? 0 :
		(xval[0][k] - num) / (xval[0][k] - xval[0][k-1]);
	  mult[i-cip] = yval[0][k-1] * frac + yval[0][k] * (1-frac);
	}
	i = ((mult[0] * (1-cfrac) + mult[1] * cfrac) * row + num) * 2;
	RAW(row,col) = LIM(i,0,65535);
      }
    free (yval[0]);
  }
}

void CLASS phase_one_load_raw()
{
  int a, b, i;
  ushort akey, bkey, t_mask;

  fseek (ifp, ph1.key_off, SEEK_SET);
  akey = get2();
  bkey = get2();
  t_mask = ph1.format == 1 ? 0x5555:0x1354;
  fseek (ifp, data_offset, SEEK_SET);
  read_shorts (raw_image, raw_width*raw_height);
  if (ph1.format)
    for (i=0; i < raw_width*raw_height; i+=2) {
      a = raw_image[i+0] ^ akey;
      b = raw_image[i+1] ^ bkey;
      raw_image[i+0] = (a & t_mask) | (b & ~t_mask);
      raw_image[i+1] = (b & t_mask) | (a & ~t_mask);
    }
}

unsigned CLASS ph1_bithuff (int nbits, ushort *huff)
{
#ifndef LIBRAW_NOTHREADS
#define bitbuf tls->ph1_bits.bitbuf
#define vbits  tls->ph1_bits.vbits    
#else
  static UINT64 bitbuf=0;
  static int vbits=0;
#endif
  unsigned c;

  if (nbits == -1)
    return bitbuf = vbits = 0;
  if (nbits == 0) return 0;
  if (vbits < nbits) {
    bitbuf = bitbuf << 32 | get4();
    vbits += 32;
  }
  c = bitbuf << (64-vbits) >> (64-nbits);
  if (huff) {
    vbits -= huff[c] >> 8;
    return (uchar) huff[c];
  }
  vbits -= nbits;
  return c;
#ifndef LIBRAW_NOTHREADS
#undef bitbuf
#undef vbits
#endif
}
#define ph1_bits(n) ph1_bithuff(n,0)
#define ph1_huff(h) ph1_bithuff(*h,h+1)

void CLASS phase_one_load_raw_c()
{
  static const int length[] = { 8,7,6,9,11,10,5,12,14,13 };
  int *offset, len[2], pred[2], row, col, i, j;
  ushort *pixel;
  short (*t_black)[2];

  pixel = (ushort *) calloc (raw_width + raw_height*4, 2);
  merror (pixel, "phase_one_load_raw_c()");
  offset = (int *) (pixel + raw_width);
  fseek (ifp, strip_offset, SEEK_SET);
  for (row=0; row < raw_height; row++)
    offset[row] = get4();
  t_black = (short (*)[2]) offset + raw_height;
  fseek (ifp, ph1.black_off, SEEK_SET);
  if (ph1.black_off)
      {
          read_shorts ((ushort *) t_black[0], raw_height*2);
#ifdef LIBRAW_LIBRARY_BUILD
          imgdata.rawdata.ph1_black = (short (*)[2])calloc(raw_height*2,sizeof(short));
          merror (imgdata.rawdata.ph1_black, "phase_one_load_raw_c()");
          memmove(imgdata.rawdata.ph1_black,(short *) t_black[0],raw_height*2*sizeof(short));
#endif
      }
  for (i=0; i < 256; i++)
    curve[i] = i*i / 3.969 + 0.5;
  for (row=0; row < raw_height; row++) {
    fseek (ifp, data_offset + offset[row], SEEK_SET);
    ph1_bits(-1);
    pred[0] = pred[1] = 0;
    for (col=0; col < raw_width; col++) {
      if (col >= (raw_width & -8))
	len[0] = len[1] = 14;
      else if ((col & 7) == 0)
	for (i=0; i < 2; i++) {
	  for (j=0; j < 5 && !ph1_bits(1); j++);
	  if (j--) len[i] = length[j*2 + ph1_bits(1)];
	}
      if ((i = len[col & 1]) == 14)
	pixel[col] = pred[col & 1] = ph1_bits(16);
      else
	pixel[col] = pred[col & 1] += ph1_bits(i) + 1 - (1 << (i - 1));
      if (pred[col & 1] >> 16) derror();
      if (ph1.format == 5 && pixel[col] < 256)
	pixel[col] = curve[pixel[col]];
    }
    for (col=0; col < raw_width; col++) {
      i = (pixel[col] << 2) - ph1.t_black + t_black[row][col >= ph1.split_col];
	if (i > 0) RAW(row,col) = i;
    }
  }
  free (pixel);
  maximum = 0xfffc - ph1.t_black;
}

void CLASS hasselblad_load_raw()
{
  struct jhead jh;
  int row, col, pred[2], len[2], diff, c;

  if (!ljpeg_start (&jh, 0)) return;
  order = 0x4949;
  ph1_bits(-1);
  for (row=0; row < raw_height; row++) {
    pred[0] = pred[1] = 0x8000 + load_flags;
    for (col=0; col < raw_width; col+=2) {
      FORC(2) len[c] = ph1_huff(jh.huff[0]);
      FORC(2) {
	diff = ph1_bits(len[c]);
	if ((diff & (1 << (len[c]-1))) == 0)
	  diff -= (1 << len[c]) - 1;
	if (diff == 65535) diff = -32768;
	pred[c] += diff;
	if (row >= 0 && (unsigned)(col+c) < width)
	  RAW(row,col+c) = pred[c];
      }
    }
  }
  ljpeg_end (&jh);
  maximum = 0xffff;
}

void CLASS leaf_hdr_load_raw()
{
  ushort *pixel=0;
  unsigned tile=0, r, c, row, col;

  if (!filters) {
    pixel = (ushort *) calloc (raw_width, sizeof *pixel);
    merror (pixel, "leaf_hdr_load_raw()");
  }
  FORC(tiff_samples)
    for (r=0; r < raw_height; r++) {
      if (r % tile_length == 0) {
	fseek (ifp, data_offset + 4*tile++, SEEK_SET);
	fseek (ifp, get4(), SEEK_SET);
      }
      if (filters && c != shot_select) continue;
      if (filters) pixel = raw_image + r*raw_width;
      read_shorts (pixel, raw_width);
      if (!filters && (row = r - top_margin) < height)
	for (col=0; col < width; col++)
	  image[row*width+col][c] = pixel[col+left_margin];
    }
  if (!filters) {
    maximum = 0xffff;
    raw_color = 1;
    free (pixel);
  }
}

void CLASS unpacked_load_raw()
{
  int row, col, bits=0;

  while (1 << ++bits < maximum);
  read_shorts (raw_image, raw_width*raw_height);
  for (row=0; row < raw_height; row++)
    for (col=0; col < raw_width; col++)
      if ((RAW(row,col) >>= load_flags) >> bits
	&& (unsigned) (row-top_margin) < height
	&& (unsigned) (col-left_margin) < width) derror();
}

void CLASS sinar_4shot_load_raw()
{
  ushort *pixel;
  unsigned shot, row, col, r, c;

  if ((shot = shot_select) || half_size) {
    if (shot) shot--;
    if (shot > 3) shot = 3;
    fseek (ifp, data_offset + shot*4, SEEK_SET);
    fseek (ifp, get4(), SEEK_SET);
    unpacked_load_raw();
    return;
  }
  free (raw_image);
  raw_image = 0;
  free (image);
  image = (ushort (*)[4])
	calloc ((iheight=height)*(iwidth=width), sizeof *image);
  merror (image, "sinar_4shot_load_raw()");
  pixel = (ushort *) calloc (raw_width, sizeof *pixel);
  merror (pixel, "sinar_4shot_load_raw()");
  for (shot=0; shot < 4; shot++) {
    fseek (ifp, data_offset + shot*4, SEEK_SET);
    fseek (ifp, get4(), SEEK_SET);
    for (row=0; row < raw_height; row++) {
      read_shorts (pixel, raw_width);
      if ((r = row-top_margin - (shot >> 1 & 1)) >= height) continue;
      for (col=0; col < raw_width; col++) {
	if ((c = col-left_margin - (shot & 1)) >= width) continue;
	image[r*width+c][FC(row,col)] = pixel[col];
      }
    }
  }
  free (pixel);
  shrink = filters = 0;
}

void CLASS imacon_full_load_raw()
{
  int row, col;

  for (row=0; row < height; row++)
    for (col=0; col < width; col++)
      read_shorts (image[row*width+col], 3);
}

void CLASS packed_load_raw()
{
  int vbits=0, bwide, pwide, rbits, bite, half, irow, row, col, val, i;
  UINT64 bitbuf=0;

  if (raw_width * 8 >= width * tiff_bps)	/* Is raw_width in bytes? */
       pwide = (bwide = raw_width) * 8 / tiff_bps;
  else bwide = (pwide = raw_width) * tiff_bps / 8;
  rbits = bwide * 8 - pwide * tiff_bps;
  if (load_flags & 1) bwide = bwide * 16 / 15;
  bite = 8 + (load_flags & 24);
  half = (raw_height+1) >> 1;
  for (irow=0; irow < raw_height; irow++) {
    row = irow;
    if (load_flags & 2 &&
	(row = irow % half * 2 + irow / half) == 1 &&
	load_flags & 4) {
      if (vbits=0, tiff_compress)
	fseek (ifp, data_offset - (-half*bwide & -2048), SEEK_SET);
      else {
	fseek (ifp, 0, SEEK_END);
	fseek (ifp, ftell(ifp) >> 3 << 2, SEEK_SET);
      }
    }
    for (col=0; col < pwide; col++) {
      for (vbits -= tiff_bps; vbits < 0; vbits += bite) {
	bitbuf <<= bite;
	for (i=0; i < bite; i+=8)
	  bitbuf |= (unsigned) (fgetc(ifp) << i);
      }
      val = bitbuf << (64-tiff_bps-vbits) >> (64-tiff_bps);
      RAW(row,col ^ (load_flags >> 6)) = val;
      if (load_flags & 1 && (col % 10) == 9 &&
	fgetc(ifp) && col < width+left_margin) derror();
    }
    vbits -= rbits;
  }
}

void CLASS nokia_load_raw()
{
  uchar  *data,  *dp;
  int rev, dwide, row, col, c;

  rev = 3 * (order == 0x4949);
  dwide = raw_width * 5 / 4;
  data = (uchar *) malloc (dwide*2);
  merror (data, "nokia_load_raw()");
  for (row=0; row < raw_height; row++) {
    if (fread (data+dwide, 1, dwide, ifp) < dwide) derror();
    FORC(dwide) data[c] = data[dwide+(c ^ rev)];
    for (dp=data, col=0; col < raw_width; dp+=5, col+=4)
      FORC4 RAW(row,col+c) = (dp[c] << 2) | (dp[4] >> (c << 1) & 3);
  }
  free (data);
  maximum = 0x3ff;
}

unsigned CLASS pana_bits (int nbits)
{
#ifndef LIBRAW_NOTHREADS
#define buf tls->pana_bits.buf
#define vbits tls->pana_bits.vbits   
#else
  static uchar buf[0x4000];
  static int vbits;
#endif
  int byte;

  if (!nbits) return vbits=0;
  if (!vbits) {
    fread (buf+load_flags, 1, 0x4000-load_flags, ifp);
    fread (buf, 1, load_flags, ifp);
  }
  vbits = (vbits - nbits) & 0x1ffff;
  byte = vbits >> 3 ^ 0x3ff0;
  return (buf[byte] | buf[byte+1] << 8) >> (vbits & 7) & ~(-1 << nbits);
#ifndef LIBRAW_NOTHREADS
#undef buf
#undef vbits
#endif
}

void CLASS panasonic_load_raw()
{
  int row, col, i, j, sh=0, pred[2], nonz[2];

  pana_bits(0);
  for (row=0; row < height; row++)
    for (col=0; col < raw_width; col++) {
      if ((i = col % 14) == 0)
	pred[0] = pred[1] = nonz[0] = nonz[1] = 0;
      if (i % 3 == 2) sh = 4 >> (3 - pana_bits(2));
      if (nonz[i & 1]) {
	if ((j = pana_bits(8))) {
	  if ((pred[i & 1] -= 0x80 << sh) < 0 || sh == 4)
	       pred[i & 1] &= ~(-1 << sh);
	  pred[i & 1] += j << sh;
	}
      } else if ((nonz[i & 1] = pana_bits(8)) || i > 11)
	pred[i & 1] = nonz[i & 1] << 4 | pana_bits(4);
      if (col < width)
	if ((RAW(row,col) = pred[col & 1]) > 4098) derror();
    }
}

void CLASS olympus_load_raw()
{
  ushort huff[4096];
  int row, col, nbits, sign, low, high, i, c, w, n, nw;
  int acarry[2][3], *carry, pred, diff;

  huff[n=0] = 0xc0c;
  for (i=12; i--; )
    FORC(2048 >> i) huff[++n] = (i+1) << 8 | i;
  fseek (ifp, 7, SEEK_CUR);
  getbits(-1);
  for (row=0; row < height; row++) {
    memset (acarry, 0, sizeof acarry);
    for (col=0; col < raw_width; col++) {
      carry = acarry[col & 1];
      i = 2 * (carry[2] < 3);
      for (nbits=2+i; (ushort) carry[0] >> (nbits+i); nbits++);
      low = (sign = getbits(3)) & 3;
      sign = sign << 29 >> 31;
      if ((high = getbithuff(12,huff)) == 12)
	high = getbits(16-nbits) >> 1;
      carry[0] = (high << nbits) | getbits(nbits);
      diff = (carry[0] ^ sign) + carry[1];
      carry[1] = (diff*3 + carry[1]) >> 5;
      carry[2] = carry[0] > 16 ? 0 : carry[2]+1;
      if (col >= width) continue;
      if (row < 2 && col < 2) pred = 0;
      else if (row < 2) pred = RAW(row,col-2);
      else if (col < 2) pred = RAW(row-2,col);
      else {
	w  = RAW(row,col-2);
	n  = RAW(row-2,col);
	nw = RAW(row-2,col-2);
	if ((w < nw && nw < n) || (n < nw && nw < w)) {
	  if (ABS(w-nw) > 32 || ABS(n-nw) > 32)
	    pred = w + n - nw;
	  else pred = (w + n) >> 1;
	} else pred = ABS(w-nw) > ABS(n-nw) ? w : n;
      }
      if ((RAW(row,col) = pred + ((diff << 2) | low)) >> 12) derror();
    }
  }
}

void CLASS minolta_rd175_load_raw()
{
  uchar pixel[768];
  unsigned irow, box, row, col;

  for (irow=0; irow < 1481; irow++) {
    if (fread (pixel, 1, 768, ifp) < 768) derror();
    box = irow / 82;
    row = irow % 82 * 12 + ((box < 12) ? box | 1 : (box-12)*2);
    switch (irow) {
      case 1477: case 1479: continue;
      case 1476: row = 984; break;
      case 1480: row = 985; break;
      case 1478: row = 985; box = 1;
    }
    if ((box < 12) && (box & 1)) {
      for (col=0; col < 1533; col++, row ^= 1)
	if (col != 1) RAW(row,col) = (col+1) & 2 ?
		   pixel[col/2-1] + pixel[col/2+1] : pixel[col/2] << 1;
      RAW(row,1)    = pixel[1]   << 1;
      RAW(row,1533) = pixel[765] << 1;
    } else
      for (col=row & 1; col < 1534; col+=2)
	RAW(row,col) = pixel[col/2] << 1;
  }
  maximum = 0xff << 1;
}

void CLASS quicktake_100_load_raw()
{
  uchar pixel[484][644];
  static const short gstep[16] =
  { -89,-60,-44,-32,-22,-15,-8,-2,2,8,15,22,32,44,60,89 };
  static const short rstep[6][4] =
  { {  -3,-1,1,3  }, {  -5,-1,1,5  }, {  -8,-2,2,8  },
    { -13,-3,3,13 }, { -19,-4,4,19 }, { -28,-6,6,28 } };
  static const short t_curve[256] =
  { 0,1,2,3,4,5,6,7,8,9,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,
    28,29,30,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,53,
    54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,74,75,76,77,78,
    79,80,81,82,83,84,86,88,90,92,94,97,99,101,103,105,107,110,112,114,116,
    118,120,123,125,127,129,131,134,136,138,140,142,144,147,149,151,153,155,
    158,160,162,164,166,168,171,173,175,177,179,181,184,186,188,190,192,195,
    197,199,201,203,205,208,210,212,214,216,218,221,223,226,230,235,239,244,
    248,252,257,261,265,270,274,278,283,287,291,296,300,305,309,313,318,322,
    326,331,335,339,344,348,352,357,361,365,370,374,379,383,387,392,396,400,
    405,409,413,418,422,426,431,435,440,444,448,453,457,461,466,470,474,479,
    483,487,492,496,500,508,519,531,542,553,564,575,587,598,609,620,631,643,
    654,665,676,687,698,710,721,732,743,754,766,777,788,799,810,822,833,844,
    855,866,878,889,900,911,922,933,945,956,967,978,989,1001,1012,1023 };
  int rb, row, col, sharp, val=0;

  getbits(-1);
  memset (pixel, 0x80, sizeof pixel);
  for (row=2; row < height+2; row++) {
    for (col=2+(row & 1); col < width+2; col+=2) {
      val = ((pixel[row-1][col-1] + 2*pixel[row-1][col+1] +
		pixel[row][col-2]) >> 2) + gstep[getbits(4)];
      pixel[row][col] = val = LIM(val,0,255);
      if (col < 4)
	pixel[row][col-2] = pixel[row+1][~row & 1] = val;
      if (row == 2)
	pixel[row-1][col+1] = pixel[row-1][col+3] = val;
    }
    pixel[row][col] = val;
  }
  for (rb=0; rb < 2; rb++)
    for (row=2+rb; row < height+2; row+=2)
      for (col=3-(row & 1); col < width+2; col+=2) {
	if (row < 4 || col < 4) sharp = 2;
	else {
	  val = ABS(pixel[row-2][col] - pixel[row][col-2])
	      + ABS(pixel[row-2][col] - pixel[row-2][col-2])
	      + ABS(pixel[row][col-2] - pixel[row-2][col-2]);
	  sharp = val <  4 ? 0 : val <  8 ? 1 : val < 16 ? 2 :
		  val < 32 ? 3 : val < 48 ? 4 : 5;
	}
	val = ((pixel[row-2][col] + pixel[row][col-2]) >> 1)
	      + rstep[sharp][getbits(2)];
	pixel[row][col] = val = LIM(val,0,255);
	if (row < 4) pixel[row-2][col+2] = val;
	if (col < 4) pixel[row+2][col-2] = val;
      }
  for (row=2; row < height+2; row++)
    for (col=3-(row & 1); col < width+2; col+=2) {
      val = ((pixel[row][col-1] + (pixel[row][col] << 2) +
	      pixel[row][col+1]) >> 1) - 0x100;
      pixel[row][col] = LIM(val,0,255);
    }
  for (row=0; row < height; row++)
    for (col=0; col < width; col++)
      RAW(row,col) = t_curve[pixel[row+2][col+2]];
  maximum = 0x3ff;
}

#define radc_token(tree) ((signed char) getbithuff(8,huff[tree]))

#define FORYX for (y=1; y < 3; y++) for (x=col+1; x >= col; x--)

#define PREDICTOR (c ? (buf[c][y-1][x] + buf[c][y][x+1]) / 2 \
: (buf[c][y-1][x+1] + 2*buf[c][y-1][x] + buf[c][y][x+1]) / 4)

void CLASS kodak_radc_load_raw()
{
  static const char src[] = {
    1,1, 2,3, 3,4, 4,2, 5,7, 6,5, 7,6, 7,8,
    1,0, 2,1, 3,3, 4,4, 5,2, 6,7, 7,6, 8,5, 8,8,
    2,1, 2,3, 3,0, 3,2, 3,4, 4,6, 5,5, 6,7, 6,8,
    2,0, 2,1, 2,3, 3,2, 4,4, 5,6, 6,7, 7,5, 7,8,
    2,1, 2,4, 3,0, 3,2, 3,3, 4,7, 5,5, 6,6, 6,8,
    2,3, 3,1, 3,2, 3,4, 3,5, 3,6, 4,7, 5,0, 5,8,
    2,3, 2,6, 3,0, 3,1, 4,4, 4,5, 4,7, 5,2, 5,8,
    2,4, 2,7, 3,3, 3,6, 4,1, 4,2, 4,5, 5,0, 5,8,
    2,6, 3,1, 3,3, 3,5, 3,7, 3,8, 4,0, 5,2, 5,4,
    2,0, 2,1, 3,2, 3,3, 4,4, 4,5, 5,6, 5,7, 4,8,
    1,0, 2,2, 2,-2,
    1,-3, 1,3,
    2,-17, 2,-5, 2,5, 2,17,
    2,-7, 2,2, 2,9, 2,18,
    2,-18, 2,-9, 2,-2, 2,7,
    2,-28, 2,28, 3,-49, 3,-9, 3,9, 4,49, 5,-79, 5,79,
    2,-1, 2,13, 2,26, 3,39, 4,-16, 5,55, 6,-37, 6,76,
    2,-26, 2,-13, 2,1, 3,-39, 4,16, 5,-55, 6,-76, 6,37
  };
  ushort huff[19][256];
  int row, col, tree, nreps, rep, step, i, c, s, r, x, y, val;
  short last[3] = { 16,16,16 }, mul[3], buf[3][3][386];
  static const ushort pt[] =
    { 0,0, 1280,1344, 2320,3616, 3328,8000, 4095,16383, 65535,16383 };

  for (i=2; i < 12; i+=2)
    for (c=pt[i-2]; c <= pt[i]; c++)
      curve[c] = (float)
	(c-pt[i-2]) / (pt[i]-pt[i-2]) * (pt[i+1]-pt[i-1]) + pt[i-1] + 0.5;
  for (s=i=0; i < sizeof src; i+=2)
    FORC(256 >> src[i])
      huff[0][s++] = src[i] << 8 | (uchar) src[i+1];
  s = kodak_cbpp == 243 ? 2 : 3;
  FORC(256) huff[18][c] = (8-s) << 8 | c >> s << s | 1 << (s-1);
  getbits(-1);
  for (i=0; i < sizeof(buf)/sizeof(short); i++)
    buf[0][0][i] = 2048;
  for (row=0; row < height; row+=4) {
    FORC3 mul[c] = getbits(6);
    FORC3 {
      val = ((0x1000000/last[c] + 0x7ff) >> 12) * mul[c];
      s = val > 65564 ? 10:12;
      x = ~(-1 << (s-1));
      val <<= 12-s;
      for (i=0; i < sizeof(buf[0])/sizeof(short); i++)
	buf[c][0][i] = (buf[c][0][i] * val + x) >> s;
      last[c] = mul[c];
      for (r=0; r <= !c; r++) {
	buf[c][1][width/2] = buf[c][2][width/2] = mul[c] << 7;
	for (tree=1, col=width/2; col > 0; ) {
	  if ((tree = radc_token(tree))) {
	    col -= 2;
	    if (tree == 8)
	      FORYX buf[c][y][x] = (uchar) radc_token(18) * mul[c];
	    else
	      FORYX buf[c][y][x] = radc_token(tree+10) * 16 + PREDICTOR;
	  } else
	    do {
	      nreps = (col > 2) ? radc_token(9) + 1 : 1;
	      for (rep=0; rep < 8 && rep < nreps && col > 0; rep++) {
		col -= 2;
		FORYX buf[c][y][x] = PREDICTOR;
		if (rep & 1) {
		  step = radc_token(10) << 4;
		  FORYX buf[c][y][x] += step;
		}
	      }
	    } while (nreps == 9);
	}
	for (y=0; y < 2; y++)
	  for (x=0; x < width/2; x++) {
	    val = (buf[c][y+1][x] << 4) / mul[c];
	    if (val < 0) val = 0;
	    if (c) RAW(row+y*2+c-1,x*2+2-c) = val;
	    else   RAW(row+r*2+y,x*2+y) = val;
	  }
	memcpy (buf[c][0]+!c, buf[c][2], sizeof buf[c][0]-2*!c);
      }
    }
    for (y=row; y < row+4; y++)
      for (x=0; x < width; x++)
	if ((x+y) & 1) {
	  r = x ? x-1 : x+1;
	  s = x+1 < width ? x+1 : x-1;
	  val = (RAW(y,x)-2048)*2 + (RAW(y,r)+RAW(y,s))/2;
	  if (val < 0) val = 0;
	  RAW(y,x) = val;
	}
  }
  for (i=0; i < height*width; i++)
    raw_image[i] = curve[raw_image[i]];
  maximum = 0x3fff;
}

#undef FORYX
#undef PREDICTOR

#ifdef NO_JPEG
void CLASS kodak_jpeg_load_raw() {}
void CLASS lossy_dng_load_raw() {}
#else

METHODDEF(boolean)
fill_input_buffer (j_decompress_ptr cinfo)
{
#ifndef LIBRAW_NOTHREADS
#define jpeg_buffer tls->jpeg_buffer
#else
  static uchar jpeg_buffer[4096];
#endif
  size_t nbytes;

  nbytes = fread (jpeg_buffer, 1, 4096, ifp);
  swab (jpeg_buffer, jpeg_buffer, nbytes);
  cinfo->src->next_input_byte = jpeg_buffer;
  cinfo->src->bytes_in_buffer = nbytes;
  return TRUE;
#ifndef LIBRAW_NOTHREADS
#undef jpeg_buffer
#endif
}

void CLASS kodak_jpeg_load_raw()
{
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  JSAMPARRAY buf;
  JSAMPLE (*pixel)[3];
  int row, col;

  cinfo.err = jpeg_std_error (&jerr);
  jpeg_create_decompress (&cinfo);
  jpeg_stdio_src (&cinfo, ifp);
  cinfo.src->fill_input_buffer = fill_input_buffer;
  jpeg_read_header (&cinfo, TRUE);
  jpeg_start_decompress (&cinfo);
  if ((cinfo.output_width      != width  ) ||
      (cinfo.output_height*2   != height ) ||
      (cinfo.output_components != 3      )) {
    fprintf (stderr,_("%s: incorrect JPEG dimensions\n"), ifname);
    jpeg_destroy_decompress (&cinfo);
    longjmp (failure, 3);
  }
  buf = (*cinfo.mem->alloc_sarray)
		((j_common_ptr) &cinfo, JPOOL_IMAGE, width*3, 1);

  while (cinfo.output_scanline < cinfo.output_height) {
    row = cinfo.output_scanline * 2;
    jpeg_read_scanlines (&cinfo, buf, 1);
    pixel = (JSAMPLE (*)[3]) buf[0];
    for (col=0; col < width; col+=2) {
      RAW(row+0,col+0) = pixel[col+0][1] << 1;
      RAW(row+1,col+1) = pixel[col+1][1] << 1;
      RAW(row+0,col+1) = pixel[col][0] + pixel[col+1][0];
      RAW(row+1,col+0) = pixel[col][2] + pixel[col+1][2];
    }
  }
  jpeg_finish_decompress (&cinfo);
  jpeg_destroy_decompress (&cinfo);
  maximum = 0xff << 1;
}

void CLASS lossy_dng_load_raw()
{
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  JSAMPARRAY buf;
  JSAMPLE (*pixel)[3];
  unsigned sorder=order, ntags, opcode, deg, i, j, c;
  unsigned save=data_offset-4, trow=0, tcol=0, row, col;
  ushort curve[3][256];
  double coeff[9], tot;

  fseek (ifp, meta_offset, SEEK_SET);
  order = 0x4d4d;
  ntags = get4();
  while (ntags--) {
    opcode = get4(); get4(); get4();
    if (opcode != 8)
    { fseek (ifp, get4(), SEEK_CUR); continue; }
    fseek (ifp, 20, SEEK_CUR);
    if ((c = get4()) > 2) break;
    fseek (ifp, 12, SEEK_CUR);
    if ((deg = get4()) > 8) break;
    for (i=0; i <= deg && i < 9; i++)
      coeff[i] = getreal(12);
    for (i=0; i < 256; i++) {
      for (tot=j=0; j <= deg; j++)
	tot += coeff[j] * pow(i/255.0, j);
      curve[c][i] = tot*0xffff;
    }
  }
  order = sorder;
  cinfo.err = jpeg_std_error (&jerr);
  jpeg_create_decompress (&cinfo);
  while (trow < raw_height) {
    fseek (ifp, save+=4, SEEK_SET);
    if (tile_length < INT_MAX)
      fseek (ifp, get4(), SEEK_SET);
    jpeg_stdio_src (&cinfo, ifp);
    jpeg_read_header (&cinfo, TRUE);
    jpeg_start_decompress (&cinfo);
    buf = (*cinfo.mem->alloc_sarray)
	((j_common_ptr) &cinfo, JPOOL_IMAGE, cinfo.output_width*3, 1);
    while (cinfo.output_scanline < cinfo.output_height &&
	(row = trow + cinfo.output_scanline) < height) {
      jpeg_read_scanlines (&cinfo, buf, 1);
      pixel = (JSAMPLE (*)[3]) buf[0];
      for (col=0; col < cinfo.output_width && tcol+col < width; col++) {
	FORC3 image[row*width+tcol+col][c] = curve[c][pixel[col][c]];
      }
    }
    jpeg_abort_decompress (&cinfo);
    if ((tcol += tile_width) >= raw_width)
      trow += tile_length + (tcol = 0);
  }
  jpeg_destroy_decompress (&cinfo);
  maximum = 0xffff;
}
#endif

void CLASS kodak_dc120_load_raw()
{
  static const int mul[4] = { 162, 192, 187,  92 };
  static const int add[4] = {   0, 636, 424, 212 };
  uchar pixel[848];
  int row, shift, col;

  for (row=0; row < height; row++) {
    if (fread (pixel, 1, 848, ifp) < 848) derror();
    shift = row * mul[row & 3] + add[row & 3];
    for (col=0; col < width; col++)
      RAW(row,col) = (ushort) pixel[(col + shift) % 848];
  }
  maximum = 0xff;
}

void CLASS eight_bit_load_raw()
{
  uchar *pixel;
  unsigned row, col;

  pixel = (uchar *) calloc (raw_width, sizeof *pixel);
  merror (pixel, "eight_bit_load_raw()");
  for (row=0; row < raw_height; row++) {
    if (fread (pixel, 1, raw_width, ifp) < raw_width) derror();
    for (col=0; col < raw_width; col++)
      RAW(row,col) = curve[pixel[col]];
  }
  free (pixel);
  maximum = curve[0xff];
}

void CLASS kodak_yrgb_load_raw()
{
  uchar *pixel;
  int row, col, y, cb, cr, rgb[3], c;

  pixel = (uchar *) calloc (raw_width, 3*sizeof *pixel);
  merror (pixel, "kodak_yrgb_load_raw()");
  for (row=0; row < height; row++) {
    if (~row & 1)
      if (fread (pixel, raw_width, 3, ifp) < 3) derror();
    for (col=0; col < raw_width; col++) {
      y  = pixel[width*2*(row & 1) + col];
      cb = pixel[width + (col & -2)]   - 128;
      cr = pixel[width + (col & -2)+1] - 128;
      rgb[1] = y-((cb + cr + 2) >> 2);
      rgb[2] = rgb[1] + cb;
      rgb[0] = rgb[1] + cr;
#ifndef LIBRAW_LIBRARY_BUILD
      FORC3 image[row*width+col][c] = curve[LIM(rgb[c],0,255)];
#else
      FORC3 color_image[(row+top_margin)*raw_width+col+left_margin][c] = curve[LIM(rgb[c],0,255)];
#endif
    }
  }
  free (pixel);
  maximum = curve[0xff];
}

void CLASS kodak_262_load_raw()
{
  static const uchar kodak_tree[2][26] =
  { { 0,1,5,1,1,2,0,0,0,0,0,0,0,0,0,0, 0,1,2,3,4,5,6,7,8,9 },
    { 0,3,1,1,1,1,1,2,0,0,0,0,0,0,0,0, 0,1,2,3,4,5,6,7,8,9 } };
  ushort *huff[2];
  uchar *pixel;
  int *strip, ns, c, row, col, chess, pi=0, pi1, pi2, pred, val;

  FORC(2) huff[c] = make_decoder (kodak_tree[c]);
  ns = (raw_height+63) >> 5;
  pixel = (uchar *) malloc (raw_width*32 + ns*4);
  merror (pixel, "kodak_262_load_raw()");
  strip = (int *) (pixel + raw_width*32);
  order = 0x4d4d;
  FORC(ns) strip[c] = get4();
  for (row=0; row < raw_height; row++) {
    if ((row & 31) == 0) {
      fseek (ifp, strip[row >> 5], SEEK_SET);
      getbits(-1);
      pi = 0;
    }
    for (col=0; col < raw_width; col++) {
      chess = (row + col) & 1;
      pi1 = chess ? pi-2           : pi-raw_width-1;
      pi2 = chess ? pi-2*raw_width : pi-raw_width+1;
      if (col <= chess) pi1 = -1;
      if (pi1 < 0) pi1 = pi2;
      if (pi2 < 0) pi2 = pi1;
      if (pi1 < 0 && col > 1) pi1 = pi2 = pi-2;
      pred = (pi1 < 0) ? 0 : (pixel[pi1] + pixel[pi2]) >> 1;
      pixel[pi] = val = pred + ljpeg_diff (huff[chess]);
      if (val >> 8) derror();
      val = curve[pixel[pi++]];
      RAW(row,col) = val;
    }
  }
  free (pixel);
  FORC(2) free (huff[c]);
}

int CLASS kodak_65000_decode (short *out, int bsize)
{
  uchar c, blen[768];
  ushort raw[6];
  INT64 bitbuf=0;
  int save, bits=0, i, j, len, diff;

  save = ftell(ifp);
  bsize = (bsize + 3) & -4;
  for (i=0; i < bsize; i+=2) {
    c = fgetc(ifp);
    if ((blen[i  ] = c & 15) > 12 ||
	(blen[i+1] = c >> 4) > 12 ) {
      fseek (ifp, save, SEEK_SET);
      for (i=0; i < bsize; i+=8) {
	read_shorts (raw, 6);
	out[i  ] = raw[0] >> 12 << 8 | raw[2] >> 12 << 4 | raw[4] >> 12;
	out[i+1] = raw[1] >> 12 << 8 | raw[3] >> 12 << 4 | raw[5] >> 12;
	for (j=0; j < 6; j++)
	  out[i+2+j] = raw[j] & 0xfff;
      }
      return 1;
    }
  }
  if ((bsize & 7) == 4) {
    bitbuf  = fgetc(ifp) << 8;
    bitbuf += fgetc(ifp);
    bits = 16;
  }
  for (i=0; i < bsize; i++) {
    len = blen[i];
    if (bits < len) {
      for (j=0; j < 32; j+=8)
	bitbuf += (INT64) fgetc(ifp) << (bits+(j^8));
      bits += 32;
    }
    diff = bitbuf & (0xffff >> (16-len));
    bitbuf >>= len;
    bits -= len;
    if ((diff & (1 << (len-1))) == 0)
      diff -= (1 << len) - 1;
    out[i] = diff;
  }
  return 0;
}

void CLASS kodak_65000_load_raw()
{
  short buf[256];
  int row, col, len, pred[2], ret, i;

  for (row=0; row < height; row++)
    for (col=0; col < width; col+=256) {
      pred[0] = pred[1] = 0;
      len = MIN (256, width-col);
      ret = kodak_65000_decode (buf, len);
      for (i=0; i < len; i++)
	if ((RAW(row,col+i) =	curve[ret ? buf[i] :
		(pred[i & 1] += buf[i])]) >> 12) derror();
    }
}

void CLASS kodak_ycbcr_load_raw()
{
  short buf[384], *bp;
  int row, col, len, c, i, j, k, y[2][2], cb, cr, rgb[3];
  ushort *ip;

  for (row=0; row < height; row+=2)
    for (col=0; col < width; col+=128) {
      len = MIN (128, width-col);
      kodak_65000_decode (buf, len*3);
      y[0][1] = y[1][1] = cb = cr = 0;
      for (bp=buf, i=0; i < len; i+=2, bp+=2) {
	cb += bp[4];
	cr += bp[5];
	rgb[1] = -((cb + cr + 2) >> 2);
	rgb[2] = rgb[1] + cb;
	rgb[0] = rgb[1] + cr;
	for (j=0; j < 2; j++)
	  for (k=0; k < 2; k++) {
	    if ((y[j][k] = y[j][k^1] + *bp++) >> 10) derror();
#ifndef LIBRAW_LIBRARY_BUILD
	    ip = image[(row+j)*width + col+i+k];
	    FORC3 ip[c] = curve[LIM(y[j][k]+rgb[c], 0, 0xfff)];
#else
	    ip = color_image[(row+top_margin+j)*raw_width + col+i+k+left_margin];
            FORC3 ip[c] = curve[LIM(y[j][k]+rgb[c], 0, 0xfff)];
#endif
	  }
      }
    }
}

void CLASS kodak_rgb_load_raw()
{
  short buf[768], *bp;
  int row, col, len, c, i, rgb[3];
  ushort *ip=image[0];

#ifndef LIBRAW_LIBRARY_BUILD
  if (raw_image) free (raw_image);
  raw_image = 0;
#endif
  for (row=0; row < height; row++)
    for (col=0; col < width; col+=256) {
      len = MIN (256, width-col);
      kodak_65000_decode (buf, len*3);
      memset (rgb, 0, sizeof rgb);
#ifdef LIBRAW_LIBRARY_BUILD
      ip = &color_image[(row+top_margin)*raw_width+left_margin][0];
#endif
      for (bp=buf, i=0; i < len; i++, ip+=4)
	FORC3 if ((ip[c] = rgb[c] += *bp++) >> 12) derror();
    }
}

void CLASS kodak_thumb_load_raw()
{
  int row, col;
  colors = thumb_misc >> 5;
  for (row=0; row < height; row++)
    for (col=0; col < width; col++)
      read_shorts (image[row*width+col], colors);
  maximum = (1 << (thumb_misc & 31)) - 1;
}

void CLASS sony_decrypt (unsigned *data, int len, int start, int key)
{
#ifndef LIBRAW_NOTHREADS
#define pad tls->sony_decrypt.pad
#define p   tls->sony_decrypt.p
#else
  static unsigned pad[128], p;
#endif
  if (start) {
    for (p=0; p < 4; p++)
      pad[p] = key = key * 48828125 + 1;
    pad[3] = pad[3] << 1 | (pad[0]^pad[2]) >> 31;
    for (p=4; p < 127; p++)
      pad[p] = (pad[p-4]^pad[p-2]) << 1 | (pad[p-3]^pad[p-1]) >> 31;
    for (p=0; p < 127; p++)
      pad[p] = htonl(pad[p]);
  }
  while (len--)
    *data++ ^= pad[p++ & 127] = pad[(p+1) & 127] ^ pad[(p+65) & 127];
#ifndef LIBRAW_NOTHREADS
#undef pad
#undef p
#endif
}

void CLASS sony_load_raw()
{
  uchar head[40];
  ushort *pixel;
  unsigned i, key, row, col;

  fseek (ifp, 200896, SEEK_SET);
  fseek (ifp, (unsigned) fgetc(ifp)*4 - 1, SEEK_CUR);
  order = 0x4d4d;
  key = get4();
  fseek (ifp, 164600, SEEK_SET);
  fread (head, 1, 40, ifp);
  sony_decrypt ((unsigned int *) head, 10, 1, key);
  for (i=26; i-- > 22; )
    key = key << 8 | head[i];
  fseek (ifp, data_offset, SEEK_SET);
  for (row=0; row < raw_height; row++) {
    pixel = raw_image + row*raw_width;
    if (fread (pixel, 2, raw_width, ifp) < raw_width) derror();
    sony_decrypt ((unsigned int *) pixel, raw_width/2, !row, key);
    for (col=0; col < raw_width; col++)
      if ((pixel[col] = ntohs(pixel[col])) >> 14) derror();
  }
  maximum = 0x3ff0;
}

void CLASS sony_arw_load_raw()
{
  ushort huff[32768];
  static const ushort tab[18] =
  { 0xf11,0xf10,0xe0f,0xd0e,0xc0d,0xb0c,0xa0b,0x90a,0x809,
    0x708,0x607,0x506,0x405,0x304,0x303,0x300,0x202,0x201 };
  int i, c, n, col, row, len, diff, sum=0;

  for (n=i=0; i < 18; i++)
    FORC(32768 >> (tab[i] >> 8)) huff[n++] = tab[i];
  getbits(-1);
  for (col = raw_width; col--; )
    for (row=0; row < raw_height+1; row+=2) {
      if (row == raw_height) row = 1;
      len = getbithuff(15,huff);
      diff = getbits(len);
      if ((diff & (1 << (len-1))) == 0)
	diff -= (1 << len) - 1;
      if ((sum += diff) >> 12) derror();
      if (row < height) RAW(row,col) = sum;
    }
}

void CLASS sony_arw2_load_raw()
{
  uchar *data, *dp;
  ushort pix[16];
  int row, col, val, max, min, imax, imin, sh, bit, i;

  data = (uchar *) malloc (raw_width);
  merror (data, "sony_arw2_load_raw()");
  for (row=0; row < height; row++) {
    fread (data, 1, raw_width, ifp);
    for (dp=data, col=0; col < raw_width-30; dp+=16) {
      max = 0x7ff & (val = sget4(dp));
      min = 0x7ff & val >> 11;
      imax = 0x0f & val >> 22;
      imin = 0x0f & val >> 26;
      for (sh=0; sh < 4 && 0x80 << sh <= max-min; sh++);
      for (bit=30, i=0; i < 16; i++)
	if      (i == imax) pix[i] = max;
	else if (i == imin) pix[i] = min;
	else {
	  pix[i] = ((sget2(dp+(bit >> 3)) >> (bit & 7) & 0x7f) << sh) + min;
	  if (pix[i] > 0x7ff) pix[i] = 0x7ff;
	  bit += 7;
	}
#ifdef LIBRAW_LIBRARY_BUILD
      if(imgdata.params.sony_arw2_hack)
          {
              for (i=0; i < 16; i++, col+=2)
                  RAW(row,col) = curve[pix[i] << 1];
          }
      else
          {
              for (i=0; i < 16; i++, col+=2)
                  RAW(row,col) = curve[pix[i] << 1] >> 2;
          }
#else
      for (i=0; i < 16; i++, col+=2)
	if (col < width) RAW(row,col) = curve[pix[i] << 1] >> 2;
#endif
      col -= col & 1 ? 1:31;
    }
  }
  free (data);
#ifdef LIBRAW_LIBRARY_BUILD
  if(imgdata.params.sony_arw2_hack)
  {
	black <<= 2;
	maximum <<=2;
  }
#endif
}

#define HOLE(row) ((holes >> (((row) - raw_height) & 7)) & 1)

/* Kudos to Rich Taylor for figuring out SMaL's compression algorithm. */
void CLASS smal_decode_segment (unsigned seg[2][2], int holes)
{
  uchar hist[3][13] = {
    { 7, 7, 0, 0, 63, 55, 47, 39, 31, 23, 15, 7, 0 },
    { 7, 7, 0, 0, 63, 55, 47, 39, 31, 23, 15, 7, 0 },
    { 3, 3, 0, 0, 63,     47,     31,     15,    0 } };
  int low, high=0xff, carry=0, nbits=8;
  int pix, s, count, bin, next, i, sym[3];
  uchar diff, pred[]={0,0};
  ushort data=0, range=0;

  fseek (ifp, seg[0][1]+1, SEEK_SET);
  getbits(-1);
  for (pix=seg[0][0]; pix < seg[1][0]; pix++) {
    for (s=0; s < 3; s++) {
      data = data << nbits | getbits(nbits);
      if (carry < 0)
	carry = (nbits += carry+1) < 1 ? nbits-1 : 0;
      while (--nbits >= 0)
	if ((data >> nbits & 0xff) == 0xff) break;
      if (nbits > 0)
	  data = ((data & ((1 << (nbits-1)) - 1)) << 1) |
	((data + (((data & (1 << (nbits-1)))) << 1)) & (-1 << nbits));
      if (nbits >= 0) {
	data += getbits(1);
	carry = nbits - 8;
      }
      count = ((((data-range+1) & 0xffff) << 2) - 1) / (high >> 4);
      for (bin=0; hist[s][bin+5] > count; bin++);
		low = hist[s][bin+5] * (high >> 4) >> 2;
      if (bin) high = hist[s][bin+4] * (high >> 4) >> 2;
      high -= low;
      for (nbits=0; high << nbits < 128; nbits++);
      range = (range+low) << nbits;
      high <<= nbits;
      next = hist[s][1];
      if (++hist[s][2] > hist[s][3]) {
	next = (next+1) & hist[s][0];
	hist[s][3] = (hist[s][next+4] - hist[s][next+5]) >> 2;
	hist[s][2] = 1;
      }
      if (hist[s][hist[s][1]+4] - hist[s][hist[s][1]+5] > 1) {
	if (bin < hist[s][1])
	  for (i=bin; i < hist[s][1]; i++) hist[s][i+5]--;
	else if (next <= bin)
	  for (i=hist[s][1]; i < bin; i++) hist[s][i+5]++;
      }
      hist[s][1] = next;
      sym[s] = bin;
    }
    diff = sym[2] << 5 | sym[1] << 2 | (sym[0] & 3);
    if (sym[0] & 4)
      diff = diff ? -diff : 0x80;
    if (ftell(ifp) + 12 >= seg[1][1])
      diff = 0;
    raw_image[pix] = pred[pix & 1] += diff;
    if (!(pix & 1) && HOLE(pix / raw_width)) pix += 2;
  }
  maximum = 0xff;
}

void CLASS smal_v6_load_raw()
{
  unsigned seg[2][2];

  fseek (ifp, 16, SEEK_SET);
  seg[0][0] = 0;
  seg[0][1] = get2();
  seg[1][0] = raw_width * raw_height;
  seg[1][1] = INT_MAX;
  smal_decode_segment (seg, 0);
}

int CLASS median4 (int *p)
{
  int min, max, sum, i;

  min = max = sum = p[0];
  for (i=1; i < 4; i++) {
    sum += p[i];
    if (min > p[i]) min = p[i];
    if (max < p[i]) max = p[i];
  }
  return (sum - min - max) >> 1;
}

void CLASS fill_holes (int holes)
{
  int row, col, val[4];

  for (row=2; row < height-2; row++) {
    if (!HOLE(row)) continue;
    for (col=1; col < width-1; col+=4) {
      val[0] = RAW(row-1,col-1);
      val[1] = RAW(row-1,col+1);
      val[2] = RAW(row+1,col-1);
      val[3] = RAW(row+1,col+1);
      RAW(row,col) = median4(val);
    }
    for (col=2; col < width-2; col+=4)
      if (HOLE(row-2) || HOLE(row+2))
	RAW(row,col) = (RAW(row,col-2) + RAW(row,col+2)) >> 1;
      else {
	val[0] = RAW(row,col-2);
	val[1] = RAW(row,col+2);
	val[2] = RAW(row-2,col);
	val[3] = RAW(row+2,col);
	RAW(row,col) = median4(val);
      }
  }
}

void CLASS smal_v9_load_raw()
{
  unsigned seg[256][2], offset, nseg, holes, i;

  fseek (ifp, 67, SEEK_SET);
  offset = get4();
  nseg = fgetc(ifp);
  fseek (ifp, offset, SEEK_SET);
  for (i=0; i < nseg*2; i++)
    seg[0][i] = get4() + data_offset*(i & 1);
  fseek (ifp, 78, SEEK_SET);
  holes = fgetc(ifp);
  fseek (ifp, 88, SEEK_SET);
  seg[nseg][0] = raw_height * raw_width;
  seg[nseg][1] = get4() + data_offset;
  for (i=0; i < nseg; i++)
    smal_decode_segment (seg+i, holes);
  if (holes) fill_holes (holes);
}

void CLASS redcine_load_raw()
{
#ifndef NO_JASPER
  int c, row, col;
  jas_stream_t *in;
  jas_image_t *jimg;
  jas_matrix_t *jmat;
  jas_seqent_t *data;
  ushort *img, *pix;

  jas_init();
#ifndef LIBRAW_LIBRARY_BUILD
  in = jas_stream_fopen (ifname, "rb");
#else
  in = (jas_stream_t*)ifp->make_jas_stream();
  if(!in)
          throw LIBRAW_EXCEPTION_DECODE_JPEG2000;
#endif
  jas_stream_seek (in, data_offset+20, SEEK_SET);
  jimg = jas_image_decode (in, -1, 0);
#ifndef LIBRAW_LIBRARY_BUILD
  if (!jimg) longjmp (failure, 3);
#else
  if(!jimg)
      {
          jas_stream_close (in);
          throw LIBRAW_EXCEPTION_DECODE_JPEG2000;
      }
#endif
  jmat = jas_matrix_create (height/2, width/2);
  merror (jmat, "redcine_load_raw()");
  img = (ushort *) calloc ((height+2)*(width+2), 2);
  merror (img, "redcine_load_raw()");
  FORC4 {
    jas_image_readcmpt (jimg, c, 0, 0, width/2, height/2, jmat);
    data = jas_matrix_getref (jmat, 0, 0);
    for (row = c >> 1; row < height; row+=2)
      for (col = c & 1; col < width; col+=2)
	img[(row+1)*(width+2)+col+1] = data[(row/2)*(width/2)+col/2];
  }
  for (col=1; col <= width; col++) {
    img[col] = img[2*(width+2)+col];
    img[(height+1)*(width+2)+col] = img[(height-1)*(width+2)+col];
  }
  for (row=0; row < height+2; row++) {
    img[row*(width+2)] = img[row*(width+2)+2];
    img[(row+1)*(width+2)-1] = img[(row+1)*(width+2)-3];
  }
  for (row=1; row <= height; row++) {
    pix = img + row*(width+2) + (col = 1 + (FC(row,1) & 1));
    for (   ; col <= width; col+=2, pix+=2) {
      c = (((pix[0] - 0x800) << 3) +
	pix[-(width+2)] + pix[width+2] + pix[-1] + pix[1]) >> 2;
      pix[0] = LIM(c,0,4095);
    }
  }
  for (row=0; row < height; row++)
    for (col=0; col < width; col++)
      RAW(row,col) = curve[img[(row+1)*(width+2)+col+1]];
  free (img);
  jas_matrix_destroy (jmat);
  jas_image_destroy (jimg);
  jas_stream_close (in);
#endif
}
#line 2937 "dcraw/dcraw.c"
void CLASS remove_zeroes()
{
  unsigned row, col, tot, n, r, c;

  for (row=0; row < height; row++)
    for (col=0; col < width; col++)
      if (BAYER(row,col) == 0) {
	tot = n = 0;
	for (r = row-2; r <= row+2; r++)
	  for (c = col-2; c <= col+2; c++)
	    if (r < height && c < width &&
		FC(r,c) == FC(row,col) && BAYER(r,c))
	      tot += (n++,BAYER(r,c));
	if (n) BAYER(row,col) = tot/n;
      }
}
#line 3117 "dcraw/dcraw.c"
void CLASS gamma_curve (double pwr, double ts, int mode, int imax)
{
  int i;
  double g[6], bnd[2]={0,0}, r;

  g[0] = pwr;
  g[1] = ts;
  g[2] = g[3] = g[4] = 0;
  bnd[g[1] >= 1] = 1;
  if (g[1] && (g[1]-1)*(g[0]-1) <= 0) {
    for (i=0; i < 48; i++) {
      g[2] = (bnd[0] + bnd[1])/2;
      if (g[0]) bnd[(pow(g[2]/g[1],-g[0]) - 1)/g[0] - 1/g[2] > -1] = g[2];
      else	bnd[g[2]/exp(1-1/g[2]) < g[1]] = g[2];
    }
    g[3] = g[2] / g[1];
    if (g[0]) g[4] = g[2] * (1/g[0] - 1);
  }
  if (g[0]) g[5] = 1 / (g[1]*SQR(g[3])/2 - g[4]*(1 - g[3]) +
		(1 - pow(g[3],1+g[0]))*(1 + g[4])/(1 + g[0])) - 1;
  else      g[5] = 1 / (g[1]*SQR(g[3])/2 + 1
		- g[2] - g[3] -	g[2]*g[3]*(log(g[3]) - 1)) - 1;
  if (!mode--) {
    memcpy (gamm, g, sizeof gamm);
    return;
  }
  for (i=0; i < 0x10000; i++) {
    curve[i] = 0xffff;
    if ((r = (double) i / imax) < 1)
      curve[i] = 0x10000 * ( mode
	? (r < g[3] ? r*g[1] : (g[0] ? pow( r,g[0])*(1+g[4])-g[4]    : log(r)*g[2]+1))
	: (r < g[2] ? r/g[1] : (g[0] ? pow((r+g[4])/(1+g[4]),1/g[0]) : exp((r-1)/g[2]))));
  }
}

void CLASS pseudoinverse (double (*in)[3], double (*out)[3], int size)
{
  double work[3][6], num;
  int i, j, k;

  for (i=0; i < 3; i++) {
    for (j=0; j < 6; j++)
      work[i][j] = j == i+3;
    for (j=0; j < 3; j++)
      for (k=0; k < size; k++)
	work[i][j] += in[k][i] * in[k][j];
  }
  for (i=0; i < 3; i++) {
    num = work[i][i];
    for (j=0; j < 6; j++)
      work[i][j] /= num;
    for (k=0; k < 3; k++) {
      if (k==i) continue;
      num = work[k][i];
      for (j=0; j < 6; j++)
	work[k][j] -= work[i][j] * num;
    }
  }
  for (i=0; i < size; i++)
    for (j=0; j < 3; j++)
      for (out[i][j]=k=0; k < 3; k++)
	out[i][j] += work[j][k+3] * in[i][k];
}

void CLASS cam_xyz_coeff (double cam_xyz[4][3])
{
  double cam_rgb[4][3], inverse[4][3], num;
  int i, j, k;

  for (i=0; i < colors; i++)		/* Multiply out XYZ colorspace */
    for (j=0; j < 3; j++)
      for (cam_rgb[i][j] = k=0; k < 3; k++)
	cam_rgb[i][j] += cam_xyz[i][k] * xyz_rgb[k][j];

  for (i=0; i < colors; i++) {		/* Normalize cam_rgb so that */
    for (num=j=0; j < 3; j++)		/* cam_rgb * (1,1,1) is (1,1,1,1) */
      num += cam_rgb[i][j];
    for (j=0; j < 3; j++)
      cam_rgb[i][j] /= num;
    pre_mul[i] = 1 / num;
  }
  pseudoinverse (cam_rgb, inverse, colors);
  for (raw_color = i=0; i < 3; i++)
    for (j=0; j < colors; j++)
      rgb_cam[i][j] = inverse[j][i];
}

#ifdef COLORCHECK
void CLASS colorcheck()
{
#define NSQ 24
// Coordinates of the GretagMacbeth ColorChecker squares
// width, height, 1st_column, 1st_row
  int cut[NSQ][4];			// you must set these
// ColorChecker Chart under 6500-kelvin illumination
  static const double gmb_xyY[NSQ][3] = {
    { 0.400, 0.350, 10.1 },		// Dark Skin
    { 0.377, 0.345, 35.8 },		// Light Skin
    { 0.247, 0.251, 19.3 },		// Blue Sky
    { 0.337, 0.422, 13.3 },		// Foliage
    { 0.265, 0.240, 24.3 },		// Blue Flower
    { 0.261, 0.343, 43.1 },		// Bluish Green
    { 0.506, 0.407, 30.1 },		// Orange
    { 0.211, 0.175, 12.0 },		// Purplish Blue
    { 0.453, 0.306, 19.8 },		// Moderate Red
    { 0.285, 0.202, 6.6 },		// Purple
    { 0.380, 0.489, 44.3 },		// Yellow Green
    { 0.473, 0.438, 43.1 },		// Orange Yellow
    { 0.187, 0.129, 6.1 },		// Blue
    { 0.305, 0.478, 23.4 },		// Green
    { 0.539, 0.313, 12.0 },		// Red
    { 0.448, 0.470, 59.1 },		// Yellow
    { 0.364, 0.233, 19.8 },		// Magenta
    { 0.196, 0.252, 19.8 },		// Cyan
    { 0.310, 0.316, 90.0 },		// White
    { 0.310, 0.316, 59.1 },		// Neutral 8
    { 0.310, 0.316, 36.2 },		// Neutral 6.5
    { 0.310, 0.316, 19.8 },		// Neutral 5
    { 0.310, 0.316, 9.0 },		// Neutral 3.5
    { 0.310, 0.316, 3.1 } };		// Black
  double gmb_cam[NSQ][4], gmb_xyz[NSQ][3];
  double inverse[NSQ][3], cam_xyz[4][3], num;
  int c, i, j, k, sq, row, col, count[4];

  memset (gmb_cam, 0, sizeof gmb_cam);
  for (sq=0; sq < NSQ; sq++) {
    FORCC count[c] = 0;
    for   (row=cut[sq][3]; row < cut[sq][3]+cut[sq][1]; row++)
      for (col=cut[sq][2]; col < cut[sq][2]+cut[sq][0]; col++) {
	c = FC(row,col);
	if (c >= colors) c -= 2;
	gmb_cam[sq][c] += BAYER(row,col);
	count[c]++;
      }
    FORCC gmb_cam[sq][c] = gmb_cam[sq][c]/count[c] - black;
    gmb_xyz[sq][0] = gmb_xyY[sq][2] * gmb_xyY[sq][0] / gmb_xyY[sq][1];
    gmb_xyz[sq][1] = gmb_xyY[sq][2];
    gmb_xyz[sq][2] = gmb_xyY[sq][2] *
		(1 - gmb_xyY[sq][0] - gmb_xyY[sq][1]) / gmb_xyY[sq][1];
  }
  pseudoinverse (gmb_xyz, inverse, NSQ);
  for (i=0; i < colors; i++)
    for (j=0; j < 3; j++)
      for (cam_xyz[i][j] = k=0; k < NSQ; k++)
	cam_xyz[i][j] += gmb_cam[k][i] * inverse[k][j];
  cam_xyz_coeff (cam_xyz);
  if (verbose) {
    printf ("    { \"%s %s\", %d,\n\t{", make, model, black);
    num = 10000 / (cam_xyz[1][0] + cam_xyz[1][1] + cam_xyz[1][2]);
    FORCC for (j=0; j < 3; j++)
      printf ("%c%d", (c | j) ? ',':' ', (int) (cam_xyz[c][j] * num + 0.5));
    puts (" } },");
  }
#undef NSQ
}
#endif

void CLASS hat_transform (float *temp, float *base, int st, int size, int sc)
{
  int i;
  for (i=0; i < sc; i++)
    temp[i] = 2*base[st*i] + base[st*(sc-i)] + base[st*(i+sc)];
  for (; i+sc < size; i++)
    temp[i] = 2*base[st*i] + base[st*(i-sc)] + base[st*(i+sc)];
  for (; i < size; i++)
    temp[i] = 2*base[st*i] + base[st*(i-sc)] + base[st*(2*size-2-(i+sc))];
}

void CLASS wavelet_denoise()
{
  float *fimg=0, *temp, thold, mul[2], avg, diff;
  int scale=1, size, lev, hpass, lpass, row, col, nc, c, i, wlast, blk[2];
  ushort *window[4];
  static const float noise[] =
  { 0.8002,0.2735,0.1202,0.0585,0.0291,0.0152,0.0080,0.0044 };

#ifdef DCRAW_VERBOSE
  if (verbose) fprintf (stderr,_("Wavelet denoising...\n"));
#endif

  while (maximum << scale < 0x10000) scale++;
  maximum <<= --scale;
  black <<= scale;
  FORC4 cblack[c] <<= scale;
  if ((size = iheight*iwidth) < 0x15550000)
    fimg = (float *) malloc ((size*3 + iheight + iwidth) * sizeof *fimg);
  merror (fimg, "wavelet_denoise()");
  temp = fimg + size*3;
  if ((nc = colors) == 3 && filters) nc++;
  FORC(nc) {			/* denoise R,G1,B,G3 individually */
    for (i=0; i < size; i++)
      fimg[i] = 256 * sqrt(image[i][c] << scale);
    for (hpass=lev=0; lev < 5; lev++) {
      lpass = size*((lev & 1)+1);
      for (row=0; row < iheight; row++) {
	hat_transform (temp, fimg+hpass+row*iwidth, 1, iwidth, 1 << lev);
	for (col=0; col < iwidth; col++)
	  fimg[lpass + row*iwidth + col] = temp[col] * 0.25;
      }
      for (col=0; col < iwidth; col++) {
	hat_transform (temp, fimg+lpass+col, iwidth, iheight, 1 << lev);
	for (row=0; row < iheight; row++)
	  fimg[lpass + row*iwidth + col] = temp[row] * 0.25;
      }
      thold = threshold * noise[lev];
      for (i=0; i < size; i++) {
	fimg[hpass+i] -= fimg[lpass+i];
	if	(fimg[hpass+i] < -thold) fimg[hpass+i] += thold;
	else if (fimg[hpass+i] >  thold) fimg[hpass+i] -= thold;
	else	 fimg[hpass+i] = 0;
	if (hpass) fimg[i] += fimg[hpass+i];
      }
      hpass = lpass;
    }
    for (i=0; i < size; i++)
      image[i][c] = CLIP(SQR(fimg[i]+fimg[lpass+i])/0x10000);
  }
  if (filters && colors == 3) {  /* pull G1 and G3 closer together */
    for (row=0; row < 2; row++) {
      mul[row] = 0.125 * pre_mul[FC(row+1,0) | 1] / pre_mul[FC(row,0) | 1];
      blk[row] = cblack[FC(row,0) | 1];
    }
    for (i=0; i < 4; i++)
      window[i] = (ushort *) fimg + width*i;
    for (wlast=-1, row=1; row < height-1; row++) {
      while (wlast < row+1) {
	for (wlast++, i=0; i < 4; i++)
	  window[(i+3) & 3] = window[i];
	for (col = FC(wlast,1) & 1; col < width; col+=2)
	  window[2][col] = BAYER(wlast,col);
      }
      thold = threshold/512;
      for (col = (FC(row,0) & 1)+1; col < width-1; col+=2) {
	avg = ( window[0][col-1] + window[0][col+1] +
		window[2][col-1] + window[2][col+1] - blk[~row & 1]*4 )
	      * mul[row & 1] + (window[1][col] + blk[row & 1]) * 0.5;
	avg = avg < 0 ? 0 : sqrt(avg);
	diff = sqrt(BAYER(row,col)) - avg;
	if      (diff < -thold) diff += thold;
	else if (diff >  thold) diff -= thold;
	else diff = 0;
	BAYER(row,col) = CLIP(SQR(avg+diff) + 0.5);
      }
    }
  }
  free (fimg);
}

// green equilibration
void CLASS green_matching()
{
  int i,j;
  double m1,m2,c1,c2;
  int o1_1,o1_2,o1_3,o1_4;
  int o2_1,o2_2,o2_3,o2_4;
  ushort (*img)[4];
  const int margin = 3;
  int oj = 2, oi = 2;
  float f;
  const float thr = 0.01f;
  if(half_size || shrink) return;
  if(FC(oj, oi) != 3) oj++;
  if(FC(oj, oi) != 3) oi++;
  if(FC(oj, oi) != 3) oj--;

  img = (ushort (*)[4]) calloc (height*width, sizeof *image);
  merror (img, "green_matching()");
  memcpy(img,image,height*width*sizeof *image);

  for(j=oj;j<height-margin;j+=2)
    for(i=oi;i<width-margin;i+=2){
      o1_1=img[(j-1)*width+i-1][1];
      o1_2=img[(j-1)*width+i+1][1];
      o1_3=img[(j+1)*width+i-1][1];
      o1_4=img[(j+1)*width+i+1][1];
      o2_1=img[(j-2)*width+i][3];
      o2_2=img[(j+2)*width+i][3];
      o2_3=img[j*width+i-2][3];
      o2_4=img[j*width+i+2][3];

      m1=(o1_1+o1_2+o1_3+o1_4)/4.0;
      m2=(o2_1+o2_2+o2_3+o2_4)/4.0;

      c1=(abs(o1_1-o1_2)+abs(o1_1-o1_3)+abs(o1_1-o1_4)+abs(o1_2-o1_3)+abs(o1_3-o1_4)+abs(o1_2-o1_4))/6.0;
      c2=(abs(o2_1-o2_2)+abs(o2_1-o2_3)+abs(o2_1-o2_4)+abs(o2_2-o2_3)+abs(o2_3-o2_4)+abs(o2_2-o2_4))/6.0;
      if((img[j*width+i][3]<maximum*0.95)&&(c1<maximum*thr)&&(c2<maximum*thr))
      {
        f = image[j*width+i][3]*m1/m2;
        image[j*width+i][3]=f>0xffff?0xffff:f;
      }
    }
  free(img);
}

void CLASS scale_colors()
{
  unsigned bottom, right, size, row, col, ur, uc, i, x, y, c, sum[8];
  int val, dark, sat;
  double dsum[8], dmin, dmax;
  float scale_mul[4], fr, fc;
  ushort *img=0, *pix;

#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_SCALE_COLORS,0,2);
#endif

  if (user_mul[0])
    memcpy (pre_mul, user_mul, sizeof pre_mul);
  if (use_auto_wb || (use_camera_wb && cam_mul[0] == -1)) {
    memset (dsum, 0, sizeof dsum);
    bottom = MIN (greybox[1]+greybox[3], height);
    right  = MIN (greybox[0]+greybox[2], width);
    for (row=greybox[1]; row < bottom; row += 8)
      for (col=greybox[0]; col < right; col += 8) {
	memset (sum, 0, sizeof sum);
	for (y=row; y < row+8 && y < bottom; y++)
	  for (x=col; x < col+8 && x < right; x++)
	    FORC4 {
	      if (filters) {
		c = fcol(y,x);
		val = BAYER2(y,x);
	      } else
		val = image[y*width+x][c];
	      if (val > maximum-25) goto skip_block;
	      if ((val -= cblack[c]) < 0) val = 0;
	      sum[c] += val;
	      sum[c+4]++;
	      if (filters) break;
	    }
	FORC(8) dsum[c] += sum[c];
skip_block: ;
      }
    FORC4 if (dsum[c]) pre_mul[c] = dsum[c+4] / dsum[c];
  }
  if (use_camera_wb && cam_mul[0] != -1) {
    memset (sum, 0, sizeof sum);
    for (row=0; row < 8; row++)
      for (col=0; col < 8; col++) {
	c = FC(row,col);
	if ((val = white[row][col] - cblack[c]) > 0)
	  sum[c] += val;
	sum[c+4]++;
      }
    if (sum[0] && sum[1] && sum[2] && sum[3])
      FORC4 pre_mul[c] = (float) sum[c+4] / sum[c];
    else if (cam_mul[0] && cam_mul[2])
      memcpy (pre_mul, cam_mul, sizeof pre_mul);
    else
      {
#ifdef LIBRAW_LIBRARY_BUILD
            imgdata.process_warnings |= LIBRAW_WARN_BAD_CAMERA_WB;
#endif
#ifdef DCRAW_VERBOSE
            fprintf (stderr,_("%s: Cannot use camera white balance.\n"), ifname);
#endif
      }
  }
  if (pre_mul[3] == 0) pre_mul[3] = colors < 4 ? pre_mul[1] : 1;
  dark = black;
  sat = maximum;
  if (threshold) wavelet_denoise();
  maximum -= black;
  for (dmin=DBL_MAX, dmax=c=0; c < 4; c++) {
    if (dmin > pre_mul[c])
	dmin = pre_mul[c];
    if (dmax < pre_mul[c])
	dmax = pre_mul[c];
  }
  if (!highlight) dmax = dmin;
  FORC4 scale_mul[c] = (pre_mul[c] /= dmax) * 65535.0 / maximum;
  if (verbose) {
    fprintf (stderr,
      _("Scaling with darkness %d, saturation %d, and\nmultipliers"), dark, sat);
    FORC4 fprintf (stderr, " %f", pre_mul[c]);
    fputc ('\n', stderr);
  }
  size = iheight*iwidth;
#ifdef LIBRAW_LIBRARY_BUILD
  scale_colors_loop(scale_mul);
#else
  for (i=0; i < size*4; i++) {
    val = image[0][i];
    if (!val) continue;
    val -= cblack[i & 3];
    val *= scale_mul[i & 3];
    image[0][i] = CLIP(val);
  }
#endif
  if ((aber[0] != 1 || aber[2] != 1) && colors == 3) {
    if (verbose)
      fprintf (stderr,_("Correcting chromatic aberration...\n"));
    for (c=0; c < 4; c+=2) {
      if (aber[c] == 1) continue;
      img = (ushort *) malloc (size * sizeof *img);
      merror (img, "scale_colors()");
      for (i=0; i < size; i++)
	img[i] = image[i][c];
      for (row=0; row < iheight; row++) {
	ur = fr = (row - iheight*0.5) * aber[c] + iheight*0.5;
	if (ur > iheight-2) continue;
	fr -= ur;
	for (col=0; col < iwidth; col++) {
	  uc = fc = (col - iwidth*0.5) * aber[c] + iwidth*0.5;
	  if (uc > iwidth-2) continue;
	  fc -= uc;
	  pix = img + ur*iwidth + uc;
	  image[row*iwidth+col][c] =
	    (pix[     0]*(1-fc) + pix[       1]*fc) * (1-fr) +
	    (pix[iwidth]*(1-fc) + pix[iwidth+1]*fc) * fr;
	}
      }
      free(img);
    }
  }
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_SCALE_COLORS,1,2);
#endif
}

void CLASS pre_interpolate()
{
  ushort (*img)[4];
  int row, col, c;
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_PRE_INTERPOLATE,0,2);
#endif
  if (shrink) {
    if (half_size) {
      height = iheight;
      width  = iwidth;
    } else {
      img = (ushort (*)[4]) calloc (height*width, sizeof *img);
      merror (img, "pre_interpolate()");
      for (row=0; row < height; row++)
	for (col=0; col < width; col++) {
	  c = fcol(row,col);
	  img[row*width+col][c] = image[(row >> 1)*iwidth+(col >> 1)][c];
	}
      free (image);
      image = img;
      shrink = 0;
    }
  }
  if (filters > 1000 && colors == 3) {
    if (four_color_rgb && colors++)
      mix_green = !half_size;
    else {
      for (row = FC(1,0) >> 1; row < height; row+=2)
	for (col = FC(row,1) & 1; col < width; col+=2)
	  image[row*width+col][1] = image[row*width+col][3];
      filters &= ~((filters & 0x55555555) << 1);
    }
  }
  if (half_size) filters = 0;
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_PRE_INTERPOLATE,1,2);
#endif
}

void CLASS border_interpolate (int border)
{
  unsigned row, col, y, x, f, c, sum[8];

  for (row=0; row < height; row++)
    for (col=0; col < width; col++) {
      if (col==border && row >= border && row < height-border)
	col = width-border;
      memset (sum, 0, sizeof sum);
      for (y=row-1; y != row+2; y++)
	for (x=col-1; x != col+2; x++)
	  if (y < height && x < width) {
	    f = fcol(y,x);
	    sum[f] += image[y*width+x][f];
	    sum[f+4]++;
	  }
      f = fcol(row,col);
      FORCC if (c != f && sum[c+4])
	image[row*width+col][c] = sum[c] / sum[c+4];
    }
}

void CLASS lin_interpolate()
{
  int code[16][16][32], size=16, *ip, sum[4];
  int f, c, i, x, y, row, col, shift, color;
  ushort *pix;

#ifdef DCRAW_VERBOSE
  if (verbose) fprintf (stderr,_("Bilinear interpolation...\n"));
#endif
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_INTERPOLATE,0,3);
#endif

  if (filters == 2) size = 6;
  border_interpolate(1);
  for (row=0; row < size; row++)
    for (col=0; col < size; col++) {
      ip = code[row][col]+1;
      f = fcol(row,col);
      memset (sum, 0, sizeof sum);
      for (y=-1; y <= 1; y++)
	for (x=-1; x <= 1; x++) {
	  shift = (y==0) + (x==0);
	  color = fcol(row+y,col+x);
	  if (color == f) continue;
	  *ip++ = (width*y + x)*4 + color;
	  *ip++ = shift;
	  *ip++ = color;
	  sum[color] += 1 << shift;
	}
      code[row][col][0] = (ip - code[row][col]) / 3;
      FORCC
	if (c != f) {
	  *ip++ = c;
	  *ip++ = 256 / sum[c];
	}
    }
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_INTERPOLATE,1,3);
#endif
  for (row=1; row < height-1; row++)
    for (col=1; col < width-1; col++) {
      pix = image[row*width+col];
      ip = code[row % size][col % size];
      memset (sum, 0, sizeof sum);
      for (i=*ip++; i--; ip+=3)
	sum[ip[2]] += pix[ip[0]] << ip[1];
      for (i=colors; --i; ip+=2)
	pix[ip[0]] = sum[ip[0]] * ip[1] >> 8;
    }
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_INTERPOLATE,2,3);
#endif
}

/*
   This algorithm is officially called:

   "Interpolation using a Threshold-based variable number of gradients"

   described in http://scien.stanford.edu/pages/labsite/1999/psych221/projects/99/tingchen/algodep/vargra.html

   I've extended the basic idea to work with non-Bayer filter arrays.
   Gradients are numbered clockwise from NW=0 to W=7.
 */
void CLASS vng_interpolate()
{
  static const signed char *cp, terms[] = {
    -2,-2,+0,-1,0,0x01, -2,-2,+0,+0,1,0x01, -2,-1,-1,+0,0,0x01,
    -2,-1,+0,-1,0,0x02, -2,-1,+0,+0,0,0x03, -2,-1,+0,+1,1,0x01,
    -2,+0,+0,-1,0,0x06, -2,+0,+0,+0,1,0x02, -2,+0,+0,+1,0,0x03,
    -2,+1,-1,+0,0,0x04, -2,+1,+0,-1,1,0x04, -2,+1,+0,+0,0,0x06,
    -2,+1,+0,+1,0,0x02, -2,+2,+0,+0,1,0x04, -2,+2,+0,+1,0,0x04,
    -1,-2,-1,+0,0,0x80, -1,-2,+0,-1,0,0x01, -1,-2,+1,-1,0,0x01,
    -1,-2,+1,+0,1,0x01, -1,-1,-1,+1,0,0x88, -1,-1,+1,-2,0,0x40,
    -1,-1,+1,-1,0,0x22, -1,-1,+1,+0,0,0x33, -1,-1,+1,+1,1,0x11,
    -1,+0,-1,+2,0,0x08, -1,+0,+0,-1,0,0x44, -1,+0,+0,+1,0,0x11,
    -1,+0,+1,-2,1,0x40, -1,+0,+1,-1,0,0x66, -1,+0,+1,+0,1,0x22,
    -1,+0,+1,+1,0,0x33, -1,+0,+1,+2,1,0x10, -1,+1,+1,-1,1,0x44,
    -1,+1,+1,+0,0,0x66, -1,+1,+1,+1,0,0x22, -1,+1,+1,+2,0,0x10,
    -1,+2,+0,+1,0,0x04, -1,+2,+1,+0,1,0x04, -1,+2,+1,+1,0,0x04,
    +0,-2,+0,+0,1,0x80, +0,-1,+0,+1,1,0x88, +0,-1,+1,-2,0,0x40,
    +0,-1,+1,+0,0,0x11, +0,-1,+2,-2,0,0x40, +0,-1,+2,-1,0,0x20,
    +0,-1,+2,+0,0,0x30, +0,-1,+2,+1,1,0x10, +0,+0,+0,+2,1,0x08,
    +0,+0,+2,-2,1,0x40, +0,+0,+2,-1,0,0x60, +0,+0,+2,+0,1,0x20,
    +0,+0,+2,+1,0,0x30, +0,+0,+2,+2,1,0x10, +0,+1,+1,+0,0,0x44,
    +0,+1,+1,+2,0,0x10, +0,+1,+2,-1,1,0x40, +0,+1,+2,+0,0,0x60,
    +0,+1,+2,+1,0,0x20, +0,+1,+2,+2,0,0x10, +1,-2,+1,+0,0,0x80,
    +1,-1,+1,+1,0,0x88, +1,+0,+1,+2,0,0x08, +1,+0,+2,-1,0,0x40,
    +1,+0,+2,+1,0,0x10
  }, chood[] = { -1,-1, -1,0, -1,+1, 0,+1, +1,+1, +1,0, +1,-1, 0,-1 };
  ushort (*brow[5])[4], *pix;
  int prow=8, pcol=2, *ip, *code[16][16], gval[8], gmin, gmax, sum[4];
  int row, col, x, y, x1, x2, y1, y2, t, weight, grads, color, diag;
  int g, diff, thold, num, c;

  lin_interpolate();
#ifdef DCRAW_VERBOSE
  if (verbose) fprintf (stderr,_("VNG interpolation...\n"));
#endif

  if (filters == 1) prow = pcol = 16;
  if (filters == 2) prow = pcol =  6;
  ip = (int *) calloc (prow*pcol, 1280);
  merror (ip, "vng_interpolate()");
  for (row=0; row < prow; row++)		/* Precalculate for VNG */
    for (col=0; col < pcol; col++) {
      code[row][col] = ip;
      for (cp=terms, t=0; t < 64; t++) {
	y1 = *cp++;  x1 = *cp++;
	y2 = *cp++;  x2 = *cp++;
	weight = *cp++;
	grads = *cp++;
	color = fcol(row+y1,col+x1);
	if (fcol(row+y2,col+x2) != color) continue;
	diag = (fcol(row,col+1) == color && fcol(row+1,col) == color) ? 2:1;
	if (abs(y1-y2) == diag && abs(x1-x2) == diag) continue;
	*ip++ = (y1*width + x1)*4 + color;
	*ip++ = (y2*width + x2)*4 + color;
	*ip++ = weight;
	for (g=0; g < 8; g++)
	  if (grads & 1<<g) *ip++ = g;
	*ip++ = -1;
      }
      *ip++ = INT_MAX;
      for (cp=chood, g=0; g < 8; g++) {
	y = *cp++;  x = *cp++;
	*ip++ = (y*width + x) * 4;
	color = fcol(row,col);
	if (fcol(row+y,col+x) != color && fcol(row+y*2,col+x*2) == color)
	  *ip++ = (y*width + x) * 8 + color;
	else
	  *ip++ = 0;
      }
    }
  brow[4] = (ushort (*)[4]) calloc (width*3, sizeof **brow);
  merror (brow[4], "vng_interpolate()");
  for (row=0; row < 3; row++)
    brow[row] = brow[4] + row*width;
  for (row=2; row < height-2; row++) {		/* Do VNG interpolation */
#ifdef LIBRAW_LIBRARY_BUILD
      if(!((row-2)%256))RUN_CALLBACK(LIBRAW_PROGRESS_INTERPOLATE,(row-2)/256+1,((height-3)/256)+1);
#endif
    for (col=2; col < width-2; col++) {
      pix = image[row*width+col];
      ip = code[row % prow][col % pcol];
      memset (gval, 0, sizeof gval);
      while ((g = ip[0]) != INT_MAX) {		/* Calculate gradients */
	diff = ABS(pix[g] - pix[ip[1]]) << ip[2];
	gval[ip[3]] += diff;
	ip += 5;
	if ((g = ip[-1]) == -1) continue;
	gval[g] += diff;
	while ((g = *ip++) != -1)
	  gval[g] += diff;
      }
      ip++;
      gmin = gmax = gval[0];			/* Choose a threshold */
      for (g=1; g < 8; g++) {
	if (gmin > gval[g]) gmin = gval[g];
	if (gmax < gval[g]) gmax = gval[g];
      }
      if (gmax == 0) {
	memcpy (brow[2][col], pix, sizeof *image);
	continue;
      }
      thold = gmin + (gmax >> 1);
      memset (sum, 0, sizeof sum);
      color = fcol(row,col);
      for (num=g=0; g < 8; g++,ip+=2) {		/* Average the neighbors */
	if (gval[g] <= thold) {
	  FORCC
	    if (c == color && ip[1])
	      sum[c] += (pix[c] + pix[ip[1]]) >> 1;
	    else
	      sum[c] += pix[ip[0] + c];
	  num++;
	}
      }
      FORCC {					/* Save to buffer */
	t = pix[color];
	if (c != color)
	  t += (sum[c] - sum[color]) / num;
	brow[2][col][c] = CLIP(t);
      }
    }
    if (row > 3)				/* Write buffer to image */
      memcpy (image[(row-2)*width+2], brow[0]+2, (width-4)*sizeof *image);
    for (g=0; g < 4; g++)
      brow[(g-1) & 3] = brow[g];
  }
  memcpy (image[(row-2)*width+2], brow[0]+2, (width-4)*sizeof *image);
  memcpy (image[(row-1)*width+2], brow[1]+2, (width-4)*sizeof *image);
  free (brow[4]);
  free (code[0][0]);
}

/*
   Patterned Pixel Grouping Interpolation by Alain Desbiolles
*/
void CLASS ppg_interpolate()
{
  int dir[5] = { 1, width, -1, -width, 1 };
  int row, col, diff[2], guess[2], c, d, i;
  ushort (*pix)[4];

  border_interpolate(3);
#ifdef DCRAW_VERBOSE
  if (verbose) fprintf (stderr,_("PPG interpolation...\n"));
#endif

/*  Fill in the green layer with gradients and pattern recognition: */
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_INTERPOLATE,0,3);
#ifdef LIBRAW_USE_OPENMP
#pragma omp parallel for default(shared) private(guess, diff, row, col, d, c, i, pix) schedule(static)
#endif
#endif
  for (row=3; row < height-3; row++)
    for (col=3+(FC(row,3) & 1), c=FC(row,col); col < width-3; col+=2) {
      pix = image + row*width+col;
      for (i=0; (d=dir[i]) > 0; i++) {
	guess[i] = (pix[-d][1] + pix[0][c] + pix[d][1]) * 2
		      - pix[-2*d][c] - pix[2*d][c];
	diff[i] = ( ABS(pix[-2*d][c] - pix[ 0][c]) +
		    ABS(pix[ 2*d][c] - pix[ 0][c]) +
		    ABS(pix[  -d][1] - pix[ d][1]) ) * 3 +
		  ( ABS(pix[ 3*d][1] - pix[ d][1]) +
		    ABS(pix[-3*d][1] - pix[-d][1]) ) * 2;
      }
      d = dir[i = diff[0] > diff[1]];
      pix[0][1] = ULIM(guess[i] >> 2, pix[d][1], pix[-d][1]);
    }
/*  Calculate red and blue for each green pixel:		*/
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_INTERPOLATE,1,3);
#ifdef LIBRAW_USE_OPENMP
#pragma omp parallel for default(shared) private(guess, diff, row, col, d, c, i, pix) schedule(static)
#endif
#endif
  for (row=1; row < height-1; row++)
    for (col=1+(FC(row,2) & 1), c=FC(row,col+1); col < width-1; col+=2) {
      pix = image + row*width+col;
      for (i=0; (d=dir[i]) > 0; c=2-c, i++)
	pix[0][c] = CLIP((pix[-d][c] + pix[d][c] + 2*pix[0][1]
			- pix[-d][1] - pix[d][1]) >> 1);
    }
/*  Calculate blue for red pixels and vice versa:		*/
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_INTERPOLATE,2,3);
#ifdef LIBRAW_USE_OPENMP
#pragma omp parallel for default(shared) private(guess, diff, row, col, d, c, i, pix) schedule(static)
#endif
#endif
  for (row=1; row < height-1; row++)
    for (col=1+(FC(row,1) & 1), c=2-FC(row,col); col < width-1; col+=2) {
      pix = image + row*width+col;
      for (i=0; (d=dir[i]+dir[i+1]) > 0; i++) {
	diff[i] = ABS(pix[-d][c] - pix[d][c]) +
		  ABS(pix[-d][1] - pix[0][1]) +
		  ABS(pix[ d][1] - pix[0][1]);
	guess[i] = pix[-d][c] + pix[d][c] + 2*pix[0][1]
		 - pix[-d][1] - pix[d][1];
      }
      if (diff[0] != diff[1])
	pix[0][c] = CLIP(guess[diff[0] > diff[1]] >> 1);
      else
	pix[0][c] = CLIP((guess[0]+guess[1]) >> 2);
    }
}

/*
   Adaptive Homogeneity-Directed interpolation is based on
   the work of Keigo Hirakawa, Thomas Parks, and Paul Lee.
 */
#define TS 256		/* Tile Size */

void CLASS ahd_interpolate()
{
  int i, j, k, top, left, row, col, tr, tc, c, d, val, hm[2];
  ushort (*pix)[4], (*rix)[3];
  static const int dir[4] = { -1, 1, -TS, TS };
  unsigned ldiff[2][4], abdiff[2][4], leps, abeps;
  float r, cbrt[0x10000], xyz[3], xyz_cam[3][4];
  ushort (*rgb)[TS][TS][3];
   short (*lab)[TS][TS][3], (*lix)[3];
   char (*homo)[TS][TS], *buffer;

  if (verbose) fprintf (stderr,_("AHD interpolation...\n"));

  for (i=0; i < 0x10000; i++) {
    r = i / 65535.0;
    cbrt[i] = r > 0.008856 ? pow(r,1/3.0) : 7.787*r + 16/116.0;
  }
  for (i=0; i < 3; i++)
    for (j=0; j < colors; j++)
      for (xyz_cam[i][j] = k=0; k < 3; k++)
	xyz_cam[i][j] += xyz_rgb[i][k] * rgb_cam[k][j] / d65_white[i];

  border_interpolate(5);
  buffer = (char *) malloc (26*TS*TS);		/* 1664 kB */
  merror (buffer, "ahd_interpolate()");
  rgb  = (ushort(*)[TS][TS][3]) buffer;
  lab  = (short (*)[TS][TS][3])(buffer + 12*TS*TS);
  homo = (char  (*)[TS][TS])   (buffer + 24*TS*TS);

  for (top=2; top < height-5; top += TS-6)
    for (left=2; left < width-5; left += TS-6) {

/*  Interpolate green horizontally and vertically:		*/
      for (row = top; row < top+TS && row < height-2; row++) {
	col = left + (FC(row,left) & 1);
	for (c = FC(row,col); col < left+TS && col < width-2; col+=2) {
	  pix = image + row*width+col;
	  val = ((pix[-1][1] + pix[0][c] + pix[1][1]) * 2
		- pix[-2][c] - pix[2][c]) >> 2;
	  rgb[0][row-top][col-left][1] = ULIM(val,pix[-1][1],pix[1][1]);
	  val = ((pix[-width][1] + pix[0][c] + pix[width][1]) * 2
		- pix[-2*width][c] - pix[2*width][c]) >> 2;
	  rgb[1][row-top][col-left][1] = ULIM(val,pix[-width][1],pix[width][1]);
	}
      }
/*  Interpolate red and blue, and convert to CIELab:		*/
      for (d=0; d < 2; d++)
	for (row=top+1; row < top+TS-1 && row < height-3; row++)
	  for (col=left+1; col < left+TS-1 && col < width-3; col++) {
	    pix = image + row*width+col;
	    rix = &rgb[d][row-top][col-left];
	    lix = &lab[d][row-top][col-left];
	    if ((c = 2 - FC(row,col)) == 1) {
	      c = FC(row+1,col);
	      val = pix[0][1] + (( pix[-1][2-c] + pix[1][2-c]
				 - rix[-1][1] - rix[1][1] ) >> 1);
	      rix[0][2-c] = CLIP(val);
	      val = pix[0][1] + (( pix[-width][c] + pix[width][c]
				 - rix[-TS][1] - rix[TS][1] ) >> 1);
	    } else
	      val = rix[0][1] + (( pix[-width-1][c] + pix[-width+1][c]
				 + pix[+width-1][c] + pix[+width+1][c]
				 - rix[-TS-1][1] - rix[-TS+1][1]
				 - rix[+TS-1][1] - rix[+TS+1][1] + 1) >> 2);
	    rix[0][c] = CLIP(val);
	    c = FC(row,col);
	    rix[0][c] = pix[0][c];
	    xyz[0] = xyz[1] = xyz[2] = 0.5;
	    FORCC {
	      xyz[0] += xyz_cam[0][c] * rix[0][c];
	      xyz[1] += xyz_cam[1][c] * rix[0][c];
	      xyz[2] += xyz_cam[2][c] * rix[0][c];
	    }
	    xyz[0] = cbrt[CLIP((int) xyz[0])];
	    xyz[1] = cbrt[CLIP((int) xyz[1])];
	    xyz[2] = cbrt[CLIP((int) xyz[2])];
	    lix[0][0] = 64 * (116 * xyz[1] - 16);
	    lix[0][1] = 64 * 500 * (xyz[0] - xyz[1]);
	    lix[0][2] = 64 * 200 * (xyz[1] - xyz[2]);
	  }
/*  Build homogeneity maps from the CIELab images:		*/
      memset (homo, 0, 2*TS*TS);
      for (row=top+2; row < top+TS-2 && row < height-4; row++) {
	tr = row-top;
	for (col=left+2; col < left+TS-2 && col < width-4; col++) {
	  tc = col-left;
	  for (d=0; d < 2; d++) {
	    lix = &lab[d][tr][tc];
	    for (i=0; i < 4; i++) {
	       ldiff[d][i] = ABS(lix[0][0]-lix[dir[i]][0]);
	      abdiff[d][i] = SQR(lix[0][1]-lix[dir[i]][1])
			   + SQR(lix[0][2]-lix[dir[i]][2]);
	    }
	  }
	  leps = MIN(MAX(ldiff[0][0],ldiff[0][1]),
		     MAX(ldiff[1][2],ldiff[1][3]));
	  abeps = MIN(MAX(abdiff[0][0],abdiff[0][1]),
		      MAX(abdiff[1][2],abdiff[1][3]));
	  for (d=0; d < 2; d++)
	    for (i=0; i < 4; i++)
	      if (ldiff[d][i] <= leps && abdiff[d][i] <= abeps)
		homo[d][tr][tc]++;
	}
      }
/*  Combine the most homogenous pixels for the final result:	*/
      for (row=top+3; row < top+TS-3 && row < height-5; row++) {
	tr = row-top;
	for (col=left+3; col < left+TS-3 && col < width-5; col++) {
	  tc = col-left;
	  for (d=0; d < 2; d++)
	    for (hm[d]=0, i=tr-1; i <= tr+1; i++)
	      for (j=tc-1; j <= tc+1; j++)
		hm[d] += homo[d][i][j];
	  if (hm[0] != hm[1])
	    FORC3 image[row*width+col][c] = rgb[hm[1] > hm[0]][tr][tc][c];
	  else
	    FORC3 image[row*width+col][c] =
		(rgb[0][tr][tc][c] + rgb[1][tr][tc][c]) >> 1;
	}
      }
    }
  free (buffer);
}
#undef TS

void CLASS median_filter()
{
  ushort (*pix)[4];
  int pass, c, i, j, k, med[9];
  static const uchar opt[] =	/* Optimal 9-element median search */
  { 1,2, 4,5, 7,8, 0,1, 3,4, 6,7, 1,2, 4,5, 7,8,
    0,3, 5,8, 4,7, 3,6, 1,4, 2,5, 4,7, 4,2, 6,4, 4,2 };

  for (pass=1; pass <= med_passes; pass++) {
#ifdef LIBRAW_LIBRARY_BUILD
      RUN_CALLBACK(LIBRAW_PROGRESS_MEDIAN_FILTER,pass-1,med_passes);
#endif
#ifdef DCRAW_VERBOSE
    if (verbose)
      fprintf (stderr,_("Median filter pass %d...\n"), pass);
#endif
    for (c=0; c < 3; c+=2) {
      for (pix = image; pix < image+width*height; pix++)
	pix[0][3] = pix[0][c];
      for (pix = image+width; pix < image+width*(height-1); pix++) {
	if ((pix-image+1) % width < 2) continue;
	for (k=0, i = -width; i <= width; i += width)
	  for (j = i-1; j <= i+1; j++)
	    med[k++] = pix[j][3] - pix[j][1];
	for (i=0; i < sizeof opt; i+=2)
	  if     (med[opt[i]] > med[opt[i+1]])
	    SWAP (med[opt[i]] , med[opt[i+1]]);
	pix[0][c] = CLIP(med[4] + pix[0][1]);
      }
    }
  }
}

void CLASS blend_highlights()
{
  int clip=INT_MAX, row, col, c, i, j;
  static const float trans[2][4][4] =
  { { { 1,1,1 }, { 1.7320508,-1.7320508,0 }, { -1,-1,2 } },
    { { 1,1,1,1 }, { 1,-1,1,-1 }, { 1,1,-1,-1 }, { 1,-1,-1,1 } } };
  static const float itrans[2][4][4] =
  { { { 1,0.8660254,-0.5 }, { 1,-0.8660254,-0.5 }, { 1,0,1 } },
    { { 1,1,1,1 }, { 1,-1,1,-1 }, { 1,1,-1,-1 }, { 1,-1,-1,1 } } };
  float cam[2][4], lab[2][4], sum[2], chratio;

  if ((unsigned) (colors-3) > 1) return;
#ifdef DCRAW_VERBOSE
  if (verbose) fprintf (stderr,_("Blending highlights...\n"));
#endif
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_HIGHLIGHTS,0,2);
#endif
  FORCC if (clip > (i = 65535*pre_mul[c])) clip = i;
  for (row=0; row < height; row++)
    for (col=0; col < width; col++) {
      FORCC if (image[row*width+col][c] > clip) break;
      if (c == colors) continue;
      FORCC {
	cam[0][c] = image[row*width+col][c];
	cam[1][c] = MIN(cam[0][c],clip);
      }
      for (i=0; i < 2; i++) {
	FORCC for (lab[i][c]=j=0; j < colors; j++)
	  lab[i][c] += trans[colors-3][c][j] * cam[i][j];
	for (sum[i]=0,c=1; c < colors; c++)
	  sum[i] += SQR(lab[i][c]);
      }
      chratio = sqrt(sum[1]/sum[0]);
      for (c=1; c < colors; c++)
	lab[0][c] *= chratio;
      FORCC for (cam[0][c]=j=0; j < colors; j++)
	cam[0][c] += itrans[colors-3][c][j] * lab[0][j];
      FORCC image[row*width+col][c] = cam[0][c] / colors;
    }
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_HIGHLIGHTS,1,2);
#endif
}

#define SCALE (4 >> shrink)
void CLASS recover_highlights()
{
  float *map, sum, wgt, grow;
  int hsat[4], count, spread, change, val, i;
  unsigned high, wide, mrow, mcol, row, col, kc, c, d, y, x;
  ushort *pixel;
  static const signed char dir[8][2] =
    { {-1,-1}, {-1,0}, {-1,1}, {0,1}, {1,1}, {1,0}, {1,-1}, {0,-1} };

#ifdef DCRAW_VERBOSE
  if (verbose) fprintf (stderr,_("Rebuilding highlights...\n"));
#endif

  grow = pow (2, 4-highlight);
  FORCC hsat[c] = 32000 * pre_mul[c];
  for (kc=0, c=1; c < colors; c++)
    if (pre_mul[kc] < pre_mul[c]) kc = c;
  high = height / SCALE;
  wide =  width / SCALE;
  map = (float *) calloc (high*wide, sizeof *map);
  merror (map, "recover_highlights()");
  FORCC if (c != kc) {
#ifdef LIBRAW_LIBRARY_BUILD
      RUN_CALLBACK(LIBRAW_PROGRESS_HIGHLIGHTS,c-1,colors-1);
#endif
    memset (map, 0, high*wide*sizeof *map);
    for (mrow=0; mrow < high; mrow++)
      for (mcol=0; mcol < wide; mcol++) {
	sum = wgt = count = 0;
	for (row = mrow*SCALE; row < (mrow+1)*SCALE; row++)
	  for (col = mcol*SCALE; col < (mcol+1)*SCALE; col++) {
	    pixel = image[row*width+col];
	    if (pixel[c] / hsat[c] == 1 && pixel[kc] > 24000) {
	      sum += pixel[c];
	      wgt += pixel[kc];
	      count++;
	    }
	  }
	if (count == SCALE*SCALE)
	  map[mrow*wide+mcol] = sum / wgt;
      }
    for (spread = 32/grow; spread--; ) {
      for (mrow=0; mrow < high; mrow++)
	for (mcol=0; mcol < wide; mcol++) {
	  if (map[mrow*wide+mcol]) continue;
	  sum = count = 0;
	  for (d=0; d < 8; d++) {
	    y = mrow + dir[d][0];
	    x = mcol + dir[d][1];
	    if (y < high && x < wide && map[y*wide+x] > 0) {
	      sum  += (1 + (d & 1)) * map[y*wide+x];
	      count += 1 + (d & 1);
	    }
	  }
	  if (count > 3)
	    map[mrow*wide+mcol] = - (sum+grow) / (count+grow);
	}
      for (change=i=0; i < high*wide; i++)
	if (map[i] < 0) {
	  map[i] = -map[i];
	  change = 1;
	}
      if (!change) break;
    }
    for (i=0; i < high*wide; i++)
      if (map[i] == 0) map[i] = 1;
    for (mrow=0; mrow < high; mrow++)
      for (mcol=0; mcol < wide; mcol++) {
	for (row = mrow*SCALE; row < (mrow+1)*SCALE; row++)
	  for (col = mcol*SCALE; col < (mcol+1)*SCALE; col++) {
	    pixel = image[row*width+col];
	    if (pixel[c] / hsat[c] > 1) {
	      val = pixel[kc] * map[mrow*wide+mcol];
	      if (pixel[c] < val) pixel[c] = CLIP(val);
	    }
	  }
      }
  }
  free (map);
}
#undef SCALE

void CLASS tiff_get (unsigned base,
	unsigned *tag, unsigned *type, unsigned *len, unsigned *save)
{
  *tag  = get2();
  *type = get2();
  *len  = get4();
  *save = ftell(ifp) + 4;
  if (*len * ("11124811248488"[*type < 14 ? *type:0]-'0') > 4)
    fseek (ifp, get4()+base, SEEK_SET);
}

void CLASS parse_thumb_note (int base, unsigned toff, unsigned tlen)
{
  unsigned entries, tag, type, len, save;

  entries = get2();
  while (entries--) {
    tiff_get (base, &tag, &type, &len, &save);
    if (tag == toff) thumb_offset = get4()+base;
    if (tag == tlen) thumb_length = get4();
    fseek (ifp, save, SEEK_SET);
  }
}
#line 4189 "dcraw/dcraw.c"
void CLASS parse_makernote (int base, int uptag)
{
  static const uchar xlat[2][256] = {
  { 0xc1,0xbf,0x6d,0x0d,0x59,0xc5,0x13,0x9d,0x83,0x61,0x6b,0x4f,0xc7,0x7f,0x3d,0x3d,
    0x53,0x59,0xe3,0xc7,0xe9,0x2f,0x95,0xa7,0x95,0x1f,0xdf,0x7f,0x2b,0x29,0xc7,0x0d,
    0xdf,0x07,0xef,0x71,0x89,0x3d,0x13,0x3d,0x3b,0x13,0xfb,0x0d,0x89,0xc1,0x65,0x1f,
    0xb3,0x0d,0x6b,0x29,0xe3,0xfb,0xef,0xa3,0x6b,0x47,0x7f,0x95,0x35,0xa7,0x47,0x4f,
    0xc7,0xf1,0x59,0x95,0x35,0x11,0x29,0x61,0xf1,0x3d,0xb3,0x2b,0x0d,0x43,0x89,0xc1,
    0x9d,0x9d,0x89,0x65,0xf1,0xe9,0xdf,0xbf,0x3d,0x7f,0x53,0x97,0xe5,0xe9,0x95,0x17,
    0x1d,0x3d,0x8b,0xfb,0xc7,0xe3,0x67,0xa7,0x07,0xf1,0x71,0xa7,0x53,0xb5,0x29,0x89,
    0xe5,0x2b,0xa7,0x17,0x29,0xe9,0x4f,0xc5,0x65,0x6d,0x6b,0xef,0x0d,0x89,0x49,0x2f,
    0xb3,0x43,0x53,0x65,0x1d,0x49,0xa3,0x13,0x89,0x59,0xef,0x6b,0xef,0x65,0x1d,0x0b,
    0x59,0x13,0xe3,0x4f,0x9d,0xb3,0x29,0x43,0x2b,0x07,0x1d,0x95,0x59,0x59,0x47,0xfb,
    0xe5,0xe9,0x61,0x47,0x2f,0x35,0x7f,0x17,0x7f,0xef,0x7f,0x95,0x95,0x71,0xd3,0xa3,
    0x0b,0x71,0xa3,0xad,0x0b,0x3b,0xb5,0xfb,0xa3,0xbf,0x4f,0x83,0x1d,0xad,0xe9,0x2f,
    0x71,0x65,0xa3,0xe5,0x07,0x35,0x3d,0x0d,0xb5,0xe9,0xe5,0x47,0x3b,0x9d,0xef,0x35,
    0xa3,0xbf,0xb3,0xdf,0x53,0xd3,0x97,0x53,0x49,0x71,0x07,0x35,0x61,0x71,0x2f,0x43,
    0x2f,0x11,0xdf,0x17,0x97,0xfb,0x95,0x3b,0x7f,0x6b,0xd3,0x25,0xbf,0xad,0xc7,0xc5,
    0xc5,0xb5,0x8b,0xef,0x2f,0xd3,0x07,0x6b,0x25,0x49,0x95,0x25,0x49,0x6d,0x71,0xc7 },
  { 0xa7,0xbc,0xc9,0xad,0x91,0xdf,0x85,0xe5,0xd4,0x78,0xd5,0x17,0x46,0x7c,0x29,0x4c,
    0x4d,0x03,0xe9,0x25,0x68,0x11,0x86,0xb3,0xbd,0xf7,0x6f,0x61,0x22,0xa2,0x26,0x34,
    0x2a,0xbe,0x1e,0x46,0x14,0x68,0x9d,0x44,0x18,0xc2,0x40,0xf4,0x7e,0x5f,0x1b,0xad,
    0x0b,0x94,0xb6,0x67,0xb4,0x0b,0xe1,0xea,0x95,0x9c,0x66,0xdc,0xe7,0x5d,0x6c,0x05,
    0xda,0xd5,0xdf,0x7a,0xef,0xf6,0xdb,0x1f,0x82,0x4c,0xc0,0x68,0x47,0xa1,0xbd,0xee,
    0x39,0x50,0x56,0x4a,0xdd,0xdf,0xa5,0xf8,0xc6,0xda,0xca,0x90,0xca,0x01,0x42,0x9d,
    0x8b,0x0c,0x73,0x43,0x75,0x05,0x94,0xde,0x24,0xb3,0x80,0x34,0xe5,0x2c,0xdc,0x9b,
    0x3f,0xca,0x33,0x45,0xd0,0xdb,0x5f,0xf5,0x52,0xc3,0x21,0xda,0xe2,0x22,0x72,0x6b,
    0x3e,0xd0,0x5b,0xa8,0x87,0x8c,0x06,0x5d,0x0f,0xdd,0x09,0x19,0x93,0xd0,0xb9,0xfc,
    0x8b,0x0f,0x84,0x60,0x33,0x1c,0x9b,0x45,0xf1,0xf0,0xa3,0x94,0x3a,0x12,0x77,0x33,
    0x4d,0x44,0x78,0x28,0x3c,0x9e,0xfd,0x65,0x57,0x16,0x94,0x6b,0xfb,0x59,0xd0,0xc8,
    0x22,0x36,0xdb,0xd2,0x63,0x98,0x43,0xa1,0x04,0x87,0x86,0xf7,0xa6,0x26,0xbb,0xd6,
    0x59,0x4d,0xbf,0x6a,0x2e,0xaa,0x2b,0xef,0xe6,0x78,0xb6,0x4e,0xe0,0x2f,0xdc,0x7c,
    0xbe,0x57,0x19,0x32,0x7e,0x2a,0xd0,0xb8,0xba,0x29,0x00,0x3c,0x52,0x7d,0xa8,0x49,
    0x3b,0x2d,0xeb,0x25,0x49,0xfa,0xa3,0xaa,0x39,0xa7,0xc5,0xa7,0x50,0x11,0x36,0xfb,
    0xc6,0x67,0x4a,0xf5,0xa5,0x12,0x65,0x7e,0xb0,0xdf,0xaf,0x4e,0xb3,0x61,0x7f,0x2f } };
  unsigned offset=0, entries, tag, type, len, save, c;
  unsigned ver97=0, serial=0, i, wbi=0, wb[4]={0,0,0,0};
  uchar buf97[324], ci, cj, ck;
  short morder, sorder=order;
  char buf[10];
/*
   The MakerNote might have its own TIFF header (possibly with
   its own byte-order!), or it might just be a table.
 */
  if (!strcmp(make,"Nokia")) return;
  fread (buf, 1, 10, ifp);
  if (!strncmp (buf,"KDK" ,3) ||	/* these aren't TIFF tables */
      !strncmp (buf,"VER" ,3) ||
      !strncmp (buf,"IIII",4) ||
      !strncmp (buf,"MMMM",4)) return;
  if (!strncmp (buf,"KC"  ,2) ||	/* Konica KD-400Z, KD-510Z */
      !strncmp (buf,"MLY" ,3)) {	/* Minolta DiMAGE G series */
    order = 0x4d4d;
    while ((i=ftell(ifp)) < data_offset && i < 16384) {
      wb[0] = wb[2];  wb[2] = wb[1];  wb[1] = wb[3];
      wb[3] = get2();
      if (wb[1] == 256 && wb[3] == 256 &&
	  wb[0] > 256 && wb[0] < 640 && wb[2] > 256 && wb[2] < 640)
	FORC4 cam_mul[c] = wb[c];
    }
    goto quit;
  }
  if (!strcmp (buf,"Nikon")) {
    base = ftell(ifp);
    order = get2();
    if (get2() != 42) goto quit;
    offset = get4();
    fseek (ifp, offset-8, SEEK_CUR);
  } else if (!strcmp (buf,"OLYMPUS")) {
    base = ftell(ifp)-10;
    fseek (ifp, -2, SEEK_CUR);
    order = get2();  get2();
  } else if (!strncmp (buf,"SONY",4) ||
	     !strcmp  (buf,"Panasonic")) {
    goto nf;
  } else if (!strncmp (buf,"FUJIFILM",8)) {
    base = ftell(ifp)-10;
nf: order = 0x4949;
    fseek (ifp,  2, SEEK_CUR);
  } else if (!strcmp (buf,"OLYMP") ||
	     !strcmp (buf,"LEICA") ||
	     !strcmp (buf,"Ricoh") ||
	     !strcmp (buf,"EPSON"))
    fseek (ifp, -2, SEEK_CUR);
  else if (!strcmp (buf,"AOC") ||
	   !strcmp (buf,"QVC"))
    fseek (ifp, -4, SEEK_CUR);
  else {
    fseek (ifp, -10, SEEK_CUR);
    if (!strncmp(make,"SAMSUNG",7))
      base = ftell(ifp);
  }
  entries = get2();
  if (entries > 1000) return;
  morder = order;
  while (entries--) {
    order = morder;
    tiff_get (base, &tag, &type, &len, &save);
    tag |= uptag << 16;
    if (tag == 2 && strstr(make,"NIKON") && !iso_speed)
      iso_speed = (get2(),get2());
    if (tag == 4 && len > 26 && len < 35) {
      if ((i=(get4(),get2())) != 0x7fff && !iso_speed)
	iso_speed = 50 * pow (2, i/32.0 - 4);
      if ((i=(get2(),get2())) != 0x7fff && !aperture)
	aperture = pow (2, i/64.0);
      if ((i=get2()) != 0xffff && !shutter)
	shutter = pow (2, (short) i/-32.0);
      wbi = (get2(),get2());
      shot_order = (get2(),get2());
    }
    if ((tag == 4 || tag == 0x114) && !strncmp(make,"KONICA",6)) {
      fseek (ifp, tag == 4 ? 140:160, SEEK_CUR);
      switch (get2()) {
	case 72:  flip = 0;  break;
	case 76:  flip = 6;  break;
	case 82:  flip = 5;  break;
      }
    }
    if (tag == 7 && type == 2 && len > 20)
      fgets (model2, 64, ifp);
    if (tag == 8 && type == 4)
      shot_order = get4();
    if (tag == 9 && !strcmp(make,"Canon"))
      fread (artist, 64, 1, ifp);
    if (tag == 0xc && len == 4) {
      cam_mul[0] = getreal(type);
      cam_mul[2] = getreal(type);
    }
    if (tag == 0xd && type == 7 && get2() == 0xaaaa) {
      for (c=i=2; (ushort) c != 0xbbbb && i < len; i++)
	c = c << 8 | fgetc(ifp);
      while ((i+=4) < len-5)
	if (get4() == 257 && (i=len) && (c = (get4(),fgetc(ifp))) < 3)
	  flip = "065"[c]-'0';
    }
    if (tag == 0x10 && type == 4)
      unique_id = get4();
    if (tag == 0x11 && is_raw && !strncmp(make,"NIKON",5)) {
      fseek (ifp, get4()+base, SEEK_SET);
      parse_tiff_ifd (base);
    }
    if (tag == 0x14 && type == 7) {
      if (len == 2560) {
	fseek (ifp, 1248, SEEK_CUR);
	goto get2_256;
      }
      fread (buf, 1, 10, ifp);
      if (!strncmp(buf,"NRW ",4)) {
	fseek (ifp, strcmp(buf+4,"0100") ? 46:1546, SEEK_CUR);
	cam_mul[0] = get4() << 2;
	cam_mul[1] = get4() + get4();
	cam_mul[2] = get4() << 2;
      }
    }
    if (tag == 0x15 && type == 2 && is_raw)
      fread (model, 64, 1, ifp);
    if (strstr(make,"PENTAX")) {
      if (tag == 0x1b) tag = 0x1018;
      if (tag == 0x1c) tag = 0x1017;
    }
    if (tag == 0x1d)
      while ((c = fgetc(ifp)) && c != EOF)
	serial = serial*10 + (isdigit(c) ? c - '0' : c % 10);
    if (tag == 0x81 && type == 4) {
      data_offset = get4();
      fseek (ifp, data_offset + 41, SEEK_SET);
      raw_height = get2() * 2;
      raw_width  = get2();
      filters = 0x61616161;
    }
    if (tag == 0x29 && type == 1) {
      c = wbi < 18 ? "012347800000005896"[wbi]-'0' : 0;
      fseek (ifp, 8 + c*32, SEEK_CUR);
      FORC4 cam_mul[c ^ (c >> 1) ^ 1] = get4();
    }
    if ((tag == 0x81  && type == 7) ||
	(tag == 0x100 && type == 7) ||
	(tag == 0x280 && type == 1)) {
      thumb_offset = ftell(ifp);
      thumb_length = len;
    }
    if (tag == 0x88 && type == 4 && (thumb_offset = get4()))
      thumb_offset += base;
    if (tag == 0x89 && type == 4)
      thumb_length = get4();
    if (tag == 0x8c || tag == 0x96)
      meta_offset = ftell(ifp);
    if (tag == 0x97) {
      for (i=0; i < 4; i++)
	ver97 = ver97 * 10 + fgetc(ifp)-'0';
      switch (ver97) {
	case 100:
	  fseek (ifp, 68, SEEK_CUR);
	  FORC4 cam_mul[(c >> 1) | ((c & 1) << 1)] = get2();
	  break;
	case 102:
	  fseek (ifp, 6, SEEK_CUR);
	  goto get2_rggb;
	case 103:
	  fseek (ifp, 16, SEEK_CUR);
	  FORC4 cam_mul[c] = get2();
      }
      if (ver97 >= 200) {
	if (ver97 != 205) fseek (ifp, 280, SEEK_CUR);
	fread (buf97, 324, 1, ifp);
      }
    }
    if (tag == 0xa1 && type == 7) {
      order = 0x4949;
      fseek (ifp, 140, SEEK_CUR);
      FORC3 cam_mul[c] = get4();
    }
    if (tag == 0xa4 && type == 3) {
      fseek (ifp, wbi*48, SEEK_CUR);
      FORC3 cam_mul[c] = get2();
    }
    if (tag == 0xa7 && (unsigned) (ver97-200) < 17) {
      ci = xlat[0][serial & 0xff];
      cj = xlat[1][fgetc(ifp)^fgetc(ifp)^fgetc(ifp)^fgetc(ifp)];
      ck = 0x60;
      for (i=0; i < 324; i++)
	buf97[i] ^= (cj += ci * ck++);
      i = "66666>666;6A;:;55"[ver97-200] - '0';
      FORC4 cam_mul[c ^ (c >> 1) ^ (i & 1)] =
	sget2 (buf97 + (i & -2) + c*2);
    }
    if (tag == 0x200 && len == 3)
      shot_order = (get4(),get4());
    if (tag == 0x200 && len == 4)
      FORC4 cblack[c ^ c >> 1] = get2();
    if (tag == 0x201 && len == 4)
      goto get2_rggb;
    if (tag == 0x220 && type == 7)
      meta_offset = ftell(ifp);
    if (tag == 0x401 && type == 4 && len == 4)
      FORC4 cblack[c ^ c >> 1] = get4();
    if (tag == 0xe01) {		/* Nikon Capture Note */
      order = 0x4949;
      fseek (ifp, 22, SEEK_CUR);
      for (offset=22; offset+22 < len; offset += 22+i) {
	tag = get4();
	fseek (ifp, 14, SEEK_CUR);
	i = get4()-4;
	if (tag == 0x76a43207) flip = get2();
	else fseek (ifp, i, SEEK_CUR);
      }
    }
    if (tag == 0xe80 && len == 256 && type == 7) {
      fseek (ifp, 48, SEEK_CUR);
      cam_mul[0] = get2() * 508 * 1.078 / 0x10000;
      cam_mul[2] = get2() * 382 * 1.173 / 0x10000;
    }
    if (tag == 0xf00 && type == 7) {
      if (len == 614)
	fseek (ifp, 176, SEEK_CUR);
      else if (len == 734 || len == 1502)
	fseek (ifp, 148, SEEK_CUR);
      else goto next;
      goto get2_256;
    }
    if ((tag == 0x1011 && len == 9) || tag == 0x20400200)
      for (i=0; i < 3; i++)
	FORC3 cmatrix[i][c] = ((short) get2()) / 256.0;
    if ((tag == 0x1012 || tag == 0x20400600) && len == 4)
      FORC4 cblack[c ^ c >> 1] = get2();
    if (tag == 0x1017 || tag == 0x20400100)
      cam_mul[0] = get2() / 256.0;
    if (tag == 0x1018 || tag == 0x20400100)
      cam_mul[2] = get2() / 256.0;
    if (tag == 0x2011 && len == 2) {
get2_256:
      order = 0x4d4d;
      cam_mul[0] = get2() / 256.0;
      cam_mul[2] = get2() / 256.0;
    }
    if ((tag | 0x70) == 0x2070 && type == 4)
      fseek (ifp, get4()+base, SEEK_SET);
    if (tag == 0x2010 && type != 7)
      load_raw = &CLASS olympus_load_raw;
    if (tag == 0x2020)
      parse_thumb_note (base, 257, 258);
    if (tag == 0x2040)
      parse_makernote (base, 0x2040);
    if (tag == 0xb028) {
      fseek (ifp, get4()+base, SEEK_SET);
      parse_thumb_note (base, 136, 137);
    }
    if (tag == 0x4001 && len > 500) {
      i = len == 582 ? 50 : len == 653 ? 68 : len == 5120 ? 142 : 126;
      fseek (ifp, i, SEEK_CUR);
get2_rggb:
      FORC4 cam_mul[c ^ (c >> 1)] = get2();
      i = len == 1312 ? 112:22;
      fseek (ifp, i, SEEK_CUR);
      FORC4 sraw_mul[c ^ (c >> 1)] = get2();
    }
    if (tag == 0xa021)
      FORC4 cam_mul[c ^ (c >> 1)] = get4();
    if (tag == 0xa028)
      FORC4 cam_mul[c ^ (c >> 1)] -= get4();
next:
    fseek (ifp, save, SEEK_SET);
  }
quit:
  order = sorder;
}

/*
   Since the TIFF DateTime string has no timezone information,
   assume that the camera's clock was set to Universal Time.
 */
void CLASS get_timestamp (int reversed)
{
  struct tm t;
  char str[20];
  int i;

  str[19] = 0;
  if (reversed)
    for (i=19; i--; ) str[i] = fgetc(ifp);
  else
    fread (str, 19, 1, ifp);
  memset (&t, 0, sizeof t);
  if (sscanf (str, "%d:%d:%d %d:%d:%d", &t.tm_year, &t.tm_mon,
	&t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec) != 6)
    return;
  t.tm_year -= 1900;
  t.tm_mon -= 1;
  t.tm_isdst = -1;
  if (mktime(&t) > 0)
    timestamp = mktime(&t);
}

void CLASS parse_exif (int base)
{
  unsigned kodak, entries, tag, type, len, save, c;
  double expo;

  kodak = !strncmp(make,"EASTMAN",7) && tiff_nifds < 3;
  entries = get2();
  while (entries--) {
    tiff_get (base, &tag, &type, &len, &save);
    switch (tag) {
      case 33434:  shutter = getreal(type);		break;
      case 33437:  aperture = getreal(type);		break;
      case 34855:  iso_speed = get2();			break;
      case 36867:
      case 36868:  get_timestamp(0);			break;
      case 37377:  if ((expo = -getreal(type)) < 128)
		     shutter = pow (2, expo);		break;
      case 37378:  aperture = pow (2, getreal(type)/2);	break;
      case 37386:  focal_len = getreal(type);		break;
      case 37500:  parse_makernote (base, 0);		break;
      case 40962:  if (kodak) raw_width  = get4();	break;
      case 40963:  if (kodak) raw_height = get4();	break;
      case 41730:
	if (get4() == 0x20002)
	  for (exif_cfa=c=0; c < 8; c+=2)
	    exif_cfa |= fgetc(ifp) * 0x01010101 << c;
    }
    fseek (ifp, save, SEEK_SET);
  }
}

void CLASS parse_gps (int base)
{
  unsigned entries, tag, type, len, save, c;

  entries = get2();
  while (entries--) {
    tiff_get (base, &tag, &type, &len, &save);
    switch (tag) {
      case 1: case 3: case 5:
	gpsdata[29+tag/2] = getc(ifp);			break;
      case 2: case 4: case 7:
	FORC(6) gpsdata[tag/3*6+c] = get4();		break;
      case 6:
	FORC(2) gpsdata[18+c] = get4();			break;
      case 18: case 29:
	fgets ((char *) (gpsdata+14+tag/3), MIN(len,12), ifp);
    }
    fseek (ifp, save, SEEK_SET);
  }
}

void CLASS romm_coeff (float romm_cam[3][3])
{
  static const float rgb_romm[3][3] =	/* ROMM == Kodak ProPhoto */
  { {  2.034193, -0.727420, -0.306766 },
    { -0.228811,  1.231729, -0.002922 },
    { -0.008565, -0.153273,  1.161839 } };
  int i, j, k;

  for (i=0; i < 3; i++)
    for (j=0; j < 3; j++)
      for (cmatrix[i][j] = k=0; k < 3; k++)
	cmatrix[i][j] += rgb_romm[i][k] * romm_cam[k][j];
}

void CLASS parse_mos (int offset)
{
  char data[40];
  int skip, from, i, c, neut[4], planes=0, frot=0;
  static const char *mod[] =
  { "","DCB2","Volare","Cantare","CMost","Valeo 6","Valeo 11","Valeo 22",
    "Valeo 11p","Valeo 17","","Aptus 17","Aptus 22","Aptus 75","Aptus 65",
    "Aptus 54S","Aptus 65S","Aptus 75S","AFi 5","AFi 6","AFi 7",
    "","","","","","","","","","","","","","","","","","AFi-II 12" };
  float romm_cam[3][3];

  fseek (ifp, offset, SEEK_SET);
  while (1) {
    if (get4() != 0x504b5453) break;
    get4();
    fread (data, 1, 40, ifp);
    skip = get4();
    from = ftell(ifp);
    if (!strcmp(data,"JPEG_preview_data")) {
      thumb_offset = from;
      thumb_length = skip;
    }
    if (!strcmp(data,"icc_camera_profile")) {
      profile_offset = from;
      profile_length = skip;
    }
    if (!strcmp(data,"ShootObj_back_type")) {
      fscanf (ifp, "%d", &i);
      if ((unsigned) i < sizeof mod / sizeof (*mod))
	strcpy (model, mod[i]);
    }
    if (!strcmp(data,"icc_camera_to_tone_matrix")) {
      for (i=0; i < 9; i++)
	romm_cam[0][i] = int_to_float(get4());
      romm_coeff (romm_cam);
    }
    if (!strcmp(data,"CaptProf_color_matrix")) {
      for (i=0; i < 9; i++)
	fscanf (ifp, "%f", &romm_cam[0][i]);
      romm_coeff (romm_cam);
    }
    if (!strcmp(data,"CaptProf_number_of_planes"))
      fscanf (ifp, "%d", &planes);
    if (!strcmp(data,"CaptProf_raw_data_rotation"))
      fscanf (ifp, "%d", &flip);
    if (!strcmp(data,"CaptProf_mosaic_pattern"))
      FORC4 {
	fscanf (ifp, "%d", &i);
	if (i == 1) frot = c ^ (c >> 1);
      }
    if (!strcmp(data,"ImgProf_rotation_angle")) {
      fscanf (ifp, "%d", &i);
      flip = i - flip;
    }
    if (!strcmp(data,"NeutObj_neutrals") && !cam_mul[0]) {
      FORC4 fscanf (ifp, "%d", neut+c);
      FORC3 cam_mul[c] = (float) neut[0] / neut[c+1];
    }
    if (!strcmp(data,"Rows_data"))
      load_flags = get4();
    parse_mos (from);
    fseek (ifp, skip+from, SEEK_SET);
  }
  if (planes)
    filters = (planes == 1) * 0x01010101 *
	(uchar) "\x94\x61\x16\x49"[(flip/90 + frot) & 3];
}

void CLASS linear_table (unsigned len)
{
  int i;
  if (len > 0x1000) len = 0x1000;
  read_shorts (curve, len);
  for (i=len; i < 0x1000; i++)
    curve[i] = curve[i-1];
  maximum = curve[0xfff];
}

void CLASS parse_kodak_ifd (int base)
{
  unsigned entries, tag, type, len, save;
  int i, c, wbi=-2, wbtemp=6500;
  float mul[3]={1,1,1}, num;
  static const int wbtag[] = { 64037,64040,64039,64041,-1,-1,64042 };

  entries = get2();
  if (entries > 1024) return;
  while (entries--) {
    tiff_get (base, &tag, &type, &len, &save);
    if (tag == 1020) wbi = getint(type);
    if (tag == 1021 && len == 72) {		/* WB set in software */
      fseek (ifp, 40, SEEK_CUR);
      FORC3 cam_mul[c] = 2048.0 / get2();
      wbi = -2;
    }
    if (tag == 2118) wbtemp = getint(type);
    if (tag == 2130 + wbi)
      FORC3 mul[c] = getreal(type);
    if (tag == 2140 + wbi && wbi >= 0)
      FORC3 {
	for (num=i=0; i < 4; i++)
	  num += getreal(type) * pow (wbtemp/100.0, i);
	cam_mul[c] = 2048 / (num * mul[c]);
      }
    if (tag == 2317) linear_table (len);
    if (tag == 6020) iso_speed = getint(type);
    if (tag == 64013) wbi = fgetc(ifp);
    if ((unsigned) wbi < 7 && tag == wbtag[wbi])
      FORC3 cam_mul[c] = get4();
    if (tag == 64019) width = getint(type);
    if (tag == 64020) height = (getint(type)+1) & -2;
    fseek (ifp, save, SEEK_SET);
  }
}
#line 4709 "dcraw/dcraw.c"
int CLASS parse_tiff_ifd (int base)
{
  unsigned entries, tag, type, len, plen=16, save;
  int ifd, use_cm=0, cfa, i, j, c, ima_len=0;
  int blrr=1, blrc=1, dblack[] = { 0,0,0,0 };
  char software[64], *cbuf, *cp;
  uchar cfa_pat[16], cfa_pc[] = { 0,1,2,3 }, tab[256];
  double cc[4][4], cm[4][3], cam_xyz[4][3], num;
  double ab[]={ 1,1,1,1 }, asn[] = { 0,0,0,0 }, xyz[] = { 1,1,1 };
  unsigned sony_curve[] = { 0,0,0,0,0,4095 };
  unsigned *buf, sony_offset=0, sony_length=0, sony_key=0;
  struct jhead jh;
#ifndef LIBRAW_LIBRARY_BUILD
  FILE *sfp;
#endif

  if (tiff_nifds >= sizeof tiff_ifd / sizeof tiff_ifd[0])
    return 1;
  ifd = tiff_nifds++;
  for (j=0; j < 4; j++)
    for (i=0; i < 4; i++)
      cc[j][i] = i == j;
  entries = get2();
  if (entries > 512) return 1;
  while (entries--) {
    tiff_get (base, &tag, &type, &len, &save);
    switch (tag) {
      case 5:   width  = get2();  break;
      case 6:   height = get2();  break;
      case 7:   width += get2();  break;
      case 9:  filters = get2();  break;
      case 17: case 18:
	if (type == 3 && len == 1)
	  cam_mul[(tag-17)*2] = get2() / 256.0;
	break;
      case 23:
	if (type == 3) iso_speed = get2();
	break;
      case 36: case 37: case 38:
	cam_mul[tag-0x24] = get2();
	break;
      case 39:
	if (len < 50 || cam_mul[0]) break;
	fseek (ifp, 12, SEEK_CUR);
	FORC3 cam_mul[c] = get2();
	break;
      case 46:
	if (type != 7 || fgetc(ifp) != 0xff || fgetc(ifp) != 0xd8) break;
	thumb_offset = ftell(ifp) - 2;
	thumb_length = len;
	break;
      case 61440:			/* Fuji HS10 table */
	parse_tiff_ifd (base);
	break;
      case 2: case 256: case 61441:	/* ImageWidth */
	tiff_ifd[ifd].t_width = getint(type);
	break;
      case 3: case 257: case 61442:	/* ImageHeight */
	tiff_ifd[ifd].t_height = getint(type);
	break;
      case 258:				/* BitsPerSample */
      case 61443:
	tiff_ifd[ifd].samples = len & 7;
	tiff_ifd[ifd].bps = getint(type);
	break;
      case 61446:
	raw_height = 0;
	load_raw = &CLASS packed_load_raw;
	load_flags = get4() && (filters=0x16161616) ? 24:80;
	break;
      case 259:				/* Compression */
	tiff_ifd[ifd].comp = getint(type);
	break;
      case 262:				/* PhotometricInterpretation */
	tiff_ifd[ifd].phint = get2();
	break;
      case 270:				/* ImageDescription */
	fread (desc, 512, 1, ifp);
	break;
      case 271:				/* Make */
	fgets (make, 64, ifp);
	break;
      case 272:				/* Model */
	fgets (model, 64, ifp);
	break;
      case 280:				/* Panasonic RW2 offset */
	if (type != 4) break;
	load_raw = &CLASS panasonic_load_raw;
	load_flags = 0x2008;
      case 273:				/* StripOffset */
      case 513:				/* JpegIFOffset */
      case 61447:
	tiff_ifd[ifd].offset = get4()+base;
	if (!tiff_ifd[ifd].bps && tiff_ifd[ifd].offset > 0) {
	  fseek (ifp, tiff_ifd[ifd].offset, SEEK_SET);
	  if (ljpeg_start (&jh, 1)) {
	    tiff_ifd[ifd].comp    = 6;
	    tiff_ifd[ifd].t_width   = jh.wide;
	    tiff_ifd[ifd].t_height  = jh.high;
	    tiff_ifd[ifd].bps     = jh.bits;
	    tiff_ifd[ifd].samples = jh.clrs;
	    if (!(jh.sraw || (jh.clrs & 1)))
	      tiff_ifd[ifd].t_width *= jh.clrs;
	    i = order;
	    parse_tiff (tiff_ifd[ifd].offset + 12);
	    order = i;
	  }
	}
	break;
      case 274:				/* Orientation */
	tiff_ifd[ifd].t_flip = "50132467"[get2() & 7]-'0';
	break;
      case 277:				/* SamplesPerPixel */
	tiff_ifd[ifd].samples = getint(type) & 7;
	break;
      case 279:				/* StripByteCounts */
      case 514:
      case 61448:
	tiff_ifd[ifd].bytes = get4();
	break;
      case 61454:
	FORC3 cam_mul[(4-c) % 3] = getint(type);
	break;
      case 305:  case 11:		/* Software */
	fgets (software, 64, ifp);
	if (!strncmp(software,"Adobe",5) ||
	    !strncmp(software,"dcraw",5) ||
	    !strncmp(software,"UFRaw",5) ||
	    !strncmp(software,"Bibble",6) ||
	    !strncmp(software,"Nikon Scan",10) ||
	    !strcmp (software,"Digital Photo Professional"))
	  is_raw = 0;
	break;
      case 306:				/* DateTime */
	get_timestamp(0);
	break;
      case 315:				/* Artist */
	fread (artist, 64, 1, ifp);
	break;
      case 322:				/* TileWidth */
	tiff_ifd[ifd].t_tile_width = getint(type);
	break;
      case 323:				/* TileLength */
	tiff_ifd[ifd].t_tile_length = getint(type);
	break;
      case 324:				/* TileOffsets */
	tiff_ifd[ifd].offset = len > 1 ? ftell(ifp) : get4();
	if (len == 4) {
	  load_raw = &CLASS sinar_4shot_load_raw;
	  is_raw = 5;
	}
	break;
#ifdef LIBRAW_LIBRARY_BUILD
      case 325:				/* TileByteCount */
          tiff_ifd[ifd].tile_maxbytes = 0;
          for(int jj=0;jj<len;jj++)
              {
                  int s = get4();
                  if(s > tiff_ifd[ifd].tile_maxbytes) tiff_ifd[ifd].tile_maxbytes=s;
              }
	break;
#endif
      case 330:				/* SubIFDs */
	if (!strcmp(model,"DSLR-A100") && tiff_ifd[ifd].t_width == 3872) {
	  load_raw = &CLASS sony_arw_load_raw;
	  data_offset = get4()+base;
	  ifd++;  break;
	}
	while (len--) {
	  i = ftell(ifp);
	  fseek (ifp, get4()+base, SEEK_SET);
	  if (parse_tiff_ifd (base)) break;
	  fseek (ifp, i+4, SEEK_SET);
	}
	break;
      case 400:
	strcpy (make, "Sarnoff");
	maximum = 0xfff;
	break;
      case 28688:
	FORC4 sony_curve[c+1] = get2() >> 2 & 0xfff;
	for (i=0; i < 5; i++)
	  for (j = sony_curve[i]+1; j <= sony_curve[i+1]; j++)
	    curve[j] = curve[j-1] + (1 << i);
	break;
      case 29184: sony_offset = get4();  break;
      case 29185: sony_length = get4();  break;
      case 29217: sony_key    = get4();  break;
      case 29264:
	parse_minolta (ftell(ifp));
	raw_width = 0;
	break;
      case 29443:
	FORC4 cam_mul[c ^ (c < 2)] = get2();
	break;
      case 29459:
	FORC4 cam_mul[c] = get2();
	i = (cam_mul[1] == 1024 && cam_mul[2] == 1024) << 1;
	SWAP (cam_mul[i],cam_mul[i+1])
	break;
      case 33405:			/* Model2 */
	fgets (model2, 64, ifp);
	break;
      case 33422:			/* CFAPattern */
      case 64777:			/* Kodak P-series */
	if ((plen=len) > 16) plen = 16;
	fread (cfa_pat, 1, plen, ifp);
	for (colors=cfa=i=0; i < plen; i++) {
	  colors += !(cfa & (1 << cfa_pat[i]));
	  cfa |= 1 << cfa_pat[i];
	}
	if (cfa == 070) memcpy (cfa_pc,"\003\004\005",3);	/* CMY */
	if (cfa == 072) memcpy (cfa_pc,"\005\003\004\001",4);	/* GMCY */
	goto guess_cfa_pc;
      case 33424:
      case 65024:
	fseek (ifp, get4()+base, SEEK_SET);
	parse_kodak_ifd (base);
	break;
      case 33434:			/* ExposureTime */
	shutter = getreal(type);
	break;
      case 33437:			/* FNumber */
	aperture = getreal(type);
	break;
      case 34306:			/* Leaf white balance */
	FORC4 cam_mul[c ^ 1] = 4096.0 / get2();
	break;
      case 34307:			/* Leaf CatchLight color matrix */
	fread (software, 1, 7, ifp);
	if (strncmp(software,"MATRIX",6)) break;
	colors = 4;
	for (raw_color = i=0; i < 3; i++) {
	  FORC4 fscanf (ifp, "%f", &rgb_cam[i][c^1]);
	  if (!use_camera_wb) continue;
	  num = 0;
	  FORC4 num += rgb_cam[i][c];
	  FORC4 rgb_cam[i][c] /= num;
	}
	break;
      case 34310:			/* Leaf metadata */
	parse_mos (ftell(ifp));
      case 34303:
	strcpy (make, "Leaf");
	break;
      case 34665:			/* EXIF tag */
	fseek (ifp, get4()+base, SEEK_SET);
	parse_exif (base);
	break;
      case 34853:			/* GPSInfo tag */
	fseek (ifp, get4()+base, SEEK_SET);
	parse_gps (base);
	break;
      case 34675:			/* InterColorProfile */
      case 50831:			/* AsShotICCProfile */
	profile_offset = ftell(ifp);
	profile_length = len;
	break;
      case 37122:			/* CompressedBitsPerPixel */
	kodak_cbpp = get4();
	break;
      case 37386:			/* FocalLength */
	focal_len = getreal(type);
	break;
      case 37393:			/* ImageNumber */
	shot_order = getint(type);
	break;
      case 37400:			/* old Kodak KDC tag */
	for (raw_color = i=0; i < 3; i++) {
	  getreal(type);
	  FORC3 rgb_cam[i][c] = getreal(type);
	}
	break;
      case 46275:			/* Imacon tags */
	strcpy (make, "Imacon");
	data_offset = ftell(ifp);
	ima_len = len;
	break;
      case 46279:
	if (!ima_len) break;
	fseek (ifp, 38, SEEK_CUR);
      case 46274:
	fseek (ifp, 40, SEEK_CUR);
	raw_width  = get4();
	raw_height = get4();
	left_margin = get4() & 7;
	width = raw_width - left_margin - (get4() & 7);
	top_margin = get4() & 7;
	height = raw_height - top_margin - (get4() & 7);
	if (raw_width == 7262) {
	  height = 5444;
	  width  = 7244;
	  left_margin = 7;
	}
	fseek (ifp, 52, SEEK_CUR);
	FORC3 cam_mul[c] = getreal(11);
	fseek (ifp, 114, SEEK_CUR);
	flip = (get2() >> 7) * 90;
	if (width * height * 6 == ima_len) {
	  if (flip % 180 == 90) SWAP(width,height);
	  raw_width = width;
	  raw_height = height;
	  left_margin = top_margin = filters = flip = 0;
	}
	sprintf (model, "Ixpress %d-Mp", height*width/1000000);
	load_raw = &CLASS imacon_full_load_raw;
	if (filters) {
	  if (left_margin & 1) filters = 0x61616161;
	  load_raw = &CLASS unpacked_load_raw;
	}
	maximum = 0xffff;
	break;
      case 50454:			/* Sinar tag */
      case 50455:
	if (!(cbuf = (char *) malloc(len))) break;
	fread (cbuf, 1, len, ifp);
	for (cp = cbuf-1; cp && cp < cbuf+len; cp = strchr(cp,'\n'))
	  if (!strncmp (++cp,"Neutral ",8))
	    sscanf (cp+8, "%f %f %f", cam_mul, cam_mul+1, cam_mul+2);
	free (cbuf);
	break;
      case 50458:
	if (!make[0]) strcpy (make, "Hasselblad");
	break;
      case 50459:			/* Hasselblad tag */
	i = order;
	j = ftell(ifp);
	c = tiff_nifds;
	order = get2();
	fseek (ifp, j+(get2(),get4()), SEEK_SET);
	parse_tiff_ifd (j);
	maximum = 0xffff;
	tiff_nifds = c;
	order = i;
	break;
      case 50706:			/* DNGVersion */
	FORC4 dng_version = (dng_version << 8) + fgetc(ifp);
	if (!make[0]) strcpy (make, "DNG");
	is_raw = 1;
	break;
      case 50710:			/* CFAPlaneColor */
	if (len > 4) len = 4;
	colors = len;
	fread (cfa_pc, 1, colors, ifp);
guess_cfa_pc:
	FORCC tab[cfa_pc[c]] = c;
	cdesc[c] = 0;
	for (i=16; i--; )
	  filters = filters << 2 | tab[cfa_pat[i % plen]];
	break;
      case 50711:			/* CFALayout */
	if (get2() == 2) {
	  fuji_width = 1;
	  filters = 0x49494949;
	}
	break;
      case 291:
      case 50712:			/* LinearizationTable */
	linear_table (len);
	break;
      case 50713:			/* BlackLevelRepeatDim */
	blrr = get2();
	blrc = get2();
	break;
      case 61450:
	blrr = blrc = 2;
      case 50714:			/* BlackLevel */
	black = getreal(type);
	if (!filters || !~filters) break;
	dblack[0] = black;
	dblack[1] = (blrc == 2) ? getreal(type):dblack[0];
	dblack[2] = (blrr == 2) ? getreal(type):dblack[0];
	dblack[3] = (blrc == 2 && blrr == 2) ? getreal(type):dblack[1];
	if (colors == 3)
	  filters |= ((filters >> 2 & 0x22222222) |
		      (filters << 2 & 0x88888888)) & filters << 1;
	FORC4 cblack[filters >> (c << 1) & 3] = dblack[c];
	black = 0;
	break;
      case 50715:			/* BlackLevelDeltaH */
      case 50716:			/* BlackLevelDeltaV */
	for (num=i=0; i < len; i++)
	  num += getreal(type);
	black += num/len + 0.5;
	break;
      case 50717:			/* WhiteLevel */
	maximum = getint(type);
	break;
      case 50718:			/* DefaultScale */
	pixel_aspect  = getreal(type);
	pixel_aspect /= getreal(type);
	break;
      case 50721:			/* ColorMatrix1 */
      case 50722:			/* ColorMatrix2 */
	FORCC for (j=0; j < 3; j++)
	  cm[c][j] = getreal(type);
	use_cm = 1;
	break;
      case 50723:			/* CameraCalibration1 */
      case 50724:			/* CameraCalibration2 */
	for (i=0; i < colors; i++)
	  FORCC cc[i][c] = getreal(type);
	break;
      case 50727:			/* AnalogBalance */
	FORCC ab[c] = getreal(type);
	break;
      case 50728:			/* AsShotNeutral */
	FORCC asn[c] = getreal(type);
	break;
      case 50729:			/* AsShotWhiteXY */
	xyz[0] = getreal(type);
	xyz[1] = getreal(type);
	xyz[2] = 1 - xyz[0] - xyz[1];
	FORC3 xyz[c] /= d65_white[c];
	break;
      case 50740:			/* DNGPrivateData */
	if (dng_version) break;
	parse_minolta (j = get4()+base);
	fseek (ifp, j, SEEK_SET);
	parse_tiff_ifd (base);
	break;
      case 50752:
	read_shorts (cr2_slice, 3);
	break;
      case 50829:			/* ActiveArea */
	top_margin = getint(type);
	left_margin = getint(type);
	height = getint(type) - top_margin;
	width = getint(type) - left_margin;
	break;
      case 50830:			/* MaskedAreas */
        for (i=0; i < len && i < 32; i++)
	  mask[0][i] = getint(type);
	black = 0;
	break;
      case 51009:			/* OpcodeList2 */
	meta_offset = ftell(ifp);
	break;
      case 64772:			/* Kodak P-series */
	if (len < 13) break;
	fseek (ifp, 16, SEEK_CUR);
	data_offset = get4();
	fseek (ifp, 28, SEEK_CUR);
	data_offset += get4();
	load_raw = &CLASS packed_load_raw;
	break;
      case 65026:
	if (type == 2) fgets (model2, 64, ifp);
    }
    fseek (ifp, save, SEEK_SET);
  }
  if (sony_length && (buf = (unsigned *) malloc(sony_length))) {
    fseek (ifp, sony_offset, SEEK_SET);
    fread (buf, sony_length, 1, ifp);
    sony_decrypt (buf, sony_length/4, 1, sony_key);
#ifndef LIBRAW_LIBRARY_BUILD
    sfp = ifp;
    if ((ifp = tmpfile())) {
      fwrite (buf, sony_length, 1, ifp);
      fseek (ifp, 0, SEEK_SET);
      parse_tiff_ifd (-sony_offset);
      fclose (ifp);
    }
    ifp = sfp;
#else
    if( !ifp->tempbuffer_open(buf,sony_length))
        {
            parse_tiff_ifd(-sony_offset);
            ifp->tempbuffer_close();
        }
#endif
    free (buf);
  }
  for (i=0; i < colors; i++)
    FORCC cc[i][c] *= ab[i];
  if (use_cm) {
    FORCC for (i=0; i < 3; i++)
      for (cam_xyz[c][i]=j=0; j < colors; j++)
	cam_xyz[c][i] += cc[c][j] * cm[j][i] * xyz[i];
    cam_xyz_coeff (cam_xyz);
  }
  if (asn[0]) {
    cam_mul[3] = 0;
    FORCC cam_mul[c] = 1 / asn[c];
  }
  if (!use_cm)
    FORCC pre_mul[c] /= cc[c][c];
  return 0;
}

int CLASS parse_tiff (int base)
{
  int doff;

  fseek (ifp, base, SEEK_SET);
  order = get2();
  if (order != 0x4949 && order != 0x4d4d) return 0;
  get2();
  while ((doff = get4())) {
    fseek (ifp, doff+base, SEEK_SET);
    if (parse_tiff_ifd (base)) break;
  }
  return 1;
}

void CLASS apply_tiff()
{
  int max_samp=0, raw=-1, thm=-1, i;
  struct jhead jh;

  thumb_misc = 16;
  if (thumb_offset) {
    fseek (ifp, thumb_offset, SEEK_SET);
    if (ljpeg_start (&jh, 1)) {
      thumb_misc   = jh.bits;
      thumb_width  = jh.wide;
      thumb_height = jh.high;
    }
  }
  for (i=0; i < tiff_nifds; i++) {
    if (max_samp < tiff_ifd[i].samples)
	max_samp = tiff_ifd[i].samples;
    if (max_samp > 3) max_samp = 3;
    if ((tiff_ifd[i].comp != 6 || tiff_ifd[i].samples != 3) &&
	(tiff_ifd[i].t_width | tiff_ifd[i].t_height) < 0x10000 &&
	tiff_ifd[i].t_width*tiff_ifd[i].t_height > raw_width*raw_height) {
      raw_width     = tiff_ifd[i].t_width;
      raw_height    = tiff_ifd[i].t_height;
      tiff_bps      = tiff_ifd[i].bps;
      tiff_compress = tiff_ifd[i].comp;
      data_offset   = tiff_ifd[i].offset;
      tiff_flip     = tiff_ifd[i].t_flip;
      tiff_samples  = tiff_ifd[i].samples;
      tile_width    = tiff_ifd[i].t_tile_width;
      tile_length   = tiff_ifd[i].t_tile_length;
#ifdef LIBRAW_LIBRARY_BUILD
      data_size     = tile_length < INT_MAX ? tiff_ifd[i].tile_maxbytes: tiff_ifd[i].bytes;
#endif
      raw = i;
    }
  }
  if (!tile_width ) tile_width  = INT_MAX;
  if (!tile_length) tile_length = INT_MAX;
  for (i=tiff_nifds; i--; )
    if (tiff_ifd[i].t_flip) tiff_flip = tiff_ifd[i].t_flip;
  if (raw >= 0 && !load_raw)
    switch (tiff_compress) {
      case 32767:
	if (tiff_ifd[raw].bytes == raw_width*raw_height) {
	  tiff_bps = 12;
	  load_raw = &CLASS sony_arw2_load_raw;			break;
	}
	if (tiff_ifd[raw].bytes*8 != raw_width*raw_height*tiff_bps) {
	  raw_height += 8;
	  load_raw = &CLASS sony_arw_load_raw;			break;
	}
	load_flags = 79;
      case 32769:
	load_flags++;
      case 32770:
      case 32773: goto slr;
      case 0:  case 1:
	if (tiff_ifd[raw].bytes*5 == raw_width*raw_height*8) {
	  load_flags = 81;
	  tiff_bps = 12;
	} slr:
	switch (tiff_bps) {
	  case  8: load_raw = &CLASS eight_bit_load_raw;	break;
	  case 12: if (tiff_ifd[raw].phint == 2)
		     load_flags = 6;
		   load_raw = &CLASS packed_load_raw;		break;
	  case 14: load_flags = 0;
	  case 16: load_raw = &CLASS unpacked_load_raw;		break;
	}
	break;
      case 6:  case 7:  case 99:
	load_raw = &CLASS lossless_jpeg_load_raw;		break;
      case 262:
	load_raw = &CLASS kodak_262_load_raw;			break;
      case 34713:
	load_raw = &CLASS nikon_load_raw;			break;
      case 34892:
	load_raw = &CLASS lossy_dng_load_raw;		break;
      case 65535:
	load_raw = &CLASS pentax_load_raw;			break;
      case 65000:
	switch (tiff_ifd[raw].phint) {
	  case 2: load_raw = &CLASS kodak_rgb_load_raw;   filters = 0;  break;
	  case 6: load_raw = &CLASS kodak_ycbcr_load_raw; filters = 0;  break;
	  case 32803: load_raw = &CLASS kodak_65000_load_raw;
	}
      case 32867: break;
      default: is_raw = 0;
    }
  if (!dng_version)
    if ( (tiff_samples == 3 && tiff_ifd[raw].bytes &&
	  tiff_bps != 14 && tiff_bps != 2048 && tiff_compress != 32770)
      || (tiff_bps == 8 && !strstr(make,"KODAK") && !strstr(make,"Kodak") &&
	  !strstr(model2,"DEBUG RAW")))
      is_raw = 0;
  for (i=0; i < tiff_nifds; i++)
    if (i != raw && tiff_ifd[i].samples == max_samp &&
	tiff_ifd[i].t_width * tiff_ifd[i].t_height / SQR(tiff_ifd[i].bps+1) >
	      thumb_width *       thumb_height / SQR(thumb_misc+1)
	&& tiff_ifd[i].comp != 34892) {
      thumb_width  = tiff_ifd[i].t_width;
      thumb_height = tiff_ifd[i].t_height;
      thumb_offset = tiff_ifd[i].offset;
      thumb_length = tiff_ifd[i].bytes;
      thumb_misc   = tiff_ifd[i].bps;
      thm = i;
    }
  if (thm >= 0) {
    thumb_misc |= tiff_ifd[thm].samples << 5;
    switch (tiff_ifd[thm].comp) {
      case 0:
	write_thumb = &CLASS layer_thumb;
	break;
      case 1:
	if (tiff_ifd[thm].bps <= 8)
	  write_thumb = &CLASS ppm_thumb;
	else if (!strcmp(make,"Imacon"))
	  write_thumb = &CLASS ppm16_thumb;
	else
	  thumb_load_raw = &CLASS kodak_thumb_load_raw;
	break;
      case 65000:
	thumb_load_raw = tiff_ifd[thm].phint == 6 ?
		&CLASS kodak_ycbcr_load_raw : &CLASS kodak_rgb_load_raw;
    }
  }
}

void CLASS parse_minolta (int base)
{
  int save, tag, len, offset, high=0, wide=0, i, c;
  short sorder=order;

  fseek (ifp, base, SEEK_SET);
  if (fgetc(ifp) || fgetc(ifp)-'M' || fgetc(ifp)-'R') return;
  order = fgetc(ifp) * 0x101;
  offset = base + get4() + 8;
  while ((save=ftell(ifp)) < offset) {
    for (tag=i=0; i < 4; i++)
      tag = tag << 8 | fgetc(ifp);
    len = get4();
    switch (tag) {
      case 0x505244:				/* PRD */
	fseek (ifp, 8, SEEK_CUR);
	high = get2();
	wide = get2();
	break;
      case 0x574247:				/* WBG */
	get4();
	i = strcmp(model,"DiMAGE A200") ? 0:3;
	FORC4 cam_mul[c ^ (c >> 1) ^ i] = get2();
	break;
      case 0x545457:				/* TTW */
	parse_tiff (ftell(ifp));
	data_offset = offset;
    }
    fseek (ifp, save+len+8, SEEK_SET);
  }
  raw_height = high;
  raw_width  = wide;
  order = sorder;
}

/*
   Many cameras have a "debug mode" that writes JPEG and raw
   at the same time.  The raw file has no header, so try to
   to open the matching JPEG file and read its metadata.
 */
void CLASS parse_external_jpeg()
{
  const char *file, *ext;
  char *jname, *jfile, *jext;
#ifndef LIBRAW_LIBRARY_BUILD
  FILE *save=ifp;
#else
#ifdef WIN32
  if(ifp->wfname())
  {
	  std::wstring rawfile(ifp->wfname());
	  rawfile.replace(rawfile.length()-3,3,L"JPG");
	  if(!ifp->subfile_open(rawfile.c_str()))
	  {
		  parse_tiff (12);
		  thumb_offset = 0;
		  is_raw = 1;
		  ifp->subfile_close();
	  }
	  else
		  imgdata.process_warnings |= LIBRAW_WARN_NO_METADATA ;
	 return;
  }
#endif
  if(!ifp->fname())
      {
          imgdata.process_warnings |= LIBRAW_WARN_NO_METADATA ;
          return;
      }
#endif

  ext  = strrchr (ifname, '.');
  file = strrchr (ifname, '/');
  if (!file) file = strrchr (ifname, '\\');
#ifndef LIBRAW_LIBRARY_BUILD
  if (!file) file = ifname-1;
#else
  if (!file) file = (char*)ifname-1;
#endif
  file++;
  if (!ext || strlen(ext) != 4 || ext-file != 8) return;
  jname = (char *) malloc (strlen(ifname) + 1);
  merror (jname, "parse_external_jpeg()");
  strcpy (jname, ifname);
  jfile = file - ifname + jname;
  jext  = ext  - ifname + jname;
  if (strcasecmp (ext, ".jpg")) {
    strcpy (jext, isupper(ext[1]) ? ".JPG":".jpg");
    if (isdigit(*file)) {
      memcpy (jfile, file+4, 4);
      memcpy (jfile+4, file, 4);
    }
  } else
    while (isdigit(*--jext)) {
      if (*jext != '9') {
	(*jext)++;
	break;
      }
      *jext = '0';
    }
#ifndef LIBRAW_LIBRARY_BUILD
  if (strcmp (jname, ifname)) {
    if ((ifp = fopen (jname, "rb"))) {
#ifdef DCRAW_VERBOSE
      if (verbose)
	fprintf (stderr,_("Reading metadata from %s ...\n"), jname);
#endif
      parse_tiff (12);
      thumb_offset = 0;
      is_raw = 1;
      fclose (ifp);
    }
  }
#else
  if (strcmp (jname, ifname)) 
      {
          if(!ifp->subfile_open(jname))
              {
                  parse_tiff (12);
                  thumb_offset = 0;
                  is_raw = 1;
                  ifp->subfile_close();
              }
          else
              imgdata.process_warnings |= LIBRAW_WARN_NO_METADATA ;
      }
#endif
  if (!timestamp)
    fprintf (stderr,_("Failed to read metadata from %s\n"), jname);
  free (jname);
#ifndef LIBRAW_LIBRARY_BUILD
  ifp = save;
#endif
}
