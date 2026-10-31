# Gods project instructions

Read [framework instructions](../../AGENTS.md), [project README](README.md) and
[recomp-first workflow](../../docs/recomp/porting_game.md).

- Keep Gods addresses, loader mappings, controls, hooks, scenarios and results here.
  Shared translator/runtime/hardware changes stay at framework level.
- Original media and observed execution are authority. Record media hashes and the
  exact variant (release, disk count, crack/trainer status) before analysis.
- Keep supplied media, ROMs and configs in ignored `local/`, captures in ignored
  `captures/`, generated code/output in ignored `build/`; reviewed reports go in
  `analysis/`.
- `project.json` memory regions and `local/gods.uae` are provisional until the
  actual machine configuration is established from the supplied media. Do not
  invent register state, RAM maps or chipset snapshots to fill gaps.
- Follow the recomp-first order: executable baseline, qualification against
  original execution, then semantic decompilation.
