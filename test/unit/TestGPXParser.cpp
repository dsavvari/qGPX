// TestGPXParser.cpp
// Unit tests for GPXParser
//
// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#include "GPXParser.hpp"

#include <gtest/gtest.h>

using namespace visualgpx::core;

// Basic Parser Tests
TEST(GPXParser, EmptyStringFailure)
{
    GPXParser parser;
    auto result = parser.parse("");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage, "XML content is empty");
}

TEST(GPXParser, InvalidXMLFailure)
{
    GPXParser parser;
    auto result = parser.parse("<invalid>");

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.errorMessage.find("XML parsing error") != std::string::npos);
}

TEST(GPXParser, EmptyGPXFileFailure)
{
    GPXParser parser;
    const char* emptyGPX = R"(<?xml version="1.0"?>
<gpx version="1.1" creator="test">
</gpx>)";

    auto result = parser.parse(emptyGPX);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage, "No tracks found in GPX file");
}

// Minimal Valid GPX Tests
TEST(GPXParser, MinimalValidGPX)
{
    GPXParser parser;
    const char* minimalGPX = R"(<?xml version="1.0"?>
<gpx version="1.1" creator="TestApp">
  <trk>
    <trkseg>
      <trkpt lat="45.0" lon="6.0">
      </trkpt>
    </trkseg>
  </trk>
</gpx>)";

    auto result = parser.parse(minimalGPX);

    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.value.has_value());

    const GPXData& data = result.value.value();
    EXPECT_EQ(data.creator, "TestApp");
    EXPECT_EQ(data.trackCount(), 1);
    EXPECT_EQ(data.tracks[0].segmentCount(), 1);
    EXPECT_EQ(data.tracks[0].segments[0].pointCount(), 1);

    const TrackPoint& pt = data.tracks[0].segments[0].points[0];
    EXPECT_DOUBLE_EQ(pt.latitude, 45.0);
    EXPECT_DOUBLE_EQ(pt.longitude, 6.0);
    EXPECT_FALSE(pt.elevation.has_value());
    EXPECT_FALSE(pt.time.has_value());
}

TEST(GPXParser, TrackPointWithElevation)
{
    GPXParser parser;
    const char* gpx = R"(<?xml version="1.0"?>
<gpx version="1.1" creator="test">
  <trk>
    <trkseg>
      <trkpt lat="45.5" lon="6.5">
        <ele>2500.75</ele>
      </trkpt>
    </trkseg>
  </trk>
</gpx>)";

    auto result = parser.parse(gpx);

    ASSERT_TRUE(result.success);
    const TrackPoint& pt = result.value.value().tracks[0].segments[0].points[0];

    EXPECT_DOUBLE_EQ(pt.latitude, 45.5);
    EXPECT_DOUBLE_EQ(pt.longitude, 6.5);
    ASSERT_TRUE(pt.elevation.has_value());
    EXPECT_DOUBLE_EQ(pt.elevation.value(), 2500.75);
}

TEST(GPXParser, TrackPointWithTime)
{
    GPXParser parser;
    const char* gpx = R"(<?xml version="1.0"?>
<gpx version="1.1" creator="test">
  <trk>
    <trkseg>
      <trkpt lat="45.0" lon="6.0">
        <time>2026-03-03T09:51:09Z</time>
      </trkpt>
    </trkseg>
  </trk>
</gpx>)";

    auto result = parser.parse(gpx);

    ASSERT_TRUE(result.success);
    const TrackPoint& pt = result.value.value().tracks[0].segments[0].points[0];

    ASSERT_TRUE(pt.time.has_value());
    // Verify time was parsed (exact value check would require time conversion)
}

TEST(GPXParser, TrackWithName)
{
    GPXParser parser;
    const char* gpx = R"(<?xml version="1.0"?>
<gpx version="1.1" creator="test">
  <trk>
    <name>My Mountain Hike</name>
    <trkseg>
      <trkpt lat="45.0" lon="6.0"></trkpt>
    </trkseg>
  </trk>
</gpx>)";

    auto result = parser.parse(gpx);

    ASSERT_TRUE(result.success);
    const Track& track = result.value.value().tracks[0];
    EXPECT_EQ(track.name, "My Mountain Hike");
}

TEST(GPXParser, TrackWithDescription)
{
    GPXParser parser;
    const char* gpx = R"(<?xml version="1.0"?>
<gpx version="1.1" creator="test">
  <trk>
    <desc>A beautiful trail</desc>
    <trkseg>
      <trkpt lat="45.0" lon="6.0"></trkpt>
    </trkseg>
  </trk>
</gpx>)";

    auto result = parser.parse(gpx);

    ASSERT_TRUE(result.success);
    const Track& track = result.value.value().tracks[0];
    EXPECT_EQ(track.description, "A beautiful trail");
}

