local name = 'Albion Security Turret (Rockets)'
local guid = '{7ee7a730-7065-4134-b127-19efba01790f}'
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
		print(name .. ': Spawned')
		Notify(name .. ': Spawned')
	end
end
