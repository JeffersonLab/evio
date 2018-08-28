package org.jlab.coda.jevio;


import org.jlab.coda.hipo.*;

import java.io.*;
import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.util.ArrayList;
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
public class EventWriterMT {

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
     * Offset to where the record length is written in the byte buffer,
     * which always has a physical record header at the top.
     */
    static final int RECORD_LENGTH_OFFSET = 0;

    /**
     * Offset to where the record number is written in the byte buffer,
     * which always has a physical record header at the top.
     */
    static final int RECORD_NUMBER_OFFSET = 4;

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

    /** Mask to get version number from 6th int in record. */
    static final int VERSION_MASK = 0xff;

    /** Size of record header in bytes. */
    static final int headerBytes = 32;



    /** Header for file only. */
    private FileHeader fileHeader;

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

    /** Number of threads doing compression simultaneously. */
    private int compressionThreadCount = 1;

    /** List of record lengths to be optionally written in trailer. */
    private ArrayList<Integer> recordLengths = new ArrayList<Integer>(1500);

    /** Number of bytes written to file/buffer at current moment. */
    private long bytesWritten;

    /** Do we add a last header or trailer to file/buffer? */
    private boolean addTrailer = true;

    /** Do we add a record index to the trailer? */
    private boolean addTrailerIndex = true;

    /** Byte array large enough to hold a header/trailer. */
    private byte[]  headerArray = new byte[RecordHeader.HEADER_SIZE_BYTES];

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

    /** <code>True</code> if appending to file/buffer, <code>false</code> if (over)writing. */
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
    
    /** Total size of the buffer in bytes. */
    private int bufferSize;

    /** The output buffer when writing to a buffer. */
    private ByteBuffer buffer;

    /** Number of bytes written to the current buffer. */
    private long bytesWrittenToBuffer;

    /** Number of events written to final destination buffer or file's current record
     * NOT including dictionary (& first event?). */
    private int eventsWrittenToBuffer;

    //-----------------------
    // File related members
    //-----------------------

    private File currentFile;

    /** The object used for writing a file. */
    private RandomAccessFile raf;

    /** The file channel, used for writing a file, derived from raf. */
    private FileChannel fileChannel;

    /** Split number associated with output file to be written next. */
    private int splitNumber;

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
     * Number of data streams currently active.
     * A data stream is a chain of ROCS and EBs ending in a single specific ER.
     */
    private int streamCount;

    /** Is it OK to overwrite a previously existing file? */
    private boolean overWriteOK;

    /** Number of bytes written to the current file (including ending header),
     *  not the total in all split files. */
    private long bytesWrittenToFile;

    /** Number of events actually written to the current file - not the total in
     * all split files - including dictionary. */
    private int eventsWrittenToFile;


    //-----------------------


    /** Class used to close files in order received, each in its own thread,
     *  to avoid slowing down while file splitting. */
    private final class FileCloser {

        /** Thread pool with 1 thread. */
        private final ExecutorService threadPool;

        FileCloser() {
            threadPool = Executors.newSingleThreadExecutor();
        }

        /**
         * Close the given file, in the order received, in a separate thread.
         * @param raf file to close
         */
        void closeFile(RandomAccessFile raf) {
            threadPool.submit(new CloseThd(raf));
        }

        /** Close the thread pool in this object while executing all existing tasks. */
        void close() {
            threadPool.shutdown();
        }

        private final class CloseThd implements Runnable {
            private final RandomAccessFile raf;

            CloseThd(RandomAccessFile raf) {
                this.raf = raf;
            }

