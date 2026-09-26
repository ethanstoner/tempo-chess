#!/usr/bin/env bash
# Runs Tempo on Lichess through lichess-bot (Linux / macOS / a VPS).
#   export LICHESS_BOT_TOKEN=lip_...   # token with the bot:play scope
#   ./deploy/run.sh -u                 # once: upgrade to a BOT account (irreversible)
#   ./deploy/run.sh                    # play
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
bot="$here/lichess-bot"
venv="$here/venv"

[[ -x "$here/../build/tempo" ]] || { echo "Build the engine first: cmake -S . -B build && cmake --build build"; exit 1; }
[[ -n "${LICHESS_BOT_TOKEN:-}" ]] || { echo "Set LICHESS_BOT_TOKEN to a token with the bot:play scope"; exit 1; }
[[ -d "$bot" ]] || git clone --depth 1 https://github.com/lichess-bot-devs/lichess-bot.git "$bot"
if [[ ! -d "$venv" ]]; then
    python3 -m venv "$venv"
    "$venv/bin/pip" install -q -r "$bot/requirements.txt"
fi

sed 's/name: "tempo.exe".*/name: "tempo"/' "$here/config.yml" > "$bot/config.linux.yml"
cd "$bot"
exec "$venv/bin/python" lichess-bot.py --config config.linux.yml "$@"
