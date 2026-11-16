#!/usr/bin/env bash
# Downloads BASS + BASSmix for iOS (un4seen.com) into ThirdParty/bass/.
# BASS is proprietary: it is downloaded at build time and never committed or redistributed by this repo.
#
# Override the URLs with the BASS_IOS_URL / BASSMIX_IOS_URL environment variables (the GitHub workflow reads them
# from repository *variables*).  The defaults below are my best recollection of un4seen's naming scheme and have
# NOT been verified - check https://www.un4seen.com/ if the download fails.
set -euo pipefail
cd "$(dirname "$0")/.."

BASS_IOS_URL=${BASS_IOS_URL:-https://www.un4seen.com/files/bass24-ios.zip}
BASSMIX_IOS_URL=${BASSMIX_IOS_URL:-https://www.un4seen.com/files/bassmix24-ios.zip}

dest=ThirdParty/bass
if [ -f "$dest/bass.h" ] && [ -f "$dest/bassmix.h" ] && [ -f "$dest/ios/libbass.a" ] && [ -f "$dest/ios/libbassmix.a" ]; then
  echo "BASS already present in $dest"; exit 0
fi

tmp=$(mktemp -d)
mkdir -p "$dest/ios"

grab() { # url name
  echo "-> $1"
  curl -fL --retry 3 -A "Mozilla/5.0" -o "$tmp/$2.zip" "$1" || { echo "::error::could not download $1"; exit 1; }
  mkdir -p "$tmp/$2" && unzip -q "$tmp/$2.zip" -d "$tmp/$2"
}
pick() { # dir filename dest -> copies the first match, or fails loudly with a listing
  local f; f=$(find "$1" -name "$2" -not -path "*/__MACOSX/*" | head -n1)
  if [ -z "$f" ]; then echo "::error::$2 not found in the downloaded package; contents:"; find "$1" | head -50; exit 1; fi
  cp "$f" "$3"
}

grab "$BASS_IOS_URL" bass
grab "$BASSMIX_IOS_URL" bassmix
pick "$tmp/bass"    bass.h        "$dest/bass.h"
pick "$tmp/bassmix" bassmix.h     "$dest/bassmix.h"
pick "$tmp/bass"    libbass.a     "$dest/ios/libbass.a"
pick "$tmp/bassmix" libbassmix.a  "$dest/ios/libbassmix.a"
echo "BASS installed:"; ls -l "$dest" "$dest/ios"
