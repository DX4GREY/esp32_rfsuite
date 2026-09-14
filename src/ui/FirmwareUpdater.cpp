// SD-card firmware updater for ESP32-S3.
//
// The updater is deliberately kept outside the normal DisplayController state
// machine so it can extend the global menu without disturbing the existing
// analyzer/sub-GHz navigation. It writes only through Arduino-ESP32 Update,
// which targets the inactive OTA application partition rather than overwriting
// the running image in place.
#define private public
#include "ui/DisplayManager.h"
#undef private

#include <Update.h>
#include <dirent.h>
#include <sys/stat.h>

#include "drivers/ButtonManager.h"
#include "drivers/RadioManager.h"
#include "services/SessionRecorder.h"
#include "services/StorageManager.h"
#include "services/Watchdog.h"

namespace {
constexpr const char* BINARY_DIR = "/RFSuite/binary";
constexpr const char* BINARY_VFS_DIR = "/sd/RFSuite/binary";
constexpr size_t MAX_BINARIES = 20;
constexpr uint8_t ESP_IMAGE_MAGIC = 0xE9;

String binaryNames[MAX_BINARIES];
uint32_t binarySizes[MAX_BINARIES] = {};
size_t binaryCount = 0;
size_t binarySelection = 0;
size_t binaryScroll = 0;
bool updaterScanned = false;
bool updaterConfirm = false;
String updaterStatus;

constexpr const char* MAIN_LABELS[] = {
    "2.4 GHz", "SUB-GHz", "SETTINGS", "SYS INFO", "LUA",
    "SD FILES", "DATA", "ANIMATION", "POWER", "UPDATER"
};
constexpr uint8_t MAIN_ICONS[] = {6, 6, 9, 10, 9, 12, 5, 8, 11, 12};

int mainFeatureIndex(int displayIndex) {
    return (!appState.animationsEnabled && displayIndex >= 7)
        ? displayIndex + 1 : displayIndex;
}

int updaterDisplayIndex() {
    return appState.animationsEnabled ? 9 : 8;
}

int extendedMainMenuCount() {
    return (appState.animationsEnabled ? 9 : 8) +
           (storageManager.sdUsable() ? 1 : 0);
}

void sortBinaries() {
    for (size_t i = 0; i < binaryCount; ++i) {
        for (size_t j = i + 1; j < binaryCount; ++j) {
            if (binaryNames[j].compareTo(binaryNames[i]) >= 0) continue;
            String name = binaryNames[i];
            binaryNames[i] = binaryNames[j];
            binaryNames[j] = name;
            const uint32_t size = binarySizes[i];
            binarySizes[i] = binarySizes[j];
            binarySizes[j] = size;
        }
    }
}

void scanBinaries() {
    binaryCount = 0;
    binarySelection = 0;
    binaryScroll = 0;
    updaterConfirm = false;
    updaterStatus = "";
    updaterScanned = true;

    if (!storageManager.sdUsable()) {
        updaterStatus = "SD NOT READY";
        return;
    }

    fs::FS& fs = storageManager.filesystem();
    if (!fs.exists(BINARY_DIR) && !fs.mkdir(BINARY_DIR)) {
        updaterStatus = "CANNOT CREATE /binary";
        return;
    }

    DIR* dir = opendir(BINARY_VFS_DIR);
    if (!dir) {
        updaterStatus = "CANNOT OPEN /binary";
        return;
    }

    for (dirent* entry = readdir(dir);
         entry && binaryCount < MAX_BINARIES;
         entry = readdir(dir)) {
        String name(entry->d_name);
        if (!name.length() || name == "." || name == "..") continue;
        String lower = name;
        lower.toLowerCase();
        if (!lower.endsWith(".bin")) continue;

        String fullPath = String(BINARY_VFS_DIR) + "/" + name;
        struct stat info {};
        if (stat(fullPath.c_str(), &info) != 0 || !S_ISREG(info.st_mode) || info.st_size <= 0)
            continue;

        binaryNames[binaryCount] = name;
        binarySizes[binaryCount] = static_cast<uint32_t>(info.st_size);
        ++binaryCount;
    }
    closedir(dir);
    sortBinaries();

    if (!binaryCount) updaterStatus = "NO .BIN FILES";
    else if (binaryCount == MAX_BINARIES) updaterStatus = "SHOWING FIRST 20";
}

void drawProgress(DisplayManager& dm, uint8_t percent, const String& name) {
    dm.tft.fillScreen(ST77XX_BLACK);
    dm.drawModernHeader("FLASH UPDATE", SPECTRUM_CRITICAL);
    dm.tft.setTextSize(1);
    dm.tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    String shown = name;
    if (shown.length() > 22) shown = shown.substring(0, 21) + "~";
    dm.tft.setCursor(max(4, (160 - static_cast<int>(shown.length()) * 6) / 2), 31);
    dm.tft.print(shown);

    dm.tft.setCursor(49, 51);
    dm.tft.setTextColor(SPECTRUM_ACCENT, ST77XX_BLACK);
    dm.tft.print("FLASHING ");
    dm.tft.print(percent);
    dm.tft.print("%");

    dm.tft.drawRoundRect(12, 67, 136, 11, 4, SPECTRUM_BORDER);
    if (percent) {
        const int width = max(2, static_cast<int>(percent) * 132 / 100);
        dm.tft.fillRoundRect(14, 69, width, 7, 3,
                             percent == 100 ? SPECTRUM_LOW : SPECTRUM_ACCENT);
    }
    dm.tft.setCursor(35, 87);
    dm.tft.setTextColor(ST77XX_GRAY, ST77XX_BLACK);
    dm.tft.print("DO NOT POWER OFF");
    dm.drawModernFooter("", "WRITING", "");
}

void failUpdate(DisplayManager& dm, const String& message) {
    Update.abort();
    watchdog.init(WATCHDOG_TIMEOUT_US);
    updaterConfirm = false;
    updaterStatus = message;
    dm.needRedraw = true;
    Serial.println("Updater: " + message);
}

void flashSelected(DisplayManager& dm) {
    if (!storageManager.sdUsable() || binarySelection >= binaryCount) {
        updaterStatus = "SD / FILE LOST";
        updaterConfirm = false;
        dm.needRedraw = true;
        return;
    }

    const String name = binaryNames[binarySelection];
    const String path = String(BINARY_DIR) + "/" + name;
    fs::FS& fs = storageManager.filesystem();
    File firmware = fs.open(path, FILE_READ);
    if (!firmware) {
        updaterStatus = "OPEN FAILED";
        updaterConfirm = false;
        dm.needRedraw = true;
        return;
    }

    const size_t imageSize = firmware.size();
    uint8_t magic = 0;
    if (imageSize < 1024 || firmware.read(&magic, 1) != 1 ||
        magic != ESP_IMAGE_MAGIC || !firmware.seek(0)) {
        firmware.close();
        updaterStatus = "INVALID ESP .BIN";
        updaterConfirm = false;
        dm.needRedraw = true;
        return;
    }

    // No RF or recorder task should touch SPI/storage while a new application
    // image is being committed. The application watchdog is intentionally
    // paused because flash writes can exceed its normal UI deadline.
    radioManager.stopAllAndWait();
    sessionRecorder.stop();
    watchdog.stop();
    drawProgress(dm, 0, name);

    if (!Update.begin(imageSize, U_FLASH)) {
        firmware.close();
        Update.printError(Serial);
        failUpdate(dm, "IMAGE TOO LARGE/INVALID");
        return;
    }

    static uint8_t buffer[1024];
    size_t written = 0;
    uint8_t lastPercent = 0;
    while (written < imageSize) {
        const size_t wanted = min(sizeof(buffer), imageSize - written);
        const int got = firmware.read(buffer, wanted);
        if (got <= 0) {
            firmware.close();
            failUpdate(dm, "SD READ FAILED");
            return;
        }
        const size_t committed = Update.write(buffer, static_cast<size_t>(got));
        if (committed != static_cast<size_t>(got)) {
            firmware.close();
            Update.printError(Serial);
            failUpdate(dm, "FLASH WRITE FAILED");
            return;
        }
        written += committed;
        const uint8_t percent = static_cast<uint8_t>((written * 100ULL) / imageSize);
        if (percent >= lastPercent + 2 || percent == 100) {
            lastPercent = percent;
            drawProgress(dm, percent, name);
        }
        yield();
    }
    firmware.close();

    if (!Update.end() || !Update.isFinished()) {
        Update.printError(Serial);
        failUpdate(dm, "UPDATE VERIFY FAILED");
        return;
    }

    drawProgress(dm, 100, name);
    dm.tft.fillRect(0, 84, 160, 18, ST77XX_BLACK);
    dm.tft.setCursor(37, 89);
    dm.tft.setTextColor(SPECTRUM_LOW, ST77XX_BLACK);
    dm.tft.print("UPDATE SUCCESS");
    Serial.println("Updater: firmware committed successfully: " + name);
    delay(700);

    storageManager.prepareForRestart();
    Serial.flush();
    ESP.restart();
}

void drawFallbackExtendedMainMenu(DisplayManager& dm) {
    const int page = dm.bandSelection / 6;
    if (page != 1) return;

    dm.drawModernHeader("MAIN MENU", SPECTRUM_ACCENT, 2, 2);
    dm.tft.fillRect(0, 15, 160, 89, ST77XX_BLACK);

    const bool list = appState.menuLayout == MENU_LAYOUT_LIST;
    const int count = extendedMainMenuCount() - 6;
    for (int slot = 0; slot < count; ++slot) {
        const int displayIndex = 6 + slot;
        const int featureIndex = mainFeatureIndex(displayIndex);
        const bool selected = displayIndex == dm.bandSelection;
        const int x = list ? 3 : 4 + (slot % 2) * 78;
        const int y = list ? 16 + slot * 22 : 16 + (slot / 2) * 29;
        const int width = list ? 154 : 74;
        const int height = list ? 20 : 26;
        const uint16_t bg = selected ? SPECTRUM_HEADER_BG : SPECTRUM_CARD_BG;
        const uint16_t edge = selected ? SPECTRUM_ACCENT : SPECTRUM_BORDER;

        dm.drawThemedMenuCard(x, y, width, height, selected, list, bg, edge);
        dm.drawMenuIcon(MAIN_ICONS[featureIndex], list ? x + 20 : x + width / 2,
                        list ? y + 10 : y + 8,
                        selected ? SPECTRUM_ACCENT : ST77XX_GRAY, bg);
        const int labelX = list ? x + 39 : x +
            (width - static_cast<int>(strlen(MAIN_LABELS[featureIndex])) * 6) / 2;
        dm.tft.setCursor(labelX, list ? y + 7 : y + 17);
        dm.tft.setTextColor(selected ? ST77XX_WHITE : ST77XX_GRAY, bg);
        dm.tft.print(MAIN_LABELS[featureIndex]);
    }
    dm.drawModernFooter("U/D MOVE", "A OPEN", "B PAGE");
    dm.needRedraw = false;
    dm.mainMenuNeedsPartialRedraw = false;
}

void drawUpdater(DisplayManager& dm) {
    if (!updaterScanned) scanBinaries();
    dm.drawModernHeader(updaterConfirm ? "CONFIRM UPDATE" : "UPDATER",
                        updaterConfirm ? SPECTRUM_HIGH : SPECTRUM_ACCENT);
    dm.tft.fillRect(0, 15, 160, 89, ST77XX_BLACK);
    dm.tft.setTextSize(1);

    if (updaterConfirm && binarySelection < binaryCount) {
        dm.tft.fillRoundRect(5, 22, 150, 72, 5, SPECTRUM_CARD_BG);
        dm.tft.drawRoundRect(5, 22, 150, 72, 5, SPECTRUM_HIGH);
        dm.tft.setCursor(37, 30);
        dm.tft.setTextColor(SPECTRUM_HIGH, SPECTRUM_CARD_BG);
        dm.tft.print("ARE U SURE TO");
        dm.tft.setCursor(58, 43);
        dm.tft.setTextColor(ST77XX_WHITE, SPECTRUM_CARD_BG);
        dm.tft.print("UPDATE");
        String shown = binaryNames[binarySelection];
        if (shown.length() > 22) shown = shown.substring(0, 21) + "~";
        dm.tft.setCursor(max(8, (160 - static_cast<int>(shown.length()) * 6) / 2), 58);
        dm.tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_CARD_BG);
        dm.tft.print(shown);
        dm.tft.setCursor(50, 75);
        dm.tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
        dm.tft.print(binarySizes[binarySelection] / 1024);
        dm.tft.print(" KB IMAGE");
        dm.drawModernFooter("", "A OK", "B CANCEL");
        return;
    }

