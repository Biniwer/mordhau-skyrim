"""Turn one of the player's own MORDHAU character profiles into a full description of how it looks. Read-only.

Reads the profile from the player's Game.ini and everything else from the player's MORDHAU install: each wearable's model,
colour map, the pattern's colour-mask texture, which colour table each colour slot uses, the colour values themselves,
and the item's hide flags. Nothing is invented: where MORDHAU's data does not say (noted below) the choice is marked.

spec(name) -> dict; run as a script to print it and write cache/<name>.json.
"""
import json, os, re, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import uasset
import profile as pf

ORDER = ('Head', 'Coif', 'UpperChest', 'LowerChest', 'Shoulders', 'Arms', 'Hands', 'Legs', 'Feet')  # last three: inferred order
DEFAULT_TABLES = [0, 0]  # an item with no ColorTables of its own: Cloth for both colours (inferred from valid indices)


def _chain(bp):
    out = []
    while bp:
        path = pf._find(bp)
        if not path:
            break
        p = uasset.Package(path)
        props, _ = p.props(next(e for e in p.exports if e['name'] == 'Default__' + bp + '_C')['offset'])
        out.append((bp, p, props))
        cls = next((e for e in p.exports if e['name'] == bp + '_C'), None)
        par = p.obj(cls['super']) if cls else None
        bp = par[:-2] if par and par.endswith('_C') else None
    return out


def _tex_path(p, obj_name):
    hit = next((path for n, path in p.imports_of('Texture2D') if n == obj_name), None)
    return hit.replace('/Game/', '') if hit else None


_tables = None


def colour_tables():
    """[(table name, [linear rgb per entry])] from BP_MordhauSingleton and the colour definitions it lists."""
    global _tables
    if _tables is not None:
        return _tables
    s = uasset.Package('Mordhau/Blueprints/BP_MordhauSingleton')
    props, _ = s.props(next(e for e in s.exports if e['name'].startswith('Default__'))['offset'])
    raw = props['ColorTables'][1]
    count, = struct.unpack_from('<i', raw, 0)
    save = s.e; s.e = raw
    o = 4; _, o = s.name(o); _, o = s.name(o); o += 8; _, o = s.name(o); o += 17
    out = []
    for _ in range(count):
        sub, o = s.props(o)
        nraw = sub['TableName'][1]; name = ''
        for q in range(len(nraw) - 5, 0, -1):
            n, = struct.unpack_from('<i', nraw, q)
            if 1 < n < 64 and q + 4 + n == len(nraw):
                name = nraw[q+4:q+4+n-1].decode('utf-8', 'replace'); break
        eraw = sub['Entries'][1]; en, = struct.unpack_from('<i', eraw, 0)
        colours = []
        for i in range(en):
            bp = s.obj(struct.unpack_from('<i', eraw, 4 + 4*i)[0])[:-2]
            c = uasset.Package(pf._find(bp))
            cp, _ = c.props(next(e for e in c.exports if e['name'].startswith('Default__'))['offset'])
            colours.append([round(x, 5) for x in struct.unpack('<4f', cp['Color'][1])[:3]] if 'Color' in cp else None)
        out.append((name, colours))
    s.e = save
    _tables = out
    return out


def single_table(prop):
    """(table name, [linear rgb]) for a one-table property of the singleton, e.g. MetalTintsColorTable."""
    s = uasset.Package('Mordhau/Blueprints/BP_MordhauSingleton')
    props, _ = s.props(next(e for e in s.exports if e['name'].startswith('Default__'))['offset'])
    raw = props[prop][1]
    save = s.e; s.e = raw
    sub, _ = s.props(0)
    s.e = save
    eraw = sub['Entries'][1]; en, = struct.unpack_from('<i', eraw, 0)
    colours = []
    for i in range(en):
        bp = s.obj(struct.unpack_from('<i', eraw, 4 + 4*i)[0])[:-2]
        c = uasset.Package(pf._find(bp))
        cp, _ = c.props(next(e for e in c.exports if e['name'].startswith('Default__'))['offset'])
        colours.append([round(x, 5) for x in struct.unpack('<4f', cp['Color'][1])[:3]] if 'Color' in cp else None)
    return prop.replace('ColorTable', ''), colours


