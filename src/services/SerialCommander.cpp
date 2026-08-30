#include "services/SerialCommander.h"
#include "core/AppState.h"
#include "drivers/RadioManager.h"
#include "ui/DisplayManager.h"
#include "services/SessionRecorder.h"
#include "services/PerformanceMonitor.h"
#include "services/RfEnvironmentAnalyzer.h"
#include "core/RfEnvironmentState.h"
#include "core/RfEnvironmentMath.h"
#include "services/RfAuthorizedProbe.h"
#include "services/LuaEngine.h"
#include "services/StorageManager.h"
#include "services/EventLog.h"

namespace {
bool parseIntStrict(const String& text, int& value) {
    if (!text.length()) return false; char* end=nullptr; long v=strtol(text.c_str(),&end,10);
    if (*end!='\0') return false; value=static_cast<int>(v); return true;
}
void printEnvHelp() {
    Serial.println("env status | start occupancy | stop | window <1|5|10|30|60>");
    Serial.println("env range <0..125> <0..125> | top | bursts | score");
    Serial.println("env compare <2-4 channels> | snapshot <before|after>");
#if RF_LAB_TX_ENABLED
    Serial.println("env probe channel <ch> | interval <20..5000> | packets <1..1000> | duration <1..60> | size <1..32> | rate <250|1|2> | start | stop");
#endif
}
}

SerialCommander serialCommander;

void SerialCommander::init(unsigned long baud) {
    Serial.begin(baud);
    delay(500);
    Serial.println("\n==============================================");
    Serial.println("   ESP32-S3 DUAL-CORE RF24 SUITE & ANALYZER   ");
    Serial.println("==============================================");
    Serial.println("Type 'help' for the list of serial commands.\n");
}

void SerialCommander::process() {
    if (Serial.available() > 0) {
        String cmd = Serial.readStringUntil('\n');
        executeCommand(cmd);
    }
}

