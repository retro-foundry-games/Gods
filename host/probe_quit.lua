-- probe: the original alone from a saved state: when it reaches the routines of the quit (usage: --script-args "state=PATH")
ami.machine{
    name = "probe", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    snapshots = "../local/save",
    content = "../local/gods.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_chipmem_size = 2, puae_joyport_order = "2143" },
}
ami.boot()
ami.wait(1)
local STATE = ami.args:match("state=(%S+)")
assert(ami.snapshot_load(STATE))
local f0 = ami.frame()
local function at(a, n) ami.hook(a, function() ami.log(("%s frame +%d  207a0=%x"):format(n, ami.frame() - f0, ami.peek32(0x207a0))) end) end
at(0x3a54, "3a54 quit") at(0x134e6, "134e6 stop sounds") at(0x1352e, "134e6 done") at(0x12d90, "12d90 text") at(0x1d302, "flip") at(0x6388, "6388 menu")
ami.wait(80)
ami.quit()
