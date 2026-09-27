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

    // personajes y armas 100% procedurales: ningun asset de terceros
    // (requisito legal del proyecto: arte y nombres originales)
    m_charMesh    = makeCharacterMesh();
    m_pickaxeMesh = makeWeaponMesh(WeaponKind::Pickaxe);
    m_shotgunMesh = makeWeaponMesh(WeaponKind::Shotgun);
    m_smgMesh     = makeWeaponMesh(WeaponKind::SMG);
    m_rifleMesh   = makeWeaponMesh(WeaponKind::Rifle);
    m_sniperMesh  = makeWeaponMesh(WeaponKind::Sniper);

    // geometria procedural: arbol, roca, cofre, caja
    m_treeMesh = makeTreeMesh();
    m_rockMesh = makeRockMesh();
    m_chestMesh = makeChestMesh();
    m_boxMesh = makeUnitCube();

    generateWorldContent();
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
    m_treeMesh.destroy(); m_rockMesh.destroy(); m_chestMesh.destroy(); m_boxMesh.destroy();
    m_charMesh.destroy(); m_pickaxeMesh.destroy(); m_shotgunMesh.destroy();
    m_smgMesh.destroy(); m_rifleMesh.destroy(); m_sniperMesh.destroy();
    m_minimapTex.destroy();
}

