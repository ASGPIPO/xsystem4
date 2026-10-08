#!/usr/bin/env python3
"""Compile the real Chipmunk handlers with only their external IO mocked."""
import os
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
out = Path(sys.argv[1] if len(sys.argv) > 1 else "/tmp/hll-startup-runtime")
out.mkdir(parents=True, exist_ok=True)
source = (root / "src/hll/ChipmunkSpriteEngine.c").read_text()
names = ("Update", "SYSTEM_SetConfigFrameSkipAllWhileMessageSkip")
exports = []
for name in names:
    match = re.search(r"HLL_EXPORT\(" + name + r",\s*(\w+)\)", source)
    assert match, "Missing ChipmunkSpriteEngine export: " + name
    exports.append("HLL_EXPORT(" + name + ", " + match[1] + ")")
start = source.index("HLL_LIBRARY(ChipmunkSpriteEngine,")
end = source.index("\nstatic struct ain_hll_function *get_fun", start)
source = source[:start] + "HLL_LIBRARY(ChipmunkSpriteEngine,\n" + ",\n".join(exports) + ");\n" + source[end:]
source += '\n#include "' + str(root / "test/chipmunk_frame_skip.c") + '"\n'
unit = out / "chipmunk_frame_skip_unit.c"
unit.write_text(source)
cglm = os.environ.get("CGLM_TEST_INCLUDE", "/root/xsystem4/subprojects/cglm/include")
subprocess.run(["cc", "-std=c11", "-D_DEFAULT_SOURCE", "-DUSE_GLES",
                "-ffunction-sections", "-fdata-sections",
                "-I" + str(root / "include"), "-I" + str(root / "src"),
                "-I" + str(root / "src/hll"),
                "-I" + str(root / "subprojects/libsys4/include"),
                "-I" + cglm, "-I/usr/include/SDL2",
                str(unit), "-Wl,--gc-sections",
                "-o", str(out / "chipmunk_frame_skip")], check=True)
subprocess.run([str(out / "chipmunk_frame_skip")], check=True)
