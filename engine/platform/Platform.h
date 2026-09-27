// ============================================================================
//  SKYVAULT Royale - engine/platform/Platform.h
//  Ventana (GLFW 3.4), entrada y bucle principal unificados:
//    - Escritorio (Linux/Windows): contexto OpenGL 4.6 core
//    - Web (Emscripten/WebAssembly): contexto WebGL2 sobre el canvas
//  El resto del motor ve la misma API en las tres plataformas.
// ============================================================================
#pragma once

#include "core/Core.h"

struct GLFWwindow;

namespace sv {

// ---------------------------------------------------------------------------
// Ventana + contexto GL
// ---------------------------------------------------------------------------
struct WindowDesc {
    const char* title   = "SKYVAULT Royale";
    i32 width           = 1600;
    i32 height          = 900;
    bool resizable      = true;
    bool vsync          = true;
    u32 msaa            = 0;     // 0 = sin MSAA (usamos FXAA en post)
};

class Window {
public:
    [[nodiscard]] bool init(const WindowDesc& desc);
    void shutdown();
    [[nodiscard]] bool shouldClose() const;
    void swap();                       // swap + poll events
    [[nodiscard]] i32 width()    const { return m_width; }
    [[nodiscard]] i32 height()   const { return m_height; }
    [[nodiscard]] f32 aspect()   const { return static_cast<f32>(m_width) / (m_height > 0 ? static_cast<f32>(m_height) : 1.0f); }
    [[nodiscard]] GLFWwindow* handle() const { return m_handle; }
    void captureMouse(bool on);        // pointer lock (escritorio y web)
    void setTitle(const char* title);
    void onResize(i32 w, i32 h) { m_width = w; m_height = h; }  // callback interno
private:
    GLFWwindow* m_handle = nullptr;
    i32 m_width = 1600, m_height = 900;
};

// ---------------------------------------------------------------------------
// Entrada (teclado + raton). GLFW codes (GLFW_KEY_W, GLFW_MOUSE_BUTTON_LEFT)
// ---------------------------------------------------------------------------
struct InputState {
    static constexpr u32 MaxKeys  = 400;
    static constexpr u32 MaxMouse = 12;

    bool down[MaxKeys]      = {};
    bool pressed[MaxKeys]   = {};   // true solo el frame en que se pulsa
    bool released[MaxKeys]  = {};
    bool mouseDown[MaxMouse]     = {};
    bool mousePressed[MaxMouse]  = {};
    bool mouseReleased[MaxMouse] = {};

    f32 mouseX = 0, mouseY = 0;        // posicion acumulada (virtual con pointer lock)
    f32 mouseDX = 0, mouseDY = 0;      // delta por frame
    f32 wheel = 0;
    f32 sensitivity = 0.0022f;
    bool mouseCaptured = false;
    f32 mouseSensMult = 1.0f;

    [[nodiscard]] bool keyDown(i32 k)      const { return k >= 0 && k < (i32)MaxKeys && down[k]; }
    [[nodiscard]] bool keyPressed(i32 k)   const { return k >= 0 && k < (i32)MaxKeys && pressed[k]; }
    [[nodiscard]] bool keyReleased(i32 k)  const { return k >= 0 && k < (i32)MaxKeys && released[k]; }
    [[nodiscard]] bool mouseDownB(i32 b)     const { return b >= 0 && b < (i32)MaxMouse && mouseDown[b]; }
    [[nodiscard]] bool mousePressedB(i32 b)  const { return b >= 0 && b < (i32)MaxMouse && mousePressed[b]; }

    void endFrame();                    // limpia pressed/released/deltas
};

InputState& input();

// ---------------------------------------------------------------------------
// Bucle principal: en escritorio es un while clasico; en web registra el
// callback de requestAnimationFrame (el navegador es el dueno del bucle).
// ---------------------------------------------------------------------------
using TickFn     = void (*)();
using ShutdownFn = void (*)();
void runMainLoop(TickFn tick, ShutdownFn onShutdown = nullptr);

// Utilidades web/escritorio
[[nodiscard]] const char* platformName();
void openUrl(const char* url);          // "jugar de nuevo" etc (no-op escritorio)

} // namespace sv
