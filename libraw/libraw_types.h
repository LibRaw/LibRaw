/* -*- C++ -*-
 * File: libraw_types.h
 * Copyright 2008-2020 LibRaw LLC (info@libraw.org)
 * Created: Sat Mar  8 , 2008
 *
 * LibRaw C data structures
 *

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#ifndef _LIBRAW_TYPES_H
#define _LIBRAW_TYPES_H

#include <sys/types.h>
#ifndef _WIN32
#include <sys/time.h>
#endif

#include <stdio.h>

#if defined(_WIN32)
#if defined(_MSC_VER) && (_MSC_VER <= 1500)
typedef signed __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef signed __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef signed __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef signed __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else /* WIN32, but not old MSVC */
#include <stdint.h>
#endif /* _WIN32 */
#include <sys/types.h>
#else
#include <inttypes.h>
#endif

#if defined(_OPENMP)

#if defined(_WIN32)
#if defined(_MSC_VER) &&                                                       \
    (_MSC_VER >= 1600 || (_MSC_VER == 1500 && _MSC_FULL_VER >= 150030729))
/* VS2010+ : OpenMP works OK, VS2008: have tested by cgilles */
#define LIBRAW_USE_OPENMP
#elif defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 910)
/*  Have not tested on 9.x and 10.x, but Intel documentation claims OpenMP 2.5
 * support in 9.1 */
#define LIBRAW_USE_OPENMP
#else
#undef LIBRAW_USE_OPENMP
#endif
/* Not Win32 */
#elif (defined(__APPLE__) || defined(__MACOSX__)) && defined(_REENTRANT)
/* Latest XCode works with OpenMP, need to recheck here */
#undef LIBRAW_USE_OPENMP
#else
#define LIBRAW_USE_OPENMP
#endif
#endif

#ifdef LIBRAW_USE_OPENMP
#include <omp.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(USE_LCMS)
#include <lcms.h>
#elif defined(USE_LCMS2)
#include <lcms2.h>
#else
#define NO_LCMS
#endif

#include "libraw_const.h"
#include "libraw_version.h"

#ifdef _WIN32
  typedef __int64 INT64;
  typedef unsigned __int64 UINT64;
#else
typedef long long INT64;
typedef unsigned long long UINT64;
#endif

  typedef unsigned char uchar;
  typedef unsigned short ushort;

