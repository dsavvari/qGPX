// SPDX-License-Identifier: MIT
#include "ViewerServer.hpp"

#include <gtest/gtest.h>
#include <json.hpp>

#include <memory>
#include <thread>

namespace
{

class ViewerTest : public testing::Test
{
protected:
    httplib::Server server;
    std::thread worker;
    std::unique_ptr<httplib::Client> client;

    void SetUp() override
    {
        ASSERT_TRUE(qGpx::viewer::configureServer(server, QGPX_TEST_ASSET_DIR));
        const int port = server.bind_to_any_port("127.0.0.1");
        ASSERT_GT(port, 0);
        worker = std::thread([this] { server.listen_after_bind(); });
        server.wait_until_ready();
        client = std::make_unique<httplib::Client>("127.0.0.1", port);
    }

    void TearDown() override
    {
        server.stop();
        if (worker.joinable())
        {
            worker.join();
        }
    }
};

const std::string GPX =
    R"(<gpx creator="test"><trk><name>Test &amp; Track</name>
    <trkseg><trkpt lat="38" lon="23"><ele>100</ele><time>2026-03-03T09:00:00Z</time></trkpt>
    <trkpt lat="38.001" lon="23"><ele>110</ele><time>2026-03-03T09:01:00Z</time></trkpt></trkseg>
    <trkseg><trkpt lat="40" lon="25"/></trkseg></trk>
    <trk><trkseg><trkpt lat="0" lon="0"/></trkseg></trk></gpx>)";

TEST_F(ViewerTest, ServesFrontend)
{
    auto result = client->Get("/");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);
    EXPECT_NE(result->body.find("qGPX test"), std::string::npos);
    EXPECT_EQ(result->get_header_value("Referrer-Policy"), "strict-origin-when-cross-origin");
}

TEST_F(ViewerTest, AnalyzesTracksWithoutConnectingSegments)
{
    auto result = client->Post("/api/analyze", GPX, "application/gpx+xml");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->status, 200);
    const auto body = nlohmann::json::parse(result->body);
    EXPECT_EQ(body["schemaVersion"], 1);
    EXPECT_EQ(body["creator"], "test");
    ASSERT_EQ(body["tracks"].size(), 2);
    const auto& track = body["tracks"][0];
    EXPECT_EQ(track["name"], "Test & Track");
    EXPECT_EQ(track["pointCount"], 3);
    EXPECT_EQ(track["elevationGainMeters"], 10);
    EXPECT_EQ(track["durationSeconds"], 60);
    ASSERT_EQ(track["segments"].size(), 2);
    const auto& first = track["segments"][0]["points"][0];
    EXPECT_EQ(first["latitude"], 38);
    EXPECT_EQ(first["longitude"], 23);
    EXPECT_EQ(first["elevationMeters"], 100);
    EXPECT_TRUE(first["speedMetersPerSecond"].is_null());
    EXPECT_EQ(first["timeUnixMs"], 1772528400000LL);
    EXPECT_EQ(track["segments"][0]["durationSeconds"], 60);
    const auto distance = track["distanceMeters"].get<double>();
    EXPECT_GT(distance, 110);
    EXPECT_LT(distance, 112);
    EXPECT_NEAR(track["maxSpeedMetersPerSecond"].get<double>(), distance / 60, 1e-10);
    EXPECT_EQ(track["segments"][0]["maxSpeedMetersPerSecond"], track["maxSpeedMetersPerSecond"]);
    EXPECT_EQ(track["segments"][0]["points"][1]["speedMetersPerSecond"],
              track["maxSpeedMetersPerSecond"]);
    const auto& point = track["segments"][1]["points"][0];
    EXPECT_DOUBLE_EQ(point["distanceMeters"].get<double>(), distance);
    EXPECT_TRUE(point["elevationMeters"].is_null());
    EXPECT_TRUE(point["timeUnixMs"].is_null());
    EXPECT_TRUE(point["speedMetersPerSecond"].is_null());
    EXPECT_TRUE(track["segments"][1]["maxSpeedMetersPerSecond"].is_null());
    EXPECT_TRUE(body["tracks"][1]["maxSpeedMetersPerSecond"].is_null());
    EXPECT_EQ(body["tracks"][1]["distanceMeters"], 0);
    EXPECT_TRUE(body["tracks"][1]["durationSeconds"].is_null());
}

TEST_F(ViewerTest, ReportsMalformedAndEmptyUploads)
{
    for (const auto& xml :
         {"", "<gpx>", "<gpx/>", "<gpx><trk><trkseg><trkpt lat=\"bad\"/></trkseg></trk></gpx>"})
    {
        auto result = client->Post("/api/analyze", xml, "application/gpx+xml");
        ASSERT_TRUE(result);
        EXPECT_EQ(result->status, 422);
        EXPECT_FALSE(nlohmann::json::parse(result->body)["error"].get<std::string>().empty());
    }
}

TEST_F(ViewerTest, RejectsUnsupportedContentType)
{
    auto result = client->Post("/api/analyze", GPX, "text/plain");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 415);
}

TEST_F(ViewerTest, ReportsNonFiniteAnalytics)
{
    const std::string xml =
        R"(<gpx><trk><trkseg><trkpt lat="0" lon="0"><ele>-1e308</ele></trkpt>
        <trkpt lat="0" lon="1"><ele>1e308</ele></trkpt></trkseg></trk></gpx>)";
    auto result = client->Post("/api/analyze", xml, "application/gpx+xml");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 422);
    EXPECT_NE(nlohmann::json::parse(result->body)["error"].get<std::string>().find("non-finite"),
              std::string::npos);
}

TEST_F(ViewerTest, RejectsCrossOriginAndForeignHost)
{
    auto result = client->Post("/api/analyze", {{"Origin", "https://example.com"}}, GPX,
                               "application/gpx+xml");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 403);
    result = client->Get("/", httplib::Headers{{"Host", "example.com"}});
    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 403);
}

TEST_F(ViewerTest, EnforcesExactUploadLimit)
{
    std::string xml = GPX;
    xml.append(qGpx::viewer::maxUploadBytes - xml.size(), ' ');
    auto result = client->Post("/api/analyze", xml, "application/gpx+xml");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);
    xml.push_back(' ');
    result = client->Post("/api/analyze", xml, "application/gpx+xml");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 413);
}

TEST_F(ViewerTest, ReportsMissingEndpoints)
{
    auto result = client->Get("/api/missing");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 404);
    EXPECT_TRUE(nlohmann::json::parse(result->body).contains("error"));
}

} // namespace
