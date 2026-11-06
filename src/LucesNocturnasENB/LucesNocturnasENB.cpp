// LucesNocturnasENB.SA.asi - rama de iluminacion nocturna (2026-10-02). GTA SA 1.0 US + ENBSeries 0.430.
//
// QUE HACE (cada parte se apaga en el .ini):
//  1. [Coronas] Dibuja CCoronas::Render DESPUES de la composicion de ENB (enblighting + 1.fx) y antes
//     de su postproceso. Antes, el juego las dibujaba debajo y ENB las borraba: contra el cielo las
//     tapaba el cielo procedural de 1.fx (linea 1351) y sobre geometria las aplastaba la reiluminacion.
//     Demostrado en la prueba 4 (DIAGNOSTICO-enb-iluminacion-nocturna.md, seccion 12).
//  2. [Luces] Luces locales diferidas (1.4: tambien las farolas lejanas o fuera de pantalla, por el escaner): con la profundidad INTZ de la escena y las point lights que el
//     juego registra cada fotograma (CPointLights::aLights, 0xC3F0E0; farolas, faros...), suma luz
//     difusa (N.L, atenuacion por radio) a suelo, paredes, peds y coches, en el mismo punto que las
//     coronas. Se dibuja con el dispositivo REAL (sin pasar por ENB). El albedo aproximado es una copia
//     de la escena del juego tomada justo antes de las coronas (antes de que ENB reilumine).
//  3. [Capturas] Mueve las capturas de ENB (enb*.bmp de la raiz) a otra carpeta y muestra
//     "CAPTURA TOMADA" con el texto del juego (CFont, encima del HUD).
//
// ENGANCHES (todos con comprobacion de los bytes vivos; si no cuadran, esa parte no se instala):
//   0x53E18E  call CCoronas::Render      (RenderEffects)        -> h_coronas
//   0x704732  call RsCameraBeginUpdate   (CPostEffects::Render)  -> h_begin (ENB ya compuso y el juego
//             reactivo la camara de RW tras la copia; 1.0 usaba 0x704726 y crasheaba con los destellos)
//   0x53E227  jmp  CPostEffects::Render  (RenderEffects)        -> h_post (respaldo)
//   0x53E52B  jmp  CFont::DrawFonts      (Render2dStuff)        -> h_fuentes (aviso + capturas)
//   vtable del envoltorio de ENB, entrada 16 (Reset)            -> h_reset (libera recursos DEFAULT)
//   0x6FD105/0x6FD347 call CPointLights::AddLight (ProcessLightsForEntity) -> h_addlight (1.2: anota)
// 1.4: escaner de farolas (pools de edificios/dummies/objetos) y diferido clasico (normal en textura +
//      un rectangulo por luz, hasta 256 luces).
// 2.0a (F1, a partir de Proper Shaders 08-26): [Modelos] admite cono y altura (farolas como focos hacia
//      abajo, tipo 2); [Especular] brillo de las luces en el suelo mojado (CWeather::WetRoads 0xC81308),
//      a un segundo destino que se compone sin albedo; atenuacion smoothstep opcional.
// 2.0f (05-10-2026): LNE_FactorModelo, factor de intensidad por modelo puesto en caliente por otro plugin.
// 2.0i (28-10-2026, aportes de Deferred SkyGFX, proyecto skygfx-aportes-directx-2026-10-28):
//      (1) las particulas FX (humo, escape, polvo) se dibujan DESPUES de la composicion de ENB, como las coronas: antes,
//      1.fx sustituia por su cielo y su niebla todo pixel con fondo lejano y las borraba; (2) lluvia iluminada (rainLit de
//      SkyGFX): estelas alrededor de la camara que se ven a la luz de faros y farolas, nunca en interiores (area, zonas
//      sin lluvia del juego, bajo el agua y prueba de techo por columnas); sustituye a las 32 estelas del juego, que ENB
//      borraba; (3) sombras de los faros por trazado en pantalla; (4) haces con el brillo por clima de lampHaze de SkyGFX;
//      (5) LNE_LucesCuadro: luces del cuadro para HumoLucesENB.
// 2.0j (29-10-2026): (1) lluvia de nueva generacion ([Lluvia] Modo=2, lluvia2.h + lluvia.hlsl): 3000 gotas animadas en la
//      GPU con la forma, la normal y la refraccion de RainStreaks de Proper Shaders 08-26 (texturas RainDrop incrustadas),
//      iluminadas por las luces cercanas, salpicaduras en el suelo y cortinas de lluvia lejanas; mismas reglas que la 2.0i
//      (sin interiores, zonas sin lluvia, agua ni techo). Modo=1 = rainLit de la 2.0i. (2) [Haces] ClimaSkyGFX=0 por
//      defecto: el usuario prefiere los haces de la 2.0h (con niebla el lampHaze x8,9 dejaba un cono artificial).
//      (3) La linea SEG anota el clima del juego (CWeather::OldWeatherType/NewWeatherType/InterpolationValue).
// 2.0h (06-10-2026): LNE_FactorModeloBlancas, el mismo factor pero solo para las luces blancas o casi blancas del modelo
//      (saturacion (max-min)/max < 0,35). Vice City Viva baja asi las cuatro rosa palido del Pole Position (254,188,216 y
//      255,191,218, que con el tope de luminancia se ven blancas) sin tocar las de color de los mismos modelos.
// 2.0g (05-10-2026): RUIDO en las superficies iluminadas, sobre todo en Vice City y Liberty City. El shader reconstruia la
//      posicion EN EL MUNDO desde la profundidad con clip x inv(View x Proj) en float32: con la camara en X 4000-7700 los
//      terminos de la traslacion se cancelan y el error de redondeo (cm) cambia de pixel a pixel y de cuadro a cuadro;
//      la normal suavizada salia girada hasta 40-60 grados (simulado: 0,01 grados en el origen). Ahora todo va RELATIVO A
//      LA CAMARA: matriz View sin traslacion (invertida en double), camara = (0,0,0) en el shader y posiciones de las
//      luces y haces restadas en la CPU. El shader no cambia.
// 2.0b (F2): [Haces] haces volumetricos bajo los focos de farola (shader VolumetricLights de Proper Shaders,
//      reescrito), mas densos con niebla (CWeather::Foggyness 0xC81300) y lluvia (CWeather::Rain 0xC81324).
//      Opcional: quitar la niebla de luces vanilla (CPointLights::RenderFogEffect 0x7002D0 -> ret).
// 2.0c (F3): [Faros] dos focos por coche en los dummies reales (geometria de Proper Shaders) y [Frenos] luces de
//      freno y marcha atras, SIN ganchos nuevos: el faro del juego (CVehicle::DoVehicleLights, AddLight en 0x6E27E6:
//      tipo 1, radio 20, blanco, en el centro del coche y SIN entidad) se empareja por posicion con el pool de
//      vehiculos (0xB74494, paso 0xA18 leido de 0x550011); la luz de freno del juego (0x6E28E7: tipo 1, radio 10,
//      (0,1; 0,02; 0,02), hacia atras, CON la entidad) da el coche directamente. Dummies: CVehicleModelInfo+0x5C
//      -> m_avDummyPos (0 = faros, 1 = pilotos traseros), como DoHeadLightBeam (0x6E0E34).
// 2.0d (F4): [Disparos] [Explosiones] [Fuego]. Se anotan ANTES del filtro de ~27 m de AddLight las llamadas de
//      FireInstantHit (0x740D68), FireInstantHitFromCar2 (0x73CCFD), FireOneInstantHitRound (0x73AF74),
//      DoPlaneGunFireFX (0x6D4D14), CExplosion::Update (0x737849, 0x7378C1) y CFire::ProcessFire (0x53AEAC).
//      Disparos y explosiones son luces con vida (Duracion y Caida exponencial de Proper Shaders; cada llamada
//      del juego en el mismo sitio la renueva); el fuego se anota cada cuadro, como las farolas.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string>
#include <vector>
#include "IndiceLuces.h"
#include <unordered_set>
#include <unordered_map>
#include "luces_vs.h"
#include "luces_vsl.h"
#include "luces_psn.h"
#include "luces_psd.h"
#include "luces_psl.h"
#include "luces_psc.h"
#include "luces_psh.h"
#include "luces_psr.h"
#include "lluvia2.h"
extern "C" IMAGE_DOS_HEADER __ImageBase;

// ---------------------------------------------------------------- configuracion y registro
static FILE* g_log; static std::string g_ini, g_dirJuego;
static LARGE_INTEGER g_f, g_t0;
static double ms() { LARGE_INTEGER t; QueryPerformanceCounter(&t); return 1000.0 * (t.QuadPart - g_t0.QuadPart) / g_f.QuadPart; }
static void L(const char* f, ...) {
    if (!g_log) return;
    SYSTEMTIME s; GetLocalTime(&s);
    fprintf(g_log, "[%02d:%02d:%02d] ", s.wHour, s.wMinute, s.wSecond);
    va_list a; va_start(a, f); vfprintf(g_log, f, a); va_end(a); fputc('\n', g_log); fflush(g_log);
}
static int   IniI(const char* s, const char* k, int d) { return GetPrivateProfileIntA(s, k, d, g_ini.c_str()); }
static float IniF(const char* s, const char* k, float d) { char b[64]; GetPrivateProfileStringA(s, k, "", b, 64, g_ini.c_str()); return b[0] ? (float)atof(b) : d; }
static std::string IniS(const char* s, const char* k, const char* d) { char b[MAX_PATH]; GetPrivateProfileStringA(s, k, d, b, MAX_PATH, g_ini.c_str()); return b; }

static int g_coronasTras = 1, g_luces = 1, g_luzFaros = 1, g_capturas = 1, g_teclas = 0, g_tecLuces = 0, g_tecCoronas = 0;
static float g_ganancia = 3.0f, g_radioMult = 1.0f, g_albedoMin = 0.15f, g_distMax = 250.0f, g_expAt = 2.0f, g_envolvente = 0.35f, g_pesoAlb = 1.0f, g_cosFaro = 0.75f;
// 1.1: farolas y demas luces puntuales (tipo 0) con ganancia y radio propios; los faros (tipo 1) usan g_ganancia/g_radioMult.
static float g_ganFarolas = 8.0f, g_radioFarolas = 1.5f; static int g_W = 1920, g_H = 1080, g_diagMrt = 0;
// 1.2: luces de farola anotadas en CEntity::ProcessLightsForEntity ANTES del filtro de CPointLights::AddLight
// (que descarta lo que esta a mas de radio + 15 m de la camara en X o Y, 0x700103, y admite solo 32).
static const int MAXL = 256; static int g_maxLuces = 256; static long g_rectUlt = 0;
struct LuzReg { float pos[3], radio, r, g, b; uint16_t modelo; };
static LuzReg g_reg[1024], g_regCuadro[1024]; static volatile long g_nReg = 0; static int g_nRegCuadro = 0;
static int g_depuracion = 0; static float g_radioNormal = 0.08f, g_capFarolas = 2.5f;
static long g_regMax = 0;
typedef void(__cdecl* PAddLight)(int, float, float, float, float, float, float, float, float, float, float, int, int, void*);
static const PAddLight CPointLights_AddLight = (PAddLight)0x7000E0;
// 1.7: la entidad que procesa ProcessLightsForEntity esta en ebp (0x6FC7AA mov ebp, ecx) al llamar a AddLight;
// el puente la guarda para saber el MODELO de cada luz ([Modelos] del .ini).
static uint8_t* volatile g_entAnot;
static void __cdecl h_addlight(int tipo, float px, float py, float pz, float dx, float dy, float dz, float radio, float r, float g, float b, int niebla, int sombras, void* ent) {
    if ((tipo & 0xFF) == 0 && radio > 0.0f && (r + g + b) > 0.0f && g_nReg < 1024) {
        LuzReg& l = g_reg[g_nReg++]; l.pos[0] = px; l.pos[1] = py; l.pos[2] = pz; l.radio = radio; l.r = r; l.g = g; l.b = b;
        uint8_t* e = g_entAnot; l.modelo = e ? *(uint16_t*)(e + 0x22) : 0xFFFF;
    }
    CPointLights_AddLight(tipo, px, py, pz, dx, dy, dz, radio, r, g, b, niebla, sombras, ent);
}
__declspec(naked) static void h_addlight_puente() { __asm { mov g_entAnot, ebp } __asm { jmp h_addlight } }
// 1.5: semaforos. CTrafficLights::DisplayActualLight (0x49DAB0) anade su point light en 0x49DF47 (radio 14,
// color de tabla x 0,00157); pasaba por el filtro de ~27 m de AddLight y recibia la ganancia de las farolas.
static LuzReg g_regSem[256], g_regSemCuadro[256]; static volatile long g_nRegSem = 0; static int g_nRegSemCuadro = 0;
static float g_ganSem = 4.0f, g_radioSem = 1.0f;
static void __cdecl h_addlight_sem(int tipo, float px, float py, float pz, float dx, float dy, float dz, float radio, float r, float g, float b, int niebla, int sombras, void* ent) {
    if ((tipo & 0xFF) == 0 && radio > 0.0f && (r + g + b) > 0.0f && g_nRegSem < 256) {
        LuzReg& l = g_regSem[g_nRegSem++]; l.pos[0] = px; l.pos[1] = py; l.pos[2] = pz; l.radio = radio; l.r = r; l.g = g; l.b = b; l.modelo = 0xFFFF;
    }
    CPointLights_AddLight(tipo, px, py, pz, dx, dy, dz, radio, r, g, b, niebla, sombras, ent);
}
static std::string g_dirCapturas;

// ---------------------------------------------------------------- utilidades de parcheo
static void escribe(uintptr_t p, const void* b, int n) {
    DWORD o; VirtualProtect((void*)p, n, PAGE_EXECUTE_READWRITE, &o); memcpy((void*)p, b, n); VirtualProtect((void*)p, n, o, &o);
    FlushInstructionCache(GetCurrentProcess(), (void*)p, n);
}
static int rel_a(uintptr_t s, uint8_t op, uintptr_t dst) { return *(uint8_t*)s == op && (uintptr_t)(s + 5 + *(int32_t*)(s + 1)) == dst; }
static void pon_rel(uintptr_t s, uint8_t op, void* dst) { uint8_t b[5]; b[0] = op; *(int32_t*)(b + 1) = (int32_t)((uintptr_t)dst - (s + 5)); escribe(s, b, 5); }

// ---------------------------------------------------------------- funciones del juego (1.0 US)
typedef void(__cdecl* PFv)(); typedef void*(__cdecl* PFpop)(); typedef void*(__cdecl* PFpush)(void*); typedef void*(__cdecl* PFfast)(void*, int, int);
static const PFv    CCoronas_Render = (PFv)0x6FAEC0, CPostEffects_Render = (PFv)0x7046E0, CFont_DrawFonts = (PFv)0x71A210;
static const PFpop  RwRasterPopContext = (PFpop)0x7FB110;
static const PFpush RwRasterPushContext = (PFpush)0x7FB060;
static const PFfast RwRasterRenderFast = (PFfast)0x7FAF50;
struct CRGBA { uint8_t r, g, b, a; };
typedef void(__cdecl* PFscale)(float, float); typedef void(__cdecl* PFcol)(CRGBA); typedef void(__cdecl* PFint)(int); typedef void(__cdecl* PFbool)(bool);
typedef void(__cdecl* PFbg)(bool, bool); typedef void(__cdecl* PFsh)(short); typedef void(__cdecl* PFflt)(float); typedef void(__cdecl* PFprint)(float, float, const char*);
static const PFscale F_SetScale = (PFscale)0x719380; static const PFcol F_SetColor = (PFcol)0x719430, F_SetDropColor = (PFcol)0x719510;
static const PFint F_SetFontStyle = (PFint)0x719490, F_SetOrientation = (PFint)0x719610; static const PFbool F_SetProportional = (PFbool)0x7195B0, F_SetJustify = (PFbool)0x719600;
static const PFbg F_SetBackground = (PFbg)0x7195C0; static const PFsh F_SetDropShadow = (PFsh)0x719570, F_SetEdge = (PFsh)0x719590;
static const PFflt F_SetWrapx = (PFflt)0x7194D0; static const PFprint F_PrintString = (PFprint)0x71A700;

struct PuntoLuz { float pos[3], dir[3], radio, r, g, b; void* ent; uint8_t tipo, niebla, sombras, pad; };
static_assert(sizeof(PuntoLuz) == 0x30, "CPointLight");

