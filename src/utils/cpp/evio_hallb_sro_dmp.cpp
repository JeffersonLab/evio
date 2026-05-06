#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <poll.h>
#include <stdexcept>
#include <string>
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

struct Options {
    std::string path;
    bool verbose = false;
    std::vector<MaskedChannel> masks;
};

void ctrlCHandler(int) {
    gQuit.store(true);
}

void printUsage(const char *name) {
    std::cerr << "Usage: " << name << " [-v] [--mask crate,slot,channel] file.evio\n";
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

        if (arg == "-v" || arg == "--verbose") {
            options.verbose = true;
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

        if (!options.path.empty()) {
            throw std::runtime_error("only one input file may be specified");
        }
        options.path = arg;
    }

    if (options.path.empty()) {
        throw std::runtime_error("missing input file");
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

size_t countVisibleHits(const std::vector<Hit> &hits, const Options &options) {
    size_t count = 0;
    for (const auto &hit : hits) {
        if (!isMasked(hit, options.masks)) {
            ++count;
        }
    }
    return count;
}

std::string formatHex16(uint16_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(4) << std::setfill('0') << value << std::dec;
    return out.str();
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

void printStructureSummary(const std::string &label, const std::shared_ptr<BaseStructure> &node) {
    if (node == nullptr) {
        std::cout << label << ": <null>\n";
        return;
    }

    auto header = node->getHeader();
    std::cout << label
              << ": tag=" << formatHex16(header ? header->getTag() : 0)
              << " type=" << (header ? header->getDataType().toString() : "UNKNOWN")
              << " children=" << node->getChildCount()
              << " dataItems=" << node->getNumberDataItems()
              << " bytes=" << node->getRawBytes().size()
              << '\n';
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

void printAggregationInfo(const std::shared_ptr<BaseStructure> &aggInfoSeg) {
    if (aggInfoSeg == nullptr) {
        std::cout << "    Aggregation info: <missing>\n";
        return;
    }

    auto header = aggInfoSeg->getHeader();
    if (header == nullptr) {
        std::cout << "    Aggregation info: <missing header>\n";
        return;
    }

    std::cout << "    Aggregation info: tag=" << formatHex16(header->getTag())
              << " type=" << header->getDataType().toString()
              << " entries=" << aggInfoSeg->getNumberDataItems() << '\n';

    try {
        if (header->getDataType() == DataType::USHORT16 ||
            header->getDataType() == DataType::SHORT16) {
            auto payloads = aggInfoSeg->getUShortData();
            for (size_t i = 0; i < payloads.size(); ++i) {
                uint16_t entry = payloads[i];
                uint16_t payloadPort = entry & 0x1f;
                uint16_t laneId = (entry >> 5) & 0x3;
                uint16_t bonded = (entry >> 7) & 0x1;
                uint16_t moduleId = (entry >> 8) & 0xf;

                std::cout << "      entry[" << i << "]"
                          << ": raw=" << formatHex16(entry)
                          << " payloadPort=" << payloadPort
                          << " lane=" << laneId
                          << " bond=" << bonded
                          << " module=" << moduleId
                          << '\n';
            }
            return;
        }

        if (header->getDataType() == DataType::UINT32 ||
            header->getDataType() == DataType::INT32) {
            std::vector<uint32_t> entries;
            if (header->getDataType() == DataType::UINT32) {
                entries = aggInfoSeg->getUIntData();
            }
            else {
                auto signedEntries = aggInfoSeg->getIntData();
                entries.assign(signedEntries.begin(), signedEntries.end());
            }

            for (size_t i = 0; i < entries.size(); ++i) {
                uint32_t entry = entries[i];
                uint16_t rocId = static_cast<uint16_t>((entry >> 16U) & 0xffffU);
                uint8_t reserved = static_cast<uint8_t>((entry >> 8U) & 0xffU);
                uint8_t status = static_cast<uint8_t>(entry & 0xffU);

                std::cout << "      entry[" << i << "]"
                          << ": raw=0x" << std::hex << std::setw(8) << std::setfill('0') << entry
                          << std::dec << std::setfill(' ')
                          << " rocId=" << rocId
                          << " reserved=" << static_cast<unsigned int>(reserved)
                          << " status=" << static_cast<unsigned int>(status)
                          << '\n';
            }
            return;
        }
    }
    catch (const std::exception &e) {
        std::cout << "      Could not decode aggregation info payload: " << e.what() << '\n';
        return;
    }

    std::cout << "      Unsupported aggregation info data type for specialized decode\n";
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

    std::sort(hits.begin(), hits.end(),
              [](const Hit &a, const Hit &b) { return a.time < b.time; });

    return hits;
}

size_t printDecodedHits(const std::vector<Hit> &hits, const Options &options) {
    size_t printed = 0;
    for (const auto &hit : hits) {
        if (isMasked(hit, options.masks)) {
            continue;
        }

        std::cout << "      crate=" << hit.crate
                  << ", slot=" << hit.slot
                  << ", channel=" << hit.channel
                  << ", charge=" << hit.charge
                  << ", time=" << hit.time
                  << '\n';
        ++printed;
    }

    if (printed == 0 && options.verbose) {
        std::cout << "      No FADC hits decoded from payload\n";
    }

    return printed;
}

void decodeStreamingEvent(const std::shared_ptr<EvioEvent> &event, const Options &options) {
    if (event == nullptr) {
        return;
    }

    auto eventHeader = event->getHeader();
    std::cout << "Event header: tag=" << formatHex16(eventHeader ? eventHeader->getTag() : 0)
              << " type=" << (eventHeader ? eventHeader->getDataType().toString() : "UNKNOWN")
              << " num=" << static_cast<unsigned int>(eventHeader ? eventHeader->getNumber() : 0)
              << " children=" << event->getChildCount()
              << '\n';

    uint32_t frameNumber = 0;
    uint64_t frameTimestamp = 0;
    bool haveFrameInfo = false;

    if (event->getChildCount() > 0) {
        auto eventInfo = event->getChildAt(0);
        printStructureSummary("  Event Info", eventInfo);

        if (readFrameInfo(eventInfo, frameNumber, frameTimestamp)) {
            haveFrameInfo = true;
            std::cout << "    Frame=" << frameNumber
                      << " Timestamp=" << frameTimestamp
                      << '\n';
        }
        else if (eventInfo != nullptr && eventInfo->getChildCount() > 0) {
            auto timeSliceSeg = eventInfo->getChildAt(0);
            if (options.verbose) {
                printStructureSummary("    Time Slice Segment", timeSliceSeg);
            }
            if (readFrameInfo(timeSliceSeg, frameNumber, frameTimestamp)) {
                haveFrameInfo = true;
                std::cout << "      Frame=" << frameNumber
                          << " Timestamp=" << frameTimestamp
                          << '\n';
            }

            if (options.verbose && eventInfo->getChildCount() > 1) {
                printAggregationInfo(eventInfo->getChildAt(1));
            }
        }
        else {
            std::cout << "    Could not decode frame/timestamp from top-level event info\n";
        }
    }

    for (size_t rocIndex = 1; rocIndex < event->getChildCount(); ++rocIndex) {
        auto rocTSB = event->getChildAt(rocIndex);
        auto rocHeader = rocTSB ? rocTSB->getHeader() : nullptr;
        int crate = rocHeader ? rocHeader->getTag() : static_cast<int>(rocIndex - 1);

        std::cout << "  ROC[" << (rocIndex - 1) << "]"
                  << ": crate=" << crate
                  << " tag=" << formatHex16(rocHeader ? rocHeader->getTag() : 0)
                  << " type=" << (rocHeader ? rocHeader->getDataType().toString() : "UNKNOWN")
                  << " children=" << (rocTSB ? rocTSB->getChildCount() : 0)
                  << '\n';

        if (rocTSB == nullptr || rocTSB->getChildCount() == 0) {
            std::cout << "    ROC bank has no children\n";
            continue;
        }

        auto sib = rocTSB->getChildAt(0);
        if (options.verbose) {
            printStructureSummary("    SIB", sib);
        }

        if (sib != nullptr && sib->getChildCount() > 0) {
            auto timeSliceSeg = sib->getChildAt(0);
            uint32_t rocFrameNumber = 0;
            uint64_t rocTimestamp = 0;

            if (options.verbose) {
                printStructureSummary("    Time Slice Segment", timeSliceSeg);
            }
            if (readFrameInfo(timeSliceSeg, rocFrameNumber, rocTimestamp)) {
                std::cout << "      Frame=" << rocFrameNumber
                          << " Timestamp=" << rocTimestamp
                          << '\n';
                if (!haveFrameInfo) {
                    frameNumber = rocFrameNumber;
                    frameTimestamp = rocTimestamp;
                    haveFrameInfo = true;
                }
            }
        }

        if (options.verbose && sib != nullptr && sib->getChildCount() > 1) {
            printAggregationInfo(sib->getChildAt(1));
        }

        for (size_t payloadIndex = 1; payloadIndex < rocTSB->getChildCount(); ++payloadIndex) {
            auto dataBank = rocTSB->getChildAt(payloadIndex);
            auto dataHeader = dataBank ? dataBank->getHeader() : nullptr;
            int slot = dataHeader ? dataHeader->getTag() : static_cast<int>(payloadIndex - 1);

            if (dataBank == nullptr) {
                if (options.verbose) {
                    std::cout << "    Payload[" << (payloadIndex - 1) << "]"
                              << ": slot=" << slot
                              << " tag=" << formatHex16(0)
                              << " type=UNKNOWN bytes=0\n";
                    std::cout << "      Payload bank missing\n";
                }
                continue;
            }

            auto hits = decodeFadc250Payload(frameTimestamp, crate, slot,
                                             dataBank->getRawBytes(),
                                             dataBank->getByteOrder());
            size_t visibleHits = countVisibleHits(hits, options);
            if (!options.verbose && visibleHits == 0) {
                continue;
            }

            std::cout << "    Payload[" << (payloadIndex - 1) << "]"
                      << ": slot=" << slot
                      << " tag=" << formatHex16(dataHeader ? dataHeader->getTag() : 0)
                      << " type=" << (dataHeader ? dataHeader->getDataType().toString() : "UNKNOWN")
                      << " bytes=" << dataBank->getRawBytes().size()
                      << '\n';

            if (!haveFrameInfo) {
                std::cout << "      Warning: no frame timestamp decoded, hit times use 0 as base\n";
            }
            printDecodedHits(hits, options);
        }
    }
}

bool waitForUser() {
    std::cout << "\nHit <Enter> for next event (Ctrl+C to quit)";
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

void printControlEventMessage(uint16_t eventTag) {
    if (eventTag == TAG_PRESTART) {
        std::cout << "Control event: PRESTART\n";
    }
    else if (eventTag == TAG_GO) {
        std::cout << "Control event: GO\n";
    }
    else if (eventTag == TAG_END) {
        std::cout << "Control event: END\n";
    }
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
        EvioReader reader(options.path);

        std::cout << "Opened " << options.path << '\n';
        std::cout << "EVIO version: " << reader.getEvioVersion() << '\n';
        std::cout << "Byte order: " << reader.getByteOrder().getName() << '\n';
        std::cout << "Event count: " << reader.getEventCount() << '\n';

        size_t eventIndex = 0;
        std::shared_ptr<EvioEvent> event;

        while (!gQuit.load() && (event = reader.parseNextEvent())) {
            ++eventIndex;

            std::cout << "\n============================================================\n";
            std::cout << "Event " << eventIndex << '\n';

            auto header = event->getHeader();
            uint16_t eventTag = header ? header->getTag() : 0;

            if (eventTag == TAG_PRESTART || eventTag == TAG_GO || eventTag == TAG_END) {
                printControlEventMessage(eventTag);
            }
            else {
                if (eventTag == TAG_BUILT_STREAMING) {
                    std::cout << "Built streaming event detected\n";
                }
                decodeStreamingEvent(event, options);
            }

            if (eventTag == TAG_END || gQuit.load()) {
                break;
            }

            if (!waitForUser()) {
                break;
            }
        }

        if (gQuit.load()) {
            std::cout << "\nSIGINT received, exiting.\n";
        }
        else {
            std::cout << "\nReached end of file.\n";
        }
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
