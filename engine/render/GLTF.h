// ============================================================================
//  SKYVAULT Royale - engine/render/GLTF.h
//  Cargador de modelos GLB (glTF 2.0 binario) propio:
//    - contenedor GLB (header + chunk JSON + chunk BIN)
//    - JSON via sv::Json (parser propio del motor)
//    - texturas embebidas via stb_image
//    - bakes de la jerarquia de nodos en los vertices (estatico)
// ============================================================================
#pragma once

#include "core/Core.h"
#include "render/RenderTypes.h"
#include "render/Renderer.h"

namespace sv {

struct GltfModel {
    struct Prim {
        Mesh mesh;
        Material mat;
    };
    std::vector<Prim> prims;
    std::vector<Texture> textures;   // propietario de las texturas
    AABB bounds;

    [[nodiscard]] bool load(const char* glbPath, bool useNodeTransforms = true,
                           f32 normalizeLongest = 0.0f);
    void destroy();
    [[nodiscard]] bool valid() const { return !prims.empty(); }
};

} // namespace sv
