//  GPXParser.cpp
// Implementation of GPXParser
//
// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#include "GPXParser.hpp"

#include <expat.h>

#include <charconv>
#include <cmath>
#include <cctype>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>

namespace qGpx
{
namespace core
{

// Parser State - Tracks current position in XML tree
struct ParserState
{
    XML_Parser parser = nullptr;
    GPXData gpxData;
    Track currentTrack;
    TrackSegment currentSegment;
    TrackPoint* currentPoint = nullptr;

    // Current element tracking
    enum class Element
    {
        None,
        GPX,
        Track,
        TrackSegment,
        TrackPoint,
        Elevation,
        Time,
        Name,
        Type,
        Description
    };

    static bool isAllowedParentElement(Element parent, Element child)
    {
        switch (child)
        {
        case Element::GPX:
            return parent == Element::None;
        case Element::Track:
            return parent == Element::GPX || parent == Element::None;
        case Element::TrackSegment:
            return parent == Element::Track;
        case Element::TrackPoint:
            return parent == Element::TrackSegment;
        case Element::Elevation:
        case Element::Time:
            return parent == Element::TrackPoint;
        case Element::Name:
        case Element::Type:
        case Element::Description:
            return parent == Element::Track;
        default:
            return true;
        }
    }

    std::vector<Element> elementStack;
    std::string characterData;
    std::string errorMessage;

    void pushElement(Element elem)
    {
        const Element parent = currentElement();
        if (!isAllowedParentElement(parent, elem))
        {
            currentPoint = nullptr;
            errorMessage = "Malformed GPX nesting";
            XML_StopParser(parser, XML_FALSE);
            return;
        }
        elementStack.push_back(elem);
    }

    void popElement()
    {
        if (!elementStack.empty())
        {
            elementStack.pop_back();
        }
    }

    Element currentElement() const
    {
        return elementStack.empty() ? Element::None : elementStack.back();
    }

