/* -*- C++ -*-
 * File: identify.cpp
 * Copyright 2008-2020 LibRaw LLC (info@libraw.org)
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

#ifdef _MSC_VER
#if _MSC_VER < 1800 /* below MSVC 2013 */
float roundf(float f)
{
 return floorf(f + 0.5);
}

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

// clang-format off

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
void print_jsonfun(FILE*, LibRaw& MyCoolRawProcessor,
                   std::string& fn, int fnum, int nfiles);
void print_compactfun(FILE*, LibRaw& MyCoolRawProcessor, std::string& fn);
void print_normfun(FILE*, LibRaw& MyCoolRawProcessor, std::string& fn);
void print_szfun(FILE*, LibRaw& MyCoolRawProcessor, std::string& fn);
void print_0fun(FILE*, LibRaw& MyCoolRawProcessor, std::string& fn);
void print_1fun(FILE*, LibRaw& MyCoolRawProcessor, std::string& fn);
void print_2fun(FILE*, LibRaw& MyCoolRawProcessor, std::string& fn);
void print_unpackfun(FILE*, LibRaw& MyCoolRawProcessor, int print_frame, std::string& fn);
void print_timer(FILE*, const starttime_t&, int c);

/*
table of fluorescents:
12 = FL-D; Daylight fluorescent (D 5700K – 7100K) (F1,F5)
13 = FL-N; Day white fluorescent (N 4600K – 5400K) (F7,F8)
14 = FL-W; Cool white fluorescent (W 3900K – 4500K) (F2,F6, office, store,warehouse)
15 = FL-WW; White fluorescent (WW 3200K – 3700K) (F3, residential)
16 = FL-L; Soft/Warm white fluorescent (L 2600K - 3250K) (F4, kitchen, bath)
*/

static const struct {
    const int NumId;
    const char *StrId;
    const char *hrStrId; // human-readable
    const int aux_setting;
} WBToStr[] = {
    {LIBRAW_WBI_Unknown,         "WBI_Unknown",         "Unknown",                 0},
    {LIBRAW_WBI_Daylight,        "WBI_Daylight",        "Daylight",                0},
    {LIBRAW_WBI_Fluorescent,     "WBI_Fluorescent",     "Fluorescent",             0},
    {LIBRAW_WBI_Tungsten,        "WBI_Tungsten",        "Tungsten (Incandescent)", 0},
    {LIBRAW_WBI_Flash,           "WBI_Flash",           "Flash",                   0},
    {LIBRAW_WBI_FineWeather,     "WBI_FineWeather",     "Fine Weather",            0},
    {LIBRAW_WBI_Cloudy,          "WBI_Cloudy",          "Cloudy",                  0},
    {LIBRAW_WBI_Shade,           "WBI_Shade",           "Shade",                   0},
    {LIBRAW_WBI_FL_D,            "WBI_FL_D",            "Daylight Fluorescent",    0},
    {LIBRAW_WBI_FL_N,            "WBI_FL_N",            "Day White Fluorescent",   0},
    {LIBRAW_WBI_FL_W,            "WBI_FL_W",            "Cool White Fluorescent",  0},
    {LIBRAW_WBI_FL_WW,           "WBI_FL_WW",           "White Fluorescent",       0},
    {LIBRAW_WBI_FL_L,            "WBI_FL_L",            "Warm White Fluorescent",  0},
    {LIBRAW_WBI_Ill_A,           "WBI_Ill_A",           "Illuminant A",            0},
    {LIBRAW_WBI_Ill_B,           "WBI_Ill_B",           "Illuminant B",            0},
    {LIBRAW_WBI_Ill_C,           "WBI_Ill_C",           "Illuminant C",            0},
    {LIBRAW_WBI_D55,             "WBI_D55",             "D55",                     0},
    {LIBRAW_WBI_D65,             "WBI_D65",             "D65",                     0},
    {LIBRAW_WBI_D75,             "WBI_D75",             "D75",                     0},
    {LIBRAW_WBI_D50,             "WBI_D50",             "D50",                     0},
    {LIBRAW_WBI_StudioTungsten,  "WBI_StudioTungsten",  "ISO Studio Tungsten",     0},
    {LIBRAW_WBI_BW,              "WBI_BW",              "BW",                      0},
    {LIBRAW_WBI_Other,           "WBI_Other",           "Other",                   0},
    {LIBRAW_WBI_Sunset,          "WBI_Sunset",          "Sunset",                  1},
    {LIBRAW_WBI_Underwater,      "WBI_Underwater",      "Underwater",              1},
    {LIBRAW_WBI_FluorescentHigh, "WBI_FluorescentHigh", "Fluorescent High",        1},
    {LIBRAW_WBI_HT_Mercury,      "WBI_HT_Mercury",      "HT Mercury",              1},
    {LIBRAW_WBI_AsShot,          "WBI_AsShot",          "As Shot",                 1},
    {LIBRAW_WBI_Measured,        "WBI_Measured",        "Camera Measured",         1},
    {LIBRAW_WBI_Auto,            "WBI_Auto",            "Camera Auto",             1},
    {LIBRAW_WBI_Auto1,           "WBI_Auto1",           "Camera Auto 1",           1},
    {LIBRAW_WBI_Auto2,           "WBI_Auto2",           "Camera Auto 2",           1},
    {LIBRAW_WBI_Auto3,           "WBI_Auto3",           "Camera Auto 3",           1},
    {LIBRAW_WBI_Auto4,           "WBI_Auto4",           "Camera Auto 4",           1},
    {LIBRAW_WBI_Custom,          "WBI_Custom",          "Custom",                  1},
    {LIBRAW_WBI_Custom1,         "WBI_Custom1",         "Custom 1",                1},
    {LIBRAW_WBI_Custom2,         "WBI_Custom2",         "Custom 2",                1},
    {LIBRAW_WBI_Custom3,         "WBI_Custom3",         "Custom 3",                1},
    {LIBRAW_WBI_Custom4,         "WBI_Custom4",         "Custom 4",                1},
    {LIBRAW_WBI_Custom5,         "WBI_Custom5",         "Custom 5",                1},
    {LIBRAW_WBI_Custom6,         "WBI_Custom6",         "Custom 6",                1},
    {LIBRAW_WBI_PC_Set1,         "WBI_PC_Set1",         "PC Set 1",                1},
    {LIBRAW_WBI_PC_Set2,         "WBI_PC_Set2",         "PC Set 2",                1},
    {LIBRAW_WBI_PC_Set3,         "WBI_PC_Set3",         "PC Set 3",                1},
    {LIBRAW_WBI_PC_Set4,         "WBI_PC_Set4",         "PC Set 4",                1},
    {LIBRAW_WBI_PC_Set5,         "WBI_PC_Set5",         "PC Set 5",                1},
    {LIBRAW_WBI_Kelvin,          "WBI_Kelvin",          "Kelvin",                  1},
};