// ---------------------------------------------------------------- dispositivo real y recursos
static IDirect3DDevice9* g_real; static uintptr_t g_enbIni, g_enbFin;
static IDirect3DVertexShader9* g_vs, *g_vsL; static IDirect3DPixelShader9* g_psN, *g_psD, *g_psL; static IDirect3DVertexDeclaration9* g_decl, *g_declL;
static IDirect3DTexture9* g_norm, *g_acc; static UINT g_normW, g_normH, g_accW, g_accH; static IDirect3DPixelShader9* g_psC;
// 1.6: clases de luz, alcance maximo, agrupado y suma con rodilla
static float g_ganOtras = 2.0f, g_radioOtras = 1.0f, g_radioMaxLuz = 15.0f, g_fusion = 4.0f, g_rodilla = 1.2f, g_luzMax = 3.0f, g_fuente = 0.2f;
static long g_fusionadasUlt = 0;
// 1.7: tope por LUMINANCIA (una luz blanca ya no brilla mas que una farola naranja) y factores por modelo
static float g_lumMax = 1.6f;
// 2.0a: cono (coseno exterior; -2 = sin cono) y desplazamiento vertical por modelo
struct FactorModelo { float intensidad, radio, cono, altura; };
static int g_focos = 1, g_espActivo = 1, g_atenModo = 0, g_tecFases = 0, g_mrtOk = -1;
static float g_espInt = 0.5f, g_espNMin = 20.0f, g_espNMax = 60.0f, g_anchoBorde = 0.12f, g_mojadoUlt = 0.0f;
static long g_focosUlt = 0;
static IDirect3DTexture9* g_esp; static UINT g_espW, g_espH;
// 2.0b: haces volumetricos
static int g_haces = 1, g_hazMax = 16, g_hazFaros = 0, g_hazSem = 0, g_quitaNiebla = 0;
static float g_hazLargo = 9.0f, g_hazRadio = 4.0f, g_hazAngInt = 10.0f, g_hazAngExt = 60.0f, g_hazInt = 0.3f, g_hazDens = 0.5f;
static float g_hazNiebla = 2.0f, g_hazLluvia = 1.0f, g_hazRMin = 6.0f, g_hazRMax = 35.0f, g_hazBoostUlt = 1.0f;
static long g_hacesUlt = 0; static IDirect3DPixelShader9* g_psH;
// 2.0i
static int g_partTras = 1, g_lluvia = 1, g_lluviaJuego = 0, g_sombFaros = 1, g_sombPasos = 12, g_hazClima = 1, g_lluN = 800;
static float g_sombGrosor = 1.5f, g_sombSesgo = 0.12f, g_sombFuerza = 0.9f, g_hazB[4] = { 0.28f, 0.60f, 1.00f, 2.50f };
static float g_lluDist = 25.0f, g_lluVel = 30.0f, g_lluAng = 10.0f, g_lluViento = 0.30f, g_lluLargo = 0.6f, g_lluGrosor = 1.5f, g_lluOpac = 0.8f;
static float g_lluTinte[3] = { 0.90f, 0.95f, 1.00f }, g_lluInt = 1.0f, g_lluAmb = 0.35f;
static IDirect3DPixelShader9* g_psR; static long g_fxDiferidos = 0, g_lluviaUlt = 0, g_techosUlt = 0, g_lluJuegoDif = 0;
// 2.0j: lluvia nueva (lluvia2.h). g_lluModo: 0 nada, 1 rainLit de la 2.0i, 2 lluvia nueva
static int g_lluModo = 2, g_accOk = 0; static float g_projA = 1.0f, g_projB = 0.0f, g_projYs = 1.0f, g_lluAmbMin[3] = { 0.045f, 0.050f, 0.060f };
static lluvia2::Config g_ll2Cfg; static lluvia2::Estado g_ll2;
// 2.0c: faros dobles, frenos y marcha atras
static int g_farosDobles = 1, g_frenos = 1, g_marchaAtras = 1, g_vehOk = -1; static uint32_t g_vehPaso = 0;
static float g_faroAdel = 0.35f, g_faroAlto = 0.1f, g_faroIncl = 20.0f, g_ganFrenos = 4.0f, g_radioFrenos = 0.7f, g_cosFreno = 0.5f, g_frenoIncl = 20.0f, g_intMarcha = 0.05f, g_radioMarcha = 7.0f;
static long g_farosDoblesUlt = 0, g_frenosUlt = 0, g_marchaUlt = 0;
struct Veh { uint8_t* v; float pos[3]; };
static std::vector<Veh> g_veh;
// 2.0d: disparos, explosiones y fuego
struct Vida { float pos[3], radio; uint32_t t; int clase; };
static std::vector<Vida> g_vida; static LuzReg g_fuego[256], g_fuegoCuadro[256]; static volatile long g_nFuego = 0; static int g_nFuegoCuadro = 0;
static int g_disparos = 1, g_explos = 1, g_fuegoAct = 1, g_sinOriginal = 0, g_ganchos20d = 0;
static float g_dispRadio = 8.0f, g_dispInt = 1.5f, g_dispDur = 0.075f, g_dispCaida = 6.0f, g_dispCol[3] = { 1.0f, 0.6f, 0.25f };
static float g_expRadio = 1.5f, g_expInt = 3.0f, g_expDur = 0.6f, g_expCaida = 5.0f, g_expCol[3] = { 1.0f, 0.5f, 0.15f }, g_expAlto = 1.5f;
static float g_fuegoGan = 2.0f, g_fuegoAlto = 1.5f, g_fuegoRadio = 1.0f;
static long g_disparosUlt = 0, g_explosUlt = 0, g_fuegoUlt = 0, g_disparosTot = 0, g_explosTot = 0;
static std::unordered_map<uint16_t, FactorModelo> g_factores; static int g_factoresLeidos = 0;
// 2.0f (05-10-2026): factor de intensidad por modelo que pone otro plugin en caliente con LNE_FactorModelo (Vice City
// Viva: las luces interiores del Club Malibu se apagan poco a poco de 5 a 21 y las de la barra y el piso de arriba van mas
// bajas). Se aplica como el de [Modelos], despues de los topes (si no, el tope de intensidad lo anularia).
static float g_factorExt[65536]; static uint8_t g_factorExtUsa[65536];
// 2.0h (06-10-2026): factor externo solo para las luces poco saturadas del modelo (LNE_FactorModeloBlancas)
static float g_factorBla[65536]; static uint8_t g_factorBlaUsa[65536];
static const float kSaturacionBlanca = 0.35f;
// 1.8: curva horaria. La adaptacion de ENB (enbseries.ini [ADAPTATION] AdaptationMax=1.8, [NIGHTDAY]
// DetectorLevelNight=0.15) aclara mucho mas la imagen de 1 a 4 que a las 22-23, y amplifica la luz que este
// plugin suma ANTES de su postproceso (prueba 13: calle y fachada 2-4,8 veces mas brillantes que a las 23:05).
// Factor por hora del juego (0..23), interpolado con los minutos; [Horas] del .ini.
static float g_horas[24] = { 0.5f, 0.35f, 0.32f, 0.37f, 0.5f, 0.75f, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1.0f, 0.85f };
static float g_factorHora = 1.0f; static int g_horaFaros = 0, g_horaActiva = 1;
static void calcula_factor_hora() {
    g_factorHora = 1.0f;
    if (!g_horaActiva) return;
    int h = *(uint8_t*)0xB70153, m = *(uint8_t*)0xB70152;     // CClock::ms_nGameClockHours / Minutes (como 0x6FAB30)
    if (h < 0 || h > 23 || m > 59) return;
    float f = m / 60.0f;
    g_factorHora = g_horas[h] * (1.0f - f) + g_horas[(h + 1) % 24] * f;
}
static IDirect3DTexture9* g_alb; static UINT g_albW, g_albH; static D3DFORMAT g_albF;
static IDirect3DStateBlock9* g_sb;
static float g_vp[16], g_cam[3]; static int g_vpValido = 0, g_albValido = 0;
static float g_vpRel[16];   // 2.0g: View sin traslacion x Proj (reconstruccion relativa a la camara)
static volatile int g_pendiente = 0, g_fallos = 0;
static long g_nCoronas = 0, g_nLuces = 0, g_nRespaldo = 0, g_lucesUlt = 0;

static IDirect3DDevice9* real_dev() {
    if (g_real) return g_real;
    IDirect3DDevice9* juego = *(IDirect3DDevice9**)0xC97C28;
    if (!juego) return nullptr;
    __try {
        uintptr_t vt = *(uintptr_t*)juego;
        if (g_enbIni && vt >= g_enbIni && vt < g_enbFin) g_real = *(IDirect3DDevice9**)((uintptr_t)juego + 8);
        else g_real = juego;
    } __except (EXCEPTION_EXECUTE_HANDLER) { g_real = nullptr; }
    if (g_real) L("Dispositivo del juego %p, real %p (%s)", juego, g_real, g_real == juego ? "sin envoltorio de ENB" : "envoltorio de ENB +8");
    return g_real;
}
static void libera_default() {
    if (g_alb) { g_alb->Release(); g_alb = nullptr; } g_albValido = 0;
    if (g_norm) { g_norm->Release(); g_norm = nullptr; }
    if (g_acc) { g_acc->Release(); g_acc = nullptr; }
    if (g_esp) { g_esp->Release(); g_esp = nullptr; }
    if (g_sb) { g_sb->Release(); g_sb = nullptr; }
}
static int crea_recursos(IDirect3DDevice9* d) {
    if (!g_vs && FAILED(d->CreateVertexShader((const DWORD*)g_vsLuces, &g_vs))) { L("ERROR CreateVertexShader"); return 0; }
    if (!g_vsL && FAILED(d->CreateVertexShader((const DWORD*)g_vsLuz, &g_vsL))) { L("ERROR CreateVertexShader (luz)"); return 0; }
    if (!g_psN && FAILED(d->CreatePixelShader((const DWORD*)g_psNormal, &g_psN))) { L("ERROR CreatePixelShader (normal)"); return 0; }
    if (!g_psD && FAILED(d->CreatePixelShader((const DWORD*)g_psDepuracion, &g_psD))) { L("ERROR CreatePixelShader (depuracion)"); return 0; }
    if (!g_psL && FAILED(d->CreatePixelShader((const DWORD*)g_psLuz, &g_psL))) { L("ERROR CreatePixelShader (luz)"); return 0; }
    if (!g_psC && FAILED(d->CreatePixelShader((const DWORD*)g_psCompone, &g_psC))) { L("ERROR CreatePixelShader (composicion)"); return 0; }
    if (!g_psH && FAILED(d->CreatePixelShader((const DWORD*)g_psHaz, &g_psH))) { L("ERROR CreatePixelShader (haces); haces apagados"); g_haces = 0; }
    if (!g_psR && FAILED(d->CreatePixelShader((const DWORD*)g_psLluvia, &g_psR))) { L("ERROR CreatePixelShader (lluvia); lluvia iluminada apagada"); g_lluvia = 0; }
    if (!g_declL) {
        D3DVERTEXELEMENT9 e[] = { {0, 0, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_POSITION, 0}, {0, 16, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_TEXCOORD, 0},
                                  {0, 32, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_TEXCOORD, 1}, {0, 48, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_TEXCOORD, 2}, D3DDECL_END() };
        if (FAILED(d->CreateVertexDeclaration(e, &g_declL))) { L("ERROR CreateVertexDeclaration (luz)"); return 0; }
    }
    if (!g_decl) {
        D3DVERTEXELEMENT9 e[] = { {0, 0, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_POSITION, 0}, {0, 16, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_TEXCOORD, 0}, D3DDECL_END() };
        if (FAILED(d->CreateVertexDeclaration(e, &g_decl))) { L("ERROR CreateVertexDeclaration"); return 0; }
    }
    if (!g_sb && FAILED(d->CreateStateBlock(D3DSBT_ALL, &g_sb))) { L("ERROR CreateStateBlock"); return 0; }
    return 1;
}
// 1.4: textura de la normal suavizada (pantalla completa, A16B16G16R16F; si no hay, A8R8G8B8)
static int crea_normal(IDirect3DDevice9* d, UINT w, UINT h) {
    if (g_norm && (g_normW != w || g_normH != h)) { g_norm->Release(); g_norm = nullptr; }
    if (g_norm) return 1;
    D3DFORMAT f = D3DFMT_A16B16G16R16F;
    if (FAILED(d->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, f, D3DPOOL_DEFAULT, &g_norm, nullptr))) {
        f = D3DFMT_A8R8G8B8;
        if (FAILED(d->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, f, D3DPOOL_DEFAULT, &g_norm, nullptr))) { L("ERROR: textura de normales %ux%u no disponible", w, h); g_luces = 0; return 0; }
    }
    g_normW = w; g_normH = h; L("Textura de normales creada: %ux%u fmt=%u", w, h, f);
    return 1;
}
// 1.6: textura donde se acumula la luz (sin albedo) antes de componerla con la rodilla
static int crea_acumulador(IDirect3DDevice9* d, UINT w, UINT h) {
    if (g_acc && (g_accW != w || g_accH != h)) { g_acc->Release(); g_acc = nullptr; }
    if (g_acc) return 1;
    if (FAILED(d->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &g_acc, nullptr))) { L("ERROR: textura de luz acumulada %ux%u no disponible", w, h); g_luces = 0; return 0; }
    g_accW = w; g_accH = h; L("Textura de luz acumulada creada: %ux%u", w, h);
    return 1;
}
// 2.0a: especular acumulado (segundo destino de PS_Luz). Si el dispositivo no admite 2 destinos, se apaga.
static int crea_especular(IDirect3DDevice9* d, UINT w, UINT h) {
    if (g_mrtOk < 0) { D3DCAPS9 cp = {}; g_mrtOk = SUCCEEDED(d->GetDeviceCaps(&cp)) && cp.NumSimultaneousRTs >= 2; L("Especular: %u destinos simultaneos -> %s", cp.NumSimultaneousRTs, g_mrtOk ? "disponible" : "NO disponible"); }
    if (!g_mrtOk) return 0;
    if (g_esp && (g_espW != w || g_espH != h)) { g_esp->Release(); g_esp = nullptr; }
    if (g_esp) return 1;
    if (FAILED(d->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &g_esp, nullptr))) { L("ERROR: textura de especular %ux%u no disponible; especular apagado", w, h); g_espActivo = 0; return 0; }
    g_espW = w; g_espH = h; L("Textura de especular creada: %ux%u", w, h);
    return 1;
}

// ---------------------------------------------------------------- Reset (envoltorio de ENB, entrada 16)
typedef HRESULT(WINAPI* PReset)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
static PReset o_reset;
static HRESULT WINAPI h_reset(IDirect3DDevice9* d, D3DPRESENT_PARAMETERS* p) {
    libera_default(); g_vpValido = 0;
    HRESULT r = o_reset(d, p);
    L("Reset %ux%u -> 0x%08X (recursos DEFAULT liberados; se recrean al volver a usarse)", p ? p->BackBufferWidth : 0, p ? p->BackBufferHeight : 0, r);
    return r;
}

// ---------------------------------------------------------------- matrices
static void mul44(const float* a, const float* b, float* o) {
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) { float s = 0; for (int k = 0; k < 4; k++) s += a[i * 4 + k] * b[k * 4 + j]; o[i * 4 + j] = s; }
}
static int inv44(const float* m, float* o) {
    float inv[16];
    inv[0] = m[5]*m[10]*m[15]-m[5]*m[11]*m[14]-m[9]*m[6]*m[15]+m[9]*m[7]*m[14]+m[13]*m[6]*m[11]-m[13]*m[7]*m[10];
    inv[4] = -m[4]*m[10]*m[15]+m[4]*m[11]*m[14]+m[8]*m[6]*m[15]-m[8]*m[7]*m[14]-m[12]*m[6]*m[11]+m[12]*m[7]*m[10];
    inv[8] = m[4]*m[9]*m[15]-m[4]*m[11]*m[13]-m[8]*m[5]*m[15]+m[8]*m[7]*m[13]+m[12]*m[5]*m[11]-m[12]*m[7]*m[9];
    inv[12] = -m[4]*m[9]*m[14]+m[4]*m[10]*m[13]+m[8]*m[5]*m[14]-m[8]*m[6]*m[13]-m[12]*m[5]*m[10]+m[12]*m[6]*m[9];
    inv[1] = -m[1]*m[10]*m[15]+m[1]*m[11]*m[14]+m[9]*m[2]*m[15]-m[9]*m[3]*m[14]-m[13]*m[2]*m[11]+m[13]*m[3]*m[10];
    inv[5] = m[0]*m[10]*m[15]-m[0]*m[11]*m[14]-m[8]*m[2]*m[15]+m[8]*m[3]*m[14]+m[12]*m[2]*m[11]-m[12]*m[3]*m[10];
    inv[9] = -m[0]*m[9]*m[15]+m[0]*m[11]*m[13]+m[8]*m[1]*m[15]-m[8]*m[3]*m[13]-m[12]*m[1]*m[11]+m[12]*m[3]*m[9];
    inv[13] = m[0]*m[9]*m[14]-m[0]*m[10]*m[13]-m[8]*m[1]*m[14]+m[8]*m[2]*m[13]+m[12]*m[1]*m[10]-m[12]*m[2]*m[9];
    inv[2] = m[1]*m[6]*m[15]-m[1]*m[7]*m[14]-m[5]*m[2]*m[15]+m[5]*m[3]*m[14]+m[13]*m[2]*m[7]-m[13]*m[3]*m[6];
    inv[6] = -m[0]*m[6]*m[15]+m[0]*m[7]*m[14]+m[4]*m[2]*m[15]-m[4]*m[3]*m[14]-m[12]*m[2]*m[7]+m[12]*m[3]*m[6];
    inv[10] = m[0]*m[5]*m[15]-m[0]*m[7]*m[13]-m[4]*m[1]*m[15]+m[4]*m[3]*m[13]+m[12]*m[1]*m[7]-m[12]*m[3]*m[5];
    inv[14] = -m[0]*m[5]*m[14]+m[0]*m[6]*m[13]+m[4]*m[1]*m[14]-m[4]*m[2]*m[13]-m[12]*m[1]*m[6]+m[12]*m[2]*m[5];
    inv[3] = -m[1]*m[6]*m[11]+m[1]*m[7]*m[10]+m[5]*m[2]*m[11]-m[5]*m[3]*m[10]-m[9]*m[2]*m[7]+m[9]*m[3]*m[6];
    inv[7] = m[0]*m[6]*m[11]-m[0]*m[7]*m[10]-m[4]*m[2]*m[11]+m[4]*m[3]*m[10]+m[8]*m[2]*m[7]-m[8]*m[3]*m[6];
    inv[11] = -m[0]*m[5]*m[11]+m[0]*m[7]*m[9]+m[4]*m[1]*m[11]-m[4]*m[3]*m[9]-m[8]*m[1]*m[7]+m[8]*m[3]*m[5];
    inv[15] = m[0]*m[5]*m[10]-m[0]*m[6]*m[9]-m[4]*m[1]*m[10]+m[4]*m[2]*m[9]+m[8]*m[1]*m[6]-m[8]*m[2]*m[5];
    float det = m[0]*inv[0]+m[1]*inv[4]+m[2]*inv[8]+m[3]*inv[12];
    if (fabsf(det) < 1e-20f) return 0;
    det = 1.0f / det; for (int i = 0; i < 16; i++) o[i] = inv[i] * det; return 1;
}

// 2.0g: inversa en double de una matriz float (la de la pasada relativa a la camara)
static int inv44d(const float* mf, float* o) {
    double m[16], inv[16]; for (int i = 0; i < 16; i++) m[i] = mf[i];
    for (int c = 0; c < 16; c++) inv[c] = (c % 5 == 0) ? 1.0 : 0.0;   // Gauss-Jordan sobre [m | I]
    for (int col = 0; col < 4; col++) {
        int piv = col; for (int r = col + 1; r < 4; r++) if (fabs(m[r * 4 + col]) > fabs(m[piv * 4 + col])) piv = r;
        if (fabs(m[piv * 4 + col]) < 1e-30) return 0;
        if (piv != col) for (int k = 0; k < 4; k++) { double t = m[col * 4 + k]; m[col * 4 + k] = m[piv * 4 + k]; m[piv * 4 + k] = t; t = inv[col * 4 + k]; inv[col * 4 + k] = inv[piv * 4 + k]; inv[piv * 4 + k] = t; }
        double dv = 1.0 / m[col * 4 + col]; for (int k = 0; k < 4; k++) { m[col * 4 + k] *= dv; inv[col * 4 + k] *= dv; }
        for (int r = 0; r < 4; r++) if (r != col) { double f = m[r * 4 + col]; if (f != 0.0) for (int k = 0; k < 4; k++) { m[r * 4 + k] -= f * m[col * 4 + k]; inv[r * 4 + k] -= f * inv[col * 4 + k]; } }
    }
    for (int i = 0; i < 16; i++) o[i] = (float)inv[i]; return 1;
}

// ---------------------------------------------------------------- copia de la escena (antes de las coronas)
static void copia_escena(IDirect3DDevice9* d) {
    g_albValido = 0;
    IDirect3DSurface9* rt = nullptr;
    if (FAILED(d->GetRenderTarget(0, &rt)) || !rt) return;
    D3DSURFACE_DESC dd; rt->GetDesc(&dd);
    g_W = (int)dd.Width; g_H = (int)dd.Height;
    if (!g_diagMrt) { g_diagMrt = 1; for (int i = 1; i < 4; i++) { IDirect3DSurface9* s = nullptr; d->GetRenderTarget(i, &s);
        if (s) { D3DSURFACE_DESC sd; s->GetDesc(&sd); L("DIAG: al copiar la escena, RT%d = %p %ux%u fmt=%u", i, s, sd.Width, sd.Height, sd.Format); s->Release(); } else L("DIAG: al copiar la escena, RT%d vacio", i); } }
    if (g_alb && (g_albW != dd.Width || g_albH != dd.Height || g_albF != dd.Format)) { g_alb->Release(); g_alb = nullptr; }
    if (!g_alb) {
        if (FAILED(d->CreateTexture(dd.Width, dd.Height, 1, D3DUSAGE_RENDERTARGET, dd.Format, D3DPOOL_DEFAULT, &g_alb, nullptr))) { rt->Release(); L("ERROR CreateTexture albedo %ux%u fmt=%u", dd.Width, dd.Height, dd.Format); g_luces = 0; return; }
        g_albW = dd.Width; g_albH = dd.Height; g_albF = dd.Format;
        L("Copia de escena creada: %ux%u fmt=%u", dd.Width, dd.Height, dd.Format);
    }
    IDirect3DSurface9* s = nullptr;
    if (SUCCEEDED(g_alb->GetSurfaceLevel(0, &s))) { if (SUCCEEDED(d->StretchRect(rt, nullptr, s, nullptr, D3DTEXF_POINT))) g_albValido = 1; s->Release(); }
    rt->Release();
    D3DMATRIX v, p; float vi[16];
    if (SUCCEEDED(d->GetTransform(D3DTS_VIEW, &v)) && SUCCEEDED(d->GetTransform(D3DTS_PROJECTION, &p)) && inv44d(&v._11, vi)) {   // 2.0g: camara en double
        mul44(&v._11, &p._11, g_vp); g_cam[0] = vi[12]; g_cam[1] = vi[13]; g_cam[2] = vi[14]; g_vpValido = 1;
        D3DMATRIX vr = v; vr._41 = 0.0f; vr._42 = 0.0f; vr._43 = 0.0f; mul44(&vr._11, &p._11, g_vpRel);   // 2.0g
        g_projA = p._33; g_projB = p._43; g_projYs = p._22;   // 2.0j
    }
}

