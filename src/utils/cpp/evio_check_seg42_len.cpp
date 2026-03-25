#include <algorithm>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

#include "eviocc.h"

using namespace evio;

namespace {

constexpr uint16_t EXPECTED_TAG = 0x42;
constexpr size_t WARMUP_EVENTS = 20;

struct Stats {
    uint64_t events = 0;
    uint64_t anomalies = 0;
    uint64_t malformed = 0;
    uint32_t maxLen = 0;
};

bool isExpectedBankOfSegments(const std::shared_ptr<BaseStructure> &s) {
    return s != nullptr &&
           s->getStructureType().isBank() &&
           s->getHeader() != nullptr &&
           s->getHeader()->getDataType().isSegment();
}

void printUsage(const char *name) {
    std::cerr << "Usage: " << name << " file1.evio [file2.evio ...]\n";
}

void scanFile(const std::string &path) {
    Stats stats;
    std::cout << "FILE " << path << '\n';

    try {
        EvioReader reader(path);
        std::shared_ptr<EvioEvent> event;

        while ((event = reader.parseNextEvent())) {
            ++stats.events;
            bool show = stats.events <= WARMUP_EVENTS;

            try {
                auto first = event->getChildAt(0);
                if (!isExpectedBankOfSegments(first) || first->getChildCount() < 2) {
                    ++stats.malformed;
                    std::cout << "  ev " << stats.events << " malformed layout\n";
                    continue;
                }

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
              << " maxDataLen=" << stats.maxLen << "\n\n";
}

} // namespace

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        scanFile(argv[i]);
    }

    return 0;
}
