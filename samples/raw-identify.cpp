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
#include <string>
#include <list>

#include "libraw/libraw.h"

#ifdef LIBRAW_WIN32_CALLS
#define snprintf _snprintf
#define strcasecmp stricmp
#define strncasecmp strnicmp
#endif

#ifndef LIBRAW_WIN32_CALLS
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#ifndef MAX_PATH
#define MAX_PATH PATH_MAX
#endif
#endif

struct starttime_t
{
#ifdef LIBRAW_WIN32_CALLS
	LARGE_INTEGER started;
#else
	struct timeval started;
#endif
};

#define P1 MyCoolRawProcessor.imgdata.idata
#define P2 MyCoolRawProcessor.imgdata.other
#define P3 MyCoolRawProcessor.imgdata.makernotes.common

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

void print_verbose(FILE*, LibRaw& MyCoolRawProcessor, std::string& fn);
void print_wbfun(FILE*, LibRaw& MyCoolRawProcessor, std::string& fn);
void print_compactfun(FILE*, LibRaw& MyCoolRawProcessor, std::string& fn);
void print_szfun(FILE*, LibRaw& MyCoolRawProcessor, std::string& fn);
void print_0fun(FILE*, LibRaw& MyCoolRawProcessor, std::string& fn);
void print_1fun(FILE*, LibRaw& MyCoolRawProcessor, std::string& fn);
void print_2fun(FILE*, LibRaw& MyCoolRawProcessor, std::string& fn);
void print_unpackfun(FILE*, LibRaw& MyCoolRawProcessor, int print_frame, std::string& fn);
void print_timer(FILE*, const starttime_t&, int c);

const char *EXIF_LightSources[] = {
    "Unknown",
    "Daylight",
    "Fluorescent",
    "Tungsten (Incandescent)",
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
    "ISO Studio Tungsten"
};
const int nEXIF_LightSources = sizeof(EXIF_LightSources) / sizeof(EXIF_LightSources[0]);

// clang-format off
/*
table of fluorescents:
12 = FL-D; Daylight fluorescent (D 5700K – 7100K) (F1,F5)
13 = FL-N; Day white fluorescent (N 4600K – 5400K) (F7,F8)
14 = FL-W; Cool white fluorescent (W 3900K – 4500K) (F2,F6, office, store,warehouse)
15 = FL-WW; White fluorescent (WW 3200K – 3700K) (F3, residential)
16 = FL-L; Soft/Warm white fluorescent (L 2600K - 3250K) (F4, kitchen, bath)
*/
//clang-format on

typedef struct
{
  unsigned long long id;
  char const *name;
} id2hr_t; // id to human readable

// clang-format off
static id2hr_t MountNames[] = {
    {LIBRAW_MOUNT_Unknown, "Undefined Mount or Fixed Lens"},
    {LIBRAW_MOUNT_IL_UM, "Interchangeable lens, mount unknown"},
    {LIBRAW_MOUNT_Alpa, "Alpa"},
    {LIBRAW_MOUNT_C, "C-mount"},
    {LIBRAW_MOUNT_Canon_EF_M, "Canon EF-M"},
    {LIBRAW_MOUNT_Canon_EF_S, "Canon EF-S"},
    {LIBRAW_MOUNT_Canon_EF, "Canon EF"},
    {LIBRAW_MOUNT_Canon_RF, "Canon RF"},
    {LIBRAW_MOUNT_Contax_N, "Contax N"},
    {LIBRAW_MOUNT_Contax645, "Contax 645"},
    {LIBRAW_MOUNT_DigitalBack, "Digital Back"},
    {LIBRAW_MOUNT_FixedLens, "Fixed Lens"},
    {LIBRAW_MOUNT_FT, "4/3"},
    {LIBRAW_MOUNT_Fuji_GF, "Fuji G"},  // Fujifilm G lenses, GFX cameras
    {LIBRAW_MOUNT_Fuji_GX, "Fuji GX"}, // Fujifilm GX680
    {LIBRAW_MOUNT_Fuji_X, "Fuji X"},
    {LIBRAW_MOUNT_Hasselblad_H, "Hasselblad H"}, // Hasselblad Hn cameras, HC & HCD lenses
    {LIBRAW_MOUNT_Hasselblad_V, "Hasselblad V"},
    {LIBRAW_MOUNT_Hasselblad_XCD, "Hasselblad XCD"},  // Hasselblad Xn cameras, XCD lenses
    {LIBRAW_MOUNT_Leica_L, "Leica L"}, // throat
    {LIBRAW_MOUNT_Leica_M, "Leica M"},
    {LIBRAW_MOUNT_Leica_R, "Leica R"},
    {LIBRAW_MOUNT_Leica_S, "Leica S"},
    {LIBRAW_MOUNT_Leica_SL, "Leica SL"}, // mounts on "L" throat
    {LIBRAW_MOUNT_Leica_TL, "Leica TL"}, // mounts on "L" throat
    {LIBRAW_MOUNT_LF, "Large format"},
    {LIBRAW_MOUNT_Mamiya67, "Mamiya RZ/RB"},
    {LIBRAW_MOUNT_Mamiya645, "Mamiya 645"},
    {LIBRAW_MOUNT_mFT, "m4/3"},
    {LIBRAW_MOUNT_Minolta_A, "Sony/Minolta A"},
    {LIBRAW_MOUNT_Nikon_CX, "Nikkor 1"},
    {LIBRAW_MOUNT_Nikon_F, "Nikkor F"},
    {LIBRAW_MOUNT_Nikon_Z, "Nikkor Z"},
    {LIBRAW_MOUNT_Pentax_645, "Pentax 645"},
    {LIBRAW_MOUNT_Pentax_K, "Pentax K"},
    {LIBRAW_MOUNT_Pentax_Q, "Pentax Q"},
    {LIBRAW_MOUNT_RicohModule, "Ricoh module"},
    {LIBRAW_MOUNT_Rollei_bayonet, "Rollei bayonet"}, // Leaf AFi, Sinar Hy-6 models
    {LIBRAW_MOUNT_Samsung_NX_M, "Samsung NX-M"},
    {LIBRAW_MOUNT_Samsung_NX, "Samsung NX"},
    {LIBRAW_MOUNT_Sigma_X3F, "Sigma SA/X3F"},
    {LIBRAW_MOUNT_Sony_E, "Sony E"},
};
#define nMounts (sizeof(MountNames) / sizeof(id2hr_t))

static id2hr_t FormatNames[] = {
    {LIBRAW_FORMAT_Unknown, "Unknown"},
    {LIBRAW_FORMAT_1div2p3INCH, "1/2.3\""},
    {LIBRAW_FORMAT_1div1p7INCH, "1/1.7\""},
    {LIBRAW_FORMAT_1INCH, "1\""},
    {LIBRAW_FORMAT_FT, "4/3"},
    {LIBRAW_FORMAT_APSC, "APS-C"}, // Canon: 22.3x14.9mm; Sony et al: 23.6-23.7x15.6mm
    {LIBRAW_FORMAT_Leica_DMR, "Leica DMR"}, // 26.4x 17.6mm
    {LIBRAW_FORMAT_APSH, "APS-H"},          // Canon: 27.9x18.6mm
    {LIBRAW_FORMAT_FF, "FF 35mm"},
    {LIBRAW_FORMAT_CROP645, "645 crop 44x33mm"},
    {LIBRAW_FORMAT_LeicaS, "Leica S 45x30mm"},
    {LIBRAW_FORMAT_3648, "48x36mm"},
    {LIBRAW_FORMAT_645, "6x4.5"},
    {LIBRAW_FORMAT_66, "6x6"},
    {LIBRAW_FORMAT_67, "6x7"},
    {LIBRAW_FORMAT_68, "6x8"},
    {LIBRAW_FORMAT_69, "6x9"},
    {LIBRAW_FORMAT_MF, "Medium Format"},
    {LIBRAW_FORMAT_LF, "Large format"},
    {LIBRAW_FORMAT_SigmaAPSC, "Sigma APS-C"}, //  Sigma Foveon X3 orig: 20.7x13.8mm
    {LIBRAW_FORMAT_SigmaMerrill, "Sigma Merrill"},
    {LIBRAW_FORMAT_SigmaAPSH, "Sigma APS-H"}, // Sigma "H" 26.7 x 17.9mm
};
#define nFormats (sizeof(FormatNames) / sizeof(id2hr_t))

