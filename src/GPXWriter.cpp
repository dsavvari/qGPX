// SPDX-License-Identifier: MIT
#include "GPXWriter.hpp"

#include <expat.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <ctime>
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
constexpr const char* XmlNamespace = "http://www.w3.org/XML/1998/namespace";

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

std::string padDecimal(std::string text, std::size_t minimumPlaces)
{
    auto dot = text.find('.');
    if (dot == std::string::npos)
    {
        dot = text.size();
        text += '.';
    }
    const auto places = text.size() - dot - 1;
    if (places < minimumPlaces)
    {
        text.append(minimumPlaces - places, '0');
    }
    return text;
}

Result<std::string> decimal(double value, std::size_t minimumPlaces)
{
    char buffer[64];
    const auto [end, error] =
        std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general);
    if (error != std::errc{})
    {
        return Result<std::string>::makeFailure("Failed to format decimal value");
    }
    std::string text(buffer, end);
    const auto exponentPosition = text.find_first_of("eE");
    if (exponentPosition == std::string::npos)
    {
        return Result<std::string>::makeSuccess(padDecimal(std::move(text), minimumPlaces));
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
    return Result<std::string>::makeSuccess(
        padDecimal((negative ? "-" : "") + digits, minimumPlaces));
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

bool addNamespaceBinding(std::map<std::string, std::string>& bindings,
                         std::vector<NamespaceBinding>& declarations, const std::string& prefix,
                         const std::string& namespaceUri)
{
    if (prefix == "xml")
    {
        return namespaceUri == XmlNamespace;
    }
    const auto existing = bindings.find(prefix);
    if (existing != bindings.end() && existing->second == namespaceUri)
    {
        return true;
    }
    const auto declared = std::find_if(declarations.begin(), declarations.end(),
                                       [&prefix](const NamespaceBinding& binding)
                                       { return binding.prefix == prefix; });
    if (declared != declarations.end())
    {
        return false;
    }
    declarations.push_back(NamespaceBinding{prefix, namespaceUri});
    bindings[prefix] = namespaceUri;
    return true;
}

Result<bool> writeExtensionNode(const ExtensionNode& node, std::ostringstream& output,
                                const std::map<std::string, std::string>& inheritedBindings,
                                bool isRoot)
{
    if (node.kind == ExtensionNode::Kind::Text)
    {
        output << escapeXml(node.text);
        return Result<bool>::makeSuccess(true);
    }

    auto bindings = inheritedBindings;
    auto declarations = isRoot ? node.inScopeNamespaces : node.namespaceDeclarations;
    std::map<std::string, std::string> declaredBindings;
    for (const auto& binding : declarations)
    {
        const auto existing = declaredBindings.find(binding.prefix);
        if (existing != declaredBindings.end() && existing->second != binding.namespaceUri)
        {
            return Result<bool>::makeFailure("Conflicting namespace declarations in GPX extension");
        }
        declaredBindings[binding.prefix] = binding.namespaceUri;
        bindings[binding.prefix] = binding.namespaceUri;
    }

    if (!addNamespaceBinding(bindings, declarations, node.prefix, node.namespaceUri))
    {
        return Result<bool>::makeFailure("Invalid namespace binding in GPX extension element");
    }
    for (const auto& attribute : node.attributes)
    {
        if (!attribute.namespaceUri.empty() &&
            (attribute.prefix.empty() ||
             !addNamespaceBinding(bindings, declarations, attribute.prefix,
                                  attribute.namespaceUri)))
        {
            return Result<bool>::makeFailure(
                "Invalid namespace binding in GPX extension attribute");
        }
    }

    const auto qualifiedName =
        node.prefix.empty() ? node.localName : node.prefix + ":" + node.localName;
    output << '<' << qualifiedName;
    for (const auto& binding : declarations)
    {
        output << " xmlns";
        if (!binding.prefix.empty())
        {
            output << ':' << binding.prefix;
        }
        output << "=\"" << escapeXml(binding.namespaceUri) << '"';
    }
    for (const auto& attribute : node.attributes)
    {
        const auto attributeName = attribute.prefix.empty()
                                       ? attribute.localName
                                       : attribute.prefix + ":" + attribute.localName;
        output << ' ' << attributeName << "=\"" << escapeXml(attribute.value) << '"';
    }
    if (node.children.empty())
    {
        output << "/>";
        return Result<bool>::makeSuccess(true);
    }

    output << '>';
    for (const auto& child : node.children)
    {
        const auto result = writeExtensionNode(child, output, bindings, false);
        if (!result.success)
        {
            return result;
        }
    }
    output << "</" << qualifiedName << '>';
    return Result<bool>::makeSuccess(true);
}

bool isGarminTrackPointExtension(const ExtensionNode& node)
{
    return node.kind == ExtensionNode::Kind::Element && node.localName == "TrackPointExtension" &&
           (node.namespaceUri == GarminTrackPointExtensionV1 ||
            node.namespaceUri == GarminTrackPointExtensionV2 ||
            node.namespaceUri == GarminGpxExtensionsV3);
}

void setExtensionText(ExtensionNode& element, const std::string& text)
{
    element.children.clear();
    ExtensionNode textNode;
    textNode.kind = ExtensionNode::Kind::Text;
    textNode.text = text;
    element.children.push_back(std::move(textNode));
}

std::size_t measurementInsertionIndex(const ExtensionNode& root, const std::string& localName)
{
    auto rank = [](const std::string& name)
    {
        if (name == "atemp")
            return 0;
        if (name == "wtemp")
            return 1;
        if (name == "depth")
            return 2;
        if (name == "hr")
            return 3;
        if (name == "cad")
            return 4;
        if (name == "speed")
            return 5;
        if (name == "course")
            return 6;
        if (name == "bearing")
            return 7;
        if (name == "Extensions")
            return 8;
        return -1;
    };
    const auto requestedRank = rank(localName);
    for (std::size_t i = 0; i < root.children.size(); ++i)
    {
        const auto& child = root.children[i];
        if (child.kind == ExtensionNode::Kind::Element && child.namespaceUri == root.namespaceUri &&
            rank(child.localName) > requestedRank)
        {
            return i;
        }
    }
    return root.children.size();
}

void syncGarminMeasurement(std::vector<ExtensionNode>& extensions, const std::string& v12Name,
                           const std::string& v3Name, const std::optional<std::string>& value)
{
    bool found = false;
    ExtensionNode* insertionRoot = nullptr;
    for (auto& root : extensions)
    {
        if (!isGarminTrackPointExtension(root))
        {
            continue;
        }
        const bool v12 = root.namespaceUri == GarminTrackPointExtensionV1 ||
                         root.namespaceUri == GarminTrackPointExtensionV2;
        const auto& localName = v12 ? v12Name : v3Name;
        if (localName.empty())
        {
            continue;
        }
        if (!insertionRoot)
        {
            insertionRoot = &root;
        }
        for (auto iter = root.children.begin(); iter != root.children.end();)
        {
            if (iter->kind != ExtensionNode::Kind::Element ||
                iter->namespaceUri != root.namespaceUri || iter->localName != localName)
            {
                ++iter;
                continue;
            }
            if (!value || found)
            {
                iter = root.children.erase(iter);
                continue;
            }
            setExtensionText(*iter, *value);
            found = true;
            ++iter;
        }
    }

    if (!value || found)
    {
        return;
    }
    if (!insertionRoot)
    {
        ExtensionNode root;
        root.localName = "TrackPointExtension";
        root.namespaceUri = GarminTrackPointExtensionV2;
        root.prefix = "gpxtpx";
        root.inScopeNamespaces.push_back(NamespaceBinding{"gpxtpx", GarminTrackPointExtensionV2});
        extensions.push_back(std::move(root));
        insertionRoot = &extensions.back();
    }

    const bool v12 = insertionRoot->namespaceUri == GarminTrackPointExtensionV1 ||
                     insertionRoot->namespaceUri == GarminTrackPointExtensionV2;
    const auto& localName = v12 ? v12Name : v3Name;
    if (localName.empty())
    {
        return;
    }
    ExtensionNode element;
    element.localName = localName;
    element.namespaceUri = insertionRoot->namespaceUri;
    element.prefix = insertionRoot->prefix;
    setExtensionText(element, *value);
    const auto insertionIndex = measurementInsertionIndex(*insertionRoot, localName);
    insertionRoot->children.insert(insertionRoot->children.begin() + insertionIndex,
                                   std::move(element));
}

Result<std::vector<ExtensionNode>> serializedPointExtensions(const TrackPoint& point)
{
    if ((point.heartRateBpm && *point.heartRateBpm == 0) ||
        (point.cadenceRpm && *point.cadenceRpm > 254))
    {
        return Result<std::vector<ExtensionNode>>::makeFailure(
            "Garmin heart rate must be in [1, 255] bpm and cadence in [0, 254] rpm");
    }
    std::vector<ExtensionNode> extensions = point.extensions;
    std::optional<std::string> heartRate;
    if (point.heartRateBpm)
    {
        heartRate = std::to_string(*point.heartRateBpm);
    }
    std::optional<std::string> cadence;
    if (point.cadenceRpm)
    {
        cadence = std::to_string(*point.cadenceRpm);
    }
    std::optional<std::string> temperature;
    if (point.temperatureCelsius)
    {
        if (!std::isfinite(*point.temperatureCelsius))
        {
            return Result<std::vector<ExtensionNode>>::makeFailure(
                "Garmin temperature must be finite");
        }
        const auto formatted = decimal(*point.temperatureCelsius, 0);
        if (!formatted.success)
        {
            return Result<std::vector<ExtensionNode>>::makeFailure(formatted.errorMessage);
        }
        temperature = *formatted.value;
    }
    syncGarminMeasurement(extensions, "hr", "", heartRate);
    syncGarminMeasurement(extensions, "cad", "", cadence);
    syncGarminMeasurement(extensions, "atemp", "Temperature", temperature);
    return Result<std::vector<ExtensionNode>>::makeSuccess(std::move(extensions));
}

Result<bool> writeExtensions(const std::vector<ExtensionNode>& extensions,
                             std::ostringstream& output)
{
    if (extensions.empty())
    {
        return Result<bool>::makeSuccess(true);
    }
    output << "<extensions>";
    const std::map<std::string, std::string> initialBindings{{"", GPXNamespace}};
    for (const auto& extension : extensions)
    {
        const auto result = writeExtensionNode(extension, output, initialBindings, true);
        if (!result.success)
        {
            return result;
        }
    }
    output << "</extensions>";
    return Result<bool>::makeSuccess(true);
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
        const auto trackExtensionsResult = writeExtensions(track.extensions, output);
        if (!trackExtensionsResult.success)
        {
            return Result<std::string>::makeFailure(trackExtensionsResult.errorMessage);
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
                const auto pointExtensions = serializedPointExtensions(point);
                if (!pointExtensions.success)
                {
                    return Result<std::string>::makeFailure(pointExtensions.errorMessage);
                }
                const auto latitude = decimal(point.latitude, 6);
                if (!latitude.success)
                {
                    return Result<std::string>::makeFailure(latitude.errorMessage);
                }
                const auto longitude = decimal(point.longitude, 6);
                if (!longitude.success)
                {
                    return Result<std::string>::makeFailure(longitude.errorMessage);
                }
                output << "      <trkpt lat=\"" << *latitude.value << "\" lon=\""
                       << *longitude.value << "\">";
                if (point.elevation)
                {
                    const auto elevation = decimal(*point.elevation, 2);
                    if (!elevation.success)
                    {
                        return Result<std::string>::makeFailure(elevation.errorMessage);
                    }
                    output << "<ele>" << *elevation.value << "</ele>";
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
                const auto extensionsResult = writeExtensions(*pointExtensions.value, output);
                if (!extensionsResult.success)
                {
                    return Result<std::string>::makeFailure(extensionsResult.errorMessage);
                }
                output << "</trkpt>\n";
            }
            const auto extensionsResult = writeExtensions(segment.extensions, output);
            if (!extensionsResult.success)
            {
                return Result<std::string>::makeFailure(extensionsResult.errorMessage);
            }
            output << "    </trkseg>\n";
        }
        output << "  </trk>\n";
    }
    const auto extensionsResult = writeExtensions(data.extensions, output);
    if (!extensionsResult.success)
    {
        return Result<std::string>::makeFailure(extensionsResult.errorMessage);
    }
    output << "</gpx>\n";
    std::string xml = output.str();

    // Expat verifies UTF-8 and XML 1.0 characters, including user-supplied text.
    XML_Parser parser = XML_ParserCreateNS("UTF-8", NamespaceSeparator);
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
