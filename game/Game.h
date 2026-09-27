// ============================================================================
//  SKYVAULT Royale - game/Game.h
//  El battle royale: jugador TPS, 5 armas + pico, 99 bots, tormenta,
//  construccion por celdas de 4 m, cofres, glider desde el Carguero Nube.
// ============================================================================
#pragma once

#include "core/Core.h"
#include "math/Math.h"
#include "render/Renderer.h"
#include "render/GLTF.h"
#include "world/Terrain.h"
#include "physics/Physics.h"
#include "audio/Audio.h"
#include "platform/Platform.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#ifdef SKYVAULT_WEB
  #include <emscripten.h>
#endif

namespace game {

using namespace sv;

// ---------------------------------------------------------------------------
// Constantes de juego
// ---------------------------------------------------------------------------
constexpr f32 PlayerHeight   = 1.8f;
constexpr f32 PlayerRadius   = 0.45f;
constexpr f32 WalkSpeed      = 5.6f;
constexpr f32 SprintSpeed    = 8.6f;
constexpr f32 JumpSpeed      = 8.0f;
constexpr f32 Gravity        = 22.0f;
constexpr f32 CellSize       = 4.0f;
constexpr i32 MaxBots        = 99;
constexpr f32 WeaponDamageMult[5] = { 1.0f, 1.05f, 1.10f, 1.16f, 1.25f };

// ---------------------------------------------------------------------------
// Tipos
// ---------------------------------------------------------------------------
enum class State : u8 { Boot, Menu, Drop, Playing, Dead, Victory };
enum class WeaponKind : u8 { Pickaxe = 0, Shotgun, SMG, Rifle, Sniper, Count };
enum class BuildKind : u8 { Wall = 0, Floor, Ramp, Roof, Count };
enum class Rarity : u8 { Common = 0, Uncommon, Rare, Epic, Legendary };

struct WeaponDef {
    const char* name = "";
    WeaponKind kind = WeaponKind::Pickaxe;
    f32 damage = 20.0f, headMult = 1.5f;
    f32 fireInterval = 0.5f, spread = 0.01f, adsSpread = 0.002f;
    i32 magSize = 1, pellets = 1;
    f32 range = 200.0f;
    bool automatic = false;
    f32 modelScale = 1.0f;
    vec3 modelOffset{0, 0, 0};
    f32 modelYaw = 0.0f;
};

struct WeaponInstance {
    WeaponKind kind = WeaponKind::Pickaxe;
    u8 rarity = 0;
    i32 ammo = 0;
    [[nodiscard]] bool valid() const { return kind != WeaponKind::Pickaxe; }
};

struct Bot {
    vec3 pos{0, 0, 0};
    vec3 vel{0, 0, 0};
    f32 yaw = 0, targetYaw = 0;
    i32 hp = 100;
    bool alive = true;
    u8 state = 0;               // 0 vagar, 1 combate, 2 correr a la zona
    f32 fireCd = 0, burstLeft = 0, reactT = 0;
    vec2 wanderTarget{0, 0};
    f32 strafeDir = 1, strafeT = 0;
    WeaponKind weapon = WeaponKind::Rifle;
    u8 weaponRarity = 0;
    bool onGround = false;
    f32 noiseT = 0;
};

struct Chest { vec3 pos; bool opened = false; };
struct FloorLoot { vec3 pos; WeaponInstance weapon; bool taken = false; };

// caja decorativa (ventanas, remates, sin colision)
struct DecoBox { AABB box; vec4 color; f32 emissive = 0.0f; };

struct BuildPiece {
    BuildKind kind = BuildKind::Wall;
    ivec3 cell{0, 0, 0};
    u8 rot = 0;                 // 0..3
    i32 hp = 150;
    [[nodiscard]] bool operator==(const BuildPiece& o) const {
        return kind == o.kind && cell == o.cell && rot == o.rot;
    }
};

struct KillFeedEntry { std::string text; f32 t = 0; };

// ---------------------------------------------------------------------------
// Particulas (pool fijo)
// ---------------------------------------------------------------------------
struct Particle {
    vec3 pos, vel;
    f32 life = 0, maxLife = 1, size = 1;
    vec4 color{1, 1, 1, 1};
    f32 stretch = 0;
    bool additive = false;
    bool active = false;
};

// ---------------------------------------------------------------------------
// Partida
// ---------------------------------------------------------------------------
class Game {
public:
    [[nodiscard]] bool init(Renderer* renderer);
    void shutdown();
    void tick(f32 dt);                 // logica
    void render();                     // envio de draw calls + HUD
    void clickUI(f32 x, f32 y);        // clics de menu (coordenadas px)

