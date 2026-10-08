#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out="${1:-/tmp/hll-startup-runtime}"
mkdir -p "$out"
cc -std=c11 -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections \
	-Iinclude -Isrc -Isubprojects/libsys4/include \
	test/http_downloader.c subprojects/libsys4/src/string.c \
	subprojects/libsys4/src/utfsjis.c subprojects/libsys4/src/system.c \
	-Wl,--gc-sections -o "$out/http_downloader"
for group in offline encode decode invalid_url charset invalid_charset; do
	"$out/http_downloader" "$group"
done
python3 test/chipmunk_frame_skip.py "$out"
