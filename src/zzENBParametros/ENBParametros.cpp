// ENB parameter name lookup elimination. Per-effect generated handles only.
// Same COM object, original methods, values, draw order and shaders.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3dx9.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <cstddef>

extern "C" IMAGE_DOS_HEADER __ImageBase;
static uintptr_t nameBegin, nameEnd;
static HMODULE d3dxModule;
extern "C" __declspec(dllexport) volatile LONG EP_Enabled=1;
static FILE* logFile;
constexpr unsigned VT_COUNT=79, CACHE_SIZE=512, NAME_SIZE=96;
// Read-only external telemetry. Lifecycle writes use an odd/even generation;
// the owner thread's existing hot counters need no lock or file I/O.
constexpr unsigned STATS_SIZE=128;
struct StatsSlot { volatile LONG sequence; void* effect; void* state; DWORD thread;
    unsigned retiredHits,retiredMisses,retiredBypasses,retiredDisabled,retiredOutside; };
extern "C" {
__declspec(dllexport) StatsSlot EP_Stats[STATS_SIZE]{};
__declspec(dllexport) unsigned EP_StatsCapacity=STATS_SIZE;
__declspec(dllexport) volatile LONG EP_StatsOverflow=0;
__declspec(dllexport) unsigned EP_StatsVersion=1;
__declspec(dllexport) unsigned EP_ResolverVersion=2;
__declspec(dllexport) unsigned EP_NegativeRetired[STATS_SIZE]{};
}
static SRWLOCK statsLock=SRWLOCK_INIT;
struct Entry { D3DXHANDLE name,handle; bool technique; unsigned char length; char text[NAME_SIZE]; };
struct EffectState {
    void* table[VT_COUNT]; // first: container address equals cloned vtable address
    void** original;
    DWORD thread;
    volatile unsigned hits,misses,bypasses,disabled,outside;
    int statsIndex;
    volatile unsigned negativeHits;
    Entry entries[CACHE_SIZE];
};
static_assert(offsetof(EffectState,negativeHits)==348,"x86 telemetry ABI");
static_assert(sizeof(Entry)==108,"bounded cache footprint unchanged");
static EffectState* state(ID3DXEffect* effect) {return reinterpret_cast<EffectState*>(*reinterpret_cast<void***>(effect));}
template<class Fn> static Fn original(EffectState* s,unsigned slot) {return reinterpret_cast<Fn>(s->original[slot]);}
static D3DXHANDLE resolve(ID3DXEffect* effect,EffectState* s,D3DXHANDLE name,bool technique=false) {
    if(GetCurrentThreadId()!=s->thread)return name; // counters cover the owner only
    if(!EP_Enabled){++s->disabled;return name;}
    if(uintptr_t(name)<nameBegin || uintptr_t(name)>=nameEnd){++s->outside;return name;}
    // Only names inside ENB's image are candidates. Generated D3DX handles and
    // other plugins' strings pass through. Content comparison handles mutable
    // in-image strings at the same address without stale-name assumptions.
    const unsigned index=((unsigned(uintptr_t(name))>>2)*2654435761u+(technique?17:0))&(CACHE_SIZE-1);
    Entry* free=nullptr;
    for(unsigned i=0;i<8;++i) {
        Entry& e=s->entries[(index+i)&(CACHE_SIZE-1)];
        if(e.name==name && e.technique==technique) {
            // Revalidate the cached text including its terminator, within the
            // current image boundary. A cache hit needs no separate strlen.
            if(nameEnd-uintptr_t(name)>e.length &&
               std::memcmp(e.text,name,size_t(e.length)+1)==0){
                if(e.handle){++s->hits;return e.handle;}
                // Metadata of an existing D3DX effect is immutable. A cached
                // miss skips only our redundant GetParameterByName, while the
                // original setter still receives its original name and returns
                // its original error. Mutable strings are compared above.
                ++s->negativeHits;++s->bypasses;return name;
            }
            // Name changed in place; refresh this entry.
            free=&e;break;
        }
        if(!e.name){free=&e;break;}
    }
    if(!free){++s->bypasses;return name;}
    size_t len=0;
    while(len<NAME_SIZE && uintptr_t(name)+len<nameEnd && name[len])++len;
    if(!len || len==NAME_SIZE || uintptr_t(name)+len>=nameEnd){++s->bypasses;return name;}
    ++s->misses;
    D3DXHANDLE handle;
    if(technique) {
        using Fn=D3DXHANDLE(WINAPI*)(ID3DXEffect*,LPCSTR);
        handle=original<Fn>(s,13)(effect,name);
    } else {
        using Fn=D3DXHANDLE(WINAPI*)(ID3DXEffect*,D3DXHANDLE,LPCSTR);
        handle=original<Fn>(s,9)(effect,nullptr,name);
    }
    free->name=name;free->handle=handle;free->technique=technique;free->length=static_cast<unsigned char>(len);
    std::memcpy(free->text,name,len+1);
    if(!handle){++s->bypasses;return name;}
    return handle;
}

