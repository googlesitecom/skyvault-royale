// ============================================================================
//  SkyVault Engine - implementacion del nucleo
// ============================================================================
#include "core/Core.h"

#include <cstdarg>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <algorithm>

#if defined(_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#else
  #include <unistd.h>
  #include <sys/stat.h>
#endif

namespace sv {

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------
namespace log {

static Level g_level  = Level::Info;
static bool  g_colors = true;

void setLevel(Level lvl) { g_level = lvl; }
void setColors(bool enabled) { g_colors = enabled; }

static const char* levelTag(Level lvl) {
    switch (lvl) {
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO ";
        case Level::Warn:  return "WARN ";
        default:           return "ERROR";
    }
}
static const char* levelAnsi(Level lvl) {
    switch (lvl) {
        case Level::Debug: return "\x1b[90m";
        case Level::Info:  return "\x1b[32m";
        case Level::Warn:  return "\x1b[33m";
        default:           return "\x1b[31m";
    }
}

void message(Level lvl, const char* channel, const char* file, int line, const char* fmt, ...) {
    if (static_cast<i32>(lvl) < static_cast<i32>(g_level)) return;

    char text[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);

    const char* slash = std::strrchr(file, '/');
    const char* bslash = std::strrchr(file, '\\');
    if (bslash && (!slash || bslash > slash)) slash = bslash;
    const char* fileName = slash ? slash + 1 : file;

    if (g_colors) {
        std::fprintf(stderr, "%s[%s]\x1b[0m [%s] \x1b[90m%s:%d\x1b[0m: %s\n",
                     levelAnsi(lvl), levelTag(lvl), channel, fileName, line, text);
    } else {
        std::fprintf(stderr, "[%s] [%s] %s:%d: %s\n", levelTag(lvl), channel, fileName, line, text);
    }
    std::fflush(stderr);
}

} // namespace log

// ---------------------------------------------------------------------------
// Tiempo
// ---------------------------------------------------------------------------
f64 nowSeconds() {
    struct timespec ts {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<f64>(ts.tv_sec) + static_cast<f64>(ts.tv_nsec) * 1e-9;
}

void FrameClock::tick() {
    const f64 now = nowSeconds();
    f32 delta = static_cast<f32>(now - last);
    last = now;
    delta = std::min(delta, 0.1f); // clamp tras pausas / debugger
    dt = delta;
    elapsed += delta;
    ++frame;
    if (delta > 1e-5f) fpsSmooth = fpsSmooth * 0.95f + (1.0f / delta) * 0.05f;
}

// ---------------------------------------------------------------------------
// RNG (PCG32)
// ---------------------------------------------------------------------------
void Random::seed(u64 s) {
    state = s ? s : 0x853c49e6748fea9bULL;
    inc = (s << 1 | 1) ^ 0xda3e39cb94b95bdbULL;
    static_cast<void>(nextU32()); // calienta el estado
}

u32 Random::nextU32() {
    const u64 old = state;
    state = old * 6364136223846793005ULL + inc;
    const u32 xorshifted = static_cast<u32>(((old >> 18) ^ old) >> 27);
    const u32 rot = static_cast<u32>(old >> 59);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

u32 Random::rangeU32(u32 hiExclusive) {
    if (hiExclusive <= 1) return 0;
    // Rechazo de sesgo de modulo: solo acepta r < (2^32 / n) * n.
    // Caso especial: n divide a 2^32 (potencias de 2) -> el modulo ya es uniforme
    // y el limite desbordaria u32 (2^32 -> 0), lo que provocaria un bucle infinito.
    const u64 limit = (0x100000000ULL / hiExclusive) * hiExclusive;
    if (limit >= 0x100000000ULL) return nextU32() % hiExclusive;
    for (;;) {
        const u32 r = nextU32();
        if (r < static_cast<u32>(limit)) return r % hiExclusive;
    }
}

i32 Random::rangeI32(i32 loInclusive, i32 hiExclusive) {
    if (hiExclusive <= loInclusive) return loInclusive;
    return loInclusive + static_cast<i32>(rangeU32(static_cast<u32>(hiExclusive - loInclusive)));
}

f32 Random::nextF32() {
    return static_cast<f32>(nextU32() >> 8) * (1.0f / 16777216.0f);
}

f32 Random::rangeF32(f32 lo, f32 hi) { return lo + (hi - lo) * nextF32(); }
f32 Random::symmetric() { return nextF32() * 2.0f - 1.0f; }

f32 Random::normal() {
    // Box-Muller con cache del segundo valor
    static thread_local bool cached = false;
    static thread_local f32 cachedValue = 0.0f;
    if (cached) { cached = false; return cachedValue; }
    const f32 u1 = std::max(nextF32(), 1e-7f);
    const f32 u2 = nextF32();
    const f32 mag = std::sqrt(-2.0f * std::log(u1));
    cachedValue = mag * std::sin(TAU_F * u2);
    cached = true;
    return mag * std::cos(TAU_F * u2);
}

bool Random::chance(f32 probability) {
    if (probability <= 0.0f) return false;
    if (probability >= 1.0f) return true;
    return nextF32() < probability;
}

// ---------------------------------------------------------------------------
// Filesystem
// ---------------------------------------------------------------------------
namespace fs {

bool readFile(const char* path, std::vector<u8>& out) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size < 0) { std::fclose(f); return false; }
    out.resize(static_cast<usize>(size));
    const usize rd = size ? std::fread(out.data(), 1, out.size(), f) : 0;
    std::fclose(f);
    return rd == out.size();
}

bool writeFile(const char* path, const void* data, usize size) {
    std::FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    const usize wr = size ? std::fwrite(data, 1, size, f) : 0;
    std::fclose(f);
    return wr == size;
}

bool fileExists(const char* path) {
#if defined(_WIN32)
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st {};
    return ::stat(path, &st) == 0;
#endif
}

std::string exeDir() {
#if defined(_WIN32)
    char buf[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string p = buf;
#elif defined(SKYVAULT_WEB)
    std::string p = "/skyvault.js";
#else
    char buf[4096] = {};
    const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return "./";
    buf[n] = '\0';
    std::string p = buf;
#endif
    const usize slash = p.find_last_of("/\\");
    return (slash == std::string::npos) ? "./" : p.substr(0, slash + 1);
}

std::string userDir(const char* app) {
#if defined(SKYVAULT_WEB)
    (void)app;
    return "/";
#else
    const char* home = std::getenv("HOME");
    #if defined(_WIN32)
    if (!home) home = std::getenv("USERPROFILE");
    const char sep = '\\';
    #else
    const char sep = '/';
    #endif
    if (home) {
        std::string dir = std::string(home) + sep + "." + app;
        #if !defined(_WIN32)
        ::mkdir(dir.c_str(), 0755); // mejor esfuerzo
        #endif
        return dir + sep;
    }
    return exeDir();
#endif
}

std::string join(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
#if defined(_WIN32)
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    const bool aEnds = a.back() == '/' || a.back() == '\\';
    const bool bStarts = b.front() == '/' || b.front() == '\\';
    if (aEnds && bStarts) return a + b.substr(1);
    if (aEnds || bStarts) return a + b;
    return a + sep + b;
}

std::string findResource(const char* rel) {
    const std::string roots[] = { exeDir(), "./", join(exeDir(), "../"), join(exeDir(), "../../") };
    for (const auto& root : roots) {
        const std::string candidate = join(root, rel);
        if (fileExists(candidate.c_str())) return candidate;
    }
    return rel; // ultimo recurso: relativo al cwd
}

} // namespace fs

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------
void Config::clear() { items.clear(); }

const std::string* Config::find(std::string_view key) const {
    for (auto it = items.rbegin(); it != items.rend(); ++it)
        if (it->first == key) return &it->second;
    return nullptr;
}

bool Config::load(const char* path) {
    std::vector<u8> data;
    if (!fs::readFile(path, data)) return false;
    data.push_back('\0');
    const char* p = reinterpret_cast<const char*>(data.data());
    std::string key, value;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
        if (!*p) break;
        if (*p == '#' || *p == ';') { // comentario
            while (*p && *p != '\n') ++p;
            continue;
        }
        key.clear(); value.clear();
        while (*p && *p != '=' && *p != '\n') key.push_back(*p++);
        if (*p != '=') { while (*p && *p != '\n') ++p; continue; }
        ++p; // '='
        while (*p == ' ' || *p == '\t') ++p;
        while (*p && *p != '\n' && *p != '\r') value.push_back(*p++);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        set(key, value);
    }
    dirty = false;
    return true;
}

bool Config::save(const char* path) const {
    std::string out = "# SkyVault Engine - configuracion\n";
    for (const auto& [k, v] : items) out += k + " = " + v + "\n";
    return fs::writeFile(path, out.data(), out.size());
}

std::string Config::getStr(std::string_view key, std::string_view def) const {
    const std::string* v = find(key);
    return v ? *v : std::string(def);
}
i32 Config::getInt(std::string_view key, i32 def) const {
    const std::string* v = find(key);
    return v ? std::atoi(v->c_str()) : def;
}
f32 Config::getFloat(std::string_view key, f32 def) const {
    const std::string* v = find(key);
    return v ? static_cast<f32>(std::atof(v->c_str())) : def;
}
bool Config::getBool(std::string_view key, bool def) const {
    const std::string* v = find(key);
    if (!v) return def;
    return *v == "1" || *v == "true" || *v == "on" || *v == "yes";
}

void Config::set(std::string_view key, std::string_view value) {
    for (auto& [k, v] : items)
        if (k == key) { if (v != value) { v = std::string(value); dirty = true; } return; }
    items.emplace_back(std::string(key), std::string(value));
    dirty = true;
}
void Config::set(std::string_view key, i32 value)  { set(key, std::to_string(value)); }
void Config::set(std::string_view key, f32 value)  { char b[32]; std::snprintf(b, sizeof(b), "%.4f", value); set(key, std::string_view(b)); }
void Config::set(std::string_view key, bool value) { set(key, std::string_view(value ? "true" : "false")); }

// ---------------------------------------------------------------------------
// format()
// ---------------------------------------------------------------------------
std::string format(const char* fmt, ...) {
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return buf;
}

} // namespace sv