    dm.tft.setCursor(4, 18);
    dm.tft.setTextColor(ST77XX_GRAY, ST77XX_BLACK);
    dm.tft.print("SD:/RFSuite/binary/");

    if (!storageManager.sdUsable()) {
        dm.drawEmptyState("SD NOT READY", "Insert/mount SD first.", "", "B BACK");
    } else if (!binaryCount) {
        dm.drawEmptyState(updaterStatus.length() ? updaterStatus.c_str() : "NO .BIN FILES",
                          "Copy firmware to /binary/.", "A REFRESH", "B BACK");
    } else {
        const size_t first = binaryScroll;
        for (size_t row = 0; row < 5 && first + row < binaryCount; ++row) {
            const size_t index = first + row;
            const int y = 32 + static_cast<int>(row) * 13;
            const bool selected = index == binarySelection;
            if (selected) dm.tft.fillRoundRect(3, y - 2, 154, 12, 2, SPECTRUM_CARD_BG);
            dm.tft.setCursor(6, y);
            dm.tft.setTextColor(selected ? SPECTRUM_ACCENT : ST77XX_WHITE,
                                selected ? SPECTRUM_CARD_BG : ST77XX_BLACK);
            dm.tft.print(selected ? "> " : "  ");
            String shown = binaryNames[index];
            if (shown.length() > 18) shown = shown.substring(0, 17) + "~";
            dm.tft.print(shown);
        }
        if (updaterStatus.length()) {
            dm.tft.setCursor(5, 96);
            dm.tft.setTextColor(SPECTRUM_LOW, ST77XX_BLACK);
            dm.tft.print(updaterStatus);
        }
        dm.drawModernFooter("U/D SEL", "A OPEN", "B BACK");
    }
}
}  // namespace

