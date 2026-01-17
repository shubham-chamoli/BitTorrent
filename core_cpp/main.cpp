#include "engine/torrent_engine.h"

#include <iostream>
#include <thread>
#include <chrono>

int main()
{
    try
    {
        // 1. Create Torrent Engine
        TorrentEngine engine(
            "sample.torrent",
            "output_file.iso"
        );

        // 2. Start Engine
        engine.start();
        std::cout << "🚀 Torrent engine started...\n";

        // 3. Monitor Progress
        while (!engine.is_finished())
        {
            std::cout << "📊 Progress: "
                      << engine.get_progress() * 100.0f
                      << "%\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cout << "✅ Download completed successfully!\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fatal Error: " << e.what() << "\n";
    }

    return 0;
}