void SerialCommander::executeCommand(String cmd) {
    cmd.trim();
    String lowerCmd = cmd;
    lowerCmd.toLowerCase();

    if (lowerCmd == "env" || lowerCmd == "env help") { printEnvHelp(); }
    else if (lowerCmd.startsWith("env ")) {
        String args=lowerCmd.substring(4); args.trim();
        if(args=="status"){Serial.printf("ENV %s range=%u-%u window=%us cycles=%lu samples/s=%lu avg=%u%% score=%u/%s\n",rfEnvironmentState.running?"RUNNING":"STOPPED",rfEnvironmentState.config.minChannel,rfEnvironmentState.config.maxChannel,rfEnvironmentState.config.sampleWindowSeconds,(unsigned long)rfEnvironmentState.completedCycles,(unsigned long)rfEnvironmentState.samplesPerSecond,rfEnvironmentState.averageOccupancy(),rfEnvironmentState.overallScore(),rfEnvironmentState.scoreLabel(rfEnvironmentState.overallScore()));}
        else if(args=="start occupancy"){if(rfEnvironmentAnalyzer.start()){appState.appMode=APP_MODE_ENV_OCCUPANCY;Serial.println("ENV occupancy started.");}else Serial.println("ENV start failed (busy or no radio). ");}
        else if(args=="stop"){rfEnvironmentAnalyzer.stop();Serial.println("ENV stop requested.");}
        else if(args.startsWith("window ")){int v;if(parseIntStrict(args.substring(7),v)&&rfEnvironmentAnalyzer.setWindow(v)){appState.markSettingsDirty();Serial.printf("ENV window=%ds\n",v);}else Serial.println("Invalid window. Use 1, 5, 10, 30, or 60.");}
        else if(args.startsWith("range ")){int split=args.indexOf(' ',6),a,b;if(split>0&&parseIntStrict(args.substring(6,split),a)&&parseIntStrict(args.substring(split+1),b)&&rfEnvironmentAnalyzer.setRange(a,b)){appState.markSettingsDirty();Serial.printf("ENV range=%d-%d\n",a,b);}else Serial.println("Invalid range. Use: env range <0..125> <min..125>");}
        else if(args=="top"){uint8_t top[5];rfEnvironmentState.topChannels(top,5);for(int i=0;i<5;i++)Serial.printf("%d. CH%u %u MHz %u%%\n",i+1,top[i],2400+top[i],rfEnvironmentState.channels[top[i]].movingAverage);}
        else if(args=="bursts"){for(int i=0;i<rfEnvironmentState.eventCount;i++){int idx=(rfEnvironmentState.eventHead+RF_ENV_BURST_EVENTS-1-i)%RF_ENV_BURST_EVENTS;const auto&e=rfEnvironmentState.events[idx];Serial.printf("#%lu ms=%lu CH%u %uMHz peak=%u base=%u delta=%u severity=%s\n",(unsigned long)e.id,(unsigned long)e.timestampMs,e.channel,e.frequencyMHz,e.peak,e.baseline,e.delta,e.severity==RF_BURST_HIGH?"HIGH":e.severity==RF_BURST_MEDIUM?"MEDIUM":"LOW");}}
        else if(args=="score"){uint8_t v=rfEnvironmentState.overallScore();Serial.printf("Interference score %u/100 %s (relative activity, not RF power)\n",v,rfEnvironmentState.scoreLabel(v));}
        else if(args.startsWith("compare ")){String list=args.substring(8);uint8_t channels[4];int n=0;while(list.length()&&n<4){int sp=list.indexOf(' ');String tok=sp<0?list:list.substring(0,sp);int v;if(!parseIntStrict(tok,v)||v<0||v>125){n=0;break;}channels[n++]=v;if(sp<0){list="";break;}list=list.substring(sp+1);list.trim();}if(n>=2&&!list.length()){rfEnvironmentState.config.compareCount=n;memcpy(rfEnvironmentState.config.compareChannels,channels,n);appState.markSettingsDirty();Serial.printf("Compare configured with %d channels.\n",n);}else Serial.println("Invalid compare list. Provide 2-4 channels in range 0..125.");}
        else if(args=="snapshot before"){rfEnvironmentState.captureSnapshot(rfEnvironmentState.before);sessionRecorder.recordEnvironmentSummary(rfEnvironmentState,"before");Serial.println("Before snapshot captured.");}
        else if(args=="snapshot after"){rfEnvironmentState.captureSnapshot(rfEnvironmentState.after);sessionRecorder.recordEnvironmentSummary(rfEnvironmentState,"after");Serial.println("After snapshot captured.");}
#if RF_LAB_TX_ENABLED
        else if(args.startsWith("probe ")){String p=args.substring(6);if(p=="start")Serial.println(rfAuthorizedProbe.start()?"Authorized bounded probe started.":"Probe start rejected.");else if(p=="stop"){rfAuthorizedProbe.stop();Serial.println("Probe stop requested.");}else{int sp=p.indexOf(' '),v;String key=sp<0?p:p.substring(0,sp);String val=sp<0?"":p.substring(sp+1);if(!parseIntStrict(val,v)){Serial.println("Probe parameter requires an integer.");}else if(key=="channel"&&v>=0&&v<=125)rfEnvironmentState.config.probeChannel=v;else if(key=="interval"&&v>=20&&v<=5000)rfEnvironmentState.config.probeIntervalMs=v;else if(key=="packets"&&v>=1&&v<=1000)rfEnvironmentState.config.probePacketCount=v;else if(key=="duration"&&v>=1&&v<=60)rfEnvironmentState.config.probeMaxDurationSeconds=v;else if(key=="size"&&v>=1&&v<=32)rfEnvironmentState.config.probePayloadSize=v;else if(key=="rate"&&(v==250||v==1||v==2))rfEnvironmentState.config.probeDataRate=v==250?RF24_250KBPS:v==1?RF24_1MBPS:RF24_2MBPS;else{Serial.println("Invalid probe parameter/bounds.");return;}appState.markSettingsDirty();Serial.println("Probe setting updated.");}}
#endif
        else printEnvHelp();
    }
    else if (lowerCmd == "start") {
        radioManager.startJammer(appState.jammerTarget);
    }
    else if (lowerCmd == "stop") {
        radioManager.stopAll();
    }
    else if (lowerCmd.startsWith("jam ")) {
        String targetStr = lowerCmd.substring(4);
        if (appState.setJammerTargetByName(targetStr)) {
            radioManager.startJammer(appState.jammerTarget);
        } else {
            Serial.println("Invalid target! Use: wifi, bt, ble, bledata, all, zigbee");
        }
    }
    else if (lowerCmd == "scan" || lowerCmd == "spectrum") {
        radioManager.stopJammer();
        printAsciiSpectrum();
    }
    else if (lowerCmd.startsWith("trace ")) {
        const String value = lowerCmd.substring(6);
        if (value == "live") appState.analyzerTraceMode = ANALYZER_TRACE_LIVE;
        else if (value == "avg" || value == "average") appState.analyzerTraceMode = ANALYZER_TRACE_AVERAGE;
        else if (value == "max") appState.analyzerTraceMode = ANALYZER_TRACE_MAX;
        else if (value == "delta") appState.analyzerTraceMode = ANALYZER_TRACE_DELTA;
        else { Serial.println("Trace options: live, avg, max, delta"); return; }
        appState.markSettingsDirty();
        Serial.println("Analyzer trace: " + String(appState.getAnalyzerTraceModeName()));
    }
    else if (lowerCmd == "freeze" || lowerCmd == "hold") {
        appState.analyzerFrozen = true;
        Serial.println("Analyzer frozen.");
    }
    else if (lowerCmd == "resume") {
        appState.analyzerFrozen = false;
        Serial.println("Analyzer resumed.");
    }
    else if (lowerCmd.startsWith("zoom ")) {
        const int zoom = lowerCmd.substring(5).toInt();
        if (zoom == 1 || zoom == 2 || zoom == 4) {
            appState.analyzerZoom = zoom;
            Serial.printf("Analyzer zoom: %dx\n", zoom);
        } else Serial.println("Zoom options: 1, 2, 4");
    }
    else if (lowerCmd.startsWith("cursor ")) {
        appState.setCursorChannel(lowerCmd.substring(7).toInt(), false);
        Serial.printf("Cursor: CH%d / %d MHz\n", appState.cursorChannel,
                      2400 + appState.cursorChannel);
    }
    else if (lowerCmd.startsWith("watch ")) {
        const int channel = constrain(lowerCmd.substring(6).toInt(), MIN_CHANNEL, MAX_CHANNEL);
        appState.toggleWatchChannel(channel);
        Serial.printf("Watch CH%d: %s\n", channel,
                      appState.watchedChannels[channel] ? "ON" : "OFF");
    }
    else if (lowerCmd == "baseline") {
        appState.captureBaseline();
        Serial.println("Baseline captured; DELTA trace selected.");
    }
    else if (lowerCmd == "max clear") {
        appState.clearAnalyzerMax();
        Serial.println("Analyzer maximum history cleared.");
    }
    else if (lowerCmd.startsWith("event ")) {
        String args = lowerCmd.substring(6);
        const int split = args.indexOf(' ');
        if (split < 0) {
            Serial.println("event <threshold|hysteresis|duration|channels> <value>");
        } else {
            const String key = args.substring(0, split);
            const int value = args.substring(split + 1).toInt();
            uint8_t threshold = appState.eventThreshold;
            uint8_t hysteresis = appState.eventHysteresis;
            uint8_t duration = appState.eventMinSweeps;
            uint8_t channels = appState.eventMinChannels;
            if (key == "threshold") threshold = value;
            else if (key == "hysteresis") hysteresis = value;
            else if (key == "duration") duration = value;
            else if (key == "channels") channels = value;
            else { Serial.println("Unknown event setting."); return; }
            appState.configureEventEngine(threshold, hysteresis, duration, channels);
            Serial.printf("Event engine: threshold=%u hysteresis=%u duration=%u channels=%u\n",
                          appState.eventThreshold, appState.eventHysteresis,
                          appState.eventMinSweeps, appState.eventMinChannels);
        }
    }
    else if (lowerCmd.startsWith("session")) {
        String action = lowerCmd.length() > 7 ? lowerCmd.substring(7) : "info";
        action.trim();
        if (action == "start") {
            appState.loggingEnabled = sessionRecorder.start();
            Serial.println(appState.loggingEnabled ? "Session recording started." :
                           "Session start failed: " + String(sessionRecorder.lastError()));
        } else if (action == "stop") {
            appState.loggingEnabled = false;
            sessionRecorder.stop();
            Serial.println("Session recording stopped.");
        } else if (action == "export") {
            Serial.println("--- RF SESSION CSV BEGIN ---");
            if (!sessionRecorder.exportCsv(Serial)) Serial.println("No session available.");
            Serial.println("--- RF SESSION CSV END ---");
        } else if (action == "replay") {
            radioManager.stopAll();
            appState.loggingEnabled = false;
            sessionRecorder.stop();
            if (sessionRecorder.replayLatest(appState)) {
                appState.appMode = APP_MODE_ANALYZER_SPECTRUM;
                Serial.println("Last recorded sweep loaded and frozen.");
            } else Serial.println("No replayable session available.");
        } else if (action == "compare") {
            SessionComparison comparison;
            if (sessionRecorder.compareWithPrevious(comparison)) {
                Serial.printf("Session comparison: previous=%lu sweeps avg=%u%% peak=CH%u; current=%lu sweeps avg=%u%% peak=CH%u; delta=%+d points\n",
                              static_cast<unsigned long>(comparison.previousSweeps),
                              comparison.previousAverage, comparison.previousPeakChannel,
                              static_cast<unsigned long>(comparison.currentSweeps),
                              comparison.currentAverage, comparison.currentPeakChannel,
                              comparison.averageDelta);
            } else Serial.println("Comparison unavailable: " + String(sessionRecorder.lastError()));
        } else {
            Serial.printf("Session: %s, %lu sweeps, %u bytes, storage=%s, path=%s, error=%s\n",
                          sessionRecorder.isRecording() ? "RECORDING" : "STOPPED",
                          static_cast<unsigned long>(sessionRecorder.recordedSweeps()),
                          static_cast<unsigned>(sessionRecorder.fileSize()),
                          sessionRecorder.storageName(), sessionRecorder.path(),
                          sessionRecorder.lastError());
        }
    }
    else if (lowerCmd == "events" || lowerCmd == "events export") {
        Serial.println("--- EVENT LOG BEGIN ---");
        if (!eventLog.exportTo(Serial)) Serial.println("Event log unavailable.");
        Serial.println("--- EVENT LOG END ---");
    }
    else if (lowerCmd == "events clear confirm") {
        Serial.println(eventLog.clear() ? "Event log cleared." : "Event log clear failed.");
    }
    else if (lowerCmd == "lua" || lowerCmd == "lua list") {
        luaEngine.list(Serial);
    }
    else if (lowerCmd.startsWith("lua run ")) {
        const String name = cmd.substring(8);
        Serial.println(luaEngine.run(name, Serial) ? "Lua script completed." :
                       "Lua failed: " + String(luaEngine.lastError()));
    }
    else if (lowerCmd == "perf") {
        const PerformanceSnapshot perf = performanceMonitor.snapshot();
        Serial.printf("PERF sweep last/avg/max=%lu/%lu/%lu us, UI avg/max=%lu/%lu us, loop=%u Hz\n",
                      static_cast<unsigned long>(perf.lastSweepUs),
                      static_cast<unsigned long>(perf.averageSweepUs),
                      static_cast<unsigned long>(perf.maxSweepUs),
                      static_cast<unsigned long>(perf.averageUiUs),
                      static_cast<unsigned long>(perf.maxUiUs), perf.loopsPerSecond);
        Serial.printf("SPI lock contention=%lu timeout=%lu avg/max wait=%lu/%lu us\n",
                      static_cast<unsigned long>(radioManager.getBusContentions()),
                      static_cast<unsigned long>(radioManager.getBusTimeouts()),
                      static_cast<unsigned long>(radioManager.getAverageBusWaitUs()),
                      static_cast<unsigned long>(radioManager.getMaxBusWaitUs()));
    }
    else if (lowerCmd == "factory reset confirm") {
        radioManager.stopAll();
        sessionRecorder.stop();
        appState.factoryResetSettings();
        Serial.println("Factory settings restored. Rebooting...");
        storageManager.prepareForRestart();
        Serial.flush();
        delay(100);
        ESP.restart();
    }
    else if (lowerCmd.startsWith("inspect ")) {
        int ch = lowerCmd.substring(8).toInt();
        if (ch >= MIN_CHANNEL && ch <= MAX_CHANNEL) {
            radioManager.stopJammer();
            appState.inspectedChannel = ch;
            uint8_t lvl = radioManager.inspectChannel(ch);
            Serial.printf("🔍 Channel %d (%d MHz): Activity = %d%% [%s]\n",
                          ch, 2400 + ch, lvl, lvl > 20 ? "RF DETECTED" : "CLEAR");
        } else {
            Serial.println("Invalid channel (0 - 125)");
        }
    }
    else if (lowerCmd.startsWith("power") || lowerCmd.startsWith("pwr")) {
        String pwrArg = lowerCmd.startsWith("power") ? lowerCmd.substring(5) : lowerCmd.substring(3);
        pwrArg.trim();
        if (pwrArg.length() > 0) {
            if (appState.setPowerLevelByName(pwrArg)) {
                radioManager.updatePALevel(appState.powerLevel);
                Serial.printf("⚡ TX Power Level set to: %s (%s)\n",
                              appState.getPowerLevelName(), appState.getPowerLevelDbmStr());
            } else {
                Serial.println("Invalid power level! Options: min (-18dBm), low (-12dBm), high (-6dBm), max (0dBm)");
            }
        } else {
            Serial.printf("⚡ Current TX Power Level: %s (%s)\n",
                          appState.getPowerLevelName(), appState.getPowerLevelDbmStr());
        }
    }
    else if (lowerCmd.startsWith("dwell")) {
        String dwellArg = lowerCmd.length() > 5 ? lowerCmd.substring(5) : "";
        dwellArg.trim();
        if (dwellArg.length() > 0) {
            int us = dwellArg.toInt();
            if (appState.setDwellTime(us)) {
                Serial.printf("⏱️ Dwell Time set to: %d µs (%s)\n",
                              appState.dwellTimeUs, appState.getDwellTimeName());
            } else {
                Serial.println("Invalid dwell time! Presets: 50, 100, 200, 500, 1000 (Range: 10-10000 µs)");
            }
        } else {
            Serial.printf("⏱️ Current Dwell Time: %d µs (%s)\n",
                          appState.dwellTimeUs, appState.getDwellTimeName());
        }
    }
    else if (lowerCmd == "config" || lowerCmd == "settings") {
        printConfig();
    }
    else if (lowerCmd == "status") {
        printStatus();
    }
    else if (lowerCmd == "help") {
        printHelp();
    }
    else if (cmd.length() > 0) {
        Serial.println("Unknown command. Type 'help' for the list of commands.");
    }

    displayManager.requestRedraw();
}

