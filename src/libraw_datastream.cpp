/* -*- C++ -*-
 * File: libraw_datastream.cpp
 * Copyright 2008-2020 LibRaw LLC (info@libraw.org)
 *
 * LibRaw C++ interface (implementation)

 LibRaw is free software; you can redistribute it and/or modify
 it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

*/

#ifdef _WIN32
#ifdef __MINGW32__
#define _WIN32_WINNT 0x0500
#include <stdexcept>
#endif
#endif

#define LIBRAW_LIBRARY_BUILD
#include "libraw/libraw.h"
#include "libraw/libraw_types.h"
#include "libraw/libraw_datastream.h"
#include <sys/stat.h>
#ifdef USE_JASPER
#include <jasper/jasper.h> /* Decode RED camera movies */
#else
#define NO_JASPER
#endif
#ifdef USE_JPEG
#include <jpeglib.h>
#include <jerror.h>
#else
#define NO_JPEG
#endif

#ifdef USE_JPEG

typedef struct
{
    struct jpeg_source_mgr pub; /* public fields */
    LibRaw_abstract_datastream *instream;            /* source stream */
    JOCTET *buffer;             /* start of buffer */
    boolean start_of_file;      /* have we gotten any data yet? */
} lr_jpg_source_mgr;

typedef lr_jpg_source_mgr *lr_jpg_src_ptr;

#define LR_JPEG_INPUT_BUF_SIZE 16384 

static void f_init_source(j_decompress_ptr cinfo)
{
    lr_jpg_src_ptr src = (lr_jpg_src_ptr)cinfo->src;
    src->start_of_file = TRUE;
}

#ifdef ERREXIT
#undef ERREXIT
#endif

#define ERREXIT(cinfo, code)                                                   \
  ((cinfo)->err->msg_code = (code),                                            \
   (*(cinfo)->err->error_exit)((j_common_ptr)(cinfo)))

static boolean lr_fill_input_buffer(j_decompress_ptr cinfo)
{
    lr_jpg_src_ptr src = (lr_jpg_src_ptr)cinfo->src;
    size_t nbytes;

    nbytes = src->instream->read((void*)src->buffer, 1, LR_JPEG_INPUT_BUF_SIZE);

    if (nbytes <= 0)
    {
        if (src->start_of_file) /* Treat empty input file as fatal error */
            ERREXIT(cinfo, JERR_INPUT_EMPTY);
        WARNMS(cinfo, JWRN_JPEG_EOF);
        /* Insert a fake EOI marker */
        src->buffer[0] = (JOCTET)0xFF;
        src->buffer[1] = (JOCTET)JPEG_EOI;
        nbytes = 2;
    }

    src->pub.next_input_byte = src->buffer;
    src->pub.bytes_in_buffer = nbytes;
    src->start_of_file = FALSE;
    return TRUE;
}

static void lr_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    struct jpeg_source_mgr *src = cinfo->src;
    if (num_bytes > 0)
    {
        while (num_bytes > (long)src->bytes_in_buffer)
        {
            num_bytes -= (long)src->bytes_in_buffer;
            (void)(*src->fill_input_buffer)(cinfo);
            /* note we assume that fill_input_buffer will never return FALSE,
             * so suspension need not be handled.
             */
        }
        src->next_input_byte += (size_t)num_bytes;
        src->bytes_in_buffer -= (size_t)num_bytes;
    }
}

static void lr_term_source(j_decompress_ptr cinfo) {}

