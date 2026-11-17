# Implementation Plan

Roadmap for the portfolio page + C++ activity tracker. See [`README.md`](README.md) for the architecture.

Each phase ends with something working and demoable. Tick items off as you go.

---

## Phase 0 — Project setup

**Goal:** an empty `.exe` builds locally and in CI.

- [x] Create layout: `tracker/{src,include,tests}`, `site/`, `docs/`
- [x] `CMakeLists.txt` (C++17, warnings-as-errors, GUI subsystem via `WIN32` executable)
- [x] Dependencies via CMake `FetchContent` (GoogleTest now; SQLite / nlohmann-json added in later phases). Chosen over vcpkg: one mechanism that works for native Linux, MinGW cross-compile and MSVC
- [x] `CMakePresets.json`: `linux` (tests), `mingw` (cross-compile), `msvc` (native Windows)
- [x] MinGW-w64 toolchain file for cross-compiling from WSL (`x86_64-w64-mingw32-g++`)
- [x] `.gitignore`, `.clang-format`
- [x] Minimal `tracker.exe` with `--help`, `--version`, `--console` + unit-tested arg parser
- [x] GitHub Actions: MSVC build + tests + smoke test + artifact upload; Linux tests; MinGW cross-compile

**Done when:** pushing to `main` produces a downloadable `tracker.exe` that runs and exits cleanly.

---

## Phase 1 — Input counting

**Goal:** count keystrokes and clicks in memory.

- [ ] `InputHooks` class: dedicated thread running `GetMessage` loop
- [ ] `WH_KEYBOARD_LL`: increment on `WM_KEYDOWN` / `WM_SYSKEYDOWN` (ignore auto-repeat if desired: track key-down state)
- [ ] `WH_MOUSE_LL`: left/right/middle clicks, wheel ticks, cumulative pixel distance
- [ ] Counters are `std::atomic<uint64_t>`; callbacks do **nothing** else
- [ ] Clean shutdown: `PostThreadMessage(WM_QUIT)` → `UnhookWindowsHookEx`
- [ ] Temporary debug mode (`--console`) printing counts every 5 s

**Done when:** typing/clicking visibly increments counters with no noticeable input lag.

---

## Phase 2 — App time tracking

**Goal:** know how long each app was in the foreground.

- [ ] `FocusSampler`: 1 Hz timer → `GetForegroundWindow` → PID → exe name
- [ ] Cache PID → exe name to avoid repeated `OpenProcess` calls
- [ ] Idle detection via `GetLastInputInfo` (threshold configurable, default 120 s)
- [ ] Pause on lock screen (`WTSRegisterSessionNotification`) and sleep (`WM_POWERBROADCAST`)
- [ ] Unit-test the time-attribution logic with an injected fake clock/window source

**Done when:** leaving the PC idle or locked stops time accumulating.

---

## Phase 3 — Persistence

**Goal:** data survives crashes and reboots.

- [ ] SQLite DB at `%LOCALAPPDATA%\PortfolioTracker\tracker.db`
- [ ] Schema:
  ```sql
  CREATE TABLE input_minute (
    minute_utc INTEGER PRIMARY KEY,   -- unix time / 60
    keys INTEGER, lclicks INTEGER, rclicks INTEGER,
    mclicks INTEGER, wheel INTEGER, mouse_px INTEGER
  );
  CREATE TABLE app_minute (
    minute_utc INTEGER, app TEXT, seconds INTEGER,
    PRIMARY KEY (minute_utc, app)
  );
  ```
- [ ] Aggregator: every 60 s, atomically swap counters to 0 and `UPSERT` the bucket
- [ ] Flush on `WM_QUERYENDSESSION` / `WM_ENDSESSION`
- [ ] WAL mode; wrap each flush in a transaction
- [ ] Tests for aggregation against an in-memory DB

**Done when:** killing the process loses at most one minute of data.

---

## Phase 4 — Lifecycle & background running

**Goal:** set-and-forget daily operation.

- [ ] CLI flags: `--install`, `--uninstall`, `--console`, `--publish-now`
- [ ] `--install`: copy exe to `%LOCALAPPDATA%\PortfolioTracker\`, write `HKCU\...\Run` entry, launch
- [ ] Single-instance named mutex
- [ ] Hidden message-only window to receive system messages
- [ ] Tray icon: status tooltip (today's keys/clicks), **Pause / Resume**, **Publish now**, **Quit**
- [ ] File logger with rotation (`tracker.log`)
- [ ] Config file (`config.json`): idle threshold, publish interval, app allowlist

**Done when:** after one `--install`, tracking resumes automatically after every reboot.

---

## Phase 5 — Publishing

**Goal:** sanitised stats appear in the GitHub repo automatically.

- [ ] Create `data` branch (orphan) or separate `portfolio-data` repo
- [ ] Fine-grained PAT: that repo only, `contents: write`; store in Windows Credential Manager (`CredWriteW`), set via `--set-token`
- [ ] JSON builders:
  - `stats/YYYY-MM-DD.json` — hourly breakdown for one day
  - `stats/summary.json` — today, last 7/30 days totals, top apps, all-time totals
- [ ] Sanitiser: app allowlist → friendly names, rest → `"Other"`; no titles, no raw events
- [ ] HTTPS client (WinHTTP or cpr): `GET` file SHA → `PUT /repos/{owner}/{repo}/contents/{path}`
- [ ] Publish every 15–30 min; skip if nothing changed; exponential backoff on network failure
- [ ] Tests for JSON shape and sanitiser

**Done when:** `summary.json` on GitHub updates on its own throughout the day.

---

## Phase 6 — Frontend

**Goal:** a simple, clean public page.

- [ ] `site/index.html`: name, short bio, links (GitHub, LinkedIn, resume PDF), projects
- [ ] "Live stats" section fetching `summary.json` from `raw.githubusercontent.com`
- [ ] Charts (Chart.js): keystrokes per hour today, top apps (bar), 30-day trend (line)
- [ ] "Last updated X min ago" indicator; graceful fallback if fetch fails
- [ ] Short "How this works" blurb linking to the repo/architecture
- [ ] Enable GitHub Pages from `main` → `/site` (or via Actions deploy)

**Done when:** the public URL shows real data from today.

---

## Phase 7 — Polish (resume value)

- [ ] Measure and document resource usage (CPU %, RAM, DB size/month) in README
- [ ] Architecture diagram as an image in `docs/`
- [ ] Design-notes doc: hook threading, idle logic, privacy choices, trade-offs vs. a hosted server
- [ ] Code coverage in CI
- [ ] Tag `v1.0.0` release with `tracker.exe` attached

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
| Developing in WSL but targeting Windows | MinGW cross-compile locally; CI builds natively on `windows-latest` |
