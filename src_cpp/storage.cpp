#include "storage.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>

extern "C" {
#include "cJSON.h"
}

namespace factor {

namespace {

constexpr int kMinimumPeriodDays = 1;
constexpr int kMaximumPeriodDays = 365;
constexpr const char* kEpochDate = "1970-01-01";

std::tm ParseDate(const std::string& value) {
    std::tm tm{};
    int year = 1970;
    int month = 1;
    int day = 1;
    std::sscanf(value.c_str(), "%d-%d-%d", &year, &month, &day);
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = 12;
    return tm;
}

std::string JsonString(cJSON* object, const char* key, const std::string& fallback = {}) {
    cJSON* item = cJSON_GetObjectItem(object, key);
    if (cJSON_IsString(item) && item->valuestring != nullptr) {
        return item->valuestring;
    }
    return fallback;
}

int JsonInt(cJSON* object, const char* key, int fallback = 0) {
    cJSON* item = cJSON_GetObjectItem(object, key);
    return cJSON_IsNumber(item) ? item->valueint : fallback;
}

std::string SanitiseName(const std::string& value, const std::string& fallback) {
    if (value.empty()) {
        return fallback;
    }
    return value;
}

int ClampPeriod(int periodDays) {
    return std::clamp(periodDays, kMinimumPeriodDays, kMaximumPeriodDays);
}

void NormaliseData(AppData& data) {
    if (!IsValidDateString(data.currentDate)) {
        data.currentDate = TodayString();
    }

    if (!IsValidDateString(EffectiveDate(data))) {
        const std::tm local = ParseDate(TodayString());
        data.simulation.year = local.tm_year + 1900;
        data.simulation.month = local.tm_mon + 1;
        data.simulation.day = local.tm_mday;
    }

    for (auto& line : data.lines) {
        line.name = SanitiseName(line.name, "Unnamed line");
        for (auto& equipment : line.equipment) {
            equipment.name = SanitiseName(equipment.name, "Unnamed equipment");
            for (auto& item : equipment.items) {
                item.name = SanitiseName(item.name, "Unnamed item");
                item.periodDays = ClampPeriod(item.periodDays);
                if (!IsValidDateString(item.lastCheckedDate)) {
                    item.lastCheckedDate = kEpochDate;
                }
                item.checkedToday = false;
            }
        }
    }
}

}  // namespace

std::string TodayString() {
    std::time_t now = std::time(nullptr);
    std::tm local = *std::localtime(&now);
    std::array<char, 11> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%04d-%02d-%02d",
                  local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
    return buffer.data();
}

bool IsValidDateString(const std::string& value) {
    if (value.size() != 10 || value[4] != '-' || value[7] != '-') {
        return false;
    }

    const std::tm parsed = ParseDate(value);
    if (parsed.tm_mon < 0 || parsed.tm_mon > 11 || parsed.tm_mday < 1 || parsed.tm_mday > 31) {
        return false;
    }

    std::tm copy = parsed;
    copy.tm_isdst = -1;
    if (std::mktime(&copy) == -1) {
        return false;
    }

    return copy.tm_year == parsed.tm_year &&
           copy.tm_mon == parsed.tm_mon &&
           copy.tm_mday == parsed.tm_mday;
}

std::string EffectiveDate(const AppData& data) {
    if (!data.simulation.enabled) {
        return TodayString();
    }

    std::array<char, 11> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%04d-%02d-%02d",
                  data.simulation.year, data.simulation.month, data.simulation.day);
    return buffer.data();
}

int DaysBetween(const std::string& from, const std::string& to) {
    std::tm start = ParseDate(from);
    std::tm end = ParseDate(to);
    const std::time_t startTime = std::mktime(&start);
    const std::time_t endTime = std::mktime(&end);
    const double diffSeconds = std::difftime(endTime, startTime);
    return static_cast<int>(diffSeconds / (60 * 60 * 24));
}

void RefreshStatuses(AppData& data) {
    data.currentDate = EffectiveDate(data);

    for (auto& line : data.lines) {
        for (auto& equipment : line.equipment) {
            for (auto& item : equipment.items) {
                item.checkedToday = (item.lastCheckedDate == data.currentDate);
            }
        }
    }
}

DueSummary BuildDueSummary(const AppData& data) {
    DueSummary summary;
    const std::string effectiveDate = EffectiveDate(data);

    for (const auto& line : data.lines) {
        for (const auto& equipment : line.equipment) {
            for (const auto& item : equipment.items) {
                const int daysSince = DaysBetween(item.lastCheckedDate, effectiveDate);
                if (item.checkedToday) {
                    continue;
                }

                if (daysSince > item.periodDays) {
                    summary.overdue.push_back(MaintenanceAlert{
                        line.name,
                        equipment.name,
                        item.name,
                        daysSince - item.periodDays
                    });
                } else if (daysSince == item.periodDays) {
                    summary.dueToday.push_back(MaintenanceAlert{
                        line.name,
                        equipment.name,
                        item.name,
                        0
                    });
                }
            }
        }
    }

    const auto sorter = [](const MaintenanceAlert& left, const MaintenanceAlert& right) {
        if (left.lineName != right.lineName) return left.lineName < right.lineName;
        if (left.equipmentName != right.equipmentName) return left.equipmentName < right.equipmentName;
        return left.slotName < right.slotName;
    };
    std::sort(summary.dueToday.begin(), summary.dueToday.end(), sorter);
    std::sort(summary.overdue.begin(), summary.overdue.end(), sorter);
    return summary;
}

