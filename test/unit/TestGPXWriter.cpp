// SPDX-License-Identifier: MIT
#include "GPXCore.h"
#include "GPXParser.hpp"
#include "GPXWriter.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

using namespace qGpx::core;

namespace
{

GPXData sampleData()
{
    GPXData data;
    data.creator = "Recorder \"A&B\"\n\t";
    Track track;
    track.name = "A < B & \"C\" \xCE\xB1";
    track.description = "First\r\nSecond";
    TrackSegment segment;
    TrackPoint point(90, -180, -12.345678901234567);
    point.time = GPXParser::parseTime("2026-11-12T18:00:00.123456789+02:00");
    segment.points = {point, TrackPoint(0.000000000000000001, 23.123456789012345)};
    track.segments = {segment, segment};
    data.tracks = {track, track};
    return data;
}

TEST(GPXWriter, RoundTripsModeledDataAndPreservesPrecision)
{
    const auto data = sampleData();
    const auto xml = GPXWriter{}.serialize(data);
    ASSERT_TRUE(xml.success) << xml.errorMessage;
    const auto parsed = GPXParser{}.parse(*xml.value);
    ASSERT_TRUE(parsed.success) << parsed.errorMessage;
    EXPECT_EQ(parsed.value->creator, data.creator);
    ASSERT_EQ(parsed.value->tracks.size(), 2u);
    const auto& track = parsed.value->tracks.front();
    EXPECT_EQ(track.name, data.tracks.front().name);
    EXPECT_EQ(track.description, data.tracks.front().description);
    ASSERT_EQ(track.segments.size(), 2u);
    ASSERT_EQ(track.segments[1].points.size(), 2u);
    EXPECT_EQ(track.segments[1].points.front().time,
              data.tracks.front().segments[1].points.front().time);
    EXPECT_EQ(parsed.value->tracks[1].name, data.tracks[1].name);
    ASSERT_EQ(track.segments.front().points.size(), 2u);
    for (std::size_t i = 0; i < 2; ++i)
    {
        const auto& original = data.tracks.front().segments.front().points[i];
        const auto& restored = track.segments.front().points[i];
        EXPECT_DOUBLE_EQ(restored.latitude, original.latitude);
        EXPECT_DOUBLE_EQ(restored.longitude, original.longitude);
        EXPECT_EQ(restored.elevation, original.elevation);
        EXPECT_EQ(restored.time, original.time);
    }
    const auto fraction = std::chrono::duration_cast<std::chrono::system_clock::duration>(
        std::chrono::nanoseconds(123456789));
    const auto utcSeconds = GPXParser::parseTime("2026-11-12T16:00:00Z");
    ASSERT_TRUE(utcSeconds.has_value());
    EXPECT_EQ(data.tracks.front().segments.front().points.front().time, *utcSeconds + fraction);

    std::ostringstream expectedTimestamp;
    expectedTimestamp.imbue(std::locale::classic());
    expectedTimestamp << "2026-11-12T16:00:00." << std::setw(9) << std::setfill('0')
                      << std::chrono::duration_cast<std::chrono::nanoseconds>(fraction).count()
                      << 'Z';
    EXPECT_NE(xml.value->find(expectedTimestamp.str()), std::string::npos);
    EXPECT_FALSE(track.totalDistance);
}

class CommaDecimal : public std::numpunct<char>
{
protected:
    char do_decimal_point() const override
    {
        return ',';
    }
};

TEST(GPXWriter, UsesSchemaDecimalsIndependentOfLocaleAndFormatsPreEpochTimes)
{
    auto data = sampleData();
    auto& point = data.tracks.front().segments.front().points.front();
    point.time = std::chrono::system_clock::time_point(-std::chrono::milliseconds(1750));
    point.elevation = 1e100;
    const auto previous = std::locale();
    std::locale::global(std::locale(previous, new CommaDecimal));
    const auto xml = GPXWriter{}.serialize(data);
    std::locale::global(previous);
    ASSERT_TRUE(xml.success) << xml.errorMessage;
    EXPECT_NE(xml.value->find("1969-12-31T23:59:58.250000000Z"), std::string::npos);
    EXPECT_NE(xml.value->find("lat=\"0.000000000000000001"), std::string::npos);
    EXPECT_EQ(xml.value->find("e+"), std::string::npos);
    const auto parsed = GPXParser{}.parse(*xml.value);
    ASSERT_TRUE(parsed.success) << parsed.errorMessage;
    const auto& restored = parsed.value->tracks.front().segments.front().points.front();
    EXPECT_EQ(restored.time, point.time);
    EXPECT_DOUBLE_EQ(*restored.elevation, *point.elevation);
}

TEST(GPXWriter, RejectsInvalidCoordinatesElevationAndXmlText)
{
    auto data = sampleData();
    auto& point = data.tracks.front().segments.front().points.front();
    for (double latitude : {-91.0, 91.0, std::numeric_limits<double>::quiet_NaN()})
    {
        point.latitude = latitude;
        const auto result = GPXWriter{}.serialize(data);
        EXPECT_FALSE(result.success);
        EXPECT_FALSE(result.errorMessage.empty());
    }
    point.latitude = -90;
    for (double longitude : {-181.0, 180.0, std::numeric_limits<double>::infinity()})
    {
        point.longitude = longitude;
        EXPECT_FALSE(GPXWriter{}.serialize(data).success);
    }
    point.longitude = -180;
    point.elevation = std::numeric_limits<double>::infinity();
    EXPECT_FALSE(GPXWriter{}.serialize(data).success);
    point.elevation.reset();
    for (const auto& text :
         {std::string("bad\0text", 8), std::string("\x01"), std::string("\xC0\xAF")})
    {
        data.tracks.front().name = text;
        EXPECT_FALSE(GPXWriter{}.serialize(data).success);
    }
}

TEST(GPXWriter, SupportsEmptyGpxAndDefaultCreator)
{
    const auto xml = GPXWriter{}.serialize(GPXData{});
    ASSERT_TRUE(xml.success) << xml.errorMessage;
    EXPECT_NE(xml.value->find("creator=\"qGPX\""), std::string::npos);
    // Empty GPX is schema-valid, but the track-oriented parser rejects it.
    EXPECT_FALSE(GPXParser{}.parse(*xml.value).success);
}

TEST(GPXWriter, FormatsClockBoundsWithoutOverflow)
{
    if (std::chrono::system_clock::period::num != 1 ||
        std::chrono::system_clock::period::den != 1000000000)
    {
        GTEST_SKIP() << "Boundary dates below are for nanosecond clocks";
    }
    auto data = sampleData();
    auto& point = data.tracks.front().segments.front().points.front();
    point.time = std::chrono::system_clock::time_point::min();
    const auto minimum = GPXWriter{}.serialize(data);
    ASSERT_TRUE(minimum.success) << minimum.errorMessage;
    EXPECT_NE(minimum.value->find("1677-09-21T00:12:43.145224192Z"), std::string::npos);
    point.time = std::chrono::system_clock::time_point::max();
    const auto maximum = GPXWriter{}.serialize(data);
    ASSERT_TRUE(maximum.success) << maximum.errorMessage;
    EXPECT_NE(maximum.value->find("2262-04-11T23:47:16.854775807Z"), std::string::npos);
}

class GPXFileIO : public ::testing::Test
{
protected:
    std::filesystem::path directory;
    bool ownsDirectory = false;

