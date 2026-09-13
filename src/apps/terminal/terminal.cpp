#include "terminal.h"

#include "../../os/app_registry.h"
#include "../../os/font.h"
#include "../../os/ui.h"

namespace terminal {
    Window* init() {
        Window* window = ui_create_window(60, 60, 800, 450, "Terminal", 0xFF000000, 0xFF808080, 0xFFFFFFFF, nullptr, nullptr);
        return window;
    }
}

REGISTER_APP(terminal);