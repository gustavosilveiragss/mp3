#pragma once

#include <Arduino.h>

#include <game/GameView.h>
#include <game/twobtn/Games.h>
#include <marmota.h>

#include "Audio.h"
#include "Library.h"

// Tudo que a Ui precisa para desenhar um quadro. As animacoes saem dos carimbos de tempo (millis do
// ultimo evento, 0 = nunca): a Ui nao guarda estado nem recebe eventos.
struct View {
    static constexpr uint32_t kVolumeVisibleMs = 1100; // painel de volume espera isto depois do ultimo passo

    static constexpr uint32_t kToastHoldMs = 1300; // aviso fica este tempo depois do ultimo evento
    static constexpr uint32_t kToastEdgeMs = 180;

    enum class Toast : uint8_t { None,
                                 Locked,
                                 Unlocked,
                                 LockedHint,
                                 NoDevice };

    enum class Mode : uint8_t { Splash,
                                Connecting,
                                Picker,
                                Config,
                                Wifi,
                                Bluetooth,
                                Language,
                                Playlists,
                                Games,
                                Game,
                                Extras,
                                Storage,
                                BtOff,
                                NoDevice,
                                NoMedia,
                                Empty,
                                Paused,
                                Playing };

    Mode mode = Mode::Splash;
    uint32_t screenAt = 0; // quando a tela atual entrou

    bool noDevice = false; // Bluetooth ligado e nenhum fone conectado

    const char* deviceCode = ""; // sufixo do nome no Wi-Fi e Bluetooth

    // Barra de topo
    Audio::Link btLink = Audio::Link::Off;
    bool wifiOn = false;
    int8_t battery = -1; // -1 esconde

    // Player
    const char* playlist = "";
    const char* track = ""; // nome do arquivo, com extensao
    uint8_t playlistIndex = 0;
    uint8_t playlistPrev = 0;
    uint8_t playlistCount = 0;
    uint16_t trackIndex = 0;
    uint16_t trackCount = 0;
    uint8_t progress = 0; // 0..100
    uint16_t positionSec = 0;
    uint8_t volume = 0;
    uint8_t volumeBefore = 0;
    uint32_t trackAt = 0;
    uint32_t volumeAt = 0;
    uint32_t volumeShownAt = 0;
    uint32_t playlistAt = 0;

    // Armazenamento: lido ao entrar na tela, nao a cada quadro
    struct Storage {
        bool sd = false; // cartao presente; senao e a memoria interna
        uint64_t freeBytes = 0;
        uint64_t totalBytes = 0;
        uint8_t usedPercent = 0;
        uint8_t playlists = 0;
        uint16_t tracks = 0;
    } storage;

    // Playlists (menu) e jogos
    const Library* library = nullptr;
    uint16_t best[mrm::game::twobtn::kCount]{};
    mrm::game::GameView game; // partida em andamento (Mode::Game)

    // Portal Wi-Fi: o que a pagina esta enviando
    struct Xfer {
        bool active = false; // um arquivo esta chegando
        bool done = false;   // algo foi recebido e nada chega agora
        uint16_t file = 0;   // posicao na fila, a partir de 1
        uint16_t files = 0;
        uint8_t percent = 0; // do arquivo
        uint8_t total = 0;   // da fila
    } xfer;
    bool wifiClient = false; // ha um celular conectado

    // Escolha de fone e conexao
    const char* peer = "";
    const Audio::Device* devices = nullptr;
    uint8_t deviceCount = 0;
    bool scanning = false;
    uint8_t cursor = 0;
    uint8_t cursorPrev = 0;
    uint32_t cursorAt = 0;

    // Aviso por cima de qualquer tela (bloqueio de botoes)
    Toast toast = Toast::None;
    uint32_t toastAt = 0;
    uint32_t toastShownAt = 0;

    // Menus
    mrm::Lang lang = mrm::Lang::PtBr;
    bool btOn = false;
    bool screenOffOn = false;
    bool lockOn = false;
    uint32_t screenOffAt = 0;
    uint32_t lockAt = 0;
    uint32_t btAt = 0;
    uint32_t wifiAt = 0;
};
