NEVER USE THIS IN ONLINE MORDHAU PLAY.
Before you play MORDHAU online, make sure there is no file named dwmapi.dll (or dwmapi.dll.old)
in MORDHAU's Mordhau\Binaries\Win64 folder. If there is one, delete it first.

MORDHAU X SKYRIM - EXPERIMENTAL
===============================

MORDHAU's melee combat inside Skyrim Special Edition. Both games run at once: Skyrim is the world, MORDHAU is the
fighter (your swings, parries, feints, stamina; the enemies you fight are driven by MORDHAU's own AI).

This is an experiment made for one person's PC and only now tried elsewhere. Expect things to break.

BEFORE YOU INSTALL - READ THIS
------------------------------

- You need both games, bought and installed through Steam: The Elder Scrolls V: Skyrim Special Edition and
  MORDHAU. Nothing of either game is in this mod. MORDHAU's models and animations are read from your own MORDHAU
  the first time you play (about 2 GB are prepared on your disk, once).
- This mod starts MORDHAU by itself, OFFLINE - a local match on your own PC, in a window behind Skyrim - every time
  you start Skyrim with the mod switched on, and closes it when Skyrim closes. MORDHAU is an online game with
  anti-cheat. This mod does not touch, disable or work around the anti-cheat, and it must never be used to play
  online. If you are not comfortable with that, do not install it.
- It writes into MORDHAU's folder. See What it does to MORDHAU's folder below.
- Skyrim Special Edition 1.7.104 (backwards compatible with 1.6.1170; other versions are untested), with
  SKSE for your version and the Address Library for SKSE Plugins
  (Nexus Mods, mod 32444).
- Windows 10 or 11, a PC that can run both games at the same time, about 3 GB of free disk space.
- It changes how Skyrim plays a great deal. Use a new save or one you do not mind losing.
- Other combat, camera and first-person-body mods for Skyrim will clash with it. Texture, quest and world mods are fine.

INSTALL
-------

With your mod manager (Vortex or Mod Organizer 2), like any other mod. Make sure MordhauWeapons.esp is switched on.
There is nothing to run and nothing to copy by hand.

PLAY
----

Start Skyrim the way you always do with mods (through SKSE). MORDHAU opens by itself in a window; Skyrim keeps the
keyboard. The first start prepares MORDHAU's models once the main menu is up (under a minute on a fast disk), then
starts MORDHAU. MORDHAU needs one to four minutes to open its match; until then Skyrim plays as usual. When *MORDHAU is
live* appears at the top left, draw a melee weapon. Controls and what to expect: HOW-TO-PLAY.md beside this file.

To play Skyrim without it: switch the mod off in your mod manager, or set StartMordhau=0 in MordhauSkyrim.ini.

WHAT IT DOES TO MORDHAU'S FOLDER
--------------------------------

All of it happens in Mordhau\Binaries\Win64 of your MORDHAU. Nothing of MORDHAU's own is changed or replaced.

- Stays there: UE4SS.dll, UE4SS-settings.ini, UE4SS.mashup.txt, and the folders UE4SS_Signatures and
  Mods (the open-source script host UE4SS, and this mod's script in Mods\PoseStream). On their own these do
  nothing: MORDHAU does not load them.
- There only while the mod's MORDHAU starts: dwmapi.dll, the mod's own small loader. The mod puts it in and
  starts MORDHAU; the loader checks, before anything else runs, that MORDHAU was started by the mod (with -insecure
  and the mod's one-time key). If so it loads the script host and renames itself to dwmapi.dll.old at once; the mod
  deletes that when MORDHAU has closed. If not - MORDHAU started any other way - it renames itself and ends the game
  on the spot, leaving a note (MordhauSkyrim-closed-MORDHAU.txt); the next start is normal.
- MORDHAU must never be started for online play while a file named dwmapi.dll is in that folder. MORDHAU has
  anti-cheat, and nobody can promise you how it treats a script loader. After a crash or a power cut, look: if
  dwmapi.dll (or dwmapi.dll.old) is there, delete it. Starting Skyrim with the mod also removes it.
- Two guards are built into the mod's script, in case the file is ever left behind: a MORDHAU that was started
  normally with the loader still in its folder closes itself about a minute after it opens, with a notice; and the
  mod's own MORDHAU closes itself within a few seconds if it is ever anything but its own offline match (a server
  joined or hosted). Do not rely on them; keep the folder clean.
- A MORDHAU that is already running when Skyrim starts is never touched: the mod waits until you have closed it.
- Never play online from the mod's MORDHAU window.
- One setting outside that folder: MORDHAU mutes itself while its window is not in front, and the mod's MORDHAU
  always runs behind Skyrim. Just before starting it, the mod sets MORDHAU's own setting for that
  (%LOCALAPPDATA%\Mordhau\Saved\Config\WindowsClient\Engine.ini, [Audio] UnfocusedVolumeMultiplier) to 1, and puts
  your own value back into the file as soon as MORDHAU is running (MORDHAU reads it only when it starts). If it is
  ever left at 1 - after a crash - MORDHAU simply keeps playing its sound when it is in the background.

HOW IT KEEPS MORDHAU ONLINE PLAY SEPARATE
-----------------------------------------

The mod is built so that nothing of it is loaded when you start MORDHAU normally:

1. It only starts its own offline MORDHAU. When Skyrim starts, the mod launches MORDHAU itself, in a local match, with the -insecure option and a one-time key that only that launch gets.
2. The loader checks before anything else runs. MORDHAU only loads the mod's script if a small loader file (dwmapi.dll) is in its folder. That loader first checks for -insecure and the one-time key. Without both it ends the game within a fraction of a second, loads nothing, and renames itself so the next start is normal.
3. The loader removes itself. Once it has done its job it renames itself to dwmapi.dll.old, which MORDHAU does not load. The mod deletes that file when MORDHAU closes or at the next session.
4. The script checks again. It verifies the one-time key a second time, and closes the game if the offline session ever becomes a joined or hosted server.
5. A MORDHAU you started yourself is never touched. If MORDHAU is already running when Skyrim starts, the mod waits until you have closed it.

What is not verified. The check while the game runs rests on one question to MORDHAU's engine: "is this still a standalone local match?" The shutdown that follows a "no" or a "can't tell" is tested. Whether the engine answers "no" for every way a session could go online, and how early in a connection it does, has not been tested, because testing it would mean taking the mod online. The launch check, -insecure and the loader's self-removal do not depend on it.

HOW TO CHECK IT YOURSELF
------------------------

Do this after closing Skyrim and before playing MORDHAU online:

1. Look in the folder. Open MORDHAU's Mordhau\Binaries\Win64 folder. There must be no file named dwmapi.dll. If dwmapi.dll.old is there, delete it. The files UE4SS.dll, UE4SS-settings.ini and the Mods folder may stay: without the loader MORDHAU ignores them.
2. Check that nothing was loaded on a normal start. Note the "date modified" of UE4SS.log in that folder, start MORDHAU from Steam, go to the main menu, quit, and look again. If the date has not changed, the mod's script host was not loaded.
3. Look for the loader's note. If the loader ever had to stop a wrong start, it leaves a file named MordhauSkyrim-closed-MORDHAU.txt in that folder saying why. If you find it, the protection worked; delete the note and dwmapi.dll.old.

If anything looks wrong, remove the mod and delete the files listed under "Remove" before playing online. These checks show the mod is not loaded; they are not a promise about how MORDHAU's anti-cheat behaves.

REMOVE
------

Remove the mod with your mod manager. Then delete from Mordhau\Binaries\Win64: UE4SS.dll, UE4SS-settings.ini,
UE4SS.mashup.txt, UE4SS.log, the folders UE4SS_Signatures and Mods, and dwmapi.dll / dwmapi.dll.old if
present. (If you had UE4SS there before, the mod left yours alone: only delete Mods\PoseStream.) The prepared models
are in %LOCALAPPDATA%\MordhauSkyrim - delete that folder too. The mod also writes model files for the
inventory into Skyrim's Data\meshes\MordhauSkyrim and Data\textures\MordhauSkyrim (about 55 MB; with Mod Organizer
they land in Overwrite) - delete those two folders as well.

WHAT IS IN HERE
---------------

- This mod's own work: MordhauSkyrim.dll (SKSE plugin), main.lua (MORDHAU script), MordhauWeapons.esp
  (MORDHAU's weapons as Skyrim items: records only, no models), and the loader dwmapi.dll in the mordhau folder.
- UE4SS v3.0.1 (runs the script in MORDHAU), in the mordhau folder beside this file: UE4SS.dll and its own Lua mods
  unchanged; UE4SS-settings.ini and Mods/mods.txt with this mod's settings.

LICENCE AND SOURCE
------------------

This mod is free software under the GPL-3.0-or-later, as the library it is built on (CommonLibSSE-NG) requires: you may
use, change and share it under that licence. The complete source of each version is on the mod's page on Nexus Mods,
under Files, as "Source code". Every licence text - this mod's, CommonLibSSE-NG's, UE4SS's and those of the other
libraries the plugin is built with - is in the LICENSES folder beside this file; MordhauSkyrim.txt there says what
is whose.

Made with AI assistance: the code, tools and texts of this mod were written with the help of an AI coding assistant (Claude, by Anthropic), directed and play-tested by the author.

Not affiliated with or endorsed by Bethesda, Triternion, or the authors of UE4SS, CommonLibSSE-NG or SkyCraft.
