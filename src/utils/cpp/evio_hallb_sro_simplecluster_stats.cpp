#include <algorithm>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <poll.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

#include "eviocc.h"

using namespace evio;

namespace {

std::atomic<bool> gQuit{false};

constexpr uint16_t TAG_PRESTART = 0xffd1;
constexpr uint16_t TAG_GO = 0xffd2;
constexpr uint16_t TAG_END = 0xffd4;
constexpr uint16_t TAG_BUILT_STREAMING = 0xff60;
constexpr uint64_t TIMEFRAME_NS = 65536;

struct Hit {
    int crate = 0;
    int slot = 0;
    int channel = 0;
    int charge = 0;
    uint64_t time = 0;
};

struct MaskedChannel {
    int crate = 0;
    int slot = 0;
    int channel = 0;
};

struct ChannelId {
    int crate = 0;
    int slot = 0;
    int channel = 0;
};

struct Options {
    bool clusterDebug = false;
    std::vector<MaskedChannel> masks;
    std::vector<std::string> inputFiles;
};

struct ClusterScanConfig {
    uint64_t windowNs = 64;
    uint64_t stepNs = 8;
    uint64_t extensionNs = 64;
    uint64_t timeframeNs = TIMEFRAME_NS;
    size_t minChannels = 3;
};

struct Cluster {
    uint64_t seedLeft = 0;
    uint64_t seedRight = 0;
    uint64_t frozenLeft = 0;
    uint64_t finalRight = 0;
    std::vector<Hit> hits;
};

struct ClusterDebugStep {
    std::string phase;
    uint64_t frameStart = 0;
    uint64_t left = 0;
    uint64_t right = 0;
    size_t windowHits = 0;
    size_t windowChannels = 0;
    size_t associatedHits = 0;
    size_t associatedChannels = 0;
    bool seedFound = false;
};

struct FrameResult {
    bool haveFrameInfo = false;
    uint32_t frameNumber = 0;
    uint64_t frameTimestamp = 0;
    uint64_t eventBytes = 0;
    std::vector<Hit> visibleHits;
    std::vector<Cluster> clusters;
};

struct RunStats {
    uint64_t inputFiles = 0;
    uint64_t inputBytes = 0;
    uint64_t eventBytes = 0;
    uint64_t events = 0;
    uint64_t controlEvents = 0;
    uint64_t frames = 0;
    uint64_t builtStreamingEvents = 0;
    uint64_t malformedFrames = 0;
    uint64_t framesWithClusters = 0;
    uint64_t visibleHits = 0;
    uint64_t clusters = 0;
    uint64_t associatedHits = 0;
    bool frameNumberSeen = false;
    uint32_t minFrameNumber = std::numeric_limits<uint32_t>::max();
    uint32_t maxFrameNumber = 0;
    bool timestampSeen = false;
    uint64_t minTimestamp = std::numeric_limits<uint64_t>::max();
    uint64_t maxTimestamp = 0;
};

using ClusterDebugCallback = std::function<bool(const ClusterDebugStep &)>;

bool waitForPrompt(const std::string &prompt);

void ctrlCHandler(int) {
    gQuit.store(true);
}

void printUsage(const char *name) {
    std::cerr << "Usage: " << name
              << " [--cluster-debug] [--mask crate,slot,channel] file1.evio [file2.evio ...]\n";
}

MaskedChannel parseMaskSpec(const std::string &spec) {
    size_t comma1 = spec.find(',');
    size_t comma2 = (comma1 == std::string::npos) ? std::string::npos : spec.find(',', comma1 + 1);

    if (comma1 == std::string::npos || comma2 == std::string::npos ||
        spec.find(',', comma2 + 1) != std::string::npos) {
        throw std::runtime_error("mask must be in crate,slot,channel form");
    }

    MaskedChannel mask;
    mask.crate = std::stoi(spec.substr(0, comma1));
    mask.slot = std::stoi(spec.substr(comma1 + 1, comma2 - comma1 - 1));
    mask.channel = std::stoi(spec.substr(comma2 + 1));
    return mask;
}

Options parseArgs(int argc, char *argv[]) {
    Options options;

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        }

        if (arg == "--cluster-debug") {
            options.clusterDebug = true;
            continue;
        }

