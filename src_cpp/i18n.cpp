#include "i18n.hpp"

#include <fstream>
#include <sstream>

extern "C" {
#include "cJSON.h"
}

namespace factor {

namespace {

std::string JsonString(cJSON* object, const char* key, const std::string& fallback = {}) {
    cJSON* item = cJSON_GetObjectItem(object, key);
    if (cJSON_IsString(item) && item->valuestring != nullptr) {
        return item->valuestring;
    }
    return fallback;
}

}  // namespace

bool Localizer::LoadFromDirectory(const std::filesystem::path& directory) {
    languages_.clear();
    activeIndex_ = 0;

    if (!std::filesystem::exists(directory)) {
        return false;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
            continue;
        }

        std::ifstream input(entry.path(), std::ios::binary);
        if (!input) {
            continue;
        }

        std::stringstream buffer;
        buffer << input.rdbuf();
        const std::string raw = buffer.str();

        cJSON* root = cJSON_Parse(raw.c_str());
        if (root == nullptr) {
            continue;
        }

        LanguageCatalog catalog;
        catalog.code = JsonString(root, "code", entry.path().stem().string());
        catalog.displayName = JsonString(root, "name", catalog.code);

        cJSON* strings = cJSON_GetObjectItem(root, "strings");
        if (cJSON_IsObject(strings)) {
            cJSON* child = strings->child;
            while (child != nullptr) {
                if (child->string != nullptr && cJSON_IsString(child) && child->valuestring != nullptr) {
                    catalog.strings[child->string] = child->valuestring;
                }
                child = child->next;
            }
        }

        if (!catalog.strings.empty()) {
            languages_.push_back(std::move(catalog));
        }

        cJSON_Delete(root);
    }

    return !languages_.empty();
}

bool Localizer::SetActiveCode(const std::string& code) {
    for (int index = 0; index < static_cast<int>(languages_.size()); ++index) {
        if (languages_[index].code == code) {
            activeIndex_ = index;
            return true;
        }
    }
    return false;
}

std::string Localizer::Translate(const std::string& key) const {
    if (languages_.empty()) {
        return key;
    }

    const auto& active = languages_[activeIndex_].strings;
    const auto activeIt = active.find(key);
    if (activeIt != active.end()) {
        return activeIt->second;
    }

    const auto& fallback = languages_.front().strings;
    const auto fallbackIt = fallback.find(key);
    if (fallbackIt != fallback.end()) {
        return fallbackIt->second;
    }

    return key;
}

}  // namespace factor
