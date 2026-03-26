#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace factor {

struct LanguageCatalog {
    std::string code;
    std::string displayName;
    std::map<std::string, std::string> strings;
};

class Localizer {
public:
    bool LoadFromDirectory(const std::filesystem::path& directory);
    bool SetActiveCode(const std::string& code);
    std::string Translate(const std::string& key) const;

    [[nodiscard]] const std::vector<LanguageCatalog>& Languages() const { return languages_; }
    [[nodiscard]] int ActiveIndex() const { return activeIndex_; }
    [[nodiscard]] bool Empty() const { return languages_.empty(); }

private:
    std::vector<LanguageCatalog> languages_;
    int activeIndex_ = 0;
};

}  // namespace factor
