-- This file is part of the dosbox-automation-showroom Project.
-- License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
--
-- Standalone install recipe for Wacky Wheels shareware v1.1 (the 1994
-- Apogee-distributed 1wacky.zip: INSTALL.EXE + WWSW11.SHR, unzipped
-- into the extracts dir by the pipeline). The installer and the game's
-- SETUP are both text-mode. Menus save on selection and ESC backs out
-- reliably, so the exit path uses ESC instead of counting rows. The
-- game never starts (house recipe rule: the single game entry is the
-- auto-launch).

local function press(key)
    dosbox.key(key, true)
    dosbox.wait_frames(8)
    dosbox.key(key, false)
    dosbox.wait_frames(8)
end

dosbox.wait_frames(30)

dosbox.type("D:\n")
dosbox.wait_frames(30)
dosbox.type("INSTALL\n")

if not dosbox.wait_for_text("Shareware Episode", 1800) then
    dosbox.abort("installer never showed its title")
end
dosbox.output["progress"] = "10"
dosbox.type("\n")

if not dosbox.wait_for_text("Hard Drive Space Required", 1800) then
    dosbox.abort("installer never offered the install")
end
dosbox.type("\n")

if not dosbox.wait_for_text("select a drive and directory", 1800) then
    dosbox.abort("installer never asked for a destination")
end
dosbox.type("\n")
dosbox.output["progress"] = "20"

-- The .SHR unpack takes ~15 s at 12000 cycles; the timeout is headroom.
if not dosbox.wait_for_text("has been installed", 7000) then
    dosbox.abort("unpack never finished")
end
dosbox.output["progress"] = "50"
dosbox.type("\n")

if not dosbox.wait_for_text("WACKY>", 1800) then
    dosbox.abort("installer never returned to DOS")
end
dosbox.output["progress"] = "60"
dosbox.wait_frames(30)

dosbox.type("SETUP\n")

if not dosbox.wait_for_text("Exit setup program", 1800) then
    dosbox.abort("SETUP never showed its menu")
end
press("KBD_down")
press("KBD_enter")

if not dosbox.wait_for_text("Music device", 1800) then
    dosbox.abort("SETUP never showed the sound submenu")
end
press("KBD_enter")

-- Sound fx list; SoundBlaster is the first entry.
if not dosbox.wait_for_text("PC speaker", 1800) then
    dosbox.abort("SETUP never showed the sound fx list")
end
press("KBD_enter")
dosbox.wait_frames(30)
press("KBD_down")
press("KBD_enter")

-- Music list: General midi is three below SoundBlaster.
if not dosbox.wait_for_text("General midi", 1800) then
    dosbox.abort("SETUP never showed the music list")
end
for _ = 1, 3 do
    press("KBD_down")
end
press("KBD_enter")
dosbox.output["progress"] = "80"
dosbox.wait_frames(30)

-- Selections are saved immediately; ESC walks back out to DOS.
press("KBD_esc")
if not dosbox.wait_for_text("Exit setup program", 1800) then
    dosbox.abort("SETUP never returned to its main menu")
end
press("KBD_esc")

if not dosbox.wait_for_text("WACKY>", 1800) then
    dosbox.abort("SETUP never returned to DOS")
end

dosbox.output["progress"] = "100"
dosbox.output["install_complete"] = "yes"
