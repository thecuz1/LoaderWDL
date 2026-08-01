-- SCRIPT: Ultra Speed
-- CATEGORY: World
-- DESCRIPTION: Sets game speed to 5.0x (ultra fast)

SetTimeScale(5.0)
SetSlomoFactor(5.0)

-- Notification
local f = io.open('notification_queue.txt', 'a')
if f then f:write('success|ULTRA Speed!|3.0\n'); f:close() end
