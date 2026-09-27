// ============================================================================
//  SKYVAULT Royale - engine/render/Shaders.h
//  Todos los GLSL del motor, embebidos. Escritos en el subconjunto comun de
//  GLSL 300 es (WebGL2) y 460 core (escritorio): in/out, texture(), sin DSA,
//  sin compute. El prelude (#version + precision) se inyecta en Shader::load.
//  Atributos de vertice (ubicaciones fijas via glBindAttribLocation):
//    0 aPos | 1 aNormal | 2 aUV | 3 aTangent | 4 aColor | 5 aJoint | 6 aWeight
//    7 aInstance (mat4, ocupa 7-10) | 11 aInstanceColor
// ============================================================================
#pragma once

namespace sv { namespace shaders {

// Fragmento comun: constantes, hash/noise/fbm, PBR, sombras PCF por cascadas
inline constexpr const char* CommonFrag = R"GLSL(
#define PI 3.14159265359
out vec4 FragColor;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
float hash13(vec3 p3) {
    p3 = fract(p3 * 0.1031);
    p3 += dot(p3, p3.zyx + 31.32);
    return fract((p3.x + p3.y) * p3.z);
}
float vnoise2(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash12(i), b = hash12(i + vec2(1, 0));
    float c = hash12(i + vec2(0, 1)), d = hash12(i + vec2(1, 1));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
float vnoise3(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = hash13(i), n100 = hash13(i + vec3(1,0,0));
    float n010 = hash13(i + vec3(0,1,0)), n110 = hash13(i + vec3(1,1,0));
    float n001 = hash13(i + vec3(0,0,1)), n101 = hash13(i + vec3(1,0,1));
    float n011 = hash13(i + vec3(0,1,1)), n111 = hash13(i + vec3(1,1,1));
    return mix(mix(mix(n000,n100,f.x), mix(n010,n110,f.x), f.y),
               mix(mix(n001,n101,f.x), mix(n011,n111,f.x), f.y), f.z);
}
float fbm2(vec2 p, int oct) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 6; ++i) {
        if (i >= oct) break;
        v += a * vnoise2(p);
        p = p * 2.03 + vec2(19.7, 7.3);
        a *= 0.5;
    }
    return v;
}
float fbm3(vec3 p, int oct) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 6; ++i) {
        if (i >= oct) break;
        v += a * vnoise3(p);
        p = p * 2.03 + vec3(11.1, 17.7, 7.3);
        a *= 0.5;
    }
    return v;
}

// --- PBR (Cook-Torrance GGX) -------------------------------------------------
float D_GGX(float NoH, float rough) {
    float a = rough * rough;
    float a2 = a * a;
    float d = NoH * NoH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 1e-5);
}
float V_SmithGGX(float NoV, float NoL, float rough) {
    float a2 = rough * rough * rough * rough;
    float lv = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float ll = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / max(lv + ll, 1e-5);
}
vec3 fresnelSchlick(float VoH, vec3 f0) {
    return f0 + (1.0 - f0) * pow(clamp(1.0 - VoH, 0.0, 1.0), 5.0);
}
vec3 shadePBR(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 albedo,
              float metallic, float rough, float shadow) {
    vec3 H = normalize(V + L);
    float NoV = max(dot(N, V), 1e-4);
    float NoL = max(dot(N, L), 0.0);
    float NoH = max(dot(N, H), 0.0);
    float VoH = max(dot(V, H), 0.0);
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    float D = D_GGX(NoH, rough);
    float Vis = V_SmithGGX(NoV, NoL, rough);
    vec3 F = fresnelSchlick(VoH, f0);
    vec3 spec = D * Vis * F;
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    return (kD * albedo / PI + spec) * radiance * NoL * shadow;
}

// --- Sombras CSM (PCF 3x3 manual sobre textura de profundidad) ---------------
uniform sampler2D uShadow0; uniform sampler2D uShadow1;
uniform sampler2D uShadow2; uniform sampler2D uShadow3;
uniform mat4 uLightVP[4];
uniform vec2 uShadowTexel;
uniform float uShadowMix;