// ---------------------------------------------------------------- 1.4: escaner de farolas
// El juego solo procesa las luces 2DFX (ProcessLightsForEntity, desde PreRender) de las entidades que dibuja
// en ese cuadro: una farola solo da luz dentro de su distancia de dibujado y si esta en pantalla, y por eso
// las luces "aparecian de golpe" al conducir. El escaner recorre por tandas los pools de edificios, dummies y
// objetos, y por cada entidad visible a menos de DistanciaMaxima con luces 2DFX constantes (tipo de parpadeo 0)
// guarda su posicion en el mundo (TransformFromObjectSpace, como el juego). Cada cuadro se decide si estan
// encendidas con la MISMA regla que ProcessLightsForEntity (0x6FCAA8: banderas 0x20/0x40 y la hora de
// 0x6FAB30; apagon de 0x441C10) y el color = byte/256 x desvanecido (0x6FD1B9). Las que el juego si proceso
// este cuadro (anotadas en AddLight) mandan: de ellas se mide la correccion K (anotada / prevista).
// Los DFF solo dan sus 2DFX con el modelo cargado; el modelo de una farola lo comparten todas sus instancias.
typedef void*(__thiscall* PGet2d)(void*, int);
typedef void*(__thiscall* PTransf)(void*, float*, const float*);
typedef float(__cdecl* PFf)(); typedef bool(__cdecl* PFb)();
typedef void*(__cdecl* PInfoNombre)(const char*, int*);
static const PGet2d  Get2dEffect = (PGet2d)0x4C4C70;
static const PTransf TransformFromObjectSpace = (PTransf)0x5334F0;
static const PFf     LightTimeFade = (PFf)0x6FAB30;
static const PFb     ApagonCercano = (PFb)0x441C10;
static const PInfoNombre GetModelInfoByName = (PInfoNombre)0x4C5940;
struct LuzEsc { float pos[3], radio; uint8_t r, g, b, pad; uint16_t flags, semilla, modelo, pad2; };
// 1.5: semaforo del escaner: posicion media de sus luces 2DFX (como DisplayActualLight) y su orientacion
// (0x49D520 -> 0x49D4D0: 1 = LightForCars1, si no LightForCars2).
struct SemEsc { float pos[3]; int dir; };
static std::vector<SemEsc> g_sem, g_semNuevo; static uint16_t g_semModelos[9]; static int g_nSemModelos = 0;
typedef float(__cdecl* PFangulo)(void*); typedef int(__cdecl* PFtipoDir)(float); typedef uint8_t(__cdecl* PFestado)();
static const PFangulo  SemAngulo = (PFangulo)0x49D520; static const PFtipoDir SemTipoDir = (PFtipoDir)0x49D4D0;
static const PFestado  LightForCars1 = (PFestado)0x49D350, LightForCars2 = (PFestado)0x49D3A0;
static long g_semUsadosUlt = 0;
static void lee_modelos_semaforo() {   // MI_TRAFFICLIGHTS* que comprueba CEntity::PreRender (0x536977...0x536B4E)
    static const uintptr_t mi[9] = { 0x8CD4F4, 0x8CD4F8, 0x8CD4FC, 0x8CD500, 0x8CD504, 0x8CD508, 0x8CD50C, 0x8CD510, 0x8CD514 };
    g_nSemModelos = 0;
    for (uintptr_t a : mi) { uint16_t v = *(uint16_t*)a; if (v != 0xFFFF) g_semModelos[g_nSemModelos++] = v; }
}
static std::vector<LuzEsc> g_esc, g_escNuevo;
static std::unordered_set<uint64_t> g_escClaves, g_anotClaves;
static int g_escar = 1, g_escPool = 0, g_escIdx = 0, g_tablaIntentos = 0, g_maxModelo = 40000;
static float g_escMediana = 0; static long g_escVueltas = 0, g_escEmpUlt = 0, g_escUsadasUlt = 0; static uintptr_t g_tablaMI = 0; static float g_escK = 1.0f;
static uint64_t clave(const float* p) {
    uint64_t x = (uint32_t)(int)floorf(p[0] * 8.0f + 0.5f) & 0x1FFFFF, y = (uint32_t)(int)floorf(p[1] * 8.0f + 0.5f) & 0x1FFFFF, z = (uint32_t)(int)floorf(p[2] * 8.0f + 0.5f) & 0x1FFFFF;
    return (x << 42) | (y << 21) | z;
}
// CModelInfo::ms_modelInfoPtrs vivo: el FLA lo reubica; se leen los disp32 vivos de varias instrucciones que lo
// usan y se valida con GetModelInfo("male01") (mismo metodo que Vice City Viva, Game.cpp).
static uintptr_t tabla_modelos() {
    static const uintptr_t sitios[] = { 0x60350C, 0x403DA7, 0x403ECF, 0x405CE2, 0x405E53, 0x407B24 };
    int id = -1; void* mi = GetModelInfoByName("male01", &id);
    if (!mi || id < 0) return 0;
    for (uintptr_t s : sitios) {
        uintptr_t t = *(uintptr_t*)s;
        if (IsBadReadPtr((void*)(t + 4u * (uint32_t)id), 4) || *(void**)(t + 4u * (uint32_t)id) != mi) continue;
        L("Escaner: tabla de modelos en 0x%08X (disp32 vivo de 0x%06X; validada con male01=%d)", (unsigned)t, (unsigned)s, id);
        return t;
    }
    return 0;
}
// [Modelos] del .ini: nombre=intensidad[,radio[,cono[,altura]]]. Los nombres se resuelven con GetModelInfo.
// 2.0a: cono = semiangulo (grados) de un foco que apunta hacia abajo (farolas); altura = metros sobre la luz 2DFX.
static void lee_factores_modelo() {
    if (g_factoresLeidos) return;
    g_factoresLeidos = 1;
    static char sec[16384];
    DWORD n = GetPrivateProfileSectionA("Modelos", sec, sizeof sec, g_ini.c_str());
    for (char* s = sec; n && *s; s += strlen(s) + 1) {
        char lin[256]; strncpy(lin, s, 255); lin[255] = 0;
        char* igual = strchr(lin, '='); if (!igual || lin[0] == ';') continue;
        *igual = 0; char* nombre = lin; while (*nombre == ' ') nombre++;
        for (char* q = nombre + strlen(nombre); q > nombre && q[-1] == ' '; ) *--q = 0;
        FactorModelo f = { 1.0f, 1.0f, -2.0f, 0.0f };
        float v[4] = { 1.0f, 1.0f, 0.0f, 0.0f }; int nv = 0;
        for (char* q = igual + 1; q && nv < 4; nv++) { v[nv] = (float)atof(q); q = strchr(q, ','); if (q) q++; }
        f.intensidad = v[0]; if (nv > 1) f.radio = v[1];
        if (nv > 2 && v[2] > 0.0f && v[2] < 89.0f) f.cono = cosf(v[2] * 3.14159265f / 180.0f);
        if (nv > 3) f.altura = v[3];
        int id = -1; void* mi = GetModelInfoByName(nombre, &id);
        if (!mi || id < 0 || id > 65534) { L("Modelos: '%s' no existe (se ignora)", nombre); continue; }
        g_factores[(uint16_t)id] = f;
        L("Modelos: %s (id %d) intensidad x%.2f radio x%.2f cono %.0f grados altura %+.2f m", nombre, id, f.intensidad, f.radio, f.cono > -1.5f ? v[2] : 0.0f, f.altura);
    }
}
static void escanea_entidad(uint8_t* e) {
    uint32_t f = *(uint32_t*)(e + 0x1C);
    if (!(f & 0x80) || (f & 0x200)) return;                      // como 0x6FC7B5: visible y sin el bit 0x200
    uint8_t* m = *(uint8_t**)(e + 0x14);
    const float* p = m ? (const float*)(m + 0x30) : (const float*)(e + 4);
    float dx = p[0] - g_cam[0], dy = p[1] - g_cam[1], lim = g_distMax + 40.0f;
    if (dx * dx + dy * dy > lim * lim) return;
    if (m && *(float*)(m + 0x28) < 0.96f) return;                // poste derribado o inclinado (0x6FC7DB)
    uint32_t id = *(uint16_t*)(e + 0x22);
    if ((int)id >= g_maxModelo) return;
    uint8_t* mi = *(uint8_t**)(g_tablaMI + 4u * id);
    if (!mi || !mi[0xD]) return;
    int n = mi[0xD];
    for (int s = 0; s < g_nSemModelos; s++) if (g_semModelos[s] == id) {
        SemEsc se = {}; int nl = 0;
        for (int i = 0; i < n; i++) {
            uint8_t* fx = (uint8_t*)Get2dEffect(mi, i);
            if (!fx || fx[0xC] != 0) continue;
            float w[3]; TransformFromObjectSpace(e, w, (const float*)fx); se.pos[0] += w[0]; se.pos[1] += w[1]; se.pos[2] += w[2]; nl++;
        }
        if (!nl) return;
        float k = 1.0f / (float)n; se.pos[0] *= k; se.pos[1] *= k; se.pos[2] *= k;   // DisplayActualLight divide por TODOS los 2DFX (0x49DE32)
        se.dir = SemTipoDir(SemAngulo(e));
        g_semNuevo.push_back(se);
        return;
    }
    for (int i = 0; i < n; i++) {
        uint8_t* fx = (uint8_t*)Get2dEffect(mi, i);
        if (!fx || fx[0xC] != 0) continue;                       // solo luces
        float radio = *(float*)(fx + 0x18);
        if (!(radio > 0.0f) || !(fx[0x10] | fx[0x11] | fx[0x12]) || fx[0x26] != 0) continue;   // con point light, color y constante
        uint16_t fl = *(uint16_t*)(fx + 0x24);
        if ((fl & 0x800) || !(fl & 0x60)) continue;              // CHECK_DIRECTION, o ni de dia ni de noche
        LuzEsc l; TransformFromObjectSpace(e, l.pos, (const float*)fx);
        if (!g_escClaves.insert(clave(l.pos)).second) continue;  // dummy y objeto en el mismo sitio
        l.radio = radio; l.r = fx[0x10]; l.g = fx[0x11]; l.b = fx[0x12]; l.pad = 0; l.flags = fl; l.modelo = (uint16_t)id; l.pad2 = 0;
        l.semilla = (uint16_t)(*(uint16_t*)(0x8D5028 + (i & 7) * 2) ^ *(uint16_t*)(e + 0x20));
        g_escNuevo.push_back(l);
    }
}
static void escanea_paso() {
    if (!g_escar || !g_vpValido) return;
    if (!g_tablaMI) {
        if ((g_tablaIntentos++ % 300) != 0) return;
        g_tablaMI = tabla_modelos();
        if (!g_tablaMI) { if (g_tablaIntentos > 3000) { g_escar = 0; L("Escaner apagado: no se localiza la tabla de modelos"); } return; }
        lee_modelos_semaforo(); L("Escaner: %d modelos de semaforo", g_nSemModelos);
        lee_factores_modelo();
    }
    static const uintptr_t pools[3] = { 0xB74498, 0xB744A0, 0xB7449C };   // CPools: edificios, dummies, objetos
    static const int paso[3] = { 0x38, 0x38, 0x19C };   // tamanos de CPools::Initialise (0x550578/0x550648/0x5506F8)
    int presupuesto = 6000, vistos = 0;
    while (presupuesto > 0 && vistos < 4) {
        uintptr_t pp = *(uintptr_t*)pools[g_escPool];
        uint8_t* obj = pp ? *(uint8_t**)pp : nullptr; uint8_t* fl = pp ? *(uint8_t**)(pp + 4) : nullptr; int tam = pp ? *(int*)(pp + 8) : 0;
        if (obj && fl && tam > 0 && tam < 1000000) {
            int fin = g_escIdx + presupuesto; if (fin > tam) fin = tam;
            for (int i = g_escIdx; i < fin; i++) if (!(fl[i] & 0x80)) escanea_entidad(obj + (size_t)i * paso[g_escPool]);
            presupuesto -= fin - g_escIdx; g_escIdx = fin;
            if (g_escIdx < tam) break;
        }
        g_escIdx = 0; vistos++;
        if (++g_escPool == 3) {
            g_escPool = 0; g_esc.swap(g_escNuevo); g_escNuevo.clear(); g_escClaves.clear(); g_sem.swap(g_semNuevo); g_semNuevo.clear(); lee_modelos_semaforo();
            if (g_escVueltas++ == 0) L("Escaner: primera vuelta completa, %u luces 2DFX constantes y %u semaforos a menos de %.0f m", (unsigned)g_esc.size(), (unsigned)g_sem.size(), g_distMax + 40.0f);
        }
    }
}
static void escanea_seguro() {
    __try { escanea_paso(); } __except (EXCEPTION_EXECUTE_HANDLER) { g_escar = 0; L("EXCEPCION en el escaner de farolas: apagado (solo quedan las luces que procesa el juego)"); }
}

