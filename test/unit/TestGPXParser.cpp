// TestGPXParser.cpp
// Unit tests for GPXParser
//
// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#include "GPXParser.hpp"

#include <gtest/gtest.h>

using namespace qGpx::core;

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

TEST(GPXParser, InvalidCoordinateFailure)
{
    GPXParser parser;
    const char* gpx = R"(<gpx><trk><trkseg><trkpt lat="north" lon="6.0"/></trkseg></trk></gpx>)";

    auto result = parser.parse(gpx);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage, "Invalid latitude value");
}

TEST(GPXParser, HexadecimalCoordinateFailure)
{
    GPXParser parser;
    for (const std::string value : {"0x1p5", "0X1P5", "+0x1p5", "-0X1P5", " 0x1p5 ", "0x20"})
    {
        SCOPED_TRACE(value);
        const std::string gpx =
            R"(<gpx><trk><trkseg><trkpt lat=")" + value + R"(" lon="6.0"/></trkseg></trk></gpx>)";

        auto result = parser.parse(gpx);

        EXPECT_FALSE(result.success);
        EXPECT_EQ(result.errorMessage, "Invalid latitude value");
    }
}

TEST(GPXParser, HexadecimalLongitudeFailure)
{
    GPXParser parser;
    for (const std::string value : {"0x1p5", "0X1P5", "+0x1p5", "-0X1P5", " 0x1p5 ", "0x20"})
    {
        SCOPED_TRACE(value);
        const std::string gpx =
            R"(<gpx><trk><trkseg><trkpt lat="45.0" lon=")" + value + R"("/></trkseg></trk></gpx>)";

        auto result = parser.parse(gpx);

        EXPECT_FALSE(result.success);
        EXPECT_EQ(result.errorMessage, "Invalid longitude value");
    }
}

TEST(GPXParser, OutOfRangeCoordinateFailure)
{
    GPXParser parser;
    const char* gpx = R"(<gpx><trk><trkseg><trkpt lat="45.0" lon="181"/></trkseg></trk></gpx>)";

    auto result = parser.parse(gpx);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage, "Invalid longitude value");
}

TEST(GPXParser, InvalidElevationFailure)
{
    GPXParser parser;
    const char* gpx = R"(<gpx><trk><trkseg><trkpt lat="45.0" lon="6.0"><ele>high</ele></trkpt>)"
                      R"(</trkseg></trk></gpx>)";

    auto result = parser.parse(gpx);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage, "Invalid elevation value");
}

TEST(GPXParser, HexadecimalElevationFailure)
{
    GPXParser parser;
    for (const std::string value : {"0x1p5", "0X1P5", "+0x1p5", "-0X1P5", " 0x1p5 ", "0x20"})
    {
        SCOPED_TRACE(value);
        const std::string gpx = R"(<gpx><trk><trkseg><trkpt lat="45.0" lon="6.0"><ele>)" + value +
                                R"(</ele></trkpt></trkseg></trk></gpx>)";

        auto result = parser.parse(gpx);

        EXPECT_FALSE(result.success);
        EXPECT_EQ(result.errorMessage, "Invalid elevation value");
    }
}

TEST(GPXParser, ScientificNotationFailure)
{
    GPXParser parser;
    for (const std::string value : {"3.2e1", "3.2E+1", "32e-1", "-3.2e1", " 3.2e1 "})
    {
        SCOPED_TRACE(value);
        auto latitude = parser.parse(R"(<gpx><trk><trkseg><trkpt lat=")" + value +
                                     R"(" lon="6.0"/></trkseg></trk></gpx>)");
        EXPECT_FALSE(latitude.success);
        EXPECT_EQ(latitude.errorMessage, "Invalid latitude value");

        auto longitude = parser.parse(R"(<gpx><trk><trkseg><trkpt lat="45.0" lon=")" + value +
                                      R"("/></trkseg></trk></gpx>)");
        EXPECT_FALSE(longitude.success);
        EXPECT_EQ(longitude.errorMessage, "Invalid longitude value");

        auto elevation = parser.parse(R"(<gpx><trk><trkseg><trkpt lat="45.0" lon="6.0"><ele>)" +
                                      value + R"(</ele></trkpt></trkseg></trk></gpx>)");
        EXPECT_FALSE(elevation.success);
        EXPECT_EQ(elevation.errorMessage, "Invalid elevation value");
    }
}

