// ============================================================================
//  SkyVault Engine - Matematicas: GLM (libreria auxiliar permitida) + capa
//  propia de utilidades, geometria y armonicos esfericos para light probes.
// ============================================================================
#pragma once

#include "core/Core.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>

namespace sv {

using vec2  = glm::vec2;
using vec3  = glm::vec3;
using vec4  = glm::vec4;
using ivec2 = glm::ivec2;
using ivec3 = glm::ivec3;
using uvec2 = glm::uvec2;
using mat3  = glm::mat3;
using mat4  = glm::mat4;
using quat  = glm::quat;
using glm::mix; using glm::min; using glm::max; using glm::abs; using glm::clamp;

// ---------------------------------------------------------------------------
// Utilidades escalares
// ---------------------------------------------------------------------------
template <class T> [[nodiscard]] constexpr T clamp01(T v) { return v < T(0) ? T(0) : (v > T(1) ? T(1) : v); }
template <class T> [[nodiscard]] constexpr T lerpT(T a, T b, T t) { return a + (b - a) * t; }
[[nodiscard]] f32 smoothstep(f32 e0, f32 e1, f32 x);
[[nodiscard]] f32 smootherstep(f32 e0, f32 e1, f32 x);
[[nodiscard]] f32 damp(f32 current, f32 target, f32 rate, f32 dt);   // interpolacion exponencial
[[nodiscard]] f32 moveTowards(f32 current, f32 target, f32 maxDelta);
[[nodiscard]] f32 remap(f32 v, f32 a0, f32 a1, f32 b0, f32 b1);
[[nodiscard]] f32 wrapAngle(f32 a);                                  // [-PI, PI]
[[nodiscard]] f32 easeOutCubic(f32 t);
[[nodiscard]] f32 easeInOutCubic(f32 t);
[[nodiscard]] f32 easeOutBack(f32 t);
[[nodiscard]] f32 bounceOut(f32 t);

// ---------------------------------------------------------------------------
// Matrices
// ---------------------------------------------------------------------------
[[nodiscard]] mat4 composeTRS(vec3 pos, quat rot, vec3 scale);
[[nodiscard]] mat4 perspectiveFov(f32 fovYRadians, f32 aspect, f32 zNear, f32 zFar);
[[nodiscard]] mat4 orthoBox(f32 l, f32 r, f32 b, f32 t, f32 zNear, f32 zFar);
[[nodiscard]] vec3 forwardVector(quat q);   // -Z de la rotacion
[[nodiscard]] vec3 rightVector(quat q);
[[nodiscard]] quat quatLookDir(vec3 dir);   // orientacion que mira a dir (up=+Y)

// ---------------------------------------------------------------------------
// Color
// ---------------------------------------------------------------------------
[[nodiscard]] constexpr u32 packRGBA(u8 r, u8 g, u8 b, u8 a) {
    return (static_cast<u32>(r)) | (static_cast<u32>(g) << 8) | (static_cast<u32>(b) << 16) | (static_cast<u32>(a) << 24);
}
[[nodiscard]] constexpr u32 packRGB(u8 r, u8 g, u8 b) { return packRGBA(r, g, b, 255u); }
constexpr void unpackRGBA(u32 c, u8& r, u8& g, u8& b, u8& a) {
    r = static_cast<u8>(c & 0xFF); g = static_cast<u8>((c >> 8) & 0xFF);
    b = static_cast<u8>((c >> 16) & 0xFF); a = static_cast<u8>((c >> 24) & 0xFF);
}
[[nodiscard]] constexpr u32 rgbaToVec4Color(u32 c) { return c; } // util para UI
[[nodiscard]] vec3 rgbToHsv(vec3 rgb);
[[nodiscard]] vec3 hsvToRgb(vec3 hsv);
[[nodiscard]] constexpr u32 withAlpha(u32 rgb, u8 a) { return (rgb & 0x00FFFFFFu) | (static_cast<u32>(a) << 24); }
[[nodiscard]] constexpr u32 fade(u32 rgba, f32 t) {
    return (rgba & 0x00FFFFFFu) | (static_cast<u32>(static_cast<u8>(clamp01(t) * 255.0f)) << 24);
}

// ---------------------------------------------------------------------------
// Geometria
// ---------------------------------------------------------------------------
struct AABB {
    vec3 bmin{0.0f, 0.0f, 0.0f};
    vec3 bmax{0.0f, 0.0f, 0.0f};

