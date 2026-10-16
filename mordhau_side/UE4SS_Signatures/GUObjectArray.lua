-- Where MORDHAU keeps its object list (GUObjectArray), for UE4SS, whose built-in search does not match this build.
-- The pattern is the start of FUObjectArray::AllocateUObjectIndex; the object list sits at a fixed distance from it.
-- Both addresses come from the debug symbols MORDHAU ships (tools/pdb_symbols.py), for version.txt 702625635.
-- The Epic Games Store build of that same version has the same code but its data laid out 0x4B20 bytes further on
-- (AllocateUObjectIndex at RVA 0x1B65C70, GUObjectArray at 0x57F60A0 in its .pdb). It is told apart by the size of
-- its program; the plugin starts MORDHAU in Binaries\Win64, so the file is found by its plain name.
local STEAM_DISTANCE = 63486224
local EPIC_DISTANCE = 63505456
local EPIC_EXE_BYTES = 95824896

local function Distance()
    local ok, size = pcall(function()
        local f = io.open("Mordhau-Win64-Shipping.exe", "rb")
        if not f then return nil end
        local bytes = f:seek("end")
        f:close()
        return bytes
    end)
    if ok and size == EPIC_EXE_BYTES then
        return EPIC_DISTANCE
    end
    return STEAM_DISTANCE
end

function Register()
    return "48 89 5C 24 10 48 89 6C 24 18 56 57 41 54 41 56 41 57 48 83 EC 30 4C 8B F2 48 8B F9"
end

function OnMatchFound(MatchAddress)
    return MatchAddress + Distance()
end
