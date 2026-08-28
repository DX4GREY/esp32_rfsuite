# Persistence and Settings Schema

## NVS namespace

Configuration uses ESP32 Preferences namespace:

```text
appstate
```

The current schema version is `6`.

## Stored keys

| Key | Type | Default | Validation / meaning |
|---|---|---|---|
| `schema` | unsigned byte | `7` when first saved | Persistence schema version |
| `power` | integer | `RF24_PA_MAX` | Four global TX levels mapped to nRF24 PA and CC1101 PA-table values |
| `dwell` | integer | `200` | Global nRF24/CC1101 lab hop dwell, clamped to `10–10000` µs |
| `target` | unsigned byte | Wi-Fi | Must be one of six target enums |
| `profile` | unsigned byte | BALANCED | FAST, BALANCED, DEEP, or CUSTOM |
| `custom` | integer | `40` | Clamped to `10–100` samples |
| `theme` | unsigned byte | CYBER | One of ten UI profiles; selects palette, menu layout, geometry, and animation |
| `sniff_sd` | boolean | `false` | Save packet-sniffer records to SD when mounted |
| `sub_pre` | unsigned byte | OOK 650 kHz | CC1101 modulation preset, validated `0–4` |
| `sub_reg` | unsigned byte | RX ONLY | Sub-GHz TX policy: RX ONLY, ETSI, or FCC |
| `sub_trig` | boolean | `true` | Enable RSSI-triggered raw recording |
| `sub_thr` | signed byte | `-80` | Auto-trigger threshold, clamped to `-90…-70` dBm |
| `sub_rep` | unsigned byte | `1` | Saved replay count, normalized to 1x, 3x, or 5x |
| `trace` | unsigned byte | LIVE | LIVE, AVG, MAX, or DELTA; DELTA restores as LIVE |
| `evt_thr` | unsigned byte | `60` | Clamped to `5–100` |
| `evt_hys` | unsigned byte | `10` | Clamped to `0–threshold` |
| `evt_dur` | unsigned byte | `2` | Clamped to `1–20` sweeps |
| `evt_multi` | unsigned byte | `1` | Clamped to `1–16` channels |
| `watch` | byte blob | all false | Exactly the size of the 126-entry watch array |
| `env_win` | unsigned short | `10` | One of `1, 5, 10, 30, 60` seconds |
| `env_min`, `env_max` | unsigned byte | `0`, `125` | Validated inclusive RF-channel range |
| `env_burst` | unsigned byte | `25` | Burst delta threshold, clamped to `5–80` |
| `env_alpha` | unsigned byte | `25` | EMA weight, clamped to `1–100` percent |
| `env_hist` | unsigned byte | `32` | History depth, clamped to `8–32` |
| `env_cmp_n` | unsigned byte | `2` | Comparison count, clamped to `2–4` |
| `env_cmp` | 4-byte blob | `6, 11, 42, 80` | Comparison RF channels; invalid values use defaults |

The lab build additionally stores `prb_ch`, `prb_int`, `prb_cnt`, `prb_dur`,
`prb_size`, and `prb_rate`. The receive-only build neither loads nor writes
these probe keys.

## Deferred writes

Setters call `markSettingsDirty()` instead of writing immediately. The main loop saves the complete configuration after 1.5 seconds without another settings change. This coalesces rapid UI changes and reduces NVS write frequency.

Explicit `saveSettings()` is used by factory reset. Shutdown currently does not force an early dirty-settings flush, so wait at least 1.5 seconds after a setting change before selecting Shutdown if persistence is important.

## Migration

Schema 0/legacy installations load the original keys. Schema 2 adds analyzer
trace, event, and watch settings; schema 3 adds RF-environment and lab-probe
settings; schema 4 added the legacy standalone menu layout; schema 5 adds
packet-sniffer SD logging; schema 6 adds Sub-GHz settings; schema 7 folds menu
layout into the selected theme. The old `menu_view` key is ignored. Missing
newer fields use defaults and schedule one deferred schema-7 save. Every loaded
enum and numeric value is validated before use.

Future migrations should:

1. Preserve the ability to read older keys.
2. Validate before casting stored integers to enums.
3. Supply deterministic defaults for new settings.
4. Increment `SETTINGS_SCHEMA_VERSION`.
5. Avoid rewriting NVS on every boot after successful migration.

## DELTA trace exception

The trace selection is persistent, but DELTA requires an environmental baseline stored only in RAM. If NVS contains DELTA at boot, the firmware restores LIVE instead. This avoids presenting an all-zero or stale comparison as a valid measurement.

## Factory reset

Run the exact command:

```text
factory reset confirm
```

It performs these actions:

1. Stops radio activity and the recorder.
2. Clears the `appstate` NVS namespace.
3. Restores default power, dwell, theme, profile, custom depth, trace, event settings, watchlist, and target.
4. Saves schema-5 defaults, including RF-environment configuration, GRID layout, and disabled sniffer logging.
5. Reboots.

Factory reset does not erase the LittleFS session file. Starting a new session replaces that file.

## Runtime-only state

The following is intentionally not stored in NVS:

- current app screen and menu selection;
- analyzer band and radio mode;
- freeze state, zoom, cursor, and peak-follow state;
- baseline values and baseline-valid flag;
- live, average, maximum, and decaying peak arrays;
- waterfall, survey totals, and event history;
- recorder active state and runtime sweep count;
- radio mutex performance counters.
- RF-environment channel statistics, histories, burst events, and snapshots.

This split prevents high-rate analyzer data from wearing NVS and avoids restoring environmental measurements that are no longer valid.
