//
// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100

#include "TestBase.h"


namespace evio {



    TestBase::TestBase() : TestBase(200000, ByteOrder::ENDIAN_LOCAL) {}


        /**
         * Put boiler plate code for doing tests here.
         * The evio events created by all methods have the same structure and data.
         * In other words, they're identical.
         *
         * @param bufSize size in bytes of internal ByteBuffer.
         * @param byteOrder byte order of internal ByteBuffer.
         */
    TestBase::TestBase(size_t bufSize, ByteOrder const & byteOrder) {

        order = byteOrder;
        buffer = std::make_shared<ByteBuffer>(bufSize);
        buffer->order(order);


        std::cout << "Running with:" << std::endl;
        std::cout << " data elements = " << dataElementCount << std::endl;
        std::cout << "       bufSize = " << bufSize << std::endl;
        std::cout << "         loops = " << bufferLoops << std::endl;
        std::cout << "          runs = " << runLoops << std::endl;
        std::cout << "        useBuf = " << useBuf << std::endl;
        std::cout << "      old evio = " << oldEvio << std::endl;

        setDataSize(dataElementCount);

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

        dictionary = ss.str();

        std::cout << "Const: dictionary = " << dictionary << std::endl;
    }


    /**
     * Create the data for constructed evio events.
     * @param elementCount number of data elements in each data structure.
     */
    void TestBase::setDataSize(int elementCount) {

        int1    = new int32_t[elementCount];
        byte1   = new char[elementCount];
        short1  = new int16_t[elementCount];
        long1   = new int64_t[elementCount];

        uint1    = new uint32_t[elementCount];
        ubyte1   = new unsigned char[elementCount];
        ushort1  = new uint16_t[elementCount];
        ulong1   = new uint64_t[elementCount];


        float1 = new float[elementCount];
        double1 = new double[elementCount];

        intVec.reserve(elementCount);
        byteVec.reserve(elementCount);
        shortVec.reserve(elementCount);
        longVec.reserve(elementCount);

        uintVec.reserve(elementCount);
        ubyteVec.reserve(elementCount);
        ushortVec.reserve(elementCount);
        ulongVec.reserve(elementCount);

        floatVec.reserve(elementCount);
        doubleVec.reserve(elementCount);
        stringsVec.reserve(elementCount);
        cDataVec.reserve(elementCount);

        for (int i = 0; i < elementCount; i++) {
            int1[i]  = i + 1;
            uint1[i] = i + 1;

            byte1[i]  = (char)  ((i + 1) % std::numeric_limits<char>::max());
            ubyte1[i] = (unsigned char) ((i + 1) % std::numeric_limits<unsigned char>::max());

            short1[i] = (int16_t)  ((i + 1) % std::numeric_limits<int16_t>::max());
            ushort1[i] = (uint16_t) ((i + 1) % std::numeric_limits<uint16_t>::max());

            long1[i] = i + 1;
            ulong1[i] = i + 1;

            float1[i]  = (float) (i + 1);
            double1[i] = (double) (i + 1);

            intVec.push_back(int1[i]);
            uintVec.push_back(uint1[i]);

            byteVec.push_back(byte1[i]);
            ubyteVec.push_back(ubyte1[i]);

            shortVec.push_back(short1[i]);
            ushortVec.push_back(ushort1[i]);

            longVec.push_back(long1[i]);
            ulongVec.push_back(ulong1[i]);

            floatVec.push_back(float1[i]);
            doubleVec.push_back(double1[i]);

            stringsVec.push_back("0x" + std::to_string(i + 1));

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
            auto cData = CompositeData::getInstance(format, myData, 1, 1, 1, order);
            cDataVec.push_back(cData);
        }
    }


