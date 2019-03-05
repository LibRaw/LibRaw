#ifndef IGNOREALL
/*
   dcraw.c -- Dave Coffin's raw photo decoder
   Copyright 1997-2015 by Dave Coffin, dcoffin a cybercom o net

   This is a command-line ANSI C program to convert raw photos from
   any digital camera on any computer running any operating system.

   No license is required to download and use dcraw.c.  However,
   to lawfully redistribute dcraw, you must either (a) offer, at
   no extra charge, full source code* for all executable files
   containing RESTRICTED functions, (b) distribute this code under
   the GPL Version 2 or later, (c) remove all RESTRICTED functions,
   re-implement them, or copy them from an earlier, unrestricted
   Revision of dcraw.c, or (d) purchase a license from the author.

   The functions that process Foveon images have been RESTRICTED
   since Revision 1.237.  All other code remains free for all uses.

   *If you have not modified dcraw.c in any way, a link to my
   homepage qualifies as "full source code".

   $Revision: 1.476 $
   $Date: 2015/05/25 02:29:14 $
 */
/*@out DEFINES
#ifndef USE_JPEG
#define NO_JPEG
#endif
#ifndef USE_JASPER
#define NO_JASPER
#endif
@end DEFINES */

#define NO_LCMS
#define DCRAW_VERBOSE
//@out DEFINES
#define DCRAW_VERSION "9.26"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define _USE_MATH_DEFINES
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
//@end DEFINES

#if defined(DJGPP) || defined(__MINGW32__)
#define fseeko fseek
#define ftello ftell
#else
#define fgetc getc_unlocked
#endif
//@out DEFINES
#ifdef __CYGWIN__
#include <io.h>
#endif
#if defined WIN32 || defined(__MINGW32__)
#include <sys/utime.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#define snprintf _snprintf
#define strcasecmp stricmp
#define strncasecmp strnicmp
//@end DEFINES
typedef __int64 INT64;
typedef unsigned __int64 UINT64;
//@out DEFINES
#else
#include <unistd.h>
#include <utime.h>
#include <netinet/in.h>
typedef long long INT64;
typedef unsigned long long UINT64;
#endif

#ifdef NODEPS
#define NO_JASPER
#define NO_JPEG
#define NO_LCMS
#endif
#ifndef NO_JASPER
#include <jasper/jasper.h> /* Decode Red camera movies */
#endif
#ifndef NO_JPEG
#include <jpeglib.h> /* Decode compressed Kodak DC120 photos */
#endif               /* and Adobe Lossy DNGs */
#ifndef NO_LCMS
#ifdef USE_LCMS
#include <lcms.h> /* Support color profiles */
#else
#include <lcms2.h> /* Support color profiles */
#endif
#endif
#ifdef LOCALEDIR
#include <libintl.h>
#define _(String) gettext(String)
#else
#define _(String) (String)
#endif

#ifdef LJPEG_DECODE
#error Please compile dcraw.c by itself.
#error Do not link it with ljpeg_decode.
#endif

#ifndef LONG_BIT
#define LONG_BIT (8 * sizeof(long))
#endif
//@end DEFINES

#if !defined(uchar)
#define uchar unsigned char
#endif
#if !defined(ushort)
#define ushort unsigned short
#endif

/*
   All global variables are defined here, and all functions that
   access them are prefixed with "CLASS".  Note that a thread-safe
   C++ class cannot have non-const static local variables.
 */
FILE *ifp, *ofp;
short order;
const char *ifname;
char *meta_data, xtrans[6][6], xtrans_abs[6][6];
char cdesc[5], desc[512], make[64], model[64], model2[64], artist[64], software[64];
float flash_used, canon_ev, iso_speed, shutter, aperture, focal_len;
time_t timestamp;
off_t strip_offset, data_offset;
off_t thumb_offset, meta_offset, profile_offset;
unsigned shot_order, kodak_cbpp, exif_cfa, unique_id;
unsigned thumb_length, meta_length, profile_length;
unsigned thumb_misc, *oprof, fuji_layout, shot_select = 0, multi_out = 0;
unsigned tiff_nifds, tiff_samples, tiff_bps, tiff_compress;
unsigned black, maximum, mix_green, raw_color, zero_is_bad;
unsigned zero_after_ff, is_raw, dng_version, is_foveon, data_error;
unsigned tile_width, tile_length, gpsdata[32], load_flags;
unsigned flip, tiff_flip, filters, colors;
ushort raw_height, raw_width, height, width, top_margin, left_margin;
ushort shrink, iheight, iwidth, fuji_width, thumb_width, thumb_height;
ushort *raw_image, (*image)[4], cblack[4102];
ushort white[8][8], curve[0x10000], cr2_slice[3], sraw_mul[4];
double pixel_aspect, aber[4] = {1, 1, 1, 1}, gamm[6] = {0.45, 4.5, 0, 0, 0, 0};
float bright = 1, user_mul[4] = {0, 0, 0, 0}, threshold = 0;
int mask[8][4];
int half_size = 0, four_color_rgb = 0, document_mode = 0, highlight = 0;
int verbose = 0, use_auto_wb = 0, use_camera_wb = 0, use_camera_matrix = 1;
int output_color = 1, output_bps = 8, output_tiff = 0, med_passes = 0;
int no_auto_bright = 0;
unsigned greybox[4] = {0, 0, UINT_MAX, UINT_MAX};
float cam_mul[4], pre_mul[4], cmatrix[3][4], rgb_cam[3][4];
const double xyz_rgb[3][3] = {/* XYZ from RGB */
                              {0.412453, 0.357580, 0.180423},
                              {0.212671, 0.715160, 0.072169},
                              {0.019334, 0.119193, 0.950227}};
const float d65_white[3] = {0.950456, 1, 1.088754};
int histogram[4][0x2000];
void (*write_thumb)(), (*write_fun)();
void (*load_raw)(), (*thumb_load_raw)();
unsigned is_NikonTransfer = 0;
unsigned is_4K_RAFdata = 0; /* =1 for Fuji X-A3, X-A5, X-A10, X-A20, X-T100, XF10 */
unsigned is_PentaxRicohMakernotes = 0; /*  =1 for Ricoh software by Pentax, Camera DNG */
jmp_buf failure;

struct decode
{
  struct decode *branch[2];
  int leaf;
} first_decode[2048], *second_decode, *free_decode;

struct tiff_ifd
{
  int t_width, t_height, bps, comp, phint, offset, t_flip, samples, bytes;
  int t_tile_width, t_tile_length, sample_format, predictor;
  float t_shutter;
} tiff_ifd[10];

struct ph1
{
  int format, key_off, tag_21a;
  int t_black, split_col, black_col, split_row, black_row;
  float tag_210;
} ph1;

#define CLASS

//@out DEFINES
#define FORC(cnt) for (c = 0; c < cnt; c++)
#define FORC3 FORC(3)
#define FORC4 FORC(4)
#define FORCC for (c = 0; c < colors && c < 4; c++)

#define SQR(x) ((x) * (x))
#define ABS(x) (((int)(x) ^ ((int)(x) >> 31)) - ((int)(x) >> 31))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define LIM(x, min, max) MAX(min, MIN(x, max))
#define ULIM(x, y, z) ((y) < (z) ? LIM(x, y, z) : LIM(x, z, y))
#define CLIP(x) LIM((int)(x), 0, 65535)
#define CLIP15(x) LIM((int)(x), 0, 32767)
#define SWAP(a, b)                                                                                                     \
  {                                                                                                                    \
    a = a + b;                                                                                                         \
    b = a - b;                                                                                                         \
    a = a - b;                                                                                                         \
  }

#define my_swap(type, i, j)                                                                                            \
  {                                                                                                                    \
    type t = i;                                                                                                        \
    i = j;                                                                                                             \
    j = t;                                                                                                             \
  }

static float fMAX(float a, float b) { return MAX(a, b); }

/*
   In order to inline this calculation, I make the risky
   assumption that all filter patterns can be described
   by a repeating pattern of eight rows and two columns

   Do not use the FC or BAYER macros with the Leaf CatchLight,
   because its pattern is 16x16, not 2x8.

   Return values are either 0/1/2/3 = G/M/C/Y or 0/1/2/3 = R/G1/B/G2

        PowerShot 600	PowerShot A50	PowerShot Pro70	Pro90 & G1
        0xe1e4e1e4:	0x1b4e4b1e:	0x1e4b4e1b:	0xb4b4b4b4:

          0 1 2 3 4 5	  0 1 2 3 4 5	  0 1 2 3 4 5	  0 1 2 3 4 5
        0 G M G M G M	0 C Y C Y C Y	0 Y C Y C Y C	0 G M G M G M
        1 C Y C Y C Y	1 M G M G M G	1 M G M G M G	1 Y C Y C Y C
        2 M G M G M G	2 Y C Y C Y C	2 C Y C Y C Y
        3 C Y C Y C Y	3 G M G M G M	3 G M G M G M
                        4 C Y C Y C Y	4 Y C Y C Y C
        PowerShot A5	5 G M G M G M	5 G M G M G M
        0x1e4e1e4e:	6 Y C Y C Y C	6 C Y C Y C Y
                        7 M G M G M G	7 M G M G M G
          0 1 2 3 4 5
        0 C Y C Y C Y
        1 G M G M G M
        2 C Y C Y C Y
        3 M G M G M G

   All RGB cameras use one of these Bayer grids:

        0x16161616:	0x61616161:	0x49494949:	0x94949494:

          0 1 2 3 4 5	  0 1 2 3 4 5	  0 1 2 3 4 5	  0 1 2 3 4 5
        0 B G B G B G	0 G R G R G R	0 G B G B G B	0 R G R G R G
        1 G R G R G R	1 B G B G B G	1 R G R G R G	1 G B G B G B
        2 B G B G B G	2 G R G R G R	2 G B G B G B	2 R G R G R G
        3 G R G R G R	3 B G B G B G	3 R G R G R G	3 G B G B G B
 */

#define RAWINDEX(row, col) ((row)*raw_width + (col))
#define RAW(row, col) raw_image[(row)*raw_width + (col)]
//@end DEFINES

#define FC(row, col) (filters >> ((((row) << 1 & 14) + ((col)&1)) << 1) & 3)

//@out DEFINES
#define BAYER(row, col) image[((row) >> shrink) * iwidth + ((col) >> shrink)][FC(row, col)]

#define BAYER2(row, col) image[((row) >> shrink) * iwidth + ((col) >> shrink)][fcol(row, col)]
//@end DEFINES

/* @out COMMON

#include <math.h>
#define CLASS LibRaw::
#include "libraw/libraw_types.h"
#define LIBRAW_LIBRARY_BUILD
#define LIBRAW_IO_REDEFINED
#include "libraw/libraw.h"
#include "internal/defines.h"
#include "internal/var_defines.h"
@end COMMON */

//@out COMMON
int CLASS fcol(int row, int col)
{
  static const char filter[16][16] = {
      {2, 1, 1, 3, 2, 3, 2, 0, 3, 2, 3, 0, 1, 2, 1, 0}, {0, 3, 0, 2, 0, 1, 3, 1, 0, 1, 1, 2, 0, 3, 3, 2},
      {2, 3, 3, 2, 3, 1, 1, 3, 3, 1, 2, 1, 2, 0, 0, 3}, {0, 1, 0, 1, 0, 2, 0, 2, 2, 0, 3, 0, 1, 3, 2, 1},
      {3, 1, 1, 2, 0, 1, 0, 2, 1, 3, 1, 3, 0, 1, 3, 0}, {2, 0, 0, 3, 3, 2, 3, 1, 2, 0, 2, 0, 3, 2, 2, 1},
      {2, 3, 3, 1, 2, 1, 2, 1, 2, 1, 1, 2, 3, 0, 0, 1}, {1, 0, 0, 2, 3, 0, 0, 3, 0, 3, 0, 3, 2, 1, 2, 3},
      {2, 3, 3, 1, 1, 2, 1, 0, 3, 2, 3, 0, 2, 3, 1, 3}, {1, 0, 2, 0, 3, 0, 3, 2, 0, 1, 1, 2, 0, 1, 0, 2},
      {0, 1, 1, 3, 3, 2, 2, 1, 1, 3, 3, 0, 2, 1, 3, 2}, {2, 3, 2, 0, 0, 1, 3, 0, 2, 0, 1, 2, 3, 0, 1, 0},
      {1, 3, 1, 2, 3, 2, 3, 2, 0, 2, 0, 1, 1, 0, 3, 0}, {0, 2, 0, 3, 1, 0, 0, 1, 1, 3, 3, 2, 3, 2, 2, 1},
      {2, 1, 3, 2, 3, 1, 2, 1, 0, 3, 0, 2, 0, 2, 0, 2}, {0, 3, 1, 0, 0, 2, 0, 3, 2, 1, 3, 1, 1, 3, 1, 3}};

  if (filters == 1)
    return filter[(row + top_margin) & 15][(col + left_margin) & 15];
  if (filters == 9)
    return xtrans[(row + 6) % 6][(col + 6) % 6];
  return FC(row, col);
}

#if !defined(__FreeBSD__)
static size_t local_strnlen(const char *s, size_t n)
{
  const char *p = (const char *)memchr(s, 0, n);
  return (p ? p - s : n);
}
/* add OS X version check here ?? */
#define strnlen(a, b) local_strnlen(a, b)
#endif

#ifdef LIBRAW_LIBRARY_BUILD
static int Fuji_wb_list1[] = {LIBRAW_WBI_FineWeather, LIBRAW_WBI_Shade, LIBRAW_WBI_FL_D,
                              LIBRAW_WBI_FL_L,        LIBRAW_WBI_FL_W,  LIBRAW_WBI_Tungsten};
static int nFuji_wb_list1 = sizeof(Fuji_wb_list1) / sizeof(int);
static int FujiCCT_K[31] = {2500, 2550, 2650, 2700, 2800, 2850, 2950, 3000, 3100, 3200, 3300,
                            3400, 3600, 3700, 3800, 4000, 4200, 4300, 4500, 4800, 5000, 5300,
                            5600, 5900, 6300, 6700, 7100, 7700, 8300, 9100, 10000};
static int Fuji_wb_list2[] = {LIBRAW_WBI_Auto,  0,  LIBRAW_WBI_Custom,   6,  LIBRAW_WBI_FineWeather, 1,
                              LIBRAW_WBI_Shade, 8,  LIBRAW_WBI_FL_D,     10, LIBRAW_WBI_FL_L,        11,
                              LIBRAW_WBI_FL_W,  12, LIBRAW_WBI_Tungsten, 2,  LIBRAW_WBI_Underwater,  35,
                              LIBRAW_WBI_Ill_A, 82, LIBRAW_WBI_D65,      83};
static int nFuji_wb_list2 = sizeof(Fuji_wb_list2) / sizeof(int);

static int Oly_wb_list1[] = {LIBRAW_WBI_Shade,    LIBRAW_WBI_Cloudy, LIBRAW_WBI_FineWeather,
                             LIBRAW_WBI_Tungsten, LIBRAW_WBI_Sunset, LIBRAW_WBI_FL_D,
                             LIBRAW_WBI_FL_N,     LIBRAW_WBI_FL_W,   LIBRAW_WBI_FL_WW};

static int Oly_wb_list2[] = {LIBRAW_WBI_Auto,
                             0,
                             LIBRAW_WBI_Tungsten,
                             3000,
                             0x100,
                             3300,
                             0x100,
                             3600,
                             0x100,
                             3900,
                             LIBRAW_WBI_FL_W,
                             4000,
                             0x100,
                             4300,
                             LIBRAW_WBI_FL_D,
                             4500,
                             0x100,
                             4800,
                             LIBRAW_WBI_FineWeather,
                             5300,
                             LIBRAW_WBI_Cloudy,
                             6000,
                             LIBRAW_WBI_FL_N,
                             6600,
                             LIBRAW_WBI_Shade,
                             7500,
                             LIBRAW_WBI_Custom1,
                             0,
                             LIBRAW_WBI_Custom2,
                             0,
                             LIBRAW_WBI_Custom3,
                             0,
                             LIBRAW_WBI_Custom4,
                             0};

static int Pentax_wb_list1[] = {LIBRAW_WBI_Daylight, LIBRAW_WBI_Shade, LIBRAW_WBI_Cloudy, LIBRAW_WBI_Tungsten,
                                LIBRAW_WBI_FL_D,     LIBRAW_WBI_FL_N,  LIBRAW_WBI_FL_W,   LIBRAW_WBI_Flash};

static int Pentax_wb_list2[] = {LIBRAW_WBI_Daylight, LIBRAW_WBI_Shade, LIBRAW_WBI_Cloudy,
                                LIBRAW_WBI_Tungsten, LIBRAW_WBI_FL_D,  LIBRAW_WBI_FL_N,
                                LIBRAW_WBI_FL_W,     LIBRAW_WBI_Flash, LIBRAW_WBI_FL_L};
static int nPentax_wb_list2 = sizeof(Pentax_wb_list2) / sizeof(int);

static int stread(char *buf, size_t len, LibRaw_abstract_datastream *fp)
{
  if(len>0)
  {
    int r = fp->read(buf, len, 1);
    buf[len - 1] = 0;
    return r;
  }
  else
    return 0;
}
#define stmread(buf, maxlen, fp) stread(buf, MIN(maxlen, sizeof(buf)), fp)
#endif

#if !defined(__GLIBC__) && !defined(__FreeBSD__)
char *my_memmem(char *haystack, size_t haystacklen, char *needle, size_t needlelen)
{
  char *c;
  for (c = haystack; c <= haystack + haystacklen - needlelen; c++)
    if (!memcmp(c, needle, needlelen))
      return c;
  return 0;
}
#define memmem my_memmem
char *my_strcasestr(char *haystack, const char *needle)
{
  char *c;
  for (c = haystack; *c; c++)
    if (!strncasecmp(c, needle, strlen(needle)))
      return c;
  return 0;
}
#define strcasestr my_strcasestr
#endif

#define strbuflen(buf) strnlen(buf, sizeof(buf) - 1)

//@end COMMON

void CLASS merror(void *ptr, const char *where)
{
  if (ptr)
    return;
  fprintf(stderr, _("%s: Out of memory in %s\n"), ifname, where);
  longjmp(failure, 1);
}

void CLASS derror()
{
  if (!data_error)
  {
    fprintf(stderr, "%s: ", ifname);
    if (feof(ifp))
      fprintf(stderr, _("Unexpected end of file\n"));
    else
      fprintf(stderr, _("Corrupt data near 0x%llx\n"), (INT64)ftello(ifp));
  }
  data_error++;
}

//@out COMMON
ushort CLASS sget2(uchar *s)
{
  if (order == 0x4949) /* "II" means little-endian */
    return s[0] | s[1] << 8;
  else /* "MM" means big-endian */
    return s[0] << 8 | s[1];
}

// DNG was written by:
#define nonDNG 0
#define CameraDNG 1
#define AdobeDNG 2

// Makernote tag type:
#define is_0x927c 0 /* most cameras */
#define is_0xc634 2 /* Adobe DNG, Sony SR2, Pentax */

#ifdef LIBRAW_LIBRARY_BUILD
static int getwords(char *line, char *words[], int maxwords, int maxlen)
{
  line[maxlen - 1] = 0;
  char *p = line;
  int nwords = 0;

  while (1)
  {
    while (isspace(*p))
      p++;
    if (*p == '\0')
      return nwords;
    words[nwords++] = p;
    while (!isspace(*p) && *p != '\0')
      p++;
    if (*p == '\0')
      return nwords;
    *p++ = '\0';
    if (nwords >= maxwords)
      return nwords;
  }
}

static ushort saneSonyCameraInfo(uchar a, uchar b, uchar c, uchar d, uchar e, uchar f)
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
    "\x00\x01\x32\xb1\x0a\x0e\x87\x28\x02\xcc\xca\xad\x1b\xdc\x08\xed\x64\x86\xf0\x4f\x8c\x6c\xb8\xcb\x69\xc4\x2c\x03"
    "\x97\xb6\x93\x7c\x14\xf3\xe2\x3e\x30\x8e\xd7\x60\x1c\xa1\xab\x37\xec\x75\xbe\x23\x15\x6a\x59\x3f\xd0\xb9\x96\xb5"
    "\x50\x27\x88\xe3\x81\x94\xe0\xc0\x04\x5c\xc6\xe8\x5f\x4b\x70\x38\x9f\x82\x80\x51\x2b\xc5\x45\x49\x9b\x21\x52\x53"
    "\x54\x85\x0b\x5d\x61\xda\x7b\x55\x26\x24\x07\x6e\x36\x5b\x47\xb7\xd9\x4a\xa2\xdf\xbf\x12\x25\xbc\x1e\x7f\x56\xea"
    "\x10\xe6\xcf\x67\x4d\x3c\x91\x83\xe1\x31\xb3\x6f\xf4\x05\x8a\x46\xc8\x18\x76\x68\xbd\xac\x92\x2a\x13\xe9\x0f\xa3"
    "\x7a\xdb\x3d\xd4\xe7\x3a\x1a\x57\xaf\x20\x42\xb2\x9e\xc3\x8b\xf2\xd5\xd3\xa4\x7e\x1f\x98\x9c\xee\x74\xa5\xa6\xa7"
    "\xd8\x5e\xb0\xb4\x34\xce\xa8\x79\x77\x5a\xc1\x89\xae\x9a\x11\x33\x9d\xf5\x39\x19\x65\x78\x16\x71\xd2\xa9\x44\x63"
    "\x40\x29\xba\xa0\x8f\xe4\xd6\x3b\x84\x0d\xc2\x4e\x58\xdd\x99\x22\x6b\xc9\xbb\x17\x06\xe5\x7d\x66\x43\x62\xf6\xcd"
    "\x35\x90\x2e\x41\x8d\x6d\xaa\x09\x73\x95\x0c\xf1\x1d\xde\x4c\x2f\x2d\xf7\xd1\x72\xeb\xef\x48\xc7\xf8\xf9\xfa\xfb"
    "\xfc\xfd\xfe\xff";

ushort CLASS sget2Rev(uchar *s) // specific to some Canon Makernotes fields, where they have endian in reverse
{
  if (order == 0x4d4d) /* "II" means little-endian, and we reverse to "MM" - big endian */
    return s[0] | s[1] << 8;
  else /* "MM" means big-endian... */
    return s[0] << 8 | s[1];
}
#endif

ushort CLASS get2()
{
  uchar str[2] = {0xff, 0xff};
  fread(str, 1, 2, ifp);
  return sget2(str);
}

unsigned CLASS sget4(uchar *s)
{
  if (order == 0x4949)
    return s[0] | s[1] << 8 | s[2] << 16 | s[3] << 24;
  else
    return s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3];
}
#define sget4(s) sget4((uchar *)s)

unsigned CLASS get4()
{
  uchar str[4] = {0xff, 0xff, 0xff, 0xff};
  fread(str, 1, 4, ifp);
  return sget4(str);
}

unsigned CLASS getint(int type) { return type == 3 ? get2() : get4(); }

float CLASS int_to_float(int i)
{
  union {
    int i;
    float f;
  } u;
  u.i = i;
  return u.f;
}

double CLASS getreal(int type)
{
  union {
    char c[8];
    double d;
  } u, v;
  int i, rev;

  switch (type)
  {
  case 3: // ushort "SHORT" (int16u)
    return (unsigned short)get2();
  case 4: // unsigned "LONG" (int32u)
    return (unsigned int)get4();
  case 5: // (unsigned, unsigned) "RATIONAL" (rational64u)
    u.d = (unsigned int)get4();
    v.d = (unsigned int)get4();
    return u.d / (v.d ? v.d : 1);
  case 8: // short "SSHORT" (int16s)
    return (signed short)get2();
  case 9: // int "SLONG" (int32s)
    return (signed int)get4();
  case 10: // (int, int) "SRATIONAL" (rational64s)
    u.d = (signed int)get4();
    v.d = (signed int)get4();
    return u.d / (v.d ? v.d : 1);
  case 11: // float "FLOAT" (float)
    return int_to_float(get4());
  case 12: // double "DOUBLE" (double)
    rev = 7 * ((order == 0x4949) == (ntohs(0x1234) == 0x1234));
    for (i = 0; i < 8; i++)
      u.c[i ^ rev] = fgetc(ifp);
    return u.d;
  default:
    return fgetc(ifp);
  }
}

void CLASS read_shorts(ushort *pixel, unsigned count)
{
  if (fread(pixel, 2, count, ifp) < count)
    derror();
  if ((order == 0x4949) == (ntohs(0x1234) == 0x1234))
    swab((char *)pixel, (char *)pixel, count * 2);
}

void CLASS cubic_spline(const int *x_, const int *y_, const int len)
{
  float **A, *b, *c, *d, *x, *y;
  int i, j;

  A = (float **)calloc(((2 * len + 4) * sizeof **A + sizeof *A), 2 * len);
  if (!A)
    return;
  A[0] = (float *)(A + 2 * len);
  for (i = 1; i < 2 * len; i++)
    A[i] = A[0] + 2 * len * i;
  y = len + (x = i + (d = i + (c = i + (b = A[0] + i * i))));
  for (i = 0; i < len; i++)
  {
    x[i] = x_[i] / 65535.0;
    y[i] = y_[i] / 65535.0;
  }
  for (i = len - 1; i > 0; i--)
  {
    b[i] = (y[i] - y[i - 1]) / (x[i] - x[i - 1]);
    d[i - 1] = x[i] - x[i - 1];
  }
  for (i = 1; i < len - 1; i++)
  {
    A[i][i] = 2 * (d[i - 1] + d[i]);
    if (i > 1)
    {
      A[i][i - 1] = d[i - 1];
      A[i - 1][i] = d[i - 1];
    }
    A[i][len - 1] = 6 * (b[i + 1] - b[i]);
  }
  for (i = 1; i < len - 2; i++)
  {
    float v = A[i + 1][i] / A[i][i];
    for (j = 1; j <= len - 1; j++)
      A[i + 1][j] -= v * A[i][j];
  }
  for (i = len - 2; i > 0; i--)
  {
    float acc = 0;
    for (j = i; j <= len - 2; j++)
      acc += A[i][j] * c[j];
    c[i] = (A[i][len - 1] - acc) / A[i][i];
  }
  for (i = 0; i < 0x10000; i++)
  {
    float x_out = (float)(i / 65535.0);
    float y_out = 0;
    for (j = 0; j < len - 1; j++)
    {
      if (x[j] <= x_out && x_out <= x[j + 1])
      {
        float v = x_out - x[j];
        y_out = y[j] + ((y[j + 1] - y[j]) / d[j] - (2 * d[j] * c[j] + c[j + 1] * d[j]) / 6) * v + (c[j] * 0.5) * v * v +
                ((c[j + 1] - c[j]) / (6 * d[j])) * v * v * v;
      }
    }
    curve[i] = y_out < 0.0 ? 0 : (y_out >= 1.0 ? 65535 : (ushort)(y_out * 65535.0 + 0.5));
  }
  free(A);
}

void CLASS canon_600_fixed_wb(int temp)
{
  static const short mul[4][5] = {
      {667, 358, 397, 565, 452}, {731, 390, 367, 499, 517}, {1119, 396, 348, 448, 537}, {1399, 485, 431, 508, 688}};
  int lo, hi, i;
  float frac = 0;

  for (lo = 4; --lo;)
    if (*mul[lo] <= temp)
      break;
  for (hi = 0; hi < 3; hi++)
    if (*mul[hi] >= temp)
      break;
  if (lo != hi)
    frac = (float)(temp - *mul[lo]) / (*mul[hi] - *mul[lo]);
  for (i = 1; i < 5; i++)
    pre_mul[i - 1] = 1 / (frac * mul[hi][i] + (1 - frac) * mul[lo][i]);
}

/* Return values:  0 = white  1 = near white  2 = not white */
int CLASS canon_600_color(int ratio[2], int mar)
{
  int clipped = 0, target, miss;

  if (flash_used)
  {
    if (ratio[1] < -104)
    {
      ratio[1] = -104;
      clipped = 1;
    }
    if (ratio[1] > 12)
    {
      ratio[1] = 12;
      clipped = 1;
    }
  }
  else
  {
    if (ratio[1] < -264 || ratio[1] > 461)
      return 2;
    if (ratio[1] < -50)
    {
      ratio[1] = -50;
      clipped = 1;
    }
    if (ratio[1] > 307)
    {
      ratio[1] = 307;
      clipped = 1;
    }
  }
  target = flash_used || ratio[1] < 197 ? -38 - (398 * ratio[1] >> 10) : -123 + (48 * ratio[1] >> 10);
  if (target - mar <= ratio[0] && target + 20 >= ratio[0] && !clipped)
    return 0;
  miss = target - ratio[0];
  if (abs(miss) >= mar * 4)
    return 2;
  if (miss < -20)
    miss = -20;
  if (miss > mar)
    miss = mar;
  ratio[0] = target - miss;
  return 1;
}

void CLASS canon_600_auto_wb()
{
  int mar, row, col, i, j, st, count[] = {0, 0};
  int test[8], total[2][8], ratio[2][2], stat[2];

  memset(&total, 0, sizeof total);
  i = canon_ev + 0.5;
  if (i < 10)
    mar = 150;
  else if (i > 12)
    mar = 20;
  else
    mar = 280 - 20 * i;
  if (flash_used)
    mar = 80;
  for (row = 14; row < height - 14; row += 4)
    for (col = 10; col < width; col += 2)
    {
      for (i = 0; i < 8; i++)
        test[(i & 4) + FC(row + (i >> 1), col + (i & 1))] = BAYER(row + (i >> 1), col + (i & 1));
      for (i = 0; i < 8; i++)
        if (test[i] < 150 || test[i] > 1500)
          goto next;
      for (i = 0; i < 4; i++)
        if (abs(test[i] - test[i + 4]) > 50)
          goto next;
      for (i = 0; i < 2; i++)
      {
        for (j = 0; j < 4; j += 2)
          ratio[i][j >> 1] = ((test[i * 4 + j + 1] - test[i * 4 + j]) << 10) / test[i * 4 + j];
        stat[i] = canon_600_color(ratio[i], mar);
      }
      if ((st = stat[0] | stat[1]) > 1)
        goto next;
      for (i = 0; i < 2; i++)
        if (stat[i])
          for (j = 0; j < 2; j++)
            test[i * 4 + j * 2 + 1] = test[i * 4 + j * 2] * (0x400 + ratio[i][j]) >> 10;
      for (i = 0; i < 8; i++)
        total[st][i] += test[i];
      count[st]++;
    next:;
    }
  if (count[0] | count[1])
  {
    st = count[0] * 200 < count[1];
    for (i = 0; i < 4; i++)
      pre_mul[i] = 1.0 / (total[st][i] + total[st][i + 4]);
  }
}

void CLASS canon_600_coeff()
{
  static const short table[6][12] = {{-190, 702, -1878, 2390, 1861, -1349, 905, -393, -432, 944, 2617, -2105},
                                     {-1203, 1715, -1136, 1648, 1388, -876, 267, 245, -1641, 2153, 3921, -3409},
                                     {-615, 1127, -1563, 2075, 1437, -925, 509, 3, -756, 1268, 2519, -2007},
                                     {-190, 702, -1886, 2398, 2153, -1641, 763, -251, -452, 964, 3040, -2528},
                                     {-190, 702, -1878, 2390, 1861, -1349, 905, -393, -432, 944, 2617, -2105},
                                     {-807, 1319, -1785, 2297, 1388, -876, 769, -257, -230, 742, 2067, -1555}};
  int t = 0, i, c;
  float mc, yc;

  mc = pre_mul[1] / pre_mul[2];
  yc = pre_mul[3] / pre_mul[2];
  if (mc > 1 && mc <= 1.28 && yc < 0.8789)
    t = 1;
  if (mc > 1.28 && mc <= 2)
  {
    if (yc < 0.8789)
      t = 3;
    else if (yc <= 2)
      t = 4;
  }
  if (flash_used)
    t = 5;
  for (raw_color = i = 0; i < 3; i++)
    FORCC rgb_cam[i][c] = table[t][i * 4 + c] / 1024.0;
}

void CLASS canon_600_load_raw()
{
  uchar data[1120], *dp;
  ushort *pix;
  int irow, row;

  for (irow = row = 0; irow < height; irow++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    if (fread(data, 1, 1120, ifp) < 1120)
      derror();
    pix = raw_image + row * raw_width;
    for (dp = data; dp < data + 1120; dp += 10, pix += 8)
    {
      pix[0] = (dp[0] << 2) + (dp[1] >> 6);
      pix[1] = (dp[2] << 2) + (dp[1] >> 4 & 3);
      pix[2] = (dp[3] << 2) + (dp[1] >> 2 & 3);
      pix[3] = (dp[4] << 2) + (dp[1] & 3);
      pix[4] = (dp[5] << 2) + (dp[9] & 3);
      pix[5] = (dp[6] << 2) + (dp[9] >> 2 & 3);
      pix[6] = (dp[7] << 2) + (dp[9] >> 4 & 3);
      pix[7] = (dp[8] << 2) + (dp[9] >> 6);
    }
    if ((row += 2) > height)
      row = 1;
  }
}

void CLASS canon_600_correct()
{
  int row, col, val;
  static const short mul[4][2] = {{1141, 1145}, {1128, 1109}, {1178, 1149}, {1128, 1109}};

  for (row = 0; row < height; row++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    for (col = 0; col < width; col++)
    {
      if ((val = BAYER(row, col) - black) < 0)
        val = 0;
      val = val * mul[row & 3][col & 1] >> 9;
      BAYER(row, col) = val;
    }
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

  for (row = 0; row < 100; row++)
  {
    fseek(ifp, row * 3340 + 3284, SEEK_SET);
    if (getc(ifp) > 15)
      return 1;
  }
  return 0;
}

unsigned CLASS getbithuff(int nbits, ushort *huff)
{
#ifdef LIBRAW_NOTHREADS
  static unsigned bitbuf = 0;
  static int vbits = 0, reset = 0;
#else
#define bitbuf tls->getbits.bitbuf
#define vbits tls->getbits.vbits
#define reset tls->getbits.reset
#endif
  unsigned c;

  if (nbits > 25)
    return 0;
  if (nbits < 0)
    return bitbuf = vbits = reset = 0;
  if (nbits == 0 || vbits < 0)
    return 0;
  while (!reset && vbits < nbits && (c = fgetc(ifp)) != EOF && !(reset = zero_after_ff && c == 0xff && fgetc(ifp)))
  {
    bitbuf = (bitbuf << 8) + (uchar)c;
    vbits += 8;
  }
  c = bitbuf << (32 - vbits) >> (32 - nbits);
  if (huff)
  {
    vbits -= huff[c] >> 8;
    c = (uchar)huff[c];
  }
  else
    vbits -= nbits;
  if (vbits < 0)
    derror();
  return c;
#ifndef LIBRAW_NOTHREADS
#undef bitbuf
#undef vbits
#undef reset
#endif
}

#define getbits(n) getbithuff(n, 0)
#define gethuff(h) getbithuff(*h, h + 1)

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
ushort *CLASS make_decoder_ref(const uchar **source)
{
  int max, len, h, i, j;
  const uchar *count;
  ushort *huff;

  count = (*source += 16) - 17;
  for (max = 16; max && !count[max]; max--)
    ;
  huff = (ushort *)calloc(1 + (1 << max), sizeof *huff);
  merror(huff, "make_decoder()");
  huff[0] = max;
  for (h = len = 1; len <= max; len++)
    for (i = 0; i < count[len]; i++, ++*source)
      for (j = 0; j < 1 << (max - len); j++)
        if (h <= 1 << max)
          huff[h++] = len << 8 | **source;
  return huff;
}

ushort *CLASS make_decoder(const uchar *source) { return make_decoder_ref(&source); }

void CLASS crw_init_tables(unsigned table, ushort *huff[2])
{
  static const uchar first_tree[3][29] = {
      {0, 1,    4,    2,    3,    1,    2,    0,    0,    0,    0,    0,    0,    0,   0,
       0, 0x04, 0x03, 0x05, 0x06, 0x02, 0x07, 0x01, 0x08, 0x09, 0x00, 0x0a, 0x0b, 0xff},
      {0, 2,    2,    3,    1,    1,    1,    1,    2,    0,    0,    0,    0,    0,   0,
       0, 0x03, 0x02, 0x04, 0x01, 0x05, 0x00, 0x06, 0x07, 0x09, 0x08, 0x0a, 0x0b, 0xff},
      {0, 0,    6,    3,    1,    1,    2,    0,    0,    0,    0,    0,    0,    0,   0,
       0, 0x06, 0x05, 0x07, 0x04, 0x08, 0x03, 0x09, 0x02, 0x00, 0x0a, 0x01, 0x0b, 0xff},
  };
  static const uchar second_tree[3][180] = {
      {0,    2,    2,    2,    1,    4,    2,    1,    2,    5,    1,    1,    0,    0,    0,    139,  0x03, 0x04,
       0x02, 0x05, 0x01, 0x06, 0x07, 0x08, 0x12, 0x13, 0x11, 0x14, 0x09, 0x15, 0x22, 0x00, 0x21, 0x16, 0x0a, 0xf0,
       0x23, 0x17, 0x24, 0x31, 0x32, 0x18, 0x19, 0x33, 0x25, 0x41, 0x34, 0x42, 0x35, 0x51, 0x36, 0x37, 0x38, 0x29,
       0x79, 0x26, 0x1a, 0x39, 0x56, 0x57, 0x28, 0x27, 0x52, 0x55, 0x58, 0x43, 0x76, 0x59, 0x77, 0x54, 0x61, 0xf9,
       0x71, 0x78, 0x75, 0x96, 0x97, 0x49, 0xb7, 0x53, 0xd7, 0x74, 0xb6, 0x98, 0x47, 0x48, 0x95, 0x69, 0x99, 0x91,
       0xfa, 0xb8, 0x68, 0xb5, 0xb9, 0xd6, 0xf7, 0xd8, 0x67, 0x46, 0x45, 0x94, 0x89, 0xf8, 0x81, 0xd5, 0xf6, 0xb4,
       0x88, 0xb1, 0x2a, 0x44, 0x72, 0xd9, 0x87, 0x66, 0xd4, 0xf5, 0x3a, 0xa7, 0x73, 0xa9, 0xa8, 0x86, 0x62, 0xc7,
       0x65, 0xc8, 0xc9, 0xa1, 0xf4, 0xd1, 0xe9, 0x5a, 0x92, 0x85, 0xa6, 0xe7, 0x93, 0xe8, 0xc1, 0xc6, 0x7a, 0x64,
       0xe1, 0x4a, 0x6a, 0xe6, 0xb3, 0xf1, 0xd3, 0xa5, 0x8a, 0xb2, 0x9a, 0xba, 0x84, 0xa4, 0x63, 0xe5, 0xc5, 0xf3,
       0xd2, 0xc4, 0x82, 0xaa, 0xda, 0xe4, 0xf2, 0xca, 0x83, 0xa3, 0xa2, 0xc3, 0xea, 0xc2, 0xe2, 0xe3, 0xff, 0xff},
      {0,    2,    2,    1,    4,    1,    4,    1,    3,    3,    1,    0,    0,    0,    0,    140,  0x02, 0x03,
       0x01, 0x04, 0x05, 0x12, 0x11, 0x06, 0x13, 0x07, 0x08, 0x14, 0x22, 0x09, 0x21, 0x00, 0x23, 0x15, 0x31, 0x32,
       0x0a, 0x16, 0xf0, 0x24, 0x33, 0x41, 0x42, 0x19, 0x17, 0x25, 0x18, 0x51, 0x34, 0x43, 0x52, 0x29, 0x35, 0x61,
       0x39, 0x71, 0x62, 0x36, 0x53, 0x26, 0x38, 0x1a, 0x37, 0x81, 0x27, 0x91, 0x79, 0x55, 0x45, 0x28, 0x72, 0x59,
       0xa1, 0xb1, 0x44, 0x69, 0x54, 0x58, 0xd1, 0xfa, 0x57, 0xe1, 0xf1, 0xb9, 0x49, 0x47, 0x63, 0x6a, 0xf9, 0x56,
       0x46, 0xa8, 0x2a, 0x4a, 0x78, 0x99, 0x3a, 0x75, 0x74, 0x86, 0x65, 0xc1, 0x76, 0xb6, 0x96, 0xd6, 0x89, 0x85,
       0xc9, 0xf5, 0x95, 0xb4, 0xc7, 0xf7, 0x8a, 0x97, 0xb8, 0x73, 0xb7, 0xd8, 0xd9, 0x87, 0xa7, 0x7a, 0x48, 0x82,
       0x84, 0xea, 0xf4, 0xa6, 0xc5, 0x5a, 0x94, 0xa4, 0xc6, 0x92, 0xc3, 0x68, 0xb5, 0xc8, 0xe4, 0xe5, 0xe6, 0xe9,
       0xa2, 0xa3, 0xe3, 0xc2, 0x66, 0x67, 0x93, 0xaa, 0xd4, 0xd5, 0xe7, 0xf8, 0x88, 0x9a, 0xd7, 0x77, 0xc4, 0x64,
       0xe2, 0x98, 0xa5, 0xca, 0xda, 0xe8, 0xf3, 0xf6, 0xa9, 0xb2, 0xb3, 0xf2, 0xd2, 0x83, 0xba, 0xd3, 0xff, 0xff},
      {0,    0,    6,    2,    1,    3,    3,    2,    5,    1,    2,    2,    8,    10,   0,    117,  0x04, 0x05,
       0x03, 0x06, 0x02, 0x07, 0x01, 0x08, 0x09, 0x12, 0x13, 0x14, 0x11, 0x15, 0x0a, 0x16, 0x17, 0xf0, 0x00, 0x22,
       0x21, 0x18, 0x23, 0x19, 0x24, 0x32, 0x31, 0x25, 0x33, 0x38, 0x37, 0x34, 0x35, 0x36, 0x39, 0x79, 0x57, 0x58,
       0x59, 0x28, 0x56, 0x78, 0x27, 0x41, 0x29, 0x77, 0x26, 0x42, 0x76, 0x99, 0x1a, 0x55, 0x98, 0x97, 0xf9, 0x48,
       0x54, 0x96, 0x89, 0x47, 0xb7, 0x49, 0xfa, 0x75, 0x68, 0xb6, 0x67, 0x69, 0xb9, 0xb8, 0xd8, 0x52, 0xd7, 0x88,
       0xb5, 0x74, 0x51, 0x46, 0xd9, 0xf8, 0x3a, 0xd6, 0x87, 0x45, 0x7a, 0x95, 0xd5, 0xf6, 0x86, 0xb4, 0xa9, 0x94,
       0x53, 0x2a, 0xa8, 0x43, 0xf5, 0xf7, 0xd4, 0x66, 0xa7, 0x5a, 0x44, 0x8a, 0xc9, 0xe8, 0xc8, 0xe7, 0x9a, 0x6a,
       0x73, 0x4a, 0x61, 0xc7, 0xf4, 0xc6, 0x65, 0xe9, 0x72, 0xe6, 0x71, 0x91, 0x93, 0xa6, 0xda, 0x92, 0x85, 0x62,
       0xf3, 0xc5, 0xb2, 0xa4, 0x84, 0xba, 0x64, 0xa5, 0xb3, 0xd2, 0x81, 0xe5, 0xd3, 0xaa, 0xc4, 0xca, 0xf2, 0xb1,
       0xe4, 0xd1, 0x83, 0x63, 0xea, 0xc3, 0xe2, 0x82, 0xf1, 0xa3, 0xc2, 0xa1, 0xc1, 0xe3, 0xa2, 0xe1, 0xff, 0xff}};
  if (table > 2)
    table = 2;
  huff[0] = make_decoder(first_tree[table]);
  huff[1] = make_decoder(second_tree[table]);
}

/*
   Return 0 if the image starts with compressed data,
   1 if it starts with uncompressed low-order bits.

   In Canon compressed data, 0xff is always followed by 0x00.
 */
int CLASS canon_has_lowbits()
{
  uchar test[0x4000];
  int ret = 1, i;

  fseek(ifp, 0, SEEK_SET);
  fread(test, 1, sizeof test, ifp);
  for (i = 540; i < sizeof test - 1; i++)
    if (test[i] == 0xff)
    {
      if (test[i + 1])
        return 1;
      ret = 0;
    }
  return ret;
}

void CLASS canon_load_raw()
{
  ushort *pixel, *prow, *huff[2];
  int nblocks, lowbits, i, c, row, r, save, val;
  int block, diffbuf[64], leaf, len, diff, carry = 0, pnum = 0, base[2];

  crw_init_tables(tiff_compress, huff);
  lowbits = canon_has_lowbits();
  if (!lowbits)
    maximum = 0x3ff;
  fseek(ifp, 540 + lowbits * raw_height * raw_width / 4, SEEK_SET);
  zero_after_ff = 1;
  getbits(-1);
#ifdef LIBRAW_LIBRARY_BUILD
  try
  {
#endif
    for (row = 0; row < raw_height; row += 8)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      pixel = raw_image + row * raw_width;
      nblocks = MIN(8, raw_height - row) * raw_width >> 6;
      for (block = 0; block < nblocks; block++)
      {
        memset(diffbuf, 0, sizeof diffbuf);
        for (i = 0; i < 64; i++)
        {
          leaf = gethuff(huff[i > 0]);
          if (leaf == 0 && i)
            break;
          if (leaf == 0xff)
            continue;
          i += leaf >> 4;
          len = leaf & 15;
          if (len == 0)
            continue;
          diff = getbits(len);
          if ((diff & (1 << (len - 1))) == 0)
            diff -= (1 << len) - 1;
          if (i < 64)
            diffbuf[i] = diff;
        }
        diffbuf[0] += carry;
        carry = diffbuf[0];
        for (i = 0; i < 64; i++)
        {
          if (pnum++ % raw_width == 0)
            base[0] = base[1] = 512;
          if ((pixel[(block << 6) + i] = base[i & 1] += diffbuf[i]) >> 10)
            derror();
        }
      }
      if (lowbits)
      {
        save = ftell(ifp);
        fseek(ifp, 26 + row * raw_width / 4, SEEK_SET);
        for (prow = pixel, i = 0; i < raw_width * 2; i++)
        {
          c = fgetc(ifp);
          for (r = 0; r < 8; r += 2, prow++)
          {
            val = (*prow << 2) + ((c >> r) & 3);
            if (raw_width == 2672 && val < 512)
              val += 2;
            *prow = val;
          }
        }
        fseek(ifp, save, SEEK_SET);
      }
    }
#ifdef LIBRAW_LIBRARY_BUILD
  }
  catch (...)
  {
    FORC(2) free(huff[c]);
    throw;
  }
#endif
  FORC(2) free(huff[c]);
}
//@end COMMON

struct jhead
{
  int algo, bits, high, wide, clrs, sraw, psv, restart, vpred[6];
  ushort quant[64], idct[64], *huff[20], *free[20], *row;
};

//@out COMMON

int CLASS ljpeg_start(struct jhead *jh, int info_only)
{
  ushort c, tag, len;
  int cnt = 0;
  uchar data[0x10000];
  const uchar *dp;

  memset(jh, 0, sizeof *jh);
  jh->restart = INT_MAX;
  if ((fgetc(ifp), fgetc(ifp)) != 0xd8)
    return 0;
  do
  {
    if (feof(ifp))
      return 0;
    if (cnt++ > 1024)
      return 0; // 1024 tags limit
    if (!fread(data, 2, 2, ifp))
      return 0;
    tag = data[0] << 8 | data[1];
    len = (data[2] << 8 | data[3]) - 2;
    if (tag <= 0xff00)
      return 0;
    fread(data, 1, len, ifp);
    switch (tag)
    {
    case 0xffc3: // start of frame; lossless, Huffman
      jh->sraw = ((data[7] >> 4) * (data[7] & 15) - 1) & 3;
    case 0xffc1:
    case 0xffc0:
      jh->algo = tag & 0xff;
      jh->bits = data[0];
      jh->high = data[1] << 8 | data[2];
      jh->wide = data[3] << 8 | data[4];
      jh->clrs = data[5] + jh->sraw;
      if (len == 9 && !dng_version)
        getc(ifp);
      break;
    case 0xffc4: // define Huffman tables
      if (info_only)
        break;
      for (dp = data; dp < data + len && !((c = *dp++) & -20);)
        jh->free[c] = jh->huff[c] = make_decoder_ref(&dp);
      break;
    case 0xffda: // start of scan
      jh->psv = data[1 + data[0] * 2];
      jh->bits -= data[3 + data[0] * 2] & 15;
      break;
    case 0xffdb:
      FORC(64) jh->quant[c] = data[c * 2 + 1] << 8 | data[c * 2 + 2];
      break;
    case 0xffdd:
      jh->restart = data[0] << 8 | data[1];
    }
  } while (tag != 0xffda);
  if (jh->bits > 16 || jh->clrs > 6 || !jh->bits || !jh->high || !jh->wide || !jh->clrs)
    return 0;
  if (info_only)
    return 1;
  if (!jh->huff[0])
    return 0;
  FORC(19) if (!jh->huff[c + 1]) jh->huff[c + 1] = jh->huff[c];
  if (jh->sraw)
  {
    FORC(4) jh->huff[2 + c] = jh->huff[1];
    FORC(jh->sraw) jh->huff[1 + c] = jh->huff[0];
  }
  jh->row = (ushort *)calloc(jh->wide * jh->clrs, 4);
  merror(jh->row, "ljpeg_start()");
  return zero_after_ff = 1;
}

void CLASS ljpeg_end(struct jhead *jh)
{
  int c;
  FORC4 if (jh->free[c]) free(jh->free[c]);
  free(jh->row);
}

int CLASS ljpeg_diff(ushort *huff)
{
  int len, diff;
  if (!huff)
#ifdef LIBRAW_LIBRARY_BUILD
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
#else
    longjmp(failure, 2);
#endif

  len = gethuff(huff);
  if (len == 16 && (!dng_version || dng_version >= 0x1010000))
    return -32768;
  diff = getbits(len);
  if ((diff & (1 << (len - 1))) == 0)
    diff -= (1 << len) - 1;
  return diff;
}

ushort *CLASS ljpeg_row(int jrow, struct jhead *jh)
{
  int col, c, diff, pred, spred = 0;
  ushort mark = 0, *row[3];

  if (jrow * jh->wide % jh->restart == 0)
  {
    FORC(6) jh->vpred[c] = 1 << (jh->bits - 1);
    if (jrow)
    {
      fseek(ifp, -2, SEEK_CUR);
      do
        mark = (mark << 8) + (c = fgetc(ifp));
      while (c != EOF && mark >> 4 != 0xffd);
    }
    getbits(-1);
  }
  FORC3 row[c] = jh->row + jh->wide * jh->clrs * ((jrow + c) & 1);
  for (col = 0; col < jh->wide; col++)
    FORC(jh->clrs)
    {
      diff = ljpeg_diff(jh->huff[c]);
      if (jh->sraw && c <= jh->sraw && (col | c))
        pred = spred;
      else if (col)
        pred = row[0][-jh->clrs];
      else
        pred = (jh->vpred[c] += diff) - diff;
      if (jrow && col)
        switch (jh->psv)
        {
        case 1:
          break;
        case 2:
          pred = row[1][0];
          break;
        case 3:
          pred = row[1][-jh->clrs];
          break;
        case 4:
          pred = pred + row[1][0] - row[1][-jh->clrs];
          break;
        case 5:
          pred = pred + ((row[1][0] - row[1][-jh->clrs]) >> 1);
          break;
        case 6:
          pred = row[1][0] + ((pred - row[1][-jh->clrs]) >> 1);
          break;
        case 7:
          pred = (pred + row[1][0]) >> 1;
          break;
        default:
          pred = 0;
        }
      if ((**row = pred + diff) >> jh->bits)
        derror();
      if (c <= jh->sraw)
        spred = **row;
      row[0]++;
      row[1]++;
    }
  return row[2];
}

void CLASS lossless_jpeg_load_raw()
{
  int jwide, jhigh, jrow, jcol, val, jidx, i, j, row = 0, col = 0;
  struct jhead jh;
  ushort *rp;

  if (!ljpeg_start(&jh, 0))
    return;

  if (jh.wide < 1 || jh.high < 1 || jh.clrs < 1 || jh.bits < 1)
#ifdef LIBRAW_LIBRARY_BUILD
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
#else
    longjmp(failure, 2);
#endif
  jwide = jh.wide * jh.clrs;
  jhigh = jh.high;
  if (jh.clrs == 4 && jwide >= raw_width * 2)
    jhigh *= 2;

#ifdef LIBRAW_LIBRARY_BUILD
  try
  {
#endif
    for (jrow = 0; jrow < jh.high; jrow++)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      rp = ljpeg_row(jrow, &jh);
      if (load_flags & 1)
        row = jrow & 1 ? height - 1 - jrow / 2 : jrow / 2;
      for (jcol = 0; jcol < jwide; jcol++)
      {
        val = curve[*rp++];
        if (cr2_slice[0])
        {
          jidx = jrow * jwide + jcol;
          i = jidx / (cr2_slice[1] * raw_height);
          if ((j = i >= cr2_slice[0]))
            i = cr2_slice[0];
          jidx -= i * (cr2_slice[1] * raw_height);
          row = jidx / cr2_slice[1 + j];
          col = jidx % cr2_slice[1 + j] + i * cr2_slice[1];
        }
        if (raw_width == 3984 && (col -= 2) < 0)
          col += (row--, raw_width);
        if (row > raw_height)
#ifdef LIBRAW_LIBRARY_BUILD
          throw LIBRAW_EXCEPTION_IO_CORRUPT;
#else
        longjmp(failure, 3);
#endif
        if ((unsigned)row < raw_height)
          RAW(row, col) = val;
        if (++col >= raw_width)
          col = (row++, 0);
      }
    }
#ifdef LIBRAW_LIBRARY_BUILD
  }
  catch (...)
  {
    ljpeg_end(&jh);
    throw;
  }
#endif
  ljpeg_end(&jh);
}

void CLASS canon_sraw_load_raw()
{
  struct jhead jh;
  short *rp = 0, (*ip)[4];
  int jwide, slice, scol, ecol, row, col, jrow = 0, jcol = 0, pix[3], c;
  int v[3] = {0, 0, 0}, ver, hue;
#ifdef LIBRAW_LIBRARY_BUILD
  int saved_w = width, saved_h = height;
#endif
  char *cp;

  if (!ljpeg_start(&jh, 0) || jh.clrs < 4)
    return;
  jwide = (jh.wide >>= 1) * jh.clrs;

#ifdef LIBRAW_LIBRARY_BUILD
  if (load_flags & 256)
  {
    width = raw_width;
    height = raw_height;
  }

  try
  {
#endif
    for (ecol = slice = 0; slice <= cr2_slice[0]; slice++)
    {
      scol = ecol;
      ecol += cr2_slice[1] * 2 / jh.clrs;
      if (!cr2_slice[0] || ecol > raw_width - 1)
        ecol = raw_width & -2;
      for (row = 0; row < height; row += (jh.clrs >> 1) - 1)
      {
#ifdef LIBRAW_LIBRARY_BUILD
        checkCancel();
#endif
        ip = (short(*)[4])image + row * width;
        for (col = scol; col < ecol; col += 2, jcol += jh.clrs)
        {
          if ((jcol %= jwide) == 0)
            rp = (short *)ljpeg_row(jrow++, &jh);
          if (col >= width)
            continue;
#ifdef LIBRAW_LIBRARY_BUILD
          if (imgdata.params.raw_processing_options & LIBRAW_PROCESSING_SRAW_NO_INTERPOLATE)
          {
            FORC(jh.clrs - 2)
            {
              ip[col + (c >> 1) * width + (c & 1)][0] = rp[jcol + c];
              ip[col + (c >> 1) * width + (c & 1)][1] = ip[col + (c >> 1) * width + (c & 1)][2] = 8192;
            }
            ip[col][1] = rp[jcol + jh.clrs - 2] - 8192;
            ip[col][2] = rp[jcol + jh.clrs - 1] - 8192;
          }
          else if (imgdata.params.raw_processing_options & LIBRAW_PROCESSING_SRAW_NO_RGB)
          {
            FORC(jh.clrs - 2)
            ip[col + (c >> 1) * width + (c & 1)][0] = rp[jcol + c];
            ip[col][1] = rp[jcol + jh.clrs - 2] - 8192;
            ip[col][2] = rp[jcol + jh.clrs - 1] - 8192;
          }
          else
#endif
          {
            FORC(jh.clrs - 2)
            ip[col + (c >> 1) * width + (c & 1)][0] = rp[jcol + c];
            ip[col][1] = rp[jcol + jh.clrs - 2] - 16384;
            ip[col][2] = rp[jcol + jh.clrs - 1] - 16384;
          }
        }
      }
    }
#ifdef LIBRAW_LIBRARY_BUILD
  }
  catch (...)
  {
    ljpeg_end(&jh);
    throw;
  }
#endif

#ifdef LIBRAW_LIBRARY_BUILD
  if (imgdata.params.raw_processing_options & LIBRAW_PROCESSING_SRAW_NO_INTERPOLATE)
  {
    ljpeg_end(&jh);
    maximum = 0x3fff;
    height = saved_h;
    width = saved_w;
    return;
  }
#endif

#ifdef LIBRAW_LIBRARY_BUILD
  try
  {
#endif
    for (cp = model2; *cp && !isdigit(*cp); cp++)
      ;
    sscanf(cp, "%d.%d.%d", v, v + 1, v + 2);
    ver = (v[0] * 1000 + v[1]) * 1000 + v[2];
    hue = (jh.sraw + 1) << 2;
    if (unique_id >= 0x80000281 || (unique_id == 0x80000218 && ver > 1000006))
      hue = jh.sraw << 1;
    ip = (short(*)[4])image;
    rp = ip[0];
    for (row = 0; row < height; row++, ip += width)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      if (row & (jh.sraw >> 1))
      {
        for (col = 0; col < width; col += 2)
          for (c = 1; c < 3; c++)
            if (row == height - 1)
            {
              ip[col][c] = ip[col - width][c];
            }
            else
            {
              ip[col][c] = (ip[col - width][c] + ip[col + width][c] + 1) >> 1;
            }
      }
      for (col = 1; col < width; col += 2)
        for (c = 1; c < 3; c++)
          if (col == width - 1)
            ip[col][c] = ip[col - 1][c];
          else
            ip[col][c] = (ip[col - 1][c] + ip[col + 1][c] + 1) >> 1;
    }
#ifdef LIBRAW_LIBRARY_BUILD
    if (!(imgdata.params.raw_processing_options & LIBRAW_PROCESSING_SRAW_NO_RGB))
#endif
      for (; rp < ip[0]; rp += 4)
      {
#ifdef LIBRAW_LIBRARY_BUILD
        checkCancel();
#endif
        if (unique_id == 0x80000218 || unique_id == 0x80000250 || unique_id == 0x80000261 || unique_id == 0x80000281 ||
            unique_id == 0x80000287)
        {
          rp[1] = (rp[1] << 2) + hue;
          rp[2] = (rp[2] << 2) + hue;
          pix[0] = rp[0] + ((50 * rp[1] + 22929 * rp[2]) >> 14);
          pix[1] = rp[0] + ((-5640 * rp[1] - 11751 * rp[2]) >> 14);
          pix[2] = rp[0] + ((29040 * rp[1] - 101 * rp[2]) >> 14);
        }
        else
        {
          if (unique_id < 0x80000218)
            rp[0] -= 512;
          pix[0] = rp[0] + rp[2];
          pix[2] = rp[0] + rp[1];
          pix[1] = rp[0] + ((-778 * rp[1] - (rp[2] << 11)) >> 12);
        }
        FORC3 rp[c] = CLIP15(pix[c] * sraw_mul[c] >> 10);
      }
#ifdef LIBRAW_LIBRARY_BUILD
  }
  catch (...)
  {
    ljpeg_end(&jh);
    throw;
  }
  height = saved_h;
  width = saved_w;
#endif
  ljpeg_end(&jh);
  maximum = 0x3fff;
}

void CLASS adobe_copy_pixel(unsigned row, unsigned col, ushort **rp)
{
  int c;

  if (tiff_samples == 2 && shot_select)
    (*rp)++;
  if (raw_image)
  {
    if (row < raw_height && col < raw_width)
      RAW(row, col) = curve[**rp];
    *rp += tiff_samples;
  }
  else
  {
#ifdef LIBRAW_LIBRARY_BUILD
    if (row < raw_height && col < raw_width)
      FORC(tiff_samples)
    image[row * raw_width + col][c] = curve[(*rp)[c]];
    *rp += tiff_samples;
#else
    if (row < height && col < width)
      FORC(tiff_samples)
    image[row * width + col][c] = curve[(*rp)[c]];
    *rp += tiff_samples;
#endif
  }
  if (tiff_samples == 2 && shot_select)
    (*rp)--;
}

void CLASS ljpeg_idct(struct jhead *jh)
{
  int c, i, j, len, skip, coef;
  float work[3][8][8];
  static float cs[106] = {0};
  static const uchar zigzag[80] = {0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,  12, 19, 26, 33,
                                   40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36,
                                   29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54,
                                   47, 55, 62, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63};

  if (!cs[0])
    FORC(106) cs[c] = cos((c & 31) * M_PI / 16) / 2;
  memset(work, 0, sizeof work);
  work[0][0][0] = jh->vpred[0] += ljpeg_diff(jh->huff[0]) * jh->quant[0];
  for (i = 1; i < 64; i++)
  {
    len = gethuff(jh->huff[16]);
    i += skip = len >> 4;
    if (!(len &= 15) && skip < 15)
      break;
    coef = getbits(len);
    if ((coef & (1 << (len - 1))) == 0)
      coef -= (1 << len) - 1;
    ((float *)work)[zigzag[i]] = coef * jh->quant[i];
  }
  FORC(8) work[0][0][c] *= M_SQRT1_2;
  FORC(8) work[0][c][0] *= M_SQRT1_2;
  for (i = 0; i < 8; i++)
    for (j = 0; j < 8; j++)
      FORC(8) work[1][i][j] += work[0][i][c] * cs[(j * 2 + 1) * c];
  for (i = 0; i < 8; i++)
    for (j = 0; j < 8; j++)
      FORC(8) work[2][i][j] += work[1][c][j] * cs[(i * 2 + 1) * c];

  FORC(64) jh->idct[c] = CLIP(((float *)work[2])[c] + 0.5);
}

void CLASS lossless_dng_load_raw()
{
  unsigned save, trow = 0, tcol = 0, jwide, jrow, jcol, row, col, i, j;
  struct jhead jh;
  ushort *rp;

  while (trow < raw_height)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    save = ftell(ifp);
    if (tile_length < INT_MAX)
      fseek(ifp, get4(), SEEK_SET);
    if (!ljpeg_start(&jh, 0))
      break;
    jwide = jh.wide;
    if (filters)
      jwide *= jh.clrs;
    jwide /= MIN(is_raw, tiff_samples);
#ifdef LIBRAW_LIBRARY_BUILD
    try
    {
#endif
      switch (jh.algo)
      {
      case 0xc1:
        jh.vpred[0] = 16384;
        getbits(-1);
        for (jrow = 0; jrow + 7 < jh.high; jrow += 8)
        {
#ifdef LIBRAW_LIBRARY_BUILD
          checkCancel();
#endif
          for (jcol = 0; jcol + 7 < jh.wide; jcol += 8)
          {
            ljpeg_idct(&jh);
            rp = jh.idct;
            row = trow + jcol / tile_width + jrow * 2;
            col = tcol + jcol % tile_width;
            for (i = 0; i < 16; i += 2)
              for (j = 0; j < 8; j++)
                adobe_copy_pixel(row + i, col + j, &rp);
          }
        }
        break;
      case 0xc3:
        for (row = col = jrow = 0; jrow < jh.high; jrow++)
        {
#ifdef LIBRAW_LIBRARY_BUILD
          checkCancel();
#endif
          rp = ljpeg_row(jrow, &jh);
	  if(tiff_samples == 1 && jh.clrs > 1 && jh.clrs*jwide == raw_width)
          for (jcol = 0; jcol < jwide*jh.clrs; jcol++)
          {
            adobe_copy_pixel(trow + row, tcol + col, &rp);
            if (++col >= tile_width || col >= raw_width)
              row += 1 + (col = 0);
	  }
	  else
          for (jcol = 0; jcol < jwide; jcol++)
          {
            adobe_copy_pixel(trow + row, tcol + col, &rp);
            if (++col >= tile_width || col >= raw_width)
              row += 1 + (col = 0);
          }
        }
      }
#ifdef LIBRAW_LIBRARY_BUILD
    }
    catch (...)
    {
      ljpeg_end(&jh);
      throw;
    }
#endif
    fseek(ifp, save + 4, SEEK_SET);
    if ((tcol += tile_width) >= raw_width)
      trow += tile_length + (tcol = 0);
    ljpeg_end(&jh);
  }
}

void CLASS packed_dng_load_raw()
{
  ushort *pixel, *rp;
  int row, col;

  pixel = (ushort *)calloc(raw_width, tiff_samples * sizeof *pixel);
  merror(pixel, "packed_dng_load_raw()");
#ifdef LIBRAW_LIBRARY_BUILD
  try
  {
#endif
    for (row = 0; row < raw_height; row++)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      if (tiff_bps == 16)
        read_shorts(pixel, raw_width * tiff_samples);
      else
      {
        getbits(-1);
        for (col = 0; col < raw_width * tiff_samples; col++)
          pixel[col] = getbits(tiff_bps);
      }
      for (rp = pixel, col = 0; col < raw_width; col++)
        adobe_copy_pixel(row, col, &rp);
    }
#ifdef LIBRAW_LIBRARY_BUILD
  }
  catch (...)
  {
    free(pixel);
    throw;
  }
#endif
  free(pixel);
}

void CLASS pentax_load_raw()
{
  ushort bit[2][15], huff[4097];
  int dep, row, col, diff, c, i;
  ushort vpred[2][2] = {{0, 0}, {0, 0}}, hpred[2];

  fseek(ifp, meta_offset, SEEK_SET);
  dep = (get2() + 12) & 15;
  fseek(ifp, 12, SEEK_CUR);
  FORC(dep) bit[0][c] = get2();
  FORC(dep) bit[1][c] = fgetc(ifp);
  FORC(dep)
  for (i = bit[0][c]; i <= ((bit[0][c] + (4096 >> bit[1][c]) - 1) & 4095);)
    huff[++i] = bit[1][c] << 8 | c;
  huff[0] = 12;
  fseek(ifp, data_offset, SEEK_SET);
  getbits(-1);
  for (row = 0; row < raw_height; row++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    for (col = 0; col < raw_width; col++)
    {
      diff = ljpeg_diff(huff);
      if (col < 2)
        hpred[col] = vpred[row & 1][col] += diff;
      else
        hpred[col & 1] += diff;
      RAW(row, col) = hpred[col & 1];
      if (hpred[col & 1] >> tiff_bps)
        derror();
    }
  }
}

#ifdef LIBRAW_LIBRARY_BUILD

void CLASS nikon_coolscan_load_raw()
{
  if(!image)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;

  int bypp = tiff_bps <= 8 ? 1 : 2;
  int bufsize = width * 3 * bypp;

  if (tiff_bps <= 8)
    gamma_curve(1.0 / imgdata.params.coolscan_nef_gamma, 0., 1, 255);
  else
    gamma_curve(1.0 / imgdata.params.coolscan_nef_gamma, 0., 1, 65535);
  fseek(ifp, data_offset, SEEK_SET);
  unsigned char *buf = (unsigned char *)malloc(bufsize);
  unsigned short *ubuf = (unsigned short *)buf;
  for (int row = 0; row < raw_height; row++) {
    int red = fread(buf, 1, bufsize, ifp);
    unsigned short(*ip)[4] = (unsigned short(*)[4])image + row * width;
    if (is_NikonTransfer == 2) { // it is also (tiff_bps == 8)
      for (int col = 0; col < width; col++) {
        ip[col][0] = ((float)curve[buf[col * 3]    ]) / 255.0f;
        ip[col][1] = ((float)curve[buf[col * 3 + 1]]) / 255.0f;
        ip[col][2] = ((float)curve[buf[col * 3 + 2]]) / 255.0f;
        ip[col][3] = 0;
      }
    } else if (tiff_bps <= 8) {
      for (int col = 0; col < width; col++) {
        ip[col][0] = curve[buf[col * 3]];
        ip[col][1] = curve[buf[col * 3 + 1]];
        ip[col][2] = curve[buf[col * 3 + 2]];
        ip[col][3] = 0;
      }
    } else {
      for (int col = 0; col < width; col++) {
        ip[col][0] = curve[ubuf[col * 3]];
        ip[col][1] = curve[ubuf[col * 3 + 1]];
        ip[col][2] = curve[ubuf[col * 3 + 2]];
        ip[col][3] = 0;
      }
    }
  }
  free(buf);
}
#endif

void CLASS nikon_read_curve()
{
  ushort ver0, ver1, vpred[2][2], hpred[2], csize;
  int i,step,max;

  fseek(ifp, meta_offset, SEEK_SET);
  ver0 = fgetc(ifp);
  ver1 = fgetc(ifp);
  if (ver0 == 0x49 || ver1 == 0x58)
    fseek(ifp, 2110, SEEK_CUR);
  read_shorts(vpred[0], 4);
  max = 1 << tiff_bps & 0x7fff;
  if ((csize = get2()) > 1)
    step = max / (csize - 1);
  if (ver0 == 0x44 && (ver1 == 0x20 || (ver1 == 0x40 && step > 3)) && step > 0)
  {
    if(ver1 == 0x40) { step /= 4; max /= 4; }
    for (i = 0; i < csize; i++)
      curve[i * step] = get2();
    for (i = 0; i < max; i++)
      curve[i] = (curve[i - i % step] * (step - i % step) + curve[i - i % step + step] * (i % step)) / step;
  }
  else if (ver0 != 0x46 && csize <= 0x4001)
    read_shorts(curve, max = csize);
}

void CLASS nikon_load_raw()
{
  static const uchar nikon_tree[][32] = {
      {0, 1, 5, 1, 1, 1, 1, 1, 1, 2, 0,  0,  0, 0, 0, 0, /* 12-bit lossy */
       5, 4, 3, 6, 2, 7, 1, 0, 8, 9, 11, 10, 12},
      {0,    1,    5,    1,    1,    1, 1, 1, 1, 2, 0, 0,  0,  0, 0, 0, /* 12-bit lossy after split */
       0x39, 0x5a, 0x38, 0x27, 0x16, 5, 4, 3, 2, 1, 0, 11, 12, 12},

      {0, 1, 4, 2, 3, 1, 2, 0, 0, 0, 0,  0,  0, 0, 0, 0, /* 12-bit lossless */
       5, 4, 6, 3, 7, 2, 8, 1, 9, 0, 10, 11, 12},
      {0, 1, 4, 3, 1, 1, 1, 1, 1, 2, 0,  0,  0,  0,  0, 0, /* 14-bit lossy */
       5, 6, 4, 7, 8, 3, 9, 2, 1, 0, 10, 11, 12, 13, 14},
      {0, 1,    5,    1,    1,    1, 1, 1, 1, 1, 2, 0, 0, 0,  0, 0, /* 14-bit lossy after split */
       8, 0x5c, 0x4b, 0x3a, 0x29, 7, 6, 5, 4, 3, 2, 1, 0, 13, 14},
      {0, 1, 4, 2, 2, 3, 1,  2, 0,  0,  0, 0, 0, 0,  0, 0, /* 14-bit lossless */
       7, 6, 8, 5, 9, 4, 10, 3, 11, 12, 2, 0, 1, 13, 14}};
  ushort *huff, ver0, ver1, vpred[2][2], hpred[2]
#if 0
    ,csize
#endif
    ;
  int i, min, max,
#if 0
    step = 0,
#endif
    tree = 0, split = 0, row, col, len, shl, diff;

  fseek(ifp, meta_offset, SEEK_SET);
  ver0 = fgetc(ifp);
  ver1 = fgetc(ifp);
  if (ver0 == 0x49 || ver1 == 0x58)
    fseek(ifp, 2110, SEEK_CUR);
  if (ver0 == 0x46)
    tree = 2;
  if (tiff_bps == 14)
    tree += 3;
  read_shorts(vpred[0], 4);
  max = 1 << tiff_bps & 0x7fff;
#if 0
  if ((csize = get2()) > 1)
    step = max / (csize - 1);
  if (ver0 == 0x44 && (ver1 == 0x20 || (ver1 == 0x40 && step > 3)) && step > 0)
  {
    if(ver1 == 0x40) { step /= 4; max /= 4; }
    for (i = 0; i < csize; i++)
      curve[i * step] = get2();
    for (i = 0; i < max; i++)
      curve[i] = (curve[i - i % step] * (step - i % step) + curve[i - i % step + step] * (i % step)) / step;
    fseek(ifp, meta_offset + 562, SEEK_SET);
    split = get2();
  }
  else if (ver0 != 0x46 && csize <= 0x4001)
    read_shorts(curve, max = csize);
#else
  if (ver0 == 0x44 && (ver1 == 0x20 || ver1 == 0x40))
  {
    if(ver1 == 0x40) max /= 4;
    fseek(ifp, meta_offset + 562, SEEK_SET);
    split = get2();
  }
#endif

  while (curve[max - 2] == curve[max - 1])
    max--;
  huff = make_decoder(nikon_tree[tree]);
  fseek(ifp, data_offset, SEEK_SET);
  getbits(-1);
#ifdef LIBRAW_LIBRARY_BUILD
  try
  {
#endif
    for (min = row = 0; row < height; row++)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      if (split && row == split)
      {
        free(huff);
        huff = make_decoder(nikon_tree[tree + 1]);
        max += (min = 16) << 1;
      }
      for (col = 0; col < raw_width; col++)
      {
        i = gethuff(huff);
        len = i & 15;
        shl = i >> 4;
        diff = ((getbits(len - shl) << 1) + 1) << shl >> 1;
        if ((diff & (1 << (len - 1))) == 0)
          diff -= (1 << len) - !shl;
        if (col < 2)
          hpred[col] = vpred[row & 1][col] += diff;
        else
          hpred[col & 1] += diff;
        if ((ushort)(hpred[col & 1] + min) >= max)
          derror();
        RAW(row, col) = curve[LIM((short)hpred[col & 1], 0, 0x3fff)];
      }
    }
#ifdef LIBRAW_LIBRARY_BUILD
  }
  catch (...)
  {
    free(huff);
    throw;
  }
#endif
  free(huff);
}

void CLASS nikon_yuv_load_raw()
{
#ifdef LIBRAW_LIBRARY_BUILD
  if (!image)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
#endif
  int row, col, yuv[4], rgb[3], b, c;
  UINT64 bitbuf = 0;
  float cmul[4];
  FORC4 { cmul[c] = cam_mul[c] > 0.001f ? cam_mul[c] : 1.f; }
  for (row = 0; row < raw_height; row++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif

    for (col = 0; col < raw_width; col++)
    {
      if (!(b = col & 1))
      {
        bitbuf = 0;
        FORC(6) bitbuf |= (UINT64)fgetc(ifp) << c * 8;
        FORC(4) yuv[c] = (bitbuf >> c * 12 & 0xfff) - (c >> 1 << 11);
      }
      rgb[0] = yuv[b] + 1.370705 * yuv[3];
      rgb[1] = yuv[b] - 0.337633 * yuv[2] - 0.698001 * yuv[3];
      rgb[2] = yuv[b] + 1.732446 * yuv[2];
      FORC3 image[row * width + col][c] = curve[LIM(rgb[c], 0, 0xfff)] / cmul[c];
    }
  }
}

/*
   Returns 1 for a Coolpix 995, 0 for anything else.
 */
int CLASS nikon_e995()
{
  int i, histo[256];
  const uchar often[] = {0x00, 0x55, 0xaa, 0xff};

  memset(histo, 0, sizeof histo);
  fseek(ifp, -2000, SEEK_END);
  for (i = 0; i < 2000; i++)
    histo[fgetc(ifp)]++;
  for (i = 0; i < 4; i++)
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

  fseek(ifp, 0, SEEK_SET);
  for (i = 0; i < 1024; i++)
  {
    fread(t, 1, 12, ifp);
    if (((t[2] & t[4] & t[7] & t[9]) >> 4 & t[1] & t[6] & t[8] & t[11] & 3) != 3)
      return 0;
  }
  return 1;
}

void CLASS nikon_3700()
{
  int bits, i;
  uchar dp[24];
  static const struct
  {
    int bits;
    char t_make[12], t_model[15];
  } table[] = {
      {0x00, "Pentax", "Optio 33WR"}, {0x03, "Nikon", "E3200"}, {0x32, "Nikon", "E3700"}, {0x33, "Olympus", "C740UZ"}};

  fseek(ifp, 3072, SEEK_SET);
  fread(dp, 1, 24, ifp);
  bits = (dp[8] & 3) << 4 | (dp[20] & 3);
  for (i = 0; i < sizeof table / sizeof *table; i++)
    if (bits == table[i].bits)
    {
      strcpy(make, table[i].t_make);
      strcpy(model, table[i].t_model);
    }
}

/*
   Separates a Minolta DiMAGE Z2 from a Nikon E4300.
 */
int CLASS minolta_z2()
{
  int i, nz;
  char tail[424];

  fseek(ifp, -sizeof tail, SEEK_END);
  fread(tail, 1, sizeof tail, ifp);
  for (nz = i = 0; i < sizeof tail; i++)
    if (tail[i])
      nz++;
  return nz > 20;
}
//@end COMMON

void CLASS jpeg_thumb();

//@out COMMON
void CLASS ppm_thumb()
{
  char *thumb;
  thumb_length = thumb_width * thumb_height * 3;
  thumb = (char *)malloc(thumb_length);
  merror(thumb, "ppm_thumb()");
  fprintf(ofp, "P6\n%d %d\n255\n", thumb_width, thumb_height);
  fread(thumb, 1, thumb_length, ifp);
  fwrite(thumb, 1, thumb_length, ofp);
  free(thumb);
}

void CLASS ppm16_thumb()
{
  int i;
  char *thumb;
  thumb_length = thumb_width * thumb_height * 3;
  thumb = (char *)calloc(thumb_length, 2);
  merror(thumb, "ppm16_thumb()");
  read_shorts((ushort *)thumb, thumb_length);
  for (i = 0; i < thumb_length; i++)
    thumb[i] = ((ushort *)thumb)[i] >> 8;
  fprintf(ofp, "P6\n%d %d\n255\n", thumb_width, thumb_height);
  fwrite(thumb, 1, thumb_length, ofp);
  free(thumb);
}

void CLASS layer_thumb()
{
  int i, c;
  char *thumb, map[][4] = {"012", "102"};

  colors = thumb_misc >> 5 & 7;
  thumb_length = thumb_width * thumb_height;
  thumb = (char *)calloc(colors, thumb_length);
  merror(thumb, "layer_thumb()");
  fprintf(ofp, "P%d\n%d %d\n255\n", 5 + (colors >> 1), thumb_width, thumb_height);
  fread(thumb, thumb_length, colors, ifp);
  for (i = 0; i < thumb_length; i++)
    FORCC putc(thumb[i + thumb_length * (map[thumb_misc >> 8][c] - '0')], ofp);
  free(thumb);
}

void CLASS rollei_thumb()
{
  unsigned i;
  ushort *thumb;

  thumb_length = thumb_width * thumb_height;
  thumb = (ushort *)calloc(thumb_length, 2);
  merror(thumb, "rollei_thumb()");
  fprintf(ofp, "P6\n%d %d\n255\n", thumb_width, thumb_height);
  read_shorts(thumb, thumb_length);
  for (i = 0; i < thumb_length; i++)
  {
    putc(thumb[i] << 3, ofp);
    putc(thumb[i] >> 5 << 2, ofp);
    putc(thumb[i] >> 11 << 3, ofp);
  }
  free(thumb);
}

void CLASS rollei_load_raw()
{
  uchar pixel[10];
  unsigned iten = 0, isix, i, buffer = 0, todo[16];
#ifdef LIBRAW_LIBRARY_BUILD
  if(raw_width > 32767 || raw_height > 32767)
    throw LIBRAW_EXCEPTION_IO_BADFILE;
#endif
  unsigned maxpixel = raw_width*(raw_height+7);

  isix = raw_width * raw_height * 5 / 8;
  while (fread(pixel, 1, 10, ifp) == 10)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    for (i = 0; i < 10; i += 2)
    {
      todo[i] = iten++;
      todo[i + 1] = pixel[i] << 8 | pixel[i + 1];
      buffer = pixel[i] >> 2 | buffer << 6;
    }
    for (; i < 16; i += 2)
    {
      todo[i] = isix++;
      todo[i + 1] = buffer >> (14 - i) * 5;
    }
    for (i = 0; i < 16; i += 2)
      if(todo[i] < maxpixel)
        raw_image[todo[i]] = (todo[i + 1] & 0x3ff);
      else
        derror();
  }
  maximum = 0x3ff;
}

int CLASS raw(unsigned row, unsigned col) { return (row < raw_height && col < raw_width) ? RAW(row, col) : 0; }

void CLASS phase_one_flat_field(int is_float, int nc)
{
  ushort head[8];
  unsigned wide, high, y, x, c, rend, cend, row, col;
  float *mrow, num, mult[4];

  read_shorts(head, 8);
  if (head[2] * head[3] * head[4] * head[5] == 0)
    return;
  wide = head[2] / head[4] + (head[2] % head[4] != 0);
  high = head[3] / head[5] + (head[3] % head[5] != 0);
  mrow = (float *)calloc(nc * wide, sizeof *mrow);
  merror(mrow, "phase_one_flat_field()");
  for (y = 0; y < high; y++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    for (x = 0; x < wide; x++)
      for (c = 0; c < nc; c += 2)
      {
        num = is_float ? getreal(11) : get2() / 32768.0;
        if (y == 0)
          mrow[c * wide + x] = num;
        else
          mrow[(c + 1) * wide + x] = (num - mrow[c * wide + x]) / head[5];
      }
    if (y == 0)
      continue;
    rend = head[1] + y * head[5];
    for (row = rend - head[5]; row < raw_height && row < rend && row < head[1] + head[3] - head[5]; row++)
    {
      for (x = 1; x < wide; x++)
      {
        for (c = 0; c < nc; c += 2)
        {
          mult[c] = mrow[c * wide + x - 1];
          mult[c + 1] = (mrow[c * wide + x] - mult[c]) / head[4];
        }
        cend = head[0] + x * head[4];
        for (col = cend - head[4]; col < raw_width && col < cend && col < head[0] + head[2] - head[4]; col++)
        {
          c = nc > 2 ? FC(row - top_margin, col - left_margin) : 0;
          if (!(c & 1))
          {
            c = RAW(row, col) * mult[c];
            RAW(row, col) = LIM(c, 0, 65535);
          }
          for (c = 0; c < nc; c += 2)
            mult[c] += mult[c + 1];
        }
      }
      for (x = 0; x < wide; x++)
        for (c = 0; c < nc; c += 2)
          mrow[c * wide + x] += mrow[(c + 1) * wide + x];
    }
  }
  free(mrow);
}

int CLASS phase_one_correct()
{
  unsigned entries, tag, data, save, col, row, type;
  int len, i, j, k, cip, val[4], dev[4], sum, max;
  int head[9], diff, mindiff = INT_MAX, off_412 = 0;
  /* static */ const signed char dir[12][2] = {{-1, -1}, {-1, 1}, {1, -1},  {1, 1},  {-2, 0}, {0, -2},
                                               {0, 2},   {2, 0},  {-2, -2}, {-2, 2}, {2, -2}, {2, 2}};
  float poly[8], num, cfrac, frac, mult[2], *yval[2] = {NULL, NULL};
  ushort *xval[2];
  int qmult_applied = 0, qlin_applied = 0;

#ifdef LIBRAW_LIBRARY_BUILD
  if (!meta_length)
#else
  if (half_size || !meta_length)
#endif
    return 0;
#ifdef DCRAW_VERBOSE
  if (verbose)
    fprintf(stderr, _("Phase One correction...\n"));
#endif
  fseek(ifp, meta_offset, SEEK_SET);
  order = get2();
  fseek(ifp, 6, SEEK_CUR);
  fseek(ifp, meta_offset + get4(), SEEK_SET);
  entries = get4();
  get4();

#ifdef LIBRAW_LIBRARY_BUILD
  try
  {
#endif
    while (entries--)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      tag = get4();
      len = get4();
      data = get4();
      save = ftell(ifp);
      fseek(ifp, meta_offset + data, SEEK_SET);
      if (tag == 0x419)
      { /* Polynomial curve */
        for (get4(), i = 0; i < 8; i++)
          poly[i] = getreal(11);
        poly[3] += (ph1.tag_210 - poly[7]) * poly[6] + 1;
        for (i = 0; i < 0x10000; i++)
        {
          num = (poly[5] * i + poly[3]) * i + poly[1];
          curve[i] = LIM(num, 0, 65535);
        }
        goto apply; /* apply to right half */
      }
      else if (tag == 0x41a)
      { /* Polynomial curve */
        for (i = 0; i < 4; i++)
          poly[i] = getreal(11);
        for (i = 0; i < 0x10000; i++)
        {
          for (num = 0, j = 4; j--;)
            num = num * i + poly[j];
          curve[i] = LIM(num + i, 0, 65535);
        }
      apply: /* apply to whole image */
        for (row = 0; row < raw_height; row++)
        {
#ifdef LIBRAW_LIBRARY_BUILD
          checkCancel();
#endif
          for (col = (tag & 1) * ph1.split_col; col < raw_width; col++)
            RAW(row, col) = curve[RAW(row, col)];
        }
      }
      else if (tag == 0x400)
      { /* Sensor defects */
        while ((len -= 8) >= 0)
        {
          col = get2();
          row = get2();
          type = get2();
          get2();
          if (col >= raw_width)
            continue;
          if (type == 131 || type == 137) /* Bad column */
            for (row = 0; row < raw_height; row++)
              if (FC(row - top_margin, col - left_margin) == 1)
              {
                for (sum = i = 0; i < 4; i++)
                  sum += val[i] = raw(row + dir[i][0], col + dir[i][1]);
                for (max = i = 0; i < 4; i++)
                {
                  dev[i] = abs((val[i] << 2) - sum);
                  if (dev[max] < dev[i])
                    max = i;
                }
                RAW(row, col) = (sum - val[max]) / 3.0 + 0.5;
              }
              else
              {
                for (sum = 0, i = 8; i < 12; i++)
                  sum += raw(row + dir[i][0], col + dir[i][1]);
                RAW(row, col) = 0.5 + sum * 0.0732233 + (raw(row, col - 2) + raw(row, col + 2)) * 0.3535534;
              }
          else if (type == 129)
          { /* Bad pixel */
            if (row >= raw_height)
              continue;
            j = (FC(row - top_margin, col - left_margin) != 1) * 4;
            for (sum = 0, i = j; i < j + 8; i++)
              sum += raw(row + dir[i][0], col + dir[i][1]);
            RAW(row, col) = (sum + 4) >> 3;
          }
        }
      }
      else if (tag == 0x401)
      { /* All-color flat fields */
        phase_one_flat_field(1, 2);
      }
      else if (tag == 0x416 || tag == 0x410)
      {
        phase_one_flat_field(0, 2);
      }
      else if (tag == 0x40b)
      { /* Red+blue flat field */
        phase_one_flat_field(0, 4);
      }
      else if (tag == 0x412)
      {
        fseek(ifp, 36, SEEK_CUR);
        diff = abs(get2() - ph1.tag_21a);
        if (mindiff > diff)
        {
          mindiff = diff;
          off_412 = ftell(ifp) - 38;
        }
      }
      else if (tag == 0x41f && !qlin_applied)
      { /* Quadrant linearization */
        ushort lc[2][2][16], ref[16];
        int qr, qc;
        for (qr = 0; qr < 2; qr++)
          for (qc = 0; qc < 2; qc++)
            for (i = 0; i < 16; i++)
              lc[qr][qc][i] = get4();
        for (i = 0; i < 16; i++)
        {
          int v = 0;
          for (qr = 0; qr < 2; qr++)
            for (qc = 0; qc < 2; qc++)
              v += lc[qr][qc][i];
          ref[i] = (v + 2) >> 2;
        }
        for (qr = 0; qr < 2; qr++)
        {
          for (qc = 0; qc < 2; qc++)
          {
            int cx[19], cf[19];
            for (i = 0; i < 16; i++)
            {
              cx[1 + i] = lc[qr][qc][i];
              cf[1 + i] = ref[i];
            }
            cx[0] = cf[0] = 0;
            cx[17] = cf[17] = ((unsigned int)ref[15] * 65535) / lc[qr][qc][15];
            cf[18] = cx[18] = 65535;
            cubic_spline(cx, cf, 19);

            for (row = (qr ? ph1.split_row : 0); row < (qr ? raw_height : ph1.split_row); row++)
            {
#ifdef LIBRAW_LIBRARY_BUILD
              checkCancel();
#endif
              for (col = (qc ? ph1.split_col : 0); col < (qc ? raw_width : ph1.split_col); col++)
                RAW(row, col) = curve[RAW(row, col)];
            }
          }
        }
        qlin_applied = 1;
      }
      else if (tag == 0x41e && !qmult_applied)
      { /* Quadrant multipliers */
        float qmult[2][2] = {{1, 1}, {1, 1}};
        get4();
        get4();
        get4();
        get4();
        qmult[0][0] = 1.0 + getreal(11);
        get4();
        get4();
        get4();
        get4();
        get4();
        qmult[0][1] = 1.0 + getreal(11);
        get4();
        get4();
        get4();
        qmult[1][0] = 1.0 + getreal(11);
        get4();
        get4();
        get4();
        qmult[1][1] = 1.0 + getreal(11);
        for (row = 0; row < raw_height; row++)
        {
#ifdef LIBRAW_LIBRARY_BUILD
          checkCancel();
#endif
          for (col = 0; col < raw_width; col++)
          {
            i = qmult[row >= ph1.split_row][col >= ph1.split_col] * RAW(row, col);
            RAW(row, col) = LIM(i, 0, 65535);
          }
        }
        qmult_applied = 1;
      }
      else if (tag == 0x431 && !qmult_applied)
      { /* Quadrant combined */
        ushort lc[2][2][7], ref[7];
        int qr, qc;
        for (i = 0; i < 7; i++)
          ref[i] = get4();
        for (qr = 0; qr < 2; qr++)
          for (qc = 0; qc < 2; qc++)
            for (i = 0; i < 7; i++)
              lc[qr][qc][i] = get4();
        for (qr = 0; qr < 2; qr++)
        {
          for (qc = 0; qc < 2; qc++)
          {
            int cx[9], cf[9];
            for (i = 0; i < 7; i++)
            {
              cx[1 + i] = ref[i];
              cf[1 + i] = ((unsigned)ref[i] * lc[qr][qc][i]) / 10000;
            }
            cx[0] = cf[0] = 0;
            cx[8] = cf[8] = 65535;
            cubic_spline(cx, cf, 9);
            for (row = (qr ? ph1.split_row : 0); row < (qr ? raw_height : ph1.split_row); row++)
            {
#ifdef LIBRAW_LIBRARY_BUILD
              checkCancel();
#endif
              for (col = (qc ? ph1.split_col : 0); col < (qc ? raw_width : ph1.split_col); col++)
                RAW(row, col) = curve[RAW(row, col)];
            }
          }
        }
        qmult_applied = 1;
        qlin_applied = 1;
      }
      fseek(ifp, save, SEEK_SET);
    }
    if (off_412)
    {
      fseek(ifp, off_412, SEEK_SET);
      for (i = 0; i < 9; i++)
        head[i] = get4() & 0x7fff;
      yval[0] = (float *)calloc(head[1] * head[3] + head[2] * head[4], 6);
      merror(yval[0], "phase_one_correct()");
      yval[1] = (float *)(yval[0] + head[1] * head[3]);
      xval[0] = (ushort *)(yval[1] + head[2] * head[4]);
      xval[1] = (ushort *)(xval[0] + head[1] * head[3]);
      get2();
      for (i = 0; i < 2; i++)
        for (j = 0; j < head[i + 1] * head[i + 3]; j++)
          yval[i][j] = getreal(11);
      for (i = 0; i < 2; i++)
        for (j = 0; j < head[i + 1] * head[i + 3]; j++)
          xval[i][j] = get2();
      for (row = 0; row < raw_height; row++)
      {
#ifdef LIBRAW_LIBRARY_BUILD
        checkCancel();
#endif
        for (col = 0; col < raw_width; col++)
        {
          cfrac = (float)col * head[3] / raw_width;
          cfrac -= cip = cfrac;
          num = RAW(row, col) * 0.5;
          for (i = cip; i < cip + 2; i++)
          {
            for (k = j = 0; j < head[1]; j++)
              if (num < xval[0][k = head[1] * i + j])
                break;
            frac = (j == 0 || j == head[1]) ? 0 : (xval[0][k] - num) / (xval[0][k] - xval[0][k - 1]);
            mult[i - cip] = yval[0][k - 1] * frac + yval[0][k] * (1 - frac);
          }
          i = ((mult[0] * (1 - cfrac) + mult[1] * cfrac) * row + num) * 2;
          RAW(row, col) = LIM(i, 0, 65535);
        }
      }
      free(yval[0]);
    }
#ifdef LIBRAW_LIBRARY_BUILD
  }
  catch (...)
  {
    if (yval[0])
      free(yval[0]);
    return LIBRAW_CANCELLED_BY_CALLBACK;
  }
#endif
  return 0;
}

void CLASS phase_one_load_raw()
{
  int a, b, i;
  ushort akey, bkey, t_mask;

  fseek(ifp, ph1.key_off, SEEK_SET);
  akey = get2();
  bkey = get2();
  t_mask = ph1.format == 1 ? 0x5555 : 0x1354;
#ifdef LIBRAW_LIBRARY_BUILD
  if (ph1.black_col || ph1.black_row)
  {
    imgdata.rawdata.ph1_cblack = (short(*)[2])calloc(raw_height * 2, sizeof(ushort));
    merror(imgdata.rawdata.ph1_cblack, "phase_one_load_raw()");
    imgdata.rawdata.ph1_rblack = (short(*)[2])calloc(raw_width * 2, sizeof(ushort));
    merror(imgdata.rawdata.ph1_rblack, "phase_one_load_raw()");
    if (ph1.black_col)
    {
      fseek(ifp, ph1.black_col, SEEK_SET);
      read_shorts((ushort *)imgdata.rawdata.ph1_cblack[0], raw_height * 2);
    }
    if (ph1.black_row)
    {
      fseek(ifp, ph1.black_row, SEEK_SET);
      read_shorts((ushort *)imgdata.rawdata.ph1_rblack[0], raw_width * 2);
    }
  }
#endif
  fseek(ifp, data_offset, SEEK_SET);
  read_shorts(raw_image, raw_width * raw_height);
  if (ph1.format)
    for (i = 0; i < raw_width * raw_height; i += 2)
    {
      a = raw_image[i + 0] ^ akey;
      b = raw_image[i + 1] ^ bkey;
      raw_image[i + 0] = (a & t_mask) | (b & ~t_mask);
      raw_image[i + 1] = (b & t_mask) | (a & ~t_mask);
    }
}

unsigned CLASS ph1_bithuff(int nbits, ushort *huff)
{
#ifndef LIBRAW_NOTHREADS
#define bitbuf tls->ph1_bits.bitbuf
#define vbits tls->ph1_bits.vbits
#else
  static UINT64 bitbuf = 0;
  static int vbits = 0;
#endif
  unsigned c;

  if (nbits == -1)
    return bitbuf = vbits = 0;
  if (nbits == 0)
    return 0;
  if (vbits < nbits)
  {
    bitbuf = bitbuf << 32 | get4();
    vbits += 32;
  }
  c = bitbuf << (64 - vbits) >> (64 - nbits);
  if (huff)
  {
    vbits -= huff[c] >> 8;
    return (uchar)huff[c];
  }
  vbits -= nbits;
  return c;
#ifndef LIBRAW_NOTHREADS
#undef bitbuf
#undef vbits
#endif
}
#define ph1_bits(n) ph1_bithuff(n, 0)
#define ph1_huff(h) ph1_bithuff(*h, h + 1)

void CLASS phase_one_load_raw_c()
{
  static const int length[] = {8, 7, 6, 9, 11, 10, 5, 12, 14, 13};
  int *offset, len[2], pred[2], row, col, i, j;
  ushort *pixel;
  short(*c_black)[2], (*r_black)[2];
#ifdef LIBRAW_LIBRARY_BUILD
  if (ph1.format == 6)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
#endif

  pixel = (ushort *)calloc(raw_width * 3 + raw_height * 4, 2);
  merror(pixel, "phase_one_load_raw_c()");
  offset = (int *)(pixel + raw_width);
  fseek(ifp, strip_offset, SEEK_SET);
  for (row = 0; row < raw_height; row++)
    offset[row] = get4();
  c_black = (short(*)[2])(offset + raw_height);
  fseek(ifp, ph1.black_col, SEEK_SET);
  if (ph1.black_col)
    read_shorts((ushort *)c_black[0], raw_height * 2);
  r_black = c_black + raw_height;
  fseek(ifp, ph1.black_row, SEEK_SET);
  if (ph1.black_row)
    read_shorts((ushort *)r_black[0], raw_width * 2);

#ifdef LIBRAW_LIBRARY_BUILD
  // Copy data to internal copy (ever if not read)
  if (ph1.black_col || ph1.black_row)
  {
    imgdata.rawdata.ph1_cblack = (short(*)[2])calloc(raw_height * 2, sizeof(ushort));
    merror(imgdata.rawdata.ph1_cblack, "phase_one_load_raw_c()");
    memmove(imgdata.rawdata.ph1_cblack, (ushort *)c_black[0], raw_height * 2 * sizeof(ushort));
    imgdata.rawdata.ph1_rblack = (short(*)[2])calloc(raw_width * 2, sizeof(ushort));
    merror(imgdata.rawdata.ph1_rblack, "phase_one_load_raw_c()");
    memmove(imgdata.rawdata.ph1_rblack, (ushort *)r_black[0], raw_width * 2 * sizeof(ushort));
  }
#endif

  for (i = 0; i < 256; i++)
    curve[i] = i * i / 3.969 + 0.5;
#ifdef LIBRAW_LIBRARY_BUILD
  try
  {
#endif
    for (row = 0; row < raw_height; row++)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      fseek(ifp, data_offset + offset[row], SEEK_SET);
      ph1_bits(-1);
      pred[0] = pred[1] = 0;
      for (col = 0; col < raw_width; col++)
      {
        if (col >= (raw_width & -8))
          len[0] = len[1] = 14;
        else if ((col & 7) == 0)
          for (i = 0; i < 2; i++)
          {
            for (j = 0; j < 5 && !ph1_bits(1); j++)
              ;
            if (j--)
              len[i] = length[j * 2 + ph1_bits(1)];
          }
        if ((i = len[col & 1]) == 14)
          pixel[col] = pred[col & 1] = ph1_bits(16);
        else
          pixel[col] = pred[col & 1] += ph1_bits(i) + 1 - (1 << (i - 1));
        if (pred[col & 1] >> 16)
          derror();
        if (ph1.format == 5 && pixel[col] < 256)
          pixel[col] = curve[pixel[col]];
      }
#ifndef LIBRAW_LIBRARY_BUILD
      for (col = 0; col < raw_width; col++)
      {
        int shift = ph1.format == 8 ? 0 : 2;
        i = (pixel[col] << shift) - ph1.t_black + c_black[row][col >= ph1.split_col] +
            r_black[col][row >= ph1.split_row];
        if (i > 0)
          RAW(row, col) = i;
      }
#else
    if (ph1.format == 8)
      memmove(&RAW(row, 0), &pixel[0], raw_width * 2);
    else
      for (col = 0; col < raw_width; col++)
        RAW(row, col) = pixel[col] << 2;
#endif
    }
#ifdef LIBRAW_LIBRARY_BUILD
  }
  catch (...)
  {
    free(pixel);
    throw;
  }
#endif
  free(pixel);
  maximum = 0xfffc - ph1.t_black;
}

void CLASS hasselblad_load_raw()
{
  struct jhead jh;
  int shot, row, col, *back[5], len[2], diff[12], pred, sh, f, s, c;
  unsigned upix, urow, ucol;
  ushort *ip;

  if (!ljpeg_start(&jh, 0))
    return;
  order = 0x4949;
  ph1_bits(-1);
#ifdef LIBRAW_LIBRARY_BUILD
  try
  {
#endif
    back[4] = (int *)calloc(raw_width, 3 * sizeof **back);
    merror(back[4], "hasselblad_load_raw()");
    FORC3 back[c] = back[4] + c * raw_width;
    cblack[6] >>= sh = tiff_samples > 1;
    shot = LIM(shot_select, 1, tiff_samples) - 1;
    for (row = 0; row < raw_height; row++)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      FORC4 back[(c + 3) & 3] = back[c];
      for (col = 0; col < raw_width; col += 2)
      {
        for (s = 0; s < tiff_samples * 2; s += 2)
        {
          FORC(2) len[c] = ph1_huff(jh.huff[0]);
          FORC(2)
          {
            diff[s + c] = ph1_bits(len[c]);
            if ((diff[s + c] & (1 << (len[c] - 1))) == 0)
              diff[s + c] -= (1 << len[c]) - 1;
            if (diff[s + c] == 65535)
              diff[s + c] = -32768;
          }
        }
        for (s = col; s < col + 2; s++)
        {
          pred = 0x8000 + load_flags;
          if (col)
            pred = back[2][s - 2];
          if (col && row > 1)
            switch (jh.psv)
            {
            case 11:
              pred += back[0][s] / 2 - back[0][s - 2] / 2;
              break;
            }
          f = (row & 1) * 3 ^ ((col + s) & 1);
          FORC(tiff_samples)
          {
            pred += diff[(s & 1) * tiff_samples + c];
            upix = pred >> sh & 0xffff;
            if (raw_image && c == shot)
              RAW(row, s) = upix;
            if (image)
            {
              urow = row - top_margin + (c & 1);
              ucol = col - left_margin - ((c >> 1) & 1);
              ip = &image[urow * width + ucol][f];
              if (urow < height && ucol < width)
                *ip = c < 4 ? upix : (*ip + upix) >> 1;
            }
          }
          back[2][s] = pred;
        }
      }
    }
#ifdef LIBRAW_LIBRARY_BUILD
  }
  catch (...)
  {
    free(back[4]);
    ljpeg_end(&jh);
    throw;
  }
#endif
  free(back[4]);
  ljpeg_end(&jh);
  if (image)
    mix_green = 1;
}

void CLASS leaf_hdr_load_raw()
{
  ushort *pixel = 0;
  unsigned tile = 0, r, c, row, col;

  if (!filters || !raw_image)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    if(!image)
      throw LIBRAW_EXCEPTION_IO_CORRUPT;
#endif
    pixel = (ushort *)calloc(raw_width, sizeof *pixel);
    merror(pixel, "leaf_hdr_load_raw()");
  }
#ifdef LIBRAW_LIBRARY_BUILD
  try
  {
#endif
    FORC(tiff_samples)
    for (r = 0; r < raw_height; r++)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      if (r % tile_length == 0)
      {
        fseek(ifp, data_offset + 4 * tile++, SEEK_SET);
        fseek(ifp, get4(), SEEK_SET);
      }
      if (filters && c != shot_select)
        continue;
      if (filters && raw_image)
        pixel = raw_image + r * raw_width;
      read_shorts(pixel, raw_width);
      if (!filters && image && (row = r - top_margin) < height)
        for (col = 0; col < width; col++)
          image[row * width + col][c] = pixel[col + left_margin];
    }
#ifdef LIBRAW_LIBRARY_BUILD
  }
  catch (...)
  {
    if (!filters)
      free(pixel);
    throw;
  }
#endif
  if (!filters)
  {
    maximum = 0xffff;
    raw_color = 1;
    free(pixel);
  }
}

void CLASS unpacked_load_raw()
{
  int row, col, bits = 0;
  while (1 << ++bits < maximum)
    ;
  read_shorts(raw_image, raw_width * raw_height);
  fseek(ifp,-2,SEEK_CUR); // avoid EOF error
  if (maximum < 0xffff || load_flags)
    for (row = 0; row < raw_height; row++)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      for (col = 0; col < raw_width; col++)
        if ((RAW(row, col) >>= load_flags) >> bits && (unsigned)(row - top_margin) < height &&
            (unsigned)(col - left_margin) < width)
          derror();
    }
}

void CLASS unpacked_load_raw_FujiDBP()
/*
for Fuji DBP for GX680, aka DX-2000
  DBP_tile_width = 688;
  DBP_tile_height = 3856;
  DBP_n_tiles = 8;
*/
{
  int scan_line, tile_n;
  int nTiles;

  nTiles = 8;
  tile_width = raw_width / nTiles;

  ushort *tile;
  tile = (ushort *)calloc(raw_height, tile_width * 2);

  for (tile_n = 0; tile_n < nTiles; tile_n++)
  {
    read_shorts(tile, tile_width * raw_height);
    for (scan_line = 0; scan_line < raw_height; scan_line++)
    {
      memcpy (&raw_image[scan_line * raw_width + tile_n * tile_width], &tile[scan_line * tile_width], tile_width * 2);
    }
  }
  free (tile);
  fseek(ifp,-2,SEEK_CUR); // avoid EOF error
}

void CLASS unpacked_load_raw_reversed()
{
  int row, col, bits = 0;
  while (1 << ++bits < maximum)
    ;
  for (row = raw_height - 1; row >= 0; row--)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    read_shorts(&raw_image[row * raw_width], raw_width);
    for (col = 0; col < raw_width; col++)
      if ((RAW(row, col) >>= load_flags) >> bits && (unsigned)(row - top_margin) < height &&
          (unsigned)(col - left_margin) < width)
        derror();
  }
}

void CLASS sinar_4shot_load_raw()
{
  ushort *pixel;
  unsigned shot, row, col, r, c;

  if (raw_image)
  {
    shot = LIM(shot_select, 1, 4) - 1;
    fseek(ifp, data_offset + shot * 4, SEEK_SET);
    fseek(ifp, get4(), SEEK_SET);
    unpacked_load_raw();
    return;
  }
#ifdef LIBRAW_LIBRARY_BUILD
  if (!image)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
#endif
  pixel = (ushort *)calloc(raw_width, sizeof *pixel);
  merror(pixel, "sinar_4shot_load_raw()");
#ifdef LIBRAW_LIBRARY_BUILD
  try
  {
#endif
    for (shot = 0; shot < 4; shot++)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      fseek(ifp, data_offset + shot * 4, SEEK_SET);
      fseek(ifp, get4(), SEEK_SET);
      for (row = 0; row < raw_height; row++)
      {
        read_shorts(pixel, raw_width);
        if ((r = row - top_margin - (shot >> 1 & 1)) >= height)
          continue;
        for (col = 0; col < raw_width; col++)
        {
          if ((c = col - left_margin - (shot & 1)) >= width)
            continue;
          image[r * width + c][(row & 1) * 3 ^ (~col & 1)] = pixel[col];
        }
      }
    }
#ifdef LIBRAW_LIBRARY_BUILD
  }
  catch (...)
  {
    free(pixel);
    throw;
  }
#endif
  free(pixel);
  mix_green = 1;
}

void CLASS imacon_full_load_raw()
{
#ifdef LIBRAW_LIBRARY_BUILD
  if (!image)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
#endif
  int row, col;

#ifdef LIBRAW_LIBRARY_BUILD
  unsigned short *buf = (unsigned short *)malloc(width * 3 * sizeof(unsigned short));
  merror(buf, "imacon_full_load_raw");
#endif

  for (row = 0; row < height; row++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
    read_shorts(buf, width * 3);
    unsigned short(*rowp)[4] = &image[row * width];
    for (col = 0; col < width; col++)
    {
      rowp[col][0] = buf[col * 3];
      rowp[col][1] = buf[col * 3 + 1];
      rowp[col][2] = buf[col * 3 + 2];
      rowp[col][3] = 0;
    }
#else
    for (col = 0; col < width; col++)
      read_shorts(image[row * width + col], 3);
#endif
  }
#ifdef LIBRAW_LIBRARY_BUILD
  free(buf);
#endif
}

void CLASS packed_load_raw()
{
  int vbits = 0, bwide, rbits, bite, half, irow, row, col, val, i;
  UINT64 bitbuf = 0;

  bwide = raw_width * tiff_bps / 8;
  bwide += bwide & load_flags >> 7;
  rbits = bwide * 8 - raw_width * tiff_bps;
  if (load_flags & 1)
    bwide = bwide * 16 / 15;
  bite = 8 + (load_flags & 24);
  half = (raw_height + 1) >> 1;
  for (irow = 0; irow < raw_height; irow++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    row = irow;
    if (load_flags & 2 && (row = irow % half * 2 + irow / half) == 1 && load_flags & 4)
    {
      if (vbits = 0, tiff_compress)
        fseek(ifp, data_offset - (-half * bwide & -2048), SEEK_SET);
      else
      {
        fseek(ifp, 0, SEEK_END);
        fseek(ifp, ftell(ifp) >> 3 << 2, SEEK_SET);
      }
    }
#ifdef LIBRAW_LIBRARY_BUILD
    if(feof(ifp)) throw LIBRAW_EXCEPTION_IO_EOF;
#endif
    for (col = 0; col < raw_width; col++)
    {
      for (vbits -= tiff_bps; vbits < 0; vbits += bite)
      {
        bitbuf <<= bite;
        for (i = 0; i < bite; i += 8)
          bitbuf |= (unsigned)(fgetc(ifp) << i);
      }
      val = bitbuf << (64 - tiff_bps - vbits) >> (64 - tiff_bps);
      RAW(row, col ^ (load_flags >> 6 & 1)) = val;
      if (load_flags & 1 && (col % 10) == 9 && fgetc(ifp) && row < height + top_margin && col < width + left_margin)
        derror();
    }
    vbits -= rbits;
  }
}

#ifdef LIBRAW_LIBRARY_BUILD

ushort raw_stride;

void CLASS parse_broadcom()
{

  /* This structure is at offset 0xb0 from the 'BRCM' ident. */
  struct
  {
    uint8_t umode[32];
    uint16_t uwidth;
    uint16_t uheight;
    uint16_t padding_right;
    uint16_t padding_down;
    uint32_t unknown_block[6];
    uint16_t transform;
    uint16_t format;
    uint8_t bayer_order;
    uint8_t bayer_format;
  } header;

  header.bayer_order = 0;
  fseek(ifp, 0xb0 - 0x20, SEEK_CUR);
  fread(&header, 1, sizeof(header), ifp);
  raw_stride = ((((((header.uwidth + header.padding_right) * 5) + 3) >> 2) + 0x1f) & (~0x1f));
  raw_width = width = header.uwidth;
  raw_height = height = header.uheight;
  filters = 0x16161616; /* default Bayer order is 2, BGGR */

  switch (header.bayer_order)
  {
  case 0: /* RGGB */
    filters = 0x94949494;
    break;
  case 1: /* GBRG */
    filters = 0x49494949;
    break;
  case 3: /* GRBG */
    filters = 0x61616161;
    break;
  }
}

void CLASS broadcom_load_raw()
{

  uchar *data, *dp;
  int rev, row, col, c;

  rev = 3 * (order == 0x4949);
  data = (uchar *)malloc(raw_stride * 2);
  merror(data, "broadcom_load_raw()");

  for (row = 0; row < raw_height; row++)
  {
    if (fread(data + raw_stride, 1, raw_stride, ifp) < raw_stride)
      derror();
    FORC(raw_stride) data[c] = data[raw_stride + (c ^ rev)];
    for (dp = data, col = 0; col < raw_width; dp += 5, col += 4)
      FORC4 RAW(row, col + c) = (dp[c] << 2) | (dp[4] >> (c << 1) & 3);
  }
  free(data);
}
#endif

void CLASS nokia_load_raw()
{
  uchar *data, *dp;
  int rev, dwide, row, col, c;
  double sum[] = {0, 0};

  rev = 3 * (order == 0x4949);
  dwide = (raw_width * 5 + 1) / 4;
  data = (uchar *)malloc(dwide * 2);
  merror(data, "nokia_load_raw()");
#ifdef LIBRAW_LIBRARY_BUILD
  try
  {
#endif
    for (row = 0; row < raw_height; row++)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      if (fread(data + dwide, 1, dwide, ifp) < dwide)
        derror();
      FORC(dwide) data[c] = data[dwide + (c ^ rev)];
      for (dp = data, col = 0; col < raw_width; dp += 5, col += 4)
        FORC4 RAW(row, col + c) = (dp[c] << 2) | (dp[4] >> (c << 1) & 3);
    }
#ifdef LIBRAW_LIBRARY_BUILD
  }
  catch (...)
  {
    free(data);
    throw;
  }
#endif
  free(data);
  maximum = 0x3ff;
  if (strncmp(make, "OmniVision", 10))
    return;
  row = raw_height / 2;
  FORC(width - 1)
  {
    sum[c & 1] += SQR(RAW(row, c) - RAW(row + 1, c + 1));
    sum[~c & 1] += SQR(RAW(row + 1, c) - RAW(row, c + 1));
  }
  if (sum[1] > sum[0])
    filters = 0x4b4b4b4b;
}

void CLASS android_tight_load_raw()
{
  uchar *data, *dp;
  int bwide, row, col, c;

  bwide = -(-5 * raw_width >> 5) << 3;
  data = (uchar *)malloc(bwide);
  merror(data, "android_tight_load_raw()");
  for (row = 0; row < raw_height; row++)
  {
    if (fread(data, 1, bwide, ifp) < bwide)
      derror();
    for (dp = data, col = 0; col < raw_width; dp += 5, col += 4)
      FORC4 RAW(row, col + c) = (dp[c] << 2) | (dp[4] >> (c << 1) & 3);
  }
  free(data);
}

void CLASS android_loose_load_raw()
{
  uchar *data, *dp;
  int bwide, row, col, c;
  UINT64 bitbuf = 0;

  bwide = (raw_width + 5) / 6 << 3;
  data = (uchar *)malloc(bwide);
  merror(data, "android_loose_load_raw()");
  for (row = 0; row < raw_height; row++)
  {
    if (fread(data, 1, bwide, ifp) < bwide)
      derror();
    for (dp = data, col = 0; col < raw_width; dp += 8, col += 6)
    {
      FORC(8) bitbuf = (bitbuf << 8) | dp[c ^ 7];
      FORC(6) RAW(row, col + c) = (bitbuf >> c * 10) & 0x3ff;
    }
  }
  free(data);
}

void CLASS canon_rmf_load_raw()
{
  int row, col, bits, orow, ocol, c;

#ifdef LIBRAW_LIBRARY_BUILD
  int *words = (int *)malloc(sizeof(int) * (raw_width / 3 + 1));
  merror(words, "canon_rmf_load_raw");
#endif
  for (row = 0; row < raw_height; row++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
    fread(words, sizeof(int), raw_width / 3, ifp);
    for (col = 0; col < raw_width - 2; col += 3)
    {
      bits = words[col / 3];
      FORC3
      {
        orow = row;
        if ((ocol = col + c - 4) < 0)
        {
          ocol += raw_width;
          if ((orow -= 2) < 0)
            orow += raw_height;
        }
        RAW(orow, ocol) = curve[bits >> (10 * c + 2) & 0x3ff];
      }
    }
#else
    for (col = 0; col < raw_width - 2; col += 3)
    {
      bits = get4();
      FORC3
      {
        orow = row;
        if ((ocol = col + c - 4) < 0)
        {
          ocol += raw_width;
          if ((orow -= 2) < 0)
            orow += raw_height;
        }
        RAW(orow, ocol) = curve[bits >> (10 * c + 2) & 0x3ff];
      }
    }
#endif
  }
#ifdef LIBRAW_LIBRARY_BUILD
  free(words);
#endif
  maximum = curve[0x3ff];
}

unsigned CLASS pana_data(int nb, unsigned *bytes)
{
#ifndef LIBRAW_NOTHREADS
#define vpos tls->pana_data.vpos
#define buf tls->pana_data.buf
#else
  static uchar buf[0x4002];
  static int vpos;
#endif
  int byte;

  if (!nb && !bytes)
    return vpos = 0;

  if (!vpos)
  {
    fread(buf + load_flags, 1, 0x4000 - load_flags, ifp);
    fread(buf, 1, load_flags, ifp);
  }

#ifdef LIBRAW_LIBRARY_BUILD /* not part of std. dcraw */
  if (pana_encoding == 5)
  {
    for (byte = 0; byte < 16; byte++)
    {
      bytes[byte] = buf[vpos++];
      vpos &= 0x3FFF;
    }
  }
  else
#endif
  {
    vpos = (vpos - nb) & 0x1ffff;
    byte = vpos >> 3 ^ 0x3ff0;
    return (buf[byte] | buf[byte + 1] << 8) >> (vpos & 7) & ~((~0u) << nb);
  }
  return 0;
#ifndef LIBRAW_NOTHREADS
#undef vpos
#undef buf
#endif
}

void CLASS panasonic_load_raw()
{
  int row, col, i, j, sh = 0, pred[2], nonz[2];
  unsigned bytes[16];
  ushort *raw_block_data;

  pana_data(0, 0);

#ifdef LIBRAW_LIBRARY_BUILD
  int enc_blck_size = pana_bpp == 12 ? 10 : 9;
  if (pana_encoding == 5)
  {
    for (row = 0; row < raw_height; row++)
    {
      raw_block_data = raw_image + row * raw_width;

      checkCancel();
      for (col = 0; col < raw_width; col += enc_blck_size)
      {
        pana_data(0, bytes);

        if (pana_bpp == 12)
        {
          raw_block_data[col] = ((bytes[1] & 0xF) << 8) + bytes[0];
          raw_block_data[col + 1] = 16 * bytes[2] + (bytes[1] >> 4);
          raw_block_data[col + 2] = ((bytes[4] & 0xF) << 8) + bytes[3];
          raw_block_data[col + 3] = 16 * bytes[5] + (bytes[4] >> 4);
          raw_block_data[col + 4] = ((bytes[7] & 0xF) << 8) + bytes[6];
          raw_block_data[col + 5] = 16 * bytes[8] + (bytes[7] >> 4);
          raw_block_data[col + 6] = ((bytes[10] & 0xF) << 8) + bytes[9];
          raw_block_data[col + 7] = 16 * bytes[11] + (bytes[10] >> 4);
          raw_block_data[col + 8] = ((bytes[13] & 0xF) << 8) + bytes[12];
          raw_block_data[col + 9] = 16 * bytes[14] + (bytes[13] >> 4);
        }
        else if (pana_bpp == 14)
        {
          raw_block_data[col] = bytes[0] + ((bytes[1] & 0x3F) << 8);
          raw_block_data[col + 1] = (bytes[1] >> 6) + 4 * (bytes[2]) +
                                    ((bytes[3] & 0xF) << 10);
          raw_block_data[col + 2] = (bytes[3] >> 4) + 16 * (bytes[4]) +
                                    ((bytes[5] & 3) << 12);
          raw_block_data[col + 3] = ((bytes[5] & 0xFC) >> 2) + (bytes[6] << 6);
          raw_block_data[col + 4] = bytes[7] + ((bytes[8] & 0x3F) << 8);
          raw_block_data[col + 5] = (bytes[8] >> 6) + 4 * bytes[9] + ((bytes[10] & 0xF) << 10);
          raw_block_data[col + 6] = (bytes[10] >> 4) + 16 * bytes[11] + ((bytes[12] & 3) << 12);
          raw_block_data[col + 7] = ((bytes[12] & 0xFC) >> 2) + (bytes[13] << 6);
          raw_block_data[col + 8] = bytes[14] + ((bytes[15] & 0x3F) << 8);
        }
      }
    }
  }
  else
#endif
  {
    for (row = 0; row < raw_height; row++)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      for (col = 0; col < raw_width; col++)
      {
        if ((i = col % 14) == 0)
          pred[0] = pred[1] = nonz[0] = nonz[1] = 0;
        if (i % 3 == 2)
          sh = 4 >> (3 - pana_data(2, 0));
        if (nonz[i & 1])
        {
          if ((j = pana_data(8, 0)))
          {
            if ((pred[i & 1] -= 0x80 << sh) < 0 || sh == 4)
              pred[i & 1] &= ~((~0u) << sh);
            pred[i & 1] += j << sh;
          }
        }
        else if ((nonz[i & 1] = pana_data(8, 0)) || i > 11)
          pred[i & 1] = nonz[i & 1] << 4 | pana_data(4, 0);
        if ((RAW(row, col) = pred[col & 1]) > 4098 && col < width && row < height)
          derror();
      }
    }
  }
}


void CLASS olympus_load_raw()
{
  ushort huff[4096];
  int row, col, nbits, sign, low, high, i, c, w, n, nw;
  int acarry[2][3], *carry, pred, diff;

  huff[n = 0] = 0xc0c;
  for (i = 12; i--;)
    FORC(2048 >> i) huff[++n] = (i + 1) << 8 | i;
  fseek(ifp, 7, SEEK_CUR);
  getbits(-1);
  for (row = 0; row < height; row++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    memset(acarry, 0, sizeof acarry);
    for (col = 0; col < raw_width; col++)
    {
      carry = acarry[col & 1];
      i = 2 * (carry[2] < 3);
      for (nbits = 2 + i; (ushort)carry[0] >> (nbits + i); nbits++)
        ;
      low = (sign = getbits(3)) & 3;
      sign = sign << 29 >> 31;
      if ((high = getbithuff(12, huff)) == 12)
        high = getbits(16 - nbits) >> 1;
      carry[0] = (high << nbits) | getbits(nbits);
      diff = (carry[0] ^ sign) + carry[1];
      carry[1] = (diff * 3 + carry[1]) >> 5;
      carry[2] = carry[0] > 16 ? 0 : carry[2] + 1;
      if (col >= width)
        continue;
      if (row < 2 && col < 2)
        pred = 0;
      else if (row < 2)
        pred = RAW(row, col - 2);
      else if (col < 2)
        pred = RAW(row - 2, col);
      else
      {
        w = RAW(row, col - 2);
        n = RAW(row - 2, col);
        nw = RAW(row - 2, col - 2);
        if ((w < nw && nw < n) || (n < nw && nw < w))
        {
          if (ABS(w - nw) > 32 || ABS(n - nw) > 32)
            pred = w + n - nw;
          else
            pred = (w + n) >> 1;
        }
        else
          pred = ABS(w - nw) > ABS(n - nw) ? w : n;
      }
      if ((RAW(row, col) = pred + ((diff << 2) | low)) >> 12)
        derror();
    }
  }
}

void CLASS minolta_rd175_load_raw()
{
  uchar pixel[768];
  unsigned irow, box, row, col;

  for (irow = 0; irow < 1481; irow++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    if (fread(pixel, 1, 768, ifp) < 768)
      derror();
    box = irow / 82;
    row = irow % 82 * 12 + ((box < 12) ? box | 1 : (box - 12) * 2);
    switch (irow)
    {
    case 1477:
    case 1479:
      continue;
    case 1476:
      row = 984;
      break;
    case 1480:
      row = 985;
      break;
    case 1478:
      row = 985;
      box = 1;
    }
    if ((box < 12) && (box & 1))
    {
      for (col = 0; col < 1533; col++, row ^= 1)
        if (col != 1)
          RAW(row, col) = (col + 1) & 2 ? pixel[col / 2 - 1] + pixel[col / 2 + 1] : pixel[col / 2] << 1;
      RAW(row, 1) = pixel[1] << 1;
      RAW(row, 1533) = pixel[765] << 1;
    }
    else
      for (col = row & 1; col < 1534; col += 2)
        RAW(row, col) = pixel[col / 2] << 1;
  }
  maximum = 0xff << 1;
}

void CLASS quicktake_100_load_raw()
{
  uchar pixel[484][644];
  static const short gstep[16] = {-89, -60, -44, -32, -22, -15, -8, -2, 2, 8, 15, 22, 32, 44, 60, 89};
  static const short rstep[6][4] = {{-3, -1, 1, 3},   {-5, -1, 1, 5},   {-8, -2, 2, 8},
                                    {-13, -3, 3, 13}, {-19, -4, 4, 19}, {-28, -6, 6, 28}};
  static const short t_curve[256] = {
      0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   11,  12,   13,   14,  15,  16,  17,  18,  19,  20,  21,  22,
      23,  24,  25,  26,  27,  28,  29,  30,  32,  33,  34,  35,   36,   37,  38,  39,  40,  41,  42,  43,  44,  45,
      46,  47,  48,  49,  50,  51,  53,  54,  55,  56,  57,  58,   59,   60,  61,  62,  63,  64,  65,  66,  67,  68,
      69,  70,  71,  72,  74,  75,  76,  77,  78,  79,  80,  81,   82,   83,  84,  86,  88,  90,  92,  94,  97,  99,
      101, 103, 105, 107, 110, 112, 114, 116, 118, 120, 123, 125,  127,  129, 131, 134, 136, 138, 140, 142, 144, 147,
      149, 151, 153, 155, 158, 160, 162, 164, 166, 168, 171, 173,  175,  177, 179, 181, 184, 186, 188, 190, 192, 195,
      197, 199, 201, 203, 205, 208, 210, 212, 214, 216, 218, 221,  223,  226, 230, 235, 239, 244, 248, 252, 257, 261,
      265, 270, 274, 278, 283, 287, 291, 296, 300, 305, 309, 313,  318,  322, 326, 331, 335, 339, 344, 348, 352, 357,
      361, 365, 370, 374, 379, 383, 387, 392, 396, 400, 405, 409,  413,  418, 422, 426, 431, 435, 440, 444, 448, 453,
      457, 461, 466, 470, 474, 479, 483, 487, 492, 496, 500, 508,  519,  531, 542, 553, 564, 575, 587, 598, 609, 620,
      631, 643, 654, 665, 676, 687, 698, 710, 721, 732, 743, 754,  766,  777, 788, 799, 810, 822, 833, 844, 855, 866,
      878, 889, 900, 911, 922, 933, 945, 956, 967, 978, 989, 1001, 1012, 1023};
  int rb, row, col, sharp, val = 0;
#ifdef LIBRAW_LIBRARY_BUILD
  if(width>640 || height > 480)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
#endif

  getbits(-1);
  memset(pixel, 0x80, sizeof pixel);
  for (row = 2; row < height + 2; row++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    for (col = 2 + (row & 1); col < width + 2; col += 2)
    {
      val = ((pixel[row - 1][col - 1] + 2 * pixel[row - 1][col + 1] + pixel[row][col - 2]) >> 2) + gstep[getbits(4)];
      pixel[row][col] = val = LIM(val, 0, 255);
      if (col < 4)
        pixel[row][col - 2] = pixel[row + 1][~row & 1] = val;
      if (row == 2)
        pixel[row - 1][col + 1] = pixel[row - 1][col + 3] = val;
    }
    pixel[row][col] = val;
  }
  for (rb = 0; rb < 2; rb++)
    for (row = 2 + rb; row < height + 2; row += 2)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      for (col = 3 - (row & 1); col < width + 2; col += 2)
      {
        if (row < 4 || col < 4)
          sharp = 2;
        else
        {
          val = ABS(pixel[row - 2][col] - pixel[row][col - 2]) + ABS(pixel[row - 2][col] - pixel[row - 2][col - 2]) +
                ABS(pixel[row][col - 2] - pixel[row - 2][col - 2]);
          sharp = val < 4 ? 0 : val < 8 ? 1 : val < 16 ? 2 : val < 32 ? 3 : val < 48 ? 4 : 5;
        }
        val = ((pixel[row - 2][col] + pixel[row][col - 2]) >> 1) + rstep[sharp][getbits(2)];
        pixel[row][col] = val = LIM(val, 0, 255);
        if (row < 4)
          pixel[row - 2][col + 2] = val;
        if (col < 4)
          pixel[row + 2][col - 2] = val;
      }
    }
  for (row = 2; row < height + 2; row++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    for (col = 3 - (row & 1); col < width + 2; col += 2)
    {
      val = ((pixel[row][col - 1] + (pixel[row][col] << 2) + pixel[row][col + 1]) >> 1) - 0x100;
      pixel[row][col] = LIM(val, 0, 255);
    }
  }
  for (row = 0; row < height; row++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    for (col = 0; col < width; col++)
      RAW(row, col) = t_curve[pixel[row + 2][col + 2]];
  }
  maximum = 0x3ff;
}

#define radc_token(tree) ((signed char)getbithuff(8, huff[tree]))

#define FORYX                                                                                                          \
  for (y = 1; y < 3; y++)                                                                                              \
    for (x = col + 1; x >= col; x--)

#define PREDICTOR                                                                                                      \
  (c ? (buf[c][y - 1][x] + buf[c][y][x + 1]) / 2 : (buf[c][y - 1][x + 1] + 2 * buf[c][y - 1][x] + buf[c][y][x + 1]) / 4)

#ifdef __GNUC__
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)
#pragma GCC optimize("no-aggressive-loop-optimizations")
#endif
#endif

void CLASS kodak_radc_load_raw()
{
#ifdef LIBRAW_LIBRARY_BUILD
  // All kodak radc images are 768x512
  if (width > 768 || raw_width > 768 || height > 512 || raw_height > 512)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
#endif
  static const signed char src[] = {
      1, 1,   2, 3,   3, 4,   4, 2,   5, 7,   6, 5,   7, 6,  7, 8,   1, 0,   2, 1,  3, 3,  4, 4,  5, 2,   6, 7,   7, 6,
      8, 5,   8, 8,   2, 1,   2, 3,   3, 0,   3, 2,   3, 4,  4, 6,   5, 5,   6, 7,  6, 8,  2, 0,  2, 1,   2, 3,   3, 2,
      4, 4,   5, 6,   6, 7,   7, 5,   7, 8,   2, 1,   2, 4,  3, 0,   3, 2,   3, 3,  4, 7,  5, 5,  6, 6,   6, 8,   2, 3,
      3, 1,   3, 2,   3, 4,   3, 5,   3, 6,   4, 7,   5, 0,  5, 8,   2, 3,   2, 6,  3, 0,  3, 1,  4, 4,   4, 5,   4, 7,
      5, 2,   5, 8,   2, 4,   2, 7,   3, 3,   3, 6,   4, 1,  4, 2,   4, 5,   5, 0,  5, 8,  2, 6,  3, 1,   3, 3,   3, 5,
      3, 7,   3, 8,   4, 0,   5, 2,   5, 4,   2, 0,   2, 1,  3, 2,   3, 3,   4, 4,  4, 5,  5, 6,  5, 7,   4, 8,   1, 0,
      2, 2,   2, -2,  1, -3,  1, 3,   2, -17, 2, -5,  2, 5,  2, 17,  2, -7,  2, 2,  2, 9,  2, 18, 2, -18, 2, -9,  2, -2,
      2, 7,   2, -28, 2, 28,  3, -49, 3, -9,  3, 9,   4, 49, 5, -79, 5, 79,  2, -1, 2, 13, 2, 26, 3, 39,  4, -16, 5, 55,
      6, -37, 6, 76,  2, -26, 2, -13, 2, 1,   3, -39, 4, 16, 5, -55, 6, -76, 6, 37};
  ushort huff[19][256];
  int row, col, tree, nreps, rep, step, i, c, s, r, x, y, val;
  short last[3] = {16, 16, 16}, mul[3], buf[3][3][386];
  static const ushort pt[] = {0, 0, 1280, 1344, 2320, 3616, 3328, 8000, 4095, 16383, 65535, 16383};

  for (i = 2; i < 12; i += 2)
    for (c = pt[i - 2]; c <= pt[i]; c++)
      curve[c] = (float)(c - pt[i - 2]) / (pt[i] - pt[i - 2]) * (pt[i + 1] - pt[i - 1]) + pt[i - 1] + 0.5;
  for (s = i = 0; i < sizeof src; i += 2)
    FORC(256 >> src[i])
  ((ushort *)huff)[s++] = src[i] << 8 | (uchar)src[i + 1];
  s = kodak_cbpp == 243 ? 2 : 3;
  FORC(256) huff[18][c] = (8 - s) << 8 | c >> s << s | 1 << (s - 1);
  getbits(-1);
  for (i = 0; i < sizeof(buf) / sizeof(short); i++)
    ((short *)buf)[i] = 2048;
  for (row = 0; row < height; row += 4)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    FORC3 mul[c] = getbits(6);
#ifdef LIBRAW_LIBRARY_BUILD
    if (!mul[0] || !mul[1] || !mul[2])
      throw LIBRAW_EXCEPTION_IO_CORRUPT;
#endif
    FORC3
    {
      val = ((0x1000000 / last[c] + 0x7ff) >> 12) * mul[c];
      s = val > 65564 ? 10 : 12;
      x = ~((~0u) << (s - 1));
      val <<= 12 - s;
      for (i = 0; i < sizeof(buf[0]) / sizeof(short); i++)
        ((short *)buf[c])[i] = (((short *)buf[c])[i] * val + x) >> s;
      last[c] = mul[c];
      for (r = 0; r <= !c; r++)
      {
        buf[c][1][width / 2] = buf[c][2][width / 2] = mul[c] << 7;
        for (tree = 1, col = width / 2; col > 0;)
        {
          if ((tree = radc_token(tree)))
          {
            col -= 2;
	    if(col>=0)
	    {
            if (tree == 8)
              FORYX buf[c][y][x] = (uchar)radc_token(18) * mul[c];
            else
              FORYX buf[c][y][x] = radc_token(tree + 10) * 16 + PREDICTOR;
	    }
          }
          else
            do
            {
              nreps = (col > 2) ? radc_token(9) + 1 : 1;
              for (rep = 0; rep < 8 && rep < nreps && col > 0; rep++)
              {
                col -= 2;
		if(col>=0)
                FORYX buf[c][y][x] = PREDICTOR;
                if (rep & 1)
                {
                  step = radc_token(10) << 4;
                  FORYX buf[c][y][x] += step;
                }
              }
            } while (nreps == 9);
        }
        for (y = 0; y < 2; y++)
          for (x = 0; x < width / 2; x++)
          {
            val = (buf[c][y + 1][x] << 4) / mul[c];
            if (val < 0)
              val = 0;
            if (c)
              RAW(row + y * 2 + c - 1, x * 2 + 2 - c) = val;
            else
              RAW(row + r * 2 + y, x * 2 + y) = val;
          }
        memcpy(buf[c][0] + !c, buf[c][2], sizeof buf[c][0] - 2 * !c);
      }
    }
    for (y = row; y < row + 4; y++)
      for (x = 0; x < width; x++)
        if ((x + y) & 1)
        {
          r = x ? x - 1 : x + 1;
          s = x + 1 < width ? x + 1 : x - 1;
          val = (RAW(y, x) - 2048) * 2 + (RAW(y, r) + RAW(y, s)) / 2;
          if (val < 0)
            val = 0;
          RAW(y, x) = val;
        }
  }
  for (i = 0; i < height * width; i++)
    raw_image[i] = curve[raw_image[i]];
  maximum = 0x3fff;
}

#undef FORYX
#undef PREDICTOR

#ifdef NO_JPEG
void CLASS kodak_jpeg_load_raw() {}
void CLASS lossy_dng_load_raw() {}
#else

#ifndef LIBRAW_LIBRARY_BUILD
METHODDEF(boolean)
fill_input_buffer(j_decompress_ptr cinfo)
{
  static uchar jpeg_buffer[4096];
  size_t nbytes;

  nbytes = fread(jpeg_buffer, 1, 4096, ifp);
  swab(jpeg_buffer, jpeg_buffer, nbytes);
  cinfo->src->next_input_byte = jpeg_buffer;
  cinfo->src->bytes_in_buffer = nbytes;
  return TRUE;
}
void CLASS kodak_jpeg_load_raw()
{
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  JSAMPARRAY buf;
  JSAMPLE(*pixel)[3];
  int row, col;

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, ifp);
  cinfo.src->fill_input_buffer = fill_input_buffer;
  jpeg_read_header(&cinfo, TRUE);
  jpeg_start_decompress(&cinfo);
  if ((cinfo.output_width != width) || (cinfo.output_height * 2 != height) || (cinfo.output_components != 3))
  {
    fprintf(stderr, _("%s: incorrect JPEG dimensions\n"), ifname);
    jpeg_destroy_decompress(&cinfo);
    longjmp(failure, 3);
  }
  buf = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, width * 3, 1);

  while (cinfo.output_scanline < cinfo.output_height)
  {
    row = cinfo.output_scanline * 2;
    jpeg_read_scanlines(&cinfo, buf, 1);
    pixel = (JSAMPLE(*)[3])buf[0];
    for (col = 0; col < width; col += 2)
    {
      RAW(row + 0, col + 0) = pixel[col + 0][1] << 1;
      RAW(row + 1, col + 1) = pixel[col + 1][1] << 1;
      RAW(row + 0, col + 1) = pixel[col][0] + pixel[col + 1][0];
      RAW(row + 1, col + 0) = pixel[col][2] + pixel[col + 1][2];
    }
  }
  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  maximum = 0xff << 1;
}
#else

struct jpegErrorManager
{
  struct jpeg_error_mgr pub;
};

static void jpegErrorExit(j_common_ptr cinfo)
{
  jpegErrorManager *myerr = (jpegErrorManager *)cinfo->err;
  throw LIBRAW_EXCEPTION_DECODE_JPEG;
}

// LibRaw's Kodak_jpeg_load_raw
void CLASS kodak_jpeg_load_raw()
{
  if (data_size < 1)
    throw LIBRAW_EXCEPTION_DECODE_JPEG;

  int row, col;
  jpegErrorManager jerr;
  struct jpeg_decompress_struct cinfo;

  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = jpegErrorExit;

  unsigned char *jpg_buf = (unsigned char *)malloc(data_size);
  merror(jpg_buf, "kodak_jpeg_load_raw");
  unsigned char *pixel_buf = (unsigned char *)malloc(width * 3);
  jpeg_create_decompress(&cinfo);
  merror(pixel_buf, "kodak_jpeg_load_raw");

  fread(jpg_buf, data_size, 1, ifp);
  swab((char *)jpg_buf, (char *)jpg_buf, data_size);
  try
  {
    jpeg_mem_src(&cinfo, jpg_buf, data_size);
    int rc = jpeg_read_header(&cinfo, TRUE);
    if (rc != 1)
      throw LIBRAW_EXCEPTION_DECODE_JPEG;

    jpeg_start_decompress(&cinfo);
    if ((cinfo.output_width != width) || (cinfo.output_height * 2 != height) || (cinfo.output_components != 3))
    {
      throw LIBRAW_EXCEPTION_DECODE_JPEG;
    }

    unsigned char *buf[1];
    buf[0] = pixel_buf;

    while (cinfo.output_scanline < cinfo.output_height)
    {
      checkCancel();
      row = cinfo.output_scanline * 2;
      jpeg_read_scanlines(&cinfo, buf, 1);
      unsigned char(*pixel)[3] = (unsigned char(*)[3])buf[0];
      for (col = 0; col < width; col += 2)
      {
        RAW(row + 0, col + 0) = pixel[col + 0][1] << 1;
        RAW(row + 1, col + 1) = pixel[col + 1][1] << 1;
        RAW(row + 0, col + 1) = pixel[col][0] + pixel[col + 1][0];
        RAW(row + 1, col + 0) = pixel[col][2] + pixel[col + 1][2];
      }
    }
  }
  catch (...)
  {
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    free(jpg_buf);
    free(pixel_buf);
    throw;
  }
  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  free(jpg_buf);
  free(pixel_buf);
  maximum = 0xff << 1;
}
#endif

#ifndef LIBRAW_LIBRARY_BUILD
void CLASS gamma_curve(double pwr, double ts, int mode, int imax);
#endif

void CLASS lossy_dng_load_raw()
{
#ifdef LIBRAW_LIBRARY_BUILD
  if (!image)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
#endif
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  JSAMPARRAY buf;
  JSAMPLE(*pixel)[3];
  unsigned sorder = order, ntags, opcode, deg, i, j, c;
  unsigned save = data_offset - 4, trow = 0, tcol = 0, row, col;
  ushort cur[3][256];
  double coeff[9], tot;

  if (meta_offset)
  {
    fseek(ifp, meta_offset, SEEK_SET);
    order = 0x4d4d;
    ntags = get4();
    while (ntags--)
    {
      opcode = get4();
      get4();
      get4();
      if (opcode != 8)
      {
        fseek(ifp, get4(), SEEK_CUR);
        continue;
      }
      fseek(ifp, 20, SEEK_CUR);
      if ((c = get4()) > 2)
        break;
      fseek(ifp, 12, SEEK_CUR);
      if ((deg = get4()) > 8)
        break;
      for (i = 0; i <= deg && i < 9; i++)
        coeff[i] = getreal(12);
      for (i = 0; i < 256; i++)
      {
        for (tot = j = 0; j <= deg; j++)
          tot += coeff[j] * pow(i / 255.0, (int)j);
        cur[c][i] = tot * 0xffff;
      }
    }
    order = sorder;
  }
  else
  {
    gamma_curve(1 / 2.4, 12.92, 1, 255);
    FORC3 memcpy(cur[c], curve, sizeof cur[0]);
  }
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  while (trow < raw_height)
  {
    fseek(ifp, save += 4, SEEK_SET);
    if (tile_length < INT_MAX)
      fseek(ifp, get4(), SEEK_SET);
#ifdef LIBRAW_LIBRARY_BUILD
    if (libraw_internal_data.internal_data.input->jpeg_src(&cinfo) == -1)
    {
      jpeg_destroy_decompress(&cinfo);
      throw LIBRAW_EXCEPTION_DECODE_JPEG;
    }
#else
    jpeg_stdio_src(&cinfo, ifp);
#endif
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);
    buf = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, cinfo.output_width * 3, 1);
#ifdef LIBRAW_LIBRARY_BUILD
    try
    {
#endif
      while (cinfo.output_scanline < cinfo.output_height && (row = trow + cinfo.output_scanline) < height)
      {
#ifdef LIBRAW_LIBRARY_BUILD
        checkCancel();
#endif
        jpeg_read_scanlines(&cinfo, buf, 1);
        pixel = (JSAMPLE(*)[3])buf[0];
        for (col = 0; col < cinfo.output_width && tcol + col < width; col++)
        {
          FORC3 image[row * width + tcol + col][c] = cur[c][pixel[col][c]];
        }
      }
#ifdef LIBRAW_LIBRARY_BUILD
    }
    catch (...)
    {
      jpeg_destroy_decompress(&cinfo);
      throw;
    }
#endif
    jpeg_abort_decompress(&cinfo);
    if ((tcol += tile_width) >= raw_width)
      trow += tile_length + (tcol = 0);
  }
  jpeg_destroy_decompress(&cinfo);
  maximum = 0xffff;
}
#endif

void CLASS kodak_dc120_load_raw()
{
  static const int mul[4] = {162, 192, 187, 92};
  static const int add[4] = {0, 636, 424, 212};
  uchar pixel[848];
  int row, shift, col;

  for (row = 0; row < height; row++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    if (fread(pixel, 1, 848, ifp) < 848)
      derror();
    shift = row * mul[row & 3] + add[row & 3];
    for (col = 0; col < width; col++)
      RAW(row, col) = (ushort)pixel[(col + shift) % 848];
  }
  maximum = 0xff;
}

void CLASS eight_bit_load_raw()
{
  uchar *pixel;
  unsigned row, col;

  pixel = (uchar *)calloc(raw_width, sizeof *pixel);
  merror(pixel, "eight_bit_load_raw()");
#ifdef LIBRAW_LIBRARY_BUILD
  try
  {
#endif
    for (row = 0; row < raw_height; row++)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      if (fread(pixel, 1, raw_width, ifp) < raw_width)
        derror();
      for (col = 0; col < raw_width; col++)
        RAW(row, col) = curve[pixel[col]];
    }
#ifdef LIBRAW_LIBRARY_BUILD
  }
  catch (...)
  {
    free(pixel);
    throw;
  }
#endif
  free(pixel);
  maximum = curve[0xff];
}

void CLASS kodak_c330_load_raw()
{
#ifdef LIBRAW_LIBRARY_BUILD
  if (!image)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
#endif
  uchar *pixel;
  int row, col, y, cb, cr, rgb[3], c;

  pixel = (uchar *)calloc(raw_width, 2 * sizeof *pixel);
  merror(pixel, "kodak_c330_load_raw()");
#ifdef LIBRAW_LIBRARY_BUILD
  try
  {
#endif
    for (row = 0; row < height; row++)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      if (fread(pixel, raw_width, 2, ifp) < 2)
        derror();
      if (load_flags && (row & 31) == 31)
        fseek(ifp, raw_width * 32, SEEK_CUR);
      for (col = 0; col < width; col++)
      {
        y = pixel[col * 2];
        cb = pixel[(col * 2 & -4) | 1] - 128;
        cr = pixel[(col * 2 & -4) | 3] - 128;
        rgb[1] = y - ((cb + cr + 2) >> 2);
        rgb[2] = rgb[1] + cb;
        rgb[0] = rgb[1] + cr;
        FORC3 image[row * width + col][c] = curve[LIM(rgb[c], 0, 255)];
      }
    }
#ifdef LIBRAW_LIBRARY_BUILD
  }
  catch (...)
  {
    free(pixel);
    throw;
  }
#endif
  free(pixel);
  maximum = curve[0xff];
}

void CLASS kodak_c603_load_raw()
{
#ifdef LIBRAW_LIBRARY_BUILD
  if (!image)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
#endif
  uchar *pixel;
  int row, col, y, cb, cr, rgb[3], c;

  pixel = (uchar *)calloc(raw_width, 3 * sizeof *pixel);
  merror(pixel, "kodak_c603_load_raw()");
#ifdef LIBRAW_LIBRARY_BUILD
  try
  {
#endif
    for (row = 0; row < height; row++)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      if (~row & 1)
        if (fread(pixel, raw_width, 3, ifp) < 3)
          derror();
      for (col = 0; col < width; col++)
      {
        y = pixel[width * 2 * (row & 1) + col];
        cb = pixel[width + (col & -2)] - 128;
        cr = pixel[width + (col & -2) + 1] - 128;
        rgb[1] = y - ((cb + cr + 2) >> 2);
        rgb[2] = rgb[1] + cb;
        rgb[0] = rgb[1] + cr;
        FORC3 image[row * width + col][c] = curve[LIM(rgb[c], 0, 255)];
      }
    }
#ifdef LIBRAW_LIBRARY_BUILD
  }
  catch (...)
  {
    free(pixel);
    throw;
  }
#endif
  free(pixel);
  maximum = curve[0xff];
}

void CLASS kodak_262_load_raw()
{
  static const uchar kodak_tree[2][26] = {
      {0, 1, 5, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
      {0, 3, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9}};
  ushort *huff[2];
  uchar *pixel;
  int *strip, ns, c, row, col, chess, pi = 0, pi1, pi2, pred, val;

  FORC(2) huff[c] = make_decoder(kodak_tree[c]);
  ns = (raw_height + 63) >> 5;
  pixel = (uchar *)malloc(raw_width * 32 + ns * 4);
  merror(pixel, "kodak_262_load_raw()");
  strip = (int *)(pixel + raw_width * 32);
  order = 0x4d4d;
  FORC(ns) strip[c] = get4();
#ifdef LIBRAW_LIBRARY_BUILD
  try
  {
#endif
    for (row = 0; row < raw_height; row++)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      if ((row & 31) == 0)
      {
        fseek(ifp, strip[row >> 5], SEEK_SET);
        getbits(-1);
        pi = 0;
      }
      for (col = 0; col < raw_width; col++)
      {
        chess = (row + col) & 1;
        pi1 = chess ? pi - 2 : pi - raw_width - 1;
        pi2 = chess ? pi - 2 * raw_width : pi - raw_width + 1;
        if (col <= chess)
          pi1 = -1;
        if (pi1 < 0)
          pi1 = pi2;
        if (pi2 < 0)
          pi2 = pi1;
        if (pi1 < 0 && col > 1)
          pi1 = pi2 = pi - 2;
        pred = (pi1 < 0) ? 0 : (pixel[pi1] + pixel[pi2]) >> 1;
        pixel[pi] = val = pred + ljpeg_diff(huff[chess]);
        if (val >> 8)
          derror();
        val = curve[pixel[pi++]];
        RAW(row, col) = val;
      }
    }
#ifdef LIBRAW_LIBRARY_BUILD
  }
  catch (...)
  {
    free(pixel);
    throw;
  }
#endif
  free(pixel);
  FORC(2) free(huff[c]);
}

int CLASS kodak_65000_decode(short *out, int bsize)
{
  uchar c, blen[768];
  ushort raw[6];
  INT64 bitbuf = 0;
  int save, bits = 0, i, j, len, diff;

  save = ftell(ifp);
  bsize = (bsize + 3) & -4;
  for (i = 0; i < bsize; i += 2)
  {
    c = fgetc(ifp);
    if ((blen[i] = c & 15) > 12 || (blen[i + 1] = c >> 4) > 12)
    {
      fseek(ifp, save, SEEK_SET);
      for (i = 0; i < bsize; i += 8)
      {
        read_shorts(raw, 6);
        out[i] = raw[0] >> 12 << 8 | raw[2] >> 12 << 4 | raw[4] >> 12;
        out[i + 1] = raw[1] >> 12 << 8 | raw[3] >> 12 << 4 | raw[5] >> 12;
        for (j = 0; j < 6; j++)
          out[i + 2 + j] = raw[j] & 0xfff;
      }
      return 1;
    }
  }
  if ((bsize & 7) == 4)
  {
    bitbuf = fgetc(ifp) << 8;
    bitbuf += fgetc(ifp);
    bits = 16;
  }
  for (i = 0; i < bsize; i++)
  {
    len = blen[i];
    if (bits < len)
    {
      for (j = 0; j < 32; j += 8)
        bitbuf += (INT64)fgetc(ifp) << (bits + (j ^ 8));
      bits += 32;
    }
    diff = bitbuf & (0xffff >> (16 - len));
    bitbuf >>= len;
    bits -= len;
    if ((diff & (1 << (len - 1))) == 0)
      diff -= (1 << len) - 1;
    out[i] = diff;
  }
  return 0;
}

void CLASS kodak_65000_load_raw()
{
  short buf[272]; /* 264 looks enough */
  int row, col, len, pred[2], ret, i;

  for (row = 0; row < height; row++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    for (col = 0; col < width; col += 256)
    {
      pred[0] = pred[1] = 0;
      len = MIN(256, width - col);
      ret = kodak_65000_decode(buf, len);
      for (i = 0; i < len; i++)
      {
        int idx = ret ? buf[i] : (pred[i & 1] += buf[i]);
        if (idx >= 0 && idx < 0xffff)
        {
          if ((RAW(row, col + i) = curve[idx]) >> 12)
            derror();
        }
        else
          derror();
      }
    }
  }
}

void CLASS kodak_ycbcr_load_raw()
{
#ifdef LIBRAW_LIBRARY_BUILD
  if (!image)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
#endif
  short buf[384], *bp;
  int row, col, len, c, i, j, k, y[2][2], cb, cr, rgb[3];
  ushort *ip;

  unsigned int bits = (load_flags && load_flags > 9 && load_flags < 17) ? load_flags : 10;
  for (row = 0; row < height; row += 2)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    for (col = 0; col < width; col += 128)
    {
      len = MIN(128, width - col);
      kodak_65000_decode(buf, len * 3);
      y[0][1] = y[1][1] = cb = cr = 0;
      for (bp = buf, i = 0; i < len; i += 2, bp += 2)
      {
        cb += bp[4];
        cr += bp[5];
        rgb[1] = -((cb + cr + 2) >> 2);
        rgb[2] = rgb[1] + cb;
        rgb[0] = rgb[1] + cr;
        for (j = 0; j < 2; j++)
          for (k = 0; k < 2; k++)
          {
            if ((y[j][k] = y[j][k ^ 1] + *bp++) >> bits)
              derror();
            ip = image[(row + j) * width + col + i + k];
            FORC3 ip[c] = curve[LIM(y[j][k] + rgb[c], 0, 0xfff)];
          }
      }
    }
  }
}

void CLASS kodak_rgb_load_raw()
{
#ifdef LIBRAW_LIBRARY_BUILD
  if (!image)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
#endif
  short buf[768], *bp;
  int row, col, len, c, i, rgb[3], ret;
  ushort *ip = image[0];

  for (row = 0; row < height; row++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    for (col = 0; col < width; col += 256)
    {
      len = MIN(256, width - col);
      ret = kodak_65000_decode(buf, len * 3);
      memset(rgb, 0, sizeof rgb);
      for (bp = buf, i = 0; i < len; i++, ip += 4)
#ifdef LIBRAW_LIBRARY_BUILD
        if (load_flags == 12)
        {
          FORC3 ip[c] = ret ? (*bp++) : (rgb[c] += *bp++);
        }
        else
#endif
          FORC3 if ((ip[c] = ret ? (*bp++) : (rgb[c] += *bp++)) >> 12) derror();
    }
  }
}

void CLASS kodak_thumb_load_raw()
{
#ifdef LIBRAW_LIBRARY_BUILD
  if (!image)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
#endif
  int row, col;
  colors = thumb_misc >> 5;
  for (row = 0; row < height; row++)
    for (col = 0; col < width; col++)
      read_shorts(image[row * width + col], colors);
  maximum = (1 << (thumb_misc & 31)) - 1;
}

void CLASS sony_decrypt(unsigned *data, int len, int start, int key)
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
      pad[p] = key = key * 48828125 + 1;
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

void CLASS sony_load_raw()
{
  uchar head[40];
  ushort *pixel;
  unsigned i, key, row, col;

  fseek(ifp, 200896, SEEK_SET);
  fseek(ifp, (unsigned)fgetc(ifp) * 4 - 1, SEEK_CUR);
  order = 0x4d4d;
  key = get4();

  fseek(ifp, 164600, SEEK_SET);
  fread(head, 1, 40, ifp);
  sony_decrypt((unsigned *)head, 10, 1, key);
  for (i = 26; i-- > 22;)
    key = key << 8 | head[i];

  fseek(ifp, data_offset, SEEK_SET);
  for (row = 0; row < raw_height; row++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    pixel = raw_image + row * raw_width;
    if (fread(pixel, 2, raw_width, ifp) < raw_width)
      derror();
    sony_decrypt((unsigned *)pixel, raw_width / 2, !row, key);
    for (col = 0; col < raw_width; col++)
      if ((pixel[col] = ntohs(pixel[col])) >> 14)
        derror();
  }
  maximum = 0x3ff0;
}

void CLASS sony_arw_load_raw()
{
  ushort huff[32770];
  static const ushort tab[18] = {0xf11, 0xf10, 0xe0f, 0xd0e, 0xc0d, 0xb0c, 0xa0b, 0x90a, 0x809,
                                 0x708, 0x607, 0x506, 0x405, 0x304, 0x303, 0x300, 0x202, 0x201};
  int i, c, n, col, row, sum = 0;

  huff[0] = 15;
  for (n = i = 0; i < 18; i++)
    FORC(32768 >> (tab[i] >> 8)) huff[++n] = tab[i];
  getbits(-1);
  for (col = raw_width; col--;)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    for (row = 0; row < raw_height + 1; row += 2)
    {
      if (row == raw_height)
        row = 1;
      if ((sum += ljpeg_diff(huff)) >> 12)
        derror();
      if (row < height)
        RAW(row, col) = sum;
    }
  }
}

void CLASS sony_arw2_load_raw()
{
  uchar *data, *dp;
  ushort pix[16];
  int row, col, val, max, min, imax, imin, sh, bit, i;

  data = (uchar *)malloc(raw_width + 1);
  merror(data, "sony_arw2_load_raw()");
#ifdef LIBRAW_LIBRARY_BUILD
  try
  {
#endif
    for (row = 0; row < height; row++)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      fread(data, 1, raw_width, ifp);
      for (dp = data, col = 0; col < raw_width - 30; dp += 16)
      {
        max = 0x7ff & (val = sget4(dp));
        min = 0x7ff & val >> 11;
        imax = 0x0f & val >> 22;
        imin = 0x0f & val >> 26;
        for (sh = 0; sh < 4 && 0x80 << sh <= max - min; sh++)
          ;
#ifdef LIBRAW_LIBRARY_BUILD
        /* flag checks if outside of loop */
        if (!(imgdata.params.raw_processing_options & LIBRAW_PROCESSING_SONYARW2_ALLFLAGS) // no flag set
            || (imgdata.params.raw_processing_options & LIBRAW_PROCESSING_SONYARW2_DELTATOVALUE))
        {
          for (bit = 30, i = 0; i < 16; i++)
            if (i == imax)
              pix[i] = max;
            else if (i == imin)
              pix[i] = min;
            else
            {
              pix[i] = ((sget2(dp + (bit >> 3)) >> (bit & 7) & 0x7f) << sh) + min;
              if (pix[i] > 0x7ff)
                pix[i] = 0x7ff;
              bit += 7;
            }
        }
        else if (imgdata.params.raw_processing_options & LIBRAW_PROCESSING_SONYARW2_BASEONLY)
        {
          for (bit = 30, i = 0; i < 16; i++)
            if (i == imax)
              pix[i] = max;
            else if (i == imin)
              pix[i] = min;
            else
              pix[i] = 0;
        }
        else if (imgdata.params.raw_processing_options & LIBRAW_PROCESSING_SONYARW2_DELTAONLY)
        {
          for (bit = 30, i = 0; i < 16; i++)
            if (i == imax)
              pix[i] = 0;
            else if (i == imin)
              pix[i] = 0;
            else
            {
              pix[i] = ((sget2(dp + (bit >> 3)) >> (bit & 7) & 0x7f) << sh) + min;
              if (pix[i] > 0x7ff)
                pix[i] = 0x7ff;
              bit += 7;
            }
        }
        else if (imgdata.params.raw_processing_options & LIBRAW_PROCESSING_SONYARW2_DELTAZEROBASE)
        {
          for (bit = 30, i = 0; i < 16; i++)
            if (i == imax)
              pix[i] = 0;
            else if (i == imin)
              pix[i] = 0;
            else
            {
              pix[i] = ((sget2(dp + (bit >> 3)) >> (bit & 7) & 0x7f) << sh);
              if (pix[i] > 0x7ff)
                pix[i] = 0x7ff;
              bit += 7;
            }
        }
#else
      /* unaltered dcraw processing */
      for (bit = 30, i = 0; i < 16; i++)
        if (i == imax)
          pix[i] = max;
        else if (i == imin)
          pix[i] = min;
        else
        {
          pix[i] = ((sget2(dp + (bit >> 3)) >> (bit & 7) & 0x7f) << sh) + min;
          if (pix[i] > 0x7ff)
            pix[i] = 0x7ff;
          bit += 7;
        }
#endif

#ifdef LIBRAW_LIBRARY_BUILD
        if (imgdata.params.raw_processing_options & LIBRAW_PROCESSING_SONYARW2_DELTATOVALUE)
        {
          for (i = 0; i < 16; i++, col += 2)
          {
            unsigned slope = pix[i] < 1001 ? 2 : curve[pix[i] << 1] - curve[(pix[i] << 1) - 2];
            unsigned step = 1 << sh;
            RAW(row, col) = curve[pix[i] << 1] > black + imgdata.params.sony_arw2_posterization_thr
                                ? LIM(((slope * step * 1000) / (curve[pix[i] << 1] - black)), 0, 10000)
                                : 0;
          }
        }
        else
        {
          for (i = 0; i < 16; i++, col += 2)
            RAW(row, col) = curve[pix[i] << 1];
        }
#else
      for (i = 0; i < 16; i++, col += 2)
        RAW(row, col) = curve[pix[i] << 1] >> 2;
#endif
        col -= col & 1 ? 1 : 31;
      }
    }
#ifdef LIBRAW_LIBRARY_BUILD
  }
  catch (...)
  {
    free(data);
    throw;
  }
  if (imgdata.params.raw_processing_options & LIBRAW_PROCESSING_SONYARW2_DELTATOVALUE)
    maximum = 10000;
#endif
  free(data);
}

void CLASS samsung_load_raw()
{
  int row, col, c, i, dir, op[4], len[4];
#ifdef LIBRAW_LIBRARY_BUILD
  if(raw_width> 32768 || raw_height > 32768)  // definitely too much for old samsung
    throw LIBRAW_EXCEPTION_IO_BADFILE;
#endif
  unsigned maxpixels = raw_width*(raw_height+7);

  order = 0x4949;
  for (row = 0; row < raw_height; row++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    fseek(ifp, strip_offset + row * 4, SEEK_SET);
    fseek(ifp, data_offset + get4(), SEEK_SET);
    ph1_bits(-1);
    FORC4 len[c] = row < 2 ? 7 : 4;
    for (col = 0; col < raw_width; col += 16)
    {
      dir = ph1_bits(1);
      FORC4 op[c] = ph1_bits(2);
      FORC4 switch (op[c])
      {
      case 3:
        len[c] = ph1_bits(4);
        break;
      case 2:
        len[c]--;
        break;
      case 1:
        len[c]++;
      }
      for (c = 0; c < 16; c += 2)
      {
        i = len[((c & 1) << 1) | (c >> 3)];
	unsigned idest = RAWINDEX(row, col + c);
	unsigned isrc = (dir ? RAWINDEX(row + (~c | -2), col + c) : col ? RAWINDEX(row, col + (c | -2)) : 0);
	if(idest < maxpixels && isrc < maxpixels) // less than zero is handled by unsigned conversion
  	RAW(row, col + c) = ((signed)ph1_bits(i) << (32 - i) >> (32 - i)) + 			                (dir ? RAW(row + (~c | -2), col + c) : col ? RAW(row, col + (c | -2)) : 128);
	else
  	  derror();
        if (c == 14)
          c = -1;
      }
    }
  }
  for (row = 0; row < raw_height - 1; row += 2)
    for (col = 0; col < raw_width - 1; col += 2)
      SWAP(RAW(row, col + 1), RAW(row + 1, col));
}

void CLASS samsung2_load_raw()
{
  static const ushort tab[14] = {0x304, 0x307, 0x206, 0x205, 0x403, 0x600, 0x709,
                                 0x80a, 0x90b, 0xa0c, 0xa0d, 0x501, 0x408, 0x402};
  ushort huff[1026], vpred[2][2] = {{0, 0}, {0, 0}}, hpred[2];
  int i, c, n, row, col, diff;

  huff[0] = 10;
  for (n = i = 0; i < 14; i++)
    FORC(1024 >> (tab[i] >> 8)) huff[++n] = tab[i];
  getbits(-1);
  for (row = 0; row < raw_height; row++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    for (col = 0; col < raw_width; col++)
    {
      diff = ljpeg_diff(huff);
      if (col < 2)
        hpred[col] = vpred[row & 1][col] += diff;
      else
        hpred[col & 1] += diff;
      RAW(row, col) = hpred[col & 1];
      if (hpred[col & 1] >> tiff_bps)
        derror();
    }
  }
}

void CLASS samsung3_load_raw()
{
  int opt, init, mag, pmode, row, tab, col, pred, diff, i, c;
  ushort lent[3][2], len[4], *prow[2];

  order = 0x4949;
  fseek(ifp, 9, SEEK_CUR);
  opt = fgetc(ifp);
  init = (get2(), get2());
  for (row = 0; row < raw_height; row++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    fseek(ifp, (data_offset - ftell(ifp)) & 15, SEEK_CUR);
    ph1_bits(-1);
    mag = 0;
    pmode = 7;
    FORC(6)((ushort *)lent)[c] = row < 2 ? 7 : 4;
    prow[row & 1] = &RAW(row - 1, 1 - ((row & 1) << 1)); // green
    prow[~row & 1] = &RAW(row - 2, 0);                   // red and blue
    for (tab = 0; tab + 15 < raw_width; tab += 16)
    {
      if (~opt & 4 && !(tab & 63))
      {
        i = ph1_bits(2);
        mag = i < 3 ? mag - '2' + "204"[i] : ph1_bits(12);
      }
      if (opt & 2)
        pmode = 7 - 4 * ph1_bits(1);
      else if (!ph1_bits(1))
        pmode = ph1_bits(3);
      if (opt & 1 || !ph1_bits(1))
      {
        FORC4 len[c] = ph1_bits(2);
        FORC4
        {
          i = ((row & 1) << 1 | (c & 1)) % 3;
          len[c] = len[c] < 3 ? lent[i][0] - '1' + "120"[len[c]] : ph1_bits(4);
          lent[i][0] = lent[i][1];
          lent[i][1] = len[c];
        }
      }
      FORC(16)
      {
        col = tab + (((c & 7) << 1) ^ (c >> 3) ^ (row & 1));
        pred =
            (pmode == 7 || row < 2)
                ? (tab ? RAW(row, tab - 2 + (col & 1)) : init)
                : (prow[col & 1][col - '4' + "0224468"[pmode]] + prow[col & 1][col - '4' + "0244668"[pmode]] + 1) >> 1;
        diff = ph1_bits(i = len[c >> 2]);
        if (diff >> (i - 1))
          diff -= 1 << i;
        diff = diff * (mag * 2 + 1) + mag;
        RAW(row, col) = pred + diff;
      }
    }
  }
}

#define HOLE(row) ((holes >> (((row)-raw_height) & 7)) & 1)

/* Kudos to Rich Taylor for figuring out SMaL's compression algorithm. */
void CLASS smal_decode_segment(unsigned seg[2][2], int holes)
{
  uchar hist[3][13] = {{7, 7, 0, 0, 63, 55, 47, 39, 31, 23, 15, 7, 0},
                       {7, 7, 0, 0, 63, 55, 47, 39, 31, 23, 15, 7, 0},
                       {3, 3, 0, 0, 63, 47, 31, 15, 0}};
  int low, high = 0xff, carry = 0, nbits = 8;
  int pix, s, count, bin, next, i, sym[3];
  uchar diff, pred[] = {0, 0};
  ushort data = 0, range = 0;

  fseek(ifp, seg[0][1] + 1, SEEK_SET);
  getbits(-1);
  if (seg[1][0] > raw_width * raw_height)
    seg[1][0] = raw_width * raw_height;
  for (pix = seg[0][0]; pix < seg[1][0]; pix++)
  {
    for (s = 0; s < 3; s++)
    {
      data = data << nbits | getbits(nbits);
      if (carry < 0)
        carry = (nbits += carry + 1) < 1 ? nbits - 1 : 0;
      while (--nbits >= 0)
        if ((data >> nbits & 0xff) == 0xff)
          break;
      if (nbits > 0)
        data = ((data & ((1 << (nbits - 1)) - 1)) << 1) |
               ((data + (((data & (1 << (nbits - 1)))) << 1)) & ((~0u) << nbits));
      if (nbits >= 0)
      {
        data += getbits(1);
        carry = nbits - 8;
      }
      count = ((((data - range + 1) & 0xffff) << 2) - 1) / (high >> 4);
      for (bin = 0; hist[s][bin + 5] > count; bin++)
        ;
      low = hist[s][bin + 5] * (high >> 4) >> 2;
      if (bin)
        high = hist[s][bin + 4] * (high >> 4) >> 2;
      high -= low;
      for (nbits = 0; high << nbits < 128; nbits++)
        ;
      range = (range + low) << nbits;
      high <<= nbits;
      next = hist[s][1];
      if (++hist[s][2] > hist[s][3])
      {
        next = (next + 1) & hist[s][0];
        hist[s][3] = (hist[s][next + 4] - hist[s][next + 5]) >> 2;
        hist[s][2] = 1;
      }
      if (hist[s][hist[s][1] + 4] - hist[s][hist[s][1] + 5] > 1)
      {
        if (bin < hist[s][1])
          for (i = bin; i < hist[s][1]; i++)
            hist[s][i + 5]--;
        else if (next <= bin)
          for (i = hist[s][1]; i < bin; i++)
            hist[s][i + 5]++;
      }
      hist[s][1] = next;
      sym[s] = bin;
    }
    diff = sym[2] << 5 | sym[1] << 2 | (sym[0] & 3);
    if (sym[0] & 4)
      diff = diff ? -diff : 0x80;
    if (ftell(ifp) + 12 >= seg[1][1])
      diff = 0;
#ifdef LIBRAW_LIBRARY_BUILD
    if (pix >= raw_width * raw_height)
      throw LIBRAW_EXCEPTION_IO_CORRUPT;
#endif
    raw_image[pix] = pred[pix & 1] += diff;
    if (!(pix & 1) && HOLE(pix / raw_width))
      pix += 2;
  }
  maximum = 0xff;
}

void CLASS smal_v6_load_raw()
{
  unsigned seg[2][2];

  fseek(ifp, 16, SEEK_SET);
  seg[0][0] = 0;
  seg[0][1] = get2();
  seg[1][0] = raw_width * raw_height;
  seg[1][1] = INT_MAX;
  smal_decode_segment(seg, 0);
}

int CLASS median4(int *p)
{
  int min, max, sum, i;

  min = max = sum = p[0];
  for (i = 1; i < 4; i++)
  {
    sum += p[i];
    if (min > p[i])
      min = p[i];
    if (max < p[i])
      max = p[i];
  }
  return (sum - min - max) >> 1;
}

void CLASS fill_holes(int holes)
{
  int row, col, val[4];

  for (row = 2; row < height - 2; row++)
  {
    if (!HOLE(row))
      continue;
    for (col = 1; col < width - 1; col += 4)
    {
      val[0] = RAW(row - 1, col - 1);
      val[1] = RAW(row - 1, col + 1);
      val[2] = RAW(row + 1, col - 1);
      val[3] = RAW(row + 1, col + 1);
      RAW(row, col) = median4(val);
    }
    for (col = 2; col < width - 2; col += 4)
      if (HOLE(row - 2) || HOLE(row + 2))
        RAW(row, col) = (RAW(row, col - 2) + RAW(row, col + 2)) >> 1;
      else
      {
        val[0] = RAW(row, col - 2);
        val[1] = RAW(row, col + 2);
        val[2] = RAW(row - 2, col);
        val[3] = RAW(row + 2, col);
        RAW(row, col) = median4(val);
      }
  }
}

void CLASS smal_v9_load_raw()
{
  unsigned seg[256][2], offset, nseg, holes, i;

  fseek(ifp, 67, SEEK_SET);
  offset = get4();
  nseg = (uchar)fgetc(ifp);
  fseek(ifp, offset, SEEK_SET);
  for (i = 0; i < nseg * 2; i++)
    ((unsigned *)seg)[i] = get4() + data_offset * (i & 1);
  fseek(ifp, 78, SEEK_SET);
  holes = fgetc(ifp);
  fseek(ifp, 88, SEEK_SET);
  seg[nseg][0] = raw_height * raw_width;
  seg[nseg][1] = get4() + data_offset;
  for (i = 0; i < nseg; i++)
    smal_decode_segment(seg + i, holes);
  if (holes)
    fill_holes(holes);
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
  in = jas_stream_fopen(ifname, "rb");
#else
  in = (jas_stream_t *)ifp->make_jas_stream();
  if (!in)
    throw LIBRAW_EXCEPTION_DECODE_JPEG2000;
#endif
  jas_stream_seek(in, data_offset + 20, SEEK_SET);
  jimg = jas_image_decode(in, -1, 0);
#ifndef LIBRAW_LIBRARY_BUILD
  if (!jimg)
    longjmp(failure, 3);
#else
  if (!jimg)
  {
    jas_stream_close(in);
    throw LIBRAW_EXCEPTION_DECODE_JPEG2000;
  }
#endif
  jmat = jas_matrix_create(height / 2, width / 2);
  merror(jmat, "redcine_load_raw()");
  img = (ushort *)calloc((height + 2), (width + 2) * 2);
  merror(img, "redcine_load_raw()");
#ifdef LIBRAW_LIBRARY_BUILD
  bool fastexitflag = false;
  try
  {
#endif
    FORC4
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      jas_image_readcmpt(jimg, c, 0, 0, width / 2, height / 2, jmat);
      data = jas_matrix_getref(jmat, 0, 0);
      for (row = c >> 1; row < height; row += 2)
        for (col = c & 1; col < width; col += 2)
          img[(row + 1) * (width + 2) + col + 1] = data[(row / 2) * (width / 2) + col / 2];
    }
    for (col = 1; col <= width; col++)
    {
      img[col] = img[2 * (width + 2) + col];
      img[(height + 1) * (width + 2) + col] = img[(height - 1) * (width + 2) + col];
    }
    for (row = 0; row < height + 2; row++)
    {
      img[row * (width + 2)] = img[row * (width + 2) + 2];
      img[(row + 1) * (width + 2) - 1] = img[(row + 1) * (width + 2) - 3];
    }
    for (row = 1; row <= height; row++)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      pix = img + row * (width + 2) + (col = 1 + (FC(row, 1) & 1));
      for (; col <= width; col += 2, pix += 2)
      {
        c = (((pix[0] - 0x800) << 3) + pix[-(width + 2)] + pix[width + 2] + pix[-1] + pix[1]) >> 2;
        pix[0] = LIM(c, 0, 4095);
      }
    }
    for (row = 0; row < height; row++)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      for (col = 0; col < width; col++)
        RAW(row, col) = curve[img[(row + 1) * (width + 2) + col + 1]];
    }
#ifdef LIBRAW_LIBRARY_BUILD
  }
  catch (...)
  {
    fastexitflag = true;
  }
#endif
  free(img);
  jas_matrix_destroy(jmat);
  jas_image_destroy(jimg);
  jas_stream_close(in);
#ifdef LIBRAW_LIBRARY_BUILD
  if (fastexitflag)
    throw LIBRAW_EXCEPTION_CANCELLED_BY_CALLBACK;
#endif
#endif
}
//@end COMMON

/* RESTRICTED code starts here */

void CLASS foveon_decoder(unsigned size, unsigned code)
{
  static unsigned huff[1024];
  struct decode *cur;
  int i, len;

  if (!code)
  {
    for (i = 0; i < size; i++)
      huff[i] = get4();
    memset(first_decode, 0, sizeof first_decode);
    free_decode = first_decode;
  }
  cur = free_decode++;
  if (free_decode > first_decode + 2048)
  {
    fprintf(stderr, _("%s: decoder table overflow\n"), ifname);
    longjmp(failure, 2);
  }
  if (code)
    for (i = 0; i < size; i++)
      if (huff[i] == code)
      {
        cur->leaf = i;
        return;
      }
  if ((len = code >> 27) > 26)
    return;
  code = (len + 1) << 27 | (code & 0x3ffffff) << 1;

  cur->branch[0] = free_decode;
  foveon_decoder(size, code);
  cur->branch[1] = free_decode;
  foveon_decoder(size, code + 1);
}

void CLASS foveon_thumb()
{
  unsigned bwide, row, col, bitbuf = 0, bit = 1, c, i;
  char *buf;
  struct decode *dindex;
  short pred[3];

  bwide = get4();
  fprintf(ofp, "P6\n%d %d\n255\n", thumb_width, thumb_height);
  if (bwide > 0)
  {
    if (bwide < thumb_width * 3)
      return;
    buf = (char *)malloc(bwide);
    merror(buf, "foveon_thumb()");
    for (row = 0; row < thumb_height; row++)
    {
      fread(buf, 1, bwide, ifp);
      fwrite(buf, 3, thumb_width, ofp);
    }
    free(buf);
    return;
  }
  foveon_decoder(256, 0);

  for (row = 0; row < thumb_height; row++)
  {
    memset(pred, 0, sizeof pred);
    if (!bit)
      get4();
    for (bit = col = 0; col < thumb_width; col++)
      FORC3
      {
        for (dindex = first_decode; dindex->branch[0];)
        {
          if ((bit = (bit - 1) & 31) == 31)
            for (i = 0; i < 4; i++)
              bitbuf = (bitbuf << 8) + fgetc(ifp);
          dindex = dindex->branch[bitbuf >> bit & 1];
        }
        pred[c] += dindex->leaf;
        fputc(pred[c], ofp);
      }
  }
}

void CLASS foveon_sd_load_raw()
{
#ifdef LIBRAW_LIBRARY_BUILD
  if (!image)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
#endif
  struct decode *dindex;
  short diff[1024];
  unsigned bitbuf = 0;
  int pred[3], row, col, bit = -1, c, i;

  read_shorts((ushort *)diff, 1024);
  if (!load_flags)
    foveon_decoder(1024, 0);

  for (row = 0; row < height; row++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    memset(pred, 0, sizeof pred);
    if (!bit && !load_flags && atoi(model + 2) < 14)
      get4();
    for (col = bit = 0; col < width; col++)
    {
      if (load_flags)
      {
        bitbuf = get4();
        FORC3 pred[2 - c] += diff[bitbuf >> c * 10 & 0x3ff];
      }
      else
        FORC3
        {
          for (dindex = first_decode; dindex->branch[0];)
          {
            if ((bit = (bit - 1) & 31) == 31)
              for (i = 0; i < 4; i++)
                bitbuf = (bitbuf << 8) + fgetc(ifp);
            dindex = dindex->branch[bitbuf >> bit & 1];
          }
          pred[c] += diff[dindex->leaf];
          if (pred[c] >> 16 && ~pred[c] >> 16)
            derror();
        }
      FORC3 image[row * width + col][c] = pred[c];
    }
  }
}

void CLASS foveon_huff(ushort *huff)
{
  int i, j, clen, code;

  huff[0] = 8;
  for (i = 0; i < 13; i++)
  {
    clen = getc(ifp);
    code = getc(ifp);
    for (j = 0; j<256>> clen;)
      huff[code + ++j] = clen << 8 | i;
  }
  get2();
}

void CLASS foveon_dp_load_raw()
{
#ifdef LIBRAW_LIBRARY_BUILD
  if (!image)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
#endif
  unsigned c, roff[4], row, col, diff;
  ushort huff[512], vpred[2][2], hpred[2];

  fseek(ifp, 8, SEEK_CUR);
  foveon_huff(huff);
  roff[0] = 48;
  FORC3 roff[c + 1] = -(-(roff[c] + get4()) & -16);
  FORC3
  {
    fseek(ifp, data_offset + roff[c], SEEK_SET);
    getbits(-1);
    vpred[0][0] = vpred[0][1] = vpred[1][0] = vpred[1][1] = 512;
    for (row = 0; row < height; row++)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      checkCancel();
#endif
      for (col = 0; col < width; col++)
      {
        diff = ljpeg_diff(huff);
        if (col < 2)
          hpred[col] = vpred[row & 1][col] += diff;
        else
          hpred[col & 1] += diff;
        image[row * width + col][c] = hpred[col & 1];
      }
    }
  }
}

void CLASS foveon_load_camf()
{
  unsigned type, wide, high, i, j, row, col, diff;
  ushort huff[258], vpred[2][2] = {{512, 512}, {512, 512}}, hpred[2];

  fseek(ifp, meta_offset, SEEK_SET);
  type = get4();
  get4();
  get4();
  wide = get4();
  high = get4();
  if (type == 2)
  {
    fread(meta_data, 1, meta_length, ifp);
    for (i = 0; i < meta_length; i++)
    {
      high = (high * 1597 + 51749) % 244944;
      wide = high * (INT64)301593171 >> 24;
      meta_data[i] ^= ((((high << 8) - wide) >> 1) + wide) >> 17;
    }
  }
  else if (type == 4)
  {
    free(meta_data);
    meta_data = (char *)malloc(meta_length = wide * high * 3 / 2);
    merror(meta_data, "foveon_load_camf()");
    foveon_huff(huff);
    get4();
    getbits(-1);
    for (j = row = 0; row < high; row++)
    {
      for (col = 0; col < wide; col++)
      {
        diff = ljpeg_diff(huff);
        if (col < 2)
          hpred[col] = vpred[row & 1][col] += diff;
        else
          hpred[col & 1] += diff;
        if (col & 1)
        {
          meta_data[j++] = hpred[0] >> 4;
          meta_data[j++] = hpred[0] << 4 | hpred[1] >> 8;
          meta_data[j++] = hpred[1];
        }
      }
    }
  }
#ifdef DCRAW_VERBOSE
  else
    fprintf(stderr, _("%s has unknown CAMF type %d.\n"), ifname, type);
#endif
}

const char *CLASS foveon_camf_param(const char *block, const char *param)
{
  unsigned idx, num;
  char *pos, *cp, *dp;

  for (idx = 0; idx < meta_length; idx += sget4(pos + 8))
  {
    pos = meta_data + idx;
    if (strncmp(pos, "CMb", 3))
      break;
    if (pos[3] != 'P')
      continue;
    if (strcmp(block, pos + sget4(pos + 12)))
      continue;
    cp = pos + sget4(pos + 16);
    num = sget4(cp);
    dp = pos + sget4(cp + 4);
    while (num--)
    {
      cp += 8;
      if (!strcmp(param, dp + sget4(cp)))
        return dp + sget4(cp + 4);
    }
  }
  return 0;
}

void *CLASS foveon_camf_matrix(unsigned dim[3], const char *name)
{
  unsigned i, idx, type, ndim, size, *mat;
  char *pos, *cp, *dp;
  double dsize;

  for (idx = 0; idx < meta_length; idx += sget4(pos + 8))
  {
    pos = meta_data + idx;
    if (strncmp(pos, "CMb", 3))
      break;
    if (pos[3] != 'M')
      continue;
    if (strcmp(name, pos + sget4(pos + 12)))
      continue;
    dim[0] = dim[1] = dim[2] = 1;
    cp = pos + sget4(pos + 16);
    type = sget4(cp);
    if ((ndim = sget4(cp + 4)) > 3)
      break;
    dp = pos + sget4(cp + 8);
    for (i = ndim; i--;)
    {
      cp += 12;
      dim[i] = sget4(cp);
    }
    if ((dsize = (double)dim[0] * dim[1] * dim[2]) > meta_length / 4)
      break;
    mat = (unsigned *)malloc((size = dsize) * 4);
    merror(mat, "foveon_camf_matrix()");
    for (i = 0; i < size; i++)
      if (type && type != 6)
        mat[i] = sget4(dp + i * 4);
      else
        mat[i] = sget4(dp + i * 2) & 0xffff;
    return mat;
  }
#ifdef DCRAW_VERBOSE
  fprintf(stderr, _("%s: \"%s\" matrix not found!\n"), ifname, name);
#endif
  return 0;
}

int CLASS foveon_fixed(void *ptr, int size, const char *name)
{
  void *dp;
  unsigned dim[3];

  if (!name)
    return 0;
  dp = foveon_camf_matrix(dim, name);
  if (!dp)
    return 0;
  memcpy(ptr, dp, size * 4);
  free(dp);
  return 1;
}

float CLASS foveon_avg(short *pix, int range[2], float cfilt)
{
  int i;
  float val, min = FLT_MAX, max = -FLT_MAX, sum = 0;

  for (i = range[0]; i <= range[1]; i++)
  {
    sum += val = pix[i * 4] + (pix[i * 4] - pix[(i - 1) * 4]) * cfilt;
    if (min > val)
      min = val;
    if (max < val)
      max = val;
  }
  if (range[1] - range[0] == 1)
    return sum / 2;
  return (sum - min - max) / (range[1] - range[0] - 1);
}

short *CLASS foveon_make_curve(double max, double mul, double filt)
{
  short *curve;
  unsigned i, size;
  double x;

  if (!filt)
    filt = 0.8;
  size = 4 * M_PI * max / filt;
  if (size == UINT_MAX)
    size--;
  curve = (short *)calloc(size + 1, sizeof *curve);
  merror(curve, "foveon_make_curve()");
  curve[0] = size;
  for (i = 0; i < size; i++)
  {
    x = i * filt / max / 4;
    curve[i + 1] = (cos(x) + 1) / 2 * tanh(i * filt / mul) * mul + 0.5;
  }
  return curve;
}

void CLASS foveon_make_curves(short **curvep, float dq[3], float div[3], float filt)
{
  double mul[3], max = 0;
  int c;

  FORC3 mul[c] = dq[c] / div[c];
  FORC3 if (max < mul[c]) max = mul[c];
  FORC3 curvep[c] = foveon_make_curve(max, mul[c], filt);
}

int CLASS foveon_apply_curve(short *curve, int i)
{
  if (abs(i) >= curve[0])
    return 0;
  return i < 0 ? -curve[1 - i] : curve[1 + i];
}

#define image ((short(*)[4])image)

void CLASS foveon_interpolate()
{
  static const short hood[] = {-1, -1, -1, 0, -1, 1, 0, -1, 0, 1, 1, -1, 1, 0, 1, 1};
  short *pix, prev[3], *curve[8], (*shrink)[3];
  float cfilt = 0, ddft[3][3][2], ppm[3][3][3];
  float cam_xyz[3][3], correct[3][3], last[3][3], trans[3][3];
  float chroma_dq[3], color_dq[3], diag[3][3], div[3];
  float(*black)[3], (*sgain)[3], (*sgrow)[3];
  float fsum[3], val, frow, num;
  int row, col, c, i, j, diff, sgx, irow, sum, min, max, limit;
  int dscr[2][2], dstb[4], (*smrow[7])[3], total[4], ipix[3];
  int work[3][3], smlast, smred, smred_p = 0, dev[3];
  int satlev[3], keep[4], active[4];
  unsigned dim[3], *badpix;
  double dsum = 0, trsum[3];
  char str[128];
  const char *cp;

#ifdef DCRAW_VERBOSE
  if (verbose)
    fprintf(stderr, _("Foveon interpolation...\n"));
#endif

  foveon_load_camf();
  foveon_fixed(dscr, 4, "DarkShieldColRange");
  foveon_fixed(ppm[0][0], 27, "PostPolyMatrix");
  foveon_fixed(satlev, 3, "SaturationLevel");
  foveon_fixed(keep, 4, "KeepImageArea");
  foveon_fixed(active, 4, "ActiveImageArea");
  foveon_fixed(chroma_dq, 3, "ChromaDQ");
  foveon_fixed(color_dq, 3, foveon_camf_param("IncludeBlocks", "ColorDQ") ? "ColorDQ" : "ColorDQCamRGB");
  if (foveon_camf_param("IncludeBlocks", "ColumnFilter"))
    foveon_fixed(&cfilt, 1, "ColumnFilter");

  memset(ddft, 0, sizeof ddft);
  if (!foveon_camf_param("IncludeBlocks", "DarkDrift") || !foveon_fixed(ddft[1][0], 12, "DarkDrift"))
    for (i = 0; i < 2; i++)
    {
      foveon_fixed(dstb, 4, i ? "DarkShieldBottom" : "DarkShieldTop");
      for (row = dstb[1]; row <= dstb[3]; row++)
        for (col = dstb[0]; col <= dstb[2]; col++)
          FORC3 ddft[i + 1][c][1] += (short)image[row * width + col][c];
      FORC3 ddft[i + 1][c][1] /= (dstb[3] - dstb[1] + 1) * (dstb[2] - dstb[0] + 1);
    }

  if (!(cp = foveon_camf_param("WhiteBalanceIlluminants", model2)))
  {
#ifdef DCRAW_VERBOSE
    fprintf(stderr, _("%s: Invalid white balance \"%s\"\n"), ifname, model2);
#endif
    return;
  }
  foveon_fixed(cam_xyz, 9, cp);
  foveon_fixed(correct, 9, foveon_camf_param("WhiteBalanceCorrections", model2));
  memset(last, 0, sizeof last);
  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
      FORC3 last[i][j] += correct[i][c] * cam_xyz[c][j];

#define LAST(x, y) last[(i + x) % 3][(c + y) % 3]
  for (i = 0; i < 3; i++)
    FORC3 diag[c][i] = LAST(1, 1) * LAST(2, 2) - LAST(1, 2) * LAST(2, 1);
#undef LAST
  FORC3 div[c] = diag[c][0] * 0.3127 + diag[c][1] * 0.329 + diag[c][2] * 0.3583;
  sprintf(str, "%sRGBNeutral", model2);
  if (foveon_camf_param("IncludeBlocks", str))
    foveon_fixed(div, 3, str);
  num = 0;
  FORC3 if (num < div[c]) num = div[c];
  FORC3 div[c] /= num;

  memset(trans, 0, sizeof trans);
  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
      FORC3 trans[i][j] += rgb_cam[i][c] * last[c][j] * div[j];
  FORC3 trsum[c] = trans[c][0] + trans[c][1] + trans[c][2];
  dsum = (6 * trsum[0] + 11 * trsum[1] + 3 * trsum[2]) / 20;
  for (i = 0; i < 3; i++)
    FORC3 last[i][c] = trans[i][c] * dsum / trsum[i];
  memset(trans, 0, sizeof trans);
  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
      FORC3 trans[i][j] += (i == c ? 32 : -1) * last[c][j] / 30;

  foveon_make_curves(curve, color_dq, div, cfilt);
  FORC3 chroma_dq[c] /= 3;
  foveon_make_curves(curve + 3, chroma_dq, div, cfilt);
  FORC3 dsum += chroma_dq[c] / div[c];
  curve[6] = foveon_make_curve(dsum, dsum, cfilt);
  curve[7] = foveon_make_curve(dsum * 2, dsum * 2, cfilt);

  sgain = (float(*)[3])foveon_camf_matrix(dim, "SpatialGain");
  if (!sgain)
    return;
  sgrow = (float(*)[3])calloc(dim[1], sizeof *sgrow);
  sgx = (width + dim[1] - 2) / (dim[1] - 1);

  black = (float(*)[3])calloc(height, sizeof *black);
  for (row = 0; row < height; row++)
  {
    for (i = 0; i < 6; i++)
      ((float *)ddft[0])[i] =
          ((float *)ddft[1])[i] + row / (height - 1.0) * (((float *)ddft[2])[i] - ((float *)ddft[1])[i]);
    FORC3 black[row][c] = (foveon_avg(image[row * width] + c, dscr[0], cfilt) +
                           foveon_avg(image[row * width] + c, dscr[1], cfilt) * 3 - ddft[0][c][0]) /
                              4 -
                          ddft[0][c][1];
  }
  memcpy(black, black + 8, sizeof *black * 8);
  memcpy(black + height - 11, black + height - 22, 11 * sizeof *black);
  memcpy(last, black, sizeof last);

  for (row = 1; row < height - 1; row++)
  {
    FORC3 if (last[1][c] > last[0][c])
    {
      if (last[1][c] > last[2][c])
        black[row][c] = (last[0][c] > last[2][c]) ? last[0][c] : last[2][c];
    }
    else if (last[1][c] < last[2][c]) black[row][c] = (last[0][c] < last[2][c]) ? last[0][c] : last[2][c];
    memmove(last, last + 1, 2 * sizeof last[0]);
    memcpy(last[2], black[row + 1], sizeof last[2]);
  }
  FORC3 black[row][c] = (last[0][c] + last[1][c]) / 2;
  FORC3 black[0][c] = (black[1][c] + black[3][c]) / 2;

  val = 1 - exp(-1 / 24.0);
  memcpy(fsum, black, sizeof fsum);
  for (row = 1; row < height; row++)
    FORC3 fsum[c] += black[row][c] = (black[row][c] - black[row - 1][c]) * val + black[row - 1][c];
  memcpy(last[0], black[height - 1], sizeof last[0]);
  FORC3 fsum[c] /= height;
  for (row = height; row--;)
    FORC3 last[0][c] = black[row][c] = (black[row][c] - fsum[c] - last[0][c]) * val + last[0][c];

  memset(total, 0, sizeof total);
  for (row = 2; row < height; row += 4)
    for (col = 2; col < width; col += 4)
    {
      FORC3 total[c] += (short)image[row * width + col][c];
      total[3]++;
    }
  for (row = 0; row < height; row++)
    FORC3 black[row][c] += fsum[c] / 2 + total[c] / (total[3] * 100.0);

  for (row = 0; row < height; row++)
  {
    for (i = 0; i < 6; i++)
      ((float *)ddft[0])[i] =
          ((float *)ddft[1])[i] + row / (height - 1.0) * (((float *)ddft[2])[i] - ((float *)ddft[1])[i]);
    pix = image[row * width];
    memcpy(prev, pix, sizeof prev);
    frow = row / (height - 1.0) * (dim[2] - 1);
    if ((irow = frow) == dim[2] - 1)
      irow--;
    frow -= irow;
    for (i = 0; i < dim[1]; i++)
      FORC3 sgrow[i][c] = sgain[irow * dim[1] + i][c] * (1 - frow) + sgain[(irow + 1) * dim[1] + i][c] * frow;
    for (col = 0; col < width; col++)
    {
      FORC3
      {
        diff = pix[c] - prev[c];
        prev[c] = pix[c];
        ipix[c] = pix[c] + floor((diff + (diff * diff >> 14)) * cfilt - ddft[0][c][1] -
                                 ddft[0][c][0] * ((float)col / width - 0.5) - black[row][c]);
      }
      FORC3
      {
        work[0][c] = ipix[c] * ipix[c] >> 14;
        work[2][c] = ipix[c] * work[0][c] >> 14;
        work[1][2 - c] = ipix[(c + 1) % 3] * ipix[(c + 2) % 3] >> 14;
      }
      FORC3
      {
        for (val = i = 0; i < 3; i++)
          for (j = 0; j < 3; j++)
            val += ppm[c][i][j] * work[i][j];
        ipix[c] =
            floor((ipix[c] + floor(val)) *
                  (sgrow[col / sgx][c] * (sgx - col % sgx) + sgrow[col / sgx + 1][c] * (col % sgx)) / sgx / div[c]);
        if (ipix[c] > 32000)
          ipix[c] = 32000;
        pix[c] = ipix[c];
      }
      pix += 4;
    }
  }
  free(black);
  free(sgrow);
  free(sgain);

  if ((badpix = (unsigned *)foveon_camf_matrix(dim, "BadPixels")))
  {
    for (i = 0; i < dim[0]; i++)
    {
      col = (badpix[i] >> 8 & 0xfff) - keep[0];
      row = (badpix[i] >> 20) - keep[1];
      if ((unsigned)(row - 1) > height - 3 || (unsigned)(col - 1) > width - 3)
        continue;
      memset(fsum, 0, sizeof fsum);
      for (sum = j = 0; j < 8; j++)
        if (badpix[i] & (1 << j))
        {
          FORC3 fsum[c] += (short)image[(row + hood[j * 2]) * width + col + hood[j * 2 + 1]][c];
          sum++;
        }
      if (sum)
        FORC3 image[row * width + col][c] = fsum[c] / sum;
    }
    free(badpix);
  }

  /* Array for 5x5 Gaussian averaging of red values */
  smrow[6] = (int(*)[3])calloc(width * 5, sizeof **smrow);
  merror(smrow[6], "foveon_interpolate()");
  for (i = 0; i < 5; i++)
    smrow[i] = smrow[6] + i * width;

  /* Sharpen the reds against these Gaussian averages */
  for (smlast = -1, row = 2; row < height - 2; row++)
  {
    while (smlast < row + 2)
    {
      for (i = 0; i < 6; i++)
        smrow[(i + 5) % 6] = smrow[i];
      pix = image[++smlast * width + 2];
      for (col = 2; col < width - 2; col++)
      {
        smrow[4][col][0] = (pix[0] * 6 + (pix[-4] + pix[4]) * 4 + pix[-8] + pix[8] + 8) >> 4;
        pix += 4;
      }
    }
    pix = image[row * width + 2];
    for (col = 2; col < width - 2; col++)
    {
      smred = (6 * smrow[2][col][0] + 4 * (smrow[1][col][0] + smrow[3][col][0]) + smrow[0][col][0] + smrow[4][col][0] +
               8) >>
              4;
      if (col == 2)
        smred_p = smred;
      i = pix[0] + ((pix[0] - ((smred * 7 + smred_p) >> 3)) >> 3);
      if (i > 32000)
        i = 32000;
      pix[0] = i;
      smred_p = smred;
      pix += 4;
    }
  }

  /* Adjust the brighter pixels for better linearity */
  min = 0xffff;
  FORC3
  {
    i = satlev[c] / div[c];
    if (min > i)
      min = i;
  }
  limit = min * 9 >> 4;
  for (pix = image[0]; pix < image[height * width]; pix += 4)
  {
    if (pix[0] <= limit || pix[1] <= limit || pix[2] <= limit)
      continue;
    min = max = pix[0];
    for (c = 1; c < 3; c++)
    {
      if (min > pix[c])
        min = pix[c];
      if (max < pix[c])
        max = pix[c];
    }
    if (min >= limit * 2)
    {
      pix[0] = pix[1] = pix[2] = max;
    }
    else
    {
      i = 0x4000 - ((min - limit) << 14) / limit;
      i = 0x4000 - (i * i >> 14);
      i = i * i >> 14;
      FORC3 pix[c] += (max - pix[c]) * i >> 14;
    }
  }
  /*
     Because photons that miss one detector often hit another,
     the sum R+G+B is much less noisy than the individual colors.
     So smooth the hues without smoothing the total.
   */
  for (smlast = -1, row = 2; row < height - 2; row++)
  {
    while (smlast < row + 2)
    {
      for (i = 0; i < 6; i++)
        smrow[(i + 5) % 6] = smrow[i];
      pix = image[++smlast * width + 2];
      for (col = 2; col < width - 2; col++)
      {
        FORC3 smrow[4][col][c] = (pix[c - 4] + 2 * pix[c] + pix[c + 4] + 2) >> 2;
        pix += 4;
      }
    }
    pix = image[row * width + 2];
    for (col = 2; col < width - 2; col++)
    {
      FORC3 dev[c] =
          -foveon_apply_curve(curve[7], pix[c] - ((smrow[1][col][c] + 2 * smrow[2][col][c] + smrow[3][col][c]) >> 2));
      sum = (dev[0] + dev[1] + dev[2]) >> 3;
      FORC3 pix[c] += dev[c] - sum;
      pix += 4;
    }
  }
  for (smlast = -1, row = 2; row < height - 2; row++)
  {
    while (smlast < row + 2)
    {
      for (i = 0; i < 6; i++)
        smrow[(i + 5) % 6] = smrow[i];
      pix = image[++smlast * width + 2];
      for (col = 2; col < width - 2; col++)
      {
        FORC3 smrow[4][col][c] = (pix[c - 8] + pix[c - 4] + pix[c] + pix[c + 4] + pix[c + 8] + 2) >> 2;
        pix += 4;
      }
    }
    pix = image[row * width + 2];
    for (col = 2; col < width - 2; col++)
    {
      for (total[3] = 375, sum = 60, c = 0; c < 3; c++)
      {
        for (total[c] = i = 0; i < 5; i++)
          total[c] += smrow[i][col][c];
        total[3] += total[c];
        sum += pix[c];
      }
      if (sum < 0)
        sum = 0;
      j = total[3] > 375 ? (sum << 16) / total[3] : sum * 174;
      FORC3 pix[c] += foveon_apply_curve(curve[6], ((j * total[c] + 0x8000) >> 16) - pix[c]);
      pix += 4;
    }
  }

  /* Transform the image to a different colorspace */
  for (pix = image[0]; pix < image[height * width]; pix += 4)
  {
    FORC3 pix[c] -= foveon_apply_curve(curve[c], pix[c]);
    sum = (pix[0] + pix[1] + pix[1] + pix[2]) >> 2;
    FORC3 pix[c] -= foveon_apply_curve(curve[c], pix[c] - sum);
    FORC3
    {
      for (dsum = i = 0; i < 3; i++)
        dsum += trans[c][i] * pix[i];
      if (dsum < 0)
        dsum = 0;
      if (dsum > 24000)
        dsum = 24000;
      ipix[c] = dsum + 0.5;
    }
    FORC3 pix[c] = ipix[c];
  }

  /* Smooth the image bottom-to-top and save at 1/4 scale */
  shrink = (short(*)[3])calloc((height / 4), (width / 4) * sizeof *shrink);
  merror(shrink, "foveon_interpolate()");
  for (row = height / 4; row--;)
    for (col = 0; col < width / 4; col++)
    {
      ipix[0] = ipix[1] = ipix[2] = 0;
      for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
          FORC3 ipix[c] += image[(row * 4 + i) * width + col * 4 + j][c];
      FORC3
      if (row + 2 > height / 4)
        shrink[row * (width / 4) + col][c] = ipix[c] >> 4;
      else
        shrink[row * (width / 4) + col][c] =
            (shrink[(row + 1) * (width / 4) + col][c] * 1840 + ipix[c] * 141 + 2048) >> 12;
    }
  /* From the 1/4-scale image, smooth right-to-left */
  for (row = 0; row < (height & ~3); row++)
  {
    ipix[0] = ipix[1] = ipix[2] = 0;
    if ((row & 3) == 0)
      for (col = width & ~3; col--;)
        FORC3 smrow[0][col][c] = ipix[c] =
            (shrink[(row / 4) * (width / 4) + col / 4][c] * 1485 + ipix[c] * 6707 + 4096) >> 13;

    /* Then smooth left-to-right */
    ipix[0] = ipix[1] = ipix[2] = 0;
    for (col = 0; col < (width & ~3); col++)
      FORC3 smrow[1][col][c] = ipix[c] = (smrow[0][col][c] * 1485 + ipix[c] * 6707 + 4096) >> 13;

    /* Smooth top-to-bottom */
    if (row == 0)
      memcpy(smrow[2], smrow[1], sizeof **smrow * width);
    else
      for (col = 0; col < (width & ~3); col++)
        FORC3 smrow[2][col][c] = (smrow[2][col][c] * 6707 + smrow[1][col][c] * 1485 + 4096) >> 13;

    /* Adjust the chroma toward the smooth values */
    for (col = 0; col < (width & ~3); col++)
    {
      for (i = j = 30, c = 0; c < 3; c++)
      {
        i += smrow[2][col][c];
        j += image[row * width + col][c];
      }
      j = (j << 16) / i;
      for (sum = c = 0; c < 3; c++)
      {
        ipix[c] =
            foveon_apply_curve(curve[c + 3], ((smrow[2][col][c] * j + 0x8000) >> 16) - image[row * width + col][c]);
        sum += ipix[c];
      }
      sum >>= 3;
      FORC3
      {
        i = image[row * width + col][c] + ipix[c] - sum;
        if (i < 0)
          i = 0;
        image[row * width + col][c] = i;
      }
    }
  }
  free(shrink);
  free(smrow[6]);
  for (i = 0; i < 8; i++)
    free(curve[i]);

  /* Trim off the black border */
  active[1] -= keep[1];
  active[3] -= 2;
  i = active[2] - active[0];
  for (row = 0; row < active[3] - active[1]; row++)
    memcpy(image[row * i], image[(row + active[1]) * width + active[0]], i * sizeof *image);
  width = i;
  height = row;
}
#undef image

/* RESTRICTED code ends here */

//@out COMMON
void CLASS crop_masked_pixels()
{
  int row, col;
  unsigned
#ifndef LIBRAW_LIBRARY_BUILD
      r,
      raw_pitch = raw_width * 2, c, m, mblack[8], zero, val;
#else
      c,
      m, zero, val;
#define mblack imgdata.color.black_stat
#endif

#ifndef LIBRAW_LIBRARY_BUILD
  if (load_raw == &CLASS phase_one_load_raw || load_raw == &CLASS phase_one_load_raw_c)
    phase_one_correct();
  if (fuji_width)
  {
    for (row = 0; row < raw_height - top_margin * 2; row++)
    {
      for (col = 0; col < fuji_width << !fuji_layout; col++)
      {
        if (fuji_layout)
        {
          r = fuji_width - 1 - col + (row >> 1);
          c = col + ((row + 1) >> 1);
        }
        else
        {
          r = fuji_width - 1 + row - (col >> 1);
          c = row + ((col + 1) >> 1);
        }
        if (r < height && c < width)
          BAYER(r, c) = RAW(row + top_margin, col + left_margin);
      }
    }
  }
  else
  {
    for (row = 0; row < height; row++)
      for (col = 0; col < width; col++)
        BAYER2(row, col) = RAW(row + top_margin, col + left_margin);
  }
#endif
  if (mask[0][3] > 0)
    goto mask_set;
  if (load_raw == &CLASS canon_load_raw || load_raw == &CLASS lossless_jpeg_load_raw)
  {
    mask[0][1] = mask[1][1] += 2;
    mask[0][3] -= 2;
    goto sides;
  }
  if (load_raw == &CLASS canon_600_load_raw || load_raw == &CLASS sony_load_raw ||
      (load_raw == &CLASS eight_bit_load_raw && strncmp(model, "DC2", 3)) || load_raw == &CLASS kodak_262_load_raw ||
      (load_raw == &CLASS packed_load_raw && (load_flags & 32)))
  {
  sides:
    mask[0][0] = mask[1][0] = top_margin;
    mask[0][2] = mask[1][2] = top_margin + height;
    mask[0][3] += left_margin;
    mask[1][1] += left_margin + width;
    mask[1][3] += raw_width;
  }
  if (load_raw == &CLASS nokia_load_raw)
  {
    mask[0][2] = top_margin;
    mask[0][3] = width;
  }
#ifdef LIBRAW_LIBRARY_BUILD
  if (load_raw == &CLASS broadcom_load_raw)
  {
    mask[0][2] = top_margin;
    mask[0][3] = width;
  }
#endif
mask_set:
  memset(mblack, 0, sizeof mblack);
  for (zero = m = 0; m < 8; m++)
    for (row = MAX(mask[m][0], 0); row < MIN(mask[m][2], raw_height); row++)
      for (col = MAX(mask[m][1], 0); col < MIN(mask[m][3], raw_width); col++)
      {
        c = FC(row - top_margin, col - left_margin);
        mblack[c] += val = raw_image[(row)*raw_pitch / 2 + (col)];
        mblack[4 + c]++;
        zero += !val;
      }
  if (load_raw == &CLASS canon_600_load_raw && width < raw_width)
  {
    black = (mblack[0] + mblack[1] + mblack[2] + mblack[3]) / MAX(1,(mblack[4] + mblack[5] + mblack[6] + mblack[7])) - 4;
#ifndef LIBRAW_LIBRARY_BUILD
    canon_600_correct();
#endif
  }
  else if (zero < mblack[4] && mblack[5] && mblack[6] && mblack[7])
  {
    FORC4 cblack[c] = mblack[c] / MAX(1,mblack[4 + c]);
    black = cblack[4] = cblack[5] = cblack[6] = 0;
  }
}
#ifdef LIBRAW_LIBRARY_BUILD
#undef mblack
#endif

void CLASS remove_zeroes()
{
  unsigned row, col, tot, n;
  int r, c;

#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_REMOVE_ZEROES, 0, 2);
#endif

  for (row = 0; row < height; row++)
    for (col = 0; col < width; col++)
      if (BAYER(row, col) == 0)
      {
        tot = n = 0;
        for (r = (int)row - 2; r <= (int)row + 2; r++)
          for (c = (int)col - 2; c <= (int)col + 2; c++)
            if (r >=0 && r < height && c >= 0 && c < width && FC(r, c) == FC(row, col) && BAYER(r, c))
              tot += (n++, BAYER(r, c));
        if (n)
          BAYER(row, col) = tot / n;
      }
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_REMOVE_ZEROES, 1, 2);
#endif
}
//@end COMMON

/* @out FILEIO
#include <math.h>
#define CLASS LibRaw::
#include "libraw/libraw_types.h"
#define LIBRAW_LIBRARY_BUILD
#include "libraw/libraw.h"
#include "internal/defines.h"
#include "internal/var_defines.h"
@end FILEIO */

// @out FILEIO
/*
   Search from the current directory up to the root looking for
   a ".badpixels" file, and fix those pixels now.
 */
void CLASS bad_pixels(const char *cfname)
{
  FILE *fp = NULL;
#ifndef LIBRAW_LIBRARY_BUILD
  char *fname, *cp, line[128];
  int len, time, row, col, r, c, rad, tot, n, fixed = 0;
#else
  char *cp, line[128];
  int time, row, col, r, c, rad, tot, n;
#ifdef DCRAW_VERBOSE
  int fixed = 0;
#endif
#endif

  if (!filters)
    return;
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_BAD_PIXELS, 0, 2);
#endif
  if (cfname)
    fp = fopen(cfname, "r");
  // @end FILEIO
  else
  {
    for (len = 32;; len *= 2)
    {
      fname = (char *)malloc(len);
      if (!fname)
        return;
      if (getcwd(fname, len - 16))
        break;
      free(fname);
      if (errno != ERANGE)
        return;
    }
#if defined(WIN32) || defined(DJGPP)
    if (fname[1] == ':')
      memmove(fname, fname + 2, len - 2);
    for (cp = fname; *cp; cp++)
      if (*cp == '\\')
        *cp = '/';
#endif
    cp = fname + strlen(fname);
    if (cp[-1] == '/')
      cp--;
    while (*fname == '/')
    {
      strcpy(cp, "/.badpixels");
      if ((fp = fopen(fname, "r")))
        break;
      if (cp == fname)
        break;
      while (*--cp != '/')
        ;
    }
    free(fname);
  }
  // @out FILEIO
  if (!fp)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    imgdata.process_warnings |= LIBRAW_WARN_NO_BADPIXELMAP;
#endif
    return;
  }
  while (fgets(line, 128, fp))
  {
    cp = strchr(line, '#');
    if (cp)
      *cp = 0;
    if (sscanf(line, "%d %d %d", &col, &row, &time) != 3)
      continue;
    if ((unsigned)col >= width || (unsigned)row >= height)
      continue;
    if (time > timestamp)
      continue;
    for (tot = n = 0, rad = 1; rad < 3 && n == 0; rad++)
      for (r = row - rad; r <= row + rad; r++)
        for (c = col - rad; c <= col + rad; c++)
          if ((unsigned)r < height && (unsigned)c < width && (r != row || c != col) && fcol(r, c) == fcol(row, col))
          {
            tot += BAYER2(r, c);
            n++;
          }
    if(n>0) BAYER2(row, col) = tot / n;
#ifdef DCRAW_VERBOSE
    if (verbose)
    {
      if (!fixed++)
        fprintf(stderr, _("Fixed dead pixels at:"));
      fprintf(stderr, " %d,%d", col, row);
    }
#endif
  }
#ifdef DCRAW_VERBOSE
  if (fixed)
    fputc('\n', stderr);
#endif
  fclose(fp);
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_BAD_PIXELS, 1, 2);
#endif
}

void CLASS subtract(const char *fname)
{
  FILE *fp;
  int dim[3] = {0, 0, 0}, comment = 0, number = 0, error = 0, nd = 0, c, row, col;
  ushort *pixel;
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_DARK_FRAME, 0, 2);
#endif

  if (!(fp = fopen(fname, "rb")))
  {
#ifdef DCRAW_VERBOSE
    perror(fname);
#endif
#ifdef LIBRAW_LIBRARY_BUILD
    imgdata.process_warnings |= LIBRAW_WARN_BAD_DARKFRAME_FILE;
#endif
    return;
  }
  if (fgetc(fp) != 'P' || fgetc(fp) != '5')
    error = 1;
  while (!error && nd < 3 && (c = fgetc(fp)) != EOF)
  {
    if (c == '#')
      comment = 1;
    if (c == '\n')
      comment = 0;
    if (comment)
      continue;
    if (isdigit(c))
      number = 1;
    if (number)
    {
      if (isdigit(c))
        dim[nd] = dim[nd] * 10 + c - '0';
      else if (isspace(c))
      {
        number = 0;
        nd++;
      }
      else
        error = 1;
    }
  }
  if (error || nd < 3)
  {
#ifdef DCRAW_VERBOSE
    fprintf(stderr, _("%s is not a valid PGM file!\n"), fname);
#endif
    fclose(fp);
    return;
  }
  else if (dim[0] != width || dim[1] != height || dim[2] != 65535)
  {
#ifdef DCRAW_VERBOSE
    fprintf(stderr, _("%s has the wrong dimensions!\n"), fname);
#endif
#ifdef LIBRAW_LIBRARY_BUILD
    imgdata.process_warnings |= LIBRAW_WARN_BAD_DARKFRAME_DIM;
#endif
    fclose(fp);
    return;
  }
  pixel = (ushort *)calloc(width, sizeof *pixel);
  merror(pixel, "subtract()");
  for (row = 0; row < height; row++)
  {
    fread(pixel, 2, width, fp);
    for (col = 0; col < width; col++)
      BAYER(row, col) = MAX(BAYER(row, col) - ntohs(pixel[col]), 0);
  }
  free(pixel);
  fclose(fp);
  memset(cblack, 0, sizeof cblack);
  black = 0;
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_DARK_FRAME, 1, 2);
#endif
}
//@end FILEIO

//@out COMMON

static const uchar xlat[2][256] = {
    {0xc1, 0xbf, 0x6d, 0x0d, 0x59, 0xc5, 0x13, 0x9d, 0x83, 0x61, 0x6b, 0x4f, 0xc7, 0x7f, 0x3d, 0x3d, 0x53, 0x59, 0xe3,
     0xc7, 0xe9, 0x2f, 0x95, 0xa7, 0x95, 0x1f, 0xdf, 0x7f, 0x2b, 0x29, 0xc7, 0x0d, 0xdf, 0x07, 0xef, 0x71, 0x89, 0x3d,
     0x13, 0x3d, 0x3b, 0x13, 0xfb, 0x0d, 0x89, 0xc1, 0x65, 0x1f, 0xb3, 0x0d, 0x6b, 0x29, 0xe3, 0xfb, 0xef, 0xa3, 0x6b,
     0x47, 0x7f, 0x95, 0x35, 0xa7, 0x47, 0x4f, 0xc7, 0xf1, 0x59, 0x95, 0x35, 0x11, 0x29, 0x61, 0xf1, 0x3d, 0xb3, 0x2b,
     0x0d, 0x43, 0x89, 0xc1, 0x9d, 0x9d, 0x89, 0x65, 0xf1, 0xe9, 0xdf, 0xbf, 0x3d, 0x7f, 0x53, 0x97, 0xe5, 0xe9, 0x95,
     0x17, 0x1d, 0x3d, 0x8b, 0xfb, 0xc7, 0xe3, 0x67, 0xa7, 0x07, 0xf1, 0x71, 0xa7, 0x53, 0xb5, 0x29, 0x89, 0xe5, 0x2b,
     0xa7, 0x17, 0x29, 0xe9, 0x4f, 0xc5, 0x65, 0x6d, 0x6b, 0xef, 0x0d, 0x89, 0x49, 0x2f, 0xb3, 0x43, 0x53, 0x65, 0x1d,
     0x49, 0xa3, 0x13, 0x89, 0x59, 0xef, 0x6b, 0xef, 0x65, 0x1d, 0x0b, 0x59, 0x13, 0xe3, 0x4f, 0x9d, 0xb3, 0x29, 0x43,
     0x2b, 0x07, 0x1d, 0x95, 0x59, 0x59, 0x47, 0xfb, 0xe5, 0xe9, 0x61, 0x47, 0x2f, 0x35, 0x7f, 0x17, 0x7f, 0xef, 0x7f,
     0x95, 0x95, 0x71, 0xd3, 0xa3, 0x0b, 0x71, 0xa3, 0xad, 0x0b, 0x3b, 0xb5, 0xfb, 0xa3, 0xbf, 0x4f, 0x83, 0x1d, 0xad,
     0xe9, 0x2f, 0x71, 0x65, 0xa3, 0xe5, 0x07, 0x35, 0x3d, 0x0d, 0xb5, 0xe9, 0xe5, 0x47, 0x3b, 0x9d, 0xef, 0x35, 0xa3,
     0xbf, 0xb3, 0xdf, 0x53, 0xd3, 0x97, 0x53, 0x49, 0x71, 0x07, 0x35, 0x61, 0x71, 0x2f, 0x43, 0x2f, 0x11, 0xdf, 0x17,
     0x97, 0xfb, 0x95, 0x3b, 0x7f, 0x6b, 0xd3, 0x25, 0xbf, 0xad, 0xc7, 0xc5, 0xc5, 0xb5, 0x8b, 0xef, 0x2f, 0xd3, 0x07,
     0x6b, 0x25, 0x49, 0x95, 0x25, 0x49, 0x6d, 0x71, 0xc7},
    {0xa7, 0xbc, 0xc9, 0xad, 0x91, 0xdf, 0x85, 0xe5, 0xd4, 0x78, 0xd5, 0x17, 0x46, 0x7c, 0x29, 0x4c, 0x4d, 0x03, 0xe9,
     0x25, 0x68, 0x11, 0x86, 0xb3, 0xbd, 0xf7, 0x6f, 0x61, 0x22, 0xa2, 0x26, 0x34, 0x2a, 0xbe, 0x1e, 0x46, 0x14, 0x68,
     0x9d, 0x44, 0x18, 0xc2, 0x40, 0xf4, 0x7e, 0x5f, 0x1b, 0xad, 0x0b, 0x94, 0xb6, 0x67, 0xb4, 0x0b, 0xe1, 0xea, 0x95,
     0x9c, 0x66, 0xdc, 0xe7, 0x5d, 0x6c, 0x05, 0xda, 0xd5, 0xdf, 0x7a, 0xef, 0xf6, 0xdb, 0x1f, 0x82, 0x4c, 0xc0, 0x68,
     0x47, 0xa1, 0xbd, 0xee, 0x39, 0x50, 0x56, 0x4a, 0xdd, 0xdf, 0xa5, 0xf8, 0xc6, 0xda, 0xca, 0x90, 0xca, 0x01, 0x42,
     0x9d, 0x8b, 0x0c, 0x73, 0x43, 0x75, 0x05, 0x94, 0xde, 0x24, 0xb3, 0x80, 0x34, 0xe5, 0x2c, 0xdc, 0x9b, 0x3f, 0xca,
     0x33, 0x45, 0xd0, 0xdb, 0x5f, 0xf5, 0x52, 0xc3, 0x21, 0xda, 0xe2, 0x22, 0x72, 0x6b, 0x3e, 0xd0, 0x5b, 0xa8, 0x87,
     0x8c, 0x06, 0x5d, 0x0f, 0xdd, 0x09, 0x19, 0x93, 0xd0, 0xb9, 0xfc, 0x8b, 0x0f, 0x84, 0x60, 0x33, 0x1c, 0x9b, 0x45,
     0xf1, 0xf0, 0xa3, 0x94, 0x3a, 0x12, 0x77, 0x33, 0x4d, 0x44, 0x78, 0x28, 0x3c, 0x9e, 0xfd, 0x65, 0x57, 0x16, 0x94,
     0x6b, 0xfb, 0x59, 0xd0, 0xc8, 0x22, 0x36, 0xdb, 0xd2, 0x63, 0x98, 0x43, 0xa1, 0x04, 0x87, 0x86, 0xf7, 0xa6, 0x26,
     0xbb, 0xd6, 0x59, 0x4d, 0xbf, 0x6a, 0x2e, 0xaa, 0x2b, 0xef, 0xe6, 0x78, 0xb6, 0x4e, 0xe0, 0x2f, 0xdc, 0x7c, 0xbe,
     0x57, 0x19, 0x32, 0x7e, 0x2a, 0xd0, 0xb8, 0xba, 0x29, 0x00, 0x3c, 0x52, 0x7d, 0xa8, 0x49, 0x3b, 0x2d, 0xeb, 0x25,
     0x49, 0xfa, 0xa3, 0xaa, 0x39, 0xa7, 0xc5, 0xa7, 0x50, 0x11, 0x36, 0xfb, 0xc6, 0x67, 0x4a, 0xf5, 0xa5, 0x12, 0x65,
     0x7e, 0xb0, 0xdf, 0xaf, 0x4e, 0xb3, 0x61, 0x7f, 0x2f}};

void CLASS gamma_curve(double pwr, double ts, int mode, int imax)
{
  int i;
  double g[6], bnd[2] = {0, 0}, r;

  g[0] = pwr;
  g[1] = ts;
  g[2] = g[3] = g[4] = 0;
  bnd[g[1] >= 1] = 1;
  if (g[1] && (g[1] - 1) * (g[0] - 1) <= 0)
  {
    for (i = 0; i < 48; i++)
    {
      g[2] = (bnd[0] + bnd[1]) / 2;
      if (g[0])
        bnd[(pow(g[2] / g[1], -g[0]) - 1) / g[0] - 1 / g[2] > -1] = g[2];
      else
        bnd[g[2] / exp(1 - 1 / g[2]) < g[1]] = g[2];
    }
    g[3] = g[2] / g[1];
    if (g[0])
      g[4] = g[2] * (1 / g[0] - 1);
  }
  if (g[0])
    g[5] = 1 / (g[1] * SQR(g[3]) / 2 - g[4] * (1 - g[3]) + (1 - pow(g[3], 1 + g[0])) * (1 + g[4]) / (1 + g[0])) - 1;
  else
    g[5] = 1 / (g[1] * SQR(g[3]) / 2 + 1 - g[2] - g[3] - g[2] * g[3] * (log(g[3]) - 1)) - 1;
  if (!mode--)
  {
    memcpy(gamm, g, sizeof gamm);
    return;
  }
  for (i = 0; i < 0x10000; i++)
  {
    curve[i] = 0xffff;
    if ((r = (double)i / imax) < 1)
      curve[i] = 0x10000 *
                 (mode ? (r < g[3] ? r * g[1] : (g[0] ? pow(r, g[0]) * (1 + g[4]) - g[4] : log(r) * g[2] + 1))
                       : (r < g[2] ? r / g[1] : (g[0] ? pow((r + g[4]) / (1 + g[4]), 1 / g[0]) : exp((r - 1) / g[2]))));
  }
}

void CLASS pseudoinverse(double (*in)[3], double (*out)[3], int size)
{
  double work[3][6], num;
  int i, j, k;

  for (i = 0; i < 3; i++)
  {
    for (j = 0; j < 6; j++)
      work[i][j] = j == i + 3;
    for (j = 0; j < 3; j++)
      for (k = 0; k < size; k++)
        work[i][j] += in[k][i] * in[k][j];
  }
  for (i = 0; i < 3; i++)
  {
    num = work[i][i];
    for (j = 0; j < 6; j++)
      if(fabs(num)>0.00001f)
      	work[i][j] /= num;
    for (k = 0; k < 3; k++)
    {
      if (k == i)
        continue;
      num = work[k][i];
      for (j = 0; j < 6; j++)
        work[k][j] -= work[i][j] * num;
    }
  }
  for (i = 0; i < size; i++)
    for (j = 0; j < 3; j++)
      for (out[i][j] = k = 0; k < 3; k++)
        out[i][j] += work[j][k + 3] * in[i][k];
}

void CLASS cam_xyz_coeff(float _rgb_cam[3][4], double cam_xyz[4][3])
{
  double cam_rgb[4][3], inverse[4][3], num;
  int i, j, k;

  for (i = 0; i < colors; i++) /* Multiply out XYZ colorspace */
    for (j = 0; j < 3; j++)
      for (cam_rgb[i][j] = k = 0; k < 3; k++)
        cam_rgb[i][j] += cam_xyz[i][k] * xyz_rgb[k][j];

  for (i = 0; i < colors; i++)
  {                               /* Normalize cam_rgb so that */
    for (num = j = 0; j < 3; j++) /* cam_rgb * (1,1,1) is (1,1,1,1) */
      num += cam_rgb[i][j];
    if (num > 0.00001)
    {
      for (j = 0; j < 3; j++)
        cam_rgb[i][j] /= num;
      pre_mul[i] = 1 / num;
    }
    else
    {
      for (j = 0; j < 3; j++)
        cam_rgb[i][j] = 0.0;
      pre_mul[i] = 1.0;
    }
  }
  pseudoinverse(cam_rgb, inverse, colors);
  for (i = 0; i < 3; i++)
    for (j = 0; j < colors; j++)
      _rgb_cam[i][j] = inverse[j][i];
}

#ifdef COLORCHECK
void CLASS colorcheck()
{
#define NSQ 24
  // Coordinates of the GretagMacbeth ColorChecker squares
  // width, height, 1st_column, 1st_row
  int cut[NSQ][4];                                             // you must set these
                                                               // ColorChecker Chart under 6500-kelvin illumination
  static const double gmb_xyY[NSQ][3] = {{0.400, 0.350, 10.1}, // Dark Skin
                                         {0.377, 0.345, 35.8}, // Light Skin
                                         {0.247, 0.251, 19.3}, // Blue Sky
                                         {0.337, 0.422, 13.3}, // Foliage
                                         {0.265, 0.240, 24.3}, // Blue Flower
                                         {0.261, 0.343, 43.1}, // Bluish Green
                                         {0.506, 0.407, 30.1}, // Orange
                                         {0.211, 0.175, 12.0}, // Purplish Blue
                                         {0.453, 0.306, 19.8}, // Moderate Red
                                         {0.285, 0.202, 6.6},  // Purple
                                         {0.380, 0.489, 44.3}, // Yellow Green
                                         {0.473, 0.438, 43.1}, // Orange Yellow
                                         {0.187, 0.129, 6.1},  // Blue
                                         {0.305, 0.478, 23.4}, // Green
                                         {0.539, 0.313, 12.0}, // Red
                                         {0.448, 0.470, 59.1}, // Yellow
                                         {0.364, 0.233, 19.8}, // Magenta
                                         {0.196, 0.252, 19.8}, // Cyan
                                         {0.310, 0.316, 90.0}, // White
                                         {0.310, 0.316, 59.1}, // Neutral 8
                                         {0.310, 0.316, 36.2}, // Neutral 6.5
                                         {0.310, 0.316, 19.8}, // Neutral 5
                                         {0.310, 0.316, 9.0},  // Neutral 3.5
                                         {0.310, 0.316, 3.1}}; // Black
  double gmb_cam[NSQ][4], gmb_xyz[NSQ][3];
  double inverse[NSQ][3], cam_xyz[4][3], balance[4], num;
  int c, i, j, k, sq, row, col, pass, count[4];

  memset(gmb_cam, 0, sizeof gmb_cam);
  for (sq = 0; sq < NSQ; sq++)
  {
    FORCC count[c] = 0;
    for (row = cut[sq][3]; row < cut[sq][3] + cut[sq][1]; row++)
      for (col = cut[sq][2]; col < cut[sq][2] + cut[sq][0]; col++)
      {
        c = FC(row, col);
        if (c >= colors)
          c -= 2;
        gmb_cam[sq][c] += BAYER2(row, col);
        BAYER2(row, col) = black + (BAYER2(row, col) - black) / 2;
        count[c]++;
      }
    FORCC gmb_cam[sq][c] = gmb_cam[sq][c] / count[c] - black;
    gmb_xyz[sq][0] = gmb_xyY[sq][2] * gmb_xyY[sq][0] / gmb_xyY[sq][1];
    gmb_xyz[sq][1] = gmb_xyY[sq][2];
    gmb_xyz[sq][2] = gmb_xyY[sq][2] * (1 - gmb_xyY[sq][0] - gmb_xyY[sq][1]) / gmb_xyY[sq][1];
  }
  pseudoinverse(gmb_xyz, inverse, NSQ);
  for (pass = 0; pass < 2; pass++)
  {
    for (raw_color = i = 0; i < colors; i++)
      for (j = 0; j < 3; j++)
        for (cam_xyz[i][j] = k = 0; k < NSQ; k++)
          cam_xyz[i][j] += gmb_cam[k][i] * inverse[k][j];
    cam_xyz_coeff(rgb_cam, cam_xyz);
    FORCC balance[c] = pre_mul[c] * gmb_cam[20][c];
    for (sq = 0; sq < NSQ; sq++)
      FORCC gmb_cam[sq][c] *= balance[c];
  }
  if (verbose)
  {
    printf("    { \"%s %s\", %d,\n\t{", make, model, black);
    num = 10000 / (cam_xyz[1][0] + cam_xyz[1][1] + cam_xyz[1][2]);
    FORCC for (j = 0; j < 3; j++) printf("%c%d", (c | j) ? ',' : ' ', (int)(cam_xyz[c][j] * num + 0.5));
    puts(" } },");
  }
#undef NSQ
}
#endif

void CLASS hat_transform(float *temp, float *base, int st, int size, int sc)
{
  int i;
  for (i = 0; i < sc; i++)
    temp[i] = 2 * base[st * i] + base[st * (sc - i)] + base[st * (i + sc)];
  for (; i + sc < size; i++)
    temp[i] = 2 * base[st * i] + base[st * (i - sc)] + base[st * (i + sc)];
  for (; i < size; i++)
    temp[i] = 2 * base[st * i] + base[st * (i - sc)] + base[st * (2 * size - 2 - (i + sc))];
}

#if !defined(LIBRAW_USE_OPENMP) || !defined(LIBRAW_LIBRARY_BUILD)
void CLASS wavelet_denoise()
{
  float *fimg = 0, *temp, thold, mul[2], avg, diff;
  int scale = 1, size, lev, hpass, lpass, row, col, nc, c, i, wlast, blk[2];
  ushort *window[4];
  static const float noise[] = {0.8002, 0.2735, 0.1202, 0.0585, 0.0291, 0.0152, 0.0080, 0.0044};

#ifdef DCRAW_VERBOSE
  if (verbose)
    fprintf(stderr, _("Wavelet denoising...\n"));
#endif

  while (maximum << scale < 0x10000)
    scale++;
  maximum <<= --scale;
  black <<= scale;
  FORC4 cblack[c] <<= scale;
  if ((size = iheight * iwidth) < 0x15550000)
    fimg = (float *)malloc((size * 3 + iheight + iwidth) * sizeof *fimg);
  merror(fimg, "wavelet_denoise()");
  temp = fimg + size * 3;
  if ((nc = colors) == 3 && filters)
    nc++;
  FORC(nc)
  { /* denoise R,G1,B,G3 individually */
    for (i = 0; i < size; i++)
      fimg[i] = 256 * sqrt((double)(image[i][c] << scale));
    for (hpass = lev = 0; lev < 5; lev++)
    {
      lpass = size * ((lev & 1) + 1);
      for (row = 0; row < iheight; row++)
      {
        hat_transform(temp, fimg + hpass + row * iwidth, 1, iwidth, 1 << lev);
        for (col = 0; col < iwidth; col++)
          fimg[lpass + row * iwidth + col] = temp[col] * 0.25;
      }
      for (col = 0; col < iwidth; col++)
      {
        hat_transform(temp, fimg + lpass + col, iwidth, iheight, 1 << lev);
        for (row = 0; row < iheight; row++)
          fimg[lpass + row * iwidth + col] = temp[row] * 0.25;
      }
      thold = threshold * noise[lev];
      for (i = 0; i < size; i++)
      {
        fimg[hpass + i] -= fimg[lpass + i];
        if (fimg[hpass + i] < -thold)
          fimg[hpass + i] += thold;
        else if (fimg[hpass + i] > thold)
          fimg[hpass + i] -= thold;
        else
          fimg[hpass + i] = 0;
        if (hpass)
          fimg[i] += fimg[hpass + i];
      }
      hpass = lpass;
    }
    for (i = 0; i < size; i++)
      image[i][c] = CLIP(SQR(fimg[i] + fimg[lpass + i]) / 0x10000);
  }
  if (filters && colors == 3)
  { /* pull G1 and G3 closer together */
    for (row = 0; row < 2; row++)
    {
      mul[row] = 0.125 * pre_mul[FC(row + 1, 0) | 1] / pre_mul[FC(row, 0) | 1];
      blk[row] = cblack[FC(row, 0) | 1];
    }
    for (i = 0; i < 4; i++)
      window[i] = (ushort *)fimg + width * i;
    for (wlast = -1, row = 1; row < height - 1; row++)
    {
      while (wlast < row + 1)
      {
        for (wlast++, i = 0; i < 4; i++)
          window[(i + 3) & 3] = window[i];
        for (col = FC(wlast, 1) & 1; col < width; col += 2)
          window[2][col] = BAYER(wlast, col);
      }
      thold = threshold / 512;
      for (col = (FC(row, 0) & 1) + 1; col < width - 1; col += 2)
      {
        avg = (window[0][col - 1] + window[0][col + 1] + window[2][col - 1] + window[2][col + 1] - blk[~row & 1] * 4) *
                  mul[row & 1] +
              (window[1][col] + blk[row & 1]) * 0.5;
        avg = avg < 0 ? 0 : sqrt(avg);
        diff = sqrt((double)BAYER(row, col)) - avg;
        if (diff < -thold)
          diff += thold;
        else if (diff > thold)
          diff -= thold;
        else
          diff = 0;
        BAYER(row, col) = CLIP(SQR(avg + diff) + 0.5);
      }
    }
  }
  free(fimg);
}
#else /* LIBRAW_USE_OPENMP and LIBRAW_LIBRARY_BUILD */
void CLASS wavelet_denoise()
{
  float *fimg = 0, *temp, thold, mul[2], avg, diff;
  int scale = 1, size, lev, hpass, lpass, row, col, nc, c, i, wlast, blk[2];
  ushort *window[4];
  static const float noise[] = {0.8002, 0.2735, 0.1202, 0.0585, 0.0291, 0.0152, 0.0080, 0.0044};

#ifdef DCRAW_VERBOSE
  if (verbose)
    fprintf(stderr, _("Wavelet denoising...\n"));
#endif

  while (maximum << scale < 0x10000)
    scale++;
  maximum <<= --scale;
  black <<= scale;
  FORC4 cblack[c] <<= scale;
  if ((size = iheight * iwidth) < 0x15550000)
    fimg = (float *)malloc((size * 3 + iheight + iwidth) * sizeof *fimg);
  merror(fimg, "wavelet_denoise()");
  temp = fimg + size * 3;
  if ((nc = colors) == 3 && filters)
    nc++;
#pragma omp parallel default(shared) private(i, col, row, thold, lev, lpass, hpass, temp, c) firstprivate(scale, size)
  {
#pragma omp critical  /* LibRaw's malloc is not local thread-safe */
    temp = (float *)malloc((iheight + iwidth) * sizeof *fimg);
    FORC(nc)
    { /* denoise R,G1,B,G3 individually */
#pragma omp for
      for (i = 0; i < size; i++)
        fimg[i] = 256 * sqrt((double)(image[i][c] << scale));
      for (hpass = lev = 0; lev < 5; lev++)
      {
        lpass = size * ((lev & 1) + 1);
#pragma omp for
        for (row = 0; row < iheight; row++)
        {
          hat_transform(temp, fimg + hpass + row * iwidth, 1, iwidth, 1 << lev);
          for (col = 0; col < iwidth; col++)
            fimg[lpass + row * iwidth + col] = temp[col] * 0.25;
        }
#pragma omp for
        for (col = 0; col < iwidth; col++)
        {
          hat_transform(temp, fimg + lpass + col, iwidth, iheight, 1 << lev);
          for (row = 0; row < iheight; row++)
            fimg[lpass + row * iwidth + col] = temp[row] * 0.25;
        }
        thold = threshold * noise[lev];
#pragma omp for
        for (i = 0; i < size; i++)
        {
          fimg[hpass + i] -= fimg[lpass + i];
          if (fimg[hpass + i] < -thold)
            fimg[hpass + i] += thold;
          else if (fimg[hpass + i] > thold)
            fimg[hpass + i] -= thold;
          else
            fimg[hpass + i] = 0;
          if (hpass)
            fimg[i] += fimg[hpass + i];
        }
        hpass = lpass;
      }
#pragma omp for
      for (i = 0; i < size; i++)
        image[i][c] = CLIP(SQR(fimg[i] + fimg[lpass + i]) / 0x10000);
    }
#pragma omp critical
    free(temp);
  } /* end omp parallel */
  /* the following loops are hard to parallize, no idea yes,
   * problem is wlast which is carrying dependency
   * second part should be easyer, but did not yet get it right.
   */
  if (filters && colors == 3)
  { /* pull G1 and G3 closer together */
    for (row = 0; row < 2; row++)
    {
      mul[row] = 0.125 * pre_mul[FC(row + 1, 0) | 1] / pre_mul[FC(row, 0) | 1];
      blk[row] = cblack[FC(row, 0) | 1];
    }
    for (i = 0; i < 4; i++)
      window[i] = (ushort *)fimg + width * i;
    for (wlast = -1, row = 1; row < height - 1; row++)
    {
      while (wlast < row + 1)
      {
        for (wlast++, i = 0; i < 4; i++)
          window[(i + 3) & 3] = window[i];
        for (col = FC(wlast, 1) & 1; col < width; col += 2)
          window[2][col] = BAYER(wlast, col);
      }
      thold = threshold / 512;
      for (col = (FC(row, 0) & 1) + 1; col < width - 1; col += 2)
      {
        avg = (window[0][col - 1] + window[0][col + 1] + window[2][col - 1] + window[2][col + 1] - blk[~row & 1] * 4) *
                  mul[row & 1] +
              (window[1][col] + blk[row & 1]) * 0.5;
        avg = avg < 0 ? 0 : sqrt(avg);
        diff = sqrt((double)BAYER(row, col)) - avg;
        if (diff < -thold)
          diff += thold;
        else if (diff > thold)
          diff -= thold;
        else
          diff = 0;
        BAYER(row, col) = CLIP(SQR(avg + diff) + 0.5);
      }
    }
  }
  free(fimg);
}

#endif

// green equilibration
void CLASS green_matching()
{
  int i, j;
  double m1, m2, c1, c2;
  int o1_1, o1_2, o1_3, o1_4;
  int o2_1, o2_2, o2_3, o2_4;
  ushort(*img)[4];
  const int margin = 3;
  int oj = 2, oi = 2;
  float f;
  const float thr = 0.01f;
  if (half_size || shrink)
    return;
  if (FC(oj, oi) != 3)
    oj++;
  if (FC(oj, oi) != 3)
    oi++;
  if (FC(oj, oi) != 3)
    oj--;

  img = (ushort(*)[4])calloc(height * width, sizeof *image);
  merror(img, "green_matching()");
  memcpy(img, image, height * width * sizeof *image);

  for (j = oj; j < height - margin; j += 2)
    for (i = oi; i < width - margin; i += 2)
    {
      o1_1 = img[(j - 1) * width + i - 1][1];
      o1_2 = img[(j - 1) * width + i + 1][1];
      o1_3 = img[(j + 1) * width + i - 1][1];
      o1_4 = img[(j + 1) * width + i + 1][1];
      o2_1 = img[(j - 2) * width + i][3];
      o2_2 = img[(j + 2) * width + i][3];
      o2_3 = img[j * width + i - 2][3];
      o2_4 = img[j * width + i + 2][3];

      m1 = (o1_1 + o1_2 + o1_3 + o1_4) / 4.0;
      m2 = (o2_1 + o2_2 + o2_3 + o2_4) / 4.0;

      c1 = (abs(o1_1 - o1_2) + abs(o1_1 - o1_3) + abs(o1_1 - o1_4) + abs(o1_2 - o1_3) + abs(o1_3 - o1_4) +
            abs(o1_2 - o1_4)) /
           6.0;
      c2 = (abs(o2_1 - o2_2) + abs(o2_1 - o2_3) + abs(o2_1 - o2_4) + abs(o2_2 - o2_3) + abs(o2_3 - o2_4) +
            abs(o2_2 - o2_4)) /
           6.0;
      if ((img[j * width + i][3] < maximum * 0.95) && (c1 < maximum * thr) && (c2 < maximum * thr))
      {
        f = image[j * width + i][3] * m1 / m2;
        image[j * width + i][3] = f > 0xffff ? 0xffff : f;
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
  ushort *img = 0, *pix;

#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_SCALE_COLORS, 0, 2);
#endif

  if (user_mul[0])
    memcpy(pre_mul, user_mul, sizeof pre_mul);
  if (use_auto_wb || (use_camera_wb && cam_mul[0] <= 0.001f))
  {
    memset(dsum, 0, sizeof dsum);
    bottom = MIN(greybox[1] + greybox[3], height);
    right = MIN(greybox[0] + greybox[2], width);
    for (row = greybox[1]; row < bottom; row += 8)
      for (col = greybox[0]; col < right; col += 8)
      {
        memset(sum, 0, sizeof sum);
        for (y = row; y < row + 8 && y < bottom; y++)
          for (x = col; x < col + 8 && x < right; x++)
            FORC4
            {
              if (filters)
              {
                c = fcol(y, x);
                val = BAYER2(y, x);
              }
              else
                val = image[y * width + x][c];
              if (val > maximum - 25)
                goto skip_block;
              if ((val -= cblack[c]) < 0)
                val = 0;
              sum[c] += val;
              sum[c + 4]++;
              if (filters)
                break;
            }
        FORC(8) dsum[c] += sum[c];
      skip_block:;
      }
    FORC4 if (dsum[c]) pre_mul[c] = dsum[c + 4] / dsum[c];
  }
  if (use_camera_wb && cam_mul[0] > 0.001f)
  {
    memset(sum, 0, sizeof sum);
    for (row = 0; row < 8; row++)
      for (col = 0; col < 8; col++)
      {
        c = FC(row, col);
        if ((val = white[row][col] - cblack[c]) > 0)
          sum[c] += val;
        sum[c + 4]++;
      }
#ifdef LIBRAW_LIBRARY_BUILD
    if (imgdata.color.as_shot_wb_applied)
    {
      // Nikon sRAW: camera WB already applied:
      pre_mul[0] = pre_mul[1] = pre_mul[2] = pre_mul[3] = 1.0;
    }
    else
#endif
        if (sum[0] && sum[1] && sum[2] && sum[3])
      FORC4 pre_mul[c] = (float)sum[c + 4] / sum[c];
    else if (cam_mul[0] > 0.001f && cam_mul[2] > 0.001f)
      memcpy(pre_mul, cam_mul, sizeof pre_mul);
    else
    {
#ifdef LIBRAW_LIBRARY_BUILD
      imgdata.process_warnings |= LIBRAW_WARN_BAD_CAMERA_WB;
#endif
#ifdef DCRAW_VERBOSE
      fprintf(stderr, _("%s: Cannot use camera white balance.\n"), ifname);
#endif
    }
  }
#ifdef LIBRAW_LIBRARY_BUILD
  // Nikon sRAW, daylight
  if (imgdata.color.as_shot_wb_applied && !use_camera_wb
       && !use_auto_wb && cam_mul[0] > 0.001f &&
      cam_mul[1] > 0.001f && cam_mul[2] > 0.001f)
  {
    for (c = 0; c < 3; c++)
      pre_mul[c] /= cam_mul[c];
  }
#endif
  if (pre_mul[1] == 0)
    pre_mul[1] = 1;
  if (pre_mul[3] == 0)
    pre_mul[3] = colors < 4 ? pre_mul[1] : 1;
  dark = black;
  sat = maximum;
  if (threshold)
    wavelet_denoise();
  maximum -= black;
  for (dmin = DBL_MAX, dmax = c = 0; c < 4; c++)
  {
    if (dmin > pre_mul[c])
      dmin = pre_mul[c];
    if (dmax < pre_mul[c])
      dmax = pre_mul[c];
  }
  if (!highlight)
    dmax = dmin;
  FORC4 scale_mul[c] = (pre_mul[c] /= dmax) * 65535.0 / maximum;
#ifdef DCRAW_VERBOSE
  if (verbose)
  {
    fprintf(stderr, _("Scaling with darkness %d, saturation %d, and\nmultipliers"), dark, sat);
    FORC4 fprintf(stderr, " %f", pre_mul[c]);
    fputc('\n', stderr);
  }
#endif
  if (filters > 1000 && (cblack[4] + 1) / 2 == 1 && (cblack[5] + 1) / 2 == 1)
  {
    FORC4 cblack[FC(c / 2, c % 2)] += cblack[6 + c / 2 % cblack[4] * cblack[5] + c % 2 % cblack[5]];
    cblack[4] = cblack[5] = 0;
  }
  size = iheight * iwidth;
#ifdef LIBRAW_LIBRARY_BUILD
  scale_colors_loop(scale_mul);
#else
  for (i = 0; i < size * 4; i++)
  {
    if (!(val = ((ushort *)image)[i]))
      continue;
    if (cblack[4] && cblack[5])
      val -= cblack[6 + i / 4 / iwidth % cblack[4] * cblack[5] + i / 4 % iwidth % cblack[5]];
    val -= cblack[i & 3];
    val *= scale_mul[i & 3];
    ((ushort *)image)[i] = CLIP(val);
  }
#endif
  if ((aber[0] != 1 || aber[2] != 1) && colors == 3)
  {
#ifdef DCRAW_VERBOSE
    if (verbose)
      fprintf(stderr, _("Correcting chromatic aberration...\n"));
#endif
    for (c = 0; c < 4; c += 2)
    {
      if (aber[c] == 1)
        continue;
      img = (ushort *)malloc(size * sizeof *img);
      merror(img, "scale_colors()");
      for (i = 0; i < size; i++)
        img[i] = image[i][c];
      for (row = 0; row < iheight; row++)
      {
        ur = fr = (row - iheight * 0.5) * aber[c] + iheight * 0.5;
        if (ur > iheight - 2)
          continue;
        fr -= ur;
        for (col = 0; col < iwidth; col++)
        {
          uc = fc = (col - iwidth * 0.5) * aber[c] + iwidth * 0.5;
          if (uc > iwidth - 2)
            continue;
          fc -= uc;
          pix = img + ur * iwidth + uc;
          image[row * iwidth + col][c] =
              (pix[0] * (1 - fc) + pix[1] * fc) * (1 - fr) + (pix[iwidth] * (1 - fc) + pix[iwidth + 1] * fc) * fr;
        }
      }
      free(img);
    }
  }
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_SCALE_COLORS, 1, 2);
#endif
}

void CLASS pre_interpolate()
{
  ushort(*img)[4];
  int row, col, c;
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_PRE_INTERPOLATE, 0, 2);
#endif
  if (shrink)
  {
    if (half_size)
    {
      height = iheight;
      width = iwidth;
      if (filters == 9)
      {
        for (row = 0; row < 3; row++)
          for (col = 1; col < 4; col++)
            if (!(image[row * width + col][0] | image[row * width + col][2]))
              goto break2;
      break2:
        for (; row < height; row += 3)
          for (col = (col - 1) % 3 + 1; col < width - 1; col += 3)
          {
            img = image + row * width + col;
            for (c = 0; c < 3; c += 2)
              img[0][c] = (img[-1][c] + img[1][c]) >> 1;
          }
      }
    }
    else
    {
      img = (ushort(*)[4])calloc(height, width * sizeof *img);
      merror(img, "pre_interpolate()");
      for (row = 0; row < height; row++)
        for (col = 0; col < width; col++)
        {
          c = fcol(row, col);
          img[row * width + col][c] = image[(row >> 1) * iwidth + (col >> 1)][c];
        }
      free(image);
      image = img;
      shrink = 0;
    }
  }
  if (filters > 1000 && colors == 3)
  {
    mix_green = four_color_rgb ^ half_size;
    if (four_color_rgb | half_size)
      colors++;
    else
    {
      for (row = FC(1, 0) >> 1; row < height; row += 2)
        for (col = FC(row, 1) & 1; col < width; col += 2)
          image[row * width + col][1] = image[row * width + col][3];
      filters &= ~((filters & 0x55555555U) << 1);
    }
  }
  if (half_size)
    filters = 0;
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_PRE_INTERPOLATE, 1, 2);
#endif
}

void CLASS border_interpolate(int border)
{
  unsigned row, col, y, x, f, c, sum[8];

  for (row = 0; row < height; row++)
    for (col = 0; col < width; col++)
    {
      if (col == border && row >= border && row < height - border)
        col = width - border;
      memset(sum, 0, sizeof sum);
      for (y = row - 1; y != row + 2; y++)
        for (x = col - 1; x != col + 2; x++)
          if (y < height && x < width)
          {
            f = fcol(y, x);
            sum[f] += image[y * width + x][f];
            sum[f + 4]++;
          }
      f = fcol(row, col);
      FORCC if (c != f && sum[c + 4]) image[row * width + col][c] = sum[c] / sum[c + 4];
    }
}

void CLASS lin_interpolate_loop(int code[16][16][32], int size)
{
  int row;
  for (row = 1; row < height - 1; row++)
  {
    int col, *ip;
    ushort *pix;
    for (col = 1; col < width - 1; col++)
    {
      int i;
      int sum[4];
      pix = image[row * width + col];
      ip = code[row % size][col % size];
      memset(sum, 0, sizeof sum);
      for (i = *ip++; i--; ip += 3)
        sum[ip[2]] += pix[ip[0]] << ip[1];
      for (i = colors; --i; ip += 2)
        pix[ip[0]] = sum[ip[0]] * ip[1] >> 8;
    }
  }
}

void CLASS lin_interpolate()
{
  int code[16][16][32], size = 16, *ip, sum[4];
  int f, c, x, y, row, col, shift, color;

#ifdef DCRAW_VERBOSE
  if (verbose)
    fprintf(stderr, _("Bilinear interpolation...\n"));
#endif
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_INTERPOLATE, 0, 3);
#endif

  if (filters == 9)
    size = 6;
  border_interpolate(1);
  for (row = 0; row < size; row++)
    for (col = 0; col < size; col++)
    {
      ip = code[row][col] + 1;
      f = fcol(row, col);
      memset(sum, 0, sizeof sum);
      for (y = -1; y <= 1; y++)
        for (x = -1; x <= 1; x++)
        {
          shift = (y == 0) + (x == 0);
          color = fcol(row + y, col + x);
          if (color == f)
            continue;
          *ip++ = (width * y + x) * 4 + color;
          *ip++ = shift;
          *ip++ = color;
          sum[color] += 1 << shift;
        }
      code[row][col][0] = (ip - code[row][col]) / 3;
      FORCC
      if (c != f)
      {
        *ip++ = c;
        *ip++ = sum[c] > 0 ? 256 / sum[c] : 0;
      }
    }
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_INTERPOLATE, 1, 3);
#endif
  lin_interpolate_loop(code, size);
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_INTERPOLATE, 2, 3);
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
  static const signed char *cp,
      terms[] = {-2, -2, +0, -1, 0, 0x01, -2, -2, +0, +0, 1, 0x01, -2, -1, -1, +0, 0, 0x01, -2, -1, +0, -1, 0, 0x02,
                 -2, -1, +0, +0, 0, 0x03, -2, -1, +0, +1, 1, 0x01, -2, +0, +0, -1, 0, 0x06, -2, +0, +0, +0, 1, 0x02,
                 -2, +0, +0, +1, 0, 0x03, -2, +1, -1, +0, 0, 0x04, -2, +1, +0, -1, 1, 0x04, -2, +1, +0, +0, 0, 0x06,
                 -2, +1, +0, +1, 0, 0x02, -2, +2, +0, +0, 1, 0x04, -2, +2, +0, +1, 0, 0x04, -1, -2, -1, +0, 0, -128,
                 -1, -2, +0, -1, 0, 0x01, -1, -2, +1, -1, 0, 0x01, -1, -2, +1, +0, 1, 0x01, -1, -1, -1, +1, 0, -120,
                 -1, -1, +1, -2, 0, 0x40, -1, -1, +1, -1, 0, 0x22, -1, -1, +1, +0, 0, 0x33, -1, -1, +1, +1, 1, 0x11,
                 -1, +0, -1, +2, 0, 0x08, -1, +0, +0, -1, 0, 0x44, -1, +0, +0, +1, 0, 0x11, -1, +0, +1, -2, 1, 0x40,
                 -1, +0, +1, -1, 0, 0x66, -1, +0, +1, +0, 1, 0x22, -1, +0, +1, +1, 0, 0x33, -1, +0, +1, +2, 1, 0x10,
                 -1, +1, +1, -1, 1, 0x44, -1, +1, +1, +0, 0, 0x66, -1, +1, +1, +1, 0, 0x22, -1, +1, +1, +2, 0, 0x10,
                 -1, +2, +0, +1, 0, 0x04, -1, +2, +1, +0, 1, 0x04, -1, +2, +1, +1, 0, 0x04, +0, -2, +0, +0, 1, -128,
                 +0, -1, +0, +1, 1, -120, +0, -1, +1, -2, 0, 0x40, +0, -1, +1, +0, 0, 0x11, +0, -1, +2, -2, 0, 0x40,
                 +0, -1, +2, -1, 0, 0x20, +0, -1, +2, +0, 0, 0x30, +0, -1, +2, +1, 1, 0x10, +0, +0, +0, +2, 1, 0x08,
                 +0, +0, +2, -2, 1, 0x40, +0, +0, +2, -1, 0, 0x60, +0, +0, +2, +0, 1, 0x20, +0, +0, +2, +1, 0, 0x30,
                 +0, +0, +2, +2, 1, 0x10, +0, +1, +1, +0, 0, 0x44, +0, +1, +1, +2, 0, 0x10, +0, +1, +2, -1, 1, 0x40,
                 +0, +1, +2, +0, 0, 0x60, +0, +1, +2, +1, 0, 0x20, +0, +1, +2, +2, 0, 0x10, +1, -2, +1, +0, 0, -128,
                 +1, -1, +1, +1, 0, -120, +1, +0, +1, +2, 0, 0x08, +1, +0, +2, -1, 0, 0x40, +1, +0, +2, +1, 0, 0x10},
      chood[] = {-1, -1, -1, 0, -1, +1, 0, +1, +1, +1, +1, 0, +1, -1, 0, -1};
  ushort(*brow[5])[4], *pix;
  int prow = 8, pcol = 2, *ip, *code[16][16], gval[8], gmin, gmax, sum[4];
  int row, col, x, y, x1, x2, y1, y2, t, weight, grads, color, diag;
  int g, diff, thold, num, c;

  lin_interpolate();
#ifdef DCRAW_VERBOSE
  if (verbose)
    fprintf(stderr, _("VNG interpolation...\n"));
#endif

  if (filters == 1)
    prow = pcol = 16;
  if (filters == 9)
    prow = pcol = 6;
  ip = (int *)calloc(prow * pcol, 1280);
  merror(ip, "vng_interpolate()");
  for (row = 0; row < prow; row++) /* Precalculate for VNG */
    for (col = 0; col < pcol; col++)
    {
      code[row][col] = ip;
      for (cp = terms, t = 0; t < 64; t++)
      {
        y1 = *cp++;
        x1 = *cp++;
        y2 = *cp++;
        x2 = *cp++;
        weight = *cp++;
        grads = *cp++;
        color = fcol(row + y1, col + x1);
        if (fcol(row + y2, col + x2) != color)
          continue;
        diag = (fcol(row, col + 1) == color && fcol(row + 1, col) == color) ? 2 : 1;
        if (abs(y1 - y2) == diag && abs(x1 - x2) == diag)
          continue;
        *ip++ = (y1 * width + x1) * 4 + color;
        *ip++ = (y2 * width + x2) * 4 + color;
        *ip++ = weight;
        for (g = 0; g < 8; g++)
          if (grads & 1 << g)
            *ip++ = g;
        *ip++ = -1;
      }
      *ip++ = INT_MAX;
      for (cp = chood, g = 0; g < 8; g++)
      {
        y = *cp++;
        x = *cp++;
        *ip++ = (y * width + x) * 4;
        color = fcol(row, col);
        if (fcol(row + y, col + x) != color && fcol(row + y * 2, col + x * 2) == color)
          *ip++ = (y * width + x) * 8 + color;
        else
          *ip++ = 0;
      }
    }
  brow[4] = (ushort(*)[4])calloc(width * 3, sizeof **brow);
  merror(brow[4], "vng_interpolate()");
  for (row = 0; row < 3; row++)
    brow[row] = brow[4] + row * width;
  for (row = 2; row < height - 2; row++)
  { /* Do VNG interpolation */
#ifdef LIBRAW_LIBRARY_BUILD
    if (!((row - 2) % 256))
      RUN_CALLBACK(LIBRAW_PROGRESS_INTERPOLATE, (row - 2) / 256 + 1, ((height - 3) / 256) + 1);
#endif
    for (col = 2; col < width - 2; col++)
    {
      pix = image[row * width + col];
      ip = code[row % prow][col % pcol];
      memset(gval, 0, sizeof gval);
      while ((g = ip[0]) != INT_MAX)
      { /* Calculate gradients */
        diff = ABS(pix[g] - pix[ip[1]]) << ip[2];
        gval[ip[3]] += diff;
        ip += 5;
        if ((g = ip[-1]) == -1)
          continue;
        gval[g] += diff;
        while ((g = *ip++) != -1)
          gval[g] += diff;
      }
      ip++;
      gmin = gmax = gval[0]; /* Choose a threshold */
      for (g = 1; g < 8; g++)
      {
        if (gmin > gval[g])
          gmin = gval[g];
        if (gmax < gval[g])
          gmax = gval[g];
      }
      if (gmax == 0)
      {
        memcpy(brow[2][col], pix, sizeof *image);
        continue;
      }
      thold = gmin + (gmax >> 1);
      memset(sum, 0, sizeof sum);
      color = fcol(row, col);
      for (num = g = 0; g < 8; g++, ip += 2)
      { /* Average the neighbors */
        if (gval[g] <= thold)
        {
          FORCC
          if (c == color && ip[1])
            sum[c] += (pix[c] + pix[ip[1]]) >> 1;
          else
            sum[c] += pix[ip[0] + c];
          num++;
        }
      }
      FORCC
      { /* Save to buffer */
        t = pix[color];
        if (c != color)
          t += (sum[c] - sum[color]) / num;
        brow[2][col][c] = CLIP(t);
      }
    }
    if (row > 3) /* Write buffer to image */
      memcpy(image[(row - 2) * width + 2], brow[0] + 2, (width - 4) * sizeof *image);
    for (g = 0; g < 4; g++)
      brow[(g - 1) & 3] = brow[g];
  }
  memcpy(image[(row - 2) * width + 2], brow[0] + 2, (width - 4) * sizeof *image);
  memcpy(image[(row - 1) * width + 2], brow[1] + 2, (width - 4) * sizeof *image);
  free(brow[4]);
  free(code[0][0]);
}

/*
   Patterned Pixel Grouping Interpolation by Alain Desbiolles
*/
void CLASS ppg_interpolate()
{
  int dir[5] = {1, width, -1, -width, 1};
  int row, col, diff[2], guess[2], c, d, i;
  ushort(*pix)[4];

  border_interpolate(3);
#ifdef DCRAW_VERBOSE
  if (verbose)
    fprintf(stderr, _("PPG interpolation...\n"));
#endif

/*  Fill in the green layer with gradients and pattern recognition: */
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_INTERPOLATE, 0, 3);
#ifdef LIBRAW_USE_OPENMP
#pragma omp parallel for default(shared) private(guess, diff, row, col, d, c, i, pix) schedule(static)
#endif
#endif
  for (row = 3; row < height - 3; row++)
    for (col = 3 + (FC(row, 3) & 1), c = FC(row, col); col < width - 3; col += 2)
    {
      pix = image + row * width + col;
      for (i = 0; (d = dir[i]) > 0; i++)
      {
        guess[i] = (pix[-d][1] + pix[0][c] + pix[d][1]) * 2 - pix[-2 * d][c] - pix[2 * d][c];
        diff[i] = (ABS(pix[-2 * d][c] - pix[0][c]) + ABS(pix[2 * d][c] - pix[0][c]) + ABS(pix[-d][1] - pix[d][1])) * 3 +
                  (ABS(pix[3 * d][1] - pix[d][1]) + ABS(pix[-3 * d][1] - pix[-d][1])) * 2;
      }
      d = dir[i = diff[0] > diff[1]];
      pix[0][1] = ULIM(guess[i] >> 2, pix[d][1], pix[-d][1]);
    }
/*  Calculate red and blue for each green pixel:		*/
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_INTERPOLATE, 1, 3);
#ifdef LIBRAW_USE_OPENMP
#pragma omp parallel for default(shared) private(guess, diff, row, col, d, c, i, pix) schedule(static)
#endif
#endif
  for (row = 1; row < height - 1; row++)
    for (col = 1 + (FC(row, 2) & 1), c = FC(row, col + 1); col < width - 1; col += 2)
    {
      pix = image + row * width + col;
      for (i = 0; (d = dir[i]) > 0; c = 2 - c, i++)
        pix[0][c] = CLIP((pix[-d][c] + pix[d][c] + 2 * pix[0][1] - pix[-d][1] - pix[d][1]) >> 1);
    }
/*  Calculate blue for red pixels and vice versa:		*/
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_INTERPOLATE, 2, 3);
#ifdef LIBRAW_USE_OPENMP
#pragma omp parallel for default(shared) private(guess, diff, row, col, d, c, i, pix) schedule(static)
#endif
#endif
  for (row = 1; row < height - 1; row++)
    for (col = 1 + (FC(row, 1) & 1), c = 2 - FC(row, col); col < width - 1; col += 2)
    {
      pix = image + row * width + col;
      for (i = 0; (d = dir[i] + dir[i + 1]) > 0; i++)
      {
        diff[i] = ABS(pix[-d][c] - pix[d][c]) + ABS(pix[-d][1] - pix[0][1]) + ABS(pix[d][1] - pix[0][1]);
        guess[i] = pix[-d][c] + pix[d][c] + 2 * pix[0][1] - pix[-d][1] - pix[d][1];
      }
      if (diff[0] != diff[1])
        pix[0][c] = CLIP(guess[diff[0] > diff[1]] >> 1);
      else
        pix[0][c] = CLIP((guess[0] + guess[1]) >> 2);
    }
}

void CLASS cielab(ushort rgb[3], short lab[3])
{
  int c, i, j, k;
  float r, xyz[3];
#ifdef LIBRAW_NOTHREADS
  static float cbrt[0x10000], xyz_cam[3][4];
#else
#define cbrt tls->ahd_data.cbrt
#define xyz_cam tls->ahd_data.xyz_cam
#endif

  if (!rgb)
  {
#ifndef LIBRAW_NOTHREADS
    if (cbrt[0] < -1.0f)
#endif
      for (i = 0; i < 0x10000; i++)
      {
        r = i / 65535.0;
        cbrt[i] = r > 0.008856 ? pow(r, 1.f / 3.0f) : 7.787f * r + 16.f / 116.0f;
      }
    for (i = 0; i < 3; i++)
      for (j = 0; j < colors; j++)
        for (xyz_cam[i][j] = k = 0; k < 3; k++)
          xyz_cam[i][j] += xyz_rgb[i][k] * rgb_cam[k][j] / d65_white[i];
    return;
  }
  xyz[0] = xyz[1] = xyz[2] = 0.5;
  FORCC
  {
    xyz[0] += xyz_cam[0][c] * rgb[c];
    xyz[1] += xyz_cam[1][c] * rgb[c];
    xyz[2] += xyz_cam[2][c] * rgb[c];
  }
  xyz[0] = cbrt[CLIP((int)xyz[0])];
  xyz[1] = cbrt[CLIP((int)xyz[1])];
  xyz[2] = cbrt[CLIP((int)xyz[2])];
  lab[0] = 64 * (116 * xyz[1] - 16);
  lab[1] = 64 * 500 * (xyz[0] - xyz[1]);
  lab[2] = 64 * 200 * (xyz[1] - xyz[2]);
#ifndef LIBRAW_NOTHREADS
#undef cbrt
#undef xyz_cam
#endif
}

#define TS 512 /* Tile Size */
#define fcol(row, col) xtrans[(row + 6) % 6][(col + 6) % 6]

/*
   Frank Markesteijn's algorithm for Fuji X-Trans sensors
 */
void CLASS xtrans_interpolate(int passes)
{
  int c, d, f, g, h, i, v, ng, row, col, top, left, mrow, mcol;

#ifdef LIBRAW_LIBRARY_BUILD
  int cstat[4] = {0, 0, 0, 0};
#endif

  int val, ndir, pass, hm[8], avg[4], color[3][8];
  static const short orth[12] = {1, 0, 0, 1, -1, 0, 0, -1, 1, 0, 0, 1},
                     patt[2][16] = {{0, 1, 0, -1, 2, 0, -1, 0, 1, 1, 1, -1, 0, 0, 0, 0},
                                    {0, 1, 0, -2, 1, 0, -2, 0, 1, 1, -2, -2, 1, -1, -1, 1}},
                     dir[4] = {1, TS, TS + 1, TS - 1};
  short allhex[3][3][2][8], *hex;
  ushort min, max, sgrow, sgcol;
  ushort(*rgb)[TS][TS][3], (*rix)[3], (*pix)[4];
  short(*lab)[TS][3], (*lix)[3];
  float(*drv)[TS][TS], diff[6], tr;
  char(*homo)[TS][TS], *buffer;

#ifdef DCRAW_VERBOSE
  if (verbose)
    fprintf(stderr, _("%d-pass X-Trans interpolation...\n"), passes);
#endif

#ifdef LIBRAW_LIBRARY_BUILD
  if (width < TS || height < TS)
    throw LIBRAW_EXCEPTION_IO_CORRUPT; // too small image
                                       /* Check against right pattern */
  for (row = 0; row < 6; row++)
    for (col = 0; col < 6; col++)
      cstat[fcol(row, col)]++;

  if (cstat[0] < 6 || cstat[0] > 10 || cstat[1] < 16 || cstat[1] > 24 || cstat[2] < 6 || cstat[2] > 10 || cstat[3])
    throw LIBRAW_EXCEPTION_IO_CORRUPT;

  // Init allhex table to unreasonable values
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      for (int k = 0; k < 2; k++)
        for (int l = 0; l < 8; l++)
          allhex[i][j][k][l] = 32700;
#endif
  cielab(0, 0);
  ndir = 4 << (passes > 1);
  buffer = (char *)malloc(TS * TS * (ndir * 11 + 6));
  merror(buffer, "xtrans_interpolate()");
  rgb = (ushort(*)[TS][TS][3])buffer;
  lab = (short(*)[TS][3])(buffer + TS * TS * (ndir * 6));
  drv = (float(*)[TS][TS])(buffer + TS * TS * (ndir * 6 + 6));
  homo = (char(*)[TS][TS])(buffer + TS * TS * (ndir * 10 + 6));

  int minv = 0, maxv = 0, minh = 0, maxh = 0;
  /* Map a green hexagon around each non-green pixel and vice versa:	*/
  for (row = 0; row < 3; row++)
    for (col = 0; col < 3; col++)
      for (ng = d = 0; d < 10; d += 2)
      {
        g = fcol(row, col) == 1;
        if (fcol(row + orth[d], col + orth[d + 2]) == 1)
          ng = 0;
        else
          ng++;
        if (ng == 4)
        {
          sgrow = row;
          sgcol = col;
        }
        if (ng == g + 1)
          FORC(8)
          {
            v = orth[d] * patt[g][c * 2] + orth[d + 1] * patt[g][c * 2 + 1];
            h = orth[d + 2] * patt[g][c * 2] + orth[d + 3] * patt[g][c * 2 + 1];
            minv = MIN(v, minv);
            maxv = MAX(v, maxv);
            minh = MIN(v, minh);
            maxh = MAX(v, maxh);
            allhex[row][col][0][c ^ (g * 2 & d)] = h + v * width;
            allhex[row][col][1][c ^ (g * 2 & d)] = h + v * TS;
          }
      }

#ifdef LIBRAW_LIBRARY_BUILD
  // Check allhex table initialization
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      for (int k = 0; k < 2; k++)
        for (int l = 0; l < 8; l++)
          if (allhex[i][j][k][l] > maxh + maxv * width + 1 || allhex[i][j][k][l] < minh + minv * width - 1)
            throw LIBRAW_EXCEPTION_IO_CORRUPT;
  int retrycount = 0;
#endif
  /* Set green1 and green3 to the minimum and maximum allowed values:	*/
  for (row = 2; row < height - 2; row++)
    for (min = ~(max = 0), col = 2; col < width - 2; col++)
    {
      if (fcol(row, col) == 1 && (min = ~(max = 0)))
        continue;
      pix = image + row * width + col;
      hex = allhex[row % 3][col % 3][0];
      if (!max)
        FORC(6)
        {
          val = pix[hex[c]][1];
          if (min > val)
            min = val;
          if (max < val)
            max = val;
        }
      pix[0][1] = min;
      pix[0][3] = max;
      switch ((row - sgrow) % 3)
      {
      case 1:
        if (row < height - 3)
        {
          row++;
          col--;
        }
        break;
      case 2:
        if ((min = ~(max = 0)) && (col += 2) < width - 3 && row > 2)
        {
          row--;
#ifdef LIBRAW_LIBRARY_BUILD
          if (retrycount++ > width * height)
            throw LIBRAW_EXCEPTION_IO_CORRUPT;
#endif
        }
      }
    }

  for (top = 3; top < height - 19; top += TS - 16)
    for (left = 3; left < width - 19; left += TS - 16)
    {
      mrow = MIN(top + TS, height - 3);
      mcol = MIN(left + TS, width - 3);
      for (row = top; row < mrow; row++)
        for (col = left; col < mcol; col++)
          memcpy(rgb[0][row - top][col - left], image[row * width + col], 6);
      FORC3 memcpy(rgb[c + 1], rgb[0], sizeof *rgb);

      /* Interpolate green horizontally, vertically, and along both diagonals: */
      for (row = top; row < mrow; row++)
        for (col = left; col < mcol; col++)
        {
          if ((f = fcol(row, col)) == 1)
            continue;
          pix = image + row * width + col;
          hex = allhex[row % 3][col % 3][0];
          color[1][0] = 174 * (pix[hex[1]][1] + pix[hex[0]][1]) - 46 * (pix[2 * hex[1]][1] + pix[2 * hex[0]][1]);
          color[1][1] = 223 * pix[hex[3]][1] + pix[hex[2]][1] * 33 + 92 * (pix[0][f] - pix[-hex[2]][f]);
          FORC(2)
          color[1][2 + c] = 164 * pix[hex[4 + c]][1] + 92 * pix[-2 * hex[4 + c]][1] +
                            33 * (2 * pix[0][f] - pix[3 * hex[4 + c]][f] - pix[-3 * hex[4 + c]][f]);
          FORC4 rgb[c ^ !((row - sgrow) % 3)][row - top][col - left][1] = LIM(color[1][c] >> 8, pix[0][1], pix[0][3]);
        }

      for (pass = 0; pass < passes; pass++)
      {
        if (pass == 1)
          memcpy(rgb += 4, buffer, 4 * sizeof *rgb);

        /* Recalculate green from interpolated values of closer pixels:	*/
        if (pass)
        {
          for (row = top + 2; row < mrow - 2; row++)
            for (col = left + 2; col < mcol - 2; col++)
            {
              if ((f = fcol(row, col)) == 1)
                continue;
              pix = image + row * width + col;
              hex = allhex[row % 3][col % 3][1];
              for (d = 3; d < 6; d++)
              {
                rix = &rgb[(d - 2) ^ !((row - sgrow) % 3)][row - top][col - left];
                val =
                    rix[-2 * hex[d]][1] + 2 * rix[hex[d]][1] - rix[-2 * hex[d]][f] - 2 * rix[hex[d]][f] + 3 * rix[0][f];
                rix[0][1] = LIM(val / 3, pix[0][1], pix[0][3]);
              }
            }
        }

        /* Interpolate red and blue values for solitary green pixels:	*/
        for (row = (top - sgrow + 4) / 3 * 3 + sgrow; row < mrow - 2; row += 3)
          for (col = (left - sgcol + 4) / 3 * 3 + sgcol; col < mcol - 2; col += 3)
          {
            rix = &rgb[0][row - top][col - left];
            h = fcol(row, col + 1);
            memset(diff, 0, sizeof diff);
            for (i = 1, d = 0; d < 6; d++, i ^= TS ^ 1, h ^= 2)
            {
              for (c = 0; c < 2; c++, h ^= 2)
              {
                g = 2 * rix[0][1] - rix[i << c][1] - rix[-i << c][1];
                color[h][d] = g + rix[i << c][h] + rix[-i << c][h];
                if (d > 1)
                  diff[d] += SQR(rix[i << c][1] - rix[-i << c][1] - rix[i << c][h] + rix[-i << c][h]) + SQR(g);
              }
              if (d > 1 && (d & 1))
                if (diff[d - 1] < diff[d])
                  FORC(2) color[c * 2][d] = color[c * 2][d - 1];
              if (d < 2 || (d & 1))
              {
                FORC(2) rix[0][c * 2] = CLIP(color[c * 2][d] / 2);
                rix += TS * TS;
              }
            }
          }

        /* Interpolate red for blue pixels and vice versa:		*/
        for (row = top + 3; row < mrow - 3; row++)
          for (col = left + 3; col < mcol - 3; col++)
          {
            if ((f = 2 - fcol(row, col)) == 1)
              continue;
            rix = &rgb[0][row - top][col - left];
            c = (row - sgrow) % 3 ? TS : 1;
            h = 3 * (c ^ TS ^ 1);
            for (d = 0; d < 4; d++, rix += TS * TS)
            {
              i = d > 1 || ((d ^ c) & 1) ||
                          ((ABS(rix[0][1] - rix[c][1]) + ABS(rix[0][1] - rix[-c][1])) <
                           2 * (ABS(rix[0][1] - rix[h][1]) + ABS(rix[0][1] - rix[-h][1])))
                      ? c
                      : h;
              rix[0][f] = CLIP((rix[i][f] + rix[-i][f] + 2 * rix[0][1] - rix[i][1] - rix[-i][1]) / 2);
            }
          }

        /* Fill in red and blue for 2x2 blocks of green:		*/
        for (row = top + 2; row < mrow - 2; row++)
          if ((row - sgrow) % 3)
            for (col = left + 2; col < mcol - 2; col++)
              if ((col - sgcol) % 3)
              {
                rix = &rgb[0][row - top][col - left];
                hex = allhex[row % 3][col % 3][1];
                for (d = 0; d < ndir; d += 2, rix += TS * TS)
                  if (hex[d] + hex[d + 1])
                  {
                    g = 3 * rix[0][1] - 2 * rix[hex[d]][1] - rix[hex[d + 1]][1];
                    for (c = 0; c < 4; c += 2)
                      rix[0][c] = CLIP((g + 2 * rix[hex[d]][c] + rix[hex[d + 1]][c]) / 3);
                  }
                  else
                  {
                    g = 2 * rix[0][1] - rix[hex[d]][1] - rix[hex[d + 1]][1];
                    for (c = 0; c < 4; c += 2)
                      rix[0][c] = CLIP((g + rix[hex[d]][c] + rix[hex[d + 1]][c]) / 2);
                  }
              }
      }
      rgb = (ushort(*)[TS][TS][3])buffer;
      mrow -= top;
      mcol -= left;

      /* Convert to CIELab and differentiate in all directions:	*/
      for (d = 0; d < ndir; d++)
      {
        for (row = 2; row < mrow - 2; row++)
          for (col = 2; col < mcol - 2; col++)
            cielab(rgb[d][row][col], lab[row][col]);
        for (f = dir[d & 3], row = 3; row < mrow - 3; row++)
          for (col = 3; col < mcol - 3; col++)
          {
            lix = &lab[row][col];
            g = 2 * lix[0][0] - lix[f][0] - lix[-f][0];
            drv[d][row][col] = SQR(g) + SQR((2 * lix[0][1] - lix[f][1] - lix[-f][1] + g * 500 / 232)) +
                               SQR((2 * lix[0][2] - lix[f][2] - lix[-f][2] - g * 500 / 580));
          }
      }

      /* Build homogeneity maps from the derivatives:			*/
      memset(homo, 0, ndir * TS * TS);
      for (row = 4; row < mrow - 4; row++)
        for (col = 4; col < mcol - 4; col++)
        {
          for (tr = FLT_MAX, d = 0; d < ndir; d++)
            if (tr > drv[d][row][col])
              tr = drv[d][row][col];
          tr *= 8;
          for (d = 0; d < ndir; d++)
            for (v = -1; v <= 1; v++)
              for (h = -1; h <= 1; h++)
                if (drv[d][row + v][col + h] <= tr)
                  homo[d][row][col]++;
        }

      /* Average the most homogenous pixels for the final result:	*/
      if (height - top < TS + 4)
        mrow = height - top + 2;
      if (width - left < TS + 4)
        mcol = width - left + 2;
      for (row = MIN(top, 8); row < mrow - 8; row++)
        for (col = MIN(left, 8); col < mcol - 8; col++)
        {
          for (d = 0; d < ndir; d++)
            for (hm[d] = 0, v = -2; v <= 2; v++)
              for (h = -2; h <= 2; h++)
                hm[d] += homo[d][row + v][col + h];
          for (d = 0; d < ndir - 4; d++)
            if (hm[d] < hm[d + 4])
              hm[d] = 0;
            else if (hm[d] > hm[d + 4])
              hm[d + 4] = 0;
          for (max = hm[0], d = 1; d < ndir; d++)
            if (max < hm[d])
              max = hm[d];
          max -= max >> 3;
          memset(avg, 0, sizeof avg);
          for (d = 0; d < ndir; d++)
            if (hm[d] >= max)
            {
              FORC3 avg[c] += rgb[d][row][col][c];
              avg[3]++;
            }
          FORC3 image[(row + top) * width + col + left][c] = avg[c] / avg[3];
        }
    }
  free(buffer);
  border_interpolate(8);
}
#undef fcol

/*
   Adaptive Homogeneity-Directed interpolation is based on
   the work of Keigo Hirakawa, Thomas Parks, and Paul Lee.
 */
#ifdef LIBRAW_LIBRARY_BUILD

void CLASS ahd_interpolate_green_h_and_v(int top, int left, ushort (*out_rgb)[TS][TS][3])
{
  int row, col;
  int c, val;
  ushort(*pix)[4];
  const int rowlimit = MIN(top + TS, height - 2);
  const int collimit = MIN(left + TS, width - 2);

  for (row = top; row < rowlimit; row++)
  {
    col = left + (FC(row, left) & 1);
    for (c = FC(row, col); col < collimit; col += 2)
    {
      pix = image + row * width + col;
      val = ((pix[-1][1] + pix[0][c] + pix[1][1]) * 2 - pix[-2][c] - pix[2][c]) >> 2;
      out_rgb[0][row - top][col - left][1] = ULIM(val, pix[-1][1], pix[1][1]);
      val = ((pix[-width][1] + pix[0][c] + pix[width][1]) * 2 - pix[-2 * width][c] - pix[2 * width][c]) >> 2;
      out_rgb[1][row - top][col - left][1] = ULIM(val, pix[-width][1], pix[width][1]);
    }
  }
}
void CLASS ahd_interpolate_r_and_b_in_rgb_and_convert_to_cielab(int top, int left, ushort (*inout_rgb)[TS][3],
                                                                short (*out_lab)[TS][3])
{
  unsigned row, col;
  int c, val;
  ushort(*pix)[4];
  ushort(*rix)[3];
  short(*lix)[3];
  float xyz[3];
  const unsigned num_pix_per_row = 4 * width;
  const unsigned rowlimit = MIN(top + TS - 1, height - 3);
  const unsigned collimit = MIN(left + TS - 1, width - 3);
  ushort *pix_above;
  ushort *pix_below;
  int t1, t2;

  for (row = top + 1; row < rowlimit; row++)
  {
    pix = image + row * width + left;
    rix = &inout_rgb[row - top][0];
    lix = &out_lab[row - top][0];

    for (col = left + 1; col < collimit; col++)
    {
      pix++;
      pix_above = &pix[0][0] - num_pix_per_row;
      pix_below = &pix[0][0] + num_pix_per_row;
      rix++;
      lix++;

      c = 2 - FC(row, col);

      if (c == 1)
      {
        c = FC(row + 1, col);
        t1 = 2 - c;
        val = pix[0][1] + ((pix[-1][t1] + pix[1][t1] - rix[-1][1] - rix[1][1]) >> 1);
        rix[0][t1] = CLIP(val);
        val = pix[0][1] + ((pix_above[c] + pix_below[c] - rix[-TS][1] - rix[TS][1]) >> 1);
      }
      else
      {
        t1 = -4 + c; /* -4+c: pixel of color c to the left */
        t2 = 4 + c;  /* 4+c: pixel of color c to the right */
        val = rix[0][1] + ((pix_above[t1] + pix_above[t2] + pix_below[t1] + pix_below[t2] - rix[-TS - 1][1] -
                            rix[-TS + 1][1] - rix[+TS - 1][1] - rix[+TS + 1][1] + 1) >>
                           2);
      }
      rix[0][c] = CLIP(val);
      c = FC(row, col);
      rix[0][c] = pix[0][c];
      cielab(rix[0], lix[0]);
    }
  }
}
void CLASS ahd_interpolate_r_and_b_and_convert_to_cielab(int top, int left, ushort (*inout_rgb)[TS][TS][3],
                                                         short (*out_lab)[TS][TS][3])
{
  int direction;
  for (direction = 0; direction < 2; direction++)
  {
    ahd_interpolate_r_and_b_in_rgb_and_convert_to_cielab(top, left, inout_rgb[direction], out_lab[direction]);
  }
}

void CLASS ahd_interpolate_build_homogeneity_map(int top, int left, short (*lab)[TS][TS][3],
                                                 char (*out_homogeneity_map)[TS][2])
{
  int row, col;
  int tr, tc;
  int direction;
  int i;
  short(*lix)[3];
  short(*lixs[2])[3];
  short *adjacent_lix;
  unsigned ldiff[2][4], abdiff[2][4], leps, abeps;
  static const int dir[4] = {-1, 1, -TS, TS};
  const int rowlimit = MIN(top + TS - 2, height - 4);
  const int collimit = MIN(left + TS - 2, width - 4);
  int homogeneity;
  char(*homogeneity_map_p)[2];

  memset(out_homogeneity_map, 0, 2 * TS * TS);

  for (row = top + 2; row < rowlimit; row++)
  {
    tr = row - top;
    homogeneity_map_p = &out_homogeneity_map[tr][1];
    for (direction = 0; direction < 2; direction++)
    {
      lixs[direction] = &lab[direction][tr][1];
    }

    for (col = left + 2; col < collimit; col++)
    {
      tc = col - left;
      homogeneity_map_p++;

      for (direction = 0; direction < 2; direction++)
      {
        lix = ++lixs[direction];
        for (i = 0; i < 4; i++)
        {
          adjacent_lix = lix[dir[i]];
          ldiff[direction][i] = ABS(lix[0][0] - adjacent_lix[0]);
          abdiff[direction][i] = SQR(lix[0][1] - adjacent_lix[1]) + SQR(lix[0][2] - adjacent_lix[2]);
        }
      }
      leps = MIN(MAX(ldiff[0][0], ldiff[0][1]), MAX(ldiff[1][2], ldiff[1][3]));
      abeps = MIN(MAX(abdiff[0][0], abdiff[0][1]), MAX(abdiff[1][2], abdiff[1][3]));
      for (direction = 0; direction < 2; direction++)
      {
        homogeneity = 0;
        for (i = 0; i < 4; i++)
        {
          if (ldiff[direction][i] <= leps && abdiff[direction][i] <= abeps)
          {
            homogeneity++;
          }
        }
        homogeneity_map_p[0][direction] = homogeneity;
      }
    }
  }
}
void CLASS ahd_interpolate_combine_homogeneous_pixels(int top, int left, ushort (*rgb)[TS][TS][3],
                                                      char (*homogeneity_map)[TS][2])
{
  int row, col;
  int tr, tc;
  int i, j;
  int direction;
  int hm[2];
  int c;
  const int rowlimit = MIN(top + TS - 3, height - 5);
  const int collimit = MIN(left + TS - 3, width - 5);

  ushort(*pix)[4];
  ushort(*rix[2])[3];

  for (row = top + 3; row < rowlimit; row++)
  {
    tr = row - top;
    pix = &image[row * width + left + 2];
    for (direction = 0; direction < 2; direction++)
    {
      rix[direction] = &rgb[direction][tr][2];
    }

    for (col = left + 3; col < collimit; col++)
    {
      tc = col - left;
      pix++;
      for (direction = 0; direction < 2; direction++)
      {
        rix[direction]++;
      }

      for (direction = 0; direction < 2; direction++)
      {
        hm[direction] = 0;
        for (i = tr - 1; i <= tr + 1; i++)
        {
          for (j = tc - 1; j <= tc + 1; j++)
          {
            hm[direction] += homogeneity_map[i][j][direction];
          }
        }
      }
      if (hm[0] != hm[1])
      {
        memcpy(pix[0], rix[hm[1] > hm[0]][0], 3 * sizeof(ushort));
      }
      else
      {
        FORC3 { pix[0][c] = (rix[0][0][c] + rix[1][0][c]) >> 1; }
      }
    }
  }
}
void CLASS ahd_interpolate()
{
  int i, j, k, top, left;
  float xyz_cam[3][4], r;
  char *buffer;
  ushort(*rgb)[TS][TS][3];
  short(*lab)[TS][TS][3];
  char(*homo)[TS][2];
  int terminate_flag = 0;

  cielab(0, 0);
  border_interpolate(5);

#ifdef LIBRAW_USE_OPENMP
#pragma omp parallel private(buffer, rgb, lab, homo, top, left, i, j, k) shared(xyz_cam, terminate_flag)
#endif
  {
#ifdef LIBRAW_USE_OPENMP
#pragma omp critical
#endif
    buffer = (char *)malloc(26 * TS * TS); /* 1664 kB */
    merror(buffer, "ahd_interpolate()");
    rgb = (ushort(*)[TS][TS][3])buffer;
    lab = (short(*)[TS][TS][3])(buffer + 12 * TS * TS);
    homo = (char(*)[TS][2])(buffer + 24 * TS * TS);

#ifdef LIBRAW_USE_OPENMP
#pragma omp for schedule(dynamic)
#endif
    for (top = 2; top < height - 5; top += TS - 6)
    {
#ifdef LIBRAW_USE_OPENMP
      if (0 == omp_get_thread_num())
#endif
        if (callbacks.progress_cb)
        {
          int rr =
              (*callbacks.progress_cb)(callbacks.progresscb_data, LIBRAW_PROGRESS_INTERPOLATE, top - 2, height - 7);
          if (rr)
            terminate_flag = 1;
        }
      for (left = 2; !terminate_flag && (left < width - 5); left += TS - 6)
      {
        ahd_interpolate_green_h_and_v(top, left, rgb);
        ahd_interpolate_r_and_b_and_convert_to_cielab(top, left, rgb, lab);
        ahd_interpolate_build_homogeneity_map(top, left, lab, homo);
        ahd_interpolate_combine_homogeneous_pixels(top, left, rgb, homo);
      }
    }
#ifdef LIBRAW_USE_OPENMP
#pragma omp critical
#endif
    free(buffer);
  }
  if (terminate_flag)
    throw LIBRAW_EXCEPTION_CANCELLED_BY_CALLBACK;
}

#else /* LIBRAW_LIBRARY_BUILD */
void CLASS ahd_interpolate()
{
  int i, j, top, left, row, col, tr, tc, c, d, val, hm[2];
  static const int dir[4] = {-1, 1, -TS, TS};
  unsigned ldiff[2][4], abdiff[2][4], leps, abeps;
  ushort(*rgb)[TS][TS][3], (*rix)[3], (*pix)[4];
  short(*lab)[TS][TS][3], (*lix)[3];
  char(*homo)[TS][TS], *buffer;

#ifdef DCRAW_VERBOSE
  if (verbose)
    fprintf(stderr, _("AHD interpolation...\n"));
#endif

  cielab(0, 0);
  border_interpolate(5);
  buffer = (char *)malloc(26 * TS * TS);
  merror(buffer, "ahd_interpolate()");
  rgb = (ushort(*)[TS][TS][3])buffer;
  lab = (short(*)[TS][TS][3])(buffer + 12 * TS * TS);
  homo = (char(*)[TS][TS])(buffer + 24 * TS * TS);

  for (top = 2; top < height - 5; top += TS - 6)
    for (left = 2; left < width - 5; left += TS - 6)
    {

      /*  Interpolate green horizontally and vertically:		*/
      for (row = top; row < top + TS && row < height - 2; row++)
      {
        col = left + (FC(row, left) & 1);
        for (c = FC(row, col); col < left + TS && col < width - 2; col += 2)
        {
          pix = image + row * width + col;
          val = ((pix[-1][1] + pix[0][c] + pix[1][1]) * 2 - pix[-2][c] - pix[2][c]) >> 2;
          rgb[0][row - top][col - left][1] = ULIM(val, pix[-1][1], pix[1][1]);
          val = ((pix[-width][1] + pix[0][c] + pix[width][1]) * 2 - pix[-2 * width][c] - pix[2 * width][c]) >> 2;
          rgb[1][row - top][col - left][1] = ULIM(val, pix[-width][1], pix[width][1]);
        }
      }

      /*  Interpolate red and blue, and convert to CIELab:		*/
      for (d = 0; d < 2; d++)
        for (row = top + 1; row < top + TS - 1 && row < height - 3; row++)
          for (col = left + 1; col < left + TS - 1 && col < width - 3; col++)
          {
            pix = image + row * width + col;
            rix = &rgb[d][row - top][col - left];
            lix = &lab[d][row - top][col - left];
            if ((c = 2 - FC(row, col)) == 1)
            {
              c = FC(row + 1, col);
              val = pix[0][1] + ((pix[-1][2 - c] + pix[1][2 - c] - rix[-1][1] - rix[1][1]) >> 1);
              rix[0][2 - c] = CLIP(val);
              val = pix[0][1] + ((pix[-width][c] + pix[width][c] - rix[-TS][1] - rix[TS][1]) >> 1);
            }
            else
              val = rix[0][1] + ((pix[-width - 1][c] + pix[-width + 1][c] + pix[+width - 1][c] + pix[+width + 1][c] -
                                  rix[-TS - 1][1] - rix[-TS + 1][1] - rix[+TS - 1][1] - rix[+TS + 1][1] + 1) >>
                                 2);
            rix[0][c] = CLIP(val);
            c = FC(row, col);
            rix[0][c] = pix[0][c];
            cielab(rix[0], lix[0]);
          }
      /*  Build homogeneity maps from the CIELab images:		*/
      memset(homo, 0, 2 * TS * TS);
      for (row = top + 2; row < top + TS - 2 && row < height - 4; row++)
      {
        tr = row - top;
        for (col = left + 2; col < left + TS - 2 && col < width - 4; col++)
        {
          tc = col - left;
          for (d = 0; d < 2; d++)
          {
            lix = &lab[d][tr][tc];
            for (i = 0; i < 4; i++)
            {
              ldiff[d][i] = ABS(lix[0][0] - lix[dir[i]][0]);
              abdiff[d][i] = SQR(lix[0][1] - lix[dir[i]][1]) + SQR(lix[0][2] - lix[dir[i]][2]);
            }
          }
          leps = MIN(MAX(ldiff[0][0], ldiff[0][1]), MAX(ldiff[1][2], ldiff[1][3]));
          abeps = MIN(MAX(abdiff[0][0], abdiff[0][1]), MAX(abdiff[1][2], abdiff[1][3]));
          for (d = 0; d < 2; d++)
            for (i = 0; i < 4; i++)
              if (ldiff[d][i] <= leps && abdiff[d][i] <= abeps)
                homo[d][tr][tc]++;
        }
      }
      /*  Combine the most homogenous pixels for the final result:	*/
      for (row = top + 3; row < top + TS - 3 && row < height - 5; row++)
      {
        tr = row - top;
        for (col = left + 3; col < left + TS - 3 && col < width - 5; col++)
        {
          tc = col - left;
          for (d = 0; d < 2; d++)
            for (hm[d] = 0, i = tr - 1; i <= tr + 1; i++)
              for (j = tc - 1; j <= tc + 1; j++)
                hm[d] += homo[d][i][j];
          if (hm[0] != hm[1])
            FORC3 image[row * width + col][c] = rgb[hm[1] > hm[0]][tr][tc][c];
          else
            FORC3 image[row * width + col][c] = (rgb[0][tr][tc][c] + rgb[1][tr][tc][c]) >> 1;
        }
      }
    }
  free(buffer);
}
#endif
#undef TS

void CLASS median_filter()
{
  ushort(*pix)[4];
  int pass, c, i, j, k, med[9];
  static const uchar opt[] = /* Optimal 9-element median search */
      {1, 2, 4, 5, 7, 8, 0, 1, 3, 4, 6, 7, 1, 2, 4, 5, 7, 8, 0,
       3, 5, 8, 4, 7, 3, 6, 1, 4, 2, 5, 4, 7, 4, 2, 6, 4, 4, 2};

  for (pass = 1; pass <= med_passes; pass++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    RUN_CALLBACK(LIBRAW_PROGRESS_MEDIAN_FILTER, pass - 1, med_passes);
#endif
#ifdef DCRAW_VERBOSE
    if (verbose)
      fprintf(stderr, _("Median filter pass %d...\n"), pass);
#endif
    for (c = 0; c < 3; c += 2)
    {
      for (pix = image; pix < image + width * height; pix++)
        pix[0][3] = pix[0][c];
      for (pix = image + width; pix < image + width * (height - 1); pix++)
      {
        if ((pix - image + 1) % width < 2)
          continue;
        for (k = 0, i = -width; i <= width; i += width)
          for (j = i - 1; j <= i + 1; j++)
            med[k++] = pix[j][3] - pix[j][1];
        for (i = 0; i < sizeof opt; i += 2)
          if (med[opt[i]] > med[opt[i + 1]])
            SWAP(med[opt[i]], med[opt[i + 1]]);
        pix[0][c] = CLIP(med[4] + pix[0][1]);
      }
    }
  }
}

void CLASS blend_highlights()
{
  int clip = INT_MAX, row, col, c, i, j;
  static const float trans[2][4][4] = {{{1, 1, 1}, {1.7320508, -1.7320508, 0}, {-1, -1, 2}},
                                       {{1, 1, 1, 1}, {1, -1, 1, -1}, {1, 1, -1, -1}, {1, -1, -1, 1}}};
  static const float itrans[2][4][4] = {{{1, 0.8660254, -0.5}, {1, -0.8660254, -0.5}, {1, 0, 1}},
                                        {{1, 1, 1, 1}, {1, -1, 1, -1}, {1, 1, -1, -1}, {1, -1, -1, 1}}};
  float cam[2][4], lab[2][4], sum[2], chratio;

  if ((unsigned)(colors - 3) > 1)
    return;
#ifdef DCRAW_VERBOSE
  if (verbose)
    fprintf(stderr, _("Blending highlights...\n"));
#endif
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_HIGHLIGHTS, 0, 2);
#endif
  FORCC if (clip > (i = 65535 * pre_mul[c])) clip = i;
  for (row = 0; row < height; row++)
    for (col = 0; col < width; col++)
    {
      FORCC if (image[row * width + col][c] > clip) break;
      if (c == colors)
        continue;
      FORCC
      {
        cam[0][c] = image[row * width + col][c];
        cam[1][c] = MIN(cam[0][c], clip);
      }
      for (i = 0; i < 2; i++)
      {
        FORCC for (lab[i][c] = j = 0; j < colors; j++) lab[i][c] += trans[colors - 3][c][j] * cam[i][j];
        for (sum[i] = 0, c = 1; c < colors; c++)
          sum[i] += SQR(lab[i][c]);
      }
      chratio = sqrt(sum[1] / sum[0]);
      for (c = 1; c < colors; c++)
        lab[0][c] *= chratio;
      FORCC for (cam[0][c] = j = 0; j < colors; j++) cam[0][c] += itrans[colors - 3][c][j] * lab[0][j];
      FORCC image[row * width + col][c] = cam[0][c] / colors;
    }
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_HIGHLIGHTS, 1, 2);
#endif
}

#define SCALE (4 >> shrink)
void CLASS recover_highlights()
{
  float *map, sum, wgt, grow;
  int hsat[4], count, spread, change, val, i;
  unsigned high, wide, mrow, mcol, row, col, kc, c, d, y, x;
  ushort *pixel;
  static const signed char dir[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}};

#ifdef DCRAW_VERBOSE
  if (verbose)
    fprintf(stderr, _("Rebuilding highlights...\n"));
#endif

  grow = pow(2.0, 4 - highlight);
  FORCC hsat[c] = 32000 * pre_mul[c];
  for (kc = 0, c = 1; c < colors; c++)
    if (pre_mul[kc] < pre_mul[c])
      kc = c;
  high = height / SCALE;
  wide = width / SCALE;
  map = (float *)calloc(high, wide * sizeof *map);
  merror(map, "recover_highlights()");
  FORCC if (c != kc)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    RUN_CALLBACK(LIBRAW_PROGRESS_HIGHLIGHTS, c - 1, colors - 1);
#endif
    memset(map, 0, high * wide * sizeof *map);
    for (mrow = 0; mrow < high; mrow++)
      for (mcol = 0; mcol < wide; mcol++)
      {
        sum = wgt = count = 0;
        for (row = mrow * SCALE; row < (mrow + 1) * SCALE; row++)
          for (col = mcol * SCALE; col < (mcol + 1) * SCALE; col++)
          {
            pixel = image[row * width + col];
            if (pixel[c] / hsat[c] == 1 && pixel[kc] > 24000)
            {
              sum += pixel[c];
              wgt += pixel[kc];
              count++;
            }
          }
        if (count == SCALE * SCALE)
          map[mrow * wide + mcol] = sum / wgt;
      }
    for (spread = 32 / grow; spread--;)
    {
      for (mrow = 0; mrow < high; mrow++)
        for (mcol = 0; mcol < wide; mcol++)
        {
          if (map[mrow * wide + mcol])
            continue;
          sum = count = 0;
          for (d = 0; d < 8; d++)
          {
            y = mrow + dir[d][0];
            x = mcol + dir[d][1];
            if (y < high && x < wide && map[y * wide + x] > 0)
            {
              sum += (1 + (d & 1)) * map[y * wide + x];
              count += 1 + (d & 1);
            }
          }
          if (count > 3)
            map[mrow * wide + mcol] = -(sum + grow) / (count + grow);
        }
      for (change = i = 0; i < high * wide; i++)
        if (map[i] < 0)
        {
          map[i] = -map[i];
          change = 1;
        }
      if (!change)
        break;
    }
    for (i = 0; i < high * wide; i++)
      if (map[i] == 0)
        map[i] = 1;
    for (mrow = 0; mrow < high; mrow++)
      for (mcol = 0; mcol < wide; mcol++)
      {
        for (row = mrow * SCALE; row < (mrow + 1) * SCALE; row++)
          for (col = mcol * SCALE; col < (mcol + 1) * SCALE; col++)
          {
            pixel = image[row * width + col];
            if (pixel[c] / hsat[c] > 1)
            {
              val = pixel[kc] * map[mrow * wide + mcol];
              if (pixel[c] < val)
                pixel[c] = CLIP(val);
            }
          }
      }
  }
  free(map);
}
#undef SCALE

void CLASS tiff_get(unsigned base, unsigned *tag, unsigned *type, unsigned *len, unsigned *save)
{
#ifdef LIBRAW_IOSPACE_CHECK
  INT64 pos = ftell(ifp);
  INT64 fsize = ifp->size();
  if(fsize < 12 || (fsize-pos) < 12)
     throw LIBRAW_EXCEPTION_IO_EOF;
#endif
  *tag = get2();
  *type = get2();
  *len = get4();
  *save = ftell(ifp) + 4;
  if (*len * ("11124811248484"[*type < 14 ? *type : 0] - '0') > 4)
    fseek(ifp, get4() + base, SEEK_SET);
}

void CLASS parse_thumb_note(int base, unsigned toff, unsigned tlen)
{
  unsigned entries, tag, type, len, save;

  entries = get2();
  while (entries--)
  {
    tiff_get(base, &tag, &type, &len, &save);
    if (tag == toff)
      thumb_offset = get4() + base;
    if (tag == tlen)
      thumb_length = get4();
    fseek(ifp, save, SEEK_SET);
  }
}
//@end COMMON

int CLASS parse_tiff_ifd(int base);

//@out COMMON

static float powf_lim(float a, float b, float limup) { return (b > limup || b < -limup) ? 0.f : powf(a, b); }
static float libraw_powf64l(float a, float b) { return powf_lim(a, b, 64.f); }

#ifdef LIBRAW_LIBRARY_BUILD

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

static float _CanonConvertAperture(ushort in)
{
  if ((in == (ushort)0xffe0) || (in == (ushort)0x7fff))
    return 0.0f;
  return libraw_powf64l(2.0, in / 64.0);
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

unsigned CLASS setCanonBodyFeatures(unsigned id)
{
  if (id == 0x03740000)      // EOS M3
    id = 0x80000374;
  else if (id == 0x03840000) // EOS M10
    id = 0x80000384;
  else if (id == 0x03940000) // EOS M5
    id = 0x80000394;
  else if (id == 0x03980000) // EOS M100
    id = 0x80000398;
  else if (id == 0x04070000) // EOS M6
    id = 0x80000407;
  else if (id == 0x00000412) // EOS M50
    id = 0x80000412;

  imgdata.lens.makernotes.CamID = id;
  if ((id == 0x80000001) || // 1D
      (id == 0x80000174) || // 1D2
      (id == 0x80000232) || // 1D2N
      (id == 0x80000169) || // 1D3
      (id == 0x80000281)    // 1D4
  ) {
    imgdata.lens.makernotes.CameraFormat = LIBRAW_FORMAT_APSH;
    imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Canon_EF;

  } else if ((id == 0x80000167) || // 1Ds
             (id == 0x80000188) || // 1Ds2
             (id == 0x80000215) || // 1Ds3
             (id == 0x80000269) || // 1DX
             (id == 0x80000328) || // 1DX2
             (id == 0x80000324) || // 1DC
             (id == 0x80000213) || // 5D
             (id == 0x80000218) || // 5D2
             (id == 0x80000285) || // 5D3
             (id == 0x80000349) || // 5D4
             (id == 0x80000382) || // 5DS
             (id == 0x80000401) || // 5DS R
             (id == 0x80000302)    // 6D
  ) {
    imgdata.lens.makernotes.CameraFormat = LIBRAW_FORMAT_FF;
    imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Canon_EF;

  } else if ((id == 0x80000331) || // M
             (id == 0x80000355) || // M2
             (id == 0x80000374) || // M3
             (id == 0x80000384) || // M10
             (id == 0x80000394) || // M5
             (id == 0x80000412) || // M50
             (id == 0x80000407) || // M6
             (id == 0x80000398)    // M100
  ) {
    imgdata.lens.makernotes.CameraFormat = LIBRAW_FORMAT_APSC;
    imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Canon_EF_M;

  } else if ((id == 0x80000424) || // EOS R
             (id == 0x80000433)    // EOS RP
  ) {
    imgdata.lens.makernotes.CameraFormat = LIBRAW_FORMAT_FF;
    imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Canon_RF;

  } else if ((id == 0x01140000) || // D30
             (id == 0x01668000) || // D60
             (id >  0x80000000)) {
    imgdata.lens.makernotes.CameraFormat = LIBRAW_FORMAT_APSC;
    imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Canon_EF;
    imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_Unknown;

  } else {
    imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_FixedLens;
    imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_FixedLens;
  }

  return id;
}

void CLASS processCanonCameraInfo(unsigned id, uchar *CameraInfo, unsigned maxlen, unsigned type)
{
  ushort iCanonLensID = 0, iCanonMaxFocal = 0, iCanonMinFocal = 0, iCanonLens = 0, iCanonCurFocal = 0,
         iCanonFocalType = 0;
  if (maxlen < 16)
    return; // too short
  CameraInfo[0] = 0;
  CameraInfo[1] = 0;
  if (type == 4) {
    if ((maxlen == 94) || (maxlen == 138) || (maxlen == 148) || (maxlen == 156) || (maxlen == 162) || (maxlen == 167) ||
        (maxlen == 171) || (maxlen == 264) || (maxlen > 400))
      imgdata.other.CameraTemperature = sget4(CameraInfo + ((maxlen - 3) << 2));
    else if (maxlen == 72)
      imgdata.other.CameraTemperature = sget4(CameraInfo + ((maxlen - 1) << 2));
    else if ((maxlen == 85) || (maxlen == 93))
      imgdata.other.CameraTemperature = sget4(CameraInfo + ((maxlen - 2) << 2));
    else if ((maxlen == 96) || (maxlen == 104))
      imgdata.other.CameraTemperature = sget4(CameraInfo + ((maxlen - 4) << 2));
  }

  switch (id) {
  case 0x80000001: // 1D
  case 0x80000167: // 1DS
    iCanonCurFocal = 10;
    iCanonLensID = 13;
    iCanonMinFocal = 14;
    iCanonMaxFocal = 16;
    if (!imgdata.lens.makernotes.CurFocal)
      imgdata.lens.makernotes.CurFocal = sget2(CameraInfo + iCanonCurFocal);
    if (!imgdata.lens.makernotes.MinFocal)
      imgdata.lens.makernotes.MinFocal = sget2(CameraInfo + iCanonMinFocal);
    if (!imgdata.lens.makernotes.MaxFocal)
      imgdata.lens.makernotes.MaxFocal = sget2(CameraInfo + iCanonMaxFocal);
    imgdata.other.CameraTemperature = 0.0f;
    break;
  case 0x80000174: // 1DMkII
  case 0x80000188: // 1DsMkII
    iCanonCurFocal = 9;
    iCanonLensID = 12;
    iCanonMinFocal = 17;
    iCanonMaxFocal = 19;
    iCanonFocalType = 45;
    break;
  case 0x80000232: // 1DMkII N
    iCanonCurFocal = 9;
    iCanonLensID = 12;
    iCanonMinFocal = 17;
    iCanonMaxFocal = 19;
    break;
  case 0x80000169: // 1DMkIII
  case 0x80000215: // 1DsMkIII
    iCanonCurFocal = 29;
    iCanonLensID = 273;
    iCanonMinFocal = 275;
    iCanonMaxFocal = 277;
    break;
  case 0x80000281: // 1DMkIV
    iCanonCurFocal = 30;
    iCanonLensID = 335;
    iCanonMinFocal = 337;
    iCanonMaxFocal = 339;
    break;
  case 0x80000269: // 1D X
    iCanonCurFocal = 35;
    iCanonLensID = 423;
    iCanonMinFocal = 425;
    iCanonMaxFocal = 427;
    break;
  case 0x80000213: // 5D
    iCanonCurFocal = 40;
    if (!sget2Rev(CameraInfo + 12))
      iCanonLensID = 151;
    else
      iCanonLensID = 12;
    iCanonMinFocal = 147;
    iCanonMaxFocal = 149;
    break;
  case 0x80000218: // 5DMkII
    iCanonCurFocal = 30;
    iCanonLensID = 230;
    iCanonMinFocal = 232;
    iCanonMaxFocal = 234;
    break;
  case 0x80000285: // 5DMkIII
    iCanonCurFocal = 35;
    iCanonLensID = 339;
    iCanonMinFocal = 341;
    iCanonMaxFocal = 343;
    break;
  case 0x80000302: // 6D
    iCanonCurFocal = 35;
    iCanonLensID = 353;
    iCanonMinFocal = 355;
    iCanonMaxFocal = 357;
    break;
  case 0x80000250: // 7D
    iCanonCurFocal = 30;
    iCanonLensID = 274;
    iCanonMinFocal = 276;
    iCanonMaxFocal = 278;
    break;
  case 0x80000190: // 40D
    iCanonCurFocal = 29;
    iCanonLensID = 214;
    iCanonMinFocal = 216;
    iCanonMaxFocal = 218;
    iCanonLens = 2347;
    break;
  case 0x80000261: // 50D
    iCanonCurFocal = 30;
    iCanonLensID = 234;
    iCanonMinFocal = 236;
    iCanonMaxFocal = 238;
    break;
  case 0x80000287: // 60D
    iCanonCurFocal = 30;
    iCanonLensID = 232;
    iCanonMinFocal = 234;
    iCanonMaxFocal = 236;
    break;
  case 0x80000325: // 70D
    iCanonCurFocal = 35;
    iCanonLensID = 358;
    iCanonMinFocal = 360;
    iCanonMaxFocal = 362;
    break;
  case 0x80000176: // 450D
    iCanonCurFocal = 29;
    iCanonLensID = 222;
    iCanonLens = 2355;
    break;
  case 0x80000252: // 500D
    iCanonCurFocal = 30;
    iCanonLensID = 246;
    iCanonMinFocal = 248;
    iCanonMaxFocal = 250;
    break;
  case 0x80000270: // 550D
    iCanonCurFocal = 30;
    iCanonLensID = 255;
    iCanonMinFocal = 257;
    iCanonMaxFocal = 259;
    break;
  case 0x80000286: // 600D
  case 0x80000288: // 1100D
    iCanonCurFocal = 30;
    iCanonLensID = 234;
    iCanonMinFocal = 236;
    iCanonMaxFocal = 238;
    break;
  case 0x80000301: // 650D
  case 0x80000326: // 700D
    iCanonCurFocal = 35;
    iCanonLensID = 295;
    iCanonMinFocal = 297;
    iCanonMaxFocal = 299;
    break;
  case 0x80000254: // 1000D
    iCanonCurFocal = 29;
    iCanonLensID = 226;
    iCanonMinFocal = 228;
    iCanonMaxFocal = 230;
    iCanonLens = 2359;
    break;
  }
  if (iCanonFocalType) {
    if (iCanonFocalType >= maxlen)
      return; // broken;
    imgdata.lens.makernotes.FocalType = CameraInfo[iCanonFocalType];
    if (!imgdata.lens.makernotes.FocalType) // zero means 'fixed' here, replacing with standard '1'
      imgdata.lens.makernotes.FocalType = 1;
  }
  if (!imgdata.lens.makernotes.CurFocal) {
    if (iCanonCurFocal >= maxlen)
      return; // broken;
    imgdata.lens.makernotes.CurFocal = sget2Rev(CameraInfo + iCanonCurFocal);
  }
  if (!imgdata.lens.makernotes.LensID) {
    if (iCanonLensID >= maxlen)
      return; // broken;
    imgdata.lens.makernotes.LensID = sget2Rev(CameraInfo + iCanonLensID);
  }
  if (!imgdata.lens.makernotes.MinFocal) {
    if (iCanonMinFocal >= maxlen)
      return; // broken;
    imgdata.lens.makernotes.MinFocal = sget2Rev(CameraInfo + iCanonMinFocal);
  }
  if (!imgdata.lens.makernotes.MaxFocal) {
    if (iCanonMaxFocal >= maxlen)
      return; // broken;
    imgdata.lens.makernotes.MaxFocal = sget2Rev(CameraInfo + iCanonMaxFocal);
  }
  if (!imgdata.lens.makernotes.Lens[0] && iCanonLens) {
    if (iCanonLens + 64 >= maxlen)
      return;                        // broken;
    if (CameraInfo[iCanonLens] < 65) { // non-Canon lens
      memcpy(imgdata.lens.makernotes.Lens, CameraInfo + iCanonLens, 64);

    } else if (!strncmp((char *)CameraInfo + iCanonLens, "EF-S", 4)) {
      memcpy(imgdata.lens.makernotes.Lens, "EF-S ", 5);
      memcpy(imgdata.lens.makernotes.LensFeatures_pre, "EF-E", 4);
      imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_Canon_EF_S;
      memcpy(imgdata.lens.makernotes.Lens + 5, CameraInfo + iCanonLens + 4, 60);

    } else if (!strncmp((char *)CameraInfo + iCanonLens, "TS-E", 4)) {
      memcpy(imgdata.lens.makernotes.Lens, "TS-E ", 5);
      memcpy(imgdata.lens.makernotes.LensFeatures_pre, "TS-E", 4);
      imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_Canon_EF;
      memcpy(imgdata.lens.makernotes.Lens + 5, CameraInfo + iCanonLens + 4, 60);

    } else if (!strncmp((char *)CameraInfo + iCanonLens, "MP-E", 4)) {
      memcpy(imgdata.lens.makernotes.Lens, "MP-E ", 5);
      memcpy(imgdata.lens.makernotes.LensFeatures_pre, "MP-E", 4);
      imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_Canon_EF;
      memcpy(imgdata.lens.makernotes.Lens + 5, CameraInfo + iCanonLens + 4, 60);

    } else if (!strncmp((char *)CameraInfo + iCanonLens, "EF-M", 4)) {
      memcpy(imgdata.lens.makernotes.Lens, "EF-M ", 5);
      memcpy(imgdata.lens.makernotes.LensFeatures_pre, "EF-M", 4);
      imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_Canon_EF_M;
      memcpy(imgdata.lens.makernotes.Lens + 5, CameraInfo + iCanonLens + 4, 60);

    } else {
      memcpy(imgdata.lens.makernotes.Lens, CameraInfo + iCanonLens, 2);
      memcpy(imgdata.lens.makernotes.LensFeatures_pre, "EF", 2);
      imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_Canon_EF;
      imgdata.lens.makernotes.Lens[2] = 32;
      memcpy(imgdata.lens.makernotes.Lens + 3, CameraInfo + iCanonLens + 2, 62);
    }
  }
  return;
}

void CLASS Canon_CameraSettings(unsigned len)
{
  fseek(ifp, 10, SEEK_CUR);
  imgdata.shootinginfo.DriveMode = get2();
  get2();
  imgdata.shootinginfo.FocusMode = get2();
  fseek(ifp, 18, SEEK_CUR);
  imgdata.shootinginfo.MeteringMode = get2();
  get2();
  imgdata.shootinginfo.AFPoint = get2();
  imgdata.shootinginfo.ExposureMode = get2();
  get2();
  imgdata.lens.makernotes.LensID = get2();
  imgdata.lens.makernotes.MaxFocal = get2();
  imgdata.lens.makernotes.MinFocal = get2();
  imgdata.lens.makernotes.CanonFocalUnits = get2();
  if (imgdata.lens.makernotes.CanonFocalUnits > 1) {
    imgdata.lens.makernotes.MaxFocal /= (float)imgdata.lens.makernotes.CanonFocalUnits;
    imgdata.lens.makernotes.MinFocal /= (float)imgdata.lens.makernotes.CanonFocalUnits;
  }
  imgdata.lens.makernotes.MaxAp = _CanonConvertAperture(get2());
  imgdata.lens.makernotes.MinAp = _CanonConvertAperture(get2());
  fseek(ifp, 12, SEEK_CUR);
  imgdata.shootinginfo.ImageStabilization = get2();
  if (len >= 48) {
    fseek(ifp, 22, SEEK_CUR);
    imgdata.makernotes.canon.SRAWQuality = get2();
  }
}

void CLASS Canon_WBpresets(int skip1, int skip2)
{
  int c;
  FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Daylight][c ^ (c >> 1)] = get2();

  if (skip1)
    fseek(ifp, skip1, SEEK_CUR);
  FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Shade][c ^ (c >> 1)] = get2();

  if (skip1)
    fseek(ifp, skip1, SEEK_CUR);
  FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Cloudy][c ^ (c >> 1)] = get2();

  if (skip1)
    fseek(ifp, skip1, SEEK_CUR);
  FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Tungsten][c ^ (c >> 1)] = get2();

  if (skip1)
    fseek(ifp, skip1, SEEK_CUR);
  FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_W][c ^ (c >> 1)] = get2();

  if (skip2)
    fseek(ifp, skip2, SEEK_CUR);
  FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][c ^ (c >> 1)] = get2();

  return;
}

void CLASS Canon_WBCTpresets(short WBCTversion) {

  int i;
  float norm;

  if (WBCTversion == 0) { // tint, as shot R, as shot B, CСT
      for (i = 0; i < 15; i++) {
        imgdata.color.WBCT_Coeffs[i][2] = imgdata.color.WBCT_Coeffs[i][4] = 1.0f;
        fseek(ifp, 2, SEEK_CUR);
        imgdata.color.WBCT_Coeffs[i][1] = 1024.0f / fMAX(get2(), 1.f);
        imgdata.color.WBCT_Coeffs[i][3] = 1024.0f / fMAX(get2(), 1.f);
        imgdata.color.WBCT_Coeffs[i][0] = get2();
    }

  } else if (WBCTversion == 1) { // as shot R, as shot B, tint, CСT
      for (i = 0; i < 15; i++) {
        imgdata.color.WBCT_Coeffs[i][2] = imgdata.color.WBCT_Coeffs[i][4] = 1.0f;
        imgdata.color.WBCT_Coeffs[i][1] = 1024.0f / fMAX(get2(), 1.f);
        imgdata.color.WBCT_Coeffs[i][3] = 1024.0f / fMAX(get2(), 1.f);
        fseek(ifp, 2, SEEK_CUR);
        imgdata.color.WBCT_Coeffs[i][0] = get2();
      }

  } else if (WBCTversion == 2) { // tint, offset, as shot R, as shot B, CСT
      if ((unique_id == 0x80000374) || // M3
          (unique_id == 0x80000384) || // M10
          (imgdata.makernotes.canon.CanonColorDataSubVer == 0xfffc)) {
          for (i = 0; i < 15; i++) {
            fseek(ifp, 4, SEEK_CUR);
            imgdata.color.WBCT_Coeffs[i][2] = imgdata.color.WBCT_Coeffs[i][4] = 1.0f;
            imgdata.color.WBCT_Coeffs[i][1] = 1024.0f / fMAX(1.f, get2());
            imgdata.color.WBCT_Coeffs[i][3] = 1024.0f / fMAX(1.f, get2());
            imgdata.color.WBCT_Coeffs[i][0] = get2();
          }
      } else if (imgdata.makernotes.canon.CanonColorDataSubVer == 0xfffd) {
          for (i = 0; i < 15; i++) {
            fseek(ifp, 2, SEEK_CUR);
            norm = (signed short)get2();
            norm = 512.0f + norm / 8.0f;
            imgdata.color.WBCT_Coeffs[i][2] = imgdata.color.WBCT_Coeffs[i][4] = 1.0f;
            imgdata.color.WBCT_Coeffs[i][1] = (float)get2();
            if(norm>0.001f) imgdata.color.WBCT_Coeffs[i][1] /= norm;
            imgdata.color.WBCT_Coeffs[i][3] = (float)get2();
            if(norm>0.001f) imgdata.color.WBCT_Coeffs[i][3] /= norm;
            imgdata.color.WBCT_Coeffs[i][0] = get2();
          }
      }
    }
  return;
}

void CLASS processNikonLensData(uchar *LensData, unsigned len) {

  ushort i;

  if (!(imgdata.lens.nikon.NikonLensType & 0x01)) {
    imgdata.lens.makernotes.LensFeatures_pre[0] = 'A';
    imgdata.lens.makernotes.LensFeatures_pre[1] = 'F';

  } else {
    imgdata.lens.makernotes.LensFeatures_pre[0] = 'M';
    imgdata.lens.makernotes.LensFeatures_pre[1] = 'F';
  }

  if (imgdata.lens.nikon.NikonLensType & 0x02) {
    if (imgdata.lens.nikon.NikonLensType & 0x04)
      imgdata.lens.makernotes.LensFeatures_suf[0] = 'G';
    else
      imgdata.lens.makernotes.LensFeatures_suf[0] = 'D';
    imgdata.lens.makernotes.LensFeatures_suf[1] = ' ';
  }

  if (imgdata.lens.nikon.NikonLensType & 0x08) {
    imgdata.lens.makernotes.LensFeatures_suf[2] = 'V';
    imgdata.lens.makernotes.LensFeatures_suf[3] = 'R';
  }

  if (imgdata.lens.nikon.NikonLensType & 0x10) {
    imgdata.lens.makernotes.LensMount = imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Nikon_CX;
    imgdata.lens.makernotes.CameraFormat = imgdata.lens.makernotes.LensFormat = LIBRAW_FORMAT_1INCH;
  } else
    imgdata.lens.makernotes.LensMount = imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Nikon_F;

  if (imgdata.lens.nikon.NikonLensType & 0x20) {
    strcpy(imgdata.lens.makernotes.Adapter, "FT-1");
    imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_Nikon_F;
    imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Nikon_CX;
    imgdata.lens.makernotes.CameraFormat = LIBRAW_FORMAT_1INCH;
  }

  imgdata.lens.nikon.NikonLensType = imgdata.lens.nikon.NikonLensType & 0xdf;

  if ((len < 20) || (len == 58)) {
    switch (len) {
    case 9:
      i = 2;
      break;
    case 15:
      i = 7;
      break;
    case 16:
      i = 8;
      break;
    case 58:
      i = 1;
      while ((LensData[i] == LensData[0]) && (i < 17)) i++;
      if (i == 17) {
        if (fabsf(imgdata.lens.makernotes.CurFocal) < 1.1f)
          imgdata.lens.makernotes.CurFocal = sget2(LensData + 56);
        if (imgdata.lens.makernotes.CurAp < 0.7f)
          imgdata.lens.makernotes.CurAp = libraw_powf64l(2.0f, (float)sget2(LensData + 52) / 384.0f -1.0f);
        if (imgdata.lens.makernotes.MaxAp4CurFocal < 0.7f)
          imgdata.lens.makernotes.MaxAp4CurFocal = libraw_powf64l(2.0f, (float)sget2(LensData + 50) / 384.0f -1.0f);
        return;
      }
      i = 9;
      break;
    }
    imgdata.lens.nikon.NikonLensIDNumber = LensData[i];
    imgdata.lens.nikon.NikonLensFStops = LensData[i + 1];
    imgdata.lens.makernotes.LensFStops = (float)imgdata.lens.nikon.NikonLensFStops / 12.0f;
    if (fabsf(imgdata.lens.makernotes.MinFocal) < 1.1f) {
      if ((imgdata.lens.nikon.NikonLensType ^ (uchar)0x01) || LensData[i + 2])
        imgdata.lens.makernotes.MinFocal = 5.0f * libraw_powf64l(2.0f, (float)LensData[i + 2] / 24.0f);
      if ((imgdata.lens.nikon.NikonLensType ^ (uchar)0x01) || LensData[i + 3])
        imgdata.lens.makernotes.MaxFocal = 5.0f * libraw_powf64l(2.0f, (float)LensData[i + 3] / 24.0f);
      if ((imgdata.lens.nikon.NikonLensType ^ (uchar)0x01) || LensData[i + 4])
        imgdata.lens.makernotes.MaxAp4MinFocal = libraw_powf64l(2.0f, (float)LensData[i + 4] / 24.0f);
      if ((imgdata.lens.nikon.NikonLensType ^ (uchar)0x01) || LensData[i + 5])
        imgdata.lens.makernotes.MaxAp4MaxFocal = libraw_powf64l(2.0f, (float)LensData[i + 5] / 24.0f);
    }
    imgdata.lens.nikon.NikonMCUVersion = LensData[i + 6];
    if (i != 2) {
      if ((LensData[i - 1]) && (fabsf(imgdata.lens.makernotes.CurFocal) < 1.1f))
        imgdata.lens.makernotes.CurFocal = 5.0f * libraw_powf64l(2.0f, (float)LensData[i - 1] / 24.0f);
      if (LensData[i + 7])
        imgdata.lens.nikon.NikonEffectiveMaxAp = libraw_powf64l(2.0f, (float)LensData[i + 7] / 24.0f);
    }
    imgdata.lens.makernotes.LensID =
        (unsigned long long)LensData[i] << 56     | (unsigned long long)LensData[i + 1] << 48 |
        (unsigned long long)LensData[i + 2] << 40 | (unsigned long long)LensData[i + 3] << 32 |
        (unsigned long long)LensData[i + 4] << 24 | (unsigned long long)LensData[i + 5] << 16 |
        (unsigned long long)LensData[i + 6] << 8  | (unsigned long long)imgdata.lens.nikon.NikonLensType;

  } else if ((len == 459) || (len == 590)) {
    memcpy(imgdata.lens.makernotes.Lens, LensData + 390, 64);

  } else if (len == 509) {
    memcpy(imgdata.lens.makernotes.Lens, LensData + 391, 64);

  } else if (len == 879) {
    memcpy(imgdata.lens.makernotes.Lens, LensData + 680, 64);
  }

  return;
}

void CLASS Nikon_NRW_WBtag (int wb, int skip)
{
#define icWB imgdata.color.WB_Coeffs
  int r, g0, g1, b;
  if (skip) get4(); // skip wb "CCT", it is not unique
  r = get4();
  g0 = get4();
  g1 = get4();
  b = get4();
  if  (r && g0 && g1 && b) {
    icWB[wb][0] = r << 1;
    icWB[wb][1] = g0;
    icWB[wb][2] = b << 1;
    icWB[wb][3] = g1;
  }
  return;
#undef icWB
}

void CLASS parseNikonMakernote (int base, int uptag, unsigned dng_writer)
{
#define imn imgdata.makernotes.nikon
#define ilm imgdata.lens.makernotes
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

  if (!strcmp(buf, "Nikon")) {
    if (buf[6] != '\2') return;
    base = ftell(ifp);
    order = get2();
    if (get2() != 42) goto quit;
    offset = get4();
    fseek(ifp, offset - 8, SEEK_CUR);

  } else {
    fseek(ifp, -10, SEEK_CUR);
  }

  entries = get2();
  if (entries > 1000) return;
  morder = order;

  while (entries--) {
    order = morder;
    tiff_get(base, &tag, &type, &len, &save);
    INT64 pos = ifp->tell();
    if (len > 8 && pos + len > 2 * fsize) {
      fseek(ifp, save, SEEK_SET); // Recover tiff-read position!!
      continue;
    }
    tag |= uptag << 16;
    if (len > 100 * 1024 * 1024)
      goto next; // 100Mb tag? No!

    if (tag == 0x0002) {
      if (!iso_speed) iso_speed = (get2(), get2());

    } else if (tag == 0x000a) {
/*
  B700, P330, P340, P6000, P7000, P7700, P7800
  E5000, E5400, E5700, E8700, E8800
*/
      ilm.LensMount = ilm.CameraMount = LIBRAW_MOUNT_FixedLens;
      ilm.FocalType = LIBRAW_FT_ZOOM;

    } else if (tag == 0x000c) {
/*
  1 AW1, 1 J1, 1 J2, 1 J3, 1 J4, 1 J5, 1 S1, 1 S2, 1 V1, 1 V2, 1 V3
  D1, D1H, D1X, D2H, D2Xs, D3, D3S, D3X, D4, D4S, Df, D5
  D600, D610, D700, D750, D800, D800E, D810, D850
  D200, D300, D300S, D500
  D40, D40X, D60, D80, D90
  D3000, D3100, D3200, D3300, D3400
  D5000, D5100, D5200, D5300, D5500, D5600
  D7000, D7100, D7200, D7500
  B700, COOLPIX A, P330, P340, P7700, P7800
*/
      cam_mul[0] = getreal(type);
      cam_mul[2] = getreal(type);
      cam_mul[1] = getreal(type);
      cam_mul[3] = getreal(type);

    } else if (tag == 0x11) {
      if (is_raw) {
        fseek(ifp, get4() + base, SEEK_SET);
        parse_tiff_ifd(base);
      }

    } else if (tag == 0x0012) {
      ci = fgetc(ifp);
      cj = fgetc(ifp);
      ck = fgetc(ifp);
      if (ck)
        imgdata.other.FlashEC = (float)(ci * cj) / (float)ck;

    } else if (tag == 0x0014) {
      if (type == 7) {
        if (len == 2560) { // E5400, E8400, E8700, E8800
          fseek(ifp, 1248, SEEK_CUR);
          order = 0x4d4d;
          cam_mul[0] = get2() / 256.0;
          cam_mul[2] = get2() / 256.0;
          cam_mul[1] = cam_mul[3] = 1.0;
          icWB[LIBRAW_WBI_Auto][0] = get2();
          icWB[LIBRAW_WBI_Auto][2] = get2();
          icWB[LIBRAW_WBI_Daylight][0] = get2();
          icWB[LIBRAW_WBI_Daylight][2] = get2();
          fseek (ifp, 24, SEEK_CUR);
          icWB[LIBRAW_WBI_Tungsten][0] = get2();
          icWB[LIBRAW_WBI_Tungsten][2] = get2();
          fseek (ifp, 24, SEEK_CUR);
          icWB[LIBRAW_WBI_FL_W][0] = get2();
          icWB[LIBRAW_WBI_FL_W][2] = get2();
          icWB[LIBRAW_WBI_FL_N][0] = get2();
          icWB[LIBRAW_WBI_FL_N][2] = get2();
          icWB[LIBRAW_WBI_FL_D][0] = get2();
          icWB[LIBRAW_WBI_FL_D][2] = get2();
          icWB[LIBRAW_WBI_Cloudy][0] = get2();
          icWB[LIBRAW_WBI_Cloudy][2] = get2();
          fseek (ifp, 24, SEEK_CUR);
          icWB[LIBRAW_WBI_Flash][0] = get2();
          icWB[LIBRAW_WBI_Flash][2] = get2();

          icWB[LIBRAW_WBI_Auto][1] = icWB[LIBRAW_WBI_Auto][3] =
            icWB[LIBRAW_WBI_Daylight][1] = icWB[LIBRAW_WBI_Daylight][3] =
            icWB[LIBRAW_WBI_Tungsten][1] = icWB[LIBRAW_WBI_Tungsten][3] =
            icWB[LIBRAW_WBI_FL_W][1] = icWB[LIBRAW_WBI_FL_W][3] =
            icWB[LIBRAW_WBI_FL_N][1] = icWB[LIBRAW_WBI_FL_N][3] =
            icWB[LIBRAW_WBI_FL_D][1] = icWB[LIBRAW_WBI_FL_D][3] =
            icWB[LIBRAW_WBI_Cloudy][1] = icWB[LIBRAW_WBI_Cloudy][3] =
            icWB[LIBRAW_WBI_Flash][1] = icWB[LIBRAW_WBI_Flash][3] = 256;

          if (strncmp(model, "E8700", 5)) {
            fseek (ifp, 24, SEEK_CUR);
            icWB[LIBRAW_WBI_Shade][0] = get2();
            icWB[LIBRAW_WBI_Shade][2] = get2();
            icWB[LIBRAW_WBI_Shade][1] = icWB[LIBRAW_WBI_Shade][3] = 256;
          }

        } else if (len == 1280) { // E5000, E5700
          cam_mul[0] = cam_mul[1] = cam_mul[2] = cam_mul[3] = 1.0;

        } else {
          fread(buf, 1, 10, ifp);
          if (!strncmp(buf, "NRW ", 4)) {   // P6000, P7000, P7100, B700, P1000
            if (!strcmp(buf + 4, "0100")) { // P6000
              fseek(ifp, 0x13de, SEEK_CUR);
              cam_mul[0] = get4() << 1;
              cam_mul[1] = get4();
              cam_mul[3] = get4();
              cam_mul[2] = get4() << 1;
              Nikon_NRW_WBtag (LIBRAW_WBI_Daylight, 0);
              Nikon_NRW_WBtag (LIBRAW_WBI_Cloudy, 0);
              fseek(ifp, 16, SEEK_CUR);
              Nikon_NRW_WBtag (LIBRAW_WBI_Tungsten, 0);
              Nikon_NRW_WBtag (LIBRAW_WBI_FL_W, 0);
              Nikon_NRW_WBtag (LIBRAW_WBI_Flash, 0);
              fseek(ifp, 16, SEEK_CUR);
              Nikon_NRW_WBtag (LIBRAW_WBI_Custom, 0);
              Nikon_NRW_WBtag (LIBRAW_WBI_Auto, 0);

            } else {                        // P7000, P7100, B700, P1000
              fseek(ifp, 0x16, SEEK_CUR);
              black = get2();
              fseek(ifp, 0x16, SEEK_CUR);
              cam_mul[0] = get4() << 1;
              cam_mul[1] = get4();
              cam_mul[3] = get4();
              cam_mul[2] = get4() << 1;
              Nikon_NRW_WBtag (LIBRAW_WBI_Daylight, 1);
              Nikon_NRW_WBtag (LIBRAW_WBI_Cloudy, 1);
              Nikon_NRW_WBtag (LIBRAW_WBI_Shade, 1);
              Nikon_NRW_WBtag (LIBRAW_WBI_Tungsten, 1);
              Nikon_NRW_WBtag (LIBRAW_WBI_FL_W, 1);
              Nikon_NRW_WBtag (LIBRAW_WBI_FL_N, 1);
              Nikon_NRW_WBtag (LIBRAW_WBI_FL_D, 1);
              Nikon_NRW_WBtag (LIBRAW_WBI_HT_Mercury, 1);
              fseek(ifp, 20, SEEK_CUR);
              Nikon_NRW_WBtag (LIBRAW_WBI_Custom, 1);
              Nikon_NRW_WBtag (LIBRAW_WBI_Auto, 1);
            }
          }
        }
      }

    } else if (tag == 0x001b) {
      imn.CropFormat = get2();
      FORC(6) imn.CropData[c] = get2(); /* box inside CropData ([2], [3]): upper left pixel (x,y), size (width,height) */

    } else if (tag == 0x001d) { // serial number
      if (len > 0) {
        while ((c = fgetc(ifp)) && (len-- > 0) && (c != EOF)) {
          if ((!custom_serial) && (!isdigit(c))) {
            if ((strbuflen(model) == 3) && (!strcmp(model, "D50"))) {
              custom_serial = 34;
            } else {
              custom_serial = 96;
            }
          }
          serial = serial * 10 + (isdigit(c) ? c - '0' : c % 10);
        }
        if (!imgdata.shootinginfo.BodySerial[0])
          sprintf(imgdata.shootinginfo.BodySerial, "%d", serial);
      }

    } else if (tag == 0x0025) {
      if (!iso_speed || (iso_speed == 65535)) {
        iso_speed = int(100.0 * libraw_powf64l(2.0f, double((uchar)fgetc(ifp)) / 12.0 - 5.0));
      }

    } else if (tag == 0x003b) { // all 1s for regular exposures
        imn.ME_WB[0] = getreal(type);
        imn.ME_WB[2] = getreal(type);
        imn.ME_WB[1] = getreal(type);
        imn.ME_WB[3] = getreal(type);

    } else if (tag == 0x03d) { // not corrected for file bitcount, to be patched in open_datastream
      FORC4 cblack[c ^ c >> 1] = get2();
      i = cblack[3];
      FORC3 if (i > cblack[c]) i = cblack[c];
      FORC4 cblack[c] -= i;
      black += i;

    } else if (tag == 0x0045) { /* box inside CropData ([2], [3]): upper left pixel (x,y), size (width,height) */
        imgdata.sizes.raw_crop.cleft = get2();
        imgdata.sizes.raw_crop.ctop = get2();
        imgdata.sizes.raw_crop.cwidth = get2();
        imgdata.sizes.raw_crop.cheight = get2();

    } else if (tag == 0x0082) { // lens attachment
      stmread(ilm.Attachment, len, ifp);

    } else if (tag == 0x0083) { // lens type
      imgdata.lens.nikon.NikonLensType = fgetc(ifp);

    } else if (tag == 0x0084) { // lens
      ilm.MinFocal = getreal(type);
      ilm.MaxFocal = getreal(type);
      ilm.MaxAp4MinFocal = getreal(type);
      ilm.MaxAp4MaxFocal = getreal(type);

    } else if (tag == 0x008b) { // lens f-stops
      ci = fgetc(ifp);
      cj = fgetc(ifp);
      ck = fgetc(ifp);
      if (ck) {
        imgdata.lens.nikon.NikonLensFStops = ci * cj * (12 / ck);
        ilm.LensFStops = (float)imgdata.lens.nikon.NikonLensFStops / 12.0f;
      }

    } else if ((tag == 0x008c) || (tag == 0x0096)) {
      meta_offset = ftell(ifp);

    } else if (tag == 0x0093) {
      imn.NEFCompression = i = get2();
      if ((i == 7) || (i == 9)) {
        ilm.LensMount = LIBRAW_MOUNT_FixedLens;
        ilm.CameraMount = LIBRAW_MOUNT_FixedLens;
      }

    } else if (tag == 0x0097) { // ver97
      FORC4 imn.ColorBalanceVersion = imn.ColorBalanceVersion * 10 + fgetc(ifp) - '0';
      switch (imn.ColorBalanceVersion) {
      case 100:
        fseek(ifp, 68, SEEK_CUR);
        FORC4 cam_mul[(c >> 1) | ((c & 1) << 1)] = get2();
        break;
      case 102:
        fseek(ifp, 6, SEEK_CUR);
        FORC4 cam_mul[c ^ (c >> 1)] = get2();
        break;
      case 103:
        fseek(ifp, 16, SEEK_CUR);
        FORC4 cam_mul[c] = get2();
      }
      if (imn.ColorBalanceVersion >= 200) {
        if (imn.ColorBalanceVersion != 205) {
          fseek(ifp, 280, SEEK_CUR);
        }
        ColorBalanceData_ready = (fread(ColorBalanceData_buf, 324, 1, ifp) == 324);
      }
      if ((imn.ColorBalanceVersion >= 400) &&
          (imn.ColorBalanceVersion <= 405)) { // 1 J1, 1 V1, 1 J2, 1 V2, 1 J3, 1 S1, 1 AW1, 1 S2, 1 J4, 1 V3, 1 J5
        ilm.CameraFormat = LIBRAW_FORMAT_1INCH;
        ilm.CameraMount = LIBRAW_MOUNT_Nikon_CX;
      } else if ((imn.ColorBalanceVersion >= 500) &&
                 (imn.ColorBalanceVersion <= 502)) { // P7700, P7800, P330, P340
        ilm.CameraMount = ilm.LensMount = LIBRAW_MOUNT_FixedLens;
        ilm.FocalType = LIBRAW_FT_ZOOM;
      } else if (imn.ColorBalanceVersion == 601) { // Coolpix A
        ilm.CameraFormat = ilm.LensFormat = LIBRAW_FORMAT_APSC;
        ilm.CameraMount = ilm.LensMount = LIBRAW_MOUNT_FixedLens;
        ilm.FocalType = LIBRAW_FT_FIXED;
      } else if (imn.ColorBalanceVersion == 800) { // "Z 7"
        ilm.CameraFormat = LIBRAW_FORMAT_FF;
        ilm.CameraMount = LIBRAW_MOUNT_Nikon_Z;
      }

    } else if (tag == 0x0098) { // contains lens data
      FORC4 imn.LensDataVersion = imn.LensDataVersion * 10 + fgetc(ifp) - '0';
      switch (imn.LensDataVersion) {
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
      if (LensData_len) {
        LensData_buf = (uchar *)malloc(LensData_len);
        fread(LensData_buf, LensData_len, 1, ifp);
      }

    } else if (tag == 0x00a0) {
        stmread(imgdata.shootinginfo.BodySerial, len, ifp);

    } else if (tag == 0x00a7) { // shutter count
      imn.NikonKey = fgetc(ifp) ^ fgetc(ifp) ^ fgetc(ifp) ^ fgetc(ifp);
      if (custom_serial) {
        ci = xlat[0][custom_serial];
      } else {
        ci = xlat[0][serial & 0xff];
      }
      cj = xlat[1][imn.NikonKey];
      ck = 0x60;
      if (((unsigned)(imn.ColorBalanceVersion - 200) < 18) && ColorBalanceData_ready) {
        for (i = 0; i < 324; i++)
          ColorBalanceData_buf[i] ^= (cj += ci * ck++);
        i = "66666>666;6A;:;555"[imn.ColorBalanceVersion - 200] - '0';
        FORC4 cam_mul[c ^ (c >> 1) ^ (i & 1)] = sget2(ColorBalanceData_buf + (i & -2) + c * 2);
      }
      if (LensData_len) {
        if (imn.LensDataVersion > 200) {
          for (i = 0; i < LensData_len; i++) {
            LensData_buf[i] ^= (cj += ci * ck++);
          }
        }
        processNikonLensData(LensData_buf, LensData_len);
        LensData_len = 0;
        free(LensData_buf);
      }

    } else if (tag == 0x00a8) { // contains flash data
      FORC4 imn.FlashInfoVersion = imn.FlashInfoVersion * 10 + fgetc(ifp) - '0';

    } else if (tag == 0x00b0) {
      get4(); // ME tag version, 4 symbols
      imn.ExposureMode = get4();
      imn.nMEshots = get4();
      imn.MEgainOn = get4();

    } else if (tag == 0x00b9) {
      imn.AFFineTune = fgetc(ifp);
      imn.AFFineTuneIndex = fgetc(ifp);
      imn.AFFineTuneAdj = (int8_t)fgetc(ifp);

    } else if ((tag == 0x100) && (type == 7 )) {
      thumb_offset = ftell(ifp);
      thumb_length = len;

    } else if (tag == 0x0e01) { /* Nikon Software / in-camera edit Note */
      int loopc = 0;
      int WhiteBalanceAdj_active = 0;
      order = 0x4949;
      fseek(ifp, 22, SEEK_CUR);
      for (offset = 22; offset + 22 < len; offset += 22 + i) {
        if (loopc++ > 1024)
          throw LIBRAW_EXCEPTION_IO_CORRUPT;
        tag = get4();
        fseek(ifp, 14, SEEK_CUR);
        i = get4() - 4;

        if (tag == 0x76a43204) {
          WhiteBalanceAdj_active = fgetc(ifp);

        } else if (tag == 0xbf3c6c20) {
          if (WhiteBalanceAdj_active) {
            cam_mul[0] = getreal(12);
            cam_mul[2] = getreal(12);
            cam_mul[1] = cam_mul[3] = 1.0;
            i -= 16;
          }
          fseek(ifp, i, SEEK_CUR);

        } else if (tag == 0x76a43207) {
          flip = get2();

        } else {
          fseek(ifp, i, SEEK_CUR);
        }
      }

    } else if (tag == 0x0e22) {
      FORC4 imn.NEFBitDepth[c] = get2();
    }
next:
    fseek(ifp, save, SEEK_SET);
  }
quit:
  order = sorder;
#undef icWB
#undef ilm
#undef imn
}


void CLASS setOlympusBodyFeatures(unsigned long long id)
{
  imgdata.lens.makernotes.CamID = id;
  if (id == 0x5330303638ULL)
    strcpy(model, "E-M10MarkIII");

  if ((id == 0x4434303430ULL) || // E-1
      (id == 0x4434303431ULL) || // E-300
      ((id & 0x00ffff0000ULL) == 0x0030300000ULL)) {
    imgdata.lens.makernotes.CameraFormat = LIBRAW_FORMAT_FT;

    if ((id == 0x4434303430ULL) ||                              // E-1
        (id == 0x4434303431ULL) ||                              // E-330
        ((id >= 0x5330303033ULL) && (id <= 0x5330303138ULL)) || // E-330 to E-520
        (id == 0x5330303233ULL) ||                              // E-620
        (id == 0x5330303239ULL) ||                              // E-450
        (id == 0x5330303330ULL) ||                              // E-600
        (id == 0x5330303333ULL)) {                              // E-5
      imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_FT;

    } else {
      imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_mFT;
    }

  } else {
    imgdata.lens.makernotes.LensMount = imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_FixedLens;
  }
  return;
}

void CLASS getOlympus_CameraType2 ()
{

  if (OlyID != 0x0ULL) return;

  int i=0;

  fread(imgdata.makernotes.olympus.CameraType2, 6, 1, ifp);
  while ((i < 6) &&
         imgdata.makernotes.olympus.CameraType2[i]) {
     OlyID = OlyID << 8 | imgdata.makernotes.olympus.CameraType2[i];
     if (isspace(imgdata.makernotes.olympus.CameraType2[i+1]))
       imgdata.makernotes.olympus.CameraType2[i+1] = '\0';
     i++;
  }
  setOlympusBodyFeatures(OlyID);
  return;
}

void CLASS getOlympus_SensorTemperature (unsigned len)
{
  if (OlyID != 0x0ULL) {
    short temp = get2();
    if ((OlyID == 0x4434303430ULL) || // E-1
        (OlyID == 0x5330303336ULL) || // E-M5
        (len != 1))
      imgdata.other.SensorTemperature = (float)temp;
    else if ((temp != -32768) && (temp != 0)) {
      if (temp > 199)
        imgdata.other.SensorTemperature = 86.474958f - 0.120228f * (float)temp;
      else
        imgdata.other.SensorTemperature = (float)temp;
    }
  }
  return;
}

void CLASS parseOlympus_Equipment (unsigned tag, unsigned type, unsigned len, unsigned dng_writer)
{
// uptag 2010

  int i;

  switch (tag) {
  case 0x0100:
    getOlympus_CameraType2();
    break;
  case 0x0101:
    if ((!imgdata.shootinginfo.BodySerial[0]) &&
        (dng_writer == nonDNG))
      stmread(imgdata.shootinginfo.BodySerial, len, ifp);
    break;
  case 0x0102:
    stmread(imgdata.shootinginfo.InternalBodySerial, len, ifp);
    break;
  case 0x0201:
    {
      unsigned char bits[4];
      fread(bits,1,4,ifp);
    imgdata.lens.makernotes.LensID =
      (unsigned long long)bits[0] << 16 |
      (unsigned long long)bits[2] << 8 |
      (unsigned long long)bits[3];
    imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_FT;
    imgdata.lens.makernotes.LensFormat = LIBRAW_FORMAT_FT;
    if (((imgdata.lens.makernotes.LensID < 0x20000) ||
         (imgdata.lens.makernotes.LensID > 0x4ffff)) &&
        (imgdata.lens.makernotes.LensID & 0x10))
      imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_mFT;
    }
    break;
  case 0x0202:
    if ((!imgdata.lens.LensSerial[0]))
      stmread(imgdata.lens.LensSerial, len, ifp);
    break;
  case 0x0203:
    stmread(imgdata.lens.makernotes.Lens, len, ifp);
    break;
  case 0x0205:
    imgdata.lens.makernotes.MaxAp4MinFocal = libraw_powf64l(sqrt(2.0f), get2() / 256.0f);
    break;
  case 0x0206:
    imgdata.lens.makernotes.MaxAp4MaxFocal = libraw_powf64l(sqrt(2.0f), get2() / 256.0f);
    break;
  case 0x0207:
    imgdata.lens.makernotes.MinFocal = (float)get2();
    break;
  case 0x0208:
    imgdata.lens.makernotes.MaxFocal = (float)get2();
    if (imgdata.lens.makernotes.MaxFocal > 1000.0f)
      imgdata.lens.makernotes.MaxFocal = imgdata.lens.makernotes.MinFocal;
    break;
  case 0x020a:
    imgdata.lens.makernotes.MaxAp4CurFocal = libraw_powf64l(sqrt(2.0f), get2() / 256.0f);
    break;
  case 0x0301:
    imgdata.lens.makernotes.TeleconverterID = fgetc(ifp) << 8;
    fgetc(ifp);
    imgdata.lens.makernotes.TeleconverterID = imgdata.lens.makernotes.TeleconverterID | fgetc(ifp);
    break;
  case 0x0303:
    stmread(imgdata.lens.makernotes.Teleconverter, len, ifp);
    break;
  case 0x0403:
    stmread(imgdata.lens.makernotes.Attachment, len, ifp);
    break;
  }

  return;
}
void CLASS parseOlympus_CameraSettings (int base, unsigned tag, unsigned type, unsigned len, unsigned dng_writer)
{
// uptag 0x2020

  int c;
  uchar uc;
  switch (tag) {
  case 0x0101:
    if (dng_writer == nonDNG) {
      thumb_offset = get4() + base;
    }
    break;
  case 0x0102:
    if (dng_writer == nonDNG) {
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
    imgdata.shootinginfo.FocusMode =
    imgdata.makernotes.olympus.FocusMode[0] = get2();
    if (len == 2) {
      imgdata.makernotes.olympus.FocusMode[1] = get2();
    }
    break;
  case 0x0304:
    for (c = 0; c < 64; c++) {
      imgdata.makernotes.olympus.AFAreas[c] = get4();
    }
    break;
  case 0x0305:
    for (c = 0; c < 5; c++) {
      imgdata.makernotes.olympus.AFPointSelected[c] = getreal(type);
    }
    break;
  case 0x0306:
    fread(&uc, 1, 1, ifp);
    imgdata.makernotes.olympus.AFFineTune = uc;
    break;
  case 0x0307:
    FORC3 imgdata.makernotes.olympus.AFFineTuneAdj[c] = get2();
    break;
  case 0x0401:
    imgdata.other.FlashEC = getreal(type);
    break;
  case 0x0507:
    imgdata.makernotes.olympus.ColorSpace = get2();
    break;
  case 0x0600:
    imgdata.shootinginfo.DriveMode =
    imgdata.makernotes.olympus.DriveMode[0] = get2();
    for (c = 1; c < len; c++) {
      imgdata.makernotes.olympus.DriveMode[c] = get2();
    }
    break;
  case 0x0604:
    imgdata.shootinginfo.ImageStabilization = get4();
    break;
  }

  return;
}

void CLASS parseOlympus_ImageProcessing (unsigned tag, unsigned type, unsigned len, unsigned dng_writer)
{
// uptag 0x2040

  int i, c, wb[4], nWB, tWB, wbG;
  ushort CT;
  short sorder;

    if ((tag == 0x0100) && (dng_writer == nonDNG)) {
      cam_mul[0] = get2() / 256.0;
      cam_mul[2] = get2() / 256.0;

  } else if ((tag == 0x0101) &&
             (len == 2)      &&
             (!strncasecmp(model, "E-410", 5) ||
              !strncasecmp(model, "E-510", 5))) {
      for (i = 0; i < 64; i++) {
        imgdata.color.WBCT_Coeffs[i][2] =
          imgdata.color.WBCT_Coeffs[i][4] =
          imgdata.color.WB_Coeffs[i][1] =
          imgdata.color.WB_Coeffs[i][3] = 0x100;
      }
      for (i = 64; i < 256; i++) {
        imgdata.color.WB_Coeffs[i][1] =
          imgdata.color.WB_Coeffs[i][3] = 0x100;
      }

  } else if ((tag > 0x0101) &&
             (tag <= 0x0111)) {
      nWB = tag - 0x0101;
      tWB = Oly_wb_list2[nWB << 1];
      CT = Oly_wb_list2[(nWB << 1) | 1];
      wb[0] = get2();
      wb[2] = get2();
      if (tWB != 0x100) {
        imgdata.color.WB_Coeffs[tWB][0] = wb[0];
        imgdata.color.WB_Coeffs[tWB][2] = wb[2];
      }
      if (CT) {
        imgdata.color.WBCT_Coeffs[nWB - 1][0] = CT;
        imgdata.color.WBCT_Coeffs[nWB - 1][1] = wb[0];
        imgdata.color.WBCT_Coeffs[nWB - 1][3] = wb[2];
      }
      if (len == 4) {
        wb[1] = get2();
        wb[3] = get2();
        if (tWB != 0x100) {
          imgdata.color.WB_Coeffs[tWB][1] = wb[1];
          imgdata.color.WB_Coeffs[tWB][3] = wb[3];
        }
        if (CT) {
          imgdata.color.WBCT_Coeffs[nWB - 1][2] = wb[1];
          imgdata.color.WBCT_Coeffs[nWB - 1][4] = wb[3];
        }
      }

  } else if ((tag >= 0x0112) &&
             (tag <= 0x011e)) {
      nWB = tag - 0x0112;
      wbG = get2();
      tWB = Oly_wb_list2[nWB << 1];
      if (nWB)
        imgdata.color.WBCT_Coeffs[nWB - 1][2] =
          imgdata.color.WBCT_Coeffs[nWB - 1][4] = wbG;
      if (tWB != 0x100)
        imgdata.color.WB_Coeffs[tWB][1] =
          imgdata.color.WB_Coeffs[tWB][3] = wbG;

  } else if (tag == 0x011f) {
      wbG = get2();
      if (imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][0])
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][1] =
          imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][3] = wbG;
      FORC4 if (imgdata.color.WB_Coeffs[LIBRAW_WBI_Custom1 + c][0])
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Custom1 + c][1] =
          imgdata.color.WB_Coeffs[LIBRAW_WBI_Custom1 + c][3] = wbG;

  } else if (tag == 0x0121) {
      imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][0] = get2();
      imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][2] = get2();
      if (len == 4) {
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][1] = get2();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][3] = get2();
      }

  } else if ((tag == 0x0200)        &&
             (dng_writer == nonDNG) &&
             strcmp(software, "v757-71")) {
      for (i = 0; i < 3; i++) {
        if (!imgdata.makernotes.olympus.ColorSpace) {
          FORC3 cmatrix[i][c] = ((short)get2()) / 256.0;
        } else {
          FORC3 imgdata.color.ccm[i][c] = ((short)get2()) / 256.0;
        }
      }

  } else if ((tag == 0x0600) && (dng_writer == nonDNG)) {
      FORC4 cblack[c ^ c >> 1] = get2();
  } else if ((tag == 0x0612) && (dng_writer == nonDNG)) {
        imgdata.sizes.raw_crop.cleft = get2();
  } else if ((tag == 0x0613)  && (dng_writer == nonDNG)){
        imgdata.sizes.raw_crop.ctop = get2();
  } else if ((tag == 0x0614)  && (dng_writer == nonDNG)){
        imgdata.sizes.raw_crop.cwidth = get2();
  } else if ((tag == 0x0615)  && (dng_writer == nonDNG)){
        imgdata.sizes.raw_crop.cheight = get2();

  } else if ((tag == 0x0805) && (len == 2)) {
      imgdata.makernotes.olympus.OlympusSensorCalibration[0] = getreal(type);
      imgdata.makernotes.olympus.OlympusSensorCalibration[1] = getreal(type);
      if (dng_writer == nonDNG)
        FORC4 imgdata.color.linear_max[c] = imgdata.makernotes.olympus.OlympusSensorCalibration[0];

  } else if (tag == 0x1112) {
        sorder = order;
        order = 0x4d4d;
        imgdata.makernotes.olympus.OlympusCropID = get2();
        order = sorder;
  } else if (tag == 0x1113) {
        FORC4 imgdata.makernotes.olympus.OlympusFrame[c] = get2();

  } else if (tag == 0x1306) {
       c = get2();
       if ((c != 0) && (c != 100)) {
          if (c < 61)
            imgdata.other.CameraTemperature = (float)c;
          else
            imgdata.other.CameraTemperature = (float)(c - 32) / 1.8f;
          if ((OlyID == 0x4434353933ULL) && // TG-5
              (imgdata.other.exifAmbientTemperature > -273.15f))
            imgdata.other.CameraTemperature += imgdata.other.exifAmbientTemperature;
       }
  }

  return;
}

void CLASS parseOlympus_RawInfo (unsigned tag, unsigned type, unsigned len, unsigned dng_writer)
{
// uptag 0x3000

  int wb_ind, c, i;

  if ((tag == 0x0110) && strcmp(software, "v757-71")) {
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][0] = get2();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][2] = get2();
        if (len == 2) {
          for (i = 0; i < 256; i++)
            imgdata.color.WB_Coeffs[i][1] = imgdata.color.WB_Coeffs[i][3] = 0x100;
        }
  } else if ((((tag >= 0x0120) && (tag <= 0x0124)) ||
              ((tag >= 0x0130) && (tag <= 0x0133))) &&
              strcmp(software, "v757-71")) {
      if (tag <= 0x0124)
        wb_ind = tag - 0x0120;
      else
        wb_ind = tag - 0x0130 + 5;

      imgdata.color.WB_Coeffs[Oly_wb_list1[wb_ind]][0] = get2();
      imgdata.color.WB_Coeffs[Oly_wb_list1[wb_ind]][2] = get2();

  } else if ((tag == 0x0200) && (dng_writer == nonDNG)) {
      for (i = 0; i < 3; i++) {
        if (!imgdata.makernotes.olympus.ColorSpace) {
          FORC3 cmatrix[i][c] = ((short)get2()) / 256.0;
        } else {
          FORC3 imgdata.color.ccm[i][c] = ((short)get2()) / 256.0;
        }
      }

  } else if ((tag == 0x0600) && (dng_writer == nonDNG)) {
      FORC4 cblack[c ^ c >> 1] = get2();
  } else if ((tag == 0x0612) && (dng_writer == nonDNG)) {
    imgdata.sizes.raw_crop.cleft = get2();
  } else if ((tag == 0x0613) && (dng_writer == nonDNG)) {
    imgdata.sizes.raw_crop.ctop = get2();
  } else if ((tag == 0x0614) && (dng_writer == nonDNG)) {
    imgdata.sizes.raw_crop.cwidth = get2();
  } else if ((tag == 0x0615) && (dng_writer == nonDNG)) {
    imgdata.sizes.raw_crop.cheight = get2();
  }
  return;
}

void CLASS setLeicaBodyFeatures(int LeicaMakernoteSignature)
{
  if (LeicaMakernoteSignature == -3) { // M8
    imgdata.lens.makernotes.CameraFormat = LIBRAW_FORMAT_APSH;
    imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Leica_M;

  } else if (LeicaMakernoteSignature == -2) { // DMR
    imgdata.lens.makernotes.CameraFormat = LIBRAW_FORMAT_Leica_DMR;
    if ((model[0] == 'R') || (model[6] == 'R'))
      imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Leica_R;

  } else if (LeicaMakernoteSignature == 0) { // DIGILUX 2
    imgdata.lens.makernotes.CameraMount =
      imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_FixedLens;
    imgdata.lens.makernotes.FocalType = LIBRAW_FT_ZOOM;

  } else if ((LeicaMakernoteSignature == 0x0100) || // X1
             (LeicaMakernoteSignature == 0x0500) || // X2, X-E (Typ 102)
             (LeicaMakernoteSignature == 0x0700) || // X (Typ 113)
             (LeicaMakernoteSignature == 0x1000)) { // X-U (Typ 113)
    imgdata.lens.makernotes.CameraFormat =
      imgdata.lens.makernotes.LensFormat = LIBRAW_FORMAT_APSC;
    imgdata.lens.makernotes.CameraMount =
      imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_FixedLens;
    imgdata.lens.makernotes.FocalType = LIBRAW_FT_FIXED;

  } else if (LeicaMakernoteSignature == 0x0400) { // X VARIO (Typ 107)
    imgdata.lens.makernotes.CameraFormat =
      imgdata.lens.makernotes.LensFormat = LIBRAW_FORMAT_APSC;
    imgdata.lens.makernotes.CameraMount =
      imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_FixedLens;
    imgdata.lens.makernotes.FocalType = LIBRAW_FT_ZOOM;

  } else if ((LeicaMakernoteSignature == 0x0200) || // M10, M10-D, S (Typ 007)
             (LeicaMakernoteSignature == 0x02ff) || // M (Typ 240), M (Typ 262), M-D (Typ 262), M Monochrom (Typ 246), S (Typ 006), S-E (Typ 006), S2, S3
             (LeicaMakernoteSignature == 0x0300)) { // M9, M9 Monochrom, M Monochrom, M-E
    if ((model[0] == 'M') || (model[6] == 'M')) {
      imgdata.lens.makernotes.CameraFormat = LIBRAW_FORMAT_FF;
      imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Leica_M;
    } else if ((model[0] == 'S') || (model[6] == 'S')) {
      imgdata.lens.makernotes.CameraFormat = LIBRAW_FORMAT_MF;
      imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Leica_S;
    }

  } else if ((LeicaMakernoteSignature == 0x0600) || // T (Typ 701), TL
             (LeicaMakernoteSignature == 0x0900) || // SL (Typ 601), CL
             (LeicaMakernoteSignature == 0x1a00)) { // TL2
    imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Leica_L;
    if ((model[0] == 'S') || (model[6] == 'S')) {
      imgdata.lens.makernotes.CameraFormat = LIBRAW_FORMAT_FF;
    } else if ((model[0] == 'T') || (model[6] == 'T') ||
               (model[0] == 'C') || (model[6] == 'C')) {
      imgdata.lens.makernotes.CameraFormat = LIBRAW_FORMAT_APSC;
    }

  } else if (LeicaMakernoteSignature == 0x0800) { // Q (Typ 116)
    imgdata.lens.makernotes.CameraFormat =
      imgdata.lens.makernotes.LensFormat = LIBRAW_FORMAT_FF;
    imgdata.lens.makernotes.CameraMount =
      imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_FixedLens;
    imgdata.lens.makernotes.FocalType = LIBRAW_FT_FIXED;

  }
}

void CLASS parseLeicaLensID()
{
  imgdata.lens.makernotes.LensID = get4();
  if (imgdata.lens.makernotes.LensID) {
    imgdata.lens.makernotes.LensID =
      ((imgdata.lens.makernotes.LensID >> 2) << 8) |
       (imgdata.lens.makernotes.LensID & 0x3);
    if ((imgdata.lens.makernotes.LensID > 0x00ff) &&
        (imgdata.lens.makernotes.LensID < 0x3b00)) {
      imgdata.lens.makernotes.LensMount = imgdata.lens.makernotes.CameraMount;
      imgdata.lens.makernotes.LensFormat = LIBRAW_FORMAT_FF;
    }
  }
}

int CLASS parseLeicaLensName(unsigned len)
{
#define plln imgdata.lens.makernotes.Lens
  if (!len) {
    strcpy (plln, "N/A");
    return 0;
  }
  stmread(plln, len, ifp);
  if ((plln[0] == ' ') ||
      !strncasecmp(plln, "not ", 4) ||
      !strncmp(plln, "---", 3) ||
      !strncmp(plln, "***", 3)) {
    strcpy (plln, "N/A");
    return 0;
  } else return 1;
#undef plln
}

int CLASS parseLeicaInternalBodySerial(unsigned len)
{
#define plibs imgdata.shootinginfo.InternalBodySerial
  if (!len) {
    strcpy (plibs, "N/A");
    return 0;
  }
  stmread(plibs, len, ifp);
  if (!strncmp(plibs, "000000000000", 12)) {
    plibs[0] = '0';
    plibs[1] = '\0';
    return 1;
  }

  if (strnlen(plibs, len) == 13) {
    for (int i=3; i<13; i++) {
      if (!isdigit(plibs[i])) goto non_std;
    }
    memcpy (plibs+15, plibs+9, 4);
    memcpy (plibs+12, plibs+7, 2);
    memcpy (plibs+9, plibs+5, 2);
    memcpy (plibs+6, plibs+3, 2);
    plibs[3] = plibs[14] =' ';
    plibs[8] = plibs[11] = '/';
    if (((short)(plibs[3]-'0')*10 + (short)(plibs[4]-'0')) < 70) {
      memcpy (plibs+4, "20", 2);
    } else {
      memcpy (plibs+4, "19", 2);
    }
    return 2;
  }
non_std:
#undef plibs
  return 1;
}

void CLASS parseLeicaMakernote (int base, int uptag, unsigned MakernoteTagType)
{
#define ilm imgdata.lens.makernotes
  int c;
  uchar ci, cj;
  unsigned offset = 0, entries, tag, type, len, save;
  short morder, sorder = order;
  char buf[10];
  int LeicaMakernoteSignature = -1;
  INT64 fsize = ifp->size();
  fread(buf, 1, 10, ifp);
  if (strncmp (buf,"LEICA", 5)) {
    fseek(ifp, -10, SEEK_CUR);
    if (uptag == 0x3400) LeicaMakernoteSignature = 0x3400;
    else LeicaMakernoteSignature = -2; // DMR
  } else {
    fseek(ifp, -2, SEEK_CUR);
    LeicaMakernoteSignature = ((uchar)buf[6] << 8) | (uchar)buf[7];
    if (!LeicaMakernoteSignature &&
        (!strncmp(model, "M8", 2) || !strncmp(model+6, "M8", 2)))
      LeicaMakernoteSignature = -3;
      if ((LeicaMakernoteSignature != 0x0000) &&
          (LeicaMakernoteSignature != 0x0800) &&
          (LeicaMakernoteSignature != 0x0900) &&
          (LeicaMakernoteSignature != 0x02ff))
      base = ftell(ifp)-8;
  }
  setLeicaBodyFeatures(LeicaMakernoteSignature);

  entries = get2();
  if (entries > 1000) return;
  morder = order;

  while (entries--) {
    order = morder;
    tiff_get(base, &tag, &type, &len, &save);
    INT64 pos = ifp->tell();
    if (len > 8 && pos + len > 2 * fsize) {
      fseek(ifp, save, SEEK_SET); // Recover tiff-read position!!
      continue;
    }
    tag |= uptag << 16;
    if (len > 100 * 1024 * 1024)
      goto next; // 100Mb tag? No!

    if (LeicaMakernoteSignature == -3) { // M8
      if (tag == 0x0310) {
        parseLeicaLensID();
      } else if ((tag == 0x0313) && (fabs(ilm.CurAp) < 0.17f)) {
        ilm.CurAp = getreal(type);
        if (ilm.CurAp > 126.3) {
          ilm.CurAp = 0.0f;
        }
      } else if (tag == 0x0320) {
        imgdata.other.CameraTemperature = getreal(type);
      }

    } else if (LeicaMakernoteSignature == -2) { // DMR
      if (tag == 0x000d) {
        FORC3 cam_mul[c] = get2();
        cam_mul[3] = cam_mul[1];
      }

    } else if (LeicaMakernoteSignature == 0) { // DIGILUX 2
      if (tag == 0x0007) {
        imgdata.shootinginfo.FocusMode = get2();
      } else if (tag == 0x001a) {
        imgdata.shootinginfo.ImageStabilization = get2();
      }

    } else if ((LeicaMakernoteSignature == 0x0100) || // X1
               (LeicaMakernoteSignature == 0x0400) || // X VARIO
               (LeicaMakernoteSignature == 0x0500) || // X2, X-E (Typ 102)
               (LeicaMakernoteSignature == 0x0700) || // X (Typ 113)
               (LeicaMakernoteSignature == 0x1000)) { // X-U (Typ 113)
      if (tag == 0x040d) {
        ci = fgetc(ifp);
        cj = fgetc(ifp);
        imgdata.shootinginfo.ExposureMode = ((ushort)ci << 8) | cj;
      }

    } else if ((LeicaMakernoteSignature == 0x0600) || // TL, T (Typ 701)
               (LeicaMakernoteSignature == 0x1a00)) { // TL2
      if (tag == 0x040d) {
        ci = fgetc(ifp);
        cj = fgetc(ifp);
        imgdata.shootinginfo.ExposureMode = ((ushort)ci << 8) | cj;
      } else if (tag == 0x0303) {
        parseLeicaLensName(len);
      }

    } else if (LeicaMakernoteSignature == 0x0200) { // M10, M10-D, S (Typ 007)

    } else if (LeicaMakernoteSignature == 0x02ff) { // M (Typ 240), M (Typ 262), M-D (Typ 262), M Monochrom (Typ 246), S (Typ 006), S-E (Typ 006), S2, S3
      if (tag == 0x0303) {
        if (parseLeicaLensName(len)) {
          ilm.LensMount = ilm.CameraMount;
          ilm.LensFormat = ilm.CameraFormat;
        }
      }

    } else if (LeicaMakernoteSignature == 0x0300) { // M9, M9 Monochrom, M Monochrom, M-E
      if (tag == 0x3400) {
        parseLeicaMakernote(base, 0x3400, MakernoteTagType);
      }

    } else if ((LeicaMakernoteSignature == 0x0800) || // Q (Typ 116)
               (LeicaMakernoteSignature == 0x0900)) { // SL (Typ 601), CL
      if ((tag == 0x0304) &&
          (len == 1) &&
          ((c = fgetc(ifp)) != 0) &&
          (imgdata.lens.makernotes.CameraMount == LIBRAW_MOUNT_Leica_L)) {
        strcpy(imgdata.lens.makernotes.Adapter, "M-Adapter L");
        imgdata.lens.makernotes.LensMount =  LIBRAW_MOUNT_Leica_M;
        imgdata.lens.makernotes.LensFormat = LIBRAW_FORMAT_FF;
        imgdata.lens.makernotes.LensID = c*256;
      } else if (tag == 0x0500) {
        parseLeicaInternalBodySerial(len);
      }

    } else if (LeicaMakernoteSignature == 0x3400) { // tag 0x3400 in M9, M9 Monochrom, M Monochrom
      if (tag == 0x34003402) {
        imgdata.other.CameraTemperature = getreal(type);
      } else if (tag == 0x34003405) {
        parseLeicaLensID();
      } else if ((tag == 0x34003406) && (fabs(ilm.CurAp) < 0.17f)) {
        ilm.CurAp = getreal(type);
        if (ilm.CurAp > 126.3) {
          ilm.CurAp = 0.0f;
        }
      }
    }

next:
    fseek(ifp, save, SEEK_SET);
  }
quit:
  order = sorder;
#undef ilm
}

void CLASS parseAdobeRAFMakernote() {

  uchar *PrivateMknBuf;
  unsigned posPrivateMknBuf;
  unsigned PrivateMknLength;
  unsigned PrivateOrder;
  unsigned PrivateEntries, PrivateTagID;
  unsigned PrivateTagBytes;
  unsigned wb_section_offset = 0;
  int posWB;
  int c;

#define CHECKSPACE(s) if(posPrivateMknBuf+(s) > PrivateMknLength) { free (PrivateMknBuf); return; }
#define isWB(posWB) sget2(posWB) != 0 && sget2(posWB+2) != 0 && sget2(posWB+4) != 0 && sget2(posWB+6) != 0 && sget2(posWB+8) != 0 && sget2(posWB+10) != 0 && sget2(posWB) != 0xff && sget2(posWB+2) != 0xff && sget2(posWB+4) != 0xff && sget2(posWB+6) != 0xff && sget2(posWB+8) != 0xff && sget2(posWB+10) != 0xff && sget2(posWB) == sget2(posWB+6) && sget2(posWB) < sget2(posWB+2) && sget2(posWB) < sget2(posWB+4) && sget2(posWB) < sget2(posWB+8) && sget2(posWB) < sget2(posWB+10)
#define icWBC imgdata.color.WB_Coeffs
#define imfRAFDataVersion imgdata.makernotes.fuji.RAFDataVersion
#define imfRAFVersion imgdata.makernotes.fuji.RAFVersion

  order = 0x4d4d;
  PrivateMknLength = get4();

  if ((PrivateMknLength > 4) &&
      (PrivateMknLength < 10240000) &&
      (PrivateMknBuf = (uchar *)malloc(PrivateMknLength+1024))) { // 1024b for safety
    fread (PrivateMknBuf, PrivateMknLength, 1, ifp);
    PrivateOrder = sget2(PrivateMknBuf);
    posPrivateMknBuf = sget4(PrivateMknBuf+2) + 12;

    memcpy(imgdata.makernotes.fuji.SerialSignature, PrivateMknBuf + 6, 0x0c);
    imgdata.makernotes.fuji.SerialSignature[0x0c] = 0;
    memcpy (model, PrivateMknBuf + 0x12, 0x20);
    model[0x20] = 0;
    memcpy (model2, PrivateMknBuf + 0x32, 4);
    model2[4] = 0;
    strcpy (imgdata.makernotes.fuji.RAFVersion, model2);
    PrivateEntries = sget2(PrivateMknBuf+posPrivateMknBuf);
    if ((PrivateEntries > 1000) ||
        ((PrivateOrder != 0x4d4d) && (PrivateOrder != 0x4949))) {
      free (PrivateMknBuf);
      return;
    }
    posPrivateMknBuf += 2;
/*
 * because Adobe DNG converter strips or misplaces 0xfnnn tags,
 * Auto WB for following cameras is missing for now
 * - F550EXR
 * - F600EXR
 * - F770EXR
 * - F800EXR
 * - F900EXR
 * - HS10
 * - HS11
 * - HS20EXR
 * - HS30EXR
 * - HS50EXR
 * - S1
 * - SL1000
**/
    while (PrivateEntries--) {
      order = 0x4d4d;
      CHECKSPACE(4);
      PrivateTagID = sget2(PrivateMknBuf+posPrivateMknBuf);
      PrivateTagBytes = sget2(PrivateMknBuf+posPrivateMknBuf+2);
      posPrivateMknBuf += 4;
      order = PrivateOrder;

      if (PrivateTagID == 0x2000) {
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][c ^ 1] = sget2(PrivateMknBuf+posPrivateMknBuf+(c<<1));
      } else if (PrivateTagID == 0x2100) {
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_FineWeather][c ^ 1] = sget2(PrivateMknBuf+posPrivateMknBuf+(c<<1));
      } else if (PrivateTagID == 0x2200) {
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Shade][c ^ 1] = sget2(PrivateMknBuf+posPrivateMknBuf+(c<<1));
      } else if (PrivateTagID == 0x2300) {
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_D][c ^ 1] = sget2(PrivateMknBuf+posPrivateMknBuf+(c<<1));
      } else if (PrivateTagID == 0x2301) {
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_N][c ^ 1] = sget2(PrivateMknBuf+posPrivateMknBuf+(c<<1));
      } else if (PrivateTagID == 0x2302) {
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_WW][c ^ 1] = sget2(PrivateMknBuf+posPrivateMknBuf+(c<<1));
      } else if (PrivateTagID == 0x2310) {
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_L][c ^ 1] = sget2(PrivateMknBuf+posPrivateMknBuf+(c<<1));
      } else if (PrivateTagID == 0x2400) {
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Tungsten][c ^ 1] = sget2(PrivateMknBuf+posPrivateMknBuf+(c<<1));
      } else if (PrivateTagID == 0x2410) {
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][c ^ 1] = sget2(PrivateMknBuf+posPrivateMknBuf+(c<<1));
      } else if (PrivateTagID == 0x2f00) {
        int nWBs = MIN(sget4(PrivateMknBuf+posPrivateMknBuf), 6);
        posWB = posPrivateMknBuf+4;
        for (int wb_ind = 0; wb_ind < nWBs; wb_ind++) {
          FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Custom1 + wb_ind][c ^ 1] = sget2(PrivateMknBuf+posWB+(c<<1));
          posWB += 8;
        }
      } else if (PrivateTagID == 0x2ff0) {
        FORC4 cam_mul[c ^ 1] = sget2(PrivateMknBuf+posPrivateMknBuf+(c<<1));
      }

      else if (PrivateTagID == 0x9650) {
        CHECKSPACE(4);
        short a = (short)sget2(PrivateMknBuf+posPrivateMknBuf);
        float b = fMAX(1.0f, sget2(PrivateMknBuf+posPrivateMknBuf+2));
        imgdata.makernotes.fuji.FujiExpoMidPointShift = a / b;

      } else if ((PrivateTagID == 0xc000) &&
                 (PrivateTagBytes > 3)    &&
                 (PrivateTagBytes < 10240000)) {
        order = 0x4949;
        if (PrivateTagBytes != 4096) { // not Fuji X-A3, X-A5, X-A10, X-A20, X-T100, XF10
          if (!sget2(PrivateMknBuf + posPrivateMknBuf))
            imfRAFDataVersion = sget2(PrivateMknBuf + posPrivateMknBuf + 2);

          for (posWB = 0; posWB < PrivateTagBytes-16; posWB++) {
            if ((!memcmp (PrivateMknBuf+posWB, "TSNERDTS", 8) &&
                (sget2(PrivateMknBuf+posWB+10) > 125))) {
              posWB +=10;
              imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][1] =
                imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][3] = sget2(PrivateMknBuf+posWB);
              imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][0] = sget2(PrivateMknBuf+posWB+2);
              imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][2] = sget2(PrivateMknBuf+posWB+4);
              break;
            }
          }

          if (imfRAFDataVersion == 0x0146  ||        // X20
              imfRAFDataVersion == 0x0149  ||        // X100S
              imfRAFDataVersion == 0x0249) {         // X100S
            wb_section_offset = 0x1410;
          } else if (imfRAFDataVersion == 0x014d  || // X-M1
                     imfRAFDataVersion == 0x014e) {  // X-A1, X-A2
            wb_section_offset = 0x1474;
          } else if (imfRAFDataVersion == 0x014f  || // X-E2
                     imfRAFDataVersion == 0x024f  || // X-E2
                     imfRAFDataVersion == 0x025d) {  // X-H1
            wb_section_offset = 0x1480;
          } else if (imfRAFDataVersion == 0x0150) {  // XQ1, XQ2
            wb_section_offset = 0x1414;
          } else if (imfRAFDataVersion == 0x0151  || // X-T1 w/diff. fws
                     imfRAFDataVersion == 0x0251  ||
                     imfRAFDataVersion == 0x0351  ||
                     imfRAFDataVersion == 0x0451) {
            wb_section_offset = 0x14b0;
          } else if (imfRAFDataVersion == 0x0152  || // X30
                     imfRAFDataVersion == 0x0153) {  // X100T
            wb_section_offset = 0x1444;
          } else if (imfRAFDataVersion == 0x0154) {  // X-T10
            wb_section_offset = 0x1824;
          } else if (imfRAFDataVersion == 0x0155) {  // X70
            wb_section_offset = 0x17b4;
          } else if (imfRAFDataVersion == 0x0255) {  // X-Pro2
            wb_section_offset = 0x135c;
          } else if (imfRAFDataVersion == 0x0258 ||  // X-T2
                     imfRAFDataVersion == 0x025b) {  // X-T20
            wb_section_offset = 0x13dc;
          } else if (imfRAFDataVersion == 0x0259) {  // X100F
            wb_section_offset = 0x1370;
          } else if (imfRAFDataVersion == 0x025a  || // GFX 50S
                     imfRAFDataVersion == 0x025f) {  // GFX 50R
            wb_section_offset = 0x1424;
          } else if (imfRAFDataVersion == 0x025c) {  // X-E3
            wb_section_offset = 0x141c;
          } else if (imfRAFDataVersion == 0x025e) {  // X-T3
            wb_section_offset = 0x2014;
          } else if (imfRAFDataVersion == 0x025f) {  // X-T30
            wb_section_offset = 0x20b8;
          } else if (imfRAFDataVersion == 0x0355) {  // X-E2S
            wb_section_offset = 0x1840;

          /* try for unknown RAF Data versions */
          } else if (!strcmp(model, "X20"))     {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x1410))
              wb_section_offset = 0x1410;
          } else if (!strcmp(model, "X100S"))   {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x1410))
              wb_section_offset = 0x1410;
          } else if (!strcmp(model, "X-M1"))    {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x1474))
              wb_section_offset = 0x1474;
          } else if (!strcmp(model, "X-A1"))    {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x1474))
              wb_section_offset = 0x1474;
          } else if (!strcmp(model, "X-A2"))    {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x1474))
              wb_section_offset = 0x1474;
          } else if (!strcmp(model, "X-E2"))    {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x1480))
              wb_section_offset = 0x1480;
          } else if (!strcmp(model, "X-H1"))    {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x1480))
              wb_section_offset = 0x1480;
          } else if (!strcmp(model, "XQ1"))     {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x1414))
              wb_section_offset = 0x1414;
          } else if (!strcmp(model, "XQ2"))     {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x1414))
              wb_section_offset = 0x1414;
          } else if (!strcmp(model, "X-T1"))    {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x14b0))
              wb_section_offset = 0x14b0;
          } else if (!strcmp(model, "X30"))     {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x1444))
              wb_section_offset = 0x1444;
          } else if (!strcmp(model, "X100T"))   {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x1444))
              wb_section_offset = 0x1444;
          } else if (!strcmp(model, "X-T10"))   {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x1824))
              wb_section_offset = 0x1824;
          } else if (!strcmp(model, "X70"))     {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x17b4))
              wb_section_offset = 0x17b4;
          } else if (!strcmp(model, "X-Pro2"))  {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x135c))
              wb_section_offset = 0x135c;
          } else if (!strcmp(model, "X-T2"))    {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x13dc))
              wb_section_offset = 0x13dc;
          } else if (!strcmp(model, "X-T20"))   {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x13dc))
              wb_section_offset = 0x13dc;
          } else if (!strcmp(model, "X100F"))   {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x1370))
              wb_section_offset = 0x1370;
          } else if (!strcmp(model, "GFX 50S")) {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x1424))
              wb_section_offset = 0x1424;
          } else if (!strcmp(model, "GFX 50R")) {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x1424))
              wb_section_offset = 0x1424;
          } else if (!strcmp(model, "X-T3"))    {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x2014))
              wb_section_offset = 0x2014;
          } else if (!strcmp(model, "X-T30"))    {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x20b8))
              wb_section_offset = 0x2014;
          } else if (!strcmp(model, "X-E3"))    {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x141c))
              wb_section_offset = 0x141c;
          } else if (!strcmp(model, "X-E2S"))   {
            if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x1840))
              wb_section_offset = 0x1840;

          /* no RAF Data version for the models below */
          } else if (!strcmp(model, "FinePix X100")) { // X100 0 0x19f0 0x19e8
            if (!strcmp(imfRAFVersion,"0069")) wb_section_offset = 0x19e8;
            else if (!strcmp(imfRAFVersion, "0100")) wb_section_offset = 0x19f0;
            else if (!strcmp(imfRAFVersion, "0110")) wb_section_offset = 0x19f0;
            else if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x19e8)) wb_section_offset = 0x19e8;
            else if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x19f0)) wb_section_offset = 0x19f0;
          } else if (!strcmp(model, "X-E1"))   { // X-E1 0 0x13ac
            if (!strcmp(imfRAFVersion, "0101")) wb_section_offset = 0x13ac;
            else if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x13ac)) wb_section_offset = 0x13ac;
          } else if (!strcmp(model, "X-Pro1")) { // X-Pro1 0 0x13a4
            if (!strcmp(imfRAFVersion,"0100")) wb_section_offset = 0x13a4;
            else if (!strcmp(imfRAFVersion, "0101")) wb_section_offset = 0x13a4;
            else if (!strcmp(imfRAFVersion, "0204")) wb_section_offset = 0x13a4;
            else if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x13a4)) wb_section_offset = 0x13a4;
          } else if (!strcmp(model, "XF1"))    { // XF1 0 0x138c
            if (!strcmp(imfRAFVersion,"0100")) wb_section_offset = 0x138c;
            else if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x138c)) wb_section_offset = 0x138c;
          } else if (!strcmp(model, "X-S1"))   { // X-S1 0 0x1284
            if (!strcmp(imfRAFVersion,"0100")) wb_section_offset = 0x1284;
            else if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x1284)) wb_section_offset = 0x1284;
          } else if (!strcmp(model, "X10"))    { // X10 0 0x1280 0x12d4
            if (!strcmp(imfRAFVersion,"0100")) wb_section_offset = 0x1280;
            else if (!strcmp(imfRAFVersion, "0102")) wb_section_offset = 0x1280;
            else if (!strcmp(imfRAFVersion, "0103")) wb_section_offset = 0x12d4;
            else if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x1280)) wb_section_offset = 0x1280;
            else if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x12d4)) wb_section_offset = 0x12d4;
          } else if (!strcmp(model, "XF1"))    { // XF1 0 0x138c
            if (!strcmp(imfRAFVersion,"0100")) wb_section_offset = 0x138c;
            else if (isWB(PrivateMknBuf + posPrivateMknBuf + 0x138c)) wb_section_offset = 0x138c;
          }
          if (wb_section_offset &&
              isWB(PrivateMknBuf + posPrivateMknBuf + wb_section_offset)) {

            if (!imfRAFDataVersion) {
              posWB = posPrivateMknBuf+wb_section_offset-6;
              imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][1] =
                imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][3] = sget2(PrivateMknBuf+posWB);
              imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][0] = sget2(PrivateMknBuf+posWB+2);
              imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][2] = sget2(PrivateMknBuf+posWB+4);
            }

            posWB = posPrivateMknBuf + wb_section_offset;
            for (int wb_ind=0; wb_ind < nFuji_wb_list1; posWB+=6, wb_ind++) {
              imgdata.color.WB_Coeffs[Fuji_wb_list1[wb_ind]][1] =
                imgdata.color.WB_Coeffs[Fuji_wb_list1[wb_ind]][3] = sget2(PrivateMknBuf + posWB);
              imgdata.color.WB_Coeffs[Fuji_wb_list1[wb_ind]][0]   = sget2(PrivateMknBuf + posWB+2);
              imgdata.color.WB_Coeffs[Fuji_wb_list1[wb_ind]][2]   = sget2(PrivateMknBuf + posWB+4);
            }

            int found = 0;
            posWB += 0xC0;
            ushort Gval = sget2(PrivateMknBuf + posWB);
            for (int posEndCCTsection = posWB; posEndCCTsection < (posWB + 30); posEndCCTsection += 6) {
              if (sget2(PrivateMknBuf + posEndCCTsection) != Gval) {
                wb_section_offset = posEndCCTsection - 186;
                found = 1;
                break;
              }
            }

            if (found) {
              for (int iCCT = 0; iCCT < 31; iCCT++) {
                imgdata.color.WBCT_Coeffs[iCCT][0]   = FujiCCT_K[iCCT];
                imgdata.color.WBCT_Coeffs[iCCT][1]   = sget2(PrivateMknBuf+wb_section_offset+iCCT*6+2);
                imgdata.color.WBCT_Coeffs[iCCT][2] =
                  imgdata.color.WBCT_Coeffs[iCCT][4] = sget2(PrivateMknBuf+wb_section_offset+iCCT*6);
                imgdata.color.WBCT_Coeffs[iCCT][3]   = sget2(PrivateMknBuf+wb_section_offset+iCCT*6+4);
              }
            }
          }
        } else { // process 4K raf data
          int wb[4];
          int nWB, tWB, pWB;
          int iCCT = 0;
          is_4K_RAFdata = 1; /* X-A3, X-A5, X-A10, X-A20, X-T100, XF10 */
          posWB = posPrivateMknBuf + 0x200;
          for (int wb_ind = 0; wb_ind < 42; wb_ind++) {
            nWB = sget4(PrivateMknBuf+posWB); posWB += 4;
            tWB = sget4(PrivateMknBuf+posWB); posWB += 4;
            wb[0] = sget4(PrivateMknBuf+posWB) << 1; posWB += 4;
            wb[1] = sget4(PrivateMknBuf+posWB); posWB += 4;
            wb[3] = sget4(PrivateMknBuf+posWB); posWB += 4;
            wb[2] = sget4(PrivateMknBuf+posWB) << 1; posWB += 4;

            if (tWB && (iCCT < 255)) {
              imgdata.color.WBCT_Coeffs[iCCT][0] = tWB;
              FORC4 imgdata.color.WBCT_Coeffs[iCCT][c + 1] = wb[c];
              iCCT++;
            }
            if (nWB != 0x46) {
              for (pWB = 1; pWB < nFuji_wb_list2; pWB += 2) {
                if (Fuji_wb_list2[pWB] == nWB) {
                  FORC4 imgdata.color.WB_Coeffs[Fuji_wb_list2[pWB - 1]][c] = wb[c];
                  break;
                }
              }
            }
          }
        }
      }
      posPrivateMknBuf += PrivateTagBytes;
    }
    free (PrivateMknBuf);
  }
#undef imfRAFVersion
#undef imfRAFDataVersion
#undef icWBC
#undef CHECKSPACE
}

void CLASS parseAdobePanoMakernote ()
{
  uchar *PrivateMknBuf;
  unsigned posPrivateMknBuf;
  unsigned PrivateMknLength;
  unsigned PrivateOrder;
  unsigned PrivateEntries, PrivateTagID, PrivateTagType, PrivateTagCount;
  unsigned PrivateTagBytes;
  int truncated;

#define CHECKSPACE(s) if(posPrivateMknBuf+(s) > PrivateMknLength) { free (PrivateMknBuf); return; }
#define icWBC imgdata.color.WB_Coeffs

  order = 0x4d4d;
  truncated = 0;
  PrivateMknLength = get4();

  if ((PrivateMknLength > 4) &&
      (PrivateMknLength < 10240000) &&
      (PrivateMknBuf = (uchar *)malloc(PrivateMknLength+1024))) { // 1024b for safety
    fread (PrivateMknBuf, PrivateMknLength, 1, ifp);
    PrivateOrder = sget2(PrivateMknBuf);
    PrivateEntries = sget2(PrivateMknBuf+2);
    if ((PrivateEntries > 1000) ||
        ((PrivateOrder != 0x4d4d) && (PrivateOrder != 0x4949))) {
      free (PrivateMknBuf);
      return;
    }
    posPrivateMknBuf = 4;
    while (PrivateEntries--) {
      order = 0x4d4d;
      CHECKSPACE(8);
      PrivateTagID = sget2(PrivateMknBuf+posPrivateMknBuf);
      PrivateTagType = sget2(PrivateMknBuf+posPrivateMknBuf+2);
      PrivateTagCount = sget4(PrivateMknBuf+posPrivateMknBuf+4);
      posPrivateMknBuf += 8;
      order = PrivateOrder;

      if (truncated && !PrivateTagCount) continue;

      PrivateTagBytes =
        PrivateTagCount * ("11124811248484"[PrivateTagType < 14 ? PrivateTagType : 0] - '0');

      if (PrivateTagID == 0x0002) {
        posPrivateMknBuf += 2;
        CHECKSPACE(2);
        if (sget2(PrivateMknBuf+posPrivateMknBuf)) {
          truncated = 1;
        } else {
          posPrivateMknBuf += 2;
        }

      } else if (PrivateTagID == 0x0013) {
        ushort nWB, cnt, tWB;
        CHECKSPACE(2);
        nWB = sget2(PrivateMknBuf+posPrivateMknBuf);
        posPrivateMknBuf += 2;
        if (nWB > 0x100)
          break;
        for (cnt = 0; cnt < nWB; cnt++) {
          CHECKSPACE(2);
          tWB = sget2(PrivateMknBuf+posPrivateMknBuf);
          if (tWB < 0x100) {
            CHECKSPACE(4);
            icWBC[tWB][0] = sget2(PrivateMknBuf+posPrivateMknBuf+2);
            icWBC[tWB][2] = sget2(PrivateMknBuf+posPrivateMknBuf+4);
            icWBC[tWB][1] = icWBC[tWB][3] = 0x100;
          }
          posPrivateMknBuf += 6;
        }

      } else if (PrivateTagID == 0x0027) {
        ushort nWB, cnt, tWB;
        CHECKSPACE(2);
        nWB = sget2(PrivateMknBuf+posPrivateMknBuf);
        posPrivateMknBuf += 2;
        if (nWB > 0x100)
          break;
        for (cnt = 0; cnt < nWB; cnt++) {
          CHECKSPACE(2);
          tWB = sget2(PrivateMknBuf+posPrivateMknBuf);
          if (tWB < 0x100) {
            CHECKSPACE(6);
            icWBC[tWB][0] = sget2(PrivateMknBuf+posPrivateMknBuf+2);
            icWBC[tWB][1] = icWBC[tWB][3] = sget2(PrivateMknBuf+posPrivateMknBuf+4);
            icWBC[tWB][2] = sget2(PrivateMknBuf+posPrivateMknBuf+6);
          }
          posPrivateMknBuf += 8;
        }

      } else if (PrivateTagID == 0x0121) {
        CHECKSPACE(4);
        imgdata.makernotes.panasonic.Multishot = sget4(PrivateMknBuf+posPrivateMknBuf);
        posPrivateMknBuf += 4;

      } else {
        if (PrivateTagBytes > 4) posPrivateMknBuf += PrivateTagBytes;
        else if (!truncated) posPrivateMknBuf += 4;
        else {
          if (PrivateTagBytes <= 2) posPrivateMknBuf += 2;
          else posPrivateMknBuf += 4;
        }
      }
    }
    free (PrivateMknBuf);
  }
#undef icWBC
#undef CHECKSPACE
}

void CLASS parseCanonMakernotes(unsigned tag, unsigned type, unsigned len)
{

  if (tag == 0x0001) {
    Canon_CameraSettings(len);
  } else if (tag == 0x0002) { // focal length
    imgdata.lens.makernotes.FocalType = get2();
    imgdata.lens.makernotes.CurFocal = get2();
    if (imgdata.lens.makernotes.CanonFocalUnits > 1) {
      imgdata.lens.makernotes.CurFocal /= (float)imgdata.lens.makernotes.CanonFocalUnits;
    }
  } else if (tag == 0x0004) { // shot info
    short tempAp;

    fseek(ifp, 24, SEEK_CUR);
    tempAp = get2();
    if (tempAp != 0)
      imgdata.other.CameraTemperature = (float)(tempAp - 128);
    tempAp = get2();
    if (tempAp != -1)
      imgdata.other.FlashGN = ((float)tempAp) / 32;
    get2();

    // fseek(ifp, 30, SEEK_CUR);
    imgdata.other.FlashEC = _CanonConvertEV((signed short)get2());
    fseek(ifp, 8 - 32, SEEK_CUR);
    if ((tempAp = get2()) != 0x7fff)
      imgdata.lens.makernotes.CurAp = _CanonConvertAperture(tempAp);
    if (imgdata.lens.makernotes.CurAp < 0.7f) {
      fseek(ifp, 32, SEEK_CUR);
      imgdata.lens.makernotes.CurAp = _CanonConvertAperture(get2());
    }
    if (!aperture)
      aperture = imgdata.lens.makernotes.CurAp;
  } else if (tag == 0x000c) {
    unsigned tS = get4();
    sprintf (imgdata.shootinginfo.BodySerial, "%d", tS);
  } else if (tag == 0x0095 && // lens model tag
           !imgdata.lens.makernotes.Lens[0]) {
    fread(imgdata.lens.makernotes.Lens, 2, 1, ifp);
    imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_Canon_EF;
    if (imgdata.lens.makernotes.Lens[0] < 65) // non-Canon lens
      fread(imgdata.lens.makernotes.Lens + 2, 62, 1, ifp);
    else {
      char efs[2];
      imgdata.lens.makernotes.LensFeatures_pre[0] = imgdata.lens.makernotes.Lens[0];
      imgdata.lens.makernotes.LensFeatures_pre[1] = imgdata.lens.makernotes.Lens[1];
      fread(efs, 2, 1, ifp);
      if (efs[0] == 45 && (efs[1] == 83 || efs[1] == 69 || efs[1] == 77)) { // "EF-S, TS-E, MP-E, EF-M" lenses
        imgdata.lens.makernotes.Lens[2] = imgdata.lens.makernotes.LensFeatures_pre[2] = efs[0];
        imgdata.lens.makernotes.Lens[3] = imgdata.lens.makernotes.LensFeatures_pre[3] = efs[1];
        imgdata.lens.makernotes.Lens[4] = 32;
        if (efs[1] == 83) {
          imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_Canon_EF_S;
          imgdata.lens.makernotes.LensFormat = LIBRAW_FORMAT_APSC;
        } else if (efs[1] == 77) {
          imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_Canon_EF_M;
        }
      } else { // "EF" lenses
        imgdata.lens.makernotes.Lens[2] = 32;
        imgdata.lens.makernotes.Lens[3] = efs[0];
        imgdata.lens.makernotes.Lens[4] = efs[1];
      }
      fread(imgdata.lens.makernotes.Lens + 5, 58, 1, ifp);
    }
  } else if (tag == 0x009a) {
    get4();
    imgdata.sizes.raw_crop.cwidth = get4();
    imgdata.sizes.raw_crop.cheight = get4();
    imgdata.sizes.raw_crop.cleft = get4();
    imgdata.sizes.raw_crop.ctop = get4();
  } else if (tag == 0x00a9) {
    long int save1 = ftell(ifp);
    int c;
    fseek(ifp, (0x1 << 1), SEEK_CUR);
    FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][c ^ (c >> 1)] = get2();
    Canon_WBpresets(0, 0);
    fseek(ifp, save1, SEEK_SET);
  } else if (tag == 0x00e0) { // sensor info
    imgdata.makernotes.canon.SensorWidth = (get2(), get2());
    imgdata.makernotes.canon.SensorHeight = get2();
    imgdata.makernotes.canon.SensorLeftBorder = (get2(), get2(), get2());
    imgdata.makernotes.canon.SensorTopBorder = get2();
    imgdata.makernotes.canon.SensorRightBorder = get2();
    imgdata.makernotes.canon.SensorBottomBorder = get2();
    imgdata.makernotes.canon.BlackMaskLeftBorder = get2();
    imgdata.makernotes.canon.BlackMaskTopBorder = get2();
    imgdata.makernotes.canon.BlackMaskRightBorder = get2();
    imgdata.makernotes.canon.BlackMaskBottomBorder = get2();
  } else if (tag == 0x4013) {
    get4();
    imgdata.makernotes.canon.AFMicroAdjMode = get4();
    float a = get4(); float b = get4();
    if(fabsf(b)>0.001f) imgdata.makernotes.canon.AFMicroAdjValue = a/b;
  } else if (tag == 0x4001 && len > 500) {
    int c;
    int bls = 0;
    long int offsetChannelBlackLevel = 0L;
    long int offsetWhiteLevels = 0L;
    long int save1 = ftell(ifp);

    switch (len) {

    case 582:
      imgdata.makernotes.canon.CanonColorDataVer = 1; // 20D / 350D

      fseek(ifp, save1 + (0x0019 << 1), SEEK_SET);
      FORC4 cam_mul[c ^ (c >> 1)] = (float)get2();
      fseek(ifp, save1 + (0x001e << 1), SEEK_SET);
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][c ^ (c >> 1)] = get2();
      fseek(ifp, save1 + (0x0041 << 1), SEEK_SET);
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Custom1][c ^ (c >> 1)] = get2();
      fseek(ifp, save1 + (0x0046 << 1), SEEK_SET);
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Custom2][c ^ (c >> 1)] = get2();

      fseek(ifp, save1 + (0x0023 << 1), SEEK_SET);
      Canon_WBpresets(2, 2);
      fseek(ifp, save1 + (0x004b << 1), SEEK_SET);
      Canon_WBCTpresets(1); // ABCT
      offsetChannelBlackLevel = save1 + (0x00a6 << 1);
      break;

    case 653:
      imgdata.makernotes.canon.CanonColorDataVer = 2; // 1Dmk2 / 1DsMK2

      fseek(ifp, save1 + (0x0018 << 1), SEEK_SET);
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][c ^ (c >> 1)] = get2();
      fseek(ifp, save1 + (0x0022 << 1), SEEK_SET);
      FORC4 cam_mul[c ^ (c >> 1)] = (float)get2();
      fseek(ifp, save1 + (0x0090 << 1), SEEK_SET);
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Custom1][c ^ (c >> 1)] = get2();
      fseek(ifp, save1 + (0x0095 << 1), SEEK_SET);
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Custom2][c ^ (c >> 1)] = get2();
      fseek(ifp, save1 + (0x009a << 1), SEEK_SET);
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Custom3][c ^ (c >> 1)] = get2();

      fseek(ifp, save1 + (0x0027 << 1), SEEK_SET);
      Canon_WBpresets(2, 12);
      fseek(ifp, save1 + (0x00a4 << 1), SEEK_SET);
      Canon_WBCTpresets(1); // ABCT
      offsetChannelBlackLevel = save1 + (0x011e << 1);
      break;

    case 796:
      imgdata.makernotes.canon.CanonColorDataVer = 3; // 1DmkIIN / 5D / 30D / 400D
      imgdata.makernotes.canon.CanonColorDataSubVer = get2();

      fseek(ifp, save1 + (0x003f << 1), SEEK_SET);
      FORC4 cam_mul[c ^ (c >> 1)] = (float)get2();
      fseek(ifp, save1 + (0x0044 << 1), SEEK_SET);
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][c ^ (c >> 1)] = get2();
      fseek(ifp, save1 + (0x0049 << 1), SEEK_SET);
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Measured][c ^ (c >> 1)] = get2();
      fseek(ifp, save1 + (0x0071 << 1), SEEK_SET);
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Custom1][c ^ (c >> 1)] = get2();
      fseek(ifp, save1 + (0x0076 << 1), SEEK_SET);
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Custom2][c ^ (c >> 1)] = get2();
      fseek(ifp, save1 + (0x007b << 1), SEEK_SET);
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Custom3][c ^ (c >> 1)] = get2();
      fseek(ifp, save1 + (0x0080 << 1), SEEK_SET);
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Custom][c ^ (c >> 1)] = get2();

      fseek(ifp, save1 + (0x004e << 1), SEEK_SET);
      Canon_WBpresets(2, 12);
      fseek(ifp, save1 + (0x0085 << 1), SEEK_SET);
      Canon_WBCTpresets(0);                       // BCAT
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
      imgdata.makernotes.canon.CanonColorDataVer = 4;
      imgdata.makernotes.canon.CanonColorDataSubVer = get2();

      fseek(ifp, save1 + (0x003f << 1), SEEK_SET);
      FORC4 cam_mul[c ^ (c >> 1)] = (float)get2();
      fseek(ifp, save1 + (0x0044 << 1), SEEK_SET);
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][c ^ (c >> 1)] = get2();
      fseek(ifp, save1 + (0x0049 << 1), SEEK_SET);
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Measured][c ^ (c >> 1)] = get2();

      fseek(ifp, save1 + (0x004e << 1), SEEK_SET);
      FORC4 sraw_mul[c ^ (c >> 1)] = get2();
      fseek(ifp, save1 + (0x0053 << 1), SEEK_SET);
      Canon_WBpresets(2, 12);
      fseek(ifp, save1 + (0x00a8 << 1), SEEK_SET);
      Canon_WBCTpresets(0);                       // BCAT

      if ((imgdata.makernotes.canon.CanonColorDataSubVer == 4) ||
          (imgdata.makernotes.canon.CanonColorDataSubVer == 5)) {
        offsetChannelBlackLevel = save1 + (0x02b4 << 1);
        offsetWhiteLevels = save1 + (0x02b8 << 1);
      } else if ((imgdata.makernotes.canon.CanonColorDataSubVer == 6) ||
                 (imgdata.makernotes.canon.CanonColorDataSubVer == 7)) {
        offsetChannelBlackLevel = save1 + (0x02cb << 1);
        offsetWhiteLevels = save1 + (0x02cf << 1);
      } else if (imgdata.makernotes.canon.CanonColorDataSubVer == 9) {
        offsetChannelBlackLevel = save1 + (0x02cf << 1);
        offsetWhiteLevels = save1 + (0x02d3 << 1);
      }
      else offsetChannelBlackLevel = save1 + (0x00e7 << 1);
      break;

    case 5120:
      imgdata.makernotes.canon.CanonColorDataVer = 5; // PowerSot G10, G12, G5 X, G7 X, G9 X, EOS M3, EOS M5, EOS M6
      imgdata.makernotes.canon.CanonColorDataSubVer = get2();

      fseek(ifp, save1 + (0x0047 << 1), SEEK_SET);
      FORC4 cam_mul[c ^ (c >> 1)] = (float)get2();

      if (imgdata.makernotes.canon.CanonColorDataSubVer == 0xfffc) { // -4: G7 X Mark II, G9 X Mark II, G1 X Mark III, M5, M100, M6
        fseek(ifp, save1 + (0x004f << 1), SEEK_SET);
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][c ^ (c >> 1)] = get2();
        fseek(ifp, 8, SEEK_CUR);
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Measured][c ^ (c >> 1)] = get2();
        fseek(ifp, 8, SEEK_CUR);
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Other][c ^ (c >> 1)] = get2();
        fseek(ifp, 8, SEEK_CUR);
        Canon_WBpresets(8, 24);
        fseek(ifp, 168, SEEK_CUR);
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_WW][c ^ (c >> 1)] = get2();
        fseek(ifp, 24, SEEK_CUR);
        Canon_WBCTpresets(2); // BCADT
        offsetChannelBlackLevel = save1 + (0x014d << 1);
      } else if (imgdata.makernotes.canon.CanonColorDataSubVer == 0xfffd) { // -3: M10/M3/G1 X/G1 X II/G10/G11/G12/G15/G16/G3 X/G5 X/G7 X/G9 X/S100/S110/S120/S90/S95/SX1 IX/SX50 HS/SX60 HS
        fseek(ifp, save1 + (0x004c << 1), SEEK_SET);
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][c ^ (c >> 1)] = get2();
        get2();
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Measured][c ^ (c >> 1)] = get2();
        get2();
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Other][c ^ (c >> 1)] = get2();
        get2();
        Canon_WBpresets(2, 12);
        fseek(ifp, save1 + (0x00ba << 1), SEEK_SET);
        Canon_WBCTpresets(2);                       // BCADT
        offsetChannelBlackLevel = save1 + (0x0108 << 1);
      }
      break;

    case 1273:
    case 1275:
      imgdata.makernotes.canon.CanonColorDataVer = 6; // 600D / 1200D
      imgdata.makernotes.canon.CanonColorDataSubVer = get2();

      fseek(ifp, save1 + (0x003f << 1), SEEK_SET);
      FORC4 cam_mul[c ^ (c >> 1)] = (float)get2();
      fseek(ifp, save1 + (0x0044 << 1), SEEK_SET);
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][c ^ (c >> 1)] = get2();
      fseek(ifp, save1 + (0x0049 << 1), SEEK_SET);
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Measured][c ^ (c >> 1)] = get2();

      fseek(ifp, save1 + (0x0062 << 1), SEEK_SET);
      FORC4 sraw_mul[c ^ (c >> 1)] = get2();
      fseek(ifp, save1 + (0x0067 << 1), SEEK_SET);
      Canon_WBpresets(2, 12);
      fseek(ifp, save1 + (0x00bc << 1), SEEK_SET);
      Canon_WBCTpresets(0);                       // BCAT
      offsetChannelBlackLevel = save1 + (0x01df << 1);
      offsetWhiteLevels = save1 + (0x01e3 << 1);
      break;

    // 1DX / 5DmkIII / 6D / 100D / 650D / 700D / EOS M / 7DmkII / 750D / 760D
    case 1312:
    case 1313:
    case 1316:
    case 1506:
      imgdata.makernotes.canon.CanonColorDataVer = 7;
      imgdata.makernotes.canon.CanonColorDataSubVer = get2();

      fseek(ifp, save1 + (0x003f << 1), SEEK_SET);
      FORC4 cam_mul[c ^ (c >> 1)] = (float)get2();
      fseek(ifp, save1 + (0x0044 << 1), SEEK_SET);
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][c ^ (c >> 1)] = get2();
      fseek(ifp, save1 + (0x0049 << 1), SEEK_SET);
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Measured][c ^ (c >> 1)] = get2();

      fseek(ifp, save1 + (0x007b << 1), SEEK_SET);
      FORC4 sraw_mul[c ^ (c >> 1)] = get2();
      fseek(ifp, save1 + (0x0080 << 1), SEEK_SET);
      Canon_WBpresets(2, 12);
      fseek(ifp, save1 + (0x00d5 << 1), SEEK_SET);
      Canon_WBCTpresets(0);                       // BCAT

      if (imgdata.makernotes.canon.CanonColorDataSubVer == 10) {
        offsetChannelBlackLevel = save1 + (0x01f8 << 1);
        offsetWhiteLevels = save1 + (0x01fc << 1);
      } else if (imgdata.makernotes.canon.CanonColorDataSubVer == 11) {
        offsetChannelBlackLevel = save1 + (0x02d8 << 1);
        offsetWhiteLevels = save1 + (0x02dc << 1);
      }
      break;

    // 5DS / 5DS R / 80D / 1300D / 2000D / 4000D / 5D4 / 800D / 77D / 6D II / 200D
    case 1560:
    case 1592:
    case 1353:
    case 1602:
      imgdata.makernotes.canon.CanonColorDataVer = 8;
      imgdata.makernotes.canon.CanonColorDataSubVer = get2();

      fseek(ifp, save1 + (0x003f << 1), SEEK_SET);
      FORC4 cam_mul[c ^ (c >> 1)] = (float)get2();
      fseek(ifp, save1 + (0x0044 << 1), SEEK_SET);
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][c ^ (c >> 1)] = get2();
      fseek(ifp, save1 + (0x0049 << 1), SEEK_SET);
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Measured][c ^ (c >> 1)] = get2();

      fseek(ifp, save1 + (0x0080 << 1), SEEK_SET);
      FORC4 sraw_mul[c ^ (c >> 1)] = get2();
      fseek(ifp, save1 + (0x0085 << 1), SEEK_SET);
      Canon_WBpresets(2, 12);
      fseek(ifp, save1 + (0x0107 << 1), SEEK_SET);
      Canon_WBCTpresets(0);                       // BCAT

      if (imgdata.makernotes.canon.CanonColorDataSubVer == 14) { // 1300D / 2000D / 4000D
        offsetChannelBlackLevel = save1 + (0x022c << 1);
        offsetWhiteLevels = save1 + (0x0230 << 1);
      } else {
        offsetChannelBlackLevel = save1 + (0x030a << 1);
        offsetWhiteLevels = save1 + (0x030e << 1);
      }
      break;

    case 1816:  // EOS RP, SX70
    case 1820:	// M50
    case 1824:	// EOS R
      imgdata.makernotes.canon.CanonColorDataVer = 9;
      imgdata.makernotes.canon.CanonColorDataSubVer = get2();

      fseek(ifp, save1 + (0x0047 << 1), SEEK_SET);
      FORC4 cam_mul[c ^ (c >> 1)] = (float)get2();
      get2();
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][c ^ (c >> 1)] = get2();
      get2();
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Measured][c ^ (c >> 1)] = get2();
      fseek(ifp, save1 + (0x0088 << 1), SEEK_SET);
      Canon_WBpresets(2, 12);
      fseek(ifp, save1 + (0x010a << 1), SEEK_SET);
      Canon_WBCTpresets(0);
      offsetChannelBlackLevel = save1 + (0x0318 << 1);
      offsetWhiteLevels = save1 + (0x031c << 1);
      break;
    }

    if (offsetChannelBlackLevel) {
      fseek(ifp, offsetChannelBlackLevel, SEEK_SET);
      FORC4
        bls += (imgdata.makernotes.canon.ChannelBlackLevel[c ^ (c >> 1)] = get2());
      imgdata.makernotes.canon.AverageBlackLevel = bls / 4;
    }
    if (offsetWhiteLevels) {
      if ((offsetWhiteLevels - offsetChannelBlackLevel) != 8L)
        fseek(ifp, offsetWhiteLevels, SEEK_SET);
      imgdata.makernotes.canon.NormalWhiteLevel = get2();
      imgdata.makernotes.canon.SpecularWhiteLevel = get2();
      FORC4
        imgdata.color.linear_max[c] = imgdata.makernotes.canon.SpecularWhiteLevel;
    }
    fseek(ifp, save1, SEEK_SET);
  }
}

void CLASS setPentaxBodyFeatures(unsigned id)
{
  imgdata.lens.makernotes.CamID = id;

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
    imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_Pentax_K;
    imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Pentax_K;
    imgdata.lens.makernotes.CameraFormat = LIBRAW_FORMAT_APSC;
    break;
  case 0x13092:
  case 0x13240:
    imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_Pentax_K;
    imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Pentax_K;
    imgdata.lens.makernotes.CameraFormat = LIBRAW_FORMAT_FF;
    break;
  case 0x12e08:
  case 0x13010:
    imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_Pentax_645;
    imgdata.lens.makernotes.LensFormat = LIBRAW_FORMAT_MF;
    imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Pentax_645;
    imgdata.lens.makernotes.CameraFormat = LIBRAW_FORMAT_MF;
    break;
  case 0x12ee4:
  case 0x12f66:
  case 0x12f7a:
  case 0x1302e:
    imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_Pentax_Q;
    imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Pentax_Q;
    break;
  case 0x1320e:
    imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_FixedLens;
    imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_FixedLens;
    imgdata.lens.makernotes.CameraFormat = LIBRAW_FORMAT_APSC;
    imgdata.lens.makernotes.LensFormat = LIBRAW_FORMAT_APSC;
    imgdata.lens.makernotes.LensID = -1;
    imgdata.lens.makernotes.FocalType = 1;
    break;
  default:
    imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_FixedLens;
    imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_FixedLens;
  }
  return;
}

void CLASS PentaxISO(ushort c)
{
  int code[] = {3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,   16,   17,  18,  19,  20,
                21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,   34,   35,  36,  37,  38,
                39,  40,  41,  42,  43,  44,  45,  50,  100, 200, 400, 800, 1600, 3200, 258, 259, 260, 261,
                262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274,  275,  276, 277, 278};
  double value[] = {50,     64,     80,     100,    125,    160,    200,    250,   320,   400,    500,    640,
                    800,    1000,   1250,   1600,   2000,   2500,   3200,   4000,  5000,  6400,   8000,   10000,
                    12800,  16000,  20000,  25600,  32000,  40000,  51200,  64000, 80000, 102400, 128000, 160000,
                    204800, 258000, 325000, 409600, 516000, 650000, 819200, 50,    100,   200,    400,    800,
                    1600,   3200,   50,     70,     100,    140,    200,    280,   400,   560,    800,    1100,
                    1600,   2200,   3200,   4500,   6400,   9000,   12800,  18000, 25600, 36000,  51200};
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

void CLASS PentaxLensInfo(unsigned id, unsigned len) // tag 0x0207
{
  ushort iLensData = 0;
  uchar *table_buf;
  table_buf = (uchar *)malloc(MAX(len, 128));
  fread(table_buf, len, 1, ifp);
  if ((id < 0x12b9c) || (((id == 0x12b9c) ||  // K100D
                          (id == 0x12b9d) ||  // K110D
                          (id == 0x12ba2)) && // K100D Super
                         ((!table_buf[20] || (table_buf[20] == 0xff)))))
  {
    iLensData = 3;
    if (imgdata.lens.makernotes.LensID == -1)
      imgdata.lens.makernotes.LensID = (((unsigned)table_buf[0]) << 8) + table_buf[1];
  }
  else
    switch (len)
    {
    case 90: // LensInfo3
      iLensData = 13;
      if (imgdata.lens.makernotes.LensID == -1)
        imgdata.lens.makernotes.LensID = ((unsigned)((table_buf[1] & 0x0f) + table_buf[3]) << 8) + table_buf[4];
      break;
    case 91: // LensInfo4
      iLensData = 12;
      if (imgdata.lens.makernotes.LensID == -1)
        imgdata.lens.makernotes.LensID = ((unsigned)((table_buf[1] & 0x0f) + table_buf[3]) << 8) + table_buf[4];
      break;
    case 80: // LensInfo5
    case 128:
      iLensData = 15;
      if (imgdata.lens.makernotes.LensID == -1)
        imgdata.lens.makernotes.LensID = ((unsigned)((table_buf[1] & 0x0f) + table_buf[4]) << 8) + table_buf[5];
      break;
    case 168: // Ricoh GR III, id 0x1320e
      break;
    default:
      if (id >= 0x12b9c) // LensInfo2
      {
        iLensData = 4;
        if (imgdata.lens.makernotes.LensID == -1)
          imgdata.lens.makernotes.LensID = ((unsigned)((table_buf[0] & 0x0f) + table_buf[2]) << 8) + table_buf[3];
      }
    }
  if (iLensData)
  {
    if (table_buf[iLensData + 9] && (fabs(imgdata.lens.makernotes.CurFocal) < 0.1f))
      imgdata.lens.makernotes.CurFocal =
          10 * (table_buf[iLensData + 9] >> 2) * libraw_powf64l(4, (table_buf[iLensData + 9] & 0x03) - 2);
    if (table_buf[iLensData + 10] & 0xf0)
      imgdata.lens.makernotes.MaxAp4CurFocal =
          libraw_powf64l(2.0f, (float)((table_buf[iLensData + 10] & 0xf0) >> 4) / 4.0f);
    if (table_buf[iLensData + 10] & 0x0f)
      imgdata.lens.makernotes.MinAp4CurFocal =
          libraw_powf64l(2.0f, (float)((table_buf[iLensData + 10] & 0x0f) + 10) / 4.0f);

    if (iLensData != 12)
    {
      switch (table_buf[iLensData] & 0x06)
      {
      case 0:
        imgdata.lens.makernotes.MinAp4MinFocal = 22.0f;
        break;
      case 2:
        imgdata.lens.makernotes.MinAp4MinFocal = 32.0f;
        break;
      case 4:
        imgdata.lens.makernotes.MinAp4MinFocal = 45.0f;
        break;
      case 6:
        imgdata.lens.makernotes.MinAp4MinFocal = 16.0f;
        break;
      }
      if (table_buf[iLensData] & 0x70)
        imgdata.lens.makernotes.LensFStops = ((float)(((table_buf[iLensData] & 0x70) >> 4) ^ 0x07)) / 2.0f + 5.0f;

      imgdata.lens.makernotes.MinFocusDistance = (float)(table_buf[iLensData + 3] & 0xf8);
      imgdata.lens.makernotes.FocusRangeIndex = (float)(table_buf[iLensData + 3] & 0x07);

      if ((table_buf[iLensData + 14] > 1) && (fabs(imgdata.lens.makernotes.MaxAp4CurFocal) < 0.7f))
        imgdata.lens.makernotes.MaxAp4CurFocal =
            libraw_powf64l(2.0f, (float)((table_buf[iLensData + 14] & 0x7f) - 1) / 32.0f);
    }
    else if ((id != 0x12e76) && // K-5
             (table_buf[iLensData + 15] > 1) && (fabs(imgdata.lens.makernotes.MaxAp4CurFocal) < 0.7f))
    {
      imgdata.lens.makernotes.MaxAp4CurFocal =
          libraw_powf64l(2.0f, (float)((table_buf[iLensData + 15] & 0x7f) - 1) / 32.0f);
    }
  }
  free(table_buf);
  return;
}

void CLASS parsePentaxMakernotes(int base, unsigned tag, unsigned type, unsigned len, unsigned dng_writer) {

  int c;
  if (tag == 0x0005) {
      unique_id = get4();
      setPentaxBodyFeatures(unique_id);
  } else if (tag == 0x0008) {  /* 4 is raw, 7 is raw w/ pixel shift, 8 is raw w/ dynamic pixel shift */
    imgdata.makernotes.pentax.Quality = get2();
  } else if (tag == 0x000d) {
      imgdata.shootinginfo.FocusMode = imgdata.makernotes.pentax.FocusMode = get2();
  } else if (tag == 0x000e) {
      imgdata.shootinginfo.AFPoint = imgdata.makernotes.pentax.AFPointSelected = get2();
  } else if (tag == 0x000f) {
      imgdata.makernotes.pentax.AFPointsInFocus = getint(type);
  } else if (tag == 0x0010) {
      imgdata.makernotes.pentax.FocusPosition = get2();
  } else if (tag == 0x0013) {
      imgdata.lens.makernotes.CurAp = (float)get2() / 10.0f;
  } else if (tag == 0x0014) {
      PentaxISO(get2());
  } else if (tag == 0x0017) {
      imgdata.shootinginfo.MeteringMode = get2();
  } else if (tag == 0x001d) {
      imgdata.lens.makernotes.CurFocal = (float)get4() / 100.0f;
  } else if (tag == 0x0034) {
      uchar uc;
      FORC4 {
        fread(&uc, 1, 1, ifp);
        imgdata.makernotes.pentax.DriveMode[c] = uc;
      }
      imgdata.shootinginfo.DriveMode = imgdata.makernotes.pentax.DriveMode[0];
  } else if (tag == 0x0038) {
      imgdata.sizes.raw_crop.cleft = get2();
      imgdata.sizes.raw_crop.ctop = get2();
  } else if (tag == 0x0039) {
      imgdata.sizes.raw_crop.cwidth = get2();
      imgdata.sizes.raw_crop.cheight = get2();
  } else if (tag == 0x003f) {
      unsigned a = fgetc(ifp) << 8;
      imgdata.lens.makernotes.LensID = a | fgetc(ifp);
  } else if (tag == 0x0047) {
      imgdata.other.CameraTemperature = (float)fgetc(ifp);
  } else if (tag == 0x004d) {
      if (type == 9)
        imgdata.other.FlashEC = getreal(type) / 256.0f;
      else
        imgdata.other.FlashEC = (float)((signed short)fgetc(ifp)) / 6.0f;
  } else if (tag == 0x005c) {
      fgetc(ifp);
      imgdata.shootinginfo.ImageStabilization = (short)fgetc(ifp);
  } else if (tag == 0x0072) {
      imgdata.makernotes.pentax.AFAdjustment = get2();
  } else if (tag == 0x007e) {
      imgdata.color.linear_max[0] =
        imgdata.color.linear_max[1] =
        imgdata.color.linear_max[2] =
        imgdata.color.linear_max[3] = (long)(-1) * get4();
  } else if ((tag == 0x0203) && (dng_writer == nonDNG)) {
      for (int i = 0; i < 3; i++)
        FORC3 cmatrix[i][c] = ((short)get2()) / 8192.0;
  } else if (tag == 0x0205) {
      if (len < 25) {
        fseek (ifp, 10, SEEK_CUR);
        imgdata.makernotes.pentax.MultiExposure = (fgetc(ifp) & 0x0f);
      }
  } else if (tag == 0x0207) {
      if (len < 65535) // Safety belt
        PentaxLensInfo(imgdata.lens.makernotes.CamID, len);
  } else if ((tag >= 0x020d) && (tag <= 0x0214)) {
      FORC4 imgdata.color.WB_Coeffs[Pentax_wb_list1[tag - 0x020d]][c ^ (c >> 1)] = get2();
  } else if (tag == 0x0221) {
      int nWB = get2();
      if (nWB <= sizeof(imgdata.color.WBCT_Coeffs) / sizeof(imgdata.color.WBCT_Coeffs[0]))
      FORC(nWB) {
        imgdata.color.WBCT_Coeffs[c][0] = (unsigned)0xcfc6 - get2();
        fseek(ifp, 2, SEEK_CUR);
        imgdata.color.WBCT_Coeffs[c][1] = get2();
        imgdata.color.WBCT_Coeffs[c][2] = imgdata.color.WBCT_Coeffs[c][4] = 0x2000;
        imgdata.color.WBCT_Coeffs[c][3] = get2();
      }
  } else if (tag == 0x0215) {
      fseek(ifp, 16, SEEK_CUR);
      sprintf(imgdata.shootinginfo.InternalBodySerial, "%d", get4());
  } else if (tag == 0x0229) {
      stmread(imgdata.shootinginfo.BodySerial, len, ifp);
  } else if (tag == 0x022d) {
      int wb_ind;
      getc(ifp);
      for (int wb_cnt = 0; wb_cnt < nPentax_wb_list2; wb_cnt++) {
        wb_ind = getc(ifp);
        if (wb_ind < nPentax_wb_list2)
          FORC4 imgdata.color.WB_Coeffs[Pentax_wb_list2[wb_ind]][c ^ (c >> 1)] = get2();
      }
  } else if (tag == 0x0239) {  // Q-series lens info (LensInfoQ)
      char LensInfo[20];
      fseek(ifp, 12, SEEK_CUR);
      stread(imgdata.lens.makernotes.Lens, 30, ifp);
      strcat(imgdata.lens.makernotes.Lens, " ");
      stread(LensInfo, 20, ifp);
      strcat(imgdata.lens.makernotes.Lens, LensInfo);
  }
}

void CLASS parseRicohMakernotes(int base, unsigned tag, unsigned type, unsigned len, unsigned dng_writer) {

   if (tag == 0x0005) {
     char buffer[17];
     int c;
     int count = 0;
     fread(buffer, 16, 1, ifp);
     buffer[16] = 0;
     FORC(16) {
       if ((isspace(buffer[c])) || (buffer[c] == 0x2D) || (isalnum(buffer[c])))
         count++;
       else
         break;
     }
     if (count == 16) {
       if (strncmp(model, "GXR", 3)) {
         sprintf(imgdata.shootinginfo.BodySerial, "%8s", buffer + 8);
       }
       buffer[8] = 0;
       sprintf(imgdata.shootinginfo.InternalBodySerial, "%8s", buffer);

     } else {
       sprintf(imgdata.shootinginfo.BodySerial, "%02x%02x%02x%02x",
                buffer[4], buffer[5], buffer[6], buffer[7]);
       sprintf(imgdata.shootinginfo.InternalBodySerial, "%02x%02x%02x%02x",
                buffer[8], buffer[9], buffer[10], buffer[11]);
     }

   } else if ((tag == 0x1001) && (type == 3)) {
     imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_FixedLens;
     imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_FixedLens;
     imgdata.lens.makernotes.CameraFormat = LIBRAW_FORMAT_APSC;
     imgdata.lens.makernotes.LensID = -1;
     imgdata.lens.makernotes.FocalType = 1;
     imgdata.shootinginfo.ExposureProgram = get2();

   } else if (tag == 0x1002) {
     imgdata.shootinginfo.DriveMode =  get2();

   } else if (tag == 0x1006) {
     imgdata.shootinginfo.FocusMode =  get2();

   } else if ((tag == 0x100b) && (type == 10)) {
     imgdata.other.FlashEC = getreal(type);

   } else if ((tag == 0x1017) && (get2() == 2)) {
     strcpy(imgdata.lens.makernotes.Attachment, "Wide-Angle Adapter");

   } else if (tag == 0x1500) {
     imgdata.lens.makernotes.CurFocal = getreal(type);

   } else if ((tag == 0x2001) && !strncmp(model, "GXR", 3)) {
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
     fseek(ifp, get4() + 20, SEEK_SET);
     stread(imgdata.shootinginfo.BodySerial, 12, ifp);
     get2();
     imgdata.lens.makernotes.LensID = getc(ifp) - '0';
     switch (imgdata.lens.makernotes.LensID)
     {
     case 1:
     case 2:
     case 3:
     case 5:
     case 6:
       imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_FixedLens;
       imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_RicohModule;
       break;
     case 8:
       imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Leica_M;
       imgdata.lens.makernotes.CameraFormat = LIBRAW_FORMAT_APSC;
       imgdata.lens.makernotes.LensID = -1;
       break;
     default:
       imgdata.lens.makernotes.LensID = -1;
     }
     fseek(ifp, 17, SEEK_CUR);
     stread(imgdata.lens.LensSerial, 12, ifp);
   }
}

void CLASS setPhaseOneFeatures(unsigned id)
{

  ushort i;
  static const struct
  {
    ushort id;
    char t_model[32];
  } p1_unique[] = {
      // Phase One section:
      {1, "Hasselblad V"},
      {10, "PhaseOne/Mamiya"},
      {12, "Contax 645"},
      {16, "Hasselblad V"},
      {17, "Hasselblad V"},
      {18, "Contax 645"},
      {19, "PhaseOne/Mamiya"},
      {20, "Hasselblad V"},
      {21, "Contax 645"},
      {22, "PhaseOne/Mamiya"},
      {23, "Hasselblad V"},
      {24, "Hasselblad H"},
      {25, "PhaseOne/Mamiya"},
      {32, "Contax 645"},
      {34, "Hasselblad V"},
      {35, "Hasselblad V"},
      {36, "Hasselblad H"},
      {37, "Contax 645"},
      {38, "PhaseOne/Mamiya"},
      {39, "Hasselblad V"},
      {40, "Hasselblad H"},
      {41, "Contax 645"},
      {42, "PhaseOne/Mamiya"},
      {44, "Hasselblad V"},
      {45, "Hasselblad H"},
      {46, "Contax 645"},
      {47, "PhaseOne/Mamiya"},
      {48, "Hasselblad V"},
      {49, "Hasselblad H"},
      {50, "Contax 645"},
      {51, "PhaseOne/Mamiya"},
      {52, "Hasselblad V"},
      {53, "Hasselblad H"},
      {54, "Contax 645"},
      {55, "PhaseOne/Mamiya"},
      {67, "Hasselblad V"},
      {68, "Hasselblad H"},
      {69, "Contax 645"},
      {70, "PhaseOne/Mamiya"},
      {71, "Hasselblad V"},
      {72, "Hasselblad H"},
      {73, "Contax 645"},
      {74, "PhaseOne/Mamiya"},
      {76, "Hasselblad V"},
      {77, "Hasselblad H"},
      {78, "Contax 645"},
      {79, "PhaseOne/Mamiya"},
      {80, "Hasselblad V"},
      {81, "Hasselblad H"},
      {82, "Contax 645"},
      {83, "PhaseOne/Mamiya"},
      {84, "Hasselblad V"},
      {85, "Hasselblad H"},
      {86, "Contax 645"},
      {87, "PhaseOne/Mamiya"},
      {99, "Hasselblad V"},
      {100, "Hasselblad H"},
      {101, "Contax 645"},
      {102, "PhaseOne/Mamiya"},
      {103, "Hasselblad V"},
      {104, "Hasselblad H"},
      {105, "PhaseOne/Mamiya"},
      {106, "Contax 645"},
      {112, "Hasselblad V"},
      {113, "Hasselblad H"},
      {114, "Contax 645"},
      {115, "PhaseOne/Mamiya"},
      {131, "Hasselblad V"},
      {132, "Hasselblad H"},
      {133, "Contax 645"},
      {134, "PhaseOne/Mamiya"},
      {135, "Hasselblad V"},
      {136, "Hasselblad H"},
      {137, "Contax 645"},
      {138, "PhaseOne/Mamiya"},
      {140, "Hasselblad V"},
      {141, "Hasselblad H"},
      {142, "Contax 645"},
      {143, "PhaseOne/Mamiya"},
      {148, "Hasselblad V"},
      {149, "Hasselblad H"},
      {150, "Contax 645"},
      {151, "PhaseOne/Mamiya"},
      {160, "A-250"},
      {161, "A-260"},
      {162, "A-280"},
      {167, "Hasselblad V"},
      {168, "Hasselblad H"},
      {169, "Contax 645"},
      {170, "PhaseOne/Mamiya"},
      {172, "Hasselblad V"},
      {173, "Hasselblad H"},
      {174, "Contax 645"},
      {175, "PhaseOne/Mamiya"},
      {176, "Hasselblad V"},
      {177, "Hasselblad H"},
      {178, "Contax 645"},
      {179, "PhaseOne/Mamiya"},
      {180, "Hasselblad V"},
      {181, "Hasselblad H"},
      {182, "Contax 645"},
      {183, "PhaseOne/Mamiya"},
      {208, "Hasselblad V"},
      {211, "PhaseOne/Mamiya"},
      {448, "Phase One 645AF"},
      {457, "Phase One 645DF"},
      {471, "Phase One 645DF+"},
      {704, "Phase One iXA"},
      {705, "Phase One iXA - R"},
      {706, "Phase One iXU 150"},
      {707, "Phase One iXU 150 - NIR"},
      {708, "Phase One iXU 180"},
      {721, "Phase One iXR"},
      // Leaf section:
      {333, "Mamiya"},
      {329, "Universal"},
      {330, "Hasselblad H1/H2"},
      {332, "Contax"},
      {336, "AFi"},
      {327, "Mamiya"},
      {324, "Universal"},
      {325, "Hasselblad H1/H2"},
      {326, "Contax"},
      {335, "AFi"},
      {340, "Mamiya"},
      {337, "Universal"},
      {338, "Hasselblad H1/H2"},
      {339, "Contax"},
      {323, "Mamiya"},
      {320, "Universal"},
      {322, "Hasselblad H1/H2"},
      {321, "Contax"},
      {334, "AFi"},
      {369, "Universal"},
      {370, "Mamiya"},
      {371, "Hasselblad H1/H2"},
      {372, "Contax"},
      {373, "Afi"},
  };
  imgdata.lens.makernotes.CamID = id;
  if (id && !imgdata.lens.makernotes.body[0])
  {
    for (i = 0; i < sizeof p1_unique / sizeof *p1_unique; i++)
      if (id == p1_unique[i].id)
      {
        strcpy(imgdata.lens.makernotes.body, p1_unique[i].t_model);
      }
  }
  return;
}

void CLASS parseFujiMakernotes(unsigned tag, unsigned type, unsigned len, unsigned dng_writer)
{
  if ((dng_writer == nonDNG) && (tag == 0x0010)) {
    char FujiSerial[sizeof(imgdata.shootinginfo.InternalBodySerial)];
    char *words[4];
    char yy[2], mm[3], dd[3], ystr[16], ynum[16];
    int year, nwords, ynum_len;
    unsigned c;
    stmread(FujiSerial, len, ifp);
    nwords = getwords(FujiSerial, words, 4, sizeof(imgdata.shootinginfo.InternalBodySerial));
    for (int i = 0; i < nwords; i++) {
      mm[2] = dd[2] = 0;
      if (strnlen(words[i], sizeof(imgdata.shootinginfo.InternalBodySerial) - 1) < 18) {
        if (i == 0) {
          strncpy(imgdata.shootinginfo.InternalBodySerial, words[0],
                  sizeof(imgdata.shootinginfo.InternalBodySerial) - 1);
        } else {
          char tbuf[sizeof(imgdata.shootinginfo.InternalBodySerial)];
          snprintf(tbuf, sizeof(tbuf), "%s %s", imgdata.shootinginfo.InternalBodySerial, words[i]);
          strncpy(imgdata.shootinginfo.InternalBodySerial, tbuf,
                  sizeof(imgdata.shootinginfo.InternalBodySerial) - 1);
        }
      } else {
        strncpy(dd, words[i] + strnlen(words[i], sizeof(imgdata.shootinginfo.InternalBodySerial) - 1) - 14, 2);
        strncpy(mm, words[i] + strnlen(words[i], sizeof(imgdata.shootinginfo.InternalBodySerial) - 1) - 16, 2);
        strncpy(yy, words[i] + strnlen(words[i], sizeof(imgdata.shootinginfo.InternalBodySerial) - 1) - 18, 2);
        year = (yy[0] - '0') * 10 + (yy[1] - '0');
        if (year < 70) year += 2000; else year += 1900;

        ynum_len = MIN((sizeof(ynum)-1), (int)strnlen(words[i], sizeof(imgdata.shootinginfo.InternalBodySerial) - 1) - 18);
        strncpy(ynum, words[i], ynum_len);
        ynum[ynum_len] = 0;
        for (int j = 0; ynum[j] && ynum[j + 1] && sscanf(ynum + j, "%2x", &c); j += 2)
          ystr[j / 2] = c;
        ystr[ynum_len / 2 + 1] = 0;
        strcpy(model2, ystr);

        if (i == 0) {
          char tbuf[sizeof(imgdata.shootinginfo.InternalBodySerial)];

          if (nwords == 1) {
            snprintf(tbuf, sizeof(tbuf), "%s %s %d:%s:%s",
                     words[0] + strnlen(words[0], sizeof(imgdata.shootinginfo.InternalBodySerial) - 1) - 12, ystr,
                     year, mm, dd);

          } else {
            snprintf(tbuf, sizeof(tbuf), "%s %d:%s:%s %s", ystr, year, mm, dd,
                     words[0] + strnlen(words[0], sizeof(imgdata.shootinginfo.InternalBodySerial) - 1) - 12);
          }
          strncpy(imgdata.shootinginfo.InternalBodySerial, tbuf,
                  sizeof(imgdata.shootinginfo.InternalBodySerial) - 1);

        } else {
          char tbuf[sizeof(imgdata.shootinginfo.InternalBodySerial)];
          snprintf(tbuf, sizeof(tbuf), "%s %s %d:%s:%s %s", imgdata.shootinginfo.InternalBodySerial, ystr, year, mm,
                   dd, words[i] + strnlen(words[i], sizeof(imgdata.shootinginfo.InternalBodySerial) - 1) - 12);
          strncpy(imgdata.shootinginfo.InternalBodySerial, tbuf,
                  sizeof(imgdata.shootinginfo.InternalBodySerial) - 1);
        }
      }
    }

  } else switch (tag) {
  case 0x1002:
    imgdata.makernotes.fuji.WB_Preset = get2();
    break;
  case 0x1011:
    imgdata.other.FlashEC = getreal(type);
    break;
  case 0x1020:
    imgdata.makernotes.fuji.Macro = get2();
    break;
  case 0x1021:
    imgdata.makernotes.fuji.FocusMode = imgdata.shootinginfo.FocusMode = get2();
    break;
  case 0x1022:
    imgdata.makernotes.fuji.AFMode = get2();
    break;
  case 0x1023:
    imgdata.makernotes.fuji.FocusPixel[0] = get2();
    imgdata.makernotes.fuji.FocusPixel[1] = get2();
    break;
  case 0x1034:
    imgdata.makernotes.fuji.ExrMode = get2();
    break;
  case 0x104d:
    imgdata.makernotes.fuji.CropMode = get2();
    break;
  case 0x1050:
    imgdata.makernotes.fuji.ShutterType = get2();
    break;
  case 0x1400:
    imgdata.makernotes.fuji.FujiDynamicRange = get2();
    break;
  case 0x1401:
    imgdata.makernotes.fuji.FujiFilmMode = get2();
    break;
  case 0x1402:
    imgdata.makernotes.fuji.FujiDynamicRangeSetting = get2();
    break;
  case 0x1403:
    imgdata.makernotes.fuji.FujiDevelopmentDynamicRange = get2();
    break;
  case 0x140b:
    imgdata.makernotes.fuji.FujiAutoDynamicRange = get2();
    break;
  case 0x1404:
    imgdata.lens.makernotes.MinFocal = getreal(type);
    break;
  case 0x1405:
    imgdata.lens.makernotes.MaxFocal = getreal(type);
    break;
  case 0x1406:
    imgdata.lens.makernotes.MaxAp4MinFocal = getreal(type);
    break;
  case 0x1407:
    imgdata.lens.makernotes.MaxAp4MaxFocal = getreal(type);
    break;
  case 0x1422:
    imgdata.makernotes.fuji.ImageStabilization[0] = get2();
    imgdata.makernotes.fuji.ImageStabilization[1] = get2();
    imgdata.makernotes.fuji.ImageStabilization[2] = get2();
    imgdata.shootinginfo.ImageStabilization =
        (imgdata.makernotes.fuji.ImageStabilization[0] << 9) + imgdata.makernotes.fuji.ImageStabilization[1];
    break;
  case 0x1431:
    imgdata.makernotes.fuji.Rating = get4();
    break;
  case 0x3820:
    imgdata.makernotes.fuji.FrameRate = get2();
    break;
  case 0x3821:
    imgdata.makernotes.fuji.FrameWidth = get2();
    break;
  case 0x3822:
    imgdata.makernotes.fuji.FrameHeight = get2();
    break;
  }
  return;
}

void CLASS parseSamsungMakernotes(int base, unsigned tag, unsigned type, unsigned len, unsigned dng_writer)
{
   int i, c;
   if (tag == 0x0002) {
     if (get4() == 0x2000) {
       imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Samsung_NX;
     } else if (!strncmp(model, "NX mini", 7)) {
       imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Samsung_NX_M;
     } else {
       imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_FixedLens;
       imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_FixedLens;
     }

   } else if (tag == 0x0003) {
     imgdata.lens.makernotes.CamID = unique_id = get4();

   } else if (tag == 0x0043) {
     if ((i = get4())) {
       imgdata.other.CameraTemperature = (float)i;
       if (get4() == 10)
         imgdata.other.CameraTemperature /= 10.0f;
     }

    } else if ((tag == 0xa002) && (dng_writer != AdobeDNG)) {
        stmread(imgdata.shootinginfo.BodySerial, len, ifp);

   } else if (tag == 0xa003) {
     imgdata.lens.makernotes.LensID = get2();
     if (imgdata.lens.makernotes.LensID)
       imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_Samsung_NX;

   } else if (tag == 0xa005) {
     stmread(imgdata.lens.InternalLensSerial, len, ifp);

   } else if (tag == 0xa010) {
     FORC4 imgdata.makernotes.samsung.ImageSizeFull[c] = get4();
     FORC4 imgdata.makernotes.samsung.ImageSizeCrop[c] = get4();

   } else if ((tag == 0xa011) && ((len == 1) || (len == 2)) && (type == 3)) {
        imgdata.makernotes.samsung.ColorSpace[0] = (int)get2();
        if (len == 2) imgdata.makernotes.samsung.ColorSpace[1] = (int)get2();

   } else if (tag == 0xa019) {
     imgdata.lens.makernotes.CurAp = getreal(type);

   } else if ((tag == 0xa01a) && (!imgdata.lens.FocalLengthIn35mmFormat)) {
     imgdata.lens.makernotes.FocalLengthIn35mmFormat = get4() / 10.0f;

   } else if (tag == 0xa020) {
     FORC(11) imgdata.makernotes.samsung.SamsungKey[c] = get4();

   } else if ((tag == 0xa021)   && (dng_writer == nonDNG)){
     FORC4 cam_mul[c ^ (c >> 1)] =
        get4() - imgdata.makernotes.samsung.SamsungKey[c];

  } else if (tag == 0xa022) {
    FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][c ^ (c >> 1)] =
      get4() - imgdata.makernotes.samsung.SamsungKey[c + 4];
    if (imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][0] < (imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][1] >> 1))
    {
      imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][1] = imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][1] >> 4;
      imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][3] = imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][3] >> 4;
    }

  } else if (tag == 0xa023) {
    imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][0] =
      get4() - imgdata.makernotes.samsung.SamsungKey[8];
    imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][1] =
      get4() - imgdata.makernotes.samsung.SamsungKey[9];
    imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][3] =
      get4() - imgdata.makernotes.samsung.SamsungKey[10];
    imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][2] =
      get4() - imgdata.makernotes.samsung.SamsungKey[0];
    if (imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][0] < (imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][1] >> 1))
    {
      imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][1] = imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][1] >> 4;
      imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][3] = imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][3] >> 4;
    }

  } else if (tag == 0xa024) {
    FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_D65][c ^ (c >> 1)] =
            get4() - imgdata.makernotes.samsung.SamsungKey[c + 1];
    if (imgdata.color.WB_Coeffs[LIBRAW_WBI_D65][0] < (imgdata.color.WB_Coeffs[LIBRAW_WBI_D65][1] >> 1))
    {
      imgdata.color.WB_Coeffs[LIBRAW_WBI_D65][1] = imgdata.color.WB_Coeffs[LIBRAW_WBI_D65][1] >> 4;
      imgdata.color.WB_Coeffs[LIBRAW_WBI_D65][3] = imgdata.color.WB_Coeffs[LIBRAW_WBI_D65][3] >> 4;
    }

  } else if (tag == 0xa025) {
      unsigned t = get4() + imgdata.makernotes.samsung.SamsungKey[0];
      if (t == 4096) imgdata.makernotes.samsung.DigitalGain = 1.0;
      else imgdata.makernotes.samsung.DigitalGain = ((double)t) / 4096.0;

  } else if ((tag == 0xa028)  && (dng_writer == nonDNG)) {
        FORC4 cblack[c ^ (c >> 1)] = get4() - imgdata.makernotes.samsung.SamsungKey[c];

  } else if ((tag == 0xa030) && (len == 9)) {
    for (i = 0; i < 3; i++)
      FORC3 imgdata.color.ccm[i][c] =
        (float)((short)((get4() + imgdata.makernotes.samsung.SamsungKey[i * 3 + c]))) / 256.0;

  } else if ((tag == 0xa032) && (len == 9) && (dng_writer == nonDNG)) {
    double aRGB_cam[3][3];
    FORC(9) ((double *)aRGB_cam)[c] =
        ((double)((short)((get4() + imgdata.makernotes.samsung.SamsungKey[c])))) / 256.0;
    aRGB_coeff(aRGB_cam);
  }

}

#define imSony imgdata.makernotes.sony
#define icWBC imgdata.color.WB_Coeffs
#define icWBCTC imgdata.color.WBCT_Coeffs
#define ilm imgdata.lens.makernotes
void CLASS setSonyBodyFeatures(unsigned id)
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
      {256, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {257, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {258, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {259, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {260, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {261, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {262, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {263, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {264, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {265, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {266, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {267, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {268, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {269, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {270, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {271, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {272, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {273, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {274, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {275, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {276, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {277, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {278, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {279, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {280, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_SLT, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {281, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_SLT, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {282, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {283, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_DSLR, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {284, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {285, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_SLT, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {286, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_SLT, 0, 2, 0x1218, 0x01bd, 0x1178, 0x1179, 0x112c},
      {287, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_SLT, 0, 2, 0x1218, 0x01bd, 0x1178, 0x1179, 0x112c},
      {288, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 1, 0x113e, 0x01bd, 0x1174, 0x1175, 0x112c},
      {289, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 2, 0x1218, 0x01bd, 0x1178, 0x1179, 0x112c},
      {290, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 2, 0x1218, 0x01bd, 0x1178, 0x1179, 0x112c},
      {291, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_SLT, 0, 3, 0x11f4, 0x01bd, 0x1154, 0x1155, 0x1108},
      {292, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_SLT, 0, 3, 0x11f4, 0x01bd, 0x1154, 0x1155, 0x1108},
      {293, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 3, 0x11f4, 0x01bd, 0x1154, 0x1155, 0x1108},
      {294, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_SLT, 0, 5, 0x1254, 0x01aa, 0x11ac, 0x11ad, 0x1160},
      {295, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 5, 0x1254, 0x01aa, 0x11ac, 0x11ad, 0x1160},
      {296, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 5, 0x1254, 0x01aa, 0x11ac, 0x11ad, 0x1160},
      {297, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC, LIBRAW_MOUNT_FixedLens, 5, 0x1254, 0xffff, 0x11ac, 0x11ad, 0x1160},
      {298, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC, LIBRAW_MOUNT_FixedLens, 5, 0x1258, 0xffff, 0x11ac, 0x11ad, 0x1160},
      {299, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 5, 0x1254, 0x01aa, 0x11ac, 0x11ad, 0x1160},
      {300, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 5, 0x1254, 0x01aa, 0x11ac, 0x11ad, 0x1160},
      {301, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {302, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 5, 0x1280, 0x01aa, 0x11ac, 0x11ad, 0x1160},
      {303, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_SLT, 0, 5, 0x1280, 0x01aa, 0x11ac, 0x11ad, 0x1160},
      {304, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {305, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 5, 0x1280, 0x01aa, 0x11ac, 0x11ad, 0x1160},
      {306, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 7, 0x0344, 0xffff, 0x025c, 0x025d, 0x0210},
      {307, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_NEX, 0, 5, 0x1254, 0x01aa, 0x11ac, 0x11ad, 0x1160},
      {308, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC, LIBRAW_MOUNT_FixedLens, 6, 0x113c, 0xffff, 0x1064, 0x1065, 0x1018},
      {309, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC, LIBRAW_MOUNT_FixedLens, 7, 0x0344, 0xffff, 0x025c, 0x025d, 0x0210},
      {310, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC, LIBRAW_MOUNT_FixedLens, 5, 0x1258, 0xffff, 0x11ac, 0x11ad, 0x1160},
      {311, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 7, 0x0344, 0xffff, 0x025c, 0x025d, 0x0210},
      {312, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 7, 0x0344, 0xffff, 0x025c, 0x025d, 0x0210},
      {313, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 7, 0x0344, 0x01aa, 0x025c, 0x025d, 0x0210},
      {314, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {315, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {316, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {317, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC, LIBRAW_MOUNT_FixedLens, 7, 0x0344, 0xffff, 0x025c, 0x025d, 0x0210},
      {318, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 7, 0x0344, 0xffff, 0x025c, 0x025d, 0x0210},
      {319, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_ILCA, 0, 7, 0x0344, 0x01a0, 0x025c, 0x025d, 0x0210},
      {320, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {321, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {322, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {323, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {324, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {325, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {326, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {327, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {328, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {329, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {330, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {331, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {332, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {333, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {334, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {335, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {336, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {337, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {338, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {339, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 7, 0x0344, 0x01a0, 0x025c, 0x025d, 0x0210},
      {340, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 7, 0x0344, 0xffff, 0x025c, 0x025d, 0x0210},
      {341, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC, LIBRAW_MOUNT_FixedLens, 8, 0x0346, 0xffff, 0x025c, 0x025d, 0x0210},
      {342, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC, LIBRAW_MOUNT_FixedLens, 8, 0x0346, 0xffff, 0x025c, 0x025d, 0x0210},
      {343, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {344, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC, LIBRAW_MOUNT_FixedLens, 8, 0x0346, 0xffff, 0x025c, 0x025d, 0x0210},
      {345, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {346, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 7, 0x0344, 0x01a0, 0x025c, 0x025d, 0x0210},
      {347, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 8, 0x0346, 0x01cb, 0x025c, 0x025d, 0x0210},
      {348, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {349, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {350, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 8, 0x0346, 0x01cb, 0x025c, 0x025d, 0x0210},
      {351, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {352, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {353, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_ILCA, 0, 7, 0x0344, 0x01a0, 0x025c, 0x025d, 0x0210},
      {354, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Minolta_A, LIBRAW_SONY_ILCA, 0, 8, 0x0346, 0x01cd, 0x025c, 0x025d, 0x0210},
      {355, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC, LIBRAW_MOUNT_FixedLens, 8, 0x0346, 0xffff, 0x025c, 0x025d, 0x0210},
      {356, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC, LIBRAW_MOUNT_FixedLens, 8, 0x0346, 0xffff, 0x025c, 0x025d, 0x0210},
      {357, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 8, 0x0346, 0x01cd, 0x025c, 0x025d, 0x0210},
      {358, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 9, 0x0320, 0x019f, 0x024b, 0x024c, 0x0208},
      {359, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {360, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 8, 0x0346, 0x01cd, 0x025c, 0x025d, 0x0210},
      {361, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {362, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 9, 0x0320, 0x019f, 0x024b, 0x024c, 0x0208},
      {363, LIBRAW_FORMAT_FF, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 9, 0x0320, 0x019f, 0x024b, 0x024c, 0x0208},
      {364, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC, LIBRAW_MOUNT_FixedLens, 8, 0x0346, 0xffff, 0x025c, 0x025d, 0x0210},
      {365, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC, LIBRAW_MOUNT_FixedLens, 9, 0x0320, 0xffff, 0x024b, 0x024c, 0x0208},
      {366, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC, LIBRAW_MOUNT_FixedLens, 9, 0x0320, 0xffff, 0x024b, 0x024c, 0x0208},
      {367, LIBRAW_FORMAT_1div2p3INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC, LIBRAW_MOUNT_FixedLens, 9, 0x0320, 0xffff, 0x024b, 0x024c, 0x0208},
      {368, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {369, LIBRAW_FORMAT_1INCH, LIBRAW_MOUNT_FixedLens, LIBRAW_SONY_DSC, LIBRAW_MOUNT_FixedLens, 9, 0x0320, 0xffff, 0x024b, 0x024c, 0x0208},
      {370, 0, 0, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
      {371, LIBRAW_FORMAT_APSC, LIBRAW_MOUNT_Sony_E, LIBRAW_SONY_ILCE, 0, 9, 0x0320, 0x019f, 0x024b, 0x024c, 0x0208},
  };
  ilm.CamID = id;

  if (id == 2)
  {
    ilm.CameraMount = ilm.LensMount = LIBRAW_MOUNT_FixedLens;
    imSony.SonyCameraType = LIBRAW_SONY_DSC;
    imSony.group2010 = 0;
    imSony.real_iso_offset = 0xffff;
    imSony.ImageCount3_offset = 0xffff;
    return;
  }
  else
    idx = id - 256;

  if ((idx >= 0) && (idx < sizeof SonyCamFeatures / sizeof *SonyCamFeatures))
  {
    if (!SonyCamFeatures[idx].scf[2])
      return;
    ilm.CameraFormat = SonyCamFeatures[idx].scf[1];
    ilm.CameraMount = SonyCamFeatures[idx].scf[2];
    imSony.SonyCameraType = SonyCamFeatures[idx].scf[3];
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

    if ((id == 306) || (id == 311))
    {
      if (imSony.firmware < 1.2f)
        imSony.ImageCount3_offset = 0x01aa;
      else
        imSony.ImageCount3_offset = 0x01c0;
    }
    else if (id == 312)
    {
      if (imSony.firmware < 2.0f)
        imSony.ImageCount3_offset = 0x01aa;
      else
        imSony.ImageCount3_offset = 0x01c0;
    }
    else if ((id == 318) || (id == 340))
    {
      if (imSony.firmware < 1.2f)
        imSony.ImageCount3_offset = 0x01a0;
      else
        imSony.ImageCount3_offset = 0x01b6;
    }
  }
}

void CLASS parseSonyLensType2(uchar a, uchar b)
{
  ushort lid2;
  lid2 = (((ushort)a) << 8) | ((ushort)b);
  if (!lid2)
    return;
  if (lid2 < 0x100)
  {
    if ((ilm.AdapterID != 0x4900) && (ilm.AdapterID != 0xEF00))
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
      case 239:
        ilm.LensMount = LIBRAW_MOUNT_Canon_EF;
        break;
      }
    }
  }
  else
    ilm.LensID = lid2;
  if ((lid2 >= 50481) && (lid2 < 50500))
  {
    strcpy(ilm.Adapter, "MC-11");
    ilm.AdapterID = 0x4900;
  }
  return;
}

#define strnXcat(buf, string) strncat(buf, string, LIM(sizeof(buf) - strbuflen(buf) - 1, 0, sizeof(buf)))

void CLASS parseSonyLensFeatures(uchar a, uchar b)
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
#undef strnXcat

void CLASS process_Sony_0x0116(uchar *buf, ushort len, unsigned id)
{
  int i = 0;

  if (((id == 257) || (id == 262) || (id == 269) || (id == 270)) && (len >= 2))
    i = 1;
  else if ((id >= 273) && (len >= 3))
    i = 2;
  else
    return;

  imgdata.other.BatteryTemperature = (float)(buf[i] - 32) / 1.8f;
}

void CLASS process_Sony_0x2010(uchar *buf, ushort len)
{
  if (!imSony.group2010)
    return;

  if ((imSony.real_iso_offset != 0xffff)    &&
      (len >= (imSony.real_iso_offset + 2)) &&
      (imgdata.other.real_ISO < 0.1f)) {
    uchar s[2];
    s[0] = SonySubstitution[buf[imSony.real_iso_offset]];
    s[1] = SonySubstitution[buf[imSony.real_iso_offset + 1]];
    imgdata.other.real_ISO =
       100.0f * libraw_powf64l(2.0f, (16 - ((float)sget2(s)) / 256.0f));
  }

  if (len >= (imSony.MeteringMode_offset + 2)) {
    imgdata.shootinginfo.MeteringMode =
       SonySubstitution[buf[imSony.MeteringMode_offset]];
    imgdata.shootinginfo.ExposureProgram =
      SonySubstitution[buf[imSony.ExposureProgram_offset]];
  }

  if (len >= (imSony.ReleaseMode2_offset + 2)) {
    imgdata.shootinginfo.DriveMode =
       SonySubstitution[buf[imSony.ReleaseMode2_offset]];
  }

}

void CLASS process_Sony_0x9050(uchar *buf, ushort len, unsigned id)
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
          my_roundf(libraw_powf64l(2.0f, ((float)SonySubstitution[buf[0]] / 8.0 - 1.06f) / 2.0f) * 10.0f) / 10.0f;

    if (buf[1])
      ilm.MinAp4CurFocal =
          my_roundf(libraw_powf64l(2.0f, ((float)SonySubstitution[buf[1]] / 8.0 - 1.06f) / 2.0f) * 10.0f) / 10.0f;
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
    if (buf[0x105] && (ilm.LensMount != LIBRAW_MOUNT_Canon_EF) &&
        (ilm.LensMount != LIBRAW_MOUNT_Sigma_X3F))
      ilm.LensMount = SonySubstitution[buf[0x105]];
    if (buf[0x106])
      ilm.LensFormat = SonySubstitution[buf[0x106]];
  }

  if (ilm.CameraMount == LIBRAW_MOUNT_Sony_E)
  {
    if (len <= 0x108)
      return;
    parseSonyLensType2(SonySubstitution[buf[0x0108]], // LensType2 - Sony lens ids
                       SonySubstitution[buf[0x0107]]);
  }

  if (len <= 0x10a)
    return;
  if ((ilm.LensID == -1) && (ilm.CameraMount == LIBRAW_MOUNT_Minolta_A) &&
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

    else if ((ilm.LensID > 0xEF00) && (ilm.LensID < 0xFFFF) &&
             (ilm.LensID != 0xFF00))
    {
      ilm.AdapterID = 0xEF00;
      ilm.LensID -= ilm.AdapterID;
      ilm.LensMount = LIBRAW_MOUNT_Canon_EF;
    }
  }

  if ((id >= 286) && (id <= 293))
  {
    if (len <= 0x116)
      return;
    // "SLT-A65", "SLT-A77", "NEX-7", "NEX-VG20E",
    // "SLT-A37", "SLT-A57", "NEX-F3", "Lunar"
    parseSonyLensFeatures(SonySubstitution[buf[0x115]], SonySubstitution[buf[0x116]]);
  }
  else if (ilm.CameraMount != LIBRAW_MOUNT_FixedLens)
  {
    if (len <= 0x117)
      return;
    parseSonyLensFeatures(SonySubstitution[buf[0x116]], SonySubstitution[buf[0x117]]);
  }

  if ((id == 347) ||
      (id == 350) ||
      (id == 354) ||
      (id == 357) ||
      (id == 358) ||
      (id == 360) ||
      (id == 362) ||
      (id == 363) ||
      (id == 371))
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
            (b88 << 40) + (b89 << 32) + (b8a << 24) + (b8b << 16) + (b8c << 8) + b8d);
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
  else if ((ilm.CameraMount == LIBRAW_MOUNT_Sony_E) && (id != 288) && (id != 289) && (id != 290))
  {
    if (len <= 0x7f)
      return;
    unsigned b7c = SonySubstitution[buf[0x7c]];
    unsigned b7d = SonySubstitution[buf[0x7d]];
    unsigned b7e = SonySubstitution[buf[0x7e]];
    unsigned b7f = SonySubstitution[buf[0x7f]];
    sprintf(imgdata.shootinginfo.InternalBodySerial, "%04x", (b7c << 24) + (b7d << 16) + (b7e << 8) + b7f);
  }

  if ((imSony.ImageCount3_offset != 0xffff) &&
      (len >= (imSony.ImageCount3_offset + 4)))
  {
    FORC4 s[c] = SonySubstitution[buf[imSony.ImageCount3_offset + c]];
    imSony.ImageCount3 = sget4(s);
  }

  return;
}

void CLASS process_Sony_0x9400(uchar *buf, ushort len, unsigned id)
{

  uchar s[4];
  int c;
  uchar bufx = buf[0];

  if (((bufx == 0x23) || (bufx == 0x24) || (bufx == 0x26)) && (len >= 0x1f))
  { // 0x9400 'c' version

    if ((id == 358) || (id == 362) || (id == 363) || (id == 365) || (id == 366) || (id == 369) || (id == 371))
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

void CLASS process_Sony_0x9402(uchar *buf, ushort len)
{

  if (len < 23)
    return;

  imgdata.shootinginfo.FocusMode = SonySubstitution[buf[0x16]];

  if ((imSony.SonyCameraType == LIBRAW_SONY_SLT) ||
      (imSony.SonyCameraType == LIBRAW_SONY_ILCA))
    return;

  uchar bufx = buf[0x00];
  if ((bufx == 0x05) || (bufx == 0xff) || (buf[0x02] != 0xff))
    return;

  imgdata.other.AmbientTemperature = (float)((short)SonySubstitution[buf[0x04]]);

  return;
}

void CLASS process_Sony_0x9403(uchar *buf, ushort len)
{
  if (len < 6)
    return;
  uchar bufx = SonySubstitution[buf[4]];
  if ((bufx == 0x00) || (bufx == 0x94))
    return;

  imgdata.other.SensorTemperature = (float)((short)SonySubstitution[buf[5]]);

  return;
}

void CLASS process_Sony_0x9406(uchar *buf, ushort len)
{
  if (len < 6)
    return;
  uchar bufx = buf[0];
  if ((bufx != 0x01) && (bufx != 0x08) && (bufx != 0x1b))
    return;
  bufx = buf[2];
  if ((bufx != 0x08) && (bufx != 0x1b))
    return;

  imgdata.other.BatteryTemperature = (float)(SonySubstitution[buf[5]] - 32) / 1.8f;

  return;
}

void CLASS process_Sony_0x940c(uchar *buf, ushort len)
{
  if ((imSony.SonyCameraType != LIBRAW_SONY_ILCE) &&
      (imSony.SonyCameraType != LIBRAW_SONY_NEX))
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
  lid2 = (((ushort)SonySubstitution[buf[0x000a]]) << 8) | ((ushort)SonySubstitution[buf[0x0009]]);
  if ((lid2 > 0) &&
      ((lid2 < 32784)  ||
       (ilm.LensID == 0x1999) ||
       (ilm.LensID == 0xffff)))
    parseSonyLensType2(SonySubstitution[buf[0x000a]], // LensType2 - Sony lens ids
                       SonySubstitution[buf[0x0009]]);
  return;
}

void CLASS process_Sony_0x940e(uchar *buf, ushort len, unsigned id)
{
  if (((imSony.SonyCameraType != LIBRAW_SONY_SLT) &&
       (imSony.SonyCameraType != LIBRAW_SONY_ILCA)) ||
      (id == 280) ||
      (id == 281) ||
      (id == 285) ||
      (len < 3))
    return;

  imSony.AFType = SonySubstitution[buf[0x02]];

  if (imSony.SonyCameraType == LIBRAW_SONY_ILCA) {
    if (len >= 0x06) {
      imgdata.shootinginfo.FocusMode = SonySubstitution[buf[0x05]];
    }
    if (len >= 0x0051) {
      imSony.AFMicroAdjValue = SonySubstitution[buf[0x0050]];
    }

  } else {
    if (len >= 0x0c) {
      imgdata.shootinginfo.FocusMode = SonySubstitution[buf[0x0b]];
    }
    if (len >= 0x017e) {
      imSony.AFMicroAdjValue = SonySubstitution[buf[0x017d]];
    }
  }

  if (imSony.AFMicroAdjValue != 0)
    imSony.AFMicroAdjOn = 1;
}

void CLASS parseSonyMakernotes(int base, unsigned tag, unsigned type, unsigned len, unsigned dng_writer,
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

  ushort lid, a, b, c, d;
  uchar *table_buf;
  uchar uc;
  uchar s[2];
  int LensDataValid = 0;
  unsigned uitemp;

  if (tag == 0xb001) { // Sony ModelID
    unique_id = get2();
    setSonyBodyFeatures(unique_id);

    if (table_buf_0x0116_len) {
      process_Sony_0x0116(table_buf_0x0116, table_buf_0x0116_len, unique_id);
      free(table_buf_0x0116);
      table_buf_0x0116_len = 0;
    }

    if (table_buf_0x2010_len) {
      process_Sony_0x2010(table_buf_0x2010, table_buf_0x2010_len);
      free(table_buf_0x2010);
      table_buf_0x2010_len = 0;
    }

    if (table_buf_0x9050_len) {
      process_Sony_0x9050(table_buf_0x9050, table_buf_0x9050_len, unique_id);
      free(table_buf_0x9050);
      table_buf_0x9050_len = 0;
    }

    if (table_buf_0x9400_len) {
      process_Sony_0x9400(table_buf_0x9400, table_buf_0x9400_len, unique_id);
      free(table_buf_0x9400);
      table_buf_0x9400_len = 0;
    }

    if (table_buf_0x9402_len) {
      process_Sony_0x9402(table_buf_0x9402, table_buf_0x9402_len);
      free(table_buf_0x9402);
      table_buf_0x9402_len = 0;
    }

    if (table_buf_0x9403_len) {
      process_Sony_0x9403(table_buf_0x9403, table_buf_0x9403_len);
      free(table_buf_0x9403);
      table_buf_0x9403_len = 0;
    }

    if (table_buf_0x9406_len) {
      process_Sony_0x9406(table_buf_0x9406, table_buf_0x9406_len);
      free(table_buf_0x9406);
      table_buf_0x9406_len = 0;
    }

    if (table_buf_0x940c_len) {
      process_Sony_0x940c(table_buf_0x940c, table_buf_0x940c_len);
      free(table_buf_0x940c);
      table_buf_0x940c_len = 0;
    }

    if (table_buf_0x940e_len) {
      process_Sony_0x940e(table_buf_0x940e, table_buf_0x940e_len, unique_id);
      free(table_buf_0x940e);
      table_buf_0x940e_len = 0;
    }

  } else if (tag == 0xb000) {
    FORC4 imSony.FileFormat = imSony.FileFormat*10 + fgetc(ifp);

  } else if (tag == 0xb026) {
    uitemp = get4();
    if (uitemp != 0xffffffff)
      imgdata.shootinginfo.ImageStabilization = uitemp;

  } else if (((tag == 0x0001)  ||  // Minolta CameraSettings, big endian
              (tag == 0x0003)) &&
             (len >= 196)) {
    table_buf = (uchar *)malloc(len);
    fread(table_buf, len, 1, ifp);

    lid = 0x01<<2;
      imgdata.shootinginfo.ExposureMode =
          (unsigned)table_buf[lid] << 24 | (unsigned)table_buf[lid + 1] << 16 |
          (unsigned)table_buf[lid + 2] << 8 | (unsigned)table_buf[lid + 3];

    lid = 0x06<<2;
      imgdata.shootinginfo.DriveMode =
          (unsigned)table_buf[lid] << 24 | (unsigned)table_buf[lid + 1] << 16 |
          (unsigned)table_buf[lid + 2] << 8 | (unsigned)table_buf[lid + 3];

    lid = 0x07<<2;
      imgdata.shootinginfo.MeteringMode =
          (unsigned)table_buf[lid] << 24 | (unsigned)table_buf[lid + 1] << 16 |
          (unsigned)table_buf[lid + 2] << 8 | (unsigned)table_buf[lid + 3];

    lid = 0x25<<2;
      imSony.MinoltaCamID =
          (unsigned)table_buf[lid] << 24 | (unsigned)table_buf[lid + 1] << 16 |
          (unsigned)table_buf[lid + 2] << 8 | (unsigned)table_buf[lid + 3];
      if (imSony.MinoltaCamID != -1)
        ilm.CamID = imSony.MinoltaCamID;

    lid = 0x30<<2;
      imgdata.shootinginfo.FocusMode =
          (unsigned)table_buf[lid] << 24 | (unsigned)table_buf[lid + 1] << 16 |
          (unsigned)table_buf[lid + 2] << 8 | (unsigned)table_buf[lid + 3];

    free(table_buf);

  } else if ((tag == 0x0004) && // Minolta CameraSettings7D, big endian
             (len >= 227)) {
    table_buf = (uchar *)malloc(len);
    fread(table_buf, len, 1, ifp);

    lid = 0x0;
      imgdata.shootinginfo.ExposureMode =
          (ushort)table_buf[lid] << 8 | (ushort)table_buf[lid + 1];

    lid = 0x0e<<1;
      imgdata.shootinginfo.FocusMode =
          (ushort)table_buf[lid] << 8 | (ushort)table_buf[lid + 1];

    lid = 0x10<<1;
      imgdata.shootinginfo.AFPoint =
          (ushort)table_buf[lid] << 8 | (ushort)table_buf[lid + 1];

    lid = 0x71<<1;
      imgdata.shootinginfo.ImageStabilization =
          (ushort)table_buf[lid] << 8 | (ushort)table_buf[lid + 1];

    free(table_buf);

  } else if ((tag == 0x0010) && // CameraInfo
           strncasecmp(model, "DSLR-A100", 9) &&
           !strncasecmp(make, "SONY", 4)      &&
           ((len == 368)  || // a700                         : CameraInfo
            (len == 5478) || // a850, a900                   : CameraInfo
            (len == 5506) || // a200, a300, a350             : CameraInfo2
            (len == 6118) || // a230, a290, a330, a380, a390 : CameraInfo2
            (len == 15360))  // a450, a500, a550, a560, a580 : CameraInfo3
                             // a33, a35, a55
                             // NEX-3, NEX-5, NEX-5C, NEX-C3, NEX-VG10E

            ) {
    table_buf = (uchar *)malloc(len);
    fread(table_buf, len, 1, ifp);
    if (memcmp(table_buf, "\xff\xff\xff\xff\xff\xff\xff\xff", 8) &&
        memcmp(table_buf, "\x00\x00\x00\x00\x00\x00\x00\x00", 8)) {
      LensDataValid = 1;
    }
    switch (len) {
    case 368:  // a700: CameraInfo
    case 5478: // a850, a900: CameraInfo
      if ((!dng_writer) ||
          (saneSonyCameraInfo(table_buf[0], table_buf[3], table_buf[2], table_buf[5], table_buf[4], table_buf[7])))
      {
        if (LensDataValid) {
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

        if (len == 5478) {
          imSony.AFMicroAdjValue = table_buf[304] - 20;
          imSony.AFMicroAdjOn = (((table_buf[305] & 0x80) == 0x80) ? 1 : 0);
          imSony.AFMicroAdjRegisteredLenses = table_buf[305] & 0x7f;
        }
      }
      break;
    default:
      // CameraInfo2 & 3
      if ((!dng_writer) ||
          (saneSonyCameraInfo(table_buf[1], table_buf[2], table_buf[3], table_buf[4], table_buf[5], table_buf[6])))
      {
        if ((LensDataValid) && strncasecmp(model, "NEX-5C", 6)) {
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
            !strncasecmp(model, "DSLR-A550", 9)) {
          imSony.AFPointSelected = table_buf[0x14];
          imgdata.shootinginfo.FocusMode = table_buf[0x15];
          imgdata.shootinginfo.AFPoint = (ushort)table_buf[0x18];

        } else if (!strncasecmp(model, "SLT-", 4) ||
            !strncasecmp(model, "DSLR-A560", 9) ||
            !strncasecmp(model, "DSLR-A580", 9)) {
          imSony.AFPointSelected = table_buf[0x1c];
          imgdata.shootinginfo.FocusMode = table_buf[0x1d];
          imgdata.shootinginfo.AFPoint = (ushort)table_buf[0x20];
        }

      }
    }
    free(table_buf);

  } else if ((!dng_writer) &&
            ((tag == 0x0020) ||
             (tag == 0xb0280020))) {
    if (!strncasecmp(model, "DSLR-A100", 9)) { // WBInfoA100
      fseek(ifp, 0x49dc, SEEK_CUR);
      stmread(imgdata.shootinginfo.InternalBodySerial, 13, ifp);

    } else if ((len == 19154) || // a200 a230 a290 a300 a330 a350 a380 a390 : FocusInfo
               (len == 19148)) { // a700 a850 a900                          : FocusInfo
      table_buf = (uchar *)malloc(128);
      fread(table_buf, 128, 1, ifp);
      imgdata.shootinginfo.DriveMode = table_buf[14];
      imgdata.shootinginfo.ExposureProgram = table_buf[63];
      free(table_buf);

    } else if (len == 20480) // a450 a500 a550 a560 a580 a33 a35 a55 : MoreInfo
                             // NEX-3 NEX-5 NEX-C3 NEX-VG10E         : MoreInfo
    {
      a = get2();
      b = get2();
      c = get2();
      d = get2();
      if ((a) && (c == 1)) {
        fseek(ifp, d-8, SEEK_CUR);
        table_buf = (uchar *)malloc(256);
        fread(table_buf, 256, 1, ifp);
        imgdata.shootinginfo.DriveMode = table_buf[1];
        imgdata.shootinginfo.ExposureProgram = table_buf[2];
        imgdata.shootinginfo.MeteringMode = table_buf[3];
        if (strncasecmp(model, "DSLR-A450", 9) &&
            strncasecmp(model, "DSLR-A500", 9) &&
            strncasecmp(model, "DSLR-A550", 9))
          imgdata.shootinginfo.FocusMode = table_buf[0x13];
        else
          imgdata.shootinginfo.FocusMode = table_buf[0x2c];
        free(table_buf);
      }
    }

  } else if (tag == 0x0102) {
    imSony.Quality = get4();

  } else if (tag == 0x0104) {
    imgdata.other.FlashEC = getreal(type);

  } else if (tag == 0x0105) { // Teleconverter
    ilm.TeleconverterID = get4();

  } else if (tag == 0x0107) {
    uitemp = get4();
    if (uitemp == 1) imgdata.shootinginfo.ImageStabilization = 0;
    else if (uitemp == 5) imgdata.shootinginfo.ImageStabilization = 1;
    else imgdata.shootinginfo.ImageStabilization = uitemp;

  } else if ((tag == 0xb0280088) && (dng_writer == nonDNG)) {
    thumb_offset = get4() + base;

  } else if ((tag == 0xb0280089) && (dng_writer == nonDNG)) {
    thumb_length = get4();

  } else if (((tag == 0x0114) || // CameraSettings
            (tag == 0xb0280114)) &&
           (len < 256000)) {
    table_buf = (uchar *)malloc(len);
    fread(table_buf, len, 1, ifp);
    switch (len) {
    case 260:  // Sony a100, big endian
      imgdata.shootinginfo.ExposureMode =
          ((ushort)table_buf[0]) << 8 | ((ushort)table_buf[1]);
      lid = 0x0a<<1;
      imgdata.shootinginfo.DriveMode =
          ((ushort)table_buf[lid]) << 8 | ((ushort)table_buf[lid+1]);
      lid = 0x0c<<1;
      imgdata.shootinginfo.FocusMode =
          ((ushort)table_buf[lid]) << 8 | ((ushort)table_buf[lid+1]);
      lid = 0x0d<<1;
      imSony.AFPointSelected = table_buf[lid+1];
      lid = 0x0e<<1;
      imSony.AFAreaModeSetting = table_buf[lid+1];
      lid = 0x12<<1;
      imgdata.shootinginfo.MeteringMode =
          ((ushort)table_buf[lid]) << 8 | ((ushort)table_buf[lid+1]);
      break;
    case 448:  // Minolta "DYNAX 5D" / "MAXXUM 5D" / "ALPHA SWEET", big endian
      lid = 0x0a<<1;
      imgdata.shootinginfo.ExposureMode =
          ((ushort)table_buf[lid]) << 8 | ((ushort)table_buf[lid+1]);
      lid = 0x25<<1;
      imgdata.shootinginfo.MeteringMode =
          ((ushort)table_buf[lid]) << 8 | ((ushort)table_buf[lid+1]);
      lid = 0xbd<<1;
      imgdata.shootinginfo.ImageStabilization =
          ((ushort)table_buf[lid]) << 8 | ((ushort)table_buf[lid+1]);
      break;
    case 280: // a200 a300 a350 a700
    case 364: // a850 a900
      // CameraSettings and CameraSettings2 are big endian
      if (table_buf[2] | table_buf[3]) {
        lid = (((ushort)table_buf[2]) << 8) | ((ushort)table_buf[3]);
        ilm.CurAp = libraw_powf64l(2.0f, ((float)lid / 8.0f - 1.0f) / 2.0f);
      }
      lid = 0x04<<1;
      imgdata.shootinginfo.DriveMode = table_buf[lid+1];
      lid = 0x4d<<1;
      imgdata.shootinginfo.FocusMode = ((ushort)table_buf[lid]) << 8 | ((ushort)table_buf[lid+1]);
      break;
    case 332: // a230 a290 a330 a380 a390
      // CameraSettings and CameraSettings2 are big endian
      if (table_buf[2] | table_buf[3]) {
        lid = (((ushort)table_buf[2]) << 8) | ((ushort)table_buf[3]);
        ilm.CurAp = libraw_powf64l(2.0f, ((float)lid / 8.0f - 1.0f) / 2.0f);
      }
      lid = 0x4d<<1;
      imgdata.shootinginfo.FocusMode = ((ushort)table_buf[lid]) << 8 | ((ushort)table_buf[lid+1]);
      lid = 0x7e<<1;
      imgdata.shootinginfo.DriveMode = table_buf[lid+1];
      break;
    case 1536: // a560 a580 a33 a35 a55 NEX-3 NEX-5 NEX-5C NEX-C3 NEX-VG10E
    case 2048: // a450 a500 a550
      // CameraSettings3 are little endian
      imgdata.shootinginfo.DriveMode = table_buf[0x34];
      parseSonyLensType2(table_buf[1016], table_buf[1015]);
      if (ilm.LensMount != LIBRAW_MOUNT_Canon_EF) {
        switch (table_buf[153]) {
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

  } else if ((tag == 0x3000) && (len < 256000)) {
    table_buf = (uchar *)malloc(len);
    fread(table_buf, len, 1, ifp);
    for (int i = 0; i < 20; i++)
      imSony.SonyDateTime[i] = table_buf[6 + i];
    free(table_buf);

  } else if (tag == 0x0116 && len < 256000) {
    table_buf_0x0116 = (uchar *)malloc(len);
    table_buf_0x0116_len = len;
    fread(table_buf_0x0116, len, 1, ifp);
    if (ilm.CamID) {
      process_Sony_0x0116(table_buf_0x0116, table_buf_0x0116_len, ilm.CamID);
      free(table_buf_0x0116);
      table_buf_0x0116_len = 0;
    }

  } else if (tag == 0x2008) {
    imSony.LongExposureNoiseReduction = get4();

  } else if (tag == 0x2009) {
    imSony.HighISONoiseReduction = get2();

  } else if (tag == 0x200a) {
    imSony.HDR[0] = get2();
    imSony.HDR[1] = get2();

  } else if (tag == 0x2010 && len < 256000) {
    table_buf_0x2010 = (uchar *)malloc(len);
    table_buf_0x2010_len = len;
    fread(table_buf_0x2010, len, 1, ifp);
    if (ilm.CamID) {
      process_Sony_0x2010(table_buf_0x2010, table_buf_0x2010_len);
      free(table_buf_0x2010);
      table_buf_0x2010_len = 0;
    }

  } else if (tag == 0x201a) {
    imSony.ElectronicFrontCurtainShutter = get4();

  } else if (tag == 0x201b) {
    if ((imSony.SonyCameraType != LIBRAW_SONY_DSC) ||
        (ilm.CamID == 365) ||
        (ilm.CamID == 366) ||
        (ilm.CamID == 369)) {
      fread(&uc, 1, 1, ifp);
      imgdata.shootinginfo.FocusMode = (short)uc;
    }

  } else if (tag == 0x201c) {
    if ((imSony.SonyCameraType != LIBRAW_SONY_DSC) ||
        (ilm.CamID == 365) ||
        (ilm.CamID == 366) ||
        (ilm.CamID == 369)) {
      imSony.AFAreaModeSetting = fgetc(ifp);
    }

  } else if (tag == 0x201d) {
    if (((imSony.AFAreaModeSetting == 3) &&
        ((imSony.SonyCameraType == LIBRAW_SONY_ILCE) ||
         (imSony.SonyCameraType == LIBRAW_SONY_NEX)  ||
         (ilm.CamID == 365) ||
         (ilm.CamID == 366) ||
         (ilm.CamID == 369))) ||
        ((imSony.AFAreaModeSetting == 4) &&
         (imSony.SonyCameraType == LIBRAW_SONY_ILCA))) {
      imSony.FlexibleSpotPosition[0] = get2();
      imSony.FlexibleSpotPosition[1] = get2();
    }

  } else if (tag == 0x201e) {
    if (imSony.SonyCameraType != LIBRAW_SONY_DSC) {
      imSony.AFPointSelected = fgetc(ifp);
    }

  } else if (tag == 0x2020) {
    if (imSony.SonyCameraType != LIBRAW_SONY_DSC) {
      fread(imSony.AFPointsUsed, 1, 10, ifp);
    }

  } else if (tag == 0x2021) {
    if ((imSony.SonyCameraType != LIBRAW_SONY_DSC) ||
        (ilm.CamID == 365) ||
        (ilm.CamID == 366) ||
        (ilm.CamID == 369)) {
      imSony.AFTracking = fgetc(ifp);
    }

  } else if (tag == 0x2027) {
    FORC4 imSony.FocusLocation[c] = get2();

  } else if (tag == 0x2028) {
    if (get2()) {
      imSony.VariableLowPassFilter = get2();
    }

  } else if (tag == 0x2029) {
    imSony.RAWFileType = get2();

  } else if (tag == 0x202c) {
    imSony.MeteringMode2 = get2();

  } else if (tag == 0x202f) {
    imSony.PixelShiftGroupID = get4();
    imSony.PixelShiftGroupPrefix = imSony.PixelShiftGroupID >> 22;
    imSony.PixelShiftGroupID =
      ((imSony.PixelShiftGroupID >> 17) & (unsigned) 0x1f) * (unsigned) 1000000 +
      ((imSony.PixelShiftGroupID >> 12) & (unsigned) 0x1f) * (unsigned) 10000 +
      ((imSony.PixelShiftGroupID >>  6) & (unsigned) 0x3f) * (unsigned) 100 +
      (imSony.PixelShiftGroupID         & (unsigned) 0x3f);

    imSony.numInPixelShiftGroup = fgetc(ifp);
    imSony.nShotsInPixelShiftGroup = fgetc(ifp);

  } else if (tag == 0x9050 && len < 256000) { // little endian
    table_buf_0x9050 = (uchar *)malloc(len);
    table_buf_0x9050_len = len;
    fread(table_buf_0x9050, len, 1, ifp);

    if (ilm.CamID) {
      process_Sony_0x9050(table_buf_0x9050, table_buf_0x9050_len, ilm.CamID);
      free(table_buf_0x9050);
      table_buf_0x9050_len = 0;
    }

  } else if (tag == 0x9400 && len < 256000) {
    table_buf_0x9400 = (uchar *)malloc(len);
    table_buf_0x9400_len = len;
    fread(table_buf_0x9400, len, 1, ifp);
    if (ilm.CamID) {
      process_Sony_0x9400(table_buf_0x9400, table_buf_0x9400_len, unique_id);
      free(table_buf_0x9400);
      table_buf_0x9400_len = 0;
    }

  } else if (tag == 0x9402 && len < 256000) {
    table_buf_0x9402 = (uchar *)malloc(len);
    table_buf_0x9402_len = len;
    fread(table_buf_0x9402, len, 1, ifp);
    if (ilm.CamID) {
      process_Sony_0x9402(table_buf_0x9402, table_buf_0x9402_len);
      free(table_buf_0x9402);
      table_buf_0x9402_len = 0;
    }

  } else if (tag == 0x9403 && len < 256000) {
    table_buf_0x9403 = (uchar *)malloc(len);
    table_buf_0x9403_len = len;
    fread(table_buf_0x9403, len, 1, ifp);
    if (ilm.CamID) {
      process_Sony_0x9403(table_buf_0x9403, table_buf_0x9403_len);
      free(table_buf_0x9403);
      table_buf_0x9403_len = 0;
    }

  } else if ((tag == 0x9405) && (len < 256000) && (len > 0x64)) {
    table_buf = (uchar *)malloc(len);
    fread(table_buf, len, 1, ifp);
    uc = table_buf[0x0];
    if (imgdata.other.real_ISO < 0.1f) {
      if ((uc == 0x25) || (uc == 0x3a) || (uc == 0x76) || (uc == 0x7e) ||
          (uc == 0x8b) || (uc == 0x9a) || (uc == 0xb3) || (uc == 0xe1)) {
        s[0] = SonySubstitution[table_buf[0x04]];
        s[1] = SonySubstitution[table_buf[0x05]];
        imgdata.other.real_ISO = 100.0f * libraw_powf64l(2.0f, (16 - ((float)sget2(s)) / 256.0f));
      }
    }
    free(table_buf);

  } else if (tag == 0x9406 && len < 256000) {
    table_buf_0x9406 = (uchar *)malloc(len);
    table_buf_0x9406_len = len;
    fread(table_buf_0x9406, len, 1, ifp);
    if (ilm.CamID) {
      process_Sony_0x9406(table_buf_0x9406, table_buf_0x9406_len);
      free(table_buf_0x9406);
      table_buf_0x9406_len = 0;
    }

  } else if (tag == 0x940c && len < 256000) {
    table_buf_0x940c = (uchar *)malloc(len);
    table_buf_0x940c_len = len;
    fread(table_buf_0x940c, len, 1, ifp);
    if (ilm.CamID) {
      process_Sony_0x940c(table_buf_0x940c, table_buf_0x940c_len);
      free(table_buf_0x940c);
      table_buf_0x940c_len = 0;
    }

  } else if (tag == 0x940e && len < 256000) {
    table_buf_0x940e = (uchar *)malloc(len);
    table_buf_0x940e_len = len;
    fread(table_buf_0x940e, len, 1, ifp);
    if (ilm.CamID) {
      process_Sony_0x940e(table_buf_0x940e, table_buf_0x940e_len, ilm.CamID);
      free(table_buf_0x940e);
      table_buf_0x940e_len = 0;
    }

  } else if (((tag == 0xb027) || (tag == 0x010c)) && (ilm.LensID == -1)) {
    ilm.LensID = get4();
    if ((ilm.LensID > 0x4900) && (ilm.LensID <= 0x5900)) {
      ilm.AdapterID = 0x4900;
      ilm.LensID -= ilm.AdapterID;
      ilm.LensMount = LIBRAW_MOUNT_Sigma_X3F;
      strcpy(ilm.Adapter, "MC-11");
    }

    else if ((ilm.LensID > 0xef00) && (ilm.LensID < 0xffff) &&
             (ilm.LensID != 0xFF00)) {
      ilm.AdapterID = 0xEF00;
      ilm.LensID -= ilm.AdapterID;
      ilm.LensMount = LIBRAW_MOUNT_Canon_EF;
    }
    if (tag == 0x010c)
      ilm.CameraMount = LIBRAW_MOUNT_Minolta_A;

  } else if (tag == 0xb02a && len < 256000) { // Sony LensSpec
    table_buf = (uchar *)malloc(len);
    fread(table_buf, len, 1, ifp);
    if ((!dng_writer) ||
        (saneSonyCameraInfo(table_buf[1], table_buf[2], table_buf[3], table_buf[4], table_buf[5], table_buf[6])))
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

  } else if ((tag == 0xb02b) && !imgdata.sizes.raw_crop.cwidth && (len == 2)) {
    imgdata.sizes.raw_crop.cheight = get4();
    imgdata.sizes.raw_crop.cwidth = get4();

  } else if (tag == 0xb041) {
    imgdata.shootinginfo.ExposureMode = get2();
  }

// MetaVersion: (unique_id >= 286)

}

void CLASS parseSonySR2 (uchar *cbuf_SR2, unsigned SR2SubIFDOffset, unsigned SR2SubIFDLength, unsigned dng_writer)
{
  unsigned c;
  unsigned entries, tag, type, len;
  unsigned icbuf_SR2;
  int ival;
  int TagProcessed;
  float num;
  int i;
  entries = sget2(cbuf_SR2);
  if (entries > 1000) return;
  icbuf_SR2 = 2;
  while (entries--) {
    tag = sget2(cbuf_SR2 + icbuf_SR2);
    icbuf_SR2 += 2;
    type = sget2(cbuf_SR2 + icbuf_SR2);
    icbuf_SR2 += 2;
    len = sget4(cbuf_SR2 + icbuf_SR2);
    icbuf_SR2 += 4;

    if (len * ("11124811248484"[type < 14 ? type : 0] - '0') > 4) {
      ival = sget4(cbuf_SR2 + icbuf_SR2) - SR2SubIFDOffset;
    } else {
      ival = icbuf_SR2;
    }
    if(ival > SR2SubIFDLength) // points out of orig. buffer size
      break; // END processing. Generally we should check against SR2SubIFDLength minus 6 of 8, depending on tag, but we allocated extra 1024b for buffer, so this does not matter

    icbuf_SR2 += 4;
    TagProcessed = 0;

    if (dng_writer == nonDNG) {
      switch (tag) {
      case 0x7300:
        for (c = 0; c < 4 && c < len; c++)
          cblack[c] = sget2(cbuf_SR2 + ival + 2 * c);
        TagProcessed = 1;
        break;
      case 0x7303:
        FORC4 cam_mul[c ^ (c < 2)] = sget2(cbuf_SR2 + ival + 2 * c);
        TagProcessed = 1;
        break;
      case 0x7310:
        FORC4 cblack[c ^ c >> 1] = sget2(cbuf_SR2 + ival + 2 * c);
        i = cblack[3];
        FORC3 if (i > cblack[c]) i = cblack[c];
        FORC4 cblack[c] -= i;
        black = i;
        TagProcessed = 1;
        break;
      case 0x7313:
        FORC4 cam_mul[c ^ (c >> 1)] = sget2(cbuf_SR2 + ival + 2 * c);
        TagProcessed = 1;
        break;
      case 0x74a0:
        c = sget4(cbuf_SR2 + ival+4);
        if (c) ilm.MaxAp4MaxFocal = ((float)sget4(cbuf_SR2 + ival)) / ((float)c);
        TagProcessed = 1;
        break;
      case 0x74a1:
        c = sget4(cbuf_SR2 + ival+4);
        if (c) ilm.MaxAp4MinFocal = ((float)sget4(cbuf_SR2 + ival)) / ((float)c);
        TagProcessed = 1;
        break;
      case 0x74a2:
        c = sget4(cbuf_SR2 + ival+4);
        if (c) ilm.MaxFocal = ((float)sget4(cbuf_SR2 + ival)) / ((float)c);
        TagProcessed = 1;
        break;
      case 0x74a3:
        c = sget4(cbuf_SR2 + ival+4);
        if (c) ilm.MinFocal = ((float)sget4(cbuf_SR2 + ival)) / ((float)c);
        TagProcessed = 1;
        break;
      case 0x7800:
        for (i = 0; i < 3; i++) {
          num = 0.0;
          for (c = 0; c < 3; c++) {
            imgdata.color.ccm[i][c] = (float)((short)sget2(cbuf_SR2 + ival + 2 * (i*3+c)));
            num += imgdata.color.ccm[i][c];
          }
          if (num > 0.01)
            FORC3 imgdata.color.ccm[i][c] = imgdata.color.ccm[i][c] / num;
        }
        TagProcessed = 1;
        break;
      case 0x787f:
        if (len == 3) {
          FORC3 imgdata.color.linear_max[c] = sget2(cbuf_SR2 + ival + 2 * c);
          imgdata.color.linear_max[3] = imgdata.color.linear_max[1];
        } else if (len == 1) {
          imgdata.color.linear_max[0] =
            imgdata.color.linear_max[1] =
            imgdata.color.linear_max[2] =
            imgdata.color.linear_max[3] = sget2(cbuf_SR2 + ival);
        }
        TagProcessed = 1;
        break;
      }
    }

    if (!TagProcessed) {
      switch (tag) {
      case 0x7302:
        FORC4 icWBC[LIBRAW_WBI_Auto][c ^ (c < 2)] = sget2(cbuf_SR2 + ival + 2 * c);
        break;
      case 0x7312:
        FORC4 icWBC[LIBRAW_WBI_Auto][c ^ (c >> 1)] = sget2(cbuf_SR2 + ival + 2 * c);
        break;
      case 0x7480:
      case 0x7820:
        FORC3 icWBC[LIBRAW_WBI_Daylight][c] = sget2(cbuf_SR2 + ival + 2 * c);
        icWBC[LIBRAW_WBI_Daylight][3] = icWBC[LIBRAW_WBI_Daylight][1];
        break;
      case 0x7481:
      case 0x7821:
        FORC3 icWBC[LIBRAW_WBI_Cloudy][c] = sget2(cbuf_SR2 + ival + 2 * c);
        icWBC[LIBRAW_WBI_Cloudy][3] = icWBC[LIBRAW_WBI_Cloudy][1];
        break;
      case 0x7482:
      case 0x7822:
        FORC3 icWBC[LIBRAW_WBI_Tungsten][c] = sget2(cbuf_SR2 + ival + 2 * c);
        icWBC[LIBRAW_WBI_Tungsten][3] = icWBC[LIBRAW_WBI_Tungsten][1];
        break;
      case 0x7483:
      case 0x7823:
        FORC3 icWBC[LIBRAW_WBI_Flash][c] = sget2(cbuf_SR2 + ival + 2 * c);
        icWBC[LIBRAW_WBI_Flash][3] = icWBC[LIBRAW_WBI_Flash][1];
        break;
      case 0x7484:
      case 0x7824:
        icWBCTC[0][0] = 4500;
        FORC3 icWBCTC[0][c + 1] = sget2(cbuf_SR2 + ival + 2 * c);
        icWBCTC[0][4] = icWBCTC[0][2];
        break;
      case 0x7486:
        FORC3 icWBC[LIBRAW_WBI_Fluorescent][c] = sget2(cbuf_SR2 + ival + 2 * c);
        icWBC[LIBRAW_WBI_Fluorescent][3] = icWBC[LIBRAW_WBI_Fluorescent][1];
        break;
      case 0x7825:
        FORC3 icWBC[LIBRAW_WBI_Shade][c] = sget2(cbuf_SR2 + ival + 2 * c);
        icWBC[LIBRAW_WBI_Shade][3] = icWBC[LIBRAW_WBI_Shade][1];
        break;
      case 0x7826:
        FORC3 icWBC[LIBRAW_WBI_FL_W][c] = sget2(cbuf_SR2 + ival + 2 * c);
        icWBC[LIBRAW_WBI_FL_W][3] = icWBC[LIBRAW_WBI_FL_W][1];
        break;
      case 0x7827:
        FORC3 icWBC[LIBRAW_WBI_FL_N][c] = sget2(cbuf_SR2 + ival + 2 * c);
        icWBC[LIBRAW_WBI_FL_N][3] = icWBC[LIBRAW_WBI_FL_N][1];
        break;
      case 0x7828:
        FORC3 icWBC[LIBRAW_WBI_FL_D][c] = sget2(cbuf_SR2 + ival + 2 * c);
        icWBC[LIBRAW_WBI_FL_D][3] = icWBC[LIBRAW_WBI_FL_D][1];
        break;
      case 0x7829:
        FORC3 icWBC[LIBRAW_WBI_FL_L][c] = sget2(cbuf_SR2 + ival + 2 * c);
        icWBC[LIBRAW_WBI_FL_L][3] = icWBC[LIBRAW_WBI_FL_L][1];
        break;
      case 0x782a:
        icWBCTC[1][0] = 8500;
        FORC3 icWBCTC[1][c + 1] = sget2(cbuf_SR2 + ival + 2 * c);
        icWBCTC[1][4] = icWBCTC[1][2];
        break;
      case 0x782b:
        icWBCTC[2][0] = 6000;
        FORC3 icWBCTC[2][c + 1] = sget2(cbuf_SR2 + ival + 2 * c);
        icWBCTC[2][4] = icWBCTC[2][2];
        break;
      case 0x782c:
        icWBCTC[3][0] = 3200;
        FORC3 icWBC[LIBRAW_WBI_StudioTungsten][c] = icWBCTC[3][c + 1] =
            sget2(cbuf_SR2 + ival + 2 * c);
        icWBC[LIBRAW_WBI_StudioTungsten][3] = icWBCTC[3][4] = icWBCTC[3][2];;
        break;
      case 0x782d:
        icWBCTC[4][0] = 2500;
        FORC3 icWBCTC[4][c + 1] = sget2(cbuf_SR2 + ival + 2 * c);
        icWBCTC[4][4] = icWBCTC[4][2];
        break;
      }
    }
  }
}

void CLASS parseSonySRF (unsigned len)
{

  if ((len > 0xfffff) || (len == 0)) return;

  INT64 save = ftell (ifp);
  INT64 offset = 0x0310c0 - save; /* for non-DNG this value normally is 0x8ddc */
  if (len < offset) return;
  INT64 decrypt_len = offset >>2; /* master key offset value is the next un-encrypted metadata field after SRF0 */

  unsigned i;
  unsigned MasterKey, SRF2Key, RawDataKey;
  INT64 srf_offset;
  uchar *srf_buf;
  short entries;
  unsigned tag_id, tag_type, tag_datalen, tag_val;

  srf_buf = (uchar *)malloc(len);
  fread (srf_buf, len, 1, ifp);

  offset += srf_buf[offset] << 2;
  if ((len-3) < offset) goto restore_after_parseSonySRF;

/* master key is stored in big endian */
  MasterKey = ((unsigned)srf_buf[offset]   << 24) |
              ((unsigned)srf_buf[offset+1] << 16) |
              ((unsigned)srf_buf[offset+2] << 8)  |
              (unsigned)srf_buf[offset+3];


/* skip SRF0 */
  srf_offset = 0;
  entries = sget2(srf_buf+srf_offset);
  if (entries > 1000) goto restore_after_parseSonySRF;
  offset = srf_offset + 2;
  srf_offset = sget4(srf_buf + offset + 12*entries) - save; /* SRF0 ends with SRF1 abs. position */

/* get SRF1, it has fixed 40 bytes length and contains keys to decode metadata and raw data */
  sony_decrypt((unsigned *)(srf_buf+srf_offset), decrypt_len - srf_offset / 4, 1, MasterKey);
  entries = sget2(srf_buf+srf_offset);
  if (entries > 1000) goto restore_after_parseSonySRF;
  offset = srf_offset + 2;

  while (entries--) {
    tag_id = sget2(srf_buf+offset);
    tag_type = sget2(srf_buf+offset+2);
    tag_datalen = sget4(srf_buf+offset+4);
    tag_val = sget4(srf_buf+offset+8);
    if (tag_id == 0x0000) SRF2Key = tag_val;
    else if (tag_id == 0x0001) RawDataKey = tag_val;
    offset += 12;
  }

/* get SRF2 */
  srf_offset = sget4(srf_buf+offset) - save; /* SRFn ends with SRFn+1 position */
  sony_decrypt((unsigned *)(srf_buf+srf_offset), decrypt_len - srf_offset / 4, 1, SRF2Key);
  entries = sget2(srf_buf+srf_offset);
  if (entries > 1000) goto restore_after_parseSonySRF;
  offset = srf_offset + 2;
  while (entries--) {
    tag_id = sget2(srf_buf+offset);
    tag_type = sget2(srf_buf+offset+2);
    tag_datalen = sget4(srf_buf+offset+4);
    tag_val = sget4(srf_buf+offset+8);
    switch (tag_id) {
/*
0x0002	SRF6Offset
0x0003	SRFDataOffset (?)
0x0004	RawDataOffset
0x0005	RawDataLength
*/
    case 0x0043:
      i = sget4(srf_buf+tag_val-save+4);
      if (i) ilm.MaxAp4MaxFocal  = ((float)sget4(srf_buf+tag_val-save)) / ((float)i);
      break;
    case 0x0044:
      i = sget4(srf_buf+tag_val-save+4);
      if (i) ilm.MaxAp4MinFocal  = ((float)sget4(srf_buf+tag_val-save)) / ((float)i);
      break;
    case 0x0045:
      i = sget4(srf_buf+tag_val-save+4);
      if (i) ilm.MinFocal = ((float)sget4(srf_buf+tag_val-save)) / ((float)i);
      break;
    case 0x0046:
      i = sget4(srf_buf+tag_val-save+4);
      if (i) ilm.MaxFocal = ((float)sget4(srf_buf+tag_val-save)) / ((float)i);
      break;

    case 0x00c0:
      icWBC[LIBRAW_WBI_Daylight][0] = tag_val;
      break;
    case 0x00c1:
      icWBC[LIBRAW_WBI_Daylight][1] = icWBC[LIBRAW_WBI_Daylight][3] = tag_val;
      break;
    case 0x00c2:
      icWBC[LIBRAW_WBI_Daylight][2] = tag_val;
      break;
    case 0x00c3:
      icWBC[LIBRAW_WBI_Cloudy][0] = tag_val;
      break;
    case 0x00c4:
      icWBC[LIBRAW_WBI_Cloudy][1] = icWBC[LIBRAW_WBI_Cloudy][3] = tag_val;
      break;
    case 0x00c5:
      icWBC[LIBRAW_WBI_Cloudy][2] = tag_val;
      break;
    case 0x00c6:
      icWBC[LIBRAW_WBI_Fluorescent][0] = tag_val;
      break;
    case 0x00c7:
      icWBC[LIBRAW_WBI_Fluorescent][1] = icWBC[LIBRAW_WBI_Fluorescent][3] = tag_val;
      break;
    case 0x00c8:
      icWBC[LIBRAW_WBI_Fluorescent][2] = tag_val;
      break;
    case 0x00c9:
      icWBC[LIBRAW_WBI_Tungsten][0] = tag_val;
      break;
    case 0x00ca:
      icWBC[LIBRAW_WBI_Tungsten][1] = icWBC[LIBRAW_WBI_Tungsten][3] = tag_val;
      break;
    case 0x00cb:
      icWBC[LIBRAW_WBI_Tungsten][2] = tag_val;
      break;
    case 0x00cc:
      icWBC[LIBRAW_WBI_Flash][0] = tag_val;
      break;
    case 0x00cd:
      icWBC[LIBRAW_WBI_Flash][1] = icWBC[LIBRAW_WBI_Flash][3] = tag_val;
      break;
    case 0x00ce:
      icWBC[LIBRAW_WBI_Flash][2] = tag_val;
      break;
    case 0x00d0:
       cam_mul[0] = tag_val;
      break;
    case 0x00d1:
       cam_mul[1] = cam_mul[3] = tag_val;
      break;
    case 0x00d2:
       cam_mul[2] = tag_val;
      break;
    }
    offset += 12;
  }

restore_after_parseSonySRF:
  free (srf_buf);
  fseek (ifp, save, SEEK_SET);
}

#undef ilm
#undef icWBC
#undef icWBCTC
#undef imSony

void CLASS parse_makernote_0xc634(int base, int uptag, unsigned dng_writer) {

  if (imgdata.params.raw_processing_options & LIBRAW_PROCESSING_SKIP_MAKERNOTES)
  	return;

  if (!strncmp(make, "NIKON", 5)) {
    parseNikonMakernote (base, uptag, AdobeDNG);
    return;
  } else if (!strncasecmp(make, "LEICA", 5)) {
    parseLeicaMakernote (base, uptag, is_0xc634);
    return;
  }

  unsigned offset = 0, entries, tag, type, len, save, c;
  unsigned i;

  uchar *CanonCameraInfo;
  unsigned lenCanonCameraInfo = 0;
  unsigned typeCanonCameraInfo = 0;

  int isSony = 0;

  int LeicaMakernoteSignature = -1;

  uchar *table_buf;
  uchar *table_buf_0x0116;
  ushort table_buf_0x0116_len = 0;
  uchar *table_buf_0x2010;
  ushort table_buf_0x2010_len = 0;
  uchar *table_buf_0x9050;
  ushort table_buf_0x9050_len = 0;
  uchar *table_buf_0x9400;
  ushort table_buf_0x9400_len = 0;
  uchar *table_buf_0x9402;
  ushort table_buf_0x9402_len = 0;
  uchar *table_buf_0x9403;
  ushort table_buf_0x9403_len = 0;
  uchar *table_buf_0x9406;
  ushort table_buf_0x9406_len = 0;
  uchar *table_buf_0x940c;
  ushort table_buf_0x940c_len = 0;
  uchar *table_buf_0x940e;
  ushort table_buf_0x940e_len = 0;

  short morder, sorder = order;
  char buf[10];
  INT64 fsize = ifp->size();

  fread(buf, 1, 10, ifp);

  if (!strcmp(buf, "OLYMPUS") ||
      !strcmp(buf, "PENTAX ") ||
      (!strncmp(make, "SAMSUNG", 7) && (dng_writer == CameraDNG))) {
    base = ftell(ifp) - 10;
    fseek(ifp, -2, SEEK_CUR);
    order = get2();
    if (buf[0] == 'O') get2();

  } else if (is_PentaxRicohMakernotes &&
             (dng_writer == CameraDNG)) {
    base = ftell(ifp) - 10;
    fseek(ifp, -4, SEEK_CUR);
    order = get2();
    is_PentaxRicohMakernotes = 1;

  } else if (!strncmp(buf, "SONY", 4) ||
             !strcmp(buf, "Panasonic")) {
    order = 0x4949;
    fseek(ifp, 2, SEEK_CUR);

  } else if (!strncmp(buf, "FUJIFILM", 8)) {
    base = ftell(ifp) - 10;
    order = 0x4949;
    fseek(ifp, 2, SEEK_CUR);

  } else if (!strcmp(buf, "OLYMP") ||
             !strcmp(buf, "Ricoh") ||
             !strcmp(buf, "EPSON")) {
    fseek(ifp, -2, SEEK_CUR);

  } else if (!strcmp(buf, "AOC") ||
             !strcmp(buf, "QVC")) {
    fseek(ifp, -4, SEEK_CUR);

  } else {
    fseek(ifp, -10, SEEK_CUR);
    if ((!strncmp(make, "SAMSUNG", 7) &&
        (dng_writer == AdobeDNG)))
      base = ftell(ifp);
  }

  entries = get2();
  if (entries > 1000) return;

  if (!strncasecmp(make, "SONY", 4)    ||
      !strncasecmp(make, "Konica", 6)  ||
      !strncasecmp(make, "Minolta", 7) ||
      (!strncasecmp(make, "Hasselblad", 10) &&
       (!strncasecmp(model, "Stellar", 7) ||
        !strncasecmp(model, "Lunar", 5)   ||
        !strncasecmp(model, "Lusso", 5)   ||
        !strncasecmp(model, "HV", 2)))
     )
   isSony = 1;

  morder = order;
  while (entries--) {
    order = morder;
    tiff_get(base, &tag, &type, &len, &save);
    INT64 pos = ifp->tell();
    if (len > 8 && pos + len > 2 * fsize) {
      fseek(ifp, save, SEEK_SET); // Recover tiff-read position!!
      continue;
    }
    tag |= uptag << 16;
    if (len > 100 * 1024 * 1024)
      goto next; // 100Mb tag? No!

    if (!strncmp(make, "Canon", 5)) {
      if (tag == 0x000d && len < 256000) { // camera info
        if (type != 4) {
          CanonCameraInfo = (uchar *)malloc(MAX(16, len));
          fread(CanonCameraInfo, len, 1, ifp);

        } else {
          CanonCameraInfo = (uchar *)malloc(MAX(16, len * 4));
          fread(CanonCameraInfo, len, 4, ifp);
        }
        lenCanonCameraInfo = len;
        typeCanonCameraInfo = type;
      }

      else if (tag == 0x10) { // Canon ModelID
        unique_id = get4();
        unique_id = setCanonBodyFeatures(unique_id);
        if (lenCanonCameraInfo) {
          processCanonCameraInfo(unique_id, CanonCameraInfo, lenCanonCameraInfo, typeCanonCameraInfo);
          free(CanonCameraInfo);
          CanonCameraInfo = 0;
          lenCanonCameraInfo = 0;
        }
      }

      else
        parseCanonMakernotes(tag, type, len);
    }

    else if (!strncmp(make, "FUJI", 4))
      parseFujiMakernotes(tag, type, len, AdobeDNG);

    else if (!strncmp(make, "OLYMPUS", 7) ||
             (!strncasecmp(make, "CLAUSS", 6) && !strncasecmp(model, "piX 5oo", 7))) {

      int SubDirOffsetValid =
          strncmp(model, "E-300", 5) &&
          strncmp(model, "E-330", 5) &&
          strncmp(model, "E-400", 5) &&
          strncmp(model, "E-500", 5) &&
          strncmp(model, "E-1", 3);

      if ((tag == 0x2010) ||
          (tag == 0x2020) ||
          (tag == 0x2030) ||
          (tag == 0x2031) ||
          (tag == 0x2040) ||
          (tag == 0x2050) ||
          (tag == 0x3000)) {
        fseek(ifp, save - 4, SEEK_SET);
        fseek(ifp, base + get4(), SEEK_SET);
        parse_makernote_0xc634(base, tag, dng_writer);
      }

      if (!SubDirOffsetValid &&
          ((len > 4) ||
           (((type == 3) || (type == 8)) && (len > 2)) ||
           (((type == 4) || (type == 9)) && (len > 1)) ||
           (type == 5) ||
           (type > 9)))
        goto skip_Oly_broken_tags;

      else if ((tag >= 0x20100000) && (tag <= 0x2010ffff))
        parseOlympus_Equipment ((tag & 0x0000ffff), type, len, AdobeDNG);

      else if ((tag >= 0x20200000) && (tag <= 0x2020ffff))
        parseOlympus_CameraSettings (base, (tag & 0x0000ffff), type, len, AdobeDNG);

      else if ((tag == 0x20300108) || (tag == 0x20310109))
        imgdata.makernotes.olympus.ColorSpace = get2();

      else if ((tag >= 0x20400000) && (tag <= 0x2040ffff))
        parseOlympus_ImageProcessing ((tag & 0x0000ffff), type, len, AdobeDNG);

      else if ((tag >= 0x30000000) && (tag <= 0x3000ffff))
        parseOlympus_RawInfo ((tag & 0x0000ffff), type, len, AdobeDNG);

      else switch (tag) {
      case 0x0207:
        getOlympus_CameraType2();
        break;
      case 0x1002:
        imgdata.lens.makernotes.CurAp = libraw_powf64l(2.0f, getreal(type) / 2);
        break;
      case 0x1007:
        imgdata.other.SensorTemperature = (float)get2();
        break;
      case 0x1008:
        imgdata.other.LensTemperature = (float)get2();
        break;
      case 0x20501500:
        getOlympus_SensorTemperature(len);
        break;
      }

skip_Oly_broken_tags:;
    }

    else if (!strncmp(make, "PENTAX", 6)  ||
             !strncmp(model, "PENTAX", 6) ||
             is_PentaxRicohMakernotes) {
      parsePentaxMakernotes(base, tag, type, len, dng_writer);

    } else if (!strncmp(make, "SAMSUNG", 7)) {
      if (dng_writer == AdobeDNG)
        parseSamsungMakernotes(base, tag, type, len, dng_writer);
      else
        parsePentaxMakernotes(base, tag, type, len, dng_writer);

    } else if (isSony) {
      parseSonyMakernotes(base, tag, type, len, AdobeDNG,
                          table_buf_0x0116, table_buf_0x0116_len,
                          table_buf_0x2010, table_buf_0x2010_len,
                          table_buf_0x9050, table_buf_0x9050_len,
                          table_buf_0x9400, table_buf_0x9400_len,
                          table_buf_0x9402, table_buf_0x9402_len,
                          table_buf_0x9403, table_buf_0x9403_len,
                          table_buf_0x9406, table_buf_0x9406_len,
                          table_buf_0x940c, table_buf_0x940c_len,
                          table_buf_0x940e, table_buf_0x940e_len);
    }
  next:
    fseek(ifp, save, SEEK_SET);
  }
quit:
  order = sorder;
}

#else
void CLASS parse_makernote_0xc634(int base, int uptag, unsigned dng_writer)
{ /*placeholder */
}
#endif

void CLASS parse_makernote(int base, int uptag)
{

#ifdef LIBRAW_LIBRARY_BUILD
  if (imgdata.params.raw_processing_options & LIBRAW_PROCESSING_SKIP_MAKERNOTES)
  	return;
  if (!strncmp(make, "NIKON", 5)) {
    parseNikonMakernote (base, uptag, nonDNG);
    return;
  } else if (!strncasecmp(make, "LEICA", 5)) {
    parseLeicaMakernote (base, uptag, is_0x927c);
    return;
  }
#endif

  unsigned offset = 0, entries, tag, type, len, save, c;
  unsigned serial = 0, i, wbi = 0, wb[4] = {0, 0, 0, 0};
  short morder, sorder = order;
  char buf[10];
  unsigned SamsungKey[11];

  uchar NikonKey;
  unsigned ver97 = 0;
  uchar buf97[324];
  uchar ci, cj, ck;


#ifdef LIBRAW_LIBRARY_BUILD
  uchar *CanonCameraInfo;
  unsigned lenCanonCameraInfo = 0;
  unsigned typeCanonCameraInfo = 0;

  int isSony = 0;

  uchar *table_buf;
  uchar *table_buf_0x0116;
  ushort table_buf_0x0116_len = 0;
  uchar *table_buf_0x2010;
  ushort table_buf_0x2010_len = 0;
  uchar *table_buf_0x9050;
  ushort table_buf_0x9050_len = 0;
  uchar *table_buf_0x9400;
  ushort table_buf_0x9400_len = 0;
  uchar *table_buf_0x9402;
  ushort table_buf_0x9402_len = 0;
  uchar *table_buf_0x9403;
  ushort table_buf_0x9403_len = 0;
  uchar *table_buf_0x9406;
  ushort table_buf_0x9406_len = 0;
  uchar *table_buf_0x940c;
  ushort table_buf_0x940c_len = 0;
  uchar *table_buf_0x940e;
  ushort table_buf_0x940e_len = 0;

  INT64 fsize = ifp->size();
#endif
/*
     The MakerNote might have its own TIFF header (possibly with
     its own byte-order!), or it might just be a table.
*/
  if (!strncmp(make, "Nokia", 5))
    return;

  fread(buf, 1, 10, ifp);

  if (!strncmp(buf, "KDK", 3)  || /* these aren't TIFF tables */
      !strncmp(buf, "VER", 3)  ||
      !strncmp(buf, "IIII", 4) ||
      !strncmp(buf, "MMMM", 4))
    return;

  if (!strncmp(buf, "KC", 2) || /* Konica KD-400Z, KD-510Z */
      !strncmp(buf, "MLY", 3))  /* Minolta DiMAGE G series */
  {
    order = 0x4d4d;
    while ((i = ftell(ifp)) < data_offset && i < 16384) {
      wb[0] = wb[2];
      wb[2] = wb[1];
      wb[1] = wb[3];
      wb[3] = get2();
      if (wb[1] == 256 && wb[3] == 256 && wb[0] > 256 && wb[0] < 640 && wb[2] > 256 && wb[2] < 640)
        FORC4 cam_mul[c] = wb[c];
    }
    goto quit;
  }

  if (!strcmp(buf, "Nikon")) {
    base = ftell(ifp);
    order = get2();
    if (get2() != 42) goto quit;
    offset = get4();
    fseek(ifp, offset - 8, SEEK_CUR);

  } else if (!strcmp(buf, "OLYMPUS") ||
             !strcmp(buf, "PENTAX ")) {
    base = ftell(ifp) - 10;
    fseek(ifp, -2, SEEK_CUR);
    order = get2();
    if (buf[0] == 'O') get2();

  } else if (!strncmp(buf, "SONY", 4) ||
           !strcmp(buf, "Panasonic")) {
    goto nf;

  } else if (!strncmp(buf, "FUJIFILM", 8)) {
    base = ftell(ifp) - 10;
  nf:
    order = 0x4949;
    fseek(ifp, 2, SEEK_CUR);

  } else if (!strcmp(buf, "OLYMP") ||
             !strncmp (buf,"LEICA", 5) ||
             !strcmp(buf, "Ricoh") ||
             !strcmp(buf, "EPSON")) {
    fseek(ifp, -2, SEEK_CUR);

  } else if (!strcmp(buf, "AOC") ||
             !strcmp(buf, "QVC")) {
    fseek(ifp, -4, SEEK_CUR);

  } else {
    fseek(ifp, -10, SEEK_CUR);
    if (!strncmp(make, "SAMSUNG", 7))
      base = ftell(ifp);
  }

#ifdef LIBRAW_LIBRARY_BUILD
  if (!strncasecmp(make, "SONY", 4)    ||
      !strncasecmp(make, "Konica", 6)  ||
      !strncasecmp(make, "Minolta", 7) ||
      (!strncasecmp(make, "Hasselblad", 10) &&
       (!strncasecmp(model, "Stellar", 7) ||
        !strncasecmp(model, "Lunar", 5)   ||
        !strncasecmp(model, "Lusso", 5)   ||
        !strncasecmp(model, "HV", 2)))
     )
   isSony = 1;

  if (strcasestr(make, "Kodak")       &&
      (sget2((uchar *)buf) > 1)       &&  // check number of entries
      (sget2((uchar *)buf) < 128)     &&
      (sget2((uchar *)(buf+4)) > 0)   &&  // check type
      (sget2((uchar *)(buf+4)) < 13)  &&
      (sget4((uchar *)(buf+6)) < 256)     // check count
     )
   imgdata.makernotes.kodak.MakerNoteKodak8a = 1;  // Kodak P712 / P850 / P880
#endif

  entries = get2();
  if (entries > 1000) return;

  morder = order;
  while (entries--) {
    order = morder;
    tiff_get(base, &tag, &type, &len, &save);
    tag |= uptag << 16;

#ifdef LIBRAW_LIBRARY_BUILD
    INT64 _pos = ftell(ifp);
    if (len > 8 && _pos + len > 2 * fsize)
    {
      fseek(ifp, save, SEEK_SET); // Recover tiff-read position!!
      continue;
    }
    if (imgdata.makernotes.kodak.MakerNoteKodak8a) {
      if ((tag == 0xff00) && (type == 4) && (len == 1)) {
        INT64 _pos1 = get4();
        if ((_pos1 < fsize) && (_pos1 > 0)) {
          fseek(ifp, _pos1, SEEK_SET);
          parse_makernote(base, tag);
        }

      } else if (tag == 0xff00f90b) {
        imgdata.makernotes.kodak.clipBlack = get2();

      } else if (tag == 0xff00f90c) {
        imgdata.makernotes.kodak.clipWhite =
          imgdata.color.linear_max[0] =
          imgdata.color.linear_max[1] =
          imgdata.color.linear_max[2] =
          imgdata.color.linear_max[3] = get2();
      }
    }
    else if (!strncmp(make, "Canon", 5))
    {
      if (tag == 0x000d && len < 256000) // camera info
      {
        if (type != 4)
        {
          CanonCameraInfo = (uchar *)malloc(MAX(16, len));
          fread(CanonCameraInfo, len, 1, ifp);
        }
        else
        {
          CanonCameraInfo = (uchar *)malloc(MAX(16, len * 4));
          fread(CanonCameraInfo, len, 4, ifp);
        }
        lenCanonCameraInfo = len;
        typeCanonCameraInfo = type;
      }

      else if (tag == 0x10) // Canon ModelID
      {
        unique_id = get4();
        unique_id = setCanonBodyFeatures(unique_id);
        if (lenCanonCameraInfo)
        {
          processCanonCameraInfo(unique_id, CanonCameraInfo, lenCanonCameraInfo, typeCanonCameraInfo);
          free(CanonCameraInfo);
          CanonCameraInfo = 0;
          lenCanonCameraInfo = 0;
        }
      }

      else
        parseCanonMakernotes(tag, type, len);
    }

    else if (!strncmp(make, "FUJI", 4))
        parseFujiMakernotes(tag, type, len, nonDNG);

    else if (!strncasecmp(model, "Hasselblad X1D", 14) ||
             !strncasecmp(model, "Hasselblad H6D", 14) ||
             !strncasecmp(model, "Hasselblad A6D", 14))
    {
      if (tag == 0x0045)
      {
        imgdata.makernotes.hasselblad.BaseISO = get4();
      }
      else if (tag == 0x0046)
      {
        imgdata.makernotes.hasselblad.Gain = getreal(type);
      }
    }

    else if (!strncmp(make, "PENTAX", 6) ||
             !strncmp(make, "RICOH", 5)  ||
             !strncmp(model, "PENTAX", 6)) {
      if (!strncmp(model, "GR",  2) ||
          !strncmp(model, "GXR", 3)) {
        parseRicohMakernotes (base, tag, type, len, CameraDNG);
      } else {
        parsePentaxMakernotes(base, tag, type, len, nonDNG);
      }
    }

    else if (!strncmp(make, "SAMSUNG", 7))
    {
      if (!dng_version)
        parseSamsungMakernotes(base, tag, type, len, nonDNG);
      else
        parsePentaxMakernotes(base, tag, type, len, CameraDNG);
    }

    else if (isSony) {
      if ((tag == 0xb028) && (len == 1) && (type == 4)) { // DSLR-A100
        if ((c = get4())) {
          fseek (ifp, c, SEEK_SET);
          parse_makernote(base, tag);
        }
      } else {
      parseSonyMakernotes(base, tag, type, len, nonDNG,
                          table_buf_0x0116, table_buf_0x0116_len,
                          table_buf_0x2010, table_buf_0x2010_len,
                          table_buf_0x9050, table_buf_0x9050_len,
                          table_buf_0x9400, table_buf_0x9400_len,
                          table_buf_0x9402, table_buf_0x9402_len,
                          table_buf_0x9403, table_buf_0x9403_len,
                          table_buf_0x9406, table_buf_0x9406_len,
                          table_buf_0x940c, table_buf_0x940c_len,
                          table_buf_0x940e, table_buf_0x940e_len);
      }
    }
    fseek(ifp, _pos, SEEK_SET);
#endif

    if (tag == 2 && strstr(make, "NIKON") && !iso_speed)
      iso_speed = (get2(), get2());
    if (tag == 37 && strstr(make, "NIKON") && (!iso_speed || iso_speed == 65535))
    {
      iso_speed = int(100.0 * libraw_powf64l(2.0f, double((uchar)fgetc(ifp)) / 12.0 - 5.0));
    }
    if (tag == 4 && len > 26 && len < 35)
    {
      if ((i = (get4(), get2())) != 0x7fff && (!iso_speed || iso_speed == 65535))
        iso_speed = 50 * libraw_powf64l(2.0, i / 32.0 - 4);
#ifdef LIBRAW_LIBRARY_BUILD
      get4();
#else
      if (((i = (get2(), get2())) != 0x7fff) && !aperture)
        aperture = libraw_powf64l(2.0, i / 64.0);
#endif
      if (((i = get2()) != 0xffff) && !shutter)
        shutter = libraw_powf64l(2.0, (short)i / -32.0);
      wbi = (get2(), get2());
      shot_order = (get2(), get2());
    }
    if ((tag == 4 || tag == 0x114) && !strncmp(make, "KONICA", 6))
    {
      fseek(ifp, tag == 4 ? 140 : 160, SEEK_CUR);
      switch (get2())
      {
      case 72:
        flip = 0;
        break;
      case 76:
        flip = 6;
        break;
      case 82:
        flip = 5;
        break;
      }
    }
    if (tag == 7 && type == 2 && len > 20)
      fgets(model2, 64, ifp);
    if (tag == 8 && type == 4)
      shot_order = get4();
    if (tag == 9 && !strncmp(make, "Canon", 5))
      fread(artist, 64, 1, ifp);
    if (tag == 0xc && len == 4)
      FORC3 cam_mul[(c << 1 | c >> 1) & 3] = getreal(type);
    if (tag == 0xd && type == 7 && get2() == 0xaaaa)
    {
      for (c = i = 2; (ushort)c != 0xbbbb && i < len; i++)
        c = c << 8 | fgetc(ifp);
      while ((i += 4) < len - 5)
        if (get4() == 257 && (i = len) && (c = (get4(), fgetc(ifp))) < 3)
          imgdata.makernotes.canon.MakernotesFlip  = "065"[c] - '0';
    }

#ifndef LIBRAW_LIBRARY_BUILD
    if (tag == 0x10 && type == 4)
      unique_id = get4();
#endif

#ifdef LIBRAW_LIBRARY_BUILD
    INT64 _pos2 = ftell(ifp);
    if (!strncasecmp(make, "Olympus", 7) ||
        (!strncasecmp(make, "CLAUSS", 6) && !strncasecmp(model, "piX 5oo", 7))) {
      if ((tag == 0x2010) ||
          (tag == 0x2020) ||
          (tag == 0x2030) ||
          (tag == 0x2031) ||
          (tag == 0x2040) ||
          (tag == 0x2050) ||
          (tag == 0x3000)) {
        if (type == 7) { parse_makernote(base, tag); }
        else if (type == 13) { fseek(ifp, base+get4(), SEEK_SET); parse_makernote(base, tag); }

      } else if (tag == 0x0207) {
          getOlympus_CameraType2();

      } else if ((tag == 0x0404) || (tag == 0x101a)) {
          if (!imgdata.shootinginfo.BodySerial[0])
            stmread(imgdata.shootinginfo.BodySerial, len, ifp);

      } else if (tag == 0x1002) {
          imgdata.lens.makernotes.CurAp = libraw_powf64l(2.0f, getreal(type) / 2);
      } else if (tag == 0x1007) {
          imgdata.other.SensorTemperature = (float)get2();
      } else if (tag == 0x1008) {
          imgdata.other.LensTemperature = (float)get2();

      } else if ((tag == 0x1011) && strcmp(software, "v757-71")) {
          for (i = 0; i < 3; i++) {
            if (!imgdata.makernotes.olympus.ColorSpace) {
              FORC3 cmatrix[i][c] = ((short)get2()) / 256.0;
            } else {
              FORC3 imgdata.color.ccm[i][c] = ((short)get2()) / 256.0;
            }
          }

      } else if (tag == 0x1012) {
          FORC4 cblack[c ^ c >> 1] = get2();
      } else if (tag == 0x1017) {
          cam_mul[0] = get2() / 256.0;
      } else if (tag == 0x1018) {
          cam_mul[2] = get2() / 256.0;

      } else if ((tag >= 0x20100000) && (tag <= 0x2010ffff)) {
          parseOlympus_Equipment ((tag & 0x0000ffff), type, len, nonDNG);

      } else if ((tag >= 0x20200000) && (tag <= 0x2020ffff)) {
          parseOlympus_CameraSettings (base, (tag & 0x0000ffff), type, len, nonDNG);

      } else if ((tag == 0x20300108) || (tag == 0x20310109)) {
          imgdata.makernotes.olympus.ColorSpace = get2();

      } else if ((tag >= 0x20400000) && (tag <= 0x2040ffff)) {
          parseOlympus_ImageProcessing ((tag & 0x0000ffff), type, len, nonDNG);

      } else if (tag == 0x20501500) {
          getOlympus_SensorTemperature(len);

      } else if ((tag >= 0x30000000) && (tag <= 0x3000ffff)) {
          parseOlympus_RawInfo ((tag & 0x0000ffff), type, len, nonDNG);
      }

    }
    fseek(ifp, _pos2, SEEK_SET);
#endif

    if (tag == 0x11 && is_raw && !strncmp(make, "NIKON", 5))
    {
      fseek(ifp, get4() + base, SEEK_SET);
      parse_tiff_ifd(base);
    }

    if (tag == 0x14 && type == 7) {
      if (len == 2560) {
        fseek(ifp, 1248, SEEK_CUR);
        goto get2_256;
      } else if (len == 1280) {
        cam_mul[0] = cam_mul[1] = cam_mul[2] = cam_mul[3] = 1.0;

      } else {
        fread(buf, 1, 10, ifp);
        if (!strncmp(buf, "NRW ", 4)) {
          fseek(ifp, strcmp(buf + 4, "0100") ? 46 : 1546, SEEK_CUR);
          cam_mul[0] = get4() << 2;
          cam_mul[1] = get4() + get4();
          cam_mul[2] = get4() << 2;
        }
      }
    }

    if (tag == 0x15 && type == 2 && is_raw)
      fread(model, 64, 1, ifp);
    if (strstr(make, "PENTAX"))
    {
      if (tag == 0x1b)
        tag = 0x1018;
      if (tag == 0x1c)
        tag = 0x1017;
    }
    if ((tag == 0x1d) && (type == 2) && (len > 0))
    {
      while ((c = fgetc(ifp)) && (len-- > 0) && (c != EOF))
        serial = serial * 10 + (isdigit(c) ? c - '0' : c % 10);
    }
    if (tag == 0x29 && type == 1)
    { // Canon PowerShot G9
      c = wbi < 18 ? "012347800000005896"[wbi] - '0' : 0;
      fseek(ifp, 8 + c * 32, SEEK_CUR);
      FORC4 cam_mul[c ^ (c >> 1) ^ 1] = get4();
    }
    if (tag == 0x3d && type == 3 && len == 4)
      FORC4 cblack[c ^ c >> 1] = get2() >> (14 - tiff_bps);
    if (tag == 0x81 && type == 4)
    {
      data_offset = get4();
      fseek(ifp, data_offset + 41, SEEK_SET);
      raw_height = get2() * 2;
      raw_width = get2();
      filters = 0x61616161;
    }
    if ((tag == 0x81 && type == 7)  ||
        (tag == 0x100 && type == 7 && strncmp(make, "NIKON", 5)) ||
        (tag == 0x280 && type == 1))
    {
      thumb_offset = ftell(ifp);
      thumb_length = len;
    }
    if (tag == 0x88 && type == 4 && (thumb_offset = get4()))
      thumb_offset += base;
    if (tag == 0x89 && type == 4)
      thumb_length = get4();
    if ((tag == 0x8c || tag == 0x96) && (type == 7))
      meta_offset = ftell(ifp);
    if ((tag == 0x97) && !strncmp(make, "NIKON", 5))
    {
      for (i = 0; i < 4; i++)
        ver97 = ver97 * 10 + fgetc(ifp) - '0';
      switch (ver97)
      {
      case 100:
        fseek(ifp, 68, SEEK_CUR);
        FORC4 cam_mul[(c >> 1) | ((c & 1) << 1)] = get2();
        break;
      case 102:
        fseek(ifp, 6, SEEK_CUR);
        FORC4 cam_mul[c ^ (c >> 1)] = get2();
        break;
      case 103:
        fseek(ifp, 16, SEEK_CUR);
        FORC4 cam_mul[c] = get2();
      }
      if (ver97 >= 200)
      {
        if (ver97 != 205)
          fseek(ifp, 280, SEEK_CUR);
        fread(buf97, 324, 1, ifp);
      }
    }
    if ((tag == 0xa1) && (type == 7) && strncasecmp(make, "Samsung", 7))
    {
      order = 0x4949;
      fseek(ifp, 140, SEEK_CUR);
      FORC3 cam_mul[c] = get4();
    }
    if (tag == 0xa4 && type == 3)
    {
      fseek(ifp, wbi * 48, SEEK_CUR);
      FORC3 cam_mul[c] = get2();
    }

    if (tag == 0xa7)
    { // shutter count
      NikonKey = fgetc(ifp) ^ fgetc(ifp) ^ fgetc(ifp) ^ fgetc(ifp);
      if ((unsigned)(ver97 - 200) < 18)
      {
        ci = xlat[0][serial & 0xff];
        cj = xlat[1][NikonKey];
        ck = 0x60;
        for (i = 0; i < 324; i++)
          buf97[i] ^= (cj += ci * ck++);
        i = "66666>666;6A;:;555"[ver97 - 200] - '0';
        FORC4 cam_mul[c ^ (c >> 1) ^ (i & 1)] = sget2(buf97 + (i & -2) + c * 2);
      }
    }

    if (tag == 0xb001 && type == 3) // Sony ModelID
    {
      unique_id = get2();
    }
    if (tag == 0x200 && len == 3)
      shot_order = (get4(), get4());
    if (tag == 0x200 && len == 4) // Pentax black level
      FORC4 cblack[c ^ c >> 1] = get2();
    if (tag == 0x201 && len == 4) // Pentax As Shot WB
      FORC4 cam_mul[c ^ (c >> 1)] = get2();
    if (tag == 0x220 && type == 7)
      meta_offset = ftell(ifp);
    if (tag == 0x401 && type == 4 && len == 4)
      FORC4 cblack[c ^ c >> 1] = get4();
    if (tag == 0xe01)
    { /* Nikon Capture Note */
      order = 0x4949;
      fseek(ifp, 22, SEEK_CUR);
      for (offset = 22; offset + 22 < len; offset += 22 + i)
      {
        tag = get4();
        fseek(ifp, 14, SEEK_CUR);
        i = get4() - 4;
        if (tag == 0x76a43207)
          flip = get2();
        else
          fseek(ifp, i, SEEK_CUR);
      }
    }
    if (tag == 0xe80 && len == 256 && type == 7)
    {
      fseek(ifp, 48, SEEK_CUR);
      cam_mul[0] = get2() * 508 * 1.078 / 0x10000;
      cam_mul[2] = get2() * 382 * 1.173 / 0x10000;
    }
    if (tag == 0xf00 && type == 7)
    {
      if (len == 614)
        fseek(ifp, 176, SEEK_CUR);
      else if (len == 734 || len == 1502)
        fseek(ifp, 148, SEEK_CUR);
      else
        goto next;
      goto get2_256;
    }

#ifndef LIBRAW_LIBRARY_BUILD
    if (((tag == 0x1011 && len == 9) || tag == 0x20400200) && strcmp(software, "v757-71"))
      for (i = 0; i < 3; i++)
        FORC3 cmatrix[i][c] = ((short)get2()) / 256.0;

    if ((tag == 0x1012 || tag == 0x20400600) && len == 4)
      FORC4 cblack[c ^ c >> 1] = get2();
    if (tag == 0x1017 || tag == 0x20400100)
      cam_mul[0] = get2() / 256.0;
    if (tag == 0x1018 || tag == 0x20400100)
      cam_mul[2] = get2() / 256.0;
#endif

    if (tag == 0x2011 && len == 2) {
get2_256:
      order = 0x4d4d;
      cam_mul[0] = get2() / 256.0;
      cam_mul[2] = get2() / 256.0;
    }

#ifndef LIBRAW_LIBRARY_BUILD
    if ((tag | 0x70) == 0x2070 && (type == 4 || type == 13))
      fseek(ifp, get4() + base, SEEK_SET);
    if ((tag == 0x2020) && ((type == 7) || (type == 13)) && !strncmp(buf, "OLYMP", 5))
      parse_thumb_note(base, 257, 258);
    if (tag == 0xb028) {
      fseek(ifp, get4() + base, SEEK_SET);
      parse_thumb_note(base, 136, 137);
    }
    if (tag == 0x2040)
      parse_makernote(base, 0x2040);

// Canon
    if (tag == 0x4001 && len > 500 && len < 100000)
    {
      i = len == 582 ? 50 : len == 653 ? 68 : len == 5120 ? 142 : 126;
      fseek(ifp, i, SEEK_CUR);
      FORC4 cam_mul[c ^ (c >> 1)] = get2();
      for (i += 18; i <= len; i += 10)
      {
        get2();
        FORC4 sraw_mul[c ^ (c >> 1)] = get2();
        if (sraw_mul[1] == 1170)
          break;
      }
    }

    if (!strncasecmp(make, "Samsung", 7))
    {
      if (tag == 0xa020) // get the full Samsung encryption key
        for (i = 0; i < 11; i++)
          SamsungKey[i] = get4();

      if (tag == 0xa021) // get and decode Samsung cam_mul array
        FORC4 cam_mul[c ^ (c >> 1)] = get4() - SamsungKey[c];

      if (tag == 0xa028)
        FORC4 cblack[c ^ (c >> 1)] = get4() - SamsungKey[c];

      if (tag == 0xa031 && len == 9) // get and decode Samsung color matrix
        for (i = 0; i < 3; i++)
          FORC3 cmatrix[i][c] = (float)((short)((get4() + SamsungKey[i * 3 + c]))) / 256.0;

    }
#endif

#ifdef LIBRAW_LIBRARY_BUILD
    if (tag == 0x4021 && (imgdata.makernotes.canon.multishot[0] = get4()) &&
        (imgdata.makernotes.canon.multishot[1] = get4()))
    {
      if (len >= 4)
      {
        imgdata.makernotes.canon.multishot[2] = get4();
        imgdata.makernotes.canon.multishot[3] = get4();
      }
      FORC4 cam_mul[c] = 1024;
    }
#else
    if (tag == 0x4021 && get4() && get4())
      FORC4 cam_mul[c] = 1024;
#endif
  next:
    fseek(ifp, save, SEEK_SET);
  }
quit:
  order = sorder;
}

/*
   Since the TIFF DateTime string has no timezone information,
   assume that the camera's clock was set to Universal Time.
 */
void CLASS get_timestamp(int reversed)
{
  struct tm t;
  char str[20];
  int i;

  str[19] = 0;
  if (reversed)
    for (i = 19; i--;)
      str[i] = fgetc(ifp);
  else
    fread(str, 19, 1, ifp);
  memset(&t, 0, sizeof t);
  if (sscanf(str, "%d:%d:%d %d:%d:%d", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec) != 6)
    return;
  t.tm_year -= 1900;
  t.tm_mon -= 1;
  t.tm_isdst = -1;
  if (mktime(&t) > 0)
    timestamp = mktime(&t);
}

void CLASS parse_exif(int base)
{
  unsigned kodak, entries, tag, type, len, save, c;
  double expo, ape;

  kodak = !strncmp(make, "EASTMAN", 7) && tiff_nifds < 3;
  entries = get2();
  if (!strncmp(make, "Hasselblad", 10) && (tiff_nifds > 3) && (entries > 512))
    return;
#ifdef LIBRAW_LIBRARY_BUILD
  INT64 fsize = ifp->size();
#endif
  while (entries--)
  {
    tiff_get(base, &tag, &type, &len, &save);

#ifdef LIBRAW_LIBRARY_BUILD
    INT64 savepos = ftell(ifp);
    if (len > 8 && savepos + len > fsize * 2)
    {
      fseek(ifp, save, SEEK_SET); // Recover tiff-read position!!
      continue;
    }
    if (callbacks.exif_cb)
    {
      callbacks.exif_cb(callbacks.exifparser_data, tag, type, len, order, ifp,base);
      fseek(ifp, savepos, SEEK_SET);
    }
#endif
    switch (tag)
    {
#ifdef LIBRAW_LIBRARY_BUILD

    case 0x9400:
      imgdata.other.exifAmbientTemperature = getreal(type);
      if ((imgdata.other.CameraTemperature > -273.15f) && (OlyID == 0x4434353933ULL)) // TG-5
        imgdata.other.CameraTemperature += imgdata.other.exifAmbientTemperature;
      break;
    case 0x9401:
      imgdata.other.exifHumidity = getreal(type);
      break;
    case 0x9402:
      imgdata.other.exifPressure = getreal(type);
      break;
    case 0x9403:
      imgdata.other.exifWaterDepth = getreal(type);
      break;
    case 0x9404:
      imgdata.other.exifAcceleration = getreal(type);
      break;
    case 0x9405:
      imgdata.other.exifCameraElevationAngle = getreal(type);
      break;

    case 0xa405: // FocalLengthIn35mmFormat
      imgdata.lens.FocalLengthIn35mmFormat = get2();
      break;
    case 0xa431: // BodySerialNumber
      stmread(imgdata.shootinginfo.BodySerial, len, ifp);
      break;
    case 0xa432: // LensInfo, 42034dec, Lens Specification per EXIF standard
      imgdata.lens.MinFocal = getreal(type);
      imgdata.lens.MaxFocal = getreal(type);
      imgdata.lens.MaxAp4MinFocal = getreal(type);
      imgdata.lens.MaxAp4MaxFocal = getreal(type);
      break;
    case 0xa435: // LensSerialNumber
      stmread(imgdata.lens.LensSerial, len, ifp);
      if (!strncmp(imgdata.lens.LensSerial, "----", 4))
        imgdata.lens.LensSerial[0] = '\0';
      break;
    case 0xc630: // DNG LensInfo, Lens Specification per EXIF standard
      imgdata.lens.dng.MinFocal = getreal(type);
      imgdata.lens.dng.MaxFocal = getreal(type);
      imgdata.lens.dng.MaxAp4MinFocal = getreal(type);
      imgdata.lens.dng.MaxAp4MaxFocal = getreal(type);
      break;
    case 0xa433: // LensMake
      stmread(imgdata.lens.LensMake, len, ifp);
      break;
    case 0xa434: // LensModel
      stmread(imgdata.lens.Lens, len, ifp);
      if (!strncmp(imgdata.lens.Lens, "----", 4))
        imgdata.lens.Lens[0] = '\0';
      break;
    case 0x9205:
      imgdata.lens.EXIF_MaxAp = libraw_powf64l(2.0f, (getreal(type) / 2.0f));
      break;
#endif
    case 33434:
      tiff_ifd[tiff_nifds - 1].t_shutter = shutter = getreal(type);
      break;
    case 33437:
      aperture = getreal(type);
      break; // 0x829d FNumber
    case 34855:
      iso_speed = get2();
      break;
    case 34865:
      if (iso_speed == 0xffff && !strncasecmp(make, "FUJI", 4))
        iso_speed = getreal(type);
      break;
    case 34866:
      if (iso_speed == 0xffff && (!strncasecmp(make, "SONY", 4) || !strncasecmp(make, "CANON", 5)))
        iso_speed = getreal(type);
      break;
    case 36867:
    case 36868:
      get_timestamp(0);
      break;
    case 37377:
      if ((expo = -getreal(type)) < 128 && shutter == 0.)
        tiff_ifd[tiff_nifds - 1].t_shutter = shutter = libraw_powf64l(2.0, expo);
      break;
    case 37378: // 0x9202 ApertureValue
      if ((fabs(ape = getreal(type)) < 256.0) && (!aperture))
        aperture = libraw_powf64l(2.0, ape / 2);
      break;
    case 37385:
      flash_used = getreal(type);
      break;
    case 37386:
      focal_len = getreal(type);
      break;
    case 37500: // tag 0x927c
#ifdef LIBRAW_LIBRARY_BUILD
      if (((make[0] == '\0') && (!strncmp(model, "ov5647", 6))) ||
          ((!strncmp(make, "RaspberryPi", 11)) && (!strncmp(model, "RP_OV5647", 9))) ||
          ((!strncmp(make, "RaspberryPi", 11)) && (!strncmp(model, "RP_imx219", 9)))) {
        char mn_text[512];
        char *pos;
        char ccms[512];
        ushort l;
        float num;

	      fgets(mn_text, MIN(len,511), ifp);
        mn_text[511] = 0;

        pos = strstr(mn_text, "gain_r=");
        if (pos) cam_mul[0] = atof(pos + 7);
        pos = strstr(mn_text, "gain_b=");
        if (pos) cam_mul[2] = atof(pos + 7);
        if ((cam_mul[0] > 0.001f) && (cam_mul[2] > 0.001f))
          cam_mul[1] = cam_mul[3] = 1.0f;
        else
          cam_mul[0] = cam_mul[2] = 0.0f;

        pos = strstr(mn_text, "ccm=");
        if(pos) {
         pos +=4;
         char *pos2 = strstr(pos, " ");
         if(pos2) {
           l = pos2 - pos;
           memcpy(ccms, pos, l);
           ccms[l] = '\0';
#if defined WIN32 || defined(__MINGW32__)
           // Win32 strtok is already thread-safe
          pos = strtok(ccms, ",");
#else
          char *last=0;
          pos = strtok_r(ccms, ",",&last);
#endif
          if(pos) {
            for (l = 0; l < 4; l++) {
              num = 0.0;
              for (c = 0; c < 3; c++) {
                imgdata.color.ccm[l][c] = (float)atoi(pos);
                num += imgdata.color.ccm[l][c];
#if defined WIN32 || defined(__MINGW32__)
                pos = strtok(NULL, ",");
#else
                pos = strtok_r(NULL, ",",&last);
#endif
                if(!pos) goto end; // broken
              }
              if (num > 0.01)
                FORC3 imgdata.color.ccm[l][c] = imgdata.color.ccm[l][c] / num;
            }
          }
        }
       }
      end:;

      } else if (!strncmp(make, "SONY", 4) &&
               (!strncmp(model, "DSC-V3", 6) || !strncmp(model, "DSC-F828", 8))) {
        parseSonySRF(len);
        break;
      } else

#endif
        if ((len == 1) && !strncmp(make, "NIKON", 5)) {
          c = get4();
          if (c) fseek (ifp, c, SEEK_SET);
          is_NikonTransfer = 1;
        }
        parse_makernote(base, 0);
      break;
    case 40962:
      if (kodak)
        raw_width = get4();
      break;
    case 40963:
      if (kodak)
        raw_height = get4();
      break;
    case 41730:
      if (get4() == 0x20002)
        for (exif_cfa = c = 0; c < 8; c += 2)
          exif_cfa |= fgetc(ifp) * 0x01010101U << c;
    }
    fseek(ifp, save, SEEK_SET);
  }
}

#ifdef LIBRAW_LIBRARY_BUILD

void CLASS parse_gps_libraw(int base)
{
  unsigned entries, tag, type, len, save, c;

  entries = get2();
  if (entries > 200)
    return;
  if (entries > 0)
    imgdata.other.parsed_gps.gpsparsed = 1;
  while (entries--)
  {
    tiff_get(base, &tag, &type, &len, &save);
    if (len > 1024)
    {
      fseek(ifp, save, SEEK_SET); // Recover tiff-read position!!
      continue; // no GPS tags are 1k or larger
    }
    switch (tag)
    {
    case 1:
      imgdata.other.parsed_gps.latref = getc(ifp);
      break;
    case 3:
      imgdata.other.parsed_gps.longref = getc(ifp);
      break;
    case 5:
      imgdata.other.parsed_gps.altref = getc(ifp);
      break;
    case 2:
      if (len == 3)
        FORC(3) imgdata.other.parsed_gps.latitude[c] = getreal(type);
      break;
    case 4:
      if (len == 3)
        FORC(3) imgdata.other.parsed_gps.longtitude[c] = getreal(type);
      break;
    case 7:
      if (len == 3)
        FORC(3) imgdata.other.parsed_gps.gpstimestamp[c] = getreal(type);
      break;
    case 6:
      imgdata.other.parsed_gps.altitude = getreal(type);
      break;
    case 9:
      imgdata.other.parsed_gps.gpsstatus = getc(ifp);
      break;
    }
    fseek(ifp, save, SEEK_SET);
  }
}
#endif

void CLASS parse_gps(int base)
{
  unsigned entries, tag, type, len, save, c;

  entries = get2();
  while (entries--)
  {
    tiff_get(base, &tag, &type, &len, &save);
    if (len > 1024)
    {
      fseek(ifp, save, SEEK_SET); // Recover tiff-read position!!
      continue; // no GPS tags are 1k or larger
    }
    switch (tag)
    {
    case 1:
    case 3:
    case 5:
      gpsdata[29 + tag / 2] = getc(ifp);
      break;
    case 2:
    case 4:
    case 7:
      FORC(6) gpsdata[tag / 3 * 6 + c] = get4();
      break;
    case 6:
      FORC(2) gpsdata[18 + c] = get4();
      break;
    case 18:
    case 29:
      fgets((char *)(gpsdata + 14 + tag / 3), MIN(len, 12), ifp);
    }
    fseek(ifp, save, SEEK_SET);
  }
}

void CLASS aRGB_coeff(double aRGB_cam[3][3])
{
  static const double rgb_aRGB[3][3] =
  {{1.39828313770000,    -0.3982830047, 9.64980900741708E-8},
   {6.09219200572997E-8,  0.9999999809, 1.33230799934103E-8},
   {2.17237099975343E-8, -0.0429383201, 1.04293828050000}};

  double cmatrix_tmp [3][3] =
  {{0.0, 0.0, 0.0},
   {0.0, 0.0, 0.0},
   {0.0, 0.0, 0.0}};
  int i, j, k;

  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
    {
      for (k = 0; k < 3; k++)
        cmatrix_tmp[i][j] += rgb_aRGB[i][k] * aRGB_cam[k][j];
      cmatrix[i][j] = (float)cmatrix_tmp[i][j];
    }
}

void CLASS romm_coeff(float romm_cam[3][3])
{
  static const float rgb_romm[3][3] = /* ROMM == Kodak ProPhoto */
      {{ 2.034193, -0.727420, -0.306766},
       {-0.228811,  1.231729, -0.002922},
       {-0.008565, -0.153273,  1.161839}};
  int i, j, k;

  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
      for (cmatrix[i][j] = k = 0; k < 3; k++)
        cmatrix[i][j] += rgb_romm[i][k] * romm_cam[k][j];
}

void CLASS parse_mos(int offset)
{
  char data[40];
  int skip, from, i, c, neut[4], planes = 0, frot = 0;
  static const char *mod[] = {"",
                              "DCB2",
                              "Volare",
                              "Cantare",
                              "CMost",
                              "Valeo 6",
                              "Valeo 11",
                              "Valeo 22",
                              "Valeo 11p",
                              "Valeo 17",
                              "",
                              "Aptus 17",
                              "Aptus 22",
                              "Aptus 75",
                              "Aptus 65",
                              "Aptus 54S",
                              "Aptus 65S",
                              "Aptus 75S",
                              "AFi 5",
                              "AFi 6",
                              "AFi 7",
                              "AFi-II 7",
                              "Aptus-II 7",
                              "",
                              "Aptus-II 6",
                              "",
                              "",
                              "Aptus-II 10",
                              "Aptus-II 5",
                              "",
                              "",
                              "",
                              "",
                              "Aptus-II 10R",
                              "Aptus-II 8",
                              "",
                              "Aptus-II 12",
                              "",
                              "AFi-II 12"};
  float romm_cam[3][3];

  fseek(ifp, offset, SEEK_SET);
  while (1)
  {
    if (get4() != 0x504b5453)
      break;
    get4();
    fread(data, 1, 40, ifp);
    skip = get4();
    from = ftell(ifp);

#ifdef LIBRAW_LIBRARY_BUILD
    if (!strcmp(data, "CameraObj_camera_type"))
    {
      stmread(imgdata.lens.makernotes.body, skip, ifp);
    }
    if (!strcmp(data, "back_serial_number"))
    {
      char buffer[sizeof(imgdata.shootinginfo.BodySerial)];
      char *words[4];
      int nwords;
      stmread(buffer, skip, ifp);
      nwords = getwords(buffer, words, 4, sizeof(imgdata.shootinginfo.BodySerial));
      strcpy(imgdata.shootinginfo.BodySerial, words[0]);
    }
    if (!strcmp(data, "CaptProf_serial_number"))
    {
      char buffer[sizeof(imgdata.shootinginfo.InternalBodySerial)];
      char *words[4];
      int nwords;
      stmread(buffer, skip, ifp);
      nwords = getwords(buffer, words, 4, sizeof(imgdata.shootinginfo.InternalBodySerial));
      strcpy(imgdata.shootinginfo.InternalBodySerial, words[0]);
    }
#endif

    if (!strcmp(data, "JPEG_preview_data"))
    {
      thumb_offset = from;
      thumb_length = skip;
    }
    if (!strcmp(data, "icc_camera_profile"))
    {
      profile_offset = from;
      profile_length = skip;
    }
    if (!strcmp(data, "ShootObj_back_type"))
    {
      fscanf(ifp, "%d", &i);
      if ((unsigned)i < sizeof mod / sizeof(*mod))
        strcpy(model, mod[i]);
    }
    if (!strcmp(data, "icc_camera_to_tone_matrix"))
    {
      for (i = 0; i < 9; i++)
        ((float *)romm_cam)[i] = int_to_float(get4());
      romm_coeff(romm_cam);
    }
    if (!strcmp(data, "CaptProf_color_matrix"))
    {
      for (i = 0; i < 9; i++)
        fscanf(ifp, "%f", (float *)romm_cam + i);
      romm_coeff(romm_cam);
    }
    if (!strcmp(data, "CaptProf_number_of_planes"))
      fscanf(ifp, "%d", &planes);
    if (!strcmp(data, "CaptProf_raw_data_rotation"))
      fscanf(ifp, "%d", &flip);
    if (!strcmp(data, "CaptProf_mosaic_pattern"))
      FORC4
      {
        fscanf(ifp, "%d", &i);
        if (i == 1)
          frot = c ^ (c >> 1);
      }
    if (!strcmp(data, "ImgProf_rotation_angle"))
    {
      fscanf(ifp, "%d", &i);
      flip = i - flip;
    }
    if (!strcmp(data, "NeutObj_neutrals") && !cam_mul[0])
    {
      FORC4 fscanf(ifp, "%d", neut + c);
      FORC3
        if(neut[c + 1])
      		cam_mul[c] = (float)neut[0] / neut[c + 1];
    }
    if (!strcmp(data, "Rows_data"))
      load_flags = get4();
    parse_mos(from);
    fseek(ifp, skip + from, SEEK_SET);
  }
  if (planes)
    filters = (planes == 1) * 0x01010101U * (uchar) "\x94\x61\x16\x49"[(flip / 90 + frot) & 3];
}

void CLASS linear_table(unsigned len)
{
  int i;
  if (len > 0x10000)
    len = 0x10000;
  else if(len < 1)
    return;
  read_shorts(curve, len);
  for (i = len; i < 0x10000; i++)
    curve[i] = curve[i - 1];
  maximum = curve[len < 0x1000 ? 0xfff : len - 1];
}

#ifdef LIBRAW_LIBRARY_BUILD

void CLASS Kodak_KDC_WBtags(int wb, int wbi)
{
  int c;
  FORC3 imgdata.color.WB_Coeffs[wb][c] = get4();
  imgdata.color.WB_Coeffs[wb][3] = imgdata.color.WB_Coeffs[wb][1];
  if (wbi == wb)
    FORC4 cam_mul[c] = imgdata.color.WB_Coeffs[wb][c];
  return;
}

void CLASS Kodak_DCR_WBtags(int wb, unsigned type, int wbi)
{
  float mul[3] = {1.0f, 1.0f, 1.0f}, num, mul2;
  int c;
  FORC3 mul[c] = (num = getreal(type)) <= 0.001f ? 1.0f : num;
  imgdata.color.WB_Coeffs[wb][1] = imgdata.color.WB_Coeffs[wb][3] = mul[1];
  mul2 = mul[1] * mul[1];
  imgdata.color.WB_Coeffs[wb][0] = mul2 / mul[0];
  imgdata.color.WB_Coeffs[wb][2] = mul2 / mul[2];
  if (wbi == wb)
    FORC4 cam_mul[c] = imgdata.color.WB_Coeffs[wb][c];
  return;
}

short CLASS KodakIllumMatrix (unsigned type, float *romm_camIllum)
{
  int c, j, romm_camTemp[9], romm_camScale[3];
  if (type == 10) {
      for (j = 0; j < 9; j++)
        ((float *)romm_camIllum)[j] = getreal(type);
      return 1;

  } else if (type == 9) {
      FORC3 {
        romm_camScale[c] = 0;
        for (j = 0; j < 3; j++) {
            romm_camTemp[c * 3 + j] = get4();
            romm_camScale[c] += romm_camTemp[c * 3 + j];
        }
      }
      if ((romm_camScale[0] > 0x1fff) &&
          (romm_camScale[1] > 0x1fff) &&
          (romm_camScale[2] > 0x1fff)) {
        FORC3 for (j = 0; j < 3; j++)
          ((float *)romm_camIllum)[c * 3 + j] =
            ((float)romm_camTemp[c * 3 + j]) / ((float)romm_camScale[c]);
          return 1;
      }
  }
  return 0;
}

/* Thanks to Alexey Danilchenko for wb as-shot parsing code */
void CLASS parse_kodak_ifd(int base)
{
  unsigned entries, tag, type, len, save;
  int j, c, wbi = -1;
  float mul[3] = {1, 1, 1}, num;

  static const int wbtag_kdc[] = {
    LIBRAW_WBI_Auto,         // 64037 / 0xfa25
    LIBRAW_WBI_Fluorescent,  // 64040 / 0xfa28
    LIBRAW_WBI_Tungsten,     // 64039 / 0xfa27
    LIBRAW_WBI_Daylight,     // 64041 / 0xfa29
    -1,
    -1,
    LIBRAW_WBI_Shade         // 64042 / 0xfa2a
  };

  static const int wbtag_dcr[] = {
    LIBRAW_WBI_Daylight,     // 2120 / 0x0848
    LIBRAW_WBI_Tungsten,     // 2121 / 0x0849
    LIBRAW_WBI_Fluorescent,  // 2122 / 0x084a
    LIBRAW_WBI_Flash,        // 2123 / 0x084b
    LIBRAW_WBI_Custom,       // 2124 / 0x084c
    LIBRAW_WBI_Auto          // 2125 / 0x084d
  };

//  int a_blck = 0;

  entries = get2();
  if (entries > 1024)
    return;
  INT64 fsize = ifp->size();
  while (entries--)
  {
    tiff_get(base, &tag, &type, &len, &save);
    INT64 savepos = ftell(ifp);
    if (len > 8 && len + savepos > 2 * fsize)
    {
      fseek(ifp, save, SEEK_SET); // Recover tiff-read position!!
      continue;
    }
    if (callbacks.exif_cb)
    {
      callbacks.exif_cb(callbacks.exifparser_data, tag | 0x20000, type, len, order, ifp,base);
      fseek(ifp, savepos, SEEK_SET);
    }
    if (tag == 1003)       // 1003
      imgdata.sizes.raw_crop.cleft = get2();
    else if (tag == 1004)  // 1004
      imgdata.sizes.raw_crop.ctop = get2();
    else if (tag == 1005)  // 1005
      imgdata.sizes.raw_crop.cwidth = get2();
    else if (tag == 1006)  // 1006
      imgdata.sizes.raw_crop.cheight = get2();
    else if (tag == 1007)  // 1007 / 0x03ef
    {
      if (!strcmp(model, "EOS D2000C"))
        black = get2();
      else
        imgdata.makernotes.kodak.BlackLevelTop = get2();
    }
    else if (tag == 1008)  // 1008 / 0x03f0
    {
      if (!strcmp(model, "EOS D2000C"))
      {
        if (black) // already set by tag 1007 (0x03ef)
          black = (black + get2()) / 2;
        else
          black = get2();
      }
      else
        imgdata.makernotes.kodak.BlackLevelBottom = get2();
    }
    else if (tag == 1011)  // 1011
      imgdata.other.FlashEC = getreal(type);

    else if (tag == 1020)  // 1020
    {
      wbi = getint(type);
      if ((wbi >= 0) && (wbi < 6) && (wbi != -2)) wbi = wbtag_dcr[wbi];
    }
    else if (tag == 1021 && len == 72)     // 1021
    { /* WB set in software */
      fseek(ifp, 40, SEEK_CUR);
      FORC3 cam_mul[c] = 2048.0 / fMAX(1.0f, get2());
      wbi = -2;
    }

    else if ((tag == 1030) && (len == 1))  // 1030
      imgdata.other.CameraTemperature = getreal(type);
    else if ((tag == 1043) && (len == 1))  // 1043
      imgdata.other.SensorTemperature = getreal(type);
    else if (tag == 0x0848)  // 2120
      Kodak_DCR_WBtags(LIBRAW_WBI_Daylight, type, wbi);
    else if (tag == 0x0849)  // 2121
      Kodak_DCR_WBtags(LIBRAW_WBI_Tungsten, type, wbi);
    else if (tag == 0x084a)  // 2122
      Kodak_DCR_WBtags(LIBRAW_WBI_Fluorescent, type, wbi);
    else if (tag == 0x084b)  // 2123
      Kodak_DCR_WBtags(LIBRAW_WBI_Flash, type, wbi);
    else if (tag == 0x084c)  // 2124
      Kodak_DCR_WBtags(LIBRAW_WBI_Custom, type, wbi);
    else if (tag == 0x084d)  // 2125
    {
      if (wbi == -1) wbi = LIBRAW_WBI_Auto;
      Kodak_DCR_WBtags(LIBRAW_WBI_Auto, type, wbi);
    }
    else if (tag == 0x0903)  // 2307
      iso_speed = getreal(type);
    else if (tag == 2317)    // 2317
      linear_table(len);
    else if (tag == 0x09ce)  // 2510
      stmread(imgdata.shootinginfo.InternalBodySerial, len, ifp);
    else if (tag == 0x0e92)  // 3730
    {
      imgdata.makernotes.kodak.val018percent = get2();
      imgdata.color.linear_max[0] = imgdata.color.linear_max[1] =
          imgdata.color.linear_max[2] = imgdata.color.linear_max[3] =
          (int)(((float)imgdata.makernotes.kodak.val018percent) / 18.0f * 170.0f);
    }
    else if (tag == 0x0e93)  // 3731
      imgdata.color.linear_max[0] = imgdata.color.linear_max[1] =
          imgdata.color.linear_max[2] = imgdata.color.linear_max[3] =
          imgdata.makernotes.kodak.val170percent = get2();
    else if (tag == 0x0e94)  // 3732
      imgdata.makernotes.kodak.val100percent = get2();
/*
    else if (tag == 6020)    // 6020
      iso_speed = getint(type);
*/
    else if (tag == 0xfa00)  // 64000
      stmread(imgdata.shootinginfo.BodySerial, len, ifp);
    else if (tag == 0xfa0d)  // 64013
    {
      wbi = fgetc(ifp);
      if ((wbi >= 0) && (wbi < 7)) wbi = wbtag_kdc[wbi];
    }
    else if (tag == 0xfa13)  // 64019
      width = getint(type);
    else if (tag == 0xfa14)  // 64020
      height = (getint(type) + 1) & -2;
/*
      height = getint(type);

    else if (tag == 0xfa16)  // 64022
      raw_width = get2();
    else if (tag == 0xfa17)  // 64023
      raw_height = get2();
*/
    else if (tag == 0xfa18)  // 64024
    {
      imgdata.makernotes.kodak.offset_left = getint(8);
      if (type != 8)
        imgdata.makernotes.kodak.offset_left += 1;
    }
    else if (tag == 0xfa19)  // 64025
    {
      imgdata.makernotes.kodak.offset_top = getint(8);
      if (type != 8)
        imgdata.makernotes.kodak.offset_top += 1;
    }

    else if (tag == 0xfa25)  // 64037
      Kodak_KDC_WBtags(LIBRAW_WBI_Auto, wbi);
    else if (tag == 0xfa27)  // 64039
      Kodak_KDC_WBtags(LIBRAW_WBI_Tungsten, wbi);
    else if (tag == 0xfa28)  // 64040
      Kodak_KDC_WBtags(LIBRAW_WBI_Fluorescent, wbi);
    else if (tag == 0xfa29)  // 64041
      Kodak_KDC_WBtags(LIBRAW_WBI_Daylight, wbi);
    else if (tag == 0xfa2a)  // 64042
      Kodak_KDC_WBtags(LIBRAW_WBI_Shade, wbi);

    else if (tag == 0xfa31)  // 64049
      imgdata.sizes.raw_crop.cwidth = get2();
    else if (tag == 0xfa32)  // 64050
      imgdata.sizes.raw_crop.cheight = get2();
    else if (tag == 0xfa3e)  // 64062
      imgdata.sizes.raw_crop.cleft = get2();
    else if (tag == 0xfa3f)  // 64063
      imgdata.sizes.raw_crop.ctop = get2();

    else if (((tag == 0x07e4) || (tag == 0xfb01)) && (len == 9))  // 2020 or 64257
    {
      if (KodakIllumMatrix (type, (float *)imgdata.makernotes.kodak.romm_camDaylight))
      {
        romm_coeff(imgdata.makernotes.kodak.romm_camDaylight);
      }
    }
    else if (((tag == 0x07e5) || (tag == 0xfb02)) && (len == 9))  // 2021 or 64258
      KodakIllumMatrix (type, (float *)imgdata.makernotes.kodak.romm_camTungsten);
    else if (((tag == 0x07e6) || (tag == 0xfb03)) && (len == 9))  // 2022 or 64259
      KodakIllumMatrix (type, (float *)imgdata.makernotes.kodak.romm_camFluorescent);
    else if (((tag == 0x07e7) || (tag == 0xfb04)) && (len == 9))  // 2023 or 64260
      KodakIllumMatrix (type, (float *)imgdata.makernotes.kodak.romm_camFlash);
    else if (((tag == 0x07e8) || (tag == 0xfb05)) && (len == 9))  // 2024 or 64261
      KodakIllumMatrix (type, (float *)imgdata.makernotes.kodak.romm_camCustom);
    else if (((tag == 0x07e9) || (tag == 0xfb06)) && (len == 9))  // 2025 or 64262
      KodakIllumMatrix (type, (float *)imgdata.makernotes.kodak.romm_camAuto);

    fseek(ifp, save, SEEK_SET);
  }
}
#else
void CLASS parse_kodak_ifd(int base)
{
  unsigned entries, tag, type, len, save;
  int i, c, wbi = -2, wbtemp = 6500;
  float mul[3] = {1, 1, 1}, num;
  static const int wbtag_kdc[] = {64037,  64040,  64039,  64041, -1, -1,  64042};
                           // 0xfa25, 0xfa28, 0xfa27, 0xfa29, -1, -1, 0xfa2a

  entries = get2();
  if (entries > 1024)
    return;
  while (entries--)
  {
    tiff_get(base, &tag, &type, &len, &save);
    if (tag == 1020)
      wbi = getint(type);
    if (tag == 1021 && len == 72)
    { /* WB set in software */
      fseek(ifp, 40, SEEK_CUR);
      FORC3 cam_mul[c] = 2048.0 / fMAX(1.0, get2());
      wbi = -2;
    }
    if (tag == 2118)
      wbtemp = getint(type);
    if (tag == 2120 + wbi && wbi >= 0)
      FORC3 cam_mul[c] = 2048.0 / fMAX(1.0, getreal(type));
    if (tag == 2130 + wbi)
      FORC3 mul[c] = getreal(type);
    if (tag == 2140 + wbi && wbi >= 0)
      FORC3
      {
        for (num = i = 0; i < 4; i++)
          num += getreal(type) * pow(wbtemp / 100.0, i);
        cam_mul[c] = 2048 / fMAX(1.0, (num * mul[c]));
      }
    if (tag == 2317)
      linear_table(len);
    if (tag == 6020)
      iso_speed = getint(type);
    if (tag == 64013)
      wbi = fgetc(ifp);
    if ((unsigned)wbi < 7 && tag == wbtag_kdc[wbi])
      FORC3 cam_mul[c] = get4();
    if (tag == 64019)
      width = getint(type);
    if (tag == 64020)
      height = (getint(type) + 1) & -2;
    fseek(ifp, save, SEEK_SET);
  }
}
#endif
//@end COMMON

void CLASS parse_minolta(int base);
int CLASS parse_tiff(int base);

//@out COMMON
int CLASS parse_tiff_ifd(int base)
{
  unsigned entries, tag, type, len, plen = 16, save;
  int ifd, use_cm = 0, cfa, i, j, c, ima_len = 0;
  char *cbuf, *cp;
  uchar cfa_pat[16], cfa_pc[] = {0, 1, 2, 3}, tab[256];
  double fm[3][4], cc[4][4], cm[4][3], cam_xyz[4][3], num;
  double ab[] = {1, 1, 1, 1}, asn[] = {0, 0, 0, 0}, xyz[] = {1, 1, 1};
  unsigned sony_curve[] = {0, 0, 0, 0, 0, 4095};
  unsigned *buf, sony_offset = 0, sony_length = 0, sony_key = 0;
  struct jhead jh;
  int pana_raw = 0;
#ifndef LIBRAW_LIBRARY_BUILD
  FILE *sfp;
#endif

#ifdef LIBRAW_LIBRARY_BUILD
  ushort *rafdata;
#endif

  if (tiff_nifds >= sizeof tiff_ifd / sizeof tiff_ifd[0])
    return 1;
  ifd = tiff_nifds++;
  for (j = 0; j < 4; j++)
    for (i = 0; i < 4; i++)
      cc[j][i] = i == j;
  entries = get2();
  if (entries > 512)
    return 1;
#ifdef LIBRAW_LIBRARY_BUILD
  INT64 fsize = ifp->size();
#endif

  while (entries--)
  {
    tiff_get(base, &tag, &type, &len, &save);
#ifdef LIBRAW_LIBRARY_BUILD
    INT64 savepos = ftell(ifp);
    if (len > 8 && savepos + len > 2 * fsize)
    {
      fseek(ifp, save, SEEK_SET); // Recover tiff-read position!!
      continue;
    }
    if (callbacks.exif_cb)
    {
      callbacks.exif_cb(callbacks.exifparser_data, tag | (pana_raw ? 0x30000 : ((ifd + 1) << 20)),
                        type, len, order, ifp,base);
      fseek(ifp, savepos, SEEK_SET);
    }
#endif
    switch (tag)
    {
    case 1:
      if (len == 4)
        pana_raw = get4();
        if (!imgdata.makernotes.panasonic.is_pana_raw) imgdata.makernotes.panasonic.is_pana_raw = pana_raw;
      break;
    case 5:
      width = get2();
      break;
    case 6:
      height = get2();
      break;
    case 7:
      width += get2();
      break;
    case 9:
      if ((i = get2()))
        filters = i;
      break;
#ifdef LIBRAW_LIBRARY_BUILD
    case 10:
      if (pana_raw && len == 1 && type == 3)
      {
        pana_bpp = get2();
      }
    break;
#endif
    case 14:
    case 15:
    case 16:
#ifdef LIBRAW_LIBRARY_BUILD
      if (pana_raw)
      {
        imgdata.color.linear_max[tag - 14] = get2();
        if (tag == 15)
          imgdata.color.linear_max[3] = imgdata.color.linear_max[1];
      }
#endif
      break;
    case 17:
    case 18:
      if (type == 3 && len == 1)
        cam_mul[(tag - 17) * 2] = get2() / 256.0;
      break;
#ifdef LIBRAW_LIBRARY_BUILD
    case 19:
      if (pana_raw)
      {
        ushort nWB, cnt, tWB;
        nWB = get2();
        if (nWB > 0x100)
          break;
        for (cnt = 0; cnt < nWB; cnt++)
        {
          tWB = get2();
          if (tWB < 0x100)
          {
            imgdata.color.WB_Coeffs[tWB][0] = get2();
            imgdata.color.WB_Coeffs[tWB][2] = get2();
            imgdata.color.WB_Coeffs[tWB][1] = imgdata.color.WB_Coeffs[tWB][3] = 0x100;
          }
          else
            get4();
        }
      }
      break;

    case 0x0120:
      if (pana_raw)
      {
        unsigned sorder = order;
        unsigned long sbase = base;
        base = ftell(ifp);
        order = get2();
        fseek(ifp, 2, SEEK_CUR);
        fseek(ifp, get4()-8, SEEK_CUR);
        parse_tiff_ifd (base);
        base = sbase;
        order = sorder;
      }
    break;
    case 0x1203: // in 0x0120
      if (imgdata.lens.FocalLengthIn35mmFormat < 0.65f)
        imgdata.lens.FocalLengthIn35mmFormat = get2();
    break;
    case 0x3420: // in 0x0120
      if (imgdata.makernotes.panasonic.is_pana_raw)
      {
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][0] = get2();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][1] = imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][3] = 1024.0f;
      }
    break;
    case 0x3421: // in 0x0120
      if (imgdata.makernotes.panasonic.is_pana_raw)
      {
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][2] = get2();
      }
    break;
    case 0x011c:
      if (pana_raw) {
        int n = get2();
        if (n >= 1024) imgdata.makernotes.panasonic.gamma = (float)n / 1024.0f;
        else if (n >= 256) imgdata.makernotes.panasonic.gamma = (float)n / 256.0f;
        else imgdata.makernotes.panasonic.gamma = (float)n / 100.0f;
      }
    break;
    case 0x0121:
      if (pana_raw)
      { /* 0 is Off, 65536 is Pixel Shift */
        imgdata.makernotes.panasonic.Multishot = get4();
      }
    break;
    case 0x2009:
      if ((pana_encoding == 4) || (pana_encoding == 5))
      {
        int n = MIN (8, len);
        int permut[8] = {3, 2, 1, 0, 3+4, 2+4, 1+4, 0+4};

        imgdata.makernotes.panasonic.BlackLevelDim = len;

        for (int i=0; i < n; i++)
        {
          imgdata.makernotes.panasonic.BlackLevel[permut[i]] =
            (float) (get2()) / (float) (powf(2.f, 14.f-pana_bpp));
        }
      }
      break;
#endif

    case 23:
      if (type == 3)
        iso_speed = get2();
      break;
    case 28:
    case 29:
    case 30:
#ifdef LIBRAW_LIBRARY_BUILD
      if (pana_raw && len == 1 && type == 3)
      {
        pana_black[tag - 28] = get2();
      }
      else
#endif
      {
        cblack[tag - 28] = get2();
        cblack[3] = cblack[1];
      }
      break;
    case 36:
    case 37:
    case 38:
      cam_mul[tag - 36] = get2();
      break;
    case 39:
#ifdef LIBRAW_LIBRARY_BUILD
      if (pana_raw)
      {
        ushort nWB, cnt, tWB;
        nWB = get2();
        if (nWB > 0x100)
          break;
        for (cnt = 0; cnt < nWB; cnt++)
        {
          tWB = get2();
          if (tWB < 0x100)
          {
            imgdata.color.WB_Coeffs[tWB][0] = get2();
            imgdata.color.WB_Coeffs[tWB][1] = imgdata.color.WB_Coeffs[tWB][3] = get2();
            imgdata.color.WB_Coeffs[tWB][2] = get2();
          }
          else
            fseek(ifp, 6, SEEK_CUR);
        }
      }
      break;
#endif
      if (len < 50 || cam_mul[0]>0.001f)
        break;
      fseek(ifp, 12, SEEK_CUR);
      FORC3 cam_mul[c] = get2();
      break;
#ifdef LIBRAW_LIBRARY_BUILD
    case 45:  /* pana_encoding:
                 not used - DMC-LX1/FZ30/FZ50/L1/LX1/LX2
                 2 - RAW DMC-FZ8/FZ18
                 3 - RAW DMC-L10
                 4 - RW2 for most other models, including G9 normal resolution and YUNEEC CGO4
                     (must add 15 to black levels for RawFormat == 4)
                 5 - RW2 DC-GH5s; G9 in HiRes mode
                 6 - RW2 DC-S1, DC-S1r
              */
      if (pana_raw && len == 1 && type == 3)
      {
        pana_encoding = get2();
      }
      break;
#endif
    case 46:
      if (type != 7 || fgetc(ifp) != 0xff || fgetc(ifp) != 0xd8)
        break;
      thumb_offset = ftell(ifp) - 2;
      thumb_length = len;
      break;
    case 61440: /* Fuji HS10 table */
      fseek(ifp, get4() + base, SEEK_SET);
      parse_tiff_ifd(base);
      break;
    case 2:  /* ImageWidth */
    case 256:
    case 61441: /* Fuji RAF IFD 0xf001 RawImageFullWidth */
      tiff_ifd[ifd].t_width = getint(type);
      break;
    case 3: /* ImageHeight */
    case 257:
    case 61442: /* Fuji RAF IFD 0xf002 RawImageFullHeight */
      tiff_ifd[ifd].t_height = getint(type);
      break;
    case 258:   /* BitsPerSample */
    case 61443: /* Fuji RAF IFD 0xf003 */
      tiff_ifd[ifd].samples = len & 7;
      tiff_ifd[ifd].bps = getint(type);
      if (tiff_bps < tiff_ifd[ifd].bps)
        tiff_bps = tiff_ifd[ifd].bps;
      break;
    case 61446: /* Fuji RAF IFD 0xf006 */
      raw_height = 0;
      if (tiff_ifd[ifd].bps > 12)
        break;
      load_raw = &CLASS packed_load_raw;
      load_flags = get4() ? 24 : 80;
      break;
    case 259: /* Compression */
      tiff_ifd[ifd].comp = getint(type);
      break;
    case 262: /* PhotometricInterpretation */
      tiff_ifd[ifd].phint = get2();
      break;
    case 270: /* ImageDescription */
      fread(desc, 512, 1, ifp);
      break;
    case 271: /* Make */
      fgets(make, 64, ifp);
      break;
    case 272: /* Model */
      fgets(model, 64, ifp);
      break;
#ifdef LIBRAW_LIBRARY_BUILD
    case 278:
      tiff_ifd[ifd].rows_per_strip = getint(type);
      break;
#endif
    case 280: /* Panasonic RW2 offset */
      if (type != 4)
        break;
      if (!strncmp(model, "D-Lux 7", 7)) {
        imgdata.lens.makernotes.CameraMount =
          imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_FixedLens;
        imgdata.lens.makernotes.CameraFormat =
          imgdata.lens.makernotes.LensFormat = LIBRAW_FORMAT_FT;
      }
      load_raw = &CLASS panasonic_load_raw;
      load_flags = 0x2008;
    case 273: /* StripOffset */
#ifdef LIBRAW_LIBRARY_BUILD
      if (len > 1 && len < 16384)
      {
        off_t sav = ftell(ifp);
        tiff_ifd[ifd].strip_offsets = (int *)calloc(len, sizeof(int));
        tiff_ifd[ifd].strip_offsets_count = len;
        for (int i = 0; i < len; i++)
          tiff_ifd[ifd].strip_offsets[i] = get4() + base;
        fseek(ifp, sav, SEEK_SET); // restore position
      }
/* fallback */
#endif
    case 513: /* JpegIFOffset */
    case 61447:
      tiff_ifd[ifd].offset = get4() + base;
      if (!tiff_ifd[ifd].bps && tiff_ifd[ifd].offset > 0)
      {
        fseek(ifp, tiff_ifd[ifd].offset, SEEK_SET);
        if (ljpeg_start(&jh, 1))
        {
          tiff_ifd[ifd].comp = 6;
          tiff_ifd[ifd].t_width = jh.wide;
          tiff_ifd[ifd].t_height = jh.high;
          tiff_ifd[ifd].bps = jh.bits;
          tiff_ifd[ifd].samples = jh.clrs;
          if (!(jh.sraw || (jh.clrs & 1)))
            tiff_ifd[ifd].t_width *= jh.clrs;
          if ((tiff_ifd[ifd].t_width > 4 * tiff_ifd[ifd].t_height) & ~jh.clrs)
          {
            tiff_ifd[ifd].t_width /= 2;
            tiff_ifd[ifd].t_height *= 2;
          }
          i = order;
          parse_tiff(tiff_ifd[ifd].offset + 12);
          order = i;
        }
      }
      break;
    case 274: /* Orientation */
      tiff_ifd[ifd].t_flip = "50132467"[get2() & 7] - '0';
      break;
    case 277: /* SamplesPerPixel */
      tiff_ifd[ifd].samples = getint(type) & 7;
      break;
    case 279: /* StripByteCounts */
#ifdef LIBRAW_LIBRARY_BUILD
      if (len > 1 && len < 16384)
      {
        off_t sav = ftell(ifp);
        tiff_ifd[ifd].strip_byte_counts = (int *)calloc(len, sizeof(int));
        tiff_ifd[ifd].strip_byte_counts_count = len;
        for (int i = 0; i < len; i++)
          tiff_ifd[ifd].strip_byte_counts[i] = get4();
        fseek(ifp, sav, SEEK_SET); // restore position
      }
/* fallback */
#endif
    case 514:
    case 61448:
      tiff_ifd[ifd].bytes = get4();
      break;
    case 61454: // FujiFilm "As Shot"
      FORC3 cam_mul[(4 - c) % 3] = getint(type);
      break;
    case 305:
    case 11: /* Software */
      if ((pana_raw) && (tag == 11) && (type == 3))
      {
#ifdef LIBRAW_LIBRARY_BUILD
        imgdata.makernotes.panasonic.Compression = get2();
#endif
        break;
      }
      fgets(software, 64, ifp);
      if (!strncmp(software, "Adobe", 5) || !strncmp(software, "dcraw", 5) || !strncmp(software, "UFRaw", 5) ||
          !strncmp(software, "Bibble", 6) || !strcmp(software, "Digital Photo Professional"))
        is_raw = 0;
      break;
    case 306: /* DateTime */
      get_timestamp(0);
      break;
    case 315: /* Artist */
      fread(artist, 64, 1, ifp);
      break;
    case 317:
      tiff_ifd[ifd].predictor = getint(type);
      break;
    case 322: /* TileWidth */
      tiff_ifd[ifd].t_tile_width = getint(type);
      break;
    case 323: /* TileLength */
      tiff_ifd[ifd].t_tile_length = getint(type);
      break;
    case 324: /* TileOffsets */
      tiff_ifd[ifd].offset = len > 1 ? ftell(ifp) : get4();
      if (len == 1)
        tiff_ifd[ifd].t_tile_width = tiff_ifd[ifd].t_tile_length = 0;
      if (len == 4)
      {
        load_raw = &CLASS sinar_4shot_load_raw;
        is_raw = 5;
      }
      break;
    case 325:
      tiff_ifd[ifd].bytes = len > 1 ? ftell(ifp) : get4();
      break;
    case 330: /* SubIFDs */
      if (!strcmp(model, "DSLR-A100") && tiff_ifd[ifd].t_width == 3872)
      {
        load_raw = &CLASS sony_arw_load_raw;
        data_offset = get4() + base;
        ifd++;
#ifdef LIBRAW_LIBRARY_BUILD
        if (ifd >= sizeof tiff_ifd / sizeof tiff_ifd[0])
          throw LIBRAW_EXCEPTION_IO_CORRUPT;
#endif
        break;
      }
#ifdef LIBRAW_LIBRARY_BUILD
      if (!strncmp(make, "Hasselblad", 10) && libraw_internal_data.unpacker_data.hasselblad_parser_flag)
      {
        fseek(ifp, ftell(ifp) + 4, SEEK_SET);
        fseek(ifp, get4() + base, SEEK_SET);
        parse_tiff_ifd(base);
        break;
      }
#endif
      if (len > 1000)
        len = 1000; /* 1000 SubIFDs is enough */
      while (len--)
      {
        i = ftell(ifp);
        fseek(ifp, get4() + base, SEEK_SET);
        if (parse_tiff_ifd(base))
          break;
        fseek(ifp, i + 4, SEEK_SET);
      }
      break;
    case 339:
      tiff_ifd[ifd].sample_format = getint(type);
      break;
    case 400:
      strcpy(make, "Sarnoff");
      maximum = 0xfff;
      break;
#ifdef LIBRAW_LIBRARY_BUILD
    case 700:
      if ((type == 1 || type == 2 || type == 6 || type == 7) && len > 1 && len < 5100000)
      {
        xmpdata = (char *)malloc(xmplen = len + 1);
        fread(xmpdata, len, 1, ifp);
        xmpdata[len] = 0;
      }
      break;
    case 0x7000:
      imgdata.makernotes.sony.SonyRawFileType = get2();
      break;
#endif
    case 28688:
      FORC4 sony_curve[c + 1] = get2() >> 2 & 0xfff;
      for (i = 0; i < 5; i++)
        for (j = sony_curve[i] + 1; j <= sony_curve[i + 1]; j++)
          curve[j] = curve[j - 1] + (1 << i);
      break;
    case 29184:  // Sony SR2Private 0x7200
      sony_offset = get4();
      break;
    case 29185:  // Sony SR2Private 0x7201
      sony_length = get4();
      break;
    case 29217:  // Sony SR2Private 0x7221
      sony_key = get4();
      break;
    case 29264:  // Sony SR2Private 0x7250
      parse_minolta(ftell(ifp));
      raw_width = 0;
      break;
    case 29443:  // Sony SR2SubIFD 0x7303
      FORC4 cam_mul[c ^ (c < 2)] = get2();
      break;
    case 29459:  // Sony SR2SubIFD 0x7313
      FORC4 cam_mul[c ^ (c >> 1)] = get2();
      break;
    case 29456:  // Sony SR2SubIFD 0x7310
      FORC4 cblack[c ^ c >> 1] = get2();
      i = cblack[3];
      FORC3 if (i > cblack[c]) i = cblack[c];
      FORC4 cblack[c] -= i;
      black = i;

#ifdef DCRAW_VERBOSE
      if (verbose)
        fprintf(stderr, _("...Sony black: %u cblack: %u %u %u %u\n"), black, cblack[0], cblack[1], cblack[2],
                cblack[3]);
#endif
      break;
    case 33405: /* Model2 */
      fgets(model2, 64, ifp);
      break;
    case 33421: /* CFARepeatPatternDim */
      if (get2() == 6 && get2() == 6)
        filters = 9;
      break;
    case 33422: /* CFAPattern */
      if (filters == 9)
      {
        FORC(36)((char *)xtrans)[c] = fgetc(ifp) & 3;
        break;
      }
    case 64777: /* Kodak P-series */
      if (len == 36)
      {
        filters = 9;
        colors = 3;
        FORC(36) xtrans[0][c] = fgetc(ifp) & 3;
      }
      else if (len > 0)
      {
        if ((plen = len) > 16)
          plen = 16;
        fread(cfa_pat, 1, plen, ifp);
        for (colors = cfa = i = 0; i < plen && colors < 4; i++)
        {
	  if(cfa_pat[i] > 31) continue; // Skip wrong data
          colors += !(cfa & (1 << cfa_pat[i]));
          cfa |= 1 << cfa_pat[i];
        }
        if (cfa == 070)
          memcpy(cfa_pc, "\003\004\005", 3); /* CMY */
        if (cfa == 072)
          memcpy(cfa_pc, "\005\003\004\001", 4); /* GMCY */
        goto guess_cfa_pc;
      }
      break;
    case 33424:
    case 65024:
      fseek(ifp, get4() + base, SEEK_SET);
      parse_kodak_ifd(base);
      break;
    case 33434: /* ExposureTime */
      tiff_ifd[ifd].t_shutter = shutter = getreal(type);
      break;
    case 33437: /* FNumber */
      aperture = getreal(type);
      break;
#ifdef LIBRAW_LIBRARY_BUILD
    // IB start
    case 0x9400:
      imgdata.other.exifAmbientTemperature = getreal(type);
      if ((imgdata.other.CameraTemperature > -273.15f) && (OlyID == 0x4434353933ULL)) // TG-5
        imgdata.other.CameraTemperature += imgdata.other.exifAmbientTemperature;
      break;
    case 0x9401:
      imgdata.other.exifHumidity = getreal(type);
      break;
    case 0x9402:
      imgdata.other.exifPressure = getreal(type);
      break;
    case 0x9403:
      imgdata.other.exifWaterDepth = getreal(type);
      break;
    case 0x9404:
      imgdata.other.exifAcceleration = getreal(type);
      break;
    case 0x9405:
      imgdata.other.exifCameraElevationAngle = getreal(type);
      break;
    case 0xa405: // FocalLengthIn35mmFormat
      imgdata.lens.FocalLengthIn35mmFormat = get2();
      break;
    case 0xa431: // BodySerialNumber
    case 0xc62f:
      stmread(imgdata.shootinginfo.BodySerial, len, ifp);
      break;
    case 0xa432: // LensInfo, 42034dec, Lens Specification per EXIF standard
      imgdata.lens.MinFocal = getreal(type);
      imgdata.lens.MaxFocal = getreal(type);
      imgdata.lens.MaxAp4MinFocal = getreal(type);
      imgdata.lens.MaxAp4MaxFocal = getreal(type);
      break;
    case 0xa435: // LensSerialNumber
      stmread(imgdata.lens.LensSerial, len, ifp);
      break;
    case 0xc630: // DNG LensInfo, Lens Specification per EXIF standard
      imgdata.lens.MinFocal = getreal(type);
      imgdata.lens.MaxFocal = getreal(type);
      imgdata.lens.MaxAp4MinFocal = getreal(type);
      imgdata.lens.MaxAp4MaxFocal = getreal(type);
      break;
    case 0xa433: // LensMake
      stmread(imgdata.lens.LensMake, len, ifp);
      break;
    case 0xa434: // LensModel
      stmread(imgdata.lens.Lens, len, ifp);
      if (!strncmp(imgdata.lens.Lens, "----", 4))
        imgdata.lens.Lens[0] = 0;
      break;
    case 0x9205:
      imgdata.lens.EXIF_MaxAp = libraw_powf64l(2.0f, (getreal(type) / 2.0f));
      break;
// IB end
#endif
    case 34306: /* Leaf white balance */
      FORC4
      {
      	int q = get2();
	if(q)
      		cam_mul[c ^ 1] = 4096.0 / q;
      }
      break;
    case 34307: /* Leaf CatchLight color matrix */
      fread(software, 1, 7, ifp);
      if (strncmp(software, "MATRIX", 6))
        break;
      colors = 4;
      for (raw_color = i = 0; i < 3; i++)
      {
        FORC4 fscanf(ifp, "%f", &rgb_cam[i][c ^ 1]);
        if (!use_camera_wb)
          continue;
        num = 0;
        FORC4 num += rgb_cam[i][c];
        FORC4 rgb_cam[i][c] /= MAX(1, num);
      }
      break;
    case 34310: /* Leaf metadata */
      parse_mos(ftell(ifp));
    case 34303:
      strcpy(make, "Leaf");
      break;
    case 34665: /* EXIF tag */
      fseek(ifp, get4() + base, SEEK_SET);
      parse_exif(base);
      break;
    case 34853: /* GPSInfo tag */
    {
      unsigned pos;
      fseek(ifp, pos = (get4() + base), SEEK_SET);
      parse_gps(base);
#ifdef LIBRAW_LIBRARY_BUILD
      fseek(ifp, pos, SEEK_SET);
      parse_gps_libraw(base);
#endif
    }
    break;
    case 34675: /* InterColorProfile */
    case 50831: /* AsShotICCProfile */
      profile_offset = ftell(ifp);
      profile_length = len;
      break;
    case 37122: /* CompressedBitsPerPixel */
      kodak_cbpp = get4();
      break;
    case 37386: /* FocalLength */
      focal_len = getreal(type);
      break;
    case 37393: /* ImageNumber */
      shot_order = getint(type);
      break;
    case 37400: /* old Kodak KDC tag */
      for (raw_color = i = 0; i < 3; i++)
      {
        getreal(type);
        FORC3 rgb_cam[i][c] = getreal(type);
      }
      break;
    case 40976:
      strip_offset = get4();
      switch (tiff_ifd[ifd].comp)
      {
      case 32770:
        load_raw = &CLASS samsung_load_raw;
        break;
      case 32772:
        load_raw = &CLASS samsung2_load_raw;
        break;
      case 32773:
        load_raw = &CLASS samsung3_load_raw;
        break;
      }
      break;
    case 46275: /* Imacon tags */
      strcpy(make, "Imacon");
      data_offset = ftell(ifp);
      ima_len = len;
      break;
    case 46279:
      if (!ima_len)
        break;
      fseek(ifp, 38, SEEK_CUR);
    case 46274:
      fseek(ifp, 40, SEEK_CUR);
      raw_width = get4();
      raw_height = get4();
      left_margin = get4() & 7;
      width = raw_width - left_margin - (get4() & 7);
      top_margin = get4() & 7;
      height = raw_height - top_margin - (get4() & 7);
      if (raw_width == 7262 && ima_len == 234317952)
      {
        height = 5412;
        width = 7216;
        left_margin = 7;
        filters = 0;
      }
      else if (raw_width == 7262)
      {
        height = 5444;
        width = 7244;
        left_margin = 7;
      }
      fseek(ifp, 52, SEEK_CUR);
      FORC3 cam_mul[c] = getreal(11);
      fseek(ifp, 114, SEEK_CUR);
      flip = (get2() >> 7) * 90;
      if (width * height * 6 == ima_len)
      {
        if (flip % 180 == 90)
          SWAP(width, height);
        raw_width = width;
        raw_height = height;
        left_margin = top_margin = filters = flip = 0;
      }
      sprintf(model, "Ixpress %d-Mp", height * width / 1000000);
      load_raw = &CLASS imacon_full_load_raw;
      if (filters)
      {
        if (left_margin & 1)
          filters = 0x61616161;
        load_raw = &CLASS unpacked_load_raw;
      }
      maximum = 0xffff;
      break;
    case 50454: /* Sinar tag */
    case 50455:
      if (len < 1 || len > 2560000 || !(cbuf = (char *)malloc(len)))
        break;
#ifndef LIBRAW_LIBRARY_BUILD
      fread(cbuf, 1, len, ifp);
#else
      if (fread(cbuf, 1, len, ifp) != len)
        throw LIBRAW_EXCEPTION_IO_CORRUPT; // cbuf to be free'ed in recycle
#endif
      cbuf[len - 1] = 0;
      for (cp = cbuf - 1; cp && cp < cbuf + len; cp = strchr(cp, '\n'))
        if (!strncmp(++cp, "Neutral ", 8))
          sscanf(cp + 8, "%f %f %f", cam_mul, cam_mul + 1, cam_mul + 2);
      free(cbuf);
      break;
    case 50458:
      if (!make[0])
        strcpy(make, "Hasselblad");
      break;
    case 50459: /* Hasselblad tag */
#ifdef LIBRAW_LIBRARY_BUILD
    if(!libraw_internal_data.unpacker_data.hasselblad_parser_flag)
#endif
    {
#ifdef LIBRAW_LIBRARY_BUILD
      libraw_internal_data.unpacker_data.hasselblad_parser_flag = 1;
#endif
      i = order;
      j = ftell(ifp);
      c = tiff_nifds;
      order = get2();
      fseek(ifp, j + (get2(), get4()), SEEK_SET);
      parse_tiff_ifd(j);
      maximum = 0xffff;
      tiff_nifds = c;
      order = i;
      break;
    }
    case 50706: /* DNGVersion */
      FORC4 dng_version = (dng_version << 8) + fgetc(ifp);
      if (!make[0])
        strcpy(make, "DNG");
      is_raw = 1;
      break;
    case 50708: /* UniqueCameraModel */
#ifdef LIBRAW_LIBRARY_BUILD
      stmread(imgdata.color.UniqueCameraModel, len, ifp);
      imgdata.color.UniqueCameraModel[sizeof(imgdata.color.UniqueCameraModel) - 1] = 0;
#endif
      if (model[0])
        break;
#ifndef LIBRAW_LIBRARY_BUILD
      fgets(make, 64, ifp);
#else
      strncpy(make, imgdata.color.UniqueCameraModel, MIN(len, sizeof(imgdata.color.UniqueCameraModel)));
#endif
      if ((cp = strchr(make, ' ')))
      {
        strcpy(model, cp + 1);
        *cp = 0;
      }
      break;
    case 50710: /* CFAPlaneColor */
      if (filters == 9)
        break;
      if (len > 4)
        len = 4;
      colors = len;
      fread(cfa_pc, 1, colors, ifp);
    guess_cfa_pc:
      FORCC tab[cfa_pc[c]] = c;
      cdesc[c] = 0;
      for (i = 16; i--;)
        filters = filters << 2 | tab[cfa_pat[i % plen]];
      filters -= !filters;
      break;
    case 50711: /* CFALayout */
      if (get2() == 2)
        fuji_width = 1;
      break;
    case 291:
    case 50712: /* LinearizationTable */
#ifdef LIBRAW_LIBRARY_BUILD
      tiff_ifd[ifd].dng_levels.parsedfields |= LIBRAW_DNGFM_LINTABLE;
      tiff_ifd[ifd].lineartable_offset = ftell(ifp);
      tiff_ifd[ifd].lineartable_len = len;
#endif
      linear_table(len);
      break;
    case 50713: /* BlackLevelRepeatDim */
#ifdef LIBRAW_LIBRARY_BUILD
      tiff_ifd[ifd].dng_levels.parsedfields |= LIBRAW_DNGFM_BLACK;
      tiff_ifd[ifd].dng_levels.dng_fcblack[4] =
      tiff_ifd[ifd].dng_levels.dng_cblack[4] =
#endif
          cblack[4] = get2();
#ifdef LIBRAW_LIBRARY_BUILD
      tiff_ifd[ifd].dng_levels.dng_fcblack[5] =
      tiff_ifd[ifd].dng_levels.dng_cblack[5] =
#endif
          cblack[5] = get2();
      if (cblack[4] * cblack[5] > (sizeof(cblack) / sizeof(cblack[0]) - 6))
#ifdef LIBRAW_LIBRARY_BUILD
        tiff_ifd[ifd].dng_levels.dng_fcblack[4] = tiff_ifd[ifd].dng_levels.dng_fcblack[5] =
        tiff_ifd[ifd].dng_levels.dng_cblack[4] = tiff_ifd[ifd].dng_levels.dng_cblack[5] =
#endif
            cblack[4] = cblack[5] = 1;
      break;

#ifdef LIBRAW_LIBRARY_BUILD
    case 0xf00c:
      if (!is_4K_RAFdata) {
        unsigned fwb[4];
        FORC4 fwb[c] = get4();
        if (fwb[3] < 0x100) {
          imgdata.color.WB_Coeffs[fwb[3]][0] = fwb[1];
          imgdata.color.WB_Coeffs[fwb[3]][1] = imgdata.color.WB_Coeffs[fwb[3]][3] = fwb[0];
          imgdata.color.WB_Coeffs[fwb[3]][2] = fwb[2];
          if ((fwb[3] == 17) && (libraw_internal_data.unpacker_data.lenRAFData > 3) &&
              (libraw_internal_data.unpacker_data.lenRAFData < 10240000)) {
            INT64 f_save = ftell(ifp);
            rafdata = (ushort *)malloc(sizeof(ushort) * libraw_internal_data.unpacker_data.lenRAFData);
            fseek(ifp, libraw_internal_data.unpacker_data.posRAFData, SEEK_SET);
            fread(rafdata, sizeof(ushort), libraw_internal_data.unpacker_data.lenRAFData, ifp);
            fseek(ifp, f_save, SEEK_SET);

            uchar *PrivateMknBuf = (uchar *)rafdata;
            int PrivateMknLength = libraw_internal_data.unpacker_data.lenRAFData << 1;
            for (int pos = 0; pos < PrivateMknLength-16; pos++) {
              if (!memcmp (PrivateMknBuf+pos, "TSNERDTS", 8)) {
                imgdata.makernotes.fuji.isTSNERDTS = 1;
                break;
              }
            }

            /* 0xc000 tag version, second ushort; valid if the first ushort is 0 */
            if (!rafdata[0])
              imgdata.makernotes.fuji.RAFDataVersion = rafdata[1];
            int fj, found = 0;
            for (int fi = 0; fi < (libraw_internal_data.unpacker_data.lenRAFData - 3); fi++) { // find Tungsten WB
              if ((fwb[0] == rafdata[fi]) && (fwb[1] == rafdata[fi + 1]) && (fwb[2] == rafdata[fi + 2])) {
                if (rafdata[fi - 15] != fwb[0]) // 15 is offset of Tungsten WB from the first preset, Fine Weather WB
                  continue;
                for (int wb_ind = 0, ofst = fi - 15; wb_ind < nFuji_wb_list1; wb_ind++, ofst += 3) {
                  imgdata.color.WB_Coeffs[Fuji_wb_list1[wb_ind]][1] =
                    imgdata.color.WB_Coeffs[Fuji_wb_list1[wb_ind]][3] = rafdata[ofst];
                  imgdata.color.WB_Coeffs[Fuji_wb_list1[wb_ind]][0] = rafdata[ofst + 1];
                  imgdata.color.WB_Coeffs[Fuji_wb_list1[wb_ind]][2] = rafdata[ofst + 2];
                }
                fi += 0x60;
                for (fj = fi; fj < (fi + 15); fj += 3)
                  if (rafdata[fj] != rafdata[fi]) {
                    found = 1;
                    break;
                  }
                if (found) {
                  fj = fj - 93;
                  for (int iCCT = 0; iCCT < 31; iCCT++) {
                    imgdata.color.WBCT_Coeffs[iCCT][0] = FujiCCT_K[iCCT];
                    imgdata.color.WBCT_Coeffs[iCCT][1] = rafdata[iCCT * 3 + 1 + fj];
                    imgdata.color.WBCT_Coeffs[iCCT][2] = imgdata.color.WBCT_Coeffs[iCCT][4] = rafdata[iCCT * 3 + fj];
                    imgdata.color.WBCT_Coeffs[iCCT][3] = rafdata[iCCT * 3 + 2 + fj];
                  }
                }
                free(rafdata);
                break;
              }
            }
          }
        }
        FORC4 fwb[c] = get4();
        if (fwb[3] < 0x100) {
          imgdata.color.WB_Coeffs[fwb[3]][0] = fwb[1];
          imgdata.color.WB_Coeffs[fwb[3]][1] = imgdata.color.WB_Coeffs[fwb[3]][3] = fwb[0];
          imgdata.color.WB_Coeffs[fwb[3]][2] = fwb[2];
        }
      }
      break;
    case 0xf00d:
      if (!is_4K_RAFdata) {
        FORC3 imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][(4 - c) % 3] = getint(type);
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][3] = imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][1];
//        free(rafdata);
      }
      break;
#endif

#ifdef LIBRAW_LIBRARY_BUILD
    case 50709:
      stmread(imgdata.color.LocalizedCameraModel, len, ifp);
      break;
#endif

    case 61450:
      cblack[4] = cblack[5] = MIN(sqrt((double)len), 64);
    case 50714: /* BlackLevel */
#ifdef LIBRAW_LIBRARY_BUILD
      if (tiff_ifd[ifd].samples > 1 && tiff_ifd[ifd].samples == len) // LinearDNG, per-channel black
      {
        tiff_ifd[ifd].dng_levels.parsedfields |= LIBRAW_DNGFM_BLACK;
        for (i = 0; i < 4 && i < len; i++)
	  {
	    tiff_ifd[ifd].dng_levels.dng_fcblack[i] = getreal(type);
	    tiff_ifd[ifd].dng_levels.dng_cblack[i] = cblack[i] = tiff_ifd[ifd].dng_levels.dng_fcblack[i]+0.5;
	  }

        tiff_ifd[ifd].dng_levels.dng_fblack = tiff_ifd[ifd].dng_levels.dng_black = black = 0;
      }
      else
#endif
          if ((cblack[4] * cblack[5] < 2) && len == 1)
      {
#ifdef LIBRAW_LIBRARY_BUILD
        tiff_ifd[ifd].dng_levels.parsedfields |= LIBRAW_DNGFM_BLACK;
        tiff_ifd[ifd].dng_levels.dng_fblack = getreal(type);
	black = tiff_ifd[ifd].dng_levels.dng_black = tiff_ifd[ifd].dng_levels.dng_fblack;
#else
            black = getreal(type);
#endif
      }
      else if (cblack[4] * cblack[5] <= len)
      {
        FORC(cblack[4] * cblack[5])
#ifdef LIBRAW_LIBRARY_BUILD
	  {
	    tiff_ifd[ifd].dng_levels.dng_fcblack[6+c] = getreal(type);
	    cblack[6+c] = tiff_ifd[ifd].dng_levels.dng_fcblack[6+c];
	  }
#else
	  cblack[6 + c] = getreal(type);
#endif
        black = 0;
        FORC4
        cblack[c] = 0;

#ifdef LIBRAW_LIBRARY_BUILD
        if (tag == 50714)
        {
          tiff_ifd[ifd].dng_levels.parsedfields |= LIBRAW_DNGFM_BLACK;
          FORC(cblack[4] * cblack[5])
	    tiff_ifd[ifd].dng_levels.dng_cblack[6 + c] = cblack[6 + c];
          tiff_ifd[ifd].dng_levels.dng_fblack = 0;
          tiff_ifd[ifd].dng_levels.dng_black = 0;
          FORC4
	    tiff_ifd[ifd].dng_levels.dng_fcblack[c] =
	    tiff_ifd[ifd].dng_levels.dng_cblack[c] = 0;
        }
#endif
      }
      break;
    case 50715: /* BlackLevelDeltaH */
    case 50716: /* BlackLevelDeltaV */
      for (num = i = 0; i < len && i < 65536; i++)
        num += getreal(type);
      if(len>0)
	{
        black += num / len + 0.5;
#ifdef LIBRAW_LIBRARY_BUILD
	tiff_ifd[ifd].dng_levels.dng_fblack += num/float(len);
	tiff_ifd[ifd].dng_levels.dng_black += num / len + 0.5;
	tiff_ifd[ifd].dng_levels.parsedfields |= LIBRAW_DNGFM_BLACK;
#endif
	}
      break;
    case 50717: /* WhiteLevel */
#ifdef LIBRAW_LIBRARY_BUILD
      tiff_ifd[ifd].dng_levels.parsedfields |= LIBRAW_DNGFM_WHITE;
      tiff_ifd[ifd].dng_levels.dng_whitelevel[0] =
#endif
          maximum = getint(type);
#ifdef LIBRAW_LIBRARY_BUILD
      if (tiff_ifd[ifd].samples > 1) // Linear DNG case
        for (i = 1; i < 4 && i < len; i++)
          tiff_ifd[ifd].dng_levels.dng_whitelevel[i] = getint(type);
#endif
      break;
    case 50718: /* DefaultScale */
      {
	float q1 = getreal(type);
	float q2 = getreal(type);
	if(q1 > 0.00001f && q2 > 0.00001f)
	 {
      		pixel_aspect = q1/q2;
      		if (pixel_aspect > 0.995 && pixel_aspect < 1.005)
        		pixel_aspect = 1.0;
	 }
      }
      break;
#ifdef LIBRAW_LIBRARY_BUILD
    case 50719: /* DefaultCropOrigin */
      if (len == 2)
      {
        tiff_ifd[ifd].dng_levels.parsedfields |= LIBRAW_DNGFM_CROPORIGIN;
        tiff_ifd[ifd].dng_levels.default_crop[0] = getreal(type);
        tiff_ifd[ifd].dng_levels.default_crop[1] = getreal(type);
        if (!strncasecmp(make, "SONY", 4))
        {
          imgdata.sizes.raw_crop.cleft = tiff_ifd[ifd].dng_levels.default_crop[0];
          imgdata.sizes.raw_crop.ctop = tiff_ifd[ifd].dng_levels.default_crop[1];
        }
      }
      break;
    case 50720: /* DefaultCropSize */
      if (len == 2)
      {
        tiff_ifd[ifd].dng_levels.parsedfields |= LIBRAW_DNGFM_CROPSIZE;
        tiff_ifd[ifd].dng_levels.default_crop[2] = getreal(type);
        tiff_ifd[ifd].dng_levels.default_crop[3] = getreal(type);
        if (!strncasecmp(make, "SONY", 4))
        {
          imgdata.sizes.raw_crop.cwidth = tiff_ifd[ifd].dng_levels.default_crop[2];
          imgdata.sizes.raw_crop.cheight = tiff_ifd[ifd].dng_levels.default_crop[3];
        }
      }
      break;
#define imSony imgdata.makernotes.sony
    case 0x74c7:
      if ((len == 2) && !strncasecmp(make, "SONY", 4))
      {
        imSony.raw_crop.cleft = get4();
        imSony.raw_crop.ctop = get4();
      }
      break;
    case 0x74c8:
      if ((len == 2) && !strncasecmp(make, "SONY", 4))
      {
        imSony.raw_crop.cwidth = get4();
        imSony.raw_crop.cheight = get4();
      }
      break;
#undef imSony
#endif
#ifdef LIBRAW_LIBRARY_BUILD
    case 50778:
      tiff_ifd[ifd].dng_color[0].illuminant = get2();
      tiff_ifd[ifd].dng_color[0].parsedfields |= LIBRAW_DNGFM_ILLUMINANT;
      break;
    case 50779:
      tiff_ifd[ifd].dng_color[1].illuminant = get2();
      tiff_ifd[ifd].dng_color[1].parsedfields |= LIBRAW_DNGFM_ILLUMINANT;
      break;

#endif
    case 50721: /* ColorMatrix1 */
    case 50722: /* ColorMatrix2 */
    {
      int chan = (len == 9)? 3 : (len == 12?4:0);
#ifdef LIBRAW_LIBRARY_BUILD
      i = tag == 50721 ? 0 : 1;
      if(chan)
         tiff_ifd[ifd].dng_color[i].parsedfields |= LIBRAW_DNGFM_COLORMATRIX;
#endif
      FORC(chan) for (j = 0; j < 3; j++)
      {
#ifdef LIBRAW_LIBRARY_BUILD
        tiff_ifd[ifd].dng_color[i].colormatrix[c][j] =
#endif
            cm[c][j] = getreal(type);
      }
      use_cm = 1;
    }
    break;

    case 0xc714: /* ForwardMatrix1 */
    case 0xc715: /* ForwardMatrix2 */
    {
      int chan = (len == 9)? 3 : (len == 12?4:0);
#ifdef LIBRAW_LIBRARY_BUILD
      i = tag == 0xc714 ? 0 : 1;
      if(chan)
         tiff_ifd[ifd].dng_color[i].parsedfields |= LIBRAW_DNGFM_FORWARDMATRIX;
#endif
      for (j = 0; j < 3; j++)
        FORC(chan)
        {
#ifdef LIBRAW_LIBRARY_BUILD
          tiff_ifd[ifd].dng_color[i].forwardmatrix[j][c] =
#endif
              fm[j][c] = getreal(type);
        }
      }
      break;

    case 50723: /* CameraCalibration1 */
    case 50724: /* CameraCalibration2 */
    {
      int chan = (len == 9)? 3 : (len == 16?4:0);
#ifdef LIBRAW_LIBRARY_BUILD
      j = tag == 50723 ? 0 : 1;
      if(chan)
        tiff_ifd[ifd].dng_color[j].parsedfields |= LIBRAW_DNGFM_CALIBRATION;
#endif
      for (i = 0; i < chan; i++)
        FORC(chan)
        {
#ifdef LIBRAW_LIBRARY_BUILD
          tiff_ifd[ifd].dng_color[j].calibration[i][c] =
#endif
              cc[i][c] = getreal(type);
        }
      }
      break;
    case 50727: /* AnalogBalance */
#ifdef LIBRAW_LIBRARY_BUILD
      if(len>=3)
        tiff_ifd[ifd].dng_levels.parsedfields |= LIBRAW_DNGFM_ANALOGBALANCE;
#endif
      for(c = 0; c < len && c < 4; c++)
      {
#ifdef LIBRAW_LIBRARY_BUILD
        tiff_ifd[ifd].dng_levels.analogbalance[c] =
#endif
            ab[c] = getreal(type);
      }
      break;
    case 50728: /* AsShotNeutral */
#ifdef LIBRAW_LIBRARY_BUILD
      if(len>=3)
        tiff_ifd[ifd].dng_levels.parsedfields |= LIBRAW_DNGFM_ASSHOTNEUTRAL;
      for(c = 0; c < len && c < 4; c++)
         tiff_ifd[ifd].dng_levels.asshotneutral[c] = asn[c] = getreal(type);
#else
      FORCC asn[c] = getreal(type);
#endif
      break;
    case 50729: /* AsShotWhiteXY */
      xyz[0] = getreal(type);
      xyz[1] = getreal(type);
      xyz[2] = 1 - xyz[0] - xyz[1];
      FORC3 xyz[c] /= d65_white[c];
      break;
#ifdef LIBRAW_LIBRARY_BUILD
    case 50730: /* DNG: 0xc62a BaselineExposure */
		tiff_ifd[ifd].dng_levels.parsedfields |= LIBRAW_DNGFM_BASELINEEXPOSURE;
		tiff_ifd[ifd].dng_levels.baseline_exposure = getreal(type);
      break;
    case 50734: /* DNG: 0xc62e LinearResponseLimit */
		tiff_ifd[ifd].dng_levels.parsedfields |= LIBRAW_DNGFM_LINEARRESPONSELIMIT;
		tiff_ifd[ifd].dng_levels.LinearResponseLimit = getreal(type);
      break;
#endif
    // IB start
    case 50740: /* tag 0xc634 : DNG Adobe, DNG Pentax, Sony SR2, DNG Private */
#ifdef LIBRAW_LIBRARY_BUILD
  if (!(imgdata.params.raw_processing_options & LIBRAW_PROCESSING_SKIP_MAKERNOTES)) {
      char mbuf[64];
      unsigned short makernote_found = 0;
      INT64 curr_pos, start_pos = ftell(ifp);
      unsigned MakN_order, m_sorder = order;
      unsigned MakN_length;
      unsigned pos_in_original_raw;
      fread(mbuf, 1, 6, ifp);

      if (!strcmp(mbuf, "Adobe")) {
        order = 0x4d4d; // Adobe header is always in "MM" / big endian
        curr_pos = start_pos + 6;
        while (curr_pos + 8 - start_pos <= len) {
          fread(mbuf, 1, 4, ifp);
          curr_pos += 8;

          if (!strncmp(mbuf, "Pano", 4)) { // PanasonicRaw, yes, they use "Pano" as signature
            parseAdobePanoMakernote();
          }

          if (!strncmp(mbuf, "MakN", 4)) {
            makernote_found = 1;
            MakN_length = get4();
            MakN_order = get2();
            pos_in_original_raw = get4();
            order = MakN_order;

            INT64 save_pos = ifp->tell();
            parse_makernote_0xc634(curr_pos + 6 - pos_in_original_raw, 0, AdobeDNG);

            curr_pos = save_pos + MakN_length - 6;
            fseek(ifp, curr_pos, SEEK_SET);

            fread(mbuf, 1, 4, ifp);
            curr_pos += 8;

            if (!strncmp(mbuf, "Pano ", 4)) {
              parseAdobePanoMakernote();
            }

            if (!strncmp(mbuf, "RAF ", 4)) { // Fujifilm Raw, AdobeRAF
              parseAdobeRAFMakernote ();
            }

            if (!strncmp(mbuf, "SR2 ", 4)) {
              order = 0x4d4d;
              MakN_length = get4();
              MakN_order = get2();
              pos_in_original_raw = get4();
              order = MakN_order;

              unsigned *buf_SR2;
              unsigned entries, tag, type, len, save;
              unsigned SR2SubIFDOffset = 0;
              unsigned SR2SubIFDLength = 0;
              unsigned SR2SubIFDKey = 0;
              int base = curr_pos + 6 - pos_in_original_raw;
              entries = get2();
              while (entries--) {
                tiff_get(base, &tag, &type, &len, &save);

                if (tag == 0x7200) {
                  SR2SubIFDOffset = get4();
                } else if (tag == 0x7201) {
                  SR2SubIFDLength = get4();
                } else if (tag == 0x7221) {
                  SR2SubIFDKey = get4();
                }
                fseek(ifp, save, SEEK_SET);
              }

              if (SR2SubIFDLength && (SR2SubIFDLength < 10240000) && (buf_SR2 = (unsigned *)malloc(SR2SubIFDLength+1024))) { // 1024b for safety
                fseek(ifp, SR2SubIFDOffset + base, SEEK_SET);
                fread(buf_SR2, SR2SubIFDLength, 1, ifp);
                sony_decrypt(buf_SR2, SR2SubIFDLength / 4, 1, SR2SubIFDKey);
                parseSonySR2 ((uchar *)buf_SR2, SR2SubIFDOffset, SR2SubIFDLength, AdobeDNG);

                free(buf_SR2);
              }

            } /* SR2 processed */
            break;
          }
        }

      } else {
        fread(mbuf + 6, 1, 2, ifp);
        if (!strcmp(mbuf, "RICOH") &&
            ((sget2((uchar *)mbuf+6) == 0x4949) ||
             (sget2((uchar *)mbuf+6) == 0x4d4d))) {
          is_PentaxRicohMakernotes = 1;
        }
        if (!strcmp(mbuf, "PENTAX ") ||
            !strcmp(mbuf, "SAMSUNG") ||
            is_PentaxRicohMakernotes) {
          makernote_found = 1;
          fseek(ifp, start_pos, SEEK_SET);
          parse_makernote_0xc634(base, 0, CameraDNG);
        }
      }
      fseek(ifp, start_pos, SEEK_SET);
      order = m_sorder;
    }
// IB end
#endif
      if (dng_version) {
        break;
      }
      parse_minolta(j = get4() + base);
      fseek(ifp, j, SEEK_SET);
      parse_tiff_ifd(base);
      break;
    case 50752:
      read_shorts(cr2_slice, 3);
      break;
    case 50829: /* ActiveArea */
      top_margin = getint(type);
      left_margin = getint(type);
      height = getint(type) - top_margin;
      width = getint(type) - left_margin;
      break;
    case 50830: /* MaskedAreas */
      for (i = 0; i < len && i < 32; i++)
        ((int *)mask)[i] = getint(type);
      black = 0;
      break;
#ifdef LIBRAW_LIBRARY_BUILD
    case 50970: /* PreviewColorSpace */
      tiff_ifd[ifd].dng_levels.parsedfields |= LIBRAW_DNGFM_PREVIEWCS;
      tiff_ifd[ifd].dng_levels.preview_colorspace = getint(type);
      break;
#endif
    case 51009: /* OpcodeList2 */
#ifdef LIBRAW_LIBRARY_BUILD
      tiff_ifd[ifd].dng_levels.parsedfields |= LIBRAW_DNGFM_OPCODE2;
      tiff_ifd[ifd].opcode2_offset =
#endif
          meta_offset = ftell(ifp);
      break;
    case 64772: /* Kodak P-series */
      if (len < 13)
        break;
      fseek(ifp, 16, SEEK_CUR);
      data_offset = get4();
      fseek(ifp, 28, SEEK_CUR);
      data_offset += get4();
      load_raw = &CLASS packed_load_raw;
      break;
    case 65026:
      if (type == 2)
        fgets(model2, 64, ifp);
    }
    fseek(ifp, save, SEEK_SET);
  }
  if (sony_length && sony_length < 10240000 && (buf = (unsigned *)malloc(sony_length)))
  {
    fseek(ifp, sony_offset, SEEK_SET);
    fread(buf, sony_length, 1, ifp);
    sony_decrypt(buf, sony_length / 4, 1, sony_key);
#ifndef LIBRAW_LIBRARY_BUILD
    sfp = ifp;
    if ((ifp = tmpfile()))
    {
      fwrite(buf, sony_length, 1, ifp);
      fseek(ifp, 0, SEEK_SET);
      parse_tiff_ifd(-sony_offset);
      fclose(ifp);
    }
    ifp = sfp;
#else
    parseSonySR2 ((uchar *)buf, sony_offset, sony_length, nonDNG);
#endif
    free(buf);
  }
  for (i = 0; i < colors; i++)
    FORCC cc[i][c] *= ab[i];
  if (use_cm)
  {
    FORCC for (i = 0; i < 3; i++) for (cam_xyz[c][i] = j = 0; j < colors; j++) cam_xyz[c][i] +=
        cc[c][j] * cm[j][i] * xyz[i];
    cam_xyz_coeff(cmatrix, cam_xyz);
  }
  if (asn[0])
  {
    cam_mul[3] = 0;
    FORCC
     if(fabs(asn[c])>0.0001)
     	cam_mul[c] = 1 / asn[c];
  }
  if (!use_cm)
    FORCC if(fabs(cc[c][c])>0.0001) pre_mul[c] /= cc[c][c];
  return 0;
}

int CLASS parse_tiff(int base)
{
  int doff;
  fseek(ifp, base, SEEK_SET);
  order = get2();
  if (order != 0x4949 && order != 0x4d4d)
    return 0;
  get2();
  while ((doff = get4()))
  {
    fseek(ifp, doff + base, SEEK_SET);
    if (parse_tiff_ifd(base))
      break;
  }
  return 1;
}

void CLASS apply_tiff()
{
  int max_samp = 0, ties = 0, raw = -1, thm = -1, i;
  unsigned long long ns, os;
  struct jhead jh;

  thumb_misc = 16;
  if (thumb_offset)
  {
    fseek(ifp, thumb_offset, SEEK_SET);
    if (ljpeg_start(&jh, 1))
    {
      if ((unsigned)jh.bits < 17 && (unsigned)jh.wide < 0x10000 && (unsigned)jh.high < 0x10000)
      {
        thumb_misc = jh.bits;
        thumb_width = jh.wide;
        thumb_height = jh.high;
      }
    }
  }
  for (i = tiff_nifds; i--;)
  {
    if (tiff_ifd[i].t_shutter)
      shutter = tiff_ifd[i].t_shutter;
    tiff_ifd[i].t_shutter = shutter;
  }
  for (i = 0; i < tiff_nifds; i++)
  {
    if( tiff_ifd[i].t_width < 1 ||  tiff_ifd[i].t_width > 65535
       || tiff_ifd[i].t_height < 1 || tiff_ifd[i].t_height > 65535)
          continue; /* wrong image dimensions */
    if (max_samp < tiff_ifd[i].samples)
      max_samp = tiff_ifd[i].samples;
    if (max_samp > 3)
      max_samp = 3;
    os = raw_width * raw_height;
    ns = tiff_ifd[i].t_width * tiff_ifd[i].t_height;
    if (tiff_bps)
    {
      os *= tiff_bps;
      ns *= tiff_ifd[i].bps;
    }
    if ((tiff_ifd[i].comp != 6 || tiff_ifd[i].samples != 3) &&
        unsigned(tiff_ifd[i].t_width | tiff_ifd[i].t_height) < 0x10000 && (unsigned)tiff_ifd[i].bps < 33 &&
        (unsigned)tiff_ifd[i].samples < 13 && ns && ((ns > os && (ties = 1)) || (ns == os && shot_select == ties++)))
    {
      raw_width = tiff_ifd[i].t_width;
      raw_height = tiff_ifd[i].t_height;
      tiff_bps = tiff_ifd[i].bps;
      tiff_compress = tiff_ifd[i].comp;
      tiff_sampleformat = tiff_ifd[i].sample_format;
      data_offset = tiff_ifd[i].offset;
#ifdef LIBRAW_LIBRARY_BUILD
      data_size = tiff_ifd[i].bytes;
#endif
      tiff_flip = tiff_ifd[i].t_flip;
      tiff_samples = tiff_ifd[i].samples;
      tile_width = tiff_ifd[i].t_tile_width;
      tile_length = tiff_ifd[i].t_tile_length;
      shutter = tiff_ifd[i].t_shutter;
      raw = i;
    }
  }

  if (is_NikonTransfer) {
    if (tiff_ifd[raw].bps == 16) {
      if (tiff_compress == 1) {
        if ((raw_width * raw_height * 3) == (tiff_ifd[raw].bytes << 1)) {
          tiff_bps = tiff_ifd[raw].bps = 12;
        } else {
          tiff_bps = tiff_ifd[raw].bps = 14;
        }
      }
    } else if (tiff_ifd[raw].bps == 8) {
      if (tiff_compress == 1) {
        is_NikonTransfer = 2; // 8-bit debayered TIFF, like CoolScan NEFs
        imgdata.params.coolscan_nef_gamma = 2.2f;
      }
    }
  }

  if (is_raw == 1 && ties)
    is_raw = ties;
  if (!tile_width)
    tile_width = INT_MAX;
  if (!tile_length)
    tile_length = INT_MAX;
  for (i = tiff_nifds; i--;)
    if (tiff_ifd[i].t_flip)
      tiff_flip = tiff_ifd[i].t_flip;
  if (raw >= 0 && !load_raw)
    switch (tiff_compress)
    {
    case 32767:
#ifdef LIBRAW_LIBRARY_BUILD
      if (!dng_version && INT64(tiff_ifd[raw].bytes) == INT64(raw_width) * INT64(raw_height))
#else
      if (tiff_ifd[raw].bytes == raw_width * raw_height)
#endif
      {
        tiff_bps = 14;
        load_raw = &CLASS sony_arw2_load_raw;
        break;
      }
#ifdef LIBRAW_LIBRARY_BUILD
      if (!dng_version && !strncasecmp(make, "Sony", 4) && INT64(tiff_ifd[raw].bytes) == INT64(raw_width) * INT64(raw_height) * 2ULL)
#else
      if (!strncasecmp(make, "Sony", 4) && tiff_ifd[raw].bytes == raw_width * raw_height * 2)
#endif
      {
        tiff_bps = 14;
        load_raw = &CLASS unpacked_load_raw;
        break;
      }
#ifdef LIBRAW_LIBRARY_BUILD
      if (INT64(tiff_ifd[raw].bytes) * 8ULL != INT64(raw_width) * INT64(raw_height) * INT64(tiff_bps))
#else
      if (tiff_ifd[raw].bytes * 8 != raw_width * raw_height * tiff_bps)
#endif
      {
        raw_height += 8;
        load_raw = &CLASS sony_arw_load_raw;
        break;
      }
      load_flags = 79;
    case 32769:
      load_flags++;
    case 32770:
    case 32773:
      goto slr;
    case 0:
    case 1:
#ifdef LIBRAW_LIBRARY_BUILD
#ifdef USE_DNGSDK
    if (dng_version && tiff_sampleformat == 3
        && (tiff_bps > 8 && (tiff_bps % 8 == 0))) // 16,24,32,48...
    {
       load_raw = &CLASS float_dng_load_raw_placeholder;
       break;
    }
#endif
      // Sony 14-bit uncompressed
      if (!dng_version && !strncasecmp(make, "Sony", 4) && INT64(tiff_ifd[raw].bytes) == INT64(raw_width) * INT64(raw_height) * 2ULL)
      {
        tiff_bps = 14;
        load_raw = &CLASS unpacked_load_raw;
        break;
      }
      if (!dng_version && !strncasecmp(make, "Sony", 4) && tiff_ifd[raw].samples == 4 &&
          INT64(tiff_ifd[raw].bytes) == INT64(raw_width) * INT64(raw_height) * 8ULL) // Sony ARQ
      {
        tiff_bps = 14;
        tiff_samples = 4;
        load_raw = &CLASS sony_arq_load_raw;
        filters = 0;
        strcpy(cdesc, "RGBG");
        break;
      }
      if (!strncasecmp(make, "Nikon", 5) &&
          (!strncmp(software, "Nikon Scan", 10) ||
          is_NikonTransfer == 2))
      {
        load_raw = &CLASS nikon_coolscan_load_raw;
        raw_color = 1;
        filters = 0;
        break;
      }
      if ((!strncmp(make, "OLYMPUS", 7) ||
          (!strncasecmp(make, "CLAUSS", 6) && !strncasecmp(model, "piX 5oo", 7))) &&
          (INT64(tiff_ifd[raw].bytes) * 2ULL == INT64(raw_width) * INT64(raw_height) * 3ULL))
#else
      if ((!strncmp(make, "OLYMPUS", 7) ||
          (!strncasecmp(make, "CLAUSS", 6) && !strncasecmp(model, "piX 5oo", 7))) &&
          (tiff_ifd[raw].bytes * 2 == raw_width * raw_height * 3))
#endif
        load_flags = 24;
#ifdef LIBRAW_LIBRARY_BUILD
      if (!dng_version && INT64(tiff_ifd[raw].bytes) * 5ULL == INT64(raw_width) * INT64(raw_height) * 8ULL)
#else
      if (tiff_ifd[raw].bytes * 5 == raw_width * raw_height * 8)
#endif
      {
        load_flags = 81;
        tiff_bps = 12;
      }
    slr:
      switch (tiff_bps)
      {
      case 8:
        load_raw = &CLASS eight_bit_load_raw;
        break;
      case 12:
        if (tiff_ifd[raw].phint == 2)
          load_flags = 6;
        if(!strncasecmp(make,"NIKON",5)
	    && !strncasecmp(model,"COOLPIX A1000",13)
	    && data_size == raw_width * raw_height * 2 )
	  load_raw = &CLASS unpacked_load_raw;
	else
          load_raw = &CLASS packed_load_raw;
        break;
      case 14:
        load_flags = 0;
      case 16:
        load_raw = &CLASS unpacked_load_raw;
#ifdef LIBRAW_LIBRARY_BUILD
        if ((!strncmp(make, "OLYMPUS", 7) ||
            (!strncasecmp(make, "CLAUSS", 6) && !strncasecmp(model, "piX 5oo", 7))) &&
            (INT64(tiff_ifd[raw].bytes) * 7ULL > INT64(raw_width) * INT64(raw_height)))
#else
        if ((!strncmp(make, "OLYMPUS", 7) ||
            (!strncasecmp(make, "CLAUSS", 6) && !strncasecmp(model, "piX 5oo", 7))) &&
            (tiff_ifd[raw].bytes * 7 > raw_width * raw_height))
#endif
          load_raw = &CLASS olympus_load_raw;
      }
      break;
    case 6:
    case 7:
    case 99:
      load_raw = &CLASS lossless_jpeg_load_raw;
      break;
    case 262:
      load_raw = &CLASS kodak_262_load_raw;
      break;
    case 34713:
#ifdef LIBRAW_LIBRARY_BUILD
      if ((INT64(raw_width) + 9ULL) / 10ULL * 16ULL * INT64(raw_height) == INT64(tiff_ifd[raw].bytes))
#else
      if ((raw_width + 9) / 10 * 16 * raw_height == tiff_ifd[raw].bytes)
#endif
      {
        load_raw = &CLASS packed_load_raw;
        load_flags = 1;
      }
#ifdef LIBRAW_LIBRARY_BUILD
      else if (INT64(raw_width) * INT64(raw_height) * 3ULL == INT64(tiff_ifd[raw].bytes) * 2ULL)
#else
      else if (raw_width * raw_height * 3 == tiff_ifd[raw].bytes * 2)
#endif
      {
        load_raw = &CLASS packed_load_raw;
        if (model[0] == 'N')
          load_flags = 80;
      }
#ifdef LIBRAW_LIBRARY_BUILD
      else if (INT64(raw_width) * INT64(raw_height) * 3ULL == INT64(tiff_ifd[raw].bytes))
#else
      else if (raw_width * raw_height * 3 == tiff_ifd[raw].bytes)
#endif
      {
        load_raw = &CLASS nikon_yuv_load_raw;
        gamma_curve(1 / 2.4, 12.92, 1, 4095);
        memset(cblack, 0, sizeof cblack);
        filters = 0;
      }
#ifdef LIBRAW_LIBRARY_BUILD
      else if (INT64(raw_width) * INT64(raw_height) * 2ULL == INT64(tiff_ifd[raw].bytes))
#else
      else if (raw_width * raw_height * 2 == tiff_ifd[raw].bytes)
#endif
      {
        load_raw = &CLASS unpacked_load_raw;
        load_flags = 4;
        order = 0x4d4d;
      }
      else
#ifdef LIBRAW_LIBRARY_BUILD
          if (INT64(raw_width) * INT64(raw_height) * 3ULL == INT64(tiff_ifd[raw].bytes) * 2ULL)
      {
        load_raw = &CLASS packed_load_raw;
        load_flags = 80;
      }
      else if (tiff_ifd[raw].rows_per_strip && tiff_ifd[raw].strip_offsets_count &&
               tiff_ifd[raw].strip_offsets_count == tiff_ifd[raw].strip_byte_counts_count)
      {
        int fit = 1;
        for (int i = 0; i < tiff_ifd[raw].strip_byte_counts_count - 1; i++) // all but last
          if (INT64(tiff_ifd[raw].strip_byte_counts[i]) * 2ULL != INT64(tiff_ifd[raw].rows_per_strip) * INT64(raw_width) * 3ULL)
          {
            fit = 0;
            break;
          }
        if (fit)
          load_raw = &CLASS nikon_load_striped_packed_raw;
        else
          load_raw = &CLASS nikon_load_raw; // fallback
      }
      else if((((INT64(raw_width) * 3ULL /2ULL) + 15ULL)/16ULL)*16ULL * INT64(raw_height) == INT64(tiff_ifd[raw].bytes))
      {
        load_raw = &CLASS nikon_load_padded_packed_raw;
        load_flags = (((INT64(raw_width) * 3ULL /2ULL) + 15ULL)/16ULL)*16ULL; // bytes per row
      }
      else
#endif
        load_raw = &CLASS nikon_load_raw;
      break;
    case 65535:
      load_raw = &CLASS pentax_load_raw;
      break;
    case 65000:
      switch (tiff_ifd[raw].phint)
      {
      case 2:
        load_raw = &CLASS kodak_rgb_load_raw;
        filters = 0;
        break;
      case 6:
        load_raw = &CLASS kodak_ycbcr_load_raw;
        filters = 0;
        break;
      case 32803:
        load_raw = &CLASS kodak_65000_load_raw;
      }
    case 32867:
    case 34892:
      break;
#ifdef LIBRAW_LIBRARY_BUILD
    case 8:
      break;
#endif
    default:
      is_raw = 0;
    }
  if (!dng_version)
    if (((tiff_samples == 3   &&
          tiff_ifd[raw].bytes &&
	  !(tiff_bps == 16 && !strncmp(make, "Leaf", 4)) && // Allow Leaf/16bit/3color files
          tiff_bps != 14      &&
          (tiff_compress & -16) != 32768) ||
         (tiff_bps == 8              &&
          strncmp(make, "Phase", 5)  &&
          strncmp(make, "Leaf", 4)   &&
          !strcasestr(make, "Kodak") &&
          !strstr(model2, "DEBUG RAW")))    &&
        strncmp(software, "Nikon Scan", 10) &&
        is_NikonTransfer != 2)
      is_raw = 0;

  for (i = 0; i < tiff_nifds; i++)
    if (i != raw &&
        (tiff_ifd[i].samples == max_samp || (tiff_ifd[i].comp == 7 && tiff_ifd[i].samples == 1)) /* Allow 1-bps JPEGs */
        && tiff_ifd[i].bps > 0 && tiff_ifd[i].bps < 33 && tiff_ifd[i].phint != 32803 && tiff_ifd[i].phint != 34892 &&
        unsigned(tiff_ifd[i].t_width | tiff_ifd[i].t_height) < 0x10000 &&
        tiff_ifd[i].t_width * tiff_ifd[i].t_height / (SQR(tiff_ifd[i].bps) + 1) >
            thumb_width * thumb_height / (SQR(thumb_misc) + 1) &&
        tiff_ifd[i].comp != 34892)
    {
      thumb_width = tiff_ifd[i].t_width;
      thumb_height = tiff_ifd[i].t_height;
      thumb_offset = tiff_ifd[i].offset;
      thumb_length = tiff_ifd[i].bytes;
      thumb_misc = tiff_ifd[i].bps;
      thm = i;
    }
  if (thm >= 0)
  {
    thumb_misc |= tiff_ifd[thm].samples << 5;
    switch (tiff_ifd[thm].comp)
    {
    case 0:
      write_thumb = &CLASS layer_thumb;
      break;
    case 1:
      if (tiff_ifd[thm].bps <= 8)
        write_thumb = &CLASS ppm_thumb;
      else if (!strncmp(make, "Imacon", 6))
        write_thumb = &CLASS ppm16_thumb;
      else
        thumb_load_raw = &CLASS kodak_thumb_load_raw;
      break;
    case 65000:
      thumb_load_raw = tiff_ifd[thm].phint == 6 ? &CLASS kodak_ycbcr_load_raw : &CLASS kodak_rgb_load_raw;
    }
  }
}

void CLASS parse_minolta(int base)
{
  int tag, len, offset, high = 0, wide = 0, i, c;
  short sorder = order;
#ifdef LIBRAW_LIBRARY_BUILD
  INT64 save;
#else
  int save;
#endif

  fseek(ifp, base, SEEK_SET);
  if (fgetc(ifp) || fgetc(ifp) - 'M' || fgetc(ifp) - 'R')
    return;
  order = fgetc(ifp) * 0x101;
  offset = base + get4() + 8;
#ifdef LIBRAW_LIBRARY_BUILD
  INT64 fsize = ifp->size();
  if(offset>fsize-8) // At least 8 bytes for tag/len
    offset = fsize-8;
#endif

  while ((save = ftell(ifp)) < offset)
  {
    for (tag = i = 0; i < 4; i++)
      tag = tag << 8 | fgetc(ifp);
    len = get4();
    if(len < 0)
      return; // just ignore wrong len?? or raise bad file exception?
#ifdef LIBRAW_LIBRARY_BUILD
    if((INT64)len + save + 8ULL > fsize)
      return; // just ignore out of file metadata, stop parse
#endif
    switch (tag)
    {
    case 0x505244: /* PRD */
      fseek(ifp, 8, SEEK_CUR);
      high = get2();
      wide = get2();
#ifdef LIBRAW_LIBRARY_BUILD
#define imSony imgdata.makernotes.sony
      imSony.prd_ImageHeight = get2();
      imSony.prd_ImageWidth = get2();
      fseek(ifp, 1L, SEEK_CUR);
      imSony.prd_RawBitDepth = (ushort)fgetc(ifp);
      imSony.prd_StorageMethod = (ushort)fgetc(ifp);
      fseek(ifp, 4L, SEEK_CUR);
      imSony.prd_BayerPattern = (ushort)fgetc(ifp);
#undef imSony
#endif
      break;
#ifdef LIBRAW_LIBRARY_BUILD
#define icWBC imgdata.color.WB_Coeffs
    case 0x524946: /* RIF */
      if (!strncasecmp(model, "DSLR-A100", 9))
      {
        fseek(ifp, 8, SEEK_CUR);
        icWBC[LIBRAW_WBI_Tungsten][0] = get2();
        icWBC[LIBRAW_WBI_Tungsten][2] = get2();
        icWBC[LIBRAW_WBI_Daylight][0] = get2();
        icWBC[LIBRAW_WBI_Daylight][2] = get2();
        icWBC[LIBRAW_WBI_Cloudy][0] = get2();
        icWBC[LIBRAW_WBI_Cloudy][2] = get2();
        icWBC[LIBRAW_WBI_FL_W][0] = get2();
        icWBC[LIBRAW_WBI_FL_W][2] = get2();
        icWBC[LIBRAW_WBI_Flash][0] = get2();
        icWBC[LIBRAW_WBI_Flash][2] = get2();
        get4();
        icWBC[LIBRAW_WBI_Shade][0] = get2();
        icWBC[LIBRAW_WBI_Shade][2] = get2();
        icWBC[LIBRAW_WBI_FL_D][0] = get2();
        icWBC[LIBRAW_WBI_FL_D][2] = get2();
        icWBC[LIBRAW_WBI_FL_N][0] = get2();
        icWBC[LIBRAW_WBI_FL_N][2] = get2();
        icWBC[LIBRAW_WBI_FL_WW][0] = get2();
        icWBC[LIBRAW_WBI_FL_WW][2] = get2();
        icWBC[LIBRAW_WBI_Daylight][1]   = icWBC[LIBRAW_WBI_Daylight][3] =
          icWBC[LIBRAW_WBI_Tungsten][1] = icWBC[LIBRAW_WBI_Tungsten][3] =
          icWBC[LIBRAW_WBI_Flash][1]    = icWBC[LIBRAW_WBI_Flash][3] =
          icWBC[LIBRAW_WBI_Cloudy][1]   = icWBC[LIBRAW_WBI_Cloudy][3] =
          icWBC[LIBRAW_WBI_Shade][1]    = icWBC[LIBRAW_WBI_Shade][3] =
          icWBC[LIBRAW_WBI_FL_D][1]     = icWBC[LIBRAW_WBI_FL_D][3] =
          icWBC[LIBRAW_WBI_FL_N][1]     = icWBC[LIBRAW_WBI_FL_N][3] =
          icWBC[LIBRAW_WBI_FL_W][1]     = icWBC[LIBRAW_WBI_FL_W][3] =
          icWBC[LIBRAW_WBI_FL_WW][1]    = icWBC[LIBRAW_WBI_FL_WW][3] = 0x100;
      }
      break;
#undef icWBC
#endif
    case 0x574247: /* WBG */
      get4();
      i = strcmp(model, "DiMAGE A200") ? 0 : 3;
      FORC4 cam_mul[c ^ (c >> 1) ^ i] = get2();
      break;
    case 0x545457: /* TTW */
      parse_tiff(ftell(ifp));
      data_offset = offset;
    }
    fseek(ifp, save + len + 8, SEEK_SET);
  }
  raw_height = high;
  raw_width = wide;
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
  FILE *save = ifp;
#else
#if defined(_WIN32) && !defined(__MINGW32__) && defined(_MSC_VER) && (_MSC_VER > 1310)
  if (ifp->wfname())
  {
    std::wstring rawfile(ifp->wfname());
    rawfile.replace(rawfile.length() - 3, 3, L"JPG");
    if (!ifp->subfile_open(rawfile.c_str()))
    {
      parse_tiff(12);
      thumb_offset = 0;
      is_raw = 1;
      ifp->subfile_close();
    }
    else
      imgdata.process_warnings |= LIBRAW_WARN_NO_METADATA;
    return;
  }
#endif
  if (!ifp->fname())
  {
    imgdata.process_warnings |= LIBRAW_WARN_NO_METADATA;
    return;
  }
#endif

  ext = strrchr(ifname, '.');
  file = strrchr(ifname, '/');
  if (!file)
    file = strrchr(ifname, '\\');
#ifndef LIBRAW_LIBRARY_BUILD
  if (!file)
    file = ifname - 1;
#else
  if (!file)
    file = (char *)ifname - 1;
#endif
  file++;
  if (!ext || strlen(ext) != 4 || ext - file != 8)
    return;
  jname = (char *)malloc(strlen(ifname) + 1);
  merror(jname, "parse_external_jpeg()");
  strcpy(jname, ifname);
  jfile = file - ifname + jname;
  jext = ext - ifname + jname;
  if (strcasecmp(ext, ".jpg"))
  {
    strcpy(jext, isupper(ext[1]) ? ".JPG" : ".jpg");
    if (isdigit(*file))
    {
      memcpy(jfile, file + 4, 4);
      memcpy(jfile + 4, file, 4);
    }
  }
  else
    while (isdigit(*--jext))
    {
      if (*jext != '9')
      {
        (*jext)++;
        break;
      }
      *jext = '0';
    }
#ifndef LIBRAW_LIBRARY_BUILD
  if (strcmp(jname, ifname))
  {
    if ((ifp = fopen(jname, "rb")))
    {
#ifdef DCRAW_VERBOSE
      if (verbose)
        fprintf(stderr, _("Reading metadata from %s ...\n"), jname);
#endif
      parse_tiff(12);
      thumb_offset = 0;
      is_raw = 1;
      fclose(ifp);
    }
  }
#else
  if (strcmp(jname, ifname))
  {
    if (!ifp->subfile_open(jname))
    {
      parse_tiff(12);
      thumb_offset = 0;
      is_raw = 1;
      ifp->subfile_close();
    }
    else
      imgdata.process_warnings |= LIBRAW_WARN_NO_METADATA;
  }
#endif
  if (!timestamp)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    imgdata.process_warnings |= LIBRAW_WARN_NO_METADATA;
#endif
#ifdef DCRAW_VERBOSE
    fprintf(stderr, _("Failed to read metadata from %s\n"), jname);
#endif
  }
  free(jname);
#ifndef LIBRAW_LIBRARY_BUILD
  ifp = save;
#endif
}

/*
   CIFF block 0x1030 contains an 8x8 white sample.
   Load this into white[][] for use in scale_colors().
 */
void CLASS ciff_block_1030()
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

void CLASS parse_ciff(int offset, int length, int depth)
{
  int tboff, nrecs, c, type, len, save, wbi = -1;
  ushort key[] = {0x410, 0x45f3};
#ifdef LIBRAW_LIBRARY_BUILD
  INT64 fsize = ifp->size();
#endif

  fseek(ifp, offset + length - 4, SEEK_SET);
  tboff = get4() + offset;
  fseek(ifp, tboff, SEEK_SET);
  nrecs = get2();
  if (nrecs<1) return;
  if ((nrecs | depth) > 127)
    return;
#ifdef LIBRAW_LIBRARY_BUILD
  if(nrecs*10 + offset > fsize) return;
#endif

  while (nrecs--)
  {
    type = get2();
    len = get4();
    save = ftell(ifp) + 4;
    INT64 see = offset + get4();
#ifdef LIBRAW_LIBRARY_BUILD
	if(see >= fsize  ) // At least one byte
	{
		fseek(ifp, save, SEEK_SET);
		continue;
	}
#endif
    fseek(ifp, see, SEEK_SET);
    if ((((type >> 8) + 8) | 8) == 0x38)
    {
      parse_ciff(ftell(ifp), len, depth + 1); /* Parse a sub-table */
    }
#ifdef LIBRAW_LIBRARY_BUILD
    if (type == 0x3004)
      parse_ciff(ftell(ifp), len, depth + 1);
#endif
    if (type == 0x0810)
      fread(artist, 64, 1, ifp);
    if (type == 0x080a)
    {
      fread(make, 64, 1, ifp);
      fseek(ifp, strbuflen(make) - 63, SEEK_CUR);
      fread(model, 64, 1, ifp);
    }
    if (type == 0x1810)
    {
      width = get4();
      height = get4();
      pixel_aspect = int_to_float(get4());
      flip = get4();
    }
    if (type == 0x1835) /* Get the decoder table */
      tiff_compress = get4();
    if (type == 0x2007)
    {
      thumb_offset = ftell(ifp);
      thumb_length = len;
    }
    if (type == 0x1818)
    {
      shutter = libraw_powf64l(2.0f, -int_to_float((get4(), get4())));
      aperture = libraw_powf64l(2.0f, int_to_float(get4()) / 2);
#ifdef LIBRAW_LIBRARY_BUILD
      imgdata.lens.makernotes.CurAp = aperture;
#endif
    }
    if (type == 0x102a)
    {
      //      iso_speed = pow (2.0, (get4(),get2())/32.0 - 4) * 50;
      get2(); // skip one
      iso_speed = libraw_powf64l(2.0f, (get2() + get2()) / 32.0f - 5.0f) * 100.0f;
#ifdef LIBRAW_LIBRARY_BUILD
      aperture = _CanonConvertAperture((get2(), get2()));
      imgdata.lens.makernotes.CurAp = aperture;
#else
      aperture = libraw_powf64l(2.0, (get2(), (short)get2()) / 64.0);
#endif
      shutter = libraw_powf64l(2.0, -((short)get2()) / 32.0);
      wbi = (get2(), get2());
      if (wbi > 17)
        wbi = 0;
      fseek(ifp, 32, SEEK_CUR);
      if (shutter > 1e6)
        shutter = get2() / 10.0;
    }
    if (type == 0x102c)
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
#ifdef LIBRAW_LIBRARY_BUILD
    if (type == 0x10a9)
    {
      int bls = 0;
      INT64 o = ftell(ifp);
      fseek(ifp, (0x1 << 1), SEEK_CUR);
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][c ^ (c >> 1)] = get2();
      Canon_WBpresets(0, 0);
      if (unique_id != 0x1668000)  // Canon EOS D60
      {
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Custom][c ^ (c >> 1)] = get2();
        fseek (ifp, 8L, SEEK_CUR);
      }
      FORC4
        bls += (imgdata.makernotes.canon.ChannelBlackLevel[c ^ (c >> 1)] = get2());
      imgdata.makernotes.canon.AverageBlackLevel = bls / 4;
      fseek(ifp, o, SEEK_SET);
    }
    if (type == 0x102d)
    {
      INT64 o = ftell(ifp);
      Canon_CameraSettings(len>>1);
      fseek(ifp, o, SEEK_SET);
    }
    if (type == 0x580b)
    {
      if (strcmp(model, "Canon EOS D30"))
        sprintf(imgdata.shootinginfo.BodySerial, "%d", len);
      else
        sprintf(imgdata.shootinginfo.BodySerial, "%0x-%05d", len >> 16, len & 0xffff);
    }
#endif
    if (type == 0x0032)
    {
      if (len == 768)
      { /* EOS D30 */
        fseek(ifp, 72, SEEK_CUR);
        FORC4
        {
          ushort q = get2();
          cam_mul[c ^ (c >> 1)] = 1024.0/ MAX(1,q);
        }
        if (!wbi)
          cam_mul[0] = -1; /* use my auto white balance */
      }
      else if (cam_mul[0] <= 0.001f)
      {
        if (get2() == key[0]) /* Pro1, G6, S60, S70 */
          c = (strstr(model, "Pro1") ? "012346000000000000" : "01345:000000006008")[LIM(0, wbi, 17)] - '0' + 2;
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
    if (type == 0x10a9)
    { /* D60, 10D, 300D, and clones */
      if (len > 66)
        wbi = "0134567028"[LIM(0, wbi, 9)] - '0';
      fseek(ifp, 2 + wbi * 8, SEEK_CUR);
      FORC4 cam_mul[c ^ (c >> 1)] = get2();
    }
    if (type == 0x1030 && wbi >= 0 && (0x18040 >> wbi & 1))
      ciff_block_1030(); /* all that don't have 0x10a9 */
    if (type == 0x1031)
    {
      raw_width = (get2(), get2());
      raw_height = get2();
    }
    if (type == 0x501c)
    {
      iso_speed = len & 0xffff;
    }
    if (type == 0x5029)
    {
#ifdef LIBRAW_LIBRARY_BUILD
      imgdata.lens.makernotes.CurFocal = len >> 16;
      imgdata.lens.makernotes.FocalType = len & 0xffff;
      if (imgdata.lens.makernotes.FocalType == 2)
      {
        imgdata.lens.makernotes.CanonFocalUnits = 32;
        if (imgdata.lens.makernotes.CanonFocalUnits > 1)
          imgdata.lens.makernotes.CurFocal /= (float)imgdata.lens.makernotes.CanonFocalUnits;
      }
      focal_len = imgdata.lens.makernotes.CurFocal;
#else
      focal_len = len >> 16;
      if ((len & 0xffff) == 2)
        focal_len /= 32;
#endif
    }
    if (type == 0x5813)
      flash_used = int_to_float(len);
    if (type == 0x5814)
      canon_ev = int_to_float(len);
    if (type == 0x5817)
      shot_order = len;
    if (type == 0x5834)
    {
      unique_id = len;
#ifdef LIBRAW_LIBRARY_BUILD
      unique_id = setCanonBodyFeatures(unique_id);
#endif
    }
    if (type == 0x580e)
      timestamp = len;
    if (type == 0x180e)
      timestamp = get4();
#ifdef LOCALTIME
    if ((type | 0x4000) == 0x580e)
      timestamp = mktime(gmtime(&timestamp));
#endif
    fseek(ifp, save, SEEK_SET);
  }
}

void CLASS parse_rollei()
{
  char line[128], *val;
  struct tm t;

  fseek(ifp, 0, SEEK_SET);
  memset(&t, 0, sizeof t);
  do
  {
    if(!fgets(line, 128, ifp)) break;
    if ((val = strchr(line, '=')))
      *val++ = 0;
    else
      val = line + strbuflen(line);
    if (!strcmp(line, "DAT"))
      sscanf(val, "%d.%d.%d", &t.tm_mday, &t.tm_mon, &t.tm_year);
    if (!strcmp(line, "TIM"))
      sscanf(val, "%d:%d:%d", &t.tm_hour, &t.tm_min, &t.tm_sec);
    if (!strcmp(line, "HDR"))
      thumb_offset = atoi(val);
    if (!strcmp(line, "X  "))
      raw_width = atoi(val);
    if (!strcmp(line, "Y  "))
      raw_height = atoi(val);
    if (!strcmp(line, "TX "))
      thumb_width = atoi(val);
    if (!strcmp(line, "TY "))
      thumb_height = atoi(val);
  } while (strncmp(line, "EOHD", 4));
  data_offset = thumb_offset + thumb_width * thumb_height * 2;
  t.tm_year -= 1900;
  t.tm_mon -= 1;
  if (mktime(&t) > 0)
    timestamp = mktime(&t);
  strcpy(make, "Rollei");
  strcpy(model, "d530flex");
  write_thumb = &CLASS rollei_thumb;
}

void CLASS parse_sinar_ia()
{
  int entries, off;
  char str[8], *cp;

  order = 0x4949;
  fseek(ifp, 4, SEEK_SET);
  entries = get4();
  if(entries < 1 || entries > 8192) return;
  fseek(ifp, get4(), SEEK_SET);
  while (entries--)
  {
    off = get4();
    get4();
    fread(str, 8, 1, ifp);
    str[7] = 0; // Ensure end of string
    if (!strcmp(str, "META"))
      meta_offset = off;
    if (!strcmp(str, "THUMB"))
      thumb_offset = off;
    if (!strcmp(str, "RAW0"))
      data_offset = off;
  }
  fseek(ifp, meta_offset + 20, SEEK_SET);
  fread(make, 64, 1, ifp);
  make[63] = 0;
  if ((cp = strchr(make, ' ')))
  {
    strcpy(model, cp + 1);
    *cp = 0;
  }
  raw_width = get2();
  raw_height = get2();
  load_raw = &CLASS unpacked_load_raw;
  thumb_width = (get4(), get2());
  thumb_height = get2();
  write_thumb = &CLASS ppm_thumb;
  maximum = 0x3fff;
}

void CLASS parse_phase_one(int base)
{
  unsigned entries, tag, type, len, data, save, i, c;
  float romm_cam[3][3];
  char *cp;

  memset(&ph1, 0, sizeof ph1);
  fseek(ifp, base, SEEK_SET);
  order = get4() & 0xffff;
  if (get4() >> 8 != 0x526177)
    return; /* "Raw" */
  unsigned offset = get4();
  if(offset == 0xbad0bad) return;
  fseek(ifp, offset + base, SEEK_SET);
  entries = get4();
  if(entries > 8192) return; // too much??
  get4();
  while (entries--)
  {
    tag = get4();
    type = get4();
    len = get4();
    data = get4();
    save = ftell(ifp);
    fseek(ifp, base + data, SEEK_SET);
    switch (tag)
    {

#ifdef LIBRAW_LIBRARY_BUILD
    case 0x0102:
      stmread(imgdata.shootinginfo.BodySerial, len, ifp);
      if ((imgdata.shootinginfo.BodySerial[0] == 0x4c) && (imgdata.shootinginfo.BodySerial[1] == 0x49))
      {
        unique_id =
            (((imgdata.shootinginfo.BodySerial[0] & 0x3f) << 5) | (imgdata.shootinginfo.BodySerial[2] & 0x3f)) - 0x41;
      }
      else
      {
        unique_id =
            (((imgdata.shootinginfo.BodySerial[0] & 0x3f) << 5) | (imgdata.shootinginfo.BodySerial[1] & 0x3f)) - 0x41;
      }
      setPhaseOneFeatures(unique_id);
      break;
    case 0x0211:
      imgdata.other.SensorTemperature2 = int_to_float(data);
      break;
    case 0x0401:
      if (type == 4)
        imgdata.lens.makernotes.CurAp = libraw_powf64l(2.0f, (int_to_float(data) / 2.0f));
      else
        imgdata.lens.makernotes.CurAp = libraw_powf64l(2.0f, (getreal(type) / 2.0f));
      break;
    case 0x0403:
      if (type == 4)
        imgdata.lens.makernotes.CurFocal = int_to_float(data);
      else
        imgdata.lens.makernotes.CurFocal = getreal(type);
      break;
    case 0x0410:
      stmread(imgdata.lens.makernotes.body, len, ifp);
      break;
    case 0x0412:
      stmread(imgdata.lens.makernotes.Lens, len, ifp);
      break;
    case 0x0414:
      if (type == 4)
      {
        imgdata.lens.makernotes.MaxAp4CurFocal = libraw_powf64l(2.0f, (int_to_float(data) / 2.0f));
      }
      else
      {
        imgdata.lens.makernotes.MaxAp4CurFocal = libraw_powf64l(2.0f, (getreal(type) / 2.0f));
      }
      break;
    case 0x0415:
      if (type == 4)
      {
        imgdata.lens.makernotes.MinAp4CurFocal = libraw_powf64l(2.0f, (int_to_float(data) / 2.0f));
      }
      else
      {
        imgdata.lens.makernotes.MinAp4CurFocal = libraw_powf64l(2.0f, (getreal(type) / 2.0f));
      }
      break;
    case 0x0416:
      if (type == 4)
      {
        imgdata.lens.makernotes.MinFocal = int_to_float(data);
      }
      else
      {
        imgdata.lens.makernotes.MinFocal = getreal(type);
      }
      if (imgdata.lens.makernotes.MinFocal > 1000.0f)
      {
        imgdata.lens.makernotes.MinFocal = 0.0f;
      }
      break;
    case 0x0417:
      if (type == 4)
      {
        imgdata.lens.makernotes.MaxFocal = int_to_float(data);
      }
      else
      {
        imgdata.lens.makernotes.MaxFocal = getreal(type);
      }
      break;
#endif

    case 0x100:
      flip = "0653"[data & 3] - '0';
      break;
    case 0x106:
      for (i = 0; i < 9; i++)
#ifdef LIBRAW_LIBRARY_BUILD
        imgdata.color.P1_color[0].romm_cam[i] =
#endif
            ((float *)romm_cam)[i] = getreal(11);
      romm_coeff(romm_cam);
      break;
    case 0x107:
      FORC3 cam_mul[c] = getreal(11);
      break;
    case 0x108:
      raw_width = data;
      break;
    case 0x109:
      raw_height = data;
      break;
    case 0x10a:
      left_margin = data;
      break;
    case 0x10b:
      top_margin = data;
      break;
    case 0x10c:
      width = data;
      break;
    case 0x10d:
      height = data;
      break;
    case 0x10e:
      ph1.format = data;
      break;
    case 0x10f:
      data_offset = data + base;
      break;
    case 0x110:
      meta_offset = data + base;
      meta_length = len;
      break;
    case 0x112:
      ph1.key_off = save - 4;
      break;
    case 0x210:
      ph1.tag_210 = int_to_float(data);
#ifdef LIBRAW_LIBRARY_BUILD
      imgdata.other.SensorTemperature = ph1.tag_210;
#endif
      break;
    case 0x21a:
      ph1.tag_21a = data;
      break;
    case 0x21c:
      strip_offset = data + base;
      break;
    case 0x21d:
      ph1.t_black = data;
      break;
    case 0x222:
      ph1.split_col = data;
      break;
    case 0x223:
      ph1.black_col = data + base;
      break;
    case 0x224:
      ph1.split_row = data;
      break;
    case 0x225:
      ph1.black_row = data + base;
      break;
#ifdef LIBRAW_LIBRARY_BUILD
    case 0x226:
      for (i = 0; i < 9; i++)
        imgdata.color.P1_color[1].romm_cam[i] = getreal(11);
      break;
#endif
    case 0x301:
      model[63] = 0;
      fread(model, 1, 63, ifp);
      if ((cp = strstr(model, " camera")))
        *cp = 0;
    }
    fseek(ifp, save, SEEK_SET);
  }

#ifdef LIBRAW_LIBRARY_BUILD
  if (!imgdata.lens.makernotes.body[0] && !imgdata.shootinginfo.BodySerial[0])
  {
    fseek(ifp, meta_offset, SEEK_SET);
    order = get2();
    fseek(ifp, 6, SEEK_CUR);
    fseek(ifp, meta_offset + get4(), SEEK_SET);
    entries = get4();
    get4();
    while (entries--)
    {
      tag = get4();
      len = get4();
      data = get4();
      save = ftell(ifp);
      fseek(ifp, meta_offset + data, SEEK_SET);
      if (tag == 0x0407)
      {
        stmread(imgdata.shootinginfo.BodySerial, len, ifp);
        if ((imgdata.shootinginfo.BodySerial[0] == 0x4c) && (imgdata.shootinginfo.BodySerial[1] == 0x49))
        {
          unique_id =
              (((imgdata.shootinginfo.BodySerial[0] & 0x3f) << 5) | (imgdata.shootinginfo.BodySerial[2] & 0x3f)) - 0x41;
        }
        else
        {
          unique_id =
              (((imgdata.shootinginfo.BodySerial[0] & 0x3f) << 5) | (imgdata.shootinginfo.BodySerial[1] & 0x3f)) - 0x41;
        }
        setPhaseOneFeatures(unique_id);
      }
      fseek(ifp, save, SEEK_SET);
    }
  }
#endif
  load_raw = ph1.format < 3 ? &CLASS phase_one_load_raw : &CLASS phase_one_load_raw_c;
  maximum = 0xffff;
  strcpy(make, "Phase One");
  if (model[0])
    return;
  switch (raw_height)
  {
  case 2060:
    strcpy(model, "LightPhase");
    break;
  case 2682:
    strcpy(model, "H 10");
    break;
  case 4128:
    strcpy(model, "H 20");
    break;
  case 5488:
    strcpy(model, "H 25");
    break;
  }
}

void CLASS parse_fuji(int offset)
{
  unsigned entries, tag, len, save, c;

  fseek(ifp, offset, SEEK_SET);
  entries = get4();
  if (entries > 255)
    return;
#ifdef LIBRAW_LIBRARY_BUILD
  imgdata.process_warnings |= LIBRAW_WARN_PARSEFUJI_PROCESSED;
#endif
  while (entries--)
  {
    tag = get2();
    len = get2();
    save = ftell(ifp);

    if (tag == 0x0100)
    {
      raw_height = get2();
      raw_width = get2();
    }
    else if (tag == 0x0121)
    {
      height = get2();
      if ((width = get2()) == 4284)
        width += 3;
    }
    else if (tag == 0x0130)
    {
      fuji_layout = fgetc(ifp) >> 7;
      fuji_width = !(fgetc(ifp) & 8);
    }
    else if (tag == 0x0131)
    {
      filters = 9;
      FORC(36)
      {
        int q = fgetc(ifp);
        xtrans_abs[0][35 - c] = MAX(0, MIN(q, 2)); /* & 3;*/
      }
    }
    else if (tag == 0x2ff0)
    {
      FORC4 cam_mul[c ^ 1] = get2();
    }

#ifdef LIBRAW_LIBRARY_BUILD
    else if (tag == 0x0110)
    {
      imgdata.sizes.raw_crop.ctop = get2();
      imgdata.sizes.raw_crop.cleft = get2();
    }

    else if (tag == 0x0111)
    {
      imgdata.sizes.raw_crop.cheight = get2();
      imgdata.sizes.raw_crop.cwidth = get2();
    }

    else if (tag == 0x9200)
    {
      int a = get4();
      if ((a == 0x01000100) || (a <= 0))
        imgdata.makernotes.fuji.FujiBrightnessCompensation = 0.0f;
      else if (a == 0x00100100)
        imgdata.makernotes.fuji.FujiBrightnessCompensation = 4.0f;
      else
        imgdata.makernotes.fuji.FujiBrightnessCompensation =
          24.0f - float(log((double)a) / log(2.0));
    }

    else if (tag == 0x9650)
    {
      short a = (short)get2();
      float b = fMAX(1.0f, get2());
      imgdata.makernotes.fuji.FujiExpoMidPointShift = a / b;
    }

    else if (tag == 0x2f00)
    {
      int nWBs = get4();
      nWBs = MIN(nWBs, 6);
      for (int wb_ind = 0; wb_ind < nWBs; wb_ind++)
      {
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Custom1 + wb_ind][c ^ 1] = get2();
        fseek(ifp, 8, SEEK_CUR);
      }
    }

    else if (tag == 0x2000)
    {
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][c ^ 1] = get2();
    }
    else if (tag == 0x2100)
    {
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_FineWeather][c ^ 1] = get2();
    }
    else if (tag == 0x2200)
    {
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Shade][c ^ 1] = get2();
    }
    else if (tag == 0x2300)
    {
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_D][c ^ 1] = get2();
    }
    else if (tag == 0x2301)
    {
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_N][c ^ 1] = get2();
    }
    else if (tag == 0x2302)
    {
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_WW][c ^ 1] = get2();
    }
    else if (tag == 0x2310)
    {
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_L][c ^ 1] = get2();
    }
    else if (tag == 0x2400)
    {
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Tungsten][c ^ 1] = get2();
    }
    else if (tag == 0x2410)
    {
      FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][c ^ 1] = get2();
    }
#endif

    else if (tag == 0xc000)
    {
      c = order;
      order = 0x4949;
      if (len > 20000) {
        if ((tag = get4()) > 10000)
          tag = get4();
        if (tag > 10000)
          tag = get4();
        width = tag;
        height = get4();
      }
#ifdef LIBRAW_LIBRARY_BUILD
      if (len == 4096) { /* X-A3, X-A5, X-A10, X-A20, X-T100, XF10 */
        int wb[4];
        int nWB, tWB, pWB;
        int iCCT = 0;
        is_4K_RAFdata = 1;
        fseek(ifp, save + 0x200, SEEK_SET);
        for (int wb_ind = 0; wb_ind < 42; wb_ind++) {
          nWB = get4();
          tWB = get4();
          wb[0] = get4() << 1;
          wb[1] = get4();
          wb[3] = get4();
          wb[2] = get4() << 1;
          if (tWB && (iCCT < 255)) {
            imgdata.color.WBCT_Coeffs[iCCT][0] = tWB;
            FORC4 imgdata.color.WBCT_Coeffs[iCCT][c + 1] = wb[c];
            iCCT++;
          }
          if (nWB != 70) {
            for (pWB = 1; pWB < nFuji_wb_list2; pWB += 2) {
              if (Fuji_wb_list2[pWB] == nWB) {
                FORC4 imgdata.color.WB_Coeffs[Fuji_wb_list2[pWB - 1]][c] = wb[c];
                break;
              }
            }
          }
        }
      } else {
        libraw_internal_data.unpacker_data.posRAFData = save;
        libraw_internal_data.unpacker_data.lenRAFData = (len >> 1);
      }
#endif
      order = c;
    }
    fseek(ifp, save + len, SEEK_SET);
  }
  height <<= fuji_layout;
  width >>= fuji_layout;
}

int CLASS parse_jpeg(int offset)
{
  int len, save, hlen, mark;
  fseek(ifp, offset, SEEK_SET);
  if (fgetc(ifp) != 0xff || fgetc(ifp) != 0xd8)
    return 0;

  while (fgetc(ifp) == 0xff && (mark = fgetc(ifp)) != 0xda)
  {
    order = 0x4d4d;
    len = get2() - 2;
    save = ftell(ifp);
    if (mark == 0xc0 || mark == 0xc3 || mark == 0xc9)
    {
      fgetc(ifp);
      raw_height = get2();
      raw_width = get2();
    }
    order = get2();
    hlen = get4();
    if (get4() == 0x48454150
#ifdef LIBRAW_LIBRARY_BUILD
        && (save + hlen) >= 0 && (save + hlen) <= ifp->size()
#endif
            ) /* "HEAP" */
    {
#ifdef LIBRAW_LIBRARY_BUILD
      imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_FixedLens;
      imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_FixedLens;
#endif
      parse_ciff(save + hlen, len - hlen, 0);
    }
    if (parse_tiff(save + 6))
      apply_tiff();
    fseek(ifp, save + len, SEEK_SET);
  }
  return 1;
}

void CLASS parse_riff()
{
  unsigned i, size, end;
  char tag[4], date[64], month[64];
  static const char mon[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  struct tm t;

  order = 0x4949;
  fread(tag, 4, 1, ifp);
  size = get4();
  end = ftell(ifp) + size;
  if (!memcmp(tag, "RIFF", 4) || !memcmp(tag, "LIST", 4))
  {
    int maxloop = 1000;
    get4();
    while (ftell(ifp) + 7 < end && !feof(ifp) && maxloop--)
      parse_riff();
  }
  else if (!memcmp(tag, "nctg", 4))
  {
    while (ftell(ifp) + 7 < end)
    {
      i = get2();
      size = get2();
      if ((i + 1) >> 1 == 10 && size == 20)
        get_timestamp(0);
      else
        fseek(ifp, size, SEEK_CUR);
    }
  }
  else if (!memcmp(tag, "IDIT", 4) && size < 64)
  {
    fread(date, 64, 1, ifp);
    date[size] = 0;
    memset(&t, 0, sizeof t);
    if (sscanf(date, "%*s %s %d %d:%d:%d %d", month, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec, &t.tm_year) == 6)
    {
      for (i = 0; i < 12 && strcasecmp(mon[i], month); i++)
        ;
      t.tm_mon = i;
      t.tm_year -= 1900;
      if (mktime(&t) > 0)
        timestamp = mktime(&t);
    }
  }
  else
    fseek(ifp, size, SEEK_CUR);
}

void CLASS parse_qt(int end)
{
  unsigned save, size;
  char tag[4];

  order = 0x4d4d;
  while (ftell(ifp) + 7 < end)
  {
    save = ftell(ifp);
    if ((size = get4()) < 8)
      return;
    if ((int)size < 0) return; // 2+GB is too much
    if (save + size < save) return; // 32bit overflow
    fread(tag, 4, 1, ifp);
    if (!memcmp(tag, "moov", 4) || !memcmp(tag, "udta", 4) || !memcmp(tag, "CNTH", 4))
      parse_qt(save + size);
    if (!memcmp(tag, "CNDA", 4))
      parse_jpeg(ftell(ifp));
    fseek(ifp, save + size, SEEK_SET);
  }
}

void CLASS parse_smal(int offset, int fsize)
{
  int ver;

  fseek(ifp, offset + 2, SEEK_SET);
  order = 0x4949;
  ver = fgetc(ifp);
  if (ver == 6)
    fseek(ifp, 5, SEEK_CUR);
  if (get4() != fsize)
    return;
  if (ver > 6)
    data_offset = get4();
  raw_height = height = get2();
  raw_width = width = get2();
  strcpy(make, "SMaL");
  sprintf(model, "v%d %dx%d", ver, width, height);
  if (ver == 6)
    load_raw = &CLASS smal_v6_load_raw;
  if (ver == 9)
    load_raw = &CLASS smal_v9_load_raw;
}

void CLASS parse_cine()
{
  unsigned off_head, off_setup, off_image, i;

  order = 0x4949;
  fseek(ifp, 4, SEEK_SET);
  is_raw = get2() == 2;
  fseek(ifp, 14, SEEK_CUR);
  is_raw *= get4();
  off_head = get4();
  off_setup = get4();
  off_image = get4();
  timestamp = get4();
  if ((i = get4()))
    timestamp = i;
  fseek(ifp, off_head + 4, SEEK_SET);
  raw_width = get4();
  raw_height = get4();
  switch (get2(), get2())
  {
  case 8:
    load_raw = &CLASS eight_bit_load_raw;
    break;
  case 16:
    load_raw = &CLASS unpacked_load_raw;
  }
  fseek(ifp, off_setup + 792, SEEK_SET);
  strcpy(make, "CINE");
  sprintf(model, "%d", get4());
  fseek(ifp, 12, SEEK_CUR);
  switch ((i = get4()) & 0xffffff)
  {
  case 3:
    filters = 0x94949494;
    break;
  case 4:
    filters = 0x49494949;
    break;
  default:
    is_raw = 0;
  }
  fseek(ifp, 72, SEEK_CUR);
  switch ((get4() + 3600) % 360)
  {
  case 270:
    flip = 4;
    break;
  case 180:
    flip = 1;
    break;
  case 90:
    flip = 7;
    break;
  case 0:
    flip = 2;
  }
  cam_mul[0] = getreal(11);
  cam_mul[2] = getreal(11);
  maximum = ~((~0u) << get4());
  fseek(ifp, 668, SEEK_CUR);
  shutter = get4() / 1000000000.0;
  fseek(ifp, off_image, SEEK_SET);
  if (shot_select < is_raw)
    fseek(ifp, shot_select * 8, SEEK_CUR);
  data_offset = (INT64)get4() + 8;
  data_offset += (INT64)get4() << 32;
}

void CLASS parse_redcine()
{
  unsigned i, len, rdvo;

  order = 0x4d4d;
  is_raw = 0;
  fseek(ifp, 52, SEEK_SET);
  width = get4();
  height = get4();
  fseek(ifp, 0, SEEK_END);
  fseek(ifp, -(i = ftello(ifp) & 511), SEEK_CUR);
  if (get4() != i || get4() != 0x52454f42)
  {
#ifdef DCRAW_VERBOSE
    fprintf(stderr, _("%s: Tail is missing, parsing from head...\n"), ifname);
#endif
    fseek(ifp, 0, SEEK_SET);
    while ((len = get4()) != EOF)
    {
      if (get4() == 0x52454456)
        if (is_raw++ == shot_select)
          data_offset = ftello(ifp) - 8;
      fseek(ifp, len - 8, SEEK_CUR);
    }
  }
  else
  {
    rdvo = get4();
    fseek(ifp, 12, SEEK_CUR);
    is_raw = get4();
    fseeko(ifp, rdvo + 8 + shot_select * 4, SEEK_SET);
    data_offset = get4();
  }
}
//@end COMMON

char *CLASS foveon_gets(int offset, char *str, int len)
{
  int i;
  fseek(ifp, offset, SEEK_SET);
  for (i = 0; i < len - 1; i++)
    if ((str[i] = get2()) == 0)
      break;
  str[i] = 0;
  return str;
}

void CLASS parse_foveon()
{
  int entries, img = 0, off, len, tag, save, i, wide, high, pent, poff[256][2];
  char name[64], value[64];
  order = 0x4949; /* Little-endian */
  fseek(ifp, 36, SEEK_SET);
  flip = get4();
  fseek(ifp, -4, SEEK_END);
  fseek(ifp, get4(), SEEK_SET);
  if (get4() != 0x64434553)
    return; /* SECd */
  entries = (get4(), get4());
  while (entries--)
  {
    off = get4();
    len = get4();
    tag = get4();
    save = ftell(ifp);
    fseek(ifp, off, SEEK_SET);

    if (get4() != (0x20434553 | (tag << 24)))
      return;
    switch (tag)
    {
    case 0x47414d49: /* IMAG */
    case 0x32414d49: /* IMA2 */
      fseek(ifp, 8, SEEK_CUR);
      pent = get4();
      wide = get4();
      high = get4();
      if (wide > raw_width && high > raw_height)
      {
        switch (pent)
        {
        case 5:
          load_flags = 1;
        case 6:
          load_raw = &CLASS foveon_sd_load_raw;
          break;
        case 30:
          load_raw = &CLASS foveon_dp_load_raw;
          break;
        default:
          load_raw = 0;
        }
        raw_width = wide;
        raw_height = high;
        data_offset = off + 28;
        is_foveon = 1;
      }
      fseek(ifp, off + 28, SEEK_SET);
      if (fgetc(ifp) == 0xff && fgetc(ifp) == 0xd8 && thumb_length < len - 28)
      {
        thumb_offset = off + 28;
        thumb_length = len - 28;
        write_thumb = &CLASS jpeg_thumb;
      }
      if (++img == 2 && !thumb_length)
      {
        thumb_offset = off + 24;
        thumb_width = wide;
        thumb_height = high;
        write_thumb = &CLASS foveon_thumb;
      }
      break;
    case 0x464d4143: /* CAMF */
      meta_offset = off + 8;
      meta_length = len - 28;
      break;
    case 0x504f5250: /* PROP */
      pent = (get4(), get4());
      fseek(ifp, 12, SEEK_CUR);
      off += pent * 8 + 24;
      if ((unsigned)pent > 256)
        pent = 256;
      for (i = 0; i < pent * 2; i++)
        ((int *)poff)[i] = off + get4() * 2;
      for (i = 0; i < pent; i++)
      {
        foveon_gets(poff[i][0], name, 64);
        foveon_gets(poff[i][1], value, 64);
        if (!strcmp(name, "ISO"))
          iso_speed = atoi(value);
        if (!strcmp(name, "CAMMANUF"))
          strcpy(make, value);
        if (!strcmp(name, "CAMMODEL"))
          strcpy(model, value);
        if (!strcmp(name, "WB_DESC"))
          strcpy(model2, value);
        if (!strcmp(name, "TIME"))
          timestamp = atoi(value);
        if (!strcmp(name, "EXPTIME"))
          shutter = atoi(value) / 1000000.0;
        if (!strcmp(name, "APERTURE"))
          aperture = atof(value);
        if (!strcmp(name, "FLENGTH"))
          focal_len = atof(value);
#ifdef LIBRAW_LIBRARY_BUILD
        if (!strcmp(name, "CAMSERIAL"))
          strcpy(imgdata.shootinginfo.BodySerial, value);
        if (!strcmp(name, "FLEQ35MM"))
          imgdata.lens.makernotes.FocalLengthIn35mmFormat = atof(value);
        if (!strcmp(name, "IMAGERTEMP"))
          imgdata.other.SensorTemperature = atof(value);
        if (!strcmp(name, "LENSARANGE"))
        {
          char *sp;
          imgdata.lens.makernotes.MaxAp4CurFocal = imgdata.lens.makernotes.MinAp4CurFocal = atof(value);
          sp = strrchr(value, ' ');
          if (sp)
          {
            imgdata.lens.makernotes.MinAp4CurFocal = atof(sp);
            if (imgdata.lens.makernotes.MaxAp4CurFocal > imgdata.lens.makernotes.MinAp4CurFocal)
              my_swap(float, imgdata.lens.makernotes.MaxAp4CurFocal, imgdata.lens.makernotes.MinAp4CurFocal);
          }
        }
        if (!strcmp(name, "LENSFRANGE"))
        {
          char *sp;
          imgdata.lens.makernotes.MinFocal = imgdata.lens.makernotes.MaxFocal = atof(value);
          sp = strrchr(value, ' ');
          if (sp)
          {
            imgdata.lens.makernotes.MaxFocal = atof(sp);
            if ((imgdata.lens.makernotes.MaxFocal + 0.17f) < imgdata.lens.makernotes.MinFocal)
              my_swap(float, imgdata.lens.makernotes.MaxFocal, imgdata.lens.makernotes.MinFocal);
          }
        }
        if (!strcmp(name, "LENSMODEL"))
        {
          char *sp;
          imgdata.lens.makernotes.LensID = strtol(value, &sp, 16); // atoi(value);
          if (imgdata.lens.makernotes.LensID)
            imgdata.lens.makernotes.LensMount = Sigma_X3F;
        }
      }
#endif
    }
#ifdef LOCALTIME
    timestamp = mktime(gmtime(&timestamp));
#endif
  }
  fseek(ifp, save, SEEK_SET);
}
}

//@out COMMON

/*
   All matrices are from Adobe DNG Converter unless otherwise noted.
 */
void CLASS adobe_coeff(const char *t_make, const char *t_model
#ifdef LIBRAW_LIBRARY_BUILD
                       ,
                       int internal_only
#endif
)
{
  // clang-format off
  static const struct
  {
    const char *prefix;
    int t_black, t_maximum, trans[12];
  } table[] = {
    { "AgfaPhoto DC-833m", 0, 0, /* DJC */
      { 11438,-3762,-1115,-2409,9914,2497,-1227,2295,5300 } },

    { "Apple QuickTake", 0, 0, /* DJC */
      { 21392,-5653,-3353,2406,8010,-415,7166,1427,2078 } },

    {"Broadcom RPi IMX219", 66, 0x3ff,
      { 5302,1083,-728,-5320,14112,1699,-863,2371,5136 } }, /* LibRaw */
    { "Broadcom RPi OV5647", 16, 0x3ff,
      { 12782,-4059,-379,-478,9066,1413,1340,1513,5176 } }, /* DJC */

    { "Canon EOS D2000", 0, 0,
      { 24542,-10860,-3401,-1490,11370,-297,2858,-605,3225 } },
    { "Canon EOS D6000", 0, 0,
      { 20482,-7172,-3125,-1033,10410,-285,2542,226,3136 } },
    { "Canon EOS D30", 0, 0, /* updated */
      { 9900,-2771,-1324,-7072,14229,3140,-2790,3344,8861 } },
    { "Canon EOS D60", 0, 0xfa0, /* updated */
      { 6211,-1358,-896,-8557,15766,3012,-3001,3507,8567 } },
    { "Canon EOS 5DS", 0, 0x3c96,
      { 6250,-711,-808,-5153,12794,2636,-1249,2198,5610 } },
    { "Canon EOS 5D Mark IV", 0, 0,
      { 6446,-366,-864,-4436,12204,2513,-952,2496,6348 } },
    { "Canon EOS 5D Mark III", 0, 0x3c80,
      { 6722,-635,-963,-4287,12460,2028,-908,2162,5668 } },
    { "Canon EOS 5D Mark II", 0, 0x3cf0,
      { 4716,603,-830,-7798,15474,2480,-1496,1937,6651 } },
    { "Canon EOS 5D", 0, 0xe6c,
      { 6347,-479,-972,-8297,15954,2480,-1968,2131,7649 } },
    { "Canon EOS 6D Mark II", 0, 0x38de,
      { 6875,-970,-932,-4691,12459,2501,-874,1953,5809 } },
    { "Canon EOS 6D", 0, 0x3c82,
      { 7034, -804, -1014, -4420, 12564, 2058, -851, 1994, 5758 } },
    { "Canon EOS 77D", 0, 0,
      { 7377,-742,-998,-4235,11981,2549,-673,1918,5538 } },
    { "Canon EOS 7D Mark II", 0, 0x3510,
      { 7268,-1082,-969,-4186,11839,2663,-825,2029,5839 } },
    { "Canon EOS 7D", 0, 0x3510,
      { 6844,-996,-856,-3876,11761,2396,-593,1772,6198 } },
    { "Canon EOS 800D", 0, 0,
      { 6970,-512,-968,-4425,12161,2553,-739,1982,5601 } },
    { "Canon EOS 80D", 0, 0,
      { 7457,-671,-937,-4849,12495,2643,-1213,2354,5492 } },
    { "Canon EOS 10D", 0, 0xfa0, /* updated */
      { 8250,-2044,-1127,-8092,15606,2664,-2893,3453,8348 } },
    { "Canon EOS 2000D", 0, 0,
      { 8532,-701,-1167,-4095,11879,2508,-797,2424,7010 } },
    { "Canon EOS 200D", 0, 0,
      { 7377,-742,-998,-4235,11981,2549,-673,1918,5538 } },
    { "Canon EOS 20Da", 0, 0,
      { 14155,-5065,-1382,-6550,14633,2039,-1623,1824,6561 } },
    { "Canon EOS 20D", 0, 0xfff,
      { 6599,-537,-891,-8071,15783,2424,-1983,2234,7462 } },
    { "Canon EOS 30D", 0, 0,
      { 6257,-303,-1000,-7880,15621,2396,-1714,1904,7046 } },
    { "Canon EOS 40D", 0, 0x3f60,
      { 6071,-747,-856,-7653,15365,2441,-2025,2553,7315 } },
    { "Canon EOS 50D", 0, 0x3d93,
      { 4920,616,-593,-6493,13964,2784,-1774,3178,7005 } },
    { "Canon EOS 60Da", 0, 0x2ff7, /* added */
      { 17492,-7240,-2023,-1791,10323,1701,-186,1329,5406 } },
    { "Canon EOS 60D", 0, 0x2ff7,
      { 6719,-994,-925,-4408,12426,2211,-887,2129,6051 } },
    { "Canon EOS 70D", 0, 0x3bc7,
      { 7034,-804,-1014,-4420,12564,2058,-851,1994,5758 } },
    { "Canon EOS 100D", 0, 0x350f,
      { 6602,-841,-939,-4472,12458,2247,-975,2039,6148 } },
    { "Canon EOS 300D", 0, 0xfa0, /* updated */
      { 8250,-2044,-1127,-8092,15606,2664,-2893,3453,8348 } },
    { "Canon EOS 350D", 0, 0xfff,
      { 6018,-617,-965,-8645,15881,2975,-1530,1719,7642 } },
    { "Canon EOS 4000D", 0, 0,
      { 6939,-1016,-866,-4428,12473,2177,-1175,2178,6162 } },
    { "Canon EOS 400D", 0, 0xe8e,
      { 7054,-1501,-990,-8156,15544,2812,-1278,1414,7796 } },
    { "Canon EOS 450D", 0, 0x390d,
      { 5784,-262,-821,-7539,15064,2672,-1982,2681,7427 } },
    { "Canon EOS 500D", 0, 0x3479,
      { 4763,712,-646,-6821,14399,2640,-1921,3276,6561 } },
    { "Canon EOS 550D", 0, 0x3dd7,
      { 6941,-1164,-857,-3825,11597,2534,-416,1540,6039 } },
    { "Canon EOS 600D", 0, 0x3510,
      { 6461,-907,-882,-4300,12184,2378,-819,1944,5931 } },
    { "Canon EOS 650D", 0, 0x354d,
      { 6602,-841,-939,-4472,12458,2247,-975,2039,6148 } },
    { "Canon EOS 750D", 0, 0x3c00,
      { 6362,-823,-847,-4426,12109,2616,-743,1857,5635 } },
    { "Canon EOS 760D", 0, 0x3c00,
      { 6362,-823,-847,-4426,12109,2616,-743,1857,5635 } },
    { "Canon EOS 700D", 0, 0x3c00,
      { 6602,-841,-939,-4472,12458,2247,-975,2039,6148 } },
    { "Canon EOS 1000D", 0, 0xe43,
      { 6771,-1139,-977,-7818,15123,2928,-1244,1437,7533 } },
    { "Canon EOS 1100D", 0, 0x3510,
      { 6444,-904,-893,-4563,12308,2535,-903,2016,6728 } },
    { "Canon EOS 1200D", 0, 0x37c2,
      { 6461,-907,-882,-4300,12184,2378,-819,1944,5931 } },
    { "Canon EOS 1300D", 0, 0x37c2,
      { 6939,-1016,-866,-4428,12473,2177,-1175,2178,6162 } },

    { "Canon EOS R", 0, 0, /* CR3 */
      { 8293,-1789,-1094,-5025,12925,2327,-1199,2769,6108 } },

    { "Canon EOS M6", 0, 0,
      { 8532,-701,-1167,-4095,11879,2508,-797,2424,7010 } },
    { "Canon EOS M50", 0, 0, /* CR3 */
      { 8532,-701,-1167,-4095,11879,2508,-797,2424,7010 } },
    { "Canon EOS M5", 0, 0,
      { 8532,-701,-1167,-4095,11879,2508,-797,2424,7010 } },
    { "Canon EOS M3", 0, 0,
      { 6362,-823,-847,-4426,12109,2616,-743,1857,5635 } },
    { "Canon EOS M2", 0, 0, /* added */
      { 6400,-480,-888,-5294,13416,2047,-1296,2203,6137 } },
    { "Canon EOS M100", 0, 0,
      { 8532,-701,-1167,-4095,11879,2508,-797,2424,7010 } },
    { "Canon EOS M10", 0, 0,
      { 6400,-480,-888,-5294,13416,2047,-1296,2203,6137 } },
    { "Canon EOS M", 0, 0,
      { 6602,-841,-939,-4472,12458,2247,-975,2039,6148 } },

    { "Canon EOS-1Ds Mark III", 0, 0x3bb0,
      { 5859,-211,-930,-8255,16017,2353,-1732,1887,7448 } },
    { "Canon EOS-1Ds Mark II", 0, 0xe80,
      { 6517,-602,-867,-8180,15926,2378,-1618,1771,7633 } },
    { "Canon EOS-1D Mark IV", 0, 0x3bb0,
      { 6014,-220,-795,-4109,12014,2361,-561,1824,5787 } },
    { "Canon EOS-1D Mark III", 0, 0x3bb0,
      { 6291,-540,-976,-8350,16145,2311,-1714,1858,7326 } },
    { "Canon EOS-1D Mark II N", 0, 0xe80,
      { 6240,-466,-822,-8180,15825,2500,-1801,1938,8042 } },
    { "Canon EOS-1D Mark II", 0, 0xe80,
      { 6264,-582,-724,-8312,15948,2504,-1744,1919,8664 } },
    { "Canon EOS-1DS", 0, 0xe20, /* updated */
      { 3925,4060,-1739,-8973,16552,2545,-3287,3945,8243 } },
    { "Canon EOS-1D C", 0, 0x3c4e,
      { 6847,-614,-1014,-4669,12737,2139,-1197,2488,6846 } },
    { "Canon EOS-1D X Mark II", 0, 0x3c4e, /* updated */
      { 7596,-978,-967,-4808,12571,2503,-1398,2567,5752 } },
    { "Canon EOS-1D X", 0, 0x3c4e,
      { 6847,-614,-1014,-4669,12737,2139,-1197,2488,6846 } },
    { "Canon EOS-1D", 0, 0xe20,
      { 6806,-179,-1020,-8097,16415,1687,-3267,4236,7690 } },
    { "Canon EOS C500", 853, 0, /* DJC */
      { 17851,-10604,922,-7425,16662,763,-3660,3636,22278 } },
    {"Canon PowerShot 600", 0, 0, /* added */
      { -3822,10019,1311,4085,-157,3386,-5341,10829,4812,-1969,10969,1126 } },
    { "Canon PowerShot A530", 0, 0,
      { 0 } }, /* don't want the A5 matrix */
    { "Canon PowerShot A50", 0, 0,
      { -5300,9846,1776,3436,684,3939,-5540,9879,6200,-1404,11175,217 } },
    { "Canon PowerShot A5", 0, 0,
      { -4801,9475,1952,2926,1611,4094,-5259,10164,5947,-1554,10883,547 } },
    { "Canon PowerShot G10", 0, 0,
      { 11093,-3906,-1028,-5047,12492,2879,-1003,1750,5561 } },
    { "Canon PowerShot G11", 0, 0,
      { 12177,-4817,-1069,-1612,9864,2049,-98,850,4471 } },
    { "Canon PowerShot G12", 0, 0,
      { 13244,-5501,-1248,-1508,9858,1935,-270,1083,4366 } },
    { "Canon PowerShot G15", 0, 0,
      { 7474,-2301,-567,-4056,11456,2975,-222,716,4181 } },
    { "Canon PowerShot G16", 0, 0, /* updated */
      { 8020,-2687,-682,-3704,11879,2052,-965,1921,5556 } },
    { "Canon PowerShot G1 X Mark III", 0, 0,
      { 8532,-701,-1167,-4095,11879,2508,-797,2424,7010 } },
    { "Canon PowerShot G1 X Mark II", 0, 0,
      { 7378,-1255,-1043,-4088,12251,2048,-876,1946,5805 } },
    { "Canon PowerShot G1 X", 0, 0,
      { 7378,-1255,-1043,-4088,12251,2048,-876,1946,5805 } },
    { "Canon PowerShot G1", 0, 0, /* updated */
      { -5686,10300,2223,4725,-1157,4383,-6128,10783,6163,-2688,12093,604 } },
    { "Canon PowerShot G2", 0, 0, /* updated */
      { 9194,-2787,-1059,-8098,15657,2608,-2610,3064,7867 } },
    { "Canon PowerShot G3 X", 0, 0,
      { 9701,-3857,-921,-3149,11537,1817,-786,1817,5147 } },
    { "Canon PowerShot G3", 0, 0, /* updated */
      { 9326,-2882,-1084,-7940,15447,2677,-2620,3090,7740 } },
    { "Canon PowerShot G5 X",0, 0,
      { 9602,-3823,-937,-2984,11495,1675,-407,1415,5049 } },
    { "Canon PowerShot G5", 0, 0, /* updated */
      { 9869,-2972,-942,-7314,15098,2369,-1898,2536,7282 } },
    { "Canon PowerShot G6", 0, 0,
      { 9877,-3775,-871,-7613,14807,3072,-1448,1305,7485 } },
    { "Canon PowerShot G7 X Mark II", 0, 0,
      { 9602,-3823,-937,-2984,11495,1675,-407,1415,5049 } },
    { "Canon PowerShot G7 X", 0, 0,
      { 9602,-3823,-937,-2984,11495,1675,-407,1415,5049 } },
    { "Canon PowerShot G9 X Mark II", 0, 0,
      { 10056,-4131,-944,-2576,11143,1625,-238,1294,5179 } },
    { "Canon PowerShot G9 X",0, 0,
      { 9602,-3823,-937,-2984,11495,1675,-407,1415,5049 } },
    { "Canon PowerShot G9", 0, 0,
      { 7368,-2141,-598,-5621,13254,2625,-1418,1696,5743 } },
    { "Canon PowerShot Pro1", 0, 0,
      { 10062,-3522,-999,-7643,15117,2730,-765,817,7323 } },
    { "Canon PowerShot Pro70", 34, 0, /* updated */
      { -5106,10695,1576,3820,53,4566,-6497,10736,6701,-3336,11887,1394 } },
    { "Canon PowerShot Pro90", 0, 0, /* updated */
      { -5912,10768,2288,4612,-989,4333,-6153,10897,5944,-2907,12288,624 } },
    { "Canon PowerShot S30", 0, 0, /* updated */
      { 10744,-3813,-1142,-7962,15966,2075,-2492,2805,7744 } },
    { "Canon PowerShot S40", 0, 0, /* updated */
      { 8606,-2573,-949,-8237,15489,2974,-2649,3076,9100 } },
    { "Canon PowerShot S45", 0, 0, /* updated */
      { 8251,-2410,-964,-8047,15430,2823,-2380,2824,8119 } },
    { "Canon PowerShot S50", 0, 0, /* updated */
      { 8979,-2658,-871,-7721,15500,2357,-1773,2366,6634 } },
    { "Canon PowerShot S60", 0, 0,
      { 8795,-2482,-797,-7804,15403,2573,-1422,1996,7082 } },
    { "Canon PowerShot S70", 0, 0,
      { 9976,-3810,-832,-7115,14463,2906,-901,989,7889 } },
    { "Canon PowerShot S90", 0, 0,
      { 12374,-5016,-1049,-1677,9902,2078,-83,852,4683 } },
    { "Canon PowerShot S95", 0, 0,
      { 13440,-5896,-1279,-1236,9598,1931,-180,1001,4651 } },
    { "Canon PowerShot S120", 0, 0,
      { 6961,-1685,-695,-4625,12945,1836,-1114,2152,5518 } },
    { "Canon PowerShot S110", 0, 0,
      { 8039,-2643,-654,-3783,11230,2930,-206,690,4194 } },
    { "Canon PowerShot S100", 0, 0,
      { 7968,-2565,-636,-2873,10697,2513,180,667,4211 } },
    { "Canon PowerShot SX1 IS", 0, 0,
      { 6578,-259,-502,-5974,13030,3309,-308,1058,4970 } },
    { "Canon PowerShot SX50 HS", 0, 0,
      { 12432,-4753,-1247,-2110,10691,1629,-412,1623,4926 } },
    { "Canon PowerShot SX60 HS", 0, 0,
      { 13161,-5451,-1344,-1989,10654,1531,-47,1271,4955 } },
    { "Canon PowerShot SX70 HS", 0, 0, /* CR3 */
      { 18285,-8907,-1951,-1845,10688,1323,364,1101,5139 } },
    { "Canon PowerShot A3300", 0, 0, /* DJC */
      { 10826,-3654,-1023,-3215,11310,1906,0,999,4960 } },
    { "Canon PowerShot A470", 0, 0, /* DJC */
      { 12513,-4407,-1242,-2680,10276,2405,-878,2215,4734 } },
    { "Canon PowerShot A610", 0, 0, /* DJC */
      { 15591,-6402,-1592,-5365,13198,2168,-1300,1824,5075 } },
    { "Canon PowerShot A620", 0, 0, /* DJC */
      { 15265,-6193,-1558,-4125,12116,2010,-888,1639,5220 } },
    { "Canon PowerShot A630", 0, 0, /* DJC */
      { 14201,-5308,-1757,-6087,14472,1617,-2191,3105,5348 } },
    { "Canon PowerShot A640", 0, 0, /* DJC */
      { 13124,-5329,-1390,-3602,11658,1944,-1612,2863,4885 } },
    { "Canon PowerShot A650", 0, 0, /* DJC */
      { 9427,-3036,-959,-2581,10671,1911,-1039,1982,4430 } },
    { "Canon PowerShot A720", 0, 0, /* DJC */
      { 14573,-5482,-1546,-1266,9799,1468,-1040,1912,3810 } },
    { "Canon PowerShot D10", 127, 0, /* DJC */
      { 14052,-5229,-1156,-1325,9420,2252,-498,1957,4116 } },
    { "Canon PowerShot S3 IS", 0, 0, /* DJC */
      { 14062,-5199,-1446,-4712,12470,2243,-1286,2028,4836 } },
    { "Canon PowerShot SX110 IS", 0, 0, /* DJC */
      { 14134,-5576,-1527,-1991,10719,1273,-1158,1929,3581 } },
    { "Canon PowerShot SX220", 0, 0, /* DJC */
      { 13898,-5076,-1447,-1405,10109,1297,-244,1860,3687 } },
    { "Canon IXUS 160", 0, 0, /* DJC */
      { 11657,-3781,-1136,-3544,11262,2283,-160,1219,4700 } },

    { "Casio EX-F1", 0, 0, /* added */
      { 9084,-2016,-848,-6711,14351,2570,-1059,1725,6135 } },
    { "Casio EX-FH100", 0, 0, /* added */
      { 12771,-4179,-1558,-2149,10938,1375,-453,1751,4494 } },
    { "Casio EX-S20", 0, 0, /* DJC */
      { 11634,-3924,-1128,-4968,12954,2015,-1588,2648,7206 } },
    { "Casio EX-Z750", 0, 0, /* DJC */
      { 10819,-3873,-1099,-4903,13730,1175,-1755,3751,4632 } },
    { "Casio EX-Z10", 128, 0xfff, /* DJC */
      { 9790,-3338,-603,-2321,10222,2099,-344,1273,4799 } },

    { "CINE 650", 0, 0,
      { 3390,480,-500,-800,3610,340,-550,2336,1192 } },
    { "CINE 660", 0, 0,
      { 3390,480,-500,-800,3610,340,-550,2336,1192 } },
    { "CINE", 0, 0,
      { 20183,-4295,-423,-3940,15330,3985,-280,4870,9800 } },

    { "Contax N Digital", 0, 0xf1e,
      { 7777,1285,-1053,-9280,16543,2916,-3677,5679,7060 } },

    { "DXO ONE", 0, 0,
      { 6596,-2079,-562,-4782,13016,1933,-970,1581,5181 } },

    { "Epson R-D1", 0, 0,
      { 6827,-1878,-732,-8429,16012,2564,-704,592,7145 } },

    { "Fujifilm E550", 0, 0, /* updated */
      { 11044,-3888,-1120,-7248,15167,2208,-1531,2276,8069 } },
    { "Fujifilm E900", 0, 0,
      { 9183,-2526,-1078,-7461,15071,2574,-2022,2440,8639 } },
    { "Fujifilm F5", 0, 0,
      { 13690,-5358,-1474,-3369,11600,1998,-132,1554,4395 } },
    { "Fujifilm F6", 0, 0,
      { 13690,-5358,-1474,-3369,11600,1998,-132,1554,4395 } },
    { "Fujifilm F77", 0, 0xfe9,
      { 13690,-5358,-1474,-3369,11600,1998,-132,1554,4395 } },
    { "Fujifilm F7", 0, 0,
      { 10004,-3219,-1201,-7036,15047,2107,-1863,2565,7736 } },
    { "Fujifilm F810", 0, 0, /* added */
      { 11044,-3888,-1120,-7248,15167,2208,-1531,2276,8069 } },
    { "Fujifilm F8", 0, 0,
      { 13690,-5358,-1474,-3369,11600,1998,-132,1554,4395 } },
    { "Fujifilm S100FS", 514, 0,
      { 11521,-4355,-1065,-6524,13767,3058,-1466,1984,6045 } },
    { "Fujifilm S1", 0, 0,
      { 12297,-4882,-1202,-2106,10691,1623,-88,1312,4790 } },
    { "Fujifilm S20Pro", 0, 0,
      { 10004,-3219,-1201,-7036,15047,2107,-1863,2565,7736 } },
    { "Fujifilm S20", 512, 0x3fff,
      { 11401,-4498,-1312,-5088,12751,2613,-838,1568,5941 } },
    { "Fujifilm S2Pro", 128, 0, /* updated */
      { 12741,-4916,-1420,-8510,16791,1715,-1767,2302,7771 } },

    { "Fujifilm DBP for GX680", 128, 0x0fff, /* temp */
      { 12741,-4916,-1420,-8510,16791,1715,-1767,2302,7771 } }, // copy from S2Pro

    { "Fujifilm S3Pro", 0, 0,
      { 11807,-4612,-1294,-8927,16968,1988,-2120,2741,8006 } },
    { "Fujifilm S5Pro", 0, 0,
      { 12300,-5110,-1304,-9117,17143,1998,-1947,2448,8100 } },
    { "Fujifilm S5000", 0, 0,
      { 8754,-2732,-1019,-7204,15069,2276,-1702,2334,6982 } },
    { "Fujifilm S5100", 0, 0,
      { 11940,-4431,-1255,-6766,14428,2542,-993,1165,7421 } },
    { "Fujifilm S5500", 0, 0,
      { 11940,-4431,-1255,-6766,14428,2542,-993,1165,7421 } },
    { "Fujifilm S5200", 0, 0,
      { 9636,-2804,-988,-7442,15040,2589,-1803,2311,8621 } },
    { "Fujifilm S5600", 0, 0,
      { 9636,-2804,-988,-7442,15040,2589,-1803,2311,8621 } },
    { "Fujifilm S6", 0, 0,
      { 12628,-4887,-1401,-6861,14996,1962,-2198,2782,7091 } },
    { "Fujifilm S7000", 0, 0,
      { 10190,-3506,-1312,-7153,15051,2238,-2003,2399,7505 } },
    { "Fujifilm S9000", 0, 0,
      { 10491,-3423,-1145,-7385,15027,2538,-1809,2275,8692 } },
    { "Fujifilm S9500", 0, 0,
      { 10491,-3423,-1145,-7385,15027,2538,-1809,2275,8692 } },
    { "Fujifilm S9100", 0, 0,
      { 12343,-4515,-1285,-7165,14899,2435,-1895,2496,8800 } },
    { "Fujifilm S9600", 0, 0,
      { 12343,-4515,-1285,-7165,14899,2435,-1895,2496,8800 } },
    { "Fujifilm SL1000", 0, 0,
      { 11705,-4262,-1107,-2282,10791,1709,-555,1713,4945 } },
    { "Fujifilm IS-1", 0, 0,
      { 21461,-10807,-1441,-2332,10599,1999,289,875,7703 } },
    { "Fujifilm IS Pro", 0, 0,
      { 12300,-5110,-1304,-9117,17143,1998,-1947,2448,8100 } },
    { "Fujifilm HS10 HS11", 0, 0xf68,
      { 12440,-3954,-1183,-1123,9674,1708,-83,1614,4086 } },
    { "Fujifilm HS2", 0, 0,
      { 13690,-5358,-1474,-3369,11600,1998,-132,1554,4395 } },
    { "Fujifilm HS3", 0, 0,
      { 13690,-5358,-1474,-3369,11600,1998,-132,1554,4395 } },
    { "Fujifilm HS50EXR", 0, 0,
      { 12085,-4727,-953,-3257,11489,2002,-511,2046,4592 } },
    { "Fujifilm F900EXR", 0, 0,
      { 12085,-4727,-953,-3257,11489,2002,-511,2046,4592 } },
    { "Fujifilm X100S", 0, 0,
      { 10592,-4262,-1008,-3514,11355,2465,-870,2025,6386 } },
    { "Fujifilm X100F", 0, 0,
      { 11434,-4948,-1210,-3746,12042,1903,-666,1479,5235 } },
    { "Fujifilm X100T", 0, 0,
      { 10592,-4262,-1008,-3514,11355,2465,-870,2025,6386 } },
    { "Fujifilm X100", 0, 0,
      { 12161,-4457,-1069,-5034,12874,2400,-795,1724,6904 } },
    { "Fujifilm X10", 0, 0,
      { 13509,-6199,-1254,-4430,12733,1865,-331,1441,5022 } },
    { "Fujifilm X20", 0, 0,
      { 11768,-4971,-1133,-4904,12927,2183,-480,1723,4605 } },
    { "Fujifilm X30", 0, 0,
      { 12328,-5256,-1144,-4469,12927,1675,-87,1291,4351 } },
    { "Fujifilm X70", 0, 0,
      { 10450,-4329,-878,-3217,11105,2421,-752,1758,6519 } },
    { "Fujifilm X-Pro1", 0, 0,
      { 10413,-3996,-993,-3721,11640,2361,-733,1540,6011 } },
    { "Fujifilm X-Pro2", 0, 0,
      { 11434,-4948,-1210,-3746,12042,1903,-666,1479,5235 } },
    { "Fujifilm X-A10", 0, 0,
      { 11540,-4999,-991,-2949,10963,2278,-382,1049,5605} },
    { "Fujifilm X-A20", 0, 0,
      { 11540,-4999,-991,-2949,10963,2278,-382,1049,5605} },
    { "Fujifilm X-A1", 0, 0,
      { 11086,-4555,-839,-3512,11310,2517,-815,1341,5940 } },
    { "Fujifilm X-A2", 0, 0,
      { 10763,-4560,-917,-3346,11311,2322,-475,1135,5843 } },
    { "Fujifilm X-A3", 0, 0,
      { 12407,-5222,-1086,-2971,11116,2120,-294,1029,5284 } },
    { "Fujifilm X-A5", 0, 0,
      { 11673,-4760,-1041,-3988,12058,2166,-771,1417,5569 } },
    { "Fujifilm X-E1", 0, 0,
      { 10413,-3996,-993,-3721,11640,2361,-733,1540,6011 } },
    { "Fujifilm X-E2S", 0, 0,
      { 11562,-5118,-961,-3022,11007,2311,-525,1569,6097 } },
    { "Fujifilm X-E2", 0, 0,
      { 8458,-2451,-855,-4597,12447,2407,-1475,2482,6526 } },
    { "Fujifilm X-E3", 0, 0,
      { 11434,-4948,-1210,-3746,12042,1903,-666,1479,5235 } },
    { "Fujifilm XF10", 0, 0,
      { 11673,-4760,-1041,-3988,12058,2166,-771,1417,5569 } },
    { "Fujifilm XF1", 0, 0,
      { 13509,-6199,-1254,-4430,12733,1865,-331,1441,5022 } },
    { "Fujifilm X-M1", 0, 0,
      { 10413,-3996,-993,-3721,11640,2361,-733,1540,6011 } },
    { "Fujifilm X-S1", 0, 0,
      { 13509,-6199,-1254,-4430,12733,1865,-331,1441,5022 } },
    { "Fujifilm X-T30", 0, 0, /* temp */
      { 13426,-6334,-1177,-4244,12136,2371,580,1303,5980 } },
    { "Fujifilm X-T3", 0, 0,
      { 13426,-6334,-1177,-4244,12136,2371,580,1303,5980 } },
    { "Fujifilm X-T20", 0, 0,
      { 11434,-4948,-1210,-3746,12042,1903,-666,1479,5235 } },
    { "Fujifilm X-T2", 0, 0,
      { 11434,-4948,-1210,-3746,12042,1903,-666,1479,5235 } },
    { "Fujifilm X-T100", 0, 0,
      { 11673,-4760,-1041,-3988,12058,2166,-771,1417,5569 } },
    { "Fujifilm X-T10", 0, 0, /* updated */
      { 8458,-2451,-855,-4597,12447,2407,-1475,2482,6526 } },
    { "Fujifilm X-T1", 0, 0,
      { 8458,-2451,-855,-4597,12447,2407,-1475,2482,6526 } },
    { "Fujifilm X-H1", 0, 0,
      { 11434,-4948,-1210,-3746,12042,1903,-666,1479,5235 } },
    { "Fujifilm XQ1", 0, 0,
      { 9252,-2704,-1064,-5893,14265,1717,-1101,2341,4349 } },
    { "Fujifilm XQ2", 0, 0,
      { 9252,-2704,-1064,-5893,14265,1717,-1101,2341,4349 } },
    { "Fujifilm GFX 50", 0, 0, /* 50S, 50R */
      { 11756,-4754,-874,-3056,11045,2305,-381,1457,6006 } },

    { "GITUP G3DUO", 130, 62000,
       { 8489,-2583,-1036,-8051,15583,2643,-1307,1407,7354 } },

    { "GITUP GIT2P", 4160, 0,
      { 8489,-2583,-1036,-8051,15583,2643,-1307,1407,7354 } },
    { "GITUP GIT2", 3200, 0,
      { 8489,-2583,-1036,-8051,15583,2643,-1307,1407,7354 } },

    { "GoPro HERO5 Black", 0, 0,
	    { 10344,-4210,-620,-2315,10625,1948,93,1058,5541 } },

    { "Hasselblad HV", 0, 0,                                /* Sony SLT-A99     */
      { 6344,-1612,-461,-4862,12476,2680,-864,1785,6898 } },
    { "Hasselblad Lunar", 0, 0,                             /* Sony NEX-7       */
      { 5491,-1192,-363,-4951,12342,2948,-911,1722,7192 } },
    { "Hasselblad Lusso", 0, 0,                             /* Sony ILCE-7R     */
      { 4912,-540,-201,-6129,13513,2906,-1563,2151,7182 } },
    { "Hasselblad Stellar II", -800, 0,                     /* Sony DSC-RX100M2 */
      { 6596,-2079,-562,-4782,13016,1933,-970,1581,5181 } },
    { "Hasselblad Stellar", -800, 0,                        /* Sony DSC-RX100   */
      { 8651,-2754,-1057,-3464,12207,1373,-568,1398,4434 } },

    { "Hasselblad 500 mech.", 0, 0, /* added */
      { 8519,-3260,-280,-5081,13459,1738,-1449,2960,7809 } },
    { "Hasselblad CFV", 0, 0,
      { 8519,-3260,-280,-5081,13459,1738,-1449,2960,7809 } },
    { "Hasselblad H-16MP", 0, 0, /* LibRaw */
      { 17765,-5322,-1734,-6168,13354,2135,-264,2524,7440 } },
    { "Hasselblad H-22MP", 0, 0, /* LibRaw */
      { 17765,-5322,-1734,-6168,13354,2135,-264,2524,7440 } },
    { "Hasselblad H-31MP",0, 0, /* LibRaw */
      { 14480,-5448,-1686,-3534,13123,2260,384,2952,7232 } },
    { "Hasselblad 39-Coated", 0, 0, /* added */
      { 3857,452,-46,-6008,14477,1596,-2627,4481,5718 } },
    { "Hasselblad H-39MP",0, 0,
      { 3857,452,-46,-6008,14477,1596,-2627,4481,5718 } },
    { "Hasselblad H2D-39", 0, 0, /* added */
      { 3894,-110,287,-4672,12610,2295,-2092,4100,6196 } },
    { "Hasselblad H3D-50", 0, 0,
      { 3857,452,-46,-6008,14477,1596,-2627,4481,5718 } },
    { "Hasselblad H3D", 0, 0, /* added */
      { 3857,452,-46,-6008,14477,1596,-2627,4481,5718 } },
    { "Hasselblad H4D-40",0, 0, /* LibRaw */
      { 6325,-860,-957,-6559,15945,266,167,770,5936 } },
    { "Hasselblad H4D-50",0, 0, /* LibRaw */
      { 15283,-6272,-465,-2030,16031,478,-2379,390,7965 } },
    { "Hasselblad H4D-60",0, 0,
      { 9662,-684,-279,-4903,12293,2950,-344,1669,6024 } },
    { "Hasselblad H5D-50c",0, 0,
      { 4932,-835,141,-4878,11868,3437,-1138,1961,7067 } },
    { "Hasselblad H5D-50",0, 0,
      { 5656,-659,-346,-3923,12306,1791,-1602,3509,5442 } },
    { "Hasselblad H6D-100c",0, 0,
      { 5110,-1357,-308,-5573,12835,3077,-1279,2025,7010 } },
    { "Hasselblad X1D",0, 0,
      { 4932,-835,141,-4878,11868,3437,-1138,1961,7067 } },

    { "HTC One A9", 64, 1023, /* this is CM1 transposed */
      { 101,-20,-2,-11,145,41,-24,1,56 } },

    { "Imacon Ixpress", 0, 0, /* DJC */
      { 7025,-1415,-704,-5188,13765,1424,-1248,2742,6038 } },

    { "Kodak NC2000", 0, 0,
      { 13891,-6055,-803,-465,9919,642,2121,82,1291 } },
    { "Kodak DCS315C", -8, 0,
      { 17523,-4827,-2510,756,8546,-137,6113,1649,2250 } },
    { "Kodak DCS330C", -8, 0,
      { 20620,-7572,-2801,-103,10073,-396,3551,-233,2220 } },
    { "Kodak DCS420", 0, 0,
      { 10868,-1852,-644,-1537,11083,484,2343,628,2216 } },
    { "Kodak DCS460", 0, 0,
      { 10592,-2206,-967,-1944,11685,230,2206,670,1273 } },
    { "Kodak EOSDCS1", 0, 0,
      { 10592,-2206,-967,-1944,11685,230,2206,670,1273 } },
    { "Kodak EOSDCS3B", 0, 0,
      { 9898,-2700,-940,-2478,12219,206,1985,634,1031 } },
    { "Kodak DCS520C", -178, 0,
      { 24542,-10860,-3401,-1490,11370,-297,2858,-605,3225 } },
    { "Kodak DCS560C", -177, 0,
      { 20482,-7172,-3125,-1033,10410,-285,2542,226,3136 } },
    { "Kodak DCS620C", -177, 0,
      { 23617,-10175,-3149,-2054,11749,-272,2586,-489,3453 } },
    { "Kodak DCS620X", -176, 0,
      { 13095,-6231,154,12221,-21,-2137,895,4602,2258 } },
    { "Kodak DCS660C", -173, 0,
      { 18244,-6351,-2739,-791,11193,-521,3711,-129,2802 } },
    { "Kodak DCS720X", 0, 0,
      { 11775,-5884,950,9556,1846,-1286,-1019,6221,2728 } },
    { "Kodak DCS760C", 0, 0,
      { 16623,-6309,-1411,-4344,13923,323,2285,274,2926 } },
    { "Kodak DCS Pro SLR", 0, 0,
      { 5494,2393,-232,-6427,13850,2846,-1876,3997,5445 } },
    { "Kodak DCS Pro 14nx", 0, 0,
      { 5494,2393,-232,-6427,13850,2846,-1876,3997,5445 } },
    { "Kodak DCS Pro 14", 0, 0,
      { 7791,3128,-776,-8588,16458,2039,-2455,4006,6198 } },
    { "Photo Control Camerz ZDS 14", 0, 0,
      { 7791,3128,-776,-8588,16458,2039,-2455,4006,6198 } },
    { "Kodak ProBack645", 0, 0,
      { 16414,-6060,-1470,-3555,13037,473,2545,122,4948 } },
    { "Kodak ProBack", 0, 0,
      { 21179,-8316,-2918,-915,11019,-165,3477,-180,4210 } },
    { "Kodak P712", 0, 3963,
      { 9658,-3314,-823,-5163,12695,2768,-1342,1843,6044 } },
    { "Kodak P850", 0, 3964,
      { 10511,-3836,-1102,-6946,14587,2558,-1481,1792,6246 } },
    { "Kodak P880", 0, 3963,
      { 12805,-4662,-1376,-7480,15267,2360,-1626,2194,7904 } },
    { "Kodak EasyShare Z980", 0, 0,
      { 11313,-3559,-1101,-3893,11891,2257,-1214,2398,4908 } },
    { "Kodak EasyShare Z981", 0, 0,
      { 12729,-4717,-1188,-1367,9187,2582,274,860,4411 } },
    { "Kodak EasyShare Z990", 0, 0xfed,
      { 11749,-4048,-1309,-1867,10572,1489,-138,1449,4522 } },
    { "Kodak EASYSHARE Z1015", 0, 0xef1,
      { 11265,-4286,-992,-4694,12343,2647,-1090,1523,5447 } },

    { "Leaf C-Most", 0, 0, /* updated */
      { 3952,2189,449,-6701,14585,2275,-4536,7349,6536 } },
    { "Leaf Valeo 6", 0, 0,
      { 3952,2189,449,-6701,14585,2275,-4536,7349,6536 } },
    { "Leaf Aptus 54S", 0, 0,
      { 8236,1746,-1314,-8251,15953,2428,-3673,5786,5771 } },
    { "Leaf Aptus-II 8", 0, 0, /* added */
      { 7361,1257,-163,-6929,14061,3176,-1839,3454,5603 } },
    { "Leaf AFi-II 7", 0, 0, /* added */
      { 7691,-108,-339,-6185,13627,2833,-2046,3899,5952 } },
    { "Leaf Aptus-II 5", 0, 0, /* added */
      { 7914,1414,-1190,-8777,16582,2280,-2811,4605,5562 } },
    { "Leaf Aptus 65", 0, 0,
      { 7914,1414,-1190,-8777,16582,2280,-2811,4605,5562 } },
    { "Leaf AFi 65S", 0, 0, /* added */
      { 7914,1414,-1190,-8777,16582,2280,-2811,4605,5562 } },
    { "Leaf Aptus 75", 0, 0,
      { 7914,1414,-1190,-8777,16582,2280,-2811,4605,5562 } },
    { "Leaf AFi 75S", 0, 0, /* added */
      { 7914,1414,-1190,-8777,16582,2280,-2811,4605,5562 } },
    { "Leaf Credo 40", 0, 0,
      { 8035,435,-962,-6001,13872,2320,-1159,3065,5434 } },
    { "Leaf Credo 50", 0, 0,
      { 3984,0,0,0,10000,0,0,0,7666 } },
    { "Leaf Credo 60", 0, 0,
      { 8035,435,-962,-6001,13872,2320,-1159,3065,5434 } },
    { "Leaf Credo 80", 0, 0,
      { 6294,686,-712,-5435, 13417,2211,-1006,2435,5042 } },
    { "Leaf", 0, 0,
      { 8236,1746,-1314,-8251,15953,2428,-3673,5786,5771 } },

    { "Leica M10", 0, 0, /* added */
      { 9090,-3342,-740,-4006,13456,493,-569,2266,6871 } },
    { "Leica M9", 0, 0, /* added */
      { 6687,-1751,-291,-3556,11373,2492,-548,2204,7146 } },
    { "Leica M8", 0, 0, /* added */
      { 7675,-2196,-305,-5860,14119,1856,-2425,4006,6578 } },
    { "Leica M (Typ 240)", 0, 0, /* added */
      { 7199,-2140,-712,-4005,13327,649,-810,2521,6673 } },
    { "Leica M (Typ 262)", 0, 0,
      { 7199,-2140,-712,-4005,13327,649,-810,2521,6673 } },
    { "Leica SL (Typ 601)", 0, 0,
      { 11865,-4523,-1441,-5423,14458,935,-1587,2687,4830} },
    { "Leica S2", 0, 0, /* added */
      { 5627,-721,-447,-4423,12456,2192,-1048,2948,7379 } },
    {"Leica S-E (Typ 006)", 0, 0, /* added */
      { 5749,-1072,-382,-4274,12432,2048,-1166,3104,7105 } },
    {"Leica S (Typ 006)", 0, 0, /* added */
      { 5749,-1072,-382,-4274,12432,2048,-1166,3104,7105 } },
    { "Leica S (Typ 007)", 0, 0,
      { 6063,-2234,-231,-5210,13787,1500,-1043,2866,6997 } },
    { "Leica Q (Typ 116)", 0, 0, /* updated */
      { 10068,-4043,-1068,-5319,14268,1044,-765,1701,6522 } },
    { "Leica T (Typ 701)", 0, 0, /* added */
      { 6295 ,-1679 ,-475 ,-5586 ,13046 ,2837 ,-1410 ,1889 ,7075 } },
    { "Leica X2", 0, 0, /* added */
      { 8336,-2853,-699,-4425,11989,2760,-954,1625,6396 } },
    { "Leica X1", 0, 0, /* added */
      { 9055,-2611,-666,-4906,12652,2519,-555,1384,7417 } },
    { "Leica X", 0, 0, /* X(113), X-U(113), XV, X Vario(107) */ /* updated */
      { 9062,-3198,-828,-4065,11772,2603,-761,1468,6458 } },

    { "Mamiya M31", 0, 0, /* added */
      { 4516,-244,-36,-7020,14976,2174,-3206,4670,7087 } },
    { "Mamiya M22", 0, 0, /* added */
      { 2905,732,-237,-8135,16626,1476,-3038,4253,7517 } },
    { "Mamiya M18", 0, 0, /* added */
      { 6516,-2050,-507,-8217,16703,1479,-3492,4741,8489 } },
    { "Mamiya ZD", 0, 0,
      { 7645,2579,-1363,-8689,16717,2015,-3712,5941,5961 } },

    { "Micron 2010", 110, 0, /* DJC */
      { 16695,-3761,-2151,155,9682,163,3433,951,4904 } },

    { "Minolta DiMAGE 5", 0, 0xf7d, /* updated */
      { 9117,-3063,-973,-7949,15763,2306,-2752,3136,8093 } },
    { "Minolta DiMAGE 7Hi", 0, 0xf7d, /* updated */
      { 11555,-4064,-1256,-7903,15633,2409,-2811,3320,7358 } },
    { "Minolta DiMAGE 7i", 0, 0xf7d, /* added */
      { 11050,-3791,-1199,-7875,15585,2434,-2797,3359,7560 } },
    { "Minolta DiMAGE 7", 0, 0xf7d, /* updated */
      { 9258,-2879,-1008,-8076,15847,2351,-2806,3280,7821 } },
    { "Minolta DiMAGE A1", 0, 0xf8b, /* updated */
      { 9274,-2548,-1167,-8220,16324,1943,-2273,2721,8340 } },
    { "Minolta DiMAGE A200", 0, 0,
      { 8560,-2487,-986,-8112,15535,2771,-1209,1324,7743 } },
    { "Minolta DiMAGE A2", 0, 0xf8f,
      { 9097,-2726,-1053,-8073,15506,2762,-966,981,7763 } },
    { "Minolta DiMAGE Z2", 0, 0, /* DJC */
      { 11280,-3564,-1370,-4655,12374,2282,-1423,2168,5396 } },
    { "Minolta DYNAX 5", 0, 0xffb,
      { 10284,-3283,-1086,-7957,15762,2316,-829,882,6644 } },
    { "Minolta Maxxum 5D", 0, 0xffb, /* added */
      { 10284,-3283,-1086,-7957,15762,2316,-829,882,6644 } },
    { "Minolta ALPHA-5 DIGITAL", 0, 0xffb, /* added */
      { 10284,-3283,-1086,-7957,15762,2316,-829,882,6644 } },
    { "Minolta ALPHA SWEET DIGITAL", 0, 0xffb, /* added */
      { 10284,-3283,-1086,-7957,15762,2316,-829,882,6644 } },
    { "Minolta DYNAX 7", 0, 0xffb,
      { 10239,-3104,-1099,-8037,15727,2451,-927,925,6871 } },
    { "Minolta Maxxum 7D", 0, 0xffb, /* added */
      { 10239,-3104,-1099,-8037,15727,2451,-927,925,6871 } },
    { "Minolta ALPHA-7 DIGITAL", 0, 0xffb, /* added */
      { 10239,-3104,-1099,-8037,15727,2451,-927,925,6871 } },

    { "Motorola PIXL", 0, 0, /* DJC */
      { 8898,-989,-1033,-3292,11619,1674,-661,3178,5216 } },

    { "Nikon D100", 0, 0,
      { 5902,-933,-782,-8983,16719,2354,-1402,1455,6464 } },
    { "Nikon D1H", 0, 0, /* updated */
      { 7659,-2238,-935,-8942,16969,2004,-2701,3051,8690 } },
    { "Nikon D1X", 0, 0,
      { 7702,-2245,-975,-9114,17242,1875,-2679,3055,8521 } },
    { "Nikon D1", 0, 0, /* multiplied by 2.218750, 1.0, 1.148438 */
      { 16772,-4726,-2141,-7611,15713,1972,-2846,3494,9521 } },
    { "Nikon D200", 0, 0xfbc,
      { 8367,-2248,-763,-8758,16447,2422,-1527,1550,8053 } },
    { "Nikon D2H", 0, 0,
      { 5733,-911,-629,-7967,15987,2055,-3050,4013,7048 } },
    { "Nikon D2X", 0, 0, /* updated */
      { 10231,-2768,-1254,-8302,15900,2551,-797,681,7148 } },
    { "Nikon D3000", 0, 0,
      { 8736,-2458,-935,-9075,16894,2251,-1354,1242,8263 } },
    { "Nikon D3100", 0, 0,
      { 7911,-2167,-813,-5327,13150,2408,-1288,2483,7968 } },
    { "Nikon D3200", 0, 0xfb9,
      { 7013,-1408,-635,-5268,12902,2640,-1470,2801,7379 } },
    { "Nikon D3300", 0, 0,
      { 6988,-1384,-714,-5631,13410,2447,-1485,2204,7318 } },
    { "Nikon D3400", 0, 0,
      { 6988,-1384,-714,-5631,13410,2447,-1485,2204,7318 } },
    { "Nikon D3500", 0, 0,
      { 8821,-2938,-785,-4178,12142,2287,-824,1651,6860 } },
    { "Nikon D300", 0, 0,
      { 9030,-1992,-715,-8465,16302,2255,-2689,3217,8069 } },
    { "Nikon D3X", 0, 0,
      { 7171,-1986,-648,-8085,15555,2718,-2170,2512,7457 } },
    { "Nikon D3S", 0, 0,
      { 8828,-2406,-694,-4874,12603,2541,-660,1509,7587 } },
    { "Nikon D3", 0, 0,
      { 8139,-2171,-663,-8747,16541,2295,-1925,2008,8093 } },
    { "Nikon D40X", 0, 0,
      { 8819,-2543,-911,-9025,16928,2151,-1329,1213,8449 } },
    { "Nikon D40", 0, 0,
      { 6992,-1668,-806,-8138,15748,2543,-874,850,7897 } },
    { "Nikon D4S", 0, 0,
      { 8598,-2848,-857,-5618,13606,2195,-1002,1773,7137 } },
    { "Nikon D4", 0, 0,
      { 8598,-2848,-857,-5618,13606,2195,-1002,1773,7137 } },
    { "Nikon Df", 0, 0,
      { 8598,-2848,-857,-5618,13606,2195,-1002,1773,7137 } },
    { "Nikon D5000", 0, 0xf00,
      { 7309,-1403,-519,-8474,16008,2622,-2433,2826,8064 } },
    { "Nikon D5100", 0, 0x3de6,
      { 8198,-2239,-724,-4871,12389,2798,-1043,2050,7181 } },
    { "Nikon D5200", 0, 0,
      { 8322,-3112,-1047,-6367,14342,2179,-988,1638,6394 } },
    { "Nikon D5300", 0, 0,
      { 6988,-1384,-714,-5631,13410,2447,-1485,2204,7318 } },
    { "Nikon D5500", 0, 0,
      { 8821,-2938,-785,-4178,12142,2287,-824,1651,6860 } },
    { "Nikon D5600", 0, 0,
      { 8821,-2938,-785,-4178,12142,2287,-824,1651,6860 } },
    { "Nikon D500", 0, 0,
        { 8813,-3210,-1036,-4703,12868,2021,-1054,1940,6129 } },
    { "Nikon D50", 0, 0,
      { 7732,-2422,-789,-8238,15884,2498,-859,783,7330 } },
    { "Nikon D5", 0, 0,
      { 9200,-3522,-992,-5755,13803,2117,-753,1486,6338 } },
    { "Nikon D600", 0, 0x3e07,
      { 8178,-2245,-609,-4857,12394,2776,-1207,2086,7298 } },
    { "Nikon D610",0, 0, /* updated */
      { 8178,-2245,-609,-4857,12394,2776,-1207,2086,7298 } },
    { "Nikon D60", 0, 0,
      { 8736,-2458,-935,-9075,16894,2251,-1354,1242,8263 } },
    { "Nikon D7000", 0, 0,
      { 8198,-2239,-724,-4871,12389,2798,-1043,2050,7181 } },
    { "Nikon D7100", 0, 0,
      { 8322,-3112,-1047,-6367,14342,2179,-988,1638,6394 } },
    { "Nikon D7200", 0, 0,
      { 8322,-3112,-1047,-6367,14342,2179,-988,1638,6394 } },
    { "Nikon D7500", 0, 0,
      { 8813,-3210,-1036,-4703,12868,2021,-1054,1940,6129 } },
    { "Nikon D750", -600, 0,
      { 9020,-2890,-715,-4535,12436,2348,-934,1919,7086 } },
    { "Nikon D700", 0, 0,
      { 8139,-2171,-663,-8747,16541,2295,-1925,2008,8093 } },
    { "Nikon D70", 0, 0,
      { 7732,-2422,-789,-8238,15884,2498,-859,783,7330 } },
    { "Nikon D850", 0, 0,
      { 10405,-3755,-1270,-5461,13787,1793,-1040,2015,6785 } },
    { "Nikon D810A", 0, 0,
      { 11973,-5685,-888,-1965,10326,1901,-115,1123,7169 } },
    { "Nikon D810", 0, 0,
      { 9369,-3195,-791,-4488,12430,2301,-893,1796,6872 } },
    { "Nikon D800", 0, 0,
      { 7866,-2108,-555,-4869,12483,2681,-1176,2069,7501 } },
    { "Nikon D80", 0, 0,
      { 8629,-2410,-883,-9055,16940,2171,-1490,1363,8520 } },
    { "Nikon D90", 0, 0xf00,
      { 7309,-1403,-519,-8474,16008,2622,-2434,2826,8064 } },
    { "Nikon E700", 0, 0x3dd, /* DJC */
      { -3746,10611,1665,9621,-1734,2114,-2389,7082,3064,3406,6116,-244 } },
    { "Nikon E800", 0, 0x3dd, /* DJC */
      { -3746,10611,1665,9621,-1734,2114,-2389,7082,3064,3406,6116,-244 } },
    { "Nikon E950", 0, 0x3dd, /* DJC */
      { -3746,10611,1665,9621,-1734,2114,-2389,7082,3064,3406,6116,-244 } },
    { "Nikon E995", 0, 0, /* copied from E5000 */
      { -5547,11762,2189,5814,-558,3342,-4924,9840,5949,688,9083,96 } },
    { "Nikon E2100", 0, 0, /* copied from Z2, new white balance */
      { 13142,-4152,-1596,-4655,12374,2282,-1769,2696,6711 } },
    { "Nikon E2500", 0, 0,
      { -5547,11762,2189,5814,-558,3342,-4924,9840,5949,688,9083,96 } },
    { "Nikon E3200", 0, 0, /* DJC */
      { 9846,-2085,-1019,-3278,11109,2170,-774,2134,5745 } },
    { "Nikon E4300", 0, 0, /* copied from Minolta DiMAGE Z2 */
      { 11280,-3564,-1370,-4655,12374,2282,-1423,2168,5396 } },
    { "Nikon E4500", 0, 0,
      { -5547,11762,2189,5814,-558,3342,-4924,9840,5949,688,9083,96 } },
    { "Nikon E5000", 0, 0, /* updated */
      { -6678,12805,2248,5725,-499,3375,-5903,10713,6034,-270,9976,134 } },
    { "Nikon E5400", 0, 0, /* updated */
      { 9349,-2988,-1001,-7918,15766,2266,-2097,2680,6839 } },
    { "Nikon E5700", 0, 0, /* updated */
      { -6475,12496,2428,5409,-16,3180,-5965,10912,5866,-177,9918,248 } },
    { "Nikon E8400", 0, 0,
      { 7842,-2320,-992,-8154,15718,2599,-1098,1342,7560 } },
    { "Nikon E8700", 0, 0,
      { 8489,-2583,-1036,-8051,15583,2643,-1307,1407,7354 } },
    { "Nikon E8800", 0, 0,
      { 7971,-2314,-913,-8451,15762,2894,-1442,1520,7610 } },
    { "Nikon COOLPIX A1000", 0, 0,
      { 10601,-3487,-1127,-2931,11443,1676,-587,1740,5278 } },
    { "Nikon COOLPIX A", 0, 0,
      { 8198,-2239,-724,-4871,12389,2798,-1043,2050,7181 } },
    { "Nikon COOLPIX B700", 0, 0,
      { 14387,-6014,-1299,-1357,9975,1616,467,1047,4744 } },
    { "Nikon COOLPIX P330", -200, 0,
      { 10321,-3920,-931,-2750,11146,1824,-442,1545,5539 } },
    { "Nikon COOLPIX P340", -200, 0,
      { 10321,-3920,-931,-2750,11146,1824,-442,1545,5539 } },
    { "Nikon COOLPIX Kalon", 0, 0, /* added */
      { 10321,-3920,-931,-2750,11146,1824,-442,1545,5539 } },
    { "Nikon COOLPIX Deneb", 0, 0, /* added */
      { 10321,-3920,-931,-2750,11146,1824,-442,1545,5539 } },
    { "Nikon COOLPIX P1000", 0, 0,
      { 14294,-6116,-1333,-1628,10219,1637,-14,1158,5022 } },
    { "Nikon COOLPIX P6000", 0, 0,
      { 9698,-3367,-914,-4706,12584,2368,-837,968,5801 } },
    { "Nikon COOLPIX P7000", 0, 0,
      { 11432,-3679,-1111,-3169,11239,2202,-791,1380,4455 } },
    { "Nikon COOLPIX P7100", 0, 0,
      { 11053,-4269,-1024,-1976,10182,2088,-526,1263,4469 } },
    { "Nikon COOLPIX P7700", -3200, 0,
      { 10321,-3920,-931,-2750,11146,1824,-442,1545,5539 } },
    { "Nikon COOLPIX P7800", -3200, 0,
      { 10321,-3920,-931,-2750,11146,1824,-442,1545,5539 } },
    { "Nikon 1 V3", -200, 0,
      { 5958,-1559,-571,-4021,11453,2939,-634,1548,5087 } },
    { "Nikon 1 J4", 0, 0,
      { 5958,-1559,-571,-4021,11453,2939,-634,1548,5087 } },
    { "Nikon 1 J5", 0, 0,
      { 7520,-2518,-645,-3844,12102,1945,-913,2249,6835 } },
    { "Nikon 1 S2", -200, 0,
      { 6612,-1342,-618,-3338,11055,2623,-174,1792,5075 } },
    { "Nikon 1 V2", 0, 0,
      { 6588,-1305,-693,-3277,10987,2634,-355,2016,5106 } },
    { "Nikon 1 J3", 0, 0, /* updated */
      { 6588,-1305,-693,-3277,10987,2634,-355,2016,5106 } },
    { "Nikon 1 AW1", 0, 0,
      { 6588,-1305,-693,-3277,10987,2634,-355,2016,5106 } },
    { "Nikon 1 ", 0, 0, /* J1, J2, S1, V1 */
      { 8994,-2667,-865,-4594,12324,2552,-699,1786,6260 } },

    { "Nikon Z 6", 0, 0,
      { 9943,-3269,-839,-5323,13269,2259,-1198,2083,7557 } },
    { "Nikon Z 7", 0, 0,
      { 10405,-3755,-1270,-5461,13787,1793,-1040,2015,6785 } },

    { "Olympus AIR-A01", 0, 0xfe1,
      { 8992,-3093,-639,-2563,10721,2122,-437,1270,5473 } },
    { "Olympus C5050", 0, 0, /* updated */
      { 10633,-3234,-1285,-7460,15570,1967,-1917,2510,6299 } },
    { "Olympus C5060", 0, 0,
      { 10445,-3362,-1307,-7662,15690,2058,-1135,1176,7602 } },
    { "Olympus C7070", 0, 0,
      { 10252,-3531,-1095,-7114,14850,2436,-1451,1723,6365 } },
    { "Olympus C70", 0, 0,
      { 10793,-3791,-1146,-7498,15177,2488,-1390,1577,7321 } },
    { "Olympus C80", 0, 0,
      { 8606,-2509,-1014,-8238,15714,2703,-942,979,7760 } },
    { "Olympus E-10", 0, 0xffc, /* updated */
      { 12970,-4703,-1433,-7466,15843,1644,-2191,2451,6668 } },
    { "Olympus E-1", 0, 0,
      { 11846,-4767,-945,-7027,15878,1089,-2699,4122,8311 } },
    { "Olympus E-20", 0, 0xffc, /* updated */
      { 13414,-4950,-1517,-7166,15293,1960,-2325,2664,7212 } },
    { "Olympus E-300", 0, 0,
      { 7828,-1761,-348,-5788,14071,1830,-2853,4518,6557 } },
    { "Olympus E-330", 0, 0,
      { 8961,-2473,-1084,-7979,15990,2067,-2319,3035,8249 } },
    { "Olympus E-30", 0, 0xfbc,
      { 8144,-1861,-1111,-7763,15894,1929,-1865,2542,7607 } },
    { "Olympus E-3", 0, 0xf99,
      { 9487,-2875,-1115,-7533,15606,2010,-1618,2100,7389 } },
    { "Olympus E-400", 0, 0,
      { 6169,-1483,-21,-7107,14761,2536,-2904,3580,8568 } },
    { "Olympus E-410", 0, 0xf6a,
      { 8856,-2582,-1026,-7761,15766,2082,-2009,2575,7469 } },
    { "Olympus E-420", 0, 0xfd7,
      { 8746,-2425,-1095,-7594,15612,2073,-1780,2309,7416 } },
    { "Olympus E-450", 0, 0xfd2,
      { 8745,-2425,-1095,-7594,15613,2073,-1780,2309,7416 } },
    { "Olympus E-500", 0, 0,
      { 8136,-1968,-299,-5481,13742,1871,-2556,4205,6630 } },
    { "Olympus E-510", 0, 0xf6a,
      { 8785,-2529,-1033,-7639,15624,2112,-1783,2300,7817 } },
    { "Olympus E-520", 0, 0xfd2,
      { 8344,-2322,-1020,-7596,15635,2048,-1748,2269,7287 } },
    { "Olympus E-5", 0, 0xeec,
      { 11200,-3783,-1325,-4576,12593,2206,-695,1742,7504 } },
    { "Olympus E-600", 0, 0xfaf,
      { 8453,-2198,-1092,-7609,15681,2008,-1725,2337,7824 } },
    { "Olympus E-620", 0, 0xfaf,
      { 8453,-2198,-1092,-7609,15681,2008,-1725,2337,7824 } },
    { "Olympus E-P1", 0, 0xffd,
      { 8343,-2050,-1021,-7715,15705,2103,-1831,2380,8235 } },
    { "Olympus E-P2", 0, 0xffd,
      { 8343,-2050,-1021,-7715,15705,2103,-1831,2380,8235 } },
    { "Olympus E-P3", 0, 0,
      { 7575,-2159,-571,-3722,11341,2725,-1434,2819,6271 } },
    { "Olympus E-P5", 0, 0,
      { 8380,-2630,-639,-2887,10725,2496,-627,1427,5438 } },
    { "Olympus E-PL1s", 0, 0,
      { 11409,-3872,-1393,-4572,12757,2003,-709,1810,7415 } },
    { "Olympus E-PL1", 0, 0,
      { 11408,-4289,-1215,-4286,12385,2118,-387,1467,7787 } },
    { "Olympus E-PL2", 0, 0xcf3,
      { 15030,-5552,-1806,-3987,12387,1767,-592,1670,7023 } },
    { "Olympus E-PL3", 0, 0,
      { 7575,-2159,-571,-3722,11341,2725,-1434,2819,6271 } },
    { "Olympus E-PL5", 0, 0xfcb,
      { 8380,-2630,-639,-2887,10725,2496,-627,1427,5438 } },
    { "Olympus E-PL6", 0, 0,
      { 8380,-2630,-639,-2887,10725,2496,-627,1427,5438 } },
    { "Olympus E-PL7", 0, 0,
      { 9197,-3190,-659,-2606,10830,2039,-458,1250,5458 } },
    { "Olympus E-PL8", 0, 0,
      { 9197,-3190,-659,-2606,10830,2039,-458,1250,5458 } },
    { "Olympus E-PL9", 0, 0,
      { 8380,-2630,-639,-2887,10725,2496,-627,1427,5438 } },
    { "Olympus E-PM1", 0, 0,
      { 7575,-2159,-571,-3722,11341,2725,-1434,2819,6271 } },
    { "Olympus E-PM2", 0, 0,
      { 8380,-2630,-639,-2887,10725,2496,-627,1427,5438 } },
    { "Olympus E-M10", 0, 0, /* Same for E-M10MarkII, E-M10MarkIII */
      { 8380,-2630,-639,-2887,10725,2496,-627,1427,5438 } },
    { "CLAUSS piX 5oo",  0,  0, /* Oly ID 0x5330303539 (alphanum S0059), Olympus E-M10MarkII */
      { 8380,-2630,-639,-2887,10725,2496,-627,1427,5438 } },
    { "Olympus E-M1X", 0, 0,
      { 11896,-5110,-1076,-3181,11378,2048,-519,1224,5166 } },
    { "Olympus E-M1MarkII", 0, 0,
      { 9383,-3170,-763,-2457,10702,2020,-384,1236,5552 } },
    { "Olympus E-M1", 0, 0,
      { 7687,-1984,-606,-4327,11928,2721,-1381,2339,6452 } },
    { "Olympus E-M5MarkII", 0, 0,
      { 9422,-3258,-711,-2655,10898,2015,-512,1354,5512 } },
    { "Olympus E-M5", 0, 0xfe1,
      { 8380,-2630,-639,-2887,10725,2496,-627,1427,5438 } },
    { "Olympus PEN-F",0, 0,
      { 9476,-3182,-765,-2613,10958,1893,-449,1315,5268 } },
    { "Olympus SP350", 0, 0,
      { 12078,-4836,-1069,-6671,14306,2578,-786,939,7418 } },
    { "Olympus SP3", 0, 0,
      { 11766,-4445,-1067,-6901,14421,2707,-1029,1217,7572 } },
    { "Olympus SP500UZ", 0, 0xfff,
      { 9493,-3415,-666,-5211,12334,3260,-1548,2262,6482 } },
    { "Olympus SP510UZ", 0, 0xffe,
      { 10593,-3607,-1010,-5881,13127,3084,-1200,1805,6721 } },
    { "Olympus SP550UZ", 0, 0xffe,
      { 11597,-4006,-1049,-5432,12799,2957,-1029,1750,6516 } },
    { "Olympus SP560UZ", 0, 0xff9,
      { 10915,-3677,-982,-5587,12986,2911,-1168,1968,6223 } },
    { "Olympus SP565UZ", 0, 0, /* added */
      { 11856,-4469,-1159,-4814,12368,2756,-993,1779,5589 } },
    { "Olympus SP570UZ", 0, 0,
      { 11522,-4044,-1146,-4736,12172,2904,-988,1829,6039 } },
    { "Olympus SH-2", 0, 0,
     { 10156,-3425,-1077,-2611,11177,1624,-385,1592,5080 } },
    { "Olympus SH-3", 0, 0, /* Alias of SH-2 */
     { 10156,-3425,-1077,-2611,11177,1624,-385,1592,5080 } },
    { "Olympus STYLUS1",0, 0, /* updated */
      { 8360,-2420,-880,-3928,12353,1739,-1381,2416,5173 } },
    { "Olympus TG-4", 0, 0,
     { 11426,-4159,-1126,-2066,10678,1593,-120,1327,4998 } },
    { "Olympus TG-5", 0, 0,
     { 10899,-3833,-1082,-2112,10736,1575,-267,1452,5269 } },
    { "Olympus XZ-10", 0, 0,
      { 9777,-3483,-925,-2886,11297,1800,-602,1663,5134 } },
    { "Olympus XZ-1", 0, 0,
      { 10901,-4095,-1074,-1141,9208,2293,-62,1417,5158 } },
    { "Olympus XZ-2", 0, 0,
      { 9777,-3483,-925,-2886,11297,1800,-602,1663,5134 } },

    { "OmniVision", 16, 0x3ff,
      { 12782,-4059,-379,-478,9066,1413,1340,1513,5176 } }, /* DJC */

    { "Pentax *ist DL2", 0, 0,
      { 10504,-2438,-1189,-8603,16207,2531,-1022,863,12242 } },
    { "Pentax *ist DL", 0, 0,
      { 10829,-2838,-1115,-8339,15817,2696,-837,680,11939 } },
    { "Pentax *ist DS2", 0, 0,
      { 10504,-2438,-1189,-8603,16207,2531,-1022,863,12242 } },
    { "Pentax *ist DS", 0, 0,
      { 10371,-2333,-1206,-8688,16231,2602,-1230,1116,11282 } },
    { "Pentax *ist D", 0, 0,
      { 9651,-2059,-1189,-8881,16512,2487,-1460,1345,10687 } },
    { "Pentax GR", 0, 0, /* added */
      { 5329,-1459,-390,-5407,12930,2768,-1119,1772,6046 } },
    { "Pentax K-01", 0, 0, /* added */
      { 8134,-2728,-645,-4365,11987,2694,-838,1509,6498 } },
    { "Pentax K10D", 0, 0, /* updated */
      { 9679,-2965,-811,-8622,16514,2182,-975,883,9793 } },
    { "Pentax K1", 0, 0,
      { 11095,-3157,-1324,-8377,15834,2720,-1108,947,11688 } },
    { "Pentax K20D", 0, 0,
      { 9427,-2714,-868,-7493,16092,1373,-2199,3264,7180 } },
    { "Pentax K200D", 0, 0,
      { 9186,-2678,-907,-8693,16517,2260,-1129,1094,8524 } },
    { "Pentax K2000", 0, 0, /* updated */
      { 9730,-2989,-970,-8527,16258,2381,-1060,970,8362 } },
    { "Pentax K-m", 0, 0, /* updated */
      { 9730,-2989,-970,-8527,16258,2381,-1060,970,8362 } },
    { "Pentax KP", 0, 0,
      { 7825,-2160,-1403,-4841,13555,1349,-1559,2449,5814 } },
    { "Pentax K-x", 0, 0,
      { 8843,-2837,-625,-5025,12644,2668,-411,1234,7410 } },
    { "Pentax K-r", 0, 0,
      { 9895,-3077,-850,-5304,13035,2521,-883,1768,6936 } },
    { "Pentax K-1 Mark II", 0, 0,
      { 8596,-2981,-639,-4202,12046,2431,-685,1424,6122 } },
    { "Pentax K-1", 0, 0, /* updated */
      { 8596,-2981,-639,-4202,12046,2431,-685,1424,6122 } },
    { "Pentax K-30", 0, 0, /* updated */
      { 8134,-2728,-645,-4365,11987,2694,-838,1509,6498 } },
    { "Pentax K-3 II", 0, 0, /* updated */
      { 7415,-2052,-721,-5186,12788,2682,-1446,2157,6773 } },
    { "Pentax K-3", 0, 0,
      { 7415,-2052,-721,-5186,12788,2682,-1446,2157,6773 } },
    { "Pentax K-5 II", 0, 0,
      { 8170,-2725,-639,-4440,12017,2744,-771,1465,6599 } },
    { "Pentax K-500", 0, 0, /* added */
      { 8109,-2740,-608,-4593,12175,2731,-1006,1515,6545 } },
    { "Pentax K-50", 0, 0, /* added */
      { 8109,-2740,-608,-4593,12175,2731,-1006,1515,6545 } },
    { "Pentax K-5", 0, 0,
      { 8713,-2833,-743,-4342,11900,2772,-722,1543,6247 } },
    { "Pentax K-70", 0, 0,
      { 8766,-3149,-747,-3976,11943,2292,-517,1259,5552 } },
    { "Pentax K-7", 0, 0,
      { 9142,-2947,-678,-8648,16967,1663,-2224,2898,8615 } },
    { "Pentax KP", 0, 0,
      { 8617,-3228,-1034,-4674,12821,2044,-803,1577,5728 } },
    { "Pentax K-S1", 0, 0,
      { 8512,-3211,-787,-4167,11966,2487,-638,1288,6054 } },
    { "Pentax K-S2", 0, 0,
      { 8662,-3280,-798,-3928,11771,2444,-586,1232,6054 } },
    { "Pentax Q-S1", 0, 0,
      { 12995,-5593,-1107,-1879,10139,2027,-64,1233,4919 } },
    { "Pentax Q7", 0, 0, /* added */
      { 10901,-3938,-1025,-2743,11210,1738,-823,1805,5344 } },
    { "Pentax Q10", 0, 0, /* updated */
      { 11562,-4183,-1172,-2357,10919,1641,-582,1726,5112 } },
    { "Pentax Q", 0, 0, /* added */
      { 11731,-4169,-1267,-2015,10727,1473,-217,1492,4870 } },
    { "Pentax MX-1", 0, 0, /* updated */
      { 9296,-3146,-888,-2860,11287,1783,-618,1698,5151 } },
    { "Pentax 645D", 0, 0x3e00,
      { 10646,-3593,-1158,-3329,11699,1831,-667,2874,6287 } },
    { "Pentax 645Z", 0, 0, /* updated */
      { 9519,-3591,-664,-4074,11725,2671,-624,1501,6653 } },

    { "Panasonic DMC-CM10", -15, 0,
      { 8770,-3194,-820,-2871,11281,1803,-513,1552,4434 } },
    { "Panasonic DMC-CM1", -15, 0,
      { 8770,-3194,-820,-2871,11281,1803,-513,1552,4434 } },
    { "Panasonic DC-FZ80", -15, 0,
      { 8550,-2908,-842,-3195,11529,1881,-338,1603,4631 } },
    { "Panasonic DMC-FZ8", 0, 0xf7f,
      { 8986,-2755,-802,-6341,13575,3077,-1476,2144,6379 } },
    { "Panasonic DMC-FZ18", 0, 0,
      { 9932,-3060,-935,-5809,13331,2753,-1267,2155,5575 } },
    { "Panasonic DMC-FZ28", -15, 0xf96,
      { 10109,-3488,-993,-5412,12812,2916,-1305,2140,5543 } },
    { "Panasonic DMC-FZ300", -15, 0xfff,
      { 8378,-2798,-769,-3068,11410,1877,-538,1792,4623 } },
    { "Panasonic DMC-FZ330", -15, 0xfff,
      { 8378,-2798,-769,-3068,11410,1877,-538,1792,4623 } },
    { "Panasonic DMC-FZ30", 0, 0xf94,
      { 10976,-4029,-1141,-7918,15491,2600,-1670,2071,8246 } },
    { "Panasonic DMC-FZ3", -15, 0,
      { 9938,-2780,-890,-4604,12393,2480,-1117,2304,4620 } },
    { "Panasonic DMC-FZ40", -15, 0,
      { 13639,-5535,-1371,-1698,9633,2430,316,1152,4108 } },
    { "Panasonic DMC-FZ50", 0, 0,
      { 7906,-2709,-594,-6231,13351,3220,-1922,2631,6537 } },
    { "Panasonic DMC-FZ7", -15, 0,
      { 11532,-4324,-1066,-2375,10847,1749,-564,1699,4351 } },
    { "Panasonic DMC-L10", -15, 0xf96,
      { 8025,-1942,-1050,-7920,15904,2100,-2456,3005,7039 } },
    { "Panasonic DMC-L1", 0, 0xf7f,
      { 8054,-1885,-1025,-8349,16367,2040,-2805,3542,7629 } },
    { "Panasonic DMC-LC1", 0, 0,
      { 11340,-4069,-1275,-7555,15266,2448,-2960,3426,7685 } },
    { "Panasonic DC-LX100M2", -15, 0,
      { 8585,-3127,-833,-4005,12250,1953,-650,1494,4862 } },
    { "Panasonic DMC-LX100", -15, 0,
      { 8844,-3538,-768,-3709,11762,2200,-698,1792,5220 } },
    { "Panasonic DMC-LF1", -15, 0,
      { 9379,-3267,-816,-3227,11560,1881,-926,1928,5340 } },
    { "Leica C (Typ 112)", -15, 0,
      { 9379,-3267,-816,-3227,11560,1881,-926,1928,5340 } },
    { "Panasonic DMC-LX9", -15, 0,
      { 7790,-2736,-755,-3452,11870,1769,-628,1647,4898 } },
    { "Panasonic DMC-LX1", 0, 0xf7f,
      { 10704,-4187,-1230,-8314,15952,2501,-920,945,8927 } },
    { "Panasonic DMC-LX2", 0, 0,
      { 8048,-2810,-623,-6450,13519,3272,-1700,2146,7049 } },
    { "Panasonic DMC-LX3", -15, 0,
      { 8128,-2668,-655,-6134,13307,3161,-1782,2568,6083 } },
    { "Panasonic DMC-LX5", -15, 0,
      { 10909,-4295,-948,-1333,9306,2399,22,1738,4582 } },
    { "Panasonic DMC-LX7", -15, 0,
      { 10148,-3743,-991,-2837,11366,1659,-701,1893,4899 } },
    { "Panasonic DMC-FZ1000", -15, 0,
      { 7830,-2696,-763,-3325,11667,1866,-641,1712,4824 } },
    { "Panasonic DMC-FZ100", -15, 0xfff,
      { 16197,-6146,-1761,-2393,10765,1869,366,2238,5248 } },
    { "Panasonic DMC-FZ150", -15, 0xfff,
      { 11904,-4541,-1189,-2355,10899,1662,-296,1586,4289 } },
    { "Panasonic DMC-FZ200", -15, 0xfff,
      { 8112,-2563,-740,-3730,11784,2197,-941,2075,4933 } },
    { "Panasonic DMC-FZ2500", -15, 0,
      { 7386,-2443,-743,-3437,11864,1757,-608,1660,4766 } },
    { "Panasonic DMC-FX150", -15, 0xfff,
      { 9082,-2907,-925,-6119,13377,3058,-1797,2641,5609 } },
    { "Panasonic DMC-FX180", -15, 0xfff, /* added */
      { 9082,-2907,-925,-6119,13377,3058,-1797,2641,5609 } },
    { "Panasonic DMC-G10", 0, 0,
      { 10113,-3400,-1114,-4765,12683,2317,-377,1437,6710 } },
    { "Panasonic DMC-G1", -15, 0xf94,
      { 8199,-2065,-1056,-8124,16156,2033,-2458,3022,7220 } },
    { "Panasonic DMC-G2", -15, 0xf3c,
      { 10113,-3400,-1114,-4765,12683,2317,-377,1437,6710 } },
    { "Panasonic DMC-G3", -15, 0xfff,
      { 6763,-1919,-863,-3868,11515,2684,-1216,2387,5879 } },
    { "Panasonic DMC-G5", -15, 0xfff,
      { 7798,-2562,-740,-3879,11584,2613,-1055,2248,5434 } },
    { "Panasonic DMC-G6", -15, 0xfff,
      { 8294,-2891,-651,-3869,11590,2595,-1183,2267,5352 } },
    { "Panasonic DMC-G7", -15, 0xfff,
      { 7610,-2780,-576,-4614,12195,2733,-1375,2393,6490 } },
    { "Panasonic DMC-G8", -15, 0xfff,
      { 7610,-2780,-576,-4614,12195,2733,-1375,2393,6490 } },
    { "Panasonic DC-G9", -15, 0,
      { 7685,-2375,-634,-3687,11700,2249,-748,1546,5111 } },

    { "Panasonic DMC-GH1", -15, 0xf92,
      { 6299,-1466,-532,-6535,13852,2969,-2331,3112,5984 } },
    { "Panasonic DMC-GH2", -15, 0xf95,
      { 7780,-2410,-806,-3913,11724,2484,-1018,2390,5298 } },
    { "Panasonic DMC-GH3", -15, 0,
      { 6559,-1752,-491,-3672,11407,2586,-962,1875,5130 } },
    { "Panasonic DMC-GH4", -15, 0,
      { 7122,-2108,-512,-3155,11201,2231,-541,1423,5045 } },
    { "Panasonic AG-GH4", -15, 0, /* added */
      { 7122,-2108,-512,-3155,11201,2231,-541,1423,5045 } },
    {"Panasonic DC-GH5s", -15, 0,
      { 6929,-2355,-708,-4192,12534,1828,-1097,1989,5195 } },
    { "Panasonic DC-GH5", -15, 0,
      { 7641,-2336,-605,-3218,11299,2187,-485,1338,5121 } },
    { "Yuneec CGO4", -15, 0,
      { 7122,-2108,-512,-3155,11201,2231,-541,1423,5045 } },

    { "Panasonic DMC-GM1", -15, 0,
      { 6770,-1895,-744,-5232,13145,2303,-1664,2691,5703 } },
    { "Panasonic DMC-GM5", -15, 0,
      { 8238,-3244,-679,-3921,11814,2384,-836,2022,5852 } },

    { "Panasonic DMC-GF1", -15, 0xf92,
      { 7888,-1902,-1011,-8106,16085,2099,-2353,2866,7330 } },
    { "Panasonic DMC-GF2", -15, 0xfff,
      { 7888,-1902,-1011,-8106,16085,2099,-2353,2866,7330 } },
    { "Panasonic DMC-GF3", -15, 0xfff,
      { 9051,-2468,-1204,-5212,13276,2121,-1197,2510,6890 } },
    { "Panasonic DMC-GF5", -15, 0xfff,
      { 8228,-2945,-660,-3938,11792,2430,-1094,2278,5793 } },
    { "Panasonic DMC-GF6", -15, 0,
      { 8130,-2801,-946,-3520,11289,2552,-1314,2511,5791 } },
    { "Panasonic DMC-GF7", -15, 0,
      { 7610,-2780,-576,-4614,12195,2733,-1375,2393,6490 } },
    { "Panasonic DMC-GF8", -15, 0,
      { 7610,-2780,-576,-4614,12195,2733,-1375,2393,6490 } },
    { "Panasonic DC-GF9", -15, 0,
      { 7610,-2780,-576,-4614,12195,2733,-1375,2393,6490 } },
    { "Panasonic DC-GF10", -15, 0,
      { 7610,-2780,-576,-4614,12195,2733,-1375,2393,6490 } },

    { "Panasonic DMC-GX1", -15, 0,
      { 6763,-1919,-863,-3868,11515,2684,-1216,2387,5879 } },
    { "Panasonic DMC-GX7", -15,0,
      { 7610,-2780,-576,-4614,12195,2733,-1375,2393,6490 } },
    { "Panasonic DMC-GX85", -15, 0,
      { 7771,-3020,-629,-4029,11950,2345,-821,1977,6119 } },
    { "Panasonic DMC-GX8", -15,0,
      { 7564,-2263,-606,-3148,11239,2177,-540,1435,4853 } },
    { "Panasonic DC-GX9", -15, 0,
      { 7564,-2263,-606,-3148,11239,2177,-540,1435,4853 } },

    { "Panasonic DMC-ZS40", -15, 0,
      { 8607,-2822,-808,-3755,11930,2049,-820,2060,5224 } },
    { "Panasonic DMC-ZS50", -15, 0,
      { 8802,-3135,-789,-3151,11468,1904,-550,1745,4810 } },
    { "Panasonic DMC-ZS60", -15, 0,
      { 8550,-2908,-842,-3195,11529,1881,-338,1603,4631 } },
    { "Panasonic DC-ZS70", -15, 0,
      { 9052,-3117,-883,-3045,11346,1927,-205,1520,4730 } },
    { "Panasonic DMC-ZS100", -15, 0,
      { 7790,-2736,-755,-3452,11870,1769,-628,1647,4898 } },
    { "Panasonic DC-ZS200", -15, 0,
      { 7790,-2736,-755,-3452,11870,1769,-628,1647,4898 } },
    { "Leica C-Lux", -15, 0,
      { 7790,-2736,-755,-3452,11870,1769,-628,1647,4898 } },

    { "Phase One H 20", 0, 0, /* DJC */
      { 1313,1855,-109,-6715,15908,808,-327,1840,6020 } },
    { "Phase One H20", 0, 0, /* DJC */
      { 1313,1855,-109,-6715,15908,808,-327,1840,6020 } },
    { "Phase One H 25", 0, 0,
      { 2905,732,-237,-8134,16626,1476,-3038,4253,7517 } },
    { "Phase One H25", 0, 0, /* added */
      { 2905,732,-237,-8134,16626,1476,-3038,4253,7517 } },
    { "Phase One IQ280", 0, 0, /* added */
      { 6294,686,-712,-5435,13417,2211,-1006,2435,5042 } },
    { "Phase One IQ260", 0, 0, /* added */
      { 8035,435,-962,-6001,13872,2320,-1159,3065,5434 } },
    { "Phase One IQ250",0, 0,
//    {3984,0,0,0,10000,0,0,0,7666}},
      {10325,845,-604,-4113,13385,481,-1791,4163,6924}}, /* emb */
    { "Phase One IQ180", 0, 0, /* added */
      { 6294,686,-712,-5435,13417,2211,-1006,2435,5042 } },
    { "Phase One IQ160", 0, 0, /* added */
      { 8035,435,-962,-6001,13872,2320,-1159,3065,5434 } },
    { "Phase One IQ150", 0, 0, /* added */
      {10325,845,-604,-4113,13385,481,-1791,4163,6924}}, /* temp */ /* emb */
//      { 3984,0,0,0,10000,0,0,0,7666 } },
    { "Phase One IQ140", 0, 0, /* added */
      { 8035,435,-962,-6001,13872,2320,-1159,3065,5434 } },
    { "Phase One P65", 0, 0,
      { 8035,435,-962,-6001,13872,2320,-1159,3065,5434 } },
    { "Phase One P 65", 0, 0,
      { 8035,435,-962,-6001,13872,2320,-1159,3065,5434 } },
    { "Phase One P45", 0, 0, /* added */
      { 5053,-24,-117,-5685,14077,1703,-2619,4491,5850 } },
    { "Phase One P 45", 0, 0, /* added */
      { 5053,-24,-117,-5685,14077,1703,-2619,4491,5850 } },
    { "Phase One P40", 0, 0,
      { 8035,435,-962,-6001,13872,2320,-1159,3065,5434 } },
    { "Phase One P 40", 0, 0,
      { 8035,435,-962,-6001,13872,2320,-1159,3065,5434 } },
    { "Phase One P30", 0, 0, /* added */
      { 4516,-244,-36,-7020,14976,2174,-3206,4670,7087 } },
    { "Phase One P 30", 0, 0, /* added */
      { 4516,-244,-36,-7020,14976,2174,-3206,4670,7087 } },
    { "Phase One P25", 0, 0, /* added */
      { 2905,732,-237,-8135,16626,1476,-3038,4253,7517 } },
    { "Phase One P 25", 0, 0, /* added */
      { 2905,732,-237,-8135,16626,1476,-3038,4253,7517 } },
    { "Phase One P21", 0, 0, /* added */
      { 6516,-2050,-507,-8217,16703,1479,-3492,4741,8489 } },
    { "Phase One P 21", 0, 0, /* added */
      { 6516,-2050,-507,-8217,16703,1479,-3492,4741,8489 } },
    { "Phase One P20", 0, 0, /* added */
      { 2905,732,-237,-8135,16626,1476,-3038,4253,7517 } },
    { "Phase One P 20", 0, 0, /* added */
      { 2905,732,-237,-8135,16626,1476,-3038,4253,7517 } },
    { "Phase One P 2", 0, 0,
      { 2905,732,-237,-8134,16626,1476,-3038,4253,7517 } },
    { "Phase One P2", 0, 0,
      { 2905,732,-237,-8134,16626,1476,-3038,4253,7517 } },
    { "Phase One IQ3 100MP", 0, 0, /* added */
//    {2423,0,0,0,9901,0,0,0,7989}},
      { 10999,354,-742,-4590,13342,937,-1060,2166,8120} }, /* emb */
    { "Phase One IQ3 80MP", 0, 0, /* added */
      { 6294,686,-712,-5435,13417,2211,-1006,2435,5042 } },
    { "Phase One IQ3 60MP", 0, 0, /* added */
      { 8035,435,-962,-6001,13872,2320,-1159,3065,5434 } },
    { "Phase One IQ3 50MP", 0, 0, /* added */
//      { 3984,0,0,0,10000,0,0,0,7666 } },
      {10058,1079,-587,-4135,12903,944,-916,2726,7480}}, /* emb */

    { "Photron BC2-HD", 0, 0, /* DJC */
      { 14603,-4122,-528,-1810,9794,2017,-297,2763,5936 } },

    { "Polaroid x530", 0, 0,
      { 13458,-2556,-510,-5444,15081,205,0,0,12120 } },

    { "Red One", 704, 0xffff, /* DJC */
      { 21014,-7891,-2613,-3056,12201,856,-2203,5125,8042 } },

    { "Ricoh S10 24-72mm F2.5-4.4 VC", 0, 0, /* added */
      { 10531,-4043,-878,-2038,10270,2052,-107,895,4577 } },
    { "Ricoh GR A12 50mm F2.5 MACRO", 0, 0, /* added */
      { 8849,-2560,-689,-5092,12831,2520,-507,1280,7104 } },
    { "Ricoh GR DIGITAL 3", 0, 0, /* added */
      { 8170,-2496,-655,-5147,13056,2312,-1367,1859,5265 } },
    { "Ricoh GR DIGITAL 4", 0, 0, /* added */
      { 8771,-3151,-837,-3097,11015,2389,-703,1343,4924 } },
    { "Ricoh GR II", 0, 0,
      { 4630,-834,-423,-4977,12805,2417,-638,1467,6115 } },
    { "Ricoh GR", 0, 0,
      { 3708,-543,-160,-5381,12254,3556,-1471,1929,8234 } },
    { "Ricoh GX200", 0, 0, /* added */
      { 8040,-2368,-626,-4659,12543,2363,-1125,1581,5660 } },
    { "Ricoh RICOH GX200", 0, 0, /* added */
      { 8040,-2368,-626,-4659,12543,2363,-1125,1581,5660 } },
    { "Ricoh GXR MOUNT A12", 0, 0, /* added */
      { 7834,-2182,-739,-5453,13409,2241,-952,2005,6620 } },
    { "Ricoh GXR A16", 0, 0, /* added */
      { 7837,-2538,-730,-4370,12184,2461,-868,1648,5830 } },
    { "Ricoh GXR A12", 0, 0, /* added */
      { 10228,-3159,-933,-5304,13158,2371,-943,1873,6685 } },

    { "Samsung EK-GN100", 0, 0, /* added */ /* Galaxy NX */
      { 7557,-2522,-739,-4679,12949,1894,-840,1777,5311 } },
    { "Samsung EK-GN110", 0, 0, /* added */ /* Galaxy NX */
      { 7557,-2522,-739,-4679,12949,1894,-840,1777,5311 } },
    { "Samsung EK-GN120", 0, 0, /* Galaxy NX */
      { 7557,-2522,-739,-4679,12949,1894,-840,1777,5311 } },
    { "Samsung EK-KN120", 0, 0, /* added */ /* Galaxy NX */
      { 7557,-2522,-739,-4679,12949,1894,-840,1777,5311 } },
    { "Samsung EX1", 0, 0x3e00,
      { 8898,-2498,-994,-3144,11328,2066,-760,1381,4576 } },
    { "Samsung EX2F", 0, 0x7ff,
      { 10648,-3897,-1055,-2022,10573,1668,-492,1611,4742 } },
    { "Samsung Galaxy S8", 0, 0, /* added */
      { 9927,-3704,-1024,-3935,12758,1257,-389,1512,4993 } },
    { "Samsung Galaxy S7 Edge", 0, 0, /* added */
      { 9927,-3704,-1024,-3935,12758,1257,-389,1512,4993 } },
    { "Samsung Galaxy S7", 0, 0, /* added */
      { 9927,-3704,-1024,-3935,12758,1257,-389,1512,4993 } },
    { "Samsung Galaxy NX", 0, 0, /* added */
      { 7557,-2522,-739,-4679,12949,1894,-840,1777,5311 } },
    { "Samsung NX U", 0, 0, /* added */
      { 7557,-2522,-739,-4679,12949,1894,-840,1777,5311 } },
    { "Samsung NX mini", 0, 0,
      { 5222,-1196,-550,-6540,14649,2009,-1666,2819,5657 } },
    { "Samsung NX3300", 0, 0,
      { 8060,-2933,-761,-4504,12890,1762,-630,1489,5227 } },
    { "Samsung NX3000", 0, 0,
      { 8060,-2933,-761,-4504,12890,1762,-630,1489,5227 } },
    { "Samsung NX30", 0, 0, /* used for NX30/NX300/NX300M */
      { 7557,-2522,-739,-4679,12949,1894,-840,1777,5311 } },
    { "Samsung NX2000", 0, 0,
      { 7557,-2522,-739,-4679,12949,1894,-840,1777,5311 } },
    { "Samsung NX2", 0, 0xfff, /* used for NX20/NX200/NX210 */
      { 6933,-2268,-753,-4921,13387,1647,-803,1641,6096 } },
    { "Samsung NX1000", 0, 0,
      { 6933,-2268,-753,-4921,13387,1647,-803,1641,6096 } },
    { "Samsung NX1100", 0, 0,
      { 6933,-2268,-753,-4921,13387,1647,-803,1641,6096 } },
    { "Samsung NX11", 0, 0,
      { 10332,-3234,-1168,-6111,14639,1520,-1352,2647,8331 } },
    { "Samsung NX10", 0, 0, /* used for NX10/NX100 */
      { 10332,-3234,-1168,-6111,14639,1520,-1352,2647,8331 } },
    { "Samsung NX500", 0, 0,
      { 10686,-4042,-1052,-3595,13238,276,-464,1259,5931 } },
    { "Samsung NX5", 0, 0,
      { 10332,-3234,-1168,-6111,14639,1520,-1352,2647,8331 } },
    { "Samsung NX1", 0, 0,
      { 10686,-4042,-1052,-3595,13238,276,-464,1259,5931 } },
    { "Samsung NXF1", 0, 0, /* added */
      { 5222,-1196,-550,-6540,14649,2009,-1666,2819,5657 } },
    { "Samsung WB2000", 0, 0xfff,
      { 12093,-3557,-1155,-1000,9534,1733,-22,1787,4576 } },
    { "Samsung GX10", 0, 0, /* added */ /* Pentax K10D */
      { 9679,-2965,-811,-8622,16514,2182,-975,883,9793 } },
    { "Samsung GX-10", 0, 0, /* added */ /* Pentax K10D */
      { 9679,-2965,-811,-8622,16514,2182,-975,883,9793 } },
    { "Samsung GX-1", 0, 0, /* used for GX-1L/GX-1S */
      { 10504,-2438,-1189,-8603,16207,2531,-1022,863,12242 } },
    { "Samsung GX20", 0, 0, /* copied from Pentax K20D */
      { 9427,-2714,-868,-7493,16092,1373,-2199,3264,7180 } },
    { "Samsung GX-20", 0, 0, /* added */ /* copied from Pentax K20D */
      { 9427,-2714,-868,-7493,16092,1373,-2199,3264,7180 } },
    { "Samsung S85", 0, 0, /* DJC */
      { 11885,-3968,-1473,-4214,12299,1916,-835,1655,5549 } },

// Foveon: LibRaw color data
    { "Sigma dp0 Quattro", 2047, 0,
      { 13801,-3390,-1016,5535,3802,877,1848,4245,3730 } },
    { "Sigma dp1 Quattro", 2047, 0,
      { 13801,-3390,-1016,5535,3802,877,1848,4245,3730 } },
    { "Sigma dp2 Quattro", 2047, 0,
      { 13801,-3390,-1016,5535,3802,877,1848,4245,3730 } },
    { "Sigma dp3 Quattro", 2047, 0,
      { 13801,-3390,-1016,5535,3802,877,1848,4245,3730 } },
    { "Sigma sd Quattro H", 256, 0,
      { 1295,108,-311, 256,828,-65,-28,750,254 } }, /* temp */
    { "Sigma sd Quattro", 2047, 0,
      { 1295,108,-311, 256,828,-65,-28,750,254 } }, /* temp */
    { "Sigma SD9", 15, 4095, /* updated */
      { 13564,-2537,-751,-5465,15154,194,-67,116,10425 } },
    { "Sigma SD10", 15, 16383, /* updated */
      { 6787,-1682,575,-3091,8357,160,217,-369,12314 } },
    { "Sigma SD14", 15, 16383, /* updated */
      { 13589,-2509,-739,-5440,15104,193,-61,105,10554 } },
    { "Sigma SD15", 15, 4095, /* updated */
      { 13556,-2537,-730,-5462,15144,195,-61,106,10577 } },
// Merills + SD1
    { "Sigma SD1", 31, 4095, /* LibRaw */
      { 5133,-1895,-353,4978,744,144,3837,3069,2777 } },
    { "Sigma DP1 Merrill", 31, 4095, /* LibRaw */
      { 5133,-1895,-353,4978,744,144,3837,3069,2777 } },
    { "Sigma DP2 Merrill", 31, 4095, /* LibRaw */
      { 5133,-1895,-353,4978,744,144,3837,3069,2777 } },
    { "Sigma DP3 Merrill", 31, 4095, /* LibRaw */
      { 5133,-1895,-353,4978,744,144,3837,3069,2777 } },
// Sigma DP (non-Merill Versions)
    { "Sigma DP1X", 0, 4095, /* updated */
      { 13704,-2452,-857,-5413,15073,186,-89,151,9820 } },
    { "Sigma DP1", 0, 4095, /* updated */
      { 12774,-2591,-394,-5333,14676,207,15,-21,12127 } },
    { "Sigma DP", 0, 4095, /* LibRaw */
      //  { 7401,-1169,-567,2059,3769,1510,664,3367,5328 } },
      { 13100,-3638,-847,6855,2369,580,2723,3218,3251 } },

    { "Sinar", 0, 0, /* DJC */
      { 16442,-2956,-2422,-2877,12128,750,-1136,6066,4559 } },

    { "Sony DSC-F828", 0, 0,
      { 7924,-1910,-777,-8226,15459,2998,-1517,2199,6818,-7242,11401,3481 } },
    { "Sony DSC-R1", 0, 0,
      { 8512,-2641,-694,-8042,15670,2526,-1821,2117,7414 } },
    { "Sony DSC-V3", 0, 0,
      { 7511,-2571,-692,-7894,15088,3060,-948,1111,8128 } },

    { "Sony DSC-HX95", -800, 0,
      { 13076,-5686,-1481,-4027,12851,1251,-167,725,4937 } },
    { "Sony DSC-HX99", -800, 0,
      { 13076,-5686,-1481,-4027,12851,1251,-167,725,4937 } },

    { "Sony DSC-RX100M6", -800, 0,
      { 7325,-2321,-596,-3494,11674,2055,-668,1562,5031 } },
    { "Sony DSC-RX100M5A", -800, 0,
      { 11176,-4700,-965,-4004,12184,2032,-763,1726,5876 } },
    { "Sony DSC-RX100M", -800, 0, /* used for M2/M3/M4/M5 */
      { 6596,-2079,-562,-4782,13016,1933,-970,1581,5181 } },
    { "Sony DSC-RX100", 0, 0,
      { 8651,-2754,-1057,-3464,12207,1373,-568,1398,4434 } },
    {"Sony DSC-RX10M4", -800, 0,
      { 7699,-2566,-629,-2967,11270,1928,-378,1286,4807 } },
    { "Sony DSC-RX10",0, 0, /* same for M2/M3 */
      { 6679,-1825,-745,-5047,13256,1953,-1580,2422,5183 } },
    { "Sony DSC-RX1RM2", 0, 0,
      { 6629,-1900,-483,-4618,12349,2550,-622,1381,6514 } },
    { "Sony DSC-RX1R", 0, 0, /* updated */
      { 6344,-1612,-462,-4863,12477,2681,-865,1786,6899 } },
    { "Sony DSC-RX1", 0, 0,
      { 6344,-1612,-462,-4863,12477,2681,-865,1786,6899 } },

    {"Sony DSC-RX0", -800, 0,
      { 9396,-3507,-843,-2497,11111,1572,-343,1355,5089 } },

    { "Sony DSLR-A100", 0, 0xfeb,
      { 9437,-2811,-774,-8405,16215,2290,-710,596,7181 } },
    { "Sony DSLR-A290", 0, 0,
      { 6038,-1484,-579,-9145,16746,2512,-875,746,7218 } },
    { "Sony DSLR-A2", 0, 0,
      { 9847,-3091,-928,-8485,16345,2225,-715,595,7103 } },
    { "Sony DSLR-A300", 0, 0,
      { 9847,-3091,-928,-8485,16345,2225,-715,595,7103 } },
    { "Sony DSLR-A330", 0, 0,
      { 9847,-3091,-929,-8485,16346,2225,-714,595,7103 } },
    { "Sony DSLR-A350", 0, 0xffc,
      { 6038,-1484,-578,-9146,16746,2513,-875,746,7217 } },
    { "Sony DSLR-A380", 0, 0,
      { 6038,-1484,-579,-9145,16746,2512,-875,746,7218 } },
    { "Sony DSLR-A390", 0, 0,
      { 6038,-1484,-579,-9145,16746,2512,-875,746,7218 } },
    { "Sony DSLR-A450", 0, 0, // close to 16596 if arw is 14-bit
      { 4950,-580,-103,-5228,12542,3029,-709,1435,7371 } },
    { "Sony DSLR-A580", 0, 16596,
      { 5932,-1492,-411,-4813,12285,2856,-741,1524,6739 } },
    { "Sony DSLR-A500", 0, 16596,
      { 6046,-1127,-278,-5574,13076,2786,-691,1419,7625 } },
    { "Sony DSLR-A550", 0, 16596,
      { 4950,-580,-103,-5228,12542,3029,-709,1435,7371 } },
    { "Sony DSLR-A560", 0, 16596,
      { 4950,-580,-103,-5228,12542,3029,-709,1435,7371 } },
    { "Sony DSLR-A5", 0, 0, /* Are there any cameras not covered w/above? */
      { 4950,-580,-103,-5228,12542,3029,-709,1435,7371 } },
    { "Sony DSLR-A700", 0, 0,
      { 5775,-805,-359,-8574,16295,2391,-1943,2341,7249 } },
    { "Sony DSLR-A850", 0, 0,
      { 5413,-1162,-365,-5665,13098,2866,-608,1179,8440 } },
    { "Sony DSLR-A900", 0, 0,
      { 5209,-1072,-397,-8845,16120,2919,-1618,1803,8654 } },
    { "Sony ILCA-68", 0, 0,
      { 6435,-1903,-536,-4722,12449,2550,-663,1363,6517 } },
    { "Sony ILCA-77M2", 0, 0,
      { 5991,-1732,-443,-4100,11989,2381,-704,1467,5992 } },
    { "Sony ILCA-99M2", 0, 0,
      { 6660,-1918,-471,-4613,12398,2485,-649,1433,6447 } },
    { "Sony ILCE-9", 0, 0,
      { 6389,-1703,-378,-4562,12265,2587,-670,1489,6550 } },
    { "Sony ILCE-7M3", 0, 0,
      { 7374,-2389,-551,-5435,13162,2519,-1006,1795,6552 } },
    { "Sony ILCE-7M2", 0, 0,
      { 5271,-712,-347,-6153,13653,2763,-1601,2366,7242 } },
    { "Sony ILCE-7SM2", 0, 0,
      { 5838,-1430,-246,-3497,11477,2297,-748,1885,5778 } },
    { "Sony ILCE-7S", 0, 0,
      { 5838,-1430,-246,-3497,11477,2297,-748,1885,5778 } },
    { "Sony ILCE-7RM3", 0, 0,
      { 6640,-1847,-503,-5238,13010,2474,-993,1673,6527 } },
    { "Sony ILCE-7RM2", 0, 0,
      { 6629,-1900,-483,-4618,12349,2550,-622,1381,6514 } },
    { "Sony ILCE-7R", 0, 0,
      { 4913,-541,-202,-6130,13513,2906,-1564,2151,7183 } },
    { "Sony ILCE-7", 0, 0,
      { 5271,-712,-347,-6153,13653,2763,-1601,2366,7242 } },
    { "Sony ILCE-6300", 0, 0,
      { 5973,-1695,-419,-3826,11797,2293,-639,1398,5789 } },
    { "Sony ILCE-6400", 0, 0,
      { 5973,-1695,-419,-3826,11797,2293,-639,1398,5789 } },
    { "Sony ILCE-6500", 0, 0,
      { 5973,-1695,-419,-3826,11797,2293,-639,1398,5789 } },
    { "Sony ILCE", 0, 0, /* 3000, 5000, 5100, 6000, and QX1 */
      { 5991,-1456,-455,-4764,12135,2980,-707,1425,6701 } },
    { "Sony MODEL-NAME", 0, 0, /* added */
      { 5491,-1192,-363,-4951,12342,2948,-911,1722,7192 } },
    { "Sony NEX-5N", 0, 0,
      { 5991,-1456,-455,-4764,12135,2980,-707,1425,6701 } },
    { "Sony NEX-5R", 0, 0,
      { 6129,-1545,-418,-4930,12490,2743,-977,1693,6615 } },
    { "Sony NEX-5T", 0, 0,
      { 6129,-1545,-418,-4930,12490,2743,-977,1693,6615 } },
    { "Sony NEX-3N", 0, 0,
      { 6129,-1545,-418,-4930,12490,2743,-977,1693,6615 } },
    { "Sony NEX-3", 0, 0,
      { 6549,-1550,-436,-4880,12435,2753,-854,1868,6976 } },
    { "Sony NEX-5", 0, 0,
      { 6549,-1550,-436,-4880,12435,2753,-854,1868,6976 } },
    { "Sony NEX-6", 0, 0,
      { 6129,-1545,-418,-4930,12490,2743,-977,1693,6615 } },
    { "Sony NEX-7", 0, 0,
      { 5491,-1192,-363,-4951,12342,2948,-911,1722,7192 } },
    { "Sony NEX-VG30", 0, 0, /* added */
      { 6129,-1545,-418,-4930,12490,2743,-977,1693,6615 } },
    { "Sony NEX-VG900", 0, 0, /* added */
      { 6344,-1612,-462,-4863,12477,2681,-865,1786,6899 } },
    { "Sony NEX", 0, 0, /* NEX-C3, NEX-F3, NEX-VG20 */
      { 5991,-1456,-455,-4764,12135,2980,-707,1425,6701 } },
    { "Sony SLT-A33", 0, 0,
      { 6069,-1221,-366,-5221,12779,2734,-1024,2066,6834 } },
    { "Sony SLT-A35", 0, 0,
      { 5986,-1618,-415,-4557,11820,3120,-681,1404,6971 } },
    { "Sony SLT-A37", 0, 0,
      { 5991,-1456,-455,-4764,12135,2980,-707,1425,6701 } },
    { "Sony SLT-A55", 0, 0,
      { 5932,-1492,-411,-4813,12285,2856,-741,1524,6739 } },
    { "Sony SLT-A57", 0, 0,
      { 5991,-1456,-455,-4764,12135,2980,-707,1425,6701 } },
    { "Sony SLT-A58", 0, 0,
      { 5991,-1456,-455,-4764,12135,2980,-707,1425,6701 } },
    { "Sony SLT-A65", 0, 0,
      { 5491,-1192,-363,-4951,12342,2948,-911,1722,7192 } },
    { "Sony SLT-A77", 0, 0,
      { 5491,-1192,-363,-4951,12342,2948,-911,1722,7192 } },
    { "Sony SLT-A99", 0, 0,
      { 6344,-1612,-462,-4863,12477,2681,-865,1786,6899 } },
    { "YI M1", 0, 0,
      { 7712,-2059,-653,-3882,11494,2726,-710,1332,5958 } },
  };
  // clang-format on

  double cam_xyz[4][3];
  char name[130];
  int i, j;

  if (colors > 4 || colors < 1)
    return;

  int bl4 = (cblack[0] + cblack[1] + cblack[2] + cblack[3]) / 4, bl64 = 0;
  if (cblack[4] * cblack[5] > 0)
  {
    for (unsigned c = 0; c < 4096 && c < cblack[4] * cblack[5]; c++)
      bl64 += cblack[c + 6];
    bl64 /= cblack[4] * cblack[5];
  }
  int rblack = black + bl4 + bl64;

  sprintf(name, "%s %s", t_make, t_model);
  for (i = 0; i < sizeof table / sizeof *table; i++)
    if (!strncasecmp(name, table[i].prefix, strlen(table[i].prefix)))
    {
      if (!dng_version)
      {
        if (table[i].t_black > 0)
        {
          black = (ushort)table[i].t_black;
          memset(cblack, 0, sizeof(cblack));
        }
        else if (table[i].t_black < 0 && rblack == 0)
        {
          black = (ushort)(-table[i].t_black);
          memset(cblack, 0, sizeof(cblack));
        }
        if (table[i].t_maximum)
          maximum = (ushort)table[i].t_maximum;
      }
      if (table[i].trans[0])
      {
        for (raw_color = j = 0; j < 12; j++)
#ifdef LIBRAW_LIBRARY_BUILD
          if (internal_only)
            imgdata.color.cam_xyz[0][j] = table[i].trans[j] / 10000.0;
          else
            imgdata.color.cam_xyz[0][j] =
#endif
                ((double *)cam_xyz)[j] = table[i].trans[j] / 10000.0;
#ifdef LIBRAW_LIBRARY_BUILD
        if (!internal_only)
#endif
          cam_xyz_coeff(rgb_cam, cam_xyz);
      }
      break;
    }
}

void CLASS simple_coeff(int index)
{
  static const float table[][12] = {/* index 0 -- all Foveon cameras */
                                    {1.4032, -0.2231, -0.1016, -0.5263, 1.4816, 0.017, -0.0112, 0.0183, 0.9113},
                                    /* index 1 -- Kodak DC20 and DC25 */
                                    {2.25, 0.75, -1.75, -0.25, -0.25, 0.75, 0.75, -0.25, -0.25, -1.75, 0.75, 2.25},
                                    /* index 2 -- Logitech Fotoman Pixtura */
                                    {1.893, -0.418, -0.476, -0.495, 1.773, -0.278, -1.017, -0.655, 2.672},
                                    /* index 3 -- Nikon E880, E900, and E990 */
                                    {-1.936280, 1.800443, -1.448486, 2.584324, 1.405365, -0.524955, -0.289090, 0.408680,
                                     -1.204965, 1.082304, 2.941367, -1.818705}};
  int i, c;

  for (raw_color = i = 0; i < 3; i++)
    FORCC rgb_cam[i][c] = table[index][i * colors + c];
}

short CLASS guess_byte_order(int words)
{
  uchar test[4][2];
  int t = 2, msb;
  double diff, sum[2] = {0, 0};

  fread(test[0], 2, 2, ifp);
  for (words -= 2; words--;)
  {
    fread(test[t], 2, 1, ifp);
    for (msb = 0; msb < 2; msb++)
    {
      diff = (test[t ^ 2][msb] << 8 | test[t ^ 2][!msb]) - (test[t][msb] << 8 | test[t][!msb]);
      sum[msb] += diff * diff;
    }
    t = (t + 1) & 3;
  }
  return sum[0] < sum[1] ? 0x4d4d : 0x4949;
}

float CLASS find_green(int bps, int bite, int off0, int off1)
{
  UINT64 bitbuf = 0;
  int vbits, col, i, c;
  ushort img[2][2064];
  double sum[] = {0, 0};
  if(width > 2064) return 0.f; // too wide

  FORC(2)
  {
    fseek(ifp, c ? off1 : off0, SEEK_SET);
    for (vbits = col = 0; col < width; col++)
    {
      for (vbits -= bps; vbits < 0; vbits += bite)
      {
        bitbuf <<= bite;
        for (i = 0; i < bite; i += 8)
          bitbuf |= (unsigned)(fgetc(ifp) << i);
      }
      img[c][col] = bitbuf << (64 - bps - vbits) >> (64 - bps);
    }
  }
  FORC(width - 1)
  {
    sum[c & 1] += ABS(img[0][c] - img[1][c + 1]);
    sum[~c & 1] += ABS(img[1][c] - img[0][c + 1]);
  }
  return 100 * log(sum[0] / sum[1]);
}

#ifdef LIBRAW_LIBRARY_BUILD
static void remove_trailing_spaces(char *string, size_t len)
{
  if (len < 1)
    return; // not needed, b/c sizeof of make/model is 64
  string[len - 1] = 0;
  if (len < 3)
    return; // also not needed
  len = strnlen(string, len - 1);
  for (int i = len - 1; i >= 0; i--)
  {
    if (isspace((unsigned char)string[i]))
      string[i] = 0;
    else
      break;
  }
}

void CLASS initdata()
{
  tiff_flip = flip = filters = UINT_MAX; /* unknown */
  raw_height = raw_width = fuji_width = fuji_layout = cr2_slice[0] = 0;
  maximum = height = width = top_margin = left_margin = 0;
  cdesc[0] = desc[0] = artist[0] = make[0] = model[0] = model2[0] = 0;
  iso_speed = shutter = aperture = focal_len = unique_id = 0;
  tiff_nifds = 0;
  memset(tiff_ifd, 0, sizeof tiff_ifd);
  for (int i = 0; i < LIBRAW_IFD_MAXCOUNT; i++)
  {
    tiff_ifd[i].dng_color[0].illuminant = tiff_ifd[i].dng_color[1].illuminant = 0xffff;
    for (int c = 0; c < 4; c++)
      tiff_ifd[i].dng_levels.analogbalance[c] = 1.0f;
  }
  for (int i = 0; i < 0x10000; i++)
    curve[i] = i;
  memset(gpsdata, 0, sizeof gpsdata);
  memset(cblack, 0, sizeof cblack);
  memset(white, 0, sizeof white);
  memset(mask, 0, sizeof mask);
  thumb_offset = thumb_length = thumb_width = thumb_height = 0;
  load_raw = thumb_load_raw = 0;
  write_thumb = &CLASS jpeg_thumb;
  data_offset = meta_offset = meta_length = tiff_bps = tiff_compress = 0;
  kodak_cbpp = zero_after_ff = dng_version = load_flags = 0;
  timestamp = shot_order = tiff_samples = black = is_foveon = 0;
  mix_green = profile_length = data_error = zero_is_bad = 0;
  pixel_aspect = is_raw = raw_color = 1;
  tile_width = tile_length = 0;
  is_NikonTransfer = 0;
  is_4K_RAFdata = 0;
  is_PentaxRicohMakernotes = 0;
}

#endif
/*
   Identify which camera created this file, and set global variables
   accordingly.
 */
void CLASS identify()
{
  static const short pana[][6] = {
    { 3130, 1743,  4,  0, -6,  0 }, /* 00 */
    { 3130, 2055,  4,  0, -6,  0 }, /* 01 */
    { 3130, 2319,  4,  0, -6,  0 }, /* 02 DMC-FZ8 */
    { 3170, 2103, 18,  0,-42, 20 }, /* 03 */
    { 3170, 2367, 18, 13,-42,-21 }, /* 04 */
    { 3177, 2367,  0,  0, -1,  0 }, /* 05 DMC-L1 */
    { 3304, 2458,  0,  0, -1,  0 }, /* 06 DMC-FZ30 */
    { 3330, 2463,  9,  0, -5,  0 }, /* 07 DMC-FZ18 */
    { 3330, 2479,  9,  0,-17,  4 }, /* 08 */
    { 3370, 1899, 15,  0,-44, 20 }, /* 09 */
    { 3370, 2235, 15,  0,-44, 20 }, /* 10 */
    { 3370, 2511, 15, 10,-44,-21 }, /* 11 */
    { 3690, 2751,  3,  0, -8, -3 }, /* 12 DMC-FZ50 */
    { 3710, 2751,  0,  0, -3,  0 }, /* 13 DMC-L10 */
    { 3724, 2450,  0,  0,  0, -2 }, /* 14 */
    { 3770, 2487, 17,  0,-44, 19 }, /* 15 */
    { 3770, 2799, 17, 15,-44,-19 }, /* 16 */
    { 3880, 2170,  6,  0, -6,  0 }, /* 17 DMC-LX1 */
    { 4060, 3018,  0,  0,  0, -2 }, /* 18 DMC-FZ38 */
    { 4290, 2391,  3,  0, -8, -1 }, /* 19 DMC-LX2 */
    { 4330, 2439, 17, 15,-44,-19 }, /* 20 D-LUX 3 */
    { 4508, 2962,  0,  0, -3, -4 }, /* 21 */
    { 4508, 3330,  0,  0, -3, -6 }, /* 22 */
  };
  static const ushort canon[][11] = {
    { 1944, 1416,   0,  0, 48,  0 },                    /* 00 0x3010000 */
    { 2144, 1560,   4,  8, 52,  2, 0, 0, 0, 25 },       /* 01 0x1120000 0x4040000 */
    { 2224, 1456,  48,  6,  0,  2 },                    /* 02 0x1140000 */
    { 2376, 1728,  12,  6, 52,  2 },                    /* 03 0x1100000 0x1110000 0x1190000 0x1210000*/
    { 2672, 1968,  12,  6, 44,  2 },                    /* 04 0x1290000 0x1310000 0x1390000 */
    { 3152, 2068,  64, 12,  0,  0, 16 },                /* 05 0x1668000 0x80000168 0x80000170 */
    { 3160, 2344,  44, 12,  4,  4 },                    /* 06 0x1400000 0x1380000 */
    { 3344, 2484,   4,  6, 52,  6 },                    /* 07 0x1370000 */
    { 3516, 2328,  42, 14,  0,  0 },                    /* 08 0x80000189 */
    { 3596, 2360,  74, 12,  0,  0 },                    /* 09 0x80000174 0x80000175 0x80000232 0x80000234 */
    { 3744, 2784,  52, 12,  8, 12 },                    /* 10 0x2700000 0x2720000 0x2920000 0x2950000 */
    { 3944, 2622,  30, 18,  6,  2 },                    /* 11 0x80000190 */
    { 3948, 2622,  42, 18,  0,  2 },                    /* 12 0x80000236 0x80000254 */
    { 3984, 2622,  76, 20,  0,  2, 14 },                /* 13 0x80000169 */
    { 4104, 3048,  48, 12, 24, 12 },                    /* 14 0x2230000 */
    { 4116, 2178,   4,  2,  0,  0 },                    /* 15 */
    { 4152, 2772, 192, 12,  0,  0 },                    /* 16 0x2460000 */
    { 4160, 3124, 104, 11,  8, 65 },                    /* 17 0x3110000 0x3320000 0x3330000 0x3360000 */
    { 4176, 3062,  96, 17,  8,  0, 0, 16, 0, 7, 0x49 }, /* 18 0x3340000 */
    { 4192, 3062,  96, 17, 24,  0, 0, 16, 0, 0, 0x49 }, /* 19 0x3540000 0x3550000 */
    { 4312, 2876,  22, 18,  0,  2 },                    /* 20 0x80000176 */
    { 4352, 2874,  62, 18,  0,  0 },                    /* 21 0x80000288 */
    { 4476, 2954,  90, 34,  0,  0 },                    /* 22 0x80000213 */
    { 4480, 3348,  12, 10, 36, 12, 0, 0, 0, 18, 0x49 }, /* 23 0x2490000 */
    { 4480, 3366,  80, 50,  0,  0 },                    /* 24 0x3640000 */
    { 4496, 3366,  80, 50, 12,  0 },                    /* 25 0x3080000 */
    { 4768, 3516,  96, 16,  0,  0, 0, 16 },             /* 26 0x3750000 */
    { 4832, 3204,  62, 26,  0,  0 },                    /* 27 0x80000252 */
    { 4832, 3228,  62, 51,  0,  0 },                    /* 28 0x80000261 */
    { 5108, 3349,  98, 13,  0,  0 },                    /* 29 0x80000188 */
    { 5120, 3318, 142, 45, 62,  0 },                    /* 30 0x80000281 */
    { 5280, 3528,  72, 52,  0,  0 },  /* EOS M */       /* 31 0x3840000 0x80000301 0x80000326 0x80000331 0x80000346 0x80000355 */
    { 5344, 3516, 142, 51,  0,  0 },                    /* 32 0x80000270 0x80000286 0x80000287 0x80000327 0x80000404 0x80000422 */
    { 5344, 3584, 126,100,  0,  2 },                    /* 33 0x80000269 0x80000324 */
    { 5360, 3516, 158, 51,  0,  0 },                    /* 34 0x80000250 */
    { 5568, 3708,  72, 38,  0,  0 },                    /* 35 0x80000289 0x80000302 0x80000325 0x80000328 */
    { 5632, 3710,  96, 17,  0,  0, 0, 16, 0, 0, 0x49 }, /* 36 0x3780000 0x3850000 0x3930000 0x3950000 0x3970000 0x4100000 */
    { 5712, 3774,  62, 20, 10,  2 },                    /* 37 0x80000215 */
    { 5792, 3804, 158, 51,  0,  0 },                    /* 38 0x80000218 */
    { 5920, 3950, 122, 80,  2,  0 },                    /* 39 0x80000285 */
    { 6096, 4051,  76, 35,  0,  0 },  /* EOS 2000D */   /* 40 0x80000432 */
    { 6096, 4056,  72, 34,  0,  0 },  /* EOS M3 */      /* 41 0x3740000 0x80000347 0x80000393 */
    { 6288, 4056, 264, 36,  0,  0 },  /* EOS 80D */     /* 42 0x3940000 0x3980000 0x4070000 0x4180000 0x80000350 0x80000405 0x80000408 0x80000417 */
    { 6384, 4224, 120, 44,  0,  0 },  /* 6D II */       /* 43 0x80000406 */
    { 6880, 4544, 136, 42,  0,  0 },  /* EOS 5D4 */     /* 44 0x80000349 */
    { 8896, 5920, 160, 64,  0,  0 },                    /* 45 0x80000382 0x80000401 */
  };
  static const struct
  {
    ushort id;
    char t_model[20];
  } unique[] =
      {
          {0x001, "EOS-1D"},
          {0x167, "EOS-1DS"},
          {0x168, "EOS 10D"},
          {0x169, "EOS-1D Mark III"},
          {0x170, "EOS 300D"},
          {0x174, "EOS-1D Mark II"},
          {0x175, "EOS 20D"},
          {0x176, "EOS 450D"},
          {0x188, "EOS-1Ds Mark II"},
          {0x189, "EOS 350D"},
          {0x190, "EOS 40D"},
          {0x213, "EOS 5D"},
          {0x215, "EOS-1Ds Mark III"},
          {0x218, "EOS 5D Mark II"},
          {0x232, "EOS-1D Mark II N"},
          {0x234, "EOS 30D"},
          {0x236, "EOS 400D"},
          {0x250, "EOS 7D"},
          {0x252, "EOS 500D"},
          {0x254, "EOS 1000D"},
          {0x261, "EOS 50D"},
          {0x269, "EOS-1D X"},
          {0x270, "EOS 550D"},
          {0x281, "EOS-1D Mark IV"},
          {0x285, "EOS 5D Mark III"},
          {0x286, "EOS 600D"},
          {0x287, "EOS 60D"},
          {0x288, "EOS 1100D"},
          {0x289, "EOS 7D Mark II"},
          {0x301, "EOS 650D"},
          {0x302, "EOS 6D"},
          {0x324, "EOS-1D C"},
          {0x325, "EOS 70D"},
          {0x326, "EOS 700D"},
          {0x327, "EOS 1200D"},
          {0x328, "EOS-1D X Mark II"},
          {0x331, "EOS M"},
          {0x335, "EOS M2"},
          {0x374, "EOS M3"},   /* temp */
          {0x384, "EOS M10"},  /* temp */
          {0x394, "EOS M5"},   /* temp */
          {0x398, "EOS M100"}, /* temp */
          {0x346, "EOS 100D"},
          {0x347, "EOS 760D"},
          {0x349, "EOS 5D Mark IV"},
          {0x350, "EOS 80D"},
          {0x382, "EOS 5DS"},
          {0x393, "EOS 750D"},
          {0x401, "EOS 5DS R"},
          {0x404, "EOS 1300D"},
          {0x405, "EOS 800D"},
          {0x406, "EOS 6D Mark II"},
          {0x407, "EOS M6"},
          {0x408, "EOS 77D"},
          {0x417, "EOS 200D"},
          {0x412, "EOS M50"},  /* temp */
          {0x422, "EOS 4000D"},
          {0x424, "EOS R"},
          {0x432, "EOS 2000D"},
          {0x433, "EOS RP"},
      },
    sonique[] = {
/*
Hasselblad re-badged SONY cameras, MakerNotes SonyModelID tag 0xb001 values:
  id 0x121 (289dec) : NEX-7       / Hasselblad Lunar
  id 0x126 (294dec) : SLT-A99     / Hasselblad HV
  id 0x129 (297dec) : DSC-RX100   / Hasselblad Stellar
  id 0x134 (308dec) : DSC-RX100M2 / Hasselblad Stellar II
  id 0x137 (311dec) : ILCE-7R     / Hasselblad Lusso
*/
        {0x002, "DSC-R1"},      {0x100, "DSLR-A100"},   {0x101, "DSLR-A900"},  {0x102, "DSLR-A700"},
        {0x103, "DSLR-A200"},   {0x104, "DSLR-A350"},   {0x105, "DSLR-A300"},  {0x106, "DSLR-A900"},
        {0x107, "DSLR-A380"},   {0x108, "DSLR-A330"},   {0x109, "DSLR-A230"},  {0x10a, "DSLR-A290"},
        {0x10d, "DSLR-A850"},   {0x10e, "DSLR-A850"},   {0x111, "DSLR-A550"},  {0x112, "DSLR-A500"},
        {0x113, "DSLR-A450"},   {0x116, "NEX-5"},       {0x117, "NEX-3"},      {0x118, "SLT-A33"},
        {0x119, "SLT-A55V"},    {0x11a, "DSLR-A560"},   {0x11b, "DSLR-A580"},  {0x11c, "NEX-C3"},
        {0x11d, "SLT-A35"},     {0x11e, "SLT-A65V"},    {0x11f, "SLT-A77V"},   {0x120, "NEX-5N"},
        {0x121, "NEX-7"},       {0x122, "NEX-VG20E"},   {0x123, "SLT-A37"},    {0x124, "SLT-A57"},
        {0x125, "NEX-F3"},      {0x126, "SLT-A99V"},    {0x127, "NEX-6"},      {0x128, "NEX-5R"},
        {0x129, "DSC-RX100"},   {0x12a, "DSC-RX1"},     {0x12b, "NEX-VG900"},  {0x12c, "NEX-VG30E"},
        {0x12e, "ILCE-3000"},   {0x12f, "SLT-A58"},     {0x131, "NEX-3N"},     {0x132, "ILCE-7"},
        {0x133, "NEX-5T"},      {0x134, "DSC-RX100M2"}, {0x135, "DSC-RX10"},   {0x136, "DSC-RX1R"},
        {0x137, "ILCE-7R"},     {0x138, "ILCE-6000"},   {0x139, "ILCE-5000"},  {0x13d, "DSC-RX100M3"},
        {0x13e, "ILCE-7S"},     {0x13f, "ILCA-77M2"},   {0x153, "ILCE-5100"},  {0x154, "ILCE-7M2"},
        {0x155, "DSC-RX100M4"}, {0x156, "DSC-RX10M2"},  {0x158, "DSC-RX1RM2"}, {0x15a, "ILCE-QX1"},
        {0x15b, "ILCE-7RM2"},   {0x15e, "ILCE-7SM2"},   {0x161, "ILCA-68"},    {0x162, "ILCA-99M2"},
        {0x163, "DSC-RX10M3"},  {0x164, "DSC-RX100M5"}, {0x165, "ILCE-6300"},  {0x166, "ILCE-9"},
        {0x168, "ILCE-6500"},   {0x16a, "ILCE-7RM3"},   {0x16b, "ILCE-7M3"},   {0x16c, "DSC-RX0"},
        {0x16d, "DSC-RX10M4"},  {0x16e, "DSC-RX100M6"}, {0x16f, "DSC-HX99"},   {0x171, "DSC-RX100M5A"},
        {0x173, "ILCE-6400"},
    };

  static const char *orig, panalias[][16] = {
    "@DC-FZ80", "DC-FZ81", "DC-FZ82", "DC-FZ83", "DC-FZ85",
    "@DC-GF9", "DC-GX850", "DC-GX800",
    "@DC-GF10", "DC-GF90",
    "@DC-GX9", "DC-GX7MK3",
    "@DC-ZS70", "DC-TZ90", "DC-TZ91", "DC-TZ92", "DC-TZ93",
    "@DMC-FZ40", "DMC-FZ42", "DMC-FZ45",
    "@DMC-FZ2500", "DMC-FZ2000", "DMC-FZH1",
    "@DMC-G8", "DMC-G80", "DMC-G81", "DMC-G85",
    "@DMC-GX85", "DMC-GX80", "DMC-GX7MK2",
    "@DMC-LX9", "DMC-LX10", "DMC-LX15",
    "@DMC-ZS40", "DMC-TZ60", "DMC-TZ61",
    "@DMC-ZS50", "DMC-TZ70", "DMC-TZ71",
    "@DMC-ZS60", "DMC-TZ80", "DMC-TZ81", "DMC-TZ85",
    "@DMC-ZS100", "DMC-ZS110", "DMC-TZ100", "DMC-TZ101", "DMC-TZ110", "DMC-TX1",
    "@DC-ZS200", "DC-TX2", "DC-TZ200", "DC-TZ202", "DC-TZ220", "DC-ZS220", "C-Lux",
    "@DC-ZS80", "DC-TZ96", "DC-TZ95",
    "@DMC-FZ50", "V-LUX1", "V-LUX 1",
    "@DMC-L1", "DIGILUX3", "DIGILUX 3",
    "@DMC-LC1", "DIGILUX2", "DIGILUX 2",
    "@DC-LX100M2", "D-Lux 7",
    "@DMC-LX100", "D-LUX (Typ 109)", "D-Lux (Typ 109)",
    "@DMC-LF1", "C (Typ 112)",
    "@DMC-LX1", "D-LUX2", "D-LUX 2",
    "@DMC-LX2", "D-LUX3", "D-LUX 3",
    "@DMC-LX3", "D-LUX 4",
    "@DMC-LX5", "D-LUX 5",
    "@DMC-LX7", "D-LUX 6",
    "@DMC-FZ1000", "V-LUX (Typ 114)",
    "@DMC-FZ100", "V-LUX 2",
    "@DMC-FZ150", "V-LUX 3",
    "@DMC-FZ200", "V-LUX 4",
  };

#ifdef LIBRAW_LIBRARY_BUILD
  static const libraw_custom_camera_t const_table[]
#else
  static const struct
  {
    unsigned fsize;
    ushort rw, rh;
    uchar lm, tm, rm, bm;
    ushort lf;
    uchar cf, max, flags;
    char t_make[10], t_model[20];
    ushort offset;
  } table[]
#endif
      = {
          {786432, 1024, 768, 0, 0, 0, 0, 0, 0x94, 0, 0, "AVT", "F-080C"},
          {1447680, 1392, 1040, 0, 0, 0, 0, 0, 0x94, 0, 0, "AVT", "F-145C"},
          {1920000, 1600, 1200, 0, 0, 0, 0, 0, 0x94, 0, 0, "AVT", "F-201C"},
          {5067304, 2588, 1958, 0, 0, 0, 0, 0, 0x94, 0, 0, "AVT", "F-510C"},
          {5067316, 2588, 1958, 0, 0, 0, 0, 0, 0x94, 0, 0, "AVT", "F-510C", 12},
          {10134608, 2588, 1958, 0, 0, 0, 0, 9, 0x94, 0, 0, "AVT", "F-510C"},
          {10134620, 2588, 1958, 0, 0, 0, 0, 9, 0x94, 0, 0, "AVT", "F-510C", 12},
          {16157136, 3272, 2469, 0, 0, 0, 0, 9, 0x94, 0, 0, "AVT", "F-810C"},
          {15980544, 3264, 2448, 0, 0, 0, 0, 8, 0x61, 0, 1, "AgfaPhoto", "DC-833m"},
          {9631728, 2532, 1902, 0, 0, 0, 0, 96, 0x61, 0, 0, "Alcatel", "5035D"},
          {31850496, 4608, 3456, 0, 0, 0, 0, 0, 0x94, 0, 0, "GITUP", "GIT2 4:3"},
          {23887872, 4608, 2592, 0, 0, 0, 0, 0, 0x94, 0, 0, "GITUP", "GIT2 16:9"},
          {32257024, 4624, 3488, 8, 2, 16, 2, 0, 0x94, 0, 0, "GITUP", "GIT2P 4:3"},
          {24192768, 4624, 2616, 8, 2, 16, 2, 0, 0x94, 0, 0, "GITUP", "GIT2P 16:9"},
	  {18016000, 4000, 2252, 0, 0, 0, 0, 0, 0x94, 0, 0, "GITUP", "G3DUO 16:9"},
	  //{24000000, 4000, 3000, 0, 0, 0, 0, 0, 0x94, 0, 0, "GITUP", "G3DUO 4:3"}, // Conflict w Samsung WB550

          //   Android Raw dumps id start
          //   File Size in bytes Horizontal Res Vertical Flag then bayer order eg 0x16 bbgr 0x94 rggb
          {1540857, 2688, 1520, 0, 0, 0, 0, 1, 0x61, 0, 0, "Samsung", "S3"},
          {2658304, 1212, 1096, 0, 0, 0, 0, 1, 0x16, 0, 0, "LG", "G3FrontMipi"},
          {2842624, 1296, 1096, 0, 0, 0, 0, 1, 0x16, 0, 0, "LG", "G3FrontQCOM"},
          {2969600, 1976, 1200, 0, 0, 0, 0, 1, 0x16, 0, 0, "Xiaomi", "MI3wMipi"},
          {3170304, 1976, 1200, 0, 0, 0, 0, 1, 0x16, 0, 0, "Xiaomi", "MI3wQCOM"},
          {3763584, 1584, 1184, 0, 0, 0, 0, 96, 0x61, 0, 0, "I_Mobile", "I_StyleQ6"},
          {5107712, 2688, 1520, 0, 0, 0, 0, 1, 0x61, 0, 0, "OmniVisi", "UltraPixel1"},
          {5382640, 2688, 1520, 0, 0, 0, 0, 1, 0x61, 0, 0, "OmniVisi", "UltraPixel2"},
          {5664912, 2688, 1520, 0, 0, 0, 0, 1, 0x61, 0, 0, "OmniVisi", "4688"},
          {5664912, 2688, 1520, 0, 0, 0, 0, 1, 0x61, 0, 0, "OmniVisi", "4688"},
          {5364240, 2688, 1520, 0, 0, 0, 0, 1, 0x61, 0, 0, "OmniVisi", "4688"},
          {6299648, 2592, 1944, 0, 0, 0, 0, 1, 0x16, 0, 0, "OmniVisi", "OV5648"},
          {6721536, 2592, 1944, 0, 0, 0, 0, 0, 0x16, 0, 0, "OmniVisi", "OV56482"},
          {6746112, 2592, 1944, 0, 0, 0, 0, 0, 0x16, 0, 0, "HTC", "OneSV"},
          {9631728, 2532, 1902, 0, 0, 0, 0, 96, 0x61, 0, 0, "Sony", "5mp"},
          {9830400, 2560, 1920, 0, 0, 0, 0, 96, 0x61, 0, 0, "NGM", "ForwardArt"},
          {10186752, 3264, 2448, 0, 0, 0, 0, 1, 0x94, 0, 0, "Sony", "IMX219-mipi 8mp"},
          {10223360, 2608, 1944, 0, 0, 0, 0, 96, 0x16, 0, 0, "Sony", "IMX"},
          {10782464, 3282, 2448, 0, 0, 0, 0, 0, 0x16, 0, 0, "HTC", "MyTouch4GSlide"},
          {10788864, 3282, 2448, 0, 0, 0, 0, 0, 0x16, 0, 0, "Xperia", "L"},
          {15967488, 3264, 2446, 0, 0, 0, 0, 96, 0x16, 0, 0, "OmniVison", "OV8850"},
          {16224256, 4208, 3082, 0, 0, 0, 0, 1, 0x16, 0, 0, "LG", "G3MipiL"},
          {16424960, 4208, 3120, 0, 0, 0, 0, 1, 0x16, 0, 0, "IMX135", "MipiL"},
          {17326080, 4164, 3120, 0, 0, 0, 0, 1, 0x16, 0, 0, "LG", "G3LQCom"},
          {17522688, 4212, 3120, 0, 0, 0, 0, 0, 0x16, 0, 0, "Sony", "IMX135-QCOM"},
          {19906560, 4608, 3456, 0, 0, 0, 0, 1, 0x16, 0, 0, "Gione", "E7mipi"},
          {19976192, 5312, 2988, 0, 0, 0, 0, 1, 0x16, 0, 0, "LG", "G4"},
          {20389888, 4632, 3480, 0, 0, 0, 0, 1, 0x16, 0, 0, "Xiaomi", "RedmiNote3Pro"},
          {20500480, 4656, 3496, 0, 0, 0, 0, 1, 0x94, 0, 0, "Sony", "IMX298-mipi 16mp"},
          {21233664, 4608, 3456, 0, 0, 0, 0, 1, 0x16, 0, 0, "Gione", "E7qcom"},
          {26023936, 4192, 3104, 0, 0, 0, 0, 96, 0x94, 0, 0, "THL", "5000"},
          {26257920, 4208, 3120, 0, 0, 0, 0, 96, 0x94, 0, 0, "Sony", "IMX214"},
          {26357760, 4224, 3120, 0, 0, 0, 0, 96, 0x61, 0, 0, "OV", "13860"},
          {41312256, 5248, 3936, 0, 0, 0, 0, 96, 0x61, 0, 0, "Meizu", "MX4"},
          {42923008, 5344, 4016, 0, 0, 0, 0, 96, 0x61, 0, 0, "Sony", "IMX230"},
          //   Android Raw dumps id end
          {20137344, 3664, 2748, 0, 0, 0, 0, 0x40, 0x49, 0, 0, "Aptina", "MT9J003", 0xffff},
          {2868726, 1384, 1036, 0, 0, 0, 0, 64, 0x49, 0, 8, "Baumer", "TXG14", 1078},
          {5298000, 2400, 1766, 12, 12, 44, 2, 40, 0x94, 0, 2, "Canon", "PowerShot SD300"},
          {6553440, 2664, 1968, 4, 4, 44, 4, 40, 0x94, 0, 2, "Canon", "PowerShot A460"},
          {6573120, 2672, 1968, 12, 8, 44, 0, 40, 0x94, 0, 2, "Canon", "PowerShot A610"},
          {6653280, 2672, 1992, 10, 6, 42, 2, 40, 0x94, 0, 2, "Canon", "PowerShot A530"},
          {7710960, 2888, 2136, 44, 8, 4, 0, 40, 0x94, 0, 2, "Canon", "PowerShot S3 IS"},
          {9219600, 3152, 2340, 36, 12, 4, 0, 40, 0x94, 0, 2, "Canon", "PowerShot A620"},
          {9243240, 3152, 2346, 12, 7, 44, 13, 40, 0x49, 0, 2, "Canon", "PowerShot A470"},
          {10341600, 3336, 2480, 6, 5, 32, 3, 40, 0x94, 0, 2, "Canon", "PowerShot A720 IS"},
          {10383120, 3344, 2484, 12, 6, 44, 6, 40, 0x94, 0, 2, "Canon", "PowerShot A630"},
          {12945240, 3736, 2772, 12, 6, 52, 6, 40, 0x94, 0, 2, "Canon", "PowerShot A640"},
          {15636240, 4104, 3048, 48, 12, 24, 12, 40, 0x94, 0, 2, "Canon", "PowerShot A650"},
          {15467760, 3720, 2772, 6, 12, 30, 0, 40, 0x94, 0, 2, "Canon", "PowerShot SX110 IS"},
          {15534576, 3728, 2778, 12, 9, 44, 9, 40, 0x94, 0, 2, "Canon", "PowerShot SX120 IS"},
          {18653760, 4080, 3048, 24, 12, 24, 12, 40, 0x94, 0, 2, "Canon", "PowerShot SX20 IS"},
          {18763488, 4104, 3048, 10, 22, 82, 22, 8, 0x49, 0, 0, "Canon", "PowerShot D10"},
          {19131120, 4168, 3060, 92, 16, 4, 1, 40, 0x94, 0, 2, "Canon", "PowerShot SX220 HS"},
          {21936096, 4464, 3276, 25, 10, 73, 12, 40, 0x16, 0, 2, "Canon", "PowerShot SX30 IS"},
          {24724224, 4704, 3504, 8, 16, 56, 8, 40, 0x49, 0, 2, "Canon", "PowerShot A3300 IS"},
          {30858240, 5248, 3920, 8, 16, 56, 16, 40, 0x94, 0, 2, "Canon", "IXUS 160"},
          {1976352, 1632, 1211, 0, 2, 0, 1, 0, 0x94, 0, 1, "Casio", "QV-2000UX"},
          {3217760, 2080, 1547, 0, 0, 10, 1, 0, 0x94, 0, 1, "Casio", "QV-3*00EX"},
          {6218368, 2585, 1924, 0, 0, 9, 0, 0, 0x94, 0, 1, "Casio", "QV-5700"},
          {7816704, 2867, 2181, 0, 0, 34, 36, 0, 0x16, 0, 1, "Casio", "EX-Z60"},
          {2937856, 1621, 1208, 0, 0, 1, 0, 0, 0x94, 7, 13, "Casio", "EX-S20"},
          {4948608, 2090, 1578, 0, 0, 32, 34, 0, 0x94, 7, 1, "Casio", "EX-S100"},
          {6054400, 2346, 1720, 2, 0, 32, 0, 0, 0x94, 7, 1, "Casio", "QV-R41"},
          {7426656, 2568, 1928, 0, 0, 0, 0, 0, 0x94, 0, 1, "Casio", "EX-P505"},
          {7530816, 2602, 1929, 0, 0, 22, 0, 0, 0x94, 7, 1, "Casio", "QV-R51"},
          {7542528, 2602, 1932, 0, 0, 32, 0, 0, 0x94, 7, 1, "Casio", "EX-Z50"},
          {7562048, 2602, 1937, 0, 0, 25, 0, 0, 0x16, 7, 1, "Casio", "EX-Z500"},
          {7753344, 2602, 1986, 0, 0, 32, 26, 0, 0x94, 7, 1, "Casio", "EX-Z55"},
          {9313536, 2858, 2172, 0, 0, 14, 30, 0, 0x94, 7, 1, "Casio", "EX-P600"},
          {10834368, 3114, 2319, 0, 0, 27, 0, 0, 0x94, 0, 1, "Casio", "EX-Z750"},
          {10843712, 3114, 2321, 0, 0, 25, 0, 0, 0x94, 0, 1, "Casio", "EX-Z75"},
          {10979200, 3114, 2350, 0, 0, 32, 32, 0, 0x94, 7, 1, "Casio", "EX-P700"},
          {12310144, 3285, 2498, 0, 0, 6, 30, 0, 0x94, 0, 1, "Casio", "EX-Z850"},
          {12489984, 3328, 2502, 0, 0, 47, 35, 0, 0x94, 0, 1, "Casio", "EX-Z8"},
          {15499264, 3754, 2752, 0, 0, 82, 0, 0, 0x94, 0, 1, "Casio", "EX-Z1050"},
          {18702336, 4096, 3044, 0, 0, 24, 0, 80, 0x94, 7, 1, "Casio", "EX-ZR100"},
          {7684000, 2260, 1700, 0, 0, 0, 0, 13, 0x94, 0, 1, "Casio", "QV-4000"},
          {787456, 1024, 769, 0, 1, 0, 0, 0, 0x49, 0, 0, "Creative", "PC-CAM 600"},
          {28829184, 4384, 3288, 0, 0, 0, 0, 36, 0x61, 0, 0, "DJI"},
          {15151104, 4608, 3288, 0, 0, 0, 0, 0, 0x94, 0, 0, "Matrix"},
          {3840000, 1600, 1200, 0, 0, 0, 0, 65, 0x49, 0, 0, "Foculus", "531C"},
          {307200, 640, 480, 0, 0, 0, 0, 0, 0x94, 0, 0, "Generic"},
          {62464, 256, 244, 1, 1, 6, 1, 0, 0x8d, 0, 0, "Kodak", "DC20"},
          {124928, 512, 244, 1, 1, 10, 1, 0, 0x8d, 0, 0, "Kodak", "DC20"},
          {1652736, 1536, 1076, 0, 52, 0, 0, 0, 0x61, 0, 0, "Kodak", "DCS200"},
          {4159302, 2338, 1779, 1, 33, 1, 2, 0, 0x94, 0, 0, "Kodak", "C330"},
          {4162462, 2338, 1779, 1, 33, 1, 2, 0, 0x94, 0, 0, "Kodak", "C330", 3160},
          {2247168, 1232, 912, 0, 0, 16, 0, 0, 0x00, 0, 0, "Kodak", "C330"},
          {3370752, 1232, 912, 0, 0, 16, 0, 0, 0x00, 0, 0, "Kodak", "C330"},
          {6163328, 2864, 2152, 0, 0, 0, 0, 0, 0x94, 0, 0, "Kodak", "C603"},
          {6166488, 2864, 2152, 0, 0, 0, 0, 0, 0x94, 0, 0, "Kodak", "C603", 3160},
          {460800, 640, 480, 0, 0, 0, 0, 0, 0x00, 0, 0, "Kodak", "C603"},
          {9116448, 2848, 2134, 0, 0, 0, 0, 0, 0x00, 0, 0, "Kodak", "C603"},
          {12241200, 4040, 3030, 2, 0, 0, 13, 0, 0x49, 0, 0, "Kodak", "12MP"},
          {12272756, 4040, 3030, 2, 0, 0, 13, 0, 0x49, 0, 0, "Kodak", "12MP", 31556},
          {18000000, 4000, 3000, 0, 0, 0, 0, 0, 0x00, 0, 0, "Kodak", "12MP"},
          {614400, 640, 480, 0, 3, 0, 0, 64, 0x94, 0, 0, "Kodak", "KAI-0340"},
          {15360000, 3200, 2400, 0, 0, 0, 0, 96, 0x16, 0, 0, "Lenovo", "A820"},
          {3884928, 1608, 1207, 0, 0, 0, 0, 96, 0x16, 0, 0, "Micron", "2010", 3212},
          {1138688, 1534, 986, 0, 0, 0, 0, 0, 0x61, 0, 0, "Minolta", "RD175", 513},
          {1581060, 1305, 969, 0, 0, 18, 6, 6, 0x1e, 4, 1, "Nikon", "E900"},
          {2465792, 1638, 1204, 0, 0, 22, 1, 6, 0x4b, 5, 1, "Nikon", "E950"},
          {2940928, 1616, 1213, 0, 0, 0, 7, 30, 0x94, 0, 1, "Nikon", "E2100"},
          {4771840, 2064, 1541, 0, 0, 0, 1, 6, 0xe1, 0, 1, "Nikon", "E990"},
          {4775936, 2064, 1542, 0, 0, 0, 0, 30, 0x94, 0, 1, "Nikon", "E3700"},
          {5865472, 2288, 1709, 0, 0, 0, 1, 6, 0xb4, 0, 1, "Nikon", "E4500"},
          {5869568, 2288, 1710, 0, 0, 0, 0, 6, 0x16, 0, 1, "Nikon", "E4300"},
          {7438336, 2576, 1925, 0, 0, 0, 1, 6, 0xb4, 0, 1, "Nikon", "E5000"},
          {8998912, 2832, 2118, 0, 0, 0, 0, 30, 0x94, 7, 1, "Nikon", "COOLPIX S6"},
          {5939200, 2304, 1718, 0, 0, 0, 0, 30, 0x16, 0, 0, "Olympus", "C770UZ"},
          {3178560, 2064, 1540, 0, 0, 0, 0, 0, 0x94, 0, 1, "Pentax", "Optio S"},
          {4841984, 2090, 1544, 0, 0, 22, 0, 0, 0x94, 7, 1, "Pentax", "Optio S"},
          {6114240, 2346, 1737, 0, 0, 22, 0, 0, 0x94, 7, 1, "Pentax", "Optio S4"},
          {10702848, 3072, 2322, 0, 0, 0, 21, 30, 0x94, 0, 1, "Pentax", "Optio 750Z"},
          {4147200, 1920, 1080, 0, 0, 0, 0, 0, 0x49, 0, 0, "Photron", "BC2-HD"},
          {4151666, 1920, 1080, 0, 0, 0, 0, 0, 0x49, 0, 0, "Photron", "BC2-HD", 8},
          {13248000, 2208, 3000, 0, 0, 0, 0, 13, 0x61, 0, 0, "Pixelink", "A782"},
          {6291456, 2048, 1536, 0, 0, 0, 0, 96, 0x61, 0, 0, "RoverShot", "3320AF"},
          {311696, 644, 484, 0, 0, 0, 0, 0, 0x16, 0, 8, "ST Micro", "STV680 VGA"},
          {16098048, 3288, 2448, 0, 0, 24, 0, 9, 0x94, 0, 1, "Samsung", "S85"},
          {16215552, 3312, 2448, 0, 0, 48, 0, 9, 0x94, 0, 1, "Samsung", "S85"},
          {20487168, 3648, 2808, 0, 0, 0, 0, 13, 0x94, 5, 1, "Samsung", "WB550"},
          {24000000, 4000, 3000, 0, 0, 0, 0, 13, 0x94, 5, 1, "Samsung", "WB550"},
          {12582980, 3072, 2048, 0, 0, 0, 0, 33, 0x61, 0, 0, "Sinar", "", 68},
          {33292868, 4080, 4080, 0, 0, 0, 0, 33, 0x61, 0, 0, "Sinar", "", 68},
          {44390468, 4080, 5440, 0, 0, 0, 0, 33, 0x61, 0, 0, "Sinar", "", 68},
          {1409024, 1376, 1024, 0, 0, 1, 0, 0, 0x49, 0, 0, "Sony", "XCD-SX910CR"},
          {2818048, 1376, 1024, 0, 0, 1, 0, 97, 0x49, 0, 0, "Sony", "XCD-SX910CR"},
      };
#ifdef LIBRAW_LIBRARY_BUILD
  libraw_custom_camera_t table[64 + sizeof(const_table) / sizeof(const_table[0])];
#endif

  static const char *corp[] = {"AgfaPhoto", "Canon", "Casio", "Epson", "Fujifilm", "Mamiya", "Minolta",
                               "Motorola", "Kodak", "Konica", "Leica", "Nikon", "Nokia", "Olympus",
                               "Pentax", "Phase One", "Ricoh", "Samsung", "Sigma", "Sinar", "Sony", "YI"};
#ifdef LIBRAW_LIBRARY_BUILD
  char head[64], *cp;
#else
  char head[32], *cp;
#endif
  int hlen, flen, fsize, zero_fsize = 1, i, c;
  struct jhead jh;

#ifdef LIBRAW_LIBRARY_BUILD
  unsigned camera_count = parse_custom_cameras(64, table, imgdata.params.custom_camera_strings);
  for (int q = 0; q < sizeof(const_table) / sizeof(const_table[0]); q++)
    memmove(&table[q + camera_count], &const_table[q], sizeof(const_table[0]));
  camera_count += sizeof(const_table) / sizeof(const_table[0]);
#endif

  tiff_flip = flip = filters = UINT_MAX; /* unknown */
  raw_height = raw_width = fuji_width = fuji_layout = cr2_slice[0] = 0;
  maximum = height = width = top_margin = left_margin = 0;
  cdesc[0] = desc[0] = artist[0] = make[0] = model[0] = model2[0] = 0;
  iso_speed = shutter = aperture = focal_len = unique_id = 0;
  tiff_nifds = 0;
  is_NikonTransfer = 0;
  is_4K_RAFdata = 0;
  is_PentaxRicohMakernotes = 0;
  memset(tiff_ifd, 0, sizeof tiff_ifd);

#ifdef LIBRAW_LIBRARY_BUILD
  imgdata.other.CameraTemperature = imgdata.other.SensorTemperature = imgdata.other.SensorTemperature2 =
      imgdata.other.LensTemperature = imgdata.other.AmbientTemperature = imgdata.other.BatteryTemperature =
          imgdata.other.exifAmbientTemperature = -1000.0f;

  for (i = 0; i < LIBRAW_IFD_MAXCOUNT; i++)
  {
    tiff_ifd[i].dng_color[0].illuminant = tiff_ifd[i].dng_color[1].illuminant = 0xffff;
    for (int c = 0; c < 4; c++)
      tiff_ifd[i].dng_levels.analogbalance[c] = 1.0f;
  }
#endif
  memset(gpsdata, 0, sizeof gpsdata);
  memset(cblack, 0, sizeof cblack);
  memset(white, 0, sizeof white);
  memset(mask, 0, sizeof mask);
  thumb_offset = thumb_length = thumb_width = thumb_height = 0;
  load_raw = thumb_load_raw = 0;
  write_thumb = &CLASS jpeg_thumb;
  data_offset = meta_offset = meta_length = tiff_bps = tiff_compress = 0;
  kodak_cbpp = zero_after_ff = dng_version = load_flags = 0;
  timestamp = shot_order = tiff_samples = black = is_foveon = 0;
  mix_green = profile_length = data_error = zero_is_bad = 0;
  pixel_aspect = is_raw = raw_color = 1;
  tile_width = tile_length = 0;

  for (i = 0; i < 4; i++)
  {
    cam_mul[i] = i == 1;
    pre_mul[i] = i < 3;
    FORC3 cmatrix[c][i] = 0;
    FORC3 rgb_cam[c][i] = c == i;
  }
  colors = 3;
  for (i = 0; i < 0x10000; i++)
    curve[i] = i;

  order = get2();
  hlen = get4();
  fseek(ifp, 0, SEEK_SET);
#ifdef LIBRAW_LIBRARY_BUILD
  if(fread(head, 1, 64, ifp) < 64) throw LIBRAW_EXCEPTION_IO_CORRUPT;
  libraw_internal_data.unpacker_data.lenRAFData = libraw_internal_data.unpacker_data.posRAFData = 0;
#else
  fread(head, 1, 32, ifp);
#endif
  fseek(ifp, 0, SEEK_END);
  flen = fsize = ftell(ifp);
  if ((cp = (char *)memmem(head, 32, (char *)"MMMM", 4)) || (cp = (char *)memmem(head, 32, (char *)"IIII", 4)))
  {
    parse_phase_one(cp - head);
    if (cp - head && parse_tiff(0))
      apply_tiff();
  }
  else if (order == 0x4949 || order == 0x4d4d)
  {
    if (!memcmp(head + 6, "HEAPCCDR", 8))
    {
      data_offset = hlen;
#ifdef LIBRAW_LIBRARY_BUILD
      imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_FixedLens;
      imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_FixedLens;
#endif
      parse_ciff(hlen, flen - hlen, 0);
      load_raw = &CLASS canon_load_raw;
    }
    else if (parse_tiff(0))
      apply_tiff();
  }
  else if (!memcmp(head, "\xff\xd8\xff\xe1", 4) && !memcmp(head + 6, "Exif", 4))
  {
    fseek(ifp, 4, SEEK_SET);
    data_offset = 4 + get2();
    fseek(ifp, data_offset, SEEK_SET);
    if (fgetc(ifp) != 0xff)
      parse_tiff(12);
    thumb_offset = 0;
  }
  else if (!memcmp(head + 25, "ARECOYK", 7))
  {
    strcpy(make, "Contax");
    strcpy(model, "N Digital");
    fseek(ifp, 33, SEEK_SET);
    get_timestamp(1);
    fseek(ifp, 52, SEEK_SET);
    switch (get4())
    {
    case 7:
      iso_speed = 25;
      break;
    case 8:
      iso_speed = 32;
      break;
    case 9:
      iso_speed = 40;
      break;
    case 10:
      iso_speed = 50;
      break;
    case 11:
      iso_speed = 64;
      break;
    case 12:
      iso_speed = 80;
      break;
    case 13:
      iso_speed = 100;
      break;
    case 14:
      iso_speed = 125;
      break;
    case 15:
      iso_speed = 160;
      break;
    case 16:
      iso_speed = 200;
      break;
    case 17:
      iso_speed = 250;
      break;
    case 18:
      iso_speed = 320;
      break;
    case 19:
      iso_speed = 400;
      break;
    }
    shutter = libraw_powf64l(2.0f, (((float)get4()) / 8.0f)) / 16000.0f;
    FORC4 cam_mul[c ^ (c >> 1)] = get4();
    fseek(ifp, 88, SEEK_SET);
    aperture = libraw_powf64l(2.0f, ((float)get4()) / 16.0f);
    fseek(ifp, 112, SEEK_SET);
    focal_len = get4();
#ifdef LIBRAW_LIBRARY_BUILD
    fseek(ifp, 104, SEEK_SET);
    imgdata.lens.makernotes.MaxAp4CurFocal = libraw_powf64l(2.0f, ((float)get4()) / 16.0f);
    fseek(ifp, 124, SEEK_SET);
    stmread(imgdata.lens.makernotes.Lens, 32, ifp);
    imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Contax_N;
    if (imgdata.lens.makernotes.Lens[0])
      imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_Contax_N;
#endif
  }
  else if (!strcmp(head, "PXN"))
  {
    strcpy(make, "Logitech");
    strcpy(model, "Fotoman Pixtura");
  }
  else if (!strcmp(head, "qktk"))
  {
    strcpy(make, "Apple");
    strcpy(model, "QuickTake 100");
    load_raw = &CLASS quicktake_100_load_raw;
  }
  else if (!strcmp(head, "qktn"))
  {
    strcpy(make, "Apple");
    strcpy(model, "QuickTake 150");
    load_raw = &CLASS kodak_radc_load_raw;
  }
  else if (!memcmp(head, "FUJIFILM", 8))
  {
#ifdef LIBRAW_LIBRARY_BUILD
    memcpy(imgdata.makernotes.fuji.SerialSignature, head + 0x10, 0x0c);
    imgdata.makernotes.fuji.SerialSignature[0x0c] = 0;
    strncpy(model, head + 0x1c,0x20);
    model[0x20]=0;
    memcpy(model2, head + 0x3c, 4);
    model2[4] = 0;
    strcpy (imgdata.makernotes.fuji.RAFVersion, model2);
#endif
    fseek(ifp, 84, SEEK_SET);
    thumb_offset = get4();
    thumb_length = get4();
    fseek(ifp, 92, SEEK_SET);
    parse_fuji(get4());
    if (thumb_offset > 120)
    {
      fseek(ifp, 120, SEEK_SET);
      is_raw += (i = get4()) ? 1 : 0;
      if (is_raw == 2 && shot_select)
        parse_fuji(i);
    }
    load_raw = &CLASS unpacked_load_raw;
    fseek(ifp, 100 + 28 * (shot_select > 0), SEEK_SET);
    parse_tiff(data_offset = get4());
    parse_tiff(thumb_offset + 12);
    apply_tiff();
  }
  else if (!memcmp(head, "RIFF", 4))
  {
    fseek(ifp, 0, SEEK_SET);
    parse_riff();
  }
  else if (!memcmp(head + 4, "ftypqt   ", 9))
  {
    fseek(ifp, 0, SEEK_SET);
    parse_qt(fsize);
    is_raw = 0;
  }
  else if (!memcmp(head, "\0\001\0\001\0@", 6))
  {
    fseek(ifp, 6, SEEK_SET);
    fread(make, 1, 8, ifp);
    fread(model, 1, 8, ifp);
    fread(model2, 1, 16, ifp);
    data_offset = get2();
    get2();
    raw_width = get2();
    raw_height = get2();
    load_raw = &CLASS nokia_load_raw;
    filters = 0x61616161;
  }
  else if (!memcmp(head, "NOKIARAW", 8))
  {
    strcpy(make, "NOKIA");
    order = 0x4949;
    fseek(ifp, 300, SEEK_SET);
    data_offset = get4();
    i = get4(); // bytes count
    width = get2();
    height = get2();
#ifdef LIBRAW_LIBRARY_BUILD
    // Data integrity check
    if (width < 1 || width > 16000 || height < 1 || height > 16000 || i < (width * height) || i > (2 * width * height))
      throw LIBRAW_EXCEPTION_IO_CORRUPT;
#endif
    switch (tiff_bps = i * 8 / (width * height))
    {
    case 8:
      load_raw = &CLASS eight_bit_load_raw;
      break;
    case 10:
      load_raw = &CLASS nokia_load_raw;
      break;
#ifdef LIBRAW_LIBRARY_BUILD
    case 0:
      throw LIBRAW_EXCEPTION_IO_CORRUPT;
      break;
#endif
    }
    raw_height = height + (top_margin = i / (width * tiff_bps / 8) - height);
    mask[0][3] = 1;
    filters = 0x61616161;
  }
  else if (!memcmp(head, "ARRI", 4))
  {
    order = 0x4949;
    fseek(ifp, 20, SEEK_SET);
    width = get4();
    height = get4();
    strcpy(make, "ARRI");
    fseek(ifp, 668, SEEK_SET);
    fread(model, 1, 64, ifp);
    data_offset = 4096;
    load_raw = &CLASS packed_load_raw;
    load_flags = 88;
    filters = 0x61616161;
  }
  else if (!memcmp(head, "XPDS", 4))
  {
    order = 0x4949;
    fseek(ifp, 0x800, SEEK_SET);
    fread(make, 1, 41, ifp);
    raw_height = get2();
    raw_width = get2();
    fseek(ifp, 56, SEEK_CUR);
    fread(model, 1, 30, ifp);
    data_offset = 0x10000;
    load_raw = &CLASS canon_rmf_load_raw;
    gamma_curve(0, 12.25, 1, 1023);
  }
  else if (!memcmp(head + 4, "RED1", 4))
  {
    strcpy(make, "Red");
    strcpy(model, "One");
    parse_redcine();
    load_raw = &CLASS redcine_load_raw;
    gamma_curve(1 / 2.4, 12.92, 1, 4095);
    filters = 0x49494949;
  }
  else if (!memcmp(head, "DSC-Image", 9))
    parse_rollei();
  else if (!memcmp(head, "PWAD", 4))
    parse_sinar_ia();
  else if (!memcmp(head, "\0MRM", 4))
    parse_minolta(0);
  else if (!memcmp(head, "FOVb", 4))
  {
#ifdef LIBRAW_LIBRARY_BUILD
    /* no foveon support for dcraw build from libraw source */
    parse_x3f();
#endif
  }
  else if (!memcmp(head, "CI", 2))
    parse_cine();
  if (make[0] == 0)
#ifdef LIBRAW_LIBRARY_BUILD
    for (zero_fsize = i = 0; i < camera_count; i++)
#else
    for (zero_fsize = i = 0; i < sizeof table / sizeof *table; i++)
#endif
      if (fsize == table[i].fsize)
      {
        strcpy(make, table[i].t_make);
#ifdef LIBRAW_LIBRARY_BUILD
        if (!strncmp(make, "Canon", 5))
        {
          imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_FixedLens;
          imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_FixedLens;
        }
#endif
        strcpy(model, table[i].t_model);
        flip = table[i].flags >> 2;
        zero_is_bad = table[i].flags & 2;
        if (table[i].flags & 1)
          parse_external_jpeg();
        data_offset = table[i].offset == 0xffff ? 0 : table[i].offset;
        raw_width = table[i].rw;
        raw_height = table[i].rh;
        left_margin = table[i].lm;
        top_margin = table[i].tm;
        width = raw_width - left_margin - table[i].rm;
        height = raw_height - top_margin - table[i].bm;
        filters = 0x1010101U * table[i].cf;
        colors = 4 - !((filters & filters >> 1) & 0x5555);
        load_flags = table[i].lf & 0xff;
        if(table[i].lf & 0x100) /* Monochrome sensor dump */
        {
          colors = 1;
          filters = 0;
        }
        switch (tiff_bps = (fsize - data_offset) * 8 / (raw_width * raw_height))
        {
        case 6:
          load_raw = &CLASS minolta_rd175_load_raw;
          break;
        case 8:
          load_raw = &CLASS eight_bit_load_raw;
          break;
        case 10:
          if ((fsize - data_offset) / raw_height * 3 >= raw_width * 4)
          {
            load_raw = &CLASS android_loose_load_raw;
            break;
          }
          else if (load_flags & 1)
          {
            load_raw = &CLASS android_tight_load_raw;
            break;
          }
        case 12:
          load_flags |= 128;
          load_raw = &CLASS packed_load_raw;
          break;
        case 16:
          order = 0x4949 | 0x404 * (load_flags & 1);
          tiff_bps -= load_flags >> 4;
          tiff_bps -= load_flags = load_flags >> 1 & 7;
          load_raw = table[i].offset == 0xffff ? &CLASS unpacked_load_raw_reversed : &CLASS unpacked_load_raw;
        }
        maximum = (1 << tiff_bps) - (1 << table[i].max);
        break;
      }
  if (zero_fsize)
    fsize = 0;
  if (make[0] == 0)
    parse_smal(0, flen);
  if (make[0] == 0)
  {
    parse_jpeg(0);
    fseek(ifp, 0, SEEK_END);
    int sz = ftell(ifp);
#ifdef LIBRAW_LIBRARY_BUILD
    if (!strncmp(model, "RP_imx219", 9) && sz >= 0x9cb600 && !fseek(ifp, -0x9cb600, SEEK_END) &&
        fread(head, 1, 0x20, ifp) && !strncmp(head, "BRCM", 4))
    {
      strcpy(make, "Broadcom");
      strcpy(model, "RPi IMX219");
      if (raw_height > raw_width)
        flip = 5;
      data_offset = ftell(ifp) + 0x8000 - 0x20;
      parse_broadcom();
      black = 66;
      maximum = 0x3ff;
      load_raw = &CLASS broadcom_load_raw;
      thumb_offset = 0;
      thumb_length = sz - 0x9cb600 - 1;
    }
    else if (!(strncmp(model, "ov5647", 6) && strncmp(model, "RP_OV5647", 9)) && sz >= 0x61b800 &&
             !fseek(ifp, -0x61b800, SEEK_END) && fread(head, 1, 0x20, ifp) && !strncmp(head, "BRCM", 4))
    {
      strcpy(make, "Broadcom");
      if (!strncmp(model, "ov5647", 6))
        strcpy(model, "RPi OV5647 v.1");
      else
        strcpy(model, "RPi OV5647 v.2");
      if (raw_height > raw_width)
        flip = 5;
      data_offset = ftell(ifp) + 0x8000 - 0x20;
      parse_broadcom();
      black = 16;
      maximum = 0x3ff;
      load_raw = &CLASS broadcom_load_raw;
      thumb_offset = 0;
      thumb_length = sz - 0x61b800 - 1;
#else
    if (!(strncmp(model, "ov", 2) && strncmp(model, "RP_OV", 5)) && sz >= 6404096 && !fseek(ifp, -6404096, SEEK_END) &&
        fread(head, 1, 32, ifp) && !strcmp(head, "BRCMn"))
    {
      strcpy(make, "OmniVision");
      data_offset = ftell(ifp) + 0x8000 - 32;
      width = raw_width;
      raw_width = 2611;
      load_raw = &CLASS nokia_load_raw;
      filters = 0x16161616;
#endif
    }
    else
      is_raw = 0;
  }
#ifdef LIBRAW_LIBRARY_BUILD
  // make sure strings are terminated
  desc[511] = artist[63] = make[63] = model[63] = model2[63] = 0;
#endif
  for (i = 0; i < sizeof corp / sizeof *corp; i++)
    if (strcasestr(make, corp[i])) /* Simplify company names */
      strcpy(make, corp[i]);
  if ((!strncmp(make, "Kodak", 5) || !strncmp(make, "Leica", 5)) &&
      ((cp = strcasestr(model, " DIGITAL CAMERA")) || (cp = strstr(model, "FILE VERSION"))))
    *cp = 0;
  if (!strncasecmp(model, "PENTAX", 6))
    strcpy(make, "Pentax");
#ifdef LIBRAW_LIBRARY_BUILD
  remove_trailing_spaces(make, sizeof(make));
  remove_trailing_spaces(model, sizeof(model));
#else
  cp = make + strlen(make); /* Remove trailing spaces */
  while (*--cp == ' ')
    *cp = 0;
  cp = model + strlen(model);
  while (*--cp == ' ')
    *cp = 0;
#endif
  i = strbuflen(make); /* Remove make from model */
  if (!strncasecmp(model, make, i) && model[i++] == ' ')
    memmove(model, model + i, 64 - i);
  if (!strncmp(model, "FinePix ", 8))
    memmove(model, model + 8,strlen(model)-7);
  if (!strncmp(model, "Digital Camera ", 15))
   memmove(model, model + 15,strlen(model)-14);
  desc[511] = artist[63] = make[63] = model[63] = model2[63] = 0;
  if (!is_raw)
    goto notraw;

  if (!height)
    height = raw_height;
  if (!width)
    width = raw_width;
  if (height == 2624 && width == 3936) /* Pentax K10D and Samsung GX10 */
  {
    height = 2616;
    width = 3896;
  }
  if (height == 3136 && width == 4864) /* Pentax K20D and Samsung GX20 */
  {
    height = 3124;
    width = 4688;
    filters = 0x16161616;
  }
  if (width == 4352 && (!strcmp(model, "K-r") || !strcmp(model, "K-x")))
  {
    width = 4309;
    filters = 0x16161616;
  }
  if (width >= 4960 && !strncmp(model, "K-5", 3))
  {
    left_margin = 10;
    width = 4950;
    filters = 0x16161616;
  }
  if (width == 6080 && !strcmp(model, "K-70"))
  {
    height = 4016;
    top_margin = 32;
    width = 6020;
    left_margin = 60;
  }
  if (width == 4736 && !strcmp(model, "K-7"))
  {
    height = 3122;
    width = 4684;
    filters = 0x16161616;
    top_margin = 2;
  }
  if (width == 6080 && !strcmp(model, "K-3 II")) /* moved back */
  {
    left_margin = 4;
    width = 6040;
  }
  if (width == 6112 && !strcmp(model, "KP"))
  {
    /* From DNG, maybe too strict */
    left_margin = 54;
    top_margin = 28;
    width = 6028;
    height = raw_height - top_margin;
  }
  if (width == 6080 && !strcmp(model, "K-3"))
  {
    left_margin = 4;
    width = 6040;
  }
  if (width == 7424 && !strcmp(model, "645D"))
  {
    height = 5502;
    width = 7328;
    filters = 0x61616161;
    top_margin = 29;
    left_margin = 48;
  }
  if (height == 3014 && width == 4096) /* Ricoh GX200 */
    width = 4014;
  if (dng_version)
  {
    if (filters == UINT_MAX)
      filters = 0;
    if (filters)
      is_raw *= tiff_samples;
    else
      colors = tiff_samples;
    switch (tiff_compress)
    {
    case 0: /* Compression not set, assuming uncompressed */
    case 1:
#ifdef LIBRAW_LIBRARY_BUILD
#ifdef USE_DNGSDK
        // Uncompressed float
	if (load_raw != &CLASS float_dng_load_raw_placeholder)
#endif
#endif
      load_raw = &CLASS packed_dng_load_raw;
      break;
    case 7:
      load_raw = &CLASS lossless_dng_load_raw;
      break;
#ifdef LIBRAW_LIBRARY_BUILD
    case 8:
      load_raw = &CLASS deflate_dng_load_raw;
      break;
#endif
    case 34892:
      load_raw = &CLASS lossy_dng_load_raw;
      break;
    default:
      load_raw = 0;
    }
    if (!strncmp(make, "Canon", 5) && unique_id)
    {
      for (i = 0; i < sizeof unique / sizeof *unique; i++)
        if (unique_id == 0x80000000 + unique[i].id)
        {
          strcpy(model, unique[i].t_model);
          break;
        }
    }
    if (!strncasecmp(make, "Sony", 4) && unique_id)
    {
      for (i = 0; i < sizeof sonique / sizeof *sonique; i++)
        if (unique_id == sonique[i].id)
        {
          strcpy(model, sonique[i].t_model);
          break;
        }
    }
    goto dng_skip;
  }
  if (!strncmp(make, "Canon", 5) && !fsize && tiff_bps != 15)
  {
    if (!load_raw)
      load_raw = &CLASS lossless_jpeg_load_raw;
    for (i = 0; i < sizeof canon / sizeof *canon; i++)
      if (raw_width == canon[i][0] && raw_height == canon[i][1])
      {
        width = raw_width - (left_margin = canon[i][2]);
        height = raw_height - (top_margin = canon[i][3]);
        width -= canon[i][4];
        height -= canon[i][5];
        mask[0][1] = canon[i][6];
        mask[0][3] = -canon[i][7];
        mask[1][1] = canon[i][8];
        mask[1][3] = -canon[i][9];
        if (canon[i][10])
          filters = canon[i][10] * 0x01010101U;
      }
    if ((unique_id | 0x20000) == 0x2720000)
    {
      left_margin = 8;
      top_margin = 16;
    }
  }
  if (!strncmp(make, "Canon", 5) && unique_id)
  {
    for (i = 0; i < sizeof unique / sizeof *unique; i++)
      if (unique_id == 0x80000000 + unique[i].id)
      {
        adobe_coeff("Canon", unique[i].t_model);
        strcpy(model, unique[i].t_model);
      }
  }

  if(!strncmp(make, "Canon", 5) && !tiff_flip && imgdata.makernotes.canon.MakernotesFlip && !dng_version )
     tiff_flip =  imgdata.makernotes.canon.MakernotesFlip ;

  if (!strncasecmp(make, "Sony", 4) && unique_id)
  {
    for (i = 0; i < sizeof sonique / sizeof *sonique; i++)
      if (unique_id == sonique[i].id)
      {
        adobe_coeff("Sony", sonique[i].t_model);
        strcpy(model, sonique[i].t_model);
      }
  }

  if (!strncmp(make, "Panasonic", 9) ||
      !strncasecmp(make, "Leica", 5)) {
    for (i=0; i < sizeof panalias / sizeof *panalias; i++)
      if (panalias[i][0] == '@') orig = panalias[i]+1;
      else if (!strcmp(model,panalias[i]))
        adobe_coeff ("Panasonic", orig);
  }

  if (!strncmp(make, "Nikon", 5))
  {
    if (!load_raw)
      load_raw = &CLASS packed_load_raw;
    if (model[0] == 'E')
      load_flags |= !data_offset << 2 | 2;
  }

  /* Set parameters based on camera name (for non-DNG files). */

  if (!strcmp(model, "KAI-0340") && find_green(16, 16, 3840, 5120) < 25)
  {
    height = 480;
    top_margin = filters = 0;
    strcpy(model, "C603");
  }
#ifndef LIBRAW_LIBRARY_BUILD
  if (!strcmp(make, "Sony") && raw_width > 3888 && !black && !cblack[0])
    black = 128 << (tiff_bps - 12);
#else
  /* Always 512 for arw2_load_raw */
  if (!strcmp(make, "Sony") && raw_width > 3888 && !black && !cblack[0])
    black = (load_raw == &LibRaw::sony_arw2_load_raw) ? 512 : (128 << (tiff_bps - 12));
#endif

  if (is_foveon)
  {
    if (height * 2 < width)
      pixel_aspect = 0.5;
    if (height > width)
      pixel_aspect = 2;
    filters = 0;
  }
  else if (!strncmp(make, "Pentax", 6) && !strncmp(model, "K-1", 3))
  {
    top_margin = 18;
    height = raw_height - top_margin;
    if (raw_width == 7392)
    {
      left_margin = 6;
      width = 7376;
    }
  }
  else if (!strncmp(make, "Canon", 5) && tiff_bps == 15)
  {
    switch (width)
    {
    case 3344:
      width -= 66;
    case 3872:
      width -= 6;
    }
    if (height > width)
    {
      SWAP(height, width);
      SWAP(raw_height, raw_width);
    }
    if (width == 7200 && height == 3888)
    {
      raw_width = width = 6480;
      raw_height = height = 4320;
    }
    filters = 0;
    tiff_samples = colors = 3;
    load_raw = &CLASS canon_sraw_load_raw;
  }
  else if (!strcmp(model, "PowerShot 600"))
  {
    height = 613;
    width = 854;
    raw_width = 896;
    colors = 4;
    filters = 0xe1e4e1e4;
    load_raw = &CLASS canon_600_load_raw;
  }
  else if (!strcmp(model, "PowerShot A5") || !strcmp(model, "PowerShot A5 Zoom"))
  {
    height = 773;
    width = 960;
    raw_width = 992;
    pixel_aspect = 256 / 235.0;
    filters = 0x1e4e1e4e;
    goto canon_a5;
  }
  else if (!strcmp(model, "PowerShot A50"))
  {
    height = 968;
    width = 1290;
    raw_width = 1320;
    filters = 0x1b4e4b1e;
    goto canon_a5;
  }
  else if (!strcmp(model, "PowerShot Pro70"))
  {
    height = 1024;
    width = 1552;
    filters = 0x1e4b4e1b;
  canon_a5:
    colors = 4;
    tiff_bps = 10;
    load_raw = &CLASS packed_load_raw;
    load_flags = 40;
  }
  else if (!strcmp(model, "PowerShot Pro90 IS") || !strcmp(model, "PowerShot G1"))
  {
    colors = 4;
    filters = 0xb4b4b4b4;
  }
  else if (!strcmp(model, "PowerShot A610"))
  {
    if (canon_s2is())
      strcpy(model + 10, "S2 IS");
  }
  else if (!strcmp(model, "PowerShot SX220 HS"))
  {
    mask[1][3] = -4;
    top_margin = 16;
    left_margin = 92;
  }
  else if (!strcmp(model, "PowerShot S120"))
  {
    raw_width = 4192;
    raw_height = 3062;
    width = 4022;
    height = 3016;
    mask[0][0] = top_margin = 31;
    mask[0][2] = top_margin + height;
    left_margin = 120;
    mask[0][1] = 23;
    mask[0][3] = 72;
  }
  else if (!strcmp(model, "PowerShot G16"))
  {
    mask[0][0] = 0;
    mask[0][2] = 80;
    mask[0][1] = 0;
    mask[0][3] = 16;
    top_margin = 29;
    left_margin = 120;
    width = raw_width - left_margin - 48;
    height = raw_height - top_margin - 14;
  }
  else if (!strcmp(model, "PowerShot SX50 HS"))
  {
    top_margin = 17;
  }
  else if (!strcmp(model, "EOS D2000C"))
  {
    filters = 0x61616161;
    if (!black)
      black = curve[200];
  }
  else if (!strcmp(model, "D1"))
  {
    cam_mul[0] *= 256 / 527.0;
    cam_mul[2] *= 256 / 317.0;
  }
  else if (!strcmp(model, "D1X"))
  {
    width -= 4;
    pixel_aspect = 0.5;
  }
  else if (!strcmp(model, "D40X") || !strcmp(model, "D60") || !strcmp(model, "D80") || !strcmp(model, "D3000"))
  {
    height -= 3;
    width -= 4;
  }
  else if (!strcmp(model, "D3") || !strcmp(model, "D3S") || !strcmp(model, "D700"))
  {
    width -= 4;
    left_margin = 2;
  }
  else if (!strcmp(model, "D3100"))
  {
    width -= 28;
    left_margin = 6;
  }
  else if (!strcmp(model, "D5000") || !strcmp(model, "D90"))
  {
    width -= 42;
  }
  else if (!strcmp(model, "D5100") || !strcmp(model, "D7000") || !strcmp(model, "COOLPIX A"))
  {
    width -= 44;
  }
  else if (!strcmp(model, "D3200") || !strcmp(model, "D600") || !strcmp(model, "D610") || !strncmp(model, "D800", 4))
  {
    width -= 46;
  }
  else if (!strcmp(model, "D4") || !strcmp(model, "Df"))
  {
    width -= 52;
    left_margin = 2;
  }
  else if (!strcmp(model, "D500"))
  {
    // Empty - to avoid width-1 below
  }
  else if (!strncmp(model, "D40", 3) || !strncmp(model, "D50", 3) || !strncmp(model, "D70", 3))
  {
    width--;
  }
  else if (!strcmp(model, "D100"))
  {
    if (load_flags)
      raw_width = (width += 3) + 3;
  }
  else if (!strcmp(model, "D200"))
  {
    left_margin = 1;
    width -= 4;
    filters = 0x94949494;
  }
  else if (!strncmp(model, "D2H", 3))
  {
    left_margin = 6;
    width -= 14;
  }
  else if (!strncmp(model, "D2X", 3))
  {
    if (width == 3264)
      width -= 32;
    else
      width -= 8;
  }
  else if (!strncmp(model, "D300", 4))
  {
    width -= 32;
  }
  else if (!strncmp(make, "Nikon", 5) && raw_width == 4032)
  {
    if (!strcmp(model, "COOLPIX P7700"))
    {
      adobe_coeff("Nikon", "COOLPIX P7700");
      maximum = 65504;
      load_flags = 0;
    }
    else if (!strcmp(model, "COOLPIX P7800"))
    {
      adobe_coeff("Nikon", "COOLPIX P7800");
      maximum = 65504;
      load_flags = 0;
    }
    else if (!strcmp(model, "COOLPIX P340"))
      load_flags = 0;
  }
  else if (!strncmp(model, "COOLPIX P", 9) && raw_width != 4032)
  {
    load_flags = 24;
    filters = 0x94949494;
/* the following 'if' is most probably obsolete, because we now read black level from metadata */
    if ((model[9] == '7') &&  /* P7000, P7100 */
        ((iso_speed >= 400) || (iso_speed == 0)) &&
        !strstr(software, "V1.2")) /* v. 1.2 seen for P7000 only */
      black = 255;
  }
  else if (!strncmp(model, "COOLPIX B700", 12))
  {
    load_flags = 24;
  }
  else if (!strncmp(model, "1 ", 2))
  {
    height -= 2;
  }
  else if (fsize == 1581060)
  {
    simple_coeff(3);
    pre_mul[0] = 1.2085;
    pre_mul[1] = 1.0943;
    pre_mul[3] = 1.1103;
  }
  else if (fsize == 3178560)
  {
    cam_mul[0] *= 4;
    cam_mul[2] *= 4;
  }
  else if (fsize == 4771840)
  {
    if (!timestamp && nikon_e995())
      strcpy(model, "E995");
    if (strcmp(model, "E995"))
    {
      filters = 0xb4b4b4b4;
      simple_coeff(3);
      pre_mul[0] = 1.196;
      pre_mul[1] = 1.246;
      pre_mul[2] = 1.018;
    }
  }
  else if (fsize == 2940928)
  {
    if (!timestamp && !nikon_e2100())
      strcpy(model, "E2500");
    if (!strcmp(model, "E2500"))
    {
      height -= 2;
      load_flags = 6;
      colors = 4;
      filters = 0x4b4b4b4b;
    }
  }
  else if (fsize == 4775936)
  {
    if (!timestamp)
      nikon_3700();
    if (model[0] == 'E' && atoi(model + 1) < 3700)
      filters = 0x49494949;
    if (!strcmp(model, "Optio 33WR"))
    {
      flip = 1;
      filters = 0x16161616;
    }
    if (make[0] == 'O')
    {
      i = find_green(12, 32, 1188864, 3576832);
      c = find_green(12, 32, 2383920, 2387016);
      if (abs(i) < abs(c))
      {
        SWAP(i, c);
        load_flags = 24;
      }
      if (i < 0)
        filters = 0x61616161;
    }
  }
  else if (fsize == 5869568)
  {
    if (!timestamp && minolta_z2())
    {
      strcpy(make, "Minolta");
      strcpy(model, "DiMAGE Z2");
    }
    load_flags = 6 + 24 * (make[0] == 'M');
  }
  else if (fsize == 6291456)
  {
    fseek(ifp, 0x300000, SEEK_SET);
    if ((order = guess_byte_order(0x10000)) == 0x4d4d)
    {
      height -= (top_margin = 16);
      width -= (left_margin = 28);
      maximum = 0xf5c0;
      strcpy(make, "ISG");
      model[0] = 0;
    }
  }
  else if (!strncmp(make, "Fujifilm", 8))
  {
    if (!strcmp(model + 7, "S2Pro"))
    {
      strcpy(model, "S2Pro");
      height = 2144;
      width = 2880;
      flip = 6;
    }
    else if (load_raw != &CLASS packed_load_raw && strncmp(model, "X-", 2) && filters >=1000) // Bayer and not an X-model
      maximum = (is_raw == 2 && shot_select) ? 0x2f00 : 0x3e00;
    top_margin = (raw_height - height) >> 2 << 1;
    left_margin = (raw_width - width) >> 2 << 1;

    if (!strcmp(model, "X-T3") ||
        !strcmp(model, "X-T30")) {
      top_margin = 0;
      if (((height == 1008)  ||  /* mechanical shutter, high speed mode */
           (height == 4140)) &&  /* any shutter, single shot mode */
          (width == 6276)) {
        top_margin = 6;
        height = 4174;
        left_margin = 0;
        width = 6251;
      } else if (height == 3350) { /* electronic shutter, high speed mode (1.25x crop) */
        height = raw_height;
        left_margin = 624;
        width = 5004;
      }
/*
      width = raw_width;
      height = raw_height;
      top_margin = 0;
      left_margin = 0;
*/
    }

    if (width == 2848 || width == 3664)
      filters = 0x16161616;
    if (width == 4032 || width == 4952)
      left_margin = 0;
    if (width == 3328 && (width -= 66))
      left_margin = 34;
    if (width == 4936)
      left_margin = 4;
    if (width == 6032)
      left_margin = 0;

  if (!strcmp(model, "DBP for GX680") ||
    !strcmp(model, "DX-2000")) {
/*
7712 2752 -> 5504 3856
*/

/*
  width = 688;
  height = 30848;
  raw_width = 688;
  raw_height = 30848;
*/

  raw_width = 5504;
  raw_height = 3856;
  left_margin = 32;
  top_margin = 8;
  width = raw_width - left_margin -32;
  height = raw_height - top_margin - 8;

  load_raw = &CLASS unpacked_load_raw_FujiDBP;
//  maximum = 0x0fff;
  filters = 0x16161616;
  load_flags = 0;
  flip = 6;
}

    if (!strcmp(model, "HS50EXR") || !strcmp(model, "F900EXR"))
    {
      width += 2;
      left_margin = 0;
      filters = 0x16161616;
    }
    if (!strncmp(model, "GFX 50",6))
    {
      left_margin = 0;
      top_margin = 0;
    }
    if (!strcmp(model, "S5500"))
    {
      height -= (top_margin = 6);
    }
    if (fuji_layout)
      raw_width *= is_raw;
    if (filters == 9)
      FORC(36)((char *)xtrans)[c] = xtrans_abs[(c / 6 + top_margin) % 6][(c + left_margin) % 6];
  }
  else if (!strcmp(model, "KD-400Z"))
  {
    height = 1712;
    width = 2312;
    raw_width = 2336;
    goto konica_400z;
  }
  else if (!strcmp(model, "KD-510Z"))
  {
    goto konica_510z;
  }
  else if (!strncasecmp(make, "Minolta", 7))
  {
    if (!load_raw && (maximum = 0xfff))
      load_raw = &CLASS unpacked_load_raw;
    if (!strncmp(model, "DiMAGE A", 8))
    {
      if (!strcmp(model, "DiMAGE A200"))
        filters = 0x49494949;
      tiff_bps = 12;
      load_raw = &CLASS packed_load_raw;
    }
    else if (!strncmp(model, "ALPHA", 5) || !strncmp(model, "DYNAX", 5) || !strncmp(model, "MAXXUM", 6))
    {
      sprintf(model + 20, "DYNAX %-10s", model + 6 + (model[0] == 'M'));
      adobe_coeff(make, model + 20);
      load_raw = &CLASS packed_load_raw;
    }
    else if (!strncmp(model, "DiMAGE G", 8))
    {
      if (model[8] == '4')
      {
        height = 1716;
        width = 2304;
      }
      else if (model[8] == '5')
      {
      konica_510z:
        height = 1956;
        width = 2607;
        raw_width = 2624;
      }
      else if (model[8] == '6')
      {
        height = 2136;
        width = 2848;
      }
      data_offset += 14;
      filters = 0x61616161;
    konica_400z:
      load_raw = &CLASS unpacked_load_raw;
      maximum = 0x3df;
      order = 0x4d4d;
    }
  }
  else if (!strcmp(model, "*ist D"))
  {
    load_raw = &CLASS unpacked_load_raw;
    data_error = -1;
  }
  else if (!strcmp(model, "*ist DS"))
  {
    height -= 2;
  }
  else if (!strncmp(make, "Samsung", 7) && raw_width == 4704)
  {
    height -= top_margin = 8;
    width -= 2 * (left_margin = 8);
    load_flags = 32;
  }
  else if (!strncmp(make, "Samsung", 7) && !strcmp(model, "NX3000"))
  {
    top_margin = 38;
    left_margin = 92;
    width = 5456;
    height = 3634;
    filters = 0x61616161;
    colors = 3;
  }
  else if (!strncmp(make, "Samsung", 7) && raw_height == 3714)
  {
    height -= top_margin = 18;
    left_margin = raw_width - (width = 5536);
    if (raw_width != 5600)
      left_margin = top_margin = 0;
    filters = 0x61616161;
    colors = 3;
  }
  else if (!strncmp(make, "Samsung", 7) && raw_width == 5632)
  {
    order = 0x4949;
    height = 3694;
    top_margin = 2;
    width = 5574 - (left_margin = 32 + tiff_bps);
    if (tiff_bps == 12)
      load_flags = 80;
  }
  else if (!strncmp(make, "Samsung", 7) && raw_width == 5664)
  {
    height -= top_margin = 17;
    left_margin = 96;
    width = 5544;
    filters = 0x49494949;
  }
  else if (!strncmp(make, "Samsung", 7) && raw_width == 6496)
  {
    filters = 0x61616161;
#ifdef LIBRAW_LIBRARY_BUILD
    if (!black && !cblack[0] && !cblack[1] && !cblack[2] && !cblack[3])
#endif
      black = 1 << (tiff_bps - 7);
  }
  else if (!strcmp(model, "EX1"))
  {
    order = 0x4949;
    height -= 20;
    top_margin = 2;
    if ((width -= 6) > 3682)
    {
      height -= 10;
      width -= 46;
      top_margin = 8;
    }
  }
  else if (!strcmp(model, "WB2000"))
  {
    order = 0x4949;
    height -= 3;
    top_margin = 2;
    if ((width -= 10) > 3718)
    {
      height -= 28;
      width -= 56;
      top_margin = 8;
    }
  }
  else if (strstr(model, "WB550"))
  {
    strcpy(model, "WB550");
  }
  else if (!strcmp(model, "EX2F"))
  {
    height = 3030;
    width = 4040;
    top_margin = 15;
    left_margin = 24;
    order = 0x4949;
    filters = 0x49494949;
    load_raw = &CLASS unpacked_load_raw;
  }
  else if (!strcmp(model, "STV680 VGA"))
  {
    black = 16;
  }
  else if (!strcmp(model, "N95"))
  {
    height = raw_height - (top_margin = 2);
  }
  else if (!strcmp(model, "640x480"))
  {
    gamma_curve(0.45, 4.5, 1, 255);
  }
  else if (!strncmp(make, "Hasselblad", 10))
  {
    if (load_raw == &CLASS lossless_jpeg_load_raw)
      load_raw = &CLASS hasselblad_load_raw;
    if (raw_width == 7262)
    {
      height = 5444;
      width = 7248;
      top_margin = 4;
      left_margin = 7;
      filters = 0x61616161;
      if (!strncasecmp(model, "H3D", 3))
      {
        adobe_coeff("Hasselblad", "H3DII-39");
        strcpy(model, "H3DII-39");
      }
    }
    else if (raw_width == 12000) // H6D 100c, A6D 100c
    {
      left_margin = 64;
      width = 11608;
      top_margin = 108;
      height = raw_height - top_margin;
      adobe_coeff("Hasselblad", "H6D-100c");
    }
    else if (raw_width == 7410 || raw_width == 8282)
    {
      height -= 84;
      width -= 82;
      top_margin = 4;
      left_margin = 41;
      filters = 0x61616161;
      adobe_coeff("Hasselblad", "H4D-40");
      strcpy(model, "H4D-40");
    }
    else if (raw_width == 8384) // X1D
    {
      top_margin = 96;
      height -= 96;
      left_margin = 48;
      width -= 106;
      adobe_coeff("Hasselblad", "X1D");
      maximum = 0xffff;
      tiff_bps = 16;
    }
    else if (raw_width == 9044)
    {
      if (black > 500)
      {
        top_margin = 12;
        left_margin = 44;
        width = 8956;
        height = 6708;
        memset(cblack, 0, sizeof(cblack));
        adobe_coeff("Hasselblad", "H4D-60");
        strcpy(model, "H4D-60");
        black = 512;
      }
      else
      {
        height = 6716;
        width = 8964;
        top_margin = 8;
        left_margin = 40;
        black += load_flags = 256;
        maximum = 0x8101;
        strcpy(model, "H3DII-60");
      }
    }
    else if (raw_width == 4090)
    {
      strcpy(model, "V96C");
      height -= (top_margin = 6);
      width -= (left_margin = 3) + 7;
      filters = 0x61616161;
    }
    else if (raw_width == 8282 && raw_height == 6240)
    {
      if (!strncasecmp(model, "H5D", 3))
      {
        /* H5D 50*/
        left_margin = 54;
        top_margin = 16;
        width = 8176;
        height = 6132;
        black = 256;
        strcpy(model, "H5D-50");
      }
      else if (!strncasecmp(model, "H3D", 3))
      {
        black = 0;
        left_margin = 54;
        top_margin = 16;
        width = 8176;
        height = 6132;
        memset(cblack, 0, sizeof(cblack));
        adobe_coeff("Hasselblad", "H3D-50");
        strcpy(model, "H3D-50");
      }
    }
    else if (raw_width == 8374 && raw_height == 6304)
    {
      /* H5D 50c*/
      left_margin = 52;
      top_margin = 100;
      width = 8272;
      height = 6200;
      black = 256;
      strcpy(model, "H5D-50c");
    }
    if (tiff_samples > 1)
    {
      is_raw = tiff_samples + 1;
      if (!shot_select && !half_size)
        filters = 0;
    }
  }
  else if (!strncmp(make, "Sinar", 5))
  {
    if (!load_raw)
      load_raw = &CLASS unpacked_load_raw;
    if (is_raw > 1 && !shot_select)
      filters = 0;
    maximum = 0x3fff;
  }
  else if(load_raw == &LibRaw::sinar_4shot_load_raw)
  {
    if (is_raw > 1 && !shot_select)
      filters = 0;
  }
  else if (!strncmp(make, "Leaf", 4))
  {
    maximum = 0x3fff;
    fseek(ifp, data_offset, SEEK_SET);
    if (ljpeg_start(&jh, 1) && jh.bits == 15)
      maximum = 0x1fff;
    if (tiff_samples > 1)
      filters = 0;
    if (tiff_samples > 1 || tile_length < raw_height)
    {
      load_raw = &CLASS leaf_hdr_load_raw;
      raw_width = tile_width;
    }
    if ((width | height) == 2048)
    {
      if (tiff_samples == 1)
      {
        filters = 1;
        strcpy(cdesc, "RBTG");
        strcpy(model, "CatchLight");
        top_margin = 8;
        left_margin = 18;
        height = 2032;
        width = 2016;
      }
      else
      {
        strcpy(model, "DCB2");
        top_margin = 10;
        left_margin = 16;
        height = 2028;
        width = 2022;
      }
    }
    else if (width + height == 3144 + 2060)
    {
      if (!model[0])
        strcpy(model, "Cantare");
      if (width > height)
      {
        top_margin = 6;
        left_margin = 32;
        height = 2048;
        width = 3072;
        filters = 0x61616161;
      }
      else
      {
        left_margin = 6;
        top_margin = 32;
        width = 2048;
        height = 3072;
        filters = 0x16161616;
      }
      if (!cam_mul[0] || model[0] == 'V')
        filters = 0;
      else
        is_raw = tiff_samples;
    }
    else if (width == 2116)
    {
      strcpy(model, "Valeo 6");
      height -= 2 * (top_margin = 30);
      width -= 2 * (left_margin = 55);
      filters = 0x49494949;
    }
    else if (width == 3171)
    {
      strcpy(model, "Valeo 6");
      height -= 2 * (top_margin = 24);
      width -= 2 * (left_margin = 24);
      filters = 0x16161616;
    }
  }
  else if (!strncmp(make, "Leica", 5) || !strncmp(make, "Panasonic", 9) || !strncasecmp(make, "YUNEEC", 6))
  {

    if (raw_width > 0 && ((flen - data_offset) / (raw_width * 8 / 7) == raw_height))
      load_raw = &CLASS panasonic_load_raw;
    if (!load_raw)
    {
      load_raw = &CLASS unpacked_load_raw;
      load_flags = 4;
    }
    zero_is_bad = 1;
    if ((height += 12) > raw_height)
      height = raw_height;
    for (i = 0; i < sizeof pana / sizeof *pana; i++)
      if (raw_width == pana[i][0] && raw_height == pana[i][1])
      {
        left_margin = pana[i][2];
        top_margin = pana[i][3];
        width += pana[i][4];
        height += pana[i][5];
      }
    filters = 0x01010101U * (uchar) "\x94\x61\x49\x16"[((filters - 1) ^ (left_margin & 1) ^ (top_margin << 1)) & 3];
  }
  else if (!strcmp(model, "C770UZ"))
  {
    height = 1718;
    width = 2304;
    filters = 0x16161616;
    load_raw = &CLASS packed_load_raw;
    load_flags = 30;
  }
  else if (!strncmp(make, "Olympus", 7) ||
           (!strncasecmp(make, "CLAUSS", 6) && !strncasecmp(model, "piX 5oo", 7)))
  {
    height += height & 1;
    if (exif_cfa)
      filters = exif_cfa;
    if (width == 4100)
      width -= 4;
    if (width == 4080)
      width -= 24;
    if (width == 10400)
      width -= 12;
    if (width == 9280)
    {
      width -= 6;
      height -= 6;
    }
    if (load_raw == &CLASS unpacked_load_raw)
      load_flags = 4;
    tiff_bps = 12;
    if (!strcmp(model, "E-300") || !strcmp(model, "E-500"))
    {
      width -= 20;
      if (load_raw == &CLASS unpacked_load_raw)
      {
        maximum = 0xfc3;
        memset(cblack, 0, sizeof cblack);
      }
    }
    else if (!strcmp(model, "STYLUS1"))
    {
      width -= 14;
      maximum = 0xfff;
    }
    else if (!strcmp(model, "E-330"))
    {
      width -= 30;
      if (load_raw == &CLASS unpacked_load_raw)
        maximum = 0xf79;
    }
    else if (!strcmp(model, "SP550UZ"))
    {
      thumb_length = flen - (thumb_offset = 0xa39800);
      thumb_height = 480;
      thumb_width = 640;
    }
    else if (!strcmp(model, "TG-4"))
    {
      width -= 16;
    }
    else if (!strcmp(model, "TG-5"))
    {
      width -= 26;
    }
  }
  else if (!strcmp(model, "N Digital"))
  {
    height = 2047;
    width = 3072;
    filters = 0x61616161;
    data_offset = 0x1a00;
    load_raw = &CLASS packed_load_raw;
  }
  else if (!strcmp(model, "DSC-F828"))
  {
    width = 3288;
    left_margin = 5;
    mask[1][3] = -17;
    data_offset = 862144;
    load_raw = &CLASS sony_load_raw;
    filters = 0x9c9c9c9c;
    colors = 4;
    strcpy(cdesc, "RGBE");
  }
  else if (!strcmp(model, "DSC-V3"))
  {
    width = 3109;
    left_margin = 59;
    mask[0][1] = 9;
    data_offset = 787392;
    load_raw = &CLASS sony_load_raw;
  }
  else if (!strncmp(make, "Sony", 4) && raw_width == 3984)
  {
    width = 3925;
    order = 0x4d4d;
  }
  else if (!strncmp(make, "Sony", 4) && raw_width == 4288)
  {
    width -= 32;
  }
  else if (!strcmp(make, "Sony") && raw_width == 4600)
  {
    if (!strcmp(model, "DSLR-A350"))
      height -= 4;
    black = 0;
  }
  else if (!strncmp(make, "Sony", 4) && raw_width == 4928)
  {
    if (height < 3280)
      width -= 8;
  }
  else if (!strncmp(make, "Sony", 4) && raw_width == 5504)
  { // ILCE-3000//5000
    width -= height > 3664 ? 8 : 32;
  }
  else if (!strncmp(make, "Sony", 4) && raw_width == 6048)
  {
    width -= 24;
    if (strstr(model, "RX1") || strstr(model, "A99"))
      width -= 6;
  }
  else if (!strncmp(make, "Sony", 4) && raw_width == 7392)
  {
    width -= 30;
  }
  else if (!strncmp(make, "Sony", 4) && raw_width == 8000)
  {
    width -= 32;
  }
  else if (!strcmp(model, "DSLR-A100"))
  {
    if (width == 3880)
    {
      height--;
      width = ++raw_width;
    }
    else
    {
      height -= 4;
      width -= 4;
      order = 0x4d4d;
      load_flags = 2;
    }
    filters = 0x61616161;
  }
  else if (!strcmp(model, "PIXL"))
  {
    height -= top_margin = 4;
    width -= left_margin = 32;
    gamma_curve(0, 7, 1, 255);
  }
  else if (!strcmp(model, "C603") || !strcmp(model, "C330") || !strcmp(model, "12MP"))
  {
    order = 0x4949;
    if (filters && data_offset)
    {
      fseek(ifp, data_offset < 4096 ? 168 : 5252, SEEK_SET);
      read_shorts(curve, 256);
    }
    else
      gamma_curve(0, 3.875, 1, 255);
    load_raw = filters ? &CLASS eight_bit_load_raw
                       : strcmp(model, "C330") ? &CLASS kodak_c603_load_raw : &CLASS kodak_c330_load_raw;
    load_flags = tiff_bps > 16;
    tiff_bps = 8;
  }
  else if (!strncasecmp(model, "EasyShare", 9))
  {
    data_offset = data_offset < 0x15000 ? 0x15000 : 0x17000;
    load_raw = &CLASS packed_load_raw;
  }
  else if (!strncasecmp(make, "Kodak", 5))
  {
    if (filters == UINT_MAX)
      filters = 0x61616161;
    if (!strncmp(model, "NC2000", 6) || !strncmp(model, "EOSDCS", 6) || !strncmp(model, "DCS4", 4))
    {
      width -= 4;
      left_margin = 2;
      if (model[6] == ' ')
        model[6] = 0;
      if (!strcmp(model, "DCS460A"))
        goto bw;
    }
    else if (!strcmp(model, "DCS660M"))
    {
      black = 214;
      goto bw;
    }
    else if (!strcmp(model, "DCS760M"))
    {
    bw:
      colors = 1;
      filters = 0;
    }
    if (!strcmp(model + 4, "20X"))
      strcpy(cdesc, "MYCY");
    if (strstr(model, "DC25"))
    {
      strcpy(model, "DC25");
      data_offset = 15424;
    }
    if (!strncmp(model, "DC2", 3))
    {
      raw_height = 2 + (height = 242);
      if (!strncmp(model, "DC290", 5))
        iso_speed = 100;
      if (!strncmp(model, "DC280", 5))
        iso_speed = 70;
      if (flen < 100000)
      {
        raw_width = 256;
        width = 249;
        pixel_aspect = (4.0 * height) / (3.0 * width);
      }
      else
      {
        raw_width = 512;
        width = 501;
        pixel_aspect = (493.0 * height) / (373.0 * width);
      }
      top_margin = left_margin = 1;
      colors = 4;
      filters = 0x8d8d8d8d;
      simple_coeff(1);
      pre_mul[1] = 1.179;
      pre_mul[2] = 1.209;
      pre_mul[3] = 1.036;
      load_raw = &CLASS eight_bit_load_raw;
    }
    else if (!strcmp(model, "40"))
    {
      strcpy(model, "DC40");
      height = 512;
      width = 768;
      data_offset = 1152;
      load_raw = &CLASS kodak_radc_load_raw;
      tiff_bps = 12;
      FORC4 cam_mul[c] = 1.0f;
    }
    else if (strstr(model, "DC50"))
    {
      strcpy(model, "DC50");
      height = 512;
      width = 768;
      iso_speed = 84;
      data_offset = 19712;
      load_raw = &CLASS kodak_radc_load_raw;
      FORC4 cam_mul[c] = 1.0f;
    }
    else if (strstr(model, "DC120"))
    {
      strcpy(model, "DC120");
      raw_height = height = 976;
      raw_width = width = 848;
      iso_speed = 160;
      pixel_aspect = height / 0.75 / width;
      load_raw = tiff_compress == 7 ? &CLASS kodak_jpeg_load_raw : &CLASS kodak_dc120_load_raw;
    }
    else if (!strcmp(model, "DCS200"))
    {
      thumb_height = 128;
      thumb_width = 192;
      thumb_offset = 6144;
      thumb_misc = 360;
      iso_speed = 140;
      write_thumb = &CLASS layer_thumb;
      black = 17;
    }
  }
  else if (!strcmp(model, "Fotoman Pixtura"))
  {
    height = 512;
    width = 768;
    data_offset = 3632;
    load_raw = &CLASS kodak_radc_load_raw;
    filters = 0x61616161;
    simple_coeff(2);
  }
  else if (!strncmp(model, "QuickTake", 9))
  {
    if (head[5])
      strcpy(model + 10, "200");
    fseek(ifp, 544, SEEK_SET);
    height = get2();
    width = get2();
    data_offset = (get4(), get2()) == 30 ? 738 : 736;
    if (height > width)
    {
      SWAP(height, width);
      fseek(ifp, data_offset - 6, SEEK_SET);
      flip = ~get2() & 3 ? 5 : 6;
    }
    filters = 0x61616161;
  }
  else if (!strncmp(make, "Rollei", 6) && !load_raw)
  {
    switch (raw_width)
    {
    case 1316:
      height = 1030;
      width = 1300;
      top_margin = 1;
      left_margin = 6;
      break;
    case 2568:
      height = 1960;
      width = 2560;
      top_margin = 2;
      left_margin = 8;
    }
    filters = 0x16161616;
    load_raw = &CLASS rollei_load_raw;
  }
  else if (!strcmp(model, "GRAS-50S5C"))
  {
    height = 2048;
    width = 2440;
    load_raw = &CLASS unpacked_load_raw;
    data_offset = 0;
    filters = 0x49494949;
    order = 0x4949;
    maximum = 0xfffC;
  }
  else if (!strcmp(model, "BB-500CL"))
  {
    height = 2058;
    width = 2448;
    load_raw = &CLASS unpacked_load_raw;
    data_offset = 0;
    filters = 0x94949494;
    order = 0x4949;
    maximum = 0x3fff;
  }
  else if (!strcmp(model, "BB-500GE"))
  {
    height = 2058;
    width = 2456;
    load_raw = &CLASS unpacked_load_raw;
    data_offset = 0;
    filters = 0x94949494;
    order = 0x4949;
    maximum = 0x3fff;
  }
  else if (!strcmp(model, "SVS625CL"))
  {
    height = 2050;
    width = 2448;
    load_raw = &CLASS unpacked_load_raw;
    data_offset = 0;
    filters = 0x94949494;
    order = 0x4949;
    maximum = 0x0fff;
  }
  /* Early reject for damaged images */
  if (!load_raw || height < 22 || width < 22 ||
#ifdef LIBRAW_LIBRARY_BUILD
    (tiff_bps > 16 && (load_raw != &LibRaw::deflate_dng_load_raw  && load_raw != &LibRaw::float_dng_load_raw_placeholder))
#else
      tiff_bps > 16
#endif
      || tiff_samples > 4 || colors > 4 || colors < 1
      /* alloc in unpack() may be fooled by size adjust */
      || ((int)width + (int)left_margin > 65535) || ((int)height + (int)top_margin > 65535))
  {
    is_raw = 0;
#ifdef LIBRAW_LIBRARY_BUILD
    RUN_CALLBACK(LIBRAW_PROGRESS_IDENTIFY, 1, 2);
#endif
    return;
  }
  if (!model[0])
    sprintf(model, "%dx%d", width, height);
#ifdef LIBRAW_LIBRARY_BUILD
  if (!(imgdata.params.raw_processing_options & LIBRAW_PROCESSING_ZEROFILTERS_FOR_MONOCHROMETIFFS) && (filters == UINT_MAX)) // Default dcraw behaviour
    filters = 0x94949494;
  else if(filters == UINT_MAX)
  {
    if(tiff_nifds > 0 && tiff_samples==1)
    {
       colors = 1;
       filters = 0;
    }
    else
    	filters = 0x94949494;
  }
#else
  if (filters == UINT_MAX)
    filters = 0x94949494;
#endif
  if (thumb_offset && !thumb_height)
  {
    fseek(ifp, thumb_offset, SEEK_SET);
    if (ljpeg_start(&jh, 1))
    {
      thumb_width = jh.wide;
      thumb_height = jh.high;
    }
  }

dng_skip:
#ifdef LIBRAW_LIBRARY_BUILD
  if (dng_version) /* Override black level by DNG tags */
  {
    /* copy DNG data from per-IFD field to color.dng */
    int iifd = 0; // Active IFD we'll show to user.
    for (; iifd < tiff_nifds; iifd++)
      if (tiff_ifd[iifd].offset == data_offset) // found
        break;
    int pifd = -1;
    for (int ii = 0; ii < tiff_nifds; ii++)
      if (tiff_ifd[ii].offset == thumb_offset) // found
      {
        pifd = ii;
        break;
      }

#define CFAROUND(value, filters) filters ? (filters >= 1000 ? ((value + 1) / 2) * 2 : ((value + 5) / 6) * 6) : value

#define IFDCOLORINDEX(ifd, subset, bit)                                                                                \
  (tiff_ifd[ifd].dng_color[subset].parsedfields & bit) ? ifd                                                           \
                                                       : ((tiff_ifd[0].dng_color[subset].parsedfields & bit) ? 0 : -1)

#define IFDLEVELINDEX(ifd, bit)                                                                                        \
  (tiff_ifd[ifd].dng_levels.parsedfields & bit) ? ifd : ((tiff_ifd[0].dng_levels.parsedfields & bit) ? 0 : -1)

#define COPYARR(to, from) memmove(&to, &from, sizeof(from))

    if (iifd < tiff_nifds)
    {
      int sidx;
      // Per field, not per structure
	  if (imgdata.params.raw_processing_options & LIBRAW_PROCESSING_CHECK_DNG_ILLUMINANT)
	  {
		  int illidx[2], cmidx[2],calidx[2], abidx;
		  for(int i = 0; i < 2; i++)
		  {
			  illidx[i] = IFDCOLORINDEX(iifd, i, LIBRAW_DNGFM_ILLUMINANT);
			  cmidx[i] = IFDCOLORINDEX(iifd, i, LIBRAW_DNGFM_COLORMATRIX);
			  calidx[i] = IFDCOLORINDEX(iifd, i, LIBRAW_DNGFM_CALIBRATION);
		  }
		  abidx = IFDLEVELINDEX(iifd, LIBRAW_DNGFM_ANALOGBALANCE);
		  // Data found, all in same ifd, illuminants are inited
		  if (illidx[0] >= 0 && illidx[0] < tiff_nifds && illidx[0] == illidx[1] && illidx[0] == cmidx[0] && illidx[0] == cmidx[1]
			  && tiff_ifd[illidx[0]].dng_color[0].illuminant>0 && tiff_ifd[illidx[0]].dng_color[1].illuminant>0)
		  {
			  sidx = illidx[0]; // => selected IFD
			  double cc[4][4], cm[4][3], cam_xyz[4][3];
			  // CM -> Color Matrix
			  // CC -> Camera calibration
			  for (int j = 0; j < 4; j++)  for (int i = 0; i < 4; i++)  cc[j][i] = i == j;
			  int colidx = -1;

			  // IS D65 here?
			  for(int i = 0; i < 2; i++)
			  {
				  int ill = tiff_ifd[sidx].dng_color[i].illuminant;
				  if (tiff_ifd[sidx].dng_color[i].illuminant == LIBRAW_WBI_D65)
				  {
					  colidx = i; break;
				  }
			  }

			  // Other daylight-type ill
			  if(colidx<0)
				  for(int i = 0; i < 2; i++)
				  {
					  int ill = tiff_ifd[sidx].dng_color[i].illuminant;
					  if (ill == LIBRAW_WBI_Daylight || ill == LIBRAW_WBI_D55 || ill == LIBRAW_WBI_D75 || ill == LIBRAW_WBI_D50 || ill == LIBRAW_WBI_Flash)
					  {
						  colidx = i; break;
					  }
				  }
			  if(colidx>=0) // Selected
			  {
				  // Init camera matrix from DNG
				  FORCC for (int j = 0; j < 3; j++)
					  cm[c][j] = tiff_ifd[sidx].dng_color[colidx].colormatrix[c][j];

				  if(calidx[colidx] == sidx)
				  {
					  for (int i = 0; i < colors; i++)
						  FORCC
						  cc[i][c] = tiff_ifd[sidx].dng_color[colidx].calibration[i][c];
				  }

				  if(abidx == sidx)
					for (int i = 0; i < colors; i++)
						  FORCC cc[i][c] *= tiff_ifd[sidx].dng_levels.analogbalance[i];
				  int j;
				  FORCC for (int i = 0; i < 3; i++) for (cam_xyz[c][i] = j = 0; j < colors; j++) cam_xyz[c][i] +=
					  cc[c][j] * cm[j][i];// add AsShotXY later * xyz[i];
				  cam_xyz_coeff(cmatrix, cam_xyz);
			  }
		  }
	  }

      if (imgdata.params.raw_processing_options & LIBRAW_PROCESSING_USE_DNG_DEFAULT_CROP)
      {
        sidx = IFDLEVELINDEX(iifd, LIBRAW_DNGFM_CROPORIGIN);
        int sidx2 = IFDLEVELINDEX(iifd, LIBRAW_DNGFM_CROPSIZE);
        if (sidx >= 0 && sidx == sidx2 && tiff_ifd[sidx].dng_levels.default_crop[2] > 0 &&
            tiff_ifd[sidx].dng_levels.default_crop[3] > 0)
        {
          int lm = tiff_ifd[sidx].dng_levels.default_crop[0];
          int lmm = CFAROUND(lm, filters);
          int tm = tiff_ifd[sidx].dng_levels.default_crop[1];
          int tmm = CFAROUND(tm, filters);
          int ww = tiff_ifd[sidx].dng_levels.default_crop[2];
          int hh = tiff_ifd[sidx].dng_levels.default_crop[3];
          if (lmm > lm)
            ww -= (lmm - lm);
          if (tmm > tm)
            hh -= (tmm - tm);
          if (left_margin + lm + ww <= raw_width && top_margin + tm + hh <= raw_height)
          {
            left_margin += lmm;
            top_margin += tmm;
            width = ww;
            height = hh;
          }
        }
      }
      if (!(imgdata.color.dng_color[0].parsedfields & LIBRAW_DNGFM_FORWARDMATRIX)) // Not set already (Leica makernotes)
      {
        sidx = IFDCOLORINDEX(iifd, 0, LIBRAW_DNGFM_FORWARDMATRIX);
        if (sidx >= 0)
          COPYARR(imgdata.color.dng_color[0].forwardmatrix, tiff_ifd[sidx].dng_color[0].forwardmatrix);
      }
      if (!(imgdata.color.dng_color[1].parsedfields & LIBRAW_DNGFM_FORWARDMATRIX)) // Not set already (Leica makernotes)
      {
        sidx = IFDCOLORINDEX(iifd, 1, LIBRAW_DNGFM_FORWARDMATRIX);
        if (sidx >= 0)
          COPYARR(imgdata.color.dng_color[1].forwardmatrix, tiff_ifd[sidx].dng_color[1].forwardmatrix);
      }
      for (int ss = 0; ss < 2; ss++)
      {
        sidx = IFDCOLORINDEX(iifd, ss, LIBRAW_DNGFM_COLORMATRIX);
        if (sidx >= 0)
          COPYARR(imgdata.color.dng_color[ss].colormatrix, tiff_ifd[sidx].dng_color[ss].colormatrix);

        sidx = IFDCOLORINDEX(iifd, ss, LIBRAW_DNGFM_CALIBRATION);
        if (sidx >= 0)
          COPYARR(imgdata.color.dng_color[ss].calibration, tiff_ifd[sidx].dng_color[ss].calibration);

        sidx = IFDCOLORINDEX(iifd, ss, LIBRAW_DNGFM_ILLUMINANT);
        if (sidx >= 0)
          imgdata.color.dng_color[ss].illuminant = tiff_ifd[sidx].dng_color[ss].illuminant;
      }
      // Levels
      sidx = IFDLEVELINDEX(iifd, LIBRAW_DNGFM_ANALOGBALANCE);
      if (sidx >= 0)
        COPYARR(imgdata.color.dng_levels.analogbalance, tiff_ifd[sidx].dng_levels.analogbalance);

	  sidx = IFDLEVELINDEX(iifd, LIBRAW_DNGFM_BASELINEEXPOSURE);
	  if (sidx >= 0)
		  imgdata.color.dng_levels.baseline_exposure = tiff_ifd[sidx].dng_levels.baseline_exposure;

	  sidx = IFDLEVELINDEX(iifd, LIBRAW_DNGFM_LINEARRESPONSELIMIT);
	  if (sidx >= 0)
	  {
		  imgdata.color.dng_levels.LinearResponseLimit = tiff_ifd[sidx].dng_levels.LinearResponseLimit;
		  if(imgdata.color.dng_levels.LinearResponseLimit>0.1 && imgdata.color.dng_levels.LinearResponseLimit <= 1.0)
		  {
			  // And approx promote it to linear_max:
			  int bl4 = 0, bl64 = 0;
			  for (int chan = 0; chan < colors && chan < 4; chan++)
				  bl4 +=  cblack[chan];
			  bl4 /= LIM(colors,1,4);

			  if (cblack[4] * cblack[5]>0)
			  {
				  unsigned cnt = 0;
				  for (unsigned c = 0; c < 4096 && c < cblack[4] * cblack[5]; c++)
				  {
					  bl64 += cblack[c + 6];
					  cnt++;
				  }
				  bl64 /= LIM(cnt,1,4096);
			  }
			  int rblack = black + bl4 + bl64;
			  for (int chan = 0; chan < colors && chan < 4; chan++)
				  imgdata.color.linear_max[i] = (maximum-rblack)*imgdata.color.dng_levels.LinearResponseLimit + rblack;
		  }
	  }

      sidx = IFDLEVELINDEX(iifd, LIBRAW_DNGFM_WHITE);
      if (sidx >= 0)
        COPYARR(imgdata.color.dng_levels.dng_whitelevel, tiff_ifd[sidx].dng_levels.dng_whitelevel);
      sidx = IFDLEVELINDEX(iifd, LIBRAW_DNGFM_ASSHOTNEUTRAL);
      if (sidx >= 0)
      {
        COPYARR(imgdata.color.dng_levels.asshotneutral, tiff_ifd[sidx].dng_levels.asshotneutral);
	if (imgdata.color.dng_levels.asshotneutral[0])
        {
	   cam_mul[3] = 0;
	   FORCC
	      if(fabs(imgdata.color.dng_levels.asshotneutral[c])>0.0001)
	         cam_mul[c] = 1 / imgdata.color.dng_levels.asshotneutral[c];
	}
      }
      sidx = IFDLEVELINDEX(iifd, LIBRAW_DNGFM_BLACK);
      if (sidx >= 0)
      {
        imgdata.color.dng_levels.dng_fblack = tiff_ifd[sidx].dng_levels.dng_fblack;
        imgdata.color.dng_levels.dng_black = tiff_ifd[sidx].dng_levels.dng_black;
        COPYARR(imgdata.color.dng_levels.dng_cblack, tiff_ifd[sidx].dng_levels.dng_cblack);
        COPYARR(imgdata.color.dng_levels.dng_fcblack, tiff_ifd[sidx].dng_levels.dng_fcblack);
      }
      if (pifd >= 0)
      {
        sidx = IFDLEVELINDEX(pifd, LIBRAW_DNGFM_PREVIEWCS);
        if (sidx >= 0)
          imgdata.color.dng_levels.preview_colorspace = tiff_ifd[sidx].dng_levels.preview_colorspace;
      }
      sidx = IFDLEVELINDEX(iifd, LIBRAW_DNGFM_OPCODE2);
      if (sidx >= 0)
        meta_offset = tiff_ifd[sidx].opcode2_offset;

      sidx = IFDLEVELINDEX(iifd, LIBRAW_DNGFM_LINTABLE);
      INT64 linoff = -1;
      int linlen = 0;
      if (sidx >= 0)
      {
        linoff = tiff_ifd[sidx].lineartable_offset;
        linlen = tiff_ifd[sidx].lineartable_len;
      }

      if (linoff >= 0 && linlen > 0)
      {
        INT64 pos = ftell(ifp);
        fseek(ifp, linoff, SEEK_SET);
        linear_table(linlen);
        fseek(ifp, pos, SEEK_SET);
      }
      // Need to add curve too
    }
    /* Copy DNG black level to LibRaw's */
    maximum = imgdata.color.dng_levels.dng_whitelevel[0];
    black = imgdata.color.dng_levels.dng_black;
    int ll = LIM(0, (sizeof(cblack) / sizeof(cblack[0])),
                 (sizeof(imgdata.color.dng_levels.dng_cblack) / sizeof(imgdata.color.dng_levels.dng_cblack[0])));
    for (int i = 0; i < ll; i++)
      cblack[i] = imgdata.color.dng_levels.dng_cblack[i];
  }
#endif
  /* Early reject for damaged images */
  if (!load_raw || height < 22 || width < 22 ||
#ifdef LIBRAW_LIBRARY_BUILD
    (tiff_bps > 16 && (load_raw != &LibRaw::deflate_dng_load_raw  && load_raw != &LibRaw::float_dng_load_raw_placeholder))
#else
      tiff_bps > 16
#endif
      || tiff_samples > 4 || colors > 4 || colors < 1)
  {
    is_raw = 0;
#ifdef LIBRAW_LIBRARY_BUILD
    RUN_CALLBACK(LIBRAW_PROGRESS_IDENTIFY, 1, 2);
#endif
    return;
  }
  {
   // Check cam_mul range
   int cmul_ok =1;
   FORCC if(cam_mul[c] <= 0.001f)  cmul_ok = 0;;

   if(cmul_ok)
   {
	  double cmin = cam_mul[0],cmax;
	  double cnorm[4];
	  FORCC	  cmin = MIN(cmin,cam_mul[c]);
	  FORCC	  cnorm[c] = cam_mul[c]/cmin;
	  cmax = cmin = cnorm[0];
	  FORCC
	  {
		  cmin = MIN(cmin,cnorm[c]);
		  cmax = MIN(cmax,cnorm[c]);
	  }
	  if(cmin <= 0.01f || cmax > 100.f)
		  cmul_ok = false;
   }
   if(!cmul_ok)
   {
     if(cam_mul[0]>0) cam_mul[0] = 0;
     cam_mul[3] = 0;
   }
  }
  if ((use_camera_matrix & ((use_camera_wb || dng_version) | 0x2)) && cmatrix[0][0] > 0.125)
  {
    memcpy(rgb_cam, cmatrix, sizeof cmatrix);
    raw_color = 0;
  }

  if (raw_color)
    adobe_coeff(make, model);
#ifdef LIBRAW_LIBRARY_BUILD
  else if (imgdata.color.cam_xyz[0][0] < 0.01)
    adobe_coeff(make, model, 1);
#endif

  if (load_raw == &CLASS kodak_radc_load_raw)
    if (raw_color)
      adobe_coeff("Apple", "Quicktake");

#ifdef LIBRAW_LIBRARY_BUILD
  // Clear erorneus fuji_width if not set through parse_fuji or for DNG
  if (fuji_width && !dng_version && !(imgdata.process_warnings & LIBRAW_WARN_PARSEFUJI_PROCESSED))
    fuji_width = 0;
#endif
  if (fuji_width)
  {
    fuji_width = width >> !fuji_layout;
    filters = fuji_width & 1 ? 0x94949494 : 0x49494949;
    width = (height >> fuji_layout) + fuji_width;
    height = width - 1;
    pixel_aspect = 1;
  }
  else
  {
    if (raw_height < height)
      raw_height = height;
    if (raw_width < width)
      raw_width = width;
  }
  if (!tiff_bps)
    tiff_bps = 12;
  if (!maximum)
  {
    maximum = (1 << tiff_bps) - 1;
    if (maximum < 0x10000 && curve[maximum] > 0 && load_raw == &CLASS sony_arw2_load_raw)
      maximum = curve[maximum];
  }
  if(maximum > 0xffff) maximum = 0xffff;
  if (!load_raw || height < 22 || width < 22 ||
#ifdef LIBRAW_LIBRARY_BUILD
    (tiff_bps > 16 && (load_raw != &LibRaw::deflate_dng_load_raw  && load_raw != &LibRaw::float_dng_load_raw_placeholder))
#else
      tiff_bps > 16
#endif
      || tiff_samples > 6 || colors > 4)
    is_raw = 0;

  if (raw_width < 22 || raw_width > 64000 || raw_height < 22 || raw_height > 64000)
    is_raw = 0;

#ifdef NO_JASPER
  if (load_raw == &CLASS redcine_load_raw)
  {
#ifdef DCRAW_VERBOSE
    fprintf(stderr, _("%s: You must link dcraw with %s!!\n"), ifname, "libjasper");
#endif
    is_raw = 0;
#ifdef LIBRAW_LIBRARY_BUILD
    imgdata.process_warnings |= LIBRAW_WARN_NO_JASPER;
#endif
  }
#endif
#ifdef NO_JPEG
  if (load_raw == &CLASS kodak_jpeg_load_raw || load_raw == &CLASS lossy_dng_load_raw)
  {
#ifdef DCRAW_VERBOSE
    fprintf(stderr, _("%s: You must link dcraw with %s!!\n"), ifname, "libjpeg");
#endif
    is_raw = 0;
#ifdef LIBRAW_LIBRARY_BUILD
    imgdata.process_warnings |= LIBRAW_WARN_NO_JPEGLIB;
#endif
  }
#endif
  if (!cdesc[0])
    strcpy(cdesc, colors == 3 ? "RGBG" : "GMCY");
  if (!raw_height)
    raw_height = height;
  if (!raw_width)
    raw_width = width;
  if (filters > 999 && colors == 3)
    filters |= ((filters >> 2 & 0x22222222) | (filters << 2 & 0x88888888)) & filters << 1;
notraw:
  if (flip == UINT_MAX)
    flip = tiff_flip;
  if (flip == UINT_MAX)
    flip = 0;

  // Convert from degrees to bit-field if needed
  if (flip > 89 || flip < -89)
  {
    switch ((flip + 3600) % 360)
    {
    case 270:
      flip = 5;
      break;
    case 180:
      flip = 3;
      break;
    case 90:
      flip = 6;
      break;
    }
  }

#ifdef LIBRAW_LIBRARY_BUILD

  if (pana_bpp)
    imgdata.color.raw_bps = pana_bpp;
  else if ((load_raw == &CLASS phase_one_load_raw) ||
           (load_raw == &CLASS phase_one_load_raw_c))
    imgdata.color.raw_bps = ph1.format;
  else
    imgdata.color.raw_bps = tiff_bps;

  RUN_CALLBACK(LIBRAW_PROGRESS_IDENTIFY, 1, 2);
#endif
}

//@end COMMON

//@out FILEIO
#ifndef NO_LCMS
void CLASS apply_profile(const char *input, const char *output)
{
  char *prof;
  cmsHPROFILE hInProfile = 0, hOutProfile = 0;
  cmsHTRANSFORM hTransform;
  FILE *fp;
  unsigned size;

  if (strcmp(input, "embed"))
    hInProfile = cmsOpenProfileFromFile(input, "r");
  else if (profile_length)
  {
#ifndef LIBRAW_LIBRARY_BUILD
    prof = (char *)malloc(profile_length);
    merror(prof, "apply_profile()");
    fseek(ifp, profile_offset, SEEK_SET);
    fread(prof, 1, profile_length, ifp);
    hInProfile = cmsOpenProfileFromMem(prof, profile_length);
    free(prof);
#else
    hInProfile = cmsOpenProfileFromMem(imgdata.color.profile, profile_length);
#endif
  }
  else
  {
#ifdef LIBRAW_LIBRARY_BUILD
    imgdata.process_warnings |= LIBRAW_WARN_NO_EMBEDDED_PROFILE;
#endif
#ifdef DCRAW_VERBOSE
    fprintf(stderr, _("%s has no embedded profile.\n"), ifname);
#endif
  }
  if (!hInProfile)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    imgdata.process_warnings |= LIBRAW_WARN_NO_INPUT_PROFILE;
#endif
    return;
  }
  if (!output)
    hOutProfile = cmsCreate_sRGBProfile();
  else if ((fp = fopen(output, "rb")))
  {
    fread(&size, 4, 1, fp);
    fseek(fp, 0, SEEK_SET);
    oprof = (unsigned *)malloc(size = ntohl(size));
    merror(oprof, "apply_profile()");
    fread(oprof, 1, size, fp);
    fclose(fp);
    if (!(hOutProfile = cmsOpenProfileFromMem(oprof, size)))
    {
      free(oprof);
      oprof = 0;
    }
  }
#ifdef DCRAW_VERBOSE
  else
    fprintf(stderr, _("Cannot open file %s!\n"), output);
#endif
  if (!hOutProfile)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    imgdata.process_warnings |= LIBRAW_WARN_BAD_OUTPUT_PROFILE;
#endif
    goto quit;
  }
#ifdef DCRAW_VERBOSE
  if (verbose)
    fprintf(stderr, _("Applying color profile...\n"));
#endif
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_APPLY_PROFILE, 0, 2);
#endif
  hTransform = cmsCreateTransform(hInProfile, TYPE_RGBA_16, hOutProfile, TYPE_RGBA_16, INTENT_PERCEPTUAL, 0);
  cmsDoTransform(hTransform, image, image, width * height);
  raw_color = 1; /* Don't use rgb_cam with a profile */
  cmsDeleteTransform(hTransform);
  cmsCloseProfile(hOutProfile);
quit:
  cmsCloseProfile(hInProfile);
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_APPLY_PROFILE, 1, 2);
#endif
}
#endif
//@end FILEIO

//@out COMMON
void CLASS convert_to_rgb()
{
#ifndef LIBRAW_LIBRARY_BUILD
  int row, col, c;
#endif
  int i, j, k;
#ifndef LIBRAW_LIBRARY_BUILD
  ushort *img;
  float out[3];
#endif
  float out_cam[3][4];
  double num, inverse[3][3];
  static const double xyzd50_srgb[3][3] = {
      {0.436083, 0.385083, 0.143055}, {0.222507, 0.716888, 0.060608}, {0.013930, 0.097097, 0.714022}};
  static const double rgb_rgb[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
  static const double adobe_rgb[3][3] = {
      {0.715146, 0.284856, 0.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 0.041166, 0.958839}};
  static const double wide_rgb[3][3] = {
      {0.593087, 0.404710, 0.002206}, {0.095413, 0.843149, 0.061439}, {0.011621, 0.069091, 0.919288}};
  static const double prophoto_rgb[3][3] = {
      {0.529317, 0.330092, 0.140588}, {0.098368, 0.873465, 0.028169}, {0.016879, 0.117663, 0.865457}};
  static const double aces_rgb[3][3] = {
      {0.432996, 0.375380, 0.189317}, {0.089427, 0.816523, 0.102989}, {0.019165, 0.118150, 0.941914}};
  static const double(*out_rgb[])[3] = {rgb_rgb, adobe_rgb, wide_rgb, prophoto_rgb, xyz_rgb, aces_rgb};
  static const char *name[] = {"sRGB", "Adobe RGB (1998)", "WideGamut D65", "ProPhoto D65", "XYZ", "ACES"};
  static const unsigned phead[] = {1024, 0, 0x2100000,  0x6d6e7472, 0x52474220, 0x58595a20, 0,
                                   0,    0, 0x61637370, 0,          0,          0x6e6f6e65, 0,
                                   0,    0, 0,          0xf6d6,     0x10000,    0xd32d};
  unsigned pbody[] = {10,         0x63707274, 0,  36, /* cprt */
                      0x64657363, 0,          40,     /* desc */
                      0x77747074, 0,          20,     /* wtpt */
                      0x626b7074, 0,          20,     /* bkpt */
                      0x72545243, 0,          14,     /* rTRC */
                      0x67545243, 0,          14,     /* gTRC */
                      0x62545243, 0,          14,     /* bTRC */
                      0x7258595a, 0,          20,     /* rXYZ */
                      0x6758595a, 0,          20,     /* gXYZ */
                      0x6258595a, 0,          20};    /* bXYZ */
  static const unsigned pwhite[] = {0xf351, 0x10000, 0x116cc};
  unsigned pcurve[] = {0x63757276, 0, 1, 0x1000000};

#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_CONVERT_RGB, 0, 2);
#endif
  gamma_curve(gamm[0], gamm[1], 0, 0);
  memcpy(out_cam, rgb_cam, sizeof out_cam);
#ifndef LIBRAW_LIBRARY_BUILD
  raw_color |= colors == 1 || document_mode || output_color < 1 || output_color > 6;
#else
  raw_color |= colors == 1 || output_color < 1 || output_color > 6;
#endif
  if (!raw_color)
  {
    oprof = (unsigned *)calloc(phead[0], 1);
    merror(oprof, "convert_to_rgb()");
    memcpy(oprof, phead, sizeof phead);
    if (output_color == 5)
      oprof[4] = oprof[5];
    oprof[0] = 132 + 12 * pbody[0];
    for (i = 0; i < pbody[0]; i++)
    {
      oprof[oprof[0] / 4] = i ? (i > 1 ? 0x58595a20 : 0x64657363) : 0x74657874;
      pbody[i * 3 + 2] = oprof[0];
      oprof[0] += (pbody[i * 3 + 3] + 3) & -4;
    }
    memcpy(oprof + 32, pbody, sizeof pbody);
    oprof[pbody[5] / 4 + 2] = strlen(name[output_color - 1]) + 1;
    memcpy((char *)oprof + pbody[8] + 8, pwhite, sizeof pwhite);
    pcurve[3] = (short)(256 / gamm[5] + 0.5) << 16;
    for (i = 4; i < 7; i++)
      memcpy((char *)oprof + pbody[i * 3 + 2], pcurve, sizeof pcurve);
    pseudoinverse((double(*)[3])out_rgb[output_color - 1], inverse, 3);
    for (i = 0; i < 3; i++)
      for (j = 0; j < 3; j++)
      {
        for (num = k = 0; k < 3; k++)
          num += xyzd50_srgb[i][k] * inverse[j][k];
        oprof[pbody[j * 3 + 23] / 4 + i + 2] = num * 0x10000 + 0.5;
      }
    for (i = 0; i < phead[0] / 4; i++)
      oprof[i] = htonl(oprof[i]);
    strcpy((char *)oprof + pbody[2] + 8, "auto-generated by dcraw");
    strcpy((char *)oprof + pbody[5] + 12, name[output_color - 1]);
    for (i = 0; i < 3; i++)
      for (j = 0; j < colors; j++)
        for (out_cam[i][j] = k = 0; k < 3; k++)
          out_cam[i][j] += out_rgb[output_color - 1][i][k] * rgb_cam[k][j];
  }
#ifdef DCRAW_VERBOSE
  if (verbose)
    fprintf(stderr, raw_color ? _("Building histograms...\n") : _("Converting to %s colorspace...\n"),
            name[output_color - 1]);
#endif
#ifdef LIBRAW_LIBRARY_BUILD
  convert_to_rgb_loop(out_cam);
#else
  memset(histogram, 0, sizeof histogram);
  for (img = image[0], row = 0; row < height; row++)
    for (col = 0; col < width; col++, img += 4)
    {
      if (!raw_color)
      {
        out[0] = out[1] = out[2] = 0;
        FORCC
        {
          out[0] += out_cam[0][c] * img[c];
          out[1] += out_cam[1][c] * img[c];
          out[2] += out_cam[2][c] * img[c];
        }
        FORC3 img[c] = CLIP((int)out[c]);
      }
      else if (document_mode)
        img[0] = img[fcol(row, col)];
      FORCC histogram[c][img[c] >> 3]++;
    }
#endif
  if (colors == 4 && output_color)
    colors = 3;
#ifndef LIBRAW_LIBRARY_BUILD
  if (document_mode && filters)
    colors = 1;
#endif
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_CONVERT_RGB, 1, 2);
#endif
}

void CLASS fuji_rotate()
{
  int i, row, col;
  double step;
  float r, c, fr, fc;
  unsigned ur, uc;
  ushort wide, high, (*img)[4], (*pix)[4];

  if (!fuji_width)
    return;
#ifdef DCRAW_VERBOSE
  if (verbose)
    fprintf(stderr, _("Rotating image 45 degrees...\n"));
#endif
  fuji_width = (fuji_width - 1 + shrink) >> shrink;
  step = sqrt(0.5);
  wide = fuji_width / step;
  high = (height - fuji_width) / step;
  img = (ushort(*)[4])calloc(high, wide * sizeof *img);
  merror(img, "fuji_rotate()");

#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_FUJI_ROTATE, 0, 2);
#endif

  for (row = 0; row < high; row++)
    for (col = 0; col < wide; col++)
    {
      ur = r = fuji_width + (row - col) * step;
      uc = c = (row + col) * step;
      if (ur > height - 2 || uc > width - 2)
        continue;
      fr = r - ur;
      fc = c - uc;
      pix = image + ur * width + uc;
      for (i = 0; i < colors; i++)
        img[row * wide + col][i] = (pix[0][i] * (1 - fc) + pix[1][i] * fc) * (1 - fr) +
                                   (pix[width][i] * (1 - fc) + pix[width + 1][i] * fc) * fr;
    }

  free(image);
  width = wide;
  height = high;
  image = img;
  fuji_width = 0;
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_FUJI_ROTATE, 1, 2);
#endif
}

void CLASS stretch()
{
  ushort newdim, (*img)[4], *pix0, *pix1;
  int row, col, c;
  double rc, frac;

  if (pixel_aspect == 1)
    return;
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_STRETCH, 0, 2);
#endif
#ifdef DCRAW_VERBOSE
  if (verbose)
    fprintf(stderr, _("Stretching the image...\n"));
#endif
  if (pixel_aspect < 1)
  {
    newdim = height / pixel_aspect + 0.5;
    img = (ushort(*)[4])calloc(width, newdim * sizeof *img);
    merror(img, "stretch()");
    for (rc = row = 0; row < newdim; row++, rc += pixel_aspect)
    {
      frac = rc - (c = rc);
      pix0 = pix1 = image[c * width];
      if (c + 1 < height)
        pix1 += width * 4;
      for (col = 0; col < width; col++, pix0 += 4, pix1 += 4)
        FORCC img[row * width + col][c] = pix0[c] * (1 - frac) + pix1[c] * frac + 0.5;
    }
    height = newdim;
  }
  else
  {
    newdim = width * pixel_aspect + 0.5;
    img = (ushort(*)[4])calloc(height, newdim * sizeof *img);
    merror(img, "stretch()");
    for (rc = col = 0; col < newdim; col++, rc += 1 / pixel_aspect)
    {
      frac = rc - (c = rc);
      pix0 = pix1 = image[c];
      if (c + 1 < width)
        pix1 += 4;
      for (row = 0; row < height; row++, pix0 += width * 4, pix1 += width * 4)
        FORCC img[row * newdim + col][c] = pix0[c] * (1 - frac) + pix1[c] * frac + 0.5;
    }
    width = newdim;
  }
  free(image);
  image = img;
#ifdef LIBRAW_LIBRARY_BUILD
  RUN_CALLBACK(LIBRAW_PROGRESS_STRETCH, 1, 2);
#endif
}

int CLASS flip_index(int row, int col)
{
  if (flip & 4)
    SWAP(row, col);
  if (flip & 2)
    row = iheight - 1 - row;
  if (flip & 1)
    col = iwidth - 1 - col;
  return row * iwidth + col;
}
//@end COMMON

struct libraw_tiff_tag
{
  ushort tag, type;
  int count;
  union {
    char c[4];
    short s[2];
    int i;
  } val;
};

struct tiff_hdr
{
  ushort t_order, magic;
  int ifd;
  ushort pad, ntag;
  struct libraw_tiff_tag tag[23];
  int nextifd;
  ushort pad2, nexif;
  struct libraw_tiff_tag exif[4];
  ushort pad3, ngps;
  struct libraw_tiff_tag gpst[10];
  short bps[4];
  int rat[10];
  unsigned gps[26];
  char t_desc[512], t_make[64], t_model[64], soft[32], date[20], t_artist[64];
};

//@out COMMON

void CLASS tiff_set(struct tiff_hdr *th, ushort *ntag, ushort tag, ushort type, int count, int val)
{
  struct libraw_tiff_tag *tt;
  int c;

  tt = (struct libraw_tiff_tag *)(ntag + 1) + (*ntag)++;
  tt->val.i = val;
  if (type == 1 && count <= 4)
    FORC(4) tt->val.c[c] = val >> (c << 3);
  else if (type == 2)
  {
    count = strnlen((char *)th + val, count - 1) + 1;
    if (count <= 4)
      FORC(4) tt->val.c[c] = ((char *)th)[val + c];
  }
  else if (type == 3 && count <= 2)
    FORC(2) tt->val.s[c] = val >> (c << 4);
  tt->count = count;
  tt->type = type;
  tt->tag = tag;
}

#define TOFF(ptr) ((char *)(&(ptr)) - (char *)th)

void CLASS tiff_head(struct tiff_hdr *th, int full)
{
  int c, psize = 0;
  struct tm *t;

  memset(th, 0, sizeof *th);
  th->t_order = htonl(0x4d4d4949) >> 16;
  th->magic = 42;
  th->ifd = 10;
  th->rat[0] = th->rat[2] = 300;
  th->rat[1] = th->rat[3] = 1;
  FORC(6) th->rat[4 + c] = 1000000;
  th->rat[4] *= shutter;
  th->rat[6] *= aperture;
  th->rat[8] *= focal_len;
  strncpy(th->t_desc, desc, 512);
  strncpy(th->t_make, make, 64);
  strncpy(th->t_model, model, 64);
  strcpy(th->soft, "dcraw v" DCRAW_VERSION);
  t = localtime(&timestamp);
  sprintf(th->date, "%04d:%02d:%02d %02d:%02d:%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour,
          t->tm_min, t->tm_sec);
  strncpy(th->t_artist, artist, 64);
  if (full)
  {
    tiff_set(th, &th->ntag, 254, 4, 1, 0);
    tiff_set(th, &th->ntag, 256, 4, 1, width);
    tiff_set(th, &th->ntag, 257, 4, 1, height);
    tiff_set(th, &th->ntag, 258, 3, colors, output_bps);
    if (colors > 2)
      th->tag[th->ntag - 1].val.i = TOFF(th->bps);
    FORC4 th->bps[c] = output_bps;
    tiff_set(th, &th->ntag, 259, 3, 1, 1);
    tiff_set(th, &th->ntag, 262, 3, 1, 1 + (colors > 1));
  }
  tiff_set(th, &th->ntag, 270, 2, 512, TOFF(th->t_desc));
  tiff_set(th, &th->ntag, 271, 2, 64, TOFF(th->t_make));
  tiff_set(th, &th->ntag, 272, 2, 64, TOFF(th->t_model));
  if (full)
  {
    if (oprof)
      psize = ntohl(oprof[0]);
    tiff_set(th, &th->ntag, 273, 4, 1, sizeof *th + psize);
    tiff_set(th, &th->ntag, 277, 3, 1, colors);
    tiff_set(th, &th->ntag, 278, 4, 1, height);
    tiff_set(th, &th->ntag, 279, 4, 1, height * width * colors * output_bps / 8);
  }
  else
    tiff_set(th, &th->ntag, 274, 3, 1, "12435867"[flip] - '0');
  tiff_set(th, &th->ntag, 282, 5, 1, TOFF(th->rat[0]));
  tiff_set(th, &th->ntag, 283, 5, 1, TOFF(th->rat[2]));
  tiff_set(th, &th->ntag, 284, 3, 1, 1);
  tiff_set(th, &th->ntag, 296, 3, 1, 2);
  tiff_set(th, &th->ntag, 305, 2, 32, TOFF(th->soft));
  tiff_set(th, &th->ntag, 306, 2, 20, TOFF(th->date));
  tiff_set(th, &th->ntag, 315, 2, 64, TOFF(th->t_artist));
  tiff_set(th, &th->ntag, 34665, 4, 1, TOFF(th->nexif));
  if (psize)
    tiff_set(th, &th->ntag, 34675, 7, psize, sizeof *th);
  tiff_set(th, &th->nexif, 33434, 5, 1, TOFF(th->rat[4]));
  tiff_set(th, &th->nexif, 33437, 5, 1, TOFF(th->rat[6]));
  tiff_set(th, &th->nexif, 34855, 3, 1, iso_speed);
  tiff_set(th, &th->nexif, 37386, 5, 1, TOFF(th->rat[8]));
  if (gpsdata[1])
  {
    tiff_set(th, &th->ntag, 34853, 4, 1, TOFF(th->ngps));
    tiff_set(th, &th->ngps, 0, 1, 4, 0x202);
    tiff_set(th, &th->ngps, 1, 2, 2, gpsdata[29]);
    tiff_set(th, &th->ngps, 2, 5, 3, TOFF(th->gps[0]));
    tiff_set(th, &th->ngps, 3, 2, 2, gpsdata[30]);
    tiff_set(th, &th->ngps, 4, 5, 3, TOFF(th->gps[6]));
    tiff_set(th, &th->ngps, 5, 1, 1, gpsdata[31]);
    tiff_set(th, &th->ngps, 6, 5, 1, TOFF(th->gps[18]));
    tiff_set(th, &th->ngps, 7, 5, 3, TOFF(th->gps[12]));
    tiff_set(th, &th->ngps, 18, 2, 12, TOFF(th->gps[20]));
    tiff_set(th, &th->ngps, 29, 2, 12, TOFF(th->gps[23]));
    memcpy(th->gps, gpsdata, sizeof th->gps);
  }
}

#ifdef LIBRAW_LIBRARY_BUILD
void CLASS jpeg_thumb_writer(FILE *tfp, char *t_humb, int t_humb_length)
{
  ushort exif[5];
  struct tiff_hdr th;
  fputc(0xff, tfp);
  fputc(0xd8, tfp);
  if (strcmp(t_humb + 6, "Exif"))
  {
    memcpy(exif, "\xff\xe1  Exif\0\0", 10);
    exif[1] = htons(8 + sizeof th);
    fwrite(exif, 1, sizeof exif, tfp);
    tiff_head(&th, 0);
    fwrite(&th, 1, sizeof th, tfp);
  }
  fwrite(t_humb + 2, 1, t_humb_length - 2, tfp);
}

void CLASS jpeg_thumb()
{
  char *thumb;

  thumb = (char *)malloc(thumb_length);
  merror(thumb, "jpeg_thumb()");
  fread(thumb, 1, thumb_length, ifp);
  jpeg_thumb_writer(ofp, thumb, thumb_length);
  free(thumb);
}
#else
void CLASS jpeg_thumb()
{
  char *thumb;
  ushort exif[5];
  struct tiff_hdr th;

  thumb = (char *)malloc(thumb_length);
  merror(thumb, "jpeg_thumb()");
  fread(thumb, 1, thumb_length, ifp);
  fputc(0xff, ofp);
  fputc(0xd8, ofp);
  if (strcmp(thumb + 6, "Exif"))
  {
    memcpy(exif, "\xff\xe1  Exif\0\0", 10);
    exif[1] = htons(8 + sizeof th);
    fwrite(exif, 1, sizeof exif, ofp);
    tiff_head(&th, 0);
    fwrite(&th, 1, sizeof th, ofp);
  }
  fwrite(thumb + 2, 1, thumb_length - 2, ofp);
  free(thumb);
}
#endif

void CLASS write_ppm_tiff()
{
  struct tiff_hdr th;
  uchar *ppm;
  ushort *ppm2;
  int c, row, col, soff, rstep, cstep;
  int perc, val, total, t_white = 0x2000;

#ifdef LIBRAW_LIBRARY_BUILD
  perc = width * height * auto_bright_thr;
#else
  perc = width * height * 0.01; /* 99th percentile white level */
#endif
  if (fuji_width)
    perc /= 2;
  if (!((highlight & ~2) || no_auto_bright))
    for (t_white = c = 0; c < colors; c++)
    {
      for (val = 0x2000, total = 0; --val > 32;)
        if ((total += histogram[c][val]) > perc)
          break;
      if (t_white < val)
        t_white = val;
    }
  gamma_curve(gamm[0], gamm[1], 2, (t_white << 3) / bright);
  iheight = height;
  iwidth = width;
  if (flip & 4)
    SWAP(height, width);
  ppm = (uchar *)calloc(width, colors * output_bps / 8);
  ppm2 = (ushort *)ppm;
  merror(ppm, "write_ppm_tiff()");
  if (output_tiff)
  {
    tiff_head(&th, 1);
    fwrite(&th, sizeof th, 1, ofp);
    if (oprof)
      fwrite(oprof, ntohl(oprof[0]), 1, ofp);
  }
  else if (colors > 3)
    fprintf(ofp, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH %d\nMAXVAL %d\nTUPLTYPE %s\nENDHDR\n", width, height, colors,
            (1 << output_bps) - 1, cdesc);
  else
    fprintf(ofp, "P%d\n%d %d\n%d\n", colors / 2 + 5, width, height, (1 << output_bps) - 1);
  soff = flip_index(0, 0);
  cstep = flip_index(0, 1) - soff;
  rstep = flip_index(1, 0) - flip_index(0, width);
  for (row = 0; row < height; row++, soff += rstep)
  {
    for (col = 0; col < width; col++, soff += cstep)
      if (output_bps == 8)
        FORCC ppm[col * colors + c] = curve[image[soff][c]] >> 8;
      else
        FORCC ppm2[col * colors + c] = curve[image[soff][c]];
    if (output_bps == 16 && !output_tiff && htons(0x55aa) != 0x55aa)
      swab((char *)ppm2, (char *)ppm2, width * colors * 2);
    fwrite(ppm, colors * output_bps / 8, width, ofp);
  }
  free(ppm);
}
//@end COMMON

int CLASS main(int argc, const char **argv)
{
  int arg, status = 0, quality, i, c;
  int timestamp_only = 0, thumbnail_only = 0, identify_only = 0;
  int user_qual = -1, user_black = -1, user_sat = -1, user_flip = -1;
  int use_fuji_rotate = 1, write_to_stdout = 0, read_from_stdin = 0;
  const char *sp, *bpfile = 0, *dark_frame = 0, *write_ext;
  char opm, opt, *ofname, *cp;
  struct utimbuf ut;
#ifndef NO_LCMS
  const char *cam_profile = 0, *out_profile = 0;
#endif

#ifndef LOCALTIME
  putenv((char *)"TZ=UTC");
#endif
#ifdef LOCALEDIR
  setlocale(LC_CTYPE, "");
  setlocale(LC_MESSAGES, "");
  bindtextdomain("dcraw", LOCALEDIR);
  textdomain("dcraw");
#endif

  if (argc == 1)
  {
    printf(_("\nRaw photo decoder \"dcraw\" v%s"), DCRAW_VERSION);
    printf(_("\nby Dave Coffin, dcoffin a cybercom o net\n"));
    printf(_("\nUsage:  %s [OPTION]... [FILE]...\n\n"), argv[0]);
    puts(_("-v        Print verbose messages"));
    puts(_("-c        Write image data to standard output"));
    puts(_("-e        Extract embedded thumbnail image"));
    puts(_("-i        Identify files without decoding them"));
    puts(_("-i -v     Identify files and show metadata"));
    puts(_("-z        Change file dates to camera timestamp"));
    puts(_("-w        Use camera white balance, if possible"));
    puts(_("-a        Average the whole image for white balance"));
    puts(_("-A <x y w h> Average a grey box for white balance"));
    puts(_("-r <r g b g> Set custom white balance"));
    puts(_("+M/-M     Use/don't use an embedded color matrix"));
    puts(_("-C <r b>  Correct chromatic aberration"));
    puts(_("-P <file> Fix the dead pixels listed in this file"));
    puts(_("-K <file> Subtract dark frame (16-bit raw PGM)"));
    puts(_("-k <num>  Set the darkness level"));
    puts(_("-S <num>  Set the saturation level"));
    puts(_("-n <num>  Set threshold for wavelet denoising"));
    puts(_("-H [0-9]  Highlight mode (0=clip, 1=unclip, 2=blend, 3+=rebuild)"));
    puts(_("-t [0-7]  Flip image (0=none, 3=180, 5=90CCW, 6=90CW)"));
    puts(_("-o [0-5]  Output colorspace (raw,sRGB,Adobe,Wide,ProPhoto,XYZ)"));
#ifndef NO_LCMS
    puts(_("-o <file> Apply output ICC profile from file"));
    puts(_("-p <file> Apply camera ICC profile from file or \"embed\""));
#endif
    puts(_("-d        Document mode (no color, no interpolation)"));
    puts(_("-D        Document mode without scaling (totally raw)"));
    puts(_("-j        Don't stretch or rotate raw pixels"));
    puts(_("-W        Don't automatically brighten the image"));
    puts(_("-b <num>  Adjust brightness (default = 1.0)"));
    puts(_("-g <p ts> Set custom gamma curve (default = 2.222 4.5)"));
    puts(_("-q [0-3]  Set the interpolation quality"));
    puts(_("-h        Half-size color image (twice as fast as \"-q 0\")"));
    puts(_("-f        Interpolate RGGB as four colors"));
    puts(_("-m <num>  Apply a 3x3 median filter to R-G and B-G"));
    puts(_("-s [0..N-1] Select one raw image or \"all\" from each file"));
    puts(_("-6        Write 16-bit instead of 8-bit"));
    puts(_("-4        Linear 16-bit, same as \"-6 -W -g 1 1\""));
    puts(_("-T        Write TIFF instead of PPM"));
    puts("");
    return 1;
  }
  argv[argc] = "";
  for (arg = 1; (((opm = argv[arg][0]) - 2) | 2) == '+';)
  {
    opt = argv[arg++][1];
    if ((cp = (char *)strchr(sp = "nbrkStqmHACg", opt)))
      for (i = 0; i < "114111111422"[cp - sp] - '0'; i++)
        if (!isdigit(argv[arg + i][0]))
        {
          fprintf(stderr, _("Non-numeric argument to \"-%c\"\n"), opt);
          return 1;
        }
    switch (opt)
    {
    case 'n':
      threshold = atof(argv[arg++]);
      break;
    case 'b':
      bright = atof(argv[arg++]);
      break;
    case 'r':
      FORC4 user_mul[c] = atof(argv[arg++]);
      break;
    case 'C':
      aber[0] = 1 / atof(argv[arg++]);
      aber[2] = 1 / atof(argv[arg++]);
      break;
    case 'g':
      gamm[0] = atof(argv[arg++]);
      gamm[1] = atof(argv[arg++]);
      if (gamm[0])
        gamm[0] = 1 / gamm[0];
      break;
    case 'k':
      user_black = atoi(argv[arg++]);
      break;
    case 'S':
      user_sat = atoi(argv[arg++]);
      break;
    case 't':
      user_flip = atoi(argv[arg++]);
      break;
    case 'q':
      user_qual = atoi(argv[arg++]);
      break;
    case 'm':
      med_passes = atoi(argv[arg++]);
      break;
    case 'H':
      highlight = atoi(argv[arg++]);
      break;
    case 's':
      shot_select = abs(atoi(argv[arg]));
      multi_out = !strcmp(argv[arg++], "all");
      break;
    case 'o':
      if (isdigit(argv[arg][0]) && !argv[arg][1])
        output_color = atoi(argv[arg++]);
#ifndef NO_LCMS
      else
        out_profile = argv[arg++];
      break;
    case 'p':
      cam_profile = argv[arg++];
#endif
      break;
    case 'P':
      bpfile = argv[arg++];
      break;
    case 'K':
      dark_frame = argv[arg++];
      break;
    case 'z':
      timestamp_only = 1;
      break;
    case 'e':
      thumbnail_only = 1;
      break;
    case 'i':
      identify_only = 1;
      break;
    case 'c':
      write_to_stdout = 1;
      break;
    case 'v':
      verbose = 1;
      break;
    case 'h':
      half_size = 1;
      break;
    case 'f':
      four_color_rgb = 1;
      break;
    case 'A':
      FORC4 greybox[c] = atoi(argv[arg++]);
    case 'a':
      use_auto_wb = 1;
      break;
    case 'w':
      use_camera_wb = 1;
      break;
    case 'M':
      use_camera_matrix = 3 * (opm == '+');
      break;
    case 'I':
      read_from_stdin = 1;
      break;
    case 'E':
      document_mode++;
    case 'D':
      document_mode++;
    case 'd':
      document_mode++;
    case 'j':
      use_fuji_rotate = 0;
      break;
    case 'W':
      no_auto_bright = 1;
      break;
    case 'T':
      output_tiff = 1;
      break;
    case '4':
      gamm[0] = gamm[1] = no_auto_bright = 1;
    case '6':
      output_bps = 16;
      break;
    default:
      fprintf(stderr, _("Unknown option \"-%c\".\n"), opt);
      return 1;
    }
  }
  if (arg == argc)
  {
    fprintf(stderr, _("No files to process.\n"));
    return 1;
  }
  if (write_to_stdout)
  {
    if (isatty(1))
    {
      fprintf(stderr, _("Will not write an image to the terminal!\n"));
      return 1;
    }
#if defined(WIN32) || defined(DJGPP) || defined(__CYGWIN__)
    if (setmode(1, O_BINARY) < 0)
    {
      perror("setmode()");
      return 1;
    }
#endif
  }
  for (; arg < argc; arg++)
  {
    status = 1;
    raw_image = 0;
    image = 0;
    oprof = 0;
    meta_data = ofname = 0;
    ofp = stdout;
    if (setjmp(failure))
    {
      if (fileno(ifp) > 2)
        fclose(ifp);
      if (fileno(ofp) > 2)
        fclose(ofp);
      status = 1;
      goto cleanup;
    }
    ifname = argv[arg];
    if (!(ifp = fopen(ifname, "rb")))
    {
      perror(ifname);
      continue;
    }
    status = (identify(), !is_raw);
    if (user_flip >= 0)
      flip = user_flip;
    switch ((flip + 3600) % 360)
    {
    case 270:
      flip = 5;
      break;
    case 180:
      flip = 3;
      break;
    case 90:
      flip = 6;
    }
    if (timestamp_only)
    {
      if ((status = !timestamp))
        fprintf(stderr, _("%s has no timestamp.\n"), ifname);
      else if (identify_only)
        printf("%10ld%10d %s\n", (long)timestamp, shot_order, ifname);
      else
      {
        if (verbose)
          fprintf(stderr, _("%s time set to %d.\n"), ifname, (int)timestamp);
        ut.actime = ut.modtime = timestamp;
        utime(ifname, &ut);
      }
      goto next;
    }
    write_fun = &CLASS write_ppm_tiff;
    if (thumbnail_only)
    {
      if ((status = !thumb_offset))
      {
        fprintf(stderr, _("%s has no thumbnail.\n"), ifname);
        goto next;
      }
      else if (thumb_load_raw)
      {
        load_raw = thumb_load_raw;
        data_offset = thumb_offset;
        height = thumb_height;
        width = thumb_width;
        filters = 0;
        colors = 3;
      }
      else
      {
        fseek(ifp, thumb_offset, SEEK_SET);
        write_fun = write_thumb;
        goto thumbnail;
      }
    }
    if (load_raw == &CLASS kodak_ycbcr_load_raw)
    {
      height += height & 1;
      width += width & 1;
    }

    if (identify_only && verbose && make[0])
    {
      printf(_("\nFilename: %s\n"), ifname);
      printf(_("Timestamp: %s"), ctime(&timestamp));
      printf(_("Camera: %s %s\n"), make, model);
      if (artist[0])
        printf(_("Owner: %s\n"), artist);
      if (dng_version)
      {
        printf(_("DNG Version: "));
        for (i = 24; i >= 0; i -= 8)
          printf("%d%c", dng_version >> i & 255, i ? '.' : '\n');
      }
      printf(_("ISO speed: %d\n"), (int)iso_speed);
      printf(_("Shutter: "));
      if (shutter > 0 && shutter < 1)
        shutter = (printf("1/"), 1 / shutter);
      printf(_("%0.1f sec\n"), shutter);
      printf(_("Aperture: f/%0.1f\n"), aperture);
      printf(_("Focal length: %0.1f mm\n"), focal_len);
      printf(_("Embedded ICC profile: %s\n"), profile_length ? _("yes") : _("no"));
      printf(_("Number of raw images: %d\n"), is_raw);
      if (pixel_aspect != 1)
        printf(_("Pixel Aspect Ratio: %0.6f\n"), pixel_aspect);
      if (thumb_offset)
        printf(_("Thumb size:  %4d x %d\n"), thumb_width, thumb_height);
      printf(_("Full size:   %4d x %d\n"), raw_width, raw_height);
    }
    else if (!is_raw)
      fprintf(stderr, _("Cannot decode file %s\n"), ifname);
    if (!is_raw)
      goto next;
    shrink = filters && (half_size || (!identify_only && (threshold || aber[0] != 1 || aber[2] != 1)));
    iheight = (height + shrink) >> shrink;
    iwidth = (width + shrink) >> shrink;
    if (identify_only)
    {
      if (verbose)
      {
        if (document_mode == 3)
        {
          top_margin = left_margin = fuji_width = 0;
          height = raw_height;
          width = raw_width;
        }
        iheight = (height + shrink) >> shrink;
        iwidth = (width + shrink) >> shrink;
        if (use_fuji_rotate)
        {
          if (fuji_width)
          {
            fuji_width = (fuji_width - 1 + shrink) >> shrink;
            iwidth = fuji_width / sqrt(0.5);
            iheight = (iheight - fuji_width) / sqrt(0.5);
          }
          else
          {
            if (pixel_aspect < 1)
              iheight = iheight / pixel_aspect + 0.5;
            if (pixel_aspect > 1)
              iwidth = iwidth * pixel_aspect + 0.5;
          }
        }
        if (flip & 4)
          SWAP(iheight, iwidth);
        printf(_("Image size:  %4d x %d\n"), width, height);
        printf(_("Output size: %4d x %d\n"), iwidth, iheight);
        printf(_("Raw colors: %d"), colors);
        if (filters)
        {
          int fhigh = 2, fwide = 2;
          if ((filters ^ (filters >> 8)) & 0xff)
            fhigh = 4;
          if ((filters ^ (filters >> 16)) & 0xffff)
            fhigh = 8;
          if (filters == 1)
            fhigh = fwide = 16;
          if (filters == 9)
            fhigh = fwide = 6;
          printf(_("\nFilter pattern: "));
          for (i = 0; i < fhigh; i++)
            for (c = i && putchar('/') && 0; c < fwide; c++)
              putchar(cdesc[fcol(i, c)]);
        }
        printf(_("\nDaylight multipliers:"));
        FORCC printf(" %f", pre_mul[c]);
        if (cam_mul[0] > 0)
        {
          printf(_("\nCamera multipliers:"));
          FORC4 printf(" %f", cam_mul[c]);
        }
        putchar('\n');
      }
      else
        printf(_("%s is a %s %s image.\n"), ifname, make, model);
    next:
      fclose(ifp);
      continue;
    }
    if (meta_length)
    {
      meta_data = (char *)malloc(meta_length);
      merror(meta_data, "main()");
    }
    if (filters || colors == 1)
    {
      raw_image = (ushort *)calloc((raw_height + 7), raw_width * 2);
      merror(raw_image, "main()");
    }
    else
    {
      image = (ushort(*)[4])calloc(iheight, iwidth * sizeof *image);
      merror(image, "main()");
    }
    if (verbose)
      fprintf(stderr, _("Loading %s %s image from %s ...\n"), make, model, ifname);
    if (shot_select >= is_raw)
      fprintf(stderr, _("%s: \"-s %d\" requests a nonexistent image!\n"), ifname, shot_select);
    fseeko(ifp, data_offset, SEEK_SET);
    if (raw_image && read_from_stdin)
      fread(raw_image, 2, raw_height * raw_width, stdin);
    else
      (*load_raw)();
    if (document_mode == 3)
    {
      top_margin = left_margin = fuji_width = 0;
      height = raw_height;
      width = raw_width;
    }
    iheight = (height + shrink) >> shrink;
    iwidth = (width + shrink) >> shrink;
    if (raw_image)
    {
      image = (ushort(*)[4])calloc(iheight, iwidth * sizeof *image);
      merror(image, "main()");
      crop_masked_pixels();
      free(raw_image);
    }
    if (zero_is_bad)
      remove_zeroes();
    bad_pixels(bpfile);
    if (dark_frame)
      subtract(dark_frame);
    quality = 2 + !fuji_width;
    if (user_qual >= 0)
      quality = user_qual;
    i = cblack[3];
    FORC3 if (i > cblack[c]) i = cblack[c];
    FORC4 cblack[c] -= i;
    black += i;
    i = cblack[6];
    FORC(cblack[4] * cblack[5])
    if (i > cblack[6 + c])
      i = cblack[6 + c];
    FORC(cblack[4] * cblack[5])
    cblack[6 + c] -= i;
    black += i;
    if (user_black >= 0)
      black = user_black;
    FORC4 cblack[c] += black;
    if (user_sat > 0)
      maximum = user_sat;
#ifdef COLORCHECK
    colorcheck();
#endif
    if (is_foveon)
    {
      if (document_mode || load_raw == &CLASS foveon_dp_load_raw)
      {
        for (i = 0; i < height * width * 4; i++)
          if ((short)image[0][i] < 0)
            image[0][i] = 0;
      }
      else
        foveon_interpolate();
    }
    else if (document_mode < 2)
      scale_colors();
    pre_interpolate();
    if (filters && !document_mode)
    {
      if (quality == 0)
        lin_interpolate();
      else if (quality == 1 || colors > 3)
        vng_interpolate();
      else if (quality == 2 && filters > 1000)
        ppg_interpolate();
      else if (filters == 9)
        xtrans_interpolate(quality * 2 - 3);
      else
        ahd_interpolate();
    }
    if (mix_green)
      for (colors = 3, i = 0; i < height * width; i++)
        image[i][1] = (image[i][1] + image[i][3]) >> 1;
    if (!is_foveon && colors == 3)
      median_filter();
    if (!is_foveon && highlight == 2)
      blend_highlights();
    if (!is_foveon && highlight > 2)
      recover_highlights();
    if (use_fuji_rotate)
      fuji_rotate();
#ifndef NO_LCMS
    if (cam_profile)
      apply_profile(cam_profile, out_profile);
#endif
    convert_to_rgb();
    if (use_fuji_rotate)
      stretch();
  thumbnail:
    if (write_fun == &CLASS jpeg_thumb)
      write_ext = ".jpg";
    else if (output_tiff && write_fun == &CLASS write_ppm_tiff)
      write_ext = ".tiff";
    else
      write_ext = ".pgm\0.ppm\0.ppm\0.pam" + colors * 5 - 5;
    ofname = (char *)malloc(strlen(ifname) + 64);
    merror(ofname, "main()");
    if (write_to_stdout)
      strcpy(ofname, _("standard output"));
    else
    {
      strcpy(ofname, ifname);
      if ((cp = strrchr(ofname, '.')))
        *cp = 0;
      if (multi_out)
        sprintf(ofname + strlen(ofname), "_%0*d", snprintf(0, 0, "%d", is_raw - 1), shot_select);
      if (thumbnail_only)
        strcat(ofname, ".thumb");
      strcat(ofname, write_ext);
      ofp = fopen(ofname, "wb");
      if (!ofp)
      {
        status = 1;
        perror(ofname);
        goto cleanup;
      }
    }
    if (verbose)
      fprintf(stderr, _("Writing data to %s ...\n"), ofname);
    (*write_fun)();
    fclose(ifp);
    if (ofp != stdout)
      fclose(ofp);
  cleanup:
    if (meta_data)
      free(meta_data);
    if (ofname)
      free(ofname);
    if (oprof)
      free(oprof);
    if (image)
      free(image);
    if (multi_out)
    {
      if (++shot_select < is_raw)
        arg--;
      else
        shot_select = 0;
    }
  }
  return status;
}
#endif