float sampleShadow(sampler2D smap, vec3 sp) {
    if (sp.x < 0.002 || sp.x > 0.998 || sp.y < 0.002 || sp.y > 0.998
        || sp.z > 1.0 || sp.z < 0.0) return 1.0;
    float bias = mix(0.0018, 0.0004, sp.z) + 0.0008;
    float sum = 0.0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x) {
            float d = texture(smap, sp.xy + vec2(float(x), float(y)) * uShadowTexel).r;
            sum += (sp.z - bias > d) ? uShadowMix : 1.0;
        }
    return sum / 9.0;
}
float shadowFactor(vec3 worldPos, float viewDepth) {
    int c = viewDepth < 40.0 ? 0 : (viewDepth < 120.0 ? 1 : (viewDepth < 300.0 ? 2 : 3));
    vec4 sp4;
    if (c == 0)      sp4 = uLightVP[0] * vec4(worldPos, 1.0);
    else if (c == 1) sp4 = uLightVP[1] * vec4(worldPos, 1.0);
    else if (c == 2) sp4 = uLightVP[2] * vec4(worldPos, 1.0);
    else             sp4 = uLightVP[3] * vec4(worldPos, 1.0);
    vec3 p = sp4.xyz / sp4.w * 0.5 + 0.5;
    if (c == 0)      return sampleShadow(uShadow0, p);
    else if (c == 1) return sampleShadow(uShadow1, p);
    else if (c == 2) return sampleShadow(uShadow2, p);
    return sampleShadow(uShadow3, p);
}
)GLSL";

// ---------------------------------------------------------------------------
// PBR: geometria del mundo. Defines: INSTANCED, VERTEXCOLOR, WIND
// ---------------------------------------------------------------------------
inline constexpr const char* PbrVert = R"GLSL(
uniform mat4 uViewProj;
uniform mat4 uModel;
uniform float uTime;
uniform vec3  uWindDir;
uniform float uWindStrength;

in vec3 aPos;
in vec3 aNormal;
in vec2 aUV;
in vec4 aTangent;
in vec4 aColor;
#ifdef INSTANCED
in mat4 aInstance;
in vec4 aInstanceColor;
#endif

out vec3 vWorld;
out vec3 vNormal;
out vec2 vUV;
out vec4 vColor;
out vec4 vTangent;
out float vViewDepth;

void main() {
    mat4 model = uModel;
#ifdef INSTANCED
    model = model * aInstance;
#endif
    vec4 wp = model * vec4(aPos, 1.0);
#ifdef WIND
#ifdef WIND_GRASS
    // pasto: respuesta fuerte con rachas y aleteo de alta frecuencia
    float hK = clamp(aPos.y * 1.8, 0.0, 1.0);
    float gust = 0.6 + 0.4 * sin(uTime * 0.9 + (wp.x + wp.z) * 0.035);
    float sway = (sin(uTime * 2.2 + wp.x * 0.5 + wp.z * 0.35) * 0.6
                + sin(uTime * 4.7 + wp.z * 0.9) * 0.25
                + sin(uTime * 7.3 + wp.x * 1.7) * 0.15) * gust;
    wp.xz += uWindDir.xz * sway * uWindStrength * 0.9 * hK * hK;
    wp.y -= abs(sway) * 0.10 * hK;
#else
    // arboles: la copa (alto) se mece, el tronco (bajo) queda firme
    float sway = sin(uTime * 1.7 + wp.x * 0.15 + wp.z * 0.12) * 0.5
               + sin(uTime * 3.1 + wp.z * 0.3) * 0.3;
    float h = clamp(aPos.y * 0.18, 0.0, 1.0);
    wp.xz += uWindDir.xz * sway * uWindStrength * h * h;
#endif
#endif
    vWorld = wp.xyz;
    vNormal = normalize(mat3(model) * aNormal);
    vUV = aUV;
    vTangent = vec4(normalize(mat3(model) * aTangent.xyz), aTangent.w);
    vColor = vec4(1.0);
#ifdef VERTEXCOLOR
    vColor *= aColor;
#endif
#ifdef INSTANCED
    vColor *= aInstanceColor;
#endif
    vec4 cp = uViewProj * wp;
    vViewDepth = cp.w;
    gl_Position = cp;
}
)GLSL";

