#include "AudioDebug.h"

#if MP3_AUDIO_DEBUG

#include "AudioCore.h"

extern uint8_t gSbcBitpool; // BtWrap.cpp
extern uint16_t gSbcMtu;

namespace audiodebug {
namespace {

// Escritos só pelo callback do A2DP. O loop lê e zera uma vez por segundo.
std::atomic<uint32_t> cbCalls{0}, cbBytes{0}, cbMaxGapUs{0}, cbMaxLen{0}, ringMin{UINT32_MAX};
uint32_t cbLastUs = 0;
std::atomic<uint32_t> peak{0}; // maior amostra PCM entregue ao A2DP no último segundo
// Frames por rodada do alarme de 30 ms da pilha: 10-11 é o normal. 7 fixo é a fila do A2DP travada
// em 1 pacote por rodada (engasgo com o fone clone, 2/3 do áudio).
std::atomic<uint32_t> tickN{0}, tickMin{UINT32_MAX}, tickMax{0};
uint32_t tickCur = 0;

void closeTick() {
    tickN.fetch_add(1);

    if (tickCur < tickMin.load())
        tickMin.store(tickCur);

    if (tickCur > tickMax.load())
        tickMax.store(tickCur);

    tickCur = 0;
}

}

void feedStart(int32_t len) {
    const uint32_t now = micros();
    if (cbLastUs && now - cbLastUs > cbMaxGapUs.load())
        cbMaxGapUs.store(now - cbLastUs);

    if (cbLastUs && now - cbLastUs > 8000 && tickCur)
        closeTick();

    if (len > 0)
        ++tickCur;

    cbLastUs = now;

    if (len <= 0)
        return;

    cbCalls.fetch_add(1);
    cbBytes.fetch_add(len);

    if (uint32_t(len) > cbMaxLen.load())
        cbMaxLen.store(len);

    if (audio::stream.buffered() < ringMin.load())
        ringMin.store(audio::stream.buffered());
}

void feedDone(const uint8_t* data, int32_t bytes) {
    const int16_t* pcm = reinterpret_cast<const int16_t*>(data);
    for (int32_t i = 0; i < bytes / 2; ++i)
        if (uint32_t(abs(pcm[i])) > peak.load())
            peak.store(abs(pcm[i]));
}

void btState(int state) {
    static int last = 0;
    if (state != last) {
        AUD_EV("bt ", last, "->", state, " (2 scan 3 conn 4 link 5 stream)");
        last = state;
    }
}

void report(int link, int state, uint8_t volume) {
    static uint32_t at = 0, under0 = 0, frames0 = 0, busy0 = 0;

    const uint32_t now = millis();
    if (now - at < 1000)
        return;

    at = now;

    const mrm::Mp3Stream::Stats& st = audio::stream.stats(); // lint: ok uma chamada de log só
    const uint32_t under = audio::stream.underruns(), frames = st.frames - frames0;
    mrm::log("aud ", now, " 1s link=", link, " bt=", static_cast<int>(audio::bt.state()), " st=", state,
             " und=", under, " +", under - under0,
             " ring_min=", ringMin.exchange(UINT32_MAX), " ring=", audio::stream.buffered(),
             " cb=", cbCalls.exchange(0), " B=", cbBytes.exchange(0),
             " gap_max_us=", cbMaxGapUs.exchange(0), " len_max=", cbMaxLen.exchange(0),
             " dec_fr=", frames, " dec_avg_us=", frames ? (st.busyUs - busy0) / frames : 0,
             " dec_max_us=", st.maxUs, " err=", st.errors,
             " pilha=", audio::stream.stackFree(), " heap=", ESP.getFreeHeap(),
             " min=", ESP.getMinFreeHeap(), " vol=", volume, " btvol=", audio::bt.get_volume(),
             " pos=", audio::stream.playedFrames(), " ticks=", tickN.exchange(0),
             " tmin=", tickMin.exchange(UINT32_MAX), " tmax=", tickMax.exchange(0),
             " bp=", gSbcBitpool, " mtu=", gSbcMtu, " pk=", peak.exchange(0),
             " loopstk=", uxTaskGetStackHighWaterMark(nullptr));

    under0 = under;
    frames0 = st.frames;
    busy0 = st.busyUs;
}

} // namespace audiodebug

#endif
