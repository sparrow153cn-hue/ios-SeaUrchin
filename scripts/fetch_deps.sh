#!/usr/bin/env bash
# Downloads the third-party sources the iOS build compiles into ThirdParty/.
#
# UNTESTED: written without network access.  Versions are variables - the engine dates from 2017-2018,
# so if a newer library breaks the build, try an older release first (see docs/PORTING.md).
# BASS is proprietary and is NOT downloaded here (license terms): see the message at the end.
set -euo pipefail
cd "$(dirname "$0")/.."
T=ThirdParty
mkdir -p "$T" && cd "$T"

BOOST_VERSION=${BOOST_VERSION:-1.83.0}
ANGELSCRIPT_VERSION=${ANGELSCRIPT_VERSION:-2.36.1}
FREETYPE_VERSION=${FREETYPE_VERSION:-2.13.2}
LIBPNG_VERSION=${LIBPNG_VERSION:-1.6.43}
MSGPACK_VERSION=${MSGPACK_VERSION:-6.1.0}

fetch() { # url outfile
  [ -f "$2" ] || { echo "-> $1"; curl -fL --retry 3 -o "$2" "$1"; }
}

# --- Boost (headers + Filesystem sources; everything else the engine uses is header-only) ---
if [ ! -d boost ]; then
  v=${BOOST_VERSION//./_}
  fetch "https://archives.boost.io/release/${BOOST_VERSION}/source/boost_${v}.tar.bz2" boost.tar.bz2
  mkdir boost && tar -xjf boost.tar.bz2 -C boost --strip-components=1
fi

# --- AngelScript ---
if [ ! -d angelscript ]; then
  fetch "https://www.angelcode.com/angelscript/sdk/files/angelscript_${ANGELSCRIPT_VERSION}.zip" angelscript.zip
  mkdir angelscript && unzip -q angelscript.zip -d angelscript
fi

# --- msgpack-c (header only) ---
if [ ! -d msgpack ]; then
  fetch "https://github.com/msgpack/msgpack-c/releases/download/cpp-${MSGPACK_VERSION}/msgpack-cxx-${MSGPACK_VERSION}.tar.gz" msgpack.tar.gz
  mkdir msgpack && tar -xzf msgpack.tar.gz -C msgpack --strip-components=1
fi

# --- FreeType: compiled as one amalgamated translation unit (the officially documented way to embed it) ---
if [ ! -d freetype ]; then
  fetch "https://download.savannah.gnu.org/releases/freetype/freetype-${FREETYPE_VERSION}.tar.xz" freetype.tar.xz
  mkdir freetype && tar -xJf freetype.tar.xz -C freetype --strip-components=1
fi
mkdir -p freetype-build
# Only the TrueType/OpenType path is needed (the engine bakes .ttf/.otf fonts); a custom module list keeps the
# amalgamation self-consistent (FreeType's default ftmodule.h also references autofit, pcf, bdf, pfr, ...).
cat > freetype-build/ftmodule_sea.h <<'EOC'
FT_USE_MODULE( tt_driver )
FT_USE_MODULE( cff_driver )
FT_USE_MODULE( psaux_module )
FT_USE_MODULE( psnames_module )
FT_USE_MODULE( pshinter_module )
FT_USE_MODULE( ft_raster1_renderer_class )
FT_USE_MODULE( sfnt_module )
FT_USE_MODULE( ft_smooth_renderer_class )
EOC
cat > freetype-build/ft_amalgam.c <<'EOC'
#define FT_CONFIG_MODULES_H "ftmodule_sea.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include "../freetype/src/base/ftsystem.c"
#include "../freetype/src/base/ftinit.c"
#include "../freetype/src/base/ftdebug.c"
#include "../freetype/src/base/ftbase.c"
#include "../freetype/src/base/ftbbox.c"
#include "../freetype/src/base/ftbitmap.c"
#include "../freetype/src/base/ftglyph.c"
#include "../freetype/src/truetype/truetype.c"
#include "../freetype/src/sfnt/sfnt.c"
#include "../freetype/src/psnames/psnames.c"
#include "../freetype/src/smooth/smooth.c"
#include "../freetype/src/raster/raster.c"
#include "../freetype/src/cff/cff.c"
#include "../freetype/src/psaux/psaux.c"
#include "../freetype/src/pshinter/pshinter.c"
EOC

# --- libpng (zlib comes from the iOS SDK: -lz) ---
if [ ! -d libpng ]; then
  fetch "https://download.sourceforge.net/libpng/libpng-${LIBPNG_VERSION}.tar.xz" libpng.tar.xz
  mkdir libpng && tar -xJf libpng.tar.xz -C libpng --strip-components=1
  cp libpng/scripts/pnglibconf.h.prebuilt libpng/pnglibconf.h
fi

cat <<'MSG'

Done.  One manual step remains - BASS (proprietary, https://www.un4seen.com):
  download the *iOS* packages of BASS and BASSmix and put them here:
      ThirdParty/bass/bass.h  bassmix.h
      ThirdParty/bass/ios/libbass.a  libbassmix.a
  BASS is free for non-commercial use; a commercial App Store release needs a license.
Then:  xcodegen generate && open Seaurchin.xcodeproj
MSG
