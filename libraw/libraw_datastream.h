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

class LibRaw_buffer_datastream;

class LibRaw_abstract_datastream
{
  public:
    LibRaw_abstract_datastream(){substream=0;};
    virtual             ~LibRaw_abstract_datastream(void){if(substream) delete substream;}
    virtual int         valid(){return 0;}
    virtual int         read(void *,size_t, size_t ){ return -1;}
    virtual int         seek(INT64 , int ){return -1;}
    virtual INT64       tell(){return -1;}
    virtual int         get_char(){return -1;}
    virtual char*       gets(char *, int){ return NULL;}
    virtual int         scanf_one(const char *, void *){return -1;}
    virtual int         eof(){return -1;}

    virtual const char* fname(){ return NULL;};
    virtual int         subfile_open(const char*){ return EINVAL;}
    virtual void        subfile_close(){}
    virtual int		tempbuffer_open(void*, size_t);
    virtual void	tempbuffer_close()
    {
        if(substream) delete substream;
        substream = NULL;
    }

  protected:
    LibRaw_abstract_datastream *substream;
};


class LibRaw_file_datastream : public LibRaw_abstract_datastream
{
  public:
                        LibRaw_file_datastream(const char *fname); 
    virtual             ~LibRaw_file_datastream();
    virtual int         valid() { return f?1:0;}

    virtual int         read(void * ptr,size_t size, size_t nmemb); 
    virtual int         eof(); 
    virtual int         seek(INT64 o, int whence); 
    virtual INT64       tell(); 
    virtual int         get_char(); 
    virtual char* gets(char *str, int sz);
    virtual int scanf_one(const char *fmt, void*val); 
    virtual const char *fname() { return filename; }
    virtual int subfile_open(const char *fn);
    virtual void subfile_close();

  private:
    FILE *f,*sav;
    const char *filename;
};

class LibRaw_buffer_datastream : public LibRaw_abstract_datastream
{
  public:
    LibRaw_buffer_datastream(void *buffer, size_t bsize);
    virtual ~LibRaw_buffer_datastream(){}
    virtual int valid() { return buf?1:0;}
    virtual int read(void * ptr,size_t sz, size_t nmemb); 
    virtual int eof(); 
    virtual int seek(INT64 o, int whence); 
    virtual INT64 tell(); 
    virtual int get_char(); 
    virtual char* gets(char *s, int sz); 
    virtual int scanf_one(const char *fmt, void* val); 

  private:
    unsigned char *buf;
    size_t   streampos,streamsize;
};

inline int LibRaw_abstract_datastream::tempbuffer_open(void  *buf, size_t size)
{
    if(substream) return EBUSY;
    substream = new LibRaw_buffer_datastream(buf,size);
    return substream?0:EINVAL;
}


#endif

#endif

