NEVER USE THIS IN ONLINE MORDHAU PLAY.
Before you play MORDHAU online, make sure there is no file named dwmapi.dll (or dwmapi.dll.old)
in MORDHAU's Mordhau\Binaries\Win64 folder. If there is one, delete it first.

MORDHAU X SKYRIM (EXPERIMENTAL)
===============================

MORDHAU's melee combat in Skyrim Special Edition. Both games run at the same time: Skyrim is the world, MORDHAU does
the fighting - your swings, parries, feints and stamina, and the enemies you fight are run by MORDHAU's own bots.

This started as a mod for one PC. Expect bugs.

BEFORE YOU INSTALL
------------------

- You need both games from Steam: Skyrim Special Edition and MORDHAU. No game files are included. The first time you
  play, the mod prepares MORDHAU's models and animations from your own copy of MORDHAU (about 2 GB, once).
- The mod starts MORDHAU by itself, offline, in a local match in a window behind Skyrim, whenever you start Skyrim with
  the mod on. It closes MORDHAU when Skyrim closes.
- MORDHAU is an online game with anti-cheat. The mod does not touch or work around the anti-cheat, and it must never be
  used to play online. If that makes you uneasy, don't install it.
- It puts files in MORDHAU's folder (listed below).
- Skyrim 1.7.104 or 1.6.1170 (others are untested), SKSE for your version, and Address Library for SKSE Plugins
  (Nexus mod 32444).
- Optional: SkyUI and MCM Helper for the settings menu. Without them the defaults are used.
- Windows 10 or 11, a PC that can run both games at once, about 3 GB free.
- It changes a lot about how Skyrim plays. Use a new save, or one you don't mind losing.
- Other combat, camera and first-person body mods will clash with it. Texture, quest and world mods are fine.

INSTALL
-------

Install it with Vortex or Mod Organizer 2 like any other mod and make sure MordhauWeapons.esp is enabled. Nothing
else to run or copy.

PLAY
----

Start Skyrim through SKSE as usual. MORDHAU opens in a window and Skyrim keeps the keyboard. On the first start the
mod prepares MORDHAU's models once the main menu is up (under a minute on a fast disk), then starts MORDHAU, which
takes one to four minutes to open its match. Skyrim plays normally until then. When "MORDHAU is live" shows at the
top left, draw a melee weapon. Controls are in HOW-TO-PLAY.md next to this file.

To play Skyrim without it: disable the mod, set StartMordhau=0 in MordhauSkyrim.ini, or turn off "Start MORDHAU
with Skyrim" on the settings menu's Safety page (takes effect on the next start).

The settings menu has damage, armour, enemy skill, how many enemies fight by MORDHAU's rules and how far around you,
kicks, sound mix, your character's voice, crouch toggle or hold, and more. MCM Helper keeps them in
Data\MCM\Settings\MordhauWeapons.ini.

WHAT GOES INTO MORDHAU'S FOLDER
-------------------------------

Everything is in Mordhau\Binaries\Win64. Nothing of MORDHAU's own is changed or replaced.

- These stay there: UE4SS.dll, UE4SS-settings.ini, UE4SS.mashup.txt, and the folders UE4SS_Signatures and
  Mods (UE4SS, an open-source script host, and the mod's script in Mods\PoseStream). MORDHAU doesn't load any of
  them on its own.
- dwmapi.dll is there only while the mod starts MORDHAU. It's the mod's own small loader. Before anything else
  runs, it checks that MORDHAU was started by the mod (with -insecure and a one-time key). If so, it loads the
  script host and renames itself to dwmapi.dll.old right away; the mod deletes that once MORDHAU has closed. If
  MORDHAU was started any other way, it renames itself, closes the game straight away and leaves a note
  (MordhauSkyrim-closed-MORDHAU.txt). The next start is normal.
- The script's small helper, MordhauBones.dll (it reads the fighters' bones), is never put in MORDHAU's folder. It
  is copied to %LOCALAPPDATA%\MordhauSkyrim for each session and loaded from there.

Never start MORDHAU for online play while a file called dwmapi.dll is in that folder. Nobody can promise how the
anti-cheat would treat a script loader. After a crash or a power cut, check the folder and delete dwmapi.dll or
dwmapi.dll.old if you see them. Starting Skyrim with the mod also removes them.

Two more guards are in the script in case the loader is ever left behind: a MORDHAU started normally with the loader
still there closes itself about a minute after it opens, and the mod's own MORDHAU closes within seconds if it is ever
anything other than its offline match. Don't rely on them; keep the folder clean.

A MORDHAU that's already running when Skyrim starts is left alone. The mod waits until you close it.

