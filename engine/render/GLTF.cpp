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
    out.compCount = type == "SCALAR" ? 1 : (type == "VEC2" ? 2 : (type == "VEC3" ? 3 :
                     (type == "MAT4" ? 16 : 4)));
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

// TRS local de un nodo (para el esqueleto en reposo)
void nodeTRS(const Json* node, vec3& t, quat& r, vec3& s) {
    t = vec3(0); r = quat(1, 0, 0, 0); s = vec3(1);
    if (const Json* tt = node->get("translation"))
        if (tt->size() >= 3)
            t = vec3((f32)tt->at(0)->number, (f32)tt->at(1)->number, (f32)tt->at(2)->number);
    if (const Json* rr = node->get("rotation"))
        if (rr->size() >= 4)
            r = quat((f32)rr->at(3)->number, (f32)rr->at(0)->number,
                     (f32)rr->at(1)->number, (f32)rr->at(2)->number);
    if (const Json* ss = node->get("scale"))
        if (ss->size() >= 3)
            s = vec3((f32)ss->at(0)->number, (f32)ss->at(1)->number, (f32)ss->at(2)->number);
    if (const Json* mm = node->get("matrix")) {
        if (mm->size() >= 16) {
            // descompone la matriz (solo casos simples: T*R*S uniforme)
            mat4 m;
            f32* cols = glm::value_ptr(m);
            for (u32 i = 0; i < 16; ++i) cols[i] = (f32)mm->at(i)->number;
            t = vec3(m[3]);
            const vec3 cx(m[0]), cy(m[1]), cz(m[2]);
            const f32 sx = length(cx), sy = length(cy), sz = length(cz);
            s = vec3(sx, sy, sz);
            mat3 rot(cx / std::max(sx, 1e-8f), cy / std::max(sy, 1e-8f), cz / std::max(sz, 1e-8f));
            r = glm::quat_cast(rot);
        }
    }
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
    // normalizeLongest (metros). Con transforms horneadas (world) se mide el
    // AABB TRANSFORMADO (los crudos viven en otro espacio); sin transforms,
    // los bounds RAW de accessors.
    f32 normScale = 1.0f;
    if (normalizeLongest > 0.0f) {
        vec3 mn(1e9f), mx(-1e9f);
        const bool baked = useNodeTransforms;
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
                    vec3 lmn((f32)amin->at(0)->number, (f32)amin->at(1)->number,
                             (f32)amin->at(2)->number);
                    vec3 lmx((f32)amax->at(0)->number, (f32)amax->at(1)->number,
                             (f32)amax->at(2)->number);
                    if (baked) {
                        // AABB transformado: 8 esquinas por la matriz world
                        for (u32 c = 0; c < 8; ++c) {
                            const vec3 p((c & 1) ? lmx.x : lmn.x,
                                         (c & 2) ? lmx.y : lmn.y,
                                         (c & 4) ? lmx.z : lmn.z);
                            const vec3 wp = vec3(world * vec4(p, 1.0f));
                            mn = glm::min(mn, wp);
                            mx = glm::max(mx, wp);
                        }
                    } else {
                        mn = glm::min(mn, lmn);
                        mx = glm::max(mx, lmx);
                    }
                }
            }
        }
        const vec3 span = mx - mn;
        const f32 longest = std::max(span.x, std::max(span.y, span.z));
        if (longest > 1e-5f) normScale = normalizeLongest / longest;
    }
    loadNormScale = normScale;   // la conjuga la piel (vertices ya escalados)

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

    // -----------------------------------------------------------------------
    // Esqueleto + animaciones (si el modelo tiene skin)
    // -----------------------------------------------------------------------
    if (const Json* skins = J.get("skins")) {
        if (skins->size() > 0) {
            const Json* skin = skins->at(0);
            const Json* jointNodes = skin->get("joints");
            if (jointNodes && jointNodes->size() > 0) {
                skinned = true;
                const usize jc = jointNodes->size();
                joints.resize(jc);
                // mapa nodo -> indice de joint
                std::unordered_map<i32, i32> nodeToJoint;
                for (usize i = 0; i < jc; ++i) {
                    const i32 ni = (i32)jointNodes->at(i)->number;
                    nodeToJoint[ni] = (i32)i;
                    joints[i].node = ni;
                    const Json* jn = nodes->at((u32)ni);
                    if (jn) {
                        if (const Json* nm = jn->get("name"))
                            joints[i].name = nm->str;
                        nodeTRS(jn, joints[i].restPos, joints[i].restRot, joints[i].restScale);
                    }
                    joints[i].parent = -1;
                    if (jn)
                        if (const Json* kids = jn->get("children")) { /* noop */ }
                    jointByName[joints[i].name] = (i32)i;
                }
                // padres dentro del conjunto
                for (usize i = 0; i < jc; ++i) {
                    const Json* jn = nodes->at((u32)joints[i].node);
                    if (!jn) continue;
                    // busca el padre via recorrido inverso: los glTF guardan
                    // children, asi que escanea todos los nodos una vez
                    (void)jn;
                }
                // construir mapa de padres global (nodo -> nodo padre)
                std::unordered_map<i32, i32> nodeParent;
                for (usize n = 0; n < nodes->size(); ++n)
                    if (const Json* kids = nodes->at(n)->get("children"))
                        for (usize k = 0; k < kids->size(); ++k)
                            nodeParent[(i32)kids->at(k)->number] = (i32)n;
                for (usize i = 0; i < jc; ++i) {
                    i32 p = nodeParent.count(joints[i].node)
                          ? nodeParent[joints[i].node] : -1;
                    while (p != -1 && !nodeToJoint.count(p))
                        p = nodeParent.count(p) ? nodeParent[p] : -1;
                    joints[i].parent = (p != -1) ? nodeToJoint[p] : -1;
                }
                // inverse bind matrices (MAT4 column-major, leidas con stride
                // completo de 64 bytes)
                bool ibmOk = false;
                if (const Json* ibm = skin->get("inverseBindMatrices")) {
                    AccessorData acc;
                    if (getAccessor(g, *accessors, (u32)ibm->number, acc) &&
                        acc.compType == 5126) {
                        const u32 step = (u32)acc.stride;
                        for (usize i = 0; i < jc && i < acc.count; ++i) {
                            const f32* p = reinterpret_cast<const f32*>(acc.ptr + i * step);
                            f32* cols = glm::value_ptr(joints[i].inverseBind);
                            for (u32 k = 0; k < 16; ++k) cols[k] = p[k];
                        }
                        ibmOk = true;
                    }
                }
                if (!ibmOk)
                    for (auto& j : joints) j.inverseBind = mat4(1);

                // PREFIJO DEL ESQUELETO derivado de los IBM: los exports de
                // Sketchfab/UE meten la conversion de ejes y escala en nodos
                // ancestros, pero los IBM estan relativos a un espacio propio.
                // P se resuelve para que P * global_bind(j) = inv(IBM_j):
                //   bind skin = P * G * IBM = I  (el bind pose reproduce el T-pose)
                // Se usa el primer joint hijo de la raiz (cuerpo principal).
                {
                    skeletonPrefix.clear();
                    mat4 P(1);
                    bool solved = false;
                    // globals bind con la raiz del skin como raiz
                    static std::vector<mat4> gb;
                    gb.assign(jc, mat4(1));
                    for (usize i = 0; i < jc; ++i) {
                        const mat4 local = composeTRS(joints[i].restPos, joints[i].restRot,
                                                      joints[i].restScale);
                        gb[i] = joints[i].parent >= 0 ? gb[(usize)joints[i].parent] * local
                                                      : local;
                    }
                    for (usize i = 0; i < jc && !solved; ++i) {
                        if (joints[i].parent != 0) continue;      // hijo directo de la raiz
                        const f32 det = glm::determinant(gb[i]);
                        if (std::fabs(det) < 1e-6f) continue;
                        // P = inv(IBM_i) * inv(gb_i)
                        P = glm::inverse(joints[i].inverseBind) * glm::inverse(gb[i]);
                        // verificacion por MAYORIA: los huesos terminales
                        // ("_end", "_0153"...) y dinamicos del export suelen
                        // discrepar sin afectar al cuerpo; cuenta el principal
                        u32 pass = 0, total = 0;
                        for (usize k = 1; k < jc; ++k) {
                            if (joints[k].parent < 1) continue;
                            if (std::fabs(glm::determinant(gb[k])) < 1e-6f) continue;
                            // descarta huesos terminales/dinamicos del recuento
                            const std::string& jn = joints[k].name;
                            if (jn.find("_end") != std::string::npos) continue;
                            if (jn.rfind("dyn_", 0) == 0) continue;
                            const mat4 s = P * gb[k] * joints[k].inverseBind;
                            const f32 err = std::fabs(s[0][0] - 1.0f) + std::fabs(s[1][1] - 1.0f) +
                                            std::fabs(s[2][2] - 1.0f) + std::fabs(s[3][3] - 1.0f) +
                                            std::fabs(s[3][0]) + std::fabs(s[3][1]) + std::fabs(s[3][2]);
                            ++total;
                            if (err < 0.08f) ++pass;
                        }
                        if (total > 0 && (f32)pass / (f32)total > 0.7f) solved = true;
                    }
                    if (!solved) P = mat4(1);   // sin solucion: sin prefijo
                    skeletonPrefix.push_back(P);
                    // correccion por articulacion: hace que el bind pose sea
                    // EXACTO incluso en huesos con export inconsistente
                    // (_end, dyn_*, ojos, dedos...). bindFix = inv(P*G*IBM).
                    for (usize i = 0; i < jc; ++i) {
                        const mat4 s = P * gb[i] * joints[i].inverseBind;
                        const f32 det = glm::determinant(s);
                        joints[i].bindFix = std::fabs(det) > 1e-6f ? glm::inverse(s) : mat4(1);
                    }
                }
                // normalizacion de la piel: el bind pose deja los vertices tal
                // como se cargaron (normalizados a normalizeLongest), solo se
                // recoloca el origen en los pies y el centro horizontal.
                {
                    skinNormScale = 1.0f;
                    skinCenter = vec3((bounds.bmin.x + bounds.bmax.x) * 0.5f,
                                      bounds.bmin.y,
                                      (bounds.bmin.z + bounds.bmax.z) * 0.5f);
                }
                SV_LOG_INFO("gltf", "Skin: %zu joints, %zu clips", joints.size(), J.get("animations") ? J.get("animations")->size() : (usize)0);
            }
        }
    }

    // animaciones embebidas
    if (const Json* animsJ = J.get("animations")) {
        for (usize ai = 0; ai < animsJ->size(); ++ai) {
            const Json* a = animsJ->at(ai);
            GltfAnim clip;
            if (const Json* nm = a->get("name")) clip.name = nm->str;
            const Json* channels = a->get("channels");
            const Json* samplers = a->get("samplers");
            if (!channels || !samplers) continue;
            for (usize ci = 0; ci < channels->size(); ++ci) {
                const Json* ch = channels->at(ci);
                const Json* tgt = ch->get("target");
                if (!tgt) continue;
                const i32 node = (i32)tgt->getNum("node", -1);
                const std::string path = tgt->getStr("path", "");
                u8 p = 255;
                if (path == "translation") p = 0;
                else if (path == "rotation") p = 1;
                else if (path == "scale") p = 2;
                if (p == 255) continue;
                const Json* sm = samplers->at((u32)ch->getNum("sampler", 0));
                if (!sm) continue;
                AccessorData in, out;
                if (!getAccessor(g, *accessors, (u32)sm->getNum("input", 0), in)) continue;
                if (!getAccessor(g, *accessors, (u32)sm->getNum("output", 0), out)) continue;
                GltfAnimChannel gc;
                gc.node = node;
                gc.path = p;
                gc.times.resize(in.count);
                for (u32 k = 0; k < in.count; ++k)
                    gc.times[k] = readComp<f32>(in.ptr + k * in.stride, in.compType);
                const u32 nKeys = std::min(in.count, out.count);
                if (p == 1) {
                    gc.quatKeys.resize(nKeys);
                    for (u32 k = 0; k < nKeys; ++k) {
                        const u8* q = out.ptr + k * out.stride;
                        gc.quatKeys[k] = quat(readComp<f32>(q + 12, out.compType),
                                              readComp<f32>(q, out.compType),
                                              readComp<f32>(q + 4, out.compType),
                                              readComp<f32>(q + 8, out.compType));
                    }
                    gc.vecKeys.clear();
                } else {
                    gc.vecKeys.resize(nKeys);
                    for (u32 k = 0; k < nKeys; ++k) {
                        const u8* q = out.ptr + k * out.stride;
                        gc.vecKeys[k] = vec3(readComp<f32>(q, out.compType),
                                             readComp<f32>(q + 4, out.compType),
                                             readComp<f32>(q + 8, out.compType));
                    }
                    gc.quatKeys.clear();
                }
                for (f32 t : gc.times) clip.duration = std::max(clip.duration, t);
                clip.channels.push_back(std::move(gc));
            }
            anims.push_back(std::move(clip));
        }
        if (!anims.empty())
            SV_LOG_INFO("gltf", "%zu clips de animacion", anims.size());
    }

    return !prims.empty();
}