    /** Create a test Evio Event in ByteBuffer form using CompactEventBuilder. */
    std::shared_ptr<ByteBuffer> TestBase::createCompactEventBuffer(uint16_t tag, uint8_t num,
                                                                   ByteOrder const & byteOrder,
                                                                   size_t bSize,
                                                                   std::shared_ptr<CompactEventBuilder> builder) {

        if (builder == nullptr) {
            std::shared_ptr <ByteBuffer> buf = std::make_shared<ByteBuffer>(bSize);
            buf->order(byteOrder);
            builder = std::make_shared<CompactEventBuilder>(buf);
        }

        // add top/event level bank of banks
        builder->openBank(tag, DataType::BANK, num);

        // add bank of banks
        builder->openBank(tag + 1, DataType::BANK, num + 1);

        // add bank of ints
        builder->openBank(tag + 2, DataType::UINT32, num + 2);
        builder->addUIntData(uint1, dataElementCount);
        builder->closeStructure();

        // add bank of bytes
        builder->openBank(tag + 3, DataType::UCHAR8, num + 3);
        builder->addUCharData(ubyte1, dataElementCount);
        builder->closeStructure();

        // add bank of shorts
        builder->openBank(tag + 4, DataType::USHORT16, num + 4);
        builder->addUShortData(ushort1, dataElementCount);
        builder->closeStructure();

        // add bank of longs
        builder->openBank(tag + 40, DataType::ULONG64, num + 40);
        builder->addULongData(ulong1, dataElementCount);
        builder->closeStructure();

        // add bank of floats
        builder->openBank(tag + 5, DataType::FLOAT32, num + 5);
        builder->addFloatData(float1, dataElementCount);
        builder->closeStructure();

        // add bank of doubles
        builder->openBank(tag + 6, DataType::DOUBLE64, num + 6);
        builder->addDoubleData(double1, dataElementCount);
        builder->closeStructure();

        // add bank of strings
        builder->openBank(tag + 7, DataType::CHARSTAR8, num + 7);
        builder->addStringData(stringsVec);
        builder->closeStructure();

        // add bank of composite data
        builder->openBank(tag + 100, DataType::COMPOSITE, num + 100);
        builder->addCompositeData(cDataVec);

        builder->closeStructure();



        // add bank of segs
        builder->openBank(tag + 14, DataType::SEGMENT, num + 14);

        // add seg of ints
        builder->openSegment(tag + 8, DataType::INT32);
        builder->addIntData(int1, dataElementCount);
        builder->closeStructure();

        // add seg of bytes
        builder->openSegment(tag + 9, DataType::CHAR8);
        builder->addCharData(byte1, dataElementCount);
        builder->closeStructure();

        // add seg of shorts
        builder->openSegment(tag + 10, DataType::SHORT16);
        builder->addShortData(short1, dataElementCount);
        builder->closeStructure();

        // add seg of longs
        builder->openSegment(tag + 40, DataType::LONG64);
        builder->addLongData(long1, dataElementCount);
        builder->closeStructure();

        // add seg of floats
        builder->openSegment(tag + 11, DataType::FLOAT32);
        builder->addFloatData(float1, dataElementCount);
        builder->closeStructure();

        // add seg of doubles
        builder->openSegment(tag + 12, DataType::DOUBLE64);
        builder->addDoubleData(double1, dataElementCount);
        builder->closeStructure();

        // add seg of strings
        builder->openSegment(tag + 13, DataType::CHARSTAR8);
        builder->addStringData(stringsVec);
        builder->closeStructure();

        builder->closeStructure();


        // add bank of tagsegs
        builder->openBank(tag + 15, DataType::TAGSEGMENT, num + 15);

        // add tagseg of ints
        builder->openTagSegment(tag + 16, DataType::UINT32);
        builder->addIntData(int1, dataElementCount);
        builder->closeStructure();

        // add tagseg of bytes
        builder->openTagSegment(tag + 17, DataType::UCHAR8);
        builder->addUCharData(ubyte1, dataElementCount);
        builder->closeStructure();

        // add tagseg of shorts
        builder->openTagSegment(tag + 18, DataType::USHORT16);
        builder->addUShortData(ushort1, dataElementCount);
        builder->closeStructure();

        // add tagseg of longs
        builder->openTagSegment(tag + 40, DataType::ULONG64);
        builder->addULongData(ulong1, dataElementCount);
        builder->closeStructure();

        // add tagseg of floats
        builder->openTagSegment(tag + 19, DataType::FLOAT32);
        builder->addFloatData(float1, dataElementCount);
        builder->closeStructure();

        // add tagseg of doubles
        builder->openTagSegment(tag + 20, DataType::DOUBLE64);
        builder->addDoubleData(double1, dataElementCount);
        builder->closeStructure();

        // add tagseg of strings
        builder->openTagSegment(tag + 21, DataType::CHARSTAR8);
        builder->addStringData(stringsVec);
        builder->closeStructure();

        builder->closeAll();

        // Make this call to set proper pos & lim
        return builder->getBuffer();
    }



