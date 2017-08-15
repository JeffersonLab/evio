package org.jlab.coda.jevio;


import net.jpountz.lz4.LZ4Compressor;
import net.jpountz.lz4.LZ4Factory;

import java.io.*;
import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.util.BitSet;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * An EventWriter object is used for writing events to a file or to a byte buffer.
 * This class does NOT write versions 1-3 data, only version 4!
 * This class is thread-safe.
 *
 * @author heddle
 * @author timmer
 */
public class EventWriter {

    /**
	 * This <code>enum</code> denotes the status of a read. <br>
	 * SUCCESS indicates a successful read/write. <br>
	 * END_OF_FILE indicates that we cannot read because an END_OF_FILE has occurred. Technically this means that what
	 * ever we are trying to read is larger than the buffer's unread bytes.<br>
	 * EVIO_EXCEPTION indicates that an EvioException was thrown during a read, possibly due to out of range values,
	 * such as a negative start position.<br>
     * CANNOT_OPEN_FILE  that we cannot write because the destination file cannot be opened.<br>
	 * UNKNOWN_ERROR indicates that an unrecoverable error has occurred.
	 */
	public enum IOStatus {
		SUCCESS, END_OF_FILE, EVIO_EXCEPTION, CANNOT_OPEN_FILE, UNKNOWN_ERROR
	}


    /**
     * Offset to where the block length is written in the byte buffer,
     * which always has a physical record header at the top.
     */
    static final int BLOCK_LENGTH_OFFSET = 0;

    /**
     * Offset to where the block number is written in the byte buffer,
     * which always has a physical record header at the top.
     */
    static final int BLOCK_NUMBER_OFFSET = 4;

    /**
     * Offset to where the header length is written in the byte buffer,
     * which always has a physical record header at the top.
     */
    static final int HEADER_LENGTH_OFFSET = 8;

    /**
     * Offset to where the event count is written in the byte buffer,
     * which always has a physical record header at the top.
     */
    static final int EVENT_COUNT_OFFSET = 12;

    /**
     * Offset to where the first reserved word is written in the byte buffer,
     * which always has a physical record header at the top.
     */
    static final int RESERVED1_COUNT_OFFSET = 16;

    /**
     * Offset to where the bit information is written in the byte buffer,
     * which always has a physical record header at the top.
     */
    static final int BIT_INFO_OFFSET = 20;

    /**
     * Offset to where the magic number is written in the byte buffer,
     * which always has a physical record header at the top.
     */
    static final int MAGIC_OFFSET = 28;

    /** Mask to get version number from 6th int in block. */
    static final int VERSION_MASK = 0xff;

    /**
     * The default maximum size for a single block used for writing in ints or words.
     * This gives block sizes of 2^22 ints or 16MB.
     * It is a soft limit since a single
     * event larger than this limit may need to be written.
     */
    static final int DEFAULT_BLOCK_SIZE = 4194304;

    /** The default maximum event count for a single block used for writing. */
    static final int DEFAULT_BLOCK_COUNT = 10000;

    /**
     * The upper limit of maximum size for a single block used for writing
     * is 2^25 ints or words. This gives block sizes of about 134MB.
     * It is a soft limit since a single event larger than this limit
     * may need to be written.
     */
    static final int MAX_BLOCK_SIZE = 33554432;

    /** The upper limit of maximum event count for a single block used for writing. */
    static final int MAX_BLOCK_COUNT = 100000;

    /**
     * The lower limit of maximum size for a single block used for writing,
     * in ints (words). This gives block sizes of about 32k bytes.
     */
    static final int MIN_BLOCK_SIZE = 16;

    /** The lower limit of maximum event count for a single block used for writing. */
    static final int MIN_BLOCK_COUNT = 1;

    /** Size of block header in bytes. */
    static final int headerBytes = 32;

    /** Size of block header in bytes. */
    static final int headerWords = 8;

    /** Turn on or off the debug printout. */
    static final boolean debug = false;



    /**
     * Maximum block size (32 bit words) for a single block (block header & following events).
     * There may be exceptions to this limit if the size of an individual
     * event exceeds this limit.
     * Default is {@link #DEFAULT_BLOCK_SIZE}.
     */
    private int blockSizeMax;

    /**
     * Maximum number of events in a block (events following a block header).
     * Default is {@link #DEFAULT_BLOCK_COUNT}.
     */
    private int blockCountMax;

    /** Running count of the block number. The next one to use starting with 1. */
    private int blockNumber;

    /**
     * Dictionary to include in xml format in the first event of the first block
     * when writing the file.
     */
    private String xmlDictionary;

    /** True if wrote dictionary to a single file (not all splits taken together). */
    private boolean wroteDictionary;

    /** Byte array containing dictionary in evio format but <b>without</b> block header. */
    private byte[] dictionaryByteArray;

    /** The number of bytes it takes to hold the dictionary
     *  in a bank of evio format. Basically the size of
     *  {@link #dictionaryByteArray} with the length of a bank header added. */
    private int dictionaryBytes;

    /** Has a first event been defined? */
    private boolean haveFirstEvent;

    /** Byte array containing firstEvent in evio format but <b>without</b> block header. */
    private byte[] firstEventByteArray;

    /** The number of bytes it takes to hold the first event
     *  in a bank of evio format. Basically the size of
     *  {@link #firstEventByteArray} with the length of a bank header added. */
    private int firstEventBytes;

    /** The number of bytes it takes to hold both the dictionary and the first event
     *  in banks of evio format (evio headers, not block headers, included). */
    private int commonBlockByteSize;

    /** Number of events in the common block. At most 2 - dictionary & firstEvent. */
    private int commonBlockCount;

    /**
     * Bit information in the block headers:<p>
     * <ul>
     * <li>Bit one: is the first event a dictionary?  Used in first block only.
     * <li>Bit two: is this the last block?
     * </ul>
     */
    private BitSet bitInfo;

    /** <code>True</code> if {@link #close()} was called, else <code>false</code>. */
    private boolean closed;

    /** <code>True</code> if writing to file, else <code>false</code>. */
    private boolean toFile;

    /** <code>True</code> if appending to file/buffer, <code>false</code> if (over)writing. */
    private boolean append;

    /** <code>True</code> if appending to file/buffer with dictionary, <code>false</code>. */
    private boolean hasAppendDictionary;

    /** Number of bytes of data to shoot for in a single block including header. */
    private int targetBlockSize;

    /** Version 4 block header reserved int 1. Used by CODA for source ID in event building. */
    private int reserved1;

    /** Version 4 block header reserved int 2. */
    private int reserved2;

    /** Number of bytes written to the current buffer. */
    private long bytesWrittenToBuffer;

    /** Number of events written to final destination buffer or file's internal buffer
     * including dictionary (although may not be flushed yet). */
    private int eventsWrittenToBuffer;

    /**
     * Total number of events written to buffer or file (although may not be flushed yet).
     * Will be the same as eventsWrittenToBuffer (- dictionary) if writing to buffer.
     * If the file being written to is split, this value refers to all split files
     * taken together. Does NOT include dictionary(ies).
     */
    private int eventsWrittenTotal;

    /** When writing to a buffer, keep tabs on the front of the last (non-ending) header written. */
    private int currentHeaderPosition;

    /** Size in 32-bit words of the currently-being-used block (includes entire block header). */
    private int currentBlockSize;

    /** Number of events written to the currently-being-used block (including dictionary if first blk). */
    private int currentBlockEventCount;

    /** Total size of the buffer in bytes. */
    private int bufferSize;

    /**
     * The output buffer when writing to a buffer.
     * The internal buffer when writing to a file.
     */
    private ByteBuffer buffer;

    /** The byte order in which to write a file or buffer. */
    private ByteOrder byteOrder;

    /** Is this buffer a direct ByteBuffer or not. */
    private boolean direct;

    //-----------------------
    // File related members
    //-----------------------

    /** File currently being written to. */
    private File currentFile;

    /** The object used for writing current file. */
    private RandomAccessFile raf;

    /** The file channel, used for writing current file, derived from raf. */
    private FileChannel fileChannel;

    /** Running count of split output files. */
    private int splitCount;

    /** Part of filename without run or split numbers. */
    private String baseFileName;

    /** Number of C-style int format specifiers contained in baseFileName. */
    private int specifierCount;

    /** Run number possibly used in naming split files. */
    private int runNumber;

    /** Id used with more than 1 ER or file-writing component to identify
     *  which data stream created the file. */
    private int streamId;

    /**
     * Do we split the file into several smaller ones (val > 0)?
     * If so, this gives the maximum number of bytes to make each file in size.
     */
    private long split;

    /** Is it OK to overwrite a previously existing file? */
    private boolean overWriteOK;

    /** Number of bytes written to the current file (including ending header),
     *  not the total in all split files. */
    private long bytesWrittenToFile;

    /** Number of events actually written to the current file - not the total in
     * all split files - including dictionary. */
    private int eventsWrittenToFile;

    /** <code>True</code> if internal buffer has the last empty block header
     * written and buffer position is immediately after it, else <code>false</code>. */
    private boolean lastEmptyBlockHeaderExists;

    /** If true, each block header written to a file will
     * contain a list of all event sizes in compacted form. */
    private boolean writeBlockEventSizes;

    //-------------------------------------
    // LZ4 Compression related members
    //-------------------------------------

    /** If true, write files as compressed evio output. */
    private boolean compressedOutput;

    /** Object used to compress data. */
    private LZ4Compressor lz4Comp;

    /** Buffer used to hold uncompressed data. */
    private ByteBuffer uncompressedDataBuf;

    /**
     * If creating an extended evio block header, this stores
     * sizes of all events in block - each as one integer.
     * Each size is the total number of bytes in the event
     * (not the length in the evio header which does not include itself).
     */
    public int[]  eventSizes;

    /** Number of current, valid entries in eventSizes. */
    public int eventSizesCount;

    /**
     * If creating an extended evio block header, this stores a compacted version
     * of all events sizes in that block. This gets written into the header.<p>
     *
     * The format is: if a high bit in a byte is 1, that's the last byte of
     * of a size. The other 7 bits are the size. If it's 0, the lower 7 bits
     * of the next byte are also part of the size. Note, big end comes first.<p>
     *
     * If a bit pattern looks like:
     *
     * <pre>
     * byte:      1         2         3         4
     *        1aaaaaaa  0bbbbbbb  0ccccccc  1ddddddd
     * </pre>
     *
     * The low 7 bits of the first byte are the first size (aaaaaaa).
     * The next size is composed of the low 7 bits of the
     * next 3 bytes (2,3 & 4) put together (bbbbbbbcccccccddddddd).
     */
    public byte[] blockIndexes;

    /** Number of valid bytes in blockIndexes.
     * This is only calculated when {@link #toByteSizes()} is called. */
    public int blockIndexBytes;


    public int runningTotalBlockIndexBytes;



    /**
     * This method doubles the amount of memory available to the blockIndexes
     * byte array. It also copies the existing valid data over to the new array
     * which is returned and also assigned to the {@link #blockIndexes} variable.
     */
    private void expandBlockIndexes() {

        // Initialize things
        if (blockIndexes == null) {
            // Start with no valid byte data
            blockIndexBytes = 0;
            // Start with 4k bytes
            blockIndexes = new byte[4000];
        }
        else {
//System.out.println("Double blockIndexes to " + (2*blockIndexes.length) + " bytes");
            // Double the amount of storage
            byte[] newBlockIndexBytes = new byte[2*blockIndexes.length];
            System.arraycopy(blockIndexes, 0,
                             newBlockIndexBytes, 0,
                             blockIndexBytes);
            blockIndexes = newBlockIndexBytes;
        }
    }

    
    /**
     * This method doubles the amount of memory available to the eventSizes
     * int array. It also copies the existing valid data over to the new array
     * which is returned and also assigned to the {@link #eventSizes} variable.
     */
    private void expandSizeArray() {

        // Initialize things
        if (eventSizes == null) {
            // Start with no valid int data
            eventSizesCount = 0;
            // Start with 1k event sizes
            eventSizes = new int[1000];
        }
        else if (eventSizesCount + 1 > eventSizes.length) {
//System.out.println("Double eventSizes to " + (2*eventSizes.length) + " ints");
            // Double the amount of storage
            int[] newEventSizes = new int[2*eventSizes.length];
            System.arraycopy(eventSizes, 0,
                             newEventSizes, 0,
                             eventSizesCount);
            eventSizes = newEventSizes;
        }
    }


    /**
     * This method adds an event size to the array of sizes
     * while making sure there is room for it in the array - doubling memory if necessary.
     * If array length is increased it also copies the existing valid data over to the new array.
     */
    public void addSizeToArray(int size) {
        // Minimum size evio event (bank) is 8 bytes
        if (!writeBlockEventSizes || size < 8) {
            return;
        }

        // Initialize or expand things if necessary
        expandSizeArray();

        eventSizes[eventSizesCount++] = size;

        // Keep a running total of how many bytes the block header indexes take up
        // (NOT including the filler at the end to ensure 4 byte multiple).

        // If event size fits in 7 bits ...
        if (size <= 127) {
            runningTotalBlockIndexBytes++;
        }
        // If event size fits in 14 bits ...
        else if (size <= 16384) {
            runningTotalBlockIndexBytes += 2;
        }
        // If event size fits in 21 bits ...
        else if (size <= 2097152) {
            runningTotalBlockIndexBytes += 3;
        }
        // If event size fits in 28 bits ...
        else if (size <= 268435456) {
            runningTotalBlockIndexBytes += 4;
        }
        // All other integers
        else {
            runningTotalBlockIndexBytes += 5;
        }
    }


    /**
     * This method takes the number of bytes currently needed to store
     * an array of event lengths in the evio block header and rounds it
     * up to the closest 4-byte boundary.
     *
     * @return  bytes currently needed to store an array of event lengths
     *          in the evio block header and rounded up to the closest
     *          4-byte boundary.
     */
    private int getEventLengthsArraySize() {
        // Following is equivalent to:
        //    ((i + 3)/4) * 4
        return ( ((runningTotalBlockIndexBytes + 3) >>> 2) << 2 );
    }


    /**
     * This method takes the number of bytes currently needed to store
     * an array of event lengths in the evio block header, plus the size
     * given in the argument, and rounds it up to the closest 4-byte boundary.
     * If not using block header event sizes, return 0;
     *
     * @param size additional event size to include in calculation.
     * @return  bytes currently needed to store an array of event lengths
     *          in the evio block header, plus the size given in the argument,
     *          and rounded up to the closest 4-byte boundary. Return 0 if not
     *          storing events sizes in block header.
     */
    private int getEventLengthsArraySize(int size) {
        if (!writeBlockEventSizes) {
            return 0;
        }

        if (size < 8) {
            return ( ((runningTotalBlockIndexBytes + 3) >>> 2) << 2 );
        }

        int runningSize = runningTotalBlockIndexBytes;

        // If event size fits in 7 bits ...
        if (size <= 127) {
            runningSize++;
        }
        // If event size fits in 14 bits ...
        else if (size <= 16384) {
            runningSize += 2;
        }
        // If event size fits in 21 bits ...
        else if (size <= 2097152) {
            runningSize += 3;
        }
        // If event size fits in 28 bits ...
        else if (size <= 268435456) {
            runningSize += 4;
        }
        // All other integers
        else {
            runningSize += 5;
        }

        return ( ((runningSize + 3) >>> 2) << 2 );
    }


    /**
     * This method takes an array of integer event sizes and compacts it into
     * a form suitable for being written as part of the evio block header.
     * Besides being returned, the compacted data is also stored in the array
     * {@link #blockIndexes}. If this array is too small to hold the data,
     * its memory is doubled and the valid data copied over. <p>
     *
     * The variable {@link #blockIndexBytes} is set to the number of
     * valid bytes in the blockIndexes array. Note that the number of bytes
     * includes any bytes of value 0 added at the end to make a 4-byte
     * boundary.
     *
     * @return byte array with compacted size data suitable for being written
     *         as part of the evio block header.
     *         Null if a size < 8 bytes.
     */
    public byte[] toByteSizes() {

        // Init things
        if (blockIndexes == null) {
            expandBlockIndexes();
        }

        int size;
        blockIndexBytes = 0;

System.out.println("toByteSizes: converting " + eventSizesCount + " integers into bytes");
        for (int i=0; i < eventSizesCount; i++) {
            size = eventSizes[i];

            // No nonsense sizes allowed
            if (size < 8) {
                return null;
            }
            // If event size fits in 7 bits ...
            else if (size <= 127) {
                if (blockIndexBytes + 1 > blockIndexes.length) {
                    expandBlockIndexes();
                }

                // Most significant bit = 1 signifies no more bytes
                // following in order to represent this size.
                blockIndexes[blockIndexBytes++] = (byte) (128 | size);
//System.out.println("\nwriting 1 byte, " + size);
            }
            // If event size fits in 14 bits ...
            else if (size <= 16384) {
                if (blockIndexBytes + 2 > blockIndexes.length) {
                    expandBlockIndexes();
                }

                // ALWAYS STORE AS BIG ENDIAN regardless of underlying architecture.

                // 1st byte: most significant bit = 0 signifies another byte
                // following in order to represent this size. Store highest 7 bits.
                blockIndexes[blockIndexBytes++] = (byte) (size >>> 7);

                // 2nd byte: most significant bit = 1 signifies no more bytes
                // following in order to represent this size. Store lowest 7 bits.
                blockIndexes[blockIndexBytes++] = (byte) (128 | (size & 127));
//System.out.println("\nwriting 2 bytes, " + size);
            }
            // If event size fits in 21 bits ...
            else if (size <= 2097152) {
                if (blockIndexBytes + 3 > blockIndexes.length) {
                    expandBlockIndexes();
                }

                blockIndexes[blockIndexBytes++] = (byte)        (size >>> 14);
                blockIndexes[blockIndexBytes++] = (byte) (127 & (size >>> 7));
                blockIndexes[blockIndexBytes++] = (byte) (128 | (size & 127));
//System.out.println("\nwriting 3 bytes, " + size);
            }
            // If event size fits in 28 bits ...
            else if (size <= 268435456) {
                if (blockIndexBytes + 4 > blockIndexes.length) {
                    expandBlockIndexes();
                }

                blockIndexes[blockIndexBytes++] = (byte)        (size >>> 21);
                blockIndexes[blockIndexBytes++] = (byte) (127 & (size >>> 14));
                blockIndexes[blockIndexBytes++] = (byte) (127 & (size >>> 7));
                blockIndexes[blockIndexBytes++] = (byte) (128 | (size & 127));
//System.out.println("\nwriting 4 bytes, " + size);
            }
            // All other integers
            else {
                if (blockIndexBytes + 5 > blockIndexes.length) {
                    expandBlockIndexes();
                }

                blockIndexes[blockIndexBytes++] = (byte)        (size >>> 28);
                blockIndexes[blockIndexBytes++] = (byte) (127 & (size >>> 21));
                blockIndexes[blockIndexBytes++] = (byte) (127 & (size >>> 14));
                blockIndexes[blockIndexBytes++] = (byte) (127 & (size >>> 7));
                blockIndexes[blockIndexBytes++] = (byte) (128 | (size & 127));
//System.out.println("\nwriting 5 bytes, " + size);
            }
        }

        // Round up # of bytes to 4-byte boundary.
        int roundedUp = (((blockIndexBytes + 3) >>> 2) << 2);

//System.out.println("\ntoByteSizes: data bytes = " + blockIndexBytes +
//", roundUp = " + roundedUp);
        // Fill last, filler bytes with zeros
        for (int i=blockIndexBytes; i < roundedUp; i++) {
//System.out.println("filling element " + i + " with zeros");
            blockIndexes[i] = (byte) 0;
        }
        System.out.println("toByteSizes: converted into " + roundedUp + " bytes");

        blockIndexBytes = roundedUp;

        return blockIndexes;
    }


