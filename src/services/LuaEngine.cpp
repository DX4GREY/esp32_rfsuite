#include "services/LuaEngine.h"
#include "services/StorageManager.h"
#include <dirent.h>
#include "core/AppState.h"
#include "drivers/RadioManager.h"
#include "drivers/Cc1101Manager.h"
#include "services/SessionRecorder.h"
#include "services/SubGhzRawService.h"
#include "services/RfEnvironmentAnalyzer.h"
#include "core/RfEnvironmentState.h"
#include "ui/DisplayManager.h"
#include "services/Watchdog.h"
#include "config/Config.h"
#include <FS.h>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

LuaEngine luaEngine;

/* LUA_LIBRARY_METADATA
{
 "annotations":[
  "---@alias RfChannel integer # RF24 channel 0..125.",
  "---@alias RfBand \"all\"|\"wifi\"|\"bt\"",
  "---@alias RfTrace \"live\"|\"avg\"|\"max\"|\"delta\"",
  "---@alias SubGhzPreset \"ook270\"|\"ook650\"|\"fsk2\"|\"fsk12\"|\"fsk47\"",
  "---@alias SubGhzRegion \"rx_only\"|\"etsi\"|\"fcc\"",
  "---@alias RfScreen \"spectrum\"|\"waterfall\"|\"inspect\"|\"survey\"|\"events\"|\"logging\"|\"status\"|\"menu\"|\"subghz\"|\"subghz_analyzer\"|\"subghz_record\"",
  "---@alias RfButton \"up\"|\"down\"|\"a\"|\"b\"|\"right\"|\"left\"",
  "---@alias RfColor \"white\"|\"black\"|\"gray\"|\"accent\"|\"cyan\"|\"green\"|\"yellow\"|\"orange\"|\"red\"",
  "",
  "---@class RfStatus",
  "---@field peak_channel RfChannel",
  "---@field peak_level integer",
  "---@field confidence integer",
  "---@field cursor RfChannel",
  "---@field sweeps integer",
  "---@field radios integer",
  "---@field frozen boolean",
  "---@field logging boolean",
  "---@field environment_running boolean",
  "",
  "---@class SubGhzStatus",
  "---@field connected boolean",
  "---@field simulation boolean",
  "---@field tx_enabled boolean",
  "---@field frequency number",
  "---@field preset string",
  "---@field region string",
  "---@field recording boolean",
  "---@field armed boolean",
  "---@field pulses integer",
  "---@field rssi integer",
  "---@field analyzer_running boolean",
  "---@field analyzer_peak_frequency number",
  "---@field analyzer_peak_rssi integer",
  "---@field last_error string"
 ],
 "functions":[
  {"name":"millis","description":"Return the device uptime.","returns":[{"type":"integer","description":"Milliseconds since boot."}]},
  {"name":"peak_channel","description":"Return the strongest channel in the latest scan.","returns":[{"type":"RfChannel","description":"Current peak channel."}]},
  {"name":"level","description":"Read the latest activity for one channel.","params":[{"name":"channel","type":"RfChannel"}],"returns":[{"type":"integer","description":"Relative activity from 0 to 100."}]},
  {"name":"log","description":"Append a timestamped message to the Lua log on the SD card.","params":[{"name":"message","type":"string"}]},
  {"name":"spectrum","description":"Return a snapshot of all channel activity values.","returns":[{"type":"integer[]","description":"126 values; index 1 represents RF channel 0."}]},
  {"name":"status","description":"Return current analyzer and recorder status.","returns":[{"type":"RfStatus"}]},
  {"name":"set_cursor","description":"Move the analyzer cursor.","params":[{"name":"channel","type":"RfChannel"}]},
  {"name":"freeze","description":"Freeze or resume analyzer acquisition.","params":[{"name":"frozen","type":"boolean"}]},
  {"name":"set_band","description":"Select the analyzer scan band.","params":[{"name":"band","type":"RfBand"}]},
  {"name":"set_trace","description":"Select the analyzer trace mode.","params":[{"name":"trace","type":"RfTrace"}]},
  {"name":"capture_baseline","description":"Capture the current levels as the DELTA baseline."},
  {"name":"clear_max","description":"Clear the maximum trace history."},
  {"name":"toggle_watch","description":"Toggle a watched-channel marker.","params":[{"name":"channel","type":"RfChannel"}]},
  {"name":"recording","description":"Start a new recording session or stop recording.","params":[{"name":"start","type":"boolean"}],"returns":[{"type":"boolean","description":"True when the requested operation succeeded."}]},
  {"name":"environment","description":"Start or stop passive occupancy analysis.","params":[{"name":"start","type":"boolean"}],"returns":[{"type":"boolean","description":"True when the requested operation succeeded."}]},
  {"name":"subghz_status","description":"Return current CC1101, analyzer, raw recorder, and TX-policy status.","returns":[{"type":"SubGhzStatus"}]},
  {"name":"subghz_set_frequency","description":"Set a supported CC1101 frequency in MHz.","params":[{"name":"frequency_mhz","type":"number"}],"returns":[{"type":"boolean","description":"True when the frequency was accepted."}]},
  {"name":"subghz_set_preset","description":"Select and persist a CC1101 modulation preset.","params":[{"name":"preset","type":"SubGhzPreset"}]},
  {"name":"subghz_set_region","description":"Select and persist the Sub-GHz TX allowlist; this does not itself transmit.","params":[{"name":"region","type":"SubGhzRegion"}]},
  {"name":"subghz_analyzer","description":"Start or stop the CC1101 frequency analyzer.","params":[{"name":"start","type":"boolean"}]},
  {"name":"subghz_record","description":"Start or stop raw GDO0 recording, optionally tuning first.","params":[{"name":"start","type":"boolean"},{"name":"frequency_mhz","type":"number","default":"current frequency"}],"returns":[{"type":"boolean","description":"True when the requested operation succeeded."}]},
  {"name":"subghz_files","description":"List available .rfr and Flipper RAW .sub files.","returns":[{"type":"string[]","description":"Up to 32 safe library filenames."}]},
  {"name":"subghz_replay","description":"Replay one library file with the native progress/result UI; unavailable in analyzer builds.","params":[{"name":"filename","type":"string"}],"returns":[{"type":"boolean","description":"True when replay completed successfully."}]},
  {"name":"open_screen","description":"Choose the TFT screen shown after the script exits.","params":[{"name":"screen","type":"RfScreen"}]},
  {"name":"gui_begin","description":"Open and clear the protected 152 x 86 Lua canvas.","params":[{"name":"title","type":"string","default":"\"LUA GUI\""}]},
  {"name":"gui_footer","description":"Set the three firmware footer labels.","params":[{"name":"left","type":"string","default":"\"\""},{"name":"middle","type":"string","default":"\"\""},{"name":"right","type":"string","default":"\"\""}]},
  {"name":"gui_clear","description":"Clear the Lua canvas without overwriting its firmware frame."},
  {"name":"gui_close","description":"Close the Lua GUI and return to the script list."},
  {"name":"gui_text","description":"Draw clipped single-line text.","params":[{"name":"x","type":"integer"},{"name":"y","type":"integer"},{"name":"text","type":"string"},{"name":"color","type":"RfColor","default":"\"white\""}]},
  {"name":"gui_pixel","description":"Draw one clipped pixel.","params":[{"name":"x","type":"integer"},{"name":"y","type":"integer"},{"name":"color","type":"RfColor","default":"\"white\""}]},
  {"name":"gui_line","description":"Draw a clipped line.","params":[{"name":"x0","type":"integer"},{"name":"y0","type":"integer"},{"name":"x1","type":"integer"},{"name":"y1","type":"integer"},{"name":"color","type":"RfColor","default":"\"white\""}]},
  {"name":"gui_rect","description":"Draw an outline or filled rectangle.","params":[{"name":"x","type":"integer"},{"name":"y","type":"integer"},{"name":"width","type":"integer"},{"name":"height","type":"integer"},{"name":"color","type":"RfColor","default":"\"white\""},{"name":"filled","type":"boolean","default":"false"}]},
  {"name":"gui_circle","description":"Draw an outline or filled circle.","params":[{"name":"x","type":"integer"},{"name":"y","type":"integer"},{"name":"radius","type":"integer"},{"name":"color","type":"RfColor","default":"\"white\""},{"name":"filled","type":"boolean","default":"false"}]},
  {"name":"button","description":"Read a hardware button during a script.","params":[{"name":"button","type":"RfButton"}],"returns":[{"type":"boolean","description":"True while the active-low button is pressed."}]},
  {"name":"delay","description":"Wait 0 to 1000 ms while feeding the watchdog.","params":[{"name":"milliseconds","type":"integer"}]},
  {"name":"lab_start","description":"Start an authorized-lab RF target; unavailable in analyzer builds.","params":[{"name":"target","type":"string"}],"returns":[{"type":"boolean","description":"True when transmission started."}]},
  {"name":"lab_stop","description":"Stop all controlled-lab RF activity."}
 ]
}
LUA_LIBRARY_METADATA_END */