static id2hr_t NikonCrops[] = {
    {0, "Uncropped"},     {1, "1.3x"},          {2, "DX"},
    {3, "5:4"},           {4, "3:2"},           {6, "16:9"},
    {8, "2.7x"},          {9, "DX Movie"},      {10, "1.3x Movie"},
    {11, "FX Uncropped"}, {12, "DX Uncropped"}, {15, "1.5x Movie"},
    {17, "1:1"},
};
#define nNikonCrops (sizeof(NikonCrops) / sizeof(id2hr_t))

static id2hr_t FujiCrops[] = {
    {0, "Uncropped"},
    {1, "GFX FF"},
    {2, "Sports Finder Mode"},
    {4, "Electronic Shutter 1.25x Crop"},
};
#define nFujiCrops (sizeof(FujiCrops) / sizeof(id2hr_t))

static id2hr_t FujiDriveModes[] = {
    {0, "Single Frame"},
    {1, "Continuous Low"},
    {2, "Continuous High"},
};
#define nFujiDriveModes (sizeof(FujiDriveModes) / sizeof(id2hr_t))

static id2hr_t AspectRatios[] = {
    {LIBRAW_IMAGE_ASPECT_UNKNOWN, "Unknown"}, {LIBRAW_IMAGE_ASPECT_3to2, "3:2"},
    {LIBRAW_IMAGE_ASPECT_1to1, "1:1"},        {LIBRAW_IMAGE_ASPECT_4to3, "4:3"},
    {LIBRAW_IMAGE_ASPECT_16to9, "16:9"},      {LIBRAW_IMAGE_ASPECT_5to4, "5:4"},
    {LIBRAW_IMAGE_ASPECT_OTHER, "Other"},
};
#define nAspectRatios (sizeof(AspectRatios) / sizeof(id2hr_t))
//clang-format on

id2hr_t *lookup_id2hr(unsigned long long id, id2hr_t *table, ushort nEntries)
{
  for (int k = 0; k < nEntries; k++)
    if (id == table[k].id)
      return &table[k];
  return 0;
}

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

struct file_mapping
{
	void *map;
	INT64 fsize;
#ifdef LIBRAW_WIN32_CALLS
	HANDLE fd,fd_map;
	file_mapping() : map(0), fsize(0), fd(INVALID_HANDLE_VALUE), fd_map(INVALID_HANDLE_VALUE){}
#else
	int  fd;
	file_mapping() : map(0), fsize(0), fd(-1){}
#endif
};



void timer_start(starttime_t& r)
{
#ifdef LIBRAW_WIN32_CALLS
	QueryPerformanceCounter(&r.started);
#else
	gettimeofday(&r.started,NULL);
#endif
}

double timer_elapsed(const starttime_t& start)
{
#ifdef LIBRAW_WIN32_CALLS
	LARGE_INTEGER ended, freq;
	QueryPerformanceCounter(&ended);
	QueryPerformanceFrequency(&freq);
	return double(ended.QuadPart - start.started.QuadPart) / double(freq.QuadPart);
#else
	struct timeval ended;
	gettimeofday(&ended, NULL);
	return double(ended.tv_sec - start.started.tv_sec) + double(ended.tv_usec - start.started.tv_usec) / 1000000.0;
#endif
}


void create_mapping(struct file_mapping& data, const std::string& fn)
{
#ifdef LIBRAW_WIN32_CALLS
	std::wstring fpath(fn.begin(), fn.end());
	if ((data.fd = CreateFileW(fpath.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0)) == INVALID_HANDLE_VALUE) return;
	LARGE_INTEGER fs;
	if(!GetFileSizeEx(data.fd,&fs)) return;
	data.fsize = fs.QuadPart;
	if((data.fd_map = ::CreateFileMapping(data.fd, 0, PAGE_READONLY, fs.HighPart, fs.LowPart, 0)) == INVALID_HANDLE_VALUE) return;
	data.map = MapViewOfFile(data.fd_map,FILE_MAP_READ,0,0,data.fsize);
#else
	struct stat stt;
	if ((data.fd = open(fn.c_str(), O_RDONLY)) < 0) return;
	if (fstat(data.fd, &stt) != 0) return;
	data.fsize = stt.st_size;
	data.map = mmap(0, data.fsize, PROT_READ | PROT_WRITE, MAP_PRIVATE, data.fd, 0);
	return;
#endif
}

void close_mapping(struct file_mapping& data)
{
#ifdef LIBRAW_WIN32_CALLS
	if (data.map) UnmapViewOfFile(data.map);
	if (data.fd_map != INVALID_HANDLE_VALUE) CloseHandle(data.fd_map);
	if (data.fd != INVALID_HANDLE_VALUE) CloseHandle(data.fd);
	data.map = 0;
	data.fsize = 0;
	data.fd = data.fd_map = INVALID_HANDLE_VALUE;
#else
	if (data.map)
		munmap(data.map, data.fsize);
	if (data.fd>=0)
		close(data.fd);
	data.map = 0;
	data.fsize = 0;
	data.fd = -1;
#endif
}

void print_usage(const char *pname)
{
	printf("Usage: %s [options] inputfiles\n", pname);
	printf("Options:\n"
		"\t-c\tcompact output\n"
		"\t-v\tverbose output\n"
		"\t-w\tprint white balance\n"
		"\t-u\tprint unpack function\n"
		"\t-f\tprint frame size (only w/ -u)\n"
		"\t-s\tprint output image size\n"
		"\t-h\tforce half-size mode (only for -s)\n"
		"\t-M\tdisable use of raw-embedded color data\n"
		"\t+M\tforce use of raw-embedded color data\n"
		"\t-L filename\tread input files list from filename\n"
		"\t-o filename\toutput to filename\n");
}

