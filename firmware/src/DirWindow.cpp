#include "DirWindow.h"

void DirWindow::open(const char* dir) {
    strlcpy(dir_, dir, sizeof(dir_));
    total_ = dir_[0] ? dirlist::count(dir_, dirs_) : 0;
    first_ = count_ = 0;
}

int DirWindow::slot(uint16_t index) const {
    return index >= first_ && index < first_ + count_ ? index - first_ : -1;
}

const char* DirWindow::name(uint16_t index) {
    if (index >= total_)
        return "";
    if (slot(index) < 0)
        fill(index);
    const int s = slot(index);
    return s < 0 ? "" : names_[s];
}

uint16_t& DirWindow::aux(uint16_t index) {
    static uint16_t none;
    none = kUnknown;
    if (index >= total_)
        return none;
    name(index);
    const int s = slot(index);
    return s < 0 ? none : aux_[s];
}

bool DirWindow::indexOf(const char* name, uint16_t& index) const {
    bool found = false;
    index = 0;
    dirlist::each(dir_, [&](const dirlist::Entry& e) {
        if (dirlist::valid(e, dirs_)) {
            const int c = dirlist::order(e.name, name);
            index += c < 0;
            found |= c == 0;
        }
        return true;
    });
    return found;
}

// Recarrega a janela para conter `index`. Um nome da janela atual serve de ancora quando o pedido cai a ate kSlots
// dela (cursor andando de um em um, ou as linhas de um menu que cruzam a borda): uma passada, e as contagens
// dos nomes que ficam sao mantidas. Saltos longos andam do comeco ou do fim.
void DirWindow::fill(uint16_t index) {
    if (count_ && index > first_ && index <= first_ + count_)
        return load(names_[index - first_ - 1], true, index);
    if (count_ && index < first_ && index + kSlots >= first_ && index + kSlots - first_ < count_)
        return load(names_[index + kSlots - first_], false, index + kSlots);
    if (index + kSlots >= total_)
        return load("", false, total_);
    load("", true, 0);
    while (index >= first_ + count_ && count_)
        load(names_[count_ - 1], true, first_ + count_);
}

// `anchor`: indice do primeiro nome carregado (forward) ou o seguinte ao ultimo (backward).
void DirWindow::load(const char* bound, bool forward, uint16_t anchor) {
    char fresh[kSlots][dirlist::kName];
    uint16_t freshAux[kSlots];
    const uint8_t got = dirlist::chunk({dir_, dirs_, bound, forward}, {fresh, nullptr, kSlots});
    for (uint8_t i = 0; i < got; ++i) {
        freshAux[i] = kUnknown;
        for (uint8_t j = 0; j < count_; ++j) {
            if (strcmp(names_[j], fresh[i]) == 0)
                freshAux[i] = aux_[j];
        }
    }
    memcpy(names_, fresh, got * sizeof(fresh[0]));
    memcpy(aux_, freshAux, got * sizeof(freshAux[0]));
    first_ = forward ? anchor : anchor > got ? anchor - got
                                             : 0; // pasta que cresceu depois do open nao pode dar volta
    count_ = got;
    if (!got)
        total_ = 0; // a pasta sumiu ou esvaziou: sem isto cada consulta varreria de novo
}
