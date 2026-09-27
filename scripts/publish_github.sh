#!/usr/bin/env bash
# ============================================================================
# publish_github.sh — Publica SKYVAULT Royale en GitHub + activa Pages
#
# Uso:   ./publish_github.sh <TOKEN> [NOMBRE_REPO]
#
#   TOKEN        Personal Access Token. Recomendado CLASSIC con scopes:
#                repo + workflow   (los workflows del repo van en el push)
#                (fine-grained vale con Contents/Workflows/Pages/Actions: write)
#   NOMBRE_REPO  opcional, por defecto "skyvault-royale"
#
# Pasos que automatiza:
#   1. Identifica la cuenta          (GET  /user)
#   2. Crea el repo publico          (POST /user/repos)
#   3. Pushea main (firma el commit con el login real en el primer push)
#   4. Activa Pages via Actions      (POST /repos/:owner/:repo/pages)
#   5. Dispara el workflow pages.yml (POST .../actions/workflows/pages.yml/dispatches)
#   6. Sondea el sitio hasta HTTP 200 y devuelve el link final
#
# IMPORTANTE: tras publicar, revoca/borra el token si se pegó en un chat.
# ============================================================================
set -euo pipefail

TOKEN="${1:-}"
REPO="${2:-skyvault-royale}"
SRC="/home/z/my-project/skyvault"
API="https://api.github.com"

[ -n "$TOKEN" ] || { echo "ERROR: falta el token. Uso: $0 <token> [nombre_repo]"; exit 1; }
cd "$SRC"

H1="Authorization: Bearer $TOKEN"
H2="Accept: application/vnd.github+json"
H3="X-GitHub-Api-Version: 2022-11-28"
say(){ echo -e "\033[1;36m>>\033[0m $*"; }

# ---------------------------------------------------------------- 1) cuenta
OWNER=$(curl -sf --max-time 30 -H "$H1" -H "$H2" -H "$H3" "$API/user" \
        | python3 -c 'import sys,json;print(json.load(sys.stdin)["login"])') \
  || { echo "ERROR: token invalido o sin conexion"; exit 1; }
say "Cuenta detectada: $OWNER  |  repo: $REPO"

# ---------------------------------------------------------------- 2) crear repo
say "Creando repo publico $REPO..."
curl -sf --max-time 30 -X POST -H "$H1" -H "$H2" -H "$H3" "$API/user/repos" \
  -d "{\"name\":\"$REPO\",\"description\":\"SKYVAULT Royale - battle royale 3D con motor 100% propio (C++20 / OpenGL 4.6 / WebAssembly)\",\"private\":false,\"has_wiki\":false,\"has_projects\":false}" \
  > /dev/null && say "Repo creado" \
  || say "El repo ya existe o se reutiliza (continuando)"

# ---------------------------------------------------------------- 3) push
say "Pusheando main..."
git remote remove origin 2>/dev/null || true
git remote add origin "https://x-access-token:${TOKEN}@github.com/${OWNER}/${REPO}.git"

if ! git ls-remote --exit-code --heads origin main > /dev/null 2>&1; then
  # Primer push: firma el commit con la cuenta real de GitHub
  git config user.name  "$OWNER"
  git config user.email "$OWNER@users.noreply.github.com"
  git commit --amend --reset-author --no-edit --quiet
fi
git push -u origin main
git remote set-url origin "https://github.com/${OWNER}/${REPO}.git"   # no dejar el token en .git/config
say "Push OK -> https://github.com/$OWNER/$REPO"

# ---------------------------------------------------------------- 4) activar Pages
say "Activando GitHub Pages (build via Actions)..."
curl -sf --max-time 30 -X POST -H "$H1" -H "$H2" -H "$H3" "$API/repos/$OWNER/$REPO/pages" \
     -d '{"build_type":"workflow"}' > /dev/null \
  || curl -sf --max-time 30 -X PUT  -H "$H1" -H "$H2" -H "$H3" "$API/repos/$OWNER/$REPO/pages" \
       -d '{"build_type":"workflow"}' > /dev/null \
  || say "No se pudo via API: activa a mano Settings > Pages > Source: GitHub Actions"

# ---------------------------------------------------------------- 5) lanzar deploy
say "Disparando el workflow de deploy (pages.yml)..."
sleep 2
curl -sf --max-time 30 -X POST -H "$H1" -H "$H2" -H "$H3" \
     "$API/repos/$OWNER/$REPO/actions/workflows/pages.yml/dispatches" \
     -d '{"ref":"main"}' > /dev/null \
  || say "No se pudo disparar: correlo en Actions > 'Deploy a GitHub Pages' > Run workflow"

# ---------------------------------------------------------------- 6) esperar
URL="https://${OWNER}.github.io/${REPO}/"
say "Esperando el despliegue (1-3 min aprox)..."
for i in $(seq 1 40); do
  sleep 10
  CODE=$(curl -s -o /dev/null -w "%{http_code}" --max-time 15 "$URL" || true)
  if [ "$CODE" = "200" ]; then
    say "DESPLIEGUE LISTO: $URL"
    say "Repo:  https://github.com/$OWNER/$REPO"
    say "Juego: $URL"
    say "Recuerda revocar el token si estuvo expuesto."
    exit 0
  fi
  printf '  ... (%s/40, HTTP %s)\n' "$i" "${CODE:-sin-red}"
done
say "Aun sin respuesta tras ~6 min. Revisa la pestana Actions del repo;"
say "cuando el workflow termine el juego estara en: $URL"
