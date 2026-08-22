#include "ui/DisplayManager.h"
#include "drivers/ButtonManager.h"
#include "drivers/RadioManager.h"
#include "ui/MenuCatalog.h"
#include "services/SessionRecorder.h"
#include "services/RfEnvironmentAnalyzer.h"
#include "core/RfEnvironmentState.h"
#include "services/RfAuthorizedProbe.h"
#include "services/LuaEngine.h"
#include "services/StorageManager.h"
#include "services/PacketSniffer.h"

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
    }

    switch (appState.appMode) {
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
        } else if (buttonManager.isPressed(BTN_B)) {
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
            if (feature.mode == APP_MODE_PACKET_SNIFFER) {
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
    // CONDITION 5: RF SETTINGS
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
            appState.appMode = APP_MODE_MENU;
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
            appState.appMode = APP_MODE_MENU;
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
                luaRunStatus = ""; appState.appMode = APP_MODE_MENU;
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
                appState.appMode = APP_MODE_MENU;
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
            appState.appMode = APP_MODE_MENU;
            needRedraw = true;
        }
    }
}
