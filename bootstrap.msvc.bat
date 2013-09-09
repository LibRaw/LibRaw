@ECHO OFF

REM  Copyright (c) 2013, Gilles Caulier, <caulier dot gilles at gmail dot com>
REM 
REM  Redistribution and use is allowed according to the terms of the BSD license.
REM  For details see the accompanying COPYING-CMAKE-SCRIPTS file.

REM adjust setup here if necessary
SET INSTALL_PREFIX=C:\
SET BUILD_TYPE=debugfull

IF NOT EXIST "build" md "build"

cd "build"

REM Microsoft Visual C++ command line compiler.
cmake -G "NMake Makefiles" . ^
      -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
      -DCMAKE_INSTALL_PREFIX=%INSTALL_PREFIX% ^
      -DENABLE_RAWSPEED=OFF ^
      -DENABLE_DEMOSAIC_PACK_GPL2=OFF ^
      -DENABLE_DEMOSAIC_PACK_GPL3=OFF ^
      -DENABLE_DCRAW_DEBUG=ON ^
      -DENABLE_SAMPLES=ON ^
      ..