        if (arg == "--mask") {
            if (i + 1 >= argc) {
                throw std::runtime_error("missing argument after --mask");
            }
            options.masks.push_back(parseMaskSpec(argv[++i]));
            continue;
        }

        if (arg.rfind("--mask=", 0) == 0) {
            options.masks.push_back(parseMaskSpec(arg.substr(7)));
            continue;
        }

        if (!arg.empty() && arg[0] == '-') {
            throw std::runtime_error("unknown option: " + arg);
        }

        options.inputFiles.push_back(arg);
    }

    if (options.inputFiles.empty()) {
        throw std::runtime_error("you must specify at least one input file");
    }

    return options;
}

bool isMasked(const Hit &hit, const std::vector<MaskedChannel> &masks) {
    for (const auto &mask : masks) {
        if (hit.crate == mask.crate &&
            hit.slot == mask.slot &&
            hit.channel == mask.channel) {
            return true;
        }
    }
    return false;
}

void appendVisibleHits(const std::vector<Hit> &hits,
                       const Options &options,
                       std::vector<Hit> &visibleHits) {
    for (const auto &hit : hits) {
        if (!isMasked(hit, options.masks)) {
            visibleHits.push_back(hit);
        }
    }
}

ChannelId getChannelId(const Hit &hit) {
    ChannelId id;
    id.crate = hit.crate;
    id.slot = hit.slot;
    id.channel = hit.channel;
    return id;
}

bool sameChannel(const ChannelId &a, const ChannelId &b) {
    return a.crate == b.crate && a.slot == b.slot && a.channel == b.channel;
}

bool hitLess(const Hit &a, const Hit &b) {
    if (a.time != b.time) {
        return a.time < b.time;
    }
    if (a.crate != b.crate) {
        return a.crate < b.crate;
    }
    if (a.slot != b.slot) {
        return a.slot < b.slot;
    }
    if (a.channel != b.channel) {
        return a.channel < b.channel;
    }
    return a.charge < b.charge;
}

bool containsIndex(const std::vector<size_t> &indices, size_t index) {
    return std::find(indices.begin(), indices.end(), index) != indices.end();
}

void addWindowAssociations(std::vector<size_t> &associatedIndices,
                           const std::vector<size_t> &windowIndices) {
    for (size_t index : windowIndices) {
        if (!containsIndex(associatedIndices, index)) {
            associatedIndices.push_back(index);
        }
    }
}

std::vector<size_t> getHitIndicesInRange(const std::vector<Hit> &hits,
                                         uint64_t left,
                                         uint64_t right) {
    std::vector<size_t> indices;
    for (size_t i = 0; i < hits.size(); ++i) {
        if (hits[i].time >= right) {
            break;
        }
        if (hits[i].time >= left) {
            indices.push_back(i);
        }
    }
    return indices;
}

size_t countDistinctChannels(const std::vector<Hit> &hits,
                             const std::vector<size_t> &indices) {
    std::vector<ChannelId> channels;

    for (size_t index : indices) {
        ChannelId id = getChannelId(hits[index]);
        bool found = false;
        for (const auto &known : channels) {
            if (sameChannel(id, known)) {
                found = true;
                break;
            }
        }
        if (!found) {
            channels.push_back(id);
        }
    }

    return channels.size();
}

size_t countDistinctChannels(const std::vector<Hit> &hits) {
    std::vector<size_t> indices;
    indices.reserve(hits.size());
    for (size_t i = 0; i < hits.size(); ++i) {
        indices.push_back(i);
    }
    return countDistinctChannels(hits, indices);
}

uint64_t earliestAssociatedTime(const std::vector<Hit> &hits,
                                const std::vector<size_t> &indices) {
    uint64_t earliest = std::numeric_limits<uint64_t>::max();
    for (size_t index : indices) {
        earliest = std::min(earliest, hits[index].time);
    }
    return earliest;
}

std::vector<Hit> makeClusterHits(const std::vector<Hit> &hits,
                                 const std::vector<size_t> &indices) {
    std::vector<Hit> clusterHits;
    clusterHits.reserve(indices.size());
    for (size_t index : indices) {
        clusterHits.push_back(hits[index]);
    }

    std::sort(clusterHits.begin(), clusterHits.end(), hitLess);
    return clusterHits;
}

