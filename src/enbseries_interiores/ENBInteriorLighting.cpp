// ENB 0.430: conserva el ambiente original dentro de las areas interiores de SA.
// Complementa los tres shaders de Vice City Viva del 06-10-2026.
// Un unico CALL de ENB, despues de su actualizacion normal; no modifica el EXE.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static uintptr_t base;
static FILE* logFile;
static int lastArea = -999;
static DWORD lastLog;
static void (__cdecl *original)(float*);
template<class T> static T& M(uintptr_t rva) { return *(T*)(base + rva); }
static void Log(const char* format, ...) {
    if (!logFile) return;
    va_list a; va_start(a, format); vfprintf(logFile, format, a); va_end(a);
    fputc('\n', logFile); fflush(logFile);
}
static bool Readable(const void* ptr, size_t n) {
    MEMORY_BASIC_INFORMATION m{};
    return VirtualQuery(ptr, &m, sizeof m) && m.State == MEM_COMMIT &&
        !(m.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
        (uintptr_t)ptr + n <= (uintptr_t)m.BaseAddress + m.RegionSize;
}
static void __cdecl Update(float* input) {
    original(input); // ENB recalcula los valores normales cada fotograma, tambien al salir.
    const int area = *(volatile int*)0xB72914; // CGame::currArea, validado desde la instruccion viva.
    const float priorFactor = M<float>(0x19C654);
    const float priorAmbient = M<float>(0x139C64);
    if (area > 0 && area < 256) {
        M<float>(0x19C654) = 1.0f;
        // StaticAmbient=0 convierte el material en blanco; =1 conserva su color original.
        // Se usa el valor nocturno configurado, sin cambiar dia/noche global ni el reloj.
        M<float>(0x139C64) = M<float>(0x139C6C);
    }
    const DWORD now = GetTickCount();
    if (area != lastArea || now - lastLog >= 60000) {
        Log("AREA=%d hora=%.3f factor=%.1f->%.1f ambiente=%.3f->%.3f noche=%.3f",
            area, M<float>(0x139730), priorFactor, M<float>(0x19C654),
            priorAmbient, M<float>(0x139C64), M<float>(0x139C6C));
        lastArea = area; lastLog = now;
    }
}
static bool Operand(uintptr_t rva, unsigned char op1, unsigned char op2, uintptr_t target) {
    return M<unsigned char>(rva) == op1 && M<unsigned char>(rva+1) == op2 &&
        M<uint32_t>(rva+2) == base + target;
}
static bool Signatures() {
    const unsigned char game[] = {0x8B,0x0D,0x14,0x29,0xB7,0x00,0x33,0xC0,0x85,0xC9,0x0F,0x94,0xC0,0xC3};
    if (!Readable((void*)0x53C4A0, sizeof game) || memcmp((void*)0x53C4A0, game, sizeof game) ||
        !Readable((void*)0xB72914, 4)) { Log("FIRMA GTA: currArea no coincide; no se instala"); return false; }
    if (!Readable((void*)base, 0x40) || M<uint16_t>(0) != IMAGE_DOS_SIGNATURE) return false;
    auto nt = (IMAGE_NT_HEADERS32*)(base + M<uint32_t>(0x3C));
    if (!Readable(nt, sizeof *nt) || nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.SizeOfImage < 0x19D200 || !Readable((void*)(base+0x139000), 0x70000)) return false;
    const unsigned char prologue[] = {0xB9,1,0,0,0,0x83,0xEC,0x10};
    bool ok = M<unsigned char>(0x2E613) == 0xE8 &&
        base + 0x2E618 + M<int32_t>(0x2E614) == base + 0x2BB50 &&
        !memcmp((void*)(base+0x2BB50), prologue, sizeof prologue) &&
        Operand(0x2BCD6,0xD9,0x1D,0x19C654) &&
        Operand(0x2BE3D,0xD9,0x05,0x139C68) &&
        Operand(0x2BE45,0xD9,0x05,0x139C6C) &&
        Operand(0x2BE4F,0xD9,0x1D,0x139C64);
    if (!ok) Log("FIRMA ENB: llamada/actualizacion/globales no coinciden; no se escribe nada");
    return ok;
}
BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        char path[MAX_PATH]{}; GetModuleFileNameA(h,path,MAX_PATH);
        char* end = strrchr(path,'\\'); if (!end) return TRUE; ++end;
        strcpy(end,"enbseries_interiores.log"); logFile = fopen(path,"w");
        strcpy(end,"enbseries_interiores.ini");
        int active = GetPrivateProfileIntA("General","Activo",1,path);
        Log("ENBInteriorLighting 1.0 (09-10-2026), Activo=%d",active);
        if (!active) return TRUE;
        base = (uintptr_t)GetModuleHandleA("enbseries.asi");
        if (!base) { Log("FIRMA: ENB no cargado; este ASI debe cargar despues"); return TRUE; }
        if (!Signatures()) return TRUE;
        original = (void(__cdecl*)(float*))(base+0x2BB50);
        DWORD protection;
        if (!VirtualProtect((void*)(base+0x2E614),4,PAGE_EXECUTE_READWRITE,&protection)) {
            Log("ERROR VirtualProtect=%lu; no se instala",GetLastError()); return TRUE;
        }
        M<int32_t>(0x2E614) = (int32_t)((uintptr_t)Update-(base+0x2E618));
        DWORD ignored; VirtualProtect((void*)(base+0x2E614),4,protection,&ignored);
        FlushInstructionCache(GetCurrentProcess(),(void*)(base+0x2E613),5);
        Log("GANCHO OK base=%p CALL +2E613; area del juego + EInteriorFactor + StaticAmbientNight",(void*)base);
    }
    if (reason == DLL_PROCESS_DETACH && logFile) { fclose(logFile); logFile=nullptr; }
    return TRUE;
}
