
project(
  'libraw',
  [ 'c', 'cpp' ],
  version: run_command('version.sh', check: true).stdout().strip(),
  license: 'LGPL>=2.1',
  meson_version: '>=0.60',
)

# host

host_os = host_machine.system()
win32 = ['windows']
sys_windows = win32.contains(host_os)

# binary

cpp = meson.get_compiler('cpp')

# dependencies

deps = []
cargs = []

if sys_windows
  deps += cpp.find_library('ws2_32')
endif

thread_dep = dependency('threads')

if get_option('openmp').enabled()
  dep = dependency('openmp')
  if dep.found()
    deps += dep
   endif
endif

if get_option('jpeg').enabled()
  dep = dependency('libjpeg', required: false)
  if dep.found()
    cargs += [ '-DUSE_JPEG', '-DUSE_JPEG8' ]
    deps += dep
   endif
endif

if get_option('jasper').enabled()
  dep = dependency('jasper', required: false)
  if dep.found()
    cargs += [ '-DUSE_JASPER' ]
    deps += dep
   endif
endif

if get_option('zlib').enabled()
  dep = dependency('zlib', required: false)
  if dep.found()
    cargs += [ '-DUSE_ZLIB' ]
    deps += dep
   endif
endif

if get_option('lcms').enabled()
  dep = dependency('lcms2', required: false)
  if dep.found()
    cargs += [ '-DUSE_LCMS2' ]
    deps += dep
   endif
endif

# sources

src = files(
  'src/decoders/canon_600.cpp',
  'src/decoders/crx.cpp',
  'src/decoders/decoders_dcraw.cpp',
  'src/decoders/decoders_libraw.cpp',
  'src/decoders/decoders_libraw_dcrdefs.cpp',
  'src/decoders/dng.cpp',
  'src/decoders/fp_dng.cpp',
  'src/decoders/fuji_compressed.cpp',
  'src/decoders/generic.cpp',
  'src/decoders/kodak_decoders.cpp',
  'src/decoders/load_mfbacks.cpp',
  'src/decoders/smal.cpp',
  'src/decoders/unpack.cpp',
  'src/decoders/unpack_thumb.cpp',
  'src/demosaic/aahd_demosaic.cpp',
  'src/demosaic/ahd_demosaic.cpp',
  'src/demosaic/dcb_demosaic.cpp',
  'src/demosaic/dht_demosaic.cpp',
  'src/demosaic/misc_demosaic.cpp',
  'src/demosaic/xtrans_demosaic.cpp',
  'src/integration/dngsdk_glue.cpp',
  'src/integration/rawspeed_glue.cpp',
  'src/libraw_c_api.cpp',
  'src/libraw_datastream.cpp',
  'src/metadata/adobepano.cpp',
  'src/metadata/canon.cpp',
  'src/metadata/ciff.cpp',
  'src/metadata/cr3_parser.cpp',
  'src/metadata/epson.cpp',
  'src/metadata/exif_gps.cpp',
  'src/metadata/fuji.cpp',
  'src/metadata/hasselblad_model.cpp',
  'src/metadata/identify.cpp',
  'src/metadata/identify_tools.cpp',
  'src/metadata/kodak.cpp',
  'src/metadata/leica.cpp',
  'src/metadata/makernotes.cpp',
  'src/metadata/mediumformat.cpp',
  'src/metadata/minolta.cpp',
  'src/metadata/misc_parsers.cpp',
  'src/metadata/nikon.cpp',
  'src/metadata/normalize_model.cpp',
  'src/metadata/olympus.cpp',
  'src/metadata/p1.cpp',
  'src/metadata/pentax.cpp',
  'src/metadata/samsung.cpp',
  'src/metadata/sony.cpp',
  'src/metadata/tiff.cpp',
  'src/postprocessing/aspect_ratio.cpp',
  'src/postprocessing/dcraw_process.cpp',
  'src/postprocessing/mem_image.cpp',
  'src/postprocessing/postprocessing_aux.cpp',
  'src/postprocessing/postprocessing_utils.cpp',
  'src/postprocessing/postprocessing_utils_dcrdefs.cpp',
  'src/preprocessing/ext_preprocess.cpp',
  'src/preprocessing/raw2image.cpp',
  'src/preprocessing/subtract_black.cpp',
  'src/tables/cameralist.cpp',
  'src/tables/colorconst.cpp',
  'src/tables/colordata.cpp',
  'src/tables/wblists.cpp',
  'src/utils/curves.cpp',
  'src/utils/decoder_info.cpp',
  'src/utils/init_close_utils.cpp',
  'src/utils/open.cpp',
  'src/utils/phaseone_processing.cpp',
  'src/utils/read_utils.cpp',
  'src/utils/thumb_utils.cpp',
  'src/utils/utils_dcraw.cpp',
  'src/utils/utils_libraw.cpp',
  'src/write/apply_profile.cpp',
  'src/write/file_write.cpp',
  'src/write/tiff_writer.cpp',
  'src/x3f/x3f_parse_process.cpp',
  'src/x3f/x3f_utils_patched.cpp',
)

