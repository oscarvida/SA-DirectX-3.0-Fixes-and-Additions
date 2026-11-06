// ShadowCasterFix — sombras complejas de ENBSeries 0.430 (SA_DirectX 3.0) que desaparecen al girar la camara.
//
// PROTOTIPO B (rama del 2026-10-01). Solo para la copia de prueba D:\GTA\gta-sa-enb-prueba.
// Documento: D:\documentos gta-sa\DIAGNOSTICO-2026-10-01-enb-sombras-camera-culling.md, §9.1.
//
// Causa demostrada (pruebas 1-3): ENB no tiene lista de casters; repite en el mapa de sombras los DIP que manda
// el juego. Cuando CEntity::GetIsOnScreen (0x534540) descarta un objeto, desaparecen a la vez su dibujo y su
// sombra. Con GetIsOnScreen siempre verdadero la sombra vuelve (tambien detras de la camara), a un coste de
// -17 % de fps, y ademas cambia poblacion y streaming.
//
// Que hace este ASI (ENB no se toca; solo se LEEN dos matrices suyas):
//  1. Cambia SOLO las 4 llamadas a GetIsOnScreen de CRenderer::SetupMapEntityVisibility (0x5540AD, 0x554174)
//     y CRenderer::SetupEntityVisibility (0x5543C2, 0x554504) por GetIsOnScreen(e) || ProyectaSombraVisible(e).
//     Las demas llamadas (streaming en ScanSectorList, poblacion, coronas...) siguen igual.
//  2. ProyectaSombraVisible(e): la esfera de colision de la entidad
//       a) cae dentro de la cascada 1 de ENB (VIEW sustituida 0x101E3730 x PROJ 0x1019D048 = VP de la luz), y
//       b) la "capsula" de su sombra (la esfera arrastrada en la direccion de la luz, hacia el suelo) toca el
//          frustum de la camara (CCamera::IsSphereVisible 0x420C40, el mismo test que GetIsOnScreen).
//     Solo actua dentro de CRenderer::ScanWorld y en exteriores.
//  3. Al terminar ScanWorld (llamada 0x5558DD de ConstructRenderList) recorre con ScanSectorList los sectores
//     que cubre la columna de la cascada 1 y no estaban en la cuna de la camara. El scan code evita repetir.
//  Tecla (4 por defecto) = activar/desactivar en caliente, para la prueba A/B con el registrador v3.
//
// v2 (tras la prueba 4): la v1 costaba -25/-30 % de fps sin limitador con la camara bajada y el sol detras. La
// capsula de la sombra bajaba 25 m por debajo del objeto y, con la camara inclinada, entraba en el frustum por
// debajo del suelo. Ahora hay 3 ajustes que se alternan con la tecla 3:
//   A = v1 (largo (r+25)/|dz|, margen 3 m, paso >= 5 m);
//   B = la sombra termina en el suelo: largo (2r+CaidaSombra)/|dz| (incluye la punta de la sombra de lo mas alto
//       de la esfera), margen y paso mas pequenos;
//   C = B + DistanciaMaxima a la camara.
// El SEG anade el ajuste, los descartes por distancia y el reparto por distancia de los aceptados.
//
// v4 (tras instalarlo en la principal, 01-10): alli no se instalaba. El FLA (parche de IDs) sustituye en 0x534546
// la lectura de ms_modelInfoPtrs por un salto a su propio codigo, y la v3 exigia esos bytes para leer la esfera.
// Ahora la esfera (centro y radio) se toma de la propia GetIsOnScreen: se engancha su llamada a
// CCamera::IsSphereVisible (E8 en 0x53458B) y se guarda lo que recibe. No hace falta la tabla de modelos.
//
// Direcciones del juego: gta_sa.exe 1.0 US. Todo se comprueba contra los bytes VIVOS antes de escribir.
// La tabla de modelos y la cuadricula de sectores se leen de las instrucciones vivas (el FLA las reubica).
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string>
#include <algorithm>
extern "C" IMAGE_DOS_HEADER __ImageBase;

// ---------------------------------------------------------------- registro
static FILE* g_log;
static LARGE_INTEGER g_f, g_t0;
static int g_frame;
static double ms() { LARGE_INTEGER t; QueryPerformanceCounter(&t); return 1000.0 * (t.QuadPart - g_t0.QuadPart) / g_f.QuadPart; }
static void L(const char* f, ...) {
    if (!g_log) return;
    fprintf(g_log, "[%10.1f f%06d] ", ms(), g_frame);
    va_list a; va_start(a, f); vfprintf(g_log, f, a); va_end(a); fputc('\n', g_log); fflush(g_log);
}
static const char* quien(const void* p) {
    static char b[4][96]; static int k; char* s = b[k++ & 3];
    HMODULE m = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &m) && m) {
        char n[MAX_PATH]; GetModuleFileNameA(m, n, MAX_PATH); const char* c = strrchr(n, '\\');
        snprintf(s, 96, "%s+0x%X", c ? c + 1 : n, (unsigned)((uintptr_t)p - (uintptr_t)m));
    } else snprintf(s, 96, "%p", p);
    return s;
}