void SerialCommander::printAsciiSpectrum() {
    Serial.println("\n📡 Scanning RF 2.4 GHz Spectrum (Channel 0 - 125)...");
    radioManager.scanSpectrum();

    Serial.println("\n=== RF 2.4 GHz SPECTRUM GRAPH ===");
    for (int row = 10; row >= 1; row--) {
        int threshold = row * 10;
        Serial.printf("%3d%% | ", threshold);
        for (int ch = 0; ch < TOTAL_CHANNELS; ch += 2) {
            uint8_t lvl = appState.spectrumLevels[ch];
            uint8_t pk = appState.peakLevels[ch];
            if (lvl >= threshold) {
                Serial.print("█");
            } else if (pk >= threshold) {
                Serial.print("▪");
            } else {
                Serial.print(" ");
            }
        }
        Serial.println();
    }
    Serial.println("     +---------------------------------------------------------------");
    Serial.println(" Ch: | 0   12(W1)   37(W6)   62(W11)   84(W14)   100       125");
    Serial.println(" MHz:| 2400  2412     2437     2462      2484      2500      2525");
    Serial.printf("\n⚡ Highest RF Peak: Channel %d (%d MHz) with Intensity %d%%\n\n",
                  appState.peakChannel, 2400 + appState.peakChannel, appState.peakLevel);
}

