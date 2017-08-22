/* -*- C++ -*-
 * File: dcraw_emu.cpp
 * Copyright 2008-2017 LibRaw LLC (info@libraw.org)
 * Created: Sun Mar 23,   2008
 *
 * LibRaw simple C++ API sample: almost complete dcraw emulator
 *

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).


 */
#ifdef WIN32
// suppress sprintf-related warning. sprintf() is permitted in sample code
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#ifndef WIN32
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#else
#include <io.h>
#endif
#include <fcntl.h>
#include <sys/stat.h>

#include "libraw/libraw.h"
#ifdef WIN32
#define snprintf _snprintf
#include <windows.h>
#else
#define O_BINARY 0
#endif

#ifdef USE_DNGSDK
#include "dng_host.h"
#include "dng_negative.h"
#include "dng_simple_image.h"
#include "dng_info.h"
#endif

#include <time.h>

#ifdef WIN32
#define strcasecmp stricmp
#define strncasecmp strnicmp
#endif

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
/*table of fluorescents:
12 = FL-D; Daylight fluorescent (D 5700K - 7100K) (F1,F5)
13 = FL-N; Day white fluorescent (N 4600K - 5400K) (F7,F8)
14 = FL-W; Cool white fluorescent (W 3900K - 4500K) (F2,F6, office, store, warehouse)
15 = FL-WW; White fluorescent (WW 3200K - 3700K) (F3, residential)
16 = FL-L; Soft/Warm white fluorescent (L 2600K - 3250K) (F4, kitchen, bath)
*/
const char *WB_LightSources[] = {
   "Unknown",  "Daylight", "Fluorescent",  "Tungsten",        "Flash",  "Reserved", "Reserved",
   "Reserved", "Reserved", "Fine Weather", "Cloudy",          "Shade",  "FL-D",     "FL-N",
   "FL-W",     "FL-WW",    "FL-L",         "Ill. A",          "Ill. B", "Ill. C",   "D55",
   "D65",      "D75",      "D50",          "Studio Tungsten",
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

void usage(const char *prog)
{
  printf("dcraw_emu: almost complete dcraw emulator\n");
  printf("Usage:  %s [OPTION]... [FILE]...\n", prog);
  printf("-c float-num       Set adjust maximum threshold (default 0.75)\n"
         "-v        Verbose: print progress messages (repeated -v will add verbosity)\n"
         "-w        Use camera white balance, if possible\n"
         "-a        Average the whole image for white balance\n"
         "-A <x y w h> Average a grey box for white balance\n"
         "-r <r g b g> Set custom white balance\n"
         "+M/-M     Use/don't use an embedded color matrix\n"
         "-C <r b>  Correct chromatic aberration\n"
         "-P <file> Fix the dead pixels listed in this file\n"
         "-K <file> Subtract dark frame (16-bit raw PGM)\n"
         "-k <num>  Set the darkness level\n"
         "-S <num>  Set the saturation level\n"
         "-R <num>  Set raw processing options to num\n"
         "-n <num>  Set threshold for wavelet denoising\n"
         "-H [0-9]  Highlight mode (0=clip, 1=unclip, 2=blend, 3+=rebuild)\n"
         "-t [0-7]  Flip image (0=none, 3=180, 5=90CCW, 6=90CW)\n"
         "-o [0-6]  Output colorspace (raw,sRGB,Adobe,Wide,ProPhoto,XYZ,ACES)\n"
#ifndef NO_LCMS
         "-o file   Output ICC profile\n"
         "-p file   Camera input profile (use \'embed\' for embedded profile)\n"
#endif
         "-j        Don't stretch or rotate raw pixels\n"
         "-W        Don't automatically brighten the image\n"
         "-b <num>  Adjust brightness (default = 1.0)\n"
         "-q N      Set the interpolation quality:\n"
         "          0 - linear, 1 - VNG, 2 - PPG, 3 - AHD, 4 - DCB\n"
#ifdef LIBRAW_DEMOSAIC_PACK_GPL2
         "          5 - modified AHD,6 - AFD (5pass), 7 - VCD, 8 - VCD+AHD, 9 - LMMSE\n"
#endif
#ifdef LIBRAW_DEMOSAIC_PACK_GPL3
         "          10-AMaZE\n"
#endif
         "-h        Half-size color image (twice as fast as \"-q 0\")\n"
         "-f        Interpolate RGGB as four colors\n"
         "-m <num>  Apply a 3x3 median filter to R-G and B-G\n"
         "-s [0..N-1] Select one raw image from input file\n"
         "-4        Linear 16-bit, same as \"-6 -W -g 1 1\n"
         "-6        Write 16-bit linear instead of 8-bit with gamma\n"
         "-g pow ts Set gamma curve to gamma pow and toe slope ts (default = 2.222 4.5)\n"
         "-T        Write TIFF instead of PPM\n"
         "-G        Use green_matching() filter\n"
         "-B <x y w h> use cropbox\n"
         "-F        Use FILE I/O instead of streambuf API\n"
         "-timing   Detailed timing report\n"
         "-fbdd N   0 - disable FBDD noise reduction (default), 1 - light FBDD, 2 - full\n"
         "-dcbi N   Number of extra DCD iterations (default - 0)\n"
         "-dcbe     DCB color enhance\n"
#ifdef LIBRAW_DEMOSAIC_PACK_GPL2
         "-eeci     EECI refine for mixed VCD/AHD (q=8)\n"
         "-esmed N  Number of edge-sensitive median filter passes (only if q=8)\n"
#endif
#ifdef LIBRAW_DEMOSAIC_PACK_GPL3
         //"-amazeca  Use AMaZE chromatic aberrations refine (only if q=10)\n"
         "-acae <r b>Use chromatic aberrations correction\n" // modifJD         
         "-aline <l> reduction of line noise\n"
         "-aclean <l c> clean CFA\n"
         "-agreen <g> equilibrate green\n"
#endif
         "-aexpo <e p> exposure correction\n"
         "-apentax4shot enables merge of 4-shot pentax files\n"
         "-apentax4shotorder 3102 sets pentax 4-shot alignment order\n"
         // WF
         "-dbnd <r g b g> debanding\n"
#ifndef WIN32
         "-mmap     Use mmap()-ed buffer instead of plain FILE I/O\n"
#endif
         "-mem    Use memory buffer instead of FILE I/O\n"
         "-disars   Do not use RawSpeed library\n"
         "-disinterp Do not run interpolation step\n"
         "-dsrawrgb1 Disable YCbCr to RGB conversion for sRAW (Cb/Cr interpolation enabled)\n"
         "-dsrawrgb2 Disable YCbCr to RGB conversion for sRAW (Cb/Cr interpolation disabled)\n"
         "-disadcf  Do not use dcraw Foveon code even if compiled with demosaic-pack-GPL2\n"
#ifdef USE_DNGSDK
         "-dngsdk   Use Adobe DNG SDK for DNG decode\n"
         "-dngflags N set DNG decoding options to value N\n"
#endif
  );
  exit(1);
}

static int verbosity = 0;
int cnt = 0;
int my_progress_callback(void *d, enum LibRaw_progress p, int iteration, int expected)
{
  char *passed = (char *)(d ? d : "default string"); // data passed to callback at set_callback stage

  if (verbosity > 2) // verbosity set by repeat -v switches
  {
    printf("CB: %s  pass %d of %d (data passed=%s)\n", libraw_strprogress(p), iteration, expected, passed);
  }
  else if (iteration == 0) // 1st iteration of each step
    printf("Starting %s (expecting %d iterations)\n", libraw_strprogress(p), expected);
  else if (iteration == expected - 1)
    printf("%s finished\n", libraw_strprogress(p));

  ///    if(++cnt>10) return 1; // emulate user termination on 10-th callback call

  return 0; // always return 0 to continue processing
}

// timer
#ifndef WIN32
static struct timeval start, end;
void timerstart(void) { gettimeofday(&start, NULL); }
void timerprint(const char *msg, const char *filename)
{
  gettimeofday(&end, NULL);
  float msec = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;
  printf("Timing: %s/%s: %6.3f msec\n", filename, msg, msec);
}
#else
LARGE_INTEGER start;
void timerstart(void) { QueryPerformanceCounter(&start); }
void timerprint(const char *msg, const char *filename)
{
  LARGE_INTEGER unit, end;
  QueryPerformanceCounter(&end);
  QueryPerformanceFrequency(&unit);
  float msec = (float)(end.QuadPart - start.QuadPart);
  msec /= (float)unit.QuadPart / 1000.0f;
  printf("Timing: %s/%s: %6.3f msec\n", filename, msg, msec);
}

#endif

int main(int argc, char *argv[])
{
  if (argc == 1)
    usage(argv[0]);

  LibRaw RawProcessor;
  int i, arg, c, ret;
  char opm, opt, *cp, *sp;
  int use_bigfile = 0, use_timing = 0, use_mem = 0;
#ifdef USE_DNGSDK
  dng_host *dnghost = NULL;
#endif
#ifndef WIN32
  int msize = 0, use_mmap = 0;

#endif
  void *iobuffer = 0;
#ifdef OUT
#undef OUT
#endif

#define OUT RawProcessor.imgdata.params

  argv[argc] = (char *)"";
  for (arg = 1; (((opm = argv[arg][0]) - 2) | 2) == '+';)
  {
    char *optstr = argv[arg];
    opt = argv[arg++][1];
    if ((cp = strchr(sp = (char *)"cnbrkStqmHABCgU", opt)) != 0)
      for (i = 0; i < "111411111144221"[cp - sp] - '0'; i++)
        if (!isdigit(argv[arg + i][0]) && !optstr[2])
        {
          fprintf(stderr, "Non-numeric argument to \"-%c\"\n", opt);
          return 1;
        }
    if (!strchr("ftdeam", opt) && argv[arg - 1][2])
      fprintf(stderr, "Unknown option \"%s\".\n", argv[arg - 1]);
    switch (opt)
    {
    case 'v':
      verbosity++;
      break;
    case 'G':
      OUT.green_matching = 1;
      break;
    case 'c':
      OUT.adjust_maximum_thr = (float)atof(argv[arg++]);
      break;
    case 'U':
      OUT.auto_bright_thr = (float)atof(argv[arg++]);
      break;
    case 'n':
      OUT.threshold = (float)atof(argv[arg++]);
      break;
    case 'b':
      OUT.bright = (float)atof(argv[arg++]);
      break;
    case 'P':
      OUT.bad_pixels = argv[arg++];
      break;
    case 'K':
      OUT.dark_frame = argv[arg++];
      break;
    case 'r':
      for (c = 0; c < 4; c++)
        OUT.user_mul[c] = (float)atof(argv[arg++]);
      break;
    case 'C':
      OUT.aber[0] = 1 / atof(argv[arg++]);
      OUT.aber[2] = 1 / atof(argv[arg++]);
      break;
    case 'g':
      OUT.gamm[0] = 1 / atof(argv[arg++]);
      OUT.gamm[1] = atof(argv[arg++]);
      break;
    case 'k':
      OUT.user_black = atoi(argv[arg++]);
      break;
    case 'S':
      OUT.user_sat = atoi(argv[arg++]);
      break;
    case 'R':
      OUT.raw_processing_options = atoi(argv[arg++]);
      break;
    case 't':
      if (!strcmp(optstr, "-timing"))
        use_timing = 1;
      else if (!argv[arg - 1][2])
        OUT.user_flip = atoi(argv[arg++]);
      else
        fprintf(stderr, "Unknown option \"%s\".\n", argv[arg - 1]);
      break;
    case 'q':
      OUT.user_qual = atoi(argv[arg++]);
      break;
    case 'm':
#ifndef WIN32
      if (!strcmp(optstr, "-mmap"))
        use_mmap = 1;
      else
#endif
      if (!strcmp(optstr, "-mem"))
        use_mem = 1;
      else
      {
        if (!argv[arg - 1][2])
          OUT.med_passes = atoi(argv[arg++]);
        else
          fprintf(stderr, "Unknown option \"%s\".\n", argv[arg - 1]);
      }
      break;
    case 'H':
      OUT.highlight = atoi(argv[arg++]);
      break;
    case 's':
      OUT.shot_select = abs(atoi(argv[arg++]));
      break;
    case 'o':
      if (isdigit(argv[arg][0]) && !isdigit(argv[arg][1]))
        OUT.output_color = atoi(argv[arg++]);
#ifndef NO_LCMS
      else
        OUT.output_profile = argv[arg++];
      break;
    case 'p':
      OUT.camera_profile = argv[arg++];
#endif
      break;
    case 'h':
      OUT.half_size = 1;
      break;
    case 'f':
      if (!strcmp(optstr, "-fbdd"))
        OUT.fbdd_noiserd = atoi(argv[arg++]);
      else
      {
      if (!argv[arg - 1][2])
        OUT.four_color_rgb = 1;
      else
        fprintf(stderr, "Unknown option \"%s\".\n", argv[arg - 1]);
      }
      break;
    case 'A':
      for (c = 0; c < 4; c++)
        OUT.greybox[c] = atoi(argv[arg++]);
      break;
    case 'B':
      for (c = 0; c < 4; c++)
        OUT.cropbox[c] = atoi(argv[arg++]);
      break;
    case 'a':
      if (!strcmp(optstr, "-aexpo"))
      {
        OUT.exp_correc = 1;
        OUT.exp_shift = (float)atof(argv[arg++]);
        OUT.exp_preser = (float)atof(argv[arg++]);
      }
      else if (!strcmp(optstr, "-apentax4shot"))
      {
        OUT.raw_processing_options |= LIBRAW_PROCESSING_PENTAX_PS_ALLFRAMES;
      }
      else if (!strcmp(optstr, "-apentax4shotorder"))
      {
        strncpy(OUT.p4shot_order, argv[arg++], 5);
      }
      else
#ifdef LIBRAW_DEMOSAIC_PACK_GPL3
      if (!strcmp(optstr, "-acae"))
      {
        OUT.ca_correc = 1;
        OUT.cared = (float)atof(argv[arg++]);
        OUT.cablue = (float)atof(argv[arg++]);
      }
      else if (!strcmp(optstr, "-aline"))
      {
        OUT.cfaline = 1;
        OUT.linenoise = (float)atof(argv[arg++]);
      }
      else if (!strcmp(optstr, "-aclean"))
      {
        OUT.cfa_clean = 1;
        OUT.lclean = (float)atof(argv[arg++]);
        OUT.cclean = (float)atof(argv[arg++]);
      }
      else if (!strcmp(optstr, "-agreen"))
      {
        OUT.cfa_green = 1;
        OUT.green_thresh = (float)atof(argv[arg++]);
      }
      else
#endif
      if (!argv[arg - 1][2])
        OUT.use_auto_wb = 1;
      else
      fprintf(stderr, "Unknown option \"%s\".\n", argv[arg - 1]);
      break;
    case 'w':
      OUT.use_camera_wb = 1;
      break;
    case 'M':
      OUT.use_camera_matrix = (opm == '+');
      break;
    case 'j':
      OUT.use_fuji_rotate = 0;
      break;
    case 'W':
      OUT.no_auto_bright = 1;
      break;
    case 'T':
      OUT.output_tiff = 1;
      break;
    case '4':
      OUT.gamm[0] = OUT.gamm[1] = OUT.no_auto_bright = 1; /* no break here! */
    case '6':
      OUT.output_bps = 16;
      break;
    case 'F':
      use_bigfile = 1;
      break;
    case 'd':
      if (!strcmp(optstr, "-dcbi"))
        OUT.dcb_iterations = atoi(argv[arg++]);
      else if (!strcmp(optstr, "-disars"))
        OUT.use_rawspeed = 0;
      else if (!strcmp(optstr, "-disadcf"))
        OUT.raw_processing_options |= LIBRAW_PROCESSING_FORCE_FOVEON_X3F;
      else if (!strcmp(optstr, "-disinterp"))
        OUT.no_interpolation = 1;
      else if (!strcmp(optstr, "-dcbe"))
        OUT.dcb_enhance_fl = 1;
      else if (!strcmp(optstr, "-dsrawrgb1"))
      {
        OUT.raw_processing_options |= LIBRAW_PROCESSING_SRAW_NO_RGB;
        OUT.raw_processing_options &= ~LIBRAW_PROCESSING_SRAW_NO_INTERPOLATE;
      }
      else if (!strcmp(optstr, "-dsrawrgb2"))
      {
        OUT.raw_processing_options &= ~LIBRAW_PROCESSING_SRAW_NO_RGB;
        OUT.raw_processing_options |= LIBRAW_PROCESSING_SRAW_NO_INTERPOLATE;
      }
      else if (!strcmp(optstr, "-dbnd"))
      {
        for (c = 0; c < 4; c++)
        OUT.wf_deband_treshold[c] = (float)atof(argv[arg++]);
        OUT.wf_debanding = 1;
      }
#ifdef USE_DNGSDK
      else if (!strcmp(optstr, "-dngsdk"))
      {
        dnghost = new dng_host;
        RawProcessor.set_dng_host(dnghost);
      }
      else if (!strcmp(optstr, "-dngflags"))
      {
        OUT.use_dngsdk = atoi(argv[arg++]);
      }
#endif
      else
        fprintf(stderr, "Unknown option \"%s\".\n", argv[arg - 1]);
      break;
#ifdef LIBRAW_DEMOSAIC_PACK_GPL2
    case 'e':
      if (!strcmp(optstr, "-eeci"))
        OUT.eeci_refine = 1;
      else if (!strcmp(optstr, "-esmed"))
        OUT.es_med_passes = atoi(argv[arg++]);
      else
        fprintf(stderr, "Unknown option \"%s\".\n", argv[arg - 1]);
      break;
#endif
    default:
      fprintf(stderr, "Unknown option \"-%c\".\n", opt);
      return 1;
    }
  }
#ifndef WIN32
  putenv((char *)"TZ=UTC"); // dcraw compatibility, affects TIFF datestamp field
#else
  _putenv((char *)"TZ=UTC"); // dcraw compatibility, affects TIFF datestamp field
#endif
#define P1 RawProcessor.imgdata.idata
#define S RawProcessor.imgdata.sizes
#define C RawProcessor.imgdata.color
#define T RawProcessor.imgdata.thumbnail
#define P2 RawProcessor.imgdata.other
   
#define mnLens RawProcessor.imgdata.lens.makernotes
#define exifLens RawProcessor.imgdata.lens
#define ShootingInfo RawProcessor.imgdata.shootinginfo
   
#define Canon RawProcessor.imgdata.makernotes.canon
#define Hasselblad RawProcessor.imgdata.makernotes.hasselblad
#define Fuji RawProcessor.imgdata.makernotes.fuji
#define Oly RawProcessor.imgdata.makernotes.olympus
   
  if (verbosity > 1)
    RawProcessor.set_progress_handler(my_progress_callback, (void *)"Sample data passed");
#ifdef LIBRAW_USE_OPENMP
  if (verbosity)
    printf("Using %d threads\n", omp_get_max_threads());
#endif

  for (; arg < argc; arg++)
  {
    char outfn[1024];

    if (verbosity)
      printf("Processing file %s\n", argv[arg]);

    timerstart();

#ifndef WIN32
    if (use_mmap)
    {
      int file = open(argv[arg], O_RDONLY);
      struct stat st;
      if (file < 0)
      {
        fprintf(stderr, "Cannot open %s: %s\n", argv[arg], strerror(errno));
        continue;
      }
      if (fstat(file, &st))
      {
        fprintf(stderr, "Cannot stat %s: %s\n", argv[arg], strerror(errno));
        close(file);
        continue;
      }
      int pgsz = getpagesize();
      msize = ((st.st_size + pgsz - 1) / pgsz) * pgsz;
      iobuffer = mmap(NULL, msize, PROT_READ, MAP_PRIVATE, file, 0);
      if (!iobuffer)
      {
        fprintf(stderr, "Cannot mmap %s: %s\n", argv[arg], strerror(errno));
        close(file);
        continue;
      }
      close(file);
      if ((ret = RawProcessor.open_buffer(iobuffer, st.st_size) != LIBRAW_SUCCESS))
      {
        fprintf(stderr, "Cannot open_buffer %s: %s\n", argv[arg], libraw_strerror(ret));
        continue; // no recycle b/c open file will recycle itself
      }
    }
    else
#endif
    if (use_mem)
    {
      int file = open(argv[arg], O_RDONLY | O_BINARY);
      struct stat st;
      if (file < 0)
      {
        fprintf(stderr, "Cannot open %s: %s\n", argv[arg], strerror(errno));
        continue;
      }
      if (fstat(file, &st))
      {
        fprintf(stderr, "Cannot stat %s: %s\n", argv[arg], strerror(errno));
        close(file);
        continue;
      }
      if (!(iobuffer = malloc(st.st_size)))
      {
        fprintf(stderr, "Cannot allocate %d kbytes for memory buffer\n", (int)(st.st_size / 1024));
        close(file);
        continue;
      }
      int rd;
      if (st.st_size != (rd = read(file, iobuffer, st.st_size)))
      {
        fprintf(stderr, "Cannot read %d bytes instead of  %d to memory buffer\n", (int)rd, (int)st.st_size);
        close(file);
        free(iobuffer);
        continue;
      }
      close(file);
      if ((ret = RawProcessor.open_buffer(iobuffer, st.st_size)) != LIBRAW_SUCCESS)
      {
        fprintf(stderr, "Cannot open_buffer %s: %s\n", argv[arg], libraw_strerror(ret));
        free(iobuffer);
        continue; // no recycle b/c open file will recycle itself
      }
    }
    else
    {
      if (use_bigfile)
        // force open_file switch to bigfile processing
        ret = RawProcessor.open_file(argv[arg], 1);
      else
        ret = RawProcessor.open_file(argv[arg]);

      if (ret != LIBRAW_SUCCESS)
      {
        fprintf(stderr, "Cannot open %s: %s\n", argv[arg], libraw_strerror(ret));
        continue; // no recycle b/c open_file will recycle itself
      }
    }

    if (use_timing)
      timerprint("LibRaw::open_file()", argv[arg]);

    timerstart();
    if ((ret = RawProcessor.unpack()) != LIBRAW_SUCCESS)
    {
      fprintf(stderr, "Cannot unpack %s: %s\n", argv[arg], libraw_strerror(ret));
      continue;
    }

    if (use_timing)
      timerprint("LibRaw::unpack()", argv[arg]);

    timerstart();
    if (LIBRAW_SUCCESS != (ret = RawProcessor.dcraw_process()))
    {
      fprintf(stderr, "Cannot do postpocessing on %s: %s\n", argv[arg], libraw_strerror(ret));
      if (LIBRAW_FATAL_ERROR(ret))
        continue;
    }
    if (use_timing)
      timerprint("LibRaw::dcraw_process()", argv[arg]);

    snprintf(outfn, sizeof(outfn), "%s.%s", argv[arg], OUT.output_tiff ? "tiff" : (P1.colors > 1 ? "ppm" : "pgm"));

    if (verbosity)
    {
      if ((ret = RawProcessor.adjust_sizes_info_only()))
      {
        fprintf(stderr, "Cannot decode %s: %s\n", argv[arg], libraw_strerror(ret));
        continue; // no recycle, open_file will recycle
      }

      printf("\nFilename: %s\n", argv[arg]);
      printf("Timestamp: %s", ctime(&(P2.timestamp)));
      printf("Camera: %s %s\n", P1.make, P1.model);
      if (ShootingInfo.BodySerial[0])
      {
        trimSpaces(ShootingInfo.BodySerial);
        printf("Body serial: %s\n", ShootingInfo.BodySerial);
      }
      if (P2.artist[0])
        printf("Owner: %s\n", P2.artist);
      if (P1.dng_version)
      {
        printf("DNG Version: ");
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
      printf("\tDriveMode: %d\n", ShootingInfo.DriveMode);
      printf("\tFocusMode: %d\n", ShootingInfo.FocusMode);
      printf("\tMeteringMode: %d\n", ShootingInfo.MeteringMode);
      printf("\tAFPoint: %d\n", ShootingInfo.AFPoint);
      printf("\tExposureMode: %d\n", ShootingInfo.ExposureMode);
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
        case 8:
           printf("4/3\n");
           break;
        default:
           printf("Unknown\n");
           break;
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
      printf("Shutter: ");
      if (P2.shutter > 0 && P2.shutter < 1)
        P2.shutter = (printf("1/"), 1 / P2.shutter);
      printf("%0.1f sec\n", P2.shutter);
      printf("Aperture: f/%0.1f\n", P2.aperture);
      printf("Focal length: %0.1f mm\n", P2.focal_len);
      printf("Flash exposure compensation: %0.2f EV\n", P2.FlashEC);
      if (C.profile)
        printf("Embedded ICC profile: yes, %d bytes\n", C.profile_length);
      else
        printf("Embedded ICC profile: no\n");
      if (C.baseline_exposure > -999.f)
        printf("Baseline exposure: %04.3f\n", C.baseline_exposure);
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
        printf("Thumb size:  %4d x %d\n", T.twidth, T.theight);
      printf("Full size:   %4d x %d\n", S.raw_width, S.raw_height);
      printf("Image size:  %4d x %d\n", S.width, S.height);
      printf("Output size: %4d x %d\n", S.iwidth, S.iheight);
      if (Canon.SensorWidth)
        printf("SensorWidth          = %d\n", Canon.SensorWidth);
      if (Canon.SensorHeight)
        printf("SensorHeight         = %d\n", Canon.SensorHeight);
      if (Canon.SensorLeftBorder)
        printf("SensorLeftBorder     = %d\n", Canon.SensorLeftBorder);
      if (Canon.SensorTopBorder)
        printf("SensorTopBorder      = %d\n", Canon.SensorTopBorder);
      if (Canon.SensorRightBorder)
        printf("SensorRightBorder    = %d\n", Canon.SensorRightBorder);
      if (Canon.SensorBottomBorder)
        printf("SensorBottomBorder   = %d\n", Canon.SensorBottomBorder);
      if (Canon.BlackMaskLeftBorder)
        printf("BlackMaskLeftBorder  = %d\n", Canon.BlackMaskLeftBorder);
      if (Canon.BlackMaskTopBorder)
        printf("BlackMaskTopBorder   = %d\n", Canon.BlackMaskTopBorder);
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
          putchar(P1.cdesc[RawProcessor.fcol(i >> 1, i & 1)]);
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
            printf("%6.4f\t", C.cmatrix[j][i]);          printf("\n");
        }
      }
      if (fabsf(C.ccm[0][0]) > 0)
      {
        printf("\nColor Correction Matrix:\n");
        for (int i = 0; i < P1.colors; i++)
        {
          for (int j = 0; j < P1.colors; j++)
            printf("%6.4f\t", C.ccm[j][i]);
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
         printf(" %f", C.pre_mul[c]);      printf("\n");
    }
    if (verbosity)
    {
      printf("Writing file %s\n\n"
             "Library encoder:  libRAW      0.19        [12 Aug 2017]\n"
             "                  libJPEG     9.3(9c)8bit [25 Jun 2017]\n"
             "                  libjasper   2.0.12      [01 Aug 2017]\n"
             "                  zlib        1.2.11.1b   [03 Jun 2017]\n"
             "                  DNG SDK     1.4         [03 Jun 2017]\n"
             "                  XMP exempi  2.5         [05 Aug 2017]\n"
             "                  libexpat    2.2.4       [19 Aug 2017]\n"
             "                  Compiled by Jamaika\n\n", outfn);
    }

    if (LIBRAW_SUCCESS != (ret = RawProcessor.dcraw_ppm_tiff_writer(outfn)))
      fprintf(stderr, "Cannot write %s: %s\n", outfn, libraw_strerror(ret));

#ifndef WIN32
    if (use_mmap && iobuffer)
    {
      munmap(iobuffer, msize);
      iobuffer = 0;
    }
#endif
    else if (use_mem && iobuffer)
    {
      free(iobuffer);
      iobuffer = 0;
    }

    RawProcessor.recycle(); // just for show this call
  }
#ifdef USE_DNGSDK
  if (dnghost)
    delete dnghost;
#endif
  return 0;
}
