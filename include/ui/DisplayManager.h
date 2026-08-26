#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <FS.h>
#include "config/Config.h"
#include "core/AppState.h"

class DisplayManager {
public:
    DisplayManager();
    void init();
    void processInput();
    void updateUI();
    void requestRedraw();
    void showSplash();
    void prepareForShutdown();
    bool replaySubGhzFile(const String& name);
    void luaGuiBegin(const char* title);
    void luaGuiFooter(const char* left, const char* middle, const char* right);
    void luaGuiClear();
    void luaGuiClose();
    void luaGuiText(int x, int y, const char* text, const char* color);
    void luaGuiPixel(int x, int y, const char* color);
    void luaGuiLine(int x0, int y0, int x1, int y1, const char* color);
    void luaGuiRect(int x, int y, int width, int height, const char* color, bool filled);
    void luaGuiCircle(int x, int y, int radius, const char* color, bool filled);

private:
    Adafruit_ST7735 tft;
    int menuSelection = 0;
    int menuPage = 0;
    int menuScrollOffset = 0;
    int bandSelection = 0;
    int mainMenuScrollOffset = 0;
    int previousBandSelection = 0;
    int previousMainMenuScrollOffset = 0;
    bool mainMenuNeedsPartialRedraw = false;
    int subGhzPreset = 1;
    int subGhzMenuSelection = 0;
    int previousSubGhzMenuSelection = 0;
    int subGhzMenuScrollOffset = 0;
    int previousSubGhzMenuScrollOffset = 0;
    bool subGhzMenuNeedsPartialRedraw = false;
    static constexpr size_t SUBGHZ_UI_MAX_FILES = 20;
    String subGhzFiles[SUBGHZ_UI_MAX_FILES];
    size_t subGhzFileCount = 0;
    size_t subGhzFileSelection = 0;
    size_t previousSubGhzFileSelection = 0;
    size_t subGhzFileScrollOffset = 0;
    size_t previousSubGhzFileScrollOffset = 0;
    bool subGhzEmulateNeedsPartialRedraw = false;
    bool subGhzDeleteArmed = false;
    bool subGhzReplayActive = false;
    bool subGhzReplayFinished = false;
    bool subGhzReplaySucceeded = false;
    String subGhzReplayFile;
    uint8_t subGhzReplayFrame = 0;
    unsigned long lastSubGhzReplayFrameMs = 0;
    int settingsSelection = 0;
    int statusPage = 0;
    int powerSelection = 0;
    uint8_t envEventScroll = 0;
    uint8_t envBandChannel = 42;
    uint8_t probeSelection = 0;
    static constexpr size_t LUA_UI_MAX_SCRIPTS = 32;
    String luaScripts[LUA_UI_MAX_SCRIPTS];
    size_t luaScriptCount = 0;
    size_t luaScriptSelection = 0;
    String luaRunStatus;
    String luaOutput;
    bool luaShowingOutput = false;
    uint8_t luaOutputScroll = 0;
    bool luaShowingGui = false;
    bool luaReturnToListRequested = false;
    static constexpr size_t FILE_UI_MAX_ENTRIES = 32;
    String fileNames[FILE_UI_MAX_ENTRIES];
    uint32_t fileSizes[FILE_UI_MAX_ENTRIES] = {};
    bool fileDirectories[FILE_UI_MAX_ENTRIES] = {};
    size_t fileEntryCount = 0;
    size_t fileSelection = 0;
    String filePath = "/";
    String fileStatus;
    File videoFile;
    String videoName;
    uint16_t videoWidth = 0;
    uint16_t videoHeight = 0;
    uint8_t videoFps = 0;
    uint32_t videoFrameCount = 0;
    uint32_t videoFrame = 0;
    uint32_t videoNextFrameMs = 0;
    bool videoLayoutDrawn = false;
    uint16_t videoLine[152] = {};
    File photoFile;
    uint16_t photoWidth = 0;
    uint16_t photoHeight = 0;
    size_t photoExplorerIndex = 0;
    bool photoNeedsDraw = false;
    bool needRedraw = true;
    unsigned long lastStatusFlash = 0;
    bool flashState = false;

    // Partial-redraw support for main menu navigation
    bool menuNeedsPartialRedraw = false;
    int prevMenuSelection = 0;
    int prevMenuScrollOffset = 0;

