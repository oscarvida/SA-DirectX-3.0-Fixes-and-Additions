// LucesNocturnasENB - luces locales diferidas (2026-10-02, v1.6).
// Se dibuja con el dispositivo REAL, despues de la composicion de ENB (enblighting + 1.fx) y antes de
// su postproceso, sumando (ONE/ONE) sobre la escena HDR. Compilar con fxc /Zpr (matrices por filas).
//
// v1.1: normal geometrica por pixel con 4 muestras de profundidad (sin los bloques 2x2 de ddx/ddy).
// v1.2: se probo la normal del MRT1 de ENB: no sirve (GTASANormalPS la pasa por WVP y luego por View;
//       GTASACharacter usa un TEXCOORD7 sin VS propio; peds y coches nunca concordaban) y el salto por
//       pixel entre las dos normales granulaba las paredes. Retirada en la 1.3.
// v1.3: normal suavizada en un radio fijo en el mundo (dos anillos de 8 muestras de profundidad).
// v1.4: DIFERIDO CLASICO. (1) PS_Normal calcula la normal suavizada UNA vez por pixel y la guarda en una
//       textura; (2) cada luz se dibuja como un rectangulo que cubre solo su esfera en pantalla (VS_Luz /
//       PS_Luz) y lee esa normal. El coste ya no es "luces x pixeles de toda la pantalla", y caben 256 luces.
//       El desvanecido por distancia va en el color de cada luz (CPU), no por pixel.
// v1.6: (1) las luces se ACUMULAN en una textura propia (sin albedo) y PS_Compone las suma a la escena con
//       una rodilla suave: lineal hasta Param.x, y comprimida hacia Param.y. Con muchas luces juntas (la
//       fachada de Binco) la suma ya no se dispara. (2) La distancia a la luz no baja de Param.w x radio:
//       un foco pegado a una pared (carteles) ya no deja un punto quemado.
//
// 2.0a (F1, ideas de Proper Shaders 08-26, shader DeferredLights): (1) foco de farola (tipo 2): cono hacia
//       abajo con borde suave saturate((cos - cosExt) / ancho) (PS usa ancho 0,12); (2) especular en mojado
//       Blinn-Phong normalizado (n+2)/8 (N.H)^n solo en superficies que miran arriba, a un segundo destino
//       (COLOR1) que se compone SIN albedo; (3) atenuacion smoothstep de PS como alternativa a pow.
// 2.0b (F2, shader VolumetricLights de Proper Shaders, reescrito): haz volumetrico bajo los focos de farola.
//       Por cada haz, un rectangulo que cubre su esfera; el rayo de la camara se corta con la esfera y con la
//       profundidad de la escena y se integra en 6 pasos el cono (interior/exterior), el perfil radial y la caida a
//       lo largo del haz, con una fase hacia delante. Se suma (ONE/ONE) sobre la escena despues de la composicion.
//
// s0 = profundidad INTZ (z/w); s1 = copia de la escena del juego ANTES de ENB (albedo aproximado);
// s2 = normal suavizada (xyz*0.5+0.5, w = pares validos/16; w = 0 en el cielo); s4 = especular acumulado.

sampler2D sProf : register(s0);
sampler2D sAlb  : register(s1);
sampler2D sNorm : register(s2);
sampler2D sAcc  : register(s3);   // 1.6: luz acumulada (sin albedo)
sampler2D sEsp  : register(s4);   // 2.0a: especular acumulado (no se multiplica por el albedo)

float4x4 InvVP    : register(c0);   // clip -> mundo (vector fila: mundo = clip * InvVP)
float4 Param      : register(c4);   // x rodilla, y maximo de la luz acumulada, z albedo minimo, w radio de la fuente (fraccion del radio)
float4 Camara     : register(c5);   // xyz posicion de la camara, w = 1/alto
float4 Param2     : register(c6);   // x exponente de atenuacion, y luz envolvente (0..1), z peso del albedo, w = 1/ancho
float4 Espec      : register(c7);   // 2.0a: x intensidad especular x mojado (0 = nada), y potencia, z atenuacion (0 pow, 1 smoothstep), w ancho del borde de los focos
float4 HazA       : register(c8);   // 2.0b: x cos interior, y cos exterior, z tan exterior, w largo del haz (m)
float4 HazB       : register(c9);   // 2.0b: x radio del haz (m), y densidad x intensidad, z 1/(cos int - cos ext), w 1/largo
float4 Param3     : register(c11);  // y modo de depuracion, w radio del suavizado de la normal (m)
// 2.0i (28-10-2026, aportes SkyGFX): sombras de faros en pantalla y lluvia iluminada
float4x4 VP       : register(c12);  // mundo relativo a la camara -> clip (vector fila), inversa de InvVP
float4 Somb       : register(c16);  // x pasos (0 = sin sombras de faros), y grosor de un oclusor (m), z sesgo (m), w fuerza 0..1

