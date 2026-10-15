# MORDHAU x Skyrim - experimental

MORDHAU's melee combat inside Skyrim Special Edition. Both games run at once: **Skyrim is the world**, **MORDHAU is the
fighter** (your swings, parries, feints, stamina; the enemies you fight are driven by MORDHAU's own AI).

**This is an experiment made for one person's PC and only now tried elsewhere. Expect things to break.**

## Before you install - read this

- **You need both games**, bought and installed through Steam: *The Elder Scrolls V: Skyrim Special Edition* and
  *MORDHAU*. Nothing of either game is in this package. MORDHAU's models and animations are read from **your own**
  MORDHAU the first time you play (about 2 GB are prepared on your disk, once).
- **MORDHAU is only ever run OFFLINE by this** - a local match on your own PC. MORDHAU is an online game with
  anti-cheat. This does not touch, disable or work around the anti-cheat, and it must **never be used to play online**.
  The file that lets the mashup's script load is in MORDHAU's folder only while a mashup session is starting; a normal
  MORDHAU launch from Steam has none of it. If you are not comfortable with that, do not install this.
- **Skyrim Special Edition version: see `payload/tested-with.json`.** The installer stops on any other version.
- **One other mod is needed: the "Address Library for SKSE Plugins"** from Nexus Mods (mod 32444, the *All in one* file
  for your game version). It is a small data file; the installer checks for it and tells you if it is missing. SKSE
  itself is not needed (it does no harm if you have it).
- Windows 10 or 11, a PC that can run both games at the same time, about 3 GB of free disk space.
- It changes how Skyrim plays a great deal. **Use a new save or one you do not mind losing.**
- Other combat, camera and first-person-body mods for Skyrim will clash with it. Texture, quest and world mods are fine.

## MORDHAU online: keep it clean

The mashup's launcher puts one small file (`dwmapi.dll`, a script loader) into MORDHAU's `Mordhau\Binaries\Win64`
folder for the few seconds in which its own offline MORDHAU starts, and takes it out again: when it starts, as soon as
MORDHAU has loaded it, and whenever the launcher ends. **MORDHAU must never be started for online play while that file
is there** - MORDHAU has anti-cheat, and nobody can promise you how it treats a script loader.

- Before you play MORDHAU online - and always after a crash, a power cut or a killed launcher - run
  `Make-Mordhau-Online-Safe.ps1`. It checks the folder, removes anything left, and tells you it is clean.
- To check by hand: `Mordhau\Binaries\Win64` must not contain a file named `dwmapi.dll`.
  (`dwmapi.dll.mashup` is the same file put aside under a name MORDHAU does not load; it does nothing.)
- Two guards are built into the mashup's own script, in case the file is ever left behind or someone tries anyway:
  a MORDHAU that was started normally with the loader still in its folder **closes itself** about a minute after it
  opens (with a notice telling you to run the script above), and a mashup session that is ever anything but its own
  offline match - a server joined or hosted - **closes itself** within a few seconds. Do not rely on them; keep the
  folder clean.
- Never play online from the mashup's MORDHAU window, and close that window before starting MORDHAU from Steam.

## Install

1. Unpack this folder somewhere it can stay (not inside either game's folder).
2. Close Skyrim and MORDHAU.
3. Right-click `Install-Mashup.ps1` > *Run with PowerShell*, or in a terminal:
   `powershell -NoProfile -ExecutionPolicy Bypass -File Install-Mashup.ps1`
   - It finds both games through Steam. Games somewhere else: add
     `-SkyrimPath "<folder with SkyrimSE.exe>" -MordhauPath "<MORDHAU's folder>"`.
   - It only adds files (its own plugin, script and two small open-source loaders) and writes down every one of them in
     `mashup-install.json`. It replaces nothing of the games.

## Play

`powershell -NoProfile -ExecutionPolicy Bypass -File Play-Mashup.ps1`

It starts MORDHAU in a window, waits for its local match, then starts Skyrim, and closes MORDHAU when you quit Skyrim.
Leave its console window open. The **first** start prepares MORDHAU's models first (a few minutes). Then load a save
and draw a weapon. Controls and what to expect: `HOW-TO-PLAY.md`.

## Remove

`Make-Mordhau-Online-Safe.ps1` only cleans MORDHAU's folder for normal play; to take everything out:

`powershell -NoProfile -ExecutionPolicy Bypass -File Uninstall-Mashup.ps1` takes every placed file out again.

## What is in here

- This mashup's own work: `DirectionalMelee.asi` (Skyrim plugin), `main.lua` (MORDHAU script), the launcher and
  installer, `MordhauWeapons.esp` (MORDHAU's weapons as Skyrim items: records only, no models).
- Two open-source loaders, unchanged, with their licences in `LICENSES/`: Ultimate ASI Loader (loads the plugin into
  Skyrim) and UE4SS (runs the script in MORDHAU).

Not affiliated with or endorsed by Bethesda, Triternion, or the authors of those loaders.
