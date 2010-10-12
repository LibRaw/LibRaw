#!/bin/sh

if [ -d CVS ] ; then
  echo 'Use mkdist script in cvs export-ed dirs'
else
  if [ -f Makefile.dist ] ; then
    make regenerate  
    rm -fr dcraw/*
    rm -f dcraw/.gdbinit
    cd dcraw
    wget http://www.cybercom.net/~dcoffin/dcraw/dcraw.c
    cd ..
    rm -f internal/preprocess.pl clist2c.pl
    rm doc/*.psd
    mv Makefile.dist Makefile
    rm mkdist.sh
  else
   echo 'Wrong directory or mkdist.sh already executed'
  fi
fi
