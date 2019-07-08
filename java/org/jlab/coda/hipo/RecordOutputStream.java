/*
 *   Copyright (c) 2016.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */
package org.jlab.coda.hipo;

import org.jlab.coda.jevio.EvioBank;
import org.jlab.coda.jevio.EvioNode;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Class which handles the creation and use of Evio & HIPO Records.<p>
 *     
 * <pre>
 * RECORD STRUCTURE:
 *
 *               Uncompressed                                      Compressed
 *
 *    +----------------------------------+            +----------------------------------+
 *    |       General Record Header      |            |       General Record Header      |
 *    +----------------------------------+            +----------------------------------+
 *
 *    +----------------------------------+ ---------> +----------------------------------+
 *    |           Index Array            |            |        Compressed Data           |
 *    +----------------------------------+            |             Record               |
 *                                                    |                                  |
 *    +----------------------------------+            |                                  |
 *    |           User Header            |            |                  ----------------|
 *    |           (Optional)             |            |                  |    Pad 3      |
 *    |                  ----------------|            +----------------------------------+
 *    |                  |    Pad 1      |           ^
 *    +----------------------------------+          /
 *                                                 /
 *    +----------------------------------+       /
 *    |           Data Record            |     /
 *    |                                  |    /
 *    |                  ----------------|   /
 *    |                  |    Pad 2      | /
 *    +----------------------------------+
 *
 *
 *
 *
 * GENERAL RECORD HEADER STRUCTURE ( see RecordHeader.java )
 *
 *    +----------------------------------+
 *  1 |         Record Length            | // 32bit words, inclusive
 *    +----------------------------------+
 *  2 +         Record Number            |
 *    +----------------------------------+
 *  3 +         Header Length            | // 14 (words)
 *    +----------------------------------+
 *  4 +       Event (Index) Count        |
 *    +----------------------------------+
 *  5 +      Index Array Length          | // bytes
 *    +-----------------------+---------+
 *  6 +       Bit Info        | Version  | // version (8 bits)
 *    +-----------------------+----------+
 *  7 +      User Header Length          | // bytes
 *    +----------------------------------+
 *  8 +          Magic Number            | // 0xc0da0100
 *    +----------------------------------+
 *  9 +     Uncompressed Data Length     | // bytes
 *    +------+---------------------------+
 * 10 +  CT  |  Data Length Compressed   | // CT = compression type (4 bits)
 *    +----------------------------------+
 * 11 +        General Register 1        | // UID 1st (64 bits)
 *    +--                              --+
 * 12 +                                  |
 *    +----------------------------------+
 * 13 +        General Register 2        | // UID 2nd (64 bits)
 *    +--                              --+
 * 14 +                                  |
 *    +----------------------------------+
 * </pre>
 *
 * @version 6.0
 * @since 6.0 9/6/17
 * @author gavalian
 * @author timmer
 */
public class RecordOutputStream {
    
    /** Maximum number of events per record. */
    private static final int ONE_MEG = 1024*1024;


    /** Maximum number of events per record. */
    private int MAX_EVENT_COUNT = 1000000;

    /**
     * Size of some internal buffers in bytes. If the recordBinary buffer is passed
     * into the constructor or given through {@link #setBuffer(ByteBuffer)}, then this
     * value is 91% of the its size (from position to capacity). This allows some
     * margin for compressed data to be larger than the uncompressed - which may
     * happen if data is random. It also allows other records to have been previously
     * stored in the given buffer (eg. common record) since it starts writing at the
     * buffer position which may not be 0.
     */
    private int MAX_BUFFER_SIZE = 8*ONE_MEG;

    /**
     * Size of buffer holding built record in bytes. If the recordBinary buffer is passed
     * into the constructor or given through {@link #setBuffer(ByteBuffer)}, then this
     * value is set to be 10% bigger than {@link #MAX_BUFFER_SIZE}. This allows some
     * margin for compressed data to be larger than the uncompressed - which may
     * happen if data is random.
     */
    private int RECORD_BUFFER_SIZE = 9*ONE_MEG;

    /** This buffer stores event lengths ONLY. */
    private ByteBuffer recordIndex;
    
    /** This buffer stores event data ONLY. */
    private ByteBuffer recordEvents;
    
    /** This buffer stores data that will be compressed. */
    private ByteBuffer recordData;

    /** Buffer in which to put constructed (& compressed) binary record.
     * Code is written so that it works whether or not it's backed by an array. */
    private ByteBuffer recordBinary;

    /** The number of initially available bytes to be written into in the user-given buffer,
     * that go from position to limit. The user-given buffer is stored in recordBinary.
     */
    private int userBufferSize;

    /** Is recordBinary a user provided buffer? */
    private boolean userProvidedBuffer;
    
    /** Compression information & type. */
    private Compressor dataCompressor;

    /** Header of this Record. */
    private RecordHeader header;

    /** Number of events written to this Record. */
    private int eventCount;

    /** Number of valid bytes in recordIndex buffer */
    private int indexSize;
    
    /** Number of valid bytes in recordEvents buffer. */
    private int eventSize;

    /** The starting position of a user-given buffer.
     * No data will be written before this position. */
    private int startingPosition;

    /** Byte order of record byte arrays to build. */
    private ByteOrder byteOrder;

    
    /** Default, no-arg constructor. Little endian. LZ4 compression. */
    public RecordOutputStream() {
        dataCompressor = Compressor.getInstance();
        header = new RecordHeader();
        header.setCompressionType(Compressor.RECORD_COMPRESSION_LZ4);
        byteOrder = ByteOrder.LITTLE_ENDIAN;
        allocate();
    }