    /**
     * This method takes an array of compacted event sizes and expands
     * it back into an array of integer lengths.
     * Besides being returned, the expanded data is also stored in the array
     * {@link #eventSizes}. If this array is too small to hold the data,
     * its memory is doubled and the valid data copied over.
     *
     * @param sizes array of compacted event sizes.
     * @param len   number of valid bytes in sizes arg.
     * @return int array with expanded size data.
     *         Null if arg null or data format error.
     */
    public int[] toIntegerSizes(byte[] sizes, int len) {

        if (sizes == null) {
            return null;
        }

        int arrayLen = sizes.length;
        len = len > arrayLen ? arrayLen : len;

        // Init things
        expandSizeArray();

        byte b;
        boolean isLastByte;
        int size, index = 0;
        eventSizesCount = 0;

        top:
        while (index < len) {

            size = 0;
            isLastByte = false;

            while (!isLastByte) {
                // If data format error
                if (index >= arrayLen)  {
                    return null;
                }

                // Examine 8 bits at a time, the lower 7 of which contribute to size.
                // If MSBit = 1, it's the last byte of the size, else if = 0,
                // read next byte too.
                b = sizes[index++];

                // 0 marks the end of valid data
                if (b == 0) {
                    //System.out.println("HIT ZERO, ending");
                    break top;
                }

                isLastByte = (b & 128) > 0;
                b = (byte) (b & 127);
                size = (size << 7) | b;
            }

            // Expand if necessary
            expandSizeArray();

            eventSizes[eventSizesCount++] = size;
        }

        return eventSizes;
    }


    //-----------------------

    
    /** Class used to close files in order received, each in its own thread,
     *  to avoid slowing down while file splitting. */
    private final class FileCloser {

        /** Thread pool with 1 thread. */
        private final ExecutorService threadPool;

        FileCloser() {threadPool = Executors.newSingleThreadExecutor();}

        /**
         * Close the given file, in the order received, in a separate thread.
         * @param raf   random access file to close
         */
        void closeFile(RandomAccessFile raf) {
            threadPool.submit(new CloseThd(raf));
        }

        /** Close the thread pool in this object while executing all existing tasks. */
        void close() {threadPool.shutdown();}


        private class CloseThd implements Runnable {
            private RandomAccessFile raf;

            CloseThd(RandomAccessFile raf) {
                this.raf  = raf;
            }

            public void run() {
                try {
                    if (raf != null) {
                        // This closes associated channel too
                        raf.close();
                    }
                }
                catch (IOException e) {
                    e.printStackTrace();
                }
            }
        };
    }


    /** Object used to close files in a separate thread when splitting
     *  so as to allow writing speed not to dip so low. */
    private FileCloser fileCloser;


    //---------------------------------------------
    // FILE Constructors
    //---------------------------------------------

    /**
     * Creates an <code>EventWriter</code> for writing to a file in native byte order.
     * If the file already exists, its contents will be overwritten.
     * If it doesn't exist, it will be created.
     *
     * @param file the file object to write to.<br>
     * @throws EvioException file cannot be created
     */
    public EventWriter(File file) throws EvioException {
        this(file, false);
    }

    /**
     * Creates an <code>EventWriter</code> for writing to a file in native byte order.
     * If the file already exists, its contents will be overwritten unless
     * it is being appended to. If it doesn't exist, it will be created.
     *
     * @param file     the file object to write to.<br>
     * @param append   if <code>true</code> and the file already exists,
     *                 all events to be written will be appended to the
     *                 end of the file.
     *
     * @throws EvioException file cannot be created
     */
    public EventWriter(File file, boolean append) throws EvioException {
        this(file, DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT,
             ByteOrder.nativeOrder(), null, null, true, append);
    }

    /**
     * Creates an <code>EventWriter</code> for writing to a file in native byte order.
     * If the file already exists, its contents will be overwritten unless
     * it is being appended to. If it doesn't exist, it will be created.
     *
     * @param file       the file object to write to.<br>
     * @param dictionary dictionary in xml format or null if none.
     * @param append     if <code>true</code> and the file already exists,
     *                   all events to be written will be appended to the
     *                   end of the file.
     *
     * @throws EvioException file cannot be created
     */
    public EventWriter(File file, String dictionary, boolean append) throws EvioException {
        this(file, DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT,
             ByteOrder.nativeOrder(), dictionary, null, true, append);
    }

    /**
     * Creates an <code>EventWriter</code> for writing to a file in native byte order.
     * If the file already exists, its contents will be overwritten.
     * If it doesn't exist, it will be created.
     *
     * @param filename name of the file to write to.<br>
     * @throws EvioException file cannot be created
     */
    public EventWriter(String filename) throws EvioException {
        this(filename, false);
    }

    /**
     * Creates an <code>EventWriter</code> for writing to a file in native byte order.
     * If the file already exists, its contents will be overwritten unless
     * it is being appended to. If it doesn't exist, it will be created.
     *
     * @param filename name of the file to write to.<br>
     * @param append   if <code>true</code> and the file already exists,
     *                 all events to be written will be appended to the
     *                 end of the file.
     *
     * @throws EvioException file cannot be created
     */
    public EventWriter(String filename, boolean append) throws EvioException {
        this(new File(filename), DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT,
             ByteOrder.nativeOrder(), null, null, true, append);
    }

    /**
     * Creates an <code>EventWriter</code> for writing to a file in the
     * specified byte order.
     * If the file already exists, its contents will be overwritten unless
     * it is being appended to. If it doesn't exist, it will be created.
     *
     * @param filename  name of the file to write to.<br>
     * @param append    if <code>true</code> and the file already exists,
     *                  all events to be written will be appended to the
     *                  end of the file.
     * @param byteOrder the byte order in which to write the file.
     *
     * @throws EvioException file cannot be created
     */
    public EventWriter(String filename, boolean append, ByteOrder byteOrder) throws EvioException {
        this(new File(filename), DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT,
             byteOrder, null, null, true, append);
    }

    /**
     * Create an <code>EventWriter</code> for writing events to a file.
     * If the file already exists, its contents will be overwritten.
     * If it doesn't exist, it will be created.
     *
     * @param file          the file object to write to.<br>
     * @param blockSizeMax  the max blocksize to use which must be &gt;= {@link #MIN_BLOCK_SIZE}
     *                      and &lt;= {@link #MAX_BLOCK_SIZE} ints.
     *                      The size of the block will not be larger than this size
     *                      unless a single event itself is larger.
     * @param blockCountMax the max number of events (including dictionary) in a single block
     *                      which must be &gt;= {@link #MIN_BLOCK_COUNT} and &lt;= {@link #MAX_BLOCK_COUNT}.
     * @param byteOrder     the byte order in which to write the file.
     * @param xmlDictionary dictionary in xml format or null if none.
     * @param bitInfo       set of bits to include in first block header.
     *
     * @throws EvioException if blockSizeMax or blockCountMax exceed limits;
     *                       file cannot be created
     */
    public EventWriter(File file, int blockSizeMax, int blockCountMax, ByteOrder byteOrder,
                                 String xmlDictionary, BitSet bitInfo)
                                        throws EvioException {

        this(file, blockSizeMax, blockCountMax, byteOrder,
             xmlDictionary, bitInfo, true, false);
    }

    /**
     * Create an <code>EventWriter</code> for writing events to a file.
     * If the file already exists, its contents will be overwritten
     * unless the "overWriteOK" argument is <code>false</code> in
     * which case an exception will be thrown. If it doesn't exist,
     * it will be created.
     *
     * @param file          the file to write to.<br>
     * @param blockSizeMax  the max blocksize to use which must be &gt;= {@link #MIN_BLOCK_SIZE}
     *                      and &lt;= {@link #MAX_BLOCK_SIZE} ints.
     *                      The size of the block will not be larger than this size
     *                      unless a single event itself is larger.
     * @param blockCountMax the max number of events (including dictionary) in a single block
     *                      which must be &gt;= {@link #MIN_BLOCK_COUNT} and &lt;= {@link #MAX_BLOCK_COUNT}.
     * @param byteOrder     the byte order in which to write the file.
     * @param xmlDictionary dictionary in xml format or null if none.
     * @param bitInfo       set of bits to include in first block header.
     * @param overWriteOK   if <code>false</code> and the file already exists,
     *                      an exception is thrown rather than overwriting it.
     *
     * @throws EvioException if blockSizeMax or blockCountMax exceed limits;
     *                       file exists and cannot be deleted;
     *                       file exists and user requested no deletion.
     */
    public EventWriter(File file, int blockSizeMax, int blockCountMax, ByteOrder byteOrder,
                                 String xmlDictionary, BitSet bitInfo, boolean overWriteOK)
                                        throws EvioException {

        this(file, blockSizeMax, blockCountMax, byteOrder,
             xmlDictionary, bitInfo, overWriteOK, false);
    }

    /**
     * Create an <code>EventWriter</code> for writing events to a file.
     * If the file already exists, its contents will be overwritten
     * unless the "overWriteOK" argument is <code>false</code> in
     * which case an exception will be thrown. Unless ..., the option to
     * append these events to an existing file is <code>true</code>,
     * in which case everything is fine. If the file doesn't exist,
     * it will be created. Byte order defaults to big endian if arg is null.
     *
     * @param file          the file to write to.<br>
     * @param blockSizeMax  the max blocksize to use which must be &gt;= {@link #MIN_BLOCK_SIZE}
     *                      and &lt;= {@link #MAX_BLOCK_SIZE} ints.
     *                      The size of the block will not be larger than this size
     *                      unless a single event itself is larger.
     * @param blockCountMax the max number of events (including dictionary) in a single block
     *                      which must be &gt;= {@link #MIN_BLOCK_COUNT} and &lt;= {@link #MAX_BLOCK_COUNT}.
     * @param byteOrder     the byte order in which to write the file. This is ignored
     *                      if appending to existing file.
     * @param xmlDictionary dictionary in xml format or null if none.
     * @param bitInfo       set of bits to include in first block header.
     * @param overWriteOK   if <code>false</code> and the file already exists,
     *                      an exception is thrown rather than overwriting it.
     * @param append        if <code>true</code> and the file already exists,
     *                      all events to be written will be appended to the
     *                      end of the file.
     *
     * @throws EvioException if blockSizeMax or blockCountMax exceed limits;
     *                       if defined dictionary while appending;
     *                       if file arg is null;
     *                       if file could not be opened or positioned;
     *                       if file exists but user requested no over-writing or appending.
     *
     */
    public EventWriter(File file, int blockSizeMax, int blockCountMax, ByteOrder byteOrder,
                                 String xmlDictionary, BitSet bitInfo, boolean overWriteOK,
                                 boolean append) throws EvioException {

        this(file.getPath(), null, null, 0, 0, blockSizeMax,
             blockCountMax, 0, byteOrder,
             xmlDictionary, bitInfo, overWriteOK, append);
    }


    /**
     * Create an <code>EventWriter</code> for writing events to a file.
     * This constructor is useful when splitting and automatically naming
     * the split files. If any of the generated files already exist,
     * it will <b>NOT</b> be overwritten. Byte order defaults to big endian.
     *
     * @param baseName      base file name used to generate complete file name (may not be null)
     * @param directory     directory in which file is to be placed
     * @param runType       name of run type configuration to be used in naming files
     * @param runNumber     number of the CODA run, used in naming files
     * @param split         if &lt; 1, do not split file, write to only one file of unlimited size.
     *                      Else this is max size in bytes to make a file
     *                      before closing it and starting writing another.
     * @param byteOrder     the byte order in which to write the file.
     *                      Defaults to big endian if null.
     * @param xmlDictionary dictionary in xml format or null if none.
     *
     * @throws EvioException if baseName arg is null;
     *                       if file could not be opened, positioned, or written to;
     *                       if file exists.
     */
    public EventWriter(String baseName, String directory, String runType,
                                 int runNumber, long split, ByteOrder byteOrder,
                                 String xmlDictionary)
            throws EvioException {

        this(baseName, directory, runType, runNumber, split,
             DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT, 0,
             byteOrder, xmlDictionary, null, false, false);
    }


    /**
     * Create an <code>EventWriter</code> for writing events to a file.
     * This constructor is useful when splitting and automatically naming
     * the split files. If any of the generated files already exist,
     * it will <b>NOT</b> be overwritten. Byte order defaults to big endian.
     *
     * @param baseName      base file name used to generate complete file name (may not be null)
     * @param directory     directory in which file is to be placed
     * @param runType       name of run type configuration to be used in naming files
     * @param runNumber     number of the CODA run, used in naming files
     * @param split         if &lt; 1, do not split file, write to only one file of unlimited size.
     *                      Else this is max size in bytes to make a file
     *                      before closing it and starting writing another.
     * @param byteOrder     the byte order in which to write the file.
     *                      Defaults to big endian if null.
     * @param xmlDictionary dictionary in xml format or null if none.
     * @param streamId      streamId number (100 &gt; id &gt; -1) for file name
     *
     * @throws EvioException if baseName arg is null;
     *                       if file could not be opened, positioned, or written to;
     *                       if file exists.
     */
    public EventWriter(String baseName, String directory, String runType,
                                 int runNumber, long split, ByteOrder byteOrder,
                                 String xmlDictionary, int streamId)
            throws EvioException {

        this(baseName, directory, runType, runNumber, split,
             DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT, 0,
             byteOrder, xmlDictionary, null, false,
             false, false, false, null, streamId);
    }


    /**
     * Create an <code>EventWriter</code> for writing events to a file.
     * This constructor is useful when splitting and automatically naming
     * the split files. Byte order defaults to big endian.
     *
     * @param baseName      base file name used to generate complete file name (may not be null)
     * @param directory     directory in which file is to be placed
     * @param runType       name of run type configuration to be used in naming files
     * @param runNumber     number of the CODA run, used in naming files
     * @param split         if &lt; 1, do not split file, write to only one file of unlimited size.
     *                      Else this is max size in bytes to make a file
     *                      before closing it and starting writing another.
     * @param byteOrder     the byte order in which to write the file.
     *                      Defaults to big endian if null.
     * @param xmlDictionary dictionary in xml format or null if none.
     * @param overWriteOK   if <code>false</code> and the file already exists,
     *                      an exception is thrown rather than overwriting it.
     *
     * @throws EvioException if baseName arg is null;
     *                       if file could not be opened, positioned, or written to;
     *                       if file exists.
     */
    public EventWriter(String baseName, String directory, String runType,
                                 int runNumber, long split, ByteOrder byteOrder,
                                 String xmlDictionary, boolean overWriteOK)
            throws EvioException {

        this(baseName, directory, runType, runNumber, split,
                DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT, 0,
                byteOrder, xmlDictionary, null, overWriteOK, false);
    }


    /**
     * Create an <code>EventWriter</code> for writing events to a file.
     * If the file already exists, its contents will be overwritten
     * unless the "overWriteOK" argument is <code>false</code> in
     * which case an exception will be thrown. Unless ..., the option to
     * append these events to an existing file is <code>true</code>,
     * in which case everything is fine. If the file doesn't exist,
     * it will be created. Byte order defaults to big endian if arg is null.
     * File can be split while writing.<p>
     *
     * The base file name may contain up to 2, C-style integer format specifiers using
     * "d" and "x" (such as <b>%03d</b>, or <b>%x</b>).
     * If more than 2 are found, an exception will be thrown.
     * If no "0" precedes any integer between the "%" and the "d" or "x" of the format specifier,
     * it will be added automatically in order to avoid spaces in the file name.
     * The first specifier will be substituted with the given runNumber value.
     * If the file is being split, the second will be substituted with the split number
     * which starts at 0.
     * If 2 specifiers exist and the file is not being split, no substitutions are made.
     * If no specifier for the splitNumber exists, it is tacked onto the end of the file
     * name after a dot (.).
     * <p>
     *
     * The base file name may contain characters of the form <b>$(ENV_VAR)</b>
     * which will be substituted with the value of the associated environmental
     * variable or a blank string if none is found.<p>
     *
     * The base file name may also contain occurrences of the string "%s"
     * which will be substituted with the value of the runType arg or nothing if
     * the runType is null.<p>
     *
     *
     * @param baseName      base file name used to generate complete file name (may not be null)
     * @param directory     directory in which file is to be placed
     * @param runType       name of run type configuration to be used in naming files
     * @param runNumber     number of the CODA run, used in naming files
     * @param split         if &lt; 1, do not split file, write to only one file of unlimited size.
     *                      Else this is max size in bytes to make a file
     *                      before closing it and starting writing another.
     * @param blockSizeMax  the max blocksize to use which must be &gt;= {@link #MIN_BLOCK_SIZE}
     *                      and &lt;= {@link #MAX_BLOCK_SIZE} ints.
     *                      The size of the block will not be larger than this size
     *                      unless a single event itself is larger.
     * @param blockCountMax the max number of events (including dictionary) in a single block
     *                      which must be &gt;= {@link #MIN_BLOCK_COUNT} and &lt;= {@link #MAX_BLOCK_COUNT}.
     * @param bufferSize    number of bytes to make the internal buffer which will
     *                      be storing events before writing them to a file. Must be at least
     *                      4*blockSizeMax + 32. If not, it is set to that.
     * @param byteOrder     the byte order in which to write the file. This is ignored
     *                      if appending to existing file.
     * @param xmlDictionary dictionary in xml format or null if none.
     * @param bitInfo       set of bits to include in first block header.
     * @param overWriteOK   if <code>false</code> and the file already exists,
     *                      an exception is thrown rather than overwriting it.
     * @param append        if <code>true</code> and the file already exists,
     *                      all events to be written will be appended to the
     *                      end of the file.
     *
     * @throws EvioException if blockSizeMax or blockCountMax exceed limits;
     *                       if defined dictionary while appending;
     *                       if splitting file while appending;
     *                       if file name arg is null;
     *                       if file could not be opened, positioned, or written to;
     *                       if file exists but user requested no over-writing or appending.
     */
    public EventWriter(String baseName, String directory, String runType,
                                 int runNumber, long split,
                                 int blockSizeMax, int blockCountMax, int bufferSize,
                                 ByteOrder byteOrder, String xmlDictionary,
                                 BitSet bitInfo, boolean overWriteOK, boolean append)
            throws EvioException {

        this(baseName, directory, runType, runNumber, split,
             blockSizeMax, blockCountMax, bufferSize,
             byteOrder, xmlDictionary, bitInfo, overWriteOK, append, null);
    }


