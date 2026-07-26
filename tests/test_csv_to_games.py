# This file is part of the dosbox-automation-showroom Project.
# License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
#
import json
import os
import stat
import sys
import tomllib
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
from csv_to_games import (
    EXPECTED_COLUMNS,
    convert,
    load_games,
    parse_row,
    render_toml,
    toml_string,
)

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


@pytest.mark.parametrize("slug", [
    "../etc", "a/b", "a\\b", "..", "Doom", "-lead", "with space", "nul\x00byte",
])
def test_hostile_slug_rejected(slug):
    """The slug names a directory and ends up in mount paths - it must stay tame."""
    with pytest.raises(ValueError, match="must be lowercase alphanumeric"):
        parse_row(make_row(slug=slug), 2)


def write_csv(tmp_path, rows) -> Path:
    csv_path = tmp_path / "games.csv"
    csv_path.write_text(csv_text(rows), encoding="utf-8")
    return csv_path


def test_convert_writes_per_game_directory_and_index(tmp_path):
    rows = [
        make_row(rank="2", game="Tyrian", slug="tyrian", license="freeware",
                 recipe_status="todo", type1="exeinstall", type2="unzip"),
        make_row(),
    ]
    out_dir = tmp_path / "assets" / "games"

    slugs = convert(write_csv(tmp_path, rows), out_dir)

    assert slugs == ["doom", "tyrian"]
    index = json.loads((out_dir / "index.json").read_text(encoding="utf-8"))
    assert [e["slug"] for e in index] == ["doom", "tyrian"]
    # Each game owns a directory: the JSON, the TOML and the screenshots
    # live together, which is the layout the assets tree already has.
    doom = json.loads((out_dir / "doom" / "doom.json").read_text(encoding="utf-8"))
    assert doom["title"] == "DOOM"
    assert doom["sources"][0]["role"] == "primary"
    assert (out_dir / "doom" / "doom.toml").is_file()
    assert (out_dir / "tyrian" / "tyrian.toml").is_file()


def place_screenshots(out_dir: Path, slug: str, *names: str) -> None:
    (out_dir / slug).mkdir(parents=True, exist_ok=True)
    for name in names:
        (out_dir / slug / name).write_bytes(b"\x89PNG\r\n\x1a\n")


def test_emitted_toml_carries_every_section(tmp_path):
    out_dir = tmp_path / "games"
    place_screenshots(out_dir, "doom", "title.png", "gameplay.png")
    convert(write_csv(tmp_path, [make_row(screen_menu="x", screen_gameplay="x")]), out_dir)

    with (out_dir / "doom" / "doom.toml").open("rb") as fh:
        doom = tomllib.load(fh)

    assert doom["slug"] == "doom"
    assert doom["title"] == "DOOM"
    assert doom["rank"] == 1
    assert doom["version"] == "1.666"
    assert doom["license"] == "shareware"
    assert doom["recipe_status"] == "done"
    assert doom["sources"]["primary"]["url"] == "https://example.org/doom.7z"
    assert doom["sources"]["primary"]["install_type"] == "floppyinstall"
    assert doom["sources"]["mirror"]["url"] == "https://example.org/mirror.iso"
    assert doom["dosbox"]["machine"] == "svga_s3"
    assert doom["dosbox"]["cpu_cycles"] == 12000
    assert doom["dosbox"]["cpu_cycles_protected"] == 12000
    assert doom["dosbox"]["sound"]["sblaster_type"] == "sb16"
    assert doom["launch"] == {"executable": "", "working_dir": "", "setup_exe": ""}
    assert doom["screenshots"] == {"title": "title.png", "gameplay": "gameplay.png"}
    assert doom["install"]["max_runtime_seconds"] == 120
    assert doom["install"]["expected_files"] == {}


def test_missing_screenshot_emits_empty_filename(tmp_path):
    out_dir = tmp_path / "games"
    place_screenshots(out_dir, "doom", "title.png")
    convert(write_csv(tmp_path, [make_row(screen_menu="x")]), out_dir)

    with (out_dir / "doom" / "doom.toml").open("rb") as fh:
        doom = tomllib.load(fh)
    assert doom["screenshots"] == {"title": "title.png", "gameplay": ""}


def test_missing_source_url_raises_before_anything_is_written(tmp_path):
    """A half-written definition is worse than none: nothing may land on disk."""
    rows = [make_row(), make_row(rank="2", slug="tyrian", source_url="", mirror_url="")]
    out_dir = tmp_path / "games"

    with pytest.raises(ValueError, match="at least one source"):
        convert(write_csv(tmp_path, rows), out_dir)

    assert not out_dir.exists() or list(out_dir.iterdir()) == []


def test_existing_toml_survives_regeneration(tmp_path):
    """The hand-filled sections are the whole point - regeneration must not eat them."""
    out_dir = tmp_path / "games"
    csv_path = write_csv(tmp_path, [make_row()])
    convert(csv_path, out_dir)

    toml_path = out_dir / "doom" / "doom.toml"
    hand_edited = toml_path.read_text(encoding="utf-8").replace(
        'executable = ""', 'executable = "DOOM.EXE"'
    )
    toml_path.write_text(hand_edited, encoding="utf-8")

    convert(csv_path, out_dir)
    with toml_path.open("rb") as fh:
        assert tomllib.load(fh)["launch"]["executable"] == "DOOM.EXE"

    convert(csv_path, out_dir, force=True)
    with toml_path.open("rb") as fh:
        assert tomllib.load(fh)["launch"]["executable"] == ""


