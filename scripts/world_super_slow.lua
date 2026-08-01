-- SCRIPT: Super Slow Motion
-- CATEGORY: World
-- DESCRIPTION: Sets game speed to 0.25x (super slow)

SetTimeScale(0.25)
SetSlomoFactor(0.25)

-- Notification
local f = io.open('notification_queue.txt', 'a')
if f then f:write('success|SUPER Slow Motion!|3.0\n'); f:close() end
