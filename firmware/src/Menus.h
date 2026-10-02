#pragma once

#include <Arduino.h>

#include <game/twobtn/Games.h>

// Posicao de cada linha nos menus, para o Navigator agir e a Ui desenhar na mesma ordem.
namespace menu {

constexpr uint8_t kConfigPlaylists = 0;
constexpr uint8_t kConfigBluetooth = 1;
constexpr uint8_t kConfigWifi = 2;
constexpr uint8_t kConfigLanguage = 3;
constexpr uint8_t kConfigExtras = 4;
constexpr uint8_t kConfigStorage = 5;
constexpr uint8_t kConfigGames = 6;
constexpr uint8_t kConfigExit = 7;
constexpr uint8_t kConfigRows = 8;

constexpr uint8_t kBluetoothToggle = 0;
constexpr uint8_t kBluetoothPick = 1;
constexpr uint8_t kBluetoothBack = 2;
constexpr uint8_t kBluetoothRows = 3;

constexpr uint8_t kWifiToggle = 0;
constexpr uint8_t kWifiBack = 1;
constexpr uint8_t kWifiRows = 2;

// Lista de dispositivos: as acoes ficam no topo para os indices nao andarem quando a busca acha um.
constexpr uint8_t kPickerRescan = 0;
constexpr uint8_t kPickerBack = 1;
constexpr uint8_t kPickerFirstDevice = 2;

// Playlists: Voltar no topo e depois uma linha por playlist.
constexpr uint8_t kPlaylistsBack = 0;
constexpr uint8_t kPlaylistsFirst = 1;

// Funcionalidades extras: dois interruptores e Voltar.
constexpr uint8_t kExtrasScreenOff = 0;
constexpr uint8_t kExtrasLock = 1;
constexpr uint8_t kExtrasBack = 2;
constexpr uint8_t kExtrasRows = 3;

// Jogos: um por linha e Voltar no fim.
constexpr uint8_t kGamesBack = mrm::game::twobtn::kCount;
constexpr uint8_t kGamesRows = mrm::game::twobtn::kCount + 1;

constexpr uint8_t kLanguagePt = 0;
constexpr uint8_t kLanguageEn = 1;
constexpr uint8_t kLanguageBack = 2;
constexpr uint8_t kLanguageRows = 3;

} // namespace menu
