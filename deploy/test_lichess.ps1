# Offline end-to-end check of the Lichess deployment: lichess-bot's mocked
# lichess.org server plays a full game against Tempo. No token needed.
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$bot = Join-Path $here 'lichess-bot'
$venv = Join-Path $here 'venv'

if (-not (Test-Path $bot)) { git clone --depth 1 https://github.com/lichess-bot-devs/lichess-bot.git $bot }
if (-not (Test-Path $venv)) {
    python -m venv $venv
    & "$venv\Scripts\python.exe" -m pip install -q -r "$bot\requirements.txt" -r "$bot\test_bot\test-requirements.txt"
}
Copy-Item (Join-Path $here 'test_tempo_lichess.py') (Join-Path $bot 'test_bot\') -Force
Push-Location $bot
try { & "$venv\Scripts\python.exe" -m pytest test_bot/test_tempo_lichess.py -s -q -p no:cacheprovider } finally { Pop-Location }
