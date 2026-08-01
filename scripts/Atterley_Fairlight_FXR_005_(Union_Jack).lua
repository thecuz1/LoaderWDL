local name = 'Atterley Fairlight \'FXR 005\' (Union Jack)'
local guid = '{b94405d5-84ad-4504-ba93-e9f5bc4bff88}'
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
