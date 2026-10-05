#pragma once

#include <Arduino.h>

// -DMP3_AUDIO_DEBUG=1: eventos do áudio e uma linha por segundo no serial (engasgo, memória, qualidade do link).
#ifndef MP3_AUDIO_DEBUG
#define MP3_AUDIO_DEBUG 0
#endif

#if MP3_AUDIO_DEBUG
#include <Log.h>
#define AUD_EV(...) mrm::log("aud ", millis(), " ", __VA_ARGS__)

namespace audiodebug {
void feedStart(int32_t asked);                    // callback do A2DP, antes de ler o ring
void feedDone(const uint8_t* pcm, int32_t bytes); // e depois, com o que foi entregue
void btState(int state);                          // só imprime quando muda
void report(int link, int state, uint8_t volume); // no loop, imprime no máximo uma vez por segundo
} // namespace audiodebug
#define AUD_FEED_START(asked) audiodebug::feedStart(asked)
#define AUD_FEED_DONE(pcm, bytes) audiodebug::feedDone(pcm, bytes)
#define AUD_REPORT(link, state, volume) audiodebug::report(static_cast<int>(link), static_cast<int>(state), volume)
#define AUD_BT_STATE(state) audiodebug::btState(static_cast<int>(state))
#else
#define AUD_EV(...) ((void)0)
#define AUD_FEED_START(asked) ((void)0)
#define AUD_FEED_DONE(pcm, bytes) ((void)0)
#define AUD_REPORT(link, state, volume) ((void)0)
#define AUD_BT_STATE(state) ((void)0)
#endif
