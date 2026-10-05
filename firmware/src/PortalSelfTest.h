#pragma once

// Teste real do portal Wi-Fi embutido no firmware. Só existe com -DMP3_TEST_PORTAL=1 (nenhum env o define).
//
// Como ligar: PLATFORMIO_BUILD_FLAGS='-DMP3_TEST_PORTAL=1' ./scripts/pio.sh run -d firmware -t upload
// O que cobre: sobe o Wi-Fi e o portal sozinho no boot, sem botões, para o computador enviar e gerenciar
// arquivos pela rede. Junte -DMP3_PORTAL_DEBUG=1 para ver tempos de SD, rede e heap no serial.
// Quem usa: firmware/web/test/portal_test.py (via run_portal_test.sh) e run_bench.sh.
#ifndef MP3_TEST_PORTAL
#define MP3_TEST_PORTAL 0
#endif

#if MP3_TEST_PORTAL

#include <Arduino.h>

class Navigator;

class PortalSelfTest {
public:
    static void begin(Navigator& navigator); // no setup, depois de navigator.begin()
    static void update();                    // no loop. Abre o Wi-Fi uma única vez

private:
    static constexpr uint32_t kOpenAfterMs = 12000; // parar o BT logo após ligar derruba o rádio: espera a varredura
};

#define PORTAL_SELF_TEST_BEGIN(navigator) PortalSelfTest::begin(navigator)
#define PORTAL_SELF_TEST_UPDATE() PortalSelfTest::update()
#else
#define PORTAL_SELF_TEST_BEGIN(navigator) ((void)0)
#define PORTAL_SELF_TEST_UPDATE() ((void)0)
#endif