    // Dynamic-screen caches. Only changed pixels/regions are sent over SPI,
    // which avoids visible clearing and keeps the UI responsive.
    uint8_t previousSpectrumLevels[TOTAL_CHANNELS];
    uint8_t previousPeakLevels[TOTAL_CHANNELS];
    uint8_t previousHeaderPeakLevel = 0xFF;
    int previousHeaderPeakChannel = -1;
    uint8_t previousHeaderRadio1Level = 0xFF;
    uint8_t previousHeaderRadio2Level = 0xFF;
    int previousSpectrumCursorX = -1;
    uint8_t previousInspectedLevel = 0xFF;
    uint8_t previousInspectedPeak = 0xFF;
    bool previousCarrierDetected = false;
    bool carrierStatusValid = false;
    unsigned long lastSpectrumRenderMs = 0;
    unsigned long lastInspectorRenderMs = 0;
    unsigned long lastJammerRenderMs = 0;
    unsigned long lastStatusRenderMs = 0;
    unsigned long lastEnvRenderMs = 0;
    unsigned long lastSnifferRenderMs = 0;
    uint8_t previousEnvHistoryHead = 0xFF;
    uint8_t previousEnvEventHead = 0xFF;
    uint8_t previousEnvEventScroll = 0xFF;
    uint8_t previousEnvTopChannels[5] = {0xFF,0xFF,0xFF,0xFF,0xFF};
    uint8_t previousEnvTopLevels[5] = {0xFF,0xFF,0xFF,0xFF,0xFF};
    uint8_t previousEnvAverage = 0xFF;
    uint8_t previousEnvScore = 0xFF;
    uint8_t previousCompareChannels[4] = {0xFF,0xFF,0xFF,0xFF};
    uint8_t previousCompareLevels[4] = {0xFF,0xFF,0xFF,0xFF};
    uint8_t previousCompareScores[4] = {0xFF,0xFF,0xFF,0xFF};
    uint16_t previousEnvBursts = 0xFFFF;
    uint8_t previousEnvPeakChannel = 0xFF;
    uint8_t previousEnvBandChannel = 0xFF;
    uint8_t previousSnapshotChannel = 0xFF;
    uint32_t previousBeforeCapturedMs = 0xFFFFFFFF;
    uint32_t previousAfterCapturedMs = 0xFFFFFFFF;
    bool previousEnvRunning = false;
    bool envRunningStatusValid = false;
    bool envLayoutDrawn = false;
    bool subRecordLayoutDrawn = false;
    bool subRfTestLayoutDrawn = false;
    bool subAnalyzerLayoutDrawn = false;
    bool subPacketLayoutDrawn = false;
    int16_t previousSubAnalyzerLevels[20] = {};
    int8_t previousSubAnalyzerPeakIndex = -1;
    uint32_t previousSubPacketCount = 0xFFFFFFFF;
    uint32_t previousSubPulseCount = 0xFFFFFFFF;
    uint32_t subGraphProcessedPulses = 0;
    int subGraphX = 8;
    bool subGraphLevel = false;
    bool subGraphInitialized = false;
    int previousSubFrequencyKhz = -1;
    int previousSubRecordState = -1;
    int previousSubTestState = -1;

    // Tracks page transitions separately from content changes. A full clear is
    // only needed when a different page replaces the current layout.
    int renderedMode = -1;
    bool jammerLayoutDrawn = false;
    bool settingsLayoutDrawn = false;
    bool powerLayoutDrawn = false;
    bool snifferLayoutDrawn = false;
    bool subPresetLayoutDrawn = false;
    int previousJammerTarget = -1;
    int previousJamChannel = -1;
    int previousJamChannel2 = -1;
    int previousPowerLevel = -1;
    int previousDwellTimeUs = -1;
    int previousSettingsSelection = -1;
    int previousSettingsTheme = -1;
    int previousSettingsMenuLayout = -1;
    int previousSettingsSniffSave = -1;
    int previousPowerSelection = -1;
    int previousSubPreset = -1;
    int previousSubRegion = -1;
    int previousSubTrigger = 999;
    int previousSubRepeats = -1;
    bool previousSnifferRunning = false;
    bool snifferRunningValid = false;
    uint8_t previousSnifferChannel = 0xFF;
    int previousSnifferRate = -1;
    uint32_t previousSnifferPackets = 0xFFFFFFFF;
    String previousSnifferText;
    bool previousJamming = false;
    bool jammingStatusValid = false;
    int renderedStatusPage = -1;
    String previousStatusValues[6];
    uint16_t previousStatusColors[6] = {0, 0, 0, 0, 0, 0};

    // Screen Renderers
    void renderMainMenu();
    void renderBandSelector();
    void renderSubGhzScreen();
    void renderSubGhzOfflinePopup();
    void drawMainMenuItem(int featureIndex, int slot, bool selected);
    void drawSubGhzMenuItem(int index, bool selected);
    void redrawMainMenuItems();
    void redrawSubGhzMenuItems();
    void drawSubGhzFileItem(size_t index, bool selected);
    void redrawSubGhzFileItems();
    void renderSubGhzRecordScreen();
    void renderSubGhzEmulateScreen();
    void renderSubGhzReplayAnimation();
    void renderSubGhzReplayResult();
    void renderSubGhzAnalyzerScreen();
    void renderSubGhzPresetsScreen();
    void renderSubGhzPacketScreen();
    void renderSubGhzRfTestScreen();
    void renderJammerScreen();
    void renderSpectrumAnalyzer();
    void renderWaterfallScreen();
    void renderChannelInspector();
    void renderSurveyScreen();
    void renderEventsScreen();
    void renderLoggingScreen();
    void renderRadioDiagScreen();
    void renderProfilesScreen();
    void renderSettingsScreen();
    void renderStatusScreen();
    void renderPowerScreen();
    void renderRfEnvironmentScreen();
    void renderLuaScriptsScreen();
    void renderFileExplorerScreen();
    void renderPacketSnifferScreen();
    void renderVideoPlayer();
    void renderPhotoViewer();
    void renderRebootScreen();
    void renderShutdownScreen();

    // Graphics Helpers
    uint16_t getSignalColor(uint8_t level);
    void drawSpectrumGrid();
    void drawSpectrumBars();
    void drawMenuItem(int index, bool selected);
    void drawMenuIcon(int index, int centerX, int centerY, uint16_t color, uint16_t background);
    void redrawMenuItems(int oldSel, int newSel);
    void resetDynamicCaches();
    void drawModernHeader(const char* title, uint16_t accent);
    void drawModernFooter(const char* left, const char* middle, const char* right);
    void drawFooterChip(int x, int width, const char* label);
    void loadFileExplorerDirectory();
    bool openVideo(const String& path);
    void closeVideo();
    void skipVideo(uint32_t seconds);
    bool openPhoto(size_t explorerIndex);
    bool changePhoto(int direction);
    void closePhoto();
    uint16_t luaGuiColor(const char* name) const;
};

extern DisplayManager displayManager;