typedef struct
{
  unsigned long long id;
  char const *name;
} id2hr_t; // id to human readable

static id2hr_t MountNames[] = {
  {LIBRAW_MOUNT_Alpa,           "Alpa"},
  {LIBRAW_MOUNT_C,              "C-mount"},
  {LIBRAW_MOUNT_Canon_EF_M,     "Canon EF-M"},
  {LIBRAW_MOUNT_Canon_EF_S,     "Canon EF-S"},
  {LIBRAW_MOUNT_Canon_EF,       "Canon EF"},
  {LIBRAW_MOUNT_Canon_RF,       "Canon RF"},
  {LIBRAW_MOUNT_Contax_N,       "Contax N"},
  {LIBRAW_MOUNT_Contax645,      "Contax 645"},
  {LIBRAW_MOUNT_FT,             "4/3"},
  {LIBRAW_MOUNT_mFT,            "m4/3"},
  {LIBRAW_MOUNT_Fuji_GF,        "Fuji G"},  // Fujifilm G lenses, GFX cameras
  {LIBRAW_MOUNT_Fuji_GX,        "Fuji GX"}, // GX680
  {LIBRAW_MOUNT_Fuji_X,         "Fuji X"},
  {LIBRAW_MOUNT_Hasselblad_H,   "Hasselblad H"},    // Hn cameras, HC & HCD lenses
  {LIBRAW_MOUNT_Hasselblad_V,   "Hasselblad V"},
  {LIBRAW_MOUNT_Hasselblad_XCD, "Hasselblad XCD"}, // Xn cameras, XCD lenses
  {LIBRAW_MOUNT_Leica_M,        "Leica M"},
  {LIBRAW_MOUNT_Leica_R,        "Leica R"},
  {LIBRAW_MOUNT_Leica_S,        "Leica S"},
  {LIBRAW_MOUNT_Leica_SL,       "Leica SL"},     // mounts on "L" throat
  {LIBRAW_MOUNT_Leica_TL,       "Leica TL"},     // mounts on "L" throat
  {LIBRAW_MOUNT_LPS_L,          "LPS L-mount"},  // throat, Leica / Panasonic / Sigma
  {LIBRAW_MOUNT_Mamiya67,       "Mamiya RZ/RB"}, // Mamiya RB67, RZ67
  {LIBRAW_MOUNT_Mamiya645,      "Mamiya 645"},
  {LIBRAW_MOUNT_Minolta_A,      "Sony/Minolta A"},
  {LIBRAW_MOUNT_Nikon_CX,       "Nikkor 1"},
  {LIBRAW_MOUNT_Nikon_F,        "Nikkor F"},
  {LIBRAW_MOUNT_Nikon_Z,        "Nikkor Z"},
  {LIBRAW_MOUNT_Pentax_645,     "Pentax 645"},
  {LIBRAW_MOUNT_Pentax_K,       "Pentax K"},
  {LIBRAW_MOUNT_Pentax_Q,       "Pentax Q"},
  {LIBRAW_MOUNT_RicohModule,    "Ricoh module"},
  {LIBRAW_MOUNT_Rollei_bayonet, "Rollei bayonet"}, // Rollei Hy-6: Leaf AFi, Sinar Hy6- models
  {LIBRAW_MOUNT_Samsung_NX_M,   "Samsung NX-M"},
  {LIBRAW_MOUNT_Samsung_NX,     "Samsung NX"},
  {LIBRAW_MOUNT_Sigma_X3F,      "Sigma SA/X3F"},
  {LIBRAW_MOUNT_Sony_E,         "Sony E"},
// generic formats:
  {LIBRAW_MOUNT_LF,             "Large format"},
  {LIBRAW_MOUNT_DigitalBack,    "Digital Back"},
  {LIBRAW_MOUNT_FixedLens,      "Fixed Lens"},
  {LIBRAW_MOUNT_IL_UM,          "Interchangeable lens, mount unknown"},
  {LIBRAW_MOUNT_Unknown,        "Undefined Mount or Fixed Lens"},
  {LIBRAW_MOUNT_TheLastOne,     "The Last One"},
};

static id2hr_t FormatNames[] = {
  {LIBRAW_FORMAT_1div2p3INCH,  "1/2.3\""},
  {LIBRAW_FORMAT_1div1p7INCH,  "1/1.7\""},
  {LIBRAW_FORMAT_1INCH,        "1\""},
  {LIBRAW_FORMAT_FT,           "4/3"},
  {LIBRAW_FORMAT_APSC,         "APS-C"},     // Canon: 22.3x14.9mm; Sony et al: 23.6-23.7x15.6mm
  {LIBRAW_FORMAT_Leica_DMR,    "Leica DMR"}, // 26.4x 17.6mm
  {LIBRAW_FORMAT_APSH,         "APS-H"},     // Canon: 27.9x18.6mm
  {LIBRAW_FORMAT_FF,           "FF 35mm"},
  {LIBRAW_FORMAT_CROP645,      "645 crop 44x33mm"},
  {LIBRAW_FORMAT_LeicaS,       "Leica S 45x30mm"},
  {LIBRAW_FORMAT_3648,         "48x36mm"},
  {LIBRAW_FORMAT_645,          "6x4.5"},
  {LIBRAW_FORMAT_66,           "6x6"},
  {LIBRAW_FORMAT_67,           "6x7"},
  {LIBRAW_FORMAT_68,           "6x8"},
  {LIBRAW_FORMAT_69,           "6x9"},
  {LIBRAW_FORMAT_SigmaAPSC,    "Sigma APS-C"}, //  Sigma Foveon X3 orig: 20.7x13.8mm
  {LIBRAW_FORMAT_SigmaMerrill, "Sigma Merrill"},
  {LIBRAW_FORMAT_SigmaAPSH,    "Sigma APS-H"}, // Sigma "H" 26.7 x 17.9mm
  {LIBRAW_FORMAT_MF,           "Medium Format"},
  {LIBRAW_FORMAT_LF,           "Large format"},
  {LIBRAW_FORMAT_Unknown,      "Unknown"},
  {LIBRAW_FORMAT_TheLastOne,   "The Last One"},
};

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

