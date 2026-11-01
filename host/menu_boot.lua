-- Boot the original from the disks to the menu, with chosen routines of the menu program replaced by their C versions
-- (build/menu.blob, with the shims of build/takeover_glue.bin that give the C functions the original's register interface).
-- At the end the RAM is saved, to be compared with the same run of the original alone.
--
--   ami9000_host projects/gods/host/menu_boot.lua --headless --script-args "use=load_file,load_unpacked until=6500 out=FILE"
--   (no use= : the original alone)
local args = ami.args or ""
local UNTIL = tonumber(args:match("until=(%d+)")) or 6500
local OUT = args:match("out=(%S+)") or "/tmp/menu_boot.ram"
local CLICK = tonumber(args:match("click=(%d+)")) or 2700
ami.machine{
    name = "Gods menu boot", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/gods.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_chipmem_size = 2, puae_joyport_order = "2143" },
}
ami.boot()
local function read_file(name) return ami.file_read(name, 0, ami.file_size(name)) end
local syms = load(read_file("build/menu_syms.lua"))()
local blob = read_file("build/menu.blob")
local glue = read_file("build/takeover_glue.bin")
local glue_at = load(read_file("build/takeover_glue.lua"))()
-- the original's entry of each routine that can be replaced
local ORIG = { load_file = 0x1d3a6, load_unpacked = 0x2702, load_directory = 0x2988, display_reset = 0x1d36e, drives_idle = 0x1d966, assets_load = 0x1eca2,
  display_setup = 0x39fa6, chips_setup = 0x39fdc, memory_map_init = 0x3a060, display_state_reset = 0x1f5c4, data_pointers_init = 0x1aa3c, sound_init = 0x3a18a }
local calls = {}
for name in (args:match("use=(%S+)") or ""):gmatch("[%w_]+") do
    calls[name] = 0
    ami.hook(ORIG[name], function()
        calls[name] = calls[name] + 1
        ami.write(syms.base, blob)   -- the program may have used the memory the C code sits in
        ami.write(glue_at.base, glue)
        ami.set_reg("pc", glue_at[name])
    end)
end
local STOP = tonumber(args:match("stop=(%x+)") or "", 16)
local stopped = false
if STOP then
    ami.hook(STOP, function()
        if stopped then return end
        stopped = true
        ami.log("stopped at frame " .. ami.frame())
        ami.save_file(OUT, ami.read(0, 0x80000))
    end)
end
while ami.frame() < UNTIL and not stopped do
    ami.wait(1)
    if ami.frame() == CLICK then ami.mouse_button(0, true) end
    if ami.frame() == CLICK + 3 then ami.mouse_button(0, false) end
end
local summary = {}
for name, n in pairs(calls) do summary[#summary + 1] = name .. "=" .. n end
ami.log("replaced calls: " .. table.concat(summary, " "))
if not STOP then ami.save_file(OUT, ami.read(0, 0x80000)) end
ami.quit(0)
