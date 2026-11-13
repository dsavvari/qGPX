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
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>

namespace qGpx
{
namespace core
{

namespace
{

constexpr char NamespaceSeparator = '\x1f';
constexpr const char* GPXNamespace = "http://www.topografix.com/GPX/1/1";
constexpr const char* GarminTrackPointExtensionV1 =
    "http://www.garmin.com/xmlschemas/TrackPointExtension/v1";
constexpr const char* GarminTrackPointExtensionV2 =
    "http://www.garmin.com/xmlschemas/TrackPointExtension/v2";
constexpr const char* GarminGpxExtensionsV3 = "http://www.garmin.com/xmlschemas/GpxExtensions/v3";
constexpr std::size_t MaxExtensionBytes = 1024 * 1024;
constexpr std::size_t MaxExtensionDepth = 32;

struct QualifiedName
{
    std::string namespaceUri;
    std::string localName;
    std::string prefix;
};

QualifiedName splitName(const char* value)
{
    const std::string name(value);
    const auto separator = name.find(NamespaceSeparator);
    if (separator == std::string::npos)
    {
        return QualifiedName{"", name, ""};
    }

    const auto prefixSeparator = name.find(NamespaceSeparator, separator + 1);
    if (prefixSeparator == std::string::npos)
    {
        return QualifiedName{name.substr(0, separator), name.substr(separator + 1), ""};
    }
    return QualifiedName{name.substr(0, separator),
                         name.substr(separator + 1, prefixSeparator - separator - 1),
                         name.substr(prefixSeparator + 1)};
}

bool isGpxElement(const QualifiedName& name, const char* localName,
                  const std::string& gpxNamespaceUri)
{
    return name.localName == localName && name.namespaceUri == gpxNamespaceUri;
}

} // namespace

struct ParserState;
static void stopWithError(ParserState* state, const std::string& message);

struct ParserState
{
    struct PreviousNamespaceBinding
    {
        std::string prefix;
        std::string namespaceUri;
        bool existed;
    };

    XML_Parser parser = nullptr;
    GPXData gpxData;
    Track currentTrack;
    TrackSegment currentSegment;
    TrackPoint* currentPoint = nullptr;

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
        Description,
        Extensions
    };

    std::vector<Element> elementStack;
    std::string characterData;
    std::string errorMessage;
    std::string gpxNamespaceUri;
    std::map<std::string, std::string> namespaceBindings;
    std::vector<std::vector<PreviousNamespaceBinding>> namespaceScopeStack;
    std::vector<NamespaceBinding> pendingNamespaceDeclarations;
    std::vector<NamespaceBinding> namespaceDeclarationsForCurrentElement;
    bool capturingExtensions = false;
    std::size_t extensionDepth = 0;
    std::size_t extensionBytes = 0;
    std::size_t gpxExtensionBytes = 0;
    std::size_t trackExtensionBytes = 0;
    std::size_t segmentExtensionBytes = 0;
    std::size_t pointExtensionBytes = 0;
    std::size_t* extensionBudget = nullptr;
    std::vector<ExtensionNode>* extensionTarget = nullptr;
    std::vector<ExtensionNode*> extensionNodeStack;

    void beginElement()
    {
        namespaceDeclarationsForCurrentElement = pendingNamespaceDeclarations;
        std::vector<PreviousNamespaceBinding> previousBindings;
        previousBindings.reserve(pendingNamespaceDeclarations.size());
        for (const auto& binding : pendingNamespaceDeclarations)
        {
            const auto existing = namespaceBindings.find(binding.prefix);
            previousBindings.push_back(PreviousNamespaceBinding{
                binding.prefix, existing == namespaceBindings.end() ? "" : existing->second,
                existing != namespaceBindings.end()});
            namespaceBindings[binding.prefix] = binding.namespaceUri;
        }
        namespaceScopeStack.push_back(std::move(previousBindings));
        pendingNamespaceDeclarations.clear();
    }

    void endElementScope()
    {
        if (!namespaceScopeStack.empty())
        {
            for (auto iter = namespaceScopeStack.back().rbegin();
                 iter != namespaceScopeStack.back().rend(); ++iter)
            {
                if (iter->existed)
                {
                    namespaceBindings[iter->prefix] = iter->namespaceUri;
                }
                else
                {
                    namespaceBindings.erase(iter->prefix);
                }
            }
            namespaceScopeStack.pop_back();
        }
    }