bool emitClusterDebug(const ClusterDebugCallback &debugCallback,
                      const ClusterDebugStep &step) {
    if (!debugCallback) {
        return true;
    }
    return debugCallback(step);
}

std::vector<Cluster> findSlidingWindowClusters(const std::vector<Hit> &hits,
                                               uint64_t frameStart,
                                               const ClusterScanConfig &config,
                                               const ClusterDebugCallback &debugCallback) {
    if (config.windowNs == 0 || config.stepNs == 0 || config.minChannels == 0) {
        throw std::runtime_error("cluster scan configuration must use non-zero window, step, and threshold");
    }
    if (config.windowNs > config.timeframeNs) {
        throw std::runtime_error("cluster scan window cannot be larger than the timeframe");
    }

    const uint64_t frameEnd = frameStart + config.timeframeNs;
    std::vector<Hit> frameHits;
    frameHits.reserve(hits.size());
    for (const auto &hit : hits) {
        if (hit.time >= frameStart && hit.time < frameEnd) {
            frameHits.push_back(hit);
        }
    }

    std::sort(frameHits.begin(), frameHits.end(), hitLess);

    std::vector<Cluster> clusters;
    if (frameHits.empty()) {
        return clusters;
    }

    const uint64_t lastSearchLeft = frameEnd - config.windowNs;
    uint64_t left = frameStart;

    while (left <= lastSearchLeft) {
        const uint64_t right = left + config.windowNs;
        std::vector<size_t> windowIndices = getHitIndicesInRange(frameHits, left, right);
        const size_t windowChannels = countDistinctChannels(frameHits, windowIndices);
        const bool seedFound = windowChannels >= config.minChannels;

        ClusterDebugStep searchStep;
        searchStep.phase = "search";
        searchStep.frameStart = frameStart;
        searchStep.left = left;
        searchStep.right = right;
        searchStep.windowHits = windowIndices.size();
        searchStep.windowChannels = windowChannels;
        searchStep.associatedHits = seedFound ? windowIndices.size() : 0;
        searchStep.associatedChannels = seedFound ? windowChannels : 0;
        searchStep.seedFound = seedFound;
        if (!emitClusterDebug(debugCallback, searchStep)) {
            return clusters;
        }

        if (!seedFound) {
            if (left + config.stepNs > lastSearchLeft) {
                break;
            }
            left += config.stepNs;
            continue;
        }

        Cluster cluster;
        cluster.seedLeft = left;
        cluster.seedRight = right;

        std::vector<size_t> associatedIndices;
        addWindowAssociations(associatedIndices, windowIndices);

        const uint64_t earliestTime = earliestAssociatedTime(frameHits, associatedIndices);
        uint64_t trackLeft = left;
        uint64_t trackRight = right;

        while (trackLeft + config.stepNs <= earliestTime &&
               trackRight + config.stepNs <= frameEnd) {
            trackLeft += config.stepNs;
            trackRight += config.stepNs;

            windowIndices = getHitIndicesInRange(frameHits, trackLeft, trackRight);
            addWindowAssociations(associatedIndices, windowIndices);

            ClusterDebugStep trackStep;
            trackStep.phase = "track";
            trackStep.frameStart = frameStart;
            trackStep.left = trackLeft;
            trackStep.right = trackRight;
            trackStep.windowHits = windowIndices.size();
            trackStep.windowChannels = countDistinctChannels(frameHits, windowIndices);
            trackStep.associatedHits = associatedIndices.size();
            trackStep.associatedChannels = countDistinctChannels(frameHits, associatedIndices);
            trackStep.seedFound = true;
            if (!emitClusterDebug(debugCallback, trackStep)) {
                return clusters;
            }
        }

        cluster.frozenLeft = trackLeft;

        const uint64_t extensionTarget = std::min(frameEnd, trackRight + config.extensionNs);
        uint64_t extendedRight = trackRight;
        while (extendedRight < extensionTarget) {
            extendedRight = std::min(extensionTarget, extendedRight + config.stepNs);

            windowIndices = getHitIndicesInRange(frameHits, trackLeft, extendedRight);
            addWindowAssociations(associatedIndices, windowIndices);

            ClusterDebugStep extendStep;
            extendStep.phase = "extend";
            extendStep.frameStart = frameStart;
            extendStep.left = trackLeft;
            extendStep.right = extendedRight;
            extendStep.windowHits = windowIndices.size();
            extendStep.windowChannels = countDistinctChannels(frameHits, windowIndices);
            extendStep.associatedHits = associatedIndices.size();
            extendStep.associatedChannels = countDistinctChannels(frameHits, associatedIndices);
            extendStep.seedFound = true;
            if (!emitClusterDebug(debugCallback, extendStep)) {
                return clusters;
            }
        }

        cluster.finalRight = extendedRight;
        cluster.hits = makeClusterHits(frameHits, associatedIndices);
        clusters.push_back(cluster);

        left = cluster.finalRight;
    }

    return clusters;
}