    /** Writing to a buffer using EventBuilder interface. */
    std::shared_ptr<ByteBuffer> TestBase::createEventBuilderBuffer(uint16_t tag, uint8_t num,
                                                                   ByteOrder const & byteOrder,
                                                                   size_t bSize) {

        std::shared_ptr<ByteBuffer> buf = std::make_shared<ByteBuffer>(bSize);
        buf->order(byteOrder);

        std::shared_ptr<EvioEvent> event = createEventBuilderEvent(tag, num);
        // Take event & write it into buffer
        event->write(*(buf.get()));
        buf->flip();
        return buf;
    }


    /** Writing to a buffer using EventBuilder interface. */
    std::shared_ptr<EvioEvent> TestBase::createEventBuilderEvent(uint16_t tag, uint8_t num) {

        try {
            // Build event (bank of banks) with EventBuilder object
            EventBuilder builder(tag, DataType::BANK, num);
            auto event = builder.getEvent();

            // bank of banks
            auto bankBanks = EvioBank::getInstance(tag + 1, DataType::BANK, num + 1);
            builder.addChild(event, bankBanks);

            // bank of ints
            auto bankInts = EvioBank::getInstance(tag + 2, DataType::UINT32, num + 2);
            builder.setUIntData(bankInts, uintVec.data(), dataElementCount);
            builder.addChild(bankBanks, bankInts);

            // bank of bytes
            auto bankBytes = EvioBank::getInstance(tag + 3, DataType::UCHAR8, num + 3);
            builder.setUCharData(bankBytes, ubyteVec.data(), dataElementCount);
            builder.addChild(bankBanks, bankBytes);

            // bank of shorts
            auto bankShorts = EvioBank::getInstance(tag + 4, DataType::USHORT16, num + 4);
            builder.setUShortData(bankShorts, ushortVec.data(), dataElementCount);
            builder.addChild(bankBanks, bankShorts);

            // bank of longs
            auto bankLongs = EvioBank::getInstance(tag + 40, DataType::ULONG64, num + 40);
            builder.setULongData(bankLongs, ulongVec.data(), dataElementCount);
            builder.addChild(bankBanks, bankLongs);

            // bank of floats
            auto bankFloats = EvioBank::getInstance(tag + 5, DataType::FLOAT32, num + 5);
            builder.setFloatData(bankFloats, floatVec.data(), dataElementCount);
            builder.addChild(bankBanks, bankFloats);

            // bank of doubles
            auto bankDoubles = EvioBank::getInstance(tag + 6, DataType::DOUBLE64, num + 6);
            builder.setDoubleData(bankDoubles, doubleVec.data(), dataElementCount);
            builder.addChild(bankBanks, bankDoubles);

            // bank of strings
            auto bankStrings = EvioBank::getInstance(tag + 7, DataType::CHARSTAR8, num + 7);
            builder.setStringData(bankStrings, stringsVec.data(), dataElementCount);
            builder.addChild(bankBanks, bankStrings);

            // add bank of composite data
            auto bankComps = EvioBank::getInstance(tag + 100, DataType::COMPOSITE, num + 100);
            builder.setCompositeData(bankComps, cDataVec.data(), dataElementCount);
            builder.addChild(bankBanks, bankComps);



            // bank of segments
            auto bankSegs = EvioBank::getInstance(tag + 14, DataType::SEGMENT, num + 14);
            builder.addChild(event, bankSegs);

            // seg of ints
            auto segInts = EvioSegment::getInstance(tag + 8, DataType::INT32);
            builder.setIntData(segInts, intVec.data(), dataElementCount);
            builder.addChild(bankSegs, segInts);

            // seg of bytes
            auto segBytes = EvioSegment::getInstance(tag + 9, DataType::CHAR8);
            builder.setCharData(segBytes, byteVec.data(), dataElementCount);
            builder.addChild(bankSegs, segBytes);

            // seg of shorts
            auto segShorts = EvioSegment::getInstance(tag + 10, DataType::SHORT16);
            builder.setShortData(segShorts, shortVec.data(), dataElementCount);
            builder.addChild(bankSegs, segShorts);

            // seg of longs
            auto segLongs = EvioSegment::getInstance(tag + 40, DataType::LONG64);
            builder.setLongData(segLongs, longVec.data(), dataElementCount);
            builder.addChild(bankSegs, segLongs);

            // seg of floats
            auto segFloats = EvioSegment::getInstance(tag + 11, DataType::FLOAT32);
            builder.setFloatData(segFloats, floatVec.data(), dataElementCount);
            builder.addChild(bankSegs, segFloats);

            // seg of doubles
            auto segDoubles = EvioSegment::getInstance(tag + 12, DataType::DOUBLE64);
            builder.setDoubleData(segDoubles, doubleVec.data(), dataElementCount);
            builder.addChild(bankSegs, segDoubles);

            // seg of strings
            auto segStrings = EvioSegment::getInstance(tag + 13, DataType::CHARSTAR8);
            builder.setStringData(segStrings, stringsVec.data(), dataElementCount);
            builder.addChild(bankSegs, segStrings);



            // bank of tagsegments
            auto bankTsegs = EvioBank::getInstance(tag + 15, DataType::TAGSEGMENT, num + 15);
            builder.addChild(event, bankTsegs);

            // tagsegments of ints
            auto tsegInts = EvioTagSegment::getInstance(tag + 16, DataType::UINT32);
            builder.setUIntData(tsegInts, uintVec.data(), dataElementCount);
            builder.addChild(bankTsegs, tsegInts);

            // tagsegments of bytes
            auto tsegBytes = EvioTagSegment::getInstance(tag + 17, DataType::UCHAR8);
            builder.setUCharData(tsegBytes, ubyteVec.data(), dataElementCount);
            builder.addChild(bankTsegs, tsegBytes);

            // tagsegments of shorts
            auto tsegShorts = EvioTagSegment::getInstance(tag + 18, DataType::USHORT16);
            builder.setUShortData(tsegShorts, ushortVec.data(), dataElementCount);
            builder.addChild(bankTsegs, tsegShorts);

            // tagsegments of longs
            auto tsegLongs = EvioTagSegment::getInstance(tag + 40, DataType::ULONG64);
            builder.setULongData(tsegLongs, ulongVec.data(), dataElementCount);
            builder.addChild(bankTsegs, tsegLongs);

            // tagsegments of floats
            auto tsegFloats = EvioTagSegment::getInstance(tag + 19, DataType::FLOAT32);
            builder.setFloatData(tsegFloats, floatVec.data(), dataElementCount);
            builder.addChild(bankTsegs, tsegFloats);

            // tagsegments of doubles
            auto tsegDoubles = EvioTagSegment::getInstance(tag + 20, DataType::DOUBLE64);
            builder.setDoubleData(tsegDoubles, doubleVec.data(), dataElementCount);
            builder.addChild(bankTsegs, tsegDoubles);

            // tagsegments of strings
            auto tsegStrings = EvioTagSegment::getInstance(tag + 21, DataType::CHARSTAR8);
            builder.setStringData(tsegStrings, stringsVec.data(), dataElementCount);
            builder.addChild(bankTsegs, tsegStrings);

            return event;

//            std::cout << "Event:\n" << event->treeToString("") << std::endl;
        }
        catch (EvioException &e) {
            std::cout << e.what() << std::endl;
        }

        return nullptr;
    }



