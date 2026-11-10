// test_elevation.cpp
// Quick test to check elevation calculations
//
// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#include "GPXParser.hpp"
#include "GPXAnalytics.hpp"

#include <iostream>
#include <iomanip>

using namespace visualgpx::core;

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <gpx_file>" << std::endl;
        return 1;
    }

    GPXParser parser;
    auto result = parser.parseFile(argv[1]);

    if (!result.success)
    {
        std::cerr << "ERROR: " << result.errorMessage << std::endl;
        return 1;
    }

    GPXData gpxData = result.value.value();

    // Analyze the data
    for (auto& track : gpxData.tracks)
    {
        GPXAnalytics::analyzeTrack(track);

        std::cout << "Track: " << track.name << std::endl;
        std::cout << "  Distance: " << std::fixed << std::setprecision(2)
                  << track.totalDistance.value() / 1000.0 << " km" << std::endl;
        std::cout << "  Elevation Gain: " << std::setprecision(0) << track.elevationGain.value()
                  << " m" << std::endl;
        std::cout << "  Elevation Loss: " << std::setprecision(0) << track.elevationLoss.value()
                  << " m" << std::endl;

        if (track.duration.has_value())
        {
            int seconds = track.duration.value().count();
            int hours = seconds / 3600;
            int minutes = (seconds % 3600) / 60;
            int secs = seconds % 60;
            std::cout << "  Duration: " << hours << ":" << std::setfill('0') << std::setw(2)
                      << minutes << ":" << std::setw(2) << secs << std::endl;
        }
    }

    return 0;
}