inline constexpr const char* PbrFrag = R"GLSL(
uniform vec3  uSunDir;
uniform vec3  uSunColor;
uniform vec3  uAmbient;
uniform vec3  uCameraPos;
uniform vec3  uFogColor;
uniform float uFogDensity;
uniform vec4  uTint;
uniform float uEmissive;
uniform float uGhost;
uniform vec3  uGhostColor;
uniform float uMetallic;
uniform float uRough;
#ifdef HAS_BASETEX
uniform sampler2D uBaseTex;
#endif
#ifdef HAS_NORMALTEX
uniform sampler2D uNormalTex;
#endif
#ifdef HAS_MRTEX
uniform sampler2D uMetalRoughTex;
#endif

in vec3 vWorld;
in vec3 vNormal;
in vec2 vUV;
in vec4 vColor;
in vec4 vTangent;
in float vViewDepth;

void main() {
    vec3 albedo = uTint.rgb * vColor.rgb;
#ifdef HAS_BASETEX
    albedo *= texture(uBaseTex, vUV).rgb;
#endif
    float metallic = uMetallic, rough = uRough;
#ifdef HAS_MRTEX
    vec3 mr = texture(uMetalRoughTex, vUV).rgb;
    metallic *= mr.b; rough *= mr.g;
#endif
    vec3 N = normalize(vNormal);
#ifdef HAS_NORMALTEX
    vec3 T = normalize(vTangent.xyz);
    vec3 B = cross(N, T) * vTangent.w;
    mat3 TBN = mat3(T, B, N);
    N = normalize(TBN * (texture(uNormalTex, vUV).xyz * 2.0 - 1.0));
#endif
    vec3 V = normalize(uCameraPos - vWorld);
    N = dot(N, V) < 0.0 ? -N : N;

    float shadow = shadowFactor(vWorld, vViewDepth);
    vec3 color = shadePBR(N, V, uSunDir, uSunColor, albedo, metallic, rough, shadow);
    float hemi = N.y * 0.5 + 0.5;
    color += albedo * mix(uAmbient * 0.55, uAmbient, hemi);
    color += albedo * uEmissive;

    float fog = 1.0 - exp(-vViewDepth * uFogDensity);
    color = mix(color, uFogColor, clamp(fog, 0.0, 1.0));

    if (uGhost > 0.5) {
        float edge = pow(1.0 - max(dot(N, V), 0.0), 1.5);
        color = uGhostColor * (0.35 + edge) + color * 0.1;
    }
    FragColor = vec4(color, uTint.a * vColor.a);
}
)GLSL";

// ---------------------------------------------------------------------------
// Paso de sombras (solo profundidad)
// ---------------------------------------------------------------------------
inline constexpr const char* ShadowVert = R"GLSL(
uniform mat4 uLightVP;
uniform mat4 uModel;
in vec3 aPos;
#ifdef INSTANCED
in mat4 aInstance;
#endif
void main() {
    mat4 model = uModel;
#ifdef INSTANCED
    model = model * aInstance;
#endif
    gl_Position = uLightVP * model * vec4(aPos, 1.0);
}
)GLSL";

inline constexpr const char* ShadowFrag = R"GLSL(
void main() {}
)GLSL";

