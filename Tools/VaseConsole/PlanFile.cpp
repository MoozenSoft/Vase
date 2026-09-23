#include "PlanFile.h"

#include "Vase/Detail/Result.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace samples::plan
{
namespace
{

std::string_view Trim(std::string_view text)
{
    constexpr std::string_view kBlanks = " \t\r\n";
    const std::size_t begin = text.find_first_not_of(kBlanks);
    if (begin == std::string_view::npos)
    {
        return {};
    }
    const std::size_t end = text.find_last_not_of(kBlanks);
    return text.substr(begin, end - begin + 1);
}

using Outcome = vase::Result<std::vector<Entry>>;

} // namespace

vase::Result<std::vector<Entry>> ParsePlanFile(const std::filesystem::path& file)
{
    std::ifstream input(file, std::ios::binary);
    if (!input.is_open())
    {
        return Outcome::Err(vase::Error("cannot open plan file: " + file.string()));
    }

    std::vector<Entry> entries;
    std::string line;
    std::size_t lineNo = 0;
    while (std::getline(input, line))
    {
        ++lineNo;
        const std::string_view body = Trim(line);
        if (body.empty() || body.front() == '#')
        {
            continue;
        }
        const std::size_t splitAt = body.find_first_of(" \t");
        if (splitAt == std::string_view::npos)
        {
            return Outcome::Err(vase::Error("plan line " + std::to_string(lineNo) + ": missing binary path"));
        }

        Entry entry;
        entry.Id = std::string(body.substr(0, splitAt));
        entry.BinaryPath = std::filesystem::path{std::string{Trim(body.substr(splitAt + 1))}};
        if (entry.Id.empty() || entry.BinaryPath.empty())
        {
            return Outcome::Err(vase::Error("plan line " + std::to_string(lineNo) + ": empty id or path"));
        }

        std::error_code ec;
        if (!std::filesystem::exists(entry.BinaryPath, ec) || ec)
        {
            return Outcome::Err(vase::Error("plan line " + std::to_string(lineNo) +
                                            ": binary not found: " + entry.BinaryPath.string()));
        }
        entries.push_back(std::move(entry));
    }

    if (entries.empty())
    {
        return Outcome::Err(vase::Error("plan file has no entries: " + file.string()));
    }
    return Outcome::Ok(std::move(entries));
}

} // namespace samples::plan