// Multiple Elements Tests
TEST(GPXParser, MultipleTrackPoints)
{
    GPXParser parser;
    const char* gpx = R"(<?xml version="1.0"?>
<gpx version="1.1" creator="test">
  <trk>
    <trkseg>
      <trkpt lat="45.0" lon="6.0"><ele>100</ele></trkpt>
      <trkpt lat="45.1" lon="6.1"><ele>200</ele></trkpt>
      <trkpt lat="45.2" lon="6.2"><ele>300</ele></trkpt>
    </trkseg>
  </trk>
</gpx>)";

    auto result = parser.parse(gpx);

    ASSERT_TRUE(result.success);
    const auto& points = result.value.value().tracks[0].segments[0].points;

    ASSERT_EQ(points.size(), 3);
    EXPECT_DOUBLE_EQ(points[0].latitude, 45.0);
    EXPECT_DOUBLE_EQ(points[1].latitude, 45.1);
    EXPECT_DOUBLE_EQ(points[2].latitude, 45.2);
    EXPECT_DOUBLE_EQ(points[0].elevation.value(), 100.0);
    EXPECT_DOUBLE_EQ(points[1].elevation.value(), 200.0);
    EXPECT_DOUBLE_EQ(points[2].elevation.value(), 300.0);
}

TEST(GPXParser, MultipleSegments)
{
    GPXParser parser;
    const char* gpx = R"(<?xml version="1.0"?>
<gpx version="1.1" creator="test">
  <trk>
    <trkseg>
      <trkpt lat="45.0" lon="6.0"></trkpt>
      <trkpt lat="45.1" lon="6.1"></trkpt>
    </trkseg>
    <trkseg>
      <trkpt lat="46.0" lon="7.0"></trkpt>
      <trkpt lat="46.1" lon="7.1"></trkpt>
    </trkseg>
  </trk>
</gpx>)";

    auto result = parser.parse(gpx);

    ASSERT_TRUE(result.success);
    const Track& track = result.value.value().tracks[0];

    ASSERT_EQ(track.segmentCount(), 2);
    EXPECT_EQ(track.segments[0].pointCount(), 2);
    EXPECT_EQ(track.segments[1].pointCount(), 2);
    EXPECT_DOUBLE_EQ(track.segments[0].points[0].latitude, 45.0);
    EXPECT_DOUBLE_EQ(track.segments[1].points[0].latitude, 46.0);
}

TEST(GPXParser, MultipleTracks)
{
    GPXParser parser;
    const char* gpx = R"(<?xml version="1.0"?>
<gpx version="1.1" creator="test">
  <trk>
    <name>Track 1</name>
    <trkseg>
      <trkpt lat="45.0" lon="6.0"></trkpt>
    </trkseg>
  </trk>
  <trk>
    <name>Track 2</name>
    <trkseg>
      <trkpt lat="46.0" lon="7.0"></trkpt>
    </trkseg>
  </trk>
</gpx>)";

    auto result = parser.parse(gpx);

    ASSERT_TRUE(result.success);
    const GPXData& data = result.value.value();

    ASSERT_EQ(data.trackCount(), 2);
    EXPECT_EQ(data.tracks[0].name, "Track 1");
    EXPECT_EQ(data.tracks[1].name, "Track 2");
}

// Time Parsing Tests
TEST(GPXParser, TimeParsingValid)
{
    auto result = GPXParser::parseTime("2026-03-03T09:51:09Z");
    EXPECT_TRUE(result.has_value());
}

TEST(GPXParser, TimeParsingWithMilliseconds)
{
    // Our parser handles this by ignoring the fractional seconds
    auto result = GPXParser::parseTime("2026-03-03T09:51:09.000Z");
    EXPECT_TRUE(result.has_value());
}

TEST(GPXParser, TimeParsingInvalid)
{
    auto result = GPXParser::parseTime("not-a-date");
    EXPECT_FALSE(result.has_value());
}

TEST(GPXParser, TimeParsingEmpty)
{
    auto result = GPXParser::parseTime("");
    EXPECT_FALSE(result.has_value());
}

// Namespace Support Tests (Garmin extensions)
TEST(GPXParser, IgnoresUnknownNamespaces)
{
    GPXParser parser;
    const char* gpx = R"(<?xml version="1.0"?>
<gpx version="1.1" creator="test"
     xmlns:ns3="http://www.garmin.com/xmlschemas/TrackPointExtension/v1">
  <trk>
    <trkseg>
      <trkpt lat="45.0" lon="6.0">
        <extensions>
          <ns3:TrackPointExtension>
            <ns3:hr>120</ns3:hr>
          </ns3:TrackPointExtension>
        </extensions>
      </trkpt>
    </trkseg>
  </trk>
</gpx>)";

    auto result = parser.parse(gpx);

    // Should parse successfully, ignoring unknown extensions
    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.value.value().tracks[0].segments[0].pointCount(), 1);
}
