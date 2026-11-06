// RendimientoHiloPrincipal.SA.asi — optimizaciones EQUIVALENTES del hilo principal de GTA SA (rama de rendimiento
// ENB/Complex Shadows, 2026-10-01). Proyecto: D:\proyectos gta-sa\enbseries-ingenieria-inversa\prototipos\ENBShadowPerformance
//
// Medido con enbseries_rendimiento.asi v3 en la principal (sesion 3, 01-10): el hilo principal esta ocupado el ~94 %
// del fotograma y la GPU espera. Parte de ese tiempo son llamadas al sistema repetidas cada fotograma que no cambian
// nada. Este plugin NO toca archivos ni el codigo de ningun mod: solo sustituye entradas de la IAT de los modulos
// indicados en el .ini por funciones que dan el MISMO resultado sin repetir la llamada al sistema:
//
//  1. DSOUND.dll -> RegOpenKeyExA(HKLM, "SOFTWARE\Microsoft\APL"): lo llama IsAPLSafeMode() en cada
//     CMultiPan3dObject::DistributeSignal (~46 veces por fotograma) sin guardar el resultado. Si la clave no existe,
//     se recuerda el error durante CacheAPLms ms (despues se vuelve a preguntar al registro).
//  2. VirtualProtect sobre el codigo de gta_sa.exe desde los mods de la lista (wantedleveleditor ~211/fotograma,
//     CLEO, CLEO+...): cada pagina se abre una vez como PAGE_EXECUTE_READWRITE y se informa de que ya lo estaba;
//     las peticiones siguientes (abrir/restaurar) no hacen llamada al sistema. Si otro modulo vuelve a proteger la
//     pagina, un manejador vectorial la reabre ante la primera escritura (solo paginas abiertas por este plugin).
//  3. GetKeyState desde los mods de la lista (CLEO+ consulta ~255 teclas por fotograma): una foto de
//     GetKeyboardState por hilo sirve a todas las consultas hechas dentro de VentanaTeclasUs microsegundos.
//     GetKeyState y GetKeyboardState leen el mismo estado de la cola del hilo; el resultado es identico.
//
//  4. (v2, tirones) GetFileAttributesA/W desde Mod Loader (std.asi.dll). Cuando un script CLEO pregunta por un archivo
//     que no esta en la carpeta del juego (0AAB, abrir...), Mod Loader prueba la ruta en la carpeta de CADA script CLEO
//     y CADA ASI registrado (~130 consultas al disco por pregunta). Myths_Maker hace varias de esas preguntas por cada
//     animal (random_%i.mp3, Anim%i.cs...): fotogramas de 50-160 ms (sesion 5, instrumento v5). Se recuerda "no existe"
//     SOLO para rutas dentro de modloader\, y la cache entera se invalida en cuanto el proceso crea un archivo o
//     directorio (NtCreateFile con FILE_CREATED) o renombra/enlaza uno (NtSetInformationFile), y a los CacheArchivosMs.
//     (v3: tambien al borrar, NtSetInformationFile Disposition o FILE_DELETE_ON_CLOSE.)
//     Sin esos dos ganchos de invalidacion la cache no se usa.
//     v3 (sesion 7: los tirones que quedaban eran rutas pedidas por PRIMERA vez): Mod Loader arma la ruta como
//     <carpeta del script>\cleo\Animals\Anim%i\..., y en casi todas las carpetas ya "cleo\Animals" no existe. Cuando el
//     disco contesta ERROR_PATH_NOT_FOUND, se sube por la ruta (1-3 consultas, una vez) hasta la carpeta mas alta que
//     no existe y se recuerda: cualquier ruta que cuelgue de ella recibe ERROR_PATH_NOT_FOUND sin ir al disco, aunque
//     nunca se haya pedido. Las claves son la ruta completa normalizada (GetFullPathNameW, igual que hace el sistema
//     antes de ir al disco). Misma invalidacion; CacheArchivosMs pasa a 10 min (solo protege de cambios hechos por
//     OTROS procesos, que los ganchos no ven). CacheDirectorios=0 apaga solo esta parte.
//
// Interruptor exportado RHP_Activo (el instrumento lo usa para el A/B en caliente). Activo=0 en el .ini lo apaga.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include "RutaHash.h"
extern "C" IMAGE_DOS_HEADER __ImageBase;