std::string formatHex16(uint16_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(4) << std::setfill('0') << value << std::dec;
    return out.str();
}

std::string formatDouble(double value, int precision = 6) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

uint64_t offsetFromFrameStart(uint64_t time, uint64_t frameStart) {
    return (time >= frameStart) ? (time - frameStart) : 0;
}

uint64_t summedCharge(const std::vector<Hit> &hits) {
    uint64_t total = 0;
    for (const auto &hit : hits) {
        if (hit.charge > 0) {
            total += static_cast<uint64_t>(hit.charge);
        }
    }
    return total;
}

double chargeWeightedAverageOffset(const std::vector<Hit> &hits, uint64_t frameStart) {
    long double weightedOffset = 0.0L;
    uint64_t totalCharge = 0;

    for (const auto &hit : hits) {
        if (hit.charge <= 0) {
            continue;
        }
        const uint64_t charge = static_cast<uint64_t>(hit.charge);
        weightedOffset += static_cast<long double>(offsetFromFrameStart(hit.time, frameStart)) *
                          static_cast<long double>(charge);
        totalCharge += charge;
    }

    if (totalCharge > 0) {
        return static_cast<double>(weightedOffset / static_cast<long double>(totalCharge));
    }

    if (hits.empty()) {
        return 0.0;
    }

    long double offsetSum = 0.0L;
    for (const auto &hit : hits) {
        offsetSum += static_cast<long double>(offsetFromFrameStart(hit.time, frameStart));
    }
    return static_cast<double>(offsetSum / static_cast<long double>(hits.size()));
}

void printClusterDebugStep(const ClusterDebugStep &step) {
    std::cout << "      cluster-debug: phase=" << step.phase
              << " window=[" << step.left << ", " << step.right << ")"
              << " offset=[" << offsetFromFrameStart(step.left, step.frameStart)
              << ", " << offsetFromFrameStart(step.right, step.frameStart) << ")"
              << " width=" << (step.right - step.left)
              << " hits=" << step.windowHits
              << " channels=" << step.windowChannels
              << " associatedHits=" << step.associatedHits
              << " associatedChannels=" << step.associatedChannels;
    if (step.seedFound) {
        std::cout << " seed";
    }
    std::cout << '\n';
}

void printClusterDetails(const std::vector<Cluster> &clusters, uint64_t frameStart) {
    for (size_t i = 0; i < clusters.size(); ++i) {
        const auto &cluster = clusters[i];
        std::cout << "    Cluster[" << i << "]"
                  << ": seedWindow=[" << cluster.seedLeft << ", " << cluster.seedRight << ")"
                  << " frozenLeft=" << cluster.frozenLeft
                  << " finalRight=" << cluster.finalRight
                  << " offsets=[" << offsetFromFrameStart(cluster.seedLeft, frameStart)
                  << ", " << offsetFromFrameStart(cluster.finalRight, frameStart) << ")"
                  << " hits=" << cluster.hits.size()
                  << " channels=" << countDistinctChannels(cluster.hits)
                  << " summedCharge=" << summedCharge(cluster.hits)
                  << " chargeWeightedOffset=" << formatDouble(chargeWeightedAverageOffset(cluster.hits, frameStart), 2)
                  << '\n';

        for (size_t j = 0; j < cluster.hits.size(); ++j) {
            const auto &hit = cluster.hits[j];
            std::cout << "      hit[" << j << "]"
                      << ": crate=" << hit.crate
                      << ", slot=" << hit.slot
                      << ", channel=" << hit.channel
                      << ", charge=" << hit.charge
                      << ", time=" << hit.time
                      << ", offset=" << offsetFromFrameStart(hit.time, frameStart)
                      << '\n';
        }
    }
}

