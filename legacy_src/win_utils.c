#include "raylib.h" // Must be included BEFORE windows.h

#ifdef _WIN32
#define NOGDI // Avoid GDI conflicts
// Renaming to avoid conflicts with Raylib
#define ShowCursor WinShowCursor
#define CloseWindow WinCloseWindow
#define PlaySound WinPlaySound
#define DrawText WinDrawText
#define LoadImage WinLoadImage
#define LoadBitmap WinLoadBitmap

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
// #include "gui.h" 

// Define Tray Message ID
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_APP_ICON 1001
#define ID_TRAY_EXIT 1002
#define ID_TRAY_RESTORE 1003

static HWND targetWindow = NULL;
static NOTIFYICONDATA notifyIconData;
static WNDPROC originalWndProc = NULL;
static bool restoreRequested = false;
static bool exitRequested = false;

// Custom Window Procedure to intercept Tray events
LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_TRAYICON) {
        if (lParam == WM_LBUTTONUP) {
            // Restore window
            restoreRequested = true;
        } else if (lParam == WM_RBUTTONUP) {
            // Show Context Menu
            POINT curPoint;
            GetCursorPos(&curPoint);
            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING, ID_TRAY_RESTORE, "開啟 (Open)");
            AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, "退出 (Exit)");
            
            // TrackPopupMenu blocks, so we do it
            SetForegroundWindow(hwnd);
            int clicked = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, curPoint.x, curPoint.y, 0, hwnd, NULL);
            if (clicked == ID_TRAY_EXIT) {
                exitRequested = true;
            } else if (clicked == ID_TRAY_RESTORE) {
                restoreRequested = true;
            }
            DestroyMenu(hMenu);
        }
    }
    
    // Call original or default
    if (originalWndProc)
        return CallWindowProc(originalWndProc, hwnd, msg, wParam, lParam);
    else
        return DefWindowProc(hwnd, msg, wParam, lParam);
}

void SystemSleep(int ms) {
    Sleep(ms);
}

void InitTrayIcon() {
    // Get Window Handle (Raylib 5.0 puts it in getWindowHandle, usually accessible via FindWindow if not exposed)
    // Raylib exposes GetWindowHandle() returning void* which is HWND on Windows.
    targetWindow = (HWND)GetWindowHandle();
    
    if (!targetWindow) {
        printf("Failed to get Window Handle.\n");
        return;
    }
    
    // Subclass WndProc
    originalWndProc = (WNDPROC)SetWindowLongPtr(targetWindow, GWLP_WNDPROC, (LONG_PTR)TrayWndProc);
    
    // Setup NotifyIconData
    memset(&notifyIconData, 0, sizeof(NOTIFYICONDATA));
    notifyIconData.cbSize = sizeof(NOTIFYICONDATA);
    notifyIconData.hWnd = targetWindow;
    notifyIconData.uID = ID_TRAY_APP_ICON;
    notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    notifyIconData.uCallbackMessage = WM_TRAYICON;
    notifyIconData.hIcon = LoadIcon(NULL, IDI_APPLICATION); // Use default app icon
    strcpy(notifyIconData.szTip, "Factory Pipeline Manager");
    
    // We don't Add here, we Add when minimizing? Or Add always? 
    // User wants "Minimize to tray". 
    // Usually icon appears only when minimized, or always.
    // Let's add it always for now so user sees it.
    Shell_NotifyIcon(NIM_ADD, &notifyIconData);
}

void RemoveTrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &notifyIconData);
    if (targetWindow && originalWndProc) {
        SetWindowLongPtr(targetWindow, GWLP_WNDPROC, (LONG_PTR)originalWndProc);
    }
}

void MinimizeToTray() {
    if (targetWindow) {
        ShowWindow(targetWindow, SW_HIDE);
    }
}

void RestoreFromTray() {
    if (targetWindow) {
        ShowWindow(targetWindow, SW_SHOW);
        ShowWindow(targetWindow, SW_RESTORE);
        SetForegroundWindow(targetWindow);
    }
}

bool UpdateTray() {
    // This function is polled by main loop
    // Check flags set by WndProc
    
    if (restoreRequested) {
        restoreRequested = false;
        RestoreFromTray();
        return true; // Restored
    }
    
    if (exitRequested) {
        // Signal app to close
        // We can force Raylib close
        exitRequested = false;
        return false; // Should Exit
    }
    
    return true; // Continue running
}

bool IsExitRequested() {
    return exitRequested;
}

#else
#include <unistd.h>
void SystemSleep(int ms) { usleep(ms * 1000); }
void InitTrayIcon() {}
void RemoveTrayIcon() {}
void MinimizeToTray() {}
void RestoreFromTray() {}
bool UpdateTray() { return true; }
bool IsExitRequested() { return false; }
#endif