    /**
     * Constructor with arguments.
     * Record header type defaults to HIPO record.
     * @param order byte order of built record byte arrays.
     * @param maxEventCount max number of events this record can hold.
     *                      Value <= O means use default (1M).
     * @param maxBufferSize max number of uncompressed data bytes this record can hold.
     *                      Value of < 8MB results in default of 8MB.
     * @param compressionType type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip).
     */
    public RecordOutputStream(ByteOrder order, int maxEventCount, int maxBufferSize,
                              int compressionType) {
        this(order, maxEventCount, maxBufferSize, compressionType, null);
    }

    /**
     * Constructor with arguments.
     * @param order byte order of built record byte arrays.
     * @param maxEventCount max number of events this record can hold.
     *                      Value <= O means use default (1M).
     * @param maxBufferSize max number of uncompressed data bytes this record can hold.
     *                      Value of < 8MB results in default of 8MB.
     * @param compressionType type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip).
     * @param hType           type of record header to use.
     */
    public RecordOutputStream(ByteOrder order, int maxEventCount, int maxBufferSize,
                              int compressionType, HeaderType hType) {
        dataCompressor = Compressor.getInstance();
        try {
            if (hType != null) {
                if      (hType.isEvioFileHeader()) hType = HeaderType.EVIO_RECORD;
                else if (hType.isHipoFileHeader()) hType = HeaderType.HIPO_RECORD;
            }
            header = new RecordHeader(hType);
        }
        catch (HipoException e) {/* never happen */}
        header.setCompressionType(compressionType);
        byteOrder = order;

        if (maxEventCount > 0) {
            MAX_EVENT_COUNT = maxEventCount;
        }

        if (maxBufferSize > MAX_BUFFER_SIZE) {
            MAX_BUFFER_SIZE = maxBufferSize;
            // If it's at least 10% bigger we'll be OK.
            // Done so that if we "compress" random data,
            // we have a place to put its larger size.
            RECORD_BUFFER_SIZE = (int) (1.1 * MAX_BUFFER_SIZE);
        }

        allocate();
    }

    /**
     * Constructor with arguments.
     * @param buffer buffer in which to put constructed (& compressed) binary record.
     *               Must have position and limit set to accept new data.
     * @param maxEventCount max number of events this record can hold.
     *                      Value <= O means use default (1M).
     * @param compressionType type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip).
     * @param hType           type of record header to use.
     */
    public RecordOutputStream(ByteBuffer buffer, int maxEventCount,
                              int compressionType, HeaderType hType) {

        dataCompressor = Compressor.getInstance();
        try {
            if (hType != null) {
                if      (hType.isEvioFileHeader()) hType = HeaderType.EVIO_RECORD;
                else if (hType.isHipoFileHeader()) hType = HeaderType.HIPO_RECORD;
            }
            header = new RecordHeader(hType);
        }
        catch (HipoException e) {/* never happen */}
        header.setCompressionType(compressionType);

        userProvidedBuffer = true;
        recordBinary = buffer;
        byteOrder = buffer.order();

        // Start writing at startingPosition, but allow writing
        // to extend to the full buffer capacity and not be limited
        // to only the limit. This is done as we may be adding records
        // to already existing records, but we most likely will not be
        // trying to fit a record in between 2 existing records.
        startingPosition = buffer.position();
        userBufferSize = buffer.capacity() - startingPosition;
        buffer.limit(buffer.capacity());

        if (maxEventCount > 0) {
            MAX_EVENT_COUNT = maxEventCount;
        }

        // Set things so user buffer is 10% bigger
        MAX_BUFFER_SIZE = (int) (0.91*userBufferSize);
        // Not used with user provided buffer,
        // but change it anyway for use in copy(RecordOutputStream)
        RECORD_BUFFER_SIZE = userBufferSize;

        allocate();
    }

    /**
     * Reset internal buffers and set the buffer in which to build this record.
     * The given buffer should be made ready to receive new data by setting its
     * position and limit properly. Its byte order is set to the same as this writer's.
     * The argument ByteBuffer can be retrieved by calling {@link #getBinaryBuffer()}.
     * @param buf buffer in which to build record.
     */
    public void setBuffer(ByteBuffer buf) {
        if (buf.order() != byteOrder) {
            System.out.println("setBuffer(): warning, changing buffer's byte order!");
        }

        recordBinary = buf;
        recordBinary.order(byteOrder);
        userProvidedBuffer = true;

        startingPosition = buf.position();
        userBufferSize = buf.capacity() - startingPosition;
        buf.limit(buf.capacity());

        // Only allocate memory if current buffers are too small
        if (userBufferSize > RECORD_BUFFER_SIZE) {
            MAX_BUFFER_SIZE = (int) (0.91*userBufferSize);
            RECORD_BUFFER_SIZE = userBufferSize;
            //System.out.println("setBuffer: changed MAX_BUFFER_SIZE to " + MAX_BUFFER_SIZE + ", reallocate");
            allocate();
        }
        else {
            MAX_BUFFER_SIZE = (int) (0.91*userBufferSize);
            RECORD_BUFFER_SIZE = userBufferSize;
            //System.out.println("setBuffer: changed MAX_BUFFER_SIZE to " + MAX_BUFFER_SIZE + ", did NOT reallocate");
        }

        reset();
    }

