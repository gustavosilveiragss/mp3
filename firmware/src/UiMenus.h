#pragma once

#include "UiDraw.h"

// Telas de menu: barra de instruções no topo, título e uma lista rolável.
namespace ui {

// Wrapper que lê o cursor da View. Row, Menu e Hint vêm do fmk.
void menuScreen(mrm::Panel& o, const View& v, uint32_t now, const Menu& m);

void config(mrm::Panel& o, const View& v, uint32_t now, const mrm::Locale& t);
void bluetooth(mrm::Panel& o, const View& v, uint32_t now, const mrm::Locale& t);
void language(mrm::Panel& o, const View& v, uint32_t now, const mrm::Locale& t);
void playlists(mrm::Panel& o, const View& v, uint32_t now, const mrm::Locale& t);
void extras(mrm::Panel& o, const View& v, uint32_t now, const mrm::Locale& t);
void storage(mrm::Panel& o, const View& v, uint32_t now, const mrm::Locale& t);
void games(mrm::Panel& o, const View& v, uint32_t now, const mrm::Locale& t);
void picker(mrm::Panel& o, const View& v, uint32_t now, const mrm::Locale& t);
void wifi(mrm::Panel& o, const View& v, uint32_t now, const mrm::Locale& t);
void connecting(mrm::Panel& o, const View& v, uint32_t now, const mrm::Locale& t);

} // namespace ui
