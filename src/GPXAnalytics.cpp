// GPXAnalytics.cpp
// Implementation of GPX analytics functions
//
// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#include "GPXAnalytics.hpp"

#include <algorithm>
#include <cmath>

namespace qGpx
{
namespace core
{

double GPXAnalytics::toRadians(double degrees)
{
    static constexpr double PI = 3.14159265358979323846;
    return degrees * PI / 180.0;
}

double GPXAnalytics::calculateDistance(const TrackPoint& p1, const TrackPoint& p2)
{
    // Haversine formula for great-circle distance
    double lat1 = toRadians(p1.latitude);
    double lat2 = toRadians(p2.latitude);
    double deltaLat = toRadians(p2.latitude - p1.latitude);
    double deltaLon = toRadians(p2.longitude - p1.longitude);

    double a =
        std::sin(deltaLat / 2.0) * std::sin(deltaLat / 2.0) +
        std::cos(lat1) * std::cos(lat2) * std::sin(deltaLon / 2.0) * std::sin(deltaLon / 2.0);

    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));

    return EARTH_RADIUS_METERS * c;
}

std::optional<double> GPXAnalytics::calculateSpeed(const TrackPoint& p1, const TrackPoint& p2)
{
    if (!p1.time || !p2.time || *p2.time <= *p1.time)
    {
        return std::nullopt;
    }
    const double seconds = std::chrono::duration<double>(*p2.time - *p1.time).count();
    const double speed = calculateDistance(p1, p2) / seconds;
    return std::isfinite(speed) ? std::optional<double>(speed) : std::nullopt;
}

std::optional<double> GPXAnalytics::calculateMaxSpeed(const TrackSegment& segment)
{
    std::optional<double> maximum;
    for (size_t i = 1; i < segment.points.size(); ++i)
    {
        const auto speed = calculateSpeed(segment.points[i - 1], segment.points[i]);
        if (speed && (!maximum || *speed > *maximum))
        {
            maximum = speed;
        }
    }
    return maximum;
}

std::optional<double> GPXAnalytics::calculateMaxSpeed(const Track& track)
{
    std::optional<double> maximum;
    for (const auto& segment : track.segments)
    {
        const auto speed = calculateMaxSpeed(segment);
        if (speed && (!maximum || *speed > *maximum))
        {
            maximum = speed;
        }
    }
    return maximum;
}

double GPXAnalytics::calculateTotalDistance(const TrackSegment& segment)
{
    double totalDistance = 0.0;

    for (size_t i = 1; i < segment.points.size(); ++i)
    {
        totalDistance += calculateDistance(segment.points[i - 1], segment.points[i]);
    }

    return totalDistance;
}

double GPXAnalytics::calculateTotalDistance(const Track& track)
{
    double totalDistance = 0.0;

    for (const auto& segment : track.segments)
    {
        totalDistance += calculateTotalDistance(segment);
    }

    return totalDistance;
}

std::pair<double, double> GPXAnalytics::calculateElevationChange(const TrackSegment& segment)
{
    double gain = 0.0;
    double loss = 0.0;

    for (size_t i = 1; i < segment.points.size(); ++i)
    {
        const auto& prev = segment.points[i - 1];
        const auto& curr = segment.points[i];

        if (prev.elevation.has_value() && curr.elevation.has_value())
        {
            double change = curr.elevation.value() - prev.elevation.value();
            if (change > 0.0)
            {
                gain += change;
            }
            else
            {
                loss += -change;
            }
        }
    }

    return {gain, loss};
}

std::pair<double, double> GPXAnalytics::calculateElevationChange(const Track& track)
{
    double totalGain = 0.0;
    double totalLoss = 0.0;

    for (const auto& segment : track.segments)
    {
        auto [gain, loss] = calculateElevationChange(segment);
        totalGain += gain;
        totalLoss += loss;
    }

    return {totalGain, totalLoss};
}

std::optional<std::chrono::seconds> GPXAnalytics::calculateDuration(const TrackSegment& segment)
{
    if (segment.points.empty())
    {
        return std::nullopt;
    }

    // Find first and last points with time data
    std::optional<std::chrono::system_clock::time_point> firstTime;
    std::optional<std::chrono::system_clock::time_point> lastTime;

    for (const auto& pt : segment.points)
    {
        if (pt.time.has_value())
        {
            if (!firstTime.has_value())
            {
                firstTime = pt.time;
            }
            lastTime = pt.time;
        }
    }

    if (!firstTime.has_value() || !lastTime.has_value())
    {
        return std::nullopt;
    }

    auto duration =
        std::chrono::duration_cast<std::chrono::seconds>(lastTime.value() - firstTime.value());

    return duration;
}

std::optional<std::chrono::seconds> GPXAnalytics::calculateDuration(const Track& track)
{
    if (track.segments.empty())
    {
        return std::nullopt;
    }

    // Find first and last time across all segments
    std::optional<std::chrono::system_clock::time_point> firstTime;
    std::optional<std::chrono::system_clock::time_point> lastTime;

    for (const auto& segment : track.segments)
    {
        for (const auto& pt : segment.points)
        {
            if (pt.time.has_value())
            {
                if (!firstTime.has_value())
                {
                    firstTime = pt.time;
                }
                lastTime = pt.time;
            }
        }
    }

    if (!firstTime.has_value() || !lastTime.has_value())
    {
        return std::nullopt;
    }

    auto duration =
        std::chrono::duration_cast<std::chrono::seconds>(lastTime.value() - firstTime.value());

    return duration;
}

void GPXAnalytics::analyzeTrack(Track& track)
{
    // Calculate stats for each segment
    for (auto& segment : track.segments)
    {
        segment.totalDistance = calculateTotalDistance(segment);
        auto [gain, loss] = calculateElevationChange(segment);
        segment.elevationGain = gain;
        segment.elevationLoss = loss;
    }

    // Calculate stats for entire track
    track.totalDistance = calculateTotalDistance(track);
    auto [totalGain, totalLoss] = calculateElevationChange(track);
    track.elevationGain = totalGain;
    track.elevationLoss = totalLoss;
    track.duration = calculateDuration(track);
}

void GPXAnalytics::analyzeGPXData(GPXData& gpxData)
{
    for (auto& track : gpxData.tracks)
    {
        analyzeTrack(track);
    }
}

} // namespace core
} // namespace qGpx
