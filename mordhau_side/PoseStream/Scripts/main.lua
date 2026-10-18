-- PoseStream: runs inside MORDHAU (through UE4SS) in a LOCAL OFFLINE match only. It makes MORDHAU the character half
-- of the mashup while Skyrim is the world half:
--   out: the local character's bones exactly as MORDHAU's own animation system posed them, its attack stage, blade
--        trace, stamina and health  -> cache/live_pose.bin (read by the Skyrim plugin)
--   in:  what the player is doing in Skyrim (move, sprint, attack direction, parry, jump) <- cache/live_input.txt,
--        handed to MORDHAU's own input functions, so its own combat rules decide what happens.
--
-- live_pose.bin (little endian): 'DMLP', uint32 sequence, float time, int32 bones, then per bone 7 floats: rotation
-- x,y,z,w and translation x,y,z relative to the parent bone; then the state block: float[3] blade start, float[3] blade
-- end (character space, cm), int32 stage (-1 none, 0 windup, 1 release, 2 recovery), int32 kind (0 other, 1 attack,
-- 2 parry), float seconds in this motion, float stamina, float health, float speed right, float speed forward (cm/s),
-- char[48] motion name, float[4] weapon rotation xyzw + float[3] weapon position (character space), int32 has weapon;
-- then uint32 sequence again so a reader can tell a complete write from a torn one.
-- After the weapon block: uint32 times the player was hit, float damage of the last one (MORDHAU's, of 100), uint32 id
-- of the stand-in that did it; then 3 x (uint32 id, uint32 times hit by the player, float damage of the last hit,
-- int32 stage of its attack or -1).
-- Stand-ins: Skyrim sends up to 3 nearby characters as "g:id,right,forward,up,yaw,attacks,blocking" (cm and degrees
-- relative to the player). Each is given one MORDHAU bot with its own AI switched off, held at that place relative to
-- the MORDHAU player, told to attack when the Skyrim character attacks and to parry when it blocks. So MORDHAU's own
-- combat decides what hits, what is parried, clashes, ripostes, stamina. Nobody dies in MORDHAU: health is put back.
-- Then float[4] camera rotation xyzw + float[3] camera position in character space and int32 "is first person":
-- Skyrim draws MORDHAU's first-person view by putting this camera on its own.
-- Then per stand-in slot (3): int32 look (1.. = which baked look it is drawn with, 0 = empty slot), 158 x 7 floats local
-- bone transforms (a rotation of all zeros = "not read, use the model's own rest pose"), float[4]+float[3] weapon
-- transform in ITS character space, int32 has weapon.
-- Reading a bone costs ~25 microseconds here, so only bones that animate are read every time: the player's body and
-- fingers each pose, the face and shape bones once a second; a stand-in's body 20 times a second, its fingers rarely.
-- Each stand-in entry also carries: uint32 times hit by ANOTHER stand-in, float damage of the last such hit, uint32
-- id of that stand-in (two Skyrim characters fighting each other).
-- live_input.txt (one line): sequence forward right sprint pitch mouseRightSum mouseUpSum, then "id:Action" pairs for
-- the latest requests.
-- Where the cache shared with Skyrim's plugin is: the folder named in cache_dir.txt beside this mod's Scripts folder
-- (the launcher writes it), else %LOCALAPPDATA%\MordhauSkyrim\cache (where the plugin builds it by default).
if not CACHE then
    local here = (debug.getinfo(1, "S").source or ""):gsub("^@", ""):gsub("[/\\][^/\\]*$", "")   -- ...\Mods\PoseStream\Scripts
    for _, path in ipairs({ here .. "\\..\\cache_dir.txt", "Mods\\PoseStream\\cache_dir.txt" }) do
        local f = io.open(path, "r")
        if f then
            local line = f:read("l")
            f:close()
            if line and #line > 2 then CACHE = line:gsub("[%s/\\]+$", "") .. "\\"; break end
        end
    end
    CACHE = CACHE or ((os.getenv("LOCALAPPDATA") or ".") .. "\\MordhauSkyrim\\cache\\")
end
do
    -- the cache is built by Skyrim's plugin (from this MORDHAU's own files) the first time Skyrim is started through
    -- the launcher: without it there is nothing to do here
    local made = io.open(CACHE .. "bone_names.lua", "r")
    if not made then
        print("[PoseStream] no cache at " .. CACHE .. " yet: start through Play-Mashup.ps1 (Skyrim's plugin builds it on a first start)\n")
        return
    end
    made:close()
end

