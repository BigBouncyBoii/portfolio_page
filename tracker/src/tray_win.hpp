#pragma once

#include "tracker/config.hpp"
#include "tracker/logger.hpp"

namespace tracker {

// Background mode: tracks with no console, shows a tray icon (tooltip with
// today's keystrokes; menu: Pause/Resume, Publish now, Quit) and saves when
// Windows ends the session. Returns when the user quits or another
// tracker.exe (--install/--uninstall) asks it to close.
int run_background(const Config& config, Logger& log);

}  // namespace tracker
