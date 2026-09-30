#!/usr/bin/env python3
"""Build a private Switch installer from a runtime kit and user-supplied game data.

Python standard library only. No downloads, key parsing, input writes, or SD access.
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
import ctypes
import errno
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path, PureWindowsPath
import re
import shutil
import stat
import struct
import subprocess
import sys
import tempfile
import time
from typing import Callable

from loading_image import make_loading_tga

TITLE_ID = "010048414c4f0000"
RUNTIME_FILES = (
    "exefs/main", "exefs/main.npdm", "control/control.nacp",
    "control/icon_AmericanEnglish.dat", "romfs/halo_guest.elf",
)
MAP_NAMES = tuple(name + ".map" for name in (
    "a10", "a30", "a50", "b30", "b40", "beavercreek", "bloodgulch",
    "boardingaction", "c10", "c20", "c40", "carousel", "chillout", "d20",
    "d40", "damnation", "hangemhigh", "longest", "prisoner", "putput",
    "ratrace", "sidewinder", "ui", "wizard",
))
MAP_BUILDS = ("01.01.14.2342", "01.10.12.2276")
MAX_MAP_BYTES = 0x11600000
SECTOR_SIZE = 2048
MEDIA_MAGIC = b"MICROSOFT*XBOX*MEDIA"
# XboxDev/extract-xiso's linear 2048-byte data-partition offsets.
XISO_BASES = (0, 0x0FD90000, 0x02080000, 0x18300000)
CHUNK = 4 * 1024 * 1024
HEADROOM = 512 * 1024 * 1024
SHADER_BYTES = 34628
SHADER_SHA256 = "ecd7c682d76eb1c72f5fac8b47893bedc278eccb3fa6bd68bded2ee1462b4dd6"
MAX_XBE_BYTES = 64 * 1024 * 1024
SHADER_HEADER = struct.pack("<II", 467064, 0)


class InstallerError(Exception):
    """A human-readable failure that never includes key contents or private paths."""


@dataclass(frozen=True)
class MapInput:
    name: str
    size: int
    build: str
    source: Path
    offset: int = 0


@dataclass(frozen=True)
class ShaderInput:
    source: Path
    offset: int
    size: int


@dataclass(frozen=True)
class GameInput:
    kind: str
    source: Path
    maps: tuple[MapInput, ...]
    partition_base: int | None = None
    executable: ShaderInput | None = None

    def summary(self) -> dict:
        result = {
            "input_kind": self.kind, "map_count": len(self.maps),
            "game_build": self.maps[0].build,
            "map_bytes": sum(item.size for item in self.maps),
            "original_shader_bytes": SHADER_BYTES,
            "maps": [{"name": item.name, "bytes": item.size} for item in self.maps],
            "validation": "Required map set and cache headers; gameplay is tested separately.",
        }
        if self.partition_base is not None:
            result["partition_base"] = self.partition_base
        return result


def _read_exact(file, size: int) -> bytes:
    value = file.read(size)
    if len(value) != size:
        raise InstallerError("A selected file is truncated. Use a complete game dump or runtime kit.")
    return value


def _hash_region(file, size: int, output=None) -> str:
    digest = hashlib.sha256()
    while size:
        data = file.read(min(size, CHUNK))
        if not data:
            raise InstallerError("A selected file changed or is truncated. Retry from the original files.")
        digest.update(data)
        if output is not None:
            output.write(data)
        size -= len(data)
    return digest.hexdigest()


def sha256_file(path: Path) -> str:
    with path.open("rb") as file:
        return _hash_region(file, path.stat().st_size)


def _regular(path: Path, label: str) -> Path:
    if path.is_symlink() or not path.is_file():
        raise InstallerError(f"Choose a regular {label} file, rather than a shortcut or symbolic link.")
    return path.resolve(strict=True)


def _header_string(value: bytes, label: str) -> str:
    if b"\0" not in value:
        raise InstallerError(f"A map has an invalid {label} in its cache header.")
    try:
        return value.split(b"\0", 1)[0].decode("ascii")
    except UnicodeDecodeError:
        raise InstallerError(f"A map has an invalid {label} in its cache header.") from None


def check_map_header(header: bytes, name: str, physical_size: int) -> str:
    # The footer is part of the 2048-byte HEADER, not the end of the map file.
    # Retail maps can be compressed: length/tag offsets refer to decompressed bytes.
    if len(header) != 2048 or header[:4] != b"daeh" or header[2044:] != b"toof":
        raise InstallerError("Game data must be original Xbox Halo cache maps; a map header is invalid.")
    version, logical_length = struct.unpack_from("<ii", header, 4)
    if version != 5:
        raise InstallerError("These maps are not Xbox cache version 5. PC, Anniversary, and Halo Custom Edition data are unsupported.")
    if not 2048 <= logical_length <= MAX_MAP_BYTES or not 2048 <= physical_size <= MAX_MAP_BYTES:
        raise InstallerError("A map has an invalid cache length or is incomplete.")
    map_name = _header_string(header[32:64], "name")
    build = _header_string(header[64:96], "build")
    if map_name.casefold() != name[:-4]:
        raise InstallerError("A required map's filename does not match its cache header.")
    if build not in MAP_BUILDS:
        raise InstallerError("Unsupported Halo game build. Use Xbox build 01.10.12.2276 or 01.01.14.2342.")
    return build


def _check_map_set(items: dict[str, MapInput]) -> tuple[MapInput, ...]:
    missing = [name for name in MAP_NAMES if name not in items]
    if missing:
        raise InstallerError("Required maps are missing: " + ", ".join(missing) + ". Choose the complete game or maps folder.")
    result = tuple(items[name] for name in MAP_NAMES)
    if len({item.build for item in result}) != 1:
        raise InstallerError("The required maps mix different Halo builds. Use one complete game dump.")
    return result


def _inspect_folder(folder: Path) -> GameInput:
    if folder.is_symlink():
        raise InstallerError("Choose the actual game folder, rather than a symbolic link.")
    root = folder.resolve(strict=True)
    children = list(root.iterdir())
    maps_dirs = [item for item in children if item.name.casefold() == "maps"]
    if maps_dirs:
        if len(maps_dirs) != 1 or maps_dirs[0].is_symlink() or not maps_dirs[0].is_dir():
            raise InstallerError("The game's maps folder is ambiguous or is a symbolic link.")
        maps_root = maps_dirs[0]
    else:
        maps_root = root
    items: dict[str, MapInput] = {}
    for item in maps_root.iterdir():
        name = item.name.casefold()
        if name not in MAP_NAMES:  # Excludes AppleDouble, executables, extras, and personal files.
            continue
        if name in items:
            raise InstallerError("The maps folder contains duplicate map names differing only in case.")
        source = _regular(item, "map")
        size = source.stat().st_size
        with source.open("rb") as file:
            build = check_map_header(_read_exact(file, 2048), name, size)
        items[name] = MapInput(name, size, build, source)
    executables = [item for item in root.iterdir() if item.name.casefold() == "default.xbe"]
    if len(executables) > 1:
        raise InstallerError("The game folder has ambiguous default.xbe files.")
    executable = None
    if executables:
        xbe = _regular(executables[0], "original game executable")
        executable = ShaderInput(xbe, 0, xbe.stat().st_size)
    return GameInput("maps-folder", root, _check_map_set(items), executable=executable)


def _safe_xiso_name(raw: bytes) -> str:
    try:
        name = raw.decode("cp1252")
    except UnicodeDecodeError:
        raise InstallerError("The Xbox image contains an invalid Windows-1252 filename.") from None
    if (not name or name in (".", "..") or any(c in name for c in '/\\:\0')
            or any(ord(c) < 32 or ord(c) == 127 for c in name)
            or name.endswith((".", " "))):
        raise InstallerError("The Xbox image contains an unsafe filename.")
    return name


def _check_nonoverlap(regions: list[tuple[int, int]], message="The Xbox image has overlapping file or directory regions.") -> None:
    regions.sort()
    for previous, current in zip(regions, regions[1:]):
        if current[0] < previous[1]:
            raise InstallerError(message)


def _inspect_xiso(source: Path) -> GameInput:
    source = _regular(source, "Xbox image")
    image_size = source.stat().st_size
    matches = []
    with source.open("rb") as file:
        for base in XISO_BASES:
            offset = base + 0x10000
            if offset + SECTOR_SIZE > image_size:
                continue
            file.seek(offset)
            media = _read_exact(file, SECTOR_SIZE)
            if media[:20] == MEDIA_MAGIC and media[2028:] == MEDIA_MAGIC:
                root_sector, root_size = struct.unpack_from("<II", media, 20)
                matches.append((base, root_sector, root_size))
        if len(matches) != 1:
            raise InstallerError("Choose a complete linear Xbox XISO/ISO or an extracted maps folder. Compressed archives and other disc layouts are unsupported.")
        base, root_sector, root_size = matches[0]
        regions = [(base + 0x10000, base + 0x10800)]
        pending = [(root_sector, root_size, (), 0)]
        directory_regions = set()
        maps: dict[str, MapInput] = {}
        executable = None
        node_count = 0
        while pending:
            sector, size, path_parts, depth = pending.pop()
            start = base + sector * SECTOR_SIZE
            if depth > 16 or not 14 <= size <= 1024 * 1024 or start < base or start + size > image_size:
                raise InstallerError("The Xbox image has an invalid directory extent or excessive nesting.")
            region = (start, start + size)
            if region in directory_regions:
                raise InstallerError("The Xbox image contains a directory cycle or duplicate directory reference.")
            directory_regions.add(region)
            regions.append(region)
            file.seek(start)
            table = _read_exact(file, size)
            offsets = [0]
            visited = set()
            record_regions = []
            names = set()
            while offsets:
                offset = offsets.pop()
                if offset in visited or offset % 4 or offset + 14 > len(table):
                    raise InstallerError("The Xbox image contains a directory tree cycle or invalid pointer.")
                visited.add(offset)
                if table[offset:offset + 14] in (b"\0" * 14, b"\xff" * 14):
                    continue  # XDVDFS permits empty directory-table records.
                node_count += 1
                if node_count > 20000:
                    raise InstallerError("The Xbox image contains too many directory records.")
                left, right, entry_sector, entry_size, attributes, name_length = struct.unpack_from("<HHIIBB", table, offset)
                record_end = offset + 14 + name_length
                if record_end > size:
                    raise InstallerError("The Xbox image has a truncated directory record.")
                name = _safe_xiso_name(table[offset + 14:offset + 14 + name_length]).casefold()
                if name in names:
                    raise InstallerError("The Xbox image contains duplicate directory filenames.")
                names.add(name)
                record_regions.append((offset, record_end))
                for child in (left, right):
                    if child not in (0, 0xFFFF):
                        offsets.append(child * 4)
                entry_start = base + entry_sector * SECTOR_SIZE
                if entry_start < base or entry_start + entry_size > image_size:
                    raise InstallerError("The Xbox image has a file extent outside the image.")
                entry_parts = path_parts + (name,)
                if attributes & 0x10:
                    if entry_size:
                        pending.append((entry_sector, entry_size, entry_parts, depth + 1))
                else:
                    if entry_size:
                        regions.append((entry_start, entry_start + entry_size))
                    if entry_parts == ("default.xbe",):
                        if executable is not None:
                            raise InstallerError("The Xbox image contains duplicate original game executables.")
                        executable = ShaderInput(source, entry_start, entry_size)
                    if entry_parts in (("maps", name), (name,)) and name in MAP_NAMES:
                        if name in maps:
                            raise InstallerError("The Xbox image contains more than one required map set.")
                        file.seek(entry_start)
                        build = check_map_header(_read_exact(file, 2048), name, entry_size)
                        maps[name] = MapInput(name, entry_size, build, source, entry_start)
            _check_nonoverlap(record_regions)
        _check_nonoverlap(regions)
    return GameInput("xbox-image", source, _check_map_set(maps), base, executable)


def inspect_game(path: Path) -> GameInput:
    try:
        return _inspect_folder(path) if path.is_dir() else _inspect_xiso(path)
    except OSError:
        raise InstallerError("Cannot read the selected game. Check permissions and disk connection.") from None


def _runtime_format(path: Path, name: str) -> None:
    size = path.stat().st_size
    limits = {"exefs/main": 128 * 1024 * 1024, "exefs/main.npdm": 65536,
              "control/control.nacp": 0x4000, "control/icon_AmericanEnglish.dat": 1024 * 1024,
              "romfs/halo_guest.elf": 64 * 1024 * 1024}
    if size > limits[name]:
        raise InstallerError("A runtime file exceeds this installer's supported size limit.")
    with path.open("rb") as file:
        if name == "exefs/main":
            if size < 0x100 or _read_exact(file, 4) != b"NSO0":
                raise InstallerError("The runtime host is not an NSO file.")
        elif name == "exefs/main.npdm":
            if size < 0x80:
                raise InstallerError("The runtime metadata is truncated.")
            header = _read_exact(file, 0x80)
            if header[:4] != b"META":
                raise InstallerError("The runtime metadata is not an NPDM file.")
            aci_offset, aci_size = struct.unpack_from("<II", header, 0x70)
            if aci_size < 0x40 or aci_offset < 0x80 or aci_offset + aci_size > size:
                raise InstallerError("The runtime metadata has an invalid application record.")
            file.seek(aci_offset)
            aci = _read_exact(file, 0x40)
            if aci[:4] != b"ACI0" or struct.unpack_from("<Q", aci, 0x10)[0] != int(TITLE_ID, 16):
                raise InstallerError("The runtime metadata uses a different application title ID.")
        elif name == "control/control.nacp":
            if size != 0x4000:
                raise InstallerError("The runtime control metadata has an invalid length.")
        elif name == "control/icon_AmericanEnglish.dat":
            if not 4 <= size <= 1024 * 1024 or _read_exact(file, 2) != b"\xff\xd8":
                raise InstallerError("The runtime icon is not a supported JPEG.")
            file.seek(-2, os.SEEK_END)
            if _read_exact(file, 2) != b"\xff\xd9":
                raise InstallerError("The runtime icon is truncated.")
        elif name == "romfs/halo_guest.elf":
            if size < 64:
                raise InstallerError("The runtime guest is truncated.")
            header = _read_exact(file, 64)
            fields = struct.unpack("<16sHHIQQQIHHHHHH", header)
            ident, kind, machine, version, _, phoff, _, _, ehsize, phsize, phcount, *_ = fields
            # Guest pointers use ILP32, but its file container is ELF64. The
            # actual Switch loader uses Elf64_Ehdr and 56-byte Elf64_Phdr.
            if (ident[:7] != b"\x7fELF\x02\x01\x01" or kind != 2 or machine != 183 or version != 1
                    or ehsize != 64 or phsize != 56 or not 1 <= phcount <= 128
                    or phoff < 64 or phoff + phcount * phsize > size):
                raise InstallerError("The runtime guest must be an ELF64 little-endian AArch64 executable with a valid program-header table.")
            file.seek(phoff)
            loads = []
            for _ in range(phcount):
                ptype, flags, offset, address, _, file_bytes, memory_bytes, _ = struct.unpack("<IIQQQQQQ", _read_exact(file, 56))
                if ptype != 1:
                    continue
                if (not memory_bytes or file_bytes > memory_bytes or offset + file_bytes > size
                        or not 0x88000000 <= address < 0x89000000 or address + memory_bytes > 0x89000000
                        or flags & 3 == 3):
                    raise InstallerError("The runtime guest has an invalid load segment.")
                loads.append((address, address + memory_bytes, flags))
            if not loads:
                raise InstallerError("The runtime guest has no load segments.")
            _check_nonoverlap([(start, end) for start, end, _ in loads], "The runtime guest has overlapping load segments.")
            low = min(start for start, _, _ in loads) & ~0xFFF
            writable = min((start for start, _, flags in loads if flags & 2), default=0)
            readonly_end = max((end for _, end, flags in loads if not flags & 2), default=0)
            if low != 0x88000000 or writable <= low or writable % 4096 or readonly_end > writable:
                raise InstallerError("The runtime guest's load layout does not match the Switch loader.")


def _manifest_for(files: dict[str, Path], build_id: str) -> dict:
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,79}", build_id):
        raise InstallerError("Use a build identifier of 1–80 letters, numbers, dots, underscores, or dashes.")
    records = {}
    for name in RUNTIME_FILES:
        path = _regular(files[name], "runtime")
        _runtime_format(path, name)
        records[name] = {"bytes": path.stat().st_size, "sha256": sha256_file(path)}
    return {"schema": 1, "title_id": TITLE_ID, "build_id": build_id, "files": records}


def load_runtime_kit(directory: Path) -> dict:
    try:
        if directory.is_symlink() or not directory.is_dir():
            raise InstallerError("Choose an extracted runtime-kit folder.")
        manifest_path = _regular(directory / "runtime-manifest.json", "runtime manifest")
        if manifest_path.stat().st_size > 16384:
            raise InstallerError("The runtime manifest is too large.")
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        if (not isinstance(manifest, dict) or set(manifest) != {"schema", "title_id", "build_id", "files"}
                or type(manifest["schema"]) is not int or manifest["schema"] != 1 or manifest["title_id"] != TITLE_ID
                or not isinstance(manifest["files"], dict) or set(manifest["files"]) != set(RUNTIME_FILES)):
            raise InstallerError("The runtime manifest does not match this installer version.")
        # Validate the entire kit allowlist, including intermediate directories.
        expected = {"runtime-manifest.json", *RUNTIME_FILES, "exefs", "control", "romfs"}
        entries = [p for p in directory.rglob("*") if not (
            not p.is_symlink() and p.is_file() and (p.name == ".DS_Store" or p.name.startswith("._")))]
        if len(entries) != len(expected) or any(p.is_symlink() or p.relative_to(directory).as_posix() not in expected for p in entries):
            raise InstallerError("The runtime kit has extra files, game assets, or symbolic links. Obtain a clean runtime kit.")
        current = _manifest_for({name: directory / name for name in RUNTIME_FILES}, manifest["build_id"])
        if current != manifest:
            raise InstallerError("A runtime file failed its length or SHA-256 check. Download or rebuild a clean kit.")
        return manifest
    except (OSError, ValueError, TypeError, KeyError):
        raise InstallerError("Cannot read a valid runtime kit. Check the selected folder and manifest.") from None


def _new_output_directory(output: Path) -> None:
    if output.exists() or output.is_symlink():
        raise InstallerError("The output folder already exists. Choose a new folder so earlier builds stay untouched.")
    if not output.parent.is_dir():
        raise InstallerError("Choose an output folder inside an existing directory.")


def _write_json(path: Path, data: dict) -> None:
    with path.open("x", encoding="utf-8", newline="\n") as file:
        json.dump(data, file, indent=2, sort_keys=True)
        file.write("\n")
        file.flush()
        os.fsync(file.fileno())


def _remove_scratch_tree(root: Path) -> None:
    """Remove only an operation's new scratch tree using absolute-path operations.

    Some macOS removable-volume drivers fail dir_fd-relative unlink used by
    shutil.rmtree. Never change shutil's global mode. Do not follow symlinks.
    Callers pass only mkdtemp roots or generated children of those roots.
    """
    try:
        info = root.lstat()
    except FileNotFoundError:
        return
    if stat.S_ISDIR(info.st_mode):
        for child in list(root.iterdir()):
            _remove_scratch_tree(child)
        root.rmdir()
    else:
        root.unlink()


@contextmanager
def _scratch(prefix: str, directory: Path):
    root = Path(tempfile.mkdtemp(prefix=prefix, dir=directory))
    try:
        yield root
    finally:
        _remove_scratch_tree(root)


def _publish_exclusive_copy(source: Path, destination: Path) -> None:
    """Fallback for filesystems that reject exclusive directory rename.

    Directory publication is NOT atomic here. Create the destination exclusively,
    copy each file exclusively with readback verification, and copy receipts last.
    On failure remove only our own entries; preserve any unrelated new entries.
    """
    entries = [item for item in source.rglob("*") if not (
        not item.is_symlink() and item.is_file() and
        (item.name == ".DS_Store" or item.name.startswith("._")))]
    if any(item.is_symlink() or not (item.is_dir() or item.is_file()) for item in entries):
        raise InstallerError("The completed output contains an unexpected symbolic link or special file.")
    needed = sum(item.stat().st_size for item in entries if item.is_file()) + 128 * 1024 * 1024
    if shutil.disk_usage(destination.parent).free < needed:
        raise InstallerError("The output filesystem requires a verified publication copy, but has insufficient free space. Choose a disk with more space.")
    created_files = []
    created_dirs = []

    def make_directory(path):
        path.mkdir()  # Never reuse or replace an existing directory.
        info = path.stat()
        created_dirs.append((path, (info.st_dev, info.st_ino)))

    try:
        make_directory(destination)
        for item in sorted((p for p in entries if p.is_dir()), key=lambda p: len(p.parts)):
            make_directory(destination / item.relative_to(source))
        receipts = {"runtime-manifest.json", "data-receipt.json", "build-receipt.json"}
        files = sorted((p for p in entries if p.is_file()),
                       key=lambda p: (p.name in receipts, p.relative_to(source).as_posix()))
        for item in files:
            target = destination / item.relative_to(source)
            with item.open("rb") as original, target.open("xb") as copied:
                info = os.fstat(copied.fileno())
                record = {"path": target, "identity": (info.st_dev, info.st_ino)}
                created_files.append(record)
                try:
                    expected = _hash_region(original, item.stat().st_size, copied)
                    copied.flush()
                    os.fsync(copied.fileno())
                finally:
                    info = os.fstat(copied.fileno())
                    record["identity"] = (info.st_dev, info.st_ino)
            if sha256_file(target) != expected:
                raise InstallerError("A publication copy failed its SHA-256 check. No completed output is ready.")
        _remove_scratch_tree(source)  # Only our freshly created completed stage.
    except BaseException:
        for record in reversed(created_files):
            try:
                info = record["path"].lstat()
                if (info.st_dev, info.st_ino) == record["identity"]:
                    record["path"].unlink()
            except OSError:
                pass
        for directory, identity in reversed(created_dirs):
            try:
                info = directory.lstat()
                if (info.st_dev, info.st_ino) == identity:
                    directory.rmdir()  # Fails safely if another program added anything.
            except OSError:
                pass
        raise


def _rename_no_replace(source: Path, destination: Path) -> None:
    """Publish without replacing a destination; atomic where the FS supports it.

    Plain POSIX rename can replace an existing empty directory. Use the native
    exclusive operation on supported systems. exFAT/FAT drivers may reject it;
    there, use exclusive creation and receipt-last verified copies instead.
    """
    if os.name == "nt":
        os.rename(source, destination)  # Windows rename rejects any existing target.
        return
    libc = ctypes.CDLL(None, use_errno=True)
    if sys.platform == "darwin":
        operation = libc.renamex_np
        operation.argtypes = (ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint)
        operation.restype = ctypes.c_int
        result = operation(os.fsencode(source), os.fsencode(destination), 0x00000004)  # RENAME_EXCL
    elif sys.platform.startswith("linux") and hasattr(libc, "renameat2"):
        operation = libc.renameat2
        operation.argtypes = (ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint)
        operation.restype = ctypes.c_int
        result = operation(-100, os.fsencode(source), -100, os.fsencode(destination), 1)  # AT_FDCWD, RENAME_NOREPLACE
    else:
        raise InstallerError("This platform lacks atomic no-replace delivery. Use the preview on a supported macOS, Windows, or Linux system.")
    if result:
        error = ctypes.get_errno()
        if error in {errno.ENOTSUP, errno.EOPNOTSUPP, errno.ENOSYS}:
            _publish_exclusive_copy(source, destination)
        else:
            raise OSError(error, "Could not publish the completed folder without replacement")


def create_runtime_kit(files: dict[str, Path], build_id: str, output: Path) -> dict:
    """Maintainer operation: copy only the five explicit runtime inputs."""
    try:
        _new_output_directory(output)
        manifest = _manifest_for(files, build_id)
        with _scratch(".nxhalo-kit-", output.parent) as scratch:
            stage = Path(scratch) / "kit"
            stage.mkdir()
            for name in RUNTIME_FILES:
                target = stage / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(files[name], target)
            _write_json(stage / "runtime-manifest.json", manifest)
            load_runtime_kit(stage)  # Catch input changes during copying.
            _rename_no_replace(stage, output)
        return manifest
    except OSError:
        raise InstallerError("Could not create the runtime kit. Check output permissions and free space.") from None


def find_packer(value: str | None = None) -> Path:
    if value:
        candidate = Path(value).expanduser()
        if not candidate.is_file():
            resolved = shutil.which(value)
            if resolved:
                candidate = Path(resolved)
    else:
        suffix = ".exe" if os.name == "nt" else ""
        bundled = Path(__file__).resolve().parent / "tools" / ("hacbrewpack" + suffix)
        found = str(bundled) if bundled.is_file() else shutil.which("hacbrewpack" + suffix)
        if not found:
            raise InstallerError("Choose a local hacbrewpack executable, or place it in the installer's tools folder.")
        candidate = Path(found)
    if not candidate.is_file() or (os.name != "nt" and not os.access(candidate, os.X_OK)):
        raise InstallerError("The selected hacbrewpack executable is missing or cannot run.")
    return candidate.resolve(strict=True)


def verify_nsp(path: Path) -> dict:
    """Bounded PFS0 verification; hashes encrypted NCAs without interpreting keys."""
    total_size = path.stat().st_size
    if not 16 <= total_size <= 0xFFFFFFFF:
        raise InstallerError("The generated installer is truncated or exceeds the FAT32 single-file limit.")
    with path.open("rb") as file:
        magic, count, string_size, reserved = struct.unpack("<4sIII", _read_exact(file, 16))
        if magic != b"PFS0" or count != 3 or not 1 <= string_size <= 4096 or reserved:
            raise InstallerError("The packer did not produce the expected three-content NSP.")
        entries = [struct.unpack("<QQII", _read_exact(file, 24)) for _ in range(count)]
        strings = _read_exact(file, string_size)
        data_start = file.tell()
        records = []
        seen = set()
        for offset, size, name_offset, entry_reserved in entries:
            if entry_reserved or name_offset >= len(strings) or size < 0xC00 or data_start + offset + size > total_size:
                raise InstallerError("The generated NSP has an invalid content record.")
            tail = strings[name_offset:]
            if b"\0" not in tail:
                raise InstallerError("The generated NSP has an unterminated content name.")
            try:
                name = tail.split(b"\0", 1)[0].decode("ascii")
            except UnicodeDecodeError:
                raise InstallerError("The generated NSP has an invalid content name.") from None
            if not re.fullmatch(r"[0-9a-f]{32}(?:\.cnmt)?\.nca", name) or name in seen:
                raise InstallerError("The generated NSP contains unexpected or duplicate files.")
            seen.add(name)
            file.seek(data_start + offset)
            digest = _hash_region(file, size)
            if not name.startswith(digest[:32]):
                raise InstallerError("Generated content failed its SHA-256 filename check. No installer was delivered.")
            records.append({"name": name, "bytes": size, "sha256": digest, "offset": offset})
        if sum(name.endswith(".cnmt.nca") for name in seen) != 1:
            raise InstallerError("The generated NSP must contain exactly one content-metadata file.")
        end = 0
        for record in sorted(records, key=lambda item: item["offset"]):
            if record["offset"] != end:
                raise InstallerError("The generated NSP has overlapping content, gaps, or trailing bytes.")
            end += record["bytes"]
        if data_start + end != total_size:
            raise InstallerError("The generated NSP has trailing bytes or an invalid total length.")
    return {"bytes": total_size, "sha256": sha256_file(path),
            "contents": [{k: value for k, value in item.items() if k != "offset"} for item in records],
            "verification": "PFS0 bounds and all encrypted NCA hashes; native launch is tested separately."}


def _inside(child: Path, parent: Path) -> bool:
    try:
        child.resolve().relative_to(parent.resolve())
        return True
    except ValueError:
        return False


def _shader_bytes(game: GameInput) -> bytes:
    item = game.executable
    if item is None:
        raise InstallerError("Choose the complete Xbox game image or extracted game folder containing default.xbe and maps. A maps-only folder cannot provide the required original shaders.")
    if not SHADER_BYTES <= item.size <= MAX_XBE_BYTES or item.offset < 0:
        raise InstallerError("The original game executable is incomplete or unsupported.")
    with item.source.open("rb") as source:
        before = os.fstat(source.fileno())
        if item.offset + item.size > before.st_size:
            raise InstallerError("The original game executable extends beyond its input file.")
        source.seek(item.offset)
        data = _read_exact(source, item.size)
        after = os.fstat(source.fileno())
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise InstallerError("The original game input changed during preparation.")
    if data[:4] != b"XBEH":
        raise InstallerError("The selected game does not contain a recognized original Xbox executable.")
    matches = []
    position = 0
    while True:
        position = data.find(SHADER_HEADER, position)
        if position < 0:
            break
        candidate = data[position:position + SHADER_BYTES]
        if len(candidate) == SHADER_BYTES and hashlib.sha256(candidate).hexdigest() == SHADER_SHA256:
            matches.append(candidate)
        position += 1
    if len(matches) != 1:
        raise InstallerError("This game executable does not contain the supported Halo shader set. Use original Xbox build 01.10.12.2276; other game builds need separate compatibility testing.")
    return matches[0]


def _game_data(game: GameInput, maps_directory: Path | None = None,
               status: Callable[[str], None] = lambda message: None) -> tuple[list[dict], dict]:
    """Hash original maps and optionally stream them to our newly created stage."""
    shaders = _shader_bytes(game)  # Validate before creating any staged map files.
    records = []
    for item in game.maps:
        status("Checking/copying " + item.name + "…")
        with item.source.open("rb") as source:
            opened = os.fstat(source.fileno())
            if item.offset < 0 or item.offset + item.size > opened.st_size or (game.kind == "maps-folder" and opened.st_size != item.size):
                raise InstallerError("A selected map changed size during preparation. Retry from unchanged game files.")
            source.seek(item.offset)
            header = _read_exact(source, 2048)
            if check_map_header(header, item.name, item.size) != item.build:
                raise InstallerError("A map changed build during preparation. Retry from unchanged game files.")
            source.seek(item.offset)
            before = os.fstat(source.fileno())
            if maps_directory is None:
                digest = _hash_region(source, item.size)
            else:
                target = maps_directory / item.name
                with target.open("xb") as destination:
                    digest = _hash_region(source, item.size, destination)
                    destination.flush()
                    os.fsync(destination.fileno())
                if sha256_file(target) != digest:
                    raise InstallerError("A copied map failed its SHA-256 check. Choose a reliable output disk and retry.")
            after = os.fstat(source.fileno())
            if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
                raise InstallerError("A game input changed during preparation. Retry from unchanged game files.")
        records.append({"name": item.name, "bytes": item.size, "sha256": digest})
    if maps_directory is not None:
        target = maps_directory / "shaders.bin"
        with target.open("xb") as destination:
            destination.write(shaders)
            destination.flush()
            os.fsync(destination.fileno())
        if sha256_file(target) != SHADER_SHA256:
            raise InstallerError("The extracted shader data failed its copy check.")
    records.append({"name": "shaders.bin", "bytes": len(shaders), "sha256": hashlib.sha256(shaders).hexdigest(),
                    "provenance": "Extracted locally from the user's original default.xbe; never supplied by NxHalo"})
    loading = make_loading_tga()
    if maps_directory is not None:
        target = maps_directory / "loading.tga"
        with target.open("xb") as destination:
            destination.write(loading)
            destination.flush()
            os.fsync(destination.fileno())
        if sha256_file(target) != hashlib.sha256(loading).hexdigest():
            raise InstallerError("The generated loading image failed its copy check.")
    asset = {"name": "maps/loading.tga", "bytes": len(loading),
             "sha256": hashlib.sha256(loading).hexdigest(),
             "provenance": "Original deterministic NxHalo procedural loading image",
             "visual_acceptance": "Pending native Switch visual test"}
    return records, asset


def _data_receipt(game: GameInput, records: list[dict], asset: dict, operation: str) -> dict:
    return {"schema": 1, "title_id": TITLE_ID, "game": game.summary(), "maps": records,
            "generated_asset": asset, "operation": operation,
            "verification": "All 24 maps, locally extracted original shaders, and generated image checked with SHA-256; gameplay is tested separately.",
            "privacy": "No game image, private key, save, or absolute input path is included in this receipt."}


def _data_space(directory: Path, game: GameInput) -> None:
    required = (sum(item.size for item in game.maps) + SHADER_BYTES + len(make_loading_tga())) * 2 + 128 * 1024 * 1024
    if shutil.disk_usage(directory).free < required:
        raise InstallerError(f"The selected disk needs at least {required / 1024**3:.1f} GiB free to prepare this game's maps.")


def export_data(game_path: Path, output: Path,
                status: Callable[[str], None] = lambda message: None) -> dict:
    """Key-free portable data folder; no runtime, game executable, or SD discovery."""
    try:
        _new_output_directory(output)
        if game_path.is_dir() and _inside(output, game_path):
            raise InstallerError("Choose an output folder outside your original game folder.")
        status("Checking your own game's map set and cache headers…")
        game = inspect_game(game_path)
        _data_space(output.parent, game)
        with _scratch(".nxhalo-data-", output.parent) as scratch:
            delivery = Path(scratch) / "data"
            maps = delivery / "maps"
            maps.mkdir(parents=True)
            records, asset = _game_data(game, maps, status)
            receipt = _data_receipt(game, records, asset, "export-data")
            _write_json(delivery / "data-receipt.json", receipt)
            _rename_no_replace(delivery, output)
        status("Game data ready. Copy the maps folder to switch/halo/maps on your SD, install the separate runtime NSP through DBI, then launch from HOME.")
        return receipt
    except OSError:
        raise InstallerError("Game data could not be prepared. Check disk permissions and free space, then retry. Original files and earlier output folders remain unchanged.") from None


def _existing_maps_match(directory: Path, records: list[dict], asset: dict, allow_missing_shader: bool = False) -> bool:
    if directory.is_symlink() or not directory.is_dir():
        raise InstallerError("The existing maps location is a symbolic link or is not a folder. Select the actual SD root.")
    expected = {record["name"]: record for record in records}
    expected["loading.tga"] = asset
    if allow_missing_shader:
        expected.pop("shaders.bin", None)
    entries = [item for item in directory.iterdir() if not (
        not item.is_symlink() and item.is_file() and
        (item.name == ".DS_Store" or item.name.startswith("._") or item.name == "data-receipt.json"))]
    if {item.name for item in entries} != set(expected):
        return False
    for item in entries:
        record = expected[item.name]
        if item.is_symlink() or not item.is_file() or item.stat().st_size != record["bytes"] or sha256_file(item) != record["sha256"]:
            return False
    return True


def _system_root(path, windows_system_drive: str | None = None) -> bool:
    """Protect the host system root while allowing explicit Windows SD roots."""
    if isinstance(path, PureWindowsPath) or os.name == "nt":
        path = PureWindowsPath(path)
        system_drive = (windows_system_drive or os.environ.get("SystemDrive", "C:")).rstrip("/\\").casefold()
        return path == PureWindowsPath(path.anchor) and path.drive.casefold() == system_drive
    return Path(path).resolve() == Path("/")


def prepare_sd(game_path: Path, sd_root: Path,
               status: Callable[[str], None] = lambda message: None) -> dict:
    """Explicit selected SD only: new verified maps or an unchanged idempotent match."""
    try:
        if sd_root.is_symlink() or not sd_root.is_dir() or _system_root(sd_root.resolve()):
            raise InstallerError("Choose the mounted Switch SD's actual root folder, not a shortcut or system root.")
        if game_path.is_dir() and _inside(sd_root, game_path):
            raise InstallerError("The selected SD root is inside your original game folder. Choose the actual Switch SD root.")
        switch = sd_root / "switch"
        if switch.is_symlink() or not switch.is_dir():
            raise InstallerError("The selected folder must contain the Switch SD's existing switch folder. No drive is selected automatically.")
        halo = switch / "halo"
        if halo.is_symlink() or (halo.exists() and not halo.is_dir()):
            raise InstallerError("The SD's switch/halo location is a symbolic link or is not a folder.")
        target = halo / "maps"
        if target.is_symlink() or (target.exists() and not target.is_dir()):
            raise InstallerError("The SD's maps location is a symbolic link or is not a folder.")
        game = inspect_game(game_path)
        if target.exists() or target.is_symlink():
            status("Checking whether the existing SD maps already match your selected game…")
            records, asset = _game_data(game, status=status)
            if not (target / "shaders.bin").exists() and _existing_maps_match(target, records, asset, allow_missing_shader=True):
                # Upgrade matching legacy data with one exclusively created file.
                # No existing map, receipt, save, or setting is replaced.
                shaders = _shader_bytes(game)
                created = False
                created_identity = None
                try:
                    with (target / "shaders.bin").open("xb") as destination:
                        created = True
                        fd_stat = os.fstat(destination.fileno())
                        created_identity = (fd_stat.st_dev, fd_stat.st_ino)
                        destination.write(shaders)
                        destination.flush()
                        os.fsync(destination.fileno())
                    if sha256_file(target / "shaders.bin") != SHADER_SHA256:
                        raise InstallerError("The extracted shaders failed their copy check.")
                except BaseException:
                    if created:
                        try:
                            current = (target / "shaders.bin").lstat()
                            if (current.st_dev, current.st_ino) == created_identity:
                                (target / "shaders.bin").unlink()
                        except FileNotFoundError:
                            pass
                    raise
                status("Added the verified original shaders; existing maps, saves and settings are unchanged.")
                return _data_receipt(game, records, asset, "add-original-shaders")
            if not _existing_maps_match(target, records, asset):
                raise InstallerError("Existing switch/halo/maps differs from this game data. Keep it: back up or rename that maps folder yourself before retrying. Saves in switch/halo/save are untouched.")
            receipt = _data_receipt(game, records, asset, "already-matches")
            status("The SD maps already match. No files were changed and the SD remains connected.")
            return receipt
        _data_space(sd_root, game)
        halo_created = not halo.exists()
        if halo_created:
            halo.mkdir()
        try:
            with _scratch(".nxhalo-data-", halo) as scratch:
                maps = Path(scratch) / "maps"
                maps.mkdir()
                records, asset = _game_data(game, maps, status)
                receipt = _data_receipt(game, records, asset, "prepare-sd")
                _write_json(maps / "data-receipt.json", receipt)
                _rename_no_replace(maps, target)
        except BaseException:
            if halo_created:
                try:
                    halo.rmdir()  # Only an empty directory created by this operation.
                except OSError:
                    pass
            raise
        status("SD game data ready. Install the separate runtime NSP through DBI, then launch from HOME. The tool has not ejected the SD.")
        return receipt
    except OSError:
        raise InstallerError("The selected SD could not be read or written. Check permissions, free space, and connection. Existing maps, saves, and configuration were not replaced.") from None


def _disk_preflight(workspace: Path, output_parent: Path, payload_bytes: int) -> None:
    # Stage + RomFS/IVFC + NCA + NSP; conservative reserve instead of packer guesses.
    workspace_needed = payload_bytes * 5 + HEADROOM
    output_needed = payload_bytes * 2 + HEADROOM
    if workspace.stat().st_dev == output_parent.stat().st_dev:
        workspace_needed += output_needed
    elif shutil.disk_usage(output_parent).free < output_needed:
        raise InstallerError(f"The output disk needs at least {output_needed / 1024**3:.1f} GiB free. Choose a disk with more space.")
    if shutil.disk_usage(workspace).free < workspace_needed:
        raise InstallerError(f"The work disk needs at least {workspace_needed / 1024**3:.1f} GiB free for this build. Choose a disk with more space.")


def build_installer(game_path: Path | None, runtime_kit: Path, keys: Path,
                    workspace: Path, output: Path, packer: str | None = None,
                    status: Callable[[str], None] = lambda message: None) -> dict:
    """Stage, pack, verify, then deliver to a new folder. All inputs remain read-only."""
    started = time.monotonic()
    try:
        _new_output_directory(output)
        if workspace.is_symlink() or not workspace.is_dir():
            raise InstallerError("Choose an existing work folder on a disk with enough free space.")
        if _inside(workspace, runtime_kit) or _inside(output, runtime_kit):
            raise InstallerError("Keep the work and output folders outside the runtime kit.")
        if game_path is not None and game_path.is_dir() and (_inside(workspace, game_path) or _inside(output, game_path)):
            raise InstallerError("Keep the work and output folders outside your original game folder.")
        keys = _regular(keys, "private prod.keys")
        if not 1 <= keys.stat().st_size <= 4 * 1024 * 1024:
            raise InstallerError("The selected private key file is empty or unexpectedly large.")
        executable = find_packer(packer)
        status("Checking the runtime kit and your game data…")
        manifest = load_runtime_kit(runtime_kit)
        game = inspect_game(game_path) if game_path is not None else None
        payload_bytes = sum(item.size for item in game.maps) if game else 0
        payload_bytes += sum(item["bytes"] for item in manifest["files"].values())
        if payload_bytes + 64 * 1024 * 1024 > 0xFFFFFFFF:
            raise InstallerError("This game data is too large for a single FAT32 installer. Use the supported original Xbox maps.")
        _disk_preflight(workspace, output.parent, payload_bytes)
        with _scratch(".nxhalo-delivery-", output.parent) as delivery_scratch, _scratch("nxhalo-build-", workspace) as scratch:
            stage = Path(scratch)
            for directory in ("exefs", "control", "romfs/maps", "packed", "temporary", "nca"):
                (stage / directory).mkdir(parents=True, exist_ok=True)
            status("Copying the runtime" + (" and the 24 required maps…" if game else "…"))
            for name in RUNTIME_FILES:
                shutil.copyfile(runtime_kit / name, stage / name)
                if sha256_file(stage / name) != manifest["files"][name]["sha256"]:
                    raise InstallerError("The runtime changed during staging. Retry with an unchanged runtime kit.")
            map_records, generated_asset = _game_data(game, stage / "romfs/maps", status) if game else ([], None)
            status("Building the installer. This can take several minutes; keep the disks connected…")
            command = [str(executable), "-k", str(keys), "--nspdir", str(stage / "packed"),
                       "--tempdir", str(stage / "temporary"), "--ncadir", str(stage / "nca"),
                       "--exefsdir", str(stage / "exefs"), "--controldir", str(stage / "control"),
                       "--romfsdir", str(stage / "romfs"), "--nologo"]
            # Tool output may include key-related diagnostics. Never store or relay it.
            result = subprocess.run(command, cwd=stage, env=dict(os.environ, COPYFILE_DISABLE="1"),
                                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
            if result.returncode:
                raise InstallerError("The packer could not finish. Check that your local hacbrewpack and private keys are compatible, and try a work disk with more free space. Raw tool output was withheld.")
            packages = list((stage / "packed").iterdir())
            if (len(packages) != 1 or packages[0].is_symlink() or not packages[0].is_file()
                    or packages[0].name.casefold() != TITLE_ID + ".nsp"):
                raise InstallerError("The packer produced unexpected output. No installer was delivered.")
            status("Checking the complete installer and its content hashes…")
            package_info = verify_nsp(packages[0])
            installer_name = "Halo_CE.nsp" if game else "Halo_CE_Runtime.nsp"
            receipt = {"schema": 1, "title_id": TITLE_ID, "runtime_build": manifest["build_id"],
                       "runtime_files": manifest["files"], "game": game.summary() if game else None,
                       "maps": map_records, "generated_asset": generated_asset,
                       "installer": {"name": installer_name, **package_info},
                       "elapsed_seconds": round(time.monotonic() - started, 1),
                       "privacy": "No game image, private key, save, or absolute input path is included in this receipt."}
            # Keep the complete delivery hidden until exclusive directory rename.
            delivery = Path(delivery_scratch) / "installer"
            delivery.mkdir()
            pending = delivery / installer_name
            with packages[0].open("rb") as source, pending.open("xb") as destination:
                _hash_region(source, package_info["bytes"], destination)
                destination.flush()
                os.fsync(destination.fileno())
            if sha256_file(pending) != package_info["sha256"]:
                raise InstallerError("The output copy failed its SHA-256 check. Choose a reliable output disk and retry.")
            _write_json(delivery / "build-receipt.json", receipt)
            # Cleanup our own workspace before publishing a complete deliverable.
            _remove_scratch_tree(stage)
            _rename_no_replace(delivery, output)
        status("Installer ready: install " + installer_name + " through DBI" +
               (", then launch Halo CE from HOME." if game else ", prepare your own game maps, then launch from HOME. Native map-free acceptance is still pending."))
        return receipt
    except OSError:
        raise InstallerError("The build could not read or write a selected disk or start hacbrewpack. Check permissions, available space, and connected disks, then retry. No existing build was overwritten.") from None


def main(argv=None) -> int:
    if sys.version_info < (3, 10):
        print("This source preview requires Python 3.10 or newer.", file=sys.stderr)
        return 1
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    inspect = commands.add_parser("inspect-game", help="Check a user-supplied Xbox image or maps folder; no writes or keys.")
    inspect.add_argument("game", type=Path)
    export = commands.add_parser("export-data", help="Key-free: export verified maps and original loading image to a new portable folder.")
    export.add_argument("--game", type=Path, required=True)
    export.add_argument("--output", type=Path, required=True)
    sd = commands.add_parser("prepare-sd", help="Key-free: prepare only switch/halo/maps on an explicitly selected mounted SD.")
    sd.add_argument("--game", type=Path, required=True)
    sd.add_argument("--sd-root", type=Path, required=True)
    kit = commands.add_parser("create-runtime-kit", help="Maintainer: copy only five explicit runtime files into a new kit.")
    for name in ("host", "npdm", "nacp", "icon", "guest"):
        kit.add_argument("--" + name, type=Path, required=True)
    kit.add_argument("--build-id", required=True)
    kit.add_argument("--output", type=Path, required=True)
    build = commands.add_parser("build", help="Build a private NSP locally; all selected inputs remain unchanged.")
    for name in ("game", "runtime-kit", "keys", "workspace", "output"):
        build.add_argument("--" + name, type=Path, required=True)
    build.add_argument("--packer", help="Path to a local hacbrewpack executable; otherwise tools/ or PATH.")
    runtime = commands.add_parser("build-runtime", help="Maintainer: privately pack a map-free runtime NSP; requires local keys and packer.")
    for name in ("runtime-kit", "keys", "workspace", "output"):
        runtime.add_argument("--" + name, type=Path, required=True)
    runtime.add_argument("--packer")
    args = parser.parse_args(argv)
    try:
        if args.command == "inspect-game":
            result = inspect_game(args.game).summary()
        elif args.command == "export-data":
            result = export_data(args.game, args.output, lambda message: print(message, file=sys.stderr, flush=True))
        elif args.command == "prepare-sd":
            result = prepare_sd(args.game, args.sd_root, lambda message: print(message, file=sys.stderr, flush=True))
        elif args.command == "create-runtime-kit":
            result = create_runtime_kit(dict(zip(RUNTIME_FILES, (args.host, args.npdm, args.nacp, args.icon, args.guest))), args.build_id, args.output)
        else:
            result = build_installer(args.game if args.command == "build" else None, args.runtime_kit, args.keys, args.workspace,
                                     args.output, args.packer, lambda message: print(message, file=sys.stderr, flush=True))
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0
    except InstallerError as error:
        print("Build stopped: " + str(error), file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("Build interrupted. Original files and earlier installers remain unchanged.", file=sys.stderr)
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
