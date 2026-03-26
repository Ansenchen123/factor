#ifndef WIN_UTILS_H
#define WIN_UTILS_H

#include <stdbool.h>

void SystemSleep(int ms);
void InitTrayIcon();    // Initializes Tray Icon and Subclassing
void RemoveTrayIcon();  // Removes Tray Icon
void MinimizeToTray();  // Hides window
void RestoreFromTray(); // Shows window
bool UpdateTray();      // Check if restore requested
bool IsExitRequested(); // Check if context menu Exit clicked

#endif