float3 Mundo(float2 uv, float d)
{
    float4 w = mul(float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, d, 1.0), InvVP);
    return w.xyz / w.w;
}

// ------------------------------------------------------------------ pasada 1: normal suavizada
struct VSI { float4 pos : POSITION; float2 uv : TEXCOORD0; };
struct VSO { float4 pos : POSITION; float2 uv : TEXCOORD0; };
VSO VS(VSI i) { VSO o; o.pos = i.pos; o.uv = i.uv; return o; }

float4 PS_Normal(float2 uv : TEXCOORD0) : COLOR
{
    float d = tex2Dlod(sProf, float4(uv, 0, 0)).r;
    if (d >= 0.999995) return float4(0.5, 0.5, 0.5, 0.0);   // cielo
    float3 P = Mundo(uv, d);
    float3 V = Camara.xyz - P;

    // normal geometrica de 4 muestras (1.1): respaldo
    float2 tx = float2(Param2.w, 0.0), ty = float2(0.0, Camara.w);
    float dxp = tex2Dlod(sProf, float4(uv + tx, 0, 0)).r, dxm = tex2Dlod(sProf, float4(uv - tx, 0, 0)).r;
    float dyp = tex2Dlod(sProf, float4(uv + ty, 0, 0)).r, dym = tex2Dlod(sProf, float4(uv - ty, 0, 0)).r;
    float3 ex = (abs(dxp - d) < abs(d - dxm)) ? (Mundo(uv + tx, dxp) - P) : (P - Mundo(uv - tx, dxm));
    float3 ey = (abs(dyp - d) < abs(d - dym)) ? (Mundo(uv + ty, dyp) - P) : (P - Mundo(uv - ty, dym));
    float3 Ng = normalize(cross(ey, ex));

    // 1.3: dos anillos de 8 muestras a un radio fijo en el mundo (Param3.w m); suma de los productos
    // vectoriales de cada par consecutivo valido (normal media del abanico). Una muestra de otra superficie
    // (mas de 4 veces el salto que tendria un plano a 75 grados) no cuenta.
    float L0 = length(Mundo(uv + tx, d) - P);
    float rpx = clamp(Param3.w / max(L0, 1e-5), 1.5, 24.0);
    float3 Ns = 0.0; float validas = 0.0;
    [unroll] for (int a = 0; a < 2; a++)
    {
        float rr = (a == 0) ? rpx : rpx * 0.5;
        float lim = 4.0 * L0 * rr;
        float vprev = 0.0; float3 qprev = 0.0;
        [unroll] for (int k = 0; k < 9; k++)
        {
            float ang = (k % 8) * 0.785398 + a * 0.392699;
            float2 o = float2(cos(ang) * tx.x, sin(ang) * ty.y) * rr;
            float dk = tex2Dlod(sProf, float4(uv + o, 0, 0)).r;
            float3 q = Mundo(uv + o, dk) - P;
            float vk = (length(q) < lim && dk < 0.999995) ? 1.0 : 0.0;
            if (k > 0) { Ns += cross(qprev, q) * (vk * vprev); validas += vk * vprev; }
            qprev = q; vprev = vk;
        }
    }
    float3 N = (validas > 1.5 && dot(Ns, Ns) > 1e-12) ? normalize(Ns) : Ng;
    if (dot(N, V) < 0.0) N = -N;
    return float4(N * 0.5 + 0.5, max(validas / 16.0, 0.02));
}

// depuracion (modo 1 = pares validos del suavizado, verde 16 / rojo 0; modo 2 = normal en color)
float4 PS_Depuracion(float2 uv : TEXCOORD0) : COLOR
{
    float4 n = tex2Dlod(sNorm, float4(uv, 0, 0));
    if (n.w <= 0.0) return float4(0, 0, 0, 0);
    return (Param3.y < 1.5) ? float4((1.0 - n.w) * 0.3, n.w * 0.3, 0.0, 0.0) : float4(n.xyz * 0.3, 0.0);
}

// ------------------------------------------------------------------ pasada 2: una luz por rectangulo
struct VSIL { float4 pos : POSITION; float4 lp : TEXCOORD0; float4 lc : TEXCOORD1; float4 ld : TEXCOORD2; };
struct VSOL { float4 pos : POSITION; float4 lp : TEXCOORD0; float4 lc : TEXCOORD1; float4 ld : TEXCOORD2; };
VSOL VS_Luz(VSIL i) { VSOL o; o.pos = i.pos; o.lp = i.lp; o.lc = i.lc; o.ld = i.ld; return o; }

