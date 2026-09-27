#!/usr/bin/env python3
"""Inspecciona bounds RAW de POSITION por primitiva (sin transform de nodos)
y los transform de nodos sospechosos (grandes)."""
import json, struct, os

def inspect(path):
    data = open(path, 'rb').read()
    magic, version, length = struct.unpack('<III', data[:12])
    off, g = 12, None
    while off < length:
        clen, ctype = struct.unpack('<II', data[off:off+8])
        chunk = data[off+8:off+8+clen]
        if ctype == 0x4E4F534A: g = json.loads(chunk)
        off += 8 + clen
    print(f"\n=== {os.path.basename(path)} ===")
    accs = g['accessors']
    # nodes con transform grande
    for i, n in enumerate(g.get('nodes', [])):
        t = n.get('translation', [0,0,0])
        s = n.get('scale', [1,1,1])
        if any(abs(x) > 500 for x in t) or any(x < 0.01 or x > 50 for x in s):
            print(f"  node[{i}] '{n.get('name','?')}': t={t} s={s} mesh={n.get('mesh')}")
    # root node transform
    scene = g.get('scene', 0)
    sc = g['scenes'][scene]
    for ri in sc.get('nodes', []):
        n = g['nodes'][ri]
        print(f"  root node[{ri}] '{n.get('name','?')}': t={n.get('translation')} r={n.get('rotation')} s={n.get('scale')}")
    # bounds raw por prim
    for mi, m in enumerate(g.get('meshes', [])):
        for pi, p in enumerate(m.get('primitives', [])):
            a = accs[p['attributes']['POSITION']]
            mn, mx = a.get('min'), a.get('max')
            print(f"  mesh[{mi}].prim[{pi}]: RAW bounds min={mn} max={mx}")

for f in ['jonesy.glb', 'scar_-_fortnite_gun.glb', 'fortnite_pickaxe.glb',
          'fortnites_pump_shotgun.glb', 'heavy_sniper_rifle_fortnite_item.glb',
          'twin_mag_smg_-_fortnite_pbr.glb']:
    inspect(f'/home/z/my-project/skyvault/assets/models/{f}')
