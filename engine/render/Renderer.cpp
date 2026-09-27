// ============================================================================
//  SKYVAULT Royale - engine/render/Renderer.cpp
// ============================================================================
#include "render/Renderer.h"
#include "render/Shaders.h"
#include "font8x8/font8x8_basic.h"
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace sv {

Renderer& Renderer::get() { static Renderer r; return r; }

// --- texturas por defecto ----------------------------------------------------
static Texture g_whiteTex, g_normalFlat, g_mrDefault;
static void ensureDefaultTextures() {
    if (g_whiteTex.tex) return;
    const u8 white[4]  = {255, 255, 255, 255};
    const u8 normal[4] = {128, 128, 255, 255};
    const u8 mr[4]     = {255, 229, 0, 255};   // rough 0.9, metal 0
    g_whiteTex.create2D(1, 1, TexFormat::RGBA8, white);
    g_normalFlat.create2D(1, 1, TexFormat::RGBA8, normal);
    g_mrDefault.create2D(1, 1, TexFormat::RGBA8, mr);
}

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------
void Camera::update() {
    const vec3 f(std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw));
    view = glm::lookAt(pos, pos + f, vec3(0, 1, 0));
    proj = glm::perspective(fovY, aspect, nearZ, farZ);
    viewProj = proj * view;
    invViewProj = glm::inverse(viewProj);
    frustum.fromMatrix(viewProj);
}
vec3 Camera::forward() const {
    return vec3(std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw));
}
vec3 Camera::right() const {
    return normalize(cross(forward(), vec3(0, 1, 0)));
}
Camera Camera::mirrored(const Camera& c, f32 planeY) {
    Camera m = c;
    m.pos.y = 2.0f * planeY - c.pos.y;
    m.pitch = -c.pitch;
    m.update();
    return m;
}

// ---------------------------------------------------------------------------
// Presets
// ---------------------------------------------------------------------------
void GraphicsSettings::apply(Preset p) {
    preset = p;
    switch (p) {
        case Preset::Bajo:
            shadowRes = 512; cloudSteps = 0;  renderScale = 0.60f;
            bloom = false; godRays = 0;    vignette = 0.25f; grain = 0.02f;  chroma = 0.0f;
            shadowMix = 0.60f; waterReflection = false; break;
        case Preset::Medio:
            shadowRes = 2048; cloudSteps = 12; renderScale = 0.85f;
            bloom = true;  godRays = 0.0f; vignette = 0.30f; grain = 0.025f; chroma = 0.4f;
            shadowMix = 0.68f; break;
        case Preset::Alto:
            shadowRes = 4096; cloudSteps = 20; renderScale = 1.0f;
            bloom = true;  godRays = 0.35f; vignette = 0.35f; grain = 0.03f; chroma = 0.6f;
            shadowMix = 0.72f; break;
        case Preset::Ludos:
            shadowRes = 4096; cloudSteps = 28; renderScale = 1.0f;
            bloom = true;  godRays = 0.55f; vignette = 0.40f; grain = 0.035f; chroma = 0.8f;
            shadowMix = 0.78f; break;
    }
}

