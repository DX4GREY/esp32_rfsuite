-- Individual radio diagnostics. Full nRF24 loopback needs authorized_rf_lab.
local function yn(value) return value and "OK" or "FAIL" end

print("=== RADIO MODULE TEST ===")
local nrf = rf.radio_test()
print("nRF24 RADIO 1 detect=" .. yn(nrf.radio1_detected))
print("nRF24 RADIO 2 detect=" .. yn(nrf.radio2_detected))
if not nrf.tx_test_enabled then
    print("LOOPBACK: UNVERIFIED (analyzer build)")
elseif not nrf.radio1_detected or not nrf.radio2_detected then
    print("LOOPBACK: UNVERIFIED (needs both radios)")
else
    print("R1 -> R2=" .. yn(nrf.radio1_to_radio2))
    print("R2 -> R1=" .. yn(nrf.radio2_to_radio1))
    if not nrf.radio1_to_radio2 then
        print("  CHECK: R1 TX or R2 RX path")
    end
    if not nrf.radio2_to_radio1 then
        print("  CHECK: R2 TX or R1 RX path")
    end
    print("nRF24 RESULT: " .. ((nrf.radio1_to_radio2 and nrf.radio2_to_radio1)
          and "PASS" or "RF PATH FAILED"))
end

print("--- CC1101 ---")
local sub = rf.subghz_status()
print("CC1101 detect=" .. yn(sub.connected))
if not sub.connected then
    print("  RESULT: NOT DETECTED")
else
    rf.subghz_analyzer(true)
    rf.delay(500)
    local after = rf.subghz_status()
    rf.subghz_analyzer(false)
    local rx_ok = after.analyzer_peak_rssi > -127
    print(string.format("  RX=%s RSSI=%d @ %.2fMHz", yn(rx_ok),
                        after.analyzer_peak_rssi, after.analyzer_peak_frequency))
    print(after.tx_enabled and
          "  TX=UNVERIFIED (needs external receiver)" or
          "  TX=BLOCKED (analyzer build)")
    print("  RESULT: " .. (rx_ok and "RX PASS" or "DETECTED BUT RX FAILED"))
end
print("=== TEST COMPLETE ===")
