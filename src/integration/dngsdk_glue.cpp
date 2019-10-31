/* -*- C++ -*-
 * Copyright 2019 LibRaw LLC (info@libraw.org)
 *
 LibRaw is free software; you can redistribute it and/or modify
 it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#include "../../internal/libraw_cxx_defs.h"

#if defined (USE_GPRSDK) && !defined(USE_DNGSDK)
#error  GPR (GoPro) SDK should be used with Adobe DNG SDK
#endif
#ifdef USE_DNGSDK
#include "dng_read_image.h"
#endif
#ifdef USE_GPRSDK
#include "gpr_read_image.h"
#endif

int LibRaw::valid_for_dngsdk()
{
#ifndef USE_DNGSDK
  return 0;
#else
  if (!imgdata.idata.dng_version)
    return 0;
#ifdef USE_GPRSDK
  if (load_raw == &LibRaw::vc5_dng_load_raw_placeholder) // regardless of flags or use_dngsdk value!
      return 1;
#endif
  if (!imgdata.params.use_dngsdk)
    return 0;
  if (load_raw == &LibRaw::lossy_dng_load_raw) // WHY??
    return 0;
  if (load_raw ==
      &LibRaw::float_dng_load_raw_placeholder) // regardless of flags!
    return 1;
  if (is_floating_point() && (imgdata.params.use_dngsdk & LIBRAW_DNG_FLOAT))
    return 1;
  if (!imgdata.idata.filters && (imgdata.params.use_dngsdk & LIBRAW_DNG_LINEAR))
    return 1;
  if (libraw_internal_data.unpacker_data.tiff_bps == 8 &&
      (imgdata.params.use_dngsdk & LIBRAW_DNG_8BIT))
    return 1;
  if (libraw_internal_data.unpacker_data.tiff_compress == 8 &&
      (imgdata.params.use_dngsdk & LIBRAW_DNG_DEFLATE))
    return 1;
  if (libraw_internal_data.unpacker_data.tiff_samples == 2)
    return 0; // Always deny 2-samples (old fuji superccd)
  if (imgdata.idata.filters == 9 &&
      (imgdata.params.use_dngsdk & LIBRAW_DNG_XTRANS))
    return 1;
  if (is_fuji_rotated())
    return 0; // refuse
  if (imgdata.params.use_dngsdk & LIBRAW_DNG_OTHER)
    return 1;
  return 0;
#endif
}

#ifdef USE_DNGSDK
static dng_ifd* search_single_ifd(const std::vector <dng_ifd *>& v, uint64 offset)
{
    for(int i = 0; i < v.size(); i++)
    {
        if(!v[i]) continue;;
        if(v[i]->fTileOffsetsOffset == offset) return v[i];
        if(v[i]->fTileOffsetsCount == 1 && v[i]->fTileOffset[0] == offset) return v[i];
    }
    return NULL;
}

static dng_ifd* search_for_ifd(const dng_info& info, uint64 offset, ushort w, ushort h)
{
    dng_ifd *ret = 0;
    ret = search_single_ifd(info.fIFD,offset);
    if(!ret) ret = search_single_ifd(info.fChainedIFD,offset);
    if(!ret)
        for(int c = 0; !ret && c < info.fChainedSubIFD.size(); c++)
            ret = search_single_ifd(info.fChainedSubIFD[c],offset);
    if(ret && (ret->fImageLength == h) && ret->fImageWidth == w)
        return ret;
    return 0;
}
#endif


int LibRaw::try_dngsdk()
{
#ifdef USE_DNGSDK
  if (!dnghost)
    return LIBRAW_UNSPECIFIED_ERROR;

  dng_host *host = static_cast<dng_host *>(dnghost);

  try
  {
    libraw_dng_stream stream(libraw_internal_data.internal_data.input);

    AutoPtr<dng_negative> negative;
    negative.Reset(host->Make_dng_negative());

    dng_info info;
    info.Parse(*host, stream);
    info.PostParse(*host);

    if (!info.IsValidDNG())
    {
      return LIBRAW_DATA_ERROR;
    }
    negative->Parse(*host, stream, info);
    negative->PostParse(*host, stream, info);
    dng_ifd *rawIFD = search_for_ifd(info,libraw_internal_data.unpacker_data.data_offset,imgdata.sizes.raw_width,imgdata.sizes.raw_height);
    if(!rawIFD)
        return LIBRAW_DATA_ERROR;

    AutoPtr<dng_simple_image> stage2(new dng_simple_image(rawIFD->Bounds (),rawIFD->fSamplesPerPixel,rawIFD->PixelType (), host->Allocator()));
#ifdef USE_GPRSDK
    if(libraw_internal_data.unpacker_data.tiff_compress == 9)
    {
        gpr_allocator allocator;
        allocator.Alloc = ::malloc;
        allocator.Free = ::free;
        gpr_buffer_auto vc5_image_obj( allocator.Alloc, allocator.Free );

        gpr_read_image reader(&vc5_image_obj);
        reader.Read(*host,*rawIFD,stream,*stage2.Get(),NULL,NULL);
    }
    else
#endif
    {
        dng_read_image reader;
        reader.Read(*host,*rawIFD,stream,*stage2.Get(),NULL,NULL);
    }

    if (stage2->Bounds().W() != S.raw_width ||
        stage2->Bounds().H() != S.raw_height)
    {
      return LIBRAW_DATA_ERROR;
    }

    int pplanes = stage2->Planes();
    int ptype = stage2->PixelType();

    dng_pixel_buffer buffer;
    stage2->GetPixelBuffer(buffer);

    int pixels = stage2->Bounds().H() * stage2->Bounds().W() * pplanes;
    bool zerocopy = false;

    if (ptype == ttShort && !is_curve_linear())
    {
      imgdata.rawdata.raw_alloc = malloc(pixels * TagTypeSize(ptype));
      ushort *src = (ushort *)buffer.fData;
      ushort *dst = (ushort *)imgdata.rawdata.raw_alloc;
      for (int i = 0; i < pixels; i++)
        dst[i] = imgdata.color.curve[src[i]];
      S.raw_pitch = S.raw_width * pplanes * TagTypeSize(ptype);
    }
    else if (ptype == ttByte)
    {
      imgdata.rawdata.raw_alloc = malloc(pixels * TagTypeSize(ttShort));
      unsigned char *src = (unsigned char *)buffer.fData;
      ushort *dst = (ushort *)imgdata.rawdata.raw_alloc;
      if (is_curve_linear())
      {
        for (int i = 0; i < pixels; i++)
          dst[i] = src[i];
      }
      else
      {
        for (int i = 0; i < pixels; i++)
          dst[i] = imgdata.color.curve[src[i]];
      }
      S.raw_pitch = S.raw_width * pplanes * TagTypeSize(ttShort);
    }
    else
    {
      // Alloc
      if (imgdata.params.raw_processing_options &
          LIBRAW_PROCESSING_DNGSDK_ZEROCOPY)
      {
        zerocopy = true;
      }
      else
      {
        imgdata.rawdata.raw_alloc = malloc(pixels * TagTypeSize(ptype));
        memmove(imgdata.rawdata.raw_alloc, buffer.fData,
                pixels * TagTypeSize(ptype));
      }
      S.raw_pitch = S.raw_width * pplanes * TagTypeSize(ptype);
    }

    if (zerocopy)
    {
      switch (ptype)
      {
      case ttFloat:
        if (pplanes == 1)
          imgdata.rawdata.float_image = (float *)buffer.fData;
        else if (pplanes == 3)
          imgdata.rawdata.float3_image = (float(*)[3])buffer.fData;
        else if (pplanes == 4)
          imgdata.rawdata.float4_image = (float(*)[4])buffer.fData;
        break;

      case ttShort:
        if (pplanes == 1)
          imgdata.rawdata.raw_image = (ushort *)buffer.fData;
        else if (pplanes == 3)
          imgdata.rawdata.color3_image = (ushort(*)[3])buffer.fData;
        else if (pplanes == 4)
          imgdata.rawdata.color4_image = (ushort(*)[4])buffer.fData;
        break;
      default:
        /* do nothing */
        break;
      }
    }
    else
    {
      switch (ptype)
      {
      case ttFloat:
        if (pplanes == 1)
          imgdata.rawdata.float_image = (float *)imgdata.rawdata.raw_alloc;
        else if (pplanes == 3)
          imgdata.rawdata.float3_image = (float(*)[3])imgdata.rawdata.raw_alloc;
        else if (pplanes == 4)
          imgdata.rawdata.float4_image = (float(*)[4])imgdata.rawdata.raw_alloc;
        break;

      case ttByte:
      case ttShort:
        if (pplanes == 1)
          imgdata.rawdata.raw_image = (ushort *)imgdata.rawdata.raw_alloc;
        else if (pplanes == 3)
          imgdata.rawdata.color3_image =
              (ushort(*)[3])imgdata.rawdata.raw_alloc;
        else if (pplanes == 4)
          imgdata.rawdata.color4_image =
              (ushort(*)[4])imgdata.rawdata.raw_alloc;
        break;
      default:
        /* do nothing */
        break;
      }
    }
    if (zerocopy)
    {
      dng_negative *stolen = negative.Release();
      dngnegative = stolen;
      dng_simple_image *simage = stage2.Release();
      dngimage = simage;
    }
  }
  catch (...)
  {
    return LIBRAW_UNSPECIFIED_ERROR;
  }
  return imgdata.rawdata.raw_alloc ? LIBRAW_SUCCESS : LIBRAW_UNSPECIFIED_ERROR;
#else
  return LIBRAW_UNSPECIFIED_ERROR;
#endif
}
void LibRaw::set_dng_host(void *p)
{
#ifdef USE_DNGSDK
  dnghost = p;
#endif
}