    // TODO: This method needs some serious attention!
    /**
     * Copy the contents of the arg into this object.
     * If the arg has more data than will fit, increase buffer sizes.
     * If the arg has more events than our allowed max, increase the max.
     * @param rec object to copy
     * @throws HipoException if we cannot replace internal buffer if it needs to be
     *                       expanded since it was provided by the user.
     */
    public void copy(RecordOutputStream rec) throws HipoException {
        if (rec == null) return;

        // Copy primitive types & immutable objects
        eventCount = rec.eventCount;
        indexSize  = rec.indexSize;
        eventSize  = rec.eventSize;
        byteOrder  = rec.byteOrder;

        // Copy header
        header.copy(rec.header);

        // It would be nice to leave MAX_EVENT_COUNT as is so RecordSupply
        // has consistent behavior. But I don't think that's possible if
        // the record output stream being copied is larger. Since the record
        // being copied may not have had build() called, go by the max sizes
        // and not how much data are in the buffers.

        // Choose the larger of rec's or this object's buffer sizes
        if (rec.MAX_BUFFER_SIZE > MAX_BUFFER_SIZE ||
            rec.RECORD_BUFFER_SIZE > RECORD_BUFFER_SIZE) {

            MAX_BUFFER_SIZE = rec.MAX_BUFFER_SIZE;
            RECORD_BUFFER_SIZE = rec.RECORD_BUFFER_SIZE;

            // Reallocate memory
            if (!userProvidedBuffer) {
                recordBinary = ByteBuffer.wrap(new byte[RECORD_BUFFER_SIZE]);
                recordBinary.order(byteOrder);
            }
            else {
                throw new HipoException("cannot replace internal buffer since it's provided by user");
            }

            recordEvents = ByteBuffer.wrap(new byte[MAX_BUFFER_SIZE]);
            recordEvents.order(byteOrder);

            recordData = ByteBuffer.wrap(new byte[MAX_BUFFER_SIZE]);
            recordData.order(byteOrder);
        }

        if (rec.eventCount > MAX_EVENT_COUNT) {
            MAX_EVENT_COUNT = rec.eventCount;
            recordIndex = ByteBuffer.wrap(new byte[MAX_EVENT_COUNT*4]);
            recordIndex.order(byteOrder);
        }

        // Copy data over
        System.arraycopy(rec.recordIndex.array(),  0, recordIndex.array(),  0, indexSize);
        System.arraycopy(rec.recordEvents.array(), 0, recordEvents.array(), 0, eventSize);

        // recordData is just a temporary holding buffer and does NOT need to be copied

        // recordBinary holds built record and may or may NOT have a backing array.
        // Assume that buffer is ready to read as is the case right after build() is called.
        if (rec.recordBinary.hasArray() && recordBinary.hasArray()) {
            System.arraycopy(rec.recordBinary.array(), 0,
                             recordBinary.array(), 0, rec.recordBinary.limit());
            // Get buffer ready to read
            recordBinary.position(0).limit(rec.recordBinary.limit());
        }
        else {
            recordBinary.position(0);
            recordBinary.put(rec.recordBinary);
            // Get buffers ready to read
            recordBinary.flip();
            rec.recordBinary.flip();
        }
    }


    /**
     * Get the number of initially available bytes to be written into in the user-given buffer,
     * that goes from position to limit. The user-given buffer is referenced by recordBinary.
     * @return umber of initially available bytes to be written into in the user-given buffer.
     */
    public int getUserBufferSize() {return userBufferSize;}

    /**
     * Get the current uncompressed size of the record in bytes.
     * This does NOT count any user header.
     * @return current uncompressed size of the record in bytes.
     */
    public int getUncompressedSize() {
        return eventSize + indexSize + RecordHeader.HEADER_SIZE_BYTES;
    }

    /**
     * Get the capacity of the internal buffer in bytes.
     * This is the upper limit of the memory needed to store this
     * uncompressed record.
     * @return capacity of the internal buffer in bytes.
     */
    public int getInternalBufferCapacity() {return MAX_BUFFER_SIZE;}

    /**
     * Get the general header of this record.
     * @return general header of this record.
     */
    public RecordHeader getHeader() {return header;}

    /**
     * Get the number of events written so far into the buffer
     * @return number of events written so far into the buffer
     */
    public int getEventCount() {return eventCount;}

    /**
     * Get the internal ByteBuffer used to construct binary representation of this record.
     * @return internal ByteBuffer used to construct binary representation of this record.
     */
    public ByteBuffer getBinaryBuffer() {return recordBinary;}

    /**
     * Was the internal buffer provided by the user?
     * @return true if internal buffer provided by user.
     */
    public boolean hasUserProvidedBuffer() {return userProvidedBuffer;}

    /**
     * Get the byte order of the record to be built.
     * @return byte order of the record to be built.
     */
    public ByteOrder getByteOrder() {return byteOrder;}

    /**
     * Set the byte order of the record to be built.
     * Only use this internally to the package.
     * @param order byte order of the record to be built.
     */
    void setByteOrder(ByteOrder order) {byteOrder = order;}

    /** Allocates all buffers for constructing the record stream. */
    private void allocate() {

        if (recordIndex == null) {
            // This only needs to be done once
            recordIndex = ByteBuffer.wrap(new byte[MAX_EVENT_COUNT * 4]);
            recordIndex.order(byteOrder);
        }

        recordEvents = ByteBuffer.wrap(new byte[MAX_BUFFER_SIZE]);
        recordEvents.order(byteOrder);

        // Making this a direct buffer slows it down by 6%
        recordData = ByteBuffer.wrap(new byte[MAX_BUFFER_SIZE]);
        recordData.order(byteOrder);

        if (!userProvidedBuffer) {
            // Trying to compress random data will expand it, so create a cushion.
            // Using a direct buffer takes 2/3 the time of an array-backed buffer
            recordBinary = ByteBuffer.wrap(new byte[RECORD_BUFFER_SIZE]);
            recordBinary.order(byteOrder);
        }
    }

    /**
     * Is there room in this record's memory for an additional event
     * of the given length in bytes (length NOT including accompanying index).
     * @param length length of data to add in bytes
     * @return {@code true} if room in record, else {@code false}.
     */
    public boolean roomForEvent(int length) {
        // Account for this record's header including index
        return ((indexSize + 4 + eventSize + RecordHeader.HEADER_SIZE_BYTES + length) <= MAX_BUFFER_SIZE);
    }

    /**
     * Does adding one more event exceed the event count limit?
     * @return {@code true} if one more event exceeds count limit, else {@code false}.
     */
    public boolean oneTooMany() {return (eventCount + 1 > MAX_EVENT_COUNT);}

