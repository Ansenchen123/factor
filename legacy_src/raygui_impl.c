#include <stdlib.h>
#include <string.h>

// RayGUI needs this helper
float TextToFloat(const char *text) {
    return (float)strtod(text, NULL);
}

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
