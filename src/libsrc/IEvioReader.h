//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_IEVIOREADER_H
#define EVIO_6_0_IEVIOREADER_H


#include <cstdint>
#include <cstring>
#include <string>
#include <memory>
#include <vector>


#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "IBlockHeader.h"
#include "EventParser.h"


namespace evio {


    /**
     * This pure, virtual class is meant to encapsulate the operation of reading both
     * 2 differently formatted evio versions. One implementing class reads evio versions
     * 1 - 4, and the other, version 6.
     *
     * @date 06/8/2020
     * @author timmer
     */
    class IEvioReader {

    public:

        /**
          * This <code>enum</code> denotes the status of a read/write. <b>Used internally.</b><br>
          * SUCCESS indicates a successful read. <br>
          * END_OF_FILE indicates that we cannot read because an END_OF_FILE has occurred.
          * Technically this means that whatever we are trying to read is larger than the
          * buffer's unread bytes.<br>
          * CANNOT_OPEN_FILE indicates that we cannot write because the destination file cannot be opened.<br>
          * EVIO_EXCEPTION indicates that an EvioException was thrown during a read/write, possibly
          * due to out of range values.<br>
          * UNKNOWN_ERROR indicates that an unrecoverable error has occurred.
          */
        enum ReadWriteStatus {
            SUCCESS = 0, END_OF_FILE, CANNOT_OPEN_FILE, EVIO_EXCEPTION, UNKNOWN_ERROR
        };

        /**
         * This method can be used to avoid creating additional EvioReader
         * objects by reusing this one with another buffer. The method
         * {@link #close()} is called before anything else.
         *
         * @param buf ByteBuffer to be read.
         * @throws underflow_error if not enough data in buffer.
         * @throws EvioException if buf is null; buf not in proper format;
         *                       if first record/block number != 1 when checkRecNumSeq arg is true.
         */
        virtual void setBuffer(std::shared_ptr<ByteBuffer> & buf) = 0;

        /**
         * Has {@link #close()} been called (without reopening by calling
         * {@link #setBuffer(std::shared_ptr<ByteBuffer> &)})?
         *
         * @return {@code true} if this object closed, else {@code false}.
         */
        virtual bool isClosed() = 0;

        /**
         * Is this reader checking the block number sequence and
         * throwing an exception if it's not sequential and starting with 1?
         * @return <code>true</code> if checking block number sequence, else <code>false</code>
         */
        virtual bool checkBlockNumberSequence() = 0;

        /**
         * Get the byte order of the file/buffer being read.
         * @return byte order of the file/buffer being read.
         */
        virtual ByteOrder & getByteOrder() = 0;

        /**
         * Get the evio version number.
         * @return evio version number.
         */
        virtual uint32_t getEvioVersion() = 0;

        /**
         * Get the path to the file.
         * @return path to the file
         */
        virtual std::string getPath() = 0;

        /**
         * Get the file/buffer parser.
         * @return file/buffer parser.
         */
        virtual std::shared_ptr<EventParser> & getParser() = 0;

        /**
         * Set the file/buffer parser.
         * @param evParser file/buffer parser.
         */
        virtual void setParser(std::shared_ptr<EventParser> & evParser) = 0;

        /**
         * Get the XML format dictionary if there is one.
         * @return XML format dictionary, else null.
         */
        virtual std::string getDictionaryXML() = 0;

        /**
         * Does this evio file have an associated XML dictionary?
         * @return <code>true</code> if this evio file has an associated XML dictionary,
         *         else <code>false</code>
         */
        virtual bool hasDictionaryXML() = 0;

        /**
         * Get the "first" event if there is one.
         * It's also called the Beginning-Of-Run event.
         * This event is defined once but included in each of the related split files written out.
         * @return the first event is it existed, else null.
         */
        virtual std::shared_ptr<EvioEvent> getFirstEvent() = 0;

        /**
         * Does this evio file have an associated first event?
         * It's also called the Beginning-Of-Run event.
         * This event is defined once but included in each of the related split files written out.
         * @return <code>true</code> if this evio file has an associated first event,
         *         else <code>false</code>
         */
        virtual bool hasFirstEvent() = 0;

        /**
         * Get the number of events remaining in the file.
         * Useful only if doing a sequential read.
         *
         * @return number of events remaining in the file
         * @throws EvioException if failed reading from file
         */
        virtual size_t getNumEventsRemaining() = 0;

        /**
         * Get the byte buffer being read. Not useful when reading files.
         * @return the byte buffer being read (in certain cases).
         */
        virtual std::shared_ptr<ByteBuffer> getByteBuffer() = 0;

        /**
         * Get the size of the file being read, in bytes.
         * @return the file size in bytes
         */
        virtual size_t fileSize() = 0;

        /**
         * This returns the FIRST block (record) header.
         * @return the first block (record) header.
         */
        virtual std::shared_ptr<IBlockHeader> getFirstBlockHeader() = 0;

        /**
         * Get the event in the file/buffer at a given index (starting at 1).
         * As useful as this sounds, most applications will probably call
         * {@link #parseNextEvent()} or {@link #parseEvent(size_t)} instead,
         * since it combines combines getting an event with parsing it.<p>
         *
         * @param  index number of event desired, starting at 1, from beginning of file/buffer
         * @return the event in the file/buffer at the given index or null if none
         * @throws EvioException if failed file access;
         *                       if failed read due to bad file/buffer format;
         *                       if index out of bounds;
         *                       if object closed
         */
        virtual std::shared_ptr<EvioEvent> getEvent(size_t index) = 0;

