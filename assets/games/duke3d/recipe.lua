-- This file is part of the dosbox-automation-showroom Project.
-- License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
--
-- Standalone install recipe for Duke Nukem 3D Shareware v1.1 (3D
-- Realms CD). The CD installer runs in mode 13h, where screen text
-- reads empty - that stretch is driven by counted keystrokes with
-- generous waits (the copy itself measured ~7 s), and the return of
-- the text-mode DOS prompt is the sync point. SETUP is a text UI and
-- is verified screen by screen; menu positions are counted from the
-- fresh-config defaults the installer just wrote.

local function press(key)
    dosbox.key(key, true)
    dosbox.wait_frames(6)
    dosbox.key(key, false)
    dosbox.wait_frames(20)
end

local function downs(count)
    for _ = 1, count do
        press("KBD_down")
    end
end

local function expect(text, frames, what)
    if not dosbox.wait_for_text(text, frames) then
        dosbox.abort("never saw " .. what)
    end
end

dosbox.wait_frames(60)
dosbox.type("d:\n")
dosbox.wait_frames(30)
dosbox.type("INSTALL\n")
dosbox.output["progress"] = "5"

-- Blind graphical stretch: welcome, title pick, C:\DUKE3D default,
-- create-directory confirmation.
dosbox.wait_frames(700)
press("KBD_enter")
dosbox.wait_frames(140)
press("KBD_enter")
dosbox.wait_frames(140)
press("KBD_enter")
dosbox.wait_frames(140)
dosbox.type("Y")
dosbox.output["progress"] = "20"

-- The copy finishes in seconds; wait far past it, then decline the
-- play offer and leave via the exit confirmation.
for _ = 1, 4 do
    dosbox.wait_frames(700)
end
dosbox.type("N")
dosbox.wait_frames(140)
press("KBD_esc")
dosbox.wait_frames(140)
dosbox.type("Y")
-- The installer exits leaving mode 13h, where the console is
-- invisible to screen text, and parks the prompt in C:\DUKE3D; MODE
-- CO80 restores text mode and makes that prompt the sync point.
dosbox.wait_frames(300)
dosbox.type("MODE CO80\n")
expect("C:\\DUKE3D>", 900, "the DOS prompt after the installer")
dosbox.output["progress"] = "50"

dosbox.type("SETUP\n")
expect("Main Menu", 900, "the SETUP main menu")

-- Sound Setup sits on top, already highlighted.
press("KBD_enter")
expect("Current Sound FX Card", 300, "the sound setup menu")
press("KBD_enter")
expect("Gravis Ultrasound", 300, "the FX card list")
downs(2)
press("KBD_enter")
expect("Sound Blaster Configuration", 300, "the SB config screen")
-- Autodetected SB16 at 0x220, IRQ 7, DMA 1/5; take it as offered.
press("KBD_enter")
expect("Number of Voices", 300, "the voices list")
downs(4)
press("KBD_enter")
expect("Number of Mixing Bits", 300, "the mixing bits list")
downs(1)
press("KBD_enter")
expect("Number of Channels", 300, "the channels list")
downs(1)
press("KBD_enter")
expect("Mixing Rate", 300, "the mixing rate list")
downs(3)
press("KBD_enter")
expect("( Sound Blaster )", 300, "the FX card taking effect")
dosbox.output["progress"] = "75"

downs(1)
press("KBD_enter")
expect("Wave Blaster", 300, "the music card list")
downs(8)
press("KBD_enter")
expect("MIDI Port", 300, "the MIDI port list")
-- 0x330 is pre-highlighted; it matches the engine's MPU-401.
press("KBD_enter")
expect("( General Midi )", 300, "the music card taking effect")

-- Both menus stay painted behind their overlays, so there is no
-- unique text to gate the first Esc on; the save prompt gates both.
press("KBD_esc")
dosbox.wait_frames(60)
press("KBD_esc")
expect("Save Settings", 300, "the save prompt")
press("KBD_enter")
expect("C:\\DUKE3D>", 900, "the DOS prompt after SETUP")

dosbox.output["progress"] = "100"
dosbox.output["install_complete"] = "yes"