    /**
     * Create an <code>EventWriter</code> for writing events to a file.
     * If the file already exists, its contents will be overwritten
     * unless the "overWriteOK" argument is <code>false</code> in
     * which case an exception will be thrown. Unless ..., the option to
     * append these events to an existing file is <code>true</code>,
     * in which case everything is fine. If the file doesn't exist,
     * it will be created. Byte order defaults to big endian if arg is null.
     * File can be split while writing.<p>
     *
     * The base file name may contain up to 2, C-style integer format specifiers using
     * "d" and "x" (such as <b>%03d</b>, or <b>%x</b>).
     * If more than 2 are found, an exception will be thrown.
     * If no "0" precedes any integer between the "%" and the "d" or "x" of the format specifier,
     * it will be added automatically in order to avoid spaces in the file name.
     * The first specifier will be substituted with the given runNumber value.
     * If the file is being split, the second will be substituted with the split number
     * which starts at 0.
     * If 2 specifiers exist and the file is not being split, no substitutions are made.
     * If no specifier for the splitNumber exists, it is tacked onto the end of the file
     * name after a dot (.).
     * <p>
     *
     * The base file name may contain characters of the form <b>$(ENV_VAR)</b>
     * which will be substituted with the value of the associated environmental
     * variable or a blank string if none is found.<p>
     *
     * The base file name may also contain occurrences of the string "%s"
     * which will be substituted with the value of the runType arg or nothing if
     * the runType is null.<p>
     *
     *
     * @param baseName      base file name used to generate complete file name (may not be null)
     * @param directory     directory in which file is to be placed
     * @param runType       name of run type configuration to be used in naming files
     * @param runNumber     number of the CODA run, used in naming files
     * @param split         if &lt; 1, do not split file, write to only one file of unlimited size.
     *                      Else this is max size in bytes to make a file
     *                      before closing it and starting writing another.
     * @param blockSizeMax  the max blocksize to use which must be &gt;= {@link #MIN_BLOCK_SIZE}
     *                      and &lt;= {@link #MAX_BLOCK_SIZE} ints.
     *                      The size of the block will not be larger than this size
     *                      unless a single event itself is larger.
     * @param blockCountMax the max number of events (including dictionary) in a single block
     *                      which must be &gt;= {@link #MIN_BLOCK_COUNT} and &lt;= {@link #MAX_BLOCK_COUNT}.
     * @param bufferSize    number of bytes to make the internal buffer which will
     *                      be storing events before writing them to a file. Must be at least
     *                      4*blockSizeMax + 32. If not, it is set to that.
     * @param byteOrder     the byte order in which to write the file. This is ignored
     *                      if appending to existing file.
     * @param xmlDictionary dictionary in xml format or null if none.
     * @param bitInfo       set of bits to include in first block header.
     * @param overWriteOK   if <code>false</code> and the file already exists,
     *                      an exception is thrown rather than overwriting it.
     * @param append        if <code>true</code> and the file already exists,
     *                      all events to be written will be appended to the
     *                      end of the file.
     * @param firstEvent    the first event written into each file (after any dictionary)
     *                      including all split files; may be null. Useful for adding
     *                      common, static info into each split file.
     *
     * @throws EvioException if blockSizeMax or blockCountMax exceed limits;
     *                       if defined dictionary while appending;
     *                       if splitting file while appending;
     *                       if file name arg is null;
     *                       if file could not be opened, positioned, or written to;
     *                       if file exists but user requested no over-writing or appending.
     */
    public EventWriter(String baseName, String directory, String runType,
                                 int runNumber, long split,
                                 int blockSizeMax, int blockCountMax, int bufferSize,
                                 ByteOrder byteOrder, String xmlDictionary,
                                 BitSet bitInfo, boolean overWriteOK, boolean append,
                                 EvioBank firstEvent)
            throws EvioException {

        this(baseName, directory, runType, runNumber, split,
             blockSizeMax, blockCountMax, bufferSize,
             byteOrder, xmlDictionary, bitInfo, overWriteOK, append,
             false, false, firstEvent, 0);
    }


    /**
     * Create an <code>EventWriter</code> for writing events to a file.
     * If the file already exists, its contents will be overwritten
     * unless the "overWriteOK" argument is <code>false</code> in
     * which case an exception will be thrown. Unless ..., the option to
     * append these events to an existing file is <code>true</code>,
     * in which case everything is fine. If the file doesn't exist,
     * it will be created. Byte order defaults to big endian if arg is null.
     * File can be split while writing.<p>
     *
     * The base file name may contain up to 2, C-style integer format specifiers using
     * "d" and "x" (such as <b>%03d</b>, or <b>%x</b>).
     * If more than 2 are found, an exception will be thrown.
     * If no "0" precedes any integer between the "%" and the "d" or "x" of the format specifier,
     * it will be added automatically in order to avoid spaces in the file name.
     * The first specifier will be substituted with the given runNumber value.
     * If the file is being split, the second will be substituted with the split number
     * which starts at 0.
     * If 2 specifiers exist and the file is not being split, no substitutions are made.
     * If no specifier for the splitNumber exists, it is tacked onto the end of the file
     * name after a dot (.).
     * <p>
     *
     * The base file name may contain characters of the form <b>$(ENV_VAR)</b>
     * which will be substituted with the value of the associated environmental
     * variable or a blank string if none is found.<p>
     *
     * The base file name may also contain occurrences of the string "%s"
     * which will be substituted with the value of the runType arg or nothing if
     * the runType is null.<p>
     *
     * If multiple streams of data, each writing a file, end up with the same file name,
     * they can be differentiated by a stream id number. If the id is &gt; 0, the string, ".strm"
     * is appended to the very end of the file followed by the id number (e.g. filename.strm1).
     * This is done after the run type, run number, split numbers, and env vars have been inserted
     * into the file name.<p>
     *
     * @param baseName      base file name used to generate complete file name (may not be null)
     * @param directory     directory in which file is to be placed
     * @param runType       name of run type configuration to be used in naming files
     * @param runNumber     number of the CODA run, used in naming files
     * @param split         if &lt; 1, do not split file, write to only one file of unlimited size.
     *                      Else this is max size in bytes to make a file
     *                      before closing it and starting writing another.
     * @param blockSizeMax  the max blocksize to use which must be &gt;= {@link #MIN_BLOCK_SIZE}
     *                      and &lt;= {@link #MAX_BLOCK_SIZE} ints.
     *                      The size of the block will not be larger than this size
     *                      unless a single event itself is larger.
     * @param blockCountMax the max number of events (including dictionary) in a single block
     *                      which must be &gt;= {@link #MIN_BLOCK_COUNT} and &lt;= {@link #MAX_BLOCK_COUNT}.
     * @param bufferSize    number of bytes to make the internal buffer which will
     *                      be storing events before writing them to a file. Must be at least
     *                      4*blockSizeMax + 32. If not, it is set to that.
     * @param byteOrder     the byte order in which to write the file. This is ignored
     *                      if appending to existing file.
     * @param xmlDictionary dictionary in xml format or null if none.
     * @param bitInfo       set of bits to include in first block header.
     * @param overWriteOK   if <code>false</code> and the file already exists,
     *                      an exception is thrown rather than overwriting it.
     * @param append        if <code>true</code> and the file already exists,
     *                      all events to be written will be appended to the
     *                      end of the file.
     * @param compressedOutput if <code>true</code> the output file will be contain
     *                      data compressed in lz4 format.
     * @param writeBlockEventSizes if <code>true</code> each block header written to a
     *                      file will contain a list of all event sizes in compacted form.
     * @param firstEvent    the first event written into each file (after any dictionary)
     *                      including all split files; may be null. Useful for adding
     *                      common, static info into each split file.
     * @param streamId      streamId number (100 &gt; id &gt; -1) for file name
     *
     * @throws EvioException if blockSizeMax or blockCountMax exceed limits;
     *                       if defined dictionary or first event while appending;
     *                       if splitting file while appending;
     *                       if file name arg is null;
     *                       if file could not be opened, positioned, or written to;
     *                       if file exists but user requested no over-writing or appending.
     */
    public EventWriter(String baseName, String directory, String runType,
                       int runNumber, long split,
                       int blockSizeMax, int blockCountMax, int bufferSize,
                       ByteOrder byteOrder, String xmlDictionary,
                       BitSet bitInfo, boolean overWriteOK, boolean append,
                       boolean compressedOutput, boolean writeBlockEventSizes,
                       EvioBank firstEvent, int streamId)

            throws EvioException {

        if (baseName == null) {
            throw new EvioException("baseName arg is null");
        }

        if (blockSizeMax < MIN_BLOCK_SIZE) {
            throw new EvioException("blockSizeMax arg must be bigger");
        }

        if (blockSizeMax > EventWriter.MAX_BLOCK_SIZE) {
            throw new EvioException("blockSizeMax arg must be smaller");
        }

        if (blockCountMax < EventWriter.MIN_BLOCK_COUNT) {
            throw new EvioException("blockCountMax arg must be bigger");
        }

        if (blockCountMax > EventWriter.MAX_BLOCK_COUNT) {
            throw new EvioException("blockCountMax arg must be smaller");
        }

        if (bufferSize < 4*blockSizeMax + 32) {
            bufferSize = 4*blockSizeMax + 32;
        }

        if (byteOrder == null) {
            byteOrder = ByteOrder.BIG_ENDIAN;
        }

        if (append) {
            if (split > 0) {
                throw new EvioException("Cannot specify split when appending");
            }
            else if ((xmlDictionary != null) || (firstEvent != null)) {
                throw new EvioException("Cannot specify dictionary or first event when appending");
            }
        }

        if (xmlDictionary != null) {
            // 56 is the minimum number of characters for a valid xml dictionary
            if (xmlDictionary.length() < 56) {
                throw new EvioException("Dictionary improper format");
            }

            // Turn dictionary data into bytes
            dictionaryByteArray = BaseStructure.stringsToRawBytes(new String[]{xmlDictionary});
            // Dictionary length in bytes including bank header of 8 bytes
            dictionaryBytes = dictionaryByteArray.length + 8;
            // Common block has dictionary
            commonBlockByteSize = dictionaryBytes;
            commonBlockCount = 1;
        }

        // Store arguments
        this.split                = split;
        this.append               = append;
        this.streamId             = streamId;
        this.runNumber            = runNumber;
        this.byteOrder            = byteOrder;   // byte order may be overwritten if appending
        this.bufferSize           = bufferSize;
        this.overWriteOK          = overWriteOK;
        this.blockSizeMax         = blockSizeMax;
        this.blockCountMax        = blockCountMax;
        this.xmlDictionary        = xmlDictionary;
        this.compressedOutput     = compressedOutput;
        this.writeBlockEventSizes = writeBlockEventSizes;

        toFile = true;
        blockNumber = 1;

        if (bitInfo != null) {
            this.bitInfo = (BitSet)bitInfo.clone();
        }

        // Create internal storage buffer.
        // Since we're doing I/O to a file, a direct buffer is more efficient.
        // Besides, the JVM will convert a non-direct to a direct one anyway
        // when doing the I/O (and copy all that data again).
        if (direct) {
            buffer = ByteBuffer.allocateDirect(bufferSize);
        }
        else {
            buffer = ByteBuffer.allocate(bufferSize);
        }
        buffer.order(byteOrder);

        // Aim for this size block (in bytes)
        targetBlockSize = 4*blockSizeMax;
System.out.println("constructor: split = " + split);

        // Object to close files in a separate thread when splitting, to speed things up
        if (split > 0) fileCloser = new FileCloser();

        // Split file number starts at 0
        splitCount = 0;

        // Make substitutions in the baseName to create the base file name.
        if (directory != null) baseName = directory + "/" + baseName;
        StringBuilder builder = new StringBuilder(100);
        specifierCount = Utilities.generateBaseFileName(baseName, runType, builder);
        baseFileName   = builder.toString();
        // Also create the first file's name with more substitutions
        String fileName = Utilities.generateFileName(baseFileName, specifierCount,
                                                     runNumber, split, splitCount++,
                                                     streamId);
        if (direct) {
            uncompressedDataBuf = ByteBuffer.allocateDirect(bufferSize);
        }
        else {
            uncompressedDataBuf = ByteBuffer.allocate(bufferSize);
        }
        uncompressedDataBuf.order(byteOrder);

        // If compressing w/ lz4 ...
        if (compressedOutput) {
            lz4Comp = LZ4Factory.fastestInstance().fastCompressor();
            //fileName += ".lz4";
        }

        //System.out.println("EventWriter const: filename = " + fileName);
        //System.out.println("                   basename = " + baseName);
        currentFile = new File(fileName);

        // If we can't overwrite or append and file exists, throw exception
        if (!overWriteOK && !append && (currentFile.exists() && currentFile.isFile())) {
            throw new EvioException("File exists but user requested no over-writing or appending, "
                                            + currentFile.getPath());
        }

        try {

            if (append) {
System.out.println("constructor: appending to file, " + fileName);
                raf = new RandomAccessFile(currentFile, "rw");
                fileChannel = raf.getChannel();

                // If we have an empty file, that's OK.
                // Otherwise we have to examine it for compatibility
                // and position ourselves for the first write.
                if (fileChannel.size() > 0) {
                    // Look at first block header to find endianness & version.
                    // Endianness given in constructor arg, when appending, is ignored.
                    examineFirstBlockHeader();

                    // Oops, gotta redo this since file has different byte order
                    // than specified in constructor arg.
                    if (this.byteOrder != byteOrder) {
                        buffer.order(this.byteOrder);
                    }

                    // Prepare for appending by moving file position to end of last block
                    toAppendPosition();

                    // File position is now after the last event written.

                    // Reset the buffer which has been used to read the header
                    // and to prepare the file for event writing.
                    buffer.clear();
                }
            }
            else {
System.out.println("constructor: create non-split file, " + fileName);
                raf = new RandomAccessFile(currentFile, "rw");
                fileChannel = raf.getChannel();
            }
        }
        catch (FileNotFoundException e) {
            throw new EvioException("File could not be opened for writing, " +
                                            currentFile.getPath(), e);
        }
        catch (IOException e) {
            throw new EvioException("File could not be positioned for appending, " +
                                            currentFile.getPath(), e);
        }

        // Convert first event into bytes
        if (firstEvent != null) {
            firstEventBytes = firstEvent.getTotalBytes();
            ByteBuffer firstEventBuf = ByteBuffer.allocate(firstEventBytes);
            firstEventBuf.order(buffer.order());
            firstEvent.write(firstEventBuf);
            firstEventByteArray = firstEventBuf.array();
            commonBlockByteSize += firstEventBytes;
            commonBlockCount++;
            haveFirstEvent = true;
        }

        // Write out the beginning block header
        // (size & count words are updated when writing event)

        if (xmlDictionary == null) {
            writeNewHeader(0,blockNumber++,bitInfo,false,
                           false, haveFirstEvent, writeBlockEventSizes);
        }
        else {
            writeNewHeader(0,blockNumber++,bitInfo,true,
                           false, haveFirstEvent, writeBlockEventSizes);
        }

        // Write out dictionary & firstEvent if any (currentBlockSize updated)
        writeCommonBlock();
    }


    //---------------------------------------------
    // BUFFER Constructors
    //---------------------------------------------


    /**
     * Create an <code>EventWriter</code> for writing events to a ByteBuffer.
     * Uses the default number and size of blocks in buffer.
     * Will overwrite any existing data in buffer!
     *
     * @param buf            the buffer to write to.
     * @throws EvioException if buf arg is null
     */
    public EventWriter(ByteBuffer buf) throws EvioException {

        this(buf, DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT, null, null, 0, false);
    }

    /**
     * Create an <code>EventWriter</code> for writing events to a ByteBuffer.
     * Uses the default number and size of blocks in buffer.
     *
     * @param buf            the buffer to write to.
     * @param append         if <code>true</code>, all events to be written will be
     *                       appended to the end of the buffer.
     * @throws EvioException if buf arg is null
     */
    public EventWriter(ByteBuffer buf, boolean append) throws EvioException {

        this(buf, DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT, null, null, 0, append);
    }

    /**
     * Create an <code>EventWriter</code> for writing events to a ByteBuffer.
     * Uses the default number and size of blocks in buffer.
     *
     * @param buf            the buffer to write to.
     * @param xmlDictionary  dictionary in xml format or null if none.
     * @param append         if <code>true</code>, all events to be written will be
     *                       appended to the end of the buffer.
     * @throws EvioException if buf arg is null
     */
    public EventWriter(ByteBuffer buf, String xmlDictionary, boolean append) throws EvioException {

        this(buf, DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT, xmlDictionary, null, 0, append);
    }

    /**
     * Create an <code>EventWriter</code> for writing events to a ByteBuffer.
     * Will overwrite any existing data in buffer!
     *
     * @param buf            the buffer to write to.
     * @param blockSizeMax   the max blocksize to use which must be &gt;= {@link #MIN_BLOCK_SIZE}
     *                       and &lt;= {@link #MAX_BLOCK_SIZE} ints.
     *                       The size of the block will not be larger than this size
     *                       unless a single event itself is larger.
     * @param blockCountMax  the max number of events (including dictionary) in a single block
     *                       which must be &gt;= {@link #MIN_BLOCK_COUNT} and &lt;= {@link #MAX_BLOCK_COUNT}.
     * @param xmlDictionary  dictionary in xml format or null if none.
     * @param bitInfo        set of bits to include in first block header.
     * @throws EvioException if blockSizeMax or blockCountMax exceed limits; if buf arg is null
     */
    public EventWriter(ByteBuffer buf, int blockSizeMax, int blockCountMax,
                                 String xmlDictionary, BitSet bitInfo) throws EvioException {

        this(buf, blockSizeMax, blockCountMax, xmlDictionary, bitInfo, 0, false);
    }

