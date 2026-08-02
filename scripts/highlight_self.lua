-- Glow effect on self
local function highlightSelf()
	local player = GetLocalPlayerEntityId()
	if player ~= nil and player ~= 0 then
		StartEntityHighlight(player, 4)
		print("[Highlight] Player glowing!")
		Notify("Player highlighted!", 0, 3.0)
	else
		Notify("No player found", 0, 3.0)
	end
end

-- Shine
highlightSelf()