TEST(GPXParser, DecimalNumberFormats)
{
    GPXParser parser;
    const struct
    {
        const char* text;
        double expected;
    } cases[] = {{"32", 32.0},     {"+32.0", 32.0}, {"-32.0", -32.0},
                 {" 32.0 ", 32.0}, {".5", 0.5},     {"32.", 32.0}};

    for (const auto& testCase : cases)
    {
        SCOPED_TRACE(testCase.text);
        const std::string value = testCase.text;
        const std::string gpx = R"(<gpx><trk><trkseg><trkpt lat=")" + value + R"(" lon=")" + value +
                                R"("><ele>)" + value + R"(</ele></trkpt></trkseg></trk></gpx>)";

        auto result = parser.parse(gpx);

        ASSERT_TRUE(result.success) << result.errorMessage;
        ASSERT_TRUE(result.value.has_value());
        const auto& point = result.value->tracks[0].segments[0].points[0];
        EXPECT_DOUBLE_EQ(point.latitude, testCase.expected);
        EXPECT_DOUBLE_EQ(point.longitude, testCase.expected);
        ASSERT_TRUE(point.elevation.has_value());
        EXPECT_DOUBLE_EQ(*point.elevation, testCase.expected);
    }
}

TEST(GPXParser, EmptyElevationFailure)
{
    GPXParser parser;
    const char* gpx = R"(<gpx><trk><trkseg><trkpt lat="45.0" lon="6.0"><ele/></trkpt>)"
                      R"(</trkseg></trk></gpx>)";

    auto result = parser.parse(gpx);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage, "Invalid elevation value");
}

TEST(GPXParser, MissingCoordinateFailure)
{
    GPXParser parser;
    const char* gpx = R"(<gpx><trk><trkseg><trkpt lat="45.0"/></trkseg></trk></gpx>)";

    auto result = parser.parse(gpx);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage, "Track point is missing latitude or longitude");
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

TEST(GPXParser, ReadsGarminMeasurementsAndRetainsUnknownExtensionTree)
{
    GPXParser parser;
    const char* gpx = R"(<?xml version="1.0"?>
<g:gpx xmlns:g="http://www.topografix.com/GPX/1/1"
       xmlns:t="http://www.garmin.com/xmlschemas/TrackPointExtension/v1"
       xmlns:v="urn:vendor:test" creator="test">
  <g:trk><g:trkseg><g:trkpt lat="45" lon="6">
    <g:extensions><t:TrackPointExtension><t:atemp>-3.5</t:atemp><t:hr>120</t:hr><t:cad>0</t:cad><t:Extensions><v:Power v:unit="W">250</v:Power></t:Extensions></t:TrackPointExtension><v:unknown v:flag="kept"><v:child/>before<em>after</em></v:unknown></g:extensions>
  </g:trkpt></g:trkseg></g:trk>
</g:gpx>)";

    auto result = parser.parse(gpx);

    ASSERT_TRUE(result.success) << result.errorMessage;
    const auto& point = result.value->tracks[0].segments[0].points[0];
    ASSERT_TRUE(point.heartRateBpm);
    EXPECT_EQ(*point.heartRateBpm, 120);
    ASSERT_TRUE(point.cadenceRpm);
    EXPECT_EQ(*point.cadenceRpm, 0);
    ASSERT_TRUE(point.temperatureCelsius);
    EXPECT_DOUBLE_EQ(*point.temperatureCelsius, -3.5);
    ASSERT_EQ(point.extensions.size(), 2u);
    const auto& retained = point.extensions[1];
    EXPECT_EQ(retained.namespaceUri, "urn:vendor:test");
    EXPECT_EQ(retained.localName, "unknown");
    ASSERT_EQ(retained.attributes.size(), 1u);
    EXPECT_EQ(retained.attributes[0].namespaceUri, "urn:vendor:test");
    EXPECT_EQ(retained.attributes[0].localName, "flag");
    EXPECT_EQ(retained.attributes[0].value, "kept");
    ASSERT_EQ(retained.children.size(), 3u);
    EXPECT_EQ(retained.children[0].localName, "child");
    EXPECT_EQ(retained.children[1].kind, ExtensionNode::Kind::Text);
    EXPECT_EQ(retained.children[1].text, "before");
    EXPECT_EQ(retained.children[2].localName, "em");
    ASSERT_EQ(retained.children[2].children.size(), 1u);
    EXPECT_EQ(retained.children[2].children[0].text, "after");
}

