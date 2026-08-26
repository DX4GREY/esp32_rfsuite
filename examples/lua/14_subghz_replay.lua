-- Authorized-lab example. Change the filename to an owned signal in Library.
-- The native progress UI opens during TX and remains on the result screen.
local filename = "SIGNAL_1.rfr"
local status = rf.subghz_status()

assert(status.tx_enabled, "use the authorized_rf_lab firmware")
assert(status.region ~= "RX ONLY", "select an appropriate TX Region first")
assert(rf.subghz_replay(filename), "replay failed")

