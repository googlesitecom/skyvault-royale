# SKYVAULT Royale — ¡PUBLICADO! 🎉

## 🔗 Links finales

| Qué | URL |
|---|---|
| **▶ JUGAR (GitHub Pages)** | **https://googlesitecom.github.io/skyvault-royale/** |
| Repositorio | https://github.com/googlesitecom/skyvault-royale |
| Workflows (CI + deploy) | https://github.com/googlesitecom/skyvault-royale/actions |

**Estado: publicado y verificado jugando en el navegador** (menú → caída desde
el Carguero Nube → aterrizaje → partida en curso con HUD, bots y tormenta).
CI en verde: build nativo + 189 tests del motor. Deploy web automático en
cada push a `main` (Emscripten → WASM → Pages).

## Copias locales de respaldo

| Archivo | Qué es |
|---|---|
| `skyvault-royale-github.zip` | El repo completo tal como está en GitHub (9 MB, con git). |
| `skyvault-web-preview.zip` | Solo la build web (112 KB). Jugable offline con `python3 -m http.server`. |
| `github-pages-en-vivo.png` | Menú del juego servido por GitHub Pages (verificado). |
| `github-pages-partida.png` | Caída con planeador sobre la isla (verificado en vivo). |
| `github-pages-jugando.png` | Partida en curso en el sitio publicado (verificado en vivo). |
| `preview-*.png` | Capturas de la sesión de pruebas previa. |

## ⚠️ SEGURIDAD — haz esto ya

El token pegado en el chat quedó **expuesto** y tiene permisos de
administrador total (`repo`, `workflow`, `delete_repo`, `admin:org`...).
Revócalo AHORA en:

**github.com → Settings → Developer settings → Personal access tokens → Delete**

Todo lo necesario ya está subido; no hace falta ese token para nada más.

## Nota sobre los modelos de Fortnite

Los .glb del repo `googlesitecom/Fortnite` (Jonesy, SCAR, etc.) son remakes
de IP de Epic Games: aunque «no tengan copyright» del autor que los subió,
son obras derivadas y un repo público con ellos puede recibir DMCA. Por eso
el juego publicado usa **personajes y armas 100% procedurales** generados
por código (`game/ProceduralModels.cpp`) — nadie puede tumbar el repo.

## Si quieres seguir desarrollando

```bash
git clone https://github.com/googlesitecom/skyvault-royale.git
cd skyvault-royale
cmake -B build && cmake --build build -j && ./build/skyvault   # escritorio
# o para web:
emcmake cmake -B build-web -DSKYVAULT_WEB=ON && cmake --build build-web -j
```

Cada `git push` a `main` recompila y republica el juego automáticamente.
