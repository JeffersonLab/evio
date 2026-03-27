#include <algorithm>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "eviocc.h"

using namespace evio;

namespace {

constexpr uint16_t EXPECTED_TAG = 0x42;
constexpr size_t WARMUP_EVENTS = 20;

struct Options {
    std::vector<std::string> inputFiles;
    std::string outputFile;
};

struct Stats {
    uint64_t events = 0;
    uint64_t anomalies = 0;
    uint64_t malformed = 0;
    uint32_t maxLen = 0;
    uint64_t written = 0;
    bool firstWordSeen = false;
    uint32_t minFirstWord = std::numeric_limits<uint32_t>::max();
    uint32_t maxFirstWord = 0;
    bool timestampSeen = false;
    uint64_t minTimestamp = std::numeric_limits<uint64_t>::max();
    uint64_t maxTimestamp = 0;
};

bool isExpectedBankOfSegments(const std::shared_ptr<BaseStructure> &s) {
    return s != nullptr &&
           s->getStructureType().isBank() &&
           s->getHeader() != nullptr &&
           s->getHeader()->getDataType().isSegment();
}

void printUsage(const char *name) {
    std::cerr << "Usage: " << name << " [-o output_v4.evio] file1.evio [file2.evio ...]\n";
}

Options parseArgs(int argc, char *argv[]) {
    Options options;

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        }
        if (arg == "-o") {
            if (i + 1 >= argc) {
                throw std::runtime_error("missing filename after -o");
            }
            options.outputFile = argv[++i];
            continue;
        }
        if (arg.rfind("-o", 0) == 0 && arg.size() > 2) {
            options.outputFile = arg.substr(2);
            continue;
        }
        options.inputFiles.emplace_back(arg);
    }

    if (options.inputFiles.empty()) {
        throw std::runtime_error("you must specify at least one input file");
    }

    return options;
}

std::string getDictionaryXml(const std::string &path) {
    EvioReader reader(path);
    return reader.hasDictionaryXML() ? reader.getDictionaryXML() : "";
}

std::unique_ptr<EventWriter> createWriter(const std::string &outputFile,
                                          const std::vector<std::string> &inputFiles) {
    constexpr uint32_t maxRecordBytes = 1000000;
    constexpr uint32_t maxEventsPerRecord = 1000;
    constexpr size_t bufferBytes = 1000000;

    std::string dictXml;
    try {
        dictXml = getDictionaryXml(inputFiles.front());
    }
    catch (const std::exception &e) {
        std::cerr << "Warning: could not read dictionary from first input file: "
                  << e.what() << '\n';
    }

    std::string outFile = outputFile;
    return std::make_unique<EventWriter>(
        outFile,
        "", "", 1, 0,
        maxRecordBytes, maxEventsPerRecord,
        ByteOrder::ENDIAN_LOCAL,
        dictXml,
        true, false,
        nullptr, 1, 0, 1, 1,
        Compressor::UNCOMPRESSED, 1, 0,
        bufferBytes
    );
}

void updateFirstSegmentStats(const std::shared_ptr<BaseStructure> &segment, Stats &stats) {
    if (segment == nullptr || !segment->getStructureType().isSegment()) {
        return;
    }

    auto &raw = segment->getRawBytes();
    if (raw.size() < sizeof(uint32_t)) {
        return;
    }

    uint32_t word = Util::toInt(raw.data(), segment->getByteOrder());
    stats.firstWordSeen = true;
    stats.minFirstWord = std::min(stats.minFirstWord, word);
    stats.maxFirstWord = std::max(stats.maxFirstWord, word);

    if (raw.size() < 3 * sizeof(uint32_t)) {
        return;
    }

    uint64_t word2 = Util::toInt(raw.data() + sizeof(uint32_t), segment->getByteOrder());
    uint64_t word3 = Util::toInt(raw.data() + 2 * sizeof(uint32_t), segment->getByteOrder());
    uint64_t timestamp = (word3 << 32U) | word2;
    stats.timestampSeen = true;
    stats.minTimestamp = std::min(stats.minTimestamp, timestamp);
    stats.maxTimestamp = std::max(stats.maxTimestamp, timestamp);
}