    /**
     * Is another event of the given length allowed into this record's memory?
     * It may not be allowed if its exceeds the memory or count limit.
     * @param length length of event to add in bytes
     * @return {@code true} if allowed into record, else {@code false}.
     */
    private boolean allowedIntoRecord(int length) {
        return (eventCount < 1 && !roomForEvent(length));
    }

    /**
     * Adds an event's ByteBuffer into the record.
     * If a single event is too large for the internal buffers,
     * more memory is allocated.
     * On the other hand, if the buffer was provided by the user,
     * then obviously the buffer cannot be expanded and false is returned.<p>
     * <b>The byte order of event must match the byte order given in constructor!</b>
     *
     *
     * @param event  event's byte array.
     * @return true if event was added; false if the event was not added because the
     *         count limit would be exceeded or the buffer is full and cannot be
     *         expanded since it's user-provided.
     */
    public boolean addEvent(byte[] event) {
        return addEvent(event, 0, event.length, 0);
    }

    /**
     * Adds an event's ByteBuffer into the record.
     * If a single event is too large for the internal buffers,
     * more memory is allocated.
     * On the other hand, if the buffer was provided by the user,
     * then obviously the buffer cannot be expanded and false is returned.<p>
     * <b>The byte order of event must match the byte order given in constructor!</b>
     *
     *
     * @param event     event's byte array.
     * @param position  offset into event byte array from which to begin reading.
     * @param eventLen  number of bytes from byte array to add.
     * @return true if event was added; false if the event was not added because the
     *         count limit would be exceeded or the buffer is full and cannot be
     *         expanded since it's user-provided.
     */
    public boolean addEvent(byte[] event, int position, int eventLen) {
        return addEvent(event, position, eventLen, 0);
    }

    /**
     * Adds an event's ByteBuffer into the record.
     * Can specify the length of additional data to follow the event
     * (such as an evio trailer record) to see if by adding this event
     * everything will fit in the available memory.<p>
     * If a single event is too large for the internal buffers,
     * more memory is allocated.
     * On the other hand, if the buffer was provided by the user,
     * then obviously the buffer cannot be expanded and false is returned.<p>
     * <b>The byte order of event must match the byte order given in constructor!</b>
     *
     *
     * @param event        event's byte array.
     * @param position     offset into event byte array from which to begin reading.
     * @param eventLen     number of bytes from byte array to add.
     * @param extraDataLen additional data bytes to follow event (e.g. trailer length).
     * @return true if event was added; false if the event was not added because the
     *         count limit would be exceeded or the buffer is full and cannot be
     *         expanded since it's user-provided.
     */
    public boolean addEvent(byte[] event, int position, int eventLen, int extraDataLen) {

        // If we receive a single event larger than our memory, we must accommodate this
        // by increasing our internal buffer size(s). Cannot simply refuse to write an
        // event during event building for example.
        if (eventCount < 1 && !roomForEvent(eventLen + extraDataLen)) {
            if (userProvidedBuffer) {
                return false;
            }

            // Allocate roughly what we need + 1MB
            MAX_BUFFER_SIZE = eventLen + ONE_MEG;
            RECORD_BUFFER_SIZE = MAX_BUFFER_SIZE + ONE_MEG;
            allocate();
            // This does NOT reset record type, compression type, or byte order
            reset();
        }

        if (oneTooMany() || !roomForEvent(eventLen)) {
            return false;
        }

        // Where do we start writing in buffer?
        int pos = recordEvents.position();
        // Add event data
        System.arraycopy(event, position, recordEvents.array(), pos, eventLen);
        // Make sure we write in the correct position next time
        recordEvents.position(pos + eventLen);
        // Same as above, but above method is a lot faster:
        //recordEvents.put(event, position, length);
        eventSize += eventLen;

        // Add 1 more index
        recordIndex.putInt(indexSize, eventLen);
        indexSize += 4;

        eventCount++;

        return true;
    }

    /**
     * Adds an event's ByteBuffer into the record.
     * If a single event is too large for the internal buffers,
     * more memory is allocated.
     * On the other hand, if the buffer was provided by the user,
     * then obviously the buffer cannot be expanded and false is returned.<p>
     * <b>The byte order of event must match the byte order given in constructor!</b>
     *
     * @param event  event's ByteBuffer object.
     * @return true if event was added; false if the buffer is full,
     *         the event count limit is exceeded, or this single event cannot fit into
     *         the user-provided buffer.
     */
    public boolean addEvent(ByteBuffer event) {
        return addEvent(event, 0);
    }

    /**
     * Adds an event's ByteBuffer into the record.
     * Can specify the length of additional data to follow the event
     * (such as an evio trailer record) to see if by adding this event
     * everything will fit in the available memory.<p>
     * If a single event is too large for the internal buffers,
     * more memory is allocated.
     * On the other hand, if the buffer was provided by the user,
     * then obviously the buffer cannot be expanded and false is returned.<p>
     * <b>The byte order of event must match the byte order given in constructor!</b>
     *
     * @param event        event's ByteBuffer object.
     * @param extraDataLen additional data bytes to follow event (e.g. trailer length).
     * @return true if event was added; false if the event was not added because the
     *         count limit would be exceeded or the buffer is full and cannot be
     *         expanded since it's user-provided.
     */
    public boolean addEvent(ByteBuffer event, int extraDataLen) {

        int eventLen = event.remaining();

        if (eventCount < 1 && !roomForEvent(eventLen + extraDataLen)) {
            if (userProvidedBuffer) {
                return false;
            }

            MAX_BUFFER_SIZE = eventLen + ONE_MEG;
            RECORD_BUFFER_SIZE = MAX_BUFFER_SIZE + ONE_MEG;
            allocate();
            reset();
        }

        if (oneTooMany() || !roomForEvent(eventLen)) {
            return false;
        }
//        int curSize =  (indexSize + 4 + eventSize + RecordHeader.HEADER_SIZE_BYTES +
//                                                eventLen + extraDataLen);
//        int diff = recordData.limit() - curSize;
//
//        if (diff < 5000) {
//            System.out.println("We got " + curSize + " bytes in recordEvents, lim " +
//                                       recordEvents.limit() + ", cap = " + recordEvents.capacity() +
//                                       ", diff = " + diff + ", max buff size = " + MAX_BUFFER_SIZE);
//        }

        int pos=0;
        if (event.hasArray()) {
//            try {
                // recordEvents backing array's offset = 0
                pos = recordEvents.position();
                System.arraycopy(event.array(),
                                 event.arrayOffset() + event.position(),
                                 recordEvents.array(), pos, eventLen);
                recordEvents.position(pos + eventLen);
//            }
//            catch (Exception e) {
//                System.out.println("addEvent, pos in recordEvent = " + pos + ", lim = " + recordEvents.limit() +
//                                           ", next pos = " + (pos + eventLen));
//
//                e.printStackTrace();
//            }
        }
        else {
            recordEvents.put(event);
        }

        eventSize += eventLen;
        recordIndex.putInt(indexSize, eventLen);
        indexSize += 4;
        eventCount++;

        return true;
    }

