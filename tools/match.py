"""Match runner around fastchess.

  gauntlet: Tempo vs Stockfish at fixed UCI_Elo levels, one match per level.
  sprt:     new build vs old build, sequential test that stops once it can
            accept or reject "the change gains at least ELO1".

Both write PGNs and fastchess logs to matches/ and print a summary line per
match. Expects tools/fastchess(.exe), tools/stockfish(.exe) and
tools/8moves_v3.pgn (see docs/TESTING.md).

  python tools/match.py gauntlet --levels 1800 2200 --games 200
  python tools/match.py sprt --new build/tempo.exe --old temp/base.exe
"""

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EXE = ".exe" if sys.platform == "win32" else ""
FASTCHESS = ROOT / "tools" / f"fastchess{EXE}"
STOCKFISH = ROOT / "tools" / f"stockfish{EXE}"
BOOK = ROOT / "tools" / "8moves_v3.pgn"
MATCHES = ROOT / "matches"


def run(args, tag):
    MATCHES.mkdir(exist_ok=True)
    log = MATCHES / f"{tag}.log"
    cmd = [str(FASTCHESS), *args,
           "-openings", f"file={BOOK}", "format=pgn", "order=random",
           "-pgnout", f"file={MATCHES / (tag + '.pgn')}",
           "-recover"]
    with open(log, "w") as f:
        subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, cwd=MATCHES)
    text = log.read_text()
    # fastchess prints a running summary; the last one is the final result.
    blocks = text.split("Results of")
    final = blocks[-1] if len(blocks) > 1 else text
    stale = MATCHES / "config.json"
    if stale.exists():
        stale.unlink()
    return final


def field(pattern, text, default="?"):
    m = re.search(pattern, text)
    return m.group(1) if m else default


def gauntlet(a):
    engine = Path(a.engine).resolve()
    print(f"{'opponent':<14}{'games':>6}{'W':>5}{'D':>5}{'L':>5}{'score':>8}  Elo diff")
    for level in a.levels:
        tag = f"gauntlet_{a.tag}_sf{level}"
        out = run(["-engine", f"cmd={engine}", "name=tempo",
                   "-engine", f"cmd={STOCKFISH}", f"name=sf{level}",
                   "option.UCI_LimitStrength=true", f"option.UCI_Elo={level}",
                   "-each", f"tc={a.tc}", "option.Hash=16",
                   "-rounds", str(a.games // 2), "-games", "2", "-repeat",
                   "-concurrency", str(a.concurrency)], tag)
        w, l, d = field(r"Wins: (\d+)", out), field(r"Losses: (\d+)", out), field(r"Draws: (\d+)", out)
        g, pts = field(r"Games: (\d+)", out), field(r"Points: [\d.]+ \(([\d.]+) %\)", out)
        elo = field(r"Elo: (-?[\d.]+ \+/- [\d.na]+)", out)
        print(f"{'SF ' + str(level):<14}{g:>6}{w:>5}{d:>5}{l:>5}{pts + '%':>8}  {elo}", flush=True)


def sprt(a):
    tag = f"sprt_{a.tag}"
    out = run(["-engine", f"cmd={Path(a.new).resolve()}", "name=new",
               "-engine", f"cmd={Path(a.old).resolve()}", "name=old",
               "-each", f"tc={a.tc}", "option.Hash=16",
               "-rounds", str(a.max_games // 2), "-games", "2", "-repeat",
               "-concurrency", str(a.concurrency),
               "-sprt", f"elo0={a.elo0}", f"elo1={a.elo1}", "alpha=0.05", "beta=0.05", "model=normalized"], tag)
    print(out.strip())


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    cpus = max(1, (os.cpu_count() or 2) - 4)

    g = sub.add_parser("gauntlet")
    g.add_argument("--engine", default=str(ROOT / "build" / f"tempo{EXE}"))
    g.add_argument("--levels", type=int, nargs="+", default=[1800, 2200, 2600])
    g.add_argument("--games", type=int, default=200)
    g.add_argument("--tc", default="10+0.1")
    g.add_argument("--concurrency", type=int, default=cpus)
    g.add_argument("--tag", default="current")

    s = sub.add_parser("sprt")
    s.add_argument("--new", required=True)
    s.add_argument("--old", required=True)
    s.add_argument("--tc", default="8+0.08")
    s.add_argument("--elo0", type=float, default=0)
    s.add_argument("--elo1", type=float, default=5)
    s.add_argument("--max-games", type=int, default=20000)
    s.add_argument("--concurrency", type=int, default=cpus)
    s.add_argument("--tag", default="test")

    a = ap.parse_args()
    gauntlet(a) if a.cmd == "gauntlet" else sprt(a)


if __name__ == "__main__":
    main()
