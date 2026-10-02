#include "Ui.h"

#include <game/GameScreen.h>

#include "Marmot.h"
#include "MarmotIcon.h"
#include <PathRules.h>
#include "Text.h"
#include "UiDraw.h"
#include "UiMenus.h"

using namespace mrm;
using namespace ::ui;

namespace {

constexpr uint32_t kSplashAnimMs = 1800; // fase das ondas; depois so a marmota e o nome
constexpr uint32_t kSlideMs = 250;       // titulo entrando ao trocar de faixa
constexpr uint32_t kHoldMs = 1200;       // marquee parado nas pontas
constexpr uint32_t kScrollPxPerSec = 36;
constexpr uint32_t kVolumeEdgeMs = 180;
constexpr uint32_t kVolumeTweenMs = 140;
constexpr int16_t kVolumePanel = 20;
constexpr uint32_t kPlaylistMs = 2400;
constexpr uint32_t kWipeMs = 220;
constexpr uint32_t kRollMs = 450;
constexpr uint8_t kWipeBands = 8;
constexpr int16_t kWipeBandH = kH / kWipeBands;

// --- intro ---

void splash(Panel& o, const View& v, uint32_t now) {
    constexpr int16_t kIconY = 13;
    constexpr int16_t kBurrowY = kIconY + art::kBurrowRow;
    constexpr int16_t kRingR0 = art::kBurrowHalf[art::kBurrowRy] + 2; // as ondas nascem na borda da toca
    constexpr int16_t kRingSpan = 55;
    constexpr char kSite[] = "marmota.dev.br";
    const uint32_t elapsed = now - v.screenAt;
    const float t = progressOf(elapsed, kSplashAnimMs); // depois da animacao fica so a marmota e o site

    marmot(o, kW / 2, kIconY, elapsed);

    const size_t shown = size_t(gfx::stage(t, 0.5f, 0.9f) * (sizeof(kSite) - 1)); // digita o site
    char typed[sizeof(kSite)];
    strlcpy(typed, kSite, min(sizeof(typed), shown + 1));
    centered(o, ArialMT_Plain_10, kW / 2, 49, typed);

    o.setColor(INVERSE); // as ondas passam por cima da marmota e do texto, sem apagar nada
    for (uint8_t i = 0; i < 3; ++i) {
        const float p = gfx::stage(t, i * 0.1f, 0.7f);
        if (p > 0 && p < 1)
            o.drawCircle(kW / 2, kBurrowY, kRingR0 + int16_t(gfx::easeOut(p) * kRingSpan));
    }
    o.setColor(WHITE);
}

// --- player ---

void stripExtension(const char* name, char* out, size_t size) {
    strlcpy(out, name, size);
    const size_t len = strlen(out);
    if (len > 4)
        out[len - 4] = '\0';
}

void title(Panel& o, const View& v, uint32_t now) {
    char name[mrm::kMaxTrackLen + 1];
    stripExtension(v.track, name, sizeof(name));
    o.setFont(ArialMT_Plain_16);

    const uint32_t elapsed = v.trackAt ? now - v.trackAt : kSlideMs;
    const int16_t slide = int16_t((1 - eased(now, v.trackAt, kSlideMs)) * kW);
    const int16_t width = textWidth(o, name);
    constexpr int16_t kRoom = kW - 4;

    if (width <= kRoom) {
        centered(o, ArialMT_Plain_16, kW / 2 + slide, 18, name);
        return;
    }
    const uint32_t overflow = width - kRoom;
    const uint32_t travel = overflow * 1000 / kScrollPxPerSec;
    const uint32_t t = elapsed % (2 * kHoldMs + travel);
    const uint32_t shift = t < kHoldMs ? 0 : t < kHoldMs + travel ? (t - kHoldMs) * kScrollPxPerSec / 1000
                                                                  : overflow;
    o.setTextAlignment(TEXT_ALIGN_LEFT);
    o.drawText(2 - int16_t(shift) + slide, 18, name);
}

void controls(Panel& o, const View& v, uint32_t now) {
    constexpr int16_t kTop = 38;
    constexpr int16_t kBase = 50;
    const bool playing = v.mode == View::Mode::Playing;

    if (playing)
        o.fillTriangle(2, kTop + 1, 2, kBase, 11, (kTop + kBase) / 2 + 1);
    else {
        o.fillRect(2, kTop + 1, 3, kBase - kTop);
        o.fillRect(8, kTop + 1, 3, kBase - kTop);
    }

    for (uint8_t i = 0; i < 6; ++i) { // equalizador: so mexe tocando
        const int16_t h = playing ? 2 + int16_t((sinf(now / (140.0f + i * 37) + i * 1.7f) + 1) * 6) : 2;
        o.fillRect(20 + i * 5, kBase - h, 3, h);
    }

    char index[16];
    char time[8];
    snprintf(index, sizeof(index), "%u/%u", v.trackIndex + 1, v.trackCount);
    snprintf(time, sizeof(time), "%u:%02u", v.positionSec / 60, v.positionSec % 60);
    o.setFont(ArialMT_Plain_10);
    o.setTextAlignment(TEXT_ALIGN_LEFT);
    o.drawText(58, kTop + 2, index);
    o.setTextAlignment(TEXT_ALIGN_RIGHT);
    o.drawText(kW - 2, kTop + 2, time);

    o.drawRect(0, 56, kW, 7);
    o.fillRect(2, 58, (kW - 4) * v.progress / 100, 3);
}

// O painel entra uma vez, acompanha o volume enquanto ele muda e so sai depois de um tempo parado.
void volumePanel(Panel& o, const View& v, uint32_t now) {
    const float enter = 1 - eased(now, v.volumeShownAt, kVolumeEdgeMs);
    const uint32_t idle = now - v.volumeAt;
    const float leave = idle > View::kVolumeVisibleMs ? gfx::easeInOut(progressOf(idle - View::kVolumeVisibleMs, kVolumeEdgeMs)) : 0;
    const int16_t y = kH - kVolumePanel + int16_t(max(enter, leave) * kVolumePanel);

    const float level = lerp(v.volumeBefore, v.volume, eased(now, v.volumeAt, kVolumeTweenMs));

    clear(o, 0, y, kW, kVolumePanel);
    o.drawHorizontalLine(0, y, kW);
    o.fillRect(4, y + 8, 3, 5);
    o.fillTriangle(7, y + 8, 12, y + 4, 12, y + 17);
    if (v.volume == 0) {
        o.drawLine(15, y + 7, 21, y + 14);
        o.drawLine(21, y + 7, 15, y + 14);
    } else {
        o.drawCircle(12, y + 10, 5); // onda sonora, so a metade da direita aparece sobre o preto
        clear(o, 4, y + 4, 8, 14);
        o.fillRect(4, y + 8, 3, 5);
        o.fillTriangle(7, y + 8, 12, y + 4, 12, y + 17);
    }
    o.drawRect(26, y + 8, 74, 6);
    o.fillRect(28, y + 10, int16_t(70 * level / 100), 2);

    char text[8];
    snprintf(text, sizeof(text), "%d", int(lroundf(level)));
    o.setFont(ArialMT_Plain_10);
    o.setTextAlignment(TEXT_ALIGN_RIGHT);
    o.drawText(kW - 2, y + 5, text);
}

int16_t pagerX(uint8_t index, uint8_t count) {
    constexpr int16_t kStep = 9;
    return kW / 2 - (count - 1) * kStep / 2 + index * kStep;
}

// Troca de playlist: cortina de faixas, numero que rola, nome entrando e bolinhas de paginacao; a
// cortina sobe de novo no fim.
void playlistChange(Panel& o, const View& v, uint32_t now, const Locale& t) {
    const uint32_t elapsed = now - v.playlistAt;
    if (elapsed < kWipeMs) {
        gfx::flapCover(o, kW, uint8_t(ceilf(progressOf(elapsed, kWipeMs) * kWipeBands)), kWipeBandH);
        return;
    }
    const uint32_t body = elapsed - kWipeMs;
    if (elapsed >= kPlaylistMs - kWipeMs) {
        const int16_t covered = int16_t(ceilf((1 - progressOf(elapsed - (kPlaylistMs - kWipeMs), kWipeMs)) * kWipeBands)) * kWipeBandH;
        clear(o, 0, kH - covered, kW, covered);
        return;
    }
    clear(o, 0, 0, kW, kH);

    char number[8];
    const float roll = gfx::easeOut(progressOf(body, kRollMs));
    o.setFont(ArialMT_Plain_24);
    o.setTextAlignment(TEXT_ALIGN_LEFT);
    snprintf(number, sizeof(number), "%02u", v.playlistPrev + 1);
    o.drawText(6, 2 - int16_t(roll * 26), number);
    snprintf(number, sizeof(number), "%02u", v.playlistIndex + 1);
    o.drawText(6, 28 - int16_t(roll * 26), number);
    clear(o, 0, 30, kW, kH - 30); // a mascara corta o numero que sobra embaixo

    char count[24];
    snprintf(count, sizeof(count), "%u %s", v.trackCount, t(Txt::Tracks));
    o.setFont(ArialMT_Plain_10);
    o.setTextAlignment(TEXT_ALIGN_RIGHT);
    o.drawText(kW - 4, 14, count);

    const int16_t slide = int16_t((1 - gfx::easeOut(progressOf(body, kRollMs + 60))) * kW);
    char name[mrm::kMaxPlaylistLen + 1];
    strlcpy(name, v.playlist, sizeof(name));
    centered(o, ArialMT_Plain_16, kW / 2 + slide, 34, name);

    const uint8_t dots = min<uint8_t>(v.playlistCount, 12);
    for (uint8_t i = 0; i < dots; ++i)
        o.fillRect(pagerX(i, dots), 58, 2, 2);
    const float x = lerp(pagerX(v.playlistPrev % dots, dots), pagerX(v.playlistIndex % dots, dots), roll);
    o.fillRect(int16_t(x) - 3, 57, 8, 4);
}

void home(Panel& o, const View& v, uint32_t now, const Locale& t) {
    topbar(o, v, now, v.playlist);
    title(o, v, now);
    controls(o, v, now);
    if (active(now, v.volumeAt, View::kVolumeVisibleMs + kVolumeEdgeMs))
        volumePanel(o, v, now);
    if (active(now, v.playlistAt, kPlaylistMs))
        playlistChange(o, v, now, t);
}

void message(Panel& o, const View& v, uint32_t now, const char* headline, const char* first, const char* second) {
    topbar(o, v, now, "mp3");
    centered(o, ArialMT_Plain_16, kW / 2, 18, headline);
    centered(o, ArialMT_Plain_10, kW / 2, 40, first);
    centered(o, ArialMT_Plain_10, kW / 2, 51, second);
}

// Sem musicas: a dica leva a enviar (Wi-Fi). Sem fone tambem, a segunda linha alterna as duas dicas.
void emptyOrNoDevice(Panel& o, const View& v, uint32_t now, const Locale& t) {
    constexpr uint32_t kAlternateMs = 3000;
    if (v.mode == View::Mode::NoDevice) {
        message(o, v, now, t(Txt::NoDeviceTitle), t(Txt::HoldConnect), "");
        return;
    }
    if (!v.noDevice) {
        message(o, v, now, t(Txt::NoMusic), t(Txt::HoldSend), "");
        return;
    }
    const bool connectHint = (now / kAlternateMs) % 2;
    message(o, v, now, t(Txt::NoMusic), t(Txt::NoDeviceTitle), t(connectHint ? Txt::HoldConnect : Txt::HoldSend));
}

} // namespace

