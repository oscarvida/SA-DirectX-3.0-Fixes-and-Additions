// Isolated SA 1.0 US adaptation: local light scattering on existing smoke.
// Does not hook D3D, replace a pipeline, create an emitter or touch other mods.
// Independent CPU/vertex approximation of SkyGFX's particle-light capability;
// not a claim of equivalence to its unresolved per-particle GPU implementation.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstddef>
#include <cstdarg>
#include "smoke_core.h"
#include "pasto_core.h"

namespace {
constexpr uintptr_t EMITTER_SLOT=0x85a79c, TRANSFORM_CALL=0x4a1620;
constexpr uintptr_t LIGHT_COUNT=0xc3f0d0, LIGHT_ARRAY=0xc3f0e0;
using Emitter=void(__thiscall*)(void*,void*,uint32_t,float,bool);
using Transform=void*(__cdecl*)(sad::Vertex*,uint32_t,void*,uint32_t);
Emitter originalEmitter=nullptr; Transform originalTransform=nullptr;
bool installed=false, selected=false, inTransform=false;
sad::Settings settings;
uint32_t saved[8192];
FILE* logFile=nullptr;
uint64_t batches=0, vertices=0, changed=0, skipped=0;
static void log(const char* fmt,...) {
    if(!logFile) return;
    va_list a;va_start(a,fmt);vfprintf(logFile,fmt,a);va_end(a);fputc('\n',logFile);fflush(logFile);
}
static bool readable(const void* ptr,size_t n) {
    uintptr_t p=reinterpret_cast<uintptr_t>(ptr),end=p+n;
    if(!p || end<p) return false;
    while(p<end) {
        MEMORY_BASIC_INFORMATION m={};
        if(!VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof m) || m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return false;
        if(!(m.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))) return false;
        uintptr_t next=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;
        if(next<=p)return false;p=next;
    }
    return true;
}
static bool writable(const void* ptr,size_t n) {
    uintptr_t p=reinterpret_cast<uintptr_t>(ptr),end=p+n;
    if(!p||end<p)return false;
    while(p<end) {
        MEMORY_BASIC_INFORMATION m={};
        if(!VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof m)||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
        if(!(m.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
        uintptr_t next=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;
        if(next<=p)return false;p=next;
    }
    return true;
}
static bool write(uintptr_t address,const void* data,size_t n) {
    DWORD old=0,unused=0;
    if(!VirtualProtect(reinterpret_cast<void*>(address),n,PAGE_EXECUTE_READWRITE,&old)) return false;
    memcpy(reinterpret_cast<void*>(address),data,n);
    if(!VirtualProtect(reinterpret_cast<void*>(address),n,old,&unused))log("AVISO: bytes escritos, no se pudo restituir la proteccion en %p",reinterpret_cast<void*>(address));
    FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(address),n);
    return true;
}
static bool isSmoke(void* emitter,bool heat) {
    if(heat||!readable(emitter,0x40)) return false;
    const auto e=static_cast<const uint8_t*>(emitter);
    // Only standard alpha smoke. Skip additive flame, heat distortion and opaque FX.
    if(!e[7] || e[5]!=4 || e[6]!=5) return false;
    const uint8_t* texture=*reinterpret_cast<uint8_t*const*>(e+0xc);
    return readable(texture,0x30) && sad::smoke(reinterpret_cast<const char*>(texture+0x10));
}
// 0.2: the light list of LucesNocturnasENB 2.0i (gains, 256 lights, scanned street lamps; the role of SkyGFX's
// deferred list). Without it, the vanilla CPointLights table (32 lights near the camera, weak lamp colours).
using LneLights=int(__cdecl*)(sad::LneLight*,int,uint32_t*);
static LneLights lneLights=nullptr; static bool lneChecked=false; static uint64_t lneBatches=0, nativeBatches=0;
static void findLne() {
    if(lneChecked) return;
    lneChecked=true;
    HMODULE m=GetModuleHandleA("LucesNocturnasENB.SA.asi");
    if(!m){log("LucesNocturnasENB no cargado: se usa la tabla CPointLights del juego");return;}
    auto version=reinterpret_cast<int(__cdecl*)()>(GetProcAddress(m,"LNE_Version"));
    auto lights=reinterpret_cast<LneLights>(GetProcAddress(m,"LNE_LucesCuadro"));
    if(version&&lights&&version()>=0x0209){lneLights=lights;log("Luces de LucesNocturnasENB %X (LNE_LucesCuadro)",version());}
    else log("LucesNocturnasENB sin LNE_LucesCuadro (anterior a 2.0i): se usa la tabla CPointLights del juego");
}
static int gather(sad::Source* out,int max) {
    findLne();
    int n=0;
    if(lneLights) {
        static sad::LneLight l[256];
        int c=std::clamp(lneLights(l,256,nullptr),0,256);
        for(int i=0;i<c&&n<max;++i) if(sad::fromLne(l[i],settings,out[n]))++n;
        ++lneBatches; return n;
    }
    if(!readable(reinterpret_cast<void*>(LIGHT_COUNT),4)) return 0;
    uint32_t count=*reinterpret_cast<uint32_t*>(LIGHT_COUNT);
    if(count==0||count>32 || !readable(reinterpret_cast<void*>(LIGHT_ARRAY),count*sizeof(sad::Light)))return 0;
    sad::Light lights[32];memcpy(lights,reinterpret_cast<void*>(LIGHT_ARRAY),count*sizeof(sad::Light));
    for(uint32_t i=0;i<count&&n<max;++i) if(sad::fromNative(lights[i],settings,out[n]))++n;
    ++nativeBatches; return n;
}
static int prepare(sad::Vertex* v,uint32_t n,void* matrix,bool& savedReady) {
    if(n==0||n>8192 || !writable(v,size_t(n)*sizeof *v) || (matrix&&!readable(matrix,64))) return 0;
    static sad::Source all[256];
    int count=gather(all,256);
    if(count<=0) return 0;
    const float* m=static_cast<const float*>(matrix);
    static float world[8192][3];
    float lo[3]={1e30f,1e30f,1e30f},hi[3]={-1e30f,-1e30f,-1e30f};
    for(uint32_t i=0;i<n;++i) {
        float* p=world[i];memcpy(p,v[i].pos,12);
        if(m) for(int j=0;j<3;++j) p[j]=v[i].pos[0]*m[j]+v[i].pos[1]*m[4+j]+v[i].pos[2]*m[8+j]+m[12+j];
        for(int j=0;j<3;++j){lo[j]=std::min(lo[j],p[j]);hi[j]=std::max(hi[j],p[j]);}
    }
    sad::Source nearby[8];
    int used=sad::select(all,count,lo,hi,settings,nearby);
    if(used<=0) return 0;
    for(uint32_t i=0;i<n;++i) saved[i]=v[i].rgba;
    savedReady=true;
    for(uint32_t i=0;i<n;++i) {
        v[i].rgba=sad::illuminate(world[i],v[i].rgba,nearby,used,settings);
        if(v[i].rgba!=saved[i])++changed;
    }
    ++batches;vertices+=n;return 1;
}
static void* __cdecl onTransform(sad::Vertex* v,uint32_t n,void* matrix,uint32_t flags) {
    if(!selected||inTransform) return originalTransform(v,n,matrix,flags);
    bool modified=false;void* result=nullptr;
    inTransform=true;
    __try {
        __try { prepare(v,n,matrix,modified); }
        __except(EXCEPTION_EXECUTE_HANDLER) { ++skipped; }
        __try { result=originalTransform(v,n,matrix,flags); }
        __finally { if(modified) for(uint32_t i=0;i<n;++i) v[i].rgba=saved[i]; }
    } __finally { inTransform=false; }
    return result;
}
static void __fastcall onEmitter(void* self,void*,void* camera,uint32_t key,float brightness,bool heat) {
    bool previous=selected;
    __try { selected=isSmoke(self,heat); }
    __except(EXCEPTION_EXECUTE_HANDLER) { selected=false;++skipped; }
    __try { originalEmitter(self,camera,key,brightness,heat); }
    __finally { selected=previous; }
}
static bool install() {
    if(installed)return true;
    // Exact profile, also rejects relocated point-light arrays. No guessing of
    // FLA table sizes or vtables: unsupported changes disable this module.
    const uint32_t vt[]={0x4a2b20,0x5c25f0,0x5c0a30,0x4a2b40,0x4a2bc0,0x4a2c40,0x4a2510};
    const uint8_t call[]={0xe8,0x2b,0xde,0x34,0x00};
    const uint8_t count[]={0x8b,0x15,0xd0,0xf0,0xc3,0x00,0x83,0xfa,0x20};
    const uint8_t array[]={0x8d,0x81,0xe0,0xf0,0xc3,0x00};
    if(!readable(reinterpret_cast<void*>(EMITTER_SLOT-20),sizeof vt) ||
       memcmp(reinterpret_cast<void*>(EMITTER_SLOT-20),vt,sizeof vt) ||
       !readable(reinterpret_cast<void*>(TRANSFORM_CALL),5) || memcmp(reinterpret_cast<void*>(TRANSFORM_CALL),call,5) ||
       !readable(reinterpret_cast<void*>(0x700176),sizeof count) || memcmp(reinterpret_cast<void*>(0x700176),count,sizeof count) ||
       !readable(reinterpret_cast<void*>(0x7001fd),sizeof array) || memcmp(reinterpret_cast<void*>(0x7001fd),array,sizeof array)) {
        log("NO ACTIVADO: perfil de vtable/FX/luces distinto. No se modifica nada.");return false;
    }
    originalEmitter=reinterpret_cast<Emitter>(0x4a2c40);
    originalTransform=reinterpret_cast<Transform>(0x7ef450);
    uint8_t patch[5]={0xe8};
    int32_t delta=int32_t(reinterpret_cast<uintptr_t>(&onTransform)-(TRANSFORM_CALL+5));memcpy(patch+1,&delta,4);
    auto hook=&onEmitter;
    if(!write(TRANSFORM_CALL,patch,5))return false;
    if(!write(EMITTER_SLOT,&hook,4)) { write(TRANSFORM_CALL,call,5);return false; }
    installed=true;log("ACTIVADO: FxEmitterBP::Render y RenderEnd -> Im3DTransform. Solo texturas smoke, alpha estandar. Sin hooks ENB/D3D.");return true;
}
// 0.4: hierba. CPlantMgr::Render lee la luz de la cara en 0x5DBCDB (mov al,[edi+47h]; mov edx,[esi-0Ch]); se
// sustituye por un salto a un trozo propio que la pasa por la tabla de pasto_core.h y vuelve a 0x5DBCE1.
constexpr uintptr_t PLANT_LIGHT=0x5dbcdb, PLANT_BACK=0x5dbce1;
const uint8_t plantOriginal[6]={0x8a,0x47,0x47,0x8b,0x56,0xf4};
uint8_t* plantCave=nullptr; bool plantInstalled=false;
static bool installGrass(int dayMin,int nightMin) {
    if(!readable(reinterpret_cast<void*>(PLANT_LIGHT),6)||memcmp(reinterpret_cast<void*>(PLANT_LIGHT),plantOriginal,6)) {
        const uint8_t* b=reinterpret_cast<const uint8_t*>(PLANT_LIGHT);
        if(readable(b,6))log("HIERBA NO ACTIVADA: 0x5DBCDB = %02X %02X %02X %02X %02X %02X (otro mod lo cambio). No se modifica nada.",b[0],b[1],b[2],b[3],b[4],b[5]);
        else log("HIERBA NO ACTIVADA: 0x5DBCDB no se puede leer");
        return false;
    }
    plantCave=static_cast<uint8_t*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    if(!plantCave){log("HIERBA NO ACTIVADA: sin memoria para el trozo de codigo");return false;}
    uint8_t* table=plantCave+256;
    pasto::table(table,dayMin,nightMin);
    uint8_t* c=plantCave;
    const uint8_t movzx[]={0x0f,0xb6,0x47,0x47};memcpy(c,movzx,4);c+=4;                     // movzx eax,byte [edi+47h]
    *c++=0x8a;*c++=0x80;uint32_t ta=uint32_t(reinterpret_cast<uintptr_t>(table));memcpy(c,&ta,4);c+=4;   // mov al,[eax+tabla]
    *c++=0x8b;*c++=0x56;*c++=0xf4;                                                            // mov edx,[esi-0Ch]
    *c++=0xe9;int32_t back=int32_t(PLANT_BACK-(reinterpret_cast<uintptr_t>(c)+4));memcpy(c,&back,4);c+=4;
    FlushInstructionCache(GetCurrentProcess(),plantCave,64);
    uint8_t patch[6]={0xe9,0,0,0,0,0x90};int32_t to=int32_t(reinterpret_cast<uintptr_t>(plantCave)-(PLANT_LIGHT+5));memcpy(patch+1,&to,4);
    if(!write(PLANT_LIGHT,patch,6)){VirtualFree(plantCave,0,MEM_RELEASE);plantCave=nullptr;log("HIERBA NO ACTIVADA: no se pudo escribir 0x5DBCDB");return false;}
    plantInstalled=true;
    log("HIERBA ACTIVADA: luz de la cara de colision con minimo %d/15 de dia y %d/15 de noche (CPlantMgr::Render 0x5DBCDB). Liberty City 4/1 -> %d/%d",
        dayMin,nightMin,table[0x14]&15,table[0x14]>>4);
    return true;
}
static void uninstallGrass() {
    if(!plantInstalled)return;
    const uint8_t* b=reinterpret_cast<const uint8_t*>(PLANT_LIGHT);
    if(b[0]==0xe9 && PLANT_LIGHT+5+*reinterpret_cast<const int32_t*>(b+1)==reinterpret_cast<uintptr_t>(plantCave))write(PLANT_LIGHT,plantOriginal,6);
    plantInstalled=false;   // el trozo no se libera: otro hilo podria estar dentro al cerrar
}
static void uninstall() {
    uninstallGrass();
    if(!installed)return;
    if(*reinterpret_cast<void**>(EMITTER_SLOT)==reinterpret_cast<void*>(&onEmitter))write(EMITTER_SLOT,&originalEmitter,4);
    uintptr_t target=TRANSFORM_CALL+5+*reinterpret_cast<int32_t*>(TRANSFORM_CALL+1);
    if(*reinterpret_cast<uint8_t*>(TRANSFORM_CALL)==0xe8 && target==reinterpret_cast<uintptr_t>(&onTransform)) {
        uint8_t patch[5]={0xe8};int32_t delta=int32_t(0x7ef450-(TRANSFORM_CALL+5));memcpy(patch+1,&delta,4);write(TRANSFORM_CALL,patch,5);
    }
    installed=false;
}
}

