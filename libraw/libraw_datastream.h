/* -*- C -*-
 * File: libraw_datastream.h
 * Copyright 2008-2010 LibRaw LLC (info@libraw.org)
 * Created: Sun Jan 18 13:07:35 2009
 *
 * LibRaw Data stream interface

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of three licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

3. LibRaw Software License 27032010
   (See file LICENSE.LibRaw.pdf provided in LibRaw distribution archive for details).

 */

#ifndef __LIBRAW_DATASTREAM_H
#define __LIBRAW_DATASTREAM_H

#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#ifndef __cplusplus

struct LibRaw_abstract_datastream;

#else /* __cplusplus */

#include "libraw_const.h"
#include "libraw_types.h"
#include <fstream>
#include <memory>

class LibRaw_abstract_datastream
{
  protected:
    std::auto_ptr<std::streambuf> f; /* will close() automatically through dtor */
    std::auto_ptr<std::streambuf> saved_f; /* when *f is a subfile, *saved_f is the master file */
    const char *filename;

  public:
    LibRaw_abstract_datastream(const char *filename)
      :filename(filename)
    {
    }

    int valid() { return f.get() ? 1 : 0; }
#define LR_STREAM_CHK() do {if(!f.get()) throw LIBRAW_EXCEPTION_IO_EOF;}while(0)
    int read(void * ptr,size_t size, size_t nmemb) 
    { 
        LR_STREAM_CHK(); 

        return f->sgetn(static_cast<char*>(ptr), nmemb * size) / size;
    }
    int eof() 
    { 
        LR_STREAM_CHK(); 
        return f->sgetc() == EOF;
    }
    int seek(INT64 o, int whence) 
    { 
        LR_STREAM_CHK(); 

        std::ios_base::seekdir dir;
        switch (whence) {
        case SEEK_SET: dir = std::ios_base::beg; break;
        case SEEK_CUR: dir = std::ios_base::cur; break;
        case SEEK_END: dir = std::ios_base::end; break;
        default: dir = std::ios_base::beg;
        }
        return f->pubseekoff(o, dir);
    }
    INT64 tell() 
    { 
        LR_STREAM_CHK(); 

        return f->pubseekoff(0, std::ios_base::cur);
    }
    int get_char() 
    { 
        LR_STREAM_CHK(); 
        return f->sbumpc();
    }
    char* gets(char *str, int sz) 
    { 
        LR_STREAM_CHK(); 

        std::istream is(f.get());
        is.getline(str, sz);
        if (is.fail()) return 0;
        return str;
    }
    virtual int scanf_one(const char *fmt, void*val) 
    { 
        LR_STREAM_CHK(); 

        std::istream is(f.get());

        /* HUGE ASSUMPTION: *fmt is either "%d" or "%f" */
        if (strcmp(fmt, "%d") == 0) {
          int d;
          is >> d;
          if (is.fail()) return EOF;
          *(static_cast<int*>(val)) = d;
        } else {
          float f;
          is >> f;
          if (is.fail()) return EOF;
          *(static_cast<float*>(val)) = f;
        }

        return 1;
    }
    const char* fname() { return filename; }
    /* You can't have a "subfile" and a "tempfile" at the same time. */
    int subfile_open(const char *fn)
    {
      LR_STREAM_CHK();

      if (saved_f.get()) return EBUSY;

      saved_f = f;

      std::auto_ptr<std::filebuf> buf(new std::filebuf());

      buf->open(fn, std::ios_base::in | std::ios_base::binary);
      if (!buf->is_open()) {
        f = saved_f;
        return ENOENT;
      } else {
        f = buf;
      }

      return 0;
    }
    void subfile_close()
    {
        if (!saved_f.get()) return;
        f = saved_f;
    }
    int tempbuffer_open(void* buf, size_t size)
    {
      LR_STREAM_CHK();

      if (saved_f.get()) return EBUSY;

      saved_f = f;

      f.reset(new std::filebuf());

      if (!f.get()) {
        f = saved_f;
        return ENOMEM;
      }

      f->pubsetbuf(static_cast<char*>(buf), size);

      return 0;
    }
    void	tempbuffer_close()
    {
      if (!saved_f.get()) return;
      f = saved_f;
    }
};
#undef LR_STREAM_CHK

class LibRaw_file_datastream : public LibRaw_abstract_datastream
{
  public:
    LibRaw_file_datastream(const char *filename) 
      : LibRaw_abstract_datastream(filename)
    { 
      if (filename) {
        std::auto_ptr<std::filebuf> buf(new std::filebuf());
        buf->open(filename, std::ios_base::in | std::ios_base::binary);
        if (buf->is_open()) {
          f = buf;
        }
      }
    }
};

class LibRaw_buffer_datastream : public LibRaw_abstract_datastream
{
  public:
    LibRaw_buffer_datastream(void* buffer, size_t size)
      : LibRaw_abstract_datastream(0)
    {
      f.reset(new std::filebuf());
      f->pubsetbuf(static_cast<char*>(buffer), size);
    }
};

#endif

#endif

