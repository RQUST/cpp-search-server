#pragma once

#include <iostream>
#include <map>
#include <set>  
#include <vector>
#include <numeric>
#include <string>
#include <string_view>

std::vector<std::string> SplitIntoWords(const std::string& text);

std::vector<std::string_view > SplitIntoWords(const std::string_view& text);

template <typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& strings)
{
    std::set<std::string, std::less<>> non_empty_strings;
    for (const auto& str : strings) {
        if (!str.empty()) {
            non_empty_strings.emplace(std::string(str));
        }
    }
    return non_empty_strings;
}