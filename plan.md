# Implementation Plan

Roadmap for the portfolio page + C++ activity tracker. See [`README.md`](README.md) for the architecture and build instructions.

Development is Windows-native: edit, build (MSVC via CMake presets) and test on the same machine the tracker runs on. There is no CI. Keep it simple: from Phase 2 on, no new unit tests; verify manually with `tracker.exe --console`.

Each phase ends with something working and demoable. Tick items off as you go.

---

## Phase 0 — Project setup ✅

**Goal:** an empty `tracker.exe` builds and runs on Windows.

- [x] Create layout: `tracker/{src,include,tests}`, `site/`, `docs/`
- [x] `CMakeLists.txt` (C++17, warnings-as-errors, GUI subsystem via `WIN32` executable)
- [x] Dependencies via CMake `FetchContent` (GoogleTest now; SQLite / nlohmann-json added in later phases), no package manager needed
- [x] `CMakePresets.json`: `msvc` configure preset, `msvc` (Release) / `msvc-debug` build presets, `msvc` test preset
- [x] `.gitignore`, `.clang-format`
- [x] Minimal `tracker.exe` with `--help`, `--version`, `--console` + unit-tested arg parser
- [x] Verified on Windows: configure, build, 8/8 tests pass, `tracker.exe --version` prints `tracker 0.1.0`

**Done when:** `cmake --preset msvc && cmake --build --preset msvc && ctest --preset msvc` succeeds and `tracker.exe --version` runs and exits cleanly.

---

## Phase 1 — Input counting ✅

**Goal:** count keystrokes in memory. (Mouse/touchpad click counts were added later, in Phase 4: same hook thread, `WH_MOUSE_LL`, counts only.)

- [x] `KeyCounter` (core, unit-tested): `std::atomic<uint64_t>` count, `take()` atomically reads and resets; auto-repeat ignored by tracking key-down state
- [x] `KeyboardHook` (Win32): dedicated thread running a `GetMessage` loop with `WH_KEYBOARD_LL`; `WM_KEYDOWN`/`WM_SYSKEYDOWN` count, `WM_KEYUP`/`WM_SYSKEYUP` release
- [x] Callback does **nothing** but update the counter; injected (synthetic) input is ignored
- [x] Clean shutdown: `PostThreadMessage(WM_QUIT)` → `UnhookWindowsHookEx`
- [x] Debug mode (`--console`) opens a console window printing the count every 5 s; Ctrl+C / closing the window stops cleanly
- [x] Running without `--console` shows a "use --console" hint until background mode lands in Phase 4

**Done when:** typing visibly increments the counter with no noticeable input lag.

---

## Phase 2 — App time tracking ✅

**Goal:** know how long each app was in the foreground.

- [x] `FocusSampler` (`src/focus_sampler_win.*`): 1 Hz thread → `GetForegroundWindow` → PID → exe name (no window titles)
- [x] Cache PID → exe name (cleared every 10 min since PIDs get reused)
- [x] Idle detection via `GetLastInputInfo` (default 120 s)
- [x] Lock screen: no foreground window or `LockApp.exe` → not counted. Sleep: adds 1 s per tick, not wall-clock time, so sleeping adds nothing (simpler than `WTSRegisterSessionNotification` / `WM_POWERBROADCAST`)
- [x] `--console` shows keys, current app/idle/locked state and per-app seconds every 5 s

**Done when:** leaving the PC idle or locked stops time accumulating.

---

## Phase 3 — Persistence ✅

**Goal:** data survives crashes and reboots.

- [x] SQLite (3.46.1 amalgamation via `FetchContent`) DB at `%LOCALAPPDATA%\PortfolioTracker\tracker.db`; `Storage` class in `tracker/src/storage.cpp`
- [x] Schema:
  ```sql
  CREATE TABLE input_minute (
    minute_utc INTEGER PRIMARY KEY,   -- unix time / 60
    keys INTEGER NOT NULL DEFAULT 0,
    clicks INTEGER NOT NULL DEFAULT 0   -- added in Phase 4
  );
  CREATE TABLE app_minute (
    minute_utc INTEGER, app TEXT, seconds INTEGER,
    PRIMARY KEY (minute_utc, app)
  );
  ```
- [x] Aggregator (in `main_win.cpp`): every 5 s `take()` counts into the current minute's bucket; when the minute changes, additive `UPSERT` of the bucket. Final flush on Ctrl+C / window close / logoff / shutdown (console ctrl handler)
- [x] Flush on `WM_QUERYENDSESSION` / `WM_ENDSESSION` → done in Phase 4 (hidden window)
- [x] WAL mode; each flush is one transaction
- [x] `--console` prints today's saved totals at start and stop

**Done when:** killing the process loses at most one minute of data.

---

## Phase 4 — Lifecycle & background running ✅

**Goal:** set-and-forget daily operation.

