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

import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.concurrent.*;

/**
 * An EventWriter object is used for writing events to a file or to a byte buffer.
 * This class does NOT write versions 1-4 data, only version 6!
 * This class is not thread-safe.
 *
 * <pre><code>
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
 * </code></pre>
 *
 * @author timmer
 */
public class EventWriter extends EventWriterUnsync {

    //---------------------------------------------
    // FILE Constructors
    //---------------------------------------------

    /**
     * Creates an object for writing to a file in native byte order.
     * If the file already exists, its contents will be overwritten.
     * If it doesn't exist, it will be created.
     *
     * @param file the file object to write to.<br>
     * @throws EvioException file cannot be created
     */
    public EventWriter(File file) throws EvioException {
        super(file);
    }

    /**
     * Creates an object for writing to a file in native byte order.
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
        super(file, append);
    }

    /**
     * Creates an object for writing to a file in NATIVE byte order.
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
        super(file, dictionary, append);    }

    /**
     * Creates an object for writing to a file in native byte order.
     * If the file already exists, its contents will be overwritten.
     * If it doesn't exist, it will be created.
     *
     * @param filename name of the file to write to.<br>
     * @throws EvioException file cannot be created
     */
    public EventWriter(String filename) throws EvioException {
        super(filename);
    }

    /**
     * Creates an object for writing to a file in NATIVE byte order.
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
        super(filename, append);
    }

    /**
     * Creates an object for writing to a file in the
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
        super(filename, append, byteOrder);
    }

    /**
     * <p>
     * Create an object for writing events to a file or files.
     * If the given file already exists, its contents will be overwritten
     * unless the "overWriteOK" argument is <code>false</code> in
     * which case an exception will be thrown. If the option to
     * "append" these events to an existing file is <code>true</code>,
     * then the write proceeds. If the file doesn't exist,
     * it will be created. Byte order defaults to big endian if arg is null.</p>
     *
     * <p>In order to keep files from getting too large, there is an option to continue writing
     * but to multiple files instead of just one. That is, when a file gets to the size given by
     * the "split" arg, it is closed and another is opened, with new writes going to the new file.
     * The trick in this case is to automatically name the new files.</p>
     *
     * <p>The baseName arg is the base string from which a final file name or names can be created.
     * The baseName may contain up to 3, C-style integer format specifiers using "d" and "x"
     * (such as <b>%03d</b>, or <b>%x</b>). These specifiers dictate how the runNumber, streamId and
     * splitNumber arguments are inserted into the filename.
     *
     * If there are multiple streams of data each writing a file, to avoid ending up with
     * the same file name, they can be differentiated by a stream id. Run number can differentiate
     * data files from different runs of the same experiment. And of course, the split number
     * tracks the number of files automatically created by this EventWriter object and is incremented
     * at each split.</p>
     *
     * <p>Back to the format specifiers, if more than 3 are found, an exception will be thrown.
     * If no "0" precedes any integer between the "%" and the "d" or "x" of the format specifier,
     * it will be added automatically in order to avoid spaces in the returned string.
     * See the documentation at
     * {@link Utilities#generateFileName(String,int,int, long, int,int, int)}
     * to understand exactly how substitutions
     * of runNumber, streamId and splitNumber are done for these specifiers in order to create
     * the split file names.</p>
     *
     * <p>In addition, the baseName may contain characters of the form
     * <b>$(ENV_VAR)</b> which will be substituted with the value of the associated environmental
     * variable or a blank string if none is found.</p>
     *
     * <p>Finally, the baseName may contain occurrences of the string "%s"
     * which will be substituted with the value of the runType arg or nothing if
     * the runType is null.</p>
     *
     * If there are multiple compression threads, then the maxRecordSize arg is used to determine
     * the max size of each record in the internally created RecordSupply. If a single event,
     * larger than this max size were to be written, a record's memory is expanded to
     * accommodate it. However, if there is only 1 compression/writing thread, this argument
     * is not used and the record will be a max size given by the bufferSize arg. If a single
     * event larger than this were then to be written, an exception would be thrown.
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
     *                      unless a single event itself is larger. Used only if compressionThreads
     *                      &gt; 1.
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
     * @param streamId      streamId number (100 &gt; id &gt; -1) for file name
     * @param splitNumber   number at which to start the split numbers
     * @param splitIncrement amount to increment split number each time another file is created.
     * @param streamCount    total number of streams in DAQ.
     * @param compressionType    type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip).
     * @param compressionThreads number of threads doing compression simultaneously. Defaults to
     *                      1 if = 0 or 1. If 1, just uses the main thread to do compression,
     *                      if any, and handling of writing to file.
     * @param ringSize      number of records in supply ring. If set to &lt; compressionThreads,
     *                      it is forced to equal that value and is also forced to be a multiple of
     *                      2, rounded up.
     * @param bufferSize    number of bytes to make each internal buffer which will
     *                      be storing events before writing them to a file.
     *                      9MB = default if bufferSize = 0. Used only if compresstionThreads = 1.
     *
     * @throws EvioException if maxRecordSize or maxEventCount exceed limits;
     *                       if streamCount &gt; 1 and streamId &lt; 0;
     *                       if defined dictionary or first event while appending;
     *                       if splitting file while appending;
     *                       if file name arg is null;
     *                       if file could not be opened, positioned, or written to;
     *                       if file exists but user requested no over-writing or appending;
     *                       if streamId &lt; 0, splitNumber &lt; 0, or splitIncrement &lt; 1.
     */
    public EventWriter(String baseName, String directory, String runType,
                       int runNumber, long split,
                       int maxRecordSize, int maxEventCount,
                       ByteOrder byteOrder, String xmlDictionary,
                       boolean overWriteOK, boolean append,
                       EvioBank firstEvent, int streamId,
                       int splitNumber, int splitIncrement, int streamCount,
                       CompressionType compressionType, int compressionThreads,
                       int ringSize, int bufferSize)
            throws EvioException {

        super(baseName, directory, runType,
                runNumber, split,
                maxRecordSize, maxEventCount,
                byteOrder, xmlDictionary,
                overWriteOK, append,
                firstEvent, streamId,
                splitNumber, splitIncrement, streamCount,
                compressionType, compressionThreads,
                ringSize, bufferSize);
    }


