#pragma once

#include <Arduino.h>
#include "core/AppTypes.h"

enum MenuOpenFlag : uint8_t {
    MENU_OPEN_NONE = 0,
    MENU_OPEN_STOP_RADIOS = 1 << 0,
    MENU_OPEN_RESET_PEAKS = 1 << 1,
    MENU_OPEN_RESET_INSPECTOR = 1 << 2
};

struct MenuFeature {
    const char* label;
    AppMode mode;
    uint8_t iconId;
    uint8_t openFlags;
};

namespace MenuCatalog {

constexpr int ITEMS_PER_PAGE = 6;
constexpr int PAGE_COUNT = 4;
constexpr int FEATURE_COUNT = 18;

const char* pageTitle(int page);
int pageItemCount(int page);
const MenuFeature& featureAt(int page, int slot);
int featureIndex(int page, int slot);

}  // namespace MenuCatalog