- [x] CLI flags: `--install`, `--uninstall`, `--console`, `--publish-now` (publish-now posts a message to the running tracker; it just logs until Phase 5)
- [x] `--install`: stop any running tracker, copy exe to `%LOCALAPPDATA%\PortfolioTracker\`, write `HKCU\...\Run` (`PortfolioTracker`) entry, launch. `--uninstall`: stop it, remove the Run entry and installed exe, keep data
- [x] Single-instance named mutex (`Local\PortfolioTracker.Instance`, covers tray and console mode)
- [x] Hidden window to receive system messages: a hidden *top-level* window rather than message-only, because message-only windows miss broadcasts (`WM_QUERYENDSESSION`, `TaskbarCreated`). Save on `WM_QUERYENDSESSION`, stop + save on `WM_ENDSESSION`
- [x] Tray icon: status tooltip (today's keystrokes, clicks, top apps), stats in the menu, **Pause / Resume**, **Publish now**, **Quit**; re-added if Explorer restarts
- [x] File logger with rotation (`tracker.log`, rotated to `tracker.log.1` at 1 MB)
- [x] Config file (`config.json`, written with defaults on first run; nlohmann/json via `FetchContent`): idle threshold, publish interval, app allowlist
- [x] Click counting (mouse + touchpad, button presses only; injected input ignored) → `clicks` column in `input_minute`, auto-migrated on existing DBs; shown in console, tooltip and tray menu
- [x] Shared `Session` (`session_win.*`) = hook + sampler + per-minute aggregator, used by both tray and console mode

**Done when:** after one `--install`, tracking resumes automatically after every reboot.

---

## Phase 5 — Publishing ✅ (code done; needs one-time setup below)

**Goal:** sanitised stats appear in the GitHub repo automatically.

- [ ] Create `data` orphan branch (one-time, see README "Publishing"; target set by `publish_repo` / `publish_branch` in `config.json`)
- [x] Fine-grained PAT (that repo only, `contents: write`) stored in Windows Credential Manager (`CredWriteW`, `PortfolioTracker/GitHubToken`), set via `--set-token` (re-read on every publish, no restart needed)
- [x] JSON builders (`publish_json.cpp`, core):
  - `stats/YYYY-MM-DD.json`: day totals, 24 hourly buckets, apps (today + yesterday are published)
  - `stats/summary.json`: today (with hours), last 7/30 days, all time, 30-day daily series, `generated_at`
- [x] Sanitiser: app allowlist (case-insensitive) → friendly names, rest → `"Other"`, top 10; no titles, no raw events. Local copies in `%LOCALAPPDATA%\PortfolioTracker\publish\`
- [x] HTTPS client (WinHTTP, `publisher_win.cpp`) on a worker thread: `GET` file SHA → `PUT /repos/{owner}/{repo}/contents/{path}`
- [x] Publish every `publish_interval_min` (30) + on start + tray/`--publish-now`; skip if day files unchanged; exponential backoff (1, 2, 4… min, capped at the interval) on failure. Console mode prints `[publish]` status

**Done when:** `summary.json` on GitHub updates on its own throughout the day.

---

## Phase 6 — Frontend

**Goal:** a simple, clean public page.

**Content source:** [`docs/portfolio_content.md`](docs/portfolio_content.md) holds everything extracted from the resume ([`james-loke-resume-2-11-26.pdf`](james-loke-resume-2-11-26.pdf)), [GitHub](https://github.com/BigBouncyBoii) and [LinkedIn](https://www.linkedin.com/in/james-loke-34a0612bb/). Build the page from it. Go back to the resume/GitHub/LinkedIn only for anything missing, and update that file first.

- [x] Extract resume + GitHub content into `docs/portfolio_content.md` (LinkedIn blocks automated fetches; merge in a "Save to PDF" export later)
- [ ] `site/index.html`: name, short bio, links (GitHub, LinkedIn, resume PDF), education, experience, featured projects, skills, awards/certificates (all from `docs/portfolio_content.md`)
- [ ] Copy the resume PDF into `site/` so the page can link it; keep the phone number off the page
- [ ] "Live stats" section fetching `summary.json` from `raw.githubusercontent.com`
- [ ] Charts (Chart.js): keystrokes per hour today, top apps (bar), 30-day trend (line)
- [ ] "Last updated X min ago" indicator; graceful fallback if fetch fails
- [ ] Short "How this works" blurb linking to the repo/architecture
- [ ] Enable GitHub Pages from `main` → `/site`

**Done when:** the public URL shows real data from today.

---

## Phase 7 — Polish (resume value)

- [ ] Measure and document resource usage (CPU %, RAM, DB size/month) in README
- [ ] Architecture diagram as an image in `docs/`
- [ ] Design-notes doc: hook threading, idle logic, privacy choices, trade-offs vs. a hosted server
- [ ] Code coverage report (e.g. OpenCppCoverage) for the core library
- [ ] Tag `v1.0.0` release with a locally built `tracker.exe` attached (`gh release create`)

---

## Stretch goals

- [ ] Platform abstraction (`IInputSource`, `IFocusSource`) + Linux implementation (X11 / evdev)
- [ ] Optional hosted C++ API (Crow/Drogon) for real-time stats instead of periodic JSON
- [ ] Activity heatmap (GitHub-contribution-style calendar)
- [ ] Words-per-minute estimate from keystroke bursts
- [ ] Code signing to avoid SmartScreen/Defender warnings

---

## Risks & mitigations

| Risk | Mitigation |
|---|---|
| Windows silently removes slow hooks | Callbacks only touch atomics; all work on other threads |
| Antivirus flags hook usage | Defender exclusion for dev folder; open-source repo; optional signing later |
| Leaking private info via app names | Strict allowlist; never publish window titles |
| Token leak | Credential Manager, minimal-scope fine-grained PAT, never committed |
| Commit spam in history | Publish to a separate `data` branch/repo |
| Sleep/idle inflating app time | Idle threshold + power/session notifications |
