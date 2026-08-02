-- This file is part of the dosbox-automation-showroom Project.
-- License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
--
-- Standalone install recipe for Heretic shareware v1.0 (Raven/id).
-- The disc is a two-game shareware sampler; only \HERETIC is used.
-- INSTALL.BAT runs De-ICE and then hands straight over to SETUP's
-- first-run wizard, which opens each list on the CURRENT value rather
-- than the top - so every list here is driven to its top edge first
-- (the lists clamp, they do not wrap) and counted down from there.

local kListHeight = 14
local kMainMenuHeight = 8

local function press(key)
    dosbox.key(key, true)
    dosbox.wait_frames(6)
    dosbox.key(key, false)
    dosbox.wait_frames(22)
end

local function downs(count)
    for _ = 1, count do
        press("KBD_down")
    end
end

local function onScreen(text)
    return dosbox.screen_text():find(text, 1, true) ~= nil
end

-- An open dialog covers the menu column, truncating its entries to
-- three characters, so the whole label proves no dialog is up.
local function atMainMenu()
    return onScreen("Choose Sound FX Card")
end

-- Re-armed slices: a single wait longer than 30 s is cut short by the
-- engine's wall ceiling even while frames keep advancing (aug-xdae).
local function waitFor(text, slices, what)
    for _ = 1, slices do
        if dosbox.wait_for_text(text, 1500) then
            return
        end
    end
    dosbox.abort("never saw " .. what)
end

local function settle(check, what)
    for _ = 1, 40 do
        if check() then
            return
        end
        dosbox.wait_frames(15)
    end
    dosbox.abort("never reached " .. what)
end

local function pickFromList(index)
    for _ = 1, kListHeight do
        press("KBD_up")
    end
    downs(index)
    press("KBD_enter")
end

local function pickFromMainMenu(index)
    for _ = 1, kMainMenuHeight do
        press("KBD_up")
    end
    downs(index)
    press("KBD_enter")
end

local function expectConfigured(text, what)
    settle(function()
        return atMainMenu() and onScreen(text)
    end, what)
end

dosbox.wait_frames(60)
dosbox.type("d:\n")
dosbox.wait_frames(30)
dosbox.type("cd \\HERETIC\n")
dosbox.wait_frames(30)
dosbox.type("INSTALL.BAT\n")
dosbox.output["progress"] = "5"

waitFor("Which drive to install", 4, "the De-ICE drive prompt")
dosbox.type("C")
waitFor("Enter directory name", 4, "the De-ICE directory prompt")
-- \HERETIC is offered and is where the launch config expects the game.
press("KBD_enter")
waitFor("does not exist", 4, "the create-directory prompt")
dosbox.type("Y")
dosbox.output["progress"] = "25"

-- SETUP is started by INSTALL.BAT itself once the archive is inflated.
waitFor("Keyboard + Joystick", 12, "the setup wizard")
dosbox.output["progress"] = "45"

-- The wizard's defaults are not ours; back out of it and set every
-- device from the main menu instead, where each choice is verifiable.
for _ = 1, 12 do
    if atMainMenu() then
        break
    end
    press("KBD_esc")
    dosbox.wait_frames(45)
end
settle(atMainMenu, "the setup main menu")

pickFromMainMenu(2)
pickFromList(0)
expectConfigured("Keyboard + Mouse", "the controller setting")
dosbox.output["progress"] = "60"

pickFromMainMenu(0)
pickFromList(0)
-- 0x330 matches the engine's MPU-401.
pickFromList(6)
expectConfigured("General Midi", "the music setting")
dosbox.output["progress"] = "75"

pickFromMainMenu(1)
pickFromList(2)
-- Address 220, IRQ 7, DMA 1 - the engine's SB16 defaults - then the
-- full eight mixing channels.
pickFromList(1)
pickFromList(2)
pickFromList(1)
pickFromList(7)
expectConfigured("Sound FX Device: Sound Blaster", "the sound effects setting")
dosbox.output["progress"] = "85"

-- Saving is only offered together with launching the game.
pickFromMainMenu(4)
settle(function()
    return not dosbox.is_text_mode()
end, "Heretic starting")
dosbox.wait_frames(600)

press("KBD_esc")
dosbox.wait_frames(90)
press("KBD_up")
dosbox.wait_frames(90)
press("KBD_enter")
dosbox.wait_frames(90)
press("KBD_y")

settle(dosbox.is_text_mode, "DOS after Heretic quit")
waitFor("C:\\HERETIC>", 4, "the DOS prompt after Heretic")

dosbox.output["progress"] = "100"
dosbox.output["install_complete"] = "yes"