    //---------------------------------------------
    // BUFFER Constructors
    //---------------------------------------------

    /**
     * Create an object for writing events to a ByteBuffer.
     * Uses the default number and size of records in buffer.
     * Will overwrite any existing data in buffer!
     *
     * @param buf            the buffer to write to.
     * @throws EvioException if buf arg is null
     */
    public EventWriter(ByteBuffer buf) throws EvioException {

        super(buf);
    }

    /**
     * Create an object for writing events to a ByteBuffer.
     * Uses the default number and size of records in buffer.
     *
     * @param buf            the buffer to write to.
     * @param xmlDictionary  dictionary in xml format or null if none.
     * @throws EvioException if buf arg is null
     */
    public EventWriter(ByteBuffer buf, String xmlDictionary) throws EvioException {

        super(buf, xmlDictionary);
    }


    /**
     * Create an object for writing events to a ByteBuffer.
     * The buffer's position is set to 0 before writing.
     * <b>When writing a buffer, only 1 record is used.</b>
     * Any dictionary will be put in a commonRecord and that record will be
     * placed in the user header associated with the single record.
     *
     * @param buf             the buffer to write to starting at position = 0.
     * @param maxRecordSize   max number of data bytes each record can hold.
     *                        Value of &lt; 8MB results in default of 8MB.
     *                        The size of the record will not be larger than this size
     *                        unless a single event itself is larger.
     *                        <b>Currently this arg is unused. Only 1 record will be
     *                        used in the given buffer.</b>
     * @param maxEventCount   max number of events each record can hold.
     *                        Value &lt;= O means use default (1M).
     * @param xmlDictionary   dictionary in xml format or null if none.
     * @param recordNumber    number at which to start record number counting.
     * @param compressionType type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip)
     *
     * @throws EvioException if maxRecordSize or maxEventCount exceed limits;
     *                       if buf arg is null;
     */
    public EventWriter(ByteBuffer buf, int maxRecordSize, int maxEventCount,
                       String xmlDictionary, int recordNumber,
                       CompressionType compressionType)
            throws EvioException {

        super(buf, maxRecordSize, maxEventCount,
                xmlDictionary, recordNumber,
                compressionType);
    }



