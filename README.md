# dosbox-automation showroom

![GPL-3.0-or-later][gpl-badge]

Sixteen DOS games, one click each, from nothing installed to playing.

Pick a game from the grid. The showroom fetches it, runs the original DOS
installer in front of you, answers the installer's questions itself, and
starts the game. No manual mounting, no setup wizards, no reading a README
from 1994 to find out which sound card option works.

## The games

DOOM, Duke Nukem 3D, Heretic, Commander Keen 4, Jazz Jackrabbit, Rayman,
Epic Pinball, Tyrian, Wacky Wheels, One Must Fall 2097, Warcraft 2,
Transport Tycoon, Dungeon Hack, God of Thunder, Beneath a Steel Sky, and
Flight of the Amazon Queen.

All of them are shareware, freeware, or otherwise freely distributable. The
showroom does not ship or download anything commercial.

## Why it exists

DOS games install themselves fine. Watching that happen is the point.
The showroom is a face for [dosbox-automation][engine], which can drive a
DOS installer through its own prompts over an HTTP API. That capability is
invisible in a terminal, so this puts it on screen: you click, and an
installer from thirty years ago walks through its own menus while you watch.

## Status

Early development. Nothing is released yet and there is nothing to download.

## License

GPL-3.0-or-later. The bundled dosbox-automation engine keeps its own
license, GPL-2.0-or-later. Third-party components are listed in
[THIRD_PARTY_LICENSES.txt](THIRD_PARTY_LICENSES.txt).

Every game keeps its own terms. The showroom redistributes only what its
authors and publishers released for free redistribution.

[engine]: https://www.dosbox-automation.org/
[gpl-badge]: https://img.shields.io/badge/license-GPL--3.0--or--later-blue

---
This project is developed with tooled assistance, but tested, reviewed and
signed off by a human developer.
