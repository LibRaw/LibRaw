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

#include "../../internal/dcraw_defs.h"

void LibRaw::selectCRXTrack(short maxTrack)
{
  if (maxTrack < 0)
    return;
  INT64 bitcounts[LIBRAW_CRXTRACKS_MAXCOUNT], maxbitcount = 0;
  uint32_t maxjpegbytes = 0;
  memset(bitcounts, 0, sizeof(bitcounts));
  for (int i = 0; i <= maxTrack && i < LIBRAW_CRXTRACKS_MAXCOUNT; i++)
  {
    crx_data_header_t *d = &libraw_internal_data.unpacker_data.crx_header[i];
    if (d->MediaType == 1) // RAW
    {
      bitcounts[i] = INT64(d->nBits) * INT64(d->f_width) * INT64(d->f_height);
      if (bitcounts[i] > maxbitcount)
        maxbitcount = bitcounts[i];
    }
    else if (d->MediaType == 2) // JPEG
    {
      if (d->MediaSize > maxjpegbytes)
      {
        maxjpegbytes = d->MediaSize;
        thumb_offset = d->MediaOffset;
        thumb_length = d->MediaSize;
      }
    }
  }
  if (maxbitcount < 8)
    return;
  int framei = -1, framecnt = 0;
  for (int i = 0; i <= maxTrack && i < LIBRAW_CRXTRACKS_MAXCOUNT; i++)
  {
    if (bitcounts[i] == maxbitcount)
    {
      if (framecnt <= (int)shot_select)
        framei = i;
      framecnt++;
    }
  }
  is_raw = framecnt;
  if (framei >= 0 && framei < LIBRAW_CRXTRACKS_MAXCOUNT)
  {
    crx_data_header_t *d =
        &libraw_internal_data.unpacker_data.crx_header[framei];
    data_offset = d->MediaOffset;
    data_size = d->MediaSize;
    raw_width = d->f_width;
    raw_height = d->f_height;
    load_raw = &LibRaw::crxLoadRaw;
    switch (d->cfaLayout)
    {
    case 0:
      filters = 0x94949494;
      break;
    case 1:
      filters = 0x61616161;
      break;
    case 2:
      filters = 0x49494949;
      break;
    case 3:
      filters = 0x16161616;
      break;
    }

    libraw_internal_data.unpacker_data.crx_track_selected = framei;

    int tiff_idx = -1;
    INT64 tpixels = 0;
    for (unsigned i = 0; i < tiff_nifds && i < LIBRAW_IFD_MAXCOUNT; i++)
      if (INT64(tiff_ifd[i].t_height) * INT64(tiff_ifd[i].t_height) > tpixels)
      {
        tpixels = INT64(tiff_ifd[i].t_height) * INT64(tiff_ifd[i].t_height);
        tiff_idx = i;
      }
    if (tiff_idx >= 0)
      flip = tiff_ifd[tiff_idx].t_flip;
  }
}

#define bad_hdr                                                                \
  (((order != 0x4d4d) && (order != 0x4949)) || (get2() != 0x002a) ||           \
   (get4() != 0x00000008))
