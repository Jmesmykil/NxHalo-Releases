"""Synthetic format and failure-path tests; no game assets or real private keys."""
import contextlib
import hashlib
import io
import json
import os
from pathlib import Path, PureWindowsPath
import struct
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import installer as core
from loading_image import make_loading_tga


def map_bytes(name, build="01.10.12.2276", version=5):
    data = bytearray(2048)
    data[:4] = b"daeh"
    struct.pack_into("<iii", data, 4, version, 4096, 0)
    struct.pack_into("<ii", data, 16, 3072, 256)
    data[32:32 + len(name[:-4])] = name[:-4].encode()
    data[64:64 + len(build)] = build.encode()
    data[2044:] = b"toof"
    return bytes(data)


def folder_fixture(root):
    game = root / "game"
    maps = game / "maps"
    maps.mkdir(parents=True)
    for name in core.MAP_NAMES:
        (maps / name).write_bytes(map_bytes(name))
    (maps / "._ui.map").write_bytes(b"OS metadata, not game data")
    (game / "default.xbe").write_bytes(b"Unrelated file, never staged")
    return game


def xiso_fixture(path, base=0):
    # Tables are 4-byte-aligned records with a right-child chain. Sparse full-disc
    # prefixes test partition arithmetic without allocating proprietary data.
    table = bytearray()
    offsets = []
    for index, name in enumerate(core.MAP_NAMES):
        offsets.append(len(table))
        raw = name.encode()
        table += struct.pack("<HHIIBB", 0, 0, 35 + index, 2048, 0, len(raw)) + raw
        table += b"\0" * (-len(table) % 4)
    for offset, next_offset in zip(offsets, offsets[1:]):
        struct.pack_into("<H", table, offset + 2, next_offset // 4)
    # 0xffff also means no child in XDVDFS.
    struct.pack_into("<H", table, offsets[-1] + 2, 0xFFFF)
    root = struct.pack("<HHIIBB", 0, 0, 34, len(table), 0x10, 4) + b"maps"
    media = bytearray(2048)
    media[:20] = core.MEDIA_MAGIC
    struct.pack_into("<II", media, 20, 33, len(root))  # Last record has no padding.
    media[2028:] = core.MEDIA_MAGIC
    with path.open("wb") as file:
        file.truncate(base + 59 * 2048)
        file.seek(base + 0x10000)
        file.write(media)
        file.seek(base + 33 * 2048)
        file.write(root)
        file.seek(base + 34 * 2048)
        file.write(table)
        for index, name in enumerate(core.MAP_NAMES):
            file.seek(base + (35 + index) * 2048)
            file.write(map_bytes(name))
    return offsets


def runtime_fixture(root):
    inputs = root / "runtime inputs"
    inputs.mkdir()
    host = b"NSO0" + b"\0" * 252
    npdm = bytearray(0xC0)
    npdm[:4] = b"META"
    struct.pack_into("<II", npdm, 0x70, 0x80, 0x40)
    npdm[0x80:0x84] = b"ACI0"
    struct.pack_into("<Q", npdm, 0x90, int(core.TITLE_ID, 16))
    guest = bytearray(288)
    ident = b"\x7fELF\x02\x01\x01" + bytes(9)
    guest[:64] = struct.pack("<16sHHIQQQIHHHHHH", ident, 2, 183, 1, 0x88000000,
                            64, 0, 0, 64, 56, 2, 0, 0, 0)
    struct.pack_into("<IIQQQQQQ", guest, 64, 1, 5, 256, 0x88000000, 0x88000000, 16, 4096, 4096)
    struct.pack_into("<IIQQQQQQ", guest, 120, 1, 6, 272, 0x88001000, 0x88001000, 16, 4096, 4096)
    blobs = (host, npdm, bytes(0x4000), b"\xff\xd8ORIGINAL-TEST\xff\xd9", guest)
    files = {}
    for index, (name, data) in enumerate(zip(core.RUNTIME_FILES, blobs)):
        source = inputs / (str(index) + ".bin")
        source.write_bytes(data)
        files[name] = source
    kit = root / "runtime-kit"
    core.create_runtime_kit(files, "synthetic-runtime-1", kit)
    return kit, files


def pfs0_bytes(corrupt=False):
    payloads = [bytes([index]) * 4096 for index in (1, 2, 3)]
    names = [hashlib.sha256(data).hexdigest()[:32] + (".cnmt.nca" if index == 2 else ".nca")
             for index, data in enumerate(payloads)]
    strings = b""
    entries = b""
    offset = 0
    for name, data in zip(names, payloads):
        entries += struct.pack("<QQII", offset, len(data), len(strings), 0)
        strings += name.encode() + b"\0"
        offset += len(data)
    if corrupt:
        payloads[0] = b"X" + payloads[0][1:]
    return struct.pack("<4sIII", b"PFS0", 3, len(strings), 0) + entries + strings + b"".join(payloads)


def fake_packer(root, mode="success"):
    path = root / ("local packer " + mode)
    # No shell interpolation. The fake emits simulated sensitive diagnostics;
    # build's subprocess suppression must prevent these reaching any receipt/UI.
    path.write_text("#!/usr/bin/env python3\n" +
        "import sys,struct,hashlib\nfrom pathlib import Path\n" +
        "print('SENSITIVE-FAKE-KEY-012345',flush=True)\n" +
        "mode=" + repr(mode) + "\n" +
        "if mode=='failure': sys.exit(2)\n" +
        "def argument(name): return Path(sys.argv[sys.argv.index(name)+1])\n" +
        "maps=argument('--romfsdir')/'maps'\n" +
        "expected=" + repr(set(core.MAP_NAMES) | {"loading.tga", "shaders.bin"}) + "\n" +
        "assert {p.name for p in maps.iterdir()}==(set() if mode=='runtime' else expected)\n" +
        "if mode!='runtime': assert (maps/'loading.tga').stat().st_size==230418\n" +
        "blobs=[bytes([i])*4096 for i in (1,2,3)]\n" +
        "names=[hashlib.sha256(b).hexdigest()[:32]+('.cnmt.nca' if i==2 else '.nca') for i,b in enumerate(blobs)]\n" +
        "strings=b''; entries=b''; offset=0\n" +
        "for name,blob in zip(names,blobs):\n" +
        " entries+=struct.pack('<QQII',offset,len(blob),len(strings),0);strings+=name.encode()+b'\\0';offset+=len(blob)\n" +
        "if mode=='corrupt': blobs[0]=b'X'+blobs[0][1:]\n" +
        "data=struct.pack('<4sIII',b'PFS0',3,len(strings),0)+entries+strings+b''.join(blobs)\n" +
        "if mode=='truncated': data=data[:-1]\n" +
        "(argument('--nspdir')/'" + core.TITLE_ID + ".nsp').write_bytes(data)\n",
        encoding="utf-8")
    path.chmod(0o700)
    return path


class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="nxhalo-test-", suffix=" with spaces")
        self.root = Path(self.temp.name)
        self.shader_mock = mock.patch.object(core, "_shader_bytes", return_value=b"synthetic-shaders")
        self.shader_mock.start()
        self.hash_mock = mock.patch.object(core, "SHADER_SHA256", hashlib.sha256(b"synthetic-shaders").hexdigest())
        self.hash_mock.start()

    def tearDown(self):
        self.shader_mock.stop()
        self.hash_mock.stop()
        self.temp.cleanup()

    def setup_build(self, mode="success"):
        game = folder_fixture(self.root)
        kit, _ = runtime_fixture(self.root)
        keys = self.root / "private.keys"
        keys.write_text("SENSITIVE-FAKE-KEY-012345", encoding="ascii")
        work = self.root / "work disk"
        work.mkdir()
        return game, kit, keys, work, self.root / "new installer", fake_packer(self.root, mode)

    def test_complete_folder_and_compressed_lengths(self):
        game = folder_fixture(self.root)
        result = core.inspect_game(game)
        self.assertEqual(len(result.maps), 24)
        self.assertEqual(result.maps[0].build, "01.10.12.2276")
        self.assertEqual(result.maps[0].size, 2048)  # Header logical length is 4096.
        self.assertEqual(core.inspect_game(game / "maps").maps, result.maps)
        self.assertNotIn(str(self.root), json.dumps(result.summary()))

    def test_folder_case_insensitive_names(self):
        game = folder_fixture(self.root)
        (game / "maps/ui.map").rename(game / "maps/UI.MAP")
        self.assertEqual(len(core.inspect_game(game).maps), 24)

    def test_missing_map(self):
        game = folder_fixture(self.root)
        (game / "maps/ui.map").unlink()
        with self.assertRaisesRegex(core.InstallerError, "missing: ui.map"):
            core.inspect_game(game)

    def test_wrong_version_build_name_and_footer(self):
        game = folder_fixture(self.root)
        target = game / "maps/ui.map"
        variants = [map_bytes("ui.map", version=7), map_bytes("ui.map", build="PC-unsupported"),
                    map_bytes("a10.map"), map_bytes("ui.map")[:-4] + b"foot"]
        for data in variants:
            with self.subTest(data=data[:4]):
                target.write_bytes(data)
                with self.assertRaises(core.InstallerError):
                    core.inspect_game(game)

    def test_mixed_builds(self):
        game = folder_fixture(self.root)
        (game / "maps/ui.map").write_bytes(map_bytes("ui.map", build="01.01.14.2342"))
        with self.assertRaisesRegex(core.InstallerError, "mix different"):
            core.inspect_game(game)

    def test_selected_map_symlink_rejected(self):
        game = folder_fixture(self.root)
        target = game / "maps/ui.map"
        saved = self.root / "actual-map"
        target.rename(saved)
        target.symlink_to(saved)
        with self.assertRaises(core.InstallerError):
            core.inspect_game(game)

    def test_all_xiso_partition_bases_and_unpadded_final_record(self):
        for base in core.XISO_BASES:
            with self.subTest(base=base):
                image = self.root / (str(base) + ".iso")
                xiso_fixture(image, base)
                result = core.inspect_game(image)
                self.assertEqual(result.partition_base, base)
                self.assertEqual(len(result.maps), 24)
                self.assertEqual(result.maps[0].offset, base + 35 * 2048)

    def test_xiso_wrong_trailing_magic(self):
        image = self.root / "game.iso"
        xiso_fixture(image)
        with image.open("r+b") as file:
            file.seek(0x10000 + 2028)
            file.write(b"X")
        with self.assertRaises(core.InstallerError):
            core.inspect_game(image)

    def test_xiso_tree_cycle(self):
        image = self.root / "game.iso"
        offsets = xiso_fixture(image)
        with image.open("r+b") as file:
            file.seek(34 * 2048 + offsets[1] + 2)
            file.write(struct.pack("<H", offsets[1] // 4))
        with self.assertRaisesRegex(core.InstallerError, "cycle"):
            core.inspect_game(image)

    def test_xiso_file_outside_image(self):
        image = self.root / "game.iso"
        xiso_fixture(image)
        with image.open("r+b") as file:
            file.seek(34 * 2048 + 4)
            file.write(struct.pack("<I", 0xFFFFFFFF))
        with self.assertRaisesRegex(core.InstallerError, "outside"):
            core.inspect_game(image)

    def test_xiso_overlapping_map_regions(self):
        image = self.root / "game.iso"
        xiso_fixture(image)
        with image.open("r+b") as file:
            file.seek(34 * 2048 + 8)
            file.write(struct.pack("<I", 4096))
        with self.assertRaisesRegex(core.InstallerError, "overlapping"):
            core.inspect_game(image)

    def test_xiso_unsafe_directory_name(self):
        image = self.root / "game.iso"
        xiso_fixture(image)
        with image.open("r+b") as file:
            file.seek(33 * 2048 + 14)
            file.write(b"../x")
        with self.assertRaisesRegex(core.InstallerError, "unsafe"):
            core.inspect_game(image)

    def test_runtime_allowlist_hashes_and_metadata(self):
        kit, _ = runtime_fixture(self.root)
        (kit / ".DS_Store").write_bytes(b"ignored OS metadata")
        (kit / "exefs/._main").write_bytes(b"ignored OS metadata")
        manifest = core.load_runtime_kit(kit)
        self.assertEqual(set(manifest["files"]), set(core.RUNTIME_FILES))
        self.assertNotIn(str(self.root), json.dumps(manifest))
        (kit / "prod.keys").write_text("SENSITIVE-FAKE-KEY-012345")
        with self.assertRaisesRegex(core.InstallerError, "extra files"):
            core.load_runtime_kit(kit)

    def test_runtime_corruption_and_wrong_title_id(self):
        kit, files = runtime_fixture(self.root)
        with (kit / "exefs/main").open("ab") as file:
            file.write(b"X")
        with self.assertRaisesRegex(core.InstallerError, "SHA-256"):
            core.load_runtime_kit(kit)
        bad = bytearray(files["exefs/main.npdm"].read_bytes())
        struct.pack_into("<Q", bad, 0x90, 0)
        files["exefs/main.npdm"].write_bytes(bad)
        with self.assertRaisesRegex(core.InstallerError, "different application"):
            core.create_runtime_kit(files, "bad-kit", self.root / "bad-kit")
        self.assertFalse((self.root / "bad-kit").exists())

    def test_runtime_no_overwrite(self):
        kit, files = runtime_fixture(self.root)
        before = (kit / "runtime-manifest.json").read_bytes()
        with self.assertRaisesRegex(core.InstallerError, "already exists"):
            core.create_runtime_kit(files, "replacement", kit)
        self.assertEqual((kit / "runtime-manifest.json").read_bytes(), before)

    def test_guest_elf64_container_and_reject_elf32_or_out_of_bounds_segment(self):
        kit, files = runtime_fixture(self.root)
        guest = files["romfs/halo_guest.elf"]
        original = guest.read_bytes()
        data = bytearray(original)
        data[4] = 1  # ILP32 pointers do not imply ELF32 container.
        guest.write_bytes(data)
        with self.assertRaisesRegex(core.InstallerError, "ELF64"):
            core.create_runtime_kit(files, "elf32-rejected", self.root / "invalid32")
        data = bytearray(original)
        struct.pack_into("<Q", data, 64 + 8, 999999)  # p_offset outside the file.
        guest.write_bytes(data)
        with self.assertRaisesRegex(core.InstallerError, "invalid load segment"):
            core.create_runtime_kit(files, "bad-segment", self.root / "bad-segment")

    def test_pfs0_content_hashes_and_truncated_payload(self):
        path = self.root / "synthetic.nsp"
        path.write_bytes(pfs0_bytes())
        self.assertEqual(len(core.verify_nsp(path)["contents"]), 3)
        path.write_bytes(pfs0_bytes(corrupt=True))
        with self.assertRaisesRegex(core.InstallerError, "SHA-256"):
            core.verify_nsp(path)
        path.write_bytes(pfs0_bytes()[:-1])
        with self.assertRaises(core.InstallerError):
            core.verify_nsp(path)

    def test_pfs0_excessive_string_table_and_trailing_bytes(self):
        path = self.root / "synthetic.nsp"
        data = bytearray(pfs0_bytes())
        struct.pack_into("<I", data, 8, 4097)
        path.write_bytes(data)
        with self.assertRaises(core.InstallerError):
            core.verify_nsp(path)
        path.write_bytes(pfs0_bytes() + b"EXTRA")
        with self.assertRaisesRegex(core.InstallerError, "trailing"):
            core.verify_nsp(path)

    def test_build_preserves_inputs_and_private_receipt(self):
        args = self.setup_build()
        before = {path: core.sha256_file(path) for path in args[0].rglob("*") if path.is_file()}
        before[args[2]] = core.sha256_file(args[2])
        result = core.build_installer(*args[:-1], packer=str(args[-1]))
        self.assertTrue((args[4] / "Halo_CE.nsp").is_file())
        self.assertEqual(core.verify_nsp(args[4] / "Halo_CE.nsp")["sha256"], result["installer"]["sha256"])
        self.assertEqual(len(result["maps"]), 25)
        self.assertEqual(result["generated_asset"]["bytes"], 230418)
        text = (args[4] / "build-receipt.json").read_text()
        self.assertNotIn(str(self.root), text)
        self.assertNotIn("SENSITIVE-FAKE-KEY", text)
        self.assertEqual(list(args[3].iterdir()), [])
        for path, digest in before.items():
            self.assertEqual(core.sha256_file(path), digest)

    def test_build_from_xiso_matches_folder_hashes(self):
        args = self.setup_build()
        image = self.root / "own image.iso"
        xiso_fixture(image)
        result = core.build_installer(image, *args[1:-1], packer=str(args[-1]))
        for record in result["maps"]:
            self.assertEqual(record["sha256"], hashlib.sha256(b"synthetic-shaders" if record["name"] == "shaders.bin" else map_bytes(record["name"])).hexdigest())

    def test_map_free_runtime_build_requires_no_game_and_excludes_maps(self):
        args = self.setup_build("runtime")
        result = core.build_installer(None, *args[1:-1], packer=str(args[-1]))
        self.assertTrue((args[4] / "Halo_CE_Runtime.nsp").is_file())
        self.assertEqual(result["maps"], [])
        self.assertIsNone(result["game"])
        self.assertIsNone(result["generated_asset"])

    def test_failed_packer_redacts_output_and_leaves_no_deliverable(self):
        args = self.setup_build("failure")
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            result = core.main(["build", "--game", str(args[0]), "--runtime-kit", str(args[1]),
                                "--keys", str(args[2]), "--workspace", str(args[3]),
                                "--output", str(args[4]), "--packer", str(args[5])])
        self.assertEqual(result, 1)
        self.assertNotIn("SENSITIVE-FAKE-KEY", out.getvalue() + err.getvalue())
        self.assertNotIn(str(args[2]), out.getvalue() + err.getvalue())
        self.assertFalse(args[4].exists())
        self.assertEqual(list(args[3].iterdir()), [])
        self.assertFalse(any(p.name.startswith(".nxhalo-delivery-") for p in self.root.iterdir()))

    def test_corrupt_packer_output_rolls_back(self):
        args = self.setup_build("corrupt")
        with self.assertRaisesRegex(core.InstallerError, "SHA-256"):
            core.build_installer(*args[:-1], packer=str(args[-1]))
        self.assertFalse(args[4].exists())
        self.assertEqual(list(args[3].iterdir()), [])

    def test_existing_output_is_preserved(self):
        args = self.setup_build()
        args[4].mkdir()
        (args[4] / "earlier.nsp").write_bytes(b"KEEP")
        with self.assertRaisesRegex(core.InstallerError, "already exists"):
            core.build_installer(*args[:-1], packer=str(args[-1]))
        self.assertEqual((args[4] / "earlier.nsp").read_bytes(), b"KEEP")

    def test_workspace_and_output_input_aliases_rejected(self):
        args = self.setup_build()
        with self.assertRaisesRegex(core.InstallerError, "outside your original"):
            core.build_installer(*args[:3], args[0] / "maps", args[4], str(args[5]))
        with self.assertRaisesRegex(core.InstallerError, "outside the runtime"):
            core.build_installer(*args[:4], args[1] / "new-output", str(args[5]))

    def test_space_shortage_before_staging(self):
        args = self.setup_build()
        with mock.patch.object(core.shutil, "disk_usage", return_value=mock.Mock(free=0)):
            with self.assertRaisesRegex(core.InstallerError, "needs at least"):
                core.build_installer(*args[:-1], packer=str(args[-1]))
        self.assertFalse(args[4].exists())
        self.assertEqual(list(args[3].iterdir()), [])

    def test_exclusive_rename_preserves_existing_empty_directory(self):
        source, target = self.root / "source", self.root / "existing"
        source.mkdir()
        (source / "data").write_bytes(b"READY")
        target.mkdir()
        with self.assertRaises(OSError):
            core._rename_no_replace(source, target)
        self.assertTrue(source.is_dir())
        self.assertEqual(list(target.iterdir()), [])

    def test_exclusive_copy_fallback_verifies_and_publishes_receipt_last(self):
        source = self.root / "completed"
        source.mkdir()
        (source / "payload").mkdir()
        (source / "payload/data.bin").write_bytes(b"ORIGINAL SYNTHETIC DATA")
        (source / "data-receipt.json").write_text('{"ready": true}')
        destination = self.root / "published"
        checked = []
        original_hash = core.sha256_file
        def track(path):
            if path != destination / "data-receipt.json":
                self.assertFalse((destination / "data-receipt.json").exists())
            checked.append(path.name)
            return original_hash(path)
        with mock.patch.object(core, "sha256_file", side_effect=track):
            core._publish_exclusive_copy(source, destination)
        self.assertEqual(checked[-1], "data-receipt.json")
        self.assertEqual((destination / "payload/data.bin").read_bytes(), b"ORIGINAL SYNTHETIC DATA")
        self.assertFalse(source.exists())

    def test_exclusive_copy_fallback_never_reuses_existing_destination(self):
        source = self.root / "source"
        destination = self.root / "existing"
        source.mkdir()
        destination.mkdir()
        (source / "data").write_bytes(b"NEW")
        with self.assertRaises(FileExistsError):
            core._publish_exclusive_copy(source, destination)
        self.assertEqual(list(destination.iterdir()), [])
        self.assertEqual((source / "data").read_bytes(), b"NEW")

    def test_publication_ignores_source_os_metadata_without_overwriting_generated_metadata(self):
        source = self.root / "source"
        (source / "control").mkdir(parents=True)
        (source / "control/data.bin").write_bytes(b"PAYLOAD")
        (source / "._control").write_bytes(b"SOURCE OS METADATA")
        (source / ".DS_Store").write_bytes(b"SOURCE FINDER METADATA")
        destination = self.root / "destination"
        original_mkdir = Path.mkdir
        def mkdir(path, *args, **kwargs):
            result = original_mkdir(path, *args, **kwargs)
            if path == destination / "control":
                (destination / "._control").write_bytes(b"AUTO DESTINATION METADATA")
            return result
        with mock.patch.object(Path, "mkdir", mkdir):
            core._publish_exclusive_copy(source, destination)
        self.assertEqual((destination / "control/data.bin").read_bytes(), b"PAYLOAD")
        self.assertEqual((destination / "._control").read_bytes(), b"AUTO DESTINATION METADATA")
        self.assertFalse((destination / ".DS_Store").exists())

    def test_exclusive_copy_failure_rolls_back_ours_and_preserves_foreign_entry(self):
        source = self.root / "source"
        destination = self.root / "destination"
        source.mkdir()
        (source / "a.bin").write_bytes(b"OUR DATA")
        (source / "data-receipt.json").write_text('{"ready": true}')
        def fail(path):
            (destination / "foreign.bin").write_bytes(b"PRESERVE PEER FILE")
            return "0" * 64
        with mock.patch.object(core, "sha256_file", side_effect=fail):
            with self.assertRaisesRegex(core.InstallerError, "SHA-256"):
                core._publish_exclusive_copy(source, destination)
        self.assertEqual({p.name for p in destination.iterdir()}, {"foreign.bin"})
        self.assertEqual((destination / "foreign.bin").read_bytes(), b"PRESERVE PEER FILE")
        self.assertTrue(source.is_dir())

    @unittest.skipIf(os.name == "nt", "Windows rename already provides exclusive semantics")
    def test_native_enotsup_uses_safe_copy_fallback(self):
        source = self.root / "source"
        source.mkdir()
        (source / "data-receipt.json").write_text('{"ready": true}')
        operation = mock.Mock(return_value=-1)
        library = mock.Mock(renamex_np=operation, renameat2=operation)
        destination = self.root / "destination"
        with mock.patch.object(core.ctypes, "CDLL", return_value=library), mock.patch.object(core.ctypes, "get_errno", return_value=core.errno.ENOTSUP):
            core._rename_no_replace(source, destination)
        self.assertEqual((destination / "data-receipt.json").read_text(), '{"ready": true}')
        self.assertFalse(source.exists())

    def test_scratch_cleanup_preserves_symlink_target_and_external_files(self):
        external = self.root / "external"
        external.mkdir()
        (external / "preserve.bin").write_bytes(b"PRESERVE ORIGINAL")
        with core._scratch("own-scratch-", self.root) as scratch:
            saved = scratch
            (scratch / "nested").mkdir()
            (scratch / "nested/temp.bin").write_bytes(b"TEMP")
            (scratch / "external-link").symlink_to(external, target_is_directory=True)
        self.assertFalse(saved.exists())
        self.assertEqual((external / "preserve.bin").read_bytes(), b"PRESERVE ORIGINAL")

    def test_publish_failure_removes_hidden_copy_and_no_output(self):
        args = self.setup_build()
        with mock.patch.object(core, "_rename_no_replace", side_effect=OSError("simulated delivery failure")):
            with self.assertRaises(core.InstallerError):
                core.build_installer(*args[:-1], packer=str(args[-1]))
        self.assertFalse(args[4].exists())
        self.assertEqual(list(args[3].iterdir()), [])
        self.assertFalse(any(p.name.startswith(".nxhalo-delivery-") for p in self.root.iterdir()))

    def test_key_free_data_export_preserves_source_and_hashes_all_copies(self):
        game = folder_fixture(self.root)
        output = self.root / "portable game data"
        result = core.export_data(game, output)
        self.assertEqual(result["operation"], "export-data")
        self.assertEqual({p.name for p in (output / "maps").iterdir()}, set(core.MAP_NAMES) | {"loading.tga", "shaders.bin"})
        for record in result["maps"]:
            self.assertEqual(core.sha256_file(output / "maps" / record["name"]), record["sha256"])
            self.assertEqual((output / "maps" / record["name"]).read_bytes(), (b"synthetic-shaders" if record["name"]=="shaders.bin" else (game / "maps" / record["name"]).read_bytes()))
        text = (output / "data-receipt.json").read_text()
        self.assertNotIn(str(self.root), text)
        self.assertEqual((output / "maps/loading.tga").read_bytes(), make_loading_tga())

    def test_key_free_export_from_partition_image(self):
        image = self.root / "own game.iso"
        xiso_fixture(image, core.XISO_BASES[1])
        output = self.root / "portable"
        self.assertEqual(len(core.export_data(image, output)["maps"]), 25)
        self.assertEqual((output / "maps/ui.map").read_bytes(), map_bytes("ui.map"))

    def test_export_existing_output_and_input_alias_are_preserved(self):
        game = folder_fixture(self.root)
        output = self.root / "existing"
        output.mkdir()
        with self.assertRaises(core.InstallerError):
            core.export_data(game, output)
        self.assertEqual(list(output.iterdir()), [])
        with self.assertRaisesRegex(core.InstallerError, "outside your original"):
            core.export_data(game, game / "new-output")

    def test_export_space_shortage_and_publish_failure_cleanup(self):
        game = folder_fixture(self.root)
        output = self.root / "new-output"
        with mock.patch.object(core.shutil, "disk_usage", return_value=mock.Mock(free=0)):
            with self.assertRaisesRegex(core.InstallerError, "needs at least"):
                core.export_data(game, output)
        with mock.patch.object(core, "_rename_no_replace", side_effect=OSError("simulated failure")):
            with self.assertRaises(core.InstallerError):
                core.export_data(game, output)
        self.assertFalse(output.exists())
        self.assertFalse(any(p.name.startswith(".nxhalo-data-") for p in self.root.iterdir()))

    def faux_sd(self):
        sd = self.root / "faux SD"
        halo = sd / "switch/halo"
        (halo / "save").mkdir(parents=True)
        (halo / "save/checkpoint.bin").write_bytes(b"PRESERVE SAVE")
        (halo / "config.toml").write_bytes(b"PRESERVE SETTINGS")
        (sd / "switch/other-app.nro").write_bytes(b"PRESERVE APP")
        return sd

    def test_explicit_sd_prepare_and_idempotence_preserve_other_files(self):
        game = folder_fixture(self.root)
        sd = self.faux_sd()
        protected = {p: (p.read_bytes(), p.stat().st_mtime_ns) for p in sd.rglob("*") if p.is_file()}
        result = core.prepare_sd(game, sd)
        self.assertEqual(result["operation"], "prepare-sd")
        maps = sd / "switch/halo/maps"
        self.assertEqual({p.name for p in maps.iterdir()}, set(core.MAP_NAMES) | {"loading.tga", "shaders.bin", "data-receipt.json"})
        before = {p: (core.sha256_file(p), p.stat().st_mtime_ns) for p in maps.iterdir()}
        (maps / ".DS_Store").write_bytes(b"metadata")
        again = core.prepare_sd(game, sd)
        self.assertEqual(again["operation"], "already-matches")
        for path, value in before.items():
            self.assertEqual((core.sha256_file(path), path.stat().st_mtime_ns), value)
        for path, value in protected.items():
            self.assertEqual((path.read_bytes(), path.stat().st_mtime_ns), value)

    def test_different_existing_sd_maps_are_not_overwritten(self):
        game = folder_fixture(self.root)
        sd = self.faux_sd()
        core.prepare_sd(game, sd)
        changed = sd / "switch/halo/maps/ui.map"
        changed.write_bytes(b"EXISTING DIFFERENT PRIVATE DATA")
        before = {p: core.sha256_file(p) for p in sd.rglob("*") if p.is_file()}
        with self.assertRaisesRegex(core.InstallerError, "back up or rename"):
            core.prepare_sd(game, sd)
        for path, digest in before.items():
            self.assertEqual(core.sha256_file(path), digest)

    def test_sd_root_selection_and_symlink_guards(self):
        game = folder_fixture(self.root)
        wrong = self.root / "not an SD"
        wrong.mkdir()
        with self.assertRaisesRegex(core.InstallerError, "existing switch"):
            core.prepare_sd(game, wrong)
        sd = self.faux_sd()
        (sd / "switch/halo/maps").symlink_to(game / "maps", target_is_directory=True)
        with self.assertRaisesRegex(core.InstallerError, "symbolic link"):
            core.prepare_sd(game, sd)
        with self.assertRaisesRegex(core.InstallerError, "inside your original"):
            core.prepare_sd(game, game / "maps")

    def test_windows_sd_drive_root_allowed_and_system_drive_protected(self):
        self.assertFalse(core._system_root(PureWindowsPath("E:\\"), "C:"))
        self.assertTrue(core._system_root(PureWindowsPath("C:\\"), "C:"))
        self.assertTrue(core._system_root(PureWindowsPath("E:\\"), "E:"))
        self.assertFalse(core._system_root(PureWindowsPath("C:\\Users\\Player"), "C:"))
        self.assertTrue(core._system_root(Path("/")))

    def test_sd_low_space_and_failed_delivery_do_not_create_maps(self):
        game = folder_fixture(self.root)
        sd = self.root / "empty faux SD"
        (sd / "switch").mkdir(parents=True)
        with mock.patch.object(core.shutil, "disk_usage", return_value=mock.Mock(free=0)):
            with self.assertRaises(core.InstallerError):
                core.prepare_sd(game, sd)
        self.assertFalse((sd / "switch/halo").exists())
        with mock.patch.object(core, "_rename_no_replace", side_effect=OSError("simulated failure")):
            with self.assertRaises(core.InstallerError):
                core.prepare_sd(game, sd)
        self.assertFalse((sd / "switch/halo").exists())

    def test_gui_worker_redacts_unexpected_failure_and_blocks_close(self):
        try:
            import wizard
        except SystemExit:
            self.skipTest("Tk not installed; GUI worker test needs the optional preview dependency")
        import queue
        app = wizard.Wizard.__new__(wizard.Wizard)
        app.root = mock.Mock()
        app.events = queue.Queue()
        app.controls = []
        app.progress = mock.Mock()
        app.open_button = mock.Mock()
        def operation():
            raise RuntimeError("SENSITIVE-FAKE-KEY /private/secret/keys")
        app.start(operation)
        kind, message = app.events.get(timeout=5)
        self.assertEqual(kind, "error")
        self.assertNotIn("SENSITIVE", message)
        self.assertNotIn("/private/", message)
        with mock.patch.object(wizard.messagebox, "showinfo"):
            app.close()
        app.root.destroy.assert_not_called()

    def test_original_loading_image_contract_determinism_orientation(self):
        first = make_loading_tga()
        self.assertEqual(first, make_loading_tga())
        self.assertEqual(len(first), 18 + 320 * 240 * 3)
        fields = struct.unpack("<BBBHHBHHHHBB", first[:18])
        self.assertEqual(fields, (0, 0, 2, 0, 0, 0, 0, 0, 320, 240, 24, 0))
        self.assertEqual(first[18:21], bytes([32]) * 3)  # Bottom-left pixel first.
        self.assertEqual(first[-960:-957], bytes([8]) * 3)  # Top-left pixel last row.
        self.assertTrue(all(first[index] == first[index + 1] == first[index + 2]
                            for index in range(18, len(first), 3)))


if __name__ == "__main__":
    unittest.main()
