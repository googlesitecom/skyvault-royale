// ============================================================================
//  SKYVAULT Royale - engine/physics/Physics.cpp
// ============================================================================
#include "physics/Physics.h"
#include <algorithm>

namespace sv {

// ---------------------------------------------------------------------------
// Suelo
// ---------------------------------------------------------------------------
f32 CollisionWorld::groundHeight(f32 x, f32 z, f32 feetY) const {
    f32 h = m_terrain ? m_terrain->height(x, z) : 0.0f;
    // cajas: si los pies estan por encima de la parte superior, se puede pisar
    for (const auto& b : m_boxes) {
        const AABB& a = b.box;
        if (x < a.bmin.x || x > a.bmax.x || z < a.bmin.z || z > a.bmax.z) continue;
        if (a.bmax.y <= feetY + 0.55f && a.bmax.y > h) h = a.bmax.y;
    }
    // rampas: altura del plano inclinado si esta bajo los pies
    for (const auto& r : m_ramps) {
        const AABB& a = r.box;
        if (x < a.bmin.x || x > a.bmax.x || z < a.bmin.z || z > a.bmax.z) continue;
        f32 t = 0.5f;
        if (r.dir == 0)      t = clamp01((x - a.bmin.x) / (a.bmax.x - a.bmin.x));
        else if (r.dir == 2) t = clamp01((a.bmax.x - x) / (a.bmax.x - a.bmin.x));
        else if (r.dir == 1) t = clamp01((z - a.bmin.z) / (a.bmax.z - a.bmin.z));
        else                 t = clamp01((a.bmax.z - z) / (a.bmax.z - a.bmin.z));
        const f32 rh = lerpT(a.bmin.y, a.bmax.y, t);
        if (rh <= feetY + 0.85f && rh > h) h = rh;
    }
    return h;
}

// ---------------------------------------------------------------------------
// Movimiento de capsula (cilindro) con resolucion por ejes
// ---------------------------------------------------------------------------
void CollisionWorld::moveCapsule(vec3& pos, const vec3& delta, f32 radius, f32 height) const {
    // Eje Y: el juego gestiona gravedad; aqui solo pasos verticales finos
    // (subir rampas) via groundHeight. Movimiento horizontal por ejes:
    const f32 bodyLo = pos.y + 0.25f;
    const f32 bodyHi = pos.y + height;

    auto blocked = [&](const vec3& p) -> bool {
        // arboles (cilindros)
        for (usize i = 0; i < m_trees.size(); ++i) {
            const vec4& tr = m_trees[i];
            if (p.y > tr.y + m_treeH[i] || bodyHi < tr.y) continue;
            const f32 dx = p.x - tr.x, dz = p.z - tr.z;
            const f32 r2 = (radius + tr.w) * (radius + tr.w);
            if (dx * dx + dz * dz < r2) return true;
        }
        // cajas (a la altura del cuerpo)
        for (const auto& b : m_boxes) {
            const AABB& a = b.box;
            if (bodyHi < a.bmin.y || bodyLo > a.bmax.y) continue;
            // ¿la caja es "andable" (se sube por encima)? si el tope esta a
            // menos de 0.55 sobre los pies, no bloquea (step-up)
            if (a.bmax.y <= pos.y + 0.55f) continue;
            if (p.x + radius > a.bmin.x && p.x - radius < a.bmax.x &&
                p.z + radius > a.bmin.z && p.z - radius < a.bmax.z) return true;
        }
        return false;
    };

    vec3 p = pos;
    vec3 q = p; q.x += delta.x;
    if (!blocked(q)) p.x = q.x;
    q = p; q.z += delta.z;
    if (!blocked(q)) p.z = q.z;
    pos = p;
}

// ---------------------------------------------------------------------------
// Raycasts
// ---------------------------------------------------------------------------
bool CollisionWorld::raycastTerrain(const vec3& o, const vec3& d, f32 maxDist,
                                    RaycastHit& out) const {
    if (!m_terrain) return false;
    const f32 step = 1.5f;
    f32 t = 0;
    f32 prevH = o.y - m_terrain->height(o.x, o.z);
    for (t = step; t <= maxDist; t += step) {
        const vec3 p = o + d * t;
        const f32 dh = p.y - m_terrain->height(p.x, p.z);
        if (dh <= 0.0f) {
            // biseccion
            f32 lo = t - step, hi = t;
            for (i32 i = 0; i < 10; ++i) {
                const f32 mid = (lo + hi) * 0.5f;
                const vec3 mp = o + d * mid;
                if (mp.y - m_terrain->height(mp.x, mp.z) <= 0.0f) hi = mid;
                else lo = mid;
            }
            out.t = hi;
            out.point = o + d * hi;
            out.normal = m_terrain->normal(out.point.x, out.point.z);
            out.type = HitType::Terrain;
            return true;
        }
        prevH = dh;
        if (p.y > 800.0f && d.y > 0.0f) return false;   // nunca volvera a bajar
    }
    (void)prevH;
    return false;
}

bool CollisionWorld::raycastBoxes(const vec3& o, const vec3& d, f32 maxDist,
                                  RaycastHit& out) const {
    bool hit = false;
    f32 best = maxDist;
    for (const auto& b : m_boxes) {
        f32 t;
        vec3 n;
        if (rayAABB(Ray{o, d}, b.box, t) && t > 0.05f && t < best) {
            // normal del plano golpeado
            const vec3 p = o + d * t;
            const vec3 c = (b.box.bmin + b.box.bmax) * 0.5f;
            const vec3 sz = (b.box.bmax - b.box.bmin) * 0.5f;
            const vec3 lp = p - c;
            if (std::fabs(lp.x) > std::fabs(lp.y) && std::fabs(lp.x) > std::fabs(lp.z))
                n = vec3(lp.x > 0 ? 1 : -1, 0, 0);
            else if (std::fabs(lp.y) > std::fabs(lp.z))
                n = vec3(0, lp.y > 0 ? 1 : -1, 0);
            else
                n = vec3(0, 0, lp.z > 0 ? 1 : -1);
            (void)sz;
            best = t;
            out.t = t; out.point = p; out.normal = n;
            out.type = HitType::Box;
            out.owner = b.owner; out.id = b.id;
            out.breakable = b.breakable;
            hit = true;
        }
    }
    return hit;
}

bool CollisionWorld::raycastTrees(const vec3& o, const vec3& d, f32 maxDist,
                                  RaycastHit& out) const {
    bool hit = false;
    f32 best = maxDist;
    for (usize i = 0; i < m_trees.size(); ++i) {
        const vec4& tr = m_trees[i];
        const f32 h = m_treeH[i];
        // cilindro finito: 2D circulo + rango y
        const vec2 oc(o.x - tr.x, o.z - tr.z);
        const vec2 dc(d.x, d.z);
        const f32 a = dot(dc, dc);
        if (a < 1e-8f) continue;
        const f32 b = 2.0f * dot(oc, dc);
        const f32 c = dot(oc, oc) - tr.w * tr.w;
        const f32 disc = b * b - 4.0f * a * c;
        if (disc < 0) continue;
        const f32 sq = std::sqrt(disc);
        f32 t = (-b - sq) / (2.0f * a);
        if (t < 0) t = (-b + sq) / (2.0f * a);
        if (t < 0.05f || t >= best) continue;
        const vec3 p = o + d * t;
        if (p.y < tr.y || p.y > tr.y + h) continue;
        best = t;
        out.t = t;
        out.point = p;
        out.normal = normalize(vec3(p.x - tr.x, 0, p.z - tr.z));
        out.type = HitType::Tree;
        out.id = (u32)i;
        hit = true;
    }
    return hit;
}

bool CollisionWorld::raycast(const vec3& origin, const vec3& dir, f32 maxDist,
                             RaycastHit& out) const {
    out = RaycastHit{};
    RaycastHit best{};
    best.t = maxDist + 1.0f;
    bool any = false;
    RaycastHit tmp;
    if (raycastTerrain(origin, dir, maxDist, tmp) && tmp.t < best.t) { best = tmp; any = true; }
    if (raycastBoxes(origin, dir, maxDist, tmp) && tmp.t < best.t) { best = tmp; any = true; }
    if (raycastTrees(origin, dir, maxDist, tmp) && tmp.t < best.t) { best = tmp; any = true; }
    if (any) out = best;
    return any;
}

} // namespace sv
