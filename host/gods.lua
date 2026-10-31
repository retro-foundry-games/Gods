-- Gods (Bitmap Brothers, 1991) from the original PAL disk 1 on a plain Kickstart 1.3 A500.
-- No WHDLoad, no patches: the boot block and the game's own loader run unmodified.
--
--   play:   tools/engine9000/ami9000_host projects/gods/host/gods.lua
--   flags:  see "Script flags" below (snapshots, screenshots)
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

-- Script flags (--script-args "..."):
--   load=NAME            restore snapshot NAME (a file in ../local/save) before running; frames below count from it
--   snap=F[,F...]        save snapshot gods-F.state at frame F (relative to the start or the loaded snapshot)
--   ram=PATH            write chip RAM (512 KiB) at the end of the events, then quit
--   shots=DIR frames=F,. write DIR/gods-F.ppm at those frames, then quit (headless checks)
local args = ami.args or ""
local function frames_of(key)
    local list = {}
    for n in (args:match(key .. "=([%d,]+)") or ""):gmatch("%d+") do list[#list + 1] = tonumber(n) end
    return list
end
local load_name = args:match("load=(%S+)")
local shots_dir = args:match("shots=(%S+)")
if load_name then
    ami.wait(1)
    if not ami.snapshot_load(load_name) then ami.log("cannot load snapshot", load_name) ami.quit(2) end
end
local base = ami.frame()
local events = {}
for _, f in ipairs(frames_of("snap")) do events[#events + 1] = { f, "snap" } end
if shots_dir then
    local list = frames_of("frames")
    if #list == 0 then list = { 300, 600, 1200, 2400 } end
    for _, f in ipairs(list) do events[#events + 1] = { f, "shot" } end
end
table.sort(events, function(a, b) return a[1] < b[1] end)
for _, e in ipairs(events) do
    ami.wait_until(function() return ami.frame() - base >= e[1] end)
    if e[2] == "snap" then
        local name = "gods-" .. e[1] .. ".state"
        ami.log("snapshot", name, ami.snapshot_save(name))
    else
        ami.screenshot(shots_dir .. "/gods-" .. e[1] .. ".ppm")
        ami.log("frame", e[1], "pc", string.format("%06x", ami.reg("pc")))
    end
end
local ram_path = args:match("ram=(%S+)")
if ram_path then
    ami.wait(1)
    ami.save_file(ram_path, ami.read(0, 0x80000))
    ami.log("ram", ram_path, "pc", string.format("%06x", ami.reg("pc")), "frame", ami.frame() - base)
end
if shots_dir or ram_path then ami.quit(0) end
