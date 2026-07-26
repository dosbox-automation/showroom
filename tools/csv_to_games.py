# This file is part of the dosbox-automation-showroom Project.
# License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
#
"""Convert the showroom games source CSV into per-game JSON assets.

One-time migration plus regeneration tool: reads the curated CSV
(design doc, human-edited) and emits assets/games/<slug>.json per game
plus assets/games/index.json with the display order. The JSON files
are what the showroom app consumes; the CSV is the editing surface.
"""

import argparse
import csv
import json
import logging
import os
import sys
import tempfile
from pathlib import Path

logger = logging.getLogger(__name__)

KNOWN_LICENSES = {"shareware", "freeware", "demo"}
KNOWN_INSTALL_TYPES = {"unzip", "unzipinstall", "exeinstall", "floppyinstall", "isoinstall"}
KNOWN_RECIPE_STATUS = {"done", "todo"}

EXPECTED_COLUMNS = [
    "rank", "game", "version", "slug", "license", "recipe_status",
    "type1", "type2", "source_url", "mirror_url", "archive_filename",
    "screen_menu", "screen_gameplay", "sha256", "notes",
]


def _clean(value: str | None) -> str | None:
    """Empty cells and the '-' placeholder both mean 'no value'."""
    if value is None:
        return None
    value = value.strip()
    return value if value not in ("", "-") else None


def _flag(value: str | None, column: str, line_no: int, slug: str) -> bool:
    """Screenshot columns: 'x' means the shot exists, empty means not yet."""
    value = _clean(value)
    if value is None:
        return False
    if value.lower() == "x":
        return True
    raise ValueError(f"line {line_no} ({slug}): {column} must be 'x' or empty, got {value!r}")


def parse_row(row: dict, line_no: int) -> dict:
    """Validate one CSV row and shape it into the per-game JSON structure."""
    slug = _clean(row.get("slug"))
    title = _clean(row.get("game"))
    if not slug or not title:
        raise ValueError(f"line {line_no}: slug and game are required")

    rank_raw = _clean(row.get("rank"))
    if rank_raw is None or not rank_raw.isdigit():
        raise ValueError(f"line {line_no} ({slug}): rank must be a positive integer")

    license_ = _clean(row.get("license"))
    if license_ not in KNOWN_LICENSES:
        raise ValueError(f"line {line_no} ({slug}): unknown license {license_!r}")

    status = _clean(row.get("recipe_status"))
    if status not in KNOWN_RECIPE_STATUS:
        raise ValueError(f"line {line_no} ({slug}): unknown recipe_status {status!r}")

    sources = []
    for role, url_col, type_col in (
        ("primary", "source_url", "type1"),
        ("mirror", "mirror_url", "type2"),
    ):
        url = _clean(row.get(url_col))
        install_type = _clean(row.get(type_col))
        if url is None:
            continue
        if install_type is not None and install_type not in KNOWN_INSTALL_TYPES:
            raise ValueError(
                f"line {line_no} ({slug}): unknown install type {install_type!r} in {type_col}"
            )
        sources.append({
            "role": role,
            "install_type": install_type,
            "url": url,
            # Filled by the license verification pass (ada-l996):
            "filename": _clean(row.get("archive_filename")) if role == "primary" else None,
            "sha256": _clean(row.get("sha256")) if role == "primary" else None,
        })
    if not sources:
        raise ValueError(f"line {line_no} ({slug}): at least one source URL required")

    return {
        "slug": slug,
        "title": title,
        "rank": int(rank_raw),
        "version": _clean(row.get("version")),
        "license": license_,
        "recipe_status": status,
        "sources": sources,
        "screenshots": {
            "menu": _flag(row.get("screen_menu"), "screen_menu", line_no, slug),
            "gameplay": _flag(row.get("screen_gameplay"), "screen_gameplay", line_no, slug),
        },
        "notes": _clean(row.get("notes")),
    }


def load_games(csv_path: Path) -> list[dict]:
    with csv_path.open(newline="", encoding="utf-8") as fh:
        reader = csv.DictReader(fh)
        if reader.fieldnames != EXPECTED_COLUMNS:
            raise ValueError(
                f"unexpected CSV columns:\n  got:      {reader.fieldnames}\n"
                f"  expected: {EXPECTED_COLUMNS}"
            )
        games = [parse_row(row, no) for no, row in enumerate(reader, start=2)]

    slugs = [g["slug"] for g in games]
    if len(slugs) != len(set(slugs)):
        dupes = sorted({s for s in slugs if slugs.count(s) > 1})
        raise ValueError(f"duplicate slugs: {dupes}")
    ranks = [g["rank"] for g in games]
    if len(ranks) != len(set(ranks)):
        dupes = sorted({r for r in ranks if ranks.count(r) > 1})
        raise ValueError(f"duplicate ranks: {dupes}")
    return sorted(games, key=lambda g: g["rank"])


def write_json_atomic(path: Path, data) -> None:
    tmp = tempfile.NamedTemporaryFile(
        "w", dir=path.parent, suffix=".tmp", delete=False, encoding="utf-8"
    )
    try:
        json.dump(data, tmp, indent=2, ensure_ascii=False)
        tmp.write("\n")
        tmp.flush()
        os.fsync(tmp.fileno())
        tmp.close()
        os.replace(tmp.name, path)
    except BaseException:
        tmp.close()
        os.unlink(tmp.name)
        raise


def convert(csv_path: Path, out_dir: Path) -> list[str]:
    games = load_games(csv_path)
    out_dir.mkdir(parents=True, exist_ok=True)
    for game in games:
        write_json_atomic(out_dir / f"{game['slug']}.json", game)
    index = [{"rank": g["rank"], "slug": g["slug"], "title": g["title"]} for g in games]
    write_json_atomic(out_dir / "index.json", index)
    return [g["slug"] for g in games]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_path", type=Path, help="showroom games source CSV")
    parser.add_argument("out_dir", type=Path, help="output directory (assets/games)")
    args = parser.parse_args(argv)

    if not args.csv_path.is_file():
        parser.error(f"no such file: {args.csv_path}")
    slugs = convert(args.csv_path, args.out_dir)
    print(f"wrote {len(slugs)} games + index.json to {args.out_dir}")
    return 0


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    sys.exit(main())
