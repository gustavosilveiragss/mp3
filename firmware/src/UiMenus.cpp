#include "UiMenus.h"

#include "Device.h"
#include "Library.h"
#include "Menus.h"
#include "SizeFormat.h"
#include "Text.h"

using namespace mrm;

namespace ui {

void menuScreen(Panel& o, const View& v, uint32_t now, const Menu& m) {
    mrm::ui::menuScreen(o, {v.cursor, v.cursorPrev, v.cursorAt}, now, m);
}

void config(Panel& o, const View& v, uint32_t now, const Locale& t) {
    const Hint hintItems[] = {{"A", t(Txt::HintDown)}, {"B", t(Txt::HintOk)}, {"A+B", t(Txt::HintBack)}};
    Row rows[menu::kConfigRows];
    rows[menu::kConfigPlaylists] = {.label = t(Txt::Library), .value = v.library && v.library->playlistCount() ? v.playlist : nullptr};
    rows[menu::kConfigBluetooth] = {.label = t(Txt::Bluetooth), .value = v.btOn ? (v.btLink == Audio::Link::Connected ? v.peer : t(Txt::NoDevice)) : t(Txt::Off)};
    rows[menu::kConfigWifi] = {.label = t(Txt::Wifi), .value = t(v.wifiOn ? Txt::On : Txt::Off)};
    rows[menu::kConfigLanguage] = {.label = t(Txt::Language), .value = Locale::nativeName(v.lang)};
    rows[menu::kConfigExtras] = {.label = t(Txt::ExtraFeatures)};
    rows[menu::kConfigStorage] = {.label = t(Txt::Storage)};
    rows[menu::kConfigGames] = {.label = t(Txt::Games)};
    rows[menu::kConfigExit] = {.label = t(Txt::Exit)};
    menuScreen(o, v, now, {hintItems, 3, t(Txt::Settings), rows, menu::kConfigRows});
}

// Biblioteca: Voltar no topo, depois uma linha por playlist com a quantidade de faixas.
void playlists(Panel& o, const View& v, uint32_t now, const Locale& t) {
    const Hint hintItems[] = {{"A", t(Txt::HintDown)}, {"B", t(Txt::HintOk)}, {"A+B", t(Txt::HintBack)}};
    constexpr uint8_t kMax = Library::kMaxPlaylists;
    Row rows[kMax + menu::kPlaylistsFirst];
    char counts[kMax][5];
    rows[menu::kPlaylistsBack] = Row{t(Txt::Back)};
    const uint8_t n = min<uint8_t>(v.library->playlistCount(), kMax);
    for (uint8_t i = 0; i < n; ++i) {
        snprintf(counts[i], sizeof(counts[i]), "%u", min<uint16_t>(v.library->trackCount(i), 999));
        rows[menu::kPlaylistsFirst + i] = Row{v.library->playlistName(i), counts[i], -1, 1, i == v.playlistIndex};
    }
    menuScreen(o, v, now, {hintItems, 3, t(Txt::Library), rows, static_cast<uint8_t>(n + menu::kPlaylistsFirst)});
}

// O titulo vira a explicacao curtissima da linha em foco, para caber numa tela de 128x64.
void extras(Panel& o, const View& v, uint32_t now, const Locale& t) {
    const Hint hintItems[] = {{"A", t(Txt::HintDown)}, {"B", t(Txt::HintOk)}, {"A+B", t(Txt::HintBack)}};
    Row rows[menu::kExtrasRows];
    rows[menu::kExtrasScreenOff] = Row{t(Txt::ScreenOff), nullptr, v.screenOffOn, eased(now, v.screenOffAt, kMoveMs)};
    rows[menu::kExtrasLock] = Row{t(Txt::ButtonLock), nullptr, v.lockOn, eased(now, v.lockAt, kMoveMs)};
    rows[menu::kExtrasBack] = Row{t(Txt::Back)};
    const Txt help[] = {Txt::ScreenOffHelp, Txt::ButtonLockHelp, Txt::ExtraFeatures};
    menuScreen(o, v, now, {hintItems, 3, t(help[v.cursor % menu::kExtrasRows]), rows, menu::kExtrasRows});
}

// Tela so de leitura: o que ocupa o cartao (ou a memoria interna) e quantas faixas ha.
void storage(Panel& o, const View& v, uint32_t now, const Locale& t) {
    (void)now;
    const Hint hintItems[] = {{"B", t(Txt::HintBack)}, {"A+B", t(Txt::HintBack)}};
    hintBar(o, hintItems, 2);
    const View::Storage& s = v.storage;
    const char sep = v.lang == Lang::PtBr ? ',' : '.';
    char freeText[12], totalText[12], line[40], tracks[16];
    formatSize(freeText, sizeof(freeText), s.freeBytes, sep);
    formatSize(totalText, sizeof(totalText), s.totalBytes, sep);
    o.setFont(ArialMT_Plain_10);
    snprintf(line, sizeof(line), "%s %s %s", freeText, t(Txt::FreeOf), totalText);
    if (textWidth(o, line) > kW - 8)
        snprintf(line, sizeof(line), "%s / %s", freeText, totalText); // numeros grandes: sem a palavra
    snprintf(tracks, sizeof(tracks), "%u %s", min<uint16_t>(s.tracks, 999), t(Txt::Tracks));
    o.setTextAlignment(TEXT_ALIGN_LEFT);
    constexpr int16_t kTitleY = 13, kLine1 = 25, kLine2 = 36, kLine3 = 47;
    o.drawText(4, kTitleY, t(Txt::Storage));
    if (s.sd) {
        o.drawText(4, kLine1, t(Txt::SdCard));
        o.drawText(4, kLine2, line);
    } else {
        o.drawText(4, kLine1, t(Txt::NoSd));
        o.drawText(4, kLine2, t(Txt::Internal));
        o.drawText(4, kLine3, line);
    }
    const char* label = s.sd ? t(Txt::SdCard) : t(Txt::Internal);
    if (textWidth(o, label) + 6 + textWidth(o, tracks) <= kW - 8) { // so se couber ao lado do rotulo
        o.setTextAlignment(TEXT_ALIGN_RIGHT);
        o.drawText(kW - 4, s.sd ? kLine1 : kLine2, tracks);
    }
    o.setTextAlignment(TEXT_ALIGN_LEFT);
    constexpr int16_t kBarY = 58, kBarH = 5;
    o.drawRect(4, kBarY, kW - 8, kBarH);
    o.fillRect(6, kBarY + 2, (kW - 12) * s.usedPercent / 100, kBarH - 4);
}

void games(Panel& o, const View& v, uint32_t now, const Locale& t) {
    const Hint hintItems[] = {{"A", t(Txt::HintDown)}, {"B", t(Txt::HintOk)}, {"A+B", t(Txt::HintBack)}};
    Row rows[menu::kGamesRows];
    char best[mrm::game::twobtn::kCount][6];
    for (uint8_t i = 0; i < mrm::game::twobtn::kCount; ++i) {
        snprintf(best[i], sizeof(best[i]), "%u", v.best[i]);
        rows[i] = Row{game::tr(t.lang(), mrm::game::twobtn::name(i)), v.best[i] ? best[i] : nullptr};
    }
    rows[menu::kGamesBack] = Row{t(Txt::Back)};
    menuScreen(o, v, now, {hintItems, 3, t(Txt::Games), rows, menu::kGamesRows});
}

void bluetooth(Panel& o, const View& v, uint32_t now, const Locale& t) {
    const Hint hintItems[] = {{"A", t(Txt::HintDown)}, {"B", t(Txt::HintOk)}, {"A+B", t(Txt::HintBack)}};
    Row rows[menu::kBluetoothRows];
    rows[menu::kBluetoothToggle] = Row{t(Txt::Bluetooth), nullptr, v.btOn, eased(now, v.btAt, kMoveMs)};
    rows[menu::kBluetoothPick] = Row{t(Txt::PickDevice)};
    rows[menu::kBluetoothBack] = {.label = t(Txt::Back)};
    menuScreen(o, v, now, {hintItems, 3, t(Txt::Bluetooth), rows, menu::kBluetoothRows});
}

void language(Panel& o, const View& v, uint32_t now, const Locale& t) {
    const Hint hintItems[] = {{"A", t(Txt::HintDown)}, {"B", t(Txt::HintOk)}, {"A+B", t(Txt::HintBack)}};
    Row rows[menu::kLanguageRows];
    rows[menu::kLanguagePt] = Row{Locale::nativeName(Lang::PtBr), nullptr, -1, 1, v.lang == Lang::PtBr};
    rows[menu::kLanguageEn] = Row{Locale::nativeName(Lang::En), nullptr, -1, 1, v.lang == Lang::En};
    rows[menu::kLanguageBack] = {.label = t(Txt::Back)};
    menuScreen(o, v, now, {hintItems, 3, t(Txt::Language), rows, menu::kLanguageRows});
}

void picker(Panel& o, const View& v, uint32_t now, const Locale& t) {
    const Hint hintItems[] = {{"A", t(Txt::HintDown)}, {"B", t(Txt::HintOk)}, {"A+B", t(Txt::HintBack)}};
    Row rows[Audio::kMaxDevices + menu::kPickerFirstDevice];
    char rescan[24];
    snprintf(rescan, sizeof(rescan), "%s%.*s", v.scanning ? t(Txt::Scanning) : t(Txt::ScanAgain), v.scanning ? int((now / 350) % 4) : 0, "...");
    rows[menu::kPickerRescan] = Row{rescan};
    rows[menu::kPickerBack] = Row{t(Txt::Back)};
    for (uint8_t i = 0; i < v.deviceCount; ++i) {
        const bool connected = v.btLink == Audio::Link::Connected && strcmp(v.devices[i].name, v.peer) == 0;
        rows[menu::kPickerFirstDevice + i] = Row{v.devices[i].name, nullptr, -1, 1, connected, v.devices[i].rssi};
    }
    menuScreen(o, v, now, {hintItems, 3, t(Txt::Devices), rows, static_cast<uint8_t>(v.deviceCount + menu::kPickerFirstDevice)});
}

namespace {

// Barra de progresso; as faixas escuras que andam mostram que o envio esta vivo.
void bar(Panel& o, int16_t y, int16_t h, uint8_t percent, uint32_t now) {
    constexpr int16_t x = 4;
    o.drawRect(x, y, kW - 8, h);
    const int16_t fill = (kW - 12) * percent / 100;
    o.fillRect(x + 2, y + 2, fill, h - 4);
    o.setColor(BLACK);
    for (int16_t i = int16_t((now / 60) % 8); i < fill; i += 8)
        o.drawVerticalLine(x + 2 + i, y + 2, h - 4);
    o.setColor(WHITE);
}

// "recebendo 3/12", barra do arquivo, barra da fila e o total em numero.
void receiving(Panel& o, const View& v, uint32_t now, const Locale& t) {
    char text[32];
    o.setFont(ArialMT_Plain_10);
    o.setTextAlignment(TEXT_ALIGN_LEFT);
    snprintf(text, sizeof(text), "%s %u/%u", t(Txt::Receiving), v.xfer.file, v.xfer.files);
    o.drawText(4, 14, text);
    o.setTextAlignment(TEXT_ALIGN_RIGHT);
    snprintf(text, sizeof(text), "%u%%", v.xfer.percent);
    o.drawText(kW - 4, 14, text);
    bar(o, 29, 9, v.xfer.percent, now);
    bar(o, 42, 7, v.xfer.total, now);
    o.setTextAlignment(TEXT_ALIGN_LEFT);
    snprintf(text, sizeof(text), "%s %u%%", t(Txt::Total), v.xfer.total);
    o.drawText(4, 51, text);
}

} // namespace

void wifi(Panel& o, const View& v, uint32_t now, const Locale& t) {
    if (!v.wifiOn) {
        const Hint hintItems[] = {{"A", t(Txt::HintDown)}, {"B", t(Txt::HintOk)}, {"A+B", t(Txt::HintBack)}};
        Row rows[menu::kWifiRows];
        rows[menu::kWifiToggle] = {.label = t(Txt::UploadMusic), .toggle = 0, .toggleMove = eased(now, v.wifiAt, kMoveMs)};
        rows[menu::kWifiBack] = {.label = t(Txt::Back)};
        menuScreen(o, v, now, {hintItems, 3, t(Txt::Wifi), rows, menu::kWifiRows});
        return;
    }
    const Hint hintItems[] = {{"B", t(Txt::HintOff)}, {"A+B", t(Txt::HintBack)}};
    hintBar(o, hintItems, 2);
    if (v.xfer.active)
        return receiving(o, v, now, t);
    char ssid[sizeof(device::kName) + 6];
    snprintf(ssid, sizeof(ssid), "%s %s", device::kName, v.deviceCode);
    centered(o, ArialMT_Plain_10, kW / 2, 15, ssid);
    if (v.xfer.done || v.wifiClient) {
        centered(o, ArialMT_Plain_16, kW / 2, 27, t(v.xfer.done ? Txt::Received : Txt::Connected));
        centered(o, ArialMT_Plain_10, kW / 2, 49, t(v.xfer.done ? Txt::SendMore : Txt::PickOnPhone));
        return;
    }
    centered(o, ArialMT_Plain_10, kW / 2, 29, t(Txt::AutoOpens));
    centered(o, ArialMT_Plain_10, kW / 2, 41, t(Txt::OrGoTo));
    const float sweep = gfx::easeInOut(fabsf(fmodf(now / 1400.0f, 2.0f) - 1)); // vai e volta
    gfx::ditherRect(o, 4 + int16_t(sweep * (kW - 8 - 28)), 61, 28, 2);
}

void connecting(Panel& o, const View& v, uint32_t now, const Locale& t) {
    const Hint hintItems[] = {{"A+B", t(Txt::HintCancel)}};
    hintBar(o, hintItems, 1);
    centered(o, ArialMT_Plain_10, kW / 2, 16, t(Txt::ConnectingTo));
    o.setFont(ArialMT_Plain_16);
    char name[34];
    strlcpy(name, v.peer, sizeof(name));
    while (strlen(name) > 1 && textWidth(o, name) > kW - 8) {
        size_t n = strlen(name) - 1;
        while (n > 1 && (name[n] & 0xC0) == 0x80)
            --n;
        name[n] = '\0';
    }
    centered(o, ArialMT_Plain_16, kW / 2, 28, name);
    gfx::wifiArcs(o, kW / 2, 61, (now / 300) % 4, 4);
}

} // namespace ui
