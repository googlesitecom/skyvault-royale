// ============================================================================
//  SKYVAULT Royale - engine/render/Renderer.h
//  Pipeline forward: sombras CSM (4x4096) -> espejo de agua -> escena HDR
//  (PBR + cielo/nubes volumetricas) -> linearizacion -> agua -> tormenta ->
//  particulas -> bloom -> composicion ACES + rayos de dios -> FXAA/vineta ->
//  UI 2D. El mismo camino en escritorio (GL 4.6) y web (WebGL2).
// ============================================================================
#pragma once

#include "core/Core.h"
#include "math/Math.h"
#include "render/RenderTypes.h"

namespace sv {

// ---------------------------------------------------------------------------
// Camara
// ---------------------------------------------------------------------------
struct Camera {
    vec3 pos{0, 10, 0};
    f32 yaw = 0, pitch = 0;          // radianes
    f32 fovY = degToRad(70.0f);
    f32 nearZ = 0.1f, farZ = 4000.0f;
    f32 aspect = 16.0f / 9.0f;
    bool ortho = false;
    f32 orthoSize = 10.0f;

    mat4 view, proj, viewProj, invViewProj;
    Frustum frustum;

    void update();                   // recalcula matrices y frustum
    [[nodiscard]] vec3 forward() const;
    [[nodiscard]] vec3 right()   const;
    [[nodiscard]] static Camera mirrored(const Camera& c, f32 planeY);
};

// ---------------------------------------------------------------------------
// Entorno (sol, niebla, colores)
// ---------------------------------------------------------------------------
struct Env {
    vec3 sunDir{normalize(vec3(0.35f, 0.55f, 0.25f))};
    vec3 sunColor{1.38f, 1.22f, 0.98f};
    vec3 ambient{0.52f, 0.56f, 0.64f};
    vec3 zenith{0.20f, 0.42f, 0.78f};
    vec3 horizon{0.72f, 0.82f, 0.92f};
    vec3 fogColor{0.68f, 0.79f, 0.90f};
    f32 fogDensity = 0.00022f;
    f32 cloudCover = 0.50f;
    vec3 stormColor{0.55f, 0.25f, 0.85f};
    f32 time = 0;
    f32 windDirX = 0.8f, windDirZ = 0.6f, windStrength = 0.55f;
};

// ---------------------------------------------------------------------------
// Presets graficos (Bajo -> Ludos)
// ---------------------------------------------------------------------------
enum class Preset : u8 { Bajo = 0, Medio, Alto, Ludos };
struct GraphicsSettings {
    Preset preset = Preset::Alto;
    i32 shadowRes    = 2048;   // por cascada
    i32 cloudSteps   = 16;
    f32 renderScale  = 1.0f;
    bool bloom       = true;
    bool waterReflection = true;   // pase espejo real (false en Bajo)
    f32 godRays      = 0.0f;
    f32 vignette     = 0.35f;
    f32 grain        = 0.03f;
    f32 chroma       = 0.6f;
    f32 shadowMix    = 0.72f;  // oscuridad de la sombra

    void apply(Preset p);
};

// ---------------------------------------------------------------------------
// Vertice unificado del motor
// ---------------------------------------------------------------------------
struct Vertex {
    vec3 pos;
    vec3 normal;
    vec2 uv;
    vec4 tangent;
    u8   joint[4];
    u8   weight[4];
    vec4 color;
};
static_assert(sizeof(Vertex) == 72, "Vertex debe ser 72 bytes");

struct InstanceData {
    mat4 model;
    vec4 color;
};

// ---------------------------------------------------------------------------
// Material PBR
// ---------------------------------------------------------------------------
struct Material {
    Texture* baseTex    = nullptr;
    Texture* normalTex  = nullptr;
    Texture* mrTex      = nullptr;
    vec4  tint{1, 1, 1, 1};
    f32   metallic = 0.0f;
    f32   rough    = 0.9f;
    f32   emissive = 0.0f;
    bool  wind     = false;    // ondular vegetacion
    bool  grass    = false;    // viento fuerte de pasto (WIND_GRASS)
    // vista previa de construccion
    f32   ghost    = 0.0f;
    vec3  ghostColor{0.2f, 0.9f, 0.4f};
};

// ---------------------------------------------------------------------------
// Particula (para el pool del juego)
// ---------------------------------------------------------------------------
struct ParticleDraw {
    vec3 pos;
    f32  size;
    vec4 color;
    vec3 vel;
    f32  stretch;
};

// ---------------------------------------------------------------------------
// Renderer
// ---------------------------------------------------------------------------
class Renderer {
public:
    [[nodiscard]] bool init(i32 viewportW, i32 viewportH);
    void shutdown();
    void resize(i32 viewportW, i32 viewportH);
    void setPreset(Preset p) { m_gfx.apply(p); rebuildTargets(); }

    // --- frame -------------------------------------------------------------
    void beginFrame(const Camera& cam, const Env& env);
    void drawMesh(const Mesh& mesh, const mat4& model, const Material& mat);
    // instancias: arboles, rocas, piezas de construccion, cofres...
    void drawInstances(const Mesh& mesh, const InstanceData* list, u32 count,
                       const Material& mat, bool castShadows = true);
    void drawParticles(const ParticleDraw* list, u32 count, bool additive);
    void drawStormWall(const mat4& model);
    void drawWater(const Camera& gameCam);      // usa espejo interno
    void endFrame(const Camera& cam);           // post + present interno