// ---------------------------------------------------------------------------
// Linearizacion de profundidad a textura de color (evita leer la depth texture
// del FBO activo: feedback loop prohibido). R = profundidad lineal [0,1].
// ---------------------------------------------------------------------------
inline constexpr const char* LinearizeFrag = R"GLSL(
out vec4 FragColor;
uniform sampler2D uDepth;
uniform vec2 uNearFar;
in vec2 vUV;
void main() {
    float z = texture(uDepth, vUV).r * 2.0 - 1.0;
    float n = uNearFar.x, f = uNearFar.y;
    float lin = (2.0 * n * f) / (f + n - z * (f - n));   // metros
    FragColor = vec4(lin / f, 0.0, 0.0, 1.0);            // normalizada a far
}
)GLSL";

// ---------------------------------------------------------------------------
// Cielo: triangulo a pantalla completa con raymarch de nubes volumetricas
// ---------------------------------------------------------------------------
inline constexpr const char* SkyVert = R"GLSL(
uniform mat4 uInvViewProj;
in vec3 aPos;
out vec3 vRayDir;
out vec2 vNdc;
void main() {
    vNdc = aPos.xy;
    vec4 near = uInvViewProj * vec4(aPos.xy, -1.0, 1.0);
    vec4 far  = uInvViewProj * vec4(aPos.xy,  1.0, 1.0);
    vRayDir = far.xyz / far.w - near.xyz / near.w;
    gl_Position = vec4(aPos.xy, 0.999999, 1.0);
}
)GLSL";

inline constexpr const char* SkyFrag = R"GLSL(
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uZenith;
uniform vec3 uHorizon;
uniform vec3 uCameraPos;
uniform float uTime;
uniform sampler2D uSceneDepth;   // profundidad LINEAL (pase de linearizacion)
uniform int  uCloudSteps;
uniform vec2 uRes;
uniform float uCloudCover;

in vec3 vRayDir;
in vec2 vNdc;

float cloudDensity(vec3 p) {
    vec3 q = p * 0.0008 + vec3(uTime * 0.006, 0.0, uTime * 0.004);
    float base = fbm3(q, 4);
    float det = fbm3(q * 3.7 + 7.0, 3);
    float d = base * 0.72 + det * 0.28;
    return smoothstep(1.0 - uCloudCover, 1.05 - uCloudCover * 0.55, d);
}

void main() {
    vec3 rd = normalize(vRayDir);
    float h = clamp(rd.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 sky = mix(uHorizon, uZenith, pow(h, 0.62));
    float sunDot = max(dot(rd, uSunDir), 0.0);
    sky += uSunColor * (pow(sunDot, 640.0) * 18.0 + pow(sunDot, 24.0) * 0.42);

    float linDepth = texture(uSceneDepth, vNdc * 0.5 + 0.5).r;
    if (linDepth > 0.9995 && rd.y > 0.015 && uCloudSteps > 0) {
        float t0 = (350.0 - uCameraPos.y) / rd.y;
        float t1 = (1500.0 - uCameraPos.y) / rd.y;
        t0 = max(t0, 0.0);
        if (t1 > t0) {
            float steps = float(uCloudSteps);
            float dt = (t1 - t0) / steps;
            float jitter = hash12(vNdc * uRes + fract(uTime) * 371.0);
            float t = t0 + dt * jitter;
            float trans = 1.0;
            vec3 lit = vec3(0.0);
            for (int i = 0; i < 32; ++i) {
                if (i >= uCloudSteps || trans < 0.03) break;
                vec3 p = uCameraPos + rd * t;
                float d = cloudDensity(p);
                if (d > 0.01) {
                    float ld = cloudDensity(p + uSunDir * 60.0) * 0.6
                             + cloudDensity(p + uSunDir * 140.0) * 0.4;
                    float light = exp(-ld * 3.4) * 1.35 + 0.18;
                    float a = 1.0 - exp(-d * dt * 0.011);
                    lit += uSunColor * light * a * trans;
                    trans *= (1.0 - a);
                }
                t += dt;
            }
            sky = sky * trans + lit;
        }
    }
    FragColor = vec4(sky, 1.0);
}
)GLSL";

