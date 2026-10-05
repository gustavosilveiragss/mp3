#include "UiDraw.h"

#include <PathRules.h>
#include "Text.h"
#include "UiArt.h"

using namespace mrm;
using namespace layout;

namespace ui {

int16_t progressBar(Panel& o, Rect bar, float percent) {
    o.drawRect(bar.x, bar.y, bar.w, bar.h);

    const int16_t fill = int16_t((bar.w - 2 * kBarPadding) * percent / 100);
    o.fillRect(bar.x + kBarPadding, bar.y + kBarPadding, fill, bar.h - 2 * kBarPadding);

    return fill;
}

bool active(uint32_t now, uint32_t at, uint32_t duration) {
    return at != 0 && now - at < duration;
}

constexpr uint8_t kBatteryLowPercent = 15;
constexpr uint32_t kBatteryBlinkMs = 400;
constexpr uint32_t kBluetoothConnectingBlinkMs = 250;
constexpr uint32_t kBluetoothBlinkMs = 700;

static void batteryGlyph(Panel& o, int16_t x, int16_t y, uint8_t level, uint32_t now) {
    gfx::drawIcon(o, x, y, mrm::ui::glyph::kBatteryFrame);

    if (level < kBatteryLowPercent && (now / kBatteryBlinkMs) % 2) // quase no fim: pisca vazia
        return;

    o.fillRect(x + kBatteryFill.x, y + kBatteryFill.y, (kBatteryFill.w * level + 50) / 100, kBatteryFill.h);
}

void topbar(Panel& o, const View& v, uint32_t now, const char* left) {
    o.setColor(WHITE);
    o.fillRect(0, 0, kW, kBar);
    o.setColor(BLACK);

    int16_t x = kW - 2;
    if (v.battery >= 0) {
        x -= mrm::ui::glyph::kBatteryFrame.w;
        batteryGlyph(o, x, kStatusY, v.battery, now);
        x -= kStatusGap;
    }

    if (v.wifiOn) {
        x -= kWifiSlotW;
        gfx::drawClip(o, x + kWifiBaseOffsetX, kWifiBaseY, mrm::ui::glyph::kWifiSmall, now);
        x -= kStatusGap;
    }

    if (v.btLink != Audio::Link::Off) {
        const uint32_t blinkMs = v.btLink == Audio::Link::Connecting ? kBluetoothConnectingBlinkMs : kBluetoothBlinkMs;
        const bool shown = v.btLink == Audio::Link::Connected || (now / blinkMs) % 2;

        x -= mrm::ui::glyph::kBluetooth.w;
        if (shown)
            gfx::drawIcon(o, x, kBluetoothY, mrm::ui::glyph::kBluetooth);

        x -= kStatusGap;
    }

    o.setFont(ArialMT_Plain_10);
    clipped(o, kTopbarTitle.x, kTopbarTitle.y, left, x - kTopbarTitle.x);

    o.setColor(WHITE);
}

static const gfx::Icon& lockIcon(View::Toast toast) {
    return toast == View::Toast::Unlocked ? art::kLockOpen : art::kLockClosed;
}

void toast(Panel& o, const View& v, uint32_t now, const Locale& t) {
    if (v.toast == View::Toast::None || !active(now, v.toastAt, View::kToastHoldMs + View::kToastEdgeMs))
        return;

    const uint32_t quiet = now - v.toastAt;
    const float enter = 1 - eased(now, v.toastShownAt, View::kToastEdgeMs);
    const float leave =
        quiet > View::kToastHoldMs ? gfx::easeInOut(progressOf(quiet - View::kToastHoldMs, View::kToastEdgeMs)) : 0;
    const int16_t y = kToastRestY - int16_t(max(enter, leave) * (kToastRestY + kToastBoxH));

    const bool noDevice = v.toast == View::Toast::NoDevice;
    const mrm::Text* key = &Txt::UnlockHint;
    if (noDevice)
        key = &Txt::NoDeviceTitle;
    else if (v.toast == View::Toast::Locked)
        key = &Txt::LockedToast;
    else if (v.toast == View::Toast::Unlocked)
        key = &Txt::UnlockedToast;

    o.setFont(ArialMT_Plain_10);

    const int16_t pad = noDevice ? kToastPadBare : kToastPadWithLock; // sem cadeado no aviso de dispositivo
    const int16_t width = textWidth(o, t(*key)) + pad;
    const int16_t x = (kW - width) / 2;

    clear(o, x, y, width, kToastBoxH);
    o.drawRect(x, y, width, kToastBoxH);

    if (!noDevice)
        gfx::drawIcon(o, x + kToastLock.x, y + kToastLock.y, lockIcon(v.toast));

    o.setTextAlignment(TEXT_ALIGN_LEFT);
    o.drawText(x + pad / 2 + (noDevice ? 0 : kToastLockShift), y + kToastTextDy, t(*key));
}

} // namespace ui
