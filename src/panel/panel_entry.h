#pragma once

#ifdef ZIBBY_HAS_PANEL

namespace zibby::panel {

// Runs the embedded Qt panel. Blocks until the window is closed.
int runPanel(int argc, char** argv);

} // namespace zibby::panel

#endif