// ---------------------------------------------------------------- pasada de luces
// 1.6: clases de luz. C_2DFX = luces 2DFX de los modelos (farolas, rotulos, carteles...); C_FARO = faros;
// C_SEM = semaforos; C_OTRA = el resto de CPointLights (fuego, explosiones, scripts y otros mods).
enum { C_2DFX = 0, C_FARO = 1, C_SEM = 2, C_OTRA = 3, C_FRENO = 4, C_DISPARO = 5, C_EXPLOSION = 6, C_FUEGO = 7 };   // 2.0c: C_FRENO; 2.0d: disparo, explosion, fuego
struct Cand { float pos[3], radio, col[3], tipo, dir[3], dist; int clase; float cosCono; };
static std::vector<Cand> g_inyectadas;   // solo el arnes
static std::vector<Cand> g_cand;
static void anade(const float* p, float radio, float r, float g, float b, int clase, const float* dir, uint16_t modelo = 0xFFFF) {
    float gan, mult;
    switch (clase) {
    case C_FARO: gan = g_ganancia; mult = g_radioMult; break;
    case C_SEM:  gan = g_ganSem; mult = g_radioSem; break;
    case C_OTRA: gan = g_ganOtras; mult = g_radioOtras; break;
    case C_FRENO: gan = g_ganFrenos; mult = 1.0f; break;
    case C_DISPARO: gan = g_dispInt; mult = 1.0f; break;
    case C_EXPLOSION: gan = g_expInt; mult = 1.0f; break;
    case C_FUEGO: gan = g_fuegoGan; mult = g_fuegoRadio; break;
    default:     gan = g_ganFarolas; mult = g_radioFarolas; break;
    }
    // 1.6: en el juego original una point light solo ilumina peds y coches, y hay 2DFX con alcances de 30-55 m
    // (rotulo de Binco Sign, rascacielos ltslasky*, lamparas de GTA Mixed). Aqui iluminan todo: se recorta.
    bool libre = clase == C_FARO || clase == C_DISPARO || clase == C_EXPLOSION;   // 2.0d: sin los topes de las 2DFX
    if (!libre && radio > g_radioMaxLuz) radio = g_radioMaxLuz;
    FactorModelo fm = { 1.0f, 1.0f, -2.0f, 0.0f };
    if (modelo != 0xFFFF && !g_factores.empty()) { auto it = g_factores.find(modelo); if (it != g_factores.end()) fm = it->second; }
    float rad = radio * mult * fm.radio;
    // 1.3: tope de intensidad (la ganancia x8 se penso para farolas; un semaforo o un neon llegan a 1,0)
    if (!libre) { float mx = r > g ? (r > b ? r : b) : (g > b ? g : b); if (mx > 0.0f && gan * mx > g_capFarolas) gan = g_capFarolas / mx; }
    // 1.7: tope por luminancia: una farola naranja (249,145,34) queda en ~1,6 y una blanca, que con el tope
    // anterior quedaba en 2,5 en los tres canales, baja al mismo nivel.
    if (!libre) { float lum = (0.2126f * r + 0.7152f * g + 0.0722f * b) * gan; if (lum > g_lumMax) gan *= g_lumMax / lum; }
    gan *= fm.intensidad;
    if (modelo != 0xFFFF && g_factorExtUsa[modelo]) { gan *= g_factorExt[modelo]; if (gan <= 0.0f) return; }   // 2.0f
    if (modelo != 0xFFFF && g_factorBlaUsa[modelo]) {   // 2.0h: solo las blancas o casi blancas, tambien tras los topes
        float mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
        if (mx > 0.0f && (mx - mn) / mx < kSaturacionBlanca) { gan *= g_factorBla[modelo]; if (gan <= 0.0f) return; }
    }
    if (clase != C_FARO || g_horaFaros) gan *= g_factorHora;   // 1.8: compensa la adaptacion nocturna de ENB
    float pz = p[2];
    bool foco = g_focos && clase == C_2DFX && fm.cono > -1.5f;   // 2.0a: farola como foco hacia abajo
    if (foco) pz += fm.altura;
    float dx = p[0] - g_cam[0], dy = p[1] - g_cam[1], dz = pz - g_cam[2];
    float dist = sqrtf(dx * dx + dy * dy + dz * dz) - rad;
    if (dist > g_distMax) return;
    // 1.4: cada luz entra desvanecida en el ultimo 25 % de DistanciaMaxima (antes el corte era por pixel)
    float fade = (g_distMax - dist) / (0.25f * g_distMax); if (fade > 1.0f) fade = 1.0f; gan *= fade;
    Cand c;
    c.pos[0] = p[0]; c.pos[1] = p[1]; c.pos[2] = pz; c.radio = rad; c.col[0] = r * gan; c.col[1] = g * gan; c.col[2] = b * gan; c.tipo = (clase == C_FARO || clase == C_FRENO) ? 1.0f : 0.0f;
    c.dir[0] = dir ? dir[0] : 0; c.dir[1] = dir ? dir[1] : 0; c.dir[2] = dir ? dir[2] : 0; c.dist = dist; c.clase = clase; c.cosCono = clase == C_FRENO ? g_cosFreno : g_cosFaro;
    if (foco) { c.tipo = 2.0f; c.dir[0] = 0; c.dir[1] = 0; c.dir[2] = -1.0f; c.cosCono = fm.cono; }
    g_cand.push_back(c);
}
// 1.6: las luces 2DFX de una misma celda de FusionarLucesMetros se funden en una (posicion media, color y
// radio maximos): las 5 luces del rotulo de Binco o las 6 del cartel md_billbolaeb ya no se suman.
static void fusiona() {
    g_fusionadasUlt = 0;
    if (g_fusion <= 0.0f) return;
    static std::unordered_map<uint64_t, int> celda; static std::vector<int> cuenta;
    celda.clear(); cuenta.assign(g_cand.size(), 1);
    float inv = 1.0f / g_fusion; size_t w = 0;
    for (size_t i = 0; i < g_cand.size(); i++) {
        Cand c = g_cand[i];
        if (c.clase != C_2DFX) { g_cand[w] = c; cuenta[w] = 1; w++; continue; }
        // 2.0e (05-10-2026): solo se funden luces del mismo color. La celda lleva el tono (r,g,b normalizados, 8 niveles
        // por canal, 9 bits): los focos rojos, verdes y azules de la estructura del Club Malibu (cl_spots, 23 luces a
        // 1-3 m, trasplantadas de Project Eagle) se fundian en una luz blanca (maximo de cada canal). Los rotulos de un
        // solo color (Binco, md_billbolaeb) se siguen fundiendo igual. z de la celda en 12 bits (4096 celdas).
        float sc = c.col[0] + c.col[1] + c.col[2]; sc = sc > 1e-6f ? 7.0f / sc : 0.0f;
        uint64_t tono = ((uint64_t)(int)(c.col[0] * sc + 0.5f) << 6) | ((uint64_t)(int)(c.col[1] * sc + 0.5f) << 3) | (uint64_t)(int)(c.col[2] * sc + 0.5f);
        uint64_t k = ((uint64_t)((int)floorf(c.pos[0] * inv) & 0x1FFFFF) << 42) | ((uint64_t)((int)floorf(c.pos[1] * inv) & 0x1FFFFF) << 21) | ((uint64_t)((int)floorf(c.pos[2] * inv) & 0xFFF) << 9) | (tono & 0x1FF);
        auto it = celda.find(k);
        if (it == celda.end()) { celda[k] = (int)w; g_cand[w] = c; cuenta[w] = 1; w++; continue; }
        Cand& a = g_cand[it->second]; int n = cuenta[it->second]++;
        for (int j = 0; j < 3; j++) { a.pos[j] = (a.pos[j] * n + c.pos[j]) / (n + 1); if (c.col[j] > a.col[j]) a.col[j] = c.col[j]; }
        if (c.radio > a.radio) a.radio = c.radio;
        if (c.dist < a.dist) a.dist = c.dist;
        g_fusionadasUlt++;
    }
    g_cand.resize(w);
}
// 2.0d: puentes de AddLight de disparos, explosiones y fuego (anotan y llaman al original)
static void anota_vida(int clase, float x, float y, float z, float radio) {
    uint32_t t = *(uint32_t*)0xB7CB84; float lim = clase == C_EXPLOSION ? 1.0f : 0.5f;
    for (Vida& v : g_vida) if (v.clase == clase && fabsf(v.pos[0] - x) < lim && fabsf(v.pos[1] - y) < lim && fabsf(v.pos[2] - z) < lim) { v.t = t; if (radio > v.radio) v.radio = radio; return; }
    if (g_vida.size() >= 64) return;
    Vida v = { { x, y, z }, radio, t, clase }; g_vida.push_back(v);
    if (clase == C_DISPARO) g_disparosTot++; else g_explosTot++;
}
static void __cdecl h_al_disparo(int tipo, float px, float py, float pz, float dx, float dy, float dz, float radio, float r, float g, float b, int niebla, int sombras, void* ent) {
    if (g_disparos) anota_vida(C_DISPARO, px, py, pz, radio);
    if (!g_sinOriginal) CPointLights_AddLight(tipo, px, py, pz, dx, dy, dz, radio, r, g, b, niebla, sombras, ent);
}
static void __cdecl h_al_explosion(int tipo, float px, float py, float pz, float dx, float dy, float dz, float radio, float r, float g, float b, int niebla, int sombras, void* ent) {
    if (g_explos) anota_vida(C_EXPLOSION, px, py, pz, radio);
    if (!g_sinOriginal) CPointLights_AddLight(tipo, px, py, pz, dx, dy, dz, radio, r, g, b, niebla, sombras, ent);
}
static void __cdecl h_al_fuego(int tipo, float px, float py, float pz, float dx, float dy, float dz, float radio, float r, float g, float b, int niebla, int sombras, void* ent) {
    if (g_fuegoAct && radio > 0.0f && (r + g + b) > 0.0f && g_nFuego < 256) {
        LuzReg& l = g_fuego[g_nFuego++]; l.pos[0] = px; l.pos[1] = py; l.pos[2] = pz; l.radio = radio; l.r = r; l.g = g; l.b = b; l.modelo = 0xFFFF;
    }
    if (!g_sinOriginal) CPointLights_AddLight(tipo, px, py, pz, dx, dy, dz, radio, r, g, b, niebla, sombras, ent);
}
static void toma_fuego() { g_nFuegoCuadro = (int)g_nFuego; if (g_nFuegoCuadro > 256) g_nFuegoCuadro = 256; memcpy(g_fuegoCuadro, g_fuego, g_nFuegoCuadro * sizeof(LuzReg)); g_nFuego = 0; }
// 2.0c: vehiculos del pool a menos de DistanciaMaxima (posicion de su matriz, como DoVehicleLights 0x6E2772)
static const float* veh_matriz(uint8_t* v) { uint8_t* m = *(uint8_t**)(v + 0x14); return m ? (const float*)m : nullptr; }
static void lee_vehiculos() {
    g_veh.clear();
    if (g_vehOk < 0) {
        g_vehPaso = *(uint32_t*)0x550011;                            // imul eax, eax, 0xA18 (CPools::GetVehicle)
        g_vehOk = (*(uint8_t*)0x55000F == 0x69 && g_vehPaso == 0xA18) ? 1 : 0;
        L("Vehiculos: paso del pool 0x%X -> %s", g_vehPaso, g_vehOk ? "faros dobles, frenos y marcha atras disponibles" : "NO esperado; 2.0c apagada");
    }
    if (g_vehOk != 1 || !g_tablaMI) return;
    uintptr_t pp = *(uintptr_t*)0xB74494; if (!pp) return;
    uint8_t* obj = *(uint8_t**)pp; uint8_t* fl = *(uint8_t**)(pp + 4); int tam = *(int*)(pp + 8);
    if (!obj || !fl || tam <= 0 || tam > 100000) return;
    float lim = g_distMax + 20.0f;
    for (int i = 0; i < tam; i++) {
        if (fl[i] & 0x80) continue;
        uint8_t* v = obj + (size_t)i * g_vehPaso;
        const float* m = veh_matriz(v); const float* p = m ? m + 12 : (const float*)(v + 4);
        float dx = p[0] - g_cam[0], dy = p[1] - g_cam[1];
        if (dx * dx + dy * dy > lim * lim) continue;
        Veh e = { v, { p[0], p[1], p[2] } }; g_veh.push_back(e);
    }
}
// Dos luces (o una, si el dummy esta en el eje) en el dummy k del modelo del coche, desplazadas y orientadas.
// atras = 1: hacia atras. Devuelve las luces anadidas (0 = no se pudo; el llamador usa la luz del juego).
static int luces_dummy(uint8_t* v, int k, int atras, float r, float g, float b, float radio, int clase, float adel, float alto, float incl) {
    const float* m = veh_matriz(v); if (!m) return 0;
    uint32_t id = *(uint16_t*)(v + 0x22); if ((int)id >= g_maxModelo) return 0;
    uint8_t* mi = *(uint8_t**)(g_tablaMI + 4u * id); if (!mi) return 0;
    uint8_t* st = *(uint8_t**)(mi + 0x5C); if (!st) return 0;
    const float* dm = (const float*)(st + k * 12);
    if (fabsf(dm[0]) + fabsf(dm[1]) + fabsf(dm[2]) < 1e-3f) return 0;
    float sg = atras ? -1.0f : 1.0f, ci = cosf(incl * 3.14159265f / 180.0f), si = sinf(incl * 3.14159265f / 180.0f);
    float dir[3]; for (int j = 0; j < 3; j++) dir[j] = m[4 + j] * sg * ci - m[8 + j] * si;
    int dos = fabsf(dm[0]) > 0.05f; float f = dos ? 0.5f : 1.0f;
    for (int lado = 0; lado < (dos ? 2 : 1); lado++) {
        float lx = lado ? -dm[0] : dm[0], ly = dm[1] + sg * adel, lz = dm[2] + alto, w[3];
        for (int j = 0; j < 3; j++) w[j] = m[12 + j] + m[j] * lx + m[4 + j] * ly + m[8 + j] * lz;
        anade(w, radio, r * f, g * f, b * f, clase, dir);
    }
    return dos ? 2 : 1;
}
static int luz_vehiculo(const PuntoLuz& l) {
    __try {
        uint8_t* v = (uint8_t*)l.ent;
        if (v) {                                                     // freno del juego (0x6E28E7, con la entidad)
            if (!g_frenos) return 0;
            int n = luces_dummy(v, 1, 1, l.r, l.g, l.b, l.radio * g_radioFrenos, C_FRENO, 0.2f, 0.0f, g_frenoIncl);
            g_frenosUlt += n ? 1 : 0; return n;
        }
        if (!g_farosDobles) return 0;
        for (const Veh& e : g_veh)                                   // faro del juego: centro del coche, sin entidad
            if (fabsf(e.pos[0] - l.pos[0]) < 0.02f && fabsf(e.pos[1] - l.pos[1]) < 0.02f && fabsf(e.pos[2] - l.pos[2]) < 0.02f) {
                int n = luces_dummy(e.v, 0, 0, l.r, l.g, l.b, l.radio, C_FARO, g_faroAdel, g_faroAlto, g_faroIncl);
                g_farosDoblesUlt += n ? 1 : 0; return n;
            }
    } __except (EXCEPTION_EXECUTE_HANDLER) { g_farosDobles = g_frenos = g_marchaAtras = 0; L("EXCEPCION en las luces de vehiculo: 2.0c apagada"); }
    return 0;
}
// Marcha atras: el juego no tiene luz. Coche con conductor, motor encendido, acelerador negativo y moviendose hacia atras.
static void marcha_atras() {
    if (!g_marchaAtras) return;
    __try {
        for (const Veh& e : g_veh) {
            uint8_t* v = e.v; const float* m = veh_matriz(v); if (!m) continue;
            if (!*(void**)(v + 0x460) || !(*(uint8_t*)(v + 0x428) & 0x10) || !(*(float*)(v + 0x49C) < -0.05f)) continue;
            const float* vel = (const float*)(v + 0x44);
            if (vel[0] * m[4] + vel[1] * m[5] + vel[2] * m[6] > -0.01f) continue;
            if (luces_dummy(v, 1, 1, g_intMarcha, g_intMarcha, g_intMarcha, g_radioMarcha, C_FRENO, 0.2f, 0.0f, g_frenoIncl)) g_marchaUlt++;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { g_marchaAtras = 0; L("EXCEPCION en la marcha atras: apagada"); }
}
static int cmp_ratio(const void* a, const void* b) { float x = *(const float*)a, y = *(const float*)b; return x < y ? -1 : (x > y ? 1 : 0); }
static IndiceLuces regIndex, semIndex, fireIndex, lifeIndex;
static void junta_luces() {
    g_cand.clear(); g_anotClaves.clear();
    regIndex.build(g_regCuadro,g_nRegCuadro);
    semIndex.build(g_regSemCuadro,g_nRegSemCuadro);
    calcula_factor_hora();
    // farolas y demas luces 2DFX que el juego proceso este cuadro (sin el filtro de 27 m de AddLight)
    for (int i = 0; i < g_nRegCuadro; i++) { const LuzReg& l = g_regCuadro[i]; g_anotClaves.insert(clave(l.pos)); anade(l.pos, l.radio, l.r, l.g, l.b, C_2DFX, nullptr, l.modelo); }
    // 1.4: farolas del escaner que el juego no proceso (lejos o fuera de pantalla)
    long usadas = 0;
    if (g_escar && !g_esc.empty()) {
        float nf = LightTimeFade(); bool apagon = ApagonCercano(); uint32_t tms = *(uint32_t*)0xB7CB84;
        static float ratios[64]; int nr = 0;
        for (const LuzEsc& l : g_esc) {
            float fade;
            if (l.flags & 0x20) { if (l.flags & 0x40) fade = 1.0f; else if (nf < 1.0f) fade = 1.0f - nf; else continue; }
            else { if (nf > 0.0f) fade = nf; else continue; }
            if (apagon) { int s = l.semilla & 3; if (s == 0 || (s == 1 && ((((uint32_t)l.semilla * 8) ^ tms) & 0x60))) continue; }
            float k = fade / 256.0f;
            if (g_anotClaves.count(clave(l.pos))) {             // ya la dio el juego: sirve para medir K
                if (nr < 64) {
                    float pred = (float)(l.r > l.g ? (l.r > l.b ? l.r : l.b) : (l.g > l.b ? l.g : l.b)) * k;
                    int i=regIndex.first(g_regCuadro,g_nRegCuadro,l.pos,0.2f);
                    if(i>=0) { const LuzReg& a=g_regCuadro[i];
                        float am = a.r > a.g ? (a.r > a.b ? a.r : a.b) : (a.g > a.b ? a.g : a.b);
                        if (pred > 1e-4f) ratios[nr++] = am / pred; }
                }
                continue;
            }
            k *= g_escK;
            anade(l.pos, l.radio, l.r * k, l.g * k, l.b * k, C_2DFX, nullptr, l.modelo); usadas++;
        }
        g_escEmpUlt = nr;
        if (nr >= 3) {
            qsort(ratios, nr, sizeof(float), cmp_ratio);
            float med = ratios[nr / 2]; g_escMediana = med; if (med < 0.02f) med = 0.02f; if (med > 4.0f) med = 4.0f;   // 1.5: antes 0,25..4 (se quedaba en 0,25)
            g_escK += (med - g_escK) * 0.1f;
        }
    }
    g_escUsadasUlt = usadas;
    // 1.5: semaforos. Los que el juego proceso este cuadro (anotados, sin el filtro de 27 m) mandan; el resto,
    // del escaner con la misma regla que DisplayActualLight (0x49DE63: estado < 3 y [0xC812A8] > 0,5).
    for (int i = 0; i < g_nRegSemCuadro; i++) { const LuzReg& l = g_regSemCuadro[i]; anade(l.pos, l.radio, l.r, l.g, l.b, C_SEM, nullptr); }
    long sems = 0;
    if (g_escar && !g_sem.empty() && *(float*)0xC812A8 > 0.5f) {
        uint8_t e1 = LightForCars1(), e2 = LightForCars2(); float kc = *(float*)0x8A621C;
        for (const SemEsc& s : g_sem) {
            uint8_t est = s.dir == 1 ? e1 : e2;
            if (est >= 3) continue;
            bool dup = false;
            dup=semIndex.first(g_regSemCuadro,g_nRegSemCuadro,s.pos,0.5f)>=0;
            if (dup) continue;
            uint8_t cr = *(uint8_t*)(0x8A6214 + est), cg = *(uint8_t*)(0x8A6218 + est), cb = *(uint8_t*)(0xA9AD94 + est);
            anade(s.pos, 14.0f, (cr < 50 ? 50 : cr) * kc, (cg < 50 ? 50 : cg) * kc, (cb < 50 ? 50 : cb) * kc, C_SEM, nullptr); sems++;
        }
    }
    g_semUsadosUlt = sems;
    // 2.0c: vehiculos para los faros dobles, frenos y marcha atras
    g_farosDoblesUlt = g_frenosUlt = g_marchaUlt = 0;
    if (g_farosDobles || g_frenos || g_marchaAtras) { __try { lee_vehiculos(); } __except (EXCEPTION_EXECUTE_HANDLER) { g_vehOk = 0; g_veh.clear(); L("EXCEPCION leyendo el pool de vehiculos: 2.0c apagada"); } marcha_atras(); }
    // 2.0d: disparos y explosiones (luces con vida) y fuego (anotado este cuadro)
    g_disparosUlt = g_explosUlt = g_fuegoUlt = 0;
    {
        uint32_t ahora = *(uint32_t*)0xB7CB84; size_t w = 0;
        for (size_t i = 0; i < g_vida.size(); i++) {
            Vida v = g_vida[i];
            bool disp = v.clase == C_DISPARO;
            float dur = disp ? g_dispDur : g_expDur, x = (float)(uint32_t)(ahora - v.t) / 1000.0f / (dur > 1e-3f ? dur : 1e-3f);
            if (x >= 1.0f || !(disp ? g_disparos : g_explos)) continue;
            g_vida[w++] = v;
            float f = expf(-(disp ? g_dispCaida : g_expCaida) * x) * (1.0f - x);
            const float* col = disp ? g_dispCol : g_expCol;
            float q[3] = { v.pos[0], v.pos[1], v.pos[2] + (disp ? 0.0f : g_expAlto) };
            anade(q, disp ? g_dispRadio : v.radio * g_expRadio, col[0] * f, col[1] * f, col[2] * f, v.clase, nullptr);
            if (disp) g_disparosUlt++; else g_explosUlt++;
        }
        g_vida.resize(w);
        for (int i = 0; i < g_nFuegoCuadro && g_fuegoAct; i++) {
            const LuzReg& l = g_fuegoCuadro[i]; float q[3] = { l.pos[0], l.pos[1], l.pos[2] + g_fuegoAlto };
            float rr = l.r * 2.0f; if (rr > 1.0f) rr = 1.0f;                 // como Proper Shaders (0x1001C110)
            anade(q, l.radio, rr, l.g, l.b, C_FUEGO, nullptr); g_fuegoUlt++;
        }
    }
    // CPointLights: faros (tipo 1) y luces puntuales que no vienen de un 2DFX (fuego, explosiones...)
    fireIndex.build(g_fuegoCuadro,g_fuegoAct?g_nFuegoCuadro:0);
    lifeIndex.build(g_vida.data(),(int)g_vida.size());
    uint32_t n = *(uint32_t*)0xC3F0D0; if (n > 32) n = 32;
    const PuntoLuz* pl = (const PuntoLuz*)0xC3F0E0;
    for (uint32_t i = 0; i < n; i++) {
        const PuntoLuz& l = pl[i];
        if (l.radio <= 0.0f || (l.r + l.g + l.b) <= 0.0f) continue;
        if (l.tipo == 1) { if (g_luzFaros && !luz_vehiculo(l)) anade(l.pos, l.radio, l.r, l.g, l.b, C_FARO, l.dir); continue; }
        if (l.tipo != 0) continue;
        bool dup = false;
        if(!dup)dup=regIndex.first(g_regCuadro,g_nRegCuadro,l.pos,0.05f)>=0;
        if(!dup)dup=semIndex.first(g_regSemCuadro,g_nRegSemCuadro,l.pos,0.05f)>=0;
        if(!dup && g_fuegoAct)dup=fireIndex.first(g_fuegoCuadro,g_nFuegoCuadro,l.pos,0.05f)>=0;
        if(!dup)dup=lifeIndex.first(g_vida.data(),(int)g_vida.size(),l.pos,1.0f)>=0;
        if (!dup) anade(l.pos, l.radio, l.r, l.g, l.b, C_OTRA, nullptr);   // 1.6: ya no reciben la ganancia de las farolas
    }
    for (const Cand& c : g_inyectadas) g_cand.push_back(c);
}
static int cmp_cand(const void* a, const void* b) { float x = ((const Cand*)a)->dist, y = ((const Cand*)b)->dist; return x < y ? -1 : (x > y ? 1 : 0); }
// Rectangulo de pantalla (clip) que cubre la esfera de la luz: 8 esquinas de su caja; si alguna queda detras
// de la camara, pantalla completa. 0 = no se ve.
static int rect_luz(const Cand& c, float* r) {
    float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f; int detras = 0;
    for (int k = 0; k < 8; k++) {
        float x = c.pos[0] + ((k & 1) ? c.radio : -c.radio), y = c.pos[1] + ((k & 2) ? c.radio : -c.radio), z = c.pos[2] + ((k & 4) ? c.radio : -c.radio);
        float cx = x * g_vp[0] + y * g_vp[4] + z * g_vp[8] + g_vp[12], cy = x * g_vp[1] + y * g_vp[5] + z * g_vp[9] + g_vp[13];
        float cw = x * g_vp[3] + y * g_vp[7] + z * g_vp[11] + g_vp[15];
        if (cw < 0.05f) { detras++; continue; }
        float nx = cx / cw, ny = cy / cw;
        if (nx < mnx) mnx = nx; if (nx > mxx) mxx = nx; if (ny < mny) mny = ny; if (ny > mxy) mxy = ny;
    }
    if (detras == 8) return 0;
    if (detras) { r[0] = -1; r[1] = -1; r[2] = 1; r[3] = 1; return 1; }
    float m = 4.0f / (float)g_W;                                 // dos pixeles de margen
    r[0] = mnx - m < -1 ? -1 : mnx - m; r[1] = mny - m < -1 ? -1 : mny - m; r[2] = mxx + m > 1 ? 1 : mxx + m; r[3] = mxy + m > 1 ? 1 : mxy + m;
    return r[0] < r[2] && r[1] < r[3];
}
struct VertLuz { float p[4], lp[4], lc[4], ld[4]; };
static VertLuz g_vert[MAXL * 6];
// 2.0b: haces volumetricos. Candidatos: focos de farola (tipo 2) y, si se pide, faros y semaforos, con un radio
// de luz entre RadioMin y RadioMax (como MinSourceRadius/MaxSourceRadius de PS); los MaxHaces mas cercanos.
// Se dibujan sobre la escena (destino 0 ya puesto, mezcla ONE/ONE) despues de la composicion de las luces.
static void pasada_haces(IDirect3DDevice9* d, int m) {
    float niebla = *(float*)0xC81300, lluvia = *(float*)0xC81324;
    if (!(niebla > 0.0f)) niebla = 0.0f; if (niebla > 1.0f) niebla = 1.0f; if (!(lluvia > 0.0f)) lluvia = 0.0f; if (lluvia > 1.0f) lluvia = 1.0f;
    float boost = 1.0f + niebla * g_hazNiebla + lluvia * g_hazLluvia;
    if (g_hazClima) {   // 2.0i: lampHaze de SkyGFX (FUN_100586e0): brillo por clima con los pesos niebla > lluvia > nubes
        float nubes = *(float*)0xC81304; if (!(nubes > 0.0f)) nubes = 0.0f; if (nubes > 1.0f) nubes = 1.0f;
        float wf = niebla, wr = (1.0f - niebla) * lluvia, wc = (1.0f - niebla) * nubes * (1.0f - lluvia), wl = 1.0f - wf - wr - wc; if (wl < 0.0f) wl = 0.0f;
        float b = wl * g_hazB[0] + wc * g_hazB[1] + wr * g_hazB[2] + wf * g_hazB[3];
        boost = b / (g_hazB[0] > 1e-3f ? g_hazB[0] : 1e-3f);
    }
    g_hazBoostUlt = boost;
    float rad = sqrtf(g_hazLargo * g_hazLargo * 0.25f + g_hazRadio * g_hazRadio);
    int nv = 0, nh = 0;
    for (int k = 0; k < m && nh < g_hazMax; k++) {
        const Cand& cd = g_cand[k];
        bool vale = cd.tipo > 1.5f || (g_hazFaros && cd.clase == C_FARO) || (g_hazSem && cd.clase == C_SEM);
        if (!vale || cd.radio < g_hazRMin || cd.radio > g_hazRMax || cd.col[0] + cd.col[1] + cd.col[2] <= 1e-4f) continue;
        float dir[3] = { 0, 0, -1 };
        if (cd.clase == C_FARO) { float n = sqrtf(cd.dir[0] * cd.dir[0] + cd.dir[1] * cd.dir[1] + cd.dir[2] * cd.dir[2]); if (n < 1e-3f) continue; dir[0] = cd.dir[0] / n; dir[1] = cd.dir[1] / n; dir[2] = cd.dir[2] / n; }
        else if (cd.tipo > 1.5f) { dir[0] = cd.dir[0]; dir[1] = cd.dir[1]; dir[2] = cd.dir[2]; }
        Cand esf = cd; esf.radio = rad;
        for (int j = 0; j < 3; j++) esf.pos[j] = cd.pos[j] + dir[j] * g_hazLargo * 0.5f;
        float r[4]; if (!rect_luz(esf, r)) continue;
        float xy[6][2] = { { r[0], r[3] }, { r[2], r[3] }, { r[0], r[1] }, { r[0], r[1] }, { r[2], r[3] }, { r[2], r[1] } };
        for (int v = 0; v < 6; v++) {
            VertLuz& vl = g_vert[nv++];
            vl.p[0] = xy[v][0]; vl.p[1] = xy[v][1]; vl.p[2] = 0; vl.p[3] = 1;
            vl.lp[0] = cd.pos[0] - g_cam[0]; vl.lp[1] = cd.pos[1] - g_cam[1]; vl.lp[2] = cd.pos[2] - g_cam[2]; vl.lp[3] = 0;   // 2.0g
            vl.lc[0] = cd.col[0]; vl.lc[1] = cd.col[1]; vl.lc[2] = cd.col[2]; vl.lc[3] = 0;
            vl.ld[0] = dir[0]; vl.ld[1] = dir[1]; vl.ld[2] = dir[2]; vl.ld[3] = 0;
        }
        nh++;
    }
    g_hacesUlt = nh;
    if (!nv) return;
    const float g2r = 3.14159265f / 180.0f;
    float ci = cosf(g_hazAngInt * g2r), ce = cosf(g_hazAngExt * g2r), dif = ci - ce;
    float c[2][4] = { { ci, ce, tanf(g_hazAngExt * g2r), g_hazLargo },
                      { g_hazRadio, g_hazDens * g_hazInt * boost, dif > 1e-4f ? 1.0f / dif : 10000.0f, 1.0f / g_hazLargo } };
    d->SetPixelShaderConstantF(8, &c[0][0], 2);
    d->SetVertexShader(g_vsL); d->SetPixelShader(g_psH); d->SetVertexDeclaration(g_declL);
    d->DrawPrimitiveUP(D3DPT_TRIANGLELIST, nv / 3, g_vert, sizeof(VertLuz));
}
// ---------------------------------------------------------------- 2.0i: luces del cuadro para otros plugins
struct LNE_Luz { float pos[3], radio, col[3], tipo, dir[3], cosCono; };
static_assert(sizeof(LNE_Luz) == 48, "LNE_Luz");
static LNE_Luz g_exp[MAXL]; static int g_nExp = 0; static uint32_t g_expCuadro = 0;
static void exporta_luces(int m) {
    g_nExp = 0; g_expCuadro++;
    for (int k = 0; k < m && k < MAXL; k++) {
        const Cand& c = g_cand[k]; LNE_Luz& e = g_exp[g_nExp++];
        memcpy(e.pos, c.pos, 12); e.radio = c.radio; memcpy(e.col, c.col, 12); e.tipo = c.tipo; memcpy(e.dir, c.dir, 12); e.cosCono = c.cosCono;
    }
}

// ---------------------------------------------------------------- 2.0i: lluvia iluminada (rainLit de SkyGFX)
typedef bool(__cdecl* PVertLine)(const float*, float, void*, void**, bool, bool, bool, bool, bool, bool, void*);
static const PVertLine CWorld_ProcessVerticalLine = (PVertLine)0x5674E0;
typedef bool(__cdecl* PFb)();
static const PFb CCullZones_CamNoRain = (PFb)0x72DDB0, CCullZones_PlayerNoRain = (PFb)0x72DDC0;
struct Gota { float p[3]; };
static std::vector<Gota> g_gotas; static uint32_t g_azar = 0x2545F491u;
static float azar01() { g_azar = g_azar * 1664525u + 1013904223u; return (float)(g_azar >> 8) * (1.0f / 16777216.0f); }
// Techos: rejilla de columnas de 4 m alrededor de la camara; en cada una, la altura del primer impacto de una linea
// vertical desde 60 m por encima (edificios y objetos, sin lo que se ve a traves). Bajo esa altura no llueve: porches,
// puentes, tuneles, garajes e interiores que el juego tiene en el area 0 (los de Mixed incluidos).
static const int kRej = 15; static const float kCelda = 4.0f;
static float g_techo[kRej * kRej]; static uint8_t g_techoOk[kRej * kRej]; static int g_techoX0 = 0x7FFFFFFF, g_techoY0 = 0x7FFFFFFF, g_techoSig = 0;
static int lluvia_posible() {
    if (!g_lluvia) return 0;
    float lluvia = *(float*)0xC81324; if (!(lluvia > 0.001f)) return 0;
    if (*(int*)0xB72914 != 0) return 0;                     // CGame::currArea: interior
    if (*(float*)0xC8132C > 0.0f) return 0;                 // CWeather::UnderWaterness
    if (CCullZones_CamNoRain() || CCullZones_PlayerNoRain()) return 0;   // zonas sin lluvia del juego
    if (g_cam[2] > 900.0f) return 0;
    return 1;
}
static void actualiza_techos() {   // en el hilo de render, en RenderEffects (como las pruebas de vision de las coronas)
    if (!g_vpValido || !g_lluvia || !(*(float*)0xC81324 > 0.001f) || *(int*)0xB72914 != 0) return;
    int x0 = (int)floorf(g_cam[0] / kCelda) - kRej / 2, y0 = (int)floorf(g_cam[1] / kCelda) - kRej / 2;
    if (x0 != g_techoX0 || y0 != g_techoY0) {   // la rejilla se desplaza con la camara conservando lo ya medido
        float t2[kRej * kRej] = {}; uint8_t o2[kRej * kRej] = {};
        for (int j = 0; j < kRej; j++) for (int i = 0; i < kRej; i++) {
            int oi = i + x0 - g_techoX0, oj = j + y0 - g_techoY0;
            if (g_techoX0 != 0x7FFFFFFF && oi >= 0 && oi < kRej && oj >= 0 && oj < kRej) { t2[j * kRej + i] = g_techo[oj * kRej + oi]; o2[j * kRej + i] = g_techoOk[oj * kRej + oi]; }
        }
        memcpy(g_techo, t2, sizeof t2); memcpy(g_techoOk, o2, sizeof o2); g_techoX0 = x0; g_techoY0 = y0;
    }
    int hechos = 0;
    for (int pasada = 0; pasada < 2 && hechos < 16; pasada++)   // primero las que faltan, luego refresco por turno
        for (int n = 0; n < kRej * kRej && hechos < 16; n++) {
            int i = (g_techoSig + n) % (kRej * kRej);
            if (pasada == 0 && g_techoOk[i]) continue;
            float o[3] = { (x0 + i % kRej + 0.5f) * kCelda, (y0 + i / kRej + 0.5f) * kCelda, g_cam[2] + 60.0f };
            alignas(4) uint8_t col[0x2C] = {}; void* ent = nullptr;
            bool hay = CWorld_ProcessVerticalLine(o, 160.0f, col, &ent, true, false, false, true, false, true, nullptr);
            float z = *(float*)(col + 8);
            g_techo[i] = hay ? z : -1.0e9f; g_techoOk[i] = 1; hechos++;
            if (pasada == 1) g_techoSig = (i + 1) % (kRej * kRej);
        }
    g_techosUlt = hechos;
}
static float techo_en(float x, float y) {
    int i = (int)floorf(x / kCelda) - g_techoX0, j = (int)floorf(y / kCelda) - g_techoY0;
    if (i < 0 || i >= kRej || j < 0 || j >= kRej || !g_techoOk[j * kRej + i]) return 1.0e9f;   // sin medir: no se dibuja
    return g_techo[j * kRej + i];
}
static void pasada_lluvia2(IDirect3DDevice9* d);
static void pasada_lluvia(IDirect3DDevice9* d) {
    g_lluviaUlt = 0;
    if (g_lluModo == 0) return;
    if (g_lluModo == 2) { pasada_lluvia2(d); return; }
    if (!g_lluvia || !g_vpValido || g_lluN <= 0) return;
    if (!crea_recursos(d) || !g_psR || !lluvia_posible()) return;
    float lluvia = *(float*)0xC81324; if (lluvia > 1.0f) lluvia = 1.0f;
    const float D = g_lluDist, Zab = -6.0f, Zar = 14.0f, H = Zar - Zab;
    if ((int)g_gotas.size() != g_lluN) {
        g_gotas.resize(g_lluN);
        for (auto& g : g_gotas) { g.p[0] = g_cam[0] + (azar01() * 2 - 1) * D; g.p[1] = g_cam[1] + (azar01() * 2 - 1) * D; g.p[2] = g_cam[2] + Zab + azar01() * H; }
    }
    bool pausa = *(uint8_t*)0xB7CB48 || *(uint8_t*)0xB7CB49;   // CTimer::m_CodePause / m_UserPause
    float dt = pausa ? 0.0f : *(float*)0xB7CB5C / 50.0f; if (!(dt > 0.0f)) dt = 0.0f; if (dt > 0.1f) dt = 0.1f;
    const float* wd = (const float*)0xC813E0; float viento = *(float*)0xC812F0; if (!(viento > 0.0f)) viento = 0.0f; if (viento > 2.0f) viento = 2.0f;
    float wl = sqrtf(wd[0] * wd[0] + wd[1] * wd[1]), wx = wl > 1e-3f ? wd[0] / wl : 0.0f, wy = wl > 1e-3f ? wd[1] / wl : 0.0f;
    float incl = tanf(g_lluAng * 3.14159265f / 180.0f) + g_lluViento * viento;
    float v[3] = { wx * g_lluVel * incl, wy * g_lluVel * incl, -g_lluVel };
    float vlen = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]), vn[3] = { v[0] / vlen, v[1] / vlen, v[2] / vlen };
    for (auto& g : g_gotas) {
        for (int j = 0; j < 3; j++) g.p[j] += v[j] * dt;
        if (g.p[2] < g_cam[2] + Zab || g.p[2] > g_cam[2] + Zar + 1.0f) { g.p[2] = g_cam[2] + Zar - azar01() * 2.0f; g.p[0] = g_cam[0] + (azar01() * 2 - 1) * D; g.p[1] = g_cam[1] + (azar01() * 2 - 1) * D; }
        for (int j = 0; j < 2; j++) { float r = g.p[j] - g_cam[j]; if (r > D) g.p[j] -= 2 * D; else if (r < -D) g.p[j] += 2 * D; }
    }
    // Luz ambiente de la estela: color de la parte baja del cielo del timecycle (CTimeCycle::m_CurrentColours.m_nSkyBottom)
    float amb[3] = { *(uint16_t*)0xB7C4CA / 255.0f * g_lluAmb, *(uint16_t*)0xB7C4CC / 255.0f * g_lluAmb, *(uint16_t*)0xB7C4CE / 255.0f * g_lluAmb };
    int cerca[MAXL], nc = 0;
    for (int k = 0; k < g_nExp; k++) { const LNE_Luz& l = g_exp[k]; float dx = l.pos[0] - g_cam[0], dy = l.pos[1] - g_cam[1], dz = l.pos[2] - g_cam[2];
        if (sqrtf(dx * dx + dy * dy + dz * dz) < D * 1.5f + l.radio && l.col[0] + l.col[1] + l.col[2] > 1e-4f) cerca[nc++] = k; }
    int activas = (int)(g_lluN * lluvia), nv = 0;
    const float* M = g_vpRel; float W = (float)g_W, Hs = (float)g_H;
    static std::vector<VertLuz> vb; vb.resize((size_t)g_lluN * 6);
    for (int i = 0; i < activas; i++) {
        const Gota& g = g_gotas[i];
        float r[3] = { g.p[0] - g_cam[0], g.p[1] - g_cam[1], g.p[2] - g_cam[2] };
        float dist = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
        if (dist > D || dist < 0.6f) continue;
        if (g.p[2] < techo_en(g.p[0], g.p[1]) - 0.1f) continue;   // bajo techo
        float c[3] = { amb[0], amb[1], amb[2] };
        float aCam[3] = { -r[0] / dist, -r[1] / dist, -r[2] / dist };
        for (int n = 0; n < nc; n++) {
            const LNE_Luz& l = g_exp[cerca[n]];
            float q[3] = { g.p[0] - l.pos[0], g.p[1] - l.pos[1], g.p[2] - l.pos[2] };
            float dd = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]); if (dd >= l.radio || dd < 1e-3f) continue;
            float t = 1.0f - dd / l.radio, a = t * t;
            for (int j = 0; j < 3; j++) q[j] /= dd;
            if (l.tipo > 0.5f) { float cs = q[0] * l.dir[0] + q[1] * l.dir[1] + q[2] * l.dir[2]; float den = 1.0f - l.cosCono; float s = (cs - l.cosCono) / (den > 1e-3f ? den : 1e-3f); if (s <= 0.0f) continue; if (s > 1.0f) s = 1.0f; a *= s * s; }
            float f = q[0] * aCam[0] + q[1] * aCam[1] + q[2] * aCam[2]; f = f > 0.0f ? f * f * f : 0.0f;   // contraluz: la gota brilla
            a *= 0.45f + 1.55f * f;
            c[0] += l.col[0] * a; c[1] += l.col[1] * a; c[2] += l.col[2] * a;
        }
        float fade = 1.0f - (dist / D) * (dist / D), k = g_lluOpac * g_lluInt * fade * (0.5f + 0.5f * lluvia);
        for (int j = 0; j < 3; j++) c[j] *= g_lluTinte[j] * k;
        if (c[0] * 0.2126f + c[1] * 0.7152f + c[2] * 0.0722f < 0.002f) continue;
        float b[3] = { r[0] - vn[0] * g_lluLargo, r[1] - vn[1] * g_lluLargo, r[2] - vn[2] * g_lluLargo };
        float cA[4], cB[4];
        for (int j = 0; j < 4; j++) { cA[j] = r[0] * M[j] + r[1] * M[4 + j] + r[2] * M[8 + j] + M[12 + j]; cB[j] = b[0] * M[j] + b[1] * M[4 + j] + b[2] * M[8 + j] + M[12 + j]; }
        if (cA[3] < 0.1f || cB[3] < 0.1f) continue;
        float ax = cA[0] / cA[3], ay = cA[1] / cA[3], bx = cB[0] / cB[3], by = cB[1] / cB[3];
        float px = (ax - bx) * W * 0.5f, py = (ay - by) * Hs * 0.5f, pl = sqrtf(px * px + py * py);
        if (pl < 1e-4f) { px = 0.0f; py = 1.0f; pl = 1e-4f; }
        float ux = px / pl, uy = py / pl;
        if (pl < 1.0f) { float e = (1.0f - pl) * 0.5f; ax += ux * e * 2.0f / W; ay += uy * e * 2.0f / Hs; bx -= ux * e * 2.0f / W; by -= uy * e * 2.0f / Hs; }
        float ox = -uy * g_lluGrosor * 2.0f / W, oy = ux * g_lluGrosor * 2.0f / Hs;
        float zA = cA[2] / cA[3], zB = cB[2] / cB[3];
        float pv[4][5] = { { bx - ox, by - oy, zB, -1, 0 }, { bx + ox, by + oy, zB, 1, 0 }, { ax - ox, ay - oy, zA, -1, 1 }, { ax + ox, ay + oy, zA, 1, 1 } };
        static const int ord[6] = { 0, 1, 2, 2, 1, 3 };
        for (int o = 0; o < 6; o++) { VertLuz& vl = vb[nv++]; const float* s = pv[ord[o]];
            vl.p[0] = s[0]; vl.p[1] = s[1]; vl.p[2] = 0; vl.p[3] = 1; vl.lp[0] = s[2]; vl.lp[1] = s[3]; vl.lp[2] = s[4]; vl.lp[3] = 0;
            vl.lc[0] = c[0]; vl.lc[1] = c[1]; vl.lc[2] = c[2]; vl.lc[3] = 0; vl.ld[0] = vl.ld[1] = vl.ld[2] = vl.ld[3] = 0; }
    }
    g_lluviaUlt = nv / 6;
    if (!nv) return;
    IDirect3DSurface9* ds = nullptr; IDirect3DTexture9* prof = nullptr; IDirect3DSurface9* mrt[3] = {};
    if (FAILED(d->GetDepthStencilSurface(&ds)) || !ds) return;
    if (FAILED(ds->GetContainer(__uuidof(IDirect3DTexture9), (void**)&prof)) || !prof) { ds->Release(); return; }
    IDirect3DSurface9* rt0 = nullptr; d->GetRenderTarget(0, &rt0); D3DSURFACE_DESC dd = {}; if (rt0) rt0->GetDesc(&dd);
    if (!rt0 || !dd.Width || !dd.Height) { if (rt0) rt0->Release(); prof->Release(); ds->Release(); return; }
    g_sb->Capture();
    for (int i = 0; i < 3; i++) { d->GetRenderTarget(i + 1, &mrt[i]); if (mrt[i]) d->SetRenderTarget(i + 1, nullptr); }
    d->SetDepthStencilSurface(nullptr);
    d->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT); d->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT); d->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    d->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP); d->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP); d->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, 0);
    d->SetRenderState(D3DRS_ZENABLE, FALSE); d->SetRenderState(D3DRS_ZWRITEENABLE, FALSE); d->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    d->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE); d->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE); d->SetRenderState(D3DRS_FOGENABLE, FALSE);
    d->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE); d->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE); d->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    d->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
    d->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE); d->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD); d->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE); d->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
    float c2[2][4] = { { 0, 0, 0, 1.0f / (float)dd.Height }, { 0, 0, 0, 1.0f / (float)dd.Width } };
    d->SetPixelShaderConstantF(5, &c2[0][0], 2);
    d->SetTexture(0, prof);
    d->SetVertexShader(g_vsL); d->SetPixelShader(g_psR); d->SetVertexDeclaration(g_declL);
    d->DrawPrimitiveUP(D3DPT_TRIANGLELIST, nv / 3, vb.data(), sizeof(VertLuz));
    g_sb->Apply();
    d->SetDepthStencilSurface(ds);
    for (int i = 0; i < 3; i++) if (mrt[i]) { d->SetRenderTarget(i + 1, mrt[i]); mrt[i]->Release(); }
    d->SetTexture(0, nullptr);
    rt0->Release(); prof->Release(); ds->Release();
}