int main(int ac, char *av[])
{
  int ret;
  int verbose = 0, print_sz = 0, print_unpack = 0, print_frame = 0,
	  print_wb = 0, use_map = 0, use_timing = 0;
  struct file_mapping mapping;
  int compact = 0, print_0 = 0, print_1 = 0, print_2 = 0;
  LibRaw MyCoolRawProcessor;
  char *filelistfile = NULL;
  char *outputfilename = NULL;
  FILE *outfile = stdout;
  std::vector <std::string> filelist;
  starttime_t started;

  filelist.reserve(ac - 1);

  for (int i = 1; i < ac; i++)
  {
	  if (av[i][0] == '-')
	  {
		  if (!strcmp(av[i], "-c"))	compact++;
		  if (!strcmp(av[i], "-v"))	verbose++;
		  if (!strcmp(av[i], "-w"))  print_wb++;
		  if (!strcmp(av[i], "-u"))  print_unpack++;
		  if (!strcmp(av[i], "-m"))  use_map++;
		  if (!strcmp(av[i], "-t"))  use_timing++;
		  if (!strcmp(av[i], "-s"))  print_sz++;
		  if (!strcmp(av[i], "-h"))	O.half_size = 1;
		  if (!strcmp(av[i], "-f"))	print_frame++;
		  if (!strcmp(av[i], "-0")) print_0++;
		  if (!strcmp(av[i], "-1")) print_1++;
		  if (!strcmp(av[i], "-2")) print_2++;
		  if (!strcmp(av[i], "-M")) MyCoolRawProcessor.imgdata.params.use_camera_matrix = 0;
		  if (!strcmp(av[i], "-L") && i < ac - 1)
		  {
			  filelistfile = av[i + 1];
			  i++;
		  }
		  if (!strcmp(av[i], "-o") && i < ac - 1)
		  {
			  outputfilename = av[i + 1];
			  i++;
		  }
		  continue;
	  }
	  else if (!strcmp(av[i], "+M"))
	  {
		  MyCoolRawProcessor.imgdata.params.use_camera_matrix = 3;
		  continue;
	  }
	  filelist.push_back(av[i]);
  }
  if(filelistfile)
  {
	  char *p;
	  char path[MAX_PATH + 1];
	  FILE *f = fopen(filelistfile, "r");
	  if (f)
	  {
		  while (fgets(path,MAX_PATH,f))
		  {
			  if ((p = strchr(path, '\n'))) *p = 0;
			  if ((p = strchr(path, '\r'))) *p = 0;
			  filelist.push_back(path);
		  }
		  fclose(f);
	  }
  }
  if (filelist.size()<1)
  {
	  print_usage(av[0]);
	  return 1;
  }
  if (outputfilename)
	  outfile = fopen(outputfilename, "wt");

  timer_start(started);
  for (int i = 0; i < filelist.size();i++)
  {
	  if (use_map)
	  {
		  create_mapping(mapping, filelist[i]);
		  if (!mapping.map)
		  {
			  fprintf(stderr, "Cannot map %s\n", filelist[i].c_str());
			  close_mapping(mapping);
			  continue;
		  }

		  if ((ret = MyCoolRawProcessor.open_buffer(mapping.map, mapping.fsize)) != LIBRAW_SUCCESS)
		  {
			  fprintf(stderr, "Cannot decode %s: %s\n", filelist[i].c_str(), libraw_strerror(ret));
			  close_mapping(mapping);
			  continue;
		  }
	  }
    else
        if ((ret = MyCoolRawProcessor.open_file(filelist[i].c_str())) != LIBRAW_SUCCESS)
		{
			fprintf(stderr, "Cannot decode %s: %s\n", filelist[i].c_str(), libraw_strerror(ret));
			continue; // no recycle, open_file will recycle
		}

	  if (use_timing)
	  {
		  /* nothing!*/
	  }
	  else if (print_sz)
		  print_szfun(outfile, MyCoolRawProcessor, filelist[i]);
	  else if (print_0)
		  print_0fun(outfile, MyCoolRawProcessor, filelist[i]);
	  else if (print_1)
		  print_1fun(outfile, MyCoolRawProcessor, filelist[i]);
	  else if (print_2)
		  print_2fun(outfile, MyCoolRawProcessor, filelist[i]);
	  else if (verbose)
		  print_verbose(outfile, MyCoolRawProcessor, filelist[i]);
	  else if (print_unpack)
		  print_unpackfun(outfile, MyCoolRawProcessor, print_frame, filelist[i]);
	  else if (print_wb)
		  print_wbfun(outfile, MyCoolRawProcessor, filelist[i]);
	  else if (compact)
		  print_compactfun(outfile, MyCoolRawProcessor, filelist[i]);
	  else
		  fprintf(outfile, "%s is a %s %s image.\n", filelist[i].c_str(), P1.make, P1.model);

    MyCoolRawProcessor.recycle();
	if (use_map)
		close_mapping(mapping);
  } // endfor

  if (use_timing && filelist.size() > 0)
	  print_timer(outfile, started, filelist.size());
  return 0;
}

void print_timer(FILE* outfile, const starttime_t& started, int files)
{
	double elapsed = timer_elapsed(started);
	if (elapsed > 1.0)
		fprintf(outfile, "%d files processed in %5.3f sec, %5.3g files/sec\n", files, elapsed, files / elapsed);
	else if (elapsed > 0.001) // 1msec
	{
		double msec = elapsed * 1000.0;
		fprintf(outfile, "%d files processed in %5.3f msec, %5.3g files/sec\n", files, msec, files / elapsed);
	}
	else if (elapsed > 0.000001)
	{
		double usec = elapsed * 1000000.0;
		fprintf(outfile, "%d files processed in %5.3f usec, %5.3g files/sec\n", files, usec, files / elapsed);
	}
	else
		fprintf(outfile, "%d files processed, time too small to estimate\n", files);
}

