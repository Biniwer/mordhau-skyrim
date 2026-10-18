# MORDHAU x Skyrim (experimental)

MORDHAU's melee combat in Skyrim Special Edition.

The mod runs your own copy of MORDHAU offline next to Skyrim and lets it do the fighting. Skyrim stays the world:
places, people, quests, loot, saves, magic and bows. MORDHAU does the melee: your swings, parries, feints, kicks and
stamina, and the people you fight are moved and fought by MORDHAU's bots. Damage, health and levels stay Skyrim's.

## Read this first

- You need both games on Steam. No game files are in the download. The first time you play, the mod prepares
  MORDHAU's models and animations from your own copy (about 2 GB, once).
- Offline only. The mod starts MORDHAU itself in a local match in a window behind Skyrim, and closes it when Skyrim
  closes. MORDHAU has anti-cheat for online play. The mod doesn't touch or work around it, and must never be used
  online.
- No guarantee for your MORDHAU account. The mod only ever runs inside its own offline MORDHAU, started with
  `-insecure`, and shuts the game down if it's started any other way. Even so, nobody, me included, can promise how
  the anti-cheat treats it. If you can't accept any risk to your MORDHAU account, don't install it.
- It puts files in MORDHAU's folder (see below).
- Experimental. It was built on one PC and has barely been tried anywhere else. Expect bugs, and use a new save.
- Not affiliated with or endorsed by Bethesda or Triternion.

## How online play is kept separate

Nothing of the mod is loaded when you start MORDHAU normally:

1. The mod only starts its own offline MORDHAU: a local match, with `-insecure` and a one-time key for that launch.
2. MORDHAU only loads the mod's script if a small loader (`dwmapi.dll`) is in its folder, and the loader checks for
   `-insecure` and the key first. Without both it closes the game within a fraction of a second, loads nothing, and
   renames itself so the next start is normal.
3. Once it has done its job the loader renames itself to `dwmapi.dll.old`, which MORDHAU doesn't load. The mod deletes
   it when MORDHAU closes or at the next session.
4. The script checks the key again, and closes the game if the session ever becomes a joined or hosted server.
5. A MORDHAU you started yourself is never touched. If it's already running when Skyrim starts, the mod waits until
   you close it.

What isn't verified: the check while the game runs relies on asking MORDHAU's engine "is this still a standalone local
match?". The shutdown after a "no" or an unclear answer is tested. Whether the engine says "no" for every way a session
could go online, and how early, is not - testing that would mean taking the mod online. The launch check, `-insecure`
and the loader removing itself don't depend on it.

## Checking it yourself

After closing Skyrim and before playing MORDHAU online:

1. Open MORDHAU's `Mordhau\Binaries\Win64` folder. There must be no `dwmapi.dll`. Delete `dwmapi.dll.old` if it's
   there. `UE4SS.dll`, `UE4SS-settings.ini` and the `Mods` folder can stay; without the loader MORDHAU ignores them.
2. Note the "date modified" of `UE4SS.log` there, start MORDHAU from Steam, go to the main menu and quit. If the date
   hasn't changed, the script host wasn't loaded.
3. If you find `MordhauSkyrim-closed-MORDHAU.txt`, the loader stopped a wrong start and says why. Delete it and
   `dwmapi.dll.old`.

If anything looks wrong, remove the mod and the files under "Uninstall" before playing online. These checks show the
mod isn't loaded; they aren't a promise about how MORDHAU's anti-cheat behaves.

## Features

- Directional melee with your Skyrim weapons. The swing follows your mouse; stabs, overheads, combos, morphs and feints
  work as in MORDHAU. Each Skyrim weapon fights as a MORDHAU one (tables below).
- Timed parries and ripostes instead of a held block.
- Shields: a Skyrim shield works as one of MORDHAU's five, by material. The bigger ones can be held up.
- Everyone fighting near you fights the MORDHAU way - enemies and followers, up to 12 at once within 10 metres by
  default (both adjustable). They parry, feint, riposte and move around you. Archers hold a MORDHAU bow, so they can't
  parry.