        /**
         * This is a workhorse method. It retrieves the desired event from the file/buffer,
         * and then parses it SAX-like. It will drill down and uncover all structures
         * (banks, segments, and tagsegments) and notify any interested listeners.
         *
         * @param  index number of event desired, starting at 1, from beginning of file/buffer
         * @return the parsed event at the given index or null if none
         * @throws EvioException if failed file access;
         *                       if failed read due to bad file/buffer format;
         *                       if index out of bounds;
         *                       if object closed
         */
        virtual std::shared_ptr<EvioEvent> parseEvent(size_t index) = 0;

        /**
         * Get the next event in the file/buffer. As useful as this sounds, most
         * applications will probably call {@link #parseNextEvent()} instead, since
         * it combines getting the next event with parsing the next event.<p>
         *
         * Although this method can get events in versions 4+, it now delegates that
         * to another method. No changes were made to this method from versions 1-3 in order
         * to read the version 4 format as it is subset of versions 1-3 with variable block
         * length.
         *
         * @return the next event in the file.
         *         On error it throws an EvioException.
         *         On end of file, it returns <code>null</code>.
         * @throws EvioException if failed file access;
         *                       if failed read due to bad buffer format;
         *                       if object closed
         */
        virtual std::shared_ptr<EvioEvent> nextEvent() = 0;

        /**
         * This is a workhorse method. It retrieves the next event from the file/buffer,
         * and then parses it SAX-like. It will drill down and uncover all structures
         * (banks, segments, and tagsegments) and notify any interested listeners.
         *
         * @return the event that was parsed.
         *         On error it throws an EvioException.
         *         On end of file, it returns <code>null</code>.
         * @throws EvioException if failed file access;
         *                       if read failure or bad format;
         *                       if object closed
         */
        virtual std::shared_ptr<EvioEvent> parseNextEvent() = 0;

        /**
         * This will parse an event, SAX-like. It will drill down and uncover all structures
         * (banks, segments, and tagsegments) and notify any interested listeners.<p>
         *
         * As useful as this sounds, most applications will probably call {@link #parseNextEvent()}
         * instead, since it combines combines getting the next event with parsing the next event.<p>
         *
         * This method is only called by synchronized methods and therefore is not synchronized.
         *
         * @param evioEvent the event to parse.
         * @throws EvioException if bad format
         */
        virtual void parseEvent(std::shared_ptr<EvioEvent> evioEvent) = 0;

        /**
         * Get an evio bank or event in vector-of-bytes form.
         * @param evNumber number of event of interest (starting at 1).
         * @param vec vector to contain bank's/event's bytes.
         * @return number of bytes in returned event.
         * @throws EvioException if failed file access;
         *                       if eventNumber out of bounds (starts at 1);
         *                       if the event number does not correspond to an existing event;
         *                       if object closed
         */
        virtual uint32_t getEventArray(size_t evNumber, std::vector<uint8_t> & vec) = 0;

        /**
         * Get an evio bank or event in ByteBuffer form.
         * @param evNumber number of event of interest
         * @param buf buffer to contain bank's/event's bytes.
         * @return number of bytes in returned event.
         * @throws EvioException if failed file access;
         *                       if eventNumber out of bounds (starts at 1);
         *                       if the event number does not correspond to an existing event;
         *                       if object closed
         */
        virtual uint32_t getEventBuffer(size_t evNumber, ByteBuffer & buf) = 0;

        /**
         * The equivalent of rewinding the file. What it actually does
         * is set the position of the file/buffer back to where it was
         * after calling the constructor - after the first header.
         *
         * @throws EvioException if object closed; if failed file access or buffer/file read
         */
        virtual void rewind() = 0;

        /**
         * This is equivalent to obtaining the current position in the file.
         * What it actually does is return the position of the buffer.
         * Only meaningful to evio versions 1-3 and for sequential reading.<p>
         *
         * @return the position of the buffer; -1 if not sequential read and version 4+
         * @throws EvioException if object closed; if error accessing file
         */
        virtual ssize_t position() = 0;

        /** This is closes the file, but for buffers it only sets the position to 0. */
        virtual void close() = 0;

        /**
         * This returns the current (active) block (physical record) header.
         * Since most users have no interest in physical records, this method
         * should not be used.
         *
         * @return the current block header.
         */
        virtual std::shared_ptr<IBlockHeader> getCurrentBlockHeader() = 0;

        /**
         * Go to a specific event in the file. The events are numbered 1..N.
         * This number is transient--it is not part of the event as stored in the evio file.
         * In versions 4 and up this is just a wrapper on {@link #getEvent(size_t)}.
         *
         * @param evNumber the event number in a 1..N counting sense, from the start of the file.
         * @return the specified event in file or null if there's an error or nothing at that event #.
         * @throws EvioException if object closed; if failed file access
         */
        virtual std::shared_ptr<EvioEvent> gotoEventNumber(size_t evNumber) = 0;

        /**
         * This is the number of events in the file/buffer.
         * Any dictionary or first event are <b>not</b>
         * included in the count.
         *
         * @return the number of events in the file/buffer.
         * @throws EvioException if read failure; if failed file access;
         *                       if object closed
         */
        virtual size_t getEventCount() = 0;

        /**
         * This is the number of blocks/records in the file/buffer including the empty
         * block, record or trailer at the end.
         *
         * @throws EvioException if object closed.
         * @return the number of records in the file/buffer (estimate for version 3 files).
         */
        virtual size_t getBlockCount() = 0;
    };

}

#endif //EVIO_6_0_IEVIOREADER_H
