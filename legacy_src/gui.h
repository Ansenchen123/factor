#ifndef GUI_H
#define GUI_H

#include "raylib.h"
#include "data.h"

// Application States
typedef enum {
    STATE_HOME,
    STATE_LINE_DETAIL
} AppState;

// Colors
#define COL_BG (Color){ 20, 20, 20, 255 }         // Dark Background
#define COL_PANEL (Color){ 40, 40, 45, 255 }      // Panel Gray
#define COL_ACCENT (Color){ 0, 120, 215, 255 }    // Blue Accent
#define COL_ACCENT_HOVER (Color){ 0, 140, 240, 255 }
#define COL_TEXT_MAIN (Color){ 240, 240, 240, 255 }
#define COL_TEXT_SEC (Color){ 180, 180, 180, 255 }
#define COL_CHECKED (Color){ 50, 205, 50, 255 }   // Green
#define COL_UNCHECKED (Color){ 200, 50, 50, 255 } // Red

// Global Font
extern Font g_font;

// UI State
typedef struct {
    AppState state;
    int currentLineIndex;
    bool showAddLineModal;
    bool showAddEquipmentModal;
    bool showAddItemModal;
    
    // Modal Inputs
    char inputBuffer[64];
    char inputPeriodBuffer[16];
    
    // Rename States
    bool showRenameLine;
    bool showRenameEq;
    bool showRenameItem;
    
    int targetLineIdx;
    int targetEqIdx;
    int targetItemIdx;
    
    // Delete Confirmation
    bool showDeleteConfirm;
    int deleteType; // 1=Line, 2=Eq, 3=Item
    
    // Tray/Window State
    bool running;
    bool minimized_to_tray;
} UIState;

extern UIState g_ui;
// ...
// Custom Button implementation remains ...

// Functions
void InitGUI();
void UpdateDrawGUI();
void CloseGUI();
void SystemSleep(int ms);

#endif // GUI_H