            public final void run() {
                try {
                    raf.close();
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
     * Creates an <code>EventWriterMT</code> for writing to a file in native byte order.
     * If the file already exists, its contents will be overwritten.
     * If it doesn't exist, it will be created.
     *
     * @param file the file object to write to.<br>
     * @throws EvioException file cannot be created
     */
    public EventWriterMT(File file) throws EvioException {
        this(file, false);
    }

    /**
     * Creates an <code>EventWriterMT</code> for writing to a file in native byte order.
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
    public EventWriterMT(File file, boolean append) throws EvioException {
        this(file, null, append);
    }

    /**
     * Creates an <code>EventWriterMT</code> for writing to a file in NATIVE byte order.
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
    public EventWriterMT(File file, String dictionary, boolean append) throws EvioException {
        this(file.getPath(), null, null,
             0, 0, 0, 0,
             ByteOrder.nativeOrder(), dictionary, false,
             append, null, 0, 1,
             0, 1, 8);

    }

    /**
     * Creates an <code>EventWriterMT</code> for writing to a file in native byte order.
     * If the file already exists, its contents will be overwritten.
     * If it doesn't exist, it will be created.
     *
     * @param filename name of the file to write to.<br>
     * @throws EvioException file cannot be created
     */
    public EventWriterMT(String filename) throws EvioException {
        this(filename, false);
    }

    /**
     * Creates an <code>EventWriterMT</code> for writing to a file in NATIVE byte order.
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
    public EventWriterMT(String filename, boolean append) throws EvioException {
        this(filename, append, ByteOrder.nativeOrder());
    }

    /**
     * Creates an <code>EventWriterMT</code> for writing to a file in the
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
    public EventWriterMT(String filename, boolean append, ByteOrder byteOrder) throws EvioException {
        this(filename, null, null,
             0, 0, 0, 0,
             byteOrder, null, false,
             append, null, 0, 1,
             0, 1, 8);
    }

    /**
     * Create an <code>EventWriterMT</code> for writing events to a file.
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
     * @param append        currently appending is not done (false regardless).
     * @param firstEvent    the first event written into each file (after any dictionary)
     *                      including all split files; may be null. Useful for adding
     *                      common, static info into each split file.
     * @param streamId      streamId number (100 &gt; id &gt; -1) for file name
     * @param streamCount   total number of data streams
     * @param compressionType    type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip)
     * @param compressionThreads number of threads doing compression simultaneously
     * @param ringSize           number of records in supply ring, must be multiple of 2
     *                           and &gt;= compressionThreads.
     *
     * @throws EvioException if maxRecordSize or maxEventCount exceed limits;
     *                       if defined dictionary or first event while appending;
     *                       if splitting file while appending;
     *                       if file name arg is null;
     *                       if file could not be opened, positioned, or written to;
     *                       if file exists but user requested no over-writing or appending.
     */
    public EventWriterMT(String baseName, String directory, String runType,
                         int runNumber, long split,
                         int maxRecordSize, int maxEventCount,
                         ByteOrder byteOrder, String xmlDictionary,
                         boolean overWriteOK, boolean append,
                         EvioBank firstEvent, int streamId, int streamCount,
                         int compressionType, int compressionThreads, int ringSize)
            throws EvioException {

        if (baseName == null) {
            throw new EvioException("baseName arg is null");
        }

        if (byteOrder == null) {
            byteOrder = ByteOrder.BIG_ENDIAN;
        }

//        if (append) {
//            if (split > 0) {
//                throw new EvioException("Cannot specify split when appending");
//            }
//            else if ((xmlDictionary != null) || (firstEvent != null)) {
//                throw new EvioException("Cannot specify dictionary or first event when appending");
//            }
//        }

        createCommonRecord(xmlDictionary, firstEvent, null);

        // Store arguments
        this.split         = split;
        //this.append        = append;
        this.append        = false;
        this.runNumber     = runNumber;
        this.byteOrder     = byteOrder;   // byte order may be overwritten if appending
        this.overWriteOK   = overWriteOK;
        this.xmlDictionary = xmlDictionary;
        this.streamCount   = streamCount;

        if (compressionType < 0 || compressionType > 3) {
            compressionType = 0;
        }
        this.compressionType = compressionType;

        if (compressionThreads < 1) {
            compressionThreads = 1;
        }
        this.compressionThreadCount = compressionThreads;

        // Number of ring items must be >= compressionThreadCount
        if (ringSize < compressionThreadCount) {
            ringSize = compressionThreadCount;
        }
        // AND is must be multiple of 2
        /* Number of records in record supply. */
        ringSize = Utilities.powerOfTwo(ringSize, true);

        toFile = true;
        recordNumber = 1;

        // Split file number normally starts at 0.
        // If there are multiple streams, then the initial split number is,
        // streamId. All subsequent split numbers are calculated
        // by adding the streamCount.
        splitNumber = 0;
        if (streamCount > 1) {
            splitNumber = streamId;
        }
        else {
            streamCount = 1;
        }

        // The following may not be backwards compatible.
        // Make substitutions in the baseName to create the base file name.
        if (directory != null) baseName = directory + "/" + baseName;
        StringBuilder builder = new StringBuilder(100);
        specifierCount = Utilities.generateBaseFileName(baseName, runType, builder);
        baseFileName   = builder.toString();
        // Also create the first file's name with more substitutions
        String fileName = Utilities.generateFileName(baseFileName, specifierCount,
                                                     runNumber, split, splitNumber,
                                                     streamId);
        splitNumber += streamCount;
        //System.out.println("EventWriter const: filename = " + fileName);
        //System.out.println("                   basename = " + baseName);
        currentFile = new File(fileName);

        // If we can't overwrite or append and file exists, throw exception
        if (!overWriteOK && !append && (currentFile.exists() && currentFile.isFile())) {
            throw new EvioException("File exists but user requested no over-writing or appending, "
                                            + currentFile.getPath());
        }

        // Evio file
        fileHeader = new FileHeader(true);
        supply = new RecordSupply(ringSize, byteOrder, compressionThreads,
                                  maxEventCount, maxRecordSize, compressionType);

        // Create and start compression threads
        recordCompressorThreads = new RecordCompressor[compressionThreadCount];
        for (int i=0; i < compressionThreadCount; i++) {
            recordCompressorThreads[i] = new RecordCompressor(i);
            recordCompressorThreads[i].start();
        }

        // Create and start writing thread
        recordWriterThread = new RecordWriter();
        recordWriterThread.start();

        // Object to close files in a separate thread when splitting, to speed things up
        if (split > 0) fileCloser = new FileCloser();

//        try {
//            if (append) {
//                // Random file access only used to read existing file
//                // and prepare for stream writes.
//                raf = new RandomAccessFile(currentFile, "rw");
//                fileChannel = raf.getChannel();
//
//                // If we have an empty file, that's OK.
//                // Otherwise we have to examine it for compatibility
//                // and position ourselves for the first write.
//                if (fileChannel.size() > 0) {
//                    // Look at first record header to find endianness & version.
//                    // Endianness given in constructor arg, when appending, is ignored.
//                    examineFirstBlockHeader();
//
//                    // Oops, gotta redo this since file has different byte order
//                    // than specified in constructor arg.
//                    if (this.byteOrder != byteOrder) {
//                        buffer.order(this.byteOrder);
//                    }
//
//                    // Prepare for appending by moving file position to end of last record
//                    toAppendPosition();
//
//                    // File position is now after the last event written.
//
//                    // Reset the buffer which has been used to read the header
//                    // and to prepare the file for event writing.
//                    buffer.clear();
//                }
//            }
//            // If not appending, file is created when data is flushed for the first time
//        }
//        catch (FileNotFoundException e) {
//            throw new EvioException("File could not be opened for writing, " +
//                                            currentFile.getPath(), e);
//        }
//        catch (IOException e) {
//            throw new EvioException("File could not be positioned for appending, " +
//                                            currentFile.getPath(), e);
//        }


//        // Write out the beginning file header
//        try {
//            writeFileHeader();
//        }
//        catch (IOException e) {
//            throw new EvioException(e);
//        }

    }


    //---------------------------------------------
    // BUFFER Constructors
    //---------------------------------------------

    /**
     * Create an <code>EventWriterMT</code> for writing events to a ByteBuffer.
     * Uses the default number and size of blocks in buffer.
     * Will overwrite any existing data in buffer!
     *
     * @param buf            the buffer to write to.
     * @throws EvioException if buf arg is null
     */
    public EventWriterMT(ByteBuffer buf) throws EvioException {

        this(buf, 0, 0, null, 1, false, null, 0);
    }

    /**
     * Create an <code>EventWriterMT</code> for writing events to a ByteBuffer.
     * Uses the default number and size of blocks in buffer.
     *
     * @param buf            the buffer to write to.
     * @param append         if <code>true</code>, all events to be written will be
     *                       appended to the end of the buffer.
     * @throws EvioException if buf arg is null
     */
    public EventWriterMT(ByteBuffer buf, boolean append) throws EvioException {

        this(buf, 0, 0, null, 1, append, null, 0);
    }

    /**
     * Create an <code>EventWriterMT</code> for writing events to a ByteBuffer.
     * Uses the default number and size of blocks in buffer.
     *
     * @param buf            the buffer to write to.
     * @param xmlDictionary  dictionary in xml format or null if none.
     * @param append         if <code>true</code>, all events to be written will be
     *                       appended to the end of the buffer.
     * @throws EvioException if buf arg is null
     */
    public EventWriterMT(ByteBuffer buf, String xmlDictionary, boolean append) throws EvioException {

        this(buf, 0, 0, xmlDictionary, 1, append, null, 0);
    }


    //TODO: et, cmsg
    /**
     * Create an <code>EventWriterMT</code> for writing events to a ByteBuffer.
     * Will overwrite any existing data in buffer!
     *
     * @param buf            the buffer to write to.
     * @param maxRecordSize  max number of data bytes each record can hold.
     *                       Value of &lt; 8MB results in default of 8MB.
     *                       The size of the record will not be larger than this size
     *                       unless a single event itself is larger.
     * @param maxEventCount  max number of events each record can hold.
     *                       Value &lt;= O means use default (1M).
     * @param xmlDictionary  dictionary in xml format or null if none.
     * @param recordNumber   number at which to start record number counting.
     * @throws EvioException if maxRecordSize or maxEventCount exceed limits; if buf arg is null
     */
    public EventWriterMT(ByteBuffer buf, int maxRecordSize, int maxEventCount,
                         String xmlDictionary, int recordNumber) throws EvioException {

        this(buf, maxRecordSize, maxEventCount, xmlDictionary,
                         recordNumber, false, null, 0);
    }


    /**
     * Create an <code>EventWriterMT</code> for writing events to a ByteBuffer.
     *
     * @param buf             the buffer to write to.
     * @param maxRecordSize   max number of data bytes each record can hold.
     *                        Value of &lt; 8MB results in default of 8MB.
     *                        The size of the record will not be larger than this size
     *                        unless a single event itself is larger.
     * @param maxEventCount   max number of events each record can hold.
     *                        Value &lt;= O means use default (1M).
     * @param xmlDictionary   dictionary in xml format or null if none.
     * @param recordNumber    number at which to start record number counting.
     * @param append          if <code>true</code>, all events to be written will be
     *                        appended to the end of the buffer.
     * @param firstEvent      the first event written into the buffer (after any dictionary).
     *                        May be null. Not useful when writing to a buffer as this
     *                        event may be written using normal means.
     * @param compressionType type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip)
     *
     * @throws EvioException if maxRecordSize or maxEventCount exceed limits;
     *                       if buf arg is null;
     *                       if defined dictionary while appending;
     */
    public EventWriterMT(ByteBuffer buf, int maxRecordSize, int maxEventCount,
                         String xmlDictionary, int recordNumber, boolean append,
                         EvioBank firstEvent, int compressionType)
            throws EvioException {


        if (buf == null) {
            throw new EvioException("Buffer arg cannot be null");
        }

        if (append && ((xmlDictionary != null) || (firstEvent != null))) {
            throw new EvioException("Cannot specify dictionary or first event when appending");
        }

        createCommonRecord(xmlDictionary, firstEvent, null);

        this.append          = append;
        this.buffer          = buf;
        this.byteOrder       = buf.order();
        this.recordNumber    = recordNumber;
        this.xmlDictionary   = xmlDictionary;
        this.compressionType = compressionType;

        // Get buffer ready for writing. If we're appending, setting
        // the position to 0 lets us read up to the end of the evio
        // data and find the proper place to append to.
        buffer.position(0);
        bufferSize = buf.capacity();

//        try {
//            if (append) {
//                // Check endianness & version
//                examineFirstBlockHeader();
//
//                // Prepare for appending by moving buffer position
//                toAppendPosition();
//
//                // Buffer position is just before empty last record header
//            }
//        }
//        catch (IOException e) {
//            throw new EvioException("Buffer could not be positioned for appending", e);
//        }

        // When writing to buffer, just fill/compress/write one record at a time
        currentRecord = new RecordOutputStream(byteOrder, maxEventCount,
                                               maxRecordSize, compressionType);
    }


    /**
     * Initialization new buffer (not from constructor).
     * The buffer's position is set to 0 before writing.
     *
     * @param buf            the buffer to write to.
     * @param recordNumber   number at which to start record number counting.
     */
    private void reInitializeBuffer(ByteBuffer buf, int recordNumber) {

        this.buffer       = buf;
        this.byteOrder    = buf.order();
        this.recordNumber = recordNumber;

        // Init variables
        split  = 0L;
        toFile = false;
        closed = false;
        eventsWrittenTotal = 0;
        eventsWrittenToBuffer = 0;
        bytesWrittenToBuffer = 0L;
        bytesWritten = 0L;
        
        // Get buffer ready for writing
        currentRecord.reset();
        buffer.position(0);
        bufferSize = buf.capacity();
    }


    /**
     * Set the buffer being written into (initially set in constructor).
     * This method allows the user to avoid having to create a new EventWriterMT
     * each time a bank needs to be written to a different buffer.
     * This does nothing if writing to a file. Not for use if appending.<p>
     * Do <b>not</b> use this method unless you know what you are doing.
     *
     * @param buf the buffer to write to.
     * @param recordNumber   number at which to start record number counting.
     * @throws EvioException if this object was not closed prior to resetting the buffer,
     *                       buffer arg is null, or in appending mode.
     */
    public void setBuffer(ByteBuffer buf, int recordNumber) throws EvioException {
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

        reInitializeBuffer(buf, recordNumber);
    }


    /**
     * Set the buffer being written into (initially set in constructor).
     * This method allows the user to avoid having to create a new EventWriterMT
     * each time a bank needs to be written to a different buffer.
     * This does nothing if writing to a file. Not for use if appending.<p>
     * Do <b>not</b> use this method unless you know what you are doing.
     *
     * @param buf the buffer to write to.
     * @throws EvioException if this object was not closed prior to resetting the buffer,
     *                       buffer arg is null, or in appending mode.
     */
    public void setBuffer(ByteBuffer buf) throws EvioException {
        setBuffer(buf, 1);
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
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or writeEvent.
     *
     * @return buffer being written into, made ready for reading;
     *         null if writing to file
     */
    public ByteBuffer getByteBuffer() {
        // It does NOT make sense to give the caller the internal buffer
        // used in writing to files. That buffer may contain nothing and
        // most probably won't contain the full file contents.
        if (toFile()) return null;

        ByteBuffer buf = buffer.duplicate().order(buffer.order());

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
    public boolean isClosed() {return closed;}


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
     * Get the current split number which is the split number of file
     * to be written next. Warning, this value may be changing.
     * @return the current split number which is the split number of file to be written next.
     */
    public int getSplitNumber() {return splitNumber;}


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
    public int getEventsWritten() {return eventsWrittenTotal;}


    /**
     * Get the byte order of the buffer/file being written into.
     * @return byte order of the buffer/file being written into.
     */
    public ByteOrder getByteOrder() {return byteOrder;}


    /**
     * Set the number with which to start record numbers.
     * This method does nothing if events have already been written.
     * @param startingRecordNumber  the number with which to start record numbers.
     */
    public void setStartingRecordNumber(int startingRecordNumber) {
        // If events have been written already, forget about it
        if (eventsWrittenTotal > 0) return;
        recordNumber = startingRecordNumber;
    }


    /**
     * Set an event which will be written to the file/buffer as
     * well as to all split files. It's called the "first event" as it will be the
     * first event written to each split file (after the dictionary) if this method
     * is called early enough or the first event was defined in the constructor.<p>
     *
     * Since this method can only called after the constructor, the common record may
     * have already been written with its dictionary and firstEvent.
     * The event given here will be written immediately somewhere in the body of the file
     * if the common record was already written to file. If using a buffer, it will
     * be written as a normal event.<p>
     *
     * The forth-coming split files will have this event in the common record
     * (file header's user header) along with any dictionary.<p>
     *
     * If the firstEvent is given in the constructor, then the
     * caller may end up with N+1 copies of it in a single file if this method
     * is called N times.<p>
     *
     * By its nature this method is not useful for writing to a buffer since
     * it is never split and the event can be written to it as any other.<p>
     *
     * Do not call this while simultaneously calling
     * close, flush, writeEvent, or getByteBuffer.
     *
     * @param node node representing event to be placed first in each file written
     *             including all splits. If null, no more first events are written
     *             to any files.
     * @throws EvioException if first event is opposite byte order of internal buffer;
     *                       if bad data format;
     *                       if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    public void setFirstEvent(EvioNode node) throws EvioException {

        // There's no way to remove an event from a record, so reconstruct it
        createCommonRecord(xmlDictionary, null, node);

        // Write this as a normal event if we've already written the common
        // record to file or if writing to a buffer.
        boolean writeEvent = true;
        if (toFile && recordsWritten < 1) {
            writeEvent = false;
        }

        if (writeEvent) {
            writeEvent(node, false);
        }
    }

// TODO: Next to add method that takes ByteBuffer arg
//    /**
//     * Set an event which will be written to the file/buffer as
//     * well as to all split files. It's called the "first event" as it will be the
//     * first event written to each split file (after the dictionary) if this method
//     * is called early enough or the first event was defined in the constructor.<p>
//     *
//     * Since this method is always called after the constructor, the common block will
//     * have already been written with a dictionary and firstEvent if either was
//     * defined in the constructor. The event given here will be written
//     * immediately somewhere in the body of the file, with the forth-coming split
//     * files having that event in the first block along with any dictionary.<p>
//     *
//     * This means that if the firstEvent is given in the constructor, then the
//     * caller may end up with 2 copies of it in a single file (if this method
//     * is called once). It's also possible to get 2 copies in a file if this
//     * method is called immediately prior to the file splitting.<p>
//     *
//     * By its nature this method is not useful for writing to a buffer since
//     * it is never split and the event can be written to it as any other.<p>
//     *
//     * The method {@link #writeEvent} calls writeCommonBlock() which, in turn,
//     * only gets called when synchronized. So synchronizing this method will
//     * make sure firstEvent only gets set while nothing is being written.
//     *
//     * @param buffer buffer containing event to be placed first in each file written
//     *               including all splits. If null, no more first events are written
//     *               to any files.
//     */
//    synchronized public void setFirstEvent(ByteBuffer buffer)
//            throws EvioException, IOException {
//
//        // If getting rid of the first event ...
//        if (buffer == null) {
//            if (xmlDictionary != null) {
//                commonBlockCount = 1;
//                commonBlockByteSize = dictionaryBytes;
//            }
//            else {
//                commonBlockCount = 0;
//                commonBlockByteSize = 0;
//            }
//            firstEventBytes = 0;
//            firstEventByteArray = null;
//            haveFirstEvent = false;
//            return;
//        }
//
//        // Find the first event's bytes and the memory size needed
//        // to contain the common events (dictionary + first event).
//        if (xmlDictionary != null) {
//            commonBlockCount = 1;
//            commonBlockByteSize = dictionaryBytes;
//        }
//        else {
//            commonBlockCount = 0;
//            commonBlockByteSize = 0;
//        }
//
//        // Copy the buffer arg & get it ready to read
//        int totalEventLength = buffer.remaining();
//        ByteBuffer buf = ByteBuffer.allocate(totalEventLength);
//        buf.put(buffer).limit(totalEventLength).position(0);
//
//        firstEventBytes = buf.array().length;
//        firstEventByteArray = buf.array();
//        commonBlockByteSize += firstEventBytes;
//        commonBlockCount++;
//        haveFirstEvent = true;
//
//        // Write it to the file/buffer now. In this case it may not be the
//        // first event written and some splits may not even have it
//        // (depending on how many events have been written so far).
//        writeEvent(null, buf, false);
//    }


    /**
     * Set an event which will be written to the file/buffer as
     * well as to all split files. It's called the "first event" as it will be the
     * first event written to each split file (after the dictionary) if this method
     * is called early enough or the first event was defined in the constructor.<p>
     *
     * Since this method can only called after the constructor, the common record may
     * have already been written with its dictionary and firstEvent.
     * The event given here will be written immediately somewhere in the body of the file
     * if the common record was already written to file. If using a buffer, it will
     * be written as a normal event.<p>
     *
     * The forth-coming split files will have this event in the common record
     * (file header's user header) along with any dictionary.<p>
     *
     * If the firstEvent is given in the constructor, then the
     * caller may end up with N+1 copies of it in a single file if this method
     * is called N times.<p>
     *
     * By its nature this method is not useful for writing to a buffer since
     * it is never split and the event can be written to it as any other.<p>
     *
     * Do not call this while simultaneously calling
     * close, flush, writeEvent, or getByteBuffer.
     *
     * @param bank event to be placed first in each file written including all splits.
     *             If null, no more first events are written to any files.
     * @throws EvioException if first event is opposite byte order of internal buffer;
     *                       if bad data format;
     *                       if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    public void setFirstEvent(EvioBank bank) throws EvioException {

        // There's no way to remove an event from a record, so reconstruct it
        createCommonRecord(xmlDictionary, bank, null);

        // Write this as a normal event if we've already written the common
        // record to file or if writing to a buffer.
        boolean writeEvent = true;
        if (toFile && recordsWritten < 1) {
            writeEvent = false;
        }

        if (writeEvent) {
            writeEvent(bank, null, false);
        }
    }


    /**
     * Create and fill the common record which contains the dictionary and first event.
     * will use firstBank as the first event if specified, else will try using the
     * firstNode.
     *
     * @param xmlDictionary  xml dictionary
     * @param firstBank      first event as EvioBank
     * @param firstNode      first event as EvioNode
     * @throws EvioException if dictionary is in improper format
     */
    private void createCommonRecord(String xmlDictionary, EvioBank firstBank, EvioNode firstNode)
                            throws EvioException {

        // Create record if necessary, else clear it
        if (commonRecord == null) {
            commonRecord = new RecordOutputStream(byteOrder, 0, 0, compressionType);
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

            // Turn dictionary data into bytes as an evio event
            dictionaryByteArray = Utilities.stringToBank(xmlDictionary, (short)0, (byte)0, byteOrder);

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
        else {
            firstEventByteArray = null;
        }

        commonRecord.build();
    }


    /**
     * Create and write a general file header into the file.
     * The general header's user header is the common record which
     * contains any existing dictionary and first event.<p>
     *
     * Call this method AFTER file split or, in constructor, after the file
     * name is created in order to ensure a consistent value for file split number.
     */
    private void writeFileHeader() throws IOException {
        // For the file header, our "user header" will be the common record,
        // which is a record containing the dictionary and first event.

        fileHeader.reset();
        // File split # in header. Go back to last one as currently is set for the next split.
        fileHeader.setFileNumber(splitNumber - streamCount);
        int commonSize = commonRecord.getHeader().getLength();
        fileHeader.setUserHeaderLength(commonSize);

        // TODO: This is now unnecessary as the above call takes care of it
        // Total header size in bytes (no padding for evio data)
        int bytes = RecordHeader.HEADER_SIZE_BYTES + commonSize;
        fileHeader.setLength(bytes);

        byte[] array = new byte[bytes];
        ByteBuffer buffer = ByteBuffer.wrap(array);
        buffer.order(byteOrder);

        // Write file header into array
        try {
            fileHeader.writeHeader(buffer, 0);
        }
        catch (HipoException e) {/* never happen */}
        
        // Write user header into array
        System.arraycopy(commonRecord.getBinaryBuffer().array(), 0, array,
                         RecordHeader.HEADER_SIZE_BYTES, commonSize);
        // Write array into file
        raf.write(array, 0, bytes);

        eventsWrittenTotal = eventsWrittenToFile = commonRecord.getEventCount();
        bytesWrittenToFile = bytesWritten = bytes;
    }


    /**
     * This method flushes any remaining internally buffered data to file.
     * Calling {@link #close()} automatically does this so it isn't necessary
     * to call before closing. This method should only be used when writing
     * events at such a low rate that it takes an inordinate amount of time
     * for internally buffered data to be written to the file.<p>
     *
     * Calling this can kill performance. May not call this when simultaneously
     * calling writeEvent, close, setFirstEvent, or getByteBuffer.
     */
    public void flush() {

        if (closed || !toFile) {
            return;
        }

        // Write any existing data.
        // Tell writer to force this record to disk which kills performance!
        currentRingItem.forceToDisk(true);
        // Send current record back to ring
        supply.publish(currentRingItem);

        // Get another empty record from ring
        currentRingItem = supply.get();
        currentRecord = currentRingItem.getRecord();
    }



    /**
     * This method flushes any remaining data to file and disables this object.
     * May not call this when simultaneously calling
     * writeEvent, flush, setFirstEvent, or getByteBuffer.
     */
    public void close() {
        if (closed) {
            return;
        }

        if (toFile) {
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
        else {
            // TODO: Close writing to buffer
            if (currentRecord.getEventCount() > 0) {
                // --------
            }
        }

        // Write any remaining data
        if (toFile) {
            if (raf != null) {
                if (addTrailer) {
                    // Write the trailer
                    try {
                        writeTrailerToFile(addTrailerIndex);
                    }
                    catch (IOException e) {
                        e.printStackTrace();
                    }
                }
            }
        }
        else {
            // Data is written, but need to write empty last header
            try {
                writeTrailerToBuffer(addTrailerIndex);
            }
            catch (EvioException e) {
                // We're here if buffer is too small
                e.printStackTrace();
            }
        }

        // Close everything
        if (toFile) {
            try {
                // Close current file
                if (raf != null) raf.close();
            }
            catch (IOException e) {}
            
            // Close split file handler thread pool
            if (fileCloser != null) fileCloser.close();
        }

        closed = true;
    }

    
    /**
     * Reads part of the first record (physical record) header in order to determine
     * the evio version # and endianness of the file or buffer in question. These things
     * do <b>not</b> need to be examined in subsequent record headers.
     *
     * @return status of read attempt
     */
    protected IOStatus examineFirstBlockHeader()
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
            if (evioVersion < 6)  {
System.out.println("ERROR: evio version# = " + evioVersion);
                return IOStatus.EVIO_EXCEPTION;
            }

            // Is there a dictionary?
            // TODO: Change this
            hasAppendDictionary = BlockHeaderV4.hasDictionary(bitInfo);


//            int blockLen   = buffer.getInt(currentPosition + BLOCK_LENGTH_OFFSET);
//            int headerLen  = buffer.getInt(currentPosition + HEADER_LENGTH_OFFSET);
//            int eventCount = buffer.getInt(currentPosition + EVENT_COUNT_OFFSET);
//            int recordNum  = buffer.getInt(currentPosition + BLOCK_NUMBER_OFFSET);
//
//            boolean lastBlock = BlockHeaderV4.isLastBlock(bitInfo);
//
//            System.out.println("blockLength     = " + blockLen);
//            System.out.println("recordNumber    = " + recordNum);
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
     * in append mode. It places the writing position after the last event (not record header).
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

        // The file's record #s may be fine or they may be messed up.
        // Assume they start with one and increment from there. That
        // way any additional records now added to the file will have a
        // reasonable # instead of incrementing from the last existing
        // record.
        recordNumber = 1;

        while (true) {
            nBytes = 0;

            // Read in 8 ints (32 bytes) of record header
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
                        // Missing last empty record header
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
            blockLength     = buffer.getInt(currentPosition + RECORD_LENGTH_OFFSET);
//            recordNum       = buffer.getInt(currentPosition + BLOCK_NUMBER_OFFSET);
            headerLength    = buffer.getInt(currentPosition + HEADER_LENGTH_OFFSET);
            blockEventCount = buffer.getInt(currentPosition + EVENT_COUNT_OFFSET);
            lastBlock       = BlockHeaderV4.isLastBlock(bitInfo);

//            System.out.println("bitInfo         = 0x" + Integer.toHexString(bitInfo));
//            System.out.println("blockLength     = " + blockLength);
//            System.out.println("recordNumber    = " + recordNum);
//            System.out.println("headerLength    = " + headerLength);
//            System.out.println("blockEventCount = " + blockEventCount);
//            System.out.println("lastBlock       = " + lastBlock);
//            System.out.println();

            // Track total number of events in file/buffer (minus dictionary)
            eventsWrittenTotal += blockEventCount;

            recordNumber++;

            // Stop at the last record. The file may not have a last record if
            // improperly terminated. Running into an End-Of-File will flag
            // this condition.
            if (lastBlock || readEOF) {
                break;
            }

            // Hop to next record header
            if (toFile) {
                int bytesToNextBlockHeader = 4*blockLength - 32;
                if (bytesLeftInFile < bytesToNextBlockHeader) {
                    throw new EvioException("bad file format");
                }
                fileChannel.position(fileChannel.position() + bytesToNextBlockHeader);
                bytesLeftInFile -=  bytesToNextBlockHeader;
            }
            else {
                // Is there enough buffer space to hop over record?
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
        // If we're here, we've just read the last record header (at least 8 words of it).
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
        }
        // If last record has event(s) in it ...
        else if (blockLength > headerLength) {
            // Clear last record bit in 6th header word
            bitInfo = BlockHeaderV4.clearLastBlockBit(bitInfo);

            // Rewrite header word with new bit info & hop over record

            // File now positioned right after the last header to be read
            if (toFile) {
                // Back up to before 6th record header word
                fileChannel.position(fileChannel.position() - (32 - BIT_INFO_OFFSET));
//System.out.println("toAppendPosition: writing over last record's 6th word, back up %d words" +(8 - 6));

                // Write over 6th record header word
                buffer.clear();
                buffer.putInt(bitInfo);
                buffer.flip();
                while (buffer.hasRemaining()) {
                    fileChannel.write(buffer);
                }

                // Hop over the entire record
//System.out.println("toAppendPosition: wrote over last record's 6th word, hop over %d words" +
// (blockLength - (6 + 1)));
                fileChannel.position(fileChannel.position() + 4 * blockLength - (BIT_INFO_OFFSET + 1));
            }
            // Buffer still positioned right before the last header to be read
            else {
//System.out.println("toAppendPosition: writing bitInfo (" +
//                   Integer.toHexString(bitInfo) +  ") over last record's 6th word for buffer at pos " +
//                   (buffer.position() + BIT_INFO_OFFSET));

                // Write over 6th record header word
                buffer.putInt(buffer.position() + BIT_INFO_OFFSET, bitInfo);

                // Hop over the entire record
                buffer.position(buffer.position() + 4*blockLength);
            }
        }
        // else if last record has NO data in it ...
        else {
            // We already read in the record header, now back up so we can overwrite it.
            // If using buffer, we never incremented the position, so we're OK.
            recordNumber--;
            if (toFile) {
                fileChannel.position(fileChannel.position() - 32);
//System.out.println("toAppendPos: position (bkup) = " + fileChannel.position());
            }
        }

        // Write empty last record header. Thus if our program crashes, the file
        // will be OK. This last record header will be over-written with each
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
//System.out.println("toAppendPos: at END, recordNum = " + recordNumber);
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
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * The bank in this case is the one represented by the node argument.
     * Once the record is full and if writing to a file, the record will be
     * sent to a thread which may compress the data, then it will be
     * sent to a thread to write the record to file.
     * If writing to a buffer, once the record is full, it may be compressed
     * and will be written into the buffer.<p>
     *
     * The buffer must contain only the event's data (event header and event data)
     * and must <b>not</b> be in complete evio file format.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     *<b>kill</b> performance.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param node   object representing the event to write in buffer form
     * @param force  if writing to disk, force it to write event to the disk.
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    public void writeEvent(EvioNode node, boolean force) throws EvioException {
        // Duplicate buffer so we can set pos & limit without messing others up
        writeEvent(node, force, true);
    }

    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * The bank in this case is the one represented by the node argument.
     * Once the record is full and if writing to a file, the record will be
     * sent to a thread which may compress the data, then it will be
     * sent to a thread to write the record to file.
     * If writing to a buffer, once the record is full, it may be compressed
     * and will be written into the buffer.<p>
     *
     * The buffer must contain only the event's data (event header and event data)
     * and must <b>not</b> be in complete evio file format.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     *<b>kill</b> performance. A true 3rd arg can be used when the backing buffer
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
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     *                       if null node arg;
     */
    public void writeEvent(EvioNode node, boolean force, boolean duplicate)
            throws EvioException {

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
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file, the record will be
     * sent to a thread which may compress the data, then it will be
     * sent to a thread to write the record to file.
     * If writing to a buffer, once the record is full, it may be compressed
     * and will be written into the buffer.<p>
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
     * @param eventBuffer the event (bank) to write in buffer form
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    public void writeEvent(ByteBuffer eventBuffer) throws EvioException {
        writeEvent(null, eventBuffer, false);
    }

    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file, the record will be
     * sent to a thread which may compress the data, then it will be
     * sent to a thread to write the record to file.
     * If writing to a buffer, once the record is full, it may be compressed
     * and will be written into the buffer.<p>
     *
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param bank the bank to write.
     * @throws EvioException if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    public void writeEvent(EvioBank bank) throws EvioException {
        writeEvent(bank, null, false);
    }


    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file, the record will be
     * sent to a thread which may compress the data, then it will be
     * sent to a thread to write the record to file.
     * If writing to a buffer, once the record is full, it may be compressed
     * and will be written into the buffer.<p>
     *
     * The buffer must contain only the event's data (event header and event data)
     * and must <b>not</b> be in complete evio file format.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     *<b>kill</b> performance.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param bankBuffer the bank (as a ByteBuffer object) to write.
     * @param force      if writing to disk, force it to write event to the disk.
     *
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    public void writeEvent(ByteBuffer bankBuffer, boolean force) throws EvioException {
        writeEvent(null, bankBuffer, force);
    }


    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file, the record will be
     * sent to a thread which may compress the data, then it will be
     * sent to a thread to write the record to file.
     * If writing to a buffer, once the record is full, it may be compressed
     * and will be written into the buffer.<p>
     *
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * Be warned that injudicious use of the 2nd arg, the force flag, will
     * <b>kill</b> performance.
     *
     * @param bank   the bank to write.
     * @param force  if writing to disk, force it to write event to the disk.
     *
     * @throws EvioException if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    public void writeEvent(EvioBank bank, boolean force) throws EvioException {
        writeEvent(bank, null, force);
    }


    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file, the record will be
     * sent to a thread which may compress the data, then it will be
     * sent to a thread to write the record to file.
     * If writing to a buffer, once the record is full, it may be compressed
     * and will be written into the buffer.<p>
     *
     * The event to be written may be in one of two forms.
     * The first is as an EvioBank object and the second is as a ByteBuffer
     * containing only the event's data (event header and event data) and must
     * <b>not</b> be in complete evio file format.
     * The first non-null of the bank arguments will be written.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     *<b>kill</b> performance.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param bank       the bank (as an EvioBank object) to write.
     * @param bankBuffer the bank (as a ByteBuffer object) to write.
     * @param force      if writing to disk, force it to write event to the disk.
     *
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if bad bankBuffer format;
     *                       if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    private void writeEvent(EvioBank bank, ByteBuffer bankBuffer, boolean force)
            throws EvioException {

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
                throw new EvioException("inconsistent event lengths");
            }
        }
        else if (bank != null) {
            currentEventBytes = bank.getTotalBytes();
        }
        else {
            return;
        }

        // If writing to buffer, we're not multi-threading compression & writing.
        // Do it all in this thread, right now.
        if (!toFile) {
            writeToBuffer(bank, bankBuffer);
            return;
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
            recordsWritten++;
            // Set flag to split file
            currentRingItem.splitFileAfterWrite(true);
            // Send current record back to ring without adding event
            supply.publish(currentRingItem);

            // Get another empty record from ring
            currentRingItem = supply.get();
            currentRecord = currentRingItem.getRecord();
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
            recordsWritten++;
            // Send current record back to ring without adding event
            supply.publish(currentRingItem);

            // Get another empty record from ring
            currentRingItem = supply.get();
            currentRecord = currentRingItem.getRecord();

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
            recordsWritten++;
            // Tell writer to force this record to disk
            currentRingItem.forceToDisk(true);
            // Send current record back to ring
            supply.publish(currentRingItem);

            // Get another empty record from ring
            currentRingItem = supply.get();
            currentRecord = currentRingItem.getRecord();
        }
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
     * Write record to file. Does nothing if close() already called.
     *
     * @param force force it to write event to the disk.
     * @return {@code false} if no data written, else {@code true}
     *
     * @throws EvioException if this object already closed;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     * @throws IOException   if error writing file
     */
    private boolean writeToFile(RecordRingItem item, boolean force)
                                throws EvioException, IOException {
        if (closed) {
            throw new EvioException("close() has already been called");
        }

        // This actually creates the file so do it only once
        if (bytesWrittenToFile < 1) {
//System.out.println("    flushToFile: create file " + currentFile.getName());
            try {
                raf = new RandomAccessFile(currentFile, "rw");
                fileChannel = raf.getChannel();
            }
            catch (FileNotFoundException e) {
                throw new EvioException("File could not be opened for writing, " +
                                                currentFile.getPath(), e);
            }

            // Write out the beginning file header including common record
            writeFileHeader();
        }

        // Do write
        RecordOutputStream record = item.getRecord();
        RecordHeader header = record.getHeader();
//System.out.println("   Writer: got record, header = \n" + header);

        // Record length of this record
        int bytesToWrite = header.getLength();
        recordLengths.add(bytesToWrite);
        int eventCount = header.getEntries();

        try {
            ByteBuffer buf = record.getBinaryBuffer();
            if (buf.hasArray()) {
//System.out.println("   Writer: use outStream to write file, buf pos = " + buf.position() +
//        ", lim = " + buf.limit() + ", bytesToWrite = " + bytesToWrite);
                raf.write(buf.array(), 0, bytesToWrite);
            }
            else {
//System.out.println("   Writer: use fileChannel to write file");
                // binary buffer is ready to read after build()
                while (buf.hasRemaining()) {
                    fileChannel.write(buf);
                }
            }
            record.reset();

        } catch (IOException ex) {
            throw new EvioException("Error writing to file, " +
                    currentFile.getPath(), ex);
        }

        // Force it to write to physical disk (KILLS PERFORMANCE!!!, 15x-20x slower),
        // but don't bother writing the metadata (arg to force()) since that slows it
        // down even more.
        if (force) fileChannel.force(false);

        // Keep track of what is written to this, one, file
        recordNumber++;
        recordsWritten++;
        bytesWritten        += bytesToWrite;
        bytesWrittenToFile  += bytesToWrite;
        eventsWrittenToFile += eventCount;
        eventsWrittenTotal  += eventCount;

//        if (debug) {
//            System.out.println("    flushToFile: after last header written, Events written to:");
//            System.out.println("                 cnt total (no dict) = " + eventsWrittenTotal);
//            System.out.println("                 file cnt total (dict) = " + eventsWrittenToFile);
//            System.out.println("                 internal buffer cnt (dict) = " + eventsWrittenToBuffer);
//            System.out.println("                 current  record  cnt (dict) = " + currentRecordEventCount);
//            System.out.println("                 bytes-written  = " + bytesWritten);
//            System.out.println("                 bytes-to-file = " + bytesWrittenToFile);
//            System.out.println("                 record # = " + recordNumber);
//        }

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
    private void splitFile() throws EvioException, IOException {

        if (raf != null) {
            if (addTrailer) {
                // Write the trailer
                writeTrailerToFile(addTrailerIndex);
            }
            //System.out.println("[writer] ---> bytes written " + writerBytesWritten);

            // Close existing file (in separate thread for speed)
            // which will also flush remaining data.
            fileCloser.closeFile(raf);
        }

        // Right now no file is open for writing
        raf = null;

        // Create the next file's name
        String fileName = Utilities.generateFileName(baseFileName, specifierCount,
                                                     runNumber, split, splitNumber);
        splitNumber += streamCount;
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

//System.out.println("    splitFile: generated file name = " + fileName);
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
                FileHeader.writeTrailer(headerArray, recordNumber, byteOrder, null);
            }
            catch (HipoException e) {/* never happen */}
            bytesToWrite = RecordHeader.HEADER_SIZE_BYTES;
            raf.write(headerArray, 0, RecordHeader.HEADER_SIZE_BYTES);
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
            }

            // Place data into headerArray - both header and index
            try {
                FileHeader.writeTrailer(headerArray, recordNumber,
                                        byteOrder, recordIndex);
            }
            catch (HipoException e) {/* never happen */}
            raf.write(headerArray, 0, bytesToWrite);
        }

        // Find & update file header's trailer position word
        raf.seek(FileHeader.TRAILER_POSITION_OFFSET);
        if (byteOrder == ByteOrder.LITTLE_ENDIAN) {
            raf.writeLong(Long.reverseBytes(trailerPosition));
        }
        else {
            raf.writeLong(trailerPosition);
        }

        // Find & update file header's bit-info word
        if (addTrailerIndex) {
            raf.seek(FileHeader.BIT_INFO_OFFSET);
            int bitInfo = fileHeader.setBitInfo(false, false, true);
            if (byteOrder == ByteOrder.LITTLE_ENDIAN) {
                raf.writeInt(Integer.reverseBytes(bitInfo));
            }
            else {
                raf.writeInt(bitInfo);
            }
        }

        // Keep track of what is written to this file.
        // We did write a record, even if it had no data.
        recordNumber++;
        recordsWritten++;
        bytesWritten       += bytesToWrite;
        bytesWrittenToFile += bytesToWrite;

        return;
    }


    /**
     * Write bank to current record. If it doesn't fit, write record to buffer
     * and add event to newly emptied record.<p>
     *
     * Does nothing if already closed.
     *
     * @throws EvioException if this object already closed
     */
    private void writeToBuffer(EvioBank bank, ByteBuffer bankBuffer)
                                    throws EvioException {
        if (closed) {
            throw new EvioException("close() has already been called");
        }

        boolean fitInRecord;

        if (bankBuffer != null) {
            fitInRecord = currentRecord.addEvent(bankBuffer);
        }
        else {
            fitInRecord = currentRecord.addEvent(bank);
        }

        // If it fit into record, we're done
        if (fitInRecord) {
            return;
        }

        // Get record header
        RecordHeader header = currentRecord.getHeader();

        // Get/set record info
        header.setRecordNumber(recordNumber);
        int bytesToWrite = header.getLength();
        // Store length here for possible trailer index
        recordLengths.add(bytesToWrite);
        int eventCount = header.getEntries();

        // Do compression
        currentRecord.build();

        // Write to record (without event) to buffer.
        // Binary buffer is ready to read after build().
        ByteBuffer buf = currentRecord.getBinaryBuffer();
        if (buf.hasArray() && buffer.hasArray()) {
            System.arraycopy(buf.array(), 0, buffer.array(),
                             buffer.position(), bytesToWrite);
        }
        else {
            buffer.put(buf);
        }

        // Keep track of what is written
        recordNumber++;
        recordsWritten++;
        bytesWritten          += bytesToWrite;
        bytesWrittenToBuffer  += bytesToWrite;
        eventsWrittenToBuffer += eventCount;
        eventsWrittenTotal    += eventCount;

        // Now the single, current event is guaranteed to fit into record
        currentRecord.reset();
        if (bankBuffer != null) {
            currentRecord.addEvent(bankBuffer);
        }
        else {
            currentRecord.addEvent(bank);
        }
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
                FileHeader.writeTrailer(buffer, (int)bytesWritten,
                                        recordNumber, byteOrder, null);
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

            // Make sure our buffer can hold everything
            if ((buffer.capacity() - (int) bytesWritten) < bytesToWrite) {
                throw new EvioException("not enough room in buffer");
            }

            try {
                // Place data into buffer - both header and index
                FileHeader.writeTrailer(buffer, (int) bytesWritten, recordNumber,
                                        byteOrder, recordIndex);
            }
            catch (HipoException e) {/* never happen */}
        }

        // Keep track of what is written to this file.
        // We did write a record, even if it had no data.
        recordNumber++;
        recordsWritten++;
        bytesWritten         += bytesToWrite;
        bytesWrittenToBuffer += bytesToWrite;
    }

}
