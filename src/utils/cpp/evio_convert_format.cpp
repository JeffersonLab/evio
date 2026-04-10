#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "eviocc.h"

using namespace evio;

enum class Fmt { EVIO4, EVIO6 };

[[noreturn]] void usage(const char *prog, const std::string &error = "") {
    if (!error.empty()) {
        std::cerr << error << "\n\n";
    }

    std::cerr
        << "Usage: " << prog << " <in_file> <in_fmt:evio4|evio6> "
        << "<out_file> <out_fmt:evio4|evio6>\n";
    std::exit(EXIT_FAILURE);
}

const char *fmtName(Fmt fmt) {
    switch (fmt) {
        case Fmt::EVIO4:
            return "evio4";
        case Fmt::EVIO6:
            return "evio6";
    }

    return "unknown";
}

uint32_t expectedVersion(Fmt fmt) {
    switch (fmt) {
        case Fmt::EVIO4:
            return 4;
        case Fmt::EVIO6:
            return 6;
    }

    return 0;
}

Fmt parseFmt(std::string value, const char *prog, const char *argName) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (value == "evio4") {
        return Fmt::EVIO4;
    }
    if (value == "evio6") {
        return Fmt::EVIO6;
    }

    usage(prog,
          std::string("Unsupported ") + argName + " '" + value +
              "'. Supported formats are: evio4, evio6.");
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        usage(argv[0]);
    }

    std::string inFile = argv[1];
    Fmt inFmt = parseFmt(argv[2], argv[0], "input format");
    std::string outFile = argv[3];
    Fmt outFmt = parseFmt(argv[4], argv[0], "output format");

    if (inFile == outFile) {
        std::cerr << "Input and output files must be different.\n";
        return EXIT_FAILURE;
    }

    constexpr uint32_t maxRecordBytes = 1000000;
    constexpr uint32_t maxEventsPerRecord = 1000;
    constexpr size_t bufferBytes = 1000000;

    try {
        auto evioReader = std::make_unique<EvioReader>(inFile);
        const uint32_t detectedVersion = evioReader->getEvioVersion();

        if (detectedVersion != expectedVersion(inFmt)) {
            std::cerr << "Input format mismatch: requested " << fmtName(inFmt)
                      << " but file reports EVIO version " << detectedVersion << ".\n";
            return EXIT_FAILURE;
        }

        std::string dictXml;
        if (evioReader->hasDictionaryXML()) {
            dictXml = evioReader->getDictionaryXML();
            std::cerr << "Copied dictionary XML from input file.\n";
        } else {
            std::cerr << "No dictionary XML found in input file.\n";
        }

        std::unique_ptr<EventWriter> evioWriter;
        std::unique_ptr<EventWriterV4> evioWriterV4;

        if (outFmt == Fmt::EVIO6) {
            evioWriter = std::make_unique<EventWriter>(
                outFile,
                "",
                "",
                1,
                0,
                maxRecordBytes,
                maxEventsPerRecord,
                ByteOrder::ENDIAN_LOCAL,
                dictXml,
                true,
                false,
                nullptr,
                1,
                0,
                1,
                1,
                Compressor::CompressionType::UNCOMPRESSED,
                0,
                0,
                bufferBytes);
        } else {
            evioWriterV4 = std::make_unique<EventWriterV4>(
                outFile,
                "",
                "",
                1,
                0,
                maxRecordBytes,
                maxEventsPerRecord,
                ByteOrder::ENDIAN_LOCAL,
                dictXml,
                true,
                false,
                nullptr,
                1,
                0,
                1,
                1,
                bufferBytes);
        }

        size_t eventCount = 0;
        std::shared_ptr<EvioEvent> event;
        while ((event = evioReader->parseNextEvent())) {
            if (evioWriter) {
                evioWriter->writeEvent(event);
            } else {
                evioWriterV4->writeEvent(event);
            }
            ++eventCount;
        }

        if (evioWriter) {
            evioWriter->close();
        }
        if (evioWriterV4) {
            evioWriterV4->close();
        }

        std::cout << "Converted " << eventCount << " events from " << fmtName(inFmt)
                  << " to " << fmtName(outFmt) << ".\n";
    }
    catch (const std::exception &e) {
        std::cerr << "Error during conversion: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
