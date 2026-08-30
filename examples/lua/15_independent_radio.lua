-- Use one nRF24 atomically without reconfiguring the other module.
local selected = 1
local channel = 76

local level, rx_error = rf.radio_sample(selected, channel, 128)
if level == nil then
    print("R" .. selected .. " RX skipped:", rx_error)
else
    print(string.format("R%d CH%d carrier=%d%%", selected, channel, level))
end

-- TX is a single bounded packet and only succeeds in authorized_rf_lab.
-- Uncomment when operating in an authorized test environment.
-- local sent, tx_error = rf.radio_transmit(selected, channel, "RFSuite test", 0, "1m")
-- print(sent and "TX OK" or ("TX skipped: " .. tostring(tx_error)))
