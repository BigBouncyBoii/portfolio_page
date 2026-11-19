// Live stats: reads summary.json published by the tracker to the `data` branch.
// For local testing: index.html?data=stats/summary.json
const DEFAULT_URL =
  "https://raw.githubusercontent.com/BigBouncyBoii/portfolio_page/data/stats/summary.json";

const status = document.getElementById("stats-status");

function fmtDuration(seconds) {
  const h = Math.floor(seconds / 3600);
  const m = Math.round((seconds % 3600) / 60);
  return h > 0 ? `${h} h ${m} min` : `${m} min`;
}

function fmtAgo(iso) {
  const min = Math.round((Date.now() - new Date(iso).getTime()) / 60000);
  if (min < 1) return "just now";
  if (min < 60) return `${min} min ago`;
  const h = Math.floor(min / 60);
  if (h < 48) return `${h} h ago`;
  return `${Math.floor(h / 24)} days ago`;
}

function totalsRow(label, t) {
  const tr = document.createElement("tr");
  const cells = [label, t.keys.toLocaleString(), t.clicks.toLocaleString(),
                 t.active_minutes.toLocaleString(), fmtDuration(t.app_seconds)];
  cells.forEach((text, i) => {
    const td = document.createElement(i === 0 ? "th" : "td");
    td.textContent = text;
    tr.appendChild(td);
  });
  return tr;
}

function chart(id, type, labels, datasets, extra = {}) {
  new Chart(document.getElementById(id), {
    type,
    data: { labels, datasets },
    options: { responsive: false, animation: false, ...extra },
  });
}

function render(s) {
  const body = document.getElementById("stats-totals");
  body.append(totalsRow("Today", s.today), totalsRow("Last 7 days", s.last_7_days),
              totalsRow("Last 30 days", s.last_30_days), totalsRow("All time", s.all_time));

  const hours = s.today.hours;
  chart("chart-hours", "bar", hours.map((h) => `${String(h.hour).padStart(2, "0")}:00`), [
    { label: "Keystrokes", data: hours.map((h) => h.keys) },
    { label: "Clicks", data: hours.map((h) => h.clicks) },
  ]);

  const apps = s.today.apps;
  chart("chart-apps", "bar", apps.map((a) => a.name),
        [{ label: "Minutes", data: apps.map((a) => Math.round(a.seconds / 60)) }],
        { indexAxis: "y" });

  const daily = s.daily;
  chart("chart-trend", "line", daily.map((d) => d.date.slice(5)), [
    { label: "Keystrokes", data: daily.map((d) => d.keys) },
    { label: "Active minutes", data: daily.map((d) => d.active_minutes), yAxisID: "y1" },
  ], {
    scales: {
      y: { beginAtZero: true, title: { display: true, text: "Keystrokes" } },
      y1: { beginAtZero: true, position: "right", grid: { drawOnChartArea: false },
            title: { display: true, text: "Active minutes" } },
    },
  });

  status.textContent = `Last updated ${fmtAgo(s.generated_at)} ` +
    `(${new Date(s.generated_at).toLocaleString()}). Today = ${s.today.date}, tracker's local time.`;
  document.getElementById("stats").hidden = false;
}

async function load() {
  const url = new URLSearchParams(location.search).get("data") || DEFAULT_URL;
  try {
    const res = await fetch(url, { cache: "no-store" });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const summary = await res.json();
    if (typeof Chart === "undefined") throw new Error("Chart.js failed to load");
    render(summary);
  } catch (err) {
    status.textContent = `Live stats are unavailable right now (${err.message}). ` +
      "The tracker may not have published yet.";
  }
}

load();
