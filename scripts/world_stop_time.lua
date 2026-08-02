-- SCRIPT: Stop Time
-- CATEGORY: World
-- DESCRIPTION: Sets game speed to 0.00x (stops time)

SetTimeScale(0.00)
SetSlomoFactor(0.00)

-- Notification
local f = io.open('notification_queue.txt', 'a')
if f then f:write('success|Time STOPPED!|3.0\n'); f:close() end
