"""Maximum-likelihood rating on Stockfish's UCI_Elo scale from gauntlet logs.

Each game against a level L is scored with the logistic Elo model; the rating
maximising the likelihood over all levels is reported with a 95% interval
(likelihood drop of 1.92).

  python tools/fit_rating.py v02 v03
"""

import math
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def games(tag):
    out = []
    for log in sorted((ROOT / "matches").glob(f"gauntlet_{tag}_sf*.log")):
        level = int(re.search(r"_sf(\d+)\.log$", log.name).group(1))
        final = log.read_text().split("Results of")[-1]
        w, d, l = (int(re.search(k + r": (\d+)", final).group(1)) for k in ("Wins", "Draws", "Losses"))
        out.append((level, w + d / 2, w + d + l))
    return out


def loglik(r, data):
    total = 0.0
    for level, score, n in data:
        p = 1 / (1 + 10 ** ((level - r) / 400))
        total += score * math.log(p) + (n - score) * math.log(1 - p)
    return total


def main():
    for tag in sys.argv[1:]:
        data = games(tag)
        grid = range(1500, 4000)
        best = max(grid, key=lambda r: loglik(r, data))
        cut = loglik(best, data) - 1.92
        inside = [r for r in grid if loglik(r, data) >= cut]
        n = sum(g for _, _, g in data)
        print(f"{tag}: {best} (95% CI {inside[0]}-{inside[-1]}) from {n} games at levels "
              f"{', '.join(str(lv) for lv, _, _ in data)}")


if __name__ == "__main__":
    main()