// ---------------------------------------------------------------------------
// Animacion: muestreo de un clip sobre la pose local
// ---------------------------------------------------------------------------
void GltfModel::sampleAnim(u32 animIndex, f32 t, GltfPose& pose) const {
    if (animIndex >= anims.size()) return;
    const GltfAnim& a = anims[animIndex];
    if (a.duration > 1e-4f) t = std::fmod(std::max(t, 0.0f), a.duration);
    else t = 0;
    for (const auto& ch : a.channels) {
        const i32 j = [&] {
            for (usize i = 0; i < joints.size(); ++i)
                if (joints[i].node == ch.node) return (i32)i;
            return -1;
        }();
        if (j < 0 || (usize)j >= pose.pos.size()) continue;
        const u32 n = (u32)ch.times.size();
        if (n == 0) continue;
        // busca el segmento (los tiempos vienen ordenados)
        u32 k = 0;
        while (k + 1 < n && ch.times[k + 1] <= t) ++k;
        const f32 t0 = ch.times[k];
        const f32 t1 = (k + 1 < n) ? ch.times[k + 1] : (t0 + 1.0f);
        const f32 alpha = (t1 > t0) ? clamp01((t - t0) / (t1 - t0)) : 0.0f;
        if (ch.path == 1) {           // rotation (slerp)
            const quat& q0 = ch.quatKeys[k];
            const quat q1 = (k + 1 < n) ? ch.quatKeys[k + 1] : q0;
            pose.rot[(usize)j] = glm::normalize(mix(q0, q1, alpha));
        } else {                      // translation / scale (lerp)
            const vec3& v0 = ch.vecKeys[k];
            const vec3 v1 = (k + 1 < n) ? ch.vecKeys[k + 1] : v0;
            if (ch.path == 0) pose.pos[(usize)j] = mix(v0, v1, alpha);
            else pose.scale[(usize)j] = mix(v0, v1, alpha);
        }
    }
}

