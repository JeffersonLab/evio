//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_RECORDNODE_H
#define EVIO_6_0_RECORDNODE_H


#include <cstdint>
#include <sstream>


namespace evio {


    /**
     * This class is used to store relevant info about an evio record (or block)
     * along with its position in a buffer.
     * This class was previously called "BlockNode" in java.
     *
     * @author timmer
     * Date: 7/22/2019
     */
    class RecordNode {

        friend class EvioCompactReaderV4;

    private:

        /** Record's length value (32-bit words). */
        uint32_t len = 0;
        /** Number of events in record. */
        uint32_t count = 0;
        /** Position of record in file/buffer.  */
        size_t pos = 0;
        /**
         * Place of this record in file/buffer. First record = 0, second = 1, etc.
         * Useful for appending banks to EvioEvent object.
         */
        uint32_t place = 0;

    public:

        RecordNode() = default;

        /** Set all internal values to 0. */
        void clear() {
            len = count = pos = place = 0;
        }

        /**
         * Get the record's length in 32-bit words.
         * @return record's length in 32-bit words.
         */
        uint32_t getLen() const {return len;}

        /**
         * Set the record's length in 32-bit words.
         * @param l record's length in 32-bit words.
         */
        void setLen(uint32_t l) {len = l;}


        /**
         * Get the number of events in this record.
         * @return number of events in this record.
         */
        uint32_t getCount() const {return count;}

        /**
         * Set the number of events in this record.
         * @param c number of events in this record.
         */
        void setCount(uint32_t c) {count = c;}


        /**
         * Get the position of this record in the file/buffer.
         * @return position of this record in the file/buffer..
         */
        size_t getPos() const {return pos;}

        /**
         * Set the position of this record in the file/buffer.
         * @param p position of this record in the file/buffer.
         */
        void setPos(size_t p) {pos = p;}


        /**
         * Get the place of this record in file/buffer. First record = 0, second = 1, etc.
         * @return place of this record in file/buffer.
         */
        uint32_t getPlace() const {return place;}

        /**
         * Set the place of this record in file/buffer. First record = 0, second = 1, etc.
         * @param p place of this record in file/buffer.
         */
        void setPlace(uint32_t p) {place = p;}


        /**
         * Get a string representation of this object.
         * @return string representation of this object.
         */
        std::string toString() const {
            std::stringstream ss;

            ss << "len = "      << len;
            ss << ", count = "  << count;
            ss << ", pos = "    << pos;
            ss << ", place = "  << place;

            return ss.str();
        }

    };

}


#endif //EVIO_6_0_RECORDNODE_H
