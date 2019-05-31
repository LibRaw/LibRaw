/* -*- C -*-
 * File: libraw_datastream.h
 * Copyright 2008-2019 LibRaw LLC (info@libraw.org)
 * Created: Sun Jan 18 13:07:35 2009
 *
 * LibRaw Data stream interface

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#ifndef __LIBRAW_DATASTREAM_H
#define __LIBRAW_DATASTREAM_H

#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#ifndef __cplusplus

#else /* __cplusplus */
#if defined(WIN32) || defined(_WIN32) || defined(__MINGW32__)
#include <winsock2.h>
#endif
/* No unique_ptr on Apple ?? */
#if __cplusplus >= 201103L || (defined(_CPPLIB_VER) && _CPPLIB_VER >= 520)
/* OK - use unique_ptr */
#else
/* Force to use auto_ptr */
#ifndef LIBRAW_USE_AUTOPTR
#define LIBRAW_USE_AUTOPTR
#endif
#endif

#include "libraw_const.h"
#include "libraw_types.h"
#include <fstream>
#include <memory>

#if defined(WIN32) || defined(_WIN32) || defined(__MINGW32__)

/* MSVS 2008 and above... */
#if _MSC_VER >= 1500
#define WIN32SECURECALLS
#endif
#endif

#ifdef USE_DNGSDK

#if defined(WIN32) || defined(_WIN32) || defined(__MINGW32__)
#define qWinOS 1
#define qMacOS 0
#elif defined(__APPLE__)
#define qWinOS 0
#define qMacOS 1
#else
/* define OS types for DNG here */
#endif
#define qDNGXMPDocOps 0
#define qDNGUseLibJPEG 1
#define qDNGXMPFiles 0
#define qDNGExperimental 1
#define qDNGThreadSafe 1
#include "dng_stream.h"
#endif /* DNGSDK */

#define IOERROR()                                                                                                      \
  do                                                                                                                   \
  {                                                                                                                    \
    throw LIBRAW_EXCEPTION_IO_EOF;                                                                                     \
  } while (0)

class LibRaw_buffer_datastream;
class LibRaw_bit_buffer;

class DllDef LibRaw_abstract_datastream
{
public:
  LibRaw_abstract_datastream() { substream = 0; };
  virtual ~LibRaw_abstract_datastream(void)
  {
    if (substream)
      delete substream;
  }
  virtual int valid() = 0;
  virtual int read(void *, size_t, size_t) = 0;
  virtual int seek(INT64, int) = 0;
  virtual INT64 tell() = 0;
  virtual INT64 size() = 0;
  virtual int get_char() = 0;
  virtual char *gets(char *, int) = 0;
  virtual int scanf_one(const char *, void *) = 0;
  virtual int eof() = 0;
  virtual void *make_jas_stream() = 0;
  virtual int jpeg_src(void *) { return -1; }
  /* reimplement in subclass to use parallel access in xtrans_load_raw() if OpenMP is not used */
  virtual int lock() { return 1; } /* success */
  virtual void unlock() {}
  /* subfile parsing not implemented in base class */
  virtual const char *fname() { return NULL; };
#if defined(_WIN32) && !defined(__MINGW32__) && defined(_MSC_VER) && (_MSC_VER > 1310)
  virtual const wchar_t *wfname() { return NULL; };
  virtual int subfile_open(const wchar_t *) { return -1; }
#endif
  virtual int subfile_open(const char *) { return -1; }
  virtual void subfile_close() {}

  virtual int tempbuffer_open(void *, size_t);
  virtual void tempbuffer_close();

protected:
  LibRaw_abstract_datastream *substream;
};

#if defined(WIN32) || defined(_WIN32)
#ifdef LIBRAW_USE_AUTOPTR
template class DllDef std::auto_ptr<std::streambuf>;
#else
template class DllDef std::unique_ptr<std::streambuf>;
#endif
#endif

class DllDef LibRaw_file_datastream : public LibRaw_abstract_datastream
{
protected:
#ifdef LIBRAW_USE_AUTOPTR
  std::auto_ptr<std::streambuf> f;       /* will close() automatically through dtor */
  std::auto_ptr<std::streambuf> saved_f; /* when *f is a subfile, *saved_f is the master file */
#else
  std::unique_ptr<std::streambuf> f;
  std::unique_ptr<std::streambuf> saved_f;
#endif
  std::string filename;
  INT64 _fsize;
#if defined(WIN32) || defined(_WIN32)
  std::wstring wfilename;
#endif
  FILE *jas_file;

public:
  virtual ~LibRaw_file_datastream();
  LibRaw_file_datastream(const char *fname);
#if defined(_WIN32) && !defined(__MINGW32__) && defined(_MSC_VER) && (_MSC_VER > 1310)
  LibRaw_file_datastream(const wchar_t *fname);
#endif
  virtual void *make_jas_stream();
  virtual int jpeg_src(void *jpegdata);
  virtual int valid();
  virtual int read(void *ptr, size_t size, size_t nmemb);
  virtual int eof();
  virtual int seek(INT64 o, int whence);
  virtual INT64 tell();
  virtual INT64 size() { return _fsize; }
  virtual int get_char()
  {
    if (substream)
      return substream->get_char();
    return f->sbumpc();
  }
  virtual char *gets(char *str, int sz);
  virtual int scanf_one(const char *fmt, void *val);
  virtual const char *fname();
#if defined(_WIN32) && !defined(__MINGW32__) && defined(_MSC_VER) && (_MSC_VER > 1310)
  virtual const wchar_t *wfname();
  virtual int subfile_open(const wchar_t *fn);
#endif
  virtual int subfile_open(const char *fn);
  virtual void subfile_close();
};

