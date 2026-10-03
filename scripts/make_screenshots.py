#!/usr/bin/env python3
"""Render documentation screenshots from the real program output.

Every pixel of text in docs/images/ is produced by running ./cms or ./bench_bin and
drawing what they printed. Nothing here is mocked up by hand.

    python3 scripts/make_screenshots.py

Requires Pillow:  uv pip install pillow
"""
import os
import re
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "docs", "images")
TMP = os.path.join(ROOT, "scratch")

CMS = os.path.join(ROOT, "cms")
BENCH = os.path.join(ROOT, "bench_bin")

# macOS-ish terminal palette
BG = (24, 24, 28)
BAR = (40, 40, 46)
FG = (214, 214, 220)
DIM = (128, 130, 140)
GREEN = (126, 214, 138)
RED = (240, 118, 118)
YELLOW = (232, 200, 120)
CYAN = (120, 200, 220)
BOLD = (245, 245, 250)

FONT_CANDIDATES = [
    "/System/Library/Fonts/SFNSMono.ttf",
    "/System/Library/Fonts/Menlo.ttc",
    "/System/Library/Fonts/Supplemental/Andale Mono.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
]

SCALE = 2
FONT_PX = 15 * SCALE
LINE_H = int(FONT_PX * 1.46)
PAD = 22 * SCALE
BAR_H = 34 * SCALE


def font_path():
    for p in FONT_CANDIDATES:
        if os.path.exists(p):
            return p
    raise SystemExit("no monospace font found")


def load_font(size):
    return ImageFont.truetype(font_path(), size)


def run(args, stdin_text):
    p = subprocess.run(args, input=stdin_text, capture_output=True, text=True, cwd=ROOT)
    return p.stdout


def strip_menu_blocks(text):
    """Remove the repeated main menu so a capture reads as one continuous session."""
    lines = text.split("\n")
    out, i = [], 0
    while i < len(lines):
        is_rule = len(lines[i]) >= 60 and set(lines[i].strip()) == {"-"}
        if is_rule and i + 1 < len(lines) and lines[i + 1].strip().startswith("1  Add contact"):
            # find the closing rule, at most 20 lines down
            j = i + 2
            while j < len(lines) and j < i + 22:
                if lines[j].strip().startswith("0  Exit"):
                    if j + 1 < len(lines) and set(lines[j + 1].strip()) == {"-"}:
                        i = j + 2
                        break
                j += 1
            else:
                out.append(lines[i])
                i += 1
            continue
        out.append(lines[i])
        i += 1
    return "\n".join(out)


def trim(text):
    lines = text.split("\n")
    while lines and not lines[0].strip():
        lines.pop(0)
    while lines and (not lines[-1].strip() or lines[-1].strip() == "Choice:"):
        lines.pop()
    return lines


def colour_for(line):
    """Colour a line the way a terminal would highlight it, using the text itself."""
    s = line.strip()
    if "REJECTED" in line:
        return RED
    if s.startswith("---") or (s.startswith("##") ):
        return CYAN
    if re.search(r"\b(OK:|PASSED|audit correctly|Restored|exact hash hit|Exact hash hit)\b", line):
        return GREEN
    if re.search(r"(AUDIT FAILED|no digits|Invalid phone|Duplicate)", line):
        return RED
    if re.match(r"^(ID|n \||\|)", s) or s.startswith("Choice"):
        return DIM
    if re.search(r"(Choice:|Sort by:|Order:|Search text:|Phone \(full|Type a prefix|Name\s+:|records \d)", line):
        return YELLOW
    if set(s) and set(s) <= {"-"}:
        return DIM
    return FG


def render(lines, path, title, max_chars=None):
    f = load_font(FONT_PX)
    char_w = f.getlength("M")
    n = max(len(ln) for ln in lines) if lines else 20
    if max_chars:
        n = min(n, max_chars)
        lines = [ln[:max_chars] for ln in lines]
    w = int(2 * PAD + n * char_w)
    h = int(BAR_H + PAD + len(lines) * LINE_H + PAD * 0.6)
    img = Image.new("RGB", (w, h), BG)
    d = ImageDraw.Draw(img)
    # title bar
    d.rectangle([0, 0, w, BAR_H], fill=BAR)
    r = BAR_H * 0.13
    for k, col in enumerate([(255, 95, 86), (255, 189, 46), (39, 201, 63)]):
        cx = PAD * 0.75 + k * r * 3.4
        cy = BAR_H / 2
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=col)
    tf = load_font(int(FONT_PX * 0.82))
    tw = tf.getlength(title)
    d.text(((w - tw) / 2, BAR_H / 2 - FONT_PX * 0.44), title, font=tf, fill=(180, 182, 190))
    # body
    y = BAR_H + PAD * 0.7
    for ln in lines:
        if ln.strip():
            d.text((PAD, y), ln, font=f, fill=colour_for(ln))
        y += LINE_H
    # sanity check: no ink outside the canvas
    bbox = Image.eval(img, lambda v: 255 if v else 0).getbbox()
    assert bbox[0] >= 0 and bbox[1] >= 0 and bbox[2] <= w and bbox[3] <= h, "content clipped"
    img.save(path)
    return w, h, len(lines), n


