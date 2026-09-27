// ============================================================================
//  SKYVAULT Royale - engine/render/GLLoader.cpp  (solo escritorio)
// ============================================================================
#include "render/GLLoader.h"

#ifndef SKYVAULT_WEB

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace sv { namespace gl {

// Definicion de los punteros globales
#define SVGL(ret, name, ...) PFN_sv_##name name = nullptr;
#include "render/GLFuncList.h"
#undef SVGL

bool loadAll() {
    if (!glfwGetCurrentContext()) {
        SV_LOG_ERROR("gl", "loadAll(): no hay contexto GL activo");
        return false;
    }
    i32 missing = 0;
#define SVGL_LOAD(name) \
    if ((name = reinterpret_cast<PFN_sv_##name>(glfwGetProcAddress(#name))) == nullptr) { \
        ++missing; SV_LOG_WARN("gl", "No se resolvio: %s", #name); \
    }
#define SVGL(ret, name, ...) SVGL_LOAD(name)
#include "render/GLFuncList.h"
#undef SVGL
#undef SVGL_LOAD

    if (missing > 0) {
        SV_LOG_ERROR("gl", "%d funciones GL sin resolver (driver sin soporte 4.6 core?)", missing);
        return false;
    }
    SV_LOG_INFO("gl", "GL: %s | %s",
                reinterpret_cast<const char*>(glGetString(GL_VERSION)),
                reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    return true;
}

}} // namespace sv::gl
#endif
