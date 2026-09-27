// Implementacion de miniaudio (backend de audio auxiliar permitido por diseno).
// El motor de mezcla, buses y espacializacion 3D son codigo propio (engine/audio).
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_PULSEAUDIO 1
#define MA_NO_JACK 1
#define MA_NO_RUNTIME_LINKING 1
#include "miniaudio.h"