// ---------------------------------------------------------------- 2.0j: lluvia nueva (lluvia2.h)
static void pasada_lluvia2(IDirect3DDevice9* d) {
    bool pausa = *(uint8_t*)0xB7CB48 || *(uint8_t*)0xB7CB49;   // CTimer::m_CodePause / m_UserPause
    float dt = pausa ? 0.0f : *(float*)0xB7CB5C / 50.0f; if (!(dt > 0.0f)) dt = 0.0f; if (dt > 0.1f) dt = 0.1f;
    lluvia2::Entrada in = {};
    in.dt = dt; in.cam[0] = g_cam[0]; in.cam[1] = g_cam[1]; in.cam[2] = g_cam[2];
    in.lluvia = lluvia_posible() ? *(float*)0xC81324 : 0.0f;   // con 0 solo se actualiza la velocidad de la camara
    if (!g_lluvia || !g_vpValido || !g_albValido || in.lluvia <= 0.001f) { if (g_vpValido) lluvia2::Dibuja(g_ll2, d, g_ll2Cfg, in); return; }
    if (!crea_recursos(d)) return;
    char err[96] = "";
    if (!lluvia2::Crea(g_ll2, d, g_ll2Cfg, err, sizeof err)) { L("ERROR al crear la lluvia nueva (%s): vuelve la lluvia de la 2.0i (Modo=1)", err); g_lluModo = 1; return; }
    memcpy(in.vpRel, g_vpRel, sizeof g_vpRel); in.projA = g_projA; in.projB = g_projB; in.projYs = g_projYs;
    const float* wd = (const float*)0xC813E0; float viento = *(float*)0xC812F0; if (!(viento > 0.0f)) viento = 0.0f; if (viento > 2.0f) viento = 2.0f;
    in.vientoDir[0] = wd[0]; in.vientoDir[1] = wd[1]; in.vientoFuerza = viento;
    // luz difusa: color bajo del cielo del timecycle x Ambiente, nunca por debajo de AmbienteMinimo (de noche la ciudad)
    for (int k = 0; k < 3; k++) { float a = *(uint16_t*)(0xB7C4CA + 2 * k) / 255.0f * g_lluAmb; in.ambiente[k] = a > g_lluAmbMin[k] ? a : g_lluAmbMin[k]; }
    in.luces = (const lluvia2::Luz*)g_exp; in.nLuces = g_nExp;
    in.techo = g_techo; in.techoOk = g_techoOk; in.techoX0 = g_techoX0; in.techoY0 = g_techoY0; in.rej = kRej; in.celda = kCelda;
    {   // cielo abierto alrededor (cortinas): columnas medidas a menos de 2 celdas sin superficie por encima de la camara
        int ab = 0, tot = 0, cx = (int)floorf(g_cam[0] / kCelda) - g_techoX0, cy = (int)floorf(g_cam[1] / kCelda) - g_techoY0;
        for (int j = cy - 2; j <= cy + 2; j++) for (int i = cx - 2; i <= cx + 2; i++) {
            if (i < 0 || j < 0 || i >= kRej || j >= kRej || !g_techoOk[j * kRej + i]) continue;
            tot++; if (g_techo[j * kRej + i] < g_cam[2] + 0.5f) ab++; }
        in.abierto = tot ? (float)ab / (float)tot : 0.0f;
    }
    IDirect3DSurface9* ds = nullptr; IDirect3DTexture9* prof = nullptr; IDirect3DSurface9* mrt[3] = {};
    if (FAILED(d->GetDepthStencilSurface(&ds)) || !ds) return;
    if (FAILED(ds->GetContainer(__uuidof(IDirect3DTexture9), (void**)&prof)) || !prof) { ds->Release(); return; }
    IDirect3DSurface9* rt0 = nullptr; d->GetRenderTarget(0, &rt0); D3DSURFACE_DESC dd = {}; if (rt0) rt0->GetDesc(&dd);
    if (!rt0 || !dd.Width || !dd.Height) { if (rt0) rt0->Release(); prof->Release(); ds->Release(); return; }
    in.escena = g_alb; in.prof = prof; in.luz = (g_accOk && g_acc && g_accW == dd.Width && g_accH == dd.Height) ? g_acc : nullptr;
    in.W = (int)dd.Width; in.H = (int)dd.Height;
    g_sb->Capture();
    for (int i = 0; i < 3; i++) { d->GetRenderTarget(i + 1, &mrt[i]); if (mrt[i]) d->SetRenderTarget(i + 1, nullptr); }
    d->SetDepthStencilSurface(nullptr);
    lluvia2::Dibuja(g_ll2, d, g_ll2Cfg, in);
    g_lluviaUlt = g_ll2.gotasUlt;
    g_sb->Apply();
    d->SetDepthStencilSurface(ds);
    for (int i = 0; i < 3; i++) if (mrt[i]) { d->SetRenderTarget(i + 1, mrt[i]); mrt[i]->Release(); }
    rt0->Release(); prof->Release(); ds->Release();
}