// ---------------------------------------------------------------- configuracion
static int g_activo = 1, g_tecla = 52, g_sitioOn[4] = { 1, 1, 1, 1 }, g_escanearSectores = 1, g_exigirPantalla = 1,
           g_soloExteriores = 1, g_maxAceptados = 400, g_limiteVisibles = 800, g_maxMuestras = 12;
struct Ajuste { char letra; float margen, caida, pasoMin, distMax; int alSuelo; };
static Ajuste g_aj[3] = { { 'A', 3.f, 25.f, 5.f, 0.f, 0 }, { 'B', 1.5f, 3.f, 2.f, 0.f, 1 }, { 'C', 1.5f, 3.f, 2.f, 40.f, 1 } };
static int g_ajuste = 1, g_teclaAjuste = 51;
static float g_margen = 3.f, g_caida = 25.f, g_largoMax = 120.f, g_pasoMin = 5.f, g_radioMin = 1.f,
             g_secBajo = 60.f, g_secSobre = 120.f, g_secMargen = 30.f, g_secRadioMax = 400.f;

// ---------------------------------------------------------------- juego (1.0 US)
static uint8_t* const k_sitio[4] = { (uint8_t*)0x5540AD, (uint8_t*)0x554174, (uint8_t*)0x5543C2, (uint8_t*)0x554504 };
static const char* const k_sitioNombre[4] = { "SetupMapEntityVisibility#1 0x5540AD", "SetupMapEntityVisibility#2 0x554174",
                                             "SetupEntityVisibility#1 0x5543C2", "SetupEntityVisibility#2 0x554504" };
static uint8_t* const k_callScanWorld = (uint8_t*)0x5558DD;   // E8 -> CRenderer::ScanWorld 0x554FE0
static const uintptr_t k_GetIsOnScreen = 0x534540, k_ScanWorld = 0x554FE0;
static void* const k_TheCamera = (void*)0xB6F028;
static void* const k_matInversa = (void*)0xB6FA74;            // TheCamera.m_mMatInverse (0xB6F028 + 0xA4C)
static const int* const k_currArea = (const int*)0xB72914;   // CGame::currArea

typedef bool(__thiscall* PIsOnScreen)(void* ent);
typedef void*(__thiscall* PTransform)(void* ent, float* out, const float* in);           // 0x5334F0
typedef bool(__thiscall* PSphereVis)(void* cam, const float* c, float r, void* mat);      // 0x420C40
typedef void(__cdecl* PScanSector)(int x, int y);
typedef void(__cdecl* PScanWorld)();
static PIsOnScreen o_sitio[4];
static PScanWorld o_ScanWorld;
static PScanSector g_ScanSectorList;
static const PTransform k_Transform = (PTransform)0x5334F0;
static const PSphereVis k_SphereVis = (PSphereVis)0x420C40;
// Esfera de la ultima llamada de GetIsOnScreen a IsSphereVisible (call E8 en 0x53458B)
static uint8_t* const k_callEsfera = (uint8_t*)0x53458B;
static PSphereVis o_esferaGIOS;
static float g_escC[3], g_escR; static int g_escOk, g_esferaOk;
static float g_secEscala, g_secOffset; static int g_secN;
static volatile int* g_cntVisibles; static volatile int* g_cntLods; static int g_listasVanilla;
static int g_sitioOk[4], g_scanWorldOk, g_juegoOk;

// ---------------------------------------------------------------- ENB
static uintptr_t g_delta; static int g_enbOk = -1;      // -1 sin resolver
static const uintptr_t VA_V1 = 0x101E3730, VA_PROJ = 0x1019D048, VA_COMPLEX = 0x1019B5C4, VA_LUZ = 0x101C0480;
static float g_M[4][4], g_Minv[4][4]; static int g_Mvalida;    // VP de la luz de la cascada 1 (vector fila x M)
static float g_gx[3], g_gy[3], g_gz[3], g_dir[3], g_cx, g_cy, g_lx, g_ly, g_lz;
static int g_enScan;

// ---------------------------------------------------------------- estadisticas por segundo
struct Seg { int fot, fallos[4], acept[4], rCascada, rPantalla, rTope, rPequena, rDist, sinEsfera, dist[4], sectores, escaneos, excep, noAfin; double usTest, usSect; int maxVis, maxLod; };
static Seg g_s; static double g_tSeg, g_intervaloMs = 1000.0; static int g_aceptFot;

// Posicion de la camara: CPlaceable::GetPosition (matriz en +0x14, pos en +0x30; si no, m_placement en +0x4)
static const float* camara_pos() {
    uintptr_t m = *(uintptr_t*)((uintptr_t)k_TheCamera + 0x14);
    return (const float*)(m ? m + 0x30 : (uintptr_t)k_TheCamera + 0x4);
}
static float* mf(uintptr_t va) { return (float*)(va + g_delta); }

