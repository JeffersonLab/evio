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

#include "EvioBank.h"
#include "EvioEvent.h"
#include "EvioSegment.h"
#include "StructureTransformer.h"
#include "EvioSwap.h"
#include "EventWriter.h"
#include "EvioReader.h"
#include "EventBuilder.h"
#include "CompactEventBuilder.h"
#include "ByteOrder.h"

#include "CompositeData.h"
#include "DataType.h"
#include "Util.h"


namespace evio {


    // Test the BaseStructure's tree methods
    static void EventWriterTest() {

        //---------------------------------------------
        // Use CompactEventBuilder to create an event
        //---------------------------------------------

        size_t bufSize = 1000;
        CompactEventBuilder ceb(bufSize, ByteOrder::ENDIAN_LOCAL, false);

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

        Util::printBytes(cebEvbuf, 0 , 200, "From CompactEventBuilder");

        // Write into a buffer
        auto newBuf = std::make_shared<ByteBuffer>(1000);
        EventWriter writer(newBuf);
        writer.writeEvent(cebEvbuf);
        writer.close();
        auto writerBuf = writer.getByteBuffer();

        Util::printBytes(newBuf, 0 , 260, "From EventWriter");

        // Read event back out of buffer
        EvioReader reader(writerBuf);
        auto cebEv2 = reader.getEvent(1);
        auto cebEv = reader.parseEvent(1);

        std::cout << "Event:\n" << cebEv->treeToString("") << std::endl;

    }




}



int main(int argc, char **argv) {
    evio::EventWriterTest();
    return 0;
}


