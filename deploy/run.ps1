# Runs Tempo on Lichess through lichess-bot.
#   $env:LICHESS_BOT_TOKEN = "lip_..."   # token with the bot:play scope
#   .\deploy\run.ps1 -Upgrade            # once: turns the account into a BOT account (irreversible)
#   .\deploy\run.ps1                     # play
param([switch]$Upgrade)
$ErrorActionPreference = 'Stop'

$here = $PSScriptRoot
$bot = Join-Path $here 'lichess-bot'
$venv = Join-Path $here 'venv'
$engine = Join-Path $here '..\build\tempo.exe'

if (-not (Test-Path $engine)) { throw "Build the engine first: cmake -S . -B build -G Ninja; cmake --build build" }
if (-not $env:LICHESS_BOT_TOKEN) { throw "Set `$env:LICHESS_BOT_TOKEN to a Lichess token with the bot:play scope" }
if (-not (Test-Path $bot)) { git clone --depth 1 https://github.com/lichess-bot-devs/lichess-bot.git $bot }
if (-not (Test-Path $venv)) {
    python -m venv $venv
    & "$venv\Scripts\python.exe" -m pip install -q -r "$bot\requirements.txt"
}

$botArgs = @('lichess-bot.py', '--config', (Join-Path $here 'config.yml'))
if ($Upgrade) { $botArgs += '-u' }
Push-Location $bot
try { & "$venv\Scripts\python.exe" @botArgs } finally { Pop-Location }
