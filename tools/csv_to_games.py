# This file is part of the dosbox-automation-showroom Project.
# License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
#
"""Convert the showroom games source CSV into per-game asset files.

Reads the curated CSV (design doc, human-edited) and emits
assets/games/<slug>/<slug>.toml plus the legacy <slug>.json, and
assets/games/index.json with the display order.

The TOML is the single source of truth the showroom app reads. The CSV
can only supply the catalogue half of it (identity, sources, licensing);
the [dosbox], [launch] and [install] sections are filled in by hand per
game. So this is a bootstrap tool: it writes a game's files once and
then refuses to touch them again without --force.

Where each field comes from, because they do not all come from the CSV:

- identity, sources, license, rank: the CSV, which is the curated list.
- screenshots: the assets directory itself. The CSV's flag columns went
  stale during the July screenshot safari - every shot was captured but
  only one flag was ever ticked - and a file being on disk is a fact
  that cannot rot. A disagreement is logged so the CSV can be fixed.
- notes: an existing per-game JSON wins over the CSV. Those notes were
  corrected by hand with findings from the install work (bass, for one,
  records that the primary ISO has mangled filenames) and the CSV never
  received them.
"""

import argparse
import csv
import json
import logging
import os
import re
import sys
import tempfile
from pathlib import Path

logger = logging.getLogger(__name__)

KNOWN_LICENSES = {"shareware", "freeware", "demo"}
KNOWN_INSTALL_TYPES = {"unzip", "unzipinstall", "exeinstall", "floppyinstall", "isoinstall"}
KNOWN_RECIPE_STATUS = {"done", "todo"}

# The slug names a directory and is embedded in mount paths downstream,
# so it may never carry a separator, traversal or anything shell-ish.
SLUG_PATTERN = re.compile(r"^[a-z0-9][a-z0-9_-]*$")

SCREENSHOT_FILENAMES = {"title": "title.png", "gameplay": "gameplay.png"}

# Proven by the July safari: the drive rig ran all sixteen games at these
# settings. They are a starting point per game, not a measurement.
DEFAULT_DOSBOX = {
    "machine": "svga_s3",
    "cpu_cycles": 12000,
    "cpu_cycles_protected": 12000,
}
DEFAULT_SOUND = {
    "sblaster_type": "sb16",
    "mpu401": "intelligent",
    "midi_device": "fluidsynth",
}
DEFAULT_MAX_RUNTIME_SECONDS = 120

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
    if not SLUG_PATTERN.match(slug):
        raise ValueError(
            f"line {line_no}: slug {slug!r} must be lowercase alphanumeric "
            f"with - or _, starting with a letter or digit"
        )

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


_TOML_ESCAPES = {
    "\\": "\\\\",
    '"': '\\"',
    "\b": "\\b",
    "\t": "\\t",
    "\n": "\\n",
    "\f": "\\f",
    "\r": "\\r",
}


def toml_string(value: str) -> str:
    """Render a TOML basic string, escaped per the TOML 1.0 spec, section 'String'.

    Hand-rendering the document is what keeps the explanatory comments in
    it, so this escape has to be right on its own - the tests parse every
    emitted file back with tomllib to prove it.
    """
    if not isinstance(value, str):
        raise TypeError(f"expected str, got {type(value).__name__}")
    out = []
    for char in value:
        if char in _TOML_ESCAPES:
            out.append(_TOML_ESCAPES[char])
        elif char < "\x20" or char == "\x7f":
            out.append(f"\\u{ord(char):04X}")
        else:
            out.append(char)
    return '"' + "".join(out) + '"'


def _comment_block(text: str, prefix: str = "# ") -> list[str]:
    """Free text from the CSV as comment lines.

    A newline must not escape the comment, and a TOML comment may hold no
    control character but tab, nor DEL (TOML 1.0, section 'Comment') - a
    stray one makes the whole file unparseable.
    """
    lines = []
    for line in text.splitlines():
        safe = "".join(
            char if char == "\t" or (" " <= char < "\x7f") or char > "\x7f"
            else f"\\x{ord(char):02x}"
            for char in line
        )
        lines.append(f"{prefix}{safe}".rstrip())
    return lines


