//
// Created by timmer on 7/18/19.
//

#ifndef EVIO_6_0_READER_H
#define EVIO_6_0_READER_H

#include <cstdint>

#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "FileHeader.h"
#include "RecordHeader.h"
#include "FileEventIndex.h"


class Reader {


private:

    /**
     * List of records in the file. The array is initialized
     * when the entire file is scanned to read out positions
     * of each record in the file (in constructor).
     */
    List<RecordPosition> recordPositions = new ArrayList<RecordPosition>();

    /** Fastest way to read/write files. */
    RandomAccessFile inStreamRandom;

    /** File being read. */
    string fileName;

    /** File size in bytes. */
    uint64_t fileSize;

    /** Buffer being read. */
    ByteBuffer buffer;

    /** Buffer used to temporarily hold data while decompressing. */
    ByteBuffer tempBuffer;

//TODO: make this 64 bit ???
    /** Initial position of buffer. */
    uint32_t bufferOffset;

    /** Limit of buffer. */
    uint32_t bufferLimit;

    /** Keep one record for reading in data record-by-record. */
    RecordInputStream inputRecordStream = new RecordInputStream();

    /** Number or position of last record to be read. */
    int currentRecordLoaded;

    /** File header. */
    FileHeader fileHeader;

    /** First record's header. */
    RecordHeader firstRecordHeader;

    /** Record number expected when reading. Used to check sequence of records. */
    int recordNumberExpected = 1;

    /** If true, throw an exception if record numbers are out of sequence. */
    bool checkRecordNumberSequence;

    /** Files may have an xml format dictionary in the user header of the file header. */
    string dictionaryXML;

    /** Each file of a set of split CODA files may have a "first" event common to all. */
    uint8_t*  firstEvent;

    /** Object to handle event indexes in context of file and having to change records. */
    FileEventIndex eventIndex = FileEventIndex();

    /** Are we reading from file (true) or buffer? */
    bool fromFile = true;

    /** Stores info of all the (top-level) events in a scanned buffer. */
    ArrayList<EvioNode> eventNodes = new ArrayList<>(1000);

    /** Is this object currently closed? */
    bool closed;

    /** Is this data in file/buffer compressed? */
    bool compressed;

    /** Byte order of file/buffer being read. */
    ByteOrder byteOrder = ByteOrder::ENDIAN_BIG;


    /** Keep track of next EvioNode when calling {@link #getNextEventNode()},
     * {@link #getEvent(int)}, or {@link #getPrevEvent()}. */
    int sequentialIndex = -1;

    /** If true, the last sequential call was to getNextEvent or getNextEventNode.
     *  If false, the last sequential call was to getPrevEvent. Used to determine
     *  which event is prev or next. */
    bool lastCalledSeqNext;

    /** Evio version of file/buffer being read. */
    int evioVersion;

    /** Source (pool) of EvioNode objects used for parsing Evio data in buffer. */
    EvioNodeSource nodePool;


    int lastRecordNum = 0;
    int *headerInfo = new int[7];


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

        RecordPosition(long pos) { position = pos; }

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
            return string.format(" POSITION = %16d, LENGTH = %12d, COUNT = %8d", position, length, count);
        }
    };


private:

    void setByteOrder(ByteOrder order);
    static int getTotalByteCounts(ByteBuffer buf, int[] info);

public:

    Reader() noexcept;
    Reader(string filename);
    Reader(string filename, bool forceScan);
    Reader(string filename, bool forceScan, bool checkRecordNumSeq);

    Reader(ByteBuffer buffer);
    Reader(ByteBuffer buffer, EvioNodeSource pool);
    Reader(ByteBuffer buffer, EvioNodeSource pool, bool checkRecordNumSeq);

    void open(string filename);
    void close();

    bool isClosed();
    bool isFile();

    void setBuffer(ByteBuffer buf);
    void setBuffer(ByteBuffer buf, EvioNodeSource pool);

    ByteBuffer setCompressedBuffer(ByteBuffer buf, EvioNodeSource pool);

    string getFileName();
    long getFileSize();

    ByteBuffer getBuffer();
    int getBufferOffset();

    FileHeader getFileHeader();
    RecordHeader getFirstRecordHeader();

    ByteOrder getByteOrder();
    int getVersion();
    bool isCompressed();
    string getDictionary();
    bool hasDictionary();

    uint8_t *getFirstEvent();
    bool hasFirstEvent();

    int getEventCount();
    int getRecordCount();

    List <RecordPosition> getRecordPositions();
    ArrayList <EvioNode> getEventNodes();

    bool getCheckRecordNumberSequence();

    int getNumEventsRemaining();

    uint8_t *getNextEvent();
    uint8_t *getPrevEvent();

    EvioNode getNextEventNode();

    ByteBuffer readUserHeader();

    uint8_t *getEvent(int index);
    ByteBuffer getEvent(ByteBuffer buf, int index);
    EvioNode getEventNode(int index);

    bool hasNext();
    bool hasPrev();

    int getRecordEventCount();

    int getCurrentRecord();

    RecordInputStream getCurrentRecordStream();

    bool readRecord(int index);


protected:

    void extractDictionaryAndFirstEvent();
    void extractDictionaryFromBuffer();
    void extractDictionaryFromFile();


    static void findRecordInfo(ByteBuffer buf, int offset, int[] info);


    ByteBuffer scanBuffer();
    void scanUncompressedBuffer();
    void forceScanFile();
    void scanFile(bool force);


    ByteBuffer removeStructure(EvioNode removeNode);
    ByteBuffer addStructure(int eventNumber, ByteBuffer addBuffer);

    void show();

    static int main(int argc, char **argv);

};

#endif //EVIO_6_0_READER_H