void Ui::draw(const View& view, uint32_t now) {
    Panel& o = display_.raw();
    const Locale& t = locale_;
    o.clear();
    switch (view.mode) {
    case View::Mode::Splash:
        splash(o, view, now);
        break;
    case View::Mode::Connecting:
        connecting(o, view, now, t);
        break;
    case View::Mode::Picker:
        picker(o, view, now, t);
        break;
    case View::Mode::Config:
        config(o, view, now, t);
        break;
    case View::Mode::Wifi:
        wifi(o, view, now, t);
        break;
    case View::Mode::Bluetooth:
        bluetooth(o, view, now, t);
        break;
    case View::Mode::Language:
        language(o, view, now, t);
        break;
    case View::Mode::Playlists:
        playlists(o, view, now, t);
        break;
    case View::Mode::Games:
        games(o, view, now, t);
        break;
    case View::Mode::Extras:
        extras(o, view, now, t);
        break;
    case View::Mode::Storage:
        storage(o, view, now, t);
        break;
    case View::Mode::Game:
        game::screen(o, view.game, now, t.lang());
        break;
    case View::Mode::BtOff:
        message(o, view, now, t(Txt::BtOffTitle), t(Txt::BtOffBody), t(Txt::HoldSettings));
        break;
    case View::Mode::NoMedia:
        message(o, view, now, t(Txt::StorageError), t(Txt::RestartOr), t(Txt::ReplaceCard));
        break;
    case View::Mode::NoDevice:
    case View::Mode::Empty:
        emptyOrNoDevice(o, view, now, t);
        break;
    case View::Mode::Paused:
    case View::Mode::Playing:
        home(o, view, now, t);
        break;
    }
    toast(o, view, now, t);
    o.display();
}