void printFirstSegmentRanges(const Stats &stats) {
    if (!stats.firstWordSeen) {
        std::cout << " firstWordMin=n/a firstWordMax=n/a";
    }
    else {
        std::cout << " firstWordMin=" << stats.minFirstWord
                  << " firstWordMax=" << stats.maxFirstWord;
    }

    if (!stats.timestampSeen) {
        std::cout << " timestampMin=n/a timestampMax=n/a";
        return;
    }

    std::cout << " timestampMin=" << stats.minTimestamp
              << " timestampMax=" << stats.maxTimestamp;
}

void scanFile(const std::string &path, EventWriter *writer, Stats &totals) {
    Stats stats;
    std::cout << "FILE " << path << '\n';

    try {
        EvioReader reader(path);
        std::shared_ptr<EvioEvent> event;

        while ((event = reader.parseNextEvent())) {
            ++stats.events;
            bool show = stats.events <= WARMUP_EVENTS;

            if (writer != nullptr) {
                writer->writeEvent(event);
                ++stats.written;
            }

            try {
                auto first = event->getChildAt(0);
                if (!isExpectedBankOfSegments(first) || first->getChildCount() < 2) {
                    ++stats.malformed;
                    std::cout << "  ev " << stats.events << " malformed layout\n";
                    continue;
                }

                updateFirstSegmentStats(first->getChildAt(0), stats);

                auto second = first->getChildAt(1);
                auto header = second ? second->getHeader() : nullptr;
                if (second == nullptr || header == nullptr ||
                    !second->getStructureType().isSegment() ||
                    header->getTag() != EXPECTED_TAG) {
                    ++stats.malformed;
                    std::cout << "  ev " << stats.events << " malformed seg2"
                              << " tag=0x" << std::hex
                              << (header ? header->getTag() : 0) << std::dec << '\n';
                    continue;
                }

                uint32_t len = header->getDataLength();
                stats.maxLen = std::max(stats.maxLen, len);
                if (len >= 4) {
                    ++stats.anomalies;
                    show = true;
                }

                if (show) {
                    std::cout << "  ev " << stats.events
                              << " seg2(tag=0x42) dataLen=" << len << '\n';
                }
            }
            catch (const std::exception &) {
                ++stats.malformed;
                std::cout << "  ev " << stats.events << " malformed layout\n";
            }
        }
    }
    catch (const std::exception &e) {
        std::cerr << "  error: " << e.what() << '\n';
        return;
    }

    std::cout << "SUMMARY events=" << stats.events
              << " anomalies=" << stats.anomalies
              << " malformed=" << stats.malformed
              << " maxDataLen=" << stats.maxLen;
    if (writer != nullptr) {
        std::cout << " written=" << stats.written;
    }
    printFirstSegmentRanges(stats);
    std::cout << "\n\n";

    totals.events += stats.events;
    totals.anomalies += stats.anomalies;
    totals.malformed += stats.malformed;
    totals.written += stats.written;
    totals.maxLen = std::max(totals.maxLen, stats.maxLen);
    if (stats.firstWordSeen) {
        totals.firstWordSeen = true;
        totals.minFirstWord = std::min(totals.minFirstWord, stats.minFirstWord);
        totals.maxFirstWord = std::max(totals.maxFirstWord, stats.maxFirstWord);
    }
    if (stats.timestampSeen) {
        totals.timestampSeen = true;
        totals.minTimestamp = std::min(totals.minTimestamp, stats.minTimestamp);
        totals.maxTimestamp = std::max(totals.maxTimestamp, stats.maxTimestamp);
    }
}

} // namespace

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    try {
        Options options = parseArgs(argc, argv);
        std::unique_ptr<EventWriter> writer;
        if (!options.outputFile.empty()) {
            std::cout << "Writing EVIO-4 output to " << options.outputFile << '\n';
            writer = createWriter(options.outputFile, options.inputFiles);
        }

        Stats totals;
        for (const auto &path : options.inputFiles) {
            scanFile(path, writer.get(), totals);
        }

        if (writer != nullptr) {
            writer->close();
            std::cout << "OUTPUT summary events=" << totals.events
                      << " anomalies=" << totals.anomalies
                      << " malformed=" << totals.malformed
                      << " maxDataLen=" << totals.maxLen
                      << " written=" << totals.written;
            printFirstSegmentRanges(totals);
            std::cout << '\n';
        }
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}
