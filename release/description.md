# MORDHAU x Skyrim (experimental)

MORDHAU's melee combat inside Skyrim Special Edition.

This mod runs your own copy of MORDHAU, offline, beside Skyrim and makes it the fighter. **Skyrim stays the world**: places, people, quests, loot, saves, magic and bows. **MORDHAU does the melee**: your swings, parries, feints, kicks and stamina are MORDHAU's own, and the people you fight hand to hand are moved and fought by MORDHAU's AI. Damage, health and levels stay Skyrim's.

## Read this first

- **You need both games on Steam.** Nothing from either game is in the download. MORDHAU's models and animations are read from your own MORDHAU the first time you play (about 2 GB are prepared on your disk, once).
- **Offline only.** The mod starts MORDHAU by itself in a local match on your PC, in a window behind Skyrim, and closes it when Skyrim closes. MORDHAU is an online game with anti-cheat. This mod does not touch or work around the anti-cheat and must never be used for online play.
- **No guarantee for your MORDHAU account.** MORDHAU has anti-cheat for online play. This mod only ever runs inside its own offline MORDHAU, started with `-insecure`, and shuts the game down if it is started any other way. Even so, nobody, including the author, can promise how the anti-cheat treats it. If you cannot accept any risk to your MORDHAU account, do not install this.
- **It writes into MORDHAU's folder.** See "What it does to MORDHAU's folder" below. If that worries you, do not install it.
- **Experimental.** It was built on one PC and has barely been tried anywhere else. Expect things to break. Use a new save.
- Not affiliated with or endorsed by Bethesda or Triternion.

## How it keeps MORDHAU online play separate

The mod is built so that nothing of it is loaded when you start MORDHAU normally:

1. **It only starts its own offline MORDHAU.** When Skyrim starts, the mod launches MORDHAU itself, in a local match, with the `-insecure` option and a one-time key that only that launch gets.
2. **The loader checks before anything else runs.** MORDHAU only loads the mod's script if a small loader file (`dwmapi.dll`) is in its folder. That loader first checks for `-insecure` and the one-time key. Without both it ends the game within a fraction of a second, loads nothing, and renames itself so the next start is normal.
3. **The loader removes itself.** Once it has done its job it renames itself to `dwmapi.dll.old`, which MORDHAU does not load. The mod deletes that file when MORDHAU closes or at the next session.
4. **The script checks again.** It verifies the one-time key a second time, and closes the game if the offline session ever becomes a joined or hosted server.
5. **A MORDHAU you started yourself is never touched.** If MORDHAU is already running when Skyrim starts, the mod waits until you have closed it.

**What is not verified.** The check while the game runs rests on one question to MORDHAU's engine: "is this still a standalone local match?" The shutdown that follows a "no" or a "can't tell" is tested. Whether the engine answers "no" for every way a session could go online, and how early in a connection it does, has not been tested, because testing it would mean taking the mod online. The launch check, `-insecure` and the loader's self-removal do not depend on it.

## How to check it yourself

Do this after closing Skyrim and before playing MORDHAU online:

1. **Look in the folder.** Open MORDHAU's `Mordhau\Binaries\Win64` folder. There must be no file named `dwmapi.dll`. If `dwmapi.dll.old` is there, delete it. The files `UE4SS.dll`, `UE4SS-settings.ini` and the `Mods` folder may stay: without the loader MORDHAU ignores them.
2. **Check that nothing was loaded on a normal start.** Note the "date modified" of `UE4SS.log` in that folder, start MORDHAU from Steam, go to the main menu, quit, and look again. If the date has not changed, the mod's script host was not loaded.
3. **Look for the loader's note.** If the loader ever had to stop a wrong start, it leaves a file named `MordhauSkyrim-closed-MORDHAU.txt` in that folder saying why. If you find it, the protection worked; delete the note and `dwmapi.dll.old`.

If anything looks wrong, remove the mod and delete the files listed under "Uninstall" before playing online. These checks show the mod is not loaded; they are not a promise about how MORDHAU's anti-cheat behaves.

## Main features

- **Directional melee with your Skyrim weapons.** Swing direction follows your mouse movement. Stabs, overheads, combos, morphs and feints work as in MORDHAU. Each Skyrim weapon stands for a MORDHAU one.
- **Timed parries and ripostes.** Blocking is a timed parry, not a held block.
- **Shields.** A Skyrim shield works as one of MORDHAU's five kinds, depending on what it is made of. The larger ones can be held up.
- **Enemies fight back the MORDHAU way.** People, draugr and skeletons with melee weapons parry, feint, riposte and step around you, up to five at a time. Followers fight the same way.
- **Alternate grips, weapon throwing, fists and kicks.**
- **Decapitations and crushed heads** on killing head hits.
- **MORDHAU's weapons as Skyrim items**, with MORDHAU's own models in the hand and in the inventory. The inventory models are made on your PC the first time you play and show from the next start of Skyrim.
- **First and third person, both with your own Skyrim character.** In first person you look out of its eyes: your arms, armour, body and legs are your Skyrim ones, with the hands placed where MORDHAU has them on the weapon.
- **Skyrim keeps what it does well.** Archers, spellcasters and creatures stay Skyrim's, and your own magic and bows are untouched.

