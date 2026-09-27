// ============================================================================
//  SKYVAULT Royale - engine/audio/Audio.h
//  SFX 100% procedural (sintetizados al iniciar, sin archivos de audio):
//    - Escritorio: miniaudio (vendido en third_party/)
//    - Web: shim WebAudio minimo via EM_ASM (plataforma, no logica de juego)
// ============================================================================
#pragma once

#include "core/Core.h"
#include "math/Math.h"

namespace sv {

enum class Sfx : u8 {
    Shoot = 0, ShootHeavy, ShootSniper, Hit, Headshot, Chop, HitWood,
    BuildPlace, BuildBreak, Pickup, Chest, Storm, StormTick, Glide,
    Land, Kill, Death, Victory, Click, Heal, Jump, StepWood,
    Count
};
constexpr u32 SfxCount = (u32)Sfx::Count;

class Audio {
public:
    [[nodiscard]] bool init();
    void shutdown();
    void play(Sfx s, f32 volume = 1.0f, f32 pitch = 1.0f);
    void setMaster(f32 v) { m_master = clamp01(v); }
    [[nodiscard]] f32 master() const { return m_master; }

#ifndef SKYVAULT_WEB
    // voz del mezclador (publico: lo usa el callback del dispositivo)
    struct Voice { const i16* data = nullptr; u32 len = 0; u32 pos = 0; f32 vol = 1, pitch = 1; };
#endif

private:
    void synthesize();
    std::vector<std::vector<i16>> m_buffers;   // PCM mono 22050 Hz
    f32 m_master = 0.8f;
    bool m_ok = false;

#ifndef SKYVAULT_WEB
    struct MaDevice;
    MaDevice* m_device = nullptr;
    std::vector<Voice> m_voices;
    void* m_mutex = nullptr;
#endif
};

Audio& audioSystem();

} // namespace sv