uint64_t combineTimestamp(uint32_t low, uint32_t high) {
    return (static_cast<uint64_t>(high) << 32U) | low;
}

uint32_t readWord(const std::vector<uint8_t> &bytes, size_t byteOffset, const ByteOrder &order) {
    if (byteOffset + sizeof(uint32_t) > bytes.size()) {
        throw std::runtime_error("payload ended in the middle of a 32-bit word");
    }
    return Util::toInt(bytes.data() + byteOffset, order);
}

bool readFrameInfo(const std::shared_ptr<BaseStructure> &node,
                   uint32_t &frameNumber,
                   uint64_t &timestamp) {
    if (node == nullptr) {
        return false;
    }

    auto header = node->getHeader();
    if (header == nullptr || !header->getDataType().isInteger()) {
        return false;
    }

    try {
        if (header->getDataType() == DataType::INT32) {
            auto ints = node->getIntData();
            if (ints.size() < 3) {
                return false;
            }

            frameNumber = static_cast<uint32_t>(ints[0]);
            timestamp = combineTimestamp(static_cast<uint32_t>(ints[1]),
                                         static_cast<uint32_t>(ints[2]));
            return true;
        }

        if (header->getDataType() == DataType::UINT32) {
            auto ints = node->getUIntData();
            if (ints.size() < 3) {
                return false;
            }

            frameNumber = ints[0];
            timestamp = combineTimestamp(ints[1], ints[2]);
            return true;
        }
    }
    catch (const std::exception &) {
        return false;
    }

    return false;
}

std::vector<Hit> decodeFadc250Payload(uint64_t frameTimestampNs,
                                      int crate,
                                      int slot,
                                      const std::vector<uint8_t> &payload,
                                      const ByteOrder &order) {
    std::vector<Hit> hits;

    if (payload.empty()) {
        return hits;
    }

    size_t payloadBytes = payload.size() - (payload.size() % sizeof(uint32_t));
    size_t numWords = payloadBytes / sizeof(uint32_t);
    hits.reserve(numWords);

    for (size_t i = 0; i < payloadBytes; i += sizeof(uint32_t)) {
        uint32_t word = readWord(payload, i, order);

        if ((word & 0x80000000U) != 0U) {
            continue;
        }

        Hit hit;
        hit.crate = crate;
        hit.slot = slot;
        hit.channel = static_cast<int>((word >> 13U) & 0x000fU);
        hit.charge = static_cast<int>(word & 0x1fffU);
        hit.time = frameTimestampNs + (static_cast<uint64_t>((word >> 17U) & 0x3fffU) * 4U);
        hits.push_back(hit);
    }

    std::sort(hits.begin(), hits.end(), hitLess);
    return hits;
}

