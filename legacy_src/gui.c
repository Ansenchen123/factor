#include "gui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include "raygui.h"

// Global
UIState g_ui;
Font g_font;

// Scroll vars
float g_scrollHomeY = 0;
float g_scrollDetailY = 0;

// Wrappers
bool GuiButtonCustom(Rectangle bounds, const char* text) {
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    bool clicked = false;
    
    if (hovered) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) DrawRectangleRec(bounds, Fade(COL_ACCENT, 0.8f));
        else DrawRectangleRec(bounds, COL_ACCENT_HOVER);
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) clicked = true;
    } else {
        DrawRectangleRec(bounds, COL_PANEL);
    }
    
    DrawRectangleLinesEx(bounds, 1, COL_ACCENT);
    
    // Center Text
    Vector2 textSize = MeasureTextEx(g_font, text, 28, 1);
    DrawTextEx(g_font, text, (Vector2){ bounds.x + (bounds.width - textSize.x)/2, bounds.y + (bounds.height - textSize.y)/2 }, 28, 1, COL_TEXT_MAIN);
    
    return clicked;
}

void DrawModalOverlay() {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.7f));
}

void InitGUI() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(1024, 768, "Factory Pipeline Manager");
    SetTargetFPS(60);
    
    // Try local font (BOLD)
    if (FileExists("data/font_bold.otf")) { 
        printf("DEBUG: Found font file. Preparing to load...\n"); fflush(stdout);
        
        int count = 0;
        // Allocate buffer (Limit to ~25k chars to fit in 4K texture at size 28)
        int* codepoints = (int*)malloc(sizeof(int) * 32000); 
        
        // 1. Basic ASCII & Latin
        for (int i = 32; i <= 255; i++) codepoints[count++] = i;
        
        // 2. General Punctuation & Symbols
        for (int i = 0x2000; i <= 0x206F; i++) codepoints[count++] = i;
        for (int i = 0x3000; i <= 0x303F; i++) codepoints[count++] = i;
        
        // 3. Fullwidth ASCII
        for (int i = 0xFF00; i <= 0xFFEF; i++) codepoints[count++] = i;
        
        // 4. CJK Unified Ideographs (Main Block)
        // 0x4E00 (19968) to 0x9FFF (40959) is ~21k chars.
        // Total so far < 1000. Total with CJK ~ 22000.
        // At size 26: 26*26 * 22000 ~= 15M pixels. Fits in 4096*4096 (16.7M).
        for (int i = 0x4E00; i <= 0x9FFF; i++) codepoints[count++] = i;
        
        printf("DEBUG: Loading %d codepoints at size 26...\n", count); fflush(stdout);
        
        // Reduce size to 26 to ensure it fits in texture
        g_font = LoadFontEx("data/font_bold.otf", 26, codepoints, count); 
        free(codepoints);
        
        printf("DEBUG: Font loaded. Texture ID: %d\n", g_font.texture.id); fflush(stdout);
    } else {
        printf("DEBUG: Font file missing. Using default.\n"); fflush(stdout);
        g_font = GetFontDefault();
    }
    
    GuiSetFont(g_font); 
    GuiSetStyle(DEFAULT, TEXT_SIZE, 22); 
    
    memset(&g_ui, 0, sizeof(UIState));
    g_ui.state = STATE_HOME;
    g_ui.running = true;
    
    InitData();
    LoadData();
}

bool HandleScroll(float* scrollY, float contentHeight, float viewHeight, float x, float y, float w, float h) {
    if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){x, y, w, h})) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            *scrollY += wheel * 40;
            if (*scrollY > 0) *scrollY = 0;
            if (contentHeight > viewHeight) {
                if (*scrollY < -(contentHeight - viewHeight)) *scrollY = -(contentHeight - viewHeight);
            } else {
                *scrollY = 0;
            }
            return true;
        }
    }
    return false;
}

