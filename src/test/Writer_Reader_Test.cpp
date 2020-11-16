//
// Created by timmer on 2/19/20.
//

/**
 * Copyright (c) 2019, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 07/05/2019
 * @author timmer
 */


#include <string>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <limits>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <sys/mman.h>

#include "eviocc.h"


namespace evio {


    // Test the BaseStructure's tree methods
    static void EventWriterTest() {

        //---------------------------------------------
        // Use CompactEventBuilder to create an event
        //---------------------------------------------

        size_t bufSize = 1000;
        CompactEventBuilder ceb(bufSize, ByteOrder::ENDIAN_LOCAL, true);

        ceb.openBank(1, DataType::BANK, 1);
          ceb.openBank(2, DataType::DOUBLE64, 2);
            double dd[3] = {1.1, 2.2, 3.3};
            ceb.addDoubleData(dd, 3);
          ceb.closeStructure();

          ceb.openBank(3, DataType::SEGMENT, 3);
            ceb.openSegment(4, DataType::UINT32);
              uint32_t ui[3] = {4, 5, 6};
              ceb.addIntData(ui, 3);
            ceb.closeStructure();
          ceb.closeStructure();

          ceb.openBank(5, DataType::TAGSEGMENT, 5);
            ceb.openTagSegment(6, DataType::USHORT16);
              uint16_t us[3] = {7, 8, 9};
              ceb.addShortData(us, 3);
            ceb.closeStructure();
          ceb.closeStructure();

          ceb.openBank(7, DataType::COMPOSITE, 7);
            // Now create some COMPOSITE data
            // Format to write 1 int and 1 float a total of N times
            std::string format = "N(I,F)";

            CompositeData::Data myData;
            myData.addN(2);
            myData.addInt(1);
            myData.addFloat(1.0F);
            myData.addInt(2);
            myData.addFloat(2.0F);

            // Create CompositeData object
            auto cData = CompositeData::getInstance(format, myData, 1, 1, 1);
            std::vector<std::shared_ptr<CompositeData>> cDataVec;
            cDataVec.push_back(cData);

            // Add to bank
            ceb.addCompositeData(cDataVec);

        ceb.closeAll();

        auto cebEvbuf = ceb.getBuffer();

        //Util::printBytes(cebEvbuf, 0 , 200, "From CompactEventBuilder");

        // Write into a buffer
        auto newBuf = std::make_shared<ByteBuffer>(1000);
        EventWriter writer(newBuf);
        writer.writeEvent(cebEvbuf);
        writer.close();
        auto writerBuf = writer.getByteBuffer();

        //Util::printBytes(newBuf, 0 , 260, "From EventWriter");

        // Read event back out of buffer
        EvioReader reader(writerBuf);
        auto cebEv = reader.parseEvent(1);

        std::cout << "Event written to and read from buffer:\n" << cebEv->treeToString("") << std::endl;

        // Write into a file
        std::string filename = "./eventData.dat";
        EventWriter writer2(filename);
        writer2.writeEvent(cebEvbuf);
        writer2.close();

        //Util::printBytes(newBuf, 0 , 260, "From EventWriter");

        // Read event back out of buffer
        EvioReader reader2(filename);
        auto cebEv2 = reader2.parseEvent(1);

        std::cout << "\nEvent written to and read from file:\n" << cebEv2->treeToString("") << std::endl;

        //---------------------------------------------
        // Create another event
        //---------------------------------------------

        CompactEventBuilder ceb2(bufSize, ByteOrder::ENDIAN_LOCAL, false);

        ceb2.openBank(10, DataType::BANK, 10);
            ceb2.openBank(20, DataType::UCHAR8, 20);
                uint8_t bb[3] = {10, 20, 30};
                ceb2.addByteData(bb, 3);
            ceb2.closeStructure();

            ceb2.openBank(30, DataType::CHARSTAR8, 30);
                std::vector<std::string> strs;
                strs.push_back("40_string");
                strs.push_back("50_string");
                strs.push_back("60_string");
                ceb2.addStringData(strs);

        ceb2.closeAll();
        auto cebEvbuf3 = ceb2.getBuffer();

        // Append this to the file
        EventWriter writer3(filename, ByteOrder::ENDIAN_LOCAL, true);
        writer3.writeEvent(cebEvbuf3);
        writer3.close();

        // Read 2 events back out of buffer
        EvioReader reader3(filename);
        auto ev3 = reader3.parseEvent(1);
        auto ev4 = reader3.parseEvent(2);

        std::cout << "\nEvent & appended event written to and read from file:\n" << ev3->treeToString("") << std::endl;
        std::cout << "\n" << ev4->treeToString("") << std::endl;

        auto stringChild = ev4->getChildAt(1);
        auto strData = stringChild->getStringData();
        std::cout << "String data of last child bank:\n";
        for (std::string s : strData) {
            std::cout << "  " << s << std::endl;
        }

        auto byteChild = ev4->getChildAt(0);
        auto byteData = byteChild->getUCharData();
        std::cout << "Unsigned char data of first child bank:\n";
        for (unsigned char c : byteData) {
            std::cout << "  " << (int)c << std::endl;
        }

        //---------------------------------------------
        // Write event with EvioNode, not ByteBuffer
        //---------------------------------------------
       // ceb.


    }




}



int main(int argc, char **argv) {
    evio::EventWriterTest();
    return 0;
}


