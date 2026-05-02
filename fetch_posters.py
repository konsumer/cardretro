#!/usr/bin/env python3
"""
fetch_posters.py — download boxart for every ROM in roms/{system}/*.ext
Output: roms/{system}/{rom_basename}.png (skips if already exists)

Strategy:
  1. Compute CRC32 for all ROMs in each system folder.
  2. Bulk-lookup game info via retroapi (grouped by system, 100 CRCs per request).
  3. Use the returned game name to find the matching libretro thumbnail.
  4. Download the poster image from the libretro CDN.
  5. Fall back to filename-based matching for ROMs not found in the API.

Usage:
  python fetch_posters.py [--roms-dir /path/to/roms] [--system nes] [-v]
  python fetch_posters.py --clear-cache    # re-download everything
"""

import json
import re
import sys
import zlib
import zipfile
import argparse
import urllib.request
import urllib.parse
from pathlib import Path

API_BASE  = "https://retroapi.konsumer.workers.dev/api"
CDN       = "https://thumbnails.libretro.com"
THUMB_ORG = "libretro-thumbnails"
CACHE_DIR = Path(".posters_cache")

# Map: local folder → (api_platform_slug, libretro_thumbnail_dir)
SYSTEM_MAP = {
    "nes":          ("nes",            "Nintendo - Nintendo Entertainment System"),
    "famicom":      ("nes",            "Nintendo - Nintendo Entertainment System"),
    "fds":          ("fds",            "Nintendo - Family Computer Disk System"),
    "snes":         ("snes",           "Nintendo - Super Nintendo Entertainment System"),
    "superfamicom": ("snes",           "Nintendo - Super Nintendo Entertainment System"),
    "gb":           ("gb",             "Nintendo - Game Boy"),
    "gbc":          ("gbc",            "Nintendo - Game Boy Color"),
    "gba":          ("gba",            "Nintendo - Game Boy Advance"),
    "n64":          ("n64",            "Nintendo - Nintendo 64"),
    "nds":          ("nds",            "Nintendo - Nintendo DS"),
    "3ds":          ("3ds",            "Nintendo - Nintendo 3DS"),
    "gamecube":     ("gamecube",       "Nintendo - GameCube"),
    "gc":           ("gamecube",       "Nintendo - GameCube"),
    "wii":          ("wii",            "Nintendo - Wii"),
    "virtualboy":   ("vb",             "Nintendo - Virtual Boy"),
    "vb":           ("vb",             "Nintendo - Virtual Boy"),
    "pokemini":     ("pokemini",       "Nintendo - Pokemon Mini"),
    "genesis":      ("megadrive",      "Sega - Mega Drive - Genesis"),
    "megadrive":    ("megadrive",      "Sega - Mega Drive - Genesis"),
    "md":           ("megadrive",      "Sega - Mega Drive - Genesis"),
    "sms":          ("mastersystem",   "Sega - Master System - Mark III"),
    "mastersystem": ("mastersystem",   "Sega - Master System - Mark III"),
    "gg":           ("gamegear",       "Sega - Game Gear"),
    "gamegear":     ("gamegear",       "Sega - Game Gear"),
    "saturn":       ("saturn",         "Sega - Saturn"),
    "32x":          ("sega32x",        "Sega - 32X"),
    "scd":          ("segacd",         "Sega - Mega-CD"),
    "segacd":       ("segacd",         "Sega - Mega-CD"),
    "dreamcast":    ("dreamcast",      "Sega - Dreamcast"),
    "dc":           ("dreamcast",      "Sega - Dreamcast"),
    "psx":          ("psx",            "Sony - PlayStation"),
    "ps1":          ("psx",            "Sony - PlayStation"),
    "playstation":  ("psx",            "Sony - PlayStation"),
    "ps2":          ("ps2",            "Sony - PlayStation 2"),
    "psp":          ("psp",            "Sony - PlayStation Portable"),
    "atari2600":    ("atari2600",      "Atari - 2600"),
    "2600":         ("atari2600",      "Atari - 2600"),
    "atari5200":    ("atari5200",      "Atari - 5200"),
    "atari7800":    ("atari7800",      "Atari - 7800"),
    "7800":         ("atari7800",      "Atari - 7800"),
    "lynx":         ("lynx",           "Atari - Lynx"),
    "jaguar":       ("jaguar",         "Atari - Jaguar"),
    "pce":          ("pce",            "NEC - PC Engine - TurboGrafx 16"),
    "tg16":         ("pce",            "NEC - PC Engine - TurboGrafx 16"),
    "pcengine":     ("pce",            "NEC - PC Engine - TurboGrafx 16"),
    "pcecd":        ("pcecd",          "NEC - PC Engine CD - TurboGrafx-CD"),
    "ngp":          ("ngp",            "SNK - Neo Geo Pocket"),
    "ngpc":         ("ngpc",           "SNK - Neo Geo Pocket Color"),
    "msx":          ("msx",            "Microsoft - MSX"),
    "msx2":         ("msx2",           "Microsoft - MSX2"),
    "fba":          ("fbneo",          "FBNeo - Arcade Games"),
    "fbneo":        ("fbneo",          "FBNeo - Arcade Games"),
    "coleco":       ("colecovision",   "Coleco - ColecoVision"),
    "intellivision":("intellivision",  "Mattel - Intellivision"),
    "vectrex":      ("vectrex",        "GCE - Vectrex"),
    "wonderswan":   ("wonderswan",     "Bandai - WonderSwan"),
    "wsc":          ("wonderswancolor","Bandai - WonderSwan Color"),
    "c64":          ("c64",            "Commodore - 64"),
    "amiga":        ("amiga",          "Commodore - Amiga"),
    "dos":          ("dos",            "DOS"),
}

