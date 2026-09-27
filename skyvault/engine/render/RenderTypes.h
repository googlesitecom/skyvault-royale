// ============================================================================
//  SKYVAULT Royale - engine/render/RenderTypes.h
//  Shader, Texture, Mesh (VAO/VBO/EBO), Framebuffer — envoltorios finos sobre
//  GL con el mismo codigo para escritorio y web (sin DSA).
// ============================================================================
#pragma once

#include "core/Core.h"
#include "render/GLLoader.h"
#include "math/Math.h"   // sv::vec3 / sv::mat4 (alias GLM)

namespace sv {

// ---------------------------------------------------------------------------
// Shader: GLSL 300 es compatible (mismo fuente compila como 460 core en
// escritorio inyectando el prelude adecuado).
// ---------------------------------------------------------------------------
class Shader {
public:
    struct Define { const char* name; i32 value; };

    [[nodiscard]] bool load(const char* vertSrc, const char* fragSrc,
                            const Define* defines = nullptr, usize defineCount = 0);
    void destroy();
    void use() const;
    [[nodiscard]] bool valid() const { return m_prog != 0; }

    void setU1i(const char* n, i32 v)             const;
    void setU1f(const char* n, f32 v)             const;
    void setU2f(const char* n, f32 x, f32 y)      const;
    void setU3f(const char* n, f32 x, f32 y, f32 z) const;
    void setU4f(const char* n, f32 x, f32 y, f32 z, f32 w) const;
    void setU3fv(const char* n, const f32* v, i32 count = 1)       const;
    void setU4fv(const char* n, const f32* v, i32 count = 1)       const;
    void setMat4(const char* n, const f32* m, i32 count = 1)       const;
    void setMat4(const char* n, const glm::mat4& m)                const;
    void setMat4v(const char* n, const f32* m, i32 count)          const;
    [[nodiscard]] i32 location(const char* n) const { return gl::glGetUniformLocation(m_prog, n); }

    u32 program() const { return m_prog; }
private:
    u32 m_prog = 0;
};

// ---------------------------------------------------------------------------
// Texture
// ---------------------------------------------------------------------------
enum class TexFormat : u32 { RGBA8, SRGB8, RGBA16F, Depth24, R8 };

struct Texture {
    u32 tex  = 0;
    i32 w    = 0, h = 0;
    TexFormat fmt = TexFormat::RGBA8;

    bool create2D(i32 width, i32 height, TexFormat format, const void* data = nullptr,
                  bool mips = false, bool clamp = true);
    // Sube datos RGBA8 desde memoria (stb_image) y genera mips
    bool uploadRGBA(i32 width, i32 height, const u8* data, bool mips = true);
    void destroy();
    void bind(u32 unit) const { gl::glActiveTexture(gl::GL_TEXTURE0 + unit); gl::glBindTexture(gl::GL_TEXTURE_2D, tex); }
};

// ---------------------------------------------------------------------------
// Mesh — geometria indexada estatica (VAO + VBO + EBO)
// ---------------------------------------------------------------------------
struct Mesh {
    u32 vao = 0, vbo = 0, ebo = 0;
    i32 indexCount = 0;
    bool hasTangent = false;   // formato de vertice

    void destroy();
    [[nodiscard]] bool valid() const { return vao != 0; }
};

// ---------------------------------------------------------------------------
// Framebuffer (color RGBA16F/RGBA8 + depth Depth24)
// ---------------------------------------------------------------------------
struct RenderTarget {
    u32 fbo = 0;
    Texture color;
    Texture depth;
    i32 w = 0, h = 0;

    bool create(i32 width, i32 height, bool hdr, bool withDepth = true);
    bool createDepthOnly(i32 width, i32 height);
    void destroy();
    void bind()     const { gl::glBindFramebuffer(gl::GL_FRAMEBUFFER, fbo); }
    void bindSize() const { gl::glViewport(0, 0, w, h); }
};

// ---------------------------------------------------------------------------
// Utilidades
// ---------------------------------------------------------------------------
void gpuCheckAndLog(const char* context);   // glGetError + warn
[[nodiscard]] i32 gpuMaxTextureSize();

} // namespace sv
