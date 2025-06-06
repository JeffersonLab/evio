#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <chrono>
#include <cmath>
#include <fcntl.h>   // open()
#include <unistd.h>  // close(), fsync()

#include "eviocc.h"

int main(int argc, char *argv[]) {
    // Default parameters
    std::string outFile = "output.evio";
    unsigned long long fileSize = 100 * 1024 * 1024; // 100 MB default
    size_t bufferSize = 1024;    // event payload size in bytes
    int repeatCount = 1;
    bool doSync = false;
    bool debug = false;
    
    // Parse simple command-line flags
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-o" || arg == "--output") && i+1 < argc) { // i+1 logic ensures argument provided
            outFile = argv[++i];
        } else if ((arg == "-s" || arg == "--size") && i+1 < argc) {
            fileSize = std::stoull(argv[++i]);
        } else if ((arg == "-b" || arg == "--bufsize") && i+1 < argc) {
            bufferSize = std::stoul(argv[++i]);
        } else if ((arg == "-n" || arg == "--repeat") && i+1 < argc) {
            repeatCount = std::stoi(argv[++i]);
        } else if (arg == "--sync") {
            doSync = true;
        } else if (arg == "--debug") {
            debug = true;
        } else {
            std::cerr << "Usage: " << argv[0] 
                      << " -o <file> -s <fileSizeBytes> -b <eventSizeBytes>"
                      << " -n <repeats> [--sync] [--debug]\n";
            return 1;
        }
    }

    // Create dummy data buffer to fill each event
    std::vector<uint8_t> data(bufferSize);
    // Fill with non-zero pattern (optional)
    for (size_t i = 0; i < bufferSize; ++i) {
        data[i] = static_cast<uint8_t>(i & 0xFF);
    }

    // Prepare EVIO event builder for an event with UCHAR8 (8-bit) data
    evio::EventBuilder builder(1, evio::DataType::UCHAR8, 1);
    auto event = builder.getEvent(); 
    // Attach the dummy data to the event
    builder.setUCharData(event, data.data(), data.size());
    builder.setAllHeaderLengths(); // ensure EVIO length fields are up-to-date

    // Calculate how many events to write to reach at least fileSize bytes
    // (We ignore EVIO file header overhead for simplicity)
    uint32_t eventBytes = event->getTotalBytes(); 
    unsigned long long eventsPerRun = 
        static_cast<unsigned long long>(std::ceil(double(fileSize) / eventBytes));
    
    // Loop for the specified repeat count
    double totalThroughput = 0.0;
    for (int r = 0; r < repeatCount; ++r) {
        // Open an EventWriter to write events to the file
        evio::EventWriter writer(outFile, evio::ByteOrder::ENDIAN_LOCAL, false);
        // Start timing
        auto start = std::chrono::high_resolution_clock::now();
        // Write events repeatedly
        for (unsigned long long i = 0; i < eventsPerRun; ++i) {
            writer.writeEvent(event);  // write the dummy event&#8203;:contentReference[oaicite:1]{index=1}
            if (debug) {
                static auto lastTime = start;
                auto now = std::chrono::high_resolution_clock::now();
                auto us = std::chrono::duration_cast<std::chrono::microseconds>(now - lastTime).count();
                std::cout << "Event " << i+1 << " written in " << us << " us\n";
                lastTime = now;
            }
        }
        writer.close();  // finish writing, flush any buffered records&#8203;:contentReference[oaicite:2]{index=2}
        // Optionally force sync data to disk
        if (doSync) {
            int fd = ::open(outFile.c_str(), O_RDONLY);
            if (fd != -1) {
                ::fsync(fd);
                ::close(fd);
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        // Calculate throughput (MB/s). 1 MB = 10^6 bytes for this calculation.
        std::chrono::duration<double> secs = end - start;
        unsigned long long bytesWritten = eventsPerRun * eventBytes;
        double MBperSec = (bytesWritten / 1.0e6) / secs.count();
        totalThroughput += MBperSec;
        std::cout.setf(std::ios::fixed);
        std::cout.precision(2);
        std::cout << "Run " << r+1 << ": Wrote " << bytesWritten 
                  << " bytes in " << secs.count() << " s (throughput = " 
                  << MBperSec << " MB/s)\n";
    }
    if (repeatCount > 1) {
        std::cout << "Average throughput over " << repeatCount 
                  << " runs = " << (totalThroughput / repeatCount) << " MB/s\n";
    }
}
