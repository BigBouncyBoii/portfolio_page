# portfolio_page

A portfolio page with a twist: alongside the usual resume content, it shows **live statistics about how I use my computer** — keystrokes, mouse clicks, time spent per application — collected by a lightweight C++ tracker running on my PC.

> **Status:** 🚧 In development — see [`plan.md`](plan.md) for the roadmap.

---

## Architecture

```
┌─────────────────────────── Windows PC ────────────────────────────┐
│                                                                   │
│  tracker.exe  (C++17, native Win32, starts at login)              │
│                                                                   │
│  ┌──────────────────┐   ┌──────────────────┐                      │
│  │  Input Thread    │   │  Focus Sampler   │                      │
│  │  WH_KEYBOARD_LL  │   │  (1 Hz)          │                      │
│  │  WH_MOUSE_LL     │   │  foreground app  │                      │
│  │  → atomic counts │   │  + idle detect   │                      │
│  └────────┬─────────┘   └────────┬─────────┘                      │
│           └──────────┬───────────┘                                │
│                      ▼                                            │
│            ┌───────────────────┐                                  │
│            │  Aggregator       │  flushes every 60 s              │
│            │  → SQLite (local) │  per-minute buckets              │
│            └─────────┬─────────┘                                  │
│                      ▼                                            │
│            ┌───────────────────┐                                  │
│            │  Publisher        │  every 15–30 min                 │
│            │  sanitise → JSON  │  app allowlist, counts only      │
│            └─────────┬─────────┘                                  │
└──────────────────────┼────────────────────────────────────────────┘
                       │  HTTPS · GitHub Contents API
                       │  (fine-grained PAT, single repo, contents:write)
                       ▼
┌──────────────────────────── GitHub ───────────────────────────────┐
│  data branch        stats/summary.json, stats/YYYY-MM-DD.json     │
│  main branch   ──►  GitHub Pages: index.html + JS (Chart.js)      │
│                     fetches JSON at load time, renders charts     │
└───────────────────────────────────────────────────────────────────┘
```

### Why this shape?

GitHub Pages only serves **static files**, so there is no server-side "backend" at request time. Instead, the C++ program *is* the backend: it runs on the machine being measured, owns all collection and aggregation, and periodically publishes pre-computed JSON snapshots. The site is a thin static viewer. This keeps hosting free, with no server to secure or keep running.

### Components

| Component | Responsibility | Key APIs / libs |
|---|---|---|
| **Input hooks** | Count keystrokes, clicks, scroll, mouse distance. Callbacks only increment `std::atomic` counters and return immediately (Windows drops slow hooks). | `SetWindowsHookExW`, dedicated message-loop thread |
| **Focus sampler** | Once per second, attribute time to the foreground process. Pauses when idle > 2 min or system is asleep. | `GetForegroundWindow`, `QueryFullProcessImageNameW`, `GetLastInputInfo`, `WM_POWERBROADCAST` |
| **Storage** | Durable per-minute buckets so a crash loses ≤ 60 s of data. | SQLite3 |
| **Publisher** | Aggregate to daily/summary JSON, sanitise, upload. | nlohmann/json, WinHTTP / cpr, GitHub REST API |
| **Lifecycle** | Auto-start at login, single instance, tray icon (pause/quit), flush on shutdown. | `HKCU\...\Run`, named mutex, `Shell_NotifyIconW`, `WM_ENDSESSION` |
| **Frontend** | Plain static page with zero CSS: resume + charts from published JSON. | Semantic HTML, vanilla JS, Chart.js |

### Frontend: plain HTML, zero CSS

The site is deliberately plain HTML with **no CSS at all**: no stylesheets, no `<style>` blocks, no inline `style` attributes, no CSS frameworks. It relies on the browser's default rendering and semantic markup (`<header>`, `<main>`, `<section>`, `<ul>`, `<table>`) for structure, so the content and data are what stand out.

### Privacy by design

- **Counts only.** Key *identities* are never read, stored, or transmitted. This is a counter, not a keylogger.
- **No window titles.** Only executable names are used, mapped through an **allowlist** to friendly labels (`Code.exe → "VS Code"`); everything else becomes `"Other"`.
- **Coarse publishing.** Public data is aggregated hourly/daily, never raw events.
- **Local-first.** Raw minute-level data never leaves the machine.
- **Pause anytime** from the tray icon.
- **Secrets** (GitHub token) live in Windows Credential Manager, never in the repo.

---

## Usage (planned)

```powershell
tracker.exe --install     # copy to %LOCALAPPDATA%, register auto-start, launch
tracker.exe --uninstall   # remove auto-start entry
tracker.exe               # normal run (invoked automatically at login)
```

## Building

Requires CMake ≥ 3.21 and a C++17 compiler. Dependencies (GoogleTest, and later SQLite / nlohmann-json) are fetched automatically by CMake `FetchContent`; there's no package manager to install.

| Preset | Host | Produces |
|---|---|---|
| `linux` | Linux / WSL | `tracker_core` + unit tests (no `.exe`) |
| `mingw` | Linux / WSL | `build/mingw/bin/tracker.exe` (cross-compiled, static) |
| `msvc` | Windows | `build/msvc/bin/Release/tracker.exe` + unit tests |

```bash
# Unit tests (WSL / Linux)
cmake --preset linux && cmake --build --preset linux && ctest --preset linux

# Windows .exe from WSL (one-time: sudo apt install mingw-w64)
cmake --preset mingw && cmake --build --preset mingw

# Native Windows (Developer PowerShell)
cmake --preset msvc; cmake --build --preset msvc; ctest --preset msvc
```

CI (`.github/workflows/build.yml`) runs all three on every push and uploads `tracker.exe` as a build artifact.

## Repository layout

```
tracker/
  include/tracker/   public headers (platform-independent)
  src/               core logic + main_win.cpp (Windows entry point)
  tests/             GoogleTest unit tests
cmake/               compiler options, MinGW toolchain
site/                static frontend served by GitHub Pages
docs/                design notes
plan.md              implementation roadmap
```
