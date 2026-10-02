#pragma once

#include <marmota.h>
#include <ui/Menu.h>

#include "View.h"

// Pecas de desenho comuns a todas as telas do OLED de 128x64.
namespace ui {

using mrm::ui::centered;
using mrm::ui::clear;
using mrm::ui::clipped;
using mrm::ui::eased;
using mrm::ui::Hint;
using mrm::ui::hintBar;
using mrm::ui::kBar;
using mrm::ui::kBodyY;
using mrm::ui::kH;
using mrm::ui::kMoveMs;
using mrm::ui::kRow;
using mrm::ui::kW;
using mrm::ui::lerp;
using mrm::ui::Menu;
using mrm::ui::progressOf;
using mrm::ui::Row;
using mrm::ui::textWidth;

bool active(uint32_t now, uint32_t at, uint32_t duration);

// Barra de topo da Home: titulo a esquerda, status (Bluetooth, Wi-Fi, bateria) a direita.
void topbar(mrm::Panel& o, const View& v, uint32_t now, const char* left);

// Aviso por cima de qualquer tela (bloqueio de botoes): desce do topo, fica um instante e sobe.
void toast(mrm::Panel& o, const View& v, uint32_t now, const mrm::Locale& t);

} // namespace ui