extern "C" __declspec(dllexport) int AportesPasto_Activo() { return plantInstalled?1:0; }
extern "C" __declspec(dllexport) void AportesHumo_Contadores(uint64_t* out) {
    if(!out)return;out[0]=batches;out[1]=vertices;out[2]=changed;out[3]=skipped;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID reserved) {
    if(reason==DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        char path[MAX_PATH]={};GetModuleFileNameA(module,path,MAX_PATH);
        char* ext=strrchr(path,'.');if(!ext)return TRUE;
        strcpy_s(ext,size_t(path+MAX_PATH-ext),".ini");
        bool active=GetPrivateProfileIntA("Humo","Activo",1,path)!=0;
        settings.maxLights=std::clamp(int(GetPrivateProfileIntA("Humo","MaxLuces",8,path)),1,8);
        auto readF=[&](const char* key,const char* def,float lo,float hi,float fallback){char value[64];GetPrivateProfileStringA("Humo",key,def,value,64,path);
            float f=float(atof(value));if(!sad::finite(f))f=fallback;return std::clamp(f,lo,hi);};
        settings.strength=readF("Intensidad","1.0",0.0f,4.0f,1.0f);
        settings.source=readF("Fuente","0.25",0.01f,1.0f,0.25f);
        settings.nativeGain=readF("GananciaNativa","3.0",0.0f,16.0f,3.0f);
        strcpy_s(ext,size_t(path+MAX_PATH-ext),".log");fopen_s(&logFile,path,"a");
        log("HumoLucesENB 0.4 (hierba: [Pasto]) | 0.2 | luz de particulas de SkyGFX (FUN_100b88f0) | intensidad %.2f, fuente %.2f, %d luces por lote, ganancia nativa %.2f",settings.strength,settings.source,settings.maxLights,settings.nativeGain);
        if(active)install();else log("Activo=0");
        if(GetPrivateProfileIntA("Pasto","Activo",1,path)!=0) {
            strcpy_s(ext,size_t(path+MAX_PATH-ext),".ini");
            installGrass(std::clamp(int(GetPrivateProfileIntA("Pasto","LuzMinimaDia",12,path)),0,15),
                         std::clamp(int(GetPrivateProfileIntA("Pasto","LuzMinimaNoche",6,path)),0,15));
        } else log("Pasto Activo=0");
    } else if(reason==DLL_PROCESS_DETACH) {
        if(!reserved)uninstall();
        log("FIN: lotes=%llu vertices=%llu colores=%llu omitidos=%llu | lotes con luces LNE=%llu, con tabla del juego=%llu",batches,vertices,changed,skipped,lneBatches,nativeBatches);
        if(logFile){fclose(logFile);logFile=nullptr;}
    }
    return TRUE;
}
