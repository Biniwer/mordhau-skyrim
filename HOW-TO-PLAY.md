# MORDHAU x Skyrim - how to play

**Experimental.** Both games run at once. **Skyrim is the world**: places, people, quests, loot, saves, magic, bows.
**MORDHAU is the fighter**: your melee swings, parries, feints, kicks and stamina, and the enemies you fight hand to
hand are moved and fought by MORDHAU's own AI. Nothing is copied out of either game: what you see is your own two
installs working together.

> **MORDHAU is run OFFLINE only** - a local match on your PC, in a window behind Skyrim. This does not touch MORDHAU's
> anti-cheat and must never be used for online play. Start MORDHAU from Steam as usual when you want to play it online;
> none of this is loaded then.

> **Before you play MORDHAU online again** - and after any crash - run `Make-Mordhau-Online-Safe.ps1`. It makes sure
> nothing of the mashup is left in MORDHAU's folder. See the README.

## Start

```
powershell -NoProfile -ExecutionPolicy Bypass -File Play-Mashup.ps1
```

1. A console window opens and stays open. Leave it: it starts MORDHAU, waits for its local match (about a minute),
   then starts Skyrim, and closes MORDHAU again when you quit Skyrim.
2. **The very first start** prepares MORDHAU's models from your own MORDHAU first (about 2 GB, under a minute on a fast
   disk). Skyrim opens during that; just wait for the console to say MORDHAU is live.
3. In Skyrim: load a save (use a new one or one you do not mind losing) and **draw a melee weapon**.
4. Keep MORDHAU's window open behind Skyrim. Do not minimise it and do not play in it.

In first and in third person you see your own Skyrim character, armour and weapon, moving as MORDHAU moves. In first
person you look out of its eyes and can see your arms, body and legs.

## Controls (with a melee weapon or your fists out)

| Input | What happens |
| --- | --- |
| Attack + move the mouse | A strike from the side you moved the mouse towards (MORDHAU's angles) |
| Attack again during a swing | Combo. During a windup, with another direction: morph |
| Mouse wheel up | Stab |
| Mouse wheel down | Overhead strike |
| Block | Parry - timed, it is not held. Attack straight after a parry: riposte |
| Q, or thumb mouse button | Feint, while your attack is winding up (at any other time Q is Skyrim's quick menu) |
| F | Kick |
| Middle mouse | Cast the spell in your left hand (a one-handed weapon in the right fights as MORDHAU's) |
| R | The weapon's other grip (longsword, messer, halberd ...). A weapon without one is **thrown** (axe, mace, maul, dagger ...) and lies where it lands |
| 4 | Fists: weapon and shield are put away. 4 again brings them back |
| 1 / 0 | Draw / put away the weapon |
| G | Drop the weapon. E on it picks it up again |
| E on an enemy's dropped weapon | Take it (it goes into your Skyrim inventory) |
| Left Shift | Sprint (MORDHAU's) |
| Left Ctrl (hold) | Crouch |
| P | First / third person (F is the kick) |
| V | Battle cry. C: voice menu. X: emote menu |
| F8 | How skilled the enemies are (five levels) |

Movement, jumping, looting, talking, menus and everything else are Skyrim's as usual.

**Shields.** A shield on your arm in Skyrim works as one of MORDHAU's five, by what it is made of: iron and banded ->
targe; hide, elven, glass -> buckler (both: timed parry only); steel, imperial, dwarven, orcish -> heater; guard,
Stormcloak, Nordic -> round shield; ebony, daedric, dragon -> kite (these three can also be **held up**: keep block
pressed). Kicks go through a shield. Holding R with a shield throws it.

## Who fights how

- **People, draugr and skeletons with a melee weapon (or bare hands)**, within about 10 metres of you, up to five at a
  time: MORDHAU fighters. They parry, feint, riposte and step around you like MORDHAU's bots. Plain repeated swings get
  parried - change angle, feint, morph, kick.
- **Archers** shoot the Skyrim way. You can hit them; they do not parry.
- **Anyone holding a spell or a staff** is left to Skyrim entirely. You can hit them and parry their blade.
- **Creatures** (wolves, spiders, bears, dragons ...) attack the Skyrim way; a MORDHAU parry stops their attacks.
  Blunt weapons stop in them, blades carry through.
- **Followers** near you fight as MORDHAU fighters too and pick the nearest enemy.
- **Your own magic and bows** are plain Skyrim: with a spell or a bow in the hand Skyrim has your character.
- **Damage and health are Skyrim's.** MORDHAU judges whether and where a blow lands; your level, your weapon and the
  target's armour decide what it does. A killing strike to the head takes it off; a blunt one crushes it.
- Sitting, riding, swimming, kill moves, being staggered, drawing and putting away: Skyrim's own animations.

## Known to be rough

- It was built on one PC. On yours something may simply not work; see *If something is wrong*.
- Tight indoor spaces: enemies pushed against walls can look stiff, and now and then a swing passes through one.
- A killing blow with a blunt weapon stops in the body (in MORDHAU it would carry on).
- Battles between many of Skyrim's people away from you are plain Skyrim.
- Horseback fighting, werewolf and vampire lord forms: untested.
- Other Skyrim mods that change combat, the camera or the first-person body will clash with this.
- A MORDHAU update can break it until the mashup is updated.

## If something is wrong

- **MORDHAU's character never appears / attacks do nothing:** look at the console window. If it says MORDHAU closed or
  never went live, look at MORDHAU's own window - if it shows a menu, join the match and spawn by hand once.
- **Everything is back to normal Skyrim combat in the middle of a session:** MORDHAU closed. The console starts it
  again by itself within a minute.
- **Logs to send along with a report:** `Documents\My Games\Skyrim Special Edition\SKSE\MordhauSkyrim.log` and
  `UE4SS.log` in MORDHAU's `Mordhau\Binaries\Win64` folder.
- **To get rid of it:** `Uninstall-Mashup.ps1`.

## Settings

Which mercenary: put its name alone in a file `player.txt` in the `cache` folder. Easier, simpler enemies: start with
`Play-Mashup.ps1 -SimpleEnemies`.