void DrawSimulationPanel() {
    DrawRectangle(0, 0, GetScreenWidth(), 50, (Color){30, 30, 30, 255});
    char dateStr[32];
    GetCurrentDateStr(dateStr);
    char label[64];
    sprintf(label, "現在日期: %s %s", dateStr, g_appData.is_simulating ? "(模擬)" : "");
    DrawTextEx(g_font, label, (Vector2){ 20, 12 }, 24, 1, g_appData.is_simulating ? ORANGE : WHITE);
    
    bool sim = g_appData.is_simulating;
    GuiSetStyle(CHECKBOX, TEXT_PADDING, 10);
    if (GuiCheckBox((Rectangle){ 450, 10, 30, 30 }, "測試模式", &sim)) {
        ToggleSimulation(sim);
    }
    
    if (g_appData.is_simulating) {
        int x = 650;
        GuiSpinner((Rectangle){ x, 10, 100, 30 }, NULL, &g_appData.sim_year, 2000, 2100, true);
        x += 110;
        GuiSpinner((Rectangle){ x, 10, 60, 30 }, NULL, &g_appData.sim_month, 1, 12, true);
        x += 70;
        GuiSpinner((Rectangle){ x, 10, 60, 30 }, NULL, &g_appData.sim_day, 1, 31, true);
        SetSimulationDate(g_appData.sim_year, g_appData.sim_month, g_appData.sim_day);
    }
}

void DrawPendingDashboard(float yStart) {
    DrawLineEx((Vector2){0, yStart}, (Vector2){(float)GetScreenWidth(), yStart}, 2, COL_ACCENT);
    DrawTextEx(g_font, "【維護儀表板】(Dashboard)", (Vector2){ 20, yStart + 10 }, 28, 1, COL_TEXT_MAIN);
    
    if (GuiButtonCustom((Rectangle){ 700, yStart + 10, 150, 35 }, "重新掃描")) CheckMaintenanceStatus();
    
    float y = yStart + 60;
    char today[11];
    GetCurrentDateStr(today);
    
    int overdueCount = 0;
    int todayCount = 0;
    float col1X = 40;
    float col2X = GetScreenWidth()/2 + 20;
    
    DrawTextEx(g_font, "逾期項目 (Overdue)", (Vector2){ col1X, y }, 24, 1, COL_UNCHECKED);
    DrawTextEx(g_font, "今日待辦 (Due Today)", (Vector2){ col2X, y }, 24, 1, (Color){ 255, 200, 0, 255 });
    
    y += 40;
    float yOver = y;
    float yToday = y;
    
    for (int i = 0; i < g_appData.line_count; i++) {
        for (int j = 0; j < g_appData.lines[i].equipment_count; j++) {
            for (int k = 0; k < g_appData.lines[i].equipments[j].item_count; k++) {
                MaintenanceItem* item = &g_appData.lines[i].equipments[j].items[k];
                if (!item->is_checked_today) {
                    int daysDiff = DaysBetween(item->last_checked_date, today);
                    if (daysDiff > item->period_days) {
                         int overdueDays = daysDiff - item->period_days;
                         char buf[256];
                         sprintf(buf, "%s-%s-%s (逾期:%d天)", g_appData.lines[i].name, g_appData.lines[i].equipments[j].name, item->name, overdueDays);
                         DrawTextEx(g_font, buf, (Vector2){ col1X, yOver }, 22, 1, WHITE);
                         yOver += 30;
                         overdueCount++;
                    } else if (daysDiff == item->period_days) {
                         char buf[256];
                         sprintf(buf, "%s-%s-%s (週期:%d天)", g_appData.lines[i].name, g_appData.lines[i].equipments[j].name, item->name, item->period_days);
                         DrawTextEx(g_font, buf, (Vector2){ col2X, yToday }, 22, 1, WHITE);
                         yToday += 30;
                         todayCount++;
                    }
                }
            }
        }
    }
    
    if (overdueCount == 0) DrawTextEx(g_font, "無逾期項目", (Vector2){ col1X, y }, 22, 1, GRAY);
    if (todayCount == 0) DrawTextEx(g_font, "今日已完成或無項目", (Vector2){ col2X, y }, 22, 1, GRAY);
}

