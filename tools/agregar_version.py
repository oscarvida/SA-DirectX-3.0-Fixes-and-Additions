import ctypes, os, struct, subprocess, sys, glob, hashlib, pefile
from ctypes import wintypes

RC = r"C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\rc.exe"
EDITBIN = r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x86\editbin.exe"
WORK = sys.argv[2]
root = sys.argv[1]

DESC = {
    "ENBShaderCache.asi": ("ENB shader cache plugin for GTA San Andreas", "1.0.0.0"),
    "enbseries_guardia.asi": ("ENB effect creation guard for GTA San Andreas", "1.0.0.0"),
    "enbseries_interiores.asi": ("ENB interior lighting fixes for GTA San Andreas", "1.0.0.0"),
    "zzENBParametros.asi": ("ENB parameters plugin for GTA San Andreas", "1.0.0.0"),
    "HumoLucesENB.SA.asi": ("Smoke and grass lighting plugin for GTA San Andreas", "1.0.0.0"),
    "LucesNocturnasENB.SA.asi": ("Night lights plugin for GTA San Andreas (2.0k-r1)", "2.0.11.1"),
    "ShadowCasterFix.asi": ("Shadow caster visibility fix for GTA San Andreas", "1.0.0.0"),
    "RendimientoHiloPrincipal.SA.asi": ("Main thread performance plugin for GTA San Andreas (v3.1)", "3.1.0.0"),
}

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
k32.BeginUpdateResourceW.restype = wintypes.HANDLE
k32.BeginUpdateResourceW.argtypes = [wintypes.LPCWSTR, wintypes.BOOL]
k32.UpdateResourceW.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p, wintypes.WORD, ctypes.c_void_p, wintypes.DWORD]
k32.EndUpdateResourceW.argtypes = [wintypes.HANDLE, wintypes.BOOL]

def res_entries(path):
    d = open(path, "rb").read(); off = 0; out = []
    while off < len(d):
        dsize, hsize = struct.unpack_from("<II", d, off)
        p = off + 8
        def rd(p):
            if struct.unpack_from("<H", d, p)[0] == 0xFFFF:
                return struct.unpack_from("<H", d, p + 2)[0], p + 4
            e = d.index(b"\0\0", p)
            while (e - p) % 2: e = d.index(b"\0\0", e + 1)
            return d[p:e].decode("utf-16le"), e + 2
        typ, p = rd(p); name, p = rd(p)
        p = (p + 3) & ~3
        lang = struct.unpack_from("<IHHII", d, p)[2]
        data = d[off + hsize: off + hsize + dsize]
        if dsize: out.append((typ, name, lang, data))
        off = (off + hsize + dsize + 3) & ~3
    return out

def text_hash(f):
    pe = pefile.PE(f)
    r = {s.Name.rstrip(b"\0"): hashlib.sha256(s.get_data()[:s.Misc_VirtualSize]).hexdigest() for s in pe.sections if s.Name.rstrip(b"\0") in (b".text", b".rdata", b".data")}
    pe.close(); return r

for f in sorted(glob.glob(root + "/**/*.asi", recursive=True)):
    name = os.path.basename(f)
    desc, ver = DESC[name]
    vc = ver.replace(".", ",")
    rc = os.path.join(WORK, name + ".rc"); res = rc[:-3] + ".res"
    open(rc, "w", encoding="utf-8").write(f'''#pragma code_page(65001)
1 VERSIONINFO
FILEVERSION {vc}
PRODUCTVERSION 3,0,2,0
FILEFLAGSMASK 0x3fL
FILEFLAGS 0x0L
FILEOS 0x40004L
FILETYPE 0x2L
FILESUBTYPE 0x0L
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904b0"
    BEGIN
      VALUE "CompanyName", "Oscar Vidal"
      VALUE "FileDescription", "{desc}"
      VALUE "FileVersion", "{ver}"
      VALUE "InternalName", "{os.path.splitext(name)[0]}"
      VALUE "LegalCopyright", "Copyright (C) 2026 Oscar Vidal. Free GTA San Andreas mod (ASI plugin)."
      VALUE "OriginalFilename", "{name}"
      VALUE "ProductName", "SA DirectX 3.0 Fixes and Additions"
      VALUE "ProductVersion", "3.0 r2"
      VALUE "Comments", "ASI plugin loaded by an ASI Loader / Mod Loader into gta_sa.exe 1.0 US"
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x409, 1200
  END
END
''')
    subprocess.run([RC, "/nologo", "/fo", res, rc], check=True)
    before = text_hash(f)
    h = k32.BeginUpdateResourceW(f, False)
    if not h: raise ctypes.WinError(ctypes.get_last_error())
    for typ, nm, lang, data in res_entries(res):
        buf = ctypes.create_string_buffer(data, len(data))
        ok = k32.UpdateResourceW(h, ctypes.c_void_p(typ), ctypes.c_void_p(nm), lang, buf, len(data))
        if not ok: raise ctypes.WinError(ctypes.get_last_error())
    if not k32.EndUpdateResourceW(h, False): raise ctypes.WinError(ctypes.get_last_error())
    subprocess.run([EDITBIN, "/nologo", "/RELEASE", f], check=True)
    after = text_hash(f)
    pe = pefile.PE(f)
    vi = {k.decode(): v.decode() for fi in pe.FileInfo[0] if fi.Key == b"StringFileInfo" for st in fi.StringTable for k, v in st.entries.items()}
    print(name, "code/data identical:", before == after, "| checksum ok:", pe.OPTIONAL_HEADER.CheckSum == pe.generate_checksum(), "|", vi.get("FileDescription"), vi.get("FileVersion"))
    pe.close()
