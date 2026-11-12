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
    GPXDataHandle gpxParse(const char* xmlContent, char** outError);

    /// Load a GPX file from an accessible local path.
    /// Returns NULL on failure; outError receives a caller-owned error (free with gpxStringFree).
    GPXDataHandle gpxParseFile(const char* filePath, char** outError);

    /// Serialize modeled fields as UTF-8 GPX 1.1 (not a lossless copy).
    /// Returns a caller-owned string (free with gpxStringFree), or NULL on failure.
    /// outError receives a caller-owned error on failure, or NULL on success.
    char* gpxSerialize(GPXDataHandle handle, char** outError);

    /// Validate and overwrite a local GPX file; false on failure, true on success.
    /// Not atomic: write failure can leave a partial file.
    /// outError receives a caller-owned error (free with gpxStringFree), or NULL on success.
    bool gpxSaveFile(GPXDataHandle handle, const char* filePath, char** outError);

    /// Free strings returned by this API, including parser errors. NULL is safe.
    void gpxStringFree(char* text);

    /// Free GPXData handle and associated memory
    void gpxDataFree(GPXDataHandle handle);

    // ============================================================================
    // MARK: - Data Access Functions
    // ============================================================================

    /// Get number of tracks in GPX data
    uint32_t gpxDataGetTrackCount(GPXDataHandle handle);

    /// Get track at index
    /// @return GPXTrackHandle (borrowed reference, do not free)
    GPXTrackHandle gpxDataGetTrack(GPXDataHandle handle, uint32_t index);

    /// Get creator string from GPX data
    /// @return String (borrowed reference, valid until creator is changed or handle is freed)
    const char* gpxDataGetCreator(GPXDataHandle handle);

    /// Copy the exporting software's name into GPX data; caller retains ownership of creator.
    /// Empty string selects the writer's "qGPX" fallback; XML text is validated on serialization.
    /// Returns false for NULL handle/creator, leaving data unchanged.
    /// outError receives a caller-owned error (free with gpxStringFree), or NULL on success.
    bool gpxDataSetCreator(GPXDataHandle handle, const char* creator, char** outError);

    // ---- Track functions ----

    /// Get track name
    const char* gpxTrackGetName(GPXTrackHandle handle);

    /// Get track description
    const char* gpxTrackGetDescription(GPXTrackHandle handle);

    /// Get number of segments in track
    uint32_t gpxTrackGetSegmentCount(GPXTrackHandle handle);

    /// Get segment at index
    GPXSegmentHandle gpxTrackGetSegment(GPXTrackHandle handle, uint32_t index);

    /// Get track's total distance (in meters), or -1 if not calculated
    double gpxTrackGetTotalDistance(GPXTrackHandle handle);

    /// Get track's elevation gain (in meters), or -1 if not calculated
    double gpxTrackGetElevationGain(GPXTrackHandle handle);

    /// Get track's elevation loss (in meters), or -1 if not calculated
    double gpxTrackGetElevationLoss(GPXTrackHandle handle);

    /// Get track's duration (in seconds), or -1 if not calculated
    int64_t gpxTrackGetDuration(GPXTrackHandle handle);

    /// Calculate maximum adjacent-point speed in m/s, or -1 if unavailable/invalid handle.
    double gpxTrackGetMaxSpeed(GPXTrackHandle handle);

    // ---- Segment functions ----

    /// Get number of points in segment
    uint32_t gpxSegmentGetPointCount(GPXSegmentHandle handle);

    /// Calculate maximum adjacent-point speed in m/s, or -1 if unavailable/invalid handle.
    double gpxSegmentGetMaxSpeed(GPXSegmentHandle handle);

    /// Speed in m/s from the preceding point; -1 for index 0, invalid input,
    /// missing/non-increasing timestamps, or non-finite measurements.
    double gpxSegmentGetPointSpeed(GPXSegmentHandle handle, uint32_t index);

    /// Get point coordinates at index
    /// @param outLat Pointer to receive latitude
    /// @param outLon Pointer to receive longitude
    /// @param outEle Pointer to receive elevation (or NaN if not available)
    void gpxSegmentGetPoint(GPXSegmentHandle handle, uint32_t index, double* outLat, double* outLon,
                            double* outEle);

    // ============================================================================
    // MARK: - Analytics Functions
    // ============================================================================

    /// Analyse GPX data and populate computed properties (distance, elevation, duration)
    void gpxAnalyse(GPXDataHandle handle);

    /// Calculate distance between two points (Haversine formula)
    /// @return Distance in meters
    double gpxCalculateDistance(double lat1, double lon1, double lat2, double lon2);

#ifdef __cplusplus
}
#endif
