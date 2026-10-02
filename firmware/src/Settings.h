#pragma once

#include <game/GameStore.h>
#include <game/twobtn/Games.h>
#include <marmota.h>

#include <PathRules.h>

// Preferencias que sobrevivem ao reboot. A flash tem vida curta, entao a escrita espera o valor
// parar de mudar (volume muda a cada passo) em vez de gravar a cada alteracao. Volume, faixa e fone
// mudam tocando e vao para a NVS: so o que mudou, uma entrada de 32 B (~3,5 ms medidos, sem buraco
// no audio); a pagina de 4 KB so e apagada a cada ~126 entradas. O resto fica no config.json.
class Settings : public mrm::game::GameStore {
public:
    static constexpr uint8_t kMaxVolume = 100;
    static constexpr uint8_t kGames = mrm::game::twobtn::kCount;

    void begin();
    void update();
    // Segura o config.json com o fone conectado: apagar um setor congela o cache nos dois nucleos por
    // ~80 ms, e com o A2DP no ar isso deixou a pilha mandando 2/3 do audio por minutos (engasgo).
    void defer(bool on) { defer_ = on; }

    uint8_t volume() const { return volume_; }
    void setVolume(uint8_t volume);

    // Playlist por nome: o indice muda quando uma playlist entra ou sai.
    const char* playlist() const { return playlist_; }
    uint16_t track() const { return track_; }
    void setPosition(const char* playlist, uint16_t track);

    mrm::Lang lang() const { return lang_; }
    void setLang(mrm::Lang lang);

    // Recorde de cada jogo; setBest so grava (e devolve true) quando a pontuacao e maior.
    uint16_t best(uint8_t game) const override { return game < kGames ? best_[game] : 0; }
    bool setBest(uint8_t game, uint16_t score) override;
    // O tutorial de cada jogo aparece so na primeira vez; depois fica em Como jogar.
    bool tutorialSeen(uint8_t game) const override { return seen_ & (1u << game); }
    void setTutorialSeen(uint8_t game) override;

    // Funcionalidades extras, desligadas por padrao.
    bool screenOff() const { return screenOff_; } // apaga a tela depois de 10 s parado, ate tocando
    void setScreenOff(bool on);
    bool lockEnabled() const { return lockEnabled_; } // A+B 3x trava e destrava os botoes
    void setLockEnabled(bool on);

    bool bluetooth() const { return bluetooth_; }
    void setBluetooth(bool on);
    // Ultimo fone conectado, para reconectar sozinho no boot.
    const char* device() const { return device_; }
    // BDA do ultimo fone (nullptr se nunca gravado): reconecta sem esperar a varredura achar o nome.
    const uint8_t* deviceBda() const { return hasBda_ ? bda_ : nullptr; }
    void setDevice(const char* name, const uint8_t* bda = nullptr);

private:
    void touch(bool hot = false);
    void loadHot();
    void saveHot();

    mrm::Config config_;
    uint8_t volume_ = 50;
    char playlist_[mrm::kMaxPlaylistLen + 1] = "";
    uint16_t track_ = 0;
    mrm::Lang lang_ = mrm::Lang::PtBr;
    uint16_t best_[kGames]{};
    uint8_t seen_ = 0;
    bool screenOff_ = false;
    bool lockEnabled_ = false;
    bool bluetooth_ = true;
    char device_[33] = "";
    uint8_t bda_[6]{};
    bool hasBda_ = false;
    bool dirty_ = false;
    bool hot_ = false;
    bool defer_ = false;
    uint32_t changedAt_ = 0;
};