static id2hr_t CanonRecordModes[] = {
    {LIBRAW_Canon_RecordMode_JPEG, "JPEG"},
    {LIBRAW_Canon_RecordMode_CRW_THM, "CRW+THM"},
    {LIBRAW_Canon_RecordMode_AVI_THM, "AVI+THM"},
    {LIBRAW_Canon_RecordMode_TIF, "TIF"},
    {LIBRAW_Canon_RecordMode_TIF_JPEG, "TIF+JPEG"},
    {LIBRAW_Canon_RecordMode_CR2, "CR2"},
    {LIBRAW_Canon_RecordMode_CR2_JPEG, "CR2+JPEG"},
    {LIBRAW_Canon_RecordMode_UNKNOWN, "Unknown"},
    {LIBRAW_Canon_RecordMode_MOV, "MOV"},
    {LIBRAW_Canon_RecordMode_MP4, "MP4"},
    {LIBRAW_Canon_RecordMode_CRM, "CRM"},
    {LIBRAW_Canon_RecordMode_CR3, "CR3"},
    {LIBRAW_Canon_RecordMode_CR3_JPEG, "CR3+JPEG"},
    {LIBRAW_Canon_RecordMode_HEIF, "HEIF"},
    {LIBRAW_Canon_RecordMode_CR3_HEIF, "CR3+HEIF"},
};
#define nCanonRecordModes LIBRAW_Canon_RecordMode_TheLastOne

static const struct {
    const int NumId;
    const char *StrId;
} CorpToStr[] = {
    {LIBRAW_CAMERAMAKER_Agfa,           "LIBRAW_CAMERAMAKER_Agfa"},
    {LIBRAW_CAMERAMAKER_Alcatel,        "LIBRAW_CAMERAMAKER_Alcatel"},
    {LIBRAW_CAMERAMAKER_Apple,          "LIBRAW_CAMERAMAKER_Apple"},
    {LIBRAW_CAMERAMAKER_Aptina,         "LIBRAW_CAMERAMAKER_Aptina"},
    {LIBRAW_CAMERAMAKER_AVT,            "LIBRAW_CAMERAMAKER_AVT"},
    {LIBRAW_CAMERAMAKER_Baumer,         "LIBRAW_CAMERAMAKER_Baumer"},
    {LIBRAW_CAMERAMAKER_Broadcom,       "LIBRAW_CAMERAMAKER_Broadcom"},
    {LIBRAW_CAMERAMAKER_Canon,          "LIBRAW_CAMERAMAKER_Canon"},
    {LIBRAW_CAMERAMAKER_Casio,          "LIBRAW_CAMERAMAKER_Casio"},
    {LIBRAW_CAMERAMAKER_CINE,           "LIBRAW_CAMERAMAKER_CINE"},
    {LIBRAW_CAMERAMAKER_Clauss,         "LIBRAW_CAMERAMAKER_Clauss"},
    {LIBRAW_CAMERAMAKER_Contax,         "LIBRAW_CAMERAMAKER_Contax"},
    {LIBRAW_CAMERAMAKER_Creative,       "LIBRAW_CAMERAMAKER_Creative"},
    {LIBRAW_CAMERAMAKER_DJI,            "LIBRAW_CAMERAMAKER_DJI"},
    {LIBRAW_CAMERAMAKER_DXO,            "LIBRAW_CAMERAMAKER_DXO"},
    {LIBRAW_CAMERAMAKER_Epson,          "LIBRAW_CAMERAMAKER_Epson"},
    {LIBRAW_CAMERAMAKER_Foculus,        "LIBRAW_CAMERAMAKER_Foculus"},
    {LIBRAW_CAMERAMAKER_Fujifilm,       "LIBRAW_CAMERAMAKER_Fujifilm"},
    {LIBRAW_CAMERAMAKER_Generic,        "LIBRAW_CAMERAMAKER_Generic"},
    {LIBRAW_CAMERAMAKER_Gione,          "LIBRAW_CAMERAMAKER_Gione"},
    {LIBRAW_CAMERAMAKER_GITUP,          "LIBRAW_CAMERAMAKER_GITUP"},
    {LIBRAW_CAMERAMAKER_Google,         "LIBRAW_CAMERAMAKER_Google"},
    {LIBRAW_CAMERAMAKER_GoPro,          "LIBRAW_CAMERAMAKER_GoPro"},
    {LIBRAW_CAMERAMAKER_Hasselblad,     "LIBRAW_CAMERAMAKER_Hasselblad"},
    {LIBRAW_CAMERAMAKER_HTC,            "LIBRAW_CAMERAMAKER_HTC"},
    {LIBRAW_CAMERAMAKER_I_Mobile,       "LIBRAW_CAMERAMAKER_I_Mobile"},
    {LIBRAW_CAMERAMAKER_Imacon,         "LIBRAW_CAMERAMAKER_Imacon"},
    {LIBRAW_CAMERAMAKER_Kodak,          "LIBRAW_CAMERAMAKER_Kodak"},
    {LIBRAW_CAMERAMAKER_Konica,         "LIBRAW_CAMERAMAKER_Konica"},
    {LIBRAW_CAMERAMAKER_Leaf,           "LIBRAW_CAMERAMAKER_Leaf"},
    {LIBRAW_CAMERAMAKER_Leica,          "LIBRAW_CAMERAMAKER_Leica"},
    {LIBRAW_CAMERAMAKER_Lenovo,         "LIBRAW_CAMERAMAKER_Lenovo"},
    {LIBRAW_CAMERAMAKER_LG,             "LIBRAW_CAMERAMAKER_LG"},
    {LIBRAW_CAMERAMAKER_Logitech,       "LIBRAW_CAMERAMAKER_Logitech"},
    {LIBRAW_CAMERAMAKER_Mamiya,         "LIBRAW_CAMERAMAKER_Mamiya"},
    {LIBRAW_CAMERAMAKER_Matrix,         "LIBRAW_CAMERAMAKER_Matrix"},
    {LIBRAW_CAMERAMAKER_Meizu,          "LIBRAW_CAMERAMAKER_Meizu"},
    {LIBRAW_CAMERAMAKER_Micron,         "LIBRAW_CAMERAMAKER_Micron"},
    {LIBRAW_CAMERAMAKER_Minolta,        "LIBRAW_CAMERAMAKER_Minolta"},
    {LIBRAW_CAMERAMAKER_Motorola,       "LIBRAW_CAMERAMAKER_Motorola"},
    {LIBRAW_CAMERAMAKER_NGM,            "LIBRAW_CAMERAMAKER_NGM"},
    {LIBRAW_CAMERAMAKER_Nikon,          "LIBRAW_CAMERAMAKER_Nikon"},
    {LIBRAW_CAMERAMAKER_Nokia,          "LIBRAW_CAMERAMAKER_Nokia"},
    {LIBRAW_CAMERAMAKER_Olympus,        "LIBRAW_CAMERAMAKER_Olympus"},
    {LIBRAW_CAMERAMAKER_OmniVison,      "LIBRAW_CAMERAMAKER_OmniVison"},
    {LIBRAW_CAMERAMAKER_Panasonic,      "LIBRAW_CAMERAMAKER_Panasonic"},
    {LIBRAW_CAMERAMAKER_Parrot,         "LIBRAW_CAMERAMAKER_Parrot"},
    {LIBRAW_CAMERAMAKER_Pentax,         "LIBRAW_CAMERAMAKER_Pentax"},
    {LIBRAW_CAMERAMAKER_PhaseOne,       "LIBRAW_CAMERAMAKER_PhaseOne"},
    {LIBRAW_CAMERAMAKER_PhotoControl,   "LIBRAW_CAMERAMAKER_PhotoControl"},
    {LIBRAW_CAMERAMAKER_Photron,        "LIBRAW_CAMERAMAKER_Photron"},
    {LIBRAW_CAMERAMAKER_Pixelink,       "LIBRAW_CAMERAMAKER_Pixelink"},
    {LIBRAW_CAMERAMAKER_Polaroid,       "LIBRAW_CAMERAMAKER_Polaroid"},
    {LIBRAW_CAMERAMAKER_RED,            "LIBRAW_CAMERAMAKER_RED"},
    {LIBRAW_CAMERAMAKER_Ricoh,          "LIBRAW_CAMERAMAKER_Ricoh"},
    {LIBRAW_CAMERAMAKER_Rollei,         "LIBRAW_CAMERAMAKER_Rollei"},
    {LIBRAW_CAMERAMAKER_RoverShot,      "LIBRAW_CAMERAMAKER_RoverShot"},
    {LIBRAW_CAMERAMAKER_Samsung,        "LIBRAW_CAMERAMAKER_Samsung"},
    {LIBRAW_CAMERAMAKER_Sigma,          "LIBRAW_CAMERAMAKER_Sigma"},
    {LIBRAW_CAMERAMAKER_Sinar,          "LIBRAW_CAMERAMAKER_Sinar"},
    {LIBRAW_CAMERAMAKER_SMaL,           "LIBRAW_CAMERAMAKER_SMaL"},
    {LIBRAW_CAMERAMAKER_Sony,           "LIBRAW_CAMERAMAKER_Sony"},
    {LIBRAW_CAMERAMAKER_ST_Micro,       "LIBRAW_CAMERAMAKER_ST_Micro"},
    {LIBRAW_CAMERAMAKER_THL,            "LIBRAW_CAMERAMAKER_THL"},
    {LIBRAW_CAMERAMAKER_Xiaomi,         "LIBRAW_CAMERAMAKER_Xiaomi"},
    {LIBRAW_CAMERAMAKER_XIAOYI,         "LIBRAW_CAMERAMAKER_XIAOYI"},
    {LIBRAW_CAMERAMAKER_YI,             "LIBRAW_CAMERAMAKER_YI"},
    {LIBRAW_CAMERAMAKER_Yuneec,         "LIBRAW_CAMERAMAKER_Yuneec"},
    {LIBRAW_CAMERAMAKER_Zeiss,          "LIBRAW_CAMERAMAKER_Zeiss"},
};

