-- SCRIPT: Rainy Weather
-- CATEGORY: Weather
-- DESCRIPTION: Sets weather to rainy

PushEnvironmentWeatherOverride("WeatherPreset.9223372121331463513", 1)

-- Notification
local f = io.open('notification_queue.txt', 'a')
if f then f:write('success|Weather: Rainy|3.0\n'); f:close() end
