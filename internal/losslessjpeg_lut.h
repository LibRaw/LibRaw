/* -*- C++ -*-
 * File: losslessjpeg_lut.h
 * Copyright 2026 LibRaw LLC (info@libraw.org)
 *
   Table-driven lossless-JPEG entropy decoding from a memory buffer,
   shared by the tile-parallel Sony ARW decoders.

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */
#pragma once
#include <stdint.h>
#include <vector>

// Bit reader over one entropy-coded segment: FF 00 -> FF; FF xx or the end
// of the buffer ends the data, zero bits are fed from then on.
struct LibRaw_ljpeg_bits
{
  const uint8_t *p, *end;
  uint64_t buf;
  int bits; // bits in buf, including padding
  int pad;  // zero bits appended after the end of data
  bool stop;
  LibRaw_ljpeg_bits(const uint8_t *b, const uint8_t *e) : p(b), end(e), buf(0), bits(0), pad(0), stop(false) {}
  void fill()
  {
    while (bits <= 56 && p < end && *p != 0xff)
    {
      buf = (buf << 8) | *p++;
      bits += 8;
    }
    while (bits <= 56)
    {
      unsigned c = 0;
      if (!stop)
      {
        if (p < end && *p != 0xff)
          c = *p++;
        else if (p + 1 < end && p[1] == 0)
        {
          c = 0xff;
          p += 2;
        }
        else
          stop = true;
      }
      if (stop)
        pad += 8;
      buf = (buf << 8) | c;
      bits += 8;
    }
  }
  // true if bits past the end of data were consumed
  bool overrun() const { return bits < pad; }
};

/*
   Huffman decoding of lossless-JPEG differences with two lookup tables:
   FIRST_BITS (64 KB, stays in cache) and 16 bits (any code).
   Table entry:
     bits 0..7   bits to consume (code length, plus difference length if
                 complete); 0 = no code here (first table: look in the
                 16-bit table; 16-bit table: invalid code)
     bits 8..15  difference bits still to read (0 = difference complete)
     bits 16..31 complete difference value (int16)
*/
struct LibRaw_ljpeg_lut
{
  enum
  {
    FIRST_BITS = 14
  };
  std::vector<uint32_t> first, full;

  // code(prefix, nbits) returns (code length << 8) | difference length of
  // the code that the nbits-bit prefix starts with; a code length of 0 or
  // above nbits means there is none.
  template <class Code> void build(Code code)
  {
    fill(first, FIRST_BITS, code);
    fill(full, 16, code);
  }

  // Sets err and returns 0 on an invalid code or difference length > 16.
  int32_t decode(LibRaw_ljpeg_bits &b, int &err) const
  {
    if (b.bits < 32)
      b.fill();
    uint32_t e = first[(b.buf >> (b.bits - FIRST_BITS)) & ((1u << FIRST_BITS) - 1)];
    if (!e)
    {
      e = full[(b.buf >> (b.bits - 16)) & 0xffff];
      if (!e)
      {
        err = 1;
        return 0;
      }
    }
    b.bits -= e & 0xff;
    int len = (e >> 8) & 0xff;
    if (len == 0)
      return int16_t(e >> 16);
    int32_t diff = int32_t((b.buf >> (b.bits - len)) & ((1u << len) - 1));
    b.bits -= len;
    if ((diff & (1 << (len - 1))) == 0)
      diff -= (1 << len) - 1;
    return diff;
  }

private:
  template <class Code> static void fill(std::vector<uint32_t> &t, int nb, Code code)
  {
    t.resize(size_t(1) << nb);
    for (uint32_t idx = 0; idx < t.size(); idx++)
    {
      uint32_t c = code(idx, nb);
      int l = c >> 8, len = c & 0xff;
      uint32_t e;
      if (!l || l > nb || len > 16)
        e = 0;
      else if (len == 0)
        e = l;
      else if (len == 16)
        e = (uint32_t(uint16_t(-32768)) << 16) | l;
      else if (l + len <= nb)
      {
        int32_t diff = int32_t((idx >> (nb - l - len)) & ((1u << len) - 1));
        if ((diff & (1 << (len - 1))) == 0)
          diff -= (1 << len) - 1;
        e = (uint32_t(uint16_t(diff)) << 16) | (l + len);
      }
      else
        e = (len << 8) | l;
      t[idx] = e;
    }
  }
};
