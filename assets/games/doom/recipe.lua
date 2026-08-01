-- This file is part of the dosbox-automation-showroom Project.
-- License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
--
-- Standalone install recipe for DOOM shareware (v1.666, 2 floppies).
-- Disk swap is in-script via dosbox.drive_swap, no external coordinator.
-- Prompt sequence per the engine repo's e2e manifest
-- (tests/files/disks/doom-shareware).

dosbox.wait_frames(30)

dosbox.type("A:\n")
dosbox.wait_frames(30)
dosbox.type("SETUP.EXE\n")
dosbox.wait_frames(60)

if not dosbox.wait_for_text("Available Drives", 1800) then
    dosbox.abort("installer never showed the drive selection")
end
dosbox.output["progress"] = "10"
dosbox.type("C\n")
dosbox.wait_frames(30)

if not dosbox.wait_for_text("insert disk DOOM 2", 1800) then
    dosbox.abort("installer never asked for disk 2")
end
-- The image name is the pinned source archive's own entry name.
dosbox.drive_swap("A", "Doom Shareware Floppy (v1.666) [Disk 2 of 2].ima")
dosbox.output["progress"] = "50"
dosbox.key("KBD_enter", true)
dosbox.wait_frames(8)
dosbox.key("KBD_enter", false)
dosbox.wait_frames(8)

if not dosbox.wait_for_text("Controller Type", 1800) then
    dosbox.abort("installer never reached the setup wizard")
end
dosbox.key("KBD_down", true)
dosbox.wait_frames(8)
dosbox.key("KBD_down", false)
dosbox.wait_frames(8)

-- Menu count varies between installer versions; terminate on the mode
-- switch, not a count.
while dosbox.is_text_mode() do
    local prev = dosbox.screen_text()
    dosbox.key("KBD_enter", true)
    dosbox.wait_frames(8)
    dosbox.key("KBD_enter", false)
    dosbox.wait_frames(8)
    for _ = 1, 20 do
        dosbox.wait_frames(10)
        if not dosbox.is_text_mode() or dosbox.screen_text() ~= prev then
            break
        end
    end
end

-- The game is running its attract screen: leave to DOS via the quit
-- menu (ESC, up to Quit, confirm).
dosbox.wait_frames(60)
dosbox.key("KBD_esc", true)
dosbox.wait_frames(8)
dosbox.key("KBD_esc", false)
dosbox.wait_frames(15)
dosbox.key("KBD_up", true)
dosbox.wait_frames(8)
dosbox.key("KBD_up", false)
dosbox.wait_frames(15)
dosbox.key("KBD_enter", true)
dosbox.wait_frames(8)
dosbox.key("KBD_enter", false)
dosbox.wait_frames(15)
dosbox.key("KBD_y", true)
dosbox.wait_frames(8)
dosbox.key("KBD_y", false)

while not dosbox.is_text_mode() do
    dosbox.wait_frames(30)
end
dosbox.output["progress"] = "90"

dosbox.output["progress"] = "100"
dosbox.output["install_complete"] = "yes"