namespace {
constexpr size_t MAX_SCRIPT_BYTES = 32U * 1024U;
constexpr int MAX_VM_INSTRUCTIONS = 200000;
Stream* activeOutput = nullptr;
int instructionBudget = 0;
bool safeName(const String& name);

int luaPrint(lua_State* state) {
    if (!activeOutput) return 0;
    const int count = lua_gettop(state);
    for (int i = 1; i <= count; ++i) {
        if (i > 1) activeOutput->print('\t');
        const char* value = lua_tostring(state, i);
        activeOutput->print(value ? value : lua_typename(state, lua_type(state, i)));
    }
    activeOutput->println();
    return 0;
}

int rfMillis(lua_State* state) { lua_pushnumber(state, millis()); return 1; }
int rfPeak(lua_State* state) { lua_pushinteger(state, appState.peakChannel); return 1; }
int rfLevel(lua_State* state) {
    const int channel = luaL_checkinteger(state, 1);
    luaL_argcheck(state, channel >= MIN_CHANNEL && channel <= MAX_CHANNEL, 1, "channel must be 0..125");
    lua_pushinteger(state, appState.spectrumLevels[channel]);
    return 1;
}

int rfSpectrum(lua_State* state) {
    lua_createtable(state, TOTAL_CHANNELS, 0);
    for (int channel = 0; channel < TOTAL_CHANNELS; ++channel) {
        lua_pushinteger(state, appState.spectrumLevels[channel]);
        lua_rawseti(state, -2, channel + 1);
    }
    return 1;
}

int rfStatus(lua_State* state) {
    lua_newtable(state);
#define RF_FIELD_INT(name, value) lua_pushinteger(state, value); lua_setfield(state, -2, name)
#define RF_FIELD_BOOL(name, value) lua_pushboolean(state, value); lua_setfield(state, -2, name)
    RF_FIELD_INT("peak_channel", appState.peakChannel);
    RF_FIELD_INT("peak_level", appState.peakLevel);
    RF_FIELD_INT("confidence", appState.analyzerConfidence);
    RF_FIELD_INT("cursor", appState.cursorChannel);
    RF_FIELD_INT("sweeps", appState.surveySweeps);
    RF_FIELD_INT("radios", radioManager.availableRadioCount());
    RF_FIELD_BOOL("frozen", appState.analyzerFrozen);
    RF_FIELD_BOOL("logging", sessionRecorder.isRecording());
    RF_FIELD_BOOL("environment_running", rfEnvironmentState.running);
#undef RF_FIELD_INT
#undef RF_FIELD_BOOL
    return 1;
}

int rfSetCursor(lua_State* state) {
    const int channel = luaL_checkinteger(state, 1);
    luaL_argcheck(state, channel >= MIN_CHANNEL && channel <= MAX_CHANNEL,
                  1, "channel must be 0..125");
    appState.setCursorChannel(channel, false);
    return 0;
}

int rfFreeze(lua_State* state) {
    appState.analyzerFrozen = lua_toboolean(state, 1);
    if (appState.analyzerFrozen) radioManager.requestScanAbort();
    return 0;
}

int rfSetBand(lua_State* state) {
    const char* band = luaL_checkstring(state, 1);
    if (!strcmp(band, "all")) appState.analyzerBand = SCAN_BAND_ALL;
    else if (!strcmp(band, "wifi")) appState.analyzerBand = SCAN_BAND_WIFI;
    else if (!strcmp(band, "bt")) appState.analyzerBand = SCAN_BAND_BT;
    else return luaL_error(state, "band must be all, wifi, or bt");
    radioManager.requestScanAbort(); appState.markSettingsDirty(); return 0;
}

int rfSetTrace(lua_State* state) {
    const char* trace = luaL_checkstring(state, 1);
    if (!strcmp(trace, "live")) appState.analyzerTraceMode = ANALYZER_TRACE_LIVE;
    else if (!strcmp(trace, "avg")) appState.analyzerTraceMode = ANALYZER_TRACE_AVERAGE;
    else if (!strcmp(trace, "max")) appState.analyzerTraceMode = ANALYZER_TRACE_MAX;
    else if (!strcmp(trace, "delta")) appState.analyzerTraceMode = ANALYZER_TRACE_DELTA;
    else return luaL_error(state, "trace must be live, avg, max, or delta");
    appState.markSettingsDirty(); return 0;
}

int rfBaseline(lua_State*) { appState.captureBaseline(); return 0; }
int rfClearMax(lua_State*) { appState.clearAnalyzerMax(); return 0; }
int rfWatch(lua_State* state) {
    const int channel = luaL_checkinteger(state, 1);
    luaL_argcheck(state, channel >= MIN_CHANNEL && channel <= MAX_CHANNEL, 1, "channel must be 0..125");
    appState.toggleWatchChannel(channel); return 0;
}

int rfSession(lua_State* state) {
    const bool start = lua_toboolean(state, 1);
    if (start) appState.loggingEnabled = sessionRecorder.start();
    else { appState.loggingEnabled = false; sessionRecorder.stop(); }
    lua_pushboolean(state, start ? appState.loggingEnabled : true); return 1;
}

int rfEnvironment(lua_State* state) {
    const bool start = lua_toboolean(state, 1);
    bool result = true;
    if (start) result = rfEnvironmentAnalyzer.start(RF_ENV_OCCUPANCY);
    else rfEnvironmentAnalyzer.stop();
    lua_pushboolean(state, result); return 1;
}

int rfSubGhzStatus(lua_State* state) {
    lua_newtable(state);
#define SUB_FIELD_INT(name, value) lua_pushinteger(state, value); lua_setfield(state, -2, name)
#define SUB_FIELD_NUM(name, value) lua_pushnumber(state, value); lua_setfield(state, -2, name)
#define SUB_FIELD_BOOL(name, value) lua_pushboolean(state, value); lua_setfield(state, -2, name)
#define SUB_FIELD_STR(name, value) lua_pushstring(state, value); lua_setfield(state, -2, name)
    SUB_FIELD_BOOL("connected", cc1101Manager.isConnected());
    SUB_FIELD_BOOL("simulation", subGhzRawService.simulationMode());
#if RF_LAB_TX_ENABLED
    SUB_FIELD_BOOL("tx_enabled", true);
#else
    SUB_FIELD_BOOL("tx_enabled", false);
#endif
    SUB_FIELD_NUM("frequency", cc1101Manager.frequencyMHz());
    SUB_FIELD_STR("preset", cc1101Manager.presetName());
    SUB_FIELD_STR("region", subGhzRawService.regionName());
    SUB_FIELD_BOOL("recording", subGhzRawService.isRecording());
    SUB_FIELD_BOOL("armed", subGhzRawService.isArmed());
    SUB_FIELD_INT("pulses", subGhzRawService.pulseCount());
    SUB_FIELD_INT("rssi", subGhzRawService.liveRssiDbm());
    SUB_FIELD_BOOL("analyzer_running", subGhzRawService.analyzerRunning());
    SUB_FIELD_NUM("analyzer_peak_frequency", subGhzRawService.analyzerPeakFrequency());
    SUB_FIELD_INT("analyzer_peak_rssi", subGhzRawService.analyzerPeakRssi());
    SUB_FIELD_STR("last_error", subGhzRawService.lastError());
#undef SUB_FIELD_INT
#undef SUB_FIELD_NUM
#undef SUB_FIELD_BOOL
#undef SUB_FIELD_STR
    return 1;
}

bool supportedSubGhzFrequency(float mhz) {
    return (mhz >= 300.0f && mhz <= 348.0f) ||
           (mhz >= 387.0f && mhz <= 464.0f) ||
           (mhz >= 779.0f && mhz <= 928.0f);
}

int rfSubGhzSetFrequency(lua_State* state) {
    const float mhz = luaL_checknumber(state, 1);
    luaL_argcheck(state, supportedSubGhzFrequency(mhz), 1,
                  "frequency must be in 300..348, 387..464, or 779..928 MHz");
    lua_pushboolean(state, cc1101Manager.setFrequency(mhz));
    return 1;
}

int rfSubGhzSetPreset(lua_State* state) {
    String preset = luaL_checkstring(state, 1); preset.toLowerCase();
    Cc1101Preset value;
    if (preset == "ook270") value = Cc1101Preset::OOK_270;
    else if (preset == "ook650") value = Cc1101Preset::OOK_650;
    else if (preset == "fsk2") value = Cc1101Preset::FSK_2K;
    else if (preset == "fsk12") value = Cc1101Preset::FSK_12K;
    else if (preset == "fsk47") value = Cc1101Preset::FSK_47K;
    else return luaL_error(state, "preset must be ook270, ook650, fsk2, fsk12, or fsk47");
    cc1101Manager.setPreset(value);
    appState.subGhzRadioPreset = static_cast<uint8_t>(value);
    appState.markSettingsDirty();
    return 0;
}

int rfSubGhzSetRegion(lua_State* state) {
    String region = luaL_checkstring(state, 1); region.toLowerCase();
    SubGhzRegion value;
    if (region == "rx_only") value = SubGhzRegion::RX_ONLY;
    else if (region == "etsi") value = SubGhzRegion::ETSI;
    else if (region == "fcc") value = SubGhzRegion::FCC;
    else return luaL_error(state, "region must be rx_only, etsi, or fcc");
    subGhzRawService.setRegion(value);
    appState.subGhzRegion = static_cast<uint8_t>(value);
    appState.markSettingsDirty();
    return 0;
}

int rfSubGhzAnalyzer(lua_State* state) {
    if (lua_toboolean(state, 1)) subGhzRawService.startAnalyzer();
    else subGhzRawService.stopAnalyzer();
    return 0;
}

int rfSubGhzRecord(lua_State* state) {
    const bool start = lua_toboolean(state, 1);
    bool result;
    if (start) {
        const float mhz = lua_gettop(state) >= 2 ? luaL_checknumber(state, 2) :
                                                  cc1101Manager.frequencyMHz();
        luaL_argcheck(state, supportedSubGhzFrequency(mhz), 2,
                      "frequency must be in 300..348, 387..464, or 779..928 MHz");
        result = subGhzRawService.startRecording(mhz);
    } else result = subGhzRawService.stopRecording();
    lua_pushboolean(state, result);
    return 1;
}

int rfSubGhzFiles(lua_State* state) {
    String names[32];
    const size_t count = subGhzRawService.listFiles(names, 32);
    lua_createtable(state, count, 0);
    for (size_t i = 0; i < count; ++i) {
        lua_pushstring(state, names[i].c_str());
        lua_rawseti(state, -2, i + 1);
    }
    return 1;
}

int rfSubGhzReplay(lua_State* state) {
#if RF_LAB_TX_ENABLED
    const String name = luaL_checkstring(state, 1);
    luaL_argcheck(state, safeName(name) && (name.endsWith(".rfr") || name.endsWith(".sub")),
                  1, "filename must be a safe .rfr or .sub library name");
    lua_pushboolean(state, displayManager.replaySubGhzFile(name));
    return 1;
#else
    return luaL_error(state, "Sub-GHz replay is disabled in analyzer build");
#endif
}

int rfOpen(lua_State* state) {
    const char* screen = luaL_checkstring(state, 1);
    if (!strcmp(screen, "spectrum")) appState.appMode = APP_MODE_ANALYZER_SPECTRUM;
    else if (!strcmp(screen, "waterfall")) appState.appMode = APP_MODE_WATERFALL;
    else if (!strcmp(screen, "inspect")) appState.appMode = APP_MODE_ANALYZER_CHANNEL;
    else if (!strcmp(screen, "survey")) appState.appMode = APP_MODE_SURVEY;
    else if (!strcmp(screen, "events")) appState.appMode = APP_MODE_EVENTS;
    else if (!strcmp(screen, "logging")) appState.appMode = APP_MODE_LOGGING;
    else if (!strcmp(screen, "status")) appState.appMode = APP_MODE_STATUS;
    else if (!strcmp(screen, "menu")) appState.appMode = APP_MODE_MENU;
    else if (!strcmp(screen, "subghz")) appState.appMode = APP_MODE_SUBGHZ;
    else if (!strcmp(screen, "subghz_analyzer")) {
        subGhzRawService.startAnalyzer(); appState.appMode = APP_MODE_SUBGHZ_ANALYZER;
    }
    else if (!strcmp(screen, "subghz_record")) appState.appMode = APP_MODE_SUBGHZ_RECORD;
    else return luaL_error(state, "unknown screen");
    return 0;
}

const char* optionalColor(lua_State* state, int index) {
    return lua_gettop(state) >= index ? luaL_checkstring(state, index) : "white";
}
int rfGuiBegin(lua_State* state) { displayManager.luaGuiBegin(luaL_optstring(state, 1, "LUA GUI")); return 0; }
int rfGuiFooter(lua_State* state) {
    displayManager.luaGuiFooter(luaL_optstring(state, 1, ""),
                                luaL_optstring(state, 2, ""),
                                luaL_optstring(state, 3, "")); return 0;
}
int rfGuiClear(lua_State*) { displayManager.luaGuiClear(); return 0; }
int rfGuiClose(lua_State*) { displayManager.luaGuiClose(); return 0; }
int rfGuiText(lua_State* state) {
    displayManager.luaGuiText(luaL_checkinteger(state, 1), luaL_checkinteger(state, 2),
                              luaL_checkstring(state, 3), optionalColor(state, 4)); return 0;
}
int rfGuiPixel(lua_State* state) {
    displayManager.luaGuiPixel(luaL_checkinteger(state, 1), luaL_checkinteger(state, 2),
                               optionalColor(state, 3)); return 0;
}
int rfGuiLine(lua_State* state) {
    displayManager.luaGuiLine(luaL_checkinteger(state, 1), luaL_checkinteger(state, 2),
                              luaL_checkinteger(state, 3), luaL_checkinteger(state, 4),
                              optionalColor(state, 5)); return 0;
}
int rfGuiRect(lua_State* state) {
    displayManager.luaGuiRect(luaL_checkinteger(state, 1), luaL_checkinteger(state, 2),
                              luaL_checkinteger(state, 3), luaL_checkinteger(state, 4),
                              optionalColor(state, 5), lua_toboolean(state, 6)); return 0;
}
int rfGuiCircle(lua_State* state) {
    displayManager.luaGuiCircle(luaL_checkinteger(state, 1), luaL_checkinteger(state, 2),
                                luaL_checkinteger(state, 3), optionalColor(state, 4),
                                lua_toboolean(state, 5)); return 0;
}
int rfButton(lua_State* state) {
    String name = luaL_checkstring(state, 1); name.toLowerCase();
    int pin = -1;
    if (name == "up") pin = BTN_UP;
    else if (name == "down") pin = BTN_DOWN;
    else if (name == "a" || name == "right") pin = BTN_A;
    else if (name == "b" || name == "left") pin = BTN_B;
    else return luaL_error(state, "button must be up, down, a, or b");
    lua_pushboolean(state, digitalRead(pin) == LOW); return 1;
}
int rfDelay(lua_State* state) {
    const int duration = luaL_checkinteger(state, 1);
    luaL_argcheck(state, duration >= 0 && duration <= 1000, 1, "delay must be 0..1000 ms");
    watchdog.feed(); delay(duration); watchdog.feed(); return 0;
}

int rfLabStart(lua_State* state) {
#if RF_LAB_TX_ENABLED
    const char* target = luaL_checkstring(state, 1);
    if (!appState.setJammerTargetByName(String(target))) return luaL_error(state, "invalid lab target");
    radioManager.startJammer(appState.jammerTarget); lua_pushboolean(state, appState.jamming); return 1;
#else
    return luaL_error(state, "active RF is disabled in analyzer build");
#endif
}
int rfLabStop(lua_State*) { radioManager.stopAll(); return 0; }

int rfLog(lua_State* state) {
    const char* message = luaL_checkstring(state, 1);
    fs::FS& fs = storageManager.filesystem();
    const char* path = storageManager.usingSd() ? "/RFSuite/log/lua.log" : "/lua.log";
    File file = fs.open(path, FILE_APPEND);
    if (!file) return luaL_error(state, "cannot open Lua log");
    file.printf("%lu,%s\n", static_cast<unsigned long>(millis()), message);
    file.close();
    return 0;
}

void vmHook(lua_State* state, lua_Debug*) {
    instructionBudget -= 1000;
    if (instructionBudget <= 0) luaL_error(state, "instruction limit exceeded");
}

bool safeName(const String& name) {
    if (!name.length() || name.length() > 48 || name.indexOf("..") >= 0 || name.indexOf('/') >= 0) return false;
    for (size_t i = 0; i < name.length(); ++i) {
        const char c = name[i];
        if (!isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-' && c != '.') return false;
    }
    return true;
}
}

bool LuaEngine::begin() {
    ready = storageManager.usingSd();
    error = ready ? "none" : "Lua scripts require an SD card";
    return ready;
}

bool LuaEngine::run(const String& requestedName, Stream& output) {
    if (!ready) { error = "SD card unavailable"; return false; }
    String name = requestedName;
    if (!name.endsWith(".lua")) name += ".lua";
    if (!safeName(name)) { error = "invalid script name"; return false; }
    const String path = String(storageManager.scriptsPath()) + "/" + name;
    File file = storageManager.filesystem().open(path, FILE_READ);
    if (!file) { error = "script not found"; return false; }
    if (file.size() > MAX_SCRIPT_BYTES) { file.close(); error = "script exceeds 32 KiB"; return false; }
    String source;
    source.reserve(file.size() + 1);
    while (file.available()) source += static_cast<char>(file.read());
    file.close();

    lua_State* state = luaL_newstate();
    if (!state) { error = "cannot allocate Lua VM"; return false; }
    luaopen_base(state); luaopen_table(state); luaopen_string(state); luaopen_math(state);
    lua_settop(state, 0);
    const char* blocked[] = {"dofile", "loadfile", "loadstring", "require", "collectgarbage", nullptr};
    for (const char** item = blocked; *item; ++item) { lua_pushnil(state); lua_setglobal(state, *item); }
    lua_register(state, "print", luaPrint);
    lua_newtable(state);
    lua_pushcfunction(state, rfMillis); lua_setfield(state, -2, "millis");
    lua_pushcfunction(state, rfPeak); lua_setfield(state, -2, "peak_channel");
    lua_pushcfunction(state, rfLevel); lua_setfield(state, -2, "level");
    lua_pushcfunction(state, rfLog); lua_setfield(state, -2, "log");
    lua_pushcfunction(state, rfSpectrum); lua_setfield(state, -2, "spectrum");
    lua_pushcfunction(state, rfStatus); lua_setfield(state, -2, "status");
    lua_pushcfunction(state, rfSetCursor); lua_setfield(state, -2, "set_cursor");
    lua_pushcfunction(state, rfFreeze); lua_setfield(state, -2, "freeze");
    lua_pushcfunction(state, rfSetBand); lua_setfield(state, -2, "set_band");
    lua_pushcfunction(state, rfSetTrace); lua_setfield(state, -2, "set_trace");
    lua_pushcfunction(state, rfBaseline); lua_setfield(state, -2, "capture_baseline");
    lua_pushcfunction(state, rfClearMax); lua_setfield(state, -2, "clear_max");
    lua_pushcfunction(state, rfWatch); lua_setfield(state, -2, "toggle_watch");
    lua_pushcfunction(state, rfSession); lua_setfield(state, -2, "recording");
    lua_pushcfunction(state, rfEnvironment); lua_setfield(state, -2, "environment");
    lua_pushcfunction(state, rfSubGhzStatus); lua_setfield(state, -2, "subghz_status");
    lua_pushcfunction(state, rfSubGhzSetFrequency); lua_setfield(state, -2, "subghz_set_frequency");
    lua_pushcfunction(state, rfSubGhzSetPreset); lua_setfield(state, -2, "subghz_set_preset");
    lua_pushcfunction(state, rfSubGhzSetRegion); lua_setfield(state, -2, "subghz_set_region");
    lua_pushcfunction(state, rfSubGhzAnalyzer); lua_setfield(state, -2, "subghz_analyzer");
    lua_pushcfunction(state, rfSubGhzRecord); lua_setfield(state, -2, "subghz_record");
    lua_pushcfunction(state, rfSubGhzFiles); lua_setfield(state, -2, "subghz_files");
    lua_pushcfunction(state, rfSubGhzReplay); lua_setfield(state, -2, "subghz_replay");
    lua_pushcfunction(state, rfOpen); lua_setfield(state, -2, "open_screen");
    lua_pushcfunction(state, rfGuiBegin); lua_setfield(state, -2, "gui_begin");
    lua_pushcfunction(state, rfGuiFooter); lua_setfield(state, -2, "gui_footer");
    lua_pushcfunction(state, rfGuiClear); lua_setfield(state, -2, "gui_clear");
    lua_pushcfunction(state, rfGuiClose); lua_setfield(state, -2, "gui_close");
    lua_pushcfunction(state, rfGuiText); lua_setfield(state, -2, "gui_text");
    lua_pushcfunction(state, rfGuiPixel); lua_setfield(state, -2, "gui_pixel");
    lua_pushcfunction(state, rfGuiLine); lua_setfield(state, -2, "gui_line");
    lua_pushcfunction(state, rfGuiRect); lua_setfield(state, -2, "gui_rect");
    lua_pushcfunction(state, rfGuiCircle); lua_setfield(state, -2, "gui_circle");
    lua_pushcfunction(state, rfButton); lua_setfield(state, -2, "button");
    lua_pushcfunction(state, rfDelay); lua_setfield(state, -2, "delay");
    lua_pushcfunction(state, rfLabStart); lua_setfield(state, -2, "lab_start");
    lua_pushcfunction(state, rfLabStop); lua_setfield(state, -2, "lab_stop");
    lua_setglobal(state, "rf");
    instructionBudget = MAX_VM_INSTRUCTIONS;
    lua_sethook(state, vmHook, LUA_MASKCOUNT, 1000);
    activeOutput = &output;
    int result = luaL_loadbuffer(state, source.c_str(), source.length(), name.c_str());
    if (result == 0) result = lua_pcall(state, 0, 0, 0);
    if (result != 0) error = lua_tostring(state, -1) ? lua_tostring(state, -1) : "Lua error";
    else error = "none";
    activeOutput = nullptr;
    lua_close(state);
    return result == 0;
}

void LuaEngine::list(Stream& output) const {
    if (!ready) { output.println("Lua scripts require an SD card."); return; }
    DIR* dir = opendir("/sd/RFSuite/scripts");
    if (!dir) { output.println("No scripts directory."); return; }
    bool found = false;
    for (dirent* entry = readdir(dir); entry; entry = readdir(dir)) {
        const String name(entry->d_name);
        if (name.endsWith(".lua")) {
            output.println(name);
            found = true;
        }
    }
    if (!found) output.println("No .lua scripts found.");
    closedir(dir);
}

size_t LuaEngine::listScripts(String* names, size_t capacity) const {
    if (!ready) return 0;
    DIR* dir = opendir("/sd/RFSuite/scripts");
    if (!dir) return 0;
    size_t count = 0;
    for (dirent* entry = readdir(dir); entry && count < capacity; entry = readdir(dir)) {
        const String name(entry->d_name);
        if (name.endsWith(".lua")) names[count++] = name;
    }
    closedir(dir);
    for (size_t i = 0; i < count; ++i)
        for (size_t j = i + 1; j < count; ++j)
            if (names[j] < names[i]) { String swap = names[i]; names[i] = names[j]; names[j] = swap; }
    return count;
}