- Alternate grips, throwing weapons, fists and kicks.
- Heads come off (or get crushed) on killing head hits.
- MORDHAU's weapons as Skyrim items, with MORDHAU's models in the hand and in the inventory. The inventory models are
  made on your PC the first time you play and show from the next start.
- First and third person with your own Skyrim character. In first person you see your own arms, armour and legs, with
  the hands where MORDHAU has them on the weapon.
- Enemy skill follows the level gap: far above your level they fight like duelists, far below like peasants.
- Settings menu (with SkyUI and MCM Helper): damage, armour, enemy skill, how many people fight MORDHAU's way and how
  far around you, kicks (including a super kick), your character's voice, a sound mix from all Skyrim to all MORDHAU,
  crouch toggle or hold, and more.
- Skyrim keeps what it does well: spellcasters and creatures fight the Skyrim way, and your own magic and bows are
  untouched.

## Requirements

- Skyrim Special Edition on Steam, version 1.7.104, or 1.6.1170 (same file; use SKSE 2.2.6 there). Other versions are
  untested.
- MORDHAU on Steam
- SKSE for your game version
- Address Library for SKSE Plugins (Nexus mod 32444)
- Optional: SkyUI and MCM Helper for the settings menu. Without them the defaults are used.
- Windows 10 or 11, about 3 GB free
- A PC that can run both games at once - roughly 16 GB of RAM, an 8 GB graphics card and an SSD
- Keyboard and mouse (no controller support)

## Installation

1. Install SKSE and Address Library if you don't have them.
2. Install the mod with Vortex or Mod Organizer 2.
3. Make sure `MordhauWeapons.esp` is enabled.

Nothing else to run or copy.

## Playing

1. Start Skyrim through SKSE as usual.
2. MORDHAU opens by itself behind Skyrim. On the very first start the mod prepares MORDHAU's models at the main menu
   first.
3. Load a save. When "MORDHAU is live" shows at the top left, draw a melee weapon.
4. Leave MORDHAU's window open behind Skyrim. Don't minimise it or play in it.

| Input | What it does |
| --- | --- |
| Attack + mouse movement | Strike from that side |
| Mouse wheel up / down | Stab / overhead |
| Block | Timed parry; attack right after to riposte |
| Q or thumb mouse button | Feint during a windup |
| F | Kick |
| Middle mouse | Cast the spell in your left hand (with a one-handed weapon in the right) |
| R | Alternate grip, or throw the weapon |
| 4 | Fists |
| P | First / third person |

All controls are in `HOW-TO-PLAY.md` in the mod's folder (`SKSE\Plugins\MordhauSkyrim`).

To play Skyrim without it, disable the mod or set `StartMordhau=0` in `MordhauSkyrim.ini`.

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

## What goes into MORDHAU's folder

Everything is in `Mordhau\Binaries\Win64`. Nothing of MORDHAU's own is changed or replaced.

- These stay: UE4SS (an open-source script host) and the mod's script. On their own they do nothing; a normal MORDHAU
  start doesn't load them.
- Only there for a moment: the mod's small loader, `dwmapi.dll`. The mod puts it in and starts MORDHAU. Before anything
  else runs, the loader checks that the mod started MORDHAU (`-insecure` and the one-time key); only then does it load
  the script host, renaming itself to `dwmapi.dll.old` in the same moment.
- The script's bone reader, `MordhauBones.dll`, never goes into MORDHAU's folder. It's copied to
  `%LOCALAPPDATA%\MordhauSkyrim` for each session and loaded from there.
- Never start MORDHAU for online play while `dwmapi.dll` is in that folder. After a crash or a power cut, check and
  delete it if it's there.
- If MORDHAU is started any other way while the loader is still there - from Steam to play online, say - the loader
  closes the game within a fraction of a second, before the script host or the game itself has started, and renames
  itself so the next start is normal. The script checks the key again, and if the mod's own session ever stops being
  a local match, it closes the game too.
