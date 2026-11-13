// GPXCore.cpp
// Implementation of core GPX functions
//
// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#include "GPXCore.h"

#include "GPXAnalytics.hpp"
#include "GPXModels.hpp"
#include "GPXParser.hpp"
#include "GPXWriter.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>

using namespace qGpx::core;

// ============================================================================
// MARK: - Internal Helpers
// ============================================================================

static char* copyString(const std::string& str)
{
    const size_t length = str.length() + 1;
    char* result = static_cast<char*>(std::malloc(length));
    if (!result)
    {
        return nullptr;
    }
    std::memcpy(result, str.c_str(), length);
    return result;
}

// ============================================================================
// MARK: - Parser Functions
// ============================================================================

GPXDataHandle gpxParse(const char* xmlContent, char** outError)
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

void gpxDataFree(GPXDataHandle handle)
{
    if (handle)
    {
        delete reinterpret_cast<GPXData*>(handle);
    }
}

GPXDataHandle gpxParseFile(const char* filePath, char** outError)
{
    GPXParser parser;
    auto result =
        filePath ? parser.parseFile(filePath) : Result<GPXData>::makeFailure("File path is null");
    if (outError)
    {
        *outError = result.success ? nullptr : copyString(result.errorMessage);
    }
    if (!result.success)
    {
        return nullptr;
    }
    return reinterpret_cast<GPXDataHandle>(new GPXData(std::move(*result.value)));
}

char* gpxSerialize(GPXDataHandle handle, char** outError)
{
    GPXWriter writer;
    const auto result = handle ? writer.serialize(*reinterpret_cast<GPXData*>(handle))
                               : Result<std::string>::makeFailure("GPX data handle is null");
    char* xml = result.success ? copyString(*result.value) : nullptr;
    if (outError)
    {
        *outError = !result.success ? copyString(result.errorMessage)
                    : !xml          ? copyString("Failed to allocate serialized GPX string")
                                    : nullptr;
    }
    return xml;
}

bool gpxSaveFile(GPXDataHandle handle, const char* filePath, char** outError)
{
    GPXWriter writer;
    const auto result = !handle ? Result<bool>::makeFailure("GPX data handle is null")
                        : !filePath
                            ? Result<bool>::makeFailure("File path is null")
                            : writer.saveFile(*reinterpret_cast<GPXData*>(handle), filePath);
    if (outError)
    {
        *outError = result.success ? nullptr : copyString(result.errorMessage);
    }
    return result.success;
}

void gpxStringFree(char* text)
{
    std::free(text);
}

// ============================================================================
// MARK: - Data Access Functions
// ============================================================================

uint32_t gpxDataGetTrackCount(GPXDataHandle handle)
{
    if (!handle)
        return 0;
    auto* data = reinterpret_cast<GPXData*>(handle);
    return static_cast<uint32_t>(data->tracks.size());
}

GPXTrackHandle gpxDataGetTrack(GPXDataHandle handle, uint32_t index)
{
    if (!handle)
        return nullptr;
    auto* data = reinterpret_cast<GPXData*>(handle);
    if (index >= data->tracks.size())
        return nullptr;
    return reinterpret_cast<GPXTrackHandle>(&data->tracks[index]);
}

const char* gpxDataGetCreator(GPXDataHandle handle)
{
    if (!handle)
        return "";
    auto* data = reinterpret_cast<GPXData*>(handle);
    return data->creator.c_str();
}

bool gpxDataSetCreator(GPXDataHandle handle, const char* creator, char** outError)
{
    if (!handle || !creator)
    {
        if (outError)
        {
            *outError = copyString(!handle ? "GPX data handle is null" : "Creator is null");
        }
        return false;
    }
    auto* data = reinterpret_cast<GPXData*>(handle);
    data->creator = creator;
    if (outError)
    {
        *outError = nullptr;
    }
    return true;
}

// ---- Track functions ----

const char* gpxTrackGetName(GPXTrackHandle handle)
{
    if (!handle)
        return "";
    auto* track = reinterpret_cast<Track*>(handle);
    return track->name.c_str();
}

const char* gpxTrackGetDescription(GPXTrackHandle handle)
{
    if (!handle)
        return "";
    auto* track = reinterpret_cast<Track*>(handle);
    return track->description.c_str();
}

uint32_t gpxTrackGetSegmentCount(GPXTrackHandle handle)
{
    if (!handle)
        return 0;
    auto* track = reinterpret_cast<Track*>(handle);
    return static_cast<uint32_t>(track->segments.size());
}

