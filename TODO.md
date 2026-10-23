# To do - the Biniwer fork

Open points found in play (as of 2026-10-23). Nothing here is built yet; pick before implementing.

## Mismatches between Skyrim and MORDHAU

1. Arrow damage: Skyrim's arrows do Skyrim's damage, not MORDHAU's; the player's arrows still go by the earlier damage
   model (`HitFraction`), not `damage_model` 1 (the old `enemy_arrows_mordhau` was removed upstream
   in 0.1.21 with the mod-flown arrows; would need doing anew).
2. Skyrim staggers (shield bash, Unrelenting Force) could knock the stand-in back (MORDHAU's `Knockback()`).
3. The camera sits 10-14 cm higher than MORDHAU's.
4. Fear is cancelled while an enemy fights by MORDHAU's rules.
5. Power attacks do not exist (Skyrim's enemies hit much harder with them: low-level enemies feel soft without).
6. Skyrim's maximum stamina and stamina regeneration do not affect MORDHAU: its stamina is always 0-100, Skyrim's bar
    only shows it as a share. Could scale MORDHAU's `StaminaCostModifier` / `StaminaRegenPerTick`.

Done by `damage_model` 1 (2026-10-23): enemy weapon quality, enemy armour and the level gap all come with Skyrim's own
damage now.

Not to do: stun instead of disarm (`bAlwaysStunInsteadOfDisarm`); the spell hand's look in first person; a wider
parry window for arrows (too easy - an arrow 0.32-0.42 s into a parry counts as a hit, as it should); a stamina cost for
parrying arrows (MORDHAU charges none for them).