// ---------------------------------------------------------------------------
// Agua: plano con oleaje, espejo real + profundidad de orilla
// ---------------------------------------------------------------------------
inline constexpr const char* WaterVert = R"GLSL(
uniform mat4 uViewProj;
uniform mat4 uModel;
uniform float uTime;
in vec3 aPos;
out vec3 vWorld;
out vec2 vNdcRaw;
out float vWaterDepth;
void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    wp.y += sin(wp.x * 0.06 + uTime * 1.1) * 0.14 + sin(wp.z * 0.09 + uTime * 1.7) * 0.11;
    vWorld = wp.xyz;
    vec4 cp = uViewProj * wp;
    vNdcRaw = cp.xy / cp.w;
    gl_Position = cp;
    vWaterDepth = cp.w;
}
)GLSL";

inline constexpr const char* WaterFrag = R"GLSL(
uniform vec3 uCameraPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uFogColor;
uniform vec3 uZenith;
uniform vec3 uHorizon;
uniform float uFogDensity;
uniform float uTime;
uniform sampler2D uSceneDepth;   // lineal normalizada a far
uniform sampler2D uReflection;
uniform vec2 uNearFar;
uniform vec2 uRes;

in vec3 vWorld;
in vec2 vNdcRaw;
in float vWaterDepth;

float waterHeight(vec2 p) {
    float t = uTime;
    float h = sin(dot(p, vec2(0.060, 0.020)) + t * 1.05) * 0.14
            + sin(dot(p, vec2(-0.045, 0.085)) + t * 1.60) * 0.10
            + sin(dot(p, vec2(0.140, -0.110)) + t * 2.30) * 0.05;
    return h;
}
vec3 waterNormal(vec2 p, float distFade) {
    float e = 0.9;
    float hx = waterHeight(p + vec2(e, 0)) - waterHeight(p - vec2(e, 0));
    float hz = waterHeight(p + vec2(0, e)) - waterHeight(p - vec2(0, e));
    float t = uTime;
    // 3 capas de rizado suaves (escala creciente, amplitud decreciente).
    // distFade apaga el detalle alto-frecuencia a lo lejos (evita aliasing
    // en mosaico cuando cada celda de ruido cae en pocos pixeles)
    float r1 = vnoise2(p * 0.28 + vec2(t * 0.50,  t * 0.28)) - 0.5;
    float r2 = vnoise2(p * 0.51 + vec2(-t * 0.38, t * 0.55)) - 0.5;
    float r3 = vnoise2(p * 0.92 + vec2(t * 0.70, -t * 0.60)) - 0.5;
    float r4 = vnoise2(p * 1.80 + vec2(-t * 0.90, t * 1.10)) - 0.5;
    vec2 grad = vec2(hx, hz) * 0.30 * mix(0.35, 1.0, distFade)
              + vec2(r1 * 0.24 + r2 * 0.16 + r3 * 0.09,
                     r2 * 0.22 + r3 * 0.13 + r1 * 0.11) * mix(0.3, 1.0, distFade)
              + vec2(r4 * 0.10, r4 * 0.08) * distFade * distFade;
    return normalize(vec3(-grad.x, 1.0, -grad.y));
}

