#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
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
constexpr float kBaseWidth = 1440.0f;
constexpr float kBaseHeight = 900.0f;
constexpr int kFontAtlasSize = 96;

Font g_uiFontRegular{};
Font g_uiFontStrong{};
bool g_hasCustomFont = false;
Localizer g_localizer;
float g_uiScale = 1.0f;

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
    float linesScroll = 0.0f;
    float equipmentScroll = 0.0f;
    float itemsScroll = 0.0f;
    float dashboardScroll = 0.0f;
    float dueAlertsScroll = 0.0f;
    float overdueAlertsScroll = 0.0f;
    bool hideDashboardInSimulation = false;
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

struct UiLayout {
    Rectangle lines;
    Rectangle equipment;
    Rectangle items;
    Rectangle dashboard;
    Rectangle alerts;
};

void LoadUiFont(const std::filesystem::path& root) {
    const auto regularPath = root / "data" / "font.otf";
    const auto boldPath = root / "data" / "font_bold.otf";
    if (std::filesystem::exists(regularPath) && std::filesystem::exists(boldPath)) {
        std::vector<int> codepoints;
        codepoints.reserve(32000);
        for (int code = 32; code <= 255; ++code) codepoints.push_back(code);
        for (int code = 0x2000; code <= 0x206F; ++code) codepoints.push_back(code);
        for (int code = 0x3000; code <= 0x303F; ++code) codepoints.push_back(code);
        for (int code = 0xFF00; code <= 0xFFEF; ++code) codepoints.push_back(code);
        for (int code = 0x4E00; code <= 0x9FFF; ++code) codepoints.push_back(code);

        g_uiFontRegular = LoadFontEx(regularPath.string().c_str(), kFontAtlasSize, codepoints.data(), static_cast<int>(codepoints.size()));
        g_uiFontStrong = LoadFontEx(boldPath.string().c_str(), kFontAtlasSize, codepoints.data(), static_cast<int>(codepoints.size()));
        if (g_uiFontRegular.texture.id != 0 && g_uiFontStrong.texture.id != 0) {
            SetTextureFilter(g_uiFontRegular.texture, TEXTURE_FILTER_POINT);
            SetTextureFilter(g_uiFontStrong.texture, TEXTURE_FILTER_POINT);
            GuiSetFont(g_uiFontRegular);
            g_hasCustomFont = true;
        }
    }

    if (!g_hasCustomFont) {
        g_uiFontRegular = GetFontDefault();
        g_uiFontStrong = g_uiFontRegular;
        GuiSetFont(g_uiFontRegular);
    }

    GuiSetStyle(DEFAULT, TEXT_SIZE, kUiTextSize);
    GuiSetStyle(DEFAULT, TEXT_SPACING, 1);
}

void UnloadUiFont() {
    if (g_hasCustomFont) {
        UnloadFont(g_uiFontRegular);
        UnloadFont(g_uiFontStrong);
    }
}

float ScalePx(float value) {
    return value * g_uiScale;
}

float ScaleText(float value) {
    return value * std::max(1.14f, g_uiScale * 1.08f);
}

void UpdateUiScale() {
    const float widthScale = static_cast<float>(GetScreenWidth()) / kBaseWidth;
    const float heightScale = static_cast<float>(GetScreenHeight()) / kBaseHeight;
    g_uiScale = std::clamp(std::min(widthScale, heightScale), 1.00f, 1.65f);
    GuiSetStyle(DEFAULT, TEXT_SIZE, static_cast<int>(std::round(ScalePx(static_cast<float>(kUiTextSize)))));
    GuiSetStyle(DEFAULT, TEXT_SPACING, static_cast<int>(std::round(std::max(1.0f, g_uiScale))));
}

float MeasureUiText(const std::string& text, float fontSize, bool strong = false) {
    const Font& font = strong ? g_uiFontStrong : g_uiFontRegular;
    return MeasureTextEx(font, text.c_str(), ScaleText(fontSize), std::max(1.0f, g_uiScale)).x;
}

void DrawUiText(const std::string& text, float x, float y, float fontSize, Color color, bool strong = false) {
    const Font& font = strong ? g_uiFontStrong : g_uiFontRegular;
    DrawTextEx(font, text.c_str(), Vector2{x, y}, ScaleText(fontSize), std::max(1.0f, g_uiScale), color);
}

std::string FitText(const std::string& text, float fontSize, float maxWidth, bool strong = false) {
    if (text.empty() || MeasureUiText(text, fontSize, strong) <= maxWidth) {
        return text;
    }

    std::string trimmed = text;
    while (!trimmed.empty()) {
        trimmed.pop_back();
        const std::string candidate = trimmed + "...";
        if (MeasureUiText(candidate, fontSize, strong) <= maxWidth) {
            return candidate;
        }
    }
    return "...";
}

std::vector<std::string> WrapTextLines(const std::string& text, float fontSize, float maxWidth, bool strong = false) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string word;
    std::string current;

    while (stream >> word) {
        const std::string candidate = current.empty() ? word : current + " " + word;
        if (MeasureUiText(candidate, fontSize, strong) <= maxWidth) {
            current = candidate;
        } else {
            if (!current.empty()) {
                lines.push_back(current);
            }
            current = word;
        }
    }

    if (!current.empty()) {
        lines.push_back(current);
    }

    if (lines.empty()) {
        lines.push_back(text);
    }
    return lines;
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

std::string T3(const std::string& key, const std::string& arg0, const std::string& arg1, const std::string& arg2) {
    return ReplaceToken(T2(key, arg0, arg1), "{2}", arg2);
}

std::string BuildAlertHeadline(const MaintenanceAlert& alert) {
    return T3("summary.headline", alert.lineName, alert.equipmentName, alert.slotName);
}

std::string BuildAlertTail(const MaintenanceAlert& alert, bool overdue) {
    if (overdue) {
        return T1("summary.overdue_days", std::to_string(alert.daysLate));
    }
    return T("summary.due_now");
}

