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


    static std::shared_ptr <ByteBuffer> createBuffer(int tag, int num) {

        auto bank = createSingleEvent(tag);
        int byteSize = bank->getTotalBytes();

        std::shared_ptr <ByteBuffer> bb = std::make_shared<ByteBuffer>(byteSize);
        bank->write(bb);
        bb->flip();

        return bb;
    }


    static std::shared_ptr <ByteBuffer> createAddBuffer(uint16_t tag, uint8_t num) {

        std::shared_ptr <ByteBuffer> bb = nullptr;
        try {
            CompactEventBuilder builder(4 * 5, ByteOrder::ENDIAN_BIG);
            builder.openBank(tag, DataType::BANK, num);
            builder.openBank(tag + 1, DataType::INT32, num + 1);
            uint32_t dat[1] = {6};

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

    /** For creating a local evio buffer, put into EvioCompactStructureHandler,
     *  remove a node, then examine resulting buffer. */
    int main(int argc, char **argv) {

        try {

            uint16_t tag = 1;

            std::shared_ptr<ByteBuffer> buf = std::make_shared<ByteBuffer>(1024);
            buf->order(ByteOrder::ENDIAN_BIG);
            std::shared_ptr<EvioEvent> event = createSingleEvent(tag);
            std::cout << "After creation, ev size = " << event->getTotalBytes() << std::endl;
            std::cout << "After creation, ev header len = " << event->getHeader()->getDataLength() << std::endl;
            std::cout << "Before writer, raw buf pos = " << buf->position() << ", lim = " << buf->limit() << std::endl;

            // Evio 6 format (buf cleared (pos -> 0, lim->cap) before writing.
            EventWriter writer(buf);
            writer.writeEvent(event);
            writer.close();

            std::shared_ptr<ByteBuffer> finishedBuf = writer.getByteBuffer();
            std::cout << "After writer, finished buf pos = " << finishedBuf->position() << ", lim = " << finishedBuf->limit() << ", first int = " << finishedBuf->getInt(60) << std::endl;
         //   Util::printBytes(finishedBuf, 56, 4*16, "FINISHED EVENT");
            Util::printBytes(finishedBuf, 0, 4*35, "FINISHED EVENT");

            std::cout << "After writer, raw buf pos = " << buf->position() << ", lim = " << buf->limit() << std::endl;
            Util::printBytes(buf, 60, 4*16, "RAW EVENT");

            // Position buffer to after block header (evio 4)
            //buf->limit(32 + 4 * 13).position(32);

            // Position buffer to after block header (evio 6)
            buf->limit(60 + 4*16).position(60);
            Util::printBytes(buf, 60, 4*16, "EVENT");

            EvioCompactStructureHandler handler(buf, DataType::BANK);
            std::vector<std::shared_ptr<EvioNode>> list = handler.getNodes();

            // Remove  node
            handler.removeStructure(list[5]);

            // buffer to add
            std::shared_ptr<ByteBuffer> bb = createAddBuffer(tag+4, tag+4);
            std::shared_ptr<ByteBuffer> newBuf = handler.addStructure(bb);

            Util::printBytes(bb, 0, bb->limit(), "New event");

            list = handler.getNodes();
            std::cout << "Got " << list.size() << " nodes after everything" << std::endl;
            int i = 0;
            for (auto n: list) {
                // Look at data for 3rd node
                std::shared_ptr <ByteBuffer> dd = handler.getStructureBuffer(list[i]);
                Util::printBytes(dd, dd->position(), dd->limit(), "Struct buf for node " + std::to_string(++i));
            }

        }
        catch (std::runtime_error &e) {
            std::cout << e.what() << std::endl;
        }

        return 0;

    }


