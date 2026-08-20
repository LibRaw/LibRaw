#define LIBRAW_LIBRARY_BUILD
#include "../libraw/libraw.h"

#include <cstdio>
#include <vector>

class SonyMakernoteHarness : public LibRaw
{
public:
  using LibRaw::parseSonyMakernotes;
};

static int run_case(size_t len, ushort expected, bool expect_parse)
{
  std::vector<uchar> payload(len, 0);
  if (len >= 227)
    payload[226] = 0x12;
  if (len >= 228)
    payload[227] = 0x34;

  SonyMakernoteHarness raw;
  LibRaw_buffer_datastream *stream =
      new LibRaw_buffer_datastream(payload.data(), payload.size());
  if (!stream->valid())
  {
    std::fprintf(stderr, "LibRaw_buffer_datastream init failed for len=%zu\n", len);
    delete stream;
    return 1;
  }
  raw.get_internal_data_pointer()->internal_data.input = stream;
  raw.get_internal_data_pointer()->internal_data.input_internal = 1;

  raw.imgdata.shootinginfo.ImageStabilization = 0xdead;

  uchar *table_buf_0x0116 = nullptr, *table_buf_0x2010 = nullptr,
        *table_buf_0x9050 = nullptr, *table_buf_0x9400 = nullptr,
        *table_buf_0x9402 = nullptr, *table_buf_0x9403 = nullptr,
        *table_buf_0x9406 = nullptr, *table_buf_0x940c = nullptr,
        *table_buf_0x940e = nullptr;
  ushort table_buf_0x0116_len = 0, table_buf_0x2010_len = 0,
         table_buf_0x9050_len = 0, table_buf_0x9400_len = 0,
         table_buf_0x9402_len = 0, table_buf_0x9403_len = 0,
         table_buf_0x9406_len = 0, table_buf_0x940c_len = 0,
         table_buf_0x940e_len = 0;

  raw.parseSonyMakernotes(
      0, 0x0004, 0, len, 0, table_buf_0x0116, table_buf_0x0116_len,
      table_buf_0x2010, table_buf_0x2010_len, table_buf_0x9050,
      table_buf_0x9050_len, table_buf_0x9400, table_buf_0x9400_len,
      table_buf_0x9402, table_buf_0x9402_len, table_buf_0x9403,
      table_buf_0x9403_len, table_buf_0x9406, table_buf_0x9406_len,
      table_buf_0x940c, table_buf_0x940c_len, table_buf_0x940e,
      table_buf_0x940e_len);

  const ushort actual = ushort(raw.imgdata.shootinginfo.ImageStabilization);
  raw.recycle();

  if (expect_parse)
  {
    if (actual != expected)
    {
      std::fprintf(stderr,
                   "expected len=%zu to parse ImageStabilization=0x%04x, got 0x%04x\n",
                   len, expected, actual);
      return 1;
    }
  }
  else if (actual != 0xdead)
  {
    std::fprintf(stderr,
                 "expected len=%zu to be ignored and keep 0xdead, got 0x%04x\n",
                 len, actual);
    return 1;
  }

  return 0;
}

int main()
{
  if (run_case(227, 0, false))
    return 1;
  if (run_case(228, 0x1234, true))
    return 1;
  return 0;
}