#ifdef LIBRAW_WIN32_DLLDEFS
#ifdef LIBRAW_NODLL
#define DllDef
#else
#ifdef LIBRAW_BUILDLIB
#define DllDef __declspec(dllexport)
#else
#define DllDef __declspec(dllimport)
#endif
#endif
#else
#define DllDef
#endif

  typedef struct
  {
    const char *decoder_name;
    unsigned decoder_flags;
  } libraw_decoder_info_t;

  typedef struct
  {
    unsigned mix_green;
    unsigned raw_color;
    unsigned zero_is_bad;
    ushort shrink;
    ushort fuji_width;
  } libraw_internal_output_params_t;

  typedef void (*memory_callback)(void *data, const char *file,
                                  const char *where);
  typedef void (*exif_parser_callback)(void *context, int tag, int type,
                                       int len, unsigned int ord, void *ifp,
                                       INT64 base);

  DllDef void default_memory_callback(void *data, const char *file,
                                      const char *where);

  typedef void (*data_callback)(void *data, const char *file, const int offset);

  DllDef void default_data_callback(void *data, const char *file,
                                    const int offset);

  typedef int (*progress_callback)(void *data, enum LibRaw_progress stage,
                                   int iteration, int expected);
  typedef int (*pre_identify_callback)(void *ctx);
  typedef void (*post_identify_callback)(void *ctx);
  typedef void (*process_step_callback)(void *ctx);

  typedef struct
  {
    memory_callback mem_cb;
    void *memcb_data;

    data_callback data_cb;
    void *datacb_data;

    progress_callback progress_cb;
    void *progresscb_data;

    exif_parser_callback exif_cb;
    void *exifparser_data;
    pre_identify_callback pre_identify_cb;
    post_identify_callback post_identify_cb;
    process_step_callback pre_subtractblack_cb, pre_scalecolors_cb,
        pre_preinterpolate_cb, pre_interpolate_cb, interpolate_bayer_cb,
        interpolate_xtrans_cb, post_interpolate_cb, pre_converttorgb_cb,
        post_converttorgb_cb;
  } libraw_callbacks_t;

  typedef struct
  {
    enum LibRaw_image_formats type;
    ushort height, width, colors, bits;
    unsigned int data_size;
    unsigned char data[1];
  } libraw_processed_image_t;

  typedef struct
  {
    char guard[4];
    char make[64];
    char model[64];
    char software[64];
    char normalized_make[64];
    char normalized_model[64];
    unsigned maker_index;
    unsigned raw_count;
    unsigned dng_version;
    unsigned is_foveon;
    int colors;
    unsigned filters;
    char xtrans[6][6];
    char xtrans_abs[6][6];
    char cdesc[5];
    unsigned xmplen;
    char *xmpdata;

  } libraw_iparams_t;

  typedef struct
  {
    ushort cleft, ctop, cwidth, cheight, aspect;
  } libraw_raw_inset_crop_t;

  typedef struct
  {
    ushort raw_height, raw_width, height, width, top_margin, left_margin;
    ushort iheight, iwidth;
    unsigned raw_pitch;
    double pixel_aspect;
    int flip;
    int mask[8][4];
    libraw_raw_inset_crop_t raw_inset_crop;
  } libraw_image_sizes_t;

  struct ph1_t
  {
    int format, key_off, tag_21a;
    int t_black, split_col, black_col, split_row, black_row;
    float tag_210;
  };

  typedef struct
  {
    unsigned parsedfields;
    ushort illuminant;
    float calibration[4][4];
    float colormatrix[4][3];
    float forwardmatrix[3][4];
  } libraw_dng_color_t;

  typedef struct
  {
    unsigned parsedfields;
    unsigned dng_cblack[LIBRAW_CBLACK_SIZE];
    unsigned dng_black;
    float dng_fcblack[LIBRAW_CBLACK_SIZE];
    float dng_fblack;
    unsigned dng_whitelevel[4];
    unsigned default_crop[4]; /* Origin and size */
    unsigned preview_colorspace;
    float analogbalance[4];
    float asshotneutral[4];
    float baseline_exposure;
    float LinearResponseLimit;
  } libraw_dng_levels_t;

  typedef struct
  {
    float romm_cam[9];
  } libraw_P1_color_t;

  typedef struct
  {
    int ColorDataVer;
    int ColorDataSubVer;
    int SpecularWhiteLevel;
    int NormalWhiteLevel;
    int ChannelBlackLevel[4];
    int AverageBlackLevel;
    /* multishot */
    unsigned int multishot[4];
    /* metering */
    short MeteringMode;
    short SpotMeteringMode;
    uchar FlashMeteringMode;
    short FlashExposureLock;
    short ExposureMode;
    short AESetting;
    uchar HighlightTonePriority;
    /* stabilization */
    short ImageStabilization;
    /* focus */
    short FocusMode;
    short AFPoint;
    short FocusContinuous;
    short AFPointsInFocus30D;
    uchar AFPointsInFocus1D[8];
    ushort AFPointsInFocus5D; /* bytes in reverse*/
                              /* AFInfo */
    ushort AFAreaMode;
    ushort NumAFPoints;
    ushort ValidAFPoints;
    ushort AFImageWidth;
    ushort AFImageHeight;
    short AFAreaWidths[61];     /* cycle to NumAFPoints */
    short AFAreaHeights[61];    /* --''--               */
    short AFAreaXPositions[61]; /* --''--               */
    short AFAreaYPositions[61]; /* --''--               */
    short AFPointsInFocus[4];   /* cycle to floor((NumAFPoints+15)/16) */
    short AFPointsSelected[4];  /* --''--               */
    ushort PrimaryAFPoint;
    /* flash */
    short FlashMode;
    short FlashActivity;
    short FlashBits;
    short ManualFlashOutput;
    short FlashOutput;
    short FlashGuideNumber;
    /* drive */
    short ContinuousDrive;
    /* sensor */
    short SensorWidth;
    short SensorHeight;
    short SensorLeftBorder;
    short SensorTopBorder;
    short SensorRightBorder;
    short SensorBottomBorder;
    short BlackMaskLeftBorder;
    short BlackMaskTopBorder;
    short BlackMaskRightBorder;
    short BlackMaskBottomBorder;
    int   AFMicroAdjMode;
    float AFMicroAdjValue;
    short MakernotesFlip;
    short RecordMode;
    short SRAWQuality;
    unsigned wbi;
    float firmware;
    short RF_lensID;
  } libraw_canon_makernotes_t;

  typedef struct
  {
    int    BaseISO;
    double Gain;
    char   Sensor[8];
    char   SensorUnit[64]; // SU
    char   HostBody[64];   // HB
    int    SensorCode;
    int    SensorSubCode;
    int    CoatingCode;
    int    uncropped;

/* CaptureSequenceInitiator is based on the content of the 'model' tag
  - values like 'Pinhole', 'Flash Sync', '500 Mech.' etc in .3FR 'model' tag
    come from MAIN MENU > SETTINGS > Camera;
  - otherwise 'model' contains:
    1. if CF/CFV/CFH, SU enclosure, can be with SU type if '-' is present
    2. else if '-' is present, HB + SU type;
    3. HB;
*/
    char CaptureSequenceInitiator[32];

/* SensorUnitConnector, makernotes 0x0015 tag:
 - in .3FR - SU side
 - in .FFF - HB side
*/
    char SensorUnitConnector[64];

    int format; // 3FR, FFF, Imacon (H3D-39 and maybe others), Hasselblad/Phocus DNG, Adobe DNG
    int nIFD_CM[2]; // number of IFD containing CM
    int RecommendedCrop[2];

/* mnColorMatrix is in makernotes tag 0x002a;
  not present in .3FR files and Imacon/H3D-39 .FFF files;
  when present in .FFF and Phocus .DNG files, it is a copy of CM1 from .3FR;
  available samples contain all '1's in the first 3 elements
*/
    double mnColorMatrix[4][3];

  } libraw_hasselblad_makernotes_t;

  typedef struct
  {
    float  ExpoMidPointShift;
    ushort DynamicRange;
    ushort FilmMode;
    ushort DynamicRangeSetting;
    ushort DevelopmentDynamicRange;
    ushort AutoDynamicRange;
    ushort DRangePriority;
    ushort DRangePriorityAuto;
    ushort DRangePriorityFixed;

    /*
    tag 0x9200, converted to BrightnessCompensation
    F700, S3Pro, S5Pro, S20Pro, S200EXR
    E550, E900, F810, S5600, S6500fd, S9000, S9500, S100FS
    */
    float BrightnessCompensation; /* in EV, if =4, raw data * 2^4 */

    ushort FocusMode;
    ushort AFMode;
    ushort FocusPixel[2];
    ushort ImageStabilization[3];
    ushort FlashMode;
    ushort WB_Preset;

    /* ShutterType:
       0 - mechanical
       1 = electronic
       2 = electronic, long shutter speed
       3 = electronic, front curtain
    */
    ushort ShutterType;
    ushort ExrMode;
    ushort Macro;
    unsigned Rating;

    /* CropMode:
       1 - FF on GFX,
       2 - sports finder (mechanical shutter),
       4 - 1.25x crop (electronic shutter, continuous high)
    */
    ushort CropMode;
    ushort FrameRate;
    ushort FrameWidth;
    ushort FrameHeight;
    char   SerialSignature[0x0c + 1];
    char   RAFVersion[4 + 1];
    ushort RAFDataVersion;
    int    isTSNERDTS;

    /* DriveMode:
       0 - single frame
       1 - continuous low
       2 - continuous high
    */
    short DriveMode;
  } libraw_fuji_info_t;

  typedef struct
  {
    ushort cleft, ctop, cwidth, cheight;
  } libraw_sensor_highspeed_crop_t;

  typedef struct
  {
    double ExposureBracketValue;
    ushort ActiveDLighting;
    ushort ShootingMode;
    /* stabilization */
    uchar ImageStabilization[7];
    uchar VibrationReduction;
    uchar VRMode;
    /* focus */
    char  FocusMode[7];
    uchar AFPoint;
    ushort AFPointsInFocus;
    uchar  ContrastDetectAF;
    uchar  AFAreaMode;
    uchar  PhaseDetectAF;
    uchar  PrimaryAFPoint;
    uchar  AFPointsUsed[29];
    ushort AFImageWidth;
    ushort AFImageHeight;
    ushort AFAreaXPposition;
    ushort AFAreaYPosition;
    ushort AFAreaWidth;
    ushort AFAreaHeight;
    uchar  ContrastDetectAFInFocus;
    /* flash */
    char  FlashSetting[13];
    char  FlashType[20];
    uchar FlashExposureCompensation[4];
    uchar ExternalFlashExposureComp[4];
    uchar FlashExposureBracketValue[4];
    uchar FlashMode;
    signed char FlashExposureCompensation2;
    signed char FlashExposureCompensation3;
    signed char FlashExposureCompensation4;
    uchar  FlashSource;
    uchar  FlashFirmware[2];
    uchar  ExternalFlashFlags;
    uchar  FlashControlCommanderMode;
    uchar  FlashOutputAndCompensation;
    uchar  FlashFocalLength;
    uchar  FlashGNDistance;
    uchar  FlashGroupControlMode[4];
    uchar  FlashGroupOutputAndCompensation[4];
    uchar  FlashColorFilter;
    ushort NEFCompression;
    int    ExposureMode;
    int    ExposureProgram;
    int    nMEshots;
    int    MEgainOn;
    double ME_WB[4];
    uchar  AFFineTune;
    uchar  AFFineTuneIndex;
    int8_t AFFineTuneAdj;
    unsigned LensDataVersion;
    unsigned FlashInfoVersion;
    unsigned ColorBalanceVersion;
    uchar key;
    ushort NEFBitDepth[4];
    ushort HighSpeedCropFormat; /* 1 -> 1.3x; 2 -> DX; 3 -> 5:4; 4 -> 3:2; 6 ->
                                   16:9; 11 -> FX uncropped; 12 -> DX uncropped;
                                   17 -> 1:1 */
    libraw_sensor_highspeed_crop_t SensorHighSpeedCrop;
    ushort SensorWidth;
    ushort SensorHeight;
  } libraw_nikon_makernotes_t;

  typedef struct
  {
    int      SensorCalibration[2];
    ushort   FocusMode[2];
    ushort   AutoFocus;
    ushort   AFPoint;
    unsigned AFAreas[64];
    double   AFPointSelected[5];
    ushort   AFResult;
    ushort   DriveMode[5];
    ushort   ColorSpace;
    uchar    AFFineTune;
    short    AFFineTuneAdj[3];
    char     CameraType2[6];
  } libraw_olympus_makernotes_t;

  typedef struct
  {
    /* Compression:
     34826 (Panasonic RAW 2): LEICA DIGILUX 2;
     34828 (Panasonic RAW 3): LEICA D-LUX 3; LEICA V-LUX 1; Panasonic DMC-LX1;
     Panasonic DMC-LX2; Panasonic DMC-FZ30; Panasonic DMC-FZ50; 34830 (not in
     exiftool): LEICA DIGILUX 3; Panasonic DMC-L1; 34316 (Panasonic RAW 1):
     others (LEICA, Panasonic, YUNEEC);
    */
    ushort   Compression;
    ushort   BlackLevelDim;
    float    BlackLevel[8];
    unsigned Multishot; /* 0 is Off, 65536 is Pixel Shift */
    float    gamma;
    int      HighISOMultiplier[3]; /* 0->R, 1->G, 2->B */
  } libraw_panasonic_makernotes_t;

  typedef struct
  {
    ushort   FocusMode;
    ushort   AFPointSelected;
    unsigned AFPointsInFocus;
    ushort   FocusPosition;
    uchar    DriveMode[4];
    short    AFAdjustment;
    uchar    MultiExposure; /* last bit is not "1" if ME is not used */
    ushort   Quality; /* 4 is raw, 7 is raw w/ pixel shift, 8 is raw w/ dynamic
                       pixel shift */
    /*    uchar AFPointMode;     */
    /*    uchar SRResult;        */
    /*    uchar ShakeReduction;  */
  } libraw_pentax_makernotes_t;

  typedef struct
  {
    unsigned ImageSizeFull[4];
    unsigned ImageSizeCrop[4];
    int      ColorSpace[2];
    unsigned key[11];
    double   DigitalGain; /* PostAEGain, digital stretch */
    int      DeviceType;
    char     LensFirmware[32];
  } libraw_samsung_makernotes_t;

  typedef struct
  {
    ushort BlackLevelTop;
    ushort BlackLevelBottom;
    short offset_left, offset_top; /* KDC files, negative values or zeros */
    ushort clipBlack, clipWhite;   /* valid for P712, P850, P880 */
    float romm_camDaylight[3][3];
    float romm_camTungsten[3][3];
    float romm_camFluorescent[3][3];
    float romm_camFlash[3][3];
    float romm_camCustom[3][3];
    float romm_camAuto[3][3];
    ushort val018percent, val100percent, val170percent;
    short MakerNoteKodak8a;
    float ISOCalibrationGain;
    float AnalogISO;
  } libraw_kodak_makernotes_t;

  typedef struct {
    char Software[64];        // tag 0x0203
    char SystemType[64];      // tag 0x0204
    char FirmwareString[256]; // tag 0x0301
    char SystemModel[64];
  } libraw_p1_makernotes_t;

  typedef struct
  {
    ushort   CameraType;
    uchar    Sony0x9400_version; /* 0 if not found/deciphered, 0xa, 0xb, 0xc
                                 following exiftool convention */
    uchar    Sony0x9400_ReleaseMode2;
    unsigned Sony0x9400_SequenceImageNumber;
    uchar    Sony0x9400_SequenceLength1;
    unsigned Sony0x9400_SequenceFileNumber;
    uchar    Sony0x9400_SequenceLength2;
    uint8_t  AFAreaModeSetting;
    ushort   FlexibleSpotPosition[2];
    uint8_t  AFPointSelected;
    uint8_t  AFPointsUsed[10];
    uint8_t  AFTracking;
    uint8_t  AFType;
    ushort   FocusLocation[4];
    int8_t   AFMicroAdjValue;
    int8_t   AFMicroAdjOn;
    uchar    AFMicroAdjRegisteredLenses;
    ushort   VariableLowPassFilter;
    unsigned LongExposureNoiseReduction;
    ushort   HighISONoiseReduction;
    ushort   HDR[2];
    ushort   group2010;
    ushort   real_iso_offset;
    ushort   MeteringMode_offset;
    ushort   ExposureProgram_offset;
    ushort   ReleaseMode2_offset;
    unsigned MinoltaCamID;
    float    firmware;
    ushort   ImageCount3_offset;
    unsigned ImageCount3;
    unsigned ElectronicFrontCurtainShutter;
    ushort   MeteringMode2;
    char     SonyDateTime[20];
    unsigned ShotNumberSincePowerUp;
    ushort   PixelShiftGroupPrefix;
    unsigned PixelShiftGroupID;
    char     nShotsInPixelShiftGroup;
    char     numInPixelShiftGroup; /* '0' if ARQ, first shot in the group has '1'
                                  here */
    ushort   prd_ImageHeight, prd_ImageWidth;
    ushort   prd_RawBitDepth;
    ushort   prd_StorageMethod; /* 82 -> Padded; 89 -> Linear */
    ushort   prd_BayerPattern;  /* 0 -> not valid; 1 -> RGGB; 4 -> GBRG */

    ushort   SonyRawFileType; /* takes precedence over RAWFileType and Quality:
                               0  for uncompressed 14-bit raw
                               1  for uncompressed 12-bit raw
                               2  for compressed raw
                               3  for lossless compressed raw
                            */
    ushort RAWFileType;     /* takes precedence over Quality
                               0 for compressed raw, 1 for uncompressed;
                            */
    unsigned Quality;       /* 0 or 6 for raw, 7 or 8 for compressed raw */
    ushort FileFormat;      /*  1000 SR2
                                2000 ARW 1.0
                                3000 ARW 2.0
                                3100 ARW 2.1
                                3200 ARW 2.2
                                3300 ARW 2.3
                                3310 ARW 2.3.1
                                3320 ARW 2.3.2
                                3330 ARW 2.3.3
                                3350 ARW 2.3.5
                             */
  } libraw_sony_info_t;

  typedef struct
  {
    ushort curve[0x10000];
    unsigned cblack[LIBRAW_CBLACK_SIZE];
    unsigned black;
    unsigned data_maximum;
    unsigned maximum;
    long linear_max[4];
    float fmaximum;
    float fnorm;
    ushort white[8][8];
    float cam_mul[4];
    float pre_mul[4];
    float cmatrix[3][4];
    float ccm[3][4];
    float rgb_cam[3][4];
    float cam_xyz[4][3];
    struct ph1_t phase_one_data;
    float flash_used;
    float canon_ev;
    char model2[64];
    char UniqueCameraModel[64];
    char LocalizedCameraModel[64];
    char ImageUniqueID[64];
    char RawDataUniqueID[17];
    char OriginalRawFileName[64];
    void *profile;
    unsigned profile_length;
    unsigned black_stat[8];
    libraw_dng_color_t dng_color[2];
    libraw_dng_levels_t dng_levels;
    int WB_Coeffs[256][4];    /* R, G1, B, G2 coeffs */
    float WBCT_Coeffs[64][5]; /* CCT, than R, G1, B, G2 coeffs */
    int as_shot_wb_applied;
    libraw_P1_color_t P1_color[2];
    unsigned raw_bps; /* for Phase One, raw format */
                      /* Phase One raw format values, makernotes tag 0x010e:
                      0    Name unknown
                      1    "RAW 1"
                      2    "RAW 2"
                      3    "IIQ L"
                      4    Never seen
                      5    "IIQ S"
                      6    "IIQ S v.2"
                      7    Never seen
                      8    Name unknown
                      */
	int ExifColorSpace;
  } libraw_colordata_t;

  typedef struct
  {
    enum LibRaw_thumbnail_formats tformat;
    ushort twidth, theight;
    unsigned tlength;
    int tcolors;
    char *thumb;
  } libraw_thumbnail_t;

  typedef struct
  {
    float latitude[3];     /* Deg,min,sec */
    float longitude[3];    /* Deg,min,sec */
    float gpstimestamp[3]; /* Deg,min,sec */
    float altitude;
    char  altref, latref, longref, gpsstatus;
    char  gpsparsed;
  } libraw_gps_info_t;

  typedef struct
  {
    float iso_speed;
    float shutter;
    float aperture;
    float focal_len;
    time_t timestamp;
    unsigned shot_order;
    unsigned gpsdata[32];
    libraw_gps_info_t parsed_gps;
    char desc[512], artist[64];
    float analogbalance[4];
  } libraw_imgother_t;

  typedef struct {
    float FlashEC;
    float FlashGN;
    float CameraTemperature;
    float SensorTemperature;
    float SensorTemperature2;
    float LensTemperature;
    float AmbientTemperature;
    float BatteryTemperature;
    float exifAmbientTemperature;
    float exifHumidity;
    float exifPressure;
    float exifWaterDepth;
    float exifAcceleration;
    float exifCameraElevationAngle;
    float real_ISO;
    float exifExposureIndex;
    ushort ColorSpace;
    char firmware[128];
  } libraw_metadata_common_t;

  typedef struct
  {
    unsigned greybox[4];   /* -A  x1 y1 x2 y2 */
    unsigned cropbox[4];   /* -B x1 y1 x2 y2 */
    double aber[4];        /* -C */
    double gamm[6];        /* -g */
    float user_mul[4];     /* -r mul0 mul1 mul2 mul3 */
    unsigned shot_select;  /* -s */
    float bright;          /* -b */
    float threshold;       /* -n */
    int half_size;         /* -h */
    int four_color_rgb;    /* -f */
    int highlight;         /* -H */
    int use_auto_wb;       /* -a */
    int use_camera_wb;     /* -w */
    int use_camera_matrix; /* +M/-M */
    int output_color;      /* -o */
    char *output_profile;  /* -o */
    char *camera_profile;  /* -p */
    char *bad_pixels;      /* -P */
    char *dark_frame;      /* -K */
    int output_bps;        /* -4 */
    int output_tiff;       /* -T */
    int user_flip;         /* -t */
    int user_qual;         /* -q */
    int user_black;        /* -k */
    int user_cblack[4];
    int user_sat;          /* -S */
    int med_passes;        /* -m */
    float auto_bright_thr;
    float adjust_maximum_thr;
    int no_auto_bright;    /* -W */
    int use_fuji_rotate;   /* -j */
    int green_matching;
    /* DCB parameters */
    int dcb_iterations;
    int dcb_enhance_fl;
    int fbdd_noiserd;
    int exp_correc;
    float exp_shift;
    float exp_preser;
    /* Raw speed */
    int use_rawspeed;
    /* DNG SDK */
    int use_dngsdk;
    /* Disable Auto-scale */
    int no_auto_scale;
    /* Disable intepolation */
    int no_interpolation;
    /*  int x3f_flags; */
    /* Sony ARW2 digging mode */
    /* int sony_arw2_options; */
    unsigned raw_processing_options;
    unsigned max_raw_memory_mb;
    int sony_arw2_posterization_thr;
    /* Nikon Coolscan */
    float coolscan_nef_gamma;
    char p4shot_order[5];
    /* Custom camera list */
    char **custom_camera_strings;
  } libraw_output_params_t;

  typedef struct
  {
    /* really allocated bitmap */
    void *raw_alloc;
    /* alias to single_channel variant */
    ushort *raw_image;
    /* alias to 4-channel variant */
    ushort (*color4_image)[4];
    /* alias to 3-color variand decoded by RawSpeed */
    ushort (*color3_image)[3];
    /* float bayer */
    float *float_image;
    /* float 3-component */
    float (*float3_image)[3];
    /* float 4-component */
    float (*float4_image)[4];

    /* Phase One black level data; */
    short (*ph1_cblack)[2];
    short (*ph1_rblack)[2];
    /* save color and sizes here, too.... */
    libraw_iparams_t iparams;
    libraw_image_sizes_t sizes;
    libraw_internal_output_params_t ioparams;
    libraw_colordata_t color;
  } libraw_rawdata_t;

  typedef struct
  {
    unsigned long long LensID;
    char Lens[128];
    ushort LensFormat; /* to characterize the image circle the lens covers */
    ushort LensMount;  /* 'male', lens itself */
    unsigned long long CamID;
    ushort CameraFormat; /* some of the sensor formats */
    ushort CameraMount;  /* 'female', body throat */
    char   body[64];
    short  FocalType; /* -1/0 is unknown; 1 is fixed focal; 2 is zoom */
    char   LensFeatures_pre[16], LensFeatures_suf[16];
    float  MinFocal, MaxFocal;
    float  MaxAp4MinFocal, MaxAp4MaxFocal, MinAp4MinFocal, MinAp4MaxFocal;
    float  MaxAp, MinAp;
    float  CurFocal, CurAp;
    float  MaxAp4CurFocal, MinAp4CurFocal;
    float  MinFocusDistance;
    float  FocusRangeIndex;
    float  LensFStops;
    unsigned long long TeleconverterID;
    char Teleconverter[128];
    unsigned long long AdapterID;
    char Adapter[128];
    unsigned long long AttachmentID;
    char   Attachment[128];
    ushort FocalUnits;
    float  FocalLengthIn35mmFormat;
  } libraw_makernotes_lens_t;

  typedef struct
  {
    float EffectiveMaxAp;
    uchar LensIDNumber, LensFStops, MCUVersion, LensType;
  } libraw_nikonlens_t;

  typedef struct
  {
    float MinFocal, MaxFocal, MaxAp4MinFocal, MaxAp4MaxFocal;
  } libraw_dnglens_t;

  typedef struct
  {
    float MinFocal, MaxFocal, MaxAp4MinFocal, MaxAp4MaxFocal, EXIF_MaxAp;
    char LensMake[128], Lens[128], LensSerial[128], InternalLensSerial[128];
    ushort FocalLengthIn35mmFormat;
    libraw_nikonlens_t nikon;
    libraw_dnglens_t dng;
    libraw_makernotes_lens_t makernotes;
  } libraw_lensinfo_t;

  typedef struct
  {
    libraw_canon_makernotes_t canon;
    libraw_nikon_makernotes_t nikon;
    libraw_hasselblad_makernotes_t hasselblad;
    libraw_fuji_info_t fuji;
    libraw_olympus_makernotes_t olympus;
    libraw_sony_info_t sony;
    libraw_kodak_makernotes_t kodak;
    libraw_panasonic_makernotes_t panasonic;
    libraw_pentax_makernotes_t pentax;
    libraw_p1_makernotes_t phaseone;
    libraw_samsung_makernotes_t samsung;
    libraw_metadata_common_t common;
  } libraw_makernotes_t;

  typedef struct
  {
    short DriveMode;
    short FocusMode;
    short MeteringMode;
    short AFPoint;
    short ExposureMode;
    short ExposureProgram;
    short ImageStabilization;
    char BodySerial[64];
    char InternalBodySerial[64]; /* this may be PCB or sensor serial, depends on
                                    make/model */
  } libraw_shootinginfo_t;

  typedef struct
  {
    unsigned fsize;
    ushort rw, rh;
    uchar lm, tm, rm, bm;
    ushort lf;
    uchar cf, max, flags;
    char t_make[10], t_model[20];
    ushort offset;
  } libraw_custom_camera_t;

  typedef struct
  {
    ushort (*image)[4];
    libraw_image_sizes_t sizes;
    libraw_iparams_t idata;
    libraw_lensinfo_t lens;
    libraw_makernotes_t makernotes;
    libraw_shootinginfo_t shootinginfo;
    libraw_output_params_t params;
    unsigned int progress_flags;
    unsigned int process_warnings;
    libraw_colordata_t color;
    libraw_imgother_t other;
    libraw_thumbnail_t thumbnail;
    libraw_rawdata_t rawdata;
    void *parent_class;
  } libraw_data_t;

  struct fuji_compressed_params
  {
    int8_t *q_table; /* quantization table */
    int q_point[5];  /* quantization points */
    int max_bits;
    int min_value;
    int raw_bits;
    int total_values;
    int maxDiff;
    ushort line_width;
  };