bool processFirmwareUpdaterInput(DisplayManager& dm) {
    if (appState.appMode == APP_MODE_BAND_SELECT) {
        const int baseCount = appState.animationsEnabled ? 9 : 8;
        if (!storageManager.sdUsable()) {
            // If the card became unavailable after UPDATER was selected, never
            // leave a hidden out-of-range menu cursor behind.
            if (dm.bandSelection >= baseCount) {
                dm.previousBandSelection = dm.bandSelection;
                dm.bandSelection = baseCount - 1;
                dm.mainMenuScrollOffset = 0;
                dm.mainMenuNeedsPartialRedraw = false;
                dm.needRedraw = true;
                return true;
            }
            return false;
        }

        const int itemCount = extendedMainMenuCount();
        if (dm.bandSelection >= itemCount) {
            dm.bandSelection = itemCount - 1;
            dm.needRedraw = true;
        }

        if (buttonManager.isPressed(BTN_UP)) {
            dm.previousBandSelection = dm.bandSelection;
            dm.previousMainMenuScrollOffset = dm.mainMenuScrollOffset;
            dm.bandSelection = (dm.bandSelection + itemCount - 1) % itemCount;
            const int slot = dm.bandSelection % 6;
            if (dm.bandSelection >= 6) dm.mainMenuScrollOffset = 0;
            else if (slot < dm.mainMenuScrollOffset) dm.mainMenuScrollOffset = slot;
            else if (slot >= dm.mainMenuScrollOffset + 4) dm.mainMenuScrollOffset = slot - 3;
            if (dm.previousBandSelection / 6 != dm.bandSelection / 6) dm.needRedraw = true;
            else dm.mainMenuNeedsPartialRedraw = true;
            return true;
        }
        if (buttonManager.isPressed(BTN_DOWN)) {
            dm.previousBandSelection = dm.bandSelection;
            dm.previousMainMenuScrollOffset = dm.mainMenuScrollOffset;
            dm.bandSelection = (dm.bandSelection + 1) % itemCount;
            const int slot = dm.bandSelection % 6;
            if (dm.bandSelection >= 6 || dm.bandSelection == 0) dm.mainMenuScrollOffset = 0;
            else if (slot >= dm.mainMenuScrollOffset + 4) dm.mainMenuScrollOffset = slot - 3;
            if (dm.previousBandSelection / 6 != dm.bandSelection / 6) dm.needRedraw = true;
            else dm.mainMenuNeedsPartialRedraw = true;
            return true;
        }
        if (buttonManager.isPressed(BTN_B)) {
            dm.previousBandSelection = dm.bandSelection;
            dm.previousMainMenuScrollOffset = dm.mainMenuScrollOffset;
            dm.bandSelection = dm.bandSelection < 6 ? 6 : 0;
            dm.mainMenuScrollOffset = 0;
            dm.mainMenuNeedsPartialRedraw = false;
            dm.needRedraw = true;
            return true;
        }
        if (dm.bandSelection == updaterDisplayIndex() && buttonManager.isPressed(BTN_A)) {
            radioManager.stopAll();
            updaterScanned = false;
            updaterConfirm = false;
            updaterStatus = "";
            appState.appMode = APP_MODE_UPDATER;
            dm.needRedraw = true;
            return true;
        }
        return false;
    }

    if (appState.appMode != APP_MODE_UPDATER) return false;

    if (updaterConfirm) {
        if (buttonManager.isPressed(BTN_A)) {
            flashSelected(dm);
        } else if (buttonManager.isPressed(BTN_B)) {
            updaterConfirm = false;
            updaterStatus = "UPDATE CANCELLED";
            dm.needRedraw = true;
        }
        return true;
    }

    if (buttonManager.isPressed(BTN_UP) && binaryCount) {
        binarySelection = (binarySelection + binaryCount - 1) % binaryCount;
        if (binarySelection < binaryScroll) binaryScroll = binarySelection;
        if (binarySelection + 1 == binaryCount && binaryCount > 5) binaryScroll = binaryCount - 5;
        updaterStatus = "";
        dm.needRedraw = true;
    } else if (buttonManager.isPressed(BTN_DOWN) && binaryCount) {
        binarySelection = (binarySelection + 1) % binaryCount;
        if (binarySelection >= binaryScroll + 5) binaryScroll = binarySelection - 4;
        if (binarySelection == 0) binaryScroll = 0;
        updaterStatus = "";
        dm.needRedraw = true;
    } else if (buttonManager.isPressed(BTN_A)) {
        if (binaryCount) {
            updaterConfirm = true;
            updaterStatus = "";
        } else {
            scanBinaries();
        }
        dm.needRedraw = true;
    } else if (buttonManager.isPressed(BTN_B)) {
        updaterConfirm = false;
        if (!storageManager.sdUsable()) {
            const int baseCount = appState.animationsEnabled ? 9 : 8;
            dm.bandSelection = min(dm.bandSelection, baseCount - 1);
        }
        appState.appMode = APP_MODE_BAND_SELECT;
        dm.needRedraw = true;
    }
    return true;
}