// ---------------------------------------------------------------------------
// init / shutdown / resize
// ---------------------------------------------------------------------------
bool Renderer::init(i32 viewportW, i32 viewportH) {
    using namespace gl;
    m_viewW = viewportW; m_viewH = viewportH;
    m_gfx.apply(Preset::Medio);
    ensureDefaultTextures();

    const Shader::Define defVC[]    = { {"VERTEXCOLOR", 1} };
    const Shader::Define defInst[]  = { {"INSTANCED", 1}, {"VERTEXCOLOR", 1}, {"WIND", 1} };
    const Shader::Define defGltf[]  = { {"HAS_BASETEX", 1}, {"HAS_NORMALTEX", 1}, {"HAS_MRTEX", 1} };
    const Shader::Define defInstS[] = { {"INSTANCED", 1} };

    // frags que necesitan las funciones comunes (ruido, PBR, sombras) van
    // precedidas de CommonFrag; CommonFrag declara `out vec4 FragColor`.
    const std::string pbrFrag  = std::string(shaders::CommonFrag) + shaders::PbrFrag;
    const std::string skyFrag  = std::string(shaders::CommonFrag) + shaders::SkyFrag;
    const std::string waterFrag= std::string(shaders::CommonFrag) + shaders::WaterFrag;
    const std::string stormFrag= std::string(shaders::CommonFrag) + shaders::StormFrag;

    bool ok = true;
    ok &= m_pbr.load(shaders::PbrVert, pbrFrag.c_str(), defGltf, 3);
    ok &= m_pbrVC.load(shaders::PbrVert, pbrFrag.c_str(), defVC, 1);
    ok &= m_pbrInst.load(shaders::PbrVert, pbrFrag.c_str(), defInst, 3);
    ok &= m_shadowStatic.load(shaders::ShadowVert, shaders::ShadowFrag);
    ok &= m_shadowInst.load(shaders::ShadowVert, shaders::ShadowFrag, defInstS, 1);
    ok &= m_sky.load(shaders::SkyVert, skyFrag.c_str());
    ok &= m_water.load(shaders::WaterVert, waterFrag.c_str());
    ok &= m_storm.load(shaders::StormVert, stormFrag.c_str());
    ok &= m_particle.load(shaders::ParticleVert, shaders::ParticleFrag);
    ok &= m_ui.load(shaders::UiVert, shaders::UiFrag);
    ok &= m_bloomThreshold.load(shaders::PostVert, shaders::BloomThresholdFrag);
    ok &= m_blur.load(shaders::PostVert, shaders::BlurFrag);
    ok &= m_composite.load(shaders::PostVert, shaders::CompositeFrag);
    ok &= m_final.load(shaders::PostVert, shaders::FinalFrag);
    ok &= m_linearize.load(shaders::PostVert, shaders::LinearizeFrag);
    if (!ok) { SV_LOG_ERROR("render", "Fallo compilando shaders"); return false; }

    // triangulo a pantalla completa
    {
        const Vertex v[3] = {
            {{-1,-1,0},{0,0,1},{0,0},{1,0,0,1},{},{},{}},
            {{ 3,-1,0},{0,0,1},{2,0},{1,0,0,1},{},{},{}},
            {{-1, 3,0},{0,0,1},{0,2},{1,0,0,1},{},{},{}},
        };
        const u32 idx[3] = {0, 1, 2};
        m_fullscreenTri = createMesh(v, 3, idx, 3);
    }
    // plano de agua 8.4x8.4 km
    {
        const f32 S = 4200.0f;
        const Vertex v[4] = {
            {{-S,0,-S},{0,1,0},{0,0},{1,0,0,1},{},{},{}},
            {{ S,0,-S},{0,1,0},{1,0},{1,0,0,1},{},{},{}},
            {{ S,0, S},{0,1,0},{1,1},{1,0,0,1},{},{},{}},
            {{-S,0, S},{0,1,0},{0,1},{1,0,0,1},{},{},{}},
        };
        const u32 idx[6] = {0, 1, 2, 0, 2, 3};
        m_waterPlane = createMesh(v, 4, idx, 6);
    }
    // cilindro de tormenta (radio 1, altura 1, y en [0,1]); vUV.y = 0 abajo
    {
        std::vector<Vertex> v;
        std::vector<u32> idx;
        constexpr u32 Segs = 64;
        for (u32 i = 0; i <= Segs; ++i) {
            const f32 a = (f32)i / Segs * TAU_F;
            const vec3 n(std::cos(a), 0, std::sin(a));
            v.push_back({{n.x, 0, n.z}, n, {(f32)i / Segs, 0}, {1,0,0,1},{},{},{}});
            v.push_back({{n.x, 1, n.z}, n, {(f32)i / Segs, 1}, {1,0,0,1},{},{},{}});
        }
        for (u32 i = 0; i < Segs; ++i) {
            const u32 b = i * 2;
            idx.insert(idx.end(), {b, b + 2, b + 1, b + 1, b + 2, b + 3});
        }
        m_stormCylinder = createMesh(v.data(), (u32)v.size(), idx.data(), (u32)idx.size());
    }
    // quad de particulas
    {
        const Vertex v[4] = {
            {{-0.5f,-0.5f,0},{0,0,1},{0,0},{1,0,0,1},{},{},{}},
            {{ 0.5f,-0.5f,0},{0,0,1},{1,0},{1,0,0,1},{},{},{}},
            {{ 0.5f, 0.5f,0},{0,0,1},{1,1},{1,0,0,1},{},{},{}},
            {{-0.5f, 0.5f,0},{0,0,1},{0,1},{1,0,0,1},{},{},{}},
        };
        const u32 idx[6] = {0, 1, 2, 0, 2, 3};
        m_particleQuad = createMesh(v, 4, idx, 6);
        // atributos de instancia de particulas (persistentes en el VAO)
        const i32 lPos = glGetAttribLocation(m_particle.program(), "aPosSize");
        const i32 lCol = glGetAttribLocation(m_particle.program(), "aColor");
        const i32 lVel = glGetAttribLocation(m_particle.program(), "aVelStretch");
        glGenBuffers(1, &m_particleVbo);
        glBindVertexArray(m_particleQuad.vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_particleVbo);
        constexpr u32 PStride = sizeof(f32) * 12;
        if (lPos >= 0) {
            glEnableVertexAttribArray((u32)lPos);
            glVertexAttribPointer((u32)lPos, 4, GL_FLOAT, GL_FALSE, (i32)PStride, (void*)0);
            glVertexAttribDivisor((u32)lPos, 1);
        }
        if (lCol >= 0) {
            glEnableVertexAttribArray((u32)lCol);
            glVertexAttribPointer((u32)lCol, 4, GL_FLOAT, GL_FALSE, (i32)PStride, (void*)16);
            glVertexAttribDivisor((u32)lCol, 1);
        }
        if (lVel >= 0) {
            glEnableVertexAttribArray((u32)lVel);
            glVertexAttribPointer((u32)lVel, 4, GL_FLOAT, GL_FALSE, (i32)PStride, (void*)32);
            glVertexAttribDivisor((u32)lVel, 1);
        }
        glBindVertexArray(0);
    }

    glGenBuffers(1, &m_instanceVbo);
    glGenBuffers(1, &m_uiVbo);
    glGenVertexArrays(1, &m_uiVao);
    // VAO de UI: aPos(0)=2f, aUV(2)=2f, aColor(4)=4f — UiVert = 8 floats
    glBindVertexArray(m_uiVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_uiVbo);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 32, (void*)0);
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 32, (void*)8);
    glEnableVertexAttribArray(4); glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 32, (void*)16);
    glBindVertexArray(0);

    if (!initFont()) SV_LOG_WARN("render", "Fuente 8x8 no inicializada");
    rebuildTargets();
    SV_LOG_INFO("render", "Renderer listo (%s, preset Alto)",
                m_hdr ? "HDR RGBA16F" : "LDR RGBA8");
    return true;
}