    // --- UI inmediata (pixeles, y hacia abajo) ------------------------------
    void uiBegin(i32 screenW, i32 screenH);
    void uiQuad(f32 x, f32 y, f32 w, f32 h, vec4 col);
    void uiQuadBorder(f32 x, f32 y, f32 w, f32 h, vec4 col, f32 thickness);
    void uiLine(f32 x0, f32 y0, f32 x1, f32 y1, vec4 col, f32 thickness);
    void uiImage(const Texture& tex, f32 x, f32 y, f32 w, f32 h, vec4 tint = vec4(1));
    void uiText(const char* text, f32 x, f32 y, f32 scale, vec4 col);
    f32  uiTextWidth(const char* text, f32 scale) const;
    void uiEnd();

    // --- recursos ------------------------------------------------------------
    [[nodiscard]] Mesh createMesh(const Vertex* verts, u32 vertCount,
                                  const u32* indices, u32 indexCount);
    void destroyMesh(Mesh& m) { m.destroy(); }
    // fuentes: textura de la fuente + registro
    [[nodiscard]] bool initFont();
    [[nodiscard]] static Renderer& get();

    [[nodiscard]] const GraphicsSettings& gfx() const { return m_gfx; }
    [[nodiscard]] i32 viewW() const { return m_viewW; }
    [[nodiscard]] i32 viewH() const { return m_viewH; }
    [[nodiscard]] bool hdrActive() const { return m_hdr; }

private:
    struct DrawCall {
        const Mesh* mesh;
        mat4 model;
        Material mat;
    };
    struct InstanceBatch {
        const Mesh* mesh;
        std::vector<InstanceData> list;
        Material mat;
        bool castShadows;
    };

    void renderShadowPasses();
    void renderScenePass(const Camera& cam, const Env& env, const mat4& viewProj,
                         const RenderTarget& target, bool mainPass);
    void renderLinearizePass();
    void renderSkyPass(const Camera& cam, const Env& env, const RenderTarget& target,
                       const Texture* linearDepth);
    void renderWaterPass(const Camera& cam, const Env& env);
    void renderStormPass(const Camera& cam, const Env& env);
    void renderParticlePass(const Camera& cam);
    void renderPostChain(const Camera& cam, const Env& env);
    void uiFlush(bool textured, const Texture* tex);
    void uiFlushTextured();
    void uploadInstances(const InstanceBatch& b);
    void setPbrCommonUniforms(const Shader& s, const Camera& cam, const Env& env,
                              const mat4& viewProj);
    void bindShadowTextures(const Shader& s, u32 firstUnit, f32 mix);
    void rebuildTargets();
    [[nodiscard]] mat4 cascadeMatrix(u32 cascade, const Camera& cam, vec3& outCenter,
                                     f32& outRadius) const;

    // shaders
    Shader m_pbr;            // GLTF con texturas
    Shader m_pbrVC;          // vertex color (terreno)
    Shader m_pbrInst;        // instanciado + vertex color + viento
    Shader m_pbrGrass;       // instanciado + viento fuerte de pasto
    Shader m_shadowStatic, m_shadowInst;
    Shader m_sky, m_water, m_storm, m_particle, m_ui;
    Shader m_bloomThreshold, m_blur, m_composite, m_final, m_linearize;

    // targets
    RenderTarget m_scene;        // HDR (o LDR fallback) + depth
    RenderTarget m_linearDepth;  // color: profundidad lineal en R
    RenderTarget m_bloomA, m_bloomB;     // 1/4 res
    RenderTarget m_ldr;          // post-tonemap, pre-FXAA
    RenderTarget m_mirror;       // reflexion del agua (RGBA8)
    RenderTarget m_shadowRT[4];  // profundidad por cascada

    // geometrias internas
    Mesh m_fullscreenTri;
    Mesh m_waterPlane;
    Mesh m_stormCylinder;
    Mesh m_particleQuad;
    u32  m_instanceVbo = 0;      // buffer reutilizable de instancias
    u32  m_particleVbo = 0;      // aPosSize/aColor/aVelStretch
    u32  m_uiVbo = 0, m_uiVao = 0;

    // fuentes 8x8
    Texture m_fontTex;
    static constexpr u32 FontGlyphs = 96;   // ' '..'\x7F'

    // estado de frame
    std::vector<DrawCall> m_opaque;
    std::vector<InstanceBatch> m_instanced;
    std::vector<ParticleDraw> m_particlesAdd;
    std::vector<ParticleDraw> m_particlesAlpha;
    mat4 m_stormModel;
    bool m_stormVisible = false;
    Camera* m_frameCam = nullptr;
    Env* m_frameEnv = nullptr;

    GraphicsSettings m_gfx;
    i32 m_viewW = 1280, m_viewH = 720;
    i32 m_renderW = 1280, m_renderH = 720;
    bool m_hdr = true;
    bool m_targetsOk = false;

    // UI batch
    struct UiVert { f32 x, y, u, v, r, g, b, a; };
    std::vector<UiVert> m_uiSolid;
    std::vector<UiVert> m_uiTextured;
    const Texture* m_uiTextTex = nullptr;
    i32 m_uiW = 0, m_uiH = 0;
};

} // namespace sv