AppData BuildSampleData() {
    AppData data;
    data.currentDate = TodayString();
    data.simulation.enabled = false;

    std::tm local = ParseDate(data.currentDate);
    data.simulation.year = local.tm_year + 1900;
    data.simulation.month = local.tm_mon + 1;
    data.simulation.day = local.tm_mday;

    ProductionLine line;
    line.name = "Line A";

    Equipment equipment;
    equipment.name = "Packaging Station";
    equipment.items.push_back({"Lubrication", 7, data.currentDate, true});
    equipment.items.push_back({"Sensor Check", 14, "2026-01-10", false});

    line.equipment.push_back(equipment);
    line.equipment.push_back(Equipment{"Sealing Unit", {{"Temperature Calibration", 30, "2025-12-20", false}}});
    data.lines.push_back(line);
    data.lines.push_back(ProductionLine{"Line B", {Equipment{"Inspection Camera", {{"Lens Cleaning", 5, "2026-01-18", false}}}}});

    NormaliseData(data);
    RefreshStatuses(data);
    return data;
}

AppData LoadAppData(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        AppData sample = BuildSampleData();
        RefreshStatuses(sample);
        return sample;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        AppData sample = BuildSampleData();
        RefreshStatuses(sample);
        return sample;
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    const std::string raw = buffer.str();

    cJSON* root = cJSON_Parse(raw.c_str());
    if (root == nullptr) {
        AppData sample = BuildSampleData();
        RefreshStatuses(sample);
        return sample;
    }

    AppData data;
    data.currentDate = JsonString(root, "last_date", TodayString());
    const std::tm local = ParseDate(TodayString());
    data.simulation.year = local.tm_year + 1900;
    data.simulation.month = local.tm_mon + 1;
    data.simulation.day = local.tm_mday;

    cJSON* linesArray = cJSON_GetObjectItem(root, "lines");
    if (cJSON_IsArray(linesArray)) {
        cJSON* lineObject = nullptr;
        cJSON_ArrayForEach(lineObject, linesArray) {
            ProductionLine line;
            line.name = JsonString(lineObject, "name", "Unnamed line");

            cJSON* equipmentArray = cJSON_GetObjectItem(lineObject, "equipments");
            if (cJSON_IsArray(equipmentArray)) {
                cJSON* equipmentObject = nullptr;
                cJSON_ArrayForEach(equipmentObject, equipmentArray) {
                    Equipment equipment;
                    equipment.name = JsonString(equipmentObject, "name", "Unnamed equipment");

                    cJSON* itemsArray = cJSON_GetObjectItem(equipmentObject, "items");
                    if (cJSON_IsArray(itemsArray)) {
                        cJSON* itemObject = nullptr;
                        cJSON_ArrayForEach(itemObject, itemsArray) {
                            MaintenanceItem item;
                            item.name = JsonString(itemObject, "name", "Unnamed item");
                            item.periodDays = JsonInt(itemObject, "period", 30);
                            item.lastCheckedDate = JsonString(itemObject, "last_checked", "1970-01-01");
                            equipment.items.push_back(item);
                        }
                    }

                    line.equipment.push_back(equipment);
                }
            }

            data.lines.push_back(line);
        }
    }

    cJSON_Delete(root);
    NormaliseData(data);
    RefreshStatuses(data);
    return data;
}

bool SaveAppData(const AppData& data, const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "last_date", TodayString().c_str());

    cJSON* linesArray = cJSON_CreateArray();
    for (const auto& line : data.lines) {
        cJSON* lineObject = cJSON_CreateObject();
        cJSON_AddStringToObject(lineObject, "name", line.name.c_str());

        cJSON* equipmentArray = cJSON_CreateArray();
        for (const auto& equipment : line.equipment) {
            cJSON* equipmentObject = cJSON_CreateObject();
            cJSON_AddStringToObject(equipmentObject, "name", equipment.name.c_str());
            cJSON_AddNumberToObject(equipmentObject, "scroll_offset", 0);

            cJSON* itemsArray = cJSON_CreateArray();
            for (const auto& item : equipment.items) {
                cJSON* itemObject = cJSON_CreateObject();
                cJSON_AddStringToObject(itemObject, "name", item.name.c_str());
                cJSON_AddNumberToObject(itemObject, "period", item.periodDays);
                cJSON_AddStringToObject(itemObject, "last_checked", item.lastCheckedDate.c_str());
                cJSON_AddBoolToObject(itemObject, "is_checked", item.checkedToday);
                cJSON_AddItemToArray(itemsArray, itemObject);
            }

            cJSON_AddItemToObject(equipmentObject, "items", itemsArray);
            cJSON_AddItemToArray(equipmentArray, equipmentObject);
        }

        cJSON_AddItemToObject(lineObject, "equipments", equipmentArray);
        cJSON_AddItemToArray(linesArray, lineObject);
    }

    cJSON_AddItemToObject(root, "lines", linesArray);

    char* json = cJSON_Print(root);
    cJSON_Delete(root);

    if (json == nullptr) {
        return false;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        std::free(json);
        return false;
    }

    output << json;
    std::free(json);
    return true;
}

}  // namespace factor