// ---------------------------------------------------------------------------
// Matrices de piel: espacio crudo del glTF (P * global * IBM)
// ---------------------------------------------------------------------------
void GltfModel::computeSkinMatricesRaw(const GltfPose& pose,
                                       std::vector<mat4>& outSkin) const {
    const usize n = joints.size();
    outSkin.resize(n);
    static std::vector<mat4> global;
    global.resize(n);
    mat4 P(1);
    for (const mat4& p : skeletonPrefix) P = P * p;
    for (usize i = 0; i < n; ++i) {
        const mat4 local = composeTRS(pose.pos[i], pose.rot[i], pose.scale[i]);
        global[i] = joints[i].parent >= 0 ? global[(usize)joints[i].parent] * local
                                          : P * local;
        outSkin[i] = joints[i].bindFix * global[i] * joints[i].inverseBind;
    }
}

void GltfModel::computeSkinMatrices(const GltfPose& pose,
                                    std::vector<mat4>& outSkin) const {
    computeSkinMatricesRaw(pose, outSkin);
    if (!skinned) return;
    // conjuga por la escala de carga (los vertices GPU ya estan normalizados)
    // y recoloca el origen en los pies: v' = T(-c) * S * skin * S^-1 * v
    const mat4 S = glm::scale(mat4(1), vec3(loadNormScale));
    const mat4 Sinv = glm::scale(mat4(1), vec3(1.0f / std::max(loadNormScale, 1e-9f)));
    const mat4 fix = glm::translate(mat4(1), -skinCenter) * S;
    for (auto& m : outSkin) m = fix * m * Sinv;
}

mat4 GltfModel::jointMatrix(const GltfPose& pose, i32 joint) const {
    if (joint < 0 || (usize)joint >= joints.size()) return mat4(1);
    static std::vector<mat4> chain;
    chain.clear();
    i32 j = joint;
    while (j >= 0) {
        chain.push_back(composeTRS(pose.pos[(usize)j], pose.rot[(usize)j], pose.scale[(usize)j]));
        j = joints[(usize)j].parent;
    }
    mat4 m(1);
    for (const mat4& p : skeletonPrefix) m = m * p;
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) m = m * *it;
    // al espacio normalizado (como computeSkinMatrices, sin IBM: es el marco
    // de la articulacion, para anclar objetos)
    const mat4 S = glm::scale(mat4(1), vec3(loadNormScale));
    const mat4 Sinv = glm::scale(mat4(1), vec3(1.0f / std::max(loadNormScale, 1e-9f)));
    return glm::translate(mat4(1), -skinCenter) * S * m * Sinv;
}

void GltfModel::destroy() {
    for (auto& p : prims) p.mesh.destroy();
    prims.clear();
    for (auto& t : textures) t.destroy();
    textures.clear();
}

} // namespace sv
