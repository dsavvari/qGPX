// GPXCore.cpp
// Implementation of core GPX functions
//
// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#include "GPXCore.h"

#include "GPXAnalytics.hpp"
#include "GPXModels.hpp"
#include "GPXParser.hpp"

#include <cmath>
#include <cstring>

using namespace qGpx::core;

// ============================================================================
// MARK: - Internal Helpers
// ============================================================================

static char* copyString(const std::string& str)
{
    char* result = new char[str.length() + 1];
    std::strcpy(result, str.c_str());
    return result;
}

// ============================================================================
// MARK: - Parser Functions
// ============================================================================

GPXDataHandle gpx_parse(const char* xmlContent, char** outError)
{
    if (!xmlContent)
    {
        if (outError)
        {
            *outError = copyString("XML content is null");
        }
        return nullptr;
    }

    GPXParser parser;
    auto result = parser.parse(std::string(xmlContent));

    if (!result.success)
    {
        if (outError)
        {
            *outError = copyString(result.errorMessage);
        }
        return nullptr;
    }

    if (outError)
    {
        *outError = nullptr;
    }

    return reinterpret_cast<GPXDataHandle>(new GPXData(std::move(result.value.value())));
}

void gpx_data_free(GPXDataHandle handle)
{
    if (handle)
    {
        delete reinterpret_cast<GPXData*>(handle);
    }
}

// ============================================================================
// MARK: - Data Access Functions
// ============================================================================

uint32_t gpx_data_get_track_count(GPXDataHandle handle)
{
    if (!handle)
        return 0;
    auto* data = reinterpret_cast<GPXData*>(handle);
    return static_cast<uint32_t>(data->tracks.size());
}

GPXTrackHandle gpx_data_get_track(GPXDataHandle handle, uint32_t index)
{
    if (!handle)
        return nullptr;
    auto* data = reinterpret_cast<GPXData*>(handle);
    if (index >= data->tracks.size())
        return nullptr;
    return reinterpret_cast<GPXTrackHandle>(&data->tracks[index]);
}

const char* gpx_data_get_creator(GPXDataHandle handle)
{
    if (!handle)
        return "";
    auto* data = reinterpret_cast<GPXData*>(handle);
    return data->creator.c_str();
}

// ---- Track functions ----

const char* gpx_track_get_name(GPXTrackHandle handle)
{
    if (!handle)
        return "";
    auto* track = reinterpret_cast<Track*>(handle);
    return track->name.c_str();
}

const char* gpx_track_get_description(GPXTrackHandle handle)
{
    if (!handle)
        return "";
    auto* track = reinterpret_cast<Track*>(handle);
    return track->description.c_str();
}

uint32_t gpx_track_get_segment_count(GPXTrackHandle handle)
{
    if (!handle)
        return 0;
    auto* track = reinterpret_cast<Track*>(handle);
    return static_cast<uint32_t>(track->segments.size());
}

GPXSegmentHandle gpx_track_get_segment(GPXTrackHandle handle, uint32_t index)
{
    if (!handle)
        return nullptr;
    auto* track = reinterpret_cast<Track*>(handle);
    if (index >= track->segments.size())
        return nullptr;
    return reinterpret_cast<GPXSegmentHandle>(&track->segments[index]);
}

double gpx_track_get_total_distance(GPXTrackHandle handle)
{
    if (!handle)
        return -1.0;
    auto* track = reinterpret_cast<Track*>(handle);
    return track->totalDistance.value_or(-1.0);
}

double gpx_track_get_elevation_gain(GPXTrackHandle handle)
{
    if (!handle)
        return -1.0;
    auto* track = reinterpret_cast<Track*>(handle);
    return track->elevationGain.value_or(-1.0);
}

double gpx_track_get_elevation_loss(GPXTrackHandle handle)
{
    if (!handle)
        return -1.0;
    auto* track = reinterpret_cast<Track*>(handle);
    return track->elevationLoss.value_or(-1.0);
}

int64_t gpx_track_get_duration(GPXTrackHandle handle)
{
    if (!handle)
        return -1;
    auto* track = reinterpret_cast<Track*>(handle);
    if (!track->duration.has_value())
        return -1;
    return track->duration.value().count();
}

// ---- Segment functions ----

uint32_t gpx_segment_get_point_count(GPXSegmentHandle handle)
{
    if (!handle)
        return 0;
    auto* segment = reinterpret_cast<TrackSegment*>(handle);
    return static_cast<uint32_t>(segment->points.size());
}

void gpx_segment_get_point(GPXSegmentHandle handle, uint32_t index, double* outLat, double* outLon,
                           double* outEle)
{
    if (!handle || !outLat || !outLon || !outEle)
        return;

    auto* segment = reinterpret_cast<TrackSegment*>(handle);
    if (index >= segment->points.size())
        return;

    const auto& pt = segment->points[index];
    *outLat = pt.latitude;
    *outLon = pt.longitude;
    *outEle = pt.elevation.value_or(NAN);
}

// ============================================================================
// MARK: - Analytics Functions
// ============================================================================

void gpx_analyze(GPXDataHandle handle)
{
    if (!handle)
        return;
    auto* data = reinterpret_cast<GPXData*>(handle);
    GPXAnalytics::analyzeGPXData(*data);
}

double gpx_calculate_distance(double lat1, double lon1, double lat2, double lon2)
{
    TrackPoint p1(lat1, lon1);
    TrackPoint p2(lat2, lon2);
    return GPXAnalytics::calculateDistance(p1, p2);
}
