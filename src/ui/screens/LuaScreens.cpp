#include "ui/DisplayManager.h"
#include "ui/DisplaySupport.h"
#include "services/LuaEngine.h"
#include "services/StorageManager.h"
#include <dirent.h>
#include <sys/stat.h>

using namespace DisplayUi;

void DisplayManager::renderLuaScriptsScreen() {
    if (luaShowingGui) return;
    if (luaShowingOutput) {
        drawModernHeader("LUA OUTPUT", SPECTRUM_ACCENT);
        tft.fillRect(0, 15, 160, 90, ST77XX_BLACK);
        String script = luaScripts[luaScriptSelection];
        if (script.length() > 24) script = script.substring(0, 23) + "~";
        tft.setCursor(5, 18);
        tft.setTextColor(ST77XX_GRAY, ST77XX_BLACK);
        tft.print(script);

        // Wrap captured output to 25 characters and keep the final nine rows,
        // where summaries and errors normally appear.
        String rows[32];
        size_t rowCount = 0;
        String row;
        for (size_t i = 0; i <= luaOutput.length(); ++i) {
            if (i == luaOutput.length() && !row.length()) break;
            const char c = i < luaOutput.length() ? luaOutput[i] : '\n';
            if (c == '\r') continue;
            if (c == '\n' || row.length() >= 25) {
                if (rowCount < 32) rows[rowCount++] = row;
                else {
                    for (size_t j = 1; j < 32; ++j) rows[j - 1] = rows[j];
                    rows[31] = row;
                }
                row = "";
                if (c != '\n') row += c;
            } else {
                row += c;
            }
        }
        const size_t maxScroll = rowCount > 8 ? rowCount - 8 : 0;
        const size_t scroll = min<size_t>(luaOutputScroll, maxScroll);
        const size_t end = rowCount - scroll;
        const size_t first = end > 8 ? end - 8 : 0;
        for (size_t i = first; i < end; ++i) {
            tft.setCursor(5, 31 + static_cast<int>(i - first) * 9);
            tft.setTextColor(luaRunStatus.startsWith("ERROR")
                                 ? SPECTRUM_CRITICAL : ST77XX_WHITE,
                             ST77XX_BLACK);
            tft.print(rows[i]);
        }
        drawModernFooter("U/D SCR", "A RERUN", "B LIST");
        return;
    }
    if (!luaScriptCount && luaRunStatus.length() == 0) {
        luaScriptCount = luaEngine.listScripts(luaScripts, LUA_UI_MAX_SCRIPTS);
        luaScriptSelection = 0;
    }
    drawModernHeader("LUA SCRIPTS", SPECTRUM_ACCENT);
    tft.fillRect(0, 15, 160, 90, ST77XX_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_GRAY, ST77XX_BLACK);
    tft.setCursor(5, 19); tft.print("SD:/RFSuite/scripts");

    if (!luaEngine.isReady()) {
        tft.setTextColor(SPECTRUM_CRITICAL, ST77XX_BLACK);
        tft.setCursor(8, 45); tft.print("SD CARD NOT READY");
    } else if (!luaScriptCount) {
        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        tft.setCursor(12, 43); tft.print("NO .LUA FILES");
        tft.setTextColor(ST77XX_GRAY, ST77XX_BLACK);
        tft.setCursor(7, 58); tft.print("A TO REFRESH");
    } else {
        const int first = max(0, static_cast<int>(luaScriptSelection) - 2);
        for (int row = 0; row < 5 && first + row < static_cast<int>(luaScriptCount); ++row) {
            const int index = first + row;
            const int y = 32 + row * 13;
            const bool selected = index == static_cast<int>(luaScriptSelection);
            if (selected) tft.fillRoundRect(3, y - 2, 154, 12, 2, SPECTRUM_CARD_BG);
            tft.setTextColor(selected ? SPECTRUM_ACCENT : ST77XX_WHITE,
                             selected ? SPECTRUM_CARD_BG : ST77XX_BLACK);
            tft.setCursor(7, y); tft.print(selected ? "> " : "  ");
            String label = luaScripts[index];
            if (label.length() > 20) label = label.substring(0, 19) + "~";
            tft.print(label);
        }
    }
    if (luaRunStatus.length()) {
        String status = luaRunStatus;
        if (status.length() > 25) status = status.substring(0, 24) + "~";
        tft.setTextColor(status.startsWith("ERROR") ? SPECTRUM_CRITICAL : SPECTRUM_LOW,
                         ST77XX_BLACK);
        tft.setCursor(4, 94); tft.print(status);
    }
    drawModernFooter("U/D SEL", "A RUN", "B BACK");
}

