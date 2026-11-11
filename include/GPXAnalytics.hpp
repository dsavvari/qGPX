// GPXAnalytics.hpp
// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#pragma once

#include "GPXModels.hpp"

namespace qGpx
{
namespace core
{

/// GPX analytics and calculations
class GPXAnalytics
{
public:
    /// Calculate distance between two points using Haversine formula
    /// @param p1 First point
    /// @param p2 Second point
    /// @return Distance in meters
    static double calculateDistance(const TrackPoint& p1, const TrackPoint& p2);

    /// Derived horizontal speed in m/s; unavailable for missing/non-increasing timestamps
    /// or non-finite measurements. Points must belong to the same segment.
    static std::optional<double> calculateSpeed(const TrackPoint& p1, const TrackPoint& p2);

    /// Maximum valid adjacent-point speed in m/s, or nullopt when none is available.
    static std::optional<double> calculateMaxSpeed(const TrackSegment& segment);
    static std::optional<double> calculateMaxSpeed(const Track& track);

    /// Calculate total distance for a track segment
    /// @param segment Track segment
    /// @return Total distance in meters
    static double calculateTotalDistance(const TrackSegment& segment);

    /// Calculate total distance for a track (all segments)
    /// @param track Track
    /// @return Total distance in meters
    static double calculateTotalDistance(const Track& track);

    /// Calculate elevation gain and loss for a track segment
    /// @param segment Track segment
    /// @return Pair of (elevation gain, elevation loss) in meters
    static std::pair<double, double> calculateElevationChange(const TrackSegment& segment);

    /// Calculate elevation gain and loss for a track (all segments)
    /// @param track Track
    /// @return Pair of (elevation gain, elevation loss) in meters
    static std::pair<double, double> calculateElevationChange(const Track& track);

    /// Calculate duration for a track segment
    /// @param segment Track segment
    /// @return Duration in seconds, or nullopt if time data is missing
    static std::optional<std::chrono::seconds> calculateDuration(const TrackSegment& segment);

    /// Calculate duration for a track (all segments)
    /// @param track Track
    /// @return Duration in seconds, or nullopt if time data is missing
    static std::optional<std::chrono::seconds> calculateDuration(const Track& track);

    /// Analyze and populate computed properties for a track
    /// This calculates distance, elevation, and duration for all segments and the track
    /// @param track Track to analyze (modified in place)
    static void analyzeTrack(Track& track);

    /// Analyze and populate computed properties for GPX data
    /// This analyzes all tracks in the GPX data
    /// @param gpxData GPX data to analyze (modified in place)
    static void analyzeGPXData(GPXData& gpxData);

private:
    static constexpr double EARTH_RADIUS_METERS = 6371000.0;

    static double toRadians(double degrees);
};

} // namespace core
} // namespace qGpx
