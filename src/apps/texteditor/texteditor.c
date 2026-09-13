#include "texteditor.h"

#include "../../os/app_registry.h"
#include "../../os/graphics.h"
#include "../../os/font.h"

namepsace texteditor {
    Window* init() {
        Window* window = ui_create_window(60, 60, 800, 450, "Texteditor", 0xFF000000, 0xFF808080, 0xFFFFFFFF, nullptr, nullptr);
        return window;
    }
}

REGISTER_APP(texteditor);