void DisplayManager::loadFileExplorerDirectory() {
    fileEntryCount = 0;
    fileSelection = 0;
    if (!storageManager.usingSd()) return;

    // SD.begin() registers the Arduino SD filesystem at /sd in ESP-IDF's VFS.
    // Enumerate through POSIX instead of File::openNextFile(): some FAT cards
    // expose valid entries with d_type == DT_UNKNOWN, which the Arduino FS
    // iterator silently skips.
    const String vfsPath = String("/sd") + filePath;
    DIR* dir = opendir(vfsPath.c_str());
    if (!dir) {
        fileStatus = "CANNOT OPEN FOLDER";
        Serial.printf("File explorer: opendir failed for %s\n", vfsPath.c_str());
        return;
    }

    for (dirent* entry = readdir(dir);
         entry && fileEntryCount < FILE_UI_MAX_ENTRIES;
         entry = readdir(dir)) {
        const String name(entry->d_name);
        if (!name.length() || name == "." || name == "..") continue;

        String entryPath = vfsPath;
        if (!entryPath.endsWith("/")) entryPath += "/";
        entryPath += name;
        struct stat info {};
        const bool hasInfo = stat(entryPath.c_str(), &info) == 0;
        fileNames[fileEntryCount] = name;
        fileDirectories[fileEntryCount] = entry->d_type == DT_DIR ||
            (hasInfo && S_ISDIR(info.st_mode));
        fileSizes[fileEntryCount] = fileDirectories[fileEntryCount] || !hasInfo
            ? 0 : static_cast<uint32_t>(info.st_size);
        Serial.printf("File explorer: %s %s%s\n",
                      fileDirectories[fileEntryCount] ? "DIR " : "FILE",
                      name.c_str(), hasInfo ? "" : " (no metadata)");
        ++fileEntryCount;
    }
    closedir(dir);
    fileStatus = fileEntryCount == FILE_UI_MAX_ENTRIES ? "SHOWING FIRST 32" : "";
    Serial.printf("File explorer: %u entries in %s\n",
                  static_cast<unsigned>(fileEntryCount), filePath.c_str());

    // Directories first, then alphabetical order.
    for (size_t i = 0; i < fileEntryCount; ++i) {
        for (size_t j = i + 1; j < fileEntryCount; ++j) {
            const bool swapNeeded =
                (fileDirectories[j] && !fileDirectories[i]) ||
                (fileDirectories[j] == fileDirectories[i] &&
                 fileNames[j].compareTo(fileNames[i]) < 0);
            if (!swapNeeded) continue;
            String swapName = fileNames[i]; fileNames[i] = fileNames[j]; fileNames[j] = swapName;
            const uint32_t swapSize = fileSizes[i]; fileSizes[i] = fileSizes[j]; fileSizes[j] = swapSize;
            const bool swapDir = fileDirectories[i]; fileDirectories[i] = fileDirectories[j]; fileDirectories[j] = swapDir;
        }
    }
}

