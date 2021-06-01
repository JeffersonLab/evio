//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_READER_H
#define EVIO_6_0_READER_H


#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <ios>
#include <iostream>
#include <stdexcept>
#include <memory>


#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "FileHeader.h"
#include "RecordHeader.h"
#include "FileEventIndex.h"
#include "RecordInput.h"
#include "EvioException.h"
#include "EvioNode.h"
#include "IBlockHeader.h"
#include "Util.h"


namespace evio {

    /**
     * Reader class that reads files stored in the HIPO format.<p>
     *
     * <pre><code>
     * File has this structure:
     *
     *    +----------------------------------+
     *    |      General File Header         |
     *    +----------------------------------+
     *    +----------------------------------+
     *    |         Index (optional)         |
     *    +----------------------------------+
     *    +----------------------------------+
     *    |     User Header (optional)       |
     *    +----------------------------------+
     *    +----------------------------------+
     *    |                                  |
     *    |            Record 1              |
     *    |                                  |
     *    |                                  |
     *    |                                  |
     *    +----------------------------------+
     *                   ...
     *    +----------------------------------+
     *    |                                  |
     *    |            Record N              |
     *    |                                  |
     *    |                                  |
     *    |                                  |
     *    +----------------------------------+
     *    +----------------------------------+
     *    |       Trailer (optional)         |
     *    +----------------------------------+
     *    +----------------------------------+
     *    |    Trailer's Index (optional)    |
     *    +----------------------------------+
     *
     *
     *
     * Buffer or streamed data has this structure:
     *
     *    +----------------------------------+
     *    |                                  |
     *    |            Record 1              |
     *    |                                  |
     *    |                                  |
     *    |                                  |
     *    +----------------------------------+
     *                   ...
     *    +----------------------------------+
     *    |                                  |
     *    |            Record N              |
     *    |                                  |
     *    |                                  |
     *    |                                  |
     *    +----------------------------------+
     *    +----------------------------------+
     *    |       Trailer (optional)         |
     *    +----------------------------------+
     *
     * The important thing with a buffer or streaming is for the last header or
     * trailer to set the "last record" bit.
     *
     * </code></pre>
     *
     * Something to keep in mind is one can intersperse sequential calls
     * (getNextEvent, getPrevEvent, or getNextEventNode) with random access
     * calls (getEvent or getEventNode), and the sequence remains unchanged
     * after the random access.
     *
     * @version 6.0
     * @since 6.0 08/10/2017
     * @author gavalian (original Java)
     * @author timmer
     * @see FileHeader
     * @see RecordInput
     */
    class Reader {

        friend class EvioCompactReaderV6;

    private:


        /**
         * Internal class to keep track of the records in the file/buffer.
         * Each entry keeps record position in the file/buffer, length of
         * the record and number of entries contained.
         */
        class RecordPosition {

        private:

            /** Position in file/buffer. */
            size_t position;

            /** Length in bytes. */
            uint32_t length;

            /** Number of entries in record. */
            uint32_t count;

        public:

            explicit RecordPosition(size_t pos) {
                count = length = 0;
                position = pos;
            }

            RecordPosition(size_t pos, uint32_t len, uint32_t cnt) {
                position = pos;
                length = len;
                count = cnt;
            }

            RecordPosition setPosition(size_t _pos) {
                position = _pos;
                return *this;
            }

            RecordPosition setLength(uint32_t _len) {
                length = _len;
                return *this;
            }

            RecordPosition setCount(uint32_t _cnt) {
                count = _cnt;
                return *this;
            }

            size_t getPosition() const { return position; }

            uint32_t getLength() const { return length; }

            uint32_t getCount() const { return count; }

            std::string toString() const {
                std::stringstream ss;
                ss << " POSITION = " << std::setw(16) << position << ", LENGTH = " << std::setw(12) <<
                        length << ", COUNT = " << std::setw(8) << count << std::endl;
                return ss.str();
            }
        };


        /** Size of array in which to store record header info. */
        static const uint32_t headerInfoLen = 8;


        /**
         * Vector of records in the file. The vector is initialized
         * when the entire file is scanned to read out positions
         * of each record in the file (in constructor).
         */
        std::vector<RecordPosition> recordPositions;
        /** Object for reading file. */
        std::ifstream inStreamRandom;
        /** File name. */
        std::string fileName {""};
        /** File size in bytes. */
        size_t fileSize = 0;
        /** File header. */
        FileHeader fileHeader;
        /** Are we reading from file (true) or buffer? */
        bool fromFile = true;


        /** Buffer being read. */
        std::shared_ptr<ByteBuffer> buffer = nullptr;
        /** Initial position of buffer. */
        size_t bufferOffset = 0;
        /** Limit of buffer. */
        size_t bufferLimit = 0;


        /** Keep one record for reading in data record-by-record. */
        RecordInput inputRecordStream;
        /** Number or position of last record to be read. */
        uint32_t currentRecordLoaded = 0;
        // TODO: Look at this
        /** First record's header. */
        std::shared_ptr<RecordHeader> firstRecordHeader = nullptr;
        /** Record number expected when reading. Used to check sequence of records. */
        uint32_t recordNumberExpected = 1;
        /** If true, throw an exception if record numbers are out of sequence. */
        bool checkRecordNumberSequence = false;
        /** Object to handle event indexes in context of file and having to change records. */
        FileEventIndex eventIndex;


