#include <algorithm>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <unordered_map>
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
    uint64_t manyCrateTSCount = 0;
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

struct FirstSegmentInfo {
    uint32_t firstWord = 0;
    bool hasTimestamp = false;
    uint64_t timestamp = 0;
};

struct CoverageStats {
    uint64_t entries = 0;
    uint64_t uniqueCounters = 0;
    uint64_t rangeSize = 0;
    uint64_t duplicates = 0;
    std::unordered_map<uint32_t, uint32_t> counterCounts;
    std::unordered_set<uint32_t> seenCounters;
};

struct DuplicateEntry {
    std::string file;
    uint64_t eventIndex = 0;
    uint32_t frame = 0;
    bool hasTimestamp = false;
    uint64_t timestamp = 0;
    bool hasSeg2Len = false;
    uint32_t seg2Len = 0;
};

struct TimestampOccurrence {
    std::string file;
    uint64_t eventIndex = 0;
    uint64_t timestamp = 0;
    uint32_t frame = 0;
    uint32_t seg2Len = 0;
    bool hasSeg2Len = false;
};

struct CrossFileTimestampStats {
    std::unordered_map<uint64_t, uint64_t> timestampFileMask;
    std::unordered_map<uint64_t, uint32_t> timestampCounts;
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

bool getFirstSegmentInfo(const std::shared_ptr<BaseStructure> &segment, FirstSegmentInfo &info) {
    if (segment == nullptr || !segment->getStructureType().isSegment()) {
        return false;
    }

    auto &raw = segment->getRawBytes();
    if (raw.size() < sizeof(uint32_t)) {
        return false;
    }

    info.firstWord = Util::toInt(raw.data(), segment->getByteOrder());

    if (raw.size() < 3 * sizeof(uint32_t)) {
        return true;
    }

    uint64_t word2 = Util::toInt(raw.data() + sizeof(uint32_t), segment->getByteOrder());
    uint64_t word3 = Util::toInt(raw.data() + 2 * sizeof(uint32_t), segment->getByteOrder());
    info.hasTimestamp = true;
    info.timestamp = (word3 << 32U) | word2;
    return true;
}

void updateFirstSegmentStats(const std::shared_ptr<BaseStructure> &segment, Stats &stats) {
    FirstSegmentInfo info;
    if (!getFirstSegmentInfo(segment, info)) {
        return;
    }

    stats.firstWordSeen = true;
    stats.minFirstWord = std::min(stats.minFirstWord, info.firstWord);
    stats.maxFirstWord = std::max(stats.maxFirstWord, info.firstWord);

    if (info.hasTimestamp) {
        stats.timestampSeen = true;
        stats.minTimestamp = std::min(stats.minTimestamp, info.timestamp);
        stats.maxTimestamp = std::max(stats.maxTimestamp, info.timestamp);
    }
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

CoverageStats computeCounterCoverage(const std::vector<std::string> &inputFiles,
                                     uint32_t globalMin,
                                     uint32_t globalMax) {
    CoverageStats coverage;
    coverage.rangeSize = static_cast<uint64_t>(globalMax) - globalMin + 1;

    coverage.seenCounters.reserve(static_cast<size_t>(
        std::min<uint64_t>(coverage.rangeSize, 1'000'000ULL)));

    for (const auto &path : inputFiles) {
        EvioReader reader(path);
        std::shared_ptr<EvioEvent> event;

        while ((event = reader.parseNextEvent())) {
            try {
                auto first = event->getChildAt(0);
                if (!isExpectedBankOfSegments(first) || first->getChildCount() < 1) {
                    continue;
                }

                FirstSegmentInfo info;
                if (!getFirstSegmentInfo(first->getChildAt(0), info)) {
                    continue;
                }

                ++coverage.entries;
                coverage.seenCounters.insert(info.firstWord);
                ++coverage.counterCounts[info.firstWord];
            }
            catch (const std::exception &) {
            }
        }
    }

    coverage.uniqueCounters = coverage.seenCounters.size();
    coverage.duplicates = coverage.entries - coverage.uniqueCounters;
    return coverage;
}

std::vector<uint32_t> getFirstDuplicateFrames(const CoverageStats &coverage, size_t limit) {
    std::vector<uint32_t> frames;
    frames.reserve(coverage.counterCounts.size());

    for (const auto &item : coverage.counterCounts) {
        if (item.second > 1) {
            frames.push_back(item.first);
        }
    }

    std::sort(frames.begin(), frames.end());
    if (frames.size() > limit) {
        frames.resize(limit);
    }
    return frames;
}

std::vector<DuplicateEntry> collectDuplicateEntries(const std::vector<std::string> &inputFiles,
                                                    const std::set<uint32_t> &frames) {
    std::vector<DuplicateEntry> entries;

    for (const auto &path : inputFiles) {
        EvioReader reader(path);
        std::shared_ptr<EvioEvent> event;
        uint64_t eventIndex = 0;

        while ((event = reader.parseNextEvent())) {
            ++eventIndex;

            try {
                auto first = event->getChildAt(0);
                if (!isExpectedBankOfSegments(first) || first->getChildCount() < 1) {
                    continue;
                }

                FirstSegmentInfo info;
                if (!getFirstSegmentInfo(first->getChildAt(0), info) ||
                    frames.find(info.firstWord) == frames.end()) {
                    continue;
                }

                DuplicateEntry entry;
                entry.file = path;
                entry.eventIndex = eventIndex;
                entry.frame = info.firstWord;
                entry.hasTimestamp = info.hasTimestamp;
                entry.timestamp = info.timestamp;

                if (first->getChildCount() >= 2) {
                    auto second = first->getChildAt(1);
                    auto header = second ? second->getHeader() : nullptr;
                    if (second != nullptr && header != nullptr && second->getStructureType().isSegment()) {
                        entry.hasSeg2Len = true;
                        entry.seg2Len = header->getDataLength();
                    }
                }

                entries.push_back(std::move(entry));
            }
            catch (const std::exception &) {
            }
        }
    }

    return entries;
}

CrossFileTimestampStats computeCrossFileTimestampStats(const std::vector<std::string> &inputFiles) {
    CrossFileTimestampStats stats;

    for (size_t fileIndex = 0; fileIndex < inputFiles.size(); ++fileIndex) {
        EvioReader reader(inputFiles[fileIndex]);
        std::shared_ptr<EvioEvent> event;

        while ((event = reader.parseNextEvent())) {
            try {
                auto first = event->getChildAt(0);
                if (!isExpectedBankOfSegments(first) || first->getChildCount() < 1) {
                    continue;
                }

                FirstSegmentInfo info;
                if (!getFirstSegmentInfo(first->getChildAt(0), info) || !info.hasTimestamp) {
                    continue;
                }

                stats.timestampFileMask[info.timestamp] |= (1ULL << fileIndex);
                ++stats.timestampCounts[info.timestamp];
            }
            catch (const std::exception &) {
            }
        }
    }

    return stats;
}

std::vector<uint64_t> getCrossFileDuplicateTimestamps(const CrossFileTimestampStats &stats,
                                                      size_t limit) {
    std::vector<uint64_t> timestamps;
    timestamps.reserve(stats.timestampFileMask.size());

    for (const auto &item : stats.timestampFileMask) {
        if (__builtin_popcountll(item.second) > 1) {
            timestamps.push_back(item.first);
        }
    }

    std::sort(timestamps.begin(), timestamps.end());
    if (timestamps.size() > limit) {
        timestamps.resize(limit);
    }
    return timestamps;
}

std::vector<TimestampOccurrence> collectTimestampOccurrences(const std::vector<std::string> &inputFiles,
                                                             const std::set<uint64_t> &timestamps) {
    std::vector<TimestampOccurrence> entries;

    for (const auto &path : inputFiles) {
        EvioReader reader(path);
        std::shared_ptr<EvioEvent> event;
        uint64_t eventIndex = 0;

        while ((event = reader.parseNextEvent())) {
            ++eventIndex;

            try {
                auto first = event->getChildAt(0);
                if (!isExpectedBankOfSegments(first) || first->getChildCount() < 1) {
                    continue;
                }

                FirstSegmentInfo info;
                if (!getFirstSegmentInfo(first->getChildAt(0), info) ||
                    !info.hasTimestamp ||
                    timestamps.find(info.timestamp) == timestamps.end()) {
                    continue;
                }

                TimestampOccurrence entry;
                entry.file = path;
                entry.eventIndex = eventIndex;
                entry.timestamp = info.timestamp;
                entry.frame = info.firstWord;

                if (first->getChildCount() >= 2) {
                    auto second = first->getChildAt(1);
                    auto header = second ? second->getHeader() : nullptr;
                    if (second != nullptr && header != nullptr && second->getStructureType().isSegment()) {
                        entry.hasSeg2Len = true;
                        entry.seg2Len = header->getDataLength();
                    }
                }

                entries.push_back(std::move(entry));
            }
            catch (const std::exception &) {
            }
        }
    }

    return entries;
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
                    ++stats.manyCrateTSCount;
                    show = true;
                }

                // if (show) {
                //     std::cout << "  ev " << stats.events
                //               << " seg2(tag=0x42) dataLen=" << len << '\n';
                // }
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
              << " manyCrateTSCount=" << stats.manyCrateTSCount
              << " malformed=" << stats.malformed
              << " maxDataLen=" << stats.maxLen;
    if (writer != nullptr) {
        std::cout << " written=" << stats.written;
    }
    printFirstSegmentRanges(stats);
    std::cout << "\n\n";

    totals.events += stats.events;
    totals.manyCrateTSCount += stats.manyCrateTSCount;
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
                      << " manyCrateTSCount=" << totals.manyCrateTSCount
                      << " malformed=" << totals.malformed
                      << " maxDataLen=" << totals.maxLen
                      << " written=" << totals.written;
            printFirstSegmentRanges(totals);
            std::cout << '\n';
        }

