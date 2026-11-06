// ENBShaderCache — cache de efectos compilados para ENBSeries 0.430 (GTA SA, SA_DirectX 3.0).
//
// PROTOTIPO. No instalado en el juego. Se prueba con tests/arnes_enb (opcion --cache).
//
// Problema que ataca (informe §13 P1): ENB compila desde HLSL sus 2 efectos internos y los 13
// del preset en cada CreateDevice y en cada Reset (39-48 s medidos; 1.fx/PS_CLOUDWORK son ~34 s).
// Su propia cache .enbc esta desactivada por codigo.
//
// Como: tras cargar enbseries.asi (VA con base 0x10000000 + desplazamiento de la base real), sustituye dos ranuras de SU tabla de
// imports:  0x100A629C D3DXCreateEffect, 0x100A62A0 D3DXCreateEffectFromFileA.
// En cada llamada calcula una clave (fuente + includes + macros + flags + version del formato).
//   - acierto: crea el efecto desde el binario fx_2_0 guardado (D3DXCreateEffect lo acepta).
//   - fallo:   compila con ID3DXEffectCompiler (el mismo compilador de d3dx9_43), guarda el
//              binario y crea el efecto desde el.
//   - cualquier error en este camino: llama a la funcion ORIGINAL con los argumentos originales.
// Opcional (OptimizacionNivel0=1): compila con D3DXSHADER_OPTIMIZATION_LEVEL0 (medido: 1.fx
// 38,5 s -> 6,1 s, PS_DX +0,5 % instrucciones). La variante forma parte de la clave.
//
// Firmas comprobadas antes de tocar nada: modulo llamado enbseries.asi (en cualquier base), thunks
// FF 25 <IAT> en 0x1003DBDA/0x1003DBE0, y las ranuras apuntando a d3dx9_43. Si algo no cuadra,
// no se modifica nada y se registra en el log.
//
// Configuracion: ENBShaderCache.ini junto a la DLL ([General] Enabled, Carpeta, OptimizacionNivel0, Log).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <vector>
extern "C" IMAGE_DOS_HEADER __ImageBase;

// ------------------------------------------------------------------ tipos minimos de D3DX
struct D3DXMACRO_ { const char* Name; const char* Definition; };
struct IBuf {   // ID3DXBuffer
    virtual HRESULT __stdcall QueryInterface(REFIID, void**) = 0; virtual ULONG __stdcall AddRef() = 0;
    virtual ULONG __stdcall Release() = 0; virtual void* __stdcall GetBufferPointer() = 0; virtual DWORD __stdcall GetBufferSize() = 0;
};
typedef HRESULT(WINAPI* PCreateEffect)(IDirect3DDevice9*, const void*, UINT, const D3DXMACRO_*, void*, DWORD, void*, void**, IBuf**);
typedef HRESULT(WINAPI* PCreateEffectFromFileA)(IDirect3DDevice9*, const char*, const D3DXMACRO_*, void*, DWORD, void*, void**, IBuf**);
typedef HRESULT(WINAPI* PCreateEffectCompiler)(const char*, UINT, const D3DXMACRO_*, void*, DWORD, void**, IBuf**);
typedef HRESULT(WINAPI* PCreateEffectCompilerFromFileA)(const char*, const D3DXMACRO_*, void*, DWORD, void**, IBuf**);
// ID3DXEffectCompiler: 57 metodos de ID3DXBaseEffect, luego SetLiteral(57), GetLiteral(58), CompileEffect(59)
typedef HRESULT(WINAPI* PCompileEffect)(void*, DWORD, IBuf**, IBuf**);

static const DWORD OPT_LEVEL0 = 0x4000;
static const uint32_t FORMATO = 1;                 // subir si cambia la forma de la clave o del archivo
static const uintptr_t IAT_CREATE = 0x100A629C, IAT_FROMFILE = 0x100A62A0;
static const uintptr_t THUNK_FROMFILE = 0x1003DBDA, THUNK_CREATE = 0x1003DBE0;
// VA con base 0x10000000 (las de analysis/); g_delta = base real de enbseries.asi - 0x10000000
static uintptr_t g_delta;