int LibRaw::parseCR3(unsigned long long oAtomList,
                     unsigned long long szAtomList, short &nesting,
                     char *AtomNameStack, short &nTrack, short &TrackType)
{
  /*
  Atom starts with 4 bytes for Atom size and 4 bytes containing Atom name
  Atom size includes the length of the header and the size of all "contained"
  Atoms if Atom size == 1, Atom has the extended size stored in 8 bytes located
  after the Atom name if Atom size == 0, it is the last top-level Atom extending
  to the end of the file Atom name is often a 4 symbol mnemonic, but can be a
  4-byte integer
  */
  const char UIID_Canon[17] =
      "\x85\xc0\xb6\x87\x82\x0f\x11\xe0\x81\x11\xf4\xce\x46\x2b\x6a\x48";

  /*
  AtomType = 0 - unknown: "unk."
  AtomType = 1 - container atom: "cont"
  AtomType = 2 - leaf atom: "leaf"
  AtomType = 3 - can be container, can be leaf: "both"
  */
  short AtomType;
  static const struct
  {
    char AtomName[5];
    short AtomType;
  } AtomNamesList[] = {
      {"dinf", 1},
      {"edts", 1},
      {"fiin", 1},
      {"ipro", 1},
      {"iprp", 1},
      {"mdia", 1},
      {"meco", 1},
      {"mere", 1},
      {"mfra", 1},
      {"minf", 1},
      {"moof", 1},
      {"moov", 1},
      {"mvex", 1},
      {"paen", 1},
      {"schi", 1},
      {"sinf", 1},
      {"skip", 1},
      {"stbl", 1},
      {"stsd", 1},
      {"strk", 1},
      {"tapt", 1},
      {"traf", 1},
      {"trak", 1},

      {"cdsc", 2},
      {"colr", 2},
      {"dimg", 2},
      // {"dref", 2},
      {"free", 2},
      {"frma", 2},
      {"ftyp", 2},
      {"hdlr", 2},
      {"hvcC", 2},
      {"iinf", 2},
      {"iloc", 2},
      {"infe", 2},
      {"ipco", 2},
      {"ipma", 2},
      {"iref", 2},
      {"irot", 2},
      {"ispe", 2},
      {"meta", 2},
      {"mvhd", 2},
      {"pitm", 2},
      {"pixi", 2},
      {"schm", 2},
      {"thmb", 2},
      {"tkhd", 2},
      {"url ", 2},
      {"urn ", 2},

      {"CCTP", 1},
      {"CRAW", 1},

      {"JPEG", 2},
      {"CDI1", 2},
      {"CMP1", 2},

      {"CNCV", 2},
      {"CCDT", 2},
      {"CTBO", 2},
      {"CMT1", 2},
      {"CMT2", 2},
      {"CMT3", 2},
      {"CMT4", 2},
      {"THMB", 2},
      {"co64", 2},
      {"mdat", 2},
      {"mdhd", 2},
      {"nmhd", 2},
      {"stsc", 2},
      {"stsz", 2},
      {"stts", 2},
      {"vmhd", 2},

      {"dref", 3},
      {"uuid", 3},
  };

  const char sHandlerType[5][5] = {"unk.", "soun", "vide", "hint", "meta"};

  int c, err;

  ushort tL;                        // Atom length represented in 4 or 8 bytes
  char nmAtom[5];                   // Atom name
  unsigned long long oAtom, szAtom; // Atom offset and Atom size
  unsigned long long oAtomContent,
      szAtomContent; // offset and size of Atom content
  unsigned long long lHdr;

  char UIID[16];
  uchar CMP1[36];
  char HandlerType[5], MediaFormatID[5];
  uint32_t relpos_inDir, relpos_inBox;
  unsigned szItem, Tag, lTag;
  ushort tItem;

  nmAtom[0] = MediaFormatID[0] = nmAtom[4] = MediaFormatID[4] = '\0';
  strcpy(HandlerType, sHandlerType[0]);
  oAtom = oAtomList;
  nesting++;
  if (nesting > 31)
    return -14; // too deep nesting
  short s_order = order;

  while ((oAtom + 8ULL) <= (oAtomList + szAtomList))
  {
    lHdr = 0ULL;
    err = 0;
    order = 0x4d4d;
    fseek(ifp, oAtom, SEEK_SET);
    szAtom = get4();
    FORC4 nmAtom[c] = AtomNameStack[nesting * 4 + c] = fgetc(ifp);
    AtomNameStack[(nesting + 1) * 4] = '\0';
    tL = 4;
    AtomType = 0;

    for (c = 0; c < int(sizeof AtomNamesList / sizeof *AtomNamesList); c++)
      if (!strcmp(nmAtom, AtomNamesList[c].AtomName))
      {
        AtomType = AtomNamesList[c].AtomType;
        break;
      }

    if (!AtomType)
    {
      err = 1;
    }

    if (szAtom == 0ULL)
    {
      if (nesting != 0)
      {
        err = -2;
        goto fin;
      }
      szAtom = szAtomList - oAtom;
      oAtomContent = oAtom + 8ULL;
      szAtomContent = szAtom - 8ULL;
    }
    else if (szAtom == 1ULL)
    {
      if ((oAtom + 16ULL) > (oAtomList + szAtomList))
      {
        err = -3;
        goto fin;
      }
      tL = 8;
      szAtom = (((unsigned long long)get4()) << 32) | get4();
      oAtomContent = oAtom + 16ULL;
      szAtomContent = szAtom - 16ULL;
    }
    else
    {
      oAtomContent = oAtom + 8ULL;
      szAtomContent = szAtom - 8ULL;
    }

    if (!strcmp(nmAtom, "trak"))
    {
      nTrack++;
      TrackType = 0;
      if (nTrack >= LIBRAW_CRXTRACKS_MAXCOUNT)
        break;
    }
    if (!strcmp(AtomNameStack, "moovuuid"))
    {
      lHdr = 16ULL;
      fread(UIID, 1, lHdr, ifp);
      if (!strncmp(UIID, UIID_Canon, lHdr))
      {
        AtomType = 1;
      }
      else
        fseek(ifp, -lHdr, SEEK_CUR);
    }
    else if (!strcmp(AtomNameStack, "moovuuidCCTP"))
    {
      lHdr = 12ULL;
    }
    else if (!strcmp(AtomNameStack, "moovuuidCMT1"))
    {
      short q_order = order;
      order = get2();
      if ((tL != 4) || bad_hdr)
      {
        err = -4;
        goto fin;
      }
      parse_tiff_ifd(oAtomContent);
      order = q_order;
    }
    else if (!strcmp(AtomNameStack, "moovuuidCMT2"))
    {
      short q_order = order;
      order = get2();
      if ((tL != 4) || bad_hdr)
      {
        err = -5;
        goto fin;
      }
      parse_exif(oAtomContent);
      order = q_order;
    }
    else if (!strcmp(AtomNameStack, "moovuuidCMT3"))
    {
      short q_order = order;
      order = get2();
      if ((tL != 4) || bad_hdr)
      {
        err = -6;
        goto fin;
      }
      fseek(ifp, -12L, SEEK_CUR);
      parse_makernote(oAtomContent, 0);
      order = q_order;
    }
    else if (!strcmp(AtomNameStack, "moovuuidCMT4"))
    {
      short q_order = order;
      order = get2();
      if ((tL != 4) || bad_hdr)
      {
        err = -6;
        goto fin;
      }
      INT64 off = ftell(ifp);
      parse_gps(oAtomContent);
      fseek(ifp, off, SEEK_SET);
      parse_gps_libraw(oAtomContent);
      order = q_order;
    }
    else if (!strcmp(AtomNameStack, "moovtrakmdiahdlr"))
    {
      fseek(ifp, 8L, SEEK_CUR);
      FORC4 HandlerType[c] = fgetc(ifp);
      for (c = 1; c < int(sizeof sHandlerType / sizeof *sHandlerType); c++)
        if (!strcmp(HandlerType, sHandlerType[c]))
        {
          TrackType = c;
          break;
        }
    }
    else if (!strcmp(AtomNameStack, "moovtrakmdiaminfstblstsd"))
    {
      if (szAtomContent >= 16)
      {
        fseek(ifp, 12L, SEEK_CUR);
        lHdr = 8;
      }
      else
      {
        err = -7;
        goto fin;
      }
      FORC4 MediaFormatID[c] = fgetc(ifp);
      if ((TrackType == 2) && (!strcmp(MediaFormatID, "CRAW")))
      {
        if (szAtomContent >= 44)
          fseek(ifp, 24L, SEEK_CUR);
        else
        {
          err = -8;
          goto fin;
        }
      }
      else
      {
        AtomType = 2; // only continue for CRAW
        lHdr = 0;
      }
#define current_track libraw_internal_data.unpacker_data.crx_header[nTrack]

      /*ImageWidth =*/ get2();
      /*ImageHeight =*/ get2();
    }
    else if (!strcmp(AtomNameStack, "moovtrakmdiaminfstblstsdCRAW"))
    {
      lHdr = 82;
    }
    else if (!strcmp(AtomNameStack, "moovtrakmdiaminfstblstsdCRAWCMP1"))
    {
      if (szAtomContent >= 40)
        fread(CMP1, 1, 36, ifp);
      else
      {
        err = -7;
        goto fin;
      }
      if (!crxParseImageHeader(CMP1, nTrack))
        current_track.MediaType = 1;
    }
    else if (!strcmp(AtomNameStack, "moovtrakmdiaminfstblstsdCRAWJPEG"))
    {
      current_track.MediaType = 2;
    }
    else if (!strcmp(AtomNameStack, "moovtrakmdiaminfstblstsz"))
    {
      if (szAtomContent == 12)
        fseek(ifp, 4L, SEEK_CUR);
      else if (szAtomContent == 16)
        fseek(ifp, 12L, SEEK_CUR);
      else
      {
        err = -9;
        goto fin;
      }
      current_track.MediaSize = get4();
    }
    else if (!strcmp(AtomNameStack, "moovtrakmdiaminfstblco64"))
    {
      if (szAtomContent == 16)
        fseek(ifp, 8L, SEEK_CUR);
      else
      {
        err = -10;
        goto fin;
      }
      current_track.MediaOffset = (((unsigned long long)get4()) << 32) | get4();
    }

    if (nTrack >= 0 && nTrack < LIBRAW_CRXTRACKS_MAXCOUNT &&
        current_track.MediaSize && current_track.MediaOffset &&
        ((oAtom + szAtom) >= (oAtomList + szAtomList)) &&
        !strncmp(AtomNameStack, "moovtrakmdiaminfstbl", 20))
    {
      if ((TrackType == 4) && (!strcmp(MediaFormatID, "CTMD")))
      {
        order = 0x4949;
        relpos_inDir = 0L;
        while (relpos_inDir + 6 < current_track.MediaSize)
        {
          if (current_track.MediaOffset + relpos_inDir > ifp->size() - 6) // need at least 6 bytes
          {
              err = -11;
              goto fin;
          }
          fseek(ifp, current_track.MediaOffset + relpos_inDir, SEEK_SET);
          szItem = get4();
          tItem = get2();
          if (szItem < 1 || (  (relpos_inDir + szItem) > current_track.MediaSize))
          {
            err = -11;
            goto fin;
          }
          if ((tItem == 7) || (tItem == 8) || (tItem == 9))
          {
            relpos_inBox = relpos_inDir + 12L;
            while (relpos_inBox + 8 < relpos_inDir + szItem)
            {
              if (current_track.MediaOffset + relpos_inBox > ifp->size() - 8) // need at least 8 bytes
              {
                  err = -11;
                  goto fin;
              }
              fseek(ifp, current_track.MediaOffset + relpos_inBox, SEEK_SET);
              lTag = get4();
              Tag = get4();
              if (lTag < 8)
              {
                err = -12;
                goto fin;
              }
              else if ((relpos_inBox + lTag) > (relpos_inDir + szItem))
              {
                err = -11;
                goto fin;
              }
              if ((Tag == 0x927c) && ((tItem == 7) || (tItem == 8)))
              {
                fseek(ifp, current_track.MediaOffset + relpos_inBox + 8L,
                      SEEK_SET);
                short q_order = order;
                order = get2();
                if (bad_hdr)
                {
                  err = -13;
                  goto fin;
                }
                fseek(ifp, -8L, SEEK_CUR);
                libraw_internal_data.unpacker_data.CR3_CTMDtag = 1;
                parse_makernote(current_track.MediaOffset + relpos_inBox + 8,
                                0);
                libraw_internal_data.unpacker_data.CR3_CTMDtag = 0;
                order = q_order;
              }
              relpos_inBox += lTag;
            }
          }
          relpos_inDir += szItem;
        }
        order = 0x4d4d;
      }
    }
#undef current_track
    if (AtomType == 1)
    {
      err = parseCR3(oAtomContent + lHdr, szAtomContent - lHdr, nesting,
                     AtomNameStack, nTrack, TrackType);
      if (err)
        goto fin;
    }
    oAtom += szAtom;
  }

fin:
  nesting--;
  if (nesting >= 0)
    AtomNameStack[nesting * 4] = '\0';
  order = s_order;
  return err;
}
#undef bad_hdr
