#pragma once

#include <filesystem>
#include <string>

#include "model.hpp"

namespace factor {

AppData LoadAppData(const std::filesystem::path& path);
bool SaveAppData(const AppData& data, const std::filesystem::path& path);

std::string TodayString();
std::string EffectiveDate(const AppData& data);
int DaysBetween(const std::string& from, const std::string& to);
bool IsValidDateString(const std::string& value);
void RefreshStatuses(AppData& data);
DueSummary BuildDueSummary(const AppData& data);
AppData BuildSampleData();

}  // namespace factor