// ---------------------------------------------------------------- 2.0i: particulas FX y lluvia del juego tras ENB
typedef void(__fastcall* PFxRender)(void*, void*, void*, int);   // Fx_c::Render(this, RwCamera*, bool) como thiscall
static PFxRender g_fxDest; static void* g_fxSelf; static void* g_fxCam; static int g_fxPend = 0;
static void __fastcall h_fx(void* self, void* edx, void* cam, int calor) {
    if (g_partTras && g_pendiente && !(calor & 0xFF)) { g_fxSelf = self; g_fxCam = cam; g_fxPend = 1; return; }
    g_fxDest(self, edx, cam, calor);
}
static PFv g_lluJuegoDest; static int g_lluJuegoPend = 0;
static void __cdecl h_lluvia_juego() {   // CWeather::RenderRainStreaks desde RenderScene (0x53E126)
    if (g_lluvia && !g_lluviaJuego) return;                          // la sustituye la lluvia iluminada
    if (g_lluviaJuego && (g_coronasTras || g_luces)) { g_lluJuegoPend = 1; return; }   // tras ENB, que la borraba
    g_lluJuegoDest();
}
static void dibuja_diferidos(IDirect3DDevice9* d) {
    if (d) __try { pasada_lluvia(d); } __except (EXCEPTION_EXECUTE_HANDLER) { g_lluvia = 0; L("EXCEPCION en la lluvia iluminada: apagada"); }
    if (g_lluJuegoPend) { g_lluJuegoPend = 0; if (*(void**)0xC9BCC0) __try { g_lluJuegoDest(); g_lluJuegoDif++; } __except (EXCEPTION_EXECUTE_HANDLER) { g_lluviaJuego = 0; L("EXCEPCION en la lluvia del juego diferida: vuelve a su sitio"); } }
}
static void dibuja_particulas() {
    if (!g_fxPend) return;
    g_fxPend = 0;
    if (!*(void**)0xC9BCC0) return;   // sin camara de RW activa (ver coronas)
    __try { g_fxDest(g_fxSelf, nullptr, g_fxCam, 0); g_fxDiferidos++; }
    __except (EXCEPTION_EXECUTE_HANDLER) { g_partTras = 0; L("EXCEPCION al dibujar las particulas tras ENB: vuelven a su sitio"); }
}

static void pasada_luces(IDirect3DDevice9* d) {
    g_nExp = 0; g_accOk = 0;
    if (!g_vpValido || !g_albValido) return;
    junta_luces();
    fusiona();
    int nc = (int)g_cand.size();
    if (nc > 1) qsort(g_cand.data(), nc, sizeof(Cand), cmp_cand);
    int m = nc < g_maxLuces ? nc : g_maxLuces;
    // 1.4: si hay mas luces que el maximo, las del final de la lista entran desvanecidas (sin saltos)
    if (nc > m && m > 0) { float corte = g_cand[m - 1].dist, ancho = 0.15f * corte + 1.0f;
        for (int k = 0; k < m; k++) { float f = (corte - g_cand[k].dist) / ancho; if (f < 1.0f) { if (f < 0) f = 0; g_cand[k].col[0] *= f; g_cand[k].col[1] *= f; g_cand[k].col[2] *= f; } } }
    g_lucesUlt = m;
    exporta_luces(m);   // 2.0i
    { long nf = 0; for (int k = 0; k < m; k++) if (g_cand[k].tipo > 1.5f) nf++; g_focosUlt = nf; }
    if (!m && !g_depuracion) return;
    float inv[16]; if (!inv44d(g_vpRel, inv)) return;   // 2.0g: relativa a la camara (antes inv44(g_vp))
    if (!crea_recursos(d)) { g_luces = 0; return; }

    IDirect3DSurface9* ds = nullptr; IDirect3DTexture9* prof = nullptr; IDirect3DSurface9* mrt[3] = {};
    if (FAILED(d->GetDepthStencilSurface(&ds)) || !ds) return;
    if (FAILED(ds->GetContainer(__uuidof(IDirect3DTexture9), (void**)&prof)) || !prof) { ds->Release(); if (g_fallos++ < 3) L("AVISO: la profundidad no es una textura (sin INTZ): no hay luces"); return; }
    IDirect3DSurface9* rt0 = nullptr; d->GetRenderTarget(0, &rt0);
    D3DSURFACE_DESC dd = {}; if (rt0) rt0->GetDesc(&dd);
    if (!rt0 || !crea_normal(d, dd.Width, dd.Height) || !crea_acumulador(d, dd.Width, dd.Height)) { if (rt0) rt0->Release(); prof->Release(); ds->Release(); return; }
    IDirect3DSurface9* sn = nullptr; g_norm->GetSurfaceLevel(0, &sn);
    IDirect3DSurface9* sa = nullptr; g_acc->GetSurfaceLevel(0, &sa);
    // 2.0a: especular solo con el suelo mojado (CWeather::WetRoads, 0..1)
    float moj = *(float*)0xC81308; if (!(moj > 0.0f)) moj = 0.0f; if (moj > 1.0f) moj = 1.0f; g_mojadoUlt = moj;
    IDirect3DSurface9* se = nullptr;
    if (g_espActivo && moj > 0.001f && !g_depuracion && crea_especular(d, dd.Width, dd.Height)) g_esp->GetSurfaceLevel(0, &se);

    g_sb->Capture();
    for (int i = 0; i < 3; i++) { d->GetRenderTarget(i + 1, &mrt[i]); if (mrt[i]) d->SetRenderTarget(i + 1, nullptr); }
    d->SetDepthStencilSurface(nullptr);
    for (int s = 0; s < 5; s++) {
        d->SetSamplerState(s, D3DSAMP_MINFILTER, D3DTEXF_POINT); d->SetSamplerState(s, D3DSAMP_MAGFILTER, D3DTEXF_POINT); d->SetSamplerState(s, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        d->SetSamplerState(s, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP); d->SetSamplerState(s, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP); d->SetSamplerState(s, D3DSAMP_SRGBTEXTURE, 0);
    }
    d->SetRenderState(D3DRS_ZENABLE, FALSE); d->SetRenderState(D3DRS_ZWRITEENABLE, FALSE); d->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    d->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE); d->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE); d->SetRenderState(D3DRS_FOGENABLE, FALSE);
    d->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE); d->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE); d->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);

    float c[12][4] = {};
    memcpy(c, inv, sizeof inv);
    c[4][0] = g_rodilla; c[4][1] = g_luzMax; c[4][2] = g_albedoMin; c[4][3] = g_fuente;
    c[5][0] = 0.0f; c[5][1] = 0.0f; c[5][2] = 0.0f; c[5][3] = 1.0f / (float)dd.Height;   // 2.0g: la camara es el origen
    c[6][0] = g_expAt; c[6][1] = g_envolvente; c[6][2] = g_pesoAlb; c[6][3] = 1.0f / (float)dd.Width;
    c[7][0] = se ? g_espInt * moj : 0.0f; c[7][1] = g_espNMin + (g_espNMax - g_espNMin) * moj; c[7][2] = (float)g_atenModo; c[7][3] = g_anchoBorde;
    c[11][1] = (float)g_depuracion; c[11][3] = g_radioNormal;
    d->SetPixelShaderConstantF(0, &c[0][0], 12);
    {   // 2.0i: VP relativa a la camara (c12..c15) y sombras de faros (c16)
        float s[5][4]; memcpy(s, g_vpRel, sizeof g_vpRel);
        s[4][0] = g_sombFaros ? (float)g_sombPasos : 0.0f; s[4][1] = g_sombGrosor; s[4][2] = g_sombSesgo; s[4][3] = g_sombFuerza;
        d->SetPixelShaderConstantF(12, &s[0][0], 5);
    }
    float W = (float)dd.Width, H = (float)dd.Height, ox = -1.0f / W, oy = 1.0f / H;   // medio texel de D3D9
    float q[4][6] = { { -1 + ox,  1 + oy, 0, 1, 0, 0 }, { 1 + ox,  1 + oy, 0, 1, 1, 0 }, { -1 + ox, -1 + oy, 0, 1, 0, 1 }, { 1 + ox, -1 + oy, 0, 1, 1, 1 } };

    // pasada 1: normal suavizada -> g_norm (sin mezcla)
    d->SetRenderTarget(0, sn);
    d->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE); d->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
    d->SetVertexShader(g_vs); d->SetPixelShader(g_psN); d->SetVertexDeclaration(g_decl);
    d->SetTexture(0, prof); d->SetTexture(1, nullptr); d->SetTexture(2, nullptr);
    d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof q[0]);

    // pasada 2: sobre la escena
    d->SetRenderTarget(0, rt0);
    d->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
    d->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE); d->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD); d->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    d->SetTexture(0, prof); d->SetTexture(1, g_alb); d->SetTexture(2, g_norm);
    if (g_depuracion) {
        d->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
        d->SetPixelShader(g_psD);
        d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof q[0]);
    } else {
        // 1.6: pasada 2 -> luz acumulada (sin albedo); pasada 3 -> composicion con rodilla sobre la escena
        if (se) { d->SetRenderTarget(0, se); d->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 0.0f, 0); }
        d->SetRenderTarget(0, sa);
        d->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 0.0f, 0);
        if (se) d->SetRenderTarget(1, se);
        d->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
        int nv = 0;
        for (int k = 0; k < m; k++) {
            const Cand& cd = g_cand[k]; float r[4];
            if (cd.col[0] + cd.col[1] + cd.col[2] <= 1e-4f || !rect_luz(cd, r)) continue;
            float xy[6][2] = { { r[0], r[3] }, { r[2], r[3] }, { r[0], r[1] }, { r[0], r[1] }, { r[2], r[3] }, { r[2], r[1] } };
            for (int v = 0; v < 6; v++) {
                VertLuz& vl = g_vert[nv++];
                vl.p[0] = xy[v][0]; vl.p[1] = xy[v][1]; vl.p[2] = 0; vl.p[3] = 1;
                vl.lp[0] = cd.pos[0] - g_cam[0]; vl.lp[1] = cd.pos[1] - g_cam[1]; vl.lp[2] = cd.pos[2] - g_cam[2]; vl.lp[3] = cd.radio;   // 2.0g
                vl.lc[0] = cd.col[0]; vl.lc[1] = cd.col[1]; vl.lc[2] = cd.col[2]; vl.lc[3] = cd.tipo;
                vl.ld[0] = cd.dir[0]; vl.ld[1] = cd.dir[1]; vl.ld[2] = cd.dir[2]; vl.ld[3] = cd.cosCono;
            }
        }
        g_rectUlt = nv / 6;
        if (nv) { d->SetVertexShader(g_vsL); d->SetPixelShader(g_psL); d->SetVertexDeclaration(g_declL); d->DrawPrimitiveUP(D3DPT_TRIANGLELIST, nv / 3, g_vert, sizeof(VertLuz)); }
        if (se) d->SetRenderTarget(1, nullptr);
        d->SetRenderTarget(0, rt0);
        d->SetTexture(3, g_acc); d->SetTexture(4, se ? g_esp : nullptr);
        d->SetVertexShader(g_vs); d->SetPixelShader(g_psC); d->SetVertexDeclaration(g_decl);
        d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof q[0]);
        g_accOk = 1;   // 2.0j
        if (g_haces && g_psH) pasada_haces(d, m);
    }
    g_nLuces++;

    g_sb->Apply();
    d->SetRenderTarget(0, rt0);
    d->SetDepthStencilSurface(ds);
    for (int i = 0; i < 3; i++) if (mrt[i]) { d->SetRenderTarget(i + 1, mrt[i]); mrt[i]->Release(); }
    for (int s = 0; s < 5; s++) d->SetTexture(s, nullptr);
    if (se) se->Release();
    sa->Release(); sn->Release(); rt0->Release(); prof->Release(); ds->Release();
}

// ---------------------------------------------------------------- enganches del juego
static void __cdecl h_coronas() {
    if (g_luces) escanea_seguro();
    g_nRegCuadro = (int)g_nReg; if (g_nRegCuadro > 1024) g_nRegCuadro = 1024; memcpy(g_regCuadro, g_reg, g_nRegCuadro * sizeof(LuzReg)); g_nReg = 0;
    g_nRegSemCuadro = (int)g_nRegSem; if (g_nRegSemCuadro > 256) g_nRegSemCuadro = 256; memcpy(g_regSemCuadro, g_regSem, g_nRegSemCuadro * sizeof(LuzReg)); g_nRegSem = 0;
    toma_fuego();
    if (g_nRegCuadro > g_regMax) g_regMax = g_nRegCuadro;
    IDirect3DDevice9* d = g_luces ? real_dev() : nullptr;
    if (d) __try { copia_escena(d); } __except (EXCEPTION_EXECUTE_HANDLER) { g_luces = 0; L("EXCEPCION en copia_escena: luces desactivadas"); }
    if (g_lluvia) __try { actualiza_techos(); } __except (EXCEPTION_EXECUTE_HANDLER) { g_lluvia = 0; L("EXCEPCION en la prueba de techos: lluvia iluminada apagada"); }
    if (g_coronasTras || g_luces) { g_pendiente = 1; if (!g_coronasTras) CCoronas_Render(); return; }
    CCoronas_Render();
}
static long g_sinCamara = 0;
static void dibuja_pendiente() {
    if (g_luces) { IDirect3DDevice9* d = real_dev(); if (d) __try { pasada_luces(d); } __except (EXCEPTION_EXECUTE_HANDLER) { g_luces = 0; L("EXCEPCION en la pasada de luces: luces desactivadas"); } }
    else g_nExp = 0;
    dibuja_diferidos(real_dev());   // 2.0i: lluvia despues de la luz local
    if (g_coronasTras) {
        // dgGGlobals.curCamera (0xC9BCC0): RwIm2DRenderIndexedPrimitive (destellos de CCoronas via
        // CSprite::FlushSpriteBuffer) lee curCamera->frameBuffer (+0x60). Sin camara activa no se dibuja.
        if (*(void**)0xC9BCC0) { CCoronas_Render(); g_nCoronas++; }
        else if (g_sinCamara++ < 5) L("AVISO: sin camara de RW activa al dibujar las coronas; este cuadro se omiten");
    }
    dibuja_particulas();   // 2.0i: el mismo orden que el juego (coronas y luego FX), ahora tras la composicion de ENB
}
// 1.0b: el enganche va en 0x704732 (call RsCameraBeginUpdate(Scene.m_pRwCamera)), NO en el PopContext de
// 0x704726. La copia de RwRasterRenderFast deja dgGGlobals.curCamera = NULL (0x7F999E) y el juego reactiva
// la camara con esta llamada; la 1.0 dibujaba antes y los destellos (indexados) leian NULL+0x60 ->
// crash 0x007FBD4A (__rwD3D9Im2DRenderIndexedPrimitive+0x1a) a los 5 s de juego (02-10, 00:46).
typedef int(__cdecl* PFbegin)(void*);
static const PFbegin RsCameraBeginUpdate = (PFbegin)0x619450;
static int __cdecl h_begin(void* cam) {
    int r = RsCameraBeginUpdate(cam);
    if (g_pendiente) {
        g_pendiente = 0; dibuja_pendiente();
        void* front = *(void**)0xC402D8;
        if (front && cam) { RwRasterPushContext(front); RwRasterRenderFast(*(void**)((uintptr_t)cam + 0x60), 0, 0); RwRasterPopContext(); r = RsCameraBeginUpdate(cam); }
    }
    return r;
}
static void __cdecl h_post() { CPostEffects_Render(); if (g_pendiente) { g_pendiente = 0; dibuja_pendiente(); g_nRespaldo++; } }