    std::vector<NamespaceBinding> activeNamespaceBindings() const
    {
        std::vector<NamespaceBinding> bindings;
        bindings.reserve(namespaceBindings.size());
        for (const auto& binding : namespaceBindings)
        {
            bindings.push_back(NamespaceBinding{binding.first, binding.second});
        }
        return bindings;
    }

    Element currentElement() const
    {
        return elementStack.empty() ? Element::None : elementStack.back();
    }

    void appendExtensionNode(const QualifiedName& name, const char** atts)
    {
        if (extensionDepth >= MaxExtensionDepth)
        {
            stopWithError(this, "GPX extension nesting exceeds the maximum depth of 32");
            return;
        }

        ExtensionNode node;
        node.namespaceUri = name.namespaceUri;
        node.localName = name.localName;
        node.prefix = name.prefix;
        if (extensionNodeStack.empty())
        {
            node.inScopeNamespaces = activeNamespaceBindings();
        }
        node.namespaceDeclarations = namespaceDeclarationsForCurrentElement;

        std::size_t nodeBytes =
            name.namespaceUri.size() + name.localName.size() + name.prefix.size();
        for (const auto& binding : node.inScopeNamespaces)
        {
            nodeBytes += binding.prefix.size() + binding.namespaceUri.size();
        }
        for (const auto& binding : node.namespaceDeclarations)
        {
            nodeBytes += binding.prefix.size() + binding.namespaceUri.size();
        }
        for (int i = 0; atts && atts[i]; i += 2)
        {
            const auto attributeName = splitName(atts[i]);
            node.attributes.push_back(ExtensionAttribute{attributeName.namespaceUri,
                                                         attributeName.localName,
                                                         attributeName.prefix, atts[i + 1]});
            nodeBytes += attributeName.namespaceUri.size() + attributeName.localName.size() +
                         attributeName.prefix.size() + std::strlen(atts[i + 1]);
        }
        if (nodeBytes > MaxExtensionBytes - extensionBytes)
        {
            stopWithError(this, "GPX extension data exceeds the 1 MiB limit");
            return;
        }
        extensionBytes += nodeBytes;

        if (extensionNodeStack.empty())
        {
            extensionTarget->push_back(std::move(node));
            extensionNodeStack.push_back(&extensionTarget->back());
        }
        else
        {
            auto* parent = extensionNodeStack.back();
            parent->children.push_back(std::move(node));
            extensionNodeStack.push_back(&parent->children.back());
        }
        ++extensionDepth;
    }

