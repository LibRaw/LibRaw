#define LIBRAW_LIBRARY_BUILD
#include "../libraw/libraw.h"

#include <cstdio>

class NewSubfileTypeHarness : public LibRaw
{
public:
  unsigned first_newsubfiletype() const { return tiff_ifd[0].newsubfiletype; }
  unsigned ifd_count()
  {
    return get_internal_data_pointer()->identify_data.tiff_nifds;
  }
};

static const unsigned char kNewSubfileTypePoc[] = {
    0x49, 0x49, 0x2a, 0x00, 0x08, 0x00, 0x00, 0x00,
    0x01, 0x00, 0xfe, 0x00, 0x04, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x00, 0xe3, 0x07, 0x1b,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

int main()
{
  NewSubfileTypeHarness raw;
  const int rc = raw.open_buffer(kNewSubfileTypePoc, sizeof(kNewSubfileTypePoc));
  if (rc != LIBRAW_SUCCESS && rc != LIBRAW_FILE_UNSUPPORTED)
  {
    std::fprintf(stderr, "open_buffer failed unexpectedly: %s (%d)\n",
                 libraw_strerror(rc), rc);
    return 1;
  }

  if (raw.ifd_count() == 0)
  {
    std::fprintf(stderr, "expected at least one TIFF IFD\n");
    raw.recycle();
    return 1;
  }

  if (raw.first_newsubfiletype() != 0xe3000001u)
  {
    std::fprintf(stderr,
                 "expected newsubfiletype 0xe3000001, got 0x%08x\n",
                 raw.first_newsubfiletype());
    raw.recycle();
    return 1;
  }

  raw.recycle();
  return 0;
}
