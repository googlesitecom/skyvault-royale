// ============================================================================
//  SKYVAULT Royale - engine/render/GLLoader.h
//  Cargador de funciones OpenGL PROPIO (sin GLEW / GLAD / glbinding).
//    - Escritorio: resuelve punteros via glfwGetProcAddress (wgl/glx)
//    - Web: Emscripten expone GLES3 (WebGL2) directamente en el header
//  El motor solo usa el subconjunto comun de OpenGL 4.6 core / GLES3,
//  con llamadas no-DSA para que el mismo codigo funcione en ambos.
//
//  En web, los enums GL_* son MACROS del sistema GLES3 que romperian el
//  prefijo sv::gl::GL_* al expandirse; por eso se #undef y se redefinen como
//  constantes reales compartidas por ambas plataformas.
// ============================================================================
#pragma once

#include "core/Core.h"

#ifdef SKYVAULT_WEB
  #include <GLES3/gl3.h>
  #undef GL_FALSE
  #undef GL_TRUE
  #undef GL_ZERO
  #undef GL_ONE
  #undef GL_SRC_COLOR
  #undef GL_ONE_MINUS_SRC_COLOR
  #undef GL_SRC_ALPHA
  #undef GL_ONE_MINUS_SRC_ALPHA
  #undef GL_FRONT
  #undef GL_BACK
  #undef GL_FRONT_AND_BACK
  #undef GL_NEVER
  #undef GL_LESS
  #undef GL_EQUAL
  #undef GL_LEQUAL
  #undef GL_GREATER
  #undef GL_CULL_FACE
  #undef GL_BLEND
  #undef GL_DEPTH_TEST
  #undef GL_SCISSOR_TEST
  #undef GL_POLYGON_OFFSET_FILL
  #undef GL_COLOR_BUFFER_BIT
  #undef GL_DEPTH_BUFFER_BIT
  #undef GL_POINTS
  #undef GL_LINES
  #undef GL_LINE_STRIP
  #undef GL_TRIANGLES
  #undef GL_TRIANGLE_STRIP
  #undef GL_UNSIGNED_BYTE
  #undef GL_UNSIGNED_SHORT
  #undef GL_UNSIGNED_INT
  #undef GL_FLOAT
  #undef GL_NEAREST
  #undef GL_LINEAR
  #undef GL_NEAREST_MIPMAP_NEAREST
  #undef GL_LINEAR_MIPMAP_NEAREST
  #undef GL_NEAREST_MIPMAP_LINEAR
  #undef GL_LINEAR_MIPMAP_LINEAR
  #undef GL_TEXTURE_MAG_FILTER
  #undef GL_TEXTURE_MIN_FILTER
  #undef GL_TEXTURE_WRAP_S
  #undef GL_TEXTURE_WRAP_T
  #undef GL_TEXTURE_WRAP_R
  #undef GL_CLAMP_TO_EDGE
  #undef GL_REPEAT
  #undef GL_MIRRORED_REPEAT
  #undef GL_TEXTURE_2D
  #undef GL_RGB
  #undef GL_RGBA
  #undef GL_RED
  #undef GL_RG
  #undef GL_RGBA8
  #undef GL_RGBA16F
  #undef GL_R8
  #undef GL_DEPTH_COMPONENT
  #undef GL_DEPTH_COMPONENT24
  #undef GL_HALF_FLOAT
  #undef GL_TEXTURE0
  #undef GL_TEXTURE1
  #undef GL_TEXTURE2
  #undef GL_TEXTURE3
  #undef GL_TEXTURE4
  #undef GL_TEXTURE5
  #undef GL_TEXTURE6
  #undef GL_TEXTURE7
  #undef GL_CW
  #undef GL_CCW
  #undef GL_ARRAY_BUFFER
  #undef GL_ELEMENT_ARRAY_BUFFER
  #undef GL_STATIC_DRAW
  #undef GL_DYNAMIC_DRAW
  #undef GL_VERTEX_SHADER
  #undef GL_FRAGMENT_SHADER
  #undef GL_COMPILE_STATUS
  #undef GL_LINK_STATUS
  #undef GL_INFO_LOG_LENGTH
  #undef GL_DEPTH_ATTACHMENT
  #undef GL_COLOR_ATTACHMENT0
  #undef GL_FRAMEBUFFER
  #undef GL_FRAMEBUFFER_COMPLETE
  #undef GL_FUNC_ADD
  #undef GL_MAX_TEXTURE_SIZE
  #undef GL_UNPACK_ALIGNMENT
  #undef GL_VERSION
  #undef GL_RENDERER
  #undef GL_VENDOR
  #undef GL_UNSIGNED_INT_24_8
#endif

