// A pilha monta 1 pacote AVDTP por rodada de 30 ms quando o link satura (7 frames com bitpool 53, 8 com
// 32); so passam 33 pacotes/s. Para carregar os 344 frames/s o pacote precisa levar kPacketFrames frames:
// bitpool = ((mtu - overhead) / kPacketFrames - 13) / 2 (frame SBC joint stereo = 13 + 2 x bitpool bytes).
// Precisa de -Wl,--wrap=bta_av_co_audio_get_sbc_config (platformio.ini).
#include <Arduino.h> // define ESP_ARDUINO_VERSION_MAJOR, usado abaixo
#include <BtSource.h>

#if MRM_HAS_BT_SOURCE
constexpr int kPacketFrames = 7;

struct SbcCie { // tA2D_SBC_CIE do Bluedroid (IDF 4.4)
    uint8_t sampFreq, chMode, blockLen, numSubbands, allocMthd, maxBitpool, minBitpool;
};

constexpr int kRtpOverhead = 14; // cabecalho RTP + SBC, com 1 byte de folga
constexpr int kMinBitpool = 10;

uint8_t gSbcBitpool = 0;
uint16_t gSbcMtu = 0;

// Arduino-ESP32 3.x devolve a memoria do Bluetooth no boot se ninguem declarar uso (a pilha nao sobe).
// So o classico: btInUse() seguraria tambem a memoria do BLE e deixaria o heap apertado para o A2DP.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
extern "C" bool btClassicInUse() {
    return true;
}
#endif

extern "C" bool __real_bta_av_co_audio_get_sbc_config(SbcCie* cfg, uint16_t* minMtu);

extern "C" bool __wrap_bta_av_co_audio_get_sbc_config(SbcCie* cfg, uint16_t* minMtu) {
    const bool ok = __real_bta_av_co_audio_get_sbc_config(cfg, minMtu);
    if (ok && *minMtu > kRtpOverhead && *minMtu != 0xFFFF) {
        int bp = (int(*minMtu - kRtpOverhead) / kPacketFrames - 13) / 2;
        if (bp < kMinBitpool)
            bp = kMinBitpool;
        if (bp < cfg->maxBitpool)
            cfg->maxBitpool = bp;
        if (cfg->minBitpool > cfg->maxBitpool)
            cfg->minBitpool = cfg->maxBitpool;
    }
    gSbcBitpool = cfg->maxBitpool;
    gSbcMtu = *minMtu;
    return ok;
}
#endif
