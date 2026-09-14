#include "calculator.h"

#include "../../os/app_registry.h"
#include "../../os/font.h"
#include "../../os/ui.h"

namespace calculator {
    Window* init() {
        Window* window = ui_create_window(60, 60, 800, 450, "Calculator", 0x50FF0000, 0xFF808080, 0xFFFFFFFF, nullptr, nullptr);
        return window;
    }
}

REGISTER_APP(calculator);