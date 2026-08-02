-- This file is part of the dosbox-automation-showroom Project.
-- License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
--
-- Standalone install recipe for the Warcraft II: Tides of Darkness
-- demo (Blizzard, 1996). The installer and the sound setup that
-- follows it are graphical from the first screen to the last, so
-- screen text is blank throughout and every step is timed; the DOS
-- prompt at the end is the only readable proof. Its device lists
-- open on the current value and scroll, so each is driven to its
-- clamped top edge and counted down from there.
--
-- The installer stamps the copied files with the install time. That
-- is what the 1996 original does; the disc image keeps its own dates.

local function press(key)
    dosbox.key(key, true)
    dosbox.wait_frames(6)
    dosbox.key(key, false)
    dosbox.wait_frames(30)
end

local function repeatKey(key, count)
    for _ = 1, count do
        press(key)
    end
end

-- Taller than any list in this installer, which clamps rather than
-- wraps, so the highlight ends on the first entry whatever it was on.
local function pickFromList(index)
    repeatKey("KBD_up", 16)
    repeatKey("KBD_down", index)
    -- Arrow keys move focus into the list; the buttons follow it.
    press("KBD_tab")
    press("KBD_tab")
    press("KBD_enter")
end

local function seconds(count)
    for _ = 1, count do
        dosbox.wait_frames(70)
    end
end

dosbox.wait_frames(60)
dosbox.type("d:\n")
dosbox.wait_frames(30)
dosbox.type("INSTALL\n")
dosbox.output["progress"] = "5"

seconds(8)
press("KBD_enter")
seconds(4)
-- C:\WAR2 is offered and is where the launch config expects the game.
press("KBD_enter")
dosbox.output["progress"] = "20"

-- The copy runs about 30 s at 12000 cycles; the success dialog then
-- waits for a keypress, so overshooting costs nothing.
seconds(80)
press("KBD_enter")
dosbox.output["progress"] = "55"

seconds(6)
pickFromList(3)
seconds(4)
press("KBD_enter")
seconds(4)
press("KBD_enter")
seconds(8)
press("KBD_enter")
dosbox.output["progress"] = "75"

seconds(6)
-- General MIDI drives the engine's MPU-401; the rest of the list is
-- FM synthesis.
pickFromList(1)
seconds(4)
press("KBD_enter")
seconds(4)
press("KBD_enter")
seconds(8)
press("KBD_enter")
dosbox.output["progress"] = "90"

-- The readme viewer is the last screen before setup exits to DOS.
seconds(6)
press("KBD_enter")

for _ = 1, 6 do
    if dosbox.wait_for_text("C:\\WAR2>", 1500) then
        dosbox.output["progress"] = "100"
        dosbox.output["install_complete"] = "yes"
        return
    end
end
dosbox.abort("never saw the DOS prompt after setup")
