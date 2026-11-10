#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Render Nightscape FM pieces to WAV files on the computer, as the FM-1 would play them, many times faster than real time.

It runs the whole firmware (its pages, Dream, the dealers, the engine) in fm1-base's host simulator, and writes 24-bit
44.1 kHz stereo. A Seed or a Dream number grows the same garden here as on the FM-1, from PLAY.

    python tools/amb_render.py fetch                       the FM-1's eight pieces to renders/pieces.amb1 (press SAVE
                                                            on the FM-1 first; it may keep playing)
    python tools/amb_render.py render [options]            one piece, or a batch

Options for render:
    --pieces FILE        the pieces from the FM-1 (default renders/pieces.amb1 when there; else the stock recipes)
    --piece NAME|1-8     which piece (default: the one the FM-1 was on, else Blue Hour); AMB-1's old names work too
    --minutes M          how long it plays (default 10); then the tails ring out (--tail, default 30 s)
    --fade S             a fade-in of S seconds (default 6)
    --dream N|A-B|wild   a Dream number, a batch of them (one file each), or a new one picked here
    --range R            Dream's Range (0-100)       --cap C   Dream's Spice cap (0-100)
    --seed N|wild        the garden's Seed, 1-999 (when no Dream); wild: one picked here (default, so each render
                         differs; the number used is printed, to find it again on the FM-1)
    --set NAME=VALUE     any knob, after Dream has dealt (amb.h's names without P_: --set hats=80 --set padtone=3)
    --out DIR            where the files go (default renders/)

The FM-1's own pieces need the firmware with backup (this repository's, from the backup build on). The simulator is
built here the first time, and again when the firmware's sources change: it needs the ziglang and Pillow packages.
"""
import argparse
import glob
import os
import random
import re
import runpy
import shutil
import subprocess
import sys
import tempfile
import time

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
APP = os.path.join(REPO, "app")
HOST = os.path.join(APP, "build", "host", "amb_host")
PIECES = ["Blue Hour", "Last Light", "Pines", "Ember Line", "Chevrons", "Afterglow", "Headlights", "Gravel"]
OLD = ["Garden", "Departures", "Voices", "Afternoon", "Night Bus", "Shoreline", "Glass", "Tides"]   # (as AMB-1)
MAGIC = "AMB1"


def params():
    """amb.h's P_* names, in order: their numbers"""
    with open(os.path.join(APP, "dsp", "amb.h"), encoding="utf-8") as f:
        t = f.read()
    t = re.sub(r"/\*.*?\*/", "", t, flags=re.S)
    m = re.search(r"enum\s*\{([^}]*P_NPARAMS[^}]*)\}", t)
    names = re.findall(r"\bP_[A-Z0-9_]+\b", m.group(1))
    return {n[2:].lower(): i for i, n in enumerate(names) if n != "P_NPARAMS"}


def zig():
    try:
        import ziglang
        z = glob.glob(os.path.join(os.path.dirname(ziglang.__file__), "zig*"))
        if z:
            return z[0]
    except ImportError:
        pass
    for p in (os.environ.get("AMB_ZIG", ""), *glob.glob(os.path.expanduser(r"~\fm1-tools\Lib\site-packages\ziglang\zig*"))):
        if p and os.path.isfile(p):
            return p
    sys.exit("amb_render: no zig compiler (pip install ziglang, or AMB_ZIG=path)")


SHIM = """from PIL import ImageFont as F
import inspect
if not getattr(F, "_amb_shim", False):
    F._amb_shim = True
    for name in ("getbbox", "getlength", "getmask", "getmask2"):
        def wrap(orig):
            sig = inspect.signature(orig)
            def f(self, *a, **k):
                b = sig.bind(self, *a, **k)
                for n in ("features", "language", "direction"):
                    if n in b.arguments:
                        b.arguments[n] = None
                return orig(*b.args, **b.kwargs)
            return f
        setattr(F.FreeTypeFont, name, wrap(getattr(F.FreeTypeFont, name)))
"""


def host_exe():
    return HOST + (".exe" if os.name == "nt" else "")