ROM_EXTENSIONS = {
    ".nes", ".fds", ".unf", ".unif",
    ".smc", ".sfc", ".fig", ".swc",
    ".gb", ".gbc", ".gba",
    ".z64", ".n64", ".v64",
    ".nds", ".3ds", ".cia",
    ".iso", ".bin", ".cue", ".chd", ".pbp",
    ".md", ".gen", ".smd",
    ".sms", ".gg",
    ".pce",
    ".a26", ".a52", ".a78",
    ".lnx",
    ".ngp", ".ngc",
    ".ws", ".wsc",
    ".col", ".int", ".vec",
    ".d64", ".t64", ".prg",
    ".adf",
    ".zip", ".7z",
}

_REGIONS = {
    "USA", "Japan", "Europe", "World", "Korea", "Brazil", "China",
    "Germany", "France", "Spain", "Italy", "Netherlands", "Australia",
    "Asia", "Hong Kong", "Taiwan", "Sweden", "Norway", "Finland",
    "Denmark", "Portugal", "Russia",
    "USA, Europe", "USA, Japan", "Japan, USA", "Europe, USA",
    "Japan, Europe", "USA, Europe, Japan", "Japan, USA, Europe",
}

# ---------------------------------------------------------------------------
# Clean API/RDB name → thumbnail-compatible name
# ---------------------------------------------------------------------------

_RDB_ALT     = re.compile(r"\s*\(Alt(?:\s+\d+)?\)")
_RDB_PIRATE  = re.compile(r"\s*\(Pirate\)")
_RDB_PAREN   = re.compile(r"\(([^)]+)\)")
_RDB_BRACKET = re.compile(r"\s*\[[^\]]*\]")
_RDB_REV     = re.compile(r"\s+Rev\s+\d+")

_REGION_ALIASES = {
    "EU-US": "USA, Europe", "US-EU": "USA, Europe",
    "USA-EUR": "USA, Europe", "EUR-USA": "USA, Europe",
    "JPN": "Japan", "JAP": "Japan", "JA": "Japan",
    "US": "USA", "EU": "Europe", "KOR": "Korea",
    "AUS": "Australia",
}


def rdb_clean_name(rdb_name: str) -> str:
    """Strip publisher names, alt/pirate/bracket markers from a game name."""
    name = _RDB_ALT.sub("", rdb_name)
    name = _RDB_PIRATE.sub("", name)
    name = _RDB_BRACKET.sub("", name)

    base  = _RDB_PAREN.sub("", name).strip()
    parts = _RDB_PAREN.findall(name)

    kept = []
    for p in parts:
        canonical = _REGION_ALIASES.get(p, p)
        if canonical in _REGIONS:
            kept.append(canonical)

    seen = set()
    kept = [r for r in kept if not (r in seen or seen.add(r))]

    base   = _RDB_REV.sub("", base)
    result = re.sub(r"\s{2,}", " ", base)
    for p in kept:
        result += f" ({p})"
    return result.strip()