FrameResult decodeStreamingFrame(const std::shared_ptr<EvioEvent> &event,
                                 const Options &options) {
    FrameResult result;
    if (event == nullptr) {
        return result;
    }

    result.eventBytes = event->getTotalBytes();

    if (event->getChildCount() > 0) {
        auto eventInfo = event->getChildAt(0);

        if (readFrameInfo(eventInfo, result.frameNumber, result.frameTimestamp)) {
            result.haveFrameInfo = true;
        }
        else if (eventInfo != nullptr && eventInfo->getChildCount() > 0) {
            auto timeSliceSeg = eventInfo->getChildAt(0);
            if (readFrameInfo(timeSliceSeg, result.frameNumber, result.frameTimestamp)) {
                result.haveFrameInfo = true;
            }
        }
    }

    for (size_t rocIndex = 1; rocIndex < event->getChildCount(); ++rocIndex) {
        auto rocTSB = event->getChildAt(rocIndex);
        auto rocHeader = rocTSB ? rocTSB->getHeader() : nullptr;
        int crate = rocHeader ? rocHeader->getTag() : static_cast<int>(rocIndex - 1);

        if (rocTSB == nullptr || rocTSB->getChildCount() == 0) {
            continue;
        }

        auto sib = rocTSB->getChildAt(0);
        if (sib != nullptr && sib->getChildCount() > 0) {
            auto timeSliceSeg = sib->getChildAt(0);
            uint32_t rocFrameNumber = 0;
            uint64_t rocTimestamp = 0;

            if (readFrameInfo(timeSliceSeg, rocFrameNumber, rocTimestamp) && !result.haveFrameInfo) {
                result.frameNumber = rocFrameNumber;
                result.frameTimestamp = rocTimestamp;
                result.haveFrameInfo = true;
            }
        }

        for (size_t payloadIndex = 1; payloadIndex < rocTSB->getChildCount(); ++payloadIndex) {
            auto dataBank = rocTSB->getChildAt(payloadIndex);
            auto dataHeader = dataBank ? dataBank->getHeader() : nullptr;
            int slot = dataHeader ? dataHeader->getTag() : static_cast<int>(payloadIndex - 1);

            if (dataBank == nullptr) {
                continue;
            }

            auto hits = decodeFadc250Payload(result.frameTimestamp, crate, slot,
                                             dataBank->getRawBytes(),
                                             dataBank->getByteOrder());
            appendVisibleHits(hits, options, result.visibleHits);
        }
    }

    return result;
}

uint64_t safeFileSize(const std::string &path) {
    std::error_code ec;
    uint64_t size = std::filesystem::file_size(path, ec);
    return ec ? 0 : size;
}

void updateTimestampStats(const FrameResult &frame, RunStats &stats) {
    if (!frame.haveFrameInfo) {
        return;
    }

    stats.frameNumberSeen = true;
    stats.minFrameNumber = std::min(stats.minFrameNumber, frame.frameNumber);
    stats.maxFrameNumber = std::max(stats.maxFrameNumber, frame.frameNumber);

    stats.timestampSeen = true;
    stats.minTimestamp = std::min(stats.minTimestamp, frame.frameTimestamp);
    stats.maxTimestamp = std::max(stats.maxTimestamp, frame.frameTimestamp);
}

void addFrameStats(const FrameResult &frame, RunStats &stats) {
    ++stats.frames;
    stats.visibleHits += frame.visibleHits.size();
    stats.clusters += frame.clusters.size();
    if (!frame.clusters.empty()) {
        ++stats.framesWithClusters;
    }
    for (const auto &cluster : frame.clusters) {
        stats.associatedHits += cluster.hits.size();
    }
    updateTimestampStats(frame, stats);
}

void mergeStats(const RunStats &src, RunStats &dest) {
    dest.inputFiles += src.inputFiles;
    dest.inputBytes += src.inputBytes;
    dest.eventBytes += src.eventBytes;
    dest.events += src.events;
    dest.controlEvents += src.controlEvents;
    dest.frames += src.frames;
    dest.builtStreamingEvents += src.builtStreamingEvents;
    dest.malformedFrames += src.malformedFrames;
    dest.framesWithClusters += src.framesWithClusters;
    dest.visibleHits += src.visibleHits;
    dest.clusters += src.clusters;
    dest.associatedHits += src.associatedHits;

    if (src.frameNumberSeen) {
        dest.frameNumberSeen = true;
        dest.minFrameNumber = std::min(dest.minFrameNumber, src.minFrameNumber);
        dest.maxFrameNumber = std::max(dest.maxFrameNumber, src.maxFrameNumber);
    }
    if (src.timestampSeen) {
        dest.timestampSeen = true;
        dest.minTimestamp = std::min(dest.minTimestamp, src.minTimestamp);
        dest.maxTimestamp = std::max(dest.maxTimestamp, src.maxTimestamp);
    }
}

double secondsForFrames(uint64_t frames, const ClusterScanConfig &config) {
    return static_cast<double>(frames) * static_cast<double>(config.timeframeNs) * 1.0e-9;
}

double divideOrZero(uint64_t numerator, double denominator) {
    return denominator > 0.0 ? static_cast<double>(numerator) / denominator : 0.0;
}

