// ============================================================================
//  SKYVAULT Royale - engine/physics/Physics.h
//  Motor de colision propio (sin dependencias): terreno analitico + AABBs +
//  cilindros (arboles) + rampas. Controlador de personaje y raycasts.
// ============================================================================
#pragma once

#include "core/Core.h"
#include "math/Math.h"
#include "world/Terrain.h"

namespace sv {

enum class HitType : u8 { None = 0, Terrain, Box, Tree };
enum class ColliderOwner : u8 { World = 0, Build, Bot, Player };

struct ColliderBox {
    AABB box;
    ColliderOwner owner = ColliderOwner::World;
    u32 id = 0;            // indice de pieza de construccion / edificio
    bool breakable = false;
    i32 hp = 0;
};

struct ColliderRamp {      // plano inclinado andable dentro de un AABB
    AABB box;
    u8 dir = 0;            // 0:+X 1:+Z 2:-X 3:-Z (cuesta arriba)
};

struct RaycastHit {
    f32 t = -1.0f;
    vec3 point, normal;
    HitType type = HitType::None;
    u32 id = 0;
    ColliderOwner owner = ColliderOwner::World;
    bool breakable = false;
};

class CollisionWorld {
public:
    void setTerrain(const Terrain* t) { m_terrain = t; }
    void clearDynamic() { m_boxes.clear(); m_ramps.clear(); }

    void addBox(const ColliderBox& b) { m_boxes.push_back(b); }
    void addRamp(const ColliderRamp& r) { m_ramps.push_back(r); }
    void addTree(vec2 xz, f32 radius, f32 height) {
        m_trees.push_back(vec4(xz.x, 0, xz.y, radius)); m_treeH.push_back(height);
    }
    void reserveBoxes(usize n) { m_boxes.reserve(n); }

    // altura del suelo bajo los pies (terreno + cajas + rampas)
    [[nodiscard]] f32 groundHeight(f32 x, f32 z, f32 feetY) const;
    // mueve un cilindro (radio, altura) resolviendo colisiones horizontales
    void moveCapsule(vec3& pos, const vec3& delta, f32 radius, f32 height) const;
    // raycast contra todo el mundo
    [[nodiscard]] bool raycast(const vec3& origin, const vec3& dir, f32 maxDist,
                               RaycastHit& out) const;
    [[nodiscard]] usize boxCount() const { return m_boxes.size(); }
    [[nodiscard]] const ColliderBox& box(usize i) const { return m_boxes[i]; }

private:
    [[nodiscard]] bool raycastTerrain(const vec3& o, const vec3& d, f32 maxDist,
                                      RaycastHit& out) const;
    [[nodiscard]] bool raycastBoxes(const vec3& o, const vec3& d, f32 maxDist,
                                    RaycastHit& out) const;
    [[nodiscard]] bool raycastTrees(const vec3& o, const vec3& d, f32 maxDist,
                                    RaycastHit& out) const;

    const Terrain* m_terrain = nullptr;
    std::vector<ColliderBox> m_boxes;
    std::vector<ColliderRamp> m_ramps;
    std::vector<vec4> m_trees;      // x,z en .x/.z, radio en .w
    std::vector<f32> m_treeH;
};

} // namespace sv
