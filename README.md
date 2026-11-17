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

Developed and built natively on Windows. Requires **Visual Studio 2022 or later** (or the standalone Build Tools) with the *Desktop development with C++* workload, which includes MSVC and CMake. Dependencies (GoogleTest, and later SQLite / nlohmann-json) are fetched automatically by CMake `FetchContent`, so there's no package manager to install.

Open a **Developer PowerShell for VS** (Start menu) and, from the repo root:

```powershell
cmake --preset msvc                  # configure (first run downloads GoogleTest)
cmake --build --preset msvc          # build Release -> build\msvc\bin\Release\tracker.exe
ctest --preset msvc                  # run unit tests
build\msvc\bin\Release\tracker.exe --version
```

Use `cmake --build --preset msvc-debug` for a Debug build (`build\msvc\bin\Debug\`). Alternatively, open the folder in Visual Studio or VS Code (CMake Tools extension), and the `msvc` preset is picked up automatically.

## Repository layout

```
tracker/
  include/tracker/   public headers (platform-independent)
  src/               core logic + main_win.cpp (Windows entry point)
  tests/             GoogleTest unit tests
cmake/               shared compiler options
site/                static frontend served by GitHub Pages
docs/                design notes
plan.md              implementation roadmap
```