void print_verbose(FILE* outfile, LibRaw& MyCoolRawProcessor, std::string& fn)
{
	id2hr_t *MountName, *FormatName, *Aspect, *Crop, *DriveMode;
	int ret;

	if ((ret = MyCoolRawProcessor.adjust_sizes_info_only()))
	{
		fprintf(outfile, "Cannot decode %s: %s\n", fn.c_str(), libraw_strerror(ret));
		return; // no recycle, open_file will recycle
	}

	fprintf(outfile, "\nFilename: %s\n", fn.c_str());
	if (C.OriginalRawFileName[0])
		fprintf(outfile, "OriginalRawFileName: =%s=\n", C.OriginalRawFileName);
	fprintf(outfile, "Timestamp: %s", ctime(&(P2.timestamp)));
	fprintf(outfile, "Camera: %s %s ID: 0x%llx\n", P1.make, P1.model, mnLens.CamID);
	fprintf(outfile, "Normalized Make/Model: =%s/%s= ", P1.normalized_make,
		P1.normalized_model);
	fprintf(outfile, "CamMaker ID: %d, ", P1.maker_index);
	const char *CamMakerName = LibRaw::cameramakeridx2maker(P1.maker_index);
	if (CamMakerName)
		fprintf(outfile, "%s\n", CamMakerName);
	else
		fprintf(outfile, "Undefined\n");
	{
		int i = 0;
		char sep[] = ", ";
		if (C.UniqueCameraModel[0]) {
			i++;
			fprintf(outfile, "UniqueCameraModel: =%s=", C.UniqueCameraModel);
		}
		if (C.LocalizedCameraModel[0]) {
			if (i) {
				fprintf(outfile, "%s", sep);
				i++;
			}
			fprintf(outfile, "LocalizedCameraModel: =%s=", C.LocalizedCameraModel);
		}
		if (i) {
			fprintf(outfile, "\n");
			i = 0;
		}
		if (C.ImageUniqueID[0]) {
			if (i) fprintf(outfile, "%s", sep);
			i++;
			fprintf(outfile, "ImageUniqueID: =%s=", C.ImageUniqueID);
		}
		if (C.RawDataUniqueID[0]) {
			if (i) fprintf(outfile, "%s", sep);
			i++;
			fprintf(outfile, "RawDataUniqueID: =%s=", C.RawDataUniqueID);
		}
		if (i) fprintf(outfile, "\n");
	}

	if (ShootingInfo.BodySerial[0] && strcmp(ShootingInfo.BodySerial, "0"))
	{
		trimSpaces(ShootingInfo.BodySerial);
		fprintf(outfile, "Body#: %s", ShootingInfo.BodySerial);
	}
	else if (C.model2[0] && (!strncasecmp(P1.normalized_make, "Kodak", 5)))
	{
		trimSpaces(C.model2);
		fprintf(outfile, "Body#: %s", C.model2);
	}
	if (ShootingInfo.InternalBodySerial[0])
	{
		trimSpaces(ShootingInfo.InternalBodySerial);
		fprintf(outfile, " BodyAssy#: %s", ShootingInfo.InternalBodySerial);
	}
	if (exifLens.LensSerial[0])
	{
		trimSpaces(exifLens.LensSerial);
		fprintf(outfile, " Lens#: %s", exifLens.LensSerial);
	}
	if (exifLens.InternalLensSerial[0])
	{
		trimSpaces(exifLens.InternalLensSerial);
		fprintf(outfile, " LensAssy#: %s", exifLens.InternalLensSerial);
	}
	if (P2.artist[0])
		fprintf(outfile, " Owner: %s\n", P2.artist);
	if (P1.dng_version)
	{
		fprintf(outfile, " DNG Version: ");
		for (int i = 24; i >= 0; i -= 8)
			fprintf(outfile, "%d%c", P1.dng_version >> i & 255, i ? '.' : '\n');
	}
	fprintf(outfile, "\nEXIF:\n");
	fprintf(outfile, "\tMinFocal: %0.1f mm\n", exifLens.MinFocal);
	fprintf(outfile, "\tMaxFocal: %0.1f mm\n", exifLens.MaxFocal);
	fprintf(outfile, "\tMaxAp @MinFocal: f/%0.1f\n", exifLens.MaxAp4MinFocal);
	fprintf(outfile, "\tMaxAp @MaxFocal: f/%0.1f\n", exifLens.MaxAp4MaxFocal);
	fprintf(outfile, "\tMaxAperture @CurFocal: f/%0.1f\n", exifLens.EXIF_MaxAp);
	fprintf(outfile, "\tFocalLengthIn35mmFormat: %d mm\n",
		exifLens.FocalLengthIn35mmFormat);
	fprintf(outfile, "\tLensMake: %s\n", exifLens.LensMake);
	fprintf(outfile, "\tLens: %s\n", exifLens.Lens);
	fprintf(outfile, "\n");

	fprintf(outfile, "\nMakernotes:\n");
	fprintf(outfile, "\tDriveMode: %d\n", ShootingInfo.DriveMode);
	fprintf(outfile, "\tFocusMode: %d\n", ShootingInfo.FocusMode);
	fprintf(outfile, "\tMeteringMode: %d\n", ShootingInfo.MeteringMode);
	fprintf(outfile, "\tAFPoint: %d\n", ShootingInfo.AFPoint);
	fprintf(outfile, "\tExposureMode: %d\n", ShootingInfo.ExposureMode);
	fprintf(outfile, "\tExposureProgram: %d\n", ShootingInfo.ExposureProgram);
	fprintf(outfile, "\tImageStabilization: %d\n", ShootingInfo.ImageStabilization);
	if (mnLens.body[0])
	{
		fprintf(outfile, "\tHost Body: %s\n", mnLens.body);
	}
	if (Hasselblad.CaptureSequenceInitiator[0])
	{
		fprintf(outfile, "\tInitiator: %s\n", Hasselblad.CaptureSequenceInitiator);
	}
	if (Hasselblad.SensorUnitConnector[0])
	{
		fprintf(outfile, "\tSU Connector: %s\n", Hasselblad.SensorUnitConnector);
	}
	fprintf(outfile, "\tCameraFormat: %d, ", mnLens.CameraFormat);
	FormatName = lookup_id2hr(mnLens.CameraFormat, FormatNames, nFormats);
	if (FormatName)
		fprintf(outfile, "%s\n", FormatName->name);
	else
		fprintf(outfile, "Unknown\n");

	if (!strncasecmp(P1.make, "Nikon", 5) && Nikon.SensorHighSpeedCrop.cwidth)
	{
		fprintf(outfile, "\tNikon crop: %d, ", Nikon.HighSpeedCropFormat);
		Crop = lookup_id2hr(Nikon.HighSpeedCropFormat, NikonCrops, nNikonCrops);
		if (Crop)
			fprintf(outfile, "%s\n", Crop->name);
		else
			fprintf(outfile, "Unknown\n");
		fprintf(outfile, "\tSensor used area %d x %d; crop from: %d x %d at top left "
			"pixel: (%d, %d)\n",
			Nikon.SensorWidth, Nikon.SensorHeight,
			Nikon.SensorHighSpeedCrop.cwidth,
			Nikon.SensorHighSpeedCrop.cheight,
			Nikon.SensorHighSpeedCrop.cleft, Nikon.SensorHighSpeedCrop.ctop);
	}

	fprintf(outfile, "\tCameraMount: %d, ", mnLens.CameraMount);
	MountName = lookup_id2hr(mnLens.CameraMount, MountNames, nMounts);
	if (MountName)
		fprintf(outfile, "%s\n", MountName->name);
	else
		fprintf(outfile, "Unknown\n");

	if (mnLens.LensID == -1)
		fprintf(outfile, "\tLensID: n/a\n");
	else
		fprintf(outfile, "\tLensID: %llu 0x%0llx\n", mnLens.LensID, mnLens.LensID);

	fprintf(outfile, "\tLens: %s\n", mnLens.Lens);
	fprintf(outfile, "\tLensFormat: %d, ", mnLens.LensFormat);
	FormatName = lookup_id2hr(mnLens.LensFormat, FormatNames, nFormats);
	if (FormatName)
		fprintf(outfile, "%s\n", FormatName->name);
	else
		fprintf(outfile, "Unknown\n");

	fprintf(outfile, "\tLensMount: %d, ", mnLens.LensMount);
	MountName = lookup_id2hr(mnLens.LensMount, MountNames, nMounts);
	if (MountName)
		fprintf(outfile, "%s\n", MountName->name);
	else
		fprintf(outfile, "Unknown\n");

	fprintf(outfile, "\tFocalType: %d, ", mnLens.FocalType);
	switch (mnLens.FocalType)
	{
	case LIBRAW_FT_UNDEFINED:
		fprintf(outfile, "Undefined\n");
		break;
	case LIBRAW_FT_PRIME_LENS:
		fprintf(outfile, "Prime lens\n");
		break;
	case LIBRAW_FT_ZOOM_LENS:
		fprintf(outfile, "Zoom lens\n");
		break;
	default:
		fprintf(outfile, "Unknown\n");
		break;
	}
	fprintf(outfile, "\tLensFeatures_pre: %s\n", mnLens.LensFeatures_pre);
	fprintf(outfile, "\tLensFeatures_suf: %s\n", mnLens.LensFeatures_suf);
	fprintf(outfile, "\tMinFocal: %0.1f mm\n", mnLens.MinFocal);
	fprintf(outfile, "\tMaxFocal: %0.1f mm\n", mnLens.MaxFocal);
	fprintf(outfile, "\tMaxAp @MinFocal: f/%0.1f\n", mnLens.MaxAp4MinFocal);
	fprintf(outfile, "\tMaxAp @MaxFocal: f/%0.1f\n", mnLens.MaxAp4MaxFocal);
	fprintf(outfile, "\tMinAp @MinFocal: f/%0.1f\n", mnLens.MinAp4MinFocal);
	fprintf(outfile, "\tMinAp @MaxFocal: f/%0.1f\n", mnLens.MinAp4MaxFocal);
	fprintf(outfile, "\tMaxAp: f/%0.1f\n", mnLens.MaxAp);
	fprintf(outfile, "\tMinAp: f/%0.1f\n", mnLens.MinAp);
	fprintf(outfile, "\tCurFocal: %0.1f mm\n", mnLens.CurFocal);
	fprintf(outfile, "\tCurAp: f/%0.1f\n", mnLens.CurAp);
	fprintf(outfile, "\tMaxAp @CurFocal: f/%0.1f\n", mnLens.MaxAp4CurFocal);
	fprintf(outfile, "\tMinAp @CurFocal: f/%0.1f\n", mnLens.MinAp4CurFocal);

	if (exifLens.makernotes.FocalLengthIn35mmFormat > 1.0f)
		fprintf(outfile, "\tFocalLengthIn35mmFormat: %0.1f mm\n",
		exifLens.makernotes.FocalLengthIn35mmFormat);

	if (exifLens.nikon.EffectiveMaxAp > 0.1f)
		fprintf(outfile, "\tEffectiveMaxAp: f/%0.1f\n",
		exifLens.nikon.EffectiveMaxAp);

	if (exifLens.makernotes.LensFStops > 0.1f)
		fprintf(outfile, "\tLensFStops @CurFocal: %0.2f\n",
		exifLens.makernotes.LensFStops);

	fprintf(outfile, "\tTeleconverterID: %lld\n", mnLens.TeleconverterID);
	fprintf(outfile, "\tTeleconverter: %s\n", mnLens.Teleconverter);
	fprintf(outfile, "\tAdapterID: %lld\n", mnLens.AdapterID);
	fprintf(outfile, "\tAdapter: %s\n", mnLens.Adapter);
	fprintf(outfile, "\tAttachmentID: %lld\n", mnLens.AttachmentID);
	fprintf(outfile, "\tAttachment: %s\n", mnLens.Attachment);
	fprintf(outfile, "\n");

	fprintf(outfile, "ISO speed: %d\n", (int)P2.iso_speed);
	if (P3.real_ISO > 0.1f)
		fprintf(outfile, "real ISO speed: %d\n", (int)P3.real_ISO);
	fprintf(outfile, "Shutter: ");
	if (P2.shutter > 0 && P2.shutter < 1)
		P2.shutter = fprintf(outfile, "1/%0.1f\n", 1.0f / P2.shutter);
	else if (P2.shutter >= 1)
		fprintf(outfile, "%0.1f sec\n", P2.shutter);
	else /* negative*/
		fprintf(outfile, " negative value\n");
	fprintf(outfile, "Aperture: f/%0.1f\n", P2.aperture);
	fprintf(outfile, "Focal length: %0.1f mm\n", P2.focal_len);
	if (P3.exifAmbientTemperature > -273.15f)
		fprintf(outfile, "Ambient temperature (exif data): %6.2f° C\n",
		P3.exifAmbientTemperature);
	if (P3.CameraTemperature > -273.15f)
		fprintf(outfile, "Camera temperature: %6.2f° C\n", P3.CameraTemperature);
	if (P3.SensorTemperature > -273.15f)
		fprintf(outfile, "Sensor temperature: %6.2f° C\n", P3.SensorTemperature);
	if (P3.SensorTemperature2 > -273.15f)
		fprintf(outfile, "Sensor temperature2: %6.2f° C\n", P3.SensorTemperature2);
	if (P3.LensTemperature > -273.15f)
		fprintf(outfile, "Lens temperature: %6.2f° C\n", P3.LensTemperature);
	if (P3.AmbientTemperature > -273.15f)
		fprintf(outfile, "Ambient temperature: %6.2f° C\n", P3.AmbientTemperature);
	if (P3.BatteryTemperature > -273.15f)
		fprintf(outfile, "Battery temperature: %6.2f° C\n", P3.BatteryTemperature);
	if (P3.FlashGN > 1.0f)
		fprintf(outfile, "Flash Guide Number: %6.2f\n", P3.FlashGN);
	fprintf(outfile, "Flash exposure compensation: %0.2f EV\n", P3.FlashEC);
	if (C.profile)
		fprintf(outfile, "Embedded ICC profile: yes, %d bytes\n", C.profile_length);
	else
		fprintf(outfile, "Embedded ICC profile: no\n");

	if (C.dng_levels.baseline_exposure > -999.f)
		fprintf(outfile, "Baseline exposure: %04.3f\n", C.dng_levels.baseline_exposure);

	fprintf(outfile, "Number of raw images: %d\n", P1.raw_count);

	if (Fuji.DriveMode != -1)
	{
		fprintf(outfile, "Fuji Drive Mode: %d, ", Fuji.DriveMode);
		DriveMode =
			lookup_id2hr(Fuji.DriveMode, FujiDriveModes, nFujiDriveModes);
		if (DriveMode)
			fprintf(outfile, "%s\n", DriveMode->name);
		else
			fprintf(outfile, "Unknown\n");
	}

	if (Fuji.CropMode)
	{
		fprintf(outfile, "Fuji Crop Mode: %d, ", Fuji.CropMode);
		Crop = lookup_id2hr(Fuji.CropMode, FujiCrops, nFujiCrops);
		if (Crop)
			fprintf(outfile, "%s\n", Crop->name);
		else
			fprintf(outfile, "Unknown\n");
	}

	if (Fuji.ExpoMidPointShift > -999.f)
		fprintf(outfile, "Fuji Exposure shift: %04.3f\n", Fuji.ExpoMidPointShift);

	if (Fuji.DynamicRange != 0xffff)
		fprintf(outfile, "Fuji Dynamic Range: %d\n", Fuji.DynamicRange);
	if (Fuji.FilmMode != 0xffff)
		fprintf(outfile, "Fuji Film Mode: %d\n", Fuji.FilmMode);
	if (Fuji.DynamicRangeSetting != 0xffff)
		fprintf(outfile, "Fuji Dynamic Range Setting: %d\n",
		Fuji.DynamicRangeSetting);
	if (Fuji.DevelopmentDynamicRange != 0xffff)
		fprintf(outfile, "Fuji Development Dynamic Range: %d\n",
		Fuji.DevelopmentDynamicRange);
	if (Fuji.AutoDynamicRange != 0xffff)
		fprintf(outfile, "Fuji Auto Dynamic Range: %d\n", Fuji.AutoDynamicRange);

	if (S.pixel_aspect != 1)
		fprintf(outfile, "Pixel Aspect Ratio: %0.6f\n", S.pixel_aspect);
	if (T.tlength)
		fprintf(outfile, "Thumb size:  %4d x %d\n", T.twidth, T.theight);
	fprintf(outfile, "Full size:   %4d x %d\n", S.raw_width, S.raw_height);

	if (S.raw_inset_crop.cwidth)
	{
		fprintf(outfile, "Raw inset, width x height: %4d x %d ", S.raw_inset_crop.cwidth,
			S.raw_inset_crop.cheight);
		if (S.raw_inset_crop.cleft != 0xffff)
			fprintf(outfile, "left: %d ", S.raw_inset_crop.cleft);
		if (S.raw_inset_crop.ctop != 0xffff)
			fprintf(outfile, "top: %d", S.raw_inset_crop.ctop);
		fprintf(outfile, "\n");
	}

	fprintf(outfile, "Aspect: ");
	Aspect =
		lookup_id2hr(S.raw_inset_crop.aspect, AspectRatios, nAspectRatios);
	if (Aspect)
		fprintf(outfile, "%s\n", Aspect->name);
	else
		fprintf(outfile, "Other %d\n", S.raw_inset_crop.aspect);

	fprintf(outfile, "Image size:  %4d x %d\n", S.width, S.height);
	fprintf(outfile, "Output size: %4d x %d\n", S.iwidth, S.iheight);
	fprintf(outfile, "Image flip: %d\n", S.flip);

	if (Canon.SensorWidth)
		fprintf(outfile, "SensorWidth          = %d\n", Canon.SensorWidth);
	if (Canon.SensorHeight)
		fprintf(outfile, "SensorHeight         = %d\n", Canon.SensorHeight);
	if (Canon.SensorLeftBorder != -1)
		fprintf(outfile, "SensorLeftBorder     = %d\n", Canon.SensorLeftBorder);
	if (Canon.SensorTopBorder != -1)
		fprintf(outfile, "SensorTopBorder      = %d\n", Canon.SensorTopBorder);
	if (Canon.SensorRightBorder)
		fprintf(outfile, "SensorRightBorder    = %d\n", Canon.SensorRightBorder);
	if (Canon.SensorBottomBorder)
		fprintf(outfile, "SensorBottomBorder   = %d\n", Canon.SensorBottomBorder);
	if (Canon.BlackMaskLeftBorder)
		fprintf(outfile, "BlackMaskLeftBorder  = %d\n", Canon.BlackMaskLeftBorder);
	if (Canon.BlackMaskTopBorder)
		fprintf(outfile, "BlackMaskTopBorder   = %d\n", Canon.BlackMaskTopBorder);
	if (Canon.BlackMaskRightBorder)
		fprintf(outfile, "BlackMaskRightBorder = %d\n", Canon.BlackMaskRightBorder);
	if (Canon.BlackMaskBottomBorder)
		fprintf(outfile, "BlackMaskBottomBorder= %d\n", Canon.BlackMaskBottomBorder);

	if (Hasselblad.BaseISO)
		fprintf(outfile, "Hasselblad base ISO: %d\n", Hasselblad.BaseISO);
	if (Hasselblad.Gain)
		fprintf(outfile, "Hasselblad gain: %g\n", Hasselblad.Gain);

	fprintf(outfile, "Raw colors: %d", P1.colors);
	if (P1.filters)
	{
		fprintf(outfile, "\nFilter pattern: ");
		if (!P1.cdesc[3])
			P1.cdesc[3] = 'G';
		for (int i = 0; i < 16; i++)
			putchar(P1.cdesc[MyCoolRawProcessor.fcol(i >> 1, i & 1)]);
	}

	if (Canon.ChannelBlackLevel[0])
		fprintf(outfile, "\nCanon makernotes, ChannelBlackLevel: %d %d %d %d\n",
		Canon.ChannelBlackLevel[0], Canon.ChannelBlackLevel[1],
		Canon.ChannelBlackLevel[2], Canon.ChannelBlackLevel[3]);

	if (C.black)
	{
		fprintf(outfile, "\nblack: %d", C.black);
	}
	if (C.cblack[0] != 0)
	{
		fprintf(outfile, "\ncblack[0 .. 3]:");
		for (int c = 0; c < 4; c++)
			fprintf(outfile, " %d", C.cblack[c]);
	}
	if ((C.cblack[4] * C.cblack[5]) > 0)
	{
		fprintf(outfile, "\nBlackLevelRepeatDim: %d x %d\n", C.cblack[4], C.cblack[4]);
		int n = C.cblack[4] * C.cblack[5];
		fprintf(outfile, "cblack[6 .. %d]:", 6 + n - 1);
		for (int c = 6; c < 6 + n; c++)
			fprintf(outfile, " %d", C.cblack[c]);
	}

	if (C.linear_max[0] != 0)
	{
		fprintf(outfile, "\nHighlight linearity limits:");
		for (int c = 0; c < 4; c++)
			fprintf(outfile, " %ld", C.linear_max[c]);
	}

	if (P1.colors > 1)
	{
		if (C.cam_mul[0] > 0)
		{
			fprintf(outfile, "\nMakernotes 'As shot' WB multipliers:");
			for (int c = 0; c < 4; c++)
				fprintf(outfile, " %f", C.cam_mul[c]);
		}

		for (int cnt = 0; cnt < nEXIF_LightSources; cnt++)
		{
			if (C.WB_Coeffs[cnt][0] > 0)
			{
				fprintf(outfile, "\nMakernotes '%s' WB multipliers:", EXIF_LightSources[cnt]);
				for (int c = 0; c < 4; c++)
					fprintf(outfile, " %d", C.WB_Coeffs[cnt][c]);
			}
		}
		if (C.WB_Coeffs[LIBRAW_WBI_Sunset][0] > 0)
		{
			fprintf(outfile, "\nMakernotes 'Sunset' multipliers:");
			for (int c = 0; c < 4; c++)
				fprintf(outfile, " %d", C.WB_Coeffs[LIBRAW_WBI_Sunset][c]);
		}

		if (C.WB_Coeffs[LIBRAW_WBI_Other][0] > 0)
		{
			fprintf(outfile, "\nMakernotes 'Other' multipliers:");
			for (int c = 0; c < 4; c++)
				fprintf(outfile, " %d", C.WB_Coeffs[LIBRAW_WBI_Other][c]);
		}

		if (C.WB_Coeffs[LIBRAW_WBI_Auto][0] > 0)
		{
			fprintf(outfile, "\nMakernotes 'Camera Auto' WB multipliers:");
			for (int c = 0; c < 4; c++)
				fprintf(outfile, " %d", C.WB_Coeffs[LIBRAW_WBI_Auto][c]);
		}
		if (C.WB_Coeffs[LIBRAW_WBI_Measured][0] > 0)
		{
			fprintf(outfile, "\nMakernotes 'Camera Measured' WB multipliers:");
			for (int c = 0; c < 4; c++)
				fprintf(outfile, " %d", C.WB_Coeffs[LIBRAW_WBI_Measured][c]);
		}

		if (C.WB_Coeffs[LIBRAW_WBI_Custom][0] > 0)
		{
			fprintf(outfile, "\nMakernotes 'Custom' WB multipliers:");
			for (int c = 0; c < 4; c++)
				fprintf(outfile, " %d", C.WB_Coeffs[LIBRAW_WBI_Custom][c]);
		}
		if (C.WB_Coeffs[LIBRAW_WBI_Custom1][0] > 0)
		{
			fprintf(outfile, "\nMakernotes 'Custom1' WB multipliers:");
			for (int c = 0; c < 4; c++)
				fprintf(outfile, " %d", C.WB_Coeffs[LIBRAW_WBI_Custom1][c]);
		}
		if (C.WB_Coeffs[LIBRAW_WBI_Custom2][0] > 0)
		{
			fprintf(outfile, "\nMakernotes 'Custom2' WB multipliers:");
			for (int c = 0; c < 4; c++)
				fprintf(outfile, " %d", C.WB_Coeffs[LIBRAW_WBI_Custom2][c]);
		}
		if (C.WB_Coeffs[LIBRAW_WBI_Custom3][0] > 0)
		{
			fprintf(outfile, "\nMakernotes 'Custom3' WB multipliers:");
			for (int c = 0; c < 4; c++)
				fprintf(outfile, " %d", C.WB_Coeffs[LIBRAW_WBI_Custom3][c]);
		}
		if (C.WB_Coeffs[LIBRAW_WBI_Custom4][0] > 0)
		{
			fprintf(outfile, "\nMakernotes 'Custom4' WB multipliers:");
			for (int c = 0; c < 4; c++)
				fprintf(outfile, " %d", C.WB_Coeffs[LIBRAW_WBI_Custom4][c]);
		}
		if (C.WB_Coeffs[LIBRAW_WBI_Custom5][0] > 0)
		{
			fprintf(outfile, "\nMakernotes 'Custom5' WB multipliers:");
			for (int c = 0; c < 4; c++)
				fprintf(outfile, " %d", C.WB_Coeffs[LIBRAW_WBI_Custom5][c]);
		}
		if (C.WB_Coeffs[LIBRAW_WBI_Custom6][0] > 0)
		{
			fprintf(outfile, "\nMakernotes 'Custom6' WB multipliers:");
			for (int c = 0; c < 4; c++)
				fprintf(outfile, " %d", C.WB_Coeffs[LIBRAW_WBI_Custom6][c]);
		}

		if ((Nikon.ME_WB[0] != 0.0f) && (Nikon.ME_WB[0] != 1.0f))
		{
			fprintf(outfile, "\nNikon multi-exposure WB multipliers:");
			for (int c = 0; c < 4; c++)
				fprintf(outfile, " %f", Nikon.ME_WB[c]);
		}

		if (C.rgb_cam[0][0] > 0.0001)
		{
			fprintf(outfile, "\n\nCamera2RGB matrix (mode: %d):\n", MyCoolRawProcessor.imgdata.params.use_camera_matrix);
			for (int i = 0; i < P1.colors; i++)
				fprintf(outfile, "%6.4f\t%6.4f\t%6.4f\n", C.rgb_cam[i][0], C.rgb_cam[i][1],
				C.rgb_cam[i][2]);
		}

		fprintf(outfile, "\nXYZ->CamRGB matrix:\n");
		for (int i = 0; i < P1.colors; i++)
			fprintf(outfile, "%6.4f\t%6.4f\t%6.4f\n", C.cam_xyz[i][0], C.cam_xyz[i][1],
			C.cam_xyz[i][2]);

    for (int cnt = 0; cnt < 2; cnt++) {
      if (fabsf(C.P1_color[cnt].romm_cam[0]) > 0)
      {
        fprintf(outfile, "\nPhaseOne Matrix %d:\n", cnt+1);
        for (int i = 0; i < 3; i++)
          fprintf(outfile, "%6.4f\t%6.4f\t%6.4f\n", C.P1_color[cnt].romm_cam[i * 3],
          C.P1_color[cnt].romm_cam[i * 3 + 1],
          C.P1_color[cnt].romm_cam[i * 3 + 2]);
      }
    }

		if (fabsf(C.cmatrix[0][0]) > 0)
		{
			fprintf(outfile, "\ncamRGB -> sRGB Matrix:\n");
			for (int i = 0; i < P1.colors; i++)
			{
				for (int j = 0; j < P1.colors; j++)
					fprintf(outfile, "%6.4f\t", C.cmatrix[i][j]);
				fprintf(outfile, "\n");
			}
		}

		if (fabsf(C.ccm[0][0]) > 0)
		{
			fprintf(outfile, "\nColor Correction Matrix:\n");
			for (int i = 0; i < P1.colors; i++)
			{
				for (int j = 0; j < P1.colors; j++)
					fprintf(outfile, "%6.4f\t", C.ccm[i][j]);
				fprintf(outfile, "\n");
			}
		}

    for (int cnt = 0; cnt < 2; cnt++) {
      if (C.dng_color[cnt].illuminant != LIBRAW_WBI_None) {
        if (C.dng_color[cnt].illuminant < nEXIF_LightSources) {
          fprintf(outfile, "\nDNG Illuminant %d: %s",
            cnt + 1, EXIF_LightSources[C.dng_color[cnt].illuminant]);
        }
        else if (C.dng_color[cnt].illuminant == LIBRAW_WBI_Other) {
          fprintf(outfile, "\nDNG Illuminant %d: Other", cnt + 1);
        }
        else {
          fprintf(outfile, "\nDNG Illuminant %d is out of EXIF LightSources range [0:24, 255]: %d",
            cnt + 1, C.dng_color[cnt].illuminant);
        }
      }
    }

    for (int n=0; n<2; n++) {
      if (fabsf(C.dng_color[n].colormatrix[0][0]) > 0)
      {
        fprintf(outfile, "\nDNG color matrix %d:\n", n+1);
        for (int i = 0; i < P1.colors; i++)
          fprintf(outfile, "%6.4f\t%6.4f\t%6.4f\n", C.dng_color[n].colormatrix[i][0],
          C.dng_color[n].colormatrix[i][1],
          C.dng_color[n].colormatrix[i][2]);
      }
    }

    for (int n=0; n<2; n++) {
      if (fabsf(C.dng_color[n].calibration[0][0]) > 0)
      {
        fprintf(outfile, "\nDNG calibration matrix %d:\n", n+1);
        for (int i = 0; i < P1.colors; i++)
        {
          for (int j = 0; j < P1.colors; j++)
            fprintf(outfile, "%6.4f\t", C.dng_color[n].calibration[j][i]);
          fprintf(outfile, "\n");
        }
      }
    }

    for (int n=0; n<2; n++) {
      if (fabsf(C.dng_color[n].forwardmatrix[0][0]) > 0)
      {
        fprintf(outfile, "\nDNG forward matrix %d:\n", n+1);
        for (int i = 0; i < P1.colors; i++)
          fprintf(outfile, "%6.4f\t%6.4f\t%6.4f\n", C.dng_color[n].forwardmatrix[0][i],
          C.dng_color[n].forwardmatrix[1][i],
          C.dng_color[n].forwardmatrix[2][i]);
      }
    }

		fprintf(outfile, "\nDerived D65 multipliers:");
		for (int c = 0; c < P1.colors; c++)
			fprintf(outfile, " %f", C.pre_mul[c]);
	}
	fprintf(outfile, "\n");

	if (Sony.PixelShiftGroupID)
	{
		fprintf(outfile, "\nSony PixelShiftGroupPrefix 0x%x PixelShiftGroupID %d, ",
			Sony.PixelShiftGroupPrefix, Sony.PixelShiftGroupID);
		if (Sony.numInPixelShiftGroup)
		{
			fprintf(outfile, "shot# %d (starts at 1) of total %d\n",
				Sony.numInPixelShiftGroup, Sony.nShotsInPixelShiftGroup);
		}
		else
		{
			fprintf(outfile, "shots in PixelShiftGroup %d, already ARQ\n",
				Sony.nShotsInPixelShiftGroup);
		}
	}

	if (Sony.Sony0x9400_version)
		fprintf(outfile, "\nSONY Sequence data, tag 0x9400 version '%x'\n"
		"\tReleaseMode2: %d\n"
		"\tSequenceImageNumber: %d (starts at zero)\n"
		"\tSequenceLength1: %d shot(s)\n"
		"\tSequenceFileNumber: %d (starts at zero, exiftool starts at 1)\n"
		"\tSequenceLength2: %d file(s)\n",
		Sony.Sony0x9400_version, Sony.Sony0x9400_ReleaseMode2,
		Sony.Sony0x9400_SequenceImageNumber,
		Sony.Sony0x9400_SequenceLength1,
		Sony.Sony0x9400_SequenceFileNumber,
		Sony.Sony0x9400_SequenceLength2);
}

