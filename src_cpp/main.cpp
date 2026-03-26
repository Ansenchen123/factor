#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>

#include "raygui.h"
#include "raylib.h"

#include "model.hpp"
#include "storage.hpp"

namespace factor {

namespace {

constexpr int kWindowWidth = 1440;
constexpr int kWindowHeight = 900;
constexpr int kWindowMinWidth = 1200;
constexpr int kWindowMinHeight = 760;
constexpr int kInputLimit = 48;
constexpr int kUiTextSize = 18;
constexpr int kMetaTextSize = 15;

Font g_uiFont{};
bool g_hasCustomFont = false;

enum class FocusField {
    None,
    NewLine,
    RenameLine,
    NewEquipment,
    RenameEquipment,
    NewItem,
    RenameItem,
};

struct AppUi {
    int selectedLine = -1;
    int selectedEquipment = -1;
    int selectedItem = -1;
    FocusField focusedField = FocusField::None;
    std::string newLineName;
    std::string renameLineName;
    std::string newEquipmentName;
    std::string renameEquipmentName;
    std::string newItemName;
    std::string renameItemName;
    int newItemPeriod = 30;
    int renameItemPeriod = 30;
    std::string banner;
    double bannerUntil = 0.0;
};

void LoadUiFont(const std::filesystem::path& root) {
    const auto fontPath = root / "data" / "font_bold.otf";
    if (std::filesystem::exists(fontPath)) {
        g_uiFont = LoadFontEx(fontPath.string().c_str(), 32, nullptr, 0);
        if (g_uiFont.texture.id != 0) {
            GuiSetFont(g_uiFont);
            g_hasCustomFont = true;
        }
    }

    if (!g_hasCustomFont) {
        g_uiFont = GetFontDefault();
        GuiSetFont(g_uiFont);
    }

    GuiSetStyle(DEFAULT, TEXT_SIZE, kUiTextSize);
    GuiSetStyle(DEFAULT, TEXT_SPACING, 1);
}

void UnloadUiFont() {
    if (g_hasCustomFont) {
        UnloadFont(g_uiFont);
    }
}

float MeasureUiText(const std::string& text, float fontSize) {
    return MeasureTextEx(g_uiFont, text.c_str(), fontSize, 1.0f).x;
}

std::string FitText(const std::string& text, float fontSize, float maxWidth) {
    if (text.empty() || MeasureUiText(text, fontSize) <= maxWidth) {
        return text;
    }

    std::string trimmed = text;
    while (!trimmed.empty()) {
        trimmed.pop_back();
        const std::string candidate = trimmed + "...";
        if (MeasureUiText(candidate, fontSize) <= maxWidth) {
            return candidate;
        }
    }
    return "...";
}

std::filesystem::path ResolveProjectRoot(char* argv0) {
    std::filesystem::path current = std::filesystem::absolute(argv0).parent_path();
    for (int depth = 0; depth < 4; ++depth) {
        if (std::filesystem::exists(current / "data" / "database.json")) {
            return current;
        }
        if (std::filesystem::exists(current / "CMakeLists.txt")) {
            return current;
        }
        if (!current.has_parent_path()) {
            break;
        }
        current = current.parent_path();
    }
    return std::filesystem::current_path();
}

void SetBanner(AppUi& ui, const std::string& message) {
    ui.banner = message;
    ui.bannerUntil = GetTime() + 3.5;
}

void ResetSimulationToToday(AppData& data) {
    const std::string today = TodayString();
    data.simulation.year = std::stoi(today.substr(0, 4));
    data.simulation.month = std::stoi(today.substr(5, 2));
    data.simulation.day = std::stoi(today.substr(8, 2));
}

void ClampSelections(const AppData& data, AppUi& ui) {
    if (data.lines.empty()) {
        ui.selectedLine = -1;
        ui.selectedEquipment = -1;
        ui.selectedItem = -1;
        return;
    }

    ui.selectedLine = std::clamp(ui.selectedLine, 0, static_cast<int>(data.lines.size()) - 1);
    const auto& line = data.lines[ui.selectedLine];

    if (line.equipment.empty()) {
        ui.selectedEquipment = -1;
        ui.selectedItem = -1;
        return;
    }

    ui.selectedEquipment = std::clamp(ui.selectedEquipment, 0, static_cast<int>(line.equipment.size()) - 1);
    const auto& equipment = line.equipment[ui.selectedEquipment];

    if (equipment.items.empty()) {
        ui.selectedItem = -1;
        return;
    }

    ui.selectedItem = std::clamp(ui.selectedItem, 0, static_cast<int>(equipment.items.size()) - 1);
}

void HandleTextInput(std::string& value, std::size_t maxLength) {
    int key = GetCharPressed();
    while (key > 0) {
        if (key >= 32 && key <= 126 && value.size() < maxLength) {
            value.push_back(static_cast<char>(key));
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && !value.empty()) {
        value.pop_back();
    }
}

void DrawInputBox(Rectangle bounds, std::string& value, FocusField field, AppUi& ui, const std::string& placeholder) {
    const bool active = (ui.focusedField == field);
    if (CheckCollisionPointRec(GetMousePosition(), bounds) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        ui.focusedField = field;
    }

    DrawRectangleRounded(bounds, 0.2f, 8, active ? Color{32, 46, 60, 255} : Color{24, 34, 46, 255});
    DrawRectangleRoundedLines(bounds, 0.2f, 8, 2.0f, active ? Color{74, 154, 255, 255} : Color{58, 78, 98, 255});

    const std::string display = FitText(value.empty() ? placeholder : value, 18.0f, bounds.width - 24.0f);
    const Color textColor = value.empty() ? Color{120, 140, 160, 255} : RAYWHITE;
    DrawText(display.c_str(), static_cast<int>(bounds.x + 12), static_cast<int>(bounds.y + 10), 20, textColor);

    if (active) {
        HandleTextInput(value, kInputLimit);
        if (IsKeyPressed(KEY_ENTER)) {
            ui.focusedField = FocusField::None;
        }
    }
}

void PersistIfAllowed(const AppData& data, const std::filesystem::path& dataFile, AppUi& ui) {
    if (data.simulation.enabled) {
        SetBanner(ui, "Editing is disabled while simulation mode is enabled.");
        return;
    }

    if (!SaveAppData(data, dataFile)) {
        SetBanner(ui, "Failed to save data.");
    }
}

void SyncRenameBuffers(const AppData& data, AppUi& ui) {
    if (ui.selectedLine < 0 || ui.selectedLine >= static_cast<int>(data.lines.size())) {
        return;
    }

    ui.renameLineName = data.lines[ui.selectedLine].name;
    const auto& line = data.lines[ui.selectedLine];

    if (ui.selectedEquipment >= 0 && ui.selectedEquipment < static_cast<int>(line.equipment.size())) {
        ui.renameEquipmentName = line.equipment[ui.selectedEquipment].name;
        const auto& equipment = line.equipment[ui.selectedEquipment];

        if (ui.selectedItem >= 0 && ui.selectedItem < static_cast<int>(equipment.items.size())) {
            ui.renameItemName = equipment.items[ui.selectedItem].name;
            ui.renameItemPeriod = equipment.items[ui.selectedItem].periodDays;
        }
    }
}

Rectangle Panel(float x, float y, float width, float height, const char* title) {
    Rectangle rect{x, y, width, height};
    GuiPanel(rect, title);
    return rect;
}

void DrawHeader(const AppData& data, AppUi& ui) {
    DrawRectangleGradientH(0, 0, GetScreenWidth(), 84, Color{9, 26, 43, 255}, Color{12, 50, 88, 255});
    DrawText("Factor Manager", 24, 18, 34, RAYWHITE);
    DrawText("Factory maintenance tracking desktop tool", 26, 54, 18, Color{187, 214, 239, 255});

    const std::string currentDate = "Current date: " + EffectiveDate(data);
    DrawText(currentDate.c_str(), GetScreenWidth() - 250, 22, 20, Color{230, 241, 255, 255});
    DrawText(data.simulation.enabled ? "Simulation mode" : "Live editing mode",
             GetScreenWidth() - 250, 50, 18,
             data.simulation.enabled ? Color{255, 212, 112, 255} : Color{145, 233, 176, 255});

    if (!ui.banner.empty() && GetTime() <= ui.bannerUntil) {
        DrawRectangleRounded(Rectangle{430, 18, 470, 40}, 0.3f, 8, Color{15, 90, 70, 220});
        DrawText(FitText(ui.banner, 18.0f, 440.0f).c_str(), 444, 28, 18, RAYWHITE);
    }
}

void DrawLinesPanel(AppData& data, AppUi& ui, const std::filesystem::path& dataFile) {
    const Rectangle panel = Panel(20, 100, 280, GetScreenHeight() - 120, "1. Production lines");
    DrawText("Pick a line, then manage its equipment.", static_cast<int>(panel.x + 12), static_cast<int>(panel.y + 44), 16, Color{180, 190, 200, 255});
    float y = panel.y + 72;

    for (int index = 0; index < static_cast<int>(data.lines.size()); ++index) {
        Rectangle selectRect{panel.x + 12, y, 150, 40};
        Rectangle duplicateRect{panel.x + 170, y, 54, 40};
        Rectangle deleteRect{panel.x + 232, y, 36, 40};
        const std::string label = FitText(data.lines[index].name, 18.0f, 126.0f);

        if (GuiButton(selectRect, label.c_str())) {
            ui.selectedLine = index;
            ui.selectedEquipment = data.lines[index].equipment.empty() ? -1 : 0;
            ui.selectedItem = (ui.selectedEquipment >= 0 && !data.lines[index].equipment[0].items.empty()) ? 0 : -1;
            SyncRenameBuffers(data, ui);
        }

        DrawText(TextFormat("%i eq.", static_cast<int>(data.lines[index].equipment.size())),
                 static_cast<int>(selectRect.x), static_cast<int>(selectRect.y + 43), 14, Color{160, 170, 180, 255});

        if (GuiButton(duplicateRect, "Copy") && !data.simulation.enabled) {
            ProductionLine copy = data.lines[index];
            copy.name += " Copy";
            data.lines.push_back(copy);
            PersistIfAllowed(data, dataFile, ui);
        }

        if (GuiButton(deleteRect, "X") && !data.simulation.enabled) {
            data.lines.erase(data.lines.begin() + index);
            ClampSelections(data, ui);
            SyncRenameBuffers(data, ui);
            PersistIfAllowed(data, dataFile, ui);
            return;
        }

        if (ui.selectedLine == index) {
            DrawRectangleLinesEx(selectRect, 2, Color{82, 168, 255, 255});
        }

        y += 60;
    }

    DrawText("Add line", static_cast<int>(panel.x + 12), static_cast<int>(panel.y + panel.height - 154), 18, RAYWHITE);
    DrawInputBox(Rectangle{panel.x + 12, panel.y + panel.height - 124, panel.width - 24, 40}, ui.newLineName, FocusField::NewLine, ui, "New line name");
    if (GuiButton(Rectangle{panel.x + 12, panel.y + panel.height - 76, 90, 36}, "Add line") && !ui.newLineName.empty()) {
        if (!data.simulation.enabled) {
            data.lines.push_back(ProductionLine{ui.newLineName, {}});
            ui.selectedLine = static_cast<int>(data.lines.size()) - 1;
            ui.selectedEquipment = -1;
            ui.selectedItem = -1;
            ui.newLineName.clear();
            SyncRenameBuffers(data, ui);
            PersistIfAllowed(data, dataFile, ui);
        } else {
            PersistIfAllowed(data, dataFile, ui);
        }
    }

    DrawInputBox(Rectangle{panel.x + 110, panel.y + panel.height - 76, panel.width - 122, 36}, ui.renameLineName, FocusField::RenameLine, ui, "Rename selected line");
    if (GuiButton(Rectangle{panel.x + 12, panel.y + panel.height - 34, 90, 32}, "Save") &&
        ui.selectedLine >= 0 && ui.selectedLine < static_cast<int>(data.lines.size()) && !ui.renameLineName.empty()) {
        if (!data.simulation.enabled) {
            data.lines[ui.selectedLine].name = ui.renameLineName;
            PersistIfAllowed(data, dataFile, ui);
        } else {
            PersistIfAllowed(data, dataFile, ui);
        }
    }
}

void DrawEquipmentPanel(AppData& data, AppUi& ui, const std::filesystem::path& dataFile) {
    const Rectangle panel = Panel(320, 100, 360, GetScreenHeight() - 120, "2. Equipment");
    if (ui.selectedLine < 0 || ui.selectedLine >= static_cast<int>(data.lines.size())) {
        DrawText("Select a production line to unlock equipment.", 336, 150, 18, Color{180, 190, 200, 255});
        return;
    }

    auto& line = data.lines[ui.selectedLine];
    DrawText(FitText(line.name, 20.0f, panel.width - 24).c_str(), static_cast<int>(panel.x + 12), static_cast<int>(panel.y + 46), 20, RAYWHITE);
    float y = panel.y + 84;

    for (int index = 0; index < static_cast<int>(line.equipment.size()); ++index) {
        Rectangle selectRect{panel.x + 12, y, 290, 40};
        Rectangle deleteRect{panel.x + 310, y, 36, 40};
        const std::string label = FitText(line.equipment[index].name, 18.0f, 266.0f);

        if (GuiButton(selectRect, label.c_str())) {
            ui.selectedEquipment = index;
            ui.selectedItem = line.equipment[index].items.empty() ? -1 : 0;
            SyncRenameBuffers(data, ui);
        }

        DrawText(TextFormat("%i items", static_cast<int>(line.equipment[index].items.size())),
                 static_cast<int>(selectRect.x), static_cast<int>(selectRect.y + 43), 14, Color{160, 170, 180, 255});

        if (GuiButton(deleteRect, "X") && !data.simulation.enabled) {
            line.equipment.erase(line.equipment.begin() + index);
            ClampSelections(data, ui);
            SyncRenameBuffers(data, ui);
            PersistIfAllowed(data, dataFile, ui);
            return;
        }

        if (ui.selectedEquipment == index) {
            DrawRectangleLinesEx(selectRect, 2, Color{82, 168, 255, 255});
        }

        y += 60;
    }

    DrawText("Add equipment", static_cast<int>(panel.x + 12), static_cast<int>(panel.y + panel.height - 154), 18, RAYWHITE);
    DrawInputBox(Rectangle{panel.x + 12, panel.y + panel.height - 124, panel.width - 24, 40}, ui.newEquipmentName, FocusField::NewEquipment, ui, "New equipment name");
    if (GuiButton(Rectangle{panel.x + 12, panel.y + panel.height - 76, 124, 36}, "Add equipment") && !ui.newEquipmentName.empty()) {
        if (!data.simulation.enabled) {
            line.equipment.push_back(Equipment{ui.newEquipmentName, {}});
            ui.selectedEquipment = static_cast<int>(line.equipment.size()) - 1;
            ui.selectedItem = -1;
            ui.newEquipmentName.clear();
            SyncRenameBuffers(data, ui);
            PersistIfAllowed(data, dataFile, ui);
        } else {
            PersistIfAllowed(data, dataFile, ui);
        }
    }

    DrawInputBox(Rectangle{panel.x + 146, panel.y + panel.height - 76, panel.width - 158, 36}, ui.renameEquipmentName, FocusField::RenameEquipment, ui, "Rename selected equipment");
    if (GuiButton(Rectangle{panel.x + 12, panel.y + panel.height - 34, 124, 32}, "Save name") &&
        ui.selectedEquipment >= 0 && ui.selectedEquipment < static_cast<int>(line.equipment.size()) && !ui.renameEquipmentName.empty()) {
        if (!data.simulation.enabled) {
            line.equipment[ui.selectedEquipment].name = ui.renameEquipmentName;
            PersistIfAllowed(data, dataFile, ui);
        } else {
            PersistIfAllowed(data, dataFile, ui);
        }
    }
}

void DrawItemsPanel(AppData& data, AppUi& ui, const std::filesystem::path& dataFile) {
    const Rectangle panel = Panel(700, 100, 540, GetScreenHeight() - 120, "3. Maintenance items");

    if (ui.selectedLine < 0 || ui.selectedLine >= static_cast<int>(data.lines.size()) ||
        ui.selectedEquipment < 0 || ui.selectedEquipment >= static_cast<int>(data.lines[ui.selectedLine].equipment.size())) {
        DrawText("Select equipment to manage maintenance items.", 716, 150, 18, Color{180, 190, 200, 255});
        return;
    }

    auto& equipment = data.lines[ui.selectedLine].equipment[ui.selectedEquipment];
    DrawText(FitText(equipment.name, 20.0f, panel.width - 24).c_str(), static_cast<int>(panel.x + 12), static_cast<int>(panel.y + 46), 20, RAYWHITE);
    float y = panel.y + 84;
    const float visibleBottom = panel.y + panel.height - 176;

    for (int index = 0; index < static_cast<int>(equipment.items.size()) && y < visibleBottom; ++index) {
        auto& item = equipment.items[index];
        const int daysSince = DaysBetween(item.lastCheckedDate, EffectiveDate(data));
        std::string status = item.checkedToday ? "done today" : (daysSince > item.periodDays ? "overdue" : (daysSince == item.periodDays ? "due today" : "scheduled"));
        const std::string title = FitText(item.name, 18.0f, 260.0f);
        const std::string meta = FitText("every " + std::to_string(item.periodDays) + " days | last " + item.lastCheckedDate, 15.0f, 300.0f);

        Rectangle selectRect{panel.x + 12, y, 318, 54};
        Rectangle checkRect{panel.x + 340, y + 7, 86, 40};
        Rectangle deleteRect{panel.x + 436, y + 7, 36, 40};

        if (GuiButton(selectRect, title.c_str())) {
            ui.selectedItem = index;
            SyncRenameBuffers(data, ui);
        }

        DrawText(meta.c_str(), static_cast<int>(selectRect.x), static_cast<int>(selectRect.y + 58), 15, Color{160, 170, 180, 255});
        DrawText(status.c_str(), static_cast<int>(panel.x + panel.width - 110), static_cast<int>(selectRect.y + 15), 16,
                 item.checkedToday ? Color{145, 233, 176, 255} : (daysSince > item.periodDays ? Color{255, 132, 132, 255} :
                 (daysSince == item.periodDays ? Color{255, 212, 112, 255} : Color{180, 190, 200, 255})));

        if (GuiButton(checkRect, item.checkedToday ? "Undo" : "Done")) {
            if (!data.simulation.enabled) {
                if (item.checkedToday) {
                    item.lastCheckedDate = "1970-01-01";
                    item.checkedToday = false;
                } else {
                    item.lastCheckedDate = EffectiveDate(data);
                    item.checkedToday = true;
                }
                RefreshStatuses(data);
                SyncRenameBuffers(data, ui);
                PersistIfAllowed(data, dataFile, ui);
            } else {
                PersistIfAllowed(data, dataFile, ui);
            }
        }

        if (GuiButton(deleteRect, "X")) {
            if (!data.simulation.enabled) {
                equipment.items.erase(equipment.items.begin() + index);
                ClampSelections(data, ui);
                SyncRenameBuffers(data, ui);
                PersistIfAllowed(data, dataFile, ui);
                return;
            } else {
                PersistIfAllowed(data, dataFile, ui);
            }
        }

        if (ui.selectedItem == index) {
            DrawRectangleLinesEx(selectRect, 2, Color{82, 168, 255, 255});
        }

        y += 80;
    }

    DrawText("Add item", static_cast<int>(panel.x + 12), static_cast<int>(panel.y + panel.height - 126), 18, RAYWHITE);
    DrawInputBox(Rectangle{panel.x + 12, panel.y + panel.height - 96, 250, 40}, ui.newItemName, FocusField::NewItem, ui, "Maintenance item name");
    GuiSpinner(Rectangle{panel.x + 270, panel.y + panel.height - 96, 100, 40}, "Days", &ui.newItemPeriod, 1, 365, true);
    if (GuiButton(Rectangle{panel.x + 378, panel.y + panel.height - 96, 80, 40}, "Add") && !ui.newItemName.empty()) {
        if (!data.simulation.enabled) {
            equipment.items.push_back(MaintenanceItem{ui.newItemName, ui.newItemPeriod, "1970-01-01", false});
            ui.selectedItem = static_cast<int>(equipment.items.size()) - 1;
            ui.newItemName.clear();
            ui.newItemPeriod = 30;
            SyncRenameBuffers(data, ui);
            PersistIfAllowed(data, dataFile, ui);
        } else {
            PersistIfAllowed(data, dataFile, ui);
        }
    }

    DrawText("Edit selected item", static_cast<int>(panel.x + 12), static_cast<int>(panel.y + panel.height - 52), 18, RAYWHITE);
    DrawInputBox(Rectangle{panel.x + 12, panel.y + panel.height - 24, 250, 40}, ui.renameItemName, FocusField::RenameItem, ui, "Selected item");
    GuiSpinner(Rectangle{panel.x + 270, panel.y + panel.height - 24, 100, 40}, "Days", &ui.renameItemPeriod, 1, 365, true);
    if (GuiButton(Rectangle{panel.x + 378, panel.y + panel.height - 24, 80, 40}, "Save") &&
        ui.selectedItem >= 0 && ui.selectedItem < static_cast<int>(equipment.items.size()) && !ui.renameItemName.empty()) {
        if (!data.simulation.enabled) {
            equipment.items[ui.selectedItem].name = ui.renameItemName;
            equipment.items[ui.selectedItem].periodDays = ui.renameItemPeriod;
            PersistIfAllowed(data, dataFile, ui);
        } else {
            PersistIfAllowed(data, dataFile, ui);
        }
    }
}

void DrawDashboard(AppData& data, AppUi& ui, const std::filesystem::path& dataFile) {
    const Rectangle panel = Panel(1260, 100, GetScreenWidth() - 1280, GetScreenHeight() - 120, "4. Dashboard");
    const DueSummary summary = BuildDueSummary(data);
    int equipmentCount = 0;
    int itemCount = 0;
    for (const auto& line : data.lines) {
        equipmentCount += static_cast<int>(line.equipment.size());
        for (const auto& equipment : line.equipment) {
            itemCount += static_cast<int>(equipment.items.size());
        }
    }

    bool simEnabled = data.simulation.enabled;
    if (GuiCheckBox(Rectangle{panel.x + 16, panel.y + 42, 24, 24}, "Enable simulation mode", &simEnabled)) {
        data.simulation.enabled = simEnabled;
        RefreshStatuses(data);
        SetBanner(ui, data.simulation.enabled ? "Simulation mode enabled. Editing is now locked." : "Simulation mode disabled.");
    }

    DrawText(TextFormat("Lines: %i", static_cast<int>(data.lines.size())), static_cast<int>(panel.x + 16), static_cast<int>(panel.y + 86), 18, Color{180, 190, 200, 255});
    DrawText(TextFormat("Equipment: %i", equipmentCount), static_cast<int>(panel.x + 16), static_cast<int>(panel.y + 110), 18, Color{180, 190, 200, 255});
    DrawText(TextFormat("Items: %i", itemCount), static_cast<int>(panel.x + 16), static_cast<int>(panel.y + 134), 18, Color{180, 190, 200, 255});

    GuiSpinner(Rectangle{panel.x + 16, panel.y + 168, 82, 34}, "Year", &data.simulation.year, 2020, 2100, true);
    GuiSpinner(Rectangle{panel.x + 104, panel.y + 168, 56, 34}, "Month", &data.simulation.month, 1, 12, true);
    GuiSpinner(Rectangle{panel.x + 166, panel.y + 168, 56, 34}, "Day", &data.simulation.day, 1, 31, true);

    if (GuiButton(Rectangle{panel.x + 16, panel.y + 210, panel.width - 32, 34}, "Jump to today")) {
        ResetSimulationToToday(data);
        RefreshStatuses(data);
        SetBanner(ui, "Simulation date reset to today.");
    }

    if (GuiButton(Rectangle{panel.x + 16, panel.y + 252, panel.width - 32, 34}, "Refresh status")) {
        RefreshStatuses(data);
    }

    if (GuiButton(Rectangle{panel.x + 16, panel.y + 294, panel.width - 32, 34}, "Load sample")) {
        if (!data.simulation.enabled) {
            data = BuildSampleData();
            RefreshStatuses(data);
            ui.selectedLine = 0;
            ui.selectedEquipment = 0;
            ui.selectedItem = 0;
            SyncRenameBuffers(data, ui);
            PersistIfAllowed(data, dataFile, ui);
            SetBanner(ui, "Sample dataset loaded.");
        } else {
            PersistIfAllowed(data, dataFile, ui);
        }
    }

    DrawText("Due today", static_cast<int>(panel.x + 16), static_cast<int>(panel.y + 346), 22, RAYWHITE);
    float y = panel.y + 378;
    if (summary.dueToday.empty()) {
        DrawText("Nothing is due today.", static_cast<int>(panel.x + 16), static_cast<int>(y), 18, Color{180, 190, 200, 255});
    } else {
        for (const auto& label : summary.dueToday) {
            if (y > panel.y + panel.height - 170) break;
            DrawText(FitText(label, 16.0f, panel.width - 32).c_str(), static_cast<int>(panel.x + 16), static_cast<int>(y), 16, Color{252, 210, 110, 255});
            y += 22;
        }
    }

    DrawText("Overdue", static_cast<int>(panel.x + 16), static_cast<int>(panel.y + panel.height - 150), 22, RAYWHITE);
    y = panel.y + panel.height - 116;
    if (summary.overdue.empty()) {
        DrawText("No overdue items.", static_cast<int>(panel.x + 16), static_cast<int>(y), 18, Color{180, 190, 200, 255});
    } else {
        for (const auto& label : summary.overdue) {
            if (y > panel.y + panel.height - 20) break;
            DrawText(FitText(label, 16.0f, panel.width - 32).c_str(), static_cast<int>(panel.x + 16), static_cast<int>(y), 16, Color{255, 130, 120, 255});
            y += 22;
        }
    }
}

}  // namespace

}  // namespace factor

int main(int argc, char** argv) {
    using namespace factor;

    (void)argc;
    const std::filesystem::path root = ResolveProjectRoot((argv != nullptr && argv[0] != nullptr) ? argv[0] : const_cast<char*>("."));
    const std::filesystem::path dataFile = root / "data" / "database.json";

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(kWindowWidth, kWindowHeight, "Factor Manager");
    SetWindowMinSize(kWindowMinWidth, kWindowMinHeight);
    SetTargetFPS(60);
    LoadUiFont(root);

    AppData data = LoadAppData(dataFile);
    RefreshStatuses(data);

    AppUi ui;
    if (!data.lines.empty()) {
        ui.selectedLine = 0;
        if (!data.lines[0].equipment.empty()) {
            ui.selectedEquipment = 0;
            if (!data.lines[0].equipment[0].items.empty()) {
                ui.selectedItem = 0;
            }
        }
    }
    SyncRenameBuffers(data, ui);

    while (!WindowShouldClose()) {
        RefreshStatuses(data);
        ClampSelections(data, ui);

        BeginDrawing();
        ClearBackground(Color{15, 20, 29, 255});

        DrawHeader(data, ui);
        DrawLinesPanel(data, ui, dataFile);
        DrawEquipmentPanel(data, ui, dataFile);
        DrawItemsPanel(data, ui, dataFile);
        DrawDashboard(data, ui, dataFile);

        EndDrawing();
    }

    UnloadUiFont();
    CloseWindow();
    return 0;
}