    void SetUp() override
    {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        directory =
            std::filesystem::temp_directory_path() / ("qgpx-writer-" + std::to_string(suffix));
        ownsDirectory = std::filesystem::create_directory(directory);
        ASSERT_TRUE(ownsDirectory);
    }

    void TearDown() override
    {
        if (ownsDirectory)
        {
            std::filesystem::remove_all(directory);
        }
    }
};

TEST_F(GPXFileIO, CppSaveLoadAndInvalidDataDoesNotOverwrite)
{
    const auto path = (directory / "track.gpx").string();
    auto data = sampleData();
    ASSERT_TRUE(GPXWriter{}.saveFile(data, path).success);
    const auto loaded = GPXParser{}.parseFile(path);
    ASSERT_TRUE(loaded.success) << loaded.errorMessage;
    EXPECT_EQ(loaded.value->creator, data.creator);
    data.tracks.front().segments.front().points.front().latitude = 100;
    EXPECT_FALSE(GPXWriter{}.saveFile(data, path).success);
    EXPECT_TRUE(GPXParser{}.parseFile(path).success);
    EXPECT_FALSE(GPXWriter{}.saveFile(sampleData(), directory.string()).success);
    EXPECT_FALSE(
        GPXWriter{}.saveFile(sampleData(), (directory / "missing" / "out.gpx").string()).success);
    EXPECT_FALSE(GPXWriter{}.saveFile(sampleData(), "").success);
    EXPECT_FALSE(GPXWriter{}.saveFile(sampleData(), path + std::string("\0suffix", 7)).success);
    const auto unreadable = GPXParser{}.parseFile(directory.string());
    EXPECT_FALSE(unreadable.success);
    EXPECT_EQ(unreadable.errorMessage, "Failed to read file: " + directory.string());
    EXPECT_FALSE(GPXParser{}.parseFile(path + std::string("\0suffix", 7)).success);
}

TEST_F(GPXFileIO, CApiLoadsSerializesSavesAndReportsErrors)
{
    const auto path = (directory / "track.gpx").string();
    ASSERT_TRUE(GPXWriter{}.saveFile(sampleData(), path).success);
    char* error = nullptr;
    const auto handle = gpxParseFile(path.c_str(), &error);
    ASSERT_NE(handle, nullptr);
    EXPECT_EQ(error, nullptr);
    char* xml = gpxSerialize(handle, &error);
    ASSERT_NE(xml, nullptr);
    EXPECT_EQ(error, nullptr);
    EXPECT_TRUE(GPXParser{}.parse(xml).success);
    gpxStringFree(xml);
    EXPECT_TRUE(gpxSaveFile(handle, path.c_str(), &error));
    EXPECT_EQ(error, nullptr);
    gpxDataFree(handle);
    EXPECT_EQ(gpxParseFile(nullptr, &error), nullptr);
    ASSERT_NE(error, nullptr);
    gpxStringFree(error);
    EXPECT_EQ(gpxParseFile((directory / "missing.gpx").string().c_str(), &error), nullptr);
    ASSERT_NE(error, nullptr);
    gpxStringFree(error);
    EXPECT_EQ(gpxSerialize(nullptr, &error), nullptr);
    ASSERT_NE(error, nullptr);
    gpxStringFree(error);
    EXPECT_FALSE(gpxSaveFile(nullptr, path.c_str(), &error));
    ASSERT_NE(error, nullptr);
    gpxStringFree(error);
    const auto valid = gpxParseFile(path.c_str(), nullptr);
    ASSERT_NE(valid, nullptr);
    EXPECT_FALSE(gpxSaveFile(valid, nullptr, &error));
    ASSERT_NE(error, nullptr);
    gpxStringFree(error);
    EXPECT_FALSE(gpxSaveFile(valid, directory.string().c_str(), &error));
    ASSERT_NE(error, nullptr);
    gpxStringFree(error);
    gpxDataFree(valid);
    gpxStringFree(nullptr);
}

TEST_F(GPXFileIO, CApiCreatorIsCopiedAndUsedForSerializationAndSaving)
{
    const auto path = (directory / "track.gpx").string();
    ASSERT_TRUE(GPXWriter{}.saveFile(sampleData(), path).success);
    const auto handle = gpxParseFile(path.c_str(), nullptr);
    ASSERT_NE(handle, nullptr);
    char creator[] = "My app & \"GPX\" \xCE\xB1";
    const std::string expected = creator;
    char* error = nullptr;
    ASSERT_TRUE(gpxDataSetCreator(handle, creator, &error));
    EXPECT_EQ(error, nullptr);
    creator[0] = 'X';
    char* xml = gpxSerialize(handle, &error);
    ASSERT_NE(xml, nullptr);
    const auto parsed = GPXParser{}.parse(xml);
    ASSERT_TRUE(parsed.success) << parsed.errorMessage;
    EXPECT_EQ(parsed.value->creator, expected);
    gpxStringFree(xml);
    ASSERT_TRUE(gpxSaveFile(handle, path.c_str(), &error));
    const auto loaded = GPXParser{}.parseFile(path);
    ASSERT_TRUE(loaded.success) << loaded.errorMessage;
    EXPECT_EQ(loaded.value->creator, expected);
    ASSERT_TRUE(gpxDataSetCreator(handle, "", nullptr));
    xml = gpxSerialize(handle, &error);
    ASSERT_NE(xml, nullptr);
    const auto fallback = GPXParser{}.parse(xml);
    ASSERT_TRUE(fallback.success) << fallback.errorMessage;
    EXPECT_EQ(fallback.value->creator, "qGPX");
    gpxStringFree(xml);
    gpxDataFree(handle);
}

TEST(GPXWriter, CApiCreatorRejectsNullInputsWithoutChangingData)
{
    const auto source = GPXWriter{}.serialize(sampleData());
    ASSERT_TRUE(source.success) << source.errorMessage;
    const auto handle = gpxParse(source.value->c_str(), nullptr);
    ASSERT_NE(handle, nullptr);
    char* error = nullptr;
    EXPECT_FALSE(gpxDataSetCreator(nullptr, "My app", &error));
    ASSERT_NE(error, nullptr);
    gpxStringFree(error);
    EXPECT_FALSE(gpxDataSetCreator(handle, nullptr, &error));
    ASSERT_NE(error, nullptr);
    EXPECT_STREQ(gpxDataGetCreator(handle), sampleData().creator.c_str());
    gpxStringFree(error);
    error = nullptr;
    EXPECT_TRUE(gpxDataSetCreator(handle, "My app", &error));
    EXPECT_EQ(error, nullptr);
    EXPECT_FALSE(gpxDataSetCreator(nullptr, "My app", nullptr));
    EXPECT_FALSE(gpxDataSetCreator(handle, nullptr, nullptr));
    gpxDataFree(handle);
}

#ifdef __linux__
TEST(GPXWriter, ReportsBufferedWriteFailure)
{
    const auto result = GPXWriter{}.saveFile(sampleData(), "/dev/full");
    EXPECT_FALSE(result.success);
    EXPECT_NE(result.errorMessage.find("write"), std::string::npos);
}
#endif

} // namespace