static const struct {
    const int NumId;
    const char *StrId;
} ColorSpaceToStr[] = {
    {LIBRAW_COLORSPACE_NotFound, "Not Found"},
    {LIBRAW_COLORSPACE_sRGB, "sRGB"},
    {LIBRAW_COLORSPACE_AdobeRGB, "Adobe RGB"},
    {LIBRAW_COLORSPACE_WideGamutRGB, "Wide Gamut RGB"},
    {LIBRAW_COLORSPACE_ProPhotoRGB, "ProPhoto RGB"},
    {LIBRAW_COLORSPACE_ICC, "ICC profile (embedded)"},
    {LIBRAW_COLORSPACE_Uncalibrated, "Uncalibrated"},
    {LIBRAW_COLORSPACE_CameraLinearUniWB, "Camera Linear, no WB"},
    {LIBRAW_COLORSPACE_CameraLinear, "Camera Linear"},
    {LIBRAW_COLORSPACE_CameraGammaUniWB, "Camera non-Linear, no WB"},
    {LIBRAW_COLORSPACE_CameraGamma, "Camera non-Linear"},
    {LIBRAW_COLORSPACE_MonochromeLinear, "Monochrome Linear"},
    {LIBRAW_COLORSPACE_MonochromeGamma, "Monochrome non-Linear"},
    {LIBRAW_COLORSPACE_Unknown, "Unknown"},
};

static const struct {
    const int NumId;
    const int LibRawId;
    const char *StrId;
} Fujifilm_WhiteBalance2Str[] = {
    {0x000, LIBRAW_WBI_Auto,       "Auto"},
    {0x100, LIBRAW_WBI_Daylight,   "Daylight"},
    {0x200, LIBRAW_WBI_Cloudy,     "Cloudy"},
    {0x300, LIBRAW_WBI_FL_D,       "Daylight Fluorescent"},
    {0x301, LIBRAW_WBI_FL_N,       "Day White Fluorescent"},
    {0x302, LIBRAW_WBI_FL_W,       "White Fluorescent"},
    {0x303, LIBRAW_WBI_FL_WW,      "Warm White Fluorescent"},
    {0x304, LIBRAW_WBI_FL_L,       "Living Room Warm White Fluorescent"},
    {0x400, LIBRAW_WBI_Tungsten,   "Incandescent"},
    {0x500, LIBRAW_WBI_Flash,      "Flash"},
    {0x600, LIBRAW_WBI_Underwater, "Underwater"},
    {0xf00, LIBRAW_WBI_Custom,     "Custom"},
    {0xf01, LIBRAW_WBI_Custom2,    "Custom2"},
    {0xf02, LIBRAW_WBI_Custom3,    "Custom3"},
    {0xf03, LIBRAW_WBI_Custom4,    "Custom4"},
    {0xf04, LIBRAW_WBI_Custom5,    "Custom5"},
    {0xff0, LIBRAW_WBI_Kelvin,     "Kelvin"},
};

static const struct {
    const int NumId;
    const char *StrId;
} Fujifilm_FilmModeToStr[] = {
    {0x000, "F0/Standard (Provia)"},
    {0x100, "F1/Studio Portrait"},
    {0x110, "F1a/Studio Portrait Enhanced Saturation"},
    {0x120, "F1b/Studio Portrait Smooth Skin Tone (Astia)"},
    {0x130, "F1c/Studio Portrait Increased Sharpness"},
    {0x200, "F2/Fujichrome (Velvia)"},
    {0x300, "F3/Studio Portrait Ex"},
    {0x400, "F4/Velvia"},
    {0x500, "Pro Neg. Std"},
    {0x501, "Pro Neg. Hi"},
    {0x600, "Classic Chrome"},
    {0x700, "Eterna"},
    {0x800, "Classic Negative"},
};

