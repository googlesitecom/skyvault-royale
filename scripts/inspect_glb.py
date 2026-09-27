#!/usr/bin/env python3
"""Inspecciona materiales/prims de Jugador.glb para diagnosticar el negro."""
import json, struct, sys

path = "/home/z/my-project/skyvault/assets/models/Jugador.glb"
with open(path, "rb") as f:
    data = f.read()

magic, version, length = struct.unpack_from("<4sII", data, 0)
assert magic == b"glTF", magic
json_len = struct.unpack_from("<I", data, 12)[0]
json_type = struct.unpack_from("<I", data, 16)[0]
J = json.loads(data[20:20 + json_len])

print("== meshes ==")
for mi, m in enumerate(J.get("meshes", [])):
    print(f"mesh {mi}: name={m.get('name','?')}")
    for pi, p in enumerate(m.get("primitives", [])):
        print(f"  prim {pi}: material={p.get('material')}")

print("\n== materials ==")
for i, mat in enumerate(J.get("materials", [])):
    pbr = mat.get("pbrMetallicRoughness", {})
    bcf = pbr.get("baseColorFactor", [1,1,1,1])
    print(f"material {i}: name={mat.get('name','?')}")
    print(f"  baseColorFactor={bcf}")
    print(f"  metallicFactor={pbr.get('metallicFactor','(def 1)')} roughnessFactor={pbr.get('roughnessFactor','(def 1)')}")
    print(f"  baseColorTexture={pbr.get('baseColorTexture')}")
    print(f"  metallicRoughnessTexture={pbr.get('metallicRoughnessTexture')}")
    print(f"  normalTexture={mat.get('normalTexture')}")
    print(f"  emissiveFactor={mat.get('emissiveFactor')}")

print("\n== images ==")
for i, img in enumerate(J.get("images", [])):
    print(f"image {i}: {img.get('mimeType','?')} bufferView={img.get('bufferView')} name={img.get('name','?')}")

print("\n== textures ==")
for i, t in enumerate(J.get("textures", [])):
    print(f"texture {i}: source={t.get('source')} sampler={t.get('sampler')}")

print("\n== bufferViews (images) ==")
for i, bv in enumerate(J.get("bufferViews", [])):
    print(f"bv {i}: off={bv.get('byteOffset',0)} len={bv.get('byteLength')}")