void Renderer::shutdown() {
    using namespace gl;
    m_scene.destroy(); m_linearDepth.destroy(); m_bloomA.destroy();
    m_bloomB.destroy(); m_ldr.destroy(); m_mirror.destroy();
    for (auto& rt : m_shadowRT) rt.destroy();
    m_fullscreenTri.destroy(); m_waterPlane.destroy();
    m_stormCylinder.destroy(); m_particleQuad.destroy();
    m_fontTex.destroy();
    if (m_instanceVbo) glDeleteBuffers(1, &m_instanceVbo);
    if (m_particleVbo) glDeleteBuffers(1, &m_particleVbo);
    if (m_uiVbo)       glDeleteBuffers(1, &m_uiVbo);
    if (m_uiVao)       glDeleteVertexArrays(1, &m_uiVao);
}

void Renderer::resize(i32 viewportW, i32 viewportH) {
    if (viewportW == m_viewW && viewportH == m_viewH) return;
    m_viewW = viewportW; m_viewH = viewportH;
    rebuildTargets();
}

void Renderer::rebuildTargets() {
    m_renderW = std::max(2, (i32)((f32)m_viewW * m_gfx.renderScale));
    m_renderH = std::max(2, (i32)((f32)m_viewH * m_gfx.renderScale));

    m_scene.destroy(); m_linearDepth.destroy(); m_bloomA.destroy();
    m_bloomB.destroy(); m_ldr.destroy(); m_mirror.destroy();

    m_hdr = m_scene.create(m_renderW, m_renderH, true);
    if (!m_hdr) {
        SV_LOG_WARN("render", "RGBA16F no renderizable; pipeline LDR (calidad reducida)");
        m_hdr = m_scene.create(m_renderW, m_renderH, false);
    }
    m_linearDepth.create(m_renderW, m_renderH, true);
    m_ldr.create(m_renderW, m_renderH, false);
    const i32 bw = std::max(2, m_renderW / 4), bh = std::max(2, m_renderH / 4);
    m_bloomA.create(bw, bh, true);
    m_bloomB.create(bw, bh, true);
    m_mirror.create(std::max(2, m_renderW / 2), std::max(2, m_renderH / 2), false);

    const i32 sres = std::min(m_gfx.shadowRes, gpuMaxTextureSize());
    for (auto& rt : m_shadowRT) {
        rt.destroy();
        rt.createDepthOnly(sres, sres);
    }
    m_targetsOk = true;
}

