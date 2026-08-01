local name = 'Barge'
local guid = '{67421587-3fa9-4f89-aa2f-3af01ac0e576}'
local pos = GetReticleHitLocation()
if pos[1] == 0 or pos[2] == 0 or pos[3] == 0 then
    print(name .. ': Invalid spawn position - aim closer')
    Notify(name .. ': Invalid spawn position - aim closer')
else
    local veh = SpawnEntityFromArchetype(guid, pos[1], pos[2], pos[3], 0, 0, 0)
    if veh == GetInvalidEntityId() then
        print(name .. ': Failed to spawn')
        Notify(name .. ': Failed to spawn')
    else
        SetVehicleLockState(veh, 1)
        print(name .. ': Spawned')
        Notify(name .. ': Spawned')
    end
end
