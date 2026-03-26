#include <cstdlib>

float TextToFloat(const char* text) {
    return static_cast<float>(std::strtod(text, nullptr));
}

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
