#!/usr/bin/env python3
"""Compile the production HP1 parser, host_log and report hook with log shims."""
from pathlib import Path
import subprocess
import re
import tempfile
root = Path(__file__).resolve().parents[1]
source = (root / "host/host_posix.c").read_text()
production = '#include "host_stall_profile.h"\n' + source[source.index("static struct host_stall_state"):source.index("/* ---------- writable")]
# Derive the complete watch/frame row bound from their production formats.
other_formats = []
for filename, marker in [("host/host_memory.c", "watch frame="), ("host/host_sdl.c", "frame_profile frame=")]:
    whole = (root / filename).read_text()
    start = whole.index('"' + marker)
    end = whole.index(',', start)
    literals = whole[start:end]
    plain = ''.join(re.findall(r'"([^"\n]*)"', literals))
    specs = re.findall(r'%[0-9]*(?:llu|u)', plain)
    args = ', '.join('(unsigned long long)UINT64_MAX' if spec.endswith('llu') else '(unsigned)UINT32_MAX' for spec in specs)
    other_formats.append(f'    bytes += (size_t)snprintf(NULL, 0, {literals}, {args}) + 8;')
production += '\nstatic size_t other_report_bytes(void) { size_t bytes = 0;\n' + '\n'.join(other_formats) + '\nreturn bytes; }\n'
with tempfile.TemporaryDirectory(prefix="nxhalo-phase-profile-") as tmp:
    temporary = Path(tmp)
    (temporary / "phase_production.h").write_text(production)
    binary = temporary / "test_phase_profile"
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-I", str(temporary),
                    "-I", str(root / "host"), str(root / "tests/test_phase_profile.c"),
                    "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