# ---------------------------------------------------------------------------
# Thumbnail index
# ---------------------------------------------------------------------------

_ILLEGAL = re.compile(r'[&\*/:"`<>?\\|]')


def _libretro(s: str) -> str:
    return _ILLEGAL.sub("_", s)


def _repo_name(system_libretro: str) -> str:
    return system_libretro.replace(" ", "_").replace("/", "-")


def load_thumb_index(system_libretro: str, verbose: bool) -> list[str]:
    """Return sorted list of Named_Boxarts stems (no .png), downloaded once."""
    cache = CACHE_DIR / f"{_repo_name(system_libretro)}.index"
    if cache.exists():
        lines = [l for l in cache.read_text().splitlines() if l]
        if lines:
            return lines

    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    repo = _repo_name(system_libretro)
    url  = f"https://api.github.com/repos/{THUMB_ORG}/{repo}/git/trees/HEAD?recursive=1"
    print(f"  [index] fetching thumbnail list for {system_libretro}...")
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "fetch_posters/5.0"})
        with urllib.request.urlopen(req, timeout=30) as resp:
            data = json.load(resp)
        names = sorted(set(
            Path(item["path"]).stem
            for item in data.get("tree", [])
            if item["path"].startswith("Named_Boxarts/") and item["path"].endswith(".png")
        ))
        if names:
            cache.write_text("\n".join(names))
            print(f"  [index] {len(names)} thumbnails indexed")
        return names
    except Exception as e:
        print(f"  [index] failed: {e}")
        return []

# ---------------------------------------------------------------------------
# Thumbnail index searching
# ---------------------------------------------------------------------------

def _prefix_match(indexed: str, base: str) -> bool:
    n, b = indexed.lower(), base.lower()
    return n == b or n.startswith(b + " (") or n.startswith(b + " [")


def _word_overlap(indexed: str, form: str) -> float:
    def sig(s: str) -> set[str]:
        return {w.lower() for w in re.findall(r"\w+", s) if len(w) >= 4}
    bw = sig(form)
    return len(bw & sig(indexed)) / len(bw) if len(bw) >= 3 else 0.0


def find_in_index(name: str, regions: list[str], index: list[str], verbose: bool) -> str | None:
    def _forms(s: str) -> list[str]:
        seen, out = set(), []
        def add(x):
            x = _libretro(x.strip())
            if x and x not in seen: seen.add(x); out.append(x)
        add(s)
        nodash = re.sub(r"\s+-\s+", " ", s)
        if nodash != s: add(nodash)
        roman = re.sub(r"\b(\d+)\b",
            lambda m: {"2":"II","3":"III","4":"IV","5":"V","6":"VI","7":"VII","8":"VIII"}.get(m.group(1), m.group(1)), s)
        if roman != s: add(roman)
        no1 = re.sub(r"\s+1$", "", s).strip()
        if no1 and no1 != s: add(no1)
        joined = re.sub(r"([A-Za-z])\s+(\d+)$", r"\1\2", s)
        if joined != s: add(joined)
        return out

    def region_score(n: str) -> int:
        for i, r in enumerate(regions):
            if f"({r})" in n: return len(regions) - i
        return 0

    forms = _forms(name)

    for form in forms:
        hits = [n for n in index if _prefix_match(n, form)]
        if hits:
            hits.sort(key=lambda n: (-region_score(n), n.count("(")))
            if verbose: print(f"  [prefix] {form!r} → {hits[0]!r}")
            return hits[0]

    best_s, best_n = 0.0, None
    for form in forms:
        for n in index:
            s = _word_overlap(n.split("(")[0].strip(), form)
            if s > best_s and s >= 0.85:
                best_s, best_n = s, n
    if best_n:
        if verbose: print(f"  [fuzzy] → {best_n!r} (score={best_s:.2f})")
        return best_n

    return None

# ---------------------------------------------------------------------------
# ROM CRC32 computation
# ---------------------------------------------------------------------------

