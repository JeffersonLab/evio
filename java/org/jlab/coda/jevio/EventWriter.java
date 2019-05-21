/*
 * Copyright (c) 2018, Jefferson Science Associates, all rights reserved.
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 */

package org.jlab.coda.jevio;


import org.jlab.coda.hipo.*;

import java.io.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.AsynchronousFileChannel;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;
import java.util.ArrayList;
import java.util.BitSet;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

/**
 * An EventWriter object is used for writing events to a file or to a byte buffer.
 * This class does NOT write versions 1-4 data, only version 6!
 * This class is not thread-safe.
 *
 * <pre>
 *
 *            FILE Uncompressed
 *
 *    +----------------------------------+
 *    +                                  +
 *    +      General File Header         +
 *    +                                  +
 *    +----------------------------------+
 *    +----------------------------------+
 *    +                                  +
 *    +     Index Array (optional)       +
 *    +                                  +
 *    +----------------------------------+
 *    +----------------------------------+
 *    +      User Header (optional)      +
 *    +        --------------------------+
 *    +       |        Padding           +
 *    +----------------------------------+
 *    +----------------------------------+
 *    +                                  +
 *    +          Data Record 1           +
 *    +                                  +
 *    +----------------------------------+
 *                    .
 *                    .
 *                    .
 *    +----------------------------------+
 *    +                                  +
 *    +          Data Record N           +
 *    +                                  +
 *    +----------------------------------+
 *
 * ---------------------------------------------
 * ---------------------------------------------
 *
 *              FILE Compressed
 *
 *    +----------------------------------+
 *    +                                  +
 *    +      General File Header         +
 *    +                                  +
 *    +----------------------------------+
 *    +----------------------------------+
 *    +                                  +
 *    +     Index Array (optional)       +
 *    +                                  +
 *    +----------------------------------+
 *    +----------------------------------+
 *    +      User Header (optional)      +
 *    +        --------------------------+
 *    +       |         Padding          +
 *    +----------------------------------+
 *    +----------------------------------+
 *    +           Compressed             +
 *    +          Data Record 1           +
 *    +                                  +
 *    +----------------------------------+
 *                    .
 *                    .
 *                    .
 *    +----------------------------------+
 *    +           Compressed             +
 *    +          Data Record N           +
 *    +                                  +
 *    +----------------------------------+
 *
 *    The User Header contains a data record which
 *    holds the dictionary and first event, if any.
 *    The general file header, index array, and
 *    user header are never compressed.
 *
 *    Writing a buffer is done without the general file header
 *    and the index array and user header which follow.
 *    
 *
 * </pre>
 *
 * @author timmer
 */
final public class EventWriter implements AutoCloseable {

    /** Dictionary and first event are stored in user header part of file header.
     *  They're written as a record which allows multiple events. */
    private RecordOutputStream commonRecord;

    /** Record currently being filled. */
    private RecordOutputStream currentRecord;

    /** Record supply item from which current record comes from. */
    private RecordRingItem currentRingItem;

    /** Fast supply of record items for filling, compressing and writing. */
    private RecordSupply supply;

    /** Type of compression being done on data
     *  (0=none, 1=LZ4fastest, 2=LZ4best, 3=gzip). */
    private int compressionType;

    /** List of record length followed by count to be optionally written in trailer. */
    private ArrayList<Integer> recordLengths = new ArrayList<Integer>(1500);

    /** Number of uncompressed bytes written to file/buffer at current moment. */
    private long bytesWritten;

    /** Do we add a last header or trailer to file/buffer? */
    private boolean addingTrailer = true;

    /** Do we add a record index to the trailer? */
    private boolean addTrailerIndex;

    /** Byte array large enough to hold a header/trailer. */
    private byte[] headerArray = new byte[RecordHeader.HEADER_SIZE_BYTES];

    /** Byte array large enough to hold a header/trailer. */
    private ByteBuffer headerBuffer = ByteBuffer.wrap(headerArray);

    /** Threads used to compress data. */
    private RecordCompressor[] recordCompressorThreads;

    /** Thread used to write data to file/buffer. */
    private RecordWriter recordWriterThread;

    /** Number of records written to split-file/buffer at current moment. */
    private int recordsWritten;

    /** Running count of the record number. The next one to use starting with 1. */
    private int recordNumber;



    /**
     * Dictionary to include in xml format in the first event of the first record
     * when writing the file.
     */
    private String xmlDictionary;

    /** Byte array containing dictionary in evio format but <b>without</b> record header. */
    private byte[] dictionaryByteArray;

    /** Byte array containing firstEvent in evio format but <b>without</b> record header. */
    private byte[] firstEventByteArray;

    /** <code>True</code> if {@link #close()} was called, else <code>false</code>. */
    private boolean closed;

    /** <code>True</code> if writing to file, else <code>false</code>. */
    private boolean toFile;

    /** <code>True</code> if appending to file, <code>false</code> if (over)writing. */
    private boolean append;

    /** <code>True</code> if appending to file/buffer with dictionary, <code>false</code>. */
    private boolean hasAppendDictionary;

    /**
     * Total number of events written to buffer or file (although may not be flushed yet).
     * Will be the same as eventsWrittenToBuffer (- dictionary) if writing to buffer.
     * If the file being written to is split, this value refers to all split files
     * taken together. Does NOT include dictionary(ies).
     */
    private int eventsWrittenTotal;

    /** The byte order in which to write a file or buffer. */
    private ByteOrder byteOrder;

    //-----------------------
    // Buffer related members
    //-----------------------

    /** CODA id of buffer sender. */
    private int sourceId;

    /** Total size of the buffer in bytes. */
    private int bufferSize;

    /**
     * The output buffer when writing to a buffer.
     * The buffer internal to the currentRecord when writing to a file
     * and which is a reference to one of the internalBuffers.
     * When dealing with files, this buffer does double duty and is
     * initially used to read in record headers before appending data
     * to an existing file and such.
     */
    private ByteBuffer buffer;

    /** Two internal buffers, first element last used in the future1 write,
     * the second last used in future2 write. */
    private ByteBuffer[] usedBuffers;

    /** Three internal buffers used for writing to a file. */
    private ByteBuffer[] internalBuffers;

    /** Number of uncompressed bytes written to the current buffer. */
    private int bytesWrittenToBuffer;

    /** Number of bytes written to the current buffer for the common record. */
    private int commonRecordBytesToBuffer;

    /** Number of events written to final destination buffer or file's current record
     * NOT including dictionary (& first event?). */
    private int eventsWrittenToBuffer;

    //-----------------------
    // File related members
    //-----------------------

    /** Header for file only. */
    private FileHeader fileHeader;

    /** Header of file being appended to. */
    private FileHeader appendFileHeader;

    /** File currently being written to. */
    private File currentFile;

    /** Path object corresponding to file currently being written. */
    private Path currentFilePath;

    /** Objects to allow efficient, asynchronous file writing. */
    private Future<Integer> future1, future2;

    /** Index for selecting which future (1 or 2) to use for next file write. */
    private int futureIndex;

    /** The asynchronous file channel, used for writing a file. */
    private AsynchronousFileChannel asyncFileChannel;

    /** The location of the next write in the file. */
    private long fileWritingPosition;

    /** Split number associated with output file to be written next. */
    private int splitNumber;

    /** Number of split files produced by this writer. */
    private int splitCount;

    /** Part of filename without run or split numbers. */
    private String baseFileName;

    /** Number of C-style int format specifiers contained in baseFileName. */
    private int specifierCount;

    /** Run number possibly used in naming split files. */
    private int runNumber;

    /**
     * Do we split the file into several smaller ones (val > 0)?
     * If so, this gives the maximum number of bytes to make each file in size.
     */
    private long split;

    /**
     * If splitting file, the amount to increment the split number each time another
     * file is written.
     */
    private int splitIncrement;

    /**
     * Id of this specific data stream.
     * In CODA, a data stream is a chain of ROCS and EBs ending in a single specific ER.
     */
    private int streamId;

    /** The total number of data streams in DAQ. */
    private int streamCount;

    /** Writing to file with single thread? */
    private boolean singleThreadedCompression;

    /** Is it OK to overwrite a previously existing file? */
    private boolean overWriteOK;

    /** Number of bytes written to the current file (including ending header),
     *  not the total in all split files. */
    private long bytesWrittenToFile;

    /** Number of events actually written to the current file - not the total in
     * all split files - including dictionary. */
    private int eventsWrittenToFile;

    /** Does file have a trailer with record indexes? */
    private boolean hasTrailerWithIndex;

    /** File header's user header length in bytes. */
    private int userHeaderLength;

    /** File header's user header's padding in bytes. */
    private int userHeaderPadding;

    /** File header's index array length in bytes. */
    private int indexLength;

    //-----------------------


    /** Class used to close files in order received, each in its own thread,
     *  to avoid slowing down while file splitting. */
    private final class FileCloser {

        /** Thread pool with 1 thread. */
        private final ExecutorService threadPool;

        FileCloser() {threadPool = Executors.newSingleThreadExecutor();}

        /** Close the thread pool in this object while executing all existing tasks. */
        void close() {threadPool.shutdown();}

        /**
         * Close the given file, in the order received, in a separate thread.
         * @param afc file channel to close
         */
        void closeAsyncFile(AsynchronousFileChannel afc) {
            threadPool.submit(new CloseAsyncFChan(afc));
        }


        private class CloseAsyncFChan implements Runnable {
            private AsynchronousFileChannel afc;

            CloseAsyncFChan(AsynchronousFileChannel afc) {this.afc = afc;}

