-- This file is part of the dosbox-automation-showroom Project.
-- License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
--
-- Standalone install recipe for Epic Pinball: Super Android (shareware
-- v2.1, Epic CD self-extractor EPICPN21.EXE). The extractor is a DOS
-- program: it runs on D: (extracts dir) and installs to C: (staging).
-- Sound is configured via the shipped SETUP.EXE afterwards, so the
-- install never enters the game (house recipe rule: the game starts
-- exactly once, at the showroom's auto-launch).

dosbox.wait_frames(30)

dosbox.type("D:\n")
dosbox.wait_frames(30)
dosbox.type("EPICPN21.EXE\n")

if not dosbox.wait_for_text("Which program do you want to install", 1800) then
    dosbox.abort("extractor never showed its install menu")
end
dosbox.output["progress"] = "10"
dosbox.type("\n")

if not dosbox.wait_for_text("Drive to install to", 1800) then
    dosbox.abort("extractor never asked for a drive")
end
dosbox.type("\n")

if not dosbox.wait_for_text("Directory to install to", 1800) then
    dosbox.abort("extractor never asked for a directory")
end
dosbox.type("\n")

if not dosbox.wait_for_text("This directory does not exist", 1800) then
    dosbox.abort("extractor never asked to create the directory")
end
dosbox.type("Y")
dosbox.output["progress"] = "20"

-- Extraction takes seconds at 12000 cycles; the timeout is headroom.
if not dosbox.wait_for_text("is now installed", 15000) then
    dosbox.abort("extraction never finished")
end
dosbox.output["progress"] = "50"

if not dosbox.wait_for_text("read the instructions", 1800) then
    dosbox.abort("extractor never offered the instructions")
end
dosbox.type("N")

-- N returns to the main menu; EXIT is the second entry.
if not dosbox.wait_for_text("Which program do you want to install", 1800) then
    dosbox.abort("extractor never returned to its menu")
end
dosbox.key("KBD_down", true)
dosbox.wait_frames(8)
dosbox.key("KBD_down", false)
dosbox.wait_frames(8)
dosbox.type("\n")

if not dosbox.wait_for_text("Thank you", 1800) then
    dosbox.abort("extractor never exited")
end
dosbox.output["progress"] = "60"
dosbox.wait_frames(60)

dosbox.type("C:\n")
dosbox.wait_frames(30)
dosbox.type("CD \\EPICPIN\n")
dosbox.wait_frames(30)
dosbox.type("SETUP\n")

if not dosbox.wait_for_text("Select Sound Card", 1800) then
    dosbox.abort("SETUP never showed its menu")
end
dosbox.type("\n")

-- Card list: No Sound Card / PC Speaker / GUS / PAS / SB / SB16 / ...
if not dosbox.wait_for_text("Select this if you do not have", 1800) then
    dosbox.abort("SETUP never showed the sound card list")
end
for _ = 1, 5 do
    dosbox.key("KBD_down", true)
    dosbox.wait_frames(8)
    dosbox.key("KBD_down", false)
    dosbox.wait_frames(8)
end
dosbox.type("\n")

-- SETUP reads the BLASTER variable and asks to keep it.
if not dosbox.wait_for_text("Accept them", 1800) then
    dosbox.abort("SETUP never offered the BLASTER settings")
end
dosbox.type("\n")

-- Quality: one down from Ultra (Pentium) to Very High (486-50),
-- matching the pinned 12000 cycles.
if not dosbox.wait_for_text("Select Playback Quality", 1800) then
    dosbox.abort("SETUP never asked for playback quality")
end
dosbox.key("KBD_down", true)
dosbox.wait_frames(8)
dosbox.key("KBD_down", false)
dosbox.wait_frames(8)
dosbox.type("\n")
dosbox.output["progress"] = "80"

-- Back at the SETUP main menu: Exit and Save is two below Select
-- Sound Card.
if not dosbox.wait_for_text("Exit and Save", 1800) then
    dosbox.abort("SETUP never returned to its menu")
end
for _ = 1, 2 do
    dosbox.key("KBD_down", true)
    dosbox.wait_frames(8)
    dosbox.key("KBD_down", false)
    dosbox.wait_frames(8)
end
dosbox.type("\n")

-- SETUP exits to DOS; the prompt is proof.
if not dosbox.wait_for_text("EPICPIN>", 1800) then
    dosbox.abort("SETUP never returned to DOS")
end

dosbox.output["progress"] = "100"
dosbox.output["install_complete"] = "yes"