void DisplayManager::renderFileExplorerScreen() {
    if (!fileEntryCount && fileStatus.length() == 0) loadFileExplorerDirectory();
    drawModernHeader("FILE EXPLORER", SPECTRUM_ACCENT);
    tft.fillRect(0, 15, 160, 90, ST77XX_BLACK);
    tft.setTextSize(1);
    String shownPath = String("SD:") + filePath;
    if (shownPath.length() > 25) shownPath = "~" + shownPath.substring(shownPath.length() - 24);
    tft.setCursor(4, 18);
    tft.setTextColor(ST77XX_GRAY, ST77XX_BLACK);
    tft.print(shownPath);

    if (!storageManager.usingSd()) {
        drawEmptyState("SD NOT READY", "Recorder uses LittleFS.", "", "B BACK");
    } else if (!fileEntryCount) {
        drawEmptyState(fileStatus.length() ? fileStatus.c_str() : "EMPTY FOLDER",
                       "Copy files to the SD card.", "", "B BACK");
    } else {
        const int first = max(0, static_cast<int>(fileSelection) - 2);
        for (int row = 0; row < 5 && first + row < static_cast<int>(fileEntryCount); ++row) {
            const int index = first + row;
            const int y = 31 + row * 13;
            const bool selected = index == static_cast<int>(fileSelection);
            if (selected) tft.fillRoundRect(3, y - 2, 154, 12, 2, SPECTRUM_CARD_BG);
            tft.setCursor(6, y);
            tft.setTextColor(selected ? SPECTRUM_ACCENT : ST77XX_WHITE,
                             selected ? SPECTRUM_CARD_BG : ST77XX_BLACK);
            tft.print(selected ? ">" : " ");
            tft.print(fileDirectories[index] ? "[D] " : "[F] ");
            String label = fileNames[index];
            if (label.length() > 20) label = label.substring(0, 19) + "~";
            tft.print(label);
        }
    }
    if (fileStatus.length() && fileEntryCount) {
        tft.fillRect(0, 94, 160, 10, ST77XX_BLACK);
        tft.setCursor(5, 95); tft.setTextColor(SPECTRUM_LOW, ST77XX_BLACK);
        tft.print(fileStatus);
    }
    drawModernFooter("U/D SEL", "A OPEN", "B BACK");
}

namespace {
constexpr uint32_t RFV_HEADER_SIZE = 14;

bool readExact(File& file, void* destination, size_t length) {
    return file.read(static_cast<uint8_t*>(destination), length) ==
           static_cast<int>(length);
}
}

bool DisplayManager::openVideo(const String& path) {
    closeVideo();
    videoFile = storageManager.filesystem().open(path, FILE_READ);
    if (!videoFile) return false;

    char magic[4];
    uint8_t reserved = 0;
    if (!readExact(videoFile, magic, sizeof(magic)) || memcmp(magic, "RFV1", 4) != 0 ||
        !readExact(videoFile, &videoWidth, sizeof(videoWidth)) ||
        !readExact(videoFile, &videoHeight, sizeof(videoHeight)) ||
        !readExact(videoFile, &videoFps, sizeof(videoFps)) ||
        !readExact(videoFile, &reserved, sizeof(reserved)) ||
        !readExact(videoFile, &videoFrameCount, sizeof(videoFrameCount)) ||
        videoWidth == 0 || videoWidth > 152 || videoHeight == 0 || videoHeight > 86 ||
        videoFps == 0 || videoFps > 30 || videoFrameCount == 0) {
        closeVideo();
        return false;
    }
    const uint64_t expected = RFV_HEADER_SIZE +
        static_cast<uint64_t>(videoWidth) * videoHeight * 2U * videoFrameCount;
    if (videoFile.size() < expected) { closeVideo(); return false; }

    videoName = path.substring(path.lastIndexOf('/') + 1);
    videoFrame = 0;
    videoNextFrameMs = 0;
    videoLayoutDrawn = false;
    return true;
}

void DisplayManager::closeVideo() {
    if (videoFile) videoFile.close();
    videoWidth = videoHeight = 0; videoFps = 0;
    videoFrameCount = videoFrame = 0; videoLayoutDrawn = false;
}

void DisplayManager::skipVideo(uint32_t seconds) {
    if (!videoFile || !videoFrameCount) return;
    const uint32_t advance = static_cast<uint32_t>(videoFps) * seconds;
    videoFrame = min(videoFrame + advance, videoFrameCount - 1);
    videoNextFrameMs = 0;
}

