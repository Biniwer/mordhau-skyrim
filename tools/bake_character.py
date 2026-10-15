"""Dev tool: pack a MORDHAU character (base body + wearables) and full-skeleton clips for the plugin's renderer.

Everything is read from the player's own MORDHAU install and written to the local cache; nothing here is shipped and no
texture is authored: colour maps are copied as stored (block-compressed) into .dds containers. The plugin's own reader
replaces this tool later.

  bake_character.py char <out dir> <wearable definition>...   -> character.dmchar + <texture>.dds
  bake_character.py spec <out dir> <spec.json from tools/character_spec.py>   -> the player's own character, dyed
  bake_character.py clip <out file> <clip under Mordhau/Content/Mordhau/>   -> full-skeleton clip (.dmfull)

character.dmchar (little endian): 'DMCH', int32 version 6, int32 bones, int32 vertices, int32 indices, int32 sections;
bones: char[64] name, int32 parent, float[4] rest rotation (x,y,z,w), float[3] rest translation;
sections: int32 first index, int32 index count, char[64] colour map file, char[64] pattern mask file ("" = none),
float[9] tint for the mask's red, green and blue regions (1,1,1 = leave as is), char[64] emblem file ("" = none),
float[2] emblem centre and float[2] emblem size in the piece's texture space, float emblem rotation (turns),
float[6] the emblem's two colours, char[64] normal map file ("" = none; two-channel, as MORDHAU stores it), char[64] surface map file ("" = none:
MORDHAU's own roughness / metal / ambient-occlusion map in red / green / blue), char[64] hair layer and char[64] beard
layer (MORDHAU paints hair and beards onto the head: white = hair), float[4] skin: kind (0 clothing, 1 body skin, 2 head,
3 eyes), hair allowed, beard allowed (0 when an item hides them), unused. Skin tone, hair colour and whether there is
hair or a beard are given by the plugin per person (from Skyrim). The plugin applies the dye in its shader
from these; no dyed texture is ever written.
vertices: float[3] position, float[3] normal, float[2] uv, uint8[4] bone ids, uint8[4] weights (sum 255);
indices: uint32. Positions are MORDHAU's (centimetres, left-handed, facing +Y); the plugin converts.
The held weapon's vertices are in the weapon's own space and carry bone id = number of bones: the plugin gives that
slot the weapon's transform (from MORDHAU's stream, or the hand's weapon bone).
.dmfull: 'DMF1', float length, int32 frames, int32 bones, then frames x bones x (float[4] rotation, float[3] translation), local.
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import animdecode, skelmesh, texture
from render_outfit import BODY, BODY_ALBEDO, wearable
from render_pose import key
from render_character import hidden_bone_ids, visible_triangles, gamma

HAIR_LAYER = 'UMA/Separated/MaleCaucasian01/T_MaleCaucasian01Hair01_Albedo'
BEARD_LAYER = 'UMA/Separated/MaleCaucasian01/T_MaleCaucasian01Beard04_Albedo'
FOURCC = {'PF_DXT1': b'DXT1', 'PF_DXT5': b'DXT5', 'PF_BC5': b'ATI2'}


CAP = 4096   # largest texture side written (the crowd's looks are baked smaller than the player's)


def write_dds(tex_path, out_dir, max_size=4096):
    max_size = min(max_size, CAP)
    t = texture.info(tex_path)
    fourcc = FOURCC[t['format']]
    mips = [m for m in t['mips'] if m[2] and max(m[0], m[1]) <= max_size]
    w, h, top = mips[0]
    header = struct.pack('<4sIIIIIII', b'DDS ', 124, 0xA1007, h, w, len(top), 0, len(mips)) + bytes(44)
    header += struct.pack('<II4sIIIII', 32, 0x4, fourcc, 0, 0, 0, 0, 0)
    header += struct.pack('<IIIII', 0x401008, 0, 0, 0, 0)
    assert len(header) == 128, len(header)
    # (two different textures can share a name, e.g. Fur_Albedo for a mantle and for boots: the path decides)
    import zlib
    name = (t['name'][:48] + '_' + format(zlib.crc32(tex_path.encode()) & 0xFFFFFF, '06x')) + '.dds'
    with open(os.path.join(out_dir, name), 'wb') as f:
        f.write(header)
        for mw, mh, data in mips:
            f.write(data)
    return name, (w, h, t['format'], len(mips))


def bake_char(out_dir, wearables, spec=None):
    os.makedirs(out_dir, exist_ok=True)
    # piece: (model, colour map per material or None, default colour map, mask, tints[3] or None, body-part names to hide)
    emblems = {}  # model -> emblem description
    normals = {}  # model -> normal map
    surfaces = {}  # model -> roughness/metal/occlusion map
    pieces = [(BODY, BODY_ALBEDO, None, None, None, sorted({h[5:] for p in spec['pieces'] for h in p['hide']}) if spec else [])]
    if spec and spec.get('no_body'):
        pieces = []   # a creature: one model of its own, no human body under it
    for bp in wearables:
        mesh_path, albedo = wearable(bp)
        pieces.append((mesh_path, None, albedo, None, None, []))
    for p in (spec['pieces'] if spec else []):
        if not p['model'] or not p['colour_map']:
            continue
        tints = [[1.0, 1.0, 1.0] for _ in range(3)]
        for d in [d for d in p['dye'] if d['used']] + ([p['metal_tint']] if p.get('metal_tint') else []):
            if d['linear_rgb'] and d['channel'] is not None:
                tints[d['channel']] = [float(x) for x in gamma(d['linear_rgb'])]
        pieces.append((p['model'], None, p['colour_map'], p['pattern_mask'], tints, []))
        if p.get('emblem'):
            emblems[p['model']] = p['emblem']
        if p.get('normal_map'):
            normals[p['model']] = p['normal_map']
        if p.get('surface_map'):
            surfaces[p['model']] = p['surface_map']
    weapon_models = set()
    for p in (spec.get('held_weapon', []) if spec else []):
        if not p['model'] or not p['colour_map']:
            continue
        tints = [[1.0, 1.0, 1.0] for _ in range(3)]
        for d in p['dye']:
            if d['linear_rgb'] and d['channel'] is not None:
                tints[d['channel']] = [float(x) for x in gamma(d['linear_rgb'])]
        pieces.append((p['model'], None, p['colour_map'], p['pattern_mask'], tints, []))
        weapon_models.add(p['model'])
        if p.get('normal_map'):
            normals[p['model']] = p['normal_map']
        if p.get('surface_map'):
            surfaces[p['model']] = p['surface_map']
    bones = pose = None
    verts, idx, sections, textures = [], [], [], {}
    base_vertex = 0
    for mesh_path, by_material, default_albedo, mask_path, tints, hide in pieces:
        try:
            m = skelmesh.load(mesh_path)
        except Exception as ex:
            if bones is None:
                raise
            print(f'  LEFT OUT {mesh_path.split("/")[-1]}: its model is stored in a way the reader does not handle yet ({type(ex).__name__})')
            continue
        if bones is None:
            bones, pose = m['bones'], m['pose']
        is_weapon = mesh_path in weapon_models
        remap = None
        if not is_weapon and [b[0] for b in m['bones']] != [b[0] for b in bones]:
            # this piece was built on a skeleton with a different bone list: match its bones to the body's by name
            # (a bone the body lacks falls back to its nearest ancestor that the body has)
            names = [b[0] for b in bones]
            remap = []
            for name, parent in m['bones']:
                while name not in names and parent >= 0:
                    name, parent = m['bones'][parent]
                remap.append(names.index(name) if name in names else 0)
            remap = np.array(remap)
            print(f'  {m["name"]}: bones matched by name ({len(m["bones"])} -> {len(bones)})')
        w8 = np.rint(m['weights'] * 255).astype(np.int64)
        w8[:, 0] += 255 - w8.sum(axis=1)                 # keep the sum at exactly 255
        nv = len(m['positions'])
        block = np.zeros(nv, dtype=[('p', '<f4', 3), ('n', '<f4', 3), ('uv', '<f4', 2), ('b', 'u1', 4), ('w', 'u1', 4)])
        block['p'], block['n'], block['uv'] = m['positions'], m['normals'], m['uvs']
        block['b'], block['w'] = (remap[m['bone_ids']] if remap is not None else m['bone_ids']), np.clip(w8, 0, 255)
        if is_weapon:
            block['b'] = 0; block['w'] = 0
            block['b'][:, 0] = len(bones); block['w'][:, 0] = 255
        verts.append(block)
        for s in m['sections']:
            mat = m['materials'][s['material']][0] if s['material'] < len(m['materials']) else None
            tex_path = by_material.get(mat, default_albedo) if by_material is not None else default_albedo
            skin = [0.0, 0.0, 0.0, 0.0]
            hair_file = beard_file = ''
            if mesh_path == BODY:
                if mat == 'INST_Torso':
                    skin[0] = 1.0
                elif mat and mat.startswith('INST_Head'):
                    skin = [2.0, 0.0 if 'Hair' in hide else 1.0, 0.0 if 'Beard' in hide else 1.0, 0.0]
                    for which, layer in (('hair', HAIR_LAYER), ('beard', BEARD_LAYER)):
                        if layer not in textures:
                            textures[layer] = write_dds(layer, out_dir, 1024)
                    hair_file, beard_file = textures[HAIR_LAYER][0], textures[BEARD_LAYER][0]
                elif mat == 'INST_Eye':
                    skin[0] = 3.0
                    tex_path = BODY_ALBEDO['INST_HeadMaleCaucasian01']   # (not sampled: eyes are drawn from their own texture coordinates)
            if not tex_path:
                print(f'  {m["name"]}: section with material {mat} left out (no colour map chosen for it yet)')
                continue
            if tex_path not in textures:
                textures[tex_path] = write_dds(tex_path, out_dir)
                print(f'  texture {textures[tex_path][0]} {textures[tex_path][1]}')
            if mask_path and mask_path not in textures:
                textures[mask_path] = write_dds(mask_path, out_dir)
                print(f'  mask {textures[mask_path][0]} {textures[mask_path][1]}')
            first = s['base_index'] // 3
            tri = m['indices'][first:first + s['triangles']]
            if hide:
                tri = visible_triangles(m, tri, hidden_bone_ids(m, hide))
            tri = tri + base_vertex
            em = emblems.get(mesh_path)
            em_file, em_vals = '', [0.0] * 11
            if em and em.get('texture'):
                if em['texture'] not in textures:
                    textures[em['texture']] = write_dds(em['texture'], out_dir)
                    print(f'  emblem {textures[em["texture"]][0]} {textures[em["texture"]][1]}')
                em_file = textures[em['texture']][0]
                cols = [gamma(c['linear_rgb']) if c.get('linear_rgb') else [1.0, 1.0, 1.0] for c in (em['colours'] + [{}, {}])[:2]]
                em_vals = [*em['centre'], *em['scale'], float(em.get('rotation_turns') or 0.0), *[float(x) for x in cols[0]], *[float(x) for x in cols[1]]]
            nm_file = ''
            nm = normals.get(mesh_path)
            if nm:
                try:
                    if nm not in textures:
                        textures[nm] = write_dds(nm, out_dir, 2048)
                        print(f'  normal map {textures[nm][0]} {textures[nm][1]}')
                    nm_file = textures[nm][0]
                except Exception as ex:
                    print(f'  normal map {nm} left out: {ex}')
            sf_file = ''
            sf = surfaces.get(mesh_path)
            if sf:
                try:
                    if sf not in textures:
                        textures[sf] = write_dds(sf, out_dir, 2048)
                        print(f'  surface map {textures[sf][0]} {textures[sf][1]}')
                    sf_file = textures[sf][0]
                except Exception as ex:
                    print(f'  surface map {sf} left out: {ex}')
            sections.append((sum(len(i) for i in idx) * 3, len(tri) * 3, textures[tex_path][0], textures[mask_path][0] if mask_path else '',
                             [c for t in (tints or [[1.0] * 3] * 3) for c in t], em_file, em_vals, nm_file, sf_file, hair_file, beard_file, skin))
            idx.append(tri)
        base_vertex += nv
    verts = np.concatenate(verts); indices = np.concatenate(idx).astype('<u4').reshape(-1)
    with open(os.path.join(out_dir, 'character.dmchar'), 'wb') as f:
        f.write(struct.pack('<4siiiii', b'DMCH', 6, len(bones), len(verts), len(indices), len(sections)))
        for (name, parent), (q, t, s) in zip(bones, pose):
            f.write(struct.pack('<64si4f3f', name.encode(), parent, *q, *t))
        for first, count, tex, mask, tint, em_file, em_vals, nm_file, sf_file, hair_file, beard_file, skin in sections:
            f.write(struct.pack('<ii64s64s9f64s11f64s64s64s64s4f', first, count, tex.encode(), mask.encode(), *tint, em_file.encode(), *em_vals, nm_file.encode(),
                                sf_file.encode(), hair_file.encode(), beard_file.encode(), *skin))
        f.write(verts.tobytes()); f.write(indices.tobytes())
    print(f'character.dmchar: {len(bones)} bones, {len(verts)} vertices, {len(indices)//3} triangles, {len(sections)} sections')


def bake_clip(out, clip):
    m = skelmesh.load(BODY)
    pose = m['pose']; nb = len(pose)
    a = animdecode.decode(clip); frames = a['num_frames']
    rot = {t['bone_index']: t['rot'][2] for t in a['tracks'] if t['rot'] and t['bone_index'] < nb}
    tra = {t['bone_index']: t['trans'][2] for t in a['tracks'] if t['trans'] and t['bone_index'] < nb}
    data = np.zeros((frames, nb, 7), dtype='<f4')
    for f in range(frames):
        for i, (q, t, s) in enumerate(pose):
            data[f, i, :4] = key(rot[i], f, frames, True) if i in rot else q
            data[f, i, 4:] = key(tra[i], f, frames, False) if i in tra else t
    with open(out, 'wb') as fh:
        fh.write(struct.pack('<4sfii', b'DMF1', a['length'], frames, nb)); fh.write(data.tobytes())
    print(f'{os.path.basename(out)}: {frames} frames, {a["length"]:.3f}s, {nb} bones, {os.path.getsize(out)} bytes')


if __name__ == '__main__':
    if sys.argv[1] == 'char':
        bake_char(sys.argv[2], sys.argv[3:])
    elif sys.argv[1] == 'spec':
        import json
        bake_char(sys.argv[2], [], json.load(open(sys.argv[3], encoding='utf-8')))
    else:
        bake_clip(sys.argv[2], sys.argv[3])