static bool inversa4(const float m[4][4], float o[4][4]) {
    double a[4][8];
    for (int i = 0; i < 4; i++) for (int j = 0; j < 8; j++) a[i][j] = j < 4 ? m[i][j] : (j - 4 == i);
    for (int c = 0; c < 4; c++) {
        int p = c; for (int r = c + 1; r < 4; r++) if (fabs(a[r][c]) > fabs(a[p][c])) p = r;
        if (fabs(a[p][c]) < 1e-12) return false;
        if (p != c) for (int j = 0; j < 8; j++) std::swap(a[p][j], a[c][j]);
        double d = a[c][c]; for (int j = 0; j < 8; j++) a[c][j] /= d;
        for (int r = 0; r < 4; r++) if (r != c) { double f = a[r][c]; for (int j = 0; j < 8; j++) a[r][j] -= f * a[c][j]; }
    }
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) o[i][j] = (float)a[i][j + 4];
    return true;
}
static void transforma(const float p[3], float w, const float m[4][4], float o[4]) {
    for (int j = 0; j < 4; j++) o[j] = p[0] * m[0][j] + p[1] * m[1][j] + p[2] * m[2][j] + w * m[3][j];
}
static float len3(const float v[3]) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }

static int firma_abs_enb(uintptr_t va, uint8_t op, uintptr_t abs) {
    const uint8_t* p = (const uint8_t*)(va + g_delta);
    if (IsBadReadPtr(p, 5)) return 0;
    return p[0] == op && *(const uint32_t*)(p + 1) == (uint32_t)(abs + g_delta);
}
static void resolver_enb() {
    HMODULE m = GetModuleHandleA("enbseries.asi");
    if (!m) { g_enbOk = 0; L("ENB: enbseries.asi no cargado: el arreglo queda inactivo"); return; }
    g_delta = (uintptr_t)m - 0x10000000u;
    // push offset g_ShadowViewCascade1 en ENB_ShadowDrawFF y mov esi, offset PROJ en ENB_ComputeShadowCascades
    g_enbOk = firma_abs_enb(0x10039230, 0x68, VA_V1) && firma_abs_enb(0x10038A1B, 0xBE, VA_PROJ);
    L("ENB: enbseries.asi en %p (desplazamiento 0x%08X): %s", m, (unsigned)g_delta,
      g_enbOk ? "firmas de la cascada 1 y de la PROJ OK" : "FIRMA DISTINTA (no es la 0.430 conocida): el arreglo queda inactivo");
}

// Lee las matrices de ENB (las del ultimo calculo de cascadas, un fotograma antes) y las valida.
static void actualizar_cascada() {
    g_Mvalida = 0;
    if (g_enbOk < 0) resolver_enb();
    if (g_enbOk != 1) return;
    if (*(int*)(VA_COMPLEX + g_delta) == 0) return;
    float V[4][4], P[4][4], M[4][4];
    memcpy(V, mf(VA_V1), 64); memcpy(P, mf(VA_PROJ), 64);
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) M[i][j] = V[i][0] * P[0][j] + V[i][1] * P[1][j] + V[i][2] * P[2][j] + V[i][3] * P[3][j];
    // La VP de la luz es ortografica: ultima columna (0,0,0,1). Si no, la PROJ cambio sin recalcular la cascada.
    if (!(fabsf(M[0][3]) + fabsf(M[1][3]) + fabsf(M[2][3]) < 1e-3f && fabsf(M[3][3] - 1.f) < 1e-2f)) { g_s.noAfin++; return; }
    float gx[3] = { M[0][0], M[1][0], M[2][0] }, gy[3] = { M[0][1], M[1][1], M[2][1] }, gz[3] = { M[0][2], M[1][2], M[2][2] };
    float d[3] = { gx[1] * gy[2] - gx[2] * gy[1], gx[2] * gy[0] - gx[0] * gy[2], gx[0] * gy[1] - gx[1] * gy[0] };
    float ld = len3(d); if (!(ld > 1e-12f) || !(len3(gx) > 1e-9f) || !(len3(gy) > 1e-9f)) return;
    for (int k = 0; k < 3; k++) d[k] /= ld;
    if (d[2] > 0) for (int k = 0; k < 3; k++) d[k] = -d[k];   // hacia abajo: lejos del sol
    float Minv[4][4]; if (!inversa4(M, Minv)) return;
    // Centro de la cascada = camara cuantizada: su NDC da los limites (mitad derecha del mapa: x +-0,5, y +-1)
    const float* cam = camara_pos();
    float o[4]; transforma(cam, 1.f, M, o);
    memcpy(g_M, M, 64); memcpy(g_Minv, Minv, 64);
    memcpy(g_gx, gx, 12); memcpy(g_gy, gy, 12); memcpy(g_gz, gz, 12); memcpy(g_dir, d, 12);
    g_cx = o[0]; g_cy = o[1];
    g_lx = len3(gx); g_ly = len3(gy); g_lz = len3(gz);
    g_Mvalida = 1;
}

