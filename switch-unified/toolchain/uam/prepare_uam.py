import ast, hashlib, importlib.util, os, shutil, subprocess, sys
from pathlib import Path
from typing import Optional
ROOT = Path(__file__).resolve().parent
DONOR = ROOT.parent / "donor"
spec = importlib.util.spec_from_file_location("ninja_syntax", DONOR / "tools/ninja_syntax.py")
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
Writer = mod.Writer
BUILD = ROOT / "build"
UAM_URL = "https://github.com/devkitPro/uam.git"
UAM_COMMIT = "5a5afc2bae8b55409ab36ba45be63fcb73f68993"
UAM_DIR = ROOT / "source"
UAM_PATCH = DONOR / "port/switch/uam.patch"
tree = ast.parse((DONOR / "tools/switch_build.py").read_text())
selected = [n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name in {"fetch_uam", "_bison_for_uam", "_uam_build"}]
exec(compile(ast.Module(body=selected, type_ignores=[]), "donor_uam_recipe", "exec"))
with (ROOT / "build.ninja").open("w") as f:
    result = _uam_build(Writer(f), Path("/opt/devkitpro"), Path("/opt/devkitpro/devkitA64/bin/aarch64-none-elf-gcc"), "-march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIE")
    if result is None:
        raise SystemExit("UAM dependency setup failed")
print(result)
