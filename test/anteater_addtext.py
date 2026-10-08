#!/usr/bin/env python3
"""Link the regression against the workspace's real engine objects."""
import json
from pathlib import Path
import shlex
import subprocess
root = Path(__file__).resolve().parent.parent
build = root / "build"
commands = json.loads((build / "compile_commands.json").read_text())
entry = next(x for x in commands if x["file"] == "../src/hll/AnteaterADVEngine.c")
compile = shlex.split(entry["command"])
for flag in ("-MF", "-MQ"):
    i = compile.index(flag)
    del compile[i:i+2]
compile.remove("-MD")
compile[compile.index("-o")+1] = "anteater_test.o"
compile[-1] = "../test/anteater_addtext.c"
subprocess.run(compile, cwd=build, check=True)
subprocess.run(["objcopy", "--redefine-sym", "main=engine_main",
                "src/xsystem4.p/system4.c.o", "anteater_system4.o"],
               cwd=build, check=True)
line = subprocess.check_output(["ninja", "-C", "build", "-t", "commands",
                                "src/xsystem4"], cwd=root, text=True).splitlines()[-1]
link = shlex.split(line)
link[link.index("-o")+1] = "anteater_addtext_test"
link = ["anteater_test.o" if x == "src/xsystem4.p/hll_AnteaterADVEngine.c.o"
        else "anteater_system4.o" if x == "src/xsystem4.p/system4.c.o"
        else x for x in link]
subprocess.run(link, cwd=build, check=True)
subprocess.run([str(build / "anteater_addtext_test")], check=True)
