"""Dev tool: bake the things MORDHAU's character can set down or throw (sheets/props.json) into cache/props/<id>/.

Each prop is one of MORDHAU's own models, read from the player's install (rigged ones through tools/skelmesh.py,
solid ones through tools/staticmesh.py), with the colour / normal / surface maps its row names. Same file format as a
character (character.dmchar), drawn by the plugin's prop loop. A rigged prop with a "shut_clip" also gets closed.txt
(the last frame of that clip: per bone local rotation and position).

  bake_props.py            bake every row
  bake_props.py <id> ...   only these
"""
import json, os, shutil, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bake_character as bc, skelmesh, staticmesh

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.join(here, '..', 'cache', 'props')
sheet = json.load(open(os.path.join(here, '..', 'sheets', 'props.json'), encoding='utf-8'))
only = set(sys.argv[1:])
bc.CAP = 1024
rigged = skelmesh.load


def any_model(base):
    """A rigged model if it reads as one, else a solid one."""
    try:
        return rigged(base)
    except Exception:
        return staticmesh.load(base)


bc.skelmesh.load = any_model
for row in sheet['rows']:
    if only and row['id'] not in only:
        continue
    out = os.path.join(root, row['id'])
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    if row.get('kind') == 'weapon':
        import character_spec as cs
        import uasset
        pieces = []
        for x in cs.weapon_pieces(row['weapon_bp'], [0] * 8, [0, 0, 0]):
            if not x['model']:
                continue
            if not x['colour_map']:
                # the part's material does not name its maps the usual way: take them by their file-name endings from
                # whichever material (or its parent) the model lists
                mp = uasset.Package(x['model'])
                seen, todo = set(), [n[6:] for n in mp.names if n.startswith('/Game/') and '.' not in n and n[6:] != x['model']]
                while todo and not x['colour_map']:
                    cand = todo.pop(0)
                    if cand in seen:
                        continue
                    seen.add(cand)
                    try:
                        c, n, r = staticmesh.textures(cand)
                    except Exception:
                        continue
                    if c:
                        x['colour_map'], x['normal_map'], x['surface_map'] = c, x.get('normal_map') or n, None
                    else:
                        try:
                            todo += [m[6:] for m in uasset.Package(cand).names if m.startswith('/Game/') and '.' not in m and ('/Materials/' in m or m.rsplit('/', 1)[-1].startswith(('M_', 'MI_', 'INST_', 'M')))]
                        except Exception:
                            pass
            if x.get('pattern_mask'):
                # (a paint mask this reader cannot open - the heater shield's - is left out: the part keeps its plain colours)
                try:
                    import texture
                    texture.info(x['pattern_mask'])
                except Exception:
                    x['pattern_mask'] = None
            if x['colour_map']:
                x['hide'] = []
                x['dye'] = [dict(t, used=True) for t in x.get('dye', [])]
                pieces.append(x)
        print(f'--- {row["id"]}: ' + ', '.join(x['part'] for x in pieces))
        try:
            if not pieces:
                raise ValueError('no parts found')
            spec = {'name': row['id'], 'no_body': True, 'pieces': pieces}
            bc.bake_char(out, [], spec)
            json.dump(spec, open(os.path.join(out, 'spec.json'), 'w', encoding='utf-8'), indent=1)
        except Exception as ex:
            shutil.rmtree(out, ignore_errors=True)
            print(f'    FAILED: {type(ex).__name__}: {ex}')
        continue
    spec = {'name': row['id'], 'no_body': True, 'pieces': [{'model': row['model'], 'colour_map': row['colour_map'], 'pattern_mask': None, 'dye': [],
                                                           'hide': [], 'normal_map': row.get('normal_map') or None, 'surface_map': row.get('surface_map') or None}]}
    print(f'--- {row["id"]}: {row["model"].split("/")[-1]}')
    try:
        bc.bake_char(out, [], spec)
        json.dump(spec, open(os.path.join(out, 'spec.json'), 'w', encoding='utf-8'), indent=1)
        if row.get('shut_clip'):
            import animdecode
            a = animdecode.decode(row['shut_clip'])
            m = rigged(row['model'])
            pose = [[list(m['pose'][i][0]), list(m['pose'][i][1])] for i in range(len(m['bones']))]
            for t in a['tracks']:
                if t['rot']:
                    pose[t['bone_index']][0] = list(t['rot'][2][-1])
                if t['trans']:
                    pose[t['bone_index']][1] = list(t['trans'][2][-1])
            open(os.path.join(out, 'closed.txt'), 'w').write('\n'.join(' '.join(f'{x:.6f}' for x in (*r, *p)) for r, p in pose) + '\n')
            print('    shut pose written')
    except Exception as ex:
        shutil.rmtree(out, ignore_errors=True)
        print(f'    FAILED: {type(ex).__name__}: {ex}')
