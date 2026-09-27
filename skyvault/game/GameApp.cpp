// ============================================================================
//  SKYVAULT Royale - game/GameApp.cpp
//  Punto de entrada: ventana, bucle, estados de captura de raton y glue web.
// ============================================================================
#include "game/Game.h"
#include "platform/Platform.h"
#include "render/Renderer.h"
#include "audio/Audio.h"
#include <cmath>

#ifdef SKYVAULT_WEB
#include <emscripten.h>
#endif

using namespace sv;
using namespace game;

#ifdef SKYVAULT_WEB
// Puentes de depuracion invocables desde JS (pruebas automaticas):
//   Module.ccall('svDebugLand')   -> aterrizaje inmediato
//   Module.ccall('svDebugState')  -> estado actual de la partida
extern "C" EMSCRIPTEN_KEEPALIVE void svDebugLand() {
    gameInstance().requestDebugLand();
}
extern "C" EMSCRIPTEN_KEEPALIVE int svDebugState() {
    return (int)gameInstance().state();
}
#endif

#ifdef SKYVAULT_WEB
// Puente de depuracion invocable desde JS (pruebas automaticas):
//   Module.ccall('svDebugLand')  -> aterrizaje inmediato desde el Carguero Nube
#endif

namespace {

Window   g_window;
FrameClock g_clock;
bool     g_running = true;
bool     g_glReady = false;

void tick() {
    if (!g_running) return;
    Renderer& r = Renderer::get();
    Game& g = gameInstance();
    InputState& in = input();

    g_clock.tick();
    const f32 dt = g_clock.dt;
    r.resize(g_window.width(), g_window.height());

    // captura de raton: en juego, capturado; en menus, libre
    const bool inMenuLike = g.state() == State::Menu || g.state() == State::Dead ||
                            g.state() == State::Victory;
    if (!inMenuLike && !in.mouseCaptured && g_window.handle()) {
        // recaptura automatica al hacer clic (requerido por los navegadores)
        if (in.mouseDownB(GLFW_MOUSE_BUTTON_LEFT) || in.mouseDownB(GLFW_MOUSE_BUTTON_RIGHT))
            g_window.captureMouse(true);
    }
    if (inMenuLike && in.mouseCaptured) g_window.captureMouse(false);
    if (in.keyPressed(GLFW_KEY_ESCAPE) && in.mouseCaptured) g_window.captureMouse(false);

    // clics de UI cuando el raton esta libre
    if (inMenuLike && in.mousePressedB(GLFW_MOUSE_BUTTON_LEFT)) {
        // posicion en pantalla (UI usa coordenadas de framebuffer)
        g.clickUI(in.mouseX, in.mouseY);
    }

    g.tick(std::min(dt, 0.05f));
    if (g.state() != State::Boot && g_glReady) g.render();

    // log de estado cada 600 frames (diagnostico ligero)
    static u32 dbgFrame = 0;
    if (++dbgFrame % 600 == 1) {
        SV_LOG_INFO("dbg", "frame %u: %.1f fps | estado=%d pos=(%.0f,%.0f) alt=%.0f",
                    (u32)dbgFrame, g_clock.fps(), (i32)g.state(),
                    gameInstance().debugPos().x, gameInstance().debugPos().z,
                    gameInstance().debugPos().y);
    }

    g_window.swap();
    if (g_window.shouldClose()) {
#ifdef SKYVAULT_WEB
        // el navegador controla el ciclo; nada que hacer
#else
        g_running = false;
#endif
    }
}

void shutdown() {
    gameInstance().shutdown();
    Renderer::get().shutdown();
    audioSystem().shutdown();
    g_window.shutdown();
    SV_LOG_INFO("app", "SKYVAULT Royale cerrado");
}

} // namespace

int main() {
    SV_LOG_INFO("app", "SKYVAULT Royale %s - compilado %s %s",
                "v0.1 preview", __DATE__, __TIME__);
    SV_LOG_INFO("app", "Plataforma: %s", platformName());

    WindowDesc desc;
    desc.title = "SKYVAULT Royale - La Bóveda (preview)";
    desc.width = 1600;
    desc.height = 900;
    if (!g_window.init(desc)) return 1;

#ifdef SKYVAULT_WEB
    // WebGL2: pide EXT_color_buffer_float para FBOs RGBA16F (HDR)
    extern void svEnableWebGLExtensions();
    svEnableWebGLExtensions();
#endif

    if (!gl::loadAll()) { shutdown(); return 1; }

    Renderer& r = Renderer::get();
    if (!r.init(g_window.width(), g_window.height())) { shutdown(); return 1; }
    g_glReady = true;

    if (!audioSystem().init())
        SV_LOG_WARN("app", "Audio no disponible (continuando sin sonido)");

    Game& g = gameInstance();
    if (!g.init(&r)) { shutdown(); return 1; }
    g.loadStatsPublic();

    g_clock.tick();   // primer delta
    runMainLoop(tick, shutdown);
    return 0;
}
