#!/usr/bin/env bash
# Installs BASS + BASSmix for iOS (un4seen.com) into ThirdParty/bass/ as .xcframeworks.
# BASS is proprietary: it is downloaded at build time and never committed or redistributed by this repo.
#
# The default URLs below worked in a real CI run.  Override with BASS_IOS_URL / BASSMIX_IOS_URL
# (the GitHub workflow reads them from repository *variables*).
#
# Needs macOS tools (lipo, file; xcodebuild only for the legacy libbass*.a packaging).
set -euo pipefail
cd "$(dirname "$0")/.."

BASS_IOS_URL=${BASS_IOS_URL:-https://www.un4seen.com/files/bass24-ios.zip}
BASSMIX_IOS_URL=${BASSMIX_IOS_URL:-https://www.un4seen.com/files/bassmix24-ios.zip}
dest=ThirdParty/bass
tmp=$(mktemp -d)
mkdir -p "$dest"

fail() { echo "::error::$1"; [ -n "${2:-}" ] && { echo "package contents:"; find "$2" -not -path '*/__MACOSX/*' | head -60; }; exit 1; }

install_pkg() { # name url      (name = bass | bassmix)
  local name=$1 url=$2 h xc lib
  echo "-> $url"
  curl -fL --retry 3 -A "Mozilla/5.0" -o "$tmp/$name.zip" "$url" || fail "could not download $url"
  mkdir -p "$tmp/$name" && unzip -q "$tmp/$name.zip" -d "$tmp/$name"

  h=$(find "$tmp/$name" -name "$name.h" -not -path '*/__MACOSX/*' | head -n1)
  [ -n "$h" ] || fail "$name.h not found in the downloaded package" "$tmp/$name"
  cp "$h" "$dest/$name.h"

  rm -rf "$dest/$name.xcframework"
  xc=$(find "$tmp/$name" -maxdepth 3 -name '*.xcframework' -not -path '*/__MACOSX/*' | head -n1)
  if [ -n "$xc" ]; then
    cp -R "$xc" "$dest/$name.xcframework"            # the folder name is arbitrary to Xcode; normalise it
  else
    # older packaging: a loose static library -> wrap it into an xcframework (device slice only)
    lib=$(find "$tmp/$name" -name "lib$name.a" -not -path '*/__MACOSX/*' | head -n1)
    [ -n "$lib" ] || fail "neither a .xcframework nor lib$name.a found" "$tmp/$name"
    mkdir -p "$tmp/$name-hdr" && cp "$h" "$tmp/$name-hdr/"
    xcodebuild -create-xcframework -library "$lib" -headers "$tmp/$name-hdr" -output "$dest/$name.xcframework"
  fi
}

# true if the device slice of the xcframework is a static archive (links in; nothing to embed)
is_static() { # xcframework-dir name
  local bin thin kind
  bin=$(find "$1" -path '*ios-arm64*' ! -path '*simulator*' -type f \( -name "$2" -o -name "lib$2.a" \) | head -n1)
  [ -n "$bin" ] || { echo "   $2: device slice binary not found, assuming dynamic" >&2; return 1; }
  thin=$(mktemp)
  lipo "$bin" -thin arm64 -output "$thin" 2>/dev/null || cp "$bin" "$thin"   # fat -> arm64; thin files are used as they are
  kind=$(file -b "$thin")
  echo "   $2 device slice ($bin): $kind" >&2
  [[ "$kind" == *"ar archive"* ]]
}

install_pkg bass    "$BASS_IOS_URL"
install_pkg bassmix "$BASSMIX_IOS_URL"

for n in bass bassmix; do
  if is_static "$dest/$n.xcframework" "$n"; then embed=false; else embed=true; fi
  python3 - "$n" "$embed" <<'PY'
import re, sys
name, embed = sys.argv[1:3]
text = open('project.yml', encoding='utf-8').read()          # text mode: CRLF is normalised to LF
dep = re.compile(r'(- framework: ThirdParty/bass/' + name + r'\.xcframework\n\s+embed: )(true|false)')

if not dep.search(text):
    # project.yml predates the xcframework switch (it linked loose libbass.a files): migrate it.
    anchor = '      - sdk: libz.tbd\n'
    if anchor not in text:
        sys.exit("project.yml: cannot find the Seaurchin target's dependency list ('- sdk: libz.tbd'); "
                 "copy the current project.yml from the repository/zip")
    text = text.replace(anchor, anchor + '      - framework: ThirdParty/bass/%s.xcframework\n        embed: true\n' % name, 1)
    print("   project.yml: added the %s.xcframework dependency (old layout migrated)" % name)

text, n = dep.subn(r'\g<1>' + embed, text)
if n != 1:
    sys.exit("project.yml: expected exactly one %s.xcframework dependency, found %d" % (name, n))

# leftovers of the old loose-library layout would break the link step
text = text.replace('        LIBRARY_SEARCH_PATHS:\n          - $(SRCROOT)/ThirdParty/bass/ios\n', '')
text = text.replace('OTHER_LDFLAGS: [-lbass, -lbassmix, -lc++]', 'OTHER_LDFLAGS: [-lc++]')
open('project.yml', 'w', encoding='utf-8').write(text)
PY
  echo "   $n: embed=$embed"
done

echo "BASS installed:"; ls -l "$dest"
