#include "Ui.h"

#include <game/GameScreen.h>

#include "Marmot.h"
#include "MarmotArt.h"
#include <Mp3Tag.h>
#include <PathRules.h>
#include "Text.h"
#include "UiArt.h"
#include "UiDraw.h"
#include "UiMenus.h"

using namespace mrm;
using namespace ::ui;
using namespace layout;

namespace {

constexpr uint32_t kSplashAnimMs = 1800; // fase das ondas, depois só a marmota e o nome
constexpr uint32_t kSlideMs = 250;       // título entrando ao trocar de faixa
constexpr uint32_t kHoldMs = 1200;       // marquee parado nas pontas
constexpr uint32_t kScrollPxPerSec = 36;
constexpr uint32_t kVolumeEdgeMs = 180;
constexpr uint32_t kVolumeTweenMs = 140;
constexpr float kEqPeriodMs = 140.0f;
constexpr float kEqPeriodStepMs = 37.0f;
constexpr float kEqPhaseStep = 1.7f;
constexpr uint32_t kPlaylistMs = 2400;
constexpr uint32_t kWipeMs = 220;
constexpr uint32_t kRollMs = 450;
constexpr uint8_t kWipeBands = 8;
constexpr int16_t kWipeBandH = kH / kWipeBands;

void splash(Panel& o, const View& v, uint32_t now, const Locale&) {
    constexpr int16_t kBurrowY = kSplashIconY + art::kBurrowCenterRow;
    constexpr int16_t kRingR0 = art::kBurrowHalfWidth + 2; // as ondas nascem na borda da toca
    constexpr uint8_t kRingCount = 3;
    constexpr float kRingStagger = 0.1f;
    constexpr float kRingEnd = 0.7f;
    constexpr float kTypeStart = 0.5f;
    constexpr float kTypeEnd = 0.9f;
    constexpr char kSite[] = "marmota.dev.br";

    const uint32_t elapsed = now - v.screenAt;
    const float t = progressOf(elapsed, kSplashAnimMs);

    marmot(o, kCenterX, kSplashIconY, elapsed);

    const size_t shown = size_t(gfx::stage(t, kTypeStart, kTypeEnd) * (sizeof(kSite) - 1)); // digita o site
    char typed[sizeof(kSite)];
    strlcpy(typed, kSite, min(sizeof(typed), shown + 1));
    centered(o, ArialMT_Plain_10, kCenterX, kSplashSiteY, typed);

    o.setColor(INVERSE); // as ondas passam por cima da marmota e do texto, sem apagar nada
    for (uint8_t i = 0; i < kRingCount; ++i) {
        const float p = gfx::stage(t, i * kRingStagger, kRingEnd);
        if (p > 0 && p < 1)
            o.drawCircle(kCenterX, kBurrowY, kRingR0 + int16_t(gfx::easeOut(p) * kSplashRingSpan));
    }

    o.setColor(WHITE);
}

void stripExtension(const char* name, char* out, size_t size) {
    strlcpy(out, name, size);
    const size_t len = strlen(out);
    if (len > 4)
        out[len - 4] = '\0';
}

int16_t slideOffset(const View& v, uint32_t now) {
    return int16_t((1 - eased(now, v.trackAt, kSlideMs)) * kW);
}

// Com artista o título sobe e os controles descem para abrir a linha dele.
int16_t titleY(const View& v) {
    return v.artist[0] ? kTitleYWithArtist : kTitleY;
}

void title(Panel& o, const View& v, uint32_t now) {
    char name[mrm::kMaxTrackLen + 1];
    stripExtension(v.track, name, sizeof(name));
    o.setFont(ArialMT_Plain_16);

    const uint32_t elapsed = v.trackAt ? now - v.trackAt : kSlideMs;
    const int16_t slide = slideOffset(v, now);
    const int16_t width = textWidth(o, name);
    const int16_t y = titleY(v);

    if (width <= kTitleRoom) {
        centered(o, ArialMT_Plain_16, kCenterX + slide, y, name);
        return;
    }

    const uint32_t overflow = width - kTitleRoom;
    const uint32_t travel = overflow * 1000 / kScrollPxPerSec;
    const uint32_t t = elapsed % (2 * kHoldMs + travel);
    const uint32_t shift = t < kHoldMs ? 0 : t < kHoldMs + travel ? (t - kHoldMs) * kScrollPxPerSec / 1000
                                                                  : overflow;

    o.setTextAlignment(TEXT_ALIGN_LEFT);
    o.drawText(kTitleMarginX - int16_t(shift) + slide, y, name);
}

void artistLine(Panel& o, const View& v, uint32_t now) {
    if (!v.artist[0])
        return;

    char text[mrm::kMaxArtistLen + 4];
    strlcpy(text, v.artist, sizeof(text));
    o.setFont(ArialMT_Plain_10);
    fitText(o, text, sizeof(text), kTitleRoom, "...");
    centered(o, ArialMT_Plain_10, kCenterX + slideOffset(v, now), kArtistY, text);
}

void formatClock(const View& v, char* out, size_t size) {
    const uint32_t hours = v.positionSec / 3600;
    if (hours > 0)
        snprintf(out, size, "%lu:%02u:%02u", static_cast<unsigned long>(hours), v.positionSec / 60 % 60,
                 v.positionSec % 60);
    else
        snprintf(out, size, "%u:%02u", v.positionSec / 60, v.positionSec % 60);
}

// Equalizador: só mexe tocando.
void equalizer(Panel& o, int16_t baseY, bool playing, uint32_t now) {
    for (uint8_t i = 0; i < kEqBars; ++i) {
        const float phase = now / (kEqPeriodMs + i * kEqPeriodStepMs) + i * kEqPhaseStep;
        const int16_t h = kEqMinH + (playing ? int16_t((sinf(phase) + 1) * kEqAmplitude) : 0);
        o.fillRect(kEqX + i * kEqPitch, baseY - h, kEqBarW, h);
    }
}

// Faixa de horas e muitas faixas: só o tempo.
void trackIndex(Panel& o, const View& v, int16_t y, const char* clock) {
    char index[16];
    snprintf(index, sizeof(index), "%u/%u", v.trackIndex + 1, v.trackCount);
    const int16_t end = kTrackIndexX + textWidth(o, index) + kTrackIndexGap + textWidth(o, clock);
    if (end < kRightEdgeX)
        o.drawText(kTrackIndexX, y, index);
}

void controls(Panel& o, const View& v, uint32_t now) {
    const int16_t top = v.artist[0] ? kControlsTopWithArtist : kControlsTop;
    const int16_t textY = top + kControlsTextDy;
    const bool playing = v.mode == View::Mode::Playing;

    gfx::drawIcon(o, kPlayIconDy.x, top + kPlayIconDy.y, playing ? art::kPlay : art::kPause);

    char clock[12];
    formatClock(v, clock, sizeof(clock));

    o.setFont(ArialMT_Plain_10);
    o.setTextAlignment(TEXT_ALIGN_LEFT);

    if (v.seekMode) { // A volta e B avança. O tempo à direita é o destino
        o.drawText(kSeekBackX, textY, "A <<");
        o.drawText(kSeekForwardX, textY, ">> B");
    } else {
        equalizer(o, top + kControlsH, playing, now);
        trackIndex(o, v, textY, clock);
    }

    o.setTextAlignment(TEXT_ALIGN_RIGHT);
    o.drawText(kRightEdgeX, textY, clock);

    progressBar(o, kProgressBar, v.progress);
}

// O painel entra uma vez, acompanha o volume enquanto ele muda e só sai depois de um tempo parado.
int16_t volumePanelY(const View& v, uint32_t now) {
    const float enter = 1 - eased(now, v.volumeShownAt, kVolumeEdgeMs);
    const uint32_t idle = now - v.volumeAt;
    const float leave =
        idle > View::kVolumeVisibleMs ? gfx::easeInOut(progressOf(idle - View::kVolumeVisibleMs, kVolumeEdgeMs)) : 0;
    return kH - kVolumePanelH + int16_t(max(enter, leave) * kVolumePanelH);
}

void volumePanel(Panel& o, const View& v, uint32_t now) {
    const int16_t y = volumePanelY(v, now);
    const float level = lerp(v.volumeBefore, v.volume, eased(now, v.volumeAt, kVolumeTweenMs));

    clear(o, 0, y, kW, kVolumePanelH);
    o.drawHorizontalLine(0, y, kW);
    gfx::drawIcon(o, kVolumeSpeaker.x, y + kVolumeSpeaker.y, v.volume == 0 ? art::kSpeakerMute : art::kSpeakerWave);
    progressBar(o, moved(kVolumeBar, y), level);

    char text[8];
    snprintf(text, sizeof(text), "%d", int(lroundf(level)));
    o.setFont(ArialMT_Plain_10);
    o.setTextAlignment(TEXT_ALIGN_RIGHT);
    o.drawText(kRightEdgeX, y + kVolumeTextY, text);
}

int16_t pagerX(uint8_t index, uint8_t count) {
    return kCenterX - (count - 1) * kPagerStep / 2 + index * kPagerStep;
}

// Cortina de faixas descendo.
void wipeIn(Panel& o, uint32_t elapsed) {
    gfx::flapCover(o, kW, uint8_t(ceilf(progressOf(elapsed, kWipeMs) * kWipeBands)), kWipeBandH);
}

// Cortina subindo de novo no fim.
void wipeOut(Panel& o, uint32_t elapsed) {
    const float wipe = progressOf(elapsed - (kPlaylistMs - kWipeMs), kWipeMs);
    const int16_t covered = int16_t(ceilf((1 - wipe) * kWipeBands)) * kWipeBandH;
    clear(o, 0, kH - covered, kW, covered);
}

void rollingNumbers(Panel& o, const View& v, float roll) {
    char number[8];
    o.setFont(ArialMT_Plain_24);
    o.setTextAlignment(TEXT_ALIGN_LEFT);

    snprintf(number, sizeof(number), "%02u", v.playlistPrev + 1);
    o.drawText(kRollX, kRollPrevY - int16_t(roll * kRollTravel), number);

    snprintf(number, sizeof(number), "%02u", v.playlistIndex + 1);
    o.drawText(kRollX, kRollNextY - int16_t(roll * kRollTravel), number);

    clear(o, kRollMask.x, kRollMask.y, kRollMask.w, kRollMask.h);
}

void playlistName(Panel& o, const View& v, const Locale& t, uint32_t body) {
    char count[24];
    snprintf(count, sizeof(count), "%u %s", v.trackCount, t(Txt::Tracks));
    o.setFont(ArialMT_Plain_10);
    o.setTextAlignment(TEXT_ALIGN_RIGHT);
    o.drawText(kPlaylistCount.x, kPlaylistCount.y, count);

    const int16_t slide = int16_t((1 - gfx::easeOut(progressOf(body, kRollMs + 60))) * kW);
    char name[mrm::kMaxPlaylistLen + 1];
    strlcpy(name, v.playlist, sizeof(name));
    centered(o, ArialMT_Plain_16, kCenterX + slide, kPlaylistNameY, name);
}

void pager(Panel& o, const View& v, float roll) {
    const uint8_t dots = min<uint8_t>(v.playlistCount, kPagerMaxDots);
    for (uint8_t i = 0; i < dots; ++i)
        o.fillRect(pagerX(i, dots), kPagerDot.y, kPagerDot.w, kPagerDot.h);

    const float x = lerp(pagerX(v.playlistPrev % dots, dots), pagerX(v.playlistIndex % dots, dots), roll);
    o.fillRect(int16_t(x) + kPagerThumb.x, kPagerThumb.y, kPagerThumb.w, kPagerThumb.h);
}

// Troca de playlist: cortina de faixas, número que rola, nome entrando e bolinhas de paginação.
void playlistChange(Panel& o, const View& v, uint32_t now, const Locale& t) {
    const uint32_t elapsed = now - v.playlistAt;
    if (elapsed < kWipeMs)
        return wipeIn(o, elapsed);

    if (elapsed >= kPlaylistMs - kWipeMs)
        return wipeOut(o, elapsed);

    const uint32_t body = elapsed - kWipeMs;
    const float roll = gfx::easeOut(progressOf(body, kRollMs));

    clear(o, 0, 0, kW, kH);
    rollingNumbers(o, v, roll);
    playlistName(o, v, t, body);
    pager(o, v, roll);
}

void home(Panel& o, const View& v, uint32_t now, const Locale& t) {
    topbar(o, v, now, v.playlist);
    title(o, v, now);
    artistLine(o, v, now);
    controls(o, v, now);

    if (active(now, v.volumeAt, View::kVolumeVisibleMs + kVolumeEdgeMs))
        volumePanel(o, v, now);

    if (active(now, v.playlistAt, kPlaylistMs))
        playlistChange(o, v, now, t);
}

void message(Panel& o, const View& v, uint32_t now, const char* headline, const char* first, const char* second) {
    topbar(o, v, now, "mp3");
    centered(o, ArialMT_Plain_16, kCenterX, kMessageHeadlineY, headline);
    centered(o, ArialMT_Plain_10, kCenterX, kMessageFirstY, first);
    centered(o, ArialMT_Plain_10, kCenterX, kMessageSecondY, second);
}

void btOffScreen(Panel& o, const View& v, uint32_t now, const Locale& t) {
    message(o, v, now, t(Txt::BtOffTitle), t(Txt::BtOffBody), t(Txt::HoldSettings));
}

void noMediaScreen(Panel& o, const View& v, uint32_t now, const Locale& t) {
    message(o, v, now, t(Txt::StorageError), t(Txt::RestartOr), t(Txt::ReplaceCard));
}

// Sem músicas: a dica leva a enviar (Wi-Fi). Sem fone também, a segunda linha alterna as duas dicas.
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

void gameScreen(Panel& o, const View& v, uint32_t now, const Locale& t) {
    game::screen(o, v.game, now, t.lang());
}

using ScreenFn = void (*)(Panel&, const View&, uint32_t, const Locale&);

// Na ordem de View::Mode.
constexpr ScreenFn kScreens[] = {
    splash,
    connecting,
    picker,
    config,
    wifi,
    bluetooth,
    language,
    playlists,
    games,
    gameScreen,
    extras,
    storage,
    btOffScreen,
    emptyOrNoDevice,
    noMediaScreen,
    emptyOrNoDevice,
    home,
    home,
};
static_assert(sizeof(kScreens) / sizeof(kScreens[0]) == size_t(View::Mode::Playing) + 1, "uma tela por modo");

} // namespace

void Ui::draw(const View& view, uint32_t now) {
    Panel& o = display_.raw();
    o.clear();
    kScreens[size_t(view.mode)](o, view, now, locale_);
    toast(o, view, now, locale_);
    o.display();
}
