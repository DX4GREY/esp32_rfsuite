#include "ui/DisplayManager.h"
#include "drivers/ButtonManager.h"
#include "drivers/RadioManager.h"
#include "drivers/Cc1101Manager.h"
#include "ui/MenuCatalog.h"
#include "services/SessionRecorder.h"
#include "services/RfEnvironmentAnalyzer.h"
#include "core/RfEnvironmentState.h"
#include "services/RfAuthorizedProbe.h"
#include "services/LuaEngine.h"
#include "services/StorageManager.h"
#include "services/PacketSniffer.h"
#include "services/SubGhzRawService.h"

void yieldToUI();

namespace {
class LuaDisplayStream : public Stream {
public:
    explicit LuaDisplayStream(String& captured) : buffer(captured) { buffer = ""; }

    size_t write(uint8_t value) override {
        Serial.write(value);
        buffer += static_cast<char>(value);
        if (buffer.length() > 1024) buffer.remove(0, buffer.length() - 1024);
        return 1;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override { Serial.flush(); }

private:
    String& buffer;
};
}

void DisplayManager::updateUI() {
    const int currentMode = static_cast<int>(appState.appMode);
    if (renderedMode != currentMode) {
        // A page transition is the only time the complete framebuffer area is
        // cleared. Updates within a page use the dirty regions below.
        tft.fillScreen(ST77XX_BLACK);
        renderedMode = currentMode;
        resetDynamicCaches();
        needRedraw = true;
        menuNeedsPartialRedraw = false;
        mainMenuNeedsPartialRedraw = false;
        subGhzMenuNeedsPartialRedraw = false;
        subGhzEmulateNeedsPartialRedraw = false;
    }

    switch (appState.appMode) {
        case APP_MODE_BAND_SELECT:
            if (mainMenuNeedsPartialRedraw) {
                redrawMainMenuItems(); mainMenuNeedsPartialRedraw = false;
            } else if (needRedraw) { renderBandSelector(); needRedraw = false; }
            break;
        case APP_MODE_MENU:
            if (menuNeedsPartialRedraw) {
                redrawMenuItems(prevMenuSelection, menuSelection);
                menuNeedsPartialRedraw = false;
            } else if (needRedraw) {
                renderMainMenu();
                needRedraw = false;
                menuNeedsPartialRedraw = false;
            }
            break;
        case APP_MODE_SUBGHZ_OFFLINE:
            if (needRedraw) { renderSubGhzOfflinePopup(); needRedraw = false; }
            break;
        case APP_MODE_SUBGHZ:
            if (subGhzMenuNeedsPartialRedraw) {
                redrawSubGhzMenuItems(); subGhzMenuNeedsPartialRedraw = false;
            } else if (needRedraw) {
                renderSubGhzScreen();
                needRedraw = false;
            }
            break;
        case APP_MODE_SUBGHZ_RECORD:
            if (needRedraw || millis() - lastStatusRenderMs >= 200) {
                renderSubGhzRecordScreen(); lastStatusRenderMs = millis(); needRedraw = false;
            }
            break;
        case APP_MODE_SUBGHZ_ANALYZER:
            if (needRedraw || millis() - lastStatusRenderMs >= 150) {
                renderSubGhzAnalyzerScreen(); lastStatusRenderMs = millis(); needRedraw = false;
            }
            break;
        case APP_MODE_SUBGHZ_EMULATE:
            if (subGhzReplayActive) {
                renderSubGhzReplayAnimation(); needRedraw = false;
            } else if (subGhzReplayFinished) {
                if (needRedraw) { renderSubGhzReplayResult(); needRedraw = false; }
            } else if (subGhzEmulateNeedsPartialRedraw) {
                redrawSubGhzFileItems(); subGhzEmulateNeedsPartialRedraw = false;
            } else if (needRedraw) { renderSubGhzEmulateScreen(); needRedraw = false; }
            break;
        case APP_MODE_SUBGHZ_PRESETS:
            if (needRedraw) { renderSubGhzPresetsScreen(); needRedraw = false; }
            break;
        case APP_MODE_SUBGHZ_PACKET:
            if (needRedraw || millis() - lastStatusRenderMs >= 150) {
                renderSubGhzPacketScreen(); lastStatusRenderMs = millis(); needRedraw = false;
            }
            break;
        case APP_MODE_SUBGHZ_RF_TEST:
            if (needRedraw || millis() - lastStatusRenderMs >= 150) {
                renderSubGhzRfTestScreen(); lastStatusRenderMs = millis(); needRedraw = false;
            }
            break;
        case APP_MODE_JAMMER:
            if (needRedraw || lastJammerRenderMs == 0 ||
                millis() - lastJammerRenderMs >= 100) {
                renderJammerScreen();
                lastJammerRenderMs = millis();
                needRedraw = false;
            }
            break;
        case APP_MODE_ANALYZER_SPECTRUM:
            renderSpectrumAnalyzer();
            break;
        case APP_MODE_WATERFALL:
            renderWaterfallScreen();
            break;
        case APP_MODE_ANALYZER_CHANNEL:
            renderChannelInspector();   // per-frame dynamic update (static part redrawn internally)
            break;
        case APP_MODE_SURVEY:
            renderSurveyScreen();
            break;
        case APP_MODE_EVENTS:
            renderEventsScreen();
            break;
        case APP_MODE_LOGGING:
            if (needRedraw) renderLoggingScreen();
            break;
        case APP_MODE_RADIO_DIAG:
            if (needRedraw) renderRadioDiagScreen();
            break;
        case APP_MODE_PROFILES:
            if (needRedraw) renderProfilesScreen();
            break;
        case APP_MODE_SETTINGS:
            if (needRedraw) {
                renderSettingsScreen();
                needRedraw = false;
            }
            break;
        case APP_MODE_STATUS:
            if (needRedraw || lastStatusRenderMs == 0 ||
                millis() - lastStatusRenderMs >= 1000) {
                renderStatusScreen();
                needRedraw = false;
            }
            break;
        case APP_MODE_POWER:
            if (needRedraw) {
                renderPowerScreen();
                needRedraw = false;
            }
            break;
        case APP_MODE_ENV_OCCUPANCY:
        case APP_MODE_ENV_COMPARE:
        case APP_MODE_ENV_STATUS:
            if (needRedraw || (rfEnvironmentState.running &&
                millis() - lastEnvRenderMs >= 1000)) {
                renderRfEnvironmentScreen(); lastEnvRenderMs = millis(); needRedraw = false;
            }
            break;
        case APP_MODE_ENV_HEATMAP:
            if (needRedraw || previousEnvHistoryHead != rfEnvironmentState.historyHead) {
                renderRfEnvironmentScreen(); lastEnvRenderMs = millis(); needRedraw = false;
                previousEnvHistoryHead = rfEnvironmentState.historyHead;
            }
            break;
        case APP_MODE_ENV_BURSTS:
            if (needRedraw || previousEnvEventHead != rfEnvironmentState.eventHead ||
                previousEnvEventScroll != envEventScroll) {
                renderRfEnvironmentScreen(); lastEnvRenderMs = millis(); needRedraw = false;
                previousEnvEventHead = rfEnvironmentState.eventHead;
                previousEnvEventScroll = envEventScroll;
            }
            break;
        case APP_MODE_ENV_BEFORE_AFTER:
        case APP_MODE_ENV_BAND_INFO:
            if (needRedraw) {
                renderRfEnvironmentScreen(); lastEnvRenderMs = millis(); needRedraw = false;
            }
            break;
        case APP_MODE_ENV_PROBE:
            if (needRedraw || (rfAuthorizedProbe.isRunning() &&
                millis() - lastEnvRenderMs >= 500)) {
                renderRfEnvironmentScreen(); lastEnvRenderMs = millis(); needRedraw = false;
            }
            break;
        case APP_MODE_LUA_SCRIPTS:
            if (needRedraw) { renderLuaScriptsScreen(); needRedraw = false; }
            break;
        case APP_MODE_FILE_EXPLORER:
            if (needRedraw) { renderFileExplorerScreen(); needRedraw = false; }
            break;
        case APP_MODE_PACKET_SNIFFER:
            if (needRedraw || millis() - lastSnifferRenderMs >= 200) {
                renderPacketSnifferScreen();
                lastSnifferRenderMs = millis();
                needRedraw = false;
            }
            break;
        case APP_MODE_VIDEO_PLAYER:
            renderVideoPlayer();
            needRedraw = false;
            break;
        case APP_MODE_PHOTO_VIEWER:
            renderPhotoViewer();
            needRedraw = false;
            break;
        case APP_MODE_REBOOT:
            renderRebootScreen();   // di-render tiap frame agar animasi titik hidup
            break;
        case APP_MODE_SHUTDOWN:
            renderShutdownScreen();
            break;
    }
}

// =============================================================================
// INPUT NAVIGATION
// =============================================================================
void DisplayManager::processInput() {
    // Every app-mode transition is an input boundary. If it was triggered on
    // button-down, consume the matching release so it cannot fire a second
    // action in the destination screen. The destructor runs on every return.
    struct ModeTransitionGuard {
        AppMode entryMode;
        ~ModeTransitionGuard() {
            if (appState.appMode != entryMode) buttonManager.suppressHeldButtons();
        }
    } modeTransitionGuard{appState.appMode};

    if (appState.appMode == APP_MODE_BAND_SELECT) {
        if (buttonManager.isPressed(BTN_UP)) {
            previousBandSelection = bandSelection;
            previousMainMenuScrollOffset = mainMenuScrollOffset;
            bandSelection = (bandSelection + 6) % 7;
            const int slot = bandSelection % 6;
            if (bandSelection == 6) mainMenuScrollOffset = 0;
            else if (slot < mainMenuScrollOffset) mainMenuScrollOffset = slot;
            else if (slot >= mainMenuScrollOffset + 4) mainMenuScrollOffset = slot - 3;
            if (previousBandSelection / 6 != bandSelection / 6) needRedraw = true;
            else mainMenuNeedsPartialRedraw = true;
        } else if (buttonManager.isPressed(BTN_DOWN)) {
            previousBandSelection = bandSelection;
            previousMainMenuScrollOffset = mainMenuScrollOffset;
            bandSelection = (bandSelection + 1) % 7;
            const int slot = bandSelection % 6;
            if (bandSelection == 6 || bandSelection == 0) mainMenuScrollOffset = 0;
            else if (slot >= mainMenuScrollOffset + 4) mainMenuScrollOffset = slot - 3;
            if (previousBandSelection / 6 != bandSelection / 6) needRedraw = true;
            else mainMenuNeedsPartialRedraw = true;
        } else if (buttonManager.isPressed(BTN_B)) {
            previousBandSelection = bandSelection;
            previousMainMenuScrollOffset = mainMenuScrollOffset;
            bandSelection = bandSelection < 6 ? 6 : 0;
            mainMenuScrollOffset = 0;
            mainMenuNeedsPartialRedraw = false;
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_A)) {
            radioManager.stopAll();
            if (bandSelection == 0) {
                appState.radioBand = RADIO_BAND_24_GHZ;
                if (radioManager.isConnected()) {
                    appState.simulationMode = false;
                    appState.appMode = APP_MODE_MENU;
                } else appState.appMode = APP_MODE_SUBGHZ_OFFLINE;
            } else if (bandSelection == 1) {
                appState.radioBand = RADIO_BAND_SUB_GHZ;
                if (cc1101Manager.isConnected()) {
                    appState.simulationMode = false;
                    subGhzRawService.setSimulationMode(false);
                    appState.appMode = APP_MODE_SUBGHZ;
                } else appState.appMode = APP_MODE_SUBGHZ_OFFLINE;
            } else if (bandSelection == 2) appState.appMode = APP_MODE_SETTINGS;
            else if (bandSelection == 3) appState.appMode = APP_MODE_STATUS;
            else if (bandSelection == 4) {
                luaScriptCount = 0; luaScriptSelection = 0; luaRunStatus = "";
                luaOutput = ""; luaShowingOutput = false; luaOutputScroll = 0;
                luaShowingGui = false; appState.appMode = APP_MODE_LUA_SCRIPTS;
            } else if (bandSelection == 5) {
                filePath = "/"; fileEntryCount = 0; fileSelection = 0; fileStatus = "";
                appState.appMode = APP_MODE_FILE_EXPLORER;
            } else appState.appMode = APP_MODE_POWER;
            needRedraw = true;
        }
        return;
    }

