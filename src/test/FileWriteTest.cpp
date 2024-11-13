/**
 * Copyright (c) 2022, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 07/01/2022
 * @author timmer
 */


#include <cstdint>
#include <memory>
#include <limits>
#include <fstream>

#include "eviocc.h"


namespace evio {


    static void EventWriterTest() {

        //---------------------------------------------
        // Use CompactEventBuilder to create an event
        //---------------------------------------------

        // Create room for entire big event
        size_t bufSize = 1300000;
        CompactEventBuilder ceb(bufSize, ByteOrder::ENDIAN_LOCAL, true);
        ceb.openBank(1, DataType::INT32, 1);
        int32_t dd[250000];
        ceb.addIntData(dd, 250000);
        ceb.closeAll();
        std::shared_ptr<ByteBuffer> bigEvt = ceb.getBuffer();

        // Little event
        bufSize = 1000;
        CompactEventBuilder eb(bufSize, ByteOrder::ENDIAN_LOCAL, true);
        eb.openBank(1, DataType::INT32, 1);
            eb.addIntData(dd, 3);
        eb.closeAll();
        std::shared_ptr<ByteBuffer> littleEvt = eb.getBuffer();

        std::cout << "Buf pos = " << bigEvt->position() << ", lim = " << bigEvt->limit() <<
        ", cap = " << bigEvt->capacity() << std::endl;

        // Write into a file
        uint32_t targetRecordBytes = 900000; // 900KB
        uint32_t bufferBytes = 1000000; // 1MB

        std::string fname = "./codaFileTestCC.ev";

        int evioVersion = 4;

        if (evioVersion == 4) {
            EventWriterV4 writer(fname, "", "", 1, 0, targetRecordBytes,
                                 100000, ByteOrder::ENDIAN_LOCAL, "", true, false,
                                 nullptr, 0, 0, 1, 1, bufferBytes);

            std::cout << "Write little event 1" << std::endl;
            writer.writeEvent(littleEvt, false);
            // Delay between writes
            //std::this_thread::sleep_for(std::chrono::seconds(2));
            std::cout << "Write BIG event 1" << std::endl;
            writer.writeEvent(bigEvt, false);
            //std::this_thread::sleep_for(std::chrono::seconds(2));
            std::cout << "Write little event 2" << std::endl;
            writer.writeEvent(littleEvt, true);
            //std::this_thread::sleep_for(std::chrono::seconds(2));
            std::cout << "Write BIG event 2" << std::endl;
            writer.writeEvent(bigEvt, true);
            std::cout << "WRTER CLOSE" << std::endl;
            writer.close();
        }
        else {
            EventWriter writer(fname, "", "", 1, 0, targetRecordBytes,
                                100000, ByteOrder::ENDIAN_LOCAL, "", true, false,
                                nullptr, 1, 0, 1, 1, Compressor::CompressionType::UNCOMPRESSED,
                                0, 0, bufferBytes);

            std::cout << "Write little event 1" << std::endl;
            writer.writeEventToFile(littleEvt, false, false);
            // Delay between writes
            //std::this_thread::sleep_for(std::chrono::seconds(2));
            std::cout << "Write BIG event 1" << std::endl;
            writer.writeEventToFile(bigEvt, false, false);
            //std::this_thread::sleep_for(std::chrono::seconds(2));
            std::cout << "Write little event 2" << std::endl;
            writer.writeEventToFile(littleEvt, false, true);
            //std::this_thread::sleep_for(std::chrono::seconds(2));
            std::cout << "Write BIG event 2" << std::endl;
            writer.writeEventToFile(bigEvt, false, true);
            std::cout << "WRTER CLOSE" << std::endl;
            writer.close();
        }
    }

}



int main(int argc, char **argv) {
    evio::EventWriterTest();
    return 0;
}


