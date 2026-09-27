// ============================================================================
//  SKYVAULT Royale - game/Game.cpp  (logica de partida)
// ============================================================================
#include "game/Game.h"
#include <algorithm>
#include <cmath>

namespace game {

Game& gameInstance() { static Game g; return g; }

// ---------------------------------------------------------------------------
// Definiciones de armas
// ---------------------------------------------------------------------------
WeaponDef Game::weaponDef(WeaponKind k) const {
    WeaponDef d;
    d.kind = k;
    switch (k) {
        case WeaponKind::Pickaxe:
            d.name = "Pico"; d.damage = 20; d.fireInterval = 0.5f; d.range = 3.2f;
            d.modelScale = 1.0f; d.modelOffset = vec3(0.32f, 1.25f, 0.25f); d.modelYaw = 0.6f;
            break;
        case WeaponKind::Shotgun:
            d.name = "Escopeta Pistón"; d.damage = 9; d.pellets = 7; d.headMult = 1.6f;
            d.fireInterval = 0.95f; d.spread = 0.045f; d.magSize = 5; d.range = 60;
            d.modelScale = 1.0f; d.modelOffset = vec3(0.3f, 1.2f, 0.4f); d.modelYaw = 0.9f;
            break;
        case WeaponKind::SMG:
            d.name = "SMG Doble Cargador"; d.damage = 16; d.headMult = 1.5f;
            d.fireInterval = 0.085f; d.spread = 0.022f; d.magSize = 30; d.range = 120;
            d.automatic = true;
            d.modelScale = 1.0f; d.modelOffset = vec3(0.3f, 1.22f, 0.4f); d.modelYaw = 0.9f;
            break;
        case WeaponKind::Rifle:
            d.name = "Fusil Escarabajo"; d.damage = 31; d.headMult = 1.7f;
            d.fireInterval = 0.165f; d.spread = 0.011f; d.magSize = 30; d.range = 200;
            d.automatic = true;
            d.modelScale = 1.0f; d.modelOffset = vec3(0.3f, 1.25f, 0.45f); d.modelYaw = 0.9f;
            break;
        case WeaponKind::Sniper:
            d.name = "Francotirador Bisonte"; d.damage = 108; d.headMult = 2.0f;
            d.fireInterval = 1.6f; d.spread = 0.002f; d.magSize = 1; d.range = 400;
            d.modelScale = 1.0f; d.modelOffset = vec3(0.3f, 1.28f, 0.5f); d.modelYaw = 0.9f;
            break;
        default: break;
    }
    return d;
}

vec4 Game::rarityColor(u8 r) {
    switch (r) {
        case 0: return vec4(0.75f, 0.75f, 0.75f, 1);
        case 1: return vec4(0.20f, 0.85f, 0.30f, 1);
        case 2: return vec4(0.25f, 0.50f, 0.98f, 1);
        case 3: return vec4(0.72f, 0.30f, 0.95f, 1);
        default: return vec4(1.0f, 0.78f, 0.15f, 1);
    }
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
bool Game::init(Renderer* renderer) {
    m_r = renderer;
    if (!m_terrain.init(m_seed)) return false;
    m_collision.setTerrain(&m_terrain);

    // modelos GLB del usuario (assets/models). Si faltan o fallan, el juego
    // usa los procedurales: nunca se rompe por un asset ausente.
    {
        const char* charNames[1] = { "Jugador" };
        (void)charNames;
        const char* weaponNames[5] = { "Pico", "Escopeta", "Subfusil", "Riflle", "Francotirador" };
        const char* prefixes[3] = { "assets/models", "../assets/models", "/assets/models" };
        char path[256];
        bool any = false;
        for (const char* pre : prefixes) {
            std::snprintf(path, sizeof(path), "%s/Jugador.glb", pre);
            if (m_gltfChar.load(path, false, 1.85f)) {
                m_gltfCharOk = true; any = true;
                SV_LOG_INFO("game", "Personaje GLB cargado (%s)", pre);
                break;
            }
        }
        const f32 weaponNorm[5] = { 1.15f, 1.15f, 0.95f, 1.25f, 1.55f };
        for (i32 w = 0; w < 5; ++w) {
            for (const char* pre : prefixes) {
                std::snprintf(path, sizeof(path), "%s/%s.glb", pre, weaponNames[w]);
                if (m_gltfWeapon[w].load(path, false, weaponNorm[w])) {
                    m_gltfWeaponOk[w] = true; any = true;
                    break;
                }
            }
        }
        if (!any) SV_LOG_WARN("game", "Sin GLB: usando modelos procedurales");
    }

    // personajes y armas procedurales (fallback si no hay GLB)
    m_charMesh    = makeCharacterMesh();
    m_pickaxeMesh = makeWeaponMesh(WeaponKind::Pickaxe);
    m_shotgunMesh = makeWeaponMesh(WeaponKind::Shotgun);
    m_smgMesh     = makeWeaponMesh(WeaponKind::SMG);
    m_rifleMesh   = makeWeaponMesh(WeaponKind::Rifle);
    m_sniperMesh  = makeWeaponMesh(WeaponKind::Sniper);

    // geometria procedural: arboles, pasto, roca, cofre, caja, dirigible
    m_treeLeafyMesh = makeLeafyTreeMesh();
    m_treePineMesh  = makePineTreeMesh();
    m_grassMesh     = makeGrassMesh();
    m_airshipMesh   = makeAirshipMesh();
    m_roofMesh      = makeRoofMesh();
    m_rockMesh = makeRockMesh();
    m_chestMesh = makeChestMesh();
    m_boxMesh = makeUnitCube();

    generateWorldContent();
    bakeHeightGrid();
    bakeMinimap();

    // bots
    m_bots.resize(MaxBots);
    respawn();          // coloca al jugador + prepara la partida (menu)

    m_particles.resize(512);
    m_state = State::Menu;
    SV_LOG_INFO("game", "Partida lista: %d bots, %zu cofres, %zu arboles",
                (i32)m_bots.size(), m_chests.size(), m_treeData.size());
    return true;
}

void Game::shutdown() {
    m_terrain.destroy();
    m_rockMesh.destroy(); m_chestMesh.destroy(); m_boxMesh.destroy();
    m_charMesh.destroy(); m_pickaxeMesh.destroy(); m_shotgunMesh.destroy();
    m_smgMesh.destroy(); m_rifleMesh.destroy(); m_sniperMesh.destroy();
    m_treeLeafyMesh.destroy(); m_treePineMesh.destroy(); m_grassMesh.destroy();
    m_airshipMesh.destroy(); m_roofMesh.destroy();
    m_gltfChar.destroy();
    for (auto& w : m_gltfWeapon) w.destroy();
    m_minimapTex.destroy();
}

// ---------------------------------------------------------------------------
// Generacion de contenido: arboles, rocas, POIs, cofres, loot
// ---------------------------------------------------------------------------

// --- mini-builder de geometria (cajas/esferas/cilindros con ruido de color) ---
namespace {

struct MeshBuilder {
    std::vector<Vertex> v;
    std::vector<u32> idx;
    u32 seed = 1;

    f32 noise(u32 n) const {
        n = (n ^ 0x9E3779B9u) * 0x85EBCA6Bu + seed;
        n ^= n >> 13; n *= 0xC2B2AE35u; n ^= n >> 16;
        return (f32)(n & 0xFFFF) / 65535.0f;
    }
    Vertex vert(vec3 p, vec3 n, vec2 uv, vec4 col) {
        Vertex o;
        o.pos = p; o.normal = n; o.uv = uv; o.tangent = vec4(1, 0, 0, 1);
        o.color = col;
        return o;
    }
    // caja (min,max) con tinte por cara y ruido sutil por vertice
    void box(vec3 mn, vec3 mx, vec3 col, f32 shade = 0.14f) {
        static const vec3 nrm[6] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        const vec3 c((mn + mx) * 0.5f), s((mx - mn) * 0.5f);
        const u32 sid = (u32)v.size();
        for (u32 f = 0; f < 6; ++f) {
            const vec3& n = nrm[f];
            const vec3 t = std::fabs(n.y) > 0.5f ? vec3(1, 0, 0) : vec3(0, 1, 0);
            const vec3 bt = cross(n, t);
            const f32 su = dot(s, t), sv = dot(s, bt), sn = dot(s, n);
            const f32 faceK = 1.0f - shade * 0.5f + shade * (f == 4 ? 1.4f : (f32)(f % 3) * 0.35f);
            const u32 b = (u32)v.size();
            for (u32 k = 0; k < 4; ++k) {
                const f32 ku = (k == 0 || k == 3) ? -1 : 1;
                const f32 kv = k < 2 ? -1 : 1;
                const vec3 p = c + n * sn + t * (ku * su) + bt * (kv * sv);
                const f32 nz = 0.92f + noise(sid + k * 7 + f * 131) * 0.16f;
                v.push_back(vert(p, n, {(f32)(k % 2), (f32)(k / 2)},
                                 vec4(col * faceK * nz, 1)));
            }
            idx.insert(idx.end(), {b, b + 2, b + 1, b, b + 3, b + 2});
        }
    }
    // esfera/elipsoide (centro, radios, color, anillos)
    void sphere(vec3 c, vec3 r, vec3 col, u32 seg = 10, u32 rings = 7) {
        const u32 base = (u32)v.size();
        for (u32 j = 0; j <= rings; ++j) {
            const f32 phi = (f32)j / (f32)rings * PI_F - PI_F * 0.5f;
            const f32 cy = std::sin(phi), cr = std::cos(phi);
            for (u32 i = 0; i <= seg; ++i) {
                const f32 th = (f32)i / (f32)seg * TAU_F;
                const vec3 n(std::cos(th) * cr, cy, std::sin(th) * cr);
                const f32 nz = 0.88f + noise(base + j * 17 + i * 3) * 0.22f;
                v.push_back(vert(c + n * r, n, {(f32)i / (f32)seg, (f32)j / (f32)rings},
                                 vec4(col * nz, 1)));
            }
        }
        for (u32 j = 0; j < rings; ++j)
            for (u32 i = 0; i < seg; ++i) {
                const u32 a = base + j * (seg + 1) + i;
                idx.insert(idx.end(), {a, a + seg + 2, a + 1, a, a + seg + 1, a + seg + 2});
            }
    }
    // cilindro/cono truncado (y0..y1, radio r0->r1)
    void cylinder(vec3 c, f32 y0, f32 y1, f32 r0, f32 r1, vec3 col, u32 seg = 8) {
        const u32 base = (u32)v.size();
        for (u32 i = 0; i <= seg; ++i) {
            const f32 a = (f32)i / (f32)seg * TAU_F;
            const vec3 n(std::cos(a), 0, std::sin(a));
            const f32 nz = 0.86f + noise(base + i * 13) * 0.24f;
            v.push_back(vert(c + vec3(n.x * r0, y0, n.z * r0), n, {0, 0}, vec4(col * nz, 1)));
            v.push_back(vert(c + vec3(n.x * r1, y1, n.z * r1), n, {1, 0}, vec4(col * nz * 1.1f, 1)));
        }
        for (u32 i = 0; i < seg; ++i) {
            const u32 b = base + i * 2;
            idx.insert(idx.end(), {b, b + 1, b + 2, b + 2, b + 1, b + 3});
        }
    }
    void cone(vec3 c, f32 y0, f32 y1, f32 r, vec3 col, u32 seg = 8) {
        const u32 base = (u32)v.size();
        for (u32 i = 0; i <= seg; ++i) {
            const f32 a = (f32)i / (f32)seg * TAU_F;
            const vec3 n(std::cos(a), 0.32f, std::sin(a));
            v.push_back(vert(c + vec3(n.x * r, y0, n.z * r), n, {0, 0}, vec4(col * (0.9f + noise(base + i) * 0.2f), 1)));
        }
        const u32 tip = (u32)v.size();
        v.push_back(vert(c + vec3(0, y1, 0), vec3(0, 1, 0), {0.5f, 0.5f}, vec4(col * 1.12f, 1)));
        for (u32 i = 0; i < seg; ++i) {
            const u32 b = base + i;
            idx.insert(idx.end(), {b, tip, b + 1});
        }
    }
};

} // namespace

Mesh Game::makeLeafyTreeMesh() {
    // frondoso: tronco + 4 copas esferoidales superpuestas (silueta "mullida")
    MeshBuilder b;
    b.seed = 7;
    const vec3 bark(0.34f, 0.24f, 0.15f);
    b.cylinder(vec3(0), 0.0f, 3.6f, 0.34f, 0.20f, bark, 9);
    const vec3 leafA(0.16f, 0.45f, 0.17f), leafB(0.21f, 0.52f, 0.20f), leafC(0.14f, 0.40f, 0.14f);
    b.sphere(vec3(0.0f, 4.6f, 0.0f),  vec3(2.6f, 2.3f, 2.6f), leafA, 10, 7);
    b.sphere(vec3(1.5f, 3.9f, 0.8f),  vec3(1.7f, 1.5f, 1.7f), leafB, 8, 6);
    b.sphere(vec3(-1.4f, 4.1f, -0.7f),vec3(1.6f, 1.4f, 1.6f), leafC, 8, 6);
    b.sphere(vec3(0.3f, 5.9f, -1.0f), vec3(1.5f, 1.3f, 1.5f), leafB, 8, 6);
    return m_r->createMesh(b.v.data(), (u32)b.v.size(), b.idx.data(), (u32)b.idx.size());
}

Mesh Game::makePineTreeMesh() {
    // pino: tronco + 4 conos apilados
    MeshBuilder b;
    b.seed = 23;
    const vec3 bark(0.30f, 0.21f, 0.13f);
    b.cylinder(vec3(0), 0.0f, 2.6f, 0.30f, 0.16f, bark, 8);
    const vec3 needle(0.10f, 0.33f, 0.14f), needleHi(0.13f, 0.40f, 0.16f);
    b.cone(vec3(0), 1.5f, 4.4f, 2.5f, needle, 9);
    b.cone(vec3(0), 3.0f, 5.9f, 1.9f, needleHi, 9);
    b.cone(vec3(0), 4.4f, 7.1f, 1.3f, needle, 8);
    b.cone(vec3(0), 5.7f, 8.1f, 0.7f, needleHi, 7);
    return m_r->createMesh(b.v.data(), (u32)b.v.size(), b.idx.data(), (u32)b.idx.size());
}

Mesh Game::makeGrassMesh() {
    // 3 quads cruzados a 60 grados con gradiente base oscura -> punta clara
    std::vector<Vertex> v;
    std::vector<u32> idx;
    const vec3 base(0.17f, 0.28f, 0.11f), tip(0.47f, 0.70f, 0.24f);
    const f32 W = 0.10f, H = 0.78f;
    for (u32 k = 0; k < 3; ++k) {
        const f32 a = (f32)k * (PI_F / 3.0f);
        const vec3 d(std::cos(a), 0, std::sin(a));
        const vec3 n(d.z, 0.35f, -d.x);
        const u32 b = (u32)v.size();
        const vec4 c0(base, 1), c1(tip, 1), c1b(tip * 1.08f, 1);
        v.push_back({{-d.x * W, 0, -d.z * W}, n, {0, 0}, {1,0,0,1},{},{}, c0});
        v.push_back({{ d.x * W, 0,  d.z * W}, n, {1, 0}, {1,0,0,1},{},{}, c0});
        v.push_back({{ d.x * W * 0.55f, H,  d.z * W * 0.55f}, n, {1, 1}, {1,0,0,1},{},{}, c1});
        v.push_back({{-d.x * W * 0.55f, H, -d.z * W * 0.55f}, n, {0, 1}, {1,0,0,1},{},{}, c1b});
        idx.insert(idx.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
    }
    return m_r->createMesh(v.data(), (u32)v.size(), idx.data(), (u32)idx.size());
}

Mesh Game::makeAirshipMesh() {
    // Carguero Nube: envolvente elipsoidal con paneles + bandas oxidadas y aletas
    MeshBuilder b;
    b.seed = 91;
    const vec3 cream(0.84f, 0.79f, 0.71f);
    const vec3 creamDark(0.72f, 0.66f, 0.57f);
    const vec3 rust(0.58f, 0.40f, 0.26f);
    const vec3 belly(0.55f, 0.52f, 0.47f);
    const u32 seg = 20, rings = 13;
    const vec3 r(9.0f, 6.4f, 21.0f);
    const u32 base = (u32)b.v.size();
    for (u32 j = 0; j <= rings; ++j) {
        const f32 phi = (f32)j / (f32)rings * PI_F - PI_F * 0.5f;
        const f32 cy = std::sin(phi), cr = std::cos(phi);
        const bool band = (j % 4) == 0;
        for (u32 i = 0; i <= seg; ++i) {
            const f32 th = (f32)i / (f32)seg * TAU_F;
            const vec3 n(std::cos(th) * cr, cy, std::sin(th) * cr);
            vec3 col = (j < 3) ? belly : (band ? mix(cream, rust, 0.45f)
                              : ((i % 2) ? cream : creamDark));
            const f32 nz = 0.9f + b.noise(base + j * 31 + i * 7) * 0.18f;
            b.v.push_back(b.vert(n * r, n, {(f32)i / (f32)seg, (f32)j / (f32)rings},
                                 vec4(col * nz, 1)));
        }
    }
    for (u32 j = 0; j < rings; ++j)
        for (u32 i = 0; i < seg; ++i) {
            const u32 a = base + j * (seg + 1) + i;
            b.idx.insert(b.idx.end(), {a, a + seg + 2, a + 1, a, a + seg + 1, a + seg + 2});
        }
    // aletas de cola: vertical + horizontal (paneles metalicos)
    b.box(vec3(-0.35f, 1.0f, -22.5f), vec3(0.35f, 8.5f, -15.5f), vec3(0.50f, 0.36f, 0.24f));
    b.box(vec3(-0.35f, -2.2f, -22.5f), vec3(0.35f, 1.0f, -15.5f), rust);
    b.box(vec3(1.0f, -0.35f, -22.5f), vec3(8.5f, 0.35f, -15.5f), vec3(0.50f, 0.36f, 0.24f));
    b.box(vec3(-2.2f, -0.35f, -22.5f), vec3(1.0f, 0.35f, -15.5f), rust);
    return m_r->createMesh(b.v.data(), (u32)b.v.size(), b.idx.data(), (u32)b.idx.size());
}

Mesh Game::makeRoofMesh() {
    // tejado a dos aguas normalizado: x[-0.5..0.5], z[-0.5..0.5], cumbrera en y=1
    std::vector<Vertex> v;
    std::vector<u32> idx;
    const vec4 slope(1.0f), gable(0.78f), ridge(1.12f);
    const f32 hw = 0.5f;
    // faldon -X: de (-0.5,0) a (0,1)
    {
        const u32 b = (u32)v.size();
        const vec3 n(-0.894f, 0.447f, 0);
        v.insert(v.end(), {
            {{-hw, 0, -hw}, n, {0, 0}, {1,0,0,1},{},{}, slope},
            {{-hw, 0,  hw}, n, {1, 0}, {1,0,0,1},{},{}, slope},
            {{ 0.0f, 1, hw}, n, {1, 1}, {1,0,0,1},{},{}, ridge},
            {{ 0.0f, 1, -hw}, n, {0, 1}, {1,0,0,1},{},{}, ridge},
        });
        idx.insert(idx.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
    }
    // faldon +X
    {
        const u32 b = (u32)v.size();
        const vec3 n(0.894f, 0.447f, 0);
        v.insert(v.end(), {
            {{hw, 0,  hw}, n, {0, 0}, {1,0,0,1},{},{}, slope},
            {{hw, 0, -hw}, n, {1, 0}, {1,0,0,1},{},{}, slope},
            {{0.0f, 1, -hw}, n, {1, 1}, {1,0,0,1},{},{}, ridge},
            {{0.0f, 1,  hw}, n, {0, 1}, {1,0,0,1},{},{}, ridge},
        });
        idx.insert(idx.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
    }
    // frontones (triangulos en z=+-0.5)
    {
        const u32 b = (u32)v.size();
        v.insert(v.end(), {
            {{-hw, 0, -hw}, vec3(0,0,-1), {0, 0}, {1,0,0,1},{},{}, gable},
            {{ hw, 0, -hw}, vec3(0,0,-1), {1, 0}, {1,0,0,1},{},{}, gable},
            {{ 0.0f, 1, -hw}, vec3(0,0,-1), {0.5f, 1}, {1,0,0,1},{},{}, gable},
        });
        idx.insert(idx.end(), {b, b + 2, b + 1});
    }
    {
        const u32 b = (u32)v.size();
        v.insert(v.end(), {
            {{ hw, 0,  hw}, vec3(0,0,1), {0, 0}, {1,0,0,1},{},{}, gable},
            {{-hw, 0,  hw}, vec3(0,0,1), {1, 0}, {1,0,0,1},{},{}, gable},
            {{ 0.0f, 1,  hw}, vec3(0,0,1), {0.5f, 1}, {1,0,0,1},{},{}, gable},
        });
        idx.insert(idx.end(), {b, b + 2, b + 1});
    }
    return m_r->createMesh(v.data(), (u32)v.size(), idx.data(), (u32)idx.size());
}

Mesh Game::makeRockMesh() {
    std::vector<Vertex> v;
    std::vector<u32> idx;
    // icosaedro deformado simple: 2 anillos + polos
    const vec3 rock(0.52f, 0.50f, 0.48f);
    const f32 R = 1.0f;
    auto ring = [&](f32 y, f32 r, u32 n, f32 twist) {
        const u32 base = (u32)v.size();
        for (u32 i = 0; i < n; ++i) {
            const f32 a = (f32)i / n * TAU_F + twist;
            const vec3 p(std::cos(a) * r, y, std::sin(a) * r);
            v.push_back({p * 1.1f, normalize(p), {0, 0}, {1,0,0,1},{},{}, vec4(rock * (0.9f + 0.2f * (f32)(i % 2)), 1)});
        }
        return base;
    };
    const u32 top = (u32)v.size();
    v.push_back({{0, R * 0.9f, 0}, vec3(0, 1, 0), {0, 0}, {1,0,0,1},{},{}, vec4(rock, 1)});
    const u32 r1 = ring(R * 0.35f, R * 0.85f, 7, 0.2f);
    const u32 r2 = ring(-R * 0.25f, R * 0.7f, 7, 0.5f);
    const u32 bot = (u32)v.size();
    v.push_back({{0, -R * 0.55f, 0}, vec3(0, -1, 0), {0, 0}, {1,0,0,1},{},{}, vec4(rock * 0.8f, 1)});
    for (u32 i = 0; i < 7; ++i) {
        const u32 a = r1 + i, b = r1 + (i + 1) % 7;
        const u32 c = r2 + i, d = r2 + (i + 1) % 7;
        idx.insert(idx.end(), {top, a, b});
        idx.insert(idx.end(), {a, d, b, a, c, d});
        idx.insert(idx.end(), {c, bot, d});
    }
    return m_r->createMesh(v.data(), (u32)v.size(), idx.data(), (u32)idx.size());
}

Mesh Game::makeChestMesh() {
    // cofre: caja dorada 1.0 x 0.7 x 0.7
    std::vector<Vertex> v;
    std::vector<u32> idx;
    const vec3 gold(0.85f, 0.62f, 0.16f), dark(0.45f, 0.30f, 0.10f);
    auto box = [&](vec3 mn, vec3 mx, vec3 col) {
        const u32 base = (u32)v.size();
        const vec3 c((mn + mx) * 0.5f), s((mx - mn) * 0.5f);
        const vec3 cols[6] = {col * 0.8f, col, col * 0.9f, col * 0.9f, col * 1.1f, col * 0.7f};
        static const vec3 normals[6] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        for (u32 f = 0; f < 6; ++f) {
            const u32 b = (u32)v.size();
            for (u32 k = 0; k < 4; ++k) {
                vec3 p = c;
                const vec3& n = normals[f];
                vec3 t = std::fabs(n.x) > 0.5f ? vec3(0, 1, 0) : (std::fabs(n.y) > 0.5f ? vec3(1, 0, 0) : vec3(1, 0, 0));
                vec3 bt = cross(n, t);
                const f32 su = (k == 0 || k == 3) ? -1 : 1;
                const f32 sv = (k < 2) ? -1 : 1;
                p += n * dot(s, n) + t * su * dot(s, t) + bt * sv * dot(s, bt);
                v.push_back({p, n, {(f32)(k % 2), (f32)(k / 2)}, {1,0,0,1},{},{}, vec4(cols[f], 1)});
            }
            idx.insert(idx.end(), {b, b + 2, b + 1, b, b + 3, b + 2});
        }
        (void)base;
    };
    box(vec3(-0.5f, 0, -0.35f), vec3(0.5f, 0.28f, 0.35f), dark);       // base
    box(vec3(-0.5f, 0.28f, -0.35f), vec3(0.5f, 0.62f, 0.35f), gold);   // tapa
    return m_r->createMesh(v.data(), (u32)v.size(), idx.data(), (u32)idx.size());
}

Mesh Game::makeUnitCube() {
    std::vector<Vertex> v;
    std::vector<u32> idx;
    const vec3 wood(0.62f, 0.44f, 0.24f);
    const vec3 nrm[6] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for (u32 f = 0; f < 6; ++f) {
        const u32 b = (u32)v.size();
        const vec3& n = nrm[f];
        const vec3 t = std::fabs(n.y) > 0.5f ? vec3(1, 0, 0) : vec3(0, 1, 0);
        const vec3 bt = cross(n, t);
        const f32 tu = std::fabs(n.y) > 0.5f ? 0.5f : 0.5f;
        const vec3 half(0.5f);
        const f32 hu = dot(half, t) * 2, hv = dot(half, bt) * 2;
        for (u32 k = 0; k < 4; ++k) {
            const f32 su = (k == 0 || k == 3) ? -1 : 1;
            const f32 sv = (k < 2) ? -1 : 1;
            vec3 p = n * 0.5f + t * su * 0.5f + bt * sv * 0.5f;
            v.push_back({p, n, {(k % 2) * tu, (k / 2) * tu}, {1,0,0,1},{},{},
                         vec4(wood * (0.85f + 0.15f * (f32)(f % 3)), 1)});
        }
        idx.insert(idx.end(), {b, b + 2, b + 1, b, b + 3, b + 2});
        (void)hu; (void)hv;
    }
    return m_r->createMesh(v.data(), (u32)v.size(), idx.data(), (u32)idx.size());
}

void Game::generateWorldContent() {
    Random rng(m_seed * 7 + 3);
    const auto& pois = m_terrain.pois();

    // arboles y rocas: distribucion por ruido, evitando POIs y agua
    m_treeData.clear();
    m_treeKind.clear();
    m_rockData.clear();
    for (i32 i = 0; i < 12000 && m_treeData.size() < 3600; ++i) {
        const f32 x = rng.symmetric() * Terrain::Half * 0.95f;
        const f32 z = rng.symmetric() * Terrain::Half * 0.95f;
        const f32 h = m_terrain.height(x, z);
        if (h < 8.0f || h > 240.0f) continue;
        const vec3 n = m_terrain.normal(x, z);
        if (n.y < 0.82f) continue;                       // pendiente
        bool nearPoi = false;
        for (const auto& poi : pois)
            if (distance(vec2(x, z), poi.pos) < poi.radius * 0.78f) { nearPoi = true; break; }
        if (nearPoi) continue;
        const f32 dens = m_terrain.moistureAt(x, z);
        if (rng.nextF32() > dens * 2.4f) continue;
        m_treeData.push_back(vec4(x, h, z, 0.8f + rng.nextF32() * 0.7f));
        m_treeKind.push_back(rng.nextF32() < 0.62f ? (u8)0 : (u8)1);
    }
    m_treeAlive.assign(m_treeData.size(), true);
    m_treeHp.assign(m_treeData.size(), 100.0f);
    for (i32 i = 0; i < 1500 && m_rockData.size() < 700; ++i) {
        const f32 x = rng.symmetric() * Terrain::Half * 0.95f;
        const f32 z = rng.symmetric() * Terrain::Half * 0.95f;
        const f32 h = m_terrain.height(x, z);
        if (h < 3.0f || h > 330.0f) continue;
        m_rockData.push_back(vec3(x, h, z));
    }

    // pueblos/ciudades: cada POI es un asentamiento con calles y edificios
    m_staticBoxes.clear();
    m_boxColors.clear();
    m_decoBoxes.clear();
    m_roofPieces.clear();
    m_chests.clear();
    m_loot.clear();
    for (const auto& poi : pois) generateTown(rng, poi);

    // arboles junto a los pueblos (anillo verde denso alrededor)
    for (const auto& poi : pois) {
        const i32 n = 8 + (i32)rng.rangeU32(7);
        for (i32 k = 0; k < n; ++k) {
            const f32 a = rng.nextF32() * TAU_F;
            const f32 r = poi.radius * (0.82f + rng.nextF32() * 0.55f);
            const f32 x = poi.pos.x + std::cos(a) * r;
            const f32 z = poi.pos.y + std::sin(a) * r;
            const f32 h = m_terrain.height(x, z);
            if (h < 8.0f || h > 200.0f) continue;
            m_treeData.push_back(vec4(x, h, z, 0.9f + rng.nextF32() * 0.6f));
            m_treeKind.push_back(rng.nextF32() < 0.7f ? (u8)0 : (u8)1);
            m_treeAlive.push_back(true);
            m_treeHp.push_back(100.0f);
        }
    }

    // arboles al collider
    for (usize i = 0; i < m_treeData.size(); ++i)
        m_collision.addTree(vec2(m_treeData[i].x, m_treeData[i].z), 0.45f * m_treeData[i].w, 6.4f * m_treeData[i].w);

    rebuildCollision();
}

// ---------------------------------------------------------------------------
// generateTown: asentamiento por POI con plaza central, calles en rejilla,
// edificios de 1-3 plantas (madera/piedra/metal), ventanas, torre de vigilancia
// y cofres/loot dentro y alrededor.
// ---------------------------------------------------------------------------
void Game::generateTown(Random& rng, const PoiDef& poi) {
    // paleta de materiales (guiada por las texturas de referencia del usuario)
    struct Pal { vec3 wall, trim, roof, name3; };
    static const Pal pals[3] = {
        { {0.55f, 0.40f, 0.25f}, {0.42f, 0.30f, 0.19f}, {0.36f, 0.23f, 0.14f}, {} },  // madera
        { {0.62f, 0.58f, 0.52f}, {0.48f, 0.44f, 0.40f}, {0.30f, 0.24f, 0.20f}, {} },  // piedra
        { {0.56f, 0.57f, 0.59f}, {0.43f, 0.44f, 0.47f}, {0.62f, 0.38f, 0.22f}, {} },  // metal
    };
    static const vec3 windowCol(0.15f, 0.21f, 0.30f);

    const f32 cell = clamp(poi.radius * 0.42f, 24.0f, 34.0f);
    const i32 half = 2;                       // rejilla 5x5 (celda central = plaza)
    i32 buildings = 0;

    auto addBox = [&](vec3 mn, vec3 mx, vec3 col) {
        m_staticBoxes.push_back({AABB{mn, mx}, ColliderOwner::World, 0, false, 0});
        m_boxColors.push_back(vec4(col, 1.0f));
    };
    auto addDeco = [&](vec3 mn, vec3 mx, vec4 col, f32 emis) {
        m_decoBoxes.push_back({AABB{mn, mx}, col, emis});
    };

    // --- edificio: planta rectangular con puerta, ventanas, plantas y tejado ----
    auto building = [&](vec2 c, f32 w, f32 d, i32 floors, u8 mat, u8 doorSide) {
        const f32 base = m_terrain.height(c.x, c.y);
        if (base < 4.0f) return;                       // en el agua, no
        const f32 dh = m_terrain.height(c.x + w * 0.5f, c.y + d * 0.5f);
        const f32 dh2 = m_terrain.height(c.x - w * 0.5f, c.y - d * 0.5f);
        if (std::fabs(dh - base) > 3.5f || std::fabs(dh2 - base) > 3.5f) return;  // muy empinado
        const f32 y0 = base - 1.2f;                    // faldon enterrado (pendientes)
        const f32 H = 3.6f;
        const Pal& pal = pals[mat];
        const f32 tint = 0.9f + rng.nextF32() * 0.2f;
        const vec3 wall = pal.wall * tint;
        const f32 T = 0.32f;
        const f32 doorW = 1.7f, doorH = 2.7f;
        for (i32 fl = 0; fl < floors; ++fl) {
            const f32 fy = base + (f32)fl * H;
            const f32 x0 = c.x - w * 0.5f, x1 = c.x + w * 0.5f;
            const f32 z0 = c.y - d * 0.5f, z1 = c.y + d * 0.5f;
            // paredes (la cara de la puerta se parte en 3: dintel + 2 laterales)
            auto wallX = [&](f32 x, bool door) {
                if (!door || fl > 0)
                    addBox(vec3(x - T * 0.5f, fy, z0), vec3(x + T * 0.5f, fy + H, z1), wall);
                else {
                    addBox(vec3(x - T * 0.5f, fy + doorH, z0), vec3(x + T * 0.5f, fy + H, z1), wall);
                    addBox(vec3(x - T * 0.5f, fy, z0), vec3(x + T * 0.5f, fy + doorH, z0 + (z1 - z0) * 0.5f - doorW * 0.5f), wall);
                    addBox(vec3(x - T * 0.5f, fy, z0 + (z1 - z0) * 0.5f + doorW * 0.5f), vec3(x + T * 0.5f, fy + doorH, z1), wall);
                }
            };
            auto wallZ = [&](f32 z, bool door) {
                if (!door || fl > 0)
                    addBox(vec3(x0, fy, z - T * 0.5f), vec3(x1, fy + H, z + T * 0.5f), wall);
                else {
                    addBox(vec3(x0, fy + doorH, z - T * 0.5f), vec3(x1, fy + H, z + T * 0.5f), wall);
                    addBox(vec3(x0, fy, z - T * 0.5f), vec3(x0 + (x1 - x0) * 0.5f - doorW * 0.5f, fy + doorH, z + T * 0.5f), wall);
                    addBox(vec3(x0 + (x1 - x0) * 0.5f + doorW * 0.5f, fy, z - T * 0.5f), vec3(x1, fy + doorH, z + T * 0.5f), wall);
                }
            };
            wallX(x0, doorSide == 3);
            wallX(x1, doorSide == 1);
            wallZ(z0, doorSide == 2);
            wallZ(z1, doorSide == 0);
            // forjado entre plantas
            if (fl > 0)
                addBox(vec3(x0, fy - 0.28f, z0), vec3(x1, fy, z1), pal.trim * 0.9f);
            // ventanas (caras que no son puerta, cada planta): marco + cristal
            const f32 wy = fy + 1.15f, ww = 1.05f, wh = 1.35f, off = 0.06f;
            auto winX = [&](f32 x) {
                for (i32 k = 0; k < 2; ++k) {
                    const f32 zc = z0 + d * (0.3f + 0.4f * (f32)k);
                    addDeco(vec3(x - off, wy, zc - ww * 0.5f), vec3(x + off, wy + wh, zc + ww * 0.5f),
                            vec4(pal.trim, 1), 0.0f);
                    addDeco(vec3(x - off * 1.6f, wy + 0.12f, zc - ww * 0.5f + 0.12f),
                            vec3(x + off * 1.6f, wy + wh - 0.12f, zc + ww * 0.5f - 0.12f),
                            vec4(windowCol, 1), 0.22f);
                }
            };
            auto winZ = [&](f32 z) {
                for (i32 k = 0; k < 2; ++k) {
                    const f32 xc = x0 + w * (0.3f + 0.4f * (f32)k);
                    addDeco(vec3(xc - ww * 0.5f, wy, z - off), vec3(xc + ww * 0.5f, wy + wh, z + off),
                            vec4(pal.trim, 1), 0.0f);
                    addDeco(vec3(xc - ww * 0.5f + 0.12f, wy + 0.12f, z - off * 1.6f),
                            vec3(xc + ww * 0.5f - 0.12f, wy + wh - 0.12f, z + off * 1.6f),
                            vec4(windowCol, 1), 0.22f);
                }
            };
            if (fl > 0) { winX(x0); winX(x1); }
            winZ(z0);
            winZ(z1);
        }
        // tejado: a dos aguas (madera/piedra) o plano con alero (metal)
        const f32 top = base + (f32)floors * H;
        if (mat == 2) {
            addBox(vec3(c.x - w * 0.5f - 0.55f, top, c.y - d * 0.5f - 0.55f),
                   vec3(c.x + w * 0.5f + 0.55f, top + 0.38f, c.y + d * 0.5f + 0.55f), pal.roof);
        } else {
            const f32 rh = 1.9f + rng.nextF32() * 1.1f;
            const bool ridgeAlongZ = w > d;
            const vec3 sz = ridgeAlongZ ? vec3(w + 1.1f, rh, d + 1.1f)
                                        : vec3(d + 1.1f, rh, w + 1.1f);
            const f32 ryaw = ridgeAlongZ ? 0.0f : PI_F * 0.5f;
            RoofPiece rp;
            rp.c = vec3(c.x, top - 0.06f, c.y);
            rp.s = sz;
            rp.col = vec4(pal.roof, 1.0f);
            rp.col.w = ryaw;                    // yaw empaquetada en .w
            m_roofPieces.push_back(rp);
            // alero plano bajo el tejado (oculta hueco entre faldones y muro)
            addBox(vec3(c.x - w * 0.5f - 0.55f, top - 0.30f, c.y - d * 0.5f - 0.55f),
                   vec3(c.x + w * 0.5f + 0.55f, top, c.y + d * 0.5f + 0.55f), pal.trim * 0.85f);
        }
        // base/zocalo de piedra
        addBox(vec3(c.x - w * 0.5f - 0.25f, y0, c.y - d * 0.5f - 0.25f),
               vec3(c.x + w * 0.5f + 0.25f, base + 0.15f, c.y + d * 0.5f + 0.25f), pals[1].wall * 0.85f);
        ++buildings;

        // cofre dentro (60%) y loot en la entrada (55%)
        if (rng.nextF32() < 0.60f) {
            const vec2 in = c + vec2(rng.symmetric(), rng.symmetric()) * vec2(w, d) * 0.18f;
            const f32 ch = m_terrain.height(in.x, in.y);
            if (ch > 3.0f) m_chests.push_back({vec3(in.x, ch, in.y), false});
        }
        if (rng.nextF32() < 0.55f) {
            const vec2 doorDir(doorSide == 1 ? 1.0f : doorSide == 3 ? -1.0f : 0.0f,
                               doorSide == 0 ? 1.0f : doorSide == 2 ? -1.0f : 0.0f);
            const vec2 p = c + doorDir * ((doorDir.x != 0 ? w : d) * 0.5f + 1.6f);
            const f32 lh = m_terrain.height(p.x, p.y);
            if (lh > 3.0f) {
                FloorLoot fl;
                fl.pos = vec3(p.x, lh + 0.3f, p.y);
                fl.weapon.kind = (WeaponKind)(1 + rng.rangeU32(4));
                fl.weapon.rarity = (u8)rng.rangeU32(5);
                fl.weapon.ammo = 999;
                fl.taken = false;
                m_loot.push_back(fl);
            }
        }
    };

    // --- rejilla de calles -------------------------------------------------------
    for (i32 gz = -half; gz <= half; ++gz)
        for (i32 gx = -half; gx <= half; ++gx) {
            if (gx == 0 && gz == 0) continue;                 // plaza central
            if (rng.nextF32() < 0.22f) continue;              // solares vacios
            const f32 jx = rng.symmetric() * 3.0f, jz = rng.symmetric() * 3.0f;
            const vec2 c(poi.pos.x + (f32)gx * cell + jx, poi.pos.y + (f32)gz * cell + jz);
            if (distance(c, poi.pos) > poi.radius * 1.05f) continue;
            const f32 w = 8.0f + rng.nextF32() * 4.0f;
            const f32 d = 7.0f + rng.nextF32() * 3.5f;
            const i32 maxF = 1 + (i32)(poi.buildingCount / 2 + rng.nextF32() * 2.2f);
            const i32 floors = 1 + (i32)rng.rangeU32((u32)clamp(maxF, 1, 3));
            const u8 mat = (u8)rng.rangeU32(3);
            // la puerta mira hacia la plaza
            const u8 doorSide = (std::fabs((f32)gx) > std::fabs((f32)gz))
                              ? (u8)(gx > 0 ? 3 : 1) : (u8)(gz > 0 ? 2 : 0);
            building(c, w, d, floors, mat, doorSide);
        }

    // --- plaza: solado de piedra + pozo + faroles --------------------------------
    {
        const f32 ph = m_terrain.height(poi.pos.x, poi.pos.y);
        if (ph > 4.0f) {
            const f32 pw = cell * 1.05f;
            addBox(vec3(poi.pos.x - pw, ph - 0.45f, poi.pos.y - pw),
                   vec3(poi.pos.x + pw, ph + 0.08f, poi.pos.y + pw), vec3(0.55f, 0.50f, 0.43f));
            // pozo (anillo de piedra + hueco oscuro + tejadizo)
            addBox(vec3(poi.pos.x - 1.3f, ph, poi.pos.y - 1.3f), vec3(poi.pos.x + 1.3f, ph + 1.0f, poi.pos.y - 0.9f), pals[1].trim);
            addBox(vec3(poi.pos.x - 1.3f, ph, poi.pos.y + 0.9f), vec3(poi.pos.x + 1.3f, ph + 1.0f, poi.pos.y + 1.3f), pals[1].trim);
            addBox(vec3(poi.pos.x - 1.3f, ph, poi.pos.y - 0.9f), vec3(poi.pos.x - 0.9f, ph + 1.0f, poi.pos.y + 0.9f), pals[1].trim);
            addBox(vec3(poi.pos.x + 0.9f, ph, poi.pos.y - 0.9f), vec3(poi.pos.x + 1.3f, ph + 1.0f, poi.pos.y + 0.9f), pals[1].trim);
            addDeco(vec3(poi.pos.x - 0.9f, ph + 0.15f, poi.pos.y - 0.9f),
                    vec3(poi.pos.x + 0.9f, ph + 0.3f, poi.pos.y + 0.9f), vec4(0.05f, 0.07f, 0.09f, 1), 0);
            addBox(vec3(poi.pos.x - 1.6f, ph + 2.6f, poi.pos.y - 1.6f), vec3(poi.pos.x + 1.6f, ph + 3.0f, poi.pos.y + 1.6f), pals[0].roof);
            addBox(vec3(poi.pos.x - 0.22f, ph + 1.0f, poi.pos.y - 0.22f), vec3(poi.pos.x + 0.22f, ph + 2.7f, poi.pos.y + 0.22f), pals[0].wall);
            // faroles con luz caliente
            for (i32 k = 0; k < 4; ++k) {
                const f32 a = (f32)k * PI_F * 0.5f + PI_F * 0.25f;
                const vec2 lp = poi.pos + vec2(std::cos(a), std::sin(a)) * pw * 0.82f;
                const f32 lh = m_terrain.height(lp.x, lp.y);
                addBox(vec3(lp.x - 0.12f, lh, lp.y - 0.12f), vec3(lp.x + 0.12f, lh + 3.1f, lp.y + 0.12f), pals[2].trim);
                addDeco(vec3(lp.x - 0.3f, lh + 3.0f, lp.y - 0.3f), vec3(lp.x + 0.3f, lh + 3.6f, lp.y + 0.3f),
                        vec4(1.0f, 0.85f, 0.55f, 1), 0.85f);
            }
            // cofre en la plaza
            m_chests.push_back({vec3(poi.pos.x, ph + 0.1f, poi.pos.y + pw * 0.55f), false});
        }
    }

    // --- torre de vigilancia (POIs grandes) -------------------------------------
    if (poi.buildingCount >= 4) {
        const vec2 tp = poi.pos + vec2(cell * 1.6f, -cell * 1.6f);
        const f32 th = m_terrain.height(tp.x, tp.y);
        if (th > 4.0f) {
            const f32 legH = 9.0f, S = 2.6f;
            for (i32 lx = 0; lx < 2; ++lx)
                for (i32 lz = 0; lz < 2; ++lz)
                    addBox(vec3(tp.x + (f32)(lx * 2 - 1) * S - 0.28f, th - 1.0f, tp.y + (f32)(lz * 2 - 1) * S - 0.28f),
                           vec3(tp.x + (f32)(lx * 2 - 1) * S + 0.28f, th + legH, tp.y + (f32)(lz * 2 - 1) * S + 0.28f),
                           pals[0].trim);
            addBox(vec3(tp.x - S - 0.8f, th + legH, tp.y - S - 0.8f),
                   vec3(tp.x + S + 0.8f, th + legH + 0.35f, tp.y + S + 0.8f), pals[2].trim);
            // barandilla
            addBox(vec3(tp.x - S - 0.8f, th + legH + 0.35f, tp.y - S - 0.8f),
                   vec3(tp.x + S + 0.8f, th + legH + 1.5f, tp.y - S - 0.5f), pals[0].roof);
            addBox(vec3(tp.x - S - 0.8f, th + legH + 0.35f, tp.y + S + 0.5f),
                   vec3(tp.x + S + 0.8f, th + legH + 1.5f, tp.y + S + 0.8f), pals[0].roof);
            addBox(vec3(tp.x - S - 0.8f, th + legH + 0.35f, tp.y - S - 0.5f),
                   vec3(tp.x - S - 0.5f, th + legH + 1.5f, tp.y + S + 0.5f), pals[0].roof);
            addBox(vec3(tp.x + S + 0.5f, th + legH + 0.35f, tp.y - S - 0.5f),
                   vec3(tp.x + S + 0.8f, th + legH + 1.5f, tp.y + S + 0.5f), pals[0].roof);
            // tejadizo + cofre en la cima
            addBox(vec3(tp.x - S - 1.0f, th + legH + 3.3f, tp.y - S - 1.0f),
                   vec3(tp.x + S + 1.0f, th + legH + 3.7f, tp.y + S + 1.0f), pals[0].roof);
            addBox(vec3(tp.x - 0.2f, th + legH + 1.5f, tp.y - 0.2f), vec3(tp.x + 0.2f, th + legH + 3.4f, tp.y + 0.2f), pals[0].wall);
            m_chests.push_back({vec3(tp.x, th + legH + 0.35f, tp.y), false});
        }
    }

    // --- cofres y loot extra por las calles --------------------------------------
    const i32 extraChests = 3 + (i32)rng.rangeU32(3);
    for (i32 k = 0; k < extraChests; ++k) {
        const f32 a = rng.nextF32() * TAU_F, r = rng.nextF32() * poi.radius * 0.85f;
        const vec2 p = poi.pos + vec2(std::cos(a), std::sin(a)) * r;
        const f32 h = m_terrain.height(p.x, p.y);
        if (h > 4.0f) m_chests.push_back({vec3(p.x, h, p.y), false});
    }
    const i32 extraLoot = 4 + (i32)rng.rangeU32(4);
    for (i32 k = 0; k < extraLoot; ++k) {
        const f32 a = rng.nextF32() * TAU_F, r = rng.nextF32() * poi.radius;
        const vec2 p = poi.pos + vec2(std::cos(a), std::sin(a)) * r;
        const f32 h = m_terrain.height(p.x, p.y);
        if (h < 3.0f) continue;
        FloorLoot fl;
        fl.pos = vec3(p.x, h + 0.3f, p.y);
        fl.weapon.kind = (WeaponKind)(1 + rng.rangeU32(4));
        fl.weapon.rarity = (u8)rng.rangeU32(5);
        fl.weapon.ammo = 999;
        fl.taken = false;
        m_loot.push_back(fl);
    }
    (void)buildings;
}

// ---------------------------------------------------------------------------
// Rejilla de alturas (muestreo barato para el pasto y utilidades)
// ---------------------------------------------------------------------------
void Game::bakeHeightGrid() {
    m_heightGrid.assign((usize)GridN * GridN, 0.0f);
    const f32 step = Terrain::Size / (f32)(GridN - 1);
    for (u32 j = 0; j < GridN; ++j)
        for (u32 i = 0; i < GridN; ++i)
            m_heightGrid[(usize)j * GridN + i] =
                m_terrain.height(-Terrain::Half + (f32)i * step, -Terrain::Half + (f32)j * step);
    SV_LOG_INFO("game", "Rejilla de alturas %ux%u lista", GridN, GridN);
}

f32 Game::gridHeight(f32 x, f32 z) const {
    if (m_heightGrid.empty()) return m_terrain.height(x, z);
    const f32 step = Terrain::Size / (f32)(GridN - 1);
    const f32 u = (x + Terrain::Half) / step;
    const f32 v = (z + Terrain::Half) / step;
    if (u < 0 || v < 0 || u > (f32)(GridN - 1) || v > (f32)(GridN - 1)) return -100.0f;
    const i32 i0 = (i32)u, j0 = (i32)v;
    const i32 i1 = std::min(i0 + 1, (i32)GridN - 1), j1 = std::min(j0 + 1, (i32)GridN - 1);
    const f32 fu = u - (f32)i0, fv = v - (f32)j0;
    const f32 h00 = m_heightGrid[(usize)j0 * GridN + i0];
    const f32 h10 = m_heightGrid[(usize)j0 * GridN + i1];
    const f32 h01 = m_heightGrid[(usize)j1 * GridN + i0];
    const f32 h11 = m_heightGrid[(usize)j1 * GridN + i1];
    return mix(mix(h00, h10, fu), mix(h01, h11, fu), fv);
}

// ---------------------------------------------------------------------------
// Pasto: campo de brizas alrededor de la camara (regenerado al moverse)
// ---------------------------------------------------------------------------
void Game::regenGrass(const vec3& camPos) {
    if (!m_grassInstances.empty() && distance(camPos, m_grassLastPos) < 12.0f) return;
    m_grassLastPos = camPos;
    m_grassInstances.clear();

    // densidad por preset
    const Preset p = m_r->gfx().preset;
    const f32 densK = p == Preset::Bajo ? 0.45f : (p == Preset::Medio ? 1.0f
                   : (p == Preset::Alto ? 1.4f : 1.8f));
    if (densK <= 0.0f) return;

    const f32 step = 2.6f;
    const i32 R = (i32)(22.0f * densK * 0.65f + 10.0f);   // radio en celdas
    // cajas cercanas para no meter pasto dentro de edificios
    static std::vector<const AABB*> nearBoxes;
    nearBoxes.clear();
    for (const auto& b : m_staticBoxes) {
        const vec3 c = (b.box.bmin + b.box.bmax) * 0.5f;
        if (distance(c, camPos) < 90.0f) nearBoxes.push_back(&b.box);
    }

    const u32 gs = m_seed * 31 + 7;
    auto hash2 = [&](i32 x, i32 z) -> u32 {
        u32 n = (u32)x * 374761393u + (u32)z * 668265263u + gs;
        n = (n ^ (n >> 13)) * 1274126177u;
        return n ^ (n >> 16);
    };

    const i32 bladesPerCell = densK >= 0.9f ? 2 : 1;
    for (i32 gz = -R; gz <= R; ++gz)
        for (i32 gx = -R; gx <= R; ++gx) {
            const f32 d2 = (f32)(gx * gx + gz * gz);
            if (d2 > (f32)R * (f32)R) continue;
            const u32 h = hash2(gx, gz);
            // jitter dentro de la celda
            const f32 jx = ((h & 0xFF) / 255.0f - 0.5f) * step;
            const f32 jz = (((h >> 8) & 0xFF) / 255.0f - 0.5f) * step;
            const f32 x = camPos.x + (f32)gx * step + jx;
            const f32 z = camPos.z + (f32)gz * step + jz;
            // altura y pendiente desde la rejilla
            const f32 hgt = gridHeight(x, z);
            if (hgt < 13.0f || hgt > 235.0f) continue;
            const f32 hx = gridHeight(x + 3.0f, z) - gridHeight(x - 3.0f, z);
            const f32 hz = gridHeight(x, z + 3.0f) - gridHeight(x, z - 3.0f);
            if (std::fabs(hx) + std::fabs(hz) > 3.4f) continue;   // pendiente
            // densidad por bioma (praderas humedas mas densas, secas mas ralas)
            const f32 moist = m_terrain.moistureAt(x, z);
            if (((h >> 16) & 0xFF) / 255.0f > 0.55f + moist * 0.45f) continue;
            // dentro de un edificio? -> fuera
            bool inside = false;
            const vec3 probe(x, hgt + 0.35f, z);
            for (const AABB* b : nearBoxes)
                if (b->contains(probe)) { inside = true; break; }
            if (inside) continue;

            const u32 h2 = hash2(gx * 3 + 11, gz * 7 - 5);
            for (i32 b = 0; b < bladesPerCell; ++b) {
                const u32 hh = hash2(gx * 5 + b, gz * 11 + b * 3);
                InstanceData inst;
                const f32 bx = x + (((hh & 0xFF) / 255.0f) - 0.5f) * 1.9f;
                const f32 bz = z + ((((hh >> 8) & 0xFF) / 255.0f) - 0.5f) * 1.9f;
                const f32 by = gridHeight(bx, bz);
                const f32 sc = 0.75f + ((h2 >> 24 & 0xFF) / 255.0f) * 0.8f;
                inst.model = glm::translate(mat4(1), vec3(bx, by - 0.05f, bz)) *
                             glm::rotate(mat4(1), (f32)(hh & 0xF) * 0.42f, vec3(0, 1, 0)) *
                             glm::scale(mat4(1), vec3(sc));
                // variacion de color: verde vivo -> seco por humedad
                const f32 dry = clamp01(1.15f - moist * 1.4f);
                const f32 vk = 0.85f + ((h2 & 0x7F) / 255.0f) * 0.35f;
                inst.color = vec4(mix(vec3(0.85f, 1.0f, 0.75f), vec3(1.15f, 0.95f, 0.55f), dry) * vk, 1.0f);
                m_grassInstances.push_back(inst);
            }
        }
}

// ---------------------------------------------------------------------------
// Utilidades GLB
// ---------------------------------------------------------------------------
mat4 Game::gltfPivot(const GltfModel& m, const vec3& pos, f32 yaw) {
    // centra el modelo y apoya su base (bmin.y) en y=0 de pos
    const vec3 c = (m.bounds.bmin + m.bounds.bmax) * 0.5f;
    const vec3 pivot(c.x, m.bounds.bmin.y, c.z);
    return glm::translate(mat4(1), pos) *
           glm::rotate(mat4(1), yaw, vec3(0, 1, 0)) *
           glm::translate(mat4(1), -pivot);
}

void Game::drawGltf(const GltfModel& m, const mat4& model, vec4 tint, f32 emissive) {
    for (const auto& p : m.prims) {
        Material mat = p.mat;
        mat.tint = vec4(mat.tint.r * tint.r, mat.tint.g * tint.g,
                        mat.tint.b * tint.b, mat.tint.a * tint.a);
        // levantamiento sutil: legibilidad en sombra (look brillante BR)
        mat.emissive += emissive + 0.06f;
        m_r->drawMesh(p.mesh, model, mat);
    }
}


void Game::rebuildCollision() {
    // reconstruye la lista dinamica: estaticos + construcciones del jugador
    m_collision.clearDynamic();
    for (const auto& b : m_staticBoxes) m_collision.addBox(b);
    for (usize i = 0; i < m_builds.size(); ++i) {
        const BuildPiece& p = m_builds[i];
        const auto [mn, mx] = buildAabb(p);
        if (p.kind == BuildKind::Ramp) {
            ColliderRamp r;
            r.box = AABB{mn, mx};
            r.dir = p.rot;
            m_collision.addRamp(r);
            // bloqueo lateral tenue: caja fina bajo la rampa no; caminable
        } else {
            m_collision.addBox({AABB{mn, mx}, ColliderOwner::Build, (u32)i, true, p.hp});
        }
    }
}

std::pair<vec3, vec3> Game::buildAabb(const BuildPiece& p) const {
    // celda: floor = y del terreno al momento de construir; guardamos cell.y como altura base
    const f32 gx = (f32)p.cell.x * CellSize;
    const f32 gz = (f32)p.cell.z * CellSize;
    const f32 gy = (f32)p.cell.y;
    switch (p.kind) {
        case BuildKind::Wall: {
            // pared vertical en el borde de la celda segun rot (0:+X,1:+Z,2:-X,3:-Z)
            if (p.rot % 2 == 0) {
                const f32 x = gx + (p.rot == 0 ? CellSize : 0.0f);
                return {{x - 0.2f, gy, gz}, {x + 0.2f, gy + CellSize, gz + CellSize}};
            }
            const f32 z = gz + (p.rot == 1 ? CellSize : 0.0f);
            return {{gx, gy, z - 0.2f}, {gx + CellSize, gy + CellSize, z + 0.2f}};
        }
        case BuildKind::Floor:
            return {{gx, gy - 0.2f, gz}, {gx + CellSize, gy, gz + CellSize}};
        case BuildKind::Ramp:
            return {{gx, gy, gz}, {gx + CellSize, gy + CellSize, gz + CellSize}};
        case BuildKind::Roof:
        default:
            return {{gx, gy, gz}, {gx + CellSize, gy + CellSize * 0.6f, gz + CellSize}};
    }
}

// ---------------------------------------------------------------------------
// Minimapa
// ---------------------------------------------------------------------------
void Game::bakeMinimap() {
    constexpr u32 M = 256;
    std::vector<u8> px(M * M * 4);
    const f32 worldPerTex = Terrain::Size / (f32)M;
    for (u32 ty = 0; ty < M; ++ty)
        for (u32 tx = 0; tx < M; ++tx) {
            const f32 x = -Terrain::Half + (f32)tx * worldPerTex;
            const f32 z = -Terrain::Half + (f32)ty * worldPerTex;
            const f32 h = m_terrain.height(x, z);
            vec3 c;
            if (h < 0.5f) {
                c = mix(vec3(0.10f, 0.30f, 0.55f), vec3(0.16f, 0.45f, 0.62f),
                        clamp01((h + 40.0f) / 40.0f));
            } else {
                const vec3 n = m_terrain.normal(x, z);
                c = m_terrain.biomeColor(x, z, h, 1.0f - n.y, m_terrain.moistureAt(x, z));
                c *= 0.75f + clamp01(n.y) * 0.35f;
            }
            const usize i = ((usize)ty * M + tx) * 4;
            px[i] = (u8)clamp01(c.r) * 255; px[i + 1] = (u8)clamp01(c.g) * 255;
            px[i + 2] = (u8)clamp01(c.b) * 255; px[i + 3] = 255;
        }
    m_minimapTex.create2D(M, M, TexFormat::RGBA8, px.data(), false, true);
    m_minimapReady = true;
}

// ---------------------------------------------------------------------------
// Partida
// ---------------------------------------------------------------------------
void Game::startMatch() {
    // autobus: cruza la isla pasando sobre 2 POIs (para ver ciudades al saltar)
    Random rng(m_seed + (u32)m_time);
    const auto& pois = m_terrain.pois();
    const auto& pa = pois[rng.rangeU32((u32)pois.size())];
    const auto* pb = &pa;
    while (pb == &pa) pb = &pois[rng.rangeU32((u32)pois.size())];
    m_busDir = normalize(vec3(pb->pos.x - pa.pos.x, 0, pb->pos.y - pa.pos.y));
    m_busPos = vec3(pa.pos.x - m_busDir.x * 1500.0f, 380.0f, pa.pos.y - m_busDir.z * 1500.0f);

    m_playerPos = m_busPos;
    m_playerVel = vec3(0);
    m_gliding = false;
    m_falling = false;
    m_dropT = 0;
    m_hp = 100; m_shield = 0; m_kills = 0;
    m_wood = 60;               // un poco de material para probar construccion
    m_shieldPots = 1;
    for (auto& s : m_slots) s = WeaponInstance{};
    m_slots[0] = { WeaponKind::Pickaxe, 0, 0 };
    m_activeSlot = 0;
    m_buildMode = false;
    m_builds.clear();
    for (auto& c : m_chests) c.opened = false;
    for (auto& l : m_loot) l.taken = false;
    for (usize i = 0; i < m_treeData.size(); ++i) m_treeAlive[i] = true;

    // bots: distribuidos en POIs y campos
    for (auto& bot : m_bots) {
        bot.alive = true;
        bot.hp = 100;
        bot.state = 0;
        if (rng.nextF32() < 0.75f) {
            const auto& poi = pois[rng.rangeU32((u32)pois.size())];
            const f32 aa = rng.nextF32() * TAU_F, rr = rng.nextF32() * poi.radius;
            bot.pos = vec3(poi.pos.x + std::cos(aa) * rr, 0, poi.pos.y + std::sin(aa) * rr);
        } else {
            bot.pos = vec3(rng.symmetric() * 1600.0f, 0, rng.symmetric() * 1600.0f);
        }
        bot.pos.y = m_terrain.height(bot.pos.x, bot.pos.z);
        bot.wanderTarget = vec2(bot.pos.x, bot.pos.z);
        bot.weapon = (WeaponKind)(1 + rng.rangeU32(4));
        bot.weaponRarity = (u8)rng.rangeU32(5);
        bot.fireCd = 1; bot.reactT = 0;
    }

    // tormenta
    m_stormCenter = vec2(0, 0);
    m_stormRadius = 2300.0f;
    m_stormTargetRadius = 2300.0f;
    m_stormPhase = 0;
    m_stormTimer = 15.0f;
    m_stormShrinking = false;
    m_stormDps = 1.0f;
    m_stormTarget = vec2(0, 0);

    m_killFeed.clear();
    rebuildCollision();
    m_state = State::Drop;
    ++m_matches;
}

void Game::respawn() { startMatch(); }

i32 Game::aliveCount() const {
    i32 n = 1;
    for (const auto& b : m_bots) if (b.alive) ++n;
    return n;
}

// ---------------------------------------------------------------------------
// Tick principal
// ---------------------------------------------------------------------------
void Game::tick(f32 dt) {
    m_time += dt;
    m_env.time = m_time;
    m_hurtFlash = std::max(0.0f, m_hurtFlash - dt * 1.6f);
    m_hitMarker = std::max(0.0f, m_hitMarker - dt * 3.0f);
    m_interactHint = std::max(0.0f, m_interactHint - dt);
    for (auto it = m_killFeed.begin(); it != m_killFeed.end();) {
        it->t -= dt;
        if (it->t <= 0) it = m_killFeed.erase(it); else ++it;
    }

    switch (m_state) {
        case State::Menu:   updateMenu(dt);   break;
        case State::Drop:   updateDrop(dt);   break;
        case State::Playing: updatePlaying(dt); break;
        case State::Dead:   m_deathT += dt; updateBots(dt); updateStorm(dt); updateParticles(dt); break;
        case State::Victory: updateParticles(dt); break;
        default: break;
    }
}

void Game::updateMenu(f32 dt) {
    // rotacion panoramica de camara sobre la isla, mirando SIEMPRE al centro
    const f32 a = m_time * 0.05f;
    m_camera.pos = vec3(std::cos(a) * 950.0f, 300.0f, std::sin(a) * 950.0f);
    // forward=(sin yaw, ., cos yaw) debe apuntar de la camara al origen:
    // sin(yaw)=-cos(a), cos(yaw)=-sin(a)  ->  yaw = -a - pi/2
    m_camera.yaw = -a - PI_F * 0.5f;
    m_camera.pitch = -0.34f;
    m_camera.aspect = (f32)m_r->viewW() / (f32)m_r->viewH();
    m_camera.update();
    (void)dt;
}

void Game::updateDrop(f32 dt) {
    m_dropT += dt;
    // el autobus avanza
    m_busPos += m_busDir * 55.0f * dt;

    InputState& in = input();
    // mirada libre durante la caida
    m_playerYaw   -= in.mouseDX * in.sensitivity * in.mouseSensMult;
    m_playerPitch -= in.mouseDY * in.sensitivity * in.mouseSensMult;
    m_playerPitch = clamp(m_playerPitch, -1.45f, 1.45f);
    m_camera.yaw = m_playerYaw;
    m_camera.pitch = m_playerPitch;

    const bool jump = in.keyPressed(GLFW_KEY_SPACE);

    if (!m_falling) {
        // pegado al Carguero Nube; solo se puede saltar sobre la isla
        m_playerPos = m_busPos + vec3(0, -2.5f, 0);
        m_playerVel = m_busDir * 55.0f;
        const f32 distCenter = length(vec2(m_busPos.x, m_busPos.z));
        const f32 groundHere = m_terrain.height(m_busPos.x, m_busPos.z);
        const bool overIsland = distCenter < 1250.0f && groundHere > 5.0f;
        if ((jump && overIsland) || distCenter > Terrain::Half * 0.98f || m_dropT > 26.0f) {
            m_falling = true;
            audioSystem().play(Sfx::Jump, 0.8f);
        }
        // L = aterrizaje rapido (pruebas / saltarse la caida): cae EN el POI
        // mas cercano (ciudad con cofres y loot garantizados)
        if (in.keyPressed(GLFW_KEY_L) || m_debugLandRequested) {
            m_debugLandRequested = false;
            m_falling = true;
            m_gliding = true;
            const auto& pois = m_terrain.pois();
            const PoiDef* best = &pois[0];
            f32 bestD = 1.0e9f;
            for (const auto& poi : pois) {
                const f32 d = distance(vec2(m_playerPos.x, m_playerPos.z), poi.pos);
                if (d < bestD) { bestD = d; best = &poi; }
            }
            const f32 a = (f32)((m_seed ^ (u32)m_time) % 97) / 97.0f * TAU_F;
            m_playerPos.x = best->pos.x + std::cos(a) * best->radius * 0.30f;
            m_playerPos.z = best->pos.y + std::sin(a) * best->radius * 0.30f;
            m_playerPos.y = m_terrain.height(m_playerPos.x, m_playerPos.z) + 60.0f;
        }
    } else {
        // caida / planeador
        const f32 ground = m_terrain.height(m_playerPos.x, m_playerPos.z);
        const f32 alt = m_playerPos.y - ground;
        if (!m_gliding && alt < 170.0f) {
            m_gliding = true;
            audioSystem().play(Sfx::Glide, 0.9f);
        }
        const f32 maxFall = m_gliding ? 8.0f : 42.0f;
        m_playerVel.y = std::max(m_playerVel.y - Gravity * dt, -maxFall);
        // control horizontal del planeador
        f32 fwd = 0, strafe = 0;
        if (in.keyDown(GLFW_KEY_W)) fwd += 1;
        if (in.keyDown(GLFW_KEY_S)) fwd -= 1;
        if (in.keyDown(GLFW_KEY_A)) strafe -= 1;
        if (in.keyDown(GLFW_KEY_D)) strafe += 1;
        const f32 speed = m_gliding ? 13.0f : 4.0f;
        const vec3 f = m_camera.forward(), r = m_camera.right();
        const vec3 want = (f * fwd + r * strafe) * speed;
        m_playerVel.x = want.x; m_playerVel.z = want.z;
        if (m_gliding) m_playerVel.x += m_camera.forward().x * 9.0f,
                       m_playerVel.z += m_camera.forward().z * 9.0f;
        m_playerPos += m_playerVel * dt;

        if (m_playerPos.y <= ground + 0.1f) {
            m_playerPos.y = ground;
            m_falling = false;
            m_gliding = false;
            m_playerVel = vec3(0);
            m_state = State::Playing;
            m_onGround = true;
            audioSystem().play(Sfx::Land, 1.0f);
        }
    }
    // camara sigue la caida (por debajo del carguero para no clipear la gondola)
    m_playerYaw = m_camera.yaw;   // orientacion del cuerpo = camara
    m_camera.pos = m_playerPos + vec3(0, m_falling ? 1.5f : -1.0f, 0) - m_camera.forward() * 6.0f;
    m_camera.pos.y = std::max(m_camera.pos.y, m_terrain.height(m_camera.pos.x, m_camera.pos.z) + 1.5f);
    m_camera.update();

    // los bots ya estan en el suelo (simplificacion)
    updateBots(dt);
    updateStorm(dt);
    updateParticles(dt);
}

void Game::updatePlaying(f32 dt) {
    updatePlayer(dt);
    updateBots(dt);
    updateStorm(dt);
    updateBuild(dt);
    updateParticles(dt);
    updateCamera(dt);
    botsDiedCheck();
}

void Game::updatePlayer(f32 dt) {
    InputState& in = input();

    // --- mirada ---------------------------------------------------------------
    m_playerYaw   -= in.mouseDX * in.sensitivity * in.mouseSensMult;
    m_playerPitch -= in.mouseDY * in.sensitivity * in.mouseSensMult;
    m_playerPitch = clamp(m_playerPitch, -1.45f, 1.45f);
    m_ads = in.mouseDownB(GLFW_MOUSE_BUTTON_RIGHT);

    // --- movimiento -----------------------------------------------------------
    f32 fwd = 0, strafe = 0;
    if (in.keyDown(GLFW_KEY_W)) fwd += 1;
    if (in.keyDown(GLFW_KEY_S)) fwd -= 1;
    if (in.keyDown(GLFW_KEY_A)) strafe -= 1;
    if (in.keyDown(GLFW_KEY_D)) strafe += 1;
    m_sprinting = in.keyDown(GLFW_KEY_LEFT_SHIFT) && fwd > 0.5f && !m_ads;
    const f32 speed = (m_ads ? WalkSpeed * 0.55f :
                      (m_sprinting ? SprintSpeed : WalkSpeed)) * (m_buildMode ? 1.0f : 1.0f);
    const vec3 f(std::sin(m_playerYaw), 0, std::cos(m_playerYaw));
    const vec3 r(f.z, 0, -f.x);
    vec3 move = f * fwd + r * strafe;
    if (length(move) > 0.01f) move = normalize(move) * speed;

    // salto / gravedad
    const f32 ground = m_collision.groundHeight(m_playerPos.x, m_playerPos.z, m_playerPos.y);
    if (m_onGround && in.keyPressed(GLFW_KEY_SPACE)) {
        m_playerVel.y = JumpSpeed;
        m_onGround = false;
        audioSystem().play(Sfx::Jump, 0.5f, 1.3f);
    }
    if (!m_onGround) {
        m_playerVel.y -= Gravity * dt;
    }

    // colision horizontal
    m_collision.moveCapsule(m_playerPos, move * dt, PlayerRadius, PlayerHeight);

    // vertical
    m_playerPos.y += m_playerVel.y * dt;
    const f32 gh = m_collision.groundHeight(m_playerPos.x, m_playerPos.z, m_playerPos.y);
    if (m_playerPos.y <= gh + 0.02f) {
        if (m_playerVel.y < -14.0f) audioSystem().play(Sfx::Land, clamp01(-m_playerVel.y / 20.0f));
        m_playerPos.y = gh;
        m_playerVel.y = 0;
        m_onGround = true;
    } else if (m_playerPos.y > gh + 0.25f) {
        m_onGround = false;
    }

    // tormenta: dano si estas fuera
    const f32 dc = distance(vec2(m_playerPos.x, m_playerPos.z), m_stormCenter);
    if (dc > m_stormRadius) {
        m_stormDmgT += dt;
        if (m_stormDmgT > 1.0f) {
            m_stormDmgT = 0;
            damagePlayer((i32)m_stormDps);
            audioSystem().play(Sfx::StormTick, 0.6f);
            emitParticle(m_playerPos + vec3(0, 1, 0), vec3(0, 2, 0), 0.5f, 0.6f,
                         vec4(0.6f, 0.3f, 0.9f, 0.8f), true);
        }
    }

    // --- armas --------------------------------------------------------------
    updateCamera(dt);   // orienta la camara antes de disparar
    const WeaponInstance& wi = m_slots[m_activeSlot];
    const WeaponDef wd = weaponDef(wi.kind);
    m_fireCd -= dt;
    m_harvestCd -= dt;

    // seleccion de slots
    for (i32 k = 0; k < 5; ++k)
        if (in.keyPressed(GLFW_KEY_1 + k)) { m_activeSlot = k; audioSystem().play(Sfx::Click, 0.5f); }
    if (in.mousePressedB(GLFW_MOUSE_BUTTON_LEFT) && !m_buildMode) {
        // click en menu no: capturado
    }
    // cambiar a pico con F
    if (in.keyPressed(GLFW_KEY_F)) { m_activeSlot = 0; audioSystem().play(Sfx::Click, 0.5f); }

    // recargar
    if (in.keyPressed(GLFW_KEY_R) && wi.valid() && wi.ammo < wd.magSize) {
        m_reloading = 1.4f;
        audioSystem().play(Sfx::Click, 0.7f, 0.7f);
    }
    if (m_reloading > 0) {
        m_reloading -= dt;
        if (m_reloading <= 0) {
            m_slots[m_activeSlot].ammo = wd.magSize;
            audioSystem().play(Sfx::Click, 0.9f, 1.2f);
        }
    }

    // pocion de escudo (H)
    if (in.keyPressed(GLFW_KEY_H) && m_shieldPots > 0 && m_shield < 100) {
        --m_shieldPots;
        m_healT = 2.0f;
        audioSystem().play(Sfx::Heal, 0.9f);
    }
    if (m_healT > 0) {
        m_healT -= dt;
        m_shield = (i32)clamp((f32)m_shield + dt * 15.0f, 0.0f, 100.0f);
    }

    // disparo
    if (!m_buildMode && in.mouseDownB(GLFW_MOUSE_BUTTON_LEFT) && m_fireCd <= 0 && m_reloading <= 0) {
        if (wi.kind == WeaponKind::Pickaxe) {
            if (m_harvestCd <= 0) {
                m_harvestCd = wd.fireInterval;
                const vec3 dir = m_camera.forward();
                RaycastHit hit;
                if (m_collision.raycast(m_camera.pos + dir * 0.5f, dir, wd.range, hit))
                    harvestHit(hit);
                m_fireCd = wd.fireInterval;
                m_pickAnim = 0.3f;
            }
        } else if (wi.ammo > 0) {
            fireWeapon(m_camera.pos, m_camera.forward());
            m_fireCd = wd.fireInterval;
            m_slots[m_activeSlot].ammo--;
            if (wi.kind == WeaponKind::Sniper) audioSystem().play(Sfx::ShootSniper, 0.9f);
            else if (wi.kind == WeaponKind::Shotgun) audioSystem().play(Sfx::ShootHeavy, 0.85f);
            else audioSystem().play(Sfx::Shoot, 0.75f, wi.kind == WeaponKind::SMG ? 1.2f : 1.0f);
            // retroceso visual
            m_playerPitch += wd.spread * 0.4f;
        } else {
            audioSystem().play(Sfx::Click, 0.5f, 0.6f);
            m_fireCd = 0.3f;
        }
    }
    if (m_pickAnim > 0) m_pickAnim -= dt;

    // interaccion (E)
    if (in.keyPressed(GLFW_KEY_E)) tryInteract();
    // prompt de cofre cercano
    m_interactText.clear();
    for (const auto& c : m_chests) {
        if (c.opened) continue;
        if (distance(c.pos, m_playerPos) < 3.4f) {
            m_interactText = "[E] Abrir cofre";
            m_interactHint = 0.2f;
            break;
        }
    }
    if (in.keyPressed(GLFW_KEY_Q)) { m_buildMode = !m_buildMode; audioSystem().play(Sfx::Click, 0.7f); }
    if (m_buildMode) {
        if (in.keyPressed(GLFW_KEY_Z)) m_buildKind = BuildKind::Wall;
        if (in.keyPressed(GLFW_KEY_X)) m_buildKind = BuildKind::Floor;
        if (in.keyPressed(GLFW_KEY_C)) m_buildKind = BuildKind::Ramp;
        if (in.keyPressed(GLFW_KEY_V)) m_buildKind = BuildKind::Roof;
        if (in.mousePressedB(GLFW_MOUSE_BUTTON_LEFT)) placeBuild();
    }
    pickLoot();
}

void Game::updateCamera(f32) {
    const f32 shoulder = m_ads ? 0.28f : 0.55f;
    const f32 dist = m_ads ? 1.6f : 3.4f;
    const vec3 f = m_camera.forward();   // ya usa yaw/pitch? -> mantener coherente
    // orientacion de camara desde yaw/pitch del jugador
    m_camera.yaw = m_playerYaw;
    m_camera.pitch = m_playerPitch;
    m_camera.update();
    const vec3 fw = m_camera.forward(), rt = m_camera.right();
    vec3 eye = m_playerPos + vec3(0, 1.68f, 0) + rt * shoulder - fw * dist;
    // no atravesar el terreno
    const f32 th = m_terrain.height(eye.x, eye.z);
    if (eye.y < th + 0.35f) eye.y = th + 0.35f;
    m_camera.pos = eye;
    m_camera.update();
}

// ---------------------------------------------------------------------------
// Tormenta
// ---------------------------------------------------------------------------
void Game::updateStorm(f32 dt) {
    m_stormTimer -= dt;
    static constexpr f32 Radii[] = { 1300.0f, 850.0f, 500.0f, 280.0f, 140.0f, 50.0f };
    static constexpr f32 Dps[] = { 1.0f, 2.0f, 5.0f, 8.0f, 10.0f, 12.0f };
    static constexpr f32 WaitT[] = { 15.0f, 18.0f, 16.0f, 14.0f, 12.0f, 10.0f };
    static constexpr f32 ShrinkT[] = { 26.0f, 24.0f, 20.0f, 17.0f, 14.0f, 11.0f };

    if (!m_stormShrinking) {
        if (m_stormTimer <= 0 && m_stormPhase < 6) {
            m_stormShrinking = true;
            m_stormTimer = ShrinkT[m_stormPhase];
            m_stormTargetRadius = Radii[m_stormPhase];
            // nuevo centro dentro del circulo actual
            Random rng(m_seed + m_stormPhase * 31 + 7);
            const f32 a = rng.nextF32() * TAU_F;
            const f32 r = rng.nextF32() * (m_stormRadius - m_stormTargetRadius) * 0.8f;
            m_stormTarget = m_stormCenter + vec2(std::cos(a), std::sin(a)) * r;
            m_stormFrom = m_stormCenter;
            m_stormFromR = m_stormRadius;
            m_stormDps = Dps[m_stormPhase];
            addKillFeed("La tormenta avanza");
        }
    } else {
        const f32 t = 1.0f - clamp01(m_stormTimer / ShrinkT[m_stormPhase]);
        m_stormCenter = mix(m_stormFrom, m_stormTarget, t);
        m_stormRadius = lerpT(m_stormFromR, m_stormTargetRadius, t);
        if (m_stormTimer <= 0) {
            m_stormShrinking = false;
            ++m_stormPhase;
            m_stormTimer = m_stormPhase < 6 ? WaitT[m_stormPhase] : 999.0f;
        }
    }
    // audio ambiental ocasional si estas cerca del borde
    const f32 dc = distance(vec2(m_playerPos.x, m_playerPos.z), m_stormCenter);
    if (dc > m_stormRadius * 0.88f && dc < m_stormRadius * 1.15f) {
        m_stormSoundT -= dt;
        if (m_stormSoundT <= 0) {
            m_stormSoundT = 3.0f + (f32)(m_seed % 3);
            audioSystem().play(Sfx::Storm, 0.35f);
        }
    }
}

// ---------------------------------------------------------------------------
// Disparos
// ---------------------------------------------------------------------------
void Game::fireWeapon(const vec3& origin, const vec3& dir) {
    const WeaponInstance& wi = m_slots[m_activeSlot];
    const WeaponDef wd = weaponDef(wi.kind);
    const f32 spread = m_ads ? wd.adsSpread : wd.spread;
    Random rng((u32)(m_time * 1000.0f));

    // fogonazo
    emitParticle(origin + dir * 1.2f, dir * 2.0f, 0.06f, 0.35f,
                 vec4(1.0f, 0.8f, 0.4f, 1.0f), true);

    for (i32 p = 0; p < wd.pellets; ++p) {
        vec3 d = dir;
        if (wd.pellets > 1 || spread > 0) {
            d = normalize(d + vec3(rng.symmetric() * spread,
                                   rng.symmetric() * spread,
                                   rng.symmetric() * spread));
        }
        RaycastHit hit;
        const vec3 o = origin + d * 0.4f;
        // impacto contra bots primero: raycast esfera por bot cercano
        f32 bestT = wd.range;
        Bot* hitBot = nullptr;
        bool head = false;
        for (auto& bot : m_bots) {
            if (!bot.alive) continue;
            const vec3 to = bot.pos + vec3(0, 0.9f, 0) - o;
            const f32 t = dot(to, d);
            if (t < 0 || t > bestT) continue;
            const vec3 closest = o + d * t;
            const vec3 cBody = bot.pos + vec3(0, 0.9f, 0);
            const vec3 cHead = bot.pos + vec3(0, 1.72f, 0);
            if (distance(closest, cHead) < 0.26f) { bestT = t; hitBot = &bot; head = true; }
            else if (distance(closest, cBody) < 0.62f) { bestT = t; hitBot = &bot; head = false; }
        }
        // mundo
        RaycastHit world;
        const bool hitWorld = m_collision.raycast(o, d, bestT, world);
        if (hitWorld && world.t < bestT) {
            bestT = world.t;
            hitBot = nullptr;
        }
        if (hitBot) {
            const i32 dmg = (i32)(wd.damage * WeaponDamageMult[wi.rarity] * (head ? wd.headMult : 1.0f));
            damageBot(*hitBot, dmg, head, true);
            emitParticle(o + d * bestT, -d * 1.5f + vec3(0, 1, 0), 0.25f, 0.28f,
                         vec4(1, 0.85f, 0.3f, 0.9f), true);
            m_hitMarker = 0.25f;
            audioSystem().play(head ? Sfx::Headshot : Sfx::Hit, 0.7f);
        } else if (hitWorld) {
            if (world.type == HitType::Tree) {
                emitParticle(world.point, world.normal * 2.0f + vec3(0, 1.5f, 0), 0.4f, 0.22f,
                             vec4(0.5f, 0.35f, 0.15f, 1), false);
                damageTree(world.id, wd.damage * 0.7f);
            } else if (world.type == HitType::Box && world.breakable) {
                damageBuildPiece(world.id, (i32)wd.damage);
                emitParticle(world.point, world.normal * 2.0f, 0.3f, 0.25f,
                             vec4(0.7f, 0.55f, 0.3f, 1), false);
                audioSystem().play(Sfx::HitWood, 0.6f);
            } else {
                emitParticle(world.point, world.normal * 2.0f + vec3(0, 1, 0), 0.3f, 0.2f,
                             vec4(0.8f, 0.8f, 0.75f, 0.9f), false);
            }
            // tracer
            emitParticle(o, d * 90.0f, 0.08f, 0.05f,
                         vec4(1, 0.95f, 0.6f, 0.9f), true, 1.0f);
        } else {
            emitParticle(o, d * 90.0f, 0.08f, 0.05f, vec4(1, 0.95f, 0.6f, 0.9f), true, 1.0f);
        }
    }
}

void Game::harvestHit(const RaycastHit& hit) {
    if (hit.type == HitType::Tree) {
        damageTree(hit.id, 34);
        m_wood += 12;
        audioSystem().play(Sfx::Chop, 0.9f);
        emitParticle(hit.point, hit.normal * 3.0f + vec3(0, 2, 0), 0.5f, 0.3f,
                     vec4(0.55f, 0.4f, 0.18f, 1), false);
    } else if (hit.type == HitType::Box && hit.breakable) {
        damageBuildPiece(hit.id, 55);
        m_wood += 8;
        audioSystem().play(Sfx::Chop, 0.8f, 0.8f);
    } else if (hit.type == HitType::Terrain) {
        audioSystem().play(Sfx::Chop, 0.5f, 0.5f);
        emitParticle(hit.point, hit.normal * 2.5f + vec3(0, 2, 0), 0.4f, 0.25f,
                     vec4(0.6f, 0.5f, 0.35f, 1), false);
        m_wood += 4;
    } else {
        audioSystem().play(Sfx::Chop, 0.4f, 0.6f);
    }
}

void Game::damageTree(u32 id, f32 dmg) {
    if (id >= m_treeData.size() || !m_treeAlive[id]) return;
    m_treeHp[id] -= dmg;
    if (m_treeHp[id] <= 0) {
        m_treeAlive[id] = false;
        m_wood += 18;
        audioSystem().play(Sfx::BuildBreak, 0.8f);
        const vec4& t = m_treeData[id];
        for (i32 i = 0; i < 10; ++i)
            emitParticle(vec3(t.x, t.y + 2 + (f32)(i % 3), t.z),
                         vec3((f32)(i % 5 - 2), 3.0f, (f32)(i / 5 - 1)) * 1.2f,
                         0.7f, 0.4f, vec4(0.5f, 0.38f, 0.16f, 1), false);
    }
}

void Game::damageBuildPiece(u32 id, i32 dmg) {
    if (id >= m_builds.size()) return;
    m_builds[id].hp -= dmg;
    if (m_builds[id].hp <= 0) {
        m_builds.erase(m_builds.begin() + id);
        rebuildCollision();
        audioSystem().play(Sfx::BuildBreak, 0.9f);
    }
}

void Game::damageBot(Bot& bot, i32 dmg, bool headshot, bool byPlayer) {
    bot.hp -= dmg;
    bot.state = 1;   // combate
    if (bot.hp <= 0 && bot.alive) {
        bot.alive = false;
        for (i32 i = 0; i < 8; ++i)
            emitParticle(bot.pos + vec3(0, 1, 0),
                         vec3((f32)(i % 3 - 1) * 2.0f, (f32)(i / 3) * 2.0f + 1, (f32)(i % 2 - 0.5f) * 2.0f),
                         0.6f, 0.35f, vec4(0.95f, 0.9f, 0.3f, 1), true);
        // suelta su arma
        if (byPlayer) {
            ++m_kills;
            audioSystem().play(Sfx::Kill, 0.9f);
            addKillFeed(format("Tu eliminaste a Bot-%03d %s", (i32)(&bot - m_bots.data()) + 1,
                               headshot ? "(cabeza)" : ""));
        } else {
            addKillFeed(format("Bot-%03d cayo en combate", (i32)(&bot - m_bots.data()) + 1));
        }
    }
}

void Game::damagePlayer(i32 dmg) {
    if (m_state != State::Playing) return;
    const i32 shieldDmg = std::min(m_shield, dmg);
    m_shield -= shieldDmg;
    m_hp -= (dmg - shieldDmg);
    m_hurtFlash = 1.0f;
    if (m_hp <= 0) {
        m_hp = 0;
        m_state = State::Dead;
        m_deathT = 0;
        audioSystem().play(Sfx::Death, 1.0f);
        addKillFeed("Fuiste eliminado");
        input().mouseCaptured = false;
    }
}

// ---------------------------------------------------------------------------
// Bots
// ---------------------------------------------------------------------------
void Game::updateBots(f32 dt) {
    const bool playerAlive = m_state == State::Playing;
    Random rng((u32)(m_time * 61.0f) + 5);

    for (usize bi = 0; bi < m_bots.size(); ++bi) {
        Bot& bot = m_bots[bi];
        if (!bot.alive) continue;
        const f32 distToPlayer = distance(bot.pos, m_playerPos);
        const bool nearSim = distToPlayer < 240.0f && (playerAlive || distToPlayer < 120.0f);

        // tormenta
        const f32 dc = distance(vec2(bot.pos.x, bot.pos.z), m_stormCenter);
        const bool inStorm = dc > m_stormRadius;
        if (inStorm) {
            bot.hp -= (i32)(m_stormDps * dt);
            if (bot.hp <= 0) {
                bot.alive = false;
                addKillFeed(format("La tormenta elimino a Bot-%03d", (i32)bi + 1));
                continue;
            }
        }

        if (!nearSim) {
            // simulacion gruesa: moverse hacia la zona si fuera; duelo aleatorio
            if (inStorm) {
                const vec2 toC = normalize(m_stormCenter - vec2(bot.pos.x, bot.pos.z));
                bot.pos.x += toC.x * 7.0f * dt;
                bot.pos.z += toC.y * 7.0f * dt;
                bot.pos.y = m_terrain.height(bot.pos.x, bot.pos.z);
            } else if (rng.nextF32() < 0.15f * dt) {
                const auto& pois = m_terrain.pois();
                bot.wanderTarget = pois[rng.rangeU32((u32)pois.size())].pos;
            } else {
                const vec2 to = bot.wanderTarget - vec2(bot.pos.x, bot.pos.z);
                if (length(to) > 4.0f) {
                    const vec2 d = normalize(to);
                    bot.pos.x += d.x * 4.5f * dt;
                    bot.pos.z += d.y * 4.5f * dt;
                    bot.pos.y = m_terrain.height(bot.pos.x, bot.pos.z);
                    bot.yaw = std::atan2(d.x, d.y);
                }
            }
            // duelos bot vs bot (estadisticos)
            bot.noiseT -= dt;
            if (bot.noiseT <= 0) {
                bot.noiseT = 2.0f + rng.nextF32() * 3.0f;
                // probabilidad de ser eliminado decreciente segun quedan vivos
                const f32 p = 0.012f * (f32)aliveCount() / 100.0f;
                if (rng.nextF32() < p) {
                    bot.alive = false;
                    addKillFeed(format("Bot-%03d elimino a Bot-%03d",
                                       (i32)(rng.rangeU32((u32)m_bots.size()) + 1), (i32)bi + 1));
                }
            }
            continue;
        }

        // simulacion completa (cerca del jugador)
        // estado: combate si el jugador esta vivo y cerca
        const bool engage = playerAlive && distToPlayer < 75.0f;
        vec3 move(0);
        if (engage) {
            bot.state = 1;
            const vec3 toP = m_playerPos - bot.pos;
            const f32 d = length(toP);
            bot.targetYaw = std::atan2(toP.x, toP.z);
            // linea de vision
            const vec3 eye = bot.pos + vec3(0, 1.6f, 0);
            const vec3 dir = normalize(m_playerPos + vec3(0, 1.2f, 0) - eye);
            RaycastHit rh;
            const bool los = !m_collision.raycast(eye, dir, d - 1.0f, rh);
            if (los) {
                bot.reactT -= dt;
                if (bot.reactT <= 0) {
                    // strafe + acercarse si lejos
                    const vec3 strafe(dir.z, 0, -dir.x);
                    bot.strafeT -= dt;
                    if (bot.strafeT <= 0) { bot.strafeDir = rng.symmetric() > 0 ? 1.0f : -1.0f; bot.strafeT = 1.2f; }
                    move = strafe * bot.strafeDir * 4.0f;
                    if (d > 28.0f) move += dir * 5.5f;
                    else if (d < 10.0f) move -= dir * 3.0f;
                    botShoot(bot, m_playerPos);
                }
            } else {
                bot.reactT = 0.3f;
                move = dir * 4.0f;
            }
        } else {
            bot.state = inStorm ? 2 : 0;
            vec2 target = bot.wanderTarget;
            if (inStorm) target = m_stormCenter;
            else if (rng.nextF32() < 0.1f * dt) {
                const auto& pois = m_terrain.pois();
                bot.wanderTarget = pois[rng.rangeU32((u32)pois.size())].pos;
            }
            const vec2 to = target - vec2(bot.pos.x, bot.pos.z);
            if (length(to) > 3.0f) {
                const vec2 dn = normalize(to);
                move = vec3(dn.x, 0, dn.y) * 4.2f;
                bot.targetYaw = std::atan2(dn.x, dn.y);
            }
        }

        // integrar con colision
        m_collision.moveCapsule(bot.pos, move * dt, 0.4f, 1.8f);
        const f32 gh = m_collision.groundHeight(bot.pos.x, bot.pos.z, bot.pos.y);
        bot.pos.y = gh;   // pegado al suelo (sin fisica vertical para bots)
        // suaviza yaw
        f32 dy = wrapAngle(bot.targetYaw - bot.yaw);
        bot.yaw += clamp(dy, -4.0f * dt, 4.0f * dt);
    }
}

void Game::botShoot(Bot& bot, const vec3& target) {
    const WeaponDef wd = weaponDef(bot.weapon);
    bot.fireCd -= 0.02f;
    if (bot.fireCd > 0) return;
    bot.fireCd = wd.fireInterval * 1.8f;
    const f32 dist = distance(bot.pos, target);
    const vec3 dir = normalize(target + vec3(0, 1.1f, 0) - (bot.pos + vec3(0, 1.5f, 0)));
    const f32 miss = 0.055f + clamp01((dist - 15.0f) / 90.0f) * 0.10f;
    Random rng((u32)(m_time * 977.0f) + (u32)(&bot - m_bots.data()));
    const bool hits = rng.nextF32() > clamp01(miss * 1.35f);
    audioSystem().play(Sfx::Shoot, clamp01(0.5f - dist / 200.0f),
                       0.8f + rng.nextF32() * 0.3f);
    emitParticle(bot.pos + vec3(0, 1.5f, 0) + dir * 0.8f, dir * 2.0f, 0.06f, 0.3f,
                 vec4(1, 0.75f, 0.35f, 1), true);
    if (hits) {
        damagePlayer((i32)(wd.damage * (0.5f + rng.nextF32() * 0.5f)));
    }
}

// ---------------------------------------------------------------------------
// Interaccion / loot / construccion
// ---------------------------------------------------------------------------
void Game::tryInteract() {
    // cofre mas cercano delante
    const vec3 fwd = m_camera.forward();
    for (auto& c : m_chests) {
        if (c.opened) continue;
        const vec3 to = c.pos + vec3(0, 0.5f, 0) - (m_playerPos + vec3(0, 1.2f, 0));
        if (length(to) < 3.2f && dot(normalize(to), fwd) > 0.4f) {
            c.opened = true;
            audioSystem().play(Sfx::Chest, 1.0f);
            // premio: arma + pociones + madera
            Random rng((u32)(m_time * 733.0f) + 1);
            WeaponInstance w;
            w.kind = (WeaponKind)(1 + rng.rangeU32(4));
            w.rarity = (u8)clamp(rng.rangeU32(6), 0u, 4u);
            w.ammo = 999;
            pickUpWeapon(w);
            m_shieldPots += 1 + (i32)rng.rangeU32(2);
            m_wood += 30;
            for (i32 i = 0; i < 12; ++i)
                emitParticle(c.pos + vec3(0, 0.8f, 0),
                             vec3(rng.symmetric() * 2, 2.5f + rng.nextF32() * 2, rng.symmetric() * 2),
                             0.9f, 0.2f, vec4(1, 0.85f, 0.3f, 1), true);
            return;
        }
    }
}

void Game::pickUpWeapon(const WeaponInstance& w) {
    // hueco libre, o reemplaza el slot activo (no el pico)
    for (i32 i = 1; i < 5; ++i)
        if (!m_slots[i].valid()) {
            m_slots[i] = w;
            audioSystem().play(Sfx::Pickup, 0.9f);
            return;
        }
    const i32 slot = std::max(1, m_activeSlot);
    m_slots[slot] = w;
    m_activeSlot = slot;
    audioSystem().play(Sfx::Pickup, 0.9f);
}

void Game::pickLoot() {
    for (auto& l : m_loot) {
        if (l.taken) continue;
        if (distance(l.pos, m_playerPos) < 2.0f) {
            l.taken = true;
            pickUpWeapon(l.weapon);
            emitParticle(l.pos, vec3(0, 2, 0), 0.4f, 0.4f, vec4(0.4f, 1, 0.5f, 1), true);
        }
    }
}

BuildPiece Game::ghostPiece() const {
    // celda frente al jugador
    const vec3 f(std::sin(m_playerYaw), 0, std::cos(m_playerYaw));
    const vec3 p = m_playerPos + f * 2.6f;
    const f32 gy = m_terrain.height(p.x, p.z);
    // snap a la rejilla de 4 m
    BuildPiece piece;
    piece.kind = m_buildKind;
    piece.cell.x = (i32)std::round(p.x / CellSize);
    piece.cell.z = (i32)std::round(p.z / CellSize);
    piece.cell.y = (i32)(std::round(gy / 2.0f) * 2.0f);
    piece.rot = 0;
    // rot de pared segun hacia donde mira
    if (m_buildKind == BuildKind::Wall) {
        const f32 rel = wrapAngle(m_playerYaw);
        piece.rot = (u8)((i32)std::round(rel / (PI_F * 0.5f)) & 3);
    }
    if (m_buildKind == BuildKind::Ramp) {
        const f32 rel = wrapAngle(m_playerYaw);
        piece.rot = (u8)((i32)std::round(rel / (PI_F * 0.5f)) & 3);
    }
    piece.hp = 150;
    return piece;
}

bool Game::buildPlacementValid(const BuildPiece& p) const {
    // no duplicado en la misma celda + mismo tipo
    for (const auto& b : m_builds)
        if (b.cell == p.cell && b.kind == p.kind) return false;
    return true;
}

void Game::placeBuild() {
    if (m_wood < 10) {
        audioSystem().play(Sfx::Click, 0.4f, 0.5f);
        return;
    }
    const BuildPiece p = ghostPiece();
    if (!buildPlacementValid(p)) return;
    m_builds.push_back(p);
    m_wood -= 10;
    rebuildCollision();
    audioSystem().play(Sfx::BuildPlace, 0.9f);
    const auto [mn, mx] = buildAabb(p);
    const vec3 c = (mn + mx) * 0.5f;
    for (i32 i = 0; i < 6; ++i)
        emitParticle(c, vec3((f32)(i % 3 - 1), 1.5f, (f32)(i / 3 - 0.5f)) * 2.0f,
                     0.4f, 0.3f, vec4(0.8, 0.65f, 0.4f, 1), false);
}

void Game::updateBuild(f32) {}

// ---------------------------------------------------------------------------
// Particulas
// ---------------------------------------------------------------------------
void Game::emitParticle(const vec3& pos, const vec3& vel, f32 life, f32 size,
                        vec4 color, bool additive, f32 stretch) {
    for (auto& p : m_particles) {
        if (p.active) continue;
        p.pos = pos; p.vel = vel;
        p.life = life; p.maxLife = life;
        p.size = size; p.color = color;
        p.additive = additive; p.stretch = stretch;
        p.active = true;
        return;
    }
}

void Game::updateParticles(f32 dt) {
    m_partDrawAdd.clear();
    m_partDrawAlpha.clear();
    for (auto& p : m_particles) {
        if (!p.active) continue;
        p.life -= dt;
        if (p.life <= 0) { p.active = false; continue; }
        p.vel.y -= 9.0f * dt;
        p.pos += p.vel * dt;
        const f32 t = p.life / p.maxLife;
        ParticleDraw d;
        d.pos = p.pos;
        d.size = p.size * (0.5f + t * 0.5f);
        d.color = vec4(vec3(p.color), p.color.a * t);
        d.vel = p.vel;
        d.stretch = p.stretch;
        if (p.additive) m_partDrawAdd.push_back(d);
        else m_partDrawAlpha.push_back(d);
    }
}

void Game::botsDiedCheck() {
    // victoria cuando solo queda el jugador
    if (m_state == State::Playing && aliveCount() <= 1) {
        m_state = State::Victory;
        ++m_wins;
        audioSystem().play(Sfx::Victory, 1.0f);
        input().mouseCaptured = false;
        saveStats();
        // confeti
        for (i32 i = 0; i < 80; ++i) {
            Random rng((u32)i * 17 + 3);
            emitParticle(m_playerPos + vec3(rng.symmetric() * 12, 6 + rng.nextF32() * 6, rng.symmetric() * 12),
                         vec3(rng.symmetric() * 2, -1.5f, rng.symmetric() * 2),
                         2.5f + rng.nextF32() * 2.0f, 0.28f,
                         vec4(hsvToRgb(vec3(rng.nextF32(), 0.9f, 1.0f)), 1), true);
        }
    }
}

void Game::saveStats() {
#ifdef SKYVAULT_WEB
    EM_ASM({ try { localStorage.setItem('sv_wins', '' + $0); } catch (e) {} }, (int)m_wins);
#else
    Config cfg;
    cfg.set("victorias", (i32)m_wins);
    cfg.set("partidas", (i32)m_matches);
    cfg.save("skyvault_stats.ini");
#endif
}

void Game::loadStats() {
#ifdef SKYVAULT_WEB
    m_wins = (u32)EM_ASM_INT({
        try { return parseInt(localStorage.getItem('sv_wins') || '0') || 0; } catch (e) { return 0; }
    });
#else
    Config cfg;
    if (cfg.load("skyvault_stats.ini")) m_wins = (u32)cfg.getInt("victorias", 0);
#endif
}

void Game::addKillFeed(const std::string& text) {
    m_killFeed.push_back({text, 6.0f});
    if (m_killFeed.size() > 6) m_killFeed.erase(m_killFeed.begin());
    SV_LOG_INFO("killfeed", "%s", text.c_str());
}

} // namespace game
