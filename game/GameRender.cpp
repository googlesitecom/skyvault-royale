// ============================================================================
//  SKYVAULT Royale - game/GameRender.cpp
//  Draw calls del mundo + animacion esqueletica + planeador + autobus +
//  HUD estilo Fortnite Ch2 + lobby (menu) con pedestal flotante.
// ============================================================================
#include "game/Game.h"
#include <algorithm>
#include <cmath>

namespace game {

// ---------------------------------------------------------------------------
// Animacion: construye la pose local del rig (idle embebido + overrides
// procedurales: ciclo de carrera, apuntado, caida, planeo) y las matrices
// de piel listas para la GPU. handMat = matriz de la mano derecha (agarre).
// ---------------------------------------------------------------------------
void Game::buildCharacterPose(const vec3& pos, f32 yaw, f32 speedK, f32 phase,
                              f32 idleT, bool airborne, bool gliding, f32 pitch,
                              WeaponKind weapon, f32 pickSwing, f32 reloadK,
                              GltfPose& pose, std::vector<mat4>& skin,
                              mat4& handMat) const {
    const GltfModel& m = m_gltfChar;
    if (!m_gltfCharOk || !m.skinned) { handMat = mat4(1); return; }

    pose.resetTo(m.joints);
    // base: clip de idle embebido (respira, se mueve sutil)
    if (!airborne && speedK < 0.25f)
        m.sampleAnim(m_idleClip, idleT, pose);

    const bool hasWeapon = weapon != WeaponKind::Pickaxe;
    const bool holding = hasWeapon || pickSwing > 0.0f;

    // ---- helpers de rotacion local (PRE-multiplicacion = espacio del PADRE,
    // ejes alineados al cuerpo: los deltas proceduralos son predecibles)
    auto rotJoint = [&](i32 j, const vec3& axis, f32 ang) {
        if (j < 0 || (usize)j >= pose.rot.size() || std::fabs(ang) < 1e-5f) return;
        pose.rot[(usize)j] = glm::normalize(glm::angleAxis(ang, normalize(axis)) *
                                            pose.rot[(usize)j]);
    };
    const f32 K = speedK;                 // 0..1 intensidad del ciclo
    const f32 p = phase;
    const f32 swing = std::sin(p);
    const f32 swingOpp = std::sin(p + PI_F);

    if (airborne) {
        // ---- CAIDA LIBRE: cuerpo estilizado, brazos extendidos -------------
        if (gliding) {
            // planeo: brazos arriba sujetando el planeador, piernas recogidas
            rotJoint(m_jUpperarmR, vec3(0, 0, 1),  1.95f);
            rotJoint(m_jUpperarmL, vec3(0, 0, 1), -1.95f);
            rotJoint(m_jLowerarmR, vec3(0, 1, 0),  0.35f);
            rotJoint(m_jLowerarmL, vec3(0, 1, 0), -0.35f);
            rotJoint(m_jThighL, vec3(1, 0, 0), -0.35f);
            rotJoint(m_jThighR, vec3(1, 0, 0), -0.20f);
            rotJoint(m_jCalfL, vec3(1, 0, 0),  0.85f);
            rotJoint(m_jCalfR, vec3(1, 0, 0),  0.60f);
            rotJoint(m_jSpine01, vec3(1, 0, 0), 0.10f);
            rotJoint(m_jSpine02, vec3(1, 0, 0), 0.12f);
        } else {
            // skydive: brazos en cruz con aleteo, piernas abiertas
            const f32 flap = std::sin(p * 2.0f) * 0.10f;
            rotJoint(m_jUpperarmR, vec3(0, 0, 1),  0.55f + flap);
            rotJoint(m_jUpperarmR, vec3(0, 1, 0),  0.25f);
            rotJoint(m_jUpperarmL, vec3(0, 0, 1), -0.55f - flap);
            rotJoint(m_jUpperarmL, vec3(0, 1, 0), -0.25f);
            rotJoint(m_jLowerarmR, vec3(0, 1, 0),  0.55f);
            rotJoint(m_jLowerarmL, vec3(0, 1, 0), -0.55f);
            rotJoint(m_jThighL, vec3(1, 0, 0), -0.30f + flap * 0.5f);
            rotJoint(m_jThighR, vec3(1, 0, 0), -0.15f - flap * 0.5f);
            rotJoint(m_jThighL, vec3(0, 0, 1),  0.16f);
            rotJoint(m_jThighR, vec3(0, 0, 1), -0.16f);
            rotJoint(m_jCalfL, vec3(1, 0, 0),  0.55f);
            rotJoint(m_jCalfR, vec3(1, 0, 0),  0.40f);
        }
    } else if (K > 0.04f) {
        // ---- CARRERA: zancada completa con contrapeso de brazos -------------
        const f32 amp = 0.85f * K;
        rotJoint(m_jThighL, vec3(1, 0, 0), -swing * amp);
        rotJoint(m_jThighR, vec3(1, 0, 0), -swingOpp * amp);
        // rodilla: se dobla al recoger la pierna
        rotJoint(m_jCalfL, vec3(1, 0, 0),  std::max(0.0f, -std::sin(p - 0.7f)) * 1.25f * K);
        rotJoint(m_jCalfR, vec3(1, 0, 0),  std::max(0.0f, -std::sin(p - 0.7f + PI_F)) * 1.25f * K);
        rotJoint(m_jFootL, vec3(1, 0, 0), -std::max(0.0f, -std::sin(p - 0.7f)) * 0.4f * K);
        rotJoint(m_jFootR, vec3(1, 0, 0), -std::max(0.0f, -std::sin(p - 0.7f + PI_F)) * 0.4f * K);
        // inclinacion adelante + balanceo de cadera
        rotJoint(m_jSpine01, vec3(1, 0, 0), 0.16f * K);
        rotJoint(m_jSpine02, vec3(1, 0, 0), 0.10f * K);
        rotJoint(m_jSpine02, vec3(0, 1, 0), swing * 0.06f * K);
        rotJoint(m_jPelvis,  vec3(0, 1, 0), -swing * 0.05f * K);
        if (holding) {
            // brazo derecho apuntando al frente, izquierdo acompana
            rotJoint(m_jUpperarmR, vec3(0, 1, 0),  1.42f);
            rotJoint(m_jUpperarmR, vec3(0, 0, 1),  0.12f);
            rotJoint(m_jUpperarmR, vec3(1, 0, 0), -0.10f * K);
            rotJoint(m_jLowerarmR, vec3(0, 1, 0),  0.28f);
            if (hasWeapon) {
                rotJoint(m_jUpperarmL, vec3(0, 1, 0), -1.05f);
                rotJoint(m_jUpperarmL, vec3(0, 0, 1), -0.28f);
                rotJoint(m_jLowerarmL, vec3(0, 1, 0), -0.75f);
            } else {
                rotJoint(m_jUpperarmL, vec3(0, 1, 0), -swingOpp * 0.55f * K);
                rotJoint(m_jUpperarmL, vec3(0, 0, 1), -0.12f);
            }
        } else {
            // sin arma: balanceo natural de brazos (adelante/atras alrededor de Y)
            rotJoint(m_jUpperarmR, vec3(0, 1, 0),  swing * 0.75f * K);
            rotJoint(m_jUpperarmR, vec3(0, 0, 1),  0.14f);
            rotJoint(m_jUpperarmL, vec3(0, 1, 0), -swingOpp * 0.75f * K);
            rotJoint(m_jUpperarmL, vec3(0, 0, 1), -0.14f);
            rotJoint(m_jLowerarmR, vec3(0, 1, 0),  0.30f);
            rotJoint(m_jLowerarmL, vec3(0, 1, 0), -0.30f);
        }
        // recarga: arma abajo y a un lado
        if (reloadK > 0.0f) {
            rotJoint(m_jUpperarmR, vec3(0, 1, 0), -0.55f * reloadK);
            rotJoint(m_jUpperarmR, vec3(1, 0, 0), -0.45f * reloadK);
        }
    } else {
        // ---- QUIETO (idle clip + arma en guardia) ---------------------------
        if (holding) {
            rotJoint(m_jUpperarmR, vec3(0, 1, 0),  1.38f);
            rotJoint(m_jUpperarmR, vec3(0, 0, 1),  0.10f);
            rotJoint(m_jLowerarmR, vec3(0, 1, 0),  0.25f);
            if (hasWeapon) {
                rotJoint(m_jUpperarmL, vec3(0, 1, 0), -1.02f);
                rotJoint(m_jUpperarmL, vec3(0, 0, 1), -0.26f);
                rotJoint(m_jLowerarmL, vec3(0, 1, 0), -0.72f);
            }
        }
        if (reloadK > 0.0f) {
            rotJoint(m_jUpperarmR, vec3(0, 1, 0), -0.55f * reloadK);
            rotJoint(m_jUpperarmR, vec3(1, 0, 0), -0.45f * reloadK);
        }
    }

    // golpe de pico: arco de swing sobre el brazo derecho
    if (pickSwing > 0.0f) {
        const f32 s = std::sin(pickSwing * PI_F);           // 0..1..0
        rotJoint(m_jUpperarmR, vec3(0, 1, 0),  1.9f - s * 1.5f);
        rotJoint(m_jUpperarmR, vec3(1, 0, 0), -0.5f + s * 0.9f);
        rotJoint(m_jSpine01, vec3(0, 1, 0), -s * 0.22f);
        rotJoint(m_jSpine02, vec3(1, 0, 0),  s * 0.12f);
    }

    // cabeza y torso siguen la mirada (pitch de camara)
    if (m_jNeck >= 0) rotJoint(m_jNeck, vec3(1, 0, 0), -pitch * 0.30f);
    if (m_jHead >= 0) rotJoint(m_jHead, vec3(1, 0, 0), -pitch * 0.25f);

    // matrices finales
    m.computeSkinMatrices(pose, skin);
    // mano derecha en espacio del modelo (para anclar el arma)
    if (m_jHandR >= 0) {
        const mat4 raw = m.jointMatrix(pose, m_jHandR);
        const mat4 norm = glm::translate(mat4(1), -m.skinCenter) *
                          glm::scale(mat4(1), vec3(m.skinNormScale));
        handMat = norm * raw;
    } else handMat = mat4(1);
    (void)pos; (void)yaw;
}

// ---------------------------------------------------------------------------
// Dibuja un personaje (GLB con piel o procedural) + arma anclada a la mano
// ---------------------------------------------------------------------------
void Game::drawCharacter(const vec3& pos, f32 yaw, const GltfPose& pose,
                         const std::vector<mat4>& skin, const mat4& handMat,
                         vec4 tint, WeaponKind weapon, u8 rarity, f32 pickSwing,
                         f32 reloadK, f32 recoilK, bool drawWeapon) {
    Renderer& r = *m_r;
    const mat4 bodyMat = glm::translate(mat4(1), pos) *
                         glm::rotate(mat4(1), yaw, vec3(0, 1, 0));

    if (m_gltfCharOk && m_gltfChar.skinned && !skin.empty()) {
        for (const auto& p : m_gltfChar.prims) {
            Material mat = p.mat;
            mat.tint = vec4(mat.tint.r * tint.r, mat.tint.g * tint.g,
                            mat.tint.b * tint.b, mat.tint.a * tint.a);
            mat.emissive += 0.05f;
            r.drawSkinned(p.mesh, bodyMat, mat, skin.data(),
                          (u32)skin.size());
        }
    } else if (m_gltfCharOk) {
        drawGltf(m_gltfChar, gltfPivot(m_gltfChar, pos, yaw), tint);
    } else {
        Material charMat;
        charMat.rough = 0.75f;
        charMat.tint = tint;
        r.drawMesh(m_charMesh, bodyMat, charMat);
    }

    // ---- arma anclada a la mano derecha -----------------------------------
    if (!drawWeapon) return;
    const i32 wk = (i32)weapon;
    if (wk < 0 || wk > 4) return;
    const WeaponDef wd = weaponDef(weapon);
    const vec4 rc = rarityColor(rarity);
    const vec4 wTint = weapon == WeaponKind::Pickaxe
                     ? vec4(1) : vec4(mix(vec3(1.0f), vec3(rc), 0.30f), 1.0f);

    mat4 hand = bodyMat * handMat;
    // retroceso: empuja el arma hacia atras y arriba
    if (recoilK > 0.0f) {
        hand = hand * glm::translate(mat4(1), vec3(0, 0.012f * recoilK, -0.05f * recoilK)) *
               glm::rotate(mat4(1), -0.10f * recoilK, vec3(1, 0, 0));
    }
    if (m_gltfWeaponOk[wk]) {
        const GltfModel& g = m_gltfWeapon[wk];
        const vec3 pivot = (g.bounds.bmin + g.bounds.bmax) * 0.5f;
        const mat4 local =
            glm::rotate(mat4(1), wd.handYawPitchRoll.x, vec3(0, 1, 0)) *
            glm::rotate(mat4(1), wd.handYawPitchRoll.y, vec3(1, 0, 0)) *
            glm::rotate(mat4(1), wd.handYawPitchRoll.z, vec3(0, 0, 1)) *
            glm::translate(mat4(1), wd.grip) *
            glm::translate(mat4(1), -pivot) *
            glm::scale(mat4(1), vec3(wd.modelScale));
        drawGltf(g, hand * local, wTint);
    } else {
        const Mesh* model = nullptr;
        switch (weapon) {
            case WeaponKind::Pickaxe:  model = &m_pickaxeMesh; break;
            case WeaponKind::Shotgun:  model = &m_shotgunMesh; break;
            case WeaponKind::SMG:      model = &m_smgMesh;     break;
            case WeaponKind::Rifle:    model = &m_rifleMesh;   break;
            case WeaponKind::Sniper:   model = &m_sniperMesh;  break;
            default: break;
        }
        if (model) {
            const mat4 local =
                glm::rotate(mat4(1), wd.handYawPitchRoll.x, vec3(0, 1, 0)) *
                glm::rotate(mat4(1), wd.handYawPitchRoll.y, vec3(1, 0, 0)) *
                glm::rotate(mat4(1), wd.handYawPitchRoll.z, vec3(0, 0, 1)) *
                glm::translate(mat4(1), wd.grip) *
                glm::rotate(mat4(1), 0.5f, vec3(0, 1, 0)) *
                glm::scale(mat4(1), vec3(wd.modelScale));
            Material wmat;
            wmat.rough = 0.55f;
            wmat.metallic = 0.30f;
            wmat.tint = wTint;
            r.drawMesh(*model, hand * local, wmat);
        }
    }
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
        if (distance(c, m_camera.pos) > 950.0f) continue;
        if (!fr.intersectsAABB(b.box)) continue;
        InstanceData inst;
        const vec3 s = b.box.bmax - b.box.bmin;
        inst.model = glm::translate(mat4(1), c) * glm::scale(mat4(1), s);
        inst.color = m_boxColors[i];
        buildingInsts.push_back(inst);
    }
    for (const auto& d : m_decoBoxes) {
        const vec3 c = (d.box.bmin + d.box.bmax) * 0.5f;
        if (distance(c, m_camera.pos) > 700.0f) continue;
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
            if (distance(rp.c, m_camera.pos) > 950.0f) continue;
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

    // --- MENU: lobby con pedestal flotante + personaje animado ---------------
    if (m_state == State::Menu) {
        // pedestal: tres losas apiladas flotando en el cielo
        static std::vector<InstanceData> pedestal;
        pedestal.clear();
        const vec3 P = m_menuCharPos;
        auto slab = [&](vec3 s, f32 dy, vec4 col) {
            InstanceData inst;
            inst.model = glm::translate(mat4(1), P + vec3(0, dy, 0)) * glm::scale(mat4(1), s);
            inst.color = col;
            pedestal.push_back(inst);
        };
        slab(vec3(5.6f, 0.42f, 5.6f), -0.42f, vec4(0.30f, 0.36f, 0.52f, 1));
        slab(vec3(6.3f, 0.20f, 6.3f), -0.72f, vec4(0.18f, 0.22f, 0.34f, 1));
        slab(vec3(4.9f, 0.14f, 4.9f),  0.02f, vec4(0.42f, 0.50f, 0.70f, 1));
        Material pedMat;
        pedMat.rough = 0.55f;
        pedMat.metallic = 0.15f;
        r.drawInstances(m_boxMesh, pedestal.data(), (u32)pedestal.size(), pedMat);
        // anillo luminoso bajo el pedestal
        InstanceData glow;
        glow.model = glm::translate(mat4(1), P + vec3(0, -0.85f, 0)) * glm::scale(mat4(1), vec3(7.4f, 0.05f, 7.4f));
        glow.color = vec4(0.35f, 0.62f, 1.0f, 1);
        Material glowMat;
        glowMat.emissive = 0.9f;
        r.drawInstances(m_boxMesh, &glow, 1, glowMat, false);

        // personaje del lobby (pose idle del GLB ya calculada en updateMenu)
        if (m_gltfCharOk && m_gltfChar.skinned && !m_menuSkin.empty()) {
            // el personaje gira hacia la camara mientras orbita (turntable)
            const f32 faceCam = m_camera.yaw + PI_F;
            for (const auto& p : m_gltfChar.prims) {
                const mat4 model = glm::translate(mat4(1), P) *
                                   glm::rotate(mat4(1), faceCam, vec3(0, 1, 0));
                Material mat = p.mat;
                mat.emissive += 0.07f;
                r.drawSkinned(p.mesh, model, mat, m_menuSkin.data(),
                              (u32)m_menuSkin.size());
            }
        }
    }

    // bots: GLB con animacion cerca, procedural lejos (LOD barato)
    {
        static const vec4 botTints[] = {
            {1.00f, 1.00f, 1.00f, 1}, {1.00f, 0.62f, 0.58f, 1}, {1.00f, 0.82f, 0.50f, 1},
            {0.65f, 1.00f, 0.70f, 1}, {0.78f, 0.68f, 1.00f, 1}, {1.00f, 0.62f, 0.95f, 1},
        };
        u32 tintIdx = 0;
        static GltfPose pose;
        static std::vector<mat4> skin;
        mat4 hand;
        for (const auto& bot : m_bots) {
            if (!bot.alive) continue;
            const f32 db = distance(bot.pos, m_camera.pos);
            if (db > 260.0f) continue;
            if (!fr.intersectsSphere(bot.pos + vec3(0, 1, 0), 2.0f)) continue;
            const vec4 tint = botTints[tintIdx++ % 6];
            if (m_gltfCharOk && m_gltfChar.skinned && db < 110.0f) {
                buildCharacterPose(bot.pos, bot.yaw, bot.animSpeedK, bot.animPhase,
                                   bot.animT, false, false, 0.0f, bot.weapon,
                                   0.0f, 0.0f, pose, skin, hand);
                drawCharacter(bot.pos, bot.yaw, pose, skin, hand, tint,
                              bot.weapon, bot.weaponRarity, 0.0f, 0.0f, 0.0f);
            } else if (m_gltfCharOk) {
                drawGltf(m_gltfChar, gltfPivot(m_gltfChar, bot.pos, bot.yaw), tint);
            } else {
                Material charMat;
                charMat.rough = 0.75f;
                charMat.tint = tint;
                const mat4 model = glm::translate(mat4(1), bot.pos) *
                                   glm::rotate(mat4(1), bot.yaw, vec3(0, 1, 0));
                m_r->drawMesh(m_charMesh, model, charMat);
            }
        }
    }

    // jugador (tercera persona): pose completa con arma y planeador
    if (m_state == State::Playing || m_state == State::Drop || m_state == State::Victory) {
        const WeaponInstance& wi = m_slots[m_activeSlot];
        const f32 reloadK = m_reloading > 0.0f
                          ? std::sin(clamp01(m_reloading / 1.4f) * PI_F) : 0.0f;
        // pose: colgado del autobus / cayendo / planeando / en suelo
        const bool airborne = m_state == State::Drop && m_falling;
        const bool onBus = m_state == State::Drop && !m_falling;
        buildCharacterPose(m_playerPos, m_playerYaw,
                           m_state == State::Playing ? m_animSpeedK : 0.0f,
                           m_animPhase, m_animIdleT, airborne && !onBus,
                           m_gliding, m_playerPitch, wi.kind,
                           m_pickAnim > 0.0f ? 1.0f - (m_pickAnim / 0.3f) : 0.0f,
                           reloadK, m_playerPose, m_playerSkin, m_playerHandMat);
        // colgado del autobus: brazos arriba sujetando el cable
        if (onBus && m_jUpperarmR >= 0) {
            auto& pose2 = m_playerPose;
            auto rotJ = [&](i32 j, const vec3& axis, f32 ang) {
                if (j < 0 || (usize)j >= pose2.rot.size()) return;
                pose2.rot[(usize)j] = glm::normalize(pose2.rot[(usize)j] *
                                                     glm::angleAxis(ang, normalize(axis)));
            };
            rotJ(m_jUpperarmR, vec3(0, 0, 1),  2.6f);
            rotJ(m_jUpperarmL, vec3(0, 0, 1), -2.6f);
            rotJ(m_jUpperarmR, vec3(0, 1, 0), -0.2f);
            rotJ(m_jUpperarmL, vec3(0, 1, 0),  0.2f);
            m_gltfChar.computeSkinMatrices(pose2, m_playerSkin);
        }
        drawCharacter(m_playerPos, m_playerYaw, m_playerPose, m_playerSkin,
                      m_playerHandMat, vec4(1), wi.kind, wi.rarity,
                      m_pickAnim > 0.0f ? 1.0f - (m_pickAnim / 0.3f) : 0.0f,
                      reloadK, m_recoil, !onBus);

        // planeador sobre la cabeza
        if (m_state == State::Drop && m_falling && m_gliding) {
            const mat4 model = glm::translate(mat4(1), m_playerPos + vec3(0, 2.45f, 0)) *
                               glm::rotate(mat4(1), m_playerYaw, vec3(0, 1, 0)) *
                               glm::rotate(mat4(1), 0.18f, vec3(1, 0, 0));
            Material glMat;
            glMat.rough = 0.8f;
            glMat.wind = true;
            m_r->drawMesh(m_gliderMesh, model, glMat);
        }
    }

    // Carguero Nube: globon + cabina de autobus (visible TODO el drop, incluso
    // despues de saltar: sigue volando como en Fortnite)
    if (m_state == State::Drop) {
        const f32 bob = std::sin(m_time * 0.5f) * 1.0f;
        const mat4 busRot = glm::rotate(mat4(1), std::atan2(m_busDir.x, m_busDir.z), vec3(0, 1, 0));
        Material busMat;
        busMat.rough = 0.55f;
        busMat.metallic = 0.15f;
        const mat4 model = glm::translate(mat4(1), m_busPos + vec3(0, bob, 0)) * busRot;
        m_r->drawMesh(m_airshipMesh, model, busMat);
        // cable del que cuelga el jugador (antes de saltar)
        if (!m_falling) {
            InstanceData cable;
            cable.model = glm::translate(mat4(1), m_busPos + vec3(0, bob, 0)) * busRot *
                          glm::translate(mat4(1), vec3(0, -6.8f + -0.9f, 0)) *
                          glm::scale(mat4(1), vec3(0.07f, 2.4f, 0.07f));
            cable.color = vec4(0.14f, 0.15f, 0.18f, 1);
            Material cMat;
            cMat.rough = 0.9f;
            m_r->drawInstances(m_boxMesh, &cable, 1, cMat);
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
    r.uiText(s, x + scale, y + scale, scale, vec4(0, 0, 0, 0.75f));
    r.uiText(s, x, y, scale, col);
}

// panel redondeado aproximado: rect central + laterales recortados
static void panel(Renderer& r, f32 x, f32 y, f32 w, f32 h, vec4 col, f32 bevel = 6.0f) {
    r.uiQuad(x + bevel, y, w - bevel * 2, h, col);
    r.uiQuad(x, y + bevel, w, h - bevel * 2, col);
    // esquinas: cuadritos decrecientes (aproximacion barata)
    for (i32 i = 0; i < (i32)bevel; i += 2) {
        const f32 o = (f32)i;
        r.uiQuad(x + o, y + o, 2, 2, col);
        r.uiQuad(x + w - o - 2, y + o, 2, 2, col);
        r.uiQuad(x + o, y + h - o - 2, 2, 2, col);
        r.uiQuad(x + w - o - 2, y + h - o - 2, 2, 2, col);
    }
}

// ---------------------------------------------------------------------------
// HUD de partida estilo Fortnite Capitulo 2
// ---------------------------------------------------------------------------
void Game::renderHud() {
    Renderer& r = *m_r;
    const f32 W = (f32)r.viewW(), H = (f32)r.viewH();

    if (m_state == State::Drop) {
        textOutlined(r, "ESPACIO PARA SALTAR", W * 0.5f - 130, H * 0.62f, 3, vec4(1, 0.95f, 0.4f, 1));
        char alt[64];
        std::snprintf(alt, sizeof(alt), "Altitud: %d m", (i32)m_playerPos.y);
        textOutlined(r, alt, W * 0.5f - 70, H * 0.56f, 2, vec4(0.8f, 0.9f, 1, 1));
        if (m_falling)
            textOutlined(r, m_gliding ? "PLANEADOR ABIERTO" : "EN CAIDA LIBRE",
                         W * 0.5f - 100, H * 0.68f, 2, vec4(1, 0.9f, 0.4f, 1));
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

    // === BARRAS DE VIDA/ESCUDO (abajo-izquierda, estilo Ch2) =================
    {
        const f32 bx = 26, by = H - 96, bw = 330;
        // escudo (arriba, azul)
        panel(r, bx - 6, by - 6, bw + 12, 24, vec4(0.02f, 0.05f, 0.12f, 0.75f));
        r.uiQuad(bx, by, bw * clamp01((f32)m_shield / 100.0f), 12, vec4(0.25f, 0.65f, 1.0f, 0.95f));
        char stxt[16];
        std::snprintf(stxt, sizeof(stxt), "%d", m_shield);
        textOutlined(r, stxt, bx + bw + 8, by - 2, 2, vec4(0.55f, 0.8f, 1.0f, 1));
        // vida (abajo, verde -> rojo)
        panel(r, bx - 6, by + 24, bw + 12, 32, vec4(0.02f, 0.05f, 0.12f, 0.75f));
        const f32 hpK = clamp01((f32)m_hp / 100.0f);
        r.uiQuad(bx, by + 30, bw * hpK, 20,
                 vec4(mix(vec3(0.92f, 0.18f, 0.12f), vec3(0.25f, 0.9f, 0.35f), hpK), 0.95f));
        char hpTxt[16];
        std::snprintf(hpTxt, sizeof(hpTxt), "%d", m_hp);
        textOutlined(r, hpTxt, bx + bw + 8, by + 32, 3, vec4(1, 1, 1, 1));
    }

    // === MATERIALES + POCIONES (junto a las barras) ===========================
    {
        const f32 mx = 26, my = H - 140;
        panel(r, mx - 4, my - 4, 210, 34, vec4(0.02f, 0.05f, 0.12f, 0.7f));
        r.uiQuad(mx + 4, my + 6, 14, 14, vec4(0.75f, 0.5f, 0.25f, 1));
        char woodTxt[48];
        std::snprintf(woodTxt, sizeof(woodTxt), "Madera %d", m_wood);
        textOutlined(r, woodTxt, mx + 26, my + 5, 2, vec4(1, 1, 1, 1));
        r.uiQuad(mx + 120, my + 6, 10, 14, vec4(0.4f, 0.75f, 1.0f, 1));
        char potTxt[32];
        std::snprintf(potTxt, sizeof(potTxt), "x%d [H]", m_shieldPots);
        textOutlined(r, potTxt, mx + 136, my + 5, 2, vec4(0.7f, 0.9f, 1.0f, 1));
    }

    // === ARMA ACTIVA + MUNICION (abajo-derecha, sobre los slots) ==============
    const WeaponInstance& wi = m_slots[m_activeSlot];
    const WeaponDef wd = weaponDef(wi.kind);
    const vec4 rc = rarityColor(wi.rarity);
    {
        char ammoTxt[64];
        if (wi.kind == WeaponKind::Pickaxe)
            std::snprintf(ammoTxt, sizeof(ammoTxt), "%s", wd.name);
        else
            std::snprintf(ammoTxt, sizeof(ammoTxt), "%d / %d", wi.ammo, wd.magSize);
        const f32 tw = r.uiTextWidth(ammoTxt, 3);
        textOutlined(r, ammoTxt, W - tw - 44, H - 128, 3, rc);
        textOutlined(r, wd.name, W - r.uiTextWidth(wd.name, 2) - 44, H - 150, 2,
                     vec4(0.9f, 0.9f, 0.95f, 1));
        r.uiQuad(W - tw - 44, H - 134, tw, 4, rc);
        if (m_reloading > 0) {
            const f32 k = 1.0f - clamp01(m_reloading / 1.4f);
            r.uiQuad(W - 220, H - 160, 176 * k, 5, vec4(1, 0.85f, 0.3f, 1));
            textOutlined(r, "RECARGANDO", W - 190, H - 176, 2, vec4(1, 0.85f, 0.3f, 1));
        }
    }

    // === SLOTS (abajo-derecha, estilo Ch2) ====================================
    {
        const f32 slotW = 58, gap = 7;
        const f32 slotsX = W - (5 * slotW + 4 * gap) - 28;
        const f32 y = H - 66;
        for (i32 i = 0; i < 5; ++i) {
            const f32 x = slotsX + (f32)i * (slotW + gap);
            const WeaponInstance& s = m_slots[i];
            const bool active = i == m_activeSlot;
            const vec4 rcS = s.valid() ? rarityColor(s.rarity) : vec4(0, 0, 0, 0);
            // fondo con tinte de rareza
            panel(r, x, y, slotW, 50, vec4(0.02f, 0.05f, 0.12f, active ? 0.92f : 0.65f));
            if (s.valid()) {
                panel(r, x + 3, y + 3, slotW - 6, 44,
                      vec4(rcS.r, rcS.g, rcS.b, active ? 0.30f : 0.16f));
                textOutlined(r, weaponDef(s.kind).name, x + 4, y + 22, 1, vec4(1, 1, 1, 0.95f));
            } else if (i == 0) {
                textOutlined(r, "PICO", x + 16, y + 18, 2, vec4(0.85f, 0.85f, 0.85f, 1));
            }
            if (active)
                r.uiQuadBorder(x - 1, y - 1, slotW + 2, 52, vec4(1, 1, 1, 0.95f), 2);
            char n[4]; std::snprintf(n, sizeof(n), "%d", i + 1);
            r.uiText(n, x + 3, y + 3, 1, vec4(0.7f, 0.7f, 0.7f, 0.8f));
        }
    }

    // === MODO CONSTRUCCION =====================================================
    if (m_buildMode) {
        static const char* names[] = { "PARED [Z]", "SUELO [X]", "RAMPA [C]", "TECHO [V]" };
        panel(r, W * 0.5f - 235, H - 132, 470, 46, vec4(0.04f, 0.12f, 0.06f, 0.8f));
        textOutlined(r, "MODO CONSTRUCCION - clic para colocar (10 madera)",
                     W * 0.5f - 210, H - 126, 2, vec4(0.5f, 1, 0.55f, 1));
        textOutlined(r, names[(i32)m_buildKind], W * 0.5f - 45, H - 108, 2, vec4(0.9f, 1, 0.9f, 1));
    }

    // === MINIMAPA (arriba-derecha, marco estilo Ch2) ===========================
    const bool outside = distance(vec2(m_playerPos.x, m_playerPos.z), m_stormCenter) > m_stormRadius;
    const f32 mmSize = 185, mmX = W - mmSize - 18, mmY = 14;
    panel(r, mmX - 5, mmY - 5, mmSize + 10, mmSize + 10, vec4(0.02f, 0.04f, 0.1f, 0.85f));
    if (m_minimapReady) r.uiImage(m_minimapTex, mmX, mmY, mmSize, mmSize);
    const f32 mmScale = mmSize / Terrain::Size;
    auto worldToMm = [&](vec2 p) -> vec2 {
        return vec2(mmX + mmSize * 0.5f + p.x * mmScale, mmY + mmSize * 0.5f + p.y * mmScale);
    };
    // circulo de tormenta (azul claro con lineas) + siguiente zona (blanca)
    {
        const vec2 sc = worldToMm(m_stormCenter);
        const f32 rr = m_stormRadius * mmScale;
        for (i32 i = 0; i < 48; ++i) {
            const f32 a = (f32)i / 48.0f * TAU_F;
            r.uiQuad(sc.x + std::cos(a) * rr - 1.5f, sc.y + std::sin(a) * rr - 1.5f, 3, 3,
                     vec4(0.45f, 0.75f, 1.0f, 0.95f));
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
    // jugador (flecha)
    {
        const vec2 pp = worldToMm(vec2(m_playerPos.x, m_playerPos.z));
        r.uiQuad(pp.x - 2, pp.y - 2, 4, 4, vec4(1, 1, 1, 1));
        const vec3 f(std::sin(m_playerYaw), 0, std::cos(m_playerYaw));
        r.uiQuad(pp.x + f.x * 4 - 1, pp.y + f.z * 4 - 1, 2, 2, vec4(1, 1, 0.4f, 1));
    }
    // contador jugadores + kills (bajo el minimapa, estilo Ch2)
    {
        panel(r, mmX - 5, mmY + mmSize + 8, mmSize + 10, 56, vec4(0.02f, 0.04f, 0.1f, 0.8f));
        char aliveTxt[64];
        std::snprintf(aliveTxt, sizeof(aliveTxt), "VIVOS   %d", aliveCount());
        textOutlined(r, aliveTxt, mmX + 8, mmY + mmSize + 14, 2, vec4(1, 1, 1, 1));
        char killTxt[48];
        std::snprintf(killTxt, sizeof(killTxt), "ELIMS   %d", m_kills);
        textOutlined(r, killTxt, mmX + 8, mmY + mmSize + 36, 2, vec4(1, 0.85f, 0.3f, 1));
    }

    // === TORMENTA: estado (arriba-centro, con icono) ==========================
    {
        char st[96];
        if (m_stormShrinking)
            std::snprintf(st, sizeof(st), "LA TORMENTA AVANZA (%ds)", (i32)std::ceil(m_stormTimer));
        else if (m_stormPhase < 6)
            std::snprintf(st, sizeof(st), "La tormenta se cierra en %ds", (i32)std::ceil(m_stormTimer));
        else
            std::snprintf(st, sizeof(st), "Zona final");
        const f32 stw = r.uiTextWidth(st, 2);
        panel(r, W * 0.5f - stw * 0.5f - 26, 8, stw + 52, 28, vec4(0.02f, 0.05f, 0.12f, 0.7f));
        // icono tormenta (circulo azul)
        r.uiQuad(W * 0.5f - stw * 0.5f - 18, 14, 16, 16, vec4(0.45f, 0.75f, 1.0f, 1));
        textOutlined(r, st, W * 0.5f - stw * 0.5f + 4, 13, 2,
                     outside ? vec4(0.7f, 0.9f, 1.0f, 1) : vec4(0.85f, 0.85f, 0.95f, 1));
    }

    // === KILL FEED (bajo el minimapa) =========================================
    {
        f32 kfy = mmY + mmSize + 76;
        for (const auto& k : m_killFeed) {
            const f32 a = clamp01(k.t / 2.0f);
            const f32 tw = r.uiTextWidth(k.text.c_str(), 2);
            panel(r, W - tw - 66, kfy - 3, tw + 14, 22, vec4(0.02f, 0.04f, 0.1f, 0.55f * a));
            textOutlined(r, k.text.c_str(), W - tw - 58, kfy, 2, vec4(0.95f, 0.95f, 1, a));
            kfy += 26;
        }
    }

    // --- prompts + overlays -----------------------------------------------------
    if (m_interactHint > 0 && !m_interactText.empty())
        textOutlined(r, m_interactText.c_str(), W * 0.5f - 100, H * 0.66f, 2, vec4(1, 1, 0.6f, 1));

    if (m_hurtFlash > 0) {
        r.uiQuad(0, 0, W, H, vec4(0.7f, 0.05f, 0.05f, m_hurtFlash * 0.28f));
    }
    if (outside) {
        // tinte de tormenta: azul claro como el muro
        r.uiQuadBorder(0, 0, W, H, vec4(0.45f, 0.75f, 1.0f, 0.4f + std::sin(m_time * 6.0f) * 0.25f), 8);
        textOutlined(r, "ESTAS EN LA TORMENTA - CORRE A LA ZONA", W * 0.5f - 175, H * 0.2f, 2,
                     vec4(0.7f, 0.9f, 1.0f, 1));
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
// Menu: LOBBY estilo Fortnite (personaje al centro, azul, JUGAR amarillo)
// ---------------------------------------------------------------------------
void Game::renderMenu() {
    Renderer& r = *m_r;
    const f32 W = (f32)r.viewW(), H = (f32)r.viewH();

    // degradado azul Fortnite por encima de la escena 3D (el personaje queda
    // al centro, despejado; arriba y abajo mas oscuros)
    {
        const i32 bands = 14;
        for (i32 i = 0; i < bands; ++i) {
            const f32 t = (f32)i / (f32)(bands - 1);      // 0 arriba .. 1 abajo
            const f32 aTop = 0.34f * (1.0f - t) ;
            const f32 aBot = 0.42f * t;
            const f32 a = std::max(aTop, aBot);
            const vec3 c = mix(vec3(0.045f, 0.16f, 0.46f), vec3(0.02f, 0.05f, 0.22f), t);
            r.uiQuad(0, H * t - H / bands, W, H / bands + 2, vec4(c, a));
        }
        // banda central transparente (el personaje brilla)
    }

    // === LOGO arriba-izquierda =================================================
    textOutlined(r, "SKYVAULT", 34, 26, 7, vec4(1, 0.85f, 0.25f, 1));
    textOutlined(r, "ROYALE", 38, 88, 4, vec4(0.55f, 0.8f, 1.0f, 1));

    // === victorias arriba-derecha ==============================================
    {
        char stats[96];
        std::snprintf(stats, sizeof(stats), "Victorias: %u", m_wins);
        const f32 tw = r.uiTextWidth(stats, 2);
        panel(r, W - tw - 60, 18, tw + 40, 30, vec4(0.02f, 0.05f, 0.12f, 0.7f));
        textOutlined(r, stats, W - tw - 40, 25, 2, vec4(1, 0.85f, 0.3f, 1));
    }

    // === BOTON JUGAR: amarillo grande abajo-derecha (estilo Fortnite) =========
    {
        const f32 bw = 300, bh = 72;
        const f32 bx = W - bw - 46, by = H - bh - 42;
        // sombra + borde exterior brillante
        r.uiQuad(bx - 3, by - 3, bw + 6, bh + 6, vec4(1, 0.85f, 0.3f, 0.35f));
        r.uiQuad(bx - 1, by - 1, bw + 2, bh + 2, vec4(0.10f, 0.08f, 0.02f, 0.9f));
        // cuerpo amarillo con brillo superior
        panel(r, bx, by, bw, bh, vec4(0.99f, 0.80f, 0.10f, 1.0f), 10);
        r.uiQuad(bx + 10, by + 6, bw - 20, 10, vec4(1.0f, 0.92f, 0.45f, 0.55f));
        const char* txt = "JUGAR";
        const f32 tw = r.uiTextWidth(txt, 5);
        r.uiText(txt, bx + bw * 0.5f - tw * 0.5f + 2, by + bh * 0.5f - 22 + 2, 5,
                 vec4(0.1f, 0.07f, 0.0f, 0.5f));
        r.uiText(txt, bx + bw * 0.5f - tw * 0.5f, by + bh * 0.5f - 22, 5,
                 vec4(0.09f, 0.07f, 0.02f, 1));
        m_uiButtons.push_back({{bx, by}, {bx + bw, by + bh}, 1});
    }

    // === selector de preset grafico (a la izquierda del boton) ================
    {
        static const char* presets[] = { "BAJO", "MEDIO", "ALTO", "LUDOS" };
        textOutlined(r, "CALIDAD GRAFICA", W - 400, H - 152, 2, vec4(0.8f, 0.85f, 0.95f, 1));
        for (i32 i = 0; i < 4; ++i) {
            const f32 y = H - 128 + (f32)i * 30;
            const f32 x = W - 400;
            const bool sel = i == m_menuPresetSel;
            panel(r, x, y, 118, 26,
                  sel ? vec4(0.16f, 0.34f, 0.72f, 0.95f) : vec4(0.03f, 0.07f, 0.16f, 0.8f));
            if (sel) r.uiQuadBorder(x, y, 118, 26, vec4(0.55f, 0.8f, 1.0f, 1), 2);
            const f32 tw = r.uiTextWidth(presets[i], 2);
            r.uiText(presets[i], x + 59 - tw * 0.5f, y + 6, 2, vec4(1, 1, 1, 1));
            m_uiButtons.push_back({{x, y}, {x + 118, y + 26}, (u32)(10 + i)});
        }
    }

    // === controles (abajo-izquierda) ==========================================
    {
        const char* help[] = {
            "WASD moverse  |  ESPACIO saltar  |  SHIFT correr",
            "Raton apuntar  |  Clic izq. disparar  |  Clic der. apuntar",
            "1-5 armas  |  R recargar  |  E abrir cofre  |  H pocion",
            "Q construir  |  Z pared  X suelo  C rampa  V techo",
        };
        f32 hy = H - 128;
        for (const char* line : help) {
            textOutlined(r, line, 30, hy, 2, vec4(0.75f, 0.8f, 0.9f, 0.85f));
            hy += 24;
        }
    }
    textOutlined(r, "100 combatientes - isla La Boveda - 1 victorioso",
                 30, 22, 2, vec4(0.6f, 0.65f, 0.75f, 0.8f));
}

void Game::renderEndScreen() {
    Renderer& r = *m_r;
    const f32 W = (f32)r.viewW(), H = (f32)r.viewH();
    r.uiQuad(0, 0, W, H, vec4(0, 0, 0, 0.55f));
    if (m_state == State::Victory) {
        textOutlined(r, "VICTORIA REAL!", W * 0.5f - 250, H * 0.3f, 8, vec4(1, 0.85f, 0.2f, 1));
    } else {
        textOutlined(r, "ELIMINADO", W * 0.5f - 160, H * 0.3f, 8, vec4(1, 0.3f, 0.25f, 1));
        char place[64];
        std::snprintf(place, sizeof(place), "Puesto #%d de 100", aliveCount() + 1);
        textOutlined(r, place, W * 0.5f - 100, H * 0.3f + 70, 3, vec4(1, 1, 1, 1));
    }
    char stats[128];
    std::snprintf(stats, sizeof(stats), "Eliminaciones: %d   |   Victorias: %u", m_kills, m_wins);
    textOutlined(r, stats, W * 0.5f - 150, H * 0.48f, 2, vec4(0.9f, 0.9f, 0.95f, 1));

    // boton volver a jugar (amarillo, mismo estilo)
    const f32 bx = W * 0.5f - 150, by = H * 0.56f;
    r.uiQuad(bx - 2, by - 2, 304, 56, vec4(1, 0.85f, 0.3f, 0.4f));
    panel(r, bx, by, 300, 52, vec4(0.99f, 0.80f, 0.10f, 1.0f), 8);
    const char* txt = "JUGAR OTRA VEZ";
    const f32 tw = r.uiTextWidth(txt, 3);
    r.uiText(txt, bx + 150 - tw * 0.5f, by + 17, 3, vec4(0.09f, 0.07f, 0.02f, 1));
    m_uiButtons.push_back({{bx, by}, {bx + 300, by + 52}, 2});
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
}

} // namespace game
