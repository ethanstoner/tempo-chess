"""Offline end-to-end test: lichess-bot + deploy/config.yml + Tempo.

Uses lichess-bot's own mocked lichess.org server, so no account or token is
needed. The stock simulator plays a fixed four-move script that assumes the
bot mates on move 4, so it is replaced with one that plays random legal moves
as White for the whole game. Tempo must win as Black on a 10+0.1 clock and the
game PGN must be written. deploy/test_lichess.ps1 copies this file into
lichess-bot/test_bot/ and runs it with pytest.
"""
import logging
import os
import random
import sys
import tempfile
from pathlib import Path

import chess
import yaml

import test_bot.test_bot as harness
from lib.timer import Timer, seconds

DEPLOY = Path(__file__).resolve().parents[2]
ENGINE = DEPLOY.parent / "build" / ("tempo.exe" if sys.platform == "win32" else "tempo")


def random_white_simulator(move_queue, board_queue, clock_queue, results) -> None:
    rng = random.Random(2026)
    increment = seconds(0.1)
    wtime = btime = seconds(10)
    board = chess.Board()
    failure = None
    while not board.is_game_over(claim_draw=True) and not failure:
        if board.turn == chess.WHITE:
            board.push(rng.choice(list(board.legal_moves)))
            board_queue.put(board)
            clock_queue.put((wtime, btime, increment))
        else:
            timer = Timer()
            while (bot_move := move_queue.get()) is None:
                board_queue.put(board)
                clock_queue.put((wtime, btime, increment))
                move_queue.task_done()
            if bot_move not in board.legal_moves:
                failure = f"illegal move {bot_move}"
            board.push(bot_move)
            move_queue.task_done()
            # Like lichess.org, the clock only starts after each side's first move.
            if len(board.move_stack) > 2:
                btime -= timer.time_since_reset()
                btime += increment
            if btime <= seconds(0):
                failure = "Tempo flagged"
    board_queue.put(board)
    clock_queue.put((wtime, btime, increment))
    outcome = board.outcome(claim_draw=True)
    print(f"\nfinal: {board.fen()}  plies={len(board.move_stack)}  black clock left "
          f"{btime.total_seconds():.1f}s  {outcome}  failure={failure}")
    results.put(failure is None and outcome is not None and outcome.winner == chess.BLACK)


def test_tempo_plays_a_game(monkeypatch) -> None:
    monkeypatch.setattr(harness, "lichess_org_simulator", random_white_simulator)
    with open("./config.yml.default") as f:
        cfg = yaml.safe_load(f)
    with open(DEPLOY / "config.yml") as f:
        ours = yaml.safe_load(f)

    for key in ("engine", "challenge", "greeting", "abort_time", "move_overhead"):
        if isinstance(ours.get(key), dict) and isinstance(cfg.get(key), dict):
            cfg[key].update(ours[key])
        elif key in ours:
            cfg[key] = ours[key]

    with tempfile.TemporaryDirectory() as temp:
        cfg["token"] = ""
        cfg["matchmaking"]["allow_matchmaking"] = False
        cfg["engine"]["dir"] = str(ENGINE.parent)
        cfg["engine"]["name"] = ENGINE.name
        cfg["pgn_directory"] = os.path.join(temp, "games")
        assert harness.run_bot(cfg, logging.INFO)
        pgns = list(Path(cfg["pgn_directory"]).glob("*.pgn"))
        assert pgns, "no PGN written"
        text = pgns[0].read_text()
        print(text)
        assert "0-1" in text
