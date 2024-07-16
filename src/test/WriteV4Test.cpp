/**
 * Copyright (c) 2022, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 07/03/2024
 * @author timmer
 */


#include <cstdint>
#include <memory>
#include <limits>
#include <sstream>
#include <algorithm>

#include "eviocc.h"


namespace evio {


    /**
     * This test is done in conjunction with an identical test written in Java.
     * It exercises as many of the EventWriterV4 methods that affect the output
     * as possible. By running this and the Java version and comparing output,
     * we can see if the port of EventWriterV4 from Java to C++ as successful
     * or not.
     */
    static void EventWriterTest() {

        std::stringstream ss;

        ss << "<xmlDict>\n" <<
           "  <bank name=\"HallD\"             tag=\"6-8\"  type=\"bank\" >\n" <<
           "      <description format=\"New Format\" >hall_d_tag_range</description>\n" <<
           "      <bank name=\"DC(%t)\"        tag=\"6\" num=\"4\" >\n" <<
           "          <leaf name=\"xpos(%n)\"  tag=\"6\" num=\"5\" />\n" <<
           "          <bank name=\"ypos(%n)\"  tag=\"6\" num=\"6\" />\n" <<
           "      </bank >\n" <<
           "      <bank name=\"TOF\"     tag=\"8\" num=\"0\" >\n" <<
           "          <leaf name=\"x\"   tag=\"8\" num=\"1\" />\n" <<
           "          <bank name=\"y\"   tag=\"8\" num=\"2\" />\n" <<
           "      </bank >\n" <<
           "      <bank name=\"BCAL\"      tag=\"7\" >\n" <<
           "          <leaf name=\"x(%n)\" tag=\"7\" num=\"1-3\" />\n" <<
           "      </bank >\n" <<
           "  </bank >\n" <<
           "  <dictEntry name=\"JUNK\" tag=\"5\" num=\"0\" />\n" <<
           "  <dictEntry name=\"SEG5\" tag=\"5\" >\n" <<
           "       <description format=\"Old Format\" >tag 5 description</description>\n" <<
           "  </dictEntry>\n" <<
           "  <bank name=\"Rangy\" tag=\"75 - 78\" >\n" <<
           "      <leaf name=\"BigTag\" tag=\"76\" />\n" <<
           "  </bank >\n" <<
           "</xmlDict>";

        std::string dictionary = ss.str();
        std::cout << "Const: len of dictionary = " << dictionary.size() << std::endl;


        //---------------------------------------------
        // Use CompactEventBuilder to create an event
        //---------------------------------------------

        // Create event bigger than desired block size (1.2 MB total)
        size_t bufSize = 1300000;
        CompactEventBuilder ceb(bufSize, ByteOrder::ENDIAN_LOCAL, true);
        ceb.openBank(1, DataType::INT32, 1);
        // 299998 words = 1199992 bytes of data
        // Add that to 8 bytes evio header = 1.2 MB event
        uint32_t dd[299998];
        std::fill(dd, dd + 299998, 0x1C);  // 0x1c --> 11100 bit pattern
        ceb.addIntData(dd, 299998);
        ceb.closeAll();
        std::shared_ptr<ByteBuffer> bigEvt = ceb.getBuffer();

        std::cout << "Buf pos = " << bigEvt->position() << ", lim = " << bigEvt->limit() <<
                  ", cap = " << bigEvt->capacity() << std::endl;


        // Create little event (100 bytes total).
        // Let's keep this in 3 different forms to exercise the various writeEvent methods:
        //      1) std::shared_ptr<ByteBuffer>
        //      2) std::shared_ptr<EvioNode>
        //      3) std::shared_ptr<EvioBank>

        bufSize = 120;
        CompactEventBuilder eb(bufSize, ByteOrder::ENDIAN_LOCAL, true);
        eb.openBank(1, DataType::INT32, 1);
        eb.addIntData(dd, 23); // 23 ints = 92 bytes
        std::fill(dd, dd + 23, 3);  // 3 --> 11 bit pattern
        eb.closeAll();

        // ByteBuffer form
        std::shared_ptr<ByteBuffer> littleBuf = eb.getBuffer();

        // EvioBank or EvioEvent form
        EvioReader reader(littleBuf);
        std::shared_ptr<EvioEvent> littleBank = reader.parseEvent(1);

        // EvioNode form
        EvioCompactReader cReader(littleBuf);
        std::shared_ptr<EvioNode> littleNode = cReader.getScannedEvent(1);


        // Write into a file
        uint32_t maxBlockBytes = 900000;  // 900KB
        uint32_t bufferBytes   = 1000000; // 1MB
        uint32_t maxEventCount = 5;

        std::string fname = "./writer_4c.ev";

        EventWriterV4 writer(fname, "", "", 1, 0, maxBlockBytes,
                             maxEventCount, ByteOrder::ENDIAN_LOCAL, "", true, false,
                             littleBank, 0, 0, 1, 1, bufferBytes);

        std::cout << "Write little event 1" << std::endl;
        writer.writeEvent(littleBank, false);
        writer.writeEvent(littleNode, false, true); // use duplicate backing buffer to not change pos, lim
        // This moves the ByteBuffer's position
        writer.writeEvent(littleBuf, false);
        littleBuf->flip(); // lim = pos, pos = 0, get ready for reading again


        std::cout << "WRTER CLOSE" << std::endl;
        writer.close();
    }

}



int main(int argc, char **argv) {
    evio::EventWriterTest();
    return 0;
}


