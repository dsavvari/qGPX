//  GPXParser.cpp
// Implementation of GPXParser
//
// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#include "GPXParser.hpp"

#include <expat.h>

#include <cmath>
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

    std::vector<Element> elementStack;
    std::string characterData;
    std::string errorMessage;

    void pushElement(Element elem)
    {
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
        state->pushElement(ParserState::Element::TrackSegment);
        state->currentSegment = TrackSegment(); // Reset current segment
    }
    else if (elementName == "trkpt")
    {
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
        state->pushElement(ParserState::Element::Elevation);
    }
    else if (elementName == "time")
    {
        state->pushElement(ParserState::Element::Time);
    }
    else if (elementName == "name")
    {
        state->pushElement(ParserState::Element::Name);
    }
    else if (elementName == "type")
    {
        state->pushElement(ParserState::Element::Type);
    }
    else if (elementName == "desc")
    {
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
        if (state->currentPoint && !state->characterData.empty())
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
            state->currentPoint->time = GPXParser::parseTime(state->characterData);
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
        break;

    case ParserState::Element::Track:
        // End of track - add to GPX data
        if (!state->currentTrack.segments.empty())
        {
            state->gpxData.tracks.push_back(std::move(state->currentTrack));
        }
        state->currentTrack = Track();
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
    // Parse ISO 8601 format: 2026-03-03T09:51:09.000Z
    // Simple implementation for common GPX format

    if (timeStr.empty())
    {
        return std::nullopt;
    }

    std::tm tm = {};
    std::istringstream ss(timeStr);

    // Parse: YYYY-MM-DDTHH:MM:SS
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    if (ss.fail())
    {
        return std::nullopt;
    }

    // Convert to time_point (UTC)
    std::time_t time = timegm(&tm);
    if (time == -1)
    {
        return std::nullopt;
    }

    return std::chrono::system_clock::from_time_t(time);
}

} // namespace core
} // namespace qGpx