bool updateFirmwareUpdaterUI(DisplayManager& dm) {
    if (appState.appMode == APP_MODE_UPDATER) {
        const int mode = static_cast<int>(APP_MODE_UPDATER);
        if (dm.renderedMode != mode) {
            dm.tft.fillScreen(ST77XX_BLACK);
            dm.renderedMode = mode;
            dm.resetDynamicCaches();
            dm.needRedraw = true;
        }
        if (dm.needRedraw) {
            drawUpdater(dm);
            dm.needRedraw = false;
        }
        return true;
    }

    const bool carouselActive = appState.animationsEnabled &&
                                appState.menuAnimationEnabled &&
                                appState.menuLayout != MENU_LAYOUT_LIST;
    if (appState.appMode == APP_MODE_BAND_SELECT &&
        storageManager.sdUsable() && !carouselActive && dm.bandSelection >= 6) {
        const int mode = static_cast<int>(APP_MODE_BAND_SELECT);
        if (dm.renderedMode != mode) {
            dm.tft.fillScreen(ST77XX_BLACK);
            dm.renderedMode = mode;
            dm.resetDynamicCaches();
            dm.needRedraw = true;
        }
        if (dm.needRedraw || dm.mainMenuNeedsPartialRedraw)
            drawFallbackExtendedMainMenu(dm);
        return true;
    }
    return false;
}
