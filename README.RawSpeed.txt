======================= Compile LibRaw with RawSpeed support ===============================

1) Prerequisites

To build RawSpeed you need libxml2, iconv, and JPEG library installed on your 
system.

Unix: you need to build RawSpeed library from sources (See RawSpeed developer 
information at http://rawstudio.org/blog/?p=800 for details)

Win32: you need to build RawSpeed library from sources. If you wish to
build it as .DLL, you need to add __declspec() to external C++ classes 
declarations. You may use rawspeed.win32-dll.patch  provided with LibRaw
to patch RawSpeed sources.
On Windows system you need POSIX Threads library (sources.redhat.com/pthreads-win32/)


2) Build: Generic
  - You need to specify -DUSE_RAWSPEED compile-time define when you compile LibRaw
  - You may specify -DNOARW2_RAWSPEED define if you do not want to use RawSpeed's
    Sony ARW2 format decoder (because result of this decoder is different from
    LibRaw's built-in decoder)
  - You need to link with RawSpeed library and additional libraries (libxml2, iconv)

3) Build: Unix


4) Build: Win32
 - Place RawSpeed in ..\RawSpeed folder (relative to LibRaw sources folder) and compile it.
 - Place all 3rd party libraries (libxml2, iconv, libjpeg, pthreads) in
    ..\RawSpeed\include - include files
    ..\RawSpeed\lib - library files
 - Uncomment CFLAGS_RAWSPEED and LDFLAGS_RAWSPEED lines in Makefile.msvc
 - use nmake -f Makefile.msvc to build LibRaw with RawSpeed support
 
You'll need to have rawspeed.dll, libxml2.dll, iconv.dll, jpeg.dll and charset.dll in your
PATH when you're running application linked with LibRaw.