void SerialCommander::printConfig() {
    Serial.println("\n=== RF CONFIGURATION & PARAMETERS ===");
    Serial.println("TX Power Level : " + String(appState.getPowerLevelName()) + " [" + String(appState.getPowerLevelDbmStr()) + "]");
    Serial.println("Dwell Time     : " + String(appState.dwellTimeUs) + " µs (" + String(appState.getDwellTimeName()) + ")");
    Serial.println("Display Theme  : " + String(appState.getDisplayThemeName()));
    Serial.println("Analyzer Trace : " + String(appState.getAnalyzerTraceModeName()));
    Serial.printf("Event Engine   : T%u H%u D%u M%u\n", appState.eventThreshold,
                  appState.eventHysteresis, appState.eventMinSweeps,
                  appState.eventMinChannels);
    Serial.println("Build Profile  : " + String(radioManager.transmitFeaturesEnabled() ?
                                                "AUTHORIZED_RF_LAB" : "ANALYZER_ONLY"));
    Serial.println("Active Target  : " + String(appState.getJammerTargetName()) + " (" + String(appState.getJammerFreqRangeStr()) + ")");
    Serial.println("Radio PA Mode  : LNA Gain MAX Enabled");
    Serial.println("Data Rate      : 2 Mbps (RF24_2MBPS)");
    Serial.println("Packet Size    : " + String(FAST_PAYLOAD_SIZE) + " Byte Payload (FAST_JAM_PAYLOAD)");
    Serial.println("Address Width  : " + String(FAST_ADDRESS_WIDTH) + " Bytes");
    Serial.println("CRC Status     : Disabled (Maximum Raw Aggression)");
    Serial.println("======================================\n");
}