    [[nodiscard]] State state() const { return m_state; }
    void loadStatsPublic() { loadStats(); }   // para el arranque
    void requestDebugLand() { m_debugLandRequested = true; }   // pruebas (web/consola)
    [[nodiscard]] vec3 debugPos() const { return m_playerPos; }

private:
    // --- actualizacion -------------------------------------------------------
    void updateMenu(f32 dt);
    void updateDrop(f32 dt);
    void updatePlaying(f32 dt);
    void updatePlayer(f32 dt);
    void updateBots(f32 dt);
    void updateStorm(f32 dt);
    void updateBuild(f32 dt);
    void updateParticles(f32 dt);
    void updateCamera(f32 dt);

    // --- acciones -------------------------------------------------------------
    void fireWeapon(const vec3& origin, const vec3& dir);
    void harvestHit(const RaycastHit& hit);
    void botShoot(Bot& bot, const vec3& target);
    void damageBot(Bot& bot, i32 dmg, bool headshot, bool byPlayer);
    void damagePlayer(i32 dmg);
    void damageBuildPiece(u32 id, i32 dmg);
    void tryInteract();
    void pickLoot();
    void placeBuild();
    void respawn();
    void startMatch();
    void botsDiedCheck();

    // --- utilidades ------------------------------------------------------------
    [[nodiscard]] WeaponDef weaponDef(WeaponKind k) const;
    [[nodiscard]] static vec4 rarityColor(u8 r);
    void emitParticle(const vec3& pos, const vec3& vel, f32 life, f32 size,
                      vec4 color, bool additive, f32 stretch = 0);
    void addKillFeed(const std::string& text);
    [[nodiscard]] static mat4 weaponModelMatrix(WeaponKind k, const mat4& playerMat);
    [[nodiscard]] BuildPiece ghostPiece() const;
    [[nodiscard]] bool buildPlacementValid(const BuildPiece& p) const;
    void rebuildCollision();
    void bakeMinimap();
    [[nodiscard]] i32 aliveCount() const;
    void renderHud();
    void renderMenu();
    void renderEndScreen();

    // generacion procedural
    [[nodiscard]] Mesh makeLeafyTreeMesh();
    [[nodiscard]] Mesh makePineTreeMesh();
    [[nodiscard]] Mesh makeRockMesh();
    [[nodiscard]] Mesh makeChestMesh();
    [[nodiscard]] Mesh makeUnitCube();
    [[nodiscard]] Mesh makeCharacterMesh();
    [[nodiscard]] Mesh makeWeaponMesh(WeaponKind k);
    [[nodiscard]] Mesh makeGrassMesh();
    [[nodiscard]] Mesh makeAirshipMesh();
    [[nodiscard]] Mesh makeRoofMesh();
    void generateWorldContent();
    void bakeHeightGrid();
    [[nodiscard]] f32 gridHeight(f32 x, f32 z) const;
    void regenGrass(const vec3& camPos);
    void drawGltf(const GltfModel& m, const mat4& model, vec4 tint, f32 emissive = 0.0f);
    [[nodiscard]] static mat4 gltfPivot(const GltfModel& m, const vec3& pos, f32 yaw);
    [[nodiscard]] std::pair<vec3, vec3> buildAabb(const BuildPiece& p) const;
    void damageTree(u32 id, f32 dmg);
    void pickUpWeapon(const WeaponInstance& w);
    void saveStats();
    void loadStats();
    void generateTown(Random& rng, const PoiDef& poi);

    // --- mundo ------------------------------------------------------------------
    Renderer* m_r = nullptr;
    Terrain m_terrain;
    CollisionWorld m_collision;
    Env m_env;
    Camera m_camera;
    u32 m_seed = 20240;

    // modelos GLB del usuario (assets/models, remakes fan-made) con
    // fallback automatico a los procedurales si faltan
    GltfModel m_gltfChar;
    GltfModel m_gltfWeapon[5];    // 0 Pico, 1 Escopeta, 2 Subfusil, 3 Riflle, 4 Francotirador
    bool m_gltfCharOk = false;
    bool m_gltfWeaponOk[5] = {};