namespace sv { namespace gl {

#ifdef SKYVAULT_WEB
  // ----------------------------------------------------------------------
  // Web (WebGL2 / GLES3): funciones ya declaradas por el sistema a nivel
  // global; se importan al namespace sv::gl para unificar el acceso.
  // ----------------------------------------------------------------------
  #define SVGL(ret, name, ...) using ::name;
  #include "render/GLFuncList.h"
  #undef SVGL
  inline bool loadAll() { return true; }
#else
  // ----------------------------------------------------------------------
  // Escritorio: punteros resueltos en runtime. La lista vive en GLFuncList.h
  // ----------------------------------------------------------------------
  #ifndef GLAPIENTRY
    #ifdef _WIN32
      #define GLAPIENTRY __stdcall
    #else
      #define GLAPIENTRY
    #endif
  #endif
  #define SVGL(ret, name, ...) \
      typedef ret (GLAPIENTRY *PFN_sv_##name)(__VA_ARGS__); \
      extern PFN_sv_##name name;

  #include "render/GLFuncList.h"

  #undef SVGL

  [[nodiscard]] bool loadAll();   // llama despues de crear el contexto
#endif

  // --- Constantes GL compartidas (valores fijos del registro OpenGL) ---------
  constexpr u32 GL_FALSE = 0, GL_TRUE = 1;
  constexpr u32 GL_ZERO = 0, GL_ONE = 1;
  constexpr u32 GL_SRC_COLOR = 0x0300, GL_ONE_MINUS_SRC_COLOR = 0x0301;
  constexpr u32 GL_SRC_ALPHA = 0x0302, GL_ONE_MINUS_SRC_ALPHA = 0x0303;
  constexpr u32 GL_FRONT = 0x0404, GL_BACK = 0x0405, GL_FRONT_AND_BACK = 0x0408;
  constexpr u32 GL_NEVER = 0x0200, GL_LESS = 0x0201, GL_EQUAL = 0x0202, GL_LEQUAL = 0x0203, GL_GREATER = 0x0204;
  constexpr u32 GL_CULL_FACE = 0x0B44, GL_BLEND = 0x0BE2, GL_DEPTH_TEST = 0x0B71;
  constexpr u32 GL_SCISSOR_TEST = 0x0C11, GL_POLYGON_OFFSET_FILL = 0x8037;
  constexpr u32 GL_COLOR_BUFFER_BIT = 0x00004000, GL_DEPTH_BUFFER_BIT = 0x00000100;
  constexpr u32 GL_POINTS = 0, GL_LINES = 1, GL_LINE_STRIP = 3, GL_TRIANGLES = 4, GL_TRIANGLE_STRIP = 5;
  constexpr u32 GL_UNSIGNED_BYTE = 0x1401, GL_UNSIGNED_SHORT = 0x1403, GL_UNSIGNED_INT = 0x1405, GL_FLOAT = 0x1406;
  constexpr u32 GL_NEAREST = 0x2600, GL_LINEAR = 0x2601;
  constexpr u32 GL_NEAREST_MIPMAP_NEAREST = 0x2700, GL_LINEAR_MIPMAP_NEAREST = 0x2701;
  constexpr u32 GL_NEAREST_MIPMAP_LINEAR = 0x2702, GL_LINEAR_MIPMAP_LINEAR = 0x2703;
  constexpr u32 GL_TEXTURE_MAG_FILTER = 0x2800, GL_TEXTURE_MIN_FILTER = 0x2801;
  constexpr u32 GL_TEXTURE_WRAP_S = 0x2802, GL_TEXTURE_WRAP_T = 0x2803, GL_TEXTURE_WRAP_R = 0x8072;
  constexpr u32 GL_CLAMP_TO_EDGE = 0x812F, GL_REPEAT = 0x2901, GL_MIRRORED_REPEAT = 0x8370;
  constexpr u32 GL_TEXTURE_2D = 0x0DE1;
  constexpr u32 GL_RGB = 0x1907, GL_RGBA = 0x1908, GL_RED = 0x1903, GL_RG = 0x8227;
  constexpr u32 GL_RGBA8 = 0x8058, GL_RGBA16F = 0x881A, GL_R8 = 0x8229;
  constexpr u32 GL_DEPTH_COMPONENT = 0x1902, GL_DEPTH_COMPONENT24 = 0x81A6;
  constexpr u32 GL_HALF_FLOAT = 0x140B;
  constexpr u32 GL_TEXTURE0 = 0x84C0, GL_TEXTURE1 = 0x84C1, GL_TEXTURE2 = 0x84C2;
  constexpr u32 GL_TEXTURE3 = 0x84C3, GL_TEXTURE4 = 0x84C4, GL_TEXTURE5 = 0x84C5;
  constexpr u32 GL_TEXTURE6 = 0x84C6, GL_TEXTURE7 = 0x84C7;
  constexpr u32 GL_CW = 0x0900, GL_CCW = 0x0901;
  constexpr u32 GL_ARRAY_BUFFER = 0x8892, GL_ELEMENT_ARRAY_BUFFER = 0x8893;
  constexpr u32 GL_STATIC_DRAW = 0x88E4, GL_DYNAMIC_DRAW = 0x88E8;
  constexpr u32 GL_VERTEX_SHADER = 0x8B31, GL_FRAGMENT_SHADER = 0x8B30;
  constexpr u32 GL_COMPILE_STATUS = 0x8B81, GL_LINK_STATUS = 0x8B82, GL_INFO_LOG_LENGTH = 0x8B84;
  constexpr u32 GL_DEPTH_ATTACHMENT = 0x8D00, GL_COLOR_ATTACHMENT0 = 0x8CE0;
  constexpr u32 GL_FRAMEBUFFER = 0x8D40, GL_FRAMEBUFFER_COMPLETE = 0x8CD5;
  constexpr u32 GL_FUNC_ADD = 0x8006;
  constexpr u32 GL_MAX_TEXTURE_SIZE = 0x0D33, GL_UNPACK_ALIGNMENT = 0x0CF5;
  constexpr u32 GL_VERSION = 0x1F02, GL_RENDERER = 0x1F01, GL_VENDOR = 0x1F00;
  constexpr u32 GL_UNSIGNED_INT_24_8 = 0x84FA;

}} // namespace sv::gl
