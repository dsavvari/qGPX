// SPDX-License-Identifier: MIT
#include "GPXWriter.hpp"

#include <expat.h>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace qGpx
{
namespace core
{
namespace
{

std::string escapeXml(const std::string& text)
{
    std::string result;
    for (char c : text)
    {
        switch (c)
        {
        case '&':
            result += "&amp;";
            break;
        case '<':
            result += "&lt;";
            break;
        case '>':
            result += "&gt;";
            break;
        case '"':
            result += "&quot;";
            break;
        case '\'':
            result += "&apos;";
            break;
        case '\r':
            result += "&#13;";
            break;
        case '\n':
            result += "&#10;";
            break;
        case '\t':
            result += "&#9;";
            break;
        default:
            result += c;
        }
    }
    return result;
}

std::string decimal(double value)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
    std::string text = output.str();
    const auto exponentPosition = text.find_first_of("eE");
    if (exponentPosition == std::string::npos)
    {
        return text;
    }

    // GPX's xsd:decimal does not permit scientific notation.
    const int exponent = std::stoi(text.substr(exponentPosition + 1));
    const bool negative = text.front() == '-';
    std::string digits = text.substr(negative ? 1 : 0, exponentPosition - (negative ? 1 : 0));
    const auto dot = digits.find('.');
    const int decimalPosition =
        static_cast<int>(dot == std::string::npos ? digits.size() : dot) + exponent;
    if (dot != std::string::npos)
    {
        digits.erase(dot, 1);
    }
    if (decimalPosition <= 0)
    {
        digits = "0." + std::string(-decimalPosition, '0') + digits;
    }
    else if (decimalPosition >= static_cast<int>(digits.size()))
    {
        digits.append(decimalPosition - digits.size(), '0');
    }
    else
    {
        digits.insert(decimalPosition, ".");
    }
    return (negative ? "-" : "") + digits;
}

Result<std::string> timestamp(std::chrono::system_clock::time_point time)
{
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch());
    auto fraction =
        std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch() - seconds);
    auto rawTime =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::time_point(seconds));
    if (fraction.count() < 0)
    {
        --rawTime;
        fraction += std::chrono::seconds(1);
    }
    std::tm utc{};
#ifdef _WIN32
    const bool valid = gmtime_s(&utc, &rawTime) == 0;
#else
    const bool valid = gmtime_r(&rawTime, &utc) != nullptr;
#endif
    if (!valid || utc.tm_year < -1899 || utc.tm_year > 8099)
    {
        return Result<std::string>::makeFailure(
            "Timestamp is outside the supported years 0001-9999");
    }
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S");
    if (fraction.count() != 0)
    {
        output << '.' << std::setw(9) << std::setfill('0') << fraction.count();
    }
    output << 'Z';
    return Result<std::string>::makeSuccess(output.str());
}

} // namespace

Result<std::string> GPXWriter::serialize(const GPXData& data) const
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           << "<gpx xmlns=\"http://www.topografix.com/GPX/1/1\" version=\"1.1\" creator=\""
           << escapeXml(data.creator.empty() ? "qGPX" : data.creator) << "\">\n";
    for (const auto& track : data.tracks)
    {
        output << "  <trk>\n";
        if (!track.name.empty())
        {
            output << "    <name>" << escapeXml(track.name) << "</name>\n";
        }
        if (!track.description.empty())
        {
            output << "    <desc>" << escapeXml(track.description) << "</desc>\n";
        }
        for (const auto& segment : track.segments)
        {
            output << "    <trkseg>\n";
            for (const auto& point : segment.points)
            {
                if (!std::isfinite(point.latitude) || point.latitude < -90 || point.latitude > 90)
                {
                    return Result<std::string>::makeFailure(
                        "Latitude must be finite and in [-90, 90]");
                }
                if (!std::isfinite(point.longitude) || point.longitude < -180 ||
                    point.longitude >= 180)
                {
                    return Result<std::string>::makeFailure(
                        "GPX 1.1 longitude must be finite and in [-180, 180)");
                }
                if (point.elevation && !std::isfinite(*point.elevation))
                {
                    return Result<std::string>::makeFailure("Elevation must be finite");
                }
                output << "      <trkpt lat=\"" << decimal(point.latitude) << "\" lon=\""
                       << decimal(point.longitude) << "\">";
                if (point.elevation)
                {
                    output << "<ele>" << decimal(*point.elevation) << "</ele>";
                }
                if (point.time)
                {
                    const auto formatted = timestamp(*point.time);
                    if (!formatted.success)
                    {
                        return Result<std::string>::makeFailure(formatted.errorMessage);
                    }
                    output << "<time>" << *formatted.value << "</time>";
                }
                output << "</trkpt>\n";
            }
            output << "    </trkseg>\n";
        }
        output << "  </trk>\n";
    }
    output << "</gpx>\n";
    std::string xml = output.str();

    // Expat verifies UTF-8 and XML 1.0 characters, including user-supplied text.
    XML_Parser parser = XML_ParserCreate("UTF-8");
    if (!parser)
    {
        return Result<std::string>::makeFailure("Failed to create XML validation parser");
    }
    for (std::size_t offset = 0; offset < xml.size();)
    {
        const auto length = std::min<std::size_t>(xml.size() - offset, 4096);
        const bool last = offset + length == xml.size();
        if (XML_Parse(parser, xml.data() + offset, static_cast<int>(length), last) ==
            XML_STATUS_ERROR)
        {
            const std::string error = XML_ErrorString(XML_GetErrorCode(parser));
            XML_ParserFree(parser);
            return Result<std::string>::makeFailure("Invalid GPX XML text: " + error);
        }
        offset += length;
    }
    XML_ParserFree(parser);
    return Result<std::string>::makeSuccess(std::move(xml));
}

Result<bool> GPXWriter::saveFile(const GPXData& data, const std::string& filePath) const
{
    if (filePath.empty() || filePath.find('\0') != std::string::npos)
    {
        return Result<bool>::makeFailure("File path is empty or contains a null character");
    }
    const auto result = serialize(data);
    if (!result.success)
    {
        return Result<bool>::makeFailure(result.errorMessage);
    }
    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        return Result<bool>::makeFailure("Failed to open file for writing: " + filePath);
    }
    file << *result.value;
    file.close();
    if (file.fail())
    {
        return Result<bool>::makeFailure("Failed to write or close file: " + filePath);
    }
    return Result<bool>::makeSuccess(true);
}

} // namespace core
} // namespace qGpx
