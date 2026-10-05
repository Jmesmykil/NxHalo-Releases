"""Synthetic shader-extraction contract tests; no proprietary payload or keys."""
import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest import mock
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import installer as core
from test_installer import folder_fixture, xiso_fixture

class ShaderTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        self.payload=core.SHADER_HEADER+b"synthetic-only"+bytes(core.SHADER_BYTES-len(core.SHADER_HEADER)-14)
        self.payload=self.payload[:core.SHADER_BYTES].ljust(core.SHADER_BYTES,b"\0")
        self.patch=mock.patch.object(core,"SHADER_SHA256",hashlib.sha256(self.payload).hexdigest());self.patch.start()
    def tearDown(self):
        self.patch.stop();self.temp.cleanup()
    def game(self,data):
        p=self.root/"default.xbe";p.write_bytes(data)
        return core.GameInput("fixture",self.root,(),executable=core.ShaderInput(p,0,len(data)))
    def test_unique_payload_and_no_executable_export(self):
        game=folder_fixture(self.root);(game/"default.xbe").write_bytes(b"XBEH"+bytes(27)+self.payload+b"tail")
        output=self.root/"data";r=core.export_data(game,output)
        self.assertEqual((output/"maps/shaders.bin").read_bytes(),self.payload)
        self.assertFalse((output/"default.xbe").exists());self.assertEqual(r["maps"][-1]["provenance"],"Extracted locally from the user's original default.xbe; never supplied by NxHalo")
        self.assertNotIn(str(game),str(r))
    def test_bad_hash_missing_duplicate_and_bad_magic(self):
        for data in [b"XBEH"+bytes(core.SHADER_BYTES),b"XBEH"+self.payload[:-1]+b"x",b"XBEH"+self.payload+self.payload,b"NOPE"+self.payload]:
            with self.subTest(size=len(data)),self.assertRaises(core.InstallerError):core._shader_bytes(self.game(data))
    def test_missing_executable_maps_only_fails_before_output(self):
        game=folder_fixture(self.root);(game/"default.xbe").unlink();out=self.root/"new-data"
        with self.assertRaisesRegex(core.InstallerError,"complete Xbox"):core.export_data(game,out)
        self.assertFalse(out.exists())
    def test_xiso_bounded_root_executable(self):
        image=self.root/"game.iso";xiso_fixture(image)
        exe=b"XBEH"+bytes(128)+self.payload
        with image.open("r+b") as f:
            f.seek(33*2048);root=bytearray(f.read(18));struct.pack_into("<H",root,2,5)
            record=struct.pack("<HHIIBB",0,0,60,len(exe),0,11)+b"default.xbe"
            table=root+b"\0\0"+record;f.seek(33*2048);f.write(table)
            f.seek(0x10000+24);f.write(struct.pack("<I",len(table)))
            f.seek(60*2048);f.write(exe)
        game=core.inspect_game(image);self.assertEqual(game.executable.offset,60*2048)
        self.assertEqual(core._shader_bytes(game),self.payload)
    def test_upgrade_only_missing_shader_preserves_legacy_maps_and_save(self):
        game=folder_fixture(self.root);(game/"default.xbe").write_bytes(b"XBEH"+self.payload)
        sd=self.root/"sd";(sd/"switch").mkdir(parents=True)
        core.prepare_sd(game,sd);maps=sd/"switch/halo/maps";(maps/"shaders.bin").unlink()
        save=sd/"switch/halo/save";save.mkdir();(save/"keep").write_bytes(b"personal-save")
        before={p.name:p.read_bytes() for p in maps.iterdir()};r=core.prepare_sd(game,sd)
        self.assertEqual(r["operation"],"add-original-shaders")
        for name,data in before.items():self.assertEqual((maps/name).read_bytes(),data)
        self.assertEqual((save/"keep").read_bytes(),b"personal-save")
        self.assertEqual((maps/"shaders.bin").read_bytes(),self.payload)
    def test_growth_after_inspection_is_rejected(self):
        game=folder_fixture(self.root);(game/"default.xbe").write_bytes(b"XBEH"+self.payload)
        inspected=core.inspect_game(game)
        with (game/"maps/a10.map").open("ab") as f:f.write(b"changed")
        with self.assertRaisesRegex(core.InstallerError,"changed size"):core._game_data(inspected)
    def test_upgrade_failure_preserves_concurrent_replacement(self):
        game=folder_fixture(self.root);(game/"default.xbe").write_bytes(b"XBEH"+self.payload)
        sd=self.root/"sd";(sd/"switch").mkdir(parents=True);core.prepare_sd(game,sd)
        maps=sd/"switch/halo/maps";(maps/"shaders.bin").unlink();real_hash=core.sha256_file
        def replace_and_fail(path):
            if path.name=="shaders.bin":
                path.rename(self.root/"ours-backup");path.write_bytes(b"concurrent-file");return "bad"
            return real_hash(path)
        with mock.patch.object(core,"sha256_file",side_effect=replace_and_fail):
            with self.assertRaises(core.InstallerError):core.prepare_sd(game,sd)
        self.assertEqual((maps/"shaders.bin").read_bytes(),b"concurrent-file")
    def test_out_of_bounds_and_resource_limit(self):
        game=self.game(b"XBEH"+self.payload)
        for offset,size in [(100,len(self.payload)+4),(0,core.MAX_XBE_BYTES+1),(-1,len(self.payload)+4)]:
            with self.assertRaises(core.InstallerError):core._shader_bytes(core.GameInput("fixture",self.root,(),executable=core.ShaderInput(game.executable.source,offset,size)))
    def test_duplicate_xbe_case_and_symlink(self):
        game=folder_fixture(self.root);(game/"DEFAULT.XBE").write_bytes(b"x")
        if len([p for p in game.iterdir() if p.name.casefold()=="default.xbe"])>1:
            with self.assertRaises(core.InstallerError):core.inspect_game(game)
        for p in list(game.iterdir()):
            if p.name.casefold()=="default.xbe":p.unlink()
        other=self.root/"other";other.write_bytes(b"x");(game/"default.xbe").symlink_to(other)
        with self.assertRaises(core.InstallerError):core.inspect_game(game)
if __name__=="__main__":unittest.main()