static ULONG WINAPI releaseEffect(ID3DXEffect* effect) {
    auto* s=state(effect);
    using Fn=ULONG(WINAPI*)(ID3DXEffect*);
    ULONG refs=original<Fn>(s,2)(effect);
    if(!refs) {
        if(s->statsIndex>=0){
            AcquireSRWLockExclusive(&statsLock);auto& row=EP_Stats[s->statsIndex];
            InterlockedIncrement(&row.sequence);
            row.retiredHits+=s->hits;row.retiredMisses+=s->misses;row.retiredBypasses+=s->bypasses;
            row.retiredDisabled+=s->disabled;row.retiredOutside+=s->outside;
            EP_NegativeRetired[s->statsIndex]+=s->negativeHits;
            row.effect=nullptr;row.state=nullptr;
            InterlockedIncrement(&row.sequence);ReleaseSRWLockExclusive(&statsLock);
        }
        if(logFile) std::fprintf(logFile,"LIBERADO hits=%u misses=%u bypass=%u\n",s->hits,s->misses,s->bypasses);
        delete s; // original has destroyed the effect; no stale address cache
    }
    return refs;
}

#include "parametros_wrappers.inc"

static bool ownerIs(void* p,HMODULE expected) {
    HMODULE m=nullptr;
    return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(p),&m) && m==expected;
}
extern "C" __declspec(dllexport) bool WINAPI EP_Attach(ID3DXEffect* effect) {
    if(!effect)return false;
    auto** v=*reinterpret_cast<void***>(effect);
    if(v[2]==reinterpret_cast<void*>(releaseEffect))return true;
    // Validate the entire original interface before installing a private table.
    for(unsigned i=0;i<VT_COUNT;++i)if(!ownerIs(v[i],d3dxModule))return false;
    auto* s=new(std::nothrow) EffectState{};
    if(!s)return false;
    std::memcpy(s->table,v,sizeof(s->table));s->original=v;s->thread=GetCurrentThreadId();s->statsIndex=-1;
    s->table[2]=reinterpret_cast<void*>(releaseEffect);
    installWrappers(s);
    *reinterpret_cast<void***>(effect)=s->table;
    AcquireSRWLockExclusive(&statsLock);
    for(unsigned j=0;j<STATS_SIZE;++j)if(!EP_Stats[j].state){
        auto& row=EP_Stats[j];InterlockedIncrement(&row.sequence);
        row.effect=effect;row.state=s;row.thread=s->thread;s->statsIndex=int(j);
        InterlockedIncrement(&row.sequence);break;
    }
    if(s->statsIndex<0)InterlockedIncrement(&EP_StatsOverflow);
    ReleaseSRWLockExclusive(&statsLock);
    if(logFile)std::fprintf(logFile,"EFECTO objeto=%p cache=%zu bytes\n",effect,sizeof(*s));
    return true;
}
extern "C" __declspec(dllexport) LONG WINAPI EP_SetEnabled(LONG value) {return InterlockedExchange(&EP_Enabled,value!=0);}
extern "C" __declspec(dllexport) void WINAPI EP_TestRange(const char* begin,size_t bytes) {nameBegin=uintptr_t(begin);nameEnd=nameBegin+bytes;d3dxModule=GetModuleHandleW(L"d3dx9_43.dll");}
extern "C" __declspec(dllexport) unsigned WINAPI EP_Hits(ID3DXEffect* effect) {return state(effect)->hits;}
extern "C" __declspec(dllexport) unsigned WINAPI EP_NegativeHits(ID3DXEffect* effect) {return state(effect)->negativeHits;}

