#!/usr/bin/env python3
"""Exercise actual syscall179 production handler with deterministic Horizon shims."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
s = (root / "host/host_syscall.c").read_text()
handler = s[s.index("static long long host_sysinfo("):s.index("/* ---------- clocks")]
# Dispatch is checked separately so this shim cannot hide wrong syscall number.
assert "#define __NR_sysinfo       179" in s
assert "case __NR_sysinfo:\n\t\treturn host_sysinfo((uint64_t)a);" in s
with tempfile.TemporaryDirectory(prefix="nxhalo-sysinfo-") as tmp:
    temporary = Path(tmp)
    (temporary / "sysinfo_production.h").write_text(handler)
    binary = temporary / "test_sysinfo"
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-I", str(temporary),
                    "-I", str(root / "host"), str(root / "tests/test_sysinfo.c"),
                    "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