        if (totals.firstWordSeen) {
            CoverageStats coverage =
                computeCounterCoverage(options.inputFiles, totals.minFirstWord, totals.maxFirstWord);

            std::cout << "GLOBAL counter range min=" << totals.minFirstWord
                      << " max=" << totals.maxFirstWord
                      << " span=" << coverage.rangeSize << '\n';

            auto oldFlags = std::cout.flags();
            auto oldPrec = std::cout.precision();
            std::cout << std::fixed << std::setprecision(6)
                      << "GLOBAL counter coverage entries=" << coverage.entries
                      << " unique=" << coverage.uniqueCounters
                      << " duplicates=" << coverage.duplicates
                      << " fraction=";
            if (coverage.rangeSize > 0) {
                std::cout << (static_cast<double>(coverage.uniqueCounters) /
                              static_cast<double>(coverage.rangeSize));
            }
            else {
                std::cout << 0.0;
            }
            std::cout << '\n';
            std::cout.flags(oldFlags);
            std::cout.precision(oldPrec);

            std::vector<uint32_t> duplicateFrames = getFirstDuplicateFrames(coverage, 5);
            if (!duplicateFrames.empty()) {
                std::set<uint32_t> frameSet(duplicateFrames.begin(), duplicateFrames.end());
                std::vector<DuplicateEntry> entries =
                    collectDuplicateEntries(options.inputFiles, frameSet);

                std::cout << "GLOBAL duplicate frame details (first "
                          << duplicateFrames.size()
                          << " duplicate frame counters by ascending value):\n";

                for (uint32_t frame : duplicateFrames) {
                    uint32_t count = coverage.counterCounts.at(frame);
                    std::cout << "  frame=" << frame << " count=" << count << '\n';
                    for (const auto &entry : entries) {
                        if (entry.frame != frame) {
                            continue;
                        }

                        std::cout << "    file=" << entry.file
                                  << " ev=" << entry.eventIndex;
                        if (entry.hasTimestamp) {
                            std::cout << " timestamp=" << entry.timestamp;
                        }
                        else {
                            std::cout << " timestamp=n/a";
                        }
                        if (entry.hasSeg2Len) {
                            std::cout << " seg2Len=" << entry.seg2Len;
                        }
                        else {
                            std::cout << " seg2Len=n/a";
                        }
                        std::cout << '\n';
                    }
                }
            }

            for (uint32_t frame = totals.minFirstWord; frame <= totals.maxFirstWord; ++frame) {
                if (coverage.seenCounters.find(frame) == coverage.seenCounters.end()) {
                    std::cout << "GLOBAL missing frame=" << frame << '\n';
                }
            }
        }

