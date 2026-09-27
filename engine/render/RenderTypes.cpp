// ============================================================================
//  SKYVAULT Royale - engine/render/RenderTypes.cpp
// ============================================================================
#include "render/RenderTypes.h"

namespace sv {

// ---------------------------------------------------------------------------
// Shader
// ---------------------------------------------------------------------------
static bool compileStage(u32 type, const char* src, const Shader::Define* defs,
                         usize defCount, u32& out) {
    using namespace gl;
    out = glCreateShader(type);

    // Prelude por plataforma + defines
    std::string prelude;
#ifdef SKYVAULT_WEB
    prelude = "#version 300 es\nprecision highp float;\nprecision highp int;\n";
#else
    prelude = "#version 460 core\n";
#endif
    for (usize i = 0; i < defCount; ++i)
        prelude += sv::format("#define %s %d\n", defs[i].name, defs[i].value);

    const char* sources[2] = { prelude.c_str(), src };
    const i32 lengths[2]   = { static_cast<i32>(prelude.size()), static_cast<i32>(std::strlen(src)) };
    glShaderSource(out, 2, sources, lengths);
    glCompileShader(out);
    i32 ok = 0;
    glGetShaderiv(out, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        i32 len = 0;
        glGetShaderInfoLog(out, sizeof(log), &len, log);
        SV_LOG_ERROR("shader", "Error compilando %s:\n%s",
                     type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        glDeleteShader(out);
        out = 0;
        return false;
    }
    return true;
}

bool Shader::load(const char* vertSrc, const char* fragSrc,
                  const Define* defines, usize defineCount) {
    using namespace gl;
    u32 vs = 0, fs = 0;
    if (!compileStage(GL_VERTEX_SHADER, vertSrc, defines, defineCount, vs)) return false;
    if (!compileStage(GL_FRAGMENT_SHADER, fragSrc, defines, defineCount, fs)) {
        glDeleteShader(vs);
        return false;
    }
    m_prog = glCreateProgram();
    glAttachShader(m_prog, vs);
    glAttachShader(m_prog, fs);
    // Ubicaciones de atributos fijas (compatible ES3 sin layout qualifiers).
    // OJO: aInstance (mat4) consume 4 ubicaciones consecutivas (7,8,9,10).
    glBindAttribLocation(m_prog, 0, "aPos");
    glBindAttribLocation(m_prog, 1, "aNormal");
    glBindAttribLocation(m_prog, 2, "aUV");
    glBindAttribLocation(m_prog, 3, "aTangent");
    glBindAttribLocation(m_prog, 4, "aColor");
    glBindAttribLocation(m_prog, 5, "aJoint");
    glBindAttribLocation(m_prog, 6, "aWeight");
    glBindAttribLocation(m_prog, 7, "aInstance");
    glBindAttribLocation(m_prog, 11, "aInstanceColor");
    glLinkProgram(m_prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    i32 ok = 0;
    glGetProgramiv(m_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        i32 len = 0;
        glGetProgramInfoLog(m_prog, sizeof(log), &len, log);
        SV_LOG_ERROR("shader", "Error enlazando programa:\n%s", log);
        glDeleteProgram(m_prog);
        m_prog = 0;
        return false;
    }
    return true;
}

void Shader::destroy() {
    if (m_prog) { gl::glDeleteProgram(m_prog); m_prog = 0; }
}
void Shader::use() const { gl::glUseProgram(m_prog); }

void Shader::setU1i(const char* n, i32 v) const { gl::glUniform1i(location(n), v); }
void Shader::setU1f(const char* n, f32 v) const { gl::glUniform1f(location(n), v); }
void Shader::setU2f(const char* n, f32 x, f32 y) const { gl::glUniform2f(location(n), x, y); }
void Shader::setU3f(const char* n, f32 x, f32 y, f32 z) const { gl::glUniform3f(location(n), x, y, z); }
void Shader::setU4f(const char* n, f32 x, f32 y, f32 z, f32 w) const { gl::glUniform4f(location(n), x, y, z, w); }
void Shader::setU3fv(const char* n, const f32* v, i32 count) const { gl::glUniform3fv(location(n), count, v); }
void Shader::setU4fv(const char* n, const f32* v, i32 count) const { gl::glUniform4fv(location(n), count, v); }
void Shader::setMat4(const char* n, const f32* m, i32 count) const { gl::glUniformMatrix4fv(location(n), count, gl::GL_FALSE, m); }
void Shader::setMat4(const char* n, const glm::mat4& m) const { setMat4(n, glm::value_ptr(m)); }
void Shader::setMat4v(const char* n, const f32* m, i32 count) const { setMat4(n, m, count); }

// ---------------------------------------------------------------------------
// Texture
// ---------------------------------------------------------------------------
bool Texture::create2D(i32 width, i32 height, TexFormat format, const void* data,
                       bool mips, bool clamp) {
    using namespace gl;
    w = width; h = height; fmt = format;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    const u32 wrap = clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (i32)wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (i32)wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (i32)GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    (i32)(mips ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));

    i32 internal = GL_RGBA8; u32 pixFmt = GL_RGBA, pixType = GL_UNSIGNED_BYTE;
    switch (format) {
        case TexFormat::RGBA8:  internal = GL_RGBA8;  pixFmt = GL_RGBA; pixType = GL_UNSIGNED_BYTE; break;
        case TexFormat::SRGB8:  internal = 0x8C43 /*GL_SRGB8_ALPHA8*/; pixFmt = GL_RGBA; pixType = GL_UNSIGNED_BYTE; break;
        case TexFormat::RGBA16F:internal = GL_RGBA16F;pixFmt = GL_RGBA; pixType = GL_HALF_FLOAT;    break;
        case TexFormat::Depth24:internal = (i32)GL_DEPTH_COMPONENT24; pixFmt = GL_DEPTH_COMPONENT; pixType = GL_UNSIGNED_INT; break;
        case TexFormat::R8:     internal = GL_R8;     pixFmt = GL_RED;  pixType = GL_UNSIGNED_BYTE; break;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, pixFmt, pixType, data);
    if (mips) glGenerateMipmap(GL_TEXTURE_2D);
    return true;
}

bool Texture::uploadRGBA(i32 width, i32 height, const u8* data, bool mips) {
    using namespace gl;
    if (!tex) { create2D(width, height, TexFormat::RGBA8, data, mips, false); return true; }
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    if (mips) glGenerateMipmap(GL_TEXTURE_2D);
    return true;
}

void Texture::destroy() {
    if (tex) { gl::glDeleteTextures(1, &tex); tex = 0; }
}

// ---------------------------------------------------------------------------
// Mesh
// ---------------------------------------------------------------------------
void Mesh::destroy() {
    using namespace gl;
    if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
    if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (ebo) { glDeleteBuffers(1, &ebo); ebo = 0; }
    indexCount = 0;
}

// ---------------------------------------------------------------------------
// RenderTarget
// ---------------------------------------------------------------------------
bool RenderTarget::create(i32 width, i32 height, bool hdr, bool withDepth) {
    using namespace gl;
    w = width; h = height;
    color.create2D(w, h, hdr ? TexFormat::RGBA16F : TexFormat::RGBA8, nullptr, false, true);
    if (withDepth) depth.create2D(w, h, TexFormat::Depth24, nullptr, false, true);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color.tex, 0);
    if (withDepth)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth.tex, 0);
    const u32 status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        SV_LOG_ERROR("rt", "FBO %dx%d incompleto (hdr=%d)", w, h, (i32)hdr);
        destroy();
        return false;
    }
    return true;
}

bool RenderTarget::createDepthOnly(i32 width, i32 height) {
    using namespace gl;
    w = width; h = height;
    depth.create2D(w, h, TexFormat::Depth24, nullptr, false, true);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth.tex, 0);
    const u32 status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        SV_LOG_ERROR("rt", "FBO de profundidad %dx%d incompleto", w, h);
        destroy();
        return false;
    }
    return true;
}

void RenderTarget::destroy() {
    using namespace gl;
    if (fbo)    { glDeleteFramebuffers(1, &fbo); fbo = 0; }
    color.destroy();
    depth.destroy();
    w = h = 0;
}

// ---------------------------------------------------------------------------
// Utilidades
// ---------------------------------------------------------------------------
void gpuCheckAndLog(const char* context) {
    const u32 err = gl::glGetError();
    if (err != 0) SV_LOG_WARN("gl", "GL error 0x%04X en %s", err, context);
}

i32 gpuMaxTextureSize() {
    i32 v = 2048;
    gl::glGetIntegerv(gl::GL_MAX_TEXTURE_SIZE, &v);
    return v;
}

} // namespace sv