static const struct {
    const int NumId;
    const char *StrId;
} Fujifilm_DynamicRangeSettingToStr[] = {
    {0x0000, "Auto (100-400%)"},
    {0x0001, "Manual"},
    {0x0100, "Standard (100%)"},
    {0x0200, "Wide1 (230%)"},
    {0x0201, "Wide2 (400%)"},
    {0x8000, "Film Simulation"},
};

//clang-format on

id2hr_t *lookup_id2hr(unsigned long long id, id2hr_t *table, ushort nEntries)
{
  for (int k = 0; k < nEntries; k++)
    if (id == table[k].id)
      return &table[k];
  return 0;
}

const char *ColorSpace_idx2str(ushort ColorSpace) {
  for (unsigned i = 0; i < (sizeof ColorSpaceToStr / sizeof *ColorSpaceToStr); i++)
    if(ColorSpaceToStr[i].NumId == ColorSpace)
      return ColorSpaceToStr[i].StrId;
  return 0;
}

const char *CameraMaker_idx2str(unsigned maker) {
  for (unsigned i = 0; i < (sizeof CorpToStr / sizeof *CorpToStr); i++)
    if(CorpToStr[i].NumId == (int)maker)
      return CorpToStr[i].StrId;
  return 0;
}

const char *WB_idx2str(unsigned WBi) {
  for (int i = 0; i < int(sizeof WBToStr / sizeof *WBToStr); i++)
    if(WBToStr[i].NumId == (int)WBi)
      return WBToStr[i].StrId;
  return 0;
}

const char *WB_idx2hrstr(unsigned WBi) {
  for (int i = 0; i < int(sizeof WBToStr / sizeof *WBToStr); i++)
    if(WBToStr[i].NumId == (int)WBi)
      return WBToStr[i].hrStrId;
  return 0;
}

const char *Fujifilm_WhiteBalance_idx2str(ushort WB) {
  for (int i = 0; i < int(sizeof Fujifilm_WhiteBalance2Str / sizeof *Fujifilm_WhiteBalance2Str); i++)
    if(Fujifilm_WhiteBalance2Str[i].NumId == WB)
      return Fujifilm_WhiteBalance2Str[i].StrId;
  return 0;
}

const char *Fujifilm_FilmMode_idx2str(ushort FilmMode) {
  for (int i = 0; i < int(sizeof Fujifilm_FilmModeToStr / sizeof *Fujifilm_FilmModeToStr); i++)
    if(Fujifilm_FilmModeToStr[i].NumId == FilmMode)
      return Fujifilm_FilmModeToStr[i].StrId;
  return 0;
}

