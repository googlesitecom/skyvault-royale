// ============================================================================
//  SKYVAULT Royale - game/GameRender.cpp  (envio de draw calls + HUD + menus)
// ============================================================================
#include "game/Game.h"
#include <algorithm>
#include <cmath>

namespace game {

// ---------------------------------------------------------------------------
mat4 Game::weaponModelMatrix(WeaponKind k, const mat4& playerMat) {
    // colocacion aproximada "en la mano" (offsets del WeaponDef)
    return playerMat;
}

// ---------------------------------------------------------------------------
// Render del mundo
// ---------------------------------------------------------------------------
void Game::render() {
    Renderer& r = *m_r;
    r.beginFrame(m_camera, m_env);

    const Frustum& fr = m_camera.frustum;

    // terreno
    Material terrainMat;
    terrainMat.rough = 0.95f;
    m_terrain.draw(r, fr, terrainMat);

    // arboles visibles (instanciado, 2 especies con tinte por ejemplar)
    static std::vector<InstanceData> leafyInsts, pineInsts;
    leafyInsts.clear();
    pineInsts.clear();
    for (usize i = 0; i < m_treeData.size(); ++i) {
        if (!m_treeAlive[i]) continue;
        const vec4& t = m_treeData[i];
        const vec3 c(t.x, t.y + 3.2f * t.w, t.z);
        if (distance(c, m_camera.pos) > 900.0f) continue;
        if (!fr.intersectsSphere(c, 6.5f * t.w)) continue;
        InstanceData inst;
        inst.model = glm::translate(mat4(1), vec3(t.x, t.y - 0.3f, t.z)) *
                     glm::scale(mat4(1), vec3(t.w));
        // variacion de verde por ejemplar (hash del indice)
        const f32 hk = (f32)((i * 2654435761u) & 0xFF) / 255.0f;
        inst.color = vec4(0.85f + hk * 0.35f, 0.95f + hk * 0.15f, 0.8f + (1.0f - hk) * 0.3f, 1.0f);
        (m_treeKind[i] == 0 ? leafyInsts : pineInsts).push_back(inst);
    }
    Material foliageMat;
    foliageMat.wind = true;
    if (!leafyInsts.empty())
        r.drawInstances(m_treeLeafyMesh, leafyInsts.data(), (u32)leafyInsts.size(), foliageMat);
    if (!pineInsts.empty())
        r.drawInstances(m_treePineMesh, pineInsts.data(), (u32)pineInsts.size(), foliageMat);

    // pasto: regenera alrededor de la camara y dibuja (viento fuerte, sin sombras)
    if (m_state == State::Playing || m_state == State::Drop) {
        regenGrass(m_camera.pos);
        if (!m_grassInstances.empty()) {
            Material grassMat;
            grassMat.wind = true;
            grassMat.grass = true;
            grassMat.rough = 0.85f;
            r.drawInstances(m_grassMesh, m_grassInstances.data(),
                            (u32)m_grassInstances.size(), grassMat, false);
        }
    }

    static std::vector<InstanceData> rockInsts;
    rockInsts.clear();

    for (const vec3& rk : m_rockData) {
        if (distance(rk, m_camera.pos) > 700.0f) continue;
        if (!fr.intersectsSphere(rk, 2.0f)) continue;
        InstanceData inst;
        inst.model = glm::translate(mat4(1), rk) * glm::scale(mat4(1), vec3(1.6f, 1.1f, 1.6f));
        inst.color = vec4(1);
        rockInsts.push_back(inst);
    }
    {
        Material rockMat;
        rockMat.rough = 0.9f;
        if (!rockInsts.empty())
            r.drawInstances(m_rockMesh, rockInsts.data(), (u32)rockInsts.size(), rockMat);
    }

    // edificios POI: cajas con color por material + decoracion (ventanas/faroles)
    static std::vector<InstanceData> buildingInsts, decoInsts;
    buildingInsts.clear();
    decoInsts.clear();
    static std::vector<f32> decoEmis;
    decoEmis.clear();
    for (usize i = 0; i < m_staticBoxes.size(); ++i) {
        const auto& b = m_staticBoxes[i];
        const vec3 c = (b.box.bmin + b.box.bmax) * 0.5f;
        if (distance(c, m_camera.pos) > 850.0f) continue;
        if (!fr.intersectsAABB(b.box)) continue;
        InstanceData inst;
        const vec3 s = b.box.bmax - b.box.bmin;
        inst.model = glm::translate(mat4(1), c) * glm::scale(mat4(1), s);
        inst.color = m_boxColors[i];
        buildingInsts.push_back(inst);
    }
    for (const auto& d : m_decoBoxes) {
        const vec3 c = (d.box.bmin + d.box.bmax) * 0.5f;
        if (distance(c, m_camera.pos) > 600.0f) continue;
        if (!fr.intersectsAABB(d.box)) continue;
        InstanceData inst;
        inst.model = glm::translate(mat4(1), c) * glm::scale(mat4(1), d.box.bmax - d.box.bmin);
        inst.color = d.color;
        decoInsts.push_back(inst);
        decoEmis.push_back(d.emissive);
    }
    {
        Material buildingMat;
        buildingMat.rough = 0.85f;
        if (!buildingInsts.empty())
            r.drawInstances(m_boxMesh, buildingInsts.data(), (u32)buildingInsts.size(), buildingMat);
        // tejados a dos aguas (malla propia, yaw empaquetada en col.w)
        static std::vector<InstanceData> roofInsts;
        roofInsts.clear();
        for (const auto& rp : m_roofPieces) {
            if (distance(rp.c, m_camera.pos) > 850.0f) continue;
            if (!fr.intersectsSphere(rp.c, std::max(rp.s.x, rp.s.z))) continue;
            InstanceData inst;
            inst.model = glm::translate(mat4(1), rp.c) *
                         glm::rotate(mat4(1), rp.col.w, vec3(0, 1, 0)) *
                         glm::scale(mat4(1), rp.s);
            inst.color = vec4(rp.col.r, rp.col.g, rp.col.b, 1.0f);
            roofInsts.push_back(inst);
        }
        if (!roofInsts.empty()) {
            Material roofMat;
            roofMat.rough = 0.9f;
            r.drawInstances(m_roofMesh, roofInsts.data(), (u32)roofInsts.size(), roofMat);
        }
        // decoracion con brillo (faroles/ventanas): tramos emisivos/no emisivos
        Material decoLit;
        decoLit.rough = 0.6f;
        decoLit.emissive = 0.75f;
        Material decoPlain;
        decoPlain.rough = 0.8f;
        static std::vector<InstanceData> litBatch;
        litBatch.clear();
        static std::vector<InstanceData> plainBatch;
        plainBatch.clear();
        for (usize i = 0; i < decoInsts.size(); ++i)
            (decoEmis[i] > 0.05f ? litBatch : plainBatch).push_back(decoInsts[i]);
        if (!litBatch.empty())
            r.drawInstances(m_boxMesh, litBatch.data(), (u32)litBatch.size(), decoLit, false);
        if (!plainBatch.empty())
            r.drawInstances(m_boxMesh, plainBatch.data(), (u32)plainBatch.size(), decoPlain, false);
    }

    // cofres (cerrados brillan y flotan un pelin para verse de lejos)
    static std::vector<InstanceData> chestInsts;
    chestInsts.clear();
    for (const auto& c : m_chests) {
        if (c.opened) continue;
        if (distance(c.pos, m_camera.pos) > 500.0f) continue;
        if (!fr.intersectsSphere(c.pos + vec3(0, 0.4f, 0), 1.2f)) continue;
        InstanceData inst;
        const f32 bob = std::sin(m_time * 2.2f + c.pos.x * 0.35f) * 0.06f;
        inst.model = glm::translate(mat4(1), c.pos + vec3(0, bob, 0)) *
                     glm::rotate(mat4(1), m_time * 0.7f, vec3(0, 1, 0));
        inst.color = vec4(1);
        chestInsts.push_back(inst);
    }
    {
        Material chestMat;
        chestMat.emissive = 0.55f + std::sin(m_time * 3.0f) * 0.25f;
        if (!chestInsts.empty())
            r.drawInstances(m_chestMesh, chestInsts.data(), (u32)chestInsts.size(), chestMat);
    }

    // loot por el suelo (halo del color de rareza)
    static std::vector<InstanceData> lootInsts;
    lootInsts.clear();
    for (const auto& l : m_loot) {
        if (l.taken) continue;
        if (distance(l.pos, m_camera.pos) > 300.0f) continue;
        if (!fr.intersectsSphere(l.pos, 1.0f)) continue;
        InstanceData inst;
        inst.model = glm::translate(mat4(1), l.pos) * glm::scale(mat4(1), vec3(0.7f, 0.12f, 0.7f));
        inst.color = rarityColor(l.weapon.rarity);
        lootInsts.push_back(inst);
    }
    Material lootMat;
    lootMat.emissive = 0.8f;
    if (!lootInsts.empty())
        r.drawInstances(m_boxMesh, lootInsts.data(), (u32)lootInsts.size(), lootMat);

    // piezas construidas
    static std::vector<InstanceData> buildInsts;
    buildInsts.clear();
    for (const auto& p : m_builds) {
        const auto [mn, mx] = buildAabb(p);
        const vec3 c = (mn + mx) * 0.5f;
        if (!fr.intersectsAABB(AABB{mn, mx})) continue;
        InstanceData inst;
        inst.model = glm::translate(mat4(1), c) * glm::scale(mat4(1), mx - mn);
        const f32 hpK = clamp01((f32)p.hp / 150.0f);
        inst.color = vec4(0.55f + hpK * 0.45f, 0.4f + hpK * 0.4f, 0.25f + hpK * 0.2f, 1);
        buildInsts.push_back(inst);
    }
    Material buildMat;
    buildMat.rough = 0.8f;
    if (!buildInsts.empty())
        r.drawInstances(m_boxMesh, buildInsts.data(), (u32)buildInsts.size(), buildMat);

    // fantasma de construccion
    if (m_state == State::Playing && m_buildMode) {
        const BuildPiece gp = ghostPiece();
        const auto [mn, mx] = buildAabb(gp);
        InstanceData inst;
        inst.model = glm::translate(mat4(1), (mn + mx) * 0.5f) * glm::scale(mat4(1), mx - mn);
        inst.color = vec4(1);
        Material ghostMat;
        ghostMat.ghost = 1.0f;
        ghostMat.ghostColor = buildPlacementValid(gp) ? vec3(0.25f, 0.95f, 0.45f)
                                                     : vec3(0.95f, 0.25f, 0.2f);
        r.drawInstances(m_boxMesh, &inst, 1, ghostMat, false);
    }

    // --- menu: escaparate del personaje (rota frente a la camara) -------------
    if (m_state == State::Menu && m_gltfCharOk) {
        const vec3 f = m_camera.forward();
        const vec3 base = m_camera.pos + f * 9.0f - vec3(0, 0.9f, 0);
        drawGltf(m_gltfChar, gltfPivot(m_gltfChar, base, m_time * 0.4f), vec4(1));
    }

    // bots: GLB cerca (con tinte de traje), procedural lejos (LOD barato)
    {
        static const vec4 botTints[] = {
            {1.00f, 1.00f, 1.00f, 1}, {1.00f, 0.62f, 0.58f, 1}, {1.00f, 0.82f, 0.50f, 1},
            {0.65f, 1.00f, 0.70f, 1}, {0.78f, 0.68f, 1.00f, 1}, {1.00f, 0.62f, 0.95f, 1},
        };
        Material charMat;
        charMat.rough = 0.75f;
        charMat.metallic = 0.05f;
        u32 tintIdx = 0;
        for (const auto& bot : m_bots) {
            if (!bot.alive) continue;
            const f32 db = distance(bot.pos, m_camera.pos);
            if (db > 260.0f) continue;
            if (!fr.intersectsSphere(bot.pos + vec3(0, 1, 0), 2.0f)) continue;
            const vec4 tint = botTints[tintIdx++ % 6];
            if (m_gltfCharOk && db < 90.0f) {
                drawGltf(m_gltfChar, gltfPivot(m_gltfChar, bot.pos, bot.yaw), tint);
            } else {
                charMat.tint = tint;
                const mat4 model = glm::translate(mat4(1), bot.pos) *
                                   glm::rotate(mat4(1), bot.yaw, vec3(0, 1, 0));
                r.drawMesh(m_charMesh, model, charMat);
            }
        }
    }

    // jugador (tercera persona): GLB del usuario o procedural
    if (m_state == State::Playing || m_state == State::Drop || m_state == State::Victory) {
        {
            if (m_gltfCharOk) {
                drawGltf(m_gltfChar, gltfPivot(m_gltfChar, m_playerPos, m_playerYaw), vec4(1));
            } else {
                Material charMat;
                charMat.rough = 0.75f;
                const mat4 model = glm::translate(mat4(1), m_playerPos) *
                                   glm::rotate(mat4(1), m_playerYaw, vec3(0, 1, 0));
                r.drawMesh(m_charMesh, model, charMat);
            }
        }
        // arma en mano: GLB (tinteada por rareza) o procedural
        {
            const WeaponInstance& wi = m_slots[m_activeSlot];
            const i32 wk = (i32)wi.kind;
            bool drawn = false;
            if (wk >= 0 && wk < 5 && m_gltfWeaponOk[wk]) {
                const WeaponDef wd = weaponDef(wi.kind);
                const mat4 playerMat = glm::translate(mat4(1), m_playerPos) *
                                       glm::rotate(mat4(1), m_playerYaw, vec3(0, 1, 0));
                const GltfModel& g = m_gltfWeapon[wk];
                const vec3 pivot = (g.bounds.bmin + g.bounds.bmax) * 0.5f;
                const vec4 rc = rarityColor(wi.rarity);
                const vec4 tint = vec4(mix(vec3(1.0f), vec3(rc), 0.30f), 1.0f);
                // pose de mano: el pico cruza la mano, las armas apuntan al frente
                const f32 pitch = wi.kind == WeaponKind::Pickaxe ? -0.55f : -0.12f;
                const vec3 off(wi.kind == WeaponKind::Pickaxe ? vec3(0.34f, 1.18f, 0.42f)
                                                             : vec3(0.30f, 1.22f, 0.52f));
                const mat4 hand = glm::translate(playerMat, off) *
                                  glm::rotate(mat4(1), wd.modelYaw, vec3(0, 1, 0)) *
                                  glm::rotate(mat4(1), pitch, vec3(1, 0, 0)) *
                                  glm::translate(mat4(1), -pivot) *
                                  glm::scale(mat4(1), vec3(wd.modelScale));
                drawGltf(g, hand, tint);
                drawn = true;
            }
            if (!drawn) {
                const Mesh* model = nullptr;
                switch (wi.kind) {
                    case WeaponKind::Pickaxe:  model = &m_pickaxeMesh; break;
                    case WeaponKind::Shotgun:  model = &m_shotgunMesh; break;
                    case WeaponKind::SMG:      model = &m_smgMesh;     break;
                    case WeaponKind::Rifle:    model = &m_rifleMesh;   break;
                    case WeaponKind::Sniper:   model = &m_sniperMesh;  break;
                    default: break;
                }
                if (model) {
                    const WeaponDef wd = weaponDef(wi.kind);
                    const mat4 playerMat = glm::translate(mat4(1), m_playerPos) *
                                           glm::rotate(mat4(1), m_playerYaw, vec3(0, 1, 0));
                    const mat4 hand = glm::translate(playerMat, wd.modelOffset) *
                                      glm::rotate(mat4(1), wd.modelYaw, vec3(0, 1, 0)) *
                                      glm::scale(mat4(1), vec3(wd.modelScale));
                    Material wmat;
                    wmat.rough = 0.55f;
                    wmat.metallic = 0.30f;
                    if (wi.kind != WeaponKind::Pickaxe) {
                        const vec4 rc = rarityColor(wi.rarity);
                        wmat.tint = vec4(mix(vec3(1.0f), vec3(rc), 0.35f), 1.0f);
                    }
                    r.drawMesh(*model, hand, wmat);
                }
            }
        }
    }

    // Carguero Nube: dirigible con envolvente texturizada + gondola iluminada
    if (m_state == State::Drop && !m_falling) {
        const f32 bob = std::sin(m_time * 0.5f) * 1.2f;
        const mat4 busRot = glm::rotate(mat4(1), std::atan2(m_busDir.x, m_busDir.z), vec3(0, 1, 0));
        // envolvente (malla propia con paneles y aletas)
        {
            Material envMat;
            envMat.rough = 0.55f;
            envMat.metallic = 0.15f;
            const mat4 model = glm::translate(mat4(1), m_busPos + vec3(0, 13.0f + bob, 0)) * busRot;
            r.drawMesh(m_airshipMesh, model, envMat);
        }
        // gondola con ventanas calientes
        {
            Material gondMat;
            gondMat.rough = 0.5f;
            gondMat.metallic = 0.35f;
            InstanceData gond;
            gond.model = glm::translate(mat4(1), m_busPos + vec3(0, 3.2f + bob, 0)) * busRot *
                         glm::scale(mat4(1), vec3(6.0f, 2.8f, 12.0f));
            gond.color = vec4(0.42f, 0.40f, 0.46f, 1);
            r.drawInstances(m_boxMesh, &gond, 1, gondMat);
            Material winMat;
            winMat.emissive = 0.85f;
            InstanceData win;
            win.model = glm::translate(mat4(1), m_busPos + vec3(0, 3.7f + bob, 0)) * busRot *
                        glm::scale(mat4(1), vec3(6.15f, 0.9f, 9.0f));
            win.color = vec4(1.0f, 0.82f, 0.45f, 1);
            r.drawInstances(m_boxMesh, &win, 1, winMat, false);
        }
    }

    // tormenta
    if (m_state != State::Menu) {
        const mat4 stormModel = glm::translate(mat4(1), vec3(m_stormCenter.x, 0, m_stormCenter.y)) *
                                glm::scale(mat4(1), vec3(m_stormRadius, 700.0f, m_stormRadius));
        r.drawStormWall(stormModel);
    }

    // agua (siempre; el renderer hace el espejo)
    r.drawWater(m_camera);

    // particulas
    if (!m_partDrawAdd.empty())
        r.drawParticles(m_partDrawAdd.data(), (u32)m_partDrawAdd.size(), true);
    if (!m_partDrawAlpha.empty())
        r.drawParticles(m_partDrawAlpha.data(), (u32)m_partDrawAlpha.size(), false);

    r.endFrame(m_camera);

    // HUD
    r.uiBegin(m_r->viewW(), m_r->viewH());
    m_uiButtons.clear();
    switch (m_state) {
        case State::Menu:    renderMenu(); break;
        case State::Drop:    renderHud(); break;
        case State::Playing: renderHud(); break;
        case State::Dead:    renderHud(); renderEndScreen(); break;
        case State::Victory: renderHud(); renderEndScreen(); break;
        default: break;
    }
    r.uiEnd();
}

// ---------------------------------------------------------------------------
// Helpers de UI
// ---------------------------------------------------------------------------
static void textOutlined(Renderer& r, const char* s, f32 x, f32 y, f32 scale, vec4 col) {
    r.uiText(s, x + scale, y + scale, scale, vec4(0, 0, 0, 0.8f));
    r.uiText(s, x, y, scale, col);
}

void Game::renderHud() {
    Renderer& r = *m_r;
    const f32 W = (f32)r.viewW(), H = (f32)r.viewH();

    if (m_state == State::Drop) {
        textOutlined(r, "SALTAR [ESPACIO]", W * 0.5f - 100, H * 0.62f, 3, vec4(1, 1, 1, 1));
        char alt[64];
        std::snprintf(alt, sizeof(alt), "Altitud: %d m", (i32)m_playerPos.y);
        textOutlined(r, alt, W * 0.5f - 70, H * 0.56f, 2, vec4(0.8f, 0.9f, 1, 1));
        if (m_falling)
            textOutlined(r, m_gliding ? "PLANEADOR ABIERTO" : "CAYENDO...",
                         W * 0.5f - 90, H * 0.68f, 2, vec4(1, 0.9f, 0.4f, 1));
        // marcador del autobus
        textOutlined(r, "Carguero Nube", W * 0.5f - 60, 60, 2, vec4(0.9f, 0.85f, 1, 1));
        return;
    }

    // --- mira ------------------------------------------------------------------
    const f32 cx = W * 0.5f, cy = H * 0.5f;
    const f32 spread = 8.0f + (m_ads ? 2.0f : 9.0f);
    r.uiQuad(cx - 1, cy - spread - 6, 2, 6, vec4(1, 1, 1, 0.9f));
    r.uiQuad(cx - 1, cy + spread, 2, 6, vec4(1, 1, 1, 0.9f));
    r.uiQuad(cx - spread - 6, cy - 1, 6, 2, vec4(1, 1, 1, 0.9f));
    r.uiQuad(cx + spread, cy - 1, 6, 2, vec4(1, 1, 1, 0.9f));
    if (m_hitMarker > 0) {
        const f32 s = 6 + m_hitMarker * 8;
        r.uiQuad(cx - s, cy - s, 5, 2, vec4(1, 0.3f, 0.2f, m_hitMarker * 4));
        r.uiQuad(cx + s - 5, cy - s, 5, 2, vec4(1, 0.3f, 0.2f, m_hitMarker * 4));
        r.uiQuad(cx - s, cy + s - 2, 5, 2, vec4(1, 0.3f, 0.2f, m_hitMarker * 4));
        r.uiQuad(cx + s - 5, cy + s - 2, 5, 2, vec4(1, 0.3f, 0.2f, m_hitMarker * 4));
    }

    // --- barras de vida/escudo ---------------------------------------------------
    const f32 bx = 30, by = H - 70, bw = 300;
    r.uiQuad(bx - 3, by - 3, bw + 6, 16, vec4(0, 0, 0, 0.55f));
    r.uiQuad(bx, by, bw * clamp01((f32)m_shield / 100.0f), 10, vec4(0.25f, 0.6f, 1, 0.95f));
    r.uiQuad(bx - 3, by + 10, bw + 6, 24, vec4(0, 0, 0, 0.55f));
    const f32 hpK = clamp01((f32)m_hp / 100.0f);
    r.uiQuad(bx, by + 13, bw * hpK, 18,
             vec4(mix(vec3(0.9f, 0.15f, 0.1f), vec3(0.2f, 0.9f, 0.3f), hpK), 0.95f));
    char hpTxt[48];
    std::snprintf(hpTxt, sizeof(hpTxt), "%d", m_hp);
    textOutlined(r, hpTxt, bx + bw + 12, by + 14, 3, vec4(1, 1, 1, 1));

    // --- materiales + pociones -----------------------------------------------------
    char woodTxt[48];
    std::snprintf(woodTxt, sizeof(woodTxt), "Madera: %d", m_wood);
    r.uiQuad(30, H - 110, 14, 14, vec4(0.75f, 0.5f, 0.25f, 1));
    textOutlined(r, woodTxt, 52, H - 112, 2, vec4(1, 1, 1, 1));
    char potTxt[48];
    std::snprintf(potTxt, sizeof(potTxt), "Pociones [H]: %d", m_shieldPots);
    textOutlined(r, potTxt, 52, H - 96, 2, vec4(0.6f, 0.8f, 1, 1));

    // --- arma activa + municion ------------------------------------------------------
    const WeaponInstance& wi = m_slots[m_activeSlot];
    const WeaponDef wd = weaponDef(wi.kind);
    const vec4 rc = rarityColor(wi.rarity);
    char ammoTxt[64];
    if (wi.kind == WeaponKind::Pickaxe)
        std::snprintf(ammoTxt, sizeof(ammoTxt), "%s", wd.name);
    else
        std::snprintf(ammoTxt, sizeof(ammoTxt), "%s  %d/%d", wd.name, wi.ammo, wd.magSize);
    const f32 tw = r.uiTextWidth(ammoTxt, 3);
    textOutlined(r, ammoTxt, W - tw - 40, H - 60, 3, rc);
    r.uiQuad(W - tw - 40, H - 66, tw, 4, rc);
    if (m_reloading > 0)
        textOutlined(r, "RECARGANDO...", W - 150, H - 92, 2, vec4(1, 0.85f, 0.3f, 1));

    // --- slots ----------------------------------------------------------------------
    const f32 slotW = 52, gap = 6;
    const f32 slotsX = W * 0.5f - (5 * slotW + 4 * gap) * 0.5f;
    for (i32 i = 0; i < 5; ++i) {
        const f32 x = slotsX + (f32)i * (slotW + gap);
        const f32 y = H - 66;
        const WeaponInstance& s = m_slots[i];
        const bool active = i == m_activeSlot;
        r.uiQuad(x, y, slotW, 46, active ? vec4(0.15f, 0.15f, 0.18f, 0.9f) : vec4(0.08f, 0.08f, 0.1f, 0.6f));
        r.uiQuadBorder(x, y, slotW, 46, active ? vec4(1, 1, 1, 0.95f) : vec4(0.4f, 0.4f, 0.45f, 0.5f), 2);
        if (s.valid()) {
            r.uiQuad(x + 6, y + 30, slotW - 12, 4, rarityColor(s.rarity));
            textOutlined(r, weaponDef(s.kind).name, x + 4, y + 8, 1, vec4(1, 1, 1, 0.95f));
        } else if (i == 0) {
            textOutlined(r, "PICO", x + 14, y + 14, 2, vec4(0.85f, 0.85f, 0.85f, 1));
        }
        char n[4]; std::snprintf(n, sizeof(n), "%d", i + 1);
        r.uiText(n, x + 3, y + 3, 1, vec4(0.7f, 0.7f, 0.7f, 0.8f));
    }

    // --- modo construccion -----------------------------------------------------------
    if (m_buildMode) {
        static const char* names[] = { "PARED [Z]", "SUELO [X]", "RAMPA [C]", "TECHO [V]" };
        textOutlined(r, "MODO CONSTRUCCION - clic para colocar (10 madera)",
                     W * 0.5f - 210, H - 100, 2, vec4(0.5f, 1, 0.55f, 1));
        textOutlined(r, names[(i32)m_buildKind], W * 0.5f - 50, H - 80, 2, vec4(0.9f, 1, 0.9f, 1));
    }

    // --- tormenta (estado + minimapa) --------------------------------------------------
    const bool outside = distance(vec2(m_playerPos.x, m_playerPos.z), m_stormCenter) > m_stormRadius;
    {
        char st[96];
        if (m_stormShrinking)
            std::snprintf(st, sizeof(st), "LA TORMENTA AVANZA (%ds)", (i32)std::ceil(m_stormTimer));
        else if (m_stormPhase < 6)
            std::snprintf(st, sizeof(st), "La tormenta se cierra en %ds", (i32)std::ceil(m_stormTimer));
        else
            std::snprintf(st, sizeof(st), "Zona final");
        const f32 stw = r.uiTextWidth(st, 2);
        textOutlined(r, st, W * 0.5f - stw * 0.5f, 14, 2,
                     outside ? vec4(1, 0.4f, 0.95f, 1) : vec4(0.85f, 0.85f, 0.95f, 1));
    }

    // minimapa 200x200 esquina superior derecha
    const f32 mmSize = 190, mmX = W - mmSize - 16, mmY = 16;
    r.uiQuad(mmX - 3, mmY - 3, mmSize + 6, mmSize + 6, vec4(0, 0, 0, 0.6f));
    if (m_minimapReady) r.uiImage(m_minimapTex, mmX, mmY, mmSize, mmSize);
    const f32 mmScale = mmSize / Terrain::Size;
    auto worldToMm = [&](vec2 p) -> vec2 {
        return vec2(mmX + mmSize * 0.5f + p.x * mmScale, mmY + mmSize * 0.5f + p.y * mmScale);
    };
    // circulo de tormenta (punteado violeta) + siguiente zona (blanca)
    {
        const vec2 sc = worldToMm(m_stormCenter);
        const f32 rr = m_stormRadius * mmScale;
        for (i32 i = 0; i < 48; ++i) {
            const f32 a = (f32)i / 48.0f * TAU_F;
            r.uiQuad(sc.x + std::cos(a) * rr - 1.5f, sc.y + std::sin(a) * rr - 1.5f, 3, 3,
                     vec4(0.75f, 0.35f, 1.0f, 0.95f));
        }
        if (m_stormShrinking) {
            const vec2 tc = worldToMm(m_stormTarget);
            const f32 tr = m_stormTargetRadius * mmScale;
            for (i32 i = 0; i < 40; ++i) {
                const f32 a = (f32)i / 40.0f * TAU_F;
                r.uiQuad(tc.x + std::cos(a) * tr - 1.2f, tc.y + std::sin(a) * tr - 1.2f, 2.4f, 2.4f,
                         vec4(1, 1, 1, 0.9f));
            }
        }
    }
    // bots cercanos
    for (const auto& bot : m_bots) {
        if (!bot.alive) continue;
        if (distance(bot.pos, m_playerPos) > 260.0f) continue;
        const vec2 bp = worldToMm(vec2(bot.pos.x, bot.pos.z));
        r.uiQuad(bp.x - 1.5f, bp.y - 1.5f, 3, 3, vec4(1, 0.25f, 0.2f, 1));
    }
    // jugador (flecha = triangulo aprox con 2 quads)
    {
        const vec2 pp = worldToMm(vec2(m_playerPos.x, m_playerPos.z));
        r.uiQuad(pp.x - 2, pp.y - 2, 4, 4, vec4(1, 1, 1, 1));
        const vec3 f(std::sin(m_playerYaw), 0, std::cos(m_playerYaw));
        r.uiQuad(pp.x + f.x * 4 - 1, pp.y + f.z * 4 - 1, 2, 2, vec4(1, 1, 0.4f, 1));
    }
    // contador jugadores
    char aliveTxt[64];
    std::snprintf(aliveTxt, sizeof(aliveTxt), "Vivos: %d", aliveCount());
    textOutlined(r, aliveTxt, mmX, mmY + mmSize + 8, 2, vec4(1, 1, 1, 1));
    char killTxt[48];
    std::snprintf(killTxt, sizeof(killTxt), "Eliminaciones: %d", m_kills);
    textOutlined(r, killTxt, mmX, mmY + mmSize + 28, 2, vec4(1, 0.85f, 0.3f, 1));

    // --- kill feed -----------------------------------------------------------------
    f32 kfy = 60;
    for (const auto& k : m_killFeed) {
        const f32 a = clamp01(k.t / 2.0f);
        textOutlined(r, k.text.c_str(), W - r.uiTextWidth(k.text.c_str(), 2) - 240, kfy, 2,
                     vec4(0.95f, 0.95f, 1, a));
        kfy += 20;
    }

    // --- prompts + overlays -----------------------------------------------------------
    if (m_interactHint > 0 && !m_interactText.empty())
        textOutlined(r, m_interactText.c_str(), W * 0.5f - 100, H * 0.66f, 2, vec4(1, 1, 0.6f, 1));

    if (m_hurtFlash > 0) {
        r.uiQuad(0, 0, W, H, vec4(0.7f, 0.05f, 0.05f, m_hurtFlash * 0.28f));
    }
    if (outside) {
        r.uiQuadBorder(0, 0, W, H, vec4(0.7f, 0.3f, 1.0f, 0.5f + std::sin(m_time * 6.0f) * 0.3f), 8);
        textOutlined(r, "ESTAS EN LA TORMENTA - CORRE A LA ZONA", W * 0.5f - 170, H * 0.2f, 2,
                     vec4(1, 0.5f, 1, 1));
    }

    // crosshair ADS de francotirador
    if (m_ads && m_slots[m_activeSlot].kind == WeaponKind::Sniper) {
        r.uiQuadBorder(cx - 60, cy - 60, 120, 120, vec4(0, 0, 0, 0.9f), 2);
        r.uiQuad(cx - 58, cy - 0.5f, 116, 1, vec4(0, 0, 0, 0.9f));
        r.uiQuad(cx - 0.5f, cy - 58, 1, 116, vec4(0, 0, 0, 0.9f));
        // oscurece fuera del reticulo
        r.uiQuad(0, 0, W, cy - 60, vec4(0, 0, 0, 0.35f));
        r.uiQuad(0, cy + 60, W, H, vec4(0, 0, 0, 0.35f));
        r.uiQuad(0, cy - 60, cx - 60, 120, vec4(0, 0, 0, 0.35f));
        r.uiQuad(cx + 60, cy - 60, W, 120, vec4(0, 0, 0, 0.35f));
    }
}

// ---------------------------------------------------------------------------
void Game::renderMenu() {
    Renderer& r = *m_r;
    const f32 W = (f32)r.viewW(), H = (f32)r.viewH();

    r.uiQuad(0, 0, W, H, vec4(0.02f, 0.03f, 0.08f, 0.55f));
    textOutlined(r, "SKYVAULT", W * 0.5f - 300, H * 0.16f, 12, vec4(1, 0.85f, 0.25f, 1));
    textOutlined(r, "ROYALE", W * 0.5f - 168, H * 0.16f + 100, 8, vec4(0.35f, 0.7f, 1, 1));
    textOutlined(r, "Isla La Bóveda - 100 combatientes, 1 victorioso",
                 W * 0.5f - 200, H * 0.16f + 175, 2, vec4(0.9f, 0.9f, 0.95f, 0.9f));

    // boton JUGAR
    auto button = [&](f32 x, f32 y, f32 w, f32 h, const char* txt, u32 id) {
        r.uiQuad(x, y, w, h, vec4(0.10f, 0.12f, 0.18f, 0.92f));
        r.uiQuadBorder(x, y, w, h, vec4(1, 0.85f, 0.3f, 0.9f), 2);
        const f32 tw = r.uiTextWidth(txt, 3);
        r.uiText(txt, x + w * 0.5f - tw * 0.5f, y + h * 0.5f - 12, 3, vec4(1, 1, 1, 1));
        m_uiButtons.push_back({{x, y}, {x + w, y + h}, id});
    };

    button(W * 0.5f - 130, H * 0.48f, 260, 56, "SALTAR DEL CARGUERO", 1);

    // selector de preset grafico
    static const char* presets[] = { "BAJO", "MEDIO", "ALTO", "LUDOS" };
    textOutlined(r, "Graficos:", W * 0.5f - 250, H * 0.62f, 2, vec4(0.8f, 0.85f, 0.95f, 1));
    for (i32 i = 0; i < 4; ++i) {
        const f32 x = W * 0.5f - 100 + (f32)i * 105;
        const bool sel = i == m_menuPresetSel;
        r.uiQuad(x, H * 0.62f - 4, 95, 30, sel ? vec4(0.25f, 0.35f, 0.55f, 0.95f)
                                               : vec4(0.1f, 0.12f, 0.18f, 0.85f));
        r.uiQuadBorder(x, H * 0.62f - 4, 95, 30, sel ? vec4(0.4f, 0.75f, 1, 1) : vec4(0.3f, 0.35f, 0.45f, 0.7f), 2);
        const f32 tw = r.uiTextWidth(presets[i], 2);
        r.uiText(presets[i], x + 47.5f - tw * 0.5f, H * 0.62f + 3, 2, vec4(1, 1, 1, 1));
        m_uiButtons.push_back({{x, H * 0.62f - 4}, {x + 95, H * 0.62f + 26}, (u32)(10 + i)});
    }

    // controles
    const char* help[] = {
        "WASD moverse  |  ESPACIO saltar  |  SHIFT correr",
        "Raton apuntar  |  Clic izq. disparar  |  Clic der. apuntar (ADS)",
        "1-5 armas  |  R recargar  |  E abrir cofre  |  H pocion de escudo",
        "Q construir  |  Z pared  X suelo  C rampa  V techo  |  F pico",
        "Pico: tala arboles para ganar madera",
    };
    f32 hy = H * 0.74f;
    for (const char* line : help) {
        textOutlined(r, line, W * 0.5f - r.uiTextWidth(line, 2) * 0.5f, hy, 2, vec4(0.75f, 0.8f, 0.9f, 0.85f));
        hy += 22;
    }
    char stats[96];
    std::snprintf(stats, sizeof(stats), "Victorias: %u", m_wins);
    textOutlined(r, stats, 20, H - 30, 2, vec4(1, 0.85f, 0.3f, 1));
    textOutlined(r, "Click para capturar el raton al empezar",
                 W * 0.5f - 160, H * 0.95f, 2, vec4(0.6f, 0.65f, 0.75f, 0.8f));
}

void Game::renderEndScreen() {
    Renderer& r = *m_r;
    const f32 W = (f32)r.viewW(), H = (f32)r.viewH();
    r.uiQuad(0, 0, W, H, vec4(0, 0, 0, 0.55f));
    if (m_state == State::Victory) {
        textOutlined(r, "¡VICTORIA REAL!", W * 0.5f - 250, H * 0.3f, 8, vec4(1, 0.85f, 0.2f, 1));
    } else {
        textOutlined(r, "ELIMINADO", W * 0.5f - 160, H * 0.3f, 8, vec4(1, 0.3f, 0.25f, 1));
        char place[64];
        std::snprintf(place, sizeof(place), "Puesto #%d de 100", aliveCount() + 1);
        textOutlined(r, place, W * 0.5f - 100, H * 0.3f + 70, 3, vec4(1, 1, 1, 1));
    }
    char stats[128];
    std::snprintf(stats, sizeof(stats), "Eliminaciones: %d   |   Victororias: %u", m_kills, m_wins);
    textOutlined(r, stats, W * 0.5f - 150, H * 0.48f, 2, vec4(0.9f, 0.9f, 0.95f, 1));

    // boton volver a jugar
    const f32 bx = W * 0.5f - 130, by = H * 0.56f;
    r.uiQuad(bx, by, 260, 50, vec4(0.1f, 0.14f, 0.2f, 0.95f));
    r.uiQuadBorder(bx, by, 260, 50, vec4(0.4f, 0.9f, 1, 0.9f), 2);
    const char* txt = "JUGAR OTRA VEZ";
    r.uiText(txt, bx + 130 - r.uiTextWidth(txt, 3) * 0.5f, by + 17, 3, vec4(1, 1, 1, 1));
    m_uiButtons.push_back({{bx, by}, {bx + 260, by + 50}, 2});
}

// ---------------------------------------------------------------------------
// Clics de UI
// ---------------------------------------------------------------------------
void Game::clickUI(f32 x, f32 y) {
    for (const auto& b : m_uiButtons) {
        if (x < b.min.x || x > b.max.x || y < b.min.y || y > b.max.y) continue;
        audioSystem().play(Sfx::Click, 0.8f);
        if (b.id == 1) {            // jugar
            m_r->setPreset((Preset)m_menuPresetSel);
            startMatch();
            input().mouseCaptured = false;   // el usuario hace click para capturar
        } else if (b.id == 2) {     // volver a jugar
            m_r->setPreset((Preset)m_menuPresetSel);
            startMatch();
        } else if (b.id >= 10 && b.id <= 13) {
            m_menuPresetSel = (i32)b.id - 10;
        }
        return;
    }
    // clic en cualquier lugar del menu = empezar (comodidad en web)
    if (m_state == State::Menu) {
        m_r->setPreset((Preset)m_menuPresetSel);
        startMatch();
    }
}

} // namespace game
