// SPDX-License-Identifier: MIT
#pragma once

#include "GPXModels.hpp"

namespace qGpx
{
namespace core
{

/// Writes modeled track fields and retained extensions as UTF-8 GPX 1.1, not a
/// lossless copy of the input.
class GPXWriter
{
public:
    /// Serialize tracks, segments, sensor fields, retained extensions and UTC timestamps.
    /// Numbers use shortest round-trip decimal representations without exponents.
    /// Coordinates are padded to at least six decimal places and elevations to two;
    /// additional significant digits are retained without rounding.
    /// Uses "qGPX" when creator is empty. Rejects invalid XML text or measurements.
    Result<std::string> serialize(const GPXData& data) const;

    /// Validate and then overwrite a local file. Returns failure for open/write/close errors.
    /// Writes are not atomic; a write failure may leave a partial file.
    Result<bool> saveFile(const GPXData& data, const std::string& filePath) const;
};

} // namespace core
} // namespace qGpx