// ---------------------------------------------------------------- capturas y aviso en pantalla
static char g_aviso[200]; static double g_avisoHasta = 0; static CRGBA g_avisoCol = { 90, 230, 90, 255 };
static int g_fuentesOk = 0, g_cuadro = 0, g_tecAntes[4], g_tecDep = 0;
static void aviso(double seg, CRGBA c, const char* f, ...) { va_list a; va_start(a, f); vsnprintf(g_aviso, sizeof g_aviso, f, a); va_end(a); g_avisoHasta = ms() + seg * 1000.0; g_avisoCol = c; }
static void mueve_capturas() {
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA((g_dirJuego + "enb*.bmp").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    FILETIME ahora; GetSystemTimeAsFileTime(&ahora); ULONGLONG t = ((ULONGLONG)ahora.dwHighDateTime << 32) | ahora.dwLowDateTime;
    do {
        ULONGLONG w = ((ULONGLONG)fd.ftLastWriteTime.dwHighDateTime << 32) | fd.ftLastWriteTime.dwLowDateTime;
        if (t - w < 10000000ULL) continue;   // menos de 1 s: ENB puede seguir escribiendo
        std::string de = g_dirJuego + fd.cFileName, a = g_dirCapturas + fd.cFileName;
        CreateDirectoryA(g_dirCapturas.c_str(), nullptr);
        if (MoveFileExA(de.c_str(), a.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING)) {
            CRGBA v = { 90, 230, 90, 255 }; aviso(4, v, "CAPTURA TOMADA: %s", fd.cFileName); L("Captura movida: %s -> %s", fd.cFileName, g_dirCapturas.c_str());
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}
static void teclas() {
    if (!g_teclas) return;
    int k[4] = { g_tecLuces ? (GetAsyncKeyState(g_tecLuces) & 0x8000) != 0 : 0, g_tecCoronas ? (GetAsyncKeyState(g_tecCoronas) & 0x8000) != 0 : 0,
                 g_tecDep ? (GetAsyncKeyState(g_tecDep) & 0x8000) != 0 : 0, g_tecFases ? (GetAsyncKeyState(g_tecFases) & 0x8000) != 0 : 0 };
    CRGBA am = { 240, 220, 60, 255 }, ve = { 90, 230, 90, 255 };
    if (k[0] && !g_tecAntes[0]) { g_luces = !g_luces; aviso(4, g_luces ? ve : am, "LUCES LOCALES: %s", g_luces ? "ACTIVAS" : "APAGADAS"); L("TECLA luces -> %d", g_luces); }
    if (k[1] && !g_tecAntes[1]) { g_coronasTras = !g_coronasTras; aviso(4, g_coronasTras ? ve : am, "CORONAS: %s", g_coronasTras ? "DESPUES DE ENB" : "EN SU SITIO ORIGINAL"); L("TECLA coronas -> %d", g_coronasTras); }
    if (k[2] && !g_tecAntes[2]) {
        g_depuracion = (g_depuracion + 1) % 3;
        static const char* modo[3] = { "NORMAL", "SUAVIZADO (verde = completo, rojo = sin vecinos)", "NORMAL EN COLOR" };
        aviso(5, am, "DEPURACION LUCES: %s", modo[g_depuracion]); L("TECLA depuracion -> %d", g_depuracion);
    }
    if (k[3] && !g_tecAntes[3]) {   // 2.0: A/B de las novedades de la 2.0 (focos y especular)
        int on = !(g_focos || g_espActivo || g_haces || g_farosDobles || g_frenos || g_marchaAtras || g_disparos || g_explos || g_fuegoAct);
        g_focos = g_espActivo = g_haces = g_farosDobles = g_frenos = g_marchaAtras = g_disparos = g_explos = g_fuegoAct = on;
        aviso(4, on ? ve : am, "LUCES 2.0 (focos, mojado, haces, faros, frenos, disparos, explosiones, fuego): %s", on ? "ACTIVAS" : "APAGADAS (como la 1.8)"); L("TECLA 2.0 -> %d", on);
    }
    g_tecAntes[0] = k[0]; g_tecAntes[1] = k[1]; g_tecAntes[2] = k[2]; g_tecAntes[3] = k[3];
}
static void __cdecl h_fuentes() {
    __try {
        if (g_capturas && (++g_cuadro % 15) == 0) mueve_capturas();
        teclas();
        if (g_aviso[0] && ms() < g_avisoHasta) {
            float sw = (float)*(int*)0xC17044;   // RsGlobal.maximumWidth
            float sh = (float)*(int*)0xC17048;   // RsGlobal.maximumHeight
            CRGBA neg = { 0, 0, 0, 255 };
            F_SetBackground(false, false); F_SetProportional(true); F_SetJustify(false); F_SetOrientation(1 /*izquierda*/);
            F_SetFontStyle(1 /*subtitulos*/); F_SetScale(sw / 640.0f * 0.45f, sh / 448.0f * 0.9f); F_SetWrapx(sw); F_SetEdge(0);
            F_SetDropShadow(2); F_SetDropColor(neg); F_SetColor(g_avisoCol);
            F_PrintString(sw * 0.03f, sh * 0.03f, g_aviso);
        }
        if ((g_cuadro % 1800) == 0) L("SEG coronas tras ENB %ld | pasadas de luces %ld (hora %02d:%02d factor %.2f | luces %ld, fusionadas %ld, rectangulos %ld, farolas anotadas %d, max %ld) | escaner %s: vueltas %ld, en lista %u, anadidas %ld, emparejadas %ld, K %.3f (mediana %.3f) | semaforos: anotados %d, en lista %u, anadidos %ld | 2.0: focos %ld, mojado %.2f, haces %ld (refuerzo %.2f), coches %u, faros dobles %ld, frenos %ld, marcha atras %ld, disparos %ld (total %ld), explosiones %ld (total %ld), fuegos %ld | respaldo %ld | 2.0i: particulas tras ENB %ld, lluvia %ld estelas (techos %ld), lluvia juego diferida %ld, luces exportadas %d | 2.0j: clima %d->%d (%.2f) lluvia %.2f niebla %.2f, gotas %ld salpicaduras %ld cortinas %ld luces %ld", g_nCoronas, g_nLuces, (int)*(uint8_t*)0xB70153, (int)*(uint8_t*)0xB70152, g_factorHora, g_lucesUlt, g_fusionadasUlt, g_rectUlt, g_nRegCuadro, g_regMax, g_escar ? "si" : "NO", g_escVueltas, (unsigned)g_esc.size(), g_escUsadasUlt, g_escEmpUlt, g_escK, g_escMediana, g_nRegSemCuadro, (unsigned)g_sem.size(), g_semUsadosUlt, g_focosUlt, g_mojadoUlt, g_hacesUlt, g_hazBoostUlt, (unsigned)g_veh.size(), g_farosDoblesUlt, g_frenosUlt, g_marchaUlt, g_disparosUlt, g_disparosTot, g_explosUlt, g_explosTot, g_fuegoUlt, g_nRespaldo, g_fxDiferidos, g_lluviaUlt, g_techosUlt, g_lluJuegoDif, g_nExp, (int)*(int16_t*)0xC81320, (int)*(int16_t*)0xC8131C, *(float*)0xC8130C, *(float*)0xC81324, *(float*)0xC81300, g_ll2.gotasUlt, g_ll2.salpUlt, g_ll2.cortinasUlt, g_ll2.lucesUlt);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    CFont_DrawFonts();
}

// ---------------------------------------------------------------- instalacion
static void instala() {
    int ok = rel_a(0x53E18E, 0xE8, 0x6FAEC0) && rel_a(0x704726, 0xE8, 0x7FB110) && rel_a(0x70470F, 0xE8, 0x7FB060)
          && rel_a(0x704721, 0xE8, 0x7FAF50) && rel_a(0x53E227, 0xE9, 0x7046E0) && rel_a(0x704732, 0xE8, 0x619450)
          && *(uint32_t*)0x7046FE == 0xC1703C && *(uint32_t*)0x70470A == 0xC402D8 && *(uint32_t*)0x70472D == 0xC1703C;
    if (ok) {
        pon_rel(0x53E18E, 0xE8, (void*)h_coronas); pon_rel(0x704732, 0xE8, (void*)h_begin); pon_rel(0x53E227, 0xE9, (void*)h_post);
        L("Coronas/luces 1.0b: 0x53E18E, 0x704732 (RsCameraBeginUpdate tras la copia) y 0x53E227 enganchados");
    } else { g_coronasTras = g_luces = 0; L("Coronas/luces NO instaladas: las llamadas de RenderEffects/CPostEffects no son las esperadas (otro mod las cambio)"); }
    if (g_luces) {
        if (rel_a(0x6FD105, 0xE8, 0x7000E0) && rel_a(0x6FD347, 0xE8, 0x7000E0)) {
            pon_rel(0x6FD105, 0xE8, (void*)h_addlight_puente); pon_rel(0x6FD347, 0xE8, (void*)h_addlight_puente);
            L("Luces de farola: 0x6FD105 y 0x6FD347 (CPointLights::AddLight en ProcessLightsForEntity) anotadas antes del filtro de 27 m");
        } else L("AVISO: las llamadas a AddLight de ProcessLightsForEntity no son las esperadas; solo se usan las de CPointLights (27 m)");
        if (rel_a(0x49DF47, 0xE8, 0x7000E0)) { pon_rel(0x49DF47, 0xE8, (void*)h_addlight_sem); L("Semaforos: 0x49DF47 (AddLight en CTrafficLights::DisplayActualLight) anotado antes del filtro de 27 m"); }
        else L("AVISO: 0x49DF47 no es la llamada a AddLight de los semaforos; los cercanos salen de CPointLights");
    }
    if (g_luces) {   // 2.0d: disparos, explosiones y fuego
        struct { uintptr_t sitio; void* puente; const char* que; } s20d[] = {
            { 0x740D68, (void*)h_al_disparo, "CWeapon::FireInstantHit" }, { 0x73CCFD, (void*)h_al_disparo, "CWeapon::FireInstantHitFromCar2" },
            { 0x73AF74, (void*)h_al_disparo, "FireOneInstantHitRound" }, { 0x6D4D14, (void*)h_al_disparo, "CVehicle::DoPlaneGunFireFX" },
            { 0x737849, (void*)h_al_explosion, "CExplosion::Update (15 m)" }, { 0x7378C1, (void*)h_al_explosion, "CExplosion::Update (20 m)" },
            { 0x53AEAC, (void*)h_al_fuego, "CFire::ProcessFire" } };
        for (auto& e : s20d) {
            if (rel_a(e.sitio, 0xE8, 0x7000E0)) { pon_rel(e.sitio, 0xE8, e.puente); g_ganchos20d++; }
            else { const uint8_t* b = (const uint8_t*)e.sitio; L("AVISO: 0x%06X (%s) no es call AddLight (%02X %02X %02X %02X %02X); esa luz sigue como en la 1.8", (unsigned)e.sitio, e.que, b[0], b[1], b[2], b[3], b[4]); }
        }
        L("Disparos, explosiones y fuego: %d de 7 llamadas a AddLight anotadas antes del filtro de 27 m", g_ganchos20d);
    }
    if (g_luces && g_haces && g_quitaNiebla) {   // 2.0b: CPointLights::RenderFogEffect -> ret (Proper Shaders hace lo mismo)
        static const uint8_t orig[5] = { 0xA0, 0x51, 0xF8, 0xB5, 0x00 };
        if (!memcmp((void*)0x7002D0, orig, 5)) { uint8_t r = 0xC3; escribe(0x7002D0, &r, 1); L("Niebla de luces vanilla: 0x7002D0 (CPointLights::RenderFogEffect) -> ret"); }
        else { const uint8_t* b = (const uint8_t*)0x7002D0; L("AVISO: 0x7002D0 no es el inicio esperado de RenderFogEffect (%02X %02X %02X %02X %02X); no se toca", b[0], b[1], b[2], b[3], b[4]); }
    }
    if (g_coronasTras || g_luces) {   // 2.0i: los dos sitios se encadenan con lo que haya (otro mod puede haberlos cambiado)
        if (*(uint8_t*)0x53E1A0 == 0xE8 && *(uint32_t*)0x53E19C == 0xA9AE00) {
            g_fxDest = (PFxRender)(0x53E1A5 + *(int32_t*)0x53E1A1); pon_rel(0x53E1A0, 0xE8, (void*)h_fx);
            L("Particulas FX tras ENB: 0x53E1A0 (g_fx.Render en RenderEffects) -> %p%s", (void*)g_fxDest, (uintptr_t)g_fxDest == 0x49E650 ? "" : " (otro mod, encadenado)");
        } else { g_partTras = 0; const uint8_t* b = (const uint8_t*)0x53E19B; L("AVISO: 0x53E19B no es mov ecx,g_fx; call (%02X %02X %02X %02X %02X %02X); particulas en su sitio", b[0], b[1], b[2], b[3], b[4], b[5]); }
        if (*(uint8_t*)0x53E126 == 0xE8) {
            g_lluJuegoDest = (PFv)(0x53E12B + *(int32_t*)0x53E127); pon_rel(0x53E126, 0xE8, (void*)h_lluvia_juego);
            L("Lluvia del juego: 0x53E126 (CWeather::RenderRainStreaks en RenderScene) -> %p%s; %s", (void*)g_lluJuegoDest, (uintptr_t)g_lluJuegoDest == 0x72AF70 ? "" : " (otro mod, encadenado)",
              g_lluvia && !g_lluviaJuego ? "la sustituye la lluvia iluminada" : g_lluviaJuego ? "se dibuja tras ENB" : "sin cambios");
        } else { g_lluvia = 0; g_lluviaJuego = 0; L("AVISO: 0x53E126 no es una llamada; lluvia iluminada apagada"); }
    } else { g_partTras = 0; g_lluvia = 0; }
    if (rel_a(0x53E52B, 0xE9, 0x71A210)) { pon_rel(0x53E52B, 0xE9, (void*)h_fuentes); g_fuentesOk = 1; L("Aviso y capturas: 0x53E52B (Render2dStuff -> CFont::DrawFonts) enganchado"); }
    else L("Aviso y capturas NO instalados: 0x53E52B no es jmp CFont::DrawFonts");
}
// El envoltorio de ENB se crea mas tarde; su vtable esta en la imagen de enbseries.asi (EnbDevice9, 0x100A6ECC
// con base 0x10000000). Se engancha la entrada 16 (Reset) directamente en esa tabla estatica.
static void engancha_reset() {
    HMODULE enb = GetModuleHandleA("enbseries.asi");
    if (!enb) { L("Sin enbseries.asi: las luces no se activan (necesitan la profundidad INTZ de ENB)"); g_luces = 0; return; }
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)((uint8_t*)enb + ((IMAGE_DOS_HEADER*)enb)->e_lfanew);
    g_enbIni = (uintptr_t)enb; g_enbFin = g_enbIni + nt->OptionalHeader.SizeOfImage;
    uintptr_t vt = (uintptr_t)enb + (0x100A6ECC - 0x10000000);
    void** e = (void**)vt;
    if (IsBadReadPtr(e, 17 * 4) || (uintptr_t)e[16] < g_enbIni || (uintptr_t)e[16] >= g_enbFin) { L("Reset de ENB no encontrado en %p: las luces no se activan", e); g_luces = 0; return; }
    o_reset = (PReset)e[16]; void* nuevo = (void*)h_reset; escribe((uintptr_t)&e[16], &nuevo, 4);
    L("Reset del envoltorio de ENB enganchado (vtable %p, original %p)", e, o_reset);
}

// 2.0i: luces locales del ultimo cuadro (posicion en el mundo, radio, color con ganancia, tipo 0 puntual/1 faro/2 foco
// de farola, direccion y coseno del cono). Las usa HumoLucesENB para iluminar el humo como la lista de SkyGFX.
extern "C" __declspec(dllexport) int LNE_LucesCuadro(LNE_Luz* out, int max, uint32_t* cuadro) {
    if (cuadro) *cuadro = g_expCuadro;
    if (!out || max <= 0) return g_nExp;
    int n = g_nExp < max ? g_nExp : max; memcpy(out, g_exp, n * sizeof(LNE_Luz)); return n;
}
extern "C" __declspec(dllexport) int LNE_Version() { return 0x020B; }   // 2.0k

// 2.0f: factor de intensidad de un modelo desde otro plugin (GetProcAddress). factor < 0 lo quita; 0 apaga sus luces.
extern "C" __declspec(dllexport) void LNE_FactorModelo(int modelo, float factor) {
    if (modelo < 0 || modelo > 65534) return;
    if (factor < 0.0f) { g_factorExtUsa[modelo] = 0; return; }
    if (!g_factorExtUsa[modelo]) L("Factor externo para el modelo %d: x%.2f (LNE_FactorModelo)", modelo, factor);
    g_factorExt[modelo] = factor; g_factorExtUsa[modelo] = 1;
}

// 2.0h: como LNE_FactorModelo, pero solo para las luces del modelo con saturacion < 0,35 (blancas o casi blancas).
// Se multiplica con el de LNE_FactorModelo. factor < 0 lo quita; 0 apaga solo esas luces.
extern "C" __declspec(dllexport) void LNE_FactorModeloBlancas(int modelo, float factor) {
    if (modelo < 0 || modelo > 65534) return;
    if (factor < 0.0f) { g_factorBlaUsa[modelo] = 0; return; }
    if (!g_factorBlaUsa[modelo] || g_factorBla[modelo] != factor)
        L("Factor externo para las luces blancas del modelo %d: x%.2f (LNE_FactorModeloBlancas)", modelo, factor);
    g_factorBla[modelo] = factor; g_factorBlaUsa[modelo] = 1;
}
// 2.0h: solo para el arnes. Luminancia que daria una luz 2DFX (clase C_2DFX) de ese color y modelo tras ganancia, topes y
// factores (la candidata se quita despues); -1 si no entra
extern "C" __declspec(dllexport) float LNE_ArnesLuminancia(float r, float g, float b, int modelo) {
    size_t n = g_cand.size();
    float p[3] = { g_cam[0], g_cam[1], g_cam[2] };
    anade(p, 5.0f, r, g, b, C_2DFX, nullptr, (uint16_t)modelo);
    if (g_cand.size() == n) return -1.0f;
    Cand c = g_cand.back(); g_cand.resize(n);
    return 0.2126f * c.col[0] + 0.7152f * c.col[1] + 0.0722f * c.col[2];
}
// Solo para el arnes (tests\arnes_luces): copia de escena + pasada de luces sobre el dispositivo dado.
extern "C" __declspec(dllexport) int LNE_PruebaArnes(IDirect3DDevice9* d) {
    g_real = d; g_luces = 1; toma_fuego(); copia_escena(d); long antes = g_nLuces; pasada_luces(d);
    L("ARNES: albedo %d, VP %d, luces usadas %ld, pasadas %ld", g_albValido, g_vpValido, g_lucesUlt, g_nLuces - antes);
    return (int)(g_nLuces - antes);
}
// 2.0: solo para el arnes. Ajustes por nombre y luces inyectadas (tipo 2 = foco de farola).
extern "C" __declspec(dllexport) void LNE_ArnesAjusta(const char* k, float v) {
    std::string n = k;
    if (n == "focos") g_focos = (int)v; else if (n == "especular") g_espActivo = (int)v; else if (n == "intensidad_especular") g_espInt = v;
    else if (n == "atenuacion") g_atenModo = (int)v; else if (n == "limpia") g_inyectadas.clear(); else if (n == "haces") g_haces = (int)v;
    else if (n == "luces_juego") g_luzFaros = (int)v; else if (n == "faros_dobles") g_farosDobles = (int)v; else if (n == "frenos") g_frenos = (int)v;
    else if (n == "marcha_atras") g_marchaAtras = (int)v; else if (n == "sin_original") g_sinOriginal = (int)v; else if (n == "vacia_vida") g_vida.clear();
    else if (n == "sombras") g_sombFaros = (int)v; else if (n == "lluvia") g_lluvia = (int)v;   // 2.0i
    else if (n == "lluvia_modo") g_lluModo = (int)v; else if (n == "haz_clima") g_hazClima = (int)v;   // 2.0j
}
// 2.0i: lluvia iluminada en el arnes con techos fijados (ProcessVerticalLine necesita el mundo cargado)
extern "C" __declspec(dllexport) int LNE_ArnesLluvia(IDirect3DDevice9* d, float techoZ) {
    g_techoX0 = (int)floorf(g_cam[0] / kCelda) - kRej / 2; g_techoY0 = (int)floorf(g_cam[1] / kCelda) - kRej / 2;
    for (int i = 0; i < kRej * kRej; i++) { g_techo[i] = techoZ; g_techoOk[i] = 1; }
    g_real = d; pasada_lluvia(d); return (int)g_lluviaUlt;
}
extern "C" __declspec(dllexport) void LNE_ArnesFoco(float x, float y, float z, float radio, float r, float g, float b, float tipo, float dx, float dy, float dz, float coseno) {
    Cand c = {}; c.pos[0] = x; c.pos[1] = y; c.pos[2] = z; c.radio = radio; c.col[0] = r; c.col[1] = g; c.col[2] = b; c.tipo = tipo;
    c.dir[0] = dx; c.dir[1] = dy; c.dir[2] = dz; c.cosCono = coseno; c.clase = C_2DFX; c.dist = 0; g_inyectadas.push_back(c);
}
extern "C" __declspec(dllexport) void LNE_ArnesTabla(uintptr_t t) { g_tablaMI = t; }
extern "C" __declspec(dllexport) long LNE_ArnesContador(const char* k) {
    std::string n = k;
    if (n == "faros_dobles") return g_farosDoblesUlt; if (n == "frenos") return g_frenosUlt; if (n == "marcha_atras") return g_marchaUlt;
    if (n == "coches") return (long)g_veh.size(); if (n == "luces") return g_lucesUlt;
    if (n == "gotas") return g_ll2.gotasUlt; if (n == "salpicaduras") return g_ll2.salpUlt; if (n == "cortinas") return g_ll2.cortinasUlt; if (n == "luces_gotas") return g_ll2.lucesUlt;   // 2.0j
    if (n == "refuerzo_haz") return (long)(g_hazBoostUlt * 1000.0f + 0.5f);
    if (n == "disparos") return g_disparosUlt; if (n == "explosiones") return g_explosUlt; if (n == "fuegos") return g_fuegoUlt; if (n == "ganchos20d") return g_ganchos20d; return -1;
}
static void instala_todo() {
    __try { instala(); if (g_luces) engancha_reset(); } __except (EXCEPTION_EXECUTE_HANDLER) { L("EXCEPCION al instalar: nada activo"); }
}
BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID) {
    if (r != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(h);
    QueryPerformanceFrequency(&g_f); QueryPerformanceCounter(&g_t0);
    char ruta[MAX_PATH]; GetModuleFileNameA((HMODULE)&__ImageBase, ruta, MAX_PATH);
    std::string base = ruta; size_t p = base.rfind('.'); if (p != std::string::npos) base.resize(p);
    g_ini = base + ".ini"; g_log = fopen((base + ".log").c_str(), "w");
    { char e[MAX_PATH]; GetModuleFileNameA(nullptr, e, MAX_PATH); char* s = strrchr(e, '\\'); if (s) s[1] = 0; g_dirJuego = e; }
    if (!IniI("General", "Activo", 1)) { L("Activo=0: no se instala nada"); return TRUE; }
    g_coronasTras = IniI("Coronas", "DespuesDeENB", 1);
    g_luces = IniI("Luces", "Activas", 1); g_luzFaros = IniI("Luces", "IncluirFaros", 1);
    g_ganancia = IniF("Luces", "GananciaFaros", IniF("Luces", "Ganancia", 3.0f)); g_radioMult = IniF("Luces", "MultiplicadorRadioFaros", IniF("Luces", "MultiplicadorRadio", 1.0f));
    g_depuracion = IniI("Luces", "Depuracion", 0); g_radioNormal = IniF("Luces", "RadioSuavizadoNormal", 0.08f); g_capFarolas = IniF("Luces", "IntensidadMaximaFarolas", 2.5f);
    g_escar = IniI("Luces", "EscanerFarolas", 1); g_maxLuces = IniI("Luces", "MaxLuces", 256); if (g_maxLuces < 1) g_maxLuces = 1; if (g_maxLuces > MAXL) g_maxLuces = MAXL;
    g_lumMax = IniF("Luces", "LuminanciaMaxima", 1.6f);
    g_horaActiva = IniI("Horas", "Activa", 1); g_horaFaros = IniI("Horas", "AplicarAFaros", 0);
    for (int h = 0; h < 24; h++) { char k[8]; sprintf(k, "%d", h); g_horas[h] = IniF("Horas", k, g_horas[h]); }
    g_ganOtras = IniF("Luces", "GananciaOtras", 2.0f); g_radioOtras = IniF("Luces", "MultiplicadorRadioOtras", 1.0f); g_radioMaxLuz = IniF("Luces", "AlcanceMaximoLuz", 15.0f);
    g_fusion = IniF("Luces", "FusionarLucesMetros", 4.0f); g_rodilla = IniF("Luces", "RodillaLuz", 1.2f); g_luzMax = IniF("Luces", "LuzMaxima", 3.0f); g_fuente = IniF("Luces", "RadioFuente", 0.2f);
    g_ganSem = IniF("Luces", "GananciaSemaforos", 4.0f); g_radioSem = IniF("Luces", "MultiplicadorRadioSemaforos", 1.0f);
    g_ganFarolas = IniF("Luces", "GananciaFarolas", 8.0f); g_radioFarolas = IniF("Luces", "MultiplicadorRadioFarolas", 1.5f);
    g_albedoMin = IniF("Luces", "AlbedoMinimo", 0.15f); g_distMax = IniF("Luces", "DistanciaMaxima", 250.0f);
    g_expAt = IniF("Luces", "ExponenteAtenuacion", 2.0f); g_envolvente = IniF("Luces", "Envolvente", 0.5f);
    g_pesoAlb = IniF("Luces", "PesoAlbedo", 1.0f); g_cosFaro = IniF("Luces", "CosenoConoFaro", 0.75f);
    g_focos = IniI("Focos", "Activo", 1); g_anchoBorde = IniF("Focos", "AnchoBorde", 0.12f); g_atenModo = IniI("Luces", "Atenuacion", 0);
    g_espActivo = IniI("Especular", "Activo", 1); g_espInt = IniF("Especular", "Intensidad", 0.5f);
    g_espNMin = IniF("Especular", "PotenciaMin", 20.0f); g_espNMax = IniF("Especular", "PotenciaMax", 60.0f);
    g_tecFases = IniI("Teclas", "Fases20", 0);
    g_farosDobles = IniI("Faros", "Dobles", 1); g_faroAdel = IniF("Faros", "Adelanto", 0.35f); g_faroAlto = IniF("Faros", "Altura", 0.1f); g_faroIncl = IniF("Faros", "Inclinacion", 20.0f);
    g_frenos = IniI("Frenos", "Activo", 1); g_ganFrenos = IniF("Frenos", "Ganancia", 4.0f); g_radioFrenos = IniF("Frenos", "MultiplicadorRadio", 0.7f);
    g_cosFreno = cosf(IniF("Frenos", "Cono", 60.0f) * 3.14159265f / 180.0f); g_frenoIncl = IniF("Frenos", "Inclinacion", 20.0f);
    g_marchaAtras = IniI("Frenos", "MarchaAtras", 1); g_intMarcha = IniF("Frenos", "IntensidadMarchaAtras", 0.05f); g_radioMarcha = IniF("Frenos", "RadioMarchaAtras", 7.0f);
    g_disparos = IniI("Disparos", "Activo", 1); g_dispRadio = IniF("Disparos", "Radio", 8.0f); g_dispInt = IniF("Disparos", "Intensidad", 1.5f);
    g_dispDur = IniF("Disparos", "Duracion", 0.075f); g_dispCaida = IniF("Disparos", "Caida", 6.0f);
    g_dispCol[0] = IniF("Disparos", "Rojo", 1.0f); g_dispCol[1] = IniF("Disparos", "Verde", 0.6f); g_dispCol[2] = IniF("Disparos", "Azul", 0.25f);
    g_explos = IniI("Explosiones", "Activo", 1); g_expRadio = IniF("Explosiones", "MultiplicadorRadio", 1.5f); g_expInt = IniF("Explosiones", "Intensidad", 3.0f);
    g_expDur = IniF("Explosiones", "Duracion", 0.6f); g_expCaida = IniF("Explosiones", "Caida", 5.0f); g_expAlto = IniF("Explosiones", "AlturaExtra", 1.5f);
    g_expCol[0] = IniF("Explosiones", "Rojo", 1.0f); g_expCol[1] = IniF("Explosiones", "Verde", 0.5f); g_expCol[2] = IniF("Explosiones", "Azul", 0.15f);
    g_fuegoAct = IniI("Fuego", "Activo", 1); g_fuegoGan = IniF("Fuego", "Ganancia", 2.0f); g_fuegoAlto = IniF("Fuego", "AlturaExtra", 1.5f); g_fuegoRadio = IniF("Fuego", "MultiplicadorRadio", 1.0f);
    g_haces = IniI("Haces", "Activo", 1); g_hazMax = IniI("Haces", "MaxHaces", 16); if (g_hazMax < 0) g_hazMax = 0; if (g_hazMax > MAXL) g_hazMax = MAXL;
    g_hazFaros = IniI("Haces", "Faros", 0); g_hazSem = IniI("Haces", "Semaforos", 0); g_quitaNiebla = IniI("Haces", "QuitarNieblaVanilla", 0);
    g_hazLargo = IniF("Haces", "Largo", 9.0f); g_hazRadio = IniF("Haces", "Radio", 4.0f); g_hazAngInt = IniF("Haces", "AnguloInterior", 10.0f); g_hazAngExt = IniF("Haces", "AnguloExterior", 60.0f);
    g_hazInt = IniF("Haces", "Intensidad", 0.3f); g_hazDens = IniF("Haces", "Densidad", 0.5f); g_hazNiebla = IniF("Haces", "RefuerzoNiebla", 2.0f); g_hazLluvia = IniF("Haces", "RefuerzoLluvia", 1.0f);
    g_hazRMin = IniF("Haces", "RadioMin", 6.0f); g_hazRMax = IniF("Haces", "RadioMax", 35.0f);
    if (g_hazLargo < 0.5f) g_hazLargo = 0.5f; if (g_hazRadio < 0.2f) g_hazRadio = 0.2f; if (g_hazAngExt > 85.0f) g_hazAngExt = 85.0f; if (g_hazAngInt >= g_hazAngExt) g_hazAngInt = g_hazAngExt - 1.0f;
    g_hazClima = IniI("Haces", "ClimaSkyGFX", 0);   // 2.0j: 0 = haces de la 2.0h (decision del usuario)
    g_hazB[0] = IniF("Haces", "BrilloDespejado", 0.28f); g_hazB[1] = IniF("Haces", "BrilloNublado", 0.60f); g_hazB[2] = IniF("Haces", "BrilloLluvia", 1.0f); g_hazB[3] = IniF("Haces", "BrilloNiebla", 2.5f);
    g_partTras = IniI("Particulas", "DespuesDeENB", 1);
    g_sombFaros = IniI("SombrasFaros", "Activo", 1); g_sombPasos = IniI("SombrasFaros", "Pasos", 12); if (g_sombPasos < 2) g_sombPasos = 2; if (g_sombPasos > 24) g_sombPasos = 24;
    g_sombGrosor = IniF("SombrasFaros", "Grosor", 1.5f); g_sombSesgo = IniF("SombrasFaros", "Sesgo", 0.12f); g_sombFuerza = IniF("SombrasFaros", "Fuerza", 0.9f);
    g_lluvia = IniI("Lluvia", "Activo", 1); g_lluviaJuego = IniI("Lluvia", "LluviaDelJuego", 0); g_lluN = IniI("Lluvia", "Estelas", 800); if (g_lluN < 0) g_lluN = 0; if (g_lluN > 4000) g_lluN = 4000;
    g_lluDist = IniF("Lluvia", "Distancia", 25.0f); if (g_lluDist < 5.0f) g_lluDist = 5.0f; if (g_lluDist > kCelda * (kRej / 2)) g_lluDist = kCelda * (kRej / 2);
    g_lluVel = IniF("Lluvia", "Velocidad", 30.0f); if (g_lluVel < 1.0f) g_lluVel = 1.0f; g_lluAng = IniF("Lluvia", "Angulo", 10.0f); g_lluViento = IniF("Lluvia", "Viento", 0.30f);
    g_lluLargo = IniF("Lluvia", "Largo", 0.6f); g_lluGrosor = IniF("Lluvia", "Grosor", 1.5f); g_lluOpac = IniF("Lluvia", "Opacidad", 0.8f);
    g_lluInt = IniF("Lluvia", "Intensidad", 1.0f); g_lluAmb = IniF("Lluvia", "Ambiente", 0.35f);
    g_lluTinte[0] = IniF("Lluvia", "TinteRojo", 0.90f); g_lluTinte[1] = IniF("Lluvia", "TinteVerde", 0.95f); g_lluTinte[2] = IniF("Lluvia", "TinteAzul", 1.00f);
    {   // 2.0j: lluvia nueva
        g_lluModo = IniI("Lluvia", "Modo", 2); if (g_lluModo < 0 || g_lluModo > 2) g_lluModo = 2;
        lluvia2::Config& c = g_ll2Cfg;
        c.gotas = IniI("Lluvia", "Gotas", 3000); if (c.gotas < 0) c.gotas = 0; if (c.gotas > 12000) c.gotas = 12000;
        c.salpicaduras = IniI("Lluvia", "Salpicaduras", 400); if (c.salpicaduras < 0) c.salpicaduras = 0; if (c.salpicaduras > 4000) c.salpicaduras = 4000;
        c.cortinas = IniI("Lluvia", "Cortinas", 1); c.luces = IniI("Lluvia", "LucesGotas", 24); if (c.luces < 0) c.luces = 0; if (c.luces > 24) c.luces = 24;
        c.distancia = IniF("Lluvia", "DistanciaGotas", 12.0f); if (c.distancia < 4.0f) c.distancia = 4.0f; if (c.distancia > 26.0f) c.distancia = 26.0f;
        c.velocidad = g_lluVel; c.angulo = g_lluAng; c.viento = g_lluViento;
        c.ancho = IniF("Lluvia", "AnchoGota", 0.014f); c.intensidad = IniF("Lluvia", "IntensidadGotas", 2.2f); c.opacidad = IniF("Lluvia", "OpacidadGotas", 0.80f);
        c.refraccion = IniF("Lluvia", "Refraccion", 0.012f); c.pesoLuz = IniF("Lluvia", "LuzGotas", 3.5f);
        c.cortinaInt = IniF("Lluvia", "IntensidadCortinas", 0.55f);
        for (int k = 0; k < 3; k++) c.tinte[k] = g_lluTinte[k];
        g_lluAmbMin[0] = IniF("Lluvia", "AmbienteMinimoRojo", 0.045f); g_lluAmbMin[1] = IniF("Lluvia", "AmbienteMinimoVerde", 0.050f); g_lluAmbMin[2] = IniF("Lluvia", "AmbienteMinimoAzul", 0.060f);
    }
    // 2.0k-r1 (05-11-2026, revision de la publicacion): sin ruta personal por defecto. La carpeta por defecto es
    // capturas-ENB y una ruta relativa cuelga de la carpeta del juego (antes, del directorio actual del proceso).
    // Carpeta vacia = no se mueve nada.
    g_capturas = IniI("Capturas", "Mover", 1); g_dirCapturas = IniS("Capturas", "Carpeta", "capturas-ENB");
    if (g_dirCapturas.empty()) g_capturas = 0;
    else {
        const bool absoluta = (g_dirCapturas.size() > 1 && g_dirCapturas[1] == ':') || g_dirCapturas[0] == '\\' || g_dirCapturas[0] == '/';
        if (!absoluta) g_dirCapturas = g_dirJuego + g_dirCapturas;
        if (g_dirCapturas.back() != '\\' && g_dirCapturas.back() != '/') g_dirCapturas += '\\';
    }
    g_teclas = IniI("Teclas", "Activas", 0); g_tecLuces = IniI("Teclas", "Luces", 0); g_tecCoronas = IniI("Teclas", "Coronas", 0); g_tecDep = IniI("Teclas", "Depuracion", 0);
    L("2.0k-r1: carpeta de capturas relativa a la carpeta del juego, sin ruta personal por defecto");
    L("2.0k (estelas solo con la caida y el viento que cruza la vista; salpicaduras solo sobre una superficie visible)");
    L("2.0j | lluvia modo %d (%d gotas a %.0f m, %d salpicaduras, cortinas %d, %d luces, ancho %.3f m, intensidad %.2f, opacidad %.2f, refraccion %.3f, luz %.2f, ambiente minimo %.3f %.3f %.3f) | haces por clima %d",
      g_lluModo, g_ll2Cfg.gotas, g_ll2Cfg.distancia, g_ll2Cfg.salpicaduras, g_ll2Cfg.cortinas, g_ll2Cfg.luces, g_ll2Cfg.ancho, g_ll2Cfg.intensidad, g_ll2Cfg.opacidad, g_ll2Cfg.refraccion, g_ll2Cfg.pesoLuz, g_lluAmbMin[0], g_lluAmbMin[1], g_lluAmbMin[2], g_hazClima);
    L("2.0i (aportes SkyGFX) | particulas tras ENB %d | lluvia iluminada %d (%d estelas, %.0f m, %.0f m/s, %.0f grados, viento %.2f, largo %.2f m, grosor %.1f px, opacidad %.2f, x%.2f, ambiente %.2f, lluvia del juego %d) | sombras de faros %d (%d pasos, grosor %.2f m, sesgo %.2f m, fuerza %.2f) | haces por clima %d (%.2f/%.2f/%.2f/%.2f)",
      g_partTras, g_lluvia, g_lluN, g_lluDist, g_lluVel, g_lluAng, g_lluViento, g_lluLargo, g_lluGrosor, g_lluOpac, g_lluInt, g_lluAmb, g_lluviaJuego, g_sombFaros, g_sombPasos, g_sombGrosor, g_sombSesgo, g_sombFuerza, g_hazClima, g_hazB[0], g_hazB[1], g_hazB[2], g_hazB[3]);
    L("LucesNocturnasENB 2.0i sobre 2.0h (reconstruccion relativa a la camara: sin ruido lejos del origen) | disparos %d (%.1f m, x%.2f, %.3f s, caida %.1f) | explosiones %d (radio x%.2f, x%.2f, %.2f s, caida %.1f, +%.1f m) | fuego %d (x%.2f, +%.1f m)",
      g_disparos, g_dispRadio, g_dispInt, g_dispDur, g_dispCaida, g_explos, g_expRadio, g_expInt, g_expDur, g_expCaida, g_expAlto, g_fuegoAct, g_fuegoGan, g_fuegoAlto);
    L("2.0c | faros dobles %d (adelanto %.2f, altura %.2f, inclinacion %.0f) | frenos %d (x%.1f, radio x%.2f, inclinacion %.0f) | marcha atras %d (%.2f, %.0f m)",
      g_farosDobles, g_faroAdel, g_faroAlto, g_faroIncl, g_frenos, g_ganFrenos, g_radioFrenos, g_frenoIncl, g_marchaAtras, g_intMarcha, g_radioMarcha);
    L("2.0b | haces %d (max %d, largo %.1f m, radio %.1f m, angulos %.0f-%.0f, x%.2f densidad %.2f, niebla x%.1f lluvia x%.1f, radios %.0f-%.0f m, faros %d semaforos %d, quitar niebla vanilla %d)",
      g_haces, g_hazMax, g_hazLargo, g_hazRadio, g_hazAngInt, g_hazAngExt, g_hazInt, g_hazDens, g_hazNiebla, g_hazLluvia, g_hazRMin, g_hazRMax, g_hazFaros, g_hazSem, g_quitaNiebla);
    L("2.0a | focos %d (borde %.2f) | especular %d (x%.2f, potencia %.0f-%.0f) | atenuacion %s | tecla 2.0 %d", g_focos, g_anchoBorde, g_espActivo, g_espInt, g_espNMin, g_espNMax, g_atenModo ? "smoothstep" : "pow", g_tecFases);
    L("Base 1.8 (horas %d: 22h %.2f 0h %.2f 2h %.2f 4h %.2f, faros %d | luminancia max %.2f, alcance max %.0f m, fusion %.1f m, rodilla %.2f-%.2f, fuente %.2f, otras x%.2f | semaforos x%.2f radio x%.2f, escaner %d, max %d luces, normal suavizada %.2f m, tope farolas %.1f, depuracion %d, dist %.0f) | coronas tras ENB=%d luces=%d (farolas x%.2f radio x%.2f | faros %d x%.2f radio x%.2f | albedo min %.2f envolvente %.2f) capturas=%d -> %s | teclas %d (%d/%d)",
      g_horaActiva, g_horas[22], g_horas[0], g_horas[2], g_horas[4], g_horaFaros, g_lumMax, g_radioMaxLuz, g_fusion, g_rodilla, g_luzMax, g_fuente, g_ganOtras, g_ganSem, g_radioSem, g_escar, g_maxLuces, g_radioNormal, g_capFarolas, g_depuracion, g_distMax, g_coronasTras, g_luces, g_ganFarolas, g_radioFarolas, g_luzFaros, g_ganancia, g_radioMult, g_albedoMin, g_envolvente, g_capturas, g_dirCapturas.c_str(), g_teclas, g_tecLuces, g_tecCoronas);
    instala_todo();
    return TRUE;
}
