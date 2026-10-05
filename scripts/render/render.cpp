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
static mrm::Locale locale;
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

static void sceneAt(const char* prefix, unsigned index, mrm::Lang lang, const View& v, uint32_t now) {
    char name[32];
    snprintf(name, sizeof name, "%s%u", prefix, index);
    scene(name, lang, v, now);
}

static void statusScenes(mrm::Lang lang, const View& base) {
    View v = base;
    v.mode = View::Mode::Playing;
    v.wifiOn = true;
    for (unsigned i = 0; i < 6; ++i)
        sceneAt("wifi-quadro", i, lang, v, 20000 + i * 100);
    v.wifiOn = false;
    int8_t levels[] = {100, 50, 10, 0};
    for (int8_t level : levels) {
        v.battery = level;
        for (uint32_t now : {20000u, 20400u})
            sceneAt(now == 20000u ? "bateria-a" : "bateria-b", level, lang, v, now);
    }
    v = base;
    v.mode = View::Mode::Playing;
    v.artist = "Artista com um nome bem comprido demais para caber";
    scene("tocando-artista", lang, v, 20000);
    v.artist = "";
    v.volumeAt = v.volumeShownAt = 19000;
    v.volumeBefore = 5;
    for (uint32_t now : {19050u, 19070u, 19500u, 20150u})
        sceneAt("volume-", now, lang, v, now);
    v.volume = v.volumeBefore = 0;
    scene("volume-mudo", lang, v, 19500);
    v = base;
    v.mode = View::Mode::Playing;
    v.playlistIndex = 1;
    v.playlistPrev = 0;
    v.playlist = "Estudo";
    v.playlistAt = 19000;
    for (uint32_t now : {19100u, 19300u, 19450u, 20000u, 21300u})
        sceneAt("troca-playlist-", now, lang, v, now);
    for (View::Toast toast : {View::Toast::Locked, View::Toast::Unlocked, View::Toast::LockedHint, View::Toast::NoDevice}) {
        v = base;
        v.mode = View::Mode::Playing;
        v.toast = toast;
        v.toastAt = v.toastShownAt = 19000;
        for (uint32_t now : {19100u, 20000u, 20400u}) {
            char name[32];
            snprintf(name, sizeof name, "aviso%u-%u", unsigned(toast), unsigned(now));
            scene(name, lang, v, now);
        }
    }
}

static void menuScenes(mrm::Lang lang, const View& base) {
    View v = base;
    v.mode = View::Mode::Picker;
    v.cursor = v.cursorPrev = 2;
    scene("picker", lang, v, 20000);
    v.scanning = true;
    for (uint32_t now : {20000u, 20350u, 20700u, 21050u})
        sceneAt("picker-busca", now, lang, v, now);
    v = base;
    v.mode = View::Mode::Connecting;
    v.peer = "Fone do Gustavo com nome longo demais";
    for (unsigned i = 0; i < 4; ++i)
        sceneAt("conectando", i, lang, v, 20000 + i * 100);
    for (View::Mode mode : {View::Mode::Bluetooth, View::Mode::Language, View::Mode::BtOff, View::Mode::NoMedia}) {
        v = base;
        v.mode = mode;
        sceneAt("tela", unsigned(mode), lang, v, 20000);
    }
    v = base;
    v.mode = View::Mode::Storage;
    v.storage = {true, 3ull * 1024 * 1024 * 1024, 8ull * 1024 * 1024 * 1024, 62, 3, 169};
    scene("storage-sd", lang, v, 20000);
    v.storage = {false, 900ull * 1024, 1500ull * 1024, 40, 1, 4000};
    scene("storage-interno", lang, v, 20000);
    v.storage = {true, 120ull * 1024 * 1024 * 1024, 128ull * 1024 * 1024 * 1024, 7, 3, 1};
    scene("storage-grande", lang, v, 20000);
}

static void uploadScenes(mrm::Lang lang, const View& base) {
    View v = base;
    v.mode = View::Mode::Wifi;
    scene("wifi-desligado", lang, v, 20000);
    v.wifiOn = true;
    for (uint32_t now : {20000u, 20700u, 21400u})
        sceneAt("upload-espera", now, lang, v, now);
    v.wifiClient = true;
    scene("upload-celular", lang, v, 20000);
    v.xfer.active = true;
    v.xfer.files = 12;
    v.xfer.file = 3;
    v.xfer.total = 40;
    for (uint32_t now : {20000u, 20030u, 20090u})
        sceneAt("upload-enviando", now, lang, v, now);
    v.xfer.files = 1;
    scene("upload-um-arquivo", lang, v, 20000);
    v.xfer = {};
    v.xfer.done = true;
    scene("upload-recebido", lang, v, 20000);
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
        v.mode = View::Mode::Splash;
        for (uint32_t elapsedMs : {0u, 400u, 600u, 1000u, 2000u, 3800u}) {
            char splashName[24];
            snprintf(splashName, sizeof splashName, "splash-%ums", unsigned(elapsedMs));
            scene(splashName, lang, v, base.screenAt + elapsedMs);
        }
        v.mode = View::Mode::Paused;
        scene("home", lang, v, 20000);
        v.mode = View::Mode::Playing;
        scene("tocando", lang, v, 20000);
        v.seekMode = true;
        v.positionSec = 4434;
        scene("busca", lang, v, 20000);
        v.seekMode = false;
        v.trackIndex = 99;
        v.trackCount = 128;
        scene("tocando-horas", lang, v, 20000);
        statusScenes(lang, base);
        menuScenes(lang, base);
        uploadScenes(lang, base);

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
