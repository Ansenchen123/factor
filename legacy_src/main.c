#include "gui.h"
#include "win_utils.h"

int main() {
    InitGUI(); // Initialize Raylib and Data
    // InitTrayIcon(); // DISABLED: Causing Crash
    
    // Main Loop
    while (g_ui.running) {
        
        // Handle Tray Updates (Restore requests, Exit requests)
        // if (!UpdateTray()) {
        //    g_ui.running = false;
        // } else if (IsExitRequested()) {
        //    g_ui.running = false;
        // }

        // Handle Close Events
        if (WindowShouldClose()) {
            // MinimizeToTray();
            // g_ui.minimized_to_tray = true;
            g_ui.running = false; // Direct Exit
        }
        
        /*
        if (g_ui.minimized_to_tray) {
            if (!IsWindowHidden()) {
                 g_ui.minimized_to_tray = false;
            } else {
                SystemSleep(100); 
                continue;
            }
        }
        */
        
        UpdateDrawGUI();
    }

    // RemoveTrayIcon();
    CloseGUI();
    return 0;
}
