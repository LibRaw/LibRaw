#!/bin/sh

if [ -d CVS ] ; then
  echo 'Use mkdist script in cvs export-ed dirs'
else
  if [ -f Makefile.devel ] ; then
    make -f Makefile.devel regenerate  
    autoreconf --install
    rm -fr dcraw/*
    rm -f dcraw/.gdbinit
    rm -f aclocal.m4
    rm -fr autom4te.cache
    cd dcraw
    wget http://www.cybercom.net/~dcoffin/dcraw/dcraw.c
    cd ..
    rm -f internal/preprocess.pl clist2c.pl
    rm doc/*.psd
    rm -f Makefile Makefile.devel Makefile.dist
    rm DEVELOPER-NOTES
    rm mkdist.sh export-dist.sh
  else
   echo 'Wrong directory or mkdist.sh already executed'
  fi
fi
