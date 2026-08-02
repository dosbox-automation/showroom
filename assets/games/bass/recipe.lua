-- This file is part of the dosbox-automation-showroom Project.
-- License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
--
-- Standalone install recipe for Beneath a Steel Sky (Virgin 1995 CD,
-- freeware since 2003). The game itself stays on the disc - the
-- install writes only a launcher and a sound config, about 0.1 MB -
-- so the CD has to remain mounted to play, which the run config
-- does.
--
-- This installer polls the keyboard slowly enough to drop most of a
-- burst, so menu moves are not counted: it marks the selected entry
-- with CP437 arrows (0x10 before, 0x11 after), and every move is
-- repeated until the screen shows the wanted entry marked.
--
-- Sound stays at the offered Sound Blaster / Ad lib. The only other
-- device on the menu is Roland, meaning MT-32, whose instruments do
-- not match the General MIDI soundfont the engine plays through.

local kSelected = "\16"

local function press(key)
    dosbox.key(key, true)
    dosbox.wait_frames(6)
    dosbox.key(key, false)
    dosbox.wait_frames(40)
end

local function isSelected(label)
    return dosbox.screen_text():find(kSelected .. "[^\n]*" .. label) ~= nil
end

local function moveOnto(label, key)
    for _ = 1, 12 do
        if isSelected(label) then
            return
        end
        press(key)
    end
    dosbox.abort("could not select " .. label)
end

local function waitFor(text, slices, what)
    for _ = 1, slices do
        if dosbox.wait_for_text(text, 1500) then
            return
        end
    end
    dosbox.abort("never saw " .. what)
end

dosbox.wait_frames(60)
dosbox.type("d:\n")
dosbox.wait_frames(30)
dosbox.type("INSTALL\n")
dosbox.output["progress"] = "10"

waitFor("Path Selection Window", 6, "the installer path window")
-- C:\SKY is offered and is where the launch config expects SKY.BAT.
press("KBD_enter")
dosbox.output["progress"] = "40"

waitFor("Setup Menu", 6, "the setup menu")
-- The menu does not wrap, so it is walked to its top edge before
-- walking down to the wanted entry.
moveOnto("Language", "KBD_up")
moveOnto("Exit Install", "KBD_down")
press("KBD_enter")
dosbox.output["progress"] = "70"

waitFor("Save Setup", 6, "the exit menu")
moveOnto("Save Setup", "KBD_up")
press("KBD_enter")
waitFor("C:\\SKY>", 6, "the DOS prompt after setup")

dosbox.output["progress"] = "100"
dosbox.output["install_complete"] = "yes"