void SerialCommander::printStatus() {
    Serial.println("\n=== DEVICE & RADIO STATUS ===");
    Serial.printf("nRF24L01+ : %u/2 receiver(s) available\n", radioManager.availableRadioCount());
    Serial.println("Jam Mode  : " + String(appState.jamming ? "🔥 ACTIVE (Core 0 Task)" : "🛑 STOPPED"));
    Serial.println("Target    : " + String(appState.getJammerTargetName()));
    Serial.println("Range     : " + String(appState.getJammerFreqRangeStr()));
    Serial.println("TX Power  : " + String(appState.getPowerLevelName()) + " (" + String(appState.getPowerLevelDbmStr()) + ")");
    Serial.println("Dwell Time: " + String(appState.dwellTimeUs) + " µs (" + String(appState.getDwellTimeName()) + ")");
    Serial.println("Peak RF   : Channel " + String(appState.peakChannel) + " (" + String(appState.peakLevel) + "%)");
    Serial.printf("Confidence: %u%% (%s)\n", appState.analyzerConfidence,
                  appState.analyzerFrozen ? "FROZEN" : "LIVE");
    Serial.printf("SD Card   : %s, type=%s, total=%llu, used=%llu, free=%llu bytes\n",
                  storageManager.sdStatus(), storageManager.sdTypeName(),
                  storageManager.sdTotalBytes(), storageManager.sdUsedBytes(),
                  storageManager.sdFreeBytes());
    Serial.printf("Recorder  : %s (%s)\n", sessionRecorder.storageName(), sessionRecorder.path());
    Serial.println("=== END STATUS ===\n");
}