    /**
     * Create an <code>EventWriter</code> for writing events to a ByteBuffer.
     * Block number starts at 0.
     *
     * @param buf            the buffer to write to.
     * @param blockSizeMax   the max blocksize to use which must be &gt;= {@link #MIN_BLOCK_SIZE}
     *                       and &lt;= {@link #MAX_BLOCK_SIZE} ints.
     *                       The size of the block will not be larger than this size
     *                       unless a single event itself is larger.
     * @param blockCountMax  the max number of events (including dictionary) in a single block
     *                       which must be &gt;= {@link #MIN_BLOCK_COUNT} and &lt;= {@link #MAX_BLOCK_COUNT}.
     * @param xmlDictionary  dictionary in xml format or null if none.
     * @param bitInfo        set of bits to include in first block header.
     * @param append         if <code>true</code>, all events to be written will be
     *                       appended to the end of the buffer.
     *
     * @throws EvioException if blockSizeMax or blockCountMax exceed limits;
     *                       if buf arg is null;
     *                       if defined dictionary while appending;
     */
    public EventWriter(ByteBuffer buf, int blockSizeMax, int blockCountMax,
                                 String xmlDictionary, BitSet bitInfo,
                                 boolean append) throws EvioException {

        this(buf, blockSizeMax, blockCountMax, xmlDictionary, bitInfo, 0, append);
    }

    /**
     * Create an <code>EventWriter</code> for writing events to a ByteBuffer.
     * Will overwrite any existing data in buffer!
     *
     * @param buf            the buffer to write to.
     * @param blockSizeMax   the max blocksize to use which must be &gt;= {@link #MIN_BLOCK_SIZE}
     *                       and &lt;= {@link #MAX_BLOCK_SIZE} ints.
     *                       The size of the block will not be larger than this size
     *                       unless a single event itself is larger.
     * @param blockCountMax  the max number of events (including dictionary) in a single block
     *                       which must be &gt;= {@link #MIN_BLOCK_COUNT} and &lt;= {@link #MAX_BLOCK_COUNT}.
     * @param xmlDictionary  dictionary in xml format or null if none.
     * @param bitInfo        set of bits to include in first block header.
     * @param reserved1      set the value of the first "reserved" int in first block header.
     *                       NOTE: only CODA (i.e. EMU) software should use this.
     * @param blockNumber    number at which to start block number counting.
     * @throws EvioException if blockSizeMax or blockCountMax exceed limits; if buf arg is null
     */
    public EventWriter(ByteBuffer buf, int blockSizeMax, int blockCountMax,
                                 String xmlDictionary, BitSet bitInfo, int reserved1,
                                 int blockNumber) throws EvioException {

        initializeBuffer(buf, blockSizeMax, blockCountMax, xmlDictionary,
                         bitInfo, reserved1, blockNumber, false, null);
    }

    /**
     * Create an <code>EventWriter</code> for writing events to a ByteBuffer.
     * Block number starts at 0.
     *
     * @param buf            the buffer to write to.
     * @param blockSizeMax   the max blocksize to use which must be &gt;= {@link #MIN_BLOCK_SIZE}
     *                       and &lt;= {@link #MAX_BLOCK_SIZE} ints.
     *                       The size of the block will not be larger than this size
     *                       unless a single event itself is larger.
     * @param blockCountMax  the max number of events (including dictionary) in a single block
     *                       which must be &gt;= {@link #MIN_BLOCK_COUNT} and &lt;= {@link #MAX_BLOCK_COUNT}.
     * @param xmlDictionary  dictionary in xml format or null if none.
     * @param bitInfo        set of bits to include in first block header.
     * @param reserved1      set the value of the first "reserved" int in first block header.
     *                       NOTE: only CODA (i.e. EMU) software should use this.
     * @param append         if <code>true</code>, all events to be written will be
     *                       appended to the end of the buffer.
     *
     * @throws EvioException if blockSizeMax or blockCountMax exceed limits;
     *                       if buf arg is null;
     *                       if defined dictionary while appending;
     */
    public EventWriter(ByteBuffer buf, int blockSizeMax, int blockCountMax,
                                 String xmlDictionary, BitSet bitInfo, int reserved1,
                                 boolean append) throws EvioException {

        initializeBuffer(buf, blockSizeMax, blockCountMax,
                         xmlDictionary, bitInfo, reserved1, 1, append, null);
    }

    /**
     * Create an <code>EventWriter</code> for writing events to a ByteBuffer.
     *
     * @param buf            the buffer to write to.
     * @param blockSizeMax   the max blocksize to use which must be &gt;= {@link #MIN_BLOCK_SIZE}
     *                       and &lt;= {@link #MAX_BLOCK_SIZE} ints.
     *                       The size of the block will not be larger than this size
     *                       unless a single event itself is larger.
     * @param blockCountMax  the max number of events (including dictionary) in a single block
     *                       which must be &gt;= {@link #MIN_BLOCK_COUNT} and &lt;= {@link #MAX_BLOCK_COUNT}.
     * @param xmlDictionary  dictionary in xml format or null if none.
     * @param bitInfo        set of bits to include in first block header.
     * @param reserved1      set the value of the first "reserved" int in first block header.
     *                       NOTE: only CODA (i.e. EMU) software should use this.
     * @param blockNumber    number at which to start block number counting.
     * @param append         if <code>true</code>, all events to be written will be
     *                       appended to the end of the buffer.
     * @param firstEvent     the first event written into the buffer (after any dictionary).
     *                       May be null. Not useful when writing to a buffer as this
     *                       event may be written using normal means.
     *
     * @throws EvioException if blockSizeMax or blockCountMax exceed limits;
     *                       if buf arg is null;
     *                       if defined dictionary while appending;
     */
    public EventWriter(ByteBuffer buf, int blockSizeMax, int blockCountMax,
                                 String xmlDictionary, BitSet bitInfo, int reserved1,
                                 int blockNumber, boolean append, EvioBank firstEvent)
            throws EvioException {

        initializeBuffer(buf, blockSizeMax, blockCountMax,
                         xmlDictionary, bitInfo, reserved1,
                         blockNumber, append, firstEvent);
    }

    /**
     * Encapsulate constructor initialization for buffers.
     * The buffer's position is set to 0 before writing.
     *
     * @param buf            the buffer to write to.
     * @param blockSizeMax   the max blocksize to use which must be &gt;= {@link #MIN_BLOCK_SIZE}
     *                       and &lt;= {@link #MAX_BLOCK_SIZE} ints.
     *                       The size of the block will not be larger than this size
     *                       unless a single event itself is larger.
     * @param blockCountMax  the max number of events (including dictionary) in a single block
     *                       which must be &gt;= {@link #MIN_BLOCK_COUNT} and &lt;= {@link #MAX_BLOCK_COUNT}.
     * @param xmlDictionary  dictionary in xml format or null if none.
     * @param bitInfo        set of bits to include in first block header.
     * @param reserved1      set the value of the first "reserved" int in first block header.
     *                       NOTE: only CODA (i.e. EMU) software should use this.
     * @param blockNumber    number at which to start block number counting.
     * @param append         if <code>true</code>, all events to be written will be
     *                       appended to the end of the buffer.
     * @param firstEvent     the first event written into the buffer (after any dictionary).
     *                       May be null. Not useful when writing to a buffer as this
     *                       event may be written using normal means.
     *
     * @throws EvioException if blockSizeMax or blockCountMax exceed limits;
     *                       if buf arg is null;
     *                       if defined dictionary while appending;
     */
    private void initializeBuffer(ByteBuffer buf, int blockSizeMax, int blockCountMax,
                                  String xmlDictionary, BitSet bitInfo, int reserved1,
                                  int blockNumber, boolean append, EvioBank firstEvent)
            throws EvioException {

        if (blockSizeMax < MIN_BLOCK_SIZE) {
            throw new EvioException("Max block size arg (" + blockSizeMax + ") must be >= " +
                                     MIN_BLOCK_SIZE);
        }

        if (blockSizeMax > MAX_BLOCK_SIZE) {
            throw new EvioException("Max block size arg (" + blockSizeMax + ") must be <= " +
                                     MAX_BLOCK_SIZE);
        }

        if (blockCountMax < MIN_BLOCK_COUNT) {
            throw new EvioException("Max block count arg (" + blockCountMax + ") must be >= " +
                                     MIN_BLOCK_COUNT);
        }

        if (blockCountMax > MAX_BLOCK_COUNT) {
            throw new EvioException("Max block count arg (" + blockCountMax + ") must be <= " +
                                     MAX_BLOCK_COUNT);
        }

        if (buf == null) {
            throw new EvioException("Buffer arg cannot be null");
        }

        if (append && ((xmlDictionary != null) || (firstEvent != null))) {
            throw new EvioException("Cannot specify dictionary or first event when appending");
        }

        if (xmlDictionary != null) {
            // 56 is the minimum number of characters for a valid xml dictionary
            if (xmlDictionary.length() < 56) {
                throw new EvioException("Dictionary improper format");
            }

            // Turn dictionary data into bytes
            dictionaryByteArray = BaseStructure.stringsToRawBytes(new String[] {xmlDictionary});
            // Dictionary length in bytes including bank header of 8 bytes
            dictionaryBytes = dictionaryByteArray.length + 8;
            // Common block has dictionary
            commonBlockByteSize = dictionaryBytes;
            commonBlockCount = 1;
        }

        this.append        = append;
        this.buffer        = buf;
        this.byteOrder     = buf.order();
        this.reserved1     = reserved1;
        this.blockNumber   = blockNumber;
        this.blockSizeMax  = blockSizeMax;
        this.blockCountMax = blockCountMax;
        this.xmlDictionary = xmlDictionary;

        // Init variables
        split  = 0L;
        toFile = false;
        closed = false;
        eventsWrittenTotal = 0;
        eventsWrittenToBuffer = 0;
        bytesWrittenToBuffer = 0;

        // Get buffer ready for writing. If we're appending, setting
        // the position to 0 lets us read up to the end of the evio
        // data and find the proper place to append to.
        buffer.position(0);
        bufferSize = buf.capacity();

//        // Write data to separate buffer initially since when
//        // writing to a file it's useful for compression and for
//        // storing events lengths in the block header
//        if (direct) {
//            uncompressedDataBuf = ByteBuffer.allocateDirect(bufferSize);
//        }
//        else {
//            uncompressedDataBuf = ByteBuffer.allocate(bufferSize);
//        }
//        uncompressedDataBuf.order(byteOrder);

        // Aim for this size block (in bytes)
        targetBlockSize = 4*blockSizeMax;

        if (bitInfo != null) {
            this.bitInfo = (BitSet)bitInfo.clone();
        }

        try {
            if (append) {
                // Check endianness & version
                examineFirstBlockHeader();

                // Prepare for appending by moving buffer position
                toAppendPosition();

                // Buffer position is just before empty last block header
            }
        }
        catch (IOException e) {
            throw new EvioException("Buffer could not be positioned for appending", e);
        }

        // Convert first event into bytes
        if (firstEvent != null) {
            firstEventBytes = firstEvent.getTotalBytes();
            ByteBuffer firstEventBuf = ByteBuffer.allocate(firstEventBytes);
            firstEventBuf.order(buffer.order());
            firstEvent.write(firstEventBuf);
            firstEventByteArray = firstEventBuf.array();
            commonBlockByteSize += firstEventBytes;
            commonBlockCount++;
            haveFirstEvent = true;
        }

        // Write first block header into buffer
        // (size & count words are updated when writing events).
        // currentHeaderPosition is set in writeNewHeader() below.
        if (xmlDictionary == null) {
            writeNewHeader(0,this.blockNumber++,bitInfo,false,
                           false, haveFirstEvent, false);
        }
        else {
            writeNewHeader(0,this.blockNumber++,bitInfo,true,
                           false, haveFirstEvent, false);
        }

        // Write out any dictionary & firstEvent (currentBlockSize updated)
        writeCommonBlock();
    }


    /**
     * Initialization new buffer (not from constructor).
     * The buffer's position is set to 0 before writing.
     *
     * @param buf            the buffer to write to.
     * @param bitInfo        set of bits to include in first block header.
     * @param blockNumber    number at which to start block number counting.
     *
     * @throws EvioException not enough memory in buf for writing.
     */
    private void reInitializeBuffer(ByteBuffer buf, BitSet bitInfo, int blockNumber)
            throws EvioException {

        this.buffer      = buf;
        this.byteOrder   = buf.order();
        this.blockNumber = blockNumber;

        // Init variables
        split  = 0L;
        toFile = false;
        closed = false;
        eventsWrittenTotal = 0;
        eventsWrittenToBuffer = 0;
        bytesWrittenToBuffer = 0;

        // Get buffer ready for writing
        buffer.position(0);
        bufferSize = buf.capacity();

        if (bitInfo != null) {
            this.bitInfo = (BitSet)bitInfo.clone();
        }

        // Write first block header into buffer
        // (size & count words are updated when writing events).
        // currentHeaderPosition is set in writeNewHeader() below.
        if (xmlDictionary == null) {
            writeNewHeader(0,this.blockNumber++,bitInfo,false,
                           false, haveFirstEvent, false);
        }
        else {
            writeNewHeader(0,this.blockNumber++,bitInfo,true,
                           false, haveFirstEvent, false);
        }

        // Write out any dictionary & firstEvent (currentBlockSize updated)
        writeCommonBlock();
    }


    /**
     * If writing to a buffer, get the number of bytes written to it
     * including the ending header.
     * @return number of bytes written to buffer
     */
    public long getBytesWrittenToBuffer() {
        return bytesWrittenToBuffer;
    }


    /**
     * Set the buffer being written into (initially set in constructor).
     * This method allows the user to avoid having to create a new EventWriter
     * each time a bank needs to be written to a different buffer.
     * This does nothing if writing to a file. Not for use if appending.<p>
     * Do <b>not</b> use this method unless you know what you are doing.
     *
     * @param buf the buffer to write to.
     * @param bitInfo        set of bits to include in first block header.
     * @param blockNumber    number at which to start block number counting.
     * @throws EvioException if this object was not closed prior to resetting the buffer,
     *                       buffer arg is null, or in appending mode.
     */
    public void setBuffer(ByteBuffer buf, BitSet bitInfo, int blockNumber) throws EvioException {
        if (toFile) return;
        if (buf == null) {
            throw new EvioException("Buffer arg null");
        }
        if (append) {
            throw new EvioException("Method not for use if appending");
        }
        if (!closed) {
            throw new EvioException("Close EventWriter before changing buffers");
        }
        this.bitInfo = bitInfo;

        reInitializeBuffer(buf, bitInfo, blockNumber);
    }


    /**
     * Set the buffer being written into (initially set in constructor).
     * This method allows the user to avoid having to create a new EventWriter
     * each time a bank needs to be written to a different buffer.
     * This does nothing if writing to a file. Not for use if appending.<p>
     * Do <b>not</b> use this method unless you know what you are doing.
     *
     * @param buf the buffer to write to.
     * @throws EvioException if this object was not closed prior to resetting the buffer,
     *                       buffer arg is null, or in appending mode.
     */
    public void setBuffer(ByteBuffer buf) throws EvioException {
        if (toFile) return;
        if (buf == null) {
            throw new EvioException("Buffer arg null");
        }
        if (append) {
            throw new EvioException("Method not for use if appending");
        }
        if (!closed) {
            throw new EvioException("Close EventWriter before changing buffers");
        }

        reInitializeBuffer(buf, bitInfo, 1);
    }


    /**
     * Get the buffer being written into.
     * If writing to a buffer, this was initially supplied by user in constructor.
     * If writing to a file, this is the internal buffer.
     * Although this method may seems useful, it requires a detailed knowledge of
     * this class's internals. The {@link #getByteBuffer()} method is much more
     * useful to the user.
     *
     * @return buffer being written into
     */
    private ByteBuffer getBuffer() {return buffer;}


    /**
     * If writing to a file, return null.
     * If writing to a buffer, get a duplicate of the user-given buffer
     * being written into. The buffer's position will be 0 and its
     * limit will be the size of the valid data. Basically, it's
     * ready to be read from. The returned buffer shares data with the
     * original buffer but has separate limit, position, and mark.
     * Useful if trying to send buffer over the network.
     *
     * @return buffer being written into, made ready for reading;
     *         null if writing to file
     */
    public ByteBuffer getByteBuffer() {
        // It does NOT make sense to give the caller the internal buffer
        // used in writing to files. That buffer may contain nothing and
        // most probably won't contain the full file contents.
        if (toFile()) return null;

        // We synchronize here so we do not write/close in the middle
        // of our messing with the buffer.
        ByteBuffer buf;
        synchronized (this) {
            buf = buffer.duplicate().order(buffer.order());
        }

        // Get buffer ready for reading
        buf.flip();

        return buf;
    }


    /**
     * Is this object writing to file?
     * @return {@code true} if writing to file, else {@code false}.
     */
    public boolean toFile() {return toFile;}


    /**
     * Has {@link #close()} been called (without reopening by calling
     * {@link #setBuffer(ByteBuffer)}) ?
     *
     * @return {@code true} if this object closed, else {@code false}.
     */
    synchronized public boolean isClosed() {return closed;}


    /**
     * Get the name of the current file being written to.
     * Returns null if no file.
     * @return the name of the current file being written to.
     */
    public String getCurrentFilename() {
        if (currentFile != null) {
            return currentFile.getName();
        }
        return null;
    }


    /**
     * Get the full name or path of the current file being written to.
     * Returns null if no file.
     * @return the full name or path of the current file being written to.
     */
    public String getCurrentFilePath() {
        if (currentFile != null) {
            return currentFile.getPath();
        }
        return null;
    }


    /**
     * Get the current split count which is the number of files
     * created by this object. Warning, this value may be changing.
     * @return the current split count which is the number of files created by this object.
     */
    public int getSplitCount() {return splitCount;}


    /**
     * Get the current block number.
     * Warning, this value may be changing.
     * @return the current block number.
     */
    public int getBlockNumber() {return blockNumber;}


    /**
     * Get the number of events written to a file/buffer.
     * Remember that a particular event may not yet be
     * flushed to the file/buffer.
     * If the file being written to is split, the returned
     * value refers to all split files taken together.
     *
     * @return number of events written to a file/buffer.
     */
    public int getEventsWritten() {return eventsWrittenTotal;}


