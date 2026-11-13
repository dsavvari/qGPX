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
    // MARK: - Movement Statistics Structure
    // ============================================================================

    /// Movement statistics result from analysis
    typedef struct
    {
        /// True if statistics are available (at least one valid interval)
        bool isAvailable;

        double movingDuration;      // Seconds of actual movement (speed >= threshold)
        double stoppedDuration;     // Seconds stopped (speed < threshold)
        double elapsedDuration;     // Total elapsed time from first to last valid timestamp
        double movingDistance;      // Distance traveled during moving periods (meters)
        double stoppedDistance;     // Distance during stopped periods (meters)
        double movingAverageSpeed;  // Moving distance / moving duration (m/s), or 0 if no movement
        double elapsedAverageSpeed; // Total distance / elapsed duration (m/s)

        uint32_t validIntervals;   // Number of valid adjacent-point intervals analyzed
        uint32_t invalidIntervals; // Number of intervals with missing/invalid data
        uint32_t gapIntervals;     // Number of intervals exceeding the gap threshold
    } GPXMovementStatistics;

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

    /// Serialize modeled fields and retained extensions as UTF-8 GPX 1.1 (not a lossless copy).
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

    /// Get track's movement statistics. Returns false if not available/unavailable.
    /// Writes outStats only on success; outStats remains caller-owned.
    bool gpxTrackGetMovementStats(GPXTrackHandle handle, GPXMovementStatistics* outStats);

    // ---- Segment functions ----

    /// Get number of points in segment
    uint32_t gpxSegmentGetPointCount(GPXSegmentHandle handle);

    /// Calculate maximum adjacent-point speed in m/s, or -1 if unavailable/invalid handle.
    double gpxSegmentGetMaxSpeed(GPXSegmentHandle handle);

    /// Get segment's movement statistics. Returns false if not available/unavailable.
    /// Writes outStats only on success; outStats remains caller-owned.
    bool gpxSegmentGetMovementStats(GPXSegmentHandle handle, GPXMovementStatistics* outStats);

    /// Speed in m/s from the preceding point; -1 for index 0, invalid input,
    /// missing/non-increasing timestamps, or non-finite measurements.
    double gpxSegmentGetPointSpeed(GPXSegmentHandle handle, uint32_t index);

    /// Get a point's Garmin heart rate in bpm. Returns false if unavailable or invalid.
    /// Writes outBpm only on success; handles are borrowed and outBpm remains caller-owned.
    bool gpxSegmentGetPointHeartRate(GPXSegmentHandle handle, uint32_t index, uint8_t* outBpm);

    /// Get a point's Garmin cadence in rpm. Returns false if unavailable or invalid.
    /// Writes outRpm only on success; handles are borrowed and outRpm remains caller-owned.
    bool gpxSegmentGetPointCadence(GPXSegmentHandle handle, uint32_t index, uint8_t* outRpm);

    /// Get a point's Garmin temperature in degrees Celsius. Returns false if unavailable or
    /// invalid. Writes outCelsius only on success; handles are borrowed and outCelsius remains
    /// caller-owned.
    bool gpxSegmentGetPointTemperature(GPXSegmentHandle handle, uint32_t index, double* outCelsius);

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
