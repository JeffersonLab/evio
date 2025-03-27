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


import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.BitSet;


/**
 * An EventWriter object is used for writing events to a file or to a byte buffer.
 * This class writes evio in version 4 format only! This is included so that
 * CODA DAQ systems can avoid using the cumbersome evio version 6 format.
 * This class is thread-safe.
 *
 * @author heddle
 * @author timmer
 */
public class EventWriterV4 extends EventWriterUnsyncV4 {

    //---------------------------------------------
    // FILE Constructors (not inherited)
    //---------------------------------------------

    /**
     * Creates an object for writing to a file in native byte order.
     * If the file already exists, its contents will be overwritten.
     * If it doesn't exist, it will be created.
     *
     * @param file the file object to write to.<br>
     * @throws EvioException file cannot be created
     */
    public EventWriterV4(File file) throws EvioException {
        super(file, false);
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
     * @throws EvioException file cannot be created, or
     *                       if appending and file is too small to be evio format;
     */
    public EventWriterV4(File file, boolean append) throws EvioException {
        super(file, DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT,
             ByteOrder.nativeOrder(), null, null, true, append);
    }

    /**
     * Creates an object for writing to a file in native byte order.
     * If the file already exists, its contents will be overwritten unless
     * it is being appended to. If it doesn't exist, it will be created.
     *
     * @param file       the file object to write to.<br>
     * @param dictionary dictionary in xml format or null if none.
     * @param append     if <code>true</code> and the file already exists,
     *                   all events to be written will be appended to the
     *                   end of the file.
     *
     * @throws EvioException file cannot be created, or
     *                       if appending and file is too small to be evio format;
     */
    public EventWriterV4(File file, String dictionary, boolean append) throws EvioException {
        super(file, DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT,
             ByteOrder.nativeOrder(), dictionary, null, true, append);
    }

    /**
     * Creates an object for writing to a file in native byte order.
     * If the file already exists, its contents will be overwritten.
     * If it doesn't exist, it will be created.
     *
     * @param filename name of the file to write to.<br>
     * @throws EvioException file cannot be created
     */
    public EventWriterV4(String filename) throws EvioException {
        super(filename, false);
    }

    /**
     * Creates an object for writing to a file in native byte order.
     * If the file already exists, its contents will be overwritten unless
     * it is being appended to. If it doesn't exist, it will be created.
     *
     * @param filename name of the file to write to.<br>
     * @param append   if <code>true</code> and the file already exists,
     *                 all events to be written will be appended to the
     *                 end of the file.
     *
     * @throws EvioException file cannot be created, or
     *                       if appending and file is too small to be evio format;
     */
    public EventWriterV4(String filename, boolean append) throws EvioException {
        super(new File(filename), DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT,
             ByteOrder.nativeOrder(), null, null, true, append);
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
     * @throws EvioException file cannot be created, or
     *                       if appending and file is too small to be evio format;
     */
    public EventWriterV4(String filename, boolean append, ByteOrder byteOrder) throws EvioException {
        super(new File(filename), DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT,
             byteOrder, null, null, true, append);
    }

    /**
     * Create an object for writing events to a file.
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
    public EventWriterV4(File file, int blockSizeMax, int blockCountMax, ByteOrder byteOrder,
                            String xmlDictionary, BitSet bitInfo)
                                        throws EvioException {

        super(file, blockSizeMax, blockCountMax, byteOrder,
             xmlDictionary, bitInfo, true, false);
    }

    /**
     * Create an object for writing events to a file.
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
    public EventWriterV4(File file, int blockSizeMax, int blockCountMax, ByteOrder byteOrder,
                            String xmlDictionary, BitSet bitInfo, boolean overWriteOK)
                                        throws EvioException {

        super(file, blockSizeMax, blockCountMax, byteOrder,
             xmlDictionary, bitInfo, overWriteOK, false);
    }

    /**
     * Create an object for writing events to a file.
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
     *                       if appending and file is too small to be evio format;
     *                       if file arg is null;
     *                       if file could not be opened or positioned;
     *                       if file exists but user requested no over-writing or appending.
     *
     */
    public EventWriterV4(File file, int blockSizeMax, int blockCountMax, ByteOrder byteOrder,
                            String xmlDictionary, BitSet bitInfo, boolean overWriteOK,
                            boolean append) throws EvioException {

        super(file.getPath(), null, null, 0, 0, blockSizeMax,
             blockCountMax, 0, byteOrder,
             xmlDictionary, bitInfo, overWriteOK, append);
    }


    /**
     * Create an object for writing events to a file.
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
    public EventWriterV4(String baseName, String directory, String runType,
                            int runNumber, long split, ByteOrder byteOrder,
                            String xmlDictionary)
            throws EvioException {

        super(baseName, directory, runType, runNumber, split,
             DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT, 0,
             byteOrder, xmlDictionary, null, false, false);
    }


    /**
     * Create an object for writing events to a file.
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
    public EventWriterV4(String baseName, String directory, String runType,
                            int runNumber, long split, ByteOrder byteOrder,
                            String xmlDictionary, int streamId)
            throws EvioException {

        super(baseName, directory, runType, runNumber, split,
             DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT, 0,
             byteOrder, xmlDictionary, null, false, false, null, streamId);
    }


    /**
     * Create an object for writing events to a file.
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
    public EventWriterV4(String baseName, String directory, String runType,
                            int runNumber, long split, ByteOrder byteOrder,
                            String xmlDictionary, boolean overWriteOK)
            throws EvioException {

        super(baseName, directory, runType, runNumber, split,
                DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT, 0,
                byteOrder, xmlDictionary, null, overWriteOK, false);
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
     * the split file names. In this constructor, splitNumber and streamId = 0.</p>
     *
     * <p>In addition, the baseName may contain characters of the form
     * <b>$(ENV_VAR)</b> which will be substituted with the value of the associated environmental
     * variable or a blank string if none is found.</p>
     *
     * <p>Finally, the baseName may contain occurrences of the string "%s"
     * which will be substituted with the value of the runType arg or nothing if
     * the runType is null.</p>
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
     *                       if appending and file is too small to be evio format;
     *                       if splitting file while appending;
     *                       if file name arg is null;
     *                       if file could not be opened, positioned, or written to;
     *                       if file exists but user requested no over-writing or appending.
     */
    public EventWriterV4(String baseName, String directory, String runType,
                            int runNumber, long split,
                            int blockSizeMax, int blockCountMax, int bufferSize,
                            ByteOrder byteOrder, String xmlDictionary,
                            BitSet bitInfo, boolean overWriteOK, boolean append)
            throws EvioException {

        super(baseName, directory, runType, runNumber, split,
             blockSizeMax, blockCountMax, bufferSize,
             byteOrder, xmlDictionary, bitInfo, overWriteOK, append, null);
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
     * the split file names. In this constructor, splitNumber and streamId = 0.</p>
     *
     * <p>In addition, the baseName may contain characters of the form
     * <b>$(ENV_VAR)</b> which will be substituted with the value of the associated environmental
     * variable or a blank string if none is found.</p>
     *
     * <p>Finally, the baseName may contain occurrences of the string "%s"
     * which will be substituted with the value of the runType arg or nothing if
     * the runType is null.</p>
     *
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
     *                       if appending and file is too small to be evio format;
     *                       if splitting file while appending;
     *                       if file name arg is null;
     *                       if file could not be opened, positioned, or written to;
     *                       if file exists but user requested no over-writing or appending.
     */
    public EventWriterV4(String baseName, String directory, String runType,
                            int runNumber, long split,
                            int blockSizeMax, int blockCountMax, int bufferSize,
                            ByteOrder byteOrder, String xmlDictionary,
                            BitSet bitInfo, boolean overWriteOK, boolean append,
                            EvioBank firstEvent)
            throws EvioException {

        super(baseName, directory, runType, runNumber, split,
             blockSizeMax, blockCountMax, bufferSize,
             byteOrder, xmlDictionary, bitInfo, overWriteOK, append, firstEvent, 0, 0, 1, 1);
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
     * the split file names. In this constructor, splitNumber = 0.</p>
     *
     * <p>In addition, the baseName may contain characters of the form
     * <b>$(ENV_VAR)</b> which will be substituted with the value of the associated environmental
     * variable or a blank string if none is found.</p>
     *
     * <p>Finally, the baseName may contain occurrences of the string "%s"
     * which will be substituted with the value of the runType arg or nothing if
     * the runType is null.</p>
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
     * @param streamId      streamId number (100 &gt; id &gt; -1) for file name
     *
     * @throws EvioException if blockSizeMax or blockCountMax exceed limits;
     *                       if defined dictionary or first event while appending;
     *                       if appending and file is too small to be evio format;
     *                       if splitting file while appending;
     *                       if file name arg is null;
     *                       if file could not be opened, positioned, or written to;
     *                       if file exists but user requested no over-writing or appending.
     */
    public EventWriterV4(String baseName, String directory, String runType,
                            int runNumber, long split,
                            int blockSizeMax, int blockCountMax, int bufferSize,
                            ByteOrder byteOrder, String xmlDictionary,
                            BitSet bitInfo, boolean overWriteOK, boolean append,
                            EvioBank firstEvent, int streamId)
            throws EvioException {

        super(baseName, directory, runType, runNumber, split,
             blockSizeMax, blockCountMax, bufferSize,
             byteOrder, xmlDictionary, bitInfo, overWriteOK,
             append, firstEvent, streamId, 0, 1, 1);
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
     * @param streamId      streamId number (100 &gt; id &gt; -1) for file name
     * @param splitNumber   number at which to start the split numbers
     * @param splitIncrement amount to increment split number each time another
     *                       file is created.
     * @param streamCount    total number of streams in DAQ.
     *
     * @throws EvioException if blockSizeMax or blockCountMax exceed limits;
     *                       if defined dictionary or first event while appending;
     *                       if appending and file is too small to be evio format;
     *                       if splitting file while appending;
     *                       if file name arg is null;
     *                       if file could not be opened, positioned, or written to;
     *                       if file exists but user requested no over-writing or appending;
     *                       if streamId &lt; 0, splitNumber &lt; 0, or splitIncrement &lt; 1.
     */
    public EventWriterV4(String baseName, String directory, String runType,
                            int runNumber, long split,
                            int blockSizeMax, int blockCountMax, int bufferSize,
                            ByteOrder byteOrder, String xmlDictionary,
                            BitSet bitInfo, boolean overWriteOK, boolean append,
                            EvioBank firstEvent, int streamId, int splitNumber,
                            int splitIncrement, int streamCount)
            throws EvioException {

        super(baseName, directory, runType,
                runNumber, split,
                blockSizeMax, blockCountMax, bufferSize,
                byteOrder, xmlDictionary,
                bitInfo, overWriteOK, append,
                firstEvent, streamId, splitNumber,
                splitIncrement,  streamCount);
    }


    //---------------------------------------------
    // BUFFER Constructors
    //---------------------------------------------


    /**
     * Create an object for writing events to a ByteBuffer.
     * Uses the default number and size of blocks in buffer.
     * Will overwrite any existing data in buffer!
     *
     * @param buf            the buffer to write to.
     * @throws EvioException if buf arg is null
     */
    public EventWriterV4(ByteBuffer buf) throws EvioException {

        super(buf, DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT, null, null, 0, false);
    }

    /**
     * Create an object for writing events to a ByteBuffer.
     * Uses the default number and size of blocks in buffer.
     *
     * @param buf            the buffer to write to.
     * @param append         if <code>true</code>, all events to be written will be
     *                       appended to the end of the buffer.
     * @throws EvioException if buf arg is null
     */
    public EventWriterV4(ByteBuffer buf, boolean append) throws EvioException {

        super(buf, DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT, null, null, 0, append);
    }

    /**
     * Create an object for writing events to a ByteBuffer.
     * Uses the default number and size of blocks in buffer.
     *
     * @param buf            the buffer to write to.
     * @param xmlDictionary  dictionary in xml format or null if none.
     * @param append         if <code>true</code>, all events to be written will be
     *                       appended to the end of the buffer.
     * @throws EvioException if buf arg is null
     */
    public EventWriterV4(ByteBuffer buf, String xmlDictionary, boolean append) throws EvioException {

        super(buf, DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT, xmlDictionary, null, 0, append);
    }

    /**
     * Create an object for writing events to a ByteBuffer.
     * Will overwrite any existing data in buffer!
     *
     * @param buf            the buffer to write to.
     * @param blockSizeMax   the max blocksize to use which must be &gt;= {@link #MIN_BLOCK_SIZE}
     *                       and &lt;= {@link #MAX_BLOCK_SIZE} ints.
     *                       The size of the block will not be larger than this size
     *                       unless a single event itself is larger.
     * @param blockCountMax the max number of events (including dictionary) in a single block
     *                      which must be &gt;= {@link #MIN_BLOCK_COUNT} and &lt;= {@link #MAX_BLOCK_COUNT}.
     * @param xmlDictionary  dictionary in xml format or null if none.
     * @param bitInfo        set of bits to include in first block header.
     * @throws EvioException if blockSizeMax or blockCountMax exceed limits; if buf arg is null
     */
    public EventWriterV4(ByteBuffer buf, int blockSizeMax, int blockCountMax,
                            String xmlDictionary, BitSet bitInfo) throws EvioException {

        super(buf, blockSizeMax, blockCountMax, xmlDictionary, bitInfo, 0, false);
    }

    /**
     * Create an object for writing events to a ByteBuffer.
     * Block number starts at 0.
     *
     * @param buf            the buffer to write to.
     * @param blockSizeMax   the max blocksize to use which must be &gt;= {@link #MIN_BLOCK_SIZE}
     *                       and &lt;= {@link #MAX_BLOCK_SIZE} ints.
     *                       The size of the block will not be larger than this size
     *                       unless a single event itself is larger.
     * @param blockCountMax the max number of events (including dictionary) in a single block
     *                      which must be &gt;= {@link #MIN_BLOCK_COUNT} and &lt;= {@link #MAX_BLOCK_COUNT}.
     * @param xmlDictionary  dictionary in xml format or null if none.
     * @param bitInfo        set of bits to include in first block header.
     * @param append         if <code>true</code>, all events to be written will be
     *                       appended to the end of the buffer.
     *
     * @throws EvioException if blockSizeMax or blockCountMax exceed limits;
     *                       if buf arg is null;
     *                       if defined dictionary while appending;
     */
    public EventWriterV4(ByteBuffer buf, int blockSizeMax, int blockCountMax,
                            String xmlDictionary, BitSet bitInfo,
                            boolean append) throws EvioException {

        super(buf, blockSizeMax, blockCountMax, xmlDictionary, bitInfo, 0, append);
    }

    /**
     * Create an object for writing events to a ByteBuffer.
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
    public EventWriterV4(ByteBuffer buf, int blockSizeMax, int blockCountMax,
                            String xmlDictionary, BitSet bitInfo, int reserved1,
                            int blockNumber) throws EvioException {

        super(buf, blockSizeMax, blockCountMax,
                xmlDictionary, bitInfo, reserved1,
                blockNumber);
    }

    /**
     * Create an object for writing events to a ByteBuffer.
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
    public EventWriterV4(ByteBuffer buf, int blockSizeMax, int blockCountMax,
                            String xmlDictionary, BitSet bitInfo, int reserved1,
                            boolean append) throws EvioException {

        super(buf, blockSizeMax, blockCountMax,
                xmlDictionary, bitInfo, reserved1,
                append);
    }

    /**
     * Create an object for writing events to a ByteBuffer.
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
    public EventWriterV4(ByteBuffer buf, int blockSizeMax, int blockCountMax,
                            String xmlDictionary, BitSet bitInfo, int reserved1,
                            int blockNumber, boolean append, EvioBank firstEvent)
            throws EvioException {

        super(buf, blockSizeMax, blockCountMax,
                xmlDictionary, bitInfo, reserved1,
                blockNumber, append, firstEvent);
    }


    //--------------------------------------------------------------
    // The following methods overwrite the equivalent methods in
    // EventWriterUnsyncV4. They are mutex protected for thread
    // safety.
    //--------------------------------------------------------------


    /** {@inheritDoc} */
    synchronized public ByteBuffer getByteBuffer() {
        return super.getByteBuffer();
    }


    /** {@inheritDoc} */
    synchronized public boolean isClosed() {return super.isClosed();}


    /** {@inheritDoc} */
    synchronized public void setFirstEvent(EvioNode node)
            throws EvioException, IOException {

        super.setFirstEvent(node);
    }


    /** {@inheritDoc} */
    synchronized public void setFirstEvent(ByteBuffer buffer)
            throws EvioException, IOException {

        super.setFirstEvent(buffer);
     }


    /** {@inheritDoc} */
    synchronized public void setFirstEvent(EvioBank bank)
            throws EvioException, IOException {

        super.setFirstEvent(bank);
    }


    /** {@inheritDoc} */
    synchronized public void flush() {
        super.flush();
    }


    /** {@inheritDoc} */
    synchronized public void close() {
        super.close();
    }


    /** {@inheritDoc} */
    synchronized public boolean writeEvent(EvioNode node, boolean force, boolean duplicate)
            throws EvioException, IOException {

        return super.writeEvent(node, force, duplicate);
    }


    /** {@inheritDoc} */
    synchronized protected boolean writeEvent(EvioBank bank, ByteBuffer bankBuffer,
                                            boolean force)
            throws EvioException, IOException {

        return super.writeEvent(bank, bankBuffer, force);
    }


    /** {@inheritDoc} */
    public boolean writeEventToFile(EvioNode node, boolean force, boolean duplicate)
            throws EvioException, IOException {

        return super.writeEventToFile(node, force, duplicate);
    }


    /** {@inheritDoc} */
    synchronized public boolean writeEventToFile(EvioBank bank, ByteBuffer bankBuffer, boolean force)
            throws EvioException, IOException {

        return super.writeEventToFile(bank, bankBuffer, force);
    }

}