std::string DashboardToggleLabel(const AppData& data, const AppUi& ui) {
    if (!data.simulation.enabled) {
        return "";
    }
    const bool isTraditionalChinese =
        !g_localizer.Empty() &&
        g_localizer.ActiveIndex() >= 0 &&
        g_localizer.ActiveIndex() < static_cast<int>(g_localizer.Languages().size()) &&
        g_localizer.Languages()[g_localizer.ActiveIndex()].code == "zh-TW";
    if (isTraditionalChinese) {
        return ui.hideDashboardInSimulation ? "顯示儀表板" : "隱藏儀表板";
    }
    return ui.hideDashboardInSimulation ? "Show dashboard" : "Hide dashboard";
}

std::string SimulationToggleLabel(const AppData& data) {
    const bool isTraditionalChinese =
        !g_localizer.Empty() &&
        g_localizer.ActiveIndex() >= 0 &&
        g_localizer.ActiveIndex() < static_cast<int>(g_localizer.Languages().size()) &&
        g_localizer.Languages()[g_localizer.ActiveIndex()].code == "zh-TW";
    if (isTraditionalChinese) {
        return data.simulation.enabled ? "離開模擬" : "模擬模式";
    }
    return data.simulation.enabled ? "Exit simulation" : "Simulation mode";
}

std::string Utf8FromCodepoint(int codepoint) {
    std::string output;
    if (codepoint <= 0x7F) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0x10FFFF) {
        output.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    return output;
}

void EraseLastUtf8Codepoint(std::string& value) {
    if (value.empty()) {
        return;
    }

    std::size_t index = value.size() - 1;
    while (index > 0 && (static_cast<unsigned char>(value[index]) & 0xC0) == 0x80) {
        --index;
    }
    value.erase(index);
}

std::filesystem::path ResolveDataFile(const std::filesystem::path& root) {
    const std::filesystem::path preferred = root / "data" / "maintenance" / "database.json";
    const std::filesystem::path legacy = root / "data" / "database.json";

    if (std::filesystem::exists(preferred)) {
        return preferred;
    }

    if (std::filesystem::exists(legacy)) {
        std::filesystem::create_directories(preferred.parent_path());
        std::error_code error;
        std::filesystem::copy_file(legacy, preferred, std::filesystem::copy_options::overwrite_existing, error);
        return preferred;
    }

    return preferred;
}

void OpenFolderInExplorer(const std::filesystem::path& folder) {
    const std::string command = "cmd /c start \"\" explorer \"" + folder.string() + "\"";
    std::thread([command]() {
        std::system(command.c_str());
    }).detach();
}

UiLayout BuildLayout(bool hideDashboard) {
    const float margin = ScalePx(18.0f);
    const float gap = ScalePx(18.0f);
    const float top = ScalePx(122.0f);
    const float bottom = ScalePx(18.0f);
    const float horizontalGaps = hideDashboard ? (gap * 2.0f) : (gap * 3.0f);
    const float totalWidth = static_cast<float>(GetScreenWidth()) - (margin * 2.0f) - horizontalGaps;
    const float panelHeight = static_cast<float>(GetScreenHeight()) - top - bottom;
    const float bottomBandHeight = std::clamp(panelHeight * 0.25f, ScalePx(200.0f), ScalePx(280.0f));
    const float topPanelHeight = panelHeight - bottomBandHeight - gap;

    const float linesWidth = std::clamp(totalWidth * 0.18f, ScalePx(240.0f), ScalePx(320.0f));
    const float equipmentWidth = std::clamp(totalWidth * 0.23f, ScalePx(280.0f), ScalePx(380.0f));
    const float dashboardWidth = hideDashboard ? 0.0f : std::clamp(totalWidth * 0.20f, ScalePx(260.0f), ScalePx(360.0f));
    const float itemsWidth = std::max(ScalePx(360.0f), totalWidth - linesWidth - equipmentWidth - dashboardWidth);

    UiLayout layout{};
    layout.lines = Rectangle{margin, top, linesWidth, panelHeight};
    layout.equipment = Rectangle{layout.lines.x + layout.lines.width + gap, top, equipmentWidth, panelHeight};
    layout.items = Rectangle{layout.equipment.x + layout.equipment.width + gap, top, itemsWidth, topPanelHeight};
    layout.dashboard = hideDashboard
        ? Rectangle{layout.items.x + layout.items.width, top, 0.0f, 0.0f}
        : Rectangle{layout.items.x + layout.items.width + gap, top, dashboardWidth, topPanelHeight};
    layout.alerts = Rectangle{
        layout.items.x,
        top + topPanelHeight + gap,
        hideDashboard ? itemsWidth : (itemsWidth + gap + dashboardWidth),
        bottomBandHeight
    };
    return layout;
}