static PCreateEffect o_CreateEffect;
static PCreateEffectFromFileA o_FromFile;
static PCreateEffectCompiler p_Compiler;
static PCreateEffectCompilerFromFileA p_CompilerFromFile;

static FILE* g_log;
static std::string g_dir;           // carpeta de la DLL
static std::string g_cache;         // carpeta de cache (absoluta)
static int g_nivel0 = 0, g_activo = 1;
static LARGE_INTEGER g_freq;

static void L(const char* f, ...) {
    if (!g_log) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(g_log, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list a; va_start(a, f); vfprintf(g_log, f, a); va_end(a); fputc('\n', g_log); fflush(g_log);
}
static double ahora() { LARGE_INTEGER t; QueryPerformanceCounter(&t); return 1000.0 * t.QuadPart / g_freq.QuadPart; }

// ------------------------------------------------------------------ clave (FNV-1a 64 en dos semillas)
struct Clave {
    uint64_t a = 1469598103934665603ULL, b = 1099511628211ULL ^ 0x9E3779B97F4A7C15ULL;
    void mete(const void* p, size_t n) {
        const uint8_t* s = (const uint8_t*)p;
        for (size_t i = 0; i < n; i++) { a = (a ^ s[i]) * 1099511628211ULL; b = (b ^ s[i]) * 0x100000001B3ULL + 0x632BE59BD9B4E019ULL; }
        uint32_t len = (uint32_t)n; const uint8_t* q = (const uint8_t*)&len;   // separador con longitud
        for (int i = 0; i < 4; i++) { a = (a ^ q[i]) * 1099511628211ULL; b = (b ^ q[i]) * 0x100000001B3ULL; }
    }
    void mete(const std::string& s) { mete(s.data(), s.size()); }
    std::string hex() const { char t[40]; snprintf(t, sizeof t, "%016llX%016llX", (unsigned long long)a, (unsigned long long)b); return t; }
};

static bool lee(const std::string& ruta, std::string& out) {
    HANDLE h = CreateFileA(ruta.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD n = GetFileSize(h, 0), r = 0; out.resize(n);
    BOOL ok = n == 0 || ReadFile(h, &out[0], n, &r, 0);
    CloseHandle(h); return ok && r == n;
}

static std::string carpeta_de(const std::string& p) { size_t k = p.find_last_of("\\/"); return k == std::string::npos ? "" : p.substr(0, k + 1); }

// Mete en la clave el contenido de cada #include "x" (recursivo, relativo al archivo que lo incluye,
// que es como resuelve d3dx9 sin ID3DXInclude). Las lineas comentadas con // se ignoran.
static void mete_includes(Clave& k, const std::string& texto, const std::string& base, int prof) {
    if (prof > 8) return;
    size_t i = 0;
    while ((i = texto.find("#include", i)) != std::string::npos) {
        size_t ini = texto.rfind('\n', i); ini = ini == std::string::npos ? 0 : ini + 1;
        std::string previo = texto.substr(ini, i - ini);
        bool comentado = previo.find("//") != std::string::npos;
        size_t q1 = texto.find('"', i), fin = texto.find('\n', i);
        size_t q2 = q1 == std::string::npos ? q1 : texto.find('"', q1 + 1);
        i += 8;
        if (comentado || q1 == std::string::npos || q2 == std::string::npos || (fin != std::string::npos && q1 > fin)) continue;
        std::string nombre = texto.substr(q1 + 1, q2 - q1 - 1), cont;
        k.mete(nombre);
        if (lee(base + nombre, cont)) { k.mete(cont); mete_includes(k, cont, carpeta_de(base + nombre), prof + 1); }
        else k.mete(std::string("<no existe>"));
    }
}

static void mete_macros(Clave& k, const D3DXMACRO_* m, std::string* txt) {
    for (int i = 0; m && m[i].Name && i < 256; i++) {
        k.mete(m[i].Name); k.mete(m[i].Definition ? m[i].Definition : "");
        if (txt) { *txt += m[i].Name; *txt += "="; *txt += m[i].Definition ? m[i].Definition : ""; *txt += " "; }
    }
}

// ------------------------------------------------------------------ archivo de cache
#pragma pack(push, 1)
struct Cabecera { char magia[8]; uint32_t formato, flags, tam; uint64_t a, b; };
#pragma pack(pop)

static bool cache_lee(const std::string& clave, const Clave& k, std::string& bin) {
    std::string raw;
    if (!lee(g_cache + clave + ".fxo", raw) || raw.size() < sizeof(Cabecera)) return false;
    const Cabecera* c = (const Cabecera*)raw.data();
    if (memcmp(c->magia, "ENBFXC\0\0", 8) || c->formato != FORMATO || c->a != k.a || c->b != k.b ||
        c->tam != raw.size() - sizeof(Cabecera)) return false;
    bin.assign(raw.data() + sizeof(Cabecera), c->tam); return true;
}

static void cache_escribe(const std::string& clave, const Clave& k, DWORD flags, const void* p, DWORD n) {
    Cabecera c = {}; memcpy(c.magia, "ENBFXC\0\0", 8); c.formato = FORMATO; c.flags = flags; c.tam = n; c.a = k.a; c.b = k.b;
    std::string tmp = g_cache + clave + ".tmp", fin = g_cache + clave + ".fxo";
    HANDLE h = CreateFileA(tmp.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (h == INVALID_HANDLE_VALUE) { L("  no se pudo escribir %s (%lu)", tmp.c_str(), GetLastError()); return; }
    DWORD w1 = 0, w2 = 0; WriteFile(h, &c, sizeof c, &w1, 0); WriteFile(h, p, n, &w2, 0); CloseHandle(h);
    if (w1 == sizeof c && w2 == n) MoveFileExA(tmp.c_str(), fin.c_str(), MOVEFILE_REPLACE_EXISTING);
    else DeleteFileA(tmp.c_str());
}

// Crea el efecto desde el binario y, si falla, devuelve el HRESULT para que el llamador caiga al original.
static HRESULT desde_binario(IDirect3DDevice9* dev, const std::string& bin, DWORD flags, void* pool, void** out, IBuf** err) {
    return o_CreateEffect(dev, bin.data(), (UINT)bin.size(), nullptr, nullptr, flags & ~OPT_LEVEL0, pool, out, err);
}

static bool compila(void* comp, DWORD flags, std::string& bin, IBuf** err, const char* nombre) {
    IBuf* fx = nullptr; IBuf* e = nullptr;
    HRESULT hr = ((PCompileEffect)(*(void***)comp)[59])(comp, flags, &fx, &e);
    if (FAILED(hr) || !fx) {
        L("  CompileEffect %s fallo 0x%08X: %.300s", nombre, hr, e ? (char*)e->GetBufferPointer() : "");
        if (err && e) { *err = e; e = nullptr; }
        if (e) e->Release(); if (fx) fx->Release(); return false;
    }
    bin.assign((const char*)fx->GetBufferPointer(), fx->GetBufferSize()); fx->Release(); if (e) e->Release();
    return true;
}

// ------------------------------------------------------------------ ganchos
static HRESULT WINAPI h_CreateEffect(IDirect3DDevice9* dev, const void* src, UINT len, const D3DXMACRO_* mac, void* inc,
                                     DWORD flags, void* pool, void** out, IBuf** err) {
    double t0 = ahora();
    DWORD fc = g_nivel0 ? (flags | OPT_LEVEL0) : flags;
    if (!g_activo || !src || inc) return o_CreateEffect(dev, src, len, mac, inc, flags, pool, out, err);
    Clave k; std::string mtxt;
    k.mete(&FORMATO, 4); k.mete(std::string("mem")); k.mete(&fc, 4); k.mete(src, len); mete_macros(k, mac, &mtxt);
    std::string clave = "mem_" + k.hex(), bin;
    if (cache_lee(clave, k, bin)) {
        HRESULT hr = desde_binario(dev, bin, flags, pool, out, err);
        L("ACIERTO memoria (%u bytes) flags=0x%X -> 0x%08X en %.1f ms", len, flags, hr, ahora() - t0);
        if (SUCCEEDED(hr)) return hr;
    }
    void* comp = nullptr; IBuf* pe = nullptr;
    HRESULT hr = p_Compiler((const char*)src, len, mac, nullptr, fc, &comp, &pe);
    if (SUCCEEDED(hr) && comp && compila(comp, fc, bin, nullptr, "memoria")) {
        ((IUnknown*)comp)->Release(); if (pe) pe->Release();
        cache_escribe(clave, k, fc, bin.data(), (DWORD)bin.size());
        hr = desde_binario(dev, bin, flags, pool, out, err);
        L("FALLO->COMPILADO memoria (%u bytes, macros: %s) flags=0x%X -> 0x%08X en %.1f ms, binario %u bytes",
          len, mtxt.c_str(), fc, hr, ahora() - t0, (unsigned)bin.size());
        if (SUCCEEDED(hr)) return hr;
    } else {
        L("  compilador de efecto (memoria) fallo 0x%08X: %.300s", hr, pe ? (char*)pe->GetBufferPointer() : "");
        if (comp) ((IUnknown*)comp)->Release(); if (pe) pe->Release();
    }
    L("  -> se usa la funcion original");
    return o_CreateEffect(dev, src, len, mac, inc, flags, pool, out, err);
}

static HRESULT WINAPI h_FromFile(IDirect3DDevice9* dev, const char* file, const D3DXMACRO_* mac, void* inc, DWORD flags,
                                 void* pool, void** out, IBuf** err) {
    double t0 = ahora();
    DWORD fc = g_nivel0 ? (flags | OPT_LEVEL0) : flags;
    std::string texto;
    if (!g_activo || !file || inc || !lee(file, texto)) return o_FromFile(dev, file, mac, inc, flags, pool, out, err);
    Clave k;
    k.mete(&FORMATO, 4); k.mete(std::string("file")); k.mete(std::string(file)); k.mete(&fc, 4); k.mete(texto);
    mete_includes(k, texto, carpeta_de(file), 0); mete_macros(k, mac, nullptr);
    std::string base = file; for (char& c : base) if (c == '\\' || c == '/' || c == ':') c = '_';
    std::string clave = base + "_" + k.hex(), bin;
    if (cache_lee(clave, k, bin)) {
        HRESULT hr = desde_binario(dev, bin, flags, pool, out, err);
        L("ACIERTO %s flags=0x%X -> 0x%08X en %.1f ms", file, flags, hr, ahora() - t0);
        if (SUCCEEDED(hr)) return hr;
    }
    void* comp = nullptr; IBuf* pe = nullptr;
    HRESULT hr = p_CompilerFromFile(file, mac, nullptr, fc, &comp, &pe);
    if (SUCCEEDED(hr) && comp && compila(comp, fc, bin, nullptr, file)) {
        ((IUnknown*)comp)->Release(); if (pe) pe->Release();
        cache_escribe(clave, k, fc, bin.data(), (DWORD)bin.size());
        hr = desde_binario(dev, bin, flags, pool, out, err);
        L("FALLO->COMPILADO %s flags=0x%X -> 0x%08X en %.1f ms, binario %u bytes", file, fc, hr, ahora() - t0, (unsigned)bin.size());
        if (SUCCEEDED(hr)) return hr;
    } else {
        if (FAILED(hr)) L("  compilador de efecto %s fallo 0x%08X: %.300s", file, hr, pe ? (char*)pe->GetBufferPointer() : "");
        if (comp) ((IUnknown*)comp)->Release(); if (pe) pe->Release();
    }
    // si el archivo no compila (o no existe en esta ruta) el original da el mismo error que siempre,
    // y ENB lo copia a su buffer de errores (0x1019B5F0) y prueba la ruta de la raiz
    return o_FromFile(dev, file, mac, inc, flags, pool, out, err);
}

// ------------------------------------------------------------------ instalacion
static bool ranura(uintptr_t iat, uintptr_t thunk, void* esperado, void* nuevo, void** orig) {
    iat += g_delta; thunk += g_delta;
    const uint8_t* t = (const uint8_t*)thunk;
    if (IsBadReadPtr((void*)thunk, 6) || t[0] != 0xFF || t[1] != 0x25 || *(uintptr_t*)(t + 2) != iat) {
        L("FIRMA: thunk %08X no es 'jmp [%08X]'", (unsigned)thunk, (unsigned)iat); return false;
    }
    void* actual = *(void**)iat;
    if (actual != esperado) {
        HMODULE m = nullptr; char n[MAX_PATH] = "?";
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)actual, &m)) GetModuleFileNameA(m, n, MAX_PATH);
        L("FIRMA: la ranura %08X apunta a %p (%s), no a d3dx9_43; otro modulo ya la engancha: no se toca", (unsigned)iat, actual, n);
        return false;
    }
    DWORD old; VirtualProtect((void*)iat, 4, PAGE_READWRITE, &old);
    *orig = actual; *(void**)iat = nuevo;
    VirtualProtect((void*)iat, 4, old, &old);
    return true;
}

