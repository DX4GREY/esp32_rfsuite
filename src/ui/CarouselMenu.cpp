#include "ui/DisplayManager.h"
#include "ui/MenuCatalog.h"
#include "ui/DisplaySupport.h"

using namespace DisplayUi;

namespace {

struct CarouselItem {
    const char* label;
    uint8_t icon;
};

constexpr const char* MAIN_LABELS[] = {
    "2.4 GHz", "SUB-GHz", "SETTINGS", "SYS INFO", "LUA",
    "SD FILES", "DATA", "ANIMATION", "POWER"
};

constexpr uint8_t MAIN_ICONS[] = {6, 6, 9, 10, 9, 12, 5, 8, 11};

constexpr const char* SUB_LABELS[] = {
    "ANALYZER", "SUB READ", "LIBRARY", "PRESETS", "PACKETS", "RF TEST"
};

constexpr uint8_t SUB_ICONS[] = {0, 5, 12, 8, 7, 6};

int wrapCarouselIndex(int index, int count) {
    if (count <= 0) return 0;
    while (index < 0) index += count;
    while (index >= count) index -= count;
    return index;
}

int lerpCarouselInt(int from, int to, float progress) {
    return from + static_cast<int>((to - from) * progress);
}

float easeOutCubic(float value) {
    const float inv = 1.0f - value;
    return 1.0f - inv * inv * inv;
}

}  // namespace