def test_existing_json_survives_regeneration(tmp_path):
    """The JSON carries hand corrections from the install work too."""
    out_dir = tmp_path / "games"
    csv_path = write_csv(tmp_path, [make_row()])
    convert(csv_path, out_dir)

    json_path = out_dir / "doom" / "doom.json"
    hand_edited = json.loads(json_path.read_text(encoding="utf-8"))
    hand_edited["notes"] = "primary ISO has mangled filenames, use the mirror"
    json_path.write_text(json.dumps(hand_edited, indent=2) + "\n", encoding="utf-8")

    convert(csv_path, out_dir)
    kept = json.loads(json_path.read_text(encoding="utf-8"))
    assert kept["notes"] == "primary ISO has mangled filenames, use the mirror"

    convert(csv_path, out_dir, force=True)
    forced = json.loads(json_path.read_text(encoding="utf-8"))
    assert forced["notes"] == "gold release"


def test_screenshots_come_from_disk_not_the_csv(tmp_path):
    """The CSV flags went stale during the safari; a file on disk cannot rot."""
    out_dir = tmp_path / "games"
    (out_dir / "doom").mkdir(parents=True)
    (out_dir / "doom" / "title.png").write_bytes(b"\x89PNG")
    (out_dir / "doom" / "gameplay.png").write_bytes(b"\x89PNG")

    # Both CSV flags empty, both files present: the files win.
    convert(write_csv(tmp_path, [make_row()]), out_dir)

    with (out_dir / "doom" / "doom.toml").open("rb") as fh:
        assert tomllib.load(fh)["screenshots"] == {
            "title": "title.png", "gameplay": "gameplay.png",
        }


def test_screenshot_disagreement_is_reported(tmp_path, caplog):
    out_dir = tmp_path / "games"
    (out_dir / "doom").mkdir(parents=True)
    (out_dir / "doom" / "title.png").write_bytes(b"\x89PNG")

    with caplog.at_level("WARNING"):
        convert(write_csv(tmp_path, [make_row(screen_gameplay="x")]), out_dir)

    messages = [r.getMessage() for r in caplog.records]
    assert any("title screenshot no" in m for m in messages), messages
    assert any("gameplay screenshot yes" in m for m in messages), messages


def test_hand_corrected_notes_reach_the_toml(tmp_path):
    out_dir = tmp_path / "games"
    csv_path = write_csv(tmp_path, [make_row()])
    convert(csv_path, out_dir)

    json_path = out_dir / "doom" / "doom.json"
    data = json.loads(json_path.read_text(encoding="utf-8"))
    data["notes"] = "the finding the CSV never got"
    json_path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    (out_dir / "doom" / "doom.toml").unlink()

    convert(csv_path, out_dir)
    assert "the finding the CSV never got" in (
        out_dir / "doom" / "doom.toml").read_text(encoding="utf-8")


def test_written_files_are_world_readable(tmp_path):
    """A temp file is created 0600 and the mode survives the rename into an asset."""
    out_dir = tmp_path / "games"
    convert(write_csv(tmp_path, [make_row()]), out_dir)

    for path in (out_dir / "index.json",
                 out_dir / "doom" / "doom.json",
                 out_dir / "doom" / "doom.toml"):
        mode = stat.S_IMODE(path.stat().st_mode)
        assert mode == 0o644, f"{path.name} has mode {mode:o}"


@pytest.mark.parametrize("hostile", [
    'quote " inside',
    "backslash \\ inside",
    "newline \n inside",
    "tab \t inside",
    "control \x01 byte",
    "delete \x7f byte",
    "unicode zero width ​ here",
    'toml injection" \nrank = 99 #',
])
def test_hostile_strings_survive_the_round_trip(hostile):
    """Hand-rendered TOML means the escaping is ours to get right."""
    game = parse_row(make_row(game=hostile, notes=hostile), 2)
    parsed = tomllib.loads(render_toml(game))

    assert parsed["title"] == hostile
    assert parsed["rank"] == 1, "a string escaped its quotes and injected a key"


def test_unchanged_content_keeps_its_timestamp(tmp_path):
    """Byte-identical output must not be rewritten: it would reset the mtime.

    Forced, so this exercises the content comparison rather than the
    leave-existing-files-alone guard.
    """
    out_dir = tmp_path / "games"
    csv_path = write_csv(tmp_path, [make_row()])
    convert(csv_path, out_dir)

    tracked = [out_dir / "index.json", out_dir / "doom" / "doom.json",
               out_dir / "doom" / "doom.toml"]
    for path in tracked:
        os.utime(path, ns=(0, 0))  # a distinctive old stamp to prove it survives

    convert(csv_path, out_dir, force=True)

    for path in tracked:
        assert path.stat().st_mtime_ns == 0, f"{path.name} was rewritten unchanged"


def test_changed_content_is_written(tmp_path):
    out_dir = tmp_path / "games"
    convert(write_csv(tmp_path, [make_row()]), out_dir)
    convert(write_csv(tmp_path, [make_row(game="DOOM II")]), out_dir, force=True)

    doom = json.loads((out_dir / "doom" / "doom.json").read_text(encoding="utf-8"))
    assert doom["title"] == "DOOM II"


def test_toml_string_rejects_non_strings():
    with pytest.raises(TypeError, match="expected str"):
        toml_string(12000)