void DrawHome() {
    float topOffset = 40; 
    int linesRows = (g_appData.line_count + 2) / 3;
    float linesH = linesRows * 180 + 200; 
    float totalH = linesH + 500; 
    HandleScroll(&g_scrollHomeY, totalH, GetScreenHeight() - topOffset, 0, topOffset, GetScreenWidth(), GetScreenHeight());
    
    BeginScissorMode(0, topOffset, GetScreenWidth(), GetScreenHeight() - topOffset);
    float effectiveY = topOffset + g_scrollHomeY;
    
    DrawTextEx(g_font, "產線管理系統 (Production Lines)", (Vector2){ 40, effectiveY + 40 }, 40, 1, COL_TEXT_MAIN);

    float y = effectiveY + 100;
    float cardWidth = 250;
    float cardHeight = 150;
    float gap = 30;
    
    for (int i = 0; i < g_appData.line_count; i++) {
        Rectangle rect = { 40 + (i % 3) * (cardWidth + gap), y + (i / 3) * (cardHeight + gap), cardWidth, cardHeight };
        
        DrawRectangleRec(rect, COL_PANEL);
        DrawRectangleLinesEx(rect, 1, COL_TEXT_SEC);
        DrawTextEx(g_font, g_appData.lines[i].name, (Vector2){ rect.x + 15, rect.y + 15 }, 28, 1, COL_TEXT_MAIN);
        
        if (GuiButtonCustom((Rectangle){ rect.x + 180, rect.y + 10, 60, 25 }, "改名")) {
            g_ui.showRenameLine = true;
            g_ui.targetLineIdx = i;
            strcpy(g_ui.inputBuffer, g_appData.lines[i].name);
        }
        if (GuiButtonCustom((Rectangle){ rect.x + 180, rect.y + 40, 60, 25 }, "複製")) {
            DuplicateLine(i);
        }
        if (GuiButtonCustom((Rectangle){ rect.x + 180, rect.y + 70, 60, 25 }, "刪除")) {
            g_ui.showDeleteConfirm = true;
            g_ui.deleteType = 1; 
            g_ui.targetLineIdx = i;
        }
        
        char countStr[32];
        sprintf(countStr, "設備數: %d", g_appData.lines[i].equipment_count);
        DrawTextEx(g_font, countStr, (Vector2){ rect.x + 15, rect.y + 60 }, 22, 1, COL_TEXT_SEC);
        
        bool btnClicked = CheckCollisionPointRec(GetMousePosition(), (Rectangle){rect.x + 180, rect.y + 10, 60, 85});
        if (!btnClicked && CheckCollisionPointRec(GetMousePosition(), rect)) {
            DrawRectangleLinesEx(rect, 3, COL_ACCENT);
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                g_ui.currentLineIndex = i;
                g_ui.state = STATE_LINE_DETAIL;
                g_scrollDetailY = 0;
            }
        }
    }
    
    Rectangle addBtn = { 40, y + linesRows * (cardHeight + gap) + 40, 200, 60 };
    if (GuiButtonCustom(addBtn, "+ 新增產線")) {
        g_ui.showAddLineModal = true;
        g_ui.inputBuffer[0] = '\0';
    }
    
    DrawPendingDashboard(addBtn.y + 100);
    EndScissorMode();
}