- One setting outside that folder: MORDHAU mutes itself when its window isn't in front, and the mod's MORDHAU always
  runs behind Skyrim. Right before starting it, the mod sets `[Audio] UnfocusedVolumeMultiplier` in MORDHAU's
  `Engine.ini` (`%LOCALAPPDATA%\Mordhau\Saved\Config\WindowsClient`) to 1, and puts your own value back as soon as
  MORDHAU is running.

## Uninstall

Remove the mod in your mod manager. Then delete these from `Mordhau\Binaries\Win64`: `UE4SS.dll`,
`UE4SS-settings.ini`, `UE4SS.mashup.txt`, `UE4SS.log`, the `UE4SS_Signatures` and `Mods` folders, and `dwmapi.dll` or
`dwmapi.dll.old` if present. Also delete `%LOCALAPPDATA%\MordhauSkyrim` (the prepared models and the bone reader), and
the inventory model folders `Data\meshes\MordhauSkyrim` and `Data\textures\MordhauSkyrim` (about 55 MB; with Mod
Organizer they end up in Overwrite).

## Network and saves

- The mod makes no internet connections and collects nothing: no telemetry, no update checks. The two games
  themselves still do what they always do (Steam, for one).
- It keeps nothing in your saves. If you remove it, the MORDHAU weapons from `MordhauWeapons.esp` disappear from
  inventories; that's the only trace. Removing it in the middle of a playthrough hasn't been tested much yet.

## Known problems

- People sometimes take a moment to draw their weapon.
- In tight indoor spaces fighters can bump into tables and walls, and now and then a swing passes through someone.
- Other combat, camera and first-person body mods will clash with it.
- Horseback fighting and werewolf or vampire lord forms are untested.
- A MORDHAU or Skyrim update can break it.

## Reporting a problem

Say what you were doing and attach `MordhauSkyrim.log` (in `Documents\My Games\Skyrim Special Edition\SKSE`, next to
your other SKSE mods' logs; versions up to 0.1.16 wrote `DirectionalMelee.log` one folder up) and `UE4SS.log` (in
MORDHAU's `Mordhau\Binaries\Win64` folder).

## Credits

- CommonLibSSE-NG by alandtse and contributors (GPL-3.0-or-later with a modding and linking exception; a fork of
  CharmedBaryon's CommonLibSSE-NG, based on CommonLibSSE by Ryan McKenzie and powerof3): the library the Skyrim plugin
  is built on.
- UE4SS v3.0.1 by the UE4SS-RE team (MIT): the script host for MORDHAU. `UE4SS.dll` is included unchanged; its
  settings file and mod list carry this mod's settings.
- SkyCraft by chasmlol (MIT): two techniques follow its approach (drawing into Skyrim's frame before post-processing;
  hits that show the health bar and give kill credit). No file of it is included.
- True Directional Movement by Ersh (GPL-3.0): the plugin includes its modder interface header, so that a running True
  Directional Movement can be asked to leave the player's movement, turning and target lock alone while MORDHAU is
  linked. No other file of it is included.
- spdlog, {fmt}, DirectXTK, DirectXMath, rapidcsv, xbyak, JSON for Modern C++, SimpleIni, toml11: libraries the plugin
  is built with (MIT or BSD-3-Clause).
- SkyUI by the SkyUI team and MCM Helper by Parapets: the settings menu is built on them (optional, not included; the
  menu's script was compiled against their published script headers).
- Address Library for SKSE Plugins by meh321, and SKSE by the SKSE team: required, not included.
- MORDHAU by Triternion and Skyrim Special Edition by Bethesda Game Studios: the two games this brings together.
- Made with AI help: the code, tools and texts were written with an AI coding assistant (Claude, by Anthropic),
  directed and play-tested by me.

## Licence and source

GPL-3.0-or-later, as the library it's built on requires. You can use, change and share it, including in your own mods,
under that licence. The full source of each version is under Files on this page ("Source code"). All licence texts are in the `LICENSES` folder of the download.