    /**
     * Get the byte order of the buffer/file being written into.
     * @return byte order of the buffer/file being written into.
     */
    public ByteOrder getByteOrder() {return byteOrder;}


    /**
     * Set the number with which to start block numbers.
     * This method does nothing if events have already been written.
     * @param startingBlockNumber  the number with which to start block numbers.
     */
    public void setStartingBlockNumber(int startingBlockNumber) {
        // If events have been written already, forget about it
        if (eventsWrittenTotal > 0) return;
        blockNumber = startingBlockNumber;
    }


    /**
     * Set an event which will be written to the file/buffer as
     * well as to all split files. It's called the "first event" as it will be the
     * first event written to each split file (after the dictionary) if this method
     * is called early enough or the first event was defined in the constructor.<p>
     *
     * Since this method is always called after the constructor, the common block will
     * have already been written with a dictionary and firstEvent if either was
     * defined in the constructor. The event given here will be written
     * immediately somewhere in the body of the file, with the forth-coming split
     * files having that event in the first block along with any dictionary.<p>
     *
     * This means that if the firstEvent is given in the constructor, then the
     * caller may end up with 2 copies of it in a single file (if this method
     * is called once). It's also possible to get 2 copies in a file if this
     * method is called immediately prior to the file splitting.<p>
     *
     * By its nature this method is not useful for writing to a buffer since
     * it is never split and the event can be written to it as any other.<p>
     *
     * The method {@link #writeEvent} calls writeCommonBlock() which, in turn,
     * only gets called when synchronized. So synchronizing this method will
     * make sure firstEvent only gets set while nothing is being written.
     *
     * @param node node representing event to be placed first in each file written
     *             including all splits. If null, no more first events are written
     *             to any files.
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if bad bankBuffer format;
     *                       if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    synchronized public void setFirstEvent(EvioNode node)
            throws EvioException, IOException {

        // If getting rid of the first event ...
        if (node == null) {
            if (xmlDictionary != null) {
                commonBlockCount = 1;
                commonBlockByteSize = dictionaryBytes;
            }
            else {
                commonBlockCount = 0;
                commonBlockByteSize = 0;
            }
            firstEventBytes = 0;
            firstEventByteArray = null;
            haveFirstEvent = false;
            return;
        }

        // Find the first event's bytes and the memory size needed
        // to contain the common events (dictionary + first event).
        if (xmlDictionary != null) {
            commonBlockCount = 1;
            commonBlockByteSize = dictionaryBytes;
        }
        else {
            commonBlockCount = 0;
            commonBlockByteSize = 0;
        }

        ByteBuffer firstEventBuf = node.getStructureBuffer(true);
        firstEventBytes = node.getTotalBytes();
        firstEventByteArray = firstEventBuf.array();
        commonBlockByteSize += firstEventBytes;
        commonBlockCount++;
        haveFirstEvent = true;

        // Write it to the file/buffer now. In this case it may not be the
        // first event written and some splits may not even have it
        // (depending on how many events have been written so far).
        writeEvent(null, firstEventBuf, false);
    }


    /**
     * Set an event which will be written to the file/buffer as
     * well as to all split files. It's called the "first event" as it will be the
     * first event written to each split file (after the dictionary) if this method
     * is called early enough or the first event was defined in the constructor.<p>
     *
     * Since this method is always called after the constructor, the common block will
     * have already been written with a dictionary and firstEvent if either was
     * defined in the constructor. The event given here will be written
     * immediately somewhere in the body of the file, with the forth-coming split
     * files having that event in the first block along with any dictionary.<p>
     *
     * This means that if the firstEvent is given in the constructor, then the
     * caller may end up with 2 copies of it in a single file (if this method
     * is called once). It's also possible to get 2 copies in a file if this
     * method is called immediately prior to the file splitting.<p>
     *
     * By its nature this method is not useful for writing to a buffer since
     * it is never split and the event can be written to it as any other.<p>
     *
     * The method {@link #writeEvent} calls writeCommonBlock() which, in turn,
     * only gets called when synchronized. So synchronizing this method will
     * make sure firstEvent only gets set while nothing is being written.
     *
     * @param bank event to be placed first in each file written including all splits.
     *             If null, no more first events are written to any files.
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if bad bankBuffer format;
     *                       if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    synchronized public void setFirstEvent(EvioBank bank)
            throws EvioException, IOException {

        // If getting rid of the first event ...
        if (bank == null) {
            if (xmlDictionary != null) {
                commonBlockCount = 1;
                commonBlockByteSize = dictionaryBytes;
            }
            else {
                commonBlockCount = 0;
                commonBlockByteSize = 0;
            }
            firstEventBytes = 0;
            firstEventByteArray = null;
            haveFirstEvent = false;
            return;
        }

        // Find the first event's bytes and the memory size needed
        // to contain the common events (dictionary + first event).
        if (xmlDictionary != null) {
            commonBlockCount = 1;
            commonBlockByteSize = dictionaryBytes;
        }
        else {
            commonBlockCount = 0;
            commonBlockByteSize = 0;
        }

        firstEventBytes = bank.getTotalBytes();
        ByteBuffer firstEventBuf = ByteBuffer.allocate(firstEventBytes);
        firstEventBuf.order(buffer.order());
        bank.write(firstEventBuf);
        firstEventBuf.flip();
        firstEventByteArray = firstEventBuf.array();
        commonBlockByteSize += firstEventBytes;
        commonBlockCount++;
        haveFirstEvent = true;

        // Write it to the file/buffer now. In this case it may not be the
        // first event written and some splits may not even have it
        // (depending on how many events have been written so far).
        writeEvent(null, firstEventBuf, false);
    }


    /**
     * This method flushes any remaining internally buffered data to the physical disk.
     * Calling {@link #close()} automatically does this so it isn't necessary
     * to call before closing. This method should only be used when writing
     * events at such a low rate that it takes an inordinate amount of time
     * for internally buffered data to be written to the file.<p>
     *
     *  <b>Calling this can kill performance.</b>
     */
    synchronized public void flush() {
        // If lastEmptyBlockHeaderExists is true, then resetBuffer
        // has been called and no events have been written into buffer yet.
        // In other words, no need to flush an empty, last block header.
        // That is only done in close().
        if (closed || !toFile || lastEmptyBlockHeaderExists) {
            return;
        }

        // Write any remaining data
        try {
            // This will kill performance!
            if (flushToFile(true)) {
                // If we actually wrote some data, start a new block.
                resetBuffer(false);
            }
        }
        catch (EvioException e) {}
        catch (IOException e)   {}
    }



    /** This method flushes any remaining data to file and disables this object. */
    synchronized public void close() {
        if (closed) {
            return;
        }

        // Write any remaining data
        try {
            if (toFile) {
                // We need to end the file with an empty block header.
                // However, if resetBuffer (or flush) was just called,
                // a last block header will already exist.
                if (eventsWrittenToBuffer > 0 || bytesWrittenToBuffer < 1) {
//System.out.println("close(): write header, free bytes In Buffer = " + (bufferSize - bytesWrittenToBuffer));
                    writeNewHeader(0, blockNumber, null,
                                   false, true, false,
                                   false);
                }
                flushToFile(true);
            }
            else {
                // Data is written, but need to write empty last header
                writeNewHeader(0, blockNumber, null,
                               false, true, false,
                               false);
            }
        }
        catch (EvioException e) {}
        catch (IOException e)   {
            e.printStackTrace();
        }

        // Close everything
        try {
            if (toFile) {
                // Close current & next files
                if (raf != null) raf.close();

                // Close split file handler thread pool
                if (fileCloser != null) fileCloser.close();
            }
        }
        catch (IOException e) {}

        closed = true;
    }


    /**
     * Reads part of the first block (physical record) header in order to determine
     * the evio version # and endianness of the file or buffer in question. These things
     * do <b>not</b> need to be examined in subsequent block headers.
     *
     * @return status of read attempt
     * @throws IOException   if error reading file
     * @throws EvioException if not in append mode, bad file format
     */
    protected synchronized IOStatus examineFirstBlockHeader()
            throws IOException, EvioException {

        // Only for append mode
        if (!append) {
            // Internal logic error, should never have gotten here
            throw new EvioException("need to be in append mode");
        }

        int nBytes, currentPosition;

        if (toFile) {
            buffer.clear();
            buffer.limit(32);

//System.out.println("Internal buffer capacity = " + buffer.capacity() + ", pos = " + buffer.position());
            // This read advances fileChannel position
            nBytes = fileChannel.read(buffer);

            // Check to see if we read the whole header
            if (nBytes != 32) {
                throw new EvioException("bad file format");
            }
            currentPosition = 0;
            fileChannel.position(0);
        }
        else {
            // Have enough remaining bytes to read?
            if (buffer.remaining() < 32) {
                return IOStatus.END_OF_FILE;
            }
            currentPosition = buffer.position();
        }

        try {
            // Set the byte order to match the buffer/file's ordering.

            // Check the magic number for endianness. This requires
            // peeking ahead 7 ints or 28 bytes. Set the endianness
            // once we figure out what it is (buffer defaults to big endian).
            byteOrder = buffer.order();
            int magicNumber = buffer.getInt(currentPosition + MAGIC_OFFSET);
//System.out.println("ERROR: magic # = " + Integer.toHexString(magicNumber));

            if (magicNumber != IBlockHeader.MAGIC_NUMBER) {
                if (byteOrder == ByteOrder.BIG_ENDIAN) {
                    byteOrder = ByteOrder.LITTLE_ENDIAN;
                }
                else {
                    byteOrder = ByteOrder.BIG_ENDIAN;
                }
                buffer.order(byteOrder);

                // Reread magic number to make sure things are OK
                magicNumber = buffer.getInt(currentPosition + MAGIC_OFFSET);
//System.out.println("Re read magic # = " + Integer.toHexString(magicNumber));
                if (magicNumber != IBlockHeader.MAGIC_NUMBER) {
System.out.println("ERROR: reread magic # (" + magicNumber + ") & still not right");
                    return IOStatus.EVIO_EXCEPTION;
                }
            }

            // Check the version number
            int bitInfo = buffer.getInt(currentPosition + BIT_INFO_OFFSET);
            int evioVersion = bitInfo & VERSION_MASK;
            if (evioVersion < 4)  {
System.out.println("ERROR: evio version# = " + evioVersion);
                return IOStatus.EVIO_EXCEPTION;
            }

            // Is there a dictionary?
            hasAppendDictionary = BlockHeaderV4.hasDictionary(bitInfo);


//            int blockLen   = buffer.getInt(currentPosition + BLOCK_LENGTH_OFFSET);
//            int headerLen  = buffer.getInt(currentPosition + HEADER_LENGTH_OFFSET);
//            int eventCount = buffer.getInt(currentPosition + EVENT_COUNT_OFFSET);
//            int blockNum   = buffer.getInt(currentPosition + BLOCK_NUMBER_OFFSET);
//
//            boolean lastBlock = BlockHeaderV4.isLastBlock(bitInfo);
//
//            System.out.println("blockLength     = " + blockLen);
//            System.out.println("blockNumber     = " + blockNum);
//            System.out.println("headerLength    = " + headerLen);
//            System.out.println("blockEventCount = " + eventCount);
//            System.out.println("lastBlock       = " + lastBlock);
//            System.out.println("bit info        = " + Integer.toHexString(bitInfo));
//            System.out.println();

        }
        catch (BufferUnderflowException a) {
System.err.println("ERROR endOfBuffer " + a);
            return IOStatus.UNKNOWN_ERROR;
        }

        return IOStatus.SUCCESS;
    }


    /**
     * This method positions a file or buffer for the first {@link #writeEvent(EvioBank)}
     * in append mode. It places the writing position after the last event (not block header).
     *
     * @throws IOException     if file reading/writing problems
     * @throws EvioException   if bad file/buffer format; if not in append mode
     */
    private void toAppendPosition() throws EvioException, IOException {

        // Only for append mode
        if (!append) {
            // Internal logic error, should never have gotten here
            throw new EvioException("need to be in append mode");
        }

        boolean lastBlock, readEOF = false;
//        int blockNum;
        int blockLength, blockEventCount;
        int nBytes, bitInfo, headerLength, currentPosition;
        long bytesLeftInFile=0L;

        if (toFile) {
            bytesLeftInFile = fileChannel.size();
        }

        // The file's block #s may be fine or they may be messed up.
        // Assume they start with one and increment from there. That
        // way any additional blocks now added to the file will have a
        // reasonable # instead of incrementing from the last existing
        // block.
        blockNumber = 1;

        while (true) {
            nBytes = 0;

            // Read in 8 ints (32 bytes) of block header
            if (toFile) {
                buffer.clear();
                buffer.limit(32);
//System.out.println("toAppendPosition: (before read) file pos = " + fileChannel.position());
                while (nBytes < 32) {
                    // This read advances fileChannel position
                    int partial = fileChannel.read(buffer);
                    // If EOF ...
                    if (partial < 0) {
                        if (nBytes != 0) {
                            throw new EvioException("bad buffer format");
                        }
                        // Missing last empty block header
                        readEOF = true;
                        break;
                    }
                    nBytes += partial;
                    bytesLeftInFile -= partial;
                }

                // If we did not read correct # of bytes or didn't run into EOF right away
                if (nBytes != 0 && nBytes != 32) {
                    throw new EvioException("internal file reading error");
                }
                currentPosition = 0;
            }
            else {
//System.out.println("toAppendPosition: pos = " + buffer.position() +
//                                           ", limit = " + buffer.limit());
                // Have enough remaining bytes to read?
                if (buffer.remaining() < 32) {
                    throw new EvioException("bad buffer format");
                }
                currentPosition = buffer.position();
            }

            bitInfo         = buffer.getInt(currentPosition + BIT_INFO_OFFSET);
            blockLength     = buffer.getInt(currentPosition + BLOCK_LENGTH_OFFSET);
//            blockNum        = buffer.getInt(currentPosition + BLOCK_NUMBER_OFFSET);
            headerLength    = buffer.getInt(currentPosition + HEADER_LENGTH_OFFSET);
            blockEventCount = buffer.getInt(currentPosition + EVENT_COUNT_OFFSET);
            lastBlock       = BlockHeaderV4.isLastBlock(bitInfo);

//            System.out.println("bitInfo         = 0x" + Integer.toHexString(bitInfo));
//            System.out.println("blockLength     = " + blockLength);
//            System.out.println("blockNumber     = " + blockNum);
//            System.out.println("headerLength    = " + headerLength);
//            System.out.println("blockEventCount = " + blockEventCount);
//            System.out.println("lastBlock       = " + lastBlock);
//            System.out.println();

            // Track total number of events in file/buffer (minus dictionary)
            eventsWrittenTotal += blockEventCount;

            blockNumber++;

            // Stop at the last block. The file may not have a last block if
            // improperly terminated. Running into an End-Of-File will flag
            // this condition.
            if (lastBlock || readEOF) {
                break;
            }

            // Hop to next block header
            if (toFile) {
                int bytesToNextBlockHeader = 4*blockLength - 32;
                if (bytesLeftInFile < bytesToNextBlockHeader) {
                    throw new EvioException("bad file format");
                }
                fileChannel.position(fileChannel.position() + bytesToNextBlockHeader);
                bytesLeftInFile -=  bytesToNextBlockHeader;
            }
            else {
                // Is there enough buffer space to hop over block?
                if (buffer.remaining() < 4*blockLength) {
                    throw new EvioException("bad buffer format");
                }

                buffer.position(buffer.position() + 4*blockLength);
            }
        }

        if (hasAppendDictionary) {
            eventsWrittenToFile = eventsWrittenToBuffer = eventsWrittenTotal + 1;
        }
        else {
            eventsWrittenToFile = eventsWrittenToBuffer = eventsWrittenTotal;
        }

        //-------------------------------------------------------------------------------
        // If we're here, we've just read the last block header (at least 8 words of it).
        // File position is just past header, but buffer position is just before it.
        // Either that or we ran into end of file (last block header missing).
        //
        // If EOF, last block header missing, we're good.
        //
        // Else check to see if the last block contains data. If it does,
        // change a single bit so it's not labeled as the last block,
        // then jump past all data.
        //
        // Else if there is no data, position file before it as preparation for writing
        // the next block.
        //-------------------------------------------------------------------------------

        // If no last, empty block header in file ...
        if (readEOF) {
            // It turns out we need to do nothing. The constructor that
            // calls this method will write out the next block header.
            blockNumber--;
        }
        // If last block has event(s) in it ...
        else if (blockLength > headerLength) {
            // Clear last block bit in 6th header word
            bitInfo = BlockHeaderV4.clearLastBlockBit(bitInfo);

            // Rewrite header word with new bit info & hop over block

            // File now positioned right after the last header to be read
            if (toFile) {
                // Back up to before 6th block header word
                fileChannel.position(fileChannel.position() - (32 - BIT_INFO_OFFSET));
//System.out.println("toAppendPosition: writing over last block's 6th word, back up %d words" +(8 - 6));

                // Write over 6th block header word
                buffer.clear();
                buffer.putInt(bitInfo);
                buffer.flip();
                while (buffer.hasRemaining()) {
                    fileChannel.write(buffer);
                }

                // Hop over the entire block
//System.out.println("toAppendPosition: wrote over last block's 6th word, hop over %d words" +
// (blockLength - (6 + 1)));
                fileChannel.position(fileChannel.position() + 4 * blockLength - (BIT_INFO_OFFSET + 1));
            }
            // Buffer still positioned right before the last header to be read
            else {
//System.out.println("toAppendPosition: writing bitInfo (" +
//                   Integer.toHexString(bitInfo) +  ") over last block's 6th word for buffer at pos " +
//                   (buffer.position() + BIT_INFO_OFFSET));

                // Write over 6th block header word
                buffer.putInt(buffer.position() + BIT_INFO_OFFSET, bitInfo);

                // Hop over the entire block
                buffer.position(buffer.position() + 4*blockLength);
            }
        }
        // else if last block has NO data in it ...
        else {
            // We already read in the block header, now back up so we can overwrite it.
            // If using buffer, we never incremented the position, so we're OK.
            blockNumber--;
            if (toFile) {
                fileChannel.position(fileChannel.position() - 32);
//System.out.println("toAppendPos: position (bkup) = " + fileChannel.position());
            }
        }

        // Write empty last block header. Thus if our program crashes, the file
        // will be OK. This last block header will be over-written with each
        // subsequent write/flush.
        if (toFile) {
//System.out.println("toAppendPos: file pos = " + fileChannel.position());
            bytesWrittenToFile = fileChannel.position();
        }
        else {
            bytesWrittenToBuffer = buffer.position() + headerBytes;
        }

        // We should now be in a state identical to that if we had
        // just now written everything currently in the file/buffer.
//System.out.println("toAppendPos: at END, blockNum = " + blockNumber);
    }


