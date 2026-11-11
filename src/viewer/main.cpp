// SPDX-License-Identifier: MIT
#include "ViewerServer.hpp"

#include <charconv>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    int port = 8080;
    if (argc > 3)
    {
        std::cerr << "Usage: qgpx-viewer [port] [frontend-dist-directory]\n";
        return 1;
    }
    if (argc >= 2)
    {
        const std::string argument = argv[1];
        const auto result =
            std::from_chars(argument.data(), argument.data() + argument.size(), port);
        if (result.ec != std::errc() || result.ptr != argument.data() + argument.size() ||
            port < 1 || port > 65535)
        {
            std::cerr << "Port must be an integer between 1 and 65535\n";
            return 1;
        }
    }
    const std::string assets = argc == 3 ? argv[2] : QGPX_VIEWER_ASSET_DIR;
    if (!std::filesystem::is_regular_file(std::filesystem::path(assets) / "index.html"))
    {
        std::cerr << "Frontend not found in " << assets
                  << ". Run npm ci && npm run build in viewer/, or supply its dist directory.\n";
        return 1;
    }
    httplib::Server server;
    if (!qGpx::viewer::configureServer(server, assets))
    {
        std::cerr << "Cannot serve frontend directory: " << assets << '\n';
        return 1;
    }
    if (!server.bind_to_port("127.0.0.1", port))
    {
        std::cerr << "Cannot bind to 127.0.0.1:" << port << '\n';
        return 1;
    }
    std::cout << "qGPX viewer: http://127.0.0.1:" << port << " (Ctrl-C to stop)\n" << std::flush;
    if (!server.listen_after_bind())
    {
        std::cerr << "Viewer server stopped unexpectedly\n";
        return 1;
    }
    return 0;
}
