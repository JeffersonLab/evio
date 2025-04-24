#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

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

    // Prepare dummy data
    std::vector<uint8_t> data(bufferSize);
    for (size_t i = 0; i < bufferSize; ++i) {
        data[i] = static_cast<uint8_t>(i & 0xFF);
    }

    // Build one dummy event using EventBuilder
    evio::EventBuilder builder(1, evio::DataType::UCHAR8, 1);
    auto event = builder.getEvent();
    builder.setUCharData(event, data.data(), data.size());
    builder.setAllHeaderLengths();
    uint32_t eventBytes = event->getTotalBytes();  // size of one event (bytes)

    // Compute number of events to write to reach fileSize
    unsigned long long eventsPerRun = 
        static_cast<unsigned long long>(std::ceil(double(fileSize) / eventBytes));

    // Retrieve binary representation of the event
    std::vector<uint8_t> eventBuffer(eventBytes);
    event->write(eventBuffer.data(), evio::ByteOrder::ENDIAN_LOCAL); // serialize event to bytes&#8203;:contentReference[oaicite:9]{index=9}

    // EVIO file and record header construction (for one record containing all events)
    const uint32_t FILE_HEADER_WORDS = 14;
    const uint32_t RECORD_HEADER_WORDS = 14;
    // Prepare file header (14 words) as per EVIO format version 6
    uint32_t fileHeader[FILE_HEADER_WORDS];
    uint32_t recordHeader[RECORD_HEADER_WORDS];
    // File Header fields (EVIO 6):
    fileHeader[0] = 0x4556494F;                // "EVIO" file type ID
    fileHeader[1] = 1;                        // File number (for split files; 1 for single file)
    fileHeader[2] = FILE_HEADER_WORDS;        // Header length (words)
    fileHeader[3] = 1;                        // Record count in file (we will use one record)
    fileHeader[4] = 0;                        // Index array length (unused, 0)
    fileHeader[5] = 6;                        // Bit info & Version: version=6, no extra flags
    fileHeader[6] = 0;                        // User header length (no user header)
    fileHeader[7] = 0xC0DA0100;               // Magic number (endianness check)
    // The remaining 6 words (user register 64-bit, trailer position 64-bit, user ints) set to 0:
    fileHeader[8] = fileHeader[9] = 0;
    fileHeader[10] = fileHeader[11] = 0;
    fileHeader[12] = fileHeader[13] = 0;
    // Record Header fields (EVIO 6):
    uint32_t recordLenWords = RECORD_HEADER_WORDS + eventsPerRun * (eventBytes / 4);
    recordHeader[0] = recordLenWords;         // Record length in words (including this header)
    recordHeader[1] = 1;                      // Record (block) number
    recordHeader[2] = RECORD_HEADER_WORDS;    // Header length in words
    recordHeader[3] = (uint32_t) eventsPerRun;// Event count in this record
    recordHeader[4] = 0;                      // Index array length (no event index)
    recordHeader[5] = 0x206;                  // Bit info & Version: 0x200 (last record flag) | 0x6 (version 6)
    recordHeader[6] = 0;                      // Reserved / user header length (unused here)
    recordHeader[7] = 0xC0DA0100;             // Magic number
    // Remaining 6 words of record header:
    recordHeader[8] = recordHeader[9] = 0;    // (possibly uncompressed length or user info, not used)
    recordHeader[10] = recordHeader[11] = 0;  // (reserved/trailer info, not used)
    recordHeader[12] = recordHeader[13] = 0;  // User integers (unused)

    double totalThroughput = 0.0;
    for (int r = 0; r < repeatCount; ++r) {
        // Open output file with low-level POSIX API
        int fd = ::open(outFile.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
        if (fd < 0) {
            std::cerr << "Cannot open output file " << outFile << "\n";
            return 1;
        }
        // Start timing
        auto start = std::chrono::high_resolution_clock::now();
        // Write file header (uncompressed, 56 bytes for 14 words)
        ::write(fd, fileHeader, FILE_HEADER_WORDS * sizeof(uint32_t));
        // Write record header (56 bytes)
        ::write(fd, recordHeader, RECORD_HEADER_WORDS * sizeof(uint32_t));
        // Write events in a loop
        for (unsigned long long i = 0; i < eventsPerRun; ++i) {
            ::write(fd, eventBuffer.data(), eventBytes);
            if (debug) {
                static auto lastTime = start;
                auto now = std::chrono::high_resolution_clock::now();
                auto us = std::chrono::duration_cast<std::chrono::microseconds>(now - lastTime).count();
                std::cout << "Event " << i+1 << " written in " << us << " us\n";
                lastTime = now;
            }
        }
        // Optional sync to disk
        if (doSync) {
            ::fsync(fd);
        }
        ::close(fd);
        auto end = std::chrono::high_resolution_clock::now();
        // Calculate throughput
        std::chrono::duration<double> secs = end - start;
        unsigned long long bytesWritten = (FILE_HEADER_WORDS + RECORD_HEADER_WORDS) * 4 
                                        + eventsPerRun * eventBytes;
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
