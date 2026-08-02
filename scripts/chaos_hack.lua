-- Hack Spam: Disable drones + overheat / lock brakes on nearby vehicles
-- Call this function once when you press a key or want chaos

local function chaosVehiclesAndDrones()
    local player = GetLocalPlayerEntityId()
    if not player or player == 0 then
        print("[Chaos Hack] No player found")
        Notify("No player found", 0, 3.0)
        return
    end

    -- All drones
    local drones = CAIAgentManager_GetInstance():GetAIDronesFromLUA("All", "", 1)
    for i, drone in ipairs(drones) do
        TryTriggerHack("DroneDisable", player, drone)
        -- TryTriggerHack("DroneHijack", player, drone)   -- if you want to steal instead
    end

    -- All vehicles
    local vehicles = CAIAgentManager_GetInstance():GetAIAgentsOfGroupFromLUA_v2("Vehicle", 0, "", 0, 0)
    for i, veh in ipairs(vehicles) do
        TryTriggerHack("EngineOverheat", player, veh)
        TryTriggerHack("LockBrakes", player, veh)
        -- ExplodeVehicle(veh)   -- uncomment if you want instant explosions
    end

    print("[Chaos Hack] " .. #drones .. " drones + " .. #vehicles .. " vehicles targeted!")
    Notify("Chaos: " .. #drones .. " drones + " .. #vehicles .. " vehicles hacked!", 0, 3.0)
end

-- Run it right away (or bind to hotkey in your hook)
chaosVehiclesAndDrones()