// ---------------------------------------------------------------- prueba por entidad
static int g_medirCostes = 0; // Detailed per-entity timing is optional, never gates the repair.
static int prueba_sombra(const float* cEsfera, float radio, double* us) {   // 0 no, 1 si, -1 excepcion
    LARGE_INTEGER t0={}, t1={}; if(g_medirCostes)QueryPerformanceCounter(&t0);
    int r = 0;
    __try {
        float c[3] = { cEsfera[0], cEsfera[1], cEsfera[2] };
        if (!(radio >= g_radioMin)) { g_s.rPequena++; r = 0; goto fin; }
        float o[4]; transforma(c, 1.f, g_M, o);
        const Ajuste& aj = g_aj[g_ajuste];
        float dc = 0;
        {   const float* cp = camara_pos(); float dx = c[0] - cp[0], dy = c[1] - cp[1], dz = c[2] - cp[2]; dc = sqrtf(dx * dx + dy * dy + dz * dz); }
        if (aj.distMax > 0 && dc - radio > aj.distMax) { g_s.rDist++; r = 0; goto fin; }
        float rr = radio + aj.margen;
        if (fabsf(o[0] - g_cx) > 0.5f + rr * g_lx || fabsf(o[1] - g_cy) > 1.f + rr * g_ly || o[2] < -rr * g_lz || o[2] > 1.f + rr * g_lz) {
            g_s.rCascada++; r = 0; goto fin;
        }
        if (!g_exigirPantalla) { r = 1; goto fin; }
        float caida = -g_dir[2]; if (caida < 1e-3f) caida = 1e-3f;
        // A: (r + caida) / |dz|. B/C: la sombra de lo mas alto de la esfera llega al suelo (base - caida) en (2r + caida) / |dz|
        float largo = std::min(g_largoMax, ((aj.alSuelo ? 2.f * radio : radio) + aj.caida) / caida);
        float paso = aj.alSuelo ? std::max(aj.pasoMin, largo / g_maxMuestras) : std::max(radio, aj.pasoMin);
        int n = (int)ceilf(largo / paso); if (n > g_maxMuestras) n = g_maxMuestras; if (n < 1) n = 1;
        paso = largo / n;
        for (int i = 0; i <= n; i++) {
            float p[3] = { c[0] + g_dir[0] * paso * i, c[1] + g_dir[1] * paso * i, c[2] + g_dir[2] * paso * i };
            if (k_SphereVis(k_TheCamera, p, radio + paso * 0.5f + aj.margen, k_matInversa)) {
                r = 1; g_s.dist[dc < 15 ? 0 : dc < 30 ? 1 : dc < 60 ? 2 : 3]++; goto fin;
            }
        }
        g_s.rPantalla++; r = 0;
    fin:;
    } __except (EXCEPTION_EXECUTE_HANDLER) { r = -1; }
    if(g_medirCostes){QueryPerformanceCounter(&t1); *us += 1e6 * (t1.QuadPart - t0.QuadPart) / g_f.QuadPart;}
    return r;
}

static bool hay_sitio_en_listas() {
    if (g_aceptFot >= g_maxAceptados) return false;
    if (g_listasVanilla && (*g_cntVisibles >= g_limiteVisibles || *g_cntLods >= g_limiteVisibles)) return false;
    return true;
}

// Engancha la llamada de GetIsOnScreen a IsSphereVisible: guarda la esfera (mundo) que el juego acaba de calcular.
static bool __fastcall h_EsferaGIOS(void* cam, void*, const float* c, float radio, void* mat) {
    g_escC[0] = c[0]; g_escC[1] = c[1]; g_escC[2] = c[2]; g_escR = radio; g_escOk = 1;
    return o_esferaGIOS(cam, c, radio, mat);
}

template <int N> static bool __fastcall h_IsOnScreen(void* e, void*) {
    g_escOk = 0;
    if (o_sitio[N](e)) return true;
    if (!g_activo || !g_enScan || !g_Mvalida || !g_sitioOn[N]) return false;
    g_s.fallos[N]++;
    if (!g_escOk) { g_s.sinEsfera++; return false; }
    if (!hay_sitio_en_listas()) { g_s.rTope++; return false; }
    float c[3] = { g_escC[0], g_escC[1], g_escC[2] };
    int r = prueba_sombra(c, g_escR, &g_s.usTest);
    if (r < 0) { g_s.excep++; g_activo = 0; L("EXCEPCION en la prueba de la entidad %p (sitio %s): arreglo DESACTIVADO", e, k_sitioNombre[N]); return false; }
    if (r) { g_s.acept[N]++; g_aceptFot++; }
    return r != 0;
}
static PIsOnScreen const k_hook[4] = { (PIsOnScreen)(void*)h_IsOnScreen<0>, (PIsOnScreen)(void*)h_IsOnScreen<1>,
                                       (PIsOnScreen)(void*)h_IsOnScreen<2>, (PIsOnScreen)(void*)h_IsOnScreen<3> };