TEST(GPXParser, ReadsTemperatureFromGarminGpxExtensionsV3)
{
    const auto result = GPXParser{}.parse(
        R"(<gpx xmlns:g="http://www.garmin.com/xmlschemas/GpxExtensions/v3" xmlns:v="urn:vendor"><trk><trkseg><trkpt lat="45" lon="6"><extensions><g:TrackPointExtension><g:Temperature>18.75</g:Temperature><g:Extensions><v:PowerInWatts>250</v:PowerInWatts></g:Extensions></g:TrackPointExtension></extensions></trkpt></trkseg></trk></gpx>)");

    ASSERT_TRUE(result.success) << result.errorMessage;
    const auto& point = result.value->tracks[0].segments[0].points[0];
    ASSERT_TRUE(point.temperatureCelsius);
    EXPECT_DOUBLE_EQ(*point.temperatureCelsius, 18.75);
    EXPECT_FALSE(point.heartRateBpm);
    ASSERT_EQ(point.extensions.size(), 1u);
}

TEST(GPXParser, MatchesGarminMeasurementsByNamespaceUriNotPrefixOrLocalNameAlone)
{
    GPXParser parser;
    const char* gpx = R"(<g:gpx xmlns:g="http://www.topografix.com/GPX/1/1"
 xmlns:a="http://www.garmin.com/xmlschemas/TrackPointExtension/v2"
 xmlns:b="urn:lookalike"><g:trk><g:trkseg><g:trkpt lat="45" lon="6">
 <g:extensions><a:TrackPointExtension><a:hr xmlns:a="urn:lookalike">120</a:hr>
 <b:cad>90</b:cad></a:TrackPointExtension></g:extensions>
 </g:trkpt></g:trkseg></g:trk></g:gpx>)";

    auto result = parser.parse(gpx);

    ASSERT_TRUE(result.success) << result.errorMessage;
    const auto& point = result.value->tracks[0].segments[0].points[0];
    EXPECT_FALSE(point.heartRateBpm);
    EXPECT_FALSE(point.cadenceRpm);
    ASSERT_EQ(point.extensions.size(), 1u);
    const auto& extension = point.extensions[0];
    EXPECT_EQ(extension.namespaceUri, "http://www.garmin.com/xmlschemas/TrackPointExtension/v2");
    ASSERT_GE(extension.children.size(), 2u);
    EXPECT_EQ(extension.children[0].namespaceUri, "urn:lookalike");

    const auto wrongGpxNamespace = parser.parse(
        R"(<x:gpx xmlns:x="urn:not-gpx"><x:trk><x:trkseg><x:trkpt lat="45" lon="6"/></x:trkseg></x:trk></x:gpx>)");
    EXPECT_FALSE(wrongGpxNamespace.success);
}