void printFrameDebugHeader(const std::string &path,
                           uint64_t eventIndex,
                           uint16_t eventTag,
                           const FrameResult &frame,
                           const ClusterScanConfig &config) {
    const double frameSeconds = secondsForFrames(1, config);
    const double eventRateMBps = divideOrZero(frame.eventBytes, frameSeconds) / 1.0e6;

    std::cout << "FRAME file=" << path
              << " event=" << eventIndex
              << " tag=" << formatHex16(eventTag);
    if (frame.haveFrameInfo) {
        std::cout << " frame=" << frame.frameNumber
                  << " timestamp=" << frame.frameTimestamp;
    }
    else {
        std::cout << " frame=n/a timestamp=n/a";
    }
    std::cout << " eventBytes=" << frame.eventBytes
              << " eventRateMBps=" << formatDouble(eventRateMBps, 3)
              << " visibleHits=" << frame.visibleHits.size()
              << '\n';
}

void printFrameDebugSummary(const FrameResult &frame,
                            const ClusterScanConfig &config) {
    const uint64_t frameStart = frame.haveFrameInfo ? frame.frameTimestamp : 0;
    std::cout << "  FRAME_SUMMARY frameStart=" << frameStart
              << " frameEnd=" << (frameStart + config.timeframeNs)
              << " visibleHits=" << frame.visibleHits.size()
              << " clusters=" << frame.clusters.size()
              << '\n';
    printClusterDetails(frame.clusters, frameStart);
}

bool waitForPrompt(const std::string &prompt) {
    std::cout << prompt;
    std::cout.flush();

    bool sawNewline = false;
    char buffer[256];

    while (!gQuit.load() && !sawNewline) {
        struct pollfd pfd{};
        pfd.fd = STDIN_FILENO;
        pfd.events = POLLIN;

        int rc = poll(&pfd, 1, 200);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "\nError waiting for keyboard input\n";
            return false;
        }

        if (rc == 0) {
            continue;
        }

        if ((pfd.revents & POLLIN) == 0) {
            continue;
        }

        ssize_t nread = read(STDIN_FILENO, buffer, sizeof(buffer));
        if (nread < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "\nError reading keyboard input\n";
            return false;
        }

        if (nread == 0) {
            std::cout << '\n';
            return false;
        }

        for (ssize_t i = 0; i < nread; ++i) {
            if (buffer[i] == '\n') {
                sawNewline = true;
                break;
            }
        }
    }

    if (gQuit.load()) {
        std::cout << '\n';
        return false;
    }

    return true;
}

void scanFile(const std::string &path,
              const Options &options,
              const ClusterScanConfig &config,
              RunStats &totals) {
    RunStats fileStats;
    fileStats.inputFiles = 1;
    fileStats.inputBytes = safeFileSize(path);

    EvioReader reader(path);
    std::shared_ptr<EvioEvent> event;
    uint64_t eventIndex = 0;

    while (!gQuit.load() && (event = reader.parseNextEvent())) {
        ++eventIndex;
        ++fileStats.events;

        const uint64_t eventBytes = event ? event->getTotalBytes() : 0;
        fileStats.eventBytes += eventBytes;

        auto header = event ? event->getHeader() : nullptr;
        uint16_t eventTag = header ? header->getTag() : 0;

        if (eventTag == TAG_PRESTART || eventTag == TAG_GO || eventTag == TAG_END) {
            ++fileStats.controlEvents;
            continue;
        }

        if (eventTag == TAG_BUILT_STREAMING) {
            ++fileStats.builtStreamingEvents;
        }

        try {
            FrameResult frame = decodeStreamingFrame(event, options);
            frame.eventBytes = eventBytes;
            const uint64_t frameStart = frame.haveFrameInfo ? frame.frameTimestamp : 0;

            if (options.clusterDebug) {
                printFrameDebugHeader(path, eventIndex, eventTag, frame, config);
            }

            ClusterDebugCallback debugCallback;
            if (options.clusterDebug) {
                debugCallback = [](const ClusterDebugStep &step) {
                    printClusterDebugStep(step);
                    bool keepGoing = waitForPrompt("\nHit <Enter> for next cluster window (Ctrl+C to quit)");
                    if (!keepGoing) {
                        gQuit.store(true);
                    }
                    return keepGoing;
                };
            }

            frame.clusters = findSlidingWindowClusters(frame.visibleHits, frameStart,
                                                       config, debugCallback);
            addFrameStats(frame, fileStats);

            if (options.clusterDebug && !gQuit.load()) {
                printFrameDebugSummary(frame, config);
            }
        }
        catch (const std::exception &e) {
            ++fileStats.malformedFrames;
            if (options.clusterDebug) {
                std::cout << "FRAME file=" << path
                          << " event=" << eventIndex
                          << " tag=" << formatHex16(eventTag)
                          << " malformed=" << e.what() << '\n';
            }
        }
    }

    mergeStats(fileStats, totals);
}