def weapon_pieces(bp, parts, colours, skin=0):
    """The models that make up a modular weapon as the profile configured it: one piece per chosen part, with the part
    material's colour map and colour mask and the profile's three colours (mask red, green, blue = colour 1, 2, 3)."""
    chain = _chain(bp)
    merged, pkgs = {}, {}
    for cname, p, props in reversed(chain):
        for k, v in props.items():
            merged[k] = v; pkgs[k] = p
    skins = pkgs['Skins'].value(*merged['Skins']) if 'Skins' in merged else []
    if not skins:
        return []
    sk = skins[min(skin, len(skins) - 1)]
    tables = colour_tables()
    item_tables = sk.get('ColorTables') if isinstance(sk.get('ColorTables'), list) else [0, 0, 0]
    tints = []
    for i in range(3):
        t = item_tables[i] if i < len(item_tables) else 0
        c = colours[i] if i < len(colours) else 0
        tints.append({'slot': i + 1, 'channel': i, 'table': tables[t][0], 'index': c, 'linear_rgb': tables[t][1][c] if c < len(tables[t][1]) else None})
    out = []
    for i, pt in enumerate(sk['PartTypes']):
        options = pt['Parts'] if isinstance(pt['Parts'], list) else []
        if not options:
            continue
        part = options[min(parts[i] if i < len(parts) else 0, len(options) - 1)][:-2]
        q = uasset.Package(pf._find(part))
        d = q.defaults('Default__' + part + '_C')
        mesh = next((n for n in q.names if n.startswith('/Game/') and n.rsplit('/', 1)[-1] == d.get('SkeletalMesh')), None)
        if not mesh and d.get('SkeletalMesh') and re.fullmatch(r'.*_(0|[1-9][0-9]*)', d['SkeletalMesh']):
            # (a model name ending in _<number>: the name table has its package path under the base name only)
            base, number = d['SkeletalMesh'].rsplit('_', 1)
            mesh = next((n + '_' + number for n in q.names if n.startswith('/Game/') and n.rsplit('/', 1)[-1] == base), None)
        if not mesh:
            continue
        mesh = mesh.replace('/Game/', '')
        mp = uasset.Package(mesh)
        # (the part's material: named M_... or MI_... for weapons, INST_... for shields)
        mat_name = next((n for n in mp.names if n.startswith('/Game/') and n.rsplit('/', 1)[-1].startswith(('M', 'INST_'))), None)
        colour_map = mask = normal = rma = None
        if mat_name:
            mat = uasset.Package(mat_name.replace('/Game/', ''))
            md = mat.defaults(mat_name.rsplit('/', 1)[-1])
            for tp in md.get('TextureParameterValues', []):
                if tp['ParameterInfo']['Name'] == 'AlbedoMap': colour_map = _tex_path(mat, tp['ParameterValue'])
                if tp['ParameterInfo']['Name'] == 'ColorMap': mask = _tex_path(mat, tp['ParameterValue'])
                if tp['ParameterInfo']['Name'] == 'NormalMap': normal = _tex_path(mat, tp['ParameterValue'])
                if tp['ParameterInfo']['Name'] == 'RoughnessMap': rma = _tex_path(mat, tp['ParameterValue'])
        out.append({'part': part, 'model': mesh, 'colour_map': colour_map, 'pattern_mask': mask, 'normal_map': normal, 'surface_map': rma, 'dye': tints})
    return out