void print_wbfun(FILE* outfile, LibRaw& MyCoolRawProcessor, std::string& fn)
{
	const char *CamMakerName = LibRaw::cameramakeridx2maker(P1.maker_index);
	fprintf(outfile, "// %s %s\n", P1.make, P1.model);
	for (int cnt = 0; cnt < nEXIF_LightSources; cnt++)
		if (C.WB_Coeffs[cnt][0])
		{
			fprintf(outfile, "{LIBRAW_CAMERAMAKER_%s, \"%s\", %d, {%6.5ff, 1.0f, %6.5ff, ", CamMakerName,
				P1.normalized_model, cnt,
				C.WB_Coeffs[cnt][0] / (float)C.WB_Coeffs[cnt][1],
				C.WB_Coeffs[cnt][2] / (float)C.WB_Coeffs[cnt][1]);
			if (C.WB_Coeffs[cnt][1] == C.WB_Coeffs[cnt][3])
				fprintf(outfile, "1.0f}},\n");
			else
				fprintf(outfile, "%6.5ff}},\n",
				C.WB_Coeffs[cnt][3] / (float)C.WB_Coeffs[cnt][1]);
		}
	if (C.WB_Coeffs[LIBRAW_WBI_Sunset][0])
	{
		fprintf(outfile, "{LIBRAW_CAMERAMAKER_%s, \"%s\", %d, {%6.5ff, 1.0f, %6.5ff, ", CamMakerName,
			P1.normalized_model, LIBRAW_WBI_Sunset,
			C.WB_Coeffs[LIBRAW_WBI_Sunset][0] /
			(float)C.WB_Coeffs[LIBRAW_WBI_Sunset][1],
			C.WB_Coeffs[LIBRAW_WBI_Sunset][2] /
			(float)C.WB_Coeffs[LIBRAW_WBI_Sunset][1]);
		if (C.WB_Coeffs[LIBRAW_WBI_Sunset][1] ==
			C.WB_Coeffs[LIBRAW_WBI_Sunset][3])
			fprintf(outfile, "1.0f}},\n");
		else
			fprintf(outfile, "%6.5ff}},\n", C.WB_Coeffs[LIBRAW_WBI_Sunset][3] /
			(float)C.WB_Coeffs[LIBRAW_WBI_Sunset][1]);
	}

	for (int cnt = 0; cnt < 64; cnt++)
		if (C.WBCT_Coeffs[cnt][0])
		{
			fprintf(outfile, "{LIBRAW_CAMERAMAKER_%s, \"%s\", %d, {%6.5ff, 1.0f, %6.5ff, ", CamMakerName,
				P1.normalized_model, (int)C.WBCT_Coeffs[cnt][0],
				C.WBCT_Coeffs[cnt][1] / C.WBCT_Coeffs[cnt][2],
				C.WBCT_Coeffs[cnt][3] / C.WBCT_Coeffs[cnt][2]);
			if (C.WBCT_Coeffs[cnt][2] == C.WBCT_Coeffs[cnt][4])
				fprintf(outfile, "1.0f}},\n");
			else
				fprintf(outfile, "%6.5ff}},\n",
				C.WBCT_Coeffs[cnt][4] / C.WBCT_Coeffs[cnt][2]);
		}
		else
			break;
	fprintf(outfile, "\n");
}

