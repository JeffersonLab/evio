#include <iostream>
#include <string>
#include <memory>
#include <algorithm>

#include "eviocc.h"               // EVIO-6 C++ API
using namespace evio;

enum class Fmt { EVIO4, EVIO6, HIPO };

void usage(const char* prog) {
    std::cerr <<
      "Usage: " << prog << " <in_file> <in_fmt:evio4|evio6|hipo> "
                     "<out_file> <out_fmt:evio4|evio6|hipo>\n";
    std::exit(1);
}

Fmt parseFmt(const std::string& s) {
    auto lo = s; std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
    if     (lo=="evio4") return Fmt::EVIO4;
    else if(lo=="evio6") return Fmt::EVIO6;
    else if(lo=="hipo")  return Fmt::HIPO;
    usage("convert");  
    return Fmt::EVIO6;  // never reached
}

int main(int argc, char* argv[]) {
    if (argc!=5) usage(argv[0]);
    std::string inFile  = argv[1];
    Fmt inFmt  = parseFmt(argv[2]);
    std::string outFile = argv[3];
    Fmt outFmt = parseFmt(argv[4]);

    // Common EVIO parameters (tweak as desired)
    uint32_t maxRecordBytes     = 1000000;
    uint32_t maxEventsPerRecord = 1000;
    uint32_t bufferBytes        = 1000000;
    size_t bufferBytesSize      = bufferBytes;

    // --- Set up reader ---
    std::unique_ptr<EvioReader> evioReader;
    if (inFmt==Fmt::EVIO4 || inFmt==Fmt::EVIO6) evioReader = std::make_unique<EvioReader>(inFile);

    // Pull dictionary XML (if present)
    std::string dictXml = "";
    if (evioReader->hasDictionaryXML()) {
        dictXml = evioReader->getDictionaryXML();   // preserves dictionary in user‐header :contentReference[oaicite:0]{index=0}
        std::cerr << "Dictionary XML: " << dictXml << "\n";
    }
    else std::cerr << "No dictionary XML found in input file.\n";


    // --- Set up writer ---
    std::unique_ptr<EventWriter> evioWriter;
    std::unique_ptr<EventWriterV4> evioWriterV4;

    if (outFmt==Fmt::EVIO6) {
        evioWriter = std::make_unique<EventWriter>(
            outFile, 
            "", "", 
            1, 0, 
            maxRecordBytes, maxEventsPerRecord,
            ByteOrder::ENDIAN_LOCAL, 
            dictXml,           // xml dictionary 
            true, false,                // overwrite existing, no append
            nullptr, 1, 0, 1, 1,
            Compressor::CompressionType::UNCOMPRESSED,  // use LZ4 compression to trigger HIPO format
            0, 0, 
            bufferBytes);
    }
    else if (outFmt==Fmt::EVIO4) {
        evioWriterV4 = std::make_unique<EventWriterV4>(
            outFile, 
            "", "", 
            1, 0, 
            maxRecordBytes, maxEventsPerRecord,
            ByteOrder::ENDIAN_LOCAL, 
            dictXml,                    // xml dictionary 
            true, false,                // overwrite existing, no append
            nullptr, 1, 0, 1, 1,
            bufferBytesSize);
    }
    else {
        evioWriter = std::make_unique<EventWriter>(
            outFile, 
            "", "", 
            1, 0, 
            maxRecordBytes, maxEventsPerRecord,
            ByteOrder::ENDIAN_LOCAL, 
            dictXml,           // xml dictionary 
            true, false,                // overwrite existing, no append
            nullptr, 1, 0, 1, 1,
            Compressor::CompressionType::LZ4,  // use LZ4 compression to trigger HIPO format
            0, 0, 
            bufferBytes);
    }


    // --- Conversion loop ---
    try {
        // EVIO → ...
        if (evioReader) {
            std::shared_ptr<EvioEvent> ev;
            while ((ev = evioReader->parseNextEvent())) {
                if (evioWriter) {
                    evioWriter->writeEvent(ev);
                }
            }
        }

        // flush/close
        if (evioWriter) evioWriter->close();

        std::cout<<"Conversion complete.\n";
    }
    catch (const std::exception &e) {
        std::cerr<<"Error during conversion: "<<e.what()<<"\n";
        return 1;
    }
    return 0;
}