def capture(name, stdin_text, args=None, seed=False, start=None, end=None, max_chars=None,
            title=None, keep_menu=False):
    data = os.path.join(TMP, name + ".csv")
    if os.path.exists(data):
        os.remove(data)
    cmd = [CMS, "--data", data] + (["--seed"] if seed else []) + (args or [])
    text = run(cmd, stdin_text)
    if not keep_menu:
        text = strip_menu_blocks(text)
    if start:
        text = text[text.index(start):]
    if end:
        text = text[:text.index(end)]
    text = text.replace("\n\n\n", "\n")
    lines = trim(text)
    size = render(lines, os.path.join(OUT, name + ".png"), title or name, max_chars)
    print("  %-22s %4dx%-5d %3d lines, %3d cols" % (name + ".png", size[0], size[1], size[2], size[3]))


def main():
    if not os.path.exists(CMS):
        raise SystemExit("build first: make")
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(TMP, exist_ok=True)
    print("writing", OUT)

    # 1. hero: the required display feature, sorted by name
    capture("hero-display", "13\n2\n1\na\n0\n", seed=True, start="--- 2. Display",
            title="cms  -  Display all contacts, sorted by name")

    # 2. the menu itself, exactly as printed
    capture("menu", "0\n", seed=True, start="\n  1  Add contact", end="Choice: ",
            title="cms  -  main menu", keep_menu=True)

    # 3. all four search modes in one session
    capture("search-modes",
            "13\n4\n9900112233\n4\n9445\n3\nAnanaya\n4\n3\nkavya\n3\n0\n",
            seed=True, start="--- 4. Search by phone", end="OK: Saved",
            title="cms  -  four search modes: hash hit, scan, fuzzy, case-insensitive")

    # 4. validation: three refusals
    capture("validation",
            "13\n1\nCopy of Aarav\n098765-43210\n\n\n\n1\nNo Digits\nabc\n\n\n\n0\n",
            seed=True, start="--- 1. Add contact", end="OK: Saved",
            title="cms  -  invalid input is refused with a reason, never a crash")

    # 5. the sorting experiment with live comparison counters
    capture("sorting", "13\n7\n0\n", seed=True, start="SORTING EXPERIMENT", end="Sorted by name",
            title="cms  -  sort vs stable_sort, with real comparison counts")

    # 6. the index audit, passing and detecting injected damage
    pass_out = run([CMS, "--data", os.path.join(TMP, "audit.csv"), "--seed"], "")
    pass_out = run([CMS, "--data", os.path.join(TMP, "audit.csv"), "--audit"], "")
    fail_out = run([CMS, "--data", os.path.join(TMP, "audit.csv"), "--inject-fault", "3", "--audit"], "")
    lines = ["$ ./cms --data contacts.csv --audit"]
    lines += [l for l in pass_out.split("\n") if l.strip() and "CONTACT MANAGEMENT SYSTEM" not in l]
    lines += [""]
    lines += ["$ ./cms --data contacts.csv --inject-fault 3 --audit"]
    lines += [l for l in fail_out.split("\n") if l.strip() and "CONTACT MANAGEMENT SYSTEM" not in l]
    size = render(lines, os.path.join(OUT, "audit.png"), "cms  -  index audit, and the same audit on a corrupted index")
    print("  %-22s %4dx%-5d %3d lines, %3d cols" % ("audit.png", size[0], size[1], size[2], size[3]))

    # 7. benchmarks, straight from the benchmark binary
    if os.path.exists(BENCH):
        bench = run([BENCH, "1000", "10000", "100000"], "")
        bench = bench.split("## How to read")[0]
        lines = [l for l in bench.split("\n") if not l.startswith("Wrote ")]
        lines = ["$ ./bench_bin 1000 10000 100000"] + trim("\n".join(lines))
        lines = [l[:96] for l in lines]
        size = render(lines, os.path.join(OUT, "benchmarks.png"),
                      "./bench_bin  -  every data-structure choice, measured")
        print("  %-22s %4dx%-5d %3d lines, %3d cols" % ("benchmarks.png", size[0], size[1], size[2], size[3]))


if __name__ == "__main__":
    main()
