-- Distract every ped
local function distractAll()
    local player = GetLocalPlayerEntityId()
    local humans = CAIAgentManager_GetInstance():GetAIAgentsOfGroupFromLUAv2("Human", 0, "", 0, 0)
    local count = 0
    for i, ped in ipairs(humans) do
        TryTriggerHack("Distract", player, ped)
        count = count + 1
    end
    print("[Distract] " .. count .. " peds distracted!")
    Notify(count .. " peds distracted!", 0, 3.0)
end

-- Go
distractAll()
