-- Save the menu program as the loader leaves it: the RAM at the moment the original enters it ($84, then $39E50), before
-- the start-up code has run.  native/build/menu-image.bin (1 MB chip RAM).  The boot-time code (the file loader's start, the
-- initialisation) is only there at this moment.
--
--   ami9000_host projects/gods/host/menu_image.lua --headless [--script-args "click=FRAME"]
local args = ami.args or ""
local CLICK = tonumber(args:match("click=(%d+)")) or 2700
ami.machine{
    name = "Gods menu image", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/gods.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_chipmem_size = 2, puae_joyport_order = "2143" },
}
ami.boot()
local done = false
ami.hook(0x39e50, function()
    if done then return end
    done = true
    ami.save_file("/home/boran/git/amiga-decomp/projects/gods/native/build/menu-image.bin", ami.read(0, 0x100000))
    ami.log("saved the menu image at frame " .. ami.frame())
end)
while not done and ami.frame() < 9000 do
    ami.wait(1)
    if ami.frame() == CLICK then ami.mouse_button(0, true) end
    if ami.frame() == CLICK + 3 then ami.mouse_button(0, false) end
end
ami.quit(done and 0 or 1)
