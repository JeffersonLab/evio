//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include <iostream>
#include "eviocc.h"


namespace evio {


    /**
     * Test generating file names automagically.
     *
     * @date 03/17/2025
     * @author timmer
     */
    class SplitTest2  {
    };
}




/** Look at split names. */
int main(int argc, char **argv) {

    try {
        // $(FILE_ENV) demonstrates substituting for environmental variables so define FILE_ENV in your environment.
        // %s substitutes for run type
        // 1st int specifier substitutes for run number
        // 2nd int specifier substitutes for stream id
        // 3rd int specifier substitutes for split number
        std::string baseName = "my_$(FILE_ENV)_%s_run#%d_streamId#%d_.%06d";

        std::string runType = "MyRunType";
        uint32_t runNumber = 2;
        // Split file every 20 MB
        uint64_t split = 20000000;

        // Split file number starts at 0
        uint32_t splitCount = 0;
        uint32_t streamId = 3;
        uint32_t streamCount = 66;

        // The following may not be backwards compatible.
        // Make substitutions in the baseName to create the base file name.
        std::string baseFileName;
        uint32_t specifierCount = evio::Util::generateBaseFileName(baseName, runType, baseFileName);

        std::cout << "BaseName = " << baseName << std::endl;
        std::cout << "BaseFileName = " << baseFileName << std::endl;
        std::cout << "Specifier Count = " << specifierCount << std::endl;
        // Also create the file names with more substitutions, one for each of 10 splits
        for (int i=0; i<10; i++) {
            std::string fileName = evio::Util::generateFileName(baseFileName, specifierCount,
                                                                runNumber, split, splitCount++,
                                                                streamId, streamCount);

            std::cout << "  Filename for split " << i << " = " << fileName << std::endl;
        }


    }
    catch (std::runtime_error & e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