// ---------------------------------------------------------------- sectores extra
static void escanear_sectores() {
    LARGE_INTEGER t0={}, t1={}; if(g_medirCostes)QueryPerformanceCounter(&t0);
    const float* cam = camara_pos();
    float dz = std::min(g_dir[2], -0.05f);   // sol casi rasante: lo limita SectoresRadioMax
    float xmin = 1e30f, xmax = -1e30f, ymin = 1e30f, ymax = -1e30f;
    for (int a = 0; a < 4; a++) {
        float ndc[3] = { g_cx + ((a & 1) ? 0.5f : -0.5f), g_cy + ((a & 2) ? 1.f : -1.f), 0.5f }, p0[4];
        transforma(ndc, 1.f, g_Minv, p0);
        if (fabsf(p0[3]) < 1e-6f) return;
        for (int k = 0; k < 3; k++) p0[k] /= p0[3];
        for (int z = 0; z < 2; z++) {
            float nivel = z ? cam[2] + g_secSobre : cam[2] - g_secBajo;
            float t = (nivel - p0[2]) / dz;
            float x = p0[0] + g_dir[0] * t, y = p0[1] + g_dir[1] * t;
            xmin = std::min(xmin, x); xmax = std::max(xmax, x); ymin = std::min(ymin, y); ymax = std::max(ymax, y);
        }
    }
    xmin = std::max(xmin - g_secMargen, cam[0] - g_secRadioMax); xmax = std::min(xmax + g_secMargen, cam[0] + g_secRadioMax);
    ymin = std::max(ymin - g_secMargen, cam[1] - g_secRadioMax); ymax = std::min(ymax + g_secMargen, cam[1] + g_secRadioMax);
    int x0 = std::max(0, (int)floorf(xmin * g_secEscala + g_secOffset)), x1 = std::min(g_secN - 1, (int)floorf(xmax * g_secEscala + g_secOffset));
    int y0 = std::max(0, (int)floorf(ymin * g_secEscala + g_secOffset)), y1 = std::min(g_secN - 1, (int)floorf(ymax * g_secEscala + g_secOffset));
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) { g_ScanSectorList(x, y); g_s.sectores++; }
    g_s.escaneos++;
    if(g_medirCostes){QueryPerformanceCounter(&t1); g_s.usSect += 1e6 * (t1.QuadPart - t0.QuadPart) / g_f.QuadPart;}
}

// v3: en el primer fotograma, con todos los plugins ya cargados (Mod Loader carga los suyos despues de los de la
// raiz), comprobar que nadie piso los 4 ganchos y que la tabla de modelos sigue donde se leyo.
static uintptr_t destino_e8(const uint8_t* p);
static void comprobar_instalacion() {
    int ok = 0;
    for (int i = 0; i < 4; i++) {
        if (!g_sitioOk[i]) continue;
        uintptr_t d = destino_e8(k_sitio[i]);
        if (k_sitio[i][0] == 0xE8 && d == (uintptr_t)(void*)k_hook[i]) ok++;
        else { g_sitioOk[i] = 0; L("AVISO: %s ya no va al arreglo (bytes %02X, destino %s): otro modulo lo cambio despues", k_sitioNombre[i], k_sitio[i][0], quien((void*)d)); }
    }
    bool esf = k_callEsfera[0] == 0xE8 && destino_e8(k_callEsfera) == (uintptr_t)(void*)h_EsferaGIOS;
    if (!esf) L("AVISO: la llamada de GetIsOnScreen a IsSphereVisible (0x53458B) ya no va al arreglo (bytes %02X, destino %s): sin esfera no se acepta nada",
                k_callEsfera[0], quien((void*)destino_e8(k_callEsfera)));
    L("Primer fotograma: %d de 4 ganchos de GetIsOnScreen intactos; captura de la esfera %s", ok, esf ? "intacta" : "PISADA");
}

static bool juego_en_primer_plano() {
    DWORD pid = 0; HWND w = GetForegroundWindow(); if (w) GetWindowThreadProcessId(w, &pid);
    return pid == GetCurrentProcessId();
}