## Requirements

- The Elder Scrolls V: Skyrim Special Edition on Steam, version **1.7.104**. Backwards compatible with version **1.6.1170** (the same file; use SKSE 2.2.6 there). Other versions are untested.
- MORDHAU, on Steam
- SKSE for your game version
- Address Library for SKSE Plugins (Nexus Mods, Skyrim Special Edition mod 32444)
- Windows 10 or 11, and about 3 GB of free disk space
- A PC that can run both games at once. As an estimate: 16 GB of RAM, a graphics card with 8 GB, and an SSD.
- Keyboard and mouse. Controllers are not supported.

## Installation

1. Install SKSE and the Address Library if you do not have them.
2. Install this mod with your mod manager (Vortex or Mod Organizer 2), like any other mod.
3. Make sure `MordhauWeapons.esp` is switched on.

There is nothing to run or copy by hand.

## How to play

1. Start Skyrim through SKSE, the way you always do with mods.
2. MORDHAU opens by itself in a window behind Skyrim. On the very first start the mod prepares MORDHAU's models at the main menu first.
3. Load a save. When "MORDHAU is live" appears at the top left, draw a melee weapon.
4. Leave MORDHAU's window open behind Skyrim. Do not minimise it and do not play in it.

| Input | What it does |
| --- | --- |
| Attack + mouse movement | Strike from that side |
| Mouse wheel up / down | Stab / overhead |
| Block | Timed parry; attack straight after for a riposte |
| Q or thumb mouse button | Feint during a windup |
| F | Kick |
| Middle mouse | Cast the spell in your left hand (with a one-handed weapon in the right) |
| R | Alternate grip, or throw the weapon |
| 4 | Fists |
| P | First / third person |

The full controls are in `HOW-TO-PLAY.md` inside the mod's folder (`SKSE\Plugins\MordhauSkyrim`).

To play Skyrim without it, switch the mod off in your mod manager or set `StartMordhau=0` in `MordhauSkyrim.ini`.

## Which MORDHAU weapon each Skyrim weapon is

A Skyrim weapon keeps its own look and fights as a MORDHAU weapon, chosen by its kind and by words in its name. The first line that fits wins; "Other" is every weapon of that kind not named before it.

**One-handed swords**

| Skyrim weapon | Fights as |
| --- | --- |
| Scimitars (of any material) | Scimitar |
| Iron | Arming Sword |
| Imperial | Arming Sword |
| Orcish, Daedric | Falchion |
| Elven | Rapier |
| Glass, Blades swords, katanas | Rapier |
| Dwarven, Ebony, Dragonbone, Stalhrim | Bastard Sword |
| Other | Arming Sword |

**Daggers**

| Skyrim weapon | Fights as |
| --- | --- |
| Iron, Steel | Knife |
| Other | Rondel Dagger |

**War axes**

| Skyrim weapon | Fights as |
| --- | --- |
| Iron, hatchets, woodcutter's axes | Hand Axe |
| Other | Axe |

**Maces**

| Skyrim weapon | Fights as |
| --- | --- |
| Orcish, Ebony, Daedric, Dragonbone | Warhammer |
| Other | Flanged Mace |

**Greatswords**

| Skyrim weapon | Fights as |
| --- | --- |
| Iron, Ancient Nord, Nord Hero | Longsword |
| Orcish | Messer |
| Dwarven | Executioner's Sword |
| Elven, Glass | Estoc |
| Ebony, Daedric, Dragonbone | Zweihander |
| Other | Greatsword |

**Battleaxes**

| Skyrim weapon | Fights as |
| --- | --- |
| Orcish | War Axe |
| Dwarven | Poleaxe |
| Elven, Glass | Halberd |
| Ebony, Daedric, Dragonbone | Bardiche |
| Other | Battle Axe |

**Warhammers**

| Skyrim weapon | Fights as |
| --- | --- |
| Orcish | Eveningstar |
| Dwarven, Elven, Glass | Polehammer |
| Other | Maul |