void DisplayManager::renderVideoPlayer() {
    if (!videoFile) {
        appState.appMode = APP_MODE_FILE_EXPLORER;
        needRedraw = true;
        return;
    }
    if (!videoLayoutDrawn) {
        drawModernHeader("SD VIDEO", SPECTRUM_ACCENT);
        tft.fillRect(3, 16, 154, 88, ST77XX_BLACK);
        tft.drawRect(2, 15, 156, 90, SPECTRUM_BORDER);
        drawModernFooter("", "A +10S", "B BACK");
        videoLayoutDrawn = true;
    }
    const uint32_t now = millis();
    if (videoNextFrameMs && static_cast<int32_t>(now - videoNextFrameMs) < 0) return;

    const uint32_t frameBytes = static_cast<uint32_t>(videoWidth) * videoHeight * 2U;
    if (!videoFile.seek(RFV_HEADER_SIZE + frameBytes * videoFrame)) {
        fileStatus = "VIDEO SEEK ERROR"; closeVideo(); return;
    }
    const int x = 4 + (152 - videoWidth) / 2;
    const int y = 17 + (86 - videoHeight) / 2;
    for (uint16_t row = 0; row < videoHeight; ++row) {
        if (!readExact(videoFile, videoLine, videoWidth * sizeof(uint16_t))) {
            fileStatus = "VIDEO READ ERROR"; closeVideo(); return;
        }
        tft.drawRGBBitmap(x, y + row, videoLine, videoWidth, 1);
    }
    videoFrame = (videoFrame + 1) % videoFrameCount;
    videoNextFrameMs = now + max<uint32_t>(1, 1000U / videoFps);
}

bool DisplayManager::openPhoto(size_t explorerIndex) {
    if (explorerIndex >= fileEntryCount || fileDirectories[explorerIndex]) return false;
    String lowerName = fileNames[explorerIndex]; lowerName.toLowerCase();
    if (!lowerName.endsWith(".rfi")) return false;

    closePhoto();
    String path = filePath;
    if (path != "/") path += "/";
    path += fileNames[explorerIndex];
    photoFile = storageManager.filesystem().open(path, FILE_READ);
    char magic[4];
    if (!photoFile || !readExact(photoFile, magic, sizeof(magic)) ||
        memcmp(magic, "RFI1", 4) != 0 ||
        !readExact(photoFile, &photoWidth, sizeof(photoWidth)) ||
        !readExact(photoFile, &photoHeight, sizeof(photoHeight)) ||
        photoWidth == 0 || photoWidth > 152 || photoHeight == 0 || photoHeight > 86 ||
        photoFile.size() < 8U + static_cast<uint32_t>(photoWidth) * photoHeight * 2U) {
        closePhoto();
        return false;
    }
    photoExplorerIndex = explorerIndex;
    photoNeedsDraw = true;
    return true;
}

bool DisplayManager::changePhoto(int direction) {
    if (!fileEntryCount) return false;
    size_t candidate = photoExplorerIndex;
    for (size_t checked = 0; checked < fileEntryCount; ++checked) {
        candidate = direction > 0
            ? (candidate + 1) % fileEntryCount
            : (candidate + fileEntryCount - 1) % fileEntryCount;
        String name = fileNames[candidate]; name.toLowerCase();
        if (!fileDirectories[candidate] && name.endsWith(".rfi") && openPhoto(candidate))
            return true;
    }
    return false;
}

void DisplayManager::closePhoto() {
    if (photoFile) photoFile.close();
    photoWidth = photoHeight = 0;
    photoNeedsDraw = false;
}

void DisplayManager::renderPhotoViewer() {
    if (!photoFile) {
        appState.appMode = APP_MODE_FILE_EXPLORER;
        needRedraw = true;
        return;
    }
    if (!photoNeedsDraw) return;
    drawModernHeader("SD PHOTO", SPECTRUM_ACCENT);
    tft.fillRect(3, 16, 154, 88, ST77XX_BLACK);
    tft.drawRect(2, 15, 156, 90, SPECTRUM_BORDER);
    drawModernFooter("U/D PHOTO", "", "B BACK");
    if (!photoFile.seek(8)) { closePhoto(); return; }

    const int x = 4 + (152 - photoWidth) / 2;
    const int y = 17 + (86 - photoHeight) / 2;
    for (uint16_t row = 0; row < photoHeight; ++row) {
        if (!readExact(photoFile, videoLine, photoWidth * sizeof(uint16_t))) {
            closePhoto(); return;
        }
        tft.drawRGBBitmap(x, y + row, videoLine, photoWidth, 1);
    }
    photoNeedsDraw = false;
}
