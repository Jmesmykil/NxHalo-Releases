#!/usr/bin/env python3
"""Native developer build: python tools/build_setup.py --output NEW_DIRECTORY.

Build-time pip/license retrieval is permitted; the resulting application is offline.
The output is exclusive and never touches an SD or user game files.
"""
from __future__ import annotations
import argparse
import hashlib
import importlib.metadata
import json
import os
import stat
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile

PIN = "6.16.0"
ROOT = Path(__file__).resolve().parents[1]
SOURCES = ("installer/wizard.py", "installer/installer.py", "installer/loading_image.py",
           "tools/setup_entrypoint.py", "tools/build_setup.py", ".github/workflows/setup-packages.yml", "LICENSE.md")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, **kwargs):
    subprocess.run(command, check=True, cwd=ROOT, **kwargs)


def retrieve_license(url, destination):
    with urllib.request.urlopen(url, timeout=30) as response:
        content = response.read(2_000_000)
    if len(content) < 100 or content.lstrip().startswith(b"<"):
        raise RuntimeError("invalid license response")
    destination.write_bytes(content)
    return {"file": destination.name, "source": url, "sha256": sha(destination)}


def licenses(directory, smoke):
    directory.mkdir()
    records = []
    shutil.copyfile(ROOT / "LICENSE.md", directory / "NxHalo-LICENSE.md")
    records.append({"file": "NxHalo-LICENSE.md", "source": "repository LICENSE.md",
                    "sha256": sha(directory / "NxHalo-LICENSE.md")})
    py_version = platform.python_version()
    records.append(retrieve_license(
        f"https://raw.githubusercontent.com/python/cpython/v{py_version}/LICENSE",
        directory / "Python-LICENSE.txt"))
    # Read the installed pinned packager distribution rather than an arbitrary HEAD.
    dist = importlib.metadata.distribution("pyinstaller")
    candidates = [dist.locate_file(f) for f in (dist.files or [])
                  if str(f).endswith("COPYING.txt")]
    if not candidates:
        raise RuntimeError("installed PyInstaller COPYING.txt missing")
    shutil.copyfile(candidates[0], directory / "PyInstaller-COPYING.txt")
    records.append({"file": "PyInstaller-COPYING.txt", "source": "installed PyInstaller " + PIN,
                    "sha256": sha(directory / "PyInstaller-COPYING.txt")})
    # Retrieve the license for the Tcl/Tk version actually bundled by this Python.
    for name, project in (("Tcl", "tcl"), ("Tk", "tk")):
        records.append(retrieve_license(
            f"https://raw.githubusercontent.com/tcltk/{project}/core-{smoke[project + '_patchlevel'].replace('.', '-')}/license.terms",
            directory / (name + "-license.terms")))
    (directory / "NOTICES.txt").write_text(
        "NxHalo Setup includes Python, Tcl/Tk, and the PyInstaller bootloader.\n"
        "Their license texts are in this folder. PyInstaller's GPL includes a bootloader exception.\n"
        "No game maps, game executable, private keys, or Switch runtime installer are bundled.\n"
        "The app performs local operations and has no download mechanism.\n", encoding="utf-8")
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--dry-run", action="store_true", help="Read-only path and source validation")
    args = parser.parse_args()
    output = args.output.expanduser().absolute()
    if output.exists() or output.is_symlink():
        parser.error("output must not already exist")
    if not output.parent.is_dir():
        parser.error("output parent must already exist")
    for source in SOURCES:
        if not (ROOT / source).is_file():
            parser.error("required source missing: " + source)
    if platform.system() not in ("Windows", "Darwin"):
        parser.error("native Windows or macOS required")
    if args.dry_run:
        print(json.dumps({"output": str(output), "sources": {s: sha(ROOT / s) for s in SOURCES}}))
        return
    if importlib.metadata.version("pyinstaller") != PIN:
        parser.error("requires PyInstaller " + PIN)
    run([sys.executable, "-m", "unittest", "discover", "-s", "installer/tests", "-v"])
    output.mkdir()
    name = "NxHalo-Setup"
    with tempfile.TemporaryDirectory(prefix="nxhalo-setup-build-") as scratch:
        scratch = Path(scratch)
        source_receipt = scratch / "source-smoke.json"
        run([sys.executable, str(ROOT / "tools/setup_entrypoint.py"), "--self-test", "--receipt", str(source_receipt)], timeout=60)
        separator = ";" if platform.system() == "Windows" else ":"
        run([sys.executable, "-m", "PyInstaller", "--noconfirm", "--clean", "--onedir", "--windowed",
             "--name", name, "--distpath", str(output / "dist"), "--workpath", str(scratch / "work"),
             "--specpath", str(scratch), "--paths", str(ROOT / "installer"),
             "--add-data", str(ROOT / "installer/loading_image.py") + separator + "installer",
             str(ROOT / "tools/setup_entrypoint.py")])
        if platform.system() == "Darwin":
            package = output / "dist" / (name + ".app")
            executable = package / "Contents/MacOS" / name
        else:
            package = output / "dist" / name
            executable = package / (name + ".exe")
        if not executable.is_file():
            raise RuntimeError("packaged executable missing")
        frozen_receipt = scratch / "frozen-smoke.json"
        run([str(executable), "--self-test", "--receipt", str(frozen_receipt)], timeout=60)
        smoke = json.loads(frozen_receipt.read_text())
        if smoke.get("result") != "pass" or smoke.get("frozen") is not True:
            raise RuntimeError("packaged GUI smoke receipt invalid")
        shutil.copyfile(source_receipt, output / "source-smoke.json")
        shutil.copyfile(frozen_receipt, output / "frozen-smoke.json")
        license_records = licenses(output / "licenses", smoke)
        manifest = {
            "schema": 1, "application": name, "python": platform.python_version(),
            "pyinstaller": PIN, "os": platform.system(), "os_release": platform.release(),
            "architecture": platform.machine(), "source_sha256": {s: sha(ROOT / s) for s in SOURCES},
            "git_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
            "gui_smoke": smoke, "licenses": license_records,
            "qa_limits": ["First-user file chooser flow unverified", "Real game import unverified in this build",
                          "Unsigned desktop application", "No physical Switch acceptance implied"],
            "files": {str(p.relative_to(output)): {"bytes": p.stat().st_size, "sha256": sha(p)}
                      for p in sorted(output.rglob("*")) if p.is_file()},
        }
        (output / "build-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        archive = output / f"{name}-{platform.system().lower()}-{platform.machine()}.zip"
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as bundle:
            members = list(package.rglob("*")) + list((output / "licenses").rglob("*"))
            members += [output / "build-manifest.json", output / "source-smoke.json", output / "frozen-smoke.json"]
            for member in sorted(members):
                if member.is_file() or member.is_symlink():
                    # Strip dist/, so app/exe folder is immediately visible after extraction.
                    arcname = str(member.relative_to(output / "dist")) if member.is_relative_to(output / "dist") else str(member.relative_to(output))
                    if member.is_symlink():
                        info = zipfile.ZipInfo(arcname)
                        info.create_system = 3
                        info.external_attr = (stat.S_IFLNK | 0o777) << 16
                        bundle.writestr(info, os.readlink(member))
                    else:
                        bundle.write(member, arcname)
        print(str(archive))


if __name__ == "__main__":
    main()
