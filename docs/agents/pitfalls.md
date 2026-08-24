# Common pitfalls checklist

*The checklist, for when something does not build and the reason is dull.*

*Part of the [build & test guide](../../AGENTS.md).*

- [ ] Built via `xenolith-cli` when it is on `PATH` (raw `make` only as
      fallback — [Golden rules](golden-rules.md)). Inside this monorepo, passed `--engine <abs-engine-root>`.
- [ ] Used absolute project / `--engine` paths (cwd drifts between calls).
- [ ] Did **not** try to compile a `.cc` subunit standalone (build its `.cpp` SCU).
- [ ] For Android: no `-j`, cleared `MAKEFLAGS`, `touch`ed edited sources, cleared
      a stale `Android.mk.tmp` if export failed with exit 126.
- [ ] To verify Windows/Android/macOS code, built the **matching target** — a
      green native build does not cover them.
- [ ] Treated a clean **build** as the signal when no runner is available
      (GUI/macOS/Android); ran the **binary** for native CLI tests and via Wine.
- [ ] Ran with `verbose=1` when a configure/module/toolchain problem is unclear.
