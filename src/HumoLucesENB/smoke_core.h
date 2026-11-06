#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <cstddef>

// 0.2: particle lighting of Deferred SkyGFX (skygfx.asi bef78891..., FUN_100b88f0 / FUN_100b85e0 / FUN_100b7d30),
// reconstructed from the binary: per batch the 8 lights whose sphere touches the batch box, ranked by
// ((1-d/R)^2+0.1)*intensity; per vertex a windowed inverse square size/(size+d^2)*(1-(d/R)^4)^2 with the spot cone;
// colour = c + 0.8/max(maxRGB,38) * c * L * 2*255*strength, clamped to 255 (alpha untouched). The light list is
// LucesNocturnasENB's (same role as SkyGFX's deferred list); the vanilla CPointLights table is only a fallback.
namespace sad {
struct Light {   // CPointLight of SA 1.0 (0xC3F0E0)
    float pos[3], dir[3], radius, r, g, b;
    uint32_t entity;
    uint8_t type, fog, shadows, pad;
};
static_assert(sizeof(Light)==48, "SA 1.0 CPointLight contract");
struct LneLight {   // LNE_LucesCuadro of LucesNocturnasENB 2.0i
    float pos[3], radius, col[3], type, dir[3], cosCone;
};
static_assert(sizeof(LneLight)==48, "LucesNocturnasENB 2.0i contract");
struct Vertex { float pos[3], normal[3]; uint32_t rgba; float uv[2]; };
static_assert(sizeof(Vertex)==36 && offsetof(Vertex,rgba)==24,"RwIm3D vertex contract");
// One light as used per vertex. cosCone < -1.5 = point light.
struct Source { float pos[3], radius, col[3], dir[3], cosCone, size; };
struct Settings { float strength=1.0f, source=0.25f, nativeGain=3.0f; int maxLights=8; };

inline bool finite(float x) { return std::isfinite(x); }
inline bool valid(const Source& s) {
    if (!(finite(s.radius) && s.radius>0.01f && s.radius<=250.0f && finite(s.size) && s.size>0.0f)) return false;
    for (int j=0;j<3;++j) if (!finite(s.pos[j]) || !finite(s.dir[j]) || !finite(s.col[j]) || s.col[j]<0.0f) return false;
    return finite(s.cosCone);
}
inline bool fromNative(const Light& l, const Settings& st, Source& s) {
    if (!(finite(l.radius) && l.radius>0.01f && l.radius<=250.0f) || l.type>1) return false;
    for (int j=0;j<3;++j) { s.pos[j]=l.pos[j]; s.dir[j]=l.dir[j]; }
    s.radius=l.radius; s.col[0]=l.r*st.nativeGain; s.col[1]=l.g*st.nativeGain; s.col[2]=l.b*st.nativeGain;
    s.cosCone=-2.0f;
    if (l.type==1) {   // headlight: direction normalised, cone of 41 degrees as LucesNocturnasENB
        float n=std::sqrt(l.dir[0]*l.dir[0]+l.dir[1]*l.dir[1]+l.dir[2]*l.dir[2]); if (!(n>0.01f)) return false;
        for (int j=0;j<3;++j) s.dir[j]/=n;
        s.cosCone=0.75f;
    }
    float r=std::max(st.source,0.01f)*s.radius; s.size=r*r;
    return valid(s);
}
inline bool fromLne(const LneLight& l, const Settings& st, Source& s) {
    for (int j=0;j<3;++j) { s.pos[j]=l.pos[j]; s.dir[j]=l.dir[j]; s.col[j]=l.col[j]; }
    s.radius=l.radius; s.cosCone=(l.type>0.5f) ? l.cosCone : -2.0f;
    float r=std::max(st.source,0.01f)*s.radius; s.size=r*r;
    return valid(s);
}
inline bool smoke(const char name[32]) {
    char lower[33] = {};
    for(int i=0;i<32;++i) { char c=name[i]; lower[i]=(c>='A'&&c<='Z') ? char(c-'A'+'a') : c; }
    // Texture names, not system names: exhaust (collisionsmoke), engine damage/overheat and other ordinary smoke.
    // Fire, blood, rain, explosions and heat haze are excluded.
    return std::strstr(lower,"smoke")!=nullptr && std::strstr(lower,"blood")==nullptr;
}
// FUN_100b7d30: the lights touching the batch box, best first, at most maxLights (<= 8 here, 16 in SkyGFX).
inline int select(const Source* all, int n, const float lo[3], const float hi[3], const Settings& st, Source* out) {
    int maxLights=std::clamp(st.maxLights,1,8), count=0;
    float score[8]={};
    float mid[3]={(lo[0]+hi[0])*0.5f,(lo[1]+hi[1])*0.5f,(lo[2]+hi[2])*0.5f};
    for (int i=0;i<n;++i) {
        const Source& s=all[i]; if (!valid(s)) continue;
        double d2=0; for (int j=0;j<3;++j) { double c=std::clamp(double(s.pos[j]),double(lo[j]),double(hi[j]))-s.pos[j]; d2+=c*c; }
        if (d2>=double(s.radius)*s.radius) continue;
        double dm=0; for (int j=0;j<3;++j) { double c=double(s.pos[j])-mid[j]; dm+=c*c; }
        float t=std::min(float(std::sqrt(dm))/s.radius,1.0f);
        float sc=((1.0f-t)*(1.0f-t)+0.1f)*(s.col[0]*0.2126f+s.col[1]*0.7152f+s.col[2]*0.0722f);
        if (count<maxLights) { out[count]=s; score[count++]=sc; continue; }
        int worst=0; for (int k=1;k<count;++k) if (score[k]<score[worst]) worst=k;
        if (sc>score[worst]) { out[worst]=s; score[worst]=sc; }
    }
    return count;
}
// FUN_100b85e0: light arriving at one point.
inline void lightAt(const float p[3], const Source* s, int n, float L[3]) {
    L[0]=L[1]=L[2]=0.0f;
    for (int i=0;i<n;++i) {
        double q[3]={double(s[i].pos[0])-p[0],double(s[i].pos[1])-p[1],double(s[i].pos[2])-p[2]};
        double d2=q[0]*q[0]+q[1]*q[1]+q[2]*q[2];
        float x=float(d2)/(s[i].radius*s[i].radius); if (x>=1.0f) continue;
        float w=1.0f-x*x; w*=w;
        float a=s[i].size/(s[i].size+float(d2))*w;
        if (s[i].cosCone>-1.5f) {
            double d=std::sqrt(d2); if (d<=1e-6) continue;
            float cs=float(-(q[0]*s[i].dir[0]+q[1]*s[i].dir[1]+q[2]*s[i].dir[2])/d);
            float c=std::clamp((cs-s[i].cosCone)/std::max(1.0f-s[i].cosCone,1e-3f),0.0f,1.0f); c=c*c*(3.0f-2.0f*c);
            a*=c*c;
        }
        for (int j=0;j<3;++j) L[j]+=s[i].col[j]*a;
    }
}
// FUN_100b88f0 (0x100b8b26..0x100b8d68): new vertex colour, D3D packed AARRGGBB.
inline uint32_t combine(uint32_t rgba, const float L[3], const Settings& st) {
    if (!finite(L[0])||!finite(L[1])||!finite(L[2]) || L[0]+L[1]+L[2]<=1e-4f) return rgba;
    float strength=std::min(std::max(finite(st.strength)?st.strength:0.0f,0.0f),4.0f)*2.0f*255.0f;
    float c[3]={float((rgba>>16)&255),float((rgba>>8)&255),float(rgba&255)};
    float m=std::max(std::max(c[0],c[1]),c[2]); if (!(m>38.0f)) m=38.0f;
    float k=0.8f/m;
    uint32_t out=rgba&0xff000000u;
    for (int j=0;j<3;++j) {
        float v=std::min(k*c[j]*L[j]*strength+c[j],255.0f);
        out|=uint32_t(int(v))<<((2-j)*8);   // FUN_10193370 = truncation
    }
    return out;
}
inline uint32_t illuminate(const float p[3], uint32_t rgba, const Source* lights, int n, const Settings& st) {
    if(!finite(p[0])||!finite(p[1])||!finite(p[2])) return rgba;
    float L[3]; lightAt(p,lights,std::clamp(n,0,8),L);
    return combine(rgba,L,st);
}
}
