#!/usr/bin/perl
 
# File: preprocess.pl
# Copyright 2008-2009 Alex Tutubalin <lexa@lexa.ru>
# Created: Sat Mar  8, 2008
# LibRaw preprocessor for dcraw source
#

use strict;
use Getopt::Std;
use Data::Dumper;

my %opts;
getopts('D:',\%opts);
#print Dumper \%opts;
#exit 0;
my $tag = $opts{D};
die 'use -DTAG option to specify tags, use __ALL__ tag to out all @out sections' unless $tag;

process_file($_) foreach @ARGV;

sub process_file
  {
    my $file = shift;
    return unless $file;
    open O,$file or die;
    my $lno = 0;
    my $out = 0;
    my $date = scalar localtime;
print <<EOM;
/* 
  Copyright 2008-2010 LibRaw LLC (info\@libraw.org)

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of three licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

3. LibRaw Software License 27032010
   (See file LICENSE.LibRaw.pdf provided in LibRaw distribution archive for details).

   This file is generated from Dave Coffin's dcraw.c
   Look into original file (probably http://cybercom.net/~dcoffin/dcraw/dcraw.c)
   for copyright information.
*/

EOM
    while (my $line= <>)
      {
        $lno++;
        if ($line=~m|^\s*/\*\s*\@_emit\s+(.*)\*/\s*$|)
          {
            print "$1\n" if $out;
            next;
          }
        if ($line=~/^\s*(\/\/\s*\@out|\/\*\s*\@out)\s+(.*)/)
          {
            my @tags = split(/\s+/,$2);
            $out = 1 if $tag eq '__ALL__';
            foreach my $t (@tags)
              {
                if ($t eq $tag)
                  {
                    $out = 1;
                    print "#line ".($lno+1)." \"$file\"\n";
                    last;
                  }
              }
            next;
          }
        if ($line=~/^\s*\/\/\s*\@end\s+(.*)/)
          {
            my @tags = split(/\s+/,$1);
            $out = 0 if $tag eq '__ALL__';
            foreach my $t (@tags)
              {
                $out = 0 if $t eq $tag;
              }
            next;
          }
        if ($line=~/^\s*\@end\s+(.*)\*\/\s*$/)
          {
            my @tags = split(/\s+/,$1);
            $out = 0 if $tag eq '__ALL__';
            foreach my $t (@tags)
              {
                $out = 0 if $t eq $tag;
              }
            next;
          }
        print $line if $out;
      }
  }
