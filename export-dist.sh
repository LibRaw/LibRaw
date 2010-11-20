#!/bin/sh

DEST=$1
VERSION=`./version.sh`

if test -d $DEST ; then
 echo Using $DEST/$VERSION
else
  echo Usage: $0 destination-dir
  exit 1
fi
cd ..
for dir in LibRaw LibRaw-demosaic-pack-GPL2 LibRaw-demosaic-pack-GPL3
do
 cd $dir
 git pull origin
 cd ..
done
for dir in LibRaw LibRaw-demosaic-pack-GPL2 LibRaw-demosaic-pack-GPL3
do
 cd $dir
 git archive --prefix=$dir-$VERSION/ $VERSION | (cd $DEST; tar xvf - )
 cd ..
done
