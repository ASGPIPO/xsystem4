#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out="${1:-/tmp/mainex-named-test}"
ain_file="${2:-/root/rance03/Rance03.ain}"
ex_file="${3:-/root/rance03/Rance03.ex}"
mkdir -p "$out"
cc -std=c11 -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections \
	-Iinclude -Isrc -Isubprojects/libsys4/include \
	test/mainex_named.c build/subprojects/libsys4/libsys4.a \
	$(pkg-config --libs libffi zlib) -lm -Wl,--gc-sections \
	-o "$out/mainex_named"
"$out/mainex_named" named "$ain_file" "$ex_file" 1808
"$out/mainex_named" legacy "$ain_file" "$ex_file" 1808