std::filesystem::path ResolveProjectRoot(char* argv0) {
    std::filesystem::path current = std::filesystem::absolute(argv0).parent_path();
    for (int depth = 0; depth < 4; ++depth) {
        if (std::filesystem::exists(current / "data" / "maintenance" / "database.json")) {
            return current;
        }
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

void HandleWheelScroll(Rectangle bounds, float contentHeight, float& scrollOffset) {
    if (!CheckCollisionPointRec(GetMousePosition(), bounds)) {
        return;
    }

    const float wheel = GetMouseWheelMove();
    if (wheel == 0.0f) {
        return;
    }

    const float maxScroll = std::max(0.0f, contentHeight - bounds.height);
    scrollOffset = std::clamp(scrollOffset - wheel * ScalePx(38.0f), 0.0f, maxScroll);
}

void DrawScrollHint(Rectangle bounds, float contentHeight, float scrollOffset) {
    if (contentHeight <= bounds.height) {
        return;
    }

    const float trackHeight = bounds.height;
    const float thumbHeight = std::max(ScalePx(42.0f), trackHeight * (bounds.height / contentHeight));
    const float travel = std::max(1.0f, trackHeight - thumbHeight);
    const float maxScroll = std::max(1.0f, contentHeight - bounds.height);
    const float thumbY = bounds.y + (scrollOffset / maxScroll) * travel;

    DrawRectangleRounded(Rectangle{bounds.x + bounds.width - ScalePx(6.0f), bounds.y, ScalePx(4.0f), trackHeight}, 0.5f, 4, Color{48, 60, 76, 180});
    DrawRectangleRounded(Rectangle{bounds.x + bounds.width - ScalePx(7.0f), thumbY, ScalePx(6.0f), thumbHeight}, 0.5f, 4, Color{106, 139, 175, 220});
}

void HandleTextInput(std::string& value, std::size_t maxLength) {
    int key = GetCharPressed();
    while (key > 0) {
        const std::string utf8 = Utf8FromCodepoint(key);
        if (key >= 32 && !utf8.empty() && value.size() + utf8.size() <= maxLength * 4) {
            value += utf8;
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && !value.empty()) {
        EraseLastUtf8Codepoint(value);
    }
}

void DrawInputBox(Rectangle bounds, std::string& value, FocusField field, AppUi& ui, const std::string& placeholder) {
    const bool active = (ui.focusedField == field);
    if (CheckCollisionPointRec(GetMousePosition(), bounds) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        ui.focusedField = field;
    }

    DrawRectangleRounded(bounds, 0.18f, 10, active ? Color{29, 45, 64, 255} : Color{20, 30, 44, 255});
    DrawRectangleRoundedLines(bounds, 0.18f, 10, std::max(1.5f, ScalePx(1.8f)), active ? Color{74, 154, 255, 255} : Color{58, 78, 98, 255});

    const std::string display = FitText(value.empty() ? placeholder : value, 18.0f, bounds.width - ScalePx(24.0f));
    const Color textColor = value.empty() ? Color{120, 140, 160, 255} : RAYWHITE;
    DrawUiText(display, bounds.x + ScalePx(12.0f), bounds.y + ScalePx(9.0f), 19.0f, textColor);

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
    DrawRectangleRounded(rect, 0.06f, 12, Color{18, 26, 38, 235});
    DrawRectangleRoundedLines(rect, 0.06f, 12, std::max(1.0f, ScalePx(1.4f)), Color{53, 70, 94, 255});
    DrawRectangleRounded(Rectangle{rect.x, rect.y, rect.width, ScalePx(46.0f)}, 0.06f, 12, Color{20, 41, 64, 255});
    DrawRectangle(rect.x + ScalePx(14.0f), rect.y + ScalePx(10.0f), ScalePx(4.0f), ScalePx(24.0f), Color{82, 168, 255, 255});
    DrawUiText(title, rect.x + ScalePx(28.0f), rect.y + ScalePx(10.0f), 20.0f, RAYWHITE, true);
    return rect;
}

void DrawInfoCard(Rectangle bounds, const std::string& title, const std::string& value, Color accent) {
    DrawRectangleRounded(bounds, 0.18f, 10, Color{24, 34, 46, 255});
    DrawRectangleRoundedLines(bounds, 0.18f, 10, std::max(1.0f, ScalePx(1.3f)), Color{60, 78, 102, 255});
    DrawRectangleRounded(Rectangle{bounds.x, bounds.y, ScalePx(6.0f), bounds.height}, 0.3f, 4, accent);
    DrawUiText(title, bounds.x + ScalePx(16.0f), bounds.y + ScalePx(12.0f), 14.0f, Color{168, 181, 196, 255});
    DrawUiText(value, bounds.x + ScalePx(16.0f), bounds.y + ScalePx(36.0f), 28.0f, RAYWHITE, true);
}

float DrawAlertSection(Rectangle bounds, const std::string& title, const std::vector<MaintenanceAlert>& alerts, bool overdue, const std::string& emptyText, Color accent) {
    DrawUiText(title, bounds.x, bounds.y, 20.0f, RAYWHITE, true);
    float y = bounds.y + ScalePx(34.0f);

    if (alerts.empty()) {
        DrawUiText(emptyText, bounds.x, y, 16.0f, Color{180, 190, 200, 255});
        return y + ScalePx(28.0f);
    }

    for (const auto& alert : alerts) {
        const std::string headline = BuildAlertHeadline(alert);
        const std::vector<std::string> headlineLines = WrapTextLines(headline, 15.5f, bounds.width - ScalePx(24.0f), true);
        const std::string tail = BuildAlertTail(alert, overdue);
        const float entryHeight = ScalePx(18.0f) + static_cast<float>(headlineLines.size()) * ScalePx(22.0f) + ScalePx(22.0f);

        if (y + entryHeight > bounds.y + bounds.height) {
            break;
        }

        DrawRectangleRounded(Rectangle{bounds.x, y, bounds.width, entryHeight}, 0.16f, 10, Color{24, 34, 46, 255});
        DrawRectangleRoundedLines(Rectangle{bounds.x, y, bounds.width, entryHeight}, 0.16f, 10, std::max(1.0f, ScalePx(1.1f)), Color{60, 78, 102, 255});
        DrawRectangleRounded(Rectangle{bounds.x, y, ScalePx(5.0f), entryHeight}, 0.3f, 4, accent);

        float textY = y + ScalePx(10.0f);
        for (const auto& line : headlineLines) {
            DrawUiText(line, bounds.x + ScalePx(12.0f), textY, 15.5f, RAYWHITE, true);
            textY += ScalePx(22.0f);
        }
        DrawUiText(tail, bounds.x + ScalePx(12.0f), textY + ScalePx(2.0f), 14.5f, overdue ? Color{255, 130, 120, 255} : Color{252, 210, 110, 255});
        y += entryHeight + ScalePx(10.0f);
    }

    return y;
}

float EstimateAlertSectionHeight(const std::vector<MaintenanceAlert>& alerts, float width) {
    float height = ScalePx(34.0f);
    if (alerts.empty()) {
        return height + ScalePx(28.0f);
    }

    for (const auto& alert : alerts) {
        const std::vector<std::string> headlineLines = WrapTextLines(BuildAlertHeadline(alert), 15.5f, width - ScalePx(24.0f), true);
        const float entryHeight = ScalePx(18.0f) + static_cast<float>(headlineLines.size()) * ScalePx(22.0f) + ScalePx(22.0f);
        height += entryHeight + ScalePx(10.0f);
    }

    return height;
}

void DrawSingleAlertFeed(Rectangle bounds,
                         const std::string& title,
                         const std::vector<MaintenanceAlert>& alerts,
                         bool overdue,
                         const std::string& emptyText,
                         Color accent,
                         float& scrollOffset) {
    const float totalContentHeight = EstimateAlertSectionHeight(alerts, bounds.width);
    HandleWheelScroll(bounds, totalContentHeight, scrollOffset);

    BeginScissorMode(static_cast<int>(bounds.x), static_cast<int>(bounds.y), static_cast<int>(bounds.width), static_cast<int>(bounds.height));
    DrawAlertSection(Rectangle{bounds.x, bounds.y - scrollOffset, bounds.width, totalContentHeight}, title, alerts, overdue, emptyText, accent);
    EndScissorMode();

    DrawScrollHint(bounds, totalContentHeight, scrollOffset);
}

void DrawAlertsPanel(const UiLayout& layout, const AppData& data, AppUi& ui) {
    const DueSummary summary = BuildDueSummary(data);
    const std::string panelTitle = T("label.due_today") + " / " + T("label.overdue");
    const Rectangle panel = Panel(layout.alerts.x, layout.alerts.y, layout.alerts.width, layout.alerts.height, panelTitle.c_str());
    const float innerGap = ScalePx(16.0f);
    const float sectionTop = panel.y + ScalePx(58.0f);
    const float sectionHeight = panel.height - ScalePx(74.0f);
    const float sectionWidth = (panel.width - innerGap * 3.0f) / 2.0f;
    const Rectangle dueBounds{panel.x + innerGap, sectionTop, sectionWidth, sectionHeight};
    const Rectangle overdueBounds{dueBounds.x + sectionWidth + innerGap, sectionTop, sectionWidth, sectionHeight};

    DrawSingleAlertFeed(dueBounds, T("label.due_today"), summary.dueToday, false, T("label.nothing_due"), Color{241, 192, 84, 255}, ui.dueAlertsScroll);
    DrawSingleAlertFeed(overdueBounds, T("label.overdue"), summary.overdue, true, T("label.nothing_overdue"), Color{239, 115, 115, 255}, ui.overdueAlertsScroll);
}

void DrawHeader(AppData& data, AppUi& ui) {
    const float headerHeight = ScalePx(104.0f);
    DrawRectangleGradientH(0, 0, GetScreenWidth(), static_cast<int>(headerHeight), Color{9, 26, 43, 255}, Color{12, 50, 88, 255});
    DrawUiText(T("app.title"), ScalePx(24.0f), ScalePx(14.0f), 38.0f, RAYWHITE, true);
    DrawUiText(T("app.subtitle"), ScalePx(26.0f), ScalePx(54.0f), 18.0f, Color{187, 214, 239, 255});

    const std::string currentDate = T1("header.current_date", EffectiveDate(data));
    const std::string simulationToggle = SimulationToggleLabel(data);
    const float rightMargin = ScalePx(24.0f);
    const float controlsTop = ScalePx(16.0f);
    const float buttonsY = ScalePx(56.0f);
    const float buttonHeight = ScalePx(30.0f);
    const float buttonGap = ScalePx(8.0f);

    DrawUiText(currentDate, static_cast<float>(GetScreenWidth()) - ScalePx(220.0f), controlsTop, 18.0f, Color{230, 241, 255, 255});
    DrawUiText(data.simulation.enabled ? T("header.mode.simulation") : T("header.mode.live"),
               static_cast<float>(GetScreenWidth()) - ScalePx(220.0f), ScalePx(40.0f), 17.0f,
               data.simulation.enabled ? Color{255, 212, 112, 255} : Color{145, 233, 176, 255});

    float totalLanguageWidth = 0.0f;
    std::vector<float> languageWidths;
    languageWidths.reserve(g_localizer.Languages().size());
    for (const auto& language : g_localizer.Languages()) {
        const float width = std::max(ScalePx(78.0f), MeasureUiText(language.displayName, 14.5f, true) + ScalePx(18.0f));
        languageWidths.push_back(width);
        totalLanguageWidth += width;
    }
    if (!languageWidths.empty()) {
        totalLanguageWidth += buttonGap * static_cast<float>(languageWidths.size() - 1);
    }

    float extraWidth = 0.0f;
    const std::string dashboardToggle = data.simulation.enabled ? DashboardToggleLabel(data, ui) : "";
    const float dashboardToggleWidth = data.simulation.enabled
        ? std::max(ScalePx(124.0f), MeasureUiText(dashboardToggle, 14.5f, true) + ScalePx(20.0f))
        : 0.0f;
    const float simulationToggleWidth = std::max(ScalePx(126.0f), MeasureUiText(simulationToggle, 14.5f, true) + ScalePx(20.0f));
    extraWidth += simulationToggleWidth + buttonGap;
    if (data.simulation.enabled) {
        extraWidth += dashboardToggleWidth + buttonGap;
    }

    float languageX = static_cast<float>(GetScreenWidth()) - rightMargin - totalLanguageWidth;
    float actionsX = languageX - extraWidth;
    if (actionsX < ScalePx(860.0f)) {
        actionsX = ScalePx(860.0f);
        languageX = actionsX + extraWidth;
    }

    DrawUiText(T("header.language"), languageX, controlsTop, 15.0f, Color{230, 241, 255, 255});
    for (int languageIndex = 0; languageIndex < static_cast<int>(g_localizer.Languages().size()); ++languageIndex) {
        const auto& language = g_localizer.Languages()[languageIndex];
        const float buttonWidth = languageWidths[languageIndex];
        if (GuiButton(Rectangle{languageX, buttonsY, buttonWidth, buttonHeight}, language.displayName.c_str())) {
            g_localizer.SetActiveCode(language.code);
            SetBanner(ui, T("banner.language_switched"));
        }
        if (languageIndex == g_localizer.ActiveIndex()) {
            DrawRectangleLinesEx(Rectangle{languageX, buttonsY, buttonWidth, buttonHeight}, std::max(1.4f, ScalePx(1.6f)), Color{82, 168, 255, 255});
        }
        languageX += buttonWidth + buttonGap;
    }

    if (GuiButton(Rectangle{actionsX, buttonsY, simulationToggleWidth, buttonHeight}, simulationToggle.c_str())) {
        data.simulation.enabled = !data.simulation.enabled;
        ui.hideDashboardInSimulation = data.simulation.enabled;
        RefreshStatuses(data);
        SetBanner(ui, data.simulation.enabled ? T("banner.simulation_enabled") : T("banner.simulation_disabled"));
    }

    if (data.simulation.enabled) {
        const Rectangle toggleRect{actionsX + simulationToggleWidth + buttonGap, buttonsY, dashboardToggleWidth, buttonHeight};
        if (GuiButton(toggleRect, dashboardToggle.c_str())) {
            ui.hideDashboardInSimulation = !ui.hideDashboardInSimulation;
        }
    } else {
        ui.hideDashboardInSimulation = false;
    }

    if (!ui.banner.empty() && GetTime() <= ui.bannerUntil) {
        const Rectangle banner{ScalePx(430.0f), ScalePx(18.0f), ScalePx(430.0f), ScalePx(42.0f)};
        DrawRectangleRounded(banner, 0.3f, 8, Color{15, 90, 70, 220});
        DrawUiText(FitText(ui.banner, 17.0f, banner.width - ScalePx(26.0f)), banner.x + ScalePx(14.0f), banner.y + ScalePx(10.0f), 17.0f, RAYWHITE);
    }
}

void DrawLinesPanel(const UiLayout& layout, AppData& data, AppUi& ui, const std::filesystem::path& dataFile) {
    const std::string panelTitle = T("panel.lines");
    const Rectangle panel = Panel(layout.lines.x, layout.lines.y, layout.lines.width, layout.lines.height, panelTitle.c_str());
    DrawUiText(T("panel.lines.hint"), panel.x + ScalePx(12.0f), panel.y + ScalePx(54.0f), 16.0f, Color{180, 190, 200, 255});
    const Rectangle listArea{panel.x + ScalePx(8.0f), panel.y + ScalePx(86.0f), panel.width - ScalePx(8.0f), panel.height - ScalePx(262.0f)};
    const float rowHeight = ScalePx(66.0f);
    const float contentHeight = static_cast<float>(data.lines.size()) * rowHeight;
    HandleWheelScroll(listArea, contentHeight, ui.linesScroll);
    float y = listArea.y - ui.linesScroll;

    for (int index = 0; index < static_cast<int>(data.lines.size()); ++index) {
        const float actionWidth = ScalePx(48.0f);
        const float copyWidth = ScalePx(62.0f);
        Rectangle selectRect{panel.x + ScalePx(12.0f), y, panel.width - ScalePx(36.0f) - actionWidth - copyWidth - ScalePx(12.0f), ScalePx(44.0f)};
        Rectangle duplicateRect{selectRect.x + selectRect.width + ScalePx(8.0f), y, copyWidth, ScalePx(44.0f)};
        Rectangle deleteRect{duplicateRect.x + duplicateRect.width + ScalePx(8.0f), y, actionWidth, ScalePx(44.0f)};
        const std::string label = FitText(data.lines[index].name, 18.0f, selectRect.width - ScalePx(20.0f));

        if (deleteRect.y + deleteRect.height < listArea.y || selectRect.y > listArea.y + listArea.height) {
            y += rowHeight;
            continue;
        }

        if (GuiButton(selectRect, label.c_str())) {
            ui.selectedLine = index;
            ui.selectedEquipment = data.lines[index].equipment.empty() ? -1 : 0;
            ui.selectedItem = (ui.selectedEquipment >= 0 && !data.lines[index].equipment[0].items.empty()) ? 0 : -1;
            SyncRenameBuffers(data, ui);
        }

        DrawUiText(T1("label.eq_short", std::to_string(static_cast<int>(data.lines[index].equipment.size()))),
                   selectRect.x, selectRect.y + ScalePx(47.0f), 14.0f, Color{160, 170, 180, 255});

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

        y += rowHeight;
    }
    DrawScrollHint(listArea, contentHeight, ui.linesScroll);

    DrawUiText(T("label.add_line"), panel.x + ScalePx(12.0f), panel.y + panel.height - ScalePx(164.0f), 18.0f, RAYWHITE, true);
    DrawInputBox(Rectangle{panel.x + ScalePx(12.0f), panel.y + panel.height - ScalePx(126.0f), panel.width - ScalePx(24.0f), ScalePx(42.0f)}, ui.newLineName, FocusField::NewLine, ui, T("placeholder.new_line"));
    if (GuiButton(Rectangle{panel.x + ScalePx(12.0f), panel.y + panel.height - ScalePx(76.0f), std::min(ScalePx(132.0f), panel.width * 0.34f), ScalePx(38.0f)}, T("button.add_line").c_str()) && !ui.newLineName.empty()) {
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

    const float lineButtonWidth = std::min(ScalePx(132.0f), panel.width * 0.34f);
    DrawInputBox(Rectangle{panel.x + ScalePx(20.0f) + lineButtonWidth, panel.y + panel.height - ScalePx(76.0f), panel.width - (ScalePx(32.0f) + lineButtonWidth), ScalePx(38.0f)}, ui.renameLineName, FocusField::RenameLine, ui, T("placeholder.rename_line"));
    if (GuiButton(Rectangle{panel.x + ScalePx(12.0f), panel.y + panel.height - ScalePx(34.0f), lineButtonWidth, ScalePx(32.0f)}, T("button.save").c_str()) &&
        ui.selectedLine >= 0 && ui.selectedLine < static_cast<int>(data.lines.size()) && !ui.renameLineName.empty()) {
        if (!data.simulation.enabled) {
            data.lines[ui.selectedLine].name = ui.renameLineName;
            PersistIfAllowed(data, dataFile, ui);
        } else {
            PersistIfAllowed(data, dataFile, ui);
        }
    }
}

void DrawEquipmentPanel(const UiLayout& layout, AppData& data, AppUi& ui, const std::filesystem::path& dataFile) {
    const std::string panelTitle = T("panel.equipment");
    const Rectangle panel = Panel(layout.equipment.x, layout.equipment.y, layout.equipment.width, layout.equipment.height, panelTitle.c_str());
    if (ui.selectedLine < 0 || ui.selectedLine >= static_cast<int>(data.lines.size())) {
        DrawUiText(T("panel.equipment.locked"), panel.x + ScalePx(14.0f), panel.y + ScalePx(62.0f), 18.0f, Color{180, 190, 200, 255});
        return;
    }

    auto& line = data.lines[ui.selectedLine];
    DrawUiText(FitText(line.name, 21.0f, panel.width - ScalePx(24.0f), true), panel.x + ScalePx(12.0f), panel.y + ScalePx(54.0f), 21.0f, RAYWHITE, true);
    const Rectangle listArea{panel.x + ScalePx(8.0f), panel.y + ScalePx(92.0f), panel.width - ScalePx(8.0f), panel.height - ScalePx(268.0f)};
    const float rowHeight = ScalePx(66.0f);
    const float contentHeight = static_cast<float>(line.equipment.size()) * rowHeight;
    HandleWheelScroll(listArea, contentHeight, ui.equipmentScroll);
    float y = listArea.y - ui.equipmentScroll;

    for (int index = 0; index < static_cast<int>(line.equipment.size()); ++index) {
        Rectangle selectRect{panel.x + ScalePx(12.0f), y, panel.width - ScalePx(74.0f), ScalePx(44.0f)};
        Rectangle deleteRect{selectRect.x + selectRect.width + ScalePx(8.0f), y, ScalePx(40.0f), ScalePx(44.0f)};
        const std::string label = FitText(line.equipment[index].name, 18.0f, selectRect.width - ScalePx(20.0f));

        if (deleteRect.y + deleteRect.height < listArea.y || selectRect.y > listArea.y + listArea.height) {
            y += rowHeight;
            continue;
        }

        if (GuiButton(selectRect, label.c_str())) {
            ui.selectedEquipment = index;
            ui.selectedItem = line.equipment[index].items.empty() ? -1 : 0;
            SyncRenameBuffers(data, ui);
        }

        DrawUiText(T1("label.items_short", std::to_string(static_cast<int>(line.equipment[index].items.size()))),
                   selectRect.x, selectRect.y + ScalePx(47.0f), 14.0f, Color{160, 170, 180, 255});

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

        y += rowHeight;
    }
    DrawScrollHint(listArea, contentHeight, ui.equipmentScroll);

    DrawUiText(T("label.add_equipment"), panel.x + ScalePx(12.0f), panel.y + panel.height - ScalePx(164.0f), 18.0f, RAYWHITE, true);
    DrawInputBox(Rectangle{panel.x + ScalePx(12.0f), panel.y + panel.height - ScalePx(126.0f), panel.width - ScalePx(24.0f), ScalePx(42.0f)}, ui.newEquipmentName, FocusField::NewEquipment, ui, T("placeholder.new_equipment"));
    const float equipmentButtonWidth = std::min(ScalePx(146.0f), panel.width * 0.4f);
    if (GuiButton(Rectangle{panel.x + ScalePx(12.0f), panel.y + panel.height - ScalePx(76.0f), equipmentButtonWidth, ScalePx(38.0f)}, T("button.add_equipment").c_str()) && !ui.newEquipmentName.empty()) {
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

    DrawInputBox(Rectangle{panel.x + ScalePx(20.0f) + equipmentButtonWidth, panel.y + panel.height - ScalePx(76.0f), panel.width - (ScalePx(32.0f) + equipmentButtonWidth), ScalePx(38.0f)}, ui.renameEquipmentName, FocusField::RenameEquipment, ui, T("placeholder.rename_equipment"));
    if (GuiButton(Rectangle{panel.x + ScalePx(12.0f), panel.y + panel.height - ScalePx(34.0f), equipmentButtonWidth, ScalePx(32.0f)}, T("button.save_name").c_str()) &&
        ui.selectedEquipment >= 0 && ui.selectedEquipment < static_cast<int>(line.equipment.size()) && !ui.renameEquipmentName.empty()) {
        if (!data.simulation.enabled) {
            line.equipment[ui.selectedEquipment].name = ui.renameEquipmentName;
            PersistIfAllowed(data, dataFile, ui);
        } else {
            PersistIfAllowed(data, dataFile, ui);
        }
    }
}

void DrawItemsPanel(const UiLayout& layout, AppData& data, AppUi& ui, const std::filesystem::path& dataFile) {
    const std::string panelTitle = T("panel.items");
    const Rectangle panel = Panel(layout.items.x, layout.items.y, layout.items.width, layout.items.height, panelTitle.c_str());

    if (ui.selectedLine < 0 || ui.selectedLine >= static_cast<int>(data.lines.size()) ||
        ui.selectedEquipment < 0 || ui.selectedEquipment >= static_cast<int>(data.lines[ui.selectedLine].equipment.size())) {
        DrawUiText(T("panel.items.locked"), panel.x + ScalePx(14.0f), panel.y + ScalePx(62.0f), 18.0f, Color{180, 190, 200, 255});
        return;
    }

    auto& equipment = data.lines[ui.selectedLine].equipment[ui.selectedEquipment];
    DrawUiText(FitText(equipment.name, 21.0f, panel.width - ScalePx(24.0f), true), panel.x + ScalePx(12.0f), panel.y + ScalePx(54.0f), 21.0f, RAYWHITE, true);
    const float footerTop = panel.y + panel.height - ScalePx(152.0f);
    const Rectangle listArea{panel.x + ScalePx(8.0f), panel.y + ScalePx(92.0f), panel.width - ScalePx(8.0f), footerTop - (panel.y + ScalePx(100.0f))};
    const float rowHeight = ScalePx(92.0f);
    const float contentHeight = static_cast<float>(equipment.items.size()) * rowHeight;
    HandleWheelScroll(listArea, contentHeight, ui.itemsScroll);
    float y = listArea.y - ui.itemsScroll;
    const float visibleBottom = listArea.y + listArea.height;

    for (int index = 0; index < static_cast<int>(equipment.items.size()) && y < visibleBottom; ++index) {
        auto& item = equipment.items[index];
        const int daysSince = DaysBetween(item.lastCheckedDate, EffectiveDate(data));
        std::string status = item.checkedToday ? T("status.done_today") : (daysSince > item.periodDays ? T("status.overdue") : (daysSince == item.periodDays ? T("status.due_today") : T("status.scheduled")));
        const float actionWidth = ScalePx(92.0f);
        const float deleteWidth = ScalePx(42.0f);
        Rectangle selectRect{panel.x + ScalePx(12.0f), y, panel.width - ScalePx(40.0f) - actionWidth - deleteWidth, ScalePx(58.0f)};
        Rectangle checkRect{selectRect.x + selectRect.width + ScalePx(8.0f), y + ScalePx(9.0f), actionWidth, ScalePx(40.0f)};
        Rectangle deleteRect{checkRect.x + checkRect.width + ScalePx(8.0f), y + ScalePx(9.0f), deleteWidth, ScalePx(40.0f)};
        const std::string title = FitText(item.name, 18.0f, selectRect.width - ScalePx(20.0f), true);
        const std::string meta = FitText(T2("meta.every_last", std::to_string(item.periodDays), item.lastCheckedDate), 15.0f, selectRect.width - ScalePx(10.0f));

        if (deleteRect.y + deleteRect.height < listArea.y || selectRect.y > visibleBottom) {
            y += rowHeight;
            continue;
        }

        if (GuiButton(selectRect, "")) {
            ui.selectedItem = index;
            SyncRenameBuffers(data, ui);
        }

        DrawUiText(title, selectRect.x + ScalePx(14.0f), selectRect.y + ScalePx(10.0f), 18.0f, RAYWHITE, true);
        DrawUiText(meta, selectRect.x + ScalePx(14.0f), selectRect.y + ScalePx(38.0f), 14.5f, Color{160, 170, 180, 255});
        DrawUiText(FitText(status, 15.5f, panel.width - (selectRect.width + ScalePx(56.0f))), checkRect.x, selectRect.y - ScalePx(18.0f), 15.5f,
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

        y += rowHeight;
    }
    DrawScrollHint(listArea, contentHeight, ui.itemsScroll);

    DrawRectangleRounded(Rectangle{panel.x + ScalePx(10.0f), footerTop - ScalePx(8.0f), panel.width - ScalePx(20.0f), ScalePx(146.0f)}, 0.12f, 8, Color{16, 24, 36, 220});
    DrawRectangleRoundedLines(Rectangle{panel.x + ScalePx(10.0f), footerTop - ScalePx(8.0f), panel.width - ScalePx(20.0f), ScalePx(146.0f)}, 0.12f, 8, std::max(1.0f, ScalePx(1.1f)), Color{48, 64, 86, 255});

    DrawUiText(T("label.add_item"), panel.x + ScalePx(18.0f), footerTop, 17.0f, RAYWHITE, true);
    const float itemInputWidth = std::max(ScalePx(180.0f), panel.width - ScalePx(258.0f));
    const float daysLabelX = panel.x + ScalePx(26.0f) + itemInputWidth;
    const float rowOneY = footerTop + ScalePx(24.0f);
    DrawInputBox(Rectangle{panel.x + ScalePx(18.0f), rowOneY, itemInputWidth, ScalePx(40.0f)}, ui.newItemName, FocusField::NewItem, ui, T("placeholder.new_item"));
    DrawUiText(T("label.days"), daysLabelX, footerTop + ScalePx(2.0f), 13.5f, Color{160, 170, 180, 255});
    GuiSpinner(Rectangle{daysLabelX, rowOneY, ScalePx(98.0f), ScalePx(40.0f)}, "", &ui.newItemPeriod, 1, 365, true);
    if (GuiButton(Rectangle{panel.x + panel.width - ScalePx(92.0f), rowOneY, ScalePx(74.0f), ScalePx(40.0f)}, T("button.add").c_str()) && !ui.newItemName.empty()) {
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

    DrawUiText(T("label.edit_item"), panel.x + ScalePx(18.0f), footerTop + ScalePx(72.0f), 17.0f, RAYWHITE, true);
    const float rowTwoY = footerTop + ScalePx(96.0f);
    DrawInputBox(Rectangle{panel.x + ScalePx(18.0f), rowTwoY, itemInputWidth, ScalePx(40.0f)}, ui.renameItemName, FocusField::RenameItem, ui, T("placeholder.rename_item"));
    DrawUiText(T("label.days"), daysLabelX, footerTop + ScalePx(74.0f), 13.5f, Color{160, 170, 180, 255});
    GuiSpinner(Rectangle{daysLabelX, rowTwoY, ScalePx(98.0f), ScalePx(40.0f)}, "", &ui.renameItemPeriod, 1, 365, true);
    if (GuiButton(Rectangle{panel.x + panel.width - ScalePx(92.0f), rowTwoY, ScalePx(74.0f), ScalePx(40.0f)}, T("button.save").c_str()) &&
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

void DrawDashboard(const UiLayout& layout, AppData& data, AppUi& ui, const std::filesystem::path& dataFile) {
    if (layout.dashboard.width <= 1.0f || layout.dashboard.height <= 1.0f) {
        return;
    }

    const std::string panelTitle = T("panel.dashboard");
    const Rectangle panel = Panel(layout.dashboard.x, layout.dashboard.y, layout.dashboard.width, layout.dashboard.height, panelTitle.c_str());
    const Rectangle contentArea{
        panel.x + ScalePx(8.0f),
        panel.y + ScalePx(52.0f),
        panel.width - ScalePx(8.0f),
        panel.height - ScalePx(60.0f)
    };
    int equipmentCount = 0;
    int itemCount = 0;
    for (const auto& line : data.lines) {
        equipmentCount += static_cast<int>(line.equipment.size());
        for (const auto& equipment : line.equipment) {
            itemCount += static_cast<int>(equipment.items.size());
        }
    }

    float totalContentHeight = ScalePx(42.0f) + ScalePx(76.0f) * 3.0f + ScalePx(86.0f) * 2.0f + ScalePx(98.0f) + ScalePx(92.0f);
    if (data.simulation.enabled) {
        totalContentHeight += ScalePx(220.0f);
    }
    HandleWheelScroll(contentArea, totalContentHeight, ui.dashboardScroll);
    BeginScissorMode(static_cast<int>(contentArea.x), static_cast<int>(contentArea.y), static_cast<int>(contentArea.width), static_cast<int>(contentArea.height));

    float y = contentArea.y - ui.dashboardScroll;

    bool simEnabled = data.simulation.enabled;
    if (GuiCheckBox(Rectangle{panel.x + ScalePx(16.0f), y, ScalePx(24.0f), ScalePx(24.0f)}, T("label.enable_simulation").c_str(), &simEnabled)) {
        data.simulation.enabled = simEnabled;
        RefreshStatuses(data);
        SetBanner(ui, data.simulation.enabled ? T("banner.simulation_enabled") : T("banner.simulation_disabled"));
    }
    y += ScalePx(42.0f);
    DrawInfoCard(Rectangle{panel.x + ScalePx(16.0f), y, panel.width - ScalePx(32.0f), ScalePx(76.0f)},
                 T("label.lines_title"), std::to_string(static_cast<int>(data.lines.size())), Color{71, 153, 255, 255});
    y += ScalePx(86.0f);
    DrawInfoCard(Rectangle{panel.x + ScalePx(16.0f), y, panel.width - ScalePx(32.0f), ScalePx(76.0f)},
                 T("label.equipment_title"), std::to_string(equipmentCount), Color{114, 197, 149, 255});
    y += ScalePx(86.0f);
    DrawInfoCard(Rectangle{panel.x + ScalePx(16.0f), y, panel.width - ScalePx(32.0f), ScalePx(76.0f)},
                 T("label.items_title"), std::to_string(itemCount), Color{241, 192, 84, 255});
    y += ScalePx(94.0f);
    DrawUiText(T("label.data_file"), panel.x + ScalePx(16.0f), y, 15.5f, Color{160, 170, 180, 255});
    DrawUiText(FitText(dataFile.string(), 14.0f, panel.width - ScalePx(32.0f)), panel.x + ScalePx(16.0f), y + ScalePx(24.0f), 14.0f, RAYWHITE);
    if (GuiButton(Rectangle{panel.x + ScalePx(16.0f), y + ScalePx(50.0f), panel.width - ScalePx(32.0f), ScalePx(36.0f)}, T("button.open_data_folder").c_str())) {
        OpenFolderInExplorer(dataFile.parent_path());
    }
    y += ScalePx(98.0f);
    if (data.simulation.enabled) {
        DrawUiText(T("label.simulation_date"), panel.x + ScalePx(16.0f), y, 17.0f, Color{255, 212, 112, 255}, true);
        GuiSpinner(Rectangle{panel.x + ScalePx(16.0f), y + ScalePx(30.0f), panel.width - ScalePx(32.0f), ScalePx(36.0f)}, T("label.year").c_str(), &data.simulation.year, 2020, 2100, true);
        GuiSpinner(Rectangle{panel.x + ScalePx(16.0f), y + ScalePx(76.0f), panel.width - ScalePx(32.0f), ScalePx(36.0f)}, T("label.month").c_str(), &data.simulation.month, 1, 12, true);
        GuiSpinner(Rectangle{panel.x + ScalePx(16.0f), y + ScalePx(122.0f), panel.width - ScalePx(32.0f), ScalePx(36.0f)}, T("label.day").c_str(), &data.simulation.day, 1, 31, true);
        if (GuiButton(Rectangle{panel.x + ScalePx(16.0f), y + ScalePx(170.0f), panel.width - ScalePx(32.0f), ScalePx(36.0f)}, T("button.jump_today").c_str())) {
            ResetSimulationToToday(data);
            RefreshStatuses(data);
            SetBanner(ui, T("banner.simulation_today"));
        }
        y += ScalePx(220.0f);
    }
    if (GuiButton(Rectangle{panel.x + ScalePx(16.0f), y, panel.width - ScalePx(32.0f), ScalePx(36.0f)}, T("button.refresh").c_str())) {
        RefreshStatuses(data);
    }
    if (GuiButton(Rectangle{panel.x + ScalePx(16.0f), y + ScalePx(46.0f), panel.width - ScalePx(32.0f), ScalePx(36.0f)}, T("button.load_sample").c_str())) {
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
    EndScissorMode();
    DrawScrollHint(contentArea, totalContentHeight, ui.dashboardScroll);

}

}  // namespace

}  // namespace factor

int main(int argc, char** argv) {
    using namespace factor;

    (void)argc;
    const std::filesystem::path root = ResolveProjectRoot((argv != nullptr && argv[0] != nullptr) ? argv[0] : const_cast<char*>("."));
    const std::filesystem::path dataFile = ResolveDataFile(root);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(kWindowWidth, kWindowHeight, "Factor Manager");
    SetWindowMinSize(kWindowMinWidth, kWindowMinHeight);
    SetTargetFPS(60);
    LoadUiFont(root);
    UpdateUiScale();
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
        UpdateUiScale();
        RefreshStatuses(data);
        ClampSelections(data, ui);
        const UiLayout layout = BuildLayout(data.simulation.enabled && ui.hideDashboardInSimulation);

        BeginDrawing();
        ClearBackground(Color{15, 20, 29, 255});

        DrawHeader(data, ui);
        DrawLinesPanel(layout, data, ui, dataFile);
        DrawEquipmentPanel(layout, data, ui, dataFile);
        DrawItemsPanel(layout, data, ui, dataFile);
        DrawDashboard(layout, data, ui, dataFile);
        DrawAlertsPanel(layout, data, ui);

        EndDrawing();
    }

    UnloadUiFont();
    CloseWindow();
    return 0;
}
