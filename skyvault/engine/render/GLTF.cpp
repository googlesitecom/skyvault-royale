// ============================================================================
//  SKYVAULT Royale - engine/render/GLTF.cpp
// ============================================================================
#include "render/GLTF.h"
#include "core/Json.h"
#include "math/Math.h"

#include <cstring>
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

namespace sv {

// ---------------------------------------------------------------------------
// Utilidades de acceso al JSON glTF
// ---------------------------------------------------------------------------
namespace {

struct Glb {
    std::vector<u8> bin;
    const Json* j = nullptr;
    std::vector<u8> jsonOwned;
    Json parsed;
};

bool loadGlb(const char* path, Glb& out) {
    std::vector<u8> data;
    if (!fs::readFile(path, data)) {
        SV_LOG_ERROR("gltf", "No se pudo abrir %s", path);
        return false;
    }
    if (data.size() < 20 || std::memcmp(data.data(), "glTF", 4) != 0) {
        SV_LOG_ERROR("gltf", "%s no es un GLB valido", path);
        return false;
    }
    u32 version = 0, length = 0;
    std::memcpy(&version, data.data() + 4, 4);
    std::memcpy(&length, data.data() + 8, 4);
    if (version != 2) { SV_LOG_ERROR("gltf", "GLB version %u no soportada", version); return false; }

    usize off = 12;
    bool gotJson = false;
    while (off + 8 <= data.size()) {
        u32 clen = 0, ctype = 0;
        std::memcpy(&clen, data.data() + off, 4);
        std::memcpy(&ctype, data.data() + off + 4, 4);
        if (off + 8 + clen > data.size()) break;
        if (ctype == 0x4E4F534A) {          // "JSON"
            out.jsonOwned.assign(data.begin() + off + 8, data.begin() + off + 8 + clen);
            // recorta espacios nulos
            while (!out.jsonOwned.empty() && out.jsonOwned.back() == 0) out.jsonOwned.pop_back();
            out.jsonOwned.push_back('\0');
            gotJson = true;
        } else if (ctype == 0x004E4942) {   // "BIN"
            out.bin.assign(data.begin() + off + 8, data.begin() + off + 8 + clen);
        }
        off += 8 + clen;
        if ((clen & 3) && off < data.size()) off += 4 - (clen & 3);  // alineacion
    }
    if (!gotJson) { SV_LOG_ERROR("gltf", "GLB sin chunk JSON"); return false; }
    std::string err;
    if (!out.parsed.parse(reinterpret_cast<const char*>(out.jsonOwned.data()), &err)) {
        SV_LOG_ERROR("gltf", "JSON glTF invalido: %s", err.c_str());
        return false;
    }
    out.j = &out.parsed;
    return true;
}

// datos de un accessor (solo soporta el subconjunto que usan los modelos)
struct AccessorData {
    const u8* ptr = nullptr;
    u32 count = 0;
    u32 compCount = 0;      // componentes
    u32 compType = 0;       // 5120 i8, 5121 u8, 5122 i16, 5123 u16, 5125 u32, 5126 f32
    u64 stride = 0;
};

bool getAccessor(const Glb& g, const Json& accessors, u32 index, AccessorData& out) {
    const Json* acc = accessors.at(index);
    if (!acc) return false;
    const Json* bv = g.j->get("bufferViews");
    if (!bv) return false;
    u32 bufferView = 0;
    if (const Json* b = acc->get("bufferView")) bufferView = (u32)b->number;
    const Json* view = bv->at(bufferView);
    if (!view) return false;
    u32 byteOffset = 0;
    if (const Json* b = view->get("byteOffset")) byteOffset = (u32)b->number;
    u32 accOffset = 0;
    if (const Json* b = acc->get("byteOffset")) accOffset = (u32)b->number;
    u32 byteLength = 0;
    if (const Json* b = view->get("byteLength")) byteLength = (u32)b->number;

    out.count = (u32)acc->getNum("count", 0);
    const std::string type = acc->getStr("type", "SCALAR");
    out.compCount = type == "SCALAR" ? 1 : (type == "VEC2" ? 2 : (type == "VEC3" ? 3 : 4));
    out.compType = (u32)acc->getNum("componentType", 5126);
    const u32 compSize = out.compType == 5126 ? 4 : (out.compType == 5125 ? 4 :
                          (out.compType == 5123 || out.compType == 5122 ? 2 : 1));
    const u32 elSize = out.compCount * compSize;
    u32 stride = elSize;
    if (const Json* st = view->get("byteStride")) stride = (u32)st->number;
    out.stride = stride;
    const u64 base = (u64)byteOffset + accOffset;
    if (base + (u64)out.count * elSize > g.bin.size()) {
        SV_LOG_WARN("gltf", "accessor fuera de rango");
        return false;
    }
    out.ptr = g.bin.data() + base;
    (void)byteLength;
    return true;
}

template <typename T>
T readComp(const u8* p, u32 compType) {
    switch (compType) {
        case 5126: { f32 f; std::memcpy(&f, p, 4); return (T)f; }
        case 5125: { u32 v; std::memcpy(&v, p, 4); return (T)v; }
        case 5123: { u16 v; std::memcpy(&v, p, 2); return (T)v; }
        case 5122: { i16 v; std::memcpy(&v, p, 2); return (T)v; }
        case 5121: return (T)p[0];
        default:   return (T)(i32)p[0];
    }
}

mat4 nodeMatrix(const Json* node) {
    mat4 m(1);
    if (const Json* t = node->get("translation")) {
        if (t->size() >= 3)
            m = glm::translate(m, vec3((f32)t->at(0)->number, (f32)t->at(1)->number,
                                       (f32)t->at(2)->number));
    }
    if (const Json* r = node->get("rotation")) {
        if (r->size() >= 4) {
            const quat q((f32)r->at(3)->number, (f32)r->at(0)->number,
                         (f32)r->at(1)->number, (f32)r->at(2)->number);
            m *= glm::mat4_cast(q);
        }
    }
    if (const Json* s = node->get("scale")) {
        if (s->size() >= 3)
            m = glm::scale(m, vec3((f32)s->at(0)->number, (f32)s->at(1)->number,
                                   (f32)s->at(2)->number));
    }
    if (const Json* mm = node->get("matrix")) {
        if (mm->size() >= 16) {
            mat4 nm;
            f32* cols = glm::value_ptr(nm);
            for (u32 i = 0; i < 16; ++i) cols[i] = (f32)mm->at(i)->number;
            m = nm;
        }
    }
    return m;
}

void collectMeshNodes(const Json& nodes, i32 nodeIndex, const mat4& parent,
                      std::vector<std::pair<i32, mat4>>& out) {
    const Json* node = nodes.at((u32)nodeIndex);
    if (!node) return;
    const mat4 local = nodeMatrix(node);
    const mat4 world = parent * local;
    if (node->get("mesh")) out.emplace_back(nodeIndex, world);
    if (const Json* kids = node->get("children"))
        for (usize i = 0; i < kids->size(); ++i)
            collectMeshNodes(nodes, (i32)kids->at(i)->number, world, out);
}

} // namespace

// ---------------------------------------------------------------------------
// GltfModel
// ---------------------------------------------------------------------------
bool GltfModel::load(const char* glbPath, bool useNodeTransforms, f32 normalizeLongest) {
    Glb g;
    if (!loadGlb(glbPath, g)) return false;
    const Json& J = *g.j;

    const Json* accessors = J.get("accessors");
    const Json* meshesJson = J.get("meshes");
    const Json* nodes = J.get("nodes");
    const Json* materials = J.get("materials");
    if (!accessors || !meshesJson || !nodes) {
        SV_LOG_ERROR("gltf", "GLB incompleto");
        return false;
    }

    // texturas (cache por imagen)
    const Json* images = J.get("images");
    std::vector<i32> imageToTex;
    auto imageTexture = [&](i32 imageIndex) -> i32 {
        if (!images || imageIndex < 0 || (usize)imageIndex >= imageToTex.size()) return -1;
        if (imageToTex[(usize)imageIndex] >= 0) return imageToTex[(usize)imageIndex];
        const Json* img = images->at((u32)imageIndex);
        if (!img) return -1;
        const Json* bvIdx = img->get("bufferView");
        if (!bvIdx) return -1;   // solo embebidas
        const Json* bvs = J.get("bufferViews");
        const Json* view = bvs ? bvs->at((u32)bvIdx->number) : nullptr;
        if (!view) return -1;
        u32 off = 0, len = 0;
        if (const Json* b = view->get("byteOffset")) off = (u32)b->number;
        if (const Json* b = view->get("byteLength")) len = (u32)b->number;
        if ((u64)off + len > g.bin.size()) return -1;
        i32 w = 0, h = 0, ch = 0;
        const u8* px = stbi_load_from_memory(g.bin.data() + off, (i32)len, &w, &h, &ch, 4);
        if (!px) { SV_LOG_WARN("gltf", "stb no pudo decodificar imagen %d", imageIndex); return -1; }
        Texture t;
        // SRGB8: las texturas de color de glTF vienen en sRGB; la GPU las
        // linealiza al muestrear (sin esto todo el modelo sale oscuro)
        t.create2D(w, h, TexFormat::SRGB8, px, true, true);
        stbi_image_free((void*)px);
        textures.push_back(std::move(t));
        imageToTex[(usize)imageIndex] = (i32)(textures.size() - 1);
        return imageToTex[(usize)imageIndex];
    };
    if (images) {
        imageToTex.assign(images->size(), -1);
        // CRITICO: los materiales guardan Texture* dentro de este vector.
        // Reservar la capacidad total evita realocs que invalidarian esos
        // punteros (sintoma: los primeros prim del modelo salen negros).
        textures.reserve(images->size());
    }

    // materiales: mapear a Material (texturas por indice)
    auto buildMaterial = [&](i32 matIndex) -> Material {
        Material mat;
        mat.metallic = 0.1f; mat.rough = 0.7f;
        if (!materials || matIndex < 0 || (usize)matIndex >= materials->size()) return mat;
        const Json* m = materials->at((u32)matIndex);
        const Json* pbr = m->get("pbrMetallicRoughness");
        if (pbr) {
            // sin mapa MR, un metallicFactor alto deja la pieza negra sin IBL:
            // se acota (estilizado, no fotorrealista)
            mat.metallic = clamp((f32)pbr->getNum("metallicFactor", 1.0), 0.0f, 0.35f);
            mat.rough    = clamp((f32)pbr->getNum("roughnessFactor", 1.0), 0.35f, 0.95f);
            if (const Json* base = pbr->get("baseColorTexture")) {
                const Json* texs = J.get("textures");
                const Json* tx = texs ? texs->at((u32)base->getNum("index", 0)) : nullptr;
                const i32 imgIdx = tx ? (i32)tx->getNum("source", -1) : -1;
                const i32 ti = imageTexture(imgIdx);
                if (ti >= 0) mat.baseTex = &textures[(usize)ti];
            }
            if (const Json* mr = pbr->get("metallicRoughnessTexture")) {
                const Json* texs = J.get("textures");
                const Json* tx = texs ? texs->at((u32)mr->getNum("index", 0)) : nullptr;
                const i32 imgIdx = tx ? (i32)tx->getNum("source", -1) : -1;
                const i32 ti = imageTexture(imgIdx);
                if (ti >= 0) mat.mrTex = &textures[(usize)ti];
            }
        }
        if (const Json* nt = m->get("normalTexture")) {
            const Json* texs = J.get("textures");
            const Json* tx = texs ? texs->at((u32)nt->getNum("index", 0)) : nullptr;
            const i32 imgIdx = tx ? (i32)tx->getNum("source", -1) : -1;
            const i32 ti = imageTexture(imgIdx);
            if (ti >= 0) mat.normalTex = &textures[(usize)ti];
        }
        if (const Json* ef = m->get("emissiveFactor")) {
            if (ef->size() >= 3 && ef->at(0)->number > 0.01) mat.emissive = 0.35f;
        }
        return mat;
    };

    // escena -> nodos con malla (transform horneada). Muchos exports de
    // Sketchfab traen transforms de rig basura (escalas x100): con
    // useNodeTransforms=false se usa el espacio local crudo de los vertices.
    std::vector<std::pair<i32, mat4>> meshNodes;
    if (useNodeTransforms) {
        if (const Json* scene = J.get("scene")) {
            const Json* scenes = J.get("scenes");
            const Json* sc = scenes ? scenes->at((u32)scene->number) : nullptr;
            if (sc)
                if (const Json* roots = sc->get("nodes"))
                    for (usize i = 0; i < roots->size(); ++i)
                        collectMeshNodes(*nodes, (i32)roots->at(i)->number, mat4(1), meshNodes);
        }
    }
    if (meshNodes.empty()) {
        // fallback: todos los nodos con malla sin transform
        for (usize i = 0; i < nodes->size(); ++i)
            if (nodes->at(i)->get("mesh")) meshNodes.emplace_back((i32)i, mat4(1));
    }

    // normalizacion: escala uniforme para que el eje mas largo mida
    // normalizeLongest (metros), calculada de los bounds RAW de accessors
    f32 normScale = 1.0f;
    if (normalizeLongest > 0.0f) {
        vec3 mn(1e9f), mx(-1e9f);
        for (const auto& [nodeIdx, world] : meshNodes) {
            const Json* node = nodes->at((u32)nodeIdx);
            const u32 meshIdx = (u32)node->getNum("mesh", -1);
            const Json* mesh = meshesJson->at(meshIdx);
            if (!mesh) continue;
            const Json* primsJ = mesh->get("primitives");
            if (!primsJ) continue;
            for (usize pi = 0; pi < primsJ->size(); ++pi) {
                const Json* attrs = primsJ->at(pi)->get("attributes");
                if (!attrs) continue;
                const Json* posA = attrs->get("POSITION");
                if (!posA) continue;
                const Json* acc = accessors->at((u32)posA->number);
                const Json* amin = acc->get("min");
                const Json* amax = acc->get("max");
                if (amin && amax && amin->size() >= 3 && amax->size() >= 3) {
                    mn = glm::min(mn, vec3((f32)amin->at(0)->number, (f32)amin->at(1)->number,
                                           (f32)amin->at(2)->number));
                    mx = glm::max(mx, vec3((f32)amax->at(0)->number, (f32)amax->at(1)->number,
                                           (f32)amax->at(2)->number));
                }
            }
        }
        const vec3 span = mx - mn;
        const f32 longest = std::max(span.x, std::max(span.y, span.z));
        if (longest > 1e-5f) normScale = normalizeLongest / longest;
    }

    bounds = AABB{vec3(1e9f), vec3(-1e9f)};
    Renderer& r = Renderer::get();

    for (const auto& [nodeIdx, world] : meshNodes) {
        const Json* node = nodes->at((u32)nodeIdx);
        const u32 meshIdx = (u32)node->getNum("mesh", -1);
        const Json* mesh = meshesJson->at(meshIdx);
        if (!mesh) continue;
        const Json* primsJson = mesh->get("primitives");
        if (!primsJson) continue;
        for (usize pi = 0; pi < primsJson->size(); ++pi) {
            const Json* prim = primsJson->at(pi);
            const Json* attrs = prim->get("attributes");
            if (!attrs) continue;
            auto attrIdx = [&](const char* name) -> i32 {
                const Json* a = attrs->get(name);
                return a ? (i32)a->number : -1;
            };
            AccessorData posAcc;
            if (attrIdx("POSITION") < 0 || !getAccessor(g, *accessors, (u32)attrIdx("POSITION"), posAcc))
                continue;

            const i32 normIdx = attrIdx("NORMAL");
            const i32 uvIdx   = attrIdx("TEXCOORD_0");
            const i32 tanIdx  = attrIdx("TANGENT");
            const i32 jointIdx = attrIdx("JOINTS_0");
            const i32 weightIdx = attrIdx("WEIGHTS_0");

            AccessorData nAcc, uAcc, tAcc, jAcc, wAcc;
            if (normIdx >= 0)  getAccessor(g, *accessors, (u32)normIdx, nAcc);
            if (uvIdx >= 0)    getAccessor(g, *accessors, (u32)uvIdx, uAcc);
            if (tanIdx >= 0)   getAccessor(g, *accessors, (u32)tanIdx, tAcc);
            if (jointIdx >= 0) getAccessor(g, *accessors, (u32)jointIdx, jAcc);
            if (weightIdx >= 0) getAccessor(g, *accessors, (u32)weightIdx, wAcc);

            std::vector<Vertex> verts(posAcc.count);
            const mat3 nm = mat3(world);
            for (u32 v = 0; v < posAcc.count; ++v) {
                Vertex& out = verts[v];
                const u8* pp = posAcc.ptr + v * posAcc.stride;
                const vec3 lp(readComp<f32>(pp, posAcc.compType) * normScale,
                              readComp<f32>(pp + 4, posAcc.compType) * normScale,
                              readComp<f32>(pp + 8, posAcc.compType) * normScale);
                const vec3 wp = world * vec4(lp, 1);
                out.pos = wp;
                if (normIdx >= 0 && v < nAcc.count) {
                    const u8* np = nAcc.ptr + v * nAcc.stride;
                    out.normal = normalize(nm * vec3(readComp<f32>(np, nAcc.compType),
                                                     readComp<f32>(np + 4, nAcc.compType),
                                                     readComp<f32>(np + 8, nAcc.compType)));
                } else out.normal = vec3(0, 1, 0);
                if (uvIdx >= 0 && v < uAcc.count) {
                    const u8* up = uAcc.ptr + v * uAcc.stride;
                    out.uv = vec2(readComp<f32>(up, uAcc.compType),
                                  readComp<f32>(up + 4, uAcc.compType));
                } else out.uv = vec2(0);
                if (tanIdx >= 0 && v < tAcc.count) {
                    const u8* tp = tAcc.ptr + v * tAcc.stride;
                    out.tangent = vec4(nm * vec3(readComp<f32>(tp, tAcc.compType),
                                                 readComp<f32>(tp + 4, tAcc.compType),
                                                 readComp<f32>(tp + 8, tAcc.compType)),
                                       readComp<f32>(tp + 12, tAcc.compType));
                } else out.tangent = vec4(1, 0, 0, 1);
                for (u32 k = 0; k < 4; ++k) {
                    out.joint[k]  = (jointIdx >= 0 && v < jAcc.count)
                        ? (u8)readComp<u32>(jAcc.ptr + v * jAcc.stride + k * (jAcc.compType == 5125 ? 4 : 1), jAcc.compType)
                        : 0;
                    out.weight[k] = (weightIdx >= 0 && v < wAcc.count)
                        ? (u8)(readComp<f32>(wAcc.ptr + v * wAcc.stride + k * 4, wAcc.compType) * 255.0f)
                        : (k == 0 ? 255 : 0);
                }
                out.color = vec4(1);
                bounds.bmin = glm::min(bounds.bmin, wp);
                bounds.bmax = glm::max(bounds.bmax, wp);
            }

            // indices
            std::vector<u32> indices;
            if (const Json* ix = prim->get("indices")) {
                AccessorData iAcc;
                if (getAccessor(g, *accessors, (u32)ix->number, iAcc)) {
                    indices.resize(iAcc.count);
                    const u32 cs = iAcc.compType == 5125 ? 4 : (iAcc.compType == 5123 || iAcc.compType == 5122 ? 2 : 1);
                    for (u32 i = 0; i < iAcc.count; ++i)
                        indices[i] = readComp<u32>(iAcc.ptr + i * iAcc.stride, iAcc.compType)
                                   + 0 * cs;
                }
            } else {
                indices.resize(posAcc.count);
                for (u32 i = 0; i < posAcc.count; ++i) indices[i] = i;
            }
            if (indices.empty()) continue;

            Prim p;
            p.mesh = r.createMesh(verts.data(), (u32)verts.size(), indices.data(), (u32)indices.size());
            p.mat = buildMaterial((i32)prim->getNum("material", -1));
            prims.push_back(std::move(p));
        }
    }

    SV_LOG_INFO("gltf", "%s: %zu prims, %zu texturas, bounds [%.1f..%.1f]",
                glbPath, prims.size(), textures.size(), bounds.bmin.y, bounds.bmax.y);
    return !prims.empty();
}

void GltfModel::destroy() {
    for (auto& p : prims) p.mesh.destroy();
    prims.clear();
    for (auto& t : textures) t.destroy();
    textures.clear();
}

} // namespace sv