    /**
     * Write a block header into the given buffer.
     * After this method is called the buffer position is after the
     * new block header.
     *
     * @param eventCount    number of events in block
     * @param blockNumber   number of block
     * @param bitInfo       set of bits to include in first block header
     * @param hasDictionary does this block have a dictionary?
     * @param isLast        is this the last block?
     * @param hasFirstEvent does this block have a first event for file splitting?
     *
     * @throws EvioException if no room in buffer to write this block header
     */
    private void writeNewHeader(int eventCount,
                                int blockNumber, BitSet bitInfo,
                                boolean hasDictionary, boolean isLast,
                                boolean hasFirstEvent, boolean writeBlockSizes)
            throws EvioException {

        int compBytes;
        System.out.println("\nwriteNewHeader: eventSizesCount = " + eventSizesCount +
                                   ", member writeBlockSize = " + writeBlockEventSizes +
                                   ", arg writeBlockSize = " + writeBlockSizes + "\n");

        // In general, we write an uncompressed header optionally followed by a
        // somewhat compressed (not lz4) array of event sizes as part of the block header,
        // followed by a block of data (possibly compressed using lz4).
        //
        // Before writing a header, check to see if we need to write size array & data
        // for the old header before adding the new header.
        // (Note, this is only true if writing to files).
        if (writeBlockEventSizes && eventSizesCount > 0) {
            System.out.println("\nwriteNewHeader: converting int size array to bytes\n");
            // Prepare sizes array by compressing it (not lz4).
            toByteSizes();

            // If no room for data to be written ...
            if (buffer.remaining() < blockIndexBytes) {
                throw new EvioException("Buffer size exceeded, need " +
                          blockIndexBytes  + " but have " + buffer.remaining() + " bytes");
            }

            // Write sizes
            buffer.put(blockIndexes, 0, blockIndexBytes);

            // Update variables
            currentBlockSize += blockIndexBytes/4;
            bytesWrittenToBuffer += blockIndexBytes;

            // Update the current block header's block size & header size
            buffer.putInt(currentHeaderPosition, currentBlockSize);

            buffer.putInt(currentHeaderPosition + 4*BlockHeaderV4.EV_HEADERSIZE,
                          (headerBytes + blockIndexBytes)/4);
        }
        
        // If we're compressing data, compress & write any data before adding the new header
        if (compressedOutput && uncompressedDataBuf.position() > 0) {
            // Get ready to read
            uncompressedDataBuf.flip();

            // If no room for data to be written ...
            if (buffer.remaining() < uncompressedDataBuf.remaining()) {
                throw new EvioException("Buffer size exceeded, need " +
                                        uncompressedDataBuf.remaining()  +
                                        " but have " + buffer.remaining() + " bytes");
            }

//System.out.println("COMPRESS: uncom pos = " + uncompressedDataBuf.position() +
//            ", lim = " + uncompressedDataBuf.limit() + ", buf pos = " + buffer.position() +
//            ", lim = " + buffer.limit());

            // Compress data and write into internal buffer
            int pos = buffer.position();
            lz4Comp.compress(uncompressedDataBuf, buffer);
            compBytes = buffer.position() - pos;

            // # of bytes to round up # of compressed bytes to 4-byte boundary
            int extraBytes = (((compBytes + 3) >>> 2) << 2) - compBytes;
//            System.out.println("writeNewHeader: round up = " + (((compBytes + 3) >>> 2) << 2) +
//                                ", comp bytes = " + compBytes + ", extraBytes = " + extraBytes);
            if (extraBytes == 1) {
                buffer.put((byte)0);
//System.out.println("writeNewHeader: 1 FILLER BYTE");
            }
            else if (extraBytes == 2) {
                buffer.putShort((short)0);
//System.out.println("writeNewHeader: 2 FILLER BYTEs");
            }
            else if (extraBytes == 3) {
                buffer.put((byte)0);
                buffer.putShort((short)0);
//System.out.println("writeNewHeader: 3 FILLER BYTEs");
            }

            // Need to write this value into the current header
            buffer.putInt(currentHeaderPosition + 4*BlockHeaderV4.EV_COMPRESSEDLENGTH,
                          compBytes);

System.out.println("AFTER COMPRESS: uncom pos = " + uncompressedDataBuf.position() +
                        ", lim = " + uncompressedDataBuf.limit() + ", buf pos = " + buffer.position() +
                        ", lim = " + buffer.limit() + ", comp bytes = " + compBytes);

            // Get ready to be written into
            uncompressedDataBuf.clear();
        }
        // If we're NOT compressing data but are writing to a file, the data is stored
        // in the uncompressedDataBuf and needs to be transferred to buffer.
        else if (toFile) {
            // Get ready to read
            uncompressedDataBuf.flip();
            // If no room for data to be written ...
            if (buffer.remaining() < uncompressedDataBuf.remaining()) {
                throw new EvioException("Buffer size exceeded, need " +
                                        uncompressedDataBuf.remaining()  +
                                        " but have " + buffer.remaining() + " bytes");
            }
            // Copy data
            buffer.put(uncompressedDataBuf);
            // Get ready to be written into
            uncompressedDataBuf.clear();

            // The current header already incremented its size in writeEventToBuffer()
            // so do not do it here.
        }

        // If no room left for a header to be written ...
        if (buffer.remaining() < 32) {
            throw new EvioException("Buffer size exceeded, need 32 but have " + buffer.remaining() + " bytes");
        }

        // Record where beginning of header is so we can
        // go back and update block size and event count.
        currentHeaderPosition = buffer.position();
System.out.println("writeNewHeader: before writing new header, set currentHeaderPos to " + currentHeaderPosition);

        // Calculate the 6th header word (ok if bitInfo == null)
        int sixthWord = BlockHeaderV4.generateSixthWord(bitInfo, 4,
                                                        hasDictionary, isLast,
                                                        hasFirstEvent, compressedOutput,
                                                        writeBlockSizes, 0);

//        System.out.println("EventWriter (header): words = " + words +
//                ", block# = " + blockNumber + ", ev Cnt = " + eventCount +
//                ", 6th wd = " + sixthWord);
//        System.out.println("Evio header: block# = " + blockNumber + ", last = " + isLast);

        // Write header words, some of which will be
        // overwritten later when the length/event count are determined.
        buffer.putInt(headerWords);
        buffer.putInt(blockNumber);
        buffer.putInt(headerWords);
        buffer.putInt(eventCount);
        buffer.putInt(reserved1);
        buffer.putInt(sixthWord);
        buffer.putInt(reserved2);
        buffer.putInt(IBlockHeader.MAGIC_NUMBER);
        if (isLast) {
//System.out.println("writeNewHeader: last empty header added");
            // Last item in internal buffer is last empty block header
            lastEmptyBlockHeaderExists = true;
        }
//System.out.println("writeNewHeader: buffer pos = " + buffer.position());

        currentBlockSize = headerWords;
        currentBlockEventCount = 0;
        bytesWrittenToBuffer += headerBytes;
//System.out.println("writeNewHeader: set bytesWrittenToBuffer +32 to " + bytesWrittenToBuffer);
    }


    /**
     * Write common events (if any) into the first block of the file/buffer.
     * The common events includes the dictionary and the firstEvent.
     * Note that the first event may be set some time after events have
     * started to be written. In that case the firstEvent is written like
     * any other. When the file subsequently splits and this method is called,
     * it will at that time be a part of the first, common block.
     *
     * @throws EvioException if not enough room in buffer
     */
    private void writeCommonBlock() throws EvioException {

        // No common events to write
        if (xmlDictionary == null && !haveFirstEvent) return;

        ByteBuffer destBuf;
        if (toFile) {
            destBuf = uncompressedDataBuf;
        }
        else {
            destBuf = buffer;
        }

        // Check to see if there is room in buffer for everything
        if (commonBlockByteSize > destBuf.remaining()) {
            // If writing to fixed sized buffer, throw exception
            if (!toFile) {
                throw new EvioException("Not enough buffer mem for dictionary & first event");
            }

            // Use a bigger buffer
            expandBuffer(commonBlockByteSize + 2*headerBytes);
            resetBuffer(true);
        }

System.out.println("writeCommonBlock: write common block with bank bytes = " +
                               commonBlockByteSize + ", remaining = " + buffer.remaining());

        if (xmlDictionary != null) {
            int pos = destBuf.position();

            // Write bank header
            destBuf.putInt(dictionaryByteArray.length / 4 + 1);

            if (destBuf.order() == ByteOrder.BIG_ENDIAN) {
                destBuf.putShort((short) 0);
                destBuf.put((byte) (DataType.CHARSTAR8.getValue()));
                destBuf.put((byte) 0);
            }
            else {
                destBuf.put((byte) 0);
                destBuf.put((byte) (DataType.CHARSTAR8.getValue()));
                destBuf.putShort((short) 0);
            }
System.out.println("writeCommonBlock: header byte 1 = 0x" +
                               Integer.toHexString(destBuf.getInt(pos)) +
                               ", header byte 2 = 0x" +
                               Integer.toHexString(destBuf.getInt(pos + 4)));

            // Write dictionary characters
            destBuf.put(dictionaryByteArray);

            // Book keeping
            // evensWrittenTotal is NOT increased here since
            // it does NOT include any dictionaries
            wroteDictionary = true;
            eventsWrittenToBuffer++;
            currentBlockEventCount++;
        }

        if (haveFirstEvent) {
            // Write first event
            destBuf.put(firstEventByteArray);

            // Book keeping
            eventsWrittenTotal++;
            eventsWrittenToBuffer++;
            currentBlockEventCount++;

            // Update header: 1 event in this block (dictionary gets ignored)
            buffer.putInt(currentHeaderPosition + EVENT_COUNT_OFFSET, 1);
        }

        // Update header's # of 32 bit words written to current block/buffer
        currentBlockSize     += commonBlockByteSize /4;
        // This is an over estimate if we're compressing data,
        // but there is no accurate way of calculating this in that case.
        bytesWrittenToBuffer += commonBlockByteSize;

        buffer.putInt(currentHeaderPosition, currentBlockSize);
        // As soon as we write an event, we need another last empty block
        lastEmptyBlockHeaderExists = false;
//if (debug) System.out.println("writeCommonBlock: done writing, remaining = " +
//                              buffer.remaining());
    }


    /**
     * This method initializes the internal buffer
     * as if the constructor was just called and resets some variables.
     * It starts a new block.
     * @param beforeDictionary is this to reset buffer as it was before the
     *                         writing of the dictionary?
     */
    private void resetBuffer(boolean beforeDictionary) {
        // Go back to the beginning of the buffer & set limit to cap
        buffer.clear();
        if (toFile) {
            uncompressedDataBuf.clear();
        }

        // Reset buffer values
        bytesWrittenToBuffer  = 0;
        eventsWrittenToBuffer = 0;

        // Write initial block header so we can start writing after it
        try {
            if (beforeDictionary) {
//if (debug) System.out.println("      resetBuffer: as in constructor");
                blockNumber = 1;
                writeNewHeader(0, blockNumber++,
                               null, (xmlDictionary != null),
                               false, false, writeBlockEventSizes);
            }
            else {
//if (debug) System.out.println("      resetBuffer: NOTTTT as in constructor");
                writeNewHeader(0, blockNumber++, null,
                               false, false,
                               false, writeBlockEventSizes);
            }
        }
        catch (EvioException e) {/* never happen */}
    }


    /**
     * This method expands the size of the internal buffer used when
     * writing to files. Some variables are updated.
     *
     * @param newSize size in bytes to make the new buffer
     */
    private void expandBuffer(int newSize) {
        // No need to increase it
        if (newSize <= bufferSize) {
//System.out.println("    expandBuffer: buffer is big enough");
            return;
        }

        // Use the new buffer from here on
        bufferSize = newSize;
        if (direct) {
            buffer = ByteBuffer.allocateDirect(bufferSize);
        }
        else {
            buffer = ByteBuffer.allocate(bufferSize);
        }
        buffer.order(byteOrder);

        if (toFile) {
            if (direct) {
                uncompressedDataBuf = ByteBuffer.allocateDirect(bufferSize);
            }
            else {
                uncompressedDataBuf = ByteBuffer.allocate(bufferSize);
            }
            uncompressedDataBuf.order(byteOrder);
        }

//System.out.println("    expandBuffer: increased buf size to " + newSize + " bytes");
        return;
    }


    /**
     * This routine writes an event into the internal buffer
     * and does much of the bookkeeping associated with it.
     *
     * @param currentEventBytes  number of bytes to write from buffer
     */
    private void writeEventToBuffer(EvioBank bank, ByteBuffer bankBuffer, int currentEventBytes)
                                            throws EvioException{

System.out.println("  writeEventToBuffer: before write, bytesToBuf = " +
                bytesWrittenToBuffer);
        //----------------------------------------------------------
        // Write event to separate buffer just for the data
        //----------------------------------------------------------
        if (toFile) {
            if (bankBuffer != null) {
                uncompressedDataBuf.put(bankBuffer);
            }
            else if (bank != null) {
                bank.write(uncompressedDataBuf);
            }
            else {
                return;
            }
        }
        else {
            if (bankBuffer != null) {
                buffer.put(bankBuffer);
            }
            else if (bank != null) {
                bank.write(buffer);
            }
            else {
                return;
            }
        }


        //--------------------------------------------------------------------------------
        // Store the size of this event in a list which is used later in header
        //--------------------------------------------------------------------------------
System.out.println("  writeEventToBuffer: add size");
        addSizeToArray(currentEventBytes);

        //--------------------------------------------------------------------------------
        // Update the current block header: size, event count, status bits
        //--------------------------------------------------------------------------------
        currentBlockSize += currentEventBytes/4;
        // This is an over estimate if we're compressing data,
        // but there is no accurate way of calculating this in that case.
        bytesWrittenToBuffer += currentEventBytes;

        eventsWrittenTotal++;
        eventsWrittenToBuffer++;
        currentBlockEventCount++;

        buffer.putInt(currentHeaderPosition, currentBlockSize);
        buffer.putInt(currentHeaderPosition + EventWriter.EVENT_COUNT_OFFSET,
                      currentBlockEventCount);

        // If we wrote a dictionary and it's the first block,
        // don't count dictionary in block header's event count
        if (wroteDictionary && (blockNumber == 2) && (currentBlockEventCount > 1)) {
//if (debug)  System.out.println("  writeEventToBuffer: subtract ev cnt since in dictionary's blk, cnt = " +
//                                      (currentBlockEventCount - 1));
            buffer.putInt(currentHeaderPosition + EventWriter.EVENT_COUNT_OFFSET,
                          currentBlockEventCount - 1);
        }

//if (debug) System.out.println("  writeEventToBuffer: after write,  bytesToBuf = " +
//                bytesWrittenToBuffer + ", blksiz = " + currentBlockSize + ", blkEvCount (w/ dict) = " +
//                currentBlockEventCount + ", blk # = " + blockNumber + ", wrote Dict = " + wroteDictionary);

        // If we're writing over the last empty block header, clear last block bit
        // This will happen if expanding buffer.
        int headerInfoWord = buffer.getInt(currentHeaderPosition + EventWriter.BIT_INFO_OFFSET);
        if (BlockHeaderV4.isLastBlock(headerInfoWord)) {
//System.out.println("  writeEventToBuffer: clear last event bit in block header");
            buffer.putInt(currentHeaderPosition + EventWriter.BIT_INFO_OFFSET,
                          BlockHeaderV4.clearLastBlockBit(headerInfoWord));
        }

        // As soon as we write an event, we need another last empty block
        lastEmptyBlockHeaderExists = false;

//        if (debug) {
//            System.out.println("evWrite: after last header written, Events written to:");
//            System.out.println("         cnt total (no dict) = " + eventsWrittenTotal);
//            System.out.println("         file cnt total (dict) = " + eventsWrittenToFile);
//            System.out.println("         internal buffer cnt (dict) = " + eventsWrittenToBuffer);
//            System.out.println("         block cnt (dict) = " + currentBlockEventCount);
//            System.out.println("         bytes-to-buf  = " + bytesWrittenToBuffer);
//            System.out.println("         bytes-to-file = " + bytesWrittenToFile);
//            System.out.println("         block # = " + blockNumber);
//        }
    }


    /**
     * Is there room to write this many bytes to an output buffer as a single event?
     * Will always return true when writing to a file.
     * @param bytes number of bytes to write
     * @return {@code true} if there still room in the output buffer, else {@code false}.
     */
    public boolean hasRoom(int bytes) {
//System.out.println("Buffer size = " + bufferSize + ", bytesWritten = " + bytesWrittenToBuffer +
//        ", <? " + (bytes + headerBytes));
        return toFile() || (bufferSize - bytesWrittenToBuffer) >= bytes + headerBytes;
    }

    /**
     * Write an event (bank) to the buffer in evio version 4 format.
     * If the internal buffer is full, it will be flushed to the file if writing to a file.
     * Otherwise an exception will be thrown.
     *
     * @param node   object representing the event to write in buffer form
     * @param force  if writing to disk, force it to write event to the disk.
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    public void writeEvent(EvioNode node, boolean force)
            throws EvioException, IOException {

        // Duplicate buffer so we can set pos & limit without messing others up
        writeEvent(node, force, true);
    }

    /**
     * Write an event (bank) to the buffer in evio version 4 format.
     * If the internal buffer is full, it will be flushed to the file if writing to a file.
     * Otherwise an exception will be thrown.
     *
     * @param node       object representing the event to write in buffer form
     * @param force      if writing to disk, force it to write event to the disk.
     * @param duplicate  if true, duplicate node's buffer so its position and limit
     *                   can be changed without issue.
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     *                       if null node arg;
     */
    public void writeEvent(EvioNode node, boolean force, boolean duplicate)
            throws EvioException, IOException {

        if (node == null) {
            throw new EvioException("null node arg");
        }

//        int origLim=0,origPos=0;
        ByteBuffer eventBuffer, bb = node.getBufferNode().getBuffer();

        // Duplicate buffer so we can set pos & limit without messing others up
        if (duplicate) {
            eventBuffer = bb.duplicate().order(bb.order());
        }
        else {
            eventBuffer = bb;
//            origLim = bb.limit();
//            origPos = bb.position();
        }

        int pos = node.getPosition();
        eventBuffer.limit(pos + node.getTotalBytes()).position(pos);
        writeEvent(null, eventBuffer, force);

        // Shouldn't the pos & lim be reset for non-duplicate?
        // It don't think it matters since node knows where to
        // go inside the buffer.
//        if (!duplicate) {
//            if (pos > origLim) {
//                bb.position(origPos).limit(origLim);
//            }
//            else {
//                bb.limit(origLim).position(origPos);
//            }
//        }
    }

