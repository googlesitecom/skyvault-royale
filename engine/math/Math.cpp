// ============================================================================
//  SkyVault Engine - implementacion de matematicas / geometria
// ============================================================================
#include "math/Math.h"

namespace sv {

// ---------------------------------------------------------------------------
// Escalares
// ---------------------------------------------------------------------------
f32 smoothstep(f32 e0, f32 e1, f32 x) {
    const f32 t = clamp01((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}
f32 smootherstep(f32 e0, f32 e1, f32 x) {
    const f32 t = clamp01((x - e0) / (e1 - e0));
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}
f32 damp(f32 current, f32 target, f32 rate, f32 dt) {
    return target + (current - target) * std::exp(-rate * dt);
}
f32 moveTowards(f32 current, f32 target, f32 maxDelta) {
    const f32 d = target - current;
    if (std::fabs(d) <= maxDelta) return target;
    return current + (d > 0.0f ? maxDelta : -maxDelta);
}
f32 remap(f32 v, f32 a0, f32 a1, f32 b0, f32 b1) {
    if (a1 - a0 == 0.0f) return b0;
    return b0 + (v - a0) / (a1 - a0) * (b1 - b0);
}
f32 wrapAngle(f32 a) {
    while (a > PI_F) a -= TAU_F;
    while (a < -PI_F) a += TAU_F;
    return a;
}
f32 easeOutCubic(f32 t) { t = clamp01(t); const f32 u = 1.0f - t; return 1.0f - u * u * u; }
f32 easeInOutCubic(f32 t) {
    t = clamp01(t);
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}
f32 easeOutBack(f32 t) {
    t = clamp01(t);
    const f32 c1 = 1.70158f, c3 = c1 + 1.0f;
    return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
}
f32 bounceOut(f32 t) {
    t = clamp01(t);
    const f32 n1 = 7.5625f, d1 = 2.75f;
    if (t < 1.0f / d1) return n1 * t * t;
    if (t < 2.0f / d1) { t -= 1.5f / d1; return n1 * t * t + 0.75f; }
    if (t < 2.5f / d1) { t -= 2.25f / d1; return n1 * t * t + 0.9375f; }
    t -= 2.625f / d1;
    return n1 * t * t + 0.984375f;
}

// ---------------------------------------------------------------------------
// Matrices
// ---------------------------------------------------------------------------
mat4 composeTRS(vec3 pos, quat rot, vec3 scale) {
    const mat4 m = glm::mat4_cast(rot);
    return glm::translate(mat4(1.0f), pos) * m * glm::scale(mat4(1.0f), scale);
}
mat4 perspectiveFov(f32 fovYRadians, f32 aspect, f32 zNear, f32 zFar) {
    return glm::perspective(fovYRadians, aspect, zNear, zFar);
}
mat4 orthoBox(f32 l, f32 r, f32 b, f32 t, f32 zNear, f32 zFar) {
    return glm::ortho(l, r, b, t, zNear, zFar);
}
vec3 forwardVector(quat q) { return glm::normalize(q * vec3(0.0f, 0.0f, -1.0f)); }
vec3 rightVector(quat q)   { return glm::normalize(q * vec3(1.0f, 0.0f, 0.0f)); }

quat quatLookDir(vec3 dir) {
    if (glm::length2(dir) < 1e-8f) return quat(1.0f, 0.0f, 0.0f, 0.0f);
    dir = glm::normalize(dir);
    if (std::fabs(dir.y) > 0.9999f) { // vertical: elige un "up" alternativo
        const vec3 up = dir.y > 0.0f ? vec3(0.0f, 0.0f, 1.0f) : vec3(0.0f, 0.0f, -1.0f);
        return glm::quatLookAt(-dir, up); // glm espera direccion de mira
    }
    return glm::quatLookAt(dir, vec3(0.0f, 1.0f, 0.0f));
}

// ---------------------------------------------------------------------------
// Color
// ---------------------------------------------------------------------------
vec3 rgbToHsv(vec3 rgb) {
    const f32 mx = std::max({rgb.x, rgb.y, rgb.z}), mn = std::min({rgb.x, rgb.y, rgb.z});
    const f32 d = mx - mn;
    f32 h = 0.0f;
    if (d > 1e-8f) {
        if (mx == rgb.x)      h = (rgb.y - rgb.z) / d + (rgb.y < rgb.z ? 6.0f : 0.0f);
        else if (mx == rgb.y) h = (rgb.z - rgb.x) / d + 2.0f;
        else                  h = (rgb.x - rgb.y) / d + 4.0f;
        h /= 6.0f;
    }
    return vec3(h, mx <= 1e-8f ? 0.0f : d / mx, mx);
}
vec3 hsvToRgb(vec3 hsv) {
    const i32 i = static_cast<i32>(std::floor(hsv.x * 6.0f));
    const f32 f = hsv.x * 6.0f - static_cast<f32>(i);
    const f32 p = hsv.z * (1.0f - hsv.y);
    const f32 q = hsv.z * (1.0f - f * hsv.y);
    const f32 t = hsv.z * (1.0f - (1.0f - f) * hsv.y);
    switch (((i % 6) + 6) % 6) {
        case 0: return vec3(hsv.z, t, p);
        case 1: return vec3(q, hsv.z, p);
        case 2: return vec3(p, hsv.z, t);
        case 3: return vec3(p, q, hsv.z);
        case 4: return vec3(t, p, hsv.z);
        default:return vec3(hsv.z, p, q);
    }
}

// ---------------------------------------------------------------------------
// Geometria
// ---------------------------------------------------------------------------
AABB AABB::transformed(const mat4& m) const {
    // Transforma las 8 esquinas y devuelve la caja envolvente (aproximacion segura)
    AABB out;
    out.bmin = vec3(INF_F);
    out.bmax = vec3(-INF_F);
    for (i32 i = 0; i < 8; ++i) {
        const vec3 corner(bmin.x + ((i & 1) ? size().x : 0.0f),
                          bmin.y + ((i & 2) ? size().y : 0.0f),
                          bmin.z + ((i & 4) ? size().z : 0.0f));
        out.extend(vec3(m * vec4(corner, 1.0f)));
    }
    return out;
}

void Frustum::fromMatrix(const mat4& m) {
    // Extraccion Gribb-Hartmann (GLM: columnas = m[col]; fila r = (m[0][r], m[1][r], m[2][r], m[3][r]))
    const auto row = [&](i32 r) { return vec4(m[0][r], m[1][r], m[2][r], m[3][r]); };
    const vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
    const vec4 planes[6] = { r3 + r0, r3 - r0, r3 + r1, r3 - r1, r3 + r2, r3 - r2 };
    for (i32 i = 0; i < 6; ++i) {
        const vec3 n = vec3(planes[i]);
        const f32 len = glm::length(n);
        if (len < 1e-8f) { p[i] = Plane{vec3(0.0f, 1.0f, 0.0f), 0.0f}; continue; }
        p[i].n = n / len;
        p[i].d = planes[i].w / len;
    }
}

bool Frustum::containsPoint(vec3 pt) const {
    for (const auto& pl : p)
        if (pl.distance(pt) < 0.0f) return false;
    return true;
}

bool Frustum::intersectsAABB(const AABB& box) const {
    for (const auto& pl : p) {
        // vertice positivo: el mas lejano en la direccion de la normal
        vec3 pv = box.bmin;
        if (pl.n.x >= 0.0f) pv.x = box.bmax.x;
        if (pl.n.y >= 0.0f) pv.y = box.bmax.y;
        if (pl.n.z >= 0.0f) pv.z = box.bmax.z;
        if (pl.distance(pv) < 0.0f) return false;
    }
    return true;
}

bool Frustum::intersectsSphere(vec3 c, f32 r) const {
    for (const auto& pl : p)
        if (pl.distance(c) < -r) return false;
    return true;
}

bool Frustum::intersectsCircle2D(vec2 c, f32 r) const {
    return intersectsSphere(vec3(c.x, p[2].n.y >= 0.0f ? -1e6f : 1e6f, c.y), r * 1.05f);
}

bool rayAABB(const Ray& r, const AABB& box, f32& tOut, vec3* normalOut) {
    // Metodo de slab
    f32 tmin = 0.0f, tmax = 1e30f;
    vec3 hitNormal(0.0f);
    for (i32 axis = 0; axis < 3; ++axis) {
        const f32 ro = r.origin[axis], rd = r.dir[axis];
        const f32 lo = box.bmin[axis], hi = box.bmax[axis];
        if (std::fabs(rd) < 1e-8f) {
            if (ro < lo || ro > hi) return false;
            continue;
        }
        f32 t1 = (lo - ro) / rd, t2 = (hi - ro) / rd;
        vec3 n(0.0f);
        n[axis] = -1.0f;
        if (t1 > t2) { std::swap(t1, t2); n[axis] = 1.0f; }
        if (t1 > tmin) { tmin = t1; hitNormal = n; }
        tmax = std::min(tmax, t2);
        if (tmin > tmax) return false;
    }
    tOut = tmin;
    if (normalOut) *normalOut = hitNormal;
    return true;
}

bool raySphere(const Ray& r, vec3 center, f32 radius, f32& tOut) {
    const vec3 oc = r.origin - center;
    const f32 b = glm::dot(oc, r.dir);
    const f32 c = glm::dot(oc, oc) - radius * radius;
    const f32 disc = b * b - c;
    if (disc < 0.0f) return false;
    const f32 sq = std::sqrt(disc);
    const f32 t0 = -b - sq, t1 = -b + sq;
    if (t0 >= 0.0f) { tOut = t0; return true; }
    if (t1 >= 0.0f) { tOut = t1; return true; }
    return false;
}

bool rayPlane(const Ray& r, const Plane& pl, f32& tOut) {
    const f32 denom = glm::dot(pl.n, r.dir);
    if (std::fabs(denom) < 1e-8f) return false;
    const f32 t = -(glm::dot(pl.n, r.origin) + pl.d) / denom;
    if (t < 0.0f) return false;
    tOut = t;
    return true;
}

bool segmentAABB(vec3 a, vec3 b, const AABB& box, f32& tOut) {
    const vec3 d = b - a;
    const f32 len = glm::length(d);
    if (len < 1e-8f) { tOut = 0.0f; return box.contains(a); }
    Ray r{a, d / len};
    if (rayAABB(r, box, tOut) && tOut <= len) return true;
    return false;
}

bool segmentSphere(vec3 a, vec3 b, vec3 center, f32 radius, f32& tOut) {
    const vec3 d = b - a;
    const f32 len = glm::length(d);
    if (len < 1e-8f) {
        if (glm::distance(a, center) <= radius) { tOut = 0.0f; return true; }
        return false;
    }
    Ray r{a, d / len};
    if (raySphere(r, center, radius, tOut) && tOut <= len) return true;
    return false;
}

vec3 closestPointOnSegment(vec3 p, vec3 a, vec3 b) {
    const vec3 ab = b - a;
    const f32 len2 = glm::length2(ab);
    if (len2 < 1e-10f) return a;
    const f32 t = clamp01(glm::dot(p - a, ab) / len2);
    return a + ab * t;
}

vec3 closestPointOnAABB(vec3 p, const AABB& box) { return glm::clamp(p, box.bmin, box.bmax); }

f32 segmentPointDist2(vec3 a, vec3 b, vec3 p) {
    return glm::distance2(p, closestPointOnSegment(p, a, b));
}

bool pointInTriangle2D(vec2 p, vec2 a, vec2 b, vec2 c) {
    const f32 d1 = (p.x - b.x) * (a.y - b.y) - (a.x - b.x) * (p.y - b.y);
    const f32 d2 = (p.x - c.x) * (b.y - c.y) - (b.x - c.x) * (p.y - c.y);
    const f32 d3 = (p.x - a.x) * (c.y - a.y) - (c.x - a.x) * (p.y - a.y);
    const bool hasNeg = (d1 < 0.0f) || (d2 < 0.0f) || (d3 < 0.0f);
    const bool hasPos = (d1 > 0.0f) || (d2 > 0.0f) || (d3 > 0.0f);
    return !(hasNeg && hasPos);
}

// ---------------------------------------------------------------------------
// SH9 - base real de armonicos esfericos hasta l=2
// ---------------------------------------------------------------------------
static void shBasis(vec3 n, f32 out[9]) {
    const f32 x = n.x, y = n.y, z = n.z;
    out[0] = 0.2820947918f;                                  // Y00
    out[1] = 0.4886025119f * y;                              // Y1-1
    out[2] = 0.4886025119f * z;                              // Y10
    out[3] = 0.4886025119f * x;                              // Y11
    out[4] = 1.0925484306f * x * y;                          // Y2-2
    out[5] = 1.0925484306f * y * z;                          // Y2-1
    out[6] = 0.3153915653f * (3.0f * z * z - 1.0f);          // Y20
    out[7] = 1.0925484306f * x * z;                          // Y21
    out[8] = 0.5462742153f * (x * x - y * y);                // Y22
}

void SH9::addSample(vec3 dir, vec3 radiance, f32 weight) {
    f32 basis[9];
    shBasis(glm::normalize(dir), basis);
    for (i32 i = 0; i < 9; ++i) c[i] += radiance * (basis[i] * weight);
}

vec3 SH9::evaluate(vec3 n) const {
    f32 basis[9];
    shBasis(glm::normalize(n), basis);
    vec3 result(0.0f);
    for (i32 i = 0; i < 9; ++i) result += c[i] * basis[i];
    return vec3(std::max(result.x, 0.0f), std::max(result.y, 0.0f), std::max(result.z, 0.0f));
}

bool SH9::isBlack() const {
    for (const auto& v : c) if (glm::length2(v) > 1e-10f) return false;
    return true;
}

} // namespace sv
