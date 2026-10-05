#pragma once

#include <Arduino.h>

// -DMP3_PORTAL_DEBUG=1: por envio, tempo em SD e fora dele (rede, tick), tamanho dos blocos e heap, numa linha no serial.
#ifndef MP3_PORTAL_DEBUG
#define MP3_PORTAL_DEBUG 0
#endif

#if MP3_PORTAL_DEBUG
#include <Log.h>
#include <lwip/sockets.h>

namespace portaldebug {
struct Stats {
    uint32_t startUs = 0, tickUs = 0, sdEndUs = 0, gapMaxUs = 0, sdUs = 0, sdMaxUs = 0, blocks = 0, bytes = 0, minBlock = UINT32_MAX, maxBlock = 0;
};
inline Stats s;

inline void start() {
    s = Stats{}, s.startUs = micros();
}
inline void sd(uint32_t t0, uint32_t n) {
    if (s.sdEndUs)
        s.gapMaxUs = max(s.gapMaxUs, t0 - s.sdEndUs);
    const uint32_t d = micros() - t0;
    s.sdUs += d;
    s.sdEndUs = micros();
    s.sdMaxUs = max(s.sdMaxUs, d);
    s.blocks++;
    s.bytes += n;
    s.minBlock = min(s.minBlock, n);
    s.maxBlock = max(s.maxBlock, n);
}
// do fim da gravacao ate aqui: so o tick
inline void tick() {
    s.tickUs += micros() - s.sdEndUs;
}
inline void event(const char* what, uint32_t bytes, int fd = -1) {
    int soError = -1;
    if (fd >= 0) {
        socklen_t len = sizeof(soError);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &soError, &len);
    }
    mrm::log("portal ", what, " fd=", fd, " so_error=", soError, " errno=", errno);
    mrm::log("portal ", what, " bytes=", bytes, " ms=", (micros() - s.startUs) / 1000, " blocks=", s.blocks, " sd_ms=", s.sdUs / 1000, " sdmax_ms=", s.sdMaxUs / 1000, " gapmax_ms=", s.gapMaxUs / 1000, " parado_ms=", (micros() - s.sdEndUs) / 1000, " heap=", ESP.getFreeHeap(), " minheap=", ESP.getMinFreeHeap());
}
inline void report(uint32_t stackFree) {
    const uint32_t total = micros() - s.startUs;
    mrm::log("portal bytes=", s.bytes, " blocks=", s.blocks, " blk=", s.minBlock, "..", s.maxBlock, " total_ms=", total / 1000, " sd_ms=", s.sdUs / 1000,
             " sdmax_ms=", s.sdMaxUs / 1000, " tick_ms=", s.tickUs / 1000, " net_ms=", (total - s.sdUs - s.tickUs) / 1000,
             " KBps=", s.bytes / 1024.0 * 1e6 / total, " sdstack=", stackFree, " heap=", ESP.getFreeHeap(), " minheap=", ESP.getMinFreeHeap());
}
} // namespace portaldebug
#define PDBG(...) __VA_ARGS__
#else
#define PDBG(...)
#endif