    // personajes y armas procedurales (fallback)
    Mesh m_charMesh, m_pickaxeMesh, m_shotgunMesh, m_smgMesh, m_rifleMesh,
         m_sniperMesh;

    // geometria procedural
    Mesh m_treeLeafyMesh, m_treePineMesh, m_rockMesh, m_chestMesh, m_boxMesh;
    Mesh m_grassMesh, m_airshipMesh, m_roofMesh;
    struct RoofPiece { vec3 c, s; vec4 col; };
    std::vector<RoofPiece> m_roofPieces;
    std::vector<InstanceData> m_treeInstances;   // visibles por frame
    std::vector<vec4> m_treeData;                // pos+scale (colision separada)
    std::vector<u8>   m_treeKind;                // 0 frondoso, 1 pino
    std::vector<bool> m_treeAlive;
    std::vector<f32> m_treeHp;
    std::vector<InstanceData> m_rockInstances;
    std::vector<vec3> m_rockData;

    // pasto instanciado alrededor de la camara
    std::vector<InstanceData> m_grassInstances;
    vec3 m_grassLastPos{1.0e9f, 0, 1.0e9f};
    static constexpr u32 GridN = 257;            // rejilla de alturas 16 m
    std::vector<f32> m_heightGrid;

    // POIs: cajas de edificios (colision estatica) + colores/decoracion
    std::vector<ColliderBox> m_staticBoxes;
    std::vector<vec4> m_boxColors;               // paralelo a m_staticBoxes
    std::vector<DecoBox> m_decoBoxes;

    // cofres y loot
    std::vector<Chest> m_chests;
    std::vector<FloorLoot> m_loot;

    // construccion
    std::vector<BuildPiece> m_builds;
    bool m_buildMode = false;
    BuildKind m_buildKind = BuildKind::Wall;
    i32 m_wood = 0;

    // jugador
    vec3 m_playerPos{0, 100, 0}, m_playerVel{0, 0, 0};
    f32 m_playerYaw = 0, m_playerPitch = 0;
    bool m_onGround = false, m_sprinting = false, m_ads = false;
    i32 m_hp = 100, m_shield = 0;
    WeaponInstance m_slots[5];
    i32 m_activeSlot = 0;
    f32 m_fireCd = 0, m_reloading = 0, m_harvestCd = 0, m_pickAnim = 0;
    i32 m_shieldPots = 0;
    f32 m_healT = 0, m_stormDmgT = 0, m_stormSoundT = 4.0f;
    i32 m_kills = 0;
    f32 m_hurtFlash = 0, m_hitMarker = 0;
    bool m_gliding = false, m_falling = false;
    vec3 m_busPos{0, 0, 0}, m_busDir{1, 0, 0};
    f32 m_dropT = 0;
    bool m_mouseWasCaptured = false;
    bool m_debugLandRequested = false;

    // bots
    std::vector<Bot> m_bots;

    // tormenta
    vec2 m_stormCenter{0, 0}, m_stormTarget{0, 0}, m_stormFrom{0, 0};
    f32 m_stormRadius = 2200.0f, m_stormTargetRadius = 2200.0f, m_stormFromR = 2200.0f;
    f32 m_stormTimer = 20.0f;
    u32 m_stormPhase = 0;
    bool m_stormShrinking = false;
    f32 m_stormDps = 1.0f;

    // camara/estado
    State m_state = State::Boot;
    f32 m_time = 0;
    f32 m_deathT = 0;
    u32 m_wins = 0;
    u32 m_matches = 0;

    // UI
    std::vector<KillFeedEntry> m_killFeed;
    Texture m_minimapTex;
    bool m_minimapReady = false;
    f32 m_interactHint = 0;
    std::string m_interactText;

    // particulas
    std::vector<Particle> m_particles;
    std::vector<ParticleDraw> m_partDrawAdd, m_partDrawAlpha;

    // cache de UI para clics (botones del menu/fin)
    struct UiButton { vec2 min, max; u32 id = 0; };
    std::vector<UiButton> m_uiButtons;
    i32 m_menuPresetSel = 1;   // indice del preset (0..3, 1=Medio por defecto)
    bool m_menuHoverSound = false;
};

Game& gameInstance();

} // namespace game
