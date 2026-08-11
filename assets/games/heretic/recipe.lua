-- This file is part of the dosbox-automation-showroom Project.
-- License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
--
-- Standalone install recipe for Heretic shareware v1.0 (Raven/id).
-- The disc is a two-game shareware sampler; only \HERETIC is used.
-- INSTALL.BAT runs De-ICE and then hands straight over to SETUP's
-- first-run wizard. The recipe backs out of the wizard and configures
-- each device from the main menu, navigating by delta from the known
-- fresh defaults rather than clamping to the top of every list.
--
-- Fresh-default positions probed 2026-08-11 with no HERETIC.CFG:
-- Controller: pos 2 (Keyboard only); Music: pos 6 (Sound Blaster);
-- SFX card: pos 2 (Sound Blaster); Address: pos 1 (220);
-- IRQ: pos 2 (7); DMA: pos 1 (1); Channels: pos 2 (3);
-- MIDI port: pos 6 (330). Main menu cursor starts at pos 4.

local menuPos = 0

local function press(key)
    dosbox.key(key, true)
    dosbox.wait_frames(6)
    dosbox.key(key, false)
    dosbox.wait_frames(10)
end

local function move(delta)
    local key = "KBD_down"
    if delta < 0 then
        key = "KBD_up"
        delta = -delta
    end
    for _ = 1, delta do
        press(key)
    end
end

local function onScreen(text)
    return dosbox.screen_text():find(text, 1, true) ~= nil
end

local function atMainMenu()
    return onScreen("Choose Sound FX Card")
end

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

local function goToMainItem(target)
    move(target - menuPos)
    menuPos = target
    press("KBD_enter")
end

local function expectConfigured(text, what)
    settle(function()
        return atMainMenu() and onScreen(text)
    end, what)
end

dosbox.wait_frames(10)
dosbox.type("d:\n")
dosbox.wait_frames(10)
dosbox.type("cd \\HERETIC\n")
dosbox.wait_frames(10)
dosbox.type("INSTALL.BAT\n")
dosbox.output["progress"] = "5"

waitFor("Which drive to install", 4, "the De-ICE drive prompt")
dosbox.type("C")
waitFor("Enter directory name", 4, "the De-ICE directory prompt")
press("KBD_enter")
waitFor("does not exist", 4, "the create-directory prompt")
dosbox.type("Y")
dosbox.output["progress"] = "25"

waitFor("Keyboard + Joystick", 12, "the setup wizard")
dosbox.output["progress"] = "45"

for _ = 1, 12 do
    if atMainMenu() then
        break
    end
    press("KBD_esc")
    dosbox.wait_frames(20)
end
settle(atMainMenu, "the setup main menu")
menuPos = 4

-- Controller: Keyboard + Mouse (default pos 2, target pos 0).
goToMainItem(2)
move(-2)
press("KBD_enter")
expectConfigured("Keyboard + Mouse", "the controller setting")
dosbox.output["progress"] = "60"

-- Music: General MIDI (default pos 6, target pos 0), then
-- MIDI port 330 (default pos 6, already correct).
goToMainItem(0)
move(-6)
press("KBD_enter")
press("KBD_enter")
expectConfigured("General Midi", "the music setting")
dosbox.output["progress"] = "75"

-- SFX: Sound Blaster (default pos 2, already correct), then
-- address 220 (default, accept), IRQ 7 (default, accept),
-- DMA 1 (default, accept), 8 channels (default pos 2, target pos 7).
goToMainItem(1)
press("KBD_enter")
press("KBD_enter")
press("KBD_enter")
press("KBD_enter")
move(5)
press("KBD_enter")
expectConfigured("Sound FX Device: Sound Blaster", "the sound effects setting")
dosbox.output["progress"] = "85"

goToMainItem(4)
settle(function()
    return not dosbox.is_text_mode()
end, "Heretic starting")
dosbox.wait_frames(100)

press("KBD_esc")
dosbox.wait_frames(15)
press("KBD_up")
dosbox.wait_frames(15)
press("KBD_enter")
dosbox.wait_frames(15)
press("KBD_y")

settle(dosbox.is_text_mode, "DOS after Heretic quit")
waitFor("C:\\HERETIC>", 4, "the DOS prompt after Heretic")

dosbox.output["progress"] = "100"
dosbox.output["install_complete"] = "yes"
