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

- Damage (`damage_model` 1 in sheets/progression.json, the default): a hit takes Skyrim's own damage for it - the weapon,
  skill and perks, the target's armour, crits and sneak attacks, so better gear and other mods' weapons and perks count,
  for the player as for everyone - times how good MORDHAU says the hit was: where it landed by MORDHAU's hit record
  (head x1.5, body and arms x1, legs x0.8) and, for a stab or kick, that weapon's own MORDHAU damage for it against its
  strike. MORDHAU's weapon type and armour count no further, and no blow kills outright. Times 2.5 both ways (hits land
  less often than in Skyrim), kept between 3% and 100% of full health; Skyrim's difficulty applies on top as usual.
  `fDamageModel = 0` in Data/MCM/Settings/MordhauWeapons.ini brings back the earlier model, `fStabByWeapon = 0` counts
  stabs as strikes.
- Arrows and bolts (Skyrim's own, anybody's): Skyrim's damage by the zone they struck (head x1.5, body,
  legs x0.8: where on the body, against its head and pelvis), not raised as melee is - an arrow is no harder to land. (`skyrim_arrow_factor`)
- Power attacks (`heavy_attacks` in sheets/builds.json): holding the attack key for Skyrim's own power-attack delay makes
  the MORDHAU strike a heavy one - its windup 1.4x longer (the telegraph), a parry of it costing twice the stamina, and
  Skyrim's power-attack damage (x2, raised by power-attack perks). As in MORDHAU it costs stamina only when it misses:
  20 instead of the weapon's own miss cost, lowered by Skyrim's power-attack stamina perks. A tap is a normal strike.
  An enemy's power attack, as Skyrim's AI chooses it, makes its stand-in's strike heavy the same way, with that power
  attack's own Skyrim damage. (Done through the weapon's MORDHAU attack table, changed for that one strike.)
- Sneak attacks: Skyrim's sneak multiplier (Backstab, Assassin's Blade, Deadly Aim, any perk's) multiplies the hit's
  share of health; it was capped with the rest of Skyrim's damage before.
- Critical hits: Skyrim's crit (rolled by the game; the weapon's critical damage, raised by Bladesman, Bone Breaker, Deep
  Wounds) multiplies the hit's share of health the same way - yours and the enemies'. (`skyrim_crits`)
- Stamina drains: whatever Skyrim takes off a fought enemy's stamina - a frost spell, a stamina poison, any mod's
  effect - its MORDHAU stand-in loses too, and its MORDHAU stamina does not regenerate while it is drained. A drained
  enemy's parries break and cost it its weapon, by MORDHAU's rules. (`standin_stamina` in sheets/builds.json)
- The same for the player: whatever Skyrim drains from your stamina (an enemy's frost spell, a poison) your MORDHAU
  character loses too, and its regeneration waits meanwhile. (`player_stamina_drain`)
- Slows: a Skyrim slow on a fought enemy (a frost spell's, or any mod's) slows its stand-in's movement; Skyrim's world
  time (Slow Time) its whole pace, attacks included. (`standin_slow`)
- The same for the player: a Skyrim slow (an enemy's frost spell, or any mod's; a speed boost too) slows your MORDHAU
  character as much, and the pace set for your Skyrim body keeps it - it was cancelled out before. (`player_slow`)
- Enemies lying limp (paralysed, knocked down): the body seen in Skyrim can be struck - MORDHAU's blade reaching it is a
  clean hit, as well as MORDHAU's own verdict on its falling stand-in (once a swing) - and it no longer flies off when
  hit. (`limp_hits_skyrim`, `limp_speed_limit`)
- Enemies with a blade and a spell (a Vigilant of Stendarr): the blade fights by MORDHAU's rules (as upstream's since
  0.1.21), the spells stay Skyrim's - while one casts, its whole body is Skyrim's, so it really casts. Pure mages and
  staff users stay Skyrim's. A cast that hangs in its start (its attack broken off just after) is interrupted, so it
  does not stand there doing nothing. (`spellsword_standin`, `spellsword_cast_stuck`; 0 = Skyrim's altogether)
- A stand-in being handed its weapon is not held up by being hit: each flinch had cost a try, and a Whiterun guard
  struck again and again fought on without its sword for 9 s.
- Stamina: MORDHAU's regeneration runs as it should - it had stopped after a heavy strike or a Skyrim drain (the
  stamina was written without MORDHAU's own OffsetStamina), and while a character was hurt (MORDHAU healed it, the
  script took the health back, and MORDHAU counted that as damage: its own health regeneration is off now, Skyrim's
  health rules). After a save is loaded - after a death too - the player has the save's stamina, not the old one.
- A disarm MORDHAU makes is a disarm: the stand-in's yield right after it no longer hands the weapon straight back
  (only the disarm sound was heard).
- A shield MORDHAU knocks away (a shield parry on too little stamina) goes to the ground in Skyrim too - the player's
  and an enemy's - as the Skyrim character's own shield, picked up again the Skyrim way. (`shield_disarm_skyrim`)
- Enemies block and parry again: MORDHAU gave stand-ins the archer bot's behaviour (which hardly parries and keeps
  switching weapons); a stand-in now gets one of MORDHAU's melee bot behaviours.
- A weapon picked up in MORDHAU that stayed in the player's slots after a reload is handed over again when it is not
  the Skyrim weapon (gore and sounds went by the wrong weapon).
- A killing blow MORDHAU itself put on the head or neck bone takes the head, even when the blade line - read a frame
  late - passed a little wide.
- A character Skyrim cannot behead (no pre-made FaceGen head) loses its head the MORDHAU way: MORDHAU's head model flies.
- A severed head flies on MORDHAU's arc - Skyrim's own severed head (its separate small skeleton), not the body's head
  bone - and wears the character's helmet; looting the helmet off the body takes it off the head too.
- A weapon knocked away or dropped is the real Skyrim item (`disarm_mode`, settings menu: Gameplay): 0 = it drops to the
  ground and leaves the inventory (no more copy on the ground, no more two swords after looting); the one who lost it
  takes it back standing within a metre of it for 3 s, anyone can take it with E. 1 = it stays in the inventory, put
  away for `disarm_lockout_seconds` (8, upstream's wait), then back in the hand (not play-tested yet).
- A disarmed enemy that grabs its weapon back does so only from within a metre (it jumped into its hand from 2 m).
- An enemy that takes its bow out with the player at arm's length gets its melee weapon back in its hand, or - with none
  left - puts the bow away and fights with its fists (`standin_bow_melee_range`). A hit breaks off a bow's draw
  (`bow_hit_stagger`).
- A MORDHAU bot handed to the next enemy is armed afresh (one fought with fists after taking over from a disarmed guard).
- Archers switching between bow and sword: the stand-in only ever carries the weapon its Skyrim character holds (a bow
  left in a holster slot made MORDHAU's brain switch back and forth, and shoot with it).
- A parried or blocked Skyrim arrow no longer plays the player's being hit; it sounds your MORDHAU weapon's (or
  shield's) block, with Skyrim's block and bounce.
- MORDHAU's music, its arena's ambience and its footsteps are silent in the mod's session; its fighting and voices are
  heard.
- The settings menu's MORDHAU volume and voice choices are applied at every start (they were often lost).

The stamina, slow and limp-enemy additions can be switched off in sheets/builds.json (then `tools/gen_code.py` and a new build).
