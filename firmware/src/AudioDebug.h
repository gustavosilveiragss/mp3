#pragma once

#include <Arduino.h>

// -DMP3_AUDIO_DEBUG=1: eventos do audio e uma linha por segundo no serial (engasgo, memoria, qualidade do link).
#ifndef MP3_AUDIO_DEBUG
#define MP3_AUDIO_DEBUG 0
#endif

#if MP3_AUDIO_DEBUG
#include <Log.h>
#define AUD_EV(...) mrm::log("aud ", millis(), " ", __VA_ARGS__)

namespace audiodebug {
void feedStart(int32_t asked);                    // callback do A2DP, antes de ler o ring
void feedDone(const uint8_t* pcm, int32_t bytes); // e depois, com o que foi entregue
void report(int link, int state, uint8_t volume); // no loop; imprime no maximo uma vez por segundo
} // namespace audiodebug
#else
#define AUD_EV(...) ((void)0)
#endif
