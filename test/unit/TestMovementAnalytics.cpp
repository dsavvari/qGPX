// SPDX-License-Identifier: MIT
#include "GPXAnalytics.hpp"

#include <gtest/gtest.h>

#include <limits>

using namespace qGpx::core;

namespace
{

TrackPoint point(double latitude, double longitude, int milliseconds)
{
    TrackPoint result(latitude, longitude);
    result.time = std::chrono::system_clock::time_point(std::chrono::milliseconds(milliseconds));
    return result;
}

TEST(MovementAnalytics, ClassifiesMovingAndStoppedIntervals)
{
    TrackSegment segment;
    const auto options = GPXAnalytics::MovementOptions(0.5);

    segment.points = {
        point(0, 0, 0),
        point(0.00005, 0, 10000),
    };

    const auto stats = GPXAnalytics::calculateMovementStats(segment, options);
    ASSERT_TRUE(stats);
    EXPECT_EQ(stats->validIntervals, 1);
    EXPECT_EQ(stats->invalidIntervals, 0);
    EXPECT_GT(stats->movingDuration, 0.0);
    EXPECT_EQ(stats->stoppedDuration, 0.0);
    EXPECT_GT(stats->movingDistance, 0.0);
    EXPECT_EQ(stats->stoppedDistance, 0.0);
}

TEST(MovementAnalytics, DetectsStoppedMovement)
{
    TrackSegment segment;
    const auto options = GPXAnalytics::MovementOptions(0.5);

    segment.points = {
        point(0, 0, 0),
        point(0, 0, 10000),
    };

    const auto stats = GPXAnalytics::calculateMovementStats(segment, options);
    ASSERT_TRUE(stats);
    EXPECT_EQ(stats->validIntervals, 1);
    EXPECT_GT(stats->stoppedDuration, 0.0);
    EXPECT_EQ(stats->movingDuration, 0.0);
    EXPECT_GT(stats->elapsedDuration, 0.0);
    EXPECT_EQ(stats->movingAverageSpeed, 0.0);
}

TEST(MovementAnalytics, HandlesThresholdBoundary)
{
    TrackSegment segment;
    const auto options = GPXAnalytics::MovementOptions(1.0);

    segment.points = {
        point(0, 0, 0),
        point(0.00001, 0, 1000),
    };

    const auto stats = GPXAnalytics::calculateMovementStats(segment, options);
    ASSERT_TRUE(stats);
    EXPECT_EQ(stats->validIntervals, 1);
    EXPECT_NEAR(stats->movingDuration, 1.0, 0.01);
    EXPECT_EQ(stats->stoppedDuration, 0.0);
}

TEST(MovementAnalytics, ExcludesInvalidIntervals)
{
    TrackSegment segment;
    const auto options = GPXAnalytics::MovementOptions(0.5);

    auto p1 = point(0, 0, 0);
    auto p2 = point(0.00001, 0, 0);
    auto p3 = point(0.00006, 0, 10000);

    segment.points = {p1, p2, p3};

    const auto stats = GPXAnalytics::calculateMovementStats(segment, options);
    ASSERT_TRUE(stats);
    EXPECT_EQ(stats->validIntervals, 1);
    EXPECT_EQ(stats->invalidIntervals, 1);
    EXPECT_GT(stats->movingDuration, 0.0);
}

TEST(MovementAnalytics, DetectsAndReportsGaps)
{
    TrackSegment segment;
    const auto options = GPXAnalytics::MovementOptions(0.5, 300.0, 50.0);

    segment.points = {
        point(0, 0, 0),
        point(100, 0, 1000),
    };

    const auto stats = GPXAnalytics::calculateMovementStats(segment, options);
    EXPECT_FALSE(stats);

    TrackSegment segment2;
    segment2.points = {
        point(0, 0, 0),
        point(0.00005, 0, 1000),
        point(100, 0, 2000),
    };

    const auto stats2 = GPXAnalytics::calculateMovementStats(segment2, options);
    ASSERT_TRUE(stats2);
    EXPECT_EQ(stats2->gapIntervals, 1);
    EXPECT_EQ(stats2->validIntervals, 1);
}

TEST(MovementAnalytics, CalculatesAverageSpeeds)
{
    TrackSegment segment;
    const auto options = GPXAnalytics::MovementOptions(0.5);

    segment.points = {
        point(0, 0, 0),
        point(0.000025, 0, 5000),
        point(0.00005, 0, 10000),
    };

    const auto stats = GPXAnalytics::calculateMovementStats(segment, options);
    ASSERT_TRUE(stats);
    EXPECT_GT(stats->movingAverageSpeed, 0.0);
    EXPECT_GT(stats->elapsedAverageSpeed, 0.0);
    EXPECT_EQ(stats->movingAverageSpeed, stats->elapsedAverageSpeed);
}

TEST(MovementAnalytics, AggregatesTotalDurationAndDistance)
{
    TrackSegment segment;
    const auto options = GPXAnalytics::MovementOptions(0.5);

    segment.points = {
        point(0, 0, 0),
        point(0, 0, 5000),
        point(0.00005, 0, 10000),
    };

    const auto stats = GPXAnalytics::calculateMovementStats(segment, options);
    ASSERT_TRUE(stats);
    EXPECT_NEAR(stats->elapsedDuration, 10.0, 0.01);
    EXPECT_NEAR(stats->stoppedDuration, 5.0, 0.01);
    EXPECT_NEAR(stats->movingDuration, 5.0, 0.01);
}

TEST(MovementAnalytics, ReturnsNulloptForEmptySegment)
{
    TrackSegment segment;
    const auto options = GPXAnalytics::MovementOptions(0.5);

    const auto stats = GPXAnalytics::calculateMovementStats(segment, options);
    EXPECT_FALSE(stats);
}

TEST(MovementAnalytics, ReturnsNulloptForSinglePoint)
{
    TrackSegment segment;
    const auto options = GPXAnalytics::MovementOptions(0.5);

    segment.points = {point(0, 0, 0)};

    const auto stats = GPXAnalytics::calculateMovementStats(segment, options);
    EXPECT_FALSE(stats);
}

TEST(MovementAnalytics, HandlesMissingTimestamps)
{
    TrackSegment segment;
    const auto options = GPXAnalytics::MovementOptions(0.5);

    auto p1 = point(0, 0, 0);
    auto p2 = point(0.00001, 0, 10000);
    p1.time.reset();

    segment.points = {p1, p2};

    const auto stats = GPXAnalytics::calculateMovementStats(segment, options);
    EXPECT_FALSE(stats);
}

TEST(MovementAnalytics, AggregatesMultipleSegments)
{
    const auto options = GPXAnalytics::MovementOptions(0.5);

    TrackSegment segment1;
    segment1.points = {
        point(0, 0, 0),
        point(0.00005, 0, 10000),
    };

    TrackSegment segment2;
    segment2.points = {
        point(50, 0, 20000),
        point(50.00005, 0, 30000),
    };

    Track track;
    track.segments = {segment1, segment2};

    const auto stats = GPXAnalytics::calculateMovementStats(track, options);
    ASSERT_TRUE(stats);
    EXPECT_EQ(stats->validIntervals, 2);
    EXPECT_NEAR(stats->elapsedDuration, 20.0, 0.01);
}

TEST(MovementAnalytics, DoesNotConnectSegments)
{
    const auto options = GPXAnalytics::MovementOptions(0.5);

    TrackSegment segment1;
    segment1.points = {
        point(0, 0, 0),
        point(0.001, 0, 10000),
    };

    TrackSegment segment2;
    segment2.points = {
        point(100, 0, 11000),
        point(100.001, 0, 21000),
    };

    Track track;
    track.segments = {segment1, segment2};

    const auto stats = GPXAnalytics::calculateMovementStats(track, options);
    ASSERT_TRUE(stats);
    EXPECT_EQ(stats->gapIntervals, 0);
    EXPECT_EQ(stats->validIntervals, 2);
}

TEST(MovementAnalytics, ComputesMovingTimeAndDistance)
{
    TrackSegment segment;
    const auto options = GPXAnalytics::MovementOptions(0.5);

    segment.points = {
        point(0, 0, 0),
        point(0, 0, 5000),
        point(0.00005, 0, 10000),
    };

    const auto stats = GPXAnalytics::calculateMovementStats(segment, options);
    ASSERT_TRUE(stats);
    EXPECT_NEAR(stats->movingDuration, 5.0, 0.01);
    EXPECT_EQ(stats->stoppedDuration, 5.0);
    EXPECT_GT(stats->movingDistance, 0.0);
    EXPECT_EQ(stats->stoppedDistance, 0.0);
}

TEST(MovementAnalytics, ReportsCoverageDiagnostics)
{
    TrackSegment segment;
    const auto options = GPXAnalytics::MovementOptions(0.5);

    auto p1 = point(0, 0, 0);
    auto p2 = point(0.000025, 0, 10000);
    auto p3 = point(0.00005, 0, 10000);
    auto p4 = point(0.000075, 0, 20000);

    segment.points = {p1, p2, p3, p4};

    const auto stats = GPXAnalytics::calculateMovementStats(segment, options);
    ASSERT_TRUE(stats);
    EXPECT_EQ(stats->validIntervals, 2);
    EXPECT_EQ(stats->invalidIntervals, 1);
}

TEST(MovementAnalytics, AnalyseTrackPopulatesMovementStats)
{
    Track track;
    track.segments.resize(1);
    track.segments[0].points = {
        point(0, 0, 0),
        point(0.00005, 0, 10000),
    };

    GPXAnalytics::analyseTrack(track);
    ASSERT_TRUE(track.movementStats);
    EXPECT_GT(track.movementStats->movingDuration, 0.0);
    EXPECT_GT(track.segments[0].movementStats->movingDuration, 0.0);
}

TEST(MovementAnalytics, HandlesNonFiniteValues)
{
    TrackSegment segment;
    const auto options = GPXAnalytics::MovementOptions(0.5);

    auto p1 = point(0, 0, 0);
    auto p2 = point(std::numeric_limits<double>::quiet_NaN(), 0, 10000);

    segment.points = {p1, p2};

    const auto stats = GPXAnalytics::calculateMovementStats(segment, options);
    EXPECT_FALSE(stats);
}

TEST(MovementAnalytics, CustomThresholdChangesClassification)
{
    TrackSegment segment;
    segment.points = {
        point(0, 0, 0),
        point(0.00005, 0, 10000),
    };

    const auto lowThreshold = GPXAnalytics::MovementOptions(0.1);
    const auto highThreshold = GPXAnalytics::MovementOptions(1.0);

    const auto statsLow = GPXAnalytics::calculateMovementStats(segment, lowThreshold);
    const auto statsHigh = GPXAnalytics::calculateMovementStats(segment, highThreshold);

    ASSERT_TRUE(statsLow);
    ASSERT_TRUE(statsHigh);
    EXPECT_GT(statsLow->movingDuration, 0.0);
    EXPECT_EQ(statsHigh->movingDuration, 0.0);
}

} // namespace