    /**
     * Write an event (bank) to the buffer in evio version 4 format.
     * The given event buffer must contain only the event's data (event header
     * and event data) and must <b>not</b> be in complete evio file format.
     * If the internal buffer is full, it will be flushed to the file if writing to a file.
     * Otherwise an exception will be thrown.
     *
     * @param eventBuffer the event (bank) to write in buffer form
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    public void writeEvent(ByteBuffer eventBuffer)
            throws EvioException, IOException {
        writeEvent(null, eventBuffer, false);
    }

    /**
     * Write an event (bank) to a buffer containing evio version 4 format blocks.
     * Each block has an integral number of events. There are limits to the
     * number of events in each block and the total size of each block.
     * If writing to a file, each full buffer is written - one at a time -
     * and may contain multiple blocks. Dictionary is never written with
     * this method.
     *
     * @param bank the bank to write.
     * @throws IOException   if error writing file
     * @throws EvioException if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    public void writeEvent(EvioBank bank)
            throws EvioException, IOException {
        writeEvent(bank, null, false);
    }


    /**
     * Write an event (bank) to the buffer in evio version 4 format.
     * The given event buffer must contain only the event's data (event header
     * and event data) and must <b>not</b> be in complete evio file format.
     * If the internal buffer is full, it will be flushed to the file if
     * writing to a file. Otherwise an exception will be thrown.<p>
     * Be warned that injudicious use of the 2nd arg, the force flag, will
     * <b>kill</b> performance.
     *
     * @param bankBuffer the bank (as a ByteBuffer object) to write.
     * @param force      if writing to disk, force it to write event to the disk.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    public void writeEvent(ByteBuffer bankBuffer, boolean force)
            throws EvioException, IOException {
        writeEvent(null, bankBuffer, force);
    }


    /**
     * Write an event (bank) to a buffer containing evio version 4 format blocks.
     * Each block has an integral number of events. There are limits to the
     * number of events in each block and the total size of each block.
     * If writing to a file, each full buffer is written - one at a time -
     * and may contain multiple blocks. Dictionary is never written with
     * this method.<p>
     * Be warned that injudicious use of the 2nd arg, the force flag, will
     * <b>kill</b> performance.
     *
     * @param bank   the bank to write.
     * @param force  if writing to disk, force it to write event to the disk.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    public void writeEvent(EvioBank bank, boolean force)
            throws EvioException, IOException {
        writeEvent(bank, null, force);
    }


    /**
     * Write an event (bank) to a buffer in evio version 4 format.
     * The event to be written may be in one of two forms.
     * The first is as an EvioBank object and the second is as a ByteBuffer
     * containing only the event's data (event header and event data) and must
     * <b>not</b> be in complete evio file format.
     * The first non-null of the bank arguments will be written.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     *<b>kill</b> performance.<p>
     *
     * This method is not used to write the dictionary or the first event
     * (common block). That is only done with the method {@link #writeCommonBlock}.
     *
     * @param bank the bank (as an EvioBank object) to write.
     * @param bankBuffer the bank (as a ByteBuffer object) to write.
     * @param force      if writing to disk, force it to write event to the disk.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if bad bankBuffer format;
     *                       if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    synchronized private void writeEvent(EvioBank bank, ByteBuffer bankBuffer, boolean force)
            throws EvioException, IOException {

        if (closed) {
            throw new EvioException("close() has already been called");
        }

        boolean doFlush = false;
        boolean roomInBuffer = true;
        boolean splittingFile = false;
        boolean needBiggerBuffer = false;
        boolean writeNewBlockHeader = true;

        // See how much space the event will take up
        int newBufSize = 0;
        int currentEventBytes;

        // Which bank do we write?
        if (bankBuffer != null) {
            if (bankBuffer.order() != byteOrder) {
                throw new EvioException("event buf is " + bankBuffer.order() + ", and writer is " + byteOrder);
            }

            // Event size in bytes (from buffer ready to read)
            currentEventBytes = bankBuffer.remaining();

            // Size must be multiple of 4 bytes (whole 32-bit ints)
            // The following is the equivalent of the mod operation:
            //  (currentEventBytes % 4 != 0) but is much faster (x mod 2^n == x & (2^n - 1))
            if ((currentEventBytes & 3) != 0) {
                throw new EvioException("bad bankBuffer format");
            }

            // Check for inconsistent lengths
            if (currentEventBytes != 4*(bankBuffer.getInt(bankBuffer.position()) + 1)) {
                throw new EvioException("inconsistent event lengths");
            }
        }
        else if (bank != null) {
            currentEventBytes = bank.getTotalBytes();
        }
        else {
            return;
        }

//        if (split > 0) {
//            System.out.println("evWrite: splitting, bytesToFile = " + bytesWrittenToFile +
//                               ", event bytes = " + currentEventBytes +
//                               ", bytesToBuf = " + bytesWrittenToBuffer +
//                               ", split = " + split);
//
//            System.out.println("evWrite: blockNum = " + blockNumber +
//                              ", (blkNum == 2) = " + (blockNumber == 2) +
//                              ", eventsToBuf (" + eventsWrittenToBuffer +
//                              ")  <=? common blk cnt (" + commonBlockCount + ")");
//        }

        //--------------------------------------------
        // Another Block?
        //--------------------------------------------

        // If we have enough room in the current block and have not exceeded
        // the number of allowed events, write it in the current block.
        // Worry about memory later.
        //
        // If block header is to include a list of event lengths,
        // take that into account with call to getEventLengthsArraySize().
        if ( ((currentEventBytes + 4*currentBlockSize +
               getEventLengthsArraySize(currentEventBytes)) <= targetBlockSize) &&
                  (currentBlockEventCount < blockCountMax)) {
            writeNewBlockHeader = false;
//System.out.println("evWrite: do NOT need a new blk header: blk size target = " + targetBlockSize +
//                    " >= " + (currentEventBytes + 4*currentBlockSize + getEventLengthsArraySize(currentEventBytes)) +
//                    " bytes,  " + "blk count = " + currentBlockEventCount + ", max = " + blockCountMax);
        }
//        else {
//System.out.println("evWrite: DO need a new blk header: blk size target = " + targetBlockSize +
//                    " < " + (currentEventBytes + 4*currentBlockSize + getEventLengthsArraySize(currentEventBytes) + headerBytes) +
//                    " bytes,  blk count = " + currentBlockEventCount + ", max = " + blockCountMax);
//            if (currentBlockEventCount >= blockCountMax) {
//                System.out.println("evWrite: too many events in block, already have " +
//                                           currentBlockEventCount);
//            }
//        }

        //--------------------------------------------
        // Are we splitting files?
        //--------------------------------------------
        while (split > 0) {
            // If it's the first block and all that has been written so far are
            // the dictionary and first event, don't split after writing it.
            if (blockNumber == 2 && eventsWrittenToBuffer <= commonBlockCount) {
//if (debug) System.out.println("evWrite: don't split file cause only common block written so far");
                break;
            }

            // Is this event (with the current buffer, current file, ending block header,
            // and current header's possible event size array) large enough to split the file?
            long totalSize = currentEventBytes + bytesWrittenToFile +
                             bytesWrittenToBuffer + headerBytes +
                             getEventLengthsArraySize(currentEventBytes);

            // If we have to add another block header (before this event), account for it.
            if (writeNewBlockHeader) {
                totalSize += headerBytes;
//                headerCount++;
//if (debug) System.out.println("evWrite: account for another block header when splitting");
            }

//if (debug) System.out.println("evWrite: splitting = " + (totalSize > split) +
//                    ": total size = " + totalSize + " >? split = " + split);
//
//if (debug) System.out.println("evWrite: total size components: bytesToFile = " +
//                bytesWrittenToFile + ", bytesToBuf = " + bytesWrittenToBuffer +
//                ", event bytes = " + currentEventBytes);

            // If we're going to split the file ...
            if (totalSize > split) {
                // Yep, we're gonna do it
                splittingFile = true;

System.out.println("evWrite: prepare to split file");
                // Flush the current buffer if any events contained and prepare
                // for a new file (split) to hold the current event.
                if (eventsWrittenToBuffer > 0) {
                    doFlush = true;
//if (debug) System.out.println("evWrite: eventsToBuf > 0 so doFlush = True");
                }
            }

            break;
        }

//if (debug) System.out.println("evWrite: bufSize = " + bufferSize +
//                              " <? bytesToWrite = " + (bytesWrittenToBuffer + currentEventBytes) +
//                              " + 64 = " + (bytesWrittenToBuffer + currentEventBytes + 64) +
//                              ", events in buf = " + eventsWrittenToBuffer);

        //--------------------------------------------
        // Internal buffer too small?
        //--------------------------------------------

        // Is this event (by itself) too big for the current internal buffer?
        // Internal buffer needs room for a block with: header,
        // (only if including events lengths in header) additional space in header,
        // and event. Don't forget ending empty block header if applicable.
        int oneBigEventBlockLen = currentEventBytes + headerBytes + getEventLengthsArraySize(currentEventBytes);

        if (bufferSize < oneBigEventBlockLen + headerBytes) {
            if (!toFile) {
                System.out.println("evWrite: error, bufSize = " + bufferSize +
                   " <? current event bytes = " + currentEventBytes +
                   " + 2 headers (64) + possible ev lens in hdr, total = " +
                   (oneBigEventBlockLen + headerBytes) +
                   ", room = " + (bufferSize - bytesWrittenToBuffer - headerBytes) );
                throw new EvioException("Buffer too small to write event");
            }
            roomInBuffer = false;
            needBiggerBuffer = true;
//System.out.println("evWrite: NEED bigger internal buffer for 1 big ev, current size = " +
//                           bufferSize + ", ev + blk hdrs size = " +
//                           (oneBigEventBlockLen + headerBytes) );
        }
        // Is this event + extended header + ending block header,
        // in combination with events previously written
        // to the current internal buffer, too big for it?
        else if ((!writeNewBlockHeader &&
                   ((bufferSize - bytesWrittenToBuffer) < (oneBigEventBlockLen))) ||
                 ( writeNewBlockHeader &&
                   ((bufferSize - bytesWrittenToBuffer) < (oneBigEventBlockLen + headerBytes))))  {

            if (!toFile) {
                throw new EvioException("Buffer too small to write event");
            }

//            if (debug) {
//                System.out.print("evWrite: NEED to flush buffer and re-use, ");
//                if (writeNewBlockHeader) {
//                    System.out.println("buf room = " + (bufferSize - bytesWrittenToBuffer) +
//                                       ", needed = "  + (oneBigEventBlockLen + headerBytes));
//                }
//                else {
//                    System.out.println("buf room = " + (bufferSize - bytesWrittenToBuffer) +
//                                       ", needed = "  + oneBigEventBlockLen);
//                }
//            }
            roomInBuffer = false;
        }
//        else {
//            // If we're here, there is room to add event into existing buffer.
//            // As we're the very first event, we need to set blockNumber.
//            System.out.println("evWrite: event will fit, events written to buf so far = " + eventsWrittenToBuffer +
//            ", bytes to buf so far = " + bytesWrittenToBuffer);
//        }


        // If there is no room in the buffer for this event ...
        if (!roomInBuffer) {
            // If we need more room for a single event ...
            if (needBiggerBuffer) {
                // We're here because there is not enough room in the internal buffer
                // to write this single large event.
                newBufSize = oneBigEventBlockLen + headerBytes;
//System.out.println("         must expand, bytes needed for 1 big ev + 2 hdrs = " + newBufSize);
            }
            // Flush what we have to file (if anything)
            doFlush = true;
//System.out.println("evWrite: no room in Buf so doFlush = 1");
        }

        // Do we flush?
        if (doFlush) {
//System.out.println("evWrite: call flushToFile 1");
            flushToFile(false);
        }

        // Do we split the file?
        if (splittingFile) {
            splitFile();
        }

        // Do we expand buffer?
        if (needBiggerBuffer) {
            // If here, we just flushed.
            expandBuffer(newBufSize);
        }

        // If we either flushed events or split the file, reset the
        // internal buffer to prepare it for writing another event.
        // Start a new block.
        if (doFlush || splittingFile) {
//System.out.println("evWrite: call resetBuffer(false) 1");
            resetBuffer(false);
            // We have a newly initialized buffer ready to write
            // to, so we don't need a new block header.
            writeNewBlockHeader = false;
        }

        //********************************************************************
        // Now we have enough room for the event in the buffer, block & file
        //********************************************************************

        //********************************************************************
        // Before we go on, if the file was actually split, we must add any
        // existing common block (dictionary & first event & block) in the
        // new file before we write the event.
        //********************************************************************
        if (splittingFile && (xmlDictionary != null || haveFirstEvent)) {
            // Memory needed to write: dictionary + first event + 3 block headers
            // (beginning, after dict & first event, and ending) + event
            int neededBytes = commonBlockByteSize + 3*headerBytes + currentEventBytes;
//if (debug) System.out.println("evWrite: write DICTIONARY after splitting, needed bytes = " + neededBytes);

            // Write block header after dictionary + first event
            writeNewBlockHeader = true;

            // Give us more buffer memory if we need it
            expandBuffer(neededBytes);

            // Reset internal buffer as it was just
            // before writing dictionary in constructor
//System.out.println("evWrite: call resetBuffer(true) 2");
            resetBuffer(true);

            // Write common block to the internal buffer
            writeCommonBlock();

            // Now continue with writing the event ...
        }

        // Write new block header if required
        if (writeNewBlockHeader) {
System.out.println("evWrite: wrote new blk hdr");
            writeNewHeader(1, blockNumber++, null,
                           false, false,
                           false, writeBlockEventSizes);
//if (debug) System.out.println("evWrite: wrote new blk hdr, bytesToBuf = " + bytesWrittenToBuffer);
        }

        // Write out the event
        writeEventToBuffer(bank, bankBuffer, currentEventBytes);

//        if (debug) {
//            System.out.println("evWrite: after last header written, Events written to:");
//            System.out.println("         cnt total (no dict) = " + eventsWrittenTotal);
//            System.out.println("         file cnt total = " + eventsWrittenToFile);
//            System.out.println("         internal buffer cnt = " + eventsWrittenToBuffer);
//            System.out.println("         common block cnt = " + commonBlockCount);
//            System.out.println("         current block cnt (dict) = " + currentBlockEventCount);
//            System.out.println("         bytes-to-buf  = " + bytesWrittenToBuffer);
//            System.out.println("         bytes-to-file = " + bytesWrittenToFile);
//            System.out.println("         block # = " + blockNumber);
//        }

        // If caller wants to flush the event to disk (say, prestart event) ...
        if (force && toFile) {
            // This will kill performance!
            flushToFile(true);
            // Start a new block
            resetBuffer(false);
        }
    }


//    /**
//      * Write an event (bank) to a buffer in evio version 4 format.
//      * The event to be written may be in one of two forms.
//      * The first is as an EvioBank object and the second is as a ByteBuffer
//      * containing only the event's data (event header and event data) and must
//      * <b>not</b> be in complete evio file format.
//      * The first non-null of the bank arguments will be written.<p>
//      *
//      * Be warned that injudicious use of a true 2nd arg, the force flag, will
//      *<b>kill</b> performance.<p>
//      *
//      * This method is not used to write the dictionary or the first event
//      * (common block). That is only done with the method {@link #writeCommonBlock}.
//      *
//      * @param bank the bank (as an EvioBank object) to write.
//      * @param bankBuffer the bank (as a ByteBuffer object) to write.
//      * @param force      if writing to disk, force it to write event to the disk.
//      *
//      * @throws IOException   if error writing file
//      * @throws EvioException if event is opposite byte order of internal buffer;
//      *                       if bad bankBuffer format;
//      *                       if close() already called;
//      *                       if file could not be opened for writing;
//      *                       if file exists but user requested no over-writing;
//      *                       if no room when writing to user-given buffer;
//      */
//     synchronized private void writeEventOrig(EvioBank bank, ByteBuffer bankBuffer, boolean force)
//             throws EvioException, IOException {
//
//         if (closed) {
//             throw new EvioException("close() has already been called");
//         }
//
//         boolean doFlush = false;
//         boolean roomInBuffer = true;
//         boolean splittingFile = false;
//         boolean needBiggerBuffer = false;
//         boolean writeNewBlockHeader = true;
//
//         // See how much space the event will take up
//         int newBufSize = 0;
//         int currentEventBytes;
//
//         // Which bank do we write?
//         if (bankBuffer != null) {
//             if (bankBuffer.order() != byteOrder) {
//                 throw new EvioException("event buf is " + bankBuffer.order() + ", and writer is " + byteOrder);
//             }
//
//             // Event size in bytes (from buffer ready to read)
//             currentEventBytes = bankBuffer.remaining();
//
//             // Size must be multiple of 4 bytes (whole 32-bit ints)
//             // The following is the equivalent of the mod operation:
//             //  (currentEventBytes % 4 != 0) but is much faster (x mod 2^n == x & (2^n - 1))
//             if ((currentEventBytes & 3) != 0) {
//                 throw new EvioException("bad bankBuffer format");
//             }
//
//             // Check for inconsistent lengths
//             if (currentEventBytes != 4*(bankBuffer.getInt(bankBuffer.position()) + 1)) {
//                 throw new EvioException("inconsistent event lengths");
//             }
//         }
//         else if (bank != null) {
//             currentEventBytes = bank.getTotalBytes();
//         }
//         else {
//             return;
//         }
//
// //        if (split > 0) {
// //            System.out.println("evWrite: splitting, bytesToFile = " + bytesWrittenToFile +
// //                               ", event bytes = " + currentEventBytes +
// //                               ", bytesToBuf = " + bytesWrittenToBuffer +
// //                               ", split = " + split);
// //
// //            System.out.println("evWrite: blockNum = " + blockNumber +
// //                              ", (blkNum == 2) = " + (blockNumber == 2) +
// //                              ", eventsToBuf (" + eventsWrittenToBuffer +
// //                              ")  <=? common blk cnt (" + commonBlockCount + ")");
// //        }
//
//         // If we have enough room in the current block and have not exceeded
//         // the number of allowed events, write it in the current block.
//         // Worry about memory later.
//         if ( ((currentEventBytes + 4*currentBlockSize) <= targetBlockSize) &&
//                   (currentBlockEventCount < blockCountMax)) {
//             writeNewBlockHeader = false;
// //System.out.println("evWrite: do NOT need a new blk header: blk size target = " + targetBlockSize +
// //                    " >= " + (currentEventBytes + 4*currentBlockSize) +
// //                    " bytes,  " + "blk count = " + currentBlockEventCount + ", max = " + blockCountMax);
//         }
// //        else {
// //System.out.println("evWrite: DO need a new blk header: blk size target = " + targetBlockSize +
// //                    " < " + (currentEventBytes + 4*currentBlockSize + headerBytes) + " bytes,  " +
// //                    "blk count = " + currentBlockEventCount + ", max = " + blockCountMax);
// //            if (currentBlockEventCount >= blockCountMax) {
// //                System.out.println("evWrite: too many events in block, already have " +
// //                                           currentBlockEventCount);
// //            }
// //        }
//
//
//         // Are we splitting files in general?
//         while (split > 0) {
//             // If it's the first block and all that has been written so far are
//             // the dictionary and first event, don't split after writing it.
//             if (blockNumber == 2 && eventsWrittenToBuffer <= commonBlockCount) {
// //if (debug) System.out.println("evWrite: don't split file cause only common block written so far");
//                 break;
//             }
//
//             // Is this event (with the current buffer, current file,
//             // and ending block header) large enough to split the file?
//             long totalSize = currentEventBytes + bytesWrittenToFile +
//                              bytesWrittenToBuffer + headerBytes;
//
//             // If we have to add another block header (before this event), account for it.
//             if (writeNewBlockHeader) {
//                 totalSize += headerBytes;
// //                headerCount++;
// //if (debug) System.out.println("evWrite: account for another block header when splitting");
//             }
//
// //if (debug) System.out.println("evWrite: splitting = " + (totalSize > split) +
// //                    ": total size = " + totalSize + " >? split = " + split);
// //
// //if (debug) System.out.println("evWrite: total size components: bytesToFile = " +
// //                bytesWrittenToFile + ", bytesToBuf = " + bytesWrittenToBuffer +
// //                ", event bytes = " + currentEventBytes);
//
//             // If we're going to split the file ...
//             if (totalSize > split) {
//                 // Yep, we're gonna do it
//                 splittingFile = true;
//
// System.out.println("evWrite: prepare to split file");
//                 // Flush the current buffer if any events contained and prepare
//                 // for a new file (split) to hold the current event.
//                 if (eventsWrittenToBuffer > 0) {
//                     doFlush = true;
// //if (debug) System.out.println("evWrite: eventsToBuf > 0 so doFlush = True");
//                 }
//             }
//
//             break;
//         }
//
// //if (debug) System.out.println("evWrite: bufSize = " + bufferSize +
// //                              " <? bytesToWrite = " + (bytesWrittenToBuffer + currentEventBytes) +
// //                              " + 64 = " + (bytesWrittenToBuffer + currentEventBytes + 64) +
// //                              ", events in buf = " + eventsWrittenToBuffer);
//
//         // Is this event (by itself) too big for the current internal buffer?
//         // Internal buffer needs room for first block header, event, and ending empty block.
//         if (bufferSize < currentEventBytes + 2*headerBytes) {
//             if (!toFile) {
//                 System.out.println("evWrite: error, bufSize = " + bufferSize +
//                    " <? current event bytes = " + currentEventBytes +
//                    " + 2 headers (64), total = " + (currentEventBytes + 64) +
//                    ", room = " + (bufferSize - bytesWrittenToBuffer - headerBytes) );
//                 throw new EvioException("Buffer too small to write event");
//             }
//             roomInBuffer = false;
//             needBiggerBuffer = true;
// //System.out.println("evWrite: NEED bigger internal buffer for 1 big ev, current size = " +
// //                           bufferSize + ", ev + blk hdrs size = " + (currentEventBytes + 2*headerBytes) );
//         }
//         // Is this event plus ending block header, in combination with events previously written
//         // to the current internal buffer, too big for it?
//         else if ((!writeNewBlockHeader && ((bufferSize - bytesWrittenToBuffer) < currentEventBytes + headerBytes)) ||
//                  ( writeNewBlockHeader && ((bufferSize - bytesWrittenToBuffer) < currentEventBytes + 2*headerBytes)))  {
//
//             if (!toFile) {
//                 throw new EvioException("Buffer too small to write event");
//             }
//
// //            if (debug) {
// //                System.out.print("evWrite: NEED to flush buffer and re-use, ");
// //                if (writeNewBlockHeader) {
// //                    System.out.println("buf room = " + (bufferSize - bytesWrittenToBuffer) +
// //                                       ", needed = "  + (currentEventBytes + 2*headerBytes));
// //                }
// //                else {
// //                    System.out.println("buf room = " + (bufferSize - bytesWrittenToBuffer) +
// //                                       ", needed = "  + (currentEventBytes + headerBytes));
// //                }
// //            }
//             roomInBuffer = false;
//         }
// //        else {
// //            // If we're here, there is room to add event into existing buffer.
// //            // As we're the very first event, we need to set blockNumber.
// //            System.out.println("evWrite: event will fit, events written to buf so far = " + eventsWrittenToBuffer +
// //            ", bytes to buf so far = " + bytesWrittenToBuffer);
// //        }
//
//
//         // If there is no room in the buffer for this event ...
//         if (!roomInBuffer) {
//             // If we need more room for a single event ...
//             if (needBiggerBuffer) {
//                 // We're here because there is not enough room in the internal buffer
//                 // to write this single large event.
//                 newBufSize = currentEventBytes + 2*headerBytes;
// //System.out.println("         must expand, bytes needed for 1 big ev + 2 hdrs = " + newBufSize);
//             }
//             // Flush what we have to file (if anything)
//             doFlush = true;
// //System.out.println("evWrite: no room in Buf so doFlush = 1");
//         }
//
//         // Do we flush?
//         if (doFlush) {
// //System.out.println("evWrite: call flushToFile 1");
//             flushToFile(false);
//         }
//
//         // Do we split the file?
//         if (splittingFile) {
//             splitFile();
//         }
//
//         // Do we expand buffer?
//         if (needBiggerBuffer) {
//             // If here, we just flushed.
//             expandBuffer(newBufSize);
//         }
//
//         // If we either flushed events or split the file, reset the
//         // internal buffer to prepare it for writing another event.
//         // Start a new block.
//         if (doFlush || splittingFile) {
// //System.out.println("evWrite: call resetBuffer(false) 1");
//             resetBuffer(false);
//             // We have a newly initialized buffer ready to write
//             // to, so we don't need a new block header.
//             writeNewBlockHeader = false;
//         }
//
//         //********************************************************************
//         // Now we have enough room for the event in the buffer, block & file
//         //********************************************************************
//
//         //********************************************************************
//         // Before we go on, if the file was actually split, we must add any
//         // existing common block (dictionary & first event & block) in the
//         // new file before we write the event.
//         //********************************************************************
//         if (splittingFile && (xmlDictionary != null || haveFirstEvent)) {
//             // Memory needed to write: dictionary + first event + 3 block headers
//             // (beginning, after dict & first event, and ending) + event
//             int neededBytes = commonBlockByteSize + 3*headerBytes + currentEventBytes;
// //if (debug) System.out.println("evWrite: write DICTIONARY after splitting, needed bytes = " + neededBytes);
//
//             // Write block header after dictionary + first event
//             writeNewBlockHeader = true;
//
//             // Give us more buffer memory if we need it
//             expandBuffer(neededBytes);
//
//             // Reset internal buffer as it was just
//             // before writing dictionary in constructor
// //System.out.println("evWrite: call resetBuffer(true) 2");
//             resetBuffer(true);
//
//             // Write common block to the internal buffer
//             writeCommonBlock();
//
//             // Now continue with writing the event ...
//         }
//
//         // Write new block header if required
//         if (writeNewBlockHeader) {
//             writeNewHeader(1, blockNumber++, null,
//                            false, false, false);
// //if (debug) System.out.println("evWrite: wrote new blk hdr, bytesToBuf = " + bytesWrittenToBuffer);
//         }
//
//         // Store the size of this event in list
//         addSizeToArray(currentEventBytes);
//
//         // Write out the event
//         writeEventToBuffer(bank, bankBuffer, currentEventBytes);
//
// //        if (debug) {
// //            System.out.println("evWrite: after last header written, Events written to:");
// //            System.out.println("         cnt total (no dict) = " + eventsWrittenTotal);
// //            System.out.println("         file cnt total = " + eventsWrittenToFile);
// //            System.out.println("         internal buffer cnt = " + eventsWrittenToBuffer);
// //            System.out.println("         common block cnt = " + commonBlockCount);
// //            System.out.println("         current block cnt (dict) = " + currentBlockEventCount);
// //            System.out.println("         bytes-to-buf  = " + bytesWrittenToBuffer);
// //            System.out.println("         bytes-to-file = " + bytesWrittenToFile);
// //            System.out.println("         block # = " + blockNumber);
// //        }
//
//         // If caller wants to flush the event to disk (say, prestart event) ...
//         if (force && toFile) {
//             // This will kill performance!
//             flushToFile(true);
//             // Start a new block
//             resetBuffer(false);
//         }
//     }



