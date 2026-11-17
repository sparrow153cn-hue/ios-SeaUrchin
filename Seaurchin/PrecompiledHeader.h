#pragma once

// C++ only.  Xcode precompiles a prefix header once per language a target uses; without this guard a C file
// in the same target makes it parse libc++ as C (the first iOS CI failure).  MSVC defines __cplusplus too.
#ifdef __cplusplus

// Force-included into every translation unit of the core (MSVC: /Yu, Xcode: prefix header).
#include "../Platform/SeaPlatform.h"

//Windows
#if defined(SEA_PLATFORM_WINDOWS)
#include <Windows.h>
#include <Shlwapi.h>
#include <Imm.h>
#endif

//C Runtime
#define _USE_MATH_DEFINES
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <cstring>
#include <cmath>
#include <climits>

//C++ Standard
#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <memory>
#include <algorithm>
#include <functional>
#include <chrono>
#include <ios>
#include <map>
#include <set>
#include <bitset>
#include <array>
#include <deque>
#include <numeric>
#include <utility>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <forward_list>
#include <list>
#include <tuple>
#include <random>
#include <exception>
#include <future>
#include <thread>
#include <mutex>
#include <type_traits>

//Boost
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/regex.hpp>
#include <boost/xpressive/xpressive.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/clamp.hpp>
#include <boost/crc.hpp>
#include <boost/any.hpp>
#include <boost/fusion/include/std_tuple.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/optional.hpp>

//Libraries
#if defined(SEA_PLATFORM_WINDOWS)
#include <DxLib.h>
#else
#include "../Platform/DxCompat/DxLib.h"
#endif
//#include <EffekseerForDXLib.h>

#include <angelscript.h>
#include <scriptarray/scriptarray.h>
#include <scriptmath/scriptmath.h>
#include <scriptmath/scriptmathcomplex.h>
#include <scriptstdstring/scriptstdstring.h>
#include <scriptdictionary/scriptdictionary.h>
#include "wscriptbuilder.h"
//#include "as_smart_ptr_wrapper.h"

#if defined(SEA_PLATFORM_WINDOWS)
#include <WacomMultiTouch.h>
#endif

#include <ft2build.h>
#include FT_FREETYPE_H

#include <zlib.h>
#include <png.h>

#include <bass.h>
#include <bassmix.h>
#if defined(SEA_PLATFORM_WINDOWS)
#include <bass_fx.h>        // included upstream but no BASS_FX function is called anywhere; not needed on iOS
#endif

#include <msgpack.hpp>

#endif // __cplusplus
