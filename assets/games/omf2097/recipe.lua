-- This file is part of the dosbox-automation-showroom Project.
-- License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
--
-- Standalone install recipe for One Must Fall 2097 (shareware v2.1,
-- Epic CD self-extractor OMFSW21.EXE). Same extractor family as Epic
-- Pinball: it runs on D: (extracts dir) and installs to C: (staging).
-- OMF's SETUP.EXE is a graphics-mode UI, so that stretch is driven
-- blind by key sequence with the mode switch as the terminator; its
-- menus are fixed and the quality preselect is speed-detected, which
-- pinned cycles make deterministic. The game itself never starts
-- (house recipe rule: the single game entry is the auto-launch).

local function press(key)
    dosbox.key(key, true)
    dosbox.wait_frames(8)
    dosbox.key(key, false)
    dosbox.wait_frames(8)
end

dosbox.wait_frames(30)

dosbox.type("D:\n")
dosbox.wait_frames(30)
dosbox.type("OMFSW21.EXE\n")

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

if not dosbox.wait_for_text("is now installed", 7000) then
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
press("KBD_down")
dosbox.type("\n")

if not dosbox.wait_for_text("Thank you", 1800) then
    dosbox.abort("extractor never exited")
end
dosbox.output["progress"] = "60"
dosbox.wait_frames(60)

dosbox.type("C:\n")
dosbox.wait_frames(30)
dosbox.type("CD \\OMF\n")
dosbox.wait_frames(30)
dosbox.type("SETUP\n")

-- SETUP switches to its graphics UI; from here the driving is blind.
local entered = false
for _ = 1, 60 do
    dosbox.wait_frames(10)
    if not dosbox.is_text_mode() then
        entered = true
        break
    end
end
if not entered then
    dosbox.abort("SETUP never entered its graphics UI")
end
dosbox.wait_frames(120)

-- Sound card list: SOUND BLASTER 16 is four below NO SOUND CARD.
for _ = 1, 4 do
    press("KBD_down")
end
press("KBD_enter")
dosbox.wait_frames(60)

-- BLASTER settings dialog, YES preselected.
press("KBD_enter")
dosbox.wait_frames(60)

-- Playback quality: accept the speed-detected preselection.
press("KBD_enter")
dosbox.wait_frames(60)
dosbox.output["progress"] = "80"

-- Setup main menu: SAVE AND EXIT is three below SELECT SOUND CARD.
for _ = 1, 3 do
    press("KBD_down")
end
press("KBD_enter")

-- SAVE AND EXIT leaves to DOS; the mode switch is the proof.
local left = false
for _ = 1, 60 do
    dosbox.wait_frames(10)
    if dosbox.is_text_mode() then
        left = true
        break
    end
end
if not left then
    dosbox.abort("SETUP never returned to DOS")
end

dosbox.output["progress"] = "100"
dosbox.output["install_complete"] = "yes"