class DllDef LibRaw_buffer_datastream : public LibRaw_abstract_datastream
{
public:
  LibRaw_buffer_datastream(void *buffer, size_t bsize);
  virtual ~LibRaw_buffer_datastream();
  virtual int valid();
  virtual void *make_jas_stream();
  virtual int jpeg_src(void *jpegdata);
  virtual int read(void *ptr, size_t sz, size_t nmemb);
  virtual int eof();
  virtual int seek(INT64 o, int whence);
  virtual INT64 tell();
  virtual INT64 size() { return streamsize; }
  virtual char *gets(char *s, int sz);
  virtual int scanf_one(const char *fmt, void *val);
  virtual int get_char()
  {
    if (substream)
      return substream->get_char();
    if (streampos >= streamsize)
      return -1;
    return buf[streampos++];
  }

private:
  unsigned char *buf;
  size_t streampos, streamsize;
};

class DllDef LibRaw_bigfile_datastream : public LibRaw_abstract_datastream
{
public:
  LibRaw_bigfile_datastream(const char *fname);
#if defined(_WIN32) && !defined(__MINGW32__) && defined(_MSC_VER) && (_MSC_VER > 1310)
  LibRaw_bigfile_datastream(const wchar_t *fname);
#endif
  virtual ~LibRaw_bigfile_datastream();
  virtual int valid();
  virtual int jpeg_src(void *jpegdata);
  virtual void *make_jas_stream();

  virtual int read(void *ptr, size_t size, size_t nmemb);
  virtual int eof();
  virtual int seek(INT64 o, int whence);
  virtual INT64 tell();
  virtual INT64 size() { return _fsize; }
  virtual char *gets(char *str, int sz);
  virtual int scanf_one(const char *fmt, void *val);
  virtual const char *fname();
#if defined(_WIN32) && !defined(__MINGW32__) && defined(_MSC_VER) && (_MSC_VER > 1310)
  virtual const wchar_t *wfname();
  virtual int subfile_open(const wchar_t *fn);
#endif
  virtual int subfile_open(const char *fn);
  virtual void subfile_close();
  virtual int get_char()
  {
#if !defined(_WIN32) && !defined(__MINGW32__)
    return substream ? substream->get_char() : getc_unlocked(f);
#else
    return substream ? substream->get_char() : fgetc(f);
#endif
  }

protected:
  FILE *f, *sav;
  std::string filename;
  INT64 _fsize;
#if defined(WIN32) || defined(_WIN32)
  std::wstring wfilename;
#endif
};

#if defined(WIN32) || defined(_WIN32)
class DllDef LibRaw_windows_datastream : public LibRaw_buffer_datastream
{
public:
  /* ctor: high level constructor opens a file by name */
  LibRaw_windows_datastream(const TCHAR *sFile);
  /* ctor: construct with a file handle - caller is responsible for closing the file handle */
  LibRaw_windows_datastream(HANDLE hFile);
  /* dtor: unmap and close the mapping handle */
  virtual ~LibRaw_windows_datastream();
  virtual INT64 size() { return cbView_; }

protected:
  void Open(HANDLE hFile);
  inline void reconstruct_base()
  {
    /* this subterfuge is to overcome the private-ness of LibRaw_buffer_datastream */
    (LibRaw_buffer_datastream &)*this = LibRaw_buffer_datastream(pView_, (size_t)cbView_);
  }

  HANDLE hMap_;    /* handle of the file mapping */
  void *pView_;    /* pointer to the mapped memory */
  __int64 cbView_; /* size of the mapping in bytes */
};

#endif

#ifdef USE_DNGSDK

class libraw_dng_stream : public dng_stream
{
public:
  libraw_dng_stream(LibRaw_abstract_datastream *p)
      : dng_stream((dng_abort_sniffer *)NULL, kBigBufferSize, 0), parent_stream(p)
  {
    if (parent_stream)
    {
      off = parent_stream->tell();
      parent_stream->seek(0UL, SEEK_SET); /* seek to start */
    }
  }
  ~libraw_dng_stream()
  {
    if (parent_stream)
      parent_stream->seek(off, SEEK_SET);
  }
  virtual uint64 DoGetLength()
  {
    if (parent_stream)
      return parent_stream->size();
    return 0;
  }
  virtual void DoRead(void *data, uint32 count, uint64 offset)
  {
    if (parent_stream)
    {
      parent_stream->seek(offset, SEEK_SET);
      parent_stream->read(data, 1, count);
    }
  }

private:
  libraw_dng_stream(const libraw_dng_stream &stream);
  libraw_dng_stream &operator=(const libraw_dng_stream &stream);
  LibRaw_abstract_datastream *parent_stream;
  INT64 off;
};

#endif

#endif /* cplusplus */

#endif
