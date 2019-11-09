GoPro HERO/Fusion .GPR files are DNG files by nature, but with own wavelet-based codec.

GoPro provides GPR SDK for reading these files, available at https://github.com/gopro/gpr

LibRaw is able to use this GPR SDK to read GoPro HERO/Fusion files.

To enable this support:

1. Build GPR SDK (see NOTES section below)
2. Build LibRaw with -DUSE_DNGSDK and -DUSE_GPRSDK compiler flags
   (you'll need to adjust INCLUDE path and linker flags to add GPR SDK files to compile/link).


NOTES:
I. GPR SDK comes with (patched) Adobe DNG SDK source. You may use this DNG SDK instead of
   Adobe's one, or use standard Adobe's distribution.

II. If Adobe's version is used:
   a) You'll need to enable dng_ifd.fCompression value == 9 in dng_ifd::IsValidCFA() call
      Use provided patch: LibRaw/GoPro/dng-sdk-allow-VC5-validate.diff 
      (it may not apply to any Adobe DNG SDK version, if so apply it by hands).

   b) Newer (than supplied w/ GPR SDK) Adobe SDK versions changed dng_read_image::ReadTile interface
     If newer Adobe SDK is used, please apply patches LibRaw/GoPro/gpr_read_image.cpp.diff 
     and  LibRaw/GoPro/gpr_read_image.h.diff to your GPR SDK code

   c) GPR SDK's gpr_sdk/private/gpr.cpp uses own (added) dng_host method GetGPMFPayload
      so it will not compile with Adobes (not patched) dng_host.h
      LibRaw does not use high-level interface provided by gpr.cpp, so
      possible problem solutions are:
       - either compile GPR SDK without gpr_sdk/private/gpr.cpp file
       - or provide GPR's dng_host.h while building GPR SDK.
     (in our software we use 1st method).       

III. LibRaw uses private gpr_read_image() interface
    So you'll need to add PATH_TO/gpr_sdk/gpr_sdk/private to -I compiler flags.

IV.  -DUSE_GPRSDK LibRaw build flag requires -DUSE_DNGSDK. LibRaw will not compile if
     USE_GPRSDK is set, but USE_DNGSDK is not

V.  LibRaw will use DNG SDK to unpack GoPro files even if imgdata.params.use_dng_sdk is set to 0.

VI. If LibRaw is built with -DUSE_GPRSDK, LibRaw::capabilities will return LIBRAW_CAPS_GPRSDK flag.


