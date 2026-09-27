// ============================================================================
//  SKYVAULT Royale - engine/world/Terrain.cpp
// ============================================================================
#include "world/Terrain.h"
#include <algorithm>

namespace sv {

// ---------------------------------------------------------------------------
// 12 POIs de La Bóveda (nombres originales en español)
// ---------------------------------------------------------------------------
const std::vector<PoiDef>& Terrain::poiTable() {
    static const std::vector<PoiDef> table = {
        { "La Bóveda",      {   0,    0}, 260.0f, 3 },  // pico volcanico central
        { "Puerto Pinchazo",{ 980, 1120}, 150.0f, 4 },
        { "Aldea Cobalto",  {-1050,  620}, 140.0f, 5 },
        { "Molino Rojo",    { 720, -980}, 130.0f, 3 },
        { "Fábrica Tormenta",{-1250, -830},160.0f, 4 },
        { "Bahía Cálida",   { 1500,  -60}, 140.0f, 3 },
        { "Pueblo Cima",    { -600, 1450}, 120.0f, 5 },
        { "Selva Umbría",   { 1350, 1350}, 150.0f, 2 },
        { "Camping Faro",   {-1550,  220}, 120.0f, 4 },
        { "Cañón Eco",      {  350,-1550}, 130.0f, 2 },
        { "Laguna Susurro", { -950, -1450},120.0f, 3 },
        { "Torres Gemelas", {  60,   730}, 110.0f, 6 },
    };
    return table;
}

// ---------------------------------------------------------------------------
bool Terrain::init(u32 seed) {
    m_seed = seed;
    m_heightNoise = Noise(seed);
    m_warpNoise   = Noise(seed + 17);
    m_moist       = Noise(seed + 43);
    m_rockNoise   = Noise(seed + 91);
    m_pois = poiTable();

    // mallas por chunk
    m_chunkMesh.clear();
    m_chunkBounds.clear();
    m_chunkMesh.reserve(Chunks * Chunks);
    const f32 step = ChunkSize / (f32)(ChunkVerts - 1);

    for (u32 cz = 0; cz < Chunks; ++cz)
        for (u32 cx = 0; cx < Chunks; ++cx) {
            const f32 x0 = -Half + (f32)cx * ChunkSize;
            const f32 z0 = -Half + (f32)cz * ChunkSize;

            std::vector<Vertex> verts;
            std::vector<u32> idx;
            verts.reserve(ChunkVerts * ChunkVerts);
            AABB bb{vec3(1e9f), vec3(-1e9f)};

            for (u32 vz = 0; vz < ChunkVerts; ++vz)
                for (u32 vx = 0; vx < ChunkVerts; ++vx) {
                    const f32 x = x0 + (f32)vx * step;
                    const f32 z = z0 + (f32)vz * step;
                    const f32 h = height(x, z);
                    const vec3 n = normal(x, z);
                    const f32 slope = 1.0f - n.y;
                    const f32 moist = moistureAt(x, z);
                    const vec3 col = biomeColor(x, z, h, slope, moist);
                    Vertex v;
                    v.pos = vec3(x, h, z);
                    v.normal = n;
                    v.uv = vec2(x * 0.05f, z * 0.05f);
                    v.tangent = vec4(1, 0, 0, 1);
                    v.color = vec4(col, 1);
                    v.joint[0] = 0; v.weight[0] = 255;
                    verts.push_back(v);
                    bb.bmin = glm::min(bb.bmin, v.pos);
                    bb.bmax = glm::max(bb.bmax, v.pos);
                }

            for (u32 vz = 0; vz < ChunkVerts - 1; ++vz)
                for (u32 vx = 0; vx < ChunkVerts - 1; ++vx) {
                    const u32 a = vz * ChunkVerts + vx;
                    // CCW visto desde arriba (normal +Y)
                    idx.push_back(a); idx.push_back(a + ChunkVerts + 1); idx.push_back(a + 1);
                    idx.push_back(a); idx.push_back(a + ChunkVerts); idx.push_back(a + ChunkVerts + 1);
                }

            Renderer& r = Renderer::get();
            m_chunkMesh.push_back(r.createMesh(verts.data(), (u32)verts.size(),
                                               idx.data(), (u32)idx.size()));
            m_chunkBounds.push_back(bb);
        }
    SV_LOG_INFO("world", "Terreno: %zu chunks (%dkm2)", m_chunkMesh.size(), (i32)(Size * Size / 1000000.0f));
    return true;
}

void Terrain::destroy() {
    for (auto& m : m_chunkMesh) m.destroy();
    m_chunkMesh.clear();
    m_chunkBounds.clear();
}

// ---------------------------------------------------------------------------
// Altura analitica: isla FBM con falloff radial + pico central "La Bóveda"
// ---------------------------------------------------------------------------
f32 Terrain::height(f32 x, f32 z) const {
    // warp de dominio para costas irregulares
    const f32 wx = m_warpNoise.fbm2(x * 0.0012f, z * 0.0012f, 3) - 0.5f;
    const f32 wz = m_warpNoise.fbm2(x * 0.0012f + 5.2f, z * 0.0012f + 1.3f, 3) - 0.5f;
    const f32 px = x + wx * 220.0f, pz = z + wz * 220.0f;

    // falloff radial (isla)
    const f32 d = std::sqrt(px * px + pz * pz);
    const f32 t = clamp01(1.0f - d / (Half * 0.92f));
    const f32 falloff = t * t * (3.0f - 2.0f * t);   // smoothstep

    // colinas base FBM
    f32 h = (m_heightNoise.fbm2(px * 0.00085f, pz * 0.00085f, 5) - 0.30f) * 300.0f;

    // crestas (montañas en el cuadrante noroeste)
    const f32 ridge = m_heightNoise.ridged2(px * 0.0006f + 3.7f, pz * 0.0006f - 2.1f, 4);
    h += ridge * 150.0f * clamp01((px - pz + 1400.0f) / 2800.0f);

    // pico central: el volcan "La Bóveda"
    const f32 dc = std::sqrt(x * x + z * z);
    const f32 volcano = smoothstep(900.0f, 120.0f, dc);
    h += volcano * 330.0f;
    // crater hundido en el centro exacto
    h -= smoothstep(150.0f, 40.0f, dc) * 95.0f;

    h *= falloff;
    h -= (1.0f - falloff) * 55.0f;    // el borde se hunde bajo el mar

    // aplanar POIs (mesetas jugables)
    for (const auto& poi : m_pois) {
        const f32 dp = distance(vec2(x, z), poi.pos);
        if (dp < poi.radius * 1.6f) {
            const f32 k = smoothstep(poi.radius * 1.6f, poi.radius * 0.5f, dp);
            // altura objetivo del POI: meseta de PRADERA (sobre el bioma de
            // arena, h 26-52 m) para que los pueblos tengan hierba alrededor
            const f32 target = 28.0f + m_heightNoise.perlin2(poi.pos.x * 0.001f, poi.pos.y * 0.001f) * 24.0f;
            h = lerpT(h, target, k * 0.85f);
        }
    }
    return h;
}

vec3 Terrain::normal(f32 x, f32 z) const {
    const f32 e = 2.0f;
    const f32 hl = height(x - e, z), hr = height(x + e, z);
    const f32 hd = height(x, z - e), hu = height(x, z + e);
    return normalize(vec3(hl - hr, 2.0f * e, hd - hu));
}

vec3 Terrain::biomeColor(f32 x, f32 z, f32 h, f32 slope, f32 moist) const {
    // paleta de La Bóveda (version viva tipo battle royale)
    const vec3 deepSand(0.78f, 0.71f, 0.50f);
    const vec3 sand    (0.89f, 0.82f, 0.59f);
    const vec3 grass   (0.33f, 0.57f, 0.20f);
    const vec3 grassDry(0.58f, 0.60f, 0.28f);
    const vec3 swamp   (0.32f, 0.45f, 0.28f);
    const vec3 rock    (0.48f, 0.46f, 0.44f);
    const vec3 rockDark(0.36f, 0.34f, 0.33f);
    const vec3 snow    (0.93f, 0.95f, 0.97f);
    const vec3 lavaRock(0.24f, 0.19f, 0.18f);

    vec3 col;
    if (h < 3.0f)       col = deepSand;
    else if (h < 13.0f) col = mix(deepSand, sand, clamp01((h - 3.0f) / 10.0f));
    else if (h < 24.0f) col = mix(sand, mix(grass, grassDry, moist), clamp01((h - 13.0f) / 11.0f));
    else if (h < 260.0f) {
        col = mix(grass, grassDry, clamp01(moist * 1.2f));
        // humedad alta y bajo: pantano
        const f32 swampK = clamp01((26.0f - h) / 10.0f) * clamp01((moist - 0.55f) * 4.0f);
        col = mix(col, swamp, swampK);
    }
    else col = mix(rock, snow, clamp01((h - 300.0f) / 90.0f));

    // roca por pendiente
    const f32 rockK = smoothstep(0.28f, 0.52f, slope);
    col = mix(col, mix(rock, rockDark, m_rockNoise.perlin2(x * 0.02f, z * 0.02f) * 0.5f + 0.5f), rockK);

    // roca volcanica cerca del crater
    const f32 dc = std::sqrt(x * x + z * z);
    col = mix(col, lavaRock, smoothstep(420.0f, 200.0f, dc) * 0.8f);

    // variacion sutil (manchas grandes de tono + grano fino)
    const f32 macro = (m_rockNoise.perlin2(x * 0.002f, z * 0.002f) * 0.5f + 0.5f) * 0.09f - 0.045f;
    const f32 v = (m_rockNoise.perlin2(x * 0.008f, z * 0.008f) * 0.5f + 0.5f) * 0.10f - 0.05f;
    const f32 k = clamp01(macro + v);
    return vec3(clamp01(col.r + k), clamp01(col.g + k), clamp01(col.b + k));
}

void Terrain::draw(Renderer& r, const Frustum& frustum, const Material& mat) const {
    const usize n = m_chunkMesh.size();
    usize drawn = 0;
    for (usize i = 0; i < n; ++i) {
        if (!frustum.intersectsAABB(m_chunkBounds[i])) continue;
        r.drawMesh(m_chunkMesh[i], mat4(1), mat);
        ++drawn;
    }

}

} // namespace sv