    /**
     * Flush everything in buffer to file.
     * Does nothing if object already closed.
     * Only called by synchronized methods.<p>
     *
     * @param force force it to write event to the disk.
     * @return {@code false} if no data written, else {@code true}
     *
     * @throws EvioException if this object already closed;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     * @throws IOException   if error writing file
     */
    private boolean flushToFile(boolean force) throws EvioException, IOException {
        if (closed) {
            throw new EvioException("close() has already been called");
        }

        // If not writing to file, just return
        if (!toFile ) {
            return false;
        }

        // If nothing to write...
        if (buffer.position() < 1) {
//System.out.println("    flushToFile: nothing to write, return");
            return false;
        }

        // The byteBuffer position is just after the last event or
        // the last, empty block header. Get buffer ready to write.
        buffer.flip();
System.out.println("    flushToFile: try writing " + eventsWrittenToBuffer + " events, " +
                    buffer.remaining() + " bytes");

        // Write everything in internal buffer out to file
        int bytesWritten = buffer.remaining();
        while (buffer.hasRemaining()) {
            fileChannel.write(buffer);
        }

        // Force it to write to physical disk (KILLS PERFORMANCE!!!, 15x-20x slower),
        // but don't bother writing the metadata (arg to force()) since that slows it
        // down too.
        if (force) fileChannel.force(false);

        // Set buf position to 0 and set limit to capacity
        buffer.clear();

        // Keep track of what is written to this, one, file
        bytesWrittenToFile  += bytesWritten;
        eventsWrittenToFile += eventsWrittenToBuffer;

        if (true) {
            System.out.println("    flushToFile: after last header written, Events written to:");
            System.out.println("                 cnt total (no dict) = " + eventsWrittenTotal);
            System.out.println("                 file cnt total (dict) = " + eventsWrittenToFile);
            System.out.println("                 internal buffer cnt (dict) = " + eventsWrittenToBuffer);
            System.out.println("                 current  block  cnt (dict) = " + currentBlockEventCount);
            System.out.println("                 bytes-written  = " + bytesWritten);
            System.out.println("                 bytes-to-file = " + bytesWrittenToFile);
            System.out.println("                 block # = " + blockNumber);
        }

        // Buffer has been flushed, nothing in it
        bytesWrittenToBuffer   = 0;
        eventsWrittenToBuffer  = 0;

        return true;
    }



//    /**
//     * Flush everything in buffer to file.
//     * Does nothing if object already closed.
//     * Only called by synchronized methods.<p>
//     *
//     * @param force force it to write event to the disk.
//     * @return {@code false} if no data written, else {@code true}
//     *
//     * @throws EvioException if this object already closed;
//     *                       if file could not be opened for writing;
//     *                       if file exists but user requested no over-writing;
//     * @throws IOException   if error writing file
//     */
//    private boolean flushToFileOrig(boolean force) throws EvioException, IOException {
//        if (closed) {
//            throw new EvioException("close() has already been called");
//        }
//
//        // If not writing to file, just return
//        if (!toFile ) {
//            return false;
//        }
//
//        // If nothing to write...
//        if (buffer.position() < 1) {
////System.out.println("    flushToFile: nothing to write, return");
//            return false;
//        }
//
//        // The byteBuffer position is just after the last event or
//        // the last, empty block header. Get buffer ready to write.
//        buffer.flip();
////System.out.println("    flushToFile: try writing " + eventsWrittenToBuffer + " events");
//
//        // Write everything in internal buffer out to file
//        int bytesWritten = buffer.remaining();
//
//        if (compressedOutput) {
//            if (buffer.hasArray()) {
////                lz4Out.write(buffer.array(),
////                             buffer.arrayOffset() + buffer.position(),
////                             bytesWritten);
//            }
//            else {
//                while(buffer.hasRemaining()) {
//                    byte b = buffer.get();
////                    lz4Out.write(b);
//                }
//            }
//
//            // TODO: this may not work
////            if (force) lz4Out.flush();
//        }
//        else {
//            while (buffer.hasRemaining()) {
//                fileChannel.write(buffer);
//            }
//
//            // Force it to write to physical disk (KILLS PERFORMANCE!!!, 15x-20x slower),
//            // but don't bother writing the metadata (arg to force()) since that slows it
//            // down too.
//            if (force) fileChannel.force(false);
//        }
//
//        // Set buf position to 0 and set limit to capacity
//        buffer.clear();
//
//        // Keep track of what is written to this, one, file
//        bytesWrittenToFile  += bytesWritten;
//        eventsWrittenToFile += eventsWrittenToBuffer;
//
////        if (debug) {
////            System.out.println("    flushToFile: after last header written, Events written to:");
////            System.out.println("                 cnt total (no dict) = " + eventsWrittenTotal);
////            System.out.println("                 file cnt total (dict) = " + eventsWrittenToFile);
////            System.out.println("                 internal buffer cnt (dict) = " + eventsWrittenToBuffer);
////            System.out.println("                 current  block  cnt (dict) = " + currentBlockEventCount);
////            System.out.println("                 bytes-written  = " + bytesWritten);
////            System.out.println("                 bytes-to-file = " + bytesWrittenToFile);
////            System.out.println("                 block # = " + blockNumber);
////        }
//
//        // Buffer has been flushed, nothing in it
//        bytesWrittenToBuffer   = 0;
//        eventsWrittenToBuffer  = 0;
//
//        return true;
//    }


    /**
     * Split the file.
     * Never called when output destination is buffer or split = 0.
     * Otherwise it resets the buffer,
     * tells the fileCloser thread to close the old file,
     * and opens the new.
     *
     * @throws EvioException if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no more room in internal buffer (should never happen).
     * @throws IOException   if error writing file
     */
    private void splitFile() throws EvioException, IOException {
        // We should end the file with an empty block header.
        if (eventsWrittenToBuffer > 0 || bytesWrittenToBuffer < 1) {
//System.out.println("    split file: write last header in old file, buf pos = " + buffer.position());
            writeNewHeader(0, blockNumber, null,
                           false, true,
                           false, false);
        }

        // Close this file
        fileCloser.closeFile(raf);

        // Name the next file
        String nextFileName = Utilities.generateFileName(baseFileName, specifierCount,
                                                         runNumber, split, splitCount++,
                                                         streamId);
        // Create the next file
        if (compressedOutput) {
            nextFileName += ".lz4";
        }
System.out.println("splitFile: creating next file, " + nextFileName);
        currentFile = new File(nextFileName);

        // If we can't overwrite and file exists ...
        if (!overWriteOK && (currentFile.exists() && currentFile.isFile())) {
            raf.close();
            throw new EvioException("File exists but user requested no over-writing, " +
                                                   currentFile.getPath());
        }

        // Create the next file
        raf  = new RandomAccessFile(currentFile, "rw");
        fileChannel = raf.getChannel();

        // Reset file values for reuse
        blockNumber         = 1;
        bytesWrittenToFile  = 0;
        eventsWrittenToFile = 0;
        wroteDictionary     = false;

//System.out.println("    splitFile: generated file name = " + fileName);
    }


//    /**
//     * Split the file.
//     * Never called when output destination is buffer or split = 0.
//     * Otherwise it resets the buffer,
//     * tells the fileCloser thread to close the old file,
//     * and opens the new.
//     *
//     * @throws EvioException if file could not be opened for writing;
//     *                       if file exists but user requested no over-writing;
//     *                       if no more room in internal buffer (should never happen).
//     * @throws IOException   if error writing file
//     */
//    private void splitFileOrig() throws EvioException, IOException {
//        // We should end the file with an empty block header.
//        if (eventsWrittenToBuffer > 0 || bytesWrittenToBuffer < 1) {
////System.out.println("    split file: write last header in old file, buf pos = " + buffer.position());
//            writeNewHeader(0, blockNumber, null,
//                           false, true,
//                           false, false);
//        }
//
//        // Close this file
////        fileCloser.closeFile(raf, lz4Out, fileOut);
//
//        // Name the next file
//        String nextFileName = Utilities.generateFileName(baseFileName, specifierCount,
//                                                         runNumber, split, splitCount++,
//                                                         streamId);
//        // Create the next file
//        if (compressedOutput) {
//            nextFileName += ".lz4";
//        }
//System.out.println("splitFile: creating next file, " + nextFileName);
//        currentFile = new File(nextFileName);
//
//        // If we can't overwrite and file exists ...
//        if (!overWriteOK && (currentFile.exists() && currentFile.isFile())) {
//            if (compressedOutput) {
////                lz4Out.close();
////                fileOut.close();
//            }
//            else {
//                raf.close();
//            }
//            throw new EvioException("File exists but user requested no over-writing, " +
//                                                   currentFile.getPath());
//        }
//
//        // Create the next file
//        if (compressedOutput) {
////            fileOut = new FileOutputStream(currentFile);
////            lz4Out = new LZ4BlockOutputStream(fileOut);
//        }
//        else {
//            raf  = new RandomAccessFile(currentFile, "rw");
//            fileChannel = raf.getChannel();
//        }
//
//        // Reset file values for reuse
//        blockNumber         = 1;
//        bytesWrittenToFile  = 0;
//        eventsWrittenToFile = 0;
//        wroteDictionary     = false;
//
////System.out.println("    splitFile: generated file name = " + fileName);
//    }



}
