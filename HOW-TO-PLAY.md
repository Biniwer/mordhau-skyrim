# MORDHAU x Skyrim - how to play

Experimental. Both games run at once. Skyrim is the world: places, people, quests, loot, saves, magic and bows.
MORDHAU does the fighting: your melee swings, parries, feints, kicks and stamina, and the people you fight hand to hand
are moved and fought by MORDHAU's bots. Nothing is copied out of either game; it's your own two installs working
together.

> MORDHAU only runs offline here: a local match on your PC, in a window behind Skyrim, started and closed by the mod.
> It doesn't touch MORDHAU's anti-cheat and must never be used for online play. To play MORDHAU online, start it from
> Steam as usual - none of this is loaded then.

> After a crash, before you play MORDHAU online again: look in MORDHAU's `Mordhau\Binaries\Win64` folder and delete
> `dwmapi.dll` if it's there. See the README.

## Starting

Start Skyrim through SKSE from your mod manager, as usual.

1. MORDHAU opens by itself in a window and Skyrim keeps the keyboard. It takes one to four minutes to open its match
   and spawn your mercenary. Skyrim plays normally until then.
2. On the very first start the mod prepares MORDHAU's models from your copy of MORDHAU once Skyrim's main menu is up
   (about 2 GB, under a minute on a fast disk). MORDHAU starts after that.
3. Load a save (a new one, or one you don't mind losing). When "MORDHAU is live" shows at the top left, draw a melee
   weapon.
4. Leave MORDHAU's window open behind Skyrim. Don't minimise it or play in it. It closes with Skyrim.

In first person you see through your character's eyes, with MORDHAU's arms. In third person you see your Skyrim
character moving the way MORDHAU moves.

## Controls (melee weapon or fists out)

| Input | What it does |
| --- | --- |
| Attack + move the mouse | A strike from the side you moved the mouse towards (MORDHAU's angles) |
| Attack again during a swing | Combo. During a windup, with another direction: morph |
| Mouse wheel up | Stab |
| Mouse wheel down | Overhead |
| Block | Parry. It's timed, not held. Attack right after a parry to riposte |
| Q, or thumb mouse button | Feint while your attack winds up (otherwise Q is Skyrim's quick menu) |
| F | Kick |
| Middle mouse | Cast the spell in your left hand (a one-handed weapon in the right still fights MORDHAU's way) |
| R | The weapon's other grip (longsword, messer, halberd...). Weapons without one are thrown (axe, mace, maul, dagger...) and lie where they land |
| 4 | Fists: weapon and shield put away. 4 again brings them back |
| 1 / 0 | Draw / put away |
| G | Drop the weapon. E on it picks it up |
| E on an enemy's dropped weapon | Take it (it goes into your Skyrim inventory) |
| Left Shift | Sprint |
| Left Ctrl | Crouch (toggle, or hold - see the settings) |
| P | First / third person (F is the kick) |
| V | Battle cry. C: voice menu. X: emote menu |
| F8 | Enemy skill (five levels) |

Moving, jumping, looting, talking and menus are Skyrim's as usual.

Shields: a shield in Skyrim works as one of MORDHAU's five, by material. Iron and banded become a targe; hide, elven
and glass a buckler (both only for timed parries); steel, imperial, dwarven and orcish a heater; guard, Stormcloak and
Nordic a round shield; ebony, daedric and dragon a kite. The last three can also be held up: keep block pressed. Kicks
go through a shield. Holding R with a shield throws it.

## Who fights how

- Everyone fighting within 10 metres of you fights by MORDHAU's rules, nearest first, up to 12 at a time - enemies and
  allies alike. Both numbers can be changed in the settings (5-20 m, 1-24 people). They parry, feint, riposte and step
  around you like MORDHAU's bots. Repeating the same swing gets it parried: change angle, feint, morph, kick.
- Archers still shoot the Skyrim way. In MORDHAU they hold a matching bow, so they can't parry; your melee hits on them
  are judged by MORDHAU.
- Anyone with a spell or a staff in hand is left to Skyrim. You can hit them and parry their blade.
- Creatures (wolves, spiders, bears, dragons...) attack the Skyrim way, and a MORDHAU parry stops their attacks. Blunt
  weapons stop in them, blades carry through.
- Your own magic and bows are plain Skyrim.
- Damage and health are Skyrim's. MORDHAU decides whether and where a blow lands; your level, your weapon and the
  target's armour decide how much it does. A killing strike to the head takes it off; a blunt one crushes it.
- Sitting, riding, swimming, kill moves, staggers, drawing and sheathing use Skyrim's own animations.

## Rough edges

- It was built on one PC, so something may just not work on yours. See below.
- In tight indoor spaces fighters can bump into tables and walls, and now and then a swing passes through someone.
- A killing blow with a blunt weapon stops in the body (in MORDHAU it would carry on).
- Fights between Skyrim's people further away than the radius are plain Skyrim.
- Fighting on horseback, and werewolf or vampire lord forms, are untested.
- Other mods that change combat, the camera or the first-person body will clash with this.
- A MORDHAU update can break it until the mod is updated.

## If something's wrong

- MORDHAU's character never shows up, or attacks do nothing: look at MORDHAU's window. If it shows a menu, join the
  match and spawn by hand once. If there's no MORDHAU window at all, check `MordhauSkyrim.log` (Documents\My Games\
  Skyrim Special Edition\SKSE).
- Combat goes back to normal Skyrim in the middle of a session: MORDHAU closed. The mod starts it again within a
  minute.
- Logs to send with a report: `Documents\My Games\Skyrim Special Edition\SKSE\MordhauSkyrim.log`, and `UE4SS.log` in
  `%LOCALAPPDATA%\MordhauSkyrim\cache\ue4ss`.
- To remove it: see "Remove" in the README.

## Settings

In game, with SkyUI and MCM Helper: Esc -> System -> Mod Configuration -> MORDHAU x Skyrim. Combat, how many people
fight MORDHAU's way and how far around you, sound mix, your character's voice, crouch toggle or hold, enemy skill, and
whether MORDHAU starts with Skyrim.

`MordhauSkyrim.ini` next to the plugin (`Data\SKSE\Plugins`): where MORDHAU is if Steam doesn't know, easier enemies
(`SimpleEnemies=1`), the size of MORDHAU's window, or the mod switched off (`StartMordhau=0`). To pick a mercenary, put
its name alone in a file called `player.txt` in the cache folder (`%LOCALAPPDATA%\MordhauSkyrim\cache`).
