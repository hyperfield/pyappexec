#ifndef INIREADER_H
#define INIREADER_H

#include <map>
#include <string>


class INIReader
{
public:
    explicit INIReader(const std::string& filename);

    int ParseError() const;
    std::string Get(const std::string& section, const std::string& name, const std::string& default_value) const;

private:
    int error_{0};
    std::map<std::string, std::map<std::string, std::string>> values_;
};

#endif
