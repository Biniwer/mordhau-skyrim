-- Where MORDHAU keeps its object list (GUObjectArray), for UE4SS, whose built-in search does not match this build.
-- The pattern is the start of FUObjectArray::AllocateUObjectIndex; the object list sits at a fixed distance from it.
-- Both addresses come from the debug symbols MORDHAU ships (tools/pdb_symbols.py), for version.txt 702625635.
function Register()
    return "48 89 5C 24 10 48 89 6C 24 18 56 57 41 54 41 56 41 57 48 83 EC 30 4C 8B F2 48 8B F9"
end

function OnMatchFound(MatchAddress)
    return MatchAddress + 63486224
end
