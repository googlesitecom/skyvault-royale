// ============================================================================
//  SKYVAULT Royale - engine/platform/Platform.cpp
// ============================================================================
#include "platform/Platform.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifdef SKYVAULT_WEB
  #include <emscripten.h>
#else
  #include <thread>
  #include <chrono>
#endif

namespace sv {

static Window*   g_windowForCb   = nullptr;
static TickFn    g_tickFn        = nullptr;
static ShutdownFn g_shutdownFn   = nullptr;

// ---------------------------------------------------------------------------
// Callbacks GLFW
// ---------------------------------------------------------------------------
static void cbKey(GLFWwindow*, i32 key, i32, i32 action, i32) {
    if (key < 0 || key >= (i32)InputState::MaxKeys) return;
    InputState& in = input();
    if (action == GLFW_PRESS || action == GLFW_REPEAT) { in.down[key] = true; in.pressed[key] = true; }
    else if (action == GLFW_RELEASE)                  { in.down[key] = false; in.released[key] = true; }
}
static void cbMouseButton(GLFWwindow*, i32 button, i32 action, i32) {
    if (button < 0 || button >= (i32)InputState::MaxMouse) return;
    InputState& in = input();
    if (action == GLFW_PRESS)   { in.mouseDown[button] = true;  in.mousePressed[button] = true; }
    else if (action == GLFW_RELEASE) { in.mouseDown[button] = false; in.mouseReleased[button] = true; }
}
static void cbCursorPos(GLFWwindow*, f64 x, f64 y) {
    InputState& in = input();
    in.mouseDX += static_cast<f32>(x) - in.mouseX;
    in.mouseDY += static_cast<f32>(y) - in.mouseY;
    in.mouseX = static_cast<f32>(x);
    in.mouseY = static_cast<f32>(y);
}
static void cbScroll(GLFWwindow*, f64, f64 dy) { input().wheel += static_cast<f32>(dy); }
static void cbFramebufferSize(GLFWwindow*, i32 w, i32 h) {
    if (g_windowForCb) g_windowForCb->onResize(w, h);
}
static void cbError(int code, const char* desc) {
    SV_LOG_ERROR("platform", "GLFW error %d: %s", code, desc ? desc : "?");
}

// ---------------------------------------------------------------------------
// Window
// ---------------------------------------------------------------------------
bool Window::init(const WindowDesc& desc) {
    glfwSetErrorCallback(cbError);
    if (!glfwInit()) {
        SV_LOG_ERROR("platform", "glfwInit() fallo");
        return false;
    }
#ifdef SKYVAULT_WEB
    // WebGL2 (GLES3). GLFW-emsdf usa estos hints al crear el contexto.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#else
    // OpenGL 4.6 core
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
    glfwWindowHint(GLFW_RESIZABLE, desc.resizable ? GLFW_TRUE : GLFW_FALSE);

    m_width = desc.width; m_height = desc.height;
    GLFWwindow* w = glfwCreateWindow(desc.width, desc.height, desc.title, nullptr, nullptr);
    if (!w) {
        SV_LOG_ERROR("platform", "glfwCreateWindow fallo (sin servidor grafico o sin WebGL2)");
        glfwTerminate();
        return false;
    }
    m_handle = w;
    glfwSetWindowUserPointer(w, this);
    glfwSetKeyCallback(w, cbKey);
    glfwSetMouseButtonCallback(w, cbMouseButton);
    glfwSetCursorPosCallback(w, cbCursorPos);
    glfwSetScrollCallback(w, cbScroll);
    glfwSetFramebufferSizeCallback(w, cbFramebufferSize);
    glfwMakeContextCurrent(w);
    glfwSwapInterval(desc.vsync ? 1 : 0);

    // Tamano real del framebuffer (HiDPI / canvas CSS)
    glfwGetFramebufferSize(w, &m_width, &m_height);
    glfwGetCursorPos(w, nullptr, nullptr);
    InputState& in = input();
    f64 x, y; glfwGetCursorPos(w, &x, &y);
    in.mouseX = static_cast<f32>(x); in.mouseY = static_cast<f32>(y);

    g_windowForCb = this;
    SV_LOG_INFO("platform", "Ventana %dx%d creada (%s)", m_width, m_height, platformName());
    return true;
}

void Window::shutdown() {
    if (m_handle) { glfwDestroyWindow(m_handle); m_handle = nullptr; }
    glfwTerminate();
}

bool Window::shouldClose() const { return glfwWindowShouldClose(m_handle) != 0; }

void Window::swap() {
    glfwSwapBuffers(m_handle);
    glfwPollEvents();
}

void Window::captureMouse(bool on) {
    InputState& in = input();
    in.mouseCaptured = on;
    glfwSetInputMode(m_handle, GLFW_CURSOR, on ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (!on) {
        // al soltar, recentrar para evitar un delta gigante en la proxima captura
        f64 x, y; glfwGetCursorPos(m_handle, &x, &y);
        in.mouseX = static_cast<f32>(x); in.mouseY = static_cast<f32>(y);
        in.mouseDX = in.mouseDY = 0;
    }
}

void Window::setTitle(const char* title) { glfwSetWindowTitle(m_handle, title); }

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
static InputState g_input;
InputState& input() { return g_input; }

void InputState::endFrame() {
    for (u32 i = 0; i < MaxKeys; ++i)  { pressed[i] = false; released[i] = false; }
    for (u32 i = 0; i < MaxMouse; ++i) { mousePressed[i] = false; mouseReleased[i] = false; }
    mouseDX = mouseDY = 0;
    wheel = 0;
}

// ---------------------------------------------------------------------------
// Bucle principal
// ---------------------------------------------------------------------------
#ifdef SKYVAULT_WEB
static void webTick() {
    g_tickFn();
    input().endFrame();
}
#else
static void desktopLoop() {
    using clock = std::chrono::steady_clock;
    while (!g_windowForCb || !g_windowForCb->shouldClose()) {
        g_tickFn();
        input().endFrame();
    }
}
#endif

void runMainLoop(TickFn tick, ShutdownFn onShutdown) {
    g_tickFn = tick;
    g_shutdownFn = onShutdown;
#ifdef SKYVAULT_WEB
    // El navegador manda: 60 fps via requestAnimationFrame.
    emscripten_set_main_loop(webTick, 0, 1 /*simulate_infinite_loop*/);
    if (g_shutdownFn) g_shutdownFn();
#else
    desktopLoop();
    if (g_shutdownFn) g_shutdownFn();
#endif
}

const char* platformName() {
#ifdef SKYVAULT_WEB
    return "Web (WebAssembly + WebGL2)";
#elif defined(_WIN32)
    return "Windows (OpenGL 4.6)";
#else
    return "Linux (OpenGL 4.6)";
#endif
}

void openUrl(const char*) { /* escritorio: el launcher del SO decide; web: ver Platform_web */ }

} // namespace sv

// ---------------------------------------------------------------------------
// WebGL2: extensiones requeridas por el renderer (HDR FBOs)
// ---------------------------------------------------------------------------
#ifdef SKYVAULT_WEB
#include <emscripten/html5_webgl.h>
void svEnableWebGLExtensions() {
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_get_current_context();
    if (ctx) {
        emscripten_webgl_enable_extension(ctx, "EXT_color_buffer_float");
        SV_LOG_INFO("platform", "Extension WebGL EXT_color_buffer_float solicitada");
    }
}
#endif