#ifdef __cplusplus
}
#endif

#if defined (LIBRAW_LIBRARY_BUILD) && defined(__cplusplus)

class libraw_static_table_t
{
public:
    libraw_static_table_t(const int *a, const unsigned s): data(a),_size(s) {}
    libraw_static_table_t(): data(0),_size(0){}
    libraw_static_table_t(const libraw_static_table_t& s) : data(s.data), _size(s._size) {}
    unsigned size() const { return _size; }
    libraw_static_table_t& operator = (const libraw_static_table_t& s)
    {
        _size = s._size;
        data = s.data;
        return *this;
    }
    int operator [] (unsigned idx) const
    {
        if (idx < _size) return data[idx];
        if(_size>0 && data) return data[0];
        return 0;
    }
private:
    const int *data;
    unsigned _size;
};

#endif


/* Byte order */
#if defined(__POWERPC__)
#define LibRawBigEndian 1

#elif defined(__INTEL__)
#define LibRawBigEndian 0

#elif defined(_M_IX86) || defined(__i386__)
#define LibRawBigEndian 0

#elif defined(_M_X64) || defined(__amd64__) || defined(__x86_64__)
#define LibRawBigEndian 0

#elif defined(__LITTLE_ENDIAN__)
#define LibRawBigEndian 0

#elif defined(__BIG_ENDIAN__)
#define LibRawBigEndian 1
#elif defined(_ARM_)
#define LibRawBigEndian 0

#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define LibRawBigEndian 0

#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define LibRawBigEndian 1
#else
#ifndef qXCodeRez
#error Unable to figure out byte order.
#endif
#endif

#endif
