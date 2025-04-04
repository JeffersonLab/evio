//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "EventWriter.h"
#ifdef USE_FILESYSTEMLIB
    #include <filesystem>
    namespace fs = std::filesystem;
#endif

namespace evio {


    //---------------------------------------------
    // FILE Constructors
    //---------------------------------------------

    /**
     * Creates an <code>EventWriter</code> for writing to a file in the
     * specified byte order.
     * If the file already exists, its contents will be overwritten unless
     * it is being appended to. If it doesn't exist, it will be created.
     *
     * @param filename  name of the file to write to.<br>
     * @param byteOrder the byte order in which to write the file.
     * @param append    if <code>true</code> and the file already exists,
     *                  all events to be written will be appended to the
     *                  end of the file.
     *
     * @throws EvioException file cannot be created
     */
    EventWriter::EventWriter(std::string & filename, const ByteOrder & byteOrder, bool append) :
            EventWriter(filename, "", "",
                        0, 0, 0, 0,
                        byteOrder, "", true, append,
                        nullptr, 0, 0, 1, 1,
                        Compressor::CompressionType::UNCOMPRESSED,
                        1, 8, 0) {
    }



    /**
     * Creates an <code>EventWriter</code> for writing to a file in NATIVE byte order.
     * If the file already exists, its contents will be overwritten unless
     * it is being appended to. If it doesn't exist, it will be created.
     *
     * @param filename   the file object to write to.<br>
     * @param dictionary dictionary in xml format or null if none.
     * @param byteOrder  the byte order in which to write the file.
     * @param append     if <code>true</code> and the file already exists,
     *                   all events to be written will be appended to the
     *                   end of the file.
     *
     * @throws EvioException file cannot be created
     */
    EventWriter::EventWriter(std::string & filename,
                             std::string & dictionary,
                             const ByteOrder & byteOrder,
                             bool append) :
            EventWriter(filename, "", "",
                        0, 0, 0, 0,
                        byteOrder, dictionary, true, append,
                        nullptr, 0, 0, 1, 1,
                        Compressor::CompressionType::UNCOMPRESSED,
                        1, 8, 0) {

    }


