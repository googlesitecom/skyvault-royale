// ============================================================================
//  SKYVAULT Royale - engine/audio/Audio.cpp
// ============================================================================
#include "audio/Audio.h"
#include "math/Math.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace sv {

Audio& audioSystem() { static Audio a; return a; }

// ---------------------------------------------------------------------------
// Sintesis: generadores basicos (ruido, tonos con envelope exponencial)
// ---------------------------------------------------------------------------
namespace {

constexpr u32 SampleRate = 22050;

struct Synth {
    std::vector<i16>& out;
    Random rng{12345};

    void noise(f32 dur, f32 vol, f32 decay, f32 lowpassK = 0.25f) {
        const u32 n = (u32)(dur * SampleRate);
        f32 lp = 0;
        for (u32 i = 0; i < n; ++i) {
            const f32 t = (f32)i / n;
            const f32 w = rng.symmetric();
            lp += (w - lp) * lowpassK;
            const f32 e = std::exp(-t * decay);
            out.push_back((i16)clamp<i32>((i32)(lp * vol * e * 32000), -32760, 32760));
        }
    }
    void tone(f32 f0, f32 f1, f32 dur, f32 vol, f32 decay, f32 harm = 0.0f) {
        const u32 n = (u32)(dur * SampleRate);
        f32 phase = 0;
        for (u32 i = 0; i < n; ++i) {
            const f32 t = (f32)i / n;
            const f32 f = lerpT(f0, f1, t);
            phase += f / SampleRate * TAU_F;
            const f32 e = std::exp(-t * decay);
            const f32 s = std::sin(phase) + std::sin(phase * 2.0f) * harm;
            out.push_back((i16)clamp<i32>((i32)(s * vol * e * 16000), -32760, 32760));
        }
    }
    void thump(f32 f, f32 dur, f32 vol) {
        const u32 n = (u32)(dur * SampleRate);
        f32 phase = 0;
        for (u32 i = 0; i < n; ++i) {
            const f32 t = (f32)i / n;
            phase += f / SampleRate * TAU_F * (1.0f - t * 0.5f);
            const f32 e = std::exp(-t * 9.0f);
            out.push_back((i16)clamp<i32>((i32)(std::sin(phase) * vol * e * 30000), -32760, 32760));
        }
    }
    void silence(f32 dur) {
        out.insert(out.end(), (usize)(dur * SampleRate), 0);
    }
};

i16 mixClamp(i32 a, i32 b) { return (i16)clamp<i32>(a + b, -32760, 32760); }

} // namespace

