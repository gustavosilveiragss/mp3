// Pareamentos do Bluetooth guardados no radio: refaz o de um fone que recusa e apaga todos a pedido.
#include <esp_gap_bt_api.h>

#include "Audio.h"
#include "AudioCore.h"
#include "AudioDebug.h"

namespace {
constexpr uint32_t kQuickFailMs = 3000; // fone ausente demora ~5 s para falhar; recusa em menos disso e chave de pareamento velha
constexpr uint32_t kMaxBonds = 8;
} // namespace

using audio::bt;

// Recusa rapida de fone conhecido: a chave guardada aqui nao vale mais la. Apaga so esse pareamento e tenta uma vez como novo.
void Audio::repairBond(uint32_t now) {
    using S = mrm::BtSource::State;
    if (repaired_ || switching_ || link_ != Link::Connecting || bt.state() != S::Failed || now - dialAt_ >= kQuickFailMs)
        return;
    repaired_ = true;
    AUD_EV("recusa rapida: pareia de novo");
    esp_bt_gap_remove_bond_device(peerBda_);
    bt.connect(peerBda_);
}

void Audio::forgetAll() {
    if (link_ == Link::Off)
        return;
    disconnect();
    esp_bd_addr_t list[kMaxBonds];
    for (int round = 0; round < 4; ++round) { // a lista pode ter mais que kMaxBonds
        int count = kMaxBonds;
        if (esp_bt_gap_get_bond_device_list(&count, list) != ESP_OK || count == 0)
            return;
        for (int i = 0; i < count; ++i)
            esp_bt_gap_remove_bond_device(list[i]);
    }
}
