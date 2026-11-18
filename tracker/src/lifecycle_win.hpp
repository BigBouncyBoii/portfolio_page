#pragma once

#include <windows.h>

#include <string>

namespace tracker {

// Class name of the background tracker's hidden window; other tracker.exe
// invocations find it with FindWindowW to send it commands.
inline constexpr wchar_t kWindowClass[] = L"PortfolioTracker.Window";
inline constexpr UINT kMsgPublishNow = WM_APP + 2;

// Takes the per-user named mutex. False if another tracker is already running
// (in either tray or console mode). Held until the process exits.
bool acquire_single_instance();

// Asks a running background tracker to publish. False if none is running.
bool request_publish();

// --install / --uninstall. `message` describes the outcome for the user.
bool install(std::string& message);
bool uninstall(std::string& message);

}  // namespace tracker