static void __cdecl h_ScanWorld() {
    g_frame++;
    static bool teclaAntes;
    bool tecla = g_tecla && juego_en_primer_plano() && (GetAsyncKeyState(g_tecla) & 0x8000);
    if (tecla && !teclaAntes) { g_activo = !g_activo; L("TECLA: arreglo %s", g_activo ? "ACTIVADO" : "DESACTIVADO"); }
    teclaAntes = tecla;
    static bool tecla2Antes;
    bool tecla2 = g_teclaAjuste && juego_en_primer_plano() && (GetAsyncKeyState(g_teclaAjuste) & 0x8000);
    if (tecla2 && !tecla2Antes) {
        g_ajuste = (g_ajuste + 1) % 3; const Ajuste& a = g_aj[g_ajuste];
        L("TECLA: ajuste %c (margen %.1f, caida %.1f, paso min %.1f, distancia max %.0f, %s)", a.letra, a.margen, a.caida, a.pasoMin, a.distMax,
          a.alSuelo ? "sombra hasta el suelo" : "largo de la v1");
    }
    tecla2Antes = tecla2;
    g_aceptFot = 0;
    static bool comprobado;
    if (!comprobado) { comprobado = true; comprobar_instalacion(); }
    bool exterior = !g_soloExteriores || *k_currArea == 0;
    if (g_activo && exterior) actualizar_cascada(); else g_Mvalida = 0;
    static int primera = 1;
    if (primera && g_Mvalida) {
        primera = 0;
        const float* luz = mf(VA_LUZ);
        L("CASCADA 1 primera lectura: centro NDC (%.3f, %.3f) [esperado ~(0,5, 0)]; direccion de la sombra (%.3f, %.3f, %.3f); "
          "vector de luz de ENB (%.3f, %.3f, %.3f); metros por unidad NDC x=%.1f y=%.1f", g_cx, g_cy, g_dir[0], g_dir[1], g_dir[2],
          luz[0], luz[1], luz[2], 1.f / g_lx, 1.f / g_ly);
    }
    g_enScan = 1;
    o_ScanWorld();
    if (g_activo && g_Mvalida && g_escanearSectores && g_ScanSectorList) {
        __try { escanear_sectores(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { g_s.excep++; g_escanearSectores = 0; L("EXCEPCION en el escaneo de sectores extra: escaneo DESACTIVADO"); }
    }
    g_enScan = 0;
    if (g_listasVanilla) { g_s.maxVis = std::max(g_s.maxVis, (int)*g_cntVisibles); g_s.maxLod = std::max(g_s.maxLod, (int)*g_cntLods); }
    g_s.fot++;
    double t = ms();
    if (t - g_tSeg >= g_intervaloMs) {
        int f = std::max(1, g_s.fot);
        L("SEG %s %c fps=%.1f cascada=%s ndc=(%.2f,%.2f) | rechazados por el juego m1=%d m2=%d e1=%d e2=%d | aceptados por sombra m1=%d m2=%d e1=%d e2=%d "
          "(%.1f/fot; a <15 m=%.1f 15-30=%.1f 30-60=%.1f >60=%.1f) | descartes: cascada=%d pantalla=%d pequena=%d distancia=%d tope=%d sin esfera=%d | sectores extra=%.1f/fot | tiempos optativos (0=sin medir): prueba=%.3f ms/fot sectores=%.3f ms/fot | "
          "visibles max=%d lods max=%d | no afin=%d excep=%d",
          g_activo ? "ON " : "OFF", g_aj[g_ajuste].letra, g_s.fot * 1000.0 / (t - g_tSeg), g_Mvalida ? "si" : "no", g_cx, g_cy,
          g_s.fallos[0] / f, g_s.fallos[1] / f, g_s.fallos[2] / f, g_s.fallos[3] / f,
          g_s.acept[0] / f, g_s.acept[1] / f, g_s.acept[2] / f, g_s.acept[3] / f,
          (double)(g_s.acept[0] + g_s.acept[1] + g_s.acept[2] + g_s.acept[3]) / f,
          (double)g_s.dist[0] / f, (double)g_s.dist[1] / f, (double)g_s.dist[2] / f, (double)g_s.dist[3] / f,
          g_s.rCascada / f, g_s.rPantalla / f, g_s.rPequena / f, g_s.rDist / f, g_s.rTope, g_s.sinEsfera / f,
          (double)g_s.sectores / f, g_s.usTest / 1000.0 / f, g_s.usSect / 1000.0 / f, g_s.maxVis, g_s.maxLod, g_s.noAfin, g_s.excep);
        memset(&g_s, 0, sizeof g_s); g_tSeg = t;
    }
}

// ---------------------------------------------------------------- instalacion
static uintptr_t destino_e8(const uint8_t* p) { return (uintptr_t)p + 5 + *(const int32_t*)(p + 1); }
static bool en_modulo_ajeno(uintptr_t a) {
    HMODULE m = nullptr;
    return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)a, &m) && m &&
           m != GetModuleHandleA(nullptr) && m != (HMODULE)&__ImageBase;
}
static void escribir_e8(uint8_t* p, const void* destino) {
    DWORD o; VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &o);
    *(int32_t*)(p + 1) = (int32_t)((uintptr_t)destino - ((uintptr_t)p + 5));
    VirtualProtect(p, 5, o, &o); FlushInstructionCache(GetCurrentProcess(), p, 5);
}
// Acepta la llamada si va a la funcion original o a otro modulo cargado (encadena delante).
static bool tomar_llamada(uint8_t* p, uintptr_t original, void** salida, const char* nombre) {
    if (IsBadReadPtr(p, 5) || p[0] != 0xE8) { L("FIRMA: %s no es un call E8 (bytes %02X %02X %02X %02X %02X): no se toca", nombre, p[0], p[1], p[2], p[3], p[4]); return false; }
    uintptr_t d = destino_e8(p);
    if (d != original && !en_modulo_ajeno(d)) { L("FIRMA: %s va a %s, ni al original 0x%X ni a otro modulo: no se toca", nombre, quien((void*)d), (unsigned)original); return false; }
    *salida = (void*)d;
    L("%s: call -> %s%s", nombre, quien((void*)d), d == original ? " (original)" : " (OTRO MODULO: se encadena delante)");
    return true;
}