    /**
     * Adds an event's ByteBuffer into the record.
     * If a single event is too large for the internal buffers,
     * more memory is allocated.
     * On the other hand, if the buffer was provided by the user,
     * then obviously the buffer cannot be expanded and false is returned.<p>
     * <b>The byte order of event must match the byte order given in constructor!</b>
     * This method is not thread-safe with respect to the node as it's backing
     * ByteBuffer's limit and position may be concurrently changed.
     *
     * @param node         event's EvioNode object
     * @return true if event was added; false if the event was not added because the
     *         count limit would be exceeded or the buffer is full and cannot be
     *         expanded since it's user-provided.
     * @throws HipoException if node does not correspond to a bank.
     */
    public boolean addEvent(EvioNode node) throws HipoException {
        return addEvent(node, 0);
    }

    /**
     * Adds an event's ByteBuffer into the record.
     * Can specify the length of additional data to follow the event
     * (such as an evio trailer record) to see if by adding this event
     * everything will fit in the available memory.<p>
     * If a single event is too large for the internal buffers,
     * more memory is allocated.
     * On the other hand, if the buffer was provided by the user,
     * then obviously the buffer cannot be expanded and false is returned.<p>
     * <b>The byte order of event must match the byte order given in constructor!</b>
     * This method is not thread-safe with respect to the node as it's backing
     * ByteBuffer's limit and position may be concurrently changed.
     *
     * @param node         event's EvioNode object
     * @param extraDataLen additional data bytes to follow event (e.g. trailer length).
     * @return true if event was added; false if the event was not added because the
     *         count limit would be exceeded or the buffer is full and cannot be
     *         expanded since it's user-provided.
     * @throws HipoException if node does not correspond to a bank.
     */
    public boolean addEvent(EvioNode node, int extraDataLen) throws HipoException {

        int eventLen = node.getTotalBytes();

        if (!node.getTypeObj().isBank()) {
            throw new HipoException("node does not represent a bank");
        }

        if (eventCount < 1 && !roomForEvent(eventLen + extraDataLen)) {
            if (userProvidedBuffer) {
                return false;
            }

            MAX_BUFFER_SIZE = eventLen + ONE_MEG;
            RECORD_BUFFER_SIZE = MAX_BUFFER_SIZE + ONE_MEG;
            allocate();
            reset();
        }

        if (oneTooMany() || !roomForEvent(eventLen)) {
            return false;
        }

        ByteBuffer buf = node.getStructureBuffer(false);
        if (buf.hasArray()) {
            int pos = recordEvents.position();
            System.arraycopy(buf.array(),
                             buf.arrayOffset() + buf.position(),
                             recordEvents.array(), pos, eventLen);
            recordEvents.position(pos + eventLen);
        }
        else {
            recordEvents.put(buf);
        }

        eventSize += eventLen;
        recordIndex.putInt(indexSize, eventLen);
        indexSize += 4;
        eventCount++;

        return true;
    }

    /**
     * Adds an event's ByteBuffer into the record.
     * If a single event is too large for the internal buffers,
     * more memory is allocated.
     * On the other hand, if the buffer was provided by the user,
     * then obviously the buffer cannot be expanded and false is returned.<p>
     * <b>The byte order of event must match the byte order given in constructor!</b>
     *
     * @param event event's EvioBank object.
     * @return true if event was added; false if the event was not added because the
     *         count limit would be exceeded or the buffer is full and cannot be
     *         expanded since it's user-provided.
     */
    public boolean addEvent(EvioBank event) {
        return addEvent(event, 0);
    }

    /**
     * Adds an event's ByteBuffer into the record.
     * Can specify the length of additional data to follow the event
     * (such as an evio trailer record) to see if by adding this event
     * everything will fit in the available memory.<p>
     * If a single event is too large for the internal buffers,
     * more memory is allocated.
     * On the other hand, if the buffer was provided by the user,
     * then obviously the buffer cannot be expanded and false is returned.<p>
     * <b>The byte order of event must match the byte order given in constructor!</b>
     *
     * @param event        event's EvioBank object.
     * @param extraDataLen additional data bytes to follow event (e.g. trailer length).
     * @return true if event was added; false if the event was not added because the
     *         count limit would be exceeded or the buffer is full and cannot be
     *         expanded since it's user-provided.
     */
    public boolean addEvent(EvioBank event, int extraDataLen) {

        int eventLen = event.getTotalBytes();

        if (eventCount < 1 && !roomForEvent(eventLen + extraDataLen)) {
            if (userProvidedBuffer) {
                return false;
            }

            MAX_BUFFER_SIZE = eventLen + ONE_MEG;
            RECORD_BUFFER_SIZE = MAX_BUFFER_SIZE + ONE_MEG;
            allocate();
            reset();
        }

        if (oneTooMany() || !roomForEvent(eventLen)) {
            return false;
        }

        event.write(recordEvents);
        
        eventSize += eventLen;
        recordIndex.putInt(indexSize, eventLen);
        indexSize += 4;
        eventCount++;

        return true;
    }