def screenshots_on_disk(game_dir: Path) -> dict[str, str]:
    """Which screenshots the game actually has. The file is the fact."""
    return {
        key: (filename if (game_dir / filename).is_file() else "")
        for key, filename in SCREENSHOT_FILENAMES.items()
    }


def render_toml(game: dict, screenshots: dict[str, str] | None = None) -> str:
    """Render one game definition, generated sections filled, hand sections stubbed.

    Without an explicit screenshot mapping the CSV flags are used, which
    is only right when there is no assets directory to look at yet.
    """
    if screenshots is None:
        screenshots = {
            "title": SCREENSHOT_FILENAMES["title"] if game["screenshots"]["menu"] else "",
            "gameplay": (SCREENSHOT_FILENAMES["gameplay"]
                         if game["screenshots"]["gameplay"] else ""),
        }
    lines = [
        "# Generated once from showroom-games-sources.csv by tools/csv_to_games.py.",
        "# This file is the source of truth for the game and is maintained by hand",
        "# from here on: the generator will not overwrite it without --force.",
    ]
    if game["notes"]:
        lines.append("#")
        lines.extend(_comment_block(game["notes"]))
    lines.append("")

    lines.append(f"slug = {toml_string(game['slug'])}")
    lines.append(f"title = {toml_string(game['title'])}")
    lines.append(f"rank = {game['rank']}")
    if game["version"]:
        lines.append(f"version = {toml_string(game['version'])}")
    lines.append(f"license = {toml_string(game['license'])}")
    lines.append(f"recipe_status = {toml_string(game['recipe_status'])}")

    for source in game["sources"]:
        lines.append("")
        lines.append(f"[sources.{source['role']}]")
        lines.append(f"role = {toml_string(source['role'])}")
        if source["install_type"]:
            lines.append(f"install_type = {toml_string(source['install_type'])}")
        lines.append(f"url = {toml_string(source['url'])}")
        if source.get("filename"):
            lines.append(f"filename = {toml_string(source['filename'])}")
        if source.get("sha256"):
            lines.append(f"sha256 = {toml_string(source['sha256'])}")

    lines.append("")
    lines.append("# Starting point from the drive rig, which ran every game at these")
    lines.append("# settings. Tune per game where the game needs it.")
    lines.append("[dosbox]")
    lines.append(f"machine = {toml_string(DEFAULT_DOSBOX['machine'])}")
    lines.append(f"cpu_cycles = {DEFAULT_DOSBOX['cpu_cycles']}")
    lines.append(f"cpu_cycles_protected = {DEFAULT_DOSBOX['cpu_cycles_protected']}")
    lines.append("")
    lines.append("[dosbox.sound]")
    for key, value in DEFAULT_SOUND.items():
        lines.append(f"{key} = {toml_string(value)}")

    lines.append("")
    lines.append("# HAND-FILLED. An empty executable means the game has no recipe yet;")
    lines.append("# the showroom shows the tile but refuses to launch it.")
    lines.append("[launch]")
    lines.append('executable = ""')
    lines.append('working_dir = ""')
    lines.append('setup_exe = ""')

    lines.append("")
    lines.append("[screenshots]")
    for key in SCREENSHOT_FILENAMES:
        lines.append(f"{key} = {toml_string(screenshots[key])}")

    lines.append("")
    lines.append("[install]")
    lines.append(f"max_runtime_seconds = {DEFAULT_MAX_RUNTIME_SECONDS}")
    lines.append("")
    lines.append("# HAND-FILLED. Files that must exist after a successful install,")
    lines.append("# checked before every launch. Sound config files vary - leave them out.")
    lines.append("[install.expected_files]")

    return "\n".join(lines) + "\n"


