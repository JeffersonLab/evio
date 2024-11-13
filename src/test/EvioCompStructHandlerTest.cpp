#include <cstdlib>
#include <cstdio>
#include <string>

#include <iostream>
#include <filesystem>


#include "eviocc.h"

namespace evio {



    static std::shared_ptr <EvioEvent> createSingleEvent(uint16_t tag) {

        // Top level event
        std::shared_ptr<EvioEvent> event = nullptr;

        // data
        int intData1[1] = {7};

        int intData2[1] = {8};

        int intData3[1] = {9};

        int intData4[1] = {10};

        try {

            // Build event (bank of banks) with EventBuilder object
            EventBuilder builder(tag, DataType::BANK, 1);
            event = builder.getEvent();

            // bank of ints
            auto bankInts = EvioBank::getInstance(tag + 1, DataType::INT32, 2);
            builder.setIntData(bankInts,intData1,1);
            builder.addChild(event, bankInts);

            // bank of banks
            auto bankBanks = EvioBank::getInstance(tag + 2, DataType::BANK, 3);
            builder.addChild(event, bankBanks);

                // bank of ints
                auto bankInts2 = EvioBank::getInstance(tag + 19, DataType::INT32, 20);
                builder.setIntData(bankInts2,intData2,1);
                builder.addChild(bankBanks, bankInts2);

            // bank of ints
            auto bankInts3 = EvioBank::getInstance(tag + 3, DataType::INT32, 4);
            builder.setIntData(bankInts3,intData3,1);
            builder.addChild(event, bankInts3);

            // bank of ints
            auto bankInts4 = EvioBank::getInstance(tag + 4, DataType::INT32, 5);
            builder.setIntData(bankInts4,intData4,1);
            builder.addChild(event, bankInts4);

        }
        catch (EvioException &e) {
            std::cout << e.what() << std::endl;
        }

        return event;
    }


    static std::shared_ptr <ByteBuffer> createAddBuffer(uint16_t tag, uint8_t num) {

        std::shared_ptr <ByteBuffer> bb = nullptr;
        try {
            CompactEventBuilder builder(4 * 5, ByteOrder::ENDIAN_LITTLE);
            builder.openBank(tag, DataType::BANK, num);
            builder.openBank(tag + 1, DataType::INT32, num + 1);
            int32_t dat[1] = {6};

            builder.addIntData(dat, 1);
            builder.closeAll();
            bb = builder.getBuffer();
        }
        catch (EvioException &e) {
            std::cout << e.what() << std::endl;
        }

        return bb;
    }

}

using namespace evio;

int main(int argc, char **argv) {

    try {

        uint16_t tag = 1;
        uint8_t  num = 1;

        std::shared_ptr<ByteBuffer> buf = std::make_shared<ByteBuffer>(1024);
        buf->order(ByteOrder::ENDIAN_LITTLE);
        std::shared_ptr<EvioEvent> event = createSingleEvent(tag);

        // Evio 6 format (buf cleared (pos -> 0, lim->cap) before writing.
        // Note: header = 14 int + 1 int / event (index) = 4*(14 + 1) bytes = 60 bytes.
        // For evio 6, position just past header = 60 (1 event in buf)
        EventWriter writer(buf);
        writer.writeEvent(event);
        writer.close();
        // The finished buffer is just a duplicate of buf (points to same data, has independent pos/limit).
        // So finishedBuf and buf behave the same.
        std::shared_ptr<ByteBuffer> finishedBuf = writer.getByteBuffer();

        Util::printBytes(finishedBuf, 0, finishedBuf->limit(), "Finished Buffer");

        // Position buffer to after block header (evio 4)
        //buf->limit(32 + 4*16).position(32);

        // Position buffer to after record header (evio 6)
        buf->limit(60 + 4*16).position(60);
        Util::printBytes(buf, 60, 4*16, "Full Event");

        EvioCompactStructureHandler handler(buf, DataType::BANK);
        std::vector<std::shared_ptr<EvioNode>> list = handler.getNodes();

        // Remove last node
        handler.removeStructure(list[5]);

        // Add buffer containing event
        std::shared_ptr<ByteBuffer> bb = createAddBuffer(tag+4, num+4);
        std::shared_ptr<ByteBuffer> newBuf = handler.addStructure(bb);

        Util::printBytes(bb, 0, bb->limit(), "New event");


        // Search event for tag = 2, num = 2
        std::vector<std::shared_ptr<EvioNode>> foundList = handler.searchStructure(2, 2);
        for (auto n: foundList) {
            // Look at each node (structure)
            std::cout << "Found struct with tag = 2, num = 2" << std::endl;
            Util::printBytes(n, n->getTotalBytes(), "found node");
        }


        list = handler.getNodes();
        std::cout << "Got " << list.size() << " nodes after everything" << std::endl;
        int i = 0;
        for (auto n: list) {
            // Look at each node (structure)
            std::shared_ptr <ByteBuffer> dat = handler.getStructureBuffer(list[i]);
            Util::printBytes(dat, dat->position(), dat->limit(), "Struct for node " + std::to_string(i + 1));

            // Look at data for each node (structure)
            std::shared_ptr <ByteBuffer> dd = handler.getData(list[i]);
            Util::printBytes(dd, dd->position(), dd->limit(), "Data for node " + std::to_string(i + 1));
            i++;
        }

    }
    catch (std::runtime_error &e) {
        std::cout << e.what() << std::endl;
    }

    return 0;

}