void main() {
    vec2 suv = vNdcRaw * 0.5 + 0.5;
    // desvanecimiento de detalle con la distancia (anti-mosaico)
    float distFade = exp(-vWaterDepth * 0.0035);
    vec3 N = waterNormal(vWorld.xz, distFade);
    vec3 V = normalize(uCameraPos - vWorld);
    // fresnel fisico (Schlick, F0=0.02)
    float fres = 0.02 + 0.98 * pow(clamp(1.0 - max(dot(N, V), 0.0), 0.0, 1.0), 5.0);

    // reflexion: espejo real distorsionado por las olas + cielo como base
    vec2 ruv = clamp(suv + N.xz * 0.055, vec2(0.001), vec2(0.999));
    vec3 refl = texture(uReflection, ruv).rgb;
    vec3 rsky = mix(uHorizon, uZenith, clamp(N.y, 0.0, 1.0));
    refl = mix(rsky, refl, 0.85);

    // absorcion: color del agua por profundidad sobre el lecho
    float sceneLin = texture(uSceneDepth, suv).r * uNearFar.y;   // metros
    float under = max(sceneLin - vWaterDepth, 0.0);              // metros de agua
    vec3 shallowCol = vec3(0.16, 0.54, 0.52);
    vec3 deep       = vec3(0.012, 0.085, 0.155);
    vec3 waterCol = mix(shallowCol, deep, 1.0 - exp(-under / 5.5));

    // combinacion specular/difusa del cuerpo de agua
    vec3 color = mix(waterCol, refl, clamp(fres * 1.05, 0.0, 1.0));

    // brillo solar: lobo estrecho (destello) + lobo ancho (resplandor)
    vec3 H = normalize(V + uSunDir);
    float NoH = max(dot(N, H), 0.0);
    color += uSunColor * (pow(NoH, 380.0) * 2.6 + pow(NoH, 48.0) * 0.45);

    // espuma en la orilla: bandas animadas + rizado, y crestas espumosas
    float shore = 1.0 - clamp(under / 2.6, 0.0, 1.0);
    float bands = 0.5 + 0.5 * sin(under * 3.0 - uTime * 2.2
                                  + vnoise2(vWorld.xz * 0.7) * 5.0);
    float foamN = smoothstep(0.42, 0.78, vnoise2(vWorld.xz * 0.9 + uTime * 0.5));
    float foam = shore * (0.55 + 0.45 * bands) * foamN;
    float crest = smoothstep(0.68, 0.86,
                             vnoise2(vWorld.xz * 0.45 + vec2(uTime * 0.4, -uTime * 0.3)) * 0.5
                           + vnoise2(vWorld.xz * 1.1  - vec2(uTime * 0.6,  uTime * 0.2)) * 0.5);
    foam = clamp(foam + crest * 0.35, 0.0, 0.9);
    color = mix(color, vec3(0.96, 0.98, 1.0), foam);

    float fog = 1.0 - exp(-vWaterDepth * uFogDensity);
    color = mix(color, uFogColor, clamp(fog, 0.0, 1.0));
    FragColor = vec4(color, 1.0);
}
)GLSL";

// ---------------------------------------------------------------------------
// Muro de tormenta (aditivo translucido)
// ---------------------------------------------------------------------------
inline constexpr const char* StormVert = R"GLSL(
uniform mat4 uViewProj;
uniform mat4 uModel;
in vec3 aPos;
in vec2 aUV;
out vec2 vUV;
out vec3 vWorld;
void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    vWorld = wp.xyz;
    vUV = aUV;
    gl_Position = uViewProj * wp;
}
)GLSL";

inline constexpr const char* StormFrag = R"GLSL(
uniform vec3 uCameraPos;
uniform float uTime;
uniform vec3 uStormColor;
uniform vec3 uFogColor;
in vec2 vUV;
in vec3 vWorld;
void main() {
    float n = fbm2(vec2(vWorld.xz * 0.012 + vec2(0.0, uTime * 0.16)), 4);
    float n2 = fbm2(vec2(vWorld.xz * 0.03 - vec2(uTime * 0.1, 0.0)), 3);
    float vertical = pow(1.0 - vUV.y, 1.6);
    float bands = smoothstep(0.35, 0.75, n * 0.65 + n2 * 0.35);
    float a = vertical * (0.30 + bands * 0.60);
    vec3 col = uStormColor * (0.9 + bands * 1.4);
    // relampagos ocasionales (celdas de ruido por tiempo)
    float cell = hash13(vec3(floor(vWorld.x * 0.015), floor(uTime * 2.0), floor(vWorld.z * 0.015)));
    float flash = step(0.9965, cell) * vertical;
    col += vec3(0.85, 0.75, 1.0) * flash * 1.6;
    a = min(a + flash * 0.35, 1.0);
    float dist = length(uCameraPos - vWorld);
    float fog = 1.0 - exp(-dist * 0.0012);
    col = mix(col, uFogColor, clamp(fog, 0.0, 0.6));
    FragColor = vec4(col, a);
}
)GLSL";

