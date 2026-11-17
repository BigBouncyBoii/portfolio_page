# CLAUDE.md

Portfolio page + a small C++17 Windows tracker that counts keystrokes and foreground-app time, then (later phases) publishes sanitised JSON to GitHub for a static GitHub Pages site. Architecture: `README.md`. Roadmap with progress ticks: `plan.md` — read it first and tick items off as you finish them.

## Working style (user preference)
- Keep it simple and token-light. This is a personal portfolio project, not production software.
- No new unit tests, no CI, no over-engineering. Verify by building and running `tracker.exe --console`.
- Keyboard only: the machine is a laptop with a touchpad, so collect **no mouse/touchpad metrics**.
- Privacy: counts and exe names only. Never read key identities or window titles.

## Build (Windows native, MSVC)
VS 18 (2026) Insiders is installed at `C:\Program Files\Microsoft Visual Studio\18\Insiders`. `cmake`/`cl` aren't on PATH in a plain shell, and `Launch-VsDevShell.ps1` / `vswhere.exe` fail in Claude's sandbox, so build through `VsDevCmd.bat`:

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\Tools\VsDevCmd.bat" -arch=amd64 >nul && cmake --preset msvc && cmake --build --preset msvc'
```

Output: `build\msvc\bin\Release\tracker.exe`. Warnings are errors (`/W4 /WX`). Existing tests: `ctest --preset msvc`.

## Layout
- `tracker/include/tracker/`, `tracker/src/cli.cpp`, `key_counter.cpp` → `tracker_core` (platform-independent)
- `tracker/src/*_win.{hpp,cpp}` → `tracker_win` (Win32: `KeyboardHook` = `WH_KEYBOARD_LL` on its own thread; `FocusSampler` = 1 Hz foreground exe + idle/lock detection)
- `tracker/src/main_win.cpp` → `tracker.exe` (GUI subsystem; `--console` opens a debug console printing stats every 5 s; plain run just shows a hint until Phase 4)
- `site/` static frontend (plain HTML, **zero CSS**), `docs/` notes

## Gotchas
- Editing via Bash heredoc + Python mangles `\n` escapes in C++ strings; use the Edit tool for C++ source. Windows Python can't see Git-Bash `/tmp`.
- Low-level hook callbacks must stay trivial (Windows drops slow hooks).
- Non-elevated hooks don't see input to elevated (admin) windows.