    /** Writing to a buffer using EventBuilder interface. */
    std::shared_ptr<ByteBuffer> TestBase::createTreeBuffer(uint16_t tag, uint8_t num,
                                                           ByteOrder const & byteOrder,
                                                           size_t bSize) {

        std::shared_ptr<ByteBuffer> buf = std::make_shared<ByteBuffer>(bSize);
        buf->order(byteOrder);

        std::shared_ptr<EvioEvent> event = createTreeEvent(tag, num);
        // Take event & write it into buffer
        event->write(*(buf.get()));
        buf->flip();
        return buf;
    }



    /** Writing to a buffer using original evio tree interface. */
    std::shared_ptr<EvioEvent> TestBase::createTreeEvent(uint16_t tag, uint8_t num) {

        try {
            // Use event constructor and insert() calls
            std::shared_ptr<EvioEvent> event = EvioEvent::getInstance(tag, DataType::BANK, num);

            // bank of banks
            auto bankBanks = EvioBank::getInstance(tag + 1, DataType::BANK, num + 1);
            event->insert(bankBanks, 0);

            // bank of ints
            auto bankInts = EvioBank::getInstance(tag + 2, DataType::UINT32, num + 2);
            auto &iData = bankInts->getUIntData();
            iData.insert(iData.begin(), uintVec.begin(), uintVec.end());
            bankInts->updateUIntData();
            bankBanks->insert(bankInts, 0);

            // bank of bytes
            auto bankBytes = EvioBank::getInstance(tag + 3, DataType::UCHAR8, num + 3);
            auto &cData = bankBytes->getUCharData();
            cData.insert(cData.begin(), ubyteVec.begin(), ubyteVec.end());
            bankBytes->updateUCharData();
            bankBanks->insert(bankBytes, 1);

            // bank of shorts
            auto bankShorts = EvioBank::getInstance(tag + 4, DataType::USHORT16, num + 4);
            auto &sData = bankShorts->getUShortData();
            sData.insert(sData.begin(), ushortVec.begin(), ushortVec.end());
            bankShorts->updateUShortData();
            bankBanks->insert(bankShorts, 2);

            // bank of longs
            auto bankLongs = EvioBank::getInstance(tag + 40, DataType::ULONG64, num + 40);
            auto &lData = bankLongs->getULongData();
            lData.insert(lData.begin(), ulongVec.begin(), ulongVec.end());
            bankLongs->updateULongData();
            bankBanks->insert(bankLongs, 3);

            // bank of floats
            auto bankFloats = EvioBank::getInstance(tag + 5, DataType::FLOAT32, num + 5);
            auto &fData = bankFloats->getFloatData();
            fData.insert(fData.begin(), floatVec.begin(), floatVec.end());
            bankFloats->updateFloatData();
            bankBanks->insert(bankFloats, 4);

            // bank of doubles
            auto bankDoubles = EvioBank::getInstance(tag + 6, DataType::DOUBLE64, num + 6);
            auto &dData = bankDoubles->getDoubleData();
            dData.insert(dData.begin(), doubleVec.begin(), doubleVec.end());
            bankDoubles->updateDoubleData();
            bankBanks->insert(bankDoubles, 5);

            // bank of strings
            auto bankStrings = EvioBank::getInstance(tag + 7, DataType::CHARSTAR8, num + 7);
            auto &stData = bankStrings->getStringData();
            stData.insert(stData.begin(), stringsVec.begin(), stringsVec.end());
            bankStrings->updateStringData();
            bankBanks->insert(bankStrings, 6);

            // bank of composite data
            auto bankComps = EvioBank::getInstance(tag + 1000, DataType::COMPOSITE, num + 1000);
            auto &oldCompData = bankComps->getCompositeData();
            oldCompData.insert(oldCompData.begin(), cDataVec.begin(), cDataVec.end());
            bankComps->updateCompositeData();
            bankBanks->insert(bankComps, 7);




            // bank of segments
            auto bankSegs = EvioBank::getInstance(tag + 14, DataType::SEGMENT, num + 14);
            event->insert(bankSegs, 1);

            // seg of ints
            auto segInts = EvioSegment::getInstance(tag + 8, DataType::INT32);
            auto &siData = segInts->getIntData();
            siData.insert(siData.begin(), intVec.begin(), intVec.end());
            segInts->updateIntData();
            bankSegs->insert(segInts, 0);

            // seg of bytes
            auto segBytes = EvioSegment::getInstance(tag + 9, DataType::CHAR8);
            auto &scData = segBytes->getCharData();
            scData.insert(scData.begin(), byteVec.begin(), byteVec.end());
            segBytes->updateCharData();
            bankSegs->insert(segBytes, 1);

            // seg of shorts
            auto segShorts = EvioSegment::getInstance(tag + 10, DataType::SHORT16);
            auto &ssData = segShorts->getShortData();
            ssData.insert(ssData.begin(), shortVec.begin(), shortVec.end());
            segShorts->updateShortData();
            bankSegs->insert(segShorts, 2);

            // seg of longs
            auto segLongs = EvioSegment::getInstance(tag + 40, DataType::LONG64);
            auto &slData = segLongs->getLongData();
            slData.insert(slData.begin(), longVec.begin(), longVec.end());
            segLongs->updateLongData();
            bankSegs->insert(segLongs, 3);

            // seg of floats
            auto segFloats = EvioSegment::getInstance(tag + 11, DataType::FLOAT32);
            auto &sfData = segFloats->getFloatData();
            sfData.insert(sfData.begin(), floatVec.begin(), floatVec.end());
            segFloats->updateFloatData();
            bankSegs->insert(segFloats, 4);

            // seg of doubles
            auto segDoubles = EvioSegment::getInstance(tag + 12, DataType::DOUBLE64);
            auto &sdData = segDoubles->getDoubleData();
            sdData.insert(sdData.begin(), doubleVec.begin(), doubleVec.end());
            segDoubles->updateDoubleData();
            bankSegs->insert(segDoubles, 5);

            // seg of strings
            auto segStrings = EvioSegment::getInstance(tag + 13, DataType::CHARSTAR8);
            auto &sstData = segStrings->getStringData();
            sstData.insert(sstData.begin(), stringsVec.begin(), stringsVec.end());
            segStrings->updateStringData();
            bankSegs->insert(segStrings, 6);



            // bank of tagsegments
            auto bankTsegs = EvioBank::getInstance(tag + 15, DataType::TAGSEGMENT, num + 15);
            event->insert(bankTsegs, 2);

            // tagsegments of ints
            auto tsegInts = EvioTagSegment::getInstance(tag + 16, DataType::UINT32);
            auto &tiData = tsegInts->getUIntData();
            tiData.insert(tiData.begin(), uintVec.begin(), uintVec.end());
            tsegInts->updateUIntData();
            bankTsegs->insert(tsegInts, 0);

            // tagsegments of bytes
            auto tsegBytes = EvioTagSegment::getInstance(tag + 17, DataType::UCHAR8);
            auto &tcData = tsegBytes->getUCharData();
            tcData.insert(tcData.begin(), ubyteVec.begin(), ubyteVec.end());
            tsegBytes->updateUCharData();
            bankTsegs->insert(tsegBytes, 1);

            // tagsegments of shorts
            auto tsegShorts = EvioTagSegment::getInstance(tag + 18, DataType::USHORT16);
            auto &tsData = tsegShorts->getUShortData();
            tsData.insert(tsData.begin(), ushortVec.begin(), ushortVec.end());
            tsegShorts->updateUShortData();
            bankTsegs->insert(tsegShorts, 2);

            // tagsegments of longs
            auto tsegLongs = EvioTagSegment::getInstance(tag + 40, DataType::ULONG64);
            auto &tlData = tsegLongs->getULongData();
            tlData.insert(tlData.begin(), ulongVec.begin(), ulongVec.end());
            tsegLongs->updateULongData();
            bankTsegs->insert(tsegLongs, 3);

            // tagsegments of floats
            auto tsegFloats = EvioTagSegment::getInstance(tag + 19, DataType::FLOAT32);
            auto &tfData = tsegFloats->getFloatData();
            tfData.insert(tfData.begin(), floatVec.begin(), floatVec.end());
            tsegFloats->updateFloatData();
            bankTsegs->insert(tsegFloats, 4);

            // tagsegments of doubles
            auto tsegDoubles = EvioTagSegment::getInstance(tag + 20, DataType::DOUBLE64);
            auto &tdData = tsegDoubles->getDoubleData();
            tdData.insert(tdData.begin(), doubleVec.begin(), doubleVec.end());
            tsegDoubles->updateDoubleData();
            bankTsegs->insert(tsegDoubles, 5);

            // tagsegments of strings
            auto tsegStrings = EvioTagSegment::getInstance(tag + 21, DataType::CHARSTAR8);
            auto &tstData = tsegStrings->getStringData();
            tstData.insert(tstData.begin(), stringsVec.begin(), stringsVec.end());
            tsegStrings->updateStringData();
            bankTsegs->insert(tsegStrings, 5);

            return event;
        }
        catch (EvioException &e) {
            std::cout << e.what() << std::endl;
        }

        return nullptr;
    }




}