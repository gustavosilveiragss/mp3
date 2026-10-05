#pragma once

#include <stdint.h>
#include <ui/Draw.h>

// Posições e tamanhos fixos das telas de 128x64, por tela e elemento. Coordenada "Dy" é deslocamento
// a partir de outro elemento. As demais são absolutas na tela (ou no painel, quando o nome diz).
namespace layout {

struct Point {
    int16_t x, y; // lint: ok coordenada
};
struct Rect {
    int16_t x, y, w, h; // lint: ok coordenada
};

using mrm::ui::kH;
using mrm::ui::kW;
constexpr int16_t kCenterX = kW / 2;
constexpr int16_t kRightEdgeX = kW - 2; // texto alinhado a direita nas telas do player
constexpr int16_t kMarginX = 4;         // margem lateral das telas de menu e envio
constexpr int16_t kBarPadding = 2;      // folga entre a moldura da barra e o preenchimento

constexpr Rect moved(Rect rect, int16_t offsetY) {
    return {rect.x, int16_t(rect.y + offsetY), rect.w, rect.h};
}

// Splash
constexpr int16_t kSplashIconY = 13;
constexpr int16_t kSplashSiteY = 49;
constexpr int16_t kSplashRingSpan = 55;

// Barra de topo
constexpr Point kTopbarTitle = {2, 1};
constexpr int16_t kStatusGap = 5;
constexpr int16_t kStatusY = 2;
constexpr int16_t kBluetoothY = 1;
constexpr int16_t kWifiSlotW = 12;
constexpr int16_t kWifiBaseY = 9;
constexpr int16_t kWifiBaseOffsetX = 6;
constexpr Rect kBatteryFill = {2, 2, 8, 3}; // w e a largura com a bateria cheia

// Player
constexpr int16_t kTitleY = 18;
constexpr int16_t kTitleYWithArtist = 12;
constexpr int16_t kArtistY = 28;
constexpr int16_t kTitleMarginX = 2;
constexpr int16_t kTitleRoom = kW - 2 * kTitleMarginX;

constexpr int16_t kControlsTop = 38;
constexpr int16_t kControlsTopWithArtist = 41;
constexpr int16_t kControlsH = 12;
constexpr Point kPlayIconDy = {2, 1};
constexpr int16_t kControlsTextDy = 2;
constexpr int16_t kSeekBackX = 16;
constexpr int16_t kSeekForwardX = 52;
constexpr int16_t kTrackIndexX = 58;
constexpr int16_t kTrackIndexGap = 4; // entre o índice da faixa e o tempo
constexpr uint8_t kEqBars = 6;
constexpr int16_t kEqX = 20;
constexpr int16_t kEqPitch = 5;
constexpr int16_t kEqBarW = 3;
constexpr int16_t kEqMinH = 2;
constexpr int16_t kEqAmplitude = 6;
constexpr Rect kProgressBar = {0, 56, kW, 7};

// Painel de volume (y relativo ao topo do painel)
constexpr int16_t kVolumePanelH = 20;
constexpr Point kVolumeSpeaker = {4, 4};
constexpr Rect kVolumeBar = {26, 8, 74, 6};
constexpr int16_t kVolumeTextY = 5;

// Troca de playlist
constexpr int16_t kRollX = 6;
constexpr int16_t kRollPrevY = 2;
constexpr int16_t kRollNextY = 28;
constexpr int16_t kRollTravel = kRollNextY - kRollPrevY;
constexpr Rect kRollMask = {0, 30, kW, kH - 30}; // esconde o número que sobra embaixo
constexpr Point kPlaylistCount = {kW - 4, 14};   // x do texto alinhado a direita
constexpr int16_t kPlaylistNameY = 34;
constexpr uint8_t kPagerMaxDots = 12;
constexpr int16_t kPagerStep = 9;
constexpr Rect kPagerDot = {0, 58, 2, 2};
constexpr Rect kPagerThumb = {-3, 57, 8, 4}; // x relativo ao centro da bolinha atual

// Mensagens (sem fone, sem músicas, erro de cartão)
constexpr int16_t kMessageHeadlineY = 18;
constexpr int16_t kMessageFirstY = 40;
constexpr int16_t kMessageSecondY = 51;

// Aviso
constexpr int16_t kToastRestY = 20;
constexpr int16_t kToastBoxH = 22;
constexpr int16_t kToastPadBare = 12;
constexpr int16_t kToastPadWithLock = 28;
constexpr int16_t kToastLockShift = 6; // texto afastado do cadeado
constexpr Point kToastLock = {7, 6};
constexpr int16_t kToastTextDy = 4;

// Armazenamento
constexpr int16_t kStorageTitleY = 13;
constexpr int16_t kStorageLine1Y = 25;
constexpr int16_t kStorageLine2Y = 36;
constexpr int16_t kStorageLine3Y = 47;
constexpr int16_t kStorageTracksGap = 6;
constexpr Rect kStorageBar = {kMarginX, 58, kW - 2 * kMarginX, 5};

// Wi-Fi e envio
constexpr int16_t kWifiSsidY = 15;
constexpr int16_t kWifiStatusY = 27;
constexpr int16_t kWifiStatusHintY = 49;
constexpr int16_t kWifiOpensY = 29;
constexpr int16_t kWifiOrGoToY = 41;
constexpr Rect kWifiSweep = {kMarginX, 61, 28, 2}; // x e a largura do trilho vêm de kW
constexpr int16_t kReceivingLabelY = 14;
constexpr Rect kReceivingBar = {kMarginX, 30, kW - 2 * kMarginX, 12};
constexpr int16_t kReceivingFilesY = 46;

// Conexão com o fone
constexpr int16_t kConnectingLabelY = 16;
constexpr int16_t kConnectingNameY = 28;
constexpr Point kConnectingWifi = {kCenterX, 61};
constexpr int16_t kConnectingNameRoom = kW - 8;

} // namespace layout