            public void run() {
                // Finish writing to current file
                if (future1 != null) {
                    try {
                        // Wait for last write to end before we continue
                        future1.get();
                    }
                    catch (Exception e) {}
                }

                if (future2 != null) {
                    try {
                        future2.get();
                    }
                    catch (Exception e) {}
                }

                try {
                    afc.close();
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
        this(file, null, append);
    }

    /**
     * Creates an <code>EventWriter</code> for writing to a file in NATIVE byte order.
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
        this(file.getPath(), null, null,
             0, 0, 0, 0,
             ByteOrder.nativeOrder(), dictionary, false,
             append, null, 0, 0, 1, 1,
             0, 1, 8);

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
     * Creates an <code>EventWriter</code> for writing to a file in NATIVE byte order.
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
        this(filename, append, ByteOrder.nativeOrder());
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
        this(filename, null, null,
             0, 0, 0, 0,
             byteOrder, null, false,
             append, null, 0, 0, 1, 1,
             0, 1, 8);
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
     * If streamCount &gt; 1, the split number is calculated starting with streamId and incremented
     * by streamCount each time. In this manner, all split files will have unique, sequential
     * names even though there are multiple parallel ERs.
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
     * they can be difcompressionferentiated by a stream id, starting split # and split increment.
     *
     * @param baseName      base file name used to generate complete file name (may not be null)
     * @param directory     directory in which file is to be placed
     * @param runType       name of run type configuration to be used in naming files
     * @param runNumber     number of the CODA run, used in naming files
     * @param split         if &lt; 1, do not split file, write to only one file of unlimited size.
     *                      Else this is max size in bytes to make a file
     *                      before closing it and starting writing another.
     * @param maxRecordSize max number of uncompressed data bytes each record can hold.
     *                      Value of &lt; 8MB results in default of 8MB.
     *                      The size of the record will not be larger than this size
     *                      unless a single event itself is larger.
     * @param maxEventCount max number of events each record can hold.
     *                      Value &lt;= O means use default (1M).
     * @param byteOrder     the byte order in which to write the file. This is ignored
     *                      if appending to existing file. Defaults to Big Endian if null.
     * @param xmlDictionary dictionary in xml format or null if none.
     * @param overWriteOK   if <code>false</code> and the file already exists,
     *                      an exception is thrown rather than overwriting it.
     * @param append        if <code>true</code> append written data to given file.
     * @param firstEvent    the first event written into each file (after any dictionary)
     *                      including all split files; may be null. Useful for adding
     *                      common, static info into each split file.
     * @param streamId      streamId number (100 > id > -1) for file name
     * @param splitNumber   number at which to start the split numbers
     * @param splitIncrement amount to increment split number each time another file is created.
     * @param streamCount    total number of streams in DAQ.
     * @param compressionType    type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip).
     * @param compressionThreads number of threads doing compression simultaneously.
     * @param ringSize           number of records in supply ring. If set to &lt; compressionThreads,
     *                           it is forced to equal that value and is also forced to be a multiple of
     *                           2, rounded up.
     *
     * @throws EvioException if maxRecordSize or maxEventCount exceed limits;
     *                       if streamCount &gt; 1 and streamId &lt; 0;
     *                       if defined dictionary or first event while appending;
     *                       if splitting file while appending;
     *                       if file name arg is null;
     *                       if file could not be opened, positioned, or written to;
     *                       if file exists but user requested no over-writing or appending;
     *                       if streamId < 0, splitNumber < 0, or splitIncrement < 1.
     */
    public EventWriter(String baseName, String directory, String runType,
                       int runNumber, long split,
                       int maxRecordSize, int maxEventCount,
                       ByteOrder byteOrder, String xmlDictionary,
                       boolean overWriteOK, boolean append,
                       EvioBank firstEvent, int streamId,
                       int splitNumber, int splitIncrement, int streamCount,
                       int compressionType, int compressionThreads, int ringSize)
            throws EvioException {

        if (baseName == null) {
            throw new EvioException("baseName arg is null");
        }

        if (streamId < 0 || splitNumber < 0 || splitIncrement < 1) {
            throw new EvioException("streamId < 0, splitNumber < 0, or splitIncrement < 1");
        }

        if (runNumber < 1) {
            runNumber = 1;
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

        // Store arguments
        this.split          = split;
        this.append         = append;
        this.runNumber      = runNumber;
        this.byteOrder      = byteOrder;   // byte order may be overwritten if appending
        this.overWriteOK    = overWriteOK;
        this.xmlDictionary  = xmlDictionary;
        this.streamId       = streamId;
        this.splitNumber    = splitNumber;
        this.splitIncrement = splitIncrement;
        this.streamCount    = streamCount;

        // Only add trailer index if writing file
        addTrailerIndex = true;

        // compressionType = 0 before creating commonRecord, so NO compression in common record.
        // But be sure byteOrder is set so commonRecord has the correct byteOrder.

        if (xmlDictionary != null || firstEvent != null) {
            // Create the common record here, but don't write it to file
            // until the file header is written to the file in writeFileHeader()
            // which in turn is written by writeToFile() which is only called
            // right after a file is created.
            createCommonRecord(xmlDictionary, firstEvent, null, null);
        }

        // Don't compress if bad value
        if (compressionType < 0 || compressionType > 3) {
            compressionType = 0;
        }
        this.compressionType = compressionType;

        if (compressionThreads < 1) {
            compressionThreads = 1;
        }

        toFile = true;
        recordNumber = 1;
System.out.println("EventWriterUnsync constr: record # set to 1");

        // The following may not be backwards compatible.
        // Make substitutions in the baseName to create the base file name.
        if (directory != null) baseName = directory + "/" + baseName;
        StringBuilder builder = new StringBuilder(100);
        int[] returnVals  = Utilities.generateBaseFileName(baseName, runType, builder);
        specifierCount    = returnVals[0];
        baseFileName      = builder.toString();
        // Also create the first file's name with more substitutions
        String fileName   = Utilities.generateFileName(baseFileName, specifierCount,
                                                       runNumber, split, splitNumber,
                                                       streamId, streamCount);
        // All subsequent split numbers are calculated by adding the splitIncrement
        this.splitNumber += splitIncrement;

        currentFilePath = Paths.get(fileName);
        currentFile = currentFilePath.toFile();

        // If we can't overwrite or append and file exists, throw exception
        if (!overWriteOK && !append && (currentFile.exists() && currentFile.isFile())) {
            throw new EvioException("File exists but user requested no over-writing or appending, "
                                            + currentFile.getPath());
        }

        // Create internal storage buffers.
        // Since we're doing I/O to a file, a direct buffer is more efficient.
        // Besides, the JVM will convert a non-direct to a direct one anyway
        // when doing the I/O (and copy all that data again).
        // Make sure these are DIRECT buffers or performance suffers!
        // The reason there are 3 internal buffers is that we'll be able to
        // do 2 asynchronous writes at once will still filling up the third
        // simultaneously.
        // One downside of the following constructor of currentRecord (supplying
        // an external buffer), is that trying to write a single event of over
        // 9MB will fail. C. Timmer
        bufferSize = 9437184; // 9MB so it's consistent with RecordOutputStream default
        usedBuffers     = new ByteBuffer[2];
        internalBuffers = new ByteBuffer[3];
        internalBuffers[0] = ByteBuffer.allocateDirect(bufferSize);
        internalBuffers[1] = ByteBuffer.allocateDirect(bufferSize);
        internalBuffers[2] = ByteBuffer.allocateDirect(bufferSize);
        internalBuffers[0].order(byteOrder);
        internalBuffers[1].order(byteOrder);
        internalBuffers[2].order(byteOrder);
        headerBuffer.order(byteOrder);
        buffer = internalBuffers[0];

        // Evio file
        fileHeader = new FileHeader(true);

        // Compression threads
        if (compressionThreads == 1) {
            // When writing single threaded, just fill/compress/write one record at a time
            singleThreadedCompression = true;
            currentRecord = new RecordOutputStream(buffer, maxEventCount,
                                                   compressionType, HeaderType.EVIO_RECORD);
        }
        else {
            // Number of ring items must be >= compressionThreads
            if (ringSize < compressionThreads) {
                ringSize = compressionThreads;
            }
            // AND must be multiple of 2
            ringSize = Utilities.powerOfTwo(ringSize, true);

            supply = new RecordSupply(ringSize, byteOrder, compressionThreads,
                                      maxEventCount, maxRecordSize, compressionType);

            // Create and start compression threads
            recordCompressorThreads = new RecordCompressor[compressionThreads];
            for (int i = 0; i < compressionThreads; i++) {
                recordCompressorThreads[i] = new RecordCompressor(i);
                recordCompressorThreads[i].start();
            }

            // Create and start writing thread
            recordWriterThread = new RecordWriter();
            recordWriterThread.start();

            // Get a single blank record to start writing into
            currentRingItem = supply.get();
            currentRecord   = currentRingItem.getRecord();
        }

        // Object to close files in a separate thread when splitting, to speed things up
        if (split > 0) fileCloser = new FileCloser();

        if (append) {
            try {
                // For reading existing file and preparing for stream writes
                asyncFileChannel = AsynchronousFileChannel.open(currentFilePath,
                                                                StandardOpenOption.READ,
                                                                StandardOpenOption.WRITE);

                // If we have an empty file, that's OK.
                // Otherwise we have to examine it for compatibility
                // and position ourselves for the first write.
                if (asyncFileChannel.size() > 0) {
                    // Look at first record header to find endianness & version.
                    // Endianness given in constructor arg, when appending, is ignored.
                    examineFirstRecordHeader();

                    // Oops, gotta redo this since file has different byte order
                    // than specified in constructor arg.
                    if (this.byteOrder != byteOrder) {
                        byteOrder = this.byteOrder;
                        internalBuffers[0].order(byteOrder);
                        internalBuffers[1].order(byteOrder);
                        internalBuffers[2].order(byteOrder);
                    }

                    // Prepare for appending by moving file position to end of last record
                    toAppendPosition();

                    // File position is now after the last event written.

                    // Reset the buffer which has been used to read the header
                    // and to prepare the file for event writing.
                    buffer.clear();
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
        }
    }


    //---------------------------------------------
    // BUFFER Constructors
    //---------------------------------------------

    /**
     * Create an <code>EventWriter</code> for writing events to a ByteBuffer.
     * Uses the default number and size of records in buffer.
     * Will overwrite any existing data in buffer!
     *
     * @param buf            the buffer to write to.
     * @throws EvioException if buf arg is null
     */
    public EventWriter(ByteBuffer buf) throws EvioException {

        this(buf, 0, 0, null, 1, null, 0);
    }

    /**
     * Create an <code>EventWriter</code> for writing events to a ByteBuffer.
     * Uses the default number and size of records in buffer.
     *
     * @param buf            the buffer to write to.
     * @param xmlDictionary  dictionary in xml format or null if none.
     * @throws EvioException if buf arg is null
     */
    public EventWriter(ByteBuffer buf, String xmlDictionary) throws EvioException {

        this(buf, 0, 0, xmlDictionary, 1, null, 0);
    }


    /**
     * Create an <code>EventWriter</code> for writing events to a ByteBuffer.
     * The buffer's position is set to 0 before writing.
     *
     * @param buf             the buffer to write to starting at position = 0.
     * @param maxRecordSize   max number of data bytes each record can hold.
     *                        Value of &lt; 8MB results in default of 8MB.
     *                        The size of the record will not be larger than this size
     *                        unless a single event itself is larger.
     * @param maxEventCount   max number of events each record can hold.
     *                        Value &lt;= O means use default (1M).
     * @param xmlDictionary   dictionary in xml format or null if none.
     * @param recordNumber    number at which to start record number counting.
     * @param firstEvent      the first event written into the buffer (after any dictionary).
     *                        May be null. Not useful when writing to a buffer as this
     *                        event may be written using normal means.
     * @param compressionType type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip)
     *
     * @throws EvioException if maxRecordSize or maxEventCount exceed limits;
     *                       if buf arg is null;
     */
    public EventWriter(ByteBuffer buf, int maxRecordSize, int maxEventCount,
                       String xmlDictionary, int recordNumber,
                       EvioBank firstEvent, int compressionType)
            throws EvioException {

        if (buf == null) {
            throw new EvioException("Buffer arg cannot be null");
        }

        this.toFile          = false;
        this.append          = false;
        this.buffer          = buf;
        this.byteOrder       = buf.order();
        this.recordNumber    = recordNumber;
System.out.println("EventWriter constr: record # set to " + recordNumber);

        this.xmlDictionary   = xmlDictionary;
        this.compressionType = compressionType;

        // Get buffer ready for writing
        buffer.clear();
        bufferSize = buf.capacity();
        headerBuffer.order(byteOrder);

        // Write any record containing dictionary and first event, first
        if (xmlDictionary != null || firstEvent != null) {
            createCommonRecord(xmlDictionary, firstEvent, null, null);
        }

        // When writing to buffer, just fill/compress/write one record at a time
        currentRecord = new RecordOutputStream(buf, maxEventCount,
                                               compressionType,
                                               HeaderType.EVIO_RECORD);

        RecordHeader header = currentRecord.getHeader();
        header.setBitInfo(false, firstEvent != null, xmlDictionary != null);
    }


    /**
     * Initialization new buffer (not from constructor).
     * The buffer's position is set to 0 before writing.
     * Only called by {@link #setBuffer(ByteBuffer)} and
     * {@link #setBuffer(ByteBuffer, BitSet, int)}.
     *
     * @param buf            the buffer to write to.
     * @param bitInfo        set of bits to include in first record header.
     * @param recordNumber   number at which to start record number counting.
     * @param useCurrentBitInfo   regardless of bitInfo arg's value, use the
     *                            current value of bitInfo in the reinitialized buffer.
     */
    private void reInitializeBuffer(ByteBuffer buf, BitSet bitInfo, int recordNumber,
                                    boolean useCurrentBitInfo) {

        this.buffer       = buf;
        this.byteOrder    = buf.order();
        this.recordNumber = recordNumber;
//System.out.println("reInitializeBuffer: record # -> " + recordNumber);

        // Init variables
        split  = 0L;
        toFile = false;
        closed = false;
        eventsWrittenTotal = 0;
        eventsWrittenToBuffer = 0;
        bytesWrittenToBuffer = 0;
//        System.out.println("** reInitializeBuffer: bytesWrittenToBuffer --> 0");
        bytesWritten = 0L;
        headerBuffer.order(byteOrder);
        buffer.clear();
        bufferSize = buffer.capacity();

        // Deal with bitInfo
        RecordHeader header = currentRecord.getHeader();
        int oldBitInfoWord = header.getBitInfoWord();

        // This will reset the record - header and all buffers (including buf)
        currentRecord.setBuffer(buffer);

//        System.out.println("reInitializeBuffer 1: header ->\n" + header.toString());
        if (useCurrentBitInfo) {
            header.setBitInfoWord(oldBitInfoWord);
        }
        else {
//            try {
//System.out.println("reInitializeBuffer: set bitInfo to" + bitInfo);
                header.setBitInfoWord(bitInfo);
//            }
//            catch (HipoException e) {/* do nothing if problem (bitInfo == null) */}
        }
//System.out.println("reInitializeBuffer: after reset, record # -> " + recordNumber);

        // Only necessary to do this when using EventWriter in EMU's
        // RocSimulation module. Only the ROC sends sourceId in header.
        header.setUserRegisterFirst(sourceId);
//        System.out.println("reInitializeBuffer 2: header ->\n" + header.toString());
    }


    /**
     * Set the buffer being written into (initially set in constructor).
     * This method allows the user to avoid having to create a new EventWriter
     * each time a bank needs to be written to a different buffer.
     * This does nothing if writing to a file.<p>
     * Do <b>not</b> use this method unless you know what you are doing.
     *
     * @param buf the buffer to write to.
     * @param bitInfo        set of bits to include in first record header.
     * @param recordNumber   number at which to start record number counting.
     * @throws EvioException if this object was not closed prior to resetting the buffer,
     *                       buffer arg is null, or writing to file.
     */
    public void setBuffer(ByteBuffer buf, BitSet bitInfo, int recordNumber) throws EvioException {
        if (toFile) return;
        if (buf == null) {
            throw new EvioException("Buffer arg null");
        }
        if (!closed) {
            throw new EvioException("Close EventWriter before changing buffers");
        }

//System.out.println("setBuffer: call reInitializeBuf with INPUT record # " + recordNumber);
        reInitializeBuffer(buf, bitInfo, recordNumber, false);
    }


    /**
     * Set the buffer being written into (initially set in constructor).
     * This method allows the user to avoid having to create a new EventWriter
     * each time a bank needs to be written to a different buffer.
     * This does nothing if writing to a file.<p>
     * Do <b>not</b> use this method unless you know what you are doing.
     *
     * @param buf the buffer to write to.
     * @throws EvioException if this object was not closed prior to resetting the buffer,
     *                       buffer arg is null, or writing to file.
     */
    public void setBuffer(ByteBuffer buf) throws EvioException {
        if (toFile) return;
        if (buf == null) {
            throw new EvioException("Buffer arg null");
        }
        if (!closed) {
            throw new EvioException("Close EventWriter before changing buffers");
        }

//System.out.println("setBuffer: call reInitializeBuf with local record # " + recordNumber);
        reInitializeBuffer(buf, null, recordNumber, true);
    }


    /**
     * Get the buffer being written into.
     * If writing to a buffer, this was initially supplied by user in constructor.
     * If writing to a file, return null.
     * Although this method may seems useful, it requires a detailed knowledge of
     * this class's internals. The {@link #getByteBuffer()} method is much more
     * useful to the user.
     *
     * @return buffer being written into
     */
    private ByteBuffer getBuffer() {
        if (toFile()) return null;
        return buffer;
    }


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
     * Set the value of the source Id in the first block header.
     * Only necessary to do this when using EventWriter in EMU's
     * RocSimulation module. Only the ROC sends sourceId in header.
     * In evio 6, the source id is stored in user register 1. In
     * earlier versions it's stored in reserved1.
     * This should only be used internally by CODA in emu software.
     *
     * @param sourceId  value of the source Id.
     */
    public void setSourceId(int sourceId) {
        this.sourceId = sourceId;
        RecordHeader header = currentRecord.getHeader();
        header.setUserRegisterFirst(sourceId);
    }


    /**
     * Set the bit info of a record header for a specified CODA event type.
     * Must be called AFTER {@link RecordHeader#setBitInfo(boolean, boolean, boolean)} or
     * {@link RecordHeader#setBitInfoWord(int)} in order to have change preserved.
     * This should only be used internally by CODA in emu software.
     *
     * @param type event type (0=ROC raw, 1=Physics, 2=Partial Physics,
     *             3=Disentangled, 4=User, 5=Control, 15=Other,
     *             else = nothing set).
     */
    public void setEventType(int type) {
        RecordHeader header = currentRecord.getHeader();
        header.setBitInfoEventType(type);
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
     * If writing to a buffer, get the number of bytes written to it
     * including the trailer.
     * @return number of bytes written to buffer
     */
    public int getBytesWrittenToBuffer() {return bytesWrittenToBuffer;}


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
     * Get the current split number which is the split number of file
     * to be written next. Warning, this value may be changing.
     * @return the current split number which is the split number of file to be written next.
     */
    public int getSplitNumber() {return splitNumber;}


    /**
     * Get the number of split files produced by this writer.
     * @return number of split files produced by this writer.
     */
    public int getSplitCount() {return splitCount;}


    /**
     * Get the current block (record) number.
     * Warning, this value may be changing.
     * @return the current block (record) number.
     */
    public int getBlockNumber() {return recordNumber;}


    /**
     * Get the current record number.
     * Warning, this value may be changing.
     * @return the current record number.
     */
    public int getRecordNumber() {return recordNumber;}


    /**
     * Get the number of events written to a file/buffer.
     * Remember that a particular event may not yet be
     * flushed to the file/buffer.
     * If the file being written to is split, the returned
     * value refers to all split files taken together.
     *
     * @return number of events written to a file/buffer.
     */
    public int getEventsWritten() {
//        System.out.println("getEventsWritten: eventsWrittenTotal = " + eventsWrittenTotal +
//                ", curRec.getEvCount = " + currentRecord.getEventCount());
        return eventsWrittenTotal + currentRecord.getEventCount();
    }


    /**
     * Get the byte order of the buffer/file being written into.
     * @return byte order of the buffer/file being written into.
     */
    public ByteOrder getByteOrder() {return byteOrder;}


    /**
     * Set the number with which to start block (record) numbers.
     * This method does nothing if events have already been written.
     * @param startingRecordNumber  the number with which to start block
     *                              (record) numbers.
     */
    public void setStartingBlockNumber(int startingRecordNumber) {
        // If events have been written already, forget about it
        if (eventsWrittenTotal > 0) return;
        recordNumber = startingRecordNumber;
System.out.println("setStartingBLOCKNumber: set to " + recordNumber);
    }


    /**
     * Set the number with which to start record numbers.
     * This method does nothing if events have already been written.
     * @param startingRecordNumber  the number with which to start record numbers.
     */
    public void setStartingRecordNumber(int startingRecordNumber) {
        // If events have been written already, forget about it
        if (eventsWrittenTotal > 0) return;
        recordNumber = startingRecordNumber;
System.out.println("setStartingRecordNumber: set to " + recordNumber);
    }


    /**
     * Set an event which will be written to the file as
     * well as to all split files. It's called the "first event" as it will be the
     * first event written to each split file if this method
     * is called early enough or the first event was defined in the constructor.
     * In evio version 6, any dictionary and the first event are written to a
     * common record which is stored in the user-header part of the file header if
     * writing to a file. When writing to a buffer it's stored in the first record's
     * user-header. The common record data is never compressed.<p>
     *     
     * <b>FILE:</b>
     * Since this method can only be called after the constructor, the common record may
     * have already been written with its dictionary and possibly another firstEvent.
     * If that is the case, the event given here will be written immediately somewhere
     * in the body of the file. Any subsequent splits will have this event as the first
     * event in the file header. On the other hand, if the common record has not yet been
     * written to the file, this event becomes the first event in the file header.<p>
     *
     * <b>BUFFER:</b>
     * By its nature this method is not all that useful for writing to a buffer since
     * the buffer is never split. Writing this event is done by storing the common record
     * in the main record's user-header. When writing to a buffer, the common record is not
     * written until main buffer is full and flushCurrentRecordToBuffer() is called. That is not
     * done until close() or flush() is called. In other words, there is still time to change
     * the common record up until close is called.<p>
     *
     * @param node node representing event to be placed first in each file written
     *             including all splits. If null, no more first events are written
     *             to any files.
     * @throws IOException   if error writing to file
     * @throws EvioException if first event is opposite byte order of internal buffer;
     *                       if bad data format;
     *                       if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    synchronized public void setFirstEvent(EvioNode node) throws EvioException, IOException {

        if (closed) {return;}

        // If there is no common record now ...
        if (node == null && xmlDictionary == null) {
            commonRecord = null;
            return;
        }

        // There's no way to remove an event from a record, so reconstruct it.
        createCommonRecord(xmlDictionary, null, node, null);

        // When writing to a buffer, the common record is not written until
        // buffer is full and flushCurrentRecordToBuffer() is called. That
        // is not done until close() or flush() is called. In other words,
        // there is still time to change the common record.

        if (toFile && (recordsWritten > 0) && (node != null)) {
            // If we've already written the file header, it's too late to place
            // the common record there, so write first event as a regular event.
            // The new common record will be written to the file header of the
            // next split.
            writeEvent(node, false);
        }
    }

    
    /**
     * Set an event which will be written to the file as
     * well as to all split files. It's called the "first event" as it will be the
     * first event written to each split file if this method
     * is called early enough or the first event was defined in the constructor.
     * In evio version 6, any dictionary and the first event are written to a
     * common record which is stored in the user-header part of the file header if
     * writing to a file. When writing to a buffer it's stored in the first record's
     * user-header. The common record data is never compressed.<p>
     *
     * <b>FILE:</b>
     * Since this method can only be called after the constructor, the common record may
     * have already been written with its dictionary and possibly another firstEvent.
     * If that is the case, the event given here will be written immediately somewhere
     * in the body of the file. Any subsequent splits will have this event as the first
     * event in the file header. On the other hand, if the common record has not yet been
     * written to the file, this event becomes the first event in the file header.<p>
     *
     * <b>BUFFER:</b>
     * By its nature this method is not all that useful for writing to a buffer since
     * the buffer is never split. Writing this event is done by storing the common record
     * in the main record's user-header. When writing to a buffer, the common record is not
     * written until main buffer is full and flushCurrentRecordToBuffer() is called. That is not
     * done until close() or flush() is called. In other words, there is still time to change
     * the common record up until close is called.<p>
     *
     * @param buffer buffer containing event to be placed first in each file written
     *               including all splits. If null, no more first events are written
     *               to any files.
     * @throws IOException   if error writing to file
     * @throws EvioException if first event is opposite byte order of internal buffer;
     *                       if bad data format;
     *                       if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    synchronized public void setFirstEvent(ByteBuffer buffer) throws EvioException, IOException {

        if (closed) {return;}

        if ((buffer == null || buffer.remaining() < 8) && xmlDictionary == null) {
            commonRecord = null;
            return;
        }

        createCommonRecord(xmlDictionary, null, null, buffer);

        if (toFile && (recordsWritten > 0) && (buffer != null) && (buffer.remaining() > 7)) {
            writeEvent(buffer, false);
        }
    }


    /**
     * Set an event which will be written to the file as
     * well as to all split files. It's called the "first event" as it will be the
     * first event written to each split file if this method
     * is called early enough or the first event was defined in the constructor.
     * In evio version 6, any dictionary and the first event are written to a
     * common record which is stored in the user-header part of the file header if
     * writing to a file. When writing to a buffer it's stored in the first record's
     * user-header. The common record data is never compressed.<p>
     *
     * <b>FILE:</b>
     * Since this method can only be called after the constructor, the common record may
     * have already been written with its dictionary and possibly another firstEvent.
     * If that is the case, the event given here will be written immediately somewhere
     * in the body of the file. Any subsequent splits will have this event as the first
     * event in the file header. On the other hand, if the common record has not yet been
     * written to the file, this event becomes the first event in the file header.<p>
     *
     * <b>BUFFER:</b>
     * By its nature this method is not all that useful for writing to a buffer since
     * the buffer is never split. Writing this event is done by storing the common record
     * in the main record's user-header. When writing to a buffer, the common record is not
     * written until main buffer is full and flushCurrentRecordToBuffer() is called. That is not
     * done until close() or flush() is called. In other words, there is still time to change
     * the common record up until close is called.<p>
     *
     * @param bank event to be placed first in each file written including all splits.
     *             If null, no more first events are written to any files.
     * @throws IOException   if error writing to file
     * @throws EvioException if first event is opposite byte order of internal buffer;
     *                       if bad data format;
     *                       if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    synchronized public void setFirstEvent(EvioBank bank) throws EvioException, IOException {

        if (closed) {return;}

        if (bank == null && xmlDictionary == null) {
            commonRecord = null;
            return;
        }

        createCommonRecord(xmlDictionary, bank, null, null);

        if (toFile && (recordsWritten > 0) && (bank != null)) {
            writeEvent(bank, null, false);
        }
    }


    /**
     * Create and fill the common record which contains the dictionary and first event.
     * Use the firstBank as the first event if specified, else try using the
     * firstNode if specified, else try the firstBuf.
     *
     * @param xmlDictionary  xml dictionary
     * @param firstBank      first event as EvioBank
     * @param firstNode      first event as EvioNode
     * @param firstBuf       first event as ByteBuffer
     * @throws EvioException if dictionary is in improper format
     */
    private void createCommonRecord(String xmlDictionary, EvioBank firstBank,
                                    EvioNode firstNode, ByteBuffer firstBuf)
                            throws EvioException {

        // Create record if necessary, else clear it
        if (commonRecord == null) {
            // No compression please ...
            commonRecord = new RecordOutputStream(byteOrder, 0, 0, 0);
        }
        else {
            commonRecord.reset();
        }

        // Place dictionary & first event into a single record
        if (xmlDictionary != null) {
            // 56 is the minimum number of characters for a valid xml dictionary
            if (xmlDictionary.length() < 56) {
                throw new EvioException("Dictionary improper format");
            }

            try {
                // Turn dictionary data into ascii (not evio bank)
                dictionaryByteArray = xmlDictionary.getBytes("US-ASCII");
            } catch (UnsupportedEncodingException e) {/* never happen */}

            // Add to record which will be our file header's "user header"
            commonRecord.addEvent(dictionaryByteArray);
        }
        else {
            dictionaryByteArray = null;
        }

        // Convert first event into bytes
        if (firstBank != null) {
            firstEventByteArray = Utilities.bankToBytes(firstBank, byteOrder);
            // Add to record which will be our file header's "user header"
            commonRecord.addEvent(firstEventByteArray);
        }
        else if (firstNode != null) {
            ByteBuffer firstEventBuf = firstNode.getStructureBuffer(true);
            firstEventByteArray = firstEventBuf.array();
            commonRecord.addEvent(firstEventByteArray);
        }
        else if (firstBuf != null) {
            firstEventByteArray = ByteDataTransformer.toByteArray(firstBuf);
            commonRecord.addEvent(firstEventByteArray);
        }
        else {
            firstEventByteArray = null;
        }

        commonRecord.build();
        commonRecordBytesToBuffer = 4*commonRecord.getHeader().getLengthWords();
    }


    /**
     * Create and write a general file header into the file.
     * The general header's user header is the common record which
     * contains any existing dictionary and first event.<p>
     *
     * Call this method AFTER file split or, in constructor, after the file
     * name is created in order to ensure a consistent value for file split number.
     */
    private void writeFileHeader() {
        // For the file header, our "user header" will be the common record,
        // which is a record containing the dictionary and first event.

        fileHeader.reset();
        // File split # in header. Go back to last one as currently is set for the next split.
        fileHeader.setFileNumber(splitNumber - splitIncrement);

        // Check to see if we have dictionary and/or first event
        int commonRecordBytes = 0;
        int commonRecordCount = 0;

        if (commonRecord != null) {
            commonRecordCount = commonRecord.getEventCount();
            if (commonRecordCount > 0) {
                commonRecordBytes = commonRecord.getHeader().getLength();
                boolean haveDict = dictionaryByteArray != null;
                boolean haveFE   = firstEventByteArray != null;
                fileHeader.setBitInfo(haveFE, haveDict, false);
            }
            // Sets file header length too
            fileHeader.setUserHeaderLength(commonRecordBytes);
        }

        // Index array is unused

        // Total header size in bytes
        int bytes = fileHeader.getLength();
        byte[] array = new byte[bytes];
        ByteBuffer buf = ByteBuffer.wrap(array);
        buf.order(byteOrder);

        // Write file header into array
        try {
            fileHeader.writeHeader(buf, 0);
        }
        catch (HipoException e) {/* never happen */}
        
        // Write user header into array if necessary
        if (commonRecordBytes > 0) {
            ByteBuffer commonBuf = commonRecord.getBinaryBuffer();
            byte[] commonArray = commonBuf.array();
            if (commonArray != null) {
                System.arraycopy(commonArray, commonBuf.arrayOffset(),
                                 array, RecordHeader.HEADER_SIZE_BYTES, commonRecordBytes);
            }
            else {
                commonBuf.get(array, RecordHeader.HEADER_SIZE_BYTES, commonRecordBytes);
            }
        }

        // Write array into file
        asyncFileChannel.write(buf, 0);

        eventsWrittenTotal = eventsWrittenToFile = commonRecordCount;
        bytesWrittenToFile = bytesWritten = bytes;
        fileWritingPosition += bytes;
    }


    /**
     * This method flushes any remaining internally buffered data to file.
     * Calling {@link #close()} automatically does this so it isn't necessary
     * to call before closing. This method should only be used when writing
     * events at such a low rate that it takes an inordinate amount of time
     * for internally buffered data to be written to the file.<p>
     *
     * Calling this may easily kill performance.
     */
    synchronized public void flush() {

        if (closed) {
            return;
        }

        if (toFile) {
            if (singleThreadedCompression) {
                try {
                    compressAndWriteToFile(true);
                }
                catch (Exception e) {
                    e.printStackTrace();
                }
            }
            else {
                // Write any existing data.
                currentRingItem.forceToDisk(true);
                if (currentRecord.getEventCount() > 0) {
                    // Send current record back to ring
                    supply.publish(currentRingItem);
                }

                // Get another empty record from ring
                currentRingItem = supply.get();
                currentRecord = currentRingItem.getRecord();
            }
        }
        else {
//System.out.println("flush(): call flushRecordToBuffer");
            flushCurrentRecordToBuffer();
        }
    }


    /**
     * This method flushes any remaining data to file and disables this object.
     */
    synchronized public void close() {
        if (closed) {
            return;
        }

        // If buffer ...
        if (!toFile) {
            flushCurrentRecordToBuffer();
            // Write empty last header
            try {
                writeTrailerToBuffer(addTrailerIndex);
            }
            catch (EvioException e) {
                // We're here if buffer is too small
                e.printStackTrace();
            }
        }
        // If file ...
        else {
            // Write record to file
            if (singleThreadedCompression) {
                try {compressAndWriteToFile(false);}
                catch (Exception e) {
                    e.printStackTrace();
                }
            }
            else {
                // If we're building a record, send it off
                // to compressing thread since we're done.
                if (currentRecord.getEventCount() > 0) {
                    // Put it back in supply for compressing
                    supply.publish(currentRingItem);
                }

                // Since the writer thread is the last to process each record,
                // wait until it's done with the last item, then exit the thread.
                recordWriterThread.waitForLastItem();

                // Stop all compressing threads which by now are stuck on get
                for (RecordCompressor rc : recordCompressorThreads) {
                    rc.interrupt();
                }
            }

            // Write trailer
            if (asyncFileChannel != null) {
                if (addingTrailer) {
                    // Write the trailer
                    try {
                        writeTrailerToFile(addTrailerIndex);
                    }
                    catch (IOException e) {
                        e.printStackTrace();
                    }
                }

                try {
                    // Find & update file header's record count word
                    ByteBuffer bb = ByteBuffer.allocate(4);
                    bb.order(byteOrder);
                    bb.putInt(0, recordNumber - 1);
                    Future future = asyncFileChannel.write(bb, FileHeader.RECORD_COUNT_OFFSET);
                    future.get();
                }
                catch (Exception e) {
                    e.printStackTrace();
                }
            }

            // Finish writing to current file
            try {
                if (future1 != null) {
                    try {
                        // Wait for last write to end before we continue
                        future1.get();
                    }
                    catch (Exception e) {}
                }

                if (future2 != null) {
                    try {
                        future2.get();
                    }
                    catch (Exception e) {}
                }

                if (asyncFileChannel != null) asyncFileChannel.close();
                // Close split file handler thread pool
                if (fileCloser != null) fileCloser.close();
            }
            catch (IOException e) {}
        }

        recordLengths.clear();
        closed = true;
    }

    
    /**
     * Reads part of the first file header in order to determine
     * the evio version # and endianness of the file in question.
     *
     * @throws EvioException not in append mode, contains too little data, is not in proper format,
     *                       version earlier than 6, and all other exceptions.
     * @throws IOException   premature EOF or file reading error.
     */
    protected void examineFirstRecordHeader() throws IOException, EvioException {

        // Only for append mode - only used for files
        if (!append) {
            // Internal logic error, should never have gotten here
            throw new EvioException("need to be in append mode");
        }

        int nBytes;

        buffer.clear();
        buffer.limit(FileHeader.HEADER_SIZE_BYTES);

        Future<Integer> f = asyncFileChannel.read(buffer, 0L);
        try {
            nBytes = f.get();
        }
        catch (Exception e) {
            throw new IOException(e);
        }

        // Check to see if we read the whole header
        if (nBytes != 32) {
            throw new EvioException("bad file format");
        }

        try {
            // Parse header info
            appendFileHeader = new FileHeader();
            buffer.position(0);
            // Buffer's position/limit does not change
            appendFileHeader.readHeader(buffer);

            // Set the byte order to match the buffer/file's ordering.
            byteOrder = appendFileHeader.getByteOrder();
            buffer.order(byteOrder);

            hasAppendDictionary = appendFileHeader.hasDictionary();
            hasTrailerWithIndex = appendFileHeader.hasTrailerWithIndex();
            indexLength         = appendFileHeader.getIndexLength();
            userHeaderLength    = appendFileHeader.getUserHeaderLength();
            userHeaderPadding   = appendFileHeader.getUserHeaderLengthPadding();

//            int fileID      = appendFileHeader.getFileId();
//            int headerLen   = appendFileHeader.getHeaderLength();
//            int recordCount = appendFileHeader.getEntries();
//            int fileNum     = appendFileHeader.getFileNumber();
//
//            System.out.println("file ID          = " + fileID);
//            System.out.println("fileNumber       = " + fileNum);
//            System.out.println("headerLength     = " + headerLen);
//            System.out.println("recordCount      = " + recordCount);
//            System.out.println("trailer index    = " + hasTrailerWithIndex);
//            System.out.println("bit info         = " + Integer.toHexString(bitInfo));
//            System.out.println();
        }
        catch (Exception a) {
            throw new EvioException(a);
        }
    }


    /**
     * This method positions a file for the first {@link #writeEvent(EvioBank)}
     * in append mode. It places the writing position after the last event (not record header).
     *
     * @throws IOException     if file reading/writing problems
     * @throws EvioException   if bad file/buffer format; if not in append mode
     */
    private void toAppendPosition() throws EvioException, IOException {

        // Only for append mode
        if (!append) {
            throw new EvioException("need to be in append mode");
        }

        System.out.println("    toAppendPosition: IN");

        // Jump over the file header, index array, and user header & padding
        long pos = FileHeader.HEADER_SIZE_BYTES + indexLength +
                                     userHeaderLength + userHeaderPadding;
        // This puts us at the beginning of the first record header
        fileWritingPosition = pos;

        long fileSize = asyncFileChannel.size();
        boolean lastRecord, readEOF = false;
        int recordLen, eventCount, nBytes, bitInfo, headerPosition;
        Future<Integer> future;

        long bytesLeftInFile = fileSize;


//        int indexArrayLen, userHeaderLen, compDataLen, unCompDataLen;
//        int userPadding, dataPadding, compPadding;
//        int headerLen, currentPosition, compType, compWord;

        // The file's record #s may be fine or they may be messed up.
        // Assume they start with one and increment from there. That
        // way any additional records now added to the file will have a
        // reasonable # instead of incrementing from the last existing
        // record.
        recordNumber = 1;
        System.out.println("toAppendPos:     record # = 1");

        while (true) {
            nBytes = 0;

            // Read in most of the normal record header, 40 bytes.
            // Skip the last 16 bytes which are only 2 user registers.
            buffer.clear();
            buffer.limit(24);

//System.out.println("toAppendPosition: (before read) file pos = " + fileChannel.position());
            while (nBytes < 24) {
                // There is no internal asyncFileChannel position
                int partial;

                future = asyncFileChannel.read(buffer, fileWritingPosition);
                try {
                    partial = future.get();
                }
                catch (Exception e) {
                    throw new IOException(e);
                }

                // If EOF ...
                if (partial < 0) {
                    if (nBytes != 0) {
                        throw new EvioException("bad buffer format");
                    }
                    // Missing last empty record header
                    readEOF = true;
                    break;
                }
                nBytes += partial;
                bytesLeftInFile -= partial;
            }

            // If we did not read correct # of bytes or didn't run into EOF right away
            if (nBytes != 0 && nBytes != 24) {
                throw new EvioException("internal file reading error");
            }

            headerPosition = 0;
            fileWritingPosition += 24;

            bitInfo    = buffer.getInt(headerPosition + RecordHeader.BIT_INFO_OFFSET);
            recordLen  = buffer.getInt(headerPosition + RecordHeader.RECORD_LENGTH_OFFSET);
            eventCount = buffer.getInt(headerPosition + RecordHeader.EVENT_COUNT_OFFSET);
            lastRecord = RecordHeader.isLastRecord(bitInfo);
////          If reading entire header, change 24 to 40 above & below
//            headerLen     = buffer.getInt(headerPosition + RecordHeader.HEADER_LENGTH_OFFSET);
//            userHeaderLen = buffer.getInt(headerPosition + RecordHeader.USER_LENGTH_OFFSET);
//            indexArrayLen = buffer.getInt(headerPosition + RecordHeader.INDEX_ARRAY_OFFSET);
//            unCompDataLen = buffer.getInt(headerPosition + RecordHeader.UNCOMPRESSED_LENGTH_OFFSET);
//
//            compWord      = buffer.getInt(headerPosition + RecordHeader.COMPRESSION_TYPE_OFFSET);
//            compType = compWord >>> 28;
//            // If there is compression ...
//            if (compType != 0) {
//                compDataLen = compWord & 0xfffffff;
//            }
//
//            System.out.println("bitInfo      = 0x" + Integer.toHexString(bitInfo));
//            System.out.println("recordLength = " + recordLen);
//            System.out.println("headerLength = " + headerLen);
//            System.out.println("eventCount   = " + eventCount);
//            System.out.println("lastRecord   = " + lastRecord);
//            System.out.println();

            // Track total number of events in file/buffer (minus dictionary)
            eventsWrittenTotal += eventCount;

            recordNumber++;
System.out.println("                 record # = " + recordNumber);

            // Stop at the last record. The file may not have a last record if
            // improperly terminated. Running into an End-Of-File will flag
            // this condition.
            if (lastRecord || readEOF) {
                break;
            }

            // Hop to next record header
            int bytesToNextBlockHeader = 4*recordLen - 24;
            if (bytesLeftInFile < bytesToNextBlockHeader) {
                throw new EvioException("bad file format");
            }
            fileWritingPosition += bytesToNextBlockHeader;
            bytesLeftInFile     -= bytesToNextBlockHeader;
        }

        if (hasAppendDictionary) {
            eventsWrittenToFile = eventsWrittenToBuffer = eventsWrittenTotal + 1;
        }
        else {
            eventsWrittenToFile = eventsWrittenToBuffer = eventsWrittenTotal;
        }

        //-------------------------------------------------------------------------------
        // If we're here, we've just read the last record header (at least 6 words of it).
        // File position is just past header, but buffer position is just before it.
        // Either that or we ran into end of file (last record header missing).
        //
        // If EOF, last record header missing, we're good.
        //
        // Else check to see if the last record contains data. If it does,
        // change a single bit so it's not labeled as the last record,
        // then jump past all data.
        //
        // Else if there is no data, position file before it as preparation for writing
        // the next record.
        //-------------------------------------------------------------------------------

        // If no last, empty record header in file ...
        if (readEOF) {
            // It turns out we need to do nothing. The constructor that
            // calls this method will write out the next record header.
            recordNumber--;
System.out.println("                 record # = " + recordNumber);
        }
        // If last record has event(s) in it ...
        else if (eventCount > 0) {
            // Clear last record bit in 6th header word
            bitInfo = RecordHeader.clearLastRecordBit(bitInfo);

            // Rewrite header word with new bit info & hop over record

            // File now positioned right after the last header to be read
            // Back up to before 6th block header word
            fileWritingPosition -= 24 - RecordHeader.BIT_INFO_OFFSET;
//System.out.println("toAppendPosition: writing over last block's 6th word, back up %d words" +(8 - 6));

            // Write over 6th block header word
            buffer.clear();
            buffer.putInt(bitInfo);
            buffer.flip();

            //fileChannel.write(buffer);
            future = asyncFileChannel.write(buffer, fileWritingPosition);
            try {
                future.get();
            }
            catch (Exception e) {
                throw new IOException(e);
            }

            // Hop over the entire block
//System.out.println("toAppendPosition: wrote over last block's 6th word, hop over %d words" +
//                   (recordLen - (6 + 4)));
            fileWritingPosition += (4 * recordLen) - (RecordHeader.BIT_INFO_OFFSET + 4);
        }
        // else if last record has NO data in it ...
        else {
            // We already partially read in the record header, now back up so we can overwrite it.
            // If using buffer, we never incremented the position, so we're OK.
            recordNumber--;
System.out.println("                 record # = " + recordNumber);
            fileWritingPosition -= 24;
//System.out.println("toAppendPos: position (bkup) = " + fileWritingPosition);
        }

//System.out.println("toAppendPos: file pos = " + fileWritingPosition);
        bytesWrittenToFile = fileWritingPosition;

        // We should now be in a state identical to that if we had
        // just now written everything currently in the file/buffer.
    }


    /**
     * Is there room to write this many bytes to an output buffer as a single event?
     * Will always return true when writing to a file.
     * @param bytes number of bytes to write
     * @return {@code true} if there still room in the output buffer, else {@code false}.
     */
    public boolean hasRoom(int bytes) {
//System.out.println("User buffer size (" + currentRecord.getInternalBufferCapacity() + ") - bytesWritten (" + bytesWrittenToBuffer +
//      ") - trailer (" + trailerBytes() +  ") = (" +
//         ((currentRecord.getInternalBufferCapacity() - bytesWrittenToBuffer) >= bytes + RecordHeader.HEADER_SIZE_BYTES) +
//      ") >= ? " + bytes);
        return toFile() || ((currentRecord.getInternalBufferCapacity() -
                             bytesWrittenToBuffer - trailerBytes()) >= bytes);
    }

    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * The bank in this case is the one represented by the node argument.
     * Once the record is full and if writing to a file, the record will be
     * sent to a thread which may compress the data, then it will be
     * sent to a thread to write the record to file.
     * If writing to a buffer, once the record is full this method returns
     * false - indicating that the last event was NOT written to the record.
     * To finish the writing process, call {@link #close()}. This will
     * compress the data if desired and then write it to the buffer.<p>
     *
     * The buffer must contain only the event's data (event header and event data)
     * and must <b>not</b> be in complete evio file format.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     *<b>kill</b> performance when writing to a file.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param node       object representing the event to write in buffer form
     * @param force      if writing to disk, force it to write event to the disk.
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         record event count limit exceeded, or bank and bankBuffer args are both null.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if null node arg;
     */
    public boolean writeEvent(EvioNode node, boolean force) throws EvioException, IOException {
        // Duplicate buffer so we can set pos & limit without messing others up
        return writeEvent(node, force, true);
    }

    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * The bank in this case is the one represented by the node argument.
     * Once the record is full and if writing to a file, the record will be
     * sent to a thread which may compress the data, then it will be
     * sent to a thread to write the record to file.
     * If writing to a buffer, once the record is full this method returns
     * false - indicating that the last event was NOT written to the record.
     * To finish the writing process, call {@link #close()}. This will
     * compress the data if desired and then write it to the buffer.<p>
     *
     * The buffer must contain only the event's data (event header and event data)
     * and must <b>not</b> be in complete evio file format.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     *<b>kill</b> performance when writing to a file.
     * A true 3rd arg can be used when the backing buffer
     * of the node is accessed by multiple threads simultaneously. This allows
     * that buffer's limit and position to be changed without interfering
     * with the other threads.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param node       object representing the event to write in buffer form
     * @param force      if writing to disk, force it to write event to the disk.
     * @param duplicate  if true, duplicate node's buffer so its position and limit
     *                   can be changed without issue.
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         record event count limit exceeded, or bank and bankBuffer args are both null.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if null node arg;
     */
    public boolean writeEvent(EvioNode node, boolean force, boolean duplicate)
            throws EvioException, IOException {

        if (node == null) {
            throw new EvioException("null node arg");
        }

//        int origLim=0,origPos=0;
        ByteBuffer eventBuffer, bb = node.getBuffer();

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
        return writeEvent(null, eventBuffer, force);

        // Shouldn't the pos & lim be reset for non-duplicate?
        // It don't think it matters since node knows where to
        // go inside the buffer.
//        if (!duplicate) {
//            bb.limit(origLim).position(origPos);
//        }
    }

    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file, the record will be
     * sent to a thread which may compress the data, then it will be
     * sent to a thread to write the record to file.
     * If writing to a buffer, once the record is full this method returns
     * false - indicating that the last event was NOT written to the record.
     * To finish the writing process, call {@link #close()}. This will
     * compress the data if desired and then write it to the buffer.<p>
     *
     * The buffer must contain only the event's data (event header and event data)
     * and must <b>not</b> be in complete evio file format.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param bankBuffer the bank (as a ByteBuffer object) to write.
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         record event count limit exceeded, or bank and bankBuffer args are both null.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing.
     */
    public boolean writeEvent(ByteBuffer bankBuffer) throws EvioException, IOException {
        return writeEvent(null, bankBuffer, false);
    }

    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file, the record will be
     * sent to a thread which may compress the data, then it will be
     * sent to a thread to write the record to file.
     * If writing to a buffer, once the record is full this method returns
     * false - indicating that the last event was NOT written to the record.
     * To finish the writing process, call {@link #close()}. This will
     * compress the data if desired and then write it to the buffer.<p>
     *
     * The buffer must contain only the event's data (event header and event data)
     * and must <b>not</b> be in complete evio file format.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     * <b>kill</b> performance when writing to a file.
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param bankBuffer the bank (as a ByteBuffer object) to write.
     * @param force      if writing to disk, force it to write event to the disk.
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         record event count limit exceeded, or bank and bankBuffer args are both null.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing.
     */
    public boolean writeEvent(ByteBuffer bankBuffer, boolean force)
            throws EvioException, IOException {
        return writeEvent(null, bankBuffer, force);
    }


    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file, the record will be
     * sent to a thread which may compress the data, then it will be
     * sent to a thread to write the record to file.
     * If writing to a buffer, once the record is full this method returns
     * false - indicating that the last event was NOT written to the record.
     * To finish the writing process, call {@link #close()}. This will
     * compress the data if desired and then write it to the buffer.<p>
     *
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param bank   the bank to write.
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         record event count limit exceeded, or bank and bankBuffer args are both null.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;.
     */
    public boolean writeEvent(EvioBank bank)
            throws EvioException, IOException {
        return writeEvent(bank, null, false);
    }


    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file, the record will be
     * sent to a thread which may compress the data, then it will be
     * sent to a thread to write the record to file.
     * If writing to a buffer, once the record is full this method returns
     * false - indicating that the last event was NOT written to the record.
     * To finish the writing process, call {@link #close()}. This will
     * compress the data if desired and then write it to the buffer.<p>
     *
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * Be warned that injudicious use of the 2nd arg, the force flag, will
     * <b>kill</b> performance when writing to a file.
     *
     * @param bank   the bank to write.
     * @param force  if writing to disk, force it to write event to the disk.
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         record event count limit exceeded, or bank and bankBuffer args are both null.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;.
     */
    public boolean writeEvent(EvioBank bank, boolean force)
            throws EvioException, IOException {
        return writeEvent(bank, null, force);
    }


    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file, the record will be
     * sent to a thread which may compress the data, then it will be
     * sent to a thread to write the record to file.
     * If writing to a buffer, once the record is full this method returns
     * false - indicating that the last event was NOT written to the record.
     * To finish the writing process, call {@link #close()}. This will
     * compress the data if desired and then write it to the buffer.<p>
     *
     * The event to be written may be in one of two forms.
     * The first is as an EvioBank object and the second is as a ByteBuffer
     * containing only the event's data (event header and event data) and must
     * <b>not</b> be in complete evio file format.
     * The first non-null of the bank arguments will be written.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     *<b>kill</b> performance when writing to a file.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param bank       the bank (as an EvioBank object) to write.
     * @param bankBuffer the bank (as a ByteBuffer object) to write.
     * @param force      if writing to disk, force it to write event to the disk.
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         record event count limit exceeded, or bank and bankBuffer args are both null.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if bad bankBuffer format;
     *                       if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing.
     */
    synchronized private boolean writeEvent(EvioBank bank, ByteBuffer bankBuffer, boolean force)
            throws EvioException, IOException {

        if (closed) {
            throw new EvioException("close() has already been called");
        }

        boolean fitInRecord;
        boolean splittingFile = false;
        // See how much space the event will take up
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
            if (currentEventBytes != 4 * (bankBuffer.getInt(bankBuffer.position()) + 1)) {
                throw new EvioException("inconsistent event lengths: total bytes from event = " +
                                       (4*(bankBuffer.getInt(bankBuffer.position()) + 1)) +
                                        ", from buffer = " + currentEventBytes);
            }
        }
        else if (bank != null) {
            currentEventBytes = bank.getTotalBytes();
        }
        else {
            return false;
        }

        // If writing to buffer, we're not multi-threading compression & writing.
        // Do it all in this thread, right now.
        if (!toFile) {
            return writeToBuffer(bank, bankBuffer);
        }

        // If here, we're writing to a file ...
        
        // If we're splitting files AND ..
        // If all that has been written so far are the dictionary and first event,
        // don't split after writing them. In other words, we must have written
        // at least one real event before one can get past this point and really
        // split the file.
        if ((split > 0) && (eventsWrittenToBuffer > 0)) {
            // Is this event -- with the current event, current file,
            // current uncompressed record plus 1 index entry for it,
            // trailer plus its record index AND possibly another
            // header if event doesn't fit in this record
            // -- large enough to split the file?
// TODO: REthink recordNumber!!!!!!!!!!!!!!
            long totalSize = currentEventBytes + bytesWrittenToFile +
                    currentRecord.getUncompressedSize() +
                    2 * RecordHeader.HEADER_SIZE_BYTES +
                    4 * recordNumber;

            // If we're going to split the file, set a couple flags
            if (totalSize > split) {
                splittingFile = true;
            }
        }

        // If event is big enough to split the file ...
        if (splittingFile) {
            if (singleThreadedCompression) {
                compressAndWriteToFile(false);
                splitFile();
            }
            else {
                // Set flag to split file
                currentRingItem.splitFileAfterWrite(true);
                // Send current record back to ring without adding event
                supply.publish(currentRingItem);

                // Get another empty record from ring
                currentRingItem = supply.get();
                currentRecord = currentRingItem.getRecord();
            }
        }

        // Try adding event to current record.
        // One event is guaranteed to fit in a record no matter the size.
        if (bankBuffer != null) {
            fitInRecord = currentRecord.addEvent(bankBuffer);
        }
        else {
            fitInRecord = currentRecord.addEvent(bank);
        }

        // If no room or too many events ...
        if (!fitInRecord) {
            if (singleThreadedCompression) {
                compressAndWriteToFile(false);
            }
            else {
                // Send current record back to ring without adding event
                supply.publish(currentRingItem);

                // Get another empty record from ring
                currentRingItem = supply.get();
                currentRecord = currentRingItem.getRecord();
            }

            // Add event to it (guaranteed to fit)
            if (bankBuffer != null) {
                currentRecord.addEvent(bankBuffer);
            }
            else {
                currentRecord.addEvent(bank);
            }
        }

        // If event must be physically written to disk ...
        if (force) {
            if (singleThreadedCompression) {
                compressAndWriteToFile(true);
            }
            else {
                // Tell writer to force this record to disk
                currentRingItem.forceToDisk(true);
                // Send current record back to ring
                supply.publish(currentRingItem);

                // Get another empty record from ring
                currentRingItem = supply.get();
                currentRecord = currentRingItem.getRecord();
            }
        }

        return true;
    }


    /**
     * Class used to take data-filled record from supply, compress it,
     * and release it back to the supply.
     */
    class RecordCompressor extends Thread {
        /** Keep track of this thread with id number. */
        private final int num;

        /**
         * Constructor.
         * @param threadNumber unique thread number starting at 0.
         */
        RecordCompressor(int threadNumber) {num = threadNumber;}

        @Override
        public void run() {
            try {
                // The first time through, we need to release all records coming before
                // our first in case there are < num records before close() is called.
                // This way close() is not waiting for thread #12 to get and subsequently
                // release items 0 - 11 when there were only 5 records total.
                // (threadNumber starts at 0).
                supply.release(num, num - 1);

                while (true) {
                    if (Thread.interrupted()) {
                        return;
                    }
//System.out.println("   Compressor: try getting record to compress");

                    // Get the next record for this thread to compress
                    RecordRingItem item = supply.getToCompress(num);
                    // Pull record out of wrapping object
                    RecordOutputStream record = item.getRecord();

                    // Set compression type and record #
                    RecordHeader header = record.getHeader();
                    // Fortunately for us, the record # is also the sequence # + 1 !
                    header.setRecordNumber((int)(item.getSequence() + 1L));
                    header.setCompressionType(compressionType);
//System.out.println("   Compressor: set record # to " + header.getRecordNumber());
                    // Do compression
                    record.build();
//System.out.println("   Compressor: got record, header = \n" + header);

//System.out.println("   Compressor: release item to supply");
                    // Release back to supply
                    supply.releaseCompressor(item);
                }
            }
            catch (InterruptedException e) {
                // We've been interrupted while blocked in getToCompress
                // which means we're all done.
            }
        }
    }


    /**
     * Class used to take data-filled record from supply, write it,
     * and release it back to the supply. Only one of these exists
     * which makes tracking indexes and lengths easy.
     */
    class RecordWriter extends Thread {

        /** The highest sequence to have been currently processed. */
        private volatile long lastSeqProcessed = -1;

        /** Wait for the last item to be processed, then exit thread. */
        void waitForLastItem() {
            while (supply.getLastSequence() > lastSeqProcessed) {
                Thread.yield();
            }
            // Interrupt this thread, not the thread calling this method
            this.interrupt();
        }

        @Override
        public void run() {
            try {
                boolean split;
                long currentSeq;

                while (true) {
                    if (Thread.interrupted()) {
                        return;
                    }

                    //System.out.println("   Writer: try getting record to write");
                    // Get the next record for this thread to write
                    RecordRingItem item = supply.getToWrite();
                    currentSeq = item.getSequence();
                    split = item.splitFileAfterWrite();

                    // Write to file
                    writeToFile(item, item.forceToDisk());

                    // Release record back to supply
                    supply.releaseWriter(item);

                    // Now we're done with this sequence
                    lastSeqProcessed = currentSeq;

                    // Split file if needed
                    if (split) {
                        splitFile();
                    }
                }
            }
            catch (InterruptedException e) {
                // We've been interrupted while blocked in getToWrite
                // which means we're all done.
            }
            catch (Exception e) {
                e.printStackTrace();
                // TODO: What do we do here??
            }
        }
    }


    /**
     * Compress data and write record to file. Does nothing if close() already called.
     * Used when doing compression & writing to file in a single thread.
     * Only called from synchronized context.
     *
     * @param force force it to write event to the disk.
     * @return {@code false} if no data written, else {@code true}
     *
     * @throws IOException   if error writing file
     * @throws EvioException if this object already closed;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     */
    private void compressAndWriteToFile(boolean force)
                                throws EvioException, IOException {
        RecordHeader header = currentRecord.getHeader();
        header.setRecordNumber(recordNumber);
        header.setCompressionType(compressionType);
        currentRecord.build();
        writeToFile(null, force);
        currentRecord.reset();
     }


    /**
     * Write record to file. Does nothing if close() already called.
     *
     * @param item  contains record to write to the disk if compression is multi-threaded.
     * @param force force it to write event to the disk.
     * @return {@code false} if no data written, else {@code true}
     *
     * @throws EvioException if this object already closed;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     * @throws IOException   if error writing file
     */
    synchronized private boolean writeToFile(RecordRingItem item, boolean force)
                                throws EvioException, IOException {
        if (closed) {
            throw new EvioException("close() has already been called");
        }

        // This actually creates the file so do it only once
        if (bytesWrittenToFile < 1) {
//System.out.println("    writeToFile: create file " + currentFile.getName());
            try {
                asyncFileChannel = AsynchronousFileChannel.open(currentFilePath,
                                                                StandardOpenOption.TRUNCATE_EXISTING,
                                                                StandardOpenOption.CREATE,
                                                                StandardOpenOption.WRITE);
//                System.out.println("\n*******\nOPENED NEW FILE " + currentFilePath.toFile().getName() +
//                                           ", size is " + asyncFileChannel.size());
                fileWritingPosition = 0L;
                splitCount++;
            }
            catch (FileNotFoundException e) {
                throw new EvioException("File could not be opened for writing, " +
                                                currentFile.getPath(), e);
            }

            // Write out the beginning file header including common record
            writeFileHeader();
        }

        // Which buffer do we fill next?
        ByteBuffer unusedBuffer;

        // We need one of the 2 future jobs to be completed in order to proceed

        // If 1st time thru, proceed without waiting
        if (future1 == null) {
            futureIndex = 0;
            // Fill 2nd buffer next
            unusedBuffer = internalBuffers[1];
        }
        // If 2nd time thru, proceed without waiting
        else if (future2 == null) {
            futureIndex = 1;
            // Fill 3rd buffer next
            unusedBuffer = internalBuffers[2];
        }
        // After first 2 times, wait until one of the
        // 2 futures is finished before proceeding
        else {
            // If future1 is finished writing, proceed
            if (future1.isDone()) {
                futureIndex = 0;
                // Reuse the buffer future1 just finished using
                unusedBuffer = usedBuffers[0];
            }
            // If future2 is finished writing, proceed
            else if (future2.isDone()) {
                futureIndex = 1;
                unusedBuffer = usedBuffers[1];
            }
            // Neither are finished, so wait for one of them to finish
            else {
                // If the last write to be submitted was future2, wait for 1
                if (futureIndex == 0) {
                    try {
                        // Wait for write to end before we continue
                        future1.get();
                        unusedBuffer = usedBuffers[0];
                    }
                    catch (Exception e) {
                        throw new IOException(e);
                    }
                }
                // Otherwise, wait for 2
                else {
                    try {
                        future2.get();
                        unusedBuffer = usedBuffers[1];
                    }
                    catch (Exception e) {
                        throw new IOException(e);
                    }
                }
            }
        }

        // Get record to write
        RecordOutputStream record = currentRecord;
        if (!singleThreadedCompression) {
            record = item.getRecord();
        }
        RecordHeader header = record.getHeader();

        // Length of this record
        int bytesToWrite = header.getLength();
        int eventCount   = header.getEntries();
        recordLengths.add(bytesToWrite);
        // Trailer's index has count following length
        recordLengths.add(eventCount);

        // Data to write
        ByteBuffer buf = record.getBinaryBuffer();

        if (futureIndex == 0) {
            //buf.position(buffer.limit() - 20);
            future1 = asyncFileChannel.write(buf, fileWritingPosition);
            // Keep track of which buffer future1 used so it can be reused when done
            usedBuffers[0] = buf;
            futureIndex = 1;
        }
        else {
            //buf.position(buffer.limit() - 20);
            future2 = asyncFileChannel.write(buf, fileWritingPosition);
            usedBuffers[1] = buf;
            futureIndex = 0;
        }

        // Next buffer to work with
        buffer = unusedBuffer;
        buffer.clear();
        record.setBuffer(buffer);
        record.reset();

        // Force it to write to physical disk (KILLS PERFORMANCE!!!, 15x-20x slower),
        // but don't bother writing the metadata (arg to force()) since that slows it
        // down even more.
        // TODO: This may not work since data may NOT have been written yet!
        if (force) asyncFileChannel.force(false);

        // Keep track of what is written to this, one, file
        recordNumber++;
        recordsWritten++;
        bytesWritten        += bytesToWrite;
        bytesWrittenToFile  += bytesToWrite;
        fileWritingPosition += bytesToWrite;
        eventsWrittenToFile += eventCount;
        eventsWrittenTotal  += eventCount;

        //fileWritingPosition += 20;
        //bytesWrittenToFile  += 20;


        if (false) {
            System.out.println("    writeToFile: after last header written, Events written to:");
            System.out.println("                 cnt total (no dict) = " + eventsWrittenTotal);
            System.out.println("                 file cnt total (dict) = " + eventsWrittenToFile);
            System.out.println("                 internal buffer cnt (dict) = " + eventsWrittenToBuffer);
            System.out.println("                 bytes-written  = " + bytesToWrite);
            System.out.println("                 bytes-to-file = " + bytesWrittenToFile);
            System.out.println("                 record # = " + recordNumber);
        }

        return true;
    }


    /**
     * Split the file.
     * Never called when output is to buffer.
     * It writes the trailer which includes an index of all records.
     * Then it closes the old file (forcing unflushed data to be written)
     * and opens the new one.
     *
     * @throws EvioException if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     * @throws IOException   if error writing file
     */
    synchronized private void splitFile() throws EvioException, IOException {

        if (asyncFileChannel != null) {
            if (addingTrailer) {
                // Write the trailer
                writeTrailerToFile(addTrailerIndex);
            }

            // Close existing file (in separate thread for speed)
            // which will also flush remaining data.
            // TODO: must make sure the above trailer write gets finished
            fileCloser.closeAsyncFile(asyncFileChannel);
        }

        // Right now no file is open for writing
        asyncFileChannel = null;

        // Create the next file's name
        String fileName = Utilities.generateFileName(baseFileName, specifierCount,
                                                     runNumber, split, splitNumber,
                                                     streamId, streamCount);
        splitNumber += splitIncrement;
        currentFile = new File(fileName);

        // If we can't overwrite and file exists, throw exception
        if (!overWriteOK && (currentFile.exists() && currentFile.isFile())) {
            throw new EvioException("File exists but user requested no over-writing, "
                    + currentFile.getPath());
        }

        // Reset file values for reuse
        recordNumber        = 1;
        recordsWritten      = 0;
        bytesWritten        = 0L;
        bytesWrittenToFile  = 0L;
        eventsWrittenToFile = 0;

System.out.println("    splitFile: generated file name = " + fileName + ", record # = " + recordNumber);
    }


    /**
     * Write a general header as the last "header" or trailer in the file
     * optionally followed by an index of all record lengths.
     * @param writeIndex if true, write an index of all record lengths in trailer.
     * @throws IOException if problems writing to file.
     */
    private void writeTrailerToFile(boolean writeIndex) throws IOException {

        // Keep track of where we are right now which is just before trailer
        long trailerPosition = bytesWritten;

        int bytesToWrite;

        // If we're NOT adding a record index, just write trailer
        if (!writeIndex) {
            try {
                headerBuffer.position(0).limit(RecordHeader.HEADER_SIZE_BYTES);
                RecordHeader.writeTrailer(headerBuffer, 0, recordNumber, null);
            }
            catch (HipoException e) {/* never happen */}
            bytesToWrite = RecordHeader.HEADER_SIZE_BYTES;

            // We don't want to let the closer thread do the work of seeing that
            // this write completes since it'll just complicate the code.
            // As this is the absolute last write to the file,
            // just make sure it gets done right here.
            Future f = asyncFileChannel.write(headerBuffer, fileWritingPosition);
            try {f.get();}
            catch (Exception e) {
                throw new IOException(e);
            }
            fileWritingPosition += RecordHeader.HEADER_SIZE_BYTES;
        }
        else {
            // Create the index of record lengths in proper byte order
            byte[] recordIndex = new byte[4 * recordLengths.size()];
            try {
                for (int i = 0; i < recordLengths.size(); i++) {
                    ByteDataTransformer.toBytes(recordLengths.get(i), byteOrder,
                                                recordIndex, 4 * i);
                }
            }
            catch (EvioException e) {/* never happen */}

            // Write trailer with index

            // How many bytes are we writing here?
            bytesToWrite = RecordHeader.HEADER_SIZE_BYTES + recordIndex.length;

            // Make sure our array can hold everything
            if (headerArray.length < bytesToWrite) {
                headerArray = new byte[bytesToWrite];
                headerBuffer = ByteBuffer.wrap(headerArray);
            }
            headerBuffer.position(0).limit(bytesToWrite);

            // Place data into headerBuffer - both header and index
            try {
                RecordHeader.writeTrailer(headerBuffer, 0, recordNumber,
                                          recordIndex);
                headerBuffer.limit();
            }
            catch (HipoException e) {/* never happen */}
            Future f = asyncFileChannel.write(headerBuffer, fileWritingPosition);
            try {f.get();}
            catch (Exception e) {
                throw new IOException(e);
            }
            fileWritingPosition += RecordHeader.HEADER_SIZE_BYTES;
        }

        // Update file header's trailer position word
        headerBuffer.position(0).limit(8);
        headerBuffer.putLong(0, trailerPosition);
        Future f = asyncFileChannel.write(headerBuffer, FileHeader.TRAILER_POSITION_OFFSET);
        try {f.get();}
        catch (Exception e) {
            throw new IOException(e);
        }

        // Update file header's bit-info word
        if (addTrailerIndex) {
            int bitInfo = fileHeader.setBitInfo(fileHeader.hasFirstEvent(),
                                                fileHeader.hasDictionary(),
                                                true);
            headerBuffer.position(0).limit(4);
            headerBuffer.putInt(0, bitInfo);
            f = asyncFileChannel.write(headerBuffer, FileHeader.BIT_INFO_OFFSET);
            try {f.get();}
            catch (Exception e) {
                throw new IOException(e);
            }
        }

        // Keep track of what is written to this file.
        // We did write a record, even if it had no data.
        bytesWritten       += bytesToWrite;
        bytesWrittenToFile += bytesToWrite;

        return;
    }


    /**
     * Flush everything in currentRecord into the buffer.
     * There is only one record containing events which is written into the buffer.
     * Following that record is a trailer - an empty record with only a header
     * in order to signify that there are no more events to follow. The trailer
     * could contain an index of all events in the buffer, although this is never
     * done when transferring evio data in buffers over the network.
     */
    private void flushCurrentRecordToBuffer() {

        int eventCount = currentRecord.getEventCount();
        // If nothing in current record, return
        if (eventCount < 1) {
            return;
        }

        // Do construction of record in buffer and possibly compression of its data
        if (commonRecord != null) {
            currentRecord.build(commonRecord.getBinaryBuffer());
        }
        else {
            currentRecord.build();
        }

        // Get record header
        RecordHeader header = currentRecord.getHeader();
        // Get/set record info before building
        header.setRecordNumber(recordNumber);

//        System.out.println("flushCurrentRecordToBuffer: comp size = " + header.getCompressedDataLength() +
//                ", comp words = " + header.getCompressedDataLengthWords() + ", padding = " +
//                header.getCompressedDataLengthPadding());

        int bytesToWrite = header.getLength();
        // Store length & count for possible trailer index

        recordLengths.add(bytesToWrite);
        // Trailer's index has count following length
        recordLengths.add(eventCount);

        // Keep track of what is written
//        recordNumber++;
        recordsWritten++;
        
        // We need to reset lengths here since the data may now be compressed
        bytesWritten = bytesToWrite;
        bytesWrittenToBuffer = bytesToWrite;
//System.out.println("flushCurrentRecordToBuffer: record len = " + bytesWritten);
    }


//    /** Flush everything in commonRecord into the buffer.
//     * Only called before any other events are written. */
//    private void flushCommonRecordToBuffer() {
//
//        if (commonRecord == null || commonRecord.getEventCount() < 1) {
//            return;
//        }
//
//        int eventCount = commonRecord.getEventCount();
//        if (eventCount < 1) {
//            return;
//        }
//
//        // Get record header
//        RecordHeader header = commonRecord.getHeader();
//        // Get/set record info
//        header.setRecordNumber(0);
//
//        int bytesToWrite = header.getLength();
//
//        // TODO: Make sure that the dictionary and first event are not
//        // too big to fit in a user-given buffer.
//        if (currentRecord.roomForEvent(bytesToWrite)) {
//
//        }
//
//        // Store length & count for possible trailer index
//        recordLengths.add(bytesToWrite);
//        // Trailer's index has count following length
//        recordLengths.add(eventCount);
//
//        // Compression/build already done (in createCommonRecord())
//
//        // Write record to buffer. Its position should be 0.
//        // Binary buffer is ready to read after build().
//        ByteBuffer buf = commonRecord.getBinaryBuffer();
//        if (buf.hasArray() && buffer.hasArray()) {
//            System.arraycopy(buf.array(), 0, buffer.array(),
//                             buffer.position(), bytesToWrite);
//        }
//        else {
//            buffer.put(buf);
//        }
//
//        // This method is called before any other events are written.
//        // However, it may be called after the constructor, meaning that
//        // when "buffer" was passed to the constructor, it's position was 0.
//        // Now that's changed, so we need to notify the currentRecord that
//        // the buffer is to be written into starting at pos = bytesToWrite.
//        currentRecord.setStartingBufferPosition(bytesToWrite);
//
//        // Keep track of what is written
//// TODO: Should this be incremented ???    NOT FOR A BUFFER!!!!
//        recordsWritten++;
//
//        bytesWritten              = bytesToWrite;
//        int i = bytesWrittenToBuffer;
//        bytesWrittenToBuffer      = bytesToWrite;
//System.out.println("** flushCommonRecordToBuffer: bytesWrittenToBuffer, " + i +
//                           " --> " + bytesWrittenToBuffer);
//        commonRecordBytesToBuffer = bytesToWrite;
//        eventsWrittenToBuffer     = eventCount;
//        eventsWrittenTotal        = eventCount;
//System.out.println("flushCommonRecordToBuffer: buf pos, lim = " + buffer.position() +
//                   ", " + buffer.limit());
//    }
    

    /**
     * Write bank to current record. If it doesn't fit, return false.
     * The currentRecord will always accept at least one event if it's not
     * writing into a user-provided buffer, expanding memory if it has to.
     * A bank in buffer form has priority, if it's null, then it looks
     * at the bank in EvioBank object form.
     *
     * @param bank        bank to write in EvioBank object form.
     * @param bankBuffer  bank to write in buffer form .
     * @return true if event was added to buffer, false if the buffer is full or
     *         event count limit exceeded.
     */
    private boolean writeToBuffer(EvioBank bank, ByteBuffer bankBuffer) {
        boolean fitInRecord;

        if (bankBuffer != null) {
            // Will this fit the event being written PLUS the ending trailer?
            fitInRecord = currentRecord.addEvent(bankBuffer, trailerBytes());
        }
        else {
            fitInRecord = currentRecord.addEvent(bank, trailerBytes());
        }

        if (fitInRecord) {
             // Update the current block header's size and event count as best we can.
             // Does NOT take compression or trailer into account.
             bytesWritten = commonRecordBytesToBuffer + currentRecord.getUncompressedSize();
             bytesWrittenToBuffer = (int) bytesWritten;
             eventsWrittenTotal++;
             eventsWrittenToBuffer++;
         }
         else {
System.out.println("** writeToBuffer: NOT FIT IN RECORD");
         }

        return fitInRecord;
    }


    /**
     * How many bytes make up the desired trailer?
     * @return  number of bytes that make up the desired trailer.
     */
    private int trailerBytes() {
        int len = 0;
        if (addingTrailer) len += RecordHeader.HEADER_SIZE_BYTES;
        if (addTrailerIndex) len += 4 * recordLengths.size();
        return len;
    }


    /**
     * Write a general header as the last "header" or trailer in the buffer
     * optionally followed by an index of all record lengths.
     * @param writeIndex if true, write an index of all record lengths in trailer.
     * @throws EvioException if not enough room in buffer to hold trailer.
     */
    private void writeTrailerToBuffer(boolean writeIndex) throws EvioException {

        int bytesToWrite;

        // If we're NOT adding a record index, just write trailer
        if (!writeIndex) {
            // Make sure buffer can hold a trailer
            if ((buffer.capacity() - (int)bytesWritten) < RecordHeader.HEADER_SIZE_BYTES ) {
                throw new EvioException("not enough room in buffer");
            }

            try {
                RecordHeader.writeTrailer(buffer, (int)bytesWritten, recordNumber);
            }
            catch (HipoException e) {/* never happen */}

            bytesToWrite = RecordHeader.HEADER_SIZE_BYTES;
        }
        else {

            // Create the index of record lengths in proper byte order
            byte[] recordIndex = new byte[4 * recordLengths.size()];
            try {
                for (int i = 0; i < recordLengths.size(); i++) {
                    ByteDataTransformer.toBytes(recordLengths.get(i), byteOrder,
                                                recordIndex, 4 * i);
                }
            }
            catch (EvioException e) {/* never happen */}

            // Write trailer with index

            // How many bytes are we writing here?
            bytesToWrite = RecordHeader.HEADER_SIZE_BYTES + recordIndex.length;
//System.out.println("writeTrailerToBuffer: bytesToWrite = " + bytesToWrite + ", record index len = " + recordIndex.length);

            // Make sure our buffer can hold everything
            if ((buffer.capacity() - (int) bytesWritten) < bytesToWrite) {
                throw new EvioException("not enough room in buffer");
            }

            try {
                // Place data into buffer - both header and index
//System.out.println("writeTrailerToBuffer: start writing at pos = " + bytesWritten);
                RecordHeader.writeTrailer(buffer, (int) bytesWritten, recordNumber,
                                          recordIndex);
            }
            catch (HipoException e) {/* never happen */}
        }

        // Keep track of what is written to this file.
        // We did write a record, even if it had no data.
//        recordNumber++;
//        recordsWritten++;
        bytesWritten         += bytesToWrite;
//        int i = bytesWrittenToBuffer;
        bytesWrittenToBuffer += bytesToWrite;
//System.out.println("** writeTrailerToBuffer: buf pos, lim = " + buffer.position() +
//                   ", " + buffer.limit() + ", bytesWrittenToBuffer, " + i+
//                           " --> " + bytesWrittenToBuffer);
    }

}
