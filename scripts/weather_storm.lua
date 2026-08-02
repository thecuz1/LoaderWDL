-- SCRIPT: Storm Weather
-- CATEGORY: Weather
-- DESCRIPTION: Sets weather to stormy

PushEnvironmentWeatherOverride("WeatherPreset.9223372121331463515", 1)

-- Notification
local f = io.open('notification_queue.txt', 'a')
if f then f:write('success|Weather: Storm|3.0\n'); f:close() end
