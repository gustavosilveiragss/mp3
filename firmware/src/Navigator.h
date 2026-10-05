#pragma once

#include "Audio.h"
#include <game/GameHost.h>
#include <ui/Cursor.h>

#include "Keypad.h"
#include "Menus.h"
#include "Player.h"
#include "Portal.h"
#include "Settings.h"
#include "View.h"

// Qual tela está ativa e o que cada gesto significa nela. Em Home os gestos vão para o Player. Nos
// menus as duas teclas só navegam: A desce, B confirma, A+B volta. Dentro de uma partida os gestos
// vão para o GameHost e os jogos leem as teclas cruas.
class Navigator {
public:
    static constexpr uint32_t kSplashMs = 3800; // ondas nos primeiros 1,8 s. Termina no meio do ciclo de 1 s
    static constexpr uint32_t kConnectTimeoutMs = 15000;
    static constexpr uint32_t kResumeMs = 60000;      // espera pelo fone após o Wi-Fi antes de desistir de retomar
    static constexpr uint32_t kRetryMs = 10000;       // reconexão em segundo plano na Home
    static constexpr uint32_t kScreenOffMs = 10000;   // tela apaga parado, com a funcionalidade extra ligada
    static constexpr uint32_t kChordGapMs = 700;      // janela para contar os toques de A+B
    static constexpr uint8_t kLockTaps = 3;           // toques de A+B que travam ou destravam
    static constexpr uint32_t kDeliveredWakeMs = 200; // quanto a troca de playlist entregue mantém a tela acesa

    Navigator(Player& player, Settings& settings, Audio& audio, mrm::Locale& locale,
              mrm::game::GameHost& games, Portal& portal)
        : player_(player)
        , settings_(settings)
        , audio_(audio)
        , locale_(locale)
        , games_(games)
        , portal_(portal) {}

    void begin();
    void update();
    void handle(Keypad::Action action);
    void setHeld(bool a, bool b) {
        aHeld_ = a;
        bHeld_ = b;
    } // teclas cruas, para os jogos

    // busca também: toque na hora
    bool menuInput() const { return screen_ == Screen::Home ? player_.seeking() : screen_ != Screen::Splash; }
    bool wifiRunning() const { return wifiOn_; }
    bool gameRunning() const { return screen_ == Screen::Play && games_.running(); }
    bool idle() const; // display pode apagar
    bool locked() const { return locked_; }
    bool toastActive() const;
    bool wantsDisplay() const;
    // Com o bloqueio ligado, A+B na Home e contado em vez de agir na hora (não acorda a tela).
    bool countsChords() const {
        return settings_.lockEnabled() && screen_ == Screen::Home && !locked_ && !player_.seeking();
    }
    void wake(); // uma tecla acordou o display: conta como atividade
    View view() const;

private:
    friend class PortalSelfTest; // teste do portal embutido, só com -DMP3_TEST_PORTAL=1
    enum class Screen : uint8_t { Splash,
                                  Connecting,
                                  Picker,
                                  Home,
                                  Config,
                                  Playlists,
                                  Games,
                                  Play,
                                  Wifi,
                                  Bluetooth,
                                  Language,
                                  Extras,
                                  Storage };

    void enter(Screen screen, uint8_t cursor = 0);
    void reconnect();
    void openPicker();
    void connectTo(const char* name, const uint8_t* bda = nullptr);
    void connectFailed();
    void retryConnection();
    uint8_t rows() const;
    void select();
    void selectBluetooth();
    void selectDevice();
    void selectConfig();
    void back();
    void toggleBluetooth();
    void startWifi();
    void stopWifi();
    void resumePlayback();
    void setLanguage(mrm::Lang lang);
    void selectExtras();
    void selectPlaylist();
    void selectGames();
    void selectWifi();
    void selectLanguage();
    void updateSplash();
    void updateConnecting();
    void updatePlay();
    void refreshStorage();
    void handleHome(Keypad::Action action);
    void handleLocked(Keypad::Action action);
    void countChord();
    void resolveChord();
    void showToast(View::Toast toast);
    void fillMenus(View& v) const;
    void fillHome(View& v) const;
    static View::Mode modeOf(Screen screen, View::Mode homeMode);

    Player& player_;
    Settings& settings_;
    Audio& audio_;
    mrm::Locale& locale_;
    mrm::game::GameHost& games_;
    Portal& portal_;

    Screen screen_ = Screen::Splash;
    bool pickedFromList_ = false; // a conexão em curso saiu da lista de dispositivos
    uint32_t screenAt_ = 0;
    uint32_t activityAt_ = 0;
    uint32_t retryAt_ = 0;
    mrm::ui::Cursor cursor_;
    uint32_t btAt_ = 0;
    uint32_t wifiAt_ = 0;
    bool wifiOn_ = false;
    bool resumeAfterWifi_ = false; // tocava ao ligar o Wi-Fi: volta a tocar quando o fone reconectar
    bool locked_ = false;
    uint8_t chordTaps_ = 0;
    uint32_t chordAt_ = 0;
    View::Toast toast_ = View::Toast::None;
    uint32_t toastAt_ = 0;
    uint32_t toastShownAt_ = 0;
    uint32_t screenOffAt_ = 0;
    uint32_t lockAt_ = 0;
    uint32_t deliveredAt_ = 0;
    bool aHeld_ = false;
    bool bHeld_ = false;
    View::Storage storage_;
    char code_[5] = "";
};
