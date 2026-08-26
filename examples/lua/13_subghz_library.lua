-- Print all native .rfr and Flipper RAW .sub files in the Sub-GHz library.
local files = rf.subghz_files()
print(string.format("Sub-GHz files: %d", #files))
for index, name in ipairs(files) do
    print(index, name)
end

