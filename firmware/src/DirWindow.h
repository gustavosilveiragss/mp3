#pragma once

#include <Arduino.h>

#include "DirScan.h"

// Janela deslizante sobre os nomes de uma pasta: so os kSlots em volta do cursor ficam na RAM,
// o resto e lido do cartao quando o cursor chega perto. Cada nome carrega um valor auxiliar (contagem de faixas).
class DirWindow {
public:
    static constexpr uint8_t kSlots = 8;
    static constexpr uint16_t kUnknown = 0xFFFF;

    explicit DirWindow(bool dirs)
        : dirs_(dirs) {}

    void open(const char* dir); // conta e esvazia a janela
    uint16_t total() const { return total_; }
    // Os ponteiros e a referencia valem ate a proxima consulta, que pode recarregar a janela: quem precisa guardar copia.
    const char* name(uint16_t index);                      // vazio fora de [0, total)
    bool indexOf(const char* name, uint16_t& index) const; // posicao de um nome que existe
    uint16_t& aux(uint16_t index);                         // valor da linha, kUnknown ate alguem preencher

private:
    int slot(uint16_t index) const;
    void fill(uint16_t start);
    void load(const char* bound, bool forward, uint16_t first);

    const bool dirs_;
    char dir_[mrm::kMaxPlaylistLen + sizeof("/music/") + 1] = "";
    char names_[kSlots][dirlist::kName];
    uint16_t aux_[kSlots];
    uint16_t first_ = 0;
    uint8_t count_ = 0;
    uint16_t total_ = 0;
};