**MORDHAU weapons as Skyrim items.** These are added to Skyrim as their own items with MORDHAU's models, and turn up in loot, in shops and on enemies: Zweihander, Longsword, Estoc, Messer, Executioner's Sword, Halberd, Bardiche, Billhook, Spear, Battle Axe, War Axe, Polehammer, Maul, Eveningstar, Arming Sword, Bastard Sword, Falchion, Rapier, Short Sword, Axe, Hand Axe, Warhammer, Flanged Mace, Rondel Dagger, Falx.

## What it does to MORDHAU's folder

Everything happens in `Mordhau\Binaries\Win64` of your MORDHAU. Nothing of MORDHAU's own is changed or replaced.

- **Stays there:** the open-source script host UE4SS and this mod's script. On their own they do nothing; a normal MORDHAU start does not load them.
- **There only for a moment:** a small loader file, `dwmapi.dll`, the mod's own. The mod puts it in and starts MORDHAU. Before anything else runs, the loader checks that MORDHAU was started by the mod (with `-insecure` and a one-time key); only then does it load the script host, and it renames itself to `dwmapi.dll.old` at that same moment.
- **Never start MORDHAU for online play while `dwmapi.dll` is in that folder.** After a crash or a power cut, look and delete it if it is there.
- Guards are built in. If MORDHAU is started any other way while the loader is still in the folder, for example from Steam to play online, the loader ends the game within a fraction of a second, before the script host or the game itself has started, and renames itself so the next start is normal. The script checks the one-time key a second time. And if the mod's own offline session ever stops being a local match, because a server was joined or hosted, it closes the game too.
- **One setting outside that folder:** MORDHAU mutes itself while its window is not in front, and the mod's MORDHAU always runs behind Skyrim. Just before starting it, the mod sets MORDHAU's own setting for that (`Engine.ini`, `[Audio] UnfocusedVolumeMultiplier`, in `%LOCALAPPDATA%\Mordhau\Saved\Config\WindowsClient`) to 1, and puts your own value back as soon as MORDHAU is running.

## Uninstall

Remove the mod with your mod manager. Then delete from `Mordhau\Binaries\Win64`: `UE4SS.dll`, `UE4SS-settings.ini`, `UE4SS.mashup.txt`, `UE4SS.log`, the folders `UE4SS_Signatures` and `Mods`, and `dwmapi.dll` or `dwmapi.dll.old` if present. The prepared models are in `%LOCALAPPDATA%\MordhauSkyrim`; delete that folder too. The mod also writes model files for the inventory into Skyrim's `Data\meshes\MordhauSkyrim` and `Data	extures\MordhauSkyrim` (about 55 MB; with Mod Organizer they land in Overwrite); delete those two folders as well.

## Known problems

- Tight indoor spaces: enemies against walls can look stiff, and now and then a swing passes through one.
- Other combat, camera and first-person-body mods will clash with it.
- Horseback fighting, werewolf and vampire lord forms are untested.
- A MORDHAU or Skyrim update can break it.

## Reporting a problem

Say what you were doing and attach `MordhauSkyrim.log` (from `Documents\My Games\Skyrim Special Edition\SKSE`, beside the logs of your other SKSE mods; versions up to 0.1.16 wrote `DirectionalMelee.log` one folder up) and `UE4SS.log` (from MORDHAU's `Mordhau\Binaries\Win64` folder).

## Credits

- **CommonLibSSE-NG** by alandtse and contributors (GPL-3.0-or-later with a modding and linking exception; a fork of CharmedBaryon's CommonLibSSE-NG, based on CommonLibSSE by Ryan McKenzie and powerof3): the library the Skyrim plugin is built on.
- **UE4SS** v3.0.1 by the UE4SS-RE team (MIT): the script host for MORDHAU. `UE4SS.dll` and its own Lua mods are included unchanged; its settings file and mod list carry this mod's settings.
- **SkyCraft** by chasmlol (MIT): two techniques follow its way of doing them (drawing into Skyrim's frame before post-processing; hits that show the health bar and give kill credit). No file of it is included.
- **spdlog, {fmt}, DirectXTK, DirectXMath, rapidcsv, xbyak, JSON for Modern C++, SimpleIni, toml11**: libraries the plugin is built with (MIT or BSD-3-Clause).
- **Address Library for SKSE Plugins** by meh321: required, not included.
- **SKSE** by the SKSE team: required, not included.
- **MORDHAU** by Triternion and **Skyrim Special Edition** by Bethesda Game Studios: the two games this brings together.
- Made with AI assistance: the code, tools and texts of this mod were written with the help of an AI coding assistant (Claude, by Anthropic), directed and play-tested by the author.

## Licence and source

This mod is free software under the **GPL-3.0-or-later**, as the library it is built on requires. You may use it, change it and share it, including in your own mods, under that licence. The complete source of each version is under **Files** on this page, as "Source code". Every licence text is in the `LICENSES` folder of the download.