    if (appState.appMode == APP_MODE_SUBGHZ_OFFLINE) {
        if (buttonManager.isPressed(BTN_A)) {
            appState.simulationMode = true;
            const bool subGhz = appState.radioBand == RADIO_BAND_SUB_GHZ;
            subGhzRawService.setSimulationMode(subGhz);
            appState.appMode = subGhz ? APP_MODE_SUBGHZ : APP_MODE_MENU;
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_B)) {
            appState.simulationMode = false; subGhzRawService.setSimulationMode(false);
            appState.appMode = APP_MODE_BAND_SELECT;
            needRedraw = true;
        }
        return;
    }

    if (appState.appMode == APP_MODE_SUBGHZ) {
        if (buttonManager.isPressed(BTN_UP)) {
            previousSubGhzMenuSelection = subGhzMenuSelection;
            previousSubGhzMenuScrollOffset = subGhzMenuScrollOffset;
            subGhzMenuSelection = (subGhzMenuSelection + 5) % 6;
            if (subGhzMenuSelection < subGhzMenuScrollOffset)
                subGhzMenuScrollOffset = subGhzMenuSelection;
            if (subGhzMenuSelection == 5 && previousSubGhzMenuSelection == 0)
                subGhzMenuScrollOffset = 2;
            subGhzMenuNeedsPartialRedraw = true;
        } else if (buttonManager.isPressed(BTN_DOWN)) {
            previousSubGhzMenuSelection = subGhzMenuSelection;
            previousSubGhzMenuScrollOffset = subGhzMenuScrollOffset;
            subGhzMenuSelection = (subGhzMenuSelection + 1) % 6;
            if (subGhzMenuSelection >= subGhzMenuScrollOffset + 4)
                subGhzMenuScrollOffset = subGhzMenuSelection - 3;
            if (subGhzMenuSelection == 0) subGhzMenuScrollOffset = 0;
            subGhzMenuNeedsPartialRedraw = true;
        } else if (buttonManager.isPressed(BTN_A)) {
            if (subGhzMenuSelection == 0) {
                subGhzRawService.startAnalyzer(); appState.appMode = APP_MODE_SUBGHZ_ANALYZER;
            } else if (subGhzMenuSelection == 1) appState.appMode = APP_MODE_SUBGHZ_RECORD;
            else if (subGhzMenuSelection == 2) {
                subGhzFileCount = subGhzRawService.listFiles(subGhzFiles, SUBGHZ_UI_MAX_FILES);
                subGhzFileSelection = 0;
                subGhzFileScrollOffset = 0;
                appState.appMode = APP_MODE_SUBGHZ_EMULATE;
            } else if (subGhzMenuSelection == 3) appState.appMode = APP_MODE_SUBGHZ_PRESETS;
            else if (subGhzMenuSelection == 4) {
                subGhzRawService.startPacketAnalyzer(); appState.appMode = APP_MODE_SUBGHZ_PACKET;
            } else appState.appMode = APP_MODE_SUBGHZ_RF_TEST;
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_B)) {
            appState.appMode = APP_MODE_BAND_SELECT;
            needRedraw = true;
        }
        return;
    }

    static constexpr float subGhzPresets[] = {315.0f, 433.92f, 868.0f, 915.0f};
    if (appState.appMode == APP_MODE_SUBGHZ_ANALYZER) {
        if (buttonManager.isPressed(BTN_A)) {
            subGhzRawService.lockAnalyzerPeak();
            subGhzRawService.stopAnalyzer();
            appState.appMode = APP_MODE_SUBGHZ_RECORD; needRedraw = true;
        } else if (buttonManager.isPressed(BTN_B)) {
            subGhzRawService.stopAnalyzer(); appState.appMode = APP_MODE_SUBGHZ; needRedraw = true;
        }
        return;
    }
    if (appState.appMode == APP_MODE_SUBGHZ_RECORD) {
        if (!subGhzRawService.isRecording() && buttonManager.isPressed(BTN_UP)) {
            subGhzPreset = (subGhzPreset + 3) % 4;
            cc1101Manager.setFrequency(subGhzPresets[subGhzPreset]); needRedraw = true;
        } else if (!subGhzRawService.isRecording() && buttonManager.isPressed(BTN_DOWN)) {
            subGhzPreset = (subGhzPreset + 1) % 4;
            cc1101Manager.setFrequency(subGhzPresets[subGhzPreset]); needRedraw = true;
        } else if (buttonManager.isPressed(BTN_A)) {
            if (subGhzRawService.isRecording()) subGhzRawService.stopRecording();
            else subGhzRawService.startRecording(cc1101Manager.frequencyMHz());
            needRedraw = true;
        } else if (!subGhzRawService.isRecording() && buttonManager.isLongPressed(BTN_B)) {
            subGhzRawService.setAutoTrigger(!subGhzRawService.autoTriggerEnabled());
            appState.subGhzAutoTrigger = subGhzRawService.autoTriggerEnabled();
            appState.markSettingsDirty();
            needRedraw = true;
        } else if (buttonManager.isShortReleased(BTN_B)) {
            if (subGhzRawService.isRecording()) subGhzRawService.stopRecording();
            appState.appMode = APP_MODE_SUBGHZ; needRedraw = true;
        }
        return;
    }
    if (appState.appMode == APP_MODE_SUBGHZ_EMULATE) {
        auto runSelectedReplay = [&]() {
            subGhzReplayActive = true;
            subGhzReplayFinished = false;
            subGhzReplayFile = subGhzFiles[subGhzFileSelection];
            subGhzReplayFrame = 0;
            lastSubGhzReplayFrameMs = 0;
            needRedraw = true;
            updateUI();
            subGhzReplaySucceeded = subGhzRawService.replay(subGhzReplayFile, yieldToUI);
            subGhzReplayActive = false;
            subGhzReplayFinished = true;
            // Replay is a blocking state transition inside the same app mode,
            // so guard it explicitly as well.
            buttonManager.suppressHeldButtons();
            needRedraw = true;
        };

        // replay() yields back into this input handler while transmitting.
        // Keep that nested pass limited to cancellation so A cannot recursively
        // start the same file again.
        if (subGhzReplayActive) {
            if (buttonManager.isPressed(BTN_B)) subGhzRawService.stopReplay();
            return;
        }
        if (subGhzReplayFinished) {
            if (buttonManager.isShortReleased(BTN_A)) {
                runSelectedReplay();
            } else if (buttonManager.isShortReleased(BTN_B)) {
                subGhzReplayFinished = false;
                needRedraw = true;
            }
            return;
        }
        if (subGhzFileCount && buttonManager.readButton(BTN_UP) == LOW &&
            buttonManager.isPressed(BTN_A)) {
            String renamed;
            subGhzRawService.renameFile(subGhzFiles[subGhzFileSelection], renamed);
            subGhzFileCount = subGhzRawService.listFiles(subGhzFiles, SUBGHZ_UI_MAX_FILES);
            needRedraw = true;
        } else if (buttonManager.isLongPressed(BTN_UP) && subGhzFileCount) {
            subGhzRawService.exportSubFile(subGhzFiles[subGhzFileSelection]); needRedraw = true;
        } else if (buttonManager.isLongPressed(BTN_DOWN) && subGhzFileCount) {
            subGhzRawService.cleanFile(subGhzFiles[subGhzFileSelection]); needRedraw = true;
        } else if (buttonManager.isLongPressed(BTN_A) && subGhzFileCount) {
            subGhzRawService.toggleFavorite(subGhzFiles[subGhzFileSelection]); needRedraw = true;
        } else if (buttonManager.isLongPressed(BTN_B) && subGhzFileCount) {
            subGhzDeleteArmed = true; needRedraw = true;
        } else if (buttonManager.isShortReleased(BTN_UP) && subGhzFileCount) {
            previousSubGhzFileSelection = subGhzFileSelection;
            previousSubGhzFileScrollOffset = subGhzFileScrollOffset;
            subGhzFileSelection = (subGhzFileSelection + subGhzFileCount - 1) % subGhzFileCount;
            if (subGhzFileSelection < subGhzFileScrollOffset) subGhzFileScrollOffset = subGhzFileSelection;
            if (subGhzFileSelection + 1 == subGhzFileCount && previousSubGhzFileSelection == 0)
                subGhzFileScrollOffset = subGhzFileCount > 4 ? subGhzFileCount - 4 : 0;
            subGhzEmulateNeedsPartialRedraw = true;
        } else if (buttonManager.isShortReleased(BTN_DOWN) && subGhzFileCount) {
            previousSubGhzFileSelection = subGhzFileSelection;
            previousSubGhzFileScrollOffset = subGhzFileScrollOffset;
            subGhzFileSelection = (subGhzFileSelection + 1) % subGhzFileCount;
            if (subGhzFileSelection >= subGhzFileScrollOffset + 4)
                subGhzFileScrollOffset = subGhzFileSelection - 3;
            if (subGhzFileSelection == 0) subGhzFileScrollOffset = 0;
            subGhzEmulateNeedsPartialRedraw = true;
        } else if (buttonManager.isShortReleased(BTN_A) && subGhzFileCount) {
            if (subGhzDeleteArmed) {
                subGhzRawService.deleteFile(subGhzFiles[subGhzFileSelection]);
                subGhzFileCount = subGhzRawService.listFiles(subGhzFiles, SUBGHZ_UI_MAX_FILES);
                if (subGhzFileSelection >= subGhzFileCount && subGhzFileSelection) --subGhzFileSelection;
                subGhzDeleteArmed = false;
            } else runSelectedReplay();
            needRedraw = true;
        } else if (buttonManager.isShortReleased(BTN_B)) {
            if (subGhzDeleteArmed) { subGhzDeleteArmed = false; needRedraw = true; return; }
            subGhzRawService.stopReplay(); appState.appMode = APP_MODE_SUBGHZ; needRedraw = true;
        }
        return;
    }
    if (appState.appMode == APP_MODE_SUBGHZ_PRESETS) {
        if (buttonManager.isPressed(BTN_UP)) {
            int preset = static_cast<int>(cc1101Manager.preset());
            preset = (preset + 4) % 5;
            cc1101Manager.setPreset(static_cast<Cc1101Preset>(preset));
            appState.subGhzRadioPreset = preset; appState.markSettingsDirty(); needRedraw = true;
        } else if (buttonManager.isPressed(BTN_DOWN)) {
            int preset = (static_cast<int>(cc1101Manager.preset()) + 1) % 5;
            cc1101Manager.setPreset(static_cast<Cc1101Preset>(preset));
            appState.subGhzRadioPreset = preset; appState.markSettingsDirty(); needRedraw = true;
        } else if (buttonManager.isLongPressed(BTN_A)) {
            subGhzRawService.cycleTriggerThreshold();
            appState.subGhzTriggerThreshold = subGhzRawService.triggerThresholdDbm();
            appState.markSettingsDirty(); needRedraw = true;
        } else if (buttonManager.isShortReleased(BTN_A)) {
            subGhzRawService.cycleRegion();
            appState.subGhzRegion = static_cast<uint8_t>(subGhzRawService.region());
            appState.markSettingsDirty(); needRedraw = true;
        } else if (buttonManager.isLongPressed(BTN_B)) {
            subGhzRawService.cycleReplayRepeatCount();
            appState.subGhzReplayRepeats = subGhzRawService.replayRepeatCount();
            appState.markSettingsDirty(); needRedraw = true;
        } else if (buttonManager.isShortReleased(BTN_B)) {
            appState.appMode = APP_MODE_SUBGHZ; needRedraw = true;
        }
        return;
    }
    if (appState.appMode == APP_MODE_SUBGHZ_PACKET) {
        if (buttonManager.isPressed(BTN_UP)) {
            subGhzPreset = (subGhzPreset + 3) % 4;
            cc1101Manager.setFrequency(subGhzPresets[subGhzPreset]);
            subGhzRawService.startPacketAnalyzer(); needRedraw = true;
        } else if (buttonManager.isPressed(BTN_DOWN)) {
            subGhzPreset = (subGhzPreset + 1) % 4;
            cc1101Manager.setFrequency(subGhzPresets[subGhzPreset]);
            subGhzRawService.startPacketAnalyzer(); needRedraw = true;
        } else if (buttonManager.isPressed(BTN_A)) {
            subGhzRawService.startPacketAnalyzer(); needRedraw = true;
        } else if (buttonManager.isPressed(BTN_B)) {
            subGhzRawService.stopPacketAnalyzer(); appState.appMode = APP_MODE_SUBGHZ; needRedraw = true;
        }
        return;
    }
    if (appState.appMode == APP_MODE_SUBGHZ_RF_TEST) {
        if (buttonManager.isPressed(BTN_A)) {
            if (subGhzRawService.isRfTesting()) subGhzRawService.stopRfTest();
            else subGhzRawService.startRfTest();
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_B)) {
            subGhzRawService.stopRfTest(); appState.appMode = APP_MODE_SUBGHZ; needRedraw = true;
        }
        return;
    }

    // -------------------------------------------------------------------------
    // CONDITION 1: MAIN MENU
    // -------------------------------------------------------------------------
    if (appState.appMode == APP_MODE_MENU) {
        // Page changes are intentionally edge-triggered only. Using the same
        // DOWN press for both an edge and a later long-press event can advance
        // two pages when the button is held near the threshold.
        if (buttonManager.isPressed(BTN_UP)) {
            if (menuSelection == 0) {
                menuPage = (menuPage - 1 + MenuCatalog::PAGE_COUNT) %
                           MenuCatalog::PAGE_COUNT;
                menuSelection = MenuCatalog::pageItemCount(menuPage) - 1;
                prevMenuSelection = menuSelection;
                menuScrollOffset = appState.menuLayout == MENU_LAYOUT_LIST ?
                    max(0, MenuCatalog::pageItemCount(menuPage) - 4) : 0;
                prevMenuScrollOffset = menuScrollOffset;
                needRedraw = true;
                menuNeedsPartialRedraw = false;
                return;
            }
            prevMenuSelection = menuSelection;
            prevMenuScrollOffset = menuScrollOffset;
            menuSelection--;
            if (appState.menuLayout == MENU_LAYOUT_LIST && menuSelection < menuScrollOffset)
                menuScrollOffset = menuSelection;
            menuNeedsPartialRedraw = true;
        } else if (buttonManager.isPressed(BTN_DOWN)) {
            const int itemCount = MenuCatalog::pageItemCount(menuPage);
            if (menuSelection == itemCount - 1) {
                menuPage = (menuPage + 1) % MenuCatalog::PAGE_COUNT;
                menuSelection = 0;
                menuScrollOffset = prevMenuScrollOffset = 0;
                needRedraw = true;
                menuNeedsPartialRedraw = false;
                return;
            }
            prevMenuSelection = menuSelection;
            prevMenuScrollOffset = menuScrollOffset;
            menuSelection++;
            if (appState.menuLayout == MENU_LAYOUT_LIST && menuSelection >= menuScrollOffset + 4)
                menuScrollOffset = menuSelection - 3;
            menuNeedsPartialRedraw = true;
        } else if (buttonManager.isLongPressed(BTN_B)) {
            radioManager.stopAll();
            appState.appMode = APP_MODE_BAND_SELECT;
            needRedraw = true;
        } else if (buttonManager.isShortReleased(BTN_B)) {
            menuPage = (menuPage + 1) % MenuCatalog::PAGE_COUNT;
            menuSelection = 0;
            menuScrollOffset = prevMenuScrollOffset = 0;
            prevMenuSelection = 0;
            needRedraw = true;
            menuNeedsPartialRedraw = false;
        } else if (buttonManager.isPressed(BTN_A)) {
            const MenuFeature& feature =
                MenuCatalog::featureAt(menuPage, menuSelection);
            if (feature.mode != APP_MODE_ANALYZER_SPECTRUM) {
                appState.analyzerFrozen = false;
            }
            if (feature.openFlags & MENU_OPEN_STOP_RADIOS) {
                radioManager.stopAll();
            }
            if (feature.openFlags & MENU_OPEN_RESET_PEAKS) {
                appState.resetPeaks();
            }
            if (feature.openFlags & MENU_OPEN_RESET_INSPECTOR) {
                appState.inspectedPeak = 0;
            }
            if (feature.mode == APP_MODE_LUA_SCRIPTS) {
                luaScriptCount = 0; luaScriptSelection = 0; luaRunStatus = "";
                luaOutput = ""; luaShowingOutput = false; luaOutputScroll = 0;
                luaShowingGui = false;
            }
            if (feature.mode == APP_MODE_FILE_EXPLORER) {
                filePath = "/"; fileEntryCount = 0; fileSelection = 0;
                fileStatus = "";
            }
            if (feature.mode == APP_MODE_PACKET_SNIFFER && !appState.simulationMode) {
                radioManager.startPacketSniffer(40, SnifferDataRate::RATE_2_MBPS);
                lastSnifferRenderMs = 0;
            }
            appState.appMode = feature.mode;
            needRedraw = true;
        }
    }
    // -------------------------------------------------------------------------
    // CONDITION 2: JAMMER MODE
    // -------------------------------------------------------------------------
    else if (appState.appMode == APP_MODE_JAMMER) {
        if (buttonManager.isPressed(BTN_UP)) {
            appState.cycleJammerTarget(-1);
            if (appState.jamming) {
                radioManager.startJammer(appState.jammerTarget);
            }
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_DOWN)) {
            appState.cycleJammerTarget(1);
            if (appState.jamming) {
                radioManager.startJammer(appState.jammerTarget);
            }
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_A)) {
            if (appState.jamming) {
                radioManager.stopJammer();
            } else {
                radioManager.startJammer(appState.jammerTarget);
            }
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_B)) {
            radioManager.stopJammer();
            appState.appMode = APP_MODE_MENU;
            needRedraw = true;
        }
    }
    // -------------------------------------------------------------------------
    // CONDITION 3: SPECTRUM ANALYZER
    // -------------------------------------------------------------------------
    else if (appState.appMode == APP_MODE_ANALYZER_SPECTRUM) {
        if (buttonManager.isLongPressed(BTN_UP)) {
            appState.cycleAnalyzerTraceMode(1);
            needRedraw = true;
        } else if (buttonManager.isLongPressed(BTN_DOWN)) {
            appState.cycleAnalyzerZoom();
            needRedraw = true;
        } else if (buttonManager.isLongPressed(BTN_A)) {
            appState.captureBaseline();
            needRedraw = true;
        } else if (buttonManager.isLongPressed(BTN_B)) {
            appState.toggleWatchChannel(appState.cursorChannel);
            needRedraw = true;
        } else if (buttonManager.isShortReleased(BTN_UP)) {
            if (appState.analyzerFrozen) appState.setCursorChannel(appState.cursorChannel + 1, false);
            else { appState.cycleAnalyzerBand(1); radioManager.requestScanAbort(); }
            needRedraw = true;
        } else if (buttonManager.isShortReleased(BTN_DOWN)) {
            if (appState.analyzerFrozen) appState.setCursorChannel(appState.cursorChannel - 1, false);
            else { appState.cycleAnalyzerRadioMode(1); radioManager.requestScanAbort(); }
            needRedraw = true;
        } else if (buttonManager.isShortReleased(BTN_A)) {
            appState.analyzerFrozen = !appState.analyzerFrozen;
            if (appState.analyzerFrozen) radioManager.requestScanAbort();
            if (!appState.analyzerFrozen) appState.cursorFollowsPeak = true;
            needRedraw = true;
        } else if (buttonManager.isShortReleased(BTN_B)) {
            radioManager.stopAll();
            appState.analyzerFrozen = false;
            appState.appMode = APP_MODE_MENU;
            needRedraw = true;
        }
    }
    else if (appState.appMode == APP_MODE_WATERFALL) {
        if (buttonManager.isPressed(BTN_UP)) {
            appState.cycleAnalyzerBand(1);
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_DOWN)) {
            appState.cycleAnalyzerRadioMode(1);
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_A)) {
            memset(appState.waterfall, 0, sizeof(appState.waterfall));
            appState.waterfallHead = 0;
            appState.waterfallCount = 0;
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_B)) {
            radioManager.stopAll();
            appState.appMode = APP_MODE_MENU;
            needRedraw = true;
        }
    }
    else if (appState.appMode == APP_MODE_SURVEY) {
        if (buttonManager.isPressed(BTN_UP) || buttonManager.isPressed(BTN_DOWN)) {
            appState.cycleAnalyzerBand(1);
            appState.resetSurvey();
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_A)) {
            appState.resetSurvey();
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_B)) {
            radioManager.stopAll();
            appState.appMode = APP_MODE_MENU;
            needRedraw = true;
        }
    }
    else if (appState.appMode == APP_MODE_EVENTS) {
        if (buttonManager.isLongPressed(BTN_UP)) {
            const uint8_t duration = appState.eventMinSweeps >= 5 ? 1 : appState.eventMinSweeps + 1;
            appState.configureEventEngine(appState.eventThreshold, appState.eventHysteresis,
                                          duration, appState.eventMinChannels);
            needRedraw = true;
        } else if (buttonManager.isLongPressed(BTN_DOWN)) {
            const uint8_t channels = appState.eventMinChannels >= 4 ? 1 : appState.eventMinChannels + 1;
            appState.configureEventEngine(appState.eventThreshold, appState.eventHysteresis,
                                          appState.eventMinSweeps, channels);
            needRedraw = true;
        } else if (buttonManager.isShortReleased(BTN_UP)) {
            const uint8_t threshold = appState.eventThreshold >= 90 ? 30 : appState.eventThreshold + 5;
            appState.configureEventEngine(threshold, appState.eventHysteresis,
                                          appState.eventMinSweeps, appState.eventMinChannels);
            needRedraw = true;
        } else if (buttonManager.isShortReleased(BTN_DOWN)) {
            const uint8_t hysteresis = appState.eventHysteresis >= 30 ? 0 : appState.eventHysteresis + 5;
            appState.configureEventEngine(appState.eventThreshold, hysteresis,
                                          appState.eventMinSweeps, appState.eventMinChannels);
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_A)) {
            appState.clearEvents();
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_B)) {
            radioManager.stopAll();
            appState.appMode = APP_MODE_MENU;
            needRedraw = true;
        }
    }
    else if (appState.appMode == APP_MODE_LOGGING) {
        if (buttonManager.isPressed(BTN_A)) {
            if (appState.loggingEnabled) {
                appState.loggingEnabled = false;
                sessionRecorder.stop();
            } else {
                appState.loggingEnabled = sessionRecorder.start();
            }
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_B)) {
            appState.appMode = APP_MODE_MENU;
            needRedraw = true;
        }
    }
    else if (appState.appMode == APP_MODE_RADIO_DIAG) {
        if (buttonManager.isPressed(BTN_A)) {
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_B)) {
            appState.appMode = APP_MODE_MENU;
            needRedraw = true;
        }
    }
    else if (appState.appMode == APP_MODE_PROFILES) {
        if (buttonManager.isPressed(BTN_UP)) {
            appState.cycleScanProfile(-1);
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_DOWN)) {
            appState.cycleScanProfile(1);
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_A) &&
                   appState.scanProfile == SCAN_PROFILE_CUSTOM) {
            appState.cycleCustomSampleCount();
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_B)) {
            appState.appMode = APP_MODE_MENU;
            needRedraw = true;
        }
    }
    // -------------------------------------------------------------------------
    // CONDITION 4: CHANNEL INSPECTOR
    // -------------------------------------------------------------------------
    else if (appState.appMode == APP_MODE_ANALYZER_CHANNEL) {
        if (buttonManager.isPressed(BTN_UP)) {
            appState.inspectedChannel = constrain(appState.inspectedChannel + 1, MIN_CHANNEL, MAX_CHANNEL);
            appState.inspectedPeak = 0;
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_DOWN)) {
            appState.inspectedChannel = constrain(appState.inspectedChannel - 1, MIN_CHANNEL, MAX_CHANNEL);
            appState.inspectedPeak = 0;
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_A)) {
            appState.inspectedChannel = (appState.inspectedChannel + 10) % (MAX_CHANNEL + 1);
            appState.inspectedPeak = 0;
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_B)) {
            radioManager.stopAll();
            appState.appMode = APP_MODE_MENU;
            needRedraw = true;
        }
    }
    // -------------------------------------------------------------------------
    // CONDITION 5: GLOBAL APP SETTINGS
    // -------------------------------------------------------------------------
    else if (appState.appMode == APP_MODE_SETTINGS) {
        if (buttonManager.isPressed(BTN_UP)) {
            settingsSelection = (settingsSelection + 4) % 5;
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_DOWN)) {
            settingsSelection = (settingsSelection + 1) % 5;
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_A)) {
            if (settingsSelection == 0) {
                appState.cyclePowerLevel(1);
                radioManager.updatePALevel(appState.powerLevel);
                cc1101Manager.updatePowerLevel();
            } else if (settingsSelection == 1) {
                appState.cycleDwellTime(1);
            } else if (settingsSelection == 2) {
                appState.cycleDisplayTheme(1);
                // Force one clean page rebuild so no pixels from the previous
                // palette remain. Normal updates stay partial afterwards.
                renderedMode = -1;
            } else if (settingsSelection == 3) {
                appState.cycleMenuLayout(1);
                menuScrollOffset = prevMenuScrollOffset = 0;
            } else if (storageManager.usingSd()) {
                appState.saveSniffPacketsToSd = !appState.saveSniffPacketsToSd;
                appState.markSettingsDirty();
            }
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_B)) {
            appState.appMode = APP_MODE_BAND_SELECT;
            needRedraw = true;
        }
    }
    // -------------------------------------------------------------------------
    // CONDITION 6: STATUS
    // -------------------------------------------------------------------------
    else if (appState.appMode == APP_MODE_STATUS) {
        if (buttonManager.isPressed(BTN_UP)) {
            statusPage = (statusPage + 4) % 5;
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_DOWN)) {
            statusPage = (statusPage + 1) % 5;
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_A)) {
            lastStatusRenderMs = 0;
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_B)) {
            appState.appMode = APP_MODE_BAND_SELECT;
            needRedraw = true;
        }
    }
    // -------------------------------------------------------------------------
    // CONDITION 7: POWER OPTIONS
    // -------------------------------------------------------------------------
    else if (appState.appMode >= APP_MODE_ENV_OCCUPANCY && appState.appMode <= APP_MODE_ENV_PROBE) {
        if (buttonManager.isPressed(BTN_B)) {
            rfEnvironmentAnalyzer.stop(); rfAuthorizedProbe.stop(); appState.appMode = APP_MODE_MENU; needRedraw = true;
        } else if (buttonManager.isPressed(BTN_A)) {
            if (appState.appMode == APP_MODE_ENV_BEFORE_AFTER) {
                if (!rfEnvironmentState.before.valid) {rfEnvironmentState.captureSnapshot(rfEnvironmentState.before);sessionRecorder.recordEnvironmentSummary(rfEnvironmentState,"before");}
                else {rfEnvironmentState.captureSnapshot(rfEnvironmentState.after);sessionRecorder.recordEnvironmentSummary(rfEnvironmentState,"after");}
            } else if (appState.appMode == APP_MODE_ENV_PROBE) {
                if (probeSelection == 4) {
                    if (rfAuthorizedProbe.isRunning()) rfAuthorizedProbe.stop(); else rfAuthorizedProbe.start();
#if RF_LAB_TX_ENABLED
                } else if (!rfAuthorizedProbe.isRunning()) {
                    auto &c = rfEnvironmentState.config;
                    if (probeSelection == 0) c.probeChannel = (c.probeChannel + 1) % 126;
                    else if (probeSelection == 1) c.probeIntervalMs = c.probeIntervalMs >= 1000 ? 20 : (c.probeIntervalMs < 100 ? c.probeIntervalMs + 20 : c.probeIntervalMs + 100);
                    else if (probeSelection == 2) c.probePacketCount = c.probePacketCount >= 1000 ? 10 : min<uint16_t>(1000, c.probePacketCount + 10);
                    else if (probeSelection == 3) c.probeMaxDurationSeconds = c.probeMaxDurationSeconds >= 60 ? 1 : min<uint16_t>(60, c.probeMaxDurationSeconds + 5);
                    appState.markSettingsDirty();
#endif
                }
            } else if (appState.appMode != APP_MODE_ENV_BAND_INFO) {
                if (rfEnvironmentState.running) rfEnvironmentAnalyzer.stop();
                else rfEnvironmentAnalyzer.start(appState.appMode == APP_MODE_ENV_COMPARE ? RF_ENV_COMPARE : RF_ENV_OCCUPANCY);
            }
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_UP)) {
            if (appState.appMode == APP_MODE_ENV_PROBE) probeSelection = (probeSelection + 4) % 5;
            else if (appState.appMode == APP_MODE_ENV_BAND_INFO) envBandChannel = min<uint8_t>(125, envBandChannel + 1);
            else if (appState.appMode == APP_MODE_ENV_BEFORE_AFTER) envBandChannel = min<uint8_t>(125, envBandChannel + 1);
            else if (appState.appMode == APP_MODE_ENV_BURSTS && envEventScroll + 1 < rfEnvironmentState.eventCount) envEventScroll++;
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_DOWN)) {
            if (appState.appMode == APP_MODE_ENV_PROBE) probeSelection = (probeSelection + 1) % 5;
            else if (appState.appMode == APP_MODE_ENV_BAND_INFO) envBandChannel = envBandChannel ? envBandChannel - 1 : 0;
            else if (appState.appMode == APP_MODE_ENV_BEFORE_AFTER) envBandChannel = envBandChannel ? envBandChannel - 1 : 0;
            else if (appState.appMode == APP_MODE_ENV_BURSTS && envEventScroll) envEventScroll--;
            needRedraw = true;
        }
    }
    else if (appState.appMode == APP_MODE_LUA_SCRIPTS) {
        if (luaShowingOutput && buttonManager.isPressed(BTN_UP)) {
            luaOutputScroll = min<uint8_t>(24, luaOutputScroll + 1);
            needRedraw = true;
        } else if (luaShowingOutput && buttonManager.isPressed(BTN_DOWN)) {
            if (luaOutputScroll) --luaOutputScroll;
            needRedraw = true;
        } else if (!luaShowingOutput && !luaShowingGui &&
            buttonManager.isPressed(BTN_UP) && luaScriptCount) {
            luaScriptSelection = (luaScriptSelection + luaScriptCount - 1) % luaScriptCount;
            luaRunStatus = ""; needRedraw = true;
        } else if (!luaShowingOutput && !luaShowingGui &&
                   buttonManager.isPressed(BTN_DOWN) && luaScriptCount) {
            luaScriptSelection = (luaScriptSelection + 1) % luaScriptCount;
            luaRunStatus = ""; needRedraw = true;
        } else if (buttonManager.isPressed(BTN_A)) {
            if (!luaScriptCount) {
                luaScriptCount = luaEngine.listScripts(luaScripts, LUA_UI_MAX_SCRIPTS);
                luaScriptSelection = 0;
                luaRunStatus = luaScriptCount ? "SCRIPTS REFRESHED" : luaEngine.lastError();
            } else {
                luaShowingGui = false;
                luaReturnToListRequested = false;
                luaOutputScroll = 0;
                LuaDisplayStream output(luaOutput);
                const bool ok = luaEngine.run(luaScripts[luaScriptSelection], output);
                luaRunStatus = ok ? "SCRIPT COMPLETED" : String("ERROR: ") + luaEngine.lastError();
                if (!ok) {
                    luaShowingGui = false;
                    if (luaOutput.length() && !luaOutput.endsWith("\n")) luaOutput += "\n";
                    luaOutput += luaRunStatus;
                }
                if (luaReturnToListRequested && ok) {
                    luaOutput = "";
                    luaRunStatus = "";
                    luaShowingOutput = false;
                    luaReturnToListRequested = false;
                } else {
                    if (!luaOutput.length()) luaOutput = luaRunStatus;
                    luaShowingOutput = !ok || !luaShowingGui;
                }
            }
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_B)) {
            if (luaShowingOutput || luaShowingGui) {
                luaShowingOutput = false; luaShowingGui = false;
                luaOutputScroll = 0; luaOutput = ""; luaRunStatus = "";
            } else {
                luaRunStatus = ""; appState.appMode = APP_MODE_BAND_SELECT;
            }
            needRedraw = true;
        }
    }
    else if (appState.appMode == APP_MODE_FILE_EXPLORER) {
        if (buttonManager.isPressed(BTN_UP) && fileEntryCount) {
            fileSelection = (fileSelection + fileEntryCount - 1) % fileEntryCount;
            fileStatus = ""; needRedraw = true;
        } else if (buttonManager.isPressed(BTN_DOWN) && fileEntryCount) {
            fileSelection = (fileSelection + 1) % fileEntryCount;
            fileStatus = ""; needRedraw = true;
        } else if (buttonManager.isPressed(BTN_A) && fileEntryCount) {
            if (fileDirectories[fileSelection]) {
                if (filePath != "/") filePath += "/";
                filePath += fileNames[fileSelection];
                fileEntryCount = 0; fileSelection = 0; fileStatus = "";
            } else {
                String lowerName = fileNames[fileSelection]; lowerName.toLowerCase();
                if (lowerName.endsWith(".rfv")) {
                    String path = filePath;
                    if (path != "/") path += "/";
                    path += fileNames[fileSelection];
                    if (openVideo(path)) appState.appMode = APP_MODE_VIDEO_PLAYER;
                    else fileStatus = "INVALID RFV VIDEO";
                } else if (lowerName.endsWith(".rfi")) {
                    if (openPhoto(fileSelection)) appState.appMode = APP_MODE_PHOTO_VIEWER;
                    else fileStatus = "INVALID RFI PHOTO";
                } else {
                    fileStatus = String(fileSizes[fileSelection]) + " bytes";
                }
            }
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_B)) {
            if (filePath == "/") {
                appState.appMode = APP_MODE_BAND_SELECT;
            } else {
                const int slash = filePath.lastIndexOf('/');
                filePath = slash <= 0 ? "/" : filePath.substring(0, slash);
                fileEntryCount = 0; fileSelection = 0; fileStatus = "";
            }
            needRedraw = true;
        }
    }
    else if (appState.appMode == APP_MODE_PACKET_SNIFFER) {
        if (buttonManager.isPressed(BTN_UP)) {
            const uint8_t next = packetSniffer.channel() >= 125 ? 0 : packetSniffer.channel() + 1;
            radioManager.setPacketSnifferChannel(next);
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_DOWN)) {
            const uint8_t next = packetSniffer.channel() == 0 ? 125 : packetSniffer.channel() - 1;
            radioManager.setPacketSnifferChannel(next);
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_A)) {
            const SnifferDataRate next = packetSniffer.dataRate() == SnifferDataRate::RATE_1_MBPS
                ? SnifferDataRate::RATE_2_MBPS : SnifferDataRate::RATE_1_MBPS;
            radioManager.setPacketSnifferDataRate(next);
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_B)) {
            radioManager.stopPacketSniffer();
            appState.appMode = APP_MODE_MENU;
            needRedraw = true;
        }
    }
    else if (appState.appMode == APP_MODE_VIDEO_PLAYER) {
        if (buttonManager.isPressed(BTN_A)) {
            skipVideo(10);
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_B)) {
            closeVideo();
            appState.appMode = APP_MODE_FILE_EXPLORER;
            needRedraw = true;
        }
    }
    else if (appState.appMode == APP_MODE_PHOTO_VIEWER) {
        if (buttonManager.isPressed(BTN_UP)) {
            changePhoto(1); needRedraw = true;
        } else if (buttonManager.isPressed(BTN_DOWN)) {
            changePhoto(-1); needRedraw = true;
        } else if (buttonManager.isPressed(BTN_B)) {
            closePhoto();
            appState.appMode = APP_MODE_FILE_EXPLORER;
            needRedraw = true;
        }
    }
    else if (appState.appMode == APP_MODE_POWER) {
        if (buttonManager.isPressed(BTN_UP) || buttonManager.isPressed(BTN_DOWN)) {
            powerSelection = (powerSelection + 1) % 2;
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_A)) {
            radioManager.stopAll();
            appState.appMode = powerSelection == 0 ? APP_MODE_REBOOT : APP_MODE_SHUTDOWN;
            needRedraw = true;
        } else if (buttonManager.isPressed(BTN_B)) {
            appState.appMode = APP_MODE_BAND_SELECT;
            needRedraw = true;
        }
    }
}
