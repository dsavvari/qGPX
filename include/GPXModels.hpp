// GPXModels.hpp
// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace qGpx
{
namespace core
{

/// Represents a single point in a GPX track
struct TrackPoint
{
    double latitude;                 // Degrees
    double longitude;                // Degrees
    std::optional<double> elevation; // Meters above sea level
    std::optional<std::chrono::system_clock::time_point> time;

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

    bool isEmpty() const
    {
        return tracks.empty();
    }
    size_t trackCount() const
    {
        return tracks.size();
    }
};

/// Result type for parsing operations
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
