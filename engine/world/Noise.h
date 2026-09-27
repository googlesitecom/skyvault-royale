// ============================================================================
//  SkyVault Engine - Ruido procedural: Perlin 2D/3D, FBM, ridged, domain warp
//  Base de la generacion de la isla, biomas, nubes y texturas procedurales.
// ============================================================================
#pragma once

#include "core/Core.h"

namespace sv {

class Noise {
public:
    explicit Noise(u32 seedValue = 1337u);

    [[nodiscard]] f32 perlin2(f32 x, f32 y) const;              // ~[-1, 1]
    [[nodiscard]] f32 perlin3(f32 x, f32 y, f32 z) const;       // ~[-1, 1]
    [[nodiscard]] f32 fbm2(f32 x, f32 y, i32 octaves, f32 lacunarity = 2.0f, f32 gain = 0.5f) const;
    [[nodiscard]] f32 fbm3(f32 x, f32 y, f32 z, i32 octaves, f32 lacunarity = 2.0f, f32 gain = 0.5f) const;
    [[nodiscard]] f32 ridged2(f32 x, f32 y, i32 octaves) const; // [0, 1]
    [[nodiscard]] f32 warpedFbm2(f32 x, f32 y, f32 strength, i32 octaves) const;

    // Hash rapido para dispersion determinista de vegetacion / loot
    [[nodiscard]] static u32 hashU32(u32 x);
    [[nodiscard]] static f32 hashF32(i32 x, i32 y, u32 seed);
    [[nodiscard]] static f32 hashF32_3(i32 x, i32 y, i32 z, u32 seed);

private:
    u32 perm[512];
    [[nodiscard]] u32 p(u32 i) const { return perm[i & 511u]; }
    [[nodiscard]] static f32 grad2(u32 hash, f32 fx, f32 fy);
    [[nodiscard]] static f32 grad3(u32 hash, f32 fx, f32 fy, f32 fz);
    [[nodiscard]] static f32 fade(f32 t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
};

} // namespace sv
