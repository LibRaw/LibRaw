#!/bin/sh

vfile=./libraw/libraw_version.h

major=`grep LIBRAW_MAJOR_VERSION $vfile |head -1 | awk '{print $3}'`
minor=`grep LIBRAW_MINOR_VERSION $vfile | head -1 | awk '{print $3}'`
patch=`grep LIBRAW_PATCH_VERSION $vfile | head -1 | awk '{print $3}'`
tail=`grep LIBRAW_VERSION_TAIL $vfile | head -1 | awk '{print $3}'`

if [ x$tail = xRelease ] ; then
 printf "$major.$minor.$patch"
else
 printf "$major.$minor.$patch-$tail"
fi


