-- Record every custom register write (cycle, register, value, PC, copper) over a window of frames, with the core built
-- with the custom-write observer (tools/engine9000/system/ami9000-trace.so, see tools/ami9000_host/build_core.sh).
--
--   ami9000_host writes.lua --headless --script-args "[rebuilt] start=F frames=N out=PATH"
local args = ami.args or ""
ami.machine{
    name = "Gods writes", model = "A500", system = "../local/system", save = "../local/save",
    content = args:match("rebuilt") and "../local/gods-c.m3u" or "../local/gods.m3u",
    core = "../../../tools/engine9000/system/ami9000-trace.so",
    roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = args:match("rebuilt") and { puae_chipmem_size = 2 } or nil,
}
ami.boot()
local start, frames = tonumber(args:match("start=(%d+)")), tonumber(args:match("frames=(%d+)"))
while ami.frame() < start do ami.wait(1) end
ami.trace_writes(true)
local from = ami.frame()
while ami.frame() < from + frames do ami.wait(1) end
ami.save_file(args:match("out=(%S+)"), ami.take_writes())
ami.quit(0)