void print_szfun(FILE* outfile, LibRaw& MyCoolRawProcessor, std::string& fn)
{
	id2hr_t *Aspect;
	fprintf(outfile, "%s\t%s\t%s\t%d\t%d\n", fn.c_str(), P1.make, P1.model, S.width,
		S.height);
	/*
	fprintf(outfile, "\n%s\t%s\t%s\n", filelist[i].c_str(), P1.make, P1.model);
	fprintf(outfile, "\tCoffin:\traw %dx%d mrg %d %d img %dx%d\n", S.raw_width,
	S.raw_height, S.left_margin, S.top_margin, S.width, S.height); printf
	("\tmnote: \t"); if (Canon.SensorWidth) printf ("sensor %dx%d ",
	Canon.SensorWidth, Canon.SensorHeight); if (Nikon.SensorWidth) printf
	("sensor %dx%d ", Nikon.SensorWidth, Nikon.SensorHeight);
	fprintf(outfile, "inset: start %d %d img %dx%d aspect calc: %f Aspect in file:
	", S.raw_inset_crop.cleft, S.raw_inset_crop.ctop,
	S.raw_inset_crop.cwidth, S.raw_inset_crop.cheight,
	((float)S.raw_inset_crop.cwidth /
	(float)S.raw_inset_crop.cheight)); Aspect =
	lookup_id2hr(S.raw_inset_crop.aspect, AspectRatios, nAspectRatios); if
	(Aspect) printf ("%s\n", Aspect->name); else printf ("Other %d\n",
	S.raw_inset_crop.aspect);

	if (Nikon.SensorHighSpeedCrop.cwidth) {
	fprintf(outfile, "\tHighSpeed crop from: %d x %d at top left pixel: (%d,
	%d)\n", Nikon.SensorHighSpeedCrop.cwidth,
	Nikon.SensorHighSpeedCrop.cheight, Nikon.SensorHighSpeedCrop.cleft,
	Nikon.SensorHighSpeedCrop.ctop);
	}
	*/
}
void print_0fun(FILE* outfile, LibRaw& MyCoolRawProcessor, std::string& fn)
{
	/*
	printf ("filename\t%s\tmodel\t%s\tisTSNERDTS\t%d\tRAF
	version\t%s\tRAF data version\t0x%x\tis4K\t%d\n", av[i], P1.model,
	Fuji.isTSNERDTS, Fuji.RAFVersion, Fuji.RAFDataVersion,
	P2.is_4K_RAFdata);
	*/

}