// ---------------------------------------------------------------------------
// Mesh
// ---------------------------------------------------------------------------
Mesh Renderer::createMesh(const Vertex* verts, u32 vertCount, const u32* indices, u32 indexCount) {
    using namespace gl;
    Mesh m;
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ebo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, (isize)vertCount * sizeof(Vertex), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (isize)indexCount * sizeof(u32), indices, GL_STATIC_DRAW);

    constexpr usize S = sizeof(Vertex);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (i32)S, (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, (i32)S, (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, (i32)S, (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, (i32)S, (void*)offsetof(Vertex, tangent));
    glEnableVertexAttribArray(4); glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, (i32)S, (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(5); glVertexAttribPointer(5, 4, GL_UNSIGNED_BYTE, GL_FALSE, (i32)S, (void*)offsetof(Vertex, joint));
    glEnableVertexAttribArray(6); glVertexAttribPointer(6, 4, GL_UNSIGNED_BYTE, GL_TRUE,  (i32)S, (void*)offsetof(Vertex, weight));
    m.indexCount = (i32)indexCount;
    glBindVertexArray(0);
    return m;
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------
void Renderer::beginFrame(const Camera& cam, const Env& env) {
    m_opaque.clear();
    m_instanced.clear();
    m_particlesAdd.clear();
    m_particlesAlpha.clear();
    m_stormVisible = false;
    static Camera s_cam; s_cam = cam; m_frameCam = &s_cam;
    static Env s_env; s_env = env; m_frameEnv = &s_env;
}

void Renderer::drawMesh(const Mesh& mesh, const mat4& model, const Material& mat) {
    m_opaque.push_back({&mesh, model, mat});
}
void Renderer::drawInstances(const Mesh& mesh, const InstanceData* list, u32 count,
                             const Material& mat, bool castShadows) {
    for (auto& b : m_instanced)
        if (b.mesh == &mesh && b.castShadows == castShadows) {
            b.list.insert(b.list.end(), list, list + count);
            return;
        }
    InstanceBatch b;
    b.mesh = &mesh; b.mat = mat; b.castShadows = castShadows;
    b.list.assign(list, list + count);
    m_instanced.push_back(std::move(b));
}
void Renderer::drawParticles(const ParticleDraw* list, u32 count, bool additive) {
    auto& dst = additive ? m_particlesAdd : m_particlesAlpha;
    dst.insert(dst.end(), list, list + count);
}
void Renderer::drawStormWall(const mat4& model) {
    m_stormModel = model;
    m_stormVisible = true;
}
void Renderer::drawWater(const Camera&) {}

// ---------------------------------------------------------------------------
// Cascadas CSM
// ---------------------------------------------------------------------------
static const f32 CascadeSplits[5] = { 0.5f, 40.0f, 120.0f, 300.0f, 4000.0f };

mat4 Renderer::cascadeMatrix(u32 cascade, const Camera& cam, vec3& outCenter, f32& outRadius) const {
    vec3 corners[8];
    const f32 n = CascadeSplits[cascade], f = CascadeSplits[cascade + 1];
    const f32 tanHalf = std::tan(cam.fovY * 0.5f);
    const vec3 fw = cam.forward(), rt = cam.right(), up = normalize(cross(rt, fw));
    const vec3 nc = cam.pos + fw * n, fc = cam.pos + fw * f;
    const f32 nh = tanHalf * n, nw = tanHalf * n * cam.aspect;
    const f32 fh = tanHalf * f, fwS = tanHalf * f * cam.aspect;
    const vec3 nT[4] = { nc + up * nh + rt * nw, nc + up * nh - rt * nw,
                         nc - up * nh + rt * nw, nc - up * nh - rt * nw };
    const vec3 fT[4] = { fc + up * fh + rt * fwS, fc + up * fh - rt * fwS,
                         fc - up * fh + rt * fwS, fc - up * fh - rt * fwS };
    for (u32 i = 0; i < 4; ++i) { corners[i] = nT[i]; corners[i + 4] = fT[i]; }

    vec3 center(0);
    for (const auto& c : corners) center += c;
    center *= 0.125f;
    f32 radius = 0;
    for (const auto& c : corners) radius = std::max(radius, distance(c, center));
    radius = std::ceil(radius * 16.0f) / 16.0f;

    const vec3 dir = m_frameEnv->sunDir;
    const mat4 lightView = glm::lookAt(center - dir * radius * 2.0f, center, vec3(0, 1, 0));
    const mat4 lightProj = glm::ortho(-radius, radius, -radius, radius, 0.1f, radius * 4.0f);

    // snap a texels de la luz (evita shimmering al mover la camara)
    const f32 res = (f32)m_shadowRT[cascade].w;
    const vec4 lc = lightView * vec4(center, 1.0f);
    const f32 texel = (2.0f * radius) / std::max(res, 1.0f);
    const vec2 snap(std::round(lc.x / texel) * texel, std::round(lc.y / texel) * texel);
    const mat4 snappedView =
        glm::translate(mat4(1), vec3(snap.x - lc.x, snap.y - lc.y, 0)) * lightView;

    outCenter = center; outRadius = radius;
    return lightProj * snappedView;
}

void Renderer::renderShadowPasses() {
    using namespace gl;
    for (u32 c = 0; c < 4; ++c) {
        vec3 ctr; f32 rad;
        const mat4 lvp = cascadeMatrix(c, *m_frameCam, ctr, rad);
        auto& rt = m_shadowRT[c];
        rt.bind();
        glViewport(0, 0, rt.w, rt.h);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.0f, 4.0f);

        m_shadowStatic.use();
        m_shadowStatic.setMat4("uLightVP", lvp);
        for (const auto& dc : m_opaque) {
            m_shadowStatic.setMat4("uModel", dc.model);
            glBindVertexArray(dc.mesh->vao);
            glDrawElements(GL_TRIANGLES, dc.mesh->indexCount, GL_UNSIGNED_INT, nullptr);
        }
        m_shadowInst.use();
        m_shadowInst.setMat4("uLightVP", lvp);
        const mat4 ident(1);
        m_shadowInst.setMat4("uModel", ident);
        for (const auto& b : m_instanced) {
            if (!b.castShadows || b.list.empty()) continue;
            uploadInstances(b);
            glBindVertexArray(b.mesh->vao);
            glDrawElementsInstanced(GL_TRIANGLES, b.mesh->indexCount, GL_UNSIGNED_INT, nullptr,
                                    (i32)b.list.size());
        }
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::uploadInstances(const InstanceBatch& b) {
    using namespace gl;
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVbo);
    glBufferData(GL_ARRAY_BUFFER, (isize)b.list.size() * sizeof(InstanceData),
                 b.list.data(), GL_DYNAMIC_DRAW);
    for (u32 a = 0; a < 4; ++a) {
        glEnableVertexAttribArray(7 + a);
        glVertexAttribPointer(7 + a, 4, GL_FLOAT, GL_FALSE, (i32)sizeof(InstanceData),
                              (void*)(a * sizeof(vec4)));
        glVertexAttribDivisor(7 + a, 1);
    }
    glEnableVertexAttribArray(11);
    glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, (i32)sizeof(InstanceData),
                          (void*)sizeof(mat4));
    glVertexAttribDivisor(11, 1);
}

void Renderer::setPbrCommonUniforms(const Shader& s, const Camera& cam, const Env& env,
                                    const mat4& viewProj) {
    s.setMat4("uViewProj", viewProj);
    s.setU3fv("uSunDir", &env.sunDir[0]);
    s.setU3fv("uSunColor", &env.sunColor[0]);
    s.setU3fv("uAmbient", &env.ambient[0]);
    s.setU3fv("uCameraPos", &cam.pos[0]);
    s.setU3fv("uFogColor", &env.fogColor[0]);
    s.setU1f("uFogDensity", env.fogDensity);
    s.setU1f("uTime", env.time);
    s.setU3f("uWindDir", env.windDirX, 0, env.windDirZ);
    s.setU1f("uWindStrength", env.windStrength);
}

void Renderer::bindShadowTextures(const Shader& s, u32 firstUnit, f32 mix) {
    using namespace gl;
    for (u32 i = 0; i < 4; ++i) {
        glActiveTexture(GL_TEXTURE0 + firstUnit + i);
        glBindTexture(GL_TEXTURE_2D, m_shadowRT[i].depth.tex);
    }
    char name[16];
    for (u32 i = 0; i < 4; ++i) {
        std::snprintf(name, sizeof(name), "uShadow%d", i);
        s.setU1i(name, (i32)(firstUnit + i));
    }
    mat4 lvp[4];
    for (u32 c = 0; c < 4; ++c) {
        vec3 ctr; f32 rad;
        lvp[c] = cascadeMatrix(c, *m_frameCam, ctr, rad);
    }
    s.setMat4v("uLightVP", glm::value_ptr(lvp[0]), 4);
    const f32 sres = (f32)m_shadowRT[0].w;
    s.setU2f("uShadowTexel", 1.0f / sres, 1.0f / sres);
    s.setU1f("uShadowMix", mix);
}

// ---------------------------------------------------------------------------
// Pase de escena (principal y espejo)
// ---------------------------------------------------------------------------
void Renderer::renderScenePass(const Camera& cam, const Env& env, const mat4& viewProj,
                               const RenderTarget& target, bool mainPass) {
    using namespace gl;
    target.bind(); target.bindSize();
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // GLTF / texturas
    m_pbr.use();
    setPbrCommonUniforms(m_pbr, cam, env, viewProj);
    bindShadowTextures(m_pbr, 1, mainPass ? m_gfx.shadowMix : 0.0f);
    m_pbr.setU1i("uBaseTex", 0);
    m_pbr.setU1i("uNormalTex", 5);
    m_pbr.setU1i("uMetalRoughTex", 6);
    for (const auto& dc : m_opaque) {
        if (!dc.mat.baseTex) continue;
        m_pbr.setMat4("uModel", dc.model);
        dc.mat.baseTex->bind(0);
        if (dc.mat.normalTex) dc.mat.normalTex->bind(5);
        else { glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, g_normalFlat.tex); }
        if (dc.mat.mrTex) dc.mat.mrTex->bind(6);
        else { glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, g_mrDefault.tex); }
        m_pbr.setU4f("uTint", dc.mat.tint.r, dc.mat.tint.g, dc.mat.tint.b, dc.mat.tint.a);
        m_pbr.setU1f("uMetallic", dc.mat.metallic);
        m_pbr.setU1f("uRough", dc.mat.rough);
        m_pbr.setU1f("uEmissive", dc.mat.emissive);
        m_pbr.setU1f("uGhost", dc.mat.ghost);
        m_pbr.setU3fv("uGhostColor", &dc.mat.ghostColor[0]);
        glBindVertexArray(dc.mesh->vao);
        glDrawElements(GL_TRIANGLES, dc.mesh->indexCount, GL_UNSIGNED_INT, nullptr);
    }

    // vertex color (terreno y procedural)
    m_pbrVC.use();
    setPbrCommonUniforms(m_pbrVC, cam, env, viewProj);
    bindShadowTextures(m_pbrVC, 1, mainPass ? m_gfx.shadowMix : 0.0f);
    for (const auto& dc : m_opaque) {
        if (dc.mat.baseTex) continue;
        m_pbrVC.setMat4("uModel", dc.model);
        m_pbrVC.setU4f("uTint", dc.mat.tint.r, dc.mat.tint.g, dc.mat.tint.b, dc.mat.tint.a);
        m_pbrVC.setU1f("uMetallic", dc.mat.metallic);
        m_pbrVC.setU1f("uRough", dc.mat.rough);
        m_pbrVC.setU1f("uEmissive", dc.mat.emissive);
        m_pbrVC.setU1f("uGhost", dc.mat.ghost);
        m_pbrVC.setU3fv("uGhostColor", &dc.mat.ghostColor[0]);
        glBindVertexArray(dc.mesh->vao);
        glDrawElements(GL_TRIANGLES, dc.mesh->indexCount, GL_UNSIGNED_INT, nullptr);
    }

    // instanciado
    m_pbrInst.use();
    setPbrCommonUniforms(m_pbrInst, cam, env, viewProj);
    bindShadowTextures(m_pbrInst, 1, mainPass ? m_gfx.shadowMix : 0.0f);
    const mat4 ident(1);
    m_pbrInst.setMat4("uModel", ident);
    m_pbrInst.setU1f("uMetallic", 0.0f);
    m_pbrInst.setU1f("uRough", 0.95f);
    m_pbrInst.setU1f("uEmissive", 0.0f);
    for (const auto& b : m_instanced) {
        if (b.list.empty()) continue;
        uploadInstances(b);
        glBindVertexArray(b.mesh->vao);
        m_pbrInst.setU4f("uTint", b.mat.tint.r, b.mat.tint.g, b.mat.tint.b, b.mat.tint.a);
        m_pbrInst.setU1f("uWindStrength", b.mat.wind ? m_frameEnv->windStrength : 0.0f);
        m_pbrInst.setU1f("uGhost", b.mat.ghost);
        m_pbrInst.setU3fv("uGhostColor", &b.mat.ghostColor[0]);
        glDrawElementsInstanced(GL_TRIANGLES, b.mesh->indexCount, GL_UNSIGNED_INT, nullptr,
                                (i32)b.list.size());
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ---------------------------------------------------------------------------
// Pases especificos
// ---------------------------------------------------------------------------
void Renderer::renderSkyPass(const Camera& cam, const Env& env, const RenderTarget& target,
                             const Texture* linearDepth) {
    using namespace gl;
    target.bind(); target.bindSize();
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    m_sky.use();
    m_sky.setMat4("uInvViewProj", cam.invViewProj);
    m_sky.setU3fv("uSunDir", &env.sunDir[0]);
    m_sky.setU3fv("uSunColor", &env.sunColor[0]);
    m_sky.setU3fv("uZenith", &env.zenith[0]);
    m_sky.setU3fv("uHorizon", &env.horizon[0]);
    m_sky.setU3fv("uCameraPos", &cam.pos[0]);
    m_sky.setU1f("uTime", env.time);
    m_sky.setU1i("uCloudSteps", m_gfx.cloudSteps);
    m_sky.setU1f("uCloudCover", env.cloudCover);
    m_sky.setU2f("uRes", (f32)target.w, (f32)target.h);
    m_sky.setU1i("uSceneDepth", 0);
    glActiveTexture(GL_TEXTURE0);
    if (linearDepth) glBindTexture(GL_TEXTURE_2D, linearDepth->tex);
    else             glBindTexture(GL_TEXTURE_2D, g_whiteTex.tex);  // espejo: todo "cielo"
    glBindVertexArray(m_fullscreenTri.vao);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);
    glDepthMask(GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::renderLinearizePass() {
    using namespace gl;
    m_linearDepth.bind(); m_linearDepth.bindSize();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    m_linearize.use();
    m_linearize.setU1i("uDepth", 0);
    m_linearize.setU2f("uNearFar", m_frameCam->nearZ, m_frameCam->farZ);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_scene.depth.tex);
    glBindVertexArray(m_fullscreenTri.vao);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::renderWaterPass(const Camera& cam, const Env& env) {
    using namespace gl;
    m_scene.bind(); m_scene.bindSize();
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    m_water.use();
    const mat4 ident(1);
    m_water.setMat4("uViewProj", cam.viewProj);
    m_water.setMat4("uModel", ident);
    m_water.setU3fv("uCameraPos", &cam.pos[0]);
    m_water.setU3fv("uSunDir", &env.sunDir[0]);
    m_water.setU3fv("uSunColor", &env.sunColor[0]);
    m_water.setU3fv("uFogColor", &env.fogColor[0]);
    m_water.setU1f("uFogDensity", env.fogDensity);
    m_water.setU1f("uTime", env.time);
    m_water.setU2f("uNearFar", cam.nearZ, cam.farZ);
    m_water.setU1i("uSceneDepth", 0);
    m_water.setU1i("uReflection", 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_linearDepth.color.tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_mirror.color.tex);
    glBindVertexArray(m_waterPlane.vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::renderStormPass(const Camera& cam, const Env& env) {
    using namespace gl;
    m_scene.bind(); m_scene.bindSize();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    m_storm.use();
    m_storm.setMat4("uViewProj", cam.viewProj);
    m_storm.setMat4("uModel", m_stormModel);
    m_storm.setU3fv("uCameraPos", &cam.pos[0]);
    m_storm.setU3fv("uStormColor", &env.stormColor[0]);
    m_storm.setU3fv("uFogColor", &env.fogColor[0]);
    m_storm.setU1f("uTime", env.time);
    glBindVertexArray(m_stormCylinder.vao);
    glDrawElements(GL_TRIANGLES, m_stormCylinder.indexCount, GL_UNSIGNED_INT, nullptr);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::renderParticlePass(const Camera& cam) {
    using namespace gl;
    if (m_particlesAlpha.empty() && m_particlesAdd.empty()) return;
    m_scene.bind(); m_scene.bindSize();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    m_particle.use();
    m_particle.setMat4("uViewProj", cam.viewProj);
    const vec3 rt = cam.right();
    const vec3 up = normalize(cross(cam.forward(), rt));
    m_particle.setU3fv("uCamRight", &rt[0]);
    m_particle.setU3fv("uCamUp", &up[0]);

    // cada particula = 12 floats
    std::vector<f32> data;
    auto drawList = [&](std::vector<ParticleDraw>& list) {
        if (list.empty()) return;
        data.clear();
        data.reserve(list.size() * 12);
        for (const auto& p : list) {
            data.push_back(p.pos.x); data.push_back(p.pos.y); data.push_back(p.pos.z);
            data.push_back(p.size);
            data.push_back(p.color.r); data.push_back(p.color.g);
            data.push_back(p.color.b); data.push_back(p.color.a);
            data.push_back(p.vel.x);   data.push_back(p.vel.y);
            data.push_back(p.vel.z);   data.push_back(p.stretch);
        }
        glBindVertexArray(m_particleQuad.vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_particleVbo);
        glBufferData(GL_ARRAY_BUFFER, (isize)data.size() * sizeof(f32), data.data(), GL_DYNAMIC_DRAW);
        glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, (i32)list.size());
    };

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    drawList(m_particlesAlpha);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    drawList(m_particlesAdd);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::renderPostChain(const Camera& cam, const Env& env) {
    using namespace gl;
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);

    const auto drawTri = [&]() {
        glBindVertexArray(m_fullscreenTri.vao);
        glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);
    };

    // bloom
    if (m_gfx.bloom) {
        m_bloomA.bind(); m_bloomA.bindSize();
        m_bloomThreshold.use();
        m_bloomThreshold.setU1i("uScene", 0);
        m_bloomThreshold.setU1f("uThreshold", 1.05f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_scene.color.tex);
        drawTri();

        m_blur.use();
        for (i32 i = 0; i < 2; ++i) {
            m_bloomB.bind(); m_bloomB.bindSize();
            m_blur.setU1i("uSrc", 0);
            m_blur.setU2f("uDir", 1.0f / (f32)m_bloomA.w, 0.0f);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_bloomA.color.tex);
            drawTri();

            m_bloomA.bind(); m_bloomA.bindSize();
            m_blur.setU1i("uSrc", 0);
            m_blur.setU2f("uDir", 0.0f, 1.0f / (f32)m_bloomB.h);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_bloomB.color.tex);
            drawTri();
        }
    }

    // composicion (ACES + rayos de dios)
    m_ldr.bind(); m_ldr.bindSize();
    m_composite.use();
    m_composite.setU1i("uScene", 0);
    m_composite.setU1i("uBloom", 1);
    m_composite.setU1i("uSceneDepth", 2);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_scene.color.tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_gfx.bloom ? m_bloomA.color.tex : g_whiteTex.tex);
    if (m_gfx.bloom) { glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); }
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, m_linearDepth.color.tex);
    // sol en NDC
    vec2 sunNdc(-9, 9);
    {
        const vec3 sp = cam.pos + env.sunDir * 1000.0f;
        const vec4 pc = cam.viewProj * vec4(sp, 1.0f);
        if (pc.w > 0.0f) sunNdc = vec2(pc.x / pc.w, pc.y / pc.w);
    }
    m_composite.setU2f("uSunNdc", sunNdc.x, sunNdc.y);
    m_composite.setU1f("uGodRays", m_gfx.godRays);
    m_composite.setU1f("uBloomStrength", m_gfx.bloom ? 0.55f : 0.0f);
    m_composite.setU1f("uExposure", 1.05f);
    drawTri();

    // final (FXAA + vineta + grano + aberracion) directo a pantalla
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_viewW, m_viewH);
    m_final.use();
    m_final.setU1i("uSrc", 0);
    m_final.setU2f("uRes", (f32)m_renderW, (f32)m_renderH);
    m_final.setU1f("uVignette", m_gfx.vignette);
    m_final.setU1f("uGrain", m_gfx.grain);
    m_final.setU1f("uChroma", m_gfx.chroma);
    m_final.setU1f("uTime", env.time);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_ldr.color.tex);
    drawTri();
    glDepthMask(GL_TRUE);
}