// ---------------------------------------------------------------------------
// Particulas: quads instanciados (billboard o estirados a la velocidad)
// ---------------------------------------------------------------------------
inline constexpr const char* ParticleVert = R"GLSL(
uniform mat4 uViewProj;
uniform vec3 uCamRight, uCamUp;
in vec3 aPos;              // esquina del quad base (-0.5..0.5)
in vec2 aUV;
in vec4 aPosSize;          // [inst] centro.xyz, tamano
in vec4 aColor;            // [inst] rgba
in vec4 aVelStretch;       // [inst] velocidad.xyz, estiramiento
out vec2 vUV;
out vec4 vCol;
void main() {
    vec3 ax = uCamRight, ay = uCamUp;
    float stretch = aVelStretch.w;
    if (stretch > 0.001) {
        ax = normalize(aVelStretch.xyz + vec3(0.0, 0.0001, 0.0));
        vec3 c = cross(ax, uCamRight);
        float l2 = length(c);
        ay = l2 > 0.01 ? c / l2 : uCamUp;
    }
    vec3 p = aPosSize.xyz
           + ax * (aPos.x * aPosSize.w * (1.0 + stretch * 10.0))
           + ay * (aPos.y * aPosSize.w);
    vUV = aUV;
    vCol = aColor;
    gl_Position = uViewProj * vec4(p, 1.0);
}
)GLSL";

inline constexpr const char* ParticleFrag = R"GLSL(
out vec4 FragColor;
in vec2 vUV;
in vec4 vCol;
void main() {
    vec2 c = vUV * 2.0 - 1.0;
    float d = dot(c, c);
    if (d > 1.0) discard;
    float soft = exp(-d * 3.0);
    FragColor = vec4(vCol.rgb, vCol.a * soft);
}
)GLSL";

// ---------------------------------------------------------------------------
// UI 2D: quads con color por vertice + textura opcional (fuente/minimapa)
// ---------------------------------------------------------------------------
inline constexpr const char* UiVert = R"GLSL(
uniform vec2 uRes;
in vec2 aPos;      // x,y en pixeles
in vec2 aUV;
in vec4 aColor;
out vec2 vUV;
out vec4 vCol;
void main() {
    vec2 ndc = vec2(aPos.x / uRes.x * 2.0 - 1.0, 1.0 - aPos.y / uRes.y * 2.0);
    vUV = aUV;
    vCol = aColor;
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)GLSL";

inline constexpr const char* UiFrag = R"GLSL(
out vec4 FragColor;
uniform sampler2D uTex;
uniform float uUseTex;
in vec2 vUV;
in vec4 vCol;
void main() {
    vec4 c = vCol;
    if (uUseTex > 0.5) {
        vec4 t = texture(uTex, vUV);
        c *= vec4(t.rgb, t.a);
    }
    if (c.a < 0.004) discard;
    FragColor = c;
}
)GLSL";

// ---------------------------------------------------------------------------
// Post-proceso
// ---------------------------------------------------------------------------
inline constexpr const char* PostVert = R"GLSL(
in vec3 aPos;
out vec2 vUV;
void main() {
    vUV = aPos.xy * 0.5 + 0.5;
    gl_Position = vec4(aPos.xy, 0.0, 1.0);
}
)GLSL";

inline constexpr const char* BloomThresholdFrag = R"GLSL(
out vec4 FragColor;
uniform sampler2D uScene;
uniform float uThreshold;
in vec2 vUV;
void main() {
    vec3 c = texture(uScene, vUV).rgb;
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    float k = max(l - uThreshold, 0.0) / max(l, 1e-4);
    FragColor = vec4(c * k, 1.0);
}
)GLSL";

