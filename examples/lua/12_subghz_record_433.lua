-- Prepare a 433.92 MHz OOK raw capture and open the native Record screen.
-- Press A on the Record screen to stop/save; B returns to the Sub-GHz menu.
local status = rf.subghz_status()
assert(status.connected or status.simulation, "CC1101 is unavailable")

rf.subghz_set_preset("ook650")
assert(rf.subghz_set_frequency(433.92), "frequency rejected")
assert(rf.subghz_record(true, 433.92), "record could not start")
rf.open_screen("subghz_record")

