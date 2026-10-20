# To do - the Biniwer fork

Open points found in play (as of 2026-10-20). Nothing here is built yet; pick before implementing.

## Mismatches between Skyrim and MORDHAU

1. Arrow damage: Skyrim's arrows do Skyrim's damage, not MORDHAU's (the old `enemy_arrows_mordhau` was removed upstream
   in 0.1.21 with the mod-flown arrows; would need doing anew).
2. Parrying an arrow costs no stamina; every MORDHAU parry does.
3. Arrow parry window: an arrow arriving 0.32-0.42 s into the parry counts as a hit (0.08 and 0.25 s counted as
   parried). It then sticks in the player.
4. Enemy weapon quality (material, tempering) does not change how hard an enemy hits.
5. Enemy armour does not reduce the player's damage as in Skyrim.
6. Level gap: how much a large level difference counts (`level_max`, `level_soften`).
7. Critical hits from Skyrim's perks do nothing.
8. Skyrim staggers (shield bash, Unrelenting Force) could knock the stand-in back (MORDHAU's `Knockback()`).
9. Slows on the player (an enemy's frost spell) probably do not slow the player's MORDHAU character.
10. The camera sits 10-14 cm higher than MORDHAU's.
11. Fear is cancelled while an enemy fights by MORDHAU's rules.
12. Power attacks do not exist.
13. Skyrim's maximum stamina and stamina regeneration do not affect MORDHAU: its stamina is always 0-100, Skyrim's bar
    only shows it as a share. Could scale MORDHAU's `StaminaCostModifier` / `StaminaRegenPerTick`.

Not to do: stun instead of disarm (`bAlwaysStunInsteadOfDisarm`); the spell hand's look in first person.
