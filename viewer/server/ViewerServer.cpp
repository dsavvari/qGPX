// SPDX-License-Identifier: MIT
#include "ViewerServer.hpp"

#include "GPXAnalytics.hpp"
#include "GPXParser.hpp"
#include "GPXWriter.hpp"

#include <json.hpp>

#include <chrono>
#include <cmath>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace qGpx
{
namespace viewer
{
namespace
{

using nlohmann::json;

void error(httplib::Response& response, int status, const std::string& message)
{
    response.status = status;
    response.set_content(json{{"error", message}}.dump(), "application/json");
}

std::optional<core::GPXData> parseUpload(const httplib::Request& request,
                                         httplib::Response& response)
{
    if (request.get_header_value("Content-Type") != "application/gpx+xml")
    {
        error(response, 415, "Send GPX XML with Content-Type: application/gpx+xml");
        return std::nullopt;
    }
    core::GPXParser parser;
    auto result = parser.parse(request.body);
    if (!result.success)
    {
        error(response, 422, result.errorMessage);
        return std::nullopt;
    }
    return std::move(result.value);
}

double finite(double value)
{
    if (!std::isfinite(value))
    {
        throw std::domain_error("Track analysis produced a non-finite measurement");
    }
    return value;
}

json elevation(const std::optional<double>& value)
{
    return value ? json(finite(*value)) : json(nullptr);
}

json serialize(core::GPXData& data)
{
    core::GPXAnalytics::analyseGPXData(data);
    json tracks = json::array();
    for (const auto& track : data.tracks)
    {
        json segments = json::array();
        double distance = 0.0;
        for (const auto& segment : track.segments)
        {
            json points = json::array();
            for (std::size_t i = 0; i < segment.points.size(); ++i)
            {
                const auto& point = segment.points[i];
                if (i > 0)
                {
                    distance += core::GPXAnalytics::calculateDistance(segment.points[i - 1], point);
                }
                json time = nullptr;
                if (point.time)
                {
                    time = std::chrono::duration_cast<std::chrono::milliseconds>(
                               point.time->time_since_epoch())
                               .count();
                }
                points.push_back(
                    {{"latitude", point.latitude},
                     {"longitude", point.longitude},
                     {"elevationMeters", elevation(point.elevation)},
                     {"timeUnixMs", time},
                     {"speedMetersPerSecond", elevation(i > 0 ? core::GPXAnalytics::calculateSpeed(
                                                                    segment.points[i - 1], point)
                                                              : std::nullopt)},
                     {"distanceMeters", finite(distance)}});
            }
            const auto duration = core::GPXAnalytics::calculateDuration(segment);
            segments.push_back(
                {{"points", points},
                 {"distanceMeters", finite(*segment.totalDistance)},
                 {"elevationGainMeters", elevation(segment.elevationGain)},
                 {"elevationLossMeters", elevation(segment.elevationLoss)},
                 {"maxSpeedMetersPerSecond",
                  elevation(core::GPXAnalytics::calculateMaxSpeed(segment))},
                 {"durationSeconds", duration ? json(duration->count()) : json(nullptr)}});
        }
        tracks.push_back(
            {{"name", track.name},
             {"description", track.description},
             {"pointCount", track.totalPointCount()},
             {"distanceMeters", finite(*track.totalDistance)},
             {"elevationGainMeters", elevation(track.elevationGain)},
             {"elevationLossMeters", elevation(track.elevationLoss)},
             {"maxSpeedMetersPerSecond", elevation(core::GPXAnalytics::calculateMaxSpeed(track))},
             {"durationSeconds", track.duration ? json(track.duration->count()) : json(nullptr)},
             {"segments", segments}});
    }
    return {{"schemaVersion", 1}, {"creator", data.creator}, {"tracks", tracks}};
}

} // namespace

bool configureServer(httplib::Server& server, const std::string& assetDirectory)
{
    if (!server.set_mount_point("/", assetDirectory))
    {
        return false;
    }
    server.set_payload_max_length(MaxUploadBytes);
    server.set_read_timeout(10, 0);
    server.set_write_timeout(30, 0);
    server.new_task_queue = [] { return new httplib::ThreadPool(2); };
    server.set_pre_routing_handler(
        [](const httplib::Request& request, httplib::Response& response)
        {
            const auto host = request.get_header_value("Host");
            const auto port = ":" + std::to_string(request.local_port);
            const bool defaultPort =
                request.local_port == 80 && (host == "127.0.0.1" || host == "localhost");
            if (!defaultPort && host != "127.0.0.1" + port && host != "localhost" + port)
            {
                error(response, 403, "Use the viewer's localhost URL");
                return httplib::Server::HandlerResponse::Handled;
            }
            if (request.has_header("Origin") &&
                request.get_header_value("Origin") != "http://" + host)
            {
                error(response, 403, "Cross-origin requests are not allowed");
                return httplib::Server::HandlerResponse::Handled;
            }
            return httplib::Server::HandlerResponse::Unhandled;
        });
    server.set_post_routing_handler(
        [](const httplib::Request&, httplib::Response& response)
        {
            response.set_header("X-Content-Type-Options", "nosniff");
            response.set_header("Cache-Control", "no-store");
            response.set_header("Referrer-Policy", "strict-origin-when-cross-origin");
        });
    server.set_exception_handler(
        [](const httplib::Request&, httplib::Response& response, std::exception_ptr exception)
        {
            try
            {
                std::rethrow_exception(exception);
            }
            catch (const std::exception& cause)
            {
                std::cerr << "Viewer request failed: " << cause.what() << '\n';
            }
            error(response, 500, "Internal viewer error; see server output");
        });
    server.set_error_handler(
        [](const httplib::Request&, httplib::Response& response)
        {
            if (response.body.empty())
            {
                error(response, response.status,
                      response.status == 413
                          ? "GPX upload exceeds the 10 MiB limit"
                          : "Request failed (HTTP " + std::to_string(response.status) + ")");
            }
        });
    server.Post("/api/analyse",
                [](const httplib::Request& request, httplib::Response& response)
                {
                    auto data = parseUpload(request, response);
                    if (!data)
                    {
                        return;
                    }
                    try
                    {
                        response.set_content(serialize(*data).dump(), "application/json");
                    }
                    catch (const std::domain_error& cause)
                    {
                        error(response, 422, cause.what());
                    }
                });
    server.Post("/api/export",
                [](const httplib::Request& request, httplib::Response& response)
                {
                    auto data = parseUpload(request, response);
                    if (!data)
                    {
                        return;
                    }
                    data->creator = "qGPX Track lab";
                    const auto result = core::GPXWriter{}.serialize(*data);
                    if (!result.success)
                    {
                        error(response, 422, result.errorMessage);
                        return;
                    }
                    response.set_header("Content-Disposition",
                                        "attachment; filename=\"track.gpx\"");
                    response.set_content(*result.value, "application/gpx+xml");
                });
    return true;
}

} // namespace viewer
} // namespace qGpx
