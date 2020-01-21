/**
 * Copyright (c) 2019, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 07/22/2019
 * @author timmer
 */

#ifndef EVIO_6_0_RECORDNODE_H
#define EVIO_6_0_RECORDNODE_H

#include <cstdint>
#include <sstream>


namespace evio {


/**
 * This class is used to store relevant info about an evio record
 * along with its position in a buffer.
 *
 * @author timmer
 * Date: 7/23/2019
 */
class RecordNode {

private:

    /** Record's length value (32-bit words). */
    uint32_t len = 0;
    /** Number of events in record. */
    uint32_t count = 0;
    /** Position of record in file/buffer.  */
    uint32_t pos = 0;
    /**
     * Place of this record in file/buffer. First record = 0, second = 1, etc.
     * Useful for appending banks to EvioEvent object.
     */
    uint32_t place = 0;

public:

    RecordNode() = default;

    void clear() {
        len = count = pos = place = 0;
    }

    uint32_t getLen() const {return len;}
    void setLen(uint32_t l) {len = l;}

    uint32_t getCount() const {return count;}
    void setCount(uint32_t c) {count = c;}

    uint32_t getPos() const {return pos;}
    void setPos(uint32_t p) {pos = p;}

    uint32_t getPlace() const {return place;}
    void setPlace(uint32_t p) {place = p;}

    string toString() const {
        stringstream ss;

        ss << "len = "      << len;
        ss << ", count = "  << count;
        ss << ", pos = "    << pos;
        ss << ", place = "  << place;

        return ss.str();
    }

};

}


#endif //EVIO_6_0_RECORDNODE_H
