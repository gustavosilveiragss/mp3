#include "UiMenus.h"

#include "Device.h"
#include "Library.h"
#include "Menus.h"
#include "SizeFormat.h"
#include "Text.h"

using namespace mrm;
using namespace layout;

namespace ui {

namespace {

// Dicas das telas de menu: A desce, B escolhe, A+B volta.
struct NavHints {
    Hint items[3];
    explicit NavHints(const Locale& t)
        : items{{"A", t(Txt::HintDown)}, {"B", t(Txt::HintOk)}, {"A+B", t(Txt::HintBack)}} {}
};

// Os pontos de "Procurando..." aparecem um a um, de 0 a 3.
const char* scanDots(uint32_t now) {
    constexpr uint32_t kDotMs = 350;
    static const char* const kDots[] = {"", ".", "..", "..."};
    return kDots[(now / kDotMs) % 4];
}

} // namespace

void menuScreen(Panel& o, const View& v, uint32_t now, const Menu& m) {
    mrm::ui::menuScreen(o, {v.cursor, v.cursorPrev, v.cursorAt}, now, m);
}

void config(Panel& o, const View& v, uint32_t now, const Locale& t) {
    const NavHints hintItems(t);
    Row rows[menu::kConfigRows];

    rows[menu::kConfigPlaylists] = {.label = t(Txt::Library),
                                    .value = v.library && v.library->playlistCount() ? v.playlist : nullptr};
    rows[menu::kConfigBluetooth] = {
        .label = t(Txt::Bluetooth),
        .value = v.btOn ? (v.btLink == Audio::Link::Connected ? v.peer : t(Txt::NoDevice)) : t(Txt::Off)};

    rows[menu::kConfigWifi] = {.label = t(Txt::Wifi), .value = t(v.wifiOn ? Txt::On : Txt::Off)};
    rows[menu::kConfigLanguage] = {.label = t(Txt::Language), .value = Locale::nativeName(v.lang)};

    rows[menu::kConfigExtras] = {.label = t(Txt::ExtraFeatures)};
    rows[menu::kConfigStorage] = {.label = t(Txt::Storage)};
    rows[menu::kConfigGames] = {.label = t(Txt::Games)};
    rows[menu::kConfigExit] = {.label = t(Txt::Exit)};

    menuScreen(o, v, now, {hintItems.items, 3, t(Txt::Settings), rows, menu::kConfigRows});
}

// Biblioteca: Voltar no topo, depois uma linha por playlist com a quantidade de faixas. Só as linhas em volta do
// cursor viram Row (a lista pode ter centenas): a janela acompanha o cursor e a rolagem sai como a de lista inteira.
void playlists(Panel& o, const View& v, uint32_t now, const Locale& t) {
    const NavHints hintItems(t);

    constexpr uint8_t kRows = 8;
    constexpr uint8_t kBehind = 3; // linhas antes do cursor

    const uint8_t total = v.library->playlistCount() + menu::kPlaylistsFirst;
    const uint8_t rowsShown = min<uint8_t>(total, kRows);
    const uint8_t start = min<uint8_t>(v.cursor > kBehind ? v.cursor - kBehind : 0, total - rowsShown);

    Row rows[kRows];
    char counts[kRows][5];
    char names[kRows][mrm::kMaxPlaylistLen + 1]; // a janela da Library troca de nomes no meio do laço

    for (uint8_t i = 0; i < rowsShown; ++i) {
        const uint8_t at = start + i;
        if (at < menu::kPlaylistsFirst) {
            rows[i] = Row{t(Txt::Back)};
            continue;
        }

        const uint8_t playlist = at - menu::kPlaylistsFirst;
        snprintf(counts[i], sizeof(counts[i]), "%u", min<uint16_t>(v.library->trackCount(playlist), 999));
        strlcpy(names[i], v.library->playlistName(playlist), sizeof(names[i]));
        rows[i] = Row{names[i], counts[i], -1, 1, playlist == v.playlistIndex};
    }

    const auto local = [&](uint8_t pos) { return static_cast<uint8_t>(constrain(int(pos) - start, 0, rowsShown - 1)); };
    mrm::ui::menuScreen(o, {local(v.cursor), local(v.cursorPrev), v.cursorAt}, now,
                        {hintItems.items, 3, t(Txt::Library), rows, rowsShown});
}

// O título vira a explicação curtíssima da linha em foco, para caber numa tela de 128x64.
void extras(Panel& o, const View& v, uint32_t now, const Locale& t) {
    const NavHints hintItems(t);
    Row rows[menu::kExtrasRows];

    rows[menu::kExtrasScreenOff] = Row{t(Txt::ScreenOff), nullptr, v.screenOffOn, eased(now, v.screenOffAt, kMoveMs)};
    rows[menu::kExtrasLock] = Row{t(Txt::ButtonLock), nullptr, v.lockOn, eased(now, v.lockAt, kMoveMs)};
    rows[menu::kExtrasBack] = Row{t(Txt::Back)};

    const mrm::Text* const help[] = {&Txt::ScreenOffHelp, &Txt::ButtonLockHelp, &Txt::ExtraFeatures};
    menuScreen(o, v, now, {hintItems.items, 3, t(*help[v.cursor % menu::kExtrasRows]), rows, menu::kExtrasRows});
}

// Tela só de leitura: o que ocupa o cartão (ou a memória interna) e quantas faixas há.
void storage(Panel& o, const View& v, uint32_t, const Locale& t) {
    const Hint hintItems[] = {{"B", t(Txt::HintBack)}, {"A+B", t(Txt::HintBack)}};
    hintBar(o, hintItems, 2);

    const View::Storage& s = v.storage;
    const char sep = v.lang == Lang::PtBr ? ',' : '.';
    char freeText[12], totalText[12], line[40], tracks[16];

    formatSize(freeText, sizeof(freeText), s.freeBytes, sep);
    formatSize(totalText, sizeof(totalText), s.totalBytes, sep);

    o.setFont(ArialMT_Plain_10);
    snprintf(line, sizeof(line), "%s %s %s", freeText, t(Txt::FreeOf), totalText);

    if (textWidth(o, line) > kW - 2 * kMarginX)
        snprintf(line, sizeof(line), "%s / %s", freeText, totalText); // números grandes: sem a palavra

    snprintf(tracks, sizeof(tracks), "%u %s", min<uint16_t>(s.tracks, 999), t(Txt::Tracks));

    o.setTextAlignment(TEXT_ALIGN_LEFT);
    o.drawText(kMarginX, kStorageTitleY, t(Txt::Storage));

    if (s.sd) {
        o.drawText(kMarginX, kStorageLine1Y, t(Txt::SdCard));
        o.drawText(kMarginX, kStorageLine2Y, line);
    } else {
        o.drawText(kMarginX, kStorageLine1Y, t(Txt::NoSd));
        o.drawText(kMarginX, kStorageLine2Y, t(Txt::Internal));
        o.drawText(kMarginX, kStorageLine3Y, line);
    }

    const char* label = s.sd ? t(Txt::SdCard) : t(Txt::Internal);
    if (textWidth(o, label) + kStorageTracksGap + textWidth(o, tracks) <= kW - 2 * kMarginX) { // só se couber ao lado
        o.setTextAlignment(TEXT_ALIGN_RIGHT);
        o.drawText(kW - kMarginX, s.sd ? kStorageLine1Y : kStorageLine2Y, tracks);
    }

    o.setTextAlignment(TEXT_ALIGN_LEFT);
    progressBar(o, kStorageBar, s.usedPercent);
}

void games(Panel& o, const View& v, uint32_t now, const Locale& t) {
    const NavHints hintItems(t);
    Row rows[menu::kGamesRows];
    char best[mrm::game::twobtn::kCount][6];

    for (uint8_t i = 0; i < mrm::game::twobtn::kCount; ++i) {
        snprintf(best[i], sizeof(best[i]), "%u", v.best[i]);
        rows[i] = Row{game::tr(t.lang(), mrm::game::twobtn::name(i)), v.best[i] ? best[i] : nullptr};
    }

    rows[menu::kGamesBack] = Row{t(Txt::Back)};
    menuScreen(o, v, now, {hintItems.items, 3, t(Txt::Games), rows, menu::kGamesRows});
}

void bluetooth(Panel& o, const View& v, uint32_t now, const Locale& t) {
    const NavHints hintItems(t);
    Row rows[menu::kBluetoothRows];

    rows[menu::kBluetoothToggle] = Row{t(Txt::Bluetooth), nullptr, v.btOn, eased(now, v.btAt, kMoveMs)};
    rows[menu::kBluetoothPick] = Row{t(Txt::PickDevice)};
    rows[menu::kBluetoothForget] = Row{t(Txt::ForgetDevice)};
    rows[menu::kBluetoothBack] = {.label = t(Txt::Back)};
    menuScreen(o, v, now, {hintItems.items, 3, t(Txt::Bluetooth), rows, menu::kBluetoothRows});
}

void language(Panel& o, const View& v, uint32_t now, const Locale& t) {
    const NavHints hintItems(t);
    Row rows[menu::kLanguageRows];

    rows[menu::kLanguagePt] = Row{Locale::nativeName(Lang::PtBr), nullptr, -1, 1, v.lang == Lang::PtBr};
    rows[menu::kLanguageEn] = Row{Locale::nativeName(Lang::En), nullptr, -1, 1, v.lang == Lang::En};
    rows[menu::kLanguageBack] = {.label = t(Txt::Back)};
    menuScreen(o, v, now, {hintItems.items, 3, t(Txt::Language), rows, menu::kLanguageRows});
}

void picker(Panel& o, const View& v, uint32_t now, const Locale& t) {
    const NavHints hintItems(t);
    Row rows[Audio::kMaxDevices + menu::kPickerFirstDevice];
    char rescan[24];

    if (v.scanning)
        snprintf(rescan, sizeof(rescan), "%s%s", t(Txt::Scanning), scanDots(now));
    else
        strlcpy(rescan, t(Txt::ScanAgain), sizeof(rescan));

    rows[menu::kPickerRescan] = Row{rescan};
    rows[menu::kPickerBack] = Row{t(Txt::Back)};

    for (uint8_t i = 0; i < v.deviceCount; ++i) {
        const bool connected = v.btLink == Audio::Link::Connected && strcmp(v.devices[i].name, v.peer) == 0;
        rows[menu::kPickerFirstDevice + i] = Row{v.devices[i].name, nullptr, -1, 1, connected, v.devices[i].rssi};
    }

    const uint8_t rowCount = v.deviceCount + menu::kPickerFirstDevice;
    menuScreen(o, v, now, {hintItems.items, 3, t(Txt::Devices), rows, rowCount});
}

namespace {

// Barra de progresso. As faixas escuras que andam mostram que o envio está vivo.
void uploadBar(Panel& o, Rect bar, uint8_t percent, uint32_t now) {
    constexpr uint32_t kStripeMs = 60;
    constexpr int16_t kStripePitch = 8;

    const int16_t fill = progressBar(o, bar, percent);
    o.setColor(BLACK);

    for (int16_t i = int16_t((now / kStripeMs) % kStripePitch); i < fill; i += kStripePitch)
        o.drawVerticalLine(bar.x + kBarPadding + i, bar.y + kBarPadding, bar.h - 2 * kBarPadding);

    o.setColor(WHITE);
}

// "enviando 3/12", uma barra só para a fila inteira e o percentual.
void receiving(Panel& o, const View& v, uint32_t now, const Locale& t) {
    char text[32];

    o.setFont(ArialMT_Plain_10);
    o.setTextAlignment(TEXT_ALIGN_LEFT);
    o.drawText(kMarginX, kReceivingLabelY, t(Txt::Receiving));

    o.setTextAlignment(TEXT_ALIGN_RIGHT);
    snprintf(text, sizeof(text), "%u%%", v.xfer.total);
    o.drawText(kW - kMarginX, kReceivingLabelY, text);

    uploadBar(o, kReceivingBar, v.xfer.total, now);

    if (v.xfer.files > 1) {
        snprintf(text, sizeof(text), "%u/%u", v.xfer.file, v.xfer.files);
        centered(o, ArialMT_Plain_10, kCenterX, kReceivingFilesY, text);
    }
}

// Faixa tracejada que vai e volta: o portal está de pé, esperando o celular.
void waitingSweep(Panel& o, uint32_t now) {
    constexpr float kSweepPeriodMs = 1400.0f;

    const float sweep = gfx::easeInOut(fabsf(fmodf(now / kSweepPeriodMs, 2.0f) - 1));
    const int16_t travel = kW - 2 * kMarginX - kWifiSweep.w;

    gfx::ditherRect(o, kWifiSweep.x + int16_t(sweep * travel), kWifiSweep.y, kWifiSweep.w, kWifiSweep.h);
}

} // namespace

void wifi(Panel& o, const View& v, uint32_t now, const Locale& t) {
    if (!v.wifiOn) {
        const NavHints hintItems(t);
        Row rows[menu::kWifiRows];

        rows[menu::kWifiToggle] = {
            .label = t(Txt::UploadMusic), .toggle = 0, .toggleMove = eased(now, v.wifiAt, kMoveMs)};
        rows[menu::kWifiBack] = {.label = t(Txt::Back)};

        menuScreen(o, v, now, {hintItems.items, 3, t(Txt::Wifi), rows, menu::kWifiRows});
        return;
    }

    const Hint hintItems[] = {{"B", t(Txt::HintOff)}, {"A+B", t(Txt::HintBack)}};
    hintBar(o, hintItems, 2);

    if (v.xfer.active)
        return receiving(o, v, now, t);

    char ssid[sizeof(device::kName) + 6];
    snprintf(ssid, sizeof(ssid), "%s %s", device::kName, v.deviceCode);
    centered(o, ArialMT_Plain_10, kCenterX, kWifiSsidY, ssid);

    if (v.xfer.done || v.wifiClient) {
        centered(o, ArialMT_Plain_16, kCenterX, kWifiStatusY, t(v.xfer.done ? Txt::Received : Txt::Connected));
        centered(o, ArialMT_Plain_10, kCenterX, kWifiStatusHintY, t(v.xfer.done ? Txt::SendMore : Txt::PickOnPhone));
        return;
    }

    centered(o, ArialMT_Plain_10, kCenterX, kWifiOpensY, t(Txt::AutoOpens));
    centered(o, ArialMT_Plain_10, kCenterX, kWifiOrGoToY, t(Txt::OrGoTo));
    waitingSweep(o, now);
}

void connecting(Panel& o, const View& v, uint32_t now, const Locale& t) {
    const Hint hintItems[] = {{"A+B", t(Txt::HintCancel)}};
    hintBar(o, hintItems, 1);

    centered(o, ArialMT_Plain_10, kCenterX, kConnectingLabelY, t(Txt::ConnectingTo));

    o.setFont(ArialMT_Plain_16);
    char name[34];
    strlcpy(name, v.peer, sizeof(name));
    fitText(o, name, sizeof(name), kConnectingNameRoom);
    centered(o, ArialMT_Plain_16, kCenterX, kConnectingNameY, name);

    gfx::drawClip(o, kConnectingWifi.x, kConnectingWifi.y, mrm::ui::glyph::kWifiLarge, now);
}

} // namespace ui
