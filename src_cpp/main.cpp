#include <algorithm>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "raygui.h"
#include "raylib.h"

#include "i18n.hpp"
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
Localizer g_localizer;

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
        std::vector<int> codepoints;
        codepoints.reserve(32000);
        for (int code = 32; code <= 255; ++code) codepoints.push_back(code);
        for (int code = 0x2000; code <= 0x206F; ++code) codepoints.push_back(code);
        for (int code = 0x3000; code <= 0x303F; ++code) codepoints.push_back(code);
        for (int code = 0xFF00; code <= 0xFFEF; ++code) codepoints.push_back(code);
        for (int code = 0x4E00; code <= 0x9FFF; ++code) codepoints.push_back(code);

        g_uiFont = LoadFontEx(fontPath.string().c_str(), 28, codepoints.data(), static_cast<int>(codepoints.size()));
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

void DrawUiText(const std::string& text, float x, float y, float fontSize, Color color) {
    DrawTextEx(g_uiFont, text.c_str(), Vector2{x, y}, fontSize, 1.0f, color);
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

std::string ReplaceToken(std::string value, const std::string& token, const std::string& replacement) {
    std::size_t position = value.find(token);
    if (position != std::string::npos) {
        value.replace(position, token.size(), replacement);
    }
    return value;
}

std::string T(const std::string& key) {
    return g_localizer.Translate(key);
}

std::string T1(const std::string& key, const std::string& arg0) {
    return ReplaceToken(T(key), "{0}", arg0);
}

std::string T2(const std::string& key, const std::string& arg0, const std::string& arg1) {
    return ReplaceToken(T1(key, arg0), "{1}", arg1);
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
    DrawUiText(display, bounds.x + 12, bounds.y + 10, 20.0f, textColor);

    if (active) {
        HandleTextInput(value, kInputLimit);
        if (IsKeyPressed(KEY_ENTER)) {
            ui.focusedField = FocusField::None;
        }
    }
}

void PersistIfAllowed(const AppData& data, const std::filesystem::path& dataFile, AppUi& ui) {
    if (data.simulation.enabled) {
        SetBanner(ui, T("banner.simulation_locked"));
        return;
    }

    if (!SaveAppData(data, dataFile)) {
        SetBanner(ui, T("banner.save_failed"));
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
    DrawUiText(T("app.title"), 24, 18, 34.0f, RAYWHITE);
    DrawUiText(T("app.subtitle"), 26, 54, 18.0f, Color{187, 214, 239, 255});

    const std::string currentDate = T1("header.current_date", EffectiveDate(data));
    DrawUiText(currentDate, static_cast<float>(GetScreenWidth() - 250), 22.0f, 20.0f, Color{230, 241, 255, 255});
    DrawUiText(data.simulation.enabled ? T("header.mode.simulation") : T("header.mode.live"),
               static_cast<float>(GetScreenWidth() - 250), 50.0f, 18.0f,
               data.simulation.enabled ? Color{255, 212, 112, 255} : Color{145, 233, 176, 255});

    DrawUiText(T("header.language"), static_cast<float>(GetScreenWidth() - 410), 22.0f, 18.0f, Color{230, 241, 255, 255});
    float languageX = static_cast<float>(GetScreenWidth() - 410);
    for (int languageIndex = 0; languageIndex < static_cast<int>(g_localizer.Languages().size()); ++languageIndex) {
        const auto& language = g_localizer.Languages()[languageIndex];
        const int buttonWidth = static_cast<int>(std::max(72.0f, MeasureUiText(language.displayName, 16.0f) + 18.0f));
        if (GuiButton(Rectangle{languageX, 46, static_cast<float>(buttonWidth), 28}, language.displayName.c_str())) {
            g_localizer.SetActiveCode(language.code);
            SetBanner(ui, T("banner.language_switched"));
        }
        if (languageIndex == g_localizer.ActiveIndex()) {
            DrawRectangleLinesEx(Rectangle{languageX, 46, static_cast<float>(buttonWidth), 28}, 2, Color{82, 168, 255, 255});
        }
        languageX += static_cast<float>(buttonWidth + 8);
    }

    if (!ui.banner.empty() && GetTime() <= ui.bannerUntil) {
        DrawRectangleRounded(Rectangle{430, 18, 470, 40}, 0.3f, 8, Color{15, 90, 70, 220});
        DrawUiText(FitText(ui.banner, 18.0f, 440.0f), 444.0f, 28.0f, 18.0f, RAYWHITE);
    }
}

void DrawLinesPanel(AppData& data, AppUi& ui, const std::filesystem::path& dataFile) {
    const std::string panelTitle = T("panel.lines");
    const Rectangle panel = Panel(20, 100, 280, GetScreenHeight() - 120, panelTitle.c_str());
    DrawUiText(T("panel.lines.hint"), panel.x + 12, panel.y + 44, 16.0f, Color{180, 190, 200, 255});
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

        DrawUiText(T1("label.eq_short", std::to_string(static_cast<int>(data.lines[index].equipment.size()))),
                   selectRect.x, selectRect.y + 43, 14.0f, Color{160, 170, 180, 255});

        if (GuiButton(duplicateRect, T("button.copy").c_str()) && !data.simulation.enabled) {
            ProductionLine copy = data.lines[index];
            copy.name += " Copy";
            data.lines.push_back(copy);
            PersistIfAllowed(data, dataFile, ui);
        }

        if (GuiButton(deleteRect, T("button.delete").c_str()) && !data.simulation.enabled) {
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

    DrawUiText(T("label.add_line"), panel.x + 12, panel.y + panel.height - 154, 18.0f, RAYWHITE);
    DrawInputBox(Rectangle{panel.x + 12, panel.y + panel.height - 124, panel.width - 24, 40}, ui.newLineName, FocusField::NewLine, ui, T("placeholder.new_line"));
    if (GuiButton(Rectangle{panel.x + 12, panel.y + panel.height - 76, 90, 36}, T("button.add_line").c_str()) && !ui.newLineName.empty()) {
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

    DrawInputBox(Rectangle{panel.x + 110, panel.y + panel.height - 76, panel.width - 122, 36}, ui.renameLineName, FocusField::RenameLine, ui, T("placeholder.rename_line"));
    if (GuiButton(Rectangle{panel.x + 12, panel.y + panel.height - 34, 90, 32}, T("button.save").c_str()) &&
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
    const std::string panelTitle = T("panel.equipment");
    const Rectangle panel = Panel(320, 100, 360, GetScreenHeight() - 120, panelTitle.c_str());
    if (ui.selectedLine < 0 || ui.selectedLine >= static_cast<int>(data.lines.size())) {
        DrawUiText(T("panel.equipment.locked"), 336.0f, 150.0f, 18.0f, Color{180, 190, 200, 255});
        return;
    }

    auto& line = data.lines[ui.selectedLine];
    DrawUiText(FitText(line.name, 20.0f, panel.width - 24), panel.x + 12, panel.y + 46, 20.0f, RAYWHITE);
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

        DrawUiText(T1("label.items_short", std::to_string(static_cast<int>(line.equipment[index].items.size()))),
                   selectRect.x, selectRect.y + 43, 14.0f, Color{160, 170, 180, 255});

        if (GuiButton(deleteRect, T("button.delete").c_str()) && !data.simulation.enabled) {
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

    DrawUiText(T("label.add_equipment"), panel.x + 12, panel.y + panel.height - 154, 18.0f, RAYWHITE);
    DrawInputBox(Rectangle{panel.x + 12, panel.y + panel.height - 124, panel.width - 24, 40}, ui.newEquipmentName, FocusField::NewEquipment, ui, T("placeholder.new_equipment"));
    if (GuiButton(Rectangle{panel.x + 12, panel.y + panel.height - 76, 124, 36}, T("button.add_equipment").c_str()) && !ui.newEquipmentName.empty()) {
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

    DrawInputBox(Rectangle{panel.x + 146, panel.y + panel.height - 76, panel.width - 158, 36}, ui.renameEquipmentName, FocusField::RenameEquipment, ui, T("placeholder.rename_equipment"));
    if (GuiButton(Rectangle{panel.x + 12, panel.y + panel.height - 34, 124, 32}, T("button.save_name").c_str()) &&
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
    const std::string panelTitle = T("panel.items");
    const Rectangle panel = Panel(700, 100, 540, GetScreenHeight() - 120, panelTitle.c_str());

    if (ui.selectedLine < 0 || ui.selectedLine >= static_cast<int>(data.lines.size()) ||
        ui.selectedEquipment < 0 || ui.selectedEquipment >= static_cast<int>(data.lines[ui.selectedLine].equipment.size())) {
        DrawUiText(T("panel.items.locked"), 716.0f, 150.0f, 18.0f, Color{180, 190, 200, 255});
        return;
    }

    auto& equipment = data.lines[ui.selectedLine].equipment[ui.selectedEquipment];
    DrawUiText(FitText(equipment.name, 20.0f, panel.width - 24), panel.x + 12, panel.y + 46, 20.0f, RAYWHITE);
    float y = panel.y + 84;
    const float visibleBottom = panel.y + panel.height - 176;

    for (int index = 0; index < static_cast<int>(equipment.items.size()) && y < visibleBottom; ++index) {
        auto& item = equipment.items[index];
        const int daysSince = DaysBetween(item.lastCheckedDate, EffectiveDate(data));
        std::string status = item.checkedToday ? T("status.done_today") : (daysSince > item.periodDays ? T("status.overdue") : (daysSince == item.periodDays ? T("status.due_today") : T("status.scheduled")));
        const std::string title = FitText(item.name, 18.0f, 260.0f);
        const std::string meta = FitText(T2("meta.every_last", std::to_string(item.periodDays), item.lastCheckedDate), 15.0f, 300.0f);

        Rectangle selectRect{panel.x + 12, y, 318, 54};
        Rectangle checkRect{panel.x + 340, y + 7, 86, 40};
        Rectangle deleteRect{panel.x + 436, y + 7, 36, 40};

        if (GuiButton(selectRect, title.c_str())) {
            ui.selectedItem = index;
            SyncRenameBuffers(data, ui);
        }

        DrawUiText(meta, selectRect.x, selectRect.y + 58, 15.0f, Color{160, 170, 180, 255});
        DrawUiText(status, panel.x + panel.width - 110, selectRect.y + 15, 16.0f,
                   item.checkedToday ? Color{145, 233, 176, 255} : (daysSince > item.periodDays ? Color{255, 132, 132, 255} :
                   (daysSince == item.periodDays ? Color{255, 212, 112, 255} : Color{180, 190, 200, 255})));

        if (GuiButton(checkRect, item.checkedToday ? T("button.undo").c_str() : T("button.done").c_str())) {
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

        if (GuiButton(deleteRect, T("button.delete").c_str())) {
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

    DrawUiText(T("label.add_item"), panel.x + 12, panel.y + panel.height - 126, 18.0f, RAYWHITE);
    DrawInputBox(Rectangle{panel.x + 12, panel.y + panel.height - 96, 250, 40}, ui.newItemName, FocusField::NewItem, ui, T("placeholder.new_item"));
    GuiSpinner(Rectangle{panel.x + 270, panel.y + panel.height - 96, 100, 40}, T("label.days").c_str(), &ui.newItemPeriod, 1, 365, true);
    if (GuiButton(Rectangle{panel.x + 378, panel.y + panel.height - 96, 80, 40}, T("button.add").c_str()) && !ui.newItemName.empty()) {
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

    DrawUiText(T("label.edit_item"), panel.x + 12, panel.y + panel.height - 52, 18.0f, RAYWHITE);
    DrawInputBox(Rectangle{panel.x + 12, panel.y + panel.height - 24, 250, 40}, ui.renameItemName, FocusField::RenameItem, ui, T("placeholder.rename_item"));
    GuiSpinner(Rectangle{panel.x + 270, panel.y + panel.height - 24, 100, 40}, T("label.days").c_str(), &ui.renameItemPeriod, 1, 365, true);
    if (GuiButton(Rectangle{panel.x + 378, panel.y + panel.height - 24, 80, 40}, T("button.save").c_str()) &&
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
    const std::string panelTitle = T("panel.dashboard");
    const Rectangle panel = Panel(1260, 100, GetScreenWidth() - 1280, GetScreenHeight() - 120, panelTitle.c_str());
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
    if (GuiCheckBox(Rectangle{panel.x + 16, panel.y + 42, 24, 24}, T("label.enable_simulation").c_str(), &simEnabled)) {
        data.simulation.enabled = simEnabled;
        RefreshStatuses(data);
        SetBanner(ui, data.simulation.enabled ? T("banner.simulation_enabled") : T("banner.simulation_disabled"));
    }

    DrawUiText(T1("label.lines_count", std::to_string(static_cast<int>(data.lines.size()))), panel.x + 16, panel.y + 86, 18.0f, Color{180, 190, 200, 255});
    DrawUiText(T1("label.equipment_count", std::to_string(equipmentCount)), panel.x + 16, panel.y + 110, 18.0f, Color{180, 190, 200, 255});
    DrawUiText(T1("label.items_count", std::to_string(itemCount)), panel.x + 16, panel.y + 134, 18.0f, Color{180, 190, 200, 255});

    GuiSpinner(Rectangle{panel.x + 16, panel.y + 168, 82, 34}, T("label.year").c_str(), &data.simulation.year, 2020, 2100, true);
    GuiSpinner(Rectangle{panel.x + 104, panel.y + 168, 56, 34}, T("label.month").c_str(), &data.simulation.month, 1, 12, true);
    GuiSpinner(Rectangle{panel.x + 166, panel.y + 168, 56, 34}, T("label.day").c_str(), &data.simulation.day, 1, 31, true);

    if (GuiButton(Rectangle{panel.x + 16, panel.y + 210, panel.width - 32, 34}, T("button.jump_today").c_str())) {
        ResetSimulationToToday(data);
        RefreshStatuses(data);
        SetBanner(ui, T("banner.simulation_today"));
    }

    if (GuiButton(Rectangle{panel.x + 16, panel.y + 252, panel.width - 32, 34}, T("button.refresh").c_str())) {
        RefreshStatuses(data);
    }

    if (GuiButton(Rectangle{panel.x + 16, panel.y + 294, panel.width - 32, 34}, T("button.load_sample").c_str())) {
        if (!data.simulation.enabled) {
            data = BuildSampleData();
            RefreshStatuses(data);
            ui.selectedLine = 0;
            ui.selectedEquipment = 0;
            ui.selectedItem = 0;
            SyncRenameBuffers(data, ui);
            PersistIfAllowed(data, dataFile, ui);
            SetBanner(ui, T("banner.sample_loaded"));
        } else {
            PersistIfAllowed(data, dataFile, ui);
        }
    }

    DrawUiText(T("label.due_today"), panel.x + 16, panel.y + 346, 22.0f, RAYWHITE);
    float y = panel.y + 378;
    if (summary.dueToday.empty()) {
        DrawUiText(T("label.nothing_due"), panel.x + 16, y, 18.0f, Color{180, 190, 200, 255});
    } else {
        for (const auto& label : summary.dueToday) {
            if (y > panel.y + panel.height - 170) break;
            DrawUiText(FitText(label, 16.0f, panel.width - 32), panel.x + 16, y, 16.0f, Color{252, 210, 110, 255});
            y += 22;
        }
    }

    DrawUiText(T("label.overdue"), panel.x + 16, panel.y + panel.height - 150, 22.0f, RAYWHITE);
    y = panel.y + panel.height - 116;
    if (summary.overdue.empty()) {
        DrawUiText(T("label.nothing_overdue"), panel.x + 16, y, 18.0f, Color{180, 190, 200, 255});
    } else {
        for (const auto& label : summary.overdue) {
            if (y > panel.y + panel.height - 20) break;
            DrawUiText(FitText(label, 16.0f, panel.width - 32), panel.x + 16, y, 16.0f, Color{255, 130, 120, 255});
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
    g_localizer.LoadFromDirectory(root / "data" / "locales");
    if (!g_localizer.SetActiveCode("zh-TW")) {
        g_localizer.SetActiveCode("en-US");
    }

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