static void instala() {
    QueryPerformanceFrequency(&g_freq);
    char ruta[MAX_PATH]; GetModuleFileNameA((HMODULE)&__ImageBase, ruta, MAX_PATH);
    g_dir = carpeta_de(ruta);
    std::string ini = g_dir + "ENBShaderCache.ini";
    g_activo = GetPrivateProfileIntA("General", "Enabled", 1, ini.c_str());
    g_nivel0 = GetPrivateProfileIntA("General", "OptimizacionNivel0", 0, ini.c_str());
    char c[MAX_PATH]; GetPrivateProfileStringA("General", "Carpeta", "enbseries\\cache_efectos", c, MAX_PATH, ini.c_str());
    if (GetPrivateProfileIntA("General", "Log", 1, ini.c_str())) g_log = fopen((g_dir + "ENBShaderCache.log").c_str(), "w");
    char cwd[MAX_PATH]; GetCurrentDirectoryA(MAX_PATH, cwd);
    g_cache = (strchr(c, ':') ? std::string(c) : std::string(cwd) + "\\" + c); if (g_cache.back() != '\\') g_cache += '\\';
    L("ENBShaderCache prototipo formato %u. Enabled=%d OptimizacionNivel0=%d cache=%s", FORMATO, g_activo, g_nivel0, g_cache.c_str());
    if (!g_activo) return;

    // En el juego ENB nunca carga en 0x10000000 (vorbisFile.dll, el ASI Loader, ya ocupa esa base): el
    // stub de UPX lo reubica entero, operandos de los thunks incluidos. Las VA se desplazan por la base real.
    HMODULE enb = GetModuleHandleA("enbseries.asi");
    if (!enb) { L("FIRMA: enbseries.asi no esta cargado: no se hace nada"); return; }
    g_delta = (uintptr_t)enb - 0x10000000u;
    L("enbseries.asi en %p (desplazamiento 0x%08X respecto a 0x10000000)", enb, (unsigned)g_delta);
    HMODULE dx = GetModuleHandleA("d3dx9_43.dll");
    if (!dx) { L("FIRMA: d3dx9_43.dll no cargada"); return; }
    p_Compiler = (PCreateEffectCompiler)GetProcAddress(dx, "D3DXCreateEffectCompiler");
    p_CompilerFromFile = (PCreateEffectCompilerFromFileA)GetProcAddress(dx, "D3DXCreateEffectCompilerFromFileA");
    void* ce = GetProcAddress(dx, "D3DXCreateEffect"), * cf = GetProcAddress(dx, "D3DXCreateEffectFromFileA");
    if (!p_Compiler || !p_CompilerFromFile || !ce || !cf) { L("FIRMA: faltan exports de d3dx9_43"); return; }
    CreateDirectoryA(g_cache.c_str(), nullptr);
    bool a = ranura(IAT_CREATE, THUNK_CREATE, ce, (void*)h_CreateEffect, (void**)&o_CreateEffect);
    bool b = ranura(IAT_FROMFILE, THUNK_FROMFILE, cf, (void*)h_FromFile, (void**)&o_FromFile);
    L("Ranuras de la IAT de ENB: D3DXCreateEffect %s, D3DXCreateEffectFromFileA %s", a ? "enganchada" : "SIN TOCAR", b ? "enganchada" : "SIN TOCAR");
    if (!a) o_CreateEffect = (PCreateEffect)ce;
    if (!b) o_FromFile = (PCreateEffectFromFileA)cf;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID) {
    if (r == DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(h); instala(); }
    if (r == DLL_PROCESS_DETACH && g_log) { L("descarga"); fclose(g_log); g_log = nullptr; }
    return TRUE;
}
