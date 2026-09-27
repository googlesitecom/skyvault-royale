// ============================================================================
//  SKYVAULT Royale - engine/render/GLTF.h
//  Cargador de modelos GLB (glTF 2.0 binario) propio:
//    - contenedor GLB (header + chunk JSON + chunk BIN)
//    - JSON via sv::Json (parser propio del motor)
//    - texturas embebidas via stb_image
//    - SKINNING: esqueleto + inverse bind matrices + animaciones embebidas
//      (muestreo de clips, pose procedural por encima y matrices de piel
//      listas para subir a la GPU)
// ============================================================================
#pragma once

#include "core/Core.h"
#include "render/RenderTypes.h"
#include "render/Renderer.h"
#include <unordered_map>

namespace sv {

// ---------------------------------------------------------------------------
// Esqueleto: articulaciones en orden glTF (skin.joints)
// ---------------------------------------------------------------------------
struct GltfJoint {
    i32 parent = -1;        // indice dentro del array de joints (-1 raiz)
    i32 node = -1;          // indice de nodo glTF
    std::string name;
    vec3 restPos{0};
    quat restRot{1, 0, 0, 0};
    vec3 restScale{1};
    mat4 inverseBind{1};
    mat4 bindFix{1};        // corrige exports inconsistentes: bindFix*(P*G*IBM)=I
};

// ---------------------------------------------------------------------------
// Un clip de animacion (canales con keyframes)
// ---------------------------------------------------------------------------
struct GltfAnimChannel {
    i32 node = -1;
    u8 path = 0;            // 0 translation, 1 rotation, 2 scale
    std::vector<f32> times;
    std::vector<vec3> vecKeys;     // translation / scale
    std::vector<quat> quatKeys;    // rotation
};

struct GltfAnim {
    std::string name;
    f32 duration = 0;
    std::vector<GltfAnimChannel> channels;
};

// ---------------------------------------------------------------------------
// Pose instantanea de un esqueleto (para un personaje por frame)
// ---------------------------------------------------------------------------
struct GltfPose {
    std::vector<vec3> pos;
    std::vector<quat> rot;
    std::vector<vec3> scale;

    void resetTo(const std::vector<GltfJoint>& joints) {
        const usize n = joints.size();
        pos.resize(n); rot.resize(n); scale.resize(n);
        for (usize i = 0; i < n; ++i) {
            pos[i] = joints[i].restPos;
            rot[i] = joints[i].restRot;
            scale[i] = joints[i].restScale;
        }
    }
};

struct GltfModel {
    struct Prim {
        Mesh mesh;
        Material mat;
    };
    std::vector<Prim> prims;
    std::vector<Texture> textures;   // propietario de las texturas
    AABB bounds;

    // --- esqueleto (vacío si el modelo no tiene skin) ----------------------
    std::vector<GltfJoint> joints;
    std::unordered_map<std::string, i32> jointByName;
    std::vector<GltfAnim> anims;
    // prefijo de nodos ancestros de la raiz del esqueleto (transform fijo)
    std::vector<mat4> skeletonPrefix;
    // normalizacion aplicada a las matrices de piel (mide 1.85 m etc.)
    f32 skinNormScale = 1.0f;
    f32 loadNormScale = 1.0f;    // escala uniforme aplicada a los vertices
    vec3 skinCenter{0};
    bool skinned = false;

    [[nodiscard]] bool load(const char* glbPath, bool useNodeTransforms = true,
                           f32 normalizeLongest = 0.0f);
    void destroy();
    [[nodiscard]] bool valid() const { return !prims.empty(); }

    // --- animacion ----------------------------------------------------------
    // escribe la pose local del clip en el tiempo t (loop) sobre `pose`
    void sampleAnim(u32 animIndex, f32 t, GltfPose& pose) const;
    // calcula las matrices de piel finales (ya normalizadas al espacio del
    // modelo: pies en y=0, altura normalizada). outSkin debe tener
    // joints.size() elementos.
    void computeSkinMatrices(const GltfPose& pose, std::vector<mat4>& outSkin) const;
    void computeSkinMatricesRaw(const GltfPose& pose, std::vector<mat4>& outSkin) const;
    // matriz de mundo de una articulacion en la pose dada (sin la parte de
    // normalizacion; util para anclar armas a la mano)
    [[nodiscard]] mat4 jointMatrix(const GltfPose& pose, i32 joint) const;
    [[nodiscard]] i32 findJoint(const char* name) const {
        const auto it = jointByName.find(name);
        return it == jointByName.end() ? -1 : it->second;
    }
};

} // namespace sv