def rom_crc32(path: Path) -> str | None:
    try:
        suffix = path.suffix.lower()
        if suffix == ".zip":
            with zipfile.ZipFile(path) as zf:
                infos = [i for i in zf.infolist() if not i.filename.endswith("/")]
                return f"{infos[0].CRC & 0xFFFFFFFF:08x}" if infos else None
        data = path.read_bytes()
        if suffix in (".smc", ".fig", ".swc") and len(data) % 1024 == 512:
            data = data[512:]
        if suffix == ".v64" and len(data) >= 4:
            data = bytes(b for pair in zip(data[1::2], data[::2]) for b in pair)
        elif suffix == ".n64" and len(data) >= 4:
            data = bytes(b for i in range(0, len(data), 4)
                         for b in [data[i+3], data[i+2], data[i+1], data[i]])
        return f"{zlib.crc32(data) & 0xFFFFFFFF:08x}"
    except Exception:
        return None

# ---------------------------------------------------------------------------
# Bulk API lookup
# ---------------------------------------------------------------------------

def bulk_api_lookup(platform: str, crcs: list[str], verbose: bool) -> dict[str, dict]:
    """Lookup game info for a list of CRC32 hex strings. Returns {crc: game_info}."""
    results = {}
    chunk_size = 100
    for i in range(0, len(crcs), chunk_size):
        chunk = crcs[i:i + chunk_size]
        url = f"{API_BASE}/{platform}/crc/{','.join(chunk)}"
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "fetch_posters/5.0"})
            with urllib.request.urlopen(req, timeout=30) as resp:
                data = json.load(resp)
            for crc, matches in zip(chunk, data):
                if matches:
                    results[crc] = matches[0]
        except Exception as e:
            if verbose:
                print(f"  [api] error: {e}")
    return results

# ---------------------------------------------------------------------------
# Filename-based fallback (GoodTools parsing)
# ---------------------------------------------------------------------------

_GT_REGIONS: list[tuple[str, list[str]]] = [
    ("JUE", ["World"]), ("JU", ["World", "USA, Japan"]),
    ("UE",  ["Europe, USA", "USA, Europe", "World"]),
    ("W",   ["World"]),
    ("U",   ["USA", "USA, Europe", "World"]),
    ("J",   ["Japan", "World"]),
    ("E",   ["Europe", "USA, Europe"]),
    ("G",   ["Germany", "Europe"]), ("F", ["France", "Europe"]),
    ("S",   ["Spain", "Europe"]),   ("I", ["Italy", "Europe"]),
    ("B",   ["Brazil"]),            ("C", ["China"]),
    ("K",   ["Korea"]),             ("A", ["Australia"]),
]
_GT_PAT    = re.compile(r"\((?:" + "|".join(re.escape(c) for c, _ in _GT_REGIONS) + r")\)")
_GT_LOOKUP = {c: r for c, r in _GT_REGIONS}
_GT_STRIP  = re.compile(r"\s*\[[^\]]*\]|\s*\([^)]*\)")


def gt_parse(stem: str) -> tuple[str, list[str]]:
    m = _GT_PAT.search(stem)
    code    = m.group(0)[1:-1] if m else None
    regions = _GT_LOOKUP.get(code, []) if code else []
    base    = _GT_STRIP.sub("", stem).strip()
    return base, regions

# ---------------------------------------------------------------------------
# HTTP
# ---------------------------------------------------------------------------

def _http_get(url: str, dest: Path, verbose: bool) -> bool:
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "fetch_posters/5.0"})
        with urllib.request.urlopen(req, timeout=15) as resp:
            if resp.status == 200:
                dest.write_bytes(resp.read())
                return True
    except Exception as e:
        if verbose: print(f"      http: {e}")
    return False


def poster_url(system_dir: str, name: str) -> str:
    return f"{CDN}/{urllib.parse.quote(system_dir)}/Named_Boxarts/{urllib.parse.quote(name)}.png"

# ---------------------------------------------------------------------------
# Per-ROM processing
# ---------------------------------------------------------------------------

