#include <marmota.h>

#include <game/GameHost.h>

#include "Audio.h"
#include "Keypad.h"
#include "Library.h"
#include "Media.h"
#include "Navigator.h"
#include "Player.h"
#include "Portal.h"
#include "PortalSelfTest.h"
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
Locale locale;
mrm::game::twobtn::Set games;
mrm::game::GameHost gameHost(settings, games.list, mrm::game::twobtn::kCount);
Portal portal(media::fs(), locale);
Navigator navigator(player, settings, audio, locale, gameHost, portal);
Ui screen(display, locale);

bool displayOff = false;
uint32_t lastDraw = 0;

// Teclas e gestos. Roda a cada volta do loop e, durante um upload (handleClient não devolve o
// controle), a cada tick do Portal.
void readKeys() {
    Keypad::Action action = keys.update();
    const bool chord = action == Keypad::Action::NextPlaylist && navigator.countsChords();

    if (action != Keypad::Action::None && displayOff && !navigator.locked() && !chord) {
        display.on(); // a primeira tecla só acorda o display
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

// Um quadro do OLED custa ~13 ms de I2C com a janela TCP fechada: durante o envio a barra anda a 2 Hz.
constexpr uint32_t kPortalFrameMs = 500;

void portalTick(void*) {
    readKeys();
    led.heartbeat();
    drawFrame(kPortalFrameMs);
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
    PORTAL_SELF_TEST_BEGIN(navigator);
}

void loop() {
    PORTAL_SELF_TEST_UPDATE();
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
