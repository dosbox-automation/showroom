-- This file is part of the dosbox-automation-showroom Project.
-- License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
--
-- Standalone install recipe for Flight of the Amazon Queen (freeware
-- full game, six 1.44M floppies). The images extract into a
-- subdirectory named after the archive; swaps name that relative
-- path, resolved against the conf anchor. The installer is text-mode
-- throughout and ends at DOS by itself. Sound setup is not driven
-- here: the game's SETUP is mouse-only (aug-72sq) - the launch path
-- (AQ.BAT) offers it to the player on first start instead.

dosbox.wait_frames(30)

dosbox.type("A:\n")
dosbox.wait_frames(30)
dosbox.type("INSTALL\n")

if not dosbox.wait_for_text("accept default", 1800) then
    dosbox.abort("installer never asked for the install path")
end
dosbox.output["progress"] = "10"
dosbox.type("\n")

-- One prompt per following disk; each disk decompresses ~15-20 s at
-- 12000 cycles.
local disks = { 2, 3, 4, 5, 6 }
for i, disk in ipairs(disks) do
    if not dosbox.wait_for_text("Insert DISK  " .. disk, 4200) then
        dosbox.abort("installer never asked for disk " .. disk)
    end
    dosbox.drive_swap("A", "001262_flight_of_the_amazon_queen/disk" .. disk
                                   .. ".img")
    dosbox.output["progress"] = tostring(10 + i * 15)
    dosbox.key("KBD_enter", true)
    dosbox.wait_frames(8)
    dosbox.key("KBD_enter", false)
    dosbox.wait_frames(8)
end

-- The last disk carries the bulk of QUEEN.1; its decompression runs
-- ~3 minutes at 12000 cycles. Waited in re-armed sub-30 s slices:
-- the engine's wall ceiling kills any single longer wait even while
-- frames advance (aug-xdae).
local finished = false
for _ = 1, 12 do
    if dosbox.wait_for_text("Installation complete", 1800) then
        finished = true
        break
    end
end
if not finished then
    dosbox.abort("installer never finished")
end

dosbox.output["progress"] = "100"
dosbox.output["install_complete"] = "yes"
