#pragma once

#include <set>
#include <string>
#include <vector>

std::vector<std::string_view> SplitIntoWords(std::string_view text);

// возвращает множество непустых строк из произвольного контейнера строк
template <typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string, std::less<>> result;
    for (const auto str : strings) {
        if (!str.empty()) {
            result.emplace(std::string(str));
        }
    }
    return result;
}