    /**
     * Reset internal buffers. The buffer is ready to receive new data.
     * Also resets the header including removing any compression.
     */
    public void reset() {
        indexSize  = 0;
        eventSize  = 0;
        eventCount = 0;

        recordData.clear();
        recordIndex.clear();
        recordEvents.clear();
        recordBinary.clear();

        // TODO: This may do way too much! Think about this more.
        header.reset();
    }

    /**
     * Set the starting position of the user-given buffer being written into.
     * Calling this may be necessary from EventWriter(Unsync) when a common record
     * (dictionary + first event) is written after the constructor for this
     * object has been called, but before any events have been written.
     * This method should <b>not</b> be called in general as it will MESS UP
     * THE WRITING OF DATA!
     * @param pos position in buffer to start writing.
     */
    public void setStartingBufferPosition(int pos) {
        recordBinary.position(pos);
        startingPosition = pos;
    }

    /**
     * Builds the record. Compresses data, header is constructed,
     * then header & data written into internal buffer.
     * This method may be called multiple times in succession without
     * any problem. The buffer obtained with {@link #getBinaryBuffer()}
     * has its position and limit set for reading.
     */
    public void build() {

        // If no events have been added yet, just write a header
        if (eventCount < 1) {
            header.setEntries(0);
            header.setDataLength(0);
            header.setIndexLength(0);
            header.setCompressedDataLength(0);
            header.setLength(RecordHeader.HEADER_SIZE_BYTES);
            recordBinary.position(startingPosition);
            try {
                header.writeHeader(recordBinary);
            }
            catch (HipoException e) {/* never happen */}
//            System.out.println("build: buf lim = " + recordBinary.limit() +
//                    ", cap = " + recordBinary.capacity());
            recordBinary.limit(RecordHeader.HEADER_SIZE_BYTES);
            return;
        }

        int compressionType = header.getCompressionType();

        // Does the recordBinary buffer have an array backing?
        boolean recBinHasArray = recordBinary.hasArray();

        // Position in recordBinary buffer of just past the record header
        int recBinPastHdr = startingPosition + RecordHeader.HEADER_SIZE_BYTES;

        // Position in recordBinary buffer's backing array of just past the record header.
        // Usually the same as the corresponding buffer position. But need to
        // account for the user providing a buffer which is mapped on to a bigger backing array.
        // This may happen if the user provides a slice() of another buffer.
        int recBinPastHdrAbsolute = recBinPastHdr;
        if (recBinHasArray) {
            recBinPastHdrAbsolute += recordBinary.arrayOffset();
        }

        // Write index & event arrays

        // If compressing data ...
        if (compressionType > 0) {
            // Write into a single, temporary buffer
//            recordData.position(0);
//            recordData.put(  recordIndex.array(), 0, indexSize);
//            recordData.put( recordEvents.array(), 0, eventSize);
            System.arraycopy(recordIndex.array(),  0, recordData.array(), 0,         indexSize);
            System.arraycopy(recordEvents.array(), 0, recordData.array(), indexSize, eventSize);
            recordData.position(indexSize + eventSize);
        }
        // If NOT compressing data ...
        else {
            // Write directly into final buffer, past where header will go
//System.out.println("build: recordBinary len = " + userBufferSize +
//                   ", start pos = " + startingPosition + ", data to write = " +
//                   (RecordHeader.HEADER_SIZE_BYTES + indexSize + eventSize));

            if (recBinHasArray) {
                System.arraycopy(recordIndex.array(),  0, recordBinary.array(),
                                 recBinPastHdrAbsolute, indexSize);
                System.arraycopy(recordEvents.array(), 0, recordBinary.array(),
                                 recBinPastHdrAbsolute + indexSize, eventSize);
                recordBinary.position(recBinPastHdr + indexSize + eventSize);
            }
            else {
                recordBinary.position(recBinPastHdr);
                recordBinary.put(recordIndex.array(), 0, indexSize);
//System.out.println("build: recordBinary pos = " + recBinPos +
//                   ", eventSize = " + eventSize + ", recordEvents.array().len = " +
//                   recordEvents.array().length);
                recordBinary.put(recordEvents.array(), 0, eventSize);
            }
//System.out.println("build: writing index of size " + indexSize);
//System.out.println("build: events of size " + eventSize);
        }

        // Evio data is padded, but not necessarily all hipo data.
        // Uncompressed data length is NOT padded, but the record length is.
        int uncompressedDataSize = indexSize + eventSize;
        int compressedSize = -1;

        // Compress that temporary buffer into destination buffer
        // (skipping over where record header will be written).
        try {
            switch (compressionType) {
                case 1:
                    // LZ4 fastest compression
                    if (recBinHasArray) {
                        //System.out.println("1. uncompressed size = " + uncompressedDataSize);
                        compressedSize = dataCompressor.compressLZ4(
                                recordData.array(), 0, uncompressedDataSize,
                                recordBinary.array(), recBinPastHdrAbsolute,
                                (recordBinary.array().length - recBinPastHdrAbsolute));
                    }
                    else {
                        //System.out.println("2. uncompressed size = " + uncompressedDataSize);
                        compressedSize = dataCompressor.compressLZ4(
                                recordData, 0, uncompressedDataSize,
                                recordBinary, recBinPastHdr,
                               (recordBinary.capacity() - recBinPastHdr));
                    }
                    // Length of compressed data in bytes
                    header.setCompressedDataLength(compressedSize);
                    // Length of entire record in bytes (don't forget padding!)
                    header.setLength(4*header.getCompressedDataLengthWords() +
                                             RecordHeader.HEADER_SIZE_BYTES);
                    break;

                case 2:
                    // LZ4 highest compression
                    if (recBinHasArray) {
                        compressedSize = dataCompressor.compressLZ4Best(
                                recordData.array(), 0, uncompressedDataSize,
                                recordBinary.array(), recBinPastHdrAbsolute,
                                (recordBinary.array().length - recBinPastHdrAbsolute));
//System.out.println("Compressing data array from offset = 0, size = " + uncompressedDataSize +
//         " to output.array offset = " + recBinPastHdrAbsolute + ", compressed size = " +  compressedSize +
//         ", available size = " + (recordBinary.array().length - recBinPastHdrAbsolute));
                    }
                    else {
                        compressedSize = dataCompressor.compressLZ4Best(
                                recordData, 0, uncompressedDataSize,
                                recordBinary, recBinPastHdr,
                               (recordBinary.capacity() - recBinPastHdr));
//System.out.println("Compressing data buffer from offset = 0, size = " + uncompressedDataSize +
//        " to output.buffer offset = " + recBinPastHdr + ", compressed size = " +  compressedSize +
//        ", available size = " + (recordBinary.array().length - recBinPastHdr));
                    }
//System.out.println("BEFORE setting header len: comp size = " + header.getCompressedDataLength() +
//                            ", comp words = " + header.getCompressedDataLengthWords() + ", padding = " +
//                            header.getCompressedDataLengthPadding());
                    header.setCompressedDataLength(compressedSize);
                    header.setLength(4*header.getCompressedDataLengthWords() +
                                             RecordHeader.HEADER_SIZE_BYTES);
//System.out.println("AFTER setting, read back from header: comp size = " + header.getCompressedDataLength() +
//        ", comp words = " + header.getCompressedDataLengthWords() + ", padding = " +
//        header.getCompressedDataLengthPadding() + ", rec len = " + header.getLength());
                    break;

                case 3:                    
                    // GZIP compression
                    byte[] gzippedData = dataCompressor.compressGZIP(recordData.array(), 0,
                                                                     uncompressedDataSize);
                    recordBinary.position(recBinPastHdr);
                    recordBinary.put(gzippedData);
                    compressedSize = gzippedData.length;
                    header.setCompressedDataLength(compressedSize);
                    header.setLength(4*header.getCompressedDataLengthWords() +
                                             RecordHeader.HEADER_SIZE_BYTES);
                    break;

                case 0:
                default:
                    // No compression. The uncompressed data size may not be padded to a 4byte boundary,
                    // so make sure that's accounted for here.
                    header.setCompressedDataLength(0);
                    int words = uncompressedDataSize/4;
                    if(uncompressedDataSize % 4 != 0) words++;
                    header.setLength(words*4 + RecordHeader.HEADER_SIZE_BYTES);                    
                    //header.setLength(uncompressedDataSize + RecordHeader.HEADER_SIZE_BYTES);
            }
        }
        catch (HipoException e) {/* should not happen */}

        // Set the rest of the header values
        header.setEntries(eventCount);
        header.setDataLength(eventSize);
        header.setIndexLength(indexSize);
        // This unnecessarily re-does what is already done above ------ SEP 26 - timmer
        // Added this line ------ SEP 21 - gagik
        //header.setCompressedDataLength(compressedSize);
//        System.out.println(" COMPRESSED = " + compressedSize + "  events size = " + eventSize + "  type = "
//                + compressionType + "  uncompressed = " + uncompressedDataSize +
//                " record bytes = " + header.getLength() + "\n\n");
//        // Go back and write header into destination buffer
        try {
            // Does NOT change recordBinary pos or lim
            header.writeHeader(recordBinary, startingPosition);
        }
        catch (HipoException e) {/* never happen */}

        // Make ready to read
        recordBinary.limit(startingPosition + header.getLength()).position(0);
    }