// ---------------------------------------------------------------------------
// endFrame
// ---------------------------------------------------------------------------
void Renderer::endFrame(const Camera& cam) {
    if (!m_targetsOk) return;
    const Env& env = *m_frameEnv;
    using namespace gl;

    renderShadowPasses();

    // espejo del agua (en Bajo se refleja solo el cielo: mucho mas barato)
    if (cam.pos.y > 0.5f) {
        const Camera mcam = Camera::mirrored(cam, 0.0f);
        glFrontFace(GL_CW);
        if (m_gfx.waterReflection) {
            renderScenePass(mcam, env, mcam.viewProj, m_mirror, false);
            renderSkyPass(mcam, env, m_mirror, nullptr);
        } else {
            renderSkyPass(mcam, env, m_mirror, nullptr);
        }
        glFrontFace(GL_CCW);
    }

    renderScenePass(cam, env, cam.viewProj, m_scene, true);
    renderLinearizePass();
    renderSkyPass(cam, env, m_scene, &m_linearDepth.color);
    renderWaterPass(cam, env);
    if (m_stormVisible) renderStormPass(cam, env);
    renderParticlePass(cam);
    renderPostChain(cam, env);
}

// ---------------------------------------------------------------------------
// Fuente 8x8
// ---------------------------------------------------------------------------
bool Renderer::initFont() {
    constexpr u32 Cols = 16, Rows = 6, Cell = 9, W = Cols * Cell, H = Rows * Cell;
    std::vector<u8> px((usize)W * H * 4, 0);
    for (u32 g = 0; g < FontGlyphs; ++g) {
        const u32 col = g % Cols, row = g / Cols;
        const unsigned char* glyph = font8x8_basic[32 + (int)g];
        for (u32 y = 0; y < 8; ++y)
            for (u32 x = 0; x < 8; ++x) {
                if ((glyph[y] >> x) & 1) {
                    const usize idx = ((usize)(row * Cell + y) * W + (col * Cell + x)) * 4;
                    px[idx] = px[idx + 1] = px[idx + 2] = px[idx + 3] = 255;
                }
            }
    }
    return m_fontTex.create2D((i32)W, (i32)H, TexFormat::RGBA8, px.data(), false, true);
}

