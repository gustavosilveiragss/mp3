#include <marmota.h>

#include <game/GameHost.h>

#include "Audio.h"
#include "Keypad.h"
#include "Library.h"
#include "Media.h"
#include "Navigator.h"
#include "Player.h"
#include "Portal.h"
#include "Settings.h"
#include "Text.h"
#include "Ui.h"
#include "pins.h"

using namespace mrm;

namespace {

Ssd1306Display display({.sda = pins::kOledSda, .scl = pins::kOledScl});
Led led({.pin = pins::kLed, .activeLow = false});
Keypad keys(pins::kKeyA, pins::kKeyB);
Library library;
Settings settings;
Audio audio;
Player player(library, settings, audio);
Locale locale(kTexts, static_cast<uint16_t>(Txt::Count));
mrm::game::twobtn::Set games;
mrm::game::GameHost gameHost(settings, games.list, mrm::game::twobtn::kCount);
Portal portal(media::fs(), locale);
Navigator navigator(player, settings, audio, locale, gameHost, portal);
Ui screen(display, locale);

bool displayOff = false;
uint32_t lastDraw = 0;

// Teclas e gestos. Roda a cada volta do loop e, durante um upload (handleClient nao devolve o
// controle), a cada tick do Portal.
void readKeys() {
    Keypad::Action action = keys.update();
    const bool chord = action == Keypad::Action::NextPlaylist && navigator.countsChords();
    if (action != Keypad::Action::None && displayOff && !navigator.locked() && !chord) {
        display.on(); // a primeira tecla so acorda o display
        displayOff = false;
        navigator.wake();
        action = Keypad::Action::None;
    }
    keys.setInstant(navigator.menuInput());
    navigator.setHeld(keys.aDown(), keys.bDown());
    navigator.handle(action);
}

void drawFrame(uint32_t frameMs = Ui::kFrameMs) {
    const uint32_t now = millis();
    if (displayOff || now - lastDraw < (navigator.gameRunning() ? mrm::game::Game::kTickMs : frameMs))
        return;
    lastDraw = now;
    screen.draw(navigator.view(), now);
}

#ifndef MP3_TEST_PORTAL
#define MP3_TEST_PORTAL 0 // 1: sobe o portal Wi-Fi sozinho no boot (teste automatizado)
#endif
#ifndef MP3_DEBUG_STACK
#define MP3_DEBUG_STACK 0 // 1: loga a folga de pilha do tick do Portal no serial
#endif

// Um quadro do OLED custa ~13 ms de I2C com a janela TCP fechada: durante o envio a barra anda a 2 Hz.
constexpr uint32_t kPortalFrameMs = 500;

void portalTick(void*) {
    readKeys();
    led.heartbeat();
    drawFrame(kPortalFrameMs);
#if MP3_DEBUG_STACK
    static uint32_t stackAt;
    if (millis() - stackAt >= 1000) {
        stackAt = millis();
        Serial.printf("portal stack min livre: %u B\n", unsigned(uxTaskGetStackHighWaterMark(nullptr)));
    }
#endif
}

} // namespace

void setup() {
    Serial.begin(115200);
    display.begin();
    led.begin();
    keys.begin();

    const bool mediaOk = media::begin();
    settings.begin();
    locale.set(settings.lang());
    audio.begin();
    audio.setEnabled(settings.bluetooth());
    player.begin(mediaOk);
    portal.onTick(portalTick, nullptr);
    navigator.begin();
}

void loop() {
#if MP3_TEST_PORTAL
    static bool wifiOpened; // parar o BT logo apos ligar derruba o radio (assert em vQueueDelete): espera a varredura assentar
    if (!wifiOpened && millis() > 12000) {
        wifiOpened = true;
        navigator.testOpenWifi();
    }
#endif
    readKeys();
    navigator.update();
    settings.defer(audio.connected());
    settings.update();
    led.heartbeat();

    if (displayOff && navigator.wantsDisplay()) { // desbloqueou ou trocou de playlist com a tela apagada
        display.on();
        displayOff = false;
    }
    if (navigator.idle() && !displayOff && !navigator.toastActive()) {
        display.off();
        displayOff = true;
    }
    drawFrame();
}
