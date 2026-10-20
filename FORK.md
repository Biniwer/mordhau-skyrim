# MORDHAU x Skyrim - the Biniwer fork

This fork (branch `linux-epic`, https://github.com/Biniwer/mordhau-skyrim) adds to the original
(https://github.com/thnokkvi-lang/mordhau-skyrim) what it takes to play on Linux with MORDHAU from the Epic Games
Store, a few gameplay additions, and a CI build. Everything else - and how to play - is the original's: HOW-TO-PLAY.md.

## Linux and the Epic Games Store

- Skyrim SE on Steam under Proton, MORDHAU from the Epic Games Store installed by Heroic. MORDHAU runs inside Skyrim's
  Proton prefix; Heroic's own MORDHAU prefix (the one for online play) is never touched.
- `linux/install.sh` - installs a build into Skyrim's Data folder: sets MordhauPath to the Heroic install, makes
  MordhauWeapons.esp from your own Skyrim.esm and switches it on, installs the VC++ 2022 runtime into Skyrim's prefix,
  and tells Wine (Skyrim's prefix only) to load the mod's loader for MORDHAU. Finds Skyrim in any Steam library.
- `linux/skyrim-launch.sh` - the Steam launch option (`.../linux/skyrim-launch.sh %command%`): starts SKSE, takes the
  mod's loader out of MORDHAU's folder before and after, and keeps a fresh Epic login for MORDHAU (from Heroic's
  bundled legendary) - the first one before Skyrim starts, a new one after every MORDHAU start. The Epic build quits
  with "cannot reach backend services" without it.
- `linux/online-safe.sh` - checks by hand that no file of the mod's loader is left in MORDHAU's folder.
- The Epic build's object list sits elsewhere than the Steam build's: `mordhau_side/UE4SS_Signatures/GUObjectArray.lua`
  tells them apart.
- UE4SS: the experimental build v3.0.1-1176 (3.0.1 corrupts this MORDHAU's memory at startup). The script works around
  two of its bugs (its Lua mutex held in hooks; Lua lists inside structs).

## Building

- `.github/workflows/build.yml` - every push builds the plugin and the loader on Windows and packages them with UE4SS
  in Skyrim's Data layout (artifact `MordhauSkyrim-Data`). UE4SS 1176 is kept in this fork's release
  `ue4ss-v3.0.1-1176-g1ba38db4`, since upstream replaced it.
- `linux/build-cross.sh` - builds the plugin on Linux in seconds (clang-cl, lld-link, Microsoft's SDK through xwin; see
  the setup at its top and `linux/cross/`) and puts it, with MORDHAU's script, into the mod manager's folder of the
  bridge, with the loader and the script's bones helper. CI's build stays the reference for releases.
- Upstream's 0.1.22 is merged in; `plugin/CMakeLists.txt` skips upstream's dev test program, whose source is not
  published.

## Gameplay

- Sneak attacks: Skyrim's sneak multiplier (Backstab, Assassin's Blade, Deadly Aim, any perk's) multiplies the hit's
  share of health; it was capped with the rest of Skyrim's damage before.
- Stamina drains: whatever Skyrim takes off a fought enemy's stamina - a frost spell, a stamina poison, any mod's
  effect - its MORDHAU stand-in loses too, and its MORDHAU stamina does not regenerate while it is drained. A drained
  enemy's parries break and cost it its weapon, by MORDHAU's rules. (`standin_stamina` in sheets/builds.json)
- The same for the player: whatever Skyrim drains from your stamina (an enemy's frost spell, a poison) your MORDHAU
  character loses too, and its regeneration waits meanwhile. (`player_stamina_drain`)
- Slows: a Skyrim slow on a fought enemy (a frost spell's, or any mod's) slows its stand-in's movement; Skyrim's world
  time (Slow Time) its whole pace, attacks included. (`standin_slow`)
- Enemies lying limp (paralysed, knocked down): the body seen in Skyrim can be struck - MORDHAU's blade reaching it is a
  clean hit, as well as MORDHAU's own verdict on its falling stand-in (once a swing) - and it no longer flies off when
  hit. (`limp_hits_skyrim`, `limp_speed_limit`)
- Enemies with a blade and a spell (a Vigilant of Stendarr): the blade fights by MORDHAU's rules (as upstream's since
  0.1.21), the spells stay Skyrim's - while one casts, its whole body is Skyrim's, so it really casts. Pure mages and
  staff users stay Skyrim's. A cast that hangs in its start (its attack broken off just after) is interrupted, so it
  does not stand there doing nothing. (`spellsword_standin`, `spellsword_cast_stuck`; 0 = Skyrim's altogether)
- Archers switching between bow and sword: the stand-in only ever carries the weapon its Skyrim character holds (a bow
  left in a holster slot made MORDHAU's brain switch back and forth, and shoot with it).
- A parried or blocked Skyrim arrow no longer plays the player's being hit; it sounds your MORDHAU weapon's (or
  shield's) block, with Skyrim's block and bounce.
- MORDHAU's music, its arena's ambience and its footsteps are silent in the mod's session; its fighting and voices are
  heard.
- The settings menu's MORDHAU volume and voice choices are applied at every start (they were often lost).

The stamina, slow and limp-enemy additions can be switched off in sheets/builds.json (then `tools/gen_code.py` and a new build).
