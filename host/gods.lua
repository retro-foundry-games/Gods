-- Gods (Bitmap Brothers, 1991) from the original PAL disk 1 on a plain Kickstart 1.3 A500.
-- No WHDLoad, no patches: the boot block and the game's own loader run unmodified.
--
--   play:   tools/engine9000/ami9000_host projects/gods/host/gods.lua
--   shots:  ... --headless --script-args shots=OUTDIR [frames=300,600,...]   (PPM per frame)
--
-- Needs the A500 Kickstart 1.3 (CRC32 c4f0f55f) in ../local/system and ../local/gods-disk1.adf.
-- ../local/gods.m3u (create: printf 'gods-disk1.adf\ngods-disk2.adf\n' > local/gods.m3u) lists both disks: the core's multidrive option puts disk 1 in DF0 and disk 2 in DF1.
ami.machine{
    name = "Gods",
    model = "A500",
    system = "../local/system",
    save = "../local/save",
    content = "../local/gods.m3u",
    roms = { { "kick34005.A500", 0xc4f0f55f } },
}
ami.boot()

local dir = (ami.args or ""):match("shots=(%S+)")
if dir then
    local frames = {}
    for n in (ami.args:match("frames=([%d,]+)") or "300,600,1200,2400"):gmatch("%d+") do frames[#frames + 1] = tonumber(n) end
    for _, f in ipairs(frames) do
        ami.wait_until(function() return ami.frame() >= f end)
        ami.screenshot(dir .. "/gods-" .. f .. ".ppm")
        ami.log("frame", f, "pc", string.format("%06x", ami.reg("pc")))
    end
    ami.quit(0)
end
