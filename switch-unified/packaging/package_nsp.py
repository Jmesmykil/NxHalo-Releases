#!/usr/bin/env python3
"""Pack the preserved Switch host/guest privately; never emit signing output."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser()
parser.add_argument('label')
parser.add_argument('--title-version', type=int, required=True, help='Monotonically increasing CNMT version, independent of display version')
parser.add_argument('--keys', type=Path, required=True, help='Local prod.keys; never copied into output')
parser.add_argument('--romfs', type=Path)
parser.add_argument('--packer', type=Path, default=Path('/Volumes/Media/nxhalo-takeover-20260929/switch-unified34/evidence/deko-integration-20261022/toolchain/hacbrewpack/hacbrewpack'))
parser.add_argument('--host-prefix', default='halo', choices=['halo','halo-deko'])
parser.add_argument('--guest', type=Path)
parser.add_argument('--validation-receipt', type=Path)
parser.add_argument('--external-work', action='store_true', help='Keep directory metadata on APFS; place large byte streams on Media')
args = parser.parse_args()
assert 0 < args.title_version <= 0xFFFFFFFF, "CNMT version must be a positive uint32"
assert args.keys.is_file(), "Local keys file is unavailable"
root = Path(__file__).resolve().parent / 'host-build/port/switch'
out = Path('/Volumes/Media/nxhalo-takeover-20260929') / args.label
out.mkdir(exist_ok=False)
stage = Path(tempfile.mkdtemp(prefix='nxhalo-' + args.label + '-'))
for directory in ['exefs', 'control', 'romfs']:
    (stage / directory).mkdir()
for source, target in [('halo.nso', 'exefs/main'), ('halo.npdm', 'exefs/main.npdm'),
                       ('halo.nacp', 'control/control.nacp'),
                       ('icon.jpg', 'control/icon_AmericanEnglish.dat'),
                       ('romfs/halo_guest.elf', 'romfs/halo_guest.elf'),
                       ('romfs/brokers.txt', 'romfs/brokers.txt'),
                       ('romfs/cacert.pem', 'romfs/cacert.pem'),
                       ('romfs/cacert-source.txt', 'romfs/cacert-source.txt'),
                       ('romfs/MPL-2.0.txt', 'romfs/MPL-2.0.txt')]:
    actual = root / source
    if source in ('halo.nso', 'halo.npdm', 'halo.nacp'):
        actual = root / (args.host_prefix + Path(source).suffix)
    if source == 'romfs/halo_guest.elf' and args.guest:
        actual = args.guest
    shutil.copyfile(actual, stage / target)

if args.host_prefix == 'halo-deko':
    for filename in ('shader_keys.dkk', 'DEKO3D-UPSTREAM-LICENSE'):
        shutil.copyfile(root / 'romfs-deko' / filename, stage / 'romfs' / filename)

# Automatic system capture: no separate application-owned 96 MiB buffer.
control_path = stage / 'control/control.nacp'
control = bytearray(control_path.read_bytes())
assert len(control) == 0x4000
control[0x3034] = 0  # screenshots allowed
control[0x3035] = 2  # automatic video recording
control_path.write_bytes(control)
assert control_path.read_bytes()[0x3034:0x3036] == bytes([0, 2])

def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as file:
        for chunk in iter(lambda: file.read(8 * 1024 * 1024), b''):
            value.update(chunk)
    return value.hexdigest()

romfs = args.romfs or stage / 'romfs'
assert not any(p.name.startswith('._') for p in romfs.rglob('*'))
guest_hash = digest(stage / 'romfs/halo_guest.elf')
assert digest(romfs / 'halo_guest.elf') == guest_hash
assert guest_hash == json.loads((args.validation_receipt or Path(__file__).resolve().parent / 'validation-receipt.json').read_text())['guest_sha256']
assert args.keys.is_file(), 'Local keys file is unavailable'
assert struct.pack('<Q', 0x010048414C210000) in (root / (args.host_prefix + '.npdm')).read_bytes()
maps = sorted((romfs / 'maps').glob('*.map'))
if args.external_work:
    scratch = out / 'large-intermediates'
    scratch.mkdir()
    for directory in ['temporary', 'nca']:
        (stage / directory).mkdir()
    # Upstream fopen/stat follow these APFS links; NCA rename renames the link.
    # Every enumerated directory stays on APFS, excluding exFAT AppleDouble.
    for directory, name in [('temporary', 'program_sec1_romfs'),
                            ('temporary', 'program_sec1_ivfc_lvl6'),
                            ('nca', 'program.nca')]:
        (stage / directory / name).symlink_to(scratch / name)
if args.romfs:
    assert len(maps) == 24 and sum(p.stat().st_size for p in maps) == 1860638720
    # RomFS, IVFC data and program NCA occupy roughly three copies on APFS.
    needed = sum(p.stat().st_size for p in romfs.rglob('*') if p.is_file()) * 3
    if args.external_work:
        assert shutil.disk_usage(out).free > needed + 4 * 1024**3, 'Insufficient external reserve'
        needed = 64 * 1024**2
    assert shutil.disk_usage(stage).free > needed + 2 * 1024**3, 'Insufficient APFS reserve'
manifest = {'label': args.label, 'stage': str(stage), 'romfs': str(romfs),
            'external_work': args.external_work, 'cnmt_title_version': args.title_version,
            'video_capture': 'automatic', 'control_sha256': digest(control_path),
            'host_sha256': digest(stage / 'exefs/main'), 'guest_sha256': guest_hash,
            'maps': [{'name': p.name, 'bytes': p.stat().st_size} for p in maps]}
(out / 'input-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print('Packing ' + args.label + ' with APFS intermediates', flush=True)
started = time.monotonic()
result = subprocess.run([str(args.packer), '-k',
    str(args.keys), '--nspdir', str(out),
    '--tempdir', str(stage / 'temporary'), '--ncadir', str(stage / 'nca'),
    '--exefsdir', str(stage / 'exefs'), '--controldir', str(stage / 'control'),
    '--romfsdir', str(romfs), '--nologo', '--keeptempdir', '--titleversion', str(args.title_version)], cwd=stage,
    env=dict(os.environ, COPYFILE_DISABLE='1'), stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
print('Packing exit', result.returncode, 'seconds', round(time.monotonic() - started), flush=True)
if result.returncode:
    raise SystemExit('Packaging failed; raw tool output withheld')
cnmt_bytes = (stage / 'temporary/cnmt/Application_010048414c210000.cnmt').read_bytes()
assert struct.unpack_from('<Q', cnmt_bytes, 0)[0] == 0x010048414C210000
assert struct.unpack_from('<I', cnmt_bytes, 8)[0] == args.title_version
(out / 'title-metadata.cnmt').write_bytes(cnmt_bytes)
packages = [p for p in out.glob('*.nsp') if not p.name.startswith('._')]
assert len(packages) == 1
package = packages[0]
with package.open('rb') as file:
    magic, count, string_size, _ = struct.unpack('<4sIII', file.read(16))
    assert magic == b'PFS0' and count == 3
    entries = [struct.unpack('<QQII', file.read(24)) for _ in range(count)]
    names = file.read(string_size)
    assert 16 + 24 * count + string_size + max(a+b for a,b,c,d in entries) == package.stat().st_size
assert all(not names[c:].split(b'\0')[0].startswith(b'._') for a,b,c,d in entries)
assert any(b >= 0xC00 and names[c:].split(b'\0')[0].endswith(b'.cnmt.nca') for a,b,c,d in entries)
manifest.update({'package': str(package), 'bytes': package.stat().st_size,
                 'sha256': digest(package),
                 'ncas': [{'name': names[c:].split(b'\0')[0].decode(), 'bytes': b}
                          for a,b,c,d in entries]})
(out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(json.dumps({k: manifest[k] for k in ['package', 'bytes', 'sha256', 'ncas']}, indent=2), flush=True)
if True:
    # Only this helper's newly created, finished scratch directory is removed.
    shutil.rmtree(stage)
    print('Released full-package APFS scratch space', flush=True)