const char *Fujifilm_DynamicRangeSetting_idx2str(ushort DynamicRangeSetting) {
  for (int i = 0; i < int(sizeof Fujifilm_DynamicRangeSettingToStr / sizeof *Fujifilm_DynamicRangeSettingToStr); i++)
    if(Fujifilm_DynamicRangeSettingToStr[i].NumId == DynamicRangeSetting)
      return Fujifilm_DynamicRangeSettingToStr[i].StrId;
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
		"\t-n\tprint make/model and norm. make/model\n"
		"\t-v\tverbose output\n"
		"\t-w\tprint white balance\n"
		"\t-j\tprint JSON\n"
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
	  print_wb = 0, print_json = 0, use_map = 0, use_timing = 0;
  struct file_mapping mapping;
  int compact = 0, normalized = 0, print_0 = 0, print_1 = 0, print_2 = 0;
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
		  if (!strcmp(av[i], "-n"))	normalized++;
		  if (!strcmp(av[i], "-v"))	verbose++;
		  if (!strcmp(av[i], "-w")) print_wb++;
		  if (!strcmp(av[i], "-j")) print_json++;
		  if (!strcmp(av[i], "-u")) print_unpack++;
		  if (!strcmp(av[i], "-m")) use_map++;
		  if (!strcmp(av[i], "-t")) use_timing++;
		  if (!strcmp(av[i], "-s")) print_sz++;
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
  for (int i = 0; i < (int)filelist.size(); i++)
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
	  else if (normalized)
		  print_normfun(outfile, MyCoolRawProcessor, filelist[i]);
	  else if (print_json)
	    print_jsonfun(outfile, MyCoolRawProcessor, filelist[i], i, filelist.size());
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

void print_jsonfun(FILE* outfile, LibRaw& MyCoolRawProcessor,
                   std::string& fn, int fnum, int nfiles) {

  const int tab_width = 4;
  int n_tabs;
  const char tab_char = ' ';
  const char *CamMakerName = CameraMaker_idx2str(P1.maker_index);
  int WBi;
  int data_present = 0;

  if (fnum == 0) fprintf (outfile, "{\n");
  n_tabs = 1;
  fprintf (outfile, "%*c\"file_%05d\":{\n",
    n_tabs*tab_width, tab_char, fnum);
  n_tabs++;

  fprintf (outfile, "%*c\"file_name\":", n_tabs*tab_width, tab_char);
  if (fn.c_str()[0]) fprintf (outfile, "\"%s\",\n", fn.c_str());
  else fprintf (outfile, "null,\n");

  fprintf (outfile, "%*c\"cam_maker\":", n_tabs*tab_width, tab_char);
  if (CamMakerName[0]) fprintf (outfile, "\"%s\",\n", CamMakerName);
  else fprintf (outfile, "null,\n");

  fprintf (outfile, "%*c\"norm_model\":", n_tabs*tab_width, tab_char);
  if (P1.normalized_model[0]) fprintf (outfile, "\"%s\",\n", P1.normalized_model);
  else fprintf (outfile, "null,\n");

  fprintf (outfile, "%*c\"body_serial\":", n_tabs*tab_width, tab_char);
  if (ShootingInfo.BodySerial[0] &&
      strcmp(ShootingInfo.BodySerial, "0")) {
    trimSpaces(ShootingInfo.BodySerial);
    fprintf (outfile, "\"%s\",\n", ShootingInfo.BodySerial);
  } else if (C.model2[0] &&
             (!strncasecmp(P1.normalized_make, "Kodak", 5))) {
    trimSpaces(C.model2);
    fprintf (outfile, "\"%s\",\n", C.model2);
  } else fprintf (outfile, "null,\n");

  fprintf (outfile, "%*c\"int_serial\":", n_tabs*tab_width, tab_char);
  if (ShootingInfo.InternalBodySerial[0]) {
    trimSpaces(ShootingInfo.InternalBodySerial);
    fprintf (outfile, "\"%s\",\n", ShootingInfo.InternalBodySerial);
  } else fprintf (outfile, "null,\n");

  fprintf (outfile, "%*c\"dng\":%s,\n",
    n_tabs*tab_width, tab_char, P1.dng_version?"true":"false");

  fprintf (outfile, "%*c\"ISO\":", n_tabs*tab_width, tab_char);
  if (P2.iso_speed > 0.1f) fprintf (outfile, "%d,\n", int(P2.iso_speed));
  else fprintf (outfile, "null,\n");

  fprintf (outfile, "%*c\"BLE\":", n_tabs*tab_width, tab_char);
  if (int(C.dng_levels.baseline_exposure) != -999)
    fprintf (outfile, "%g,\n", C.dng_levels.baseline_exposure);
  else fprintf (outfile, "null,\n");

  fprintf (outfile, "%*c\"CameraCalibration\":", n_tabs*tab_width, tab_char);
	for (int n = 0; n < 2; n++) {
		if (fabsf(C.dng_color[n].calibration[0][0]) > 0) {
			int numel = 3;
			if (fabsf(C.dng_color[n].calibration[3][3]) > 0) numel = 4;
			if (!data_present) {
			  fprintf (outfile, "{");
			  n_tabs++;
			} else fprintf (outfile, ",");
			fprintf (outfile, "\n%*c\"%s\":",
			  n_tabs*tab_width, tab_char, WB_idx2str(C.dng_color[n].illuminant));
			for (int cnt = 0; cnt < numel; cnt++) {
				fprintf (outfile, "%s%g%s", (!cnt)?"[":"",
				  C.dng_color[n].calibration[cnt][cnt], (cnt < (numel - 1))?",":"]");
			}
			data_present++;
		}
	}
	if (data_present) {
	  data_present = 0;
	  n_tabs--;
	  fprintf (outfile, "\n%*c},\n", n_tabs*tab_width, tab_char);
	} else fprintf (outfile, "null,\n");

  fprintf (outfile, "%*c\"ColorMatrix\":", n_tabs*tab_width, tab_char);
	for (int n = 0; n < 2; n++) {
		if (fabsf(C.dng_color[n].colormatrix[0][0]) > 0) {
			int numel = 3;
			if (!data_present) {
			  fprintf (outfile, "{");
			  n_tabs++;
			} else fprintf (outfile, ",");
			fprintf (outfile, "\n%*c\"%s\":",
			  n_tabs*tab_width, tab_char, WB_idx2str(C.dng_color[n].illuminant));
			for (int i = 0; i < P1.colors; i++) {
				for (int cnt = 0; cnt < numel; cnt++) {
					fprintf (outfile, "%s%g%s", (!i&&!cnt)?"[":"",
					  C.dng_color[n].colormatrix[i][cnt],
					  ((i<(P1.colors-1)) || (cnt < (numel - 1)))?",":"]");
				}
			}
			data_present++;
		}
	}
	if (data_present) {
	  data_present = 0;
	  n_tabs--;
	  fprintf (outfile, "\n%*c},\n", n_tabs*tab_width, tab_char);
	} else fprintf (outfile, "null,\n");

  fprintf (outfile, "%*c\"WB data\":",
    n_tabs*tab_width, tab_char);

  if (C.cam_mul[0]) {
    if (!data_present) {
      fprintf (outfile, "{");
      n_tabs++;
    } else fprintf (outfile, ",");
    fprintf (outfile, "\n%*c\"%s\":",
      n_tabs*tab_width, tab_char, WB_idx2str(LIBRAW_WBI_AsShot));
    for (int i = 0; i < 4; i++) {
      fprintf (outfile, "%s%g%s", (!i)?"[":"",
        C.cam_mul[i], (i < 3)?",":"]");
    }
    data_present++;
  }

  for (int cnt = 0; cnt < int(sizeof WBToStr / sizeof *WBToStr); cnt++) {
    WBi = WBToStr[cnt].NumId;
    if (C.WB_Coeffs[WBi][0]) {
			if (!data_present) {
			  fprintf (outfile, "{");
			  n_tabs++;
			} else fprintf (outfile, ",");
      fprintf (outfile, "\n%*c\"%s\":",
        n_tabs*tab_width, tab_char, WBToStr[cnt].StrId);
      for (int i = 0; i < 4; i++) {
				fprintf (outfile, "%s%d%s", (!i)?"[":"",
				  C.WB_Coeffs[WBi][i], (i < 3)?",":"]");
      }
      data_present++;
    }
  }

  for (int cnt = 0; cnt < 64; cnt++) {
    if (C.WBCT_Coeffs[cnt][0]) {
			if (!data_present) {
			  fprintf (outfile, "{");
			  n_tabs++;
			} else fprintf (outfile, ",");
      fprintf (outfile, "\n%*c\"%g\":",
        n_tabs*tab_width, tab_char, C.WBCT_Coeffs[cnt][0]);
      for (int i = 1; i < 5; i++) {
				fprintf (outfile, "%s%g%s", (i == 1)?"[":"",
				  C.WBCT_Coeffs[cnt][i], (i < 4)?",":"]");
      }
      data_present++;
    }
  }

	if (data_present) {
	  data_present = 0;
	  n_tabs--;
	  fprintf (outfile, "\n%*c}\n", n_tabs*tab_width, tab_char);
	} else fprintf (outfile, "null\n");

  n_tabs--;
  fprintf (outfile, "%*c}\n", n_tabs*tab_width, tab_char);

  if ((fnum+1) == nfiles) fprintf (outfile, "}\n");
  else fprintf (outfile, ",");
}

#define PRINTMATRIX3x4(of,mat,clrs)																		\
	do{																								    \
		for(int r = 0; r < 3; r++)																		\
			if(clrs==4) 																				\
				fprintf(of, "%6.4f\t%6.4f\t%6.4f\t%6.4f\n", mat[r][0],mat[r][1],mat[r][2],mat[r][3]); 	\
			else																						\
				fprintf(of, "%6.4f\t%6.4f\t%6.4f\n", mat[r][0],mat[r][1],mat[r][2]);					\
	}while(0)

#define PRINTMATRIX4x3(of,mat,clrs)																		\
	do{																									\
		for(int r = 0; r < clrs && r < 4; r++)																	\
				fprintf(of, "%6.4f\t%6.4f\t%6.4f\n", mat[r][0],mat[r][1],mat[r][2]);					\
	}while(0)


void print_verbose(FILE* outfile, LibRaw& MyCoolRawProcessor, std::string& fn)
{
	id2hr_t *MountName, *FormatName, *Aspect, *Crop, *DriveMode;
	const char *CamMakerName = LibRaw::cameramakeridx2maker(P1.maker_index);
	const char *ColorSpaceName = ColorSpace_idx2str(P3.ColorSpace);
	int WBi;
	float denom;
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
	fprintf(outfile, "\tCurFocal: %0.1f mm\n", P2.focal_len);
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
	FormatName = lookup_id2hr(mnLens.CameraFormat, FormatNames, LIBRAW_FORMAT_TheLastOne);
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
	MountName = lookup_id2hr(mnLens.CameraMount, MountNames, LIBRAW_MOUNT_TheLastOne);
	if (MountName)
		fprintf(outfile, "%s\n", MountName->name);
	else
		fprintf(outfile, "Unknown\n");

	if (mnLens.LensID == 0xffffffff)
		fprintf(outfile, "\tLensID: n/a\n");
	else
		fprintf(outfile, "\tLensID: %llu 0x%0llx\n", mnLens.LensID, mnLens.LensID);

	fprintf(outfile, "\tLens: %s\n", mnLens.Lens);
	fprintf(outfile, "\tLensFormat: %d, ", mnLens.LensFormat);
	FormatName = lookup_id2hr(mnLens.LensFormat, FormatNames, LIBRAW_FORMAT_TheLastOne);
	if (FormatName)
		fprintf(outfile, "%s\n", FormatName->name);
	else
		fprintf(outfile, "Unknown\n");

	fprintf(outfile, "\tLensMount: %d, ", mnLens.LensMount);
	MountName = lookup_id2hr(mnLens.LensMount, MountNames, LIBRAW_MOUNT_TheLastOne);
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

  if (Fuji.WB_Preset != 0xffff)
    fprintf(outfile, "Fuji WB preset: 0x%03x, %s\n", Fuji.WB_Preset, Fujifilm_WhiteBalance_idx2str(Fuji.WB_Preset));
	if (Fuji.ExpoMidPointShift > -999.f) // tag 0x9650
		fprintf(outfile, "Fuji Exposure shift: %04.3f\n", Fuji.ExpoMidPointShift);
	if (Fuji.DynamicRange != 0xffff)
		fprintf(outfile, "Fuji Dynamic Range (0x1400): %d, %s\n",
		  Fuji.DynamicRange, Fuji.DynamicRange==1?"Standard":"Wide");
	if (Fuji.FilmMode != 0xffff)
		fprintf(outfile, "Fuji Film Mode (0x1401): 0x%03x, %s\n", Fuji.FilmMode, Fujifilm_FilmMode_idx2str(Fuji.FilmMode));
	if (Fuji.DynamicRangeSetting != 0xffff)
		fprintf(outfile, "Fuji Dynamic Range Setting (0x1402): 0x%04x, %s\n",
		Fuji.DynamicRangeSetting, Fujifilm_DynamicRangeSetting_idx2str(Fuji.DynamicRangeSetting));
	if (Fuji.DevelopmentDynamicRange != 0xffff)
		fprintf(outfile, "Fuji Development Dynamic Range (0x1403): %d\n",
		Fuji.DevelopmentDynamicRange);
	if (Fuji.AutoDynamicRange != 0xffff)
		fprintf(outfile, "Fuji Auto Dynamic Range (0x140b): %d\n", Fuji.AutoDynamicRange);
	if (Fuji.DRangePriority != 0xffff)
		fprintf(outfile, "Fuji Dynamic Range priority (0x1443): %d, %s\n",
		   Fuji.DRangePriority, Fuji.DRangePriority?"Fixed":"Auto");
	if (Fuji.DRangePriorityAuto)
		fprintf(outfile, "Fuji Dynamic Range priority Auto (0x1444): %d, %s\n",
		   Fuji.DRangePriorityAuto, Fuji.DRangePriorityAuto==1?"Weak":"Strong");
	if (Fuji.DRangePriorityFixed)
		fprintf(outfile, "Fuji Dynamic Range priority Fixed (0x1445): %d, %s\n",
		   Fuji.DRangePriorityFixed, Fuji.DRangePriorityFixed==1?"Weak":"Strong");

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

	if (Canon.RecordMode) {
	  id2hr_t *RecordMode =
	    lookup_id2hr(Canon.RecordMode, CanonRecordModes, nCanonRecordModes);
	  fprintf(outfile, "Canon record mode: %d, %s\n", Canon.RecordMode, RecordMode->name);
	}
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
		fprintf(outfile, "\nCanon makernotes, ChannelBlackLevel: %d %d %d %d",
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
		fprintf(outfile, "\nBlackLevelRepeatDim: %d x %d\n", C.cblack[4], C.cblack[5]);
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
		fprintf(outfile, "\nMakernotes WB data:               coeffs                  EVs");
		if ((C.cam_mul[0] > 0) && (C.cam_mul[1] > 0)) {
      fprintf(outfile, "\n  %-23s   %g %g %g %g   %5.2f %5.2f %5.2f %5.2f",
		          "As shot",
		          C.cam_mul[0], C.cam_mul[1], C.cam_mul[2], C.cam_mul[3],
		          roundf(log2(C.cam_mul[0] / C.cam_mul[1])*100.0f)/100.0f,
		          0.0f,
		          roundf(log2(C.cam_mul[2] / C.cam_mul[1])*100.0f)/100.0f,
		          C.cam_mul[3]?roundf(log2(C.cam_mul[3] / C.cam_mul[1])*100.0f)/100.0f:0.0f);
		}

		for (int cnt = 0; cnt < int(sizeof WBToStr / sizeof *WBToStr); cnt++) {
			WBi = WBToStr[cnt].NumId;
			if ((C.WB_Coeffs[WBi][0] > 0) && (C.WB_Coeffs[WBi][1] > 0)) {
        denom = (float)C.WB_Coeffs[WBi][1];
        fprintf(outfile, "\n  %-23s   %4d %4d %4d %4d   %5.2f %5.2f %5.2f %5.2f",
		            WBToStr[cnt].hrStrId,
		            C.WB_Coeffs[WBi][0], C.WB_Coeffs[WBi][1], C.WB_Coeffs[WBi][2], C.WB_Coeffs[WBi][3],
		            roundf(log2((float)C.WB_Coeffs[WBi][0] / denom)*100.0f)/100.0f,
		            0.0f,
		            roundf(log2((float)C.WB_Coeffs[WBi][2] / denom)*100.0f)/100.0f,
		            C.WB_Coeffs[3]?roundf(log2((float)C.WB_Coeffs[WBi][3] / denom)*100.0f)/100.0f:0.0f);
			}
		}

		if ((Nikon.ME_WB[0] != 0.0f) && (Nikon.ME_WB[0] != 1.0f))
		{
			fprintf(outfile, "\nNikon multi-exposure WB multipliers:");
			for (int c = 0; c < 4; c++)
				fprintf(outfile, " %g", Nikon.ME_WB[c]);
		}

		if (C.rgb_cam[0][0] > 0.0001)
		{
			fprintf(outfile, "\n\nCamera2RGB matrix (mode: %d):\n", MyCoolRawProcessor.imgdata.params.use_camera_matrix);
            PRINTMATRIX3x4(outfile,C.rgb_cam,P1.colors);
		}

		fprintf(outfile, "\nXYZ->CamRGB matrix:\n");
        PRINTMATRIX4x3(outfile,C.cam_xyz,P1.colors);

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
            PRINTMATRIX3x4(outfile,C.cmatrix,P1.colors);
		}

		if (fabsf(C.ccm[0][0]) > 0)
		{
			fprintf(outfile, "\nColor Correction Matrix:\n");
            PRINTMATRIX3x4(outfile,C.ccm,P1.colors);
		}

    for (int cnt = 0; cnt < 2; cnt++) {
      if (C.dng_color[cnt].illuminant != LIBRAW_WBI_None) {
        if (C.dng_color[cnt].illuminant <= LIBRAW_WBI_StudioTungsten) {
          fprintf(outfile, "\nDNG Illuminant %d: %s",
            cnt + 1, WB_idx2hrstr(C.dng_color[cnt].illuminant));
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
        PRINTMATRIX4x3(outfile,C.dng_color[n].colormatrix,P1.colors);
      }
    }

    for (int n=0; n<2; n++) {
      if (fabsf(C.dng_color[n].calibration[0][0]) > 0)
      {
        fprintf(outfile, "\nDNG calibration matrix %d:\n", n+1);
        for (int i = 0; i < P1.colors && i < 4; i++)
        {
          for (int j = 0; j < P1.colors && j < 4; j++)
            fprintf(outfile, "%6.4f\t", C.dng_color[n].calibration[j][i]);
          fprintf(outfile, "\n");
        }
      }
    }

    for (int n=0; n<2; n++) {
      if (fabsf(C.dng_color[n].forwardmatrix[0][0]) > 0)
      {
        fprintf(outfile, "\nDNG forward matrix %d:\n", n+1);
        PRINTMATRIX3x4(outfile,C.dng_color[n].forwardmatrix,P1.colors);
      }
    }

		fprintf(outfile, "\nDerived D65 multipliers:");
		for (int c = 0; c < P1.colors; c++)
			fprintf(outfile, " %f", C.pre_mul[c]);
	}
	fprintf(outfile, "\nColor space (makernotes) : %d, %s", P3.ColorSpace, ColorSpaceName);
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
	int WBi;
	float denom;
	const char *CamMakerName = CameraMaker_idx2str(P1.maker_index);
	fprintf(outfile, "// %s %s\n", P1.make, P1.model);
	for (int cnt = 0; cnt < int(sizeof WBToStr / sizeof *WBToStr); cnt++) {
		WBi = WBToStr[cnt].NumId;
		if (C.WB_Coeffs[WBi][0] && C.WB_Coeffs[WBi][1] && !WBToStr[cnt].aux_setting) {
      denom = (float) C.WB_Coeffs[WBi][1];
      fprintf(outfile, "{%s, \"%s\", %s, {%6.5ff, 1.0f, %6.5ff, ", CamMakerName,
        P1.normalized_model, WBToStr[cnt].StrId,
        C.WB_Coeffs[WBi][0] / denom,
        C.WB_Coeffs[WBi][2] / denom);
      if (C.WB_Coeffs[WBi][1] == C.WB_Coeffs[WBi][3])
        fprintf(outfile, "1.0f}},\n");
      else
        fprintf(outfile, "%6.5ff}},\n",
        C.WB_Coeffs[WBi][3] / denom);
    }
  }

	for (int cnt = 0; cnt < 64; cnt++)
		if (C.WBCT_Coeffs[cnt][0])
		{
			fprintf(outfile, "{%s, \"%s\", %d, {%6.5ff, 1.0f, %6.5ff, ", CamMakerName,
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
	fprintf(outfile, "%s\t%s\t%s\t%d\t%d\n", fn.c_str(), P1.make, P1.model, S.width,
		S.height);
	/*
	fprintf(outfile, "\n%s\t%s\t%s\n", filelist[i].c_str(), P1.make, P1.model);
	fprintf(outfile, "\tCoffin:\traw %dx%d mrg %d %d img %dx%d\n",
	  S.raw_width, S.raw_height, S.left_margin, S.top_margin, S.width, S.height);
	fprintf (outfile, "\tmnote: \t");
	if (Canon.SensorWidth)
	  fprintf (outfile, "sensor %dx%d ", Canon.SensorWidth, Canon.SensorHeight);
	if (Nikon.SensorWidth)
	  fprintf (outfile, "sensor %dx%d ", Nikon.SensorWidth, Nikon.SensorHeight);
	fprintf(outfile, "inset: start %d %d img %dx%d aspect calc: %f Aspect in file:",
	  S.raw_inset_crop.cleft, S.raw_inset_crop.ctop,
	  S.raw_inset_crop.cwidth, S.raw_inset_crop.cheight,
	  ((float)S.raw_inset_crop.cwidth /
	  (float)S.raw_inset_crop.cheight));
	Aspect =
	  lookup_id2hr(S.raw_inset_crop.aspect, AspectRatios, nAspectRatios);
	if (Aspect) fprintf (outfile, "%s\n", Aspect->name);
	else fprintf (outfile, "Other %d\n", S.raw_inset_crop.aspect);

	if (Nikon.SensorHighSpeedCrop.cwidth) {
	  fprintf(outfile, "\tHighSpeed crop from: %d x %d at top left pixel: (%d, %d)\n",
	    Nikon.SensorHighSpeedCrop.cwidth, Nikon.SensorHighSpeedCrop.cheight,
	    Nikon.SensorHighSpeedCrop.cleft, Nikon.SensorHighSpeedCrop.ctop);
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

void print_normfun(FILE* outfile, LibRaw& MyCoolRawProcessor, std::string& fn)
{
	trimSpaces(P1.make);
	trimSpaces(P1.model);
	fprintf(outfile, "\nFilename: %s\n", fn.c_str());
	fprintf(outfile, "make/model: =%s/%s= ID: 0x%llx   norm. make/model: =%s/%s=\n",
	        P1.make, P1.model, mnLens.CamID, P1.normalized_make, P1.normalized_model);
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
