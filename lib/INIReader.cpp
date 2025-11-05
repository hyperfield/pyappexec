#include "INIReader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace {

std::string trim(const std::string& input)
{
    auto begin = std::find_if_not(input.begin(), input.end(), [](unsigned char ch) {
        return std::isspace(ch);
    });
    auto end = std::find_if_not(input.rbegin(), input.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    }).base();

    if (begin >= end) {
        return {};
    }

    return std::string(begin, end);
}

}


INIReader::INIReader(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file) {
        error_ = -1;
        return;
    }

    std::string currentSection;
    std::string line;

    while (std::getline(file, line)) {
        auto semicolon = line.find(';');
        if (semicolon != std::string::npos) {
            line = line.substr(0, semicolon);
        }

        auto hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }

        line = trim(line);
        if (line.empty()) {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            currentSection = trim(line.substr(1, line.size() - 2));
            continue;
        }

        auto equalsPos = line.find('=');
        if (equalsPos == std::string::npos) {
            error_ = 1;
            continue;
        }

        std::string key = trim(line.substr(0, equalsPos));
        std::string value = trim(line.substr(equalsPos + 1));

        values_[currentSection][key] = value;
    }
}


int INIReader::ParseError() const
{
    return error_;
}


std::string INIReader::Get(const std::string& section, const std::string& name, const std::string& default_value) const
{
    auto sectionIt = values_.find(section);
    if (sectionIt == values_.end()) {
        return default_value;
    }

    auto valueIt = sectionIt->second.find(name);
    if (valueIt == sectionIt->second.end()) {
        return default_value;
    }

    return valueIt->second;
}