        /** Files may have an xml format dictionary in the user header of the file header. */
        std::string dictionaryXML {""};
        /** Each file of a set of split CODA files may have a "first" event common to all. */
        std::shared_ptr<uint8_t> firstEvent = nullptr;
        /** First event size in bytes. */
        uint32_t firstEventSize = 0;

        // TODO: The HIPO library is NOT evio dependent!!!!

        /** Stores info of all the (top-level) events in a scanned buffer. */
        std::vector<std::shared_ptr<EvioNode>> eventNodes;


        /** Is this object currently closed? */
        bool closed = false;
        /** Is this data in file/buffer compressed? */
        bool compressed = false;
        /** Byte order of file/buffer being read. */
        ByteOrder byteOrder {ByteOrder::ENDIAN_LOCAL};
        /** Keep track of next EvioNode when calling {@link #getNextEventNode()},
        * {@link #getEvent(uint32_t, uint32_t *)}, or {@link #getPrevEvent(uint32_t *)}. */
        int32_t sequentialIndex = -1;

        /**
         * If this buf/file contains non-evio events (permissible to read in this class),
         * set this flag to false, which helps EvioCompactReader and EvioReader to avoid
         * choking while trying to parse them.
         */
        bool evioFormat = true;

        /** If true, the last sequential call was to getNextEvent or getNextEventNode.
         *  If false, the last sequential call was to getPrevEvent. Used to determine
         *  which event is prev or next. */
        bool lastCalledSeqNext = false;
        /** Evio version of file/buffer being read. */
        uint32_t evioVersion = 6;



        void setByteOrder(ByteOrder & order);
        static uint32_t getTotalByteCounts(ByteBuffer & buf, uint32_t* info, uint32_t infoLen);
        static uint32_t getTotalByteCounts(std::shared_ptr<ByteBuffer> & buf, uint32_t* info, uint32_t infoLen);
        //static std::string getStringArray(ByteBuffer & buffer, int wrap, int max);
        //static std::string getHexStringInt(int32_t value);

    public:

        Reader();
        explicit Reader(std::string const & filename);
        Reader(std::string const & filename, bool forceScan);
        explicit Reader(std::shared_ptr<ByteBuffer> & buffer, bool checkRecordNumSeq = false);

        ~Reader() = default;

        void open(std::string const & filename, bool scan = true);
        void close();

        bool isClosed() const;
        bool isFile() const;

        std::string getFileName() const;
        size_t getFileSize() const;

        void setBuffer(std::shared_ptr<ByteBuffer> & buf);
        std::shared_ptr<ByteBuffer> getBuffer();
        size_t getBufferOffset() const;

        FileHeader & getFileHeader();
        std::shared_ptr<RecordHeader> & getFirstRecordHeader();

        ByteOrder & getByteOrder();
        uint32_t getVersion() const;
        bool isCompressed() const;
        bool isEvioFormat() const;
        std::string getDictionary();
        bool hasDictionary() const;

        std::shared_ptr<uint8_t> & getFirstEvent(uint32_t *size);
        uint32_t getFirstEventSize();
        bool hasFirstEvent() const;

        uint32_t getEventCount() const;
        uint32_t getRecordCount() const;

        std::vector<RecordPosition> & getRecordPositions();
        std::vector<std::shared_ptr<EvioNode>> & getEventNodes();

        bool getCheckRecordNumberSequence() const;

        uint32_t getNumEventsRemaining() const;

        std::shared_ptr<uint8_t> getNextEvent(uint32_t * len);
        std::shared_ptr<uint8_t> getPrevEvent(uint32_t * len);

        std::shared_ptr<EvioNode> getNextEventNode();
        std::shared_ptr<ByteBuffer> readUserHeader();

        std::shared_ptr<uint8_t> getEvent(uint32_t index, uint32_t * len);
        ByteBuffer & getEvent(ByteBuffer & buf, uint32_t index);
        std::shared_ptr<ByteBuffer> getEvent(std::shared_ptr<ByteBuffer> & buf, uint32_t index);
        uint32_t getEventLength(uint32_t index);
        std::shared_ptr<EvioNode> getEventNode(uint32_t index);

        bool hasNext() const;
        bool hasPrev() const;

        uint32_t getRecordEventCount() const;
        uint32_t getCurrentRecord() const;
        RecordInput & getCurrentRecordStream();
        bool readRecord(uint32_t index);


    protected:

        void extractDictionaryAndFirstEvent();
        void extractDictionaryFromBuffer();
        void extractDictionaryFromFile();


        static void findRecordInfo(std::shared_ptr<ByteBuffer> & buf, uint32_t offset,
                                   uint32_t* info, uint32_t infoLen);
        static void findRecordInfo(ByteBuffer & buf, uint32_t offset,
                                   uint32_t* info, uint32_t infoLen);


        std::shared_ptr<ByteBuffer> scanBuffer();
        void scanUncompressedBuffer();
        void forceScanFile();
        void scanFile(bool force);

        // The next 2 methods will not work on events which are not evio format data.
        // They are included here so other classes, like EvioCompactReader and EvioReader,
        // can use their APIs to call the new evio version 6 classes like this one.
        // These new classes were initially designed to be data format agnostic, but adding
        // these methods violates that.
        std::shared_ptr<ByteBuffer> & addStructure(uint32_t eventNumber, ByteBuffer & addBuffer);
        std::shared_ptr<ByteBuffer> & removeStructure(std::shared_ptr<EvioNode> & removeNode);

        void show() const;

        //int main(int argc, char **argv);

    };

}


#endif //EVIO_6_0_READER_H