void print_1fun(FILE* outfile, LibRaw& MyCoolRawProcessor, std::string& fn)
{
	char frame[64] = "";
	snprintf(frame, 64, "rw %d rh %d lm %d tm %d", S.raw_width, S.raw_height,
		S.left_margin, S.top_margin);
	fprintf(outfile, "%s=%s=nFms %02d=%s=bps %02d=%s", P1.normalized_make, P1.normalized_model, P1.raw_count,
		MyCoolRawProcessor.unpack_function_name(), C.raw_bps, frame);
	fprintf(outfile, "\n");
}

void print_2fun(FILE* outfile, LibRaw& MyCoolRawProcessor, std::string& fn)
{
	fprintf(outfile, "// %s %s", P1.make, P1.model);
	if (C.cam_mul[0] > 0)
	{
		fprintf(outfile, "\n'As shot' WB:");
		for (int c = 0; c < 4; c++)
			fprintf(outfile, " %.3f", C.cam_mul[c]);
	}
	if (C.WB_Coeffs[LIBRAW_WBI_Auto][0] > 0)
	{
		fprintf(outfile, "\n'Camera Auto' WB:");
		for (int c = 0; c < 4; c++)
			fprintf(outfile, " %d", C.WB_Coeffs[LIBRAW_WBI_Auto][c]);
	}
	if (C.WB_Coeffs[LIBRAW_WBI_Measured][0] > 0)
	{
		fprintf(outfile, "\n'Camera Measured' WB:");
		for (int c = 0; c < 4; c++)
			fprintf(outfile, " %d", C.WB_Coeffs[LIBRAW_WBI_Measured][c]);
	}
	fprintf(outfile, "\n\n");
}