static bool leer_juego() {
    // La esfera se toma de la llamada de GetIsOnScreen a CCamera::IsSphereVisible (0x420C40) en 0x53458B. El resto de
    // GetIsOnScreen puede estar cambiado (el FLA salta desde 0x534546 a su lectura de la tabla de modelos).
    {   const uint8_t* g = (const uint8_t*)k_GetIsOnScreen;
        L("GetIsOnScreen 0x534540: %02X %02X %02X ... 0x534546: %02X %02X %02X %02X %02X%s", g[0], g[1], g[2], g[6], g[7], g[8], g[9], g[10],
          g[6] == 0xE9 ? (std::string(" (salto a ") + quien((void*)destino_e8(g + 6)) + ": tabla de modelos del FLA)").c_str() : "");
        void* d;
        if (!tomar_llamada(k_callEsfera, 0x420C40, &d, "GetIsOnScreen -> IsSphereVisible 0x53458B")) return false;
        o_esferaGIOS = (PSphereVis)d; g_esferaOk = 1;
    }
    // Cuadricula de sectores de ScanWorld: fmul [escala] en 0x55544E, fadd [offset] en 0x555462; push ScanSectorList en 0x555454
    const uint8_t* f1 = (const uint8_t*)0x55544E; const uint8_t* f2 = (const uint8_t*)0x555462; const uint8_t* ps = (const uint8_t*)0x555454;
    if (f1[0] != 0xD8 || f1[1] != 0x0D || f2[0] != 0xD8 || f2[1] != 0x05 || ps[0] != 0x68) {
        L("FIRMA: conversion a sectores de ScanWorld distinta (%02X %02X / %02X %02X / %02X): sin escaneo extra", f1[0], f1[1], f2[0], f2[1], ps[0]);
    } else {
        g_secEscala = **(float**)(f1 + 2); g_secOffset = **(float**)(f2 + 2); g_secN = (int)(2.f * g_secOffset + 0.5f);
        g_ScanSectorList = (PScanSector)(uintptr_t)*(const uint32_t*)(ps + 1);
        bool ok = g_secEscala > 0.001f && g_secEscala < 1.f && g_secN > 0 && g_secN <= 2000;
        L("Sectores: x*%.5f+%.1f (%.0f m, %dx%d); ScanSectorList = %s%s", g_secEscala, g_secOffset, 1.f / g_secEscala, g_secN, g_secN,
          quien((void*)g_ScanSectorList), ok ? "" : "  VALORES RAROS: sin escaneo extra");
        if (!ok) g_ScanSectorList = nullptr;
    }
    // Listas de visibles de AddEntityToRenderList: vanilla 1000 + 1000 sin comprobacion (OLA 'unlimited' las cambia)
    const uint8_t* a = (const uint8_t*)0x553521; const uint8_t* b = (const uint8_t*)0x5534ED;
    if (a[0] == 0xA1 && a[5] == 0x89 && a[6] == 0x34 && a[7] == 0x85 && *(const uint32_t*)(a + 8) == 0xB75898 &&
        b[0] == 0xA1 && b[5] == 0x89 && b[6] == 0x34 && b[7] == 0x85 && *(const uint32_t*)(b + 8) == 0xB748F8) {
        g_cntVisibles = (volatile int*)(uintptr_t)*(const uint32_t*)(a + 1); g_cntLods = (volatile int*)(uintptr_t)*(const uint32_t*)(b + 1);
        g_listasVanilla = 1;
        L("Listas de visibles vanilla (1000 + 1000): se acepta por sombra solo mientras haya menos de %d", g_limiteVisibles);
    } else L("Listas de visibles cambiadas por otro modulo (OLA?): sin tope por cuenta; tope por fotograma %d", g_maxAceptados);
    return true;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD razon, LPVOID) {
    if (razon != DLL_PROCESS_ATTACH) { if (razon == DLL_PROCESS_DETACH && g_log) { L("fin"); fclose(g_log); g_log = nullptr; } return TRUE; }
    DisableThreadLibraryCalls(h);
    QueryPerformanceFrequency(&g_f); QueryPerformanceCounter(&g_t0);
    char ruta[MAX_PATH]; GetModuleFileNameA((HMODULE)&__ImageBase, ruta, MAX_PATH);
    std::string base = ruta; size_t punto = base.rfind('.'); if (punto != std::string::npos) base.resize(punto);
    std::string ini = base + ".ini"; const char* I = ini.c_str(); const char* S = "ShadowCasterFix";
    auto fl = [&](const char* k, float def) { char b[32]; GetPrivateProfileStringA(S, k, "", b, sizeof b, I); return b[0] ? (float)atof(b) : def; };
    g_activo = GetPrivateProfileIntA(S, "Activo", 1, I);
    g_medirCostes = GetPrivateProfileIntA(S, "MedirCostes", 0, I);
    g_tecla = GetPrivateProfileIntA(S, "TeclaAlternar", 52, I);
    g_sitioOn[0] = g_sitioOn[1] = GetPrivateProfileIntA(S, "Mapa", 1, I);
    g_sitioOn[2] = GetPrivateProfileIntA(S, "PedsYVehiculos", 1, I);
    g_sitioOn[3] = GetPrivateProfileIntA(S, "ObjetosSinStreaming", 1, I);
    g_escanearSectores = GetPrivateProfileIntA(S, "EscanearSectores", 1, I);
    g_exigirPantalla = GetPrivateProfileIntA(S, "ExigirSombraEnPantalla", 1, I);
    g_soloExteriores = GetPrivateProfileIntA(S, "SoloExteriores", 1, I);
    g_maxAceptados = GetPrivateProfileIntA(S, "MaxAceptadosPorFotograma", 400, I);
    g_limiteVisibles = GetPrivateProfileIntA(S, "LimiteListaVisibles", 800, I);
    g_maxMuestras = std::max(1, (int)GetPrivateProfileIntA(S, "MaxMuestras", 12, I));
    g_intervaloMs = 1000.0 * std::max(1, (int)GetPrivateProfileIntA(S, "IntervaloRegistro", 1, I));
    g_largoMax = fl("LargoMaxSombra", 120.f); g_radioMin = fl("RadioMinimo", 1.f);
    {   char b[8]; GetPrivateProfileStringA(S, "Ajuste", "B", b, sizeof b, I); g_ajuste = (b[0] == 'A' || b[0] == 'a') ? 0 : (b[0] == 'C' || b[0] == 'c') ? 2 : 1; }
    g_teclaAjuste = GetPrivateProfileIntA(S, "TeclaAjuste", 51, I);
    for (int k = 1; k < 3; k++) {   // B y C desde el .ini; A es siempre la v1
        g_aj[k].margen = fl("MargenMetros", 1.5f); g_aj[k].caida = fl("CaidaSombra", 3.f); g_aj[k].pasoMin = std::max(0.5f, fl("PasoMin", 2.f));
    }
    g_aj[2].distMax = fl("DistanciaMaxima", 40.f);
    g_margen = g_aj[g_ajuste].margen; g_caida = g_aj[g_ajuste].caida; g_pasoMin = g_aj[g_ajuste].pasoMin;
    g_secBajo = fl("SectoresBajoCamara", 60.f); g_secSobre = fl("SectoresSobreCamara", 120.f);
    g_secMargen = fl("SectoresMargen", 30.f); g_secRadioMax = fl("SectoresRadioMax", 400.f);
    {   WIN32_FILE_ATTRIBUTE_DATA fa;   // conservar el log anterior
        if (GetFileAttributesExA((base + ".log").c_str(), GetFileExInfoStandard, &fa)) {
            FILETIME lt; SYSTEMTIME st; FileTimeToLocalFileTime(&fa.ftLastWriteTime, &lt); FileTimeToSystemTime(&lt, &st);
            char viejo[MAX_PATH]; snprintf(viejo, sizeof viejo, "%s_%04d%02d%02d_%02d%02d%02d.log", base.c_str(), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
            MoveFileExA((base + ".log").c_str(), viejo, MOVEFILE_REPLACE_EXISTING);
        }
    }
    g_log = fopen((base + ".log").c_str(), "w");
    L("ShadowCasterFix v4.1. Ajuste=%c TeclaAjuste=%d DistanciaMaxima(C)=%.0f IntervaloRegistro=%.0f s", g_aj[g_ajuste].letra, g_teclaAjuste, g_aj[2].distMax, g_intervaloMs / 1000.0);
    L("Activo=%d Tecla=%d Mapa=%d PedsYVehiculos=%d ObjetosSinStreaming=%d EscanearSectores=%d ExigirSombraEnPantalla=%d "
      "Margen=%.1f Caida=%.1f LargoMax=%.1f PasoMin=%.1f MaxMuestras=%d RadioMinimo=%.1f MaxAceptados=%d LimiteListaVisibles=%d",
      g_activo, g_tecla, g_sitioOn[0], g_sitioOn[2], g_sitioOn[3], g_escanearSectores, g_exigirPantalla, g_margen, g_caida, g_largoMax,
      g_pasoMin, g_maxMuestras, g_radioMin, g_maxAceptados, g_limiteVisibles);
    {   char exe[MAX_PATH]; GetModuleFileNameA(nullptr, exe, MAX_PATH);
        const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)0x400000;   // imagen de gta_sa.exe (base fija)
        const IMAGE_NT_HEADERS* nt = (const IMAGE_NT_HEADERS*)((const uint8_t*)dos + dos->e_lfanew);
        if (nt->FileHeader.TimeDateStamp != 0x427101CA) { L("FIRMA: %s no es gta_sa.exe 1.0 US (marca 0x%08X): no se toca nada", exe, (unsigned)nt->FileHeader.TimeDateStamp); return TRUE; }
    }
    if (!leer_juego()) { L("No se toca nada."); return TRUE; }
    escribir_e8(k_callEsfera, (void*)h_EsferaGIOS);
    void* d;
    if (!tomar_llamada(k_callScanWorld, k_ScanWorld, &d, "ConstructRenderList -> ScanWorld 0x5558DD")) { L("Sin el gancho de ScanWorld el arreglo no puede actuar: no se toca nada."); return TRUE; }
    o_ScanWorld = (PScanWorld)d;
    int n = 0;
    for (int i = 0; i < 4; i++) if (tomar_llamada(k_sitio[i], k_GetIsOnScreen, &d, k_sitioNombre[i])) { o_sitio[i] = (PIsOnScreen)d; g_sitioOk[i] = 1; n++; }
    if (!n) { L("Ningun sitio de GetIsOnScreen reconocible: no se toca nada."); return TRUE; }
    for (int i = 0; i < 4; i++) if (g_sitioOk[i]) escribir_e8(k_sitio[i], (void*)k_hook[i]);
    escribir_e8(k_callScanWorld, (void*)h_ScanWorld);
    g_scanWorldOk = 1; g_juegoOk = 1;
    L("Instalado: %d de 4 llamadas a GetIsOnScreen y la llamada a ScanWorld. ENB se resuelve en el primer fotograma.", n);
    return TRUE;
}
