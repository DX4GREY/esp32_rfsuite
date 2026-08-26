-- Read-only CC1101/Sub-GHz status report.
local s = rf.subghz_status()

print("CC1101", s.connected and "connected" or "offline")
print("simulation", s.simulation)
print("TX compiled", s.tx_enabled)
print(string.format("frequency %.2f MHz", s.frequency))
print("preset", s.preset, "region", s.region)
print("recording", s.recording, "armed", s.armed)
print("pulses", s.pulses, "RSSI", s.rssi)
print("analyzer", s.analyzer_running)
print(string.format("peak %.2f MHz %d dBm",
                    s.analyzer_peak_frequency, s.analyzer_peak_rssi))
print("last error", s.last_error)