        CrossFileTimestampStats crossFileTs = computeCrossFileTimestampStats(options.inputFiles);
        std::vector<uint64_t> crossFileDuplicates = getCrossFileDuplicateTimestamps(crossFileTs, 5);

        std::cout << "GLOBAL cross-file duplicate timestamps="
                  << ([&]() -> uint64_t {
                        uint64_t count = 0;
                        for (const auto &item : crossFileTs.timestampFileMask) {
                            if (__builtin_popcountll(item.second) > 1) {
                                ++count;
                            }
                        }
                        return count;
                     })()
                  << '\n';

        if (!crossFileDuplicates.empty()) {
            std::set<uint64_t> timestampSet(crossFileDuplicates.begin(), crossFileDuplicates.end());
            std::vector<TimestampOccurrence> entries =
                collectTimestampOccurrences(options.inputFiles, timestampSet);

            std::cout << "GLOBAL cross-file duplicate timestamp details (first "
                      << crossFileDuplicates.size()
                      << " by ascending value):\n";

            for (uint64_t timestamp : crossFileDuplicates) {
                uint64_t mask = crossFileTs.timestampFileMask.at(timestamp);
                uint32_t fileCount = __builtin_popcountll(mask);
                uint32_t count = crossFileTs.timestampCounts.at(timestamp);
                std::cout << "  timestamp=" << timestamp
                          << " files=" << fileCount
                          << " count=" << count << '\n';
                for (const auto &entry : entries) {
                    if (entry.timestamp != timestamp) {
                        continue;
                    }
                    std::cout << "    file=" << entry.file
                              << " ev=" << entry.eventIndex
                              << " frame=" << entry.frame;
                    if (entry.hasSeg2Len) {
                        std::cout << " seg2Len=" << entry.seg2Len;
                    }
                    else {
                        std::cout << " seg2Len=n/a";
                    }
                    std::cout << '\n';
                }
            }
        }
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}
