// Renderiza as telas reais (Ui.cpp, UiMenus.cpp...) no host, com a fonte real do OLED.
// Saida: uma cena por bloco ("# nome" e 64 linhas de 128 colunas).
#include <Arduino.h>
#include <marmota.h>

#include "Text.h"
#include "Ui.h"

uint32_t g_now = 0;

namespace mrm {
Ssd1306Display::Ssd1306Display()
    : oled_(0x3c, 5, 6) {}
} // namespace mrm

static Library lib;
static mrm::Ssd1306Display display;
static mrm::Locale locale(kTexts, static_cast<uint16_t>(Txt::Count));
static mrm::game::twobtn::Set games;
static Ui ui(display, locale);
static Audio::Device devs[3] = {{"TWS-Q29", -48}, {"JBL Tune 510BT", -63}, {"Fone do Gustavo", -80}};

static void scene(const char* name, mrm::Lang lang, View v, uint32_t now) {
    locale.set(lang);
    v.lang = lang;
    g_now = now;
    ui.draw(v, now);
    printf("# %s %s\n", name, mrm::Locale::code(lang));
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 128; ++x)
            putchar(display.raw().pixel(x, y) ? '#' : '.');
        putchar('\n');
    }
}

int main() {
    display.raw().init();
    lib.n = 3;
    lib.names[0] = "Rock";
    lib.names[1] = "Estudo";
    lib.names[2] = "Viagem longa";
    lib.counts[0] = 42;
    lib.counts[1] = 7;
    lib.counts[2] = 120;

    View base;
    base.library = &lib;
    base.playlist = "Rock";
    base.playlistCount = 3;
    base.deviceCode = "A1B2";
    base.btLink = Audio::Link::Connected;
    base.btOn = true;
    base.battery = 80;
    base.peer = "TWS-Q29";
    base.track = "01 - Nome da faixa bem longa.mp3";
    base.trackCount = 42;
    base.trackIndex = 3;
    base.progress = 40;
    base.positionSec = 95;
    base.volume = 8;
    base.volumeBefore = 8;
    base.screenAt = 1;
    base.devices = devs;
    base.deviceCount = 3;

    for (mrm::Lang lang : {mrm::Lang::PtBr, mrm::Lang::En}) {
        View v = base;
        v.mode = View::Mode::Paused;
        scene("home", lang, v, 20000);
        v.mode = View::Mode::Playing;
        scene("tocando", lang, v, 20000);

        v = base;
        v.mode = View::Mode::NoDevice;
        v.noDevice = true;
        v.btLink = Audio::Link::Idle;
        scene("sem-dispositivo", lang, v, 20000);

        v = base;
        v.mode = View::Mode::Empty;
        scene("biblioteca-vazia", lang, v, 20000);

        v.noDevice = true;
        v.btLink = Audio::Link::Idle;
        scene("sem-os-dois-a", lang, v, 20000);
        scene("sem-os-dois-b", lang, v, 23000);

        v = base;
        v.mode = View::Mode::Config;
        scene("config", lang, v, 20000);
        v.cursor = v.cursorPrev = 4;
        scene("config-extras", lang, v, 20000);

        v = base;
        v.mode = View::Mode::Playlists;
        v.cursor = v.cursorPrev = 1;
        scene("biblioteca", lang, v, 20000);

        v = base;
        v.mode = View::Mode::Extras;
        v.screenOffOn = true;
        scene("extras", lang, v, 20000);

        v = base;
        v.mode = View::Mode::Games;
        scene("jogos", lang, v, 20000);

        for (uint8_t i = 0; i < mrm::game::twobtn::kCount; ++i) {
            mrm::game::Game& g = *games.list[i];
            char name[24];
            v = base;
            v.mode = View::Mode::Game;
            v.game.game = &g;
            v.game.index = i;
            v.game.best = 30;
            for (uint8_t page = 0; page < 2; ++page) {
                v.game.phase = mrm::game::GamePhase::Tutorial;
                v.game.page = page;
                snprintf(name, sizeof name, "jogo%u-tutorial%u", i, page + 1);
                scene(name, lang, v, 20000);
            }
            g.reset(7);
            for (int t = 0; t < 400 && !g.over(); ++t) {
                const bool a = (t / 15) % 2 == 0, b = (t / 22) % 3 == 0;
                g.tick({a, b, t % 15 == 0 && a, t % 22 == 0 && b});
            }
            v.game.phase = mrm::game::GamePhase::Playing;
            snprintf(name, sizeof name, "jogo%u-jogando", i);
            scene(name, lang, v, 20000);
            v.game.phase = mrm::game::GamePhase::Over;
            v.game.score = g.score();
            snprintf(name, sizeof name, "jogo%u-fim", i);
            scene(name, lang, v, 20000);
        }
    }
}