void printOptionalRanges(const RunStats &stats) {
    if (stats.frameNumberSeen) {
        std::cout << " frameMin=" << stats.minFrameNumber
                  << " frameMax=" << stats.maxFrameNumber;
    }
    else {
        std::cout << " frameMin=n/a frameMax=n/a";
    }

    if (stats.timestampSeen) {
        std::cout << " timestampMin=" << stats.minTimestamp
                  << " timestampMax=" << stats.maxTimestamp;
    }
    else {
        std::cout << " timestampMin=n/a timestampMax=n/a";
    }
}

void printFinalStats(const RunStats &stats, const ClusterScanConfig &config) {
    const double liveSeconds = secondsForFrames(stats.frames, config);
    const double clustersPerFrame = stats.frames > 0
        ? static_cast<double>(stats.clusters) / static_cast<double>(stats.frames)
        : 0.0;
    const double clustersPerMillionFrames = clustersPerFrame * 1.0e6;
    const double clusterRateHz = divideOrZero(stats.clusters, liveSeconds);
    const double inputRateMBps = divideOrZero(stats.inputBytes, liveSeconds) / 1.0e6;
    const double eventRateMBps = divideOrZero(stats.eventBytes, liveSeconds) / 1.0e6;

    std::cout << "FINAL events=" << stats.events
              << " controlEvents=" << stats.controlEvents
              << " frames=" << stats.frames
              << " builtStreamingEvents=" << stats.builtStreamingEvents
              << " malformedFrames=" << stats.malformedFrames;
    printOptionalRanges(stats);
    std::cout << '\n';

    std::cout << "FINAL_CLUSTER_STATS clusters=" << stats.clusters
              << " framesWithClusters=" << stats.framesWithClusters
              << " visibleHits=" << stats.visibleHits
              << " associatedHits=" << stats.associatedHits
              << " clustersPerFrame=" << formatDouble(clustersPerFrame)
              << " clustersPerMillionFrames=" << formatDouble(clustersPerMillionFrames, 3)
              << " clusterRateHz=" << formatDouble(clusterRateHz, 3)
              << '\n';

    std::cout << "FINAL_TIME_STATS timeframeNs=" << config.timeframeNs
              << " liveSeconds=" << formatDouble(liveSeconds, 6)
              << '\n';

    std::cout << "FINAL_DATA_RATE inputFiles=" << stats.inputFiles
              << " inputBytes=" << stats.inputBytes
              << " eventBytes=" << stats.eventBytes
              << " inputRateMBps=" << formatDouble(inputRateMBps, 3)
              << " eventRateMBps=" << formatDouble(eventRateMBps, 3)
              << " inputBytesPerFrame=" << formatDouble(stats.frames > 0
                     ? static_cast<double>(stats.inputBytes) / static_cast<double>(stats.frames)
                     : 0.0, 3)
              << " eventBytesPerFrame=" << formatDouble(stats.frames > 0
                     ? static_cast<double>(stats.eventBytes) / static_cast<double>(stats.frames)
                     : 0.0, 3)
              << '\n';
}

} // namespace

int main(int argc, char *argv[]) {
    struct sigaction sa{};
    sa.sa_handler = ctrlCHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);

    try {
        Options options = parseArgs(argc, argv);
        ClusterScanConfig config;

        RunStats totals;
        for (const auto &path : options.inputFiles) {
            if (gQuit.load()) {
                break;
            }
            scanFile(path, options, config, totals);
        }

        if (gQuit.load()) {
            std::cout << "\nStop requested, printing stats collected so far.\n";
        }

        printFinalStats(totals, config);
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}
