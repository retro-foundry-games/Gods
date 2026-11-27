-- Takes a saved state of the original and saves it again at its next screen flip ($1D302: the game's iteration has just ended and its next one has not begun), as a state and as its chip RAM:
-- a quickload of the port (gods-sdl-dev --quickload) begins an iteration at its top, so the original's state has to be at the end of one.
--   ../amiga_porting/ami9000/build/ami9000_host --headless --frames 400 host/gods_flipsave.lua --script-args "state=/ABS/IN.state out=/ABS/OUTPREFIX"   (writes OUTPREFIX.state and OUTPREFIX.ram)
local args = ami.args or ""
local STATE = assert(args:match("state=(%S+)"), "state=PATH")
local OUT = assert(args:match("out=(%S+)"), "out=PREFIX")
ami.machine{
    name = "Gods flipsave", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    snapshots = "../local/save",
    content = "../local/gods.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_chipmem_size = 2, puae_joyport_order = "2143" },
}
ami.boot()
ami.wait(1)
assert(ami.snapshot_load(STATE), "cannot load " .. STATE)
local reached = false
ami.hook(0x1d302, function() reached = true end)
for _ = 1, 200 do
    ami.wait(1)
    if reached then break end
end
assert(reached, "the original did not flip")
-- (the frame in which it flipped is over: it waits in the flip for the next blank; its next iteration has not begun)
assert(ami.snapshot_save(OUT .. ".state"))
ami.save_file(OUT .. ".ram", ami.read(0, 0x100000))
ami.log(("flipsave: %s at frame %d"):format(OUT, ami.frame()))