def build_host():
    """the simulator, when it's missing or older than the firmware's sources"""
    exe = host_exe()
    srcs = [p for e in ("c", "h", "toml", "tsv") for p in glob.glob(os.path.join(APP, "**", "*." + e), recursive=True)
            if os.sep + "build" + os.sep not in p]
    if os.path.isfile(exe) and os.path.getmtime(exe) >= max(os.path.getmtime(p) for p in srcs):
        return exe
    print("amb_render: building the simulator (once, and after the firmware changes) ...", flush=True)
    z = zig()
    run = subprocess.run

    def cc(cmd, *a, **k):
        if isinstance(cmd, (list, tuple)) and cmd and cmd[0] == "cc":
            cmd = [z, "cc", *cmd[1:]]
        return run(cmd, *a, **k)

    try:
        import PIL  # noqa: F401  (the build draws its fonts with Pillow)
    except ImportError:
        sys.exit("amb_render: the build needs Pillow (pip install pillow)")
    shim = tempfile.mkdtemp()               # (a Pillow without libraqm: its font features dropped, in every step)
    open(os.path.join(shim, "sitecustomize.py"), "w", encoding="utf-8").write(SHIM)
    old_pp = os.environ.get("PYTHONPATH")
    os.environ["PYTHONPATH"] = shim + (os.pathsep + old_pp if old_pp else "")
    fj = os.path.join(REPO, "firmware.json")
    kept = open(fj, "rb").read() if os.path.isfile(fj) else None
    here, argv = os.getcwd(), sys.argv
    subprocess.run = cc
    try:
        os.chdir(REPO)
        sys.argv = ["base/tools/build.py", "app", "--host"]
        runpy.run_path(os.path.join("base", "tools", "build.py"), run_name="__main__")
    except SystemExit as e:
        if e.code:
            sys.exit(f"amb_render: the simulator's build failed ({e.code})")
    finally:
        subprocess.run, sys.argv = run, argv
        os.chdir(here)
        if kept is not None:                # (the build writes it: left as it was)
            open(fj, "wb").write(kept)
        if old_pp is None:
            os.environ.pop("PYTHONPATH", None)
        else:
            os.environ["PYTHONPATH"] = old_pp
        shutil.rmtree(shim, ignore_errors=True)
    if os.name == "nt":
        shutil.copyfile(HOST, exe)
    return exe


def fetch(out):
    os.makedirs(os.path.dirname(out), exist_ok=True)
    py = sys.executable                    # (the backup needs mido and python-rtmidi: here, or the fm1-tools venv)
    try:
        import mido  # noqa: F401
        import rtmidi  # noqa: F401
    except ImportError:
        for p in (os.environ.get("AMB_TOOLS_PY", ""), os.path.join(os.path.expanduser("~"), "fm1-tools", "Scripts", "python.exe"),
                  os.path.expanduser("~/fm1-tools/bin/python")):
            if p and os.path.isfile(p):
                py = p
                break
    r = subprocess.run([py, os.path.join(REPO, "base", "tools", "fm1_backup.py"), "backup", out, "--magic", MAGIC])
    if r.returncode:
        sys.exit("amb_render: no backup (is the FM-1 plugged in, on this firmware? fm1_backup.py needs mido and"
                 " python-rtmidi)")
    print(f"amb_render: the FM-1's pieces in {out}")


def piece_index(s):
    if s is None:
        return None
    if s.isdigit() and 1 <= int(s) <= 8:
        return int(s) - 1
    for i, n in enumerate(PIECES):
        if s.lower().replace(" ", "") in (n.lower().replace(" ", ""), OLD[i].lower().replace(" ", "")):
            return i
    sys.exit(f"amb_render: no piece {s!r} (one of {', '.join(PIECES)}, or 1-8)")


def fwd(p):
    return os.path.abspath(p).replace("\\", "/")