void print_compactfun(FILE* outfile, LibRaw& MyCoolRawProcessor, std::string& fn)
{
	trimSpaces(P1.make);
	trimSpaces(P1.model);
	trimSpaces(C.model2);
	trimSpaces(ShootingInfo.BodySerial);
	trimSpaces(ShootingInfo.InternalBodySerial);
	fprintf(outfile, "%s=%s", P1.make, P1.model);
	if (ShootingInfo.BodySerial[0] &&
		!(ShootingInfo.BodySerial[0] == 48 && !ShootingInfo.BodySerial[1]))
		fprintf(outfile, "=Body#: %s", ShootingInfo.BodySerial);
	else if (C.model2[0] && (!strncasecmp(P1.normalized_make, "Kodak", 5)))
		fprintf(outfile, "=Body#: %s", C.model2);
	if (ShootingInfo.InternalBodySerial[0])
		fprintf(outfile, "=Assy#: %s", ShootingInfo.InternalBodySerial);
	if (exifLens.LensSerial[0])
		fprintf(outfile, "=Lens#: %s", exifLens.LensSerial);
	if (exifLens.InternalLensSerial[0])
		fprintf(outfile, "=LensAssy#: %s", exifLens.InternalLensSerial);
	fprintf(outfile, "=\n");
}

void print_unpackfun(FILE* outfile, LibRaw& MyCoolRawProcessor, int print_frame, std::string& fn)
{
	char frame[48] = "";
	if (print_frame)
	{
		ushort right_margin = S.raw_width - S.width - S.left_margin;
		ushort bottom_margin = S.raw_height - S.height - S.top_margin;
		snprintf(frame, 48, "F=%dx%dx%dx%d RS=%dx%d", S.left_margin,
			S.top_margin, right_margin, bottom_margin, S.raw_width,
			S.raw_height);
	}
	fprintf(outfile, "%s\t%s\t%s\t%s/%s\n", fn.c_str(),MyCoolRawProcessor.unpack_function_name(), frame, P1.make,	P1.model);
}
