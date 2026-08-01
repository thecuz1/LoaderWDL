-- Quick +10 Tech Points
local function addTechPoints()
    local player = GetLocalPlayerEntityId()
    if not player or player == 0 then 
        Notify("No player found", 0, 3.0)
        return 
    end
    TriggerRuleSmithRule('189922678', '', player)
    print("[Tech Points] +10 added!")
    Notify("+10 Tech Points added!", 0, 3.0)
end

-- Run
addTechPoints()
