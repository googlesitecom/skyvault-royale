// ============================================================================
//  SkyVault Engine - implementacion de ruido (Perlin clasico + variantes)
// ============================================================================
#include "world/Noise.h"
#include "math/Math.h"
#include <cmath>

namespace sv {

Noise::Noise(u32 seedValue) {
    // Tabla de permutacion mezclada con PCG32 a partir de la semilla
    Random rng;
    rng.seed(seedValue);
    u32 table[256];
    for (u32 i = 0; i < 256; ++i) table[i] = i;
    for (u32 i = 255; i > 0; --i) {
        const u32 j = rng.rangeU32(i + 1);
        const u32 tmp = table[i];
        table[i] = table[j];
        table[j] = tmp;
    }
    for (u32 i = 0; i < 256; ++i) perm[i] = table[i];
    for (u32 i = 0; i < 256; ++i) perm[256 + i] = table[i];
}

f32 Noise::grad2(u32 hash, f32 fx, f32 fy) {
    // 8 direcciones de gradiente, todas unitarias: las diagonales se escalan por
    // 1/sqrt(2) para que el rango teorico quede en +/-sqrt(2)/2 y la normalizacion
    // posterior (x1.4142) produzca +/-1.0 exactos.
    static constexpr f32 INV_SQRT2 = 0.70710678118654752f;
    switch (hash & 7u) {
        case 0: return ( fx + fy) * INV_SQRT2;
        case 1: return (-fx + fy) * INV_SQRT2;
        case 2: return ( fx - fy) * INV_SQRT2;
        case 3: return (-fx - fy) * INV_SQRT2;
        case 4: return  fx;
        case 5: return -fx;
        case 6: return  fy;
        default:return -fy;
    }
}

f32 Noise::grad3(u32 hash, f32 fx, f32 fy, f32 fz) {
    switch (hash & 15u) {
        case 0:  return  fx + fy;
        case 1:  return -fx + fy;
        case 2:  return  fx - fy;
        case 3:  return -fx - fy;
        case 4:  return  fx + fz;
        case 5:  return -fx + fz;
        case 6:  return  fx - fz;
        case 7:  return -fx - fz;
        case 8:  return  fy + fz;
        case 9:  return -fy + fz;
        case 10: return  fy - fz;
        case 11: return -fy - fz;
        case 12: return  fx + fy;
        case 13: return -fy + fz;
        case 14: return -fx + fy;
        default: return -fy - fz;
    }
}

f32 Noise::perlin2(f32 x, f32 y) const {
    const i32 xi = static_cast<i32>(std::floor(x));
    const i32 yi = static_cast<i32>(std::floor(y));
    const f32 fx = x - static_cast<f32>(xi);
    const f32 fy = y - static_cast<f32>(yi);
    const f32 u = fade(fx);
    const f32 v = fade(fy);

    const u32 A = p(static_cast<u32>(xi)) + static_cast<u32>(yi);
    const u32 B = p(static_cast<u32>(xi) + 1) + static_cast<u32>(yi);

    const f32 g00 = grad2(p(A), fx, fy);
    const f32 g10 = grad2(p(B), fx - 1.0f, fy);
    const f32 g01 = grad2(p(A + 1), fx, fy - 1.0f);
    const f32 g11 = grad2(p(B + 1), fx - 1.0f, fy - 1.0f);

    const f32 lx0 = lerpT(g00, g10, u);
    const f32 lx1 = lerpT(g01, g11, u);
    return lerpT(lx0, lx1, v) * 1.4142f; // normaliza amplitud aprox
}

f32 Noise::perlin3(f32 x, f32 y, f32 z) const {
    const i32 xi = static_cast<i32>(std::floor(x));
    const i32 yi = static_cast<i32>(std::floor(y));
    const i32 zi = static_cast<i32>(std::floor(z));
    const f32 fx = x - static_cast<f32>(xi);
    const f32 fy = y - static_cast<f32>(yi);
    const f32 fz = z - static_cast<f32>(zi);
    const f32 u = fade(fx), v = fade(fy), w = fade(fz);

    const u32 AA = p(p(static_cast<u32>(xi)) + static_cast<u32>(yi)) + static_cast<u32>(zi);
    const u32 AB = p(p(static_cast<u32>(xi)) + static_cast<u32>(yi) + 1) + static_cast<u32>(zi);
    const u32 BA = p(p(static_cast<u32>(xi) + 1) + static_cast<u32>(yi)) + static_cast<u32>(zi);
    const u32 BB = p(p(static_cast<u32>(xi) + 1) + static_cast<u32>(yi) + 1) + static_cast<u32>(zi);

    const f32 x00 = lerpT(grad3(p(AA), fx, fy, fz),     grad3(p(BA), fx - 1.0f, fy, fz), u);
    const f32 x10 = lerpT(grad3(p(AB), fx, fy - 1.0f, fz), grad3(p(BB), fx - 1.0f, fy - 1.0f, fz), u);
    const f32 x01 = lerpT(grad3(p(AA + 1), fx, fy, fz - 1.0f), grad3(p(BA + 1), fx - 1.0f, fy, fz - 1.0f), u);
    const f32 x11 = lerpT(grad3(p(AB + 1), fx, fy - 1.0f, fz - 1.0f), grad3(p(BB + 1), fx - 1.0f, fy - 1.0f, fz - 1.0f), u);

    const f32 y0 = lerpT(x00, x10, v);
    const f32 y1 = lerpT(x01, x11, v);
    return lerpT(y0, y1, w);
}

f32 Noise::fbm2(f32 x, f32 y, i32 octaves, f32 lacunarity, f32 gain) const {
    f32 sum = 0.0f, amp = 1.0f, norm = 0.0f;
    for (i32 i = 0; i < octaves; ++i) {
        sum += perlin2(x, y) * amp;
        norm += amp;
        amp *= gain;
        x *= lacunarity;
        y *= lacunarity;
    }
    return norm > 0.0f ? sum / norm : 0.0f;
}

f32 Noise::fbm3(f32 x, f32 y, f32 z, i32 octaves, f32 lacunarity, f32 gain) const {
    f32 sum = 0.0f, amp = 1.0f, norm = 0.0f;
    for (i32 i = 0; i < octaves; ++i) {
        sum += perlin3(x, y, z) * amp;
        norm += amp;
        amp *= gain;
        x *= lacunarity; y *= lacunarity; z *= lacunarity;
    }
    return norm > 0.0f ? sum / norm : 0.0f;
}

f32 Noise::ridged2(f32 x, f32 y, i32 octaves) const {
    f32 sum = 0.0f, amp = 1.0f, norm = 0.0f;
    for (i32 i = 0; i < octaves; ++i) {
        const f32 n = 1.0f - std::fabs(perlin2(x, y));
        sum += n * n * amp;
        norm += amp;
        amp *= 0.5f;
        x *= 2.0f;
        y *= 2.0f;
    }
    return norm > 0.0f ? sum / norm : 0.0f;
}

f32 Noise::warpedFbm2(f32 x, f32 y, f32 strength, i32 octaves) const {
    const f32 qx = fbm2(x + 5.2f, y + 1.3f, 4);
    const f32 qy = fbm2(x + 1.7f, y + 9.2f, 4);
    return fbm2(x + strength * qx, y + strength * qy, octaves);
}

u32 Noise::hashU32(u32 x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

f32 Noise::hashF32(i32 x, i32 y, u32 seed) {
    u32 h = static_cast<u32>(x) * 374761393u + static_cast<u32>(y) * 668265263u + seed * 2246822519u;
    h = hashU32(h);
    return static_cast<f32>(h >> 8) * (1.0f / 16777216.0f);
}

f32 Noise::hashF32_3(i32 x, i32 y, i32 z, u32 seed) {
    u32 h = static_cast<u32>(x) * 374761393u + static_cast<u32>(y) * 668265263u +
            static_cast<u32>(z) * 2147483647u + seed * 2246822519u;
    h = hashU32(h);
    return static_cast<f32>(h >> 8) * (1.0f / 16777216.0f);
}

} // namespace sv
