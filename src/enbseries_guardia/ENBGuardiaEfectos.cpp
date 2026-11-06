// enbseries_guardia.asi — guardia de los efectos internos de ENBSeries 0.430 (GTA SA, SA_DirectX 3.0).
//
// PROTOTIPO (02-10-2026). No instalado en el juego. Se prueba con tests/arnes_enb (--extra).
//
// Crash que corrige: enbseries.asi+0x1BC42 (26-09-2026 18:39, 52 min de partida, 3,3 GB virtuales).
// ENB_BlitPrevFrame (0x1001BB60) hace SetVector("ScreenSize") sobre g_pInternalEffect1 (0x101E4044) = NULL.
//
// Mecanismo [E] (DIAGNOSTICO-2026-10-02-crash-enb-1BC42-efecto-interno-nulo.md):
//   OnLostDevice -> ReleaseResources -> 0x10034EC0 libera los efectos y pone los punteros a 0.
//   OnResetDevice (0x1002C790) -> CreateResources -> Effects_LoadAllFx: si D3DXCreateEffect falla (falta de
//     espacio de direcciones), el puntero queda NULL y ENB pone UseEffect (0x1019C788) = 0 ...
//   ... pero a continuacion Config_LoadAll (0x1002C79F) relee el INI y deja UseEffect = 1.
//   En CADA Present, ENB_EndOfFrame llama a BlitPrevFrame (0x1002E1AE) si 0x1019C684 = 1, bandera que
//   ENB_AfterPresent (0x1002CEF0) vuelve a poner a 1 tras cada Present; BlitPrevFrame no mira ni
//   UseEffect ni el puntero -> crash en el primer Present tras el Reset (reproducido en el arnes).
//   ENB si se protege en ENB_DrawOverlayAndApply (UseEffect = 0 si falta un efecto interno), pero esa
//   funcion corre DESPUES de BlitPrevFrame. Con UseEffect = 0 el resto de caminos de ENB no usan los
//   efectos internos (OnDrawPrimitive, OnDrawIndexedPrimitive, Clear y PostProcessFromGameQuad lo miran),
//   SALVO EnbDevice9::StretchRect (0x10008B86), que llama a CaptureOpaqueScene sin mirarlo: lo encontro
//   el arnes con fotogramas con dibujo (crash +0x1CCDC con la version 1 de esta guardia).
//
// Que hace (6 llamadas E8 de ENB, redirigidas a thunks propios; el original se llama siempre igual):
//   0x1002C6E9 Config_LoadAll    (OnCreateDevice) -> original; despues sanea()
//   0x1002C79F Config_LoadAll    (OnResetDevice)  -> original; despues sanea()
//   0x1002D0EE Effects_LoadAllFx (Apply Changes)  -> original; despues sanea()
//   0x1002E1AE ENB_BlitPrevFrame (EndOfFrame)     -> solo si los dos efectos internos existen
//   0x1002E35B ENB_DrawCursor    (EndOfFrame)     -> solo si los dos efectos internos existen
//   0x10008B86 ENB_CaptureOpaqueScene (EnbDevice9::StretchRect, primer StretchRect de cada fotograma; no
//              mira UseEffect) -> solo si los dos efectos internos existen; si no, ret 4 (su argumento)
//   sanea(): si falta un efecto interno, UseEffect = 0 (ENB pasa todo al d3d9 real: el juego se ve sin
//   ENB). En el siguiente Reset ENB vuelve a crearlos y Config_LoadAll restaura
//   UseEffect desde el INI: se recupera solo. Sin fallo no cambia nada (una comparacion por punto).
//
// Firmas (todas o ninguna): cada sitio es E8 al destino esperado, el destino empieza por sus bytes, y
// los operandos absolutos de 0x1002D036/0x1002D045/0x1002D03F son los globales reubicados.
// En el juego ENB no carga en 0x10000000 (vorbisFile.dll): VA real = base + (VA - 0x10000000).
//
// Configuracion: enbseries_guardia.ini junto a la DLL ([General] Activo, Log).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
extern "C" IMAGE_DOS_HEADER __ImageBase;

static uintptr_t g_delta;
static FILE* g_log;
static volatile LONG g_eventos, g_omitidos, g_recuperados;
static int g_fallo;                    // 1 mientras falte un efecto interno

// destinos originales (los llaman los thunks)
static void* o_cfg;
static void* o_load;
static void* o_blit;
static void* o_cursor;
static void* o_capture;

static uintptr_t G(uintptr_t va) { return va + g_delta; }

