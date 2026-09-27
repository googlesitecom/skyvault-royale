// ============================================================================
//  SKYVAULT Royale - engine/world/Terrain.h
//  Isla procedural "La Bóveda": 4x4 km, FBM + warp de dominio + pico central.
//  Altura consultable en cualquier punto (colision y generacion usan la misma
//  funcion analitica). Chunks de malla con colores de bioma por vertice.
// ============================================================================
#pragma once

#include "core/Core.h"
#include "math/Math.h"
#include "world/Noise.h"
#include "render/Renderer.h"

namespace sv {

struct PoiDef {
    const char* name;
    vec2 pos;             // mundo
    f32  radius;          // area de influencia (despeje + loot)
    i32  buildingCount;
};

class Terrain {
public:
    static constexpr f32 Size       = 4096.0f;   // lado completo
    static constexpr f32 Half       = Size * 0.5f;
    static constexpr u32 Chunks     = 16;        // 16x16 chunks
    static constexpr f32 ChunkSize  = Size / Chunks;
    static constexpr u32 ChunkVerts = 33;        // 32 quads de 8 m

    [[nodiscard]] bool init(u32 seed);
    void destroy();

    // --- consulta analitica --------------------------------------------------
    [[nodiscard]] f32 height(f32 x, f32 z) const;
    [[nodiscard]] vec3 normal(f32 x, f32 z) const;
    [[nodiscard]] vec3 biomeColor(f32 x, f32 z, f32 h, f32 slope, f32 moisture) const;
    [[nodiscard]] f32 moistureAt(f32 x, f32 z) const { return m_moist.perlin2(x * 0.0008f, z * 0.0008f) * 0.5f + 0.5f; }

    // --- render --------------------------------------------------------------
    void draw(Renderer& r, const Frustum& frustum, const Material& mat) const;

    [[nodiscard]] const std::vector<PoiDef>& pois() const { return m_pois; }
    [[nodiscard]] static const std::vector<PoiDef>& poiTable();  // definiciones estaticas

    [[nodiscard]] bool isLand(vec2 p) const { return height(p.x, p.y) > 2.0f; }

private:
    Noise m_heightNoise, m_warpNoise, m_moist, m_rockNoise;
    u32 m_seed = 1;
    std::vector<Mesh> m_chunkMesh;
    std::vector<AABB> m_chunkBounds;
    std::vector<PoiDef> m_pois;
};

} // namespace sv
