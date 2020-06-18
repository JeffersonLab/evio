/**
 * Copyright (c) 2019, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 07/18/2019
 * @author timmer
 */

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
#include "EvioNodeSource.h"
#include "IBlockHeader.h"

using namespace std;


namespace evio {


class Reader {


private:


    /**
     * Internal class to keep track of the records in the file/buffer.
     * Each entry keeps record position in the file/buffer, length of
     * the record and number of entries contained.
     */
    class RecordPosition {

    private:
        /** Position in file/buffer. */
        uint64_t position;

        /** Length in bytes. */
        uint32_t length;

        /** Number of entries in record. */
        uint32_t count;

    public:

        explicit RecordPosition(uint64_t pos) {
            count = length = 0;
            position = pos;
        }

        RecordPosition(long pos, uint32_t len, uint32_t cnt) {
            position = pos;
            length = len;
            count = cnt;
        }

        RecordPosition setPosition(uint64_t _pos) {
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

        uint64_t getPosition() const { return position; }

        uint32_t getLength() const { return length; }

        uint32_t getCount() const { return count; }

        string toString() const {
            stringstream ss;
            ss << " POSITION = " << setw(16) << position << ", LENGTH = " << setw(12) << length << ", COUNT = " << setw(8) << count << endl;
            return ss.str();
        }
    };


    /** Use this to initialize Reader objects in constructor when no nodePool is needed but
     *  must be inititalized anyway. */
    static EvioNodeSource nodePoolStatic;
    /** Size of array in which to store record header info. */
    static const uint32_t headerInfoLen = 7;


    /**
     * Vector of records in the file. The vector is initialized
     * when the entire file is scanned to read out positions
     * of each record in the file (in constructor).
     */
    vector<RecordPosition> recordPositions;
    /** Object for reading file. */
    ifstream inStreamRandom;
    /** File name. */
    string fileName {""};
    /** File size in bytes. */
    uint64_t fileSize = 0;
    /** File header. */
    FileHeader fileHeader;
    /** Are we reading from file (true) or buffer? */
    bool fromFile = true;


    /** Buffer being read. */
    std::shared_ptr<ByteBuffer> buffer;
    /** Buffer used to temporarily hold data while decompressing. */
    std::shared_ptr<ByteBuffer> tempBuffer;
    //TODO: make this 64 bit ???
    /** Initial position of buffer. */
    uint32_t bufferOffset = 0;
    /** Limit of buffer. */
    uint32_t bufferLimit = 0;


    /** Keep one record for reading in data record-by-record. */
    RecordInput inputRecordStream;
    /** Number or position of last record to be read. */
    uint32_t currentRecordLoaded = 0;
    // TODO: Look at this
    /** First record's header. */
    std::shared_ptr<RecordHeader> firstRecordHeader;
    /** Record number expected when reading. Used to check sequence of records. */
    uint32_t recordNumberExpected = 1;
    /** If true, throw an exception if record numbers are out of sequence. */
    bool checkRecordNumberSequence = false;
    /** Object to handle event indexes in context of file and having to change records. */
    FileEventIndex eventIndex;


    /** Files may have an xml format dictionary in the user header of the file header. */
    string dictionaryXML {""};
    /** Each file of a set of split CODA files may have a "first" event common to all. */
    shared_ptr<uint8_t> firstEvent = nullptr;
    /** First event size in bytes. */
    uint32_t firstEventSize = 0;
    /** Stores info of all the (top-level) events in a scanned buffer. */
    vector<EvioNode> eventNodes;


    /** Is this object currently closed? */
    bool closed = false;
    /** Is this data in file/buffer compressed? */
    bool compressed = false;
    /** Byte order of file/buffer being read. */
    ByteOrder byteOrder = ByteOrder::ENDIAN_BIG;
    /** Keep track of next EvioNode when calling {@link #getNextEventNode()},
    * {@link #getEvent(int)}, or {@link #getPrevEvent()}. */
    int32_t sequentialIndex = -1;


    /** If true, the last sequential call was to getNextEvent or getNextEventNode.
     *  If false, the last sequential call was to getPrevEvent. Used to determine
     *  which event is prev or next. */
    bool lastCalledSeqNext = false;
    /** Evio version of file/buffer being read. */
    int evioVersion = 6;
    /** Source (pool) of EvioNode objects used for parsing Evio data in buffer (NOT file!). */
    EvioNodeSource & nodePool;


    /** Place to store data read in from record header. */
    uint32_t headerInfo[headerInfoLen]{};


private:

    void setByteOrder(ByteOrder & order);
    static int getTotalByteCounts(ByteBuffer & buf, uint32_t* info, uint32_t infoLen);
    static string getStringArray(ByteBuffer & buffer, int wrap, int max);
    static string getHexStringInt(int32_t value);

public:

    Reader();
    explicit Reader(string const & filename);
    Reader(string const & filename, bool forceScan);

    explicit Reader(std::shared_ptr<ByteBuffer> & buffer);
    Reader(std::shared_ptr<ByteBuffer> & buffer, EvioNodeSource & pool, bool checkRecordNumSeq = false);

    ~Reader() = default;

    void open(string const & filename);
    void close();

    bool isClosed() const;
    bool isFile() const;

    void setBuffer(std::shared_ptr<ByteBuffer> & buf);
    void setBuffer(std::shared_ptr<ByteBuffer> & buf, EvioNodeSource & pool);

    std::shared_ptr<ByteBuffer> & setCompressedBuffer(std::shared_ptr<ByteBuffer> & buf, EvioNodeSource & pool);

    string getFileName() const;
    long getFileSize() const;

    std::shared_ptr<ByteBuffer> & getBuffer();
    int getBufferOffset() const;

    FileHeader & getFileHeader();
    std::shared_ptr<RecordHeader> & getFirstRecordHeader();

    ByteOrder & getByteOrder();
    int getVersion() const;
    bool isCompressed() const;
    string getDictionary();
    bool hasDictionary() const;

    shared_ptr<uint8_t> & getFirstEvent();
    uint32_t getFirstEventSize();
    bool hasFirstEvent() const;

    uint32_t getEventCount() const;
    uint32_t getRecordCount() const;

    vector<RecordPosition> & getRecordPositions();
    vector<EvioNode> & getEventNodes();

    bool getCheckRecordNumberSequence() const;

    uint32_t getNumEventsRemaining() const;

    shared_ptr<uint8_t> getNextEvent(uint32_t * len);
    shared_ptr<uint8_t> getPrevEvent(uint32_t * len);

    EvioNode *getNextEventNode();

    ByteBuffer readUserHeader();

    shared_ptr<uint8_t> getEvent(uint32_t index, uint32_t * len);
    ByteBuffer & getEvent(ByteBuffer & buf, uint32_t index);
    uint32_t getEventLength(uint32_t index);
    EvioNode & getEventNode(uint32_t index);

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


    static void findRecordInfo(ByteBuffer & buf, uint32_t offset, uint32_t* info, uint32_t infoLen);


    ByteBuffer scanBuffer();
    void scanUncompressedBuffer();
    void forceScanFile();
    void scanFile(bool force);


    std::shared_ptr<ByteBuffer> & addStructure(uint32_t eventNumber, ByteBuffer & addBuffer);
    std::shared_ptr<ByteBuffer> & removeStructure(EvioNode & removeNode);

    void show() const;

    int main(int argc, char **argv);

};

}


#endif //EVIO_6_0_READER_H