void SerialCommander::printHelp() {
    Serial.println("\n=== SERIAL COMMAND LIST ===");
    Serial.println("jam <target> - Targets: wifi, bt, ble, bledata, all, zigbee");
    Serial.println("power <lvl>  - Set TX Power: min, low, high, max (e.g. 'power max')");
    Serial.println("dwell <us>   - Set Dwell Time in µs: 50, 100, 200, 500, 1000");
    Serial.println("config       - Show current RF configuration & parameters");
    Serial.println("start        - Start jammer on active target");
    Serial.println("stop         - Stop jammer transmission");
    Serial.println("scan         - Run Spectrum Analyzer and print RF graph");
    Serial.println("inspect <ch> - Analyze RF activity on a specific channel");
    Serial.println("trace <mode>  - live, avg, max, or delta");
    Serial.println("freeze/resume - Hold or resume analyzer acquisition");
    Serial.println("zoom <1|2|4>  - Set graph zoom around cursor");
    Serial.println("cursor <ch>   - Move analyzer cursor");
    Serial.println("watch <ch>    - Toggle persistent channel watch marker");
    Serial.println("baseline      - Capture baseline and select delta trace");
    Serial.println("max clear     - Clear maximum history");
    Serial.println("event <key> <value> - threshold/hysteresis/duration/channels");
    Serial.println("session <start|stop|info|export|replay|compare> - recorder and two-session comparison");
    Serial.println("events [export] | events clear confirm - Persistent diagnostic log");
    Serial.println("lua <list|run NAME> - Run sandboxed /RFSuite/scripts/*.lua");
    Serial.println("perf          - Runtime scan/UI/SPI timing diagnostics");
    Serial.println("env help      - RF Environment Test commands");
    Serial.println("factory reset confirm - Restore persistent defaults and reboot");
    Serial.println("status       - Show system status and radio module");
    Serial.println("help         - Show this help");
    Serial.println("==============================\n");
}