One setting outside that folder: MORDHAU mutes itself when its window isn't in front, and the mod's MORDHAU always runs
behind Skyrim. Right before starting it, the mod sets [Audio] UnfocusedVolumeMultiplier in
%LOCALAPPDATA%\Mordhau\Saved\Config\WindowsClient\Engine.ini to 1, and puts your own value back as soon as MORDHAU
is running. If it's ever left at 1 after a crash, MORDHAU just keeps its sound in the background.

HOW ONLINE PLAY IS KEPT SEPARATE
--------------------------------

1. The mod only starts its own offline MORDHAU: a local match, with -insecure and a one-time key for that launch.
2. The loader checks before anything runs. Without -insecure and the key it closes the game within a fraction of a
   second, loads nothing, and renames itself so the next start is normal.
3. The loader takes itself out of the way (dwmapi.dll.old, which MORDHAU doesn't load), and the mod deletes it when
   MORDHAU closes or at the next session.
4. The script checks the key again, and closes the game if the session ever turns into a joined or hosted server.
5. A MORDHAU you started yourself is never touched.

What isn't verified: the check while the game runs relies on asking MORDHAU's engine whether this is still a
standalone local match. The shutdown after a "no" or an unclear answer is tested. Whether the engine says "no" for
every way a session could go online, and how early, is not - testing that would mean taking the mod online. The launch
check, -insecure and the loader removing itself don't depend on it.

CHECKING IT YOURSELF
--------------------

After closing Skyrim and before playing MORDHAU online:

1. Open Mordhau\Binaries\Win64. There must be no dwmapi.dll. Delete dwmapi.dll.old if it's there. The UE4SS
   files and the Mods folder can stay; without the loader MORDHAU ignores them.
2. Note the "date modified" of UE4SS.log in that folder, start MORDHAU from Steam, go to the main menu and quit. If
   the date hasn't changed, the script host wasn't loaded.
3. If you find MordhauSkyrim-closed-MORDHAU.txt, the loader stopped a wrong start and says why. Delete it and
   dwmapi.dll.old.

If anything looks wrong, remove the mod and the files below before playing online. These checks show the mod isn't
loaded; they aren't a promise about how MORDHAU's anti-cheat behaves.

NETWORK AND SAVES
-----------------

- The mod makes no internet connections and collects nothing: no telemetry, no update checks. The two games
  themselves still do what they always do (Steam, for one).
- It keeps nothing in your saves. If you remove it, the MORDHAU weapons from MordhauWeapons.esp disappear from
  inventories; that's the only trace. Removing it in the middle of a playthrough hasn't been tested much yet.

KNOWN PROBLEMS
--------------

- People sometimes take a moment to draw their weapon.
- In tight indoor spaces fighters can bump into tables and walls, and now and then a swing passes through someone.
- Other combat, camera and first-person body mods will clash with it.
- Horseback fighting and werewolf or vampire lord forms are untested.
- A MORDHAU or Skyrim update can break it.

REMOVE
------

Remove the mod in your mod manager. Then delete these from Mordhau\Binaries\Win64: UE4SS.dll,
UE4SS-settings.ini, UE4SS.mashup.txt, UE4SS.log, the UE4SS_Signatures and Mods folders, and dwmapi.dll /
dwmapi.dll.old if present. If you had your own UE4SS there before, the mod left it alone - only delete
Mods\PoseStream. Also delete %LOCALAPPDATA%\MordhauSkyrim (the prepared models and the helper), and the inventory
model folders Data\meshes\MordhauSkyrim and Data\textures\MordhauSkyrim (about 55 MB; with Mod Organizer they end
up in Overwrite).

WHAT'S IN THE DOWNLOAD
----------------------

- The mod's own files: MordhauSkyrim.dll (the SKSE plugin), MordhauBones.dll (the script's bone reader),
  main.lua (the MORDHAU script), MordhauWeapons.esp (MORDHAU's weapons as Skyrim items; records only, no models)
  and the loader dwmapi.dll in the mordhau folder.
- UE4SS v3.0.1, unchanged, in the mordhau folder, with this mod's UE4SS-settings.ini and Mods/mods.txt.

LICENCE AND SOURCE
------------------

GPL-3.0-or-later, because the library it's built on (CommonLibSSE-NG) requires it: you can use, change and share it
under that licence. The full source of each version is on the mod's Nexus page under Files ("Source code"). All licence texts are in the LICENSES folder next to this file;
MordhauSkyrim.txt there says what is whose.

Made with AI help: the code, tools and texts were written with an AI coding assistant (Claude, by Anthropic), directed
and play-tested by the author.

Not affiliated with or endorsed by Bethesda, Triternion, or the authors of UE4SS, CommonLibSSE-NG or SkyCraft.
