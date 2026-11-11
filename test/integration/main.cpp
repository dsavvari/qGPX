// SPDX-License-Identifier: MIT
#include "GPXAnalytics.hpp"
#include "GPXParser.hpp"
#include "GPXCore.h"

#include <cstdlib>
#include <iostream>

int main()
{
    const std::string xml =
        R"(<gpx><trk><trkseg><trkpt lat="38" lon="23"/><trkpt lat="38.001" lon="23"/></trkseg></trk></gpx>)";
    qGpx::core::GPXParser parser;
    auto result = parser.parse(xml);
    if (!result.success)
    {
        std::cerr << result.errorMessage << '\n';
        return 1;
    }
    qGpx::core::GPXAnalytics::analyzeGPXData(*result.value);
    const double distance = *result.value->tracks.front().totalDistance;
    if (distance < 110 || distance > 112)
    {
        std::cerr << "Unexpected C++ distance: " << distance << '\n';
        return 1;
    }
    char* error = nullptr;
    const auto handle = gpx_parse(xml.c_str(), &error);
    if (!handle)
    {
        std::cerr << "C API parse failed: " << (error ? error : "no error message") << '\n';
        std::free(error);
        return 1;
    }
    const auto count = gpx_data_get_track_count(handle);
    gpx_data_free(handle);
    if (count != 1)
    {
        std::cerr << "Unexpected C API track count: " << count << '\n';
        return 1;
    }
    return 0;
}