    /**
     * Builds the record. Compresses data, header is constructed,
     * then header & data written into internal buffer.
     * If user header is not padded to 4-byte boundary, it's done here.
     * This method may be called multiple times in succession without
     * any problem.
     *
     * @param userHeader user's ByteBuffer which must be READY-TO-READ!
     */
    public void build(ByteBuffer userHeader) {

        // Arg check
        if (userHeader == null) {
            build();
            return;
        }

        //int userhSize = userHeader.array().length; // May contain unused bytes at end!
        // How much user-header data do we actually have (limit - position) ?
        int userHeaderSize = userHeader.remaining();

        if (userHeaderSize < 1) {
            build();
            return;
        }

//        System.out.println("  INDEX = 0 " + indexSize + "  " + (indexSize + userHeaderSize)
//                + "  DIFF " + userHeaderSize);

        int compressionType = header.getCompressionType();
        int uncompressedDataSize = indexSize;

        // Does the recordBinary buffer have an array backing?
        boolean recBinHasArray = recordBinary.hasArray();

        // Position in recordBinary buffer of just past the record header
        int recBinPastHdr = startingPosition + RecordHeader.HEADER_SIZE_BYTES;

        // Position in recordBinary buffer's backing array of just past the record header.
        // Usually the same as the corresponding buffer position. But need to
        // account for the user providing a buffer which is mapped on to a bigger backing array.
        // This may happen if the user provides a slice() of another buffer.
        int recBinPastHdrAbsolute = recBinPastHdr;
        if (recBinHasArray) {
            recBinPastHdrAbsolute += recordBinary.arrayOffset();
        }


        // If compressing data ...
        if (compressionType > 0) {
            // Write into a single, temporary buffer the following:

            // 1) uncompressed index array
            // recordData.position(0);
            // Note, put() will increment position
            // recordData.put(recordIndex.array(), 0, indexSize);
            System.arraycopy(recordIndex.array(),  0, recordData.array(), 0, indexSize);

            // 2) uncompressed user header

            // If there is a backing array, do it the fast way ...
            if (userHeader.hasArray()) {
                //recordData.put(userHeader.array(), 0, userHeaderSize);
                System.arraycopy(userHeader.array(), 0, recordData.array(), indexSize, userHeaderSize);
            }
            else {
                int pos = userHeader.position();
                recordData.position(indexSize);
                recordData.put(userHeader);
                // Set position back to original value
                userHeader.position(pos);
            }

            // Account for unpadded user header.
            // This will find the user header length in words & account for padding.
            header.setUserHeaderLength(userHeaderSize);
            // Hop over padded user header length
            uncompressedDataSize += 4*header.getUserHeaderLengthWords();
            //recordData.position(uncompressedDataSize);

            // 3) uncompressed data array
            //recordData.put(recordEvents.array(), 0, eventSize);
            System.arraycopy(recordEvents.array(), 0, recordData.array(), uncompressedDataSize, eventSize);
            // Evio data is padded, but not necessarily all hipo data.
            // Uncompressed data length is NOT padded, but the record length is.
            uncompressedDataSize += eventSize;
            recordData.position(uncompressedDataSize);
        }
        // If NOT compressing data ...
        else {
            // Write directly into final buffer, past where header will go

            // 1) uncompressed index array
            if (recBinHasArray) {
                System.arraycopy(recordIndex.array(),  0, recordBinary.array(), recBinPastHdrAbsolute, indexSize);
                recordBinary.position(recBinPastHdr + indexSize);
            }
            else {
                recordBinary.position(recBinPastHdr);
                recordBinary.put(recordIndex.array(), 0, indexSize);
            }

            // 2) uncompressed user header array
            if (recBinHasArray && userHeader.hasArray()) {
                //recordBinary.put(userHeader.array(), 0, userHeaderSize);
                System.arraycopy(userHeader.array(), 0, recordBinary.array(),
                                 recBinPastHdrAbsolute+indexSize, userHeaderSize);
            }
            else {
                int pos = userHeader.position();
                recordBinary.put(userHeader);
                userHeader.position(pos);
            }

            header.setUserHeaderLength(userHeaderSize);
            uncompressedDataSize += 4*header.getUserHeaderLengthWords();

            // 3) uncompressed data array (hipo/evio data is already padded)
            if (recBinHasArray) {
                System.arraycopy(recordEvents.array(), 0, recordBinary.array(),
                                 recBinPastHdrAbsolute + uncompressedDataSize, eventSize);
                recordBinary.position(recBinPastHdr + uncompressedDataSize + eventSize);
            }
            else {
                recordBinary.position(recBinPastHdr + uncompressedDataSize);
                recordBinary.put(recordEvents.array(), 0, eventSize);
            }

            // TODO: This does NOT include padding. Shouldn't we do that?
            uncompressedDataSize += eventSize;
        }

        // Compress that temporary buffer into destination buffer
        // (skipping over where record header will be written).
        int compressedSize;
        try {
            switch (compressionType) {
                case 1:
                    // LZ4 fastest compression
                    if (recBinHasArray) {
                        compressedSize = dataCompressor.compressLZ4(
                                recordData.array(), 0, uncompressedDataSize,
                                recordBinary.array(),
                                recBinPastHdrAbsolute,
                                (recordBinary.array().length - recBinPastHdrAbsolute));
                    }
                    else {
                        compressedSize = dataCompressor.compressLZ4(
                                recordData, 0, uncompressedDataSize,
                                recordBinary,
                                recBinPastHdr,
                               (recordBinary.capacity() - recBinPastHdr));
                    }
                    // Length of compressed data in bytes
                    header.setCompressedDataLength(compressedSize);
                    // Length of entire record in bytes (don't forget padding!)
                    header.setLength(4*header.getCompressedDataLengthWords() +
                                             RecordHeader.HEADER_SIZE_BYTES);
                    break;

                case 2:
                    // LZ4 highest compression
                    if (recBinHasArray) {
                        compressedSize = dataCompressor.compressLZ4Best(
                                recordData.array(), 0, uncompressedDataSize,
                                recordBinary.array(),
                                recBinPastHdrAbsolute,
                                (recordBinary.array().length - recBinPastHdrAbsolute));
                    }
                    else {
                        compressedSize = dataCompressor.compressLZ4Best(
                                recordData, 0, uncompressedDataSize,
                                recordBinary,
                                recBinPastHdr,
                               (recordBinary.capacity() - recBinPastHdr));
                    }
                    header.setCompressedDataLength(compressedSize);
                    header.setLength(4*header.getCompressedDataLengthWords() +
                                             RecordHeader.HEADER_SIZE_BYTES);
                    break;

                case 3:
                    // GZIP compression
                    byte[] gzippedData = dataCompressor.compressGZIP(recordData.array(), 0,
                                                                     uncompressedDataSize);
                    recordBinary.position(recBinPastHdr);
                    recordBinary.put(gzippedData);
                    compressedSize = gzippedData.length;
                    header.setCompressedDataLength(compressedSize);
                    header.setLength(4*header.getCompressedDataLengthWords() +
                                             RecordHeader.HEADER_SIZE_BYTES);
                    break;

                case 0:
                default:
                    // No compression. The uncompressed data size may not be padded to a 4byte boundary,
                    // so make sure that's accounted for here.
                    header.setCompressedDataLength(0);
                    int words = uncompressedDataSize/4;
                    if(uncompressedDataSize % 4 != 0) words++;
                    header.setLength(words*4 + RecordHeader.HEADER_SIZE_BYTES);
            }
        }
        catch (HipoException e) {/* should not happen */}

        //System.out.println(" COMPRESSED SIZE = " + compressedSize);

        // Set header values (user header length already set above)
        header.setEntries(eventCount);
        header.setDataLength(eventSize);
        header.setIndexLength(indexSize);

        // Go back and write header into destination buffer
        try {
            header.writeHeader(recordBinary, startingPosition);
        }
        catch (HipoException e) {/* never happen */}
        
        //System.out.println();
        // Make ready to read
        recordBinary.limit(startingPosition + header.getLength()).position(0);
    }
}
