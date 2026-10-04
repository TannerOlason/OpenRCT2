# Codex Local Instructions

- This project is Factory Tour, a mergeable fork of OpenRCT2 with a Factorio-style production layer.
- Read [`CLAUDE.md`](CLAUDE.md) first: it holds the toolchain setup (conda, no sudo), build and test
  commands, fork conventions, reserved ids and hazards. Then read [`CONTEXT.md`](CONTEXT.md) and the
  relevant `docs/adr/` records before design or implementation work.
- [`wiki/SCOPE.md`](wiki/SCOPE.md) is the authoritative product boundary; [`wiki/ROADMAP.md`](wiki/ROADMAP.md)
  tracks milestone status. Pick the next slice from the current milestone, not from retired ideas.
- Prefer `GPT-5.3-Codex-Spark` for small, low-risk tasks until the user says otherwise; use
  `$prefer-codex-spark` when routing is relevant.
- Use `$codex-monitor` for long-running work with `CODEX_MONITOR_AGENT=factory-tour` and
  `CODEX_MONITOR_LOG=/tmp/codex-monitor/factory-tour.jsonl`. Mark approval waits with `--approval`.
- Never commit to `develop`; it mirrors upstream. Work on `factory-tour/main` or branches off it.
- Mark every upstream edit with `// FACTORY-TOUR:` and add every new source file to the `.vcxproj` lists.
- Keep builds and large data on `/media/user/D`; the root disk is nearly full.