    void appendExtensionText(const char* text, int length)
    {
        if (length <= 0)
        {
            return;
        }
        const auto textLength = static_cast<std::size_t>(length);
        if (textLength > MaxExtensionBytes - extensionBytes)
        {
            stopWithError(this, "GPX extension data exceeds the 1 MiB limit");
            return;
        }
        extensionBytes += textLength;
        auto* children =
            extensionNodeStack.empty() ? extensionTarget : &extensionNodeStack.back()->children;
        if (!children->empty() && children->back().kind == ExtensionNode::Kind::Text)
        {
            children->back().text.append(text, textLength);
            return;
        }
        ExtensionNode node;
        node.kind = ExtensionNode::Kind::Text;
        node.text.assign(text, textLength);
        children->push_back(std::move(node));
    }
};

static bool parseFiniteDouble(const std::string& value, double& result)
{
    // GPX's xsd:decimal permits neither hexadecimal nor scientific notation.
    // Stream implementations can differ in whether they accept hexadecimal or scientific notation.
    // Reject them explicitly by only allowing digits, sign, decimal point, and whitespace.
    if (value.find_first_not_of("0123456789+-. \t\n\r") != std::string::npos)
    {
        return false;
    }

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

static std::string trimXmlWhitespace(const std::string& value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return "";
    }
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

static bool parseGarminUnsignedByte(const std::string& text, unsigned int minimum,
                                    unsigned int maximum, uint8_t& value)
{
    auto normalized = trimXmlWhitespace(text);
    if (normalized.empty())
    {
        return false;
    }
    if (normalized.front() == '+')
    {
        normalized.erase(normalized.begin());
        if (normalized.empty())
        {
            return false;
        }
    }
    unsigned int parsed = 0;
    const auto [end, error] =
        std::from_chars(normalized.data(), normalized.data() + normalized.size(), parsed);
    if (error != std::errc{} || end != normalized.data() + normalized.size() || parsed < minimum ||
        parsed > maximum)
    {
        return false;
    }
    value = static_cast<uint8_t>(parsed);
    return true;
}

static bool parseGarminTemperature(const std::string& text, double& value)
{
    std::istringstream input(trimXmlWhitespace(text));
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
    value = parsed;
    return true;
}

static bool parseGarminMeasurement(const ExtensionNode& node, std::string& text)
{
    for (const auto& child : node.children)
    {
        if (child.kind != ExtensionNode::Kind::Text)
        {
            return false;
        }
        text += child.text;
    }
    return true;
}

static bool applyGarminSensors(ParserState* state, const std::vector<ExtensionNode>& extensions)
{
    if (!state->currentPoint)
    {
        return true;
    }

    for (const auto& extension : extensions)
    {
        if (extension.kind != ExtensionNode::Kind::Element ||
            extension.localName != "TrackPointExtension")
        {
            continue;
        }
        const bool versionOneOrTwo = extension.namespaceUri == GarminTrackPointExtensionV1 ||
                                     extension.namespaceUri == GarminTrackPointExtensionV2;
        const bool versionThree = extension.namespaceUri == GarminGpxExtensionsV3;
        if (!versionOneOrTwo && !versionThree)
        {
            continue;
        }

        for (const auto& child : extension.children)
        {
            if (child.kind != ExtensionNode::Kind::Element ||
                child.namespaceUri != extension.namespaceUri)
            {
                continue;
            }

            const bool isHeartRate = versionOneOrTwo && child.localName == "hr";
            const bool isCadence = versionOneOrTwo && child.localName == "cad";
            const bool isV1OrV2Temperature = versionOneOrTwo && child.localName == "atemp";
            const bool isV3Temperature = versionThree && child.localName == "Temperature";
            if (!isHeartRate && !isCadence && !isV1OrV2Temperature && !isV3Temperature)
            {
                continue;
            }

            std::string text;
            if (!parseGarminMeasurement(child, text))
            {
                stopWithError(state, "Malformed Garmin sensor value");
                return false;
            }
            if (isHeartRate)
            {
                uint8_t value = 0;
                if (state->currentPoint->heartRateBpm ||
                    !parseGarminUnsignedByte(text, 1, 255, value))
                {
                    stopWithError(state, state->currentPoint->heartRateBpm
                                             ? "Duplicate Garmin heart rate value"
                                             : "Invalid Garmin heart rate value");
                    return false;
                }
                state->currentPoint->heartRateBpm = value;
            }
            else if (isCadence)
            {
                uint8_t value = 0;
                if (state->currentPoint->cadenceRpm ||
                    !parseGarminUnsignedByte(text, 0, 254, value))
                {
                    stopWithError(state, state->currentPoint->cadenceRpm
                                             ? "Duplicate Garmin cadence value"
                                             : "Invalid Garmin cadence value");
                    return false;
                }
                state->currentPoint->cadenceRpm = value;
            }
            else
            {
                double value = 0.0;
                if (state->currentPoint->temperatureCelsius || !parseGarminTemperature(text, value))
                {
                    stopWithError(state, state->currentPoint->temperatureCelsius
                                             ? "Duplicate Garmin temperature value"
                                             : "Invalid Garmin temperature value");
                    return false;
                }
                state->currentPoint->temperatureCelsius = value;
            }
        }
    }
    return true;
}

static void XMLCALL startNamespaceDeclHandler(void* userData, const char* prefix, const char* uri)
{
    auto* state = static_cast<ParserState*>(userData);
    state->pendingNamespaceDeclarations.push_back(
        NamespaceBinding{prefix ? prefix : "", uri ? uri : ""});
}

static std::vector<ExtensionNode>* extensionTarget(ParserState* state)
{
    switch (state->currentElement())
    {
    case ParserState::Element::GPX:
        return &state->gpxData.extensions;
    case ParserState::Element::Track:
        return &state->currentTrack.extensions;
    case ParserState::Element::TrackSegment:
        return &state->currentSegment.extensions;
    case ParserState::Element::TrackPoint:
        return state->currentPoint ? &state->currentPoint->extensions : nullptr;
    default:
        return nullptr;
    }
}

static std::size_t* extensionBudget(ParserState* state)
{
    switch (state->currentElement())
    {
    case ParserState::Element::GPX:
        return &state->gpxExtensionBytes;
    case ParserState::Element::Track:
        return &state->trackExtensionBytes;
    case ParserState::Element::TrackSegment:
        return &state->segmentExtensionBytes;
    case ParserState::Element::TrackPoint:
        return &state->pointExtensionBytes;
    default:
        return nullptr;
    }
}

static void XMLCALL startElementHandler(void* userData, const char* rawName, const char** atts)
{
    auto* state = static_cast<ParserState*>(userData);
    state->beginElement();
    const auto name = splitName(rawName);

    if (state->capturingExtensions)
    {
        state->appendExtensionNode(name, atts);
        return;
    }

    state->characterData.clear();
    const auto parent = state->currentElement();
    if (isGpxElement(name, "extensions", state->gpxNamespaceUri))
    {
        auto* target = extensionTarget(state);
        auto* budget = extensionBudget(state);
        if (target && budget)
        {
            state->capturingExtensions = true;
            state->extensionDepth = 0;
            state->extensionBytes = *budget;
            state->extensionBudget = budget;
            state->extensionTarget = target;
            state->extensionNodeStack.clear();
            state->elementStack.push_back(ParserState::Element::Extensions);
            return;
        }
    }
    if (name.localName == "gpx" && (name.namespaceUri.empty() || name.namespaceUri == GPXNamespace))
    {
        if (parent != ParserState::Element::None)
        {
            stopWithError(state, "Malformed GPX nesting");
            return;
        }
        state->gpxNamespaceUri = name.namespaceUri;
        state->gpxExtensionBytes = 0;
        state->elementStack.push_back(ParserState::Element::GPX);
        for (int i = 0; atts[i]; i += 2)
        {
            const auto attributeName = splitName(atts[i]);
            if (attributeName.namespaceUri.empty() && attributeName.localName == "creator")
            {
                state->gpxData.creator = atts[i + 1];
            }
        }
    }
    else if (isGpxElement(name, "trk", state->gpxNamespaceUri))
    {
        if (parent != ParserState::Element::GPX)
        {
            stopWithError(state, "Malformed GPX nesting");
            return;
        }
        state->elementStack.push_back(ParserState::Element::Track);
        state->currentTrack = Track();
        state->trackExtensionBytes = 0;
    }
    else if (isGpxElement(name, "trkseg", state->gpxNamespaceUri))
    {
        if (parent != ParserState::Element::Track)
        {
            stopWithError(state, "Malformed GPX nesting");
            return;
        }
        state->currentPoint = nullptr;
        state->elementStack.push_back(ParserState::Element::TrackSegment);
        state->currentSegment = TrackSegment();
        state->segmentExtensionBytes = 0;
    }
    else if (isGpxElement(name, "trkpt", state->gpxNamespaceUri))
    {
        if (parent != ParserState::Element::TrackSegment)
        {
            stopWithError(state, "Malformed GPX nesting");
            return;
        }
        state->elementStack.push_back(ParserState::Element::TrackPoint);

        // Parse lat/lon attributes
        const char* latitude = nullptr;
        const char* longitude = nullptr;

        for (int i = 0; atts[i]; i += 2)
        {
            const auto attributeName = splitName(atts[i]);
            if (attributeName.namespaceUri.empty() && attributeName.localName == "lat")
            {
                latitude = atts[i + 1];
            }
            else if (attributeName.namespaceUri.empty() && attributeName.localName == "lon")
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
        state->pointExtensionBytes = 0;
    }
    else if (isGpxElement(name, "ele", state->gpxNamespaceUri) &&
             parent == ParserState::Element::TrackPoint)
    {
        state->elementStack.push_back(ParserState::Element::Elevation);
    }
    else if (isGpxElement(name, "time", state->gpxNamespaceUri) &&
             parent == ParserState::Element::TrackPoint)
    {
        state->elementStack.push_back(ParserState::Element::Time);
    }
    else if (isGpxElement(name, "name", state->gpxNamespaceUri) &&
             parent == ParserState::Element::Track)
    {
        state->elementStack.push_back(ParserState::Element::Name);
    }
    else if (isGpxElement(name, "type", state->gpxNamespaceUri) &&
             parent == ParserState::Element::Track)
    {
        state->elementStack.push_back(ParserState::Element::Type);
    }
    else if (isGpxElement(name, "desc", state->gpxNamespaceUri) &&
             parent == ParserState::Element::Track)
    {
        state->elementStack.push_back(ParserState::Element::Description);
    }
    else
    {
        state->elementStack.push_back(ParserState::Element::None);
    }
}

static void XMLCALL endElementHandler(void* userData, const char* rawName)
{
    auto* state = static_cast<ParserState*>(userData);
    const auto name = splitName(rawName);

    if (state->capturingExtensions)
    {
        if (state->extensionDepth > 0)
        {
            state->extensionNodeStack.pop_back();
            --state->extensionDepth;
            state->endElementScope();
            return;
        }

        if (isGpxElement(name, "extensions", state->gpxNamespaceUri))
        {
            const auto parent = state->elementStack.size() > 1
                                    ? state->elementStack[state->elementStack.size() - 2]
                                    : ParserState::Element::None;
            if (parent == ParserState::Element::TrackPoint &&
                !applyGarminSensors(state, *state->extensionTarget))
            {
                return;
            }
            state->capturingExtensions = false;
            *state->extensionBudget = state->extensionBytes;
            state->extensionBudget = nullptr;
            state->extensionTarget = nullptr;
            state->extensionNodeStack.clear();
            if (!state->elementStack.empty())
            {
                state->elementStack.pop_back();
            }
            state->endElementScope();
            state->characterData.clear();
            return;
        }

        stopWithError(state, "Malformed GPX extension nesting");
        return;
    }

    ParserState::Element elem = state->currentElement();

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

    if (!state->elementStack.empty())
    {
        state->elementStack.pop_back();
    }
    state->endElementScope();
    state->characterData.clear();
}

static void XMLCALL characterDataHandler(void* userData, const char* s, int len)
{
    auto* state = static_cast<ParserState*>(userData);

    if (state->capturingExtensions)
    {
        state->appendExtensionText(s, len);
        return;
    }
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
        return Result<GPXData>::makeFailure("XML content is empty");
    }

    // Create expat parser
    XML_Parser parser = XML_ParserCreateNS(nullptr, NamespaceSeparator);
    if (!parser)
    {
        return Result<GPXData>::makeFailure("Failed to create XML parser");
    }

    // Set up parser state
    ParserState state;
    state.parser = parser;
    XML_SetUserData(parser, &state);
    XML_SetReturnNSTriplet(parser, XML_TRUE);
    XML_SetNamespaceDeclHandler(parser, startNamespaceDeclHandler, nullptr);
    XML_SetElementHandler(parser, startElementHandler, endElementHandler);
    XML_SetCharacterDataHandler(parser, characterDataHandler);

    // Parse the XML content, Expat doesn't throw exceptions but returns error codes
    int parseResult = XML_Parse(parser, xmlContent.c_str(), static_cast<int>(xmlContent.size()), 1);

    if (parseResult == XML_STATUS_ERROR)
    {
        if (!state.errorMessage.empty())
        {
            XML_ParserFree(parser);
            return Result<GPXData>::makeFailure(state.errorMessage);
        }

        std::string errorMsg = "XML parsing error at line ";
        errorMsg += std::to_string(XML_GetCurrentLineNumber(parser));
        errorMsg += ": ";
        errorMsg += XML_ErrorString(XML_GetErrorCode(parser));

        XML_ParserFree(parser);
        return Result<GPXData>::makeFailure(errorMsg);
    }

    XML_ParserFree(parser);

    // Validate we got some data
    if (state.gpxData.tracks.empty())
    {
        return Result<GPXData>::makeFailure("No tracks found in GPX file");
    }

    return Result<GPXData>::makeSuccess(std::move(state.gpxData));
}

Result<GPXData> GPXParser::parseFile(const std::string& filePath)
{
    if (filePath.empty() || filePath.find('\0') != std::string::npos)
    {
        return Result<GPXData>::makeFailure("File path is empty or contains a null character");
    }
    std::error_code error;
    const bool isDirectory = std::filesystem::is_directory(filePath, error);
    if (error)
    {
        return Result<GPXData>::makeFailure("Failed to open file: " + filePath);
    }
    if (isDirectory)
    {
        return Result<GPXData>::makeFailure("Failed to read file: " + filePath);
    }
    // Read file content
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
    {
        return Result<GPXData>::makeFailure("Failed to open file: " + filePath);
    }

    std::string xmlContent;
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer)))
    {
        xmlContent.append(buffer, sizeof(buffer));
    }
    xmlContent.append(buffer, static_cast<std::size_t>(file.gcount()));
    if (file.bad() || !file.eof())
    {
        return Result<GPXData>::makeFailure("Failed to read file: " + filePath);
    }

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
