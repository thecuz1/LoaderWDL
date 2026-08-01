-- SCRIPT: Set Time to Noon
-- CATEGORY: World
-- DESCRIPTION: Sets the in-game time to 12:00 (noon)

SetTimeOfDayHourAndMinute(12, 0)

-- Notification
local f = io.open('notification_queue.txt', 'a')
if f then f:write('success|Time set to NOON (12:00)|3.0\n'); f:close() end
