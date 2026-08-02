
local function inspectFunction(fn)
local info = debug.getinfo(fn, "uS")
-- info.nups = number of upvalues
-- info.nparams = number of parameters (available in some Lua versions)

-- print("Function defined at line:", info.linedefined)
-- print("Source file:", info.source)

local i = 1
local parameter = ''
while true do
    local name, value = debug.getlocal(fn, i) -- Note: usually works best on active stack levels
    if not name then break end
    parameter = parameter + string.format(" Parameter %d: %s", i, name)
    i = i + 1
end
return parameter
end

local path = "globals.lua"
local file, err = io.open(path, "w")
if not file then
print("Failed to open dump file: " .. tostring(err))
return
end

file:write(string.format("--- GLOBAL TABLE DUMP (%s) ---\n", _VERSION))

for k, v in pairs(_G) do
    local success, valStr = pcall(function() return tostring(v) end)
    if success then
        file:write(string.format("[%s] %s\n", type(v), tostring(k)))
        if type(v) == "table" then
            local tableName = tostring(k)
            if tableName ~= "_G" then
                for subK, subV in pairs(v) do
                    local subSuccess, subValStr = pcall(function() return tostring(subV) end)
                    if subSuccess then
                        file:write(string.format("  [%s] %s\n", type(subV), tostring(subK)))
                    end
                end
            end
        end
    end
end

file:close()

print("Global table dumped successfully to " .. path)
