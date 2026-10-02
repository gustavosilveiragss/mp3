#pragma once

#include <marmota.h>

#include "View.h"

class Ui {
public:
    static constexpr uint32_t kFrameMs = 50;

    Ui(mrm::Ssd1306Display& display, const mrm::Locale& locale)
        : display_(display)
        , locale_(locale) {}

    void draw(const View& view, uint32_t now);

private:
    mrm::Ssd1306Display& display_;
    const mrm::Locale& locale_;
};