// ---------------------------------------------------------------------------
// Generacion de contenido: arboles, rocas, POIs, cofres, loot
// ---------------------------------------------------------------------------
Mesh Game::makeTreeMesh() {
    // pino bajo: tronco + 2 conos
    std::vector<Vertex> v;
    std::vector<u32> idx;
    const vec3 trunk(0.36f, 0.22f, 0.14f), leaf1(0.13f, 0.42f, 0.16f), leaf2(0.16f, 0.50f, 0.20f);
    // tronco (cilindro de 6 lados, radio 0.25, altura 3.2)
    for (u32 i = 0; i <= 6; ++i) {
        const f32 a = (f32)i / 6.0f * TAU_F;
        const vec3 n(std::cos(a), 0, std::sin(a));
        v.push_back({{n.x * 0.25f, 0, n.z * 0.25f}, n, {0, 0}, {1,0,0,1},{},{}, vec4(trunk, 1)});
        v.push_back({{n.x * 0.25f, 3.2f, n.z * 0.25f}, n, {1, 0}, {1,0,0,1},{},{}, vec4(trunk, 1)});
    }
    for (u32 i = 0; i < 6; ++i) {
        const u32 b = i * 2;
        idx.insert(idx.end(), {b, b + 1, b + 2, b + 2, b + 1, b + 3});   // CCW fuera
    }
    auto cone = [&](f32 y0, f32 y1, f32 r, vec3 col) {
        const u32 base = (u32)v.size();
        for (u32 i = 0; i <= 7; ++i) {
            const f32 a = (f32)i / 7.0f * TAU_F;
            const vec3 n(std::cos(a), 0.35f, std::sin(a));
            v.push_back({{std::cos(a) * r, y0, std::sin(a) * r}, n, {0, 0}, {1,0,0,1},{},{}, vec4(col, 1)});
        }
        const u32 tip = (u32)v.size();
        v.push_back({{0, y1, 0}, vec3(0, 1, 0), {0.5f, 0.5f}, {1,0,0,1},{},{}, vec4(col, 1)});
        for (u32 i = 0; i < 7; ++i) {
            const u32 b = base + i;
            idx.insert(idx.end(), {b, tip, b + 1});   // CCW fuera
        }
    };
    cone(1.6f, 4.6f, 2.3f, leaf1);
    cone(3.4f, 6.4f, 1.5f, leaf2);
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
    m_rockData.clear();
    for (i32 i = 0; i < 9000 && m_treeData.size() < 3200; ++i) {
        const f32 x = rng.symmetric() * Terrain::Half * 0.95f;
        const f32 z = rng.symmetric() * Terrain::Half * 0.95f;
        const f32 h = m_terrain.height(x, z);
        if (h < 8.0f || h > 235.0f) continue;
        const vec3 n = m_terrain.normal(x, z);
        if (n.y < 0.82f) continue;                       // pendiente
        bool nearPoi = false;
        for (const auto& poi : pois)
            if (distance(vec2(x, z), poi.pos) < poi.radius * 0.8f) { nearPoi = true; break; }
        if (nearPoi) continue;
        const f32 dens = m_terrain.moistureAt(x, z);
        if (rng.nextF32() > dens * 1.5f) continue;
        m_treeData.push_back(vec4(x, h, z, 0.8f + rng.nextF32() * 0.7f));
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

    // edificios de POI + cofres + loot
    m_staticBoxes.clear();
    m_chests.clear();
    m_loot.clear();
    m_collision.clearDynamic();
    for (const auto& poi : pois) {
        const f32 groundH = m_terrain.height(poi.pos.x, poi.pos.y);
        const bool flatish = true;
        for (i32 b = 0; b < poi.buildingCount; ++b) {
            const f32 ang = rng.nextF32() * TAU_F + (f32)b * 2.4f;
            const f32 rad = poi.radius * (0.25f + rng.nextF32() * 0.4f);
            const f32 bx = poi.pos.x + std::cos(ang) * rad;
            const f32 bz = poi.pos.y + std::sin(ang) * rad;
            const f32 bh = m_terrain.height(bx, bz);
            if (bh < 4.0f) continue;                     // en el agua, no
            const f32 w = 7.0f, d = 7.0f, hgt = 4.0f;
            const vec3 sz(w, hgt, d);
            // 4 paredes con hueco de puerta (cara -Z) + techo
            const vec3 mn(bx - sz.x * 0.5f, bh, bz - sz.z * 0.5f);
            const vec3 mx(bx + sz.x * 0.5f, bh + hgt, bz + sz.z * 0.5f);
            const f32 T = 0.35f;
            auto addWall = [&](vec3 a, vec3 b) {
                m_staticBoxes.push_back({AABB{a, b}, ColliderOwner::World, 0, false, 0});
            };
            addWall(vec3(mn.x, bh, mn.z), vec3(mx.x, bh + hgt, mn.z + T));                    // frente
            addWall(vec3(mn.x, bh, mx.z - T), vec3(mx.x, bh + hgt, mx.z));                    // atras
            addWall(vec3(mn.x, bh, mn.z + T), vec3(mn.x + T, bh + hgt, mx.z - T));            // izq
            addWall(vec3(mx.x - T, bh, mn.z + T), vec3(mx.x, bh + hgt, mx.z - T));            // der
            // puerta: hueco en la pared frontal -> dividir en 2 (simplemente dejamos
            // la pared frontal partida restando un segmento central)
            // (solucion: la pared frontal ya es solida; abrimos hueco quitando el
            //  tramo central con un segundo box "negativo" no es posible; en su
            //  lugar elevamos la pared frontal como medio-arco)
            m_staticBoxes.pop_back();
            addWall(vec3(mn.x, bh + 2.6f, mn.z), vec3(mx.x, bh + hgt, mn.z + T));             // dintel
            addWall(vec3(mn.x, bh, mn.z), vec3(mn.x + 1.6f, bh + 2.6f, mn.z + T));            // izquierda puerta
            addWall(vec3(mx.x - 1.6f, bh, mn.z), vec3(mx.x, bh + 2.6f, mn.z + T));            // derecha puerta
            addWall(vec3(mn.x, bh + hgt, mn.z), vec3(mx.x, bh + hgt + T, mx.z));              // techo
        }
        // cofres: 2-4 por POI
        const i32 chestCount = 2 + (i32)(rng.nextF32() * 3.0f);
        for (i32 c = 0; c < chestCount; ++c) {
            const f32 a = rng.nextF32() * TAU_F, r = rng.nextF32() * poi.radius * 0.7f;
            const f32 cx = poi.pos.x + std::cos(a) * r;
            const f32 cz = poi.pos.y + std::sin(a) * r;
            const f32 ch = m_terrain.height(cx, cz);
            if (ch > 3.0f) m_chests.push_back({vec3(cx, ch, cz), false});
        }
        // loot por el suelo
        const i32 lootCount = 2 + (i32)(rng.nextF32() * 3.0f);
        for (i32 l = 0; l < lootCount; ++l) {
            const f32 a = rng.nextF32() * TAU_F, r = rng.nextF32() * poi.radius;
            const f32 lx = poi.pos.x + std::cos(a) * r;
            const f32 lz = poi.pos.y + std::sin(a) * r;
            const f32 lh = m_terrain.height(lx, lz);
            if (lh < 3.0f) continue;
            FloorLoot fl;
            fl.pos = vec3(lx, lh + 0.3f, lz);
            fl.weapon.kind = (WeaponKind)(1 + rng.rangeU32(4));
            fl.weapon.rarity = (u8)rng.rangeU32(5);
            fl.weapon.ammo = 999;
            fl.taken = false;
            m_loot.push_back(fl);
        }
        (void)groundH; (void)flatish;
    }

    // arboles al collider
    for (usize i = 0; i < m_treeData.size(); ++i)
        m_collision.addTree(vec2(m_treeData[i].x, m_treeData[i].z), 0.45f * m_treeData[i].w, 6.4f * m_treeData[i].w);

    rebuildCollision();
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
    // autobus: cruza la isla por una linea aleatoria
    Random rng(m_seed + (u32)m_time);
    const f32 a = rng.nextF32() * TAU_F;
    m_busDir = vec3(std::cos(a), 0, std::sin(a));
    const vec3 perp(-m_busDir.z, 0, m_busDir.x);
    const f32 off = rng.symmetric() * 600.0f;
    m_busPos = -m_busDir * (Terrain::Half * 0.62f) + perp * off;
    m_busPos.y = 380.0f;

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
        const auto& pois = m_terrain.pois();
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
    m_stormTimer = 26.0f;
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
        // L = aterrizaje rapido (pruebas / saltarse la caida): cae en
        // zona de tierra firme a media distancia del centro (nunca agua)
        if (in.keyPressed(GLFW_KEY_L) || m_debugLandRequested) {
            m_debugLandRequested = false;
            m_falling = true;
            m_gliding = true;
            vec2 dir = normalize(vec2(m_playerPos.x, m_playerPos.z));
            if (!(dir.x == dir.x) || std::fabs(dir.x) + std::fabs(dir.y) < 1e-4f)
                dir = vec2(1, 0);                       // bus justo sobre el centro
            f32 dropR = 1300.0f;
            for (f32 r = 1300.0f; r > 250.0f; r -= 75.0f) {
                if (m_terrain.height(dir.x * r, dir.y * r) > 8.0f) { dropR = r; break; }
            }
            m_playerPos.x = dir.x * dropR;
            m_playerPos.z = dir.y * dropR;
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
    static constexpr f32 Radii[] = { 1400.0f, 900.0f, 520.0f, 280.0f, 130.0f, 45.0f };
    static constexpr f32 Dps[] = { 1.0f, 2.0f, 5.0f, 8.0f, 10.0f, 12.0f };
    static constexpr f32 WaitT[] = { 26.0f, 24.0f, 22.0f, 20.0f, 18.0f, 16.0f };
    static constexpr f32 ShrinkT[] = { 34.0f, 30.0f, 26.0f, 22.0f, 18.0f, 14.0f };

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