def process_rom(
    rom:         Path,
    sys_dir:     str,
    api_results: dict[str, dict],
    th_index:    list[str],
    args,
) -> str:
    poster = rom.with_suffix(".png")
    if poster.exists():
        if args.verbose: print(f"  skip   {rom.name}")
        return "skip"

    crc = rom_crc32(rom)

    # --- 1. API lookup ---
    clean_name: str | None = None
    regions: list[str] = []

    if crc and crc in api_results:
        game_name  = api_results[crc].get("name", "")
        clean_name = rdb_clean_name(game_name)
        regions    = [p for p in re.findall(r"\(([^)]+)\)", clean_name) if p in _REGIONS]
        if args.verbose:
            print(f"  [api] {crc} → {game_name!r} → {clean_name!r}")
    elif args.verbose and crc:
        print(f"  [api] {crc} not found")

    # Strip trailing region/bracket parentheticals to get bare search title
    search_name = clean_name
    if clean_name:
        search_name = re.sub(r"\s*\[[^\]]*\]", "", clean_name)
        search_name = re.sub(r"(\s*\([^)]+\))+$", "", search_name).strip()
        if not search_name:
            search_name = clean_name

    # --- 2. Fall back to filename parsing if no API hit ---
    if not clean_name:
        search_name, regions = gt_parse(rom.stem)

    # --- 3. Search thumbnail index ---
    if th_index:
        match = find_in_index(search_name, regions, th_index, args.verbose)
        if match:
            if _http_get(poster_url(sys_dir, match), poster, args.verbose):
                label = "api" if crc and crc in api_results else "name"
                print(f"  found  {rom.name}  [{label}: {match!r}]")
                return "found"

    # --- 4. Direct URL fallback (when index unavailable) ---
    if clean_name:
        url = poster_url(sys_dir, _libretro(clean_name))
        if args.verbose: print(f"  [direct] {url}")
        if _http_get(url, poster, args.verbose):
            print(f"  found  {rom.name}  [direct: {clean_name!r}]")
            return "found"

    print(f"  miss   {rom.name}")
    return "miss"

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(description="Fetch boxart for ROMs from libretro thumbnails")
    parser.add_argument("--roms-dir",    default="roms")
    parser.add_argument("--system",      help="Only process this system folder")
    parser.add_argument("--verbose", "-v", action="store_true")
    parser.add_argument("--clear-cache", action="store_true", help="Delete and re-download all cached data")
    args = parser.parse_args()

    if args.clear_cache and CACHE_DIR.exists():
        import shutil
        shutil.rmtree(CACHE_DIR)
        print("Cache cleared.")

    roms_root = Path(args.roms_dir)
    if not roms_root.is_dir():
        print(f"ERROR: roms directory not found: {roms_root}", file=sys.stderr)
        sys.exit(1)

    system_dirs = sorted(
        d for d in roms_root.iterdir()
        if d.is_dir() and (args.system is None or d.name.lower() == args.system.lower())
    )
    if not system_dirs:
        print("No system directories found.")
        return

    total_found = total_miss = total_skip = 0

    for sdir in system_dirs:
        entry = SYSTEM_MAP.get(sdir.name.lower())
        if entry is None:
            print(f"\n[{sdir.name}] — unknown system (add to SYSTEM_MAP)")
            continue

        api_slug, sys_libretro = entry
        roms = sorted(f for f in sdir.iterdir() if f.suffix.lower() in ROM_EXTENSIONS)
        if not roms:
            continue

        print(f"\n[{sdir.name}]  ({len(roms)} ROMs)")

        # Compute all CRCs first, then bulk-lookup
        rom_crcs = {rom: rom_crc32(rom) for rom in roms}
        valid_crcs = [crc for crc in rom_crcs.values() if crc]

        print(f"  Looking up {len(valid_crcs)} CRCs via API...")
        api_results = bulk_api_lookup(api_slug, valid_crcs, args.verbose)
        print(f"  API matched {len(api_results)} of {len(valid_crcs)}")

        th_index = load_thumb_index(sys_libretro, args.verbose)
        if th_index:
            print(f"  Thumbnails: {len(th_index)} indexed")

        for rom in roms:
            result = process_rom(rom, sys_libretro, api_results, th_index, args)
            if result == "found":   total_found += 1
            elif result == "miss":  total_miss  += 1
            else:                   total_skip  += 1

    print(f"\nDone.  found={total_found}  miss={total_miss}  skip={total_skip}")
    if total_miss:
        print("Tip: -v to see what was tried.")


if __name__ == "__main__":
    main()
