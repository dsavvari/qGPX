// SPDX-License-Identifier: MIT
#pragma once

#include <httplib.h>

#include <cstddef>
#include <string>

namespace qGpx
{
namespace viewer
{

constexpr std::size_t MaxUploadBytes = 10 * 1024 * 1024;

bool configureServer(httplib::Server& server, const std::string& assetDirectory);

} // namespace viewer
} // namespace qGpx
