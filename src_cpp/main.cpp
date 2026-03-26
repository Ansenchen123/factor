#include <algorithm>
#include <filesystem>
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

    const std::string display = value.empty() ? placeholder : value;
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

    if (!ui.banner.empty() && GetTime() <= ui.bannerUntil) {
        DrawRectangleRounded(Rectangle{480, 18, 420, 40}, 0.3f, 8, Color{15, 90, 70, 220});
        DrawText(ui.banner.c_str(), 494, 28, 18, RAYWHITE);
    }
}

void DrawLinesPanel(AppData& data, AppUi& ui, const std::filesystem::path& dataFile) {
    const Rectangle panel = Panel(20, 100, 300, GetScreenHeight() - 120, "Production lines");
    float y = panel.y + 36;

    for (int index = 0; index < static_cast<int>(data.lines.size()); ++index) {
        Rectangle selectRect{panel.x + 12, y, 160, 34};
        Rectangle duplicateRect{panel.x + 180, y, 48, 34};
        Rectangle deleteRect{panel.x + 236, y, 48, 34};

        if (GuiButton(selectRect, data.lines[index].name.c_str())) {
            ui.selectedLine = index;
            ui.selectedEquipment = data.lines[index].equipment.empty() ? -1 : 0;
            ui.selectedItem = (ui.selectedEquipment >= 0 && !data.lines[index].equipment[0].items.empty()) ? 0 : -1;
            SyncRenameBuffers(data, ui);
        }

        if (GuiButton(duplicateRect, "Dup") && !data.simulation.enabled) {
            ProductionLine copy = data.lines[index];
            copy.name += " Copy";
            data.lines.push_back(copy);
            PersistIfAllowed(data, dataFile, ui);
        }

        if (GuiButton(deleteRect, "Del") && !data.simulation.enabled) {
            data.lines.erase(data.lines.begin() + index);
            ClampSelections(data, ui);
            SyncRenameBuffers(data, ui);
            PersistIfAllowed(data, dataFile, ui);
            return;
        }

        if (ui.selectedLine == index) {
            DrawRectangleLinesEx(selectRect, 2, Color{82, 168, 255, 255});
        }

        y += 42;
    }

    DrawText("Add line", static_cast<int>(panel.x + 12), static_cast<int>(panel.y + panel.height - 154), 20, RAYWHITE);
    DrawInputBox(Rectangle{panel.x + 12, panel.y + panel.height - 124, 190, 40}, ui.newLineName, FocusField::NewLine, ui, "New line name");
    if (GuiButton(Rectangle{panel.x + 210, panel.y + panel.height - 124, 74, 40}, "Add") && !ui.newLineName.empty()) {
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

    DrawText("Rename selected line", static_cast<int>(panel.x + 12), static_cast<int>(panel.y + panel.height - 72), 20, RAYWHITE);
    DrawInputBox(Rectangle{panel.x + 12, panel.y + panel.height - 42, 190, 40}, ui.renameLineName, FocusField::RenameLine, ui, "Selected line");
    if (GuiButton(Rectangle{panel.x + 210, panel.y + panel.height - 42, 74, 40}, "Save") &&
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
    const Rectangle panel = Panel(340, 100, 460, 280, "Equipment");
    if (ui.selectedLine < 0 || ui.selectedLine >= static_cast<int>(data.lines.size())) {
        DrawText("Create or select a production line to manage equipment.", 356, 150, 20, Color{180, 190, 200, 255});
        return;
    }

    auto& line = data.lines[ui.selectedLine];
    float y = panel.y + 36;

    for (int index = 0; index < static_cast<int>(line.equipment.size()); ++index) {
        Rectangle selectRect{panel.x + 12, y, 270, 34};
        Rectangle deleteRect{panel.x + 290, y, 48, 34};

        if (GuiButton(selectRect, line.equipment[index].name.c_str())) {
            ui.selectedEquipment = index;
            ui.selectedItem = line.equipment[index].items.empty() ? -1 : 0;
            SyncRenameBuffers(data, ui);
        }

        if (GuiButton(deleteRect, "Del") && !data.simulation.enabled) {
            line.equipment.erase(line.equipment.begin() + index);
            ClampSelections(data, ui);
            SyncRenameBuffers(data, ui);
            PersistIfAllowed(data, dataFile, ui);
            return;
        }

        if (ui.selectedEquipment == index) {
            DrawRectangleLinesEx(selectRect, 2, Color{82, 168, 255, 255});
        }

        y += 42;
    }

    DrawText("Add equipment", static_cast<int>(panel.x + 12), static_cast<int>(panel.y + panel.height - 108), 20, RAYWHITE);
    DrawInputBox(Rectangle{panel.x + 12, panel.y + panel.height - 78, 260, 40}, ui.newEquipmentName, FocusField::NewEquipment, ui, "New equipment name");
    if (GuiButton(Rectangle{panel.x + 282, panel.y + panel.height - 78, 60, 40}, "Add") && !ui.newEquipmentName.empty()) {
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

    DrawInputBox(Rectangle{panel.x + 12, panel.y + panel.height - 30, 260, 40}, ui.renameEquipmentName, FocusField::RenameEquipment, ui, "Rename selected equipment");
    if (GuiButton(Rectangle{panel.x + 282, panel.y + panel.height - 30, 60, 40}, "Save") &&
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
    const Rectangle panel = Panel(340, 400, 700, GetScreenHeight() - 420, "Maintenance items");

    if (ui.selectedLine < 0 || ui.selectedLine >= static_cast<int>(data.lines.size()) ||
        ui.selectedEquipment < 0 || ui.selectedEquipment >= static_cast<int>(data.lines[ui.selectedLine].equipment.size())) {
        DrawText("Select a piece of equipment to manage maintenance items.", 356, 450, 20, Color{180, 190, 200, 255});
        return;
    }

    auto& equipment = data.lines[ui.selectedLine].equipment[ui.selectedEquipment];
    float y = panel.y + 36;
    const float visibleBottom = panel.y + panel.height - 170;

    for (int index = 0; index < static_cast<int>(equipment.items.size()) && y < visibleBottom; ++index) {
        auto& item = equipment.items[index];
        const int daysSince = DaysBetween(item.lastCheckedDate, EffectiveDate(data));
        std::string status = item.checkedToday ? "done today" : (daysSince > item.periodDays ? "overdue" : (daysSince == item.periodDays ? "due today" : "scheduled"));
        std::string label = item.name + " | every " + std::to_string(item.periodDays) + " days | last " + item.lastCheckedDate + " | " + status;

        Rectangle selectRect{panel.x + 12, y, 470, 34};
        Rectangle checkRect{panel.x + 490, y, 90, 34};
        Rectangle deleteRect{panel.x + 588, y, 48, 34};

        if (GuiButton(selectRect, label.c_str())) {
            ui.selectedItem = index;
            SyncRenameBuffers(data, ui);
        }

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

        if (GuiButton(deleteRect, "Del")) {
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

        y += 42;
    }

    DrawText("Add item", static_cast<int>(panel.x + 12), static_cast<int>(panel.y + panel.height - 118), 20, RAYWHITE);
    DrawInputBox(Rectangle{panel.x + 12, panel.y + panel.height - 88, 240, 40}, ui.newItemName, FocusField::NewItem, ui, "Maintenance item name");
    GuiSpinner(Rectangle{panel.x + 260, panel.y + panel.height - 88, 120, 40}, "Days", &ui.newItemPeriod, 1, 365, true);
    if (GuiButton(Rectangle{panel.x + 388, panel.y + panel.height - 88, 70, 40}, "Add") && !ui.newItemName.empty()) {
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

    DrawText("Edit selected item", static_cast<int>(panel.x + 12), static_cast<int>(panel.y + panel.height - 56), 20, RAYWHITE);
    DrawInputBox(Rectangle{panel.x + 12, panel.y + panel.height - 26, 240, 40}, ui.renameItemName, FocusField::RenameItem, ui, "Selected item");
    GuiSpinner(Rectangle{panel.x + 260, panel.y + panel.height - 26, 120, 40}, "Days", &ui.renameItemPeriod, 1, 365, true);
    if (GuiButton(Rectangle{panel.x + 388, panel.y + panel.height - 26, 70, 40}, "Save") &&
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
    const Rectangle panel = Panel(1060, 100, GetScreenWidth() - 1080, GetScreenHeight() - 120, "Dashboard");
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

    GuiSpinner(Rectangle{panel.x + 16, panel.y + 80, 100, 34}, "Year", &data.simulation.year, 2020, 2100, true);
    GuiSpinner(Rectangle{panel.x + 126, panel.y + 80, 70, 34}, "Month", &data.simulation.month, 1, 12, true);
    GuiSpinner(Rectangle{panel.x + 206, panel.y + 80, 70, 34}, "Day", &data.simulation.day, 1, 31, true);

    if (GuiButton(Rectangle{panel.x + 286, panel.y + 80, 120, 34}, "Jump to today")) {
        ResetSimulationToToday(data);
        RefreshStatuses(data);
        SetBanner(ui, "Simulation date reset to today.");
    }

    if (GuiButton(Rectangle{panel.x + 16, panel.y + 124, 120, 34}, "Refresh status")) {
        RefreshStatuses(data);
    }

    if (GuiButton(Rectangle{panel.x + 146, panel.y + 124, 120, 34}, "Load sample")) {
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

    DrawText(TextFormat("Lines: %i", static_cast<int>(data.lines.size())), static_cast<int>(panel.x + 16), static_cast<int>(panel.y + 166), 18, Color{180, 190, 200, 255});
    DrawText(TextFormat("Equipment: %i", equipmentCount), static_cast<int>(panel.x + 116), static_cast<int>(panel.y + 166), 18, Color{180, 190, 200, 255});
    DrawText(TextFormat("Items: %i", itemCount), static_cast<int>(panel.x + 246), static_cast<int>(panel.y + 166), 18, Color{180, 190, 200, 255});

    DrawText("Due today", static_cast<int>(panel.x + 16), static_cast<int>(panel.y + 194), 24, RAYWHITE);
    float y = panel.y + 228;
    if (summary.dueToday.empty()) {
        DrawText("Nothing is due today.", static_cast<int>(panel.x + 16), static_cast<int>(y), 18, Color{180, 190, 200, 255});
    } else {
        for (const auto& label : summary.dueToday) {
            if (y > panel.y + panel.height - 180) break;
            DrawText(label.c_str(), static_cast<int>(panel.x + 16), static_cast<int>(y), 18, Color{252, 210, 110, 255});
            y += 24;
        }
    }

    DrawText("Overdue", static_cast<int>(panel.x + 16), static_cast<int>(panel.y + panel.height - 150), 24, RAYWHITE);
    y = panel.y + panel.height - 116;
    if (summary.overdue.empty()) {
        DrawText("No overdue items.", static_cast<int>(panel.x + 16), static_cast<int>(y), 18, Color{180, 190, 200, 255});
    } else {
        for (const auto& label : summary.overdue) {
            if (y > panel.y + panel.height - 20) break;
            DrawText(label.c_str(), static_cast<int>(panel.x + 16), static_cast<int>(y), 18, Color{255, 130, 120, 255});
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

    CloseWindow();
    return 0;
}
