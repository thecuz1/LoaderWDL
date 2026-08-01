-- SCRIPT: Heavy Storm Weather
-- CATEGORY: Weather
-- DESCRIPTION: Sets weather to heavy storm

pcall(function() PushEnvironmentWeatherOverride("WeatherPreset.9223372121331463515") end)

local f = io.open('notification_queue.txt', 'a')
if f then f:write('success|Weather: Heavy Storm!|3.0\n'); f:close() end
