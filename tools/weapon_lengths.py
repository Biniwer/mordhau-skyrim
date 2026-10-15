"""Dev: how long each of MORDHAU's melee weapons is (its default parts put together, longest side of the box around
them, cm), read from the player's own MORDHAU - to compare with the Skyrim weapons they stand for.
usage: weapon_lengths.py"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import character_spec as cs, skelmesh, staticmesh

WEAPONS = ['ArmingSword', 'Axe', 'BastardSword', 'Corseca', 'Dagger', 'Falchion', 'HandAxe', 'Knife', 'Mace', 'Mallet', 'MeatCleaver', 'Rapier', 'Scimitar', 'Shortspear', 'Shortsword',
           'SmithHammer', 'Warhammer', 'Bardiche', 'BattleAxe', 'Billhook', 'Halberd', 'Poleaxe', 'Polehammer', 'Quarterstaff', 'Spear', '2HStick', '2Hmace', 'LargeBone', 'Maul',
           'Ancient_Falx', 'Estoc', 'ExecutionerSword', 'Falx', 'Federschwert', 'Greatsword', 'Longsword', 'Messer', 'WarAxe', 'Zweihander', 'Pickaxe', 'Sledgehammer', 'Sickle', 'Scythe',
           'Pitchfork', 'Hoe', 'MetalShovel', 'Stick', 'FemurBone', 'MetalFork', 'FryingPan', 'Torch']


def load(model):
    try:
        return skelmesh.load(model)
    except Exception:
        return staticmesh.load(model)


for name in WEAPONS:
    try:
        pieces = cs.weapon_pieces('BP_' + name, [0] * 8, [0, 0, 0])
        lo, hi = [1e9] * 3, [-1e9] * 3
        parts = 0
        for x in pieces:
            if not x['model']:
                continue
            m = load(x['model'])
            import numpy as np
            pos = np.asarray(m['positions'] if isinstance(m, dict) else m.positions, dtype=float).reshape(-1, 3)
            for k in range(3):
                lo[k] = min(lo[k], float(pos[:, k].min())); hi[k] = max(hi[k], float(pos[:, k].max()))
            parts += 1
        if not parts:
            print(f'{name}: no parts read')
            continue
        size = sorted((hi[k] - lo[k] for k in range(3)), reverse=True)
        chain = cs._chain('BP_' + name)
        two = None
        for cname, p, props in chain:
            if 'bIsTwoHanded' in props and two is None:
                two = p.value(*props['bIsTwoHanded'])
        print(f'{name}: {size[0]:.0f} cm long, {size[1]:.0f} wide; {parts} part(s); two-handed in MORDHAU: {two}; from {chain[1][0] if len(chain) > 1 else "?"}')
    except Exception as e:
        print(f'{name}: could not be read ({type(e).__name__}: {str(e)[:80]})')
