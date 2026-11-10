// test_parser.cpp
// Simple test program for GPX parser
//
// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#include "GPXParser.hpp"

#include <iostream>
#include <iomanip>

using namespace qGpx::core;

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <gpx_file>" << std::endl;
        return 1;
    }

    std::string filePath = argv[1];
    std::cout << "Parsing GPX file: " << filePath << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    GPXParser parser;
    auto result = parser.parseFile(filePath);

    if (!result.success)
    {
        std::cerr << "ERROR: " << result.errorMessage << std::endl;
        return 1;
    }

    const GPXData& gpxData = result.value.value();

    // Print basic info
    std::cout << "Creator: " << gpxData.creator << std::endl;
    std::cout << "Tracks: " << gpxData.trackCount() << std::endl;
    std::cout << std::endl;

    // Print track details
    for (size_t i = 0; i < gpxData.tracks.size(); ++i)
    {
        const Track& track = gpxData.tracks[i];
        std::cout << "Track " << (i + 1) << ": " << track.name << std::endl;
        std::cout << "  Segments: " << track.segmentCount() << std::endl;
        std::cout << "  Total Points: " << track.totalPointCount() << std::endl;

        for (size_t j = 0; j < track.segments.size(); ++j)
        {
            const TrackSegment& seg = track.segments[j];
            std::cout << "  Segment " << (j + 1) << ": " << seg.pointCount() << " points"
                      << std::endl;

            // Print first few points as sample
            size_t sampleCount = std::min(size_t(3), seg.points.size());
            for (size_t k = 0; k < sampleCount; ++k)
            {
                const TrackPoint& pt = seg.points[k];
                std::cout << "    Point " << (k + 1) << ": ";
                std::cout << std::fixed << std::setprecision(6);
                std::cout << "lat=" << pt.latitude << ", lon=" << pt.longitude;

                if (pt.elevation.has_value())
                {
                    std::cout << ", ele=" << std::setprecision(2) << pt.elevation.value() << "m";
                }

                if (pt.time.has_value())
                {
                    std::time_t t = std::chrono::system_clock::to_time_t(pt.time.value());
                    std::cout << ", time=" << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
                }

                std::cout << std::endl;
            }

            if (seg.points.size() > sampleCount)
            {
                std::cout << "    ... (" << (seg.points.size() - sampleCount) << " more points)"
                          << std::endl;
            }
        }
        std::cout << std::endl;
    }

    std::cout << "✓ Parsing completed successfully!" << std::endl;
    return 0;
}
