-- Non-transmitting Sub-GHz Lua API self-test.
local passed, failed = 0, 0

local function test(name, fn)
    local ok, message = pcall(fn)
    if ok then
        passed = passed + 1
        print("PASS", name)
    else
        failed = failed + 1
        print("FAIL", name, tostring(message))
    end
end

test("status shape", function()
    local s = rf.subghz_status()
    assert(type(s.connected) == "boolean")
    assert(type(s.tx_enabled) == "boolean")
    assert(type(s.frequency) == "number")
    assert(type(s.preset) == "string")
    assert(type(s.region) == "string")
    assert(type(s.recording) == "boolean")
    assert(type(s.pulses) == "number")
    assert(type(s.last_error) == "string")
end)

test("library shape", function()
    local files = rf.subghz_files()
    assert(type(files) == "table")
    for _, name in ipairs(files) do
        assert(type(name) == "string")
        assert(string.match(name, "%.rfr$") or string.match(name, "%.sub$"))
    end
end)

test("invalid frequency rejected", function()
    local ok = pcall(function() rf.subghz_set_frequency(500) end)
    assert(not ok, "500 MHz should be outside supported CC1101 bands")
end)

test("invalid preset rejected", function()
    local ok = pcall(function() rf.subghz_set_preset("invalid") end)
    assert(not ok, "invalid preset should fail")
end)

test("invalid region rejected", function()
    local ok = pcall(function() rf.subghz_set_region("invalid") end)
    assert(not ok, "invalid region should fail")
end)

local summary = string.format("SUBGHZ SELF TEST: %d PASS, %d FAIL", passed, failed)
print(summary)
if failed > 0 then error(summary) end
