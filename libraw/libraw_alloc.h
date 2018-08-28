/* -*- C++ -*-
 * File: libraw_alloc.h
 * Copyright 2008-2018 LibRaw LLC (info@libraw.org)
 * Created: Sat Mar  22, 2008
 *
 * LibRaw C++ interface
 *
LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#ifndef __LIBRAW_ALLOC_H
#define __LIBRAW_ALLOC_H

#include <stdlib.h>
#include <string.h>
#include "libraw_const.h"

#ifdef __cplusplus

#define LIBRAW_MSIZE 512

class DllDef libraw_memmgr
{
public:
  libraw_memmgr(unsigned ee) : extra_bytes(ee)
  {
    size_t alloc_sz = LIBRAW_MSIZE * sizeof(void *);
    mems = (void **)::malloc(alloc_sz);
    memset(mems, 0, alloc_sz);
  }
  ~libraw_memmgr()
  {
    cleanup();
    ::free(mems);
  }
  void *malloc(size_t sz)
  {
#ifdef LIBRAW_USE_CALLOC_INSTEAD_OF_MALLOC
    void *ptr = ::calloc(sz + extra_bytes,1);
#else
    void *ptr = ::malloc(sz + extra_bytes);
#endif
    mem_ptr(ptr);
    return ptr;
  }
  void *calloc(size_t n, size_t sz)
  {
    void *ptr = ::calloc(n + (extra_bytes + sz - 1) / (sz ? sz : 1), sz);
    mem_ptr(ptr);
    return ptr;
  }
  void *realloc(void *ptr, size_t newsz)
  {
    void *ret = ::realloc(ptr, newsz + extra_bytes);
    forget_ptr(ptr);
    mem_ptr(ret);
    return ret;
  }
  void free(void *ptr)
  {
    forget_ptr(ptr);
    ::free(ptr);
  }
  void cleanup(void)
  {
    for (int i = 0; i < LIBRAW_MSIZE; i++)
      if (mems[i])
      {
        ::free(mems[i]);
        mems[i] = NULL;
      }
  }

private:
  void **mems;
  unsigned extra_bytes;
  void mem_ptr(void *ptr)
  {
    if (ptr)
    {
      for (int i = 0; i < LIBRAW_MSIZE; i++)
        if (!mems[i])
        {
          mems[i] = ptr;
          return;
        }
#ifdef LIBRAW_MEMPOOL_CHECK
      throw LIBRAW_EXCEPTION_MEMPOOL;
#endif
    }
  }
  void forget_ptr(void *ptr)
  {
    if (ptr)
      for (int i = 0; i < LIBRAW_MSIZE; i++)
        if (mems[i] == ptr)
        {
          mems[i] = NULL;
          break;
        }
  }
};

#endif /* C++ */

#endif
