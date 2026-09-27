// ============================================================================
//  SkyVault Engine - Nucleo: tipos, logging, tiempo, RNG, filesystem, config
//  Motor 100% original escrito para SKYVAULT Royale (C++20 / OpenGL 4.6 core)
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace sv {

// ---------------------------------------------------------------------------
// Tipos fundamentales
// ---------------------------------------------------------------------------
using i8  = int8_t;   using u8  = uint8_t;
using i16 = int16_t;  using u16 = uint16_t;
using i32 = int32_t;  using u32 = uint32_t;
using i64 = int64_t;  using u64 = uint64_t;
using f32 = float;    using f64 = double;
using usize = std::size_t;
using isize = std::ptrdiff_t;

constexpr f32 PI_F  = 3.14159265358979f;
constexpr f32 TAU_F = 6.28318530717959f;
constexpr f32 EPS_F = 1e-6f;
constexpr f32 INF_F = 3.402823466e+38f;

[[nodiscard]] constexpr f32 degToRad(f32 d) { return d * (PI_F / 180.0f); }
[[nodiscard]] constexpr f32 radToDeg(f32 r) { return r * (180.0f / PI_F); }

// ---------------------------------------------------------------------------
// String IDs (hash FNV-1a 32, evaluable en tiempo de compilacion)
// ---------------------------------------------------------------------------
using StringId = u32;

[[nodiscard]] constexpr u32 fnv1a32(const char* s, usize n) {
    u32 h = 0x811c9dc5u;
    for (usize i = 0; i < n; ++i) { h ^= static_cast<u32>(static_cast<u8>(s[i])); h *= 0x01000193u; }
    return h ? u32(h) : 1u; // evita id 0 (= invalido)
}
[[nodiscard]] constexpr StringId sid(std::string_view s) { return fnv1a32(s.data(), s.size()); }
[[nodiscard]] inline StringId sidRuntime(std::string_view s) { return fnv1a32(s.data(), s.size()); }

[[nodiscard]] constexpr u32 hashCombine(u32 a, u32 b) {
    return a ^ (b + 0x9e3779b9u + (a << 6) + (a >> 2));
}

// ---------------------------------------------------------------------------
// Logging (thread-safe, printf-style, colores ANSI si TTY)
// ---------------------------------------------------------------------------
namespace log {
    enum class Level : i32 { Debug = 0, Info, Warn, Error, Off };

    void setLevel(Level lvl);
    void setColors(bool enabled);
    void message(Level lvl, const char* channel, const char* file, int line, const char* fmt, ...)
        __attribute__((format(printf, 5, 6)));
}

#define SV_LOG(lvl, ch, ...) ::sv::log::message(lvl, ch, __FILE__, __LINE__, __VA_ARGS__)
#define SV_LOG_DEBUG(ch, ...) SV_LOG(::sv::log::Level::Debug,  ch, __VA_ARGS__)
#define SV_LOG_INFO(ch, ...)  SV_LOG(::sv::log::Level::Info,   ch, __VA_ARGS__)
#define SV_LOG_WARN(ch, ...)  SV_LOG(::sv::log::Level::Warn,   ch, __VA_ARGS__)
#define SV_LOG_ERROR(ch, ...) SV_LOG(::sv::log::Level::Error,  ch, __VA_ARGS__)

// ---------------------------------------------------------------------------
// Tiempo
// ---------------------------------------------------------------------------
[[nodiscard]] f64 nowSeconds(); // reloj monotono

struct Stopwatch {
    f64 start = 0.0;
    void  reset() { start = nowSeconds(); }
    [[nodiscard]] f64 elapsed() const { return nowSeconds() - start; }
};

struct FrameClock {
    f64  elapsed  = 0.0;   // segundos desde init
    f32  dt       = 1.0f / 60.0f;
    u64  frame    = 0;
    void init() { last = nowSeconds(); elapsed = 0; fpsSmooth = 60.0f; frame = 0; }
    void tick();           // avanza un frame (dt limitado a 100 ms)
    [[nodiscard]] f32 fps() const { return fpsSmooth; }
private:
    f64  last = 0.0;
    f32  fpsSmooth = 60.0f;
};

// ---------------------------------------------------------------------------
// RNG determinista (PCG32) - mismo resultado en todas las plataformas
// ---------------------------------------------------------------------------
struct Random {
    u64 state = 0x853c49e6748fea9bULL;
    u64 inc   = 0xda3e39cb94b95bdbULL;

    void seed(u64 s);
    [[nodiscard]] u32 nextU32();
    [[nodiscard]] u32 rangeU32(u32 hiExclusive);      // [0, hi)
    [[nodiscard]] i32 rangeI32(i32 loInclusive, i32 hiExclusive);
    [[nodiscard]] f32 nextF32();                      // [0, 1)
    [[nodiscard]] f32 rangeF32(f32 lo, f32 hi);
    [[nodiscard]] f32 symmetric();                    // [-1, 1)
    [[nodiscard]] f32 normal();                       // N(0,1) Box-Muller
    [[nodiscard]] bool chance(f32 probability);
    [[nodiscard]] u32 pick(u32 n) { return rangeU32(n); }
};

// ---------------------------------------------------------------------------
// Filesystem
// ---------------------------------------------------------------------------
namespace fs {
    [[nodiscard]] bool readFile(const char* path, std::vector<u8>& out);
    [[nodiscard]] bool writeFile(const char* path, const void* data, usize size);
    [[nodiscard]] bool fileExists(const char* path);
    [[nodiscard]] std::string exeDir();                 // directorio del ejecutable
    [[nodiscard]] std::string userDir(const char* app); // ~/.app (o exeDir de respaldo)
    [[nodiscard]] std::string join(const std::string& a, const std::string& b);
    // Busca un recurso en varias raices: exeDir/rel, cwd/rel, exeDir/../rel, ...
    [[nodiscard]] std::string findResource(const char* rel);
}

// ---------------------------------------------------------------------------
// Config INI-lite (clave = valor) con orden preservado
// ---------------------------------------------------------------------------
class Config {
public:
    void clear();
    [[nodiscard]] bool load(const char* path);
    [[nodiscard]] bool save(const char* path) const;

    [[nodiscard]] std::string getStr(std::string_view key, std::string_view def = "") const;
    [[nodiscard]] i32 getInt(std::string_view key, i32 def = 0) const;
    [[nodiscard]] f32 getFloat(std::string_view key, f32 def = 0.0f) const;
    [[nodiscard]] bool getBool(std::string_view key, bool def = false) const;

    void set(std::string_view key, std::string_view value);
    // Overload const char*: sin el, set("k", "texto") resolveria al overload bool
    // (conversion puntero->bool es estandar y gana a la conversion a string_view).
    void set(std::string_view key, const char* value) { set(key, std::string_view(value)); }
    void set(std::string_view key, i32 value);
    void set(std::string_view key, f32 value);
    void set(std::string_view key, bool value);

    bool dirty = false;
private:
    std::vector<std::pair<std::string, std::string>> items;
    [[nodiscard]] const std::string* find(std::string_view key) const;
};

// Formato auxiliar portable (vsnprintf seguro)
[[nodiscard]] std::string format(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// ---------------------------------------------------------------------------
// Macros auxiliares
// ---------------------------------------------------------------------------
#define SV_ASSERT(cond) \
    do { if (!(cond)) { ::sv::log::message(::sv::log::Level::Error, "assert", __FILE__, __LINE__, "Fallo de asercion: %s", #cond); } } while (0)

#define SV_NO_COPY(Type) \
    Type(const Type&) = delete; \
    Type& operator=(const Type&) = delete

} // namespace sv
