// ============================================================================
//  SKYVAULT Royale - game/ProceduralModels.cpp
//  Characters and weapons built 100% from code (no third-party assets).
//  All shapes are authored boxes/cylinders with per-vertex colors, matching
//  the stylized look of the procedural terrain, trees and rocks.
//  Character: feet at y=0, faces +Z, total height ~1.8 m.
//  Weapons:   grip at origin, muzzle towards +Z, up is +Y.
// ============================================================================
#include "game/Game.h"
#include <cmath>

namespace game {

namespace {

// ---------------------------------------------------------------------------
// Tiny primitive builder (same vertex layout/winding as Game.cpp generators)
// ---------------------------------------------------------------------------
struct PrimBuilder {
    std::vector<Vertex> v;
    std::vector<u32>    idx;

    // Axis-aligned box between min/max corners. Per-face shade fakes AO.
    void box(vec3 mn, vec3 mx, vec3 col) {
        const vec3 c((mn + mx) * 0.5f), s((mx - mn) * 0.5f);
        static const vec3 nrm[6] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        // shade per face: +x -x +y -y +z -z
        const f32 shade[6] = {0.84f, 0.92f, 1.10f, 0.70f, 1.00f, 0.86f};
        for (u32 f = 0; f < 6; ++f) {
            const u32 b = (u32)v.size();
            const vec3& n = nrm[f];
            const vec3 t = std::fabs(n.x) > 0.5f ? vec3(0, 1, 0)
                          : (std::fabs(n.y) > 0.5f ? vec3(1, 0, 0) : vec3(1, 0, 0));
            const vec3 bt = cross(n, t);
            for (u32 k = 0; k < 4; ++k) {
                const f32 su = (k == 0 || k == 3) ? -1.0f : 1.0f;
                const f32 sv = (k < 2) ? -1.0f : 1.0f;
                const vec3 p = c + n * dot(s, n) + t * (su * dot(s, t)) + bt * (sv * dot(s, bt));
                v.push_back({p, n, {(f32)(k % 2), (f32)(k / 2)}, {1,0,0,1}, {}, {},
                             vec4(col * shade[f], 1.0f)});
            }
            idx.insert(idx.end(), {b, b + 2, b + 1, b, b + 3, b + 2});
        }
    }

    // Shorthand: centered box with full size.
    void boxC(vec3 c, vec3 size, vec3 col) { box(c - size * 0.5f, c + size * 0.5f, col); }

    // Open cylinder between two points (barrels, scope, handle).
    void cyl(vec3 a, vec3 b, f32 r, vec3 col, u32 seg = 8) {
        const vec3 dir = normalize(b - a);
        const vec3 up  = std::fabs(dir.y) > 0.95f ? vec3(1, 0, 0) : vec3(0, 1, 0);
        const vec3 t   = normalize(cross(dir, up));
        const vec3 bt  = cross(dir, t);
        const u32 base = (u32)v.size();
        for (u32 i = 0; i <= seg; ++i) {
            const f32 ang = TAU_F * (f32)i / (f32)seg;
            const vec3 n = t * std::cos(ang) + bt * std::sin(ang);
            v.push_back({a + n * r, n, {0, 0}, {1,0,0,1}, {}, {}, vec4(col, 1.0f)});
            v.push_back({b + n * r, n, {1, 0}, {1,0,0,1}, {}, {}, vec4(col, 1.0f)});
        }
        for (u32 i = 0; i < seg; ++i) {
            const u32 q = base + i * 2;
            idx.insert(idx.end(), {q, q + 1, q + 2, q + 2, q + 1, q + 3});
        }
    }