extern "C" __declspec(dllexport) volatile LONG RHP_Activo = 1;
static FILE* g_log; static LARGE_INTEGER g_f, g_t0;
static double ms() { LARGE_INTEGER t; QueryPerformanceCounter(&t); return 1000.0 * (t.QuadPart - g_t0.QuadPart) / g_f.QuadPart; }
static CRITICAL_SECTION g_csLog;
static void L(const char* f, ...) {
    if (!g_log) return;
    EnterCriticalSection(&g_csLog);
    SYSTEMTIME st; GetLocalTime(&st);
    fprintf(g_log, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    va_list a; va_start(a, f); vfprintf(g_log, f, a); va_end(a); fputc('\n', g_log); fflush(g_log);
    LeaveCriticalSection(&g_csLog);
}

// ---------------------------------------------------------------- configuracion
static int g_usarAPL = 1, g_usarVP = 1, g_usarTeclas = 1, g_cacheAPLms = 5000, g_ventanaTeclasUs = 1000, g_resumenSeg = 60;
static std::vector<std::string> g_modsVP, g_modsTeclas, g_modsAPL;
static void lista(const char* ini, const char* clave, const char* def, std::vector<std::string>& v) {
    char b[2048]; GetPrivateProfileStringA("Rendimiento", clave, def, b, sizeof b, ini);
    std::string s = b; size_t i = 0;
    while (i <= s.size()) { size_t j = s.find(',', i); if (j == std::string::npos) j = s.size();
        std::string t = s.substr(i, j - i); while (!t.empty() && t.front() == ' ') t.erase(0, 1); while (!t.empty() && t.back() == ' ') t.pop_back();
        if (!t.empty()) v.push_back(t); i = j + 1; }
}

// ---------------------------------------------------------------- contadores
static volatile LONG c_aplReal, c_aplCache, c_vpReal, c_vpElidida, c_vpPasa, c_vehReabre, c_tecFoto, c_tecServida, c_tecPasa;

// ---------------------------------------------------------------- 1. RegOpenKeyExA de DSOUND (IsAPLSafeMode)
typedef LSTATUS(WINAPI* PROKA)(HKEY, LPCSTR, DWORD, REGSAM, PHKEY);
static PROKA o_ROKA;
static volatile LONG g_aplError = 0; static volatile double g_aplHasta = 0;
static LSTATUS WINAPI s_ROKA(HKEY k, LPCSTR sub, DWORD op, REGSAM sam, PHKEY out) {
    if (RHP_Activo && g_usarAPL && k == HKEY_LOCAL_MACHINE && sub && _stricmp(sub, "SOFTWARE\\Microsoft\\APL") == 0) {
        LONG e = g_aplError;
        if (e && ms() < g_aplHasta) { InterlockedIncrement(&c_aplCache); if (out) *out = nullptr; return e; }
        LSTATUS r = o_ROKA(k, sub, op, sam, out); InterlockedIncrement(&c_aplReal);
        if (r != ERROR_SUCCESS) { g_aplHasta = ms() + g_cacheAPLms; g_aplError = r; } else g_aplError = 0;
        return r;
    }
    return o_ROKA(k, sub, op, sam, out);
}

// ---------------------------------------------------------------- 2. VirtualProtect sobre gta_sa.exe
typedef BOOL(WINAPI* PVP)(LPVOID, SIZE_T, DWORD, PDWORD);
static PVP o_VP;
static uintptr_t g_exeIni, g_exeFin; static volatile uint8_t* g_abierta;   // 1 byte por pagina de la imagen de gta_sa.exe
static int es_escritura(DWORD p) { p &= 0xFF; return p == PAGE_EXECUTE_READWRITE || p == PAGE_READWRITE || p == PAGE_EXECUTE_WRITECOPY || p == PAGE_WRITECOPY; }
static int es_restaurar(DWORD p) { p &= 0xFF; return p == PAGE_EXECUTE_READ || p == PAGE_READONLY || p == PAGE_EXECUTE || es_escritura(p); }
static BOOL WINAPI s_VP(LPVOID a, SIZE_T n, DWORD prot, PDWORD old) {
    uintptr_t ini = (uintptr_t)a, fin = ini + n;
    if (!RHP_Activo || !g_usarVP || !g_abierta || !old || n == 0 || ini < g_exeIni || fin > g_exeFin || (prot & (PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE)) || !es_restaurar(prot)) {
        InterlockedIncrement(&c_vpPasa); return o_VP(a, n, prot, old);
    }
    size_t p0 = (ini - g_exeIni) >> 12, p1 = (fin - 1 - g_exeIni) >> 12; int todas = 1;
    for (size_t p = p0; p <= p1; p++) if (!g_abierta[p]) { todas = 0; break; }
    if (todas) { *old = PAGE_EXECUTE_READWRITE; InterlockedIncrement(&c_vpElidida); return TRUE; }
    if (!es_escritura(prot)) { InterlockedIncrement(&c_vpPasa); return o_VP(a, n, prot, old); }   // una restauracion de una pagina que no abrimos
    DWORD real = 0; BOOL ok = o_VP(a, n, PAGE_EXECUTE_READWRITE, &real); InterlockedIncrement(&c_vpReal);
    if (!ok) return o_VP(a, n, prot, old);
    for (size_t p = p0; p <= p1; p++) g_abierta[p] = 1;
    *old = PAGE_EXECUTE_READWRITE;   // asi la restauracion del llamador tambien se omite
    return TRUE;
}
// Si otro modulo vuelve a proteger una pagina abierta por este plugin y un mod de la lista escribe en ella: reabrirla.
static LONG CALLBACK veh(EXCEPTION_POINTERS* e) {
    const EXCEPTION_RECORD* r = e->ExceptionRecord;
    if (r->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || r->NumberParameters < 2 || r->ExceptionInformation[0] != 1) return EXCEPTION_CONTINUE_SEARCH;
    uintptr_t a = r->ExceptionInformation[1];
    if (!g_abierta || a < g_exeIni || a >= g_exeFin || !g_abierta[(a - g_exeIni) >> 12]) return EXCEPTION_CONTINUE_SEARCH;
    DWORD o; if (!o_VP((void*)(a & ~0xFFFu), 0x1000, PAGE_EXECUTE_READWRITE, &o)) return EXCEPTION_CONTINUE_SEARCH;
    InterlockedIncrement(&c_vehReabre);
    return EXCEPTION_CONTINUE_EXECUTION;
}

// ---------------------------------------------------------------- 3. GetKeyState
typedef SHORT(WINAPI* PGKS)(int);
typedef BOOL(WINAPI* PGKBS)(PBYTE);
static PGKS o_GKS; static PGKBS o_GKBS;
struct FotoTeclas { DWORD hilo; double t; BYTE k[256]; };
static FotoTeclas g_foto[4]; static volatile LONG g_fotoSig = 0;
static SHORT WINAPI s_GKS(int vk) {
    if (!RHP_Activo || !g_usarTeclas || vk < 0 || vk > 255) { InterlockedIncrement(&c_tecPasa); return o_GKS(vk); }
    DWORD h = GetCurrentThreadId(); double t = ms(); FotoTeclas* f = nullptr;
    for (auto& x : g_foto) if (x.hilo == h) { f = &x; break; }
    if (!f) { f = &g_foto[InterlockedIncrement(&g_fotoSig) & 3]; f->hilo = h; f->t = -1e9; }
    if ((t - f->t) * 1000.0 > g_ventanaTeclasUs) {
        if (!o_GKBS(f->k)) { InterlockedIncrement(&c_tecPasa); return o_GKS(vk); }
        f->t = t; InterlockedIncrement(&c_tecFoto);
    }
    InterlockedIncrement(&c_tecServida);
    return (SHORT)(signed char)(f->k[vk] & 0x81);   // bit alto = pulsada, bit bajo = conmutada (igual que GetKeyState)
}

// ---------------------------------------------------------------- 4. GetFileAttributesA/W de Mod Loader
static int g_usarArch = 1, g_usarDirs = 1, g_cacheArchMs = 600000, g_ganchosArch = 0;
static std::vector<std::string> g_modsArch;
static volatile LONG c_archReal, c_archCache, c_archGuardadas, c_archInval, c_archVaciados, c_dirCache, c_dirGuardados, c_dirSondeos;
static volatile LONG g_genArch = 1;   // sube con cada archivo creado o renombrado en el proceso
struct NoExiste { DWORD err; LONG gen; double t; };
// One hash/equality for owning keys and non-owning prefix views; no allocations in lookup.

using CacheRutas=std::unordered_map<std::wstring,NoExiste,RutaHash,std::equal_to<>>;
static CacheRutas* g_noExiste; static CacheRutas* g_dirNo;
static SRWLOCK g_srwArch = SRWLOCK_INIT;
static const size_t MAX_NO_EXISTE = 100000, MAX_DIR_NO = 20000;
typedef DWORD(WINAPI* PGFAA)(LPCSTR); typedef DWORD(WINAPI* PGFAW)(LPCWSTR);
static PGFAA o_GFAA; static PGFAW o_GFAW; static PGFAW p_GFAWsis;   // p_GFAWsis: la de kernelbase, para subir por la ruta
// Clave: ruta COMPLETA normalizada como la normaliza el sistema (GetFullPathNameW: '/', '.', '..', separadores dobles,
// puntos y espacios finales), en minusculas. Devuelve 0 si no esta dentro de modloader\ (esas no se recuerdan); si no,
// la posicion donde empieza el nombre de la carpeta del mod (las carpetas recordadas siempre estan por debajo).
// *dirs = 0 si la ruta tiene algo raro para razonar por carpetas (dispositivo, \\?\, ADS, comodines).
static size_t clave_arch(const wchar_t* s, std::wstring& k, int* dirs) {
    *dirs = 0;
    size_t n0 = wcsnlen(s, 1024); if (n0 == 0 || n0 >= 1024) return 0;
    if ((s[0] == L'\\' || s[0] == L'/') && (s[1] == L'\\' || s[1] == L'/')) return 0;   // UNC, \\?\, \\.\: tal cual al sistema
    wchar_t full[1024]; DWORD n = GetFullPathNameW(s, 1024, full, nullptr); if (n == 0 || n >= 1024) return 0;
    k.resize(n); for (DWORD i = 0; i < n; i++) k[i] = (wchar_t)towlower(full[i]);
    size_t m = k.find(L"\\modloader\\"); if (m == std::wstring::npos) return 0;
    size_t ini = m + 11; if (ini >= n) return 0;
    *dirs = k.find_first_of(L"*?\"<>|", 0) == std::wstring::npos && k.find(L':', 2) == std::wstring::npos && k.find(L"\\\\") == std::wstring::npos && k.back() != L'\\';
    return ini;
}
static int vigente(const NoExiste& e, double t) { return e.gen == g_genArch && t - e.t < g_cacheArchMs; }
// v3: tras un ERROR_PATH_NOT_FOUND la carpeta que contiene la ruta no existe. Se sube con la funcion del sistema hasta la
// mas alta que no existe (la que da ERROR_FILE_NOT_FOUND: su padre si existe) sin salir de modloader\<mod>, y se recuerda.
static void recuerda_dir(const std::wstring& k, size_t ini, LONG gen) {
    size_t c = k.rfind(L'\\'); if (c == std::wstring::npos || c <= ini) return;
    std::wstring d = k.substr(0, c);   // no existe: lo prueba el ERROR_PATH_NOT_FOUND de la ruta
    for (int nivel = 0; nivel < 8; nivel++) {
        size_t pc = d.rfind(L'\\'); if (pc == std::wstring::npos || pc < ini) break;   // su padre seria modloader\: no subir mas
        DWORD r = p_GFAWsis(d.c_str()); DWORD e = GetLastError(); InterlockedIncrement(&c_dirSondeos);
        if (r != INVALID_FILE_ATTRIBUTES) return;            // existe (p. ej. un ARCHIVO usado como carpeta): no recordar nada
        if (e != ERROR_PATH_NOT_FOUND) break;                // ERROR_FILE_NOT_FOUND: d es la mas alta que falta
        d.resize(pc);                                        // tampoco existe su padre: subir
    }
    AcquireSRWLockExclusive(&g_srwArch);
    if (g_dirNo->size() >= MAX_DIR_NO) { g_dirNo->clear(); InterlockedIncrement(&c_archVaciados); }
    (*g_dirNo)[d] = NoExiste{ ERROR_PATH_NOT_FOUND, gen, ms() };
    ReleaseSRWLockExclusive(&g_srwArch);
    InterlockedIncrement(&c_dirGuardados);
}
static DWORD consulta_arch(const std::wstring& k, size_t ini, int dirs, DWORD(*real)(const void*), const void* arg) {
    if (ini) {
        double t = ms(); int vale = 0; DWORD err = 0; int deDir = 0;
        AcquireSRWLockShared(&g_srwArch);
        auto it = g_noExiste->find(k);
        if (it != g_noExiste->end() && vigente(it->second, t)) { vale = 1; err = it->second.err; }
        else if (dirs && g_usarDirs && !g_dirNo->empty()) {
            // alguna carpeta de la ruta (estrictamente por encima del ultimo componente) recordada como inexistente
            const std::wstring_view vista(k);
            for (size_t p = k.find(L'\\', ini); p != std::wstring::npos && p + 1 < k.size(); p = k.find(L'\\', p + 1)) {
                auto d = g_dirNo->find(vista.substr(0,p));
                if (d != g_dirNo->end() && vigente(d->second, t)) { vale = 1; deDir = 1; err = ERROR_PATH_NOT_FOUND; break; }
            }
        }
        ReleaseSRWLockShared(&g_srwArch);
        if (vale) { InterlockedIncrement(deDir ? &c_dirCache : &c_archCache); SetLastError(err); return INVALID_FILE_ATTRIBUTES; }
    }
    LONG gen = g_genArch;   // antes de preguntar: si mientras tanto se crea algo, la entrada nace caducada
    DWORD r = real(arg); DWORD err = GetLastError(); InterlockedIncrement(&c_archReal);
    if (ini && r == INVALID_FILE_ATTRIBUTES && (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)) {
        AcquireSRWLockExclusive(&g_srwArch);
        if (g_noExiste->size() >= MAX_NO_EXISTE) { g_noExiste->clear(); InterlockedIncrement(&c_archVaciados); }
        (*g_noExiste)[k] = NoExiste{ err, gen, ms() };
        ReleaseSRWLockExclusive(&g_srwArch);
        InterlockedIncrement(&c_archGuardadas);
        if (err == ERROR_PATH_NOT_FOUND && dirs && g_usarDirs && p_GFAWsis) recuerda_dir(k, ini, gen);
    }
    SetLastError(err);
    return r;
}
// Para el arnes (prueba_rhp.exe)
extern "C" __declspec(dllexport) LONG RHP_DirCache() { return c_dirCache; }
extern "C" __declspec(dllexport) LONG RHP_DirSondeos() { return c_dirSondeos; }
static DWORD real_A(const void* p) { return o_GFAA((LPCSTR)p); }
static DWORD real_W(const void* p) { return o_GFAW((LPCWSTR)p); }
static DWORD WINAPI s_GFAA(LPCSTR p) {
    if (!RHP_Activo || !g_usarArch || !g_ganchosArch || !p) return o_GFAA(p);
    wchar_t w[1024]; if (!MultiByteToWideChar(CP_ACP, 0, p, -1, w, 1024)) return o_GFAA(p);
    std::wstring k; int dirs; size_t ini = clave_arch(w, k, &dirs);
    return consulta_arch(k, ini, dirs, real_A, p);
}
static DWORD WINAPI s_GFAW(LPCWSTR p) {
    if (!RHP_Activo || !g_usarArch || !g_ganchosArch || !p) return o_GFAW(p);
    std::wstring k; int dirs; size_t ini = clave_arch(p, k, &dirs);
    return consulta_arch(k, ini, dirs, real_W, p);
}
// Invalidacion: ganchos DESPUES de la llamada en los stubs WoW64 de ntdll ("B8 imm32" -> jmp a la funcion propia, que
// llama al original por un trampolin "mov eax, imm32; jmp stub+5"; el "ret N" del stub vuelve a la funcion propia).
typedef struct { union { LONG Status; PVOID Pointer; }; ULONG_PTR Information; } IOSB;
typedef LONG(NTAPI* PNtCreateFile)(PHANDLE, ACCESS_MASK, PVOID, IOSB*, PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
typedef LONG(NTAPI* PNtSetInformationFile)(HANDLE, IOSB*, PVOID, ULONG, ULONG);
static PNtCreateFile t_NtCreateFile; static PNtSetInformationFile t_NtSetInformationFile;
static LONG NTAPI h_NtCreateFile(PHANDLE h, ACCESS_MASK a, PVOID oa, IOSB* io, PLARGE_INTEGER sz, ULONG at, ULONG sh, ULONG disp, ULONG op, PVOID ea, ULONG eal) {
    LONG s = t_NtCreateFile(h, a, oa, io, sz, at, sh, disp, op, ea, eal);
    if (s >= 0 && ((disp != 1 /*FILE_OPEN*/ && disp != 4 /*FILE_OVERWRITE*/ && io && io->Information == 2 /*FILE_CREATED*/) || (op & 0x1000 /*FILE_DELETE_ON_CLOSE*/))) { InterlockedIncrement(&g_genArch); InterlockedIncrement(&c_archInval); }
    return s;
}
static LONG NTAPI h_NtSetInformationFile(HANDLE h, IOSB* io, PVOID info, ULONG len, ULONG cls) {
    LONG s = t_NtSetInformationFile(h, io, info, len, cls);
    // Rename/Link(Ex); v3 tambien Disposition(Ex) = borrar: no hace existir nada, pero al borrar una CARPETA las rutas de
    // dentro pasan de ERROR_FILE_NOT_FOUND a ERROR_PATH_NOT_FOUND (lo encontro el diferencial del arnes)
    if (s >= 0 && (cls == 10 || cls == 11 || cls == 65 || cls == 72 || cls == 13 || cls == 64)) { InterlockedIncrement(&g_genArch); InterlockedIncrement(&c_archInval); }
    return s;
}
static uint8_t* g_tramp;
static const char* engancha_stub(const char* fn, void* propio, void** trampolin, int k) {
    HMODULE nt = GetModuleHandleA("ntdll.dll"); uint8_t* f = nt ? (uint8_t*)GetProcAddress(nt, fn) : nullptr; if (!f) return "no encontrada";
    if (f[0] != 0xB8) return "sin mov eax (ya enganchada por otro)";
    if (((uintptr_t)f & 7) > 3) return "no alineada";
    uint8_t* t = g_tramp + k * 16;
    DWORD o; VirtualProtect(g_tramp, 4096, PAGE_EXECUTE_READWRITE, &o);
    t[0] = 0xB8; *(uint32_t*)(t + 1) = *(uint32_t*)(f + 1); t[5] = 0xE9; *(int32_t*)(t + 6) = (int32_t)((f + 5) - (t + 10));
    VirtualProtect(g_tramp, 4096, PAGE_EXECUTE_READ, &o); FlushInstructionCache(GetCurrentProcess(), t, 16);
    *trampolin = t;
    volatile LONG64* q = (volatile LONG64*)((uintptr_t)f & ~7u); int off = (int)((uintptr_t)f & 7);
    VirtualProtect((void*)q, 8, PAGE_EXECUTE_READWRITE, &o);
    LONG64 viejo = *q, nuevo = viejo; uint8_t* nb = (uint8_t*)&nuevo;
    nb[off] = 0xE9; *(int32_t*)(nb + off + 1) = (int32_t)((uint8_t*)propio - (f + 5));
    int ok = InterlockedCompareExchange64(q, nuevo, viejo) == viejo;
    VirtualProtect((void*)q, 8, o, &o); FlushInstructionCache(GetCurrentProcess(), (void*)q, 8);
    return ok ? "OK" : "carrera";
}

// ---------------------------------------------------------------- parcheo de IAT
static int iat(HMODULE m, const void* const* objetivos, int nObj, void* nuevo, void** original, const char* nombre) {
    int n = 0;
    __try {
        uint8_t* b = (uint8_t*)m; IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(b + ((IMAGE_DOS_HEADER*)b)->e_lfanew);
        IMAGE_DATA_DIRECTORY dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!dd.VirtualAddress) return 0;
        for (IMAGE_IMPORT_DESCRIPTOR* d = (IMAGE_IMPORT_DESCRIPTOR*)(b + dd.VirtualAddress); d->Name; d++) {
            void** slot = (void**)(b + d->FirstThunk);
            for (; *slot; slot++) {
                int es = 0; for (int i = 0; i < nObj; i++) if (*slot == objetivos[i]) es = 1;
                if (!es || *slot == nuevo) continue;
                if (!*original) *original = *slot;
                DWORD o; VirtualProtect(slot, 4, PAGE_READWRITE, &o);
                InterlockedExchangePointer(slot, nuevo);
                VirtualProtect(slot, 4, o, &o);
                n++;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { L("IAT: excepcion leyendo las importaciones de %s", nombre); }
    return n;
}
static void* resuelve(const char* dll, const char* fn) { HMODULE h = GetModuleHandleA(dll); return h ? (void*)GetProcAddress(h, fn) : nullptr; }
static std::vector<std::string> g_hechos;
static int ya_hecho(const std::string& k) { for (auto& x : g_hechos) if (x == k) return 1; g_hechos.push_back(k); return 0; }
static void parchea_modulos() {
    HMODULE hm[1024]; DWORD nb = 0; if (!EnumProcessModules(GetCurrentProcess(), hm, sizeof hm, &nb)) return;
    const void* vp[] = { resuelve("kernel32.dll", "VirtualProtect"), resuelve("kernelbase.dll", "VirtualProtect") };
    const void* gks[] = { resuelve("user32.dll", "GetKeyState") };
    const void* rok[] = { resuelve("advapi32.dll", "RegOpenKeyExA"), resuelve("kernelbase.dll", "RegOpenKeyExA") };
    const void* gfaa[] = { resuelve("kernel32.dll", "GetFileAttributesA"), resuelve("kernelbase.dll", "GetFileAttributesA") };
    const void* gfaw[] = { resuelve("kernel32.dll", "GetFileAttributesW"), resuelve("kernelbase.dll", "GetFileAttributesW") };
    for (DWORD i = 0; i < nb / sizeof(HMODULE); i++) {
        char nm[MAX_PATH]; GetModuleBaseNameA(GetCurrentProcess(), hm[i], nm, MAX_PATH);
        for (auto& x : g_modsVP) if (_stricmp(nm, x.c_str()) == 0 && g_usarVP && !ya_hecho(std::string("VP|") + nm))
            L("VirtualProtect: %d entradas de la IAT de %s redirigidas", iat(hm[i], vp, 2, (void*)s_VP, (void**)&o_VP, nm), nm);
        for (auto& x : g_modsTeclas) if (_stricmp(nm, x.c_str()) == 0 && g_usarTeclas && !ya_hecho(std::string("GKS|") + nm))
            L("GetKeyState: %d entradas de la IAT de %s redirigidas", iat(hm[i], gks, 1, (void*)s_GKS, (void**)&o_GKS, nm), nm);
        for (auto& x : g_modsAPL) if (_stricmp(nm, x.c_str()) == 0 && g_usarAPL && !ya_hecho(std::string("APL|") + nm))
            L("RegOpenKeyExA (APL): %d entradas de la IAT de %s redirigidas", iat(hm[i], rok, 2, (void*)s_ROKA, (void**)&o_ROKA, nm), nm);
        for (auto& x : g_modsArch) if (_stricmp(nm, x.c_str()) == 0 && g_usarArch && g_ganchosArch && !ya_hecho(std::string("ARCH|") + nm)) {
            int a = iat(hm[i], gfaa, 2, (void*)s_GFAA, (void**)&o_GFAA, nm), w = iat(hm[i], gfaw, 2, (void*)s_GFAW, (void**)&o_GFAW, nm);
            L("GetFileAttributesA/W (cache de inexistencia en modloader\\): %d + %d entradas de la IAT de %s redirigidas", a, w, nm);
        }
    }
}
static DWORD WINAPI hilo(void*) {
    // Los .cleo y los ASI de Mod Loader se cargan despues que este plugin: revisar los modulos durante un rato
    double tUlt = 0;
    for (int i = 0;; i++) {
        Sleep(i < 120 ? 500 : 5000);
        parchea_modulos();
        double t = ms();
        if (t - tUlt >= g_resumenSeg * 1000.0) {
            tUlt = t;
            L("RESUMEN activo=%ld | APL: %ld al registro, %ld de cache | VirtualProtect gta_sa: %ld reales, %ld omitidas, %ld sin tocar, %ld reabiertas por excepcion | "
              "GetKeyState: %ld fotos, %ld servidas de la foto, %ld directas | GetFileAttributes (Mod Loader): %ld al disco, %ld de cache, %ld recordadas, "
              "%ld invalidaciones (archivo creado/renombrado/borrado), %ld vaciados | carpetas inexistentes: %ld recordadas, %ld rutas contestadas por carpeta, %ld sondeos",
              RHP_Activo, c_aplReal, c_aplCache, c_vpReal, c_vpElidida, c_vpPasa, c_vehReabre, c_tecFoto, c_tecServida, c_tecPasa,
              c_archReal, c_archCache, c_archGuardadas, c_archInval, c_archVaciados, c_dirGuardados, c_dirCache, c_dirSondeos);
        }
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD razon, LPVOID) {
    if (razon != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(h); InitializeCriticalSection(&g_csLog);
    QueryPerformanceFrequency(&g_f); QueryPerformanceCounter(&g_t0);
    char ruta[MAX_PATH]; GetModuleFileNameA((HMODULE)&__ImageBase, ruta, MAX_PATH);
    std::string base = ruta; size_t punto = base.rfind('.'); if (punto != std::string::npos) base.resize(punto);
    if (base.size() > 3 && _stricmp(base.c_str() + base.size() - 3, ".SA") == 0) base.resize(base.size() - 3);
    std::string ini = base + ".ini";
    if (!GetPrivateProfileIntA("Rendimiento", "Activo", 1, ini.c_str())) return TRUE;
    g_usarAPL = GetPrivateProfileIntA("Rendimiento", "CacheAPL", 1, ini.c_str());
    g_usarVP = GetPrivateProfileIntA("Rendimiento", "VirtualProtectGtaSa", 1, ini.c_str());
    g_usarTeclas = GetPrivateProfileIntA("Rendimiento", "FotoTeclas", 1, ini.c_str());
    g_cacheAPLms = GetPrivateProfileIntA("Rendimiento", "CacheAPLms", 5000, ini.c_str());
    g_ventanaTeclasUs = GetPrivateProfileIntA("Rendimiento", "VentanaTeclasUs", 1000, ini.c_str());
    g_resumenSeg = std::max(5, (int)GetPrivateProfileIntA("Rendimiento", "ResumenSegundos", 60, ini.c_str()));
    lista(ini.c_str(), "ModulosVirtualProtect", "wantedleveleditor.asi,CLEO+.cleo,CLEO.asi,PedsExtender.SA.asi,Urbanize (Junior_Djjr).asi,ModelVariations.asi,FxsFuncs.asi,Soundize (Junior_Djjr).asi", g_modsVP);
    lista(ini.c_str(), "ModulosTeclas", "CLEO+.cleo,CLEO.asi,wantedleveleditor.asi,ModelVariations.asi", g_modsTeclas);
    lista(ini.c_str(), "ModulosAPL", "dsound.dll", g_modsAPL);
    g_usarArch = GetPrivateProfileIntA("Rendimiento", "CacheArchivosModLoader", 1, ini.c_str());
    g_cacheArchMs = GetPrivateProfileIntA("Rendimiento", "CacheArchivosMs", 600000, ini.c_str());
    g_usarDirs = GetPrivateProfileIntA("Rendimiento", "CacheDirectorios", 1, ini.c_str());
    lista(ini.c_str(), "ModulosArchivos", "std.asi.dll", g_modsArch);
    g_log = fopen((base + ".log").c_str(), "w");
    o_GKBS = (PGKBS)resuelve("user32.dll", "GetKeyboardState");
    if (!o_GKBS) g_usarTeclas = 0;
    {   MODULEINFO mi; HMODULE exe = GetModuleHandleA(nullptr);
        if (GetModuleInformation(GetCurrentProcess(), exe, &mi, sizeof mi)) {
            g_exeIni = (uintptr_t)mi.lpBaseOfDll; g_exeFin = g_exeIni + mi.SizeOfImage;
            g_abierta = (volatile uint8_t*)VirtualAlloc(nullptr, (g_exeFin - g_exeIni) >> 12, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        }
    }
    if (g_usarVP) AddVectoredExceptionHandler(1, veh);
    L("RendimientoHiloPrincipal v3.1 (2026-10-12; prefijos sin asignacion). CacheAPL=%d (%d ms) VirtualProtectGtaSa=%d FotoTeclas=%d (%d us) CacheArchivosModLoader=%d (%d ms) CacheDirectorios=%d. gta_sa.exe %p-%p",
      g_usarAPL, g_cacheAPLms, g_usarVP, g_usarTeclas, g_ventanaTeclasUs, g_usarArch, g_cacheArchMs, g_usarDirs, (void*)g_exeIni, (void*)g_exeFin);
    if (g_usarArch) {
        g_noExiste = new CacheRutas(); g_dirNo = new CacheRutas();
        p_GFAWsis = (PGFAW)resuelve("kernelbase.dll", "GetFileAttributesW"); if (!p_GFAWsis) p_GFAWsis = (PGFAW)resuelve("kernel32.dll", "GetFileAttributesW");
        g_tramp = (uint8_t*)VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READ);
        const char* r1 = g_tramp ? engancha_stub("NtCreateFile", (void*)h_NtCreateFile, (void**)&t_NtCreateFile, 0) : "sin memoria";
        const char* r2 = g_tramp ? engancha_stub("NtSetInformationFile", (void*)h_NtSetInformationFile, (void**)&t_NtSetInformationFile, 1) : "sin memoria";
        g_ganchosArch = strcmp(r1, "OK") == 0 && strcmp(r2, "OK") == 0;
        L("Invalidacion de la cache de archivos: NtCreateFile=%s NtSetInformationFile=%s -> cache %s", r1, r2, g_ganchosArch ? "ACTIVA" : "NO se usa");
    }
    CreateThread(nullptr, 0, hilo, nullptr, 0, nullptr);
    return TRUE;
}
