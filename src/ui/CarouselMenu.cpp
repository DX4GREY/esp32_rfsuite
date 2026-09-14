// Carousel menu renderer inspired by compact handheld selector UIs.
// It is intentionally isolated from the legacy card renderer: when menu
// animations or GRID layout are disabled, this hook returns false and the
// existing DisplayManager::updateUI() path renders the original menu.
#define private public
#include "ui/DisplayManager.h"
#undef private

#include "ui/MenuCatalog.h"
#include "services/SubGhzRawService.h"
#include "services/StorageManager.h"

namespace {
struct CarouselItem {
    const char* label;
    uint8_t icon;
};

constexpr const char* MAIN_LABELS[] = {
    "2.4 GHz", "SUB-GHz", "SETTINGS", "SYS INFO", "LUA",
    "SD FILES", "DATA", "ANIMATION", "POWER", "UPDATER"
};
constexpr uint8_t MAIN_ICONS[] = {6, 6, 9, 10, 9, 12, 5, 8, 11, 12};
constexpr const char* SUB_LABELS[] = {
    "ANALYZER", "SUB READ", "LIBRARY", "PRESETS", "PACKETS", "RF TEST"
};
constexpr uint8_t SUB_ICONS[] = {0, 5, 12, 8, 7, 6};

int wrapIndex(int value, int count) {
    if (count <= 0) return 0;
    while (value < 0) value += count;
    while (value >= count) value -= count;
    return value;
}

float easeOutCubic(float t) {
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

int mixInt(int from, int to, float t) {
    return from + static_cast<int>((to - from) * t);
}
}

bool updateCarouselMenuUI(DisplayManager& dm) {
    const bool supportedMode = appState.appMode == APP_MODE_BAND_SELECT ||
                               appState.appMode == APP_MODE_MENU ||
                               appState.appMode == APP_MODE_SUBGHZ;
    const bool enabled = appState.animationsEnabled &&
                         appState.menuAnimationEnabled &&
                         appState.menuLayout != MENU_LAYOUT_LIST;
    if (!supportedMode || !enabled) return false;

    const int mode = static_cast<int>(appState.appMode);
    if (dm.renderedMode != mode) {
        dm.tft.fillScreen(ST77XX_BLACK);
        dm.renderedMode = mode;
        dm.resetDynamicCaches();
        dm.needRedraw = true;
        dm.menuNeedsPartialRedraw = false;
        dm.mainMenuNeedsPartialRedraw = false;
        dm.subGhzMenuNeedsPartialRedraw = false;
    }

    auto drawCard = [&](const CarouselItem& item, int centerX, int scale) {
        scale = constrain(scale, 58, 100);
        const int width = 29 + (scale - 58) * 31 / 42;
        const int height = 29 + (scale - 58) * 23 / 42;
        const int centerY = 49;
        const int x = centerX - width / 2;
        const int y = centerY - height / 2;
        const bool selected = scale >= 85;
        const uint16_t background = selected ? SPECTRUM_HEADER_BG : SPECTRUM_CARD_BG;
        const uint16_t edge = selected ? SPECTRUM_ACCENT : SPECTRUM_BORDER;
        dm.tft.fillRoundRect(x, y, width, height, selected ? 7 : 4, background);
        dm.tft.drawRoundRect(x, y, width, height, selected ? 7 : 4, edge);
        if (selected) {
            dm.tft.drawRoundRect(x + 2, y + 2, width - 4, height - 4, 5, SPECTRUM_GRID);
            dm.tft.drawCircle(centerX, centerY - 2, 13, SPECTRUM_GRID);
        }
        dm.drawMenuIcon(item.icon, centerX, centerY - (selected ? 2 : 0),
                        selected ? SPECTRUM_ACCENT : ST77XX_GRAY, background);
    };

    auto drawLabel = [&](const char* label) {
        dm.tft.fillRect(0, 76, 160, 16, ST77XX_BLACK);
        const int width = static_cast<int>(strlen(label)) * 6;
        dm.tft.setCursor(max(1, (160 - width) / 2), 80);
        dm.tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        dm.tft.print(label);
    };

    auto drawDots = [&](int count, int selected) {
        dm.tft.fillRect(0, 92, 160, 10, ST77XX_BLACK);
        if (count <= 1) return;
        const int spacing = 7;
        const int firstX = 80 - ((count - 1) * spacing) / 2;
        for (int i = 0; i < count; ++i) {
            if (i == selected) dm.tft.fillCircle(firstX + i * spacing, 96, 2, SPECTRUM_ACCENT);
            else dm.tft.drawPixel(firstX + i * spacing, 96, SPECTRUM_GRID);
        }
    };

    auto drawStatic = [&](CarouselItem* items, int count, int selected) {
        dm.tft.fillRect(0, 15, 160, 89, ST77XX_BLACK);
        if (count > 1) {
            drawCard(items[wrapIndex(selected - 1, count)], 18, 58);
            drawCard(items[wrapIndex(selected + 1, count)], 142, 58);
        }
        drawCard(items[selected], 80, 100);
        drawLabel(items[selected].label);
        drawDots(count, selected);
    };

    auto directionFor = [&](int oldSel, int newSel, int count) {
        if (oldSel == newSel || count <= 1) return 0;
        if (wrapIndex(oldSel + 1, count) == newSel) return 1;
        if (wrapIndex(oldSel - 1, count) == newSel) return -1;
        return newSel > oldSel ? 1 : -1;
    };

    auto animate = [&](CarouselItem* items, int count, int oldSel, int newSel) {
        const int direction = directionFor(oldSel, newSel, count);
        if (!direction) { drawStatic(items, count, newSel); return; }
        constexpr int FRAMES = 5;
        for (int frame = 0; frame <= FRAMES; ++frame) {
            const float raw = static_cast<float>(frame) / FRAMES;
            const float p = easeOutCubic(raw);
            dm.tft.fillRect(0, 15, 160, 77, ST77XX_BLACK);

            const int oldX = mixInt(80, direction > 0 ? 18 : 142, p);
            const int newX = mixInt(direction > 0 ? 142 : 18, 80, p);
            const int oldScale = mixInt(100, 58, p);
            const int newScale = mixInt(58, 100, p);

            if (count > 2) {
                const int outgoing = wrapIndex(oldSel - direction, count);
                const int incoming = wrapIndex(newSel + direction, count);
                drawCard(items[outgoing],
                         mixInt(direction > 0 ? 18 : 142,
                                direction > 0 ? -20 : 180, p), 58);
                drawCard(items[incoming],
                         mixInt(direction > 0 ? 180 : -20,
                                direction > 0 ? 142 : 18, p), 58);
            }

            drawCard(items[oldSel], oldX, oldScale);
            drawCard(items[newSel], newX, newScale);
            drawLabel(raw < 0.45f ? items[oldSel].label : items[newSel].label);
            if (frame != FRAMES) {
                delay(appState.scaledAnimationDelay(10));
                yield();
            }
        }
        drawDots(count, newSel);
    };

    CarouselItem items[6] = {};
    int count = 0;
    int selected = 0;
    int previous = 0;
    bool partial = false;

    if (appState.appMode == APP_MODE_BAND_SELECT) {
        const int page = dm.bandSelection / 6;
        const int first = page * 6;
        count = page == 0 ? 6 : 3 + (storageManager.sdUsable() ? 1 : 0);
        selected = constrain(dm.bandSelection - first, 0, count - 1);
        previous = constrain(dm.previousBandSelection - first, 0, count - 1);
        for (int i = 0; i < count; ++i) {
            items[i] = {MAIN_LABELS[first + i], MAIN_ICONS[first + i]};
        }
        partial = dm.mainMenuNeedsPartialRedraw;
        if (dm.needRedraw) {
            dm.drawModernHeader("MAIN MENU", SPECTRUM_ACCENT, page + 1, 2);
            dm.drawModernFooter("U/D MOVE", "A OPEN", "B PAGE");
            drawStatic(items, count, selected);
            dm.needRedraw = false;
        } else if (partial) {
            animate(items, count, previous, selected);
        }
        dm.mainMenuNeedsPartialRedraw = false;
    } else if (appState.appMode == APP_MODE_MENU) {
        count = min(6, MenuCatalog::pageItemCount(dm.menuPage));
        selected = constrain(dm.menuSelection, 0, count - 1);
        previous = constrain(dm.prevMenuSelection, 0, count - 1);
        for (int i = 0; i < count; ++i) {
            const MenuFeature& feature = MenuCatalog::featureAt(dm.menuPage, i);
            items[i] = {feature.label, feature.iconId};
        }
        partial = dm.menuNeedsPartialRedraw;
        if (dm.needRedraw) {
            dm.drawModernHeader(appState.simulationMode ? "2.4 GHz [SIM]" :
                                MenuCatalog::pageTitle(dm.menuPage),
                                SPECTRUM_ACCENT, dm.menuPage + 1, MenuCatalog::PAGE_COUNT);
            dm.drawModernFooter("U/D MOVE", "A OPEN", "B PAGE");
            drawStatic(items, count, selected);
            dm.needRedraw = false;
        } else if (partial) {
            animate(items, count, previous, selected);
        }
        dm.menuNeedsPartialRedraw = false;
    } else {
        count = 6;
        selected = constrain(dm.subGhzMenuSelection, 0, 5);
        previous = constrain(dm.previousSubGhzMenuSelection, 0, 5);
        for (int i = 0; i < count; ++i) items[i] = {SUB_LABELS[i], SUB_ICONS[i]};
        partial = dm.subGhzMenuNeedsPartialRedraw;
        if (dm.needRedraw) {
            dm.drawModernHeader(subGhzRawService.simulationMode() ? "SUB-GHz [SIM]" : "SUB-GHz",
                                SPECTRUM_HIGH, 1, 1);
            dm.drawModernFooter("U/D MOVE", "A OPEN", "B MAIN");
            drawStatic(items, count, selected);
            dm.needRedraw = false;
        } else if (partial) {
            animate(items, count, previous, selected);
        }
        dm.subGhzMenuNeedsPartialRedraw = false;
    }

    return true;
}
