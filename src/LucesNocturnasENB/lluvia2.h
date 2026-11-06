// LucesNocturnasENB 2.0j (29-10-2026): lluvia de nueva generacion (ver lluvia.hlsl). Modulo sin estado global del
// plugin: lo usan LucesNocturnasENB.cpp y la vista previa fuera del juego (tests\preview_lluvia.cpp).
// Recursos D3DPOOL_MANAGED (texturas, VB, IB): sobreviven al Reset del dispositivo, no hay que liberarlos alli.
#pragma once
#include <d3d9.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include "lluvia_vsg.h"
#include "lluvia_vss.h"
#include "lluvia_psg.h"
#include "lluvia_pss.h"
#include "lluvia_psc.h"
#include "lluvia_vsp.h"
#include "lluvia_tex.h"

namespace lluvia2 {

struct Luz { float pos[3], radio, col[3], tipo, dir[3], cosCono; };   // = LNE_Luz (posicion absoluta)
static_assert(sizeof(Luz) == 48, "Luz");

struct Config {
    int gotas = 3000, salpicaduras = 400, cortinas = 1, luces = 24;
    float distancia = 12.0f, alto = 14.0f, base = -5.0f;     // caja de gotas alrededor de la camara (m)
    float velocidad = 30.0f, angulo = 10.0f, viento = 0.30f; // caida (m/s), inclinacion (grados) y efecto del viento
    float exposicion = 0.022f;                               // estela de obturador (s)
    float intensidad = 2.2f;                                 // multiplicador de la forma de la gota (g_fIntensity de Proper Shaders)
    float ancho = 0.014f, anchoMinPx = 1.4f;                 // ancho de la gota (m) y minimo en pantalla (px)
    float opacidad = 0.80f, refraccion = 0.012f, brilloRefr = 1.10f, pesoLuz = 3.5f, contraluz = 1.55f;
    float suavidad = 2.0f;                                   // borde suave contra la geometria (1/m)
    float salpZona = 20.0f, salpRadio = 0.22f, salpAlto = 0.8f, salpOpacidad = 1.0f, salpPeriodo = 0.30f;
    float cortinaD[3] = { 10.0f, 22.0f, 45.0f }, cortinaInt = 0.55f, cortinaVel = 9.0f, cortinaPatron = 7.0f, cortinaLuz = 1.0f;
    float tinte[3] = { 0.90f, 0.95f, 1.00f };
};
struct Entrada {
    float vpRel[16];          // View sin traslacion x Proj (vector fila), como g_vpRel
    float projA, projB, projYs;   // de D3DTS_PROJECTION: _33, _43 (z de vista = B / (d - A)) y _22
    double cam[3];            // posicion de la camara en el mundo
    float dt;                 // segundos de juego de este cuadro (0 en pausa)
    float lluvia;             // CWeather::Rain 0..1
    float vientoDir[2], vientoFuerza;
    float ambiente[3];        // luz difusa que reciben las gotas
    const Luz* luces; int nLuces;
    const float* techo; const uint8_t* techoOk; int techoX0, techoY0, rej; float celda;   // rejilla de la 2.0i
    float abierto;            // 0..1: cielo abierto alrededor de la camara (cortinas)
    IDirect3DTexture9* escena; IDirect3DTexture9* prof; IDirect3DTexture9* luz;   // luz: acumulada o nullptr
    int W, H;
};
struct Estado {
    IDirect3DVertexShader9* vsG = nullptr, *vsS = nullptr, *vsP = nullptr; IDirect3DPixelShader9* psG = nullptr, *psS = nullptr, *psC = nullptr;
    IDirect3DVertexDeclaration9* decl = nullptr; IDirect3DVertexBuffer9* vbG = nullptr, *vbS = nullptr; IDirect3DIndexBuffer9* ib = nullptr;
    IDirect3DTexture9* tGota = nullptr, *tGotaN = nullptr, *tPatron = nullptr, *tNegro = nullptr;
    int nG = 0, nS = 0, fallo = 0;
    double t = 0.0; double prevCam[3] = { 0, 0, 0 }; float camVel[3] = { 0, 0, 0 }; int camOk = 0;
    long gotasUlt = 0, salpUlt = 0, cortinasUlt = 0, lucesUlt = 0;
};

static IDirect3DTexture9* CargaDDS(IDirect3DDevice9* d, const unsigned char* b, size_t n) {
    if (n < 128 || memcmp(b, "DDS ", 4)) return nullptr;
    uint32_t h = *(const uint32_t*)(b + 12), w = *(const uint32_t*)(b + 16), mips = *(const uint32_t*)(b + 28);
    if (memcmp(b + 84, "DXT1", 4) || !w || !h) return nullptr;
    if (!mips) mips = 1;
    IDirect3DTexture9* t = nullptr;
    if (FAILED(d->CreateTexture(w, h, mips, 0, D3DFMT_DXT1, D3DPOOL_MANAGED, &t, nullptr))) return nullptr;
    size_t o = 128;
    for (uint32_t m = 0; m < mips; m++) {
        uint32_t bw = (w + 3) / 4 ? (w + 3) / 4 : 1, bh = (h + 3) / 4 ? (h + 3) / 4 : 1, fila = bw * 8;
        if (o + (size_t)fila * bh > n) break;
        D3DLOCKED_RECT l; if (FAILED(t->LockRect(m, &l, nullptr, 0))) break;
        for (uint32_t y = 0; y < bh; y++) memcpy((uint8_t*)l.pBits + y * l.Pitch, b + o + (size_t)y * fila, fila);
        t->UnlockRect(m); o += (size_t)fila * bh;
        w = w > 1 ? w / 2 : 1; h = h > 1 ? h / 2 : 1;
    }
    return t;
}
static IDirect3DTexture9* CargaL8(IDirect3DDevice9* d, const unsigned char* b, int lado, int niveles) {
    IDirect3DTexture9* t = nullptr;
    if (FAILED(d->CreateTexture(lado, lado, niveles, 0, D3DFMT_L8, D3DPOOL_MANAGED, &t, nullptr))) return nullptr;
    size_t o = 0; int s = lado;
    for (int m = 0; m < niveles; m++) {
        D3DLOCKED_RECT l; if (FAILED(t->LockRect(m, &l, nullptr, 0))) break;
        for (int y = 0; y < s; y++) memcpy((uint8_t*)l.pBits + y * l.Pitch, b + o + (size_t)y * s, s);
        t->UnlockRect(m); o += (size_t)s * s; s = s > 1 ? s / 2 : 1;
    }
    return t;
}
static uint32_t Azar(uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
struct Vert { float s[4]; float e[2]; };
static IDirect3DVertexBuffer9* CreaVB(IDirect3DDevice9* d, int n, uint32_t semilla) {
    IDirect3DVertexBuffer9* vb = nullptr;
    if (FAILED(d->CreateVertexBuffer(n * 4 * sizeof(Vert), D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &vb, nullptr))) return nullptr;
    Vert* v = nullptr; if (FAILED(vb->Lock(0, 0, (void**)&v, 0))) { vb->Release(); return nullptr; }
    static const float esq[4][2] = { { -1, 0 }, { 1, 0 }, { -1, 1 }, { 1, 1 } };
    for (int i = 0; i < n; i++) {
        float s4[4]; for (int k = 0; k < 4; k++) s4[k] = (float)(Azar(semilla) >> 8) * (1.0f / 16777216.0f);
        s4[3] = ((float)i + 0.5f) / (float)n;   // fraccion activa ordenada: con poca lluvia se dibujan las primeras
        for (int c = 0; c < 4; c++) { memcpy(v[i * 4 + c].s, s4, 16); v[i * 4 + c].e[0] = esq[c][0]; v[i * 4 + c].e[1] = esq[c][1]; }
    }
    vb->Unlock(); return vb;
}
static int Crea(Estado& e, IDirect3DDevice9* d, const Config& c, char* err, size_t nerr) {
    if (e.fallo) return 0;
    auto mal = [&](const char* m) { e.fallo = 1; if (err) { strncpy(err, m, nerr - 1); err[nerr - 1] = 0; } return 0; };
    if (!e.vsG && FAILED(d->CreateVertexShader((const DWORD*)g_vsGota, &e.vsG))) return mal("VS de gotas");
    if (!e.vsS && FAILED(d->CreateVertexShader((const DWORD*)g_vsSalpica, &e.vsS))) return mal("VS de salpicaduras");
    if (!e.vsP && FAILED(d->CreateVertexShader((const DWORD*)g_vsPantalla, &e.vsP))) return mal("VS de pantalla");
    if (!e.psG && FAILED(d->CreatePixelShader((const DWORD*)g_psGota, &e.psG))) return mal("PS de gotas");
    if (!e.psS && FAILED(d->CreatePixelShader((const DWORD*)g_psSalpica, &e.psS))) return mal("PS de salpicaduras");
    if (!e.psC && FAILED(d->CreatePixelShader((const DWORD*)g_psCortina, &e.psC))) return mal("PS de cortinas");
    if (!e.decl) { D3DVERTEXELEMENT9 el[] = { { 0, 0, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_POSITION, 0 }, { 0, 16, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_TEXCOORD, 0 }, D3DDECL_END() };
        if (FAILED(d->CreateVertexDeclaration(el, &e.decl))) return mal("declaracion de vertices"); }
    int nMax = c.gotas > c.salpicaduras ? c.gotas : c.salpicaduras;
    if (!e.ib) {
        if (FAILED(d->CreateIndexBuffer(nMax * 6 * 2, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &e.ib, nullptr))) return mal("IB");
        uint16_t* ix = nullptr; if (FAILED(e.ib->Lock(0, 0, (void**)&ix, 0))) return mal("IB lock");
        for (int i = 0; i < nMax; i++) { uint16_t b = (uint16_t)(i * 4); uint16_t q[6] = { b, (uint16_t)(b + 1), (uint16_t)(b + 2), (uint16_t)(b + 2), (uint16_t)(b + 1), (uint16_t)(b + 3) }; memcpy(ix + i * 6, q, 12); }
        e.ib->Unlock();
    }
    if (!e.vbG && c.gotas > 0) { if (!(e.vbG = CreaVB(d, c.gotas, 0x9E3779B9u))) return mal("VB de gotas"); e.nG = c.gotas; }
    if (!e.vbS && c.salpicaduras > 0) { if (!(e.vbS = CreaVB(d, c.salpicaduras, 0x85EBCA6Bu))) return mal("VB de salpicaduras"); e.nS = c.salpicaduras; }
    if (!e.tGota && !(e.tGota = CargaDDS(d, kGotaDDS, sizeof kGotaDDS))) return mal("RainDrop.dds");
    if (!e.tGotaN && !(e.tGotaN = CargaDDS(d, kGotaNDDS, sizeof kGotaNDDS))) return mal("RainDrop_nm.dds");
    if (!e.tPatron && !(e.tPatron = CargaL8(d, kPatronL8, 256, 9))) return mal("patron de cortinas");
    if (!e.tNegro) { if (FAILED(d->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &e.tNegro, nullptr))) return mal("textura negra");
        D3DLOCKED_RECT l; if (SUCCEEDED(e.tNegro->LockRect(0, &l, nullptr, 0))) { *(uint32_t*)l.pBits = 0; e.tNegro->UnlockRect(0); } }
    return 1;
}
static void Libera(Estado& e) {
    IUnknown* r[] = { e.vsG, e.vsS, e.vsP, e.psG, e.psS, e.psC, e.decl, e.vbG, e.vbS, e.ib, e.tGota, e.tGotaN, e.tPatron, e.tNegro };
    for (auto* x : r) if (x) x->Release();
    Estado limpio; e = limpio;
}
// Dibuja sobre el destino 0 actual. El llamador guarda y repone el estado del dispositivo (state block).
static void Dibuja(Estado& e, IDirect3DDevice9* d, const Config& c, const Entrada& in) {
    e.gotasUlt = e.salpUlt = e.cortinasUlt = e.lucesUlt = 0;
    float ll = in.lluvia < 0 ? 0 : (in.lluvia > 1 ? 1 : in.lluvia);
    e.t += in.dt;
    // velocidad de la camara (estela relativa), suavizada; un salto grande (teletransporte) no cuenta
    if (e.camOk && in.dt > 1e-4f) {
        float v[3]; double s = 0; for (int k = 0; k < 3; k++) { v[k] = (float)((in.cam[k] - e.prevCam[k]) / in.dt); s += (double)v[k] * v[k]; }
        if (s < 120.0 * 120.0) for (int k = 0; k < 3; k++) e.camVel[k] += (v[k] - e.camVel[k]) * 0.25f;
        else for (int k = 0; k < 3; k++) e.camVel[k] = 0;
    }
    if (in.dt > 1e-4f || !e.camOk) { memcpy(e.prevCam, in.cam, sizeof e.prevCam); e.camOk = 1; }
    if (ll <= 0.001f || !in.prof || !in.escena) return;
    // caida con viento
    float wl = sqrtf(in.vientoDir[0] * in.vientoDir[0] + in.vientoDir[1] * in.vientoDir[1]);
    float wx = wl > 1e-3f ? in.vientoDir[0] / wl : 0.0f, wy = wl > 1e-3f ? in.vientoDir[1] / wl : 0.0f;
    float incl = tanf(c.angulo * 3.14159265f / 180.0f) + c.viento * in.vientoFuerza;
    float vel[3] = { wx * c.velocidad * incl, wy * c.velocidad * incl, -c.velocidad };
    // envoltura anclada al mundo: fraccion de (vel*t - camara) en la caja, en double
    double caja[3] = { 2.0 * c.distancia, 2.0 * c.distancia, c.alto };
    double rel[3] = { vel[0] * e.t - in.cam[0] + c.distancia, vel[1] * e.t - in.cam[1] + c.distancia, vel[2] * e.t - in.cam[2] - c.base };
    float off[3]; for (int k = 0; k < 3; k++) { double f = rel[k] / caja[k]; off[k] = (float)(f - floor(f)); }
    // proyeccion: focal en px a 1 m y linealizacion de la profundidad
    const float* M = in.vpRel;
    float focal = in.projYs * (float)in.H * 0.5f;
    float lin[4] = { in.projA, in.projB, 0, 0 };
    // luces: las mas cercanas, relativas a la camara
    float VSc[142][4]; memset(VSc, 0, sizeof VSc);
    memcpy(VSc[0], M, 64);
    VSc[4][0] = (float)caja[0]; VSc[4][1] = c.alto; VSc[4][2] = c.base; VSc[4][3] = c.distancia;
    VSc[5][0] = vel[0]; VSc[5][1] = vel[1]; VSc[5][2] = vel[2]; VSc[5][3] = c.exposicion;
    VSc[6][0] = off[0]; VSc[6][1] = off[1]; VSc[6][2] = off[2]; VSc[6][3] = c.ancho;
    VSc[7][0] = (float)in.W; VSc[7][1] = (float)in.H; VSc[7][2] = c.anchoMinPx; VSc[7][3] = ll;
    for (int k = 0; k < 3; k++) VSc[8][k] = in.ambiente[k] * c.tinte[k];
    VSc[8][3] = c.opacidad * (0.55f + 0.45f * ll);
    VSc[9][0] = e.camVel[0]; VSc[9][1] = e.camVel[1]; VSc[9][2] = e.camVel[2]; VSc[9][3] = focal;
    int rej = in.rej; float celda = in.celda > 0 ? in.celda : 4.0f;
    VSc[10][0] = (float)(in.techoX0 * (double)celda - in.cam[0]); VSc[10][1] = (float)(in.techoY0 * (double)celda - in.cam[1]);
    VSc[10][2] = 1.0f / celda; VSc[10][3] = (float)(-in.cam[2]);
    float* techo = &VSc[11][0];
    for (int i = 0; i < 225; i++) {
        float h = -1.0e9f;   // sin medir o sin rejilla: se trata como cielo abierto sin superficie
        if (in.techo && rej == 15) { h = (in.techoOk && !in.techoOk[i]) ? 1.0e9f : in.techo[i]; if (h < -1.0e8f) h = -1.0e9f; }
        techo[i] = h;
    }
    // (nota: celda sin medir = 1e9 -> todo "bajo techo", como la 2.0i, que no dibujaba alli)
    int idx[24], n = 0; float dist2[24];
    for (int k = 0; k < in.nLuces; k++) {
        const Luz& l = in.luces[k];
        if (l.col[0] + l.col[1] + l.col[2] <= 1e-4f) continue;
        double dx = l.pos[0] - in.cam[0], dy = l.pos[1] - in.cam[1], dz = l.pos[2] - in.cam[2];
        float dd = (float)sqrt(dx * dx + dy * dy + dz * dz), lim = c.distancia * 1.2f + l.radio;
        if (dd > lim) continue;
        float key = dd - l.radio;
        int maxL = c.luces < 24 ? c.luces : 24;
        if (n < maxL) { idx[n] = k; dist2[n] = key; n++; }
        else { int peor = 0; for (int j = 1; j < n; j++) if (dist2[j] > dist2[peor]) peor = j; if (key < dist2[peor]) { idx[peor] = k; dist2[peor] = key; } }
    }
    e.lucesUlt = n;
    VSc[68][0] = (float)n; VSc[68][1] = c.contraluz; VSc[68][2] = (float)fmod(e.t, 3600.0); VSc[68][3] = c.salpPeriodo;
    for (int j = 0; j < n; j++) {
        const Luz& l = in.luces[idx[j]];
        VSc[69 + j][0] = (float)(l.pos[0] - in.cam[0]); VSc[69 + j][1] = (float)(l.pos[1] - in.cam[1]); VSc[69 + j][2] = (float)(l.pos[2] - in.cam[2]); VSc[69 + j][3] = l.radio;
        for (int k = 0; k < 3; k++) VSc[93 + j][k] = l.col[k] * c.pesoLuz * c.tinte[k];
        VSc[93 + j][3] = l.tipo;
        memcpy(VSc[117 + j], l.dir, 12); VSc[117 + j][3] = l.cosCono;
    }
    VSc[141][0] = c.salpZona; VSc[141][1] = c.salpRadio; VSc[141][2] = c.salpAlto; VSc[141][3] = c.salpOpacidad * (0.4f + 0.6f * ll);
    float PSc[3][4] = { { 1.0f / in.W, 1.0f / in.H, c.refraccion, c.suavidad }, { lin[0], lin[1], 0, 0 }, { c.brilloRefr, 1.0f, (float)fmod(e.t, 1000.0), c.intensidad } };

    d->SetRenderState(D3DRS_ZENABLE, FALSE); d->SetRenderState(D3DRS_ZWRITEENABLE, FALSE); d->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    d->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE); d->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE); d->SetRenderState(D3DRS_FOGENABLE, FALSE);
    d->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE); d->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE); d->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    d->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
    d->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE); d->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    auto muestreo = [&](int s, D3DTEXTUREFILTERTYPE f, D3DTEXTUREADDRESS a, D3DTEXTUREFILTERTYPE mip) {
        d->SetSamplerState(s, D3DSAMP_MINFILTER, f); d->SetSamplerState(s, D3DSAMP_MAGFILTER, f); d->SetSamplerState(s, D3DSAMP_MIPFILTER, mip);
        d->SetSamplerState(s, D3DSAMP_ADDRESSU, a); d->SetSamplerState(s, D3DSAMP_ADDRESSV, a); d->SetSamplerState(s, D3DSAMP_SRGBTEXTURE, 0); d->SetSamplerState(s, D3DSAMP_MAXMIPLEVEL, 0); };

    // (3) cortinas: lo mas lejano primero (premultiplicado: ONE, INVSRCALPHA)
    if (c.cortinas && in.abierto > 0.01f) {
        IDirect3DVertexShader9* vsP = e.vsP; IDirect3DVertexDeclaration9* dP = e.decl;
        float inv[16];
        { // inversa (double) de la VP relativa
            double m[16], r[16]; for (int i = 0; i < 16; i++) { m[i] = M[i]; r[i] = (i % 5 == 0) ? 1.0 : 0.0; }
            int ok = 1;
            for (int col = 0; col < 4 && ok; col++) { int piv = col; for (int rr = col + 1; rr < 4; rr++) if (fabs(m[rr * 4 + col]) > fabs(m[piv * 4 + col])) piv = rr;
                if (fabs(m[piv * 4 + col]) < 1e-30) { ok = 0; break; }
                for (int k = 0; k < 4; k++) { double t = m[col * 4 + k]; m[col * 4 + k] = m[piv * 4 + k]; m[piv * 4 + k] = t; t = r[col * 4 + k]; r[col * 4 + k] = r[piv * 4 + k]; r[piv * 4 + k] = t; }
                double dv = 1.0 / m[col * 4 + col]; for (int k = 0; k < 4; k++) { m[col * 4 + k] *= dv; r[col * 4 + k] *= dv; }
                for (int rr = 0; rr < 4; rr++) if (rr != col) { double f = m[rr * 4 + col]; for (int k = 0; k < 4; k++) { m[rr * 4 + k] -= f * m[col * 4 + k]; r[rr * 4 + k] -= f * r[col * 4 + k]; } } }
            for (int i = 0; i < 16; i++) inv[i] = (float)r[i];
            if (!ok) vsP = nullptr;
        }
        if (vsP && dP) {
            float C[12][4] = {}; memcpy(C[0], inv, 64); memcpy(C[4], M, 64);
            C[8][0] = c.cortinaD[0]; C[8][1] = c.cortinaD[1]; C[8][2] = c.cortinaD[2]; C[8][3] = (float)fmod(e.t, 3600.0);
            C[9][0] = c.cortinaVel; C[9][1] = -incl * 0.5f; C[9][2] = c.cortinaPatron; C[9][3] = c.cortinaInt * ll * in.abierto;
            for (int k = 0; k < 3; k++) C[10][k] = in.ambiente[k] * c.tinte[k] * 1.3f;
            C[10][3] = in.luz ? c.cortinaLuz : 0.0f;
            C[11][0] = lin[0]; C[11][1] = lin[1]; C[11][2] = 1.0f / in.W; C[11][3] = 1.0f / in.H;
            d->SetPixelShaderConstantF(0, &C[0][0], 12);
            d->SetTexture(0, in.prof); d->SetTexture(1, e.tPatron); d->SetTexture(2, in.luz ? (IDirect3DBaseTexture9*)in.luz : e.tNegro);
            muestreo(0, D3DTEXF_POINT, D3DTADDRESS_CLAMP, D3DTEXF_NONE); muestreo(1, D3DTEXF_LINEAR, D3DTADDRESS_WRAP, D3DTEXF_LINEAR); muestreo(2, D3DTEXF_LINEAR, D3DTADDRESS_CLAMP, D3DTEXF_NONE);
            d->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE); d->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            float W = (float)in.W, H = (float)in.H, ox = -1.0f / W, oy = 1.0f / H;
            float q[4][6] = { { -1 + ox, 1 + oy, 0, 1, 0, 0 }, { 1 + ox, 1 + oy, 0, 1, 1, 0 }, { -1 + ox, -1 + oy, 0, 1, 0, 1 }, { 1 + ox, -1 + oy, 0, 1, 1, 1 } };
            d->SetVertexShader(vsP); d->SetPixelShader(e.psC); d->SetVertexDeclaration(dP);
            if (SUCCEEDED(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof q[0]))) e.cortinasUlt = 3;
        }
    }
    // (1) y (2): gotas y salpicaduras, mezcla normal con la refraccion de la escena
    d->SetVertexShaderConstantF(0, &VSc[0][0], 142);
    d->SetPixelShaderConstantF(0, &PSc[0][0], 3);
    d->SetTexture(0, in.prof); d->SetTexture(1, in.escena); d->SetTexture(2, e.tGota); d->SetTexture(3, e.tGotaN);
    muestreo(0, D3DTEXF_POINT, D3DTADDRESS_CLAMP, D3DTEXF_NONE); muestreo(1, D3DTEXF_LINEAR, D3DTADDRESS_CLAMP, D3DTEXF_NONE);
    muestreo(2, D3DTEXF_LINEAR, D3DTADDRESS_CLAMP, D3DTEXF_LINEAR); muestreo(3, D3DTEXF_LINEAR, D3DTADDRESS_CLAMP, D3DTEXF_LINEAR);
    d->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA); d->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    d->SetVertexDeclaration(e.decl); d->SetIndices(e.ib);
    if (e.vbS && e.nS > 0 && c.salpicaduras > 0) {
        d->SetStreamSource(0, e.vbS, 0, sizeof(Vert)); d->SetVertexShader(e.vsS); d->SetPixelShader(e.psS);
        int n2 = c.salpicaduras < e.nS ? c.salpicaduras : e.nS;
        if (SUCCEEDED(d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, n2 * 4, 0, n2 * 2))) e.salpUlt = (long)(n2 * ll);
    }
    if (e.vbG && e.nG > 0) {
        d->SetStreamSource(0, e.vbG, 0, sizeof(Vert)); d->SetVertexShader(e.vsG); d->SetPixelShader(e.psG);
        int n1 = c.gotas < e.nG ? c.gotas : e.nG, act = (int)(n1 * ll + 0.999f); if (act > n1) act = n1;
        if (act > 0 && SUCCEEDED(d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, act * 4, 0, act * 2))) e.gotasUlt = act;
    }
    d->SetStreamSource(0, nullptr, 0, 0); d->SetIndices(nullptr);
    for (int s = 0; s < 4; s++) d->SetTexture(s, nullptr);
}

}   // namespace lluvia2