static void lr_jpeg_src(j_decompress_ptr cinfo, LibRaw_abstract_datastream *inf)
{
    lr_jpg_src_ptr src;
    if (cinfo->src == NULL)
    { /* first time for this JPEG object? */
        cinfo->src = (struct jpeg_source_mgr *)(*cinfo->mem->alloc_small)(
            (j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof(lr_jpg_source_mgr));
        src = (lr_jpg_src_ptr)cinfo->src;
        src->buffer = (JOCTET *)(*cinfo->mem->alloc_small)(
            (j_common_ptr)cinfo, JPOOL_PERMANENT,
            LR_JPEG_INPUT_BUF_SIZE * sizeof(JOCTET));
    }
    else if (cinfo->src->init_source != f_init_source)
    {
        ERREXIT(cinfo, JERR_BUFFER_SIZE);
    }

    src = (lr_jpg_src_ptr)cinfo->src;
    src->pub.init_source = f_init_source;
    src->pub.fill_input_buffer = lr_fill_input_buffer;
    src->pub.skip_input_data = lr_skip_input_data;
    src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
    src->pub.term_source = lr_term_source;
    src->instream = inf;
    src->pub.bytes_in_buffer = 0;    /* forces fill_input_buffer on first read */
    src->pub.next_input_byte = NULL; /* until buffer loaded */
}
#endif

int LibRaw_abstract_datastream::jpeg_src(void *jpegdata)
{
#ifdef NO_JPEG
    return -1;
#else
    j_decompress_ptr cinfo = (j_decompress_ptr)jpegdata;
    buffering_off();
    lr_jpeg_src(cinfo, this);
    return 0; // OK
#endif
}


// == LibRaw_file_datastream ==

LibRaw_file_datastream::~LibRaw_file_datastream()
{
  if (jas_file)
    fclose(jas_file);
}

LibRaw_file_datastream::LibRaw_file_datastream(const char *fname)
    : filename(fname), _fsize(0)
#ifdef LIBRAW_WIN32_UNICODEPATHS
      ,
      wfilename()
#endif
      ,
      jas_file(NULL)
{
  if (filename.size() > 0)
  {
#ifndef LIBRAW_WIN32_CALLS
    struct stat st;
    if (!stat(filename.c_str(), &st))
      _fsize = st.st_size;
#else
    struct _stati64 st;
    if (!_stati64(filename.c_str(), &st))
      _fsize = st.st_size;
#endif
#ifdef LIBRAW_USE_AUTOPTR
    std::auto_ptr<std::filebuf> buf(new std::filebuf());
#else
    std::unique_ptr<std::filebuf> buf(new std::filebuf());
#endif
    buf->open(filename.c_str(), std::ios_base::in | std::ios_base::binary);
    if (buf->is_open())
    {
#ifdef LIBRAW_USE_AUTOPTR
      f = buf;
#else
      f = std::move(buf);
#endif
    }
  }
}
#ifdef LIBRAW_WIN32_UNICODEPATHS
LibRaw_file_datastream::LibRaw_file_datastream(const wchar_t *fname)
    : filename(), wfilename(fname), jas_file(NULL), _fsize(0)
{
  if (wfilename.size() > 0)
  {
    struct _stati64 st;
    if (!_wstati64(wfilename.c_str(), &st))
      _fsize = st.st_size;
#ifdef LIBRAW_USE_AUTOPTR
    std::auto_ptr<std::filebuf> buf(new std::filebuf());
#else
    std::unique_ptr<std::filebuf> buf(new std::filebuf());
#endif
    buf->open(wfilename.c_str(), std::ios_base::in | std::ios_base::binary);
    if (buf->is_open())
    {
#ifdef LIBRAW_USE_AUTOPTR
      f = buf;
#else
      f = std::move(buf);
#endif
	}
  }
}
const wchar_t *LibRaw_file_datastream::wfname()
{
  return wfilename.size() > 0 ? wfilename.c_str() : NULL;
}
#endif

int LibRaw_file_datastream::valid() { return f.get() ? 1 : 0; }

#define LR_STREAM_CHK()                                                        \
  do                                                                           \
  {                                                                            \
    if (!f.get())                                                              \
      throw LIBRAW_EXCEPTION_IO_EOF;                                           \
  } while (0)

int LibRaw_file_datastream::read(void *ptr, size_t size, size_t nmemb)
{
/* Visual Studio 2008 marks sgetn as insecure, but VS2010 does not. */
#if defined(WIN32SECURECALLS) && (_MSC_VER < 1600)
  LR_STREAM_CHK();
  return int(f->_Sgetn_s(static_cast<char *>(ptr), nmemb * size, nmemb * size) /
             (size > 0 ? size : 1));
#else
  LR_STREAM_CHK();
  return int(f->sgetn(static_cast<char *>(ptr), std::streamsize(nmemb * size)) /
             (size > 0 ? size : 1));
#endif
}

int LibRaw_file_datastream::eof()
{
  LR_STREAM_CHK();
  return f->sgetc() == EOF;
}

int LibRaw_file_datastream::seek(INT64 o, int whence)
{
  LR_STREAM_CHK();
  std::ios_base::seekdir dir;
  switch (whence)
  {
  case SEEK_SET:
    dir = std::ios_base::beg;
    break;
  case SEEK_CUR:
    dir = std::ios_base::cur;
    break;
  case SEEK_END:
    dir = std::ios_base::end;
    break;
  default:
    dir = std::ios_base::beg;
  }
  return f->pubseekoff((long)o, dir) < 0;
}

INT64 LibRaw_file_datastream::tell()
{
  LR_STREAM_CHK();
  return f->pubseekoff(0, std::ios_base::cur);
}

char *LibRaw_file_datastream::gets(char *str, int sz)
{
  LR_STREAM_CHK();
  std::istream is(f.get());
  is.getline(str, sz);
  if (is.fail())
    return 0;
  return str;
}

int LibRaw_file_datastream::scanf_one(const char *fmt, void *val)
{
  LR_STREAM_CHK();

  std::istream is(f.get());

  /* HUGE ASSUMPTION: *fmt is either "%d" or "%f" */
  if (strcmp(fmt, "%d") == 0)
  {
    int d;
    is >> d;
    if (is.fail())
      return EOF;
    *(static_cast<int *>(val)) = d;
  }
  else
  {
    float f;
    is >> f;
    if (is.fail())
      return EOF;
    *(static_cast<float *>(val)) = f;
  }

  return 1;
}

const char *LibRaw_file_datastream::fname()
{
  return filename.size() > 0 ? filename.c_str() : NULL;
}

#undef LR_STREAM_CHK

void *LibRaw_file_datastream::make_jas_stream()
{
#ifdef NO_JASPER
  return NULL;
#else
#ifdef LIBRAW_WIN32_UNICODEPATHS
  if (wfname())
  {
    jas_file = _wfopen(wfname(), L"rb");
    return jas_stream_fdopen(fileno(jas_file), "rb");
  }
  else
#endif
  {
    return jas_stream_fopen(fname(), "rb");
  }
#endif
}

// == LibRaw_buffer_datastream
LibRaw_buffer_datastream::LibRaw_buffer_datastream(void *buffer, size_t bsize)
{
  buf = (unsigned char *)buffer;
  streampos = 0;
  streamsize = bsize;
}

LibRaw_buffer_datastream::~LibRaw_buffer_datastream() {}

int LibRaw_buffer_datastream::read(void *ptr, size_t sz, size_t nmemb)
{
  size_t to_read = sz * nmemb;
  if (to_read > streamsize - streampos)
    to_read = streamsize - streampos;
  if (to_read < 1)
    return 0;
  memmove(ptr, buf + streampos, to_read);
  streampos += to_read;
  return int((to_read + sz - 1) / (sz > 0 ? sz : 1));
}

int LibRaw_buffer_datastream::seek(INT64 o, int whence)
{
  switch (whence)
  {
  case SEEK_SET:
    if (o < 0)
      streampos = 0;
    else if (size_t(o) > streamsize)
      streampos = streamsize;
    else
      streampos = size_t(o);
    return 0;
  case SEEK_CUR:
    if (o < 0)
    {
      if (size_t(-o) >= streampos)
        streampos = 0;
      else
        streampos += (size_t)o;
    }
    else if (o > 0)
    {
      if (o + streampos > streamsize)
        streampos = streamsize;
      else
        streampos += (size_t)o;
    }
    return 0;
  case SEEK_END:
    if (o > 0)
      streampos = streamsize;
    else if (size_t(-o) > streamsize)
      streampos = 0;
    else
      streampos = streamsize + (size_t)o;
    return 0;
  default:
    return 0;
  }
}

INT64 LibRaw_buffer_datastream::tell()
{
  return INT64(streampos);
}

char *LibRaw_buffer_datastream::gets(char *s, int sz)
{
  unsigned char *psrc, *pdest, *str;
  str = (unsigned char *)s;
  psrc = buf + streampos;
  pdest = str;
  if(streampos >= streamsize) return NULL;
  while ((size_t(psrc - buf) < streamsize) && ((pdest - str) < sz))
  {
    *pdest = *psrc;
    if (*psrc == '\n')
      break;
    psrc++;
    pdest++;
  }
  if (size_t(psrc - buf) < streamsize)
    psrc++;
  if ((pdest - str) < sz)
    *(++pdest) = 0;
  streampos = psrc - buf;
  return s;
}

int LibRaw_buffer_datastream::scanf_one(const char *fmt, void *val)
{
  int scanf_res;
  if (streampos > streamsize)
    return 0;
#ifndef WIN32SECURECALLS
  scanf_res = sscanf((char *)(buf + streampos), fmt, val);
#else
  scanf_res = sscanf_s((char *)(buf + streampos), fmt, val);
#endif
  if (scanf_res > 0)
  {
    int xcnt = 0;
    while (streampos < streamsize)
    {
      streampos++;
      xcnt++;
      if (buf[streampos] == 0 || buf[streampos] == ' ' ||
          buf[streampos] == '\t' || buf[streampos] == '\n' || xcnt > 24)
        break;
    }
  }
  return scanf_res;
}

int LibRaw_buffer_datastream::eof()
{
  return streampos >= streamsize;
}
int LibRaw_buffer_datastream::valid() { return buf ? 1 : 0; }

void *LibRaw_buffer_datastream::make_jas_stream()
{
#ifdef NO_JASPER
  return NULL;
#else
  return jas_stream_memopen((char *)buf + streampos, streamsize - streampos);
#endif
}

int LibRaw_buffer_datastream::jpeg_src(void *jpegdata)
{
#if defined(NO_JPEG) || !defined(USE_JPEG)
  return -1;
#else
  j_decompress_ptr cinfo = (j_decompress_ptr)jpegdata;
  jpeg_mem_src(cinfo, (unsigned char *)buf + streampos, streamsize - streampos);
  return 0;
#endif
}

// int LibRaw_buffer_datastream

// == LibRaw_bigfile_datastream
LibRaw_bigfile_datastream::LibRaw_bigfile_datastream(const char *fname)
    : filename(fname)
#ifdef LIBRAW_WIN32_UNICODEPATHS
      ,
      wfilename()
#endif
{
  if (filename.size() > 0)
  {
#ifndef LIBRAW_WIN32_CALLS
    struct stat st;
    if (!stat(filename.c_str(), &st))
      _fsize = st.st_size;
#else
    struct _stati64 st;
    if (!_stati64(filename.c_str(), &st))
      _fsize = st.st_size;
#endif

#ifndef WIN32SECURECALLS
    f = fopen(fname, "rb");
#else
    if (fopen_s(&f, fname, "rb"))
      f = 0;
#endif
  }
  else
  {
    filename = std::string();
    f = 0;
  }
}

#ifdef LIBRAW_WIN32_UNICODEPATHS
LibRaw_bigfile_datastream::LibRaw_bigfile_datastream(const wchar_t *fname)
    : filename(), wfilename(fname)
{
  if (wfilename.size() > 0)
  {
    struct _stati64 st;
    if (!_wstati64(wfilename.c_str(), &st))
      _fsize = st.st_size;
#ifndef WIN32SECURECALLS
    f = _wfopen(wfilename.c_str(), L"rb");
#else
    if (_wfopen_s(&f, fname, L"rb"))
      f = 0;
#endif
  }
  else
  {
    wfilename = std::wstring();
    f = 0;
  }
}
const wchar_t *LibRaw_bigfile_datastream::wfname()
{
  return wfilename.size() > 0 ? wfilename.c_str() : NULL;
}
#endif

LibRaw_bigfile_datastream::~LibRaw_bigfile_datastream()
{
  if (f)
    fclose(f);
}
int LibRaw_bigfile_datastream::valid() { return f ? 1 : 0; }

#define LR_BF_CHK()                                                            \
  do                                                                           \
  {                                                                            \
    if (!f)                                                                    \
      throw LIBRAW_EXCEPTION_IO_EOF;                                           \
  } while (0)

int LibRaw_bigfile_datastream::read(void *ptr, size_t size, size_t nmemb)
{
  LR_BF_CHK();
  return int(fread(ptr, size, nmemb, f));
}

int LibRaw_bigfile_datastream::eof()
{
  LR_BF_CHK();
  return feof(f);
}

int LibRaw_bigfile_datastream::seek(INT64 o, int whence)
{
  LR_BF_CHK();
#if defined(_WIN32)
#ifdef WIN32SECURECALLS
  return _fseeki64(f, o, whence);
#else
  return fseek(f, (long)o, whence);
#endif
#else
  return fseeko(f, o, whence);
#endif
}

INT64 LibRaw_bigfile_datastream::tell()
{
  LR_BF_CHK();
#if defined(_WIN32)
#ifdef WIN32SECURECALLS
  return _ftelli64(f);
#else
  return ftell(f);
#endif
#else
  return ftello(f);
#endif
}

char *LibRaw_bigfile_datastream::gets(char *str, int sz)
{
  LR_BF_CHK();
  return fgets(str, sz, f);
}

int LibRaw_bigfile_datastream::scanf_one(const char *fmt, void *val)
{
  LR_BF_CHK();
  return 
#ifndef WIN32SECURECALLS
                   fscanf(f, fmt, val)
#else
                   fscanf_s(f, fmt, val)
#endif
      ;
}

const char *LibRaw_bigfile_datastream::fname()
{
  return filename.size() > 0 ? filename.c_str() : NULL;
}


void *LibRaw_bigfile_datastream::make_jas_stream()
{
#ifdef NO_JASPER
  return NULL;
#else
  return jas_stream_fdopen(fileno(f), "rb");
#endif
}


// == LibRaw_windows_datastream
#ifdef LIBRAW_WIN32_CALLS

LibRaw_windows_datastream::LibRaw_windows_datastream(const TCHAR *sFile)
    : LibRaw_buffer_datastream(NULL, 0), hMap_(0), pView_(NULL)
{
  HANDLE hFile = CreateFile(sFile, GENERIC_READ, 0, 0, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, 0);
  if (hFile == INVALID_HANDLE_VALUE)
    throw std::runtime_error("failed to open the file");

  try
  {
    Open(hFile);
  }
  catch (...)
  {
    CloseHandle(hFile);
    throw;
  }

  CloseHandle(hFile); // windows will defer the actual closing of this handle
                      // until the hMap_ is closed
  reconstruct_base();
}

// ctor: construct with a file handle - caller is responsible for closing the
// file handle
LibRaw_windows_datastream::LibRaw_windows_datastream(HANDLE hFile)
    : LibRaw_buffer_datastream(NULL, 0), hMap_(0), pView_(NULL)
{
  Open(hFile);
  reconstruct_base();
}

// dtor: unmap and close the mapping handle
LibRaw_windows_datastream::~LibRaw_windows_datastream()
{
  if (pView_ != NULL)
    ::UnmapViewOfFile(pView_);

  if (hMap_ != 0)
    ::CloseHandle(hMap_);
}

void LibRaw_windows_datastream::Open(HANDLE hFile)
{
  // create a file mapping handle on the file handle
  hMap_ = ::CreateFileMapping(hFile, 0, PAGE_READONLY, 0, 0, 0);
  if (hMap_ == NULL)
    throw std::runtime_error("failed to create file mapping");

  // now map the whole file base view
  if (!::GetFileSizeEx(hFile, (PLARGE_INTEGER)&cbView_))
    throw std::runtime_error("failed to get the file size");

  pView_ = ::MapViewOfFile(hMap_, FILE_MAP_READ, 0, 0, (size_t)cbView_);
  if (pView_ == NULL)
    throw std::runtime_error("failed to map the file");
}

#endif
