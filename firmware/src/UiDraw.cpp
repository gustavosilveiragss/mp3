#include "UiDraw.h"

#include <PathRules.h>
#include "Text.h"

using namespace mrm;

namespace ui {

bool active(uint32_t now, uint32_t at, uint32_t duration) {
    return at != 0 && now - at < duration;
}

// --- barra de topo: titulo a esquerda, status a direita (bluetooth, wi-fi, bateria) ---

static void bluetoothGlyph(Panel& o, int16_t x, int16_t y) {
    o.drawLine(x, y + 2, x + 6, y + 6);
    o.drawLine(x + 6, y + 6, x + 3, y + 8);
    o.drawLine(x + 3, y + 8, x + 3, y);
    o.drawLine(x + 3, y, x + 6, y + 2);
    o.drawLine(x + 6, y + 2, x, y + 6);
}

static void batteryGlyph(Panel& o, int16_t x, int16_t y, uint8_t level, uint32_t now) {
    o.drawRect(x, y, 12, 7);
    o.fillRect(x + 12, y + 2, 2, 3);
    if (level < 15 && (now / 400) % 2) // quase no fim: pisca vazia
        return;
    o.fillRect(x + 2, y + 2, (8 * level + 50) / 100, 3);
}

void topbar(Panel& o, const View& v, uint32_t now, const char* left) {
    o.setColor(WHITE);
    o.fillRect(0, 0, kW, kBar);
    o.setColor(BLACK);

    int16_t x = kW - 2;
    if (v.battery >= 0) {
        x -= 14;
        batteryGlyph(o, x, 2, v.battery, now);
        x -= 5;
    }
    if (v.wifiOn) {
        x -= 12;
        gfx::wifiArcs(o, x + 6, 9, (now / 250) % 4, 2);
        x -= 5;
    }
    if (v.btLink != Audio::Link::Off) {
        const bool shown = v.btLink == Audio::Link::Connected || (now / (v.btLink == Audio::Link::Connecting ? 250 : 700)) % 2;
        x -= 7;
        if (shown)
            bluetoothGlyph(o, x, 1);
        x -= 5;
    }
    o.setFont(ArialMT_Plain_10);
    clipped(o, 2, 1, left, x - 2);
    o.setColor(WHITE);
}

static void lockGlyph(Panel& o, int16_t x, int16_t y, bool locked) {
    if (locked) {
        o.drawRect(x + 1, y, 5, 5);
    } else { // argola aberta: levantada do lado direito
        o.drawLine(x + 1, y + 4, x + 1, y + 1);
        o.drawLine(x + 1, y + 1, x + 4, y + 1);
        o.drawLine(x + 4, y + 1, x + 4, y + 2);
    }
    o.fillRect(x, y + 4, 7, 6);
}

void toast(Panel& o, const View& v, uint32_t now, const Locale& t) {
    if (v.toast == View::Toast::None || !active(now, v.toastAt, View::kToastHoldMs + View::kToastEdgeMs))
        return;
    const uint32_t quiet = now - v.toastAt;
    const float enter = 1 - eased(now, v.toastShownAt, View::kToastEdgeMs);
    const float leave = quiet > View::kToastHoldMs ? gfx::easeInOut(progressOf(quiet - View::kToastHoldMs, View::kToastEdgeMs)) : 0;
    constexpr int16_t kBoxH = 22;
    const int16_t y = 20 - int16_t(max(enter, leave) * (20 + kBoxH));

    const bool noDevice = v.toast == View::Toast::NoDevice;
    Txt key = Txt::UnlockHint;
    if (noDevice)
        key = Txt::NoDeviceTitle;
    else if (v.toast == View::Toast::Locked)
        key = Txt::LockedToast;
    else if (v.toast == View::Toast::Unlocked)
        key = Txt::UnlockedToast;
    o.setFont(ArialMT_Plain_10);
    const int16_t pad = noDevice ? 12 : 28; // sem cadeado no aviso de dispositivo
    const int16_t width = textWidth(o, t(key)) + pad;
    const int16_t x = (kW - width) / 2;
    clear(o, x, y, width, kBoxH);
    o.drawRect(x, y, width, kBoxH);
    if (!noDevice)
        lockGlyph(o, x + 7, y + 6, v.toast != View::Toast::Unlocked);
    o.setTextAlignment(TEXT_ALIGN_LEFT);
    o.drawText(x + pad / 2 + (noDevice ? 0 : 6), y + 4, t(key));
}

} // namespace ui