GPXSegmentHandle gpxTrackGetSegment(GPXTrackHandle handle, uint32_t index)
{
    if (!handle)
        return nullptr;
    auto* track = reinterpret_cast<Track*>(handle);
    if (index >= track->segments.size())
        return nullptr;
    return reinterpret_cast<GPXSegmentHandle>(&track->segments[index]);
}

double gpxTrackGetTotalDistance(GPXTrackHandle handle)
{
    if (!handle)
        return -1.0;
    auto* track = reinterpret_cast<Track*>(handle);
    return track->totalDistance.value_or(-1.0);
}

double gpxTrackGetElevationGain(GPXTrackHandle handle)
{
    if (!handle)
        return -1.0;
    auto* track = reinterpret_cast<Track*>(handle);
    return track->elevationGain.value_or(-1.0);
}

double gpxTrackGetElevationLoss(GPXTrackHandle handle)
{
    if (!handle)
        return -1.0;
    auto* track = reinterpret_cast<Track*>(handle);
    return track->elevationLoss.value_or(-1.0);
}

int64_t gpxTrackGetDuration(GPXTrackHandle handle)
{
    if (!handle)
        return -1;
    auto* track = reinterpret_cast<Track*>(handle);
    if (!track->duration.has_value())
        return -1;
    return track->duration.value().count();
}

// ---- Segment functions ----

double gpxTrackGetMaxSpeed(GPXTrackHandle handle)
{
    if (!handle)
        return -1.0;
    return GPXAnalytics::calculateMaxSpeed(*reinterpret_cast<Track*>(handle)).value_or(-1.0);
}

double gpxSegmentGetMaxSpeed(GPXSegmentHandle handle)
{
    if (!handle)
        return -1.0;
    return GPXAnalytics::calculateMaxSpeed(*reinterpret_cast<TrackSegment*>(handle)).value_or(-1.0);
}

double gpxSegmentGetPointSpeed(GPXSegmentHandle handle, uint32_t index)
{
    if (!handle)
        return -1.0;
    const auto& points = reinterpret_cast<TrackSegment*>(handle)->points;
    if (index == 0 || index >= points.size())
        return -1.0;
    return GPXAnalytics::calculateSpeed(points[index - 1], points[index]).value_or(-1.0);
}

bool gpxSegmentGetPointHeartRate(GPXSegmentHandle handle, uint32_t index, uint8_t* outBpm)
{
    if (!handle || !outBpm)
        return false;
    const auto& points = reinterpret_cast<TrackSegment*>(handle)->points;
    if (index >= points.size() || !points[index].heartRateBpm)
        return false;
    *outBpm = *points[index].heartRateBpm;
    return true;
}

bool gpxSegmentGetPointCadence(GPXSegmentHandle handle, uint32_t index, uint8_t* outRpm)
{
    if (!handle || !outRpm)
        return false;
    const auto& points = reinterpret_cast<TrackSegment*>(handle)->points;
    if (index >= points.size() || !points[index].cadenceRpm)
        return false;
    *outRpm = *points[index].cadenceRpm;
    return true;
}

bool gpxSegmentGetPointTemperature(GPXSegmentHandle handle, uint32_t index, double* outCelsius)
{
    if (!handle || !outCelsius)
        return false;
    const auto& points = reinterpret_cast<TrackSegment*>(handle)->points;
    if (index >= points.size() || !points[index].temperatureCelsius)
        return false;
    *outCelsius = *points[index].temperatureCelsius;
    return true;
}

uint32_t gpxSegmentGetPointCount(GPXSegmentHandle handle)
{
    if (!handle)
        return 0;
    auto* segment = reinterpret_cast<TrackSegment*>(handle);
    return static_cast<uint32_t>(segment->points.size());
}

void gpxSegmentGetPoint(GPXSegmentHandle handle, uint32_t index, double* outLat, double* outLon,
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

void gpxAnalyse(GPXDataHandle handle)
{
    if (!handle)
        return;
    auto* data = reinterpret_cast<GPXData*>(handle);
    GPXAnalytics::analyseGPXData(*data);
}

double gpxCalculateDistance(double lat1, double lon1, double lat2, double lon2)
{
    TrackPoint p1(lat1, lon1);
    TrackPoint p2(lat2, lon2);
    return GPXAnalytics::calculateDistance(p1, p2);
}
