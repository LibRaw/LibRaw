/* -*- C++ -*-
 * Copyright 2019-2020 LibRaw LLC (info@libraw.org)
 *
 LibRaw is free software; you can redistribute it and/or modify
 it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#include "../../internal/libraw_cxx_defs.h"

#ifndef NO_JPEG
struct jpegErrorManager
{
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};

static void jpegErrorExit(j_common_ptr cinfo)
{
  jpegErrorManager *myerr = (jpegErrorManager *)cinfo->err;
  longjmp(myerr->setjmp_buffer, 1);
}
#endif

int LibRaw::unpack_thumb(void)
{
  CHECK_ORDER_LOW(LIBRAW_PROGRESS_IDENTIFY);
  CHECK_ORDER_BIT(LIBRAW_PROGRESS_THUMB_LOAD);

#define THUMB_SIZE_CHECKT(A) \
  do { \
    if (INT64(A) > 1024ULL * 1024ULL * LIBRAW_MAX_THUMBNAIL_MB) throw LIBRAW_EXCEPTION_IO_CORRUPT; \
    if (INT64(A) > 0 &&  INT64(A) < 64ULL)        throw LIBRAW_EXCEPTION_IO_CORRUPT; \
  } while (0)

#define THUMB_SIZE_CHECKTNZ(A) \
  do { \
    if (INT64(A) > 1024ULL * 1024ULL * LIBRAW_MAX_THUMBNAIL_MB) throw LIBRAW_EXCEPTION_IO_CORRUPT; \
    if (INT64(A) < 64ULL)        throw LIBRAW_EXCEPTION_IO_CORRUPT; \
  } while (0)


#define THUMB_SIZE_CHECKWH(W,H) \
  do { \
    if (INT64(W)*INT64(H) > 1024ULL * 1024ULL * LIBRAW_MAX_THUMBNAIL_MB) throw LIBRAW_EXCEPTION_IO_CORRUPT; \
    if (INT64(W)*INT64(H) < 64ULL)        throw LIBRAW_EXCEPTION_IO_CORRUPT; \
  } while (0)

  try
  {
    if (!libraw_internal_data.internal_data.input)
      return LIBRAW_INPUT_CLOSED;

    int t_colors = libraw_internal_data.unpacker_data.thumb_misc >> 5 & 7;
    int t_bytesps = (libraw_internal_data.unpacker_data.thumb_misc & 31) / 8;

    if (!ID.toffset && !(imgdata.thumbnail.tlength > 0 &&
                         load_raw == &LibRaw::broadcom_load_raw) // RPi
    )
    {
      return LIBRAW_NO_THUMBNAIL;
    }
    else if (thumb_load_raw)
    {
      kodak_thumb_loader();
      T.tformat = LIBRAW_THUMBNAIL_BITMAP;
      SET_PROC_FLAG(LIBRAW_PROGRESS_THUMB_LOAD);
      return 0;
    }
    else
    {
#ifdef USE_X3FTOOLS
	if (write_thumb == &LibRaw::x3f_thumb_loader)
      {
        INT64 tsize = x3f_thumb_size();
        if (tsize < 2048 || INT64(ID.toffset) + tsize < 1)
          throw LIBRAW_EXCEPTION_IO_CORRUPT;

        if (INT64(ID.toffset) + tsize > ID.input->size() + THUMB_READ_BEYOND)
          throw LIBRAW_EXCEPTION_IO_EOF;
        THUMB_SIZE_CHECKT(tsize);
      }
#else
	if (0) {}
#endif
      else
      {
        if (INT64(ID.toffset) + INT64(T.tlength) < 1)
          throw LIBRAW_EXCEPTION_IO_CORRUPT;

        if (INT64(ID.toffset) + INT64(T.tlength) >
            ID.input->size() + THUMB_READ_BEYOND)
          throw LIBRAW_EXCEPTION_IO_EOF;
      }

      ID.input->seek(ID.toffset, SEEK_SET);
      if (write_thumb == &LibRaw::jpeg_thumb)
      {
        THUMB_SIZE_CHECKTNZ(T.tlength);

        if (T.thumb)
          free(T.thumb);
        T.thumb = (char *)malloc(T.tlength);
        merror(T.thumb, "jpeg_thumb()");
        ID.input->read(T.thumb, 1, T.tlength);
        unsigned char *tthumb = (unsigned char *)T.thumb;
        tthumb[0] = 0xff;
        tthumb[1] = 0xd8;
#ifdef NO_JPEG
        T.tcolors = 3;
#else
        {
          jpegErrorManager jerr;
          struct jpeg_decompress_struct cinfo;
          cinfo.err = jpeg_std_error(&jerr.pub);
          jerr.pub.error_exit = jpegErrorExit;
          if (setjmp(jerr.setjmp_buffer))
          {
          err2:
            // Error in original JPEG thumb, read it again because
            // original bytes 0-1 was damaged above
            jpeg_destroy_decompress(&cinfo);
            T.tcolors = 3;
            T.tformat = LIBRAW_THUMBNAIL_UNKNOWN;
            ID.input->seek(ID.toffset, SEEK_SET);
            ID.input->read(T.thumb, 1, T.tlength);
            SET_PROC_FLAG(LIBRAW_PROGRESS_THUMB_LOAD);
            return 0;
          }
          jpeg_create_decompress(&cinfo);
          jpeg_mem_src(&cinfo, (unsigned char *)T.thumb, T.tlength);
          int rc = jpeg_read_header(&cinfo, TRUE);
          if (rc != 1)
            goto err2;
          T.tcolors = (cinfo.num_components > 0 && cinfo.num_components <= 3)
                          ? cinfo.num_components
                          : 3;
          jpeg_destroy_decompress(&cinfo);
        }
#endif
        T.tformat = LIBRAW_THUMBNAIL_JPEG;
        SET_PROC_FLAG(LIBRAW_PROGRESS_THUMB_LOAD);
        return 0;
      }
      else if (write_thumb == &LibRaw::layer_thumb)
      {
        int colors = libraw_internal_data.unpacker_data.thumb_misc >> 5 & 7;
        if (colors != 1 && colors != 3)
          return LIBRAW_UNSUPPORTED_THUMBNAIL;

        THUMB_SIZE_CHECKWH(T.twidth, T.theight);

        int tlength = T.twidth * T.theight;
        if (T.thumb)
          free(T.thumb);
        T.thumb = (char *)calloc(colors, tlength);
        merror(T.thumb, "layer_thumb()");
        unsigned char *tbuf = (unsigned char *)calloc(colors, tlength);
        merror(tbuf, "layer_thumb()");
        ID.input->read(tbuf, colors, T.tlength);
        if (libraw_internal_data.unpacker_data.thumb_misc >> 8 &&
            colors == 3) // GRB order
          for (int i = 0; i < tlength; i++)
          {
            T.thumb[i * 3] = tbuf[i + tlength];
            T.thumb[i * 3 + 1] = tbuf[i];
            T.thumb[i * 3 + 2] = tbuf[i + 2 * tlength];
          }
        else if (colors == 3) // RGB or 1-channel
          for (int i = 0; i < tlength; i++)
          {
            T.thumb[i * 3] = tbuf[i];
            T.thumb[i * 3 + 1] = tbuf[i + tlength];
            T.thumb[i * 3 + 2] = tbuf[i + 2 * tlength];
          }
        else if (colors == 1)
        {
          free(T.thumb);
          T.thumb = (char *)tbuf;
          tbuf = 0;
        }
        if (tbuf)
          free(tbuf);
        T.tcolors = colors;
        T.tlength = colors * tlength;
        T.tformat = LIBRAW_THUMBNAIL_BITMAP;
        SET_PROC_FLAG(LIBRAW_PROGRESS_THUMB_LOAD);
        return 0;
      }
      else if (write_thumb == &LibRaw::rollei_thumb)
      {
        int i;
        THUMB_SIZE_CHECKWH(T.twidth, T.theight);
        int tlength = T.twidth * T.theight;
        if (T.thumb)
          free(T.thumb);
        T.tcolors = 3;
        T.thumb = (char *)calloc(T.tcolors, tlength);
        merror(T.thumb, "layer_thumb()");
        unsigned short *tbuf = (unsigned short *)calloc(2, tlength);
        merror(tbuf, "layer_thumb()");
        read_shorts(tbuf, tlength);
        for (i = 0; i < tlength; i++)
        {
          T.thumb[i * 3] = (tbuf[i] << 3) & 0xff;
          T.thumb[i * 3 + 1] = (tbuf[i] >> 5 << 2) & 0xff;
          T.thumb[i * 3 + 2] = (tbuf[i] >> 11 << 3) & 0xff;
        }
        free(tbuf);
        T.tlength = T.tcolors * tlength;
        T.tformat = LIBRAW_THUMBNAIL_BITMAP;
        SET_PROC_FLAG(LIBRAW_PROGRESS_THUMB_LOAD);
        return 0;
      }
      else if (write_thumb == &LibRaw::ppm_thumb)
      {
        if (t_bytesps > 1)
          throw LIBRAW_EXCEPTION_IO_CORRUPT; // 8-bit thumb, but parsed for more
                                             // bits
        THUMB_SIZE_CHECKWH(T.twidth, T.theight);
        int t_length = T.twidth * T.theight * t_colors;

        if (T.tlength &&
            (int)T.tlength < t_length) // try to find tiff ifd with needed offset
        {
          int pifd = find_ifd_by_offset(libraw_internal_data.internal_data.toffset);
          if (pifd >= 0 && tiff_ifd[pifd].strip_offsets_count &&
              tiff_ifd[pifd].strip_byte_counts_count)
          {
            // We found it, calculate final size
            unsigned total_size = 0;
            for (int i = 0; i < tiff_ifd[pifd].strip_byte_counts_count; i++)
              total_size += tiff_ifd[pifd].strip_byte_counts[i];
            if (total_size != (unsigned)t_length) // recalculate colors
            {
              if (total_size == T.twidth * T.tlength * 3)
                T.tcolors = 3;
              else if (total_size == T.twidth * T.tlength)
                T.tcolors = 1;
            }
            T.tlength = total_size;
            THUMB_SIZE_CHECKTNZ(T.tlength);
            if (T.thumb)
              free(T.thumb);
            T.thumb = (char *)malloc(T.tlength);
            merror(T.thumb, "ppm_thumb()");

            char *dest = T.thumb;
            INT64 pos = ID.input->tell();

            for (int i = 0; i < tiff_ifd[pifd].strip_byte_counts_count &&
                            i < tiff_ifd[pifd].strip_offsets_count;
                 i++)
            {
              int remain = T.tlength;
              int sz = tiff_ifd[pifd].strip_byte_counts[i];
              int off = tiff_ifd[pifd].strip_offsets[i];
              if (off >= 0 && off + sz <= ID.input->size() && sz <= remain)
              {
                ID.input->seek(off, SEEK_SET);
                ID.input->read(dest, sz, 1);
                remain -= sz;
                dest += sz;
              }
            }
            ID.input->seek(pos, SEEK_SET);
            T.tformat = LIBRAW_THUMBNAIL_BITMAP;
            SET_PROC_FLAG(LIBRAW_PROGRESS_THUMB_LOAD);
            return 0;
          }
        }

        if (!T.tlength)
          T.tlength = t_length;
        if (T.thumb)
          free(T.thumb);

        THUMB_SIZE_CHECKTNZ(T.tlength);

        T.thumb = (char *)malloc(T.tlength);
        if (!T.tcolors)
          T.tcolors = t_colors;
        merror(T.thumb, "ppm_thumb()");

        ID.input->read(T.thumb, 1, T.tlength);

        T.tformat = LIBRAW_THUMBNAIL_BITMAP;
        SET_PROC_FLAG(LIBRAW_PROGRESS_THUMB_LOAD);
        return 0;
      }
      else if (write_thumb == &LibRaw::ppm16_thumb)
      {
        if (t_bytesps > 2)
          throw LIBRAW_EXCEPTION_IO_CORRUPT; // 16-bit thumb, but parsed for
                                             // more bits
        int o_bps = (imgdata.params.raw_processing_options &
                     LIBRAW_PROCESSING_USE_PPM16_THUMBS)
                        ? 2
                        : 1;
        int o_length = T.twidth * T.theight * t_colors * o_bps;
        int i_length = T.twidth * T.theight * t_colors * 2;
        if (!T.tlength)
          T.tlength = o_length;
        THUMB_SIZE_CHECKTNZ(o_length);
        THUMB_SIZE_CHECKTNZ(i_length);
        THUMB_SIZE_CHECKTNZ(T.tlength);

        ushort *t_thumb = (ushort *)calloc(i_length, 1);
        ID.input->read(t_thumb, 1, i_length);
        if ((libraw_internal_data.unpacker_data.order == 0x4949) ==
            (ntohs(0x1234) == 0x1234))
          swab((char *)t_thumb, (char *)t_thumb, i_length);

        if (T.thumb)
          free(T.thumb);
        if ((imgdata.params.raw_processing_options &
             LIBRAW_PROCESSING_USE_PPM16_THUMBS))
        {
          T.thumb = (char *)t_thumb;
          T.tformat = LIBRAW_THUMBNAIL_BITMAP16;
        }
        else
        {
          T.thumb = (char *)malloc(o_length);
          merror(T.thumb, "ppm_thumb()");
          for (int i = 0; i < o_length; i++)
            T.thumb[i] = t_thumb[i] >> 8;
          free(t_thumb);
          T.tformat = LIBRAW_THUMBNAIL_BITMAP;
        }
        SET_PROC_FLAG(LIBRAW_PROGRESS_THUMB_LOAD);
        return 0;
      }
#ifdef USE_X3FTOOLS
	  else if (write_thumb == &LibRaw::x3f_thumb_loader)
      {
        x3f_thumb_loader();
        SET_PROC_FLAG(LIBRAW_PROGRESS_THUMB_LOAD);
        return 0;
      }
#endif
      else
      {
        return LIBRAW_UNSUPPORTED_THUMBNAIL;
      }
    }
    // last resort
    return LIBRAW_UNSUPPORTED_THUMBNAIL;
  }
  catch (LibRaw_exceptions err)
  {
    EXCEPTION_HANDLER(err);
  }
}
