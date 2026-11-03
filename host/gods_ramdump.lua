-- Writes the chip RAM of a saved state of the original (the quickload image of the port: `gods-sdl --quickload FILE`).
--   tools/engine9000/ami9000_host --headless --frames 20 projects/gods/host/gods_ramdump.lua --script-args "state=/ABS/PATH.state out=/ABS/PATH.ram"
local args = ami.args or ""
local STATE = assert(args:match("state=(%S+)"), "state=PATH")
local OUT = assert(args:match("out=(%S+)"), "out=PATH")
ami.machine{
    name = "Gods ramdump", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    snapshots = "../local/save",
    content = "../local/gods.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_chipmem_size = 2, puae_joyport_order = "2143" },
}
ami.boot()
ami.wait(1)
assert(ami.snapshot_load(STATE), "cannot load " .. STATE)
ami.wait(1)
ami.save_file(OUT, ami.read(0, 0x100000))
ami.log("ramdump: " .. OUT)
