// SPDX-License-Identifier: MIT
#include "GPXAnalytics.hpp"
#include "GPXCore.h"

#include <gtest/gtest.h>

#include <limits>

using namespace qGpx::core;

namespace
{

TrackPoint point(double latitude, int milliseconds)
{
    TrackPoint result(latitude, 0);
    result.time = std::chrono::system_clock::time_point(std::chrono::milliseconds(milliseconds));
    return result;
}

TEST(GPXSpeed, UsesFractionalSecondsAndHorizontalDistance)
{
    const auto a = point(0, 0);
    const auto b = point(0.001, 500);
    const auto speed = GPXAnalytics::calculateSpeed(a, b);
    ASSERT_TRUE(speed);
    EXPECT_NEAR(*speed, 222.389853, 0.00001);
    EXPECT_EQ(GPXAnalytics::calculateSpeed(a, point(0, 500)), 0.0);
}

TEST(GPXSpeed, ExcludesInvalidAndMissingIntervals)
{
    auto a = point(0, 1000);
    auto b = point(0.001, 1000);
    EXPECT_FALSE(GPXAnalytics::calculateSpeed(a, b));
    b.time = a.time.value() - std::chrono::seconds(1);
    EXPECT_FALSE(GPXAnalytics::calculateSpeed(a, b));
    b.time.reset();
    EXPECT_FALSE(GPXAnalytics::calculateSpeed(a, b));
    a.time.reset();
    b.time = std::chrono::system_clock::time_point{};
    EXPECT_FALSE(GPXAnalytics::calculateSpeed(a, b));
    a = point(0, 0);
    b = point(std::numeric_limits<double>::quiet_NaN(), 1000);
    EXPECT_FALSE(GPXAnalytics::calculateSpeed(a, b));
}

TEST(GPXSpeed, MaximumNeverConnectsSegmentsOrSkipsMissingPoints)
{
    TrackSegment first;
    first.points = {point(0, 0), point(0.001, 10000)};
    TrackSegment second;
    second.points = {point(50, 11000), point(50.002, 21000)};
    Track track;
    track.segments = {first, second};
    EXPECT_NEAR(*GPXAnalytics::calculateMaxSpeed(track), 22.2389853, 0.00001);
    EXPECT_NEAR(*GPXAnalytics::calculateMaxSpeed(first), 11.1194927, 0.00001);
    EXPECT_FALSE(GPXAnalytics::calculateMaxSpeed(Track{}));
    EXPECT_FALSE(GPXAnalytics::calculateMaxSpeed(TrackSegment{}));
    first.points = {point(0, 0), TrackPoint(0.001, 0), point(0.002, 20000)};
    EXPECT_FALSE(GPXAnalytics::calculateMaxSpeed(first));
    first.points = {point(0, 0), point(0, 1000)};
    EXPECT_EQ(GPXAnalytics::calculateMaxSpeed(first), 0.0);
}

TEST(GPXSpeed, CApiCalculatesWithoutPriorAnalysisAndHandlesInvalidInputs)
{
    const auto data = gpx_parse(
        R"(<gpx><trk><trkseg><trkpt lat="0" lon="0"><time>2026-03-03T09:00:00Z</time></trkpt>
        <trkpt lat="0.001" lon="0"><time>2026-03-03T09:00:10Z</time></trkpt></trkseg></trk></gpx>)",
        nullptr);
    ASSERT_NE(data, nullptr);
    const auto track = gpx_data_get_track(data, 0);
    const auto segment = gpx_track_get_segment(track, 0);
    EXPECT_NEAR(gpx_track_get_max_speed(track), 11.1194927, 0.00001);
    EXPECT_DOUBLE_EQ(gpx_segment_get_max_speed(segment), gpx_track_get_max_speed(track));
    EXPECT_DOUBLE_EQ(gpx_segment_get_point_speed(segment, 1), gpx_track_get_max_speed(track));
    EXPECT_EQ(gpx_segment_get_point_speed(segment, 0), -1);
    EXPECT_EQ(gpx_segment_get_point_speed(segment, 2), -1);
    EXPECT_EQ(gpx_segment_get_point_speed(nullptr, 1), -1);
    EXPECT_EQ(gpx_segment_get_max_speed(nullptr), -1);
    EXPECT_EQ(gpx_track_get_max_speed(nullptr), -1);
    gpx_data_free(data);
}

} // namespace
