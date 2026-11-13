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
    /// Options for movement statistics analysis
    struct MovementOptions
    {
        double speedThresholdMps; // Minimum speed to classify as "moving" (m/s)
        double maxGapSeconds;     // Maximum gap between points before gap is reported (seconds)
        double maxGapSpeedMps;    // Maximum plausible speed; gaps above this threshold are skipped

        MovementOptions() : speedThresholdMps(0.5), maxGapSeconds(300.0), maxGapSpeedMps(50.0)
        {
        }

        explicit MovementOptions(double threshold, double gapSeconds = 300.0,
                                 double gapSpeed = 50.0)
            : speedThresholdMps(threshold), maxGapSeconds(gapSeconds), maxGapSpeedMps(gapSpeed)
        {
        }
    };

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

    /// Calculate movement statistics for a track segment.
    /// Classifies intervals as moving/stopped based on speed threshold,
    /// reports missing/invalid data, and aggregates valid adjacent-point intervals.
    /// @param segment Track segment
    /// @param options Movement analysis options
    /// @return MovementStatistics or nullopt if no valid intervals exist
    static std::optional<MovementStatistics>
    calculateMovementStats(const TrackSegment& segment,
                           const MovementOptions& options = MovementOptions());

    /// Calculate movement statistics for a track (all segments aggregated).
    /// Never connects separate segments; aggregates only within-segment intervals.
    /// @param track Track
    /// @param options Movement analysis options
    /// @return MovementStatistics or nullopt if no valid intervals exist
    static std::optional<MovementStatistics>
    calculateMovementStats(const Track& track, const MovementOptions& options = MovementOptions());

    /// Analyse and populate computed properties for a track
    /// This calculates distance, elevation, duration, and movement statistics for all segments and
    /// the track
    /// @param track Track to analyse (modified in place)
    /// @param options Movement analysis options
    static void analyseTrack(Track& track, const MovementOptions& options = MovementOptions());

    /// Analyse and populate computed properties for GPX data
    /// This analyses all tracks in the GPX data
    /// @param gpxData GPX data to analyse (modified in place)
    /// @param options Movement analysis options
    static void analyseGPXData(GPXData& gpxData,
                               const MovementOptions& options = MovementOptions());

private:
    static constexpr double EarthRadiusMeters = 6371000.0;

    static double toRadians(double degrees);
};

} // namespace core
} // namespace qGpx