inline constexpr const char* BlurFrag = R"GLSL(
out vec4 FragColor;
uniform sampler2D uSrc;
uniform vec2 uDir;
in vec2 vUV;
void main() {
    float w[5] = float[5](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec3 c = texture(uSrc, vUV).rgb * w[0];
    for (int i = 1; i < 5; ++i) {
        c += texture(uSrc, vUV + uDir * float(i)).rgb * w[i];
        c += texture(uSrc, vUV - uDir * float(i)).rgb * w[i];
    }
    FragColor = vec4(c, 1.0);
}
)GLSL";

inline constexpr const char* CompositeFrag = R"GLSL(
out vec4 FragColor;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform sampler2D uSceneDepth;   // lineal normalizada
uniform vec2  uSunNdc;
uniform float uGodRays;
uniform float uBloomStrength;
uniform float uExposure;
in vec2 vUV;

vec3 acesFilm(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 col = texture(uScene, vUV).rgb;
    col += texture(uBloom, vUV).rgb * uBloomStrength;
    if (uGodRays > 0.001 && uSunNdc.x > -2.0) {
        vec2 toSun = uSunNdc * 0.5 + 0.5 - vUV;
        float len = length(toSun);
        if (len > 0.001) {
            vec2 stp = toSun / 20.0;
            vec2 p = vUV;
            float acc = 0.0;
            for (int i = 0; i < 20; ++i) {
                p += stp;
                float d = texture(uSceneDepth, p).r;
                acc += (d > 0.9995) ? 1.0 : 0.0;
            }
            acc /= 20.0;
            float fall = exp(-len * 1.8);
            col += vec3(1.0, 0.92, 0.78) * acc * fall * uGodRays;
        }
    }
    col *= uExposure;
    col = acesFilm(col);
    col = pow(col, vec3(1.0 / 2.2));
    FragColor = vec4(col, 1.0);
}
)GLSL";

inline constexpr const char* FinalFrag = R"GLSL(
out vec4 FragColor;
uniform sampler2D uSrc;
uniform vec2 uRes;
uniform float uVignette;
uniform float uGrain;
uniform float uChroma;
uniform float uTime;
in vec2 vUV;

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main() {
    vec2 texel = 1.0 / uRes;
    vec3 rgbM  = texture(uSrc, vUV).rgb;
    float lM = luma(rgbM);
    float lNW = luma(texture(uSrc, vUV + vec2(-1, -1) * texel).rgb);
    float lNE = luma(texture(uSrc, vUV + vec2( 1, -1) * texel).rgb);
    float lSW = luma(texture(uSrc, vUV + vec2(-1,  1) * texel).rgb);
    float lSE = luma(texture(uSrc, vUV + vec2( 1,  1) * texel).rgb);
    float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
    float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));
    vec2 dir = vec2(-((lNW + lNE) - (lSW + lSE)), ((lNW + lSW) - (lNE + lSE)));
    float dirReduce = max((lNW + lNE + lSW + lSE) * 0.25 * 0.2, 1.0 / 128.0);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-8.0), vec2(8.0)) * texel;
    vec3 rgbA = 0.5 * (texture(uSrc, vUV + dir * (1.0 / 3.0 - 0.5)).rgb +
                       texture(uSrc, vUV + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (texture(uSrc, vUV - dir * 0.5).rgb +
                                     texture(uSrc, vUV + dir * 0.5).rgb);
    float lB = luma(rgbB);
    vec3 res = ((lB < lMin) || (lB > lMax)) ? rgbA : rgbB;

    if (uChroma > 0.001) {
        vec2 off = (vUV - 0.5) * uChroma * 0.004;
        res.r = texture(uSrc, vUV + off).r;
        res.b = texture(uSrc, vUV - off).b;
    }
    vec2 vd = vUV - 0.5;
    float vig = 1.0 - uVignette * dot(vd, vd) * 1.9;
    res *= clamp(vig, 0.0, 1.0);
    float g = fract(sin(dot(vUV * uRes + fract(uTime) * 977.0, vec2(12.9898, 78.233))) * 43758.5453);
    res += (g - 0.5) * uGrain;

    FragColor = vec4(res, 1.0);
}
)GLSL";

}} // namespace sv::shaders
