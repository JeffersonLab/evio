#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <condition_variable>
#include <deque>
#include <exception>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <poll.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <unistd.h>
#include <utility>
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
    uint32_t channelKey = 0;
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
    bool timing = false;
    bool validateClusterSearch = false;
    bool storeClusterHits = false;
    bool adaptiveHotChannels = false;
    bool weightedClusterScore = false;
    size_t hotChannelWindowFrames = 1024;
    double hotChannelRateThreshold = 8.0;
    double weightedScoreThreshold = 3.0;
    double hotChannelWeight = 0.1;
    size_t threads = 1;
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
    uint32_t nHits = 0;
    uint32_t nChannels = 0;
    uint64_t summedCharge = 0;
    double chargeWeightedOffset = 0.0;
    std::vector<Hit> hits;
};

struct HitLite {
    uint16_t channelId = 0;
    uint16_t charge = 0;
    uint16_t t4 = 0;
    uint32_t originalIndex = 0;
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

struct TimingStats {
    uint64_t parseNs = 0;
    uint64_t decodeNs = 0;
    uint64_t clusterNs = 0;
    uint64_t statsNs = 0;
    uint64_t totalNs = 0;
};

struct ClusterSearchOptions {
    bool storeClusterHits = false;
    bool adaptiveHotChannels = false;
    bool weightedClusterScore = false;
    double weightedScoreThreshold = 3.0;
    double hotChannelWeight = 0.1;
};

using HotChannelSet = std::unordered_set<uint32_t>;
using HotChannelSetPtr = std::shared_ptr<const HotChannelSet>;

struct ClusterSearchWorkspace {
    std::vector<HitLite> hits;
    std::unordered_map<uint32_t, uint16_t> denseChannelByKey;
    std::vector<uint32_t> channelKeys;
    std::vector<uint16_t> activeCounts;
    std::vector<uint16_t> associatedChannelCounts;
    std::vector<uint8_t> associatedFlags;
    std::vector<uint8_t> hotChannels;
    std::vector<size_t> associatedIndices;
    std::vector<uint16_t> touchedAssociatedChannels;
};

struct ValidationStats {
    uint64_t framesChecked = 0;
    uint64_t mismatchedFrames = 0;
};

using ClusterDebugCallback = std::function<bool(const ClusterDebugStep &)>;

bool waitForPrompt(const std::string &prompt);

void ctrlCHandler(int) {
    gQuit.store(true);
}

void printUsage(const char *name) {
    std::cerr << "Usage: " << name
              << " [--cluster-debug] [--timing] [--threads N]"
              << " [--validate-cluster-search] [--store-cluster-hits]"
              << " [--adaptive-hot-channels] [--hot-channel-window-frames N]"
              << " [--hot-channel-rate-threshold X]"
              << " [--weighted-cluster-score] [--weighted-score-threshold X]"
              << " [--mask crate,slot,channel] file1.evio [file2.evio ...]\n";
}

size_t parsePositiveSize(const std::string &text, const std::string &optionName) {
    size_t consumed = 0;
    unsigned long long value = std::stoull(text, &consumed);
    if (consumed != text.size() || value == 0) {
        throw std::runtime_error(optionName + " must be a positive integer");
    }
    if (value > static_cast<unsigned long long>(std::numeric_limits<size_t>::max())) {
        throw std::runtime_error(optionName + " is too large");
    }
    return static_cast<size_t>(value);
}

double parsePositiveDouble(const std::string &text, const std::string &optionName) {
    size_t consumed = 0;
    double value = std::stod(text, &consumed);
    if (consumed != text.size() || value <= 0.0) {
        throw std::runtime_error(optionName + " must be a positive number");
    }
    return value;
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

        if (arg == "--timing" || arg == "--profile-stages") {
            options.timing = true;
            continue;
        }

        if (arg == "--validate-cluster-search") {
            options.validateClusterSearch = true;
            continue;
        }

        if (arg == "--store-cluster-hits") {
            options.storeClusterHits = true;
            continue;
        }

        if (arg == "--adaptive-hot-channels") {
            options.adaptiveHotChannels = true;
            continue;
        }

        if (arg == "--weighted-cluster-score") {
            options.weightedClusterScore = true;
            options.adaptiveHotChannels = true;
            continue;
        }

        if (arg == "--threads") {
            if (i + 1 >= argc) {
                throw std::runtime_error("missing argument after --threads");
            }
            options.threads = parsePositiveSize(argv[++i], "--threads");
            continue;
        }

        if (arg.rfind("--threads=", 0) == 0) {
            options.threads = parsePositiveSize(arg.substr(10), "--threads");
            continue;
        }

        if (arg == "--hot-channel-window-frames") {
            if (i + 1 >= argc) {
                throw std::runtime_error("missing argument after --hot-channel-window-frames");
            }
            options.hotChannelWindowFrames = parsePositiveSize(argv[++i], "--hot-channel-window-frames");
            continue;
        }

        if (arg.rfind("--hot-channel-window-frames=", 0) == 0) {
            options.hotChannelWindowFrames = parsePositiveSize(arg.substr(28), "--hot-channel-window-frames");
            continue;
        }

        if (arg == "--hot-channel-rate-threshold") {
            if (i + 1 >= argc) {
                throw std::runtime_error("missing argument after --hot-channel-rate-threshold");
            }
            options.hotChannelRateThreshold = parsePositiveDouble(argv[++i], "--hot-channel-rate-threshold");
            continue;
        }

        if (arg.rfind("--hot-channel-rate-threshold=", 0) == 0) {
            options.hotChannelRateThreshold = parsePositiveDouble(arg.substr(29), "--hot-channel-rate-threshold");
            continue;
        }

        if (arg == "--weighted-score-threshold") {
            if (i + 1 >= argc) {
                throw std::runtime_error("missing argument after --weighted-score-threshold");
            }
            options.weightedScoreThreshold = parsePositiveDouble(argv[++i], "--weighted-score-threshold");
            continue;
        }

        if (arg.rfind("--weighted-score-threshold=", 0) == 0) {
            options.weightedScoreThreshold = parsePositiveDouble(arg.substr(27), "--weighted-score-threshold");
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

uint32_t packChannel(int crate, int slot, int channel) {
    // FADC slot/channel values are small. Keeping crate in the full high
    // 16 bits makes the hot loop compare one integer without losing EVIO tag
    // range in normal Hall-B SRO payloads.
    return ((static_cast<uint32_t>(crate) & 0xffffU) << 16U) |
           ((static_cast<uint32_t>(slot) & 0x00ffU) << 8U) |
           (static_cast<uint32_t>(channel) & 0x00ffU);
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

uint32_t countDistinctPackedChannels(const std::vector<Hit> &hits) {
    std::vector<uint32_t> channels;
    channels.reserve(hits.size());

    for (const auto &hit : hits) {
        const uint32_t key = hit.channelKey != 0 ? hit.channelKey : packChannel(hit.crate, hit.slot, hit.channel);
        if (std::find(channels.begin(), channels.end(), key) == channels.end()) {
            channels.push_back(key);
        }
    }

    return static_cast<uint32_t>(channels.size());
}

void populateClusterSummaryFromHits(Cluster &cluster, uint64_t frameStart) {
    cluster.nHits = static_cast<uint32_t>(cluster.hits.size());
    cluster.nChannels = countDistinctPackedChannels(cluster.hits);

    uint64_t totalCharge = 0;
    long double weightedOffset = 0.0L;
    long double offsetSum = 0.0L;

    for (const auto &hit : cluster.hits) {
        const uint64_t offset = (hit.time >= frameStart) ? (hit.time - frameStart) : 0;
        offsetSum += static_cast<long double>(offset);
        if (hit.charge <= 0) {
            continue;
        }
        const uint64_t charge = static_cast<uint64_t>(hit.charge);
        totalCharge += charge;
        weightedOffset += static_cast<long double>(offset) * static_cast<long double>(charge);
    }

    cluster.summedCharge = totalCharge;
    if (totalCharge > 0) {
        cluster.chargeWeightedOffset = static_cast<double>(weightedOffset / static_cast<long double>(totalCharge));
    }
    else if (!cluster.hits.empty()) {
        cluster.chargeWeightedOffset = static_cast<double>(offsetSum / static_cast<long double>(cluster.hits.size()));
    }
    else {
        cluster.chargeWeightedOffset = 0.0;
    }
}

uint32_t clusterAssociatedHits(const Cluster &cluster) {
    return cluster.nHits != 0 ? cluster.nHits : static_cast<uint32_t>(cluster.hits.size());
}

uint32_t clusterAssociatedChannels(const Cluster &cluster) {
    return cluster.nChannels != 0 ? cluster.nChannels : countDistinctPackedChannels(cluster.hits);
}

bool emitClusterDebug(const ClusterDebugCallback &debugCallback,
                      const ClusterDebugStep &step) {
    if (!debugCallback) {
        return true;
    }
    return debugCallback(step);
}

std::vector<Cluster> findSlidingWindowClustersLegacy(const std::vector<Hit> &hits,
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
        populateClusterSummaryFromHits(cluster, frameStart);
        clusters.push_back(cluster);

        left = cluster.finalRight;
    }

    return clusters;
}

uint64_t hitOffsetNs(const HitLite &hit) {
    return static_cast<uint64_t>(hit.t4) * 4U;
}

uint64_t alignUpToStep(uint64_t value, uint64_t step) {
    if (step == 0) {
        return value;
    }
    const uint64_t remainder = value % step;
    return remainder == 0 ? value : value + (step - remainder);
}

uint64_t firstSearchLeftThatCanContain(uint64_t hitOffset, const ClusterScanConfig &config) {
    if (hitOffset < config.windowNs) {
        return 0;
    }

    // Search windows use [left, left + windowNs), so a hit exactly on the
    // right edge is not included. Add one nanosecond before aligning upward.
    return alignUpToStep(hitOffset - config.windowNs + 1, config.stepNs);
}

uint16_t clampChargeToU16(int charge) {
    if (charge <= 0) {
        return 0;
    }
    if (charge > static_cast<int>(std::numeric_limits<uint16_t>::max())) {
        return std::numeric_limits<uint16_t>::max();
    }
    return static_cast<uint16_t>(charge);
}

void prepareClusterWorkspace(const std::vector<Hit> &hits,
                             uint64_t frameStart,
                             const ClusterScanConfig &config,
                             const HotChannelSetPtr &hotChannels,
                             ClusterSearchWorkspace &workspace) {
    workspace.hits.clear();
    workspace.denseChannelByKey.clear();
    workspace.channelKeys.clear();

    workspace.hits.reserve(hits.size());
    workspace.denseChannelByKey.reserve(hits.size());
    workspace.channelKeys.reserve(hits.size());

    const uint64_t frameEnd = frameStart + config.timeframeNs;

    for (size_t i = 0; i < hits.size(); ++i) {
        const auto &hit = hits[i];
        if (hit.time < frameStart || hit.time >= frameEnd) {
            continue;
        }
        if (i > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
            throw std::runtime_error("too many hits in one frame for compact cluster workspace");
        }

        const uint32_t channelKey = hit.channelKey != 0 ? hit.channelKey : packChannel(hit.crate, hit.slot, hit.channel);
        auto denseIt = workspace.denseChannelByKey.find(channelKey);
        uint16_t denseChannel = 0;
        if (denseIt == workspace.denseChannelByKey.end()) {
            if (workspace.channelKeys.size() > static_cast<size_t>(std::numeric_limits<uint16_t>::max())) {
                throw std::runtime_error("too many distinct channels in one frame for compact cluster workspace");
            }
            denseChannel = static_cast<uint16_t>(workspace.channelKeys.size());
            workspace.denseChannelByKey.emplace(channelKey, denseChannel);
            workspace.channelKeys.push_back(channelKey);
        }
        else {
            denseChannel = denseIt->second;
        }

        const uint64_t offsetNs = hit.time - frameStart;
        HitLite lite;
        lite.channelId = denseChannel;
        lite.charge = clampChargeToU16(hit.charge);
        lite.t4 = static_cast<uint16_t>(std::min<uint64_t>(offsetNs / 4U,
                                                           std::numeric_limits<uint16_t>::max()));
        lite.originalIndex = static_cast<uint32_t>(i);
        workspace.hits.push_back(lite);
    }

    std::sort(workspace.hits.begin(), workspace.hits.end(),
              [&hits](const HitLite &a, const HitLite &b) {
                  return hitLess(hits[a.originalIndex], hits[b.originalIndex]);
              });

    const size_t channelCount = workspace.channelKeys.size();
    workspace.activeCounts.assign(channelCount, 0);
    workspace.associatedChannelCounts.assign(channelCount, 0);
    workspace.hotChannels.assign(channelCount, 0);
    workspace.associatedFlags.assign(workspace.hits.size(), 0);
    workspace.associatedIndices.clear();
    workspace.touchedAssociatedChannels.clear();

    if (hotChannels) {
        for (size_t i = 0; i < workspace.channelKeys.size(); ++i) {
            if (hotChannels->find(workspace.channelKeys[i]) != hotChannels->end()) {
                workspace.hotChannels[i] = 1;
            }
        }
    }
}

double seedChannelWeight(const ClusterSearchWorkspace &workspace,
                         uint16_t channelId,
                         const ClusterSearchOptions &searchOptions) {
    if (workspace.hotChannels[channelId] != 0) {
        return searchOptions.hotChannelWeight;
    }
    return 1.0;
}

void addActiveHit(const ClusterSearchWorkspace &workspace,
                  const HitLite &hit,
                  const ClusterSearchOptions &searchOptions,
                  uint32_t &activeDistinctChannels,
                  uint32_t &activeHotChannels,
                  double &activeWeightedScore,
                  std::vector<uint16_t> &activeCounts) {
    uint16_t &count = activeCounts[hit.channelId];
    if (count == 0) {
        ++activeDistinctChannels;
        if (workspace.hotChannels[hit.channelId] != 0) {
            ++activeHotChannels;
        }
        if (searchOptions.weightedClusterScore) {
            activeWeightedScore += seedChannelWeight(workspace, hit.channelId, searchOptions);
        }
    }
    ++count;
}

void removeActiveHit(const ClusterSearchWorkspace &workspace,
                     const HitLite &hit,
                     const ClusterSearchOptions &searchOptions,
                     uint32_t &activeDistinctChannels,
                     uint32_t &activeHotChannels,
                     double &activeWeightedScore,
                     std::vector<uint16_t> &activeCounts) {
    uint16_t &count = activeCounts[hit.channelId];
    if (count == 0) {
        return;
    }
    --count;
    if (count == 0) {
        --activeDistinctChannels;
        if (workspace.hotChannels[hit.channelId] != 0) {
            --activeHotChannels;
        }
        if (searchOptions.weightedClusterScore) {
            activeWeightedScore -= seedChannelWeight(workspace, hit.channelId, searchOptions);
        }
    }
}

bool seedConditionMet(const ClusterSearchOptions &searchOptions,
                      const ClusterScanConfig &config,
                      uint32_t activeDistinctChannels,
                      uint32_t activeHotChannels,
                      double activeWeightedScore) {
    if (searchOptions.weightedClusterScore) {
        return activeWeightedScore >= searchOptions.weightedScoreThreshold;
    }

    if (searchOptions.adaptiveHotChannels) {
        return (activeDistinctChannels - activeHotChannels) >= config.minChannels;
    }

    return activeDistinctChannels >= config.minChannels;
}

uint32_t countDistinctLiteChannels(const ClusterSearchWorkspace &workspace,
                                   size_t begin,
                                   size_t end) {
    std::vector<uint16_t> channels;
    channels.reserve(end - begin);

    for (size_t i = begin; i < end; ++i) {
        const uint16_t channelId = workspace.hits[i].channelId;
        if (std::find(channels.begin(), channels.end(), channelId) == channels.end()) {
            channels.push_back(channelId);
        }
    }

    return static_cast<uint32_t>(channels.size());
}

struct ClusterAssociationAccumulator {
    uint32_t nHits = 0;
    uint32_t nChannels = 0;
    uint64_t summedCharge = 0;
    long double weightedOffset = 0.0L;
    long double offsetSum = 0.0L;
};

void addLiteAssociations(ClusterSearchWorkspace &workspace,
                         size_t begin,
                         size_t end,
                         ClusterAssociationAccumulator &accumulator) {
    for (size_t i = begin; i < end; ++i) {
        if (workspace.associatedFlags[i] != 0) {
            continue;
        }

        const auto &hit = workspace.hits[i];
        workspace.associatedFlags[i] = 1;
        workspace.associatedIndices.push_back(i);

        ++accumulator.nHits;
        const uint64_t offsetNs = hitOffsetNs(hit);
        accumulator.offsetSum += static_cast<long double>(offsetNs);

        if (hit.charge > 0) {
            accumulator.summedCharge += hit.charge;
            accumulator.weightedOffset += static_cast<long double>(offsetNs) *
                                          static_cast<long double>(hit.charge);
        }

        uint16_t &channelCount = workspace.associatedChannelCounts[hit.channelId];
        if (channelCount == 0) {
            ++accumulator.nChannels;
            workspace.touchedAssociatedChannels.push_back(hit.channelId);
        }
        ++channelCount;
    }
}

void clearLiteAssociations(ClusterSearchWorkspace &workspace) {
    for (size_t index : workspace.associatedIndices) {
        workspace.associatedFlags[index] = 0;
    }
    for (uint16_t channelId : workspace.touchedAssociatedChannels) {
        workspace.associatedChannelCounts[channelId] = 0;
    }
    workspace.associatedIndices.clear();
    workspace.touchedAssociatedChannels.clear();
}

void fillClusterFromAccumulator(Cluster &cluster,
                                const ClusterAssociationAccumulator &accumulator) {
    cluster.nHits = accumulator.nHits;
    cluster.nChannels = accumulator.nChannels;
    cluster.summedCharge = accumulator.summedCharge;

    if (accumulator.summedCharge > 0) {
        cluster.chargeWeightedOffset = static_cast<double>(
            accumulator.weightedOffset / static_cast<long double>(accumulator.summedCharge));
    }
    else if (accumulator.nHits > 0) {
        cluster.chargeWeightedOffset = static_cast<double>(
            accumulator.offsetSum / static_cast<long double>(accumulator.nHits));
    }
    else {
        cluster.chargeWeightedOffset = 0.0;
    }
}

void maybeStoreClusterHits(const std::vector<Hit> &sourceHits,
                           const ClusterSearchWorkspace &workspace,
                           Cluster &cluster,
                           bool storeClusterHits) {
    if (!storeClusterHits) {
        return;
    }

    cluster.hits.clear();
    cluster.hits.reserve(workspace.associatedIndices.size());
    for (size_t liteIndex : workspace.associatedIndices) {
        cluster.hits.push_back(sourceHits[workspace.hits[liteIndex].originalIndex]);
    }
    std::sort(cluster.hits.begin(), cluster.hits.end(), hitLess);
}

std::vector<Cluster> findSlidingWindowClusters(const std::vector<Hit> &hits,
                                               uint64_t frameStart,
                                               const ClusterScanConfig &config,
                                               const ClusterSearchOptions &searchOptions,
                                               const HotChannelSetPtr &hotChannels,
                                               ClusterSearchWorkspace &workspace,
                                               const ClusterDebugCallback &debugCallback) {
    if (config.windowNs == 0 || config.stepNs == 0 || config.minChannels == 0) {
        throw std::runtime_error("cluster scan configuration must use non-zero window, step, and threshold");
    }
    if (config.windowNs > config.timeframeNs) {
        throw std::runtime_error("cluster scan window cannot be larger than the timeframe");
    }

    prepareClusterWorkspace(hits, frameStart, config, hotChannels, workspace);

    std::vector<Cluster> clusters;
    if (workspace.hits.empty()) {
        return clusters;
    }

    const uint64_t lastSearchLeftOffset = config.timeframeNs - config.windowNs;
    uint64_t leftOffset = 0;

    size_t activeLeft = 0;
    size_t activeRight = 0;
    uint32_t activeDistinctChannels = 0;
    uint32_t activeHotChannels = 0;
    double activeWeightedScore = 0.0;

    while (leftOffset <= lastSearchLeftOffset) {
        const uint64_t rightOffset = leftOffset + config.windowNs;

        // The seed scan is a sweep line: activeRight only moves forward as
        // windows advance, and activeLeft only moves forward as hits age out.
        // Therefore each hit enters and leaves the active seed window once.
        while (activeLeft < activeRight &&
               hitOffsetNs(workspace.hits[activeLeft]) < leftOffset) {
            removeActiveHit(workspace, workspace.hits[activeLeft], searchOptions,
                            activeDistinctChannels, activeHotChannels,
                            activeWeightedScore, workspace.activeCounts);
            ++activeLeft;
        }
        while (activeRight < workspace.hits.size() &&
               hitOffsetNs(workspace.hits[activeRight]) < leftOffset) {
            ++activeRight;
            activeLeft = activeRight;
        }
        while (activeRight < workspace.hits.size() &&
               hitOffsetNs(workspace.hits[activeRight]) < rightOffset) {
            addActiveHit(workspace, workspace.hits[activeRight], searchOptions,
                         activeDistinctChannels, activeHotChannels,
                         activeWeightedScore, workspace.activeCounts);
            ++activeRight;
        }

        const bool seedFound = seedConditionMet(searchOptions, config,
                                                activeDistinctChannels,
                                                activeHotChannels,
                                                activeWeightedScore);

        if (debugCallback) {
            ClusterDebugStep searchStep;
            searchStep.phase = "search";
            searchStep.frameStart = frameStart;
            searchStep.left = frameStart + leftOffset;
            searchStep.right = frameStart + rightOffset;
            searchStep.windowHits = activeRight - activeLeft;
            searchStep.windowChannels = activeDistinctChannels;
            searchStep.associatedHits = seedFound ? (activeRight - activeLeft) : 0;
            searchStep.associatedChannels = seedFound ? activeDistinctChannels : 0;
            searchStep.seedFound = seedFound;
            if (!debugCallback(searchStep)) {
                return clusters;
            }
        }

        if (!seedFound) {
            if (!debugCallback && activeDistinctChannels == 0) {
                if (activeRight >= workspace.hits.size()) {
                    break;
                }

                const uint64_t nextHitOffset = hitOffsetNs(workspace.hits[activeRight]);
                const uint64_t nextUsefulLeft =
                    firstSearchLeftThatCanContain(nextHitOffset, config);
                if (nextUsefulLeft > leftOffset) {
                    leftOffset = std::min(nextUsefulLeft, lastSearchLeftOffset);
                    continue;
                }
            }

            if (leftOffset + config.stepNs > lastSearchLeftOffset) {
                break;
            }
            leftOffset += config.stepNs;
            continue;
        }

        Cluster cluster;
        cluster.seedLeft = frameStart + leftOffset;
        cluster.seedRight = frameStart + rightOffset;

        ClusterAssociationAccumulator accumulator;
        clearLiteAssociations(workspace);
        addLiteAssociations(workspace, activeLeft, activeRight, accumulator);

        const uint64_t earliestAssociatedOffset =
            (activeLeft < activeRight) ? hitOffsetNs(workspace.hits[activeLeft]) : leftOffset;

        uint64_t trackLeftOffset = leftOffset;
        uint64_t trackRightOffset = rightOffset;
        size_t trackLeftIndex = activeLeft;
        size_t trackRightIndex = activeRight;

        while (trackLeftOffset + config.stepNs <= earliestAssociatedOffset &&
               trackRightOffset + config.stepNs <= config.timeframeNs) {
            trackLeftOffset += config.stepNs;
            trackRightOffset += config.stepNs;

            while (trackRightIndex < workspace.hits.size() &&
                   hitOffsetNs(workspace.hits[trackRightIndex]) < trackRightOffset) {
                ++trackRightIndex;
            }
            while (trackLeftIndex < trackRightIndex &&
                   hitOffsetNs(workspace.hits[trackLeftIndex]) < trackLeftOffset) {
                ++trackLeftIndex;
            }

            addLiteAssociations(workspace, trackLeftIndex, trackRightIndex, accumulator);

            if (debugCallback) {
                ClusterDebugStep trackStep;
                trackStep.phase = "track";
                trackStep.frameStart = frameStart;
                trackStep.left = frameStart + trackLeftOffset;
                trackStep.right = frameStart + trackRightOffset;
                trackStep.windowHits = trackRightIndex - trackLeftIndex;
                trackStep.windowChannels = countDistinctLiteChannels(workspace, trackLeftIndex, trackRightIndex);
                trackStep.associatedHits = accumulator.nHits;
                trackStep.associatedChannels = accumulator.nChannels;
                trackStep.seedFound = true;
                if (!debugCallback(trackStep)) {
                    return clusters;
                }
            }
        }

        cluster.frozenLeft = frameStart + trackLeftOffset;

        const uint64_t extensionTargetOffset =
            std::min(config.timeframeNs, trackRightOffset + config.extensionNs);
        uint64_t extendedRightOffset = trackRightOffset;
        size_t extendRightIndex = trackRightIndex;

        while (extendedRightOffset < extensionTargetOffset) {
            extendedRightOffset = std::min(extensionTargetOffset,
                                           extendedRightOffset + config.stepNs);

            while (extendRightIndex < workspace.hits.size() &&
                   hitOffsetNs(workspace.hits[extendRightIndex]) < extendedRightOffset) {
                ++extendRightIndex;
            }

            addLiteAssociations(workspace, trackLeftIndex, extendRightIndex, accumulator);

            if (debugCallback) {
                ClusterDebugStep extendStep;
                extendStep.phase = "extend";
                extendStep.frameStart = frameStart;
                extendStep.left = frameStart + trackLeftOffset;
                extendStep.right = frameStart + extendedRightOffset;
                extendStep.windowHits = extendRightIndex - trackLeftIndex;
                extendStep.windowChannels = countDistinctLiteChannels(workspace, trackLeftIndex, extendRightIndex);
                extendStep.associatedHits = accumulator.nHits;
                extendStep.associatedChannels = accumulator.nChannels;
                extendStep.seedFound = true;
                if (!debugCallback(extendStep)) {
                    return clusters;
                }
            }
        }

        cluster.finalRight = frameStart + extendedRightOffset;
        fillClusterFromAccumulator(cluster, accumulator);
        maybeStoreClusterHits(hits, workspace, cluster,
                              searchOptions.storeClusterHits || static_cast<bool>(debugCallback));
        clusters.push_back(std::move(cluster));
        clearLiteAssociations(workspace);

        leftOffset = extendedRightOffset;
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
                  << " hits=" << clusterAssociatedHits(cluster)
                  << " channels=" << clusterAssociatedChannels(cluster)
                  << " summedCharge=" << cluster.summedCharge
                  << " chargeWeightedOffset=" << formatDouble(cluster.chargeWeightedOffset, 2)
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
        hit.channelKey = packChannel(hit.crate, hit.slot, hit.channel);
        hits.push_back(hit);
    }

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
        stats.associatedHits += clusterAssociatedHits(cluster);
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

void mergeTiming(const TimingStats &src, TimingStats &dest) {
    dest.parseNs += src.parseNs;
    dest.decodeNs += src.decodeNs;
    dest.clusterNs += src.clusterNs;
    dest.statsNs += src.statsNs;
}

class ScopedStageTimer {
public:
    ScopedStageTimer(bool enabled, uint64_t &accumulator)
        : enabled_(enabled), accumulator_(accumulator) {
        if (enabled_) {
            start_ = std::chrono::steady_clock::now();
        }
    }

    ~ScopedStageTimer() {
        if (!enabled_) {
            return;
        }
        const auto stop = std::chrono::steady_clock::now();
        accumulator_ += static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start_).count());
    }

private:
    bool enabled_ = false;
    uint64_t &accumulator_;
    std::chrono::steady_clock::time_point start_;
};

double secondsFromNs(uint64_t ns) {
    return static_cast<double>(ns) * 1.0e-9;
}

class HotChannelTracker {
public:
    HotChannelTracker(bool enabled, size_t windowFrames, double rateThreshold)
        : enabled_(enabled), windowFrames_(windowFrames), rateThreshold_(rateThreshold) {}

    HotChannelSetPtr updateAndSnapshot(const std::vector<Hit> &hits) {
        if (!enabled_) {
            return nullptr;
        }

        std::unordered_map<uint32_t, uint32_t> frameCounts;
        frameCounts.reserve(hits.size());
        for (const auto &hit : hits) {
            const uint32_t key = hit.channelKey != 0 ? hit.channelKey : packChannel(hit.crate, hit.slot, hit.channel);
            ++frameCounts[key];
        }

        for (const auto &entry : frameCounts) {
            channelWindowTotals_[entry.first] += entry.second;
        }
        recentFrames_.push_back(std::move(frameCounts));

        while (recentFrames_.size() > windowFrames_) {
            for (const auto &entry : recentFrames_.front()) {
                auto totalIt = channelWindowTotals_.find(entry.first);
                if (totalIt == channelWindowTotals_.end()) {
                    continue;
                }
                if (totalIt->second <= entry.second) {
                    channelWindowTotals_.erase(totalIt);
                }
                else {
                    totalIt->second -= entry.second;
                }
            }
            recentFrames_.pop_front();
        }

        auto hot = std::make_shared<HotChannelSet>();
        const double framesInWindow = static_cast<double>(recentFrames_.size());
        if (framesInWindow <= 0.0) {
            return hot;
        }

        for (const auto &entry : channelWindowTotals_) {
            const double hitsPerFrame = static_cast<double>(entry.second) / framesInWindow;
            if (hitsPerFrame >= rateThreshold_) {
                hot->insert(entry.first);
            }
        }

        return hot;
    }

private:
    bool enabled_ = false;
    size_t windowFrames_ = 1;
    double rateThreshold_ = 1.0;
    std::deque<std::unordered_map<uint32_t, uint32_t>> recentFrames_;
    std::unordered_map<uint32_t, uint64_t> channelWindowTotals_;
};

ClusterSearchOptions makeClusterSearchOptions(const Options &options) {
    ClusterSearchOptions searchOptions;
    searchOptions.storeClusterHits = options.storeClusterHits || options.clusterDebug;
    searchOptions.adaptiveHotChannels = options.adaptiveHotChannels;
    searchOptions.weightedClusterScore = options.weightedClusterScore;
    searchOptions.weightedScoreThreshold = options.weightedScoreThreshold;
    searchOptions.hotChannelWeight = options.hotChannelWeight;
    return searchOptions;
}

std::string frameValidationLabel(const FrameResult &frame) {
    std::ostringstream out;
    if (frame.haveFrameInfo) {
        out << "frame=" << frame.frameNumber << " timestamp=" << frame.frameTimestamp;
    }
    else {
        out << "frame=n/a timestamp=n/a";
    }
    return out.str();
}

bool compareClusterVectors(const std::vector<Cluster> &expected,
                           const std::vector<Cluster> &actual,
                           std::string &detail) {
    if (expected.size() != actual.size()) {
        std::ostringstream out;
        out << "cluster-count expected=" << expected.size()
            << " actual=" << actual.size();
        detail = out.str();
        return false;
    }

    for (size_t i = 0; i < expected.size(); ++i) {
        const auto &oldCluster = expected[i];
        const auto &newCluster = actual[i];

        if (oldCluster.seedLeft != newCluster.seedLeft ||
            oldCluster.seedRight != newCluster.seedRight ||
            oldCluster.frozenLeft != newCluster.frozenLeft ||
            oldCluster.finalRight != newCluster.finalRight ||
            clusterAssociatedHits(oldCluster) != clusterAssociatedHits(newCluster) ||
            clusterAssociatedChannels(oldCluster) != clusterAssociatedChannels(newCluster)) {
            std::ostringstream out;
            out << "cluster[" << i << "]"
                << " expectedSeed=[" << oldCluster.seedLeft << "," << oldCluster.seedRight << ")"
                << " actualSeed=[" << newCluster.seedLeft << "," << newCluster.seedRight << ")"
                << " expectedFrozenLeft=" << oldCluster.frozenLeft
                << " actualFrozenLeft=" << newCluster.frozenLeft
                << " expectedFinalRight=" << oldCluster.finalRight
                << " actualFinalRight=" << newCluster.finalRight
                << " expectedHits=" << clusterAssociatedHits(oldCluster)
                << " actualHits=" << clusterAssociatedHits(newCluster)
                << " expectedChannels=" << clusterAssociatedChannels(oldCluster)
                << " actualChannels=" << clusterAssociatedChannels(newCluster);
            detail = out.str();
            return false;
        }
    }

    return true;
}

class ValidationReporter {
public:
    void record(const FrameResult &frame, bool ok, const std::string &detail) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++stats_.framesChecked;
        if (ok) {
            return;
        }

        ++stats_.mismatchedFrames;
        if (mismatchesPrinted_ < maxMismatchPrints_) {
            ++mismatchesPrinted_;
            std::cout << "VALIDATE_CLUSTER_SEARCH_MISMATCH "
                      << frameValidationLabel(frame)
                      << " " << detail << '\n';
        }
    }

    ValidationStats snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

private:
    mutable std::mutex mutex_;
    ValidationStats stats_;
    uint64_t mismatchesPrinted_ = 0;
    uint64_t maxMismatchPrints_ = 5;
};

void validateClusterSearchForFrame(const FrameResult &frame,
                                   uint64_t frameStart,
                                   const ClusterScanConfig &config,
                                   const std::vector<Cluster> &optimizedClusters,
                                   ValidationReporter &reporter) {
    const std::vector<Cluster> legacyClusters =
        findSlidingWindowClustersLegacy(frame.visibleHits, frameStart, config, ClusterDebugCallback{});

    std::string detail;
    const bool ok = compareClusterVectors(legacyClusters, optimizedClusters, detail);
    reporter.record(frame, ok, detail);
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

void processDecodedFrame(FrameResult &frame,
                         const Options &options,
                         const ClusterScanConfig &config,
                         const ClusterSearchOptions &searchOptions,
                         const HotChannelSetPtr &hotChannels,
                         RunStats &stats,
                         TimingStats &timing,
                         ClusterSearchWorkspace &workspace,
                         ValidationReporter *validationReporter) {
    const uint64_t frameStart = frame.haveFrameInfo ? frame.frameTimestamp : 0;

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

    {
        ScopedStageTimer timer(options.timing, timing.clusterNs);
        frame.clusters = findSlidingWindowClusters(frame.visibleHits, frameStart,
                                                   config, searchOptions,
                                                   hotChannels, workspace,
                                                   debugCallback);
        if (options.validateClusterSearch && validationReporter != nullptr) {
            validateClusterSearchForFrame(frame, frameStart, config,
                                          frame.clusters, *validationReporter);
        }
    }

    {
        ScopedStageTimer timer(options.timing, timing.statsNs);
        addFrameStats(frame, stats);
    }
}

struct FrameJob {
    FrameResult frame;
    HotChannelSetPtr hotChannels;
};

class FrameJobQueue {
public:
    explicit FrameJobQueue(size_t maxDepth) : maxDepth_(std::max<size_t>(1, maxDepth)) {}

    bool push(FrameJob &&job) {
        std::unique_lock<std::mutex> lock(mutex_);
        notFull_.wait(lock, [this] {
            return finished_ || gQuit.load() || jobs_.size() < maxDepth_;
        });

        if (finished_ || gQuit.load()) {
            return false;
        }

        jobs_.push_back(std::move(job));
        notEmpty_.notify_one();
        return true;
    }

    bool pop(FrameJob &job) {
        std::unique_lock<std::mutex> lock(mutex_);
        notEmpty_.wait(lock, [this] {
            return finished_ || gQuit.load() || !jobs_.empty();
        });

        if (jobs_.empty()) {
            return false;
        }

        job = std::move(jobs_.front());
        jobs_.pop_front();
        notFull_.notify_one();
        return true;
    }

    void finish() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            finished_ = true;
        }
        notEmpty_.notify_all();
        notFull_.notify_all();
    }

private:
    size_t maxDepth_ = 1;
    std::mutex mutex_;
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;
    std::deque<FrameJob> jobs_;
    bool finished_ = false;
};

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
              const ClusterSearchOptions &searchOptions,
              RunStats &totals,
              TimingStats &timingTotals,
              ValidationReporter *validationReporter) {
    RunStats fileStats;
    TimingStats fileTiming;
    fileStats.inputFiles = 1;
    fileStats.inputBytes = safeFileSize(path);

    EvioReader reader(path);
    uint64_t eventIndex = 0;
    HotChannelTracker hotTracker(options.adaptiveHotChannels,
                                 options.hotChannelWindowFrames,
                                 options.hotChannelRateThreshold);
    ClusterSearchWorkspace sequentialWorkspace;

    auto processEvent = [&](const std::shared_ptr<EvioEvent> &event,
                            FrameJobQueue *queue,
                            RunStats &localStats,
                            TimingStats &localTiming) {
        ++eventIndex;
        ++fileStats.events;

        const uint64_t eventBytes = event ? event->getTotalBytes() : 0;
        fileStats.eventBytes += eventBytes;

        auto header = event ? event->getHeader() : nullptr;
        uint16_t eventTag = header ? header->getTag() : 0;

        if (eventTag == TAG_PRESTART || eventTag == TAG_GO || eventTag == TAG_END) {
            ++fileStats.controlEvents;
            return;
        }

        if (eventTag == TAG_BUILT_STREAMING) {
            ++fileStats.builtStreamingEvents;
        }

        try {
            FrameResult frame;
            {
                ScopedStageTimer timer(options.timing, fileTiming.decodeNs);
                frame = decodeStreamingFrame(event, options);
            }
            frame.eventBytes = eventBytes;

            // This first-pass online noise model is deliberately simple: it
            // labels channels hot from recent per-frame hit rates and lets the
            // cluster seed logic ignore or downweight them only in opt-in modes.
            HotChannelSetPtr hotChannels = hotTracker.updateAndSnapshot(frame.visibleHits);

            if (options.clusterDebug) {
                printFrameDebugHeader(path, eventIndex, eventTag, frame, config);
            }

            if (queue != nullptr) {
                FrameJob job;
                job.frame = std::move(frame);
                job.hotChannels = std::move(hotChannels);
                queue->push(std::move(job));
            }
            else {
                processDecodedFrame(frame, options, config, searchOptions,
                                    hotChannels, localStats, localTiming,
                                    sequentialWorkspace, validationReporter);

                if (options.clusterDebug && !gQuit.load()) {
                    printFrameDebugSummary(frame, config);
                }
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
    };

    if (options.threads <= 1) {
        while (!gQuit.load()) {
            std::shared_ptr<EvioEvent> event;
            {
                ScopedStageTimer timer(options.timing, fileTiming.parseNs);
                event = reader.parseNextEvent();
            }
            if (!event) {
                break;
            }
            processEvent(event, nullptr, fileStats, fileTiming);
        }
    }
    else {
        // EVIO event objects are not shared with workers. The reader thread
        // parses and decodes sequentially, then workers cluster independent
        // decoded frames with per-thread stats/workspaces and no hot-path atomics.
        FrameJobQueue queue(options.threads * 4);
        std::vector<RunStats> workerStats(options.threads);
        std::vector<TimingStats> workerTimings(options.threads);
        std::vector<std::thread> workers;
        workers.reserve(options.threads);

        for (size_t workerIndex = 0; workerIndex < options.threads; ++workerIndex) {
            workers.emplace_back([&, workerIndex] {
                ClusterSearchWorkspace workspace;
                FrameJob job;
                while (queue.pop(job)) {
                    try {
                        processDecodedFrame(job.frame, options, config, searchOptions,
                                            job.hotChannels, workerStats[workerIndex],
                                            workerTimings[workerIndex], workspace,
                                            validationReporter);
                    }
                    catch (const std::exception &) {
                        ++workerStats[workerIndex].malformedFrames;
                    }
                }
            });
        }

        while (!gQuit.load()) {
            std::shared_ptr<EvioEvent> event;
            {
                ScopedStageTimer timer(options.timing, fileTiming.parseNs);
                event = reader.parseNextEvent();
            }
            if (!event) {
                break;
            }
            processEvent(event, &queue, fileStats, fileTiming);
        }

        queue.finish();
        for (auto &worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        for (size_t workerIndex = 0; workerIndex < options.threads; ++workerIndex) {
            ScopedStageTimer timer(options.timing, fileTiming.statsNs);
            mergeStats(workerStats[workerIndex], fileStats);
            mergeTiming(workerTimings[workerIndex], fileTiming);
        }
    }

    {
        ScopedStageTimer timer(options.timing, fileTiming.statsNs);
        mergeStats(fileStats, totals);
    }
    mergeTiming(fileTiming, timingTotals);
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

void printTimingStats(const RunStats &stats, const TimingStats &timing) {
    const double parseSeconds = secondsFromNs(timing.parseNs);
    const double decodeSeconds = secondsFromNs(timing.decodeNs);
    const double clusterSeconds = secondsFromNs(timing.clusterNs);
    const double statsSeconds = secondsFromNs(timing.statsNs);
    const double totalSeconds = secondsFromNs(timing.totalNs);

    std::cout << "FINAL_STAGE_TIMING"
              << " parseSeconds=" << formatDouble(parseSeconds, 6)
              << " decodeSeconds=" << formatDouble(decodeSeconds, 6)
              << " clusterSeconds=" << formatDouble(clusterSeconds, 6)
              << " statsSeconds=" << formatDouble(statsSeconds, 6)
              << " totalSeconds=" << formatDouble(totalSeconds, 6)
              << " framesPerSecond=" << formatDouble(totalSeconds > 0.0
                     ? static_cast<double>(stats.frames) / totalSeconds
                     : 0.0, 3)
              << " clusterSearchFramesPerSecond=" << formatDouble(clusterSeconds > 0.0
                     ? static_cast<double>(stats.frames) / clusterSeconds
                     : 0.0, 3)
              << " visibleHitsPerSecond=" << formatDouble(totalSeconds > 0.0
                     ? static_cast<double>(stats.visibleHits) / totalSeconds
                     : 0.0, 3)
              << '\n';
}

void printValidationStats(const ValidationReporter &reporter) {
    const ValidationStats stats = reporter.snapshot();
    std::cout << "VALIDATE_CLUSTER_SEARCH"
              << " frames=" << stats.framesChecked
              << " mismatchedFrames=" << stats.mismatchedFrames
              << " status=" << (stats.mismatchedFrames == 0 ? "PASS" : "FAIL")
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

        if (options.clusterDebug && options.threads != 1) {
            throw std::runtime_error("--cluster-debug requires --threads 1");
        }
        if (options.clusterDebug && options.validateClusterSearch) {
            throw std::runtime_error("--validate-cluster-search cannot be combined with --cluster-debug");
        }
        if (options.validateClusterSearch &&
            (options.adaptiveHotChannels || options.weightedClusterScore)) {
            throw std::runtime_error("--validate-cluster-search compares default clustering; disable adaptive options");
        }

        const ClusterSearchOptions searchOptions = makeClusterSearchOptions(options);
        RunStats totals;
        TimingStats timings;
        ValidationReporter validationReporter;
        ValidationReporter *validationReporterPtr =
            options.validateClusterSearch ? &validationReporter : nullptr;

        std::chrono::steady_clock::time_point totalStart;
        if (options.timing) {
            totalStart = std::chrono::steady_clock::now();
        }

        for (const auto &path : options.inputFiles) {
            if (gQuit.load()) {
                break;
            }
            scanFile(path, options, config, searchOptions, totals, timings,
                     validationReporterPtr);
        }

        if (options.timing) {
            const auto totalStop = std::chrono::steady_clock::now();
            timings.totalNs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(totalStop - totalStart).count());
        }

        if (gQuit.load()) {
            std::cout << "\nStop requested, printing stats collected so far.\n";
        }

        printFinalStats(totals, config);
        if (options.timing) {
            printTimingStats(totals, timings);
        }
        if (validationReporterPtr != nullptr) {
            printValidationStats(validationReporter);
        }
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}
