-- This file is part of the dosbox-automation-showroom Project.
-- License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
--
-- Standalone install recipe for Commander Keen 4 (Distant Markets
-- release floppy). The disk's INSTALL is only Apogee's help text; the
-- real installers are the two ARJSFX self-extractors, run from C:
-- into the same directory. Sound needs no setup step: the game
-- detects its hardware in its own menu.

dosbox.wait_frames(30)

dosbox.type("C:\n")
dosbox.wait_frames(30)
dosbox.type("A:\\K4E1-ASP.EXE\n")

if not dosbox.wait_for_text("which hard drive", 1800) then
    dosbox.abort("first extractor never asked for a drive")
end
dosbox.output["progress"] = "10"
dosbox.type("\n")

if not dosbox.wait_for_text("disk directory", 1800) then
    dosbox.abort("first extractor never asked for a directory")
end
dosbox.type("KEEN4\n")

if not dosbox.wait_for_text("MUST also install", 4200) then
    dosbox.abort("first extractor never finished")
end
dosbox.output["progress"] = "50"

-- The first run's identical prompts are still on screen; CLS keeps
-- the second run's waits from matching stale text.
dosbox.type("CLS\n")
dosbox.wait_frames(30)
dosbox.type("A:\\K4E2-ASP.EXE\n")

if not dosbox.wait_for_text("which hard drive", 1800) then
    dosbox.abort("second extractor never asked for a drive")
end
dosbox.type("\n")

if not dosbox.wait_for_text("disk directory", 1800) then
    dosbox.abort("second extractor never asked for a directory")
end
dosbox.type("KEEN4\n")
dosbox.output["progress"] = "80"

if not dosbox.wait_for_text("type \"KEEN4E\"", 4200) then
    dosbox.abort("second extractor never finished")
end

dosbox.output["progress"] = "100"
dosbox.output["install_complete"] = "yes"