def write_text_atomic(path: Path, text: str) -> None:
    """Write via a temp file in the same directory, then rename.

    Unchanged content is left alone: rewriting a file byte for byte would
    reset its modification time and lose the record of when the data
    actually last changed.

    The mode is set explicitly: a temp file is created 0600 and that mode
    survives the rename, which leaves an asset unreadable for anyone but
    the owner once it is packaged.
    """
    if path.exists() and path.read_text(encoding="utf-8") == text:
        return

    # file has to be closed, chmod-ed and renamed while still on disk, and
    # a context manager would delete or close it at the wrong moment.
    tmp = tempfile.NamedTemporaryFile(  # noqa: SIM115
        "w", dir=path.parent, suffix=".tmp", delete=False, encoding="utf-8"
    )
    try:
        tmp.write(text)
        tmp.flush()
        os.fsync(tmp.fileno())
        tmp.close()
        os.chmod(tmp.name, 0o644)
        os.replace(tmp.name, path)
    except BaseException:
        tmp.close()
        os.unlink(tmp.name)
        raise


def write_json_atomic(path: Path, data) -> None:
    write_text_atomic(path, json.dumps(data, indent=2, ensure_ascii=False) + "\n")


def _existing_notes(json_path: Path) -> str | None:
    """The note from an already generated JSON, which may carry hand corrections."""
    if not json_path.is_file():
        return None
    try:
        with json_path.open(encoding="utf-8") as fh:
            return json.load(fh).get("notes") or None
    except (OSError, ValueError) as exc:
        logger.warning("cannot read notes from %s: %s", json_path, exc)
        return None


def _writable(path: Path, force: bool) -> bool:
    if path.exists() and not force:
        logger.info("keeping %s (use --force to overwrite)", path.name)
        return False
    return True


def convert(csv_path: Path, out_dir: Path, force: bool = False) -> list[str]:
    """Bootstrap one directory per game. Returns the slugs in display order.

    Per-game files are written once. An existing file is left alone unless
    force is set, because both the TOML and (by now) the JSON carry hand
    corrections the CSV never received.
    """
    games = load_games(csv_path)
    out_dir.mkdir(parents=True, exist_ok=True)
    for game in games:
        game_dir = out_dir / game["slug"]
        game_dir.mkdir(exist_ok=True)
        json_path = game_dir / f"{game['slug']}.json"
        toml_path = game_dir / f"{game['slug']}.toml"

        # --force means the CSV wins outright, hand corrections included.
        notes = None if force else _existing_notes(json_path)
        if notes:
            game = {**game, "notes": notes}

        screenshots = screenshots_on_disk(game_dir)
        for key, flag in (("title", game["screenshots"]["menu"]),
                          ("gameplay", game["screenshots"]["gameplay"])):
            if bool(screenshots[key]) != flag:
                logger.warning(
                    "%s: CSV says %s screenshot %s, the assets directory says %s",
                    game["slug"], key, "yes" if flag else "no",
                    "yes" if screenshots[key] else "no",
                )

        if _writable(json_path, force):
            write_json_atomic(json_path, game)
        if _writable(toml_path, force):
            write_text_atomic(toml_path, render_toml(game, screenshots))
    index = [{"rank": g["rank"], "slug": g["slug"], "title": g["title"]} for g in games]
    write_json_atomic(out_dir / "index.json", index)
    return [g["slug"] for g in games]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_path", type=Path, help="showroom games source CSV")
    parser.add_argument("out_dir", type=Path, help="output directory (assets/games)")
    parser.add_argument(
        "--force", action="store_true",
        help="overwrite existing per-game files, discarding hand edits",
    )
    args = parser.parse_args(argv)

    if not args.csv_path.is_file():
        parser.error(f"no such file: {args.csv_path}")
    slugs = convert(args.csv_path, args.out_dir, force=args.force)
    print(f"wrote {len(slugs)} games + index.json to {args.out_dir}")
    return 0


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    sys.exit(main())
