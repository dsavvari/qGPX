// GPXParser.hpp
// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#pragma once

#include "GPXModels.hpp"

#include <string>

namespace qGpx
{
namespace core
{

/// GPX XML parser using libexpat (SAX-style streaming parser). Recognized GPX tags
/// are matched by namespace URI; extension trees retain expanded names and bindings.
/// Coordinates and elevations accept finite decimal numbers with optional surrounding
/// XML whitespace, but not hexadecimal numbers or scientific notation.
class GPXParser
{
public:
    GPXParser() = default;
    ~GPXParser() = default;

    /// Parse GPX data from XML string
    /// @param xmlContent The GPX XML content as a string
    /// @return Result containing GPXData on success, or error message on failure
    Result<GPXData> parse(const std::string& xmlContent);

    /// Parse GPX data from file path
    /// @param filePath Path to the GPX file
    /// @return Result containing GPXData on success, or error message on failure
    Result<GPXData> parseFile(const std::string& filePath);

    /// Parse ISO 8601 time string (e.g., "2026-03-03T09:51:09.000Z")
    /// Fractional seconds are truncated to nanoseconds, then to system_clock's native precision.
    /// @param timeStr ISO 8601 formatted time string
    /// @return Optional time_point, or nullopt if parsing fails
    static std::optional<std::chrono::system_clock::time_point>
    parseTime(const std::string& timeStr);
};

} // namespace core
} // namespace qGpx