void DrawLineDetail() {
    if (g_ui.currentLineIndex < 0) return;
    ProductionLine* line = &g_appData.lines[g_ui.currentLineIndex];
    float topOffset = 40;
    
    if (GuiButtonCustom((Rectangle){ 20, 60, 80, 30 }, "< 返回")) {
        g_ui.state = STATE_HOME;
        g_ui.currentLineIndex = -1;
        return;
    }
    DrawTextEx(g_font, line->name, (Vector2){ 120, 60 }, 32, 1, COL_TEXT_MAIN);
    
    if (GuiButtonCustom((Rectangle){ 800, 60, 150, 40 }, "+ 新增設備")) {
        g_ui.showAddEquipmentModal = true;
        g_ui.inputBuffer[0] = '\0';
    }
    
    float contentY = 120;
    float estHeight = line->equipment_count * 220 + 200; 
    bool localScroll = false;
    
    BeginScissorMode(0, contentY, GetScreenWidth(), GetScreenHeight() - contentY);
    float y = contentY + g_scrollDetailY;
    
    for (int i = 0; i < line->equipment_count; i++) {
        Equipment* eq = &line->equipments[i];
        Rectangle eqRect = {40, y, 900, 200}; 
        DrawRectangleRec(eqRect, COL_PANEL);
        DrawRectangleLinesEx(eqRect, 1, COL_TEXT_SEC);
        DrawTextEx(g_font, eq->name, (Vector2){ 60, y + 15 }, 28, 1, COL_ACCENT);
        
        if (GuiButtonCustom((Rectangle){ 750, y + 15, 60, 25 }, "改名")) {
            g_ui.showRenameEq = true;
            g_ui.targetEqIdx = i;
            g_ui.targetLineIdx = g_ui.currentLineIndex;
            strcpy(g_ui.inputBuffer, eq->name);
        }
        if (GuiButtonCustom((Rectangle){ 820, y + 15, 60, 25 }, "刪除")) {
            g_ui.showDeleteConfirm = true;
            g_ui.deleteType = 2; 
            g_ui.targetLineIdx = g_ui.currentLineIndex;
            g_ui.targetEqIdx = i;
        }
        
        // Item List Area (Nested Scroll)
        Rectangle listRect = {60, y + 60, 800, 100};
        DrawRectangleRec(listRect, (Color){30,30,30,255});
        float itemContentH = eq->item_count * 35.0f;
        float eqScroll = (float)eq->scroll_offset;
        if (HandleScroll(&eqScroll, itemContentH, listRect.height, listRect.x, listRect.y, listRect.width, listRect.height)) {
             eq->scroll_offset = (int)eqScroll;
             localScroll = true;
        }
        
        BeginScissorMode(listRect.x, listRect.y, listRect.width, listRect.height);
        float itemY = listRect.y + eq->scroll_offset + 5;
        
        for (int k = 0; k < eq->item_count; k++) {
            MaintenanceItem* item = &eq->items[k];
            char periodStr[32];
            sprintf(periodStr, "[%d天]", item->period_days);
            DrawTextEx(g_font, periodStr, (Vector2){ 70, itemY }, 20, 1, COL_TEXT_SEC);
            
            // Name Area (Extended)
            DrawTextEx(g_font, item->name, (Vector2){ 150, itemY }, 20, 1, COL_TEXT_MAIN);
             
            // Buttons shifted right
            Rectangle renameRect = {400, itemY, 40, 20};
            if (itemY > listRect.y - 20 && itemY < listRect.y + listRect.height) {
                 if (GuiButtonCustom(renameRect, "改")) {
                    g_ui.showRenameItem = true;
                    g_ui.targetLineIdx = g_ui.currentLineIndex;
                    g_ui.targetEqIdx = i;
                    g_ui.targetItemIdx = k;
                    strcpy(g_ui.inputBuffer, item->name);
                 }
            }
            
            Rectangle delItemRect = {450, itemY, 40, 20};
            if (itemY > listRect.y - 20 && itemY < listRect.y + listRect.height) {
                 if (GuiButtonCustom(delItemRect, "刪")) { 
                    g_ui.showDeleteConfirm = true;
                    g_ui.deleteType = 3; 
                    g_ui.targetLineIdx = g_ui.currentLineIndex;
                    g_ui.targetEqIdx = i;
                    g_ui.targetItemIdx = k;
                 }
            }

            Rectangle checkRect = { 510, itemY, 24, 24 };
            Color checkCol = item->is_checked_today ? COL_CHECKED : COL_PANEL;
            DrawRectangleRec(checkRect, checkCol);
            DrawRectangleLinesEx(checkRect, 1, COL_TEXT_SEC);
             if (item->is_checked_today) {
                DrawTextEx(g_font, "v", (Vector2){ checkRect.x + 4, checkRect.y - 4 }, 24, 1, WHITE);
            }
            if (itemY > listRect.y - 20 && itemY < listRect.y + listRect.height) {
                if (CheckCollisionPointRec(GetMousePosition(), checkRect)) {
                    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                        SetMaintenanceItemStatus(g_ui.currentLineIndex, i, k, !item->is_checked_today);
                    }
                }
            }
            
            char lastStr[64];
            sprintf(lastStr, "上次維護: %s", item->last_checked_date);
            DrawTextEx(g_font, lastStr, (Vector2){ 550, itemY }, 18, 1, GRAY);
            itemY += 35;
        }
        EndScissorMode();
        
        // Add Item Button - Moved to bottom right of card area
        Rectangle btnRect = { 760, y + 165, 100, 30 };
        if (GuiButtonCustom(btnRect, "+ 新增項目")) {
             g_ui.targetEqIdx = i;
             g_ui.showAddItemModal = true;
             g_ui.inputBuffer[0] = '\0';
             strcpy(g_ui.inputPeriodBuffer, "1"); 
        }
        y += 220; 
    }
    EndScissorMode();
    
    if (!localScroll) {
        HandleScroll(&g_scrollDetailY, estHeight, 600, 0, 120, GetScreenWidth(), GetScreenHeight());
    }
}