void Audio::synthesize() {
    m_buffers.resize(SfxCount);
    auto mk = [&](Sfx id, auto fn) {
        std::vector<i16> buf;
        Synth s{buf};
        fn(s);
        m_buffers[(u32)id] = std::move(buf);
    };

    mk(Sfx::Shoot,       [&](Synth& s){ s.noise(0.09f, 0.9f, 7.0f, 0.55f); s.thump(120, 0.10f, 0.7f); });
    mk(Sfx::ShootHeavy,  [&](Synth& s){ s.noise(0.16f, 1.0f, 5.0f, 0.30f); s.thump(75, 0.20f, 0.9f); });
    mk(Sfx::ShootSniper, [&](Synth& s){ s.noise(0.28f, 1.0f, 4.0f, 0.22f); s.thump(58, 0.30f, 1.0f); s.tone(1400, 300, 0.18f, 0.15f, 12.0f); });
    mk(Sfx::Hit,         [&](Synth& s){ s.tone(900, 500, 0.05f, 0.5f, 18.0f); });
    mk(Sfx::Headshot,    [&](Synth& s){ s.tone(1250, 940, 0.14f, 0.55f, 8.0f, 0.4f); });
    mk(Sfx::Chop,        [&](Synth& s){ s.noise(0.07f, 0.8f, 12.0f, 0.5f); s.thump(160, 0.08f, 0.6f); });
    mk(Sfx::HitWood,     [&](Synth& s){ s.noise(0.05f, 0.6f, 14.0f, 0.6f); s.tone(420, 300, 0.06f, 0.3f, 16.0f); });
    mk(Sfx::BuildPlace,  [&](Synth& s){ s.thump(190, 0.09f, 0.8f); s.noise(0.05f, 0.35f, 16.0f); });
    mk(Sfx::BuildBreak,  [&](Synth& s){ s.noise(0.30f, 0.9f, 6.0f, 0.35f); s.thump(90, 0.16f, 0.7f); });
    mk(Sfx::Pickup,      [&](Synth& s){ s.tone(620, 980, 0.09f, 0.45f, 7.0f); });
    mk(Sfx::Chest,       [&](Synth& s){ s.tone(660, 660, 0.10f, 0.4f, 6.0f, 0.5f); s.silence(0.05f); s.tone(880, 880, 0.10f, 0.4f, 6.0f, 0.5f); s.silence(0.05f); s.tone(1320, 1320, 0.16f, 0.4f, 5.0f, 0.5f); });
    mk(Sfx::Storm,       [&](Synth& s){ s.noise(0.55f, 0.55f, 2.2f, 0.10f); s.thump(48, 0.5f, 0.5f); });
    mk(Sfx::StormTick,   [&](Synth& s){ s.noise(0.10f, 0.35f, 9.0f, 0.7f); });
    mk(Sfx::Glide,       [&](Synth& s){ s.noise(0.5f, 0.30f, 1.5f, 0.12f); });
    mk(Sfx::Land,        [&](Synth& s){ s.thump(85, 0.14f, 1.0f); s.noise(0.08f, 0.4f, 14.0f); });
    mk(Sfx::Kill,        [&](Synth& s){ s.tone(740, 740, 0.08f, 0.5f, 7.0f); s.silence(0.06f); s.tone(1110, 1110, 0.14f, 0.55f, 6.0f, 0.3f); });
    mk(Sfx::Death,       [&](Synth& s){ s.tone(420, 160, 0.5f, 0.5f, 4.0f); });
    mk(Sfx::Victory,     [&](Synth& s){ s.tone(523, 523, 0.16f, 0.5f, 4.0f, 0.4f); s.silence(0.04f); s.tone(659, 659, 0.16f, 0.5f, 4.0f, 0.4f); s.silence(0.04f); s.tone(784, 784, 0.16f, 0.5f, 4.0f, 0.4f); s.silence(0.04f); s.tone(1047, 1047, 0.4f, 0.55f, 3.0f, 0.4f); });
    mk(Sfx::Click,       [&](Synth& s){ s.tone(1100, 900, 0.03f, 0.4f, 20.0f); });
    mk(Sfx::Heal,        [&](Synth& s){ s.tone(440, 760, 0.35f, 0.35f, 4.0f); });
    mk(Sfx::Jump,        [&](Synth& s){ s.noise(0.12f, 0.2f, 8.0f, 0.4f); });
    mk(Sfx::StepWood,    [&](Synth& s){ s.noise(0.04f, 0.25f, 18.0f, 0.5f); });
}

// ---------------------------------------------------------------------------
// Web: shim WebAudio (plataforma). Los PCM se registran una vez y se
// reproducen con rate/volumen.
// ---------------------------------------------------------------------------
#ifdef SKYVAULT_WEB
#include <emscripten.h>

bool Audio::init() {
    synthesize();
    EM_ASM({
        // OJO: sin comas de nivel superior (el macro EM_ASM las interpreta
        // como separadores de argumentos)
        window.svAudio = {};
        window.svAudio.ctx = null;
        window.svAudio.bufs = [];
        try {
            window.svAudio.ctx = new (window.AudioContext || window.webkitAudioContext)();
        } catch (e) { console.warn('WebAudio no disponible'); }
    });
    for (u32 i = 0; i < SfxCount; ++i) {
        const std::vector<i16>& b = m_buffers[i];
        if (b.empty()) continue;
        EM_ASM({ window.svAudio.bufs[$0] = window.svAudio.ctx.createBuffer(1, $1, $2); },
               i, (int)b.size(), (int)SampleRate);
        EM_ASM({
            const buf = window.svAudio.bufs[$0];
            const ch = buf.getChannelData(0);
            const src = HEAP16.subarray($1 >> 1, ($1 >> 1) + $2);
            for (let i = 0; i < $2; ++i) ch[i] = src[i] / 32768.0;
        }, i, (int)(usize)b.data(), (int)b.size());
    }
    m_ok = true;
    SV_LOG_INFO("audio", "WebAudio listo (%d SFX sintetizados)", (i32)SfxCount);
    return true;
}
void Audio::shutdown() {}