static void L(const char* f, ...) {
    if (!g_log) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(g_log, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list a; va_start(a, f); vfprintf(g_log, f, a); va_end(a); fputc('\n', g_log); fflush(g_log);
}

static const char* ORIGEN[] = { "?", "OnCreateDevice", "OnResetDevice", "Apply Changes", "BlitPrevFrame", "DrawCursor", "StretchRect->CaptureOpaqueScene" };

static void __cdecl sanea(int origen) {
    void* i1 = *(void**)G(0x101E4044);
    void* i2 = *(void**)G(0x101E4048);
    int* use = (int*)G(0x1019C788);
    if (i1 && i2) {
        if (g_fallo) { g_fallo = 0; InterlockedIncrement(&g_recuperados); L("RECUPERADO tras %s: interno1=%p interno2=%p UseEffect=%d", ORIGEN[origen], i1, i2, *use); }
        return;
    }
    InterlockedIncrement(&g_eventos);
    L("EFECTO INTERNO NULO tras %s: interno1=%p interno2=%p UseEffect %d -> 0", ORIGEN[origen], i1, i2, *use);
    *use = 0;
    g_fallo = 1;
}

static int __cdecl permite(int origen) {
    if (*(void**)G(0x101E4044) && *(void**)G(0x101E4048)) return 1;
    LONG n = InterlockedIncrement(&g_omitidos);
    if (n <= 5 || n % 1000 == 0) L("%s OMITIDO: falta un efecto interno (omisiones: %ld)", ORIGEN[origen], n);
    return 0;
}

// Thunks. Las cuatro funciones originales no reciben argumentos en la pila (ret simple, medido) y
// Effects_LoadAllFx/BlitPrevFrame/DrawCursor reciben "this" en ECX, que llega intacto.
__declspec(naked) static void t_cfg_crear() {
    __asm { call dword ptr [o_cfg]
            pushad
            push 1
            call sanea
            add esp, 4
            popad
            ret }
}
__declspec(naked) static void t_cfg_reset() {
    __asm { call dword ptr [o_cfg]
            pushad
            push 2
            call sanea
            add esp, 4
            popad
            ret }
}
__declspec(naked) static void t_load_apply() {
    __asm { call dword ptr [o_load]
            pushad
            push 3
            call sanea
            add esp, 4
            popad
            ret }
}
__declspec(naked) static void t_blit() {
    __asm { pushad
            push 4
            call permite
            add esp, 4
            test eax, eax
            popad
            jz omite
            jmp dword ptr [o_blit]
          omite:
            ret }
}
__declspec(naked) static void t_cursor() {
    __asm { pushad
            push 5
            call permite
            add esp, 4
            test eax, eax
            popad
            jz omite
            jmp dword ptr [o_cursor]
          omite:
            ret }
}

// CaptureOpaqueScene es thiscall con un argumento (la textura INTZ) y termina en ret 4.
__declspec(naked) static void t_capture() {
    __asm { pushad
            push 6
            call permite
            add esp, 4
            test eax, eax
            popad
            jz omite
            jmp dword ptr [o_capture]
          omite:
            ret 4 }
}

struct Sitio { uintptr_t va, destino; const uint8_t* prologo; int nprologo; void* thunk; void** orig; const char* nombre; };
static const uint8_t P_CFG[] = { 0x81, 0xEC, 0x20, 0x03, 0x00, 0x00 };          // Config_LoadAll: sub esp,320h
static const uint8_t P_LOAD[] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8 };         // Effects_LoadAllFx
static const uint8_t P_BLIT[] = { 0x81, 0xEC, 0xDC, 0x00, 0x00, 0x00, 0x53, 0x33, 0xDB };
static const uint8_t P_CURSOR[] = { 0x81, 0xEC, 0x80, 0x00, 0x00, 0x00, 0x53, 0x55 };
static const uint8_t P_CAPTURE[] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0xB8, 0x0C, 0x81, 0x00, 0x00 };   // ... mov eax,810Ch
static Sitio SITIOS[] = {
    { 0x1002C6E9, 0x1002AFF0, P_CFG, sizeof P_CFG, (void*)t_cfg_crear, &o_cfg, "Config_LoadAll en OnCreateDevice" },
    { 0x1002C79F, 0x1002AFF0, P_CFG, sizeof P_CFG, (void*)t_cfg_reset, &o_cfg, "Config_LoadAll en OnResetDevice" },
    { 0x1002D0EE, 0x10035060, P_LOAD, sizeof P_LOAD, (void*)t_load_apply, &o_load, "Effects_LoadAllFx en Apply Changes" },
    { 0x1002E1AE, 0x1001BB60, P_BLIT, sizeof P_BLIT, (void*)t_blit, &o_blit, "ENB_BlitPrevFrame en EndOfFrame" },
    { 0x1002E35B, 0x1002DA70, P_CURSOR, sizeof P_CURSOR, (void*)t_cursor, &o_cursor, "ENB_DrawCursor en EndOfFrame" },
    { 0x10008B86, 0x1001CBF0, P_CAPTURE, sizeof P_CAPTURE, (void*)t_capture, &o_capture, "ENB_CaptureOpaqueScene en StretchRect" },
};
// Operandos absolutos que fijan los globales (opcode, longitud del opcode, global)
struct Operando { uintptr_t va; uint8_t op[2]; int nop; uintptr_t global; };
static const Operando OPERANDOS[] = {
    { 0x1002D036, { 0xA1 }, 1, 0x101E4044 },          // mov eax,[g_pInternalEffect1]
    { 0x1002D045, { 0x39, 0x2D }, 2, 0x101E4048 },    // cmp [g_pInternalEffect2],ebp
    { 0x1002D03F, { 0x89, 0x2D }, 2, 0x1019C788 },    // mov [UseEffect],ebp
};

