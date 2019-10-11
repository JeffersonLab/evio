//
// Created by timmer on 7/18/19.
//

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

#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "FileHeader.h"
#include "RecordHeader.h"
#include "FileEventIndex.h"
#include "RecordInput.h"
#include "HipoException.h"
#include "EvioNode.h"
#include "EvioNodeSource.h"

using namespace std;



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
        long position;

        /** Length in bytes. */
        int length;

        /** Number of entries in record. */
        int count;

    public:

        RecordPosition(long pos) {
            count = length = 0;
            position = pos;
        }

        RecordPosition(long pos, int len, int cnt) {
            position = pos;
            length = len;
            count = cnt;
        }

        RecordPosition setPosition(long _pos) {
            position = _pos;
            return *this;
        }

        RecordPosition setLength(int _len) {
            length = _len;
            return *this;
        }

        RecordPosition setCount(int _cnt) {
            count = _cnt;
            return *this;
        }

        long getPosition() { return position; }

        int getLength() { return length; }

        int getCount() { return count; }

        string toString() {
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


    // Try to store members in memory somewhat efficiently


    /** File size in bytes. */
    uint64_t fileSize = 0;

    //TODO: make this 64 bit ???
    /** Initial position of buffer. */
    uint32_t bufferOffset = 0;

    /** Limit of buffer. */
    uint32_t bufferLimit = 0;

    /** Number or position of last record to be read. */
    uint32_t currentRecordLoaded = 0;

    /** Record number expected when reading. Used to check sequence of records. */
    uint32_t recordNumberExpected = 1;

    /** Keep track of next EvioNode when calling {@link #getNextEventNode()},
    * {@link #getEvent(int)}, or {@link #getPrevEvent()}. */
    int sequentialIndex = -1;

    /** Evio version of file/buffer being read. */
    int evioVersion = 6;

    uint32_t lastRecordNum = 0;

    /** Place to store data read in from record header. */
    uint32_t *headerInfo = new uint32_t[headerInfoLen];

    /** Each file of a set of split CODA files may have a "first" event common to all. */
    shared_ptr<uint8_t> firstEvent = nullptr;

    /**
     * List of records in the file. The list is initialized
     * when the entire file is scanned to read out positions
     * of each record in the file (in constructor).
     */
    vector<RecordPosition> recordPositions = vector<RecordPosition>();

    /** Stores info of all the (top-level) events in a scanned buffer. */
    vector<EvioNode> eventNodes = vector<EvioNode>();

    /** Object for reading file. */
    ifstream inStreamRandom;

    /** File name. */
    string fileName;

    /** Buffer being read. */
    ByteBuffer buffer;

    /** Buffer used to temporarily hold data while decompressing. */
    ByteBuffer tempBuffer;

    /** Keep one record for reading in data record-by-record. */
    RecordInput inputRecordStream = RecordInput();

    /** File header. */
    FileHeader fileHeader;

// TODO: Look at this
    /** First record's header. */
    RecordHeader firstRecordHeader;

    /** Files may have an xml format dictionary in the user header of the file header. */
    string dictionaryXML;

    /** Object to handle event indexes in context of file and having to change records. */
    FileEventIndex eventIndex = FileEventIndex();

    /** Source (pool) of EvioNode objects used for parsing Evio data in buffer (NOT file!). */
    EvioNodeSource & nodePool;

    /** Byte order of file/buffer being read. */
    ByteOrder byteOrder = ByteOrder::ENDIAN_BIG;

    /** If true, throw an exception if record numbers are out of sequence. */
    bool checkRecordNumberSequence = false;

    /** Are we reading from file (true) or buffer? */
    bool fromFile = true;

    /** Is this object currently closed? */
    bool closed = false;

    /** Is this data in file/buffer compressed? */
    bool compressed = false;

    /** If true, the last sequential call was to getNextEvent or getNextEventNode.
     *  If false, the last sequential call was to getPrevEvent. Used to determine
     *  which event is prev or next. */
    bool lastCalledSeqNext = true;


private:

    void setByteOrder(ByteOrder & order);
    static int getTotalByteCounts(ByteBuffer & buf, uint32_t* info, uint32_t infoLen);
    static void toIntArray(char const *data, uint32_t dataOffset,  uint32_t dataLen,
                           const ByteOrder & byteOrder, int *dest, uint32_t destOffset);
    static int toInt(char b1, char b2, char b3, char b4, const ByteOrder & byteOrder);
    static string getStringArray(ByteBuffer & buffer, int wrap, int max);
    static string getHexStringInt(int32_t value);

public:

    Reader();
    explicit Reader(string & filename);
    Reader(string & filename, bool forceScan);
    Reader(string & filename, bool forceScan, bool checkRecordNumSeq);

    explicit Reader(ByteBuffer & buffer);
    Reader(ByteBuffer & buffer, EvioNodeSource & pool);
    Reader(ByteBuffer & buffer, EvioNodeSource & pool, bool checkRecordNumSeq);

    ~Reader() {
        //delete(inStreamRandom);
    }

    void open(string & filename);
    void close();

    bool isClosed();
    bool isFile();

    void setBuffer(ByteBuffer & buf);
    void setBuffer(ByteBuffer & buf, EvioNodeSource & pool);

    ByteBuffer setCompressedBuffer(ByteBuffer & buf, EvioNodeSource & pool);

    string getFileName();
    long getFileSize();

    ByteBuffer & getBuffer();
    int getBufferOffset();

    FileHeader & getFileHeader();
    RecordHeader & getFirstRecordHeader();

    ByteOrder getByteOrder();
    int getVersion();
    bool isCompressed();
    string getDictionary();
    bool hasDictionary();

    shared_ptr<uint8_t> getFirstEvent();
    bool hasFirstEvent();

    uint32_t getEventCount();
    uint32_t getRecordCount();

    vector<RecordPosition> & getRecordPositions();
    vector<EvioNode> & getEventNodes();

    bool getCheckRecordNumberSequence();

    uint32_t getNumEventsRemaining();

    shared_ptr<uint8_t>getNextEvent();
    shared_ptr<uint8_t>getPrevEvent();

    EvioNode *getNextEventNode();

    ByteBuffer readUserHeader();

    shared_ptr<uint8_t> getEvent(int index);
    ByteBuffer *getEvent(ByteBuffer buf, int index);
    uint32_t getEventLength(uint32_t index);
    EvioNode *getEventNode(uint32_t index);

    bool hasNext();
    bool hasPrev();

    uint32_t getRecordEventCount();

    uint32_t getCurrentRecord();

    RecordInput getCurrentRecordStream();

    bool readRecord(int index);


protected:

    void extractDictionaryAndFirstEvent();
    void extractDictionaryFromBuffer();
    void extractDictionaryFromFile();


    static void findRecordInfo(ByteBuffer & buf, uint32_t offset, uint32_t* info, uint32_t infoLen);


    ByteBuffer scanBuffer();
    void scanUncompressedBuffer();
    void forceScanFile();
    void scanFile(bool force);


    ByteBuffer removeStructure(EvioNode removeNode);
    ByteBuffer addStructure(uint32_t eventNumber, ByteBuffer & addBuffer);
    ByteBuffer removeStructure(EvioNode & removeNode);

    void show();

    int main(int argc, char **argv);

};


#endif //EVIO_6_0_READER_H
