// TestGPXModels.cpp
// Unit tests for GPX data models
//
// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#include "GPXModels.hpp"

#include <gtest/gtest.h>

using namespace qGpx::core;

// TrackPoint Tests
TEST(TrackPoint, BasicConstruction)
{
    TrackPoint pt(45.0, 6.0);

    EXPECT_DOUBLE_EQ(pt.latitude, 45.0);
    EXPECT_DOUBLE_EQ(pt.longitude, 6.0);
    EXPECT_FALSE(pt.elevation.has_value());
    EXPECT_FALSE(pt.time.has_value());
}

TEST(TrackPoint, ConstructionWithElevation)
{
    TrackPoint pt(45.0, 6.0, 2500.0);

    EXPECT_DOUBLE_EQ(pt.latitude, 45.0);
    EXPECT_DOUBLE_EQ(pt.longitude, 6.0);
    ASSERT_TRUE(pt.elevation.has_value());
    EXPECT_DOUBLE_EQ(pt.elevation.value(), 2500.0);
}

// TrackSegment Tests
TEST(TrackSegment, EmptySegment)
{
    TrackSegment seg;

    EXPECT_TRUE(seg.isEmpty());
    EXPECT_EQ(seg.pointCount(), 0);
}

TEST(TrackSegment, WithPoints)
{
    TrackSegment seg;
    seg.points.emplace_back(45.0, 6.0);
    seg.points.emplace_back(45.1, 6.1);

    EXPECT_FALSE(seg.isEmpty());
    EXPECT_EQ(seg.pointCount(), 2);
}

// Track Tests
TEST(Track, EmptyTrack)
{
    Track track;

    EXPECT_TRUE(track.isEmpty());
    EXPECT_EQ(track.segmentCount(), 0);
    EXPECT_EQ(track.totalPointCount(), 0);
}

TEST(Track, WithSegments)
{
    Track track;
    track.name = "Test Track";

    TrackSegment seg1;
    seg1.points.emplace_back(45.0, 6.0);
    seg1.points.emplace_back(45.1, 6.1);
    track.segments.push_back(seg1);

    TrackSegment seg2;
    seg2.points.emplace_back(46.0, 7.0);
    track.segments.push_back(seg2);

    EXPECT_FALSE(track.isEmpty());
    EXPECT_EQ(track.segmentCount(), 2);
    EXPECT_EQ(track.totalPointCount(), 3); // 2 + 1
}

// GPXData Tests
TEST(GPXData, EmptyData)
{
    GPXData data;

    EXPECT_TRUE(data.isEmpty());
    EXPECT_EQ(data.trackCount(), 0);
}

TEST(GPXData, WithTracks)
{
    GPXData data;
    data.creator = "TestApp";

    Track track1;
    TrackSegment seg;
    seg.points.emplace_back(45.0, 6.0);
    track1.segments.push_back(seg);
    data.tracks.push_back(track1);

    EXPECT_FALSE(data.isEmpty());
    EXPECT_EQ(data.trackCount(), 1);
    EXPECT_EQ(data.creator, "TestApp");
}

// Result Tests
TEST(Result, SuccessCreation)
{
    auto result = Result<int>::makeSuccess(42);

    EXPECT_TRUE(result.success);
    ASSERT_TRUE(result.value.has_value());
    EXPECT_EQ(result.value.value(), 42);
    EXPECT_TRUE(result.errorMessage.empty());
}

TEST(Result, FailureCreation)
{
    auto result = Result<int>::makeFailure("Something went wrong");

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.value.has_value());
    EXPECT_EQ(result.errorMessage, "Something went wrong");
}

TEST(Result, SuccessWithComplexType)
{
    GPXData data;
    data.creator = "Test";

    auto result = Result<GPXData>::makeSuccess(std::move(data));

    EXPECT_TRUE(result.success);
    ASSERT_TRUE(result.value.has_value());
    EXPECT_EQ(result.value.value().creator, "Test");
}