void Audio::play(Sfx s, f32 volume, f32 pitch) {
    if (!m_ok || (u32)s >= SfxCount) return;
    EM_ASM({
        const A = window.svAudio;
        if (!A || !A.ctx || !A.bufs[$0]) return;
        if (A.ctx.state === 'suspended') A.ctx.resume();
        const src = A.ctx.createBufferSource();
        src.buffer = A.bufs[$0];
        src.playbackRate.value = $2;
        const g = A.ctx.createGain();
        g.gain.value = $1;
        src.connect(g); g.connect(A.ctx.destination);
        src.start();
    }, (int)s, (double)(volume * m_master), (double)pitch);
}

#else // -----------------------------------------------------------------------
// Escritorio: miniaudio con mezclador propio en el callback
// ---------------------------------------------------------------------------
#define MA_NO_PULSEAUDIO 1
#define MA_NO_JACK 1
#define MA_NO_RUNTIME_LINKING 1
#include "miniaudio.h"

struct Audio::MaDevice {
    ma_device device{};
};

// voces compartidas (el objeto Audio es singleton)
static std::vector<Audio::Voice>* g_voicesInternal = nullptr;
static void* g_audioMutex = nullptr;
static f32 g_masterVol = 0.8f;

static void dataCallback(ma_device* pDevice, void* pOutput, const void*, u32 frameCount) {
    (void)pDevice;
    i16* out = (i16*)pOutput;
    std::memset(out, 0, frameCount * sizeof(i16) * 2);   // estereo
    if (!g_voicesInternal) return;
    if (g_audioMutex) ma_mutex_lock((ma_mutex*)g_audioMutex);
    auto& voices = *g_voicesInternal;
    for (auto& v : voices) {
        if (v.pos >= v.len) continue;
        const f32 step = v.pitch;
        f32 src = (f32)v.pos;
        for (u32 f = 0; f < frameCount && src < (f32)v.len; ++f, src += step) {
            const i16 s = v.data[(u32)src];
            out[f * 2] = mixClamp(out[f * 2], (i32)(s * v.vol * g_masterVol));
            out[f * 2 + 1] = mixClamp(out[f * 2 + 1], (i32)(s * v.vol * g_masterVol));
        }
        v.pos = (u32)src;
    }
    // limpia voces terminadas
    voices.erase(std::remove_if(voices.begin(), voices.end(),
                 [](const auto& v) { return v.pos >= v.len; }), voices.end());
    if (g_audioMutex) ma_mutex_unlock((ma_mutex*)g_audioMutex);
}

bool Audio::init() {
    synthesize();
    m_device = new MaDevice();
    auto* mutex = new ma_mutex;
    ma_mutex_init(mutex);
    m_mutex = mutex;
    g_audioMutex = mutex;
    g_voicesInternal = &m_voices;

    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format = ma_format_s16;
    cfg.playback.channels = 2;
    cfg.sampleRate = SampleRate;
    cfg.dataCallback = dataCallback;
    if (ma_device_init(nullptr, &cfg, &m_device->device) != MA_SUCCESS) {
        SV_LOG_WARN("audio", "miniaudio: no se pudo abrir el dispositivo (sin sonido)");
        delete m_device; m_device = nullptr;
        return false;   // el juego sigue sin audio
    }
    if (ma_device_start(&m_device->device) != MA_SUCCESS) {
        SV_LOG_WARN("audio", "miniaudio: fallo al iniciar la reproduccion");
        ma_device_uninit(&m_device->device);
        delete m_device; m_device = nullptr;
        return false;
    }
    m_ok = true;
    SV_LOG_INFO("audio", "miniaudio listo (%d SFX sintetizados)", (i32)SfxCount);
    return true;
}

void Audio::shutdown() {
    if (m_device) {
        ma_device_uninit(&m_device->device);
        delete m_device;
        m_device = nullptr;
    }
    if (m_mutex) {
        ma_mutex_uninit((ma_mutex*)m_mutex);
        delete (ma_mutex*)m_mutex;
        m_mutex = nullptr;
        g_audioMutex = nullptr;
    }
}

void Audio::play(Sfx s, f32 volume, f32 pitch) {
    if (!m_ok || (u32)s >= SfxCount || m_buffers[(u32)s].empty()) return;
    if (m_mutex) ma_mutex_lock((ma_mutex*)m_mutex);
    if (m_voices.size() > 48) m_voices.erase(m_voices.begin());   // limite de voces
    m_voices.push_back({m_buffers[(u32)s].data(),
                        (u32)m_buffers[(u32)s].size(), 0, volume, pitch});
    if (m_mutex) ma_mutex_unlock((ma_mutex*)m_mutex);
}

#endif

} // namespace sv
