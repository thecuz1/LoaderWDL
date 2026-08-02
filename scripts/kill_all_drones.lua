-- One-time mass drone disable (repeat if new drones spawn)

local function killAllDrones()
	local player = GetLocalPlayerEntityId()
	if not player or player == 0 then 
		Notify("No player found", 0, 3.0)
		return 
	end

	local drones = CAIAgentManager_GetInstance():GetAIDronesFromLUA("All", "", 1)
	local count = 0

	for i, drone in ipairs(drones) do
		TryTriggerHack("DroneDisable", player, drone)
		count = count + 1
	end

	print("[Drone Killer] " .. count .. " drones shut down")
	Notify(count .. " drones disabled!", 0, 3.0)
end

-- Execute
killAllDrones()