def spec(name, items=None):
    """name: one of the player's mercenaries. items (optional): {slot: (wearable definition, [colour indices], pattern)}
    for an outfit put together from MORDHAU's wearables instead (no profile: no emblem, no weapon, first metal tint)."""
    if items is None:
        me = next(p for p in pf.read_profiles() if p[0].lower() == name.lower())
        resolved = pf.resolve([e[0] for e in me[1]], ORDER)
        entries = [(slot, resolved[slot], item_id, colours, pattern) for slot, (item_id, colours, pattern) in zip(ORDER, me[1])]
    else:
        me = None
        entries = [(slot, items[slot][0], -1, items[slot][1], items[slot][2]) for slot in ORDER]
    tables = colour_tables()
    pieces = []
    for slot, bp, item_id, colours, pattern in entries:
        merged, pkgs = {}, {}
        for cname, p, props in reversed(_chain(bp)):      # parent first, the item itself last
            for k, v in props.items():
                merged[k] = v; pkgs[k] = p
        def val(k):
            return pkgs[k].value(*merged[k]) if k in merged else None
        mesh = pkgs['Mesh'].names[struct.unpack_from('<i', merged['Mesh'][1])[0]].replace('/Game/', '').split('.')[0] if 'Mesh' in merged else None
        pats = val('Patterns') or []
        pat = pats[pattern] if pattern < len(pats) else (pats[0] if pats else None)
        item_tables = list(merged['ColorTables'][1][4:]) if 'ColorTables' in merged else list(DEFAULT_TABLES)
        dye = []
        for i, c in enumerate(colours):
            t = item_tables[i] if i < len(item_tables) else item_tables[-1]
            rgb = tables[t][1][c] if c < len(tables[t][1]) else None
            # mask channel per colour: colour 1 = red, colour 2 = blue (checked against items that use only 1 and 2:
            # their masks have red and blue and no green); green is the metal and takes the profile's metal tint
            dye.append({'slot': i + 1, 'channel': (0, 2, 1)[i] if i < 3 else None, 'table': tables[t][0], 'index': c, 'linear_rgb': rgb,
                        'used': bool(pat and pat.get(f'bHasColor{i + 1}')) if pat else False})
        emblem = None
        if 'bHasEmblem' in merged and merged['bHasEmblem'][2]:
            sc = val('EmblemScale') if 'EmblemScale' in merged else [1.0, 1.0, 1.0]
            of = val('EmblemOffset') if 'EmblemOffset' in merged else [0.0, 0.0, 0.0]
            # where the emblem sits in the item's own texture space: a square `scale` wide centred at 0.5 - offset x scale,
            # with the whole texture space turned about its middle by the item's rotation. Checked on both of coolhook's
            # emblem items: the cuirass (no rotation) lands on the outer cloth at the middle of the chest, the skirt
            # (half a turn) on the front panel, as in MORDHAU's own mercenary screen.
            import math
            turns = val('EmblemRotation') if 'EmblemRotation' in merged else 0.0
            cx, cy = -of[0] * sc[0], -of[1] * (sc[1] if sc[1] else sc[0])
            ca, sa = math.cos(turns * 2 * math.pi), math.sin(turns * 2 * math.pi)
            emblem = {'centre': [0.5 + cx * ca - cy * sa, 0.5 + cx * sa + cy * ca], 'scale': [sc[0], sc[1] if sc[1] else sc[0]], 'rotation_turns': turns}
        hide = sorted(k for k in merged if k.startswith('bHide') and k != 'bHideIn1P' and merged[k][0] == 'BoolProperty' and merged[k][2])
        pieces.append({'slot': slot, 'item': bp, 'item_number': item_id, 'model': mesh,
                       'colour_map': _tex_path(pkgs['AlbedoMap'], val('AlbedoMap')) if 'AlbedoMap' in merged else None,
                       'normal_map': _tex_path(pkgs['NormalMap'], val('NormalMap')) if 'NormalMap' in merged else None,
                       # MORDHAU's "RMA" map: red roughness, green metal, blue ambient occlusion
                       'surface_map': _tex_path(pkgs['RoughnessMap'], val('RoughnessMap')) if 'RoughnessMap' in merged else None,
                       'pattern': pattern, 'pattern_mask': _tex_path(pkgs['Patterns'], pat['Texture']) if pat and isinstance(pat.get('Texture'), str) else None,
                       'tables_from_item': 'ColorTables' in merged, 'dye': dye, 'hide': hide, 'emblem': emblem})
    metal = single_table('MetalTintsColorTable')
    if me is None:
        for piece in pieces:
            piece['metal_tint'] = {'channel': 1, 'table': metal[0], 'index': 0, 'linear_rgb': metal[1][0] if metal[1] else None}
            piece['emblem'] = None
        return {'name': name, 'pieces': pieces, 'appearance': {}, 'weapons': [], 'held_weapon': []}
    app0 = dict(re.findall(r'(\w+)=([\w.]+)', re.search(r'AppearanceCustomization=\((.*?)\),FaceCustomization', me[2]).group(1)))
    tint = int(app0.get('MetalTint', 0))
    metal_rgb = metal[1][tint] if tint < len(metal[1]) else None
    for piece in pieces:
        piece['metal_tint'] = {'channel': 1, 'table': metal[0], 'index': tint, 'linear_rgb': metal_rgb}
    app = dict(re.findall(r'(\w+)=([\w.]+)', re.search(r'AppearanceCustomization=\((.*?)\),FaceCustomization', me[2]).group(1)))
    eq = re.search(r'Equipment=\((.*?)\)\),AppearanceCustomization', me[2])
    pf.top_lists()
    weapons = [pf._cache['equipment'][int(i)] for i in re.findall(r'\(ID=(\d+)', eq.group(1))] if eq else []
    # the profile's emblem: texture from MORDHAU's emblem list, its two colours from the emblem colour table
    emblem = None
    try:
        single = uasset.Package('Mordhau/Blueprints/BP_MordhauSingleton')
        sd = single.defaults(next(e for e in single.exports if e['name'].startswith('Default__'))['name'])
        number = int(re.search(r'Emblem=(\d+)', me[2]).group(1))
        picked = [int(x) for x in re.search(r'EmblemColors=\(([\d,]*)\)', me[2]).group(1).split(',') if x]
        ebp = sd['Emblems'][number][:-2]
        eq_pkg = uasset.Package(pf._find(ebp))
        tex = eq_pkg.imports_of('Texture2D')[0][1].replace('/Game/', '')
        table = single_table('EmblemColorTable')[1]
        emblem = {'number': number, 'item': ebp, 'texture': tex, 'colours': [{'index': c, 'linear_rgb': table[c] if c < len(table) else None} for c in picked]}
    except Exception as ex:
        emblem = {'error': str(ex)}
    for piece in pieces:
        if piece.get('emblem') and emblem and 'texture' in emblem:
            piece['emblem'].update({'texture': emblem['texture'], 'colours': emblem['colours']})
        elif piece.get('emblem'):
            piece['emblem'] = None
    held = []
    if eq:
        first = re.match(r'\(ID=(\d+)(?:,Colors=\(([\d,]*)\))?(?:,Parts=\(([\d,]*)\))?', eq.group(1))
        if first and weapons:
            nums = lambda g: [int(x) for x in g.split(',') if x] if g else []
            held = weapon_pieces(weapons[0], nums(first.group(3)), nums(first.group(2)))
    return {'name': me[0], 'pieces': pieces, 'appearance': app, 'weapons': weapons, 'held_weapon': held}


if __name__ == '__main__':
    s = spec(sys.argv[1])
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'cache', s['name'].lower() + '.json')
    open(out, 'w', encoding='utf-8').write(json.dumps(s, indent=1))
    for p in s['pieces']:
        dye = ', '.join(f"{d['slot']}:{d['table']} {d['index']} {d['linear_rgb']}{'' if d['used'] else ' (unused by pattern)'}" for d in p['dye'])
        print(f"{p['slot']:11s} {p['item']:30s} mask {str(p['pattern_mask']).split('/')[-1]:34s} hide {','.join(h[5:] for h in p['hide']) or '-'}")
        print(f"{'':11s} dye {dye or '-'}{'' if p['tables_from_item'] else '   [tables: default]'}")
    print('weapons', s['weapons'], '| appearance', s['appearance'])
    for w in s['held_weapon']:
        print('held', w['part'], w['model'].split('/')[-1], str(w['colour_map']).split('/')[-1], str(w['pattern_mask']).split('/')[-1], [(t['table'], t['index']) for t in w['dye']])