TEST(GPXParser, RejectsInvalidAndDuplicateGarminMeasurements)
{
    GPXParser parser;
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"<t:hr>0</t:hr>", "Invalid Garmin heart rate value"},
        {"<t:cad>255</t:cad>", "Invalid Garmin cadence value"},
        {"<t:atemp>NaN</t:atemp>", "Invalid Garmin temperature value"},
        {"<t:hr>100</t:hr><t:hr>101</t:hr>", "Duplicate Garmin heart rate value"},
        {"<t:hr><v:value>100</v:value></t:hr>", "Malformed Garmin sensor value"}};
    for (const auto& testCase : cases)
    {
        SCOPED_TRACE(testCase.first);
        const auto xml =
            R"(<gpx xmlns:t="http://www.garmin.com/xmlschemas/TrackPointExtension/v1" xmlns:v="urn:v"><trk><trkseg><trkpt lat="45" lon="6"><extensions><t:TrackPointExtension>)" +
            testCase.first +
            R"(</t:TrackPointExtension></extensions></trkpt></trkseg></trk></gpx>)";
        const auto result = parser.parse(xml);
        EXPECT_FALSE(result.success);
        EXPECT_EQ(result.errorMessage, testCase.second);
    }
}

TEST(GPXParser, EnforcesExtensionDepthAndSizeLimits)
{
    GPXParser parser;
    std::string deepContent;
    for (int i = 0; i < 33; ++i)
    {
        deepContent += "<v:x>";
    }
    for (int i = 0; i < 33; ++i)
    {
        deepContent += "</v:x>";
    }
    auto deep =
        parser.parse(R"(<gpx xmlns:v="urn:v"><trk><trkseg><trkpt lat="45" lon="6"><extensions>)" +
                     deepContent + R"(</extensions></trkpt></trkseg></trk></gpx>)");
    EXPECT_FALSE(deep.success);
    EXPECT_NE(deep.errorMessage.find("maximum depth of 32"), std::string::npos);

    const std::string largeValue(600 * 1024, 'x');
    const std::string largeExtension =
        R"(<extensions><v:x>)" + largeValue + R"(</v:x></extensions>)";
    auto oversized =
        parser.parse(R"(<gpx xmlns:v="urn:v"><trk><trkseg><trkpt lat="45" lon="6">)" +
                     largeExtension + largeExtension + R"(</trkpt></trkseg></trk></gpx>)");
    EXPECT_FALSE(oversized.success);
    EXPECT_NE(oversized.errorMessage.find("1 MiB limit"), std::string::npos);
}

TEST(GPXParser, IgnoresDataTagsOutsideTracks)
{
    GPXParser parser;
    const char* gpx = R"(<?xml version="1.0"?>
<gpx version="1.1" creator="test">
  <metadata>
    <name>Ignored name</name>
    <desc>Ignored description</desc>
    <author><name>Someone</name></author>
    <link href="example.com"><text>Link</text></link>
    <time>2026-03-03T09:51:09.000Z</time>
  </metadata>
  <wpt lat="1.0" lon="2.0"><name>Waypoint</name><ele>5</ele><time>2026-03-03T09:51:09Z</time></wpt>
  <trk>
    <name>Real name</name>
    <extensions><name>Extension name</name></extensions>
    <trkseg>
      <trkpt lat="45.0" lon="6.0">
        <ele>10</ele>
        <extensions><name>Point extension</name><time>invalid</time></extensions>
      </trkpt>
    </trkseg>
  </trk>
</gpx>)";

    auto result = parser.parse(gpx);

    ASSERT_TRUE(result.success) << result.errorMessage;
    const auto& track = result.value.value().tracks[0];
    EXPECT_EQ(track.name, "Real name");
    ASSERT_EQ(track.segments[0].pointCount(), 1);
    EXPECT_DOUBLE_EQ(track.segments[0].points[0].elevation.value(), 10.0);
    EXPECT_FALSE(track.segments[0].points[0].time.has_value());
}
