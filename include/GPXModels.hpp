// GPXModels.hpp
// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace qGpx
{
namespace core
{

/// One namespace declaration retained with extension XML.
struct NamespaceBinding
{
    std::string prefix;
    std::string namespaceUri;
};

/// One attribute retained in an extension element.
struct ExtensionAttribute
{
    std::string namespaceUri;
    std::string localName;
    std::string prefix;
    std::string value;
};

/// Namespace-aware extension infoset node. Text nodes use Kind::Text; element
/// nodes keep attributes, namespace declarations and children in document order.
struct ExtensionNode
{
    enum class Kind
    {
        Element,
        Text
    };

    Kind kind = Kind::Element;
    std::string namespaceUri;
    std::string localName;
    std::string prefix;
    std::string text;
    std::vector<ExtensionAttribute> attributes;
    /// Namespace declarations introduced by this element.
    std::vector<NamespaceBinding> namespaceDeclarations;
    /// Effective bindings on a retained extension root, for standalone serialization.
    std::vector<NamespaceBinding> inScopeNamespaces;
    std::vector<ExtensionNode> children;
};

/// Represents a single point in a GPX track
struct TrackPoint
{
    double latitude;                 // Degrees
    double longitude;                // Degrees
    std::optional<double> elevation; // Meters above sea level
    /// Timestamp precision is limited to the platform's system_clock duration.
    std::optional<std::chrono::system_clock::time_point> time;
    std::optional<std::uint8_t> heartRateBpm; // Beats per minute
    std::optional<std::uint8_t> cadenceRpm;   // Revolutions per minute
    std::optional<double> temperatureCelsius; // Degrees Celsius
    std::vector<ExtensionNode> extensions;

    TrackPoint(double lat, double lon) : latitude(lat), longitude(lon)
    {
    }

    TrackPoint(double lat, double lon, double ele) : latitude(lat), longitude(lon), elevation(ele)
    {
    }
};

/// Represents a segment within a track (continuous line between points)
struct TrackSegment
{
    std::vector<TrackPoint> points;
    std::vector<ExtensionNode> extensions;

    // Computed properties (cached after analysis)
    std::optional<double> totalDistance; // Meters
    std::optional<double> elevationGain; // Meters
    std::optional<double> elevationLoss; // Meters

    bool isEmpty() const
    {
        return points.empty();
    }
    size_t pointCount() const
    {
        return points.size();
    }
};

/// Represents a complete track (may contain multiple segments)
struct Track
{
    std::string name;
    std::string description;
    std::vector<TrackSegment> segments;
    std::vector<ExtensionNode> extensions;

    // Computed properties (cached after analysis)
    std::optional<double> totalDistance; // Meters
    std::optional<double> elevationGain; // Meters
    std::optional<double> elevationLoss; // Meters
    std::optional<std::chrono::seconds> duration;

    bool isEmpty() const
    {
        return segments.empty();
    }
    size_t segmentCount() const
    {
        return segments.size();
    }

    size_t totalPointCount() const
    {
        size_t count = 0;
        for (const auto& seg : segments)
        {
            count += seg.pointCount();
        }
        return count;
    }
};

/// Complete GPX file data
struct GPXData
{
    std::string creator;
    std::vector<Track> tracks;
    std::vector<ExtensionNode> extensions;

    bool isEmpty() const
    {
        return tracks.empty();
    }
    size_t trackCount() const
    {
        return tracks.size();
    }
};

/// Result type for GPX operations
template <typename T> struct Result
{
    bool success;
    std::optional<T> value;
    std::string errorMessage;

    static Result<T> makeSuccess(T val)
    {
        return Result<T>{true, std::move(val), ""};
    }

    static Result<T> makeFailure(const std::string& error)
    {
        return Result<T>{false, std::nullopt, error};
    }
};

} // namespace core
} // namespace qGpx
