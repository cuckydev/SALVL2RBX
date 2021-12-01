local id_reject = "rbxassetid://8146828973"
local id_accept = "rbxassetid://8146791213"

local function rec(p)
	for _,v in pairs(p:GetChildren()) do
		if v:IsA("MeshPart") then
			if v.TextureID == id_reject then
				v.TextureID = id_accept
			end
		elseif v:IsA("SurfaceAppearance") then
			if v.ColorMap == id_reject then
				v.ColorMap = id_accept
			end
		end
		rec(v)
	end
end

rec(workspace.Level)