headers = files(
  'libraw/libraw.h',
  'libraw/libraw_alloc.h',
  'libraw/libraw_const.h',
  'libraw/libraw_datastream.h',
  'libraw/libraw_internal.h',
  'libraw/libraw_types.h',
  'libraw/libraw_version.h'
)

libraw_lib = library(
  'libraw',
  src,
  c_args: [ cargs, '-DLIBRAW_NOTHREADS' ],
  dependencies: deps,
  gnu_symbol_visibility: 'hidden',
  include_directories: include_directories('.'),
  install: true,
  pic: true,
  version: meson.project_version()
)

libraw_dep = declare_dependency(
  include_directories : [ include_directories('.')],
  link_with : libraw_lib,
  dependencies : deps
)

if thread_dep.found()
  libraw_r_lib = library(
    'libraw_r',
    src,
    c_args: cargs,
    dependencies: [ deps, thread_dep ],
    gnu_symbol_visibility: 'hidden',
    include_directories: include_directories('.'),
    install: true,
    pic: true,
    version: meson.project_version()
  )

  libraw_r_dep = declare_dependency(
    include_directories : [ include_directories('.')],
    link_with : libraw_r_lib,
    dependencies : deps
  )
endif

install_headers(
  headers,
  install_dir : get_option('prefix') / get_option('includedir') / 'libraw'
)

# examples

if get_option('examples')
  binaries = [
    [ 'raw-identify',             files(['samples/raw-identify.cpp']) ],
    [ 'unprocessed_raw',          files(['samples/unprocessed_raw.cpp']) ],
    [ '4channels',                files(['samples/4channels.cpp']) ],
    [ 'rawtextdump',              files(['samples/rawtextdump.cpp']) ],
    [ 'simple_dcraw',             files(['samples/simple_dcraw.cpp']) ],
    [ 'mem_image',                files(['samples/mem_image_sample.cpp']) ],
    [ 'dcraw_half',               files(['samples/dcraw_half.c']) ],
    [ 'half_mt',                  files(['samples/half_mt.c']) ],
    [ 'multirender_test',         files(['samples/multirender_test.cpp']) ],
    [ 'postprocessing_benchmark', files(['samples/postprocessing_benchmark.cpp']) ],
    [ 'dcraw_emu',                files(['samples/dcraw_emu.cpp'])]
  ]

  foreach bin: binaries
    if thread_dep.found() and bin[0] == 'half_mt'
      bin_dep = libraw_r_dep
    else
      bin_dep = libraw_dep
    endif
    cargs_dep = cargs
    e = executable(
      bin[0],
      bin[1],
      c_args: cargs_dep,
      dependencies: bin_dep,
      include_directories : [ include_directories('.')],
      install: true
    )
  endforeach
endif

pkg = import('pkgconfig')

pkg.generate(
  libraw_lib,
  description: 'Raw image decoder library (non-thread-safe)'
)

pkg.generate(
  libraw_r_lib,
  description: 'Raw image decoder library (thread-safe)'
)
