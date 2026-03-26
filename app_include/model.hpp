#pragma once

#include <string>
#include <vector>

namespace factor {

struct MaintenanceItem {
    std::string name;
    int periodDays = 30;
    std::string lastCheckedDate = "1970-01-01";
    bool checkedToday = false;
};

struct Equipment {
    std::string name;
    std::vector<MaintenanceItem> items;
};

struct ProductionLine {
    std::string name;
    std::vector<Equipment> equipment;
};

struct SimulationState {
    bool enabled = false;
    int year = 2025;
    int month = 1;
    int day = 1;
};

struct AppData {
    std::vector<ProductionLine> lines;
    std::string currentDate;
    SimulationState simulation;
};

struct DueSummary {
    std::vector<std::string> dueToday;
    std::vector<std::string> overdue;
};

}  // namespace factor
