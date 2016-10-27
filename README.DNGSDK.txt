LibRaw can decode most common DNG formats (Bayer/Linear, 16-bit integer and 
floating point) by itself (you need to use zlib for deflate decompression 
support and libjpeg for lossy compressed DNGs).

Some rare flavours of DNG files (e.g. for 8-bit files) are not implemented in 
LibRaw. These files never come from camera or DNG files created by 
Adobe DNG Converter, but created by some processing software that uses DNG as
intermediate format.

To decode such files you need to compile LibRaw with Adobe DNG SDK. This SDK
to be used internally, standard LibRaw API remains untouched (you may specify
what kinds of DNG files you want to decode via Adobe code, see below).

To build LibRaw with DNG SDK specify USE_DNGSDK in defines and adjust 
include/linker path settings to point to DNG SDK's include and library folders.

In your application
 
 * create dng_host object (or derived object, e.g. with multithreaded) entity in your program;
 * pass it to LibRaw via LibRaw::set_dng_host(dng_host *) call to enable DNG SDK use on runtime
 * adjust imgdata.params.use_dngsdk parameter with one of this values
       (ORed bitwise):
       LIBRAW_DNG_NONE - do not use DNG SDK for DNG processing
       LIBRAW_DNG_FLOAT - process floating point DNGs via Adobe DNG SDK
       LIBRAW_DNG_LINEAR - process 'linear DNG' (i.e. demosaiced)
       LIBRAW_DNG_DEFLATE - process DNGs compressed with deflate (gzip)
       LIBRAW_DNG_XTRANS - process Fuji X-Trans DNGs (6x6 CFA)
       LIBRAW_DNG_OTHER - process other files (so usual 2x2 bayer)

    Default value for this field:
       LIBRAW_DNG_DEFAULT=LIBRAW_DNG_FLOAT|LIBRAW_DNG_LINEAR|LIBRAW_DNG_DEFLATE

    You also may use this macro to pass all supported DNGs to Adobe SDK:
       LIBRAW_DNG_ALL = LIBRAW_DNG_FLOAT|LIBRAW_DNG_LINEAR|LIBRAW_DNG_XTRANS|LIBRAW_DNG_OTHER