def render_one(exe, P, a, pieces, piece, dream, seed, out):
    lines = ["wait 400", "transport 0", "wait 300"]
    if pieces:
        lines += [f"restore {fwd(pieces)}", "wait 400", "transport 0", "wait 300"]
    if piece is not None:
        lines += [f"piece {piece}", "wait 300"]
    lines += [f"knob {P['midi']} 0"]          # (MIDI out off: nothing to log)
    if a.range is not None:
        lines.append(f"knob {P['drange']} {a.range}")
    if a.cap is not None:
        lines.append(f"knob {P['dcap']} {a.cap}")
    if dream is not None:
        lines += [f"knob {P['dream']} 0", f"knob {P['dream']} {dream + 1}"]
    elif seed is not None:
        lines.append(f"knob {P['seed']} {seed}")
    for kv in a.set or []:
        k, _, v = kv.partition("=")
        k = k.strip().lower().removeprefix("p_")
        if k not in P or not v.strip().lstrip("-").isdigit():
            sys.exit(f"amb_render: --set {kv}: a knob from amb.h (hats, padtone, density ...) and a whole number")
        lines.append(f"knob {P[k]} {int(v)}")
    lines += ["wait 300", f"render {fwd(out)} {a.minutes * 60:.0f} {a.fade} {a.tail}"]
    with tempfile.TemporaryDirectory() as d:
        sc = os.path.join(d, "render.amb")
        open(sc, "w", encoding="utf-8").write("\n".join(lines) + "\n")
        r = subprocess.run([exe, sc, d], capture_output=True, text=True)
    said = [l for l in r.stdout.splitlines() if l.startswith("render:") or "cannot" in l or "FAIL" in l]
    for l in said:
        print("  " + l.replace("render: ", ""))
    if r.returncode or not os.path.isfile(out):
        sys.exit(f"amb_render: the simulator stopped ({r.returncode}):\n{r.stdout[-800:]}{r.stderr[-800:]}")


def main():
    ap = argparse.ArgumentParser(description="Nightscape FM pieces to WAV on the computer")
    sp = ap.add_subparsers(dest="cmd", required=True)
    f = sp.add_parser("fetch", help="the FM-1's eight pieces to a file")
    f.add_argument("--to", default=os.path.join(REPO, "renders", "pieces.amb1"))
    r = sp.add_parser("render", help="one piece, or a batch")
    r.add_argument("--pieces")
    r.add_argument("--piece")
    r.add_argument("--minutes", type=float, default=10.0)
    r.add_argument("--fade", type=float, default=6.0)
    r.add_argument("--tail", type=float, default=30.0)
    r.add_argument("--dream")
    r.add_argument("--range", type=int)
    r.add_argument("--cap", type=int)
    r.add_argument("--seed", default="wild")
    r.add_argument("--set", action="append")
    r.add_argument("--out", default=os.path.join(REPO, "renders"))
    a = ap.parse_args()
    if a.cmd == "fetch":
        return fetch(a.to)
    pieces = a.pieces or (os.path.join(REPO, "renders", "pieces.amb1")
                          if os.path.isfile(os.path.join(REPO, "renders", "pieces.amb1")) else None)
    if pieces and not os.path.isfile(pieces):
        sys.exit(f"amb_render: no {pieces}")
    piece = piece_index(a.piece)
    dreams = [None]
    if a.dream:
        if a.dream.lower() == "wild":
            dreams = [random.randint(1, 999)]
        elif re.fullmatch(r"\d+-\d+", a.dream):
            lo, hi = map(int, a.dream.split("-"))
            dreams = list(range(max(1, lo), min(999, hi) + 1))
        elif a.dream.isdigit() and 1 <= int(a.dream) <= 999:
            dreams = [int(a.dream)]
        else:
            sys.exit("amb_render: --dream N (1-999), A-B or wild")
    seed = None
    if dreams == [None]:
        seed = random.randint(1, 999) if a.seed.lower() == "wild" else int(a.seed)
    os.makedirs(a.out, exist_ok=True)
    P = params()
    exe = build_host()
    name = PIECES[piece].replace(" ", "") if piece is not None else "Piece"
    print(f"amb_render: {'the FM-1s pieces, ' if pieces else 'the stock recipes, '}"
          f"{PIECES[piece] if piece is not None else 'the piece it was on'}, {a.minutes:g} min each")
    t0 = time.time()
    for dm in dreams:
        tag = f"dream{dm}" if dm is not None else f"seed{seed}"
        out = os.path.join(a.out, f"{name}_{tag}.wav")
        print(f"{name}, {'Dream ' + str(dm) if dm is not None else 'Seed ' + str(seed)} -> {out}", flush=True)
        render_one(exe, P, a, pieces, piece, dm, seed, out)
    print(f"amb_render: done in {time.time() - t0:.0f} s")


if __name__ == "__main__":
    main()