    /**
     * <p>Create an <code>EventWriter</code> for writing events to a file or files.
     * If the given file already exists, its contents will be overwritten
     * unless the "overWriteOK" argument is <code>false</code> in
     * which case an exception will be thrown. If the option to
     * "append" these events to an existing file is <code>true</code>,
     * then the write proceeds. If the file doesn't exist,
     * it will be created.</p>
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
     * See the documentation at {@link Util::generateFileName} to understand exactly how substitutions
     * of runNumber, streamId and splitNumber are done for these specifiers in order to create
     * the split file names.</p>
     *
     * <p>In addition, the baseName may contain characters of the form
     * <b>\$(ENV_VAR)</b> which will be substituted with the value of the associated environmental
     * variable or a blank string if none is found.</p>
     *
     * <p>Finally, the baseName may contain occurrences of the string "%s"
     * which will be substituted with the value of the runType arg or nothing if
     * the runType is null.</p>
     *
     *
     * @param baseName      base file name used to generate complete file name(s) (may not be empty)
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
     *                      if appending to existing file.
     * @param xmlDictionary dictionary in xml format or empty if none.
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
     * @param compressionType    type of data compression to do.
     * @param compressionThreads number of threads doing compression simultaneously.
     * @param ringSize           number of records in supply ring. If set to &lt; compressionThreads,
     *                           it is forced to equal that value and is also forced to be a multiple of
     *                           2, rounded up.
     * @param bufferSize    number of bytes to make each internal buffer which will
     *                      be storing events before writing them to a file.
     *                      9MB = default if bufferSize = 0.
     *
     * @throws EvioException if maxRecordSize or maxEventCount exceed limits;
     *                       if defined dictionary or first event while appending;
     *                       if splitting file while appending;
     *                       if file name arg is empty;
     *                       if file could not be opened, positioned, or written to;
     *                       if file exists but user requested no over-writing or appending.
     */
    EventWriter::EventWriter(std::string & baseName, const std::string & directory, const std::string & runType,
                             uint32_t runNumber, uint64_t split,
                             uint32_t maxRecordSize, uint32_t maxEventCount,
                             const ByteOrder & byteOrder, const std::string & xmlDictionary,
                             bool overWriteOK, bool append,
                             std::shared_ptr<EvioBank> firstEvent, uint32_t streamId,
                             uint32_t splitNumber, uint32_t splitIncrement, uint32_t streamCount,
                             Compressor::CompressionType compressionType, uint32_t compressionThreads,
                             uint32_t ringSize, size_t bufferSize) {

        if (baseName.empty()) {
            throw EvioException("baseName arg is empty");
        }

        if (splitIncrement < 1) {
            splitIncrement = 1;
        }

        if (runNumber < 1) {
            runNumber = 1;
        }

        if (append) {
            if (split > 0) {
                throw EvioException("Cannot specify split when appending");
            }
            else if ((!xmlDictionary.empty()) || (firstEvent != nullptr && firstEvent->getHeader()->getLength() > 0)) {
                throw EvioException("Cannot specify dictionary or first event when appending");
            }
        }

        // Store arguments
        this->split          = split;
        this->append         = append;
        this->runNumber      = runNumber;
        this->byteOrder      = byteOrder;   // byte order may be overwritten if appending
        this->overWriteOK    = overWriteOK;
        this->xmlDictionary  = xmlDictionary;
        this->streamId       = streamId;
        this->splitNumber    = splitNumber;
        this->splitIncrement = splitIncrement;
        this->streamCount    = streamCount;

        // Only add trailer index if writing file
        addTrailerIndex = true;

        // compressionType = 0 before creating commonRecord, so NO compression in common record.
        // But be sure byteOrder is set so commonRecord has the correct byteOrder.

        if (!xmlDictionary.empty() || firstEvent != nullptr) {
            // Create the common record here, but don't write it to file
            // until the file header is written to the file in writeFileHeader()
            // which in turn is written by writeToFile() which is only called
            // right after a file is created.
            createCommonRecord(xmlDictionary, firstEvent, nullptr, nullptr);
        }

        this->compressionType = compressionType;

        // How much compression will data experience? Percentage of original size.
        switch (compressionType) {
            case Compressor::LZ4:
                compressionFactor = 58;
                break;
            case Compressor::LZ4_BEST:
                compressionFactor = 47;
                break;
            case Compressor::GZIP:
                compressionFactor = 42;
                break;

            case Compressor::UNCOMPRESSED:
            default:
                compressionFactor = 100;
        }

        if (compressionThreads < 1) {
            compressionThreads = 1;
        }

        toFile = true;
        recordNumber = 1;
        //cout << "EventWriter constr: split = "  << split << endl;
        //cout << "EventWriter constr: record # set to 1" << endl;

        // The following may not be backwards compatible.
        // Make substitutions in the baseName to create the base file name.
        if (!directory.empty()) baseName = directory + "/" + baseName;
        specifierCount = Util::generateBaseFileName(baseName, runType, baseFileName);
        // Also create the first file's name with more substitutions
        std::string fileName = Util::generateFileName(baseFileName, specifierCount,
                                                 runNumber, split, splitNumber,
                                                 streamId, streamCount);
        // All subsequent split numbers are calculated by adding the splitIncrement
        this->splitNumber += splitIncrement;

#ifdef USE_FILESYSTEMLIB
        currentFilePath = fs::path(fileName);
        currentFileName = currentFilePath.generic_string();
        bool fileExists = fs::exists(currentFilePath);
        bool isRegularFile = fs::is_regular_file(currentFilePath);
#else
        currentFileName = fileName;
        struct stat stt{};
        bool fileExists = stat( fileName.c_str(), &stt ) == 0;
        bool isRegularFile = S_ISREG(stt.st_mode);
#endif

        if (!overWriteOK && !append && (fileExists && isRegularFile)) {
            throw EvioException("File exists but user requested no over-writing of or no appending to "
                                + fileName);
        }

        // Create internal storage buffers.
        // The reason there are 2 internal buffers is that we'll be able to
        // do 1 asynchronous write while still filling up the second
        // simultaneously.

        // Allow the user to set the size of the internal buffers up to a point.
        // Value of 0 means use default of 9MB. This value is consistent with
        // RecordOutput's own default. Won't use any size < 1MB.
        // One downside of the following constructor of currentRecord (supplying
        // an external buffer), is that trying to write a single event of over
        // bufferSize will fail. C. Timmer
        if (bufferSize < 1) {
            internalBufSize = 9437184;
        }
        else {
            internalBufSize = bufferSize < 1000000 ? 1000000 : bufferSize;
        }

        internalBuffers.reserve(2);
        internalBuffers.push_back(std::make_shared<ByteBuffer>(internalBufSize));
        internalBuffers.push_back(std::make_shared<ByteBuffer>(internalBufSize));
        internalBuffers[0]->order(byteOrder);
        internalBuffers[1]->order(byteOrder);
        buffer = internalBuffers[0];

        headerArray.resize(RecordHeader::HEADER_SIZE_BYTES);

        // Evio file
        fileHeader = FileHeader(true);
        recordLengths = std::make_shared<std::vector<uint32_t>>();

        asyncFileChannel = std::make_shared<std::fstream>();

        if (append) {
            asyncFileChannel->open(currentFileName, std::ios::binary | std::ios::in | std::ios::out);
            if (asyncFileChannel->fail()) {
                throw EvioException("error opening file " + currentFileName);
            }

            // Right now file is open for writing
            fileOpen = true;

            // If we have an empty file, that's OK.
            // Otherwise we have to examine it for compatibility
            // and position ourselves for the first write.
#ifdef USE_FILESYSTEMLIB
            if (fs::file_size(currentFilePath) > 0) {
#else
            if (stt.st_size > 0) {
#endif

                // Look at first record header to find endianness & version.
                // Endianness given in constructor arg, when appending, is ignored.
                // this->byteOrder set in next call.
                examineFileHeader();

                // Oops, gotta redo this since file has different byte order
                // than specified in constructor arg.
                if (this->byteOrder != byteOrder) {
                    // From now on, this->byteOrder must be used, not local byteOrder!
                    internalBuffers[0]->order(this->byteOrder);
                    internalBuffers[1]->order(this->byteOrder);
                }

                // Prepare for appending by moving file position to end of last record w/ data.
                // Needs buffer to be defined and set to proper endian (which is done just above).
                toAppendPosition();

                // File position is now after the last event written.
            }
        }

        // Compression threads
        if (compressionThreads == 1) {
            // When writing single threaded, just fill/compress/write one record at a time
            singleThreadedCompression = true;
            currentRecord = std::make_shared<RecordOutput>(buffer, maxEventCount,
                                                           compressionType,
                                                           HeaderType::EVIO_RECORD);
        }
        else {
            // Number of ring items must be >= # of compressionThreads, plus 1 which
            // is being written, plus 1 being filled - all simultaneously.
            if (ringSize < 16) {
                ringSize = 16;
            }

            if (ringSize < compressionThreads + 2) {
                ringSize = compressionThreads + 2;
            }

            // AND must be power of 2, round up
            ringSize = Util::powerOfTwo(ringSize, true);
            //cout << "EventWriter constr: record ring size set to " << ringSize << endl;

            supply = std::make_shared<RecordSupply>(ringSize, this->byteOrder,
                                                    compressionThreads,
                                                    maxEventCount, maxRecordSize,
                                                    compressionType);

            // Do a quick calculation as to how much data a ring full
            // of records can hold since we may have to write that to
            // disk before we can shut off the spigot when disk is full.
            maxSupplyBytes = supply->getMaxRingBytes();

            // Number of available bytes in file's disk partition

#ifdef USE_FILESYSTEMLIB
            fs::space_info spaceInfo = fs::space(currentFilePath.parent_path());
            uint64_t freeBytes = spaceInfo.available;
            //cout << "EventWriter constr: " << freeBytes << " bytes available in dir = " <<
            //         currentFilePath.parent_path().generic_string() << endl;
#else
            uint64_t freeBytes;
            struct statvfs sttvfs{};

            // Use Boost.Filesystem to extract the directory (file may not exist yet)
            boost::filesystem::path path(fileName);
            boost::filesystem::path directory = path.parent_path();
            std::string dirStr = directory.string();

            if (statvfs(dirStr.c_str(), &sttvfs) == 0) {
                freeBytes = sttvfs.f_bavail * sttvfs.f_frsize;
                //std::cout << "EventWriter constr: freeBytes for (" << dirStr << ") = " << freeBytes << std::endl;
            }
            else {
                perror(dirStr.c_str());
                // Trouble getting info about disk partition file is sitting on.
                // Just assume everything is OK (1TB available).
                freeBytes = 1000000000000UL;
            }
#endif
            // If there isn't enough to accommodate 1 split of the file + full supply + 10MB extra,
            // then don't even start writing ...
            if (freeBytes < split + maxSupplyBytes + 10000000) {
                //cout << "EventWriter constr: Disk is FULL" << endl;
                diskIsFull = true;
                diskIsFullVolatile = true;
            }

            // Create compression threads
            recordCompressorThreads.reserve(compressionThreads);
            for (uint32_t i = 0; i < compressionThreads; i++) {
                recordCompressorThreads.emplace_back(i, compressionType, supply);
            }
            //cout << "EventWriter constr: created " << compressionThreads << " number of comp thds" << endl;

            // Start compression threads
            for (uint32_t i=0; i < compressionThreads; i++) {
                recordCompressorThreads[i].startThread();
            }

            // Create and start writing thread
            recordWriterThread.emplace_back(this, supply);
            recordWriterThread[0].startThread();

            // Get a single blank record to start writing into
            currentRingItem = supply->get();
            //cout << "EventWriter constr: get seq " << currentRingItem->getSequence() << endl;
            currentRecord   = currentRingItem->getRecord();

            // When obtained from supply, record has record number = 1.
            // This is fine in single threaded compression which sets runNumber
            // just before being written, in (try)compressAndWriteToFile.
            // But needs setting if multiple threads:
            currentRecord->getHeader()->setRecordNumber(recordNumber++);
        }

        // Object to close files in a separate thread when splitting, to speed things up
        if (split > 0) {
            fileCloser = std::make_shared<FileCloser>();
        }
    }


    //---------------------------------------------
    // BUFFER Constructors
    //---------------------------------------------


    /**
     * Create an <code>EventWriter</code> for writing events to a ByteBuffer.
     * Uses the default number and size of records in buffer.
     *
     * @param buf            the buffer to write to.
     * @param xmlDictionary  dictionary in xml format or null if none.
     * @throws EvioException if buf arg is null
     */
    EventWriter::EventWriter(std::shared_ptr<ByteBuffer> & buf, const std::string & xmlDictionary) :
                EventWriter(buf, 0, 0, xmlDictionary, 1,
                                Compressor::CompressionType::UNCOMPRESSED) {
    }


     /**
      * Create an <code>EventWriter</code> for writing events to a ByteBuffer.
      * The buffer's position is set to 0 before writing.
      * Any dictionary will be put in a commonRecord and that record will be
      * placed in the user header associated with the single record.
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
      * @param compressionType type of data compression to do.
      * @param eventType       first record header holds the following type of event encoded in bitInfo,
      *                        (0=RocRaw, 1=Physics, 2=Partial Physics, 3=Disentangled,
      *                         4=User, 5=Control, 6=Mixed, 8=RocRawStream, 9=PhysicsStream, 15=Other).
      *                        If &lt; 0 or &gt; 15, this arg is ignored.
      *
      * @throws EvioException if maxRecordSize or maxEventCount exceed limits;
      */
    EventWriter::EventWriter(std::shared_ptr<ByteBuffer> & buf,
                             uint32_t maxRecordSize, uint32_t maxEventCount,
                             const std::string & xmlDictionary, uint32_t recordNumber,
                             Compressor::CompressionType compressionType, int eventType) {

        this->toFile          = false;
        this->append          = false;
        this->buffer          = buf;
        this->byteOrder       = buf->order();
        this->recordNumber    = recordNumber;

        this->xmlDictionary   = xmlDictionary;
        this->compressionType = compressionType;

        // How much compression will data experience? Percentage of original size.
        switch (compressionType) {
            case Compressor::LZ4:
                compressionFactor = 58;
                break;
            case Compressor::LZ4_BEST:
                compressionFactor = 47;
                break;
            case Compressor::GZIP:
                compressionFactor = 42;
                break;

            case Compressor::UNCOMPRESSED:
            default:
                compressionFactor = 100;
        }

        // Get buffer ready for writing
        buffer->clear();
        bufferSize = buf->capacity();
        headerArray.resize(RecordHeader::HEADER_SIZE_BYTES);
        recordLengths = std::make_shared<std::vector<uint32_t>>();

        // Write any record containing dictionary and first event, first
        if (!xmlDictionary.empty()) {
            createCommonRecord(xmlDictionary, nullptr, nullptr, nullptr);
        }

        // When writing to buffer, just fill/compress/write one record at a time
        currentRecord = std::make_shared<RecordOutput>(buf, maxEventCount,
                                                       compressionType,
                                                       HeaderType::EVIO_RECORD);

        auto header = currentRecord->getHeader();
        header->setBitInfo(false, !xmlDictionary.empty());
        if (eventType >=0 && eventType <= 15) {
            header->setBitInfoEventType(eventType);
        }
    }


    /**
     * Initialization new buffer (not from constructor).
     * The buffer's position is set to 0 before writing.
     * Only called by {@link #setBuffer(std::shared_ptr<ByteBuffer> &)} and
     * {@link #setBuffer(std::shared_ptr<ByteBuffer> &, std::bitset<24> *, uint32_t)}.
     *
     * @param buf            the buffer to write to.
     * @param bitInfo        set of bits to include in first record header.
     * @param recNumber      number at which to start record number counting.
     * @param useCurrentBitInfo   regardless of bitInfo arg's value, use the
     *                            current value of bitInfo in the reinitialized buffer.
     */
    void EventWriter::reInitializeBuffer(std::shared_ptr<ByteBuffer> & buf,
                                         const std::bitset<24> *bitInfo,
                                         uint32_t recNumber, bool useCurrentBitInfo) {

        this->buffer       = buf;
        this->byteOrder    = buf->order();
        this->recordNumber = recNumber;

        // Init variables
        split  = 0L;
        toFile = false;
        closed = false;
        eventsWrittenTotal = 0;
        eventsWrittenToBuffer = 0;
        bytesWritten = 0L;
        buffer->clear();
        bufferSize = buffer->capacity();

        // Deal with bitInfo
        auto header = currentRecord->getHeader();

        // This will reset the record - header and all buffers (including buf)
        currentRecord->setBuffer(buffer);

        if (!useCurrentBitInfo) {
            header->setBitInfoWord(*bitInfo);
        }
        //cout << "reInitializeBuffer: after reset, record # -> " << recNumber << endl;

        // Only necessary to do this when using EventWriter in EMU's
        // RocSimulation module. Only the ROC sends sourceId in header.
        header->setUserRegisterFirst(sourceId);
    }


    /**
     * Static wrapper function used to asynchronously run threads to write to file.
     * Necesssary because std::async cannot run member functions (i.e. asyncFileChannel->write)
     * directly.
     *
     * @param pWriter pointer to this object.
     * @param data    pointer to data.
     * @param len     number of bytes to write.
     */
    void EventWriter::staticWriteFunction(EventWriter *pWriter, const char* data, size_t len) {
        pWriter->asyncFileChannel->write(data, len);
    }


    /**
     * Static wrapper function used to asynchronously run threads to do nothing.
     * Used when testing this software but not actually writing to file.
     *
     * @param pWriter pointer to this object.
     */
    void EventWriter::staticDoNothingFunction(EventWriter *pWriter) {}


    /**
     * If writing file, is the partition it resides on full?
     * Not full, in this context, means there's enough space to write
     * a full split file + a full record + an extra 10MB as a safety factor.
     *
     * @return true if the partition the file resides on is full, else false.
     */
    bool EventWriter::isDiskFull() {
        if (!toFile) return false;
        return diskIsFull;
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
     * @param recNumber      number at which to start record number counting.
     * @throws EvioException if this object was not closed prior to resetting the buffer,
     *                       or writing to file.
     */
    void EventWriter::setBuffer(std::shared_ptr<ByteBuffer> & buf, std::bitset<24> *bitInfo, uint32_t recNumber) {
        if (toFile) return;
        if (!closed) {
            throw EvioException("Close EventWriter before changing buffers");
        }

        //cout << "setBuffer: call reInitializeBuf with INPUT record # " << recNumber << endl;
        reInitializeBuffer(buf, bitInfo, recNumber, false);
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
     *                       or writing to file.
     */
    void EventWriter::setBuffer(std::shared_ptr<ByteBuffer> & buf) {
        if (toFile) return;
        if (!closed) {
            throw EvioException("Close EventWriter before changing buffers");
        }

        //std::cout << "setBuffer: call reInitializeBuf with local record # " << recordNumber << std::endl;
        reInitializeBuffer(buf, nullptr, recordNumber, true);
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
    std::shared_ptr<ByteBuffer> EventWriter::getBuffer() {
        if (toFile) return nullptr;
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
    std::shared_ptr<ByteBuffer> EventWriter::getByteBuffer() {
        // It does NOT make sense to give the caller the internal buffer
        // used in writing to files. That buffer may contain nothing and
        // most probably won't contain the full file contents.
        if (toFile) return nullptr;

        auto buf = buffer->duplicate();
        buf->order(buffer->order());
        buf->limit(bytesWritten);

        return buf;
    }


    /**
     * <p>
     * Increase internal buffer size when writing to a file with a single
     * compression thread. Design to accommodate a single event which
     * exceeds the current memory limit. This happens automatically for
     * multiple compression threads, but with only one such thread,
     * code get excercised in the constructor in which
     * this object creates buffers "by hand" and passes one to the constructor
     * for RecordOutputStream. Since it's not created by the constructor, it
     * cannot be automatically increased in size for a large event. Thus, we do
     * it here instead.</p>
     *
     * This method can only be called if all events have been written to file
     * and so record is empty.
     *
     * @param bytes size in bytes of new internal buffers.
     */
    void EventWriter::expandInternalBuffers(size_t bytes) {

        if ((bytes <= internalBufSize) || !toFile || !singleThreadedCompression) {
            return;
        }

        // Give it 20% more than asked for
        internalBufSize = bytes / 10 * 12;

        internalBuffers.clear();

        internalBuffers.push_back(std::make_shared<ByteBuffer>(internalBufSize));
        internalBuffers.push_back(std::make_shared<ByteBuffer>(internalBufSize));
        internalBuffers[0]->order(byteOrder);
        internalBuffers[1]->order(byteOrder);
        buffer = internalBuffers[0];

        try {
            // There may be a simultaneous write in progress,
            // wait for it to finish.
            if (future1 != nullptr && future1->valid()) {
                // Wait for last write to end before we continue
                future1->get();
            }
        }
        catch (std::exception & e) {}

        // Start over
        future1 = nullptr;
        usedBuffer = nullptr;

        currentRecord->setBuffer(buffer);

//        std::cout << "expandInternalBuffers: buffer cap = " << buffer->capacity() <<
//                     ", lim = " << buffer->limit() << ", pos = " << buffer->position() << std::endl;
    }


    /**
     * Set the value of the source Id in the first block header.
     * Only necessary to do this when using EventWriter in EMU's
     * RocSimulation module. Only the ROC sends sourceId in header.
     * In evio 6, the source id is stored in user register 1. In
     * earlier versions it's stored in reserved1.
     * This should only be used internally by CODA in emu software.
     *
     * @param sId  value of the source Id.
     */
    void EventWriter::setSourceId(int sId) {
        sourceId = sId;
        auto header = currentRecord->getHeader();
        header->setUserRegisterFirst(sId);
    }


    /**
     * Set the bit info of a record header for a specified CODA event type.
     * Must be called AFTER {@link RecordHeader#setBitInfo(bool, bool)} or
     * {@link RecordHeader#setBitInfoWord(uint32_t)} in order to have change preserved.
     * This should only be used internally by CODA in emu software.
     *
     * @param type event type (0=ROC raw, 1=Physics, 2=Partial Physics,
     *             3=Disentangled, 4=User, 5=Control, 15=Other,
     *             else = nothing set).
     */
    void EventWriter::setEventType(int type) {
        auto header = currentRecord->getHeader();
        header->setBitInfoEventType(type);
    }


    /**
     * Is this object writing to file?
     * @return {@code true} if writing to file, else {@code false}.
     */
    bool EventWriter::writingToFile() const {return toFile;}


    /**
     * Has {@link #close()} been called (without reopening by calling
     * {@link #setBuffer()}) ?
     *
     * @return {@code true} if this object closed, else {@code false}.
     */
    bool EventWriter::isClosed() const {return closed;}


    /**
     * Get the name of the current file being written to.
     * Returns empty string if no file.
     * @return the name of the current file being written to.
     */
    std::string EventWriter::getCurrentFilename() const {return currentFileName;}


    /**
     * If writing to a buffer, get the number of bytes written to it
     * including the trailer.
     * @return number of bytes written to buffer
     */
    size_t EventWriter::getBytesWrittenToBuffer() const {return bytesWritten;}


    /**
     * Get the full or absolute name or path of the current file being written to.
     * Returns empty string if no file.
     * @return the full name or path of the current file being written to.
     */
    std::string EventWriter::getCurrentFilePath() const {
#ifdef USE_FILESYSTEMLIB
        fs::path absolutePath = fs::absolute(currentFilePath);
        return absolutePath.generic_string();
#else
        char *actualPath = realpath(currentFileName.c_str(), nullptr);
        if (actualPath != nullptr) {
            auto strPath = std::string(actualPath);
            free(actualPath);
            return strPath;
        }
        return currentFileName;
#endif
    }


    /**
     * Get the current split number which is the split number of file
     * to be written next. Warning, this value may be changing.
     * @return the current split number which is the split number of file to be written next.
     */
    uint32_t EventWriter::getSplitNumber() const {return splitNumber;}


    /**
     * Get the number of split files produced by this writer.
     * @return number of split files produced by this writer.
     */
    uint32_t EventWriter::getSplitCount() const {return splitCount;}


    /**
     * Get the current record number.
     * Warning, this value may be changing.
     * @return the current record number.
     */
    uint32_t EventWriter::getRecordNumber() const {return recordNumber;}


    /**
     * Get the number of events written to a file/buffer.
     * Remember that a particular event may not yet be
     * flushed to the file/buffer.
     * If the file being written to is split, the returned
     * value refers to all split files taken together.
     *
     * @return number of events written to a file/buffer.
     */
    uint32_t EventWriter::getEventsWritten() const {
        //std::cout << "getEventsWritten: eventsWrittenTotal = " << eventsWrittenTotal <<
        //        ", curRec.getEvCount = " << currentRecord->getEventCount() << std::endl;
        //return eventsWrittenTotal + currentRecord->getEventCount();
        return eventsWrittenTotal;
    }


    /**
     * Get the byte order of the buffer/file being written into.
     * @return byte order of the buffer/file being written into.
     */
    ByteOrder EventWriter::getByteOrder()  const {return byteOrder;}


    /**
     * Set the number with which to start record numbers.
     * This method does nothing if events have already been written.
     * @param startingRecordNumber  the number with which to start record numbers.
     */
    void EventWriter::setStartingRecordNumber(uint32_t startingRecordNumber) {
        // If events have been written already, forget about it
        if (eventsWrittenTotal > 0) return;
        recordNumber = startingRecordNumber;
        //std::cout << "setStartingRecordNumber: set to " << recordNumber << std::endl;
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
     * the buffer is never split. For that reason it throws an exception.<p>
     *
     * Do not call this while simultaneously calling
     * close, flush, writeEvent, or getByteBuffer.
     *
     * @param node node representing event to be placed first in each file written
     *             including all splits.
     *
     * @throws EvioException if error writing to file
     *                       if first event is opposite byte order of internal buffer;
     *                       if bad data format;
     *                       if close() already called;
      *                      if writing to buffer;
    *                        if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    void EventWriter::setFirstEvent(std::shared_ptr<EvioNode> node) {

        if (closed) {return;}

        if (!toFile) {
            throw EvioException("cannot write first event to buffer");
        }

        // There's no way to remove an event from a record, so reconstruct it.
        createCommonRecord(xmlDictionary, nullptr, node, nullptr);

        if (recordsWritten > 0) {
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
     * the buffer is never split. For that reason it throws an exception.<p>
     *
     * Do not call this while simultaneously calling
     * close, flush, writeEvent, or getByteBuffer.
     *
     * @param buf buffer containing event to be placed first in each file written
     *               including all splits.
     *
     * @throws EvioException if error writing to file
     *                       if first event is opposite byte order of internal buffer;
     *                       if bad data format;
     *                       if close() already called;
     *                       if writing to buffer;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    void EventWriter::setFirstEvent(std::shared_ptr<ByteBuffer> buf) {

        if (closed) {return;}

        if ((buf->remaining() < 8) && (xmlDictionary.empty())) {
            commonRecord = nullptr;
            return;
        }

        if (!toFile) {
            throw EvioException("cannot write first event to buffer");
        }

        createCommonRecord(xmlDictionary, nullptr, nullptr, buf);

        if ((recordsWritten > 0) && (buf->remaining() > 7)) {
            writeEvent(buf, false, false);
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
     * the buffer is never split. For that reason it throws an exception.<p>
     *
     * Do not call this while simultaneously calling
     * close, flush, writeEvent, or getByteBuffer.
     *
     * @param bank event to be placed first in each file written including all splits.
     *
     * @throws EvioException if error writing to file
     *                       if first event is opposite byte order of internal buffer;
     *                       if bad data format;
     *                       if close() already called;
     *                       if writing to buffer;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    void EventWriter::setFirstEvent(std::shared_ptr<EvioBank> bank) {

        if (closed) {return;}

        if (!toFile) {
            throw EvioException("cannot write first event to buffer");
        }

        createCommonRecord(xmlDictionary, bank, nullptr, nullptr);

        if (recordsWritten > 0) {
            writeEvent(bank, nullptr, false, false);
        }
    }


    /**
     * Create and fill the common record which contains the dictionary and first event.
     * Use the firstBank as the first event if specified, else try using the
     * firstNode if specified, else try the firstBuf.
     *
     * @param xmlDict        xml dictionary
     * @param firstBank      first event as EvioBank
     * @param firstNode      first event as EvioNode
     * @param firstBuf       first event as ByteBuffer
     * @throws EvioException if dictionary is in improper format
     */
    void EventWriter::createCommonRecord(const std::string & xmlDict,
                                         std::shared_ptr<EvioBank> firstBank,
                                         std::shared_ptr<EvioNode> firstNode,
                                         std::shared_ptr<ByteBuffer> firstBuf) {

        // Create record if necessary, else clear it
        if (commonRecord == nullptr) {
            // No compression please ...
            commonRecord = std::make_shared<RecordOutput>(byteOrder, 0, 0,
                                                          Compressor::CompressionType::UNCOMPRESSED);
        }
        else {
            commonRecord->reset();
        }

        // Place dictionary & first event into a single record
        if (!xmlDict.empty()) {
//std::cout << "createCommonRecord: got dictionary, # chars = " << xmlDict.size() << std::endl;
            // 56 is the minimum number of characters for a valid xml dictionary
            if (xmlDict.length() < 56) {
                throw EvioException("Dictionary improper format, too few characters");
            }

            // Turn dictionary data into ascii (not evio bank)
            Util::stringToASCII(xmlDict, dictionaryByteArray);

            // Add to record which will be our file header's "user header"
            commonRecord->addEvent(dictionaryByteArray);
//std::cout << "createCommonRecord: turned dictionary into ascii & added to commonRecord, byte size = " <<
//             dictionaryByteArray.size() << std::endl;
        }
        else {
            dictionaryByteArray.clear();
        }

        // Convert first event into bytes
        haveFirstEvent = true;
        if (firstBank != nullptr) {
            firstEventByteArray.resize(firstBank->getTotalBytes());
            firstBank->write(firstEventByteArray.data(), byteOrder);

            // Add to record which will be our file header's "user header"
            commonRecord->addEvent(firstEventByteArray);
        }
        else if (firstNode != nullptr) {
            ByteBuffer firstEventBuf(firstNode->getTotalBytes());
            firstNode->getStructureBuffer(firstEventBuf, true);
            commonRecord->addEvent(firstEventBuf);
        }
        else if (firstBuf != nullptr) {
            commonRecord->addEvent(firstBuf);
        }
        else {
            haveFirstEvent = false;
        }

        commonRecord->build();
        commonRecordBytesToBuffer = 4*commonRecord->getHeader()->getLengthWords();
//std::cout << "createCommonRecord: padded commonRecord size is " << commonRecordBytesToBuffer << " bytes" << std::endl;
    }


    /**
     * Create and write a general file header into the file.
     * The general header's user header is the common record which
     * contains any existing dictionary and first event.<p>
     *
     * Call this method AFTER file split or, in constructor, after the file
     * name is created in order to ensure a consistent value for file split number.
     */
    void EventWriter::writeFileHeader() {
        // For the file header, our "user header" will be the common record,
        // which is a record containing the dictionary and first event.

        fileHeader.reset();
        // File split # in header. Go back to last one as currently is set for the next split.
        fileHeader.setFileNumber(splitNumber - splitIncrement);

        // Check to see if we have dictionary and/or first event
        int commonRecordBytes = 0;
        int commonRecordCount = 0;

        if (commonRecord != nullptr) {
            commonRecordCount = commonRecord->getEventCount();
            if (commonRecordCount > 0) {
                commonRecordBytes = commonRecord->getHeader()->getLength();
                bool haveDict = !dictionaryByteArray.empty();
                fileHeader.setBitInfo(haveFirstEvent, haveDict, false);
            }
            // Sets file header length too
            fileHeader.setUserHeaderLength(commonRecordBytes);
        }

        // Index array is unused

        // Total header size in bytes
        int bytes = fileHeader.getLength();
        ByteBuffer buf(bytes);
        uint8_t* array = buf.array();
        buf.order(byteOrder);

        // Write file header into array
        try {
            fileHeader.writeHeader(buf, 0);
        }
        catch (EvioException & e) {/* never happen */}

        // Write user header into array if necessary
        if (commonRecordBytes > 0) {
            auto commonBuf = commonRecord->getBinaryBuffer();
            uint8_t* commonArray = commonBuf->array();
            if (commonArray != nullptr) {
                std::memcpy((void *)(array + FileHeader::HEADER_SIZE_BYTES),
                            (const void *)(commonArray + commonBuf->arrayOffset()), commonRecordBytes);
            }
            else {
                commonBuf->getBytes(array + FileHeader::HEADER_SIZE_BYTES, commonRecordBytes);
            }
        }

        // Write array into file
        asyncFileChannel->write(reinterpret_cast<char *>(array), bytes);

        eventsWrittenTotal = eventsWrittenToFile = commonRecordCount;
        bytesWritten = bytes;
        fileWritingPosition += bytes;
    }


    /**
     * This method flushes any remaining internally buffered data to file.
     * Calling {@link #close()} automatically does this so it isn't necessary
     * to call before closing. This method should only be used when writing
     * events at such a low rate that it takes an inordinate amount of time
     * for internally buffered data to be written to the file.<p>
     *
     * Calling this may easily kill performance. May not call this when simultaneously
     * calling writeEvent, close, setFirstEvent, or getByteBuffer.
     */
    void EventWriter::flush() {

        if (closed) {
            return;
        }

        if (toFile) {
            if (singleThreadedCompression) {
                try {
                    compressAndWriteToFile(true);
                }
                catch (EvioException & e) {
                    std::cout << e.what() << std::endl;
                }
            }
            else {
                // Write any existing data.
                currentRingItem->forceToDisk(true);
                if (currentRecord->getEventCount() > 0) {
                    // Send current record back to ring
                    supply->publish(currentRingItem);
                }

                // Get another empty record from ring
                std::cout << "EventWriter: flush, get ring item, seq = " << currentRingItem->getSequence() << std::endl;
                currentRingItem = supply->get();
                currentRecord = currentRingItem->getRecord();
            }
        }
        else {
            flushCurrentRecordToBuffer();
        }
    }


    /**
     * This method flushes any remaining data to file and disables this object.
     * This needs to be called before getting buffer through
     * {@link #getByteBuffer()} so that the buffer is ready to read.
     * May not call this when simultaneously calling
     * writeEvent, flush, setFirstEvent, or getByteBuffer.
     */
    void EventWriter::close() {
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
            catch (std::exception & e) {
                // We're here if buffer is too small
                std::cout << e.what() << std::endl;
            }
        }
        // If file ...
        else {
            // Write record to file
            if (singleThreadedCompression) {
               if (currentRecord->getEventCount() > 0) {
                    try {compressAndWriteToFile(false);}
                    catch (std::exception & e) {
                        std::cout << e.what() << std::endl;
                    }
                }
            }
            else {
                // If we're building a record, send it off
                // to compressing thread since we're done.
                // This should never happen as END event forces things through.
                if (currentRecord->getEventCount() > 0) {
                    // Put it back in supply for compressing and force to disk
                    supply->publish(currentRingItem);
                }

                // Since the writer thread is the last to process each record,
                // wait until it's done with the last item, then exit the thread.
                recordWriterThread[0].waitForLastItem();

                // Stop all compressing threads which by now are stuck on get
                for (RecordCompressor &thd : recordCompressorThreads) {
                    thd.stopThread();
                }
            }

            // Finish writing record to current file
            try {
                if (future1 != nullptr && future1->valid()) {
                    // Wait for last write to end before we continue
                    future1->get();
                }
            }
            catch (std::exception & e) {}

            // Write trailer
            if (addingTrailer) {
                // Write the trailer
                try {
                    writeTrailerToFile(addTrailerIndex);
                }
                catch (std::exception & e) {
                    std::cout << e.what() << std::endl;
                }
            }

            try {
                // Find & update file header's record count word
                ByteBuffer bb(4);
                bb.order(byteOrder);
                bb.putInt(0, recordNumber - 1);
                asyncFileChannel->seekg(FileHeader::RECORD_COUNT_OFFSET);
                asyncFileChannel->write(reinterpret_cast<char*>(bb.array()), 4);
            }
            catch (std::exception & e) {
                std::cout << e.what() << std::endl;
            }

            try {
                if (asyncFileChannel->is_open()) asyncFileChannel->close();

                // Shut down all file closing threads
                if (fileCloser != nullptr) fileCloser->close();
            }
            catch (std::exception & e) {}

            // release resources
            supply.reset();
            currentRecord.reset();
            recordWriterThread.clear();
            recordCompressorThreads.clear();
            ringItem1.reset();
            currentRingItem.reset();
        }

        recordLengths->clear();
        closed = true;
    }


    /**
     * Reads part of the file header in order to determine
     * the evio version # and endianness of the file in question.
     *
     * @throws EvioException not in append mode, contains too little data, is not in proper format,
     *                       version earlier than 6, premature EOF or file reading error,
     *                       and all other exceptions.
     */
    void EventWriter::examineFileHeader() {

        // Only for append mode - only used for files
        if (!append) {
            // Internal logic error, should never have gotten here
            throw EvioException("need to be in append mode");
        }

        int nBytes;

        auto headerBytes = new char[FileHeader::HEADER_SIZE_BYTES];
        ByteBuffer buf(headerBytes, FileHeader::HEADER_SIZE_BYTES);

        asyncFileChannel->read(headerBytes, FileHeader::HEADER_SIZE_BYTES);
        nBytes = asyncFileChannel->gcount();
        if (asyncFileChannel->fail()) {
            throw EvioException("error reading first record header from " + currentFileName);
        }

        // Check to see if we read the whole header
        if (nBytes != FileHeader::HEADER_SIZE_BYTES) {
            throw EvioException("bad file format");
        }

        // Parse header info
        appendFileHeader = FileHeader();
        buf.position(0);
        // Buffer's position/limit does not change
        appendFileHeader.readHeader(buf);

        // Set the byte order to match the buffer/file's ordering.
        byteOrder = appendFileHeader.getByteOrder();

        hasAppendDictionary = appendFileHeader.hasDictionary();
        hasTrailerWithIndex = appendFileHeader.hasTrailerWithIndex();
        indexLength         = appendFileHeader.getIndexLength();
        userHeaderLength    = appendFileHeader.getUserHeaderLength();
        userHeaderPadding   = appendFileHeader.getUserHeaderLengthPadding();
        //
        //        int fileID      = appendFileHeader.getFileId();
        //        int headerLen   = appendFileHeader.getHeaderLength();
        //        int recordCount = appendFileHeader.getEntries();
        //        int fileNum     = appendFileHeader.getFileNumber();
        //
        //        std::cout << "file ID          = " << fileID << std::endl;
        //        std::cout << "fileNumber       = " << fileNum << std::endl;
        //        std::cout << "headerLength     = " << headerLen << std::endl;
        //        std::cout << "recordCount      = " << recordCount << std::endl;
        //        std::cout << "trailer index    = " << hasTrailerWithIndex << std::endl;
        //        std::cout << "bit info         = " << Integer.toHexString(bitInfo) << std::endl;
        //        std::cout << std::endl;
    }


    /**
     * This method positions a file for the first {@link #writeEvent(std::shared_ptr<EvioBank>)}
     * in append mode. It places the writing position after the last event (not record header).
     *
     * @throws EvioException     if file reading/writing problems,
     *                           if bad file/buffer format; if not in append mode
     */
    void EventWriter::toAppendPosition() {

        // Only for append mode
        if (!append) {
            throw EvioException("need to be in append mode");
        }

        // Jump over the file header, index array, and user header & padding
        uint64_t pos = FileHeader::HEADER_SIZE_BYTES + indexLength +
                       userHeaderLength + userHeaderPadding;
        // This puts us at the beginning of the first record header
        fileWritingPosition = pos;

#ifdef USE_FILESYSTEMLIB
        uint64_t fileSize = fs::file_size(currentFileName);
#else
        uint64_t fileSize;
        struct stat stt{};
        if (stat(currentFileName.c_str(), &stt) == 0) {
            fileSize = stt.st_size;
        }
        else {
            throw EvioException("error getting file size of " + currentFileName);
        }
#endif
//std::cout << "toAppendPos:  fileSize = " << fileSize << ", jump to pos = " << fileWritingPosition << std::endl;

        bool lastRecord, isTrailer, readEOF = false;
        uint32_t recordLen, eventCount, nBytes, bitInfo, headerPosition;

        uint64_t bytesLeftInFile = fileSize;

        //        uint32_t indexArrayLen, userHeaderLen, compDataLen, unCompDataLen;
        //        uint32_t userPadding, dataPadding, compPadding;
        //        uint32_t headerLen, currentPosition, compType, compWord;

        // The file's record #s may be fine or they may be messed up.
        // Assume they start with one and increment from there. That
        // way any additional records now added to the file will have a
        // reasonable # instead of incrementing from the last existing
        // record.
        recordNumber = 1;
//std::cout << "toAppendPos:     record # = 1" << std::endl;

        // To read in all of the normal record header set this to 40 bytes.
        // To read the bare minimum to do the append set this to 24 bytes,
        // but be sure to comment out lines reading beyond this point in the header.
        uint32_t headerBytesToRead = 40;

        while (true) {
            nBytes = 0;

            buffer->clear();
            buffer->limit(headerBytesToRead);

            while (nBytes < headerBytesToRead) {
 //std::cout << "Read Header bytes" << std::endl;

                // There is no internal asyncFileChannel position
                asyncFileChannel->seekg(fileWritingPosition);
                asyncFileChannel->read(reinterpret_cast<char *>(buffer->array()) + nBytes, headerBytesToRead-nBytes);
                int partial = asyncFileChannel->gcount();
                if (asyncFileChannel->fail()) {
                    throw EvioException("error reading record header from " + currentFileName);
                }

                // TODO: handling EOF seems wrong.............
                // TODO: If eof hit with nBytes = 0, then header should NOT be parsed ....

                // If EOF ...
                if (asyncFileChannel->eof()) {
                    if (nBytes != 0) {
                        throw EvioException("bad buffer format");
                    }
                    // Missing last empty record header
                    readEOF = true;
                    break;
                }
                nBytes += partial;
                bytesLeftInFile -= partial;
            }

            // If we did not read correct # of bytes or didn't run into EOF right away
            if (nBytes != 0 && nBytes != headerBytesToRead) {
                throw EvioException("internal file reading error");
            }

            headerPosition = 0;
            fileWritingPosition += headerBytesToRead;

            bitInfo    = buffer->getInt(headerPosition + RecordHeader::BIT_INFO_OFFSET);
            recordLen  = buffer->getInt(headerPosition + RecordHeader::RECORD_LENGTH_OFFSET);
            eventCount = buffer->getInt(headerPosition + RecordHeader::EVENT_COUNT_OFFSET);
            lastRecord = RecordHeader::isLastRecord(bitInfo);
            isTrailer  = RecordHeader::isEvioTrailer(bitInfo);

            ////          If reading entire header, change headerBytesToRead from 24 to 40
            //            int headerLen     = buffer->getInt(headerPosition + RecordHeader::HEADER_LENGTH_OFFSET);
            //            int userHeaderLen = buffer->getInt(headerPosition + RecordHeader::USER_LENGTH_OFFSET);
            //            int indexArrayLen = buffer->getInt(headerPosition + RecordHeader::INDEX_ARRAY_OFFSET);
            //            int unCompDataLen = buffer->getInt(headerPosition + RecordHeader::UNCOMPRESSED_LENGTH_OFFSET);
            //
            //            int compWord      = buffer->getInt(headerPosition + RecordHeader::COMPRESSION_TYPE_OFFSET);
            //            int compType = compWord >> 28 & 0xff;
            //            // If there is compression ...
            //            if (compType != 0) {
            //                compDataLen = compWord & 0xfffffff;
            //            }

//            std::cout << "bitInfo      = 0x" << std::hex << bitInfo << std::dec << std::endl;
//            std::cout << "recordLength = " << recordLen << std::endl;
//            std::cout << "eventCount   = " << eventCount << std::endl;
//            std::cout << "lastRecord   = " << lastRecord << std::endl;
//            std::cout << std::endl;

            // Update vector with record size & event count unless this is the trailer
            if (!isTrailer) {
//std::cout << "                 adding to recordLengths append: " << (4 * recordLen) << ", " <<
//             eventCount << "   ------" << std::endl;
                recordLengths->push_back(4 * recordLen);
                recordLengths->push_back(eventCount);
            }

            // Track total number of events in file/buffer (minus dictionary)
            eventsWrittenTotal += eventCount;

            recordNumber++;
//std::cout << "                 next record # = " << recordNumber << std::endl;

            // Stop at the last record. The file may not have a last record if
            // improperly terminated. Running into an End-Of-File will flag
            // this condition.
            if (isTrailer || lastRecord || readEOF) {
                break;
            }

            // Hop to next record header
            if (4*recordLen < headerBytesToRead) {
                throw EvioException("bad file format");
            }

            uint32_t bytesToNextBlockHeader = 4*recordLen - headerBytesToRead;
            if (bytesLeftInFile < bytesToNextBlockHeader) {
                throw EvioException("bad file format");
            }
            fileWritingPosition += bytesToNextBlockHeader;
            bytesLeftInFile     -= bytesToNextBlockHeader;
            asyncFileChannel->seekg(fileWritingPosition);
        }

        if (hasAppendDictionary) {
            eventsWrittenToFile = eventsWrittenToBuffer = eventsWrittenTotal + 1;
        }
        else {
            eventsWrittenToFile = eventsWrittenToBuffer = eventsWrittenTotal;
        }

        //-------------------------------------------------------------------------------
        // If we're here, we've just read the last record header (at least some of it).
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
//std::cout << "                 read EOF, record # = " << recordNumber << std::endl;
        }
            // else if last record or has NO data in it ...
        else if (isTrailer || eventCount < 1) {
            // We already partially read in the record header, now back up so we can overwrite it.
            // If using buffer, we never incremented the position, so we're OK.

            // Since creating next record does ++recordNumber, we decrement it first
            recordNumber--;

//std::cout << "                 last rec has no data, is Trailer = " << isTrailer << ", record # = " << recordNumber << std::endl;
            fileWritingPosition -= headerBytesToRead;
//std::cout << "toAppendPos: position (bkup) = " << fileWritingPosition << std::endl;
            asyncFileChannel->seekg(fileWritingPosition);
        }
        else {
            // Clear last record bit in 6th header word
            bitInfo = RecordHeader::clearLastRecordBit(bitInfo);

            // Rewrite header word with new bit info & hop over record

            // File now positioned right after the last header to be read
            // Back up to before 6th block header word
            fileWritingPosition -= headerBytesToRead - RecordHeader::BIT_INFO_OFFSET;
            asyncFileChannel->seekg(fileWritingPosition);

//std::cout << "toAppendPosition: writing over last block's 6th word, back up " <<
//             (headerBytesToRead - RecordHeader::BIT_INFO_OFFSET)/4 << " words" << std::endl;

            // Write over 6th block header word
            buffer->clear();
            buffer->putInt(bitInfo);

            asyncFileChannel->read(reinterpret_cast<char *>(buffer->array()), 4);
            if (asyncFileChannel->fail()) {
                throw EvioException("error updating last record header in " + currentFileName);
            }

            // Hop over the entire block
//std::cout << "toAppendPosition: wrote over last block's 6th word, hop over whole record, " <<
//             ((4 * recordLen) - (RecordHeader::BIT_INFO_OFFSET + 4))/4 << " words" << std::endl;
            fileWritingPosition += (4 * recordLen) - (RecordHeader::BIT_INFO_OFFSET + 4);
            asyncFileChannel->seekg(fileWritingPosition);
        }

        bytesWritten = fileWritingPosition;
        recordsWritten = recordNumber - 1;

        //        // The when writing the NEXT record, code does an ++recordNumber,
        //        // so account for that
        //        recordNumber--;
//std::cout << "toAppendPos: file pos = " << fileWritingPosition <<
//             ", recordNumber = " << recordNumber << std::endl;

        // We should now be in a state identical to that if we had
        // just now written everything currently in the file/buffer.
        buffer->clear();
    }


    /**
     * Is there room to write this many bytes to an output buffer as a single event?
     * Will always return true when writing to a file.
     * @param bytes number of bytes to write
     * @return {@code true} if there still room in the output buffer, else {@code false}.
     */
    bool EventWriter::hasRoom(uint32_t bytes) {
        //std::cout << "User buffer size (" << currentRecord->getInternalBufferCapacity() << ") - bytesWritten (" << bytesWritten <<
        //      ") - trailer (" << trailerBytes() <<  ") = (" <<
        //         ((currentRecord->getInternalBufferCapacity() - bytesWritten) >= bytes + RecordHeader::HEADER_SIZE_BYTES) <<
        //      ") >= ? " << bytes << std::endl;
        return writingToFile() || (((currentRecord->getInternalBufferCapacity() -
                                    bytesWritten - trailerBytes()) >= bytes) &&
                                    !currentRecord->oneTooMany());
    }


    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file (for multiple compression
     * threads), the record will be sent to a thread which may compress the data,
     * then it will be sent to a thread to write the record to file.
     * If there is only 1 compression thread, it's all done in the thread which
     * call this method.<p>
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
     * @param ownRecord  if true, write event in its own record regardless
     *                   of event count and record size limits.
     *
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         or record event count limit exceeded.
     *
     * @throws EvioException if error writing file
     *                       if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     */
    bool EventWriter::writeEvent(std::shared_ptr<EvioNode> & node, bool force, bool duplicate, bool ownRecord) {

        std::shared_ptr<ByteBuffer> eventBuffer;
        auto bb = node->getBuffer();

        // Duplicate buffer so we can set pos & limit without messing others up
        if (duplicate) {
            eventBuffer = bb->duplicate();
            eventBuffer->order(bb->order());
        }
        else {
            eventBuffer = bb;
        }

        uint32_t pos = node->getPosition();
        eventBuffer->limit(pos + node->getTotalBytes()).position(pos);
        return writeEvent(nullptr, eventBuffer, force, ownRecord);
    }

    /**
     * Write an event (bank) into a record and eventually to a file in evio/hipo
     * version 6 format.
     * Once the record is full and if writing with multiple compression
     * threads, the record will be sent to a thread which may compress the data,
     * then it will be sent to a thread to write the record to file.
     * If there is only 1 compression thread, it's all done in the thread which
     * call this method.<p>
     *
     * <b>
     * If splitting files, this method returns false if disk partition is too full
     * to write the complete, next split file. If force arg is true, write anyway.
     * DO NOT mix calling this method with calling
     * {@link #writeEvent()} (all variants).
     * Results are unpredictable as it messes up the
     * logic used to quit writing to full disk.
     * </b>
     *
     * The buffer must contain only the event's data (event header and event data)
     * and must <b>not</b> be in complete evio file format.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     * <b>kill</b> performance when writing to a file.
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
     * @param ownRecord  if true, write event in its own record regardless
     *                   of event count and record size limits.
     *
     * @return true if event was added to record. If splitting files, false if disk
     *         partition too full to write the complete, next split file.
     *         False if interrupted. If force arg is true, write anyway.
     *
     * @throws EvioException if error writing file
     *                       if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     */
    bool EventWriter::writeEventToFile(std::shared_ptr<EvioNode> & node, bool force,
                                       bool duplicate, bool ownRecord) {

        std::shared_ptr<ByteBuffer> eventBuffer;
        auto bb = node->getBuffer();

        // Duplicate buffer so we can set pos & limit without messing others up
        if (duplicate) {
            eventBuffer = bb->duplicate();
            eventBuffer->order(bb->order());
        }
        else {
            eventBuffer = bb;
        }

        uint32_t pos = node->getPosition();
        eventBuffer->limit(pos + node->getTotalBytes()).position(pos);
        return writeEventToFile(nullptr, eventBuffer, force, ownRecord);
    }

    /**
     * Write an event (bank) into a record and eventually to a file in evio/hipo
     * version 6 format.
     * Once the record is full and if writing with multiple compression
     * threads, the record will be sent to a thread which may compress the data,
     * then it will be sent to a thread to write the record to file.
     * If there is only 1 compression thread, it's all done in the thread which
     * call this method.<p>
     *
     * <b>
     * If splitting files, this method returns false if disk partition is too full
     * to write the complete, next split file. If force arg is true, write anyway.
     * DO NOT mix calling this method with calling
     * {@link #writeEvent()} (all variants).
     * Results are unpredictable as it messes up the
     * logic used to quit writing to full disk.
     * </b>
     *
     * The buffer must contain only the event's data (event header and event data)
     * and must <b>not</b> be in complete evio file format.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     * <b>kill</b> performance when writing to a file.
     * A true 3rd arg can be used when the backing buffer
     * of the node is accessed by multiple threads simultaneously. This allows
     * that buffer's limit and position to be changed without interfering
     * with the other threads.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param bb         ByteBuffer representing the event.
     * @param force      if writing to disk, force it to write event to the disk.
     * @param duplicate  if true, duplicate bb so its position and limit
     *                   can be changed without issue.
     * @param ownRecord  if true, write event in its own record regardless
     *                   of event count and record size limits.
     *
     * @return true if event was added to record. If splitting files, false if disk
     *         partition too full to write the complete, next split file.
     *         False if interrupted. If force arg is true, write anyway.
     *
     * @throws EvioException if error writing file
     *                       if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     */
    bool EventWriter::writeEventToFile(std::shared_ptr<ByteBuffer> & bb, bool force,
                                       bool duplicate, bool ownRecord) {

        std::shared_ptr<ByteBuffer> eventBuffer;

        // Duplicate buffer so we can set pos & limit without messing others up
        if (duplicate) {
            eventBuffer = bb->duplicate();
            eventBuffer->order(bb->order());
        }
        else {
            eventBuffer = bb;
        }

        return writeEventToFile(nullptr, eventBuffer, force, ownRecord);
    }


    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file (for multiple compression
     * threads), the record will be sent to a thread which may compress the data,
     * then it will be sent to a thread to write the record to file.
     * If there is only 1 compression thread, it's all done in the thread which
     * call this method.<p>
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
     * @param ownRecord  if true, write event in its own record regardless
     *                   of event count and record size limits.
     *
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         or record event count limit exceeded.
     *
     * @throws EvioException if error writing file
     *                       if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing.
     */
    bool EventWriter::writeEvent(std::shared_ptr<ByteBuffer> & bankBuffer, bool force, bool ownRecord) {
        return writeEvent(nullptr, bankBuffer, force, ownRecord);
    }


    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file (for multiple compression
     * threads), the record will be sent to a thread which may compress the data,
     * then it will be sent to a thread to write the record to file.
     * If there is only 1 compression thread, it's all done in the thread which
     * call this method.<p>
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
     * @param ownRecord  if true, write event in its own record regardless
     *                   of event count and record size limits.
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         or record event count limit exceeded.
     *
     * @throws EvioException if error writing file
     *                       if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;.
     */
    bool EventWriter::writeEvent(std::shared_ptr<EvioBank> bank, bool force, bool ownRecord) {
        return writeEvent(bank, nullptr, force, ownRecord);
    }


    /**
     * Write an event (bank) into a record and eventually to a file in evio/hipo
     * version 6 format.
     * Once the record is full and if writing with multiple compression
     * threads, the record will be sent to a thread which may compress the data,
     * then it will be sent to a thread to write the record to file.
     * If there is only 1 compression thread, it's all done in the thread which
     * call this method.<p>
     *
     * <b>
     * If splitting files, this method returns false if disk partition is too full
     * to write the complete, next split file. If force arg is true, write anyway.
     * DO NOT mix calling this method with calling
     * {@link #writeEvent()} (all variants).
     * Results are unpredictable as it messes up the
     * logic used to quit writing to full disk.
     * </b>
     *
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     * <b>kill</b> performance when writing to a file.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param bank       the bank (as an EvioBank object) to write.
     * @param force      if writing to disk, force it to write event to the disk.
     * @param ownRecord  if true, write event in its own record regardless
     *                   of event count and record size limits.
     *
     * @return true if event was added to record. If splitting files, false if disk
     *         partition too full to write the complete, next split file.
     *         False if interrupted. If force arg is true, write anyway.
     *
     * @throws EvioException if error writing file
     *                       if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     */
    bool EventWriter::writeEventToFile(std::shared_ptr<EvioBank> bank, bool force, bool ownRecord) {
        return writeEventToFile(bank, nullptr, force, ownRecord);
    }


    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file (for multiple compression
     * threads), the record will be sent to a thread which may compress the data,
     * then it will be sent to a thread to write the record to file.
     * If there is only 1 compression thread, it's all done in the thread which
     * call this method.<p>
     * If writing to a buffer, once the record is full this method returns
     * false - indicating that the last event was NOT written to the record.
     * To finish the writing process, call {@link #close()}. This will
     * compress the data if desired and then write it to the buffer.<p>
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
     *<b>kill</b> performance when writing to a file.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param bank       the bank (as an EvioBank object) to write.
     * @param bankBuffer the bank (as a ByteBuffer object) to write.
     * @param force      if writing to disk, force it to write event to the disk.
     * @param ownRecord  if true, write event in its own record regardless
     *                   of event count and record size limits.
     *
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         record event count limit exceeded, or bank and bankBuffer args are both null.
     *
     * @throws EvioException if error writing file;
     *                       if event is opposite byte order of internal buffer;
     *                       if bad bankBuffer format;
     *                       if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing.
     */
    bool EventWriter::writeEvent(std::shared_ptr<EvioBank> bank,
                                 std::shared_ptr<ByteBuffer> bankBuffer,
                                 bool force, bool ownRecord) {

        if (closed) {
            throw EvioException("close() has already been called");
        }

        bool fitInRecord;
        bool splittingFile = false;
        // See how much space the event will take up
        size_t currentEventBytes;

        // Which bank do we write?
        if (bankBuffer != nullptr) {
            if (bankBuffer->order() != byteOrder) {
                throw EvioException("event buf is " + bankBuffer->order().getName() +
                                    ", and writer is " + byteOrder.getName());
            }

            // Event size in bytes (from buffer ready to read)
            currentEventBytes = bankBuffer->remaining();

            // Size must be multiple of 4 bytes (whole 32-bit ints)
            // The following is the equivalent of the mod operation:
            //  (currentEventBytes % 4 != 0) but is much faster (x mod 2^n == x & (2^n - 1))
            if ((currentEventBytes & 3) != 0) {
                throw EvioException("bad bankBuffer format");
            }

            // Check for inconsistent lengths
            if (currentEventBytes != 4 * (bankBuffer->getUInt(bankBuffer->position()) + 1)) {
                throw EvioException("inconsistent event lengths: total bytes from event = " +
                                            std::to_string(4*(bankBuffer->getUInt(bankBuffer->position()) + 1)) +
                                    ", from buffer = " + std::to_string(currentEventBytes));
            }
        }
        else if (bank != nullptr) {
            currentEventBytes = bank->getTotalBytes();
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

        // If we're splitting files,
        // we must have written at least one real event before we
        // can actually split the file.
        if ((split > 0) && (splitEventCount > 0)) {
            // Is event, along with the previous events, large enough to split the file?
            // For simplicity ignore the headers which will take < 2Kb.
            // Take any compression roughly into account.
            uint64_t totalSize = (currentEventBytes + splitEventBytes)*compressionFactor/100;

            // If we're going to split the file, set a couple flags
            if (totalSize > split) {
                //                std::cout << "Split at total size = " << totalSize <<
                //                                   ", ev = " << currentEventBytes <<
                //                                   ", prev = " << splitEventBytes <<
                //                                   ", compressed data = " <<
                //                                   ((currentEventBytes + splitEventBytes)*compressionFactor/100) << std::endl;
                splittingFile = true;
            }
        }

        // First, if multithreaded write, check for any errors that may have
        // occurred asynchronously in the write or one of the compression threads.
        if (!singleThreadedCompression && supply->haveError()) {
            // Wake up any of these threads waiting for another record
            supply->errorAlert();
            throw EvioException(supply->getError());
        }

        // Including this event, this is the total data size & event count
        // for this split file.
        splitEventBytes += currentEventBytes;
        splitEventCount++;

        // If event is big enough to split the file ...
        if (splittingFile) {
            if (singleThreadedCompression) {
                try {
                    compressAndWriteToFile(false);
                }
                catch (boost::thread_interrupted & e) {
                    return false;
                }
                catch (std::exception & e) {
                    throw EvioException(e);
                }

                splitFile();
            }
            else {
                // Set flag to split file
                currentRingItem->splitFileAfterWrite(true);
                //currentRecord->getHeader().setRecordNumber(recordNumber);
                // Send current record back to ring without adding event
                supply->publish(currentRingItem);

                // Get another empty record from ring.
                // Record number reset for new file.
                recordNumber = 1;
                currentRingItem = supply->get();
                currentRecord = currentRingItem->getRecord();
                currentRecord->getHeader()->setRecordNumber(recordNumber++);
                //std::cout << "writeEvent: split after just published rec, new rec# = 1, next = " << recordNumber << std::endl;
                // Reset record number for records coming after this one
            }

            // Reset split-tracking variables
            splitEventBytes = 0L;
            splitEventCount = 0;
        }

        // If event to be written in its own record
        if (ownRecord) {
            fitInRecord = false;
        }
        // Try adding event to current record.
        // One event is guaranteed to fit in a record no matter the size,
        // IFF using multithreaded compression.
        else if (bankBuffer != nullptr) {
            fitInRecord = currentRecord->addEvent(bankBuffer);
        }
        else {
            fitInRecord = currentRecord->addEvent(bank);
        }

        // If no room or too many events ...
        if (!fitInRecord) {
            if (singleThreadedCompression) {
                // Only try to write what we have if there's something to write
                if (currentRecord->getEventCount() > 0) {
                    try {
                        compressAndWriteToFile(false);
                    }
                    catch (boost::thread_interrupted &e) {
                        return false;
                    }
                    catch (std::exception &e) {
                        throw EvioException(e);
                    }

                    // Try add eventing to it (1 very big event many not fit)
                    if (bankBuffer != nullptr) {
                        fitInRecord = currentRecord->addEvent(bankBuffer);
                    }
                    else {
                        fitInRecord = currentRecord->addEvent(bank);
                    }
                }

                // Since we're here, we have a single event too big to fit in the
                // allocated buffers. So expand buffers and try again.
                if (!fitInRecord) {
//std::cout << "writeEventToFile: expand buffers to " << (currentEventBytes/10*12) << "  bytes" << std::endl;
                    expandInternalBuffers(currentEventBytes);

                    if (bankBuffer != nullptr) {
                        fitInRecord = currentRecord->addEvent(bankBuffer);
                    }
                    else {
                        fitInRecord = currentRecord->addEvent(bank);
                    }

                    if (!fitInRecord) {
                        throw EvioException("cannot fit event into buffer");
                    }
//std::cout << "writeEventToFile: EVENT FINALLY FIT!" << std::endl;
                }
            }
            else {
                // This "if" is only needed since we may want event in its own record
                if (currentRecord->getEventCount() > 0) {
                    // Send current record back to ring without adding event
                    supply->publish(currentRingItem);

                    // Get another empty record from ring
                    currentRingItem = supply->get();
                    currentRecord = currentRingItem->getRecord();
                    currentRecord->getHeader()->setRecordNumber(recordNumber++);
//std::cout << "writeEvent: just published rec, new rec# = " << (recordNumber - 1) << ", next = " << recordNumber << std::endl;
                }

                // Add event to it (guaranteed to fit)
                if (bankBuffer != nullptr) {
                    currentRecord->addEvent(bankBuffer);
                }
                else {
                    currentRecord->addEvent(bank);
                }
            }
        }

        // If event must be physically written to disk,
        // or the event must be written in its own record ...
        if (force || ownRecord) {
            if (singleThreadedCompression) {
                try {
                    compressAndWriteToFile(force);
                }
                catch (boost::thread_interrupted & e) {
                    return false;
                }
                catch (std::exception & e) {
                    throw EvioException(e);
                }
            }
            else {
                // Tell writer to force this record to disk
                currentRingItem->forceToDisk(force);
                // Send current record back to ring
                supply->publish(currentRingItem);

                // Get another empty record from ring
                currentRingItem = supply->get();
                currentRecord = currentRingItem->getRecord();
                currentRecord->getHeader()->setRecordNumber(recordNumber++);
//std::cout << "writeEvent: FORCED published rec, new rec# = " << (recordNumber - 1) << ", next = " << recordNumber << std::endl;
            }
        }

        return true;
    }

    /**
     * Write an event (bank) into a record and eventually to a file in evio/hipo
     * version 6 format.
     * Once the record is full and if writing with multiple compression
     * threads, the record will be sent to a thread which may compress the data,
     * then it will be sent to a thread to write the record to file.
     * If there is only 1 compression thread, it's all done in the thread which
     * call this method.<p>
     *
     * <b>
     * If splitting files, this method returns false if disk partition is too full
     * to write the complete, next split file. If force arg is true, write anyway.
     * DO NOT mix calling this method with calling
     * {@link #writeEvent(std::shared_ptr<EvioBank>, std::shared_ptr<ByteBuffer>, bool)}
     * (or the various writeEvent() methods which call it).
     * Results are unpredictable as it messes up the
     * logic used to quit writing to full disk.
     * </b>
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
     *<b>kill</b> performance when writing to a file.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param bank       the bank (as an EvioBank object) to write.
     * @param bankBuffer the bank (as a ByteBuffer object) to write.
     * @param force      if writing to disk, force it to write event to the disk.
     * @param ownRecord  if true, write event in its own record regardless
     *                   of event count and record size limits.
     *
     * @return true if event was added to record. If splitting files, false if disk
     *         partition too full to write the complete, next split file.
     *         False if interrupted. If force arg is true, write anyway.
     *
     * @throws EvioException if error writing file
     *                       if event is opposite byte order of internal buffer;
     *                       if both buffer args are null;
     *                       if bad bankBuffer format;
     *                       if close() already called;
     *                       if not writing to file;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing.
     */
    bool EventWriter::writeEventToFile(std::shared_ptr<EvioBank> bank,
                                       std::shared_ptr<ByteBuffer> bankBuffer,
                                       bool force, bool ownRecord) {

        if (closed) {
            throw EvioException("close() has already been called");
        }

        if (!toFile) {
            throw EvioException("cannot write to buffer with this method");
        }

        // If here, we're writing to a file ...

        // First, if multithreaded write, check for any errors that may have
        // occurred asynchronously in the write or one of the compression threads.
        // Also check to see if disk is full.
        if (!singleThreadedCompression) {
            if (supply->haveError()) {
                // Wake up any of these threads waiting for another record
                supply->errorAlert();
                throw EvioException(supply->getError());
            }

            // With multithreaded writing, if the writing thread discovers that
            // the disk partition is full, everything that has made it past this check
            // and all the records in the pipeline (ring in this case) will be
            // written.
            if (diskIsFullVolatile.load() && !force) {
                // Check again to see if it's still full
                if (fullDisk()) {
                    // Still full
                    return false;
                }
                std::cout << "writeEventToFile: disk is NOT full, emptied" << std::endl;
            }
        }
            // If single threaded write, and we can't allow more events in due to limited disk space
        else if (diskIsFull && !force) {
            // Actually check disk again
            if (fullDisk()) {
                return false;
            }
        }

        bool fitInRecord;
        bool splittingFile = false;
        // See how much space the event will take up
        int currentEventBytes;

        // Which bank do we write?
        if (bankBuffer != nullptr) {
            if (bankBuffer->order() != byteOrder) {
                throw EvioException("event buf is " + bankBuffer->order().getName() +
                                    ", and writer is " + byteOrder.getName());
            }

            // Event size in bytes (from buffer ready to read)
            currentEventBytes = bankBuffer->remaining();

            // Size must be multiple of 4 bytes (whole 32-bit ints)
            // The following is the equivalent of the mod operation:
            //  (currentEventBytes % 4 != 0) but is much faster (x mod 2^n == x & (2^n - 1))
            if ((currentEventBytes & 3) != 0) {
                throw EvioException("bad bankBuffer format");
            }

            // Check for inconsistent lengths
            if (currentEventBytes != 4 * (bankBuffer->getInt(bankBuffer->position()) + 1)) {
                throw EvioException("inconsistent event lengths: total bytes from event = " +
                                            std::to_string(4*(bankBuffer->getInt(bankBuffer->position()) + 1)) +
                                    ", from buffer = " + std::to_string(currentEventBytes));
            }
        }
        else if (bank != nullptr) {
            currentEventBytes = bank->getTotalBytes();
        }
        else {
            throw EvioException("both buffer args are null");
        }

        // If we're splitting files,
        // we must have written at least one real event before we
        // can actually split the file.
        if ((split > 0) && (splitEventCount > 0)) {
            // Is event, along with the previous events, large enough to split the file?
            // For simplicity ignore the headers which will take < 2Kb.
            // Take any compression roughly into account.
            uint64_t totalSize = (currentEventBytes + splitEventBytes)*compressionFactor/100;

            // If we're going to split the file, set a couple flags
            if (totalSize > split) {
                splittingFile = true;
            }
        }

        // Including this event, this is the total data size & event count
        // for this split file.
        splitEventBytes += currentEventBytes;
        splitEventCount++;

        // If event is big enough to split the file, write what we already have
        // (not including current event).
        if (splittingFile) {
            if (singleThreadedCompression) {
                try {
                    // Should always be able to finish writing current file
                    compressAndWriteToFile(force);
                }
                catch (boost::thread_interrupted & e) {
                    return false;
                }
                catch (std::exception & e) {
                    throw EvioException(e);
                }

                splitFile();
            }
            else {
                // Set flag to split file. In this case, allow split to happen
                // even if disk partition is "full" since we've allowed enough
                // space for that.
                currentRingItem->splitFileAfterWrite(true);
                currentRingItem->setCheckDisk(false);
                // Send current record back to ring without adding event
                supply->publish(currentRingItem);

                // Get another empty record from ring.
                // Record number reset for new file.
                recordNumber = 1;
                currentRingItem = supply->get();
                currentRecord = currentRingItem->getRecord();
                currentRecord->getHeader()->setRecordNumber(recordNumber++);
                //std::cout << "writeEventToFile: split after just published rec, new rec# = 1, next = " << recordNumber << std::endl;
                // Reset record number for records coming after this one
            }

            // Reset split-tracking variables
            splitEventBytes = 0L;
            splitEventCount = 0;
            //cout << "Will split, reset splitEventBytes = "  << splitEventBytes << endl;
        }

        // If event to be written in its own record
        if (ownRecord) {
            fitInRecord = false;
        }
        // Try adding event to current record.
        // One event is guaranteed to fit in a record no matter the size,
        // IFF using multithreaded compression.
        // If record's memory is expanded to fit the one big event,
        // every subsequent record written using this object may be
        // bigger as well.
        else if (bankBuffer != nullptr) {
            fitInRecord = currentRecord->addEvent(bankBuffer);
        }
        else {
            fitInRecord = currentRecord->addEvent(bank);
        }

//std::cout << "writeEventToFile: fit in record = " << fitInRecord << std::endl;

        // If no room or too many events in record, write out current record first,
        // then start working on a new record with this event.

        // If no room or too many events ...
        if (!fitInRecord) {

            // We will not end up here if the file just split, so
            // splitEventBytes and splitEventCount will NOT have
            // just been set to 0.

            if (singleThreadedCompression) {
                // Only try to write what we have if there's something to write
                if (currentRecord->getEventCount() > 0) {
//std::cout << "writeEventToFile: single threaded compression, force = " << force << std::endl;
                    try {
                        // This might be the first write after the file split.
                        // If so, return false if disk is full,
                        // otherwise write what we already have first.
                        if (!tryCompressAndWriteToFile(force)) {
                            // Undo stuff since we're no longer writing
                            splitEventCount--;
                            splitEventBytes -= currentEventBytes;
//std::cout << "writeEventToFile: did NOT write existing record to disk" << std::endl;
                            return false;
                        }
//std::cout << "writeEventToFile: wrote existing record to disk" << std::endl;
                    }
                    catch (boost::thread_interrupted &e) {
                        return false;
                    }
                    catch (std::exception &e) {
                        throw EvioException(e);
                    }

                    // Add event to it which will fit -- unless buffer is provided
                    // by user and single event is bigger than available memory.
                    if (bankBuffer != nullptr) {
                        fitInRecord = currentRecord->addEvent(bankBuffer);
                    }
                    else {
                        fitInRecord = currentRecord->addEvent(bank);
                    }
//std::cout << "writeEventToFile: try again to fit in record = " << fitInRecord << std::endl;
                }

                // Since we're here, we have a single event too big to fit in the
                // allocated buffers. So expand buffers and try again.
                if (!fitInRecord) {
//std::cout << "writeEventToFile: expand buffers to " << (currentEventBytes/10*12) << "  bytes" << std::endl;
                    expandInternalBuffers(currentEventBytes);

                    if (bankBuffer != nullptr) {
                        fitInRecord = currentRecord->addEvent(bankBuffer);
                    }
                    else {
                        fitInRecord = currentRecord->addEvent(bank);
                    }

                    if (!fitInRecord) {
                        throw EvioException("cannot fit event into buffer");
                    }
//std::cout << "writeEventToFile: EVENT FINALLY FIT!" << std::endl;
                }
            }
            else {
                // This "if" is only needed since we may want event in its own record
                if (currentRecord->getEventCount() > 0) {
                    currentRingItem->setCheckDisk(true);
                    supply->publish(currentRingItem);
                    currentRingItem = supply->get();
                    currentRecord = currentRingItem->getRecord();
                    currentRecord->getHeader()->setRecordNumber(recordNumber++);
//std::cout << "writeEventToFile: just published rec, new rec# = " << (recordNumber - 1) << ", next = " << recordNumber << std::endl;
                }

                // Add event to it (guaranteed to fit)
                if (bankBuffer != nullptr) {
                    currentRecord->addEvent(bankBuffer);
                }
                else {
                    currentRecord->addEvent(bank);
                }
            }
        }

        // If event must be physically written to disk,
        // or the event must be written in its own record ...
        if (force || ownRecord) {
            if (singleThreadedCompression) {
                try {
                    if (!tryCompressAndWriteToFile(true)) {
                        splitEventCount--;
                        splitEventBytes -= currentEventBytes;
                        return false;
                    }
                }
                catch (boost::thread_interrupted & e) {
                    return false;
                }
                catch (std::exception & e) {
                    throw EvioException(e);
                }
            }
            else {
                if (force) {
                    // Force things to disk by telling the writing
                    // thread which record started the force to disk.
                    // This will force this record, along with all preceeding records in
                    // the pipeline, to the file.
                    // Once it's written, we can go back to the normal of not
                    // forcing things to disk.
                    currentRingItem->setId(++idCounter);
                    recordWriterThread[0].setForcedRecordId(idCounter);
                }

                supply->publish(currentRingItem);
                currentRingItem = supply->get();
                currentRecord = currentRingItem->getRecord();
                currentRecord->getHeader()->setRecordNumber(recordNumber++);
                //std::cout << "writeEventToFile: FORCED published rec, new rec# = " << (recordNumber - 1) << ", next = " << recordNumber << std::endl;
            }
        }

        return true;
    }


    /**
     * Check to see if the disk is full.
     * Is it able to store 1 full split, 1 supply of records, and a 10MB buffer zone?
     * Two variables are set, one atomic and one not, depending on needs.
     * @return  true if full, else false.
     */
    bool EventWriter::fullDisk() {
#ifdef USE_FILESYSTEMLIB
        // How much free space is available on the disk?
        fs::space_info dirInfo = fs::space(currentFilePath.parent_path());
        uint64_t freeBytes = dirInfo.available;
#else
        uint64_t freeBytes;
        struct statvfs sttvfs{};

        // Use Boost.Filesystem to extract the directory (in case file does not exist yet)
        boost::filesystem::path path(currentFileName);
        boost::filesystem::path directory = path.parent_path();

        if (statvfs(directory.string().c_str(), &sttvfs) == 0) {
            freeBytes = sttvfs.f_bavail * sttvfs.f_frsize;
            //std::cout << "EventWriter: freeBytes for (" << dirStr << ") = " << freeBytes << std::endl;
        }
        else {
            // assume there is room if we can't get disk info
            diskIsFull = false;
            if (!singleThreadedCompression) {
                diskIsFullVolatile = false;
            }
            return false;
        }
#endif
        // If there isn't enough free space to write the complete, projected size file
        // plus full records + 10MB extra ...
        diskIsFull = freeBytes < split + maxSupplyBytes + 10000000;
        if (!singleThreadedCompression) {
            diskIsFullVolatile = diskIsFull;
        }
        return diskIsFull;
    }


    /**
     * Compress data and write record to file. Does nothing if close() already called.
     * Used when doing compression & writing to file in a single thread.
     *
     * @param force  if true, force writing event physically to disk.
     *
     * @throws EvioException if this object already closed;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if error opening/writing/forcing write to file.
     */
    void EventWriter::compressAndWriteToFile(bool force) {

        auto header = currentRecord->getHeader();
        header->setRecordNumber(recordNumber);
        header->setCompressionType(compressionType);
        currentRecord->build();
        // Resets currentRecord too
        writeToFile(force, false);
    }


    /**
     * Compress data and write record to file. Does nothing if close() already called.
     * Used when doing compression & writing to file in a single thread.
     * Will not write file if no room on disk (and force arg is false).
     *
     * @param force  if true, force writing event physically to disk.
     * @return true if everything normal; false if a new file needs to be created
     *         (first write after a split) but there is not enough free space on
     *         the disk partition for the next, complete file.
     *         If force arg is true, write anyway.
     *
     * @throws EvioException if this object already closed;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if error opening/writing/forcing write to file.
     */
    bool EventWriter::tryCompressAndWriteToFile(bool force) {

        auto header = currentRecord->getHeader();
        header->setRecordNumber(recordNumber);
        header->setCompressionType(compressionType);
        currentRecord->build();
        return writeToFile(force, true);
    }


    /**
     * For single threaded compression, write record to file.
     * In this case, we have 1 record, but 2 buffers.
     * One buffer can be written, while the 2nd is being filled
     * in the record.
     * Does nothing if close() already called.
     *
     * @param force force it to write event to the disk.
     * @param checkDisk if true and if a new file needs to be created but there is
     *                  not enough free space on the disk partition for the
     *                  complete intended file, return false without creating or
     *                  writing to file. If force arg is true, write anyway.
     *
     * @return true if everything normal; false if a new file needs to be created
     *         (first write after a split) but there is not enough free space on
     *         the disk partition for the next, complete file and checkDisk arg is true.
     *
     * @throws EvioException if this object already closed;
     *                       if file exists but user requested no over-writing;
     *                       if error opening/writing/forcing write to file.
     */
    bool EventWriter::writeToFile(bool force, bool checkDisk) {
        if (closed) {
            throw EvioException("close() has already been called");
        }

        // This actually creates the file so do it only once
        if (bytesWritten < 1) {

            // We want to check to see if there is enough room to write the
            // next split, before it's written. Thus, before writing the first record
            // of a new file, we check to see if there's space for the whole thing.
            // (Actually, the whole split + current block + some extra for safety).
            if (checkDisk && (!force) && fullDisk()) {
                // If we're told to check the disk, and we're not forcing things,
                // AND disk is full, don't write the record.
                return false;
            }

            // New shared pointer for each file ...
            asyncFileChannel = std::make_shared<std::fstream>();
            asyncFileChannel->open(currentFileName, std::ios::binary | std::ios::trunc | std::ios::out);
            if (asyncFileChannel->fail()) {
                throw EvioException("error opening file " + currentFileName);
            }

            // Right now file is open for writing
            fileOpen = true;
            fileWritingPosition = 0L;
            splitCount++;

            // Write out the beginning file header including common record
            writeFileHeader();
        }

        // Which buffer do we fill next?
        std::shared_ptr<ByteBuffer> unusedBuffer;

        // We need future job to be completed in order to proceed

        // If 1st time thru, proceed without waiting
        if (future1 == nullptr) {
            // Fill 2nd buffer next
            unusedBuffer = internalBuffers[1];
        }
        // After first time, wait until the future is finished before proceeding
        else {
            // If previous write in progress ...
            if (future1->valid()) {
                future1->get();

                // Reuse the buffer future1 just finished using
                unusedBuffer = usedBuffer;
            }
        }

        // Get record to write
        auto record = currentRecord;
        auto header = record->getHeader();

        // Length of this record
        int bytesToWrite = header->getLength();
        int eventCount   = header->getEntries();
//std::cout << "    writeToFile: adding to recordLengths: " << bytesToWrite << ", evCount = " << eventCount << std::endl;
        recordLengths->push_back(bytesToWrite);
        // Trailer's index has count following length
        recordLengths->push_back(eventCount);

        // Data to write
        auto buf = record->getBinaryBuffer();

//        std::cout << "\n    writeToFile: file pos = " << asyncFileChannel->tellg() << ", fileWritingPOsition = " <<
//                  fileWritingPosition << std::endl;
//        std::cout << "\n    writeToFile: write into buf: pos = " << buf->position() <<
//                     ", lim =  = " << buf->limit() <<
//                  ", cap = " << buf->capacity() << std::endl;

        future1 = std::make_shared<std::future<void>>(std::future<void>(
                std::async(std::launch::async,  // run in a separate thread
                           staticWriteFunction, // function to run
                           this,                // arguments to function ...
                           reinterpret_cast<const char *>(buf->array()),
                           bytesToWrite)));

        // Keep track of which buffer future1 used so it can be reused when done
        usedBuffer = buf;

        // Next buffer to work with
        buffer = unusedBuffer;
        // Clear buffer since we don't know what state it was left in
        // after write to file AND setBuffer uses its position to start from
        buffer->clear();
        record->setBuffer(buffer);
        record->reset();

        // Force it to write to physical disk (KILLS PERFORMANCE!!!)
        // TODO: This may not work since data may NOT have been written yet!
        if (force) asyncFileChannel->sync();

        // Keep track of what is written to this, one, file
        recordNumber++;
        recordsWritten++;
        bytesWritten        += bytesToWrite;
        fileWritingPosition += bytesToWrite;
        eventsWrittenToFile += eventCount;
        eventsWrittenTotal  += eventCount;

//        if (false) {
//            std::cout << "    writeToFile: after last header written, Events written to:" << std::endl;
//            std::cout << "                 cnt total (no dict) = " << eventsWrittenTotal << std::endl;
//            std::cout << "                 file cnt total (dict) = " << eventsWrittenToFile << std::endl;
//            std::cout << "                 internal buffer cnt (dict) = " << eventsWrittenToBuffer << std::endl;
//            std::cout << "                 bytes-written  = " << bytesToWrite << std::endl;
//            std::cout << "                 bytes-to-file = " << bytesWritten << std::endl;
//            std::cout << "                 record # = " << recordNumber << std::endl;
//        }

        return true;
    }


    /**
     * For multi-threaded compression, write record to file.
     * In this case we do NOT have 1 record with 3 buffers.
     * Instead we have a ring of records, each with its own buffers.
     * Does nothing if close() already called.
     *
     * @param item  contains record to write to the disk if compression is multi-threaded.
     * @param force force it to write event to the disk.
     *
     * @throws EvioException if this object already closed;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     * @throws IOException   if error writing file
     */
    void EventWriter::writeToFileMT(std::shared_ptr<RecordRingItem> item, bool force) {
        if (closed) {
            throw EvioException("close() has already been called");
        }

        // This actually creates the file so do it only once
        if (bytesWritten < 1) {
            //std::cout << "Creating channel to " << currentFileName << std::endl;

            // New shared pointer for each file ...
            asyncFileChannel = std::make_shared<std::fstream>();
            asyncFileChannel->open(currentFileName, std::ios::binary | std::ios::trunc | std::ios::out);
            if (asyncFileChannel->fail()) {
                throw EvioException("error opening file " + currentFileName);
            }

            // Right now file is open for writing
            fileOpen = true;
            fileWritingPosition = 0L;
            splitCount++;

            // Write out the beginning file header including common record
            writeFileHeader();
        }

        // We the future job to be completed in order to proceed

        // If 1st time thru, proceed without waiting
        if (future1 != nullptr) {
            // If previous write in progress ...
            if (future1->valid()) {
                future1->get();
            }
            supply->releaseWriterSequential(ringItem1);
        }

        // Get record to write
        auto record = item->getRecord();
        auto header = record->getHeader();

        // Length of this record
        int bytesToWrite = header->getLength();
        int eventCount   = header->getEntries();

        //std::cout << "   **** added to recordLengths MT: " << bytesToWrite << ", " << eventCount << std::endl;

        recordLengths->push_back(bytesToWrite);
        // Trailer's index has count following length
        recordLengths->push_back(eventCount);

        // Data to write
        auto buf = record->getBinaryBuffer();

        if (noFileWriting) {
            future1 = std::make_shared<std::future<void>>(std::future<void>(
                    std::async(std::launch::async,
                               staticDoNothingFunction,
                               this)));

        }
        else {
            future1 = std::make_shared<std::future<void>>(std::future<void>(
                    std::async(std::launch::async,  // run in a separate thread
                               staticWriteFunction,        // function to run
                               this,                       // arguments to function ...
                               reinterpret_cast<const char *>(buf->array()),
                               bytesToWrite)));
        }

        ringItem1 = item;

        // Force it to write to physical disk (KILLS PERFORMANCE!!!, 15x-20x slower),
        // but don't bother writing the metadata (arg to force()) since that slows it
        // down even more.
        // TODO: This may not work since data may NOT have been written yet!
        if (force) asyncFileChannel->sync();

        // Keep track of what is written to this, one, file
        //recordNumber++;
        recordsWritten++;
        bytesWritten        += bytesToWrite;
        fileWritingPosition += bytesToWrite;
        eventsWrittenToFile += eventCount;
        eventsWrittenTotal  += eventCount;

        //fileWritingPosition += 20;
        //bytesWritten  += 20;

        //        if (false) {
        //            std::cout << "    writeToFile: after last header written, Events written to:" << std::endl;
        //            std::cout << "                 cnt total (no dict) = " << eventsWrittenTotal << std::endl;
        //            std::cout << "                 file cnt total (dict) = " << eventsWrittenToFile << std::endl;
        //            std::cout << "                 internal buffer cnt (dict) = " << eventsWrittenToBuffer << std::endl;
        //            std::cout << "                 bytes-written  = " << bytesToWrite << std::endl;
        //            std::cout << "                 bytes-to-file = " << bytesWritten << std::endl;
        //            std::cout << "                 record # = " << recordNumber << std::endl;
        //        }
    }



    /**
     * Split the file for multithreaded compression.
     * Never called when output is to buffer.
     * It writes the trailer which includes an index of all records.
     * Then it closes the old file (forcing unflushed data to be written)
     * and creates the name of the new one.
     *
     * @throws EvioException if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     */
    void EventWriter::splitFile() {

        if (fileOpen) {
            // Finish writing data & trailer and then close existing file -
            // all in a separate thread for speed. Copy over values so they
            // don't change in the meantime.
            fileCloser->closeAsyncFile(asyncFileChannel, future1, supply, ringItem1,
                                       fileHeader, recordLengths, bytesWritten,
                                       recordNumber,
                                       addingTrailer, addTrailerIndex,
                                       noFileWriting, byteOrder);

            // Reset for next write
            if (!singleThreadedCompression) {
                future1 = nullptr;
            }
            recordLengths->clear();
            // Right now no file is open for writing
            fileOpen = false;
        }

        // Create the next file's name
        std::string fileName = Util::generateFileName(baseFileName, specifierCount,
                                                 runNumber, split, splitNumber,
                                                 streamId, streamCount);
        splitNumber += splitIncrement;

#ifdef USE_FILESYSTEMLIB
        currentFilePath = fs::path(fileName);
        bool fileExists = fs::exists(currentFilePath);
        bool isRegularFile = fs::is_regular_file(currentFilePath);
#else
        struct stat stt{};
        bool fileExists = stat( fileName.c_str(), &stt ) == 0;
        bool isRegularFile = S_ISREG(stt.st_mode);
#endif

        if (!overWriteOK && (fileExists && isRegularFile)) {
            if (supply != nullptr) {
                supply->haveError(true);
                std::string errMsg("file exists but user requested no over-writing");
                supply->setError(errMsg);
            }
            throw EvioException("file " + fileName + " exists, but user requested no over-writing");
        }
        currentFileName = fileName;


        // Reset file values for reuse
        if (singleThreadedCompression) {
            recordNumber = 1;
        }
        recordsWritten      = 0;
        bytesWritten        = 0L;
        eventsWrittenToFile = 0;

        std::cout << "    splitFile: generated file name = " << fileName << ", record # = " << recordNumber << std::endl;
    }


    /**
     * Write a general header as the last "header" or trailer in the file
     * optionally followed by an index of all record lengths.
     * This writes synchronously.
     *
     * @param writeIndex if true, write an index of all record lengths in trailer.
     * @throws EvioException if problems writing to file.
     */
    void EventWriter::writeTrailerToFile(bool writeIndex) {

        // Keep track of where we are right now which is just before trailer
        uint64_t trailerPosition = bytesWritten;

        // If we're NOT adding a record index, just write trailer
        if (!writeIndex) {
            RecordHeader::writeTrailer(headerArray, 0, recordNumber, byteOrder, recordLengths);

            // We don't want to let the closer thread do the work of seeing that
            // this write completes since it'll just complicate the code.
            // As this is the absolute last write to the file,
            // just make sure it gets done right here.
            asyncFileChannel->seekg(fileWritingPosition);
            asyncFileChannel->write(reinterpret_cast<char *>(headerArray.data()),
                                    RecordHeader::HEADER_SIZE_BYTES);
            if (asyncFileChannel->fail()) {
                throw EvioException("error writing to  file " + currentFileName);
            }
        }
        else {
            // Write trailer with index

            // How many bytes are we writing here?
            size_t bytesToWrite = RecordHeader::HEADER_SIZE_BYTES + 4*recordLengths->size();

            // Make sure our array can hold everything
            if (headerArray.capacity() < bytesToWrite) {
                headerArray.resize(bytesToWrite, 0);
            }

            // Place data into headerBuffer - both header and index
            RecordHeader::writeTrailer(headerArray, 0, recordNumber, byteOrder, recordLengths);

            //std::cout << "\nwriteTrailerToFile: file pos = " << asyncFileChannel->tellg() << ", fileWritingPOsition = " <<
            //                 fileWritingPosition << std::endl;
            // TODO: is this seekg really necessary?
            asyncFileChannel->seekg(fileWritingPosition);
            asyncFileChannel->write(reinterpret_cast<char *>(headerArray.data()), bytesToWrite);
            if (asyncFileChannel->fail()) {
                throw EvioException("error writing to  file " + currentFileName);
            }

            //            asyncFileChannel->flush();
            //            std::cout << "After writing trailer to " + currentFileName << std::endl;
            //            Util::printBytes(currentFileName, 0, 120, "File Beginning of " + currentFileName);
        }

        // Update file header's trailer position word
        if (!byteOrder.isLocalEndian()) {
            trailerPosition = SWAP_64(trailerPosition);
        }
        asyncFileChannel->seekg(FileHeader::TRAILER_POSITION_OFFSET);
        asyncFileChannel->write(reinterpret_cast<char *>(&trailerPosition), sizeof(trailerPosition));
        if (asyncFileChannel->fail()) {
            throw EvioException("error writing to  file " + currentFileName);
        }

        // Update file header's bit-info word
        if (addTrailerIndex) {
            uint32_t bitInfo = fileHeader.setBitInfo(fileHeader.hasFirstEvent(),
                                                     fileHeader.hasDictionary(),
                                                     true);
            if (!byteOrder.isLocalEndian()) {
                bitInfo = SWAP_32(bitInfo);
            }
            asyncFileChannel->seekg(FileHeader::BIT_INFO_OFFSET);
            asyncFileChannel->write(reinterpret_cast<char *>(&bitInfo), sizeof(bitInfo));
            if (asyncFileChannel->fail()) {
                throw EvioException("error writing to  file " + currentFileName);
            }
        }

        //        asyncFileChannel->flush();
        //        std::cout << "After writing trailer & updating file header to " + currentFileName << std::endl;
        //        Util::printBytes(currentFileName, 0, 120, "File Beginning of " + currentFileName);

    }

    // TODO: THIS DOES NOT LOOK RIGHT, recordNumber may need to be incremented ..........................................
    /**
     * Flush everything in currentRecord into the buffer.
     * There is only one record containing events which is written into the buffer.
     * Following that record is a trailer - an empty record with only a header
     * in order to signify that there are no more events to follow. The trailer
     * could contain an index of all events in the buffer, although this is never
     * done when transferring evio data in buffers over the network.
     */
    void EventWriter::flushCurrentRecordToBuffer() {

        uint32_t eventCount = currentRecord->getEventCount();
        // If nothing in current record, return
        if (eventCount < 1) {
            return;
        }

        // Do construction of record in buffer and possibly compression of its data
        if (commonRecord != nullptr) {
            currentRecord->build(*(commonRecord->getBinaryBuffer().get()));
        }
        else {
            currentRecord->build();
        }

        // Get record header
        auto header = currentRecord->getHeader();
        // Get/set record info before building
        header->setRecordNumber(recordNumber);

        //        cout << "flushCurrentRecordToBuffer: comp size = " << header->getCompressedDataLength() <<
        //                ", comp words = " << header->getCompressedDataLengthWords() << ", padding = " <<
        //                header->getCompressedDataLengthPadding() << endl;

        uint32_t bytesToWrite = header->getLength();
        // Store length & count for possible trailer index

//std::cout << "   ********** adding to recordLengths flush: " << bytesToWrite << ", " << eventCount << std::endl;
        recordLengths->push_back(bytesToWrite);
        // Trailer's index has count following length
        recordLengths->push_back(eventCount);

        // Keep track of what is written
        //        recordNumber++;
        recordsWritten++;

        // We need to reset lengths here since the data may now be compressed
        bytesWritten = bytesToWrite;
    }


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
    bool EventWriter::writeToBuffer(std::shared_ptr<EvioBank> bank, std::shared_ptr<ByteBuffer> bankBuffer) {
        bool fitInRecord;

        if (bankBuffer != nullptr) {
            // Will this fit the event being written PLUS the ending trailer?
            fitInRecord = currentRecord->addEvent(bankBuffer, trailerBytes());
        }
        else {
            fitInRecord = currentRecord->addEvent(bank, trailerBytes());
        }

        if (fitInRecord) {
            // Update the current block header's size and event count as best we can.
            // Does NOT take compression or trailer into account.
            bytesWritten = commonRecordBytesToBuffer + currentRecord->getUncompressedSize();
            eventsWrittenTotal++;
            eventsWrittenToBuffer++;
        }

        return fitInRecord;
    }


    /**
     * How many bytes make up the desired trailer?
     * @return  number of bytes that make up the desired trailer.
     */
    uint32_t EventWriter::trailerBytes() {
        uint32_t len = 0;
        if (addingTrailer) len += RecordHeader::HEADER_SIZE_BYTES;
        if (addTrailerIndex) len += 4 * recordLengths->size();
        return len;
    }


    /**
     * Write a general header as the last "header" or trailer in the buffer
     * optionally followed by an index of all record lengths.
     * @param writeIndex if true, write an index of all record lengths in trailer.
     * @throws EvioException if not enough room in buffer to hold trailer.
     */
    void EventWriter::writeTrailerToBuffer(bool writeIndex) {

        // If we're NOT adding a record index, just write trailer
        if (!writeIndex) {
            // Make sure buffer can hold a trailer
            if ((buffer->capacity() - bytesWritten) < RecordHeader::HEADER_SIZE_BYTES) {
                throw EvioException("not enough room in buffer");
            }

            int bytes = RecordHeader::writeTrailer(*(buffer.get()), bytesWritten, recordNumber);
            bytesWritten += bytes;
            buffer->limit(bytesWritten);
        }
        else {
            // Create the index of record lengths in proper byte order
            uint32_t arraySize = 4*recordLengths->size();

            // Write trailer with index

            // How many bytes are we writing here?
            uint32_t bytesToWrite = RecordHeader::HEADER_SIZE_BYTES + arraySize;
            //std::cout << "writeTrailerToBuffer: bytesToWrite = " << bytesToWrite <<
            //        ", record index len = " << arraySize << std::endl;

            // Make sure our buffer can hold everything
            if ((buffer->capacity() - bytesWritten) < bytesToWrite) {
                throw EvioException("not enough room in buffer");
            }

            // Place data into buffer - both header and index
            //std::cout << "writeTrailerToBuffer: start writing at pos = " << bytesWritten << std::endl;
            int bytes = RecordHeader::writeTrailer(*(buffer.get()), bytesWritten, recordNumber, recordLengths);
            bytesWritten += bytes;
            buffer->limit(bytesWritten);
        }
    }

}