// lp = posicion + radio; lc = color x ganancia x desvanecido + tipo (0 puntual, 1 faro, 2 foco de farola);
// ld = direccion + coseno del cono (2: coseno exterior)
struct PSL { float4 dif : COLOR0; float4 esp : COLOR1; };
PSL PS_Luz(float4 lp : TEXCOORD0, float4 lc : TEXCOORD1, float4 ld : TEXCOORD2, float2 vpos : VPOS)
{
    float2 uv = (vpos + 0.5) * float2(Param2.w, Camara.w);
    float4 nn = tex2Dlod(sNorm, float4(uv, 0, 0));
    clip(nn.w - 0.001);                                   // cielo
    float d = tex2Dlod(sProf, float4(uv, 0, 0)).r;
    float3 P = Mundo(uv, d);
    float3 N = nn.xyz * 2.0 - 1.0;
    float3 Lv = lp.xyz - P;
    float dist = length(Lv);
    clip(lp.w - dist);                                    // fuera de la esfera
    float t = saturate(1.0 - max(dist, Param.w * lp.w) / lp.w);
    float att = (Espec.z > 0.5) ? t * t * (3.0 - 2.0 * t) : pow(t, Param2.x);
    float3 Ln = Lv / max(dist, 0.001);
    float nl = dot(N, Ln);
    float ndl = saturate((nl + Param2.y) / (1.0 + Param2.y));
    if (lc.w > 1.5)
    {
        att *= saturate((dot(-Ln, ld.xyz) - ld.w) / max(Espec.w, 0.001));   // 2.0a: foco de farola (borde de PS)
    }
    else if (lc.w > 0.5)
    {
        float sp = saturate((dot(-Ln, ld.xyz) - ld.w) / max(1.0 - ld.w, 0.001));
        att *= sp * sp;
        // 2.0i: sombra del faro (headlightMapBeam de SkyGFX, aqui sin mapa de sombras: trazado en pantalla
        // contra la profundidad de la escena). Se recorre el segmento superficie -> faro y, si algo visible
        // queda delante del punto a menos de Somb.y metros, la luz no llega. Lo que esta fuera de pantalla no
        // proyecta sombra (limite conocido del metodo).
        if (Somb.x > 0.5 && att > 0.0005)
        {
            float3 seg = Lv;
            float fin = saturate(1.0 - 0.35 / max(dist, 0.36));   // se para 35 cm antes del faro (carroceria)
            float ign = frac(52.98292 * frac(dot(vpos, float2(0.06711056, 0.00583715))));
            float tapa = 0.0;
            [loop] for (int k = 0; k < 24; k++)
            {
                if (k >= Somb.x) break;
                float s = (k + 0.5 + ign * 0.5) / Somb.x * fin;
                float3 Q = P + seg * s + N * 0.04;
                float4 c = mul(float4(Q, 1.0), VP);
                if (c.w < 0.05) continue;
                float2 suv = float2(c.x / c.w * 0.5 + 0.5, 0.5 - c.y / c.w * 0.5);
                if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0) continue;
                float ds = tex2Dlod(sProf, float4(suv, 0, 0)).r;
                if (ds >= 0.99999) continue;
                float hueco = length(Q) - length(Mundo(suv, ds));
                if (hueco > Somb.z && hueco < Somb.y) { tapa = 1.0; break; }
            }
            att *= 1.0 - tapa * Somb.w;
        }
    }
    PSL o;
    o.dif = float4(lc.rgb * (att * ndl), 0.0);
    o.esp = float4(0, 0, 0, 0);
    if (Espec.x > 0.0)
    {
        float3 V = normalize(Camara.xyz - P);
        float3 H = normalize(Ln + V);
        float arriba = saturate((N.z - 0.7) * 5.0);       // solo suelo y superficies casi horizontales
        float s = (Espec.y + 2.0) * 0.125 * pow(saturate(dot(N, H)), Espec.y) * saturate(nl) * arriba * Espec.x;
        o.esp = float4(lc.rgb * (att * s), 0.0);
    }
    return o;
}