    void clearCharacterData()
    {
        characterData.clear();
    }
};

static bool parseFiniteDouble(const std::string& value, double& result)
{
    std::istringstream input(value);
    input.imbue(std::locale::classic());

    double parsed = 0.0;
    input >> parsed;
    if (input.fail() || !std::isfinite(parsed))
    {
        return false;
    }

    input >> std::ws;
    if (!input.eof())
    {
        return false;
    }

    result = parsed;
    return true;
}

static void stopWithError(ParserState* state, const std::string& message)
{
    state->errorMessage = message;
    XML_StopParser(state->parser, XML_FALSE);
}

// Expat Callback Handlers
static void XMLCALL startElementHandler(void* userData, const char* name, const char** atts)
{
    auto* state = static_cast<ParserState*>(userData);
    state->clearCharacterData();

    // Parse element name (ignore namespace prefixes for simplicity)
    std::string elementName(name);
    size_t colonPos = elementName.find(':');
    if (colonPos != std::string::npos)
    {
        elementName = elementName.substr(colonPos + 1);
    }

    // Handle different elements
    if (elementName == "gpx")
    {
        state->pushElement(ParserState::Element::GPX);

        // Parse creator attribute
        for (int i = 0; atts[i]; i += 2)
        {
            if (std::strcmp(atts[i], "creator") == 0)
            {
                state->gpxData.creator = atts[i + 1];
            }
        }
    }
    else if (elementName == "trk")
    {
        state->pushElement(ParserState::Element::Track);
        state->currentTrack = Track(); // Reset current track
    }
    else if (elementName == "trkseg")
    {
        state->currentPoint = nullptr;
        state->pushElement(ParserState::Element::TrackSegment);
        state->currentSegment = TrackSegment(); // Reset current segment
    }
    else if (elementName == "trkpt")
    {
        const auto parent = state->currentElement();
        if (parent != ParserState::Element::TrackSegment)
        {
            state->currentPoint = nullptr;
            stopWithError(state, "Malformed GPX nesting");
            return;
        }

        state->pushElement(ParserState::Element::TrackPoint);

        // Parse lat/lon attributes
        const char* latitude = nullptr;
        const char* longitude = nullptr;

        for (int i = 0; atts[i]; i += 2)
        {
            if (std::strcmp(atts[i], "lat") == 0)
            {
                latitude = atts[i + 1];
            }
            else if (std::strcmp(atts[i], "lon") == 0)
            {
                longitude = atts[i + 1];
            }
        }

        if (!latitude || !longitude)
        {
            stopWithError(state, "Track point is missing latitude or longitude");
            return;
        }

        double lat = 0.0;
        double lon = 0.0;
        if (!parseFiniteDouble(latitude, lat) || lat < -90.0 || lat > 90.0)
        {
            stopWithError(state, "Invalid latitude value");
            return;
        }
        if (!parseFiniteDouble(longitude, lon) || lon < -180.0 || lon > 180.0)
        {
            stopWithError(state, "Invalid longitude value");
            return;
        }

        state->currentSegment.points.emplace_back(lat, lon);
        state->currentPoint = &state->currentSegment.points.back();
    }
    else if (elementName == "ele")
    {
        if (state->currentElement() != ParserState::Element::TrackPoint)
        {
            state->currentPoint = nullptr;
            stopWithError(state, "Malformed GPX nesting");
            return;
        }
        state->pushElement(ParserState::Element::Elevation);
    }
    else if (elementName == "time")
    {
        if (state->currentElement() != ParserState::Element::TrackPoint)
        {
            state->currentPoint = nullptr;
            stopWithError(state, "Malformed GPX nesting");
            return;
        }
        state->pushElement(ParserState::Element::Time);
    }
    else if (elementName == "name")
    {
        if (state->currentElement() != ParserState::Element::Track)
        {
            state->currentPoint = nullptr;
            stopWithError(state, "Malformed GPX nesting");
            return;
        }
        state->pushElement(ParserState::Element::Name);
    }
    else if (elementName == "type")
    {
        if (state->currentElement() != ParserState::Element::Track)
        {
            state->currentPoint = nullptr;
            stopWithError(state, "Malformed GPX nesting");
            return;
        }
        state->pushElement(ParserState::Element::Type);
    }
    else if (elementName == "desc")
    {
        if (state->currentElement() != ParserState::Element::Track)
        {
            state->currentPoint = nullptr;
            stopWithError(state, "Malformed GPX nesting");
            return;
        }
        state->pushElement(ParserState::Element::Description);
    }
    else
    {
        // Unknown element, push None to maintain stack depth
        state->pushElement(ParserState::Element::None);
    }
}

static void XMLCALL endElementHandler(void* userData, const char* /* name */)
{
    auto* state = static_cast<ParserState*>(userData);

    ParserState::Element elem = state->currentElement();

    // Process accumulated character data
    switch (elem)
    {
    case ParserState::Element::Elevation:
        if (state->currentPoint)
        {
            double elevation = 0.0;
            if (!parseFiniteDouble(state->characterData, elevation))
            {
                stopWithError(state, "Invalid elevation value");
                return;
            }
            state->currentPoint->elevation = elevation;
        }
        break;

    case ParserState::Element::Time:
        if (state->currentPoint && !state->characterData.empty())
        {
            auto parsedTime = GPXParser::parseTime(state->characterData);
            if (!parsedTime.has_value())
            {
                stopWithError(state, "Invalid timestamp value");
                return;
            }
            state->currentPoint->time = parsedTime;
        }
        break;

    case ParserState::Element::Name:
        if (!state->characterData.empty())
        {
            state->currentTrack.name = state->characterData;
        }
        break;

    case ParserState::Element::Description:
        if (!state->characterData.empty())
        {
            state->currentTrack.description = state->characterData;
        }
        break;

    case ParserState::Element::TrackPoint:
        state->currentPoint = nullptr;
        break;

    case ParserState::Element::TrackSegment:
        // End of segment - add to current track
        if (!state->currentSegment.points.empty())
        {
            state->currentTrack.segments.push_back(std::move(state->currentSegment));
        }
        state->currentSegment = TrackSegment();
        state->currentPoint = nullptr;
        break;

    case ParserState::Element::Track:
        // End of track - add to GPX data
        if (!state->currentTrack.segments.empty())
        {
            state->gpxData.tracks.push_back(std::move(state->currentTrack));
        }
        state->currentTrack = Track();
        state->currentPoint = nullptr;
        break;

    default:
        break;
    }

    state->popElement();
    state->clearCharacterData();
}

static void XMLCALL characterDataHandler(void* userData, const char* s, int len)
{
    auto* state = static_cast<ParserState*>(userData);

    // Only accumulate character data for relevant elements
    switch (state->currentElement())
    {
    case ParserState::Element::Elevation:
    case ParserState::Element::Time:
    case ParserState::Element::Name:
    case ParserState::Element::Type:
    case ParserState::Element::Description:
        state->characterData.append(s, len);
        break;
    default:
        break;
    }
}

// GPXParser Implementation
Result<GPXData> GPXParser::parse(const std::string& xmlContent)
{
    if (xmlContent.empty())
    {
        return Result<GPXData>::Failure("XML content is empty");
    }

    // Create expat parser
    XML_Parser parser = XML_ParserCreate(nullptr);
    if (!parser)
    {
        return Result<GPXData>::Failure("Failed to create XML parser");
    }

    // Set up parser state
    ParserState state;
    state.parser = parser;
    XML_SetUserData(parser, &state);
    XML_SetElementHandler(parser, startElementHandler, endElementHandler);
    XML_SetCharacterDataHandler(parser, characterDataHandler);

    // Parse the XML content
    int parseResult = XML_Parse(parser, xmlContent.c_str(), static_cast<int>(xmlContent.size()), 1);

    if (parseResult == XML_STATUS_ERROR)
    {
        if (!state.errorMessage.empty())
        {
            XML_ParserFree(parser);
            return Result<GPXData>::Failure(state.errorMessage);
        }

        std::string errorMsg = "XML parsing error at line ";
        errorMsg += std::to_string(XML_GetCurrentLineNumber(parser));
        errorMsg += ": ";
        errorMsg += XML_ErrorString(XML_GetErrorCode(parser));

        XML_ParserFree(parser);
        return Result<GPXData>::Failure(errorMsg);
    }

    XML_ParserFree(parser);

    // Validate we got some data
    if (state.gpxData.tracks.empty())
    {
        return Result<GPXData>::Failure("No tracks found in GPX file");
    }

    return Result<GPXData>::Success(std::move(state.gpxData));
}

Result<GPXData> GPXParser::parseFile(const std::string& filePath)
{
    // Read file content
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        return Result<GPXData>::Failure("Failed to open file: " + filePath);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string xmlContent = buffer.str();

    return parse(xmlContent);
}

std::optional<std::chrono::system_clock::time_point>
GPXParser::parseTime(const std::string& timeStr)
{
    if (timeStr.empty())
    {
        return std::nullopt;
    }

    std::string value = timeStr;
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
    {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
    {
        value.pop_back();
    }
    if (value.empty())
    {
        return std::nullopt;
    }

    std::string offsetPart;
    if (value.back() == 'Z')
    {
        offsetPart = "Z";
        value.pop_back();
    }
    else
    {
        const auto tzPos = value.find_last_of("+-");
        if (tzPos != std::string::npos && tzPos > 10)
        {
            offsetPart = value.substr(tzPos);
            value = value.substr(0, tzPos);
        }
    }

    std::string base = value;
    std::string fractionText;
    const auto decimalPos = base.find('.');
    if (decimalPos != std::string::npos)
    {
        fractionText = base.substr(decimalPos + 1);
        base = base.substr(0, decimalPos);
    }

    std::tm tm = {};
    std::istringstream ss(base);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (ss.fail() || !ss.eof())
    {
        return std::nullopt;
    }

    if (!fractionText.empty())
    {
        for (char c : fractionText)
        {
            if (!std::isdigit(static_cast<unsigned char>(c)))
            {
                return std::nullopt;
            }
        }
    }

    std::chrono::nanoseconds fractional{0};
    if (!fractionText.empty())
    {
        std::string normalized = fractionText;
        if (normalized.size() > 9)
        {
            normalized.resize(9);
        }
        normalized.resize(9, '0');

        int64_t valueNs = 0;
        const auto [ptr, ec] =
            std::from_chars(normalized.data(), normalized.data() + normalized.size(), valueNs);
        if (ec != std::errc{} || ptr != normalized.data() + normalized.size())
        {
            return std::nullopt;
        }
        fractional = std::chrono::nanoseconds(valueNs);
    }

    std::chrono::hours tzHours{0};
    std::chrono::minutes tzMinutes{0};
    if (!offsetPart.empty() && offsetPart != "Z")
    {
        const bool negative = offsetPart.front() == '-';
        const std::string offsetValue = offsetPart.substr(1);
        if (offsetValue.size() != 5 || offsetValue[2] != ':')
        {
            return std::nullopt;
        }

        int hours = 0;
        int minutes = 0;
        const auto [hoursPtr, hoursEc] =
            std::from_chars(offsetValue.data(), offsetValue.data() + 2, hours);
        const auto [minutesPtr, minutesEc] =
            std::from_chars(offsetValue.data() + 3, offsetValue.data() + 5, minutes);
        if (hoursEc != std::errc{} || hoursPtr != offsetValue.data() + 2 ||
            minutesEc != std::errc{} || minutesPtr != offsetValue.data() + 5)
        {
            return std::nullopt;
        }

        tzHours = std::chrono::hours(hours);
        tzMinutes = std::chrono::minutes(minutes);
        if (negative)
        {
            tzHours = -tzHours;
            tzMinutes = -tzMinutes;
        }
    }

    std::time_t time = timegm(&tm);
    if (time == -1)
    {
        return std::nullopt;
    }

    auto tp = std::chrono::system_clock::from_time_t(time);
    tp += std::chrono::duration_cast<std::chrono::system_clock::duration>(fractional);
    if (tzHours != std::chrono::hours{0} || tzMinutes != std::chrono::minutes{0})
    {
        tp -= tzHours + tzMinutes;
    }

    return tp;
}

} // namespace core
} // namespace qGpx