void UpdateDrawGUI() {
    CheckMaintenanceStatus(); 
    bool anyModal = g_ui.showAddLineModal || g_ui.showAddEquipmentModal || g_ui.showAddItemModal || g_ui.showRenameLine || g_ui.showRenameEq || g_ui.showRenameItem || g_ui.showDeleteConfirm;
                    
    if (anyModal) {
        if (!g_ui.showDeleteConfirm) { // Don't type into delete confirm (no inputs)
            int key = GetCharPressed();
            while (key > 0) {
                 int byteSize = 0;
                 const char* charUtf8 = CodepointToUTF8(key, &byteSize);
                 char* targetBuffer = g_ui.inputBuffer;
                 if (g_ui.showAddItemModal && GetMouseY() > 370 && GetMouseY() < 420) targetBuffer = g_ui.inputPeriodBuffer;
                 if (strlen(targetBuffer) + byteSize < 63) strcat(targetBuffer, charUtf8);
                key = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE)) {
                 char* targetBuffer = g_ui.inputBuffer;
                 if (g_ui.showAddItemModal && GetMouseY() > 370 && GetMouseY() < 420) targetBuffer = g_ui.inputPeriodBuffer;
                int len = strlen(targetBuffer);
                if (len > 0) {
                    targetBuffer[len-1] = '\0'; // Remove last byte
                    len--;
                    while (len > 0 && (targetBuffer[len] & 0xC0) == 0x80) {
                         targetBuffer[len-1] = '\0';
                         len--;
                    }
                }
            }
        }
    }

    BeginDrawing();
    ClearBackground(COL_BG);
    DrawSimulationPanel();
    
    if (g_ui.state == STATE_HOME) {
        DrawHome();
    } else if (g_ui.state == STATE_LINE_DETAIL) {
        DrawLineDetail();
    }
    
    if (g_ui.showAddLineModal) {
        DrawModalOverlay();
        DrawRectangle(300, 300, 400, 200, COL_PANEL);
        DrawTextEx(g_font, "輸入產線名稱:", (Vector2){ 320, 320 }, 24, 1, WHITE);
        DrawRectangle(320, 360, 360, 40, BLACK);
        DrawTextEx(g_font, g_ui.inputBuffer, (Vector2){ 330, 365 }, 24, 1, WHITE);
        if (GuiButtonCustom((Rectangle){ 420, 440, 80, 40 }, "確定")) {
            AddLine(g_ui.inputBuffer);
            g_ui.showAddLineModal = false;
        }
        if (GuiButtonCustom((Rectangle){ 520, 440, 80, 40 }, "取消")) g_ui.showAddLineModal = false;
    }
    else if (g_ui.showAddEquipmentModal) {
        DrawModalOverlay();
        DrawRectangle(300, 300, 400, 200, COL_PANEL);
        DrawTextEx(g_font, "輸入設備名稱:", (Vector2){ 320, 320 }, 24, 1, WHITE);
        DrawRectangle(320, 360, 360, 40, BLACK);
        DrawTextEx(g_font, g_ui.inputBuffer, (Vector2){ 330, 365 }, 24, 1, WHITE);
        if (GuiButtonCustom((Rectangle){ 420, 440, 80, 40 }, "確定")) {
            AddEquipment(g_ui.currentLineIndex, g_ui.inputBuffer);
            g_ui.showAddEquipmentModal = false;
        }
        if (GuiButtonCustom((Rectangle){ 520, 440, 80, 40 }, "取消")) g_ui.showAddEquipmentModal = false;
    }
    else if (g_ui.showAddItemModal) {
        DrawModalOverlay();
        DrawRectangle(300, 250, 400, 300, COL_PANEL);
        DrawTextEx(g_font, "項目名稱:", (Vector2){ 320, 270 }, 20, 1, WHITE);
        DrawRectangle(320, 300, 360, 30, BLACK);
        DrawTextEx(g_font, g_ui.inputBuffer, (Vector2){ 325, 305 }, 20, 1, WHITE);
        DrawTextEx(g_font, "維護週期 (天):", (Vector2){ 320, 350 }, 20, 1, WHITE); 
        DrawRectangle(320, 380, 100, 30, BLACK);
        DrawTextEx(g_font, g_ui.inputPeriodBuffer, (Vector2){ 325, 385 }, 20, 1, WHITE);
        if (GuiButtonCustom((Rectangle){ 420, 480, 80, 40 }, "確定")) {
            int p = atoi(g_ui.inputPeriodBuffer);
            if (p <= 0) p = 1;
            AddMaintenanceItem(g_ui.currentLineIndex, g_ui.targetEqIdx, g_ui.inputBuffer, p);
            g_ui.showAddItemModal = false;
        }
         if (GuiButtonCustom((Rectangle){ 520, 480, 80, 40 }, "取消")) g_ui.showAddItemModal = false;
    }
    else if (g_ui.showRenameLine || g_ui.showRenameEq || g_ui.showRenameItem) {
        DrawModalOverlay();
        DrawRectangle(300, 300, 400, 200, COL_PANEL);
        DrawTextEx(g_font, "更改名稱:", (Vector2){ 320, 320 }, 24, 1, WHITE);
        DrawRectangle(320, 360, 360, 40, BLACK);
        DrawTextEx(g_font, g_ui.inputBuffer, (Vector2){ 330, 365 }, 24, 1, WHITE);
        if (GuiButtonCustom((Rectangle){ 420, 440, 80, 40 }, "確定")) {
            if (g_ui.showRenameLine) UpdateLineName(g_ui.targetLineIdx, g_ui.inputBuffer);
            else if (g_ui.showRenameEq) UpdateEquipmentName(g_ui.targetLineIdx, g_ui.targetEqIdx, g_ui.inputBuffer);
            else if (g_ui.showRenameItem) UpdateItemName(g_ui.targetLineIdx, g_ui.targetEqIdx, g_ui.targetItemIdx, g_ui.inputBuffer);
            g_ui.showRenameLine = false; g_ui.showRenameEq = false; g_ui.showRenameItem = false;
        }
        if (GuiButtonCustom((Rectangle){ 520, 440, 80, 40 }, "取消")) {
            g_ui.showRenameLine = false; g_ui.showRenameEq = false; g_ui.showRenameItem = false;
        }
    }
    else if (g_ui.showDeleteConfirm) {
        DrawModalOverlay();
        DrawRectangle(300, 300, 400, 200, COL_PANEL);
        DrawTextEx(g_font, "確定要刪除嗎?", (Vector2){ 320, 320 }, 24, 1, WHITE);
        DrawRectangle(320, 360, 360, 40, BLACK);
        DrawTextEx(g_font, "(此動作無法復原)", (Vector2){ 330, 365 }, 24, 1, RED);
        if (GuiButtonCustom((Rectangle){ 420, 440, 80, 40 }, "是 (Yes)")) {
            if (g_ui.deleteType == 1) DeleteLine(g_ui.targetLineIdx);
            else if (g_ui.deleteType == 2) DeleteEquipment(g_ui.targetLineIdx, g_ui.targetEqIdx);
            else if (g_ui.deleteType == 3) DeleteItem(g_ui.targetLineIdx, g_ui.targetEqIdx, g_ui.targetItemIdx);
            g_ui.showDeleteConfirm = false;
            if (g_ui.deleteType == 1) { // If Line deleted, go home
                 g_ui.state = STATE_HOME;
                 g_ui.currentLineIndex = -1;
            }
        }
        if (GuiButtonCustom((Rectangle){ 520, 440, 80, 40 }, "否 (No)")) g_ui.showDeleteConfirm = false;
    }

    EndDrawing();
}

void CloseGUI() {
    CloseWindow();
}