// ---------------------------------------------------------------------------
// UI inmediata
// ---------------------------------------------------------------------------
void Renderer::uiBegin(i32 screenW, i32 screenH) {
    m_uiSolid.clear();
    m_uiTextured.clear();
    m_uiW = screenW; m_uiH = screenH;
}
void Renderer::uiQuad(f32 x, f32 y, f32 w, f32 h, vec4 col) {
    const UiVert q[6] = {
        {x, y,       0, 0, col.r, col.g, col.b, col.a},
        {x + w, y,   0, 0, col.r, col.g, col.b, col.a},
        {x + w, y + h,0, 0, col.r, col.g, col.b, col.a},
        {x, y,       0, 0, col.r, col.g, col.b, col.a},
        {x + w, y + h,0, 0, col.r, col.g, col.b, col.a},
        {x, y + h,   0, 0, col.r, col.g, col.b, col.a},
    };
    m_uiSolid.insert(m_uiSolid.end(), q, q + 6);
}
void Renderer::uiQuadBorder(f32 x, f32 y, f32 w, f32 h, vec4 col, f32 t) {
    uiQuad(x, y, w, t, col);
    uiQuad(x, y + h - t, w, t, col);
    uiQuad(x, y, t, h, col);
    uiQuad(x + w - t, y, t, h, col);
}
void Renderer::uiLine(f32 x0, f32 y0, f32 x1, f32 y1, vec4 col, f32 t) {
    uiQuad(std::min(x0, x1), std::min(y0, y1),
           std::max(std::fabs(x1 - x0), t), std::max(std::fabs(y1 - y0), t), col);
}
void Renderer::uiImage(const Texture& tex, f32 x, f32 y, f32 w, f32 h, vec4 tint) {
    const UiVert q[6] = {
        {x, y,       0, 0, tint.r, tint.g, tint.b, tint.a},
        {x + w, y,   1, 0, tint.r, tint.g, tint.b, tint.a},
        {x + w, y + h,1, 1, tint.r, tint.g, tint.b, tint.a},
        {x, y,       0, 0, tint.r, tint.g, tint.b, tint.a},
        {x + w, y + h,1, 1, tint.r, tint.g, tint.b, tint.a},
        {x, y + h,   0, 1, tint.r, tint.g, tint.b, tint.a},
    };
    if (m_uiTextTex != &tex) { uiFlushTextured(); }
    m_uiTextTex = &tex;
    m_uiTextured.insert(m_uiTextured.end(), q, q + 6);
}
void Renderer::uiText(const char* text, f32 x, f32 y, f32 scale, vec4 col) {
    f32 cx = x;
    for (const char* c = text; *c; ++c) {
        if (*c < ' ' || *c > '~') { cx += 8 * scale; continue; }
        const u32 g = (u32)(*c - ' ');
        const f32 u0 = (f32)(g % 16 * 9) / 144.0f;
        const f32 v0 = (f32)(g / 16 * 9) / 54.0f;
        const f32 u1 = u0 + 8.0f / 144.0f, v1 = v0 + 8.0f / 54.0f;
        const f32 x0 = cx, y0 = y, x1 = cx + 8 * scale, y1 = y + 8 * scale;
        const UiVert q[6] = {
            {x0, y0, u0, v0, col.r, col.g, col.b, col.a},
            {x1, y0, u1, v0, col.r, col.g, col.b, col.a},
            {x1, y1, u1, v1, col.r, col.g, col.b, col.a},
            {x0, y0, u0, v0, col.r, col.g, col.b, col.a},
            {x1, y1, u1, v1, col.r, col.g, col.b, col.a},
            {x0, y1, u0, v1, col.r, col.g, col.b, col.a},
        };
        if (m_uiTextTex != &m_fontTex) uiFlushTextured();
        m_uiTextTex = &m_fontTex;
        m_uiTextured.insert(m_uiTextured.end(), q, q + 6);
        cx += 8 * scale;
    }
}
f32 Renderer::uiTextWidth(const char* text, f32 scale) const {
    return (f32)std::strlen(text) * 8.0f * scale;
}
void Renderer::uiFlushTextured() {
    if (m_uiTextured.empty()) return;
    // vuelca el lote con textura actual; se llama al cambiar de textura o en uiEnd
    uiFlush(true, m_uiTextTex);
    m_uiTextured.clear();
}
void Renderer::uiFlush(bool textured, const Texture* tex) {
    using namespace gl;
    const auto& list = textured ? m_uiTextured : m_uiSolid;
    if (list.empty()) return;
    m_ui.use();
    m_ui.setU2f("uRes", (f32)m_uiW, (f32)m_uiH);
    m_ui.setU1f("uUseTex", textured ? 1.0f : 0.0f);
    m_ui.setU1i("uTex", 0);
    glActiveTexture(GL_TEXTURE0);
    if (tex) glBindTexture(GL_TEXTURE_2D, tex->tex);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_uiW, m_uiH);
    glBindVertexArray(m_uiVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_uiVbo);
    glBufferData(GL_ARRAY_BUFFER, (isize)list.size() * sizeof(UiVert), list.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (i32)list.size());
    glDisable(GL_BLEND);
}
void Renderer::uiEnd() {
    uiFlushTextured();
    uiFlush(false, nullptr);
    m_uiTextTex = nullptr;
}

} // namespace sv
