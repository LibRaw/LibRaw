
# Installing LibRaw

## Installation via ./configure script

If you're using Github snapshot, create ./configure script:
    autoreconf --install
before using it (you'll need autotools installed).

### Unpack the distribution file:

 $  tar xzvf LibRaw-0.xx.yy.tar.gz

### Go to LibRaw folder and run ./configure and make:

 $ cd LibRaw-0.xx.yy
 $ ./configure  [...optional args...]
 $ make

#### ./configure options

--enable-openmp
--disable-openmp

  Enable/disable OpenMP support if compiler supports it.
  OpenMP is enabled by default.

--enable-lcms
--disable-lcms

  Enable/disable LCMS color engine support. If enabled, ./configure will try to 
  find lcms library.  Both LCMS-1.x and LCMS-2.x are supported
  LCMS support is enabled by default


--enable-examples
--disable-examples

  Enables/disables examples compilation and installation. Enabled by default

### Install by run make install as root:
 
 $ sudo make install


## Installation via Makefile.
You may use pre-created Makefiles:
 * Makefile.dist for Unix/macOS systems
 * Makefile.msvc for Windows/VisualStudio
 * Makefile.mingw for Windows/MinGW
 * Makefile.devel - developer mode makefile
  
Sample makefiles with stripped down functions:
 * Makefile.devel.nopp - developer makefile without postprocessing code (postprocessing_ph.cpp in included in build instead of standard postprocessing routines)
 * Makefile.devel.noppr2i - placeholder code included instead of file write code and without preprocessing routines

These makefiles can be edited to suit your needs: uncomment sections with additional libraries, etc.

# Compile-time defines
Some of the library's functionality is controlled via preprocessor flags at build time, see docs/Install-LibRaw.html for details