// ------------------------------------------------------------------ 2.0b: haz volumetrico (uno por rectangulo)
// lp = origen del haz; lc = color; ld = direccion del haz
float4 PS_Haz(float4 lp : TEXCOORD0, float4 lc : TEXCOORD1, float4 ld : TEXCOORD2, float2 vpos : VPOS) : COLOR
{
    float2 uv = (vpos + 0.5) * float2(Param2.w, Camara.w);
    float d = tex2Dlod(sProf, float4(uv, 0, 0)).r;
    float3 P = Mundo(uv, min(d, 0.99999));
    float3 V = P - Camara.xyz;
    float dEsc = length(V);
    V /= max(dEsc, 1e-4);
    if (d >= 0.999995) dEsc = 1e5;                         // cielo: sin tope
    float largo = HazA.w;
    float3 D = ld.xyz;
    float3 oc = Camara.xyz - (lp.xyz + D * (largo * 0.5));
    float b = dot(oc, V);
    float h = b * b - (dot(oc, oc) - (largo * largo * 0.25 + HazB.x * HazB.x));
    clip(h);
    float sq = sqrt(h);
    float t0 = max(-b - sq, 0.0), t1 = min(dEsc, -b + sq);
    clip(t1 - t0 - 1e-3);
    float paso = (t1 - t0) / 6.0;
    float t = t0 + paso * 0.5;
    float suma = 0.0;
    [loop] for (int k = 0; k < 6; k++)
    {
        float3 q = Camara.xyz + V * t - lp.xyz;
        float a = dot(q, D);
        if (a > 0.0 && a < largo)
        {
            float c = saturate((a / max(length(q), 1e-4) - HazA.y) * HazB.z);
            c = pow(c * c * (3.0 - 2.0 * c), 1.15);
            float rc = min(max(0.12, a * HazA.z), HazB.x);
            float f = saturate((length(q - D * a) - rc * 0.12) / (rc * 0.88));
            f = 1.0 - f * f * (3.0 - 2.0 * f); f *= f;
            float an = a * HazB.w;
            float s0 = saturate(an * 16.66667); s0 = s0 * s0 * (3.0 - 2.0 * s0);
            suma += c * f * s0 * pow(saturate(1.0 - an), 1.35);
        }
        t += paso;
    }
    float fase = saturate(dot(V, D) * 0.5 + 0.5); fase = fase * fase * 0.35 + 0.65;
    float cant = saturate((t1 - t0) * HazB.w * suma * fase * HazB.y * 0.8333334);
    float ign = frac(52.98292 * frac(dot(vpos, float2(0.06711056, 0.00583715))));   // dither IGN (como PS)
    cant = saturate(cant + (ign - 0.5) / 255.0);
    return float4(lc.rgb * cant, 0.0);
}

// ------------------------------------------------------------------ 2.0i: lluvia iluminada (rainLit de SkyGFX)
// Una estela por quad, dibujada tras la composicion de ENB (ONE/ONE). lp.x = profundidad z/w de la estela,
// lp.y = coordenada a lo ancho (-1..1), lp.z = 0..1 a lo largo; lc = color ya iluminado en la CPU.
float4 PS_Lluvia(float4 lp : TEXCOORD0, float4 lc : TEXCOORD1, float4 ld : TEXCOORD2, float2 vpos : VPOS) : COLOR
{
    float2 uv = (vpos + 0.5) * float2(Param2.w, Camara.w);
    float d = tex2Dlod(sProf, float4(uv, 0, 0)).r;
    clip(d - lp.x);                                        // detras de la geometria: no se ve
    float w = saturate(1.0 - abs(lp.y)); w *= w;
    float a = saturate(lp.z * 4.0) * saturate((1.0 - lp.z) * 1.5);
    return float4(lc.rgb * (w * a), 0.0);
}

// ------------------------------------------------------------------ pasada 3: composicion con rodilla suave
float4 PS_Compone(float2 uv : TEXCOORD0) : COLOR
{
    float3 acc = tex2Dlod(sAcc, float4(uv, 0, 0)).rgb;
    float L = max(acc.r, max(acc.g, acc.b));
    float k = Param.x, M = Param.y;
    float Lc = (L <= k) ? L : k + (L - k) / (1.0 + (L - k) / max(M - k, 0.001));
    acc *= Lc / max(L, 1e-5);
    float3 alb = tex2Dlod(sAlb, float4(uv, 0, 0)).rgb;
    alb = lerp(1.0, max(alb, Param.z), Param2.z);
    float3 esp = 0.0;
    if (Espec.x > 0.0)
    {
        esp = tex2Dlod(sEsp, float4(uv, 0, 0)).rgb;
        float Le = max(esp.r, max(esp.g, esp.b));
        esp *= 1.0 / (1.0 + Le / max(M, 0.001));            // tope suave hacia LuzMaxima
    }
    return float4(acc * alb + esp, 0.0);
}
