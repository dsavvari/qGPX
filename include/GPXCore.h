// GPXCore.h
// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Opaque handle types for C++ objects
    typedef void* GPXDataHandle;
    typedef void* GPXTrackHandle;
    typedef void* GPXSegmentHandle;

    // ============================================================================
    // MARK: - Parser Functions
    // ============================================================================

    /// Parse GPX XML string and return handle to GPXData
    /// @param xmlContent Null-terminated XML string
    /// @param outError Pointer to receive error message (caller must free), or NULL if no error
    /// @return GPXDataHandle on success, NULL on failure
    GPXDataHandle gpx_parse(const char* xmlContent, char** outError);

    /// Free GPXData handle and associated memory
    void gpx_data_free(GPXDataHandle handle);

    // ============================================================================
    // MARK: - Data Access Functions
    // ============================================================================

    /// Get number of tracks in GPX data
    uint32_t gpx_data_get_track_count(GPXDataHandle handle);

    /// Get track at index
    /// @return GPXTrackHandle (borrowed reference, do not free)
    GPXTrackHandle gpx_data_get_track(GPXDataHandle handle, uint32_t index);

    /// Get creator string from GPX data
    /// @return String (borrowed reference, valid until handle is freed)
    const char* gpx_data_get_creator(GPXDataHandle handle);

    // ---- Track functions ----

    /// Get track name
    const char* gpx_track_get_name(GPXTrackHandle handle);

    /// Get track description
    const char* gpx_track_get_description(GPXTrackHandle handle);

    /// Get number of segments in track
    uint32_t gpx_track_get_segment_count(GPXTrackHandle handle);

    /// Get segment at index
    GPXSegmentHandle gpx_track_get_segment(GPXTrackHandle handle, uint32_t index);

    /// Get track's total distance (in meters), or -1 if not calculated
    double gpx_track_get_total_distance(GPXTrackHandle handle);

    /// Get track's elevation gain (in meters), or -1 if not calculated
    double gpx_track_get_elevation_gain(GPXTrackHandle handle);

    /// Get track's elevation loss (in meters), or -1 if not calculated
    double gpx_track_get_elevation_loss(GPXTrackHandle handle);

    /// Get track's duration (in seconds), or -1 if not calculated
    int64_t gpx_track_get_duration(GPXTrackHandle handle);

    /// Calculate maximum adjacent-point speed in m/s, or -1 if unavailable/invalid handle.
    double gpx_track_get_max_speed(GPXTrackHandle handle);

    // ---- Segment functions ----

    /// Get number of points in segment
    uint32_t gpx_segment_get_point_count(GPXSegmentHandle handle);

    /// Calculate maximum adjacent-point speed in m/s, or -1 if unavailable/invalid handle.
    double gpx_segment_get_max_speed(GPXSegmentHandle handle);

    /// Speed in m/s from the preceding point; -1 for index 0, invalid input,
    /// missing/non-increasing timestamps, or non-finite measurements.
    double gpx_segment_get_point_speed(GPXSegmentHandle handle, uint32_t index);

    /// Get point coordinates at index
    /// @param outLat Pointer to receive latitude
    /// @param outLon Pointer to receive longitude
    /// @param outEle Pointer to receive elevation (or NaN if not available)
    void gpx_segment_get_point(GPXSegmentHandle handle, uint32_t index, double* outLat,
                               double* outLon, double* outEle);

    // ============================================================================
    // MARK: - Analytics Functions
    // ============================================================================

    /// Analyze GPX data and populate computed properties (distance, elevation, duration)
    void gpx_analyze(GPXDataHandle handle);

    /// Calculate distance between two points (Haversine formula)
    /// @return Distance in meters
    double gpx_calculate_distance(double lat1, double lon1, double lat2, double lon2);

#ifdef __cplusplus
}
#endif