static bool lee_ok(uintptr_t p, size_t n) { MEMORY_BASIC_INFORMATION m; return VirtualQuery((void*)p, &m, sizeof m) && m.State == MEM_COMMIT && !(m.Protect & (PAGE_NOACCESS | PAGE_GUARD)) && p + n <= (uintptr_t)m.BaseAddress + m.RegionSize; }

static bool firmas() {
    bool ok = true;
    for (auto& s : SITIOS) {
        uintptr_t v = G(s.va);
        if (!lee_ok(v, 5)) { L("FIRMA: %s (%08X) no legible", s.nombre, (unsigned)v); ok = false; continue; }
        const uint8_t* b = (const uint8_t*)v;
        uintptr_t dst = v + 5 + *(int32_t*)(b + 1);
        if (b[0] != 0xE8 || dst != G(s.destino)) {
            L("FIRMA: %s (%08X) no es 'call %08X' (bytes %02X %02X %02X %02X %02X, destino %08X)", s.nombre, (unsigned)v,
              (unsigned)G(s.destino), b[0], b[1], b[2], b[3], b[4], (unsigned)dst);
            ok = false; continue;
        }
        if (!lee_ok(dst, s.nprologo) || memcmp((void*)dst, s.prologo, s.nprologo)) { L("FIRMA: el destino de %s (%08X) no empieza como se espera", s.nombre, (unsigned)dst); ok = false; }
    }
    for (auto& o : OPERANDOS) {
        uintptr_t v = G(o.va);
        if (!lee_ok(v, o.nop + 4) || memcmp((void*)v, o.op, o.nop) || *(uint32_t*)(v + o.nop) != (uint32_t)G(o.global)) {
            L("FIRMA: la instruccion de %08X no usa el global %08X", (unsigned)v, (unsigned)G(o.global)); ok = false;
        }
    }
    return ok;
}

static void instala() {
    char ruta[MAX_PATH]; GetModuleFileNameA((HMODULE)&__ImageBase, ruta, MAX_PATH);
    std::string dir = ruta; dir = dir.substr(0, dir.find_last_of("\\/") + 1);
    std::string ini = dir + "enbseries_guardia.ini";
    int activo = GetPrivateProfileIntA("General", "Activo", 1, ini.c_str());
    if (GetPrivateProfileIntA("General", "Log", 1, ini.c_str())) g_log = fopen((dir + "enbseries_guardia.log").c_str(), "w");
    L("enbseries_guardia prototipo 2 (02-10-2026). Activo=%d", activo);
    if (!activo) return;
    HMODULE enb = GetModuleHandleA("enbseries.asi");
    if (!enb) { L("FIRMA: enbseries.asi no esta cargado (esta DLL debe cargar despues): no se hace nada"); return; }
    g_delta = (uintptr_t)enb - 0x10000000u;
    L("enbseries.asi en %p (desplazamiento 0x%08X respecto a 0x10000000)", enb, (unsigned)g_delta);
    if (!firmas()) { L("No se escribe nada: alguna firma no cuadra (otra version de ENB u otro mod en el mismo sitio)"); return; }
    for (auto& s : SITIOS) {
        uintptr_t v = G(s.va);
        *s.orig = (void*)G(s.destino);
        int32_t rel = (int32_t)((uintptr_t)s.thunk - (v + 5));
        DWORD old; VirtualProtect((void*)(v + 1), 4, PAGE_EXECUTE_READWRITE, &old);
        *(int32_t*)(v + 1) = rel;
        VirtualProtect((void*)(v + 1), 4, old, &old);
        FlushInstructionCache(GetCurrentProcess(), (void*)v, 5);
        L("GANCHO OK %s (%08X)", s.nombre, (unsigned)v);
    }
    L("Estado inicial: interno1=%p interno2=%p UseEffect=%d", *(void**)G(0x101E4044), *(void**)G(0x101E4048), *(int*)G(0x1019C788));
}

// Solo para el arnes: contadores (eventos de efecto nulo, omisiones de Blit/Cursor, recuperaciones)
extern "C" __declspec(dllexport) void EGE_Contadores(long* eventos, long* omitidos, long* recuperados) {
    if (eventos) *eventos = g_eventos; if (omitidos) *omitidos = g_omitidos; if (recuperados) *recuperados = g_recuperados;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID) {
    if (r == DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(h); instala(); }
    if (r == DLL_PROCESS_DETACH && g_log) {
        L("descarga: eventos=%ld omisiones=%ld recuperaciones=%ld", g_eventos, g_omitidos, g_recuperados);
        fclose(g_log); g_log = nullptr;
    }
    return TRUE;
}
