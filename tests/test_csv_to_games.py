# This file is part of the dosbox-automation-showroom Project.
# License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
#
import json
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
from csv_to_games import EXPECTED_COLUMNS, convert, load_games, parse_row

HEADER = ",".join(EXPECTED_COLUMNS)


def make_row(**overrides) -> dict:
    row = {
        "rank": "1", "game": "DOOM", "version": "1.666", "slug": "doom",
        "license": "shareware", "recipe_status": "done",
        "type1": "floppyinstall", "type2": "isoinstall",
        "source_url": "https://example.org/doom.7z",
        "mirror_url": "https://example.org/mirror.iso",
        "archive_filename": "", "screen_menu": "", "screen_gameplay": "",
        "sha256": "", "notes": "gold release",
    }
    row.update(overrides)
    return row


def csv_text(rows: list[dict]) -> str:
    lines = [HEADER]
    for row in rows:
        lines.append(",".join(f'"{row[c]}"' for c in EXPECTED_COLUMNS))
    return "\n".join(lines) + "\n"


def test_parse_row_shapes_sources():
    game = parse_row(make_row(), 2)
    assert game["slug"] == "doom"
    assert game["rank"] == 1
    assert [s["role"] for s in game["sources"]] == ["primary", "mirror"]
    assert game["sources"][0]["install_type"] == "floppyinstall"
    assert game["sources"][1]["url"] == "https://example.org/mirror.iso"
    assert game["screenshots"] == {"menu": False, "gameplay": False}


def test_screenshot_flags():
    game = parse_row(make_row(screen_menu="x", screen_gameplay="X"), 2)
    assert game["screenshots"] == {"menu": True, "gameplay": True}
    with pytest.raises(ValueError, match="must be 'x' or empty"):
        parse_row(make_row(screen_menu="yes"), 2)


def test_dash_placeholder_means_none():
    game = parse_row(make_row(archive_filename="-", version="-"), 2)
    assert game["sources"][0]["filename"] is None
    assert game["version"] is None


def test_mirror_optional():
    game = parse_row(make_row(mirror_url="", type2=""), 2)
    assert [s["role"] for s in game["sources"]] == ["primary"]


@pytest.mark.parametrize("field,value,match", [
    ("license", "warez", "unknown license"),
    ("recipe_status", "maybe", "unknown recipe_status"),
    ("type1", "telepathy", "unknown install type"),
    ("slug", "", "slug and game are required"),
    ("rank", "first", "rank must be a positive integer"),
])
def test_bad_values_fail_loudly(field, value, match):
    with pytest.raises(ValueError, match=match):
        parse_row(make_row(**{field: value}), 2)


def test_no_sources_fails():
    with pytest.raises(ValueError, match="at least one source"):
        parse_row(make_row(source_url="", mirror_url=""), 2)


def test_duplicate_slug_rejected(tmp_path):
    rows = [make_row(), make_row(rank="2")]
    csv_path = tmp_path / "games.csv"
    csv_path.write_text(csv_text(rows), encoding="utf-8")
    with pytest.raises(ValueError, match="duplicate slugs"):
        load_games(csv_path)


def test_unexpected_columns_rejected(tmp_path):
    csv_path = tmp_path / "games.csv"
    csv_path.write_text("rank,game\n1,DOOM\n", encoding="utf-8")
    with pytest.raises(ValueError, match="unexpected CSV columns"):
        load_games(csv_path)


def test_convert_writes_per_game_and_index(tmp_path):
    rows = [
        make_row(rank="2", game="Tyrian", slug="tyrian", license="freeware",
                 recipe_status="todo", type1="exeinstall", type2="unzip"),
        make_row(),
    ]
    csv_path = tmp_path / "games.csv"
    csv_path.write_text(csv_text(rows), encoding="utf-8")
    out_dir = tmp_path / "assets" / "games"

    slugs = convert(csv_path, out_dir)

    assert slugs == ["doom", "tyrian"]
    index = json.loads((out_dir / "index.json").read_text(encoding="utf-8"))
    assert [e["slug"] for e in index] == ["doom", "tyrian"]
    doom = json.loads((out_dir / "doom.json").read_text(encoding="utf-8"))
    assert doom["title"] == "DOOM"
    assert doom["sources"][0]["role"] == "primary"