bool DisplayManager::updateCarouselMenuUI() {
    const bool carouselMode = appState.appMode == APP_MODE_BAND_SELECT ||
                              appState.appMode == APP_MODE_MENU ||
                              appState.appMode == APP_MODE_SUBGHZ;
    const bool carouselEnabled = appState.animationsEnabled &&
                                 appState.menuAnimationEnabled &&
                                 appState.menuLayout != MENU_LAYOUT_LIST;
    if (!carouselMode || !carouselEnabled) return false;

    const int currentMode = static_cast<int>(appState.appMode);
    const bool modeChanged = renderedMode != currentMode;
    if (modeChanged) {
        tft.fillScreen(ST77XX_BLACK);
        renderedMode = currentMode;
        resetDynamicCaches();
        needRedraw = true;
        menuNeedsPartialRedraw = false;
        mainMenuNeedsPartialRedraw = false;
        subGhzMenuNeedsPartialRedraw = false;
    }

    auto scaledOffset = [](int value, int scalePercent) -> int {
        const int bias = value >= 0 ? 50 : -50;
        return (value * scalePercent + bias) / 100;
    };

    auto scaledLength = [](int value, int scalePercent) -> int {
        return max(1, (value * scalePercent + 50) / 100);
    };

    auto drawScaledIcon = [&](uint8_t index, int centerX, int centerY,
                              uint16_t color, uint16_t background,
                              int scalePercent) {
        auto o = [&](int value) { return scaledOffset(value, scalePercent); };
        auto n = [&](int value) { return scaledLength(value, scalePercent); };

        switch (index) {
            case 0: // Spectrum bars
                tft.drawFastVLine(centerX + o(-7), centerY + o(1), n(5), color);
                tft.drawFastVLine(centerX + o(-3), centerY + o(-3), n(9), color);
                tft.drawFastVLine(centerX + o(1), centerY + o(-6), n(12), color);
                tft.drawFastVLine(centerX + o(5), centerY + o(-1), n(7), color);
                tft.drawFastHLine(centerX + o(-9), centerY + o(6), n(18), color);
                break;
            case 1: // Waterfall/history
                for (int row = 0; row < 4; ++row) {
                    tft.drawFastHLine(centerX + o(-8 + row),
                                      centerY + o(-6 + row * 4),
                                      n(16 - row * 2), color);
                }
                break;
            case 2: // Magnifier
                tft.drawCircle(centerX + o(-2), centerY + o(-2), n(6), color);
                tft.drawLine(centerX + o(3), centerY + o(3),
                             centerX + o(8), centerY + o(8), color);
                tft.fillCircle(centerX + o(-2), centerY + o(-2), n(1), color);
                break;
            case 3: // Survey chart
                tft.drawFastHLine(centerX + o(-9), centerY + o(6), n(18), color);
                tft.fillRect(centerX + o(-7), centerY + o(0), n(3), n(6), color);
                tft.fillRect(centerX + o(-2), centerY + o(-4), n(3), n(10), color);
                tft.fillRect(centerX + o(3), centerY + o(-1), n(3), n(7), color);
                break;
            case 4: // Event marker
                tft.drawCircle(centerX, centerY, n(7), color);
                tft.drawLine(centerX, centerY + o(-5),
                             centerX + o(-2), centerY + o(1), color);
                tft.drawLine(centerX + o(-2), centerY + o(1),
                             centerX + o(3), centerY + o(1), color);
                tft.drawFastVLine(centerX + o(3), centerY + o(1), n(4), color);
                break;
            case 5: // Recording/logging
                tft.drawRoundRect(centerX + o(-9), centerY + o(-7),
                                  n(18), n(14), max(1, n(3)), color);
                tft.fillCircle(centerX, centerY, n(4), color);
                break;
            case 6: // RF antenna
                tft.drawFastVLine(centerX, centerY + o(-4), n(10), color);
                tft.fillCircle(centerX, centerY + o(-5), n(2), color);
                tft.drawLine(centerX + o(-3), centerY + o(5),
                             centerX + o(3), centerY + o(5), color);
                tft.drawLine(centerX + o(-5), centerY + o(-3),
                             centerX + o(-8), centerY, color);
                tft.drawLine(centerX + o(5), centerY + o(-3),
                             centerX + o(8), centerY, color);
                break;
            case 7: // Dual-radio diagnostics
                tft.drawRoundRect(centerX + o(-9), centerY + o(-6),
                                  n(7), n(12), max(1, n(2)), color);
                tft.drawRoundRect(centerX + o(2), centerY + o(-6),
                                  n(7), n(12), max(1, n(2)), color);
                tft.fillCircle(centerX + o(-6), centerY + o(3), n(1), color);
                tft.fillCircle(centerX + o(5), centerY + o(3), n(1), color);
                break;
            case 8: // Profiles
                tft.drawRoundRect(centerX + o(-9), centerY + o(-7),
                                  n(18), n(5), max(1, n(2)), color);
                tft.drawRoundRect(centerX + o(-7), centerY,
                                  n(14), n(5), max(1, n(2)), color);
                tft.drawRoundRect(centerX + o(-5), centerY + o(7),
                                  n(10), n(3), max(1, n(1)), color);
                break;
            case 9: // Settings sliders
                tft.drawFastHLine(centerX + o(-9), centerY + o(-5), n(18), color);
                tft.drawFastHLine(centerX + o(-9), centerY, n(18), color);
                tft.drawFastHLine(centerX + o(-9), centerY + o(5), n(18), color);
                tft.fillCircle(centerX + o(-3), centerY + o(-5), n(2), background);
                tft.drawCircle(centerX + o(-3), centerY + o(-5), n(2), color);
                tft.fillCircle(centerX + o(4), centerY, n(2), background);
                tft.drawCircle(centerX + o(4), centerY, n(2), color);
                tft.fillCircle(centerX, centerY + o(5), n(2), background);
                tft.drawCircle(centerX, centerY + o(5), n(2), color);
                break;
            case 10: // Device status
                tft.drawRoundRect(centerX + o(-8), centerY + o(-7),
                                  n(16), n(14), max(1, n(3)), color);
                tft.fillCircle(centerX, centerY + o(-3), n(1), color);
                tft.drawFastVLine(centerX, centerY, n(4), color);
                break;
            case 11: // Power/reboot
                tft.drawCircle(centerX, centerY, n(7), color);
                tft.fillRect(centerX + o(-2), centerY + o(-8),
                             n(5), n(7), background);
                tft.drawFastVLine(centerX, centerY + o(-8), n(9), color);
                break;
            case 12: // Folder / SD file explorer
                tft.drawRoundRect(centerX + o(-9), centerY + o(-5),
                                  n(18), n(12), max(1, n(2)), color);
                tft.fillRect(centerX + o(-7), centerY + o(-8), n(8), n(4), color);
                tft.drawFastHLine(centerX + o(-6), centerY, n(12), color);
                break;
        }
    };

    auto drawCard = [&](const CarouselItem& item, int centerX, int scalePercent) {
        const int boundedScale = constrain(scalePercent, 68, 136);
        const int width = 32 + (boundedScale - 68) * 34 / 68;
        const int height = 32 + (boundedScale - 68) * 24 / 68;
        const int centerY = 48;
        const int x = centerX - width / 2;
        const int y = centerY - height / 2;
        const bool selected = boundedScale >= 108;
        const uint16_t background = selected ? SPECTRUM_HEADER_BG : SPECTRUM_CARD_BG;
        const uint16_t border = selected ? SPECTRUM_ACCENT : SPECTRUM_BORDER;
        const uint16_t iconColor = selected ? SPECTRUM_ACCENT : ST77XX_GRAY;

        tft.fillRoundRect(x, y, width, height, selected ? 7 : 5, background);
        tft.drawRoundRect(x, y, width, height, selected ? 7 : 5, border);
        if (selected && width > 48 && height > 40) {
            tft.drawRoundRect(x + 2, y + 2, width - 4, height - 4, 5,
                              SPECTRUM_GRID);
            tft.fillCircle(centerX, y + height - 5, 1, SPECTRUM_ACCENT);
        }
        drawScaledIcon(item.icon, centerX, centerY - (selected ? 2 : 0),
                       iconColor, background, boundedScale);
    };

    auto drawLabel = [&](const char* label) {
        if (!label) return;
        const int width = static_cast<int>(strlen(label)) * 6;
        const int x = max(1, (160 - width) / 2);
        tft.setCursor(x, 80);
        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        tft.print(label);
    };

    auto drawPositionDots = [&](int count, int selected) {
        if (count <= 1) return;
        const int spacing = 7;
        const int firstX = 80 - ((count - 1) * spacing) / 2;
        for (int i = 0; i < count; ++i) {
            if (i == selected) tft.fillCircle(firstX + i * spacing, 96, 2, SPECTRUM_ACCENT);
            else tft.drawPixel(firstX + i * spacing, 96, SPECTRUM_GRID);
        }
    };

    auto drawStaticFrame = [&](CarouselItem* items, int count, int selected) {
        if (count <= 0) return;
        tft.fillRect(0, 15, 160, 89, ST77XX_BLACK);
        if (count > 1) {
            drawCard(items[wrapCarouselIndex(selected - 1, count)], 19, 70);
            drawCard(items[wrapCarouselIndex(selected + 1, count)], 141, 70);
        }
        drawCard(items[selected], 80, 136);
        drawLabel(items[selected].label);
        drawPositionDots(count, selected);
    };

    auto drawAnimatedFrame = [&](CarouselItem* items, int count,
                                 int oldSelection, int newSelection,
                                 int direction, float progress) {
        if (count <= 0) return;
        if (count == 1 || direction == 0) {
            drawStaticFrame(items, count, newSelection);
            return;
        }

        tft.fillRect(0, 15, 160, 89, ST77XX_BLACK);
        const float p = easeOutCubic(progress);
        const int sideScale = 70;
        const int centerScale = 136;
        const int leftX = 19;
        const int centerX = 80;
        const int rightX = 141;
        const int offLeftX = -24;
        const int offRightX = 184;

        if (direction > 0) {
            const int oldPrevious = wrapCarouselIndex(oldSelection - 1, count);
            const int newNext = wrapCarouselIndex(newSelection + 1, count);
            if (count > 2) {
                drawCard(items[oldPrevious], lerpCarouselInt(leftX, offLeftX, p), sideScale);
                drawCard(items[newNext], lerpCarouselInt(offRightX, rightX, p), sideScale);
            }
            drawCard(items[oldSelection], lerpCarouselInt(centerX, leftX, p),
                     lerpCarouselInt(centerScale, sideScale, p));
            drawCard(items[newSelection], lerpCarouselInt(rightX, centerX, p),
                     lerpCarouselInt(sideScale, centerScale, p));
        } else {
            const int oldNext = wrapCarouselIndex(oldSelection + 1, count);
            const int newPrevious = wrapCarouselIndex(newSelection - 1, count);
            if (count > 2) {
                drawCard(items[oldNext], lerpCarouselInt(rightX, offRightX, p), sideScale);
                drawCard(items[newPrevious], lerpCarouselInt(offLeftX, leftX, p), sideScale);
            }
            drawCard(items[oldSelection], lerpCarouselInt(centerX, rightX, p),
                     lerpCarouselInt(centerScale, sideScale, p));
            drawCard(items[newSelection], lerpCarouselInt(leftX, centerX, p),
                     lerpCarouselInt(sideScale, centerScale, p));
        }

        drawLabel((progress < 0.48f ? items[oldSelection] : items[newSelection]).label);
        drawPositionDots(count, progress < 0.48f ? oldSelection : newSelection);
    };

    auto transitionDirection = [&](int oldSelection, int newSelection, int count) {
        if (count <= 1 || oldSelection == newSelection) return 0;
        if (wrapCarouselIndex(oldSelection + 1, count) == newSelection) return 1;
        if (wrapCarouselIndex(oldSelection - 1, count) == newSelection) return -1;
        return newSelection > oldSelection ? 1 : -1;
    };

    auto animateSelection = [&](CarouselItem* items, int count,
                                int oldSelection, int newSelection) {
        const int direction = transitionDirection(oldSelection, newSelection, count);
        constexpr int FRAME_COUNT = 4;
        for (int frame = 0; frame <= FRAME_COUNT; ++frame) {
            const float progress = static_cast<float>(frame) / FRAME_COUNT;
            drawAnimatedFrame(items, count, oldSelection, newSelection,
                              direction, progress);
            if (frame != FRAME_COUNT) {
                delay(appState.scaledAnimationDelay(9));
                yield();
            }
        }
    };

    CarouselItem items[6] = {};
    int itemCount = 0;
    int selected = 0;
    int previous = 0;
    bool partialRedraw = false;

    if (appState.appMode == APP_MODE_BAND_SELECT) {
        const int page = bandSelection / 6;
        const int firstFeature = page * 6;
        itemCount = page == 0 ? 6 : 3;
        selected = bandSelection - firstFeature;
        previous = previousBandSelection - firstFeature;
        previous = constrain(previous, 0, itemCount - 1);
        for (int i = 0; i < itemCount; ++i) {
            const int feature = firstFeature + i;
            items[i] = {MAIN_LABELS[feature], MAIN_ICONS[feature]};
        }
        partialRedraw = mainMenuNeedsPartialRedraw;

        if (needRedraw || modeChanged) {
            drawModernHeader("MAIN MENU", SPECTRUM_ACCENT, page + 1, 2);
            drawModernFooter("U/D MOVE", "A OPEN", "B PAGE");
            drawStaticFrame(items, itemCount, selected);
            needRedraw = false;
            mainMenuNeedsPartialRedraw = false;
        } else if (partialRedraw) {
            animateSelection(items, itemCount, previous, selected);
            mainMenuNeedsPartialRedraw = false;
        }
    } else if (appState.appMode == APP_MODE_MENU) {
        itemCount = min(6, MenuCatalog::pageItemCount(menuPage));
        selected = constrain(menuSelection, 0, itemCount - 1);
        previous = constrain(prevMenuSelection, 0, itemCount - 1);
        for (int i = 0; i < itemCount; ++i) {
            const MenuFeature& feature = MenuCatalog::featureAt(menuPage, i);
            items[i] = {feature.label, feature.iconId};
        }
        partialRedraw = menuNeedsPartialRedraw;

        if (needRedraw || modeChanged) {
            drawModernHeader(appState.simulationMode ? "2.4 GHz [SIM]" :
                             MenuCatalog::pageTitle(menuPage),
                             SPECTRUM_ACCENT, menuPage + 1, MenuCatalog::PAGE_COUNT);
            drawModernFooter("U/D MOVE", "A OPEN", "B PAGE");
            drawStaticFrame(items, itemCount, selected);
            needRedraw = false;
            menuNeedsPartialRedraw = false;
        } else if (partialRedraw) {
            animateSelection(items, itemCount, previous, selected);
            menuNeedsPartialRedraw = false;
        }

        const int featureIndex = MenuCatalog::featureIndex(menuPage, selected);
        if (featureIndex == 4 && appState.eventCount > 0) {
            tft.fillCircle(108, 28, 6, SPECTRUM_HIGH);
            tft.setCursor(appState.eventCount < 10 ? 106 : 103, 25);
            tft.setTextColor(ST77XX_BLACK, SPECTRUM_HIGH);
            tft.print(appState.eventCount);
        } else if (featureIndex == 5 && appState.loggingEnabled) {
            tft.fillCircle(108, 28, 4, SPECTRUM_CRITICAL);
        }
    } else {
        itemCount = 6;
        selected = constrain(subGhzMenuSelection, 0, itemCount - 1);
        previous = constrain(previousSubGhzMenuSelection, 0, itemCount - 1);
        for (int i = 0; i < itemCount; ++i) items[i] = {SUB_LABELS[i], SUB_ICONS[i]};
        partialRedraw = subGhzMenuNeedsPartialRedraw;

        if (needRedraw || modeChanged) {
            drawModernHeader(subGhzRawService.simulationMode() ? "SUB-GHz [SIM]" : "SUB-GHz",
                             SPECTRUM_HIGH, 1, 1);
            drawModernFooter("U/D MOVE", "A OPEN", "B MAIN");
            drawStaticFrame(items, itemCount, selected);
            needRedraw = false;
            subGhzMenuNeedsPartialRedraw = false;
        } else if (partialRedraw) {
            animateSelection(items, itemCount, previous, selected);
            subGhzMenuNeedsPartialRedraw = false;
        }
    }

    drawThemeAnimation();
    if (errorModalActive) drawActionableErrorModal();
    if (toastActive) drawToastOverlay();
    return true;
}