    [[nodiscard]] static AABB fromCenterHalf(vec3 c, vec3 h) { return AABB{c - h, c + h}; }
    [[nodiscard]] static AABB fromMinMax(vec3 lo, vec3 hi) { return AABB{lo, hi}; }
    void extend(vec3 p) { bmin = glm::min(bmin, p); bmax = glm::max(bmax, p); }
    void extend(const AABB& o) { bmin = glm::min(bmin, o.bmin); bmax = glm::max(bmax, o.bmax); }
    [[nodiscard]] vec3 center() const { return (bmin + bmax) * 0.5f; }
    [[nodiscard]] vec3 half() const { return (bmax - bmin) * 0.5f; }
    [[nodiscard]] vec3 size() const { return bmax - bmin; }
    [[nodiscard]] bool valid() const { return bmin.x <= bmax.x && bmin.y <= bmax.y && bmin.z <= bmax.z; }
    [[nodiscard]] bool contains(vec3 p) const { return p.x >= bmin.x && p.x <= bmax.x && p.y >= bmin.y && p.y <= bmax.y && p.z >= bmin.z && p.z <= bmax.z; }
    [[nodiscard]] bool contains2D(vec2 p) const { return p.x >= bmin.x && p.x <= bmax.x && p.y >= bmin.z && p.y <= bmax.z; }
    [[nodiscard]] bool intersects(const AABB& o) const {
        return bmin.x <= o.bmax.x && bmax.x >= o.bmin.x &&
               bmin.y <= o.bmax.y && bmax.y >= o.bmin.y &&
               bmin.z <= o.bmax.z && bmax.z >= o.bmin.z;
    }
    [[nodiscard]] bool intersectsSphere(vec3 c, f32 r) const {
        const vec3 q = glm::clamp(c, bmin, bmax);
        return glm::distance2(q, c) <= r * r;
    }
    [[nodiscard]] AABB expanded(f32 e) const { return AABB{bmin - vec3(e), bmax + vec3(e)}; }
    [[nodiscard]] AABB transformed(const mat4& m) const; // esquinas transformadas (AABB envolvente)
    [[nodiscard]] f32 diagonal() const { return glm::length(size()); }
};

struct Sphere { vec3 c{0.0f}; f32 r = 1.0f; };

struct Ray {
    vec3 origin{0.0f};
    vec3 dir{0.0f, 0.0f, -1.0f}; // normalizado
    [[nodiscard]] vec3 at(f32 t) const { return origin + dir * t; }
};

struct Plane {
    vec3 n{0.0f, 1.0f, 0.0f};
    f32  d = 0.0f;
    [[nodiscard]] static Plane fromNormalPoint(vec3 n_, vec3 p) { Plane pl; pl.n = glm::normalize(n_); pl.d = -glm::dot(pl.n, p); return pl; }
    [[nodiscard]] f32 distance(vec3 p) const { return glm::dot(n, p) + d; }
};

struct Capsule {
    vec3 a{0.0f}, b{0.0f};
    f32  r = 0.5f;
};

struct Frustum {
    Plane p[6]; // L R B T N F
    void fromMatrix(const mat4& viewProj);
    [[nodiscard]] bool containsPoint(vec3 pt) const;
    [[nodiscard]] bool intersectsAABB(const AABB& box) const; // test de vertice positivo
    [[nodiscard]] bool intersectsSphere(vec3 c, f32 r) const;
    [[nodiscard]] bool intersectsCircle2D(vec2 c, f32 r) const; // para tormenta/minimapa (ignora Y)
};

// Intersecciones
[[nodiscard]] bool  rayAABB(const Ray& r, const AABB& box, f32& tOut, vec3* normalOut = nullptr);
[[nodiscard]] bool  raySphere(const Ray& r, vec3 center, f32 radius, f32& tOut);
[[nodiscard]] bool  rayPlane(const Ray& r, const Plane& pl, f32& tOut);
[[nodiscard]] bool  segmentAABB(vec3 a, vec3 b, const AABB& box, f32& tOut);
[[nodiscard]] bool  segmentSphere(vec3 a, vec3 b, vec3 center, f32 radius, f32& tOut);
[[nodiscard]] vec3  closestPointOnSegment(vec3 p, vec3 a, vec3 b);
[[nodiscard]] vec3  closestPointOnAABB(vec3 p, const AABB& box);
[[nodiscard]] f32  segmentPointDist2(vec3 a, vec3 b, vec3 p);
[[nodiscard]] bool pointInTriangle2D(vec2 p, vec2 a, vec2 b, vec2 c);

// ---------------------------------------------------------------------------
// Armonicos esfericos de orden 2 (light probes / GI aproximada barata)
// ---------------------------------------------------------------------------
struct SH9 {
    vec3 c[9]{}; // coeficientes RGB

    void addSample(vec3 dir, vec3 radiance, f32 weight = 1.0f);
    [[nodiscard]] vec3 evaluate(vec3 n) const; // n normalizado
    void clear() { for (auto& v : c) v = vec3(0.0f); }
    [[nodiscard]] bool isBlack() const;
};

} // namespace sv
