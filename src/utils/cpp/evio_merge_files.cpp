//
// Copyright 2025, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100

#include <string>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <memory>
#include <limits>
#include <unistd.h>
#include <random>
#include <iostream>

#include "eviocc.h"

using namespace evio;

// Globals
static std::vector<char*> INFILENAMES;
static char* OUTFILENAME = nullptr;
static bool QUIT = false;

// Prototypes
void ParseCommandLineArguments(int argc, char* argv[]);
void Usage();
void ctrlCHandle(int);
void Process(unsigned int &NEvents, unsigned int &NEvents_read);
std::ifstream::pos_type GetFilesize(const char* filename);

// ---------- GetFilesize ----------
std::ifstream::pos_type GetFilesize(const char* filename) {
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg();
}

// ---------- main ----------
int main(int argc, char* argv[]) {
    signal(SIGINT, ctrlCHandle);

    ParseCommandLineArguments(argc, argv);

    // Print input files and their sizes
    for (auto file : INFILENAMES) {
        std::ifstream::pos_type size = GetFilesize(file);
        std::cout << "Input file: " << file << " (size: " << size << " bytes)" << std::endl;
    }

    unsigned int NEvents = 0;
    unsigned int NEvents_read = 0;
    Process(NEvents, NEvents_read);

    return 0;
}

// ---------- ParseCommandLineArguments ----------
void ParseCommandLineArguments(int argc, char* argv[]) {
    INFILENAMES.clear();

    for (int i = 1; i < argc; ++i) {
        char* ptr = argv[i];
        if (ptr[0] == '-') {
            switch (ptr[1]) {
                case 'h': Usage(); break;
                case 'o': OUTFILENAME = &ptr[2]; break;
                default:
                    std::cerr << "Unknown option: " << ptr << std::endl;
                    Usage();
            }
        } else {
            INFILENAMES.push_back(ptr);
        }
    }

    if (INFILENAMES.empty()) {
        std::cerr << "\nYou must specify at least one input file!\n" << std::endl;
        Usage();
    }

    if (!OUTFILENAME) {
        OUTFILENAME = new char[256];
        strcpy(OUTFILENAME, "merged.evio");
    }
}

// ---------- Usage ----------
void Usage() {
    std::cout << "\nUsage:\n";
    std::cout << "  evio_merge_files [-oOutputfile] file1.evio file2.evio ...\n\n";
    std::cout << "Options:\n";
    std::cout << "  -oOutputfile   Set output filename (default: merged.evio)\n";
    std::cout << "\nThis tool merges multiple EVIO files into one output file.\n";
    exit(0);
}

// ---------- ctrlCHandle ----------
void ctrlCHandle(int sig) {
    QUIT = true;
    std::cerr << "\nSIGINT received... exiting soon.\n";
}

// ---------- Process ----------
void Process(unsigned int &NEvents, unsigned int &NEvents_read) {
    std::cout << "Process() stub: would call into EVIO merge library here.\n";
    std::cout << "Output file: " << OUTFILENAME << std::endl;
    std::cout << "Number of input files: " << INFILENAMES.size() << std::endl;

    // TODO: merge here
    

}
