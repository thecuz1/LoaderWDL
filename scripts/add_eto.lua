-- Add 1,000 ETO instantly (repeat for more)
-- Real native from mod tools

local function addETO()
    local player = GetLocalPlayerEntityId()
    if not player or player == 0 then 
        Notify("No player found", 0, 3.0)
        return 
    end
    TriggerRuleSmithRule('589221860', '', player)
    print("[ETO Hack] +1,000 ETO added!")
    Notify("+1,000 ETO added!", 0, 3.0)
end

-- Run it
addETO()