-- OFFLINE ONLY. This script is never to be in a MORDHAU that is, or could go, online. Two guards:
-- (1) MORDHAU was started any other way than by the mashup's launcher (which leaves the time in
--     cache/live_session.txt just before it starts MORDHAU) - a normal start from Steam, with the loader left behind in
--     the folder: MORDHAU is closed again at once, with a notice saying how to clean the folder.
-- (2) In a mashup session, the moment the game is anything but a stand-alone local match (it joined a server, or
--     hosts one): MORDHAU is closed. Looked at every second (see PS_offline_guard).
-- A window with the reason is shown (PowerShell's message box; the game itself is on its way out).
function PS_close_mordhau(why, atOnce)
    if PS_closing then return end
    PS_closing = true
    print("[PoseStream] CLOSING MORDHAU: " .. why .. "\n")
    pcall(function()
        local text = ("MORDHAU x Skyrim: MORDHAU was closed. " .. why):gsub("[\"']", "")
        os.execute('start "" /min powershell -NoProfile -WindowStyle Hidden -Command "Add-Type -AssemblyName PresentationFramework; [void][System.Windows.MessageBox]::Show(\'' .. text .. '\', \'MORDHAU x Skyrim - offline only\')"')
    end)
    local function quit()
        local ks = StaticFindObject("/Script/Engine.Default__KismetSystemLibrary")
        local pc = FindFirstOf("PlayerController")
        if ks and ks:IsValid() and pc and pc:IsValid() then
            if not pcall(function() ks:QuitGame(pc, pc, 0, false) end) then pcall(function() ks:ExecuteConsoleCommand(pc, "quit", pc) end) end
            return true
        end
        return false
    end
    -- (asked nicely first; three seconds later the process is ended outright - early in the game's start there is
    -- nothing to ask yet, and it must not stay open)
    -- (the script's own exit alone left the process alive for about a minute - measured 63 s on a normal start with
    -- another loader in the folder, long enough to join a server: the process is also ended from outside, at once)
    local function hard()
        pcall(function() os.execute('start "" /min taskkill /F /IM Mordhau-Win64-Shipping.exe') end)
        os.exit(0)
    end
    if atOnce == true then hard() end   -- (not this mod's own MORDHAU: not one frame more with the script in it)
    pcall(quit)
    if atOnce == "now" then hard() end   -- (no longer offline: the normal exit is asked, the hard one not waited for)
    LoopAsync(3000, function() hard(); return true end)
end
function PS_offline_guard(pc)
    -- (every frame: sheets/live_link.json offline_stop. Returns true only when the match is known to be a
    -- stand-alone local one; unknown counts as not offline.)
    local alone = nil
    pcall(function()
        if not PS_ks or not PS_ks:IsValid() then PS_ks = StaticFindObject("/Script/Engine.Default__KismetSystemLibrary") end
        local answer = PS_ks:IsStandalone(pc)
        if answer == true then alone = true elseif answer == false then alone = false end
    end)
    if PS_testNotAlone then alone = false end   -- (for trying the guard out: set by a snippet)
    if PS_testUnknown then alone = nil end
    local function stop(why, text)
        -- everything of the mod stops this frame; the plugin is told to end the process; the exit is not waited for
        PS_stopped = true
        pcall(function()
            local f = io.open(CACHE .. "live_stop.txt", "w")
            if f then f:write(why .. "\n"); f:close() end
        end)
        PS_close_mordhau(text, "now")
    end
    if alone == nil then
        PS_unknownSince = PS_unknownSince or os.clock()
        if os.clock() - PS_unknownSince > 1.0 then
            stop("could not tell whether the match is offline", "It could not be confirmed that MORDHAU is in its own offline match. The mod is for offline play only.")
        end
        return false
    end
    PS_unknownSince = nil
    if not alone then
        stop("not a stand-alone local match any more", "It is not in its own offline match any more (a server was joined or hosted). The mod is for offline play only. Before you play MORDHAU online, make sure there is no dwmapi.dll in its Mordhau/Binaries/Win64 folder.")
        return false
    end
    return true
    -- (That it was started with -insecure is not checked here: the game's command line could not be read from the
    -- script - KismetSystemLibrary:GetCommandLine came back as garbage. It follows from the session key:
    -- only the mod's own start has the key, and that start always passes -insecure.)
end
if not PS_loop then
    -- (sheets/live_link.json session_key: the key in this game's environment must be the one written for this start)
    local stamp, key = nil, nil
    local f = io.open((CACHE .. "live_session.txt"), "r")
    if f then stamp = tonumber(f:read("l") or ""); key = f:read("l"); f:close() end
    local mine = os.getenv("MORDHAU_SKYRIM_SESSION")
    if not stamp or math.abs(os.time() - stamp) > 600 or not key or #key < 16 or mine ~= key then
        print("[PoseStream] not this mod's own MORDHAU (no session key, or not the one of this start): ending it\n")
        PS_close_mordhau("It was started normally while the script loader of the MORDHAU x Skyrim mod was still in its folder. Delete dwmapi.dll from MORDHAUs Mordhau/Binaries/Win64 folder, then start MORDHAU again. Do not play online before that.", true)
        return
    end
    -- A mashup session, and the script is running: the loader has done its work. It is made unloadable at once - a
    -- loaded file can be renamed, not deleted - so that it never loads again even if whoever started this session dies
    -- right now (sheets/live_link.json: loader_self_removal). The starter deletes dwmapi.dll.old later.
    pcall(function()
        local here = (debug.getinfo(1, "S").source or ""):gsub("^@", ""):gsub("[/\\][^/\\]*$", "")   -- ...\Mods\PoseStream\Scripts
        for _, bin in ipairs({ here .. "\\..\\..\\..\\", "" }) do
            os.remove(bin .. "dwmapi.dll.old")
            if os.rename(bin .. "dwmapi.dll", bin .. "dwmapi.dll.old") then print("[PoseStream] the script loader is renamed to dwmapi.dll.old: it does not load again\n"); break end
        end
    end)
end

local OUT = (CACHE .. "live_pose.bin")
local IN = (CACHE .. "live_input.mem")
-- Shared memory with Skyrim: two fixed-size files kept open for the whole session (Skyrim's plugin maps them into its
-- memory; Lua can only use file handles, which Windows backs with the same pages). Written and read in place.
local POSE_BYTES, INPUT_BYTES = 131072, 16384   -- (doubled for 24 stand-ins; the plugin's kPoseBytes, kInputBytes)
local function shared(path, size)
    local f = io.open(path, "r+b")
    if not f then
        local make = io.open(path, "wb")
        if make then make:write(string.rep("\0", size)); make:close() end
        f = io.open(path, "r+b")
    elseif f:seek("end") < size then
        f:seek("end"); f:write(string.rep("\0", size - f:seek("end")))
    end
    if f then f:setvbuf("full", size) end
    return f
end
local CMD = (CACHE .. "live_cmd.lua")
local CONFIG = (CACHE .. "live_config.lua")
local MAP = "FFA_ThePit"
do
    -- (dev: another map to stand in, named alone in cache/map.txt - for trying out flatter ground)
    local f = io.open(CACHE .. "map.txt", "r")
    if f then
        local name = (f:read("l") or ""):gsub("%s+$", "")
        f:close()
        if name:match("^[%w_]+$") then MAP = name end
    end
end
local BONES = dofile((CACHE .. "bone_names.lua"))
local RTS_ParentBoneSpace = 3
local STREAM_EVERY = 0.0035  -- seconds between poses written: every frame up to ~250 a second (Skyrim shows them unsmoothed)
-- UE4SS keeps a registry reference to every table handed to a function as an out-parameter (the hit result of the
-- "set location" calls) and never lets go of it: measured, about 570 a second with three stand-ins, 1.36
-- million after 40 minutes and 4 GB of script memory that no collection could free (MORDHAU at 14 GB). The reference
-- is given back here the way lauxlib's luaL_unref does it (Lua 5.4: the registry's free list starts at slot 3).
local REG, REG_FREE = debug.getregistry(), 3
local function ref_coming()
    local f = REG[REG_FREE]
    if type(f) ~= "number" then return nil end   -- (not the registry layout this was written for: leave it alone)
    return f ~= 0 and f or rawlen(REG) + 1
end
local function ref_release(ref)
    if not ref then return end
    local v = REG[ref]
    if type(v) == "table" and rawget(v, "ImpactNormal") ~= nil then REG[ref] = REG[REG_FREE]; REG[REG_FREE] = ref end
end
-- A stand-in's body (its mesh: what blades hit, what holds its weapon) against its own capsule (what walks on
-- MORDHAU's ground). MORDHAU's arena floor is uneven where Skyrim's ground is level, and the other way round: a free
-- stand-in stood 20-38 cm lower than the player while Skyrim had the two level, and chest-high swings went over it or
-- counted as head hits. The capsule keeps walking MORDHAU's ground as MORDHAU's AI moves it; the body is shifted up or
-- down by the difference (a_lift cm, eased, at most 60), nil = as MORDHAU has it.
-- How: MORDHAU sets the mesh back to the character's BaseTranslationOffset every frame (a shift of the mesh itself
-- was gone the same frame: measured). So that offset is what is changed; the player's own, never touched, is the
-- measure of what it was.
local BODY_SHIFT_MOST = 150  -- cm: the most a stand-in's body is raised or lowered against its capsule (sheets/builds.json standin_body_shift_most)
function PS_body_height(a_char, a_bot, a_lift)
    pcall(function()
        local base = (PAWN and PAWN:IsValid() and PAWN.BaseTranslationOffset.Z) or -97
        local want = 0
        -- (a stand-in of another size than the player - sheets/builds.json standin_scale: its capsule is that much
        -- shorter or taller, so with the two capsules' middles level its feet are not; and the offset set here is in
        -- its own, scaled measure. Measured without this: feet 4.8 cm too high at 0.95 of the player's size.)
        local size = a_bot.size or 1
        if a_lift then
            -- (up to BODY_SHIFT_MOST: measured in duels, MORDHAU's own ground under a free stand-in was up to 142 cm off the
            -- player's; with the old limit of 60 the body stood wrong in 4-21% of the samples)
            want = math.max(-BODY_SHIFT_MOST, math.min(BODY_SHIFT_MOST, a_lift + base * (1 - size)))
            a_bot.lift = (a_bot.lift or 0) + (want - (a_bot.lift or 0)) * 0.25
            want = a_bot.lift
        else
            a_bot.lift = nil
        end
        local has = a_char.BaseTranslationOffset
        if math.abs(has.Z - (base + want / size)) > 0.3 then
            PS_why["height"] = (PS_why["height"] or 0) + 1
            a_char.BaseTranslationOffset = { X = has.X, Y = has.Y, Z = base + want / size }
        end
    end)
end
-- Stands a character on MORDHAU's ground where it is: lifted above it, then moved down until it touches (a sweep
-- stops at the first thing in the way). a_near: a height near the ground there (the player's own).
function PS_seat(a_char, a_near)
    pcall(function()
        local at = a_char:K2_GetActorLocation()
        local ref = ref_coming()
        a_char:K2_SetActorLocation({ X = at.X, Y = at.Y, Z = a_near + 130 }, false, {}, true)
        a_char:K2_SetActorLocation({ X = at.X, Y = at.Y, Z = a_near - 160 }, true, {}, false)
        ref_release(ref)
    end)
end
function PS_place(a, at, rot, teleport)
    local ref = ref_coming()
    a:K2_SetActorLocationAndRotation(at, rot, false, {}, teleport)
    ref_release(ref)
end
function PS_place_at(a, at)
    local ref = ref_coming()
    a:K2_SetActorLocation(at, false, {}, false)
    ref_release(ref)
end
-- (what an earlier version of this script left behind in a running MORDHAU, and the ones the above misses - measured
-- about 9 a second in a duel, when something else takes a reference in between: swept every few seconds from the frame)
function PS_sweep_refs()
    if type(REG[REG_FREE]) ~= "number" then return end
    local old = {}
    for k, v in next, REG do
        if type(k) == "number" and type(v) == "table" and rawget(v, "ImpactNormal") ~= nil then old[#old + 1] = k end
    end
    for _, k in ipairs(old) do REG[k] = REG[REG_FREE]; REG[REG_FREE] = k end
    if #old > 1000 then print(string.format("[PoseStream] gave back %d leaked hit-result references\n", #old)) end
end
PS_sweep_refs()
local LEASH = 600            -- cm the MORDHAU character may wander before it is put back (Skyrim owns the position)

local sequence = 0
local opened = false
local reported = false
local started = os.clock()
local lastStream = 0
local names = {}
for i, n in ipairs(BONES) do names[i] = FName(n) end

local function log(msg) print("[PoseStream] " .. msg .. "\n") end

-- The class name of a game object, "" when there is none. (Asking a missing object for its class crashes the loader
-- outright instead of raising an error, so every such question goes through here.)
function class_name(o)
    if o and o:IsValid() then return o:GetClass():GetFName():ToString() end
    return ""
end

-- True when a game object is really there (calling into a missing one can crash the loader, see above).
function there(o) return o ~= nil and o:IsValid() end

-- A stage number from a motion object, nil when there is no motion or it has no stages.
function motion_stage(m)
    if m and m:IsValid() then
        local st = m.Stage
        if type(st) == "number" then return st end
    end
    return nil
end

-- which bones move with animation (the rest shape the face and body and stay put)
local BODY, FINGERS, STILL = {}, {}, {}
for i, n in ipairs(BONES) do
    if n:find("Finger") then FINGERS[#FINGERS + 1] = i
    elseif n:find("Adjust") or n:find("Breast") or n:find("Lips") or n:find("Maxilar") or n:find("Cheek") or n:find("Nose") or n:find("Eyebrow") or n:find("Ear")
        or n:find("Eye") or n:find("Lid") or n:find("Tongue") or n:find("Mandible") or n:find("Gluteus") or n:find("Trapezius") or n:find("Belly") then STILL[#STILL + 1] = i
    else BODY[#BODY + 1] = i end
end
local ZERO_BONE = string.pack("<fffffff", 0, 0, 0, 0, 0, 0, 0)
local function read_bones(mesh, list, into)
    for k = 1, #list do
        local i = list[k]
        local t = mesh:GetSocketTransform(names[i], RTS_ParentBoneSpace)
        local r, p = t.Rotation, t.Translation
        into[i] = string.pack("<fffffff", r.X, r.Y, r.Z, r.W, p.X, p.Y, p.Z)
    end
end

-- the Skyrim side's wishes
local input = { seq = -1, forward = 0, right = 0, sprint = false, pitch = 0, stamp = 0 }
function PS_get_input() return input end   -- (dev: for snippets)
local lastAction = nil       -- highest action id already carried out (nil: adopt whatever is in the file at start)
local sprinting = false
local releaseBlockAt = nil
local home = nil
local leashReported = false

-- MORDHAU's own input functions, by the names the Skyrim side uses
local WEAPON_FOLDERS = { "Specific/OneHanded/", "Specific/TwoHandedSword/", "Specific/Polearms/", "Specific/TwoHandedBlunt/", "Specific/Makeshift/" }
function PS_take(p, name)
    local have = class_name(p.RightHandEquipment)
    if have ~= "" and not have:find("Fists", 1, true) then p:RequestDrop() end
    PS_pickLater = { class = "BP_" .. name, t = os.clock() + 0.6 }
end
function PS_spawn_weapon(pawn, bp)
    for _, folder in ipairs(WEAPON_FOLDERS) do
        local base = "/Game/Mordhau/Blueprints/Equipment/Weapons/" .. folder .. bp
        local cls = StaticFindObject(base .. "." .. bp .. "_C")
        if not there(cls) then pcall(LoadAsset, base); cls = StaticFindObject(base .. "." .. bp .. "_C") end
        if there(cls) then
            local l = pawn:K2_GetActorLocation()
            local w = pawn:GetWorld():SpawnActor(cls, { X = l.X, Y = l.Y, Z = l.Z + 60 }, { Pitch = 0, Yaw = 0, Roll = 0 })
            PS_finish_weapon(w)
            return w
        end
    end
    return nil
end

function PS_open_menu(class)
    if not there(PS_voiceMenu) then PS_voiceMenu = FindFirstOf("BP_VoiceMenu_C") end
    if not there(PS_emoteMenu) then PS_emoteMenu = FindFirstOf("BP_EmoteMenu_C") end
    local mine = class == "BP_VoiceMenu_C" and PS_voiceMenu or PS_emoteMenu
    local other = class == "BP_VoiceMenu_C" and PS_emoteMenu or PS_voiceMenu
    if there(other) and other:GetVisibility() == 3 then other:Hide() end
    if there(mine) then mine:Show() end
end
-- is one of MORDHAU's two menus open right now
function PS_menu_open()
    local open = false
    pcall(function()
        open = (there(PS_voiceMenu) and PS_voiceMenu:GetVisibility() == 3) or (there(PS_emoteMenu) and PS_emoteMenu:GetVisibility() == 3) or false
    end)
    return open
end

local ACTIONS = {
    Strike = function(p) p:RequestStrike360() end,   -- side and angle from the mouse movement MORDHAU was fed (its own 240 system)
    Stab = function(p) p:RequestStab360() end,
    RightStrike = function(p) p:RequestRightStrike() end,
    LeftStrike = function(p) p:RequestLeftStrike() end,
    RightUpperStrike = function(p) p:RequestRightUpperStrike() end,
    LeftUpperStrike = function(p) p:RequestLeftUpperStrike() end,
    RightLowerStrike = function(p) p:RequestRightLowerStrike() end,
    LeftLowerStrike = function(p) p:RequestLeftLowerStrike() end,
    RightStab = function(p) p:RequestRightStab() end,
    LeftStab = function(p) p:RequestLeftStab() end,
    Parry = function(p) p:FeintOrBlockPressed(); releaseBlockAt = os.clock() + 0.12 end,  -- MORDHAU's own key: parry, or feint during a windup
    Feint = function(p) p:RequestFeint() end,
    -- MORDHAU's own dismemberment sound (a head crushed or cut off: the head itself is Skyrim's, the sound MORDHAU's)
    GoreSound = function(p)
        local gs = StaticFindObject("/Script/Engine.Default__GameplayStatics")
        local path = "/Game/Mordhau/Audio/Cues/Foley/SC_DismembermentGore"
        local cue = StaticFindObject(path .. ".SC_DismembermentGore")
        if not there(cue) then pcall(LoadAsset, path); cue = StaticFindObject(path .. ".SC_DismembermentGore") end
        if there(gs) and there(cue) then gs:PlaySound2D(p, cue, 1.0, 1.0, 0.0, nil, nil, true) else log("gore sound: MORDHAU's SC_DismembermentGore was not found") end
    end,
    -- MORDHAU's own hit sound, for a hit that Skyrim judged (a creature)
    HitSound = function(p)
        local gs = StaticFindObject("/Script/Engine.Default__GameplayStatics")
        local cue = StaticFindObject("/Game/Mordhau/Audio/Cues/UI/SC_HitIndicator.SC_HitIndicator")
        if there(gs) and there(cue) then gs:PlaySound2D(p, cue, 1.0, 1.0, 0.0, nil, nil, true) end
    end,
    -- a shield: the block is held for as long as Skyrim's block button is (no timed release)
    -- MORDHAU's held shield block is its "shield wall": the shield's alternate mode (MORDHAU's R with a shield on the
    -- arm) raises it and it stays up (the parry motion stays in its blocking stage); pressing block lowers it. Found
    -- by sampling the motion: a plain block press with a shield is only the timed parry (0.38 s, then idle
    -- however long the button is held - 80% of swings hit through a "held" block).
    BlockDown = function(p) releaseBlockAt = nil; PS_blockHeld = true; PS_wall_up(p) end,
    BlockUp = function(p) PS_blockHeld = false; PS_wall_down(p) end,
    FirePress = function(p) p:FirePressed() end,     -- a bow: draw (held) ...
    FireRelease = function(p) p:FireReleased() end,  -- ... and loose
    Kick = function(p) p:RequestKick() end,
    -- Skyrim's weapon is put away: MORDHAU's own Holster (its 0 key): the weapon goes to the belt, the arms hang by
    -- the side. (1 = the argument that does it, found by trying; SwitchToFists raised the fists instead.)
    Sheathe = function(p)
        PS_sheatheAsks = (os.clock() - (PS_sheatheLast or -100) < 6) and (PS_sheatheAsks or 0) + 1 or 1
        PS_sheatheLast = os.clock()
        if PS_sheatheAsks >= 2 then p:SwitchToFists() else p:RequestHolster(1) end   -- (a weapon in no slot cannot be holstered)
    end,
    Draw = function(p) p:EquipSlot1() end,           -- and its own draw
    -- (the key is let go a moment later: pressed and released within one frame, MORDHAU never saw it held and
    -- did not jump - the Skyrim body rose with no jump in it and seemed to float up)
    Jump = function(p) p:JumpPressed(); PS_jumpUp = os.clock() + 0.18 end,
    -- MORDHAU's own menus (C / X): Show opens, Show again turns the page
    VoiceMenu = function(p) PS_open_menu("BP_VoiceMenu_C") end,
    EmoteMenu = function(p) PS_open_menu("BP_EmoteMenu_C") end,
    -- the blade struck Skyrim's world (a wall, a rock, the ground): the attack ends there, as MORDHAU ends one on its
    -- own walls. (The attack motion's own OnEnded: the swing stops and can no longer hit. MORDHAU's recoil animation
    -- could not be started from here - AssignNetMotionSimple and OnBlockedMelee refuse every argument tried.)
    WorldHit = function(p)
        local m = p:GetMotion()
        local name = class_name(m)
        if motion_stage(m) == 1 and (name:find("Strike") or name:find("Stab")) then m:OnEnded() end
    end,
    -- the kick reached somebody who has no stand-in here (a wolf, a spider): for MORDHAU it met nothing and would run
    -- out as a missed kick. It is ended there, so the next attack can be started at once.
    KickLanded = function(p)
        local m = p:GetMotion()
        local name = class_name(m)
        if name:find("Kick") then m:OnEnded(); log("kick landed on an enemy of Skyrim's alone: " .. name .. " ended") end
    end,
    -- a knockdown the player is spared (sheets/builds.json creature_knockdown_every): MORDHAU's character is stood up again
    StandUp = function(p)
        local ok = pcall(function() p:SetIsRagdollFalling(false) end)
        log("fall: the player's character is stood up again (" .. (ok and "done" or "refused") .. ")")
    end,
    Drop = function(p) p:RequestDrop() end,
    -- MORDHAU's throw of the held weapon (which of its functions does it is found by trying, once; logged)
    Throw = function(p)
        local tried = {}
        for _, f in ipairs({ "RequestThrow", "ThrowPressed", "RequestThrowWeapon", "Throw" }) do
            local ok = pcall(function() p[f](p) end)
            tried[#tried + 1] = f .. (ok and " ok" or " -")
            if ok then break end
        end
        if not PS_throwTold then PS_throwTold = true; log("throw: " .. table.concat(tried, ", ")) end
    end,                     -- MORDHAU's G
    WeaponMode = function(p) p:RequestToggleWeaponMode() end,   -- MORDHAU's R: the weapon's other grip / mode
    Battlecry = function(p) PS_pc:DoBattlecry() end,            -- MORDHAU's V
    -- pick the dropped weapon up again (MORDHAU's Use on it; Skyrim decides whether the player stands at it)
    PickUp = function(p) if there(PS_playerHeld) then p:PickUp(PS_playerHeld, false) end   -- (false: with true MORDHAU first holsters what is in the hand and crashes on empty hands)
    end,
}
-- "Voice7", "Emote12": a line of MORDHAU's voice menu / an emote of its emote menu, by MORDHAU's own number
local function numbered_action(p, name)
    local kind, n = name:match("^(%a+)(%d+)$")
    n = tonumber(n)
    if kind == "PickUp" then
        -- an enemy's dropped weapon (n = the Skyrim character it belonged to): what is in the hand is dropped first
        for _, b in ipairs(PS_get_bots()) do
            if b.ghost == n and b.weapon and there(b.weapon.held) and b.weapon.has == 2 then
                local have = class_name(p.RightHandEquipment)
                if have ~= "" and not have:find("Fists", 1, true) then p:RequestDrop() end
                PS_pickLater = { w = b.weapon.held, t = os.clock() + 0.6 }
                b.weapon.held = nil; b.weapon.has = 0
                return true
            end
        end
        return false
    elseif kind == "Choose" then
        -- a number key while MORDHAU's own voice / emote menu is open: its own Choose
        for _, m in ipairs({ PS_voiceMenu, PS_emoteMenu }) do
            if there(m) and m:GetVisibility() == 3 then m:Choose(n); return true end
        end
        return false
    elseif kind == "Voice" then
        pcall(function() p.NextAllowedVoiceCommandTime = 0.0 end)
        p:RequestVoiceCommand(n, false)
        return true
    elseif kind == "Emote" then
        p:RequestEmote(n)
        return true
    elseif kind == "Volume" then
        -- the settings menu's "MORDHAU volume", in percent
        PS_volume = math.min(n, 100) / 100
        PS_apply_volume(true)
        return true
    elseif kind == "VoiceSet" then
        -- the settings menu's voice choices: voice * 1000000 + pitch step (0..255 over 0.75 - 1.15) * 1000 + heard * 10 + enemies heard
        PS_voiceSet = { voice = n // 1000000, pitch = 0.75 + (n // 1000 % 1000) / 255 * 0.4, heard = n // 10 % 10 == 1 }
        PS_STANDIN_VOICES = n % 10 == 1
        PS_apply_voice(p, true)
        return true
    end
    return false
end

local function read_input()
    PS_inFile = PS_inFile or shared(IN, INPUT_BYTES)
    local f = PS_inFile
    if not f then return end
    f:seek("set", 0)
    local raw = f:read(INPUT_BYTES)
    if not raw or #raw < 12 then return end
    local head, length = string.unpack("<I4I4", raw)
    if length < 1 or length > INPUT_BYTES - 12 then return end
    if string.unpack("<I4", raw, 9 + length) ~= head then return end   -- (caught in the middle of being written)
    local line = raw:sub(9, 8 + length)
    local seq, fwd, right, sprint, pitch, mx, my, fp, rest = line:match("^(%-?%d+) (%S+) (%S+) (%d) (%S+) (%S+) (%S+) (%d)(.*)$")
    if not seq then return end
    seq = tonumber(seq)
    -- (the line found when the script starts was written by the LAST session, if any: nothing in it is asked of this
    -- one until Skyrim has written afresh. A troll from before a restart was spawned from it and roared on, unseen.)
    if PS_staleSeq == nil then PS_staleSeq = seq end
    if seq == PS_staleSeq then return end
    if seq ~= input.seq then input.stamp = os.clock() end
    input.seq, input.forward, input.right, input.sprint, input.pitch = seq, tonumber(fwd) or 0, tonumber(right) or 0, sprint == "1", tonumber(pitch) or 0
    input.mouseRight, input.mouseUp = tonumber(mx) or 0, tonumber(my) or 0
    input.firstPerson = fp == "1"
    input.health = tonumber(rest:match(" h:(%d+)")) or 100
    input.actions = {}
    input.playerWeapon = rest:match(" pw:([%w_/]+)")   -- the MORDHAU weapon for what the player has equipped in Skyrim
    do
        local a, b, c = rest:match(" wl:(%d+),(%d+),(%d+)")
        input.walls = a and { tonumber(a), tonumber(b), tonumber(c) } or nil
    end
    do
        local gn, gp = rest:match(" sg:(%d+),([%d%.]+)")
        if gn and tonumber(gn) ~= PS_staminaGains then
            local first = PS_staminaGains == nil
            PS_staminaGains = tonumber(gn)
            if not first then input.staminaGain = tonumber(gp) end   -- (what was in the file when the script started is old)
        end
        local level = tonumber(rest:match(" ai:(%d)"))
        if level and PS_ai_levels[level] and level ~= PS_ai_level then
            PS_ai_level = level
            for _, b in ipairs(PS_get_bots()) do b.skillFor = nil end   -- (every stand-in takes the new level)
            log("enemy skill level " .. level)
        end
    end
    PS_skillOf = {}
    PS_passive = {}
    PS_ghostShield = {}
    -- (Skyrim is paused - a menu: every stand-in is passive for as long, so none walks, strikes or parries)
    if rest:find(" pz:1", 1, true) then setmetatable(PS_passive, { __index = function() return true end }) end
    PS_refused = {}
    for id in rest:gmatch(" hr:(%d+)") do PS_refused[tonumber(id)] = true end
    -- (how big each fought character is against the player in Skyrim: sheets/builds.json standin_scale)
    PS_ghostSize = {}
    for id, size in rest:gmatch(" gz:(%d+),([%d%.]+)") do PS_ghostSize[tonumber(id)] = tonumber(size) end
    do
        local on, engage, snap, hold = rest:match(" hy:(%d),(%d+),(%d+),?(%d*)")
        PS_HYBRID = on == "1"; PS_ENGAGE = tonumber(engage) or 450; PS_SNAP = tonumber(snap) or 110; PS_HOLD = (tonumber(hold) or 500) / 1000
        local px, py, pf = rest:match(" pp:([%-%d%.]+),([%-%d%.]+),([%-%d%.]+)")
        input.pp = px and { x = tonumber(px), y = tonumber(py), fa = tonumber(pf) } or nil
    end
    PS_ghostTarget = {}
    for id, t in rest:gmatch(" gt:(%d+),(%d+)") do PS_ghostTarget[tonumber(id)] = tonumber(t) end
    for id, cls in rest:gmatch(" gs:(%d+),([%w_]+)") do PS_ghostShield[tonumber(id)] = cls end
    for id in rest:gmatch(" gp:(%d+)") do PS_passive[tonumber(id)] = true end
    -- (a creature's stand-in: sheets/builds.json creature_standin)
    PS_dummy = {}
    for id in rest:gmatch(" gd:(%d+)") do PS_dummy[tonumber(id)] = true end
    for id, n in rest:gmatch(" gb:(%d+),(%d+)") do PS_skillOf[tonumber(id)] = tonumber(n) / 10 end   -- (each stand-in's own skill, 1..5 in tenths: by the level gap)
    input.shield = rest:match(" sh:([%w_]+)")   -- the MORDHAU shield for the one on the arm in Skyrim (nil: none)
    input.crouch = rest:find(" cr:1", 1, true) ~= nil   -- sneaking in Skyrim = crouching in MORDHAU
    for id, name in rest:gmatch(" (%d+):(%w+)") do input.actions[#input.actions + 1] = { tonumber(id), name } end
    -- who is limp in Skyrim right now: " down:<player 0/1>,<id>,..."
    input.downPlayer, input.down = false, {}
    local dp, ids = rest:match(" down:(%d)([%d,]*)")
    if dp then
        input.downPlayer = dp == "1"
        for id in ids:gmatch("(%d+)") do input.down[tonumber(id)] = true end
    end
    input.weaponWish = rest:match(" w:([%w_/]+)")   -- a bow or crossbow is out in Skyrim: the MORDHAU item to hold
    input.ghosts = {}
    for id, r, f, u, yaw, attacks, blocking, vr, vf, weapon, hp in rest:gmatch("g:(%d+),([%-%d%.]+),([%-%d%.]+),([%-%d%.]+),([%-%d%.]+),(%d+),(%d),([%-%d%.]+),([%-%d%.]+),([%w_/%-]+),(%d+)") do
        input.ghosts[#input.ghosts + 1] = { id = tonumber(id), right = tonumber(r), forward = tonumber(f), up = tonumber(u), yaw = tonumber(yaw),
            attacks = tonumber(attacks), blocking = blocking == "1", vr = tonumber(vr) or 0, vf = tonumber(vf) or 0, weapon = weapon ~= "-" and weapon or nil, hp = tonumber(hp) or 100 }
    end
end

local lastMouseRight, lastMouseUp = nil, nil
local function apply_input(pawn, pc)
    read_input()
    if PS_jumpUp and os.clock() >= PS_jumpUp then PS_jumpUp = nil; pcall(function() pawn:JumpReleased() end) end
    -- mouse movement in Skyrim -> MORDHAU's own attack-angle input (what its mouse axes feed)
    if input.mouseRight then
        if lastMouseRight then
            local dx, dy = input.mouseRight - lastMouseRight, input.mouseUp - lastMouseUp
            if (dx ~= 0 or dy ~= 0) and math.abs(dx) < 5000 and math.abs(dy) < 5000 then
                -- MORDHAU's own axes: turn = mouse right positive, look-up axis = mouse DOWN positive (measured: a
                -- positive Y gives the lower strike, +0.80; the upper strike is -0.96)
                pc:RegisterAnglingXInput(dx * 0.05)
                pc:RegisterAnglingYInput(-dy * 0.05)
            end
        end
        lastMouseRight, lastMouseUp = input.mouseRight, input.mouseUp
    end
    local fresh = os.clock() - input.stamp < 0.5   -- Skyrim stopped writing: stand still
    local fwd, right = fresh and input.forward or 0, fresh and input.right or 0
    if fwd ~= 0 then pawn:MoveForward(fwd) end
    if right ~= 0 then pawn:MoveRight(right) end
    -- where the player looks up or down in Skyrim -> MORDHAU's own look value (its swings aim with it)
    if fresh then
        pcall(function()
            local target = math.max(-70, math.min(55, -input.pitch))
            local d = target - pawn:GetRawLookUpValue()
            if math.abs(d) > 0.3 then pawn:LookUpNotAbsolute(-d / 2.5) end   -- measured: one unit moves the value by -2.5
        end)
    end
    if PS_pickLater and os.clock() >= PS_pickLater.t then
        local w = PS_pickLater.w
        if PS_pickLater.class then w = PS_spawn_weapon(pawn, PS_pickLater.class) end
        PS_pickLater = nil
        if there(w) then pawn:PickUp(w, false); log("picked up " .. class_name(w)) end
    end
    if os.clock() - (PS_sweptAt or 0) > 5 then PS_sweptAt = os.clock(); pcall(PS_sweep_refs) end
    if input.staminaGain then
        local gain = input.staminaGain
        input.staminaGain = nil
        pcall(function()
            local before = pawn.Stamina
            pawn.Stamina = math.min(100, before + gain)
            log(string.format("stamina: Skyrim gave %.0f%%: %.0f -> %.0f", gain, before, pawn.Stamina))
        end)
    end
    pcall(PS_player_arm, pawn)
    pcall(PS_player_shield, pawn)
    pcall(PS_wall_keep, pawn)
    if os.clock() - input.stamp > 0.5 then PS_blockHeld = false end   -- (Skyrim stopped writing: nothing is held)
    local okWalls, wallErr = pcall(PS_place_walls, pawn)
    if not okWalls and not PS_wallErr then PS_wallErr = true; log("placing the wall pieces failed: " .. tostring(wallErr)) end
    local wantCrouch = fresh and input.crouch or false
    if wantCrouch ~= (crouching or false) then
        crouching = wantCrouch
        pcall(function() if crouching then pawn:StartCrouching() else pawn:StopCrouching() end end)
    end
    local wantSprint = fresh and input.sprint
    if wantSprint ~= sprinting then
        sprinting = wantSprint
        if sprinting then pawn:SprintPressed() else pawn:SprintReleased() end
    end
    if releaseBlockAt and os.clock() >= releaseBlockAt then releaseBlockAt = nil; pawn:FeintOrBlockReleased() end
    if PS_showcase then
        -- (dev, recordings: cache/showcase.txt - the repertoire. What is queued is done at its time; with nothing
        -- queued, nobody attacking and an opponent in reach, the next piece of the repertoire starts.)
        local now, mine = os.clock(), ""
        pcall(function() mine = class_name(pawn:GetMotion()) end)
        -- (the repertoire spends stamina faster than a fight does, and a parry on none drops the weapon: kept up)
        pcall(function() pawn.Stamina = 100 end)
        local busy = mine:find("Strike", 1, true) or mine:find("Stab", 1, true) or mine:find("Kick", 1, true) or mine:find("Parry", 1, true) or mine:find("Flinch", 1, true)
        if PS_showQueue then
            local q = PS_showQueue[1]
            if q and now >= q[1] then
                table.remove(PS_showQueue, 1)
                local hurt = false
                if q[4] then pcall(function() hurt = pawn.Health < (PS_riposteHealth or 0) - 0.5 end) end
                if hurt or mine:find("Flinch", 1, true) then
                    PS_showQueue = nil
                else
                    -- (side and height go in the way MORDHAU's mouse gives them - the named requests come out
                    -- right-handed whatever is asked: the controller's angle input, then its own 360 request)
                    local name = q[2]
                    if name:find("Strike", 1, true) or name:find("Stab", 1, true) then
                        local sx = name:find("Left", 1, true) and -1 or 1
                        local sy = name:find("Upper", 1, true) and -1 or (name:find("Lower", 1, true) and 1 or 0)   -- (MORDHAU's Y: positive = the lower strike)
                        pcall(function()
                            local ctrl = pawn.Controller
                            for _ = 1, 4 do ctrl:RegisterAnglingXInput(sx * 8); ctrl:RegisterAnglingYInput(sy * 8) end
                        end)
                        if name:find("Stab", 1, true) then pcall(function() pawn:RequestStab360() end) else pcall(function() pawn:RequestStrike360() end) end
                    else
                        pcall(ACTIONS[name], pawn)
                    end
                    PS_showLast = now
                    if q[3] then log("showcase: " .. q[3]) end
                end
            end
            if PS_showQueue and #PS_showQueue == 0 then PS_showQueue = nil end
        elseif not busy and PS_showNear and PS_showNear < 270 ^ 2 and now - (PS_showLast or 0) > 1.0 and now - (PS_showThreat or 0) > 0.3 then
            local pieces = {
                { { 0, "RightStrike", "right strike" } },
                { { 0, "LeftUpperStrike", "left upper strike" } },
                { { 0, "RightStab", "right stab" } },
                { { 0, "RightStrike", "feint: right strike ..." }, { 0.22, "Feint" }, { 0.50, "LeftStrike", "... into a left strike" } },
                { { 0, "LeftLowerStrike", "left lower strike" } },
                { { 0, "RightUpperStrike", "morph: right upper strike ..." }, { 0.20, "RightStab", "... into a stab" } },
                { { 0, "Kick", "kick" } },
                { { 0, "LeftStrike", "left strike" } },
                { { 0, "RightLowerStrike", "right lower strike" } },
                { { 0, "LeftStab", "morph: left stab ..." }, { 0.20, "LeftStrike", "... into a strike" } },
                { { 0, "RightUpperStrike", "right upper strike" } },
                { { 0, "LeftStrike", "combo: left strike ..." }, { 0.55, "RightStrike", "... right strike" } },
                { { 0, "LeftStab", "left stab" } },
                { { 0, "LeftUpperStrike", "feint: left upper strike ..." }, { 0.22, "Feint" }, { 0.50, "RightStab", "... into a stab" } },
            }
            PS_showPiece = (PS_showPiece or 0) % #pieces + 1
            if pieces[PS_showPiece][1][2] == "Kick" and PS_showNear > 190 ^ 2 then PS_showPiece = PS_showPiece % #pieces + 1 end   -- (a kick only when close)
            PS_showQueue = {}
            for _, step in ipairs(pieces[PS_showPiece]) do PS_showQueue[#PS_showQueue + 1] = { now + step[1], step[2], step[3] } end
        end
        PS_showNear = nil
    end
    if PS_riposteFrom and os.clock() >= PS_riposteFrom then
        -- (dev, recordings: the answer to a parry that held - asked until MORDHAU's character strikes, or the time is up)
        local hurt, mine = false, ""
        pcall(function() hurt = pawn.Health < (PS_riposteHealth or 0) - 0.5 end)
        pcall(function() mine = class_name(pawn:GetMotion()) end)
        if hurt or os.clock() > PS_riposteUntil or mine:find("Flinch", 1, true) then
            PS_riposteFrom = nil
        elseif mine:find("Strike", 1, true) or mine:find("Stab", 1, true) then
            PS_riposteFrom = nil
            PS_ripostes = (PS_ripostes or 0) + 1
            log("autoparry: riposte (" .. PS_ripostes .. " so far)")
        else
            pcall(function() pawn:RequestRightStrike() end)
        end
    end
    if input.actions then
        if lastAction == nil then
            lastAction = 0
            -- (settings are not one-off moves: the latest of each still counts. The plugin sends them as the link comes up,
            -- often before this runs - seen 2026-10-18: no Volume, so MORDHAU's footsteps and music were heard)
            local settings = {}
            for _, a in ipairs(input.actions) do
                lastAction = math.max(lastAction, a[1])
                local kind = a[2]:match("^(%a+)%d+$")
                if (kind == "Volume" or kind == "VoiceSet") and (not settings[kind] or settings[kind][1] < a[1]) then settings[kind] = a end
            end
            for _, a in pairs(settings) do
                local ok, done = pcall(numbered_action, pawn, a[2])
                log("action " .. a[1] .. " " .. a[2] .. " (sent before the script listened)" .. ((ok and done) and "" or (" FAILED: " .. tostring(done))))
            end
        end
        table.sort(input.actions, function(a, b) return a[1] < b[1] end)
        for _, a in ipairs(input.actions) do
            if a[1] > lastAction then
                lastAction = a[1]
                local fn = ACTIONS[a[2]]
                if not fn and a[2]:match("^Take%w+$") then
                    local ok, err = pcall(PS_take, pawn, a[2]:sub(5))
                    log("action " .. a[1] .. " " .. a[2] .. (ok and "" or (" FAILED: " .. tostring(err))))
                elseif not fn and a[2]:match("^%a+%d+$") then
                    local ok, done = pcall(numbered_action, pawn, a[2])
                    log("action " .. a[1] .. " " .. a[2] .. ((ok and done) and "" or (" FAILED: " .. tostring(done))))
                elseif fn then
                    local ok, err = pcall(fn, pawn)
                    log("action " .. a[1] .. " " .. a[2] .. (ok and "" or (" FAILED: " .. tostring(err))))
                else
                    log("unknown action " .. a[2])
                end
            end
        end
    end
end

-- a world-space point in the character mesh's own space
local function to_local(T, v)
    local q, t = T.Rotation, T.Translation
    local x, y, z = v.X - t.X, v.Y - t.Y, v.Z - t.Z
    local qx, qy, qz, qw = -q.X, -q.Y, -q.Z, q.W
    local tx, ty, tz = 2 * (qy * z - qz * y), 2 * (qz * x - qx * z), 2 * (qx * y - qy * x)
    return x + qw * tx + (qy * tz - qz * ty), y + qw * ty + (qz * tx - qx * tz), z + qw * tz + (qx * ty - qy * tx)
end

-- ---------------------------------------------------------------------------------------------------------------
-- Stand-ins for Skyrim's characters
local MAX_GHOSTS = 5   -- (the plugin's Link::kGhosts; the first three's states sit where they always did, the rest after the bow's pose)
-- Nobody may die inside MORDHAU (a dead character is destroyed, and its stand-in with it): the match's damage is
-- scaled down so even a 100-damage hit leaves health, health is put back each frame, and what we report to Skyrim is
-- scaled back up to MORDHAU's real number.
-- Health is ONE pool, Skyrim's: MORDHAU's characters are kept at the same percentage as their Skyrim selves (so its
-- low-health behaviour and effects fit), but never under HEALTH_FLOOR, so that no single hit can kill inside MORDHAU.
local HEAD_REACH = 30        -- cm: a blade's hit line this near the head bone counts as a blow to the head (sheets/gore.json)
local HEAD_LINE = 5          -- cm: the blade meets the body no lower than this under the head bone = a blow to the head (sheets/gore.json head_rule)
local HEAD_SPINE = 20        -- cm: ... and the blade passes this near the upright through the head bone - through the neck or head, not past its side
local HEAD_ANY = 30          -- cm: ... a headshot 'in general' (what crushes a head with a blunt weapon; sheets/gore.json head_any_cm)
local HEAD_TOP = 35          -- cm: ... and no higher than this over the head bone (sheets/gore.json head_top_cm_above_head_bone)
-- enemy skill levels (sheets/enemy_ai.json skill_levels): parry chance, well-timed parry, feint, morph, hesitance (s)
PS_ai_levels = {
    { 0.15, 0.10, 0.00, 0.00, 0.9 },
    { 0.35, 0.30, 0.05, 0.00, 0.6 },
    { 0.55, 0.55, 0.20, 0.15, 0.4 },
    { 0.75, 0.75, 0.35, 0.30, 0.25 },
    { 0.90, 0.90, 0.50, 0.45, 0.1 },
}
PS_ai_level = PS_ai_level or 3
-- one stand-in's skill (1..5, in tenths, sent by the plugin by the level gap; else the chosen level), and the values for
-- it: between two of the five levels, blended
function PS_skill_of(b) return PS_skillOf and b and PS_skillOf[b.ghost] or PS_ai_level end
function PS_ai_row(level)
    level = math.max(1, math.min(5, level or PS_ai_level))
    local lo = math.floor(level); local hi = math.min(5, lo + 1); local t = level - lo
    local row = {}
    for i = 1, 5 do row[i] = PS_ai_levels[lo][i] * (1 - t) + PS_ai_levels[hi][i] * t end
    return row
end
local HEALTH_FLOOR = 30
local DAMAGE_FACTOR = 0.15
-- On unless cache/live_options.lua says otherwise (the launcher writes it; -SimpleEnemies turns it off). Tested on
-- for 12 minutes and off for 25 minutes of continuous fighting.
if BOT_BRAINS == nil then
    BOT_BRAINS = true
    local ok, options = pcall(dofile, (CACHE .. "live_options.lua"))
    if ok and type(options) == "table" and options.bot_brains ~= nil then BOT_BRAINS = options.bot_brains and true or false end
end
local bots = {}
function PS_get_bots() return bots end   -- (the list is replaced when it is rebuilt: always ask)            -- { obj, prepared, ghost (id or nil), health, hits, damage, attacks, parriedFor }
local lastBotScan, lastBotAsk = -10, -100
local playerHits, playerDamage, playerAttacker = 0, 0, 0
local playerHealth = nil
local STRIKES = { "RequestRightStrike", "RequestLeftStrike", "RequestRightUpperStrike", "RequestLeftUpperStrike", "RequestRightStab" }

local function scan_bots(pc)
    lastBotScan = os.clock()
    local seen = {}
    for _, b in ipairs(bots) do if not b.dead and b.obj:IsValid() then seen[b.obj:GetFullName()] = b end end
    local list = {}
    local aiCharacters = 0
    for _, c in ipairs(FindAllOf("MordhauCharacter") or {}) do
        -- (a Demon Horde creature spawned for Skyrim is a stand-in too, but only ever for its own Skyrim body)
        local demon = class_name(c):match("DemonHordeEnemy_Special_(%a+)")
        if c:IsValid() and not demon and class_name(c.Controller):find("AIController", 1, true) then aiCharacters = aiCharacters + 1 end
        -- (dev: a creature spawned by hand to be looked at - PS_shownByHand[full name] - is left to itself)
        if c:IsValid() and not c.bIsDead and (c.Health or 0) > 0 and not (PS_shownByHand and PS_shownByHand[c:GetFullName()]) then
            local ctrl = c.Controller
            if ctrl and ctrl:IsValid() and class_name(ctrl):find("AIController", 1, true) then
                list[#list + 1] = seen[c:GetFullName()] or { obj = c, prepared = false, health = 100, hits = 0, damage = 0, demon = demon }
            end
        end
    end
    bots = list
    -- (a dead one comes back by itself: MORDHAU respawns its bots; only ask when there really are too few)
    if aiCharacters < MAX_GHOSTS and os.clock() - lastBotAsk > 30 then
        lastBotAsk = os.clock()
        pcall(function() pc:AddBots(MAX_GHOSTS - aiCharacters) end)
        log("asked MORDHAU for " .. (MAX_GHOSTS - aiCharacters) .. " stand-in(s)")
    end
end

local LOOKS = nil
local function looks()
    if not LOOKS then
        local ok, cfg = pcall(dofile, CONFIG)
        LOOKS = ok and cfg and cfg.enemies or {}
    end
    return LOOKS
end

local function prepare(b)
    b.prepared = true
    -- BOT_BRAINS: MORDHAU's own bot AI keeps deciding when to attack, parry, feint and riposte (it cannot walk: we hold
    -- it where its Skyrim character stands). Off: it only attacks when the Skyrim character does.
    -- (do NOT change a stand-in's Team: MORDHAU kills and respawns a character whose team changes)
    b.since = os.clock()
    -- MORDHAU kills whoever stays outside its arena for a few seconds: stand-ins are exempt (they stand wherever the
    -- Skyrim character is), and an unused one waits where MORDHAU itself spawned it
    pcall(function() b.obj.bIsAllowedOutOfBounds = true end)
    pcall(function() local l = b.obj:K2_GetActorLocation(); b.park = { X = l.X, Y = l.Y, Z = l.Z } end)
    -- (never next to the player: a bot taken on while it stood there - the script loaded anew in the middle of a fight -
    -- was parked at the player's side, unseen and in the way of every swing, as if an unseen person were being hit)
    pcall(function()
        local p = PAWN:K2_GetActorLocation()
        if b.park and (b.park.X - p.X) ^ 2 + (b.park.Y - p.Y) ^ 2 < 1500 * 1500 then
            PS_parkedFar = (PS_parkedFar or 0) + 1
            b.park = { X = p.X + 2200 + PS_parkedFar * 150, Y = p.Y + 2200, Z = p.Z + 30 }
        end
    end)
    pcall(function()
        local ctrl = b.obj.Controller
        if there(ctrl) then
            -- its own brain sleeps at least until it holds its look's weapon (see wake below)
            if there(ctrl.BrainComponent) then ctrl.BrainComponent:StopLogic("stand-in") end
            ctrl:StopMovement()
        end
        if there(b.obj.CharacterMovement) then b.obj.CharacterMovement:DisableMovement() end
    end)   -- held in place by us; free movement breaks hit detection
    -- which look it is drawn with in Skyrim: the lowest one no other stand-in has
    local used = {}
    for _, o in ipairs(bots) do if o ~= b and o.look then used[o.look] = true end end
    -- (one that already carries a look's weapon, from before this script was reloaded, keeps that look)
    pcall(function()
        local held = class_name(b.obj.RightHandEquipment)
        for k, look in ipairs(looks()) do
            if not used[k] and look.weapon:match("%.([%w_]+)$") == held then b.look = k end
        end
    end)
    if not b.look then for k = 1, math.max(#looks(), 1) do if not used[k] then b.look = k; break end end end
    b.look = b.look or 1
    b.parts = {}
    for i = 1, #names do b.parts[i] = ZERO_BONE end
end

-- With BOT_BRAINS the stand-in's own MORDHAU brain is started once its weapon is settled (armed, or given up on).
local function wake(b)
    -- (an archer's stand-in only stands there to be hit: its brain stays off)
    -- (An archer's stand-in - passive - keeps its brain: with the brain stopped the player's blade went through it
    -- without counting, twice seen in play (attacks passed through; archers could not be hit). Its attacks are
    -- broken off instead: see where the stand-ins are placed.)
    if b.awake or not BOT_BRAINS then return end
    b.awake = true
    pcall(function()
        local ctrl = b.obj.Controller
        if there(ctrl) and there(ctrl.BrainComponent) then ctrl.BrainComponent:RestartLogic() end
    end)
end

-- The MORDHAU weapon a stand-in fights with follows what its Skyrim character holds (one-handed sword -> arming
-- sword, battleaxe -> battle axe ...: sheets/weapon_families.json), so its moves and reach fit; Skyrim draws the
-- character's own Skyrim weapon in its hand. The stand-in drops what it holds, the weapon is spawned in MORDHAU once
-- and handed over, then it is asked to bring out slot 1, 2, 3 in turn until that weapon is in its hand. The equipment
-- list itself is never walked: it can hold weapons MORDHAU has already destroyed, and touching those crashes the
-- loader. After a few tries it gives up and keeps what it has.
local WEAPONS = "/Game/Mordhau/Blueprints/Equipment/Weapons/"
-- A weapon made here has no parts yet - no blade, no model, so no line for MORDHAU to hit with (its hit trace was
-- a single point at the grip: swings went through people). MORDHAU's own call puts the default parts on it.
function PS_finish_weapon(w)
    if not there(w) then return end
    -- (no Colors / Parts lists: newer UE4SS sizes a Lua list inside a struct by the call's first argument instead of the
    -- list - LuaUObject.cpp push_arrayproperty, lua_rawlen(L, 1) - and crashes MORDHAU on it. Left out, they stay empty.)
    local ok = pcall(function() w:AssignCustomization({ ID = 0, Pattern = 0, Skin = 0 }, 0, 0, 0) end)
    if not ok and not PS_finishWarned then PS_finishWarned = true; log("a spawned weapon could not be given its parts (its swings will not reach)") end
end

-- A fought character with a shield in Skyrim (sheets/live_link.json standin_shield): its stand-in is handed MORDHAU's
-- shield for it once its weapon is in the hand, and brings it onto the arm - so it blocks with the shield.
function PS_standin_shield(b, c)
    local want = b.ghost and PS_ghostShield and PS_ghostShield[b.ghost] or nil
    if not want or not b.armed then return end
    if os.clock() - (b.shieldTry or -10) < 1.2 then return end
    local l = c.LeftHandEquipment
    local have = there(l) and class_name(l) or ""
    if have == want .. "_C" then
        if not b.shieldTold then b.shieldTold = true; log("stand-in " .. tostring(b.look) .. " carries " .. want) end
        return
    end
    if class_name(c:GetMotion()) ~= "IdleMotion" then return end
    b.shieldTry = os.clock()
    b.shieldStep = (b.shieldStep or 0) + 1
    if b.shieldStep > 6 then if b.shieldStep == 7 then log("stand-in " .. tostring(b.look) .. " could not be handed " .. want) end return end
    if b.shieldGiven == want then pcall(function() c:NextShieldEquipment() end) return end   -- (in its slot: brought onto the arm)
    local full = "/Game/Mordhau/Blueprints/Equipment/Shields/" .. want .. "." .. want .. "_C"
    local cls = StaticFindObject(full)
    if not there(cls) then pcall(LoadAsset, "/Game/Mordhau/Blueprints/Equipment/Shields/" .. want); cls = StaticFindObject(full) end
    if not there(cls) then log("stand-in shield: MORDHAU has no " .. full); b.shieldStep = 7 return end
    local at = c:K2_GetActorLocation()
    local w = c:GetWorld():SpawnActor(cls, { X = at.X, Y = at.Y, Z = at.Z + 60 }, { Pitch = 0, Yaw = 0, Roll = 0 })
    PS_finish_weapon(w)
    if not there(w) then return end
    local slot = nil
    pcall(function()
        local e = c.Equipment
        local held = there(c.RightHandEquipment) and c.RightHandEquipment:GetFullName() or ""
        for i = 1, math.min(#e, 3) do if not slot and not there(e[i]) then slot = i - 1 end end
        for i = math.min(#e, 3), 1, -1 do
            if not slot and there(e[i]) and e[i]:GetFullName() ~= held then e[i]:K2_DestroyActor(); slot = i - 1 end
        end
    end)
    local ok = pcall(function() c:PickUpToSlot(w, slot or 2) end)
    if ok then b.shieldGiven = want end
    log("stand-in " .. tostring(b.look) .. ": " .. want .. " handed over (slot " .. tostring(slot or 2) .. ")")
end
-- Where the stand-ins stand (sheets/live_link.json standin_move): 20 bytes each, five of them.
PS_why = PS_why or {}   -- (how often a stand-in is set somewhere, by reason: for looking into it)
function PS_move_block()
    -- (the stand-ins are gone over less often than this is written: the last round's is kept for up to 0.3 s)
    if PS_moveOut then
        local out = {}
        for i = 1, MAX_GHOSTS do out[i] = PS_moveOut[i] or string.pack("<I4fffi4", 0, 0, 0, 0, 0) end
        PS_moveOut = nil
        PS_moveLast = table.concat(out); PS_moveLastAt = os.clock()
    elseif os.clock() - (PS_moveLastAt or -10) > 0.3 then
        PS_moveLast = nil
    end
    return PS_moveLast or string.rep(string.pack("<I4fffi4", 0, 0, 0, 0, 0), MAX_GHOSTS)
end
-- Stand-ins are mute (sheets/builds.json standin_voices = 0; the settings menu's "Enemies' MORDHAU voices"): Skyrim's
-- people speak only as Skyrim has them speak. MORDHAU's own switch for a muffled voice is used, with the muffled volume
-- at nothing; set again every 2 s, since MORDHAU sets the switch itself from what the character wears. Switched on in
-- the menu, a muted stand-in gets MORDHAU's own muffled volume back.
PS_STANDIN_VOICES = false
function PS_mute_voice(b, c)
    if os.clock() - (b.mutedAt or -10) < 2.0 then return end
    b.mutedAt = os.clock()
    if PS_STANDIN_VOICES then
        if b.muted then
            b.muted = false
            pcall(function() local vc = c.CharacterVoiceComponent; vc.MuffledVoiceVolumeMultiplier = b.muffledWas or 1.5; vc.bIsVoiceMuffled = false end)
        end
        return
    end
    local ok = pcall(function()
        local vc = c.CharacterVoiceComponent
        if not b.muffledWas then b.muffledWas = vc.MuffledVoiceVolumeMultiplier end
        vc.MuffledVoiceVolumeMultiplier = 0.0
        vc.bIsVoiceMuffled = true
    end)
    b.muted = ok
    if not b.muteTold then b.muteTold = true; log("stand-in " .. tostring(b.look) .. (ok and " is mute (no MORDHAU voice)" or ": its voice could NOT be muted")) end
end
-- MORDHAU's arena is not Skyrim's world: its walls, pillars and props stand where they stand in MORDHAU, so an
-- enemy's swing 4-6 m from the player met one of them and was thrown back (BlockedMotion) at nothing to be seen
-- (sheets/builds.json mordhau_world_hits = 0: "npcs are hitting invisible objects", 2026-10-15; 21 such swings in one
-- test). Every attack - the player's and the stand-ins' - gets MORDHAU's own bDisableWorldCollision as it begins.
-- (Skyrim's walls stopping a blade is the plugin's, sheets/builds.json wall_cut - off for now.)
function PS_no_world_hits(pawn)
    local function off(ch)
        pcall(function()
            local m = ch:GetMotion()
            if there(m) and m.bDisableWorldCollision == false then m.bDisableWorldCollision = true end   -- (only attack motions have it)
        end)
    end
    off(pawn)
    for _, b in ipairs(PS_get_bots()) do
        if not b.dead and there(b.obj) then off(b.obj) end
    end
end
-- MORDHAU's sound classes kept silent in the mod's session (see PS_apply_volume)
PS_SILENT_CLASSES = {
    "Classes/MusicSoundClass.MusicSoundClass", "Classes/Music.Music",
    "Classes/Ambient/AmbientSoundClass.AmbientSoundClass", "Classes/Ambient/ThePitAmbient.ThePitAmbient",
    -- (footsteps: Skyrim plays its own for every character; MORDHAU's character also walks on the spot by design)
    "Classes/FoleyClasses/FootstepsSoundClass.FootstepsSoundClass", "Classes/PreMixClasses/PreFootsteps.PreFootsteps",
}
-- The settings menu's "MORDHAU volume": MORDHAU's master sound class turned down through its own master sound mix, in
-- this session only (GameplayStatics SetSoundMixClassOverride - MORDHAU's saved sound settings are never touched). Put
-- on again when MORDHAU has a new world (the override belongs to one).
function PS_apply_volume(now)
    if not PS_volume then return end
    local ok, err = pcall(function()
        local w = FindFirstOf("PlayerController"):GetWorld()
        if not now and PS_volumeWorld == w:GetFullName() then return end
        local base = "/Game/Mordhau/Audio/Other/"
        local cls = StaticFindObject(base .. "Classes/GlobalMasterSoundClass.GlobalMasterSoundClass")
        local mix = StaticFindObject(base .. "Mix/GlobalMasterSoundMix.GlobalMasterSoundMix")
        if not (there(cls) and there(mix)) then
            LoadAsset(base .. "Classes/GlobalMasterSoundClass.GlobalMasterSoundClass"); LoadAsset(base .. "Mix/GlobalMasterSoundMix.GlobalMasterSoundMix")
            cls = StaticFindObject(base .. "Classes/GlobalMasterSoundClass.GlobalMasterSoundClass"); mix = StaticFindObject(base .. "Mix/GlobalMasterSoundMix.GlobalMasterSoundMix")
        end
        local gs = StaticFindObject("/Script/Engine.Default__GameplayStatics")
        gs:SetSoundMixClassOverride(w, mix, cls, PS_volume, 1.0, 0.0, true)
        -- MORDHAU's music, its arena's ambience (the wind of FFA_ThePit) and its footsteps are silenced, with their child
        -- classes: beside Skyrim only MORDHAU's fighting and voices are wanted
        for _, path in ipairs(PS_SILENT_CLASSES) do
            local quiet = StaticFindObject(base .. path)
            if not there(quiet) then LoadAsset(base .. path); quiet = StaticFindObject(base .. path) end
            if there(quiet) then gs:SetSoundMixClassOverride(w, mix, quiet, 0.0, 1.0, 0.0, true) end
        end
        if PS_volumeWorld ~= w:GetFullName() then gs:PushSoundMixModifier(w, mix) end
        PS_volumeWorld = w:GetFullName()
    end)
    if now or not ok then log(string.format("volume: MORDHAU at %.0f%%", PS_volume * 100) .. (ok and "" or " - FAILED: " .. tostring(err))) end
end
-- The player's voice (sheets/settings_menu.json, page "Character voice", and "Your yells and grunts"): which of
-- MORDHAU's voices, at which pitch (kept within the voice's own limits), and whether it is heard. Sent by the plugin as
-- "VoiceSet<n>" (see numbered_action); until it comes, the mercenary's own voice and pitch. Put on the player's character again
-- every 2 s, since MORDHAU sets them back from the mercenary when it gives the player a new character.
-- (sheets/settings_menu.json "voice" voice_assets, in that order - the menu lists them by MORDHAU's own names)
local PS_VOICES = { "Barbarian", "Englishman", "CruelKnight", "FemaleCruel", "Raziel", "MiddleEastern", "FemaleFrench", "FemaleGerman", "FemaleItalian", "FemaleSlavic", "Reginald", "French", "German", "Irish", "Knight", "FemaleNoble", "Nordic", "FemaleNorthern", "Bernard", "Scot", "Slavic", "FemaleYoung", "Spanish", "YoungLad" }
PS_voiceSet = PS_voiceSet or { voice = 0, pitch = 1.0, heard = true }
function PS_apply_voice(p, now)
    if not now and os.clock() - (PS_voiceAt or -10) < 2.0 then return end
    PS_voiceAt = os.clock()
    local set = PS_voiceSet
    local ok, err = pcall(function()
        local vc = p.CharacterVoiceComponent
        if not PS_ownVoice then PS_ownVoice = { voice = vc.CharacterVoice, pitch = vc.VoicePitch, muffled = vc.MuffledVoiceVolumeMultiplier } end
        local name = PS_VOICES[set.voice]
        local voice = PS_ownVoice.voice
        if name then
            local path = "/Game/Mordhau/Blueprints/Voices/BP_" .. name
            voice = StaticFindObject(path .. ".Default__BP_" .. name .. "_C")
            if not there(voice) then LoadAsset(path .. ".BP_" .. name .. "_C"); voice = StaticFindObject(path .. ".Default__BP_" .. name .. "_C") end
        end
        if there(voice) and vc.CharacterVoice ~= voice then vc.CharacterVoice = voice end
        local pitch = PS_ownVoice.pitch
        if name then
            pitch = set.pitch
            pcall(function() local l = voice.PitchLimits; pitch = math.max(l.X, math.min(l.Y, pitch)) end)   -- (each voice's own range in MORDHAU)
        end
        vc.VoicePitch = pitch
        if not set.heard then
            vc.MuffledVoiceVolumeMultiplier = 0.0; vc.bIsVoiceMuffled = true
        elseif vc.MuffledVoiceVolumeMultiplier == 0.0 then
            vc.MuffledVoiceVolumeMultiplier = PS_ownVoice.muffled; vc.bIsVoiceMuffled = false
        end
    end)
    if now then log("voice: " .. (PS_VOICES[set.voice] or "the mercenary's own") .. string.format(", pitch %.2f", set.pitch) .. (set.heard and "" or ", not heard") .. (ok and "" or " - FAILED: " .. tostring(err))) end
end
-- MORDHAU can hand out a bot with a Demon Horde voice: a stand-in for a Skyrim person gets a human one (the player's)
function PS_human_voice(c, pawn)
    pcall(function()
        local vc, mine = c.CharacterVoiceComponent, pawn.CharacterVoiceComponent
        if not (there(vc) and there(mine) and there(vc.CharacterVoice) and there(mine.CharacterVoice)) then return end
        local name = vc.CharacterVoice:GetFullName()
        if name:find("Demon", 1, true) or name:find("Horde", 1, true) or name:find("Minotaur", 1, true) or name:find("Ogre", 1, true) then
            vc.CharacterVoice = mine.CharacterVoice
            log("a stand-in had the voice " .. name:match("[^%.]+$") .. ": it speaks with a human voice now")
        end
    end)
end

-- Skyrim's walls: three unseen wall pieces of MORDHAU's own arena, held where Skyrim reports something solid
-- (" wl:left,ahead,right" in cm). MORDHAU's blade then stops on them by MORDHAU's own rules.
PS_walls = PS_walls or {}
local WALL_ANGLES = { -45, 0, 45 }
local function wall_piece(pawn, i)
    local W = PS_walls[i]
    if W and there(W.a) then return W end
    if PS_wallFailed then return nil end
    if not there(PS_wallMesh) then
        for _, a in ipairs(FindAllOf("StaticMeshActor") or {}) do
            local found = false
            pcall(function()
                if a:IsValid() and a.StaticMeshComponent.StaticMesh:GetFullName():match("[^%.]+$") == "PlankWall01" then PS_wallMesh = a.StaticMeshComponent.StaticMesh; found = true end
            end)
            if found then break end
        end
    end
    local cls = StaticFindObject("/Script/Engine.StaticMeshActor")
    if not (there(PS_wallMesh) and there(cls)) then PS_wallFailed = true; log("no wall piece for Skyrim's walls (PlankWall01 not in this map)"); return nil end
    local l = pawn:K2_GetActorLocation()
    local park = { X = l.X, Y = l.Y, Z = l.Z - 6000 - i * 500 }
    local a = pawn:GetWorld():SpawnActor(cls, park, { Pitch = 0, Yaw = 0, Roll = 0 })
    if not there(a) then PS_wallFailed = true; return nil end
    local comp = a.StaticMeshComponent
    comp:SetMobility(2)
    comp:SetStaticMesh(PS_wallMesh)
    pcall(function() a:SetActorHiddenInGame(true) end)
    W = { a = a, park = park, off = { X = 0, Y = 0, Z = 0 }, ext = { X = 34, Y = 300, Z = 111 } }
    pcall(function()
        local origin, extent = {}, {}
        a:GetActorBounds(false, origin, extent, false)
        W.off = { X = origin.X - park.X, Y = origin.Y - park.Y, Z = origin.Z - park.Z }
        W.ext = { X = extent.X, Y = extent.Y, Z = extent.Z }
    end)
    PS_walls[i] = W
    log(string.format("wall piece %d ready: half size %.0f x %.0f x %.0f cm", i, W.ext.X, W.ext.Y, W.ext.Z))
    return W
end
function PS_place_walls(pawn)
    local fresh = os.clock() - input.stamp < 0.5
    local l, yaw
    for i = 1, 3 do
        local d = fresh and input.walls and input.walls[i] or 0
        if PS_wallTest then d = PS_wallTest[i] end   -- (dev)
        local W = (d > 0 or PS_walls[i]) and wall_piece(pawn, i) or nil
        if W then
            if d >= 45 then
                l = l or pawn:K2_GetActorLocation(); yaw = yaw or pawn:K2_GetActorRotation().Yaw
                local dir = math.rad(yaw + WALL_ANGLES[i])
                local cx, cy = math.cos(dir), math.sin(dir)
                -- the piece's thin side faces the player: its near face at the distance Skyrim gave
                local thinX = W.ext.X <= W.ext.Y
                local half = thinX and W.ext.X or W.ext.Y
                local turn = thinX and (yaw + WALL_ANGLES[i]) or (yaw + WALL_ANGLES[i] - 90)
                local tr = math.rad(turn)
                -- (the mesh's middle is not its pivot: turn that offset with the piece)
                local ox = W.off.X * math.cos(tr) - W.off.Y * math.sin(tr)
                local oy = W.off.X * math.sin(tr) + W.off.Y * math.cos(tr)
                local at = { X = l.X + cx * (d + half) - ox, Y = l.Y + cy * (d + half) - oy, Z = l.Z - W.off.Z }
                PS_place(W.a, at, { Pitch = 0, Yaw = turn, Roll = 0 }, true)
                W.out = true
            elseif W.out then
                PS_place(W.a, W.park, { Pitch = 0, Yaw = 0, Roll = 0 }, true)
                W.out = false
            end
        end
    end
end

-- A stand-in fights with a set skill, not with whichever behaviour profile MORDHAU gave the bot (sheets/enemy_ai.json
-- standin_skill): it parries in time more often than not.
function PS_standin_skill(c, level)
    pcall(function()
        local prof = c.Controller.BehaviorProfile
        if not there(prof) then return end
        local name = prof:GetFullName():match("[^%.]+$")
        local function set(k, v) pcall(function() prof[k] = v end) end
        local L = PS_ai_row(level)
        set("PerfectParryProbability", L[2])
        set("ParryTimingRandom", 0.3 * (1 - L[2]))
        pcall(function() prof.ParryTimingVariance = { X = 0.0, Y = 0.4 * (1 - L[2]) } end)
        set("FallForFeintProbability", 1 - L[2] * 0.8); set("FootworkInsteadOfParryProbability", 0.05)
        set("RiposteProbability", 0.4 + 0.6 * L[2]); set("FeintProbability", L[3]); set("MorphProbability", L[4]); set("ChamberProbability", L[2] * 0.5)
        set("BaseAttackHesitanceTime", L[5])
        log("stand-in skill set on " .. tostring(name) .. string.format(" (skill %.1f: well-timed parry %.2f, timing error now %.2f s)", level or PS_ai_level, prof.PerfectParryProbability, prof.ParryTimingRandom))
    end)
end

local function arm(b)
    local path = b.wantWeapon
    if not path then return end
    if b.armedFor ~= path and b.armingFor ~= path then
        b.armingFor = path; b.armed = false; b.armGaveUp = false; b.armStep = 0; b.armTry = -10; b.armDrops = 0; b.armBegan = os.clock()   -- a new character (or a new weapon): start over
    end
    if b.armed or b.armGaveUp then wake(b) end
    if b.armed or b.armGaveUp then return end
    local want = path:match("([%w_]+)$") .. "_C"
    local c = b.obj
    -- (looked at every frame: the moment the weapon is in the hand the stand-in counts as armed. Before, each step
    -- waited a fixed 3 s and arming took about 7 - play-test note)
    local inHand = class_name(c.RightHandEquipment)
    if class_name(c.LeftHandEquipment) == want then inHand = want end   -- (a bow is carried in the left hand)
    if inHand == want or (want == "BP_FistsWeapon_C" and inHand == "" and (b.armStep or 0) >= 1) then
        b.armed = true; b.armedFor = path
        log(string.format("stand-in %s holds %s (armed in %.1f s)", tostring(b.look), want, os.clock() - (b.armBegan or os.clock())))
        return
    end
    -- (the time a step needs before the next may look at its result: a drop little, a pick-up or a slot change more)
    if os.clock() - (b.armTry or -10) < (b.armGap or 0.3) or os.clock() - (b.since or 0) < 0.25 then return end
    b.armTry = os.clock()
    b.armStep = (b.armStep or 0) + 1
    b.armGap = 0.3
    if want == "BP_FistsWeapon_C" then
        -- bare hands in Skyrim (a brawl): every weapon the bot came with is taken out of MORDHAU's world, so its brain
        -- cannot bring one out again, and its fists come up. (Before: one SwitchToFists, and if the bot went back to its
        -- own knife it kept it - unseen in Skyrim.)
        if b.armStep > 8 then b.armGaveUp = true; log("stand-in " .. tostring(b.look) .. " could not be made to fight with its fists") return end
        local gone = 0
        pcall(function()
            local e = c.Equipment
            for i = 1, #e do
                local w = e[i]
                if there(w) and not class_name(w):find("Fists", 1, true) then w:K2_DestroyActor(); gone = gone + 1 end
            end
        end)
        for _, hand in ipairs({ "RightHandEquipment", "LeftHandEquipment" }) do
            pcall(function()
                local w = c[hand]
                if there(w) and not class_name(w):find("Fists", 1, true) then w:K2_DestroyActor(); gone = gone + 1 end
            end)
        end
        pcall(function() c:SwitchToFists() end)
        if gone > 0 then log("stand-in " .. tostring(b.look) .. ": " .. gone .. " weapon(s) taken away, it fights with its fists") end
        b.armGap = 0.4
        return
    end
    local holding = class_name(c.RightHandEquipment)
    local full = holding ~= "" and not holding:find("Fists", 1, true)
    if b.armStep == 1 then
        c:RequestDrop()   -- its hands and a slot must be free (a bot can spawn with all three slots taken)
    elseif b.armStep == 2 and full and (b.armDrops or 0) < 4 then
        b.armDrops = (b.armDrops or 0) + 1; b.armStep = 1; b.armGap = 0.5   -- (still holding its own: drop again)
        c:RequestDrop()
    elseif b.armStep == 2 then
        -- (what it dropped is taken out of MORDHAU's world: it is not this character's weapon)
        pcall(function()
            if b.weapon and there(b.weapon.held) and not there(b.weapon.held:GetAttachParentActor()) then b.weapon.held:K2_DestroyActor() end
            if b.weapon then b.weapon.held = nil; b.weapon.has = 0 end
        end)
        local base = path:find("^Ranged/") and "/Game/Mordhau/Blueprints/Equipment/" or WEAPONS   -- (bows are not under Weapons/)
        local full = base .. path .. "." .. path:match("([%w_]+)$") .. "_C"
        local cls = StaticFindObject(full)
        if not there(cls) then
            pcall(LoadAsset, base .. path)
            cls = StaticFindObject(full)
        end
        if there(cls) then
            local l = c:K2_GetActorLocation()
            local w = c:GetWorld():SpawnActor(cls, { X = l.X, Y = l.Y, Z = l.Z + 60 }, { Pitch = 0, Yaw = 0, Roll = 0 })
            log("arming stand-in " .. tostring(b.look) .. ": " .. want .. " made, giving it its parts")
            PS_finish_weapon(w)
            log("arming: parts done, picking up")
            if there(w) then c:PickUp(w, false) end   -- (into the empty hand)
            b.armGap = 0.8
            log("arming: picked up")
        else
            b.armGaveUp = true
            log("stand-in weapon class not found: " .. full)
        end
    elseif b.armStep <= 8 then
        local slot = ({ 1, 2, 3, 1, 2, 3 })[b.armStep - 2]   -- the picked-up weapon is in one of the three slots
        c["EquipSlot" .. slot](c)
        b.armGap = 1.0
    else
        b.armGaveUp = true
        log("stand-in " .. b.look .. " could not be handed " .. want .. "; it keeps its own weapon")
    end
end

local function player_attack_stage(pawn)
    local stage, name = -1, ""
    pcall(function()
        local m = pawn:GetMotion()
        name = class_name(m)
        local st = motion_stage(m)
        if st and not name:find("Parry") then stage = st end
    end)
    return stage, name
end

-- A Skyrim character sent with the "weapon" Demon/<Name> is the body of one of MORDHAU's Demon Horde creatures: its
-- stand-in is that creature itself (spawned here once, with MORDHAU's own controller), so its moves are its own.
local DEMONS = "/Game/Mordhau/Blueprints/GameModes/Horde/DemonInvasion/Enemies/"
PLAYER_BLADE_NEAR = 110   -- cm: how near the player's blade must pass to a stand-in's middle line for its lost health to be the player's hit (a big Demon Horde creature is up to 50 cm round, a person 35, and a blow lands on the arm or the shoulder)
PS_DEMON_BANNED = { Gremlin = { "GremlinLeap", "GremlinGrapple" } }   -- sheets/creature_fighters.json 'banned'
PS_DEMON_STRIKES = { Gremlin = true }   -- sheets/creature_fighters.json 'person_strikes'
local lastDemonAsk = -100
local function spawn_demon(pawn, name)
    if os.clock() - lastDemonAsk < 6 then return end
    lastDemonAsk = os.clock()
    local base = "BP_DemonHordeEnemy_Special_" .. name
    local full = DEMONS .. base .. "." .. base .. "_C"
    local cls = StaticFindObject(full)
    if not there(cls) then pcall(LoadAsset, DEMONS .. base); cls = StaticFindObject(full) end
    if not there(cls) then log("demon: MORDHAU has no " .. base); return end
    local l = pawn:K2_GetActorLocation()
    local a = pawn:GetWorld():SpawnActor(cls, { X = l.X + 2500, Y = l.Y, Z = l.Z + 300 }, { Pitch = 0, Yaw = 0, Roll = 0 })
    if not there(a) then log("demon: MORDHAU did not spawn " .. base); return end
    pcall(function() a:SpawnDefaultController() end)
    bots[#bots + 1] = { obj = a, prepared = false, health = 100, hits = 0, damage = 0, demon = name }
    log("demon: spawned MORDHAU's " .. name .. " (" .. class_name(a.Controller) .. ")")
end

-- Falling matches in both games. Limp in Skyrim -> MORDHAU's own ragdoll fall is started on the character and started
-- again whenever MORDHAU begins to get it up while Skyrim still has it down. (MORDHAU stands it up by itself afterwards.)
local FALLEN = 7
local function fallen(c)
    if not there(c) then return false end
    return class_name(c:GetMotion()):find("RagdollFalling", 1, true) ~= nil
end
local function keep_down(c, state, down)
    if not down then state.downAsked = nil; return end
    if os.clock() - (state.downAsked or -10) < 0.4 then return end
    local m = c:GetMotion()
    local lying = class_name(m):find("RagdollFalling", 1, true) ~= nil
    local rising = false
    if lying then pcall(function() rising = m.bIsGettingUp and true or false end) end
    if not lying or rising then
        state.downAsked = os.clock()
        pcall(function() c:SetIsRagdollFalling(true) end)
        if not state.downLogged then state.downLogged = true; log("fall: limp in Skyrim, so MORDHAU's " .. (state.look and ("stand-in " .. state.look) or "character") .. " falls too") end
    end
end
local playerFall = {}

-- Archery. While Skyrim's player has a bow out, MORDHAU's character holds MORDHAU's bow (handed over once per life,
-- then brought out by slot); its quiver is kept full (Skyrim's arrows are the ones that count). When MORDHAU looses
-- an arrow, its direction, speed, gravity and damage table go to Skyrim, which fires the real one; MORDHAU's own
-- projectile is removed (it would only hit the arena or a stand-in).
PS_shots = PS_shots or 0
PS_shot = PS_shot or { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
local bow = {}
local function ranged_item(pawn)
    for _, hand in ipairs({ "LeftHandEquipment", "RightHandEquipment" }) do
        local e = pawn[hand]
        local n = class_name(e)
        if n:lower():find("bow", 1, true) then return e, n end   -- (BP_RecurveBow, BP_Longbow, BP_Crossbow)
    end
    return nil, ""
end
function PS_player_ranged(pawn)
    local wish = os.clock() - input.stamp < 0.5 and input.weaponWish or nil
    local held, heldName = ranged_item(pawn)
    local me = pawn:GetFullName()
    if bow.owner ~= me then bow = { owner = me } end
    -- (the bow itself is handed over by PS_player_arm, as melee weapons are; here its quiver is kept full)
    if wish and held and os.clock() - (bow.fill or 0) > 1.0 then
        bow.fill = os.clock()
        pcall(function() held:SetAmmo(held:GetCurrentMaxAmmo()) end)
    end
    -- an arrow loosed just now?
    local m = pawn:GetMotion()
    local mname = class_name(m)
    if mname ~= bow.motion then
        if mname:find("Ranged", 1, true) or (bow.motion or ""):find("Ranged", 1, true) then log(string.format("archery: motion %s -> %s at %.2f, stamina %.0f", tostring(bow.motion), mname, os.clock(), pawn.Stamina or -1)) end
        bow.motion = mname
    end
    -- MORDHAU creates the arrow during its release motion: it is looked for in every frame of that motion (and a
    -- moment after), so Skyrim's arrow leaves when MORDHAU's does - not when the motion is over, about a third of a
    -- second later, as before.
    local releasingNow = mname:find("RangedRelease", 1, true) ~= nil
    if releasingNow and not bow.releasing then bow.releasing = true; bow.found = false; bow.releaseAt = os.clock() end
    if bow.releasing and not releasingNow and not bow.endAt then bow.endAt = os.clock() end
    local giveUp = bow.releasing and bow.endAt and os.clock() - bow.endAt > 0.25
    if bow.releasing and (not bow.found or giveUp) then
        bow.seen = bow.seen or {}
        local p = nil
        for _, q in ipairs(FindAllOf("MordhauProjectile") or {}) do
            if q:IsValid() then
                local name = q:GetFullName()
                if not bow.seen[name] then
                    bow.seen[name] = true
                    local kind = class_name(q)
                    if kind:find("Arrow", 1, true) or kind:find("Bolt", 1, true) then p = q; bow.thrown = false
                    elseif kind:find("Thrown", 1, true) then p = q; bow.thrown = true; bow.thrownShield = (kind:find("Targe", 1, true) or kind:find("Buckler", 1, true)) ~= nil end   -- (a thrown weapon, or a shield)
                end
            end
        end
        if giveUp then
            if not bow.found then log("archery: released, but MORDHAU made no arrow (not drawn far enough, or no ammunition)") end
            bow.releasing = false; bow.endAt = nil
        end
        if p then
            local v = p:GetVelocity()
            local speed = math.sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z)
            local shot = { 0, 1, 0, speed, 1, 0, 0, 0, 0, 0, 0, 0, 0 }
            pcall(function() shot[4] = p.ProjectileMovement.InitialSpeed end)   -- (as loosed; it has slowed a little by now)
            local T = pawn.Mesh:K2_GetComponentToWorld()
            local t = T.Translation
            local lx, ly, lz = to_local(T, { X = t.X + v.X, Y = t.Y + v.Y, Z = t.Z + v.Z })
            if speed > 1 then shot[1], shot[2], shot[3] = -lx / speed, ly / speed, lz / speed end
            pcall(function() shot[5] = p.ProjectileMovement.ProjectileGravityScale end)
            pcall(function() local d = p.Damage; for i = 1, math.min(#d, 4) do shot[5 + i] = d[i] end end)
            pcall(function() local d = p.HeadBonus; for i = 1, math.min(#d, 4) do shot[9 + i] = d[i] end end)
            if bow.thrown then
                shot[4] = -math.abs(shot[4])
                -- (a shield: gravity scale negative too; the weapon stays in the hand)
                if bow.thrownShield then shot[5] = -math.abs(shot[5]); PS_shieldTry = os.clock() + 4 else PS_thrownAt = os.clock() end
            end   -- (speed sent negative = a thrown weapon, not an arrow)
            PS_shot = shot
            PS_shots = PS_shots + 1
            log(string.format("archery: %s loosed, %.0f cm/s, direction right %.2f forward %.2f up %.2f, damage %.0f/%.0f/%.0f/%.0f, head bonus %.0f/%.0f/%.0f/%.0f",
                class_name(p), shot[4], shot[1], shot[2], shot[3], shot[6], shot[7], shot[8], shot[9], shot[10], shot[11], shot[12], shot[13]))
            log(string.format("archery: the arrow left %.2f s into the release", os.clock() - (bow.releaseAt or os.clock())))
            bow.found = true
            pcall(function() p:K2_DestroyActor() end)
        end
    end
end
function PS_shot_block()
    return string.pack("<I4fffffffffffff", PS_shots, table.unpack(PS_shot))
end

local lastPlayerStage = -1
PS_PLAYER_GAP = 101   -- cm: MORDHAU's characters are 50 cm round (CapsuleRadius, read in play), so two of them never stand nearer than 100 - set nearer, MORDHAU throws the stand-in back out the next frame
local function update_ghosts(pawn, pc)
    local okRanged, rangedErr = pcall(PS_player_ranged, pawn)
    if not okRanged and (bow.errs or 0) < 3 then bow.errs = (bow.errs or 0) + 1; log("archery failed: " .. tostring(rangedErr)) end
    pcall(keep_down, pawn, playerFall, os.clock() - input.stamp < 0.5 and input.downPlayer)
    if not input.downPlayer then playerFall.downLogged = nil end
    if os.clock() - lastBotScan > 2.0 then scan_bots(pc) end
    local wanted = input.ghosts or {}
    if os.clock() - input.stamp > 0.5 then wanted = {} end
    -- keep assignments, free the rest
    local byId = {}
    for _, g in ipairs(wanted) do byId[g.id] = g end
    for _, b in ipairs(bots) do if b.ghost and not byId[b.ghost] then b.ghost = nil; b.read = false end end
    for _, g in ipairs(wanted) do
        local have = false
        for _, b in ipairs(bots) do if b.ghost == g.id then have = true end end
        if not have then
            local demon = g.weapon and g.weapon:match("^Demon/(%a+)") or nil
            for _, b in ipairs(bots) do
                if not b.ghost and not b.dead and b.obj:IsValid() and b.demon == demon then
                    b.ghost = g.id; b.attacks = g.attacks; b.hits = 0; b.damage = 0; have = true; b.idleSince = nil
                    b.awake = false; b.busyAt = os.clock()   -- (a new character to stand in for: its brain is started afresh)
                    break
                end
            end
            if not have and demon then spawn_demon(pawn, demon) end
        end
    end

    local l = pawn:K2_GetActorLocation()
    -- (the fixed spot MORDHAU's character is kept at - see keep_near_home, which lets it stray up to 5 cm before setting
    -- it back. Where a FREE stand-in stands in Skyrim's world is reckoned from this spot, not from where the character
    -- happens to be in this frame: reckoned from the character, every free stand-in's place in Skyrim took on that
    -- 0-5 cm saw-tooth whenever the player walked - the jitter 'when i move'. sheets/builds.json hybrid_fixed_origin)
    local spot = home and { X = home.X, Y = home.Y } or l
    local f = pawn:GetActorForwardVector()
    local r = pawn:GetActorRightVector()
    local yaw = pawn:K2_GetActorRotation().Yaw
    local stage = player_attack_stage(pawn)
    local releaseBegan = stage == 1 and lastPlayerStage ~= 1
    if releaseBegan then
        -- what each stand-in is doing as the player's blade is let go (for the log: parried? out of reach?)
        local what = {}
        for _, b in ipairs(bots) do
            if b.ghost and b.obj:IsValid() then
                local g = byId[b.ghost]
                what[#what + 1] = string.format("%d:%s at %.0f cm", b.look or 0, class_name(b.obj:GetMotion()), g and math.sqrt(g.forward * g.forward + g.right * g.right) or -1)
            end
        end
        local angle = "?"
        pcall(function() local m = pawn:GetMotion(); if there(m) then angle = string.format("%s, angle %.2f", class_name(m), m.AngleTarget) end end)
        log("player release (" .. angle .. "); stand-ins " .. (#what > 0 and table.concat(what, ", ") or "none"))
    end
    local windupBegan = stage == 0 and lastPlayerStage ~= 0
    lastPlayerStage = stage
    -- a stand-in's lost health counts as the player's hit only around the player's own release (stand-ins with their
    -- own brains can also clip each other)
    if stage == 1 then playerReleaseUntil = os.clock() + 0.3 end
    local playerStriking = os.clock() <= (playerReleaseUntil or 0)
    local parked = 0
    for i = #bots, 1, -1 do if bots[i].dead then table.remove(bots, i) end end
    PS_apply_voice(pawn)
    if os.clock() - (PS_volumeAt or -10) > 2.0 then PS_volumeAt = os.clock(); PS_apply_volume(false) end
    PS_no_world_hits(pawn)
    for _, b in ipairs(bots) do
        local c = b.obj
        if c:IsValid() then
            local gone = true
            pcall(function() gone = c.bIsDead or (c.Health or 0) <= 0 end)
            if gone then
                b.dead = true   -- dead or dying: never touch it again (MORDHAU destroys it soon)
                local why = "?"
                pcall(function() local l = c:K2_GetActorLocation(); why = string.format("health %s, dead flag %s, at (%.0f, %.0f, %.0f), %.1f s after it was taken on", tostring(c.Health), tostring(c.bIsDead), l.X, l.Y, l.Z, os.clock() - (b.since or 0)) end)
                log("stand-in " .. tostring(b.look) .. " is gone: " .. why)
                goto continue
            end
            if not b.prepared then prepare(b) end
            local g = b.ghost and byId[b.ghost]
            b.wantWeapon = g and not b.demon and (g.weapon or "Specific/OneHanded/BP_ArmingSword") or nil
            if g and b.demon and g.weapon then b.wantWeapon = g.weapon:match("^Demon/%a+/(.+)$") end   -- (sheets/creature_fighters.json weapon)
            if g and not b.demon and not b.voiceChecked then b.voiceChecked = true; PS_human_voice(c, pawn) end
            if g then PS_mute_voice(b, c) end
            if g and not b.demon then pcall(PS_standin_shield, b, c) end
            -- A passive stand-in (an archer's, a bystander's) does not parry: its own behaviour is told to step aside
            -- instead of parrying, always - and held in place it cannot step either. (Ending its parry motion as it began
            -- came too late: archers still parried with their bows out.) Its skill is put back when it fights.
            if g and not b.demon then
                local passive = PS_passive and PS_passive[b.ghost] or false
                if passive and (b.noParry ~= b.ghost or os.clock() - (b.noParryAt or 0) > 3.0) then
                    b.noParry = b.ghost; b.noParryAt = os.clock()
                    pcall(function()
                        local prof = c.Controller.BehaviorProfile
                        prof.FootworkInsteadOfParryProbability = 1.0
                        prof.PerfectParryProbability = 0.0
                        prof.ChamberProbability = 0.0
                        prof.RiposteProbability = 0.0
                    end)
                elseif not passive and b.noParry then
                    b.noParry = nil
                    PS_standin_skill(c, PS_skill_of(b))
                end
            end
            -- A stand-in that has lost its weapon (disarmed, dropped) takes it up again: after 3 s when it lies within
            -- reach, and after 8 s it draws another like it. (Held in place at its Skyrim character's spot, its brain
            -- never walked over to it: a disarmed opponent did not pick her sword up again.)
            if g and not b.demon and b.armed and b.weapon and b.weapon.has == 2 then
                b.dropAt = b.dropAt or os.clock()
                local t = os.clock() - b.dropAt
                if t > 3.0 and os.clock() - (b.pickTry or 0) > 1.5 and class_name(c:GetMotion()) == "IdleMotion" then
                    b.pickTry = os.clock()
                    local w = b.weapon.held
                    local near = false
                    pcall(function()
                        local a, e = w:K2_GetActorLocation(), c:K2_GetActorLocation()
                        near = (a.X - e.X) ^ 2 + (a.Y - e.Y) ^ 2 < 220 * 220
                    end)
                    if there(w) and near then
                        pcall(function() c:PickUp(w, false) end)
                        log("stand-in " .. tostring(b.look) .. " takes its weapon up again")
                    elseif t > 8.0 then
                        b.armed = false; b.armedFor = nil; b.armingFor = nil; b.dropAt = nil
                        log("stand-in " .. tostring(b.look) .. " has been without its weapon for 8 s: it is armed anew")
                    end
                end
            else
                b.dropAt = nil
            end
            -- A bot may duck under a swing, as in MORDHAU - but one that stays down (seen: a bandit crouched for a
            -- whole fight, under every horizontal swing) is stood up again after a second.
            if g and not b.demon then
                local down = false
                pcall(function() down = c.bIsCrouched == true end)
                if not down then b.crouchSince = nil
                elseif not b.crouchSince then b.crouchSince = os.clock()
                elseif os.clock() - b.crouchSince > 1.0 then
                    pcall(function() c:StopCrouching() end)
                    pcall(function() c:UnCrouch(false) end)
                    if not b.crouchTold then b.crouchTold = true; log("stand-in " .. tostring(b.look) .. " stayed crouched: stood up again") end
                    b.crouchSince = os.clock()
                end
            end
            local skill = g and PS_skill_of(b) or PS_ai_level
            -- A stand-in that stands idle within reach of the player for 6 s has a brain that has gone to sleep (seen
            -- after Skyrim had been paused or in the background for minutes: the bandits no longer attacked): started again.
            if g and not b.demon and not (PS_passive and PS_passive[b.ghost]) then
                local busy = true
                pcall(function() busy = class_name(c:GetMotion()) ~= "IdleMotion" end)
                local dist = math.sqrt((g.forward or 0) ^ 2 + (g.right or 0) ^ 2)
                if busy or dist > 320 then b.busyAt = os.clock() end
                if b.awake and os.clock() - (b.busyAt or os.clock()) > 6 then
                    b.awake = false; b.busyAt = os.clock()
                    log("stand-in " .. tostring(b.look) .. " stood idle beside the player for 6 s: its brain is started again")
                end
            end
            if g and not b.demon and (b.skillFor ~= b.ghost or b.skillLevel ~= skill) then b.skillFor = b.ghost; b.skillLevel = skill; PS_standin_skill(c, skill) end
            local okArm, armErr = pcall(arm, b)
            if PS_parrytest and g and not b.demon then
                pcall(function()
                    local m = c:GetMotion()
                    local mname = class_name(m)
                    local st = motion_stage(m)
                    local attacking = st ~= nil and (mname:find("Strike") or mname:find("Stab")) and true or false
                    b.pt = b.pt or { n = 0 }
                    local T = b.pt
                    if attacking and st == 1 and T.stage ~= 1 and not T.active then
                        T.n = T.n + 1
                        local parry = PS_parrytest == 2 or T.n % 2 == 0
                        local pitch = 0
                        pcall(function() pitch = pawn:GetRawLookUpValue() end)
                        T.active = { t = os.clock(), hits0 = playerHits, motion = mname, parry = parry, seen = {}, mine = {}, pitch = pitch,
                            dist = math.sqrt(g.forward * g.forward + g.right * g.right), angle = math.deg(math.atan(g.right, g.forward)), up = g.up }
                        if parry then pawn:FeintOrBlockPressed(); releaseBlockAt = os.clock() + 0.12 end
                    end
                    T.stage = attacking and st or -1
                    local A = T.active
                    if A then
                        if mname ~= A.motion then A.seen[mname] = true end
                        A.mine[class_name(pawn:GetMotion())] = true
                        if os.clock() - A.t > 0.9 then
                            local seen, mine = {}, {}
                            for k in pairs(A.seen) do seen[#seen + 1] = k end
                            for k in pairs(A.mine) do mine[#mine + 1] = k end
                            log(string.format("parrytest: %s, parry %s -> %s | attacker %.0f cm away, %.0f deg off my front, %.0f cm %s, my look pitch %.0f | attacker then: %s | me: %s",
                                A.motion, A.parry and "pressed" or "not pressed", playerHits > A.hits0 and "HIT ME" or "no hit", A.dist, A.angle, math.abs(A.up), A.up >= 0 and "above" or "below", A.pitch,
                                table.concat(seen, ","), table.concat(mine, ",")))
                            T.active = nil
                        end
                    end
                end)
            end
            if g and not b.demon then
                pcall(function()
                    local m = c:GetMotion()
                    local mname = class_name(m)
                    local st = motion_stage(m)
                    local attacking = st ~= nil and (mname:find("Strike") or mname:find("Stab")) and true or false
                    local S = b.swing
                    if attacking and st == 1 and not S then
                        S = { t = os.clock(), hits0 = playerHits, motion = mname, near = 1e9, dist = math.sqrt(g.forward * g.forward + g.right * g.right),
                              angle = math.deg(math.atan(g.right, g.forward)), up = g.up, seen = {}, mine = class_name(pawn:GetMotion()) }
                        -- which way it faces and aims as the blade is let go, against where the player stands
                        pcall(function()
                            local cl, cf = c:K2_GetActorLocation(), c:GetActorForwardVector()
                            local to = math.deg(math.atan(l.Y - cl.Y, l.X - cl.X))
                            local function off(a) local d = (a - to + 540) % 360 - 180; return d end
                            S.faces = off(math.deg(math.atan(cf.Y, cf.X)))
                            local cr = c:GetControlRotation()
                            S.aims = off(cr.Yaw); S.pitch = cr.Pitch > 180 and cr.Pitch - 360 or cr.Pitch
                            S.look = c:GetRawLookUpValue()
                        end)
                        b.swing = S
                    end
                    if S then
                        if attacking and st == 1 then
                            -- how near the blade comes to the player's body (its middle line, feet to head)
                            local w = c.RightHandEquipment
                            if there(w) then
                                local a, e = w.CurrentTraceStart, w.CurrentTraceEnd
                                for k = 0, 4 do
                                    local x, y, z = a.X + (e.X - a.X) * k / 4, a.Y + (e.Y - a.Y) * k / 4, a.Z + (e.Z - a.Z) * k / 4
                                    local dz = math.max(0, math.abs(z - l.Z) - 90)
                                    local d = math.sqrt((x - l.X) ^ 2 + (y - l.Y) ^ 2 + dz * dz)
                                    if d < S.near then S.near = d end
                                end
                            end
                        elseif mname ~= S.motion then
                            S.seen[#S.seen + 1] = mname
                        end
                        if (not attacking or st ~= 1) and os.clock() - S.t > 0.25 then
                            local hit = playerHits > S.hits0
                            log(string.format("swing: %s -> %s | blade came within %.0f cm of the player | from %.0f cm, %.0f deg off the player's front, %.0f cm %s | then %s | player was in %s",
                                S.motion .. string.format(" [body %.0f deg off the player, aim %.0f deg off, aim pitch %.0f, look %.0f]", S.faces or 999, S.aims or 999, S.pitch or 999, S.look or 999), hit and "HIT" or "no hit", S.near, S.dist, S.angle, math.abs(S.up), S.up >= 0 and "above" or "below", S.seen[1] or mname, S.mine))
                            b.swing = nil
                        end
                    end
                end)
            end
            pcall(keep_down, c, b, g and input.down and input.down[b.ghost] or false)
            if not (g and input.down and input.down[b.ghost]) then b.downLogged = nil end
            if not okArm and not b.armErr then b.armErr = true; log("arming a stand-in failed: " .. tostring(armErr)) end
            pcall(function()
                if g then
                    -- (On sloping ground the Skyrim character stands a little higher or lower than the player; set down that
                    -- much into or above MORDHAU's flat floor, the bot's legs folded up to meet it: an odd crouch, seen in play
                    -- on one stretch of sloping ground. Up to 45 cm of
                    -- height difference is taken as slope and left out; beyond that - a ledge, stairs - the rest counts.)
                    local up = g.up
                    if up > 45 then up = up - 45 elseif up < -45 then up = up + 45 else up = 0 end
                    -- Hybrid movement (sheets/builds.json hybrid_movement): near its opponent the bot is set free and
                    -- MORDHAU's own AI moves it; Skyrim walks its character after it (and may refuse a step: a wall, a
                    -- ledge - then the bot is set back to where the character really is). Far off, and for one that
                    -- only stands to be hit, it is held at the Skyrim character's place as before.
                    local home = { X = l.X + f.X * g.forward + r.X * g.right, Y = l.Y + f.Y * g.forward + r.Y * g.right, Z = l.Z + up - 97 * (1 - (b.size or 1)) }   -- (a smaller stand-in's capsule is shorter: set lower by that, so its feet are right)
                    local free = false
                    if PS_HYBRID and input.pp and BOT_BRAINS and b.armed and not b.demon and not (PS_passive and PS_passive[b.ghost])
                        and not (input.down and input.down[b.ghost]) and os.clock() >= (b.holdUntil or 0)
                        and math.abs(g.up) < 80 then   -- (on another level than the player - a ledge, stairs - it is held as before)
                        local opponent, at = pawn, l
                        local foe = PS_ghostTarget and PS_ghostTarget[b.ghost]
                        if foe then
                            opponent = nil
                            for _, o in ipairs(bots) do
                                if o.ghost == foe and o ~= b and o.obj:IsValid() then opponent = o.obj; at = o.obj:K2_GetActorLocation() break end
                            end
                        end
                        if opponent then
                            local away = math.sqrt((home.X - at.X) ^ 2 + (home.Y - at.Y) ^ 2)
                            free = away < (PS_ENGAGE or 450) + (b.free and 150 or 0)
                            if free and (not b.free or os.clock() - (b.aimedAt or 0) > 0.5) then
                                b.aimedAt = os.clock()
                                pcall(function() c.Controller:SetClosestEnemyOverride(opponent) end)
                            end
                        end
                    end
                    -- (Skyrim refuses its steps - a wall, a ledge: after three such frames it is set back to its character
                    -- and held there for hybrid_hold_seconds (0.5), as before the hybrid movement. Left free it stood off its Skyrim body and
                    -- the player's swings at that body went through, seen in play by walls and in tight spaces.)
                    if free and b.free and PS_refused and PS_refused[b.ghost] then
                        -- (Third way: no waiting and no holding. The very frame Skyrim refuses its step the
                        -- bot is put back where its Skyrim character stands, and stays free - it walks against the wall as
                        -- against a wall, never apart from the body the player sees. Held for 0.5 s after three refused
                        -- frames, it still stood up to 70 cm off that body in between, and in a crypt's passages that was
                        -- most of the time: swings passed through.)
                        pcall(function() local cl = c:K2_GetActorLocation(); PS_why["refused"] = (PS_why["refused"] or 0) + 1; PS_place(c, { X = home.X, Y = home.Y, Z = cl.Z }, c:K2_GetActorRotation(), false) end)
                        b.held = (b.held or 0) + 1
                        if b.held == 1 or b.held % 200 == 0 then log("hybrid: stand-in " .. tostring(b.look) .. " walks against something Skyrim does not let it through (" .. b.held .. " frames so far)") end
                    end
                    if free and not b.free then
                        b.free = true; b.astray = nil
                        -- (onto MORDHAU's floor first: held, it stood at Skyrim's height against the player, which can be
                        -- under MORDHAU's ground or in the air above it - set free there it fell and stayed in its landing
                        -- animation, where nothing could hurt it)
                        PS_seat(c, l.Z)
                        -- (MORDHAU's AI walks it over MORDHAU's own ground, at its own pace - as in the version found
                        -- good in play. Movement mode 5, "flying", tried for the height, made it move at the engine's
                        -- flying speed (600 cm/s, no braking): it ran back to its spot faster than the turn correction
                        -- moved it away: the enemy kept its place relative to the player. Its height is the BODY's
                        -- matter now, not the movement's: see PS_body_height.)
                        pcall(function() c.CharacterMovement:SetMovementMode(1, 0) end)
                        pcall(function() c.Controller.BrainComponent:RestartLogic() end)
                        b.awake = true
                        PS_freed = (PS_freed or 0) + 1
                        if PS_freed <= 3 or PS_freed % 10 == 0 then log("hybrid: stand-in " .. tostring(b.look) .. " is set free near its opponent (" .. PS_freed .. " so far)") end
                    elseif b.free and not free then
                        b.free = false
                        pcall(function() c.Controller:StopMovement() end)
                        pcall(function() c.CharacterMovement:DisableMovement() end)
                    end
                    if free then
                        -- MORDHAU's player character walks on the spot and does not turn with Skyrim's view: this little
                        -- world is the player's own frame. So a free bot is shifted against whatever the player has
                        -- walked and turned in Skyrim since last time - it stays where it is in Skyrim's world.
                        local pp = input.pp
                        if b.ppFa and (pp.x ~= b.ppX or pp.y ~= b.ppY or pp.fa ~= b.ppFa) then
                            local cl = c:K2_GetActorLocation()
                            local dx, dy = cl.X - spot.X, cl.Y - spot.Y
                            local R, F = dx * r.X + dy * r.Y, dx * f.X + dy * f.Y
                            local a0, a1 = math.rad(b.ppFa), math.rad(pp.fa)
                            -- its place in Skyrim's world as of last time ...
                            local wx = b.ppX + math.cos(a0) * R + math.sin(a0) * F
                            local wy = b.ppY - math.sin(a0) * R + math.cos(a0) * F
                            -- ... seen from where the player is and faces now
                            local ox, oy = wx - pp.x, wy - pp.y
                            local R1, F1 = ox * math.cos(a1) - oy * math.sin(a1), ox * math.sin(a1) + oy * math.cos(a1)
                            local turn = pp.fa - b.ppFa
                            local rot = c:K2_GetActorRotation()
                            PS_why["playerframe"] = (PS_why["playerframe"] or 0) + 1; PS_place(c, { X = spot.X + f.X * F1 + r.X * R1, Y = spot.Y + f.Y * F1 + r.Y * R1, Z = cl.Z }, { Pitch = 0, Yaw = rot.Yaw - turn, Roll = 0 }, false)
                            if math.abs(turn) > 0.01 then
                                pcall(function() local cr = c.Controller:GetControlRotation(); c.Controller:SetControlRotation({ Pitch = cr.Pitch, Yaw = cr.Yaw - turn, Roll = 0 }) end)
                            end
                        end
                        b.ppX, b.ppY, b.ppFa = pp.x, pp.y, pp.fa
                        -- (its height: the BODY is raised or lowered against the ground MORDHAU has it walk on, so that it
                        -- stands against the player as Skyrim has the two)
                        if c:K2_GetActorLocation().Z < l.Z - 180 then PS_why["fell"] = (PS_why["fell"] or 0) + 1; PS_seat(c, l.Z) end   -- (through MORDHAU's ground)
                        -- (the WHOLE difference Skyrim has between the two, g.up - not the part left after the slope
                        -- allowance: with the enemy 25 cm uphill in Skyrim and level here, a swing aimed at his chest
                        -- met his head: measured at the player's own hits)
                        PS_body_height(c, b, (l.Z + g.up) - c:K2_GetActorLocation().Z)
                        pcall(function() local mode = c.CharacterMovement.MovementMode; if mode == 5 or mode == 0 then c.CharacterMovement:SetMovementMode(1, 0) end end)
                        -- (never nearer to the player's spot than Skyrim lets two characters stand - sheets/builds.json
                        -- hybrid_player_gap. MORDHAU's player walks on the spot, so walking INTO a fighter in Skyrim only shifts
                        -- his stand-in towards that spot, through it if need be; his Skyrim body is held off at the gap, the two
                        -- came apart and the stand-in was set back to the body - "he starts teleporting back then runs back to
                        -- me". Held off at the same distance here, he is simply pushed along.)
                        do
                            local at = c:K2_GetActorLocation()
                            local gx, gy = at.X - spot.X, at.Y - spot.Y
                            local near = math.sqrt(gx * gx + gy * gy)
                            if near < PS_PLAYER_GAP then
                                if near < 1 then gx, gy, near = f.X, f.Y, 1 end
                                PS_why["gap"] = (PS_why["gap"] or 0) + 1
                                PS_place(c, { X = spot.X + gx / near * PS_PLAYER_GAP, Y = spot.Y + gy / near * PS_PLAYER_GAP, Z = at.Z }, c:K2_GetActorRotation(), false)
                            end
                        end
                        -- (Skyrim did not follow - its character is elsewhere for a third of a second: back to it)
                        local cl = c:K2_GetActorLocation()
                        if (cl.X - home.X) ^ 2 + (cl.Y - home.Y) ^ 2 > (PS_SNAP or 110) ^ 2 then
                            b.astray = b.astray or os.clock()
                            if os.clock() - b.astray > 0.1 then
                                b.astray = nil
                                PS_why["astray"] = (PS_why["astray"] or 0) + 1; PS_place(c, { X = home.X, Y = home.Y, Z = cl.Z }, c:K2_GetActorRotation(), true)
                                b.snaps = (b.snaps or 0) + 1
                                if b.snaps == 1 or b.snaps % 10 == 0 then log("hybrid: stand-in " .. tostring(b.look) .. " set back to its Skyrim character (" .. b.snaps .. " so far)") end
                            end
                        else
                            b.astray = nil
                        end
                    else
                    b.ppFa = nil
                    PS_body_height(c, b, g.up - up)   -- (held at Skyrim's place less the slope allowance: the body makes up the rest)
                    PS_why["held"] = (PS_why["held"] or 0) + 1; PS_place(c, home, { Pitch = 0, Yaw = yaw + g.yaw, Roll = 0 }, false)
                    end
                    -- (where it stands and faces now, for Skyrim: sheets/live_link.json standin_move)
                    do
                        local cl = c:K2_GetActorLocation()
                        local dx, dy = cl.X - spot.X, cl.Y - spot.Y
                        PS_moveOut = PS_moveOut or {}
                        local R, F = dx * r.X + dy * r.Y, dx * f.X + dy * f.Y
                        local pp = input.pp
                        if pp then
                            local a = math.rad(pp.fa)
                            PS_moveOut[#PS_moveOut + 1] = string.pack("<I4fffi4", b.ghost, pp.x + math.cos(a) * R + math.sin(a) * F, pp.y - math.sin(a) * R + math.cos(a) * F,
                                pp.fa + (c:K2_GetActorRotation().Yaw - yaw), free and 1 or 0)
                        else
                            PS_moveOut[#PS_moveOut + 1] = string.pack("<I4fffi4", b.ghost, 0, 0, 0, 0)
                        end
                    end
                    -- the Skyrim character is walking: so does the stand-in (for its legs; its place is set above anyway)
                    local speed = math.sqrt(g.vr * g.vr + g.vf * g.vf)
                    -- (its movement is switched off, so its speed is written directly: MORDHAU's animation reads it)
                    if not free and (speed > 20 or b.moving) then
                        b.moving = speed > 20
                        if there(c.CharacterMovement) then c.CharacterMovement.Velocity = { X = f.X * g.vf + r.X * g.vr, Y = f.Y * g.vf + r.Y * g.vr, Z = 0 } end
                    end
                    -- the Skyrim character began an attack: its stand-in attacks, by MORDHAU's rules
                    if g.attacks ~= b.attacks and free then b.attacks = g.attacks end   -- (free: its brain decides when to strike)
                    if g.attacks ~= b.attacks then
                        b.attacks = g.attacks
                        -- (with its own brain too, when that brain is doing nothing: held in place it mostly waits to
                        -- walk closer, so the Skyrim character's decision to attack is what sets it off)
                        -- (only from where a MORDHAU blade can reach: Skyrim characters start their attacks from farther away)
                        local reach = math.sqrt(g.forward * g.forward + g.right * g.right) <= 215
                        -- (a Demon Horde stand-in attacks by its own brain - except one whose own attack is left out
                        -- (sheets/creature_fighters.json 'banned': the Gremlin has nothing but its leap): that one strikes
                        -- as a person's stand-in does, when its Skyrim creature attacks)
                        local own = b.demon and not PS_DEMON_STRIKES[b.demon]
                        if not BOT_BRAINS or (not own and reach and class_name(c:GetMotion()) == "IdleMotion") then
                            local how = STRIKES[math.random(#STRIKES)]
                            local ok, err = pcall(function() c[how](c) end)
                            if b.demon and (b.struck or 0) < 6 then b.struck = (b.struck or 0) + 1; log("stand-in " .. tostring(b.look) .. " (" .. b.demon .. ") is asked for " .. how .. (ok and "" or ": " .. tostring(err))) end
                        end
                    end
                    -- it looks at the player's character, not at the spot on the ground its brain wants to walk to
                    -- (looking there made it swing at the ground)
                    -- ... and it aims at the player's body with every frame: where it looks (side and height) is where its
                    -- swing goes. (Left to its brain the aim was 17-54 degrees off to the side and down to straight at
                    -- the ground: blades passed through the player without counting.)
                    if not b.demon and not free then
                        local ctrl = c.Controller
                        if there(ctrl) then
                            local cl = c:K2_GetActorLocation()
                            -- (whom it fights in Skyrim, when that is another of Skyrim's people with a stand-in: it aims
                            -- there. Aiming at the player always, two of them fighting each other below the player both
                            -- looked up and swung over each other.)
                            local aim = l
                            local foe = PS_ghostTarget and PS_ghostTarget[b.ghost]
                            local foeThere = false
                            if foe then
                                for _, o in ipairs(bots) do
                                    if o.ghost == foe and o ~= b and o.obj:IsValid() then pcall(function() aim = o.obj:K2_GetActorLocation(); foeThere = true end) break end
                                end
                            end
                            local dx, dy = aim.X - cl.X, aim.Y - cl.Y
                            -- Fighting another of Skyrim's people: the bot's own brain only ever goes for the player's
                            -- character, so against anyone else it stood and parried (a follower made one swing in a
                            -- whole fight). Within a blade's reach of its opponent it is made to strike, every 1 to 2.2 s.
                            if foeThere and not (PS_passive and PS_passive[b.ghost]) and b.armed and dx * dx + dy * dy <= 190 * 190
                                and os.clock() >= (b.nextDrive or 0) and class_name(c:GetMotion()) == "IdleMotion" then
                                b.nextDrive = os.clock() + 1.0 + math.random() * 1.2
                                b.driven = (b.driven or 0) + 1
                                pcall(function() c[STRIKES[math.random(#STRIKES)]](c) end)
                                if b.driven == 1 or b.driven % 10 == 0 then log("stand-in " .. tostring(b.look) .. " strikes at its Skyrim opponent (" .. b.driven .. " so far)") end
                            end
                            local pitchTo = math.deg(math.atan(aim.Z - cl.Z, math.sqrt(dx * dx + dy * dy)))
                            pitchTo = math.max(-45, math.min(45, pitchTo))
                            pcall(function() ctrl:SetControlRotation({ Pitch = pitchTo, Yaw = math.deg(math.atan(dy, dx)), Roll = 0 }) end)
                            -- (its look height is written directly: steering it with look input, as for the player,
                            -- ran it to its stop - straight down - on a bot)
                            pcall(function() c.LookUpValue = pitchTo end)
                            pcall(function() c.MouseSmoothedLookUpValue = pitchTo end)
                            pcall(function() c.LookUpValueSmoothingTarget = pitchTo; c.LookUpValueSmoothingFrom = pitchTo end)
                        end
                    end
                    if not b.demon and not free and os.clock() - (b.focused or 0) > 0.5 then
                        b.focused = os.clock()
                        local ctrl = c.Controller
                        if there(ctrl) then
                            pcall(function() ctrl:StopMovement() end)
                            pcall(function() ctrl:K2_SetFocus(pawn) end)
                        end
                    end
                    -- the Skyrim character is blocking: its stand-in parries the player's attack as it comes in
                    if g.blocking and releaseBegan and not BOT_BRAINS then pcall(function() c:FeintOrBlockPressed() end); b.parryUp = os.clock() + 0.12 end
                    -- The stand-in parries the player's swing as it is let go (sheets/enemy_ai.json standin_skill): with
                    -- its own brain the bot often stood idle or played an emote and never parried at all.
                    -- A Demon Horde stand-in (sheets/creature_fighters.json): its moves that are left out are ended as
                    -- they begin (the Gremlin's leap, the Troll's thrown stone), and it is as big as its Skyrim creature.
                    -- (Both were inside the part below, which is for people's stand-ins only: neither ever ran.)
                    if b.demon then
                        local mine = class_name(c:GetMotion())
                        for _, piece in ipairs(PS_DEMON_BANNED[b.demon] or {}) do
                            if mine:find(piece, 1, true) then
                                pcall(function() c:GetMotion():OnEnded() end)
                                b.banned = (b.banned or 0) + 1
                                if b.banned == 1 or b.banned % 20 == 0 then log("stand-in " .. tostring(b.look) .. " (" .. b.demon .. "): its " .. mine .. " was broken off (" .. b.banned .. " so far)") end
                                break
                            end
                        end
                        local size = PS_ghostSize and PS_ghostSize[b.ghost] or 1
                        if math.abs((b.size or 1) - size) > 0.004 then
                            b.size = size
                            pcall(function() c:SetActorScale3D({ X = size, Y = size, Z = size }) end)
                            log(string.format("stand-in %s (%s) is %.3f of its own size, as tall as its Skyrim creature", tostring(b.look), b.demon, size))
                        end
                    end
                    if not b.demon then
                        local mine = class_name(c:GetMotion())
                        -- (dev, for test fights and recordings: a file cache/autoparry.txt makes the PLAYER parry every
                        -- attack of a stand-in the moment its release begins, by MORDHAU's own parry key)
                        if PS_autoparry == nil then local f = io.open(CACHE .. "autoparry.txt", "r"); PS_autoparry = f ~= nil; if f then f:close() end end
                        if PS_autoparry then
                            local st = -1
                            if mine:find("Strike", 1, true) or mine:find("Stab", 1, true) then pcall(function() st = c:GetMotion().Stage end) end
                            if PS_showcase == nil then local sf = io.open(CACHE .. "showcase.txt", "r"); PS_showcase = sf ~= nil; if sf then sf:close() end end
                            if PS_showcase then
                                pcall(function() pawn.Stamina = 100 end)
                                -- (how near the nearest opponent is, and whether one is attacking: for the repertoire below)
                                local sl = c:K2_GetActorLocation()
                                local d2 = (sl.X - l.X) ^ 2 + (sl.Y - l.Y) ^ 2
                                if not PS_showNear or d2 < PS_showNear then PS_showNear = d2 end
                                if st == 0 or st == 1 then PS_showThreat = os.clock() end
                            end
                            if st == 1 and b.apStage ~= 1 and PS_showcase and (PS_showNear or 1e12) < 320 ^ 2 and ((PS_showDefences or 0) + 1) % 4 == 0 and not mine:find("Stab", 1, true) then   -- (strikes only: both times a stab was chambered in the test takes the weapon was lost)
                                -- a chamber: the same attack, from the side it comes from, as it comes in
                                PS_showDefences = (PS_showDefences or 0) + 1
                                local act = "RightStab"
                                if not mine:find("Stab", 1, true) then
                                    act = (mine:find("Right", 1, true) and "Left" or "Right") .. (mine:find("Upper", 1, true) and "Upper" or (mine:find("Lower", 1, true) and "Lower" or "")) .. "Strike"
                                end
                                PS_showQueue = { { os.clock() + 0.02, act, "chamber with " .. act .. " against " .. mine } }
                            elseif st == 1 and b.apStage ~= 1 then
                                local cl = c:K2_GetActorLocation()
                                if (cl.X - l.X) ^ 2 + (cl.Y - l.Y) ^ 2 < 320 ^ 2 then
                                    if PS_showcase then
                                        -- ... and the answer to a parry that held, from the next angle
                                        PS_showDefences = (PS_showDefences or 0) + 1
                                        PS_showRipostes = (PS_showRipostes or 0) + 1
                                        local angles = { "RightStrike", "LeftUpperStrike", "RightStab", "LeftStrike", "RightUpperStrike", "LeftLowerStrike", "LeftStab", "RightLowerStrike" }
                                        local angle = angles[(PS_showRipostes - 1) % #angles + 1]
                                        pcall(function() PS_riposteHealth = pawn.Health end)
                                        PS_showQueue = { { os.clock() + 0.30, angle, "riposte " .. angle, true } }
                                    end
                                    pcall(function() pawn:FeintOrBlockPressed() end)
                                    releaseBlockAt = os.clock() + 0.12
                                    PS_autoparries = (PS_autoparries or 0) + 1
                                    -- (dev, recordings: cache/autoriposte.txt - a parry that held is answered with a strike)
                                    if PS_autoriposte == nil then local rf = io.open(CACHE .. "autoriposte.txt", "r"); PS_autoriposte = rf ~= nil; if rf then rf:close() end end
                                    if PS_autoriposte and not PS_showcase then
                                        PS_riposteFrom = os.clock() + 0.22; PS_riposteUntil = os.clock() + 0.85
                                        pcall(function() PS_riposteHealth = pawn.Health end)
                                    end
                                    log("autoparry: parry against " .. mine .. " (" .. PS_autoparries .. " so far)")
                                end
                            end
                            b.apStage = st
                        end
                        -- (sheets/builds.json standin_scale: the stand-in is as big against MORDHAU's player as its Skyrim
                        -- character is against Skyrim's - a swing then meets its head and shoulders where Skyrim shows them)
                        -- (sheets/builds.json creature_standin: a creature's stand-in is only there to be hit. Skyrim's creature
                        -- comes nearer to the player than two of MORDHAU's characters can stand (they are 50 cm round), so its
                        -- capsule lets other characters through - the blade is traced against its body, not its capsule.
                        -- Channel 2 = pawns; 0 = ignore, 2 = block.)
                        do
                            local dummy = PS_dummy and PS_dummy[b.ghost] or false
                            if (b.dummy or false) ~= dummy then
                                b.dummy = dummy
                                local ok, err = pcall(function() c.CapsuleComponent:SetCollisionResponseToChannel(2, dummy and 0 or 2) end)
                                log("stand-in " .. tostring(b.look) .. (dummy and " stands in for a creature: it only stands to be hit, and lets characters through" or " stands in for a person again") .. (ok and "" or " (capsule not changed: " .. tostring(err) .. ")"))
                            end
                        end
                        do
                            local size = PS_ghostSize and PS_ghostSize[b.ghost] or 1
                            if math.abs((b.size or 1) - size) > 0.004 then
                                b.size = size
                                pcall(function() c:SetActorScale3D({ X = size, Y = size, Z = size }) end)
                                log(string.format("stand-in %s is %.3f of the player's size, as its Skyrim character", tostring(b.look), size))
                            end
                        end
                        -- (a trail of what it does, for finding the "cower": each change of motion, written out every 6 s)
                        if mine ~= b.trailLast then
                            b.trailLast = mine
                            b.trail = (b.trail or "") .. (b.trail and " > " or "") .. (mine:gsub("^BP_", ""):gsub("Motion_C$", ""):gsub("Motion$", ""))
                        end
                        -- (looking into "i am looking at the top of his head": every 5 s, where this stand-in's feet,
                        -- hips and head are against the player's character inside MORDHAU, and how far its body is shifted)
                        -- (dev: only with a file cache/measure.txt)
                        if PS_measure == nil then local mf = io.open(CACHE .. "measure.txt", "r"); PS_measure = mf ~= nil; if mf then mf:close() end end
                        if PS_measure and os.clock() - (b.pairAt or 0) > 0.1 then
                            -- (the pair of the plugin's "hm" line: the same heights as MORDHAU has them, with its game time)
                            b.pairAt = os.clock()
                            pcall(function()
                                local l, p = c:K2_GetActorLocation(), pawn:K2_GetActorLocation()
                                local hb, hp = c.Mesh:GetSocketLocation(FName("Head")).Z, pawn.Mesh:GetSocketLocation(FName("Head")).Z
                                local pb, pp = c.Mesh:GetSocketLocation(FName("Pelvis")).Z, pawn.Mesh:GetSocketLocation(FName("Pelvis")).Z
                                local fb, fp = c.Mesh:K2_GetComponentToWorld().Translation.Z, pawn.Mesh:K2_GetComponentToWorld().Translation.Z
                                log(string.format("hm gt=%.3f id=%d head=%.1f hips=%.1f feet=%.1f phead=%.1f capsule=%.1f shift=%.1f up=%.1f free=%d ownhead=%.1f", PS_ks:GetGameTimeInSeconds(PS_pc), b.ghost,
                                    hb - hp, pb - pp, fb - fp, hp - fp, l.Z - p.Z, b.lift or 0, g.up, b.free and 1 or 0, hb - fb))
                            end)
                        end
                        if PS_measure and os.clock() - (b.heightAt or 0) > 5 then
                            b.heightAt = os.clock()
                            pcall(function()
                                local l, p = c:K2_GetActorLocation(), pawn:K2_GetActorLocation()
                                local hb, hp = c.Mesh:GetSocketLocation(FName("Head")).Z, pawn.Mesh:GetSocketLocation(FName("Head")).Z
                                local pb, pp = c.Mesh:GetSocketLocation(FName("Pelvis")).Z, pawn.Mesh:GetSocketLocation(FName("Pelvis")).Z
                                log(string.format("heights of stand-in %s against the player in MORDHAU (cm): capsule %+.0f, hips %+.0f, head %+.0f; its head %.0f above its capsule's middle, the player's %.0f; body shift %s; %s",
                                    tostring(b.look), l.Z - p.Z, pb - pp, hb - hp, hb - l.Z, hp - p.Z, tostring(b.lift), b.free and "free" or "held"))
                            end)
                        end
                        if b.trail and os.clock() - (b.trailAt or 0) > 6 then
                            if b.trail:find(">", 1, true) then log("motions of stand-in " .. tostring(b.look) .. ": " .. b.trail:sub(-380)) end
                            b.trail, b.trailAt = (mine:gsub("^BP_", ""):gsub("Motion_C$", ""):gsub("Motion$", "")), os.clock()
                        end
                        local near = math.sqrt(g.forward * g.forward + g.right * g.right) <= 230
                        -- a passive stand-in (its Skyrim character shoots, or does not fight) never attacks: whatever
                        -- attack its brain begins is ended at once
                        if PS_passive and PS_passive[b.ghost] and (mine:find("Strike", 1, true) or mine:find("Stab", 1, true) or mine:find("Kick", 1, true) or mine:find("Parry", 1, true) or mine:find("Block", 1, true) or mine:find("Ranged", 1, true)) then   -- (nor does it parry: archers had parried with their bows out)
                            pcall(function() c:GetMotion():OnEnded() end)
                            b.brokenOff = (b.brokenOff or 0) + 1
                            if b.brokenOff == 1 or b.brokenOff % 20 == 0 then log("stand-in " .. tostring(b.look) .. " is passive: its " .. mine .. " was broken off (" .. b.brokenOff .. " so far)") end
                            mine = "IdleMotion"
                        end
                        -- (MORDHAU's bots yield - kneel with their hands up - and play emotes when they have nothing to do; in
                        -- Skyrim that read as a cower at the start of a fight, still often seen in play after the
                        -- first fix. Any such motion of a stand-in is ended at once, not only under the player's swing.)
                        -- (An emote's own motion is a blueprint class named after the emote - "FistShake_C", the cower -
                        -- with neither word in its name: only the menu step before it was caught, and the emote played.
                        -- Now anything that IS an emote motion, by its class, is ended: in play it was plainly the cower emote
                        -- that the AI was playing.)
                        local emoting = false
                        pcall(function()
                            if not PS_emoteClass or not PS_emoteClass:IsValid() then PS_emoteClass = StaticFindObject("/Script/Mordhau.EmoteMotion") end
                            emoting = PS_emoteClass ~= nil and PS_emoteClass:IsValid() and c:GetMotion():IsA(PS_emoteClass)
                        end)
                        if emoting or mine:find("Yield", 1, true) or mine:find("Emote", 1, true) then
                            pcall(function() c:GetMotion():OnEnded() end)
                            b.yields = (b.yields or 0) + 1
                            if b.yields == 1 or b.yields % 20 == 0 then log("stand-in " .. tostring(b.look) .. ": its " .. mine .. " was ended (" .. b.yields .. " so far)") end
                            -- (MORDHAU's yield lets go of the weapon as it begins - before it can be ended here: bandits
                            -- "do the mordhau cower and lose their weapon". A stop-gap until bots
                            -- cannot yield at all: the stand-in is armed again at once - still holding it, it counts as armed
                            -- in the next frame; without it, a new one is made and picked up.)
                            if mine:find("Yield", 1, true) and b.armed then
                                b.armed, b.armStep, b.armTry, b.armGap, b.armDrops, b.armBegan = false, 1, os.clock(), 0.15, 0, os.clock()
                                log("stand-in " .. tostring(b.look) .. " yielded by itself: it is armed again")
                            end
                            mine = "IdleMotion"
                        end
                        if releaseBegan and near and mine == "IdleMotion" and not (PS_passive and PS_passive[b.ghost]) and math.random() < PS_ai_row(b.skillLevel or PS_ai_level)[1] then
                            -- (the same parry button the player has: RequestParry is refused for a bot)
                            pcall(function() c:FeintOrBlockPressed() end)
                            b.parryUp = os.clock() + 0.12
                            b.parries = (b.parries or 0) + 1
                        end
                        if b.parryUp and os.clock() >= b.parryUp then
                            b.parryUp = nil
                            pcall(function() c:FeintOrBlockReleased() end)
                        end
                    end
                else
                    parked = parked + 1
                    if b.park then PS_place(c, b.park, { Pitch = 0, Yaw = yaw, Roll = 0 }, true) end
                    -- (a Demon Horde creature is only ever spawned for one Skyrim body: with none for 5 s it is
                    -- taken away - left, it was heard roaring beside the player with nothing to be seen)
                    if b.demon then
                        b.idleSince = b.idleSince or os.clock()
                        if os.clock() - b.idleSince > 5 then
                            pcall(function() c:K2_DestroyActor() end)
                            b.dead = true
                            log("demon: MORDHAU's " .. b.demon .. " had no Skyrim body for 5 s: taken away")
                        end
                    end
                end
                -- a hit by the player shows as lost health; note it and put the health back
                local h = c.Health
                if type(h) == "number" then
                    if g and h < (b.health or 100) - 1 then
                        local lost = ((b.health or 100) - h) / DAMAGE_FACTOR
                        -- (the player's only if the player's blade was really near it: within 0.3 s of the player's swing
                        -- an Ogre's stomp hurt a Riverwood man beside it, it counted as the player's hit, and the town
                        -- turned on the player. Off by more than PLAYER_BLADE_NEAR from its middle line: somebody else's.)
                        local bladeOff = 0
                        if playerStriking then
                            pcall(function()
                                local w = pawn.RightHandEquipment
                                local feet = c.Mesh:K2_GetComponentToWorld().Translation
                                local best = 1e9
                                for _, line in ipairs({ { w.CurrentTraceStart, w.CurrentTraceEnd }, { w.PreviousTraceStart, w.PreviousTraceEnd } }) do
                                    local a, e = line[1], line[2]
                                    for i = 0, 20 do
                                        local t = i / 20
                                        best = math.min(best, math.sqrt((a.X + (e.X - a.X) * t - feet.X) ^ 2 + (a.Y + (e.Y - a.Y) * t - feet.Y) ^ 2))
                                    end
                                end
                                bladeOff = best
                            end)
                            if bladeOff > PLAYER_BLADE_NEAR then
                                log(string.format("stand-in %d lost %.0f within the player's swing, but the player's blade was %.0f cm away: not the player's", b.look or 0, lost, bladeOff))
                            end
                        end
                        if playerStriking and bladeOff <= PLAYER_BLADE_NEAR then
                            -- on the head? (the blade's hit line against the head bone, now and a frame ago)
                            local onHead, anyHead = false, false
                            pcall(function()
                                local w = pawn.RightHandEquipment
                                local h = c.Mesh:GetSocketLocation(FName("Head"))
                                local function near(a, e)
                                    local ax, ay, az = e.X - a.X, e.Y - a.Y, e.Z - a.Z
                                    local len2 = ax * ax + ay * ay + az * az
                                    local t = len2 > 1 and math.max(0, math.min(1, ((h.X - a.X) * ax + (h.Y - a.Y) * ay + (h.Z - a.Z) * az) / len2)) or 0
                                    local dx, dy, dz = a.X + ax * t - h.X, a.Y + ay * t - h.Y, a.Z + az * t - h.Z
                                    return math.sqrt(dx * dx + dy * dy + dz * dz)
                                end
                                local d = math.min(near(w.CurrentTraceStart, w.CurrentTraceEnd), near(w.PreviousTraceStart, w.PreviousTraceEnd))
                                onHead = false   -- (decided below, by where the blade meets the body: sheets/gore.json head_rule)
                                b.headNote = string.format("%.0f cm from the head", d)
                                -- (looking into torso hits that counted as head hits: where on the body the blade
                                -- is - the point of its hit line nearest the body's upright middle line - and the heights)
                                pcall(function()
                                    local feet = c.Mesh:K2_GetComponentToWorld().Translation
                                    local mine = pawn.Mesh:K2_GetComponentToWorld().Translation
                                    local chest = c.Mesh:GetSocketLocation(FName("Spine1"))
                                    local best, at = 1e9, 0
                                    for _, line in ipairs({ { w.CurrentTraceStart, w.CurrentTraceEnd }, { w.PreviousTraceStart, w.PreviousTraceEnd } }) do
                                        local a, e = line[1], line[2]
                                        for i = 0, 20 do
                                            local t = i / 20
                                            local x, y, z = a.X + (e.X - a.X) * t, a.Y + (e.Y - a.Y) * t, a.Z + (e.Z - a.Z) * t
                                            local off = math.sqrt((x - feet.X) ^ 2 + (y - feet.Y) ^ 2)
                                            if off < best then best, at = off, z end
                                        end
                                    end
                                    -- (the headshot itself: by the upright through the HEAD BONE, not through the feet - a fighter's
                                    -- head is rarely over its feet, and blows plainly on the head were "20-48 cm off the middle line",
                                    -- Some point of the hit line at head height passes within HEAD_SPINE of it.)
                                    local nearHead = 1e9
                                    for _, line in ipairs({ { w.CurrentTraceStart, w.CurrentTraceEnd }, { w.PreviousTraceStart, w.PreviousTraceEnd } }) do
                                        local a, e = line[1], line[2]
                                        for i = 0, 40 do
                                            local t = i / 40
                                            local x, y, z = a.X + (e.X - a.X) * t, a.Y + (e.Y - a.Y) * t, a.Z + (e.Z - a.Z) * t
                                            if z >= h.Z - HEAD_LINE and z <= h.Z + HEAD_TOP then
                                                nearHead = math.min(nearHead, math.sqrt((x - h.X) ^ 2 + (y - h.Y) ^ 2))
                                            end
                                        end
                                    end
                                    onHead = nearHead < HEAD_SPINE
                                    anyHead = nearHead < HEAD_ANY
                                    b.headNote = b.headNote .. string.format("; at head height the blade passes %.0f cm from the head's upright", nearHead < 1e8 and nearHead or -1)
                                    local eye = PS_pc.PlayerCameraManager:GetCameraLocation()
                                    b.headNote = b.headNote .. (onHead and "; ON THE HEAD" or (anyHead and "; a headshot, beside the neck" or "; not on the head")) .. string.format("; blade meets the body %.0f cm above its feet (%.0f cm off its middle line); its head bone %.0f and chest %.0f above its feet; its feet %.0f against the player's; body shift %.0f; the player's eye %.0f above his feet, pitch %.0f",
                                        at - feet.Z, best, h.Z - feet.Z, chest.Z - feet.Z, feet.Z - mine.Z, b.lift or 0, eye.Z - mine.Z, PS_pc:GetControlRotation().Pitch)
                                end)
                            end)
                            -- (1000 added to the damage = on the head, through the neck or skull; 2000 = a headshot beside that)
                            b.hits = b.hits + 1; b.damage = lost + (onHead and 1000 or (anyHead and 2000 or 0))
                        else
                            -- not the player's doing: another stand-in's blade (two Skyrim characters fighting each other).
                            -- Which one: the stand-in in an attack right now, the nearest if there are two.
                            local by, best = 0, 1e9
                            local bl = c:K2_GetActorLocation()
                            for _, o in ipairs(bots) do
                                if o ~= b and o.ghost and not o.dead and o.obj:IsValid() then
                                    local name = class_name(o.obj:GetMotion())
                                    if name:find("Strike") or name:find("Stab") or name:find("Kick") or name:find("AbilityMotion") then
                                        local ol = o.obj:K2_GetActorLocation()
                                        local d = (ol.X - bl.X) ^ 2 + (ol.Y - bl.Y) ^ 2
                                        if d < best then best = d; by = o.ghost end
                                    end
                                end
                            end
                            if by ~= 0 then
                                b.otherHits = (b.otherHits or 0) + 1; b.otherDamage = lost; b.otherBy = by
                            end
                        end
                        log(string.format("stand-in %d took a hit of %.0f (%s)", b.look or 0, lost, playerStriking and ("the player's, " .. tostring(b.headNote)) or ((b.otherBy or 0) ~= 0 and "another stand-in's" or "nobody's: ignored")))
                    end
                    local want = g and math.max(HEALTH_FLOOR, math.min(100, math.floor(g.hp + 0.5))) or 100
                    if h ~= want then c:OffsetHealth(want - h, false) end
                    -- (what it really has now: a Demon Horde creature did not take the health given back, and the
                    -- difference counted again as a fresh hit every frame - "swinging at its feet hits it 100 times")
                    local now = c.Health
                    b.health = (type(now) == "number" and now < want) and now or want
                end
            end)
            ::continue::
        end
    end
    -- the player: a hit shows as lost health; note who did it (the stand-in that is attacking), put the health back
    pcall(function()
        local h = pawn.Health
        if type(h) == "number" and h < (playerHealth or 100) - 1 then
            playerHits = playerHits + 1
            playerDamage = ((playerHealth or 100) - h) / DAMAGE_FACTOR
            playerAttacker = 0
            for _, b in ipairs(bots) do
                if b.ghost and b.obj:IsValid() then
                    local name = class_name(b.obj:GetMotion())
                    if name:find("Strike") or name:find("Stab") or name:find("AbilityMotion") then playerAttacker = b.ghost end
                end
            end
        end
        if type(h) == "number" then
            local want = os.clock() - input.stamp < 0.5 and math.max(HEALTH_FLOOR, math.min(100, math.floor((input.health or 100) + 0.5))) or 100
            if h ~= want then pawn:OffsetHealth(want - h, false) end
            playerHealth = want
        end
    end)
end

local function ghost_block()
    local parts = { string.pack("<I4fI4", playerHits, playerDamage, playerAttacker) }
    local n = 0
    for _, b in ipairs(bots) do
        if b.ghost and not b.dead and n < MAX_GHOSTS then
            n = n + 1
            local stage = -1
            pcall(function()
                if not there(b.obj) then return end
                stage = motion_stage(b.obj:GetMotion()) or -1
                if fallen(b.obj) then stage = FALLEN end
            end)
            parts[#parts + 1] = string.pack("<I4I4fi4I4fI4", b.ghost, b.hits, b.damage, stage, b.otherHits or 0, b.otherDamage or 0, b.otherBy or 0)
        end
    end
    for _ = n + 1, MAX_GHOSTS do parts[#parts + 1] = string.pack("<I4I4fi4I4fI4", 0, 0, 0, -1, 0, 0, 0) end
    -- (parts[1] is the player's own numbers, 2-4 the first three stand-ins; the others go to the end of the state block)
    PS_ghostExtra = table.concat(parts, "", 5)
    return table.concat(parts, "", 1, 4)
end

-- The weapon a character holds or last held, and where it is now: 1 in the hand, 2 lying in the world (dropped,
-- disarmed), 3 put away on the character (holstered). Its transform in the character's own space rides along, so
-- Skyrim can draw it where MORDHAU has it. `state` remembers the weapon between calls.
function PS_weapon_of(c, mesh, state)
    local wq, wt, has = { 0, 0, 0, 1 }, { 0, 0, 0 }, 0
    pcall(function()
        local w = c.RightHandEquipment
        local inHand = there(w) and there(w.SkeletalMeshComponent) and not class_name(w):find("Fists", 1, true)
        if not inHand then
            -- (a bow is carried in the other hand)
            local l = c.LeftHandEquipment
            if there(l) and there(l.SkeletalMeshComponent) and class_name(l):lower():find("bow", 1, true) then
                w = l; inHand = true
            end
        end
        if inHand then
            state.held = w; has = 1
        else
            w = state.held
            if not (there(w) and there(w.SkeletalMeshComponent)) then state.held = nil; return end
            local parent = nil
            pcall(function() parent = w:GetAttachParentActor() end)
            if there(parent) then
                if parent:GetFullName() ~= c:GetFullName() then state.held = nil; return end   -- someone else has it now
                has = 3
            else
                has = 2
            end
        end
        if has ~= state.has then
            state.has = has; state.since = os.clock()
        end
        local T = mesh:K2_GetComponentToWorld()
        local W = w.SkeletalMeshComponent:K2_GetComponentToWorld()
        local a, q = T.Rotation, W.Rotation
        local ax, ay, az, aw = -a.X, -a.Y, -a.Z, a.W       -- inverse of the character's rotation
        wq = { aw * q.X + ax * q.W + ay * q.Z - az * q.Y, aw * q.Y - ax * q.Z + ay * q.W + az * q.X,
               aw * q.Z + ax * q.Y - ay * q.X + az * q.W, aw * q.W - ax * q.X - ay * q.Y - az * q.Z }
        wt = { to_local(T, W.Translation) }
    end)
    if has == 0 then state.has = 0 end
    return wq, wt, has
end

-- The player's Skyrim weapon as its MORDHAU counterpart (" pw:<path>"): handed over when it changes. What MORDHAU's
-- character held is dropped and taken out of the world, the new weapon is made at its feet and picked up.
-- The weapon in the hand, and weapons left in the slots by earlier hand-overs, are taken out of MORDHAU's world without
-- a motion. (Before, the old weapon was let go with MORDHAU's drop, and a switch to fists with a weapon that had found
-- no free slot dropped it too.)
function PS_silent_unhand(pawn)
    local n = 0
    pcall(function()
        local e = pawn.Equipment
        for i = 1, #e do
            local w = e[i]
            if there(w) then
                local cn = class_name(w)
                if not cn:find("Shield", 1, true) and not cn:find("Fists", 1, true) then w:K2_DestroyActor(); n = n + 1 end
            end
        end
    end)
    for _, hand in ipairs({ "RightHandEquipment", "LeftHandEquipment" }) do
        pcall(function()
            local w = pawn[hand]
            if there(w) then
                local cn = class_name(w)
                if not cn:find("Shield", 1, true) and not cn:find("Fists", 1, true) then w:K2_DestroyActor(); n = n + 1 end
            end
        end)
    end
    if n > 0 then
        PS_unhandAt = os.clock()   -- (taken away, not dropped: see where the weapon's state is told)
        if PS_playerWeapon then PS_playerWeapon.held = nil; PS_playerWeapon.has = 0 end
    end
    return n
end
function PS_player_arm(pawn)
    -- (a bow or crossbow out in Skyrim comes as input.weaponWish, relative to Equipment/; melee as playerWeapon, relative to Equipment/Weapons/)
    local ranged = input.weaponWish ~= nil and input.weaponWish ~= ""
    local path = ranged and input.weaponWish or input.playerWeapon
    local base = ranged and "/Game/Mordhau/Blueprints/Equipment/" or WEAPONS
    local life = pawn:GetFullName()
    if PS_armLife ~= life then PS_armLife = life; PS_armedFor = nil; PS_playerArm = nil end
    if not path or path == "" or path == PS_armedFor then return end
    if path == "Fists" then
        -- bare hands in Skyrim: MORDHAU's own switch to fists (asked again every second until it has happened)
        local have = class_name(pawn.RightHandEquipment)
        if have == "" or have:find("Fists", 1, true) then
            PS_armedFor = path; PS_playerArm = nil
            log("the player fights with its fists (nothing in the hand in Skyrim)")
        elseif os.clock() - (PS_fistsAsk or 0) > 1.0 and class_name(pawn:GetMotion()) == "IdleMotion" then
            PS_fistsAsk = os.clock()
            local gone = PS_silent_unhand(pawn)
            pcall(function() pawn:SwitchToFists() end)
            log("fists: " .. gone .. " weapon(s) taken away without a drop")
        end
        return
    end
    local A = PS_playerArm
    if not A or A.path ~= path then A = { path = path, step = 0, t = 0 }; PS_playerArm = A end
    if os.clock() - A.t < 0.8 then return end
    if class_name(pawn:GetMotion()) ~= "IdleMotion" then return end
    A.t = os.clock()
    local want = path:match("([%w_]+)$") .. "_C"
    local have = class_name(pawn.RightHandEquipment)
    local _, bowName = ranged_item(pawn)   -- (a bow sits in the left hand)
    if bowName ~= "" then have = bowName end
    if have == want then
        PS_armedFor = path; PS_playerArm = nil
        log("the player holds " .. want .. " (for the Skyrim weapon)")
        return
    end
    A.step = A.step + 1
    if A.step == 1 then
        if have ~= "" and not have:find("Fists", 1, true) then PS_silent_unhand(pawn); A.t = os.clock() - 0.55 else A.step = 2 end
    end
    if A.step == 2 and have ~= "" and not have:find("Fists", 1, true) then
        A.step = 0; A.tries = (A.tries or 0) + 1   -- (the old weapon is still in the hand: drop it again)
        if A.tries > 4 then PS_armedFor = path; PS_playerArm = nil; log("could not free the player's hands for " .. want) end
        return
    end
    if A.step == 2 then
        if there(PS_playerHeld) and PS_playerWeapon and PS_playerWeapon.has == 2 then
            pcall(function() PS_playerHeld:K2_DestroyActor() end)
            PS_playerWeapon.held = nil
        end
        local full = base .. path .. "." .. path:match("([%w_]+)$") .. "_C"
        local cls = StaticFindObject(full)
        if not there(cls) then pcall(LoadAsset, base .. path); cls = StaticFindObject(full) end
        if there(cls) then
            local l = pawn:K2_GetActorLocation()
            local w = pawn:GetWorld():SpawnActor(cls, { X = l.X, Y = l.Y, Z = l.Z + 60 }, { Pitch = 0, Yaw = 0, Roll = 0 })
            PS_finish_weapon(w)
            A.spawned = w
            if there(w) then pawn:PickUp(w, false) end
        else
            PS_armedFor = path; PS_playerArm = nil
            log("no MORDHAU weapon class " .. full .. "; the player keeps what it holds")
        end
    elseif A.step >= 3 and A.step <= 5 then
        pawn["EquipSlot" .. (A.step - 2)](pawn)   -- (picked up into a slot instead of the hand: bring it out)
    elseif A.step > 6 then
        -- (a weapon made for nothing is taken out of MORDHAU's world: it showed MORDHAU's "pick up" prompt in Skyrim)
        pcall(function()
            local w = A.spawned
            if there(w) and not there(w:GetAttachParentActor()) then w:K2_DestroyActor() end
        end)
        -- (it came out with another weapon in the hand - e.g. the hand-over began while the old one was put away, so
        -- nothing was dropped and the slots brought the old one back: start over, now that there is something to drop)
        A.rounds = (A.rounds or 0) + 1
        if A.rounds <= 3 then
            A.step = 0; A.tries = 0; A.spawned = nil
            log("handing the player " .. want .. " again (round " .. A.rounds .. "): it holds " .. (have ~= "" and have or "nothing"))
            return
        end
        PS_armedFor = path; PS_playerArm = nil
        log("could not hand the player " .. want .. "; it keeps " .. have)
    end
end
-- The shield on the left arm (32 bytes; sheets/live_link.json shield_pose): carried or not, and where it is in
-- character space, for Skyrim to draw MORDHAU's own model of it there.
function PS_shield_block(pawn, mesh)
    local has, q, t = 0, { 0, 0, 0, 1 }, { 0, 0, 0 }
    pcall(function()
        local l = pawn.LeftHandEquipment
        if not (there(l) and class_name(l):find("Shield", 1, true) and there(l.SkeletalMeshComponent)) then return end
        local T = mesh:K2_GetComponentToWorld()
        local W = l.SkeletalMeshComponent:K2_GetComponentToWorld()
        local a, r = T.Rotation, W.Rotation
        local ax, ay, az, aw = -a.X, -a.Y, -a.Z, a.W
        q = { aw * r.X + ax * r.W + ay * r.Z - az * r.Y, aw * r.Y - ax * r.Z + ay * r.W + az * r.X,
              aw * r.Z + ax * r.Y - ay * r.X + az * r.W, aw * r.W - ax * r.X - ay * r.Y - az * r.Z }
        t = { to_local(T, W.Translation) }
        local cn = class_name(l)
        has = cn:find("Heater", 1, true) and 2 or cn:find("Targe", 1, true) and 3 or cn:find("Buckler", 1, true) and 4 or cn:find("Round", 1, true) and 5 or 1
    end)
    -- (+256: the weapon in the right hand is in its alternate mode - its other grip)
    pcall(function() if pawn.RightHandEquipment.bIsUsingAlternateMode == true then has = has + 256 end end)
    return string.pack("<i4fffffff", has, q[1], q[2], q[3], q[4], t[1], t[2], t[3])
end
-- The bow in the hand, as MORDHAU poses it (428 bytes; sheets/live_link.json bow_pose): number of bones (0 = no bow),
-- 12 bones x 7 floats in the bow's own space (its limbs and string), then the arrow on the string: shown or not, where
-- its part sits in the bow's space, and its 2 bones in its own space.
local BOW_EMPTY = string.rep("\0", 428)
local RTS_Component = 2
function PS_bow_block(pawn)
    local out = nil
    pcall(function()
        local e = pawn.LeftHandEquipment
        if not there(e) then return end
        local cn = class_name(e)
        if not cn:lower():find("bow", 1, true) then return end
        local m = e.SkeletalMeshComponent
        if not there(m) then return end
        local key = e:GetFullName()
        if PS_bowKey ~= key then
            PS_bowKey = key; PS_bowNames = {}; PS_arrowNames = {}
            for i = 0, math.min(m:GetNumBones(), 12) - 1 do PS_bowNames[#PS_bowNames + 1] = m:GetBoneName(i) end
            local mi = e.Missile
            if there(mi) then for i = 0, math.min(mi:GetNumBones(), 2) - 1 do PS_arrowNames[#PS_arrowNames + 1] = mi:GetBoneName(i) end end
            log("archery: the bow's " .. #PS_bowNames .. " bones and the arrow's " .. #PS_arrowNames .. " are streamed")
        end
        local parts = { string.pack("<i4", #PS_bowNames) }
        for i = 1, 12 do
            if PS_bowNames[i] then
                local t = m:GetSocketTransform(PS_bowNames[i], RTS_Component)
                local r, q = t.Rotation, t.Translation
                parts[#parts + 1] = string.pack("<fffffff", r.X, r.Y, r.Z, r.W, q.X, q.Y, q.Z)
            else
                parts[#parts + 1] = string.pack("<fffffff", 0, 0, 0, 1, 0, 0, 0)
            end
        end
        local mi = e.Missile
        local shown, rel = 0, string.pack("<fffffff", 0, 0, 0, 1, 0, 0, 0)
        local bones = { rel, rel }
        if there(mi) and #PS_arrowNames > 0 then
            local vis = false
            pcall(function() vis = mi:IsVisible() end)
            if vis then
                shown = 1
                local B, A = m:K2_GetComponentToWorld(), mi:K2_GetComponentToWorld()
                local a, q = B.Rotation, A.Rotation
                local ax, ay, az, aw = -a.X, -a.Y, -a.Z, a.W
                local x, y, z = to_local(B, A.Translation)
                rel = string.pack("<fffffff", aw * q.X + ax * q.W + ay * q.Z - az * q.Y, aw * q.Y - ax * q.Z + ay * q.W + az * q.X,
                    aw * q.Z + ax * q.Y - ay * q.X + az * q.W, aw * q.W - ax * q.X - ay * q.Y - az * q.Z, x, y, z)
                for i = 1, 2 do
                    if PS_arrowNames[i] then
                        local t = mi:GetSocketTransform(PS_arrowNames[i], RTS_Component)
                        local r, v = t.Rotation, t.Translation
                        bones[i] = string.pack("<fffffff", r.X, r.Y, r.Z, r.W, v.X, v.Y, v.Z)
                    end
                end
            end
        end
        parts[#parts + 1] = string.pack("<i4", shown)
        parts[#parts + 1] = rel
        parts[#parts + 1] = bones[1]
        parts[#parts + 1] = bones[2]
        out = table.concat(parts)
    end)
    return (out and #out == 428) and out or BOW_EMPTY
end
function PS_wall_is_up(p)
    local up = false
    pcall(function() local m = p:GetMotion(); up = class_name(m):find("ShieldParry", 1, true) ~= nil and m.Stage == 0 end)
    return up
end
function PS_wall_up(p)
    local l = p.LeftHandEquipment
    if not (there(l) and class_name(l):find("Shield", 1, true)) then
        p:FeintOrBlockPressed(); releaseBlockAt = os.clock() + 0.12   -- (no shield in MORDHAU's hand: the timed parry)
        return
    end
    -- As in MORDHAU: the press itself is the shield's timed parry (its 0.38 s window, ripostes and all). Only if the
    -- button is still held when that parry is over does the shield wall go up (PS_wall_keep) - MORDHAU has the wall
    -- on its own key (R, which works here too); holding the block button for it is this mashup's addition.
    PS_wallAsk = os.clock()
    if not PS_wall_is_up(p) then p:FeintOrBlockPressed(); releaseBlockAt = os.clock() + 0.12 end
end
function PS_wall_raise(p)
    PS_wallAsk = os.clock()
    if not PS_wall_is_up(p) then pcall(function() p:RequestToggleWeaponMode() end) end
end
function PS_wall_down(p)
    if PS_wall_is_up(p) then
        p:FeintOrBlockPressed(); releaseBlockAt = os.clock() + 0.05
    end
end
-- (held, but the wall has come down - after it took a blow, or it was asked in the middle of another motion: up again)
function PS_wall_keep(p)
    if not PS_blockHeld or os.clock() - (PS_wallAsk or 0) < 0.35 then return end
    local idle = false
    pcall(function() idle = class_name(p:GetMotion()) == "IdleMotion" end)
    local l = p.LeftHandEquipment
    if idle and there(l) and class_name(l):find("Shield", 1, true) then PS_wall_raise(p) end
end
-- A shield on the arm in Skyrim: MORDHAU's character carries one too (its held block then works by MORDHAU's shield
-- rules); taken away again when Skyrim's is.
function PS_player_shield(pawn)
    -- (PickUp leaves a spawned shield on the ground, and PickUp(w, true) crashed MORDHAU; PickUpToSlot(w, 2) puts it in
    -- the third slot and onto the arm - found)
    local fresh = os.clock() - input.stamp < 0.5
    local want = fresh and input.shield and not (input.weaponWish and input.weaponWish ~= "") and input.shield or nil
    if want == "1" then want = "BP_KiteShield" end
    local l = pawn.LeftHandEquipment
    local have = (there(l) and class_name(l):find("Shield", 1, true)) and class_name(l) or nil
    if PS_shieldBring and os.clock() >= PS_shieldBring then
        PS_shieldBring = nil
        if want and not have then pcall(function() pawn:NextShieldEquipment() end) end   -- (in its slot but not on the arm yet)
    end
    if os.clock() - (PS_shieldTry or -10) < 2.0 or PS_playerArm then return end
    if class_name(pawn:GetMotion()) ~= "IdleMotion" then return end
    if have and (not want or have ~= want .. "_C") then
        -- none on the arm in Skyrim any more, or a different kind of shield: this one goes
        PS_shieldTry = os.clock() - 1.5
        PS_shieldStep = 0
        pcall(function() l:K2_DestroyActor() end)
        log("shield: " .. have .. " taken away (" .. (want and ("Skyrim's shield is a " .. want .. " now") or "none on the arm in Skyrim") .. ")")
    elseif want and not have then
        PS_shieldTry = os.clock()
        if PS_shieldWant ~= want then PS_shieldWant = want; PS_shieldStep = 0 end   -- (another shield: its own tries)
        PS_shieldStep = (PS_shieldStep or 0) + 1
        if PS_shieldStep > 8 then if PS_shieldStep == 9 then log("shield: could not give the player a " .. want) end return end
        -- (already picked into its slot on an earlier try, only not on the arm yet: bring it out, do not make another)
        local mine = false
        pcall(function()
            if there(PS_shieldActor) and class_name(PS_shieldActor) == want .. "_C" then
                local par = PS_shieldActor:GetAttachParentActor()
                mine = there(par) and par:GetFullName() == pawn:GetFullName()
            end
        end)
        if mine then
            pcall(function() pawn:NextShieldEquipment() end)
            log("shield: " .. want .. " is in its slot; brought out again (try " .. PS_shieldStep .. ")")
            return
        end
        local full = "/Game/Mordhau/Blueprints/Equipment/Shields/" .. want .. "." .. want .. "_C"
        local cls = StaticFindObject(full)
        if not there(cls) then pcall(LoadAsset, "/Game/Mordhau/Blueprints/Equipment/Shields/" .. want); cls = StaticFindObject(full) end
        if there(cls) then
            local at = pawn:K2_GetActorLocation()
            local w = pawn:GetWorld():SpawnActor(cls, { X = at.X, Y = at.Y, Z = at.Z + 60 }, { Pitch = 0, Yaw = 0, Roll = 0 })
            PS_finish_weapon(w)
            if there(PS_shieldActor) then pcall(function() local par = PS_shieldActor:GetAttachParentActor(); if not there(par) then PS_shieldActor:K2_DestroyActor() end end) end
            PS_shieldActor = w
            if there(w) then
                -- a free slot for it (0-2): an empty one, else one holding a weapon left over from an earlier hand-over
                -- (never the one in the hand) - that leftover is taken out of the world. With all three slots taken the
                -- shield simply stayed on the ground (hand axe, short sword, bastard sword).
                local slot = nil
                pcall(function()
                    local e = pawn.Equipment
                    local held = there(pawn.RightHandEquipment) and pawn.RightHandEquipment:GetFullName() or ""
                    for i = 1, math.min(#e, 3) do
                        if not slot and not there(e[i]) then slot = i - 1 end
                    end
                    for i = math.min(#e, 3), 1, -1 do
                        if not slot and there(e[i]) and e[i]:GetFullName() ~= held then
                            log("shield: slot " .. i .. " freed (a left-over " .. class_name(e[i]) .. ")")
                            e[i]:K2_DestroyActor()
                            slot = i - 1
                        end
                    end
                end)
                local ok = pcall(function() pawn:PickUpToSlot(w, slot or 2) end)
                if ok then PS_shieldBring = os.clock() + 0.6 end
            end
            log("shield: " .. want .. " handed to the player (try " .. PS_shieldStep .. ")")
        else
            log("shield: MORDHAU has no " .. full)
        end
    elseif have and want then
        if PS_shieldStep and PS_shieldStep > 0 then
            log("shield: the player carries " .. have); PS_shieldStep = 0
            -- (not drawn in MORDHAU's own view: its picture got into the strip of MORDHAU's window that Skyrim shows)
            pcall(function() l.SkeletalMeshComponent:SetVisibility(false, true) end)
            pcall(function() l.StaticMeshComponent:SetVisibility(false, true) end)
        end
    end
end
-- the class of the weapon the player holds or last held (32 bytes), for Skyrim to draw the right model
function PS_player_weapon_block()
    local name = ""
    pcall(function() if there(PS_playerHeld) then name = class_name(PS_playerHeld) end end)
    return string.pack("<c32", name:sub(1, 31))
end

local ghostTick = 0
function PS_ghost_poses()
    ghostTick = ghostTick + 1
    local out = {}
    local n = 0
    for _, b in ipairs(bots) do
        if b.ghost and not b.dead and n < MAX_GHOSTS then
            n = n + 1
            local wq, wt, hasWeapon = { 0, 0, 0, 1 }, { 0, 0, 0 }, 0
            pcall(function()
                if not there(b.obj) then return end
                -- (one that is dying or already limp in MORDHAU is no longer read: its replacement is on the way)
                local alive = true
                pcall(function() alive = b.obj.Health > 0 and not b.obj.bIsDead end)
                if not alive then return end
                local mesh = b.obj.Mesh
                if not there(mesh) then return end
                -- a stand-in that is doing something (attacking, parrying, flinching) has its body sent with every pose
                -- written, so what Skyrim shows is not behind what MORDHAU judges; one that stands idle every third
                local busy = class_name(b.obj:GetMotion()) ~= "IdleMotion"
                if busy or PS_standins == 1 or ghostTick % 5 == (n - 1) % 5 or not b.read then   -- (an idle one every fifth pose, as with five)
                    read_bones(mesh, BODY, b.parts)
                    if not b.read or ghostTick % 45 == n - 1 then read_bones(mesh, FINGERS, b.parts) end
                    b.read = true
                end
                b.weapon = b.weapon or {}
                wq, wt, hasWeapon = PS_weapon_of(b.obj, mesh, b.weapon)
                -- (what a bot lets go of while it is being handed its Skyrim character's weapon is not a disarm)
                if hasWeapon == 2 and not b.armed and not b.armGaveUp then hasWeapon = 0 end
                -- disarmed: like a MORDHAU bot it goes without for a while, then has its weapon in hand again
                if hasWeapon == 2 and b.weapon.has == 2 and os.clock() - (b.weapon.since or 0) > 12.0 and not b.demon then
                    b.weapon.since = os.clock()
                    log("stand-in " .. tostring(b.look) .. " picks its weapon up again")
                    b.obj:PickUp(b.weapon.held, false)
                end
            end)
            out[#out + 1] = string.pack("<i4", b.read and b.look or 0) .. table.concat(b.parts) ..
                string.pack("<fffffffi4", wq[1], wq[2], wq[3], wq[4], wt[1], wt[2], wt[3], hasWeapon)
        end
    end
    PS_standins = n
    local empty = nil
    for _ = n + 1, MAX_GHOSTS do
        empty = empty or (string.pack("<i4", 0) .. string.rep(ZERO_BONE, #names) .. string.pack("<fffffffi4", 0, 0, 0, 1, 0, 0, 0, 0))
        out[#out + 1] = empty
    end
    return table.concat(out)
end

local function state_block(pawn, mesh)
    local sx, sy, sz, ex, ey, ez = 0, 0, 0, 0, 0, 0
    local stage, kind, elapsed, name = -1, 0, 0, ""
    local vr, vf = 0, 0
    pcall(function()
        local T = mesh:K2_GetComponentToWorld()
        local w = pawn.RightHandEquipment
        if w and w:IsValid() then
            sx, sy, sz = to_local(T, w.CurrentTraceStart)
            ex, ey, ez = to_local(T, w.CurrentTraceEnd)
        end
        local v = pawn:GetVelocity()
        local t = T.Translation
        -- mesh space: +Y is the way the character faces, -X is its right
        local lx, ly = to_local(T, { X = t.X + v.X, Y = t.Y + v.Y, Z = t.Z })
        vr, vf = -lx, ly
    end)
    pcall(function()
        local m = pawn:GetMotion()
        if m and m:IsValid() then
            name = class_name(m)
            local s = motion_stage(m)
            if s then stage = s; kind = 1 end
            -- a parry has stages too: 0 while it can stop a hit, then its recovery
            if name:find("Parry") or name:find("Block") then kind = 2 end
            if name:find("RagdollFalling", 1, true) then stage = 7; kind = 0 end
            local st = m.StartRealTime
            if type(st) == "number" then
                GS = GS and GS:IsValid() and GS or StaticFindObject("/Script/Engine.Default__GameplayStatics")
                elapsed = GS:GetRealTimeSeconds(pawn) - st
            end
        end
    end)
    -- the held weapon's own transform in character space (its model is drawn rigidly with this)
    PS_playerWeapon = PS_playerWeapon or {}
    local lifeKey = pawn:GetFullName()
    if PS_playerWeapon.pawn ~= lifeKey then PS_playerWeapon = { pawn = lifeKey } end   -- (a new life: nothing held before)
    local wq, wt, hasWeapon = PS_weapon_of(pawn, mesh, PS_playerWeapon)
    PS_playerHeld = PS_playerWeapon.held
    -- (the old weapon let go while a new one is handed over is not a disarm: Skyrim laid a copy of the equipped
    -- weapon on the ground for it - "Take Hunting Bow" at every switch to the bow)
    if hasWeapon == 2 and PS_playerArm then hasWeapon = 3 end
    -- (nor is a thrown weapon: it is in the air, then lies in Skyrim as the Skyrim item - no MORDHAU weapon to take up)
    if hasWeapon == 2 and (name:find("Ranged", 1, true) or os.clock() - (PS_thrownAt or -100) < 8 or os.clock() - (PS_unhandAt or -100) < 3) then hasWeapon = 0 end
    if hasWeapon ~= PS_playerHasLogged then
        PS_playerHasLogged = hasWeapon
        log("the player's weapon is " .. (({ [0] = "gone", "in the hand", "on the ground (dropped or disarmed)", "put away" })[hasWeapon] or "?"))
    end
    local stamina, health = 0, 0
    pcall(function() stamina = pawn.Stamina; health = pawn.Health end)
    return string.pack("<ffffffi4i4fffffc48", sx, sy, sz, ex, ey, ez, stage, kind, elapsed, stamina, health, vr, vf, name:sub(1, 47))
        .. string.pack("<fffffffi4", wq[1], wq[2], wq[3], wq[4], wt[1], wt[2], wt[3], hasWeapon)
end

local playerParts, playerKey, streamTick = {}, nil, 0
local function stream(pawn, mesh)
    local parts = playerParts
    streamTick = streamTick + 1
    local ok, err = pcall(function()
        local key = pawn:GetFullName()
        if key ~= playerKey or #parts < #names or streamTick % 60 == 0 then
            playerKey = key
            read_bones(mesh, STILL, parts)
        end
        read_bones(mesh, BODY, parts)
        read_bones(mesh, FINGERS, parts)
    end)
    if not ok then
        if not reported then reported = true; log("reading bones failed: " .. tostring(err)) end
        return
    end
    sequence = sequence + 1
    PS_outFile = PS_outFile or shared(OUT, POSE_BYTES)
    local f = PS_outFile
    if f then
        -- (60 zero bytes after the shot block: the bear trap and toolbox / firebomb blocks were here; kept so nothing after them moves)
        -- (the time of this report is MORDHAU's own game time - it advances with the game's frames, which the places and
        -- poses in it belong to; the script's own clock runs between them. sheets/builds.json hybrid_smooth)
        local stamp = os.clock()
        pcall(function() if PS_ks and PS_ks:IsValid() and there(PS_pc) then stamp = PS_ks:GetGameTimeInSeconds(PS_pc) end end)
        local block = table.concat({ string.pack("<c4I4fi4", "DMLP", sequence, stamp, #names), table.concat(parts), state_block(pawn, mesh), ghost_block(),
            PS_camera_block(pawn, PS_pc, mesh), PS_shot_block(), string.rep("\0", 60), PS_player_weapon_block(), PS_bow_block(pawn), PS_ghostExtra or string.rep("\0", (MAX_GHOSTS - 3) * 28), PS_shield_block(pawn, mesh), PS_move_block(), PS_ghost_poses(), string.pack("<I4", sequence) })
        f:seek("set", 0)
        f:write(block)
        f:flush()
    end
    if not reported then
        reported = true
        log(string.format("streaming %d bones of %s", #names, pawn:GetFullName()))
        -- MORDHAU's own 3D picture is never seen beside Skyrim: draw it small and plain (sheets/live_link.json
        -- mordhau_render_load; the HUD and menus Skyrim shows are not affected, nothing is saved to MORDHAU's settings)
        pcall(function()
            local ksl, pc = StaticFindObject("/Script/Engine.Default__KismetSystemLibrary"), pawn.Controller
            if not there(ksl) or not there(pc) then return end
            for _, c in ipairs({ "r.ScreenPercentage 25", "sg.ShadowQuality 0", "sg.PostProcessQuality 0", "sg.EffectsQuality 0" }) do
                ksl:ExecuteConsoleCommand(pc, c, pc)
            end
            log("MORDHAU's hidden 3D picture set small and plain")
        end)
    end
end

-- Skyrim owns where the player is. MORDHAU's character walks on the spot: every frame it is put back where it
-- spawned (height left alone, for jumps), keeping its speed, so it animates and never reaches the arena's walls. Its
-- stand-ins are placed relative to it right afterwards, so nothing shifts between them.
local function keep_near_home(pawn)
    local ok, err = pcall(function()
        local p = pawn:K2_GetActorLocation()
        if not home then home = { X = p.X, Y = p.Y, Z = p.Z }; return end
        local dx, dy = p.X - home.X, p.Y - home.Y
        if dx * dx + dy * dy > 25 then
            PS_place_at(pawn, { X = home.X, Y = home.Y, Z = p.Z })
        end
    end)
    if not ok and not leashReported then leashReported = true; log("holding the character in place failed: " .. tostring(err)) end
end

local lastCameraSwitch = -10
local function setup(pawn)
    -- the same view as in Skyrim: MORDHAU plays different (camera-framed) arm animations in first person
    if os.clock() - lastCameraSwitch < 0.5 then return end
    pcall(function()
        local want = input.firstPerson and os.clock() - input.stamp < 0.5
        if (pawn.bIsFirstPerson and true or false) ~= (want and true or false) then
            lastCameraSwitch = os.clock()
            pawn:CycleCamera()
        end
    end)
end

-- MORDHAU's camera in character space (rotation as a quaternion, UE's own conversion from pitch/yaw/roll)
function PS_camera_block(pawn, pc, mesh)
    local q, t, first = { 0, 0, 0, 1 }, { 0, 0, 0 }, 0
    pcall(function()
        local cm = pc.PlayerCameraManager
        if not there(cm) then return end
        local loc, rot = cm:GetCameraLocation(), cm:GetCameraRotation()
        local T = mesh:K2_GetComponentToWorld()
        local hp, hy, hr = math.rad(rot.Pitch) * 0.5, math.rad(rot.Yaw) * 0.5, math.rad(rot.Roll) * 0.5
        local SP, SY, SR, CP, CY, CR = math.sin(hp), math.sin(hy), math.sin(hr), math.cos(hp), math.cos(hy), math.cos(hr)
        local bx, by, bz, bw = CR * SP * SY - SR * CP * CY, -CR * SP * CY - SR * CP * SY, CR * CP * SY - SR * SP * CY, CR * CP * CY + SR * SP * SY
        local a = T.Rotation
        local ax, ay, az, aw = -a.X, -a.Y, -a.Z, a.W
        q = { aw * bx + ax * bw + ay * bz - az * by,
              aw * by - ax * bz + ay * bw + az * bx,
              aw * bz + ax * by - ay * bx + az * bw,
              aw * bw - ax * bx - ay * by - az * bz }
        t = { to_local(T, loc) }
        first = pawn.bIsFirstPerson and 1 or 0
        -- the field of view MORDHAU really renders with rides along in the same number (x2)
        local fov = 0
        pcall(function() if there(pawn.MordhauCamera) then fov = math.floor(pawn.MordhauCamera.FieldOfView + 0.5) end end)
        if fov >= 40 and fov <= 170 then first = first + 2 * fov end
        if PS_menu_open() then first = first + 4096 end   -- MORDHAU's voice / emote menu is open
    end)
    return string.pack("<fffffffi4", q[1], q[2], q[3], q[4], t[1], t[2], t[3], first)
end

-- looking an object up scans every object in the game, so the controller is looked up rarely and remembered
local cachedPc, lastLookup = nil, -10
local function controller()
    if cachedPc and cachedPc:IsValid() then return cachedPc end
    cachedPc = nil
    if os.clock() - lastLookup < 1.0 then return nil end
    lastLookup = os.clock()
    local pc = FindFirstOf("PlayerController")
    if pc and pc:IsValid() then cachedPc = pc end
    return cachedPc
end

-- Every frame, from MORDHAU's own controller tick (before its character moves: MORDHAU only honours movement input
-- given in the same frame).
local function frame()
    if PS_stopped then return end   -- (no longer an offline session: nothing of the mod acts any more)
    PS_frameAt = os.clock()
    -- (the slow duties - heartbeat, getting back into the map - from here, on the game thread; see the loop at the end)
    if os.clock() - (PS_tickAt or 0) > 0.25 then
        PS_tickAt = os.clock()
        local okTick, tickErr = pcall(PS_tick)
        if not okTick and PS_lastError ~= tostring(tickErr) then PS_lastError = tostring(tickErr); log("tick error: " .. PS_lastError) end
    end
    local pc = controller()
    if not pc then return end
    local pawn = pc.Pawn
    if pawn and (not pawn:IsValid() or not pawn:GetFullName():find("MordhauCharacter", 1, true)) then pawn = nil end

    -- dev: run a snippet dropped into the cache folder (PAWN and PC are in scope), then remove it
    snippetCheck = (snippetCheck or 0) + 1
    if snippetCheck % 10 == 0 then
        local chunk = loadfile(CMD)
        if chunk then
            os.remove(CMD)
            PAWN, PC = pawn, pc
            local ok, res = pcall(chunk)
            log("snippet " .. (ok and "ok: " or "FAILED: ") .. tostring(res))
        end
    end

    if not pawn then
        -- not in the world (not joined yet, or killed): ask MORDHAU to spawn us, at most once a second
        home = nil
        if os.clock() - (lastSpawnAsk or 0) > 1.0 then
            lastSpawnAsk = os.clock()
            -- as the player's own mercenary (the one the Skyrim side draws), in the match rather than spectating
            pcall(function()
                local cfg = dofile(CONFIG)
                if pc.SelectedCharacterProfile ~= cfg.profile_index then
                    pc.SelectedCharacterProfile = cfg.profile_index
                    pc.SelectedDefaultProfile = -1
                    pc:PrepareAndSendCustomizationIfChanged()
                    log("chose mercenary " .. tostring(cfg.profile_name) .. " (number " .. tostring(cfg.profile_index) .. " in the list)")
                    lastSpawnAsk = os.clock() + 1.5   -- give MORDHAU a moment to take the choice in before spawning
                end
            end)
            if not joinTried then
                joinTried = true
                for _, attempt in ipairs({ function() pc:ServerRequestSetTeam(0) end, function() pc:ServerRequestSetTeam(0, false) end }) do
                    local ok, err = pcall(attempt)
                    log("join the match: " .. (ok and "asked" or ("failed: " .. tostring(err))))
                    if ok then break end
                end
            end
            pcall(function() if pc:CanAskForSpawn() then pc:AskForSpawn(); log("asked MORDHAU to spawn the character") end end)
        end
        return
    end
    local mesh = pawn.Mesh
    if not mesh or not mesh:IsValid() then return end

    -- spawned before MORDHAU had taken the mercenary in (bare fists): go again, a couple of times at most
    if not pawnChecked or (frameCount or 0) % 60 == 0 then
        local key = pawn:GetFullName()
        if key ~= checkedPawn then checkedPawn = key; pawnSince = os.clock(); pawnChecked = false end
    end
    frameCount = (frameCount or 0) + 1
    if not pawnChecked and os.clock() - pawnSince > 1.5 then
        pawnChecked = true
        local bare = false
        pcall(function() bare = class_name(pawn.RightHandEquipment):find("Fists", 1, true) ~= nil end)
        if bare and (respawns or 0) < 3 then
            respawns = (respawns or 0) + 1
            log("spawned without the mercenary's gear; respawning")
            pcall(function() pc:PrepareAndSendCustomizationIfChanged() end)
            pcall(function() pawn:Suicide() end)
            return
        elseif not bare then
            local w = "?"; pcall(function() w = class_name(pawn.RightHandEquipment) end)
            log("in the match holding " .. w)
        end
    end

    -- MORDHAU's window is only read for its health / stamina display (Skyrim shows that): its 3D view is blacked out
    -- and its "Pick a Side" screen, which stays up when the match is joined from here, is taken down.
    -- (only in the first half minute after the character appears: searching the game's objects is not safe to do for ever)
    if (frameCount or 0) % 300 == 5 and os.clock() - (pawnSince or 0) < 30 then
        pcall(function()
            local cm = pc.PlayerCameraManager
            if there(cm) then cm:StartCameraFade(1.0, 1.0, 0.1, { R = 0, G = 0, B = 0, A = 1 }, false, true) end
        end)
        pcall(function()
            for _, w in ipairs(FindAllOf("BP_OneTeamSelect_C") or {}) do
                if w:IsValid() and w:IsInViewport() then w:RemoveFromParent() end
            end
            -- MORDHAU's own "press E to pick up" prompt: weapons do lie on its ground (dropped, disarmed), but picking up
            -- is decided in Skyrim, where the weapon is shown - the prompt would only float in the streamed HUD
            for _, w in ipairs(FindAllOf("BP_InteractWidget_C") or {}) do
                if w:IsValid() then pcall(function() w:SetVisibility(1) end); pcall(function() w:RemoveFromParent() end) end
            end
        end)
    end
    -- MORDHAU's own button prompts ("E Pick Up ..." for a weapon on its ground) are kept off its screen: the whole
    -- prompt panel of its HUD is collapsed, a few times a second (MORDHAU shows the single prompts again by itself,
    -- so taking those down one by one did not last). Skyrim shows its own prompt for a weapon on the ground.
    if os.clock() - (PS_promptSwept or -100) > 0.3 then
        PS_promptSwept = os.clock()
        pcall(function()
            local panel = pc.MyHUD.HUDButtonPrompts
            if there(panel) and panel:GetVisibility() ~= 1 then panel:SetVisibility(1) end
        end)
        -- ... and the single "E Pick Up <weapon>" labels MORDHAU puts over things on its ground: the list of them is
        -- looked up every 5 s (a look-up walks every object of the game), each is collapsed whenever it shows
        pcall(function()
            if os.clock() - (PS_labelsListed or -100) > 5.0 then
                PS_labelsListed = os.clock()
                PS_labels = FindAllOf("BP_InteractWidget_C") or {}
            end
            for _, w in ipairs(PS_labels or {}) do
                if w:IsValid() and w:GetVisibility() ~= 1 then w:SetVisibility(1) end
            end
        end)
        -- ... and weapons that lie loose on MORDHAU's ground without belonging to the player's or a stand-in's drop
        -- (left over from handing weapons out) are taken out of its world
        if os.clock() - (PS_strayswept or -100) > 6.0 then
            PS_strayswept = os.clock()
            pcall(function()
                local keep = {}
                if there(PS_playerHeld) then keep[PS_playerHeld:GetFullName()] = true end
                for _, b in ipairs(PS_get_bots()) do
                    if b.weapon and there(b.weapon.held) then keep[b.weapon.held:GetFullName()] = true end
                end
                if PS_pickLater and there(PS_pickLater.w) then keep[PS_pickLater.w:GetFullName()] = true end
                local gone = 0
                for _, w in ipairs(FindAllOf("MordhauWeapon") or {}) do
                    PS_strayDone = PS_strayDone or {}
                    if w:IsValid() and not keep[w:GetFullName()] and not PS_strayDone[w:GetFullName()] then
                        local loose = false
                        pcall(function()
                            -- (only near the character: the arena has its own weapons lying about, and they come back)
                            local at, me = w:K2_GetActorLocation(), pawn:K2_GetActorLocation()
                            local near = (at.X - me.X) ^ 2 + (at.Y - me.Y) ^ 2 < 700 * 700
                            loose = near and not there(w:GetAttachParentActor()) and not there(w:GetOwner())
                        end)
                        local name = class_name(w)
                        if loose and not name:find("Fists", 1, true) and not name:lower():find("bow", 1, true) and not name:find("ToolBox", 1, true) and not name:find("Bomb", 1, true) and not name:find("Trap", 1, true) then
                            PS_strayDone[w:GetFullName()] = true   -- (asked once: some stay listed although gone)
                            w:K2_DestroyActor(); gone = gone + 1
                        end
                    end
                end
                if gone > 0 then log(gone .. " stray weapon(s) taken off MORDHAU's ground") end
            end)
        end
    end
    PS_pc = pc
    if PS_offline_guard(pc) ~= true then return end   -- (not known to be offline: nothing of the mod acts)
    if (frameCount or 0) % 120 == 1 then pcall(function() pawn.bIsAllowedOutOfBounds = true end) end
    -- MORDHAU's own AFK watch (player controller: the camera has not moved for MaxAFKTime -> it quits the game). Our
    -- character walks on the spot by design, so the watch is held at zero, a few times a second.
    if (frameCount or 0) % 30 == 2 then
        pcall(function() pc.CurrentAFKTime = 0.0 end)
        pcall(function() pc.MaxAFKTime = 1000000000.0 end)
    end
    -- First of all, before anything is moved: MORDHAU's camera as it was drawn last frame, the body where it stood
    -- then and the pose it had then all belong together. (Read after the character was put back on its spot, the
    -- camera was a few centimetres off the body from frame to frame: the first-person view shook.)
    if os.clock() - lastStream >= STREAM_EVERY then
        lastStream = os.clock()
        stream(pawn, mesh)
    end
    setup(pawn)
    apply_input(pawn, pc)
    keep_near_home(pawn)
    local ok, err = pcall(update_ghosts, pawn, pc)
    if not ok and PS_ghostError ~= tostring(err) then PS_ghostError = tostring(err); log("stand-in error: " .. PS_ghostError) end
    -- the local match must not end (time or score): Skyrim's session goes on as long as the player likes
    if os.clock() - (lastKeepAlive or -100) > 20 then
        lastKeepAlive = os.clock()
        pcall(function() GAMESTATE = GAMESTATE and GAMESTATE:IsValid() and GAMESTATE or FindFirstOf("MordhauGameState") end)
        pcall(function() GAMEMODE = GAMEMODE and GAMEMODE:IsValid() and GAMEMODE or FindFirstOf("MordhauGameMode") end)
        pcall(function() GAMEMODE.DamageFactor = DAMAGE_FACTOR end)
        pcall(function() GAMEMODE.ScoreToWin = 1000000000.0 end)
        pcall(function() GAMESTATE.MatchDurationMax = 100000000 end)
        pcall(function() GAMESTATE.ElapsedTime = 0 end)
    end
end

-- A few times a second, outside the frame: get from the main menu into the local map, and attach to the controller's
-- tick once the match's controller exists.
local HOOK = "/Game/Mordhau/Blueprints/BP_MordhauPlayerController.BP_MordhauPlayerController_C:ReceiveTick"
-- A line every 20 s on how things stand, so that when MORDHAU closes by itself (seen: a clean exit with no reason in
-- its own log twice) the last seconds before it are on record: is the frame still running, what
-- state is the match in, how long has Skyrim been silent.
local function heartbeat(pc)
    if os.clock() - (PS_beatAt or -100) < 20 then return end
    PS_beatAt = os.clock()
    local parts = {}
    parts[#parts + 1] = string.format("frame ran %.1f s ago", os.clock() - (PS_frameAt or -1000))
    parts[#parts + 1] = string.format("Skyrim's input %.1f s old", os.clock() - (input.stamp or -1000))
    pcall(function()
        local gs = FindFirstOf("MordhauGameState")
        if gs and gs:IsValid() then
            local state = "?"
            pcall(function() state = gs.MatchState:ToString() end)
            parts[#parts + 1] = "match " .. state .. string.format(", elapsed %s of %s", tostring(gs.ElapsedTime), tostring(gs.MatchDurationMax))
        else
            parts[#parts + 1] = "no game state"
        end
    end)
    pcall(function() parts[#parts + 1] = string.format("AFK %.0f of %.0f", pc.CurrentAFKTime, pc.MaxAFKTime) end)
    pcall(function() parts[#parts + 1] = "controller " .. (pc:GetFullName():match("([%w_]+)$") or "?") end)
    parts[#parts + 1] = string.format("script memory %.0f MB", collectgarbage("count") / 1024)
    log("alive: " .. table.concat(parts, "; "))
end

local function tick()
    local pc = controller()
    if not pc then
        if os.clock() - (PS_noPcAt or -100) > 20 then PS_noPcAt = os.clock(); log("alive: no player controller right now") end
        return
    end
    pcall(heartbeat, pc)
    -- (the AFK watch is also held off here, for the moments the character is not in the world)
    pcall(function() pc.CurrentAFKTime = 0.0; pc.MaxAFKTime = 1000000000.0 end)
    local inMap = pc:GetFullName():find(MAP, 1, true) ~= nil
    -- not in our map (main menu at start, or MORDHAU moved on after a match ended): open it, again if need be
    if not inMap and os.clock() - started > 12 and os.clock() - (lastOpen or -100) > 45 then
        lastOpen = os.clock()
        cachedPc = nil
        local ksl = StaticFindObject("/Script/Engine.Default__KismetSystemLibrary")
        if ksl and ksl:IsValid() then
            log("opening local map " .. MAP)
            ksl:ExecuteConsoleCommand(pc, "open " .. MAP, pc)
        else
            log("KismetSystemLibrary not found; cannot open a map")
        end
        return
    end
    if inMap and not PS_hooked then
        local fn = StaticFindObject(HOOK)
        if fn and fn:IsValid() then
            PS_hooked = true
            -- (the frame runs as a game thread action, not inside the hook: newer UE4SS holds its Lua mutex for a hook's
            -- whole callback and takes the same mutex for every object constructed, on any thread. A load the frame
            -- causes - PrepareAndSendCustomizationIfChanged loading a voice, seen 2026-10-16 - then waits for the async
            -- loading thread, which waits for the mutex: MORDHAU hangs. Game thread actions run without it.)
            RegisterHook(HOOK, function()
                if PS_framePending then return end   -- never queue a second frame behind one that has not run yet
                PS_framePending = true
                ExecuteInGameThread(function()
                    PS_framePending = false
                    local ok, err = pcall(PS_frame)
                    if not ok and PS_lastError ~= tostring(err) then PS_lastError = tostring(err); log("frame error: " .. PS_lastError) end
                end)
            end)
            log("attached to the controller's tick")
        end
    end
end

-- Why does MORDHAU sometimes close by itself? Note who asks for it (seen: a clean "EngineExit() was called" with
-- nothing else in MORDHAU's log). Logged only; nothing is changed.
-- (off unless cache/exit_watch.txt exists: suspected of ending MORDHAU itself when a trap is placed)
local watchFile = io.open((CACHE .. "exit_watch.txt"), "r")
if watchFile then watchFile:close() end
if watchFile and not PS_exitWatch then
    PS_exitWatch = true
    for _, fn in ipairs({ "/Script/Engine.KismetSystemLibrary:QuitGame", "/Script/Engine.PlayerController:ConsoleCommand",
        "/Script/Engine.PlayerController:ClientReturnToMainMenuWithTextReason", "/Script/Engine.PlayerController:ClientTravel",
        "/Script/Mordhau.MordhauPlayerController:OnAFKTimeExceededMaximum", "/Script/Mordhau.MordhauGameInstance:QuitGame",
        "/Script/Engine.KismetSystemLibrary:ExecuteConsoleCommand" }) do
        local ok, err = pcall(RegisterHook, fn, function(ctx, a, b)
            local extra = ""
            pcall(function() extra = tostring(a:get():ToString()) end)
            log("EXIT WATCH: " .. fn:match("[^:]+$") .. " was called " .. extra)
        end)
        if not ok then log("exit watch: could not watch " .. fn:match("[^:]+$")) end
    end
end

-- The loop is started once and calls whatever PS_tick currently is, so the dev tools can load a newer copy of this
-- file into the running game (tools/mordhau_lua.sh --reload) without UE4SS's own mod restart, which can hang here.
PS_tick = tick
PS_frame = frame
if not PS_loop then
    PS_loop = true
    log("loaded; will open " .. MAP .. " and stream the local character to " .. OUT)
    log("stand-ins use MORDHAU's own bot brains: " .. (BOT_BRAINS and "yes" or "no (they copy the Skyrim character's attacks and blocks)"))
    LoopAsync(250, function()
        -- Once the controller's own tick drives the script (game thread), this loop ENDS: script code running here, on
        -- UE4SS's other thread, beside the game thread's was what ended MORDHAU now and then - caught with
        -- tools/exit_watch.py: a C++ exception thrown inside UE4SS on the game thread, the engine closing as it unwound
        -- ("EngineExit() was called" with no reason). From then on the frame itself calls the tick (see frame()).
        if PS_hooked and PS_frameAt and os.clock() - PS_frameAt < 2.0 then
            print("[PoseStream] the outer loop has ended: everything runs from the controller's tick now\n")
            return true
        end
        if PS_pending then return false end   -- never queue a second tick behind one that has not run yet
        PS_pending = true
        ExecuteInGameThread(function()
            PS_pending = false
            local ok, err = pcall(PS_tick)
            if not ok and PS_lastError ~= tostring(err) then PS_lastError = tostring(err); log("tick error: " .. PS_lastError) end
        end)
        return false
    end)
else
    opened = true
    log("reloaded")
end
