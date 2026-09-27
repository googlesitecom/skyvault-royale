// ============================================================================
//  SKYVAULT Royale - tests unitarios del motor (solo CPU, sin GPU ni ventana)
//  Ejecutar: ./build/sv_tests
// ============================================================================
#include "core/Core.h"
#include "core/Json.h"
#include "math/Math.h"
#include "memory/Memory.h"
#include "threading/JobSystem.h"
#include "ecs/ECS.h"
#include "world/Noise.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>

using namespace sv;

static int g_checks = 0, g_failures = 0;

#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failures; \
    std::printf("  FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define CHECK_NEAR(a, b, eps) do { ++g_checks; const f64 d_ = std::fabs(static_cast<f64>(a) - static_cast<f64>(b)); \
    if (!(d_ <= static_cast<f64>(eps))) { ++g_failures; \
    std::printf("  FALLO %s:%d: |%s - %s| = %f > %f\n", __FILE__, __LINE__, #a, #b, d_, static_cast<f64>(eps)); } } while (0)

// ---------------------------------------------------------------------------
static void testCore() {
    std::printf("[core]\n");

    CHECK(sv::format("hola %d %.2f %s", 42, 3.14159, "x") == "hola 42 3.14 x");

    sv::Random r1, r2;
    r1.seed(777);
    r2.seed(777);
    bool same = true;
    for (i32 i = 0; i < 1000; ++i)
        if (r1.nextU32() != r2.nextU32()) { same = false; break; }
    CHECK(same); // determinismo

    r1.seed(777);
    bool inRange = true;
    for (i32 i = 0; i < 10000; ++i) {
        const u32 v = r1.rangeU32(37);
        if (v >= 37) inRange = false;
    }
    CHECK(inRange);

    sv::Config cfg;
    cfg.set("graficos", "alto");
    cfg.set("fps_cap", 144);
    cfg.set("vsync", true);
    cfg.set("gamma", 2.35f);
    CHECK(cfg.getStr("graficos") == "alto");
    CHECK(cfg.getInt("fps_cap") == 144);
    CHECK(cfg.getBool("vsync"));
    CHECK_NEAR(cfg.getFloat("gamma"), 2.35f, 1e-3f);
    CHECK(cfg.getInt("inexistente", 7) == 7);

    const std::string tmp = "test_config_tmp.ini";
    CHECK(cfg.save(tmp.c_str()));
    sv::Config loaded;
    CHECK(loaded.load(tmp.c_str()));
    CHECK(loaded.getInt("fps_cap") == 144);
    CHECK(loaded.getBool("vsync"));
    std::remove(tmp.c_str());

    CHECK(sv::sid("hello") == sv::sidRuntime("hello"));
    CHECK(sv::sid("hello") != sv::sid("world"));
}

// ---------------------------------------------------------------------------
static void testMath() {
    std::printf("[math]\n");

    CHECK_NEAR(sv::clamp01(2.0f), 1.0f, 1e-6);
    CHECK_NEAR(sv::lerpT(0.0f, 10.0f, 0.25f), 2.5f, 1e-6);
    CHECK_NEAR(sv::wrapAngle(7.0f), 7.0f - sv::TAU_F, 1e-5f);
    CHECK_NEAR(sv::wrapAngle(-7.0f), -7.0f + sv::TAU_F, 1e-5f);
    CHECK_NEAR(sv::smoothstep(0.0f, 1.0f, 0.5f), 0.5f, 1e-5f);
    CHECK_NEAR(sv::smoothstep(0.0f, 1.0f, 0.0f), 0.0f, 1e-6f);
    CHECK_NEAR(sv::smoothstep(0.0f, 1.0f, 1.0f), 1.0f, 1e-6f);

    // AABB
    sv::AABB box = sv::AABB::fromCenterHalf(sv::vec3(0.0f), sv::vec3(1.0f));
    CHECK(box.contains(sv::vec3(0.5f, 0.5f, 0.5f)));
    CHECK(!box.contains(sv::vec3(1.5f, 0.0f, 0.0f)));
    CHECK(box.intersects(sv::AABB::fromCenterHalf(sv::vec3(1.0f), sv::vec3(0.5f))));
    CHECK(!box.intersects(sv::AABB::fromCenterHalf(sv::vec3(5.0f), sv::vec3(0.5f))));
    CHECK(box.intersectsSphere(sv::vec3(1.8f, 0.0f, 0.0f), 1.0f));
    CHECK(!box.intersectsSphere(sv::vec3(2.5f, 0.0f, 0.0f), 1.0f));

    // rayAABB
    sv::Ray ray{sv::vec3(-5.0f, 0.0f, 0.0f), sv::vec3(1.0f, 0.0f, 0.0f)};
    f32 t = -1.0f;
    CHECK(sv::rayAABB(ray, box, t));
    CHECK_NEAR(t, 4.0f, 1e-4f);
    sv::Ray miss{sv::vec3(-5.0f, 5.0f, 0.0f), sv::vec3(1.0f, 0.0f, 0.0f)};
    CHECK(!sv::rayAABB(miss, box, t));

    // segmentSphere
    f32 ts = -1.0f;
    CHECK(sv::segmentSphere(sv::vec3(-5.0f, 0.0f, 0.0f), sv::vec3(5.0f, 0.0f, 0.0f), sv::vec3(0.0f), 1.0f, ts));
    CHECK_NEAR(ts, 4.0f, 1e-4f);

    // closestPointOnSegment
    const sv::vec3 cp = sv::closestPointOnSegment(sv::vec3(0.5f, 1.0f, 0.0f), sv::vec3(0.0f), sv::vec3(1.0f, 0.0f, 0.0f));
    CHECK_NEAR(glm::distance(cp, sv::vec3(0.5f, 0.0f, 0.0f)), 0.0f, 1e-5f);

    // Frustum
    const sv::mat4 proj = sv::perspectiveFov(sv::degToRad(70.0f), 16.0f / 9.0f, 0.1f, 100.0f);
    const sv::vec3 eye(0.0f, 0.0f, 5.0f);
    const sv::mat4 view = glm::lookAt(eye, sv::vec3(0.0f, 0.0f, 0.0f), sv::vec3(0.0f, 1.0f, 0.0f));
    sv::Frustum fr;
    fr.fromMatrix(proj * view);
    CHECK(fr.containsPoint(sv::vec3(0.0f, 0.0f, 0.0f)));   // delante de la camara
    CHECK(!fr.containsPoint(sv::vec3(0.0f, 0.0f, 6.0f)));  // detras
    CHECK(!fr.containsPoint(sv::vec3(500.0f, 0.0f, 0.0f))); // muy a la derecha
    CHECK(fr.intersectsSphere(sv::vec3(0.0f, 0.0f, -10.0f), 1.0f));

    // SH9: luz desde arriba ilumina mas la normal hacia arriba
    sv::SH9 sh;
    sh.addSample(sv::vec3(0.0f, 1.0f, 0.0f), sv::vec3(1.0f, 0.5f, 0.2f));
    const sv::vec3 up = sh.evaluate(sv::vec3(0.0f, 1.0f, 0.0f));
    const sv::vec3 dn = sh.evaluate(sv::vec3(0.0f, -1.0f, 0.0f));
    CHECK(up.y > dn.y);
    CHECK(up.x > 0.0f);

    // hsv roundtrip
    const sv::vec3 col(0.33f, 0.75f, 0.92f);
    const sv::vec3 hsv = sv::rgbToHsv(col);
    const sv::vec3 back = sv::hsvToRgb(hsv);
    CHECK_NEAR(glm::distance(back, col), 0.0f, 2e-3f);
}

// ---------------------------------------------------------------------------
static void testJson() {
    std::printf("[json]\n");

    sv::Json root = sv::Json::makeObject();
    root.set("nombre", sv::Json::makeString("SkyVault"));
    root.set("nivel", sv::Json::makeNumber(42));
    root.set("activo", sv::Json::makeBool(true));
    sv::Json arr = sv::Json::makeArray();
    for (i32 i = 0; i < 3; ++i) arr.push(sv::Json::makeNumber(i * 10.0));
    root.set("puntuaciones", std::move(arr));
    sv::Json anidado = sv::Json::makeObject();
    anidado.set("x", sv::Json::makeNumber(1.5));
    root.set("pos", std::move(anidado));

    const std::string text = root.dumpStr(true);
    sv::Json parsed;
    std::string err;
    CHECK(parsed.parse(text, &err));
    CHECK(parsed.getStr("nombre", "") == "SkyVault");
    CHECK(parsed.getInt("nivel", -1) == 42);
    CHECK(parsed.getBool("activo", false));
    const sv::Json* scores = parsed.get("puntuaciones");
    CHECK(scores && scores->size() == 3);
    CHECK(scores && scores->at(2) && scores->at(2)->number == 20.0);
    CHECK_NEAR(parsed.get("pos")->getNum("x", 0.0), 1.5, 1e-9);

    // errores
    CHECK(!parsed.parse("{\"a\": ", &err));
    CHECK(!parsed.parse("[1, 2", &err));
    CHECK(!parsed.parse("nulo", &err));
    // escape unicode
    CHECK(parsed.parse("\"A\"", &err) && parsed.str == "A");
    CHECK(parsed.parse("\"\\u0041\"", &err) && parsed.str == "A");
}

// ---------------------------------------------------------------------------
struct Position { f32 x = 0, y = 0; };
struct Health   { i32 hp = 100; };
struct Tag      { u32 id = 0; };

static void testEcs() {
    std::printf("[ecs]\n");

    sv::ecs::Registry reg;
    sv::ecs::Entity ents[100];
    for (i32 i = 0; i < 100; ++i) {
        ents[i] = reg.create();
        CHECK(reg.valid(ents[i]));
        if (i % 2 == 0) reg.emplace<Position>(ents[i], static_cast<f32>(i), 0.0f);
        if (i % 3 == 0) reg.emplace<Health>(ents[i], 100);
    }
    CHECK(reg.size() == 100);
    CHECK(reg.count<Position>() == 50);
    CHECK(reg.count<Health>() == 34); // multiplos de 3 en [0,100): 0,3,...,99

    usize posCount = 0;
    reg.each<Position>([&](sv::ecs::Entity, Position&) { ++posCount; });
    CHECK(posCount == 50);

    usize both = 0;
    reg.each2<Position, Health>([&](sv::ecs::Entity, Position&, Health&) { ++both; });
    CHECK(both == 17); // multiplos de 6 en [0,100)

    // acceso
    CHECK(reg.has<Position>(ents[0]));
    CHECK_NEAR(reg.get<Position>(ents[0]).x, 0.0f, 1e-6f);
    reg.get<Position>(ents[2]).x = 99.0f;
    CHECK_NEAR(reg.get<Position>(ents[2]).x, 99.0f, 1e-6f);

    // destroy limpia componentes
    reg.destroy(ents[0]);
    CHECK(!reg.valid(ents[0]));
    CHECK(reg.count<Position>() == 49);
    CHECK(reg.size() == 99);

    // handle viejo invalido por generacion
    const sv::ecs::Entity stale = ents[0];
    const sv::ecs::Entity reused = reg.create();
    CHECK(!reg.valid(stale));
    (void)reused;

    // find en entidad sin componente
    CHECK(reg.find<Health>(ents[1]) == nullptr);

    // each3
    reg.emplace<Tag>(ents[6], 1u);
    usize tri = 0;
    reg.each3<Position, Health, Tag>([&](sv::ecs::Entity, Position&, Health&, Tag&) { ++tri; });
    CHECK(tri == 1);
}

// ---------------------------------------------------------------------------
static void testNoise() {
    std::printf("[noise]\n");

    sv::Noise a(1234), b(1234), c(9999);
    CHECK_NEAR(a.perlin2(3.7f, 8.1f), b.perlin2(3.7f, 8.1f), 1e-12f); // determinista
    bool differs = std::fabs(a.perlin2(3.7f, 8.1f) - c.perlin2(3.7f, 8.1f)) > 1e-6f;
    CHECK(differs); // semillas distintas generan ruido distinto

    sv::Random rng;
    rng.seed(42);
    bool inRange = true;
    for (i32 i = 0; i < 20000; ++i) {
        const f32 x = rng.symmetric() * 100.0f, y = rng.symmetric() * 100.0f;
        const f32 v = a.perlin2(x, y);
        if (v < -1.05f || v > 1.05f) inRange = false;
    }
    CHECK(inRange);

    f32 mn = 1e9f, mx = -1e9f;
    for (i32 i = 0; i < 5000; ++i) {
        const f32 v = a.fbm2(rng.nextF32() * 40.0f, rng.nextF32() * 40.0f, 5);
        mn = std::min(mn, v); mx = std::max(mx, v);
    }
    CHECK(mn > -1.1f && mx < 1.1f);
    CHECK(mx > 0.05f && mn < -0.05f); // hay variacion real

    inRange = true;
    for (i32 i = 0; i < 5000; ++i) {
        const f32 v = a.ridged2(rng.nextF32() * 40.0f, rng.nextF32() * 40.0f, 4);
        if (v < -0.01f || v > 1.01f) inRange = false;
    }
    CHECK(inRange);

    CHECK_NEAR(a.perlin2(0.0f, 0.0f), 0.0f, 1e-6f); // puntos de reticula = 0
}

// ---------------------------------------------------------------------------
static void testMemory() {
    std::printf("[memory]\n");

    sv::LinearArena arena;
    arena.init(4096);
    void* p1 = arena.alloc(100, 16);
    void* p2 = arena.alloc(100, 16);
    CHECK(p1 && p2);
    CHECK(reinterpret_cast<usize>(p1) % 16 == 0);
    CHECK(reinterpret_cast<u8*>(p2) - reinterpret_cast<u8*>(p1) >= 100);
    CHECK(arena.used() >= 200);
    void* big = arena.alloc(8000, 16);
    CHECK(big == nullptr); // excede capacidad -> rechazo limpio
    arena.reset();
    CHECK(arena.used() == 0);

    sv::PoolAllocator pool;
    pool.init(64, 16);
    void* blocks[16];
    bool ok = true;
    for (i32 i = 0; i < 16; ++i) {
        blocks[i] = pool.obtain();
        if (!blocks[i]) ok = false;
    }
    CHECK(ok);
    CHECK(pool.obtain() == nullptr); // agotado
    pool.release(blocks[7]);
    CHECK(pool.obtain() == blocks[7]);
    CHECK(pool.live() == 16);

    sv::FrameArena frame;
    frame.init(1024);
    void* f1 = frame.alloc(128, 16);
    CHECK(f1 != nullptr);
    frame.flip();
    CHECK(frame.used() == 0); // buffer nuevo vacio
}

// ---------------------------------------------------------------------------
static void testJobs() {
    std::printf("[jobs]\n");

    auto& jobs = sv::JobSystem::get();
    jobs.init(2);
    CHECK(jobs.isRunning());

    std::atomic<u32> counter{0};
    struct Ctx { std::atomic<u32>* c; };
    Ctx ctx{&counter};
    jobs.parallelFor(10000, 64, [](void* user, u32) {
        static_cast<Ctx*>(user)->c->fetch_add(1);
    }, &ctx);
    CHECK(counter.load() == 10000);

    std::atomic<u32> counter2{0};
    jobs.parallelForLambda(5000, [&](u32) { counter2.fetch_add(1); });
    CHECK(counter2.load() == 5000);

    // suma de valores correcta (tickets sin duplicar ni perder)
    std::vector<std::atomic<i32>> hits(2048);
    for (auto& h : hits) h.store(0);
    jobs.parallelForLambda(2048, [&](u32 i) { hits[i].fetch_add(1); });
    bool exact = true;
    for (const auto& h : hits)
        if (h.load() != 1) exact = false;
    CHECK(exact);

    jobs.shutdown();
}

// ---------------------------------------------------------------------------
int main() {
    std::printf("=== SKYVAULT Engine - tests unitarios ===\n");
    testCore();
    testMath();
    testJson();
    testEcs();
    testNoise();
    testMemory();
    testJobs();
    std::printf("=== %d checks, %d fallos ===\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