using CreateFn=HRESULT(WINAPI*)(IDirect3DDevice9*,const void*,UINT,const D3DXMACRO*,ID3DXInclude*,DWORD,ID3DXEffectPool*,ID3DXEffect**,ID3DXBuffer**);
using FileFn=HRESULT(WINAPI*)(IDirect3DDevice9*,const char*,const D3DXMACRO*,ID3DXInclude*,DWORD,ID3DXEffectPool*,ID3DXEffect**,ID3DXBuffer**);
static CreateFn createOriginal;
static FileFn fileOriginal;
static HRESULT WINAPI createHook(IDirect3DDevice9* d,const void* p,UINT n,const D3DXMACRO* m,ID3DXInclude* i,DWORD f,ID3DXEffectPool* pool,ID3DXEffect** out,ID3DXBuffer** err) {
    HRESULT hr=createOriginal(d,p,n,m,i,f,pool,out,err);DWORD last=GetLastError();
    if(SUCCEEDED(hr)&&out&&*out)EP_Attach(*out);
    SetLastError(last);return hr;
}
static HRESULT WINAPI fileHook(IDirect3DDevice9* d,const char* p,const D3DXMACRO* m,ID3DXInclude* i,DWORD f,ID3DXEffectPool* pool,ID3DXEffect** out,ID3DXBuffer** err) {
    HRESULT hr=fileOriginal(d,p,m,i,f,pool,out,err);DWORD last=GetLastError();
    if(SUCCEEDED(hr)&&out&&*out)EP_Attach(*out);
    SetLastError(last);return hr;
}
static bool slotValid(uintptr_t base,uintptr_t slot,uintptr_t thunk) {
    auto* p=reinterpret_cast<unsigned char*>(base+thunk);
    if(p[0]!=0xff||p[1]!=0x25||*reinterpret_cast<uintptr_t*>(p+2)!=base+slot)return false;
    void* fn=*reinterpret_cast<void**>(base+slot);
    HMODULE cache=GetModuleHandleW(L"ENBShaderCache.asi");
    return ownerIs(fn,d3dxModule)||(cache&&ownerIs(fn,cache));
}
static void initialize(HMODULE module) {
    wchar_t path[MAX_PATH];GetModuleFileNameW(module,path,MAX_PATH);
    wchar_t* tail=wcsrchr(path,L'\\');if(!tail)return;tail[1]=0;
    wchar_t ini[MAX_PATH],log[MAX_PATH];swprintf_s(ini,L"%sENBParametros.ini",path);swprintf_s(log,L"%sENBParametros.log",path);
    if(!GetPrivateProfileIntW(L"General",L"Activo",1,ini))return;
    _wfopen_s(&logFile,log,L"wt");
    HMODULE enb=GetModuleHandleW(L"enbseries.asi");d3dxModule=GetModuleHandleW(L"d3dx9_43.dll");
    if(!enb||!d3dxModule)return;
    uintptr_t base=uintptr_t(enb);
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || nt->Signature!=IMAGE_NT_SIGNATURE ||
       nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
       nt->FileHeader.TimeDateStamp!=0x5ed67399 || nt->OptionalHeader.SizeOfImage!=0x8f7000 ||
       nt->OptionalHeader.AddressOfEntryPoint!=0x8f5070) {
        if(logFile)std::fprintf(logFile,"FIRMA PE INCOMPATIBLE; no instalado\n");return;
    }
    if(!slotValid(base,0xa629c,0x3dbe0)||!slotValid(base,0xa62a0,0x3dbda)) {
        if(logFile)std::fprintf(logFile,"FIRMA INCOMPATIBLE; no instalado\n");return;
    }
    nameBegin=base;nameEnd=base+nt->OptionalHeader.SizeOfImage;
    DWORD old;
    if(!VirtualProtect(reinterpret_cast<void*>(base+0xa629c),8,PAGE_READWRITE,&old))return;
    createOriginal=*reinterpret_cast<CreateFn*>(base+0xa629c);fileOriginal=*reinterpret_cast<FileFn*>(base+0xa62a0);
    *reinterpret_cast<CreateFn*>(base+0xa629c)=createHook;*reinterpret_cast<FileFn*>(base+0xa62a0)=fileHook;
    DWORD ignored;VirtualProtect(reinterpret_cast<void*>(base+0xa629c),8,old,&ignored);
    if(logFile){std::fprintf(logFile,"INSTALADO 2/2 ENBParametros 1.3; cache de ausencias sin cambiar retornos; contadores vivos sin I/O; shaders intactos\n");std::fflush(logFile);}
}
BOOL WINAPI DllMain(HMODULE m,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(m);initialize(m);}
    return TRUE; // module remains loaded until process exit, as do its objects
}
