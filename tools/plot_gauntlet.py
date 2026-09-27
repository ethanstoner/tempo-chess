"""Render docs/strength.svg: Tempo's score against each Stockfish level.

Reads matches/gauntlet_<tag>_sf<level>.log (written by tools/match.py) for
each tag given, so the chart only ever shows measured results.

  python tools/plot_gauntlet.py v01:"v0.1 (PeSTO eval)" final:"v0.2"
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LEVELS = [1800, 2200, 2600, 2800, 3000, 3190]
W, H = 720, 380
L, R, T, B = 64, 40, 84, 56
# (light, dark) per series, oldest first; the newest build always gets blue.
COLORS = {1: [("#2a78d6", "#3987e5")],
          2: [("#eb6834", "#d95926"), ("#2a78d6", "#3987e5")],
          3: [("#1baf7a", "#199e70"), ("#eb6834", "#d95926"), ("#2a78d6", "#3987e5")]}


def score(tag, level):
    log = ROOT / "matches" / f"gauntlet_{tag}_sf{level}.log"
    if not log.exists():
        return None
    final = log.read_text().split("Results of")[-1]
    m = re.search(r"Points: [\d.]+ \(([\d.]+) %\)", final)
    g = re.search(r"Games: (\d+)", final)
    return (float(m.group(1)), int(g.group(1))) if m and g else None


def x_of(level):
    return L + (level - 1800) / (3190 - 1800) * (W - L - R)


def y_of(pct):
    return T + (100 - pct) / 100 * (H - T - B)


def main():
    series = [arg.split(":", 1) for arg in sys.argv[1:]]
    css_light, css_dark, marks, all_pts = [], [], [], []
    for i, (tag, label) in enumerate(series):
        light, dark = COLORS[len(series)][i]
        css_light.append(f".l{i}{{stroke:{light}}} .d{i}{{fill:{light}}}")
        css_dark.append(f".l{i}{{stroke:{dark}}} .d{i}{{fill:{dark}}}")
        pts = [(lv, score(tag, lv)) for lv in LEVELS]
        pts = [(lv, s) for lv, s in pts if s]
        path = " ".join(f"{'M' if j == 0 else 'L'}{x_of(lv):.1f},{y_of(s[0]):.1f}" for j, (lv, s) in enumerate(pts))
        marks.append(f'<path class="l{i}" d="{path}" fill="none" stroke-width="2"/>')
        for lv, (pct, games) in pts:
            marks.append(f'<circle class="d{i} ring" cx="{x_of(lv):.1f}" cy="{y_of(pct):.1f}" r="5">'
                         f'<title>{label} vs Stockfish {lv}: {pct:.1f}% over {games} games</title></circle>')
        all_pts.append(dict((lv, s[0]) for lv, s in pts))
        # Legend row: identity never rests on colour alone.
        lx = L + i * 110
        marks.append(f'<circle class="d{i}" cx="{lx + 5}" cy="{T - 12}" r="5"/>'
                     f'<text class="lbl" x="{lx + 16}" y="{T - 8}">{label}</text>')

    # Direct label for each series at the level where it sits furthest from
    # the others: above the point if it is the highest line there, else below.
    for i, pts_i in enumerate(all_pts):
        def gap(lv):
            others = [p[lv] for j, p in enumerate(all_pts) if j != i and lv in p]
            return min((abs(pts_i[lv] - o) for o in others), default=100)
        lv = max(pts_i, key=lambda k: (gap(k), -abs(k - 2600)))
        others = [p[lv] for j, p in enumerate(all_pts) if j != i and lv in p]
        above = all(pts_i[lv] >= o for o in others)
        dx, dy, anchor = (8, -12, "start") if above else (-12, 18, "end")
        marks.append(f'<text class="lbl" x="{x_of(lv) + dx:.1f}" y="{y_of(pts_i[lv]) + dy:.1f}" '
                     f'text-anchor="{anchor}">{series[i][1]}</text>')

    grid = []
    for pct in (0, 25, 50, 75, 100):
        y = y_of(pct)
        cls = "mid" if pct == 50 else "grid"
        grid.append(f'<line class="{cls}" x1="{L}" x2="{W - R}" y1="{y:.1f}" y2="{y:.1f}"/>')
        grid.append(f'<text class="tick" x="{L - 10}" y="{y + 4:.1f}" text-anchor="end">{pct}%</text>')
    for lv in LEVELS:
        grid.append(f'<text class="tick" x="{x_of(lv):.1f}" y="{H - B + 22}" text-anchor="middle">{lv}</text>')

    svg = f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" font-family="-apple-system,Segoe UI,Helvetica,Arial,sans-serif">
<style>
.bg{{fill:#fcfcfb}} .title{{fill:#0b0b0b;font-size:16px;font-weight:600}} .sub,.tick,.axis{{fill:#52514e;font-size:12px}}
.lbl{{fill:#0b0b0b;font-size:13px}} .grid{{stroke:#e4e3de;stroke-width:1}} .mid{{stroke:#9b9a93;stroke-width:1;stroke-dasharray:4 4}}
.ring{{stroke:#fcfcfb;stroke-width:2}} {"".join(css_light)}
@media (prefers-color-scheme: dark) {{
.bg{{fill:#1a1a19}} .title,.lbl{{fill:#ffffff}} .sub,.tick,.axis{{fill:#c3c2b7}} .grid{{stroke:#34332f}} .mid{{stroke:#6b6a64}} .ring{{stroke:#1a1a19}} {"".join(css_dark)}
}}
</style>
<rect class="bg" width="{W}" height="{H}" rx="8"/>
<text class="title" x="{L}" y="26">Score against Stockfish 19 at each UCI_Elo limit</text>
<text class="sub" x="{L}" y="42">100 games per point, 10s + 0.1s, one thread, 8-move opening book, both colours per opening</text>
{"".join(grid)}
<text class="axis" x="{(L + W - R) / 2:.0f}" y="{H - 8}" text-anchor="middle">Stockfish UCI_Elo setting</text>
{"".join(marks)}
</svg>
"""
    out = ROOT / "docs" / "strength.svg"
    out.parent.mkdir(exist_ok=True)
    out.write_text(svg)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