    /**
     * Create an object for writing events to a ByteBuffer.
     * The buffer's position is set to 0 before writing.
     * <b>When writing a buffer, only 1 record is used.</b>
     * Any dictionary will be put in a commonRecord and that record will be
     * placed in the user header associated with the single record.
     *
     * @param buf             the buffer to write to starting at position = 0.
     * @param maxRecordSize   max number of data bytes each record can hold.
     *                        Value of &lt; 8MB results in default of 8MB.
     *                        The size of the record will not be larger than this size
     *                        unless a single event itself is larger.
     *                        <b>Currently this arg is unused. Only 1 record will be
     *                        used in the given buffer.</b>
     * @param maxEventCount   max number of events each record can hold.
     *                        Value &lt;= O means use default (1M).
     * @param xmlDictionary   dictionary in xml format or null if none.
     * @param recordNumber    number at which to start record number counting.
     * @param compressionType type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip)
     * @param eventType       first record header holds the following type of event encoded in bitInfo,
     *                        (0=RocRaw, 1=Physics, 2=Partial Physics, 3=Disentangled,
     *                         4=User, 5=Control, 6=Mixed, 8=RocRawStream, 9=PhysicsStream, 15=Other).
     *                        If &lt; 0 or &gt; 15, this arg is ignored.
     *
     * @throws EvioException if maxRecordSize or maxEventCount exceed limits;
     *                       if buf arg is null;
     */
    public EventWriter(ByteBuffer buf, int maxRecordSize, int maxEventCount,
                       String xmlDictionary, int recordNumber,
                       CompressionType compressionType, int eventType)
            throws EvioException {

        super(buf, maxRecordSize, maxEventCount,
                xmlDictionary, recordNumber,
                compressionType, eventType);
    }



    //--------------------------------------------------------------
    // The following methods overwrite the equivalent methods in
    // EventWriterUnsync. They are mutex protected for thread
    // safety.
    //--------------------------------------------------------------

    /** {@inheritDoc} */
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
            buf.limit((int)bytesWritten);
        }

        return buf;
    }

    /** {@inheritDoc} */
    synchronized public void setFirstEvent(EvioNode node) throws EvioException, IOException {
        super.setFirstEvent(node);
    }

    /** {@inheritDoc} */
    synchronized public void setFirstEvent(ByteBuffer buffer) throws EvioException, IOException {
        super.setFirstEvent(buffer);
    }

    /** {@inheritDoc} */
    synchronized public void setFirstEvent(EvioBank bank) throws EvioException, IOException {
        super.setFirstEvent(bank);
    }

    /** {@inheritDoc} */
    synchronized public void flush() {super.flush();}

    /** {@inheritDoc} */
    synchronized public void close() {super.close();}

    /** {@inheritDoc} */
    synchronized protected boolean writeEvent(EvioBank bank, ByteBuffer bankBuffer,
                                            boolean force, boolean ownRecord)
            throws EvioException, IOException {

        return super.writeEvent(bank, bankBuffer, force, ownRecord);
    }

    /** {@inheritDoc} */
    synchronized public boolean writeEventToFile(EvioBank bank, ByteBuffer bankBuffer,
                                                 boolean force)
            throws EvioException, IOException {

        return super.writeEventToFile(bank, bankBuffer, force);
    }

    /** {@inheritDoc} */
    synchronized public boolean writeEventToFile(EvioBank bank, ByteBuffer bankBuffer,
                                                 boolean force, boolean ownRecord)
            throws EvioException, IOException {

        return super.writeEventToFile(bank, bankBuffer, force, ownRecord);
    }

    /** {@inheritDoc} */
    synchronized protected boolean writeToFile(boolean force, boolean checkDisk)
            throws EvioException, IOException,
            InterruptedException, ExecutionException {

        return super.writeToFile(force, checkDisk);
    }

    /** {@inheritDoc} */
    synchronized protected void writeToFileMT(RecordRingItem item, boolean force)
            throws EvioException, IOException,
            InterruptedException, ExecutionException {

        super.writeToFileMT(item, force);
    }

    /** {@inheritDoc} */
    synchronized protected void splitFile() throws EvioException {
        super.splitFile();
    }

}
