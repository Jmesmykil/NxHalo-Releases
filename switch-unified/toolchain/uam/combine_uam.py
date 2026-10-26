import json, os, subprocess, hashlib
from pathlib import Path
root=Path(__file__).resolve().parent
os.chdir(root)
os.environ["DEVKITPRO"]="/opt/devkitpro"
os.environ["DEVKITA64"]="/opt/devkitpro/devkitA64"
bin=Path("/opt/devkitpro/devkitA64/bin")
def run(args):
    subprocess.run([str(x) for x in args],check=True)
flags="-std=c++11 -DNDEBUG -DDESKTOP -D_USE_MATH_DEFINES -D_GNU_SOURCE -DHAVE_POSIX_MEMALIGN -D__SWITCH__ -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIE".split()
incs=[root/"source/source",root/"source/mesa-imported",root/"build/uam/meson/mesa-imported",root/"build/uam/meson/mesa-imported/glsl",root/"build/uam/meson/mesa-imported/glsl/glcpp"]
run([bin/"aarch64-none-elf-g++",*flags,"-isystem","/opt/devkitpro/libnx/include",*["-I"+str(p) for p in incs],"-c",root.parent/"donor/port/switch/host/host_dk_compiler.cpp","-o","host_dk_compiler.cpp.o"])
run([bin/"aarch64-none-elf-ld","-r","-o","uam_combined.o","host_dk_compiler.cpp.o","--whole-archive","build/uam/libuam.a","--no-whole-archive"])
nm=subprocess.check_output([str(bin/"aarch64-none-elf-nm"),"--defined-only","uam_combined.o"],text=True)
symbols=sorted({l.split()[2] for l in nm.splitlines() if len(l.split())==3 and l.split()[2]!="host_dk_compile_glsl" and not l.split()[2].startswith("$")})
allocs=[("malloc","host_uam_malloc"),("calloc","host_uam_calloc"),("realloc","host_uam_realloc"),("free","host_uam_free"),("strdup","host_uam_strdup"),("strndup","host_uam_strndup"),("_Znwm","host_uam_new"),("_Znam","host_uam_new_array"),("_ZdlPv","host_uam_delete"),("_ZdaPv","host_uam_delete_array")]
Path("uam_symbols.txt").write_text("".join(s+" uam_"+s+"\n" for s in symbols)+"".join(a+" "+b+"\n" for a,b in allocs))
run([bin/"aarch64-none-elf-objcopy","--redefine-syms=uam_symbols.txt","--keep-global-symbol=host_dk_compile_glsl",*["--keep-global-symbol="+b for a,b in allocs],"uam_combined.o","uam_compiler.o"])
exports=subprocess.check_output([str(bin/"aarch64-none-elf-nm"),"-g","--defined-only","uam_compiler.o"],text=True)
assert [l.split()[-1] for l in exports.splitlines()]==["host_dk_compile_glsl"],exports
Path("uam_compiler.exports.txt").write_text(exports)
undef=subprocess.check_output([str(bin/"aarch64-none-elf-nm"),"-u","uam_compiler.o"],text=True)
Path("uam_compiler.undefined.txt").write_text(undef)
for old,new in allocs:
    assert old not in {l.split()[-1] for l in undef.splitlines()},old
receipt={"upstream_commit":"5a5afc2bae8b55409ab36ba45be63fcb73f68993","patch_sha256":hashlib.sha256((root.parent/"donor/port/switch/uam.patch").read_bytes()).hexdigest(),"flags":flags,"b_staticpic":False,"archive_sha256":hashlib.sha256(Path("build/uam/libuam.a").read_bytes()).hexdigest(),"object_sha256":hashlib.sha256(Path("uam_compiler.o").read_bytes()).hexdigest(),"exports":exports,"acceptance":"AArch64 cross compile and relocatable link verified; no runtime shader compile on hardware tested"}
Path("receipt.json").write_text(json.dumps(receipt,indent=2)+"\n")
print(json.dumps(receipt,indent=2))
