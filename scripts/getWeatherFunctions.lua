print('=== Searching Weather Functions ===')

-- Search for weather-related functions
for name, func in pairs(_G) do
    if type(func) == 'function' then
        local lowerName = string.lower(name)
        if string.find(lowerName, 'weather') or
            string.find(lowerName, 'climate') or
            string.find(lowerName, 'rain') or
            string.find(lowerName, 'storm') then
            print('Found: ' .. name)
        end
    end
end

-- Test if WeatherIDs exist
if WeatherIDs then
    print('WeatherIDs table exists!')
    local count = 0
    for k, v in pairs(WeatherIDs) do
        count = count + 1
        if count <= 5 then
            print('  ' .. k .. ' = ' .. v)
        end
    end
    print('Total weather presets: ' .. count)
end

print('===================================')