    [[nodiscard]] Mesh finish(Renderer& r) const {
        return r.createMesh(v.data(), (u32)v.size(), idx.data(), (u32)idx.size());
    }
};

// Shared palette (SKYVAULT house style: charcoal + teal + rust orange)
const vec3 Charcoal{0.13f, 0.145f, 0.17f};
const vec3 Slate   {0.24f, 0.28f, 0.35f};
const vec3 SlateL  {0.30f, 0.35f, 0.43f};
const vec3 Jacket  {0.09f, 0.38f, 0.42f};
const vec3 JacketL {0.14f, 0.52f, 0.56f};
const vec3 Rust    {0.78f, 0.40f, 0.14f};
const vec3 Orange  {0.92f, 0.55f, 0.13f};
const vec3 Steel   {0.52f, 0.55f, 0.60f};
const vec3 SteelD  {0.20f, 0.21f, 0.24f};
const vec3 Gunmetal{0.16f, 0.17f, 0.19f};
const vec3 Polymer {0.12f, 0.13f, 0.15f};
const vec3 WoodGrip{0.42f, 0.27f, 0.13f};
const vec3 Visor   {0.38f, 0.95f, 1.00f};

} // namespace

// ---------------------------------------------------------------------------
// Character: stylized "Buceador de La Boveda" explorer suit.
// Feet at y=0, facing +Z, ~1.8 m tall. Bots get per-instance tints.
// ---------------------------------------------------------------------------
Mesh Game::makeCharacterMesh() {
    PrimBuilder p;

    // --- legs -------------------------------------------------------------
    for (f32 sx : {-1.0f, 1.0f}) {
        const f32 x = sx * 0.105f;
        p.box(vec3(x - 0.085f, 0.00f, -0.09f), vec3(x + 0.085f, 0.145f, 0.15f), Charcoal); // boot
        p.box(vec3(x - 0.075f, 0.145f, -0.075f), vec3(x + 0.075f, 0.52f, 0.075f), Slate);  // shin
        p.box(vec3(x - 0.080f, 0.50f, -0.085f), vec3(x + 0.080f, 0.60f, 0.085f), Orange);  // knee pad
        p.box(vec3(x - 0.090f, 0.60f, -0.095f), vec3(x + 0.090f, 0.94f, 0.095f), SlateL);  // thigh
    }

    // --- hips + torso -------------------------------------------------------
    p.box(vec3(-0.19f, 0.94f, -0.125f), vec3(0.19f, 1.08f, 0.105f), Charcoal);             // pelvis
    p.box(vec3(-0.21f, 1.08f, -0.135f), vec3(0.21f, 1.46f, 0.115f), Jacket);               // jacket
    p.box(vec3(-0.15f, 1.14f, 0.055f), vec3(0.15f, 1.44f, 0.145f), JacketL);               // chest plate
    p.box(vec3(-0.16f, 1.12f, -0.29f), vec3(0.16f, 1.50f, -0.145f), Rust);                 // backpack
    p.box(vec3(-0.13f, 1.24f, -0.315f), vec3(0.13f, 1.40f, -0.29f), Orange);               // pack flap
    p.box(vec3(-0.20f, 1.07f, -0.03f), vec3(0.20f, 1.10f, 0.12f), Orange);                 // belt

    // --- arms (slightly out, static pose) ------------------------------------
    for (f32 sx : {-1.0f, 1.0f}) {
        const f32 x = sx * 0.275f;
        p.box(vec3(x - 0.065f, 1.36f, -0.095f), vec3(x + 0.065f, 1.50f, 0.095f), Charcoal); // shoulder
        p.box(vec3(x - 0.058f, 0.98f, -0.075f), vec3(x + 0.058f, 1.38f, 0.075f), SlateL);   // upper arm
        p.box(vec3(x - 0.052f, 0.60f, -0.068f), vec3(x + 0.052f, 0.99f, 0.068f), Slate);    // forearm
        p.box(vec3(x - 0.048f, 0.52f, -0.062f), vec3(x + 0.048f, 0.61f, 0.075f), JacketL);  // glove
    }

    // --- head + helmet --------------------------------------------------------
    p.box(vec3(-0.075f, 1.46f, -0.075f), vec3(0.075f, 1.56f, 0.075f), Charcoal);            // neck
    p.box(vec3(-0.125f, 1.56f, -0.115f), vec3(0.125f, 1.80f, 0.115f), Charcoal);            // helmet
    p.box(vec3(-0.092f, 1.615f, 0.105f), vec3(0.092f, 1.735f, 0.145f), Visor);              // visor
    p.box(vec3(-0.125f, 1.775f, -0.115f), vec3(0.125f, 1.815f, 0.115f), SlateL);            // crest
    p.box(vec3(0.075f, 1.80f, -0.02f), vec3(0.10f, 1.94f, 0.01f), Orange);                  // antenna

    return p.finish(*m_r);
}

// ---------------------------------------------------------------------------
// Weapons: grip at origin, muzzle +Z, up +Y. Sized to the WeaponDef offsets.
// ---------------------------------------------------------------------------
Mesh Game::makeWeaponMesh(WeaponKind k) {
    PrimBuilder p;

    switch (k) {

    // --- Pico "Recolector" (multi-tool, ~0.9 m) ------------------------------
    case WeaponKind::Pickaxe: {
        p.box(vec3(-0.038f, -0.045f, -0.12f), vec3(0.038f, 0.075f, 0.50f), Charcoal);   // handle
        p.box(vec3(-0.042f, 0.005f, 0.30f), vec3(0.042f, 0.055f, 0.345f), Orange);      // grip ring
        p.box(vec3(-0.22f, 0.01f, 0.40f), vec3(0.22f, 0.10f, 0.52f), Steel);            // head
        p.box(vec3(-0.34f, 0.025f, 0.415f), vec3(-0.22f, 0.085f, 0.505f), Steel * 0.85f); // blade
        p.box(vec3(0.22f, 0.03f, 0.42f), vec3(0.35f, 0.08f, 0.50f), SteelD);            // spike
        break;
    }

    // --- Escopeta Piston (~0.9 m) ---------------------------------------------
    case WeaponKind::Shotgun: {
        p.box(vec3(-0.032f, -0.065f, -0.29f), vec3(0.032f, 0.03f, -0.02f), WoodGrip);   // stock
        p.box(vec3(-0.045f, -0.018f, -0.02f), vec3(0.045f, 0.078f, 0.24f), Gunmetal);   // receiver
        p.cyl(vec3(0, 0.046f, 0.20f), vec3(0, 0.046f, 0.62f), 0.025f, SteelD);          // barrel
        p.cyl(vec3(0, 0.002f, 0.22f), vec3(0, 0.002f, 0.58f), 0.017f, SteelD * 0.9f);   // tube
        p.box(vec3(-0.037f, -0.055f, 0.30f), vec3(0.037f, -0.002f, 0.44f), WoodGrip);   // pump
        p.box(vec3(-0.012f, 0.078f, 0.56f), vec3(0.012f, 0.10f, 0.62f), Orange);        // front sight
        break;
    }

    // --- SMG Doble Cargador (~0.6 m) -------------------------------------------
    case WeaponKind::SMG: {
        p.box(vec3(-0.042f, -0.012f, -0.06f), vec3(0.042f, 0.072f, 0.30f), Polymer);    // body
        p.box(vec3(-0.030f, -0.175f, 0.02f), vec3(-0.006f, -0.012f, 0.10f), JacketL);   // mag L
        p.box(vec3(0.006f, -0.175f, 0.02f), vec3(0.030f, -0.012f, 0.10f), JacketL);     // mag R
        p.cyl(vec3(0, 0.036f, 0.30f), vec3(0, 0.036f, 0.42f), 0.016f, SteelD);          // barrel
        p.box(vec3(-0.012f, 0.012f, -0.19f), vec3(0.012f, 0.034f, -0.06f), Steel);      // wire stock
        p.box(vec3(-0.030f, 0.002f, -0.22f), vec3(0.030f, 0.052f, -0.19f), Polymer);    // butt plate
        p.box(vec3(-0.026f, 0.072f, 0.10f), vec3(0.026f, 0.095f, 0.20f), Steel);        // sight rail
        p.box(vec3(-0.028f, -0.095f, -0.02f), vec3(0.028f, -0.012f, 0.05f), Polymer);   // grip
        break;
    }

    // --- Fusil Escarabajo (~0.85 m) --------------------------------------------
    case WeaponKind::Rifle: {
        p.box(vec3(-0.030f, -0.008f, -0.23f), vec3(0.030f, 0.055f, -0.04f), Polymer);   // stock
        p.box(vec3(-0.030f, 0.048f, -0.21f), vec3(0.030f, 0.082f, -0.08f), Polymer);    // cheek riser
        p.box(vec3(-0.045f, -0.012f, -0.04f), vec3(0.045f, 0.082f, 0.26f), Gunmetal);   // receiver
        p.box(vec3(-0.040f, 0.000f, 0.26f), vec3(0.040f, 0.066f, 0.46f), SlateL);       // handguard
        p.cyl(vec3(0, 0.046f, 0.46f), vec3(0, 0.046f, 0.60f), 0.014f, SteelD);          // barrel
        p.box(vec3(-0.026f, 0.028f, 0.58f), vec3(0.026f, 0.064f, 0.64f), Steel);        // muzzle
        p.box(vec3(-0.026f, -0.14f, 0.04f), vec3(0.026f, -0.012f, 0.125f), Gunmetal);   // mag upper
        p.box(vec3(-0.026f, -0.235f, 0.10f), vec3(0.026f, -0.14f, 0.185f), Gunmetal);   // mag lower (curve)
        p.box(vec3(-0.014f, 0.082f, 0.02f), vec3(0.014f, 0.108f, 0.16f), Steel);        // carry sight
        p.box(vec3(-0.028f, -0.095f, -0.045f), vec3(0.028f, -0.012f, 0.025f), Polymer); // grip
        break;
    }

    // --- Francotirador Bisonte (~1.35 m) ----------------------------------------
    case WeaponKind::Sniper: {
        p.box(vec3(-0.032f, -0.022f, -0.30f), vec3(0.032f, 0.062f, -0.02f), vec3(0.20f, 0.27f, 0.18f)); // stock
        p.box(vec3(-0.032f, 0.052f, -0.28f), vec3(0.032f, 0.088f, -0.10f), vec3(0.20f, 0.27f, 0.18f));  // cheek
        p.box(vec3(-0.046f, -0.012f, -0.02f), vec3(0.046f, 0.088f, 0.28f), Gunmetal);   // receiver
        p.cyl(vec3(0, 0.050f, 0.28f), vec3(0, 0.050f, 0.98f), 0.019f, SteelD);          // long barrel
        p.box(vec3(-0.030f, 0.028f, 0.98f), vec3(0.030f, 0.072f, 1.06f), Steel);        // muzzle brake
        p.cyl(vec3(0, 0.142f, 0.02f), vec3(0, 0.142f, 0.27f), 0.032f, Gunmetal);        // scope tube
        p.box(vec3(-0.030f, 0.118f, 0.255f), vec3(0.030f, 0.166f, 0.285f), Visor);      // front lens
        p.box(vec3(-0.026f, 0.088f, 0.06f), vec3(0.026f, 0.116f, 0.10f), Gunmetal);     // mount A
        p.box(vec3(-0.026f, 0.088f, 0.19f), vec3(0.026f, 0.116f, 0.23f), Gunmetal);     // mount B
        p.box(vec3(-0.026f, -0.115f, 0.06f), vec3(0.026f, -0.012f, 0.155f), Gunmetal);  // mag
        p.box(vec3(-0.028f, -0.10f, -0.055f), vec3(0.028f, -0.012f, 0.015f), Polymer);  // grip
        p.box(vec3(-0.030f, -0.045f, 0.56f), vec3(-0.012f, -0.018f, 0.72f), SteelD);    // bipod L
        p.box(vec3(0.012f, -0.045f, 0.56f), vec3(0.030f, -0.018f, 0.72f), SteelD);      // bipod R
        break;
    }

    default: break;
    }

    return p.finish(*m_r);
}

} // namespace game
