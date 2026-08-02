-- Turn off profiler on player (stealth?)
local function disableProfiler()
	local player = GetLocalPlayerEntityId()
	if not player or player == 0 then 
		Notify("No player found", 0, 3.0)
		return 
	end
	SetProfilerOn(player, 0)
	print("[Profiler] Disabled on self")
	Notify("Profiler disabled on self", 0, 3.0)
end

-- Stealth mode
disableProfiler()
