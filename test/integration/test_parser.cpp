// test_parser.cpp
// Integration test program for the GPX parser
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
        std::cerr << "Usage: " << argv[0] << " <gpx_file> [sample_points_count=3]\n";

        return 1;
    }

    std::string filePath = argv[1];

    size_t samplePointsCount = 3;
    if (argc >= 3)
    {
        samplePointsCount = std::stoul(argv[2]);
    }
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
    for (const auto& track : gpxData.tracks)
    {
        std::cout << "Track: " << track.name << std::endl;
        std::cout << "  Segments: " << track.segmentCount() << std::endl;
        std::cout << "  Total Points: " << track.totalPointCount() << std::endl;

        for (const auto& seg : track.segments)
        {
            std::cout << "  Segment: " << seg.pointCount() << " points" << std::endl;

            // Print first few points as sample
            size_t sampleCount = std::min(samplePointsCount, seg.points.size());

            auto k = size_t(0);
            for (const auto& pt : seg.points)
            {
                k++;
                if (k > sampleCount)
                    break;
                std::cout << "    Point " << k << ": ";